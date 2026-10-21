#pragma once

#include "TimePatterns.h"

#include <animalaudio/dsp/Smoothing.h>
#include <animalaudio/dsp/TempoSync.h>

#include <atomic>
#include <vector>

namespace turtletime::dsp
{
/**
 * Tempo-locked time warp: record into a circular buffer at 1x and read it back at a pattern's rate.
 *
 * On each musical window the read pointer snaps (catch-up) or a captured slice repeats. A short
 * crossfade hides the jump. The window is locked to host PPQ while the transport is running, and
 * free-runs from `fallbackBpm` otherwise. Memory is allocated only in prepare().
 */
class TurtleTimeEngine
{
public:
    static constexpr int kMaxChannels = 2;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset() noexcept;

    void setPatternIndex(int index) noexcept;
    void setMix(float newMix) noexcept;
    void setFallbackBpm(double bpm) noexcept;

    void process(float* const* channels, int numChannels, int numSamples,
                 const animalaudio::dsp::TransportInfo& transport) noexcept;

    int getPatternIndex() const noexcept { return patternIndex; }
    float getPlayheadPhase() const noexcept { return playheadPhase.load(std::memory_order_relaxed); }
    float getReadMarker() const noexcept { return readMarker.load(std::memory_order_relaxed); }
    bool isHostSynced() const noexcept { return hostSynced.load(std::memory_order_relaxed) != 0; }

private:
    void startFade(double fromPosition, float fromRate, int span) noexcept;
    float readCubic(int channel, double position) const noexcept;
    int fadeSamplesFor(const TimePattern& pattern, double bpm) const noexcept;

    std::vector<float> buffer;
    int capacity = 0;
    int writeIndex = 0;

    double readPos = 0.0;
    float activeRate = 1.0f;

    double fadeReadPos = 0.0;
    float fadeRate = 1.0f;
    int fadeRemaining = 0;
    int fadeSpanSamples = 1;

    double ppqClock = 0.0;
    int lastWindow = 0;
    int lastSlice = -1;
    double anchorPos = 0.0;
    bool primed = false;
    bool patternDirty = false;

    animalaudio::dsp::OnePoleSmoother mixSmoother;
    float mix = 1.0f;
    int patternIndex = 0;
    double fallbackBpm = 120.0;
    double sampleRate = 48000.0;
    int crossfadeSamples = 1;

    std::atomic<float> playheadPhase{0.0f};
    std::atomic<float> readMarker{0.0f};
    std::atomic<int> hostSynced{0};
};
} // namespace turtletime::dsp
