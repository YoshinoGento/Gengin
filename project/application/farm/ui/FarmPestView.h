#pragma once

#include "farm/ui/FarmRuntimeUI.h"

namespace farmui {
inline void BuildPestView(View& view, int tileIndex, bool selected, bool warning) {
    const auto label = selected ? Label::PestCure : warning ? Label::PestWarning : Label::PestActive;
    view.Add(label, HUDLayout::shortGoal, {selected ? Action::PestCure : Action::PestFocus, tileIndex},
        true, true);
    if (selected) {
        view.shortGoalDetail = true;
        view.Add(warning ? Label::PestWarningDetail : Label::PestActiveDetail, HUDLayout::shortGoalDetail);
    }
}
}
