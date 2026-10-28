#include "farm/system/FarmRuntimeController.h"
#include "farm/ui/FarmHotbarView.h"
#include "farm/system/FarmRuntimeContext.h"
#include "editor/GamePlayEditorBridge.h"
#include "farm/render/FarmArrivalRenderer.h"
#include "farm/render/FarmSeedShopRenderer.h"
#include "farm/system/FarmInputSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmContestDaySystem.h"
#include "farm/system/FarmVisualSystem.h"
#include "level/LevelGameplaySystem.h"
#include "3d/Camera.h"
#include "scene/SceneManager.h"
#include "io/Input.h"
#include "farm/system/FarmLayoutSystem.h"
#include "farm/ui/FarmRecordDialog.h"
#include "farm/ui/FarmObservationView.h"
#include "farm/ui/FarmPlayFlowView.h"
#include "farm/ui/FarmFieldActionView.h"
#include "farm/ui/FarmCameraView.h"
#include "farm/ui/FarmQualityView.h"
#include "farm/ui/FarmContestDetailView.h"
#include "title/TitleAudioSettingsSystem.h"
#include "farm/ui/FarmContestOutcomeView.h"
#include "farm/ui/FarmExhibitionView.h"
#include "farm/ui/FarmTerrainView.h"
#include "farm/ui/FarmSeedShopView.h"
#include "farm/ui/FarmHarvestDisplayView.h"
#include "farm/system/FarmOverviewCamera.h"
#include "farm/system/FarmSoilSystem.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "base/WinApp.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <utility>
#include <Windows.h>

namespace {
using farmui::Action;
using farmui::Label;
using editor::GamePlayEditorCommandType;
constexpr int kSoilCarePage = 5;
constexpr int kQualityCurrentPage = 6;
constexpr int kQualityHarvestPage = 7;
constexpr int kHarvestInventoryPage = 8;
constexpr int kContestPreviewPage = 9;
constexpr int kContestEntryPage = 10;
constexpr int kContestResultsPage = 11;
constexpr int kHarvestDisplayPage = 12;
constexpr int kStageGuidePage = 13;
constexpr int kContestOutcomePage = 14;
template<class... Args> std::string Numbers(const char* format, Args... args) {
    char buffer[96]{};
    std::snprintf(buffer, sizeof(buffer), format, args...);
    return buffer;
}
}

void FarmRuntimeController::OpenContestDetail(const FarmRuntimeContext& s, int index, bool replay) {
    const auto& results = s.farmEconomySystem.GetContestResults();
    if (index < 0 || index >= static_cast<int>(results.size())) return;
    const auto crop = results[index].harvest.quality.crop;
    const auto comparison = FarmContestComparisonSystem::Build(index, s.farmDateSystem.GetDay(), results,
        s.farmProgressionSystem.StagePolicy().enabled, s.farmToolActionSystem.MaximumHarvestSize(crop));
    if (!comparison.valid) { status_ = Label::ContestInvalid; return; }
    contestPresentation_.Begin(comparison, replay);
    title::TitleAudioSettingsSystem settings;
    settings.Initialize();
    contestAudio_.Begin(settings.GetContestVolume());
    shop_.Close(); open_ = true; page_ = kContestResultsPage; focusedItem_ = -1;
}

