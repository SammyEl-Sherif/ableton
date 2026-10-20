#include "IcePenguinEditor.h"

#include "dsp/IcePenguinPresets.h"

#include <animalaudio/core/BuildStamp.h>
namespace icepenguin
{
namespace
{
void styleKnob(animalaudio::ui::ThemedKnob& knob)
{
    knob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
}
} // namespace

IcePenguinEditor::IcePenguinEditor(IcePenguinProcessor& processor)
    : juce::AudioProcessorEditor(processor)
    , owner(processor)
    , theme(ui::makeTheme())
    , brand(theme.palette)
    , mixKnob(theme)
    , decayKnob(theme)
    , toneKnob(theme)
    , predelayKnob(theme)
    , sizeKnob(theme)
{
    brand.setTooltip(animalaudio::core::buildStampLong(theme.name));

    for (auto* label : {&mixLabel, &decayLabel, &toneLabel, &predelayLabel, &sizeLabel})
    {
        label->setJustificationType(juce::Justification::centred);
        label->setColour(juce::Label::textColourId, theme.palette.textDim);
        label->setFont(animalaudio::ui::makeFont(11.0f));
    }
    mixLabel.setText("MIX", juce::dontSendNotification);
    decayLabel.setText("DECAY", juce::dontSendNotification);
    toneLabel.setText("TONE", juce::dontSendNotification);
    predelayLabel.setText("PREDELAY", juce::dontSendNotification);
    sizeLabel.setText("SIZE", juce::dontSendNotification);

    for (auto* knob : {&mixKnob, &decayKnob, &toneKnob, &predelayKnob, &sizeKnob})
        styleKnob(*knob);

    auto attach = [&](animalaudio::ui::ThemedKnob& knob, const juce::ParameterID& id)
    {
        auto* param = owner.apvts.getParameter(id.getParamID());
        knob.setDoubleClickReturnValue(true, param->convertFrom0to1(param->getDefaultValue()));
        return std::make_unique<juce::SliderParameterAttachment>(*param, knob);
    };
    mixAttachment = attach(mixKnob, ids::mix);
    decayAttachment = attach(decayKnob, ids::decay);
    toneAttachment = attach(toneKnob, ids::tone);
    predelayAttachment = attach(predelayKnob, ids::predelay);
    sizeAttachment = attach(sizeKnob, ids::size);

    presetHeader.setText("PRESETS", juce::dontSendNotification);
    presetHeader.setJustificationType(juce::Justification::centredLeft);
    presetHeader.setColour(juce::Label::textColourId, theme.palette.textDim);
    presetHeader.setFont(animalaudio::ui::makeFont(12.0f, juce::Font::bold));

    const auto& presets = reverbPresets();
    for (int i = 0; i < kNumReverbPresets; ++i)
    {
        const auto& presetName = presets[static_cast<std::size_t>(i)].name;
        auto button = std::make_unique<animalaudio::ui::PillButton>(juce::String(presetName.data(), presetName.size()),
                                                                    theme.palette);
        button->setTriggeredOnMouseDown(true);
        button->setClickingTogglesState(false);
        button->onClick = [&, i]()
        {
            if (auto* param = owner.apvts.getParameter(ids::preset.getParamID()))
                param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(i)));
            refreshPresetHighlight();
        };
        addAndMakeVisible(*button);
        presetButtons[static_cast<std::size_t>(i)] = std::move(button);
    }

    addAndMakeVisible(brand);
    addAndMakeVisible(mixKnob);
    addAndMakeVisible(decayKnob);
    addAndMakeVisible(toneKnob);
    addAndMakeVisible(predelayKnob);
    addAndMakeVisible(sizeKnob);
    addAndMakeVisible(mixLabel);
    addAndMakeVisible(decayLabel);
    addAndMakeVisible(toneLabel);
    addAndMakeVisible(predelayLabel);
    addAndMakeVisible(sizeLabel);
    addAndMakeVisible(presetHeader);

    owner.apvts.addParameterListener(ids::preset.getParamID(), this);
    refreshPresetHighlight();
    setSize(kWidth, kHeight);
    startTimerHz(24);
}

IcePenguinEditor::~IcePenguinEditor()
{
    owner.apvts.removeParameterListener(ids::preset.getParamID(), this);
    stopTimer();
}

