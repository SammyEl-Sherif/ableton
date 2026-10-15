#pragma once

#include "ShapeCurve.h"

#include <vector>

namespace animalaudio::dsp
{
struct CurvePreset
{
    const char* name;
    std::vector<ShapeCurve::Node> nodes;

    ShapeCurve makeCurve() const { return ShapeCurve(nodes); }
};

/** A plugin's factory shapes. Index 0 is the default. The UI shows 16 (2 rows of 8). */
using CurvePresetList = std::vector<CurvePreset>;

constexpr int kNumCurvePresets = 16;
} // namespace animalaudio::dsp
