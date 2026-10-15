#include "PluginProcessor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;
using namespace animalaudio::dsp;

namespace
{
struct FakePlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> position;
    juce::Optional<PositionInfo> getPosition() const override { return position; }
};

struct ProcessorFixture
{
    duck::DuckProcessor processor;
    FakePlayHead playHead;
    juce::MidiBuffer midi;

    explicit ProcessorFixture(double sampleRate = 48000.0, int blockSize = 512)
    {
        processor.setPlayHead(&playHead);
        processor.setPlayConfigDetails(2, 2, sampleRate, blockSize);
        processor.prepareToPlay(sampleRate, blockSize);
    }

    void setParam(const juce::ParameterID& id, float realValue)
    {
        auto* p = processor.apvts.getParameter(id.getParamID());
        p->setValueNotifyingHost(p->convertTo0to1(realValue));
    }

    juce::AudioBuffer<float> run(int numSamples, int blockSize = 512)
    {
        juce::AudioBuffer<float> all(2, numSamples);
        for (int done = 0; done < numSamples; done += blockSize)
        {
            const int n = std::min(blockSize, numSamples - done);
            juce::AudioBuffer<float> block(2, n);
            block.clear();
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill(block.getWritePointer(ch), 1.0f, n);
            processor.processBlock(block, midi);
            for (int ch = 0; ch < 2; ++ch)
                all.copyFrom(ch, done, block, ch, 0, n);
        }
        return all;
    }
};
} // namespace

TEST_CASE("Processor exposes the expected parameters")
{
    duck::DuckProcessor p;
    REQUIRE(p.apvts.getParameter("division") != nullptr);
    REQUIRE(p.apvts.getParameter("mix") != nullptr);
    REQUIRE(p.apvts.getParameter("fallbackBpm") != nullptr);

    auto* division = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("division"));
    REQUIRE(division != nullptr);
    REQUIRE(division->choices == juce::StringArray{"1/8", "1/4", "1/2", "1/1"});
    REQUIRE(division->getIndex() == 1);
}

TEST_CASE("Free-running fallback ducks at the fallback BPM when the host gives no transport")
{
    ProcessorFixture f;
    f.setParam(duck::ids::mix, 1.0f);
    f.setParam(duck::ids::fallbackBpm, 120.0f);
    f.setParam(duck::ids::division, 1.0f); // 1/4 -> 24000 samples

    const auto out = f.run(48000);
    REQUIRE(f.processor.getSyncSource() == SyncSource::FreeRun);

    // default preset starts fully ducked each beat and recovers
    const float* l = out.getReadPointer(0);
    for (int beat = 1; beat < 2; ++beat)
    {
        REQUIRE(l[beat * 24000 + 300] < 0.15f);
        REQUIRE(l[beat * 24000 - 10] > 0.85f);
    }
}

TEST_CASE("Locks to the host PPQ position while the transport is playing")
{
    ProcessorFixture f;
    f.setParam(duck::ids::mix, 1.0f);
    f.setParam(duck::ids::division, 1.0f);

    juce::AudioPlayHead::PositionInfo info;
    info.setIsPlaying(true);
    info.setBpm(120.0);
    info.setPpqPosition(8.0); // exactly on a beat
    f.playHead.position = info;

    const auto out = f.run(256, 256);
    REQUIRE(f.processor.getSyncSource() == SyncSource::Host);
    REQUIRE(out.getSample(0, 255) < 0.15f); // just after the beat -> ducked

    info.setPpqPosition(8.75);
    f.playHead.position = info;
    const auto later = f.run(256, 256);
    REQUIRE(later.getSample(0, 255) > 0.9f); // 3/4 through the beat -> recovered
}

TEST_CASE("Mix at zero passes audio through untouched")
{
    ProcessorFixture f;
    f.setParam(duck::ids::mix, 0.0f);
    f.processor.prepareToPlay(48000.0, 512); // snap smoothers to the new mix
    const auto out = f.run(4096);
    for (int i = 0; i < 4096; ++i)
        REQUIRE(out.getSample(0, i) == 1.0f);
}

TEST_CASE("Edited curves reach the audio thread")
{
    ProcessorFixture f;
    f.setParam(duck::ids::mix, 1.0f);

    ShapeCurve silent(std::vector<ShapeCurve::Node>{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}});
    f.processor.getCurveState().setCurve(silent, -1);

    const auto out = f.run(8192);
    REQUIRE(out.getSample(0, 8191) == Approx(0.0f).margin(1.0e-3f));
}

TEST_CASE("State (parameters + curve) saves and restores")
{
    juce::MemoryBlock saved;
    ShapeCurve custom(
        std::vector<ShapeCurve::Node>{{0.0f, 0.2f, 0.3f}, {0.4f, 0.9f, -0.5f}, {0.7f, 0.1f, 0.0f}, {1.0f, 0.6f, 0.0f}});
    {
        ProcessorFixture a;
        a.setParam(duck::ids::division, 3.0f);
        a.setParam(duck::ids::mix, 0.37f);
        a.setParam(duck::ids::fallbackBpm, 97.5f);
        a.processor.getCurveState().setCurve(custom, -1);
        a.processor.getStateInformation(saved);
    }

    ProcessorFixture b;
    b.processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

    REQUIRE(b.processor.apvts.getRawParameterValue("division")->load() == Approx(3.0f));
    REQUIRE(b.processor.apvts.getRawParameterValue("mix")->load() == Approx(0.37f).margin(1.0e-3f));
    REQUIRE(b.processor.apvts.getRawParameterValue("fallbackBpm")->load() == Approx(97.5f).margin(0.1f));

    const auto restored = b.processor.getCurveState().getCurve();
    REQUIRE(restored.size() == custom.size());
    REQUIRE(b.processor.getCurveState().getPresetIndex() == -1);
    for (int i = 0; i <= 50; ++i)
    {
        const float x = static_cast<float>(i) / 50.0f;
        REQUIRE(restored.evaluate(x) == Approx(custom.evaluate(x)).margin(1.0e-4f));
    }

    // and the restored curve is what the audio thread uses (curve ends at y=0.6, mix 0.37)
    b.processor.prepareToPlay(48000.0, 512);
    juce::AudioPlayHead::PositionInfo info;
    info.setIsPlaying(true);
    info.setPpqPosition(2.0);
    info.setBpm(120.0);
    b.playHead.position = info;
    b.setParam(duck::ids::division, 1.0f);
    const auto out = b.run(512, 512);
    // curve(0) = 0.2 -> gain = 1 - 0.37 * 0.8 ~ 0.704 once smoothers settle
    REQUIRE(out.getSample(0, 20) > 0.6f);
}

TEST_CASE("State without a Curve (older session) falls back to the default preset")
{
    ProcessorFixture a;
    auto state = a.processor.apvts.copyState();
    state.removeChild(state.getChildWithName(duck::tree::curve), nullptr);
    juce::MemoryBlock block;
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary(*xml, block);

    ProcessorFixture b;
    b.processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(b.processor.getCurveState().getPresetIndex() == 0);
}

TEST_CASE("Garbage state data is ignored")
{
    ProcessorFixture f;
    const char junk[] = "not xml at all";
    f.processor.setStateInformation(junk, sizeof(junk));
    REQUIRE(f.processor.getCurveState().getCurve().size() >= 2);
}
