#include "__NAME__Theme.h"

namespace __NAME_LOWER__::ui
{
animalaudio::ui::Palette makePalette()
{
    return animalaudio::ui::makePaletteFromHue(kThemeHue);
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "__NAME_UPPER__";
    theme.tagline = "VOLUME SHAPER";
    return theme;
}
} // namespace __NAME_LOWER__::ui
