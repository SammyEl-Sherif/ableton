#include "OwlEditor.h"
#include "OwlProcessor.h"

#include <animalaudio/testing/ShaperPluginSuite.h>

#include <cmath>

using animalaudio::testing::ProcessorHarness;
using Catch::Approx;
using owl::OwlEditor;
using owl::OwlProcessor;
namespace ids = owl::ids;
namespace dsp = owl::dsp;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

using Harness = ProcessorHarness<OwlProcessor>;

float paramValue(OwlProcessor& p, const juce::ParameterID& id)
{
    return p.apvts.getRawParameterValue(id.getParamID())->load();
}

void setParam(OwlProcessor& p, const juce::ParameterID& id, float value)
{
    auto* param = p.apvts.getParameter(id.getParamID());
    param->setValueNotifyingHost(param->convertTo0to1(value));
}

juce::AudioBuffer<float> makeSignal(int numSamples, std::function<float(int)> sampleAt)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    for (int i = 0; i < numSamples; ++i)
    {
        const float s = sampleAt(i);
        buffer.setSample(0, i, s);
        buffer.setSample(1, i, s);
    }
    return buffer;
}

juce::AudioBuffer<float> sine(double hz, int numSamples, float amplitude = 0.25f)
{
    return makeSignal(numSamples,
                      [=](int i) { return amplitude * static_cast<float>(std::sin(kTwoPi * hz * i / kRate)); });
}

juce::AudioBuffer<float> noise(int numSamples, int seed = 3)
{
    juce::Random random(seed);
    return makeSignal(numSamples, [&](int) { return (random.nextFloat() * 2.0f - 1.0f) * 0.5f; });
}

/** Processes `buffer` in place in blocks of `blockSize`. */
void process(OwlProcessor& p, juce::AudioBuffer<float>& buffer, int blockSize = 512,
             std::function<void(int)> beforeBlock = {})
{
    juce::MidiBuffer midi;
    for (int start = 0; start < buffer.getNumSamples(); start += blockSize)
    {
        if (beforeBlock)
            beforeBlock(start);
        const int n = std::min(blockSize, buffer.getNumSamples() - start);
        juce::AudioBuffer<float> block(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, n);
        p.processBlock(block, midi);
    }
}

double rms(const juce::AudioBuffer<float>& b, int from)
{
    return static_cast<double>(b.getRMSLevel(0, from, b.getNumSamples() - from));
}

double measuredGainDb(OwlProcessor& p, double hz)
{
    const auto in = sine(hz, 48000);
    auto out = in;
    process(p, out);
    return 20.0 * std::log10(rms(out, 24000) / rms(in, 24000));
}
} // namespace

TEST_CASE("owl: exposes its parameters with neutral defaults")
{
    OwlProcessor p;
    for (const auto* id : {&ids::lowFreq, &ids::lowGain, &ids::midFreq, &ids::midGain, &ids::midQ, &ids::highFreq,
                           &ids::highGain, &ids::output})
        REQUIRE(p.apvts.getParameter(id->getParamID()) != nullptr);
    REQUIRE(p.getParameters().size() == 8);

    REQUIRE(paramValue(p, ids::lowFreq) == Approx(120.0f));
    REQUIRE(paramValue(p, ids::midFreq) == Approx(1000.0f));
    REQUIRE(paramValue(p, ids::highFreq) == Approx(8000.0f));
    REQUIRE(paramValue(p, ids::midQ) == Approx(1.0f));
    for (const auto* id : {&ids::lowGain, &ids::midGain, &ids::highGain, &ids::output})
    {
        REQUIRE(paramValue(p, *id) == 0.0f);
        auto* param = dynamic_cast<juce::AudioParameterFloat*>(p.apvts.getParameter(id->getParamID()));
        REQUIRE(param->range.start == -owl::kMaxGainDb);
        REQUIRE(param->range.end == owl::kMaxGainDb);
    }
    REQUIRE(p.apvts.getParameter("midFreq")->getText(p.apvts.getParameter("midFreq")->getValue(), 32) == "1.00 kHz");
    REQUIRE(p.apvts.getParameter("lowGain")->getCurrentValueAsText() == "0.0 dB");
}

