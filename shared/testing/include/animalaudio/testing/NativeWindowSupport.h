#pragma once

namespace animalaudio::testing
{
/** Call before JUCE first touches the windowing system. Returns false if there is no display (skip UI tests). */
bool prepareNativeWindows();
} // namespace animalaudio::testing
