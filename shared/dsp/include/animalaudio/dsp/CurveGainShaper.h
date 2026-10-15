#pragma once

#include "CycleScope.h"
#include "ShapeCurve.h"
#include "Smoothing.h"

#include <algorithm>
#include <cmath>

namespace animalaudio::dsp
{
/**
 * Tempo-synced gain shaper: a baked curve (1 = unity, 0 = silent) sampled at the cycle phase, scaled
 * by `mix`, then declicked. This is Duck's engine; plugins whose effect is "a curve drives a gain" use
 * it directly, others copy its structure (table lookup + smoothers + CycleScope) around their own DSP.
 * JUCE-free and allocation-free after construction.
 *
 * Per sample:  curveGain = table(phase)
 *              target    = 1 - mix * (1 - curveGain)
 *              gain      = one-pole(target)       (declick: ~0.7 ms time constant)
 */
class CurveGainShaper
{
public:
    static constexpr int kScopeBins = 128;
    static constexpr double kGainSmoothingMs = 0.7;
    static constexpr double kMixSmoothingMs = 15.0;

    using Scope = CycleScope<kScopeBins>;

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
        scope.clear();
    }

    void setTable(const CurveTable& newTable) noexcept { table = newTable; }

    void setMix(float mix) noexcept { mixSmoother.setTarget(std::min(std::max(mix, 0.0f), 1.0f)); }

    /** Applies the shaped gain in place. `startPhase` in [0,1), `increment` in cycles/sample. */
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

            scope.push(phase, peak);

            phase += increment;
            if (phase >= 1.0)
                phase -= std::floor(phase);
        }

        scope.setPhase(phase);
    }

    const Scope& getScope() const noexcept { return scope; }
    float getDisplayPhase() const noexcept { return scope.getPhase(); }
    float getScopeBin(int index) const noexcept { return scope.getBin(index); }
    float getCurrentGain() const noexcept { return gainSmoother.getCurrent(); }

private:
    CurveTable table;
    OnePoleSmoother gainSmoother;
    OnePoleSmoother mixSmoother;
    Scope scope;
};
} // namespace animalaudio::dsp
