#include "EqDisplay.h"

#include "QuailTheme.h"

#include <cmath>

namespace quail::ui
{
using animalaudio::dsp::FilterShape;
using animalaudio::ui::makeFont;

namespace
{
constexpr float kSpectrumFloorDb = -96.0f;
constexpr float kSpectrumTiltDbPerOctave = 4.5f;

bool sameCurve(const dsp::BandSettings& a, const dsp::BandSettings& b)
{
    using juce::exactlyEqual;
    return a.used == b.used && a.enabled == b.enabled && a.shape == b.shape && a.slope == b.slope &&
           exactlyEqual(a.frequency, b.frequency) && exactlyEqual(a.gainDb, b.gainDb) && exactlyEqual(a.q, b.q) &&
           a.placement == b.placement && exactlyEqual(a.rangeDb, b.rangeDb) && a.dynamicsEnabled == b.dynamicsEnabled &&
           a.spectral == b.spectral;
}

juce::String noteName(double hz)
{
    const int note = juce::roundToInt(69.0 + 12.0 * std::log2(hz / 440.0));
    return note >= 0 && note < 128 ? juce::MidiMessage::getMidiNoteName(note, true, true, 4) : juce::String();
}

int slopeIndex(int slope)
{
    for (std::size_t i = 0; i < animalaudio::dsp::kFilterSlopes.size(); ++i)
        if (animalaudio::dsp::kFilterSlopes[i] == slope)
            return static_cast<int>(i);
    return 1;
}
} // namespace

EqDisplay::EqDisplay(BandActions& a, const animalaudio::ui::Theme& t)
    : actions(a)
    , processor(a.getProcessor())
    , theme(t)
    , colours(t.palette)
    , scratch(static_cast<std::size_t>(QuailProcessor::AnalyserFifo::capacity()))
{
    for (auto* spectrum : {&pre, &post, &side})
        spectrum->shown.fill(kSpectrumFloorDb);
    setWantsKeyboardFocus(true);
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

void EqDisplay::setAnalyzer(Analyzer options)
{
    analyzer = options;
    repaint();
}

void EqDisplay::setRangeDb(float db)
{
    rangeDb = db;
    repaint();
}

juce::Rectangle<float> EqDisplay::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(1.0f);
}

float EqDisplay::xForFrequency(double hz) const
{
    const auto plot = getPlotArea();
    const double t = std::log(juce::jlimit(kMinHz, kMaxHz, hz) / kMinHz) / std::log(kMaxHz / kMinHz);
    return plot.getX() + static_cast<float>(t) * plot.getWidth();
}

double EqDisplay::frequencyForX(float x) const
{
    const auto plot = getPlotArea();
    const double t = juce::jlimit(0.0, 1.0, static_cast<double>((x - plot.getX()) / plot.getWidth()));
    return kMinHz * std::pow(kMaxHz / kMinHz, t);
}

float EqDisplay::yForDb(double db) const
{
    const auto plot = getPlotArea();
    return plot.getCentreY() - static_cast<float>(db / rangeDb) * plot.getHeight() * 0.45f;
}

double EqDisplay::dbForY(float y) const
{
    const auto plot = getPlotArea();
    return static_cast<double>((plot.getCentreY() - y) / (plot.getHeight() * 0.45f)) * rangeDb;
}

double EqDisplay::nodeDbForQ(float q)
{
    return 6.0 * std::log2(static_cast<double>(q));
}

float EqDisplay::qForNodeDb(double db)
{
    return juce::jlimit(dsp::kMinQ, dsp::kMaxQ, static_cast<float>(std::exp2(db / 6.0)));
}

double EqDisplay::nodeDb(const dsp::BandSettings& band, float gainScale) const
{
    if (animalaudio::dsp::shapeHasGain(band.shape))
        return static_cast<double>(band.gainDb * gainScale);
    if (animalaudio::dsp::shapeHasQ(band.shape, band.slope))
        return nodeDbForQ(band.q);
    return 0.0;
}

juce::Point<float> EqDisplay::nodePosition(int band) const
{
    const auto s = processor.getSettings();
    const auto& b = s.bands[static_cast<std::size_t>(band)];
    const auto plot = getPlotArea();
    return {xForFrequency(b.frequency),
            juce::jlimit(plot.getY() + 8.0f, plot.getBottom() - 8.0f, yForDb(nodeDb(b, s.global.gainScale)))};
}

bool EqDisplay::hasRangeHandle(int band) const
{
    const auto s = processor.getSettings();
    return s.bands[static_cast<std::size_t>(band)].hasDynamics();
}

juce::Point<float> EqDisplay::rangeHandlePosition(int band) const
{
    const auto s = processor.getSettings();
    const auto& b = s.bands[static_cast<std::size_t>(band)];
    const auto plot = getPlotArea();
    return {xForFrequency(b.frequency), juce::jlimit(plot.getY() + 6.0f, plot.getBottom() - 6.0f,
                                                     yForDb(b.gainDb * s.global.gainScale + b.rangeDb))};
}

int EqDisplay::bandAt(juce::Point<float> position) const
{
    int best = -1;
    float bestDistance = kHitRadius;
    for (int band = 0; band < dsp::kNumBands; ++band)
    {
        if (!actions.isUsed(band))
            continue;
        // the selection wins ties, so stacked nodes stay reachable
        const float d = nodePosition(band).getDistanceFrom(position) - (isSelected(band) ? 2.0f : 0.0f);
        if (d <= bestDistance)
        {
            best = band;
            bestDistance = d;
        }
    }
    return best;
}

int EqDisplay::rangeHandleAt(juce::Point<float> position) const
{
    for (int band = 0; band < dsp::kNumBands; ++band)
        if (hasRangeHandle(band) && rangeHandlePosition(band).getDistanceFrom(position) <= 9.0f)
            return band;
    return -1;
}

bool EqDisplay::isSelected(int band) const noexcept
{
    return band >= 0 && band < dsp::kNumBands && selected[static_cast<std::size_t>(band)];
}

int EqDisplay::numSelected() const noexcept
{
    int n = 0;
    for (bool s : selected)
        n += s ? 1 : 0;
    return n;
}

void EqDisplay::notifySelection()
{
    if (onSelectionChanged)
        onSelectionChanged();
}

void EqDisplay::select(int band, bool add)
{
    if (!add)
        selected.fill(false);
    if (band >= 0)
    {
        selected[static_cast<std::size_t>(band)] = true;
        primary = band;
    }
    else if (!add)
        primary = -1;
    notifySelection();
    repaint();
}

void EqDisplay::selectAll()
{
    for (int band = 0; band < dsp::kNumBands; ++band)
        selected[static_cast<std::size_t>(band)] = actions.isUsed(band);
    if (!isSelected(primary))
    {
        primary = -1;
        for (int band = 0; band < dsp::kNumBands && primary < 0; ++band)
            if (isSelected(band))
                primary = band;
    }
    notifySelection();
    repaint();
}

void EqDisplay::feed(SpectrumFeed& spectrum, QuailProcessor::AnalyserFifo& fifo, bool shown)
{
    bool fresh = false;
    const int capacity = static_cast<int>(scratch.size());
    for (int n = fifo.pop(scratch.data(), capacity); n > 0; n = fifo.pop(scratch.data(), capacity))
    {
        spectrum.analyser.write(scratch.data(), n);
        fresh = true;
    }
    if (!shown)
    {
        spectrum.shown.fill(kSpectrumFloorDb);
        return;
    }
    if (fresh)
        spectrum.analyser.compute();

    const double sampleRate = processor.getDisplaySampleRate();
    const double ratio = kMaxHz / kMinHz;
    const auto at = [&](double index) { return kMinHz * std::pow(ratio, index / (kSpectrumPoints - 1)); };
    for (int i = 0; i < kSpectrumPoints; ++i)
    {
        const double hz = at(i);
        float target = kSpectrumFloorDb;
        if (fresh && hz < sampleRate * 0.5)
            target =
                juce::jmax(kSpectrumFloorDb, spectrum.analyser.peakDbBetween(at(i - 0.5), at(i + 0.5), sampleRate) +
                                                 kSpectrumTiltDbPerOctave * static_cast<float>(std::log2(hz / 1000.0)));
        auto& shownDb = spectrum.shown[static_cast<std::size_t>(i)];
        shownDb = target > shownDb ? target : shownDb + (target - shownDb) * 0.16f;
    }
}

void EqDisplay::refresh()
{
    const bool collisions = analyzer.sidechain;
    feed(pre, processor.getInputFifo(), analyzer.pre || collisions);
    feed(post, processor.getOutputFifo(), analyzer.post || collisions);
    feed(side, processor.getSidechainFifo(), analyzer.sidechain);

    bool changed = false;
    for (int band = 0; band < dsp::kNumBands; ++band)
        if (isSelected(band) && !actions.isUsed(band))
        {
            selected[static_cast<std::size_t>(band)] = false;
            changed = true;
        }
    if (primary >= 0 && !isSelected(primary))
    {
        primary = -1;
        for (int band = 0; band < dsp::kNumBands && primary < 0; ++band)
            if (isSelected(band))
                primary = band;
        changed = true;
    }
    if (changed)
        notifySelection();
    repaint();
}

void EqDisplay::updateCurves(const dsp::Settings& settings)
{
    const auto plot = getPlotArea();
    const auto n = static_cast<std::size_t>(juce::jmax(2, static_cast<int>(std::ceil(plot.getWidth() / 2.0f)) + 1));
    const double sampleRate = processor.getDisplaySampleRate();
    const float scale = settings.global.gainScale;
    using juce::exactlyEqual;
    if (xs.size() != n || !exactlyEqual(xs.front(), plot.getX()) || !exactlyEqual(xs.back(), plot.getRight()) ||
        !exactlyEqual(curveScale, scale) || !exactlyEqual(curveRate, sampleRate))
    {
        xs.resize(n);
        hzs.resize(n);
        for (std::size_t i = 0; i < n; ++i)
        {
            xs[i] = plot.getX() + plot.getWidth() * static_cast<float>(i) / static_cast<float>(n - 1);
            hzs[i] = static_cast<float>(frequencyForX(xs[i]));
        }
        curveValid.fill(false);
        curveScale = scale;
        curveRate = sampleRate;
    }

    const auto sample = [&](const animalaudio::dsp::FilterCascade& cascade, std::vector<float>& out)
    {
        out.resize(n);
        for (std::size_t i = 0; i < n; ++i)
            out[i] = static_cast<float>(cascade.magnitudeDbAt(hzs[i], sampleRate));
    };

    for (std::size_t b = 0; b < settings.bands.size(); ++b)
    {
        const auto& band = settings.bands[b];
        if (curveValid[b] && sameCurve(band, curveKeys[b]))
            continue;
        curveKeys[b] = band;
        curveValid[b] = true;
        staticDb[b].assign(n, 0.0f);
        rangeEdgeDb[b].clear();
        if (!band.isActive())
            continue;
        sample(animalaudio::dsp::designFilter(dsp::filterSpec(band, scale), sampleRate), staticDb[b]);
        if (band.hasDynamics())
            sample(animalaudio::dsp::designFilter(dsp::filterSpec(band, scale, band.rangeDb), sampleRate),
                   rangeEdgeDb[b]);
    }

    const auto& engine = processor.getEngine();
    groupUsed.fill(false);
    groupMoving.fill(false);
    for (std::size_t p = 0; p < groupStatic.size(); ++p)
    {
        groupStatic[p].assign(n, 0.0f);
        groupLive[p].assign(n, 0.0f);
    }
    for (std::size_t b = 0; b < settings.bands.size(); ++b)
    {
        const auto& band = settings.bands[b];
        liveMoving[b] = false;
        if (!band.isActive())
            continue;
        const auto p = static_cast<std::size_t>(band.placement);
        groupUsed[p] = true;
        const auto* live = &staticDb[b];
        if (band.hasDynamics() && !band.isSpectral())
        {
            const float dynamic = engine.getBandDynamicDb(static_cast<int>(b));
            if (std::abs(dynamic) > 0.05f)
            {
                sample(animalaudio::dsp::designFilter(dsp::filterSpec(band, scale, dynamic), sampleRate), liveDb[b]);
                live = &liveDb[b];
                liveMoving[b] = true;
                groupMoving[p] = true;
            }
        }
        for (std::size_t i = 0; i < n; ++i)
        {
            groupStatic[p][i] += staticDb[b][i];
            groupLive[p][i] += (*live)[i];
        }
    }

    const auto& spectral = engine.getSpectral();
    constexpr int points = dsp::SpectralDynamics::kDisplayPoints;
    const double f0 = dsp::SpectralDynamics::displayFrequency(0);
    const double span = std::log(dsp::SpectralDynamics::displayFrequency(points - 1) / f0);
    for (std::size_t p = 0; p < groupLive.size(); ++p)
    {
        const auto placement = static_cast<dsp::Placement>(p);
        if (!groupUsed[p] || !spectral.isDisplayActive(placement))
            continue;
        groupMoving[p] = true;
        for (std::size_t i = 0; i < n; ++i)
        {
            const double t = std::log(hzs[i] / f0) / span * (points - 1);
            const int lo = juce::jlimit(0, points - 2, static_cast<int>(std::floor(t)));
            const auto frac = static_cast<float>(juce::jlimit(0.0, 1.0, t - lo));
            groupLive[p][i] +=
                spectral.getDisplayDb(placement, lo) * (1.0f - frac) + spectral.getDisplayDb(placement, lo + 1) * frac;
        }
    }
}

juce::Path EqDisplay::curvePath(const std::vector<float>& db) const
{
    const auto plot = getPlotArea();
    juce::Path path;
    for (std::size_t i = 0; i < db.size() && i < xs.size(); ++i)
    {
        const float y = juce::jlimit(plot.getY() - 2.0f, plot.getBottom() + 2.0f, yForDb(db[i]));
        if (i == 0)
            path.startNewSubPath(xs[i], y);
        else
            path.lineTo(xs[i], y);
    }
    return path;
}

float EqDisplay::spectrumY(float db, juce::Rectangle<float> plot) const
{
    const float level = juce::jlimit(0.0f, 1.0f, (db - kSpectrumFloorDb) / -kSpectrumFloorDb);
    return plot.getBottom() - level * plot.getHeight() * 0.9f;
}

juce::Path EqDisplay::spectrumPath(const SpectrumFeed& spectrum, juce::Rectangle<float> plot) const
{
    juce::Path line;
    const auto at = [&](int j)
    { return spectrum.shown[static_cast<std::size_t>(juce::jlimit(0, kSpectrumPoints - 1, j))]; };
    for (int i = 0; i < kSpectrumPoints; ++i)
    {
        const float db = 0.25f * at(i - 1) + 0.5f * at(i) + 0.25f * at(i + 1);
        const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / (kSpectrumPoints - 1);
        if (i == 0)
            line.startNewSubPath(x, spectrumY(db, plot));
        else
            line.lineTo(x, spectrumY(db, plot));
    }
    return line;
}

void EqDisplay::resized()
{
    curveValid.fill(false);
}

void EqDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = getPlotArea();
    const auto settings = processor.getSettings();
    const double seconds = timeOverride >= 0.0 ? timeOverride : juce::Time::getMillisecondCounterHiRes() * 0.001;
    updateCurves(settings);

