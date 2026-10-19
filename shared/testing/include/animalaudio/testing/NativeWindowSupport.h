#pragma once

namespace animalaudio::testing
{
/** Call before JUCE first touches the windowing system. Returns false unless AA_REAL_MOUSE_TESTS=1 is set (the
    tests move the real OS pointer) or if there is no usable display (none, or a locked macOS screen): skip the
    native-window tests then. */
bool prepareNativeWindows();
} // namespace animalaudio::testing
