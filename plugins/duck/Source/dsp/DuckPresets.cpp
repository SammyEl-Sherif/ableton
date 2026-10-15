#include "DuckPresets.h"

#include <animalaudio/dsp/StandardPresets.h>

namespace duck
{
const animalaudio::dsp::CurvePresetList& getPresets()
{
    static const animalaudio::dsp::CurvePresetList presets = animalaudio::dsp::standardCurvePresets({
        // row 1: full-depth ducks
        "Classic Duck",
        "Quick Bill",
        "Pond Swell",
        "Late Float",
        "Splash Gate",
        "Dive",
        "Reverse Dive",
        "Feather Tip",
        // row 2: gentler variants
        "Soft Quack",
        "Half Duck",
        "Lily Pad",
        "Hold & Hop",
        "Stutter",
        "Wave Rider",
        "Tail Dip",
        "Ripple",
    });
    return presets;
}
} // namespace duck
