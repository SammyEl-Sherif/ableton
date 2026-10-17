#pragma once

#include <animalaudio/ui/Theme.h>

namespace owl::ui
{
/** The owl: perched mascot, owl-face knob cap that tilts its head with the value, a twinkling night sky,
    a crescent moon and a barred feather. */
class OwlMotif : public animalaudio::ui::Motif
{
public:
    void drawMascot(juce::Graphics&, juce::Rectangle<float> area, const animalaudio::ui::Palette&) const override;
    void drawKnobCap(juce::Graphics&, juce::Point<float> centre, float radius, float angle,
                     const animalaudio::ui::Palette&) const override;
    void drawAmbient(juce::Graphics&, juce::Rectangle<float> area, double timeSeconds,
                     const animalaudio::ui::Palette&) const override;
    void drawDecoration(juce::Graphics&, juce::Rectangle<float> panelArea,
                        const animalaudio::ui::Palette&) const override;
};

/** Small knob cap drawn as an owl's eye: an `iris`-coloured ring whose pupil looks towards `angle`. */
void drawEyeCap(juce::Graphics&, juce::Point<float> centre, float radius, float angle, const animalaudio::ui::Palette&,
                juce::Colour iris);
} // namespace owl::ui
