#include "OwlMotif.h"

#include <cmath>
#include <cstdint>

namespace owl::ui
{
namespace
{
using animalaudio::ui::Palette;

std::uint32_t hash(std::uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

float unitHash(std::uint32_t x)
{
    return static_cast<float>(hash(x) & 0xffffu) / 65535.0f;
}

/** Crescent moon inside `area`, open to the right. */
juce::Path makeCrescent(juce::Rectangle<float> area)
{
    const float r = area.getWidth() * 0.5f;
    const auto c = area.getCentre();
    const float d = 0.2f * r;
    const float innerR = std::sqrt((0.5f * r - d) * (0.5f * r - d) + 0.75f * r * r);
    const float tipAngle = juce::MathConstants<float>::pi / 3.0f;
    const float innerTip = std::atan2(0.866f * r, 0.5f * r - d);

    juce::Path p;
    constexpr int steps = 32;
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / steps;
        const float a = tipAngle + t * (juce::MathConstants<float>::twoPi - 2.0f * tipAngle);
        const juce::Point<float> pt(c.x + r * std::cos(a), c.y + r * std::sin(a));
        if (i == 0)
            p.startNewSubPath(pt);
        else
            p.lineTo(pt);
    }
    for (int i = 0; i <= steps; ++i)
    {
        const float t = static_cast<float>(i) / steps;
        const float a = -innerTip - t * (juce::MathConstants<float>::twoPi - 2.0f * innerTip);
        p.lineTo(c.x + d + innerR * std::cos(a), c.y + innerR * std::sin(a));
    }
    p.closeSubPath();
    return p;
}

void drawBarredFeather(juce::Graphics& g, juce::Rectangle<float> area, float rotation, juce::Colour colour)
{
    juce::Path vane;
    vane.startNewSubPath(0.5f, 0.0f);
    vane.cubicTo(0.86f, 0.2f, 0.8f, 0.76f, 0.5f, 1.0f);
    vane.cubicTo(0.2f, 0.76f, 0.14f, 0.2f, 0.5f, 0.0f);
    vane.closeSubPath();

    const auto map = juce::AffineTransform::scale(area.getWidth(), area.getHeight())
                         .translated(area.getX(), area.getY())
                         .rotated(rotation, area.getCentreX(), area.getCentreY());
    g.setColour(colour);
    g.fillPath(vane, map);

    juce::Graphics::ScopedSaveState state(g);
    g.reduceClipRegion(vane, map);
    juce::Path bars;
    for (int k = 1; k <= 5; ++k)
    {
        const float y = static_cast<float>(k) / 6.0f;
        bars.startNewSubPath(0.1f, y + 0.05f);
        bars.lineTo(0.9f, y - 0.05f);
    }
    g.setColour(colour.withMultipliedAlpha(1.6f).darker(0.4f));
    g.strokePath(bars, juce::PathStrokeType(0.05f), map);

    juce::Path quill;
    quill.startNewSubPath(0.5f, 0.02f);
    quill.lineTo(0.5f, 1.0f);
    g.strokePath(quill, juce::PathStrokeType(0.025f), map);
}

void fillUnitEllipse(juce::Graphics& g, float cx, float cy, float rx, float ry, const juce::AffineTransform& map)
{
    juce::Path p;
    p.addEllipse(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f);
    g.fillPath(p, map);
}

void strokeChevrons(juce::Graphics& g, const std::initializer_list<juce::Point<float>>& centres, float halfWidth,
                    float strokeWidth, const juce::AffineTransform& map)
{
    juce::Path v;
    for (auto c : centres)
    {
        v.startNewSubPath(c.x - halfWidth, c.y - halfWidth * 0.5f);
        v.lineTo(c.x, c.y + halfWidth * 0.5f);
        v.lineTo(c.x + halfWidth, c.y - halfWidth * 0.5f);
    }
    g.strokePath(v, juce::PathStrokeType(strokeWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                 map);
}

void drawSoftShadow(juce::Graphics& g, juce::Point<float> centre, float radius)
{
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(centre.x - radius, centre.y - radius + radius * 0.07f, radius * 2.0f, radius * 2.0f);
}
} // namespace

void OwlMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette) const
{
    const auto map =
        juce::AffineTransform::scale(area.getWidth(), area.getHeight()).translated(area.getX(), area.getY());
    const auto at = [&](float x, float y) { return juce::Point<float>(x, y).transformedBy(map); };

    juce::Path branch;
    branch.addRoundedRectangle(0.02f, 0.9f, 0.96f, 0.07f, 0.035f);
    g.setColour(palette.outline.brighter(0.35f));
    g.fillPath(branch, map);

    juce::Path tufts;
    tufts.startNewSubPath(0.25f, 0.3f);
    tufts.lineTo(0.17f, 0.03f);
    tufts.lineTo(0.43f, 0.2f);
    tufts.closeSubPath();
    tufts.startNewSubPath(0.75f, 0.3f);
    tufts.lineTo(0.83f, 0.03f);
    tufts.lineTo(0.57f, 0.2f);
    tufts.closeSubPath();

    juce::Path body;
    body.addEllipse(0.17f, 0.12f, 0.66f, 0.8f);

    g.setGradientFill(juce::ColourGradient(palette.accent.brighter(0.15f), at(0.5f, 0.1f), palette.accent.darker(0.4f),
                                           at(0.5f, 0.92f), false));
    g.fillPath(tufts, map);
    g.fillPath(body, map);

    juce::Path wings;
    wings.startNewSubPath(0.21f, 0.46f);
    wings.quadraticTo(0.08f, 0.7f, 0.27f, 0.88f);
    wings.quadraticTo(0.31f, 0.64f, 0.21f, 0.46f);
    wings.closeSubPath();
    wings.startNewSubPath(0.79f, 0.46f);
    wings.quadraticTo(0.92f, 0.7f, 0.73f, 0.88f);
    wings.quadraticTo(0.69f, 0.64f, 0.79f, 0.46f);
    wings.closeSubPath();
    g.setColour(palette.accent.darker(0.6f).withAlpha(0.85f));
    g.fillPath(wings, map);

    g.setColour(palette.accent.brighter(0.6f).withAlpha(0.5f));
    fillUnitEllipse(g, 0.5f, 0.69f, 0.18f, 0.19f, map);
    g.setColour(palette.accent.darker(0.35f).withAlpha(0.75f));
    strokeChevrons(g, {{0.42f, 0.6f}, {0.58f, 0.6f}, {0.5f, 0.69f}, {0.42f, 0.78f}, {0.58f, 0.78f}}, 0.035f, 0.022f,
                   map);

    juce::Path disc;
    disc.addEllipse(0.22f, 0.2f, 0.3f, 0.3f);
    disc.addEllipse(0.48f, 0.2f, 0.3f, 0.3f);
    g.setColour(palette.accent.brighter(0.75f));
    g.fillPath(disc, map);

    for (float cx : {0.37f, 0.63f})
    {
        g.setGradientFill(juce::ColourGradient(palette.detail.brighter(0.3f), at(cx, 0.25f),
                                               palette.detail.darker(0.25f), at(cx, 0.45f), false));
        fillUnitEllipse(g, cx, 0.35f, 0.105f, 0.105f, map);
        g.setColour(palette.background);
        fillUnitEllipse(g, cx, 0.355f, 0.06f, 0.06f, map);
        g.setColour(palette.text);
        fillUnitEllipse(g, cx + 0.025f, 0.325f, 0.022f, 0.022f, map);
    }

    juce::Path beak;
    beak.startNewSubPath(0.455f, 0.44f);
    beak.lineTo(0.545f, 0.44f);
    beak.lineTo(0.5f, 0.55f);
    beak.closeSubPath();
    g.setColour(palette.detail.darker(0.15f));
    g.fillPath(beak, map);

    g.setColour(palette.detail);
    for (float fx : {0.36f, 0.4f, 0.44f, 0.56f, 0.6f, 0.64f})
        fillUnitEllipse(g, fx, 0.915f, 0.022f, 0.035f, map);
}

void OwlMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                           const Palette& palette) const
{
    drawSoftShadow(g, centre, radius);
    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.1f), centre.x, centre.y - radius,
                                           palette.panel, centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(palette.outline);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // the owl turns its head with the value: unit = head radius, origin = knob centre
    const float head = radius * 0.74f;
    const auto face = juce::AffineTransform::scale(head).translated(centre).followedBy(
        juce::AffineTransform::rotation(angle, centre.x, centre.y));
    const auto lit = [&](juce::Colour top, juce::Colour bottom)
    { return juce::ColourGradient(top, centre.x, centre.y - head, bottom, centre.x, centre.y + head, false); };

    juce::Path tufts;
    tufts.startNewSubPath(-0.66f, -0.6f);
    tufts.lineTo(-0.86f, -1.18f);
    tufts.lineTo(-0.2f, -0.9f);
    tufts.closeSubPath();
    tufts.startNewSubPath(0.66f, -0.6f);
    tufts.lineTo(0.86f, -1.18f);
    tufts.lineTo(0.2f, -0.9f);
    tufts.closeSubPath();
    juce::Path headPath;
    headPath.addEllipse(-1.0f, -0.96f, 2.0f, 1.96f);
    g.setGradientFill(lit(palette.accent.brighter(0.2f), palette.accent.darker(0.45f)));
    g.fillPath(tufts, face);
    g.fillPath(headPath, face);

    g.setColour(palette.accent.darker(0.4f).withAlpha(0.7f));
    strokeChevrons(g, {{-0.18f, 0.66f}, {0.18f, 0.66f}, {0.0f, 0.8f}}, 0.11f, 0.06f, face);

    juce::Path disc;
    disc.addEllipse(-0.88f, -0.53f, 0.96f, 0.96f);
    disc.addEllipse(-0.08f, -0.53f, 0.96f, 0.96f);
    g.setColour(palette.accent.brighter(0.75f));
    g.fillPath(disc, face);
    g.setColour(palette.accent.darker(0.2f).withAlpha(0.6f));
    g.strokePath(disc, juce::PathStrokeType(0.04f), face);

    for (float cx : {-0.4f, 0.4f})
    {
        g.setGradientFill(lit(palette.detail.brighter(0.35f), palette.detail.darker(0.3f)));
        fillUnitEllipse(g, cx, -0.05f, 0.3f, 0.3f, face);
        g.setColour(palette.background);
        fillUnitEllipse(g, cx, -0.04f, 0.17f, 0.17f, face);
        g.setColour(palette.text);
        fillUnitEllipse(g, cx + 0.08f, -0.14f, 0.06f, 0.06f, face);
    }

    juce::Path beak;
    beak.startNewSubPath(-0.12f, 0.24f);
    beak.lineTo(0.12f, 0.24f);
    beak.lineTo(0.0f, 0.52f);
    beak.closeSubPath();
    g.setColour(palette.detail.darker(0.1f));
    g.fillPath(beak, face);

    // crown feather between the tufts: the value pointer
    g.setColour(palette.detail);
    fillUnitEllipse(g, 0.0f, -0.8f, 0.08f, 0.15f, face);
}

