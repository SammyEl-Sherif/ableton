#pragma once

#include "PillButton.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

namespace animalaudio::ui
{
/**
 * Row of radio-style pills bound to a choice parameter (Duck's 1/8 | 1/4 | 1/2 | 1/1 selector).
 * Click selects exactly that choice; host automation and state restore move the highlight.
 */
class ChoicePillRow : public juce::Component
{
public:
    static constexpr int kPillWidth = 62;
    static constexpr int kGap = 8;

    ChoicePillRow(juce::AudioParameterChoice& parameter, const Palette& palette, const juce::String& tooltip);

    /** Width needed for `numChoices` pills. */
    static int widthFor(int numChoices) noexcept { return numChoices * (kPillWidth + kGap) - kGap; }

    int getNumPills() const noexcept { return static_cast<int>(pills.size()); }
    juce::Button& getPill(int index) { return *pills[static_cast<std::size_t>(index)]; }

    void resized() override;

private:
    void show(int index);

    juce::AudioParameterChoice& choice;
    std::vector<std::unique_ptr<PillButton>> pills;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};
} // namespace animalaudio::ui
