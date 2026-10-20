#pragma once

#include <animalaudio/dsp/Smoothing.h>

#include <array>
#include <cstddef>

namespace icepenguin::dsp
{
struct ReverbSettings
{
    float roomSize = 0.5f;
    float damping = 0.5f;
    float width = 1.0f;
    float predelayMs = 20.0f;
    float decaySeconds = 2.0f;
};

/** Modulated 8-line FDN with ER cluster — lush, dense tail (Valhalla-inspired, original implementation). */
class IceReverb
{
public:
    static constexpr int kLines = 8;
    static constexpr int kErTaps = 4;
    static constexpr int kMaxDelaySamples = 88200; // ~1.84 s @ 48 kHz per line
    static constexpr int kMaxPredelaySamples = 9600;

    void prepare(double sampleRate, int maxBlockSize);
    void reset() noexcept;
    void setSettings(const ReverbSettings& settings) noexcept;
    void setMix(float wetMix) noexcept;
    void process(float* left, float* right, int numSamples) noexcept;

private:
    struct DelayLine
    {
        void prepare(int maxSamples) noexcept;
        void clear() noexcept;
        void setLength(int samples) noexcept;
        float read(float offsetSamples) const noexcept;
        void write(float sample) noexcept;
        void advance() noexcept;
        int length() const noexcept { return size; }

        std::array<float, kMaxDelaySamples> buffer{};
        int size = 1;
        int writeIndex = 0;
    };

    struct OnePoleLP
    {
        void reset() noexcept { z = 0.0f; }
        float process(float x, float coeff) noexcept;

        float z = 0.0f;
    };

    void rebuildDelayLengths() noexcept;
    void updateFeedbackTarget() noexcept;
    void updateCoeffs() noexcept;
    float diffuseInput(float sample) noexcept;
    float readPredelay() const noexcept;
    void writePredelay(float sample) noexcept;
    void advancePredelay() noexcept;
    float earlyReflections(float input) noexcept;
    static float mixFeedback(const std::array<float, kLines>& lineOut, int line) noexcept;

    double sampleRate = 48000.0;
    ReverbSettings settings{};
    float roomSizeBuilt = -1.0f;

    animalaudio::dsp::OnePoleSmoother mixSmoother;
    animalaudio::dsp::OnePoleSmoother dampSmoother;
    animalaudio::dsp::OnePoleSmoother feedbackSmoother;
    animalaudio::dsp::OnePoleSmoother widthSmoother;
    animalaudio::dsp::OnePoleSmoother modSmoother;

    std::array<DelayLine, kLines> delays{};
    std::array<OnePoleLP, kLines> dampFilters{};
    std::array<float, kLines> linePhase{};
    std::array<float, kLines> linePhaseInc{};

    std::array<DelayLine, 2> inputDiffusers{};
    std::array<float, kErTaps> erGain{};
    std::array<int, kErTaps> erOffsetSamples{};

    std::array<float, kMaxPredelaySamples> predelay{};
    int predelayWrite = 0;
    int predelaySamples = 0;

    float inputHpZ = 0.0f;
    float feedbackGain = 0.85f;
    float dampCoeff = 0.5f;
    float modDepthSamples = 1.2f;
    float mix = 0.35f;
};

} // namespace icepenguin::dsp
