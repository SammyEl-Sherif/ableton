#include <animalaudio/dsp/EqBand.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

using animalaudio::dsp::EqBand;
using animalaudio::dsp::FilterShape;
using animalaudio::dsp::FilterSpec;
using Catch::Approx;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

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

double rmsDb(const std::vector<float>& v, std::size_t from, std::size_t to)
{
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double>(v[i]) * v[i];
    return 10.0 * std::log10(sum / static_cast<double>(to - from));
}

void run(EqBand& band, std::vector<float>& data, int from, int count, const float* dynamic = nullptr)
{
    float* channels[] = {data.data() + from};
    band.process(channels, 1, count, dynamic);
}

float maxStep(const std::vector<float>& v, std::size_t from, std::size_t to)
{
    float m = 0.0f;
    for (std::size_t i = from + 1; i < to; ++i)
        m = std::max(m, std::abs(v[i] - v[i - 1]));
    return m;
}
} // namespace

TEST_CASE("EqBand at 0 dB is a bit-exact pass-through")
{
    EqBand band;
    band.setTarget(spec(FilterShape::Bell, 1000.0, 0.0));
    band.prepare(kRate);
    const auto in = noise(8192);
    auto out = in;
    run(band, out, 0, static_cast<int>(out.size()));
    REQUIRE(out == in);
}

TEST_CASE("An inactive EqBand is idle and leaves the signal untouched")
{
    EqBand band;
    band.setActive(false);
    band.setTarget(spec(FilterShape::LowCut, 2000.0, 0.0, 1.0, 48));
    band.prepare(kRate);
    REQUIRE(band.isIdle());
    const auto in = noise(4096);
    auto out = in;
    run(band, out, 0, static_cast<int>(out.size()));
    REQUIRE(out == in);
}

TEST_CASE("EqBand glides to its target and then has the target response")
{
    for (auto shape : {FilterShape::Bell, FilterShape::LowShelf, FilterShape::HighShelf, FilterShape::LowCut,
                       FilterShape::HighCut, FilterShape::Notch, FilterShape::TiltShelf})
    {
        EqBand band;
        band.setTarget(spec(shape, 300.0, 0.0));
        band.prepare(kRate);
        band.setTarget(spec(shape, 2000.0, -9.0, 2.0, 24));
        const double expected = band.targetCascade().magnitudeDbAt(1500.0, kRate);
        auto signal = sine(1500.0, 48000);
        const double before = rmsDb(signal, 24000, 48000);
        run(band, signal, 0, 48000);
        INFO("shape " << static_cast<int>(shape));
        REQUIRE(rmsDb(signal, 24000, 48000) - before == Approx(expected).margin(0.05));
    }
}

TEST_CASE("EqBand applies the dynamic gain offset")
{
    EqBand band;
    band.setTarget(spec(FilterShape::Bell, 1000.0, 3.0));
    band.prepare(kRate);
    const std::vector<float> offset(48000, -9.0f);
    auto signal = sine(1000.0, 48000);
    const double before = rmsDb(signal, 24000, 48000);
    run(band, signal, 0, 48000, offset.data());
    REQUIRE(rmsDb(signal, 24000, 48000) - before == Approx(-6.0).margin(0.02));
}

TEST_CASE("EqBand output does not depend on the block size")
{
    const auto input = noise(24000, 7);
    std::vector<float> dynamic(input.size());
    for (std::size_t i = 0; i < dynamic.size(); ++i)
        dynamic[i] = -6.0f * static_cast<float>(std::sin(static_cast<double>(i) * 0.001));

    const auto render = [&](int blockSize)
    {
        EqBand band;
        band.setTarget(spec(FilterShape::LowShelf, 200.0, 0.0, 0.7));
        band.prepare(kRate);
        auto data = input;
        for (int start = 0; start < static_cast<int>(data.size()); start += blockSize)
        {
            if (start == 6000)
                band.setTarget(spec(FilterShape::LowShelf, 900.0, 15.0, 0.7));
            if (start == 12000)
                band.setTarget(spec(FilterShape::HighCut, 900.0, 15.0, 0.7, 36));
            if (start == 18000)
                band.setActive(false);
            run(band, data, start, std::min(blockSize, static_cast<int>(data.size()) - start), dynamic.data() + start);
        }
        return data;
    };
    // the changes happen at samples 6000/12000/18000 for every block size that divides them
    const auto reference = render(1);
    for (int blockSize : {2, 3, 16, 48, 500})
        REQUIRE(render(blockSize) == reference);
}

TEST_CASE("EqBand gain jumps are click-free")
{
    const double frequency = 200.0;
    EqBand band;
    band.setTarget(spec(FilterShape::Bell, frequency, 0.0));
    band.prepare(kRate);

    auto signal = sine(frequency, 96000);
    run(band, signal, 0, 24000);
    band.setTarget(spec(FilterShape::Bell, frequency, 18.0));
    run(band, signal, 24000, 72000);

    const float settled = maxStep(signal, 72000, 96000);
    REQUIRE(settled > 0.5f * 0.026f * 7.0f); // the +18 dB boost really happened
    REQUIRE(maxStep(signal, 23990, 30000) <= settled * 1.05f);
}

TEST_CASE("EqBand shape changes and enabling crossfade without clicks")
{
    const double frequency = 300.0;
    EqBand band;
    band.setTarget(spec(FilterShape::Bell, frequency, 0.0));
    band.setActive(false);
    band.prepare(kRate);

    auto signal = sine(frequency, 96000);
    const float reference = maxStep(signal, 0, 4800);
    run(band, signal, 0, 12000);
    band.setActive(true);
    band.setTarget(spec(FilterShape::Bell, frequency, -12.0));
    run(band, signal, 12000, 24000);
    band.setTarget(spec(FilterShape::LowCut, frequency * 4.0, 0.0, 1.0, 48));
    run(band, signal, 36000, 24000);
    band.setTarget(spec(FilterShape::HighShelf, 100.0, 3.0));
    run(band, signal, 60000, 24000);
    band.setActive(false);
    run(band, signal, 84000, 12000);

    REQUIRE(maxStep(signal, 0, 96000) <= reference * 2.0f);
    REQUIRE(band.isIdle());
    // the low cut far above the sine really removed it once the fade finished
    REQUIRE(rmsDb(signal, 48000, 60000) < -60.0);
}
