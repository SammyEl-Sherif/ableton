#include "IceReverb.h"

#include <algorithm>
#include <cmath>

namespace icepenguin::dsp
{
namespace
{
constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.283185307179586f;
constexpr float kHalfPi = 1.5707963267948966f;

constexpr int kBaseLineDelays441[IceReverb::kLines] = {2677, 3137, 3593, 4373, 4969, 5743, 6599, 7529};
constexpr int kInputDiffuser441[2] = {142, 379};

float dampCoeffFromTone(float damping01, double sampleRate) noexcept
{
    const float brightHz = 18000.0f;
    const float darkHz = 280.0f;
    const float hz = darkHz + (1.0f - damping01) * (brightHz - darkHz);
    const double g = std::exp(-2.0 * static_cast<double>(kPi) * hz / sampleRate);
    return static_cast<float>(g);
}

float feedbackFromDecay(float decaySeconds, float avgDelaySamples, double sampleRate) noexcept
{
    const float sec = std::max(0.08f, decaySeconds);
    const float loops = sec * static_cast<float>(sampleRate) / std::max(64.0f, avgDelaySamples);
    return std::pow(0.001f, 1.0f / std::max(1.0f, loops));
}
} // namespace

void IceReverb::DelayLine::prepare(int maxSamples) noexcept
{
    size = std::clamp(maxSamples, 1, static_cast<int>(buffer.size()) - 2);
    writeIndex = 0;
    clear();
}

void IceReverb::DelayLine::clear() noexcept
{
    buffer.fill(0.0f);
}

void IceReverb::DelayLine::setLength(int samples) noexcept
{
    size = std::clamp(samples, 8, static_cast<int>(buffer.size()) - 2);
}

float IceReverb::DelayLine::read(float offsetSamples) const noexcept
{
    const float readPos = static_cast<float>(writeIndex) - offsetSamples;
    const int i0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(i0);
    auto at = [this](int idx)
    {
        idx %= size;
        if (idx < 0)
            idx += size;
        return buffer[static_cast<std::size_t>(idx)];
    };
    return at(i0) + frac * (at(i0 + 1) - at(i0));
}

void IceReverb::DelayLine::write(float sample) noexcept
{
    buffer[static_cast<std::size_t>(writeIndex)] = sample;
}

void IceReverb::DelayLine::advance() noexcept
{
    writeIndex = (writeIndex + 1) % size;
}

float IceReverb::OnePoleLP::process(float x, float coeff) noexcept
{
    z = x * (1.0f - coeff) + z * coeff;
    return z;
}

void IceReverb::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate;
    mixSmoother.prepare(sampleRate, 20.0);
    dampSmoother.prepare(sampleRate, 35.0);
    feedbackSmoother.prepare(sampleRate, 45.0);
    widthSmoother.prepare(sampleRate, 30.0);
    modSmoother.prepare(sampleRate, 40.0);

    for (auto& d : delays)
        d.prepare(kMaxDelaySamples);
    for (auto& d : inputDiffusers)
        d.prepare(2048);

    const float srScale = static_cast<float>(sampleRate / 44100.0);
    inputDiffusers[0].setLength(std::max(8, static_cast<int>(kInputDiffuser441[0] * srScale)));
    inputDiffusers[1].setLength(std::max(12, static_cast<int>(kInputDiffuser441[1] * srScale)));

    linePhaseInc = {0.00021f, 0.00017f, 0.00023f, 0.00019f, 0.00025f, 0.00016f, 0.00022f, 0.00018f};
    for (float& p : linePhase)
        p = 0.0f;

    predelay.fill(0.0f);
    predelayWrite = 0;
    roomSizeBuilt = -1.0f;

    reset();
    rebuildDelayLengths();
    updateFeedbackTarget();
    updateCoeffs();
}

