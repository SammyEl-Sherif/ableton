#pragma once

#include <animalaudio/core/CurveShaperProcessor.h>

namespace __NAME_LOWER__
{
/** Starts as the stock curve-driven gain shaper. Replace the base class / DSP hooks to make it your own. */
class __NAME__Processor : public animalaudio::core::GainShaperProcessor
{
public:
    __NAME__Processor();

    juce::AudioProcessorEditor* createEditor() override;
};
} // namespace __NAME_LOWER__
