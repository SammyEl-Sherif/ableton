#include "__NAME__Editor.h"

#include "ui/__NAME__Theme.h"

namespace __NAME_LOWER__
{
__NAME__Editor::__NAME__Editor(animalaudio::core::CurveShaperProcessor& owner)
    : ShaperEditor(owner, ui::makeTheme())
{
}
} // namespace __NAME_LOWER__
