#include <animalaudio/core/BuildStamp.h>

#include <animalaudio_build_info.h>

namespace animalaudio::core
{
namespace
{
#if AA_BUILD_GIT_DIRTY
constexpr const char* kShaSuffix = "+dirty";
#define AA_MARKER_SHA AA_BUILD_GIT_SHA "+dirty"
#else
constexpr const char* kShaSuffix = "";
#define AA_MARKER_SHA AA_BUILD_GIT_SHA
#endif

constexpr char kMarker[] = "AA-BUILD|" AA_BUILD_VERSION "|" AA_MARKER_SHA "|" AA_BUILD_DATE "|";
} // namespace

BuildInfo getBuildInfo() noexcept
{
    return {AA_BUILD_VERSION, AA_BUILD_GIT_SHA, AA_BUILD_GIT_DIRTY != 0, AA_BUILD_DATE};
}

juce::String buildStampShort()
{
    const auto info = getBuildInfo();
    auto text = juce::String(info.version) + " | " + info.gitSha + kShaSuffix + " | " + info.date;
#if AA_BUILD_LOCAL
    text << " | local";
#endif
    return text;
}

juce::String buildStampLong(const juce::String& pluginName)
{
    const auto info = getBuildInfo();
    auto text = pluginName + " " + info.version + " (build " + info.gitSha + kShaSuffix + ", " + info.date + ")";

    // Also the one runtime use of the marker, so the linker can never discard it as unreferenced.
    if (juce::SystemStats::getEnvironmentVariable("AA_SHOW_BUILD_MARKER", {}).isNotEmpty())
        text << " [" << buildMarker() << "]";
    return text;
}

const char* buildMarker() noexcept
{
    return kMarker;
}
} // namespace animalaudio::core
