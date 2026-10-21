#pragma once

#include <array>
#include <string_view>

namespace turtletime::dsp
{
/** How a pattern moves the read pointer inside each musical window. */
enum class TimeMode
{
    CatchUp, // restart at "now" on each window and play at `rate` (half-time, reverse)
    Repeat   // capture one slice and replay it until the window ends
};

struct TimePattern
{
    std::string_view name;
    std::string_view blurb;
    float rate;         // samples the read pointer advances per output sample; negative plays backwards
    double windowBeats; // musical length between catch-up / recapture points (quarter-note beats)
    double sliceBeats;  // Repeat only: length of the looped slice
    TimeMode mode;
};

inline constexpr int kNumPatterns = 8;

inline constexpr std::array<TimePattern, kNumPatterns> kPatterns{{
    {"Shell", "Half speed across one bar. The classic skip.", 0.5f, 4.0, 0.0, TimeMode::CatchUp},
    {"Paddle", "Half speed, retriggered every half note.", 0.5f, 2.0, 0.0, TimeMode::CatchUp},
    {"Nap", "Half speed across two bars. Extra lazy.", 0.5f, 8.0, 0.0, TimeMode::CatchUp},
    {"Hatch", "Half speed, retriggered every beat.", 0.5f, 1.0, 0.0, TimeMode::CatchUp},
    {"Reverse", "One bar, played backwards.", -1.0f, 4.0, 0.0, TimeMode::CatchUp},
    {"Drift", "Half speed and backwards, across one bar.", -0.5f, 4.0, 0.0, TimeMode::CatchUp},
    {"Tap", "Repeats the first 1/8 note for a whole bar.", 1.0f, 4.0, 0.5, TimeMode::Repeat},
    {"Skitter", "Repeats the first 1/16 note for a whole bar.", 1.0f, 4.0, 0.25, TimeMode::Repeat},
}};

inline const TimePattern& patternAt(int index) noexcept
{
    if (index < 0)
        index = 0;
    if (index >= kNumPatterns)
        index = kNumPatterns - 1;
    return kPatterns[static_cast<std::size_t>(index)];
}
} // namespace turtletime::dsp
