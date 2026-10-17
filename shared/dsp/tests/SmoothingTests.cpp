#include <animalaudio/dsp/Smoothing.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <utility>

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

TEST_CASE("OnePoleSmoother reports when it has settled exactly on the target")
{
    OnePoleSmoother s;
    s.prepare(48000.0, 2.0);
    s.reset(0.25f);
    REQUIRE(s.isSettled());
    s.setTarget(0.75f);
    REQUIRE_FALSE(s.isSettled());
    for (int i = 0; i < 48000 && !s.isSettled(); ++i)
        s.next();
    REQUIRE(s.isSettled());
    REQUIRE(s.getCurrent() == 0.75f);
}

TEST_CASE("OnePoleSmoother settles exactly even where float rounding stalls the recurrence")
{
    for (double rate : {44100.0, 48000.0, 96000.0, 192000.0})
        for (double ms : {15.0, 20.0, 100.0})
            for (auto [from, to] : {std::pair{1.0f, 1.9952623f}, std::pair{1.9952623f, 1.0f}, std::pair{0.0f, 30.0f},
                                    std::pair{6.9f, 9.9f}, std::pair{-12.0f, -11.5f}})
            {
                INFO(rate << " Hz, " << ms << " ms, " << from << " -> " << to);
                OnePoleSmoother s;
                s.prepare(rate, ms);
                s.reset(from);
                s.setTarget(to);
                const int limit = static_cast<int>(rate * ms * 0.001 * 40.0);
                for (int i = 0; i < limit && !s.isSettled(); ++i)
                    s.next();
                REQUIRE(s.isSettled());
                REQUIRE(s.getCurrent() == to);
            }
}

TEST_CASE("OnePoleSmoother with zero time snaps immediately")
{
    OnePoleSmoother s;
    s.prepare(48000.0, 0.0);
    s.reset(0.0f);
    s.setTarget(0.5f);
    REQUIRE(s.next() == 0.5f);
}
