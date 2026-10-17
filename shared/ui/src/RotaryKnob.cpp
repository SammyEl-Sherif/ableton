#include <animalaudio/ui/RotaryKnob.h>

namespace animalaudio::ui
{
RotaryKnob::RotaryKnob(const Palette& palette)
    : colours(palette)
{
    setSliderStyle(juce::Slider::RotaryVerticalDrag);
    setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters(kStartAngle, kEndAngle, true);
    setMouseDragSensitivity(220);
    setScrollWheelEnabled(true);
}

void RotaryKnob::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(4.0f);
    const float diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    const auto centre = bounds.getCentre();
    const float outer = diameter * 0.5f;
    const float trackThickness = outer * 0.1f;
    const float arcRadius = outer - trackThickness * 0.5f;

    const float proportion = static_cast<float>(valueToProportionOfLength(getValue()));
    const float angle = kStartAngle + proportion * (kEndAngle - kStartAngle);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, kStartAngle, kEndAngle, true);
    g.setColour(colours.panelRaised);
    g.strokePath(track,
                 juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float originProportion =
        arcOrigin ? static_cast<float>(valueToProportionOfLength(juce::jlimit(getMinimum(), getMaximum(), *arcOrigin)))
                  : 0.0f;
    if (std::abs(proportion - originProportion) > 0.001f)
    {
        const float originAngle = kStartAngle + originProportion * (kEndAngle - kStartAngle);
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f, juce::jmin(originAngle, angle),
                            juce::jmax(originAngle, angle), true);
        g.setGradientFill(juce::ColourGradient(colours.accentAlt, centre.x - outer, centre.y + outer, colours.accent,
                                               centre.x + outer, centre.y - outer, false));
        g.strokePath(value,
                     juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float capRadius = outer - trackThickness * 2.4f;
    drawCap(g, centre, capRadius, angle - juce::MathConstants<float>::twoPi);

    if (hasKeyboardFocus(false))
    {
        g.setColour(colours.accent.withAlpha(0.5f));
        g.drawEllipse(bounds.reduced(1.0f), 1.0f);
    }
}

void RotaryKnob::setArcOrigin(double value)
{
    arcOrigin = value;
    repaint();
}

void RotaryKnob::drawCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle)
{
    g.setColour(colours.panel);
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-radius * 0.06f, -radius * 0.95f, radius * 0.12f, radius * 0.4f, radius * 0.05f);
    g.setColour(colours.accent);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));
}
} // namespace animalaudio::ui
