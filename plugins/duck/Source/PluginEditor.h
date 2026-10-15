#pragma once

#include "PluginProcessor.h"
#include "ui/CurveEditor.h"
#include "ui/DuckKnob.h"

#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/PillButton.h>

#include <array>
#include <memory>

namespace duck
{
class DuckEditor : public juce::AudioProcessorEditor, private juce::Timer, private juce::ChangeListener
{
public:
    explicit DuckEditor(DuckProcessor&);
    ~DuckEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    static constexpr int kWidth = 820;
    static constexpr int kHeight = 470;

    ui::CurveEditor& getCurveEditor() noexcept { return curveEditor; }

private:
    class PresetButton;

    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void stepPreset(int delta);
    void refreshToggles();

    DuckProcessor& owner;
    animalaudio::ui::Palette palette;

    animalaudio::ui::BrandBadge brand;
    ui::DuckKnob mixKnob;
    juce::Slider bpmSlider;
    ui::CurveEditor curveEditor;

    std::array<std::unique_ptr<animalaudio::ui::PillButton>, animalaudio::dsp::kNumDivisions> divisionButtons;
    std::unique_ptr<animalaudio::ui::PillButton> prevButton, nextButton, flipButton, invertButton;
    std::array<std::unique_ptr<PresetButton>, kNumPresets> presetButtons;

    std::unique_ptr<juce::ParameterAttachment> divisionAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> mixAttachment, bpmAttachment;

    juce::Rectangle<int> divisionRow, statusArea, presetNameArea;
    animalaudio::dsp::SyncSource shownSource = animalaudio::dsp::SyncSource::FreeRun;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckEditor)
};
} // namespace duck
