#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace __NAME_LOWER__
{
juce::AudioProcessorValueTreeState::ParameterLayout __NAME__Processor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(juce::ParameterID{"gain", 1}, "Gain",
                                                           juce::NormalisableRange<float>(-24.0f, 12.0f, 0.1f), 0.0f));
    return layout;
}

__NAME__Processor::__NAME__Processor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "__NAME__State", createParameterLayout())
{
    gainParam = apvts.getRawParameterValue("gain");
}

bool __NAME__Processor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void __NAME__Processor::prepareToPlay(double sampleRate, int)
{
    gain.prepare(sampleRate);
}

void __NAME__Processor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Audio thread: no allocation, no locks, no I/O. See AGENTS.md.
    juce::ScopedNoDenormals noDenormals;
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    gain.setDecibels(gainParam->load());
    gain.process(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
}

juce::AudioProcessorEditor* __NAME__Processor::createEditor()
{
    return new __NAME__Editor(*this);
}

void __NAME__Processor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void __NAME__Processor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}
} // namespace __NAME_LOWER__

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new __NAME_LOWER__::__NAME__Processor();
}
