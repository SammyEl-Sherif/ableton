#include "TurtleTimeTheme.h"

#include "TurtleTimeMotif.h"

namespace turtletime::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette palette;
    palette.background = juce::Colour(0xff0d1010);
    palette.panel = juce::Colour(0xff151a19);
    palette.panelRaised = juce::Colour(0xff1f2624);
    palette.outline = juce::Colour(0xff2e3835);
    palette.text = juce::Colour(0xffeef3f0);
    palette.textDim = juce::Colour(0xffa1aea8);
    palette.accent = juce::Colour(0xff45e09a);
    palette.accentAlt = juce::Colour(0xfff0b35e);
    palette.onAccent = juce::Colour(0xff04150c);
    palette.detail = juce::Colour(0xff6b7a74);
    palette.ambient = juce::Colour(0xff080b0a);
    return palette;
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "TURTLE TIME";
    theme.tagline = "TIME WARP";
    theme.motif = std::make_shared<TurtleTimeMotif>();
    return theme;
}
} // namespace turtletime::ui
