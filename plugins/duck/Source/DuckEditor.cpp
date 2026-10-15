#include "DuckEditor.h"

#include "ui/DuckTheme.h"

namespace duck
{
DuckEditor::DuckEditor(animalaudio::core::CurveShaperProcessor& owner)
    : ShaperEditor(owner, ui::makeTheme())
{
}
} // namespace duck
