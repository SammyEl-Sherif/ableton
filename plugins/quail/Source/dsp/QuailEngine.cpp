#include "QuailEngine.h"

#include <algorithm>
#include <functional>
#include <cmath>

namespace quail::dsp
{
namespace
{
using animalaudio::dsp::EnvelopeFollower;
using animalaudio::dsp::SvfCoefficients;

constexpr float kFloorDb = -120.0f;
constexpr double kPi = 3.14159265358979323846;
/** The mean square of a sine is half its peak squared: +3 dB makes a full-scale sine read 0 dB. */
constexpr float kSineCalibrationDb = 3.0103f;
constexpr double kAverageMs = 1500.0;
constexpr double kOffsetReleaseMs = 60.0;

float dbToGain(float db) noexcept
{
    return std::pow(10.0f, db / 20.0f);
}
float powerToDb(float power) noexcept
{
    return 10.0f * std::log10(power + 1.0e-12f) + kSineCalibrationDb;
}

/** Section that isolates what a band listens to: unity-peak band pass at the band (bells and everything without
    a side), Butterworth low pass for low shelves/cuts, high pass for high shelves/cuts. */
SvfCoefficients isolatingSection(FilterShape shape, double hz, double q, double sampleRate) noexcept
{
    SvfCoefficients c;
    c.g = std::tan(kPi * std::clamp(hz, 10.0, 0.49 * sampleRate) / sampleRate);
    c.m0 = 0.0;
    if (shape == FilterShape::LowShelf || shape == FilterShape::LowCut)
    {
        c.k = 1.0 / SvfCoefficients::kButterworthQ;
        c.m2 = 1.0;
    }
    else if (shape == FilterShape::HighShelf || shape == FilterShape::HighCut)
    {
        c.k = 1.0 / SvfCoefficients::kButterworthQ;
        c.m0 = 1.0;
        c.m1 = -c.k;
        c.m2 = -1.0;
    }
    else
    {
        c.k = 1.0 / std::clamp(q, 0.3, 40.0);
        c.m1 = c.k;
    }
    return c;
}

bool usesIsolation(FilterShape shape) noexcept
{
    return shape != FilterShape::TiltShelf && shape != FilterShape::FlatTilt;
}

bool same(float a, float b) noexcept
{
    return std::equal_to<float>()(a, b);
}

bool sameStaticEq(const Settings& a, const Settings& b) noexcept
{
    if (!same(a.global.gainScale, b.global.gainScale))
        return false;
    for (std::size_t i = 0; i < a.bands.size(); ++i)
    {
        const auto& x = a.bands[i];
        const auto& y = b.bands[i];
        if (x.isActive() != y.isActive())
            return false;
        if (x.isActive() && (x.shape != y.shape || x.slope != y.slope || !same(x.frequency, y.frequency) ||
                             !same(x.gainDb, y.gainDb) || !same(x.q, y.q) || x.placement != y.placement))
            return false;
    }
    return true;
}

std::size_t at(int a, int b) noexcept
{
    return static_cast<std::size_t>(a) * static_cast<std::size_t>(b);
}
} // namespace

void Engine::prepare(double newSampleRate)
{
    sampleRate = newSampleRate;
    for (auto& b : bands)
        b.prepare(sampleRate);
    for (auto& d : detectors)
    {
        d.envelope.prepare(sampleRate);
        d.average.prepare(sampleRate);
        d.average.setTimes(kAverageMs, kAverageMs);
    }
    dynamicDb.assign(at(kNumBands, kChunk), 0.0f);
    for (auto& p : pre)
        p.assign(kChunk, 0.0f);
    for (auto& a : audition)
        a.assign(kChunk, 0.0f);

    const int minOrder = spectralOrder(SpectralResolution::Low, sampleRate);
    const int maxOrder = spectralOrder(SpectralResolution::High, sampleRate);
    stft.prepare(minOrder, maxOrder);
    spectral.prepare((1 << maxOrder) / 2 + 1);
    if (spectralRequested > 0)
        spectralRequested = std::clamp(spectralRequested, minOrder, maxOrder);

    std::size_t delaySize = 1;
    while (delaySize < static_cast<std::size_t>((1 << maxOrder) + kChunk))
        delaySize <<= 1;
    for (auto& d : dryDelay)
        d.assign(delaySize, 0.0f);
    dryMask = delaySize - 1;

    soloMix.prepare(sampleRate, 4.0);
    for (auto& g : outputGain)
        g.prepare(sampleRate, 15.0);
    bypassMix.prepare(sampleRate, 8.0);
    switchStep = 1.0f / std::max(1.0f, static_cast<float>(0.004 * sampleRate));
    offsetRelease = EnvelopeFollower::coefficient(kOffsetReleaseMs, sampleRate);
    reset();
}

void Engine::reset() noexcept
{
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& s = settings.bands[static_cast<std::size_t>(b)];
        auto& state = bandStates[static_cast<std::size_t>(b)];
        state.placement = s.placement;
        auto& band = bands[static_cast<std::size_t>(b)];
        band.setActive(s.isActive());
        band.setTarget(filterSpec(s, settings.global.gainScale));
        band.reset();
        auto& d = detectors[static_cast<std::size_t>(b)];
        d.primed = false;
        d.offsetDb = 0.0f;
        bandDynamicDb[static_cast<std::size_t>(b)].store(0.0f, std::memory_order_relaxed);
        bandTriggerDb[static_cast<std::size_t>(b)].store(kFloorDb, std::memory_order_relaxed);
        bandThresholdDb[static_cast<std::size_t>(b)].store(s.thresholdDb, std::memory_order_relaxed);
    }