void IceReverb::reset() noexcept
{
    for (auto& d : delays)
        d.clear();
    for (auto& f : dampFilters)
        f.reset();
    for (auto& d : inputDiffusers)
        d.clear();
    predelay.fill(0.0f);
    predelayWrite = 0;
    inputHpZ = 0.0f;
}

void IceReverb::rebuildDelayLengths() noexcept
{
    if (roomSizeBuilt == settings.roomSize)
        return;
    roomSizeBuilt = settings.roomSize;

    const float srScale = static_cast<float>(sampleRate / 44100.0);
    const float sizeMul = 0.72f + settings.roomSize * 0.55f;

    for (int i = 0; i < kLines; ++i)
    {
        const int len = std::clamp(static_cast<int>(std::lround(kBaseLineDelays441[i] * srScale * sizeMul)), 64,
                                   kMaxDelaySamples - 4);
        delays[static_cast<std::size_t>(i)].setLength(len);
    }

    const float erScale = sizeMul * srScale;
    const int erBase441[kErTaps] = {347, 577, 911, 1381};
    erGain = {0.55f, 0.42f, 0.32f, 0.24f};
    for (int t = 0; t < kErTaps; ++t)
        erOffsetSamples[static_cast<std::size_t>(t)] = std::max(1, static_cast<int>(erBase441[t] * erScale * 0.35f));
}

void IceReverb::updateFeedbackTarget() noexcept
{
    float delaySum = 0.0f;
    for (const auto& d : delays)
        delaySum += static_cast<float>(d.length());
    const float avgDelay = delaySum / static_cast<float>(kLines);
    feedbackSmoother.setTarget(feedbackFromDecay(settings.decaySeconds, avgDelay, sampleRate));
}

void IceReverb::setSettings(const ReverbSettings& newSettings) noexcept
{
    settings = newSettings;
    settings.roomSize = std::clamp(settings.roomSize, 0.0f, 1.0f);
    settings.damping = std::clamp(settings.damping, 0.0f, 1.0f);
    settings.width = std::clamp(settings.width, 0.0f, 1.0f);
    settings.predelayMs = std::clamp(settings.predelayMs, 0.0f, 200.0f);
    settings.decaySeconds = std::clamp(settings.decaySeconds, 0.1f, 25.0f);

    dampSmoother.setTarget(settings.damping);
    widthSmoother.setTarget(settings.width);
    modSmoother.setTarget(0.35f + settings.roomSize * 1.85f);

    const int delay = static_cast<int>(std::lround(settings.predelayMs * 0.001 * sampleRate));
    predelaySamples = std::clamp(delay, 0, kMaxPredelaySamples - 1);

    rebuildDelayLengths();
    updateFeedbackTarget();
}

void IceReverb::updateCoeffs() noexcept
{
    dampCoeff = dampCoeffFromTone(dampSmoother.getCurrent(), sampleRate);
    feedbackGain = std::min(0.9925f, feedbackSmoother.getCurrent());
    modDepthSamples = modSmoother.getCurrent();
}

float IceReverb::readPredelay() const noexcept
{
    if (predelaySamples <= 0)
        return 0.0f;
    const int readIndex = (predelayWrite - predelaySamples + kMaxPredelaySamples) % kMaxPredelaySamples;
    return predelay[static_cast<std::size_t>(readIndex)];
}

void IceReverb::writePredelay(float sample) noexcept
{
    predelay[static_cast<std::size_t>(predelayWrite)] = sample;
}

void IceReverb::advancePredelay() noexcept
{
    predelayWrite = (predelayWrite + 1) % kMaxPredelaySamples;
}

float IceReverb::earlyReflections(float) noexcept
{
    float er = 0.0f;
    for (int t = 0; t < kErTaps; ++t)
    {
        const int off = erOffsetSamples[static_cast<std::size_t>(t)];
        const int idx = (predelayWrite - off + kMaxPredelaySamples) % kMaxPredelaySamples;
        er += predelay[static_cast<std::size_t>(idx)] * erGain[static_cast<std::size_t>(t)];
    }
    return er;
}

