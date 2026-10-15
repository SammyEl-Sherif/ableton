#pragma once

#include <animalaudio/dsp/ShapeCurve.h>

#include <juce_gui_basics/juce_gui_basics.h>

namespace duck::ui
{
/** Side-view rubber duck facing right, filling `area` (aspect ~1:1). */
void drawRubberDuck(juce::Graphics& g, juce::Rectangle<float> area);

/** A single stylised feather, tip pointing up-right, used as a decorative motif. */
void drawFeather(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour);

/** Layered sine ripples across `area` (water motif). `time` animates them. */
void drawWater(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour, double time);

/** Mini plot of a curve, used on preset buttons. */
juce::Path makeCurvePath(const animalaudio::dsp::ShapeCurve& curve, juce::Rectangle<float> area, int steps = 48);
} // namespace duck::ui