    spectralCurrent = 0;
    if (spectralRequested > 0)
    {
        stft.setOrder(spectralRequested);
        spectralCurrent = stft.getOrder();
        spectral.configure(sampleRate, stft.getSize(), stft.getHop());
    }
    else
        spectral.reset();
    switchGain = 1.0f;
    switchHold = 0;

    soloBand = -1;
    soloMix.reset(0.0f);
    for (auto& f : soloFilters)
        f.reset();

    autoGainValid = false;
    autoGainCountdown = 0;
    updateAutoGain(0);
    updateOutputTargets(true);
    bypassMix.reset(settings.global.bypass ? 1.0f : 0.0f);
    for (auto& d : dryDelay)
        std::fill(d.begin(), d.end(), 0.0f);
    dryWrite = 0;
}

void Engine::setSettings(const Settings& newSettings) noexcept
{
    settings = newSettings;
}

void Engine::setSpectralOrder(int order) noexcept
{
    if (order <= 0)
        spectralRequested = 0;
    else
        spectralRequested = stft.isPrepared() ? std::clamp(order, stft.getMinOrder(), stft.getMaxOrder()) : order;
}

void Engine::setSoloBand(int band) noexcept
{
    soloRequested = band;
}

void Engine::updateBands(bool stereo) noexcept
{
    const bool spectralRunning = spectralCurrent > 0 && spectralRequested == spectralCurrent;
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& s = settings.bands[static_cast<std::size_t>(b)];
        auto& state = bandStates[static_cast<std::size_t>(b)];
        auto& band = bands[static_cast<std::size_t>(b)];
        const bool want = s.isActive() && (stereo || s.placement == Placement::Stereo);

        // a band moving to another part of the signal fades out where it was, then back in
        if (s.placement != state.placement)
        {
            band.setActive(false);
            if (band.isIdle())
            {
                state.placement = s.placement;
                detectors[static_cast<std::size_t>(b)].primed = false;
            }
        }
        if (s.placement == state.placement)
            band.setActive(want);
        band.setTarget(filterSpec(s, settings.global.gainScale));

        state.spectralDynamics = want && s.isSpectral() && spectralRunning;
        state.timeDynamics = want && s.hasDynamics() && !state.spectralDynamics && s.placement == state.placement;
    }
}

void Engine::updateOutputTargets(bool jump) noexcept
{
    const auto& g = settings.global;
    float gain = dbToGain(g.outputDb + (g.autoGain ? autoGainDb.load(std::memory_order_relaxed) : 0.0f));
    if (g.phaseInvert)
        gain = -gain;
    const float pan = std::clamp(g.pan, -1.0f, 1.0f);
    const float left = gain * (pan > 0.0f ? 1.0f - pan : 1.0f);
    const float right = gain * (pan < 0.0f ? 1.0f + pan : 1.0f);
    outputGain[0].setTarget(left);
    outputGain[1].setTarget(right);
    if (jump)
        for (auto& o : outputGain)
            o.reset(o.getTarget());
}

void Engine::updateAutoGain(int n) noexcept
{
    if (!settings.global.autoGain)
    {
        autoGainDb.store(0.0f, std::memory_order_relaxed);
        autoGainValid = false;
        return;
    }
    if (autoGainCountdown > 0)
    {
        autoGainCountdown -= n;
        return;
    }
    if (autoGainValid && sameStaticEq(settings, autoGainSettings))
        return;
    autoGainDb.store(-std::clamp(estimateLoudnessChangeDb(settings, sampleRate), -24.0f, 24.0f),
                     std::memory_order_relaxed);
    autoGainSettings = settings;
    autoGainValid = true;
    autoGainCountdown = 2048;
}

