#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmFungicideSystem.h"
#include "farm/ui/FarmHUD.h"

namespace farmui {
inline void BuildFungicideView(View& view, FarmFungicideSystem::Status status, int stock,
    float seconds, int tileIndex, bool unlocked) {
    using S = FarmFungicideSystem::Status;
    if ((!stock && status != S::Protected) || view.count >= view.items.size()) return;
    constexpr Rect row{24, 404, 314, 46};
    const auto label = status == S::Protected ? Label::FungicideActive : status == S::Sick ?
        Label::FungicideSick : status == S::Ready ? Label::FungicideApply : Label::FungicidePick;
    view.Add(label,row,{Action::ApplyFungicide,tileIndex},unlocked && status == S::Ready);
    auto& item = view.items[view.count-1];
    if (status == S::Ready || status == S::Protected) {
        item.valueOffset = 226;
        item.value = status == S::Protected ? std::to_string(static_cast<int>(std::ceil(seconds)))+"s" :
            (stock > 99 ? "99+" : std::to_string(stock));
    }
}
inline void BuildFieldActionView(View& view, const FarmToolActionResult& evaluation,
    bool paused, bool cleared, FarmHUDNextAction nextAction = FarmHUDNextAction::SelectTile,
    bool contestSeason = false) {
    view.fieldActions = true;
    Label action = Label::ApplyHoe;
    switch (evaluation.tool) {
    case FarmTool::Hoe: action = Label::ApplyHoe; break;
    case FarmTool::Water: action = Label::ApplyWater; break;
    case FarmTool::Seed: action = Label::ApplySeed; break;
    case FarmTool::Harvest: action = Label::ApplyHarvest; break;
    default: break;
    }
    Label reason = Label::ActionReady;
    switch (evaluation.status) {
    case FarmToolActionStatus::Applied:
    case FarmToolActionStatus::Harvested: break;
    case FarmToolActionStatus::InvalidTarget: reason = Label::ActionNoTile; break;
    case FarmToolActionStatus::AlreadyWatered: reason = Label::ActionWaterFull; break;
    case FarmToolActionStatus::NoSeed: reason = Label::ActionNoSeed; break;
    case FarmToolActionStatus::Locked: reason = Label::StageLocked; break;
    case FarmToolActionStatus::InventoryFull: reason = Label::HarvestFull; break;
    case FarmToolActionStatus::NotReady: reason = Label::ActionGrowing; break;
    default: reason = Label::ActionOtherTool; break;
    }
    if (paused && evaluation.Succeeded()) reason = Label::Paused;
    if (cleared) reason = contestSeason ? Label::SeasonEnded : Label::ClearLocked;
    const bool shop = !cleared && evaluation.status == FarmToolActionStatus::NoSeed;
    Request request{shop ? Action::OpenSeedShop : Action::ApplyTool};
    bool enabled = !cleared && (evaluation.Succeeded() || shop);
    if (shop) action = Label::OpenSeedShop;
    if (!cleared && !evaluation.Succeeded() &&
        (evaluation.status == FarmToolActionStatus::InvalidState || evaluation.status == FarmToolActionStatus::NotReady ||
         evaluation.status == FarmToolActionStatus::AlreadyWatered)) {
        int toolIndex = -1;
        Label switchLabel = Label::ActionOtherTool;
        switch (nextAction) {
        case FarmHUDNextAction::Hoe: toolIndex = 0; switchLabel = Label::SwitchHoe; break;
        case FarmHUDNextAction::WaterOrSeed:
        case FarmHUDNextAction::Water: toolIndex = 1; switchLabel = Label::SwitchWater; break;
        case FarmHUDNextAction::Seed: toolIndex = 2; switchLabel = Label::SwitchSeed; break;
        case FarmHUDNextAction::Harvest: toolIndex = 3; switchLabel = Label::SwitchHarvest; break;
        case FarmHUDNextAction::BuySeed:
            action = Label::OpenSeedShop; reason = Label::ActionNoSeed;
            request = {Action::OpenSeedShop}; enabled = true; break;
        default: break;
        }
        constexpr FarmTool tools[] = {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest};
        if (toolIndex >= 0 && evaluation.tool != tools[toolIndex]) {
            action = switchLabel; reason = Label::NextTool;
            request = {Action::Tool, toolIndex}; enabled = true;
        }
    }
    view.Add(reason, {964, 590, 216, 36});
    view.Add(action, {964, 642, 216, 46}, request, enabled);
}
}
