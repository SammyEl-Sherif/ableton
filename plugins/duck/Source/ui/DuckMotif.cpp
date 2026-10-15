#include "DuckMotif.h"

namespace duck::ui
{
namespace
{
void drawFeather(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    juce::Path vane;
    vane.startNewSubPath(0.0f, 1.0f);
    vane.cubicTo(-0.15f, 0.5f, 0.35f, 0.1f, 1.0f, 0.0f);
    vane.cubicTo(1.05f, 0.5f, 0.55f, 0.95f, 0.0f, 1.0f);
    vane.closeSubPath();

    const auto map =
        juce::AffineTransform::scale(area.getWidth(), area.getHeight()).translated(area.getX(), area.getY());
    g.setColour(colour);
    g.fillPath(vane, map);

    juce::Path spine;
    spine.startNewSubPath(-0.02f, 1.04f);
    spine.quadraticTo(0.45f, 0.55f, 1.0f, 0.0f);
    g.setColour(colour.withMultipliedBrightness(0.6f).withAlpha(juce::jmin(1.0f, colour.getFloatAlpha() * 1.5f)));
    g.strokePath(spine, juce::PathStrokeType(area.getWidth() * 0.03f), map);
}
} // namespace

void DuckMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area,
                           const animalaudio::ui::Palette& palette) const
{
    const auto map =
        juce::AffineTransform::scale(area.getWidth(), area.getHeight()).translated(area.getX(), area.getY());

    juce::Path tail;
    tail.startNewSubPath(0.12f, 0.62f);
    tail.quadraticTo(0.02f, 0.58f, 0.0f, 0.36f);
    tail.quadraticTo(0.14f, 0.44f, 0.26f, 0.5f);
    tail.closeSubPath();

    juce::Path body;
    body.addEllipse(0.04f, 0.46f, 0.80f, 0.46f);

    juce::Path head;
    head.addEllipse(0.50f, 0.08f, 0.40f, 0.42f);

    juce::Path neck;
    neck.addRoundedRectangle(0.54f, 0.34f, 0.26f, 0.3f, 0.08f);

    juce::Path bill;
    bill.startNewSubPath(0.84f, 0.26f);
    bill.quadraticTo(1.0f, 0.27f, 1.0f, 0.34f);
    bill.quadraticTo(0.99f, 0.42f, 0.84f, 0.38f);
    bill.closeSubPath();

    juce::Path wing;
    wing.startNewSubPath(0.22f, 0.66f);
    wing.quadraticTo(0.40f, 0.60f, 0.62f, 0.64f);
    wing.quadraticTo(0.50f, 0.86f, 0.26f, 0.78f);
    wing.closeSubPath();

    g.setGradientFill(juce::ColourGradient(palette.accent.brighter(0.25f), area.getX() + area.getWidth() * 0.5f,
                                           area.getY(), palette.accent.darker(0.1f),
                                           area.getX() + area.getWidth() * 0.3f, area.getBottom(), false));
    g.fillPath(tail, map);
    g.fillPath(body, map);
    g.fillPath(neck, map);
    g.fillPath(head, map);

    g.setColour(palette.accentAlt.withAlpha(0.85f));
    g.fillPath(wing, map);

    g.setColour(palette.detail);
    g.fillPath(bill, map);

    g.setColour(juce::Colour(0xff1a1610));
    const auto eye = juce::Rectangle<float>(0.69f, 0.2f, 0.06f, 0.06f).transformedBy(map);
    g.fillEllipse(eye);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.fillEllipse(eye.reduced(eye.getWidth() * 0.32f).translated(eye.getWidth() * 0.12f, -eye.getHeight() * 0.12f));

    g.setColour(juce::Colours::white.withAlpha(0.35f));
    juce::Path shine;
    shine.addCentredArc(0.7f, 0.29f, 0.17f, 0.17f, 0.0f, juce::MathConstants<float>::pi * 1.05f,
                        juce::MathConstants<float>::pi * 1.45f, true);
    g.strokePath(
        shine,
        juce::PathStrokeType(area.getWidth() * 0.025f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
        map);
}

void DuckMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                            const animalaudio::ui::Palette& palette) const
{
    const auto rotate = juce::AffineTransform::rotation(angle, centre.x, centre.y);

    // soft drop shadow so the cap floats above the dial
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(centre.x - radius, centre.y - radius + radius * 0.06f, radius * 2.0f, radius * 2.0f);

    // base disc ("pond")
    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.1f), centre.x, centre.y - radius,
                                           palette.panel, centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(palette.outline);
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

    g.setGradientFill(juce::ColourGradient(palette.detail.brighter(0.3f), centre.x, centre.y - radius,
                                           palette.detail.darker(0.1f), centre.x, centre.y - headR * 0.5f, false));
    g.fillPath(billPath, rotate);

    g.setGradientFill(juce::ColourGradient(palette.accent.brighter(0.35f), centre.x - headR * 0.4f, centre.y - headR,
                                           palette.accent.darker(0.15f), centre.x + headR * 0.5f, centre.y + headR,
                                           false));
    juce::Path headPath;
    headPath.addEllipse(head);
    g.fillPath(headPath, rotate);

    // nostril dots on the bill
    g.setColour(palette.accentAlt.darker(0.5f));
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
    g.setColour(palette.accentAlt.withAlpha(0.65f));
    for (int i = -1; i <= 1; ++i)
    {
        juce::Path tuft;
        const float tx = centre.x + static_cast<float>(i) * headR * 0.18f;
        tuft.addEllipse(tx - headR * 0.05f, centre.y + headR * 0.5f, headR * 0.1f, headR * 0.26f);
        g.fillPath(tuft, juce::AffineTransform::rotation(static_cast<float>(i) * 0.35f, tx, centre.y + headR * 0.5f)
                             .followedBy(rotate));
    }
}

void DuckMotif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea,
                               const animalaudio::ui::Palette& palette) const
{
    drawFeather(g, juce::Rectangle<float>(panelArea.getRight() - 62.0f, panelArea.getY() + 128.0f, 44.0f, 44.0f),
                palette.accent.withAlpha(0.07f));
    drawFeather(g, juce::Rectangle<float>(panelArea.getX() + 6.0f, panelArea.getY() + 244.0f, 36.0f, 36.0f),
                palette.accentAlt.withAlpha(0.07f));
}
} // namespace duck::ui
