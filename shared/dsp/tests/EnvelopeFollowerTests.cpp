#include <animalaudio/dsp/EnvelopeFollower.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using animalaudio::dsp::EnvelopeFollower;
using Catch::Approx;

TEST_CASE("EnvelopeFollower attacks and releases with its time constants")
{
    EnvelopeFollower env;
    env.prepare(48000.0);
    env.setTimes(10.0, 100.0);

    for (int i = 0; i < 480; ++i)
        env.process(1.0f);
    REQUIRE(env.getEnvelope() == Approx(1.0 - 0.36788).margin(1.0e-3));

    for (int i = 0; i < 48000; ++i)
        env.process(1.0f);
    REQUIRE(env.getEnvelope() == Approx(1.0f).margin(1.0e-4));

    for (int i = 0; i < 4800; ++i)
        env.process(0.0f);
    REQUIRE(env.getEnvelope() == Approx(0.36788).margin(1.0e-3));
}

TEST_CASE("EnvelopeFollower with zero times follows the input exactly")
{
    EnvelopeFollower env;
    env.prepare(44100.0);
    env.setTimes(0.0, 0.0);
    REQUIRE(env.process(0.7f) == 0.7f);
    REQUIRE(env.process(0.2f) == 0.2f);
    REQUIRE(EnvelopeFollower::coefficient(1.0, 1000.0) == Approx(0.36788f).margin(1.0e-4));
}
