#pragma once

#include "dsp/DuckPresets.h"

#include <animalaudio/dsp/ShapeCurve.h>

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <cstdint>

namespace duck
{
namespace tree
{
inline const juce::Identifier curve{"Curve"};
inline const juce::Identifier node{"Node"};
inline const juce::Identifier preset{"preset"};
inline const juce::Identifier x{"x"};
inline const juce::Identifier y{"y"};
inline const juce::Identifier bend{"bend"};
} // namespace tree

/** Serialises a curve as <Curve preset="0"><Node x= y= bend=/>...</Curve>. */
juce::ValueTree curveToTree(const animalaudio::dsp::ShapeCurve& curve, int presetIndex);

/** Tolerant parser: bad/missing data yields a sanitised curve (never throws). */
animalaudio::dsp::ShapeCurve curveFromTree(const juce::ValueTree& curveTree);

/**
 * Stores the editable curve inside a parent ValueTree (the APVTS state) so it is saved/restored
 * with the rest of the plugin state, and publishes a baked lookup table for the audio thread.
 *
 * Threading: setCurve()/getCurve() are message-thread. fetchTable() is audio-thread safe: it uses a
 * try-lock and never blocks; if the message thread happens to be publishing, the audio thread simply
 * keeps its previous table for this block.
 */
class CurveState : public juce::ChangeBroadcaster, private juce::ValueTree::Listener
{
public:
    /** `stateRoot` must outlive this object and is held by reference: APVTS::replaceState() reassigns
        the handle, and we need to follow it. */
    explicit CurveState(juce::ValueTree& stateRoot);
    ~CurveState() override;

    /** Creates the Curve child with the default preset if the state doesn't have one. */
    void ensureExists();

    animalaudio::dsp::ShapeCurve getCurve() const;
    int getPresetIndex() const;

    /** presetIndex = -1 marks the curve as custom. */
    void setCurve(const animalaudio::dsp::ShapeCurve& curve, int presetIndex = -1);
    void loadPreset(int index);

    /** Audio thread. Returns true (and fills `destination`) if a newer table was available. */
    bool fetchTable(animalaudio::dsp::CurveTable& destination, std::uint64_t& knownVersion) noexcept;

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeRedirected(juce::ValueTree&) override;

    bool isCurveRelated(const juce::ValueTree& tree) const;
    juce::ValueTree getCurveTree() const;
    void republish();

    juce::ValueTree& root;
    bool batching = false;

    juce::SpinLock lock;
    animalaudio::dsp::CurveTable published;
    std::uint64_t version = 1;
};
} // namespace duck
