#include "dsp/QuailEngine.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <functional>
#include <random>
#include <tuple>
#include <vector>

using namespace quail::dsp;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

using Signal = std::vector<float>;

Signal sine(double hz, float amplitude, int n)
{
    Signal s(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        s[static_cast<std::size_t>(i)] = amplitude * static_cast<float>(std::sin(kTwoPi * hz * i / kRate));
    return s;
}

Signal noise(int n, float amplitude, unsigned seed)
{
    std::mt19937 random(seed);
    std::uniform_real_distribution<float> dist(-amplitude, amplitude);
    Signal s(static_cast<std::size_t>(n));
    for (auto& x : s)
        x = dist(random);
    return s;
}

Signal plus(Signal a, const Signal& b)
{
    for (std::size_t i = 0; i < a.size(); ++i)
        a[i] += b[i];
    return a;
}

/** Level (dBFS, peak amplitude) of the `hz` component of x[from, to). */
double toneDb(const Signal& x, double hz, int from, int to)
{
    double re = 0.0, im = 0.0;
    for (int i = from; i < to; ++i)
    {
        re += x[static_cast<std::size_t>(i)] * std::cos(kTwoPi * hz * i / kRate);
        im += x[static_cast<std::size_t>(i)] * std::sin(kTwoPi * hz * i / kRate);
    }
    return 20.0 * std::log10(2.0 * std::sqrt(re * re + im * im) / (to - from) + 1e-12);
}

struct Stereo
{
    Signal left, right;
};

/** Runs the engine over the signals in `block`-sized calls; the side chain is optional, `mono` uses one channel. */
Stereo run(Engine& engine, const Settings& settings, Stereo in, const Stereo* sidechain = nullptr, int block = 512,
           bool mono = false, const std::function<void(int, Settings&)>& automate = {})
{
    auto s = settings;
    const int n = static_cast<int>(in.left.size());
    for (int start = 0; start < n; start += block)
    {
        const int len = std::min(block, n - start);
        if (automate)
            automate(start, s);
        float* channels[] = {in.left.data() + start, in.right.data() + start};
        const float* keys[] = {sidechain != nullptr ? sidechain->left.data() + start : nullptr,
                               sidechain != nullptr ? sidechain->right.data() + start : nullptr};
        engine.setSettings(s);
        engine.process(channels, mono ? 1 : 2, keys, sidechain != nullptr ? 2 : 0, len);
    }
    return in;
}

BandSettings band(FilterShape shape, float hz, float gainDb = 0.0f, float q = 1.0f, int slope = 12)
{
    BandSettings b;
    b.used = true;
    b.shape = shape;
    b.frequency = hz;
    b.gainDb = gainDb;
    b.q = q;
    b.slope = slope;
    return b;
}

void prepare(Engine& engine, const Settings& settings, int spectralOrderToUse = 0)
{
    engine.setSettings(settings);
    engine.setSpectralOrder(spectralOrderToUse);
    engine.prepare(kRate);
}
} // namespace

TEST_CASE("quail: an empty or flat EQ is a bit-exact pass-through")
{
    const auto input = noise(20000, 0.5f, 1);
    for (bool withFlatBands : {false, true})
    {
        Settings s;
        if (withFlatBands)
        {
            s.bands[0] = band(FilterShape::Bell, 1000.0f, 0.0f);
            s.bands[3] = band(FilterShape::LowShelf, 100.0f, 0.0f, 0.7f, 24);
            s.bands[7] = band(FilterShape::FlatTilt, 1000.0f, 0.0f);
        }
        Engine engine;
        prepare(engine, s);
        const auto out = run(engine, s, {input, input});
        REQUIRE(out.left == input);
        REQUIRE(out.right == input);
    }
}

TEST_CASE("quail: after automation returns to flat the pass-through is bit-exact again")
{
    const auto input = noise(48000, 0.25f, 4);
    Settings s;
    s.bands[2] = band(FilterShape::Bell, 1500.0f, 0.0f, 0.7f);
    Engine engine;
    prepare(engine, s);
    const auto out = run(engine, s, {input, input}, nullptr, 512, false,
                         [](int start, Settings& t)
                         {
                             const bool moved = start < 9600;
                             t.global.outputDb = moved ? 6.0f : 0.0f;
                             t.bands[2].gainDb = moved ? 9.0f : 0.0f;
                         });
    for (std::size_t i = 0; i < 9600; i += 97)
        REQUIRE(std::abs(out.left[i] - input[i]) > 0.0f);
    for (std::size_t i = 30000; i < input.size(); ++i)
    {
        REQUIRE(out.left[i] == input[i]);
        REQUIRE(out.right[i] == input[i]);
    }
}

