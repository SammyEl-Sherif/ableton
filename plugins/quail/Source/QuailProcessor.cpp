#include "QuailProcessor.h"

#include "QuailEditor.h"

namespace quail
{
namespace
{
const juce::Identifier kUiStateType{"QuailUi"};
} // namespace

QuailProcessor::QuailProcessor()
    : PluginProcessor("Quail", "QuailState", createParameterLayout(), SidechainInput::stereo)
    , parameters(apvts)
{
    // hosts may process before prepareToPlay
    engine.setSettings(parameters.read());
    engine.prepare(44100.0);
    startTimerHz(10);
}

QuailProcessor::~QuailProcessor()
{
    stopTimer();
}

double QuailProcessor::getDisplaySampleRate() const noexcept
{
    const double rate = preparedSampleRate.load(std::memory_order_relaxed);
    return rate > 0.0 ? rate : 48000.0;
}

int QuailProcessor::wantedSpectralOrder() const
{
    const auto settings = parameters.read();
    if (!settings.anySpectral(isStereo()))
        return 0;
    return dsp::spectralOrder(settings.global.resolution, getDisplaySampleRate());
}

void QuailProcessor::updateLatency()
{
    const int order = wantedSpectralOrder();
    if (order != spectralOrder.load(std::memory_order_relaxed) || getLatencySamples() != (order > 0 ? 1 << order : 0))
    {
        spectralOrder.store(order, std::memory_order_relaxed);
        setLatencySamples(order > 0 ? 1 << order : 0);
    }
}

void QuailProcessor::prepareToPlay(double sampleRate, int)
{
    preparedSampleRate.store(sampleRate, std::memory_order_relaxed);
    const int order = wantedSpectralOrder();
    spectralOrder.store(order, std::memory_order_relaxed);
    setLatencySamples(order > 0 ? 1 << order : 0);

    engine.setSettings(parameters.read());
    engine.setSpectralOrder(order);
    engine.prepare(sampleRate);
}

void QuailProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    auto mainBus = getBusBuffer(buffer, true, 0);
    const int numMain = juce::jmin(mainBus.getNumChannels(), dsp::Engine::kMaxChannels);
    float* main[dsp::Engine::kMaxChannels] = {};
    for (int ch = 0; ch < numMain; ++ch)
        main[ch] = mainBus.getWritePointer(ch);

    const float* sidechain[2] = {};
    int numSidechain = 0;
    if (getBusCount(true) > 1)
    {
        auto sidechainBus = getBusBuffer(buffer, true, 1);
        numSidechain = juce::jmin(sidechainBus.getNumChannels(), 2);
        for (int ch = 0; ch < numSidechain; ++ch)
            sidechain[ch] = sidechainBus.getReadPointer(ch);
    }
    sidechainActive.store(numSidechain > 0, std::memory_order_relaxed);

    inputFifo.pushMixdown(main, numMain, numSamples);
    if (numSidechain > 0)
        sidechainFifo.pushMixdown(sidechain, numSidechain, numSamples);

    engine.setSettings(parameters.read());
    engine.setSpectralOrder(spectralOrder.load(std::memory_order_relaxed));
    engine.setSoloBand(soloBand.load(std::memory_order_relaxed));
    engine.process(main, numMain, sidechain, numSidechain, numSamples);

    outputFifo.pushMixdown(main, numMain, numSamples);
}

juce::AudioProcessorParameter* QuailProcessor::getBypassParameter() const
{
    return &parameters.global(GlobalParam::bypass);
}

double QuailProcessor::getTailLengthSeconds() const
{
    // the longest dynamic release plus a low cut ringing out
    return 0.5;
}

juce::ValueTree QuailProcessor::getUiState()
{
    auto ui = apvts.state.getChildWithName(kUiStateType);
    if (!ui.isValid())
    {
        ui = juce::ValueTree(kUiStateType);
        apvts.state.appendChild(ui, nullptr);
    }
    return ui;
}

juce::AudioProcessorEditor* QuailProcessor::createEditor()
{
    return new QuailEditor(*this);
}
} // namespace quail

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new quail::QuailProcessor();
}
