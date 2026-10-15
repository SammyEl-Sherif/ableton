#pragma once

#include <animalaudio/ui/Theme.h>

namespace duck::ui
{
/** Rubber duck mascot, duck-head knob cap, feather decorations and pond ripples. */
class DuckMotif : public animalaudio::ui::Motif
{
public:
    void drawMascot(juce::Graphics&, juce::Rectangle<float> area, const animalaudio::ui::Palette&) const override;
    void drawKnobCap(juce::Graphics&, juce::Point<float> centre, float radius, float angle,
                     const animalaudio::ui::Palette&) const override;
    void drawDecoration(juce::Graphics&, juce::Rectangle<float> panelArea,
                        const animalaudio::ui::Palette&) const override;
    // drawAmbient: the shared ripples, tinted with the palette's `ambient` (water blue), are exactly Duck's pond.
};
} // namespace duck::ui
