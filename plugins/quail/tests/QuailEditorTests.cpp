#include "QuailEditor.h"
#include "QuailProcessor.h"
#include "ui/QuailTheme.h"

#include <animalaudio/testing/RealMouseSuite.h>
#include <animalaudio/testing/ShaperPluginSuite.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace quail;
using animalaudio::testing::click;
using dsp::FilterShape;

namespace
{
constexpr double kRate = 48000.0;
constexpr double kTwoPi = 6.283185307179586;

struct Fixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    QuailProcessor processor;
    std::unique_ptr<QuailEditor> editor;

    Fixture()
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.inputBuses.add(juce::AudioChannelSet::stereo());
        layout.outputBuses.add(juce::AudioChannelSet::stereo());
        processor.setBusesLayout(layout);
        processor.setRateAndBufferSizeDetails(kRate, 512);
        processor.prepareToPlay(kRate, 512);
        open();
    }

    void open() { editor.reset(static_cast<QuailEditor*>(processor.createEditor())); }

    ui::EqDisplay& display() { return editor->getDisplay(); }
    ui::BandPanel& panel() { return editor->getBandPanel(); }
    ui::BandActions& actions() { return editor->getActions(); }
    float value(int band, BandParam param) const { return processor.params().value(band, param); }
    dsp::BandSettings settings(int band) const { return processor.getSettings().bands[static_cast<std::size_t>(band)]; }

    /** Pushes audio through the processor (main + side chain) and lets the editor take a frame, `frames` times. */
    void play(int frames, const std::function<float(int, int)>& sample)
    {
        juce::MidiBuffer midi;
        int t = 0;
        for (int frame = 0; frame < frames; ++frame)
        {
            for (int block = 0; block < 4; ++block)
            {
                juce::AudioBuffer<float> buffer(4, 512);
                for (int ch = 0; ch < 4; ++ch)
                    for (int i = 0; i < 512; ++i)
                        buffer.setSample(ch, i, sample(ch, t + i));
                processor.processBlock(buffer, midi);
                t += 512;
            }
            editor->refreshNow();
        }
    }
};

juce::MouseEvent eventAt(juce::Component& c, juce::Point<float> position,
                         juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier, int clicks = 1)
{
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), position, mods, 1.0f, 0.0f, 0.0f, 0.0f,
                            0.0f, &c, &c, juce::Time::getCurrentTime(), position, juce::Time::getCurrentTime(), clicks,
                            false);
}

struct GestureCounter : juce::AudioProcessorListener
{
    int begins = 0;
    int ends = 0;
    void audioProcessorParameterChanged(juce::AudioProcessor*, int, float) override {}
    void audioProcessorChanged(juce::AudioProcessor*, const ChangeDetails&) override {}
    void audioProcessorParameterChangeGestureBegin(juce::AudioProcessor*, int) override { ++begins; }
    void audioProcessorParameterChangeGestureEnd(juce::AudioProcessor*, int) override { ++ends; }
};

void drag(juce::Component& c, juce::Point<float> from, juce::Point<float> to,
          juce::ModifierKeys mods = juce::ModifierKeys::leftButtonModifier)
{
    c.mouseDown(eventAt(c, from, mods));
    c.mouseDrag(eventAt(c, from + (to - from) * 0.5f, mods));
    c.mouseDrag(eventAt(c, to, mods));
    c.mouseUp(eventAt(c, to, mods));
}

/** A tone at `hz` plus a little noise, for the analyser. */
float musicSample(int t, double hz, float amplitude)
{
    const auto noise = static_cast<float>((static_cast<unsigned>(t) * 2654435761u) >> 8) / 16777216.0f - 0.5f;
    return amplitude * static_cast<float>(std::sin(kTwoPi * hz * t / kRate)) + 0.05f * noise;
}
} // namespace

TEST_CASE("quail: editor: the build stamp is shown")
{
    animalaudio::testing::suite::buildStampIsShown<QuailProcessor, QuailEditor>();
}

