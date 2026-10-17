#pragma once

#include <algorithm>
#include <cmath>
#include <complex>

namespace animalaudio::dsp
{
/**
 * Coefficients of one linear trapezoidal state-variable filter section (Andrew Simper's SVF). Unlike a
 * direct-form biquad the topology stays well behaved when the coefficients change every few samples and at very
 * low frequencies in single precision.
 *
 * Output = m0 * input + m1 * bandpass + m2 * lowpass, i.e. H(s) = m0 + (m1 s' + m2) / (s'^2 + k s' + 1) with
 * s' = s / g, where s is the prewarped analog frequency (s = j tan(pi f / fs) on the unit circle).
 */
struct SvfCoefficients
{
    static constexpr double kButterworthQ = 0.70710678118654752;

    double g = 0.0;
    double k = 2.0;
    double m0 = 1.0;
    double m1 = 0.0;
    double m2 = 0.0;

    /** The section (b2 s^2 + b1 s + b0) / (s^2 + a1 s + a0) in the prewarped domain; a0 > 0 and a1 > 0 (stable).
        Identical numerator and denominator give exactly m0 = 1, m1 = m2 = 0 (a bit-exact pass-through). */
    static SvfCoefficients fromAnalog(double b2, double b1, double b0, double a1, double a0) noexcept
    {
        SvfCoefficients c;
        const double w0 = std::sqrt(a0);
        c.g = w0;
        c.k = a1 / w0;
        c.m0 = b2;
        c.m1 = b1 / w0 - b2 * c.k;
        c.m2 = b0 / a0 - b2;
        return c;
    }

    bool isIdentity() const noexcept { return m0 == 1.0 && m1 == 0.0 && m2 == 0.0; }

    /** Exact complex response of the digital section at `frequency` (trapezoidal integration = bilinear
        transform). */
    std::complex<double> responseAt(double frequency, double sampleRate) const noexcept
    {
        if (m1 == 0.0 && m2 == 0.0)
            return {m0, 0.0};
        const double f = std::clamp(frequency, 0.0, 0.4999 * sampleRate);
        const std::complex<double> s(0.0, std::tan(kPi * f / sampleRate) / g);
        return m0 + (m1 * s + m2) / (s * s + k * s + 1.0);
    }

    double magnitudeAt(double frequency, double sampleRate) const noexcept
    {
        return std::abs(responseAt(frequency, sampleRate));
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