TEST_CASE("owl: a flat EQ is a bit-exact pass-through")
{
    Harness h(kRate, 512);
    // also after the host moved every gain away and back to 0 dB
    for (float db : {7.3f, 0.0f})
        for (const auto* id : {&ids::lowGain, &ids::midGain, &ids::highGain, &ids::output})
            setParam(h.processor, *id, db);
    auto settle = noise(9600, 5);
    process(h.processor, settle);

    const auto in = noise(20000);
    auto out = in;
    process(h.processor, out, 333);
    for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < in.getNumSamples(); ++i)
            REQUIRE(out.getSample(ch, i) == in.getSample(ch, i));
}

TEST_CASE("owl: every band shapes the sound exactly like the drawn response")
{
    Harness h(kRate, 512);
    setParam(h.processor, ids::lowGain, 9.0f);
    setParam(h.processor, ids::lowFreq, 200.0f);
    setParam(h.processor, ids::midGain, -7.5f);
    setParam(h.processor, ids::midFreq, 1500.0f);
    setParam(h.processor, ids::midQ, 2.0f);
    setParam(h.processor, ids::highGain, 6.0f);
    setParam(h.processor, ids::highFreq, 6000.0f);
    const auto settings = h.processor.getSettings();

    for (double hz : {40.0, 200.0, 700.0, 1500.0, 4000.0, 12000.0})
    {
        INFO(hz << " Hz");
        REQUIRE(measuredGainDb(h.processor, hz) == Approx(dsp::responseDb(settings, hz, kRate)).margin(0.1));
    }
    REQUIRE(dsp::responseDb(settings, 40.0, kRate) > 8.0);
    REQUIRE(dsp::responseDb(settings, 1500.0, kRate) < -6.0);
    REQUIRE(dsp::responseDb(settings, 15000.0, kRate) > 5.0);
}

TEST_CASE("owl: output trims the level")
{
    Harness h(kRate, 512);
    setParam(h.processor, ids::output, -6.0f);
    REQUIRE(measuredGainDb(h.processor, 1000.0) == Approx(-6.0).margin(0.01));
}

TEST_CASE("owl: output does not depend on the host block size")
{
    const auto input = noise(28800, 11);
    const auto render = [&](int blockSize)
    {
        Harness h(kRate, blockSize);
        setParam(h.processor, ids::lowGain, 5.0f);
        setParam(h.processor, ids::midQ, 3.0f);
        h.processor.prepareToPlay(kRate, blockSize);
        auto out = input;
        process(h.processor, out, blockSize,
                [&](int start)
                {
                    if (start == 9600)
                    {
                        setParam(h.processor, ids::midGain, -12.0f);
                        setParam(h.processor, ids::output, 3.0f);
                    }
                });
        return out;
    };
    const auto reference = render(1);
    for (int blockSize : {32, 480, 960})
    {
        const auto out = render(blockSize);
        for (int i = 0; i < input.getNumSamples(); ++i)
            REQUIRE(out.getSample(0, i) == reference.getSample(0, i));
    }
}

TEST_CASE("owl: parameter jumps are click-free")
{
    Harness h(kRate, 256);
    auto signal = sine(1000.0, 96000);
    process(h.processor, signal, 256,
            [&](int start)
            {
                if (start == 24064)
                {
                    setParam(h.processor, ids::midGain, owl::kMaxGainDb);
                    setParam(h.processor, ids::output, 6.0f);
                }
            });

    const auto maxStep = [&](int from, int to)
    {
        float m = 0.0f;
        for (int i = from + 1; i < to; ++i)
            m = std::max(m, std::abs(signal.getSample(0, i) - signal.getSample(0, i - 1)));
        return m;
    };
    const float settled = maxStep(72000, 96000);
    REQUIRE(settled > 0.25f * 0.13f * 7.9f * 1.9f); // +18 dB and +6 dB really happened
    REQUIRE(maxStep(24000, 36000) <= settled * 1.05f);
}