float IceReverb::diffuseInput(float sample) noexcept
{
    constexpr float kG = 0.62f;
    float x = sample;
    for (int d = 0; d < 2; ++d)
    {
        auto& line = inputDiffusers[static_cast<std::size_t>(d)];
        const float delayed = line.read(static_cast<float>(line.length()));
        const float v = x + delayed * kG;
        line.write(v);
        line.advance();
        x = delayed - v * kG;
    }
    return x;
}

float IceReverb::mixFeedback(const std::array<float, kLines>& lineOut, int line) noexcept
{
    float sum = 0.0f;
    for (float v : lineOut)
        sum += v;
    return (2.0f / static_cast<float>(kLines)) * sum - lineOut[static_cast<std::size_t>(line)];
}

void IceReverb::setMix(float wetMix) noexcept
{
    mix = std::clamp(wetMix, 0.0f, 1.0f);
    mixSmoother.setTarget(mix);
}

void IceReverb::process(float* left, float* right, int numSamples) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        if ((i & 31) == 0)
            updateCoeffs();
        dampSmoother.next();
        feedbackSmoother.next();
        widthSmoother.next();
        modSmoother.next();
        mixSmoother.next();

        const float mixNow = mixSmoother.getCurrent();
        const float dryGain = std::cos(mixNow * kHalfPi);
        const float wetGain = std::sin(mixNow * kHalfPi);

        const float inL = left[i];
        const float inR = right[i];
        float monoIn = 0.5f * (inL + inR);

        const float hpCoeff = 0.9955f;
        monoIn = monoIn - inputHpZ;
        inputHpZ = inputHpZ * hpCoeff + monoIn * (1.0f - hpCoeff);

        writePredelay(monoIn);
        const float afterPredelay = readPredelay();
        advancePredelay();

        const float diffused = diffuseInput(afterPredelay);
        const float er = earlyReflections(monoIn);
        const float fdnIn = (diffused + er * 0.45f) * (0.32f / static_cast<float>(kLines));

        std::array<float, kLines> lineOut{};
        for (int line = 0; line < kLines; ++line)
        {
            linePhase[static_cast<std::size_t>(line)] += linePhaseInc[static_cast<std::size_t>(line)];
            if (linePhase[static_cast<std::size_t>(line)] > kTwoPi)
                linePhase[static_cast<std::size_t>(line)] -= kTwoPi;

            const float mod =
                std::sin(linePhase[static_cast<std::size_t>(line)]) * modDepthSamples * (0.65f + 0.35f * (line & 1));
            const float delayOffset = static_cast<float>(delays[static_cast<std::size_t>(line)].length()) + mod;

            float sample = delays[static_cast<std::size_t>(line)].read(delayOffset);
            sample = dampFilters[static_cast<std::size_t>(line)].process(sample, dampCoeff);
            sample *= feedbackGain;
            lineOut[static_cast<std::size_t>(line)] = sample;
        }

        for (int line = 0; line < kLines; ++line)
        {
            const float fb = mixFeedback(lineOut, line);
            delays[static_cast<std::size_t>(line)].write(fb + fdnIn);
            delays[static_cast<std::size_t>(line)].advance();
        }

        float wetL = 0.0f;
        float wetR = 0.0f;
        for (int line = 0; line < kLines; ++line)
        {
            const float tap = lineOut[static_cast<std::size_t>(line)];
            if ((line & 1) == 0)
                wetL += tap;
            else
                wetR += tap;
        }
        const float norm = 0.22f;
        wetL *= norm;
        wetR *= norm;

        const float width = widthSmoother.getCurrent();
        const float mid = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR);
        wetL = mid + side * width;
        wetR = mid - side * width;

        left[i] = inL * dryGain + wetL * wetGain;
        right[i] = inR * dryGain + wetR * wetGain;
    }
}

} // namespace icepenguin::dsp
