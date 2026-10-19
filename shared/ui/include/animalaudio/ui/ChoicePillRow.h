#pragma once

#include "PillButton.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <memory>
#include <vector>

namespace animalaudio::ui
{
/**
 * Row of radio-style pills bound to a choice parameter (Duck's 1/8 | 1/4 | 1/2 | 1/1 selector).
 * Click selects exactly that choice; host automation and state restore move the highlight.
 * The pills share the row's width evenly (kPillWidth each at widthFor()); a plugin can supply its own
 * PillButton subclass through `makePill` and change the gap, e.g. for a segmented control.
 */
class ChoicePillRow : public juce::Component
{
public:
    static constexpr int kPillWidth = 62;
    static constexpr int kGap = 8;

    using PillFactory = std::function<std::unique_ptr<PillButton>(const juce::String& text)>;

    ChoicePillRow(juce::AudioParameterChoice& parameter, const Palette& palette, const juce::String& tooltip,
                  const PillFactory& makePill = {});

    /** Width needed for `numChoices` pills. */
    static int widthFor(int numChoices) noexcept { return numChoices * (kPillWidth + kGap) - kGap; }

    int getNumPills() const noexcept { return static_cast<int>(pills.size()); }
    juce::Button& getPill(int index) { return *pills[static_cast<std::size_t>(index)]; }
    void setGap(int pixels);

    void resized() override;

private:
    void show(int index);

    juce::AudioParameterChoice& choice;
    std::vector<std::unique_ptr<PillButton>> pills;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int gap = kGap;
};
} // namespace animalaudio::ui
