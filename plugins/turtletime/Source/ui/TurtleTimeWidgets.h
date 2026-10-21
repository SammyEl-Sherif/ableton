#pragma once

#include <animalaudio/ui/RotaryKnob.h>
#include <animalaudio/ui/Theme.h>

namespace turtletime::ui
{
/** Font with extra letter spacing, for the small upper-case labels. */
juce::Font trackedFont(float height, bool bold, float kerning = 0.08f);

/** Large knob: tick ring, thin glowing value arc, cap from the Motif. */
class TurtleKnob : public animalaudio::ui::RotaryKnob
{
public:
    explicit TurtleKnob(const animalaudio::ui::Theme& theme);
    void paint(juce::Graphics& g) override;

private:
    std::shared_ptr<const animalaudio::ui::Motif> motif;
};

/** One cell of the pattern matrix. Fires on mouse down (see AGENTS.md "Gotchas"). */
class PatternCell : public juce::Button
{
public:
    PatternCell(const juce::String& label, const animalaudio::ui::Palette& palette);
    void paintButton(juce::Graphics& g, bool isMouseOver, bool isButtonDown) override;

private:
    animalaudio::ui::Palette colours;
};
} // namespace turtletime::ui
