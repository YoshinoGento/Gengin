#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmExperienceSystem.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmContestDaySystem.h"
#include "farm/system/FarmPestSystem.h"
#include "farm/ui/FarmExperienceView.h"
#include "editor/GamePlayEditorBridge.h"

void FarmRuntimeController::BuildExperienceMenu() {
    farmui::BuildExperienceView(view_, experienceSelection_, experienceConfirm_, status_ == farmui::Label::Failure);
    if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
}

void FarmRuntimeController::BuildExperienceHUD(const FarmRuntimeContext& s) {
    if (!s.farmDocumentSystem.IsPrepared()) activeExperience_ = -1;
    farmui::BuildExperienceHUD(view_, activeExperience_, !s.farmIrrigationPreviewSystem.IsActive() &&
        !s.farmCropSelectionSystem.IsOpen() && !s.host.IsTimelineScrubbing());
}

bool FarmRuntimeController::HandleExperienceRequest(const FarmRuntimeContext& s, farmui::Request request) {
    using farmui::Action;
    using farmui::Label;
    using Topic = FarmExperienceSystem::Topic;
    const bool menu = open_ && page_ == kExperiencePage;
    if (menu && experienceConfirm_ && request.action != Action::ExperienceConfirm &&
        request.action != Action::ExperienceCancel && request.action != Action::Close &&
        request.action != Action::MenuDetails) return true;
    if (menu && (request.action == Action::Close || request.action == Action::MenuDetails)) {
        if (experienceConfirm_) { experienceConfirm_ = false; focusedItem_ = -1; return true; }
        open_ = false; page_ = 0; focusedItem_ = -1; return true;
    }
    if (menu && request.action == Action::Tab) {
        if (experienceConfirm_) return true;
        // Only ordinary menu pages can be requested from the footer.
        if (request.argument < 0 || request.argument >= 5) return true;
        page_ = request.argument; menuDetails_ = false; focusedItem_ = -1; return true;
    }
    switch (request.action) {
    case Action::ExperienceOpen:
        if (pending_ != Action::None || shop_.IsOpen() || s.host.IsTimelineScrubbing() ||
            s.farmIrrigationPreviewSystem.IsActive() || s.farmCropSelectionSystem.IsOpen() ||
            flow_.GetPhase() == FarmPlayFlow::Phase::Starting) return true;
        s.farmTimeAdvance.Interrupt();
        open_ = true; page_ = kExperiencePage; experienceConfirm_ = false;
        status_ = Label::Ready; focusedItem_ = -1; return true;
    case Action::ExperiencePage:
        if (menu && !experienceConfirm_ && request.argument >= 0 && request.argument < FarmExperienceSystem::kPages)
            experienceSelection_ = request.argument * FarmExperienceSystem::kPageSize;
        focusedItem_ = -1; return true;
    case Action::ExperienceSelect:
        if (menu && !experienceConfirm_ && FarmExperienceSystem::Valid(request.argument)) experienceSelection_ = request.argument;
        focusedItem_ = -1; return true;
    case Action::ExperienceBegin:
        if (menu && !experienceConfirm_) experienceConfirm_ = true;
        focusedItem_ = -1; return true;
    case Action::ExperienceCancel:
        experienceConfirm_ = false; focusedItem_ = -1; return true;
    case Action::ExperienceConfirm: {
        if (!menu || !experienceConfirm_ || !FarmExperienceSystem::Valid(experienceSelection_)) return true;
        experienceConfirm_ = false; focusedItem_ = -1;
        editor::FarmDocumentCommand command{};
        command.type = editor::FarmDocumentCommandType::LoadExperience;
        command.experienceIndex = experienceSelection_;
        if (!s.gamePlayEditorBridge.Execute(command)) { status_ = Label::Failure; return true; }
        activeExperience_ = experienceSelection_;
        contestPresentation_.Close(); contestOutcome_.Reset(); shop_.Close(); saleReview_.Cancel();
        s.farmCropSelectionSystem.Cancel(); s.farmTimeAdvance.Reset();
        flow_.Reset(); flow_.Continue();
        open_ = observation_ = terrain_ = paused_ = contestNotice_ = false;
        pending_ = Action::None; pendingSubmission_ = {}; pendingExpansion_ = {}; pendingScreenTransition_ = {};
        pendingDocumentId_.clear(); pendingContestDay_ = 0;
        pickingSlot_ = -1; brush_ = 0; page_ = 0; harvestPage_ = 0;
        menuDetails_ = harvestDetails_ = false;
        status_ = Label::Ready;
        FrameFarm(s);
        return true;
    }
    case Action::ExperienceAction: {
        if (open_ || !s.farmDocumentSystem.IsPrepared() || !FarmExperienceSystem::Valid(activeExperience_) ||
            flow_.BlocksSimulation()) return true;
        Action action = Action::None;
        int argument = 0;
        switch (static_cast<Topic>(activeExperience_)) {
        case Topic::Plant: action = Action::Tool; argument = 2; break;
        case Topic::Harvest: action = Action::Tool; argument = 3; break;
        case Topic::Soil: action = Action::SoilCare; break;
        case Topic::Trade: action = Action::HarvestInventory; break;
        case Topic::Water: action = Action::TerrainField; break;
        case Topic::Rain: action = Action::TerrainField; break;
        case Topic::Pests: action = Action::PestFocus; argument = FarmPestSystem::FindActive(s.farmGrid); break;
        case Topic::Repellent: action = Action::Repellent; break;
        case Topic::Time: action = Action::TimeAdvance; break;
        case Topic::Judging:
            Execute(s, {Action::HarvestDisplay}, false);
            action = Action::ContestPreview; argument = 1; break;
        case Topic::Results: OpenContestDetail(s, 0, true); break;
        case Topic::Development: action = Action::Development; break;
        case Topic::Book: action = Action::CropBook; break;
        case Topic::Compare: action = Action::ObserveField; break;
        case Topic::Recovery: action = Action::Recovery; break;
        case Topic::Records: open_ = true; page_ = 2; menuDetails_ = false; break;
        default: break;
        }
        if (action != Action::None) Execute(s, {action, argument}, false);
        return true;
    }
    default: return false;
    }
}
