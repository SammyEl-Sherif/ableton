#include "dsp/DuckPresets.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

TEST_CASE("There are 16 valid, uniquely named presets")
{
    const auto& presets = duck::getPresets();
    REQUIRE(static_cast<int>(presets.size()) == duck::kNumPresets);

    std::set<std::string> names;
    for (const auto& p : presets)
    {
        REQUIRE(names.insert(p.name).second);
        const auto curve = p.makeCurve();
        REQUIRE(curve.size() >= 2);
        REQUIRE(curve.getNodes().front().x == 0.0f);
        REQUIRE(curve.getNodes().back().x == 1.0f);
        for (int i = 0; i <= 100; ++i)
        {
            const float v = curve.evaluate(static_cast<float>(i) / 100.0f);
            REQUIRE(v >= 0.0f);
            REQUIRE(v <= 1.0f);
        }
    }
}

TEST_CASE("Presets survive normalise unchanged (authored within constraints)")
{
    for (const auto& p : duck::getPresets())
    {
        const auto curve = p.makeCurve();
        REQUIRE(curve.size() == p.nodes.size());
        for (std::size_t i = 0; i < p.nodes.size(); ++i)
        {
            REQUIRE(curve.getNodes()[i].x == p.nodes[i].x);
            REQUIRE(curve.getNodes()[i].y == p.nodes[i].y);
        }
    }
}
