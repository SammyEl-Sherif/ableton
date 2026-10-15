#pragma once

#include "TestKit.h"

namespace animalaudio::testing::suite
{
using Catch::Approx;
using dsp::ShapeCurve;

template <class P>
void parametersExist()
{
    P p;
    REQUIRE(p.apvts.getParameter("division") != nullptr);
    REQUIRE(p.apvts.getParameter("mix") != nullptr);
    REQUIRE(p.apvts.getParameter("fallbackBpm") != nullptr);

    auto* division = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("division"));
    REQUIRE(division != nullptr);
    REQUIRE(division->choices == juce::StringArray{"1/8", "1/4", "1/2", "1/1"});
    REQUIRE(division->getIndex() == 1);
}

template <class P>
void presetsAreSane()
{
    P p;
    const auto& presets = p.getCurveState().getPresets();
    REQUIRE(static_cast<int>(presets.size()) == dsp::kNumCurvePresets);
    for (const auto& preset : presets)
    {
        const auto curve = preset.makeCurve();
        REQUIRE(curve.size() == preset.nodes.size()); // authored within the curve constraints
        for (int i = 0; i <= 100; ++i)
        {
            const float v = curve.evaluate(static_cast<float>(i) / 100.0f);
            REQUIRE(v >= 0.0f);
            REQUIRE(v <= 1.0f);
        }
    }
}

template <class P>
void stateRoundTrip()
{
    juce::MemoryBlock saved;
    ShapeCurve custom(
        std::vector<ShapeCurve::Node>{{0.0f, 0.2f, 0.3f}, {0.4f, 0.9f, -0.5f}, {0.7f, 0.1f, 0.0f}, {1.0f, 0.6f, 0.0f}});
    {
        ProcessorHarness<P> a;
        a.setParam(core::ids::division, 3.0f);
        a.setParam(core::ids::mix, 0.37f);
        a.setParam(core::ids::fallbackBpm, 97.5f);
        a.processor.getCurveState().setCurve(custom, -1);
        a.processor.getStateInformation(saved);
    }

    ProcessorHarness<P> b;
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
}

template <class P>
void stateWithoutCurveFallsBack()
{
    ProcessorHarness<P> a;
    auto state = a.processor.apvts.copyState();
    state.removeChild(state.getChildWithName(core::tree::curve), nullptr);
    juce::MemoryBlock block;
    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary(*xml, block);

    ProcessorHarness<P> b;
    b.processor.setStateInformation(block.getData(), static_cast<int>(block.getSize()));
    REQUIRE(b.processor.getCurveState().getPresetIndex() == 0);
}

template <class P>
void garbageStateIsIgnored()
{
    ProcessorHarness<P> f;
    const char junk[] = "not xml at all";
    f.processor.setStateInformation(junk, sizeof(junk));
    REQUIRE(f.processor.getCurveState().getCurve().size() >= 2);
}

template <class P>
void divisionParameterRoundTrip()
{
    P p;
    auto* param = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("division"));
    REQUIRE(param != nullptr);
    for (int i = 0; i < dsp::kNumDivisions; ++i)
    {
        const float normalised = param->convertTo0to1(static_cast<float>(i));
        REQUIRE(static_cast<int>(param->convertFrom0to1(normalised)) == i);
        param->setValueNotifyingHost(normalised);
        REQUIRE(param->getIndex() == i);
        REQUIRE(param->getCurrentChoiceName() == juce::String(dsp::divisionLabel(static_cast<dsp::Division>(i))));
        REQUIRE(static_cast<int>(p.apvts.getRawParameterValue("division")->load()) == i);
    }
}

inline void requireOnlyPillSelected(ui::ShaperEditor& editor, int expected)
{
    for (int i = 0; i < dsp::kNumDivisions; ++i)
        REQUIRE(editor.getDivisionButton(i).getToggleState() == (i == expected));
}

