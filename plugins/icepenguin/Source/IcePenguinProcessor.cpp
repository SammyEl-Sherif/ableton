#include "IcePenguinProcessor.h"

#include "IcePenguinEditor.h"
#include "dsp/IcePenguinPresets.h"

namespace icepenguin
{
namespace
{
juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray presetNames;
    for (const auto& preset : reverbPresets())
        presetNames.add(juce::String(preset.name.data(), preset.name.size()));

    layout.add(std::make_unique<juce::AudioParameterChoice>(ids::preset, "Preset", presetNames, 0));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ids::mix, "Mix", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.38f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(juce::roundToInt(v * 100.0f)) + " %"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue() * 0.01f; })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(ids::decay, "Decay",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.55f,
                                                           juce::AudioParameterFloatAttributes().withLabel("trim")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(ids::tone, "Tone",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.48f,
                                                           juce::AudioParameterFloatAttributes().withLabel("damp")));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ids::predelay, "Predelay", juce::NormalisableRange<float>(0.0f, 80.0f, 0.1f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float v, int) { return juce::String(v, 1) + " ms"; })
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); })));

    layout.add(std::make_unique<juce::AudioParameterFloat>(ids::size, "Size",
                                                           juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f,
                                                           juce::AudioParameterFloatAttributes().withLabel("room")));

    return layout;
}
} // namespace

IcePenguinProcessor::IcePenguinProcessor()
    : juce::AudioProcessor(BusesProperties()
                               .withInput("Input", juce::AudioChannelSet::stereo(), true)
                               .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    , apvts(*this, nullptr, "IcePenguinState", createParameterLayout())
{
    presetParam = apvts.getRawParameterValue(ids::preset.getParamID());
    mixParam = apvts.getRawParameterValue(ids::mix.getParamID());
    decayParam = apvts.getRawParameterValue(ids::decay.getParamID());
    toneParam = apvts.getRawParameterValue(ids::tone.getParamID());
    predelayParam = apvts.getRawParameterValue(ids::predelay.getParamID());
    sizeParam = apvts.getRawParameterValue(ids::size.getParamID());
}

bool IcePenguinProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void IcePenguinProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    reverb.prepare(sampleRate, samplesPerBlock);
    pushParametersToEngine();
}

double IcePenguinProcessor::getTailLengthSeconds() const
{
    const float decayTrim = decayParam != nullptr ? decayParam->load() : 0.55f;
    return 2.0 + decayTrim * 18.0;
}

int IcePenguinProcessor::getCurrentProgram()
{
    return juce::roundToInt(presetParam->load());
}

void IcePenguinProcessor::setCurrentProgram(int index)
{
    if (auto* param = apvts.getParameter(ids::preset.getParamID()))
        param->setValueNotifyingHost(
            param->convertTo0to1(static_cast<float>(std::clamp(index, 0, kNumReverbPresets - 1))));
}

const juce::String IcePenguinProcessor::getProgramName(int index)
{
    if (index < 0 || index >= kNumReverbPresets)
        return {};
    const auto name = reverbPresets()[static_cast<std::size_t>(index)].name;
    return juce::String(name.data(), name.size());
}

void IcePenguinProcessor::pushParametersToEngine() noexcept
{
    const int presetIndex = juce::roundToInt(presetParam->load());
    const auto settings =
        settingsForPreset(presetIndex, decayParam->load(), toneParam->load(), predelayParam->load(), sizeParam->load());
    reverb.setSettings(settings);
    reverb.setMix(mixParam->load());
}

void IcePenguinProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    pushParametersToEngine();

    auto* left = buffer.getWritePointer(0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : left;
    reverb.process(left, right, buffer.getNumSamples());
}

void IcePenguinProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void IcePenguinProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* IcePenguinProcessor::createEditor()
{
    return new IcePenguinEditor(*this);
}

} // namespace icepenguin

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new icepenguin::IcePenguinProcessor();
}
