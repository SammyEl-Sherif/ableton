#pragma once

#include <animalaudio/ui/Theme.h>

namespace quail::ui
{
/**
 * The quail: a round slate-blue California quail with its forward-curling teardrop topknot (in the accent colour),
 * cream face stripe and scaled chestnut belly. The knob cap is a dark lens whose pointer is that topknot; the
 * ambient pattern is the scalloped feather scales of the belly.
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
