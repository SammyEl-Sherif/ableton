#pragma once

#include <cmath>
#include <functional>

namespace animalaudio::dsp
{
/**
 * Attack/release envelope follower (one pole each way) for level detection. Feed it a rectified or squared
 * signal; allocation free, safe on the audio thread.
 */
class EnvelopeFollower
{
public:
    /** Per-step coefficient of a one-pole with time constant `ms` when stepped `rate` times per second. */
    static float coefficient(double ms, double rate) noexcept
    {
        const double steps = ms * 0.001 * rate;
        return steps > 0.0 ? static_cast<float>(std::exp(-1.0 / steps)) : 0.0f;
    }

    void prepare(double newSampleRate) noexcept
    {
        sampleRate = newSampleRate;
        attackCoeff = coefficient(attackMs, sampleRate);
        releaseCoeff = coefficient(releaseMs, sampleRate);
    }

    /** Cheap when the times did not change, so it can be called every block. */
    void setTimes(double newAttackMs, double newReleaseMs) noexcept
    {
        const std::equal_to<double> same;
        if (!same(newAttackMs, attackMs))
        {
            attackMs = newAttackMs;
            attackCoeff = coefficient(attackMs, sampleRate);
        }
        if (!same(newReleaseMs, releaseMs))
        {
            releaseMs = newReleaseMs;
            releaseCoeff = coefficient(releaseMs, sampleRate);
        }
    }

    void reset(float value = 0.0f) noexcept { envelope = value; }

    float process(float input) noexcept
    {
        const float c = input > envelope ? attackCoeff : releaseCoeff;
        envelope = input + c * (envelope - input);
        return envelope;
    }

    float getEnvelope() const noexcept { return envelope; }

private:
    double sampleRate = 44100.0;
    double attackMs = 10.0;
    double releaseMs = 100.0;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float envelope = 0.0f;
};
} // namespace animalaudio::dsp
