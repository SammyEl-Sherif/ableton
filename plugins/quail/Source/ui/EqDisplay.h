#pragma once

#include "BandActions.h"

#include <animalaudio/dsp/Spectrum.h>
#include <animalaudio/ui/Theme.h>

#include <array>
#include <functional>
#include <vector>

namespace quail::ui
{
/**
 * The big display: pre / post / side-chain spectra (red glow where the side chain collides with the signal), each
 * band's curve and dynamic range, the summed curve per stereo placement (static and live) and a node per band.
 *
 * Mouse: double-click empty space adds a band, double-click a node removes it. Drag a node for frequency + gain
 * (Q for shapes without gain), alt-drag for Q, shift for fine moves. Wheel over a node = Q (slope for 6 dB/oct
 * cuts), alt+wheel = dynamic range. Drag the diamond for the dynamic range, drag empty space to lasso, right-click
 * for a menu. Keys: delete/backspace removes the selection, escape clears it, cmd/ctrl+A selects every band.
 */
class EqDisplay : public juce::Component
{
public:
    static constexpr double kMinHz = 10.0;
    static constexpr double kMaxHz = 30000.0;
    static constexpr std::array<float, 4> kRanges{3.0f, 6.0f, 12.0f, 30.0f};

    struct Analyzer
    {
        bool pre = false;
        bool post = true;
        bool sidechain = true;
    };

    EqDisplay(BandActions& actions, const animalaudio::ui::Theme& theme);

    void setAnalyzer(Analyzer options);
    Analyzer getAnalyzer() const noexcept { return analyzer; }
    void setRangeDb(float db);
    float getRangeDb() const noexcept { return rangeDb; }

    /** Drains the analyser FIFOs and follows the live dynamics (the editor calls this at 30 Hz). */
    void refresh();
    /** Fixed animation time for deterministic snapshots (tests/screenshots); negative = wall clock. */
    void setTimeOverride(double seconds) { timeOverride = seconds; }

    juce::Rectangle<float> getPlotArea() const;
    float xForFrequency(double hz) const;
    double frequencyForX(float x) const;
    float yForDb(double db) const;
    double dbForY(float y) const;
    juce::Point<float> nodePosition(int band) const;
    bool hasRangeHandle(int band) const;
    juce::Point<float> rangeHandlePosition(int band) const;
    int bandAt(juce::Point<float> position) const;
    int rangeHandleAt(juce::Point<float> position) const;

    int getSelectedBand() const noexcept { return primary; }
    bool isSelected(int band) const noexcept;
    int numSelected() const noexcept;
    /** -1 clears; `add` keeps the current selection. */
    void select(int band, bool add = false);
    void selectAll();
    std::function<void()> onSelectionChanged;
    /** The band whose node is being dragged, or -1. */
    int getDraggedBand() const noexcept { return dragBand; }

    /** Display height of a node for shapes without gain: Q mapped so the neutral Q of 1 (Butterworth for cuts) sits
        on the 0 dB line, 6 dB per octave of Q. */
    static double nodeDbForQ(float q);
    static float qForNodeDb(double db);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed(const juce::KeyPress&) override;

private:
    static constexpr int kSpectrumPoints = 256;
    static constexpr float kNodeRadius = 7.0f;
    static constexpr float kHitRadius = 13.0f;

    struct SpectrumFeed
    {
        animalaudio::dsp::SpectrumAnalyser analyser{12};
        std::array<float, kSpectrumPoints> shown{};
    };

    enum class Drag
    {
        none,
        node,
        q,
        range,
        lasso
    };

    struct DragOrigin
    {
        float hz = 1000.0f, gainDb = 0.0f, q = 1.0f;
    };

    void feed(SpectrumFeed& spectrum, QuailProcessor::AnalyserFifo& fifo, bool shown);
    void updateCurves(const dsp::Settings& settings);
    juce::Path curvePath(const std::vector<float>& db) const;
    juce::Path spectrumPath(const SpectrumFeed& spectrum, juce::Rectangle<float> plot) const;
    float spectrumY(float db, juce::Rectangle<float> plot) const;
    double nodeDb(const dsp::BandSettings& band, float gainScale) const;
    void notifySelection();

    void drawGrid(juce::Graphics&, juce::Rectangle<float> plot);
    void drawSpectra(juce::Graphics&, juce::Rectangle<float> plot);
    void drawBands(juce::Graphics&, const dsp::Settings& settings);
    void drawTotals(juce::Graphics&, const dsp::Settings& settings);
    void drawNodes(juce::Graphics&, const dsp::Settings& settings);
    void drawReadout(juce::Graphics&, int band, const dsp::Settings& settings);
    void drawCursor(juce::Graphics&, juce::Rectangle<float> plot);
    void drawFrame(juce::Graphics&, juce::Rectangle<float> plot, bool empty);

    void beginNodeDrag(int band, const juce::MouseEvent& e, bool forQ);
    void endDrag();
    void showBandMenu(int band);
    void showBackgroundMenu(juce::Point<float> position);
    void removeSelected();

    BandActions& actions;
    QuailProcessor& processor;
    animalaudio::ui::Theme theme;
    animalaudio::ui::Palette colours;
    Analyzer analyzer;
    float rangeDb = 12.0f;
    double timeOverride = -1.0;

    SpectrumFeed pre, post, side;
    std::vector<float> scratch;

    // curves sampled at xs (2 px apart)
    std::vector<float> xs, hzs;
    std::array<std::vector<float>, dsp::kNumBands> staticDb, rangeEdgeDb, liveDb;
    std::array<dsp::BandSettings, dsp::kNumBands> curveKeys{};
    std::array<bool, dsp::kNumBands> curveValid{};
    std::array<bool, dsp::kNumBands> liveMoving{};
    std::array<std::vector<float>, dsp::kNumPlacements> groupStatic, groupLive;
    std::array<bool, dsp::kNumPlacements> groupUsed{}, groupMoving{};
    float curveScale = -1.0f;
    double curveRate = 0.0;

    std::array<bool, dsp::kNumBands> selected{};
    int primary = -1;
    int hoverBand = -1;
    int hoverRange = -1;
    bool hoverInside = false;
    juce::Point<float> hoverPoint;

    Drag drag = Drag::none;
    int dragBand = -1;
    juce::Point<float> dragStart;
    std::array<DragOrigin, dsp::kNumBands> dragFrom{};
    std::vector<std::pair<int, BandParam>> openGestures;
    juce::Rectangle<float> lasso;
    std::array<bool, dsp::kNumBands> lassoBase{};
};
} // namespace quail::ui
