#include "__NAME__Presets.h"

#include <animalaudio/dsp/StandardPresets.h>

namespace __NAME_LOWER__
{
const animalaudio::dsp::CurvePresetList& getPresets()
{
    static const animalaudio::dsp::CurvePresetList presets = animalaudio::dsp::standardCurvePresets({
        // row 1: full-depth shapes
        "Preset 1",
        "Preset 2",
        "Preset 3",
        "Preset 4",
        "Preset 5",
        "Preset 6",
        "Preset 7",
        "Preset 8",
        // row 2: gentler variants
        "Preset 9",
        "Preset 10",
        "Preset 11",
        "Preset 12",
        "Preset 13",
        "Preset 14",
        "Preset 15",
        "Preset 16",
    });
    return presets;
}
} // namespace __NAME_LOWER__
