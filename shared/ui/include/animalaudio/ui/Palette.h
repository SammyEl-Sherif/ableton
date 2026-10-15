#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace animalaudio::ui
{
inline constexpr const char* kCompanyName = "Animal Audio";
inline constexpr const char* kCompanyNameUpper = "ANIMAL AUDIO";

/** Colour set every Animal Audio widget draws from. Each plugin supplies its own. */
struct Palette
{
    juce::Colour background;
    juce::Colour panel;
    juce::Colour panelRaised;
    juce::Colour outline;
    juce::Colour text;
    juce::Colour textDim;
    juce::Colour accent;    // primary highlight
    juce::Colour accentAlt; // secondary highlight / gradient end
    juce::Colour onAccent;  // text drawn on top of `accent`
};

/** Bold UI font; falls back to the platform default sans-serif. */
inline juce::Font makeFont(float height, bool bold = true)
{
    return juce::Font(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace animalaudio::ui
