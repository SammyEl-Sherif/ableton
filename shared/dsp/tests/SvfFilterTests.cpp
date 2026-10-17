#include <animalaudio/dsp/EqBand.h>
#include <animalaudio/dsp/SvfFilter.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

using animalaudio::dsp::EqBand;
using animalaudio::dsp::FilterShape;
using animalaudio::dsp::SvfCoefficients;
using animalaudio::dsp::SvfFilter;
using Catch::Approx;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;
constexpr FilterShape kShapes[] = {FilterShape::Bell, FilterShape::LowShelf, FilterShape::HighShelf};

std::vector<float> noise(int n, unsigned seed = 1)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(static_cast<std::size_t>(n));
    for (auto& x : v)
        x = dist(rng);
    return v;
}

std::vector<float> sine(double frequency, int n, float amplitude = 0.5f)
{
    std::vector<float> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t>(i)] =
            amplitude * static_cast<float>(std::sin(kTwoPi * frequency * static_cast<double>(i) / kRate));
    return v;
}

double rms(const std::vector<float>& v, std::size_t from)
{
    double sum = 0.0;
    for (std::size_t i = from; i < v.size(); ++i)
        sum += static_cast<double>(v[i]) * v[i];
    return std::sqrt(sum / static_cast<double>(v.size() - from));
}

/** Gain in dB measured by running a sine through `process` (after the filter settled). */
template <class Process>
double measuredGainDb(double frequency, Process&& process)
{
    const int n = 48000;
    auto in = sine(frequency, n);
    auto out = in;
    process(out);
    return 20.0 * std::log10(rms(out, n / 2) / rms(in, n / 2));
}
} // namespace

TEST_CASE("SVF at 0 dB is a bit-exact pass-through for every shape")
{
    for (auto shape : kShapes)
    {
        const auto c = SvfCoefficients::make(shape, 1000.0, 0.9, 0.0, kRate);
        REQUIRE(c.m0 == 1.0);
        REQUIRE(c.m1 == 0.0);
        REQUIRE(c.m2 == 0.0);

        SvfFilter f;
        f.setCoefficients(c);
        for (float x : noise(4096))
            REQUIRE(f.process(x) == x);
    }
}

TEST_CASE("SVF bell peaks at its centre frequency with the requested gain")
{
    for (double gain : {-18.0, -6.0, 3.0, 12.0, 18.0})
        for (double fc : {60.0, 1000.0, 9000.0})
        {
            const auto c = SvfCoefficients::make(FilterShape::Bell, fc, 1.2, gain, kRate);
            REQUIRE(c.magnitudeDbAt(fc, kRate) == Approx(gain).margin(1.0e-6));
            REQUIRE(std::abs(c.magnitudeDbAt(fc / 40.0, kRate)) < 0.25);
        }
}

TEST_CASE("SVF shelves reach their gain at one end, half of it at the corner and 0 dB at the other end")
{
    const double gain = 12.0;
    const auto low = SvfCoefficients::make(FilterShape::LowShelf, 500.0, SvfCoefficients::kButterworthQ, gain, kRate);
    REQUIRE(low.magnitudeDbAt(10.0, kRate) == Approx(gain).margin(0.05));
    REQUIRE(low.magnitudeDbAt(500.0, kRate) == Approx(gain * 0.5).margin(1.0e-6));
    REQUIRE(low.magnitudeDbAt(20000.0, kRate) == Approx(0.0).margin(0.05));

    const auto high =
        SvfCoefficients::make(FilterShape::HighShelf, 4000.0, SvfCoefficients::kButterworthQ, -gain, kRate);
    REQUIRE(high.magnitudeDbAt(23900.0, kRate) == Approx(-gain).margin(0.1));
    REQUIRE(high.magnitudeDbAt(4000.0, kRate) == Approx(-gain * 0.5).margin(1.0e-6));
    REQUIRE(high.magnitudeDbAt(40.0, kRate) == Approx(0.0).margin(0.05));
}

