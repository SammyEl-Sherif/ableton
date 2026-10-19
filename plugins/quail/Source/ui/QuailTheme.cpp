#include "QuailTheme.h"

#include "QuailMotif.h"

#include <array>

namespace quail::ui
{
animalaudio::ui::Palette makePalette()
{
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff0a0b0e);
    p.panel = juce::Colour(0xff111318);
    p.panelRaised = juce::Colour(0xff191c22);
    p.outline = juce::Colour(0xff262a32);
    p.text = juce::Colour(0xffedeff3);
    p.textDim = juce::Colour(0xff8a919e);
    p.accent = juce::Colour(0xff8ea4ff);    // periwinkle: the slate-blue breast, lit
    p.accentAlt = juce::Colour(0xffb6a7ff); // lavender, the accent's gradient partner
    p.onAccent = juce::Colour(0xff0b0e1c);
    p.detail = juce::Colour(0xfff2e6cf);  // face-stripe cream
    p.ambient = juce::Colour(0xff55648a); // slate plumage, only as faint scales
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
        juce::Colour(0xff8ea4ff), juce::Colour(0xff67d4c4), juce::Colour(0xfff5a3c7), juce::Colour(0xffebc872),
        juce::Colour(0xff9ad88a), juce::Colour(0xff74c3f0), juce::Colour(0xffc5a6f7), juce::Colour(0xfff59e92)};
    return colours[static_cast<std::size_t>((band % 8 + 8) % 8)];
}

juce::Colour placementColour(const animalaudio::ui::Palette& palette, dsp::Placement placement)
{
    switch (placement)
    {
    case dsp::Placement::Left:
        return juce::Colour(0xff74c3f0);
    case dsp::Placement::Right:
        return juce::Colour(0xfff59e92);
    case dsp::Placement::Mid:
        return juce::Colour(0xff9ad88a);
    case dsp::Placement::Side:
        return juce::Colour(0xffc5a6f7);
    case dsp::Placement::Stereo:
        break;
    }
    return palette.text;
}

juce::Colour sidechainColour()
{
    return juce::Colour(0xffffa96b);
}

juce::Colour collisionColour()
{
    return juce::Colour(0xffff6b85);
}

juce::Colour warningColour()
{
    return juce::Colour(0xfff5c46b);
}

juce::Font labelFont(float height)
{
    return juce::Font(juce::FontOptions(height, juce::Font::plain).withKerningFactor(0.09f));
}

juce::Font valueFont(float height)
{
    return juce::Font(juce::FontOptions(height, juce::Font::bold).withKerningFactor(0.02f));
}

juce::Font titleFont(float height)
{
    return juce::Font(juce::FontOptions(height, juce::Font::bold).withKerningFactor(0.2f));
}
} // namespace quail::ui
