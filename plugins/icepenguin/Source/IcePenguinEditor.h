#pragma once

#include "IcePenguinProcessor.h"
#include "dsp/IcePenguinPresets.h"
#include "ui/IcePenguinTheme.h"

#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/PillButton.h>
#include <animalaudio/ui/ThemedKnob.h>

namespace icepenguin
{
class IcePenguinEditor : public juce::AudioProcessorEditor,
                           public juce::AudioProcessorValueTreeState::Listener,
                           private juce::Timer
{
public:
    static constexpr int kWidth = 860;
    static constexpr int kHeight = 520;

    explicit IcePenguinEditor(IcePenguinProcessor& owner);
    ~IcePenguinEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void refreshPresetHighlight();

    IcePenguinProcessor& owner;
    animalaudio::ui::Theme theme;
    animalaudio::ui::BrandBadge brand;
    animalaudio::ui::ThemedKnob mixKnob;
    animalaudio::ui::ThemedKnob decayKnob;
    animalaudio::ui::ThemedKnob toneKnob;
    animalaudio::ui::ThemedKnob predelayKnob;
    animalaudio::ui::ThemedKnob sizeKnob;
    juce::Label mixLabel;
    juce::Label decayLabel;
    juce::Label toneLabel;
    juce::Label predelayLabel;
    juce::Label sizeLabel;
    juce::Label presetHeader;
    std::array<std::unique_ptr<animalaudio::ui::PillButton>, kNumReverbPresets> presetButtons;

    std::unique_ptr<juce::SliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> decayAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> toneAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> predelayAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> sizeAttachment;

    float ambientPhase = 0.0f;
    juce::TooltipWindow tooltipWindow{this, 500};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IcePenguinEditor)
};
} // namespace icepenguin
