#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/ui/FarmMenuView.h"
#include "farm/ui/FarmObservationView.h"
#include "farm/ui/FarmSaleView.h"
#include "io/Input.h"
#include <algorithm>
#include <cmath>

void FarmRuntimeController::BuildSaleOverview(const FarmRuntimeContext& s) {
    farmui::SaleViewState state;
    const auto& economy=s.farmEconomySystem;
    state.confirming=saleReview_.Confirming(); state.notice=saleReview_.GetNotice();
    state.quote=state.confirming ? saleReview_.Frozen() : saleReview_.Current(economy);
    state.owned[0]=economy.GetTotalCropCount();
    for (int i=0;i<farm::kPlayableCropCount;++i) state.owned[i+1]=economy.GetCropCount(farm::PlayableCrop(i));
    state.editable=!s.farmProgressionSystem.IsCleared() && !s.farmIrrigationPreviewSystem.IsActive() && !s.host.IsTimelineScrubbing();
    farmui::BuildSaleView(view_,state);
    if (focusedItem_>=0 && focusedItem_<static_cast<int>(view_.count)) view_.items[focusedItem_].focused=true;
}

bool FarmRuntimeController::HandleSaleRequest(const FarmRuntimeContext& s,farmui::Request request) {
    using farmui::Action;
    const bool active=open_ && page_==kHarvestInventoryPage && !harvestDetails_ && pending_==Action::None;
    const bool saleAction=request.action==Action::SaleSelect || request.action==Action::SaleReview ||
        request.action==Action::SaleConfirm || request.action==Action::SaleCancel || request.action==Action::HarvestDetails;
    if (!active) return saleAction;
    // The confirmation owns all input, including menu-tab and footer shortcuts.
    if (saleReview_.Confirming() && request.action!=Action::SaleConfirm && request.action!=Action::SaleCancel) return true;
    const bool editable=!s.farmProgressionSystem.IsCleared() && !s.farmIrrigationPreviewSystem.IsActive() && !s.host.IsTimelineScrubbing();
    switch (request.action) {
    case Action::SaleSelect:
        if (request.argument>=0 && request.argument<=farm::kPlayableCropCount)
            (void)saleReview_.Select(request.argument==0 ? farm::CropType::None : farm::PlayableCrop(request.argument-1));
        break;
    case Action::SaleReview:
        if (editable) (void)saleReview_.Begin(s.farmEconomySystem);
        break;
    case Action::SaleCancel: saleReview_.Cancel(); break;
    case Action::SaleConfirm:
        if (editable) {
            const auto result=saleReview_.Confirm(s.farmEconomySystem);
            if (result.Succeeded()) s.host.RouteFarmSale(result);
        } else saleReview_.Cancel();
        break;
    case Action::HarvestDetails:
        harvestDetails_=true; harvestPage_=0; break;
    default: return false;
    }
    focusedItem_=-1;
    return true;
}

void FarmRuntimeController::BuildMenuOverview(const FarmRuntimeContext& s) {
    const auto hud=s.host.BuildFarmHUDViewData();
    farmui::MenuOverviewState state;
    state.page=page_;
    state.unlocked=!s.farmProgressionSystem.IsCleared() && !s.farmIrrigationPreviewSystem.IsActive();
    state.money=hud.money; state.seeds=hud.seedCount;
    state.maximumHeight=s.farmToolActionSystem.MaximumAllowedHeight();
    state.suppliedCanals=s.farmIrrigationSystem.GetSuppliedCanalCount();
    state.suppliedTiles=s.farmIrrigationSystem.GetIrrigationRangeTileCount();
    state.following=s.host.IsFollowingPlayer();
    state.contestSeason=hud.contestSeason; state.freeFarming=hud.freeFarming;
    if (page_==2) {
        const auto& docs=s.farmDocumentSystem.GetDocuments();
        documentIndex_=std::clamp(documentIndex_,0,(std::max)(0,static_cast<int>(docs.size())-1));
        state.documentIndex=documentIndex_; state.documents=static_cast<int>(docs.size());
        state.canSaveCopy=!s.farmIrrigationPreviewSystem.IsActive();
        state.canSave=state.canSaveCopy && s.farmDocumentSystem.FileExists();
        state.dirty=s.farmDocumentSystem.IsDirty();
        state.recordNames={s.farmDocumentSystem.GetDisplayName(),docs.empty() ? "記録なし" : docs[documentIndex_].displayName};
    }
    if (page_==3) {
        const auto comparison=s.farmGrowthComparisonSystem.GetView(s.farmGrid);
        state.comparing=comparison.status==FarmComparisonStatus::Running;
        state.canCompare=state.unlocked && !state.comparing && comparison.startIssue==FarmComparisonIssue::None;
        state.compareA=comparison.rows[0].tileIndex; state.compareB=comparison.rows[1].tileIndex;
        if (comparison.startIssue!=FarmComparisonIssue::None)
            state.compareIssue=farmui::ComparisonIssueLabel(comparison.startIssue);
    }
    farmui::BuildMenuOverview(view_,state);
    ui_.PrepareNames(view_);
    if (focusedItem_>=0 && focusedItem_<static_cast<int>(view_.count)) view_.items[focusedItem_].focused=true;
}

bool FarmRuntimeController::UpdateMenuAudio(const FarmRuntimeContext& s,const Input& input) {
    if (!menuAudio_.GetFrame().open) return false;
    title::AudioSettingsInput request;
    request.pointer=pointer_;
    request.pointerValid=std::isfinite(pointer_.x) && std::isfinite(pointer_.y) &&
        pointer_.x>=0 && pointer_.y>=0 && pointer_.x<1280 && pointer_.y<720;
    request.pressed=input.TriggerMouseButton(InputMouseButton::Left);
    request.held=input.PushMouseButton(InputMouseButton::Left);
    request.toggle=input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::Start);
    request.back=input.TriggerGamepadButton(InputGamepadButton::B);
    request.confirm=input.TriggerKey(InputKey::Enter) || input.TriggerKey(InputKey::Space) || input.TriggerGamepadButton(InputGamepadButton::A);
    request.up=input.TriggerKey(InputKey::ArrowUp) || input.TriggerGamepadButton(InputGamepadButton::DPadUp);
    request.down=input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::Tab) || input.TriggerGamepadButton(InputGamepadButton::DPadDown);
    request.left=input.TriggerKey(InputKey::ArrowLeft) || input.TriggerGamepadButton(InputGamepadButton::DPadLeft);
    request.right=input.TriggerKey(InputKey::ArrowRight) || input.TriggerGamepadButton(InputGamepadButton::DPadRight);
    (void)menuAudio_.Update(request);
    if (!menuAudio_.GetFrame().open && menuAudio_.GetFrame().saveFailed)
        status_=farmui::Label::MenuAudioFailed;
    BuildView(s);
    frameCaptured_=true;
    return true;
}
