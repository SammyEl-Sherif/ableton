#include <animalaudio/dsp/FilterDesign.h>
#include <animalaudio/dsp/SvfFilter.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

using animalaudio::dsp::designFilter;
using animalaudio::dsp::effectiveSlope;
using animalaudio::dsp::FilterCascade;
using animalaudio::dsp::FilterShape;
using animalaudio::dsp::FilterSpec;
using animalaudio::dsp::kFilterSlopes;
using animalaudio::dsp::SvfCoefficients;
using animalaudio::dsp::SvfFilter;
using Catch::Approx;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kPi = 3.14159265358979323846;
constexpr FilterShape kGainShapes[] = {FilterShape::Bell, FilterShape::LowShelf, FilterShape::HighShelf,
                                       FilterShape::TiltShelf, FilterShape::FlatTilt};

FilterSpec spec(FilterShape shape, double hz, double gainDb, double q = 1.0, int slope = 12)
{
    FilterSpec s;
    s.shape = shape;
    s.frequency = hz;
    s.gainDb = gainDb;
    s.q = q;
    s.slope = slope;
    return s;
}

double db(const FilterCascade& c, double hz)
{
    return c.magnitudeDbAt(hz, kRate);
}

/** Analog frequency (prewarped) of `hz` relative to the corner `fc`. */
double warpedRatio(double hz, double fc)
{
    return std::tan(kPi * hz / kRate) / std::tan(kPi * fc / kRate);
}

std::vector<float> noise(int n, unsigned seed = 1)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(static_cast<std::size_t>(n));
    for (auto& x : v)
        x = dist(rng);
    return v;
}

/** Runs a sine through the cascade and measures its gain once the filter settled. */
double measuredGainDb(const FilterCascade& c, double hz)
{
    std::vector<SvfFilter> filters(static_cast<std::size_t>(c.numSections));
    for (int i = 0; i < c.numSections; ++i)
        filters[static_cast<std::size_t>(i)].setCoefficients(c.sections[static_cast<std::size_t>(i)]);
    const int n = 96000;
    double in = 0.0, out = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.25f * static_cast<float>(std::sin(2.0 * kPi * hz * i / kRate));
        float y = x;
        for (auto& f : filters)
            y = f.process(y);
        y *= static_cast<float>(c.gain);
        if (i >= n / 2)
        {
            in += static_cast<double>(x) * x;
            out += static_cast<double>(y) * y;
        }
    }
    return 10.0 * std::log10(out / in);
}
} // namespace

TEST_CASE("Every gain shape at 0 dB is an empty cascade, the others never are")
{
    for (auto shape : kGainShapes)
        for (int slope : kFilterSlopes)
            REQUIRE(designFilter(spec(shape, 1000.0, 0.0, 1.0, slope), kRate).isIdentity());
    for (auto shape :
         {FilterShape::LowCut, FilterShape::HighCut, FilterShape::Notch, FilterShape::BandPass, FilterShape::AllPass})
        REQUIRE(designFilter(spec(shape, 1000.0, 0.0), kRate).numSections > 0);
}

TEST_CASE("An SVF section with equal numerator and denominator is a bit-exact pass-through")
{
    const auto c = SvfCoefficients::fromAnalog(1.0, 0.37, 0.09, 0.37, 0.09);
    REQUIRE(c.isIdentity());
    SvfFilter f;
    f.setCoefficients(c);
    for (float x : noise(4096))
        REQUIRE(f.process(x) == x);
}

TEST_CASE("Bells peak with the requested gain and reach half of it at the Q edges, for every slope")
{
    for (int slope : {12, 24, 36, 48, 72, 96})
        for (double gain : {-18.0, -6.0, 3.0, 12.0, 30.0})
            for (double fc : {60.0, 1000.0, 9000.0})
                for (double q : {0.3, 1.0, 4.0})
                {
                    INFO("slope " << slope << " gain " << gain << " fc " << fc << " q " << q);
                    const auto c = designFilter(spec(FilterShape::Bell, fc, gain, q, slope), kRate);
                    REQUIRE(db(c, fc) == Approx(gain).margin(1.0e-6));
                    // band edge: q (w - 1/w) = 1 in the prewarped domain
                    const double w = (1.0 + std::sqrt(1.0 + 4.0 * q * q)) / (2.0 * q);
                    const double edge = kRate / kPi * std::atan(w * std::tan(kPi * fc / kRate));
                    REQUIRE(db(c, edge) == Approx(gain / 2.0).margin(1.0e-6));
                    REQUIRE(std::abs(db(c, fc / (40.0 / q))) < std::abs(gain) * 0.02 + 0.05);
                }
}

TEST_CASE("Steeper bells have flatter tops and steeper skirts")
{
    const double fc = 1000.0;
    const auto gentle = designFilter(spec(FilterShape::Bell, fc, 12.0, 1.0, 12), kRate);
    const auto steep = designFilter(spec(FilterShape::Bell, fc, 12.0, 1.0, 48), kRate);
    REQUIRE(db(steep, fc * 1.3) > db(gentle, fc * 1.3));
    REQUIRE(db(steep, fc * 3.0) < db(gentle, fc * 3.0));
}

