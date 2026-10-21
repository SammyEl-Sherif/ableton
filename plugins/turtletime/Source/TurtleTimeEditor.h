#pragma once

#include "TurtleTimeProcessor.h"
#include "ui/TurtleTimeTheme.h"

#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/PillButton.h>
#include <animalaudio/ui/ThemedKnob.h>

#include <array>
#include <memory>

namespace turtletime
{
class TurtleTimeEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kWidth = 900;
    static constexpr int kHeight = 540;

    explicit TurtleTimeEditor(TurtleTimeProcessor& owner);
    ~TurtleTimeEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct Layout
    {
        juce::Rectangle<int> left;
        juce::Rectangle<int> right;
        juce::Rectangle<int> lane;
        juce::Rectangle<int> grid;
    };

    Layout layout() const;
    void timerCallback() override;
    void refreshPatternHighlight();
    void refreshReadouts();
    void paintLane(juce::Graphics& g, juce::Rectangle<float> area) const;

    TurtleTimeProcessor& owner;
    animalaudio::ui::Theme theme;
    animalaudio::ui::BrandBadge brand;
    animalaudio::ui::ThemedKnob mixKnob;
    animalaudio::ui::ThemedKnob bpmKnob;
    juce::Label mixLabel;
    juce::Label bpmLabel;
    juce::Label patternHeader;
    juce::Label syncLabel;
    juce::Label blurbLabel;
    std::array<std::unique_ptr<animalaudio::ui::PillButton>, dsp::kNumPatterns> patternButtons;

    std::unique_ptr<juce::SliderParameterAttachment> mixAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> bpmAttachment;

    double ambientSeconds = 0.0;
    juce::TooltipWindow tooltipWindow{this, 500};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TurtleTimeEditor)
};
} // namespace turtletime
