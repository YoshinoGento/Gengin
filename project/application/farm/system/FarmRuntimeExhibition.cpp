#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmToolActionSystem.h"
#include "title/TitleAudioSettingsSystem.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "io/Input.h"

void FarmRuntimeController::ExecuteExhibition(const FarmRuntimeContext& s, farmui::Request request) {
    using farmui::Action;
    switch (request.action) {
    case Action::ExhibitionSelect: (void)exhibition_.Select(request.argument); break;
    case Action::ExhibitionWatch:
        (void)exhibition_.Begin(s.farmToolActionSystem.MaximumHarvestSize(exhibition_.Crop())); break;
    case Action::ExhibitionHome: exhibition_.Home(); break;
    case Action::ExhibitionExit:
        // Keep the exhibition frame alive until SceneManager captures the outgoing scene.
        if (exhibition_.Startup()) flow_.RequestTitle();
        else exhibition_.Close();
        break;
    case Action::ContestTab: (void)exhibition_.SelectPage(request.argument); break;
    case Action::ContestExplanation: exhibition_.ToggleExplanation(); break;
    case Action::ContestReveal: exhibition_.Reveal(); break;
    default: break;
    }
    focusedItem_ = -1;
}

bool FarmRuntimeController::UpdateExhibition(const FarmRuntimeContext& s, const Input& input) {
    frameCaptured_ = true;
    if (!s.host.IsViewportFocused()) {
        contestAudio_.Update(FarmContestPresentationSystem::Cue::None, true, false);
        return true;
    }
    s.host.ConvertMouseToVirtualScreen(input.GetMousePosition(), pointer_);
    BuildView(s);
    const bool complete = exhibition_.Complete();
    const auto phase = exhibition_.GetPhase();
    const auto* framework = Framework::GetInstance();
    const auto* clock = framework ? framework->GetFrameClock() : nullptr;
    exhibition_.Update(clock ? clock->GetRealDeltaSeconds() : 0);
    const bool accept = input.TriggerKey(InputKey::Enter) || input.TriggerKey(InputKey::Space) ||
        input.TriggerGamepadButton(InputGamepadButton::A);
    const bool back = input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::B);
    const auto hit = view_.Hit(pointer_);
    // Use the pre-update display: a reveal input cannot also start the next playback.
    if (input.TriggerMouseButton(InputMouseButton::Left) && hit.action != farmui::Action::None)
        ExecuteExhibition(s, hit);
    else if (!complete && (accept || back)) exhibition_.Reveal();
    else if (back) {
        if (phase == FarmExhibitionSystem::Phase::Menu) ExecuteExhibition(s, {farmui::Action::ExhibitionExit});
        else { exhibition_.Back(); focusedItem_ = -1; }
    } else if (complete) {
        const int step = (input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::ArrowRight) ||
            input.TriggerGamepadButton(InputGamepadButton::DPadDown) || input.TriggerGamepadButton(InputGamepadButton::DPadRight)) ? 1 :
            (input.TriggerKey(InputKey::ArrowUp) || input.TriggerKey(InputKey::ArrowLeft) ||
            input.TriggerGamepadButton(InputGamepadButton::DPadUp) || input.TriggerGamepadButton(InputGamepadButton::DPadLeft)) ? -1 : 0;
        if (step) focusedItem_ = view_.NextActionable(focusedItem_, step);
        if (accept) {
            if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) ExecuteExhibition(s, view_.items[focusedItem_].request);
            else if (phase == FarmExhibitionSystem::Phase::Menu) ExecuteExhibition(s, {farmui::Action::ExhibitionWatch});
            else { exhibition_.Back(); focusedItem_ = -1; }
        }
    }
    if (exhibition_.ConsumeBeginning()) {
        title::TitleAudioSettingsSystem settings;
        settings.Initialize(); contestAudio_.Begin(settings.GetContestVolume());
    }
    const bool playing = exhibition_.GetPhase() == FarmExhibitionSystem::Phase::Result ||
        exhibition_.GetPhase() == FarmExhibitionSystem::Phase::Missed;
    contestAudio_.Update(exhibition_.ConsumeCue(), playing, true);
    BuildView(s);
    return true;
}
