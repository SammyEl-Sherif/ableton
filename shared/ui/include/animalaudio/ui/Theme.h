#pragma once

#include "Palette.h"

#include <memory>

namespace animalaudio::ui
{
/**
 * A plugin's animal personality: how its mascot, knob cap, background motif and decorations are drawn.
 * Shared widgets call these hooks; the colours come from the Palette handed in, so a motif never
 * hard-codes colours and a palette can be swapped without touching drawing code.
 *
 * Every hook has a neutral default (paw print, plain knob cap, soft ripples), so a brand-new plugin
 * looks right before anyone has drawn an animal.
 */
class Motif
{
public:
    virtual ~Motif() = default;

    /** Mascot drawn next to the title (area ~62x62) and, smaller, riding the curve at the playhead. */
    virtual void drawMascot(juce::Graphics&, juce::Rectangle<float> area, const Palette&) const;

    /** Cap of the big knob. `angle` is the value pointer in radians (0 = 12 o'clock, clockwise). */
    virtual void drawKnobCap(juce::Graphics&, juce::Point<float> centre, float radius, float angle,
                             const Palette&) const;

    /** Animated background pattern inside panels (curve editor, bottom of the left panel). */
    virtual void drawAmbient(juce::Graphics&, juce::Rectangle<float> area, double timeSeconds, const Palette&) const;

    /** Very faint decorations scattered over the left panel. */
    virtual void drawDecoration(juce::Graphics&, juce::Rectangle<float> panelArea, const Palette&) const;
};

struct Theme
{
    Palette palette;
    juce::String name;    // shown big, upper case, e.g. "DUCK"
    juce::String tagline; // small caps line under the name, e.g. "VOLUME SHAPER"
    std::shared_ptr<const Motif> motif = std::make_shared<Motif>();
};
} // namespace animalaudio::ui
