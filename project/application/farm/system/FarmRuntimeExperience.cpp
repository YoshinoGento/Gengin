#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"
#include "farm/system/FarmExperienceSystem.h"
#include "farm/ui/FarmExperienceView.h"
#include "editor/GamePlayEditorBridge.h"

void FarmRuntimeController::BuildExperienceMenu() {
    farmui::BuildExperienceView(view_, experienceSelection_, experienceConfirm_, status_ == farmui::Label::Failure);
    if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
}

bool FarmRuntimeController::HandleExperienceRequest(const FarmRuntimeContext& s, farmui::Request request) {
    using farmui::Action;
    using farmui::Label;
    using Topic = FarmExperienceSystem::Topic;
    if (experienceConfirm_ && request.action != Action::ExperienceConfirm &&
        request.action != Action::ExperienceCancel && request.action != Action::Close &&
        request.action != Action::MenuDetails) return true;
    if (request.action == Action::Close || request.action == Action::MenuDetails) {
        if (experienceConfirm_) { experienceConfirm_ = false; focusedItem_ = -1; return true; }
        open_ = false; page_ = 0; focusedItem_ = -1; return true;
    }
    if (request.action == Action::Tab) {
        if (experienceConfirm_) return true;
        // Only ordinary menu pages can be requested from the footer.
        if (request.argument < 0 || request.argument >= 5) return true;
        page_ = request.argument; menuDetails_ = false; focusedItem_ = -1; return true;
    }
    switch (request.action) {
    case Action::ExperiencePage:
        if (!experienceConfirm_ && request.argument >= 0 && request.argument < FarmExperienceSystem::kPages)
            experienceSelection_ = request.argument * FarmExperienceSystem::kPageSize;
        focusedItem_ = -1; return true;
    case Action::ExperienceSelect:
        if (!experienceConfirm_ && FarmExperienceSystem::Valid(request.argument)) experienceSelection_ = request.argument;
        focusedItem_ = -1; return true;
    case Action::ExperienceBegin:
        experienceConfirm_ = true;
        focusedItem_ = -1; return true;
    case Action::ExperienceCancel:
        experienceConfirm_ = false; focusedItem_ = -1; return true;
    case Action::ExperienceConfirm: {
        if (!experienceConfirm_ || !FarmExperienceSystem::Valid(experienceSelection_)) return true;
        experienceConfirm_ = false; focusedItem_ = -1;
        editor::FarmDocumentCommand command{};
        command.type = editor::FarmDocumentCommandType::LoadExperience;
        command.experienceIndex = experienceSelection_;
        if (!s.gamePlayEditorBridge.Execute(command)) { status_ = Label::Failure; return true; }
        contestPresentation_.Close(); contestOutcome_.Reset(); shop_.Close(); saleReview_.Cancel();
        s.farmCropSelectionSystem.Cancel(); s.farmTimeAdvance.Reset();
        flow_.Reset();
        if (static_cast<Topic>(experienceSelection_) != Topic::CampaignStart) flow_.Continue();
        open_ = observation_ = terrain_ = paused_ = false;
        pending_ = Action::None; pendingSubmission_ = {}; pendingExpansion_ = {}; pendingScreenTransition_ = {};
        pendingDocumentId_.clear(); pendingContestDay_ = 0;
        pickingSlot_ = -1; brush_ = 0; page_ = 0; harvestPage_ = 0;
        menuDetails_ = harvestDetails_ = false;
        status_ = Label::Ready;
        FrameFarm(s);
        return true;
    }
    default: return false;
    }
}
