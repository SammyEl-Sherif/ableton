#include "TurtleTimeWidgets.h"

#include <cmath>

namespace turtletime::ui
{
juce::Font trackedFont(float height, bool bold, float kerning)
{
    return animalaudio::ui::makeFont(height, bold).withExtraKerningFactor(kerning);
}

TurtleKnob::TurtleKnob(const animalaudio::ui::Theme& theme)
    : animalaudio::ui::RotaryKnob(theme.palette)
    , motif(theme.motif)
{
}

void TurtleKnob::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(2.0f);
    const float outer = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
    const float angle = kStartAngle + proportion * (kEndAngle - kStartAngle);

    constexpr int kTicks = 21;
    for (int i = 0; i < kTicks; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(kTicks - 1);
        const float a = kStartAngle + t * (kEndAngle - kStartAngle);
        const bool major = i % 5 == 0;
        const float r0 = outer - (major ? 7.0f : 4.5f);
        const float r1 = outer - 1.0f;
        const juce::Point<float> dir(std::sin(a), -std::cos(a));
        g.setColour(t <= proportion + 1.0e-4f ? colours.accent.withAlpha(major ? 0.95f : 0.7f)
                                              : colours.outline.brighter(major ? 0.35f : 0.1f));
        g.drawLine(centre.x + dir.x * r0, centre.y + dir.y * r0, centre.x + dir.x * r1, centre.y + dir.y * r1,
                   major ? 1.6f : 1.1f);
    }

    const float arcRadius = outer - 13.0f;
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour(colours.background);
    g.strokePath(track, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (proportion > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, kStartAngle, angle, true);
        g.setColour(colours.accent.withAlpha(0.18f));
        g.strokePath(value, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colours.accent);
        g.strokePath(value, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    if (motif)
        motif->drawKnobCap(g, centre, arcRadius - 7.0f, angle, colours);
}

PatternCell::PatternCell(const juce::String& label, const animalaudio::ui::Palette& palette)
    : juce::Button(label)
    , colours(palette)
{
    setClickingTogglesState(false);
    setTriggeredOnMouseDown(true);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void PatternCell::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    const auto bounds = getLocalBounds().toFloat().reduced(1.5f);
    const bool on = getToggleState();
    constexpr float radius = 4.0f;

    if (on)
    {
        g.setColour(colours.accent.withAlpha(0.16f));
        g.fillRoundedRectangle(bounds.expanded(1.5f), radius + 1.5f);
        g.setGradientFill(juce::ColourGradient(colours.accent.brighter(0.12f), bounds.getX(), bounds.getY(),
                                               colours.accent.darker(0.18f), bounds.getX(), bounds.getBottom(), false));
        g.fillRoundedRectangle(bounds, radius);
    }
    else
    {
        g.setGradientFill(juce::ColourGradient(colours.panelRaised.brighter(isMouseOver ? 0.14f : 0.05f), bounds.getX(),
                                               bounds.getY(), colours.panelRaised, bounds.getX(), bounds.getBottom(),
                                               false));
        g.fillRoundedRectangle(bounds, radius);
        g.setColour(isMouseOver ? colours.accent.withAlpha(0.55f) : colours.outline);
        g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
        g.setColour(colours.text.withAlpha(0.05f));
        g.drawHorizontalLine(static_cast<int>(bounds.getY() + 1.0f), bounds.getX() + radius,
                             bounds.getRight() - radius);
    }

    if (isButtonDown)
    {
        g.setColour(colours.background.withAlpha(0.15f));
        g.fillRoundedRectangle(bounds, radius);
    }

    g.setColour(on ? colours.onAccent : (isMouseOver ? colours.text : colours.text.withAlpha(0.82f)));
    g.setFont(trackedFont(12.5f, true, 0.04f));
    g.drawText(getButtonText(), bounds, juce::Justification::centred, false);
}
} // namespace turtletime::ui
