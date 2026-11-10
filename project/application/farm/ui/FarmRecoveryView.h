#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmRecoverySystem.h"

namespace farmui {
inline void BuildRecoveryView(View& view, const FarmRecoverySystem::View& state) {
    using Status = FarmRecoverySystem::Status;
    view.Add(Label::Recovery, {176,128,920,44});
    view.Add(Label::RecoveryOffer, {176,198,920,40});
    view.Add(Label::RecoveryRule, {176,250,920,38});
    Label reason = Label::RecoveryBlocked;
    Label next = Label::RecoveryNext;
    Action action = Action::None;
    Label actionLabel = Label::RecoveryClaim;
    switch (state.status) {
    case Status::Eligible: reason = Label::RecoveryEligible; action = Action::RecoveryClaim; break;
    case Status::ClaimedToday: reason = Label::RecoveryDaily; next = Label::RecoveryTomorrow; break;
    case Status::SeedsAvailable: reason = Label::RecoverySeeds; break;
    case Status::CropGrowing: reason = Label::RecoveryGrowing; next = Label::RecoveryCare; break;
    case Status::HarvestAvailable:
        reason = Label::RecoveryHarvest; next = Label::RecoveryProtected;
        action = Action::HarvestInventory; actionLabel = Label::HarvestInventory; break;
    case Status::CanBuy:
        reason = Label::RecoveryBuy; action = Action::OpenSeedShop; actionLabel = Label::ShopTitle; break;
    default: break;
    }
    view.Add(reason, {176,326,920,40});
    if (state.status != Status::Blocked) view.Add(next, {176,382,920,38});
    view.Add(actionLabel, {176,466,450,48},
        {action, state.day, state.inventoryGeneration, state.farmGeneration}, action != Action::None);
    view.Metric(Label::ShopMoney, {646,466,450,40}, 160, std::to_string(state.money)+"G");
    view.Add(Label::RecoveryNoCash, {176,548,920,38});
}

inline void BuildRecoveryPrompt(View& view, FarmRecoverySystem::Status status) {
    view.shortGoal = true;
    view.Add(status == FarmRecoverySystem::Status::Eligible ? Label::RecoveryHelp : Label::RecoveryDaily,
        HUDLayout::shortGoal, {Action::Recovery});
}
}
