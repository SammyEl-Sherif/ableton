#pragma once

#include "QuailParameters.h"
#include "dsp/QuailEngine.h"

#include <animalaudio/core/PluginProcessor.h>
#include <animalaudio/dsp/SampleFifo.h>

#include <atomic>

namespace quail
{
/**
 * Quail: a 24-band dynamic EQ. Bells, shelves, cuts, notch, band pass, tilts and all pass at 6-96 dB/oct, each
 * band on Stereo/Left/Right/Mid/Side, with dynamics triggered by the band's own input or by the side-chain
 * input, optionally per frequency (spectral). See dsp::Engine for the signal path.
 *
 * Spectral bands add latency (one FFT frame). The reported latency follows the parameters on the message thread
 * (updateLatency(), from a timer, prepareToPlay and state restore); the audio thread only reads the result.
 */
class QuailProcessor : public animalaudio::core::PluginProcessor, private juce::Timer
{
public:
    static constexpr int kAnalyserFifoSize = 1 << 15;
    using AnalyserFifo = animalaudio::dsp::SampleFifo<kAnalyserFifoSize>;

    QuailProcessor();
    ~QuailProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorParameter* getBypassParameter() const override;
    double getTailLengthSeconds() const override;

    const ParameterSet& params() const noexcept { return parameters; }
    dsp::Settings getSettings() const noexcept { return parameters.read(); }
    dsp::Engine& getEngine() noexcept { return engine; }
    const dsp::Engine& getEngine() const noexcept { return engine; }
    double getDisplaySampleRate() const noexcept;

    AnalyserFifo& getInputFifo() noexcept { return inputFifo; }
    AnalyserFifo& getOutputFifo() noexcept { return outputFifo; }
    AnalyserFifo& getSidechainFifo() noexcept { return sidechainFifo; }
    /** True while the host feeds the side-chain bus. */
    bool isSidechainActive() const noexcept { return sidechainActive.load(std::memory_order_relaxed); }
    bool isStereo() const noexcept { return getMainBusNumInputChannels() > 1; }

    /** Band to audition (-1 = none), any thread. */
    void setSoloBand(int band) noexcept { soloBand.store(band, std::memory_order_relaxed); }
    int getSoloBand() const noexcept { return soloBand.load(std::memory_order_relaxed); }

    /** Message thread: reports the latency the current spectral bands need and tells the engine. */
    void updateLatency();
    /** FFT order the spectral stage should run at (0 = no spectral band). */
    int wantedSpectralOrder() const;

    /** Editor size and display options, saved with the plugin state. */
    juce::ValueTree getUiState();

private:
    void timerCallback() override { updateLatency(); }
    void stateRestored() override { updateLatency(); }

    ParameterSet parameters;
    dsp::Engine engine;
    std::atomic<int> spectralOrder{0};
    std::atomic<int> soloBand{-1};
    std::atomic<double> preparedSampleRate{0.0};
    std::atomic<bool> sidechainActive{false};
    AnalyserFifo inputFifo, outputFifo, sidechainFifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuailProcessor)
};
} // namespace quail
