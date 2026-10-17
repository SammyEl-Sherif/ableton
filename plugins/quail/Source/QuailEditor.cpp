#include "QuailEditor.h"

#include "ui/QuailTheme.h"

#include <animalaudio/core/BuildStamp.h>
#include <animalaudio/ui/ThemedPanel.h>

namespace quail
{
using animalaudio::ui::makeFont;

namespace
{
const juce::Identifier kWidthId{"width"};
const juce::Identifier kHeightId{"height"};
const juce::Identifier kRangeId{"rangeDb"};
const juce::Identifier kPreId{"analyzerPre"};
const juce::Identifier kPostId{"analyzerPost"};
const juce::Identifier kSidechainId{"analyzerSidechain"};

constexpr int kTopBarHeight = 48;
constexpr int kBottomBarHeight = 44;
constexpr int kMeterWidth = 30;
} // namespace

QuailEditor::QuailEditor(QuailProcessor& p)
    : AudioProcessorEditor(p)
    , owner(p)
    , theme(ui::makeTheme())
    , lookAndFeel(theme.palette)
    , actions(p)
    , brand(theme.palette)
    , display(actions, theme)
    , bandPanel(actions, theme)
    , meter(theme.palette)
    , bypass("BYPASS", theme.palette)
    , analyzerPre("PRE", theme.palette)
    , analyzerPost("POST", theme.palette)
    , analyzerSidechain("SC", theme.palette)
    , rangeButton("", theme.palette)
    , autoGain("AUTO GAIN", theme.palette)
    , phase("PHASE", theme.palette)
    , scaleChip("SCALE", theme.palette)
    , panChip("PAN", theme.palette)
    , outputChip("OUTPUT", theme.palette)
{
    setLookAndFeel(&lookAndFeel);
    const auto& params = owner.params();

    brand.setTooltip(animalaudio::core::buildStampLong(theme.name));
    addAndMakeVisible(brand);
    addAndMakeVisible(display);
    addAndMakeVisible(meter);
    addChildComponent(bandPanel);

    display.onSelectionChanged = [this] { selectionChanged(); };
    bandPanel.onNavigate = [this](int direction) { navigate(direction); };
    bandPanel.onRemove = [this]
    {
        const int band = bandPanel.getBand();
        if (band < 0)
            return;
        actions.remove(band);
        display.select(-1);
    };

    for (auto* toggle : {&bypass, &autoGain, &phase})
    {
        toggle->setClickingTogglesState(true);
        addAndMakeVisible(*toggle);
    }
    bypass.setTooltip("Bypass (latency stays compensated)");
    autoGain.setTooltip("Auto gain: compensate the loudness change of the static EQ curve");
    phase.setTooltip("Invert the output polarity");
    bypassAttachment = std::make_unique<juce::ButtonParameterAttachment>(params.global(GlobalParam::bypass), bypass);
    autoGainAttachment =
        std::make_unique<juce::ButtonParameterAttachment>(params.global(GlobalParam::autoGain), autoGain);
    phaseAttachment = std::make_unique<juce::ButtonParameterAttachment>(params.global(GlobalParam::phaseInvert), phase);

    analyzerPre.setTooltip("Show the spectrum before the EQ");
    analyzerPost.setTooltip("Show the spectrum after the EQ");
    analyzerSidechain.setTooltip("Show the side-chain spectrum; red glow = where it collides with the signal");
    rangeButton.setTooltip("Display range of the EQ curve");
    for (int i = 0; i < 3; ++i)
    {
        getAnalyzerButton(i).onClick = [this, i] { toggleAnalyzer(i); };
        addAndMakeVisible(getAnalyzerButton(i));
    }
    rangeButton.onClick = [this] { cycleRange(); };
    addAndMakeVisible(rangeButton);

    scaleChip.setTooltip("Gain scale: scales every band's gain (and its dynamic range stays as set)");
    panChip.setTooltip("Output balance");
    outputChip.setTooltip("Output level");
    scaleAttachment =
        std::make_unique<juce::SliderParameterAttachment>(params.global(GlobalParam::gainScale), scaleChip);
    panAttachment = std::make_unique<juce::SliderParameterAttachment>(params.global(GlobalParam::pan), panChip);
    outputAttachment =
        std::make_unique<juce::SliderParameterAttachment>(params.global(GlobalParam::output), outputChip);
    for (auto* chip : {&scaleChip, &panChip, &outputChip})
        addAndMakeVisible(*chip);

    auto* resolutionParam = dynamic_cast<juce::AudioParameterChoice*>(&params.global(GlobalParam::spectralResolution));
    jassert(resolutionParam != nullptr);
    resolution = std::make_unique<animalaudio::ui::ChoicePillRow>(
        *resolutionParam, theme.palette,
        "Spectral resolution: frequency detail of spectral bands vs. the latency they add");
    addAndMakeVisible(*resolution);

    loadUiState();
    {
        // setResizeLimits already resizes (to the minimum), which must not overwrite the stored size
        const juce::ScopedValueSetter<bool> restoring(restoringSize, true);
        auto ui = owner.getUiState();
        const int width = juce::jlimit(kMinWidth, kMaxWidth, static_cast<int>(ui.getProperty(kWidthId, kDefaultWidth)));
        const int height =
            juce::jlimit(kMinHeight, kMaxHeight, static_cast<int>(ui.getProperty(kHeightId, kDefaultHeight)));
        setResizable(true, true);
        setResizeLimits(kMinWidth, kMinHeight, kMaxWidth, kMaxHeight);
        setSize(width, height);
    }
    refreshNow();
    startTimerHz(30);
}

QuailEditor::~QuailEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

juce::Button& QuailEditor::getAnalyzerButton(int index) noexcept
{
    return index == 0 ? analyzerPre : index == 1 ? analyzerPost : analyzerSidechain;
}

void QuailEditor::loadUiState()
{
    auto ui = owner.getUiState();
    ui::EqDisplay::Analyzer options;
    options.pre = ui.getProperty(kPreId, options.pre);
    options.post = ui.getProperty(kPostId, options.post);
    options.sidechain = ui.getProperty(kSidechainId, options.sidechain);
    display.setAnalyzer(options);
    float range = ui.getProperty(kRangeId, 12.0f);
    if (std::find(ui::EqDisplay::kRanges.begin(), ui::EqDisplay::kRanges.end(), range) == ui::EqDisplay::kRanges.end())
        range = 12.0f;
    display.setRangeDb(range);

    analyzerPre.setToggleState(options.pre, juce::dontSendNotification);
    analyzerPost.setToggleState(options.post, juce::dontSendNotification);
    analyzerSidechain.setToggleState(options.sidechain, juce::dontSendNotification);
    rangeButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc2\xb1")) + juce::String(juce::roundToInt(range)) +
                              " dB");
}

