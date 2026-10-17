#pragma once

#include "FilterDesign.h"
#include "Smoothing.h"
#include "SvfFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>

namespace animalaudio::dsp
{
/**
 * One EQ band for up to kMaxChannels channels, any FilterShape/slope (a cascade of SVF sections per channel).
 *
 * - Frequency, gain and Q glide to their targets (one-pole in log2 Hz / dB / log2 Q). Coefficients are refreshed
 *   every kUpdateInterval samples while anything moves, on a grid kept across blocks, so the output does not
 *   depend on the block size.
 * - A shape or slope change crossfades from the old filter to the new one; setActive() fades the whole band in
 *   or out. A band that has faded out costs nothing and leaves the signal untouched.
 * - process() can take a per-sample dynamic gain offset (dB, added to the gain at each refresh) for dynamic EQ.
 *
 * Audio-thread safe: no allocation, no locks; the filter design runs once per refresh.
 */
class EqBand
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kUpdateInterval = 16;

    EqBand() noexcept
    {
        setSmootherTargets(target);
        for (auto* s : {&logFrequency, &gainDb, &logQ})
            s->reset(s->getTarget());
    }

    /** Resets the filters and jumps straight to the current target. */
    void prepare(double newSampleRate, double smoothingMs = 20.0, double fadeMs = 12.0) noexcept
    {
        sampleRate = newSampleRate;
        for (auto* s : {&logFrequency, &gainDb, &logQ})
            s->prepare(sampleRate, smoothingMs);
        fadeStep = 1.0f / std::max(1.0f, static_cast<float>(fadeMs * 0.001 * sampleRate));
        reset();
    }

    /** Drops the filter state and any glide or fade: the band is at its target, fully in or out. */
    void reset() noexcept
    {
        for (auto* s : {&logFrequency, &gainDb, &logQ})
            s->reset(s->getTarget());
        activeLevel = active ? 1.0f : 0.0f;
        fading = false;
        pendingStructure = false;
        voices[current].shape = target.shape;
        voices[current].slope = target.slope;
        for (auto& v : voices)
            v.clear();
        lastDynamicDb = 0.0f;
        refreshVoices(0.0f);
        countdown = kUpdateInterval;
    }

    /** Cheap when nothing changed, so it can be called every block. */
    void setTarget(const FilterSpec& spec) noexcept
    {
        const std::equal_to<double> same;
        if (spec.shape == target.shape && spec.slope == target.slope && same(spec.frequency, target.frequency) &&
            same(spec.gainDb, target.gainDb) && same(spec.q, target.q))
            return;
        setSmootherTargets(spec);
        target = spec;
        settled = false;
        const auto& heading = fading ? voices[1 - current] : voices[current];
        pendingStructure = heading.shape != spec.shape || heading.slope != spec.slope;
        if (activeLevel <= 0.0f && !active)
            jumpToTarget();
    }

    /** Fades the band in (it starts at its target, from a clean state) or out. */
    void setActive(bool shouldBeActive) noexcept
    {
        if (shouldBeActive == active)
            return;
        active = shouldBeActive;
        if (active && activeLevel <= 0.0f)
            jumpToTarget();
    }

    bool isActive() const noexcept { return active; }

    /** True when the band neither processes nor fades: process() leaves the signal untouched. */
    bool isIdle() const noexcept { return !active && activeLevel <= 0.0f; }

    const FilterSpec& getTarget() const noexcept { return target; }

    /** The filter the band is gliding towards (for drawing the response). */
    FilterCascade targetCascade(float dynamicDb = 0.0f) const noexcept
    {
        auto spec = target;
        spec.gainDb += dynamicDb;
        return designFilter(spec, sampleRate);
    }