void Engine::process(float* const* main, int numMain, const float* const* sidechain, int numSidechain,
                     int numSamples) noexcept
{
    numMain = std::min(numMain, kMaxChannels);
    if (numMain <= 0 || numSamples <= 0 || pre[0].empty())
        return;

    updateBands(numMain > 1);
    updateAutoGain(numSamples);
    updateOutputTargets(false);
    bypassMix.setTarget(settings.global.bypass ? 1.0f : 0.0f);

    for (int offset = 0; offset < numSamples; offset += kChunk)
    {
        const int n = std::min(kChunk, numSamples - offset);
        float* channels[kMaxChannels] = {main[0] + offset, numMain > 1 ? main[1] + offset : nullptr};
        const float* scA = numSidechain > 0 && sidechain[0] != nullptr ? sidechain[0] + offset : nullptr;
        const float* scB = numSidechain > 1 && sidechain[1] != nullptr ? sidechain[1] + offset : scA;
        processChunk(channels, numMain, scA, scB, n);
    }

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto i = static_cast<std::size_t>(b);
        if (bandStates[i].spectralDynamics)
        {
            bandDynamicDb[i].store(spectral.getBandGainDb(b), std::memory_order_relaxed);
            bandTriggerDb[i].store(spectral.getBandLevelDb(b), std::memory_order_relaxed);
            bandThresholdDb[i].store(spectral.getBandThresholdDb(b), std::memory_order_relaxed);
        }
        else
            bandDynamicDb[i].store(detectors[i].offsetDb, std::memory_order_relaxed);
    }
}

