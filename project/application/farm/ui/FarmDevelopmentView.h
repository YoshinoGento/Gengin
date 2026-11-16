#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmDevelopmentSystem.h"

namespace farmui {
inline void BuildDevelopmentView(View& view, const FarmDevelopmentSystem::View& state) {
    view.Add(Label::Development, {176,128,920,40});
    view.Metric(Label::DevelopmentRank, {176,186,450,38}, 260, std::to_string(state.rank));
    view.Metric(Label::DevelopmentArea, {646,186,450,38}, 260, std::to_string(state.tiles));
    if (!state.standardFarm || state.maximumArea) {
        view.Add(state.maximumArea ? Label::DevelopmentFull : Label::DevelopmentCustom, {176,278,920,40});
        return;
    }
    view.Metric(Label::DevelopmentSalesGoal, {176,238,920,38}, 430,
        std::to_string(state.sales)+" / "+std::to_string(state.requiredSales)+"G");
    if (state.unlockDay != 0)
        view.Metric(state.stageReady ? Label::DevelopmentDayReady : Label::DevelopmentDay,
            {176,290,920,38}, 540, std::to_string(state.unlockDay));
    else view.Add(Label::DevelopmentNoDay, {176,290,920,38});
    view.Metric(Label::DevelopmentCost, {176,342,920,38}, 540, std::to_string(state.cost)+"G");
    view.Metric(Label::ShopMoney, {176,394,920,38}, 540, std::to_string(state.money)+"G");
    view.Metric(Label::DevelopmentReserve, {176,446,920,38}, 540, std::to_string(state.seedReserve)+"G");
    view.Add(Label::DevelopmentExpand, {176,502,450,48},
        {Action::ExpandFarm, state.height, state.quote.inventoryGeneration, state.quote.farmGeneration}, state.canExpand);
    if (!state.stageReady) view.Add(Label::DevelopmentWaitDay, {176,566,920,38});
    else if (!state.salesReady) view.Add(Label::DevelopmentNeedSales, {176,566,920,38});
    else if (!state.affordable) view.Add(Label::DevelopmentNeedFunds, {176,566,920,38});
    else if (!state.canExpand) view.Add(Label::DevelopmentLocked, {176,566,920,38});
}
inline void BuildDevelopmentConfirmation(View& view, const FarmDevelopmentSystem::View& offer, bool current) {
    view.Add(Label::DevelopmentConfirm, {176,148,920,44});
    view.Metric(Label::DevelopmentArea, {176,224,920,38}, 540,
        std::to_string(offer.tiles)+" -> "+std::to_string(offer.tiles+FarmDevelopmentSystem::kWidth));
    view.Metric(Label::DevelopmentCost, {176,280,920,38}, 540, std::to_string(offer.cost)+"G");
    view.Metric(Label::DevelopmentAfter, {176,336,920,38}, 540, std::to_string(offer.moneyAfter)+"G");
    view.Metric(Label::DevelopmentReserve, {176,392,920,38}, 540, std::to_string(offer.seedReserve)+"G");
    view.Add(current ? Label::DevelopmentKeep : Label::DevelopmentChanged, {176,448,920,38});
    view.Add(Label::DevelopmentPay, {176,516,450,48}, {Action::Accept}, current);
    view.Add(Label::Cancel, {646,516,450,48}, {Action::Cancel});
}
}
