#pragma once

#include "CurveState.h"
#include "dsp/DuckEngine.h"

#include <animalaudio/dsp/TempoSync.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace duck
{
namespace ids
{
inline const juce::ParameterID division{"division", 1};
inline const juce::ParameterID mix{"mix", 1};
inline const juce::ParameterID fallbackBpm{"fallbackBpm", 1};
} // namespace ids

class DuckProcessor : public juce::AudioProcessor
{
public:
    DuckProcessor();
    ~DuckProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Duck"; }
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
    CurveState& getCurveState() noexcept { return curveState; }
    const DuckEngine& getEngine() const noexcept { return engine; }
    animalaudio::dsp::SyncSource getSyncSource() const noexcept
    {
        return static_cast<animalaudio::dsp::SyncSource>(syncSource.load(std::memory_order_relaxed));
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    CurveState curveState;
    DuckEngine engine;
    animalaudio::dsp::PhaseTracker tracker;
    animalaudio::dsp::CurveTable localTable;
    std::uint64_t tableVersion = 0;
    std::atomic<int> syncSource{static_cast<int>(animalaudio::dsp::SyncSource::FreeRun)};

    std::atomic<float>* divisionParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* fallbackBpmParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckProcessor)
};
} // namespace duck