template <class P, class E>
void divisionPillsHitTest()
{
    juce::ScopedJuceInitialiser_GUI gui;
    P processor;
    E editor(processor);
    for (int i = 0; i < dsp::kNumDivisions; ++i)
    {
        auto& pill = editor.getDivisionButton(i);
        const auto centre = editor.getLocalArea(&pill, pill.getLocalBounds()).getCentre();
        const auto* hit =
            topmostChildAt(editor.getDivisionRow(), editor.getDivisionRow().getLocalPoint(&editor, centre));
        REQUIRE(hit == &pill);
        for (int j = i + 1; j < dsp::kNumDivisions; ++j)
            REQUIRE_FALSE(pill.getBounds().intersects(editor.getDivisionButton(j).getBounds()));
    }
}

template <class P, class E>
void divisionPillsClick()
{
    juce::ScopedJuceInitialiser_GUI gui;
    P processor;
    E editor(processor);

    for (int from = 0; from < dsp::kNumDivisions; ++from)
    {
        for (int to = 0; to < dsp::kNumDivisions; ++to)
        {
            setDivisionFromHost(processor.apvts, from);
            requireOnlyPillSelected(editor, from);

            click(editor.getDivisionButton(to));

            INFO("from " << from << " to " << to);
            REQUIRE(divisionIndex(processor.apvts) == to);
            requireOnlyPillSelected(editor, to);
        }
    }

    // reflected immediately, not one click late
    for (int i : {0, 1, 2, 3, 3, 2, 1, 0, 2, 0, 3, 1})
    {
        click(editor.getDivisionButton(i));
        requireOnlyPillSelected(editor, i);
    }
}

template <class P, class E>
void divisionPillsFollowAutomationAndState()
{
    juce::ScopedJuceInitialiser_GUI gui;
    {
        P processor;
        E editor(processor);
        for (int i : {0, 1, 2, 3, 2, 0, 3, 1})
        {
            setDivisionFromHost(processor.apvts, i);
            REQUIRE(divisionIndex(processor.apvts) == i);
            requireOnlyPillSelected(editor, i);
        }
    }

    for (int i = 0; i < dsp::kNumDivisions; ++i)
    {
        juce::MemoryBlock saved;
        {
            P source;
            setDivisionFromHost(source.apvts, i);
            source.getStateInformation(saved);
        }
        P restored;
        E editor(restored);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
        REQUIRE(divisionIndex(restored.apvts) == i);
        requireOnlyPillSelected(editor, i);
    }
}

template <class P, class E>
void presetPanelSelectsAndSteps()
{
    juce::ScopedJuceInitialiser_GUI gui;
    P processor;
    E editor(processor);
    auto& panel = editor.getPresetPanel();
    auto& state = processor.getCurveState();

    REQUIRE(panel.getNumPresetButtons() == dsp::kNumCurvePresets);
    for (int i = 0; i < panel.getNumPresetButtons(); ++i)
    {
        click(panel.getPresetButton(i));
        REQUIRE(state.getPresetIndex() == i);
        for (int j = 0; j < panel.getNumPresetButtons(); ++j)
            REQUIRE(panel.getPresetButton(j).getToggleState() == (j == i));
    }

    state.loadPreset(0);
    click(panel.getPrevButton());
    REQUIRE(state.getPresetIndex() == dsp::kNumCurvePresets - 1); // wraps
    click(panel.getNextButton());
    REQUIRE(state.getPresetIndex() == 0);

    click(panel.getFlipButton());
    REQUIRE(state.getPresetIndex() == -1);
}

