#pragma once

namespace animalaudio::testing
{
/** Call before JUCE first touches the windowing system. Returns false if there is no usable display (none, or a
    locked macOS screen): skip the native-window tests then. */
bool prepareNativeWindows();
} // namespace animalaudio::testing
