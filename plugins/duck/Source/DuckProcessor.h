#pragma once

#include <animalaudio/core/CurveShaperProcessor.h>

namespace duck
{
/** Duck = the stock curve-driven gain shaper plus the duck presets, theme and editor. */
class DuckProcessor : public animalaudio::core::GainShaperProcessor
{
public:
    DuckProcessor();

    juce::AudioProcessorEditor* createEditor() override;
};
} // namespace duck