template <class P, class E>
void curveEditorMouse()
{
    juce::ScopedJuceInitialiser_GUI gui;
    P processor;
    E host(processor);
    ui::CurveEditor editor(processor, ui::Theme{});
    editor.setBounds(0, 0, 500, 250);

    auto& state = processor.getCurveState();
    state.setCurve(ShapeCurve(), -1);
    editor.changeListenerCallbackForTests();
    REQUIRE(state.getCurve().size() == 2);

    // double-click adds a node, drag moves it, right-click removes it
    const auto target = editor.toScreen(0.5f, 0.2f);
    editor.mouseDoubleClick(makeMouseEvent(editor, target));
    REQUIRE(state.getCurve().size() == 3);
    REQUIRE(state.getCurve().getNodes()[1].x == Approx(0.5f).margin(0.01f));
    REQUIRE(state.getCurve().getNodes()[1].y == Approx(0.2f).margin(0.01f));
    REQUIRE(state.getPresetIndex() == -1);

    editor.mouseDown(makeMouseEvent(editor, target));
    editor.mouseDrag(makeMouseEvent(editor, editor.toScreen(0.6f, 0.9f)));
    editor.mouseUp(makeMouseEvent(editor, editor.toScreen(0.6f, 0.9f)));
    REQUIRE(state.getCurve().getNodes()[1].x == Approx(0.6f).margin(0.01f));
    REQUIRE(state.getCurve().getNodes()[1].y == Approx(0.9f).margin(0.01f));

    editor.mouseDown(makeMouseEvent(editor, editor.toScreen(0.6f, 0.9f), true));
    REQUIRE(state.getCurve().size() == 2);

    // bend handle
    editor.mouseDown(makeMouseEvent(editor, editor.toScreen(0.5f, 0.5f)));
    editor.mouseDrag(makeMouseEvent(editor, editor.toScreen(0.5f, 0.15f)));
    editor.mouseUp(makeMouseEvent(editor, editor.toScreen(0.5f, 0.15f)));
    REQUIRE(state.getCurve().getNodes()[0].bend > 0.1f);
    REQUIRE(state.getCurve().evaluate(0.5f) == Approx(0.15f).margin(0.02f));

    // end nodes only move vertically and cannot be deleted
    state.setCurve(ShapeCurve(), -1);
    editor.changeListenerCallbackForTests();
    editor.mouseDown(makeMouseEvent(editor, editor.toScreen(0.0f, 0.0f)));
    editor.mouseDrag(makeMouseEvent(editor, editor.toScreen(0.4f, 0.7f)));
    editor.mouseUp(makeMouseEvent(editor, editor.toScreen(0.4f, 0.7f)));
    REQUIRE(state.getCurve().getNodes().front().x == 0.0f);
    REQUIRE(state.getCurve().getNodes().front().y == Approx(0.7f).margin(0.01f));
    editor.mouseDoubleClick(makeMouseEvent(editor, editor.toScreen(1.0f, 1.0f)));
    REQUIRE(state.getCurve().size() == 2);
}

/** Pushes audio through so the scope has something to show, then renders the editor headlessly.
    Set AA_SCREENSHOT_DIR to also write `<name>-ui.png` and a second preset variant. */
