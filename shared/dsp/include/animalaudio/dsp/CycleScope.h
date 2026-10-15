#pragma once

#include <algorithm>
#include <array>
#include <atomic>

namespace animalaudio::dsp
{
/**
 * Audio -> UI visualisation data for cycle-synced effects: the playhead phase and a peak level per
 * phase bin. Written on the audio thread with relaxed atomics, read by the UI thread.
 */
template <int NumBins>
class CycleScope
{
public:
    static constexpr int kNumBins = NumBins;

    void clear() noexcept
    {
        for (auto& bin : bins)
            bin.store(0.0f, std::memory_order_relaxed);
    }

    void push(double phase, float peak) noexcept
    {
        auto& bin = bins[static_cast<std::size_t>(std::min(static_cast<int>(phase * NumBins), NumBins - 1))];
        const float old = bin.load(std::memory_order_relaxed);
        bin.store(std::max(peak, old * kDecay), std::memory_order_relaxed);
    }

    void setPhase(double phase) noexcept { displayPhase.store(static_cast<float>(phase), std::memory_order_relaxed); }

    float getPhase() const noexcept { return displayPhase.load(std::memory_order_relaxed); }
    float getBin(int index) const noexcept
    {
        return bins[static_cast<std::size_t>(index)].load(std::memory_order_relaxed);
    }

private:
    static constexpr float kDecay = 0.9995f;

    std::atomic<float> displayPhase{0.0f};
    std::array<std::atomic<float>, NumBins> bins{};
};
} // namespace animalaudio::dsp
