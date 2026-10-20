#include <animalaudio/ui/CurveEditor.h>

#include <animalaudio/ui/Drawing.h>

namespace animalaudio::ui
{
using dsp::ShapeCurve;

CurveEditor::CurveEditor(core::CurveShaperProcessor& p, const Theme& themeToUse)
    : processor(p)
    , theme(themeToUse)
    , colours(themeToUse.palette)
    , curve(p.getCurveState().getCurve())
{
    setOpaque(false);
    processor.getCurveState().addChangeListener(this);
    startTimerHz(30);
}

CurveEditor::~CurveEditor()
{
    stopTimer();
    processor.getCurveState().removeChangeListener(this);
}

juce::Rectangle<float> CurveEditor::getPlotArea() const
{
    return getLocalBounds().toFloat().reduced(18.0f, 20.0f);
}

juce::Point<float> CurveEditor::toScreen(float x, float y) const
{
    const auto a = getPlotArea();
    return {a.getX() + x * a.getWidth(), a.getBottom() - y * a.getHeight()};
}

juce::Point<float> CurveEditor::fromScreen(juce::Point<float> p) const
{
    const auto a = getPlotArea();
    return {(p.x - a.getX()) / a.getWidth(), (a.getBottom() - p.y) / a.getHeight()};
}

juce::Point<float> CurveEditor::bendHandlePosition(std::size_t segment) const
{
    const auto& nodes = curve.getNodes();
    const auto& a = nodes[segment];
    const auto& b = nodes[segment + 1];
    const float x = 0.5f * (a.x + b.x);
    const float y = a.y + (b.y - a.y) * ShapeCurve::midFractionFromBend(a.bend);
    return toScreen(x, y);
}

CurveEditor::Hit CurveEditor::hitTestAt(juce::Point<float> p) const
{
    const auto& nodes = curve.getNodes();
    for (std::size_t i = 0; i < nodes.size(); ++i)
        if (toScreen(nodes[i].x, nodes[i].y).getDistanceFrom(p) <= kNodeHitRadius)
            return {Target::Node, i};

    for (std::size_t i = 0; i + 1 < nodes.size(); ++i)
        if (bendHandlePosition(i).getDistanceFrom(p) <= kBendHitRadius)
            return {Target::Bend, i};

    return {};
}

void CurveEditor::commit(int presetIndex)
{
    processor.getCurveState().setCurve(curve, presetIndex);
}

void CurveEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    if (dragging.target == Target::None)
        curve = processor.getCurveState().getCurve();
    repaint();
}

void CurveEditor::timerCallback()
{
    if (!processor.isHostTransportPlaying())
        return;

    ambientTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    repaint();
}

