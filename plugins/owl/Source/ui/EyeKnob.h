#pragma once

#include "OwlMotif.h"

#include <animalaudio/ui/RotaryKnob.h>

namespace owl::ui
{
/** Band knob: the value arc and an owl-eye cap in the band's colour. */
class EyeKnob : public animalaudio::ui::RotaryKnob
{
public:
    EyeKnob(const animalaudio::ui::Palette& palette, juce::Colour bandColour)
        : RotaryKnob(withBandColour(palette, bandColour))
        , iris(bandColour)
    {
    }

protected:
    void drawCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle) override
    {
        drawEyeCap(g, centre, radius, angle, colours, iris);
    }

private:
    static animalaudio::ui::Palette withBandColour(animalaudio::ui::Palette p, juce::Colour band)
    {
        p.accent = band;
        p.accentAlt = band.darker(0.35f);
        return p;
    }

    juce::Colour iris;
};
} // namespace owl::ui
