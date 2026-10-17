#include "ResponseView.h"

#include "OwlTheme.h"

namespace owl::ui
{
using animalaudio::ui::makeFont;

namespace
{
constexpr float kSpectrumFloorDb = -90.0f;
constexpr float kSpectrumTiltDbPerOctave = 4.5f;
constexpr const char* kBandNames[] = {"LOW", "MID", "HIGH"};
constexpr const char* kBandLetters[] = {"L", "M", "H"};
} // namespace

ResponseView::ResponseView(OwlProcessor& p, const animalaudio::ui::Theme& themeToUse)
    : processor(p)
    , theme(themeToUse)
    , colours(themeToUse.palette)
    , scratch(static_cast<std::size_t>(OwlProcessor::AnalyserFifo::capacity()))
{
    shownDb.fill(kSpectrumFloorDb);
    setOpaque(false);
    startTimerHz(30);
}

ResponseView::~ResponseView()
{
    stopTimer();
}

juce::Rectangle<float> ResponseView::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(16.0f, 14.0f).withTrimmedTop(12.0f).withTrimmedBottom(14.0f);
}

float ResponseView::xForFrequency(double hz) const
{
    const auto plot = getPlotArea();
    const double t = std::log(juce::jlimit(kMinHz, kMaxHz, hz) / kMinHz) / std::log(kMaxHz / kMinHz);
    return plot.getX() + static_cast<float>(t) * plot.getWidth();
}

double ResponseView::frequencyForX(float x) const
{
    const auto plot = getPlotArea();
    const double t = juce::jlimit(0.0, 1.0, static_cast<double>((x - plot.getX()) / plot.getWidth()));
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

float ResponseView::yForDb(double db) const
{
    const auto plot = getPlotArea();
    return plot.getCentreY() - static_cast<float>(db / kRangeDb) * plot.getHeight() * 0.5f;
}

double ResponseView::dbForY(float y) const
{
    const auto plot = getPlotArea();
    return static_cast<double>((plot.getCentreY() - y) / (plot.getHeight() * 0.5f)) * kRangeDb;
}

juce::Point<float> ResponseView::nodePosition(int band) const
{
    const auto s = processor.getSettings();
    return {xForFrequency(s.frequency(band)), yForDb(s.gainDb(band))};
}

int ResponseView::bandAt(juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = kNodeHitRadius;
    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        const float d = nodePosition(band).getDistanceFrom(position);
        if (d <= bestDistance)
        {
            best = band;
            bestDistance = d;
        }
    }
    return best;
}

void ResponseView::setParameter(const juce::ParameterID& id, float value)
{
    if (auto* param = processor.apvts.getParameter(id.getParamID()))
        param->setValueNotifyingHost(param->convertTo0to1(value));
}

void ResponseView::updateSpectrum()
{
    auto& fifo = processor.getAnalyserFifo();
    bool fresh = false;
    for (int n = fifo.pop(scratch.data(), static_cast<int>(scratch.size())); n > 0;
         n = fifo.pop(scratch.data(), static_cast<int>(scratch.size())))
    {
        analyser.write(scratch.data(), n);
        fresh = true;
    }
    if (fresh)
        analyser.compute();

    const double sampleRate = processor.getDisplaySampleRate();
    const double ratio = kMaxHz / kMinHz;
    for (int i = 0; i < kSpectrumPoints; ++i)
    {
        const auto at = [&](double index) { return kMinHz * std::pow(ratio, index / (kSpectrumPoints - 1)); };
        const double centre = at(i);
        float target = kSpectrumFloorDb;
        if (fresh)
            target = juce::jmax(kSpectrumFloorDb,
                                analyser.peakDbBetween(at(i - 0.5), at(i + 0.5), sampleRate) +
                                    kSpectrumTiltDbPerOctave * static_cast<float>(std::log2(centre / 1000.0)));
        auto& shown = shownDb[static_cast<std::size_t>(i)];
        shown = target > shown ? target : shown + (target - shown) * 0.18f;
    }
}

void ResponseView::timerCallback()
{
    updateSpectrum();
    repaint();
}

void ResponseView::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = getPlotArea();
    const auto settings = processor.getSettings();
    const double seconds = timeOverride >= 0.0 ? timeOverride : juce::Time::getMillisecondCounterHiRes() * 0.001;

    g.setColour(colours.panel.darker(0.3f));
    g.fillRoundedRectangle(bounds, 12.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, 12.0f);
        g.reduceClipRegion(clip);

        g.setGradientFill(juce::ColourGradient(colours.ambient.withAlpha(0.0f), 0.0f, bounds.getCentreY(),
                                               colours.ambient.withAlpha(0.07f), 0.0f, bounds.getBottom(), false));
        g.fillRect(bounds);
        theme.motif->drawAmbient(g, bounds.withHeight(bounds.getHeight() * 0.6f), seconds, colours);

        drawGrid(g, plot);
        drawSpectrum(g, plot);
        drawCurves(g, plot, settings);
    }

    g.setColour(colours.textDim.withAlpha(0.55f));
    g.setFont(makeFont(9.5f, false));
    g.drawText("DRAG A BAND   |   SCROLL ON MID FOR Q   |   DOUBLE-CLICK TO FLATTEN",
               bounds.reduced(24.0f, 8.0f).withHeight(14.0f), juce::Justification::centredLeft, false);

    drawNodes(g, settings);
    const int readoutBand = draggedBand >= 0 ? draggedBand : hoveredBand;
    if (readoutBand >= 0)
        drawReadout(g, readoutBand, settings);
}

