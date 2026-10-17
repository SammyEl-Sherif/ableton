#pragma once

#include <animalaudio/ui/Theme.h>

namespace owl::ui
{
inline constexpr float kThemeHue = 258.0f; // read by scripts/new_plugin.py to keep plugins' hues apart

animalaudio::ui::Palette makePalette();

/** Palette + name/tagline + the owl motif. */
animalaudio::ui::Theme makeTheme();

/** Signature colour of each EQ band (dsp::Band order): amber lows, lavender mids, dusk-pink highs. */
juce::Colour bandColour(const animalaudio::ui::Palette& palette, int band);
} // namespace owl::ui