TEST_CASE("quail: editor: double-click adds a band where you click and removes it again")
{
    Fixture f;
    auto& d = f.display();
    REQUIRE(d.getWidth() > 600);
    REQUIRE_FALSE(f.panel().isVisible());

    d.mouseDoubleClick(eventAt(d, {d.xForFrequency(1000.0), d.yForDb(6.0)}));
    REQUIRE(f.settings(0).used);
    REQUIRE(f.settings(0).shape == FilterShape::Bell);
    REQUIRE(f.value(0, BandParam::frequency) == Catch::Approx(1000.0f).epsilon(0.01));
    REQUIRE(f.value(0, BandParam::gain) == Catch::Approx(6.0f).margin(0.06));
    REQUIRE(d.getSelectedBand() == 0);
    REQUIRE(f.panel().isVisible());
    REQUIRE(f.panel().getBand() == 0);

    // the extremes make cuts
    d.mouseDoubleClick(eventAt(d, {d.xForFrequency(18.0), d.yForDb(0.0)}));
    REQUIRE(f.settings(1).shape == FilterShape::LowCut);
    d.mouseDoubleClick(eventAt(d, {d.xForFrequency(20000.0), d.yForDb(0.0)}));
    REQUIRE(f.settings(2).shape == FilterShape::HighCut);

    d.mouseDoubleClick(eventAt(d, d.nodePosition(0)));
    REQUIRE_FALSE(f.settings(0).used);
    REQUIRE(f.value(0, BandParam::gain) == 0.0f);
    REQUIRE(f.actions().numUsed() == 2);
}

TEST_CASE("quail: editor: dragging a node moves frequency and gain as host gestures")
{
    Fixture f;
    auto& d = f.display();
    const int band = f.actions().create(500.0f, 0.0f, FilterShape::Bell);
    GestureCounter counter;
    f.processor.addListener(&counter);

    drag(d, d.nodePosition(band), {d.xForFrequency(2000.0), d.yForDb(-6.0)});
    REQUIRE(f.value(band, BandParam::frequency) == Catch::Approx(2000.0f).epsilon(0.02));
    REQUIRE(f.value(band, BandParam::gain) == Catch::Approx(-6.0f).margin(0.11));
    REQUIRE(counter.begins == 2);
    REQUIRE(counter.ends == 2);

    // alt-drag upwards narrows the band
    const float q = f.value(band, BandParam::q);
    const auto node = d.nodePosition(band);
    drag(d, node, node.translated(0.0f, -60.0f),
         juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier | juce::ModifierKeys::altModifier));
    REQUIRE(f.value(band, BandParam::q) == Catch::Approx(q * 2.0f).epsilon(0.02));
    REQUIRE(f.value(band, BandParam::frequency) == Catch::Approx(2000.0f).epsilon(0.02));

    // a cut has no gain: it sits on the 0 dB line at its neutral Q and vertical drags set its resonance
    const int cut = f.actions().create(100.0f, 0.0f, FilterShape::LowCut);
    const auto cutNode = d.nodePosition(cut);
    REQUIRE(cutNode.y == Catch::Approx(d.yForDb(0.0)).margin(0.5));
    drag(d, cutNode, {cutNode.x, d.yForDb(6.0)});
    REQUIRE(f.value(cut, BandParam::q) == Catch::Approx(2.0f).epsilon(0.03));
    f.processor.removeListener(&counter);
}

TEST_CASE("quail: editor: the diamond sets a band's dynamic range")
{
    Fixture f;
    auto& d = f.display();
    const int band = f.actions().create(800.0f, 3.0f, FilterShape::Bell);
    REQUIRE_FALSE(d.hasRangeHandle(band));
    f.actions().set(band, BandParam::range, -6.0f);
    REQUIRE(d.hasRangeHandle(band));
    const auto handle = d.rangeHandlePosition(band);
    REQUIRE(handle.y == Catch::Approx(d.yForDb(-3.0)).margin(0.5));
    REQUIRE(d.rangeHandleAt(handle) == band);

    drag(d, handle, {handle.x, d.yForDb(-9.0)});
    REQUIRE(f.value(band, BandParam::range) == Catch::Approx(-12.0f).margin(0.11));
    REQUIRE(f.value(band, BandParam::gain) == Catch::Approx(3.0f));
}

