#include <animalaudio/dsp/Stft.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <vector>

using animalaudio::dsp::StftProcessor;
using Catch::Approx;

namespace
{
constexpr double kTwoPi = 6.283185307179586;

std::vector<float> noise(int n, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<float> v(static_cast<std::size_t>(n));
    for (auto& x : v)
        x = dist(rng);
    return v;
}

std::vector<float> sine(double cyclesPerSample, int n, float amplitude = 0.5f)
{
    std::vector<float> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t>(i)] = amplitude * static_cast<float>(std::sin(kTwoPi * cyclesPerSample * i));
    return v;
}

double rms(const std::vector<float>& v, std::size_t from, std::size_t to)
{
    double sum = 0.0;
    for (std::size_t i = from; i < to; ++i)
        sum += static_cast<double>(v[i]) * v[i];
    return std::sqrt(sum / static_cast<double>(to - from));
}

struct Untouched
{
    int frames = 0;
    void operator()(StftProcessor::Frame&) { ++frames; }
};
} // namespace

TEST_CASE("StftProcessor resynthesises its input delayed by exactly one frame")
{
    for (int order : {8, 10, 12})
        for (int blockSize : {1, 37, 512, 4096})
        {
            StftProcessor stft;
            stft.prepare(order, order);
            REQUIRE(stft.getLatency() == (1 << order));

            const int n = 6 * (1 << order) + 123;
            const auto left = noise(n, 1);
            const auto right = noise(n, 2);
            auto outLeft = left, outRight = right;
            Untouched hook;
            for (int start = 0; start < n; start += blockSize)
            {
                float* channels[] = {outLeft.data() + start, outRight.data() + start};
                stft.process(channels, 2, nullptr, 0, std::min(blockSize, n - start), hook);
            }
            REQUIRE(hook.frames == n / stft.getHop());

            const int latency = stft.getLatency();
            for (int i = 0; i < latency; ++i)
                REQUIRE(std::abs(outLeft[static_cast<std::size_t>(i)]) < 1.0e-6f);
            for (int i = latency; i < n; ++i)
            {
                REQUIRE(outLeft[static_cast<std::size_t>(i)] ==
                        Approx(left[static_cast<std::size_t>(i - latency)]).margin(1.0e-5));
                REQUIRE(outRight[static_cast<std::size_t>(i)] ==
                        Approx(right[static_cast<std::size_t>(i - latency)]).margin(1.0e-5));
            }
        }
}

TEST_CASE("StftProcessor key spectra line up with the processed channels")
{
    StftProcessor stft;
    stft.prepare(9, 9);
    const int size = stft.getSize();
    const int bin = 20;
    const auto signal = sine(static_cast<double>(bin) / size, 4 * size, 1.0f);
    auto mono = signal;

    int frames = 0;
    const auto hook = [&](StftProcessor::Frame& frame)
    {
        REQUIRE(frame.numChannels == 1);
        REQUIRE(frame.numKeys == 3);
        REQUIRE(frame.numBins == size / 2 + 1);
        for (int k = 0; k < frame.numBins; ++k)
        {
            REQUIRE(std::abs(frame.keys[0][k] - frame.channels[0][k]) < 1.0e-3f);
            REQUIRE(std::abs(frame.keys[1][k]) < 1.0e-3f);
            REQUIRE(std::abs(frame.keys[2][k] - frame.channels[0][k]) < 1.0e-3f);
        }
        if (++frames >= 4) // the frame is full of signal from here on
            REQUIRE(std::abs(frame.channels[0][bin]) == Approx(size / 4.0).epsilon(1.0e-3));
    };
    const float* keys[] = {signal.data(), nullptr, signal.data()};
    float* channels[] = {mono.data()};
    stft.process(channels, 1, keys, 3, 4 * size, hook);
    REQUIRE(frames == 16);
}

TEST_CASE("StftProcessor applies per-bin gains")
{
    StftProcessor stft;
    stft.prepare(10, 10);
    const int size = stft.getSize();
    const int n = 12 * size;
    auto low = sine(10.0 / size, n);
    auto high = sine(200.0 / size, n);
    std::vector<float> mix(low.size());
    for (std::size_t i = 0; i < mix.size(); ++i)
        mix[i] = low[i] + high[i];

    const auto removeAbove100 = [](StftProcessor::Frame& frame)
    {
        for (int k = 100; k < frame.numBins; ++k)
            frame.channels[0][k] = {};
    };
    float* channels[] = {mix.data()};
    stft.process(channels, 1, nullptr, 0, n, removeAbove100);

    const auto from = static_cast<std::size_t>(4 * size);
    std::vector<float> difference(mix.size());
    for (std::size_t i = from; i < mix.size(); ++i)
        difference[i] = mix[i] - low[i - static_cast<std::size_t>(size)];
    REQUIRE(rms(difference, from, mix.size()) < 1.0e-3);
}

TEST_CASE("StftProcessor switches frame size without allocating")
{
    StftProcessor stft;
    stft.prepare(8, 11);
    REQUIRE(stft.getOrder() == 11);
    stft.setOrder(9);
    REQUIRE(stft.getLatency() == 512);
    stft.setOrder(20);
    REQUIRE(stft.getOrder() == 11);
    stft.setOrder(3);
    REQUIRE(stft.getOrder() == 8);

    const auto in = noise(2000, 4);
    auto out = in;
    float* channels[] = {out.data()};
    Untouched hook;
    stft.process(channels, 1, nullptr, 0, 2000, hook);
    for (int i = 256; i < 2000; ++i)
        REQUIRE(out[static_cast<std::size_t>(i)] == Approx(in[static_cast<std::size_t>(i - 256)]).margin(1.0e-5));
}
