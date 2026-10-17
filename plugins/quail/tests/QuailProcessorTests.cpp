#include "QuailProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace quail;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

void setParam(juce::RangedAudioParameter& p, float value)
{
    p.setValueNotifyingHost(p.convertTo0to1(value));
}

void setBand(QuailProcessor& processor, int band, BandParam param, float value)
{
    setParam(processor.params().band(band, param), value);
}

void setGlobal(QuailProcessor& processor, GlobalParam param, float value)
{
    setParam(processor.params().global(param), value);
}

/** Main bus stereo, side chain stereo (enabled or not), prepared at kRate. */
void prepareWithSidechain(QuailProcessor& processor, bool sidechain, int blockSize = 512)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.inputBuses.add(sidechain ? juce::AudioChannelSet::stereo() : juce::AudioChannelSet::disabled());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    REQUIRE(processor.setBusesLayout(layout));
    processor.setRateAndBufferSizeDetails(kRate, blockSize);
    processor.prepareToPlay(kRate, blockSize);
}

double toneDb(const std::vector<float>& x, double hz, std::size_t from, std::size_t to)
{
    double re = 0.0, im = 0.0;
    for (std::size_t i = from; i < to; ++i)
    {
        re += x[i] * std::cos(kTwoPi * hz * static_cast<double>(i) / kRate);
        im += x[i] * std::sin(kTwoPi * hz * static_cast<double>(i) / kRate);
    }
    return 20.0 * std::log10(2.0 * std::sqrt(re * re + im * im) / static_cast<double>(to - from) + 1e-12);
}

/** Feeds `seconds` of a 1 kHz tone (and optionally a 1 kHz side-chain tone); returns the left output. */
std::vector<float> runTone(QuailProcessor& processor, double seconds, float sidechainAmplitude, int blockSize = 512)
{
    const auto total = static_cast<int>(seconds * kRate);
    const int channels = processor.getTotalNumInputChannels();
    std::vector<float> out;
    juce::MidiBuffer midi;
    for (int start = 0; start < total; start += blockSize)
    {
        const int n = std::min(blockSize, total - start);
        juce::AudioBuffer<float> buffer(channels, n);
        for (int i = 0; i < n; ++i)
        {
            const auto t = static_cast<double>(start + i) / kRate;
            const auto tone = static_cast<float>(std::sin(kTwoPi * 1000.0 * t));
            for (int ch = 0; ch < channels; ++ch)
                buffer.setSample(ch, i, ch < 2 ? 0.25f * tone : sidechainAmplitude * tone);
        }
        processor.processBlock(buffer, midi);
        for (int i = 0; i < n; ++i)
            out.push_back(buffer.getSample(0, i));
    }
    return out;
}

void makeDuckingBand(QuailProcessor& processor, int band)
{
    setBand(processor, band, BandParam::used, 1.0f);
    setBand(processor, band, BandParam::frequency, 1000.0f);
    setBand(processor, band, BandParam::range, -12.0f);
    setBand(processor, band, BandParam::autoThreshold, 0.0f);
    setBand(processor, band, BandParam::threshold, -40.0f);
    setBand(processor, band, BandParam::sidechain, 1.0f);
    setBand(processor, band, BandParam::attack, 5.0f);
    setBand(processor, band, BandParam::release, 50.0f);
}
} // namespace

