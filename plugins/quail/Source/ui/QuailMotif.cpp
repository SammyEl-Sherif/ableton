#include "QuailMotif.h"

#include <cmath>

namespace quail::ui
{
using animalaudio::ui::Palette;

juce::Path QuailMotif::makePlume(juce::Point<float> base, float size)
{
    juce::Path p;
    p.startNewSubPath(-0.07f, 0.0f);
    p.cubicTo(-0.11f, -0.36f, -0.03f, -0.66f, 0.17f, -0.81f);
    p.cubicTo(0.30f, -0.93f, 0.53f, -0.87f, 0.51f, -0.70f);
    p.cubicTo(0.49f, -0.57f, 0.35f, -0.54f, 0.24f, -0.62f);
    p.cubicTo(0.12f, -0.52f, 0.07f, -0.30f, 0.07f, 0.0f);
    p.closeSubPath();
    p.applyTransform(juce::AffineTransform::scale(size).translated(base));
    return p;
}

void QuailMotif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette) const
{
    const float w = area.getWidth();
    const float h = area.getHeight();
    const auto at = [&](float fx, float fy) { return juce::Point<float>(area.getX() + fx * w, area.getY() + fy * h); };
    const auto box = [&](float cx, float cy, float bw, float bh)
    { return juce::Rectangle<float>(bw * w, bh * h).withCentre(at(cx, cy)); };

    // shadow, legs, tail
    g.setColour(palette.background.withAlpha(0.5f));
    g.fillEllipse(box(0.5f, 0.95f, 0.62f, 0.07f));
    g.setColour(palette.detail.withAlpha(0.7f));
    for (float x : {0.42f, 0.56f})
        g.drawLine(juce::Line<float>(at(x, 0.82f), at(x - 0.02f, 0.94f)), 0.035f * w);

    juce::Path tail;
    tail.startNewSubPath(at(0.24f, 0.55f));
    tail.lineTo(at(0.04f, 0.47f));
    tail.lineTo(at(0.08f, 0.62f));
    tail.closeSubPath();
    g.setColour(palette.ambient.darker(0.5f));
    g.fillPath(tail);

    // body
    const auto body = box(0.47f, 0.62f, 0.64f, 0.46f);
    g.setGradientFill(juce::ColourGradient(palette.ambient.brighter(0.25f), body.getX(), body.getY(),
                                           palette.ambient.darker(0.55f), body.getRight(), body.getBottom(), false));
    g.fillEllipse(body);

    // chestnut belly with cream-edged scales
    const auto belly = box(0.58f, 0.74f, 0.34f, 0.2f);
    g.setColour(palette.accentAlt.withAlpha(0.9f));
    g.fillEllipse(belly);
    juce::Path scales;
    const float s = 0.05f * w;
    for (int row = 0; row < 2; ++row)
        for (int col = 0; col < 4; ++col)
        {
            const auto c = at(0.47f + 0.065f * static_cast<float>(col) + (row == 1 ? 0.03f : 0.0f),
                              0.7f + 0.06f * static_cast<float>(row));
            scales.addCentredArc(c.x, c.y, s * 0.5f, s * 0.5f, 0.0f, juce::MathConstants<float>::halfPi,
                                 juce::MathConstants<float>::pi * 1.5f, true);
        }
    g.setColour(palette.detail.withAlpha(0.75f));
    g.strokePath(scales, juce::PathStrokeType(0.018f * w));

    // wing with cream streaks
    juce::Path wing;
    wing.addEllipse(box(0.36f, 0.58f, 0.36f, 0.22f));
    wing.applyTransform(juce::AffineTransform::rotation(-0.25f, at(0.36f, 0.58f).x, at(0.36f, 0.58f).y));
    g.setColour(palette.ambient.darker(0.35f));
    g.fillPath(wing);
    g.setColour(palette.detail.withAlpha(0.55f));
    for (int i = 0; i < 3; ++i)
    {
        const float y = 0.53f + 0.045f * static_cast<float>(i);
        g.drawLine(juce::Line<float>(at(0.24f, y + 0.03f), at(0.46f, y - 0.02f)), 0.014f * w);
    }

    // head: dark face and throat ringed by the cream stripe
    const auto head = box(0.72f, 0.34f, 0.3f, 0.3f);
    g.setColour(palette.ambient.darker(0.15f));
    g.fillEllipse(head);
    juce::Path throat;
    throat.addEllipse(box(0.77f, 0.42f, 0.17f, 0.15f));
    g.setColour(palette.background);
    g.fillPath(throat);
    g.setColour(palette.detail);
    g.strokePath(throat, juce::PathStrokeType(0.022f * w));
    g.drawLine(juce::Line<float>(at(0.65f, 0.28f), at(0.84f, 0.27f)), 0.022f * w);

    // eye and beak
    g.setColour(palette.text);
    g.fillEllipse(box(0.76f, 0.31f, 0.065f, 0.065f));
    g.setColour(palette.background);
    g.fillEllipse(box(0.767f, 0.31f, 0.032f, 0.032f));
    juce::Path beak;
    beak.startNewSubPath(at(0.85f, 0.32f));
    beak.lineTo(at(0.95f, 0.36f));
    beak.lineTo(at(0.85f, 0.39f));
    beak.closeSubPath();
    g.setColour(palette.background.brighter(0.2f));
    g.fillPath(beak);

    // the topknot
    const auto plume = makePlume(at(0.7f, 0.21f), 0.24f * h);
    g.setColour(palette.accent.withAlpha(0.3f));
    g.strokePath(plume, juce::PathStrokeType(0.04f * w));
    g.setColour(palette.accent);
    g.fillPath(plume);
}