void ResponseView::drawGrid(juce::Graphics& g, juce::Rectangle<float> plot)
{
    g.setFont(makeFont(9.5f));
    for (double hz : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
    {
        const float x = xForFrequency(hz);
        g.setColour(colours.outline.withAlpha(hz == 100.0 || hz == 1000.0 || hz == 10000.0 ? 0.7f : 0.4f));
        g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        g.setColour(colours.textDim.withAlpha(0.75f));
        const juce::String label =
            hz >= 1000.0 ? juce::String(juce::roundToInt(hz / 1000.0)) + "k" : juce::String(juce::roundToInt(hz));
        g.drawText(label, juce::Rectangle<float>(x - 20.0f, plot.getBottom() + 2.0f, 40.0f, 12.0f),
                   juce::Justification::centred, false);
    }

    for (int db = -18; db <= 18; db += 6)
    {
        const float y = yForDb(db);
        g.setColour(colours.outline.withAlpha(db == 0 ? 0.95f : 0.35f));
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
        if (db != 0 && std::abs(db) != 18)
        {
            g.setColour(colours.textDim.withAlpha(0.6f));
            g.drawText((db > 0 ? "+" : "") + juce::String(db),
                       juce::Rectangle<float>(plot.getX() + 4.0f, y - 13.0f, 30.0f, 12.0f),
                       juce::Justification::centredLeft, false);
        }
    }
}

void ResponseView::drawSpectrum(juce::Graphics& g, juce::Rectangle<float> plot)
{
    juce::Path line;
    for (int i = 0; i < kSpectrumPoints; ++i)
    {
        const auto at = [&](int j)
        { return shownDb[static_cast<std::size_t>(juce::jlimit(0, kSpectrumPoints - 1, j))]; };
        const float db = 0.25f * at(i - 1) + 0.5f * at(i) + 0.25f * at(i + 1);
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / (kSpectrumPoints - 1);
        const float level = juce::jlimit(0.0f, 1.0f, (db - kSpectrumFloorDb) / -kSpectrumFloorDb);
        const float y = plot.getBottom() - level * plot.getHeight() * 0.92f;
        if (i == 0)
            line.startNewSubPath(x, y);
        else
            line.lineTo(x, y);
    }
    juce::Path fill(line);
    fill.lineTo(plot.getRight(), plot.getBottom());
    fill.lineTo(plot.getX(), plot.getBottom());
    fill.closeSubPath();

    g.setGradientFill(juce::ColourGradient(colours.ambient.withAlpha(0.2f), 0.0f, plot.getY(),
                                           colours.ambient.withAlpha(0.02f), 0.0f, plot.getBottom(), false));
    g.fillPath(fill);
    g.setColour(colours.ambient.withAlpha(0.32f));
    g.strokePath(line, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved));
}

