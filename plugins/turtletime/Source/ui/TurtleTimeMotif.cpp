#include "TurtleTimeMotif.h"

#include <cmath>

namespace turtletime::ui
{
namespace
{
juce::Point<float> hexVertex(juce::Point<float> centre, float radius, int index)
{
    const float angle = juce::MathConstants<float>::pi / 3.0f * static_cast<float>(index);
    return {centre.x + radius * std::cos(angle), centre.y + radius * std::sin(angle)};
}

juce::Point<float> polar(juce::Point<float> centre, float radius, float angle)
{
    // angle 0 = 12 o'clock, clockwise, like RotaryKnob
    return {centre.x + radius * std::sin(angle), centre.y - radius * std::cos(angle)};
}
} // namespace

juce::Path makeHexagon(juce::Point<float> centre, float radius)
{
    juce::Path path;
    path.startNewSubPath(hexVertex(centre, radius, 0));
    for (int i = 1; i < 6; ++i)
        path.lineTo(hexVertex(centre, radius, i));
    path.closeSubPath();
    return path;
}

void TurtleTimeMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area,
                                 const animalaudio::ui::Palette& palette) const
{
    const float size = std::min(area.getWidth(), area.getHeight());
    const auto centre = area.getCentre().translated(0.0f, size * 0.04f);
    const float shell = size * 0.30f;
    const float stroke = std::max(1.2f, size * 0.045f);

    const float limb = size * 0.11f;
    for (const float angle : {0.85f, 2.29f, 3.99f, 5.43f})
    {
        const auto at = polar(centre, shell * 1.08f, angle);
        g.setColour(palette.accent);
        g.fillEllipse(at.x - limb * 0.5f, at.y - limb * 0.5f, limb, limb);
    }
    const float head = size * 0.15f;
    const auto headAt = polar(centre, shell * 1.22f, 0.0f);
    g.setColour(palette.accent);
    g.fillEllipse(headAt.x - head * 0.5f, headAt.y - head * 0.6f, head, head * 1.1f);

    const auto outer = makeHexagon(centre, shell);
    g.setColour(palette.background);
    g.fillPath(outer);
    g.setColour(palette.accent.withAlpha(0.16f));
    g.fillPath(outer);
    g.setColour(palette.accent);
    g.strokePath(outer, juce::PathStrokeType(stroke, juce::PathStrokeType::mitered));

    const float inner = shell * 0.42f;
    g.strokePath(makeHexagon(centre, inner), juce::PathStrokeType(stroke * 0.8f, juce::PathStrokeType::mitered));
    for (int i = 0; i < 6; ++i)
    {
        const auto a = hexVertex(centre, inner, i);
        const auto b = hexVertex(centre, shell, i);
        g.drawLine(a.x, a.y, b.x, b.y, stroke * 0.8f);
    }
}

void TurtleTimeMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                                  const animalaudio::ui::Palette& palette) const
{
    const float body = radius * 0.84f;

    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillEllipse(centre.x - body, centre.y - body + radius * 0.07f, body * 2.0f, body * 2.0f);

    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.35f), centre.x, centre.y - body,
                                           palette.background, centre.x, centre.y + body, false));
    g.fillEllipse(centre.x - body, centre.y - body, body * 2.0f, body * 2.0f);

    g.setGradientFill(juce::ColourGradient(palette.text.withAlpha(0.22f), centre.x, centre.y - body,
                                           palette.text.withAlpha(0.0f), centre.x, centre.y + body * 0.4f, false));
    g.drawEllipse(centre.x - body + 0.5f, centre.y - body + 0.5f, body * 2.0f - 1.0f, body * 2.0f - 1.0f, 1.0f);

    const float face = body * 0.80f;
    g.setGradientFill(juce::ColourGradient(palette.panelRaised, centre.x, centre.y - face, palette.panel, centre.x,
                                           centre.y + face, false));
    g.fillEllipse(centre.x - face, centre.y - face, face * 2.0f, face * 2.0f);

    g.setColour(palette.outline.withAlpha(0.9f));
    g.strokePath(makeHexagon(centre, face * 0.36f), juce::PathStrokeType(1.0f));

    const auto from = polar(centre, face * 0.44f, angle);
    const auto to = polar(centre, face * 0.92f, angle);
    g.setColour(palette.text);
    g.drawLine(from.x, from.y, to.x, to.y, std::max(2.0f, radius * 0.06f));
}
} // namespace turtletime::ui
