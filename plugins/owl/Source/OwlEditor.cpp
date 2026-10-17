#include "OwlEditor.h"

#include "ui/OwlTheme.h"

#include <animalaudio/core/BuildStamp.h>
#include <animalaudio/ui/ThemedPanel.h>

namespace owl
{
using animalaudio::ui::makeFont;

namespace
{
constexpr int kKnobSize = 72;
constexpr int kSmallKnobSize = 60;
constexpr int kCardHeader = 30;
constexpr const char* kBandNames[] = {"LOW", "MID", "HIGH"};
constexpr const char* kBandShapes[] = {"SHELF", "BELL", "SHELF"};
} // namespace

OwlEditor::OwlEditor(OwlProcessor& p)
    : juce::AudioProcessorEditor(&p)
    , owner(p)
    , theme(ui::makeTheme())
    , palette(theme.palette)
    , brand(palette)
    , outputKnob(theme)
    , flatButton("FLAT", palette)
    , response(p, theme)
{
    brand.setTooltip(animalaudio::core::buildStampLong(theme.name));
    addAndMakeVisible(brand);
    addAndMakeVisible(response);

    attach(outputKnob, ids::output, "Output: trims the level after the EQ");
    outputKnob.setArcOrigin(0.0);
    addAndMakeVisible(outputKnob);

    flatButton.setTooltip("Set every band back to 0 dB");
    flatButton.onClick = [this] { flattenAllBands(); };
    addAndMakeVisible(flatButton);

    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        auto& card = cards[static_cast<std::size_t>(band)];
        const auto colour = ui::bandColour(palette, band);
        const juce::String name = juce::String(kBandNames[band]).toLowerCase();

        card.frequency = std::make_unique<ui::EyeKnob>(palette, colour);
        attach(*card.frequency, ids::frequencyOf(band), "Frequency of the " + name + " band");
        card.gain = std::make_unique<ui::EyeKnob>(palette, colour);
        attach(*card.gain, ids::gainOf(band), "Boost or cut of the " + name + " band");
        card.gain->setArcOrigin(0.0);
        addAndMakeVisible(*card.frequency);
        addAndMakeVisible(*card.gain);

        if (band == dsp::kMid)
        {
            card.q = std::make_unique<ui::EyeKnob>(palette, colour);
            attach(*card.q, ids::midQ, "Width of the mid band: low Q is broad, high Q is narrow");
            addAndMakeVisible(*card.q);
        }
    }

    setSize(kWidth, kHeight);
}

void OwlEditor::attach(juce::Slider& slider, const juce::ParameterID& id, const juce::String& tooltip)
{
    auto* parameter = owner.apvts.getParameter(id.getParamID());
    jassert(parameter != nullptr);
    slider.setTooltip(tooltip);
    attachments.push_back(std::make_unique<juce::SliderParameterAttachment>(*parameter, slider));
    slider.setDoubleClickReturnValue(true, parameter->convertFrom0to1(parameter->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

juce::Slider* OwlEditor::getBandKnob(int band, Control control)
{
    auto& card = cards[static_cast<std::size_t>(band)];
    switch (control)
    {
    case Control::Frequency:
        return card.frequency.get();
    case Control::Gain:
        return card.gain.get();
    case Control::Q:
        return card.q.get();
    }
    return nullptr;
}

void OwlEditor::flattenAllBands()
{
    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        auto* gain = owner.apvts.getParameter(ids::gainOf(band).getParamID());
        gain->beginChangeGesture();
        gain->setValueNotifyingHost(gain->convertTo0to1(0.0f));
        gain->endChangeGesture();
    }
}

void OwlEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto left = area.removeFromLeft(240);
    area.removeFromLeft(18);

    brand.setBounds(left.removeFromTop(22).withWidth(170));
    left.removeFromTop(78); // title block is painted
    outputKnob.setBounds(left.removeFromTop(228).reduced(6));
    left.removeFromTop(34); // "OUTPUT +0.0 dB" is painted
    left.removeFromTop(22);
    flatButton.setBounds(left.removeFromTop(30).withSizeKeepingCentre(112, 30));

    response.setBounds(area.removeFromTop(290));
    area.removeFromTop(12);

    const int gap = 10;
    const int sideWidth = 176;
    const int widths[] = {sideWidth, area.getWidth() - 2 * (sideWidth + gap), sideWidth};
    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        auto& card = cards[static_cast<std::size_t>(band)];
        card.bounds = area.removeFromLeft(widths[band]);
        area.removeFromLeft(gap);

        std::vector<std::pair<juce::Slider*, int>> knobs{{card.frequency.get(), kKnobSize},
                                                         {card.gain.get(), kKnobSize}};
        if (card.q)
            knobs.emplace_back(card.q.get(), kSmallKnobSize);

        int used = 0;
        for (const auto& k : knobs)
            used += k.second;
        const int spacing = (card.bounds.getWidth() - used) / static_cast<int>(knobs.size() + 1);
        const int knobTop = card.bounds.getY() + kCardHeader + 20;
        int x = card.bounds.getX() + spacing;
        for (const auto& [knob, size] : knobs)
        {
            knob->setBounds(x, knobTop + (kKnobSize - size) / 2, size, size);
            x += size + spacing;
        }
    }
}

void OwlEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    animalaudio::ui::drawEditorBackground(g, bounds, palette);

    const auto leftPanel = juce::Rectangle<float>(8.0f, 8.0f, 262.0f, bounds.getHeight() - 16.0f);
    animalaudio::ui::drawThemedPanel(g, leftPanel, theme);
    animalaudio::ui::drawTitleBlock(g, {32.0f, 44.0f}, theme);

    // OUTPUT read-out under the big knob
    const auto knobArea = outputKnob.getBounds().toFloat();
    const auto labelArea =
        juce::Rectangle<float>(knobArea.getX(), knobArea.getBottom() + 2.0f, knobArea.getWidth(), 28.0f);
    const juce::String value = owner.apvts.getParameter(ids::output.getParamID())->getCurrentValueAsText();
    g.setFont(makeFont(22.0f));
    const float nameW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), "OUTPUT ");
    const float valueW = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), value);
    const float startX = labelArea.getCentreX() - (nameW + valueW) * 0.5f;
    g.setColour(palette.textDim);
    g.drawText("OUTPUT", labelArea.withX(startX).withWidth(nameW), juce::Justification::centredLeft, false);
    g.setColour(palette.accent);
    g.drawText(value, labelArea.withX(startX + nameW).withWidth(valueW + 4.0f), juce::Justification::centredLeft,
               false);

    animalaudio::ui::drawBuildStamp(g, juce::Rectangle<float>(30.0f, leftPanel.getBottom() - 22.0f, 226.0f, 12.0f),
                                    palette);

    for (int band = 0; band < dsp::kNumBands; ++band)
        paintCard(g, band);
}

void OwlEditor::paintCard(juce::Graphics& g, int band)
{
    const auto& card = cards[static_cast<std::size_t>(band)];
    const auto box = card.bounds.toFloat();
    const auto colour = ui::bandColour(palette, band);

    g.setColour(palette.panel);
    g.fillRoundedRectangle(box, 12.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(box, 12.0f);
        g.reduceClipRegion(clip);
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(0.1f), 0.0f, box.getY(), colour.withAlpha(0.0f), 0.0f,
                                               box.getY() + 70.0f, false));
        g.fillRect(box);
        g.setColour(colour.withAlpha(0.85f));
        g.fillRect(box.withHeight(3.0f));
    }

    auto header = box.withHeight(static_cast<float>(kCardHeader)).translated(0.0f, 6.0f).reduced(14.0f, 0.0f);
    g.setColour(colour);
    g.fillEllipse(header.getX(), header.getCentreY() - 4.0f, 8.0f, 8.0f);
    g.setFont(makeFont(15.0f));
    g.drawText(kBandNames[band], header.withTrimmedLeft(14.0f), juce::Justification::centredLeft, false);
    g.setColour(palette.textDim);
    g.setFont(makeFont(10.5f, false));
    g.drawText(kBandShapes[band], header, juce::Justification::centredRight, false);

    paintKnobLabel(g, *card.frequency, "FREQ", ids::frequencyOf(band), palette.text);
    paintKnobLabel(g, *card.gain, "GAIN", ids::gainOf(band), palette.text);
    if (card.q)
        paintKnobLabel(g, *card.q, "Q", ids::midQ, palette.text);
}

void OwlEditor::paintKnobLabel(juce::Graphics& g, juce::Slider& knob, const juce::String& name,
                               const juce::ParameterID& id, juce::Colour valueColour)
{
    const auto k = knob.getBounds().toFloat();
    const float top = static_cast<float>(cards[0].frequency->getBottom()) + 4.0f;
    const auto column = juce::Rectangle<float>(k.getCentreX() - 42.0f, top, 84.0f, 14.0f);
    g.setColour(palette.textDim);
    g.setFont(makeFont(10.0f));
    g.drawText(name, column, juce::Justification::centred, false);
    g.setColour(valueColour);
    g.setFont(makeFont(13.0f));
    g.drawText(owner.apvts.getParameter(id.getParamID())->getCurrentValueAsText(), column.translated(0.0f, 15.0f),
               juce::Justification::centred, false);
}
} // namespace owl