    g.setGradientFill(
        juce::ColourGradient(colours.panel, 0.0f, bounds.getY(), colours.background, 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 10.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, 10.0f);
        g.reduceClipRegion(clip);
        theme.motif->drawAmbient(g, bounds.withTop(bounds.getBottom() - bounds.getHeight() * 0.3f), seconds, colours);
        drawGrid(g, plot);
        drawSpectra(g, plot);
        drawBands(g, settings);
        drawTotals(g, settings);
    }
    g.setColour(colours.outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 10.0f, 1.0f);

    drawFrame(g, plot, actions.numUsed() == 0);
    drawNodes(g, settings);

    if (drag == Drag::lasso && !lasso.isEmpty())
    {
        g.setColour(colours.accent.withAlpha(0.08f));
        g.fillRect(lasso);
        g.setColour(colours.accent.withAlpha(0.6f));
        g.drawRect(lasso, 1.0f);
    }

    const int readout = dragBand >= 0 ? dragBand : hoverBand >= 0 ? hoverBand : hoverRange;
    if (readout >= 0 && actions.isUsed(readout))
        drawReadout(g, readout, settings);
    else if (hoverInside && drag == Drag::none)
        drawCursor(g, plot);
}

void EqDisplay::drawGrid(juce::Graphics& g, juce::Rectangle<float> plot)
{
    for (double decade = 10.0; decade < kMaxHz; decade *= 10.0)
        for (int m = 1; m <= 9; ++m)
        {
            const double hz = decade * m;
            if (hz <= kMinHz || hz >= kMaxHz)
                continue;
            g.setColour(colours.outline.withAlpha(m == 1 ? 0.6f : 0.25f));
            g.drawVerticalLine(juce::roundToInt(xForFrequency(hz)), plot.getY(), plot.getBottom());
        }

    g.setFont(makeFont(10.0f, false));
    for (double hz : {20.0, 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0, 20000.0})
    {
        const float x = xForFrequency(hz);
        const juce::String label =
            hz >= 1000.0 ? juce::String(juce::roundToInt(hz / 1000.0)) + "k" : juce::String(juce::roundToInt(hz));
        g.setColour(colours.textDim.withAlpha(0.8f));
        g.drawText(label, juce::Rectangle<float>(x - 20.0f, plot.getBottom() - 17.0f, 40.0f, 14.0f),
                   juce::Justification::centred, false);
    }

    const float step = rangeDb <= 3.0f ? 1.0f : rangeDb <= 6.0f ? 2.0f : rangeDb <= 12.0f ? 3.0f : 6.0f;
    for (float db = -rangeDb; db <= rangeDb + 0.01f; db += step)
    {
        const float y = yForDb(db);
        const bool zero = std::abs(db) < 0.01f;
        g.setColour(colours.outline.withAlpha(zero ? 0.9f : 0.3f));
        g.drawHorizontalLine(juce::roundToInt(y), plot.getX(), plot.getRight());
        const juce::String label = zero ? "0" : (db > 0.0f ? "+" : "") + juce::String(juce::roundToInt(db));
        g.setColour(colours.accent.withAlpha(zero ? 0.75f : 0.5f));
        g.drawText(label, juce::Rectangle<float>(plot.getRight() - 36.0f, y - 8.0f, 30.0f, 16.0f),
                   juce::Justification::centredRight, false);
    }
}

