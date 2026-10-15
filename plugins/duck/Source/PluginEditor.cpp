#include "PluginEditor.h"

#include "ui/DuckArt.h"
#include "ui/DuckPalette.h"

namespace duck
{
using animalaudio::ui::makeFont;
using animalaudio::ui::PillButton;

/** Preset slot showing a thumbnail of its curve. */
class DuckEditor::PresetButton : public juce::Button
{
public:
    PresetButton(int presetIndex, const animalaudio::ui::Palette& p)
        : juce::Button(getPresets()[static_cast<std::size_t>(presetIndex)].name)
        , colours(p)
        , thumbnail(getPresets()[static_cast<std::size_t>(presetIndex)].makeCurve())
    {
        setTooltip(getButtonText());
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void paintButton(juce::Graphics& g, bool over, bool down) override
    {
        const auto bounds = getLocalBounds().toFloat().reduced(1.5f);
        const bool on = getToggleState();

        auto fill = on ? colours.accent : colours.panelRaised;
        if (!on && over)
            fill = fill.brighter(0.12f);
        if (down)
            fill = fill.darker(0.1f);
        g.setColour(fill);
        g.fillRoundedRectangle(bounds, 7.0f);

        const auto plot = bounds.reduced(bounds.getWidth() * 0.2f, bounds.getHeight() * 0.24f);
        const auto path = ui::makeCurvePath(thumbnail, plot, 40);
        g.setColour(on ? colours.onAccent : colours.text.withAlpha(0.85f));
        g.strokePath(path, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    animalaudio::ui::Palette colours;
    animalaudio::dsp::ShapeCurve thumbnail;
};

namespace
{
void drawArrow(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, bool pointRight)
{
    const float side = std::min(r.getWidth(), r.getHeight());
    r = juce::Rectangle<float>(side * 0.8f, side).withCentre(r.getCentre());
    juce::Path p;
    if (pointRight)
        p.addTriangle(r.getX(), r.getY(), r.getX(), r.getBottom(), r.getRight(), r.getCentreY());
    else
        p.addTriangle(r.getRight(), r.getY(), r.getRight(), r.getBottom(), r.getX(), r.getCentreY());
    g.setColour(c);
    g.fillPath(p.createPathWithRoundedCorners(2.0f));
}
} // namespace

DuckEditor::DuckEditor(DuckProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , owner(p)
    , palette(ui::makePalette())
    , brand(palette)
    , mixKnob(palette)
    , curveEditor(p, palette)
{
    addAndMakeVisible(brand);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(curveEditor);

    mixKnob.setDoubleClickReturnValue(true, 1.0);
    mixKnob.setTooltip("Mix: how much of the ducking is applied (dry/wet of the effect)");
    mixAttachment =
        std::make_unique<juce::SliderParameterAttachment>(*owner.apvts.getParameter(ids::mix.getParamID()), mixKnob);
    mixKnob.onValueChange = [this] { repaint(); };

    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    bpmSlider.setColour(juce::Slider::trackColourId, palette.accent);
    bpmSlider.setColour(juce::Slider::backgroundColourId, palette.panelRaised);
    bpmSlider.setColour(juce::Slider::thumbColourId, palette.text);
    bpmSlider.setDoubleClickReturnValue(true, 120.0);
    bpmSlider.setTooltip("Tempo used when the host doesn't report one, and while the transport is stopped");
    bpmSlider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(bpmSlider);
    bpmAttachment = std::make_unique<juce::SliderParameterAttachment>(
        *owner.apvts.getParameter(ids::fallbackBpm.getParamID()), bpmSlider);

    for (int i = 0; i < animalaudio::dsp::kNumDivisions; ++i)
    {
        auto b = std::make_unique<PillButton>(
            animalaudio::dsp::divisionLabel(static_cast<animalaudio::dsp::Division>(i)), palette);
        b->setRadioGroupId(1);
        b->onClick = [this, i] { divisionAttachment->setValueAsCompleteGesture(static_cast<float>(i)); };
        b->setTooltip("Duck once per note division, synced to the host tempo");
        addAndMakeVisible(*b);
        divisionButtons[static_cast<std::size_t>(i)] = std::move(b);
    }
    divisionAttachment = std::make_unique<juce::ParameterAttachment>(
        *owner.apvts.getParameter(ids::division.getParamID()), [this](float) { refreshToggles(); });

    prevButton = std::make_unique<PillButton>("Prev", palette);
    nextButton = std::make_unique<PillButton>("Next", palette);
    flipButton = std::make_unique<PillButton>("FLIP", palette);
    invertButton = std::make_unique<PillButton>("INVERT", palette);
    prevButton->glyph = [](juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { drawArrow(g, r, c, false); };
    nextButton->glyph = [](juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c) { drawArrow(g, r, c, true); };
    prevButton->onClick = [this] { stepPreset(-1); };
    nextButton->onClick = [this] { stepPreset(1); };
    flipButton->setTooltip("Mirror the curve in time");
    invertButton->setTooltip("Flip the curve upside down");
    flipButton->onClick = [this]
    {
        auto& s = owner.getCurveState();
        s.setCurve(s.getCurve().reversed(), -1);
    };
    invertButton->onClick = [this]
    {
        auto& s = owner.getCurveState();
        s.setCurve(s.getCurve().inverted(), -1);
    };
    for (auto* b : {prevButton.get(), nextButton.get(), flipButton.get(), invertButton.get()})
        addAndMakeVisible(*b);

    for (int i = 0; i < kNumPresets; ++i)
    {
        auto b = std::make_unique<PresetButton>(i, palette);
        b->onClick = [this, i] { owner.getCurveState().loadPreset(i); };
        addAndMakeVisible(*b);
        presetButtons[static_cast<std::size_t>(i)] = std::move(b);
    }

    owner.getCurveState().addChangeListener(this);
    refreshToggles();

    setSize(kWidth, kHeight);
    startTimerHz(10);
}

DuckEditor::~DuckEditor()
{
    stopTimer();
    owner.getCurveState().removeChangeListener(this);
}

void DuckEditor::refreshToggles()
{
    const int division = juce::roundToInt(owner.apvts.getRawParameterValue(ids::division.getParamID())->load());
    for (int i = 0; i < animalaudio::dsp::kNumDivisions; ++i)
        divisionButtons[static_cast<std::size_t>(i)]->setToggleState(i == division, juce::dontSendNotification);

    const int preset = owner.getCurveState().getPresetIndex();
    for (int i = 0; i < kNumPresets; ++i)
        presetButtons[static_cast<std::size_t>(i)]->setToggleState(i == preset, juce::dontSendNotification);
    repaint(presetNameArea);
}

void DuckEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refreshToggles();
}

void DuckEditor::timerCallback()
{
    const auto source = owner.getSyncSource();
    if (source != shownSource)
    {
        shownSource = source;
        repaint(statusArea);
    }
}

void DuckEditor::stepPreset(int delta)
{
    const int current = owner.getCurveState().getPresetIndex();
    const int next = current < 0 ? (delta > 0 ? 0 : kNumPresets - 1) : (current + delta + kNumPresets) % kNumPresets;
    owner.getCurveState().loadPreset(next);
}

void DuckEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto left = area.removeFromLeft(240);
    area.removeFromLeft(18);

    brand.setBounds(left.removeFromTop(22).withWidth(170));
    left.removeFromTop(78); // title block is painted
    mixKnob.setBounds(left.removeFromTop(228).reduced(6));
    left.removeFromTop(34); // "MIX 79 %" text is painted
    left.removeFromTop(14);
    bpmSlider.setBounds(left.removeFromTop(26).reduced(2, 0));

    divisionRow = area.removeFromTop(34);
    auto row = divisionRow;
    for (auto& b : divisionButtons)
    {
        b->setBounds(row.removeFromLeft(62));
        row.removeFromLeft(8);
    }
    statusArea = row.withTrimmedLeft(10);

    area.removeFromTop(10);
    curveEditor.setBounds(area.removeFromTop(250));

    area.removeFromTop(10);
    auto nav = area.removeFromTop(30);
    prevButton->setBounds(nav.removeFromLeft(54));
    nav.removeFromLeft(6);
    nextButton->setBounds(nav.removeFromLeft(54));
    invertButton->setBounds(nav.removeFromRight(78));
    nav.removeFromRight(6);
    flipButton->setBounds(nav.removeFromRight(62));
    presetNameArea = nav.reduced(10, 0);

    area.removeFromTop(8);
    const int cellW = area.getWidth() / 8;
    const int cellH = juce::jmin(40, area.getHeight() / 2);
    for (int i = 0; i < kNumPresets; ++i)
        presetButtons[static_cast<std::size_t>(i)]->setBounds(area.getX() + (i % 8) * cellW,
                                                              area.getY() + (i / 8) * cellH, cellW, cellH);
}

void DuckEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(palette.background.brighter(0.04f), 0.0f, 0.0f,
                                           palette.background.darker(0.2f), 0.0f, bounds.getBottom(), false));
    g.fillAll();

    // left panel
    const auto leftPanel = juce::Rectangle<float>(8.0f, 8.0f, 262.0f, bounds.getHeight() - 16.0f);
    g.setColour(palette.panel);
    g.fillRoundedRectangle(leftPanel, 14.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(leftPanel, 14.0f);
        g.reduceClipRegion(clip);
        ui::drawWater(g, leftPanel.withTop(leftPanel.getBottom() - 70.0f), ui::waterColour(), 0.0);
        ui::drawFeather(g,
                        juce::Rectangle<float>(leftPanel.getRight() - 62.0f, leftPanel.getY() + 128.0f, 44.0f, 44.0f),
                        palette.accent.withAlpha(0.07f));
        ui::drawFeather(g, juce::Rectangle<float>(leftPanel.getX() + 6.0f, leftPanel.getY() + 244.0f, 36.0f, 36.0f),
                        palette.accentAlt.withAlpha(0.07f));
    }

    // title block
    const auto titleArea = juce::Rectangle<float>(32.0f, 44.0f, 170.0f, 64.0f);
    g.setColour(palette.accent);
    g.setFont(makeFont(60.0f));
    g.drawText("DUCK", titleArea, juce::Justification::centredLeft, false);
    g.setColour(palette.textDim);
    g.setFont(makeFont(11.0f, false));
    g.drawText("VOLUME SHAPER", juce::Rectangle<float>(34.0f, 98.0f, 150.0f, 14.0f), juce::Justification::centredLeft,
               false);
    ui::drawRubberDuck(g, juce::Rectangle<float>(190.0f, 44.0f, 62.0f, 62.0f));

    // MIX read-out
    const float mix = static_cast<float>(mixKnob.getValue());
    const auto mixArea = mixKnob.getBounds().toFloat();
    const auto labelArea =
        juce::Rectangle<float>(mixArea.getX(), mixArea.getBottom() + 2.0f, mixArea.getWidth(), 28.0f);
    g.setFont(makeFont(22.0f));
    g.setColour(palette.textDim);
    const juce::String percent = juce::String(juce::roundToInt(mix * 100.0f)) + "%";
    const float percentW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), percent);
    const float mixW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), "MIX ");
    const float startX = labelArea.getCentreX() - (mixW + percentW) * 0.5f;
    g.drawText("MIX", juce::Rectangle<float>(startX, labelArea.getY(), mixW, labelArea.getHeight()),
               juce::Justification::centredLeft, false);
    g.setColour(palette.accent);
    g.drawText(percent, juce::Rectangle<float>(startX + mixW, labelArea.getY(), percentW + 4.0f, labelArea.getHeight()),
               juce::Justification::centredLeft, false);

    // fallback BPM read-out
    const auto bpmBounds = bpmSlider.getBounds().toFloat();
    g.setFont(makeFont(10.5f));
    g.setColour(palette.textDim);
    g.drawText("FALLBACK BPM", bpmBounds.withY(bpmBounds.getY() - 17.0f).withHeight(14.0f),
               juce::Justification::centredLeft, false);
    g.setColour(palette.text);
    g.drawText(juce::String(bpmSlider.getValue(), 1), bpmBounds.withY(bpmBounds.getY() - 17.0f).withHeight(14.0f),
               juce::Justification::centredRight, false);

    // sync status
    const bool host = shownSource == animalaudio::dsp::SyncSource::Host;
    auto status = statusArea.toFloat().withSizeKeepingCentre(static_cast<float>(statusArea.getWidth()), 24.0f);
    const float dotR = 4.0f;
    g.setColour(host ? palette.accent : palette.textDim);
    g.fillEllipse(status.getX() + 4.0f, status.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);
    g.setFont(makeFont(12.0f));
    g.drawText(host ? "HOST SYNC" : "FREE RUN", status.withTrimmedLeft(20.0f), juce::Justification::centredLeft, false);

    // preset name
    const int preset = owner.getCurveState().getPresetIndex();
    const juce::String name = preset >= 0 && preset < kNumPresets
                                  ? juce::String(getPresets()[static_cast<std::size_t>(preset)].name)
                                  : juce::String("Custom");
    g.setColour(palette.text);
    g.setFont(makeFont(13.0f));
    g.drawText(name.toUpperCase(), presetNameArea, juce::Justification::centred, false);
}
} // namespace duck
