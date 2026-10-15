#pragma once

#include <animalaudio/dsp/ShapeCurve.h>
#include <animalaudio/dsp/Smoothing.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace duck
{
/**
 * The ducking DSP. JUCE-free and allocation-free after construction so it can be unit tested and
 * run on the audio thread.
 *
 * Per sample:  curveGain = table(phase)          (1 = unity, 0 = fully ducked)
 *              target    = 1 - mix * (1 - curveGain)
 *              gain      = one-pole(target)       (declick: ~0.7 ms time constant)
 */
class DuckEngine
{
public:
    static constexpr int kScopeBins = 128;
    static constexpr double kGainSmoothingMs = 0.7;
    static constexpr double kMixSmoothingMs = 15.0;

    void prepare(double newSampleRate)
    {
        gainSmoother.prepare(newSampleRate, kGainSmoothingMs);
        mixSmoother.prepare(newSampleRate, kMixSmoothingMs);
        table.fillConstant(1.0f);
        reset(0.0f);
    }

    void reset(float mix) noexcept
    {
        mixSmoother.reset(mix);
        gainSmoother.reset(1.0f);
        for (auto& bin : scope)
            bin.store(0.0f, std::memory_order_relaxed);
    }

    /** Replace the curve table (called on the audio thread with a table already copied there). */
    void setTable(const animalaudio::dsp::CurveTable& newTable) noexcept { table = newTable; }

    void setMix(float mix) noexcept { mixSmoother.setTarget(std::min(std::max(mix, 0.0f), 1.0f)); }

    /** Applies ducking in place. `startPhase` in [0,1), `increment` in cycles/sample. */
    void process(float* const* channels, int numChannels, int numSamples, double startPhase, double increment) noexcept
    {
        double phase = startPhase;

        for (int i = 0; i < numSamples; ++i)
        {
            const float curveGain = table.lookup(phase);
            const float mix = mixSmoother.next();
            gainSmoother.setTarget(1.0f - mix * (1.0f - curveGain));
            const float gain = gainSmoother.next();

            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                const float out = channels[ch][i] * gain;
                channels[ch][i] = out;
                peak = std::max(peak, std::abs(out));
            }

            const int bin = std::min(static_cast<int>(phase * kScopeBins), kScopeBins - 1);
            const float old = scope[static_cast<std::size_t>(bin)].load(std::memory_order_relaxed);
            scope[static_cast<std::size_t>(bin)].store(std::max(peak, old * 0.9995f), std::memory_order_relaxed);

            phase += increment;
            if (phase >= 1.0)
                phase -= std::floor(phase);
        }

        displayPhase.store(static_cast<float>(phase), std::memory_order_relaxed);
    }

    float getDisplayPhase() const noexcept { return displayPhase.load(std::memory_order_relaxed); }
    float getScopeBin(int index) const noexcept
    {
        return scope[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
    }

    float getCurrentGain() const noexcept { return gainSmoother.getCurrent(); }

private:
    animalaudio::dsp::CurveTable table;
    animalaudio::dsp::OnePoleSmoother gainSmoother;
    animalaudio::dsp::OnePoleSmoother mixSmoother;
    std::atomic<float> displayPhase{0.0f};
    std::array<std::atomic<float>, kScopeBins> scope{};
};
} // namespace duck