TEST_CASE("quail: editor: the band panel follows the selection and edits that band")
{
    Fixture f;
    auto& d = f.display();
    auto& panel = f.panel();
    f.actions().create(1000.0f, 0.0f, FilterShape::Bell);
    f.actions().create(5000.0f, -3.0f, FilterShape::HighShelf);

    d.select(1);
    REQUIRE(panel.isVisible());
    REQUIRE(panel.getBand() == 1);
    REQUIRE(panel.getKnob(BandParam::frequency).getValue() == Catch::Approx(5000.0).epsilon(1e-3));
    REQUIRE(panel.getKnob(BandParam::gain).getValue() == Catch::Approx(-3.0));
    REQUIRE(panel.getMenu(BandParam::shape).getIndex() == static_cast<int>(FilterShape::HighShelf));

    panel.getKnob(BandParam::gain).setValue(4.5, juce::sendNotificationSync);
    REQUIRE(f.value(1, BandParam::gain) == Catch::Approx(4.5f));
    REQUIRE(f.value(0, BandParam::gain) == 0.0f);

    d.select(0);
    REQUIRE(panel.getBand() == 0);
    REQUIRE(panel.getKnob(BandParam::gain).getValue() == Catch::Approx(0.0));

    // the arrows walk through the bands in use, wrapping around
    f.actions().create(200.0f, 0.0f, FilterShape::Bell);
    click(panel.getNextButton());
    REQUIRE(d.getSelectedBand() == 1);
    click(panel.getNextButton());
    REQUIRE(d.getSelectedBand() == 2);
    click(panel.getNextButton());
    REQUIRE(d.getSelectedBand() == 0);
    click(panel.getPreviousButton());
    REQUIRE(d.getSelectedBand() == 2);
    REQUIRE(panel.getBand() == 2);

    click(panel.getRemoveButton());
    REQUIRE_FALSE(f.settings(2).used);
    REQUIRE(f.settings(1).used);
    REQUIRE(d.getSelectedBand() == -1);
    REQUIRE_FALSE(panel.isVisible());
}

TEST_CASE("quail: editor: DYNAMIC, SIDE CHAIN and SPECTRAL switch dynamics on with a starting range")
{
    Fixture f;
    auto& panel = f.panel();
    const int band = f.actions().create(2000.0f, 0.0f, FilterShape::Bell);
    f.display().select(band);

    click(panel.getToggle(BandParam::dynamics));
    REQUIRE(f.settings(band).hasDynamics());
    REQUIRE(f.value(band, BandParam::range) == Catch::Approx(-6.0f));
    REQUIRE(panel.getToggle(BandParam::dynamics).getToggleState());
    click(panel.getToggle(BandParam::dynamics));
    REQUIRE_FALSE(f.settings(band).hasDynamics());

    click(panel.getToggle(BandParam::spectral));
    REQUIRE(f.settings(band).isSpectral());
    f.processor.updateLatency();
    REQUIRE(f.processor.getLatencySamples() == 4096);

    click(panel.getToggle(BandParam::sidechain));
    REQUIRE(f.settings(band).externalSidechain);
    REQUIRE(panel.getToggle(BandParam::sidechain).getToggleState());
}

TEST_CASE("quail: editor: the shape decides which controls are live")
{
    Fixture f;
    auto& panel = f.panel();
    const int band = f.actions().create(300.0f, 0.0f, FilterShape::Bell);
    f.display().select(band);

    panel.getMenu(BandParam::shape).choose(static_cast<int>(FilterShape::LowCut));
    REQUIRE(f.settings(band).shape == FilterShape::LowCut);
    panel.refresh();
    REQUIRE_FALSE(panel.getKnob(BandParam::gain).isEnabled());
    REQUIRE(panel.getKnob(BandParam::q).isEnabled());
    REQUIRE_FALSE(panel.getToggle(BandParam::dynamics).isEnabled());

    panel.getMenu(BandParam::slope).choose(0); // 6 dB/oct: no resonance
    panel.refresh();
    REQUIRE_FALSE(panel.getKnob(BandParam::q).isEnabled());

    panel.getMenu(BandParam::shape).choose(static_cast<int>(FilterShape::FlatTilt));
    panel.refresh();
    REQUIRE(panel.getKnob(BandParam::gain).isEnabled());
    REQUIRE_FALSE(panel.getMenu(BandParam::slope).isEnabled());
    REQUIRE_FALSE(panel.getToggle(BandParam::spectral).isEnabled());

    panel.getMenu(BandParam::shape).choose(static_cast<int>(FilterShape::HighShelf));
    panel.refresh();
    REQUIRE(panel.getToggle(BandParam::dynamics).isEnabled());
    REQUIRE(panel.getToggle(BandParam::spectral).isEnabled());
    REQUIRE(panel.getMenu(BandParam::placement).isEnabled());
}

