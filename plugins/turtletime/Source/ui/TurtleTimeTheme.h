#pragma once

#include <animalaudio/ui/Theme.h>

namespace turtletime::ui
{
inline constexpr float kThemeHue = 147.0f; // read by scripts/new_plugin.py to keep plugins' hues apart

/** Graphite faceplate, mint-green accent; amber marks only the read head (the shell). `ambient` is the display glass. */
animalaudio::ui::Palette makePalette();

animalaudio::ui::Theme makeTheme();
} // namespace turtletime::ui