TEST_CASE("quail: static bands match the response the UI draws")
{
    const std::vector<BandSettings> bands{
        band(FilterShape::Bell, 1000.0f, 6.0f, 1.0f),        band(FilterShape::Bell, 300.0f, -9.0f, 4.0f),
        band(FilterShape::LowShelf, 150.0f, 6.0f, 0.7f, 12), band(FilterShape::HighShelf, 6000.0f, -8.0f, 0.7f, 24),
        band(FilterShape::LowCut, 120.0f, 0.0f, 0.7f, 24),   band(FilterShape::HighCut, 4000.0f, 0.0f, 0.7f, 48),
        band(FilterShape::Notch, 1000.0f, 0.0f, 2.0f),       band(FilterShape::BandPass, 2000.0f, 0.0f, 1.5f),
        band(FilterShape::TiltShelf, 1000.0f, 6.0f, 0.7f),   band(FilterShape::FlatTilt, 1000.0f, 4.0f),
        band(FilterShape::AllPass, 1000.0f, 0.0f, 1.0f),     band(FilterShape::Bell, 2500.0f, 5.0f, 1.0f, 36)};
    for (const auto& b : bands)
        for (double hz : {60.0, 250.0, 1000.0, 2500.0, 9000.0})
        {
            INFO("shape " << static_cast<int>(b.shape) << " slope " << b.slope << " at " << hz << " Hz");
            Settings s;
            s.bands[5] = b;
            Engine engine;
            prepare(engine, s);
            const int n = 14400;
            const auto in = sine(hz, 0.25f, n);
            const auto out = run(engine, s, {in, in});
            const double expected = bandResponseDb(b, hz, kRate, 1.0f);
            const double measured = toneDb(out.left, hz, n / 2, n) - toneDb(in, hz, n / 2, n);
            if (expected < -40.0)
                REQUIRE(measured < -35.0);
            else
                REQUIRE(measured == Catch::Approx(expected).margin(0.15));
        }
}

TEST_CASE("quail: the output does not depend on the block size")
{
    Settings s;
    s.bands[0] = band(FilterShape::Bell, 900.0f, 4.0f, 1.2f);
    s.bands[0].rangeDb = -6.0f;
    s.bands[0].autoThreshold = false;
    s.bands[0].thresholdDb = -30.0f;
    s.bands[1] = band(FilterShape::LowShelf, 120.0f, 3.0f, 0.7f, 24);
    s.bands[1].placement = Placement::Left;
    s.bands[2] = band(FilterShape::Bell, 4000.0f, -5.0f, 2.0f);
    s.bands[2].placement = Placement::Mid;
    s.bands[3] = band(FilterShape::HighCut, 12000.0f, 0.0f, 0.7f, 48);
    s.global.outputDb = -2.0f;

    const int n = 30000;
    const Stereo in{plus(noise(n, 0.3f, 2), sine(900.0, 0.4f, n)), noise(n, 0.3f, 3)};
    const auto withSpectral = [&](bool spectral)
    {
        auto settings = s;
        settings.bands[4] = band(FilterShape::Bell, 2000.0f, 0.0f, 0.5f);
        settings.bands[4].rangeDb = -9.0f;
        settings.bands[4].spectral = spectral;
        settings.bands[4].used = spectral;
        return settings;
    };
    for (bool spectral : {false, true})
    {
        INFO("spectral " << spectral);
        const auto settings = withSpectral(spectral);
        const int order = spectral ? spectralOrder(SpectralResolution::Low, kRate) : 0;
        Engine reference;
        prepare(reference, settings, order);
        const auto expected = run(reference, settings, in, nullptr, 512);
        for (int block : {1, 37, 1000})
        {
            INFO("block " << block);
            Engine engine;
            prepare(engine, settings, order);
            const auto out = run(engine, settings, in, nullptr, block);
            float worst = 0.0f;
            for (std::size_t i = 0; i < out.left.size(); ++i)
                worst = std::max(
                    {worst, std::abs(out.left[i] - expected.left[i]), std::abs(out.right[i] - expected.right[i])});
            REQUIRE(worst < 1.0e-5f);
        }
    }
}

