#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmControlGuideView.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/system/FarmPlayFlow.h"
#include "farm/system/FarmStartPresentationSystem.h"
#include "farm/system/FarmArrivalAudioSystem.h"
#include "farm/system/FarmSeedShopSystem.h"
#include "farm/system/FarmSaleReviewSystem.h"
#include "farm/system/FarmHarvestDisplaySystem.h"
#include "farm/system/FarmContestOutcomeSystem.h"
#include "farm/system/FarmContestAudioSystem.h"
#include "farm/system/FarmPestAudioSystem.h"
#include "farm/system/FarmExhibitionSystem.h"
#include "farm/system/FarmDevelopmentSystem.h"
#include "farm/system/FarmCanalLessonSystem.h"
#include "farm/system/FarmFieldInput.h"
#include "farm/system/FarmWalkSystem.h"
#include <optional>

struct FarmRuntimeContext;
namespace farmui { struct TerrainViewState; }
class Input;

// Routes Release controls to existing gameplay Systems; owns only menu/input state.
class FarmRuntimeController final {
public:
    bool Initialize(SpriteCommon* common) { InitializeEdition(); ready_ = ui_.Initialize(common); return ready_; }
    InitializationTask InitializeIncrementally(SpriteCommon* common) {
        InitializeEdition();
        ready_ = co_await ui_.InitializeIncrementally(common);
        co_return ready_;
    }
    bool Update(const FarmRuntimeContext& context, const Input& input);
    void InitializePresentationAudio(Audio* audio) {
        menuAudio_.Initialize();
        (void)arrivalAudio_.Initialize(audio); (void)contestAudio_.Initialize(audio);
        (void)pestAudio_.Initialize(audio);
    }
    void FinalizePresentationAudio() { arrivalAudio_.Finalize(); contestAudio_.Finalize(); pestAudio_.Finalize(); }
    [[nodiscard]] int EnvironmentVolume() const noexcept { return menuAudio_.GetFrame().volume[0]; }
    [[nodiscard]] int WaterVolume() const noexcept { return menuAudio_.GetFrame().volume[1]; }
    [[nodiscard]] int EffectVolume() const noexcept { return menuAudio_.GetContestVolume(); }
    FarmStartPresentationSystem::Frame StartFrame() const noexcept {
        return flow_.GetPhase()==FarmPlayFlow::Phase::Starting ? startPresentation_.GetFrame() : FarmStartPresentationSystem::Frame{};
    }
    void Draw() { ui_.Draw(view_, pointer_); }
    void PauseForPestWarning() noexcept { paused_ = true; }
    void PauseForWeatherChange() noexcept { paused_ = true; }
    void PauseOnComparisonEnd(bool ended) noexcept { if (ended && observation_) paused_ = true; }
    bool BlocksTimeAdvance() const noexcept { return exhibition_.Active() || shop_.IsOpen() || HasBlockingContestNotice() || open_ || terrain_ || observation_ || (ready_ && flow_.BlocksSimulation()); }
    bool BlocksSimulation() const noexcept { return exhibition_.Active() || shop_.IsOpen() || HasBlockingContestNotice() || open_ || paused_ || terrain_ || frameCaptured_ || (ready_ && flow_.BlocksSimulation()); }
    bool ConsumesInput() const noexcept { return fieldInputConsumed_ || exhibition_.Active() || shop_.IsOpen() || HasBlockingContestNotice() || open_ || paused_ || terrain_ || observation_ || frameCaptured_ || (ready_ && flow_.BlocksSimulation()); }
    bool IsOpen() const noexcept { return open_; }
    const farm::FarmerPose* Farmer() const noexcept { return walk_.Active() ? &walk_.Pose() : nullptr; }
    bool TakeWalkHit() noexcept { const bool hit=walkHit_; walkHit_=false; return hit; }
    bool CapturesWheel() const noexcept { return wheelCaptured_; }
    const farm::FarmCanalLessonSystem* CanalLesson() const noexcept { return lesson_ ? &*lesson_ : nullptr; }
    bool RequestsTitle() const noexcept { return flow_.RequestsTitle(); }
    bool PresentsFarmResult() const noexcept { return ready_ &&
        (flow_.GetPhase() == FarmPlayFlow::Phase::Result || flow_.GetPhase() == FarmPlayFlow::Phase::Review); }
    bool HidesFarmHUD() const noexcept { return exhibition_.Active() || shop_.IsOpen() || HasBlockingContestNotice() || open_ || observation_ || terrain_ || (ready_ && flow_.BlocksSimulation()); }
    bool PresentsIntroduction() const noexcept { return ready_ &&
        (flow_.GetPhase() == FarmPlayFlow::Phase::Briefing || flow_.GetPhase() == FarmPlayFlow::Phase::Starting); }
    bool ShowsSeedShop() const noexcept { return view_.seedShop; }
    bool SimpleField() const noexcept { return view_.simpleField; }
    bool ShowsLiveGap() const noexcept { return entryGap_.value_or(-1)>=0 && view_.briefing && !open_; }
    bool ShowsIrrigationForecast() const noexcept { return view_.irrigationForecast; }
    int SeedShopSelection() const noexcept { return shop_.Selection(); }
    bool ShowsHarvestDisplay() const noexcept { return view_.harvestDisplay; }
    int HarvestDisplaySelection() const noexcept { return view_.counterSelection; }
    bool ShowsContestDetail() const noexcept { return view_.contestDetail; }
    FarmContestPresentationSystem::Frame ContestFrame() const noexcept { return exhibition_.Active() ? exhibition_.ContestFrame() : contestPresentation_.GetFrame(); }
    [[nodiscard]] int ResolveHoveredTile(const FarmRuntimeContext& context) const;
    void UpdateFieldWheelHint(const FarmRuntimeContext& context, const Input& input, int hoveredTile);
    [[nodiscard]] bool AllowsFieldHover(const FarmRuntimeContext& context) const;
    void ObserveProgress(const FarmRuntimeContext& context);
    void Refresh(const FarmRuntimeContext& context) { BuildView(context); }
    void RequestSeasonRetry(const FarmRuntimeContext& context) {
        if (!ready_) return;
        Execute(context, {farmui::Action::SeasonRetry}); BuildView(context);
    }
    void CommitScreenTransition(const FarmRuntimeContext& context);
    void OpenLayoutLibrary(const FarmRuntimeContext& context);
    void UpdateFollowCamera(const FarmRuntimeContext& context) { FrameFarm(context, true); }
private:
    [[nodiscard]] farm::FieldWheelRoute WheelRoute(const FarmRuntimeContext& context) const;
    [[nodiscard]] int CurrentWheelSlot(const FarmRuntimeContext& context, farm::FieldWheelRoute route) const;
    void InspectForecast(const FarmRuntimeContext& context, farmui::TerrainViewState& state);
    struct ForecastPick {
        std::uint64_t revision{};
        Vector3 origin{}, direction{};
        int index = -1;
        bool valid = false;
    } forecastPick_;
    bool OffersCanalLesson(const FarmRuntimeContext& context) const;
    bool UpdateLiveGap(const FarmRuntimeContext&, const Input&, bool click);
    std::optional<int> entryGap_;
    bool gapMissed_ = false, fieldDetails_ = false;
    std::optional<farmui::GuideDevice> controlGuide_;
    bool guideFromMenu_ = false;
    bool UpdateControlGuide(const FarmRuntimeContext&, const Input&, bool insideViewport);
    farm::FieldWheelNotice::Clock::time_point gapNoticeEnd_{};
    bool UpdateCanalLesson(const FarmRuntimeContext& context, const Input& input, bool click, farmui::Action action);
    std::optional<farm::FarmCanalLessonSystem> lesson_;
    static constexpr int kExperiencePage = 19;
    bool HandleExperienceRequest(const FarmRuntimeContext& context, farmui::Request request);
    void BuildExperienceMenu();
    int experienceSelection_ = 0;
    bool experienceConfirm_ = false;
    static constexpr int kHarvestInventoryPage = 8;
    void BuildSaleOverview(const FarmRuntimeContext& context);
    bool HandleSaleRequest(const FarmRuntimeContext& context, farmui::Request request);
    bool UpdateExhibition(const FarmRuntimeContext& context, const Input& input);
    void BuildMenuOverview(const FarmRuntimeContext& context);
    bool UpdateMenuAudio(const FarmRuntimeContext& context, const Input& input);
    void ExecuteExhibition(const FarmRuntimeContext& context, farmui::Request request);
    void BuildView(const FarmRuntimeContext& context);
    bool UpdateWalk(const FarmRuntimeContext&,const Input&,bool inside,bool click);
    farm::FarmWalkSystem walk_;
    Vector2 walkPointer_{};
    int walkDestination_=-1;
    bool walkHit_{},walkWaitRelease_{};
    void BuildObservationOverlay(farmui::View& view, const FarmRuntimeContext& context) const;
    [[nodiscard]] FarmContestOutcomeSystem::Notice CurrentContestNotice() const noexcept;
    [[nodiscard]] bool HasBlockingContestNotice() const noexcept;
    void UpdateContestScreen();
    void BeginContestAudio();
    void OpenContestDetail(const FarmRuntimeContext& context, int index, bool replay);
    void Execute(const FarmRuntimeContext& context, farmui::Request request, bool allowTransition = true);
    bool DeferScreenTransition(const FarmRuntimeContext& context, farmui::Request request);
    void EnterObservation(const FarmRuntimeContext& context);
    void EnterTerrain(const FarmRuntimeContext& context);
    void LeaveTerrain(const FarmRuntimeContext& context);
    bool UpdateTerrain(const FarmRuntimeContext& context, const Input& input, bool click);
    bool UpdateFieldClick(const FarmRuntimeContext& context, const Input& input, farm::FieldClick press, bool click, farmui::Request candidate);
    bool UpdateHotbarFocus(const FarmRuntimeContext& context, const Input& input, farm::FieldToolWheel previousWheel, bool insideViewport);
    bool UpdateRightTap(const FarmRuntimeContext& context, const Input& input, farm::FieldRightTap gesture, bool insideViewport);
    void UpdateToolWheel(const FarmRuntimeContext& context, const Input& input, farm::FieldToolWheel previous, bool insideViewport);
    bool FrameFarm(const FarmRuntimeContext& context, bool followPlayer = false, bool briefing = false);
    void BeginFarmStart(const FarmRuntimeContext& context);
    void ApplyStartCamera(const FarmRuntimeContext& context);
    bool UpdateObservation(const FarmRuntimeContext& context, const Input& input, bool click);
    farmui::RuntimeUI ui_;
    FarmPlayFlow flow_;
    FarmStartPresentationSystem startPresentation_;
    FarmArrivalAudioSystem arrivalAudio_;
    FarmSeedShopSystem shop_;
    FarmSaleReviewSystem saleReview_;
    bool harvestDetails_ = false;
    FarmHarvestDisplaySystem harvestDisplay_;
    FarmContestPresentationSystem contestPresentation_;
    FarmContestAudioSystem contestAudio_;
    FarmPestAudioSystem pestAudio_;
    FarmContestOutcomeSystem contestOutcome_;
    int dueContestDay_ = 0; // Snapshot of the day system, refreshed on update and acknowledgement.
    FarmExhibitionSystem exhibition_;
    void InitializeEdition();
    bool mainGameOnly_ = false;
    title::TitleAudioSettingsSystem menuAudio_;
    bool menuDetails_ = false;
    bool seasonRetryFailed_ = false;
    farmui::View view_;
    Vector2 pointer_{-1, -1};
    farmui::Action pending_ = farmui::Action::None;
    farmui::Request pendingSubmission_{};
    FarmDevelopmentSystem::View pendingExpansion_{};
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
    int hotbarSlot_ = -1; // A candidate only; tool/crop selection changes on acceptance.
    bool open_ = false;
    bool paused_ = false;
    bool ready_ = false;
    bool frameCaptured_ = false;
    bool fieldInputConsumed_ = false; // Field input is exclusive but does not pause the clock.
    farm::FieldClick fieldPress_;
    farm::FieldWheelNotice wheelNotice_;
    farm::FieldRightTap rightTap_;
    farm::FieldToolWheel fieldWheel_;
    bool wheelCaptured_ = false;
    bool observation_ = false;
    bool terrain_ = false;
    bool pauseBeforeObservation_ = false;
    int pickingSlot_ = -1;
};
