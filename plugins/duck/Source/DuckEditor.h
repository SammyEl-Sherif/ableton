#pragma once

#include <animalaudio/ui/ShaperEditor.h>

namespace duck
{
class DuckProcessor;

class DuckEditor : public animalaudio::ui::ShaperEditor
{
public:
    explicit DuckEditor(animalaudio::core::CurveShaperProcessor& owner);
};
} // namespace duck
