#pragma once

#include "PluginProcessor.h"

#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/RotaryKnob.h>

namespace __NAME_LOWER__
{
class __NAME__Editor : public juce::AudioProcessorEditor
{
public:
    explicit __NAME__Editor(__NAME__Processor&);

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    animalaudio::ui::Palette palette;
    animalaudio::ui::BrandBadge brand;
    animalaudio::ui::RotaryKnob knob;
    juce::SliderParameterAttachment attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(__NAME__Editor)
};
} // namespace __NAME_LOWER__
