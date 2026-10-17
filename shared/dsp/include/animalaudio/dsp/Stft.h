#pragma once

#include "Fft.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace animalaudio::dsp
{
/**
 * Streaming short-time Fourier processing by weighted overlap-add: periodic Hann analysis and synthesis windows,
 * hop = size / 4, so an untouched spectrum resynthesises the input exactly, delayed by getLatency() = size
 * samples. Up to kMaxChannels channels are processed; up to kMaxKeys more (a side chain, a detector feed) are
 * only analysed, frame-aligned with them. Two real channels share one complex FFT.
 *
 * prepare() allocates for a range of frame sizes; setOrder(), process() and the frame callback are allocation
 * and lock free (audio thread).
 */
class StftProcessor
{
public:
    static constexpr int kMaxChannels = 2;
    static constexpr int kMaxKeys = 4;

    /** Bins 0..numBins-1 (DC..Nyquist) of every channel; edit `channels` in place, `keys` are read only. A
        full-scale sine centred on bin k has |X[k]| = size / 4. */
    struct Frame
    {
        std::array<std::complex<float>*, kMaxChannels> channels{};
        std::array<const std::complex<float>*, kMaxKeys> keys{};
        int numChannels = 0;
        int numKeys = 0;
        int numBins = 0;
        int size = 0;
    };

    /** Message thread: allocates every frame size from 2^minOrder to 2^maxOrder and selects maxOrder. */
    void prepare(int minOrder, int maxOrder)
    {
        firstOrder = minOrder;
        ffts.clear();
        windows.clear();
        for (int order = minOrder; order <= maxOrder; ++order)
        {
            ffts.emplace_back(order);
            const auto n = std::size_t{1} << order;
            std::vector<float> w(n);
            for (std::size_t i = 0; i < n; ++i)
                w[i] =
                    static_cast<float>(0.5 - 0.5 * std::cos(kTwoPi * static_cast<double>(i) / static_cast<double>(n)));
            windows.push_back(std::move(w));
        }
        const auto maxSize = std::size_t{1} << maxOrder;
        for (auto& b : inputs)
            b.assign(maxSize, 0.0f);
        for (auto& b : outputs)
            b.assign(maxSize, 0.0f);
        for (auto& w : work)
            w.assign(maxSize, {});
        for (auto& s : spectra)
            s.assign(maxSize / 2 + 1, {});
        setOrder(maxOrder);
    }

    bool isPrepared() const noexcept { return !ffts.empty(); }
    int getMinOrder() const noexcept { return firstOrder; }
    int getMaxOrder() const noexcept { return firstOrder + static_cast<int>(ffts.size()) - 1; }

    /** Selects a prepared frame size (clamped into the prepared range) and clears all state. */
    void setOrder(int order) noexcept
    {
        if (ffts.empty())
            return;
        orderIndex = static_cast<std::size_t>(std::clamp(order, getMinOrder(), getMaxOrder()) - firstOrder);
        size = ffts[orderIndex].getSize();
        hop = size / 4;
        reset();
    }

    int getOrder() const noexcept { return firstOrder + static_cast<int>(orderIndex); }
    int getSize() const noexcept { return size; }
    int getNumBins() const noexcept { return size / 2 + 1; }
    int getHop() const noexcept { return hop; }
    int getLatency() const noexcept { return size; }

    void reset() noexcept
    {
        for (auto& b : inputs)
            std::fill(b.begin(), b.end(), 0.0f);
        for (auto& b : outputs)
            std::fill(b.begin(), b.end(), 0.0f);
        position = 0;
        hopCounter = 0;
    }

    /**
     * Processes `main` in place (it comes out delayed by getLatency()) and analyses `keys` (null pointers read as
     * silence). onFrame(Frame&) runs once per hop.
     */
    template <class OnFrame>
    void process(float* const* main, int numMain, const float* const* keys, int numKeys, int numSamples,
                 OnFrame&& onFrame) noexcept
    {
        if (ffts.empty())
            return;
        numMain = std::clamp(numMain, 0, kMaxChannels);
        numKeys = std::clamp(numKeys, 0, kMaxKeys);
        const auto mask = static_cast<std::size_t>(size - 1);

        int done = 0;
        while (done < numSamples)
        {
            const int n = std::min(hop - hopCounter, numSamples - done);
            for (int ch = 0; ch < numMain; ++ch)
            {
                auto& in = inputs[static_cast<std::size_t>(ch)];
                auto& out = outputs[static_cast<std::size_t>(ch)];
                float* data = main[ch] + done;
                for (int i = 0; i < n; ++i)
                {
                    const auto idx = (position + static_cast<std::size_t>(i)) & mask;
                    in[idx] = data[i];
                    data[i] = out[idx];
                    out[idx] = 0.0f;
                }
            }
            for (int key = 0; key < numKeys; ++key)
            {
                auto& in = inputs[static_cast<std::size_t>(kMaxChannels + key)];
                const float* data = keys[key] != nullptr ? keys[key] + done : nullptr;
                for (int i = 0; i < n; ++i)
                    in[(position + static_cast<std::size_t>(i)) & mask] = data != nullptr ? data[i] : 0.0f;
            }
            position = (position + static_cast<std::size_t>(n)) & mask;
            hopCounter += n;
            done += n;

            if (hopCounter == hop)
            {
                hopCounter = 0;
                runFrame(numMain, numKeys, onFrame);
            }
        }
    }

private:
    static constexpr double kTwoPi = 6.283185307179586;

    /** Windowed frame of inputs a (real part) and b (imaginary part, -1 = silence) -> FFT -> two spectra. */
    void analysePair(std::size_t workIndex, int a, int b, std::complex<float>* specA,
                     std::complex<float>* specB) noexcept
    {
        const auto& win = windows[orderIndex];
        const auto mask = static_cast<std::size_t>(size - 1);
        auto& z = work[workIndex];
        const auto& inA = inputs[static_cast<std::size_t>(a)];
        for (std::size_t n = 0; n < static_cast<std::size_t>(size); ++n)
        {
            const auto idx = (position + n) & mask;
            const float im = b >= 0 ? inputs[static_cast<std::size_t>(b)][idx] : 0.0f;
            z[n] = {inA[idx] * win[n], im * win[n]};
        }
        ffts[orderIndex].forward(z.data());

        const auto bins = static_cast<std::size_t>(size / 2 + 1);
        for (std::size_t k = 0; k < bins; ++k)
        {
            const auto zk = z[k];
            const auto zn = std::conj(z[(static_cast<std::size_t>(size) - k) & mask]);
            specA[k] = 0.5f * (zk + zn);
            if (specB != nullptr)
                specB[k] = std::complex<float>(0.0f, -0.5f) * (zk - zn);
        }
    }

    template <class OnFrame>
    void runFrame(int numMain, int numKeys, OnFrame& onFrame) noexcept
    {
        Frame frame;
        frame.numChannels = numMain;
        frame.numKeys = numKeys;
        frame.numBins = size / 2 + 1;
        frame.size = size;

        if (numMain > 0)
            analysePair(0, 0, numMain > 1 ? 1 : -1, spectra[0].data(), numMain > 1 ? spectra[1].data() : nullptr);
        for (int key = 0; key < numKeys; key += 2)
        {
            const int a = kMaxChannels + key;
            const int b = key + 1 < numKeys ? a + 1 : -1;
            auto* specB = b >= 0 ? spectra[static_cast<std::size_t>(b)].data() : nullptr;
            analysePair(static_cast<std::size_t>(1 + key / 2), a, b, spectra[static_cast<std::size_t>(a)].data(),
                        specB);
        }
        for (int ch = 0; ch < numMain; ++ch)
            frame.channels[static_cast<std::size_t>(ch)] = spectra[static_cast<std::size_t>(ch)].data();
        for (int key = 0; key < numKeys; ++key)
            frame.keys[static_cast<std::size_t>(key)] = spectra[static_cast<std::size_t>(kMaxChannels + key)].data();

        onFrame(frame);

        if (numMain == 0)
            return;
        auto& z = work[0];
        const auto n = static_cast<std::size_t>(size);
        const auto half = n / 2;
        const auto& a = spectra[0];
        const auto& b = spectra[1];
        const bool stereo = numMain > 1;
        for (std::size_t k = 0; k <= half; ++k)
        {
            auto ak = a[k];
            auto bk = stereo ? b[k] : std::complex<float>{};
            if (k == 0 || k == half)
            {
                ak = {ak.real(), 0.0f};
                bk = {bk.real(), 0.0f};
            }
            z[k] = ak + std::complex<float>(-bk.imag(), bk.real());
            if (k > 0 && k < half)
            {
                const auto ac = std::conj(ak);
                const auto bc = std::conj(bk);
                z[n - k] = ac + std::complex<float>(-bc.imag(), bc.real());
            }
        }
        ffts[orderIndex].inverse(z.data());

        const auto& win = windows[orderIndex];
        const auto mask = n - 1;
        constexpr float kOverlapGain = 1.0f / 1.5f;
        auto& outA = outputs[0];
        auto& outB = outputs[1];
        for (std::size_t i = 0; i < n; ++i)
        {
            const auto idx = (position + i) & mask;
            const float w = win[i] * kOverlapGain;
            outA[idx] += z[i].real() * w;
            if (stereo)
                outB[idx] += z[i].imag() * w;
        }
    }

    int firstOrder = 0;
    std::vector<Fft> ffts;
    std::vector<std::vector<float>> windows;
    std::size_t orderIndex = 0;
    int size = 0;
    int hop = 0;
    std::array<std::vector<float>, kMaxChannels + kMaxKeys> inputs;
    std::array<std::vector<float>, kMaxChannels> outputs;
    std::array<std::vector<std::complex<float>>, 1 + kMaxKeys / 2> work;
    std::array<std::vector<std::complex<float>>, kMaxChannels + kMaxKeys> spectra;
    std::size_t position = 0;
    int hopCounter = 0;
};
} // namespace animalaudio::dsp
