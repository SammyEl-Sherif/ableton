#include "TurtleTimeMotif.h"

#include <cmath>

namespace turtletime::ui
{
namespace
{
juce::Path scute(float cx, float cy, float rx, float ry)
{
    juce::Path path;
    path.addRoundedRectangle(cx - rx, cy - ry, rx * 2.0f, ry * 2.0f, rx * 0.45f);
    return path;
}

void drawShellTop(juce::Graphics& g, juce::Point<float> centre, float radius, const animalaudio::ui::Palette& palette)
{
    g.setColour(palette.onAccent.withAlpha(0.35f));
    g.fillEllipse(centre.x - radius, centre.y - radius + radius * 0.08f, radius * 2.0f, radius * 2.0f);

    g.setGradientFill(juce::ColourGradient(palette.accent.brighter(0.18f), centre.x, centre.y - radius,
                                           palette.accent.darker(0.25f), centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    g.setColour(palette.accentAlt);
    g.fillEllipse(centre.x - radius * 0.42f, centre.y - radius * 0.48f, radius * 0.84f, radius * 0.7f);
    g.fillEllipse(centre.x - radius * 0.62f, centre.y - radius * 0.05f, radius * 0.5f, radius * 0.42f);
    g.fillEllipse(centre.x + radius * 0.12f, centre.y - radius * 0.05f, radius * 0.5f, radius * 0.42f);

    g.setColour(palette.onAccent.withAlpha(0.45f));
    g.drawEllipse(centre.x - radius * 0.72f, centre.y - radius * 0.72f, radius * 1.44f, radius * 1.44f, radius * 0.06f);
}
} // namespace

void TurtleTimeMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area,
                                 const animalaudio::ui::Palette& palette) const
{
    const auto map =
        juce::AffineTransform::scale(area.getWidth(), area.getHeight()).translated(area.getX(), area.getY());
    const auto skin = palette.detail.darker(0.12f);

    juce::Path tail;
    tail.startNewSubPath(0.22f, 0.58f);
    tail.quadraticTo(0.08f, 0.56f, 0.06f, 0.46f);
    tail.quadraticTo(0.14f, 0.50f, 0.24f, 0.52f);
    tail.closeSubPath();

    juce::Path backLeg;
    backLeg.addEllipse(0.24f, 0.66f, 0.16f, 0.12f);
    juce::Path frontLeg;
    frontLeg.addEllipse(0.58f, 0.68f, 0.18f, 0.13f);
    juce::Path backFoot;
    backFoot.addEllipse(0.30f, 0.74f, 0.10f, 0.07f);
    juce::Path frontFoot;
    frontFoot.addEllipse(0.66f, 0.76f, 0.12f, 0.07f);

    juce::Path shell;
    shell.addEllipse(0.18f, 0.28f, 0.56f, 0.46f);

    juce::Path head;
    head.addEllipse(0.64f, 0.40f, 0.28f, 0.24f);

    g.setColour(skin);
    g.fillPath(tail, map);
    g.fillPath(backLeg, map);
    g.fillPath(frontLeg, map);
    g.fillPath(backFoot, map);
    g.fillPath(frontFoot, map);

    g.setGradientFill(juce::ColourGradient(palette.accent.brighter(0.2f), area.getX() + area.getWidth() * 0.4f,
                                           area.getY() + area.getHeight() * 0.3f, palette.accent.darker(0.28f),
                                           area.getX() + area.getWidth() * 0.45f, area.getBottom(), false));
    g.fillPath(shell, map);

    g.setColour(palette.accentAlt);
    g.fillPath(scute(0.40f, 0.40f, 0.09f, 0.07f), map);
    g.fillPath(scute(0.54f, 0.42f, 0.09f, 0.07f), map);
    g.fillPath(scute(0.46f, 0.54f, 0.11f, 0.07f), map);

    g.setColour(palette.onAccent.withAlpha(0.35f));
    g.strokePath(shell, juce::PathStrokeType(area.getWidth() * 0.012f), map);

    g.setColour(skin.brighter(0.08f));
    g.fillPath(head, map);

    const auto eye = juce::Rectangle<float>(0.78f, 0.47f, 0.045f, 0.04f).transformedBy(map);
    g.setColour(palette.onAccent);
    g.fillEllipse(eye);
    g.setColour(palette.text.withAlpha(0.9f));
    g.fillEllipse(eye.reduced(eye.getWidth() * 0.35f).translated(eye.getWidth() * 0.12f, -eye.getHeight() * 0.1f));

    juce::Path lid;
    lid.addCentredArc(0.80f, 0.48f, 0.045f, 0.028f, 0.0f, juce::MathConstants<float>::pi * 1.05f,
                      juce::MathConstants<float>::pi * 1.95f, true);
    g.setColour(skin.darker(0.25f));
    g.strokePath(
        lid,
        juce::PathStrokeType(area.getWidth() * 0.012f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
        map);

    g.setColour(palette.onAccent.withAlpha(0.7f));
    const auto nostril = juce::Rectangle<float>(0.88f, 0.52f, 0.018f, 0.014f).transformedBy(map);
    g.fillEllipse(nostril);

    juce::Path smile;
    smile.addCentredArc(0.84f, 0.55f, 0.05f, 0.03f, 0.15f, 0.2f, 2.2f, true);
    g.strokePath(
        smile,
        juce::PathStrokeType(area.getWidth() * 0.01f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
        map);
}

void TurtleTimeMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                                  const animalaudio::ui::Palette& palette) const
{
    drawShellTop(g, centre, radius * 0.86f, palette);

    // Same convention as RotaryKnob's pointer: local +Y is up, then rotate with the value.
    juce::Point<float> headOffset(0.0f, -radius * 0.78f);
    headOffset = headOffset.transformedBy(juce::AffineTransform::rotation(angle));
    const auto head = centre + headOffset;
    const float headRadius = radius * 0.34f;
    const auto skin = palette.detail.darker(0.08f);
    const auto outward = headOffset / std::max(1.0f, headOffset.getDistanceFromOrigin());
    const auto pupil = head + outward * headRadius * 0.28f;

    g.setColour(palette.onAccent.withAlpha(0.25f));
    g.fillEllipse(head.x - headRadius, head.y - headRadius + headRadius * 0.12f, headRadius * 2.0f, headRadius * 2.0f);
    g.setColour(skin);
    g.fillEllipse(head.x - headRadius, head.y - headRadius, headRadius * 2.0f, headRadius * 2.0f);
    g.setColour(palette.onAccent);
    g.fillEllipse(pupil.x - headRadius * 0.14f, pupil.y - headRadius * 0.14f, headRadius * 0.28f, headRadius * 0.28f);
}

void TurtleTimeMotif::drawAmbient(juce::Graphics& g, juce::Rectangle<float> area, double timeSeconds,
                                  const animalaudio::ui::Palette& palette) const
{
    for (int i = 0; i < 4; ++i)
    {
        const float drift = static_cast<float>(std::fmod(timeSeconds * 0.03 + static_cast<double>(i) * 0.27, 1.0));
        const float x = area.getX() + area.getWidth() * (0.08f + drift * 0.7f);
        const float y = area.getY() + area.getHeight() * (0.70f + static_cast<float>(i) * 0.05f);
        const float w = 36.0f + static_cast<float>(i) * 8.0f;
        const float h = w * 0.62f;

        juce::Path pad;
        pad.addEllipse(x, y, w, h);
        juce::Path notch;
        notch.addTriangle(x + w * 0.72f, y + h * 0.15f, x + w, y + h * 0.45f, x + w * 0.7f, y + h * 0.7f);
        pad.addPath(notch);

        g.setColour(palette.ambient.withAlpha(0.16f));
        g.fillPath(pad);
        g.setColour(palette.accent.withAlpha(0.2f));
        g.drawEllipse(x, y, w, h, 1.0f);
    }
}

void TurtleTimeMotif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea,
                                     const animalaudio::ui::Palette& palette) const
{
    const auto corner = panelArea.getTopRight().translated(-22.0f, 22.0f);
    drawShellTop(g, corner, 14.0f, palette);
    g.setColour(palette.accent.withAlpha(0.35f));
    g.drawEllipse(corner.x - 18.0f, corner.y - 18.0f, 36.0f, 36.0f, 1.0f);
}
} // namespace turtletime::ui