void FarmRuntimeController::BuildView(const FarmRuntimeContext& s) {
    view_ = {};
    contestNotice_ = false;
    if (exhibition_.Active()) {
        farmui::BuildExhibitionView(view_, exhibition_);
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    contestOutcome_.Observe(s.farmDateSystem.GetDay(), s.farmEconomySystem.GetContestResults(),
        s.farmProgressionSystem.IsContestSeason(), s.farmProgressionSystem.StagePolicy().enabled);
    const auto previousPhase = flow_.GetPhase();
    flow_.Observe(s.farmProgressionSystem.IsCleared());
    if (s.farmProgressionSystem.IsContestSeason() && previousPhase != FarmPlayFlow::Phase::Result &&
        flow_.GetPhase() == FarmPlayFlow::Phase::Result) {
        open_ = false; pending_ = Action::None; observation_ = false; terrain_ = false; pickingSlot_ = -1;
    }
    if (contestPresentation_.Active()) {
        open_ = true;
        farmui::BuildContestDetailView(view_, contestPresentation_.GetFrame());
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    const auto& outcome = contestOutcome_.GetFrame();
    const bool finalPage = (!open_ && flow_.GetPhase() == FarmPlayFlow::Phase::Result) ||
        (open_ && page_ == kContestOutcomePage);
    if (outcome.season.valid && outcome.season.finalized &&
        ((finalPage && pending_ == Action::None) || pending_ == Action::SeasonRetry)) {
        shop_.Close();
        farmui::BuildContestOutcomeView(view_, outcome, s.farmEconomySystem.GetContestResults(),
            pending_ == Action::SeasonRetry, seasonRetryFailed_);
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    if (outcome.missedDay && flow_.GetPhase() == FarmPlayFlow::Phase::Playing && !open_ &&
        pending_ == Action::None && !s.farmContestDaySystem.PendingDay()) {
        shop_.Close(); contestNotice_ = true;
        farmui::BuildContestMissedView(view_, outcome);
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    if (flow_.BlocksSimulation() && !open_ && pending_ == Action::None) {
        shop_.Close();
        if (flow_.GetPhase() == FarmPlayFlow::Phase::Starting) {
            farmui::BuildFarmStartView(view_, startPresentation_.GetFrame(), s.host.BuildFarmHUDViewData());
            return;
        }
        if (flow_.GetPhase() == FarmPlayFlow::Phase::Briefing) FrameFarm(s, false, true);
        if (s.farmProgressionSystem.IsContestSeason() && flow_.GetPhase()==FarmPlayFlow::Phase::Result)
            farmui::BuildSeasonEndView(view_, s.farmEconomySystem.GetContestResults(),
                FarmContestSeasonSystem::Evaluate(s.farmDateSystem.GetDay(),s.farmEconomySystem.GetContestResults()),
                FarmContestGoalSystem::Evaluate(s.farmDateSystem.GetDay(), s.farmEconomySystem.GetContestResults(),
                    s.farmProgressionSystem.StagePolicy().enabled));
        else farmui::BuildPlayFlowView(view_, flow_.GetPhase(), s.host.BuildFarmHUDViewData(), s.farmProgressionSystem.StagePolicy().enabled);
        if (flow_.GetPhase() == FarmPlayFlow::Phase::Briefing)
            view_.Add(Label::ExhibitionEntry, {40,428,440,52}, {Action::ExhibitionOpen});
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    contestNotice_ = s.farmContestDaySystem.PendingDay() > 0 &&
        !s.farmProgressionSystem.IsCleared() && pending_ == Action::None;
    if (contestNotice_) {
        shop_.Close();
        farmui::BuildContestDayNoticeView(view_, s.farmContestDaySystem.PendingDay());
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    if (shop_.IsOpen()) {
        if (s.farmProgressionSystem.IsCleared() || s.farmIrrigationPreviewSystem.IsActive()) shop_.Close();
        else { farmui::BuildSeedShopView(view_, shop_, s.farmEconomySystem); return; }
    }
    view_.modal = open_;
    if (!open_) {
        if (terrain_) {
            const auto& preview = s.farmIrrigationPreviewSystem;
            const int index = s.farmGrid.GetSelectedIndex();
            const auto* tile = s.farmGrid.GetTile(index);
            const bool unlocked = !s.farmProgressionSystem.IsCleared();
            farmui::TerrainViewState state;
            state.preview = preview.IsActive();
            state.canConfirm = preview.CanConfirm(s.farmGrid);
            state.canRaise = unlocked && tile && tile->heightLevel < s.farmToolActionSystem.MaximumAllowedHeight();
            state.canLower = unlocked && tile && tile->heightLevel > FarmToolActionSystem::kMinimumHeightLevel;
            state.canCanal = unlocked && s.farmToolActionSystem.CanToggleCanal(s.farmGrid, index);
            state.canSource = unlocked && s.farmToolActionSystem.CanToggleWaterSource(s.farmGrid, index);
            state.canPath = unlocked && tile;
            state.canUndo = unlocked && s.farmToolActionSystem.GetHistory().CanUndo();
            state.canRedo = unlocked && s.farmToolActionSystem.GetHistory().CanRedo();
            state.canCompost = unlocked && tile && FarmSoilSystem::CanCompost(*tile);
            state.canSetIrrigation = unlocked && s.farmToolActionSystem.CanSetIrrigation(s.farmGrid, index);
            state.irrigationEnabled = tile && tile->irrigationEnabled;
            state.status = state.preview ? (brush_ == 1 ? Label::PathActive : brush_ == 2 ? Label::RemoveActive : Label::Preview)
                : (status_ == Label::Failure ? status_ : Label::PauseCapture);
            state.pathIssue = preview.GetPathIssue();
            if (state.preview && preview.GetOperation() == farm::FarmIrrigationPreviewOperation::RaiseTerrain)
                state.status = Label::RaiseBrush;
            if (state.preview && preview.GetOperation() == farm::FarmIrrigationPreviewOperation::LowerTerrain)
                state.status = Label::LowerBrush;
            state.blockedTileIndex = preview.GetBlockedTileIndex();
            state.changeCount = preview.GetChangeCount();
            // Read candidate values without publishing them to the authoritative grid.
            int displayedIndex = index;
            if (const auto* candidate = preview.GetPreviewGrid()) {
                displayedIndex = candidate->GetSelectedIndex();
                tile = candidate->GetTile(displayedIndex);
            }
            state.tileValues = tile ? Numbers("%d / H%d / %.0f / %.0f", displayedIndex, tile->heightLevel,
                tile->moisture * 100, tile->growth * 100) : "--";
            farmui::BuildTerrainView(view_, state);
            return;
        }
        if (observation_) {
            const auto comparison = s.farmGrowthComparisonSystem.GetView(s.farmGrid);
            if (lastComparisonStatus_ == FarmComparisonStatus::Running && comparison.status != FarmComparisonStatus::Running) paused_ = true;
            lastComparisonStatus_ = comparison.status;
            const bool valid = comparison.status != FarmComparisonStatus::Invalidated;
            const std::array<bool, 2> jumps = {valid && s.farmGrid.GetTile(comparison.rows[0].tileIndex), valid && s.farmGrid.GetTile(comparison.rows[1].tileIndex)};
            farmui::BuildObservationView(view_, comparison, jumps, pickingSlot_, s.farmGrid.GetSelectedIndex(),
                paused_, s.farmDateSystem.GetTimeScale(), !s.farmProgressionSystem.IsCleared() && !s.farmIrrigationPreviewSystem.IsActive());
            return;
        }
        const auto hud = s.host.BuildFarmHUDViewData();
        view_.compactHUD = true;
        view_.selectedTile = hud.selectedTileValid;
        view_.goalVisible = !hud.freeFarming;
        view_.feedback = hud.feedback != FarmHUDFeedback::None;
        view_.IconButton(Label::Menu, farmui::HUDIcon::Menu, farmui::HUDLayout::UtilitySlot(5), {Action::Menu});
        if (s.farmIrrigationPreviewSystem.IsActive()) {
            view_.Add(Label::Confirm, {1040, 544, 216, 44}, {Action::Confirm}, s.farmIrrigationPreviewSystem.CanConfirm(s.farmGrid));
            view_.Add(Label::Cancel, {1040, 596, 216, 44}, {Action::Cancel});
        } else if (!s.farmCropSelectionSystem.IsOpen()) {
            farmui::BuildHotbarView(view_, hud, !s.farmProgressionSystem.IsCleared());
            farmui::BuildPlayQuickView(view_, paused_, !s.farmProgressionSystem.IsCleared(),
                !s.farmProgressionSystem.IsCleared(), s.farmDateSystem.GetTimeScale());
            const auto* reserved = s.farmEconomySystem.GetContestReservation();
            const auto* tile = s.farmGrid.GetSelectedTile();
            const int index = s.farmGrid.GetSelectedIndex();
            const auto forecast = tile ? s.farmGrowthSystem.Evaluate(*tile,
                s.farmCropSelectionSystem.GetSelectedCrop(), s.farmDateSystem.GetTimeScale(),
                s.farmIrrigationSystem.GetAvailableIrrigationStrength(s.farmGrid, index)) : FarmGrowthForecast{};
            farmui::BuildWaterGuidanceView(view_, FarmGrowthSystem::AnalyzeWater(tile, forecast,
                s.farmIrrigationSystem.GetWaterStatus(s.farmGrid, index)), !s.farmProgressionSystem.IsCleared(),
                s.farmToolActionSystem.CanSetIrrigation(s.farmGrid, index));
            farmui::BuildPlayCropStatusView(view_, hud.feedback != FarmHUDFeedback::None,
                s.farmEconomySystem.GetProtectedCropCount(),
                reserved ? reserved->quality.crop : farm::CropType::None, nullptr);
            farmui::BuildCameraView(view_, s.host.IsFollowingPlayer(), s.levelGameplay.HasPlayer());
            view_.IconButton(Label::ShopTitle, farmui::HUDIcon::Shop, farmui::HUDLayout::UtilitySlot(0),
                {Action::OpenSeedShop}, !s.farmProgressionSystem.IsCleared());
            view_.IconButton(Label::HarvestDisplay, farmui::HUDIcon::Inventory, farmui::HUDLayout::UtilitySlot(1),
                {Action::HarvestDisplay}, !s.farmProgressionSystem.IsCleared());
            const auto evaluation = s.farmToolActionSystem.EvaluateTool(s.farmGrid,
                s.farmToolSystem.GetCurrentTool(), s.farmCropSelectionSystem.GetSelectedCrop(), &s.farmEconomySystem);
            farmui::BuildFieldActionView(view_, evaluation, paused_, s.farmProgressionSystem.IsCleared(),
                hud.nextAction, hud.contestSeason);
        }
        return;
    }
    if (page_ == kHarvestDisplayPage && pending_ == Action::None) {
        farmui::BuildHarvestDisplayView(view_, harvestDisplay_.Observe(s.farmEconomySystem, s.farmDateSystem.GetDay()),
            !s.farmProgressionSystem.IsCleared() && !s.farmIrrigationPreviewSystem.IsActive() && !s.host.IsTimelineScrubbing());
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    constexpr Label tabs[] = {Label::Farm, Label::Terrain, Label::Records, Label::Observe, Label::Settings};
    for (int i = 0; i < 5; ++i) view_.Add(tabs[i], {150.0f + i * 196, 52, 188, 48}, {Action::Tab, i}, pending_ == Action::None,
        page_ == i || ((page_ == kSoilCarePage || page_ == kQualityCurrentPage || page_ == kQualityHarvestPage || page_ == kHarvestInventoryPage || page_ == kContestPreviewPage || page_ == kContestEntryPage || page_ == kContestResultsPage) && i == 0));
    view_.Add(Label::Resume, {916, 628, 214, 44}, {Action::Close});
    view_.Add(status_, {166, 628, 492, 42});
    const bool showOutcomeLink = outcome.season.finalized && s.farmProgressionSystem.IsContestSeason() && pending_ == Action::None;
    view_.Add(showOutcomeLink ? Label::OutcomeBack : Label::ReturnTitle, {678, 628, 224, 44},
        {showOutcomeLink ? Action::ContestOutcome : Action::ReturnTitle}, pending_ == Action::None);
    if (pending_ != Action::None) {
        view_.Add(pending_ == Action::ChangePlayMode ? Label::ModeConfirm :
            pending_ == Action::SubmitContest ? Label::ContestSubmitConfirm :
            pending_ == Action::ReturnTitle ? Label::ReturnTitleConfirm : Label::ConfirmDestructive, {256, 250, 760, 54});
        view_.Add(pending_ == Action::ChangePlayMode ? Label::ModeAccept :
            pending_ == Action::SubmitContest ? Label::ContestSubmitAccept :
            pending_ == Action::ReturnTitle ? Label::ReturnTitle : Label::Accept, {256, 342, 360, 56}, {Action::Accept});
        view_.Add(Label::Cancel, {648, 342, 360, 56}, {Action::Cancel});
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
        return;
    }
    int row = 0;
    const auto button = [&](Label label, Action action, int argument = 0, bool enabled = true, bool selected = false) {
        const int i = row++;
        view_.Add(label, {170.0f + (i % 2) * 476, 130.0f + (i / 2) * 57, 450, 48}, {action, argument}, enabled, selected);
    };
    const auto hud = s.host.BuildFarmHUDViewData();
    const bool unlocked = !s.farmProgressionSystem.IsCleared() && !s.farmIrrigationPreviewSystem.IsActive();
    const auto* tile = s.farmGrid.GetSelectedTile();
    const auto crop = s.farmCropSelectionSystem.GetSelectedCrop();
    if (page_ == 0) {
        button(Label::Hoe, Action::Tool, 0, unlocked, hud.currentToolIndex == 0);
        button(Label::Water, Action::Tool, 1, unlocked, hud.currentToolIndex == 1);
        button(Label::Seed, Action::Tool, 2, unlocked, hud.currentToolIndex == 2);
        button(Label::Harvest, Action::Tool, 3, unlocked, hud.currentToolIndex == 3);
        button(Label::Tomato, Action::Crop, 2, unlocked, crop == farm::CropType::Tomato);
        button(Label::Carrot, Action::Crop, 1, unlocked, crop == farm::CropType::Carrot);
        button(Label::ShopTitle, Action::OpenSeedShop, 0, unlocked);
        button(Label::Sell, Action::Sell, 0, unlocked && s.farmEconomySystem.PreviewSale(crop).Succeeded());
        button(Label::SellAll, Action::SellAll, 0, unlocked && s.farmEconomySystem.PreviewSale().Succeeded());
        button(Label::Pumpkin, Action::Crop, 3, unlocked, crop == farm::CropType::Pumpkin);
        button(Label::SoilCare, Action::SoilCare);
        button(Label::QualityOpen, Action::Quality);
        view_.Value(Label::Money, 482, Numbers("%dG / %dG", hud.money, hud.seedPrice));
        view_.Value(Label::Inventory, 528, Numbers("%d / %d / %dG", hud.seedCount, s.farmEconomySystem.GetCropCount(crop), s.farmEconomySystem.PreviewSale(crop).earnedMoney));
        view_.Add(Label::HarvestDisplay, {176,574,450,42}, {Action::HarvestDisplay});
        view_.Add(Label::StageGuide, {646,574,450,42}, {Action::StageGuide});
    } else if (page_ == 1) {
        button(Label::Raise, Action::Raise, 0, unlocked && tile && tile->heightLevel < s.farmToolActionSystem.MaximumAllowedHeight());
        button(Label::Lower, Action::Lower, 0, unlocked && tile && tile->heightLevel > FarmToolActionSystem::kMinimumHeightLevel);
        button(Label::Canal, Action::Canal, 0, unlocked && s.farmToolActionSystem.CanToggleCanal(s.farmGrid, hud.selectedTileIndex));
        button(Label::Source, Action::Source, 0, unlocked && s.farmToolActionSystem.CanToggleWaterSource(s.farmGrid, hud.selectedTileIndex));
        button(Label::Path, Action::Path, 0, unlocked && tile);
        button(Label::RemovePath, Action::RemovePath, 0, unlocked && tile);
        button(Label::Undo, Action::Undo, 0, unlocked && s.farmToolActionSystem.GetHistory().CanUndo());
        button(Label::Redo, Action::Redo, 0, unlocked && s.farmToolActionSystem.GetHistory().CanRedo());
        button(Label::Confirm, Action::Confirm, 0, s.farmIrrigationPreviewSystem.CanConfirm(s.farmGrid));
        button(Label::Cancel, Action::Cancel, 0, s.farmIrrigationPreviewSystem.IsActive());
        button(Label::Overview, Action::TerrainField, 0, !s.farmProgressionSystem.IsCleared());
        view_.Value(Label::Tile, 478, Numbers("%d / H%d / %d / %d", hud.selectedTileIndex, hud.selectedTileHeight, hud.selectedTileMoisturePercent, hud.selectedTileGrowthPercent));
        view_.Value(Label::WaterStats, 524, Numbers("%d / %d", s.farmIrrigationSystem.GetSuppliedCanalCount(), s.farmIrrigationSystem.GetIrrigationRangeTileCount()));
        view_.Metric(Label::StageHeight, {176,574,450,38}, 240, "H" + std::to_string(s.farmToolActionSystem.MaximumAllowedHeight()));
        view_.Add(Label::StageGuide, {646,574,450,38}, {Action::StageGuide});
    } else if (page_ == 2) {
        const auto& docs = s.farmDocumentSystem.GetDocuments();
        documentIndex_ = std::clamp(documentIndex_, 0, (std::max)(0, static_cast<int>(docs.size()) - 1));
        button(Label::Save, Action::Save, 0, !s.farmIrrigationPreviewSystem.IsActive() && s.farmDocumentSystem.FileExists());
        button(Label::SaveCopy, Action::SaveCopy, 0, !s.farmIrrigationPreviewSystem.IsActive());
        button(Label::Previous, Action::Previous, 0, documentIndex_ > 0);
        button(Label::Next, Action::Next, 0, documentIndex_ + 1 < static_cast<int>(docs.size()));
        button(Label::Load, Action::Load, 0, !docs.empty());
        button(Label::Restart, Action::Restart);
        button(Label::LayoutLibrary, Action::LayoutLibrary, 0, !s.farmIrrigationPreviewSystem.IsActive());
        view_.Value(Label::Record, 382, Numbers("%d / %zu", docs.empty() ? 0 : documentIndex_ + 1, docs.size()));
        view_.recordNames = {s.farmDocumentSystem.GetDisplayName(), docs.empty() ? "記録なし" : docs[documentIndex_].displayName};
        view_.Add(docs.empty() ? Label::NoSaves : (s.farmDocumentSystem.IsDirty() ? Label::Dirty : Label::Clean), {176, 554, 900, 40});
    } else if (page_ == 3) {
        const auto comparison = s.farmGrowthComparisonSystem.GetView(s.farmGrid);
        const bool running = comparison.status == FarmComparisonStatus::Running;
        button(Label::PinA, Action::PinA, 0, unlocked && tile && !running);
        button(Label::PinB, Action::PinB, 0, unlocked && tile && !running);
        button(Label::Start, Action::Start, 0, unlocked && !running && comparison.startIssue == FarmComparisonIssue::None);
        button(Label::Stop, Action::Stop, 0, comparison.status == FarmComparisonStatus::Running);
        button(Label::Clear, Action::Clear);
        button(Label::ObserveField, Action::ObserveField, 0, unlocked);
        view_.Value(Label::Compare, 330, Numbers("%d / %d / %.1f", comparison.rows[0].tileIndex, comparison.rows[1].tileIndex, comparison.elapsedSeconds));
        view_.Add(comparison.status == FarmComparisonStatus::Running ? Label::Running : Label::NotRunning, {176, 374, 500, 38});
        if (tile && farm::IsPlantableCrop(tile->crop)) {
            const auto quality = s.farmToolActionSystem.EvaluateHarvestQuality(*tile);
            view_.Value(Label::Quality, 418, Numbers("%.0f / %.0f / %.0f", quality.maturity * 100, quality.waterBalance * 100, quality.terrainFit * 100));
            view_.Value(Label::Score, 462, Numbers("%d / %dG", quality.score, quality.salePrice));
        } else view_.Add(Label::NoCrop, {176, 418, 850, 38});
        view_.Value(Label::Status, 506, Numbers("A %.0f/%.0f B %.0f/%.0f", comparison.rows[0].current.moisture * 100, comparison.rows[0].current.growth * 100, comparison.rows[1].current.moisture * 100, comparison.rows[1].current.growth * 100));
        if (comparison.startIssue != FarmComparisonIssue::None) view_.Add(farmui::ComparisonIssueLabel(comparison.startIssue), {176, 550, 910, 38});
        if (tile && farm::IsPlantableCrop(tile->crop)) {
            const auto quality = s.farmToolActionSystem.EvaluateHarvestQuality(*tile);
            view_.Value(Label::NutrientQuality, 590, Numbers("%.0f / 100", quality.nutrientBalance * 100));
        }
    } else if (page_ == kStageGuidePage) {
        farmui::BuildStageGuideView(view_, s.farmStageAccessSystem.Get());
    } else if (page_ == kContestPreviewPage) {
        farmui::BuildContestJudgeView(view_, FarmContestJudgeSystem::Evaluate(s.farmEconomySystem.GetContestReservation()));
    } else if (page_ == kContestEntryPage) {
        farmui::BuildContestEntryView(view_, FarmContestEntrySystem::Evaluate(s.farmDateSystem.GetDay(),
            s.farmEconomySystem.GetContestReservation()),
            FarmContestSubmissionSystem::Evaluate(s.farmEconomySystem,s.farmDateSystem.GetDay()),
            {Action::SubmitContest,s.farmEconomySystem.GetContestReservationId(),s.farmEconomySystem.GetInventoryGeneration()});
    } else if (page_ == kContestResultsPage) {
        farmui::BuildContestResultsView(view_,s.farmEconomySystem.GetContestResults(),
            FarmContestSeasonSystem::Evaluate(s.farmDateSystem.GetDay(),s.farmEconomySystem.GetContestResults()),
            FarmContestGoalSystem::Evaluate(s.farmDateSystem.GetDay(), s.farmEconomySystem.GetContestResults(),
                s.farmProgressionSystem.StagePolicy().enabled));
    } else if (page_ == kHarvestInventoryPage) {
        const auto& economy = s.farmEconomySystem;
        farmui::HarvestInventoryViewState state;
        state.records = static_cast<int>(economy.GetHarvestRecordCount());
        state.capacity = static_cast<int>(FarmEconomySystem::kMaxHarvestRecords);
        state.unknownCount = economy.GetUnrecordedCropCount();
        state.protectedCount = economy.GetProtectedCropCount();
        state.inventoryGeneration = economy.GetInventoryGeneration();
        state.canChangeProtection = !s.farmIrrigationPreviewSystem.IsActive() && !s.host.IsTimelineScrubbing();
        state.contestReservationId = economy.GetContestReservationId();
        if (const auto* reserved = economy.GetContestReservation()) state.reservedCrop = reserved->quality.crop;
        state.pages = (std::max)(1, (state.records + state.kRows - 1) / state.kRows);
        state.page = harvestPage_ = std::clamp(harvestPage_, 0, state.pages - 1);
        for (int i = 0; i < state.kRows; ++i) {
            if (const auto* record = economy.GetHarvestRecord(state.page * state.kRows + i))
                state.rows[state.rowCount++] = {record->quality, record->quantity, record->id, record->saleProtected,
                    FarmEconomySystem::CanReserveForContest(*record), record->harvestedDay};
        }
        farmui::BuildHarvestInventoryView(view_, state);
    } else if (page_ == kQualityCurrentPage || page_ == kQualityHarvestPage) {
        const auto quality = page_ == kQualityHarvestPage ? s.farmEconomySystem.GetLastHarvestQuality()
            : tile ? s.farmToolActionSystem.EvaluateHarvestQuality(*tile) : FarmCropQualityResult{};
        farmui::BuildQualityView(view_, quality, FarmCropQualitySystem::Analyze(quality),
            hud.selectedTileIndex, page_ == kQualityHarvestPage);
    } else if (page_ == kSoilCarePage) {
        button(Label::Compost, Action::Compost, 0, unlocked && tile && FarmSoilSystem::CanCompost(*tile));
        button(Label::QualityOpen, Action::Quality);
        const auto soilCrop = tile && farm::IsPlantableCrop(tile->crop) ? tile->crop : crop;
        const auto* profile = FarmSoilSystem::Profile(soilCrop);
        const bool soil = tile && tile->feature == farm::FarmTileFeature::None;
        view_.Add(Label::SoilSelectedCrop, {176, 198, 900, 38});
        view_.Add(farmui::CropLabel(soilCrop), {176, 238, 240, 38});
        view_.Metric(Label::Selected, {716, 238, 350, 38}, 120, Numbers("#%d", hud.selectedTileIndex));
        view_.Value(Label::SoilNutrients, 286, soil ? Numbers("%.0f / 100", tile->soilNutrients * 100) : "--");
        view_.Value(Label::NutrientTarget, 332, profile ? Numbers("%.0f / 100", profile->target * 100) : "--");
        view_.Value(Label::NutrientUse, 378, profile ? Numbers("%.0f", profile->consumptionPerGrowth * 100) : "--");
        const auto quality = tile ? s.farmToolActionSystem.EvaluateHarvestQuality(*tile) : FarmCropQualityResult{};
        view_.Value(Label::NutrientQuality, 424, quality.IsValid() ? Numbers("%.0f / 100", quality.nutrientBalance * 100) : "--");
        const Label advice = !soil || tile->state == farm::FarmTileState::Empty ? Label::SoilTill
            : farm::IsPlantableCrop(tile->crop) ? Label::SoilGrowing
            : tile->soilNutrients >= 1.0f ? Label::SoilFull : Label::SoilReady;
        view_.Add(advice, {176, 484, 920, 38});
        view_.Add(Label::SoilHelp, {176, 534, 920, 38});
        farm::FarmTile preview;
        if (soil) preview = *tile;
        // Read-only species guidance is also available before preparing the soil.
        preview.state = farm::FarmTileState::Tilled;
        preview.feature = farm::FarmTileFeature::None;
        preview.crop = soilCrop;
        const auto water = s.farmGrowthSystem.Evaluate(preview, soilCrop);
        view_.Value(Label::OptimalWater, 580, Numbers("%.0f - %.0f %%",
            water.goodMoistureMinimum * 100, water.goodMoistureMaximum * 100));
    } else {
        button(paused_ ? Label::Play : Label::Pause, Action::Pause);
        button(Label::Speed1, Action::Speed, 1, unlocked, !paused_ && hud.timeScale == 1);
        button(Label::Speed2, Action::Speed, 2, unlocked, !paused_ && hud.timeScale == 2);
        button(Label::Speed4, Action::Speed, 4, unlocked, !paused_ && hud.timeScale == 4);
        button(Label::Follow, Action::Follow, 0, true, s.host.IsFollowingPlayer());
        button(Label::Overview, Action::Overview, 0, true, !s.host.IsFollowingPlayer());
        button(Label::Exit, Action::Exit);
        button(Label::SeasonMode, Action::ChangePlayMode, 1, true, hud.contestSeason);
        button(Label::TrialMode, Action::ChangePlayMode, 0, true, !hud.contestSeason && !hud.freeFarming);
        button(Label::FreeMode, Action::ChangePlayMode, 2, true, hud.freeFarming);
        view_.Value(Label::Day, 440, Numbers("%d / x%.0f", hud.day, hud.timeScale));
        view_.Add(Label::PauseCapture, {176, 492, 900, 40});
    }
    ui_.PrepareNames(view_);
    if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) view_.items[focusedItem_].focused = true;
}

bool FarmRuntimeController::DeferScreenTransition(const FarmRuntimeContext& s, farmui::Request request) {
    if (pendingScreenTransition_.action != Action::None) return true;
    scene::RipplePoint origin{};
    if (pointerTriggered_ && std::isfinite(pointer_.x) && std::isfinite(pointer_.y) &&
        pointer_.x >= 0 && pointer_.x <= WinApp::kClientWidth && pointer_.y >= 0 && pointer_.y <= WinApp::kClientHeight)
        origin = {pointer_.x / static_cast<float>(WinApp::kClientWidth), pointer_.y / static_cast<float>(WinApp::kClientHeight)};
    auto sound = scene::TransitionSoundTheme::Ledger;
    auto style = scene::TransitionStyle::Ripple;
    switch (request.action) {
    case Action::OpenSeedShop:
        sound = scene::TransitionSoundTheme::SeedShop; style = scene::TransitionStyle::ShopEnter; break;
    case Action::HarvestDisplay: sound = scene::TransitionSoundTheme::HarvestDisplay; break;
    case Action::ShopClose:
        sound = scene::TransitionSoundTheme::ReturnToFarm; style = scene::TransitionStyle::ShopExit; break;
    case Action::Close: sound = scene::TransitionSoundTheme::ReturnToFarm; break;
    default: break;
    }
    const auto result = s.host.BeginScreenTransition(origin, sound, style);
    if (result == ScreenTransitionResult::Unavailable) return false;
    if (result == ScreenTransitionResult::Started) pendingScreenTransition_ = request;
    return true;
}

void FarmRuntimeController::CommitScreenTransition(const FarmRuntimeContext& s) {
    const auto request = pendingScreenTransition_;
    pendingScreenTransition_ = {};
    if (request.action == Action::None) return;
    Execute(s, request, false);
    BuildView(s);
}

void FarmRuntimeController::Execute(const FarmRuntimeContext& s, farmui::Request request, bool allowTransition) {
    using C = GamePlayEditorCommandType;
    const auto dispatch = [&](C type) {
        editor::GamePlayEditorCommand command{};
        command.type = type;
        command.farmGeneration = s.farmGrid.GetGeneration();
        command.farmTileIndex = type == C::ConfirmFarmIrrigationPreview ? s.farmIrrigationPreviewSystem.GetTileIndex() : s.farmGrid.GetSelectedIndex();
        return s.gamePlayEditorBridge.Execute(command);
    };
    bool success = false;
    switch (request.action) {
    case Action::ExhibitionOpen:
        exhibition_.Open(); focusedItem_ = -1; return;
    case Action::ContestReveal: contestPresentation_.Reveal(); focusedItem_ = -1; return;
    case Action::ContestTab:
        if (contestPresentation_.SelectPage(request.argument)) focusedItem_ = -1;
        return;
    case Action::ContestExplanation: contestPresentation_.ToggleExplanation(); return;
    case Action::ContestOutcomeReveal: contestOutcome_.Reveal(); focusedItem_ = -1; return;
    case Action::ContestDetailClose:
        contestPresentation_.Close();
        page_ = contestOutcome_.GetFrame().season.finalized ? kContestOutcomePage : kContestResultsPage;
        open_ = page_ != kContestOutcomePage || flow_.GetPhase() != FarmPlayFlow::Phase::Result;
        focusedItem_ = -1; return;
    case Action::ContestMissedContinue:
        if (contestOutcome_.AcknowledgeMissed(request.argument)) { contestNotice_ = false; focusedItem_ = -1; }
        return;
    case Action::ContestOutcome:
        if (contestOutcome_.GetFrame().season.finalized) { open_ = true; page_ = kContestOutcomePage; focusedItem_ = -1; }
        return;
    case Action::SeasonRetry:
        if (!s.farmProgressionSystem.IsContestSeason() || !contestOutcome_.GetFrame().season.finalized) return;
        pending_ = Action::SeasonRetry; page_ = kContestOutcomePage; open_ = true;
        seasonRetryFailed_ = false; focusedItem_ = -1; return;
    case Action::ContestDetail:
        if (pending_ == Action::None && ((open_ && (page_ == kContestResultsPage || page_ == kContestOutcomePage)) || flow_.GetPhase() == FarmPlayFlow::Phase::Result))
            OpenContestDetail(s, request.argument, true);
        return;
    case Action::ShopSelect:
        shop_.Select(request.argument); return;
    case Action::ShopQuantity:
        shop_.ChangeQuantity(request.argument); return;
    case Action::ShopReview:
        shop_.BeginConfirmation(s.farmEconomySystem); return;
    case Action::ShopCancel:
        shop_.CancelConfirmation(); return;
    case Action::ShopClose:
        if (!shop_.IsOpen()) return;
        if (allowTransition && DeferScreenTransition(s, request)) return;
        shop_.Close(); return;
    case Action::ShopConfirm: {
        if (s.farmProgressionSystem.IsCleared() || s.farmIrrigationPreviewSystem.IsActive()) { shop_.Close(); return; }
        const auto result = shop_.Confirm(s.farmEconomySystem);
        if (result.Succeeded()) {
            s.farmCropSelectionSystem.SetSelectedCrop(result.crop);
            s.farmDocumentSystem.MarkDirty();
            s.farmFeedbackSystem.ShowSeedPurchased(result.crop, result.purchasedCount, result.spentMoney);
        }
        return;
    }
    case Action::OpenSeedShop:
        if (shop_.IsOpen() || observation_ || terrain_ || pending_ != Action::None || flow_.BlocksSimulation() ||
            s.farmProgressionSystem.IsCleared() || s.farmIrrigationPreviewSystem.IsActive() ||
            s.farmCropSelectionSystem.IsOpen()) return;
        if (!s.farmSeedShopRenderer.IsReady()) { status_ = Label::Failure; return; }
        if (allowTransition && DeferScreenTransition(s, request)) return;
        shop_.Open(s.farmCropSelectionSystem.GetSelectedCrop()); open_ = false; focusedItem_ = -1;
        return;
    case Action::ContestDayReview:
    case Action::ContestDayPrepare:
    case Action::ContestDayResume: {
        editor::GamePlayEditorCommand command{};
        command.type = C::AcknowledgeContestDay;
        command.farmGeneration = s.farmGrid.GetGeneration();
        command.contestDay = request.argument;
        if (!s.gamePlayEditorBridge.Execute(command)) return;
        if (terrain_) LeaveTerrain(s);
        observation_ = false; pickingSlot_ = -1;
        s.farmCropSelectionSystem.Cancel();
        pending_ = Action::None;
        paused_ = request.action != Action::ContestDayResume;
        open_ = request.action == Action::ContestDayReview;
        if (open_) page_ = kContestEntryPage;
        status_ = paused_ ? Label::Paused : Label::Ready;
        focusedItem_ = -1;
        return;
    }
    case Action::TerrainField:
        if (!flow_.BlocksSimulation() && !s.farmProgressionSystem.IsCleared()) EnterTerrain(s);
        return;
    case Action::TerrainExit: LeaveTerrain(s); return;
    case Action::ApplyTool: {
        if (open_ || observation_ || terrain_ || flow_.BlocksSimulation() ||
            s.farmProgressionSystem.IsCleared() || s.farmIrrigationPreviewSystem.IsActive() ||
            s.farmCropSelectionSystem.IsOpen()) return;
        // Revalidate on execution; the displayed evaluation is not mutation authority.
        const auto result = s.farmToolActionSystem.ApplyToolDetailed(s.farmGrid,
            s.farmToolSystem.GetCurrentTool(), s.farmCropSelectionSystem.GetSelectedCrop(), s.farmEconomySystem, s.farmDateSystem.GetDay());
        if (result.Succeeded()) s.farmDocumentSystem.MarkDirty();
        s.host.RouteFarmToolFeedback(result);
        status_ = result.Succeeded() ? Label::Success : Label::Failure;
        return;
    }
    case Action::FlowContinue:
        if (flow_.GetPhase() == FarmPlayFlow::Phase::Briefing) BeginFarmStart(s);
        else flow_.Continue();
        open_ = false; observation_ = false; pickingSlot_ = -1; focusedItem_ = -1;
        return;
    case Action::FlowRecords: open_ = true; page_ = 2; focusedItem_ = -1; return;
    case Action::SoilCare: open_ = true; page_ = kSoilCarePage; focusedItem_ = -1; status_ = Label::SoilCare; return;
    case Action::Quality:
        if (request.argument < 0 || request.argument > 1) return;
        open_ = true; page_ = request.argument == 1 ? kQualityHarvestPage : kQualityCurrentPage;
        focusedItem_ = -1; status_ = Label::Paused; return;
    case Action::Menu: open_ = true; if (observation_) page_ = 3; else if (terrain_) page_ = 1; focusedItem_ = -1; s.farmCropSelectionSystem.Cancel(); return;
    case Action::ObserveField: EnterObservation(s); return;
    case Action::ObserveExit:
        observation_ = false; pickingSlot_ = -1; paused_ = pauseBeforeObservation_;
        if (!s.host.IsFollowingPlayer()) FrameFarm(s);
        return;
    case Action::PickSlot:
        if (request.argument == -1) { pickingSlot_ = -1; return; }
        if (request.argument < 0 || request.argument > 1 || s.farmGrowthComparisonSystem.GetView(s.farmGrid).status == FarmComparisonStatus::Running) return;
        EnterObservation(s); pickingSlot_ = request.argument; paused_ = true; return;
    case Action::JumpSlot: {
        if (request.argument < 0 || request.argument > 1) return;
        const auto comparison = s.farmGrowthComparisonSystem.GetView(s.farmGrid);
        const int index = comparison.rows[request.argument].tileIndex;
        if (comparison.status != FarmComparisonStatus::Invalidated && s.farmGrid.GetTile(index)) {
            s.farmGrid.SetSelectedIndex(index); pickingSlot_ = -1;
        }
        return;
    }
    case Action::Close:
        if (open_ && page_ == kHarvestDisplayPage && pending_ == Action::None &&
            allowTransition && DeferScreenTransition(s, request)) return;
        open_ = false; pending_ = Action::None; return;
    case Action::StageGuide:
        if (pending_ != Action::None) return;
        open_ = true; page_ = kStageGuidePage; focusedItem_ = -1; return;
    case Action::Tab:
        if (open_ && page_ == kHarvestDisplayPage && allowTransition && DeferScreenTransition(s, request)) return;
        page_ = std::clamp(request.argument, 0, 4); focusedItem_ = -1; status_ = Label::Ready;
        if (terrain_ && page_ != 1) LeaveTerrain(s);
        if (page_ < 3 && observation_) {
            observation_ = false; pickingSlot_ = -1; paused_ = pauseBeforeObservation_;
            if (!s.host.IsFollowingPlayer()) FrameFarm(s);
        }
        return;
    case Action::Cancel:
        if (pending_ != Action::None) {
            pending_ = Action::None; focusedItem_ = -1;
            if (page_ == kContestOutcomePage && flow_.GetPhase() == FarmPlayFlow::Phase::Result) open_ = false;
            return;
        }
        s.farmIrrigationPreviewSystem.Cancel(); brush_ = 0; success = true; break;
    case Action::Load:
    {
        const auto& docs = s.farmDocumentSystem.GetDocuments();
        std::vector<std::string> names;
        for (const auto& doc : docs) names.push_back(doc.displayName + "  /  " + doc.savedAt);
        const auto result = farmui::ShowRecordDialog(s.window->GetHwnd(),
            farmui::RecordDialogMode::ProgressPicker, names);
        if (result.action != farmui::RecordDialogAction::Load || result.selected < 0 || result.selected >= static_cast<int>(docs.size())) return;
        documentIndex_ = result.selected;
        pendingDocumentId_ = docs[documentIndex_].id;
        pending_ = Action::Load; return;
    }
    case Action::Restart:
        if (s.farmProgressionSystem.IsContestSeason() && contestOutcome_.GetFrame().season.finalized) {
            Execute(s, {Action::SeasonRetry}); return;
        }
        pending_ = request.action; open_ = true; return;
    case Action::ReturnTitle:
    case Action::Exit:
        pending_ = request.action; open_ = true; return;
    case Action::ChangePlayMode:
        if (!FarmProgressionSystem::ValidMode(static_cast<FarmProgressionMode>(request.argument))) return;
        pendingPlayMode_=request.argument; pending_=request.action; open_=true; return;
    case Action::LayoutLibrary: OpenLayoutLibrary(s); return;
    case Action::Accept: {
        const Action action = pending_; pending_ = Action::None;
        if (action == Action::None) return;
        if (action == Action::SeasonRetry) {
            if (!s.host.RetryCompletedSeason()) {
                seasonRetryFailed_ = true; pending_ = Action::SeasonRetry; focusedItem_ = -1; return;
            }
            contestPresentation_.Close(); contestOutcome_.Reset(); shop_.Close();
            s.farmGrowthComparisonSystem.Reset(); s.host.InitializeTimeline();
            flow_.Reset(); open_ = false; paused_ = false; observation_ = false; terrain_ = false;
            pickingSlot_ = -1; brush_ = 0; page_ = 0; focusedItem_ = -1;
            seasonRetryFailed_ = false; status_ = Label::Success;
            return;
        }
        if (action == Action::ReturnTitle) {
            flow_.RequestTitle();
            arrivalAudio_.Update({},false,true);
            return;
        }
        if (action == Action::ChangePlayMode) {
            editor::GamePlayEditorCommand command{};
            command.type=C::SetFarmProgressionMode; command.farmGeneration=s.farmGrid.GetGeneration();
            command.progressionMode=static_cast<FarmProgressionMode>(pendingPlayMode_);
            success=s.gamePlayEditorBridge.Execute(command);
            if (success) {
                flow_.Reset(); contestOutcome_.Reset(); open_=false; observation_=false; terrain_=false; pickingSlot_=-1; paused_=false;
                focusedItem_=-1; s.farmCropSelectionSystem.Cancel();
            }
            status_=success ? Label::Success : Label::Failure;
            return;
        }
        if (action == Action::SubmitContest) {
            editor::GamePlayEditorCommand command{};
            command.type=C::SubmitContestHarvest; command.farmGeneration=s.farmGrid.GetGeneration();
            command.harvestRecordId=pendingSubmission_.argument; command.inventoryGeneration=pendingSubmission_.inventoryGeneration;
            command.contestDay=pendingContestDay_;
            success=s.gamePlayEditorBridge.Execute(command);
            status_=success ? Label::ContestSubmitted : Label::Failure;
            if(success) {
                page_=kContestResultsPage;
                for (int i=0; i<static_cast<int>(FarmContestEntrySystem::kContestDays.size()); ++i)
                    if (FarmContestEntrySystem::kContestDays[i] == pendingContestDay_) OpenContestDetail(s,i,false);
            }
            pendingSubmission_={}; pendingContestDay_=0; focusedItem_=-1;
            return;
        }
        if (action == Action::Exit) { PostQuitMessage(0); return; }
        s.farmIrrigationPreviewSystem.Cancel(); s.farmCropSelectionSystem.Cancel(); brush_ = 0;
        if (action == Action::Restart) success = s.host.ResetFarmSession();
        else {
            editor::FarmDocumentCommand command{};
            command.type = editor::FarmDocumentCommandType::Load;
            command.documentId = pendingDocumentId_;
            success = s.gamePlayEditorBridge.Execute(command);
        }
        if (success) {
            contestOutcome_.Reset(); contestPresentation_.Close();
            s.farmGrowthComparisonSystem.Reset(); s.host.InitializeTimeline();
            flow_.Reset(); open_ = false; observation_ = false; terrain_ = false; pickingSlot_ = -1; paused_ = false;
            status_ = action == Action::Restart ? Label::Success : Label::Loaded;
        }
        else status_ = Label::Failure;
        return;
    }
    case Action::HarvestDisplay:
        if (pending_ != Action::None || s.farmIrrigationPreviewSystem.IsActive() || s.host.IsTimelineScrubbing() || !s.farmSeedShopRenderer.IsReady()) return;
        if (open_ && page_ == kHarvestDisplayPage) return;
        if (allowTransition && DeferScreenTransition(s, request)) return;
        open_ = true; page_ = kHarvestDisplayPage; harvestDisplay_.Reset(); focusedItem_ = -1; return;
    case Action::HarvestDisplaySelect:
        if (open_ && page_ == kHarvestDisplayPage && pending_ == Action::None)
            static_cast<void>(harvestDisplay_.Select(s.farmEconomySystem, request.argument, request.inventoryGeneration));
        return;
    case Action::HarvestDisplayPage:
        if (open_ && page_ == kHarvestDisplayPage && pending_ == Action::None) {
            harvestDisplay_.MovePage(s.farmEconomySystem, request.argument); focusedItem_ = -1;
        }
        return;
    case Action::HarvestInventory:
        if (open_ && page_ == kHarvestDisplayPage && allowTransition && DeferScreenTransition(s, request)) return;
        open_ = true; page_ = kHarvestInventoryPage; harvestPage_ = 0; focusedItem_ = -1; return;
    case Action::ContestPreview:
        if (!open_ || (page_ != kHarvestDisplayPage && page_ != kHarvestInventoryPage && page_ != kContestPreviewPage && page_ != kContestEntryPage && page_ != kContestResultsPage)) return;
        if (request.argument != 0 && request.argument != 1) return;
        if (page_ == kHarvestDisplayPage && allowTransition && DeferScreenTransition(s, request)) return;
        page_ = request.argument == 1 ? kContestEntryPage : kContestPreviewPage; focusedItem_ = -1; return;
    case Action::ContestResults:
        if(!open_ || page_!=kContestEntryPage) return;
        page_=kContestResultsPage; focusedItem_=-1; return;
    case Action::SubmitContest:
        if(!open_ || page_!=kContestEntryPage || pending_!=Action::None ||
            FarmContestSubmissionSystem::Evaluate(s.farmEconomySystem,s.farmDateSystem.GetDay())!=FarmContestSubmissionStatus::Ready) return;
        pending_=Action::SubmitContest; pendingSubmission_=request; pendingContestDay_=s.farmDateSystem.GetDay(); focusedItem_=-1; return;
    case Action::HarvestPage:
        if (page_ == kHarvestInventoryPage && (request.argument == -1 || request.argument == 1))
            harvestPage_ = std::clamp(harvestPage_ + request.argument, 0, static_cast<int>(FarmEconomySystem::kMaxHarvestRecords));
        return;
    case Action::ProtectHarvest:
    case Action::UnprotectHarvest: {
        if (!open_ || (page_ != kHarvestInventoryPage && page_ != kHarvestDisplayPage)) return;
        editor::GamePlayEditorCommand command{};
        command.type = C::SetHarvestProtection; command.farmGeneration = s.farmGrid.GetGeneration();
        command.harvestRecordId = request.argument; command.inventoryGeneration = request.inventoryGeneration;
        command.harvestProtected = request.action == Action::ProtectHarvest;
        status_ = s.gamePlayEditorBridge.Execute(command) ? Label::ProtectionCommitted : Label::Failure;
        return;
    }
    case Action::ReserveContestHarvest:
    case Action::CancelContestReservation: {
        if (!open_ || (page_ != kHarvestInventoryPage && page_ != kHarvestDisplayPage)) return;
        editor::GamePlayEditorCommand command{};
        command.type = request.action == Action::ReserveContestHarvest ? C::ReserveContestHarvest : C::CancelContestReservation;
        command.farmGeneration = s.farmGrid.GetGeneration();
        command.harvestRecordId = request.argument; command.inventoryGeneration = request.inventoryGeneration;
        status_ = s.gamePlayEditorBridge.Execute(command) ? Label::ContestChanged : Label::Failure;
        return;
    }
    case Action::Previous: --documentIndex_; return;
    case Action::Next: ++documentIndex_; return;
    case Action::Save:
    case Action::SaveCopy: {
        if (s.farmIrrigationPreviewSystem.IsActive()) break;
        editor::FarmDocumentCommand command{};
        command.type = request.action == Action::Save ? editor::FarmDocumentCommandType::Save : editor::FarmDocumentCommandType::SaveAs;
        if (request.action == Action::SaveCopy) {
            const auto result = farmui::ShowRecordDialog(s.window->GetHwnd(),
                farmui::RecordDialogMode::Name, {}, s.farmDocumentSystem.GetDisplayName());
            if (result.action != farmui::RecordDialogAction::SaveNew) return;
            command.displayName = result.name;
        }
        success = s.gamePlayEditorBridge.Execute(command);
        if (!success) farmui::RecordNotice(s.window->GetHwnd(), s.farmDocumentSystem.GetStatusMessage());
        status_ = success ? Label::Saved : Label::Failure; return;
    }
    case Action::Pause: paused_ = !paused_; if (!paused_) pickingSlot_ = -1; return;
    case Action::Follow: s.host.SetUsePlayerCamera(true); open_ = false; return;
    case Action::Overview: {
        if (FrameFarm(s)) open_ = false;
        else status_ = Label::Failure;
        return;
    }
    default: break;
    }
    if (request.action == Action::Confirm) { success = dispatch(C::ConfirmFarmIrrigationPreview); if (success) brush_ = 0; }
    else if (request.action == Action::Stop) { success = s.farmGrowthComparisonSystem.Stop(); if (success && observation_) paused_ = true; }
    else if (request.action == Action::Clear) { s.farmGrowthComparisonSystem.Reset(); pickingSlot_ = -1; success = true; }
    else if (s.farmProgressionSystem.IsCleared() || s.farmIrrigationPreviewSystem.IsActive()) {
        if (request.action != Action::Cancel) { status_ = Label::Failure; return; }
    } else {
        switch (request.action) {
        case Action::SetIrrigation: {
            if (request.argument != 0 && request.argument != 1) break;
            editor::GamePlayEditorCommand command{};
            command.type = C::SetFarmIrrigation;
            command.farmGeneration = s.farmGrid.GetGeneration();
            command.farmTileIndex = s.farmGrid.GetSelectedIndex();
            command.irrigationEnabled = request.argument == 1;
            success = s.gamePlayEditorBridge.Execute(command);
            break;
        }
        case Action::Compost:
            success = dispatch(C::CompostFarmTile);
            status_ = success ? Label::SoilApplied : Label::Failure;
            return;
        case Action::Tool: {
            constexpr FarmTool tools[] = {FarmTool::Hoe, FarmTool::Water, FarmTool::Seed, FarmTool::Harvest};
            if (request.argument < 0 || request.argument >= 4) break;
            s.farmToolSystem.SetTool(tools[request.argument]); open_ = false; success = true; break;
        }
        case Action::Crop: {
            if (request.argument < 0 || request.argument >= farm::kFarmCropTypeCount ||
                !farm::IsPlantableCrop(farm::CropTypeFromSlot(request.argument))) break;
            const auto previous = s.farmCropSelectionSystem.GetSelectedCrop();
            success = s.farmCropSelectionSystem.SetSelectedCrop(farm::CropTypeFromSlot(request.argument));
            if (success && previous != s.farmCropSelectionSystem.GetSelectedCrop()) s.farmDocumentSystem.MarkDirty();
            if (success && !open_) s.farmToolSystem.SetTool(FarmTool::Seed);
            s.farmFeedbackSystem.ShowCropSelected(s.farmCropSelectionSystem.GetSelectedCrop()); break;
        }
        case Action::Buy:
        case Action::BuyAndReturn: {
            const auto result = s.farmEconomySystem.BuySeed(s.farmCropSelectionSystem.GetSelectedCrop());
            success = result.Succeeded();
            if (success) { s.farmDocumentSystem.MarkDirty(); s.farmFeedbackSystem.ShowSeedPurchased(result.crop, result.purchasedCount, result.spentMoney); }
            else s.farmFeedbackSystem.ShowInsufficientMoney();
            if (success && request.action == Action::BuyAndReturn) {
                s.farmToolSystem.SetTool(FarmTool::Seed); open_ = false;
            }
            break;
        }
        case Action::Sell: case Action::SellAll:
            s.host.RouteFarmSale(request.action == Action::SellAll ? s.farmEconomySystem.SellAll() : s.farmEconomySystem.SellCrop(s.farmCropSelectionSystem.GetSelectedCrop()));
            open_ = false; success = true; break;
        case Action::Raise: success = dispatch(C::BeginFarmRaiseTerrainPreview); break;
        case Action::Lower: success = dispatch(C::BeginFarmLowerTerrainPreview); break;
        case Action::Canal: success = dispatch(C::BeginFarmCanalPreview); break;
        case Action::Source: success = dispatch(C::BeginFarmWaterSourcePreview); break;
        case Action::Path: success = dispatch(C::BeginFarmCanalPathPreview); if (success) brush_ = 1; break;
        case Action::RemovePath: success = dispatch(C::BeginFarmCanalRemovalPathPreview); if (success) brush_ = 2; break;
        case Action::Undo: success = dispatch(C::UndoFarmEdit); break;
        case Action::Redo: success = dispatch(C::RedoFarmEdit); break;
        case Action::PinA: case Action::PinB:
            success = s.farmGrowthComparisonSystem.Pin(s.farmGrid, request.action == Action::PinA ? 0 : 1, s.farmGrid.GetSelectedIndex());
            if (success) {
                EnterObservation(s);
                const int other = request.action == Action::PinA ? 1 : 0;
                const auto comparison = s.farmGrowthComparisonSystem.GetView(s.farmGrid);
                pickingSlot_ = comparison.rows[other].tileIndex < 0 ? other : -1;
                paused_ = true;
            }
            break;
        case Action::Start:
            success = s.farmGrowthComparisonSystem.Start(s.farmGrid);
            if (success) { EnterObservation(s); pickingSlot_ = -1; paused_ = false; }
            break;
        case Action::Speed:
            if (request.argument != 1 && request.argument != 2 && request.argument != 4) break;
            s.farmDateSystem.SetTimeScale(static_cast<float>(request.argument));
            s.farmDocumentSystem.MarkDirty();
            pickingSlot_ = -1; paused_ = false; success = true; break;
        default: break;
        }
        if (s.farmIrrigationPreviewSystem.IsActive()) EnterTerrain(s);
    }
    status_ = success ? Label::Success : Label::Failure;
}

void FarmRuntimeController::BeginFarmStart(const FarmRuntimeContext& s) {
    const auto initial = s.host.ReadCameraPose();
    if (!FrameFarm(s)) { flow_.Continue(); return; }
    const auto final = s.host.ReadCameraPose();
    const auto& grid = std::as_const(s.farmGrid);
    if (s.farmArrivalRenderer.IsReady() && FarmStartPresentationSystem::SupportsArrival(grid) &&
        startPresentation_.BeginArrival(final)) {
        flow_.BeginStart(); ApplyStartCamera(s); return;
    }
    s.farmIrrigationSystem.Rebuild(grid);
    std::array<FarmStartPresentationSystem::Cell, FarmStartPresentationSystem::kMaximumCells> cells{};
    const auto count = static_cast<std::size_t>(grid.GetTileCount());
    std::size_t validCount = 0;
    if (count <= cells.size()) {
        const auto& layout = s.farmVisualSystem.GetLayout();
        const auto& projection = s.host.ReadCamera()->GetProjectionMatrix();
        constexpr float kCropHeadroom = 1.5f;
        float ceiling = layout.center.y + kCropHeadroom;
        for (int i = 0; i < grid.GetTileCount(); ++i)
            ceiling = (std::max)(ceiling, s.farmVisualSystem.GetTileCenter(grid, i).y + kCropHeadroom);
        const float radius = (layout.tileSize + layout.tileGap) * 1.4f;
        constexpr farm::OverviewFrame kCinematicFrame{0.08f, 0.15f, 0.92f, 0.84f};
        for (int i = 0; i < grid.GetTileCount(); ++i) {
            const auto* tile = grid.GetTile(i);
            if (!tile) continue;
            auto& cell = cells[static_cast<std::size_t>(i)];
            const auto center = s.farmVisualSystem.GetTileCenter(grid, i);
            const Vector3 minimum{center.x-radius, center.y, center.z-radius};
            const Vector3 maximum{center.x+radius, ceiling, center.z+radius};
            if (!farm::FitOverviewCamera(minimum, maximum, projection.m[0][0], projection.m[1][1],
                s.host.ReadCamera()->GetNearClip(), s.host.ReadCamera()->GetFarClip(), kCinematicFrame, cell.pose, 0.86f)) continue;
            cell.source = tile->feature == farm::FarmTileFeature::WaterSource;
            cell.canal = tile->feature == farm::FarmTileFeature::Canal;
            cell.upstream = s.farmIrrigationSystem.GetUpstreamTileIndex(i);
        }
        validCount = count;
    }
    if (!startPresentation_.Begin(std::span(cells).first(validCount), grid.GetWidth(), initial, final)) {
        flow_.Continue(); return;
    }
    flow_.BeginStart();
    ApplyStartCamera(s);
}

void FarmRuntimeController::ApplyStartCamera(const FarmRuntimeContext& s) {
    const auto pose = startPresentation_.GetFrame().pose;
    s.host.SetUsePlayerCamera(false);
    s.host.ApplyCameraPose(pose, true);
}

bool FarmRuntimeController::FrameFarm(const FarmRuntimeContext& s, bool followPlayer, bool briefing) {
    if (!s.host.ReadCamera() || s.farmGrid.GetWidth() <= 0 || s.farmGrid.GetHeight() <= 0) return false;
    const auto& layout = s.farmVisualSystem.GetLayout();
    const float pitch = layout.tileSize + layout.tileGap;
    const float halfX = static_cast<float>(s.farmGrid.GetWidth())*pitch*0.5f;
    const float halfZ = static_cast<float>(s.farmGrid.GetHeight())*pitch*0.5f;
    // Include every editable height and modest crop headroom, not only the current two corner tiles.
    constexpr float cropHeadroom = 0.8f;
    const Vector3 minimum{layout.center.x-halfX, layout.center.y +
        FarmToolActionSystem::kMinimumHeightLevel*layout.heightStep, layout.center.z-halfZ};
    const Vector3 maximum{layout.center.x+halfX, layout.center.y +
        FarmToolActionSystem::kMaximumHeightLevel*layout.heightStep+cropHeadroom, layout.center.z+halfZ};
    const auto& projection = s.host.ReadCamera()->GetProjectionMatrix();
    farm::OverviewPose pose;
    constexpr farm::OverviewFrame kBriefingFrame{0.06f, 0.16f, 0.94f, 0.66f};
    const auto frame = briefing ? kBriefingFrame : terrain_ ? farm::kTerrainOverviewFrame :
        observation_ ? farm::kObservationOverviewFrame : farm::kFarmOverviewFrame;
    if (followPlayer) {
        if (!s.levelGameplay.HasPlayer() || !farm::FitFarmFollowCamera(minimum, maximum,
            s.levelGameplay.GetPlayerColliderCenter(), s.levelGameplay.GetPlayerColliderHalfExtents(),
            projection.m[0][0], projection.m[1][1], s.host.ReadCamera()->GetNearClip(), s.host.ReadCamera()->GetFarClip(), frame, pose)) return false;
    } else {
        if (!farm::FitOverviewCamera(minimum, maximum, projection.m[0][0], projection.m[1][1],
            s.host.ReadCamera()->GetNearClip(), s.host.ReadCamera()->GetFarClip(), frame, pose)) return false;
        s.host.SetUsePlayerCamera(false);
    }
    s.host.ApplyCameraPose(pose, !followPlayer);
    return true;
}

int FarmRuntimeController::ResolveHoveredTile(const FarmRuntimeContext& s) const {
    if (!ready_ || !s.host.IsViewportFocused() || !s.host.IsViewportHovered() || open_ || observation_ ||
        frameCaptured_ || flow_.BlocksSimulation() || s.farmProgressionSystem.IsCleared() ||
        s.farmIrrigationPreviewSystem.IsActive() || s.farmCropSelectionSystem.IsOpen() ||
        view_.Covers(pointer_)) return -1;
    Vector3 origin{}, direction{};
    int index = -1;
    if (!s.host.TryBuildViewportRay(origin, direction) ||
        !s.farmVisualSystem.TryPickTile(s.farmGrid, origin, direction, index)) return -1;
    return index;
}

void FarmRuntimeController::EnterObservation(const FarmRuntimeContext& s) {
    if (terrain_) LeaveTerrain(s);
    const bool entering = !observation_;
    if (entering) { pauseBeforeObservation_ = paused_; paused_ = true; }
    observation_ = true; open_ = false; focusedItem_ = -1;
    if (entering) FrameFarm(s);
    s.farmCropSelectionSystem.Cancel();
}

void FarmRuntimeController::EnterTerrain(const FarmRuntimeContext& s) {
    const bool entering = !terrain_;
    if (observation_) {
        observation_ = false; pickingSlot_ = -1; paused_ = pauseBeforeObservation_;
    }
    terrain_ = true; open_ = false; focusedItem_ = -1;
    s.farmCropSelectionSystem.Cancel();
    if (entering) { status_ = Label::Terrain; FrameFarm(s); }
}

void FarmRuntimeController::LeaveTerrain(const FarmRuntimeContext& s) {
    // Uncommitted edits are discarded; the player's explicit pause setting is unchanged.
    s.farmIrrigationPreviewSystem.Cancel(); brush_ = 0;
    terrain_ = false;
    if (!s.host.IsFollowingPlayer()) FrameFarm(s);
}

bool FarmRuntimeController::UpdateTerrain(const FarmRuntimeContext& s, const Input& input, bool click) {
    const int dx = (input.TriggerKey(InputKey::ArrowRight) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowLeft) ? 1 : 0);
    const int dy = (input.TriggerKey(InputKey::ArrowDown) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowUp) ? 1 : 0);
    if (dx || dy) s.farmGrid.MoveSelection(dx, dy);
    if (click && !view_.Covers(pointer_)) {
        Vector3 origin{}, direction{}; int index = -1;
        if (s.host.TryBuildViewportRay(origin, direction) && s.farmVisualSystem.TryPickTile(s.farmGrid, origin, direction, index))
            s.farmGrid.SetSelectedIndex(index);
    }
    Action action = Action::None;
    if (input.TriggerKey(InputKey::N)) action = Action::Canal;
    if (input.TriggerKey(InputKey::M)) action = Action::Source;
    if (input.TriggerKey(InputKey::PageUp)) action = Action::Raise;
    if (input.TriggerKey(InputKey::PageDown)) action = Action::Lower;
    if (action != Action::None) Execute(s, {action});
    BuildView(s);
    return true;
}

bool FarmRuntimeController::UpdateObservation(const FarmRuntimeContext& s, const Input& input, bool click) {
    // Observation captures gameplay input, but only explicit pause blocks simulation.
    // Panel backgrounds and disabled buttons must not pick a tile behind the UI.
    if (input.TriggerMouseButton(InputMouseButton::Right)) { pickingSlot_ = -1; BuildView(s); return true; }
    const int dx = (input.TriggerKey(InputKey::ArrowRight) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowLeft) ? 1 : 0);
    const int dy = (input.TriggerKey(InputKey::ArrowDown) ? 1 : 0) - (input.TriggerKey(InputKey::ArrowUp) ? 1 : 0);
    if (dx || dy) { s.farmGrid.MoveSelection(dx, dy); BuildView(s); }
    if (pickingSlot_ >= 0 && input.TriggerKey(InputKey::Enter)) {
        Execute(s, {pickingSlot_ == 0 ? Action::PinA : Action::PinB}); BuildView(s); frameCaptured_ = true; return true;
    }
    if (view_.Covers(pointer_)) return true;
    if (click) {
        Vector3 origin{}, direction{}; int index = -1;
        if (s.host.TryBuildViewportRay(origin, direction) && s.farmVisualSystem.TryPickTile(s.farmGrid, origin, direction, index)) {
            s.farmGrid.SetSelectedIndex(index);
            if (pickingSlot_ >= 0) Execute(s, {pickingSlot_ == 0 ? Action::PinA : Action::PinB});
            BuildView(s); frameCaptured_ = true;
        }
    }
    return true;
}

void FarmRuntimeController::OpenLayoutLibrary(const FarmRuntimeContext& s) {
    if (!s.window || s.farmIrrigationPreviewSystem.IsActive()) return;
    const HWND owner = s.window->GetHwnd();
    FarmLayoutSystem layouts;
    if (!layouts.Initialize("Settings/farm_layouts")) { farmui::RecordNotice(owner, layouts.Error()); return; }
    s.farmCropSelectionSystem.Cancel();
    std::string name;
    for (;;) {
        std::vector<std::string> names;
        for (const auto& entry : layouts.Entries()) names.push_back(entry.name);
        const auto result = farmui::ShowRecordDialog(owner, farmui::RecordDialogMode::LayoutLibrary, names, name);
        if (result.action == farmui::RecordDialogAction::Cancel) return;
        if (result.action == farmui::RecordDialogAction::SaveNew) {
            name = result.name;
            if (!layouts.SaveNew(name, s.farmGrid)) farmui::RecordNotice(owner, layouts.Error());
            else { farmui::RecordNotice(owner, "配置を保存しました。進行セーブは変更していません。"); name.clear(); }
            continue;
        }
        if (result.selected < 0 || result.selected >= static_cast<int>(layouts.Entries().size())) continue;
        if (!farmui::ConfirmLayoutReplacement(owner)) continue;
        if (!layouts.Load(layouts.Entries()[result.selected].id, s.farmGrid, s.farmToolActionSystem.MaximumAllowedHeight())) {
            farmui::RecordNotice(owner, layouts.Error()); continue;
        }
        s.farmToolActionSystem.ClearHistory();
        s.farmIrrigationPreviewSystem.Cancel();
        s.farmGrowthComparisonSystem.Reset();
        s.farmFeedbackSystem.Clear();
        s.farmIrrigationSystem.Rebuild(s.farmGrid);
        s.farmDocumentSystem.MarkDirty();
        s.host.InitializeTimeline();
        brush_ = 0;
        status_ = Label::Loaded;
        farmui::RecordNotice(owner, "配置を再現しました。所持金・持ち物・日数はそのままです。");
        return;
    }
}

bool FarmRuntimeController::Update(const FarmRuntimeContext& s, const Input& input) {
    frameCaptured_ = false;
    if (!ready_) return false;
    if (exhibition_.Active()) return UpdateExhibition(s, input);
    if (!s.host.IsViewportFocused()) {
        arrivalAudio_.Update(StartFrame(),flow_.GetPhase()==FarmPlayFlow::Phase::Starting,false);
        contestAudio_.Update(contestPresentation_.ConsumeCue(),contestPresentation_.Active(),false);
        s.farmIrrigationPreviewSystem.EndTerrainStroke();
        frameCaptured_ = true; return true;
    }
    s.host.ConvertMouseToVirtualScreen(input.GetMousePosition(), pointer_);
    BuildView(s);
    const bool click = input.TriggerMouseButton(InputMouseButton::Left);
    pointerTriggered_ = click || input.TriggerMouseButton(InputMouseButton::Right);
    const auto hit = view_.Hit(pointer_);
    if (contestPresentation_.Active()) {
        const auto* framework = Framework::GetInstance();
        const auto* clock = framework ? framework->GetFrameClock() : nullptr;
        contestPresentation_.Update(clock ? clock->GetRealDeltaSeconds() : 0);
        const bool accept = input.TriggerKey(InputKey::Enter) || input.TriggerKey(InputKey::Space) ||
            input.TriggerGamepadButton(InputGamepadButton::A);
        const bool back = input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::B);
        // Use the displayed snapshot so a skip never activates newly enabled navigation.
        if (click && hit.action != Action::None) { Execute(s, hit); focusedItem_ = -1; }
        else if (!view_.contestFrame.complete) {
            if (accept || back) Execute(s, {Action::ContestReveal});
        } else if (back) {
            if (!contestPresentation_.Back()) Execute(s, {Action::ContestDetailClose});
            focusedItem_ = -1;
        } else {
            const int step = (input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::ArrowRight) ||
                input.TriggerGamepadButton(InputGamepadButton::DPadDown) || input.TriggerGamepadButton(InputGamepadButton::DPadRight)) ? 1 :
                (input.TriggerKey(InputKey::ArrowUp) || input.TriggerKey(InputKey::ArrowLeft) ||
                input.TriggerGamepadButton(InputGamepadButton::DPadUp) || input.TriggerGamepadButton(InputGamepadButton::DPadLeft)) ? -1 : 0;
            if (step) focusedItem_ = view_.NextActionable(focusedItem_, step);
            if (accept) {
                if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count))
                    Execute(s, view_.items[focusedItem_].request);
                else if (!contestPresentation_.Back()) Execute(s, {Action::ContestDetailClose});
            }
        }
        contestAudio_.Update(contestPresentation_.ConsumeCue(),contestPresentation_.Active(),true);
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (view_.contestOutcome) {
        const auto* framework = Framework::GetInstance();
        const auto* clock = framework ? framework->GetFrameClock() : nullptr;
        const bool opening = contestOutcome_.ConsumeOpening();
        if (opening) {
            title::TitleAudioSettingsSystem settings;
            settings.Initialize();
            contestAudio_.Begin(settings.GetContestVolume());
        }
        // Input uses the displayed snapshot: a skip cannot activate a newly revealed retry button.
        const bool revealing = view_.seasonOutcome && contestOutcome_.GetFrame().controlsReveal < 1;
        contestOutcome_.Update(clock ? clock->GetRealDeltaSeconds() : 0);
        if (revealing && (input.TriggerKey(InputKey::Enter) || input.TriggerKey(InputKey::Space) ||
            input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::A) ||
            input.TriggerGamepadButton(InputGamepadButton::B))) Execute(s, {Action::ContestOutcomeReveal});
        else if (click && hit.action != Action::None) { Execute(s, hit); focusedItem_ = -1; }
        else if (input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::B)) {
            if (pending_ == Action::SeasonRetry) Execute(s, {Action::Cancel});
        } else {
            const int step = (input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::ArrowRight) ||
                input.TriggerGamepadButton(InputGamepadButton::DPadDown) || input.TriggerGamepadButton(InputGamepadButton::DPadRight)) ? 1 :
                (input.TriggerKey(InputKey::ArrowUp) || input.TriggerKey(InputKey::ArrowLeft) ||
                input.TriggerGamepadButton(InputGamepadButton::DPadUp) || input.TriggerGamepadButton(InputGamepadButton::DPadLeft)) ? -1 : 0;
            if (step) focusedItem_ = view_.NextActionable(focusedItem_, step);
            if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count) &&
                (input.TriggerKey(InputKey::Enter) || input.TriggerGamepadButton(InputGamepadButton::A)))
                Execute(s, view_.items[focusedItem_].request);
        }
        BuildView(s);
        contestAudio_.Update(opening ? FarmContestPresentationSystem::Cue::Recorded : FarmContestPresentationSystem::Cue::None,
            view_.contestOutcome, true);
        frameCaptured_ = true; return true;
    }
    contestAudio_.Update(contestPresentation_.ConsumeCue(),false,true);
    if (flow_.GetPhase() == FarmPlayFlow::Phase::Starting) {
        const bool held = input.PushKey(InputKey::Enter) || input.PushKey(InputKey::Space) ||
            input.PushKey(InputKey::Escape) || input.PushMouseButton(InputMouseButton::Left) ||
            input.PushMouseButton(InputMouseButton::Right) || input.PushGamepadButton(InputGamepadButton::A) ||
            input.PushGamepadButton(InputGamepadButton::B);
        const bool skip = input.TriggerKey(InputKey::Enter) || input.TriggerKey(InputKey::Space) ||
            input.TriggerKey(InputKey::Escape) || input.TriggerGamepadButton(InputGamepadButton::A) ||
            input.TriggerGamepadButton(InputGamepadButton::B) || (click && hit.action == Action::StartSkip);
        const auto* framework = Framework::GetInstance();
        const auto* clock = framework ? framework->GetFrameClock() : nullptr;
        startPresentation_.Update(clock ? clock->GetRealDeltaSeconds() : 0.0f, held, skip);
        ApplyStartCamera(s);
        if (startPresentation_.ConsumeCompleted()) flow_.FinishStart();
        arrivalAudio_.Update(StartFrame(),flow_.GetPhase()==FarmPlayFlow::Phase::Starting,true);
        BuildView(s); frameCaptured_ = true; return true;
    }
    arrivalAudio_.Update({},false,true);
    // Shop input never falls through to farm picking, quick purchases or player movement.
    if (view_.seedShop) {
        if (input.TriggerKey(InputKey::Escape) || input.TriggerMouseButton(InputMouseButton::Right)) {
            if (shop_.GetPhase() == FarmSeedShopSystem::Phase::Confirming) shop_.CancelConfirmation();
            else Execute(s, {Action::ShopClose});
        } else if (click && hit.action != Action::None) Execute(s, hit);
        else {
            if (input.TriggerKey(InputKey::ArrowLeft)) shop_.Select((shop_.Selection() + FarmSeedShopSystem::kProductCount - 1) % FarmSeedShopSystem::kProductCount);
            else if (input.TriggerKey(InputKey::ArrowRight)) shop_.Select((shop_.Selection() + 1) % FarmSeedShopSystem::kProductCount);
            if (input.TriggerKey(InputKey::ArrowUp)) shop_.ChangeQuantity(1);
            else if (input.TriggerKey(InputKey::ArrowDown)) shop_.ChangeQuantity(-1);
            if (input.TriggerKey(InputKey::Enter)) Execute(s, {shop_.GetPhase() == FarmSeedShopSystem::Phase::Confirming ? Action::ShopConfirm : Action::ShopReview});
        }
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (click && hit.action != Action::None) {
        s.farmIrrigationPreviewSystem.EndTerrainStroke();
        focusedItem_ = -1;
        Execute(s, hit); BuildView(s); frameCaptured_ = true; return true;
    }
    if (!contestNotice_ && input.TriggerKey(InputKey::Escape)) {
        if (pending_ != Action::None) Execute(s, {Action::Cancel});
        else if (open_) Execute(s, {Action::Close});
        else if (observation_ && pickingSlot_ >= 0) pickingSlot_ = -1;
        else if (s.farmIrrigationPreviewSystem.IsActive()) { s.farmIrrigationPreviewSystem.Cancel(); brush_ = 0; }
        else if (terrain_) LeaveTerrain(s);
        else if (s.farmCropSelectionSystem.IsOpen()) s.farmCropSelectionSystem.Cancel();
        else open_ = true;
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (contestNotice_ || open_ || flow_.BlocksSimulation()) {
        s.farmIrrigationPreviewSystem.EndTerrainStroke();
        if (!contestNotice_ && open_ && input.TriggerKey(InputKey::Tab) && pending_ == Action::None) {
            Execute(s, {Action::Tab, (page_ + 1) % 5});
        }
        const int step = (input.TriggerKey(InputKey::ArrowDown) || input.TriggerKey(InputKey::ArrowRight)) ? 1
            : (input.TriggerKey(InputKey::ArrowUp) || input.TriggerKey(InputKey::ArrowLeft)) ? -1 : 0;
        if (step != 0) {
            focusedItem_ = view_.NextActionable(focusedItem_, step);
        }
        if (focusedItem_ >= 0 && focusedItem_ < static_cast<int>(view_.count)) {
            const auto& item = view_.items[focusedItem_];
            if (step) pointer_ = {item.rect.x + item.rect.width * 0.5f, item.rect.y + item.rect.height * 0.5f};
            if (input.TriggerKey(InputKey::Enter) && item.enabled) {
                Execute(s, item.request); focusedItem_ = -1;
            }
        }
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (observation_) return UpdateObservation(s, input, click);
    if (click && view_.Covers(pointer_)) {
        s.farmIrrigationPreviewSystem.EndTerrainStroke();
        frameCaptured_ = true; return true;
    }
    if (s.farmIrrigationPreviewSystem.IsActive()) {
        auto& preview = s.farmIrrigationPreviewSystem;
        const auto operation = preview.GetOperation();
        const bool heightBrush = operation == farm::FarmIrrigationPreviewOperation::RaiseTerrain ||
            operation == farm::FarmIrrigationPreviewOperation::LowerTerrain;
        if (input.TriggerKey(InputKey::Enter)) Execute(s, {Action::Confirm});
        else if (input.TriggerMouseButton(InputMouseButton::Right)) Execute(s, {Action::Cancel});
        else if ((brush_ || heightBrush) && !view_.Covers(pointer_) && input.PushMouseButton(InputMouseButton::Left)) {
            Vector3 origin{}, direction{}; int index = -1;
            if (s.host.TryBuildViewportRay(origin, direction) && s.farmVisualSystem.TryPickTile(s.farmGrid, origin, direction, index)) {
                if (heightBrush) static_cast<void>(preview.VisitTerrainTile(s.farmGrid, index));
                else static_cast<void>(preview.VisitCanalPathTile(s.farmGrid, index));
            } else preview.EndTerrainStroke();
        } else preview.EndTerrainStroke();
        BuildView(s); frameCaptured_ = true; return true;
    }
    if (terrain_) return UpdateTerrain(s, input, click);
    if (!s.farmCropSelectionSystem.IsOpen()) {
        if (click) {
            // A HUD click must never select or modify a tile hidden behind it.
            if (view_.Covers(pointer_)) { frameCaptured_ = true; return true; }
        }
        Action action = Action::None;
        if (input.TriggerKey(InputKey::N)) action = Action::Canal;
        if (input.TriggerKey(InputKey::M)) action = Action::Source;
        if (input.TriggerKey(InputKey::PageUp)) action = Action::Raise;
        if (input.TriggerKey(InputKey::PageDown)) action = Action::Lower;
        if (action != Action::None) { Execute(s, {action}); BuildView(s); frameCaptured_ = true; return true; }
    }
    if (paused_) {
        // Pause freezes simulation, not explicit farm edits. Consume this frame so
        // Scene cannot repeat the action or move the player after the edit.
        if (s.farmProgressionSystem.IsCleared() || s.farmCropSelectionSystem.IsOpen()) return true;
        if (click && s.host.IsViewportHovered()) {
            Vector3 origin{}, direction{}; int index = -1;
            if (s.host.TryBuildViewportRay(origin, direction) &&
                s.farmVisualSystem.TryPickTile(s.farmGrid, origin, direction, index))
                s.farmGrid.SetSelectedIndex(index);
        }
        FarmInputContext context{};
        context.currentDay = s.farmDateSystem.GetDay();
        const auto result = s.farmInputSystem.Update(input, context, s.farmGrid, s.farmToolSystem,
            s.farmCropSelectionSystem.GetSelectedCrop(), s.farmEconomySystem, s.farmToolActionSystem);
        if (result.contentChanged) s.farmDocumentSystem.MarkDirty();
        s.host.RouteFarmToolFeedback(result.toolAction);
        if (result.buySeedRequested) Execute(s, {Action::Buy});
        if (result.sellSelectedRequested) Execute(s, {Action::Sell});
        else if (result.sellRequested) Execute(s, {Action::SellAll});
        BuildView(s);
        return true;
    }
    return false;
}
