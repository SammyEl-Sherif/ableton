#include "IcePenguinTheme.h"

#include "IcePenguinMotif.h"

namespace icepenguin::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff0c1218);
    p.panel = juce::Colour(0xff121c26);
    p.panelRaised = juce::Colour(0xff1a2836);
    p.outline = juce::Colour(0xff2a4054);
    p.text = juce::Colour(0xffe8f4ff);
    p.textDim = juce::Colour(0xff8aa8c0);
    p.accent = juce::Colour(0xff6ec8ff);
    p.accentAlt = juce::Colour(0xffffa24a);
    p.onAccent = juce::Colour(0xff081018);
    p.detail = juce::Colour(0xff9ad4ff);
    p.ambient = juce::Colour(0xff3a8fd4);
    return p;
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "ICE PENGUIN";
    theme.tagline = "GLACIAL REVERB";
    theme.motif = std::make_shared<IcePenguinMotif>();
    return theme;
}
} // namespace icepenguin::ui