void EqDisplay::drawSpectra(juce::Graphics& g, juce::Rectangle<float> plot)
{
    const auto closed = [&](juce::Path line)
    {
        line.lineTo(plot.getRight(), plot.getBottom());
        line.lineTo(plot.getX(), plot.getBottom());
        line.closeSubPath();
        return line;
    };
    const auto fillAndStroke = [&](const SpectrumFeed& spectrum, juce::Colour colour, float fillAlpha, float lineAlpha)
    {
        const auto line = spectrumPath(spectrum, plot);
        g.setGradientFill(juce::ColourGradient(colour.withAlpha(fillAlpha), 0.0f, plot.getY() + plot.getHeight() * 0.2f,
                                               colour.withAlpha(0.0f), 0.0f, plot.getBottom(), false));
        g.fillPath(closed(line));
        g.setColour(colour.withAlpha(lineAlpha));
        g.strokePath(line, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved));
    };

    if (analyzer.pre)
        fillAndStroke(pre, colours.textDim, 0.12f, 0.35f);
    if (analyzer.post)
        fillAndStroke(post, colours.ambient.brighter(0.5f), 0.22f, 0.55f);

    if (!analyzer.sidechain || !processor.isSidechainActive())
        return;
    const auto& main = analyzer.post ? post : pre;
    const float columnWidth = plot.getWidth() / (kSpectrumPoints - 1);
    for (int i = 0; i < kSpectrumPoints; ++i)
    {
        const float level =
            juce::jmin(main.shown[static_cast<std::size_t>(i)], side.shown[static_cast<std::size_t>(i)]);
        const float intensity = juce::jlimit(0.0f, 1.0f, (level + 54.0f) / 30.0f);
        if (intensity < 0.02f)
            continue;
        const float x = plot.getX() + columnWidth * (static_cast<float>(i) - 0.5f);
        const float top = spectrumY(level, plot);
        g.setGradientFill(juce::ColourGradient(collisionColour().withAlpha(0.0f), 0.0f, top,
                                               collisionColour().withAlpha(0.45f * intensity), 0.0f, plot.getBottom(),
                                               false));
        g.fillRect(juce::Rectangle<float>(x, top, columnWidth + 0.5f, plot.getBottom() - top));
    }
    const auto line = spectrumPath(side, plot);
    g.setColour(colours.accentAlt.withAlpha(0.06f));
    g.fillPath(closed(line));
    g.setColour(colours.accentAlt.withAlpha(0.85f));
    g.strokePath(line, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved));
}