void CurveEditor::mouseMove(const juce::MouseEvent& e)
{
    const auto hit = hitTestAt(e.position);
    if (hit.target != hovered.target || hit.index != hovered.index)
    {
        hovered = hit;
        setMouseCursor(hit.target == Target::Node   ? juce::MouseCursor::DraggingHandCursor
                       : hit.target == Target::Bend ? juce::MouseCursor::UpDownResizeCursor
                                                    : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void CurveEditor::mouseExit(const juce::MouseEvent&)
{
    hovered = {};
    repaint();
}

void CurveEditor::mouseDown(const juce::MouseEvent& e)
{
    const auto hit = hitTestAt(e.position);

    if (e.mods.isPopupMenu() && hit.target == Target::Node)
    {
        if (curve.removeNode(hit.index))
        {
            hovered = {};
            commit();
        }
        return;
    }

    dragging = hit;
}

void CurveEditor::mouseDrag(const juce::MouseEvent& e)
{
    if (dragging.target == Target::None)
        return;

    const auto p = fromScreen(e.position);

    if (dragging.target == Target::Node)
    {
        curve.moveNode(dragging.index, p.x, p.y);
    }
    else
    {
        const auto& nodes = curve.getNodes();
        const auto& a = nodes[dragging.index];
        const auto& b = nodes[dragging.index + 1];
        const float span = b.y - a.y;
        if (std::abs(span) > 0.02f)
        {
            const float fraction = (juce::jlimit(0.0f, 1.0f, p.y) - a.y) / span;
            curve.setBend(dragging.index, ShapeCurve::bendFromMidFraction(fraction));
        }
    }

    commit();
    repaint();
}

void CurveEditor::mouseUp(const juce::MouseEvent&)
{
    dragging = {};
    curve = processor.getCurveState().getCurve();
    repaint();
}

void CurveEditor::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto hit = hitTestAt(e.position);

    if (hit.target == Target::Node)
    {
        if (curve.removeNode(hit.index))
            commit();
    }
    else if (hit.target == Target::Bend)
    {
        curve.setBend(hit.index, 0.0f);
        commit();
    }
    else
    {
        const auto p = fromScreen(e.position);
        if (curve.insertNode(p.x, p.y) != static_cast<std::size_t>(-1))
            commit();
    }

    hovered = hitTestAt(e.position);
    repaint();
}

void CurveEditor::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto plot = getPlotArea();
    const double seconds = phaseOverride >= 0.0f ? 0.0 : ambientTimeSeconds;

    // panel + water motif
    g.setColour(colours.panel.darker(0.25f));
    g.fillRoundedRectangle(bounds, 10.0f);
    {
        juce::Graphics::ScopedSaveState state(g);
        juce::Path clip;
        clip.addRoundedRectangle(bounds, 10.0f);
        g.reduceClipRegion(clip);

        g.setGradientFill(juce::ColourGradient(colours.ambient.withAlpha(0.0f), 0.0f,
                                               bounds.getY() + bounds.getHeight() * 0.35f,
                                               colours.ambient.withAlpha(0.10f), 0.0f, bounds.getBottom(), false));
        g.fillRect(bounds);
        theme.motif->drawAmbient(g, bounds.withTop(bounds.getY() + bounds.getHeight() * 0.45f), seconds, colours);

        // beat grid: quarters of the cycle
        for (int i = 0; i <= 4; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * static_cast<float>(i) / 4.0f;
            g.setColour(colours.outline.withAlpha(i == 0 || i == 4 ? 0.0f : 0.55f));
            g.drawVerticalLine(juce::roundToInt(x), plot.getY(), plot.getBottom());
        }
        g.setColour(colours.outline.withAlpha(0.55f));
        g.drawHorizontalLine(juce::roundToInt(plot.getY()), plot.getX(), plot.getRight());
        g.drawHorizontalLine(juce::roundToInt(plot.getBottom()), plot.getX(), plot.getRight());

        // post-effect signal level, mirrored around the centre line
        const float centreY = plot.getCentreY();
        const int numBins = processor.getNumScopeBins();
        const float binW = numBins > 0 ? plot.getWidth() / static_cast<float>(numBins) : 0.0f;
        g.setColour(colours.textDim.withAlpha(0.24f));
        for (int i = 0; i < numBins; ++i)
        {
            const float level = juce::jlimit(0.0f, 1.0f, processor.getScopeBin(i));
            const float h = level * plot.getHeight() * 0.5f;
            g.fillRect(plot.getX() + binW * static_cast<float>(i), centreY - h, juce::jmax(1.0f, binW - 0.5f),
                       h * 2.0f);
        }

        // effective gain (curve scaled by mix) as a translucent fill
        const float mix = processor.getMix();
        juce::Path fill;
        fill.startNewSubPath(plot.getX(), plot.getBottom());
        const int steps = 160;
        for (int i = 0; i <= steps; ++i)
        {
            const float x = static_cast<float>(i) / static_cast<float>(steps);
            const float y = 1.0f - mix * (1.0f - curve.evaluate(x));
            const auto s = toScreen(x, y);
            fill.lineTo(s.x, s.y);
        }
        fill.lineTo(plot.getRight(), plot.getBottom());
        fill.closeSubPath();
        g.setGradientFill(juce::ColourGradient(colours.accent.withAlpha(0.32f), 0.0f, plot.getY(),
                                               colours.accentAlt.withAlpha(0.04f), 0.0f, plot.getBottom(), false));
        g.fillPath(fill);

        // the editable curve
        const auto line = makeCurvePath(curve, plot, 200);
        g.setColour(colours.accentAlt.withAlpha(0.35f));
        g.strokePath(line, juce::PathStrokeType(7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(colours.accent);
        g.strokePath(line, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour(colours.textDim.withAlpha(0.55f));
    g.setFont(makeFont(9.5f, false));
    g.drawText("DOUBLE-CLICK TO ADD / REMOVE A NODE   |   DRAG THE DIAMONDS TO BEND",
               getLocalBounds().toFloat().withTrimmedLeft(24.0f).withTrimmedBottom(3.0f),
               juce::Justification::bottomLeft, false);

    // bend handles
    const auto& nodes = curve.getNodes();
    for (std::size_t i = 0; i + 1 < nodes.size(); ++i)
    {
        const auto c = bendHandlePosition(i);
        const bool active = (hovered.target == Target::Bend && hovered.index == i) ||
                            (dragging.target == Target::Bend && dragging.index == i);
        const float r = active ? 6.5f : 5.0f;
        juce::Path diamond;
        diamond.addQuadrilateral(c.x, c.y - r, c.x + r, c.y, c.x, c.y + r, c.x - r, c.y);
        g.setColour(colours.background.withAlpha(0.85f));
        g.fillPath(diamond);
        g.setColour(active ? colours.accent : colours.detail);
        g.strokePath(diamond, juce::PathStrokeType(1.6f));
    }

    // nodes
    for (std::size_t i = 0; i < nodes.size(); ++i)
    {
        const auto c = toScreen(nodes[i].x, nodes[i].y);
        const bool active = (hovered.target == Target::Node && hovered.index == i) ||
                            (dragging.target == Target::Node && dragging.index == i);
        const float r = active ? 8.0f : 6.5f;
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.fillEllipse(c.x - r, c.y - r + 1.5f, r * 2.0f, r * 2.0f);
        g.setColour(colours.accent);
        g.fillEllipse(c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.setColour(colours.background);
        g.fillEllipse(c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);
    }

    // playhead + mascot riding the curve
    const float phase = phaseOverride >= 0.0f ? phaseOverride : processor.getDisplayPhase();
    const float px = plot.getX() + phase * plot.getWidth();
    g.setColour(juce::Colours::white.withAlpha(0.55f));
    g.drawVerticalLine(juce::roundToInt(px), plot.getY(), plot.getBottom());

    const float mascotSize = 26.0f;
    const auto onCurve = toScreen(phase, curve.evaluate(phase));
    theme.motif->drawMascot(
        g, juce::Rectangle<float>(mascotSize, mascotSize).withCentre({onCurve.x, onCurve.y - mascotSize * 0.5f + 5.0f}),
        colours);
}
} // namespace animalaudio::ui