TEST_CASE("quail: a dynamic band ducks when the side chain is loud")
{
    Settings s;
    auto& b = s.bands[0];
    b = band(FilterShape::Bell, 1000.0f, 0.0f, 1.0f);
    b.rangeDb = -12.0f;
    b.autoThreshold = false;
    b.thresholdDb = -40.0f;
    b.externalSidechain = true;
    b.attackMs = 5.0f;
    b.releaseMs = 50.0f;

    const int n = 96000;
    const auto music = sine(1000.0, 0.25f, n);
    auto key = sine(1000.0, 0.5f, n);
    std::fill(key.begin(), key.begin() + 48000, 0.0f);

    Engine engine;
    prepare(engine, s);
    const Stereo sidechain{key, key};
    const auto out = run(engine, s, {music, music}, &sidechain, 256);
    const double before = toneDb(out.left, 1000.0, 24000, 48000) - toneDb(music, 1000.0, 24000, 48000);
    const double during = toneDb(out.left, 1000.0, 72000, 96000) - toneDb(music, 1000.0, 72000, 96000);
    REQUIRE(std::abs(before) < 0.2);
    REQUIRE(during < -10.5);
    REQUIRE(during > -13.0);
    REQUIRE(engine.getBandDynamicDb(0) < -10.0f);

    // without the external key the band ignores its own (loud) input
    Engine unkeyed;
    prepare(unkeyed, s);
    const auto dry = run(unkeyed, s, {music, music}, nullptr, 256);
    REQUIRE(std::abs(toneDb(dry.left, 1000.0, 72000, 96000) - toneDb(music, 1000.0, 72000, 96000)) < 0.2);
}

TEST_CASE("quail: self-triggered dynamics act only above the threshold")
{
    const auto measure = [](float rangeDb, float amplitude)
    {
        Settings s;
        auto& b = s.bands[2];
        b = band(FilterShape::Bell, 1000.0f, 0.0f, 1.0f);
        b.rangeDb = rangeDb;
        b.autoThreshold = false;
        b.thresholdDb = -30.0f;
        Engine engine;
        prepare(engine, s);
        const int n = 48000;
        const auto in = sine(1000.0, amplitude, n);
        const auto out = run(engine, s, {in, in});
        return toneDb(out.left, 1000.0, n / 2, n) - toneDb(in, 1000.0, n / 2, n);
    };
    REQUIRE(measure(-8.0f, 0.5f) < -6.0);
    REQUIRE(std::abs(measure(-8.0f, 0.003f)) < 0.2);
    REQUIRE(measure(6.0f, 0.5f) > 4.0);
}

TEST_CASE("quail: spectral dynamics duck only the frequencies the side chain plays")
{
    Settings s;
    auto& b = s.bands[0];
    b = band(FilterShape::Bell, 2000.0f, 0.0f, 0.3f);
    b.rangeDb = -15.0f;
    b.autoThreshold = false;
    b.thresholdDb = -50.0f;
    b.externalSidechain = true;
    b.spectral = true;
    b.spectralDensity = 0.8f;
    b.spectralTilt = false;
    b.attackMs = 5.0f;
    b.releaseMs = 50.0f;

    const int n = 4 * 48000;
    const auto music = plus(sine(1000.0, 0.1f, n), sine(3000.0, 0.1f, n));
    const auto key = sine(3000.0, 0.3f, n);

    Engine engine;
    prepare(engine, s, spectralOrder(SpectralResolution::Medium, kRate));
    REQUIRE(engine.getLatencySamples() == 4096);
    const Stereo sidechain{key, key};
    const auto out = run(engine, s, {music, music}, &sidechain);
    const int from = 3 * 48000;
    REQUIRE(std::abs(toneDb(out.left, 1000.0, from, n) - toneDb(music, 1000.0, from, n)) < 1.0);
    REQUIRE(toneDb(out.left, 3000.0, from, n) - toneDb(music, 3000.0, from, n) < -8.0);

    Engine quiet;
    prepare(quiet, s, spectralOrder(SpectralResolution::Medium, kRate));
    const Stereo silence{Signal(static_cast<std::size_t>(n)), Signal(static_cast<std::size_t>(n))};
    const auto untouched = run(quiet, s, {music, music}, &silence);
    for (double hz : {1000.0, 3000.0})
        REQUIRE(std::abs(toneDb(untouched.left, hz, from, n) - toneDb(music, hz, from, n)) < 0.5);
}