void QuailEditor::saveUiState()
{
    auto ui = owner.getUiState();
    const auto options = display.getAnalyzer();
    ui.setProperty(kPreId, options.pre, nullptr);
    ui.setProperty(kPostId, options.post, nullptr);
    ui.setProperty(kSidechainId, options.sidechain, nullptr);
    ui.setProperty(kRangeId, display.getRangeDb(), nullptr);
    if (!restoringSize)
    {
        ui.setProperty(kWidthId, getWidth(), nullptr);
        ui.setProperty(kHeightId, getHeight(), nullptr);
    }
}

void QuailEditor::toggleAnalyzer(int index)
{
    auto options = display.getAnalyzer();
    bool& flag = index == 0 ? options.pre : index == 1 ? options.post : options.sidechain;
    flag = !flag;
    display.setAnalyzer(options);
    getAnalyzerButton(index).setToggleState(flag, juce::dontSendNotification);
    saveUiState();
}

void QuailEditor::cycleRange()
{
    const auto& ranges = ui::EqDisplay::kRanges;
    auto it = std::find(ranges.begin(), ranges.end(), display.getRangeDb());
    const float next = it == ranges.end() || std::next(it) == ranges.end() ? ranges.front() : *std::next(it);
    display.setRangeDb(next);
    rangeButton.setButtonText(juce::String(juce::CharPointer_UTF8("\xc2\xb1")) + juce::String(juce::roundToInt(next)) +
                              " dB");
    saveUiState();
}

