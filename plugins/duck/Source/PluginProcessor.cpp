#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace duck
{
using namespace animalaudio::dsp;

juce::AudioProcessorValueTreeState::ParameterLayout DuckProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray divisions;
    for (int i = 0; i < kNumDivisions; ++i)
        divisions.add(divisionLabel(static_cast<Division>(i)));

    layout.add(std::make_unique<juce::AudioParameterChoice>(ids::division, "Division", divisions,
                                                            static_cast<int>(Division::Quarter)));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ids::mix, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() * 0.01f; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ids::fallbackBpm, "Fallback BPM", juce::NormalisableRange<float>(40.0f, 240.0f, 0.1f), 120.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float v, int)
                                                                          { return juce::String(v, 1); })));

    return layout;
}

DuckProcessor::DuckProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "DuckState", createParameterLayout())
    , curveState(apvts.state)
{
    divisionParam = apvts.getRawParameterValue(ids::division.getParamID());
    mixParam = apvts.getRawParameterValue(ids::mix.getParamID());
    fallbackBpmParam = apvts.getRawParameterValue(ids::fallbackBpm.getParamID());
}

bool DuckProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void DuckProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    engine.reset(mixParam->load());
    engine.setMix(mixParam->load());
    tracker.prepare(sampleRate);
    tableVersion = 0;
}

void DuckProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    if (curveState.fetchTable(localTable, tableVersion))
        engine.setTable(localTable);

    TransportInfo transport;
    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            transport.playing = position->getIsPlaying();
            if (const auto bpm = position->getBpm())
            {
                transport.hasBpm = true;
                transport.bpm = *bpm;
            }
            if (const auto ppq = position->getPpqPosition())
            {
                transport.hasPpq = true;
                transport.ppq = *ppq;
            }
        }
    }

    const auto division = divisionFromIndex(static_cast<int>(divisionParam->load()));
    const auto block = tracker.beginBlock(transport, division, fallbackBpmParam->load(), buffer.getNumSamples());
    syncSource.store(static_cast<int>(block.source), std::memory_order_relaxed);

    engine.setMix(mixParam->load());
    engine.process(buffer.getArrayOfWritePointers(), juce::jmin(buffer.getNumChannels(), getTotalNumOutputChannels()),
                   buffer.getNumSamples(), block.startPhase, block.increment);
}

juce::AudioProcessorEditor* DuckProcessor::createEditor()
{
    return new DuckEditor(*this);
}

void DuckProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void DuckProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            curveState.ensureExists();
            curveState.sendSynchronousChangeMessage();
        }
    }
}
} // namespace duck

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new duck::DuckProcessor();
}
