#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/Drawing.h>
#include <animalaudio/ui/Theme.h>

namespace animalaudio::ui
{
void Motif::drawMascot(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette) const
{
    g.setGradientFill(juce::ColourGradient(palette.accent.brighter(0.2f), area.getX(), area.getY(), palette.accentAlt,
                                           area.getRight(), area.getBottom(), false));
    g.fillPath(makePawPath(area));
}

void Motif::drawKnobCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle,
                        const Palette& palette) const
{
    g.setGradientFill(juce::ColourGradient(palette.panelRaised.brighter(0.1f), centre.x, centre.y - radius,
                                           palette.panel, centre.x, centre.y + radius, false));
    g.fillEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour(palette.outline);
    g.drawEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    juce::Path pointer;
    pointer.addRoundedRectangle(-radius * 0.07f, -radius * 0.92f, radius * 0.14f, radius * 0.42f, radius * 0.06f);
    g.setColour(palette.accent);
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre));
}

void Motif::drawAmbient(juce::Graphics& g, juce::Rectangle<float> area, double timeSeconds,
                        const Palette& palette) const
{
    drawRipples(g, area, palette.ambient, timeSeconds);
}

void Motif::drawDecoration(juce::Graphics& g, juce::Rectangle<float> panelArea, const Palette& palette) const
{
    g.setColour(palette.accent.withAlpha(0.06f));
    g.fillPath(
        makePawPath(juce::Rectangle<float>(panelArea.getRight() - 62.0f, panelArea.getY() + 128.0f, 44.0f, 44.0f)));
    g.setColour(palette.accentAlt.withAlpha(0.06f));
    g.fillPath(makePawPath(juce::Rectangle<float>(panelArea.getX() + 8.0f, panelArea.getY() + 244.0f, 34.0f, 34.0f)));
}
} // namespace animalaudio::ui
