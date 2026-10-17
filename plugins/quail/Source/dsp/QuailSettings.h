#pragma once

#include <animalaudio/dsp/FilterDesign.h>

#include <array>
#include <cmath>

namespace quail::dsp
{
using animalaudio::dsp::FilterShape;
using animalaudio::dsp::FilterSpec;

inline constexpr int kNumBands = 24;
inline constexpr float kMaxGainDb = 30.0f;
inline constexpr float kMaxRangeDb = 30.0f;
inline constexpr float kMinHz = 10.0f;
inline constexpr float kMaxHz = 30000.0f;
inline constexpr float kMinQ = 0.025f;
inline constexpr float kMaxQ = 40.0f;

/** Which part of the signal a band processes. L/R bands run before M/S bands; on mono input only Stereo bands
    apply. */
enum class Placement
{
    Stereo,
    Left,
    Right,
    Mid,
    Side
};
inline constexpr int kNumPlacements = 5;

enum class SpectralResolution
{
    Low,
    Medium,
    High
};

constexpr bool supportsDynamics(FilterShape s) noexcept
{
    return animalaudio::dsp::shapeHasGain(s);
}

constexpr bool supportsSpectral(FilterShape s) noexcept
{
    return s == FilterShape::Bell || s == FilterShape::LowShelf || s == FilterShape::HighShelf;
}

struct BandSettings
{
    bool used = false;
    bool enabled = true;
    FilterShape shape = FilterShape::Bell;
    int slope = 12;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    Placement placement = Placement::Stereo;

    /** Dynamic EQ: up to this many dB are added to the gain while the trigger is over the threshold
        (negative = compress / duck, positive = expand / boost). */
    float rangeDb = 0.0f;
    bool dynamicsEnabled = true;
    bool autoThreshold = true;
    float thresholdDb = -30.0f;
    float attackMs = 10.0f;
    float releaseMs = 150.0f;
    /** Trigger from the side-chain input instead of the band's own input. */
    bool externalSidechain = false;
    /** Spectral dynamics: every frequency inside the band follows its own level in the trigger signal. */
    bool spectral = false;
    float spectralDensity = 0.5f;
    bool spectralTilt = true;

    bool isActive() const noexcept { return used && enabled; }
    bool hasDynamics() const noexcept
    {
        return isActive() && dynamicsEnabled && rangeDb != 0.0f && supportsDynamics(shape);
    }
    bool isSpectral() const noexcept { return hasDynamics() && spectral && supportsSpectral(shape); }
};

struct GlobalSettings
{
    float outputDb = 0.0f;
    float pan = 0.0f;
    float gainScale = 1.0f;
    bool autoGain = false;
    bool phaseInvert = false;
    bool bypass = false;
    SpectralResolution resolution = SpectralResolution::Medium;
};

struct Settings
{
    std::array<BandSettings, kNumBands> bands{};
    GlobalSettings global;

    bool anySpectral(bool stereoInput = true) const noexcept
    {
        for (const auto& b : bands)
            if (b.isSpectral() && (stereoInput || b.placement == Placement::Stereo))
                return true;
        return false;
    }
};

/** The filter a band runs: its static gain times the global gain scale, plus a dynamic offset. */
inline FilterSpec filterSpec(const BandSettings& band, float gainScale, float dynamicDb = 0.0f) noexcept
{
    FilterSpec spec;
    spec.shape = band.shape;
    spec.slope = band.slope;
    spec.frequency = band.frequency;
    spec.q = band.q;
    spec.gainDb = static_cast<double>(band.gainDb * gainScale + dynamicDb);
    return spec;
}

/** dB response of one band at `hz` (0 if it is not active). */
inline double bandResponseDb(const BandSettings& band, double hz, double sampleRate, float gainScale,
                             float dynamicDb = 0.0f) noexcept
{
    if (!band.isActive())
        return 0.0;
    return animalaudio::dsp::designFilter(filterSpec(band, gainScale, dynamicDb), sampleRate)
        .magnitudeDbAt(hz, sampleRate);
}

/** FFT order of the spectral stage: 2048/4096/8192 points at 44.1/48 kHz, scaled with the sample rate so the
    frequency resolution stays the same. */
inline int spectralOrder(SpectralResolution resolution, double sampleRate) noexcept
{
    const int base = resolution == SpectralResolution::Low ? 11 : resolution == SpectralResolution::Medium ? 12 : 13;
    return base + (sampleRate > 150000.0 ? 2 : sampleRate > 75000.0 ? 1 : 0);
}

/**
 * Gain change of a dynamic band for a trigger `overDb` above its threshold: 0 below it, the full range once the
 * trigger is max(9, 1.5 |range|) dB over (about 3:1 while it moves), with a smooth knee at both ends.
 */
inline float dynamicGainDb(float overDb, float rangeDb) noexcept
{
    if (overDb <= 0.0f || rangeDb == 0.0f)
        return 0.0f;
    const float width = std::fmax(9.0f, 1.5f * std::fabs(rangeDb));
    const float t = std::fmin(1.0f, overDb / width);
    return rangeDb * t * t * (3.0f - 2.0f * t);
}

/** An estimate (no audio involved) of how much louder the static EQ makes a typical mix, for auto gain. */
inline float estimateLoudnessChangeDb(const Settings& settings, double sampleRate) noexcept
{
    constexpr int kPoints = 32;
    std::array<animalaudio::dsp::FilterCascade, kNumBands> cascades{};
    std::array<float, kNumBands> weights{};
    bool any = false;
    for (std::size_t b = 0; b < cascades.size(); ++b)
    {
        const auto& band = settings.bands[b];
        if (!band.isActive())
            continue;
        cascades[b] = animalaudio::dsp::designFilter(filterSpec(band, settings.global.gainScale), sampleRate);
        weights[b] = band.placement == Placement::Stereo ? 1.0f : 0.5f;
        any = any || !cascades[b].isIdentity();
    }
    if (!any)
        return 0.0f;

    double sum = 0.0, weightSum = 0.0;
    for (int i = 0; i < kPoints; ++i)
    {
        const double hz = 40.0 * std::pow(12000.0 / 40.0, static_cast<double>(i) / (kPoints - 1));
        const double w = hz >= 150.0 && hz <= 6000.0 ? 1.0 : 0.5;
        double db = 0.0;
        for (std::size_t b = 0; b < cascades.size(); ++b)
            if (weights[b] > 0.0f)
                db += weights[b] * cascades[b].magnitudeDbAt(hz, sampleRate);
        sum += w * std::fmax(db, -30.0);
        weightSum += w;
    }
    return static_cast<float>(sum / weightSum);
}
} // namespace quail::dsp