TEST_CASE("The 12 dB bell is the RBJ peaking filter")
{
    const double fc = 2500.0, gain = 7.5, q = 1.7;
    const auto c = designFilter(spec(FilterShape::Bell, fc, gain, q), kRate);
    REQUIRE(c.numSections == 1);
    const double a = std::pow(10.0, gain / 40.0);
    for (double hz : {100.0, 900.0, 2500.0, 7000.0, 19000.0})
    {
        const double w = warpedRatio(hz, fc);
        const std::complex<double> s(0.0, w);
        const double rbj = std::abs((s * s + s * (a / q) + 1.0) / (s * s + s / (a * q) + 1.0));
        REQUIRE(db(c, hz) == Approx(20.0 * std::log10(rbj)).margin(1.0e-9));
    }
}

TEST_CASE("Shelves reach their gain at one end, half of it at the corner and 0 dB at the other end")
{
    for (int slope : kFilterSlopes)
        for (double q : {0.5, 1.0, 2.0})
        {
            INFO("slope " << slope << " q " << q);
            const auto low = designFilter(spec(FilterShape::LowShelf, 500.0, 12.0, q, slope), kRate);
            REQUIRE(db(low, 500.0) == Approx(6.0).margin(1.0e-6));
            REQUIRE(db(low, 10.0) == Approx(12.0).margin(slope == 6 ? 0.5 : 0.1));
            REQUIRE(db(low, 20000.0) == Approx(0.0).margin(slope == 6 ? 0.5 : 0.1));

            const auto high = designFilter(spec(FilterShape::HighShelf, 3000.0, -12.0, q, slope), kRate);
            REQUIRE(db(high, 3000.0) == Approx(-6.0).margin(1.0e-6));
            REQUIRE(db(high, 23900.0) == Approx(-12.0).margin(slope == 6 ? 0.5 : 0.1));
            REQUIRE(db(high, 20.0) == Approx(0.0).margin(slope == 6 ? 0.5 : 0.1));
        }
}

TEST_CASE("The 12 dB shelf with Q 1 is the RBJ shelf with Q 0.707")
{
    const double fc = 800.0, gain = 9.0;
    const auto c = designFilter(spec(FilterShape::LowShelf, fc, gain), kRate);
    const double a = std::pow(10.0, gain / 40.0);
    const double k = 1.0 / SvfCoefficients::kButterworthQ;
    for (double hz : {30.0, 400.0, 800.0, 1600.0, 12000.0})
    {
        const std::complex<double> s(0.0, warpedRatio(hz, fc));
        const double rbj = std::abs(a * (s * s + std::sqrt(a) * k * s + a) / (a * s * s + std::sqrt(a) * k * s + 1.0));
        REQUIRE(db(c, hz) == Approx(20.0 * std::log10(rbj)).margin(1.0e-9));
    }
}

TEST_CASE("The tilt shelf pivots around 0 dB at its frequency")
{
    for (int slope : {6, 12, 24})
    {
        const auto c = designFilter(spec(FilterShape::TiltShelf, 1000.0, 10.0, 1.0, slope), kRate);
        REQUIRE(db(c, 1000.0) == Approx(0.0).margin(1.0e-6));
        REQUIRE(db(c, 15.0) == Approx(-5.0).margin(0.3));
        REQUIRE(db(c, 22000.0) == Approx(5.0).margin(0.3));
    }
}

TEST_CASE("Cuts are Butterworth: -3 dB at the corner, then the slope")
{
    for (int slope : kFilterSlopes)
    {
        INFO("slope " << slope);
        const int order = slope / 6;
        const auto analytic = [&](double ratio) { return -10.0 * std::log10(1.0 + std::pow(ratio, 2.0 * order)); };

        const double fc = 1000.0;
        const auto low = designFilter(spec(FilterShape::LowCut, fc, 0.0, 1.0, slope), kRate);
        const auto high = designFilter(spec(FilterShape::HighCut, fc, 0.0, 1.0, slope), kRate);
        for (double hz : {50.0, 250.0, 700.0, 1000.0, 1400.0, 4000.0, 15000.0})
        {
            if (const double expected = analytic(1.0 / warpedRatio(hz, fc)); expected > -120.0)
                REQUIRE(db(low, hz) == Approx(expected).margin(1.0e-6));
            if (const double expected = analytic(warpedRatio(hz, fc)); expected > -120.0)
                REQUIRE(db(high, hz) == Approx(expected).margin(1.0e-6));
        }
        REQUIRE(db(low, fc) == Approx(-3.0103).margin(1.0e-3));
    }
}

TEST_CASE("Q above 1 makes cuts resonate at the corner")
{
    const auto flat = designFilter(spec(FilterShape::LowCut, 1000.0, 0.0, 1.0, 24), kRate);
    const auto resonant = designFilter(spec(FilterShape::LowCut, 1000.0, 0.0, 4.0, 24), kRate);
    REQUIRE(db(flat, 1100.0) < 0.0);
    REQUIRE(db(resonant, 1100.0) > 6.0);
}

