#pragma once

#include "QuailSettings.h"
#include "QuailSpectral.h"

#include <animalaudio/dsp/EnvelopeFollower.h>
#include <animalaudio/dsp/EqBand.h>
#include <animalaudio/dsp/Smoothing.h>
#include <animalaudio/dsp/Stft.h>
#include <animalaudio/dsp/SvfFilter.h>

#include <array>
#include <atomic>
#include <vector>

namespace quail::dsp
{
/**
 * Quail's audio engine (JUCE free).
 *
 * - 24 bands in two stages: Stereo/Left/Right bands, then Mid/Side bands (M = (L+R)/2, S = (L-R)/2).
 * - Dynamic EQ: a band-limited detector on the band's own input or on the side chain moves the band's gain by up
 *   to its range (EqBand dynamic offset, per sample).
 * - Spectral dynamics: an STFT stage after the filters (latency = FFT size) where spectral bands act per bin.
 * - Band solo, output gain/pan/phase invert/auto gain, and a soft bypass whose dry path is delayed by the
 *   current latency.
 *
 * prepare() allocates (message thread); everything else is audio-thread safe: no allocation, no locks.
 */
class Engine
{
public:
    static constexpr int kChunk = 512;
    static constexpr int kMaxChannels = 2;
    /** Auto threshold of time-domain dynamics: this far above the trigger's long-term average. */
    static constexpr float kAutoThresholdOffsetDb = 3.0f;

    void prepare(double sampleRate);
    void reset() noexcept;

    /** Audio thread, once per block before process(). */
    void setSettings(const Settings& newSettings) noexcept;
    /** FFT order of the spectral stage (0 = off); switching fades the output out and back in. */
    void setSpectralOrder(int order) noexcept;
    /** Band to audition (-1 = none). */
    void setSoloBand(int band) noexcept;

    void process(float* const* main, int numMain, const float* const* sidechain, int numSidechain,
                 int numSamples) noexcept;

    double getSampleRate() const noexcept { return sampleRate; }
    int getSpectralOrder() const noexcept { return spectralCurrent; }
    int getLatencySamples() const noexcept { return spectralCurrent > 0 ? stft.getLatency() : 0; }

    /** Gain change (dB) the dynamics currently apply to a band (spectral bands: the largest one). */
    float getBandDynamicDb(int band) const noexcept;
    /** Level of the band's trigger (dB, a full-scale sine reads 0). */
    float getBandTriggerDb(int band) const noexcept;
    /** Threshold in use (the auto threshold when that is on). */
    float getBandThresholdDb(int band) const noexcept;
    float getAutoGainDb() const noexcept { return autoGainDb.load(std::memory_order_relaxed); }
    const SpectralDynamics& getSpectral() const noexcept { return spectral; }

    /** Peak |sample| since the last call (UI meters). */
    float takeOutputPeak(int channel) noexcept;
    float takeSidechainPeak() noexcept;

private:
    struct Detector
    {
        std::array<animalaudio::dsp::SvfFilter, 2> filters{};
        animalaudio::dsp::EnvelopeFollower envelope, average;
        bool filtered = false;
        FilterShape shape = FilterShape::Bell;
        float frequency = 0.0f, q = 0.0f;
        bool primed = false;
        /** Gain change at the end of the last chunk; released towards 0 once the dynamics stop. */
        float offsetDb = 0.0f;
    };

    struct BandState
    {
        Placement placement = Placement::Stereo;
        bool timeDynamics = false;
        bool spectralDynamics = false;
    };

    void updateBands(bool stereo) noexcept;
    void updateOutputTargets(bool jump) noexcept;
    void updateAutoGain(int n) noexcept;
    void processChunk(float* const* main, int numMain, const float* scA, const float* scB, int n) noexcept;
    void runDetector(int band, const float* a, const float* b, int n) noexcept;
    void runStage(bool midSide, float* const* channels, int numMain, int n,
                  const std::array<const float*, kNumBands>& offsets) noexcept;
    void runSolo(float* const* main, int numMain, int n) noexcept;

    double sampleRate = 44100.0;
    Settings settings;
    Settings autoGainSettings;
    bool autoGainValid = false;
    int autoGainCountdown = 0;

    std::array<animalaudio::dsp::EqBand, kNumBands> bands{};
    std::array<BandState, kNumBands> bandStates{};
    std::array<Detector, kNumBands> detectors{};
    std::vector<float> dynamicDb; // kNumBands x kChunk
    float offsetRelease = 0.999f;

    std::array<std::vector<float>, kMaxChannels> pre;
    std::array<std::vector<float>, kMaxChannels> audition;

    animalaudio::dsp::StftProcessor stft;
    SpectralDynamics spectral;
    int spectralRequested = 0;
    int spectralCurrent = 0;
    float switchGain = 1.0f;
    float switchStep = 0.01f;
    int switchHold = 0;

    int soloRequested = -1;
    int soloBand = -1;
    animalaudio::dsp::OnePoleSmoother soloMix;
    std::array<animalaudio::dsp::SvfFilter, kMaxChannels> soloFilters{};
    FilterShape soloShape = FilterShape::Bell;
    float soloFrequency = 0.0f, soloQ = 0.0f;

    std::array<animalaudio::dsp::OnePoleSmoother, kMaxChannels> outputGain;
    animalaudio::dsp::OnePoleSmoother bypassMix;
    std::array<std::vector<float>, kMaxChannels> dryDelay;
    std::size_t dryMask = 0;
    std::size_t dryWrite = 0;

    std::array<std::atomic<float>, kNumBands> bandDynamicDb{};
    std::array<std::atomic<float>, kNumBands> bandTriggerDb{};
    std::array<std::atomic<float>, kNumBands> bandThresholdDb{};
    std::atomic<float> autoGainDb{0.0f};
    std::array<std::atomic<float>, kMaxChannels> outputPeak{};
    std::atomic<float> sidechainPeak{0.0f};
};
} // namespace quail::dsp
