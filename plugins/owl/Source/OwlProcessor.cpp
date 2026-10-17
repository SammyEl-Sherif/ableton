#include "OwlProcessor.h"

#include "OwlEditor.h"

namespace owl
{
namespace
{
juce::NormalisableRange<float> frequencyRange(float low, float high, float centre)
{
    juce::NormalisableRange<float> range(low, high, 0.01f);
    range.setSkewForCentre(centre);
    return range;
}

juce::String formatHz(float hz, int)
{
    return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                         : juce::String(juce::roundToInt(hz)) + " Hz";
}

float parseHz(const juce::String& text)
{
    const float value = text.getFloatValue();
    return text.containsIgnoreCase("k") ? value * 1000.0f : value;
}

juce::String formatDb(float db, int)
{
    return (db > 0.04f ? "+" : "") + juce::String(db, 1) + " dB";
}

std::unique_ptr<juce::AudioParameterFloat> frequencyParameter(const juce::ParameterID& id, const juce::String& name,
                                                              float low, float high, float centre, float defaultHz)
{
    return std::make_unique<juce::AudioParameterFloat>(id, name, frequencyRange(low, high, centre), defaultHz,
                                                       juce::AudioParameterFloatAttributes()
                                                           .withLabel("Hz")
                                                           .withStringFromValueFunction(formatHz)
                                                           .withValueFromStringFunction(parseHz));
}

std::unique_ptr<juce::AudioParameterFloat> gainParameter(const juce::ParameterID& id, const juce::String& name)
{
    // JUCE's interval snap (start + n * interval) lands 0 dB on ~2.7e-7; exact tenths keep a flat EQ bit-exact
    juce::NormalisableRange<float> range(
        -kMaxGainDb, kMaxGainDb, [](float start, float end, float t) { return start + t * (end - start); },
        [](float start, float end, float v) { return (v - start) / (end - start); },
        [](float start, float end, float v) { return juce::jlimit(start, end, std::round(v * 10.0f) / 10.0f); });
    range.interval = 0.1f;
    return std::make_unique<juce::AudioParameterFloat>(
        id, name, range, 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(formatDb)
            .withValueFromStringFunction([](const juce::String& s) { return s.getFloatValue(); }));
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout OwlProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(frequencyParameter(ids::lowFreq, "Low Freq", 20.0f, 1000.0f, 150.0f, 120.0f));
    layout.add(gainParameter(ids::lowGain, "Low Gain"));
    layout.add(frequencyParameter(ids::midFreq, "Mid Freq", 100.0f, 12000.0f, 1000.0f, 1000.0f));
    layout.add(gainParameter(ids::midGain, "Mid Gain"));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        ids::midQ, "Mid Q",
        []
        {
            juce::NormalisableRange<float> range(0.3f, 8.0f, 0.01f);
            range.setSkewForCentre(1.2f);
            return range;
        }(),
        1.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction([](float q, int)
                                                                          { return juce::String(q, 2); })));
    layout.add(frequencyParameter(ids::highFreq, "High Freq", 1500.0f, 20000.0f, 6000.0f, 8000.0f));
    layout.add(gainParameter(ids::highGain, "High Gain"));
    layout.add(gainParameter(ids::output, "Output"));
    return layout;
}

OwlProcessor::OwlProcessor()
    : PluginProcessor("Owl", "OwlState", createParameterLayout())
{
    lowFreq = apvts.getRawParameterValue(ids::lowFreq.getParamID());
    lowGain = apvts.getRawParameterValue(ids::lowGain.getParamID());
    midFreq = apvts.getRawParameterValue(ids::midFreq.getParamID());
    midGain = apvts.getRawParameterValue(ids::midGain.getParamID());
    midQ = apvts.getRawParameterValue(ids::midQ.getParamID());
    highFreq = apvts.getRawParameterValue(ids::highFreq.getParamID());
    highGain = apvts.getRawParameterValue(ids::highGain.getParamID());
    outputGain = apvts.getRawParameterValue(ids::output.getParamID());

    // hosts may process before prepareToPlay
    eq.setSettings(getSettings());
    eq.prepare(44100.0);
}

dsp::Settings OwlProcessor::getSettings() const noexcept
{
    dsp::Settings s;
    s.lowFreq = lowFreq->load();
    s.lowGain = lowGain->load();
    s.midFreq = midFreq->load();
    s.midGain = midGain->load();
    s.midQ = midQ->load();
    s.highFreq = highFreq->load();
    s.highGain = highGain->load();
    s.output = outputGain->load();
    return s;
}

double OwlProcessor::getDisplaySampleRate() const noexcept
{
    const double rate = preparedSampleRate.load(std::memory_order_relaxed);
    return rate > 0.0 ? rate : 48000.0;
}

void OwlProcessor::prepareToPlay(double sampleRate, int)
{
    eq.setSettings(getSettings());
    eq.prepare(sampleRate);
    preparedSampleRate.store(sampleRate, std::memory_order_relaxed);
}

void OwlProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin(buffer.getNumChannels(), getTotalNumInputChannels());
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, buffer.getNumSamples());

    eq.setSettings(getSettings());
    eq.process(buffer.getArrayOfWritePointers(), numChannels, buffer.getNumSamples());
    analyserFifo.pushMixdown(buffer.getArrayOfReadPointers(), numChannels, buffer.getNumSamples());
}

juce::AudioProcessorEditor* OwlProcessor::createEditor()
{
    return new OwlEditor(*this);
}
} // namespace owl

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new owl::OwlProcessor();
}
