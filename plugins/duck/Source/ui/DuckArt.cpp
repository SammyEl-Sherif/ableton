#include "DuckArt.h"

#include "DuckPalette.h"

namespace duck::ui
{
void drawRubberDuck(juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto map =
        juce::AffineTransform::scale(area.getWidth(), area.getHeight()).translated(area.getX(), area.getY());
    const auto palette = makePalette();

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

    g.setColour(billColour());
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

void drawWater(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, double time)
{
    constexpr int layers = 3;
    for (int layer = 0; layer < layers; ++layer)
    {
        const float yBase = area.getY() + area.getHeight() * (0.2f + 0.3f * static_cast<float>(layer));
        const float amplitude = 2.0f + static_cast<float>(layer);
        const float wavelength = 46.0f + 22.0f * static_cast<float>(layer);
        const float phase =
            static_cast<float>(time) * (0.5f + 0.25f * static_cast<float>(layer)) * (layer % 2 == 0 ? 1.0f : -1.0f);

        juce::Path wave;
        for (float x = 0.0f; x <= area.getWidth(); x += 3.0f)
        {
            const float y = yBase + amplitude * std::sin(x / wavelength * juce::MathConstants<float>::twoPi + phase);
            if (x == 0.0f)
                wave.startNewSubPath(area.getX() + x, y);
            else
                wave.lineTo(area.getX() + x, y);
        }
        g.setColour(colour.withAlpha(0.09f + 0.03f * static_cast<float>(layers - layer)));
        g.strokePath(wave, juce::PathStrokeType(1.4f));
    }
}

juce::Path makeCurvePath(const animalaudio::dsp::ShapeCurve& curve, juce::Rectangle<float> area, int steps)
{
    juce::Path p;
    for (int i = 0; i <= steps; ++i)
    {
        const float x = static_cast<float>(i) / static_cast<float>(steps);
        const float px = area.getX() + x * area.getWidth();
        const float py = area.getBottom() - curve.evaluate(x) * area.getHeight();
        if (i == 0)
            p.startNewSubPath(px, py);
        else
            p.lineTo(px, py);
    }
    return p;
}
} // namespace duck::ui
