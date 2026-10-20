#include "IcePenguinMotif.h"

#include <cmath>

namespace icepenguin::ui
{
namespace
{
void drawSnowflake(juce::Graphics& g, juce::Point<float> centre, float radius, juce::Colour colour)
{
    juce::Path flake;
    for (int arm = 0; arm < 6; ++arm)
    {
        const float a = static_cast<float>(arm) * juce::MathConstants<float>::pi / 3.0f;
        const juce::Point<float> tip(centre.x + std::sin(a) * radius, centre.y - std::cos(a) * radius);
        flake.startNewSubPath(centre);
        flake.lineTo(tip);
        const juce::Point<float> branch(centre.x + std::sin(a) * radius * 0.55f,
                                        centre.y - std::cos(a) * radius * 0.55f);
        flake.startNewSubPath(branch);
        flake.lineTo(branch.x + std::sin(a + 0.55f) * radius * 0.22f, branch.y - std::cos(a + 0.55f) * radius * 0.22f);
        flake.startNewSubPath(branch);
        flake.lineTo(branch.x + std::sin(a - 0.55f) * radius * 0.22f, branch.y - std::cos(a - 0.55f) * radius * 0.22f);
    }
    g.setColour(colour);
    g.strokePath(
        flake, juce::PathStrokeType(radius * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}
} // namespace

void IcePenguinMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area,
                                 const animalaudio::ui::Palette& palette) const
{
    const auto map =
        juce::AffineTransform::scale(area.getWidth(), area.getHeight()).translated(area.getX(), area.getY());

    juce::Path body;
    body.addEllipse(0.28f, 0.38f, 0.44f, 0.56f);

    juce::Path belly;
    belly.addEllipse(0.36f, 0.48f, 0.28f, 0.38f);

    juce::Path head;
    head.addEllipse(0.34f, 0.06f, 0.32f, 0.34f);

    juce::Path flipperL;
    flipperL.startNewSubPath(0.28f, 0.58f);
    flipperL.quadraticTo(0.08f, 0.62f, 0.12f, 0.78f);
    flipperL.quadraticTo(0.22f, 0.72f, 0.32f, 0.68f);
    flipperL.closeSubPath();

    juce::Path flipperR;
    flipperR.startNewSubPath(0.72f, 0.58f);
    flipperR.quadraticTo(0.92f, 0.62f, 0.88f, 0.78f);
    flipperR.quadraticTo(0.78f, 0.72f, 0.68f, 0.68f);
    flipperR.closeSubPath();

    juce::Path beak;
    beak.startNewSubPath(0.48f, 0.22f);
    beak.lineTo(0.62f, 0.26f);
    beak.lineTo(0.48f, 0.30f);
    beak.closeSubPath();

    g.setColour(palette.panelRaised.darker(0.15f));
    g.fillPath(body, map);
    g.fillPath(flipperL, map);
    g.fillPath(flipperR, map);
    g.fillPath(head, map);

    g.setColour(palette.text.withAlpha(0.92f));
    g.fillPath(belly, map);

    g.setColour(palette.accentAlt);
    g.fillPath(beak, map);

    g.setColour(palette.onAccent);
    const auto eyeL = juce::Rectangle<float>(0.42f, 0.16f, 0.05f, 0.06f).transformedBy(map);
    const auto eyeR = juce::Rectangle<float>(0.53f, 0.16f, 0.05f, 0.06f).transformedBy(map);
    g.fillEllipse(eyeL);
    g.fillEllipse(eyeR);

    g.setColour(palette.accent.withAlpha(0.85f));
    juce::Path scarf;
    scarf.addRoundedRectangle(0.36f, 0.32f, 0.28f, 0.08f, 0.03f);
    g.fillPath(scarf, map);
}

void IcePenguinMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                                  const animalaudio::ui::Palette& palette) const
{
    g.setColour(juce::Colours::black.withAlpha(0.28f));
    g.fillEllipse(centre.x - radius, centre.y - radius + radius * 0.05f, radius * 2.0f, radius * 2.0f);

    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.2f), centre.x, centre.y - radius,
                                           palette.panel.darker(0.1f), centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    const auto rotated = juce::AffineTransform::rotation(angle, centre.x, centre.y);
    drawSnowflake(g, centre.transformedBy(rotated), radius * 0.62f, palette.accent.withAlpha(0.95f));
}

void IcePenguinMotif::drawAmbient(juce::Graphics& g, juce::Rectangle<float> area, double timeSeconds,
                                  const animalaudio::ui::Palette& palette) const
{
    const float phase01 = static_cast<float>(std::fmod(timeSeconds * 0.08, 1.0));
    g.setColour(palette.ambient.withAlpha(0.12f));
    for (int i = 0; i < 5; ++i)
    {
        const float drift = std::fmod(phase01 + static_cast<float>(i) * 0.17f, 1.0f);
        const float x = area.getX() + area.getWidth() * (0.1f + drift * 0.8f);
        const float y = area.getY() + area.getHeight() * (0.15f + static_cast<float>(i) * 0.14f);
        juce::Path crystal;
        crystal.startNewSubPath(x, y - 18.0f);
        crystal.lineTo(x + 10.0f, y);
        crystal.lineTo(x, y + 22.0f);
        crystal.lineTo(x - 10.0f, y);
        crystal.closeSubPath();
        g.fillPath(crystal);
    }
}

void IcePenguinMotif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea,
                                     const animalaudio::ui::Palette& palette) const
{
    g.setColour(palette.detail.withAlpha(0.35f));
    drawSnowflake(g, panelArea.getTopRight().translated(-28.0f, 28.0f), 10.0f, palette.detail.withAlpha(0.5f));
    drawSnowflake(g, panelArea.getBottomLeft().translated(24.0f, -20.0f), 8.0f, palette.accent.withAlpha(0.35f));
}
} // namespace icepenguin::ui
