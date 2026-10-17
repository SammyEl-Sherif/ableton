#pragma once

#include <cmath>
#include <functional>

namespace animalaudio::dsp
{
/** One-pole parameter/gain smoother. Allocation free, safe on the audio thread. */
class OnePoleSmoother
{
public:
    void prepare(double sampleRate, double timeConstantMs) noexcept
    {
        const double tau = timeConstantMs * 0.001 * sampleRate;
        coeff = tau > 0.0 ? static_cast<float>(std::exp(-1.0 / tau)) : 0.0f;
    }

    void reset(float value) noexcept { current = target = value; }

    void setTarget(float newTarget) noexcept { target = newTarget; }

    float next() noexcept
    {
        current = target + coeff * (current - target);
        if (std::abs(current - target) < 2.0e-5f)
            current = target;
        return current;
    }

    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }

    /** True once next() has snapped exactly onto the target (it stays there until the target changes). */
    bool isSettled() const noexcept { return std::equal_to<float>()(current, target); }

private:
    float coeff = 0.0f;
    float current = 0.0f;
    float target = 0.0f;
};
} // namespace animalaudio::dsp