void EqDisplay::drawBands(juce::Graphics& g, const dsp::Settings& settings)
{
    const float zeroY = yForDb(0.0);
    const auto plot = getPlotArea();
    for (std::size_t b = 0; b < settings.bands.size(); ++b)
    {
        const auto& band = settings.bands[b];
        if (!band.isActive() || staticDb[b].empty())
            continue;
        const bool isSel = selected[b];
        const auto colour = bandColour(static_cast<int>(b));
        const auto line = curvePath(staticDb[b]);

        juce::Path fill(line);
        fill.lineTo(plot.getRight(), zeroY);
        fill.lineTo(plot.getX(), zeroY);
        fill.closeSubPath();
        g.setColour(colour.withAlpha(isSel ? 0.2f : 0.07f));
        g.fillPath(fill);
        g.setColour(colour.withAlpha(isSel ? 0.95f : 0.4f));
        g.strokePath(line, juce::PathStrokeType(isSel ? 1.6f : 1.0f, juce::PathStrokeType::curved));

        if (!rangeEdgeDb[b].empty())
        {
            const auto edge = curvePath(rangeEdgeDb[b]);
            juce::Path region(line);
            for (std::size_t i = rangeEdgeDb[b].size(); i-- > 0;)
                region.lineTo(xs[i],
                              juce::jlimit(plot.getY() - 2.0f, plot.getBottom() + 2.0f, yForDb(rangeEdgeDb[b][i])));
            region.closeSubPath();
            const auto dyn = dynamicColour(band.rangeDb > 0.0f);
            g.setColour(dyn.withAlpha(isSel ? 0.2f : 0.1f));
            g.fillPath(region);
            juce::Path dashed;
            const float dashes[] = {4.0f, 4.0f};
            juce::PathStrokeType(1.0f).createDashedStroke(dashed, edge, dashes, 2);
            g.setColour(dyn.withAlpha(isSel ? 0.85f : 0.45f));
            g.fillPath(dashed);
        }

        if (liveMoving[b])
        {
            g.setColour(colour.brighter(0.3f).withAlpha(0.95f));
            g.strokePath(curvePath(liveDb[b]), juce::PathStrokeType(1.5f, juce::PathStrokeType::curved));
        }
    }
}

