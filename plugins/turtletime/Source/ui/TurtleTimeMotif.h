#pragma once

#include <animalaudio/ui/Theme.h>

namespace turtletime::ui
{
/** Hexagon with flat sides left and right, the shape of one shell scute. */
juce::Path makeHexagon(juce::Point<float> centre, float radius);

/** Geometric top-view turtle mark, machined knob cap with a hex-engraved face. No ambient art. */
class TurtleTimeMotif : public animalaudio::ui::Motif
{
public:
    void drawMascot(juce::Graphics&, juce::Rectangle<float> area, const animalaudio::ui::Palette&) const override;
    void drawKnobCap(juce::Graphics&, juce::Point<float> centre, float radius, float angle,
                     const animalaudio::ui::Palette&) const override;
    void drawAmbient(juce::Graphics&, juce::Rectangle<float>, double, const animalaudio::ui::Palette&) const override {}
    void drawDecoration(juce::Graphics&, juce::Rectangle<float>, const animalaudio::ui::Palette&) const override {}
};
} // namespace turtletime::ui