void OwlMotif::drawAmbient(juce::Graphics& g, juce::Rectangle<float> area, double timeSeconds,
                           const Palette& palette) const
{
    const int count = juce::jlimit(6, 140, static_cast<int>(area.getWidth() * area.getHeight() / 700.0f));
    for (int i = 0; i < count; ++i)
    {
        const auto seed = static_cast<std::uint32_t>(i) * 3u;
        const float fx = unitHash(seed + 1u);
        const float fy = unitHash(seed + 2u);
        const float fs = unitHash(seed + 3u);
        const float twinkle = 0.5f + 0.5f * std::sin(static_cast<float>(timeSeconds) * (0.6f + fs * 1.4f) + fs * 40.0f);
        const float size = 0.9f + fs * 1.5f;
        const float alpha = 0.08f + 0.22f * twinkle;
        const float x = area.getX() + fx * area.getWidth();
        const float y = area.getY() + fy * area.getHeight();

        g.setColour((i % 5 == 0 ? palette.text : palette.ambient).withAlpha(alpha));
        g.fillEllipse(x - size * 0.5f, y - size * 0.5f, size, size);
        if (fs > 0.88f)
        {
            g.setColour(palette.ambient.withAlpha(alpha * 0.6f));
            g.drawLine(x - size * 2.2f, y, x + size * 2.2f, y, 0.6f);
            g.drawLine(x, y - size * 2.2f, x, y + size * 2.2f, 0.6f);
        }
    }
}

