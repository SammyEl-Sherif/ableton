#pragma once

#include "QuailProcessor.h"
#include "ui/BandActions.h"
#include "ui/BandPanel.h"
#include "ui/EqDisplay.h"
#include "ui/Widgets.h"

#include <animalaudio/ui/BrandBadge.h>
#include <animalaudio/ui/ChoicePillRow.h>
#include <animalaudio/ui/PillButton.h>

#include <memory>

namespace quail
{
/**
 * Top bar: brand badge, QUAIL + mascot, spectral resolution (with the latency it costs), side-chain status, bypass.
 * Middle: the EQ display with the output meter; the selected band's panel floats over the display.
 * Bottom bar: analyser sources, display range, build stamp, auto gain, phase invert, gain scale, pan, output.
 * Resizable; size and display options are saved with the plugin state.
 */
class QuailEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kDefaultWidth = 1080;
    static constexpr int kDefaultHeight = 660;
    static constexpr int kMinWidth = 960;
    static constexpr int kMinHeight = 600;
    static constexpr int kMaxWidth = 2200;
    static constexpr int kMaxHeight = 1400;

    explicit QuailEditor(QuailProcessor& processor);
    ~QuailEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    const animalaudio::ui::Theme& getTheme() const noexcept { return theme; }
    animalaudio::ui::BrandBadge& getBrandBadge() noexcept { return brand; }
    ui::EqDisplay& getDisplay() noexcept { return display; }
    ui::BandPanel& getBandPanel() noexcept { return bandPanel; }
    ui::OutputMeter& getMeter() noexcept { return meter; }
    ui::BandActions& getActions() noexcept { return actions; }
    juce::Button& getBypassButton() noexcept { return bypass; }
    /** 0 = pre, 1 = post, 2 = side chain. */
    juce::Button& getAnalyzerButton(int index) noexcept;
    juce::Button& getRangeButton() noexcept { return rangeButton; }
    juce::Button& getAutoGainButton() noexcept { return autoGain; }
    juce::Button& getPhaseButton() noexcept { return phase; }
    juce::Slider& getOutputChip() noexcept { return outputChip; }
    juce::Slider& getPanChip() noexcept { return panChip; }
    juce::Slider& getScaleChip() noexcept { return scaleChip; }
    animalaudio::ui::ChoicePillRow& getResolutionRow() noexcept { return *resolution; }

    /** One UI frame now (analysers, meters, band panel); the timer does this at 30 Hz. */
    void refreshNow();
    /** Fixed animation time for deterministic snapshots. */
    void setTimeOverride(double seconds);

private:
    void timerCallback() override { refreshNow(); }
    void selectionChanged();
    void placeBandPanel();
    void navigate(int direction);
    void toggleAnalyzer(int index);
    void cycleRange();
    void saveUiState();
    void loadUiState();
    juce::String statusKey() const;

    QuailProcessor& owner;
    animalaudio::ui::Theme theme;
    ui::QuailLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow{this, 500}; // plugins have no app-wide tooltip window; without it none shows
    ui::BandActions actions;

    animalaudio::ui::BrandBadge brand;
    ui::EqDisplay display;
    ui::BandPanel bandPanel;
    ui::OutputMeter meter;
    animalaudio::ui::PillButton bypass;
    animalaudio::ui::PillButton analyzerPre, analyzerPost, analyzerSidechain, rangeButton;
    animalaudio::ui::PillButton autoGain, phase;
    ui::ValueChip scaleChip, panChip, outputChip;
    std::unique_ptr<animalaudio::ui::ChoicePillRow> resolution;

    std::unique_ptr<juce::ButtonParameterAttachment> bypassAttachment, autoGainAttachment, phaseAttachment;
    std::unique_ptr<juce::SliderParameterAttachment> scaleAttachment, panAttachment, outputAttachment;

    juce::Rectangle<int> topBar, bottomBar, content;
    juce::String lastStatus;
    bool restoringSize = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuailEditor)
};
} // namespace quail
