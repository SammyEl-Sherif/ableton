#include "QuailMotif.h"

namespace quail::ui
{
using animalaudio::ui::Palette;

namespace
{
juce::Path scaleField(juce::Rectangle<float> area, float cell)
{
    juce::Path scales;
    const int rows = static_cast<int>(area.getHeight() / (cell * 0.6f)) + 1;
    const int cols = static_cast<int>(area.getWidth() / cell) + 2;
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
        {
            const float x = area.getX() + static_cast<float>(col) * cell + (row % 2 == 1 ? cell * 0.5f : 0.0f);
            const float y = area.getBottom() - static_cast<float>(row) * cell * 0.6f;
            scales.addCentredArc(x, y, cell * 0.5f, cell * 0.5f, 0.0f, juce::MathConstants<float>::halfPi,
                                 juce::MathConstants<float>::pi * 1.5f, true);
        }
    return scales;
}
} // namespace

juce::Path QuailMotif::makePlume(juce::Point<float> base, float size)
{
    juce::Path p;
    p.startNewSubPath(-0.09f, 0.0f);
    p.cubicTo(-0.13f, -0.36f, -0.05f, -0.66f, 0.15f, -0.82f);
    p.cubicTo(0.3f, -0.95f, 0.58f, -0.9f, 0.56f, -0.7f);
    p.cubicTo(0.54f, -0.54f, 0.36f, -0.5f, 0.24f, -0.6f);
    p.cubicTo(0.13f, -0.5f, 0.09f, -0.3f, 0.09f, 0.0f);
    p.closeSubPath();
    p.applyTransform(juce::AffineTransform::scale(size).translated(base));
    return p;
}

void QuailMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette) const
{
    const float w = area.getWidth();
    const float h = area.getHeight();
    const auto at = [&](float fx, float fy) { return juce::Point<float>(area.getX() + fx * w, area.getY() + fy * h); };

    // one teardrop silhouette facing right: beak, crown, back, cocked tail, round belly, chest, throat
    juce::Path body;
    body.startNewSubPath(at(0.93f, 0.405f));
    body.quadraticTo(at(0.86f, 0.33f), at(0.8f, 0.3f));
    body.cubicTo(at(0.76f, 0.22f), at(0.62f, 0.22f), at(0.6f, 0.32f));
    body.cubicTo(at(0.58f, 0.4f), at(0.5f, 0.42f), at(0.4f, 0.44f));
    body.cubicTo(at(0.3f, 0.46f), at(0.22f, 0.5f), at(0.16f, 0.5f));
    body.lineTo(at(0.05f, 0.46f));
    body.cubicTo(at(0.08f, 0.55f), at(0.12f, 0.6f), at(0.17f, 0.62f));
    body.cubicTo(at(0.18f, 0.8f), at(0.32f, 0.9f), at(0.48f, 0.9f));
    body.cubicTo(at(0.68f, 0.9f), at(0.82f, 0.76f), at(0.83f, 0.58f));
    body.cubicTo(at(0.84f, 0.5f), at(0.86f, 0.45f), at(0.89f, 0.43f));
    body.closeSubPath();
    const auto bounds = body.getBounds();
    g.setGradientFill(juce::ColourGradient(palette.accent, bounds.getX(), bounds.getY(), palette.accentAlt.darker(0.5f),
                                           bounds.getRight(), bounds.getBottom(), false));
    g.fillPath(body);

    // wing line and belly scales cut into the silhouette
    g.setColour(palette.background.withAlpha(0.4f));
    juce::Path wing;
    wing.startNewSubPath(at(0.26f, 0.58f));
    wing.quadraticTo(at(0.44f, 0.56f), at(0.62f, 0.7f));
    g.strokePath(wing, juce::PathStrokeType(0.028f * w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    juce::Path scales;
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 3; ++col)
        {
            const auto c = at(0.5f + 0.075f * static_cast<float>(col) - (row == 1 ? 0.035f : 0.0f),
                              0.75f + 0.07f * static_cast<float>(row));
            scales.addCentredArc(c.x, c.y, 0.03f * w, 0.03f * w, 0.0f, juce::MathConstants<float>::halfPi,
                                 juce::MathConstants<float>::pi * 1.5f, true);
        }
    g.strokePath(scales, juce::PathStrokeType(0.02f * w));

    // cream face stripe over the eye, the eye itself
    juce::Path stripe;
    stripe.startNewSubPath(at(0.66f, 0.31f));
    stripe.quadraticTo(at(0.76f, 0.275f), at(0.85f, 0.335f));
    g.setColour(palette.detail);
    g.strokePath(stripe, juce::PathStrokeType(0.024f * w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(palette.background);
    g.fillEllipse(juce::Rectangle<float>(0.05f * w, 0.05f * w).withCentre(at(0.785f, 0.36f)));

    g.setColour(palette.text);
    g.fillPath(makePlume(at(0.72f, 0.255f), 0.26f * h));
}

void QuailMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                             const Palette& palette) const
{
    const auto disc = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
    g.setColour(palette.background.withAlpha(0.75f));
    g.fillEllipse(disc.translated(0.0f, radius * 0.1f).expanded(radius * 0.03f));
    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.22f), centre.x, disc.getY(),
                                           palette.panel.darker(0.15f), centre.x, disc.getBottom(), false));
    g.fillEllipse(disc);
    g.setGradientFill(juce::ColourGradient(palette.text.withAlpha(0.16f), centre.x, disc.getY(),
                                           palette.text.withAlpha(0.0f), centre.x, centre.y + radius * 0.3f, false));
    g.drawEllipse(disc.reduced(0.5f), 1.0f);
    g.setColour(palette.text.withAlpha(0.04f));
    g.drawEllipse(disc.reduced(radius * 0.18f), 1.0f);

    const auto plume = makePlume({0.0f, -radius * 0.3f}, radius * 0.6f);
    g.setColour(palette.text);
    g.fillPath(plume, juce::AffineTransform::rotation(angle).translated(centre));
}

void QuailMotif::drawAmbient(juce::Graphics& g, juce::Rectangle<float> area, double timeSeconds,
                             const Palette& palette) const
{
    juce::ignoreUnused(timeSeconds);
    g.setGradientFill(juce::ColourGradient(palette.ambient.withAlpha(0.0f), 0.0f, area.getY(),
                                           palette.ambient.withAlpha(0.06f), 0.0f, area.getBottom(), false));
    g.strokePath(scaleField(area, 18.0f), juce::PathStrokeType(1.0f));
}

void QuailMotif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea, const Palette& palette) const
{
    g.setColour(palette.ambient.withAlpha(0.05f));
    g.strokePath(scaleField(panelArea.withTop(panelArea.getBottom() - 40.0f), 14.0f), juce::PathStrokeType(1.0f));
}
} // namespace quail::ui
