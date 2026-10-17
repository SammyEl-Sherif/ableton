#include <animalaudio/core/PluginProcessor.h>

namespace animalaudio::core
{
juce::AudioProcessor::BusesProperties PluginProcessor::makeBuses(SidechainInput sidechain)
{
    auto buses = BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    if (sidechain == SidechainInput::stereo)
        buses = buses.withInput("Sidechain", juce::AudioChannelSet::stereo(), true);
    return buses;
}

PluginProcessor::PluginProcessor(const juce::String& name, const juce::Identifier& stateType,
                                 juce::AudioProcessorValueTreeState::ParameterLayout layout, SidechainInput sidechain)
    : juce::AudioProcessor(makeBuses(sidechain))
    , apvts(*this, nullptr, stateType, std::move(layout))
    , pluginName(name)
{
}

bool PluginProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (out != layouts.getMainInputChannelSet())
        return false;
    for (int bus = 1; bus < layouts.inputBuses.size(); ++bus)
    {
        const auto& sidechain = layouts.getChannelSet(true, bus);
        if (!sidechain.isDisabled() && sidechain != juce::AudioChannelSet::mono() &&
            sidechain != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void PluginProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void PluginProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            stateRestored();
        }
    }
}
} // namespace animalaudio::core
