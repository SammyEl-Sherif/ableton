#pragma once

#include "PillButton.h"
#include "Theme.h"

#include <animalaudio/core/CurveState.h>

#include <memory>
#include <vector>

namespace animalaudio::ui
{
/**
 * Preset row under the curve editor: prev/next arrows, the current preset name, FLIP / INVERT, and a grid
 * of curve thumbnails (8 per row). Bound to a CurveState; follows external changes.
 */
class CurvePresetPanel : public juce::Component, private juce::ChangeListener
{
public:
    CurvePresetPanel(core::CurveState& state, const Theme& theme);
    ~CurvePresetPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    int getNumPresetButtons() const noexcept { return static_cast<int>(presetButtons.size()); }
    juce::Button& getPresetButton(int index);
    juce::Button& getPrevButton() { return *prevButton; }
    juce::Button& getNextButton() { return *nextButton; }
    juce::Button& getFlipButton() { return *flipButton; }
    juce::Button& getInvertButton() { return *invertButton; }

private:
    class PresetButton;

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void stepPreset(int delta);
    void refresh();

    core::CurveState& curveState;
    Palette palette;

    std::unique_ptr<PillButton> prevButton, nextButton, flipButton, invertButton;
    std::vector<std::unique_ptr<PresetButton>> presetButtons;
    juce::Rectangle<int> nameArea;
};
} // namespace animalaudio::ui