TEST_CASE("quail: stereo placement sends a band to left, right, mid or side")
{
    const int n = 24000;
    const auto tone = sine(1000.0, 0.25f, n);
    const auto gainOf = [&](Placement placement, float gainDb, bool mono = false)
    {
        Settings s;
        s.bands[9] = band(FilterShape::Bell, 1000.0f, gainDb, 1.0f);
        s.bands[9].placement = placement;
        Engine engine;
        prepare(engine, s);
        const auto out = run(engine, s, {tone, tone}, nullptr, 512, mono);
        return std::pair<double, double>{toneDb(out.left, 1000.0, n / 2, n) - toneDb(tone, 1000.0, n / 2, n),
                                         toneDb(out.right, 1000.0, n / 2, n) - toneDb(tone, 1000.0, n / 2, n)};
    };

    const auto left = gainOf(Placement::Left, 6.0f);
    REQUIRE(left.first == Catch::Approx(6.0).margin(0.1));
    REQUIRE(std::abs(left.second) < 0.01);
    const auto right = gainOf(Placement::Right, -6.0f);
    REQUIRE(std::abs(right.first) < 0.01);
    REQUIRE(right.second == Catch::Approx(-6.0).margin(0.1));
    // identical channels have no side signal
    const auto side = gainOf(Placement::Side, 12.0f);
    REQUIRE(std::abs(side.first) < 0.01);
    REQUIRE(std::abs(side.second) < 0.01);
    const auto mid = gainOf(Placement::Mid, -6.0f);
    REQUIRE(mid.first == Catch::Approx(-6.0).margin(0.1));
    REQUIRE(mid.second == Catch::Approx(-6.0).margin(0.1));
    // a mono input only has the stereo placement
    REQUIRE(std::abs(gainOf(Placement::Left, 6.0f, true).first) < 0.01);
    REQUIRE(gainOf(Placement::Stereo, 6.0f, true).first == Catch::Approx(6.0).margin(0.1));
}

TEST_CASE("quail: changing shape, slope or placement does not click")
{
    Settings s;
    s.bands[0] = band(FilterShape::Bell, 300.0f, 9.0f, 1.0f);
    const int n = 48000;
    const auto in = sine(300.0, 0.5f, n);
    Engine engine;
    prepare(engine, s);
    const auto out = run(engine, s, {in, in}, nullptr, 128, false,
                         [](int start, Settings& settings)
                         {
                             auto& b = settings.bands[0];
                             if (start == 9600)
                                 b.shape = FilterShape::LowShelf;
                             if (start == 19200)
                             {
                                 b.shape = FilterShape::HighCut;
                                 b.slope = 48;
                             }
                             if (start == 28800)
                                 b.placement = Placement::Side;
                             if (start == 38400)
                                 b.used = false;
                         });
    float worst = 0.0f;
    for (std::size_t i = 2; i < out.left.size(); ++i)
        worst = std::max(worst, std::abs(out.left[i] - 2.0f * out.left[i - 1] + out.left[i - 2]));
    REQUIRE(worst < 0.02f);
}

TEST_CASE("quail: solo plays only the band's frequency range")
{
    Settings s;
    s.bands[4] = band(FilterShape::Bell, 1000.0f, 0.0f, 2.0f);
    const int n = 24000;
    for (const auto& [hz, low, high] : {std::tuple<double, double, double>{1000.0, -1.0, 1.0},
                                        std::tuple<double, double, double>{100.0, -200.0, -15.0}})
    {
        Engine engine;
        prepare(engine, s);
        engine.setSoloBand(4);
        const auto in = sine(hz, 0.25f, n);
        const auto out = run(engine, s, {in, in});
        const double change = toneDb(out.left, hz, n / 2, n) - toneDb(in, hz, n / 2, n);
        INFO(hz << " Hz: " << change << " dB");
        REQUIRE(change > low);
        REQUIRE(change < high);
    }
}

