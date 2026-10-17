#pragma once

#include "SvfFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <utility>

namespace animalaudio::dsp
{
enum class FilterShape
{
    Bell,
    LowShelf,
    LowCut,
    HighShelf,
    HighCut,
    Notch,
    BandPass,
    TiltShelf,
    FlatTilt,
    AllPass
};

inline constexpr int kNumFilterShapes = 10;

/** Slopes (dB/octave) a filter can be asked for. Bell, notch, band pass and all pass realise them in 12 dB steps. */
inline constexpr std::array<int, 9> kFilterSlopes{6, 12, 18, 24, 30, 36, 48, 72, 96};

constexpr bool shapeHasGain(FilterShape s) noexcept
{
    return s == FilterShape::Bell || s == FilterShape::LowShelf || s == FilterShape::HighShelf ||
           s == FilterShape::TiltShelf || s == FilterShape::FlatTilt;
}

constexpr bool shapeHasSlope(FilterShape s) noexcept
{
    return s != FilterShape::FlatTilt;
}

constexpr bool isBandShape(FilterShape s) noexcept
{
    return s == FilterShape::Bell || s == FilterShape::Notch || s == FilterShape::BandPass || s == FilterShape::AllPass;
}

/** Order of the analog prototype: Butterworth order for cuts and shelves, low-pass prototype order (before the
    band transform) for bell, notch, band pass and all pass. */
constexpr int prototypeOrder(FilterShape s, int slope) noexcept
{
    if (s == FilterShape::FlatTilt)
        return 1;
    if (isBandShape(s))
        return std::clamp((slope + 6) / 12, 1, 8);
    return std::clamp(slope / 6, 1, 16);
}

/** The slope the shape actually realises for a requested slope (0 for the flat tilt, which has none). */
constexpr int effectiveSlope(FilterShape s, int slope) noexcept
{
    if (s == FilterShape::FlatTilt)
        return 0;
    return prototypeOrder(s, slope) * (isBandShape(s) ? 12 : 6);
}

/** First-order (6 dB/oct) cuts and shelves have no resonance, so no Q. */
constexpr bool shapeHasQ(FilterShape s, int slope) noexcept
{
    return s != FilterShape::FlatTilt && (isBandShape(s) || prototypeOrder(s, slope) > 1);
}

struct FilterSpec
{
    FilterShape shape = FilterShape::Bell;
    double frequency = 1000.0;
    double gainDb = 0.0;
    double q = 1.0;
    int slope = 12;
};

/** Up to kMaxSections SVF sections in series followed by a gain. */
struct FilterCascade
{
    static constexpr int kMaxSections = 8;

    std::array<SvfCoefficients, kMaxSections> sections{};
    int numSections = 0;
    double gain = 1.0;

    bool isIdentity() const noexcept { return numSections == 0 && gain == 1.0; }

    std::complex<double> responseAt(double frequency, double sampleRate) const noexcept
    {
        std::complex<double> h(gain, 0.0);
        for (int i = 0; i < numSections; ++i)
            h *= sections[static_cast<std::size_t>(i)].responseAt(frequency, sampleRate);
        return h;
    }

    double magnitudeDbAt(double frequency, double sampleRate) const noexcept
    {
        return 20.0 * std::log10(std::max(std::abs(responseAt(frequency, sampleRate)), 1.0e-9));
    }
};

namespace filter_design_detail
{
constexpr double kPi = 3.14159265358979323846;

class Builder
{
public:
    explicit Builder(FilterCascade& target) noexcept
        : cascade(target)
    {
    }

    void biquad(double b2, double b1, double b0, double a1, double a0) noexcept
    {
        if (cascade.numSections < FilterCascade::kMaxSections)
            cascade.sections[static_cast<std::size_t>(cascade.numSections++)] =
                SvfCoefficients::fromAnalog(b2, b1, b0, a1, a0);
    }

    /** (b1 s + b0) / (s + a0). Two first-order sections share one (overdamped) SVF section. */
    void firstOrder(double b1, double b0, double a0) noexcept
    {
        if (!hasPending)
        {
            pendingB1 = b1;
            pendingB0 = b0;
            pendingA0 = a0;
            hasPending = true;
            return;
        }
        hasPending = false;
        biquad(pendingB1 * b1, pendingB1 * b0 + pendingB0 * b1, pendingB0 * b0, pendingA0 + a0, pendingA0 * a0);
    }

