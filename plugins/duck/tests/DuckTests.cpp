#include "DuckEditor.h"
#include "DuckProcessor.h"

#include <animalaudio/testing/RealMouseSuite.h>
#include <animalaudio/testing/ShaperPluginSuite.h>

AA_SHAPER_PLUGIN_TESTS(duck::DuckProcessor, duck::DuckEditor, "duck")
AA_GAIN_SHAPER_PLUGIN_TESTS(duck::DuckProcessor, "duck")
AA_REAL_MOUSE_PLUGIN_TESTS(duck::DuckProcessor, duck::DuckEditor, "duck")
