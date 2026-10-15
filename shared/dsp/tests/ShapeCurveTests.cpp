#include <animalaudio/dsp/ShapeCurve.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace animalaudio::dsp;
using Catch::Approx;

TEST_CASE("Default curve is a linear ramp")
{
    ShapeCurve c;
    REQUIRE(c.evaluate(0.0f) == Approx(0.0f));
    REQUIRE(c.evaluate(0.5f) == Approx(0.5f));
    REQUIRE(c.evaluate(1.0f) == Approx(1.0f));
}

TEST_CASE("Positive bend eases in, negative bend eases out, endpoints fixed")
{
    ShapeCurve c;
    c.setBend(0, 0.5f);
    REQUIRE(c.evaluate(0.0f) == Approx(0.0f));
    REQUIRE(c.evaluate(1.0f) == Approx(1.0f));
    REQUIRE(c.evaluate(0.5f) < 0.5f);

    c.setBend(0, -0.5f);
    REQUIRE(c.evaluate(0.5f) > 0.5f);
}

TEST_CASE("Evaluation is monotonic for a rising two-node segment at any bend")
{
    for (float bend = -1.0f; bend <= 1.0f; bend += 0.25f)
    {
        ShapeCurve c;
        c.setBend(0, bend);
        float previous = -1.0f;
        for (int i = 0; i <= 200; ++i)
        {
            const float v = c.evaluate(static_cast<float>(i) / 200.0f);
            REQUIRE(v >= previous - 1.0e-6f);
            REQUIRE(v >= 0.0f);
            REQUIRE(v <= 1.0f);
            previous = v;
        }
    }
}

TEST_CASE("bendFromMidFraction inverts midFractionFromBend")
{
    for (float bend = -0.95f; bend <= 0.95f; bend += 0.05f)
    {
        const float fraction = ShapeCurve::midFractionFromBend(bend);
        REQUIRE(ShapeCurve::bendFromMidFraction(fraction) == Approx(bend).margin(1.0e-3f));
    }
}

TEST_CASE("Mid fraction of 0.5 means straight")
{
    REQUIRE(ShapeCurve::bendFromMidFraction(0.5f) == 0.0f);
}

TEST_CASE("normalise sanitises bad input")
{
    ShapeCurve c(std::vector<ShapeCurve::Node>{{0.7f, 2.0f, 5.0f}, {std::nanf(""), -1.0f, 0.0f}, {0.2f, 0.3f, 0.0f}});

    const auto& n = c.getNodes();
    REQUIRE(n.size() == 3);
    REQUIRE(n.front().x == 0.0f);
    REQUIRE(n.back().x == 1.0f);
    for (std::size_t i = 0; i < n.size(); ++i)
    {
        REQUIRE(n[i].y >= 0.0f);
        REQUIRE(n[i].y <= 1.0f);
        REQUIRE(n[i].bend >= -1.0f);
        REQUIRE(n[i].bend <= 1.0f);
        if (i > 0)
            REQUIRE(n[i].x - n[i - 1].x >= ShapeCurve::kMinNodeGap - 1.0e-6f);
    }
}

TEST_CASE("Fewer than two nodes falls back to the ramp")
{
    ShapeCurve c(std::vector<ShapeCurve::Node>{{0.5f, 0.5f, 0.0f}});
    REQUIRE(c.size() == 2);
}

TEST_CASE("Too many nodes are truncated")
{
    std::vector<ShapeCurve::Node> nodes;
    for (int i = 0; i < 40; ++i)
        nodes.push_back({static_cast<float>(i) / 39.0f, 0.5f, 0.0f});
    ShapeCurve c(nodes);
    REQUIRE(c.size() == ShapeCurve::kMaxNodes);
    REQUIRE(c.getNodes().back().x == 1.0f);
}

TEST_CASE("Insert, move and remove nodes")
{
    ShapeCurve c;
    const auto idx = c.insertNode(0.5f, 0.2f);
    REQUIRE(idx == 1);
    REQUIRE(c.size() == 3);
    REQUIRE(c.evaluate(0.5f) == Approx(0.2f));

    // can't cross neighbours
    c.moveNode(1, 5.0f, 0.9f);
    REQUIRE(c.getNodes()[1].x < 1.0f);
    REQUIRE(c.getNodes()[1].y == Approx(0.9f));

    // end nodes keep their x
    c.moveNode(0, 0.4f, 0.7f);
    REQUIRE(c.getNodes()[0].x == 0.0f);
    REQUIRE(c.getNodes()[0].y == Approx(0.7f));

    REQUIRE_FALSE(c.removeNode(0));
    REQUIRE_FALSE(c.removeNode(2));
    REQUIRE(c.removeNode(1));
    REQUIRE(c.size() == 2);
    REQUIRE_FALSE(c.removeNode(1));
}

TEST_CASE("Inserting too close to an existing node is rejected")
{
    ShapeCurve c;
    REQUIRE(c.insertNode(0.5f, 0.5f) != static_cast<std::size_t>(-1));
    REQUIRE(c.insertNode(0.501f, 0.5f) == static_cast<std::size_t>(-1));
    REQUIRE(c.insertNode(0.0005f, 0.5f) == static_cast<std::size_t>(-1));
}

TEST_CASE("reversed mirrors the curve in time")
{
    ShapeCurve c(std::vector<ShapeCurve::Node>{{0.0f, 0.0f, 0.6f}, {0.3f, 0.8f, -0.4f}, {1.0f, 0.2f, 0.0f}});
    const ShapeCurve r = c.reversed();
    for (int i = 0; i <= 100; ++i)
    {
        const float x = static_cast<float>(i) / 100.0f;
        REQUIRE(r.evaluate(x) == Approx(c.evaluate(1.0f - x)).margin(2.0e-3f));
    }
}

TEST_CASE("inverted flips the curve vertically")
{
    ShapeCurve c(std::vector<ShapeCurve::Node>{{0.0f, 0.1f, 0.3f}, {0.6f, 0.9f, -0.5f}, {1.0f, 0.4f, 0.0f}});
    const ShapeCurve v = c.inverted();
    for (int i = 0; i <= 100; ++i)
    {
        const float x = static_cast<float>(i) / 100.0f;
        REQUIRE(v.evaluate(x) == Approx(1.0f - c.evaluate(x)).margin(1.0e-5f));
    }
}

TEST_CASE("CurveTable matches direct evaluation, including both ends")
{
    ShapeCurve c(std::vector<ShapeCurve::Node>{{0.0f, 0.0f, 0.5f}, {0.5f, 1.0f, -0.5f}, {1.0f, 0.3f, 0.0f}});
    CurveTable table;
    table.fill(c);

    REQUIRE(table.values.front() == Approx(c.evaluate(0.0f)));
    REQUIRE(table.values.back() == Approx(c.evaluate(1.0f)));
    for (int i = 0; i < 1000; ++i)
    {
        const double phase = static_cast<double>(i) / 1000.0;
        REQUIRE(table.lookup(phase) == Approx(c.evaluate(static_cast<float>(phase))).margin(2.0e-3f));
    }
    // phase right at the end of the cycle never reads out of bounds
    REQUIRE(table.lookup(0.9999999999) == Approx(c.evaluate(1.0f)).margin(2.0e-3f));
}
