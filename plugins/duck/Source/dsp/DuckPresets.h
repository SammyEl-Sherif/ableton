#pragma once

#include <animalaudio/dsp/CurvePreset.h>

namespace duck
{
/** 16 factory shapes (two rows of eight in the UI). Index 0 is the default. y=1 is unity gain. */
const animalaudio::dsp::CurvePresetList& getPresets();
} // namespace duck
