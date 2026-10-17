#pragma once

#include <juce_core/juce_core.h>

namespace animalaudio::core
{
/** Which build is this? Version, git short SHA and date are baked in when the binary is compiled
    (cmake/GenerateBuildInfo.cmake), so a user can tell whether a host really loaded the build they think it did. */
struct BuildInfo
{
    const char* version;
    const char* gitSha; // "unknown" without git; see `dirty`
    bool dirty;         // built from a working tree with uncommitted changes
    const char* date;   // UTC, YYYY-MM-DD
};

BuildInfo getBuildInfo() noexcept;

/** "0.1.0 | a1b2c3d | 2026-10-17" (SHA gets a "+dirty" suffix for uncommitted changes). */
juce::String buildStampShort();

/** "Duck 0.1.0 (build a1b2c3d, 2026-10-17)": for tooltips and about text. */
juce::String buildStampLong(const juce::String& pluginName);

/** The machine-readable `AA-BUILD|version|sha|date|` string embedded in the binary. scripts/install_plugins.py
    finds it in an installed bundle to report which build is on disk without loading it in a host. */
const char* buildMarker() noexcept;
} // namespace animalaudio::core
