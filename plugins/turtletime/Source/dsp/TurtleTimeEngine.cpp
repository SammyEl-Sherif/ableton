#include "TurtleTimeEngine.h"

#include <algorithm>
#include <cmath>

namespace turtletime::dsp
{
namespace
{
constexpr double kHistorySeconds = 24.0;

double wrapPosition(double position, double size) noexcept
{
    if (size <= 0.0)
        return 0.0;
    double wrapped = std::fmod(position, size);
    if (wrapped < 0.0)
        wrapped += size;
    if (wrapped >= size)
        wrapped = 0.0;
    return wrapped;
}

int wrapIndex(int index, int size) noexcept
{
    if (size <= 0)
        return 0;
    int wrapped = index % size;
    if (wrapped < 0)
        wrapped += size;
    return wrapped;
}

/** 4-point cubic (Catmull-Rom). Reproduces linear ramps, so half-speed stays in tune without a harsh image. */
float catmullRom(float y0, float y1, float y2, float y3, float t) noexcept
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    return 0.5f * ((2.0f * y1) + (-y0 + y2) * t + (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3) * t2 +
                   (-y0 + 3.0f * y1 - 3.0f * y2 + y3) * t3);
}

double safeBpm(const animalaudio::dsp::TransportInfo& transport, double fallback) noexcept
{
    if (transport.hasBpm && transport.bpm > 1.0 && std::isfinite(transport.bpm))
        return transport.bpm;
    if (fallback > 1.0 && std::isfinite(fallback))
        return fallback;
    return 120.0;
}
} // namespace

void TurtleTimeEngine::prepare(double newSampleRate, int)
{
    sampleRate = newSampleRate > 1000.0 ? newSampleRate : 44100.0;
    capacity = std::max(16, static_cast<int>(std::ceil(sampleRate * kHistorySeconds)));
    buffer.assign(static_cast<std::size_t>(kMaxChannels * capacity), 0.0f);
    crossfadeSamples = std::max(1, static_cast<int>(std::lround(sampleRate * 0.010)));
    mixSmoother.prepare(sampleRate, 15.0);
    mixSmoother.reset(mix);
    reset();
}

void TurtleTimeEngine::reset() noexcept
{
    std::fill(buffer.begin(), buffer.end(), 0.0f);
    writeIndex = 0;
    readPos = 0.0;
    activeRate = patternAt(patternIndex).rate;
    fadeReadPos = 0.0;
    fadeRate = activeRate;
    fadeRemaining = 0;
    fadeSpanSamples = 1;
    ppqClock = 0.0;
    lastWindow = 0;
    lastSlice = -1;
    anchorPos = 0.0;
    primed = false;
    patternDirty = false;
    playheadPhase.store(0.0f, std::memory_order_relaxed);
    readMarker.store(0.0f, std::memory_order_relaxed);
    hostSynced.store(0, std::memory_order_relaxed);
}

void TurtleTimeEngine::setPatternIndex(int index) noexcept
{
    if (index < 0)
        index = 0;
    if (index >= kNumPatterns)
        index = kNumPatterns - 1;
    if (index != patternIndex)
    {
        patternIndex = index;
        patternDirty = true;
    }
}

void TurtleTimeEngine::setMix(float newMix) noexcept
{
    mix = std::clamp(newMix, 0.0f, 1.0f);
    mixSmoother.setTarget(mix);
}

void TurtleTimeEngine::setFallbackBpm(double bpm) noexcept
{
    if (bpm > 1.0 && std::isfinite(bpm))
        fallbackBpm = bpm;
}

void TurtleTimeEngine::startFade(double fromPosition, float fromRate, int span) noexcept
{
    fadeReadPos = fromPosition;
    fadeRate = fromRate;
    fadeSpanSamples = std::max(1, span);
    fadeRemaining = fadeSpanSamples;
}

int TurtleTimeEngine::fadeSamplesFor(const TimePattern& pattern, double bpm) const noexcept
{
    const double beats =
        pattern.mode == TimeMode::Repeat && pattern.sliceBeats > 0.0 ? pattern.sliceBeats : pattern.windowBeats;
    const double windowSamples = beats * sampleRate * 60.0 / bpm;
    const int limit = std::max(1, static_cast<int>(windowSamples * 0.25));
    return std::min(crossfadeSamples, limit);
}

float TurtleTimeEngine::readCubic(int channel, double position) const noexcept
{
    const double size = static_cast<double>(capacity);
    const double wrapped = wrapPosition(position, size);
    const int i1 = std::clamp(static_cast<int>(std::floor(wrapped)), 0, capacity - 1);
    const float t = static_cast<float>(wrapped - std::floor(wrapped));
    const int base = channel * capacity;
    const float y0 = buffer[static_cast<std::size_t>(base + wrapIndex(i1 - 1, capacity))];
    const float y1 = buffer[static_cast<std::size_t>(base + i1)];
    const float y2 = buffer[static_cast<std::size_t>(base + wrapIndex(i1 + 1, capacity))];
    const float y3 = buffer[static_cast<std::size_t>(base + wrapIndex(i1 + 2, capacity))];
    return catmullRom(y0, y1, y2, y3, t);
}

