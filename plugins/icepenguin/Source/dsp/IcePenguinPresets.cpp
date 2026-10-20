#include "IcePenguinPresets.h"

#include <algorithm>
#include <cmath>

namespace icepenguin
{
namespace
{
constexpr std::array<ReverbPreset, kNumReverbPresets> kPresets{{
    {"Glacier Hall", {0.88f, 0.28f, 1.0f, 22.0f, 8.5f}},
    {"Crevasse", {0.72f, 0.48f, 0.92f, 12.0f, 4.2f}},
    {"Aurora Shimmer", {0.92f, 0.18f, 1.0f, 38.0f, 11.0f}},
    {"Blizzard", {0.62f, 0.58f, 0.98f, 6.0f, 2.8f}},
    {"Igloo Chamber", {0.52f, 0.42f, 0.78f, 14.0f, 1.9f}},
    {"Sea Ice", {0.84f, 0.32f, 0.95f, 26.0f, 6.8f}},
    {"Penguin Colony", {0.68f, 0.38f, 0.82f, 18.0f, 3.4f}},
    {"Frozen Lake", {0.94f, 0.22f, 1.0f, 32.0f, 9.5f}},
    {"Ice Cave", {0.80f, 0.52f, 0.72f, 8.0f, 5.2f}},
    {"Crystal Shelf", {0.78f, 0.14f, 1.0f, 45.0f, 12.5f}},
    {"Polar Night", {0.86f, 0.62f, 0.88f, 16.0f, 4.8f}},
    {"Snow Drift", {0.58f, 0.34f, 0.92f, 24.0f, 3.2f}},
}};
} // namespace

const std::array<ReverbPreset, kNumReverbPresets>& reverbPresets()
{
    return kPresets;
}

dsp::ReverbSettings settingsForPreset(int presetIndex, float decayTrim, float toneTrim, float predelayTrimMs,
                                      float sizeTrim) noexcept
{
    const int idx = std::clamp(presetIndex, 0, kNumReverbPresets - 1);
    dsp::ReverbSettings s = kPresets[static_cast<std::size_t>(idx)].settings;

    const float decayMul = 0.4f + decayTrim * 1.75f;
    s.decaySeconds = std::clamp(s.decaySeconds * decayMul, 0.2f, 28.0f);

    s.damping = std::clamp(s.damping + (toneTrim - 0.5f) * 0.45f, 0.05f, 0.92f);
    s.predelayMs = std::clamp(s.predelayMs + predelayTrimMs, 0.0f, 200.0f);
    s.roomSize = std::clamp(s.roomSize * (0.75f + sizeTrim * 0.55f), 0.05f, 1.0f);

    return s;
}

} // namespace icepenguin
