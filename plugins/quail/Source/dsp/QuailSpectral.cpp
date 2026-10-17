#include "QuailSpectral.h"

#include <animalaudio/dsp/EnvelopeFollower.h>

#include <algorithm>
#include <cmath>

namespace quail::dsp
{
namespace
{
constexpr float kFloorDb = -120.0f;
constexpr float kDbToNeper = 0.11512925464970229f; // ln(10) / 20
constexpr double kSlowMs = 1500.0;

float dbToGain(float db) noexcept
{
    return std::exp(db * kDbToNeper);
}

std::size_t at(int a, int b) noexcept
{
    return static_cast<std::size_t>(a) * static_cast<std::size_t>(b);
}
} // namespace

void SpectralDynamics::prepare(int newMaxBins)
{
    maxBins = newMaxBins;
    const auto bins = static_cast<std::size_t>(maxBins);
    envelopes.assign(bins * kNumBands, kFloorDb);
    weights.assign(bins * kNumBands, 0.0f);
    tiltDb.assign(bins, 0.0f);
    powers.assign(bins * kNumSources * kNumPlacements, 0.0f);
    prefixes.assign((bins + 1) * kNumSources * kNumPlacements, 0.0);
    groupGains.assign(bins * kNumPlacements, 0.0f);
    numBins = 0;
    reset();
}

void SpectralDynamics::configure(double newSampleRate, int newFftSize, int newHop) noexcept
{
    sampleRate = newSampleRate;
    fftSize = newFftSize;
    hop = newHop;
    numBins = std::min(maxBins, fftSize / 2 + 1);
    for (int k = 0; k < numBins; ++k)
    {
        const double hz = std::max(1, k) * sampleRate / fftSize;
        tiltDb[static_cast<std::size_t>(k)] = static_cast<float>(3.0 * std::log2(hz / 1000.0));
    }
    reset();
}

void SpectralDynamics::reset() noexcept
{
    for (auto& b : bands)
    {
        b.wasActive = false;
        b.weightsValid = false;
        b.slowLevelDb = kFloorDb;
    }
    for (auto& g : bandGainDb)
        g.store(0.0f, std::memory_order_relaxed);
    for (auto& l : bandLevelDb)
        l.store(kFloorDb, std::memory_order_relaxed);
    for (auto& d : display)
        d.store(0.0f, std::memory_order_relaxed);
    for (auto& a : displayActive)
        a.store(false, std::memory_order_relaxed);
}

void SpectralDynamics::updateWeights(int band, const BandSettings& s) noexcept
{
    auto& state = bands[static_cast<std::size_t>(band)];
    const WeightKey key{s.shape, s.slope, s.frequency, s.q};
    if (state.weightsValid && state.key == key)
        return;
    state.key = key;
    state.weightsValid = true;

    float* w = weights.data() + at(band, maxBins);
    const double fc = std::clamp(static_cast<double>(s.frequency), 10.0, 0.49 * sampleRate);
    const double q = std::clamp(static_cast<double>(s.q), static_cast<double>(kMinQ), static_cast<double>(kMaxQ));
    const double binHz = sampleRate / fftSize;
    const int order = animalaudio::dsp::prototypeOrder(s.shape, s.slope);
    const double edge = std::pow(999.0, 0.5 / order); // the weight falls to 1e-3 here

    double loHz = 0.0, hiHz = 0.5 * sampleRate;
    if (s.shape == FilterShape::Bell)
    {
        const double c = edge / q;
        const double rHigh = 0.5 * (c + std::sqrt(c * c + 4.0));
        loHz = fc / rHigh;
        hiHz = fc * rHigh;
    }
    else if (s.shape == FilterShape::LowShelf)
        hiHz = fc * edge;
    else
        loHz = fc / edge;

    state.lo = std::clamp(static_cast<int>(std::floor(loHz / binHz)), 1, numBins - 1);
    state.hi = std::clamp(static_cast<int>(std::ceil(hiHz / binHz)), 1, numBins - 1);
    for (int k = state.lo; k <= state.hi; ++k)
    {
        const double r = k * binHz / fc;
        double x = 0.0;
        if (s.shape == FilterShape::Bell)
        {
            const double d = q * (r - 1.0 / r);
            x = std::pow(d * d, order);
        }
        else if (s.shape == FilterShape::LowShelf)
            x = std::pow(r, 2.0 * order);
        else
            x = std::pow(1.0 / r, 2.0 * order);
        w[k] = static_cast<float>(1.0 / (1.0 + x));
    }
}

const float* SpectralDynamics::triggerPower(int source, Placement placement,
                                            const animalaudio::dsp::StftProcessor::Frame& frame,
                                            bool stereoInput) noexcept
{
    const int index = source * kNumPlacements + static_cast<int>(placement);
    float* p = powers.data() + at(index, maxBins);
    if (powerValid[static_cast<std::size_t>(index)])
        return p;
    powerValid[static_cast<std::size_t>(index)] = true;

    const auto* a = frame.keys[static_cast<std::size_t>(2 * source)];
    const auto* b = stereoInput || source == 1 ? frame.keys[static_cast<std::size_t>(2 * source + 1)] : a;
    // a full-scale sine centred on a bin reads 0 dB (|X| = size / 4)
    const float scale = 16.0f / (static_cast<float>(fftSize) * static_cast<float>(fftSize));
    double* prefix = prefixes.data() + at(index, maxBins + 1);
    prefix[0] = 0.0;
    for (int k = 0; k < numBins; ++k)
    {
        const auto ak = a[k];
        const auto bk = b[k];
        float value = 0.0f;
        switch (placement)
        {
        case Placement::Stereo:
            value = 0.5f * (std::norm(ak) + std::norm(bk));
            break;
        case Placement::Left:
            value = std::norm(ak);
            break;
        case Placement::Right:
            value = std::norm(bk);
            break;
        case Placement::Mid:
            value = 0.25f * std::norm(ak + bk);
            break;
        case Placement::Side:
            value = 0.25f * std::norm(ak - bk);
            break;
        }
        p[k] = value * scale;
        prefix[k + 1] = prefix[k] + static_cast<double>(p[k]);
    }
    return p;
}

void SpectralDynamics::processFrame(animalaudio::dsp::StftProcessor::Frame& frame, const Settings& settings,
                                    bool stereoInput) noexcept
{
    using animalaudio::dsp::EnvelopeFollower;
    if (numBins == 0 || frame.numBins != numBins || frame.numKeys < 4 || frame.numChannels < 1)
        return;

    powerValid.fill(false);
    groupUsed.fill(false);
    int touchedLo = numBins, touchedHi = -1;
    const double frameRate = sampleRate / hop;
    const float slowCoeff = EnvelopeFollower::coefficient(kSlowMs, frameRate);

    for (int b = 0; b < kNumBands; ++b)
    {
        const auto& s = settings.bands[static_cast<std::size_t>(b)];
        auto& state = bands[static_cast<std::size_t>(b)];
        const bool active = s.isSpectral() && (stereoInput || s.placement == Placement::Stereo);
        if (!active)
        {
            state.wasActive = false;
            bandGainDb[static_cast<std::size_t>(b)].store(0.0f, std::memory_order_relaxed);
            bandLevelDb[static_cast<std::size_t>(b)].store(kFloorDb, std::memory_order_relaxed);
            continue;
        }

        updateWeights(b, s);
        const int source = s.externalSidechain ? 1 : 0;
        triggerPower(source, s.placement, frame, stereoInput);
        const double* prefix =
            prefixes.data() + at(source * kNumPlacements + static_cast<int>(s.placement), maxBins + 1);
        float* env = envelopes.data() + at(b, maxBins);
        const float* w = weights.data() + at(b, maxBins);
        const float attack = EnvelopeFollower::coefficient(s.attackMs, frameRate);
        const float release = EnvelopeFollower::coefficient(s.releaseMs, frameRate);
        // density 0: each bin listens to a 1 octave wide neighbourhood, density 1: 1/24 octave
        const double octaves = std::pow(1.0 / 24.0, std::clamp(static_cast<double>(s.spectralDensity), 0.0, 1.0));
        const double ratio = std::exp2(0.5 * octaves);

        double meanSum = 0.0, weightSum = 0.0;
        float loudest = kFloorDb;
        for (int k = state.lo; k <= state.hi; ++k)
        {
            const int k0 = std::max(1, static_cast<int>(std::lround(k / ratio)));
            const int k1 = std::min(numBins - 1, std::max(k0, static_cast<int>(std::lround(k * ratio))));
            const double average = (prefix[k1 + 1] - prefix[k0]) / (k1 - k0 + 1);
            const float level = static_cast<float>(10.0 * std::log10(average + 1.0e-20)) +
                                (s.spectralTilt ? tiltDb[static_cast<std::size_t>(k)] : 0.0f);
            float& e = env[k];
            e = state.wasActive ? level + (level > e ? attack : release) * (e - level) : level;
            meanSum += static_cast<double>(w[k]) * e;
            weightSum += w[k];
            if (w[k] >= 0.5f)
                loudest = std::max(loudest, e);
        }

        float threshold = s.thresholdDb;
        if (s.autoThreshold)
        {
            const float mean = weightSum > 0.0 ? static_cast<float>(meanSum / weightSum) : kFloorDb;
            state.slowLevelDb = state.wasActive ? mean + slowCoeff * (state.slowLevelDb - mean) : mean;
            threshold = state.slowLevelDb + kAutoThresholdOffsetDb;
        }

        const auto group = static_cast<std::size_t>(s.placement);
        float* gains = groupGains.data() + at(static_cast<int>(group), maxBins);
        if (!groupUsed[group])
        {
            std::fill(gains, gains + numBins, 0.0f);
            groupUsed[group] = true;
        }
        float extreme = 0.0f;
        for (int k = state.lo; k <= state.hi; ++k)
        {
            const float g = dynamicGainDb(env[k] - threshold, s.rangeDb) * w[k];
            gains[k] += g;
            if (std::abs(g) > std::abs(extreme))
                extreme = g;
        }
        touchedLo = std::min(touchedLo, state.lo);
        touchedHi = std::max(touchedHi, state.hi);
        state.wasActive = true;
        bandGainDb[static_cast<std::size_t>(b)].store(extreme, std::memory_order_relaxed);
        bandLevelDb[static_cast<std::size_t>(b)].store(loudest, std::memory_order_relaxed);
        bandThresholdDb[static_cast<std::size_t>(b)].store(threshold, std::memory_order_relaxed);
    }

    const auto groupPtr = [&](Placement p) -> const float*
    {
        const auto g = static_cast<std::size_t>(p);
        return groupUsed[g] ? groupGains.data() + at(static_cast<int>(g), maxBins) : nullptr;
    };
    const float* gStereo = groupPtr(Placement::Stereo);
    const float* gLeft = groupPtr(Placement::Left);
    const float* gRight = groupPtr(Placement::Right);
    const float* gMid = groupPtr(Placement::Mid);
    const float* gSide = groupPtr(Placement::Side);
    auto* chA = frame.channels[0];
    auto* chB = frame.numChannels > 1 ? frame.channels[1] : nullptr;
    for (int k = touchedLo; k <= touchedHi; ++k)
    {
        const float common = gStereo != nullptr ? gStereo[k] : 0.0f;
        const float left = common + (gLeft != nullptr ? gLeft[k] : 0.0f);
        const float right = common + (gRight != nullptr ? gRight[k] : 0.0f);
        if (left != 0.0f)
            chA[k] *= dbToGain(left);
        if (chB == nullptr)
            continue;
        if (right != 0.0f)
            chB[k] *= dbToGain(right);
        const float mid = gMid != nullptr ? gMid[k] : 0.0f;
        const float side = gSide != nullptr ? gSide[k] : 0.0f;
        if (mid != 0.0f || side != 0.0f)
        {
            const auto m = 0.5f * (chA[k] + chB[k]) * dbToGain(mid);
            const auto d = 0.5f * (chA[k] - chB[k]) * dbToGain(side);
            chA[k] = m + d;
            chB[k] = m - d;
        }
    }

    const double binsPerHz = fftSize / sampleRate;
    for (int p = 0; p < kNumPlacements; ++p)
    {
        const bool used = groupUsed[static_cast<std::size_t>(p)];
        displayActive[static_cast<std::size_t>(p)].store(used, std::memory_order_relaxed);
        if (!used)
            continue;
        const float* g = groupGains.data() + at(p, maxBins);
        for (int i = 0; i < kDisplayPoints; ++i)
        {
            const int first =
                std::clamp(static_cast<int>(std::lround(displayFrequency(i) * binsPerHz)), 0, numBins - 1);
            const int last = std::clamp(static_cast<int>(displayFrequency(i + 1) * binsPerHz), first, numBins - 1);
            float value = g[first];
            for (int k = first + 1; k <= last; ++k)
                if (std::abs(g[k]) > std::abs(value))
                    value = g[k];
            display[static_cast<std::size_t>(p * kDisplayPoints + i)].store(value, std::memory_order_relaxed);
        }
    }
}

float SpectralDynamics::getBandGainDb(int band) const noexcept
{
    return bandGainDb[static_cast<std::size_t>(band)].load(std::memory_order_relaxed);
}

float SpectralDynamics::getBandLevelDb(int band) const noexcept
{
    return bandLevelDb[static_cast<std::size_t>(band)].load(std::memory_order_relaxed);
}

float SpectralDynamics::getBandThresholdDb(int band) const noexcept
{
    return bandThresholdDb[static_cast<std::size_t>(band)].load(std::memory_order_relaxed);
}

float SpectralDynamics::getDisplayDb(Placement placement, int point) const noexcept
{
    return display[static_cast<std::size_t>(static_cast<int>(placement) * kDisplayPoints + point)].load(
        std::memory_order_relaxed);
}

bool SpectralDynamics::isDisplayActive(Placement placement) const noexcept
{
    return displayActive[static_cast<std::size_t>(placement)].load(std::memory_order_relaxed);
}

double SpectralDynamics::displayFrequency(int point) noexcept
{
    return static_cast<double>(kMinHz) *
           std::pow(static_cast<double>(kMaxHz / kMinHz), static_cast<double>(point) / (kDisplayPoints - 1));
}
} // namespace quail::dsp