void EqDisplay::drawTotals(juce::Graphics& g, const dsp::Settings& settings)
{
    juce::ignoreUnused(settings);
    const bool stereo = processor.isStereo();
    for (std::size_t p = 0; p < groupUsed.size(); ++p)
    {
        if (!groupUsed[p])
            continue;
        const auto placement = static_cast<dsp::Placement>(p);
        const float alpha = stereo || placement == dsp::Placement::Stereo ? 1.0f : 0.35f;
        const auto colour = placementColour(colours, placement).withMultipliedAlpha(alpha);
        const auto staticLine = curvePath(groupStatic[p]);
        if (groupMoving[p])
        {
            g.setColour(colour.withMultipliedAlpha(0.4f));
            g.strokePath(staticLine, juce::PathStrokeType(1.2f, juce::PathStrokeType::curved));
        }
        const auto line = groupMoving[p] ? curvePath(groupLive[p]) : staticLine;
        g.setColour(colour.withMultipliedAlpha(0.14f));
        g.strokePath(line, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colour);
        g.strokePath(line, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void EqDisplay::drawNodes(juce::Graphics& g, const dsp::Settings& settings)
{
    const float scale = settings.global.gainScale;
    const auto& engine = processor.getEngine();
    const auto drawNode = [&](int b)
    {
        const auto& band = settings.bands[static_cast<std::size_t>(b)];
        const auto colour = bandColour(b);
        const auto c = nodePosition(b);
        const bool isSel = isSelected(b);
        const bool hot = hoverBand == b || dragBand == b;

        if (band.hasDynamics())
        {
            const auto h = rangeHandlePosition(b);
            const auto dyn = dynamicColour(band.rangeDb > 0.0f);
            juce::Path stem;
            stem.startNewSubPath(c);
            stem.lineTo(h);
            juce::Path dotted;
            const float dashes[] = {2.0f, 3.0f};
            juce::PathStrokeType(1.0f).createDashedStroke(dotted, stem, dashes, 2);
            g.setColour(dyn.withAlpha(0.7f));
            g.fillPath(dotted);

            const float d = hoverRange == b || (drag == Drag::range && dragBand == b) ? 6.5f : 5.0f;
            juce::Path diamond;
            diamond.addQuadrilateral(h.x, h.y - d, h.x + d, h.y, h.x, h.y + d, h.x - d, h.y);
            g.setColour(colours.background);
            g.strokePath(diamond, juce::PathStrokeType(2.0f));
            g.setColour(dyn);
            g.fillPath(diamond);

            const float live = band.isSpectral() ? engine.getSpectral().getBandGainDb(b) : engine.getBandDynamicDb(b);
            if (std::abs(live) > 0.05f)
            {
                const float y = yForDb(band.gainDb * scale + live);
                g.setColour(colours.text);
                g.fillRoundedRectangle(juce::Rectangle<float>(c.x - 6.0f, y - 1.0f, 12.0f, 2.0f), 1.0f);
            }
        }

        const float r = kNodeRadius + (isSel || hot ? 1.5f : 0.0f);
        const auto disc = juce::Rectangle<float>(r * 2.0f, r * 2.0f).withCentre(c);
        if (isSel)
        {
            g.setColour(colour.withAlpha(0.2f));
            g.fillEllipse(disc.expanded(6.0f));
        }
        g.setColour(colours.background.withAlpha(0.6f));
        g.fillEllipse(disc.translated(0.0f, 1.5f));
        if (band.enabled)
        {
            g.setColour(colour);
            g.fillEllipse(disc);
            g.setColour(colours.background.withAlpha(0.7f));
            g.drawEllipse(disc, 1.2f);
        }
        else
        {
            g.setColour(colours.panel);
            g.fillEllipse(disc);
            g.setColour(colour.withAlpha(0.7f));
            g.drawEllipse(disc.reduced(0.7f), 1.4f);
        }
        if (isSel)
        {
            g.setColour(colours.text);
            g.drawEllipse(disc.expanded(2.5f), 1.5f);
        }
        g.setColour(band.enabled ? colours.onAccent : colour);
        g.setFont(makeFont(r * 1.15f));
        g.drawText(juce::String(b + 1), disc.expanded(2.0f, 0.0f), juce::Justification::centred, false);

        juce::StringArray tags;
        if (band.placement != dsp::Placement::Stereo)
            tags.add(placementNames()[static_cast<int>(band.placement)].substring(0, 1).toUpperCase());
        if (band.hasDynamics() && band.externalSidechain)
            tags.add("SC");
        if (band.isSpectral())
            tags.add("SPEC");
        if (!tags.isEmpty())
        {
            g.setFont(makeFont(9.0f));
            g.setColour(band.placement != dsp::Placement::Stereo ? placementColour(colours, band.placement)
                                                                 : colours.textDim);
            g.drawText(tags.joinIntoString(" "), juce::Rectangle<float>(c.x - 30.0f, c.y + r + 3.0f, 60.0f, 11.0f),
                       juce::Justification::centred, false);
        }
    };

    for (int b = 0; b < dsp::kNumBands; ++b)
        if (settings.bands[static_cast<std::size_t>(b)].used && !isSelected(b))
            drawNode(b);
    for (int b = 0; b < dsp::kNumBands; ++b)
        if (settings.bands[static_cast<std::size_t>(b)].used && isSelected(b))
            drawNode(b);
}

void EqDisplay::drawReadout(juce::Graphics& g, int band, const dsp::Settings& settings)
{
    const auto& b = settings.bands[static_cast<std::size_t>(band)];
    juce::StringArray lines;
    lines.add("BAND " + juce::String(band + 1) + "  " + shapeNames()[static_cast<int>(b.shape)].toUpperCase() +
              (b.enabled ? "" : "  (OFF)"));
    juce::String values = actions.text(band, BandParam::frequency);
    if (animalaudio::dsp::shapeHasGain(b.shape))
        values << "   " << actions.text(band, BandParam::gain);
    if (animalaudio::dsp::shapeHasSlope(b.shape))
        values << "   " << actions.text(band, BandParam::slope);
    if (animalaudio::dsp::shapeHasQ(b.shape, b.slope))
        values << "   Q " << actions.text(band, BandParam::q);
    lines.add(values);
    if (b.hasDynamics())
    {
        juce::String dyn = "RANGE " + actions.text(band, BandParam::range);
        dyn << (b.externalSidechain ? "   SIDE CHAIN" : "   SELF") << (b.isSpectral() ? "   SPECTRAL" : "");
        lines.add(dyn);
    }

    const auto font = makeFont(11.5f);
    float width = 0.0f;
    for (const auto& line : lines)
        width = juce::jmax(width, juce::GlyphArrangement::getStringWidth(font, line));
    width += 22.0f;
    const float height = 10.0f + 15.0f * static_cast<float>(lines.size());
    const auto node = nodePosition(band);
    const auto plot = getPlotArea();
    auto box = juce::Rectangle<float>(width, height).withCentre({node.x, node.y - height * 0.5f - 22.0f});
    if (box.getY() < plot.getY() + 4.0f)
        box.setY(node.y + 22.0f);
    box.setX(juce::jlimit(plot.getX() + 4.0f, plot.getRight() - width - 4.0f, box.getX()));

    g.setColour(colours.panelRaised.withAlpha(0.96f));
    g.fillRoundedRectangle(box, 7.0f);
    g.setColour(bandColour(band).withAlpha(0.85f));
    g.drawRoundedRectangle(box.reduced(0.5f), 7.0f, 1.0f);
    g.setFont(font);
    auto row = box.reduced(11.0f, 5.0f);
    for (int i = 0; i < lines.size(); ++i)
    {
        g.setColour(i == 0 ? bandColour(band) : colours.text);
        g.drawText(lines[i], row.removeFromTop(15.0f), juce::Justification::centredLeft, false);
    }
}

void EqDisplay::drawCursor(juce::Graphics& g, juce::Rectangle<float> plot)
{
    if (!plot.contains(hoverPoint))
        return;
    juce::Path line;
    line.startNewSubPath(hoverPoint.x, plot.getY());
    line.lineTo(hoverPoint.x, plot.getBottom());
    juce::Path dashed;
    const float dashes[] = {3.0f, 4.0f};
    juce::PathStrokeType(1.0f).createDashedStroke(dashed, line, dashes, 2);
    g.setColour(colours.text.withAlpha(0.18f));
    g.fillPath(dashed);

    const double hz = frequencyForX(hoverPoint.x);
    const juce::String text = formatHz(static_cast<float>(hz)) + "   " + noteName(hz) + "   " +
                              formatDb(static_cast<float>(dbForY(hoverPoint.y)));
    const auto font = makeFont(11.0f);
    const float width = juce::GlyphArrangement::getStringWidth(font, text) + 16.0f;
    auto box = juce::Rectangle<float>(width, 20.0f).withPosition(hoverPoint.x + 12.0f, hoverPoint.y - 30.0f);
    if (box.getRight() > plot.getRight() - 4.0f)
        box.setX(hoverPoint.x - 12.0f - width);
    if (box.getY() < plot.getY() + 4.0f)
        box.setY(hoverPoint.y + 12.0f);
    g.setColour(colours.panelRaised.withAlpha(0.85f));
    g.fillRoundedRectangle(box, 5.0f);
    g.setColour(colours.textDim);
    g.setFont(font);
    g.drawText(text, box, juce::Justification::centred, false);
}

void EqDisplay::drawFrame(juce::Graphics& g, juce::Rectangle<float> plot, bool empty)
{
    const auto corners = plot.reduced(8.0f);
    const float arm = 14.0f;
    juce::Path brackets;
    for (const auto& corner :
         {corners.getTopLeft(), corners.getTopRight(), corners.getBottomLeft(), corners.getBottomRight()})
    {
        const float sx = corner.x < plot.getCentreX() ? 1.0f : -1.0f;
        const float sy = corner.y < plot.getCentreY() ? 1.0f : -1.0f;
        brackets.startNewSubPath(corner.x + sx * arm, corner.y);
        brackets.lineTo(corner);
        brackets.lineTo(corner.x, corner.y + sy * arm);
    }
    g.setColour(colours.accent.withAlpha(0.35f));
    g.strokePath(brackets, juce::PathStrokeType(1.2f));

    if (!empty)
        return;
    auto hint = plot.withSizeKeepingCentre(plot.getWidth(), 44.0f).translated(0.0f, -plot.getHeight() * 0.18f);
    g.setColour(colours.text.withAlpha(0.75f));
    g.setFont(makeFont(15.0f));
    g.drawText("DOUBLE-CLICK TO ADD A BAND", hint.removeFromTop(22.0f), juce::Justification::centred, false);
    g.setColour(colours.textDim.withAlpha(0.8f));
    g.setFont(makeFont(11.0f, false));
    g.drawText("DRAG TO SHAPE   |   SCROLL FOR Q   |   RIGHT-CLICK FOR SHAPES, STEREO AND DYNAMICS", hint,
               juce::Justification::centred, false);
}

void EqDisplay::mouseMove(const juce::MouseEvent& e)
{
    hoverPoint = e.position;
    hoverInside = getPlotArea().contains(e.position);
    const int band = bandAt(e.position);
    const int range = band < 0 ? rangeHandleAt(e.position) : -1;
    if (band != hoverBand || range != hoverRange)
    {
        hoverBand = band;
        hoverRange = range;
        setMouseCursor(band >= 0 || range >= 0 ? juce::MouseCursor::DraggingHandCursor
                                               : juce::MouseCursor::CrosshairCursor);
    }
    repaint();
}

void EqDisplay::mouseExit(const juce::MouseEvent&)
{
    hoverBand = -1;
    hoverRange = -1;
    hoverInside = false;
    repaint();
}

void EqDisplay::beginNodeDrag(int band, const juce::MouseEvent& e, bool forQ)
{
    drag = forQ ? Drag::q : Drag::node;
    dragBand = band;
    dragStart = e.position;
    const auto s = processor.getSettings();
    const auto open = [this](int b, BandParam p)
    {
        actions.begin(b, p);
        openGestures.emplace_back(b, p);
    };
    for (int b = 0; b < dsp::kNumBands; ++b)
    {
        if (!isSelected(b))
            continue;
        const auto& settings = s.bands[static_cast<std::size_t>(b)];
        dragFrom[static_cast<std::size_t>(b)] = {settings.frequency, settings.gainDb, settings.q};
        const bool hasQ = animalaudio::dsp::shapeHasQ(settings.shape, settings.slope);
        if (forQ)
        {
            if (hasQ)
                open(b, BandParam::q);
            continue;
        }
        open(b, BandParam::frequency);
        if (animalaudio::dsp::shapeHasGain(settings.shape))
            open(b, BandParam::gain);
        else if (hasQ)
            open(b, BandParam::q);
    }
}

void EqDisplay::endDrag()
{
    for (const auto& [band, param] : openGestures)
        actions.end(band, param);
    openGestures.clear();
    drag = Drag::none;
    dragBand = -1;
    lasso = {};
}

void EqDisplay::mouseDown(const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    endDrag();
    dragStart = e.position;
    const int band = bandAt(e.position);
    const bool adding = e.mods.isShiftDown() || e.mods.isCommandDown();

    if (e.mods.isPopupMenu())
    {
        if (band >= 0)
        {
            if (!isSelected(band))
                select(band);
            showBandMenu(band);
        }
        else
            showBackgroundMenu(e.position);
        return;
    }

    if (band >= 0)
    {
        if (adding)
            select(band, true);
        else if (!isSelected(band))
            select(band);
        else if (primary != band)
        {
            primary = band;
            notifySelection();
        }
        beginNodeDrag(band, e, e.mods.isAltDown());
        return;
    }

    const int range = rangeHandleAt(e.position);
    if (range >= 0)
    {
        if (!isSelected(range))
            select(range);
        drag = Drag::range;
        dragBand = range;
        actions.begin(range, BandParam::range);
        openGestures.emplace_back(range, BandParam::range);
        return;
    }

    drag = Drag::lasso;
    if (adding)
        lassoBase = selected;
    else
    {
        lassoBase.fill(false);
        select(-1);
    }
    lasso = {};
}

void EqDisplay::mouseDrag(const juce::MouseEvent& e)
{
    const auto s = processor.getSettings();
    if (drag == Drag::node || drag == Drag::q)
    {
        const auto plot = getPlotArea();
        const double fine = e.mods.isShiftDown() ? 0.2 : 1.0;
        const double octaves =
            static_cast<double>((e.position.x - dragStart.x) / plot.getWidth()) * std::log2(kMaxHz / kMinHz) * fine;
        const double db = (dbForY(e.position.y) - dbForY(dragStart.y)) * fine;
        const double scale = juce::jmax(0.1f, s.global.gainScale);
        for (const auto& [b, param] : openGestures)
        {
            const auto& from = dragFrom[static_cast<std::size_t>(b)];
            switch (param)
            {
            case BandParam::frequency:
                actions.move(b, param,
                             juce::jlimit(dsp::kMinHz, dsp::kMaxHz, static_cast<float>(from.hz * std::exp2(octaves))));
                break;
            case BandParam::gain:
                actions.move(
                    b, param,
                    juce::jlimit(-dsp::kMaxGainDb, dsp::kMaxGainDb, static_cast<float>(from.gainDb + db / scale)));
                break;
            case BandParam::q:
                if (drag == Drag::q)
                    actions.move(b, param,
                                 juce::jlimit(dsp::kMinQ, dsp::kMaxQ,
                                              static_cast<float>(
                                                  from.q * std::exp2(-(e.position.y - dragStart.y) / 60.0f * fine))));
                else
                    actions.move(b, param, qForNodeDb(nodeDbForQ(from.q) + db));
                break;
            case BandParam::used:
            case BandParam::enabled:
            case BandParam::shape:
            case BandParam::slope:
            case BandParam::placement:
            case BandParam::range:
            case BandParam::dynamics:
            case BandParam::threshold:
            case BandParam::autoThreshold:
            case BandParam::attack:
            case BandParam::release:
            case BandParam::sidechain:
            case BandParam::spectral:
            case BandParam::density:
            case BandParam::spectralTilt:
                break;
            }
        }
    }
    else if (drag == Drag::range && dragBand >= 0)
    {
        const auto& band = s.bands[static_cast<std::size_t>(dragBand)];
        actions.move(dragBand, BandParam::range,
                     juce::jlimit(-dsp::kMaxRangeDb, dsp::kMaxRangeDb,
                                  static_cast<float>(dbForY(e.position.y)) - band.gainDb * s.global.gainScale));
    }
    else if (drag == Drag::lasso)
    {
        lasso = juce::Rectangle<float>(dragStart, e.position);
        selected = lassoBase;
        for (int b = 0; b < dsp::kNumBands; ++b)
            if (actions.isUsed(b) && lasso.contains(nodePosition(b)))
                selected[static_cast<std::size_t>(b)] = true;
        if (!isSelected(primary))
        {
            primary = -1;
            for (int b = 0; b < dsp::kNumBands && primary < 0; ++b)
                if (isSelected(b))
                    primary = b;
        }
        notifySelection();
    }
    hoverPoint = e.position;
    repaint();
}

void EqDisplay::mouseUp(const juce::MouseEvent& e)
{
    endDrag();
    hoverBand = bandAt(e.position);
    hoverRange = hoverBand < 0 ? rangeHandleAt(e.position) : -1;
    notifySelection();
    repaint();
}

void EqDisplay::mouseDoubleClick(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    endDrag();
    const int band = bandAt(e.position);
    if (band >= 0)
    {
        actions.remove(band);
        selected[static_cast<std::size_t>(band)] = false;
        if (primary == band)
            primary = -1;
        hoverBand = -1;
        notifySelection();
    }
    else if (getPlotArea().contains(e.position))
    {
        const auto hz = static_cast<float>(frequencyForX(e.position.x));
        const auto shape = BandActions::shapeForNewBand(hz);
        const float scale = juce::jmax(0.1f, processor.getSettings().global.gainScale);
        const int created = actions.create(hz, static_cast<float>(dbForY(e.position.y)) / scale, shape);
        if (created >= 0)
            select(created);
    }
    repaint();
}

void EqDisplay::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int band = bandAt(e.position);
    const float delta = wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    if (band < 0 || delta == 0.0f)
    {
        Component::mouseWheelMove(e, wheel);
        return;
    }
    const auto s = processor.getSettings();
    const auto& b = s.bands[static_cast<std::size_t>(band)];
    if (e.mods.isAltDown())
    {
        if (dsp::supportsDynamics(b.shape))
            actions.set(band, BandParam::range,
                        juce::jlimit(-dsp::kMaxRangeDb, dsp::kMaxRangeDb, b.rangeDb + delta * 10.0f));
    }
    else if (animalaudio::dsp::shapeHasQ(b.shape, b.slope))
        actions.set(band, BandParam::q, juce::jlimit(dsp::kMinQ, dsp::kMaxQ, b.q * std::exp2(delta * 2.0f)));
    else if (animalaudio::dsp::shapeHasSlope(b.shape))
    {
        const int index = juce::jlimit(0, static_cast<int>(animalaudio::dsp::kFilterSlopes.size()) - 1,
                                       slopeIndex(b.slope) + (delta > 0.0f ? 1 : -1));
        actions.set(band, BandParam::slope, static_cast<float>(index));
    }
    repaint();
}

bool EqDisplay::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        removeSelected();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        select(-1);
        return true;
    }
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        selectAll();
        return true;
    }
    return false;
}

