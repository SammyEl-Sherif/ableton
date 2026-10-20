#pragma once

#include "IceReverb.h"

#include <array>
#include <string_view>

namespace icepenguin
{
struct ReverbPreset
{
    std::string_view name;
    dsp::ReverbSettings settings;
};

inline constexpr int kNumReverbPresets = 12;

const std::array<ReverbPreset, kNumReverbPresets>& reverbPresets();

dsp::ReverbSettings settingsForPreset(int presetIndex, float decayTrim, float toneTrim, float predelayTrimMs,
                                      float sizeTrim) noexcept;

} // namespace icepenguin
