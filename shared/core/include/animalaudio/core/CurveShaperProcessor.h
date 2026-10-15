#pragma once

#include "CurveState.h"

#include <animalaudio/dsp/CurveGainShaper.h>
#include <animalaudio/dsp/TempoSync.h>

#include <juce_audio_processors/juce_audio_processors.h>

namespace animalaudio::core
{
/** IDs of the standard parameters every curve-shaper plugin has. They are part of the saved-state
    contract: never rename them. */
namespace ids
{
inline const juce::ParameterID division{"division", 1};
inline const juce::ParameterID mix{"mix", 1};
inline const juce::ParameterID fallbackBpm{"fallbackBpm", 1};
} // namespace ids

/**
 * Base class for tempo-synced, curve-driven effects (Duck and friends). Owns the APVTS with the
 * standard parameters (division, mix, fallbackBpm), the editable curve (CurveState), transport/phase
 * tracking, and state save/restore. Subclasses supply the DSP via renderCycle() and the editor via
 * createEditor().
 */
class CurveShaperProcessor : public juce::AudioProcessor
{
public:
    /** The per-block phase information handed to the DSP. */
    struct Cycle
    {
        double startPhase = 0.0;
        double increment = 0.0; // cycles per sample
        float mix = 1.0f;
    };

    CurveShaperProcessor(const juce::String& pluginName, const juce::Identifier& stateType,
                         const dsp::CurvePresetList& presets);

    // ---- DSP hooks (audio thread unless noted) ----
    /** prepareToPlay: allocate/reset here. */
    virtual void prepareEffect(double sampleRate, int maxBlockSize, float mix) = 0;
    /** Called when the user edited the curve; copy the table (no allocation). */
    virtual void curveTableChanged(const dsp::CurveTable& table) = 0;
    /** Process `buffer` in place. All channels have been validated; extra outputs already cleared. */
    virtual void renderCycle(juce::AudioBuffer<float>& buffer, const Cycle& cycle) = 0;

    // ---- visualisation feed for the shared editor (any thread, relaxed reads) ----
    virtual float getDisplayPhase() const { return 0.0f; }
    virtual int getNumScopeBins() const { return 0; }
    virtual float getScopeBin(int) const { return 0.0f; }

    // ---- AudioProcessor boilerplate ----
    void prepareToPlay(double sampleRate, int samplesPerBlock) final;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) final;
    using juce::AudioProcessor::processBlock;

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
    CurveState& getCurveState() noexcept { return curveState; }

    dsp::SyncSource getSyncSource() const noexcept
    {
        return static_cast<dsp::SyncSource>(syncSource.load(std::memory_order_relaxed));
    }

    int getDivisionIndex() const;
    float getMix() const noexcept { return mixParam->load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::String pluginName;
    CurveState curveState;
    dsp::PhaseTracker tracker;
    dsp::CurveTable localTable;
    std::uint64_t tableVersion = 0;
    std::atomic<int> syncSource{static_cast<int>(dsp::SyncSource::FreeRun)};

    std::atomic<float>* divisionParam = nullptr;
    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* fallbackBpmParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CurveShaperProcessor)
};

/** Ready-made processor whose DSP is dsp::CurveGainShaper (Duck's engine). Derive from this for
    "curve drives a gain" effects, or derive from CurveShaperProcessor to bring your own DSP. */
class GainShaperProcessor : public CurveShaperProcessor
{
public:
    using CurveShaperProcessor::CurveShaperProcessor;

    void prepareEffect(double sampleRate, int, float mix) override
    {
        shaper.prepare(sampleRate);
        shaper.reset(mix);
        shaper.setMix(mix);
    }

    void curveTableChanged(const dsp::CurveTable& table) override { shaper.setTable(table); }

    void renderCycle(juce::AudioBuffer<float>& buffer, const Cycle& cycle) override
    {
        shaper.setMix(cycle.mix);
        shaper.process(buffer.getArrayOfWritePointers(),
                       juce::jmin(buffer.getNumChannels(), getTotalNumOutputChannels()), buffer.getNumSamples(),
                       cycle.startPhase, cycle.increment);
    }

    float getDisplayPhase() const override { return shaper.getDisplayPhase(); }
    int getNumScopeBins() const override { return dsp::CurveGainShaper::kScopeBins; }
    float getScopeBin(int index) const override { return shaper.getScopeBin(index); }

    const dsp::CurveGainShaper& getShaper() const noexcept { return shaper; }

private:
    dsp::CurveGainShaper shaper;
};
} // namespace animalaudio::core
