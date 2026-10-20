#pragma once

#include "Theme.h"

#include <animalaudio/core/CurveShaperProcessor.h>

namespace animalaudio::ui
{
/**
 * Interactive curve editor.
 *   drag a node           -> move it (end nodes move vertically only)
 *   drag a diamond handle -> bend the segment
 *   double-click empty    -> add a node        double-click / right-click node -> delete it
 *   double-click diamond  -> straighten the segment
 * The theme's mascot rides the curve at the current playhead phase, and the signal level (post-duck) is
 * drawn behind the curve as a faint scope.
 */
class CurveEditor : public juce::Component, private juce::ChangeListener, private juce::Timer
{
public:
    CurveEditor(core::CurveShaperProcessor& processor, const Theme& theme);
    ~CurveEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override {}

    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;

    /** Fixed playhead for screenshots / tests; negative = follow the engine. */
    void setPhaseOverride(float phase) { phaseOverride = phase; }

    /** Re-reads the curve from the state immediately (the change listener does this asynchronously). */
    void changeListenerCallbackForTests() { curve = processor.getCurveState().getCurve(); }

    /** Hit-test helpers exposed for tests. */
    juce::Rectangle<float> getPlotArea() const;
    juce::Point<float> toScreen(float x, float y) const;
    juce::Point<float> fromScreen(juce::Point<float> p) const;

private:
    enum class Target
    {
        None,
        Node,
        Bend
    };
    struct Hit
    {
        Target target = Target::None;
        std::size_t index = 0;
    };

    Hit hitTestAt(juce::Point<float> p) const;
    juce::Point<float> bendHandlePosition(std::size_t segment) const;
    void commit(int presetIndex = -1);

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    core::CurveShaperProcessor& processor;
    Theme theme;
    Palette colours;
    dsp::ShapeCurve curve;
    Hit hovered;
    Hit dragging;
    float phaseOverride = -1.0f;
    double ambientTimeSeconds = 0.0;

    static constexpr float kNodeHitRadius = 10.0f;
    static constexpr float kBendHitRadius = 9.0f;
};
} // namespace animalaudio::ui
