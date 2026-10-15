#pragma once

#include <animalaudio/ui/RotaryKnob.h>

namespace duck::ui
{
/** MIX knob: the cap is a rubber duck seen from above, its bill is the value pointer. */
class DuckKnob : public animalaudio::ui::RotaryKnob
{
public:
    using animalaudio::ui::RotaryKnob::RotaryKnob;

protected:
    void drawCap(juce::Graphics& g, juce::Point<float> centre, float radius, float angle) override;
};
} // namespace duck::ui
