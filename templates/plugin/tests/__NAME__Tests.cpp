#include "__NAME__Editor.h"
#include "__NAME__Processor.h"

#include <animalaudio/testing/ShaperPluginSuite.h>

AA_SHAPER_PLUGIN_TESTS(__NAME_LOWER__::__NAME__Processor, __NAME_LOWER__::__NAME__Editor, "__NAME_LOWER__")
AA_GAIN_SHAPER_PLUGIN_TESTS(__NAME_LOWER__::__NAME__Processor, "__NAME_LOWER__")
