#include "TurtleTimeTheme.h"

#include "TurtleTimeMotif.h"

namespace turtletime::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette palette;
    palette.background = juce::Colour(0xff101610);
    palette.panel = juce::Colour(0xff17211b);
    palette.panelRaised = juce::Colour(0xff243028);
    palette.outline = juce::Colour(0xff3d5648);
    palette.text = juce::Colour(0xffe7f3e4);
    palette.textDim = juce::Colour(0xff8eaa96);
    palette.accent = juce::Colour(0xff3fbe78);
    palette.accentAlt = juce::Colour(0xffe2a15a);
    palette.onAccent = juce::Colour(0xff102016);
    palette.detail = juce::Colour(0xffc6d9a8);
    palette.ambient = juce::Colour(0xff2f8f86);
    return palette;
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "TURTLE TIME";
    theme.tagline = "HALF-TIME";
    theme.motif = std::make_shared<TurtleTimeMotif>();
    return theme;
}
} // namespace turtletime::ui
