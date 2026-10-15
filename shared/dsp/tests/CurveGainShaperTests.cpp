#include <animalaudio/dsp/CurveGainShaper.h>
#include <animalaudio/dsp/TempoSync.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstring>
#include <vector>

using namespace animalaudio::dsp;
using Catch::Approx;

namespace
{
CurveTable makeTable(const ShapeCurve& c)
{
    CurveTable t;
    t.fill(c);
    return t;
}

// Classic duck: starts at 0, rises to 1.
ShapeCurve duckCurve()
{
    return ShapeCurve(std::vector<ShapeCurve::Node>{{0.0f, 0.0f, -0.55f}, {1.0f, 1.0f, 0.0f}});
}

// Hard on/off: gain 0 for the first half of the cycle, 1 afterwards (worst case for clicks).
ShapeCurve gateCurve()
{
    return ShapeCurve(
        std::vector<ShapeCurve::Node>{{0.0f, 0.0f, 0.0f}, {0.49f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}});
}

struct Render
{
    std::vector<float> out;
};

Render renderConstant(CurveGainShaper& engine, int numSamples, double startPhase, double inc, float input = 1.0f)
{
    Render r;
    r.out.assign(static_cast<std::size_t>(numSamples), input);
    float* ch[1] = {r.out.data()};
    engine.process(ch, 1, numSamples, startPhase, inc);
    return r;
}
} // namespace

TEST_CASE("Mix 0 is a bit-exact bypass")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(duckCurve()));
    engine.reset(0.0f);
    engine.setMix(0.0f);

    const auto r = renderConstant(engine, 4800, 0.0, 1.0 / 24000.0, 0.7f);
    const float expected = 0.7f;
    for (float v : r.out)
        REQUIRE(std::memcmp(&v, &expected, sizeof(float)) == 0);
}

TEST_CASE("Full mix drives gain to the curve minimum, and never exceeds unity")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(duckCurve()));
    engine.reset(1.0f);
    engine.setMix(1.0f);

    const auto r = renderConstant(engine, 48000, 0.0, 1.0 / 24000.0);
    float minimum = 10.0f, maximum = -10.0f;
    for (float v : r.out)
    {
        minimum = std::min(minimum, v);
        maximum = std::max(maximum, v);
    }
    REQUIRE(maximum <= 1.0f + 1.0e-6f);
    REQUIRE(minimum >= 0.0f);
    REQUIRE(minimum < 0.05f);
    REQUIRE(maximum > 0.95f);
}

TEST_CASE("Half mix limits the depth of the duck")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(duckCurve()));
    engine.reset(0.5f);
    engine.setMix(0.5f);

    const auto r = renderConstant(engine, 48000, 0.0, 1.0 / 24000.0);
    const float minimum = *std::min_element(r.out.begin(), r.out.end());
    REQUIRE(minimum >= 0.5f - 1.0e-4f);
    REQUIRE(minimum < 0.56f);
}

TEST_CASE("Gain changes are click-free even for a hard gate curve")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(gateCurve()));
    engine.reset(1.0f);
    engine.setMix(1.0f);

    const auto r = renderConstant(engine, 96000, 0.0, 1.0 / 24000.0);

    // Without smoothing the step would be 1.0 in a single sample. With a 0.7 ms one-pole the
    // per-sample slew is bounded by (1 - exp(-1/tau)) ~ 3%.
    float largestStep = 0.0f;
    for (std::size_t i = 1; i < r.out.size(); ++i)
        largestStep = std::max(largestStep, std::abs(r.out[i] - r.out[i - 1]));
    REQUIRE(largestStep < 0.05f);
}

TEST_CASE("Ducking is periodic at the synced rate: minima every beat at 120 BPM / 48 kHz")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(duckCurve()));
    engine.reset(1.0f);
    engine.setMix(1.0f);

    const double inc = phaseIncrementPerSample(120.0, 48000.0, Division::Quarter);
    const auto r = renderConstant(engine, 24000 * 4, 0.0, inc);

    // Gain is near zero just after each beat (once the ~0.7 ms declick has settled) and ~unity right before the next.
    for (int beat = 1; beat < 4; ++beat)
    {
        REQUIRE(r.out[static_cast<std::size_t>(beat * 24000 + 300)] < 0.1f);
        REQUIRE(r.out[static_cast<std::size_t>(beat * 24000 - 20)] > 0.9f);
    }
}

TEST_CASE("Different divisions change the ducking rate")
{
    auto countDucks = [](Division d)
    {
        CurveGainShaper engine;
        engine.prepare(48000.0);
        engine.setTable(makeTable(duckCurve()));
        engine.reset(1.0f);
        engine.setMix(1.0f);
        const double inc = phaseIncrementPerSample(120.0, 48000.0, d);
        const auto r = renderConstant(engine, 96000, 0.0, inc); // 2 seconds = 4 beats
        int ducks = 0;
        bool low = false;
        for (float v : r.out)
        {
            if (!low && v < 0.1f)
            {
                low = true;
                ++ducks;
            }
            if (low && v > 0.9f)
                low = false;
        }
        return ducks;
    };

    REQUIRE(countDucks(Division::Whole) == 1);
    REQUIRE(countDucks(Division::Half) == 2);
    REQUIRE(countDucks(Division::Quarter) == 4);
    REQUIRE(countDucks(Division::Eighth) == 8);
}

TEST_CASE("Block-size independence")
{
    auto render = [](int blockSize)
    {
        CurveGainShaper engine;
        engine.prepare(44100.0);
        engine.setTable(makeTable(duckCurve()));
        engine.reset(0.8f);
        engine.setMix(0.8f);

        PhaseTracker tracker;
        tracker.prepare(44100.0);
        std::vector<float> all;
        TransportInfo t;
        for (int done = 0; done < 22050; done += blockSize)
        {
            std::vector<float> buf(static_cast<std::size_t>(blockSize), 1.0f);
            const auto b = tracker.beginBlock(t, Division::Eighth, 133.0, blockSize);
            float* ch[1] = {buf.data()};
            engine.process(ch, 1, blockSize, b.startPhase, b.increment);
            all.insert(all.end(), buf.begin(), buf.end());
        }
        return all;
    };

    const auto a = render(64);
    const auto b = render(441);
    for (std::size_t i = 0; i < 22050; ++i)
        REQUIRE(a[i] == Approx(b[i]).margin(1.0e-4f));
}

TEST_CASE("Stereo channels receive identical gain")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(duckCurve()));
    engine.reset(1.0f);
    engine.setMix(1.0f);

    std::vector<float> l(2000, 1.0f), r(2000, 0.5f);
    float* ch[2] = {l.data(), r.data()};
    engine.process(ch, 2, 2000, 0.0, 1.0 / 24000.0);
    for (std::size_t i = 0; i < l.size(); ++i)
        REQUIRE(r[i] == Approx(l[i] * 0.5f).margin(1.0e-6f));
}

TEST_CASE("Engine publishes its playhead phase and scope")
{
    CurveGainShaper engine;
    engine.prepare(48000.0);
    engine.setTable(makeTable(duckCurve()));
    engine.reset(1.0f);
    engine.setMix(1.0f);

    renderConstant(engine, 12000, 0.0, 1.0 / 24000.0);
    REQUIRE(engine.getDisplayPhase() == Approx(0.5f).margin(1.0e-3f));
    REQUIRE(engine.getScopeBin(CurveGainShaper::kScopeBins / 4) > 0.0f);
}
