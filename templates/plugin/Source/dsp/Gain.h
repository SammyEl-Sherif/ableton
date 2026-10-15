#pragma once

#include <animalaudio/dsp/Smoothing.h>

#include <cmath>

namespace __NAME_LOWER__
{
inline float decibelsToLinear(float db) noexcept
{
    return db <= -100.0f ? 0.0f : std::pow(10.0f, db * 0.05f);
}

/** Starter DSP block: smoothed gain. Replace with the real effect. JUCE-free so it is unit-testable. */
class Gain
{
public:
    void prepare(double sampleRate)
    {
        smoother.prepare(sampleRate, 10.0);
        smoother.reset(1.0f);
    }

    void setDecibels(float db) noexcept { smoother.setTarget(decibelsToLinear(db)); }

    void process(float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoother.next();
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= g;
        }
    }

private:
    animalaudio::dsp::OnePoleSmoother smoother;
};
} // namespace __NAME_LOWER__