void IcePenguinEditor::parameterChanged(const juce::String& parameterID, float)
{
    if (parameterID == ids::preset.getParamID())
        refreshPresetHighlight();
}

void IcePenguinEditor::refreshPresetHighlight()
{
    const int selected =
        dynamic_cast<juce::AudioParameterChoice*>(owner.apvts.getParameter(ids::preset.getParamID()))->getIndex();
    for (int i = 0; i < kNumReverbPresets; ++i)
        presetButtons[static_cast<std::size_t>(i)]->setToggleState(i == selected, juce::dontSendNotification);
}

void IcePenguinEditor::timerCallback()
{
    ambientPhase += 0.04f;
    repaint();
}

void IcePenguinEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme.palette.background);

    auto bounds = getLocalBounds().toFloat();
    const auto leftPanel = bounds.removeFromLeft(300.0f).reduced(14.0f);
    const auto rightPanel = bounds.reduced(14.0f);

    auto panel = [&](juce::Rectangle<float> area)
    {
        g.setColour(theme.palette.panel);
        g.fillRoundedRectangle(area, 14.0f);
        g.setColour(theme.palette.outline.withAlpha(0.65f));
        g.drawRoundedRectangle(area, 14.0f, 1.0f);
    };
    panel(leftPanel);
    panel(rightPanel);

    if (theme.motif)
    {
        theme.motif->drawAmbient(g, getLocalBounds().toFloat(), static_cast<double>(ambientPhase), theme.palette);
        theme.motif->drawDecoration(g, rightPanel, theme.palette);
    }

    g.setColour(theme.palette.text);
    g.setFont(animalaudio::ui::makeFont(22.0f, juce::Font::bold));
    g.drawText(theme.name, leftPanel.withTrimmedTop(52.0f).removeFromTop(34.0f), juce::Justification::centredLeft,
               false);

    g.setColour(theme.palette.accent);
    g.setFont(animalaudio::ui::makeFont(11.0f, juce::Font::bold));
    g.drawText(theme.tagline, leftPanel.withTrimmedTop(84.0f).removeFromTop(16.0f), juce::Justification::centredLeft,
               false);

    if (theme.motif)
        theme.motif->drawMascot(g, leftPanel.withTrimmedTop(108.0f).removeFromTop(130.0f).reduced(8.0f, 0.0f),
                                theme.palette);

    g.setColour(theme.palette.textDim);
    g.setFont(animalaudio::ui::makeFont(10.0f));
    g.drawText(animalaudio::core::buildStampShort(),
               juce::Rectangle<float>(leftPanel.getX() + 16.0f, leftPanel.getBottom() - 20.0f,
                                      leftPanel.getWidth() - 32.0f, 14.0f),
               juce::Justification::centredLeft, false);
}

void IcePenguinEditor::resized()
{
    auto left = getLocalBounds().removeFromLeft(300).reduced(14);
    brand.setBounds(left.removeFromTop(36).removeFromLeft(160));

    const int knobSize = 72;
    const int rowH = 88;
    auto knobColumn = left.withTrimmedTop(250);
    auto placeKnob = [&](animalaudio::ui::ThemedKnob& knob, juce::Label& label)
    {
        auto row = knobColumn.removeFromTop(rowH);
        knob.setBounds(row.removeFromLeft(knobSize).reduced(4));
        label.setBounds(row.reduced(4, 24));
    };
    placeKnob(mixKnob, mixLabel);
    placeKnob(decayKnob, decayLabel);
    placeKnob(toneKnob, toneLabel);
    placeKnob(predelayKnob, predelayLabel);
    placeKnob(sizeKnob, sizeLabel);

    auto right = getLocalBounds().withTrimmedLeft(300).reduced(14);
    presetHeader.setBounds(right.removeFromTop(28).reduced(18, 0));

    const int cols = 3;
    const int rows = 4;
    const int gap = 8;
    auto grid = right.reduced(18, 8);
    const int cellW = (grid.getWidth() - gap * (cols - 1)) / cols;
    const int cellH = (grid.getHeight() - gap * (rows - 1)) / rows;

    for (int i = 0; i < kNumReverbPresets; ++i)
    {
        const int row = i / cols;
        const int col = i % cols;
        presetButtons[static_cast<std::size_t>(i)]->setBounds(grid.getX() + col * (cellW + gap),
                                                              grid.getY() + row * (cellH + gap), cellW, cellH);
    }
}
} // namespace icepenguin
