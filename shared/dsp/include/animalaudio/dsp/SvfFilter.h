#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace animalaudio::dsp
{
enum class FilterShape
{
    Bell,
    LowShelf,
    HighShelf
};

/**
 * Coefficients of a linear trapezoidal state-variable filter (Andrew Simper's SVF). Bell and shelves have the
 * RBJ-cookbook responses, but unlike a direct-form biquad the topology stays well behaved when the coefficients
 * change every few samples and at very low frequencies in single precision.
 *
 * Output = m0 * input + m1 * bandpass + m2 * lowpass.
 */
struct SvfCoefficients
{
    static constexpr double kButterworthQ = 0.70710678118654752;

    double g = 0.0;
    double k = 1.0;
    double m0 = 1.0;
    double m1 = 0.0;
    double m2 = 0.0;

    /** `frequency` is clamped into [10 Hz, 0.49 * sampleRate], `q` to >= 0.05. Shelves use `q` for their slope
        (kButterworthQ = no overshoot). Gain 0 dB gives exactly m0 = 1, m1 = m2 = 0 (a bit-exact pass-through). */
    static SvfCoefficients make(FilterShape shape, double frequency, double q, double gainDb,
                                double sampleRate) noexcept
    {
        const double f = std::clamp(frequency, 10.0, 0.49 * sampleRate);
        const double a = std::pow(10.0, gainDb / 40.0);
        const double w = std::tan(kPi * f / sampleRate);
        q = std::max(q, 0.05);

        SvfCoefficients c;
        switch (shape)
        {
        case FilterShape::Bell:
            c.g = w;
            c.k = 1.0 / (q * a);
            c.m1 = c.k * (a * a - 1.0);
            break;
        case FilterShape::LowShelf:
            c.g = w / std::sqrt(a);
            c.k = 1.0 / q;
            c.m1 = c.k * (a - 1.0);
            c.m2 = a * a - 1.0;
            break;
        case FilterShape::HighShelf:
            c.g = w * std::sqrt(a);
            c.k = 1.0 / q;
            c.m0 = a * a;
            c.m1 = c.k * (1.0 - a) * a;
            c.m2 = 1.0 - a * a;
            break;
        }
        return c;
    }

    /** Exact magnitude of the digital filter at `frequency` (trapezoidal integration = bilinear transform). */
    double magnitudeAt(double frequency, double sampleRate) const noexcept
    {
        const double f = std::clamp(frequency, 0.0, 0.4999 * sampleRate);
        const std::complex<double> s(0.0, std::tan(kPi * f / sampleRate) / g);
        const auto denominator = s * s + k * s + 1.0;
        return std::abs(m0 + (m1 * s + m2) / denominator);
    }

    double magnitudeDbAt(double frequency, double sampleRate) const noexcept
    {
        return 20.0 * std::log10(std::max(magnitudeAt(frequency, sampleRate), 1.0e-9));
    }

private:
    static constexpr double kPi = 3.14159265358979323846;
};

/** One channel of the SVF. Allocation free; change coefficients at any time. */
class SvfFilter
{
public:
    void setCoefficients(const SvfCoefficients& c) noexcept
    {
        const double a1d = 1.0 / (1.0 + c.g * (c.g + c.k));
        a1 = static_cast<float>(a1d);
        a2 = static_cast<float>(c.g * a1d);
        a3 = static_cast<float>(c.g * c.g * a1d);
        m0 = static_cast<float>(c.m0);
        m1 = static_cast<float>(c.m1);
        m2 = static_cast<float>(c.m2);
    }

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    float process(float v0) noexcept
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return m0 * v0 + m1 * v1 + m2 * v2;
    }

private:
    float a1 = 1.0f, a2 = 0.0f, a3 = 0.0f;
    float m0 = 1.0f, m1 = 0.0f, m2 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};
} // namespace animalaudio::dsp
