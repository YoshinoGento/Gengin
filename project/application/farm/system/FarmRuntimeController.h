#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/system/FarmPlayFlow.h"
#include "farm/system/FarmStartPresentationSystem.h"
#include "farm/system/FarmArrivalAudioSystem.h"
#include "farm/system/FarmSeedShopSystem.h"
#include "farm/system/FarmHarvestDisplaySystem.h"
#include "farm/system/FarmContestOutcomeSystem.h"
#include "farm/system/FarmContestAudioSystem.h"
#include "farm/system/FarmExhibitionSystem.h"

struct FarmRuntimeContext;
class Input;

// Routes Release controls to existing gameplay Systems; owns only menu/input state.
class FarmRuntimeController final {
public:
    bool Initialize(SpriteCommon* common) { exhibition_.Initialize(); ready_ = ui_.Initialize(common); return ready_; }
    InitializationTask InitializeIncrementally(SpriteCommon* common) {
        exhibition_.Initialize();
        ready_ = co_await ui_.InitializeIncrementally(common);
        co_return ready_;
    }
    bool Update(const FarmRuntimeContext& context, const Input& input);
    void InitializePresentationAudio(Audio* audio) {
        (void)arrivalAudio_.Initialize(audio); (void)contestAudio_.Initialize(audio);
    }
    void FinalizePresentationAudio() { arrivalAudio_.Finalize(); contestAudio_.Finalize(); }
    FarmStartPresentationSystem::Frame StartFrame() const noexcept {
        return flow_.GetPhase()==FarmPlayFlow::Phase::Starting ? startPresentation_.GetFrame() : FarmStartPresentationSystem::Frame{};
    }
    void Draw() { ui_.Draw(view_, pointer_); }
    bool BlocksSimulation() const noexcept { return exhibition_.Active() || shop_.IsOpen() || contestNotice_ || open_ || paused_ || terrain_ || frameCaptured_ || (ready_ && flow_.BlocksSimulation()); }
    bool ConsumesInput() const noexcept { return exhibition_.Active() || shop_.IsOpen() || contestNotice_ || open_ || paused_ || terrain_ || observation_ || frameCaptured_ || (ready_ && flow_.BlocksSimulation()); }
    bool IsOpen() const noexcept { return open_; }
    bool RequestsTitle() const noexcept { return flow_.RequestsTitle(); }
    bool PresentsFarmResult() const noexcept { return ready_ &&
        (flow_.GetPhase() == FarmPlayFlow::Phase::Result || flow_.GetPhase() == FarmPlayFlow::Phase::Review); }
    bool HidesFarmHUD() const noexcept { return exhibition_.Active() || shop_.IsOpen() || contestNotice_ || open_ || observation_ || terrain_ || (ready_ && flow_.BlocksSimulation()); }
    bool PresentsIntroduction() const noexcept { return ready_ &&
        (flow_.GetPhase() == FarmPlayFlow::Phase::Briefing || flow_.GetPhase() == FarmPlayFlow::Phase::Starting); }
    bool ShowsSeedShop() const noexcept { return view_.seedShop; }
    int SeedShopSelection() const noexcept { return shop_.Selection(); }
    bool ShowsHarvestDisplay() const noexcept { return view_.harvestDisplay; }
    int HarvestDisplaySelection() const noexcept { return view_.counterSelection; }
    bool ShowsContestDetail() const noexcept { return view_.contestDetail; }
    FarmContestPresentationSystem::Frame ContestFrame() const noexcept { return exhibition_.Active() ? exhibition_.ContestFrame() : contestPresentation_.GetFrame(); }
    [[nodiscard]] int ResolveHoveredTile(const FarmRuntimeContext& context) const;
    void Refresh(const FarmRuntimeContext& context) { BuildView(context); }
    void RequestSeasonRetry(const FarmRuntimeContext& context) {
        if (!ready_) return;
        BuildView(context); Execute(context, {farmui::Action::SeasonRetry}); BuildView(context);
    }
    void CommitScreenTransition(const FarmRuntimeContext& context);
    void OpenLayoutLibrary(const FarmRuntimeContext& context);
    void UpdateFollowCamera(const FarmRuntimeContext& context) { FrameFarm(context, true); }
private:
    bool UpdateExhibition(const FarmRuntimeContext& context, const Input& input);
    void ExecuteExhibition(const FarmRuntimeContext& context, farmui::Request request);
    void BuildView(const FarmRuntimeContext& context);
    void OpenContestDetail(const FarmRuntimeContext& context, int index, bool replay);
    void Execute(const FarmRuntimeContext& context, farmui::Request request, bool allowTransition = true);
    bool DeferScreenTransition(const FarmRuntimeContext& context, farmui::Request request);
    void EnterObservation(const FarmRuntimeContext& context);
    void EnterTerrain(const FarmRuntimeContext& context);
    void LeaveTerrain(const FarmRuntimeContext& context);
    bool UpdateTerrain(const FarmRuntimeContext& context, const Input& input, bool click);
    bool FrameFarm(const FarmRuntimeContext& context, bool followPlayer = false, bool briefing = false);
    void BeginFarmStart(const FarmRuntimeContext& context);
    void ApplyStartCamera(const FarmRuntimeContext& context);
    bool UpdateObservation(const FarmRuntimeContext& context, const Input& input, bool click);
    farmui::RuntimeUI ui_;
    FarmPlayFlow flow_;
    FarmStartPresentationSystem startPresentation_;
    FarmArrivalAudioSystem arrivalAudio_;
    FarmSeedShopSystem shop_;
    FarmHarvestDisplaySystem harvestDisplay_;
    FarmContestPresentationSystem contestPresentation_;
    FarmContestAudioSystem contestAudio_;
    FarmContestOutcomeSystem contestOutcome_;
    FarmExhibitionSystem exhibition_;
    bool seasonRetryFailed_ = false;
    farmui::View view_;
    Vector2 pointer_{-1, -1};
    farmui::Action pending_ = farmui::Action::None;
    farmui::Request pendingSubmission_{};
    farmui::Request pendingScreenTransition_{};
    bool pointerTriggered_ = false;
    int pendingContestDay_ = 0;
    int pendingPlayMode_ = 0;
    farmui::Label status_ = farmui::Label::Ready;
    std::string pendingDocumentId_;
    int page_ = 0;
    int documentIndex_ = 0;
    int harvestPage_ = 0;
    int brush_ = 0;
    int focusedItem_ = -1;
    bool open_ = false;
    bool paused_ = false;
    bool ready_ = false;
    bool frameCaptured_ = false;
    bool contestNotice_ = false;
    bool observation_ = false;
    bool terrain_ = false;
    bool pauseBeforeObservation_ = false;
    int pickingSlot_ = -1;
    FarmComparisonStatus lastComparisonStatus_ = FarmComparisonStatus::Idle;
};