void QuailMotif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                             const Palette& palette) const
{
    const auto disc = juce::Rectangle<float>(radius * 2.0f, radius * 2.0f).withCentre(centre);
    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.15f), disc.getX(), disc.getY(),
                                           palette.background, disc.getRight(), disc.getBottom(), false));
    g.fillEllipse(disc);
    g.setColour(palette.outline);
    g.drawEllipse(disc.reduced(radius * 0.16f), 1.0f);
    g.setColour(palette.background.withAlpha(0.8f));
    g.drawEllipse(disc, 1.2f);

    const auto plume = makePlume({0.0f, -radius * 0.12f}, radius * 0.86f).createPathWithRoundedCorners(radius * 0.02f);
    const auto toKnob = juce::AffineTransform::rotation(angle).translated(centre);
    g.setColour(palette.accent.withAlpha(0.28f));
    g.strokePath(plume, juce::PathStrokeType(radius * 0.12f), toKnob);
    g.setColour(palette.accent);
    g.fillPath(plume, toKnob);

    g.setColour(palette.detail);
    g.fillEllipse(juce::Rectangle<float>(radius * 0.2f, radius * 0.2f).withCentre(centre));
}

void QuailMotif::drawAmbient(juce::Graphics& g, juce::Rectangle<float> area, double timeSeconds,
                             const Palette& palette) const
{
    // scalloped belly scales, fading out upwards
    const float cell = 22.0f;
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
    g.setGradientFill(juce::ColourGradient(palette.ambient.withAlpha(0.0f), 0.0f, area.getY(),
                                           palette.ambient.withAlpha(0.09f), 0.0f, area.getBottom(), false));
    g.strokePath(scales, juce::PathStrokeType(1.0f));

    // a slow scan of light sweeping across
    const float t = static_cast<float>(std::fmod(timeSeconds / 14.0, 1.0));
    const float x = area.getX() + (t * 1.6f - 0.3f) * area.getWidth();
    juce::ColourGradient sweep(palette.accent.withAlpha(0.0f), x - 160.0f, 0.0f, palette.accent.withAlpha(0.0f),
                               x + 160.0f, 0.0f, false);
    sweep.addColour(0.5, palette.accent.withAlpha(0.035f));
    g.setGradientFill(sweep);
    g.fillRect(area);
}

void QuailMotif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea, const Palette& palette) const
{
    juce::Random random(184);
    g.setColour(palette.detail.withAlpha(0.06f));
    for (int i = 0; i < 18; ++i)
    {
        const float x = panelArea.getX() + random.nextFloat() * panelArea.getWidth();
        const float y = panelArea.getY() + random.nextFloat() * panelArea.getHeight();
        const float size = 6.0f + random.nextFloat() * 8.0f;
        g.fillPath(makePlume({x, y}, size), juce::AffineTransform::rotation(random.nextFloat() * 6.28f, x, y));
    }
}
} // namespace quail::ui
