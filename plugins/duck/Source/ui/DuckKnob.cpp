#include "DuckKnob.h"

#include "DuckPalette.h"

namespace duck::ui
{
void DuckKnob::drawCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle)
{
    const auto rotate = juce::AffineTransform::rotation(angle, centre.x, centre.y);

    // soft drop shadow so the cap floats above the dial
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(centre.x - radius, centre.y - radius + radius * 0.06f, radius * 2.0f, radius * 2.0f);

    // base disc ("pond")
    g.setGradientFill(juce::ColourGradient(colours.panelRaised.brighter(0.1f), centre.x, centre.y - radius,
                                           colours.panel, centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(colours.outline);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // duck head from above
    const float headR = radius * 0.62f;
    const juce::Rectangle<float> head(centre.x - headR, centre.y - headR * 0.9f, headR * 2.0f, headR * 2.0f);

    juce::Path billPath;
    const float billW = headR * 0.78f;
    billPath.startNewSubPath(centre.x - billW * 0.5f, centre.y - headR * 0.8f);
    billPath.quadraticTo(centre.x, centre.y - radius * 1.12f, centre.x + billW * 0.5f, centre.y - headR * 0.8f);
    billPath.lineTo(centre.x + billW * 0.45f, centre.y - headR * 0.55f);
    billPath.lineTo(centre.x - billW * 0.45f, centre.y - headR * 0.55f);
    billPath.closeSubPath();

    g.setGradientFill(juce::ColourGradient(billColour().brighter(0.3f), centre.x, centre.y - radius,
                                           billColour().darker(0.1f), centre.x, centre.y - headR * 0.5f, false));
    g.fillPath(billPath, rotate);

    g.setGradientFill(juce::ColourGradient(colours.accent.brighter(0.35f), centre.x - headR * 0.4f, centre.y - headR,
                                           colours.accent.darker(0.15f), centre.x + headR * 0.5f, centre.y + headR,
                                           false));
    juce::Path headPath;
    headPath.addEllipse(head);
    g.fillPath(headPath, rotate);

    // nostril dots on the bill
    g.setColour(colours.accentAlt.darker(0.5f));
    for (float dx : {-0.14f, 0.14f})
    {
        juce::Path dot;
        dot.addEllipse(centre.x + dx * headR - 1.5f, centre.y - headR * 0.92f, 3.0f, 3.0f);
        g.fillPath(dot, rotate);
    }

    // eyes
    for (float dx : {-0.42f, 0.42f})
    {
        const float ex = centre.x + dx * headR;
        const float ey = centre.y - headR * 0.25f;
        juce::Path eye;
        eye.addEllipse(ex - headR * 0.11f, ey - headR * 0.11f, headR * 0.22f, headR * 0.22f);
        g.setColour(juce::Colour(0xff1a1610));
        g.fillPath(eye, rotate);
        juce::Path glint;
        glint.addEllipse(ex - headR * 0.01f, ey - headR * 0.08f, headR * 0.07f, headR * 0.07f);
        g.setColour(juce::Colours::white);
        g.fillPath(glint, rotate);
    }

    // tuft of feathers at the back of the head
    g.setColour(colours.accentAlt.withAlpha(0.65f));
    for (int i = -1; i <= 1; ++i)
    {
        juce::Path tuft;
        const float tx = centre.x + static_cast<float>(i) * headR * 0.18f;
        tuft.addEllipse(tx - headR * 0.05f, centre.y + headR * 0.5f, headR * 0.1f, headR * 0.26f);
        g.fillPath(tuft, juce::AffineTransform::rotation(static_cast<float>(i) * 0.35f, tx, centre.y + headR * 0.5f)
                             .followedBy(rotate));
    }
}
} // namespace duck::ui