TEST_CASE("Notch, band pass and all pass")
{
    for (int slope : {12, 24, 48})
    {
        INFO("slope " << slope);
        const auto notch = designFilter(spec(FilterShape::Notch, 2000.0, 0.0, 2.0, slope), kRate);
        REQUIRE(db(notch, 2000.0) < -100.0);
        REQUIRE(std::abs(db(notch, 200.0)) < 0.05);
        REQUIRE(std::abs(db(notch, 15000.0)) < 0.05);

        const auto bandPass = designFilter(spec(FilterShape::BandPass, 2000.0, 0.0, 2.0, slope), kRate);
        REQUIRE(db(bandPass, 2000.0) == Approx(0.0).margin(1.0e-6));
        REQUIRE(db(bandPass, 200.0) < -18.0);

        const auto allPass = designFilter(spec(FilterShape::AllPass, 2000.0, 0.0, 2.0, slope), kRate);
        for (double hz : {30.0, 500.0, 2000.0, 9000.0})
            REQUIRE(db(allPass, hz) == Approx(0.0).margin(1.0e-9));
    }
    const auto allPass = designFilter(spec(FilterShape::AllPass, 2000.0, 0.0, 2.0, 12), kRate);
    REQUIRE(allPass.responseAt(2000.0, kRate).real() == Approx(-1.0).margin(1.0e-9));
}

TEST_CASE("The flat tilt is a straight line through 0 dB at the pivot")
{
    const double gain = 10.0; // 2 dB per octave
    const auto c = designFilter(spec(FilterShape::FlatTilt, 640.0, gain), kRate);
    REQUIRE(db(c, 640.0) == Approx(0.0).margin(1.0e-9));
    for (double octaves : {-4.0, -2.0, -1.0, 1.0, 2.0})
        REQUIRE(db(c, 640.0 * std::exp2(octaves)) == Approx(octaves * gain / 5.0).margin(0.1));
    // the top shelves sit close to Nyquist, so the line bends a little above 10 kHz
    REQUIRE(db(c, 640.0 * 16.0) == Approx(8.0).margin(0.35));
}

TEST_CASE("Effective slopes")
{
    REQUIRE(effectiveSlope(FilterShape::Bell, 6) == 12);
    REQUIRE(effectiveSlope(FilterShape::Bell, 18) == 24);
    REQUIRE(effectiveSlope(FilterShape::Bell, 96) == 96);
    REQUIRE(effectiveSlope(FilterShape::LowCut, 6) == 6);
    REQUIRE(effectiveSlope(FilterShape::LowCut, 96) == 96);
    REQUIRE(effectiveSlope(FilterShape::FlatTilt, 48) == 0);
}

TEST_CASE("Processing a cascade matches its analytic response")
{
    const FilterSpec specs[] = {
        spec(FilterShape::Bell, 1000.0, 9.0, 1.5, 12),     spec(FilterShape::Bell, 1000.0, -12.0, 0.7, 48),
        spec(FilterShape::LowShelf, 300.0, 6.0, 1.0, 18),  spec(FilterShape::HighShelf, 5000.0, -9.0, 1.0, 36),
        spec(FilterShape::LowCut, 200.0, 0.0, 1.0, 96),    spec(FilterShape::HighCut, 3000.0, 0.0, 2.0, 30),
        spec(FilterShape::Notch, 1500.0, 0.0, 3.0, 24),    spec(FilterShape::BandPass, 800.0, 0.0, 1.0, 12),
        spec(FilterShape::TiltShelf, 1000.0, 8.0, 1.0, 6), spec(FilterShape::FlatTilt, 1000.0, -6.0),
    };
    for (const auto& s : specs)
    {
        const auto c = designFilter(s, kRate);
        for (double hz : {80.0, 700.0, 3000.0, 12000.0})
        {
            const double expected = db(c, hz);
            if (expected < -60.0)
                continue;
            INFO("shape " << static_cast<int>(s.shape) << " slope " << s.slope << " at " << hz << " Hz");
            REQUIRE(measuredGainDb(c, hz) == Approx(expected).margin(0.05));
        }
    }
}

TEST_CASE("Extreme designs stay stable")
{
    const auto input = noise(22050);
    for (int shape = 0; shape < animalaudio::dsp::kNumFilterShapes; ++shape)
        for (int slope : {6, 96})
            for (double q : {0.025, 40.0})
                for (double hz : {10.0, 21000.0, 40000.0})
                {
                    const auto c = designFilter(spec(static_cast<FilterShape>(shape), hz, 30.0, q, slope), 44100.0);
                    std::vector<SvfFilter> filters(static_cast<std::size_t>(c.numSections));
                    for (int i = 0; i < c.numSections; ++i)
                        filters[static_cast<std::size_t>(i)].setCoefficients(c.sections[static_cast<std::size_t>(i)]);
                    for (float x : input)
                    {
                        float y = x;
                        for (auto& f : filters)
                            y = f.process(y);
                        REQUIRE(std::isfinite(y));
                    }
                }
}
