#pragma once

#include <animalaudio/core/CurveShaperProcessor.h>
#include <animalaudio/ui/ShaperEditor.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdlib>

namespace animalaudio::testing
{
struct FakePlayHead : juce::AudioPlayHead
{
    juce::Optional<PositionInfo> position;
    juce::Optional<PositionInfo> getPosition() const override { return position; }
};

/** A processor prepared at a sample rate, with a fake host playhead and helpers to push audio through. */
template <class Processor>
struct ProcessorHarness
{
    Processor processor;
    FakePlayHead playHead;
    juce::MidiBuffer midi;

    explicit ProcessorHarness(double sampleRate = 48000.0, int blockSize = 512)
    {
        processor.setPlayHead(&playHead);
        processor.setPlayConfigDetails(2, 2, sampleRate, blockSize);
        processor.prepareToPlay(sampleRate, blockSize);
    }

    void setParam(const juce::ParameterID& id, float realValue)
    {
        auto* p = processor.apvts.getParameter(id.getParamID());
        p->setValueNotifyingHost(p->convertTo0to1(realValue));
    }

    /** Runs `numSamples` of a constant 1.0 signal (both channels) and returns the output. */
    juce::AudioBuffer<float> run(int numSamples, int blockSize = 512)
    {
        juce::AudioBuffer<float> all(2, numSamples);
        for (int done = 0; done < numSamples; done += blockSize)
        {
            const int n = std::min(blockSize, numSamples - done);
            juce::AudioBuffer<float> block(2, n);
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill(block.getWritePointer(ch), 1.0f, n);
            processor.processBlock(block, midi);
            for (int ch = 0; ch < 2; ++ch)
                all.copyFrom(ch, done, block, ch, 0, n);
        }
        return all;
    }
};

inline void setDivisionFromHost(juce::AudioProcessorValueTreeState& apvts, int index)
{
    auto* param = apvts.getParameter(core::ids::division.getParamID());
    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
}

inline int divisionIndex(juce::AudioProcessorValueTreeState& apvts)
{
    return dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(core::ids::division.getParamID()))->getIndex();
}

inline juce::MouseEvent makeMouseEvent(juce::Component& c, juce::Point<float> pos, bool popup = false)
{
    const auto mods = popup ? juce::ModifierKeys::rightButtonModifier : juce::ModifierKeys::leftButtonModifier;
    return juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            &c, &c, juce::Time::getCurrentTime(), pos, juce::Time::getCurrentTime(), 1, false);
}

/** Button::triggerClick() posts a message; pump the loop so the click is delivered. */
inline void click(juce::Button& button)
{
    button.triggerClick();
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
}

inline juce::Component* topmostChildAt(juce::Component& parent, juce::Point<int> point)
{
    const auto& children = parent.getChildren();
    for (int i = children.size(); --i >= 0;)
        if (children.getUnchecked(i)->isVisible() && children.getUnchecked(i)->getBounds().contains(point))
            return children.getUnchecked(i);
    return nullptr;
}

inline void writePngIfRequested(const juce::Image& image, const juce::String& fileName)
{
    const char* dir = std::getenv("AA_SCREENSHOT_DIR");
    if (dir == nullptr)
        return;
    juce::File file = juce::File(dir).getChildFile(fileName);
    file.getParentDirectory().createDirectory();
    file.deleteFile();
    juce::FileOutputStream out(file);
    REQUIRE(out.openedOk());
    juce::PNGImageFormat().writeImageToStream(image, out);
}

inline dsp::ShapeCurve gateCurve()
{
    return dsp::ShapeCurve(std::vector<dsp::ShapeCurve::Node>{
        {0.0f, 0.0f, 0.0f}, {0.49f, 0.0f, 0.0f}, {0.5f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}});
}
} // namespace animalaudio::testing