    /** A first-order section left without a partner becomes (b1 s + b0)(s + a0) / (s + a0)^2. */
    void finish() noexcept
    {
        if (!hasPending)
            return;
        hasPending = false;
        biquad(pendingB1, pendingB1 * pendingA0 + pendingB0, pendingB0 * pendingA0, 2.0 * pendingA0,
               pendingA0 * pendingA0);
    }

private:
    FilterCascade& cascade;
    bool hasPending = false;
    double pendingB1 = 0.0, pendingB0 = 0.0, pendingA0 = 1.0;
};

/** Damping k = 2 cos(angle) of the conjugate pole pairs of an order-n Butterworth polynomial, most damped first
    (the last one has the highest Q). Odd orders also have a real pole at -1. */
inline int butterworthDampings(int order, std::array<double, 8>& k) noexcept
{
    int count = 0;
    for (int i = 1; i <= order / 2 && count < 8; ++i)
    {
        const double angle = order % 2 == 0 ? kPi * (2.0 * i - 1.0) / (2.0 * order) : kPi * i / order;
        k[static_cast<std::size_t>(count++)] = 2.0 * std::cos(angle);
    }
    return count;
}

/** Monic quadratic s^2 + c1 s + c0. */
struct Quadratic
{
    double c1 = 0.0;
    double c0 = 1.0;
};

inline Quadratic conjugatePair(std::complex<double> root) noexcept
{
    return {-2.0 * root.real(), std::norm(root)};
}

/** Quadratics of the band-pass transform s -> q (s + 1/s) (centre 1, bandwidth 1/q) applied to the roots of an
    order-n Butterworth polynomial scaled to `radius`. Each complex prototype pair gives two quadratics (one above
    and one below the centre, always in that order), the real root of an odd order one. */
inline int bandTransformQuadratics(int order, double radius, double q, std::array<Quadratic, 8>& out) noexcept
{
    std::array<double, 8> k{};
    const int pairs = butterworthDampings(order, k);
    int count = 0;
    for (int i = 0; i < pairs && count + 1 < 8; ++i)
    {
        const double ki = k[static_cast<std::size_t>(i)];
        const std::complex<double> p(-0.5 * ki * radius, radius * std::sqrt(std::max(0.0, 1.0 - 0.25 * ki * ki)));
        const auto d = std::sqrt(p * p - 4.0 * q * q);
        out[static_cast<std::size_t>(count++)] = conjugatePair((p + d) / (2.0 * q));
        out[static_cast<std::size_t>(count++)] = conjugatePair((p - d) / (2.0 * q));
    }
    if (order % 2 == 1 && count < 8)
        out[static_cast<std::size_t>(count++)] = {radius / q, 1.0};
    return count;
}

/** Holters/Zoelzer low shelf of order n: DC gain `gain` (linear), 1 at high frequencies, exactly half the dB gain
    at w for any Q. Order 2 with q = 1 is the RBJ shelf with Q = 0.707. */
inline void lowShelf(Builder& b, double gain, int order, double q, double w) noexcept
{
    std::array<double, 8> k{};
    const int pairs = butterworthDampings(order, k);
    if (pairs > 0)
        k[static_cast<std::size_t>(pairs - 1)] /= q;
    const double zero = w * std::pow(gain, 0.5 / order);
    const double pole = w * std::pow(gain, -0.5 / order);
    for (int i = 0; i < pairs; ++i)
    {
        const double ki = k[static_cast<std::size_t>(i)];
        b.biquad(1.0, ki * zero, zero * zero, ki * pole, pole * pole);
    }
    if (order % 2 == 1)
        b.firstOrder(1.0, zero, pole);
}

/** Butterworth high pass (low cut) or low pass (high cut), -3 dB at w; q scales the most resonant section. */
inline void butterworthCut(Builder& b, bool highPass, int order, double q, double w) noexcept
{
    std::array<double, 8> k{};
    const int pairs = butterworthDampings(order, k);
    if (pairs > 0)
        k[static_cast<std::size_t>(pairs - 1)] /= q;
    for (int i = 0; i < pairs; ++i)
    {
        const double ki = k[static_cast<std::size_t>(i)];
        if (highPass)
            b.biquad(1.0, 0.0, 0.0, ki * w, w * w);
        else
            b.biquad(0.0, 0.0, w * w, ki * w, w * w);
    }
    if (order % 2 == 1)
    {
        if (highPass)
            b.firstOrder(1.0, 0.0, w);
        else
            b.firstOrder(0.0, w, w);
    }
}

/** A straight tilt of gainDb / 5 dB per octave across the audible range (+-gainDb at 20 Hz and 20 kHz when the
    pivot is in the middle), built from first-order shelves an octave apart and normalised to 0 dB at the pivot. */
inline void flatTilt(Builder& b, FilterCascade& cascade, double gainDb, double pivotHz, double sampleRate) noexcept
{
    constexpr int kSteps = 12;
    const double stepGain = std::pow(10.0, gainDb / 5.0 / 20.0);
    const double rootGain = std::sqrt(stepGain);
    for (int i = 0; i < kSteps; ++i)
    {
        const double hz = std::min(20.0 * std::exp2(i - 0.5), 0.45 * sampleRate);
        const double w = std::tan(kPi * hz / sampleRate);
        b.firstOrder(stepGain, w * rootGain, w * rootGain);
    }
    b.finish();
    cascade.gain = 1.0 / std::max(std::abs(cascade.responseAt(pivotHz, sampleRate)), 1.0e-9);
}
} // namespace filter_design_detail

/**
 * Designs `spec` as a cascade of SVF sections: analog prototypes mapped with the bilinear transform, prewarped so
 * the corner/centre frequency is exact. Shapes with gain and 0 dB give an empty cascade (a bit-exact
 * pass-through). The frequency is clamped into [10 Hz, 0.49 * sampleRate], q into [0.025, 40].
 * No allocation; a few tan/pow/sqrt per call, so it can run on the audio thread every few samples.
 */
inline FilterCascade designFilter(const FilterSpec& spec, double sampleRate) noexcept
{
    namespace d = filter_design_detail;

    FilterCascade cascade;
    if (shapeHasGain(spec.shape) && spec.gainDb == 0.0)
        return cascade;

    const double hz = std::clamp(spec.frequency, 10.0, 0.49 * sampleRate);
    const double w = std::tan(d::kPi * hz / sampleRate);
    const double q = std::clamp(spec.q, 0.025, 40.0);
    const double gain = std::pow(10.0, spec.gainDb / 20.0);
    const int order = prototypeOrder(spec.shape, spec.slope);

    d::Builder b(cascade);
    switch (spec.shape)
    {
    case FilterShape::LowShelf:
        d::lowShelf(b, gain, order, q, w);
        break;
    case FilterShape::HighShelf:
        d::lowShelf(b, 1.0 / gain, order, q, w);
        cascade.gain = gain;
        break;
    case FilterShape::TiltShelf:
        d::lowShelf(b, 1.0 / gain, order, q, w);
        cascade.gain = std::sqrt(gain);
        break;
    case FilterShape::LowCut:
        d::butterworthCut(b, true, order, q, w);
        break;
    case FilterShape::HighCut:
        d::butterworthCut(b, false, order, q, w);
        break;
    case FilterShape::FlatTilt:
        d::flatTilt(b, cascade, spec.gainDb, hz, sampleRate);
        return cascade;
    case FilterShape::Bell:
    {
        std::array<d::Quadratic, 8> zeros{}, poles{};
        const int count = d::bandTransformQuadratics(order, std::pow(gain, 0.5 / order), q, zeros);
        d::bandTransformQuadratics(order, std::pow(gain, -0.5 / order), q, poles);
        for (std::size_t i = 0; i < static_cast<std::size_t>(count); ++i)
            b.biquad(1.0, zeros[i].c1 * w, zeros[i].c0 * w * w, poles[i].c1 * w, poles[i].c0 * w * w);
        break;
    }
    case FilterShape::Notch:
    case FilterShape::BandPass:
    case FilterShape::AllPass:
    {
        std::array<d::Quadratic, 8> poles{};
        const int count = d::bandTransformQuadratics(order, 1.0, q, poles);
        for (std::size_t i = 0; i < static_cast<std::size_t>(count); ++i)
        {
            const double a1 = poles[i].c1 * w;
            const double a0 = poles[i].c0 * w * w;
            if (spec.shape == FilterShape::Notch)
                b.biquad(1.0, 0.0, w * w, a1, a0);
            else if (spec.shape == FilterShape::BandPass)
                b.biquad(0.0, w / q, 0.0, a1, a0);
            else
                b.biquad(1.0, -a1, a0, a1, a0);
        }
        break;
    }
    }
    b.finish();
    return cascade;
}
} // namespace animalaudio::dsp
