#include <animalaudio/core/CurveState.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace animalaudio::dsp;
using namespace animalaudio::core;
using Catch::Approx;

namespace
{
using N = ShapeCurve::Node;

const CurvePresetList& testPresets()
{
    static const CurvePresetList presets = {
        {"Ramp", {N{0.0f, 0.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}}},
        {"A", {N{0.0f, 1.0f, 0.0f}, N{1.0f, 0.5f, 0.0f}}},
        {"B", {N{0.0f, 0.2f, 0.5f}, N{1.0f, 1.0f, 0.0f}}},
        {"S", {N{0.0f, 0.0f, 0.5f}, N{0.5f, 0.5f, -0.5f}, N{1.0f, 1.0f, 0.0f}}},
        {"Dive", {N{0.0f, 1.0f, 0.0f}, N{1.0f, 0.0f, 0.0f}}},
        {"Dive2", {N{0.0f, 1.0f, 0.0f}, N{1.0f, 0.0f, 0.0f}}},
    };
    return presets;
}
} // namespace

TEST_CASE("Curve round-trips through a ValueTree and XML")
{
    ShapeCurve original(std::vector<ShapeCurve::Node>{{0.0f, 0.1f, 0.4f}, {0.35f, 0.9f, -0.6f}, {1.0f, 0.25f, 0.0f}});

    const auto xml = curveToTree(original, 7).createXml();
    REQUIRE(xml != nullptr);
    const auto restored = curveFromTree(juce::ValueTree::fromXml(*xml), ShapeCurve());

    REQUIRE(restored.size() == original.size());
    for (std::size_t i = 0; i < original.size(); ++i)
    {
        REQUIRE(restored.getNodes()[i].x == Approx(original.getNodes()[i].x));
        REQUIRE(restored.getNodes()[i].y == Approx(original.getNodes()[i].y));
        REQUIRE(restored.getNodes()[i].bend == Approx(original.getNodes()[i].bend));
    }
}

TEST_CASE("A garbage Curve tree yields the default preset instead of crashing")
{
    juce::ValueTree bad(tree::curve);
    bad.appendChild(juce::ValueTree(tree::node), nullptr);
    const auto c = curveFromTree(bad, ShapeCurve());
    REQUIRE(c.size() >= 2);
}

TEST_CASE("CurveState stores into the parent tree and publishes tables")
{
    juce::ValueTree root("State");
    CurveState state(root, testPresets());

    REQUIRE(root.getChildWithName(tree::curve).isValid());
    REQUIRE(state.getPresetIndex() == 0);
    REQUIRE(state.getPresets().size() == testPresets().size());

    CurveTable table;
    std::uint64_t version = 0;
    REQUIRE(state.fetchTable(table, version));
    REQUIRE_FALSE(state.fetchTable(table, version)); // nothing new

    state.loadPreset(4); // "Dive": 1 -> 0
    REQUIRE(state.getPresetIndex() == 4);
    REQUIRE(state.fetchTable(table, version));
    REQUIRE(table.values.front() == Approx(1.0f));
    REQUIRE(table.values.back() == Approx(0.0f));

    ShapeCurve custom(std::vector<ShapeCurve::Node>{{0.0f, 0.5f, 0.0f}, {0.5f, 0.0f, 0.0f}, {1.0f, 0.5f, 0.0f}});
    state.setCurve(custom, -1);
    REQUIRE(state.getPresetIndex() == -1);
    REQUIRE(state.fetchTable(table, version));
    REQUIRE(table.lookup(0.5) == Approx(0.0f).margin(1.0e-3f));

    // same node count -> updated in place; fewer/more nodes -> rebuilt
    ShapeCurve ramp;
    state.setCurve(ramp, -1);
    REQUIRE(state.getCurve().size() == 2);
}

TEST_CASE("CurveState follows the parent handle when the whole state is replaced")
{
    juce::ValueTree root("State");
    CurveState state(root, testPresets());

    auto incoming = juce::ValueTree("State");
    incoming.appendChild(curveToTree(testPresets()[3].makeCurve(), 3), nullptr);
    root = incoming; // what APVTS::replaceState does

    REQUIRE(state.getPresetIndex() == 3);
    CurveTable table;
    std::uint64_t version = 0;
    REQUIRE(state.fetchTable(table, version));
    REQUIRE(table.lookup(0.5) == Approx(testPresets()[3].makeCurve().evaluate(0.5f)).margin(2.0e-3f));
}