void EqDisplay::removeSelected()
{
    for (int b = 0; b < dsp::kNumBands; ++b)
        if (isSelected(b))
            actions.remove(b);
    select(-1);
}

void EqDisplay::showBandMenu(int band)
{
    const auto s = processor.getSettings();
    const auto& b = s.bands[static_cast<std::size_t>(band)];
    juce::PopupMenu shapes, slopes, placements;
    for (int i = 0; i < animalaudio::dsp::kNumFilterShapes; ++i)
        shapes.addItem(100 + i, shapeNames()[i], true, static_cast<int>(b.shape) == i);
    for (int i = 0; i < static_cast<int>(animalaudio::dsp::kFilterSlopes.size()); ++i)
        slopes.addItem(200 + i, slopeNames()[i], true,
                       animalaudio::dsp::kFilterSlopes[static_cast<std::size_t>(i)] == b.slope);
    for (int i = 0; i < dsp::kNumPlacements; ++i)
        placements.addItem(300 + i, placementNames()[i], processor.isStereo() || i == 0,
                           static_cast<int>(b.placement) == i);

    const bool dynamics = dsp::supportsDynamics(b.shape);
    juce::PopupMenu menu;
    menu.addSectionHeader("BAND " + juce::String(band + 1));
    menu.addSubMenu("Shape", shapes);
    menu.addSubMenu("Slope", slopes, animalaudio::dsp::shapeHasSlope(b.shape));
    menu.addSubMenu("Stereo placement", placements);
    menu.addSeparator();
    menu.addItem(1, "Enabled", true, b.enabled);
    menu.addItem(2, "Dynamic EQ", dynamics, b.dynamicsEnabled && b.rangeDb != 0.0f);
    menu.addItem(3, "External side chain", dynamics, b.externalSidechain);
    menu.addItem(4, "Spectral", dsp::supportsSpectral(b.shape), b.spectral);
    menu.addSeparator();
    menu.addItem(5, "Reset gain", animalaudio::dsp::shapeHasGain(b.shape) && b.gainDb != 0.0f);
    menu.addItem(6, numSelected() > 1 ? "Remove selected bands" : "Remove band");
    menu.setLookAndFeel(&getLookAndFeel());

    const auto node = nodePosition(band).toInt();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
                           juce::Rectangle<int>(1, 1).withPosition(localPointToGlobal(node))),
                       [safe = juce::Component::SafePointer<EqDisplay>(this), band, b](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           auto& act = safe->actions;
                           const auto ensureDynamics = [&]
                           {
                               if (!b.dynamicsEnabled)
                                   act.set(band, BandParam::dynamics, 1.0f);
                               if (b.rangeDb == 0.0f)
                                   act.set(band, BandParam::range, -6.0f);
                           };
                           if (result >= 300)
                               act.set(band, BandParam::placement, static_cast<float>(result - 300));
                           else if (result >= 200)
                               act.set(band, BandParam::slope, static_cast<float>(result - 200));
                           else if (result >= 100)
                               act.set(band, BandParam::shape, static_cast<float>(result - 100));
                           else if (result == 1)
                               act.set(band, BandParam::enabled, b.enabled ? 0.0f : 1.0f);
                           else if (result == 2)
                           {
                               if (b.dynamicsEnabled && b.rangeDb != 0.0f)
                                   act.set(band, BandParam::dynamics, 0.0f);
                               else
                                   ensureDynamics();
                           }
                           else if (result == 3)
                           {
                               act.set(band, BandParam::sidechain, b.externalSidechain ? 0.0f : 1.0f);
                               if (!b.externalSidechain)
                                   ensureDynamics();
                           }
                           else if (result == 4)
                           {
                               act.set(band, BandParam::spectral, b.spectral ? 0.0f : 1.0f);
                               if (!b.spectral)
                                   ensureDynamics();
                           }
                           else if (result == 5)
                               act.set(band, BandParam::gain, 0.0f);
                           else if (result == 6)
                               safe->removeSelected();
                           safe->repaint();
                       });
}

