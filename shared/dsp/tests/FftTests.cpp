#include <animalaudio/dsp/Fft.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <random>
#include <vector>

using animalaudio::dsp::Fft;
using Catch::Approx;

namespace
{
std::vector<std::complex<float>> randomSignal(int n, unsigned seed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    std::vector<std::complex<float>> v(static_cast<std::size_t>(n));
    for (auto& x : v)
        x = {dist(rng), dist(rng)};
    return v;
}
} // namespace

TEST_CASE("Fft matches a direct DFT")
{
    const Fft fft(6);
    const int n = fft.getSize();
    const auto input = randomSignal(n, 3);
    auto data = input;
    fft.forward(data.data());

    for (int k = 0; k < n; ++k)
    {
        std::complex<double> sum;
        for (int i = 0; i < n; ++i)
            sum += std::complex<double>(input[static_cast<std::size_t>(i)]) *
                   std::polar(1.0, -6.283185307179586 * k * i / n);
        REQUIRE(data[static_cast<std::size_t>(k)].real() == Approx(sum.real()).margin(1.0e-4));
        REQUIRE(data[static_cast<std::size_t>(k)].imag() == Approx(sum.imag()).margin(1.0e-4));
    }
}

TEST_CASE("Fft inverse undoes forward")
{
    for (int order : {1, 4, 10, 13})
    {
        const Fft fft(order);
        const auto input = randomSignal(fft.getSize(), static_cast<unsigned>(order));
        auto data = input;
        fft.forward(data.data());
        fft.inverse(data.data());
        for (std::size_t i = 0; i < input.size(); ++i)
            REQUIRE(std::abs(data[i] - input[i]) < 1.0e-5f);
    }
}
