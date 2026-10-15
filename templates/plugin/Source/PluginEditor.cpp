#include "PluginEditor.h"

namespace __NAME_LOWER__
{
namespace
{
animalaudio::ui::Palette makePalette()
{
    // TODO: pick this plugin's colours. Keep the Animal Audio BrandBadge somewhere visible.
    animalaudio::ui::Palette p;
    p.background = juce::Colour(0xff17171c);
    p.panel = juce::Colour(0xff211f27);
    p.panelRaised = juce::Colour(0xff2f2c38);
    p.outline = juce::Colour(0xff45414f);
    p.text = juce::Colour(0xfff1ecf8);
    p.textDim = juce::Colour(0xff9c96ac);
    p.accent = juce::Colour(0xffffcb1f);
    p.accentAlt = juce::Colour(0xffff8a1f);
    p.onAccent = juce::Colour(0xff1c1708);
    return p;
}
} // namespace

__NAME__Editor::__NAME__Editor(__NAME__Processor& p)
    : juce::AudioProcessorEditor(&p)
    , palette(makePalette())
    , brand(palette)
    , knob(palette)
    , attachment(*p.apvts.getParameter("gain"), knob)
{
    addAndMakeVisible(brand);
    addAndMakeVisible(knob);
    setSize(360, 320);
}

void __NAME__Editor::paint(juce::Graphics& g)
{
    g.fillAll(palette.background);
    g.setColour(palette.accent);
    g.setFont(animalaudio::ui::makeFont(34.0f));
    g.drawText("__NAME_UPPER__", getLocalBounds().removeFromTop(90).reduced(20, 0).withTrimmedTop(36),
               juce::Justification::centredLeft, false);
}

void __NAME__Editor::resized()
{
    auto area = getLocalBounds().reduced(20);
    brand.setBounds(area.removeFromTop(22).withWidth(170));
    area.removeFromTop(60);
    knob.setBounds(area.withSizeKeepingCentre(180, 180));
}
} // namespace __NAME_LOWER__