void EqDisplay::showBackgroundMenu(juce::Point<float> position)
{
    const auto hz = static_cast<float>(frequencyForX(position.x));
    const auto db = static_cast<float>(dbForY(position.y));
    const bool free = actions.firstFree() >= 0;
    juce::PopupMenu menu;
    menu.addSectionHeader("ADD AT " + formatHz(hz));
    for (int i = 0; i < animalaudio::dsp::kNumFilterShapes; ++i)
        menu.addItem(10 + i, shapeNames()[i], free);
    menu.addSeparator();
    menu.addItem(2, "Select all bands", actions.numUsed() > 0);
    menu.addItem(3, "Remove all bands", actions.numUsed() > 0);
    menu.setLookAndFeel(&getLookAndFeel());

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(
                           juce::Rectangle<int>(1, 1).withPosition(localPointToGlobal(position.toInt()))),
                       [safe = juce::Component::SafePointer<EqDisplay>(this), hz, db](int result)
                       {
                           if (safe == nullptr || result == 0)
                               return;
                           if (result >= 10)
                           {
                               const auto shape = static_cast<FilterShape>(result - 10);
                               const float scale = juce::jmax(0.1f, safe->processor.getSettings().global.gainScale);
                               const int band = safe->actions.create(hz, db / scale, shape);
                               if (band >= 0)
                                   safe->select(band);
                           }
                           else if (result == 2)
                               safe->selectAll();
                           else if (result == 3)
                           {
                               safe->actions.removeAll();
                               safe->select(-1);
                           }
                           safe->repaint();
                       });
}
} // namespace quail::ui
