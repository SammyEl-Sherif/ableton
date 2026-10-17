#include "OwlTheme.h"

#include "OwlMotif.h"

namespace owl::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff120f1c); // midnight
    p.panel = juce::Colour(0xff1b1729);
    p.panelRaised = juce::Colour(0xff2a2340);
    p.outline = juce::Colour(0xff3d3456);
    p.text = juce::Colour(0xfff1ecff);
    p.textDim = juce::Colour(0xffa097be);
    p.accent = juce::Colour(0xffb89cff);    // twilight lavender plumage
    p.accentAlt = juce::Colour(0xffff8bc6); // dusk pink
    p.onAccent = juce::Colour(0xff1b1430);
    p.detail = juce::Colour(0xffffb547);  // owl-eye amber
    p.ambient = juce::Colour(0xff8fd6ff); // moonlight
    return p;
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "OWL";
    theme.tagline = "THREE-BAND EQ";
    theme.motif = std::make_shared<OwlMotif>();
    return theme;
}

juce::Colour bandColour(const animalaudio::ui::Palette& palette, int band)
{
    switch (band)
    {
    case 0:
        return palette.detail;
    case 1:
        return palette.accent;
    default:
        return palette.accentAlt;
    }
}
} // namespace owl::ui