TEST_CASE("owl: state round-trips every parameter")
{
    const std::vector<std::pair<const juce::ParameterID*, float>> values{
        {&ids::lowFreq, 64.0f}, {&ids::lowGain, -4.5f},     {&ids::midFreq, 2400.0f}, {&ids::midGain, 7.2f},
        {&ids::midQ, 3.3f},     {&ids::highFreq, 11000.0f}, {&ids::highGain, -9.9f},  {&ids::output, 2.5f}};

    juce::MemoryBlock saved;
    {
        OwlProcessor a;
        for (const auto& [id, v] : values)
            setParam(a, *id, v);
        a.getStateInformation(saved);
    }
    OwlProcessor b;
    b.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    for (const auto& [id, v] : values)
    {
        INFO(id->getParamID());
        REQUIRE(paramValue(b, *id) == Approx(v).margin(0.02f));
    }
}

TEST_CASE("owl: garbage and foreign state are ignored")
{
    OwlProcessor p;
    setParam(p, ids::midGain, 3.0f);
    const char junk[] = "not xml at all";
    p.setStateInformation(junk, sizeof(junk));

    juce::MemoryBlock foreign;
    juce::AudioProcessor::copyXmlToBinary(juce::XmlElement("DuckState"), foreign);
    p.setStateInformation(foreign.getData(), static_cast<int>(foreign.getSize()));
    REQUIRE(paramValue(p, ids::midGain) == Approx(3.0f));
}

TEST_CASE("owl: supports mono and stereo, in == out")
{
    OwlProcessor p;
    using Set = juce::AudioChannelSet;
    const auto layout = [](Set in, Set out)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add(in);
        l.outputBuses.add(out);
        return l;
    };
    REQUIRE(p.checkBusesLayoutSupported(layout(Set::mono(), Set::mono())));
    REQUIRE(p.checkBusesLayoutSupported(layout(Set::stereo(), Set::stereo())));
    REQUIRE_FALSE(p.checkBusesLayoutSupported(layout(Set::mono(), Set::stereo())));
    REQUIRE_FALSE(p.checkBusesLayoutSupported(layout(Set::create5point1(), Set::create5point1())));
}

TEST_CASE("owl: processes safely before prepareToPlay")
{
    OwlProcessor p;
    p.setPlayConfigDetails(2, 2, kRate, 64);
    setParam(p, ids::highGain, 12.0f);
    auto buffer = noise(64);
    juce::MidiBuffer midi;
    p.processBlock(buffer, midi);
    for (int i = 0; i < 64; ++i)
        REQUIRE(std::isfinite(buffer.getSample(0, i)));
}

TEST_CASE("owl: the analyser feed receives the processed audio")
{
    Harness h(kRate, 512);
    REQUIRE(h.processor.getAnalyserFifo().getNumReady() == 0);
    auto signal = sine(440.0, 2048);
    process(h.processor, signal);
    REQUIRE(h.processor.getAnalyserFifo().getNumReady() == 2048);

    float first[4];
    h.processor.getAnalyserFifo().pop(first, 4);
    for (int i = 0; i < 4; ++i)
        REQUIRE(first[i] == signal.getSample(0, i));
}

TEST_CASE("owl: build stamp is baked in and shown")
{
    animalaudio::testing::suite::buildStampIsShown<OwlProcessor, OwlEditor>();
}

