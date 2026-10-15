#pragma once

#include "Palette.h"

namespace animalaudio::ui
{
/**
 * Big rotary knob: a value arc around a cap. Subclasses draw the cap (e.g. a duck head) by
 * overriding drawCap(); the arc, interaction, and keyboard/wheel handling live here.
 * Vertical drag, mouse wheel, double-click resets to the default value.
 */
class RotaryKnob : public juce::Slider
{
public:
    explicit RotaryKnob(const Palette& palette);

    void paint(juce::Graphics& g) override;

protected:
    /** Draw the knob cap inside a circle of `radius` around `centre`, pointing at `angle` (radians,
        0 = 12 o'clock, clockwise positive). */
    virtual void drawCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle);

    Palette colours;
    static constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.2f;
    static constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.8f;
};
} // namespace animalaudio::ui
