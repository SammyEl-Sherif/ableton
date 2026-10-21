#include "TurtleTimeEditor.h"

#include <animalaudio/core/BuildStamp.h>

namespace turtletime
{
namespace
{
void styleKnob(animalaudio::ui::ThemedKnob& knob)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}

void styleLabel(juce::Label& label, juce::Colour colour, float size, bool bold)
{
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, colour);
    label.setFont(animalaudio::ui::makeFont(size, bold));
    label.setInterceptsMouseClicks(false, false);
}
} // namespace

TurtleTimeEditor::TurtleTimeEditor(TurtleTimeProcessor& plugin)
    : juce::AudioProcessorEditor(plugin)
    , owner(plugin)
    , theme(ui::makeTheme())
    , brand(theme.palette)
    , mixKnob(theme)
    , bpmKnob(theme)
{
    setOpaque(true);
    brand.setTooltip(animalaudio::core::buildStampLong(theme.name));

    styleLabel(mixLabel, theme.palette.textDim, 11.0f, true);
    styleLabel(bpmLabel, theme.palette.textDim, 11.0f, true);
    styleKnob(mixKnob);
    styleKnob(bpmKnob);
    mixKnob.setTooltip("Dry / wet. 100% is the full time effect.");
    bpmKnob.setTooltip("Tempo used while the host transport is stopped.");

    auto attach = [&](animalaudio::ui::ThemedKnob& knob, const juce::ParameterID& id)
    {
        auto* param = owner.apvts.getParameter(id.getParamID());
        knob.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
        return std::make_unique<juce::SliderParameterAttachment>(*param, knob);
    };
    mixAttachment = attach(mixKnob, ids::mix);
    bpmAttachment = attach(bpmKnob, ids::fallbackBpm);

    styleLabel(patternHeader, theme.palette.textDim, 12.0f, true);
    patternHeader.setJustificationType(juce::Justification::centredLeft);
    patternHeader.setText("TIME PATTERNS", juce::dontSendNotification);

    styleLabel(syncLabel, theme.palette.accent, 11.0f, true);
    syncLabel.setJustificationType(juce::Justification::centredRight);
    syncLabel.setText("FREE RUN", juce::dontSendNotification);

    styleLabel(blurbLabel, theme.palette.text, 13.0f, false);
    blurbLabel.setJustificationType(juce::Justification::centredLeft);

    for (int i = 0; i < dsp::kNumPatterns; ++i)
    {
        const auto& pattern = dsp::kPatterns[static_cast<std::size_t>(i)];
        auto button = std::make_unique<animalaudio::ui::PillButton>(
            juce::String(pattern.name.data(), pattern.name.size()), theme.palette);
        button->setTooltip(juce::String(pattern.blurb.data(), pattern.blurb.size()));
        button->onClick = [&, i]()
        {
            if (auto* param = owner.apvts.getParameter(ids::pattern.getParamID()))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(i)));
            refreshPatternHighlight();
        };
        addAndMakeVisible(*button);
        patternButtons[static_cast<std::size_t>(i)] = std::move(button);
    }

    addAndMakeVisible(brand);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(bpmKnob);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(patternHeader);
    addAndMakeVisible(syncLabel);
    addAndMakeVisible(blurbLabel);

    refreshPatternHighlight();
    refreshReadouts();
    setSize(kWidth, kHeight);
    startTimerHz(24);
}

TurtleTimeEditor::~TurtleTimeEditor()
{
    stopTimer();
}

TurtleTimeEditor::Layout TurtleTimeEditor::layout() const
{
    Layout bounds;
    auto area = getLocalBounds().reduced(14);
    bounds.left = area.removeFromLeft(292);
    area.removeFromLeft(12);
    bounds.right = area;

    auto right = bounds.right;
    right.removeFromTop(36);
    bounds.lane = right.removeFromTop(250).reduced(8, 0);
    right.removeFromTop(8);
    bounds.grid = right.reduced(8, 0);
    return bounds;
}

