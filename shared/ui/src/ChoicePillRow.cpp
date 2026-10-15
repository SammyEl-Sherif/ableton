#include <animalaudio/ui/ChoicePillRow.h>

namespace animalaudio::ui
{
ChoicePillRow::ChoicePillRow(juce::AudioParameterChoice& parameter, const Palette& palette, const juce::String& tooltip)
    : choice(parameter)
{
    for (int i = 0; i < choice.choices.size(); ++i)
    {
        auto pill = std::make_unique<PillButton>(choice.choices[i], palette);
        pill->setRadioGroupId(1);
        pill->setTooltip(tooltip);
        pill->onClick = [this, i] { attachment->setValueAsCompleteGesture(static_cast<float>(i)); };
        addAndMakeVisible(*pill);
        pills.push_back(std::move(pill));
    }

    // Use the value the callback is given: listeners are notified in reverse registration order, so
    // reading the APVTS raw value here can still be the *previous* selection.
    attachment = std::make_unique<juce::ParameterAttachment>(choice, [this](float denormalisedIndex)
                                                             { show(juce::roundToInt(denormalisedIndex)); });
    show(choice.getIndex());
}

void ChoicePillRow::show(int index)
{
    for (int i = 0; i < getNumPills(); ++i)
        pills[static_cast<std::size_t>(i)]->setToggleState(i == index, juce::dontSendNotification);
}

void ChoicePillRow::resized()
{
    auto row = getLocalBounds();
    for (auto& pill : pills)
    {
        pill->setBounds(row.removeFromLeft(kPillWidth));
        row.removeFromLeft(kGap);
    }
}
} // namespace animalaudio::ui
