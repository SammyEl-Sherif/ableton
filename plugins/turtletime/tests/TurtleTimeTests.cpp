#include "TurtleTimeEditor.h"
#include "TurtleTimeProcessor.h"
#include "dsp/TimePatterns.h"
#include "dsp/TurtleTimeEngine.h"

#include <animalaudio/core/BuildStamp.h>
#include <animalaudio/dsp/TempoSync.h>
#include <animalaudio/testing/TestKit.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using animalaudio::testing::ProcessorHarness;
using animalaudio::testing::writePngIfRequested;
using Catch::Approx;

namespace
{
constexpr double kSampleRate = 48000.0;

animalaudio::dsp::TransportInfo playingAt(double bpm, double ppq)
{
    animalaudio::dsp::TransportInfo transport;
    transport.playing = true;
    transport.hasBpm = true;
    transport.bpm = bpm;
    transport.hasPpq = true;
    transport.ppq = ppq;
    return transport;
}

std::vector<float> render(turtletime::dsp::TurtleTimeEngine& engine, const std::vector<float>& input, double bpm,
                          int blockSize, bool host)
{
    std::vector<float> output(input.size());
    const double beatsPerSample = bpm / (60.0 * kSampleRate);
    int done = 0;
    while (done < static_cast<int>(input.size()))
    {
        const int count = std::min(blockSize, static_cast<int>(input.size()) - done);
        std::vector<float> block(input.begin() + done, input.begin() + done + count);
        float* channels[] = {block.data()};
        animalaudio::dsp::TransportInfo transport;
        if (host)
            transport = playingAt(bpm, static_cast<double>(done) * beatsPerSample);
        else
            engine.setFallbackBpm(bpm);
        engine.process(channels, 1, count, transport);
        std::copy(block.begin(), block.end(), output.begin() + done);
        done += count;
    }
    return output;
}

void prepareEngine(turtletime::dsp::TurtleTimeEngine& engine, int pattern, float mix)
{
    engine.setPatternIndex(pattern);
    engine.setMix(mix);
    engine.setFallbackBpm(120.0);
    engine.prepare(kSampleRate, 512);
}
} // namespace

TEST_CASE("turtletime: parameters exist")
{
    turtletime::TurtleTimeProcessor processor;
    REQUIRE(processor.getName() == "Turtle Time");
    REQUIRE(processor.apvts.getParameter("pattern") != nullptr);
    REQUIRE(processor.apvts.getParameter("mix") != nullptr);
    REQUIRE(processor.apvts.getParameter("fallbackBpm") != nullptr);
    REQUIRE(processor.getNumPrograms() == turtletime::dsp::kNumPatterns);
}

TEST_CASE("turtletime: patterns are named")
{
    REQUIRE(static_cast<int>(turtletime::dsp::kPatterns.size()) == turtletime::dsp::kNumPatterns);
    for (const auto& pattern : turtletime::dsp::kPatterns)
    {
        REQUIRE(pattern.name.size() > 2);
        REQUIRE(pattern.blurb.size() > 8);
        REQUIRE(pattern.windowBeats > 0.0);
        REQUIRE(std::isfinite(pattern.rate));
    }
}

TEST_CASE("turtletime: state round trip")
{
    juce::MemoryBlock saved;
    {
        turtletime::TurtleTimeProcessor processor;
        processor.apvts.getParameter("pattern")->setValueNotifyingHost(
            processor.apvts.getParameter("pattern")->convertTo0to1(3.0f));
        processor.apvts.getParameter("mix")->setValueNotifyingHost(0.25f);
        processor.apvts.getParameter("fallbackBpm")
            ->setValueNotifyingHost(processor.apvts.getParameter("fallbackBpm")->convertTo0to1(96.0f));
        processor.getStateInformation(saved);
    }

    turtletime::TurtleTimeProcessor restored;
    restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(restored.apvts.getRawParameterValue("mix")->load() == Approx(0.25f).margin(0.01f));
    REQUIRE(restored.apvts.getRawParameterValue("fallbackBpm")->load() == Approx(96.0f).margin(0.2f));
    REQUIRE(restored.getCurrentProgram() == 3);
}

TEST_CASE("turtletime: programs select patterns")
{
    turtletime::TurtleTimeProcessor processor;
    processor.setCurrentProgram(5);
    REQUIRE(processor.getCurrentProgram() == 5);
    REQUIRE(processor.getProgramName(5) == "Drift");
}

