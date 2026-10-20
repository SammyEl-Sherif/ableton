#include "IcePenguinEditor.h"
#include "IcePenguinProcessor.h"
#include "dsp/IceReverb.h"

#include <animalaudio/core/BuildStamp.h>
#include <animalaudio/testing/TestKit.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using animalaudio::testing::ProcessorHarness;
using animalaudio::testing::writePngIfRequested;
using Catch::Approx;

TEST_CASE("icepenguin: parameters exist")
{
    icepenguin::IcePenguinProcessor p;
    REQUIRE(p.apvts.getParameter("preset") != nullptr);
    REQUIRE(p.apvts.getParameter("mix") != nullptr);
    REQUIRE(p.apvts.getParameter("decay") != nullptr);
    REQUIRE(p.apvts.getParameter("tone") != nullptr);
    REQUIRE(p.apvts.getParameter("predelay") != nullptr);
    REQUIRE(p.apvts.getParameter("size") != nullptr);
}

TEST_CASE("icepenguin: reverb presets are named and sane")
{
    const auto& presets = icepenguin::reverbPresets();
    REQUIRE(static_cast<int>(presets.size()) == icepenguin::kNumReverbPresets);
    for (const auto& preset : presets)
    {
        REQUIRE(preset.name.size() > 3);
        REQUIRE(preset.settings.roomSize >= 0.0f);
        REQUIRE(preset.settings.decaySeconds > 0.0f);
    }
}

TEST_CASE("icepenguin: state round trip")
{
    juce::MemoryBlock saved;
    {
        icepenguin::IcePenguinProcessor a;
        a.apvts.getParameter("preset")->setValueNotifyingHost(0.42f);
        a.apvts.getParameter("mix")->setValueNotifyingHost(0.61f);
        a.apvts.getParameter("decay")->setValueNotifyingHost(0.33f);
        a.getStateInformation(saved);
    }
    icepenguin::IcePenguinProcessor b;
    b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    REQUIRE(b.apvts.getRawParameterValue("mix")->load() == Approx(0.61f).margin(0.01f));
    REQUIRE(b.apvts.getRawParameterValue("decay")->load() == Approx(0.33f).margin(0.01f));
}

TEST_CASE("icepenguin: mix zero stays dry")
{
    ProcessorHarness<icepenguin::IcePenguinProcessor> h;
    h.setParam(icepenguin::ids::mix, 0.0f);
    const auto out = h.run(2048);
    for (int i = 512; i < 2048; ++i)
    {
        REQUIRE(out.getSample(0, i) == Approx(1.0f).margin(0.02f));
        REQUIRE(out.getSample(1, i) == Approx(1.0f).margin(0.02f));
    }
}

TEST_CASE("icepenguin: dsp output stays finite")
{
    icepenguin::dsp::IceReverb reverb;
    reverb.prepare(48000.0, 512);
    reverb.setSettings({0.8f, 0.4f, 1.0f, 20.0f, 4.0f});
    reverb.setMix(0.5f);
    std::array<float, 512> l{};
    std::array<float, 512> r{};
    for (int i = 0; i < 512; ++i)
        l[static_cast<std::size_t>(i)] = r[static_cast<std::size_t>(i)] = std::sin(static_cast<float>(i) * 0.07f);
    reverb.process(l.data(), r.data(), 512);
    for (int i = 0; i < 512; ++i)
    {
        REQUIRE(std::isfinite(l[static_cast<std::size_t>(i)]));
        REQUIRE(std::isfinite(r[static_cast<std::size_t>(i)]));
    }
}

TEST_CASE("icepenguin: editor renders headlessly")
{
    juce::ScopedJuceInitialiser_GUI gui;
    icepenguin::IcePenguinProcessor processor;
    processor.setPlayConfigDetails(2, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    juce::MidiBuffer midi;
    for (int block = 0; block < 80; ++block)
    {
        juce::AudioBuffer<float> buffer(2, 512);
        buffer.clear();
        for (int i = 0; i < 512; ++i)
        {
            const float s = 0.4f * std::sin(static_cast<float>(i + block * 512) * 0.03f);
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        processor.processBlock(buffer, midi);
    }

    std::unique_ptr<icepenguin::IcePenguinEditor> editor(
        static_cast<icepenguin::IcePenguinEditor*>(processor.createEditor()));
    const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
    REQUIRE(image.getWidth() == icepenguin::IcePenguinEditor::kWidth * 2);
    REQUIRE(image.getHeight() == icepenguin::IcePenguinEditor::kHeight * 2);

    int bright = 0;
    for (int y = 0; y < image.getHeight(); y += 4)
        for (int x = 0; x < image.getWidth(); x += 4)
            if (image.getPixelAt(x, y).getBrightness() > 0.65f)
                ++bright;
    REQUIRE(bright > 120);
    writePngIfRequested(image, "icepenguin-ui.png");
}

TEST_CASE("icepenguin: build stamp is baked in")
{
    REQUIRE(animalaudio::core::buildStampShort().isNotEmpty());
}
