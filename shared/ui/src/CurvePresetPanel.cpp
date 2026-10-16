#include <animalaudio/ui/CurvePresetPanel.h>

#include <animalaudio/ui/Drawing.h>

namespace animalaudio::ui
{
class CurvePresetPanel::PresetButton : public juce::Button
{
public:
    PresetButton(const dsp::CurvePreset& preset, const Palette& p)
        : juce::Button(preset.name)
        , colours(p)
        , thumbnail(preset.makeCurve())
    {
        setTooltip(getButtonText());
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        setTriggeredOnMouseDown(true); // see PillButton: do not depend on state re-read from the OS pointer
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
        g.setColour(on ? colours.onAccent : colours.text.withAlpha(0.85f));
        g.strokePath(makeCurvePath(thumbnail, plot, 40),
                     juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    Palette colours;
    dsp::ShapeCurve thumbnail;
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

CurvePresetPanel::CurvePresetPanel(core::CurveState& state, const Theme& theme)
    : curveState(state)
    , palette(theme.palette)
{
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
    flipButton->onClick = [this] { curveState.setCurve(curveState.getCurve().reversed(), -1); };
    invertButton->onClick = [this] { curveState.setCurve(curveState.getCurve().inverted(), -1); };
    for (auto* b : {prevButton.get(), nextButton.get(), flipButton.get(), invertButton.get()})
        addAndMakeVisible(*b);

    const auto& presets = curveState.getPresets();
    for (int i = 0; i < static_cast<int>(presets.size()); ++i)
    {
        auto b = std::make_unique<PresetButton>(presets[static_cast<std::size_t>(i)], palette);
        b->onClick = [this, i] { curveState.loadPreset(i); };
        addAndMakeVisible(*b);
        presetButtons.push_back(std::move(b));
    }

    curveState.addChangeListener(this);
    refresh();
}

juce::Button& CurvePresetPanel::getPresetButton(int index)
{
    return *presetButtons[static_cast<std::size_t>(index)];
}

CurvePresetPanel::~CurvePresetPanel()
{
    curveState.removeChangeListener(this);
}

void CurvePresetPanel::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}

void CurvePresetPanel::refresh()
{
    const int current = curveState.getPresetIndex();
    for (int i = 0; i < getNumPresetButtons(); ++i)
        presetButtons[static_cast<std::size_t>(i)]->setToggleState(i == current, juce::dontSendNotification);
    repaint(nameArea);
}

void CurvePresetPanel::stepPreset(int delta)
{
    const int count = getNumPresetButtons();
    const int current = curveState.getPresetIndex();
    const int next = current < 0 ? (delta > 0 ? 0 : count - 1) : (current + delta + count) % count;
    curveState.loadPreset(next);
}

void CurvePresetPanel::resized()
{
    auto area = getLocalBounds();

    auto nav = area.removeFromTop(30);
    prevButton->setBounds(nav.removeFromLeft(54));
    nav.removeFromLeft(6);
    nextButton->setBounds(nav.removeFromLeft(54));
    invertButton->setBounds(nav.removeFromRight(78));
    nav.removeFromRight(6);
    flipButton->setBounds(nav.removeFromRight(62));
    nameArea = nav.reduced(10, 0);

    area.removeFromTop(8);
    constexpr int columns = 8;
    const int rows = std::max(1, (getNumPresetButtons() + columns - 1) / columns);
    const int cellW = area.getWidth() / columns;
    const int cellH = std::min(40, area.getHeight() / rows);
    for (int i = 0; i < getNumPresetButtons(); ++i)
        presetButtons[static_cast<std::size_t>(i)]->setBounds(area.getX() + (i % columns) * cellW,
                                                              area.getY() + (i / columns) * cellH, cellW, cellH);
}

void CurvePresetPanel::paint(juce::Graphics& g)
{
    const int preset = curveState.getPresetIndex();
    const auto& presets = curveState.getPresets();
    const juce::String name = preset >= 0 && preset < static_cast<int>(presets.size())
                                  ? juce::String(presets[static_cast<std::size_t>(preset)].name)
                                  : juce::String("Custom");
    g.setColour(palette.text);
    g.setFont(makeFont(13.0f));
    g.drawText(name.toUpperCase(), nameArea, juce::Justification::centred, false);
}
} // namespace animalaudio::ui