void Engine::processChunk(float* const* main, int numMain, const float* scA, const float* scB, int n) noexcept
{
    const bool stereo = numMain > 1;
    for (int ch = 0; ch < numMain; ++ch)
        std::copy(main[ch], main[ch] + n, pre[static_cast<std::size_t>(ch)].begin());
    for (int ch = 0; ch < numMain; ++ch)
    {
        auto& delay = dryDelay[static_cast<std::size_t>(ch)];
        for (int i = 0; i < n; ++i)
            delay[(dryWrite + static_cast<std::size_t>(i)) & dryMask] = main[ch][i];
    }
    if (scA != nullptr)
    {
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max({peak, std::abs(scA[i]), std::abs(scB[i])});
        sidechainPeak.store(std::max(sidechainPeak.load(std::memory_order_relaxed), peak), std::memory_order_relaxed);
    }

    // dynamics: per-sample gain offsets, or a release back to 0 after they were switched off
    std::array<const float*, kNumBands> offsets{};
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto i = static_cast<std::size_t>(b);
        auto& d = detectors[i];
        float* out = dynamicDb.data() + at(b, kChunk);
        if (bandStates[i].timeDynamics && !bands[i].isIdle())
        {
            const auto& s = settings.bands[i];
            const float* a = s.externalSidechain ? scA : pre[0].data();
            const float* c = s.externalSidechain ? scB : pre[stereo ? 1 : 0].data();
            runDetector(b, a, c, n);
            offsets[i] = out;
        }
        else if (std::abs(d.offsetDb) > 0.01f)
        {
            d.primed = false;
            for (int k = 0; k < n; ++k)
            {
                d.offsetDb *= offsetRelease;
                out[k] = d.offsetDb;
            }
            offsets[i] = out;
        }
        else
            d.offsetDb = 0.0f;
    }

    runStage(false, main, numMain, n, offsets);
    if (stereo)
    {
        bool midSide = false;
        for (int b = 0; b < kNumBands && !midSide; ++b)
        {
            const auto p = bandStates[static_cast<std::size_t>(b)].placement;
            midSide = (p == Placement::Mid || p == Placement::Side) && !bands[static_cast<std::size_t>(b)].isIdle();
        }
        if (midSide)
        {
            for (int i = 0; i < n; ++i)
            {
                const float l = main[0][i], r = main[1][i];
                main[0][i] = 0.5f * (l + r);
                main[1][i] = 0.5f * (l - r);
            }
            runStage(true, main, numMain, n, offsets);
            for (int i = 0; i < n; ++i)
            {
                const float m = main[0][i], s = main[1][i];
                main[0][i] = m + s;
                main[1][i] = m - s;
            }
        }
    }

    if (spectralCurrent > 0)
    {
        const float* keys[] = {pre[0].data(), pre[stereo ? 1 : 0].data(), scA, scB};
        stft.process(main, numMain, keys, 4, n,
                     [this, stereo](animalaudio::dsp::StftProcessor::Frame& frame)
                     { spectral.processFrame(frame, settings, stereo); });
    }

    runSolo(main, numMain, n);

    for (int ch = 0; ch < numMain; ++ch)
    {
        auto& gain = outputGain[static_cast<std::size_t>(ch)];
        if (gain.isSettled() && gain.getTarget() == 1.0f)
            continue;
        for (int i = 0; i < n; ++i)
            main[ch][i] *= gain.next();
    }
    if (numMain == 1)
        outputGain[1].reset(outputGain[1].getTarget());

    if (!(bypassMix.isSettled() && bypassMix.getTarget() == 0.0f))
    {
        const auto latency = static_cast<std::size_t>(getLatencySamples());
        std::array<float, kChunk> mix{};
        for (int i = 0; i < n; ++i)
            mix[static_cast<std::size_t>(i)] = bypassMix.next();
        for (int ch = 0; ch < numMain; ++ch)
        {
            const auto& delay = dryDelay[static_cast<std::size_t>(ch)];
            for (int i = 0; i < n; ++i)
            {
                const float dry = delay[(dryWrite + static_cast<std::size_t>(i) - latency) & dryMask];
                const float m = mix[static_cast<std::size_t>(i)];
                main[ch][i] = m >= 1.0f ? dry : main[ch][i] + (dry - main[ch][i]) * m;
            }
        }
    }
    dryWrite = (dryWrite + static_cast<std::size_t>(n)) & dryMask;

    // switching the spectral stage on/off: fade out, switch, wait for the new latency to fill, fade in
    if (spectralRequested != spectralCurrent || switchGain < 1.0f || switchHold > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            if (spectralRequested != spectralCurrent)
                switchGain = std::max(0.0f, switchGain - switchStep);
            else if (switchHold > 0)
                --switchHold;
            else
                switchGain = std::min(1.0f, switchGain + switchStep);
            for (int ch = 0; ch < numMain; ++ch)
                main[ch][i] *= switchGain;
        }
        if (spectralRequested != spectralCurrent && switchGain <= 0.0f)
        {
            spectralCurrent = 0;
            if (spectralRequested > 0)
            {
                stft.setOrder(spectralRequested);
                spectralCurrent = stft.getOrder();
                spectral.configure(sampleRate, stft.getSize(), stft.getHop());
                switchHold = stft.getLatency();
            }
            else
                spectral.reset();
        }
    }

    for (int ch = 0; ch < numMain; ++ch)
    {
        float peak = 0.0f;
        for (int i = 0; i < n; ++i)
            peak = std::max(peak, std::abs(main[ch][i]));
        auto& meter = outputPeak[static_cast<std::size_t>(ch)];
        meter.store(std::max(meter.load(std::memory_order_relaxed), peak), std::memory_order_relaxed);
    }
}

void Engine::runDetector(int band, const float* a, const float* b, int n) noexcept
{
    const auto i = static_cast<std::size_t>(band);
    const auto& s = settings.bands[i];
    auto& d = detectors[i];
    float* out = dynamicDb.data() + at(band, kChunk);

    if (!d.primed || d.shape != s.shape || !same(d.frequency, s.frequency) || !same(d.q, s.q))
    {
        const auto c = isolatingSection(s.shape, s.frequency, s.q, sampleRate);
        for (auto& f : d.filters)
            f.setCoefficients(c);
        d.shape = s.shape;
        d.frequency = s.frequency;
        d.q = s.q;
        d.filtered = usesIsolation(s.shape);
    }
    if (!d.primed)
    {
        for (auto& f : d.filters)
            f.reset();
        d.envelope.reset(0.0f);
        d.average.reset(0.0f);
        d.primed = true;
    }
    d.envelope.setTimes(s.attackMs, s.releaseMs);

    const auto placement = bandStates[i].placement;
    const auto isolate = [&](std::size_t filter, float x) noexcept
    { return d.filtered ? d.filters[filter].process(x) : x; };

    float level = kFloorDb, threshold = s.thresholdDb;
    for (int k = 0; k < n; ++k)
    {
        const float x = a != nullptr ? a[k] : 0.0f;
        const float y = b != nullptr ? b[k] : x;
        float power = 0.0f;
        switch (placement)
        {
        case Placement::Stereo:
        {
            const float fx = isolate(0, x), fy = isolate(1, y);
            power = 0.5f * (fx * fx + fy * fy);
            break;
        }
        case Placement::Left:
        {
            const float f = isolate(0, x);
            power = f * f;
            break;
        }
        case Placement::Right:
        {
            const float f = isolate(0, y);
            power = f * f;
            break;
        }
        case Placement::Mid:
        {
            const float f = isolate(0, 0.5f * (x + y));
            power = f * f;
            break;
        }
        case Placement::Side:
        {
            const float f = isolate(0, 0.5f * (x - y));
            power = f * f;
            break;
        }
        }
        level = powerToDb(d.envelope.process(power));
        const float average = d.average.process(power);
        if (s.autoThreshold)
            threshold = powerToDb(average) + kAutoThresholdOffsetDb;
        out[k] = dynamicGainDb(level - threshold, s.rangeDb);
    }
    d.offsetDb = out[n - 1];
    bandTriggerDb[i].store(level, std::memory_order_relaxed);
    bandThresholdDb[i].store(threshold, std::memory_order_relaxed);
}

