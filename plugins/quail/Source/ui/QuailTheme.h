#pragma once

#include "dsp/QuailSettings.h"

#include <animalaudio/ui/Theme.h>

namespace quail::ui
{
inline constexpr float kThemeHue = 228.0f; // used by scripts/new_plugin.py to keep other plugins' hues apart

/** Quail palette: neutral graphite with one periwinkle accent (the slate-blue breast), lavender and cream. */
animalaudio::ui::Palette makePalette();

/** Complete Quail theme: palette, name/tagline and the quail motif. */
animalaudio::ui::Theme makeTheme();

/** Colour of a band's node and curve (eight matched pastels, cycling). */
juce::Colour bandColour(int band);

/** Colour of the summed curve of one stereo placement (Stereo = the text colour). */
juce::Colour placementColour(const animalaudio::ui::Palette& palette, dsp::Placement placement);

/** The side-chain signal: its spectrum, meter and status light (the quail's chestnut flank). */
juce::Colour sidechainColour();

/** Analyser glow where the side chain and the signal fight over the same frequencies. */
juce::Colour collisionColour();

/** Attention without alarm: bypass engaged, a side-chain band with no side-chain input. */
juce::Colour warningColour();

/** Small upper-case labels: regular weight, tracked out. */
juce::Font labelFont(float height);
/** Values and buttons: bold, slightly tracked. */
juce::Font valueFont(float height);
/** The QUAIL wordmark. */
juce::Font titleFont(float height);
} // namespace quail::ui
