#include <animalaudio/ui/ChoicePillRow.h>

namespace animalaudio::ui
{
ChoicePillRow::ChoicePillRow(juce::AudioParameterChoice& parameter, const Palette& palette, const juce::String& tooltip,
                             const PillFactory& makePill)
    : choice(parameter)
{
    for (int i = 0; i < choice.choices.size(); ++i)
    {
        auto pill = makePill ? makePill(choice.choices[i]) : std::make_unique<PillButton>(choice.choices[i], palette);
        pill->setTooltip(tooltip);
        pill->onClick = [this, i]
        {
            show(i);
            attachment->setValueAsCompleteGesture(static_cast<float>(i));
        };
        addAndMakeVisible(*pill);
        pills.push_back(std::move(pill));
    }

    attachment = std::make_unique<juce::ParameterAttachment>(choice, [this](float) { show(choice.getIndex()); });
    attachment->sendInitialUpdate();
}

void ChoicePillRow::show(int index)
{
    index = juce::jlimit(0, getNumPills() - 1, index);
    for (int i = 0; i < getNumPills(); ++i)
        pills[static_cast<std::size_t>(i)]->setToggleState(i == index, juce::dontSendNotification);
}

void ChoicePillRow::setGap(int pixels)
{
    gap = pixels;
    resized();
}

void ChoicePillRow::resized()
{
    const int n = getNumPills();
    if (n == 0)
        return;
    const double pitch = static_cast<double>(getWidth() + gap) / n;
    for (int i = 0; i < n; ++i)
    {
        const int x = juce::roundToInt(pitch * i);
        const int right = juce::roundToInt(pitch * (i + 1)) - gap;
        pills[static_cast<std::size_t>(i)]->setBounds(x, 0, right - x, getHeight());
    }
}
} // namespace animalaudio::ui
