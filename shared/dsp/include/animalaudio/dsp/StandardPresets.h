#pragma once

#include "CurvePreset.h"

#include <array>

namespace animalaudio::dsp
{
/**
 * The 16 stock curve shapes every shaper plugin ships (two rows of eight in the UI); only the names are
 * plugin-specific, so each plugin gives its own animal-flavoured names. y = 1 is unity gain.
 *
 *   row 1: full-depth shapes   row 2: gentler variants
 */
inline CurvePresetList standardCurvePresets(const std::array<const char*, kNumCurvePresets>& names)
{
    using N = ShapeCurve::Node;
    const std::array<std::vector<N>, kNumCurvePresets> shapes = {{
        {N{0.0f, 0.0f, -0.55f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.0f, -0.9f}, N{0.55f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.0f, 0.55f}, N{0.5f, 0.5f, -0.55f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.0f, 0.7f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 1.0f, 0.0f}, N{0.04f, 0.0f, 0.0f}, N{0.42f, 0.0f, -0.5f}, N{0.62f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 1.0f, 0.0f}, N{1.0f, 0.0f, 0.0f}},
        {N{0.0f, 1.0f, -0.55f}, N{0.5f, 0.5f, 0.55f}, N{1.0f, 0.0f, 0.0f}},
        {N{0.0f, 0.0f, 0.0f}, N{0.5f, 1.0f, 0.0f}, N{1.0f, 0.0f, 0.0f}},

        {N{0.0f, 0.2f, -0.6f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.5f, -0.5f}, N{0.6f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.3f, 0.45f}, N{0.5f, 0.65f, -0.45f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.0f, 0.0f}, N{0.2f, 0.0f, -0.7f}, N{0.6f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 0.0f, 0.0f}, N{0.48f, 0.0f, 0.0f}, N{0.5f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 1.0f, 0.5f}, N{0.5f, 0.15f, -0.5f}, N{1.0f, 1.0f, 0.0f}},
        {N{0.0f, 1.0f, 0.0f}, N{0.7f, 1.0f, 0.6f}, N{1.0f, 0.1f, 0.0f}},
        {N{0.0f, 0.25f, 0.0f}, N{0.25f, 1.0f, 0.0f}, N{0.5f, 0.25f, 0.0f}, N{0.75f, 1.0f, 0.0f}, N{1.0f, 0.25f, 0.0f}},
    }};

    CurvePresetList presets;
    presets.reserve(kNumCurvePresets);
    for (std::size_t i = 0; i < shapes.size(); ++i)
        presets.push_back({names[i], shapes[i]});
    return presets;
}
} // namespace animalaudio::dsp
