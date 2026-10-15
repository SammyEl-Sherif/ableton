#pragma once

#include <animalaudio/ui/Theme.h>

namespace duck::ui
{
inline constexpr float kThemeHue = 46.0f; // used by scripts/new_plugin.py to keep other plugins' hues apart

/** Duck palette: warm charcoal "pond at dusk" with rubber-duck yellow and bill orange. */
animalaudio::ui::Palette makePalette();

/** Complete Duck theme: palette, name/tagline and the rubber-duck motif. */
animalaudio::ui::Theme makeTheme();
} // namespace duck::ui
