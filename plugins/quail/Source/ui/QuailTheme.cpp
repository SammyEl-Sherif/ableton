#include "QuailTheme.h"

#include "QuailMotif.h"

#include <array>

namespace quail::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff070b10);
    p.panel = juce::Colour(0xff0d131b);
    p.panelRaised = juce::Colour(0xff16202c);
    p.outline = juce::Colour(0xff223040);
    p.text = juce::Colour(0xffe8f1f8);
    p.textDim = juce::Colour(0xff7d8fa3);
    p.accent = juce::Colour(0xff3ee6f0);    // electric plume cyan
    p.accentAlt = juce::Colour(0xffff9257); // chestnut flank
    p.onAccent = juce::Colour(0xff031418);
    p.detail = juce::Colour(0xfff2d9a6);  // face-stripe cream
    p.ambient = juce::Colour(0xff5b7fa8); // slate-blue plumage
    return p;
}

animalaudio::ui::Theme makeTheme()
{
    animalaudio::ui::Theme theme;
    theme.palette = makePalette();
    theme.name = "QUAIL";
    theme.tagline = "DYNAMIC EQ";
    theme.motif = std::make_shared<QuailMotif>();
    return theme;
}

juce::Colour bandColour(int band)
{
    static const std::array<juce::Colour, 8> colours{
        juce::Colour(0xff3ee6f0), juce::Colour(0xff9c8cff), juce::Colour(0xffff6fb5), juce::Colour(0xffffc24a),
        juce::Colour(0xff9bea5b), juce::Colour(0xff5ab8ff), juce::Colour(0xffff8a5c), juce::Colour(0xff4fe3a8)};
    return colours[static_cast<std::size_t>((band % 8 + 8) % 8)];
}

juce::Colour placementColour(const animalaudio::ui::Palette& palette, dsp::Placement placement)
{
    switch (placement)
    {
    case dsp::Placement::Left:
        return juce::Colour(0xfff4f7fa);
    case dsp::Placement::Right:
        return juce::Colour(0xffff6b6b);
    case dsp::Placement::Mid:
        return juce::Colour(0xff6be38a);
    case dsp::Placement::Side:
        return juce::Colour(0xff6b8bff);
    case dsp::Placement::Stereo:
        break;
    }
    return palette.accent;
}

juce::Colour collisionColour()
{
    return juce::Colour(0xffff4d6a);
}

juce::Colour dynamicColour(bool boost)
{
    return boost ? juce::Colour(0xff9bea5b) : juce::Colour(0xffffc24a);
}
} // namespace quail::ui
