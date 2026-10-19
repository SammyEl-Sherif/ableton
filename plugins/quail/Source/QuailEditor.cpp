#include "QuailEditor.h"

#include "ui/QuailTheme.h"

#include <animalaudio/core/BuildStamp.h>
#include <animalaudio/ui/ThemedPanel.h>

namespace quail
{
using ui::QuailToggle;

namespace
{
const juce::Identifier kWidthId{"width"};
const juce::Identifier kHeightId{"height"};
const juce::Identifier kRangeId{"rangeDb"};
const juce::Identifier kPreId{"analyzerPre"};
const juce::Identifier kPostId{"analyzerPost"};
const juce::Identifier kSidechainId{"analyzerSidechain"};

constexpr int kTopBarHeight = 54;
constexpr int kBottomBarHeight = 50;
constexpr int kMeterWidth = 26;
constexpr int kEdge = 18;
constexpr int kTrackPadding = 3;

void drawPowerGlyph(juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
{
    const auto c = area.getCentre();
    const float r = juce::jmin(area.getWidth(), area.getHeight()) * 0.5f;
    juce::Path ring;
    ring.addCentredArc(c.x, c.y, r * 0.8f, r * 0.8f, 0.0f, 0.65f, juce::MathConstants<float>::twoPi - 0.65f, true);
    g.setColour(colour);
    g.strokePath(ring, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine(c.x, c.y - r, c.x, c.y - r * 0.15f, 1.5f);
}
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
    , analyzerPre("PRE", theme.palette, QuailToggle::Style::segment)
    , analyzerPost("POST", theme.palette, QuailToggle::Style::segment)
    , analyzerSidechain("SC", theme.palette, QuailToggle::Style::segment)
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
    bypass.glyph = [](juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour)
    { drawPowerGlyph(g, area, colour); };
    bypass.setTint(ui::warningColour());
    autoGain.setShowsLight(true);
    phase.setShowsLight(true);
    analyzerSidechain.setTint(ui::sidechainColour());
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
        "Spectral resolution: frequency detail of spectral bands vs. the latency they add",
        [this](const juce::String& text)
        { return std::make_unique<QuailToggle>(text.toUpperCase(), theme.palette, QuailToggle::Style::segment); });
    resolution->setGap(2);
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
    auto card = juce::Rectangle<int>(area.getCentreX() - width / 2, area.getBottom() - 14 - ui::BandPanel::kHeight,
                                     width, ui::BandPanel::kHeight);
    const int band = display.getSelectedBand();
    if (band >= 0)
    {
        // keep the selected node visible: move the panel to the top when the node sits under it
        const auto node = display.nodePosition(band).toInt() + area.getPosition();
        if (card.expanded(16).contains(node))
            card.setY(area.getY() + 14);
    }
    bandPanel.setBounds(card.expanded(ui::BandPanel::kShadow));
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
    const auto brandArea = brand.getBounds().toFloat();
    animalaudio::ui::drawBuildStamp(
        g, juce::Rectangle<float>(brandArea.getX() + 1.0f, brandArea.getBottom() + 4.0f, 170.0f, 11.0f), palette);
    g.setColour(palette.text.withAlpha(0.07f));
    g.fillRect(juce::Rectangle<float>(194.0f, top.getY() + 14.0f, 1.0f, top.getHeight() - 28.0f));

    theme.motif->drawMascot(g, juce::Rectangle<float>(208.0f, top.getY() + 10.0f, 34.0f, 34.0f), palette);
    g.setColour(palette.text);
    g.setFont(ui::titleFont(18.0f));
    g.drawText(theme.name, juce::Rectangle<float>(250.0f, top.getY() + 11.0f, 120.0f, 20.0f),
               juce::Justification::centredLeft, false);
    const int used = actions.numUsed();
    g.setColour(palette.textDim);
    g.setFont(ui::labelFont(9.0f));
    g.drawText(theme.tagline + juce::String(juce::CharPointer_UTF8("  \xc2\xb7  ")) + juce::String(used) +
                   (used == 1 ? " BAND" : " BANDS"),
               juce::Rectangle<float>(251.0f, top.getY() + 31.0f, 170.0f, 12.0f), juce::Justification::centredLeft,
               false);

    const auto track = resolutionTrack.toFloat();
    ui::drawSegmentTrack(g, track, palette);
    const int latency = owner.getLatencySamples();
    const double sampleRate = owner.getDisplaySampleRate();
    const auto caption = track.withX(track.getX() - 142.0f).withWidth(130.0f);
    g.setColour(palette.textDim);
    g.setFont(ui::labelFont(9.0f));
    g.drawText("SPECTRAL RESOLUTION", caption.withHeight(track.getHeight() * 0.5f).translated(0.0f, 1.0f),
               juce::Justification::centredRight, false);
    g.setColour(latency > 0 ? palette.text : palette.textDim.withAlpha(0.7f));
    g.setFont(ui::valueFont(10.0f));
    g.drawText(latency > 0 ? juce::String(1000.0 * latency / sampleRate, 1) + " MS LATENCY" : "ZERO LATENCY",
               caption.withTrimmedTop(track.getHeight() * 0.5f).translated(0.0f, -1.0f),
               juce::Justification::centredRight, false);

    const auto status = sidechainStatus.toFloat().reduced(0.5f);
    const bool connected = owner.isSidechainActive();
    const bool signal = connected && meter.getLevelDb(2) > -70.0f;
    const auto sc = ui::sidechainColour();
    g.setColour(signal ? sc.withAlpha(0.05f) : palette.text.withAlpha(0.025f));
    g.fillRoundedRectangle(status, 7.0f);
    g.setColour(signal ? sc.withAlpha(0.2f) : palette.text.withAlpha(0.08f));
    g.drawRoundedRectangle(status.reduced(0.5f), 7.0f, 1.0f);
    const auto light = juce::Rectangle<float>(6.0f, 6.0f).withCentre({status.getX() + 15.0f, status.getCentreY()});
    if (signal)
    {
        g.setColour(sc.withAlpha(0.3f));
        g.fillEllipse(light.expanded(2.5f));
    }
    g.setColour(signal ? sc : connected ? sc.withAlpha(0.5f) : palette.text.withAlpha(0.2f));
    g.fillEllipse(light);
    g.setColour(connected ? palette.text : palette.textDim);
    g.setFont(ui::valueFont(10.5f).withExtraKerningFactor(0.06f));
    g.drawText(connected ? "SIDE CHAIN" : "NO SIDE CHAIN", status.withTrimmedLeft(27.0f),
               juce::Justification::centredLeft, false);

    const auto bottom = bottomBar.toFloat();
    ui::drawSegmentTrack(g, analyzerTrack.toFloat(), palette);
    g.setColour(palette.textDim);
    g.setFont(ui::labelFont(9.0f));
    g.drawText("ANALYZER", juce::Rectangle<float>(static_cast<float>(kEdge), bottom.getY(), 70.0f, bottom.getHeight()),
               juce::Justification::centredLeft, false);
}

void QuailEditor::resized()
{
    auto bounds = getLocalBounds();
    topBar = bounds.removeFromTop(kTopBarHeight);
    bottomBar = bounds.removeFromBottom(kBottomBarHeight);
    content = bounds.reduced(kEdge, 2);

    brand.setBounds(kEdge, topBar.getY() + 13, 146, 16);
    const int controlY = topBar.getCentreY() - 14;
    bypass.setBounds(topBar.getRight() - kEdge - 104, controlY, 104, 28);
    sidechainStatus = juce::Rectangle<int>(bypass.getX() - 10 - 136, controlY, 136, 28);
    resolutionTrack = juce::Rectangle<int>(sidechainStatus.getX() - 22 - 204, controlY, 204, 28);
    resolution->setBounds(resolutionTrack.reduced(kTrackPadding));

    auto meterArea = content.removeFromRight(kMeterWidth);
    content.removeFromRight(10);
    meter.setBounds(meterArea.withTrimmedTop(4).withTrimmedBottom(4));
    display.setBounds(content);

    auto row = bottomBar.withSizeKeepingCentre(bottomBar.getWidth() - 2 * kEdge, 28);
    row.removeFromLeft(72);
    analyzerTrack = row.removeFromLeft(3 * 54 + 2 * kTrackPadding);
    auto segments = analyzerTrack.reduced(kTrackPadding);
    for (int i = 0; i < 3; ++i)
    {
        getAnalyzerButton(i).setBounds(segments.removeFromLeft(i == 2 ? segments.getWidth() : 52));
        segments.removeFromLeft(2);
    }
    row.removeFromLeft(10);
    rangeButton.setBounds(row.removeFromLeft(74));

    outputChip.setBounds(row.removeFromRight(128));
    row.removeFromRight(8);
    panChip.setBounds(row.removeFromRight(96));
    row.removeFromRight(8);
    scaleChip.setBounds(row.removeFromRight(112));
    row.removeFromRight(18);
    phase.setBounds(row.removeFromRight(84));
    row.removeFromRight(8);
    autoGain.setBounds(row.removeFromRight(110));

    placeBandPanel();
    saveUiState();
}
} // namespace quail