template <class P, class E>
void editorRendersHeadlessly(const juce::String& name)
{
    juce::ScopedJuceInitialiser_GUI gui;
    P processor;
    processor.setPlayConfigDetails(2, 2, 48000.0, 512);
    processor.prepareToPlay(48000.0, 512);
    juce::MidiBuffer midi;
    juce::Random random(42);
    float phase = 0.0f;
    for (int block = 0; block < 400; ++block)
    {
        juce::AudioBuffer<float> buffer(2, 512);
        for (int i = 0; i < 512; ++i)
        {
            phase += 0.045f;
            const float s =
                0.55f * std::sin(phase) + 0.25f * std::sin(phase * 2.7f) + 0.15f * (random.nextFloat() - 0.5f);
            buffer.setSample(0, i, s);
            buffer.setSample(1, i, s);
        }
        processor.processBlock(buffer, midi);
    }

    processor.apvts.getParameter("mix")->setValueNotifyingHost(0.79f);

    const auto render = [&](float playhead, float scale)
    {
        std::unique_ptr<E> editor(static_cast<E*>(processor.createEditor()));
        editor->getCurveEditor().setPhaseOverride(playhead);
        return editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
    };

    const auto image = render(0.32f, 2.0f);
    REQUIRE(image.getWidth() == E::kWidth * 2);
    REQUIRE(image.getHeight() == E::kHeight * 2);

    int bright = 0;
    for (int y = 0; y < image.getHeight(); y += 4)
        for (int x = 0; x < image.getWidth(); x += 4)
            if (image.getPixelAt(x, y).getBrightness() > 0.7f)
                ++bright;
    REQUIRE(bright > 200);
    writePngIfRequested(image, name + "-ui.png");

    auto* division = processor.apvts.getParameter("division");
    division->setValueNotifyingHost(division->convertTo0to1(0.0f));
    processor.getCurveState().loadPreset(2);
    const juce::String presetName = processor.getCurveState().getPresets()[2].name;
    writePngIfRequested(render(0.62f, 1.0f),
                        name + "-ui-" + presetName.toLowerCase().replaceCharacter(' ', '-') + ".png");
}

/** Standard tests for any plugin built on core::CurveShaperProcessor + ui::ShaperEditor. */
#define AA_SHAPER_PLUGIN_TESTS(Processor, Editor, label)                                                               \
    TEST_CASE(label ": exposes the standard parameters")                                                               \
    {                                                                                                                  \
        animalaudio::testing::suite::parametersExist<Processor>();                                                     \
    }                                                                                                                  \
    TEST_CASE(label ": presets are valid")                                                                             \
    {                                                                                                                  \
        animalaudio::testing::suite::presetsAreSane<Processor>();                                                      \
    }                                                                                                                  \
    TEST_CASE(label ": state round-trips parameters and curve")                                                        \
    {                                                                                                                  \
        animalaudio::testing::suite::stateRoundTrip<Processor>();                                                      \
    }                                                                                                                  \
    TEST_CASE(label ": state without a curve falls back to the default preset")                                        \
    {                                                                                                                  \
        animalaudio::testing::suite::stateWithoutCurveFallsBack<Processor>();                                          \
    }                                                                                                                  \
    TEST_CASE(label ": garbage state is ignored")                                                                      \
    {                                                                                                                  \
        animalaudio::testing::suite::garbageStateIsIgnored<Processor>();                                               \
    }                                                                                                                  \
    TEST_CASE(label ": division parameter round-trips for every index")                                                \
    {                                                                                                                  \
        animalaudio::testing::suite::divisionParameterRoundTrip<Processor>();                                          \
    }                                                                                                                  \
    TEST_CASE(label ": division pills do not overlap and are hit individually")                                        \
    {                                                                                                                  \
        animalaudio::testing::suite::divisionPillsHitTest<Processor, Editor>();                                        \
    }                                                                                                                  \
    TEST_CASE(label ": clicking a division pill selects exactly that division")                                        \
    {                                                                                                                  \
        animalaudio::testing::suite::divisionPillsClick<Processor, Editor>();                                          \
    }                                                                                                                  \
    TEST_CASE(label ": division pills follow automation and state restore")                                            \
    {                                                                                                                  \
        animalaudio::testing::suite::divisionPillsFollowAutomationAndState<Processor, Editor>();                       \
    }                                                                                                                  \
    TEST_CASE(label ": preset panel selects, steps and flips")                                                         \
    {                                                                                                                  \
        animalaudio::testing::suite::presetPanelSelectsAndSteps<Processor, Editor>();                                  \
    }                                                                                                                  \
    TEST_CASE(label ": curve editor edits the stored curve with the mouse")                                            \
    {                                                                                                                  \
        animalaudio::testing::suite::curveEditorMouse<Processor, Editor>();                                            \
    }                                                                                                                  \
    TEST_CASE(label ": editor renders headlessly")                                                                     \
    {                                                                                                                  \
        animalaudio::testing::suite::editorRendersHeadlessly<Processor, Editor>(label);                                \
    }

