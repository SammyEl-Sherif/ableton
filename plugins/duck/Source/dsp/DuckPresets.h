#pragma once

#include <animalaudio/dsp/ShapeCurve.h>

#include <vector>

namespace duck
{
struct CurvePreset
{
    const char* name;
    std::vector<animalaudio::dsp::ShapeCurve::Node> nodes;

    animalaudio::dsp::ShapeCurve makeCurve() const { return animalaudio::dsp::ShapeCurve(nodes); }
};

constexpr int kNumPresets = 16;

/** 16 factory shapes (two rows of eight in the UI). Index 0 is the default. y=1 is unity gain. */
const std::vector<CurvePreset>& getPresets();
} // namespace duck
