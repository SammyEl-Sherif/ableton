#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace animalaudio::dsp
{
/**
 * Piecewise curve over x in [0,1] with y in [0,1], built from draggable nodes.
 * Each node carries a `bend` for the segment that follows it:
 *   bend = 0  -> straight line
 *   bend > 0  -> ease-in  (slow start, fast end)
 *   bend < 0  -> ease-out (fast start, slow end)
 * Two segments with opposite bends make a smooth S-curve.
 *
 * This type is for the message thread / offline use. Audio code reads a baked `CurveTable`.
 */
class ShapeCurve
{
public:
    struct Node
    {
        float x = 0.0f;
        float y = 0.0f;
        float bend = 0.0f;
    };

    static constexpr std::size_t kMaxNodes = 16;
    static constexpr float kMinNodeGap = 0.004f;
    static constexpr float kMaxBendExponentOctaves = 3.0f; // exponent range 2^-3 .. 2^3

    ShapeCurve() { resetToRamp(); }
    explicit ShapeCurve(std::vector<Node> n)
        : nodes(std::move(n))
    {
        normalise();
    }

    const std::vector<Node>& getNodes() const noexcept { return nodes; }
    std::size_t size() const noexcept { return nodes.size(); }

    void resetToRamp() { nodes = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f}}; }

    /** Sorts, clamps, forces first/last node to x=0/x=1, enforces gaps and max node count. */
    void normalise()
    {
        for (auto& n : nodes)
        {
            n.x = sanitise(n.x, 0.0f, 1.0f, 0.0f);
            n.y = sanitise(n.y, 0.0f, 1.0f, 0.0f);
            n.bend = sanitise(n.bend, -1.0f, 1.0f, 0.0f);
        }

        std::stable_sort(nodes.begin(), nodes.end(), [](const Node& a, const Node& b) { return a.x < b.x; });

        if (nodes.size() > kMaxNodes)
            nodes.resize(kMaxNodes);

        if (nodes.size() < 2)
        {
            resetToRamp();
            return;
        }

        nodes.front().x = 0.0f;
        nodes.back().x = 1.0f;

        // Forward pass pushes nodes apart, backward pass pulls the tail back inside [0,1].
        for (std::size_t i = 1; i < nodes.size(); ++i)
            nodes[i].x = std::max(nodes[i].x, nodes[i - 1].x + kMinNodeGap);
        nodes.back().x = 1.0f;
        for (std::size_t i = nodes.size() - 1; i-- > 0;)
            nodes[i].x = std::min(nodes[i].x, nodes[i + 1].x - kMinNodeGap);
        nodes.front().x = 0.0f;
    }

    float evaluate(float x) const noexcept
    {
        if (nodes.size() < 2)
            return 1.0f;

        x = std::min(std::max(x, 0.0f), 1.0f);

        std::size_t i = 0;
        while (i + 2 < nodes.size() && x > nodes[i + 1].x)
            ++i;

        const Node& a = nodes[i];
        const Node& b = nodes[i + 1];
        const float span = b.x - a.x;
        const float t = span > 0.0f ? (x - a.x) / span : 1.0f;
        return a.y + (b.y - a.y) * shapeSegment(t, a.bend);
    }

    /** Writes `count` (>= 2) samples spanning x = 0 .. 1 inclusive. */
    void bake(float* destination, std::size_t count) const noexcept
    {
        for (std::size_t i = 0; i < count; ++i)
            destination[i] = evaluate(static_cast<float>(i) / static_cast<float>(count - 1));
    }

    // ---- editing helpers (message thread) ----

    /** Moves node `index`, keeping ordering. End nodes only move vertically. Returns the final index. */
    std::size_t moveNode(std::size_t index, float x, float y)
    {
        if (index >= nodes.size())
            return index;

        y = std::min(std::max(y, 0.0f), 1.0f);
        nodes[index].y = y;

        if (index == 0 || index + 1 == nodes.size())
            return index;

        const float lo = nodes[index - 1].x + kMinNodeGap;
        const float hi = nodes[index + 1].x - kMinNodeGap;
        nodes[index].x = std::min(std::max(x, lo), hi);
        return index;
    }

    /** Inserts a node at x (y taken from `y`). Returns its index, or npos if full / too close. */
    std::size_t insertNode(float x, float y)
    {
        constexpr std::size_t npos = static_cast<std::size_t>(-1);
        if (nodes.size() >= kMaxNodes || x <= kMinNodeGap || x >= 1.0f - kMinNodeGap)
            return npos;

        std::size_t i = 0;
        while (i + 1 < nodes.size() && nodes[i + 1].x < x)
            ++i;

        if (x - nodes[i].x < kMinNodeGap || nodes[i + 1].x - x < kMinNodeGap)
            return npos;

        // Split the segment: keep its overall look by preserving the bend on both halves.
        Node n{x, std::min(std::max(y, 0.0f), 1.0f), nodes[i].bend};
        nodes.insert(nodes.begin() + static_cast<std::ptrdiff_t>(i) + 1, n);
        return i + 1;
    }

    bool removeNode(std::size_t index)
    {
        if (index == 0 || index + 1 >= nodes.size() || nodes.size() <= 2)
            return false;
        nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    void setBend(std::size_t index, float bend)
    {
        if (index + 1 < nodes.size())
            nodes[index].bend = std::min(std::max(bend, -1.0f), 1.0f);
    }

    /** Mirrors the curve in time. */
    ShapeCurve reversed() const
    {
        ShapeCurve out;
        out.nodes.clear();
        const auto n = nodes.size();
        for (std::size_t i = 0; i < n; ++i)
        {
            const Node& src = nodes[n - 1 - i];
            out.nodes.push_back({1.0f - src.x, src.y, 0.0f});
        }
        // Segment (i -> i+1) of the mirror is segment (n-2-i -> n-1-i) of the source, traversed backwards.
        for (std::size_t i = 0; i + 1 < n; ++i)
            out.nodes[i].bend = -nodes[n - 2 - i].bend;
        out.nodes.back().bend = 0.0f;
        out.normalise();
        return out;
    }

    /** Flips the curve vertically (loud <-> quiet). */
    ShapeCurve inverted() const
    {
        ShapeCurve out = *this;
        for (auto& n : out.nodes)
            n.y = 1.0f - n.y;
        return out;
    }

    // ---- segment shaping maths ----

    /** Maps t in [0,1] through the bend (0 = linear). Endpoints are fixed at 0 and 1. */
    static float shapeSegment(float t, float bend) noexcept
    {
        if (bend == 0.0f)
            return t;
        const float p = std::exp2(std::abs(bend) * kMaxBendExponentOctaves);
        return bend > 0.0f ? std::pow(t, p) : 1.0f - std::pow(1.0f - t, p);
    }

    /** Fraction (0..1) of the way between the two nodes' y values that the segment reaches at t=0.5. */
    static float midFractionFromBend(float bend) noexcept { return shapeSegment(0.5f, bend); }

    /** Inverse of midFractionFromBend; used by the editor's bend handle. */
    static float bendFromMidFraction(float fraction) noexcept
    {
        fraction = std::min(std::max(fraction, 0.004f), 0.996f);
        if (std::abs(fraction - 0.5f) < 1.0e-4f)
            return 0.0f;
        if (fraction < 0.5f)
        {
            const float p = std::log(fraction) / std::log(0.5f); // fraction = 0.5^p
            return std::min(std::log2(p) / kMaxBendExponentOctaves, 1.0f);
        }
        const float p = std::log(1.0f - fraction) / std::log(0.5f);
        return -std::min(std::log2(p) / kMaxBendExponentOctaves, 1.0f);
    }

private:
    static float sanitise(float v, float lo, float hi, float fallback) noexcept
    {
        if (!std::isfinite(v))
            return fallback;
        return std::min(std::max(v, lo), hi);
    }

    std::vector<Node> nodes;
};

/** Immutable-by-convention lookup table of a baked curve, safe to read on the audio thread. */
struct CurveTable
{
    static constexpr std::size_t kSize = 1024;

    std::array<float, kSize + 1> values{}; // x = i / kSize, inclusive of both ends

    void fill(const ShapeCurve& curve) noexcept { curve.bake(values.data(), kSize + 1); }

    void fillConstant(float v) noexcept { values.fill(v); }

    /** Linear-interpolated lookup, phase in [0,1). */
    float lookup(double phase) const noexcept
    {
        const double scaled = phase * static_cast<double>(kSize);
        auto index = static_cast<std::size_t>(scaled);
        if (index >= kSize)
            index = kSize - 1;
        const float frac = static_cast<float>(scaled - static_cast<double>(index));
        return values[index] + (values[index + 1] - values[index]) * frac;
    }
};
} // namespace animalaudio::dsp
