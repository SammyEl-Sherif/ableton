#pragma once

#include <animalaudio/ui/Theme.h>

namespace quail::ui
{
/**
 * The quail, drawn as a clean glyph: a California quail silhouette in the accent gradient with a cream face stripe,
 * cut-out eye and belly scales, crowned by its forward-curling teardrop topknot. The knob cap is a dark lens whose
 * pointer is that topknot; the ambient pattern is a faint field of scalloped feather scales.
 */
class QuailMotif : public animalaudio::ui::Motif
{
public:
    void drawMascot(juce::Graphics&, juce::Rectangle<float> area, const animalaudio::ui::Palette&) const override;
    void drawKnobCap(juce::Graphics&, juce::Point<float> centre, float radius, float angle,
                     const animalaudio::ui::Palette&) const override;
    void drawAmbient(juce::Graphics&, juce::Rectangle<float> area, double timeSeconds,
                     const animalaudio::ui::Palette&) const override;
    void drawDecoration(juce::Graphics&, juce::Rectangle<float> panelArea,
                        const animalaudio::ui::Palette&) const override;

    /** The topknot plume as a path: a stem from `base` curling forward into a teardrop, `size` tall. */
    static juce::Path makePlume(juce::Point<float> base, float size);
};
} // namespace quail::ui
