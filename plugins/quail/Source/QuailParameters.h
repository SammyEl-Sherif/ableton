#pragma once

#include "dsp/QuailSettings.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

namespace quail
{
/** Per-band parameters. IDs are "b<1..24><suffix>" (e.g. "b3Freq"); IDs, ranges and order are the saved-state
    contract. */
enum class BandParam
{
    used,
    enabled,
    frequency,
    gain,
    q,
    shape,
    slope,
    placement,
    range,
    dynamics,
    threshold,
    autoThreshold,
    attack,
    release,
    sidechain,
    spectral,
    density,
    spectralTilt
};
inline constexpr int kNumBandParams = 18;

enum class GlobalParam
{
    output,
    pan,
    gainScale,
    autoGain,
    phaseInvert,
    bypass,
    spectralResolution
};
inline constexpr int kNumGlobalParams = 7;

juce::String bandParamId(int band, BandParam param);
juce::String globalParamId(GlobalParam param);

/** Shape / slope / placement / resolution choice labels, in parameter order. */
const juce::StringArray& shapeNames();
const juce::StringArray& slopeNames();
const juce::StringArray& placementNames();
const juce::StringArray& resolutionNames();

/** Default frequency of a band before anything moved it: spread from 50 Hz to 16 kHz. */
float defaultBandFrequency(int band);

juce::String formatHz(float hz);
juce::String formatDb(float db);

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

/** Typed access to every Quail parameter; read() is lock free (atomics), usable from any thread. */
class ParameterSet
{
public:
    explicit ParameterSet(juce::AudioProcessorValueTreeState& apvts);

    dsp::Settings read() const noexcept;

    juce::RangedAudioParameter& band(int index, BandParam param) const noexcept;
    juce::RangedAudioParameter& global(GlobalParam param) const noexcept;
    float value(int index, BandParam param) const noexcept;

private:
    std::array<std::array<juce::RangedAudioParameter*, kNumBandParams>, dsp::kNumBands> bandParams{};
    std::array<std::array<std::atomic<float>*, kNumBandParams>, dsp::kNumBands> bandValues{};
    std::array<juce::RangedAudioParameter*, kNumGlobalParams> globalParams{};
    std::array<std::atomic<float>*, kNumGlobalParams> globalValues{};
};
} // namespace quail