TEST_CASE("turtletime: mix zero stays dry")
{
    ProcessorHarness<turtletime::TurtleTimeProcessor> harness;
    harness.setParam(turtletime::ids::mix, 0.0f);
    harness.processor.prepareToPlay(kSampleRate, 256);

    juce::AudioBuffer<float> buffer(2, 1024);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float sample = std::sin(static_cast<float>(i) * 0.07f);
        buffer.setSample(0, i, sample);
        buffer.setSample(1, i, sample * 0.5f);
    }
    const juce::AudioBuffer<float> input = buffer;
    harness.processor.processBlock(buffer, harness.midi);
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        REQUIRE(buffer.getSample(0, i) == Approx(input.getSample(0, i)).margin(0.0f));
        REQUIRE(buffer.getSample(1, i) == Approx(input.getSample(1, i)).margin(0.0f));
    }
}

TEST_CASE("turtletime: half speed reads the ramp at half rate")
{
    turtletime::dsp::TurtleTimeEngine engine;
    prepareEngine(engine, 3, 1.0f); // Hatch: 0.5x, one beat
    constexpr double bpm = 240.0;
    constexpr int count = 4000;
    std::vector<float> input(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        input[static_cast<std::size_t>(i)] = 0.0002f * static_cast<float>(i);

    const auto output = render(engine, input, bpm, 256, true);
    for (int k = 4; k < 800; k += 2)
        REQUIRE(output[static_cast<std::size_t>(2 * k)] == Approx(input[static_cast<std::size_t>(k)]).margin(1.0e-4f));
}

TEST_CASE("turtletime: half speed drops a sine one octave")
{
    turtletime::dsp::TurtleTimeEngine engine;
    prepareEngine(engine, 3, 1.0f);
    constexpr double bpm = 240.0;
    constexpr int count = 12000;
    constexpr float frequency = 960.0f;
    std::vector<float> input(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        input[static_cast<std::size_t>(i)] = std::sin(2.0f * juce::MathConstants<float>::pi * frequency *
                                                      static_cast<float>(i) / static_cast<float>(kSampleRate));

    const auto output = render(engine, input, bpm, 512, true);
    int crossings = 0;
    for (int i = 3001; i < 9000; ++i)
        if (output[static_cast<std::size_t>(i - 1)] <= 0.0f && output[static_cast<std::size_t>(i)] > 0.0f)
            ++crossings;

    // 480 Hz over 6000 samples at 48 kHz is 60 rising zeros.
    REQUIRE(crossings == Approx(60).margin(2));
}

TEST_CASE("turtletime: reverse plays the previous window backwards")
{
    turtletime::dsp::TurtleTimeEngine engine;
    prepareEngine(engine, 4, 1.0f); // Reverse: -1x, one bar
    constexpr double bpm = 240.0;
    constexpr int window = 48000;
    std::vector<float> input(static_cast<std::size_t>(window * 2));
    for (int i = 0; i < window * 2; ++i)
        input[static_cast<std::size_t>(i)] = std::sin(static_cast<float>(i) * 0.01f);

    const auto output = render(engine, input, bpm, 512, true);
    for (int n = 800; n < window; n += 17)
        REQUIRE(output[static_cast<std::size_t>(window + n)] ==
                Approx(input[static_cast<std::size_t>(window - n)]).margin(1.0e-4f));
}

TEST_CASE("turtletime: repeat retriggers inside the block")
{
    turtletime::dsp::TurtleTimeEngine engine;
    prepareEngine(engine, 6, 1.0f); // Tap: 1/8 note slice
    constexpr double bpm = 1440.0;  // 0.5 beats == 1000 samples
    constexpr int count = 3000;
    std::vector<float> input(static_cast<std::size_t>(count), -0.8f);
    for (int i = 0; i < 1000; ++i)
        input[static_cast<std::size_t>(i)] = 0.8f;

    const auto output = render(engine, input, bpm, 2048, true);
    REQUIRE(output[500] == Approx(0.8f).margin(0.02f));
    REQUIRE(output[1500] == Approx(0.8f).margin(0.02f));
    REQUIRE(output[2500] == Approx(0.8f).margin(0.02f));
}

TEST_CASE("turtletime: jumps stay click-free")
{
    turtletime::dsp::TurtleTimeEngine engine;
    prepareEngine(engine, 3, 1.0f);
    constexpr double bpm = 240.0;
    constexpr int count = 24000; // two 1-beat windows, so the snap is included
    std::vector<float> input(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        input[static_cast<std::size_t>(i)] = 0.8f * std::sin(2.0f * juce::MathConstants<float>::pi * 440.0f *
                                                             static_cast<float>(i) / static_cast<float>(kSampleRate));

    const auto output = render(engine, input, bpm, 300, true);
    float maxDelta = 0.0f;
    for (int i = 1; i < count; ++i)
        maxDelta =
            std::max(maxDelta, std::abs(output[static_cast<std::size_t>(i)] - output[static_cast<std::size_t>(i - 1)]));

    REQUIRE(std::isfinite(maxDelta));
    REQUIRE(maxDelta < 0.2f);
}

TEST_CASE("turtletime: block size does not change the render")
{
    constexpr double bpm = 240.0;
    constexpr int count = 15000;
    std::vector<float> input(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        input[static_cast<std::size_t>(i)] = std::sin(static_cast<float>(i) * 0.017f);

    turtletime::dsp::TurtleTimeEngine fine;
    turtletime::dsp::TurtleTimeEngine coarse;
    prepareEngine(fine, 3, 1.0f);
    prepareEngine(coarse, 3, 1.0f);
    const auto a = render(fine, input, bpm, 1, true);
    const auto b = render(coarse, input, bpm, 173, true);
    for (int i = 0; i < count; i += 10)
        REQUIRE(a[static_cast<std::size_t>(i)] == Approx(b[static_cast<std::size_t>(i)]).margin(0.02f));
}

TEST_CASE("turtletime: free run matches a host at the fallback tempo")
{
    constexpr double bpm = 240.0;
    constexpr int count = 6000;
    std::vector<float> input(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        input[static_cast<std::size_t>(i)] = 0.0002f * static_cast<float>(i);

    turtletime::dsp::TurtleTimeEngine host;
    turtletime::dsp::TurtleTimeEngine freeRun;
    prepareEngine(host, 3, 1.0f);
    prepareEngine(freeRun, 3, 1.0f);
    const auto a = render(host, input, bpm, 128, true);
    const auto b = render(freeRun, input, bpm, 128, false);
    for (int i = 0; i < count; i += 8)
        REQUIRE(a[static_cast<std::size_t>(i)] == Approx(b[static_cast<std::size_t>(i)]).margin(1.0e-4f));
}

TEST_CASE("turtletime: output stays finite")
{
    turtletime::dsp::TurtleTimeEngine engine;
    prepareEngine(engine, 0, 0.7f);
    constexpr int count = 20000;
    std::vector<float> input(static_cast<std::size_t>(count));
    unsigned state = 1u;
    for (int i = 0; i < count; ++i)
    {
        state = state * 1664525u + 1013904223u;
        input[static_cast<std::size_t>(i)] = static_cast<float>(static_cast<int>(state >> 16) % 2000 - 1000) / 1000.0f;
    }
    const auto output = render(engine, input, 120.0, 64, true);
    for (float sample : output)
    {
        REQUIRE(std::isfinite(sample));
        REQUIRE(std::abs(sample) < 4.0f);
    }
}

TEST_CASE("turtletime: editor renders headlessly")
{
    juce::ScopedJuceInitialiser_GUI gui;
    turtletime::TurtleTimeProcessor processor;
    processor.setPlayConfigDetails(2, 2, kSampleRate, 512);
    processor.prepareToPlay(kSampleRate, 512);

    animalaudio::testing::FakePlayHead playHead;
    juce::AudioPlayHead::PositionInfo position;
    position.setIsPlaying(true);
    position.setBpm(120.0);
    position.setPpqPosition(0.0);
    playHead.position = position;
    processor.setPlayHead(&playHead);

    juce::MidiBuffer midi;
    double ppq = 0.0;
    const double beatsPerSample = 120.0 / (60.0 * kSampleRate);
    for (int block = 0; block < 40; ++block)
    {
        position.setPpqPosition(ppq);
        playHead.position = position;
        juce::AudioBuffer<float> buffer(2, 512);
        for (int i = 0; i < 512; ++i)
        {
            const float sample = 0.4f * std::sin(static_cast<float>(i + block * 512) * 0.03f);
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample * 0.8f);
        }
        processor.processBlock(buffer, midi);
        ppq += beatsPerSample * 512.0;
    }
    REQUIRE(processor.isHostSynced());

    std::unique_ptr<turtletime::TurtleTimeEditor> editor(
        static_cast<turtletime::TurtleTimeEditor*>(processor.createEditor()));
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
    REQUIRE(image.getWidth() == turtletime::TurtleTimeEditor::kWidth * 2);
    REQUIRE(image.getHeight() == turtletime::TurtleTimeEditor::kHeight * 2);

    int bright = 0;
    for (int y = 0; y < image.getHeight(); y += 4)
        for (int x = 0; x < image.getWidth(); x += 4)
            if (image.getPixelAt(x, y).getBrightness() > 0.55f)
                ++bright;
    REQUIRE(bright > 80);
    writePngIfRequested(image, "turtletime-ui.png");
}

TEST_CASE("turtletime: build stamp is baked in")
{
    REQUIRE(animalaudio::core::buildStampShort().isNotEmpty());
}