TEST_CASE("quail: parameters: 7 globals and 18 per band for 24 bands, with stable IDs")
{
    QuailProcessor processor;
    REQUIRE(processor.getParameters().size() == kNumGlobalParams + dsp::kNumBands * kNumBandParams);
    // saved-state contract: band IDs count from 1, like the numbers on the nodes
    for (const char* id :
         {"outputGain", "outputPan",  "gainScale", "autoGain", "phaseInvert",  "bypass",   "spectralRes",
          "b1Used",     "b1On",       "b1Freq",    "b1Gain",   "b1Q",          "b1Shape",  "b1Slope",
          "b1Stereo",   "b1Range",    "b1Dyn",     "b1Thresh", "b1AutoThresh", "b1Attack", "b1Release",
          "b1SC",       "b1Spectral", "b1Density", "b1Tilt",   "b24Freq",      "b24Tilt"})
    {
        INFO(id);
        REQUIRE(processor.apvts.getParameter(id) != nullptr);
    }
    REQUIRE(processor.apvts.getParameter("b0Freq") == nullptr);
    REQUIRE(processor.apvts.getParameter("b25Freq") == nullptr);
    REQUIRE(bandParamId(5, BandParam::gain) == "b6Gain");

    // every band starts free and flat, spread over the spectrum
    const auto settings = processor.getSettings();
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        REQUIRE_FALSE(settings.bands[static_cast<std::size_t>(b)].used);
        REQUIRE(settings.bands[static_cast<std::size_t>(b)].gainDb == 0.0f);
        if (b > 0)
            REQUIRE(defaultBandFrequency(b) > defaultBandFrequency(b - 1));
    }
    REQUIRE(settings.global.gainScale == 1.0f);
}

TEST_CASE("quail: gains from the host snap to exact tenths")
{
    QuailProcessor processor;
    auto& gain = processor.params().band(0, BandParam::gain);
    gain.setValueNotifyingHost(0.5f);
    REQUIRE(processor.params().value(0, BandParam::gain) == 0.0f);
    gain.setValueNotifyingHost(gain.convertTo0to1(3.04f));
    REQUIRE(processor.params().value(0, BandParam::gain) == Catch::Approx(3.0f).margin(1e-6));
    REQUIRE(formatHz(1250.0f) == "1.25 kHz");
    REQUIRE(formatHz(80.0f) == "80.0 Hz");
}

TEST_CASE("quail: buses: stereo or mono main, side chain stereo, mono or off")
{
    QuailProcessor processor;
    REQUIRE(processor.getBusCount(true) == 2);
    REQUIRE(processor.getBus(true, 1)->getName() == "Sidechain");

    const auto layout = [](juce::AudioChannelSet main, juce::AudioChannelSet out, juce::AudioChannelSet sidechain)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add(main);
        l.inputBuses.add(sidechain);
        l.outputBuses.add(out);
        return l;
    };
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mono = juce::AudioChannelSet::mono();
    const auto off = juce::AudioChannelSet::disabled();
    REQUIRE(processor.checkBusesLayoutSupported(layout(stereo, stereo, stereo)));
    REQUIRE(processor.checkBusesLayoutSupported(layout(stereo, stereo, mono)));
    REQUIRE(processor.checkBusesLayoutSupported(layout(stereo, stereo, off)));
    REQUIRE(processor.checkBusesLayoutSupported(layout(mono, mono, stereo)));
    REQUIRE_FALSE(processor.checkBusesLayoutSupported(layout(mono, stereo, off)));
    REQUIRE_FALSE(processor.checkBusesLayoutSupported(layout(stereo, stereo, juce::AudioChannelSet::create5point1())));
}

TEST_CASE("quail: the side-chain bus drives a ducking band through processBlock")
{
    QuailProcessor processor;
    prepareWithSidechain(processor, true);
    makeDuckingBand(processor, 3);
    REQUIRE(processor.getTotalNumInputChannels() == 4);

    const auto quiet = runTone(processor, 0.5, 0.0f);
    const auto keyed = runTone(processor, 0.5, 0.5f);
    const auto dry = 20.0 * std::log10(0.25);
    REQUIRE(std::abs(toneDb(quiet, 1000.0, 12000, 24000) - dry) < 0.2);
    REQUIRE(toneDb(keyed, 1000.0, 12000, 24000) - dry < -10.0);
    REQUIRE(processor.isSidechainActive());

    // the host disables the side chain: the band waits for a key that never comes
    QuailProcessor unkeyed;
    prepareWithSidechain(unkeyed, false);
    makeDuckingBand(unkeyed, 3);
    REQUIRE(unkeyed.getTotalNumInputChannels() == 2);
    const auto out = runTone(unkeyed, 0.5, 0.0f, 37);
    REQUIRE(std::abs(toneDb(out, 1000.0, 12000, 24000) - dry) < 0.2);
    REQUIRE_FALSE(unkeyed.isSidechainActive());
}

