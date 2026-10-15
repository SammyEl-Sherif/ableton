#include "DuckTheme.h"

#include "DuckMotif.h"

namespace duck::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff1a1610);
    p.panel = juce::Colour(0xff241f15);
    p.panelRaised = juce::Colour(0xff342d1f);
    p.outline = juce::Colour(0xff4a4029);
    p.text = juce::Colour(0xfff6ecd2);
    p.textDim = juce::Colour(0xffa89a78);
    p.accent = juce::Colour(0xffffcb1f);    // duck yellow
    p.accentAlt = juce::Colour(0xffff8a1f); // bill orange
    p.onAccent = juce::Colour(0xff1f1a0e);
    p.detail = juce::Colour(0xffff7a1a);  // bill
    p.ambient = juce::Colour(0xff4fb6d9); // pond water
    return p;
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "DUCK";
    theme.tagline = "VOLUME SHAPER";
    theme.motif = std::make_shared<DuckMotif>();
    return theme;
}
} // namespace duck::ui
