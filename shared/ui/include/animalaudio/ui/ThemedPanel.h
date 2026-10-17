#pragma once

#include "Theme.h"

namespace animalaudio::ui
{
/** Editor backdrop: a vertical gradient over the whole component, from the palette's background. */
void drawEditorBackground(juce::Graphics& g, juce::Rectangle<float> bounds, const Palette& palette);

/** Rounded side panel with the motif's ambient pattern along the bottom `ambientHeight` and its decorations. */
void drawThemedPanel(juce::Graphics& g, juce::Rectangle<float> panel, const Theme& theme, float ambientHeight = 70.0f);

/**
 * Title block: big NAME in the accent colour (60 px, shrunk to fit 156 px), tagline underneath and the mascot
 * (62x62) to the right. `origin` is the top-left of the name; the block is about 220x70.
 */
void drawTitleBlock(juce::Graphics& g, juce::Point<float> origin, const Theme& theme);

/** Subtle "version | sha | date" line so a user can tell which build the host loaded. */
void drawBuildStamp(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette);
} // namespace animalaudio::ui