TEST_CASE("owl: knobs and parameters follow each other")
{
    juce::ScopedJuceInitialiser_GUI gui;
    OwlProcessor p;
    OwlEditor editor(p);

    setParam(p, ids::midFreq, 2500.0f);
    REQUIRE(editor.getBandKnob(dsp::kMid, OwlEditor::Control::Frequency)->getValue() == Approx(2500.0).margin(1.0));
    setParam(p, ids::output, -3.0f);
    REQUIRE(editor.getOutputKnob().getValue() == Approx(-3.0).margin(0.01));

    editor.getBandKnob(dsp::kHigh, OwlEditor::Control::Gain)->setValue(4.0, juce::sendNotificationSync);
    REQUIRE(paramValue(p, ids::highGain) == Approx(4.0f).margin(0.01f));
    editor.getBandKnob(dsp::kMid, OwlEditor::Control::Q)->setValue(2.5, juce::sendNotificationSync);
    REQUIRE(paramValue(p, ids::midQ) == Approx(2.5f).margin(0.01f));
    REQUIRE(editor.getBandKnob(dsp::kLow, OwlEditor::Control::Q) == nullptr);
}

TEST_CASE("owl: band nodes drag, flatten and take the wheel")
{
    juce::ScopedJuceInitialiser_GUI gui;
    OwlProcessor p;
    OwlEditor editor(p);
    auto& view = editor.getResponseView();
    using animalaudio::testing::makeMouseEvent;

    // drag the low node to 400 Hz / +6 dB
    const auto start = view.nodePosition(dsp::kLow);
    REQUIRE(view.bandAt(start) == dsp::kLow);
    const juce::Point<float> target(view.xForFrequency(400.0), view.yForDb(6.0));
    view.mouseDown(makeMouseEvent(view, start));
    view.mouseDrag(makeMouseEvent(view, target));
    view.mouseUp(makeMouseEvent(view, target));
    REQUIRE(paramValue(p, ids::lowFreq) == Approx(400.0f).epsilon(0.02));
    REQUIRE(paramValue(p, ids::lowGain) == Approx(6.0f).margin(0.15f));
    REQUIRE(view.nodePosition(dsp::kLow).getDistanceFrom(target) < 2.0f);

    // gains are clamped to the parameter range
    view.mouseDown(makeMouseEvent(view, view.nodePosition(dsp::kHigh)));
    view.mouseDrag(makeMouseEvent(view, {view.xForFrequency(9000.0), view.yForDb(40.0)}));
    view.mouseUp(makeMouseEvent(view, {view.xForFrequency(9000.0), view.yForDb(40.0)}));
    REQUIRE(paramValue(p, ids::highGain) == Approx(owl::kMaxGainDb));

    // double-click flattens just that band
    view.mouseDoubleClick(makeMouseEvent(view, view.nodePosition(dsp::kLow)));
    REQUIRE(paramValue(p, ids::lowGain) == 0.0f);
    REQUIRE(paramValue(p, ids::highGain) == Approx(owl::kMaxGainDb));

    // wheel over the mid node narrows it; elsewhere it does nothing
    const float q = paramValue(p, ids::midQ);
    juce::MouseWheelDetails wheel{};
    wheel.deltaY = 0.25f;
    view.mouseWheelMove(makeMouseEvent(view, view.nodePosition(dsp::kMid)), wheel);
    REQUIRE(paramValue(p, ids::midQ) > q * 1.3f);
    const float q2 = paramValue(p, ids::midQ);
    view.mouseWheelMove(makeMouseEvent(view, {view.xForFrequency(30.0), view.yForDb(-20.0)}), wheel);
    REQUIRE(paramValue(p, ids::midQ) == q2);

    // clicking empty space changes nothing
    view.mouseDown(makeMouseEvent(view, {view.xForFrequency(5000.0), view.yForDb(-20.0)}));
    view.mouseDrag(makeMouseEvent(view, {view.xForFrequency(50.0), view.yForDb(20.0)}));
    view.mouseUp(makeMouseEvent(view, {view.xForFrequency(50.0), view.yForDb(20.0)}));
    REQUIRE(paramValue(p, ids::lowFreq) == Approx(400.0f).epsilon(0.02));
}

