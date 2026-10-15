#pragma once

#include <animalaudio/ui/Palette.h>

namespace duck::ui
{
/** Duck theme: warm charcoal "pond at dusk" with rubber-duck yellow and bill orange. */
inline animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff1a1610);
    p.panel = juce::Colour(0xff241f15);
    p.panelRaised = juce::Colour(0xff342d1f);
    p.outline = juce::Colour(0xff4a4029);
    p.text = juce::Colour(0xfff6ecd2);
    p.textDim = juce::Colour(0xffa89a78);
    p.accent = juce::Colour(0xffffcb1f);    // duck yellow
    p.accentAlt = juce::Colour(0xffff8a1f); // bill orange
    p.onAccent = juce::Colour(0xff1f1a0e);
    return p;
}

inline juce::Colour waterColour()
{
    return juce::Colour(0xff4fb6d9);
}
inline juce::Colour billColour()
{
    return juce::Colour(0xffff7a1a);
}
} // namespace duck::ui