void TurtleTimeEditor::refreshPatternHighlight()
{
    const int selected =
        dynamic_cast<juce::AudioParameterChoice*>(owner.apvts.getParameter(ids::pattern.getParamID()))->getIndex();
    for (int i = 0; i < dsp::kNumPatterns; ++i)
        patternButtons[static_cast<std::size_t>(i)]->setToggleState(i == selected, juce::dontSendNotification);

    const auto blurb = dsp::patternAt(selected).blurb;
    blurbLabel.setText(juce::String(blurb.data(), blurb.size()), juce::dontSendNotification);
}

void TurtleTimeEditor::refreshReadouts()
{
    mixLabel.setText("MIX  " + juce::String(juce::roundToInt(static_cast<float>(mixKnob.getValue()) * 100.0f)) + "%",
                     juce::dontSendNotification);
    bpmLabel.setText(juce::String(juce::roundToInt(bpmKnob.getValue())) + " BPM", juce::dontSendNotification);

    const bool host = owner.isHostSynced();
    syncLabel.setText(host ? "HOST SYNC" : "FREE RUN", juce::dontSendNotification);
    syncLabel.setColour(juce::Label::textColourId, host ? theme.palette.accent : theme.palette.textDim);
}

void TurtleTimeEditor::timerCallback()
{
    ambientSeconds += 1.0 / 24.0;
    refreshPatternHighlight();
    refreshReadouts();
    repaint();
}

void TurtleTimeEditor::paintLane(juce::Graphics& g, juce::Rectangle<float> area) const
{
    g.setColour(theme.palette.panelRaised);
    g.fillRoundedRectangle(area, 16.0f);
    g.setColour(theme.palette.outline.withAlpha(0.8f));
    g.drawRoundedRectangle(area, 16.0f, 1.0f);

    const int selected =
        dynamic_cast<juce::AudioParameterChoice*>(owner.apvts.getParameter(ids::pattern.getParamID()))->getIndex();
    const auto& pattern = dsp::patternAt(selected);
    const int divisions = std::max(1, static_cast<int>(std::lround(pattern.windowBeats)));

    auto track = area.reduced(28.0f, 36.0f);
    const float centreY = track.getCentreY() + 16.0f;

    g.setColour(theme.palette.outline);
    g.fillRoundedRectangle(track.getX(), centreY - 5.0f, track.getWidth(), 10.0f, 5.0f);

    g.setColour(theme.palette.textDim.withAlpha(0.7f));
    g.setFont(animalaudio::ui::makeFont(10.0f, false));
    for (int i = 0; i <= divisions; ++i)
    {
        const float x = track.getX() + track.getWidth() * static_cast<float>(i) / static_cast<float>(divisions);
        g.drawLine(x, centreY - 16.0f, x, centreY + 16.0f, 1.0f);
    }

    const float play = owner.getPlayheadPhase();
    const float marker = owner.getReadMarker();
    const float playX = track.getX() + track.getWidth() * play;
    const float markerX = track.getX() + track.getWidth() * marker;

    g.setColour(theme.palette.accent.withAlpha(0.18f));
    const float spanLeft = std::min(playX, markerX);
    const float spanRight = std::max(playX, markerX);
    g.fillRoundedRectangle(spanLeft, centreY - 5.0f, std::max(4.0f, spanRight - spanLeft), 10.0f, 5.0f);

    g.setColour(theme.palette.accent);
    g.drawLine(playX, track.getY(), playX, centreY + 28.0f, 2.0f);

    g.setColour(theme.palette.textDim);
    g.drawText("NOW", juce::Rectangle<float>(playX - 18.0f, track.getY() - 2.0f, 36.0f, 14.0f),
               juce::Justification::centred, false);

    if (theme.motif)
        theme.motif->drawMascot(g, juce::Rectangle<float>(markerX - 28.0f, centreY - 58.0f, 56.0f, 48.0f),
                                theme.palette);
}

void TurtleTimeEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme.palette.background);

    const auto bounds = layout();
    const auto left = bounds.left.toFloat();
    const auto right = bounds.right.toFloat();

    auto panel = [&](juce::Rectangle<float> area)
    {
        g.setColour(theme.palette.panel);
        g.fillRoundedRectangle(area, 16.0f);
        g.setColour(theme.palette.outline.withAlpha(0.65f));
        g.drawRoundedRectangle(area, 16.0f, 1.0f);
    };
    panel(left);
    panel(right);

    if (theme.motif)
    {
        theme.motif->drawAmbient(g, left, ambientSeconds, theme.palette);
    }

    g.setColour(theme.palette.text);
    g.setFont(animalaudio::ui::makeFont(26.0f));
    g.drawText(theme.name, left.withTrimmedTop(48.0f).removeFromTop(32.0f).reduced(16.0f, 0.0f),
               juce::Justification::centredLeft, false);

    g.setColour(theme.palette.accent);
    g.setFont(animalaudio::ui::makeFont(12.0f));
    g.drawText(theme.tagline, left.withTrimmedTop(80.0f).removeFromTop(16.0f).reduced(16.0f, 0.0f),
               juce::Justification::centredLeft, false);

    auto mascotArea = left.withTrimmedTop(108.0f).withTrimmedBottom(150.0f).reduced(28.0f, 8.0f);
    g.setColour(theme.palette.ambient.withAlpha(0.18f));
    g.fillEllipse(mascotArea.getX(), mascotArea.getBottom() - 28.0f, mascotArea.getWidth(), 36.0f);

    if (theme.motif)
        theme.motif->drawMascot(g, mascotArea, theme.palette);

    paintLane(g, bounds.lane.toFloat());

    g.setColour(theme.palette.textDim);
    g.setFont(animalaudio::ui::makeFont(10.0f, false));
    g.drawText(animalaudio::core::buildStampShort(),
               juce::Rectangle<float>(left.getX() + 16.0f, left.getBottom() - 22.0f, left.getWidth() - 32.0f, 14.0f),
               juce::Justification::centredLeft, false);
}

void TurtleTimeEditor::resized()
{
    const auto bounds = layout();
    auto left = bounds.left;
    brand.setBounds(left.removeFromTop(40).reduced(16, 8).removeFromLeft(168));

    auto knobs = bounds.left.reduced(16, 0).removeFromBottom(132).withTrimmedBottom(28);
    auto mixArea = knobs.removeFromLeft(knobs.getWidth() / 2);
    auto bpmArea = knobs;
    mixKnob.setBounds(mixArea.removeFromTop(86).withSizeKeepingCentre(86, 86));
    bpmKnob.setBounds(bpmArea.removeFromTop(86).withSizeKeepingCentre(86, 86));
    mixLabel.setBounds(mixArea.removeFromTop(22));
    bpmLabel.setBounds(bpmArea.removeFromTop(22));

    auto header = bounds.right.reduced(16, 10).removeFromTop(24);
    patternHeader.setBounds(header.removeFromLeft(header.getWidth() / 2));
    syncLabel.setBounds(header);

    auto grid = bounds.grid.withTrimmedTop(bounds.grid.getHeight() - 96);
    blurbLabel.setBounds(bounds.grid.withTrimmedBottom(100).removeFromBottom(36).reduced(4, 4));

    const int columns = 4;
    const int rows = 2;
    const int gap = 8;
    const int cellW = (grid.getWidth() - gap * (columns - 1)) / columns;
    const int cellH = (grid.getHeight() - gap * (rows - 1)) / rows;
    for (int i = 0; i < dsp::kNumPatterns; ++i)
    {
        const int row = i / columns;
        const int column = i % columns;
        patternButtons[static_cast<std::size_t>(i)]->setBounds(grid.getX() + column * (cellW + gap),
                                                               grid.getY() + row * (cellH + gap), cellW, cellH);
    }
}
} // namespace turtletime
