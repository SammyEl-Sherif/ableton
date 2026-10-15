#include <animalaudio/ui/ShaperEditor.h>

namespace animalaudio::ui
{
namespace
{
juce::AudioParameterChoice& getChoice(core::CurveShaperProcessor& p)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter(core::ids::division.getParamID()));
    jassert(choice != nullptr);
    return *choice;
}
} // namespace

ShaperEditor::ShaperEditor(core::CurveShaperProcessor& p, const Theme& themeToUse)
    : juce::AudioProcessorEditor(&p)
    , owner(p)
    , theme(themeToUse)
    , palette(themeToUse.palette)
    , brand(palette)
    , mixKnob(theme)
    , curveEditor(p, theme)
    , divisionRow(getChoice(p), palette, "Duck once per note division, synced to the host tempo")
    , presetPanel(p.getCurveState(), theme)
{
    addAndMakeVisible(brand);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(curveEditor);
    addAndMakeVisible(divisionRow);
    addAndMakeVisible(presetPanel);

    auto* mixParameter = owner.apvts.getParameter(core::ids::mix.getParamID());
    mixKnob.setTooltip("Mix: how much of the effect is applied (dry/wet of the effect)");
    mixAttachment = std::make_unique<juce::SliderParameterAttachment>(*mixParameter, mixKnob);
    mixKnob.setDoubleClickReturnValue(true, mixParameter->convertFrom0to1(mixParameter->getDefaultValue()));
    mixKnob.onValueChange = [this] { repaint(); };

    auto* bpmParameter = owner.apvts.getParameter(core::ids::fallbackBpm.getParamID());
    bpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    bpmSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    bpmSlider.setColour(juce::Slider::trackColourId, palette.accent);
    bpmSlider.setColour(juce::Slider::backgroundColourId, palette.panelRaised);
    bpmSlider.setColour(juce::Slider::thumbColourId, palette.text);
    bpmSlider.setTooltip("Tempo used when the host doesn't report one, and while the transport is stopped");
    bpmSlider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(bpmSlider);
    bpmAttachment = std::make_unique<juce::SliderParameterAttachment>(*bpmParameter, bpmSlider);
    bpmSlider.setDoubleClickReturnValue(true, bpmParameter->convertFrom0to1(bpmParameter->getDefaultValue()));

    setSize(kWidth, kHeight);
    startTimerHz(10);
}

ShaperEditor::~ShaperEditor()
{
    stopTimer();
}

void ShaperEditor::timerCallback()
{
    const auto source = owner.getSyncSource();
    if (source != shownSource)
    {
        shownSource = source;
        repaint(statusArea);
    }
}

void ShaperEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto left = area.removeFromLeft(240);
    area.removeFromLeft(18);

    brand.setBounds(left.removeFromTop(22).withWidth(170));
    left.removeFromTop(78); // title block is painted
    mixKnob.setBounds(left.removeFromTop(228).reduced(6));
    left.removeFromTop(34); // "MIX 79%" text is painted
    left.removeFromTop(14);
    bpmSlider.setBounds(left.removeFromTop(26).reduced(2, 0));

    auto row = area.removeFromTop(34);
    divisionRow.setBounds(row.removeFromLeft(ChoicePillRow::widthFor(divisionRow.getNumPills())));
    statusArea = row.withTrimmedLeft(ChoicePillRow::kGap + 10);

    area.removeFromTop(10);
    curveEditor.setBounds(area.removeFromTop(250));

    area.removeFromTop(10);
    presetPanel.setBounds(area);
}

void ShaperEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(palette.background.brighter(0.04f), 0.0f, 0.0f,
                                           palette.background.darker(0.2f), 0.0f, bounds.getBottom(), false));
    g.fillAll();

    const auto leftPanel = juce::Rectangle<float>(8.0f, 8.0f, 262.0f, bounds.getHeight() - 16.0f);
    g.setColour(palette.panel);
    g.fillRoundedRectangle(leftPanel, 14.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(leftPanel, 14.0f);
        g.reduceClipRegion(clip);
        theme.motif->drawAmbient(g, leftPanel.withTop(leftPanel.getBottom() - 70.0f), 0.0, palette);
        theme.motif->drawDecoration(g, leftPanel, palette);
    }

    // Title: 60 px unless the name is too wide for the column, then shrunk to fit.
    const auto titleArea = juce::Rectangle<float>(32.0f, 44.0f, 170.0f, 64.0f);
    float titleSize = 60.0f;
    const float titleWidth = juce::GlyphArrangement::getStringWidth(makeFont(titleSize), theme.name);
    if (titleWidth > 156.0f)
        titleSize = std::max(24.0f, titleSize * 156.0f / titleWidth);
    g.setColour(palette.accent);
    g.setFont(makeFont(titleSize));
    g.drawText(theme.name, titleArea, juce::Justification::centredLeft, false);
    g.setColour(palette.textDim);
    g.setFont(makeFont(11.0f, false));
    g.drawText(theme.tagline, juce::Rectangle<float>(34.0f, 98.0f, 150.0f, 14.0f), juce::Justification::centredLeft,
               false);
    theme.motif->drawMascot(g, juce::Rectangle<float>(190.0f, 44.0f, 62.0f, 62.0f), palette);

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
    const bool host = shownSource == dsp::SyncSource::Host;
    auto status = statusArea.toFloat().withSizeKeepingCentre(static_cast<float>(statusArea.getWidth()), 24.0f);
    const float dotR = 4.0f;
    g.setColour(host ? palette.accent : palette.textDim);
    g.fillEllipse(status.getX() + 4.0f, status.getCentreY() - dotR, dotR * 2.0f, dotR * 2.0f);
    g.setFont(makeFont(12.0f));
    g.drawText(host ? "HOST SYNC" : "FREE RUN", status.withTrimmedLeft(20.0f), juce::Justification::centredLeft, false);
}
} // namespace animalaudio::ui
