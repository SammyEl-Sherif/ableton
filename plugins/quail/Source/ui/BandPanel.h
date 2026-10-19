#pragma once

#include "BandActions.h"
#include "Widgets.h"

#include <functional>
#include <map>
#include <memory>

namespace quail::ui
{
/**
 * Floating card for the selected band. Left: power, band number with previous/next, shape, slope and stereo
 * placement, solo (held) and remove. Middle: FREQ / GAIN / Q (the gain knob also shows the dynamic range and the
 * live gain). Right: dynamics (on, side chain, spectral, spectral tilt, threshold over the trigger meter + auto)
 * and the RANGE / ATTACK / RELEASE / DENSITY knobs. Everything takes the band's colour.
 *
 * The component is the card plus a transparent kShadow margin for its drop shadow; clicks in the margin fall
 * through to whatever is underneath.
 */
class BandPanel : public juce::Component
{
public:
    static constexpr int kWidth = 860;
    static constexpr int kHeight = 136;
    static constexpr int kShadow = 14;

    BandPanel(BandActions& actions, const animalaudio::ui::Theme& theme);
    ~BandPanel() override;

    /** -1 detaches everything. */
    void setBand(int band);
    int getBand() const noexcept { return band; }

    /** Live values and enablement (the editor calls this at 30 Hz). */
    void refresh();

    std::function<void(int direction)> onNavigate;
    std::function<void()> onRemove;

    juce::Slider& getKnob(BandParam param);
    /** enabled (power), dynamics, sidechain, spectral, spectralTilt or autoThreshold. */
    juce::Button& getToggle(BandParam param);
    /** shape, slope or placement. */
    MenuPill& getMenu(BandParam param);
    ThresholdBar& getThreshold() noexcept { return threshold; }
    juce::Button& getSoloButton() noexcept { return solo; }
    juce::Button& getRemoveButton() noexcept { return remove; }
    juce::Button& getPreviousButton() noexcept { return previous; }
    juce::Button& getNextButton() noexcept { return next; }

    /** The card inside the shadow margin, in this component's coordinates. */
    juce::Rectangle<int> getCardBounds() const noexcept { return getLocalBounds().reduced(kShadow); }

    bool hitTest(int x, int y) override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        std::unique_ptr<QuailKnob> knob;
        juce::String label;
        std::unique_ptr<juce::SliderParameterAttachment> attachment;
    };

    void addKnob(BandParam param, const juce::String& label, const juce::String& tooltip, bool bipolar);
    void addToggle(BandParam param, const juce::String& text, const juce::String& tooltip);
    void toggle(BandParam param);
    void ensureDynamics();
    void applyTint(juce::Colour colour);
    juce::String stateKey() const;

    BandActions& actions;
    animalaudio::ui::Theme theme;
    animalaudio::ui::Palette colours;
    int band = -1;

    std::map<BandParam, Knob> knobs;
    std::map<BandParam, std::unique_ptr<QuailToggle>> toggles;
    MenuPill shapeMenu, slopeMenu, placementMenu;
    QuailToggle previous{"", colours}, next{"", colours};
    QuailToggle solo{"SOLO", colours}, remove{"REMOVE", colours};
    ThresholdBar threshold;
    juce::Image shadow;
    juce::String lastKey;
};
} // namespace quail::ui
