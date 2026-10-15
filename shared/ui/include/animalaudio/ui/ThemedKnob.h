#pragma once

#include "RotaryKnob.h"
#include "Theme.h"

namespace animalaudio::ui
{
/** Big rotary knob whose cap is drawn by the theme's motif (duck head, fox face, ...). */
class ThemedKnob : public RotaryKnob
{
public:
    explicit ThemedKnob(const Theme& themeToUse)
        : RotaryKnob(themeToUse.palette)
        , theme(themeToUse)
    {
    }

protected:
    void drawCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle) override
    {
        theme.motif->drawKnobCap(g, centre, radius, angle, theme.palette);
    }

private:
    Theme theme;
};
} // namespace animalaudio::ui
