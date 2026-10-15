#include "DuckPresets.h"

namespace duck
{
const std::vector<CurvePreset>& getPresets()
{
    using N = animalaudio::dsp::ShapeCurve::Node;

    static const std::vector<CurvePreset> presets = {
        // Row 1: full-depth ducks
        {"Classic Duck", {N{0.0f, 0.0f, -0.55f}, N{1.0f, 1.0f, 0.0f}}},
        {"Quick Bill", {N{0.0f, 0.0f, -0.9f}, N{0.55f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}}},
        {"Pond Swell", {N{0.0f, 0.0f, 0.55f}, N{0.5f, 0.5f, -0.55f}, N{1.0f, 1.0f, 0.0f}}},
        {"Late Float", {N{0.0f, 0.0f, 0.7f}, N{1.0f, 1.0f, 0.0f}}},
        {"Splash Gate",
         {N{0.0f, 1.0f, 0.0f}, N{0.04f, 0.0f, 0.0f}, N{0.42f, 0.0f, -0.5f}, N{0.62f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}}},
        {"Dive", {N{0.0f, 1.0f, 0.0f}, N{1.0f, 0.0f, 0.0f}}},
        {"Reverse Dive", {N{0.0f, 1.0f, -0.55f}, N{0.5f, 0.5f, 0.55f}, N{1.0f, 0.0f, 0.0f}}},
        {"Feather Tip", {N{0.0f, 0.0f, 0.0f}, N{0.5f, 1.0f, 0.0f}, N{1.0f, 0.0f, 0.0f}}},

        // Row 2: gentler variants
        {"Soft Quack", {N{0.0f, 0.2f, -0.6f}, N{1.0f, 1.0f, 0.0f}}},
        {"Half Duck", {N{0.0f, 0.5f, -0.5f}, N{0.6f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}}},
        {"Lily Pad", {N{0.0f, 0.3f, 0.45f}, N{0.5f, 0.65f, -0.45f}, N{1.0f, 1.0f, 0.0f}}},
        {"Hold & Hop", {N{0.0f, 0.0f, 0.0f}, N{0.2f, 0.0f, -0.7f}, N{0.6f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}}},
        {"Stutter", {N{0.0f, 0.0f, 0.0f}, N{0.48f, 0.0f, 0.0f}, N{0.5f, 1.0f, 0.0f}, N{1.0f, 1.0f, 0.0f}}},
        {"Wave Rider", {N{0.0f, 1.0f, 0.5f}, N{0.5f, 0.15f, -0.5f}, N{1.0f, 1.0f, 0.0f}}},
        {"Tail Dip", {N{0.0f, 1.0f, 0.0f}, N{0.7f, 1.0f, 0.6f}, N{1.0f, 0.1f, 0.0f}}},
        {"Ripple",
         {N{0.0f, 0.25f, 0.0f}, N{0.25f, 1.0f, 0.0f}, N{0.5f, 0.25f, 0.0f}, N{0.75f, 1.0f, 0.0f},
          N{1.0f, 0.25f, 0.0f}}},
    };
    return presets;
}
} // namespace duck
