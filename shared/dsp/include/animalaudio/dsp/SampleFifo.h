#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace animalaudio::dsp
{
/**
 * Single-producer / single-consumer sample FIFO for audio -> UI feeds (analysers, meters). Lock and allocation
 * free; when the consumer falls behind (or no editor is open) the producer drops what does not fit, so the audio
 * thread never waits.
 */
template <std::size_t Capacity>
class SampleFifo
{
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    /** Producer: pushes the average of `numChannels` channels. Returns the number of frames stored. */
    int pushMixdown(const float* const* channels, int numChannels, int numSamples) noexcept
    {
        if (numChannels <= 0 || numSamples <= 0)
            return 0;
        const std::size_t w = writeIndex.load(std::memory_order_relaxed);
        const std::size_t r = readIndex.load(std::memory_order_acquire);
        const std::size_t space = Capacity - (w - r);
        const std::size_t n =
            static_cast<std::size_t>(numSamples) < space ? static_cast<std::size_t>(numSamples) : space;
        const float scale = 1.0f / static_cast<float>(numChannels);
        for (std::size_t i = 0; i < n; ++i)
        {
            float sum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                sum += channels[ch][i];
            buffer[(w + i) & kMask] = sum * scale;
        }
        writeIndex.store(w + n, std::memory_order_release);
        return static_cast<int>(n);
    }

    /** Consumer: copies up to `maxSamples` of the oldest samples into `destination`. Returns how many. */
    int pop(float* destination, int maxSamples) noexcept
    {
        if (maxSamples <= 0)
            return 0;
        const std::size_t r = readIndex.load(std::memory_order_relaxed);
        const std::size_t w = writeIndex.load(std::memory_order_acquire);
        const std::size_t available = w - r;
        const std::size_t n =
            static_cast<std::size_t>(maxSamples) < available ? static_cast<std::size_t>(maxSamples) : available;
        for (std::size_t i = 0; i < n; ++i)
            destination[i] = buffer[(r + i) & kMask];
        readIndex.store(r + n, std::memory_order_release);
        return static_cast<int>(n);
    }

    int getNumReady() const noexcept
    {
        return static_cast<int>(writeIndex.load(std::memory_order_acquire) - readIndex.load(std::memory_order_acquire));
    }

    static constexpr int capacity() noexcept { return static_cast<int>(Capacity); }

private:
    static constexpr std::size_t kMask = Capacity - 1;
    std::array<float, Capacity> buffer{};
    std::atomic<std::size_t> writeIndex{0};
    std::atomic<std::size_t> readIndex{0};
};
} // namespace animalaudio::dsp
