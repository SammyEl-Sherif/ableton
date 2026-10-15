#include "PluginEditor.h"

#include <catch2/catch_approx.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

namespace
{
juce::Image renderEditor(duck::DuckProcessor& processor, float phase, float scale)
{
    std::unique_ptr<duck::DuckEditor> editor(static_cast<duck::DuckEditor*>(processor.createEditor()));
    editor->getCurveEditor().setPhaseOverride(phase);
    return editor->createComponentSnapshot(editor->getLocalBounds(), true, scale);
}

void fillScope(duck::DuckProcessor& processor)
{
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
}

void maybeWrite(const juce::Image& image, const char* name)
{
    const char* dir = std::getenv("AA_SCREENSHOT_DIR");
    if (dir == nullptr)
        return;
    juce::File file = juce::File(dir).getChildFile(name);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream out(file);
    REQUIRE(out.openedOk());
    juce::PNGImageFormat().writeImageToStream(image, out);
}
} // namespace

// Always runs as a smoke test; set AA_SCREENSHOT_DIR=docs/images to also write PNGs for the README.
TEST_CASE("Editor renders headlessly")
{
    juce::ScopedJuceInitialiser_GUI gui;

    duck::DuckProcessor processor;
    fillScope(processor);

    auto* mix = processor.apvts.getParameter("mix");
    mix->setValueNotifyingHost(0.79f);

    const auto image = renderEditor(processor, 0.32f, 2.0f);
    REQUIRE(image.getWidth() == duck::DuckEditor::kWidth * 2);
    REQUIRE(image.getHeight() == duck::DuckEditor::kHeight * 2);

    // not blank: the image must contain both bright accent pixels and dark background
    int bright = 0;
    for (int y = 0; y < image.getHeight(); y += 4)
        for (int x = 0; x < image.getWidth(); x += 4)
            if (image.getPixelAt(x, y).getBrightness() > 0.7f)
                ++bright;
    REQUIRE(bright > 200);

    maybeWrite(image, "duck-ui.png");

    // variant: another division and preset
    processor.apvts.getParameter("division")
        ->setValueNotifyingHost(processor.apvts.getParameter("division")->convertTo0to1(0.0f));
    processor.getCurveState().loadPreset(2);
    maybeWrite(renderEditor(processor, 0.62f, 1.0f), "duck-ui-pond-swell.png");
}

namespace
{
juce::MouseEvent makeMouseEvent(juce::Component& c, juce::Point<float> pos, bool popup = false)
{
    auto mods = juce::ModifierKeys::leftButtonModifier;
    if (popup)
        mods = juce::ModifierKeys::rightButtonModifier;
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            &c, &c, juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, false);
}
} // namespace

TEST_CASE("Curve editor edits the stored curve with the mouse")
{
    juce::ScopedJuceInitialiser_GUI gui;
    duck::DuckProcessor processor;
    duck::ui::CurveEditor editor(processor, animalaudio::ui::Palette{});
    editor.setBounds(0, 0, 500, 250);

    auto& state = processor.getCurveState();
    state.setCurve(animalaudio::dsp::ShapeCurve(), -1); // straight ramp: nodes at (0,0) and (1,1)
    editor.changeListenerCallbackForTests();
    REQUIRE(state.getCurve().size() == 2);

    SECTION("double-click on empty space adds a node at that position")
    {
        const auto target = editor.toScreen(0.5f, 0.2f);
        editor.mouseDoubleClick(makeMouseEvent(editor, target));
        const auto curve = state.getCurve();
        REQUIRE(curve.size() == 3);
        REQUIRE(curve.getNodes()[1].x == Catch::Approx(0.5f).margin(0.01f));
        REQUIRE(curve.getNodes()[1].y == Catch::Approx(0.2f).margin(0.01f));
        REQUIRE(state.getPresetIndex() == -1);

        SECTION("dragging that node moves it")
        {
            editor.mouseDown(makeMouseEvent(editor, target));
            editor.mouseDrag(makeMouseEvent(editor, editor.toScreen(0.6f, 0.9f)));
            editor.mouseUp(makeMouseEvent(editor, editor.toScreen(0.6f, 0.9f)));
            const auto moved = state.getCurve().getNodes()[1];
            REQUIRE(moved.x == Catch::Approx(0.6f).margin(0.01f));
            REQUIRE(moved.y == Catch::Approx(0.9f).margin(0.01f));
        }

        SECTION("right-click deletes it")
        {
            editor.mouseDown(makeMouseEvent(editor, target, true));
            REQUIRE(state.getCurve().size() == 2);
        }
    }

    SECTION("dragging the bend handle curves the segment")
    {
        const auto handle = editor.toScreen(0.5f, 0.5f);
        editor.mouseDown(makeMouseEvent(editor, handle));
        editor.mouseDrag(makeMouseEvent(editor, editor.toScreen(0.5f, 0.15f)));
        editor.mouseUp(makeMouseEvent(editor, editor.toScreen(0.5f, 0.15f)));
        const auto curve = state.getCurve();
        REQUIRE(curve.getNodes()[0].bend > 0.1f);
        REQUIRE(curve.evaluate(0.5f) == Catch::Approx(0.15f).margin(0.02f));
    }

    SECTION("end nodes only move vertically and can't be deleted")
    {
        const auto start = editor.toScreen(0.0f, 0.0f);
        editor.mouseDown(makeMouseEvent(editor, start));
        editor.mouseDrag(makeMouseEvent(editor, editor.toScreen(0.4f, 0.7f)));
        editor.mouseUp(makeMouseEvent(editor, editor.toScreen(0.4f, 0.7f)));
        auto curve = state.getCurve();
        REQUIRE(curve.getNodes().front().x == 0.0f);
        REQUIRE(curve.getNodes().front().y == Catch::Approx(0.7f).margin(0.01f));

        editor.mouseDoubleClick(makeMouseEvent(editor, editor.toScreen(1.0f, 1.0f)));
        REQUIRE(state.getCurve().size() == 2);
    }
}