TEST_CASE("owl: FLAT sets every band back to 0 dB")
{
    juce::ScopedJuceInitialiser_GUI gui;
    OwlProcessor p;
    OwlEditor editor(p);
    setParam(p, ids::lowGain, 5.0f);
    setParam(p, ids::midGain, -8.0f);
    setParam(p, ids::highGain, 3.0f);
    setParam(p, ids::output, 2.0f);
    animalaudio::testing::click(editor.getFlatButton());
    REQUIRE(paramValue(p, ids::lowGain) == 0.0f);
    REQUIRE(paramValue(p, ids::midGain) == 0.0f);
    REQUIRE(paramValue(p, ids::highGain) == 0.0f);
    REQUIRE(paramValue(p, ids::output) == Approx(2.0f)); // the trim is not a band
}

TEST_CASE("owl: editor renders headlessly")
{
    juce::ScopedJuceInitialiser_GUI gui;
    OwlProcessor p;
    p.setPlayConfigDetails(2, 2, kRate, 512);
    p.prepareToPlay(kRate, 512);

    const auto render = [&](float scale, bool hoverMid)
    {
        // something with a musical spectrum: a few partials over pink-ish noise
        juce::Random random(42);
        float pink = 0.0f;
        auto signal = makeSignal(8192,
                                 [&](int i)
                                 {
                                     pink = 0.97f * pink + 0.03f * (random.nextFloat() * 2.0f - 1.0f);
                                     const double t = i / kRate;
                                     return 0.3f * static_cast<float>(std::sin(kTwoPi * 110.0 * t)) +
                                            0.12f * static_cast<float>(std::sin(kTwoPi * 330.0 * t)) +
                                            0.06f * static_cast<float>(std::sin(kTwoPi * 1250.0 * t)) +
                                            0.03f * static_cast<float>(std::sin(kTwoPi * 5200.0 * t)) + 2.5f * pink;
                                 });
        process(p, signal);

        std::unique_ptr<OwlEditor> editor(static_cast<OwlEditor*>(p.createEditor()));
        auto& view = editor->getResponseView();
        view.setTimeOverride(1.7);
        view.updateSpectrum();
        if (hoverMid)
            view.mouseMove(animalaudio::testing::makeMouseEvent(view, view.nodePosition(dsp::kMid)));
        return editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
    };

    setParam(p, ids::lowGain, 4.5f);
    setParam(p, ids::lowFreq, 110.0f);
    setParam(p, ids::midGain, -4.0f);
    setParam(p, ids::midFreq, 650.0f);
    setParam(p, ids::midQ, 1.4f);
    setParam(p, ids::highGain, 6.0f);
    setParam(p, ids::highFreq, 7000.0f);

    const auto image = render(2.0f, true);
    REQUIRE(image.getWidth() == OwlEditor::kWidth * 2);
    REQUIRE(image.getHeight() == OwlEditor::kHeight * 2);
    int bright = 0;
    for (int y = 0; y < image.getHeight(); y += 4)
        for (int x = 0; x < image.getWidth(); x += 4)
            if (image.getPixelAt(x, y).getBrightness() > 0.7f)
                ++bright;
    REQUIRE(bright > 200);
    animalaudio::testing::writePngIfRequested(image, "owl-ui.png");

    setParam(p, ids::lowGain, -6.0f);
    setParam(p, ids::lowFreq, 300.0f);
    setParam(p, ids::midGain, 8.0f);
    setParam(p, ids::midFreq, 2200.0f);
    setParam(p, ids::midQ, 0.8f);
    setParam(p, ids::highGain, -3.0f);
    setParam(p, ids::output, -2.5f);
    animalaudio::testing::writePngIfRequested(render(1.0f, false), "owl-ui-presence.png");
}
