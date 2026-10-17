#pragma once

#include "QuailSettings.h"

#include <animalaudio/dsp/Stft.h>

#include <array>
#include <atomic>
#include <functional>
#include <vector>

namespace quail::dsp
{
/**
 * Spectral dynamics, run once per StftProcessor frame. For every spectral band and every FFT bin inside it:
 * the trigger spectrum (the band's own input, or the side chain) is smoothed across frequency (density) and over
 * time (attack/release), compared with the threshold, and moves that bin's gain by up to the band's range,
 * weighted by how far inside the band the bin lies. So a side-chained band ducks (or lifts) only the frequencies
 * the side chain actually has.
 *
 * Frame keys: 0/1 = the plugin input (pre EQ), 2/3 = the side chain. Allocation free after prepare().
 */
class SpectralDynamics
{
public:
    static constexpr int kDisplayPoints = 128;
    /** Auto threshold: this far above the band's long-term average level. */
    static constexpr float kAutoThresholdOffsetDb = 3.0f;

    /** Message thread: allocates for frames of up to maxBins bins. */
    void prepare(int maxBins);
    /** New frame layout (sample rate, FFT size, hop): clears all envelopes. */
    void configure(double sampleRate, int fftSize, int hop) noexcept;
    void reset() noexcept;

    void processFrame(animalaudio::dsp::StftProcessor::Frame& frame, const Settings& settings,
                      bool stereoInput) noexcept;

    /** Signed largest gain change (dB) inside the band in the last frame, 0 when it is not spectral. */
    float getBandGainDb(int band) const noexcept;
    /** Loudest smoothed trigger level (dB) inside the band in the last frame. */
    float getBandLevelDb(int band) const noexcept;
    /** Threshold the band used in the last frame (the auto threshold when that is on). */
    float getBandThresholdDb(int band) const noexcept;
    /** Gain change of all spectral bands of one placement at display point `point` (0..kDisplayPoints-1). */
    float getDisplayDb(Placement placement, int point) const noexcept;
    bool isDisplayActive(Placement placement) const noexcept;
    static double displayFrequency(int point) noexcept;

private:
    struct WeightKey
    {
        FilterShape shape = FilterShape::Bell;
        int slope = 0;
        float frequency = 0.0f;
        float q = 0.0f;
        bool operator==(const WeightKey& o) const noexcept
        {
            const std::equal_to<float> same;
            return shape == o.shape && slope == o.slope && same(frequency, o.frequency) && same(q, o.q);
        }
    };

    struct BandState
    {
        bool wasActive = false;
        bool weightsValid = false;
        WeightKey key;
        int lo = 1, hi = 0;
        float slowLevelDb = -120.0f;
    };

    void updateWeights(int band, const BandSettings& settings) noexcept;
    /** Power spectrum (index source * kNumPlacements + placement) computed lazily once per frame. */
    const float* triggerPower(int source, Placement placement, const animalaudio::dsp::StftProcessor::Frame& frame,
                              bool stereoInput) noexcept;

    double sampleRate = 44100.0;
    int fftSize = 0;
    int hop = 0;
    int numBins = 0;
    int maxBins = 0;

    std::array<BandState, kNumBands> bands{};
    std::vector<float> envelopes; // kNumBands x maxBins
    std::vector<float> weights;   // kNumBands x maxBins
    std::vector<float> tiltDb;    // maxBins
    static constexpr int kNumSources = 2;
    std::vector<float> powers;    // kNumSources * kNumPlacements x maxBins
    std::vector<double> prefixes; // kNumSources * kNumPlacements x (maxBins + 1)
    std::array<bool, kNumSources * kNumPlacements> powerValid{};
    std::vector<float> groupGains; // kNumPlacements x maxBins (dB)
    std::array<bool, kNumPlacements> groupUsed{};

    std::array<std::atomic<float>, kNumBands> bandGainDb{};
    std::array<std::atomic<float>, kNumBands> bandLevelDb{};
    std::array<std::atomic<float>, kNumBands> bandThresholdDb{};
    std::array<std::atomic<float>, kNumPlacements * kDisplayPoints> display{};
    std::array<std::atomic<bool>, kNumPlacements> displayActive{};
};
} // namespace quail::dsp