TEST_CASE("quail: editor: lasso, select all and delete")
{
    Fixture f;
    auto& d = f.display();
    for (float hz : {100.0f, 1000.0f, 10000.0f})
        f.actions().create(hz, 0.0f, FilterShape::Bell);

    drag(d, {d.xForFrequency(50.0), d.yForDb(3.0)}, {d.xForFrequency(2000.0), d.yForDb(-3.0)});
    REQUIRE(d.isSelected(0));
    REQUIRE(d.isSelected(1));
    REQUIRE_FALSE(d.isSelected(2));

    // dragging one of a selection moves all of it
    drag(d, d.nodePosition(1), d.nodePosition(1).translated(0.0f, d.yForDb(4.0) - d.yForDb(0.0)));
    REQUIRE(f.value(0, BandParam::gain) == Catch::Approx(4.0f).margin(0.11));
    REQUIRE(f.value(1, BandParam::gain) == Catch::Approx(4.0f).margin(0.11));
    REQUIRE(f.value(2, BandParam::gain) == 0.0f);

    REQUIRE(d.keyPressed(juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0)));
    REQUIRE(d.numSelected() == 3);
    REQUIRE(d.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)));
    REQUIRE(f.actions().numUsed() == 0);
    REQUIRE(d.getSelectedBand() == -1);
}

TEST_CASE("quail: editor: SOLO auditions the band while it is held")
{
    Fixture f;
    const int band = f.actions().create(700.0f, 0.0f, FilterShape::Bell);
    f.display().select(band);
    auto& solo = f.panel().getSoloButton();
    solo.setState(juce::Button::buttonDown);
    REQUIRE(f.processor.getSoloBand() == band);
    solo.setState(juce::Button::buttonNormal);
    REQUIRE(f.processor.getSoloBand() == -1);
}

TEST_CASE("quail: editor: display options and the window size are saved")
{
    Fixture f;
    REQUIRE(f.editor->getWidth() == QuailEditor::kDefaultWidth);
    click(f.editor->getAnalyzerButton(0));
    REQUIRE(f.display().getAnalyzer().pre);
    click(f.editor->getRangeButton());
    REQUIRE(f.display().getRangeDb() == 30.0f);
    f.editor->setSize(1300, 760);

    f.open();
    REQUIRE(f.editor->getWidth() == 1300);
    REQUIRE(f.editor->getHeight() == 760);
    REQUIRE(f.display().getAnalyzer().pre);
    REQUIRE(f.editor->getAnalyzerButton(0).getToggleState());
    REQUIRE(f.display().getRangeDb() == 30.0f);
    REQUIRE(f.editor->getRangeButton().getButtonText().contains("30"));
}

TEST_CASE("quail: editor: the bottom and top bar controls are the global parameters")
{
    Fixture f;
    f.editor->getOutputChip().setValue(-6.0, juce::sendNotificationSync);
    REQUIRE(f.processor.getSettings().global.outputDb == Catch::Approx(-6.0f));
    f.editor->getPanChip().setValue(-40.0, juce::sendNotificationSync);
    REQUIRE(f.processor.getSettings().global.pan == Catch::Approx(-0.4f));
    f.editor->getScaleChip().setValue(50.0, juce::sendNotificationSync);
    REQUIRE(f.processor.getSettings().global.gainScale == Catch::Approx(0.5f));
    click(f.editor->getBypassButton());
    REQUIRE(f.processor.getSettings().global.bypass);
    click(f.editor->getAutoGainButton());
    REQUIRE(f.processor.getSettings().global.autoGain);
    click(f.editor->getPhaseButton());
    REQUIRE(f.processor.getSettings().global.phaseInvert);
    click(f.editor->getResolutionRow().getPill(2));
    REQUIRE(f.processor.getSettings().global.resolution == dsp::SpectralResolution::High);
}

