#include <animalaudio/dsp/StandardPresets.h>

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

using namespace animalaudio::dsp;

namespace
{
const std::array<const char*, kNumCurvePresets> kNames = {"a", "b", "c", "d", "e", "f", "g", "h",
                                                          "i", "j", "k", "l", "m", "n", "o", "p"};
}

TEST_CASE("Standard presets: 16 shapes, named by the plugin, all within the curve constraints")
{
    const auto presets = standardCurvePresets(kNames);
    REQUIRE(static_cast<int>(presets.size()) == kNumCurvePresets);

    std::set<std::string> names;
    for (std::size_t i = 0; i < presets.size(); ++i)
    {
        REQUIRE(std::string(presets[i].name) == kNames[i]);
        REQUIRE(names.insert(presets[i].name).second);

        const auto curve = presets[i].makeCurve();
        REQUIRE(curve.size() == presets[i].nodes.size());
        REQUIRE(curve.getNodes().front().x == 0.0f);
        REQUIRE(curve.getNodes().back().x == 1.0f);
        for (int step = 0; step <= 100; ++step)
        {
            const float v = curve.evaluate(static_cast<float>(step) / 100.0f);
            REQUIRE(v >= 0.0f);
            REQUIRE(v <= 1.0f);
        }
    }
}

TEST_CASE("Standard presets: the first one is the classic ramp-up duck shape")
{
    const auto presets = standardCurvePresets(kNames);
    const auto curve = presets.front().makeCurve();
    REQUIRE(curve.evaluate(0.0f) == 0.0f);
    REQUIRE(curve.evaluate(1.0f) == 1.0f);
}
