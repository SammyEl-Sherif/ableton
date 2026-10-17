#pragma once

#include "OwlProcessor.h"

#include <animalaudio/dsp/Spectrum.h>
#include <animalaudio/ui/Theme.h>

#include <array>
#include <vector>

namespace owl::ui
{
/**
 * The big display: live post-EQ spectrum, each band's contribution, the summed response curve and one draggable
 * node per band (drag = frequency + gain, wheel over the mid node = Q, double-click = gain back to 0 dB).
 */
class ResponseView : public juce::Component, private juce::Timer
{
public:
    static constexpr double kMinHz = 20.0;
    static constexpr double kMaxHz = 20000.0;
    static constexpr double kRangeDb = 24.0;

    ResponseView(OwlProcessor& processor, const animalaudio::ui::Theme& theme);
    ~ResponseView() override;

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    juce::Rectangle<float> getPlotArea() const;
    float xForFrequency(double hz) const;
    double frequencyForX(float x) const;
    float yForDb(double db) const;
    double dbForY(float y) const;
    juce::Point<float> nodePosition(int band) const;
    int bandAt(juce::Point<float> position) const;

    /** Drains the processor's analyser FIFO and refreshes the spectrum (the timer does this at 30 Hz). */
    void updateSpectrum();

    /** Fixed animation time for deterministic snapshots (tests/screenshots); negative = wall clock. */
    void setTimeOverride(double seconds) { timeOverride = seconds; }

private:
    static constexpr int kSpectrumPoints = 240;
    static constexpr float kNodeHitRadius = 14.0f;

    void timerCallback() override;
    void setParameter(const juce::ParameterID& id, float value);
    void drawGrid(juce::Graphics&, juce::Rectangle<float> plot);
    void drawSpectrum(juce::Graphics&, juce::Rectangle<float> plot);
    void drawCurves(juce::Graphics&, juce::Rectangle<float> plot, const dsp::Settings& settings);
    void drawNodes(juce::Graphics&, const dsp::Settings& settings);
    void drawReadout(juce::Graphics&, int band, const dsp::Settings& settings);

    OwlProcessor& processor;
    animalaudio::ui::Theme theme;
    animalaudio::ui::Palette colours;

    animalaudio::dsp::SpectrumAnalyser analyser{12};
    std::vector<float> scratch;
    std::array<float, kSpectrumPoints> shownDb{};

    int hoveredBand = -1;
    int draggedBand = -1;
    double timeOverride = -1.0;
};
} // namespace owl::ui
