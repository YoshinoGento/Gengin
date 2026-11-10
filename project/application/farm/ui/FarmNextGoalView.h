#pragma once

#include "farm/system/FarmNextGoalSystem.h"
#include "farm/ui/FarmRuntimeUI.h"

#include <optional>

namespace farmui {
struct FarmNextGoalCopy {
    Label title;
    std::optional<Label> detail{};
};

[[nodiscard]] inline std::optional<FarmNextGoalCopy> DescribeFarmNextGoal(FarmNextGoalKind kind) noexcept {
    using Kind = FarmNextGoalKind;
    switch (kind) {
    case Kind::PestWarning: return FarmNextGoalCopy{Label::PestWarning, Label::PestWarningDetail};
    case Kind::PestActive: return FarmNextGoalCopy{Label::PestActive, Label::PestActiveDetail};
    case Kind::Submit: return FarmNextGoalCopy{Label::GoalSubmit, Label::GoalWhyContest};
    case Kind::ProtectAndReserve: return FarmNextGoalCopy{Label::GoalProtectReserve, Label::GoalWhyContest};
    case Kind::Reserve: return FarmNextGoalCopy{Label::GoalReserve, Label::GoalWhyContest};
    case Kind::Reserved: return FarmNextGoalCopy{Label::GoalReserved, Label::GoalWhyContest};
    case Kind::SelectSoil: return FarmNextGoalCopy{Label::GoalSoil};
    case Kind::Till: return FarmNextGoalCopy{Label::GoalTill};
    case Kind::Plant: return FarmNextGoalCopy{Label::GoalPlant};
    case Kind::ChooseCrop: return FarmNextGoalCopy{Label::GoalChooseCrop, Label::GoalWhySeeds};
    case Kind::BuySeed: return FarmNextGoalCopy{Label::GoalBuy, Label::GoalWhySeeds};
    case Kind::SellForSeeds: return FarmNextGoalCopy{Label::GoalSell, Label::GoalWhyFunds};
    case Kind::NeedFunds: return FarmNextGoalCopy{Label::GoalFunds, Label::GoalWhyFunds};
    case Kind::CheckWater: return FarmNextGoalCopy{Label::GoalCheckWater, Label::GoalWhyWater};
    case Kind::Water: return FarmNextGoalCopy{Label::GoalWater, Label::GoalWhyWater};
    case Kind::StopIrrigation: return FarmNextGoalCopy{Label::GoalStopIrrigation, Label::GoalWhyWater};
    case Kind::ReduceWater: return FarmNextGoalCopy{Label::GoalReduceWater, Label::GoalWhyWater};
    case Kind::Grow: return FarmNextGoalCopy{Label::GoalGrow};
    case Kind::ResumeGrowth: return FarmNextGoalCopy{Label::GoalResumeGrowth};
    case Kind::Harvest: return FarmNextGoalCopy{Label::GoalHarvest};
    case Kind::InventoryFull: return FarmNextGoalCopy{Label::GoalFull, Label::GoalWhyHarvest};
    default: return std::nullopt;
    }
}

inline void BuildFarmNextGoalView(View& view, const FarmNextGoal& goal, bool details) {
    const auto copy = DescribeFarmNextGoal(goal.kind);
    if (!copy || goal.pestTile >= 0) return;
    view.shortGoal = true;
    if (goal.contestDay > 0)
        view.Metric(copy->title, HUDLayout::shortGoal, 316, "D" + std::to_string(goal.contestDay));
    else view.Add(copy->title, HUDLayout::shortGoal);
    if (details && copy->detail) {
        view.shortGoalDetail = true;
        view.Add(*copy->detail, HUDLayout::shortGoalDetail);
    }
}

#ifdef _DEBUG
[[nodiscard]] inline const char* FarmGoalDebugText(Label label) noexcept {
    const auto index = static_cast<std::size_t>(label) - static_cast<std::size_t>(Label::PestWarning);
    return index < kGoalDebugText.size() ? reinterpret_cast<const char*>(kGoalDebugText[index].data()) : "";
}
#endif
}
