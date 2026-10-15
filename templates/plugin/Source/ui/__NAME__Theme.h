#pragma once

#include <animalaudio/ui/Theme.h>

namespace __NAME_LOWER__::ui
{
inline constexpr float kThemeHue = __HUE__f; // read by scripts/new_plugin.py to keep plugins' hues apart

animalaudio::ui::Palette makePalette();

/** Palette + name/tagline + motif. Give the plugin its animal by deriving a Motif (see shared/ui/Theme.h). */
animalaudio::ui::Theme makeTheme();
} // namespace __NAME_LOWER__::ui
