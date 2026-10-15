#include "DuckProcessor.h"

#include "DuckEditor.h"
#include "dsp/DuckPresets.h"

namespace duck
{
DuckProcessor::DuckProcessor()
    : GainShaperProcessor("Duck", "DuckState", getPresets())
{
}

juce::AudioProcessorEditor* DuckProcessor::createEditor()
{
    return new DuckEditor(*this);
}
} // namespace duck

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new duck::DuckProcessor();
}
