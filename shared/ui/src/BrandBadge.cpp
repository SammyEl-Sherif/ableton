#include <animalaudio/ui/BrandBadge.h>

namespace animalaudio::ui
{
juce::Path makePawPath(juce::Rectangle<float> area)
{
    juce::Path p;
    const float w = area.getWidth();
    const float h = area.getHeight();
    const auto at = [&](float fx, float fy) { return juce::Point<float>(area.getX() + fx * w, area.getY() + fy * h); };

    // main pad
    p.addEllipse(
        juce::Rectangle<float>(0.26f * w, 0.5f * h, 0.48f * w, 0.42f * h).translated(area.getX(), area.getY()));

    const float toeW = 0.2f * w;
    const float toeH = 0.26f * h;
    const juce::Point<float> toes[] = {at(0.12f, 0.42f), at(0.36f, 0.2f), at(0.64f, 0.2f), at(0.88f, 0.42f)};
    for (auto c : toes)
        p.addEllipse(c.x - toeW * 0.5f, c.y - toeH * 0.5f, toeW, toeH);
    return p;
}

void BrandBadge::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const float pawSize = bounds.getHeight();
    auto pawArea = bounds.removeFromLeft(pawSize).reduced(pawSize * 0.12f);

    g.setColour(colours.accent);
    g.fillPath(makePawPath(pawArea));

    bounds.removeFromLeft(4.0f);
    g.setColour(colours.textDim);
    g.setFont(makeFont(bounds.getHeight() * 0.62f));
    g.drawText(kCompanyNameUpper, bounds, juce::Justification::centredLeft, false);
}
} // namespace animalaudio::ui
