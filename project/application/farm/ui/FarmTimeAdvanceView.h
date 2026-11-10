#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"

namespace farmui {
inline Label TimeAdvanceLabel(const FarmTimeAdvanceControlSystem::View& state) noexcept {
    if (state.status.active) return Label::TimeRunning;
    using R = FarmTimeAdvanceSystem::StopReason;
    switch (state.status.reason) {
    case R::HarvestReady: return Label::TimeHarvest;
    case R::WaterLow: return Label::TimeDry;
    case R::WaterExcess: return Label::TimeExcess;
    case R::PestWarning: return Label::TimePest;
    case R::WeatherChanged: return Label::TimeWeather;
    case R::ContestDay: return Label::TimeContest;
    case R::Finished: return Label::TimeFinished;
    case R::NoTarget: return Label::TimeNoTarget;
    case R::Blocked: case R::ContextChanged: return Label::TimeBlocked;
    case R::BudgetLimit: return Label::TimeBudget;
    case R::InvalidState: return Label::TimeInvalid;
    default: return Label::TimeHold;
    }
}
inline void BuildTimeAdvanceControl(View& view, const FarmTimeAdvanceControlSystem::View& state, bool enabled) {
    view.IconButton(state.status.active ? Label::TimeCancel : Label::TimeAdvance,
        state.status.active ? HUDIcon::Close : HUDIcon::Calendar, HUDLayout::UtilitySlot(4),
        {state.status.active ? Action::TimeCancel : Action::TimeAdvance}, enabled || state.status.active);
}
inline void BuildTimeAdvanceStatus(View& view, const FarmTimeAdvanceControlSystem::View& state) {
    view.Add(TimeAdvanceLabel(state), HUDLayout::shortGoal,
        {state.status.tileIndex >= 0 ? Action::TimeFocus : Action::None});
}
}
