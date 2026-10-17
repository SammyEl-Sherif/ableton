#include <animalaudio/ui/ThemedPanel.h>

#include <animalaudio/core/BuildStamp.h>

namespace animalaudio::ui
{
void drawEditorBackground(juce::Graphics& g, juce::Rectangle<float> bounds, const Palette& palette)
{
    g.setGradientFill(juce::ColourGradient(palette.background.brighter(0.04f), 0.0f, 0.0f,
                                           palette.background.darker(0.2f), 0.0f, bounds.getBottom(), false));
    g.fillAll();
}

void drawThemedPanel(juce::Graphics& g, juce::Rectangle<float> panel, const Theme& theme, float ambientHeight)
{
    g.setColour(theme.palette.panel);
    g.fillRoundedRectangle(panel, 14.0f);

    juce::Graphics::ScopedSaveState state(g);
    juce::Path clip;
    clip.addRoundedRectangle(panel, 14.0f);
    g.reduceClipRegion(clip);
    theme.motif->drawAmbient(g, panel.withTop(panel.getBottom() - ambientHeight), 0.0, theme.palette);
    theme.motif->drawDecoration(g, panel, theme.palette);
}

void drawTitleBlock(juce::Graphics& g, juce::Point<float> origin, const Theme& theme)
{
    const auto& palette = theme.palette;
    const auto titleArea = juce::Rectangle<float>(origin.x, origin.y, 170.0f, 64.0f);
    float titleSize = 60.0f;
    const float titleWidth = juce::GlyphArrangement::getStringWidth(makeFont(titleSize), theme.name);
    if (titleWidth > 156.0f)
        titleSize = std::max(24.0f, titleSize * 156.0f / titleWidth);
    g.setColour(palette.accent);
    g.setFont(makeFont(titleSize));
    g.drawText(theme.name, titleArea, juce::Justification::centredLeft, false);

    g.setColour(palette.textDim);
    g.setFont(makeFont(11.0f, false));
    g.drawText(theme.tagline, juce::Rectangle<float>(origin.x + 2.0f, origin.y + 54.0f, 150.0f, 14.0f),
               juce::Justification::centredLeft, false);

    theme.motif->drawMascot(g, juce::Rectangle<float>(origin.x + 158.0f, origin.y, 62.0f, 62.0f), palette);
}

void drawBuildStamp(juce::Graphics& g, juce::Rectangle<float> area, const Palette& palette)
{
    g.setColour(palette.textDim.withAlpha(0.6f));
    g.setFont(makeFont(9.5f, false));
    g.drawText(core::buildStampShort(), area, juce::Justification::centredLeft, false);
}
} // namespace animalaudio::ui