void OwlMotif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea, const Palette& palette) const
{
    g.setColour(palette.ambient.withAlpha(0.07f));
    g.fillPath(
        makeCrescent(juce::Rectangle<float>(panelArea.getRight() - 64.0f, panelArea.getY() + 128.0f, 46.0f, 46.0f)));
    drawBarredFeather(g, juce::Rectangle<float>(panelArea.getX() + 10.0f, panelArea.getY() + 236.0f, 26.0f, 50.0f),
                      -0.45f, palette.accentAlt.withAlpha(0.07f));
}

void drawEyeCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle, const Palette& palette,
                juce::Colour iris)
{
    drawSoftShadow(g, centre, radius);
    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.12f), centre.x, centre.y - radius,
                                           palette.panel, centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(palette.outline);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);

    const float ir = radius * 0.6f;
    g.setGradientFill(juce::ColourGradient(iris.brighter(0.35f), centre.x, centre.y - ir, iris.darker(0.4f), centre.x,
                                           centre.y + ir, false));
    g.fillEllipse(centre.x - ir, centre.y - ir, ir * 2.0f, ir * 2.0f);
    g.setColour(iris.darker(0.6f).withAlpha(0.8f));
    g.drawEllipse(centre.x - ir, centre.y - ir, ir * 2.0f, ir * 2.0f, 1.0f);

    // the pupil looks where the knob points
    const juce::Point<float> look(std::sin(angle), -std::cos(angle));
    const auto pupil = centre + look * (ir * 0.34f);
    const float pr = ir * 0.46f;
    g.setColour(palette.background);
    g.fillEllipse(pupil.x - pr, pupil.y - pr, pr * 2.0f, pr * 2.0f);
    g.setColour(palette.text.withAlpha(0.9f));
    const float gr = ir * 0.16f;
    g.fillEllipse(centre.x - ir * 0.32f - gr, centre.y - ir * 0.36f - gr, gr * 2.0f, gr * 2.0f);

    juce::Path tick;
    tick.addRoundedRectangle(-radius * 0.06f, -radius * 0.97f, radius * 0.12f, radius * 0.24f, radius * 0.05f);
    g.setColour(iris);
    g.fillPath(tick, juce::AffineTransform::rotation(angle).translated(centre));
}
} // namespace owl::ui
