#include <animalaudio/ui/PillButton.h>

namespace animalaudio::ui
{
PillButton::PillButton(const juce::String& text, const Palette& palette)
    : juce::Button(text)
    , colours(palette)
{
    setClickingTogglesState(false);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void PillButton::paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    const bool on = getToggleState();

    juce::Colour fill = on ? colours.accent : colours.panelRaised;
    if (!on && isMouseOver)
        fill = fill.brighter(0.12f);
    if (isButtonDown)
        fill = fill.darker(0.1f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, bounds.getHeight() * 0.5f);

    if (on)
    {
        g.setColour(colours.accentAlt.withAlpha(0.8f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), bounds.getHeight() * 0.5f, 1.0f);
    }

    const auto textColour = on ? colours.onAccent : colours.text;
    if (glyph)
    {
        glyph(g, bounds.reduced(bounds.getHeight() * 0.3f), textColour);
    }
    else
    {
        g.setColour(textColour);
        g.setFont(makeFont(bounds.getHeight() * 0.5f));
        g.drawText(getButtonText(), bounds, juce::Justification::centred, false);
    }
}
} // namespace animalaudio::ui
