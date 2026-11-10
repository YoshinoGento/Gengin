#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmDevelopmentSystem.h"

namespace farmui {
inline void BuildDevelopmentView(View& view, const FarmDevelopmentSystem::View& state,
    std::uint64_t generation) {
    view.Add(Label::Development, {176,128,920,44});
    view.Metric(Label::DevelopmentRank, {176,202,450,40}, 260, std::to_string(state.rank));
    view.Metric(Label::DevelopmentArea, {646,202,450,40}, 260, std::to_string(state.tiles));
    view.Metric(Label::DevelopmentSales, {176,278,920,40}, 540, std::to_string(state.sales)+"G");
    if (state.maximumRank) view.Add(Label::DevelopmentMax, {176,348,920,40});
    else view.Metric(Label::DevelopmentNext, {176,348,920,40}, 540, std::to_string(state.remainingSales)+"G");
    view.Add(Label::DevelopmentRule, {176,420,920,38});
    view.Add(Label::DevelopmentExpand, {176,492,450,48},
        {Action::ExpandFarm, state.height, 0, generation}, state.canExpand);
    if (!state.standardFarm) view.Add(Label::DevelopmentCustom, {176,552,920,38});
    else if (state.maximumArea) view.Add(Label::DevelopmentFull, {176,552,920,38});
    else if (!state.canExpand) view.Add(Label::DevelopmentLocked, {176,552,920,38});
}
}