    /** `dynamicDb` (may be null): gain offset per sample, read at each coefficient refresh. */
    void process(float* const* channels, int numChannels, int numSamples, const float* dynamicDb = nullptr) noexcept
    {
        if (isIdle())
            return;

        numChannels = std::min(numChannels, kMaxChannels);
        int done = 0;
        while (done < numSamples && !isIdle())
        {
            if (countdown <= 0)
            {
                countdown = kUpdateInterval;
                const float dyn = dynamicDb != nullptr ? dynamicDb[done] : 0.0f;
                if (pendingStructure && !fading)
                    startFade();
                if (!settled || fading || std::abs(dyn - lastDynamicDb) > 1.0e-4f)
                    refreshVoices(dyn);
            }

            const int n = std::min(countdown, numSamples - done);
            processSegment(channels, numChannels, done, n);
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
    struct Voice
    {
        FilterShape shape = FilterShape::Bell;
        int slope = 12;
        int numSections = 0;
        float gain = 1.0f;
        std::array<std::array<SvfFilter, FilterCascade::kMaxSections>, kMaxChannels> filters{};

        void clear() noexcept
        {
            for (auto& channel : filters)
                for (auto& f : channel)
                    f.reset();
        }

        void apply(const FilterCascade& c) noexcept
        {
            for (int s = numSections; s < c.numSections; ++s)
                for (auto& channel : filters)
                    channel[static_cast<std::size_t>(s)].reset();
            numSections = c.numSections;
            gain = static_cast<float>(c.gain);
            for (int s = 0; s < numSections; ++s)
                for (auto& channel : filters)
                    channel[static_cast<std::size_t>(s)].setCoefficients(c.sections[static_cast<std::size_t>(s)]);
        }

        void run(int channel, float* data, int n) noexcept
        {
            auto& chain = filters[static_cast<std::size_t>(channel)];
            for (int s = 0; s < numSections; ++s)
            {
                auto& f = chain[static_cast<std::size_t>(s)];
                for (int i = 0; i < n; ++i)
                    data[i] = f.process(data[i]);
            }
            if (!std::equal_to<float>()(gain, 1.0f))
                for (int i = 0; i < n; ++i)
                    data[i] *= gain;
        }
    };

    void setSmootherTargets(const FilterSpec& spec) noexcept
    {
        logFrequency.setTarget(static_cast<float>(std::log2(std::max(spec.frequency, 1.0))));
        gainDb.setTarget(static_cast<float>(spec.gainDb));
        logQ.setTarget(static_cast<float>(std::log2(std::max(spec.q, 0.001))));
    }

    void jumpToTarget() noexcept
    {
        for (auto* s : {&logFrequency, &gainDb, &logQ})
            s->reset(s->getTarget());
        fading = false;
        pendingStructure = false;
        voices[current].shape = target.shape;
        voices[current].slope = target.slope;
        for (auto& v : voices)
        {
            v.clear();
            v.numSections = 0;
        }
        refreshVoices(lastDynamicDb);
        settled = true;
    }

    void startFade() noexcept
    {
        auto& incoming = voices[1 - current];
        incoming.shape = target.shape;
        incoming.slope = target.slope;
        incoming.clear();
        incoming.numSections = 0;
        fading = true;
        fadePosition = 0.0f;
        pendingStructure = false;
    }

    void refreshVoices(float dyn) noexcept
    {
        lastDynamicDb = dyn;
        FilterSpec spec;
        spec.frequency = std::exp2(static_cast<double>(logFrequency.getCurrent()));
        spec.gainDb = static_cast<double>(gainDb.getCurrent()) + static_cast<double>(dyn);
        spec.q = std::exp2(static_cast<double>(logQ.getCurrent()));
        for (std::size_t v = 0; v < (fading ? 2u : 1u); ++v)
        {
            auto& voice = voices[v == 0 ? current : 1 - current];
            spec.shape = voice.shape;
            spec.slope = voice.slope;
            voice.apply(designFilter(spec, sampleRate));
        }
        settled = logFrequency.isSettled() && gainDb.isSettled() && logQ.isSettled();
    }

    void processSegment(float* const* channels, int numChannels, int offset, int n) noexcept
    {
        const float activeTarget = active ? 1.0f : 0.0f;
        if (!fading && std::equal_to<float>()(activeLevel, activeTarget))
        {
            for (int ch = 0; ch < numChannels; ++ch)
                voices[current].run(ch, channels[ch] + offset, n);
            return;
        }

        std::array<float, kUpdateInterval> fadeIn{}, level{};
        float position = fadePosition, a = activeLevel;
        for (int i = 0; i < n; ++i)
        {
            if (fading)
                position = std::min(1.0f, position + fadeStep);
            a = active ? std::min(1.0f, a + fadeStep) : std::max(0.0f, a - fadeStep);
            fadeIn[static_cast<std::size_t>(i)] = position;
            level[static_cast<std::size_t>(i)] = a;
        }

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = channels[ch] + offset;
            std::array<float, kUpdateInterval> dry{}, wet{}, next{};
            std::copy(data, data + n, dry.begin());
            std::copy(data, data + n, wet.begin());
            voices[current].run(ch, wet.data(), n);
            // the ends of each ramp are exact, so finishing a fade mid-segment matches the plain path bit for bit
            if (fading)
            {
                std::copy(data, data + n, next.begin());
                voices[1 - current].run(ch, next.data(), n);
                for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i)
                    wet[i] = fadeIn[i] >= 1.0f ? next[i] : wet[i] + (next[i] - wet[i]) * fadeIn[i];
            }
            for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i)
                data[i] = level[i] >= 1.0f ? wet[i] : dry[i] + (wet[i] - dry[i]) * level[i];
        }

        fadePosition = position;
        activeLevel = a;
        if (fading && fadePosition >= 1.0f)
        {
            fading = false;
            current = 1 - current;
        }
        if (!active && activeLevel <= 0.0f)
            for (auto& v : voices)
                v.clear();
    }

    double sampleRate = 44100.0;
    FilterSpec target;
    OnePoleSmoother logFrequency, gainDb, logQ;
    std::array<Voice, 2> voices{};
    std::size_t current = 0;
    bool fading = false;
    bool pendingStructure = false;
    float fadePosition = 0.0f;
    float fadeStep = 0.01f;
    bool active = true;
    float activeLevel = 1.0f;
    float lastDynamicDb = 0.0f;
    int countdown = 0;
    bool settled = false;
};
} // namespace animalaudio::dsp
