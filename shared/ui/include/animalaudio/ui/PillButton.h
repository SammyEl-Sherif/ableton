#pragma once

#include "Palette.h"

namespace animalaudio::ui
{
/** Rounded toggle/radio button. Lit with the accent colour when on. */
class PillButton : public juce::Button
{
public:
    PillButton(const juce::String& text, const Palette& palette);

    void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;

    /** Optional custom glyph drawn instead of text (e.g. arrows). Receives bounds + colour. */
    std::function<void(juce::Graphics&, juce::Rectangle<float>, juce::Colour)> glyph;

private:
    Palette colours;
};
} // namespace animalaudio::ui
