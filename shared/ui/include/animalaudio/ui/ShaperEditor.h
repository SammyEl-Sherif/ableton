#pragma once

#include "BrandBadge.h"
#include "ChoicePillRow.h"
#include "CurveEditor.h"
#include "CurvePresetPanel.h"
#include "Theme.h"
#include "ThemedKnob.h"

#include <animalaudio/core/CurveShaperProcessor.h>

#include <memory>

namespace animalaudio::ui
{
/**
 * The Animal Audio "shaper" editor (Duck's layout language), fully driven by a Theme:
 *
 *   left   brand badge, big NAME + tagline + mascot, big MIX knob with read-out, fallback-BPM slider
 *   right  division pills + sync status, curve editor, preset row (prev/next, name, flip/invert, grid)
 *
 * Every colour and every animal-specific drawing comes from the Theme, so a new plugin only supplies
 * a Theme (palette + Motif) and its processor derives from core::CurveShaperProcessor.
 */
class ShaperEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int kWidth = 820;
    static constexpr int kHeight = 470;

    ShaperEditor(core::CurveShaperProcessor& processor, const Theme& theme);
    ~ShaperEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    const Theme& getTheme() const noexcept { return theme; }
    BrandBadge& getBrandBadge() noexcept { return brand; }
    CurveEditor& getCurveEditor() noexcept { return curveEditor; }
    ChoicePillRow& getDivisionRow() noexcept { return divisionRow; }
    juce::Button& getDivisionButton(int index) { return divisionRow.getPill(index); }
    CurvePresetPanel& getPresetPanel() noexcept { return presetPanel; }
    juce::Slider& getMixKnob() noexcept { return mixKnob; }

private:
    void timerCallback() override;

    core::CurveShaperProcessor& owner;
    Theme theme;
    Palette palette;

    juce::TooltipWindow tooltipWindow{this,
                                      500}; // plugins have no app-wide tooltip window; without it no tooltip shows
    BrandBadge brand;
    ThemedKnob mixKnob;
    juce::Slider bpmSlider;
    CurveEditor curveEditor;
    ChoicePillRow divisionRow;
    CurvePresetPanel presetPanel;

    std::unique_ptr<juce::SliderParameterAttachment> mixAttachment, bpmAttachment;

    juce::Rectangle<int> statusArea;
    dsp::SyncSource shownSource = dsp::SyncSource::FreeRun;
    double ambientTimeSeconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShaperEditor)
};
} // namespace animalaudio::ui
