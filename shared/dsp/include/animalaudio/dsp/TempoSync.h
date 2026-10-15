#pragma once

#include <cmath>

namespace animalaudio::dsp
{
/** Note-length divisions, expressed as one cycle per division. */
enum class Division : int
{
    Eighth = 0, // 1/8 note  = 0.5 beats
    Quarter,    // 1/4 note  = 1 beat
    Half,       // 1/2 note  = 2 beats
    Whole,      // 1/1 note  = 4 beats
    Count
};

constexpr int kNumDivisions = static_cast<int>(Division::Count);

inline const char* divisionLabel(Division d) noexcept
{
    switch (d)
    {
    case Division::Eighth:
        return "1/8";
    case Division::Quarter:
        return "1/4";
    case Division::Half:
        return "1/2";
    case Division::Whole:
        return "1/1";
    case Division::Count:
        break;
    }
    return "?";
}

/** Length of one cycle in quarter-note beats (the unit hosts use for PPQ). */
constexpr double beatsPerCycle(Division d) noexcept
{
    switch (d)
    {
    case Division::Eighth:
        return 0.5;
    case Division::Quarter:
        return 1.0;
    case Division::Half:
        return 2.0;
    case Division::Whole:
        return 4.0;
    case Division::Count:
        break;
    }
    return 1.0;
}

inline Division divisionFromIndex(int index) noexcept
{
    if (index < 0)
        index = 0;
    if (index >= kNumDivisions)
        index = kNumDivisions - 1;
    return static_cast<Division>(index);
}

/** Wrap into [0, 1). */
inline double wrapPhase(double phase) noexcept
{
    phase -= std::floor(phase);
    return phase >= 1.0 ? 0.0 : phase;
}

/** Cycle phase [0,1) for a host PPQ (quarter-note) position. */
inline double phaseFromPpq(double ppq, Division d) noexcept
{
    return wrapPhase(ppq / beatsPerCycle(d));
}

/** Cycles advanced per sample. */
inline double phaseIncrementPerSample(double bpm, double sampleRate, Division d) noexcept
{
    return (bpm / 60.0) / (beatsPerCycle(d) * sampleRate);
}

/** Number of samples in one cycle (useful for tests/UI). */
inline double samplesPerCycle(double bpm, double sampleRate, Division d) noexcept
{
    return 1.0 / phaseIncrementPerSample(bpm, sampleRate, d);
}

/** What the host told us about the transport for the current block. */
struct TransportInfo
{
    bool playing = false;
    bool hasBpm = false;
    double bpm = 120.0;
    bool hasPpq = false;
    double ppq = 0.0;
};

enum class SyncSource
{
    Host,   // locked to the host playhead
    FreeRun // running on its own clock (transport stopped or no position info)
};

/**
 * Produces a cycle phase for each block. While the host transport is running and reports a PPQ
 * position we lock to it; otherwise (stopped, or host gives no position) we free-run using the
 * host BPM if known, or the supplied fallback BPM.
 */
class PhaseTracker
{
public:
    struct Block
    {
        double startPhase = 0.0;
        double increment = 0.0;
        SyncSource source = SyncSource::FreeRun;
    };

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        freePhase = 0.0;
    }

    void reset() noexcept { freePhase = 0.0; }

    Block beginBlock(const TransportInfo& transport, Division division, double fallbackBpm, int numSamples) noexcept
    {
        const double bpm = (transport.hasBpm && transport.bpm > 0.0) ? transport.bpm : fallbackBpm;

        Block block;
        block.increment = phaseIncrementPerSample(bpm, sampleRate, division);

        if (transport.playing && transport.hasPpq)
        {
            block.startPhase = phaseFromPpq(transport.ppq, division);
            block.source = SyncSource::Host;
        }
        else
        {
            block.startPhase = freePhase;
            block.source = SyncSource::FreeRun;
        }

        freePhase = wrapPhase(block.startPhase + block.increment * numSamples);
        return block;
    }

private:
    double sampleRate = 44100.0;
    double freePhase = 0.0;
};
} // namespace animalaudio::dsp
