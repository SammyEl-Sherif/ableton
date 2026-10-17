#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace animalaudio::dsp
{
/**
 * In-place radix-2 complex FFT of size 2^order. Allocates only in the constructor; forward()/inverse() are
 * allocation and lock free, so they can run on the audio thread.
 *
 * forward: X[k] = sum x[n] e^(-2 pi i k n / N)      inverse: x[n] = 1/N sum X[k] e^(+2 pi i k n / N)
 */
class Fft
{
public:
    explicit Fft(int fftOrder)
        : order(fftOrder)
        , size(std::size_t{1} << fftOrder)
        , bitReversed(size)
        , twiddles(size / 2)
    {
        constexpr double twoPi = 6.283185307179586;
        for (std::size_t i = 0; i < size; ++i)
        {
            std::size_t r = 0;
            for (int bit = 0; bit < order; ++bit)
                if ((i >> bit) & 1u)
                    r |= std::size_t{1} << (order - 1 - bit);
            bitReversed[i] = r;
        }
        for (std::size_t i = 0; i < size / 2; ++i)
            twiddles[i] =
                std::polar(1.0f, static_cast<float>(-twoPi * static_cast<double>(i) / static_cast<double>(size)));
    }

    int getOrder() const noexcept { return order; }
    int getSize() const noexcept { return static_cast<int>(size); }

    void forward(std::complex<float>* data) const noexcept
    {
        permute(data);
        butterflies(data, false);
    }

    void inverse(std::complex<float>* data) const noexcept
    {
        permute(data);
        butterflies(data, true);
        const float scale = 1.0f / static_cast<float>(size);
        for (std::size_t i = 0; i < size; ++i)
            data[i] *= scale;
    }

private:
    void permute(std::complex<float>* data) const noexcept
    {
        for (std::size_t i = 0; i < size; ++i)
            if (i < bitReversed[i])
                std::swap(data[i], data[bitReversed[i]]);
    }

    void butterflies(std::complex<float>* data, bool conjugateTwiddles) const noexcept
    {
        for (std::size_t length = 2; length <= size; length <<= 1)
        {
            const std::size_t half = length / 2;
            const std::size_t step = size / length;
            for (std::size_t start = 0; start < size; start += length)
                for (std::size_t j = 0; j < half; ++j)
                {
                    const auto w = conjugateTwiddles ? std::conj(twiddles[j * step]) : twiddles[j * step];
                    const auto u = data[start + j];
                    const auto v = data[start + j + half] * w;
                    data[start + j] = u + v;
                    data[start + j + half] = u - v;
                }
        }
    }

    int order;
    std::size_t size;
    std::vector<std::size_t> bitReversed;
    std::vector<std::complex<float>> twiddles;
};
} // namespace animalaudio::dsp