void TurtleTimeEngine::process(float* const* channels, int numChannels, int numSamples,
                               const animalaudio::dsp::TransportInfo& transport) noexcept
{
    if (capacity < 4 || numSamples <= 0 || numChannels <= 0 || channels == nullptr)
        return;

    const int count = std::min(numChannels, kMaxChannels);
    const TimePattern pattern = patternAt(patternIndex);
    const double bpm = safeBpm(transport, fallbackBpm);
    const double beatsPerSample = bpm / (60.0 * sampleRate);
    const bool host = transport.playing && transport.hasPpq && std::isfinite(transport.ppq);
    hostSynced.store(host ? 1 : 0, std::memory_order_relaxed);

    const double blockStart = host ? transport.ppq : ppqClock;
    const double windowBeats = pattern.windowBeats > 1.0e-6 ? pattern.windowBeats : 1.0;
    const int fadeSpan = fadeSamplesFor(pattern, bpm);

    float lastPlay = 0.0f;
    float lastMarker = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        double ppq = blockStart + static_cast<double>(sample) * beatsPerSample;
        if (!std::isfinite(ppq))
            ppq = 0.0;

        const int window = static_cast<int>(std::floor(ppq / windowBeats));
        const double windowStart = static_cast<double>(window) * windowBeats;
        const double into = std::clamp(ppq - windowStart, 0.0, windowBeats);
        const bool newWindow = !primed || window != lastWindow || patternDirty;

        int sliceIndex = 0;
        if (pattern.mode == TimeMode::Repeat && pattern.sliceBeats > 1.0e-6)
            sliceIndex = static_cast<int>(std::floor(into / pattern.sliceBeats));

        const bool newSlice = pattern.mode == TimeMode::Repeat && !newWindow && sliceIndex != lastSlice;

        if (newWindow && primed)
            startFade(readPos, activeRate, fadeSpan);
        else if (newSlice)
            startFade(readPos, activeRate, fadeSpan);

        for (int ch = 0; ch < count; ++ch)
            buffer[static_cast<std::size_t>(ch * capacity + writeIndex)] = channels[ch][sample];

        if (newWindow)
        {
            if (pattern.mode == TimeMode::Repeat)
                anchorPos = static_cast<double>(writeIndex);
            readPos = pattern.mode == TimeMode::Repeat ? anchorPos : static_cast<double>(writeIndex);
            activeRate = pattern.rate;
            lastSlice = sliceIndex;
            patternDirty = false;
            primed = true;
        }
        else if (newSlice)
        {
            readPos = anchorPos;
            activeRate = pattern.rate;
            lastSlice = sliceIndex;
        }

        lastWindow = window;

        const float mixNow = mixSmoother.next();
        const bool fading = fadeRemaining > 0 && fadeSpanSamples > 0;
        const float fadeT =
            fading ? 1.0f - static_cast<float>(fadeRemaining) / static_cast<float>(fadeSpanSamples) : 1.0f;

        for (int ch = 0; ch < count; ++ch)
        {
            float wet = readCubic(ch, readPos);
            if (fading)
                wet = readCubic(ch, fadeReadPos) * (1.0f - fadeT) + wet * fadeT;

            const float dry = channels[ch][sample];
            channels[ch][sample] = dry + (wet - dry) * mixNow;
        }

        writeIndex = wrapIndex(writeIndex + 1, capacity);
        readPos = wrapPosition(readPos + static_cast<double>(activeRate), static_cast<double>(capacity));
        if (fading)
        {
            fadeReadPos = wrapPosition(fadeReadPos + static_cast<double>(fadeRate), static_cast<double>(capacity));
            --fadeRemaining;
        }

        lastPlay = static_cast<float>(into / windowBeats);
        if (pattern.mode == TimeMode::CatchUp)
        {
            lastMarker = pattern.rate >= 0.0f ? std::clamp(pattern.rate * lastPlay, 0.0f, 1.0f)
                                              : std::clamp(1.0f + pattern.rate * lastPlay, 0.0f, 1.0f);
        }
        else
        {
            const double slice = pattern.sliceBeats > 1.0e-6 ? pattern.sliceBeats : windowBeats;
            lastMarker = static_cast<float>(std::fmod(into, slice) / windowBeats);
        }
    }

    ppqClock = blockStart + static_cast<double>(numSamples) * beatsPerSample;
    playheadPhase.store(std::clamp(lastPlay, 0.0f, 1.0f), std::memory_order_relaxed);
    readMarker.store(std::clamp(lastMarker, 0.0f, 1.0f), std::memory_order_relaxed);
}
} // namespace turtletime::dsp
