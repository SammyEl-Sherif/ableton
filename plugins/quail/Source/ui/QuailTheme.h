#pragma once

#include "dsp/QuailSettings.h"

#include <animalaudio/ui/Theme.h>

namespace quail::ui
{
inline constexpr float kThemeHue = 184.0f; // used by scripts/new_plugin.py to keep other plugins' hues apart

/** Quail palette: near-black slate with an electric plume cyan, chestnut-flank copper and face-stripe cream. */
animalaudio::ui::Palette makePalette();

/** Complete Quail theme: palette, name/tagline and the quail motif. */
animalaudio::ui::Theme makeTheme();

/** Colour of a band's node and curve (eight hues, cycling). */
juce::Colour bandColour(int band);

/** Colour of the summed curve of one stereo placement (Stereo = the accent). */
juce::Colour placementColour(const animalaudio::ui::Palette& palette, dsp::Placement placement);

/** Analyser glow where the side chain and the signal fight over the same frequencies. */
juce::Colour collisionColour();

/** Gain reduction (cut) or boost applied by dynamics, as drawn on the range ring and the live curve. */
juce::Colour dynamicColour(bool boost);
} // namespace quail::ui
