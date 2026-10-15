#include "CurveState.h"

namespace duck
{
using animalaudio::dsp::CurveTable;
using animalaudio::dsp::ShapeCurve;

juce::ValueTree curveToTree(const ShapeCurve& curve, int presetIndex)
{
    juce::ValueTree t(tree::curve);
    t.setProperty(tree::preset, presetIndex, nullptr);
    for (const auto& n : curve.getNodes())
    {
        juce::ValueTree child(tree::node);
        child.setProperty(tree::x, n.x, nullptr);
        child.setProperty(tree::y, n.y, nullptr);
        child.setProperty(tree::bend, n.bend, nullptr);
        t.appendChild(child, nullptr);
    }
    return t;
}

ShapeCurve curveFromTree(const juce::ValueTree& curveTree)
{
    std::vector<ShapeCurve::Node> nodes;
    for (const auto& child : curveTree)
    {
        if (!child.hasType(tree::node))
            continue;
        nodes.push_back({static_cast<float>(static_cast<double>(child.getProperty(tree::x, 0.0))),
                         static_cast<float>(static_cast<double>(child.getProperty(tree::y, 0.0))),
                         static_cast<float>(static_cast<double>(child.getProperty(tree::bend, 0.0)))});
    }
    if (nodes.size() < 2)
        return getPresets().front().makeCurve();
    return ShapeCurve(std::move(nodes));
}

CurveState::CurveState(juce::ValueTree& stateRoot)
    : root(stateRoot)
{
    root.addListener(this);
    ensureExists();
    republish();
}

CurveState::~CurveState()
{
    root.removeListener(this);
}

juce::ValueTree CurveState::getCurveTree() const
{
    return root.getChildWithName(tree::curve);
}

void CurveState::ensureExists()
{
    if (!getCurveTree().isValid())
    {
        root.appendChild(curveToTree(getPresets().front().makeCurve(), 0), nullptr);
    }
}

ShapeCurve CurveState::getCurve() const
{
    return curveFromTree(getCurveTree());
}

int CurveState::getPresetIndex() const
{
    return static_cast<int>(getCurveTree().getProperty(tree::preset, -1));
}

void CurveState::setCurve(const ShapeCurve& curve, int presetIndex)
{
    juce::ScopedValueSetter<bool> guard(batching, true);

    auto existing = getCurveTree();
    if (!existing.isValid())
    {
        root.appendChild(curveToTree(curve, presetIndex), nullptr);
    }
    else if (existing.getNumChildren() == static_cast<int>(curve.size()))
    {
        existing.setProperty(tree::preset, presetIndex, nullptr);
        int i = 0;
        for (const auto& n : curve.getNodes())
        {
            auto child = existing.getChild(i++);
            child.setProperty(tree::x, n.x, nullptr);
            child.setProperty(tree::y, n.y, nullptr);
            child.setProperty(tree::bend, n.bend, nullptr);
        }
    }
    else
    {
        root.removeChild(existing, nullptr);
        root.appendChild(curveToTree(curve, presetIndex), nullptr);
    }

    batching = false;
    republish();
}

void CurveState::loadPreset(int index)
{
    const auto& presets = getPresets();
    if (index < 0 || index >= static_cast<int>(presets.size()))
        return;
    setCurve(presets[static_cast<std::size_t>(index)].makeCurve(), index);
}

bool CurveState::fetchTable(CurveTable& destination, std::uint64_t& knownVersion) noexcept
{
    const juce::SpinLock::ScopedTryLockType tryLock(lock);
    if (!tryLock.isLocked() || version == knownVersion)
        return false;

    destination = published;
    knownVersion = version;
    return true;
}

bool CurveState::isCurveRelated(const juce::ValueTree& t) const
{
    return t.hasType(tree::curve) || t.hasType(tree::node) || t.getParent().hasType(tree::curve);
}

void CurveState::valueTreePropertyChanged(juce::ValueTree& t, const juce::Identifier&)
{
    if (!batching && isCurveRelated(t))
        republish();
}

void CurveState::valueTreeChildAdded(juce::ValueTree& parent, juce::ValueTree& child)
{
    if (!batching && (isCurveRelated(parent) || child.hasType(tree::curve)))
        republish();
}

void CurveState::valueTreeChildRemoved(juce::ValueTree& parent, juce::ValueTree& child, int)
{
    if (!batching && (isCurveRelated(parent) || child.hasType(tree::curve)))
        republish();
}

void CurveState::valueTreeRedirected(juce::ValueTree&)
{
    republish();
}

void CurveState::republish()
{
    CurveTable fresh;
    fresh.fill(getCurve());

    {
        const juce::SpinLock::ScopedLockType scoped(lock);
        published = fresh;
        ++version;
    }
    sendChangeMessage();
}
} // namespace duck
