#include "TurtleTimeEngine.h"

#include <algorithm>
#include <cmath>

namespace turtletime::dsp
{
namespace
{
// Reverse reads up to two windows back and Repeat 2 Bar up to four bars, so this covers 2-bar reverse at 30 BPM.
constexpr double kHistorySeconds = 32.0;

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

double hostBpm(const animalaudio::dsp::TransportInfo& transport) noexcept
{
    if (transport.hasBpm && transport.bpm > 1.0 && std::isfinite(transport.bpm))
        return transport.bpm;
    return 120.0;
}

void markersFor(const TimePattern& pattern, double ppq, float& play, float& marker) noexcept
{
    const double windowBeats = pattern.windowBeats > 1.0e-6 ? pattern.windowBeats : 1.0;
    const double safe = std::isfinite(ppq) ? ppq : 0.0;
    const double windowStart = std::floor(safe / windowBeats) * windowBeats;
    const double into = std::clamp(safe - windowStart, 0.0, windowBeats);
    play = static_cast<float>(into / windowBeats);
    if (pattern.mode == TimeMode::CatchUp)
    {
        marker = pattern.rate >= 0.0f ? std::clamp(pattern.rate * play, 0.0f, 1.0f)
                                      : std::clamp(1.0f + pattern.rate * play, 0.0f, 1.0f);
    }
    else
    {
        const double slice = pattern.sliceBeats > 1.0e-6 ? pattern.sliceBeats : windowBeats;
        marker = static_cast<float>(std::fmod(into, slice) / windowBeats);
    }
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
    lastWindow = 0;
    lastSlice = -1;
    anchorPos = 0.0;
    primed = false;
    patternDirty = false;
    holding = false;
    playheadPhase.store(0.0f, std::memory_order_relaxed);
    readMarker.store(0.0f, std::memory_order_relaxed);
    hostSynced.store(0, std::memory_order_relaxed);
    inputScope.clear();
    outputScope.clear();
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
    const bool host = transport.playing && transport.hasPpq && std::isfinite(transport.ppq);
    hostSynced.store(host ? 1 : 0, std::memory_order_relaxed);
    const bool bpmKnown = transport.hasBpm && transport.bpm > 1.0 && std::isfinite(transport.bpm);
    hostBpmSeen.store(bpmKnown ? transport.bpm : 0.0, std::memory_order_relaxed);

    if (!host)
    {
        holding = true;
        for (int sample = 0; sample < numSamples; ++sample)
        {
            for (int ch = 0; ch < count; ++ch)
                buffer[static_cast<std::size_t>(ch * capacity + writeIndex)] = channels[ch][sample];
            writeIndex = wrapIndex(writeIndex + 1, capacity);
            mixSmoother.next();
        }
        if (transport.hasPpq && std::isfinite(transport.ppq))
        {
            float play = 0.0f;
            float marker = 0.0f;
            markersFor(pattern, transport.ppq, play, marker);
            (void)marker;
            playheadPhase.store(std::clamp(play, 0.0f, 1.0f), std::memory_order_relaxed);
            readMarker.store(std::clamp(play, 0.0f, 1.0f), std::memory_order_relaxed);
        }
        return;
    }

    if (holding)
    {
        patternDirty = true;
        holding = false;
    }

    const double bpm = hostBpm(transport);
    const double beatsPerSample = bpm / (60.0 * sampleRate);
    const double blockStart = transport.ppq;
    const double windowBeats = pattern.windowBeats > 1.0e-6 ? pattern.windowBeats : 1.0;
    const int fadeSpan = fadeSamplesFor(pattern, bpm);

    float lastPlay = 0.0f;
    float lastMarker = 0.0f;

    if (patternDirty)
    {
        inputScope.clear();
        outputScope.clear();
    }

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

        float inPeak = 0.0f;
        float outPeak = 0.0f;
        for (int ch = 0; ch < count; ++ch)
        {
            float wet = readCubic(ch, readPos);
            if (fading)
                wet = readCubic(ch, fadeReadPos) * (1.0f - fadeT) + wet * fadeT;

            const float dry = channels[ch][sample];
            const float out = dry + (wet - dry) * mixNow;
            channels[ch][sample] = out;
            inPeak = std::max(inPeak, std::abs(dry));
            outPeak = std::max(outPeak, std::abs(out));
        }

        writeIndex = wrapIndex(writeIndex + 1, capacity);
        readPos = wrapPosition(readPos + static_cast<double>(activeRate), static_cast<double>(capacity));
        if (fading)
        {
            fadeReadPos = wrapPosition(fadeReadPos + static_cast<double>(fadeRate), static_cast<double>(capacity));
            --fadeRemaining;
        }

        markersFor(pattern, ppq, lastPlay, lastMarker);
        inputScope.push(static_cast<double>(lastPlay), inPeak);
        outputScope.push(static_cast<double>(lastPlay), outPeak);
    }

    playheadPhase.store(std::clamp(lastPlay, 0.0f, 1.0f), std::memory_order_relaxed);
    readMarker.store(std::clamp(lastMarker, 0.0f, 1.0f), std::memory_order_relaxed);
}
} // namespace turtletime::dsp
