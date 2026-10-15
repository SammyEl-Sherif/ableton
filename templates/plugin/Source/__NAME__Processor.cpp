#include "__NAME__Processor.h"

#include "__NAME__Editor.h"
#include "dsp/__NAME__Presets.h"

namespace __NAME_LOWER__
{
__NAME__Processor::__NAME__Processor()
    : GainShaperProcessor("__NAME__", "__NAME__State", getPresets())
{
}

juce::AudioProcessorEditor* __NAME__Processor::createEditor()
{
    return new __NAME__Editor(*this);
}
} // namespace __NAME_LOWER__

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new __NAME_LOWER__::__NAME__Processor();
}
