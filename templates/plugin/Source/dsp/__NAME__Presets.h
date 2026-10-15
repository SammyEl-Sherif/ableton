#pragma once

#include <animalaudio/dsp/CurvePreset.h>

namespace __NAME_LOWER__
{
/** The 16 factory shapes. Index 0 is the default. y=1 is unity gain. */
const animalaudio::dsp::CurvePresetList& getPresets();
} // namespace __NAME_LOWER__
