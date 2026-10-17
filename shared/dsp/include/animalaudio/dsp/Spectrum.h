#pragma once

#include "Fft.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace animalaudio::dsp
{
/**
 * Magnitude spectrum of the most recent 2^order samples: Hann window + radix-2 FFT. This is the UI side of an
 * analyser (feed it from a SampleFifo on a timer); it allocates only in the constructor. A full-scale sine
 * centred on a bin reads 0 dB.
 */
class SpectrumAnalyser
{
public:
    static constexpr float kFloorDb = -120.0f;

    explicit SpectrumAnalyser(int order = 12)
        : size(std::size_t{1} << order)
        , fft(order)
        , history(size, 0.0f)
        , window(size)
        , work(size)
        , magnitudesDb(size / 2 + 1, kFloorDb)
    {
        constexpr double twoPi = 6.283185307179586;
        double windowSum = 0.0;
        for (std::size_t i = 0; i < size; ++i)
        {
            window[i] =
                static_cast<float>(0.5 - 0.5 * std::cos(twoPi * static_cast<double>(i) / static_cast<double>(size)));
            windowSum += window[i];
        }
        amplitudeScale = static_cast<float>(2.0 / windowSum);
    }

    int getSize() const noexcept { return static_cast<int>(size); }
    int getNumBins() const noexcept { return static_cast<int>(magnitudesDb.size()); }

    /** Appends samples to the analysis history (oldest samples fall out). */
    void write(const float* samples, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            history[writePos] = samples[i];
            writePos = (writePos + 1) & (size - 1);
        }
    }

    /** Windows the history, runs the FFT and updates the per-bin dB values. */
    void compute() noexcept
    {
        for (std::size_t i = 0; i < size; ++i)
            work[i] = {history[(writePos + i) & (size - 1)] * window[i], 0.0f};

        fft.forward(work.data());

        for (std::size_t bin = 0; bin < magnitudesDb.size(); ++bin)
        {
            const float amplitude = std::abs(work[bin]) * amplitudeScale;
            magnitudesDb[bin] = std::max(kFloorDb, 20.0f * std::log10(std::max(amplitude, 1.0e-7f)));
        }
    }

    float binDb(int bin) const noexcept { return magnitudesDb[static_cast<std::size_t>(bin)]; }

    double binFrequency(int bin, double sampleRate) const noexcept
    {
        return static_cast<double>(bin) * sampleRate / static_cast<double>(size);
    }

    /** Loudest bin between two frequencies, or the interpolated value at their centre if no bin falls inside
        (low frequencies, where a display column is narrower than a bin). */
    float peakDbBetween(double lowHz, double highHz, double sampleRate) const noexcept
    {
        const double binsPerHz = static_cast<double>(size) / sampleRate;
        const double lo = std::max(0.0, lowHz * binsPerHz);
        const double hi = std::min(static_cast<double>(magnitudesDb.size() - 1), highHz * binsPerHz);
        const auto first = static_cast<std::size_t>(std::ceil(lo));
        const auto last = static_cast<std::size_t>(std::floor(hi));
        if (first <= last && last < magnitudesDb.size())
            return *std::max_element(magnitudesDb.begin() + static_cast<std::ptrdiff_t>(first),
                                     magnitudesDb.begin() + static_cast<std::ptrdiff_t>(last) + 1);

        const double centre = std::clamp(0.5 * (lo + hi), 0.0, static_cast<double>(magnitudesDb.size() - 1));
        const auto below = static_cast<std::size_t>(centre);
        const auto above = std::min(below + 1, magnitudesDb.size() - 1);
        const auto t = static_cast<float>(centre - static_cast<double>(below));
        return magnitudesDb[below] + t * (magnitudesDb[above] - magnitudesDb[below]);
    }

private:
    std::size_t size;
    Fft fft;
    std::vector<float> history;
    std::size_t writePos = 0;
    std::vector<float> window;
    std::vector<std::complex<float>> work;
    std::vector<float> magnitudesDb;
    float amplitudeScale = 1.0f;
};
} // namespace animalaudio::dsp
