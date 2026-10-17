#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace animalaudio::core
{
/**
 * Base AudioProcessor for Animal Audio effects: stereo-in/stereo-out buses (mono/stereo, in == out), an optional
 * side-chain input bus, the APVTS built from the subclass's parameter layout, one program, and state save/restore
 * of the whole APVTS ValueTree. Subclasses add DSP (prepareToPlay/processBlock) and the editor; extra state lives
 * as children of `apvts.state` and is fixed up in stateRestored().
 */
class PluginProcessor : public juce::AudioProcessor
{
public:
    /** `stereo`: a second input bus "Sidechain" (stereo by default; the host may make it mono or disable it). */
    enum class SidechainInput
    {
        none,
        stereo
    };

    PluginProcessor(const juce::String& pluginName, const juce::Identifier& stateType,
                    juce::AudioProcessorValueTreeState::ParameterLayout layout,
                    SidechainInput sidechain = SidechainInput::none);

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void releaseResources() override {}

    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return pluginName; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

protected:
    /** Called on the message thread after setStateInformation() replaced the APVTS state. */
    virtual void stateRestored() {}

private:
    static BusesProperties makeBuses(SidechainInput sidechain);

    juce::String pluginName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};
} // namespace animalaudio::core
