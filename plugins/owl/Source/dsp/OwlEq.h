#pragma once

#include <animalaudio/dsp/EqBand.h>
#include <animalaudio/dsp/Smoothing.h>

#include <array>
#include <cmath>
#include <functional>

namespace owl::dsp
{
enum Band
{
    kLow,
    kMid,
    kHigh,
    kNumBands
};

/** Everything that shapes Owl's sound: low shelf, mid bell, high shelf and an output trim. */
struct Settings
{
    float lowFreq = 120.0f;
    float lowGain = 0.0f; // dB
    float midFreq = 1000.0f;
    float midGain = 0.0f;
    float midQ = 1.0f;
    float highFreq = 8000.0f;
    float highGain = 0.0f;
    float output = 0.0f;

    double frequency(int band) const noexcept { return band == kLow ? lowFreq : band == kMid ? midFreq : highFreq; }
    double gainDb(int band) const noexcept { return band == kLow ? lowGain : band == kMid ? midGain : highGain; }
    double q(int band) const noexcept
    {
        return band == kMid ? static_cast<double>(midQ) : animalaudio::dsp::SvfCoefficients::kButterworthQ;
    }
};

inline animalaudio::dsp::FilterShape shapeOf(int band) noexcept
{
    using animalaudio::dsp::FilterShape;
    return band == kLow ? FilterShape::LowShelf : band == kMid ? FilterShape::Bell : FilterShape::HighShelf;
}

inline animalaudio::dsp::SvfCoefficients coefficients(const Settings& s, int band, double sampleRate) noexcept
{
    return animalaudio::dsp::SvfCoefficients::make(shapeOf(band), s.frequency(band), s.q(band), s.gainDb(band),
                                                   sampleRate);
}

/** One band's response in dB at `frequency` (no output trim). */
inline double bandResponseDb(const Settings& s, int band, double frequency, double sampleRate) noexcept
{
    return coefficients(s, band, sampleRate).magnitudeDbAt(frequency, sampleRate);
}

/** The whole EQ's response in dB at `frequency`, output trim included. */
inline double responseDb(const Settings& s, double frequency, double sampleRate) noexcept
{
    double db = s.output;
    for (int band = 0; band < kNumBands; ++band)
        db += bandResponseDb(s, band, frequency, sampleRate);
    return db;
}

/** The audio engine: three EqBands in series, then a smoothed output gain. Realtime safe. */
class OwlEq
{
public:
    void prepare(double sampleRate) noexcept
    {
        for (auto& band : bands)
            band.prepare(sampleRate);
        outputGain.prepare(sampleRate, 20.0);
        outputGain.reset(outputGain.getTarget());
    }

    /** Call once per block (it does a pow for the output gain). */
    void setSettings(const Settings& s) noexcept
    {
        for (int band = 0; band < kNumBands; ++band)
            bands[static_cast<std::size_t>(band)].setTarget(s.frequency(band), s.gainDb(band), s.q(band));
        outputGain.setTarget(static_cast<float>(std::pow(10.0, static_cast<double>(s.output) / 20.0)));
    }

    void process(float* const* channels, int numChannels, int numSamples) noexcept
    {
        for (auto& band : bands)
            band.process(channels, numChannels, numSamples);

        if (outputGain.isSettled())
        {
            const float gain = outputGain.getTarget();
            if (std::not_equal_to<float>()(gain, 1.0f))
                for (int ch = 0; ch < numChannels; ++ch)
                    for (int i = 0; i < numSamples; ++i)
                        channels[ch][i] *= gain;
            return;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            const float gain = outputGain.next();
            for (int ch = 0; ch < numChannels; ++ch)
                channels[ch][i] *= gain;
        }
    }

private:
    std::array<animalaudio::dsp::EqBand, kNumBands> bands{animalaudio::dsp::EqBand(shapeOf(kLow)),
                                                          animalaudio::dsp::EqBand(shapeOf(kMid)),
                                                          animalaudio::dsp::EqBand(shapeOf(kHigh))};
    animalaudio::dsp::OnePoleSmoother outputGain;
};
} // namespace owl::dsp