void Engine::runStage(bool midSide, float* const* channels, int numMain, int n,
                      const std::array<const float*, kNumBands>& offsets) noexcept
{
    for (int b = 0; b < kNumBands; ++b)
    {
        const auto i = static_cast<std::size_t>(b);
        const auto placement = bandStates[i].placement;
        const bool isMidSide = placement == Placement::Mid || placement == Placement::Side;
        auto& band = bands[i];
        if (isMidSide != midSide || band.isIdle())
            continue;

        float* targets[2] = {channels[0], numMain > 1 ? channels[1] : nullptr};
        int count = numMain;
        if (placement == Placement::Left || placement == Placement::Mid)
            count = 1;
        else if (placement == Placement::Right || placement == Placement::Side)
        {
            if (numMain < 2)
                continue;
            targets[0] = channels[1];
            count = 1;
        }
        band.process(targets, count, n, offsets[i]);
    }
}

void Engine::runSolo(float* const* main, int numMain, int n) noexcept
{
    const int requested = soloRequested;
    const bool valid =
        requested >= 0 && requested < kNumBands && settings.bands[static_cast<std::size_t>(requested)].isActive();
    if (valid && requested != soloBand)
    {
        soloBand = requested;
        soloFrequency = 0.0f;
        for (auto& f : soloFilters)
            f.reset();
    }
    soloMix.setTarget(valid ? 1.0f : 0.0f);
    if (soloMix.isSettled() && soloMix.getTarget() == 0.0f)
    {
        soloBand = -1;
        return;
    }
    if (soloBand < 0)
        return;

    const auto& s = settings.bands[static_cast<std::size_t>(soloBand)];
    if (s.shape != soloShape || !same(s.frequency, soloFrequency) || !same(s.q, soloQ))
    {
        const double q = s.shape == FilterShape::TiltShelf || s.shape == FilterShape::FlatTilt ? 0.5 : s.q;
        const auto c = isolatingSection(s.shape, s.frequency, q, sampleRate);
        for (auto& f : soloFilters)
            f.setCoefficients(c);
        soloShape = s.shape;
        soloFrequency = s.frequency;
        soloQ = s.q;
    }

    std::array<float, kChunk> mix{};
    for (int i = 0; i < n; ++i)
        mix[static_cast<std::size_t>(i)] = soloMix.next();
    for (int ch = 0; ch < numMain; ++ch)
    {
        auto& filter = soloFilters[static_cast<std::size_t>(ch)];
        const float* in = pre[static_cast<std::size_t>(ch)].data();
        for (int i = 0; i < n; ++i)
        {
            const float heard = filter.process(in[i]);
            main[ch][i] += (heard - main[ch][i]) * mix[static_cast<std::size_t>(i)];
        }
    }
}

float Engine::getBandDynamicDb(int band) const noexcept
{
    return bandDynamicDb[static_cast<std::size_t>(band)].load(std::memory_order_relaxed);
}

float Engine::getBandTriggerDb(int band) const noexcept
{
    return bandTriggerDb[static_cast<std::size_t>(band)].load(std::memory_order_relaxed);
}

float Engine::getBandThresholdDb(int band) const noexcept
{
    return bandThresholdDb[static_cast<std::size_t>(band)].load(std::memory_order_relaxed);
}

float Engine::takeOutputPeak(int channel) noexcept
{
    return outputPeak[static_cast<std::size_t>(channel)].exchange(0.0f, std::memory_order_relaxed);
}

float Engine::takeSidechainPeak() noexcept
{
    return sidechainPeak.exchange(0.0f, std::memory_order_relaxed);
}
} // namespace quail::dsp