TEST_CASE("quail: output level, balance and polarity")
{
    const int n = 19200;
    const auto in = noise(n, 0.25f, 9);
    const auto settle = 14400; // 20 time constants of the 15 ms output smoother
    const auto check = [&](const std::function<void(Settings&)>& change, float leftGain, float rightGain)
    {
        Settings s;
        change(s);
        Engine engine;
        prepare(engine, Settings{});
        const auto out = run(engine, s, {in, in});
        for (std::size_t i = static_cast<std::size_t>(settle); i < out.left.size(); ++i)
        {
            REQUIRE(out.left[i] == Catch::Approx(in[i] * leftGain).margin(1e-5));
            REQUIRE(out.right[i] == Catch::Approx(in[i] * rightGain).margin(1e-5));
        }
    };
    check([](Settings& s) { s.global.outputDb = 6.0f; }, 1.9952623f, 1.9952623f);
    check([](Settings& s) { s.global.pan = -1.0f; }, 1.0f, 0.0f);
    check([](Settings& s) { s.global.pan = 0.5f; }, 0.5f, 1.0f);
    check([](Settings& s) { s.global.phaseInvert = true; }, -1.0f, -1.0f);
}

TEST_CASE("quail: bypass crossfades to the input delayed by the latency")
{
    Settings s;
    s.bands[0] = band(FilterShape::Bell, 2000.0f, 6.0f, 0.5f);
    s.bands[0].rangeDb = -6.0f;
    s.bands[0].spectral = true;
    const int order = spectralOrder(SpectralResolution::Low, kRate);
    Engine engine;
    prepare(engine, s, order);
    const int latency = engine.getLatencySamples();
    REQUIRE(latency == 2048);

    const int n = 24000;
    const auto in = noise(n, 0.3f, 4);
    const auto out = run(engine, s, {in, in}, nullptr, 512, false,
                         [](int start, Settings& settings) { settings.global.bypass = start >= 8192; });
    for (int i = 8192 + 4800; i < n; ++i)
        REQUIRE(out.left[static_cast<std::size_t>(i)] ==
                Catch::Approx(in[static_cast<std::size_t>(i - latency)]).margin(1e-6));
}

TEST_CASE("quail: auto gain counters the loudness of a static boost")
{
    Settings s;
    s.bands[0] = band(FilterShape::Bell, 1000.0f, 12.0f, 0.4f);
    s.global.autoGain = true;
    Engine engine;
    prepare(engine, s);
    const auto in = noise(4096, 0.1f, 5);
    run(engine, s, {in, in});
    REQUIRE(engine.getAutoGainDb() < -3.0f);
    REQUIRE(engine.getAutoGainDb() == Catch::Approx(-estimateLoudnessChangeDb(s, kRate)).margin(0.01));

    s.global.autoGain = false;
    run(engine, s, {in, in});
    REQUIRE(engine.getAutoGainDb() == 0.0f);
    REQUIRE(estimateLoudnessChangeDb(Settings{}, kRate) == 0.0f);
}

TEST_CASE("quail: the dynamic gain curve")
{
    REQUIRE(dynamicGainDb(-3.0f, -12.0f) == 0.0f);
    REQUIRE(dynamicGainDb(0.0f, -12.0f) == 0.0f);
    REQUIRE(dynamicGainDb(18.0f, -12.0f) == Catch::Approx(-12.0f));
    REQUIRE(dynamicGainDb(40.0f, 6.0f) == Catch::Approx(6.0f));
    REQUIRE(dynamicGainDb(10.0f, 0.0f) == 0.0f);
    float previous = 0.0f;
    for (float over = 0.0f; over <= 20.0f; over += 0.25f)
    {
        const float g = dynamicGainDb(over, -12.0f);
        REQUIRE(g <= previous + 1e-6f);
        previous = g;
    }
}

TEST_CASE("quail: the spectral FFT size keeps its frequency resolution across sample rates")
{
    REQUIRE(spectralOrder(SpectralResolution::Low, 44100.0) == 11);
    REQUIRE(spectralOrder(SpectralResolution::Medium, 48000.0) == 12);
    REQUIRE(spectralOrder(SpectralResolution::High, 48000.0) == 13);
    REQUIRE(spectralOrder(SpectralResolution::Medium, 96000.0) == 13);
    REQUIRE(spectralOrder(SpectralResolution::High, 192000.0) == 15);
}
