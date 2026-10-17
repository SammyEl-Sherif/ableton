#pragma once

#include "dsp/OwlEq.h"

#include <animalaudio/core/PluginProcessor.h>
#include <animalaudio/dsp/SampleFifo.h>

#include <array>
#include <atomic>

namespace owl
{
/** Owl's parameter IDs. Saved-state contract: never rename them. */
namespace ids
{
inline const juce::ParameterID lowFreq{"lowFreq", 1};
inline const juce::ParameterID lowGain{"lowGain", 1};
inline const juce::ParameterID midFreq{"midFreq", 1};
inline const juce::ParameterID midGain{"midGain", 1};
inline const juce::ParameterID midQ{"midQ", 1};
inline const juce::ParameterID highFreq{"highFreq", 1};
inline const juce::ParameterID highGain{"highGain", 1};
inline const juce::ParameterID output{"output", 1};

/** Frequency and gain parameter of each band, indexed by dsp::Band. */
inline const juce::ParameterID& frequencyOf(int band)
{
    return band == dsp::kLow ? lowFreq : band == dsp::kMid ? midFreq : highFreq;
}
inline const juce::ParameterID& gainOf(int band)
{
    return band == dsp::kLow ? lowGain : band == dsp::kMid ? midGain : highGain;
}
} // namespace ids

inline constexpr float kMaxGainDb = 18.0f;

/** Three-band EQ (low shelf, mid bell, high shelf) + output trim, with a post-EQ feed for the spectrum view. */
class OwlProcessor : public animalaudio::core::PluginProcessor
{
public:
    using AnalyserFifo = animalaudio::dsp::SampleFifo<8192>;

    OwlProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;

    /** Current parameter values (any thread). */
    dsp::Settings getSettings() const noexcept;

    /** Rate for drawing the response before the host has prepared the processor. */
    double getDisplaySampleRate() const noexcept;

    AnalyserFifo& getAnalyserFifo() noexcept { return analyserFifo; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    dsp::OwlEq eq;
    AnalyserFifo analyserFifo;
    std::atomic<double> preparedSampleRate{0.0};

    std::atomic<float>* lowFreq = nullptr;
    std::atomic<float>* lowGain = nullptr;
    std::atomic<float>* midFreq = nullptr;
    std::atomic<float>* midGain = nullptr;
    std::atomic<float>* midQ = nullptr;
    std::atomic<float>* highFreq = nullptr;
    std::atomic<float>* highGain = nullptr;
    std::atomic<float>* outputGain = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OwlProcessor)
};
} // namespace owl
