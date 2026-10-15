#pragma once

#include "Palette.h"

namespace animalaudio::ui
{
/** Draws a simple paw print (pad + four toes) into `area`. */
juce::Path makePawPath(juce::Rectangle<float> area);

/** The "ANIMAL AUDIO" wordmark with a paw glyph. Place it on every plugin UI. */
class BrandBadge : public juce::Component
{
public:
    explicit BrandBadge(const Palette& palette)
        : colours(palette)
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override;

private:
    Palette colours;
};
} // namespace animalaudio::ui