TEST_CASE("quail: latency follows the spectral bands and the resolution")
{
    QuailProcessor processor;
    prepareWithSidechain(processor, true);
    REQUIRE(processor.getLatencySamples() == 0);

    setBand(processor, 0, BandParam::used, 1.0f);
    setBand(processor, 0, BandParam::range, -6.0f);
    setBand(processor, 0, BandParam::spectral, 1.0f);
    processor.updateLatency();
    REQUIRE(processor.getLatencySamples() == 4096);

    setGlobal(processor, GlobalParam::spectralResolution, 2.0f);
    processor.updateLatency();
    REQUIRE(processor.getLatencySamples() == 8192);
    runTone(processor, 0.1, 0.0f);
    REQUIRE(processor.getEngine().getLatencySamples() == 8192);

    // spectral needs a shape it supports and a dynamic range
    setBand(processor, 0, BandParam::shape, static_cast<float>(dsp::FilterShape::LowCut));
    processor.updateLatency();
    REQUIRE(processor.getLatencySamples() == 0);
    setBand(processor, 0, BandParam::shape, static_cast<float>(dsp::FilterShape::HighShelf));
    processor.updateLatency();
    REQUIRE(processor.getLatencySamples() == 8192);
    setBand(processor, 0, BandParam::range, 0.0f);
    processor.updateLatency();
    REQUIRE(processor.getLatencySamples() == 0);
}

TEST_CASE("quail: state round trip restores bands, globals and the editor state")
{
    juce::MemoryBlock state;
    {
        QuailProcessor processor;
        setBand(processor, 7, BandParam::used, 1.0f);
        setBand(processor, 7, BandParam::shape, static_cast<float>(dsp::FilterShape::HighShelf));
        setBand(processor, 7, BandParam::frequency, 6500.0f);
        setBand(processor, 7, BandParam::gain, -4.5f);
        setBand(processor, 7, BandParam::placement, static_cast<float>(dsp::Placement::Side));
        setBand(processor, 7, BandParam::range, 3.0f);
        setBand(processor, 7, BandParam::spectral, 1.0f);
        setGlobal(processor, GlobalParam::output, -3.0f);
        setGlobal(processor, GlobalParam::autoGain, 1.0f);
        processor.getUiState().setProperty("width", 1234, nullptr);
        processor.getStateInformation(state);
    }
    QuailProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    const auto s = restored.getSettings();
    const auto& b = s.bands[7];
    REQUIRE(b.used);
    REQUIRE(b.shape == dsp::FilterShape::HighShelf);
    REQUIRE(b.frequency == Catch::Approx(6500.0f).epsilon(1e-4));
    REQUIRE(b.gainDb == Catch::Approx(-4.5f));
    REQUIRE(b.placement == dsp::Placement::Side);
    REQUIRE(b.rangeDb == Catch::Approx(3.0f));
    REQUIRE(b.isSpectral());
    REQUIRE(s.global.outputDb == Catch::Approx(-3.0f));
    REQUIRE(s.global.autoGain);
    REQUIRE(static_cast<int>(restored.getUiState().getProperty("width")) == 1234);
    // restoring a spectral band reports its latency straight away
    REQUIRE(restored.getLatencySamples() > 0);
}

TEST_CASE("quail: the bypass parameter is the host's bypass")
{
    QuailProcessor processor;
    REQUIRE(processor.getBypassParameter() == &processor.params().global(GlobalParam::bypass));
}

TEST_CASE("quail: any block size, including 1, with a flat EQ is a bit-exact pass-through")
{
    QuailProcessor processor;
    prepareWithSidechain(processor, true, 64);
    juce::Random random(7);
    juce::MidiBuffer midi;
    for (int n : {1, 2, 64, 3, 512, 1000})
    {
        juce::AudioBuffer<float> buffer(4, n);
        for (int ch = 0; ch < 4; ++ch)
            for (int i = 0; i < n; ++i)
                buffer.setSample(ch, i, random.nextFloat() * 2.0f - 1.0f);
        juce::AudioBuffer<float> copy(buffer);
        processor.processBlock(buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < n; ++i)
                REQUIRE(buffer.getSample(ch, i) == copy.getSample(ch, i));
    }
}
