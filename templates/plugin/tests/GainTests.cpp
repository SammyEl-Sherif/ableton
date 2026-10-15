#include "dsp/Gain.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

TEST_CASE("decibelsToLinear")
{
    REQUIRE(__NAME_LOWER__::decibelsToLinear(0.0f) == Catch::Approx(1.0f));
    REQUIRE(__NAME_LOWER__::decibelsToLinear(-6.0206f) == Catch::Approx(0.5f).margin(1.0e-3f));
    REQUIRE(__NAME_LOWER__::decibelsToLinear(-100.0f) == 0.0f);
}

TEST_CASE("Gain settles at the requested level")
{
    __NAME_LOWER__::Gain gain;
    gain.prepare(48000.0);
    gain.setDecibels(-6.0206f);

    std::vector<float> buffer(48000, 1.0f);
    float* ch[1] = {buffer.data()};
    gain.process(ch, 1, static_cast<int>(buffer.size()));
    REQUIRE(buffer.back() == Catch::Approx(0.5f).margin(1.0e-3f));
}