void ResponseView::drawCurves(juce::Graphics& g, juce::Rectangle<float> plot, const dsp::Settings& settings)
{
    const double sampleRate = processor.getDisplaySampleRate();
    const float zeroY = yForDb(0.0);
    const int steps = juce::jmax(2, juce::roundToInt(plot.getWidth() / 2.0f));
    const auto curveFor = [&](auto&& dbAt)
    {
        juce::Path p;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(steps);
            const float y = juce::jlimit(plot.getY() - 4.0f, plot.getBottom() + 4.0f, yForDb(dbAt(frequencyForX(x))));
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
        }
        return p;
    };
    const auto closedToZero = [&](juce::Path p)
    {
        p.lineTo(plot.getRight(), zeroY);
        p.lineTo(plot.getX(), zeroY);
        p.closeSubPath();
        return p;
    };

    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        if (std::abs(settings.gainDb(band)) < 0.05)
            continue;
        const auto colour = bandColour(colours, band);
        const auto line = curveFor([&](double hz) { return dsp::bandResponseDb(settings, band, hz, sampleRate); });
        g.setColour(colour.withAlpha(0.12f));
        g.fillPath(closedToZero(line));
        g.setColour(colour.withAlpha(0.4f));
        g.strokePath(line, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved));
    }

    const auto total = curveFor([&](double hz) { return dsp::responseDb(settings, hz, sampleRate); });
    g.setGradientFill(juce::ColourGradient(colours.accent.withAlpha(0.26f), 0.0f, plot.getY(),
                                           colours.accentAlt.withAlpha(0.06f), 0.0f, plot.getBottom(), false));
    g.fillPath(closedToZero(total));
    g.setColour(colours.accentAlt.withAlpha(0.3f));
    g.strokePath(total, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour(colours.accent);
    g.strokePath(total, juce::PathStrokeType(2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ResponseView::drawNodes(juce::Graphics& g, const dsp::Settings& settings)
{
    const float zeroY = yForDb(0.0);
    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        const auto colour = bandColour(colours, band);
        const juce::Point<float> c(xForFrequency(settings.frequency(band)), yForDb(settings.gainDb(band)));
        const bool active = band == hoveredBand || band == draggedBand;
        const float r = active ? 11.0f : 9.0f;

        g.setColour(colour.withAlpha(0.45f));
        g.drawLine(c.x, c.y, c.x, zeroY, 1.0f);
        if (active)
        {
            g.setColour(colour.withAlpha(0.22f));
            g.fillEllipse(c.x - r - 6.0f, c.y - r - 6.0f, (r + 6.0f) * 2.0f, (r + 6.0f) * 2.0f);
        }
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(c.x - r, c.y - r + 1.5f, r * 2.0f, r * 2.0f);
        g.setColour(colour);
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour(colours.background.withAlpha(0.6f));
        g.drawEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f, 1.2f);
        g.setColour(colours.onAccent);
        g.setFont(makeFont(r * 1.15f));
        g.drawText(kBandLetters[band], juce::Rectangle<float>(c.x - r, c.y - r, r * 2.0f, r * 2.0f),
                   juce::Justification::centred, false);
    }
}

void ResponseView::drawReadout(juce::Graphics& g, int band, const dsp::Settings& settings)
{
    const auto text = [&](const juce::ParameterID& id)
    { return processor.apvts.getParameter(id.getParamID())->getCurrentValueAsText(); };
    juce::String line =
        juce::String(kBandNames[band]) + "   " + text(ids::frequencyOf(band)) + "   " + text(ids::gainOf(band));
    if (band == dsp::kMid)
        line << "   Q " << text(ids::midQ);

    const auto font = makeFont(11.0f);
    const float w = juce::GlyphArrangement::getStringWidth(font, line) + 22.0f;
    const auto node = juce::Point<float>(xForFrequency(settings.frequency(band)), yForDb(settings.gainDb(band)));
    const auto plot = getPlotArea();
    auto box = juce::Rectangle<float>(w, 22.0f).withCentre({node.x, node.y - 30.0f});
    if (box.getY() < plot.getY())
        box.setY(node.y + 20.0f);
    box.setX(juce::jlimit(plot.getX(), plot.getRight() - w, box.getX()));

    g.setColour(colours.panelRaised.withAlpha(0.95f));
    g.fillRoundedRectangle(box, 11.0f);
    g.setColour(bandColour(colours, band).withAlpha(0.8f));
    g.drawRoundedRectangle(box.reduced(0.5f), 11.0f, 1.0f);
    g.setColour(colours.text);
    g.setFont(font);
    g.drawText(line, box, juce::Justification::centred, false);
}

void ResponseView::mouseMove(const juce::MouseEvent& e)
{
    const int band = bandAt(e.position);
    if (band != hoveredBand)
    {
        hoveredBand = band;
        setMouseCursor(band >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void ResponseView::mouseExit(const juce::MouseEvent&)
{
    hoveredBand = -1;
    repaint();
}

void ResponseView::mouseDown(const juce::MouseEvent& e)
{
    draggedBand = bandAt(e.position);
    if (draggedBand < 0)
        return;
    for (const auto* id : {&ids::frequencyOf(draggedBand), &ids::gainOf(draggedBand)})
        processor.apvts.getParameter(id->getParamID())->beginChangeGesture();
}

void ResponseView::mouseDrag(const juce::MouseEvent& e)
{
    if (draggedBand < 0)
        return;
    setParameter(ids::frequencyOf(draggedBand), static_cast<float>(frequencyForX(e.position.x)));
    setParameter(ids::gainOf(draggedBand),
                 juce::jlimit(-kMaxGainDb, kMaxGainDb, static_cast<float>(dbForY(e.position.y))));
    repaint();
}

void ResponseView::mouseUp(const juce::MouseEvent& e)
{
    if (draggedBand >= 0)
        for (const auto* id : {&ids::frequencyOf(draggedBand), &ids::gainOf(draggedBand)})
            processor.apvts.getParameter(id->getParamID())->endChangeGesture();
    draggedBand = -1;
    hoveredBand = bandAt(e.position);
    repaint();
}

void ResponseView::mouseDoubleClick(const juce::MouseEvent& e)
{
    const int band = bandAt(e.position);
    if (band < 0)
        return;
    auto* gain = processor.apvts.getParameter(ids::gainOf(band).getParamID());
    gain->beginChangeGesture();
    gain->setValueNotifyingHost(gain->convertTo0to1(0.0f));
    gain->endChangeGesture();
    repaint();
}

void ResponseView::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (bandAt(e.position) != dsp::kMid)
    {
        Component::mouseWheelMove(e, wheel);
        return;
    }
    auto* q = processor.apvts.getParameter(ids::midQ.getParamID());
    const float current = q->convertFrom0to1(q->getValue());
    const float delta = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    q->beginChangeGesture();
    q->setValueNotifyingHost(q->convertTo0to1(current * std::pow(2.0f, delta * 2.0f)));
    q->endChangeGesture();
    repaint();
}
} // namespace owl::ui