TEST_CASE("quail: editor: renders headlessly with spectra, curves and the band panel")
{
    Fixture f;
    auto& a = f.actions();
    const auto shaped = [&](float hz, float gain, FilterShape shape, float q, int slopeIndex)
    {
        const int band = a.create(hz, gain, shape);
        a.set(band, BandParam::q, q);
        a.set(band, BandParam::slope, static_cast<float>(slopeIndex));
        return band;
    };
    shaped(30.0f, 0.0f, FilterShape::LowCut, 0.7f, 3);
    shaped(110.0f, 3.5f, FilterShape::LowShelf, 0.7f, 1);
    const int mud = shaped(380.0f, -4.0f, FilterShape::Bell, 1.6f, 1);
    a.set(mud, BandParam::range, -5.0f);
    const int duck = shaped(2600.0f, 1.5f, FilterShape::Bell, 0.6f, 1);
    a.set(duck, BandParam::range, -9.0f);
    a.set(duck, BandParam::sidechain, 1.0f);
    a.set(duck, BandParam::spectral, 1.0f);
    a.set(duck, BandParam::autoThreshold, 0.0f);
    a.set(duck, BandParam::threshold, -48.0f);
    const int air = shaped(9500.0f, 4.0f, FilterShape::HighShelf, 0.7f, 1);
    a.set(air, BandParam::placement, static_cast<float>(dsp::Placement::Side));
    shaped(6200.0f, -6.0f, FilterShape::Bell, 5.0f, 1);
    shaped(19000.0f, 0.0f, FilterShape::HighCut, 0.7f, 1);
    f.processor.updateLatency();
    REQUIRE(f.processor.getLatencySamples() > 0);

    f.display().select(duck);
    f.editor->setTimeOverride(4.0);
    f.play(24,
           [](int ch, int t)
           {
               if (ch < 2)
                   return musicSample(t, ch == 0 ? 220.0 : 330.0, 0.3f) + musicSample(t + 77, 2600.0, 0.12f);
               // a vocal-ish side chain around 2-3 kHz
               return musicSample(t, 2400.0, 0.4f) + musicSample(t + 31, 3100.0, 0.25f);
           });

    REQUIRE(f.processor.getEngine().getSpectral().isDisplayActive(dsp::Placement::Stereo));
    REQUIRE(f.processor.getEngine().getBandDynamicDb(duck) < -0.5f);

    const auto image = f.editor->createComponentSnapshot(f.editor->getLocalBounds(), true, 1.0f);
    int accent = 0, bright = 0;
    const auto colour = f.editor->getTheme().palette.accent;
    for (int y = 0; y < image.getHeight(); y += 2)
        for (int x = 0; x < image.getWidth(); x += 2)
        {
            const auto pixel = image.getPixelAt(x, y);
            bright += pixel.getBrightness() > 0.7f ? 1 : 0;
            accent += animalaudio::testing::realmouse::colourDistance(pixel, colour) < 0.25f ? 1 : 0;
        }
    REQUIRE(bright > 400);
    REQUIRE(accent > 150);
    animalaudio::testing::writePngIfRequested(image, "quail-ui.png");

    // same mix, highlighting a self-triggered band; the panel moves to the top for low nodes
    f.display().select(mud);
    f.editor->refreshNow();
    animalaudio::testing::writePngIfRequested(f.editor->createComponentSnapshot(f.editor->getLocalBounds(), true, 1.0f),
                                              "quail-ui-dynamic.png");
}

TEST_CASE("quail: editor: an empty Quail invites a first band")
{
    Fixture f;
    f.editor->setTimeOverride(2.0);
    f.editor->refreshNow();
    const auto image = f.editor->createComponentSnapshot(f.editor->getLocalBounds(), true, 1.0f);
    REQUIRE(image.getWidth() == QuailEditor::kDefaultWidth);
}

TEST_CASE("quail: real mouse: double-click and drag through the native window at host scales")
{
    using namespace animalaudio::testing::realmouse;
    if (!animalaudio::testing::prepareNativeWindows())
        SKIP("no usable display (none, or the screen is locked)");
    juce::ScopedJuceInitialiser_GUI gui;
    QuailProcessor processor;
    for (const auto& scenario : {Scenario{ScaleKind::None, 1.0f}, Scenario{ScaleKind::EditorScale, 1.5f},
                                 Scenario{ScaleKind::DesktopScale, 2.0f}})
    {
        INFO(describe(scenario));
        DesktopEditor<QuailEditor> window(processor, scenario.kind, scenario.scale);
        auto& d = window.editor->getDisplay();
        const juce::Point<float> at(d.xForFrequency(800.0), d.yForDb(4.0));
        window.click(window.peerPositionOf(d, at));
        window.click(window.peerPositionOf(d, at));
        pump(50);
        REQUIRE(window.editor->getActions().numUsed() == 1);
        REQUIRE(processor.params().value(0, BandParam::frequency) == Catch::Approx(800.0f).epsilon(0.03));
        REQUIRE(processor.params().value(0, BandParam::gain) == Catch::Approx(4.0f).margin(0.3));

        const auto node = d.nodePosition(0);
        const juce::Point<float> target(d.xForFrequency(1600.0), d.yForDb(-2.0));
        window.send(window.peerPositionOf(d, node), false);
        window.send(window.peerPositionOf(d, node), true);
        window.send(window.peerPositionOf(d, node + (target - node) * 0.5f), true);
        window.send(window.peerPositionOf(d, target), true);
        window.send(window.peerPositionOf(d, target), false);
        REQUIRE(processor.params().value(0, BandParam::frequency) == Catch::Approx(1600.0f).epsilon(0.03));
        REQUIRE(processor.params().value(0, BandParam::gain) == Catch::Approx(-2.0f).margin(0.3));

        window.editor->getActions().removeAll();
        window.parkMouse();
        pump(500); // let the next click not count as a double-click
    }
}
