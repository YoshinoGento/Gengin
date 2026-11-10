#pragma once

#include "farm/ui/FarmRuntimeUI.h"
#include <algorithm>
#include <array>

namespace farmui {
inline void BuildPestView(View& view, int tileIndex, bool selected, bool warning,
    int activeCount = 1, bool spreading = false) {
    auto label = selected ? Label::PestCure : warning ? Label::PestWarning : Label::PestActive;
    if (spreading) {
        constexpr std::array focus{Label::PestFocusOne, Label::PestFocusTwo, Label::PestFocusThree};
        constexpr std::array cure{Label::PestCureOne, Label::PestCureTwo, Label::PestCureThree};
        const auto countIndex = static_cast<std::size_t>(std::clamp(activeCount, 1, 3) - 1);
        label = selected ? cure[countIndex] : focus[countIndex];
    }
    view.Add(label, HUDLayout::shortGoal, {selected ? Action::PestCure : Action::PestFocus, tileIndex},
        true, true);
    if (selected) {
        view.shortGoalDetail = true;
        view.Add(spreading ? (warning ? Label::PestSpreadWarning : Label::PestSpreadActive) :
            warning ? Label::PestWarningDetail : Label::PestActiveDetail, HUDLayout::shortGoalDetail);
    }
}
}
