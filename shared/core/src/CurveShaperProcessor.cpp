#include <animalaudio/core/CurveShaperProcessor.h>

#include <animalaudio/core/TransportReader.h>

namespace animalaudio::core
{
using namespace dsp;

juce::AudioProcessorValueTreeState::ParameterLayout CurveShaperProcessor::createParameterLayout()
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

CurveShaperProcessor::CurveShaperProcessor(const juce::String& name, const juce::Identifier& stateType,
                                           const CurvePresetList& presets)
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, stateType, createParameterLayout())
    , pluginName(name)
    , curveState(apvts.state, presets)
{
    divisionParam = apvts.getRawParameterValue(ids::division.getParamID());
    mixParam = apvts.getRawParameterValue(ids::mix.getParamID());
    fallbackBpmParam = apvts.getRawParameterValue(ids::fallbackBpm.getParamID());
}

int CurveShaperProcessor::getDivisionIndex() const
{
    return juce::roundToInt(divisionParam->load());
}

bool CurveShaperProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CurveShaperProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    prepareEffect(sampleRate, samplesPerBlock, mixParam->load());
    tracker.prepare(sampleRate);
    tableVersion = 0;
}

void CurveShaperProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    if (curveState.fetchTable(localTable, tableVersion))
        curveTableChanged(localTable);

    const auto transport = readTransport(getPlayHead());
    const auto division = divisionFromIndex(juce::roundToInt(divisionParam->load()));
    const auto block = tracker.beginBlock(transport, division, fallbackBpmParam->load(), buffer.getNumSamples());
    syncSource.store(static_cast<int>(block.source), std::memory_order_relaxed);

    renderCycle(buffer, {block.startPhase, block.increment, mixParam->load()});
}

void CurveShaperProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void CurveShaperProcessor::setStateInformation(const void* data, int sizeInBytes)
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
} // namespace animalaudio::core
