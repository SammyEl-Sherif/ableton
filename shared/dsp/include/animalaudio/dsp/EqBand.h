#pragma once

#include "Smoothing.h"
#include "SvfFilter.h"

#include <array>
#include <cmath>

namespace animalaudio::dsp
{
/**
 * One EQ band for up to kMaxChannels channels: an SVF per channel whose frequency, gain and Q glide to their
 * targets (one-pole, in log2 Hz / dB / log2 Q), with the coefficients refreshed every kUpdateInterval samples
 * while anything moves. The refresh grid is kept across blocks, so the output does not depend on the block size.
 * Audio-thread safe: no allocation, no locks; tan/pow only once per refresh.
 */
class EqBand
{
public:
    static constexpr int kMaxChannels = 8;
    static constexpr int kUpdateInterval = 16;

    explicit EqBand(FilterShape bandShape = FilterShape::Bell) noexcept
        : shape(bandShape)
    {
    }

    /** Resets the filters and jumps straight to the current targets. */
    void prepare(double newSampleRate, double smoothingMs = 20.0) noexcept
    {
        sampleRate = newSampleRate;
        for (auto* s : {&logFrequency, &gainDb, &logQ})
        {
            s->prepare(sampleRate, smoothingMs);
            s->reset(s->getTarget());
        }
        for (auto& f : filters)
            f.reset();
        refresh();
        countdown = kUpdateInterval;
    }

    void setTarget(double frequency, double gain, double q) noexcept
    {
        logFrequency.setTarget(static_cast<float>(std::log2(std::max(frequency, 1.0))));
        gainDb.setTarget(static_cast<float>(gain));
        logQ.setTarget(static_cast<float>(std::log2(std::max(q, 0.01))));
        settled = false;
    }

    /** The coefficients the band is gliding towards (for drawing the response). */
    SvfCoefficients targetCoefficients() const noexcept
    {
        return SvfCoefficients::make(shape, std::exp2(static_cast<double>(logFrequency.getTarget())),
                                     std::exp2(static_cast<double>(logQ.getTarget())), gainDb.getTarget(), sampleRate);
    }

    void process(float* const* channels, int numChannels, int numSamples) noexcept
    {
        numChannels = numChannels < kMaxChannels ? numChannels : kMaxChannels;
        int done = 0;
        while (done < numSamples)
        {
            if (countdown <= 0)
            {
                countdown = kUpdateInterval;
                if (!settled)
                    refresh();
            }

            const int n = countdown < numSamples - done ? countdown : numSamples - done;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto& filter = filters[static_cast<std::size_t>(ch)];
                float* data = channels[ch] + done;
                for (int i = 0; i < n; ++i)
                    data[i] = filter.process(data[i]);
            }
            for (int i = 0; i < n; ++i)
            {
                logFrequency.next();
                gainDb.next();
                logQ.next();
            }
            countdown -= n;
            done += n;
        }
    }

private:
    void refresh() noexcept
    {
        const auto c =
            SvfCoefficients::make(shape, std::exp2(static_cast<double>(logFrequency.getCurrent())),
                                  std::exp2(static_cast<double>(logQ.getCurrent())), gainDb.getCurrent(), sampleRate);
        for (auto& f : filters)
            f.setCoefficients(c);
        settled = logFrequency.getCurrent() == logFrequency.getTarget() && gainDb.getCurrent() == gainDb.getTarget() &&
                  logQ.getCurrent() == logQ.getTarget();
    }

    FilterShape shape;
    double sampleRate = 44100.0;
    OnePoleSmoother logFrequency, gainDb, logQ;
    std::array<SvfFilter, kMaxChannels> filters;
    int countdown = 0;
    bool settled = false;
};
} // namespace animalaudio::dsp