void QuailEditor::navigate(int direction)
{
    const int current = display.getSelectedBand();
    for (int step = 1; step <= dsp::kNumBands; ++step)
    {
        const int band = ((current + direction * step) % dsp::kNumBands + dsp::kNumBands) % dsp::kNumBands;
        if (actions.isUsed(band))
        {
            display.select(band);
            return;
        }
    }
}

void QuailEditor::selectionChanged()
{
    const int band = display.getSelectedBand();
    if (band != bandPanel.getBand())
        bandPanel.setBand(band);
    bandPanel.setVisible(band >= 0);
    if (display.getDraggedBand() < 0)
        placeBandPanel();
}

void QuailEditor::placeBandPanel()
{
    const auto area = display.getBounds();
    const int width = juce::jmin(ui::BandPanel::kWidth, area.getWidth() - 24);
    auto bounds = juce::Rectangle<int>(area.getCentreX() - width / 2, area.getBottom() - 12 - ui::BandPanel::kHeight,
                                       width, ui::BandPanel::kHeight);
    const int band = display.getSelectedBand();
    if (band >= 0)
    {
        // keep the selected node visible: move the panel to the top when the node sits under it
        const auto node = display.nodePosition(band).toInt() + area.getPosition();
        if (bounds.expanded(16).contains(node))
            bounds.setY(area.getY() + 12);
    }
    bandPanel.setBounds(bounds);
}

juce::String QuailEditor::statusKey() const
{
    return juce::String(owner.getLatencySamples()) + "|" + (owner.isSidechainActive() ? "1" : "0") + "|" +
           juce::String(actions.numUsed()) + "|" + juce::String(meter.getLevelDb(2) > -70.0f ? 1 : 0);
}

void QuailEditor::refreshNow()
{
    display.refresh();
    auto& engine = owner.getEngine();
    meter.push(engine.takeOutputPeak(0), engine.takeOutputPeak(1), engine.takeSidechainPeak(),
               owner.isSidechainActive());
    bandPanel.refresh();
    const auto status = statusKey();
    if (status != lastStatus)
    {
        lastStatus = status;
        repaint(topBar);
    }
}

void QuailEditor::setTimeOverride(double seconds)
{
    display.setTimeOverride(seconds);
}