TEST_CASE("SVF processing matches the analytic response")
{
    for (auto shape : kShapes)
        for (double testFrequency : {80.0, 700.0, 3000.0, 12000.0})
        {
            const auto c = SvfCoefficients::make(shape, 1000.0, 1.5, 9.0, kRate);
            SvfFilter f;
            f.setCoefficients(c);
            const double measured = measuredGainDb(testFrequency,
                                                   [&](std::vector<float>& v)
                                                   {
                                                       for (auto& x : v)
                                                           x = f.process(x);
                                                   });
            INFO("shape " << static_cast<int>(shape) << " at " << testFrequency << " Hz");
            REQUIRE(measured == Approx(c.magnitudeDbAt(testFrequency, kRate)).margin(0.05));
        }
}

TEST_CASE("SVF clamps frequencies above Nyquist and stays stable")
{
    for (auto shape : kShapes)
    {
        SvfFilter f;
        f.setCoefficients(SvfCoefficients::make(shape, 20000.0, 4.0, 18.0, 22050.0));
        for (float x : noise(22050))
        {
            const float y = f.process(x);
            REQUIRE(std::isfinite(y));
            REQUIRE(std::abs(y) < 100.0f);
        }
    }
}

TEST_CASE("EqBand at 0 dB is a bit-exact pass-through")
{
    EqBand band(FilterShape::Bell);
    band.setTarget(1000.0, 0.0, 1.0);
    band.prepare(kRate);
    auto in = noise(8192);
    auto out = in;
    float* channels[] = {out.data()};
    band.process(channels, 1, static_cast<int>(out.size()));
    REQUIRE(out == in);
}

TEST_CASE("EqBand glides to its target and then has the target response")
{
    for (auto shape : kShapes)
    {
        EqBand band(shape);
        band.setTarget(300.0, 0.0, 1.0);
        band.prepare(kRate);
        band.setTarget(2000.0, -9.0, 2.0);
        const double target = band.targetCoefficients().magnitudeDbAt(1500.0, kRate);
        const double measured = measuredGainDb(1500.0,
                                               [&](std::vector<float>& v)
                                               {
                                                   float* channels[] = {v.data()};
                                                   band.process(channels, 1, static_cast<int>(v.size()));
                                               });
        REQUIRE(measured == Approx(target).margin(0.05));
    }
}

TEST_CASE("EqBand output does not depend on the block size")
{
    const auto input = noise(24000, 7);
    const auto render = [&](int blockSize)
    {
        EqBand band(FilterShape::LowShelf);
        band.setTarget(200.0, 0.0, 0.7);
        band.prepare(kRate);
        auto data = input;
        for (int start = 0; start < static_cast<int>(data.size()); start += blockSize)
        {
            if (start >= 6000 && start < 6000 + blockSize)
                band.setTarget(900.0, 15.0, 0.7);
            float* channels[] = {data.data() + start};
            band.process(channels, 1, std::min(blockSize, static_cast<int>(data.size()) - start));
        }
        return data;
    };
    // the target changes at sample 6000 for every block size that divides it
    const auto reference = render(1);
    for (int blockSize : {2, 3, 16, 48, 500})
        REQUIRE(render(blockSize) == reference);
}

TEST_CASE("EqBand gain jumps are click-free")
{
    const double frequency = 200.0;
    EqBand band(FilterShape::Bell);
    band.setTarget(frequency, 0.0, 1.0);
    band.prepare(kRate);

    auto signal = sine(frequency, 96000);
    float* channels[] = {signal.data()};
    band.process(channels, 1, 24000);
    band.setTarget(frequency, 18.0, 1.0);
    float* rest[] = {signal.data() + 24000};
    band.process(rest, 1, 72000);

    const auto maxStep = [&](std::size_t from, std::size_t to)
    {
        float m = 0.0f;
        for (std::size_t i = from + 1; i < to; ++i)
            m = std::max(m, std::abs(signal[i] - signal[i - 1]));
        return m;
    };
    const float settled = maxStep(72000, 96000);
    REQUIRE(settled > 0.5f * 0.026f * 7.0f); // the +18 dB boost really happened
    REQUIRE(maxStep(23990, 30000) <= settled * 1.05f);
}
