#pragma once

#include "OwlProcessor.h"
#include "ui/EyeKnob.h"
#include "ui/ResponseView.h"

#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/PillButton.h>
#include <animalaudio/ui/ThemedKnob.h>

#include <array>
#include <memory>
#include <vector>

namespace owl
{
/**
 * Left: brand badge, OWL title + perched mascot, the big OUTPUT knob (an owl that turns its head) and FLAT.
 * Right: the response view (spectrum + curves + draggable bands) above one card per band with eye knobs.
 */
class OwlEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr int kWidth = 900;
    static constexpr int kHeight = 520;

    enum class Control
    {
        Frequency,
        Gain,
        Q
    };

    explicit OwlEditor(OwlProcessor& processor);

    void paint(juce::Graphics&) override;
    void resized() override;

    animalaudio::ui::BrandBadge& getBrandBadge() noexcept { return brand; }
    ui::ResponseView& getResponseView() noexcept { return response; }
    juce::Slider& getOutputKnob() noexcept { return outputKnob; }
    juce::Button& getFlatButton() noexcept { return flatButton; }
    /** nullptr for Q on the shelves. */
    juce::Slider* getBandKnob(int band, Control control);

private:
    struct BandCard
    {
        std::unique_ptr<ui::EyeKnob> frequency, gain, q;
        juce::Rectangle<int> bounds;
    };

    void attach(juce::Slider& slider, const juce::ParameterID& id, const juce::String& tooltip);
    void paintCard(juce::Graphics&, int band);
    void paintKnobLabel(juce::Graphics&, juce::Slider& knob, const juce::String& name, const juce::ParameterID& id,
                        juce::Colour valueColour);
    void flattenAllBands();

    OwlProcessor& owner;
    animalaudio::ui::Theme theme;
    animalaudio::ui::Palette palette;

    juce::TooltipWindow tooltipWindow{this, 500}; // plugins have no app-wide tooltip window
    animalaudio::ui::BrandBadge brand;
    animalaudio::ui::ThemedKnob outputKnob;
    animalaudio::ui::PillButton flatButton;
    ui::ResponseView response;
    std::array<BandCard, dsp::kNumBands> cards;
    std::vector<std::unique_ptr<juce::SliderParameterAttachment>> attachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OwlEditor)
};
} // namespace owl