void QuailEditor::paint(juce::Graphics& g)
{
    const auto& palette = theme.palette;
    animalaudio::ui::drawEditorBackground(g, getLocalBounds().toFloat(), palette);

    const auto top = topBar.toFloat();
    g.setGradientFill(
        juce::ColourGradient(palette.panel, 0.0f, top.getY(), palette.background, 0.0f, top.getBottom(), false));
    g.fillRect(top);
    g.setColour(palette.outline);
    g.fillRect(top.withTop(top.getBottom() - 1.0f));
    theme.motif->drawDecoration(g, top, palette);

    animalaudio::ui::drawBuildStamp(g, juce::Rectangle<float>(18.0f, top.getY() + 32.0f, 176.0f, 11.0f), palette);
    theme.motif->drawMascot(g, juce::Rectangle<float>(198.0f, top.getY() + 4.0f, 40.0f, 40.0f), palette);
    g.setColour(palette.accent);
    g.setFont(makeFont(24.0f));
    g.drawText(theme.name, juce::Rectangle<float>(244.0f, top.getY() + 5.0f, 110.0f, 24.0f),
               juce::Justification::centredLeft, false);
    g.setColour(palette.textDim);
    g.setFont(makeFont(10.0f));
    const int used = actions.numUsed();
    g.drawText(theme.tagline + "  |  " + juce::String(used) + (used == 1 ? " BAND" : " BANDS"),
               juce::Rectangle<float>(245.0f, top.getY() + 29.0f, 160.0f, 12.0f), juce::Justification::centredLeft,
               false);

    const auto resolutionBounds = resolution->getBounds().toFloat();
    const int latency = owner.getLatencySamples();
    const double sampleRate = owner.getDisplaySampleRate();
    g.setColour(palette.textDim);
    g.setFont(makeFont(10.0f));
    g.drawText("SPECTRAL", resolutionBounds.withX(resolutionBounds.getX() - 130.0f).withWidth(124.0f).withHeight(12.0f),
               juce::Justification::centredRight, false);
    g.setColour(latency > 0 ? palette.detail : palette.textDim.withAlpha(0.7f));
    g.drawText(latency > 0 ? "LATENCY " + juce::String(1000.0 * latency / sampleRate, 1) + " MS" : "ZERO LATENCY",
               resolutionBounds.withX(resolutionBounds.getX() - 130.0f).withWidth(124.0f).withTrimmedTop(12.0f),
               juce::Justification::centredRight, false);

    const auto status =
        juce::Rectangle<float>(static_cast<float>(resolution->getRight() + 18), top.getY() + 12.0f, 120.0f, 24.0f);
    const bool connected = owner.isSidechainActive();
    const bool signal = connected && meter.getLevelDb(2) > -70.0f;
    g.setColour(palette.panelRaised);
    g.fillRoundedRectangle(status, 12.0f);
    g.setColour(signal ? palette.accentAlt : connected ? palette.accentAlt.withAlpha(0.45f) : palette.outline);
    g.fillEllipse(juce::Rectangle<float>(8.0f, 8.0f).withCentre({status.getX() + 14.0f, status.getCentreY()}));
    g.setColour(connected ? palette.text : palette.textDim);
    g.setFont(makeFont(10.5f));
    g.drawText(connected ? "SIDE CHAIN" : "NO SIDE CHAIN", status.withTrimmedLeft(24.0f),
               juce::Justification::centredLeft, false);

    const auto bottom = bottomBar.toFloat();
    g.setColour(palette.outline);
    g.fillRect(bottom.withHeight(1.0f));
    g.setColour(palette.textDim);
    g.setFont(makeFont(10.0f));
    g.drawText("ANALYZER", juce::Rectangle<float>(16.0f, bottom.getY(), 70.0f, bottom.getHeight()),
               juce::Justification::centredLeft, false);
}

void QuailEditor::resized()
{
    auto bounds = getLocalBounds();
    topBar = bounds.removeFromTop(kTopBarHeight);
    bottomBar = bounds.removeFromBottom(kBottomBarHeight);
    content = bounds.reduced(16, 6);

    brand.setBounds(16, topBar.getY() + 11, 146, 18);
    bypass.setBounds(topBar.getRight() - 16 - 100, topBar.getY() + 11, 100, 26);
    const int resolutionWidth = animalaudio::ui::ChoicePillRow::widthFor(3);
    resolution->setBounds(bypass.getX() - 18 - 120 - 18 - resolutionWidth, topBar.getY() + 12, resolutionWidth, 24);

    auto meterArea = content.removeFromRight(kMeterWidth);
    content.removeFromRight(8);
    meter.setBounds(meterArea);
    display.setBounds(content);

    auto row = bottomBar.reduced(16, 9);
    row.removeFromLeft(70);
    for (int i = 0; i < 3; ++i)
    {
        getAnalyzerButton(i).setBounds(row.removeFromLeft(i == 1 ? 52 : 44));
        row.removeFromLeft(4);
    }
    row.removeFromLeft(10);
    rangeButton.setBounds(row.removeFromLeft(72));

    outputChip.setBounds(row.removeFromRight(124));
    row.removeFromRight(6);
    panChip.setBounds(row.removeFromRight(92));
    row.removeFromRight(6);
    scaleChip.setBounds(row.removeFromRight(110));
    row.removeFromRight(10);
    phase.setBounds(row.removeFromRight(66));
    row.removeFromRight(6);
    autoGain.setBounds(row.removeFromRight(96));

    placeBandPanel();
    saveUiState();
}
} // namespace quail
