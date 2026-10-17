#include <animalaudio/dsp/Spectrum.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using animalaudio::dsp::SpectrumAnalyser;
using Catch::Approx;

namespace
{
constexpr double kRate = 48000.0;

std::vector<float> sine(double frequency, int n, float amplitude)
{
    std::vector<float> v(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i)
        v[static_cast<std::size_t>(i)] =
            amplitude * static_cast<float>(std::sin(6.283185307179586 * frequency * i / kRate));
    return v;
}
} // namespace

TEST_CASE("SpectrumAnalyser reads a full-scale sine on a bin as 0 dB at that bin")
{
    SpectrumAnalyser analyser(11);
    const int bin = 100;
    const double frequency = analyser.binFrequency(bin, kRate);
    const auto s = sine(frequency, analyser.getSize(), 1.0f);
    analyser.write(s.data(), static_cast<int>(s.size()));
    analyser.compute();

    REQUIRE(analyser.binDb(bin) == Approx(0.0f).margin(0.05f));
    REQUIRE(analyser.binDb(bin + 10) < -60.0f);
    REQUIRE(analyser.binDb(bin - 10) < -60.0f);
    for (int i = 0; i < analyser.getNumBins(); ++i)
        REQUIRE(analyser.binDb(i) <= analyser.binDb(bin));
}

TEST_CASE("SpectrumAnalyser follows the amplitude in dB")
{
    SpectrumAnalyser analyser(11);
    const int bin = 64;
    const auto s = sine(analyser.binFrequency(bin, kRate), analyser.getSize(), 0.1f);
    analyser.write(s.data(), static_cast<int>(s.size()));
    analyser.compute();
    REQUIRE(analyser.binDb(bin) == Approx(-20.0f).margin(0.05f));
}

TEST_CASE("SpectrumAnalyser only analyses the most recent samples")
{
    SpectrumAnalyser analyser(10);
    const auto loud = sine(1000.0, analyser.getSize(), 1.0f);
    const std::vector<float> silence(static_cast<std::size_t>(analyser.getSize()), 0.0f);
    analyser.write(loud.data(), static_cast<int>(loud.size()));
    analyser.write(silence.data(), static_cast<int>(silence.size()));
    analyser.compute();
    for (int i = 0; i < analyser.getNumBins(); ++i)
        REQUIRE(analyser.binDb(i) == SpectrumAnalyser::kFloorDb);
}

TEST_CASE("SpectrumAnalyser::peakDbBetween takes the loudest bin or interpolates inside a bin")
{
    SpectrumAnalyser analyser(11);
    const int bin = 200;
    const double frequency = analyser.binFrequency(bin, kRate);
    const auto s = sine(frequency, analyser.getSize(), 1.0f);
    analyser.write(s.data(), static_cast<int>(s.size()));
    analyser.compute();

    REQUIRE(analyser.peakDbBetween(frequency * 0.8, frequency * 1.2, kRate) == analyser.binDb(bin));

    const double binWidth = kRate / analyser.getSize();
    const float between = analyser.peakDbBetween(frequency + 0.2 * binWidth, frequency + 0.3 * binWidth, kRate);
    REQUIRE(between < analyser.binDb(bin));
    REQUIRE(between > analyser.binDb(bin + 1));
}
