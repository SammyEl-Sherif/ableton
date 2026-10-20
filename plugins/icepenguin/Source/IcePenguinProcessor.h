#pragma once

#include "dsp/IcePenguinPresets.h"
#include "dsp/IceReverb.h"

#include <juce_audio_processors/juce_audio_processors.h>

namespace icepenguin
{
namespace ids
{
inline const juce::ParameterID preset{"preset", 1};
inline const juce::ParameterID mix{"mix", 1};
inline const juce::ParameterID decay{"decay", 1};
inline const juce::ParameterID tone{"tone", 1};
inline const juce::ParameterID predelay{"predelay", 1};
inline const juce::ParameterID size{"size", 1};
} // namespace ids

class IcePenguinProcessor : public juce::AudioProcessor
{
public:
    IcePenguinProcessor();

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Ice Penguin"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return kNumReverbPresets; }
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    void pushParametersToEngine() noexcept;

    dsp::IceReverb reverb;
    std::atomic<float>* presetParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* decayParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* predelayParam = nullptr;
    std::atomic<float>* sizeParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(IcePenguinProcessor)
};
} // namespace icepenguin
