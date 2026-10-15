#include <animalaudio/dsp/Smoothing.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using animalaudio::dsp::OnePoleSmoother;

TEST_CASE("OnePoleSmoother converges monotonically without overshoot")
{
    OnePoleSmoother s;
    s.prepare(48000.0, 5.0);
    s.reset(0.0f);
    s.setTarget(1.0f);

    float previous = 0.0f;
    for (int i = 0; i < 48000; ++i)
    {
        const float v = s.next();
        REQUIRE(v >= previous);
        REQUIRE(v <= 1.0f);
        previous = v;
    }
    REQUIRE(previous == 1.0f);
}

TEST_CASE("OnePoleSmoother reaches ~63% after one time constant")
{
    OnePoleSmoother s;
    s.prepare(48000.0, 10.0);
    s.reset(0.0f);
    s.setTarget(1.0f);
    float v = 0.0f;
    for (int i = 0; i < 480; ++i)
        v = s.next();
    REQUIRE(v == Catch::Approx(1.0 - std::exp(-1.0)).margin(0.01));
}

TEST_CASE("OnePoleSmoother with zero time snaps immediately")
{
    OnePoleSmoother s;
    s.prepare(48000.0, 0.0);
    s.reset(0.0f);
    s.setTarget(0.5f);
    REQUIRE(s.next() == 0.5f);
}
