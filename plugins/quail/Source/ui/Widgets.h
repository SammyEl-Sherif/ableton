#pragma once

#include <animalaudio/ui/ThemedKnob.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <functional>
#include <memory>

namespace quail::ui
{
/** Popup menus, tooltips and text boxes in the palette's colours. */
class QuailLookAndFeel : public juce::LookAndFeel_V4
{
public:
    explicit QuailLookAndFeel(const animalaudio::ui::Palette& palette);

    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawTooltip(juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds(const juce::String& text, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override;

private:
    animalaudio::ui::Palette colours;
};

/** A pill showing a choice parameter's value with a caret; clicking opens the list. */
class MenuPill : public juce::Button
{
public:
    MenuPill(const animalaudio::ui::Palette& palette, const juce::String& tooltip);

    /** Follows `parameter` (nullptr detaches). `itemEnabled` can grey out choices. */
    void attachTo(juce::RangedAudioParameter* parameter, std::function<bool(int)> itemEnabled = {});
    juce::RangedAudioParameter* getParameter() const noexcept { return parameter; }
    int getIndex() const noexcept { return index; }
    /** Chooses an item as the menu would (one host gesture). */
    void choose(int item);

    void paintButton(juce::Graphics&, bool isMouseOver, bool isButtonDown) override;
    void clicked() override;

private:
    animalaudio::ui::Palette colours;
    juce::RangedAudioParameter* parameter = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    std::function<bool(int)> enabledItem;
    int index = 0;
};

/** Compact "LABEL value" chip: drag up/down or scroll to change, double-click for the default. */
class ValueChip : public juce::Slider
{
public:
    ValueChip(const juce::String& label, const animalaudio::ui::Palette& palette);

    void paint(juce::Graphics&) override;
    /** Text shown for the value; defaults to the attached parameter's text. */
    std::function<juce::String(double)> valueText;

private:
    juce::String label;
    animalaudio::ui::Palette colours;
};

/**
 * Band knob with the Quail cap. A gain knob can also show the band's dynamic range as an inner arc from the gain
 * to gain + range, with a dot at the gain the dynamics apply right now.
 */
class QuailKnob : public animalaudio::ui::ThemedKnob
{
public:
    explicit QuailKnob(const animalaudio::ui::Theme& theme);

    void paint(juce::Graphics&) override;
    void setDynamicRange(bool shown, double rangeDb, double liveDb);

private:
    bool rangeShown = false;
    double range = 0.0;
    double live = 0.0;
};

/**
 * Threshold of a dynamic band over a live level meter of its trigger. Drag to set the threshold (turns auto off),
 * double-click to toggle auto threshold.
 */
class ThresholdBar : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float kMinDb = -80.0f;
    static constexpr float kMaxDb = 0.0f;

    explicit ThresholdBar(const animalaudio::ui::Palette& palette);

    void attachTo(juce::RangedAudioParameter* threshold, juce::RangedAudioParameter* autoThreshold);
    /** Live trigger level and the threshold the band is really using (auto or manual), dB. */
    void setLevels(float triggerDb, float effectiveThresholdDb, bool active);

    float xForDb(float db) const noexcept;
    float dbForX(float x) const noexcept;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> track() const noexcept;
    bool isAuto() const noexcept;

    animalaudio::ui::Palette colours;
    juce::RangedAudioParameter* threshold = nullptr;
    juce::RangedAudioParameter* autoThreshold = nullptr;
    float trigger = -120.0f;
    float effective = -30.0f;
    bool live = false;
    bool dragging = false;
};

/** Output peak meter (L, R) plus the side-chain level, with peak hold and a clip light. */
class OutputMeter : public juce::Component, public juce::SettableTooltipClient
{
public:
    static constexpr float kMinDb = -60.0f;
    static constexpr float kMaxDb = 6.0f;

    explicit OutputMeter(const animalaudio::ui::Palette& palette);

    /** Linear peaks since the last call (one call per UI frame). */
    void push(float left, float right, float sidechain, bool sidechainActive);
    float getLevelDb(int channel) const noexcept { return levels[static_cast<std::size_t>(channel)]; }

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;

private:
    float yForDb(float db, juce::Rectangle<float> area) const noexcept;

    animalaudio::ui::Palette colours;
    std::array<float, 3> levels{kMinDb, kMinDb, kMinDb};
    std::array<float, 3> holds{kMinDb, kMinDb, kMinDb};
    std::array<int, 3> holdFrames{};
    bool clipped = false;
    bool sidechainShown = false;
};
} // namespace quail::ui