// ---- extra tests for processors whose DSP is dsp::CurveGainShaper (GainShaperProcessor) ----

template <class P>
void freeRunFallback()
{
    ProcessorHarness<P> f;
    f.processor.getCurveState().setCurve(gateCurve(), -1);
    f.setParam(core::ids::mix, 1.0f);
    f.setParam(core::ids::fallbackBpm, 120.0f);
    f.setParam(core::ids::division, 1.0f); // 1/4 note -> 24000 samples per cycle
    const auto out = f.run(48000);
    REQUIRE(f.processor.getSyncSource() == dsp::SyncSource::FreeRun);
    const float* l = out.getReadPointer(0);
    for (int cycle = 0; cycle < 2; ++cycle)
    {
        REQUIRE(l[cycle * 24000 + 300] < 0.05f);   // first half of the cycle is silent
        REQUIRE(l[cycle * 24000 + 12300] > 0.95f); // second half is unity
    }
}

template <class P>
void hostLock()
{
    ProcessorHarness<P> f;
    f.processor.getCurveState().setCurve(gateCurve(), -1);
    f.setParam(core::ids::mix, 1.0f);
    f.setParam(core::ids::division, 1.0f);

    juce::AudioPlayHead::PositionInfo info;
    info.setIsPlaying(true);
    info.setBpm(120.0);
    info.setPpqPosition(8.0); // exactly on a beat
    f.playHead.position = info;
    REQUIRE(f.run(256, 256).getSample(0, 255) < 0.05f);
    REQUIRE(f.processor.getSyncSource() == dsp::SyncSource::Host);

    info.setPpqPosition(8.75); // three quarters through the beat
    f.playHead.position = info;
    REQUIRE(f.run(256, 256).getSample(0, 255) > 0.95f);
}

template <class P>
void mixZeroBypass()
{
    ProcessorHarness<P> f;
    f.setParam(core::ids::mix, 0.0f);
    f.processor.prepareToPlay(48000.0, 512);
    const auto out = f.run(4096);
    for (int i = 0; i < 4096; ++i)
        REQUIRE(out.getSample(0, i) == 1.0f);
}

template <class P>
void restoredCurveReachesAudio()
{
    juce::MemoryBlock saved;
    {
        ProcessorHarness<P> a;
        a.processor.getCurveState().setCurve(
            ShapeCurve(std::vector<ShapeCurve::Node>{{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}}), -1);
        a.setParam(core::ids::mix, 1.0f);
        a.processor.getStateInformation(saved);
    }
    ProcessorHarness<P> b;
    b.processor.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
    b.processor.prepareToPlay(48000.0, 512);
    REQUIRE(b.run(8192).getSample(0, 8191) == Approx(0.0f).margin(1.0e-3f));
}

#define AA_GAIN_SHAPER_PLUGIN_TESTS(Processor, label)                                                                  \
    TEST_CASE(label ": free-runs at the fallback BPM without a transport")                                             \
    {                                                                                                                  \
        animalaudio::testing::suite::freeRunFallback<Processor>();                                                     \
    }                                                                                                                  \
    TEST_CASE(label ": locks to the host PPQ while playing")                                                           \
    {                                                                                                                  \
        animalaudio::testing::suite::hostLock<Processor>();                                                            \
    }                                                                                                                  \
    TEST_CASE(label ": mix at zero is a bit-exact bypass")                                                             \
    {                                                                                                                  \
        animalaudio::testing::suite::mixZeroBypass<Processor>();                                                       \
    }                                                                                                                  \
    TEST_CASE(label ": restored curve reaches the audio thread")                                                       \
    {                                                                                                                  \
        animalaudio::testing::suite::restoredCurveReachesAudio<Processor>();                                           \
    }
} // namespace animalaudio::testing::suite
