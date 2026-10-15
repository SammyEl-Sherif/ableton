#pragma once

#include <animalaudio/dsp/CurvePreset.h>

#include <juce_data_structures/juce_data_structures.h>
#include <juce_events/juce_events.h>

#include <atomic>
#include <cstdint>

namespace animalaudio::core
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
juce::ValueTree curveToTree(const dsp::ShapeCurve& curve, int presetIndex);

/** Tolerant parser: bad/missing data yields `fallback` (sanitised nodes otherwise). Never throws. */
dsp::ShapeCurve curveFromTree(const juce::ValueTree& curveTree, const dsp::ShapeCurve& fallback);

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
    /** `stateRoot` is held by reference (APVTS::replaceState() reassigns the handle and we must follow
        it); `presets` must outlive this object and be non-empty, entry 0 is the default curve. */
    CurveState(juce::ValueTree& stateRoot, const dsp::CurvePresetList& presets);
    ~CurveState() override;

    /** Creates the Curve child with the default preset if the state doesn't have one. */
    void ensureExists();

    dsp::ShapeCurve getCurve() const;
    int getPresetIndex() const;
    const dsp::CurvePresetList& getPresets() const noexcept { return presets; }

    /** presetIndex = -1 marks the curve as custom. */
    void setCurve(const dsp::ShapeCurve& curve, int presetIndex = -1);
    void loadPreset(int index);

    /** Audio thread. Returns true (and fills `destination`) if a newer table was available. */
    bool fetchTable(dsp::CurveTable& destination, std::uint64_t& knownVersion) noexcept;

private:
    void valueTreePropertyChanged(juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded(juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved(juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeRedirected(juce::ValueTree&) override;

    bool isCurveRelated(const juce::ValueTree& tree) const;
    juce::ValueTree getCurveTree() const;
    void republish();

    juce::ValueTree& root;
    const dsp::CurvePresetList& presets;
    bool batching = false;

    juce::SpinLock lock;
    dsp::CurveTable published;
    std::uint64_t version = 1;
};
} // namespace animalaudio::core
