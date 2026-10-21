#pragma once

#include <animalaudio/ui/Theme.h>

namespace turtletime::ui
{
inline constexpr float kThemeHue = 147.0f; // read by scripts/new_plugin.py to keep plugins' hues apart

/** Swamp greens with a warm shell accent. */
animalaudio::ui::Palette makePalette();

animalaudio::ui::Theme makeTheme();
} // namespace turtletime::ui
