#include "editor/GamePlayEditorBridge.h"
#include "farm/system/FarmSoilSystem.h"

#include "3d/Object3d.h"
#include "base/FrameClock.h"
#include "base/Framework.h"
#include "effect/ParticleManager.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolActionSystem.h"
#include "gameplay/GamePlaySessionSystem.h"

#include <algorithm>
#include <string_view>

using namespace editor;

namespace {
constexpr float kDirectionEpsilon = 0.0001f;

Vector3 NormalizeOrKeep(const Vector3& candidate, const Vector3& fallback) {
	return MatrixMath::Length(candidate) > kDirectionEpsilon
		? MatrixMath::Normalize(candidate)
		: fallback;
}

EditorGpuParticleMode ToEditorGpuParticleMode(GamePlaySessionSystem::GPUParticleDebugMode mode) {
	switch (mode) {
	case GamePlaySessionSystem::GPUParticleDebugMode::Agriculture:
		return EditorGpuParticleMode::Agriculture;
	case GamePlaySessionSystem::GPUParticleDebugMode::Interaction:
		return EditorGpuParticleMode::Interaction;
	case GamePlaySessionSystem::GPUParticleDebugMode::Off:
	default:
		return EditorGpuParticleMode::Off;
	}
}

GamePlaySessionSystem::GPUParticleDebugMode ToSceneGpuParticleMode(EditorGpuParticleMode mode) {
	switch (mode) {
	case EditorGpuParticleMode::Agriculture:
		return GamePlaySessionSystem::GPUParticleDebugMode::Agriculture;
	case EditorGpuParticleMode::Interaction:
		return GamePlaySessionSystem::GPUParticleDebugMode::Interaction;
	case EditorGpuParticleMode::Off:
	default:
		return GamePlaySessionSystem::GPUParticleDebugMode::Off;
	}
}

GamePlaySessionSystem::AgricultureParticleType ToSceneAgricultureParticleType(
	EditorAgricultureParticleType type) {
	switch (type) {
	case EditorAgricultureParticleType::WaterSplash:
		return GamePlaySessionSystem::AgricultureParticleType::WaterSplash;
	case EditorAgricultureParticleType::HarvestSparkle:
		return GamePlaySessionSystem::AgricultureParticleType::HarvestSparkle;
	case EditorAgricultureParticleType::DirtDust:
	default:
		return GamePlaySessionSystem::AgricultureParticleType::DirtDust;
	}
}
} // namespace

void GamePlaySessionSystem::BuildViewModel(GamePlayEditorViewModel& output) const {
	output.farmWidth = 0;
	output.farmHeight = 0;
	output.irrigationWaterSourceCount = 0;
	output.irrigationSuppliedCanalCount = 0;
	output.irrigationRangeTileCount = 0;
	output.irrigationLastStep = {};
	output.irrigationPreviewActive = false;
	output.irrigationPreviewCanConfirm = false;
	output.irrigationPreviewTileIndex = -1;
	output.irrigationPreviewOperation = farm::FarmIrrigationPreviewOperation::None;
	output.irrigationPreviewOriginalFeature = farm::FarmTileFeature::None;
	output.irrigationPreviewCandidateFeature = farm::FarmTileFeature::None;
	output.irrigationPreviewOriginalHeightLevel = 0;
	output.irrigationPreviewCandidateHeightLevel = 0;
	output.irrigationPreviewChangeCount = 0;
	output.irrigationPathIssue = farm::FarmCanalPathIssue::None;
	output.irrigationBlockedTileIndex = -1;
	output.irrigationPreviewWaterSourceCount = 0;
	output.irrigationPreviewSuppliedCanalCount = 0;
	output.irrigationPreviewRangeTileCount = 0;
	output.selectedFarmTileIndex = -1;
	output.farmGeneration = 0;
	output.farmDocumentDirty = false;
	output.farmDocumentExists = false;
	output.farmDocumentHasError = false;
	output.farmDocumentStatus = FarmDocumentStatus::Ready;
	output.farmDocumentActiveId.clear();
	output.farmDocumentName.clear();
	output.farmDocumentMessage.clear();
	output.farmDocuments.clear();
	output.farmTiles.clear();
	output.canUndo = false;
	output.canRedo = false;
	output.undoCount = 0;
	output.redoCount = 0;
	output.undoName.clear();
	output.redoName.clear();
	output.currentFarmTool = FarmTool::Hoe;
	output.selectedFarmAction = {};
	output.farmPlaytest = {};
	output.farmGrowthComparison = {};
	output.visibility = {};
	output.camera = {};
	output.objectInspector.directionalLight = {};
	output.objectInspector.spotLight = {};
	output.objectInspector.sphere = {};
	output.objectInspector.animation = {};
	output.objectInspector.cylinder = {};
	output.objectInspector.joints.clear();
	output.particles = {};
	output.simulation = {};
	if (!framework_) {
		return;
	}

	output.farmWidth = farmGrid_.GetWidth();
	output.farmHeight = farmGrid_.GetHeight();
	output.irrigationWaterSourceCount = farmIrrigationSystem_.GetWaterSourceCount();
	output.irrigationSuppliedCanalCount = farmIrrigationSystem_.GetSuppliedCanalCount();
	output.irrigationRangeTileCount = farmIrrigationSystem_.GetIrrigationRangeTileCount();
	const farm::FarmGrid* displayedGrid = &farmGrid_;
	const farm::FarmIrrigationSystem* displayedIrrigation = &farmIrrigationSystem_;
	const farm::FarmGrid* previewGrid = farmIrrigationPreviewSystem_.GetPreviewGrid();
	const farm::FarmIrrigationSystem* previewIrrigation =
		farmIrrigationPreviewSystem_.GetPreviewIrrigation();
	if (previewGrid != nullptr && previewIrrigation != nullptr) {
		output.irrigationPreviewActive = true;
		output.irrigationPreviewCanConfirm =
			farmIrrigationPreviewSystem_.CanConfirm(farmGrid_);
		output.irrigationPreviewTileIndex =
			farmIrrigationPreviewSystem_.GetTileIndex();
		output.irrigationPreviewOperation =
			farmIrrigationPreviewSystem_.GetOperation();
		output.irrigationPreviewOriginalFeature =
			farmIrrigationPreviewSystem_.GetOriginalFeature();
		output.irrigationPreviewCandidateFeature =
			farmIrrigationPreviewSystem_.GetCandidateFeature();
		output.irrigationPreviewOriginalHeightLevel =
			farmIrrigationPreviewSystem_.GetOriginalHeightLevel();
		output.irrigationPreviewCandidateHeightLevel =
			farmIrrigationPreviewSystem_.GetCandidateHeightLevel();
		output.irrigationPreviewChangeCount = static_cast<int>(
			farmIrrigationPreviewSystem_.GetChangeCount());
		output.irrigationPathIssue = farmIrrigationPreviewSystem_.GetPathIssue();
		output.irrigationBlockedTileIndex = farmIrrigationPreviewSystem_.GetBlockedTileIndex();
		output.irrigationPreviewWaterSourceCount = previewIrrigation->GetWaterSourceCount();
		output.irrigationPreviewSuppliedCanalCount = previewIrrigation->GetSuppliedCanalCount();
		output.irrigationPreviewRangeTileCount = previewIrrigation->GetIrrigationRangeTileCount();
		displayedGrid = previewGrid;
		displayedIrrigation = previewIrrigation;
	}
	output.selectedFarmTileIndex = farmGrid_.GetSelectedIndex();
	output.irrigationLastStep = displayedIrrigation->GetLastStep(*displayedGrid);
	const auto irrigationFlows = displayedIrrigation->GetLastTileFlows(*displayedGrid);
	output.farmGeneration = farmGrid_.GetGeneration();
	output.farmGrowthComparison = farmGrowthComparisonSystem_.GetView(farmGrid_);
	output.farmDocumentDirty = farmDocumentSystem_.IsDirty();
	output.farmDocumentExists = farmDocumentSystem_.FileExists();
	output.farmDocumentHasError = farmDocumentSystem_.HasError();
	output.farmDocumentStatus = farmDocumentSystem_.GetStatus();
	output.farmDocumentActiveId = farmDocumentSystem_.GetActiveDocumentId();
	output.farmDocumentName = farmDocumentSystem_.GetDisplayName();
	output.farmDocumentMessage = farmDocumentSystem_.GetStatusMessage();
	const std::vector<FarmDocumentEntry>& documents = farmDocumentSystem_.GetDocuments();
	output.farmDocuments.resize(documents.size());
	for (std::size_t index = 0; index < documents.size(); ++index) {
		const FarmDocumentEntry& source = documents[index];
		FarmDocumentEditorViewData& destination = output.farmDocuments[index];
		destination.id = source.id;
		destination.displayName = source.displayName;
		destination.savedAt = source.savedAt;
		destination.active = source.id == output.farmDocumentActiveId;
	}
	output.farmTiles.resize(static_cast<std::size_t>(farmGrid_.GetTileCount()));
	for (int index = 0; index < farmGrid_.GetTileCount(); ++index) {
		const farm::FarmTile* tile = displayedGrid->GetTile(index);
		const farm::FarmTile* authoritativeTile = farmGrid_.GetTile(index);
		FarmTileEditorViewData& destination = output.farmTiles[static_cast<std::size_t>(index)];
		destination = {};
		destination.index = index;
		destination.column = output.farmWidth > 0 ? index % output.farmWidth : 0;
		destination.row = output.farmWidth > 0 ? index / output.farmWidth : 0;
		if (tile != nullptr) {
			const farm::CropType selectedCrop =
				farmCropSelectionSystem_.GetSelectedCrop();
			destination.heightLevel = tile->heightLevel;
			destination.feature = tile->feature;
			destination.state = tile->state;
			destination.crop = tile->crop;
			destination.growthStage = farm::GetCropGrowthStage(*tile);
			destination.moisture = tile->moisture;
			destination.careHistory = tile->careHistory;
			destination.soilNutrients = tile->soilNutrients;
			const auto soilCrop = farm::IsPlantableCrop(tile->crop) ? tile->crop : farmCropSelectionSystem_.GetSelectedCrop();
			const auto* soilProfile = FarmSoilSystem::Profile(soilCrop);
			destination.nutrientTarget = soilProfile ? soilProfile->target : 0.0f;
			destination.canCompost = FarmSoilSystem::CanCompost(*tile);
			destination.storedWater = tile->waterAmount;
			destination.irrigationEnabled = tile->irrigationEnabled;
			destination.canSetIrrigation = !farmProgressionSystem_.IsCleared() && !output.irrigationPreviewActive &&
				farmToolActionSystem_.CanSetIrrigation(farmGrid_, index);
			destination.waterStatus = displayedIrrigation->GetWaterStatus(*displayedGrid, index);
			if (output.irrigationLastStep.valid && static_cast<std::size_t>(index) < irrigationFlows.size()) {
				destination.irrigationFlow = irrigationFlows[static_cast<std::size_t>(index)];
			}
			destination.growth = tile->growth;
			destination.irrigationInRange =
				displayedIrrigation->IsInIrrigationRange(index);
			destination.irrigationSupplyStrength =
				displayedIrrigation->GetSupplyStrength(index);
			destination.irrigationStrength =
				displayedIrrigation->GetIrrigationStrength(index);
			destination.irrigationDownstreamCanalCount =
				displayedIrrigation->GetDownstreamCanalCount(index);
			destination.irrigationSupplierTileIndex =
				displayedIrrigation->GetSupplyingCanalIndex(index);
			destination.growthForecast = farmGrowthSystem_.Evaluate(
				*tile,
				selectedCrop,
				farmDateSystem_.GetTimeScale(),
				displayedIrrigation->GetAvailableIrrigationStrength(*displayedGrid, index));
			destination.waterGuidance = FarmGrowthSystem::AnalyzeWater(
				tile, destination.growthForecast, destination.waterStatus);
			destination.quality = farmToolActionSystem_.EvaluateHarvestQuality(*tile);
			destination.qualityAdvice = FarmCropQualitySystem::Analyze(destination.quality);
			destination.canHoe = farmToolActionSystem_.EvaluateTool(
				farmGrid_, index, FarmTool::Hoe, selectedCrop).Succeeded();
			destination.canWater = farmToolActionSystem_.EvaluateTool(
				farmGrid_, index, FarmTool::Water, selectedCrop).Succeeded();
			destination.canSeed = farmToolActionSystem_.EvaluateTool(
				farmGrid_, index, FarmTool::Seed, selectedCrop,
				&farmEconomySystem_).Succeeded();
			destination.canHarvest = farmToolActionSystem_.EvaluateTool(
				farmGrid_, index, FarmTool::Harvest, selectedCrop).Succeeded();
			destination.canToggleCanal = authoritativeTile != nullptr &&
				farmToolActionSystem_.CanToggleCanal(farmGrid_, index);
			destination.canToggleWaterSource = authoritativeTile != nullptr &&
				farmToolActionSystem_.CanToggleWaterSource(farmGrid_, index);
			destination.irrigationSupplied = displayedIrrigation->IsSupplied(index);
			destination.irrigationPreviewChanged =
				farmIrrigationPreviewSystem_.IsTileChanged(index);
		}
	}

	const CommandHistory& history = farmToolActionSystem_.GetHistory();
	output.canUndo = history.CanUndo();
	output.canRedo = history.CanRedo();
	output.undoCount = history.GetUndoCount();
	output.redoCount = history.GetRedoCount();
	if (output.canUndo) {
		const std::string_view undoName = history.GetUndoName();
		output.undoName.assign(undoName.data(), undoName.size());
	}
	if (output.canRedo) {
		const std::string_view redoName = history.GetRedoName();
		output.redoName.assign(redoName.data(), redoName.size());
	}
	output.currentFarmTool = farmToolSystem_.GetCurrentTool();
	const farm::CropType selectedCrop =
		farmCropSelectionSystem_.GetSelectedCrop();
	output.selectedFarmAction = farmToolActionSystem_.EvaluateTool(
		farmGrid_, output.currentFarmTool, selectedCrop,
		&farmEconomySystem_);
	output.farmPlaytest.money = farmEconomySystem_.GetMoney();
	output.farmPlaytest.clock = farmDateSystem_.CaptureSnapshot();
	output.farmPlaytest.dayLengthSeconds = farmDateSystem_.GetDayLengthSeconds();
	output.farmPlaytest.playerPosition = levelGameplay_.GetPlayerPosition();
	output.farmPlaytest.playerGrounded = levelGameplay_.IsPlayerGrounded();
	output.farmPlaytest.canPlacePlayer = levelGameplay_.HasPlayer() && levelGameplay_.HasGround() &&
		!farmGameMode_ && !timelineScrubbing_ && !farmIrrigationPreviewSystem_.IsActive() &&
		!farmProgressionSystem_.IsCleared();
	output.farmPlaytest.playerMovementBlocked = levelGameplay_.WasPlayerMovementBlocked();
	const auto& playerOffset = levelGameplay_.GetPlayerColliderCenterOffset();
	const auto& playerHalf = levelGameplay_.GetPlayerColliderHalfExtents();
	const auto& playerPosition = output.farmPlaytest.playerPosition;
	output.farmPlaytest.playerGroundSample = farm::FarmTerrainQuerySystem::SampleGround(
		farmGrid_, farmVisualSystem_,
		{playerPosition.x + playerOffset.x, playerPosition.y, playerPosition.z + playerOffset.z}, {playerHalf.x, playerHalf.z});
	output.farmPlaytest.targetMoney = farmProgressionSystem_.GetTargetMoney();
	output.farmPlaytest.remainingMoney = farmProgressionSystem_.GetRemainingMoney(
		output.farmPlaytest.money);
	output.farmPlaytest.cropCount = farmEconomySystem_.GetTotalCropCount();
	output.farmPlaytest.cropSellPrice = farmEconomySystem_.GetSellPrice(selectedCrop);
	output.farmPlaytest.saleValue = farmEconomySystem_.GetSalePreviewValue();
	output.farmPlaytest.selectedSeedCrop = selectedCrop;
	output.farmPlaytest.seedCount = farmEconomySystem_.GetSeedCount(selectedCrop);
	output.farmPlaytest.seedPrice = farmEconomySystem_.GetSeedPrice(selectedCrop);
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		const std::size_t index = static_cast<std::size_t>(slot);
		const farm::CropType crop = farm::CropTypeFromSlot(slot);
		output.farmPlaytest.cropCounts[index] =
			farmEconomySystem_.GetCropCount(crop);
		output.farmPlaytest.cropValues[index] =
			farmEconomySystem_.GetCropInventoryValue(crop);
		output.farmPlaytest.seedCounts[index] =
			farmEconomySystem_.GetSeedCount(crop);
	}
	output.farmPlaytest.requiredCropCount = farmProgressionSystem_.GetRequiredCropCount(
		output.farmPlaytest.money, output.farmPlaytest.cropSellPrice);
	output.farmPlaytest.progress = farmProgressionSystem_.GetProgress(
		output.farmPlaytest.money, farmDateSystem_.GetDay());
	output.farmPlaytest.cleared = farmProgressionSystem_.IsCleared();
	output.farmPlaytest.inputLocked = output.farmPlaytest.cleared;
	output.farmPlaytest.feedbackMessage = farmFeedbackSystem_.GetCurrentMessage();
	output.farmPlaytest.lastFeedbackKind = farmFeedbackSystem_.GetLastKind();
	output.farmPlaytest.feedbackStats = farmFeedbackSystem_.GetStats();
	output.farmPlaytest.restartCount = farmRestartCount_;
	output.farmPlaytest.lastHarvestQuality =
		farmEconomySystem_.GetLastHarvestQuality();
	output.farmPlaytest.lastHarvestAdvice = FarmCropQualitySystem::Analyze(output.farmPlaytest.lastHarvestQuality);
	output.farmPlaytest.harvestRecordCount = farmEconomySystem_.GetHarvestRecordCount();
	output.farmPlaytest.unrecordedCropCount = farmEconomySystem_.GetUnrecordedCropCount();
	output.farmPlaytest.protectedCropCount = farmEconomySystem_.GetProtectedCropCount();
	output.farmPlaytest.inventoryGeneration = farmEconomySystem_.GetInventoryGeneration();
	output.farmPlaytest.canChangeHarvestProtection =
		!timelineScrubbing_ && !farmIrrigationPreviewSystem_.IsActive();
	output.farmPlaytest.contestReservationId = farmEconomySystem_.GetContestReservationId();
	output.farmPlaytest.activeHarvestVisuals = farmHarvestVisualSystem_.GetActiveCount(farmGrid_);
	output.farmPlaytest.contestJudge = FarmContestJudgeSystem::Evaluate(farmEconomySystem_.GetContestReservation());
	output.farmPlaytest.contestResults = farmEconomySystem_.GetContestResults();
	output.farmPlaytest.progressionMode = farmProgressionSystem_.GetMode();
	output.farmPlaytest.contestSeason = FarmContestSeasonSystem::Evaluate(farmDateSystem_.GetDay(),
		output.farmPlaytest.contestResults);
	output.farmPlaytest.contestSubmissionStatus = FarmContestSubmissionSystem::Evaluate(farmEconomySystem_,farmDateSystem_.GetDay());
	output.farmPlaytest.contestNoticeDay = farmContestDaySystem_.PendingDay();
	output.farmPlaytest.contestEntry = FarmContestEntrySystem::Evaluate(farmDateSystem_.GetDay(),
		farmEconomySystem_.GetContestReservation());
	for (std::size_t i = 0; i < output.farmPlaytest.harvestRecordCount; ++i) {
		output.farmPlaytest.harvestRecords[i] = *farmEconomySystem_.GetHarvestRecord(i);
		output.farmPlaytest.contestEligible[i] =
			FarmEconomySystem::CanReserveForContest(output.farmPlaytest.harvestRecords[i]);
	}

	output.visibility.selectedTarget = selectedTarget_;
	output.visibility.showTerrain = showTerrain_;
	output.visibility.showFarmMeshes = farmRenderer_.IsVisible();
	output.visibility.farmMeshesReady = farmRenderer_.IsReady();
	output.visibility.farmMeshLimitExceeded = farmRenderer_.IsLimitExceeded();
	output.visibility.farmMeshPartCount = farmRenderer_.GetPartCount();
	output.visibility.showSphere = showSphere_;
	output.visibility.showPlane = showPlane_;
	output.visibility.showSprite = showSprite_;
	output.visibility.showParticles = showParticles_;
	output.visibility.showAnimatedModel = showAnimModel_;
	output.visibility.showSkeleton = showSkeleton_;
	output.visibility.showLevelObjects = showLevelObjects_;
	output.visibility.showLevelGizmos = showLevelGizmos_;
	output.visibility.showCollisionGizmos = showLevelCollisionGizmos_;
	output.visibility.cullMode = cullMode_;
	output.camera = {
		cameraPos_,
		cameraRot_,
		cameraMoveSpeed_,
		cameraRotateSpeed_,
	};

	ObjectInspectorEditorViewData& objectView = output.objectInspector;
	objectView.directionalLight = {
		lightDirection_, lightColor_, lightIntensity_ };
	objectView.spotLight = {
		spotLightColor_,
		spotLightPos_,
		spotLightIntensity_,
		spotLightDir_,
		spotLightDistance_,
		spotLightDecay_,
		spotLightAngle_,
		spotLightFalloff_,
	};
	objectView.sphere = { sphereRadius_, spherePos_, objectRot_ };
	objectView.animation.modelIndex = currentAnimModelIdx_;
	objectView.animation.showModel = showAnimModel_;
	objectView.animation.hasObject = animObj_ != nullptr;
	if (animObj_) {
		objectView.animation.playing = animObj_->GetIsAnimationPlaying();
		objectView.animation.speed = animObj_->GetAnimationSpeed();
		objectView.animation.time = animObj_->GetAnimationTime();
		objectView.animation.duration = animObj_->GetAnimation().duration;
		const std::optional<Skeleton>& skeleton = animObj_->GetSkeleton();
		if (skeleton.has_value()) {
			objectView.joints.resize(skeleton->joints.size());
			for (std::size_t index = 0; index < skeleton->joints.size(); ++index) {
				const Joint& joint = skeleton->joints[index];
				JointEditorViewData& destination = objectView.joints[index];
				destination.name = joint.name;
				destination.jointIndex = static_cast<int>(index);
				destination.translate = joint.transform.translate;
				destination.scale = joint.transform.scale;
			}
		}
	}
	objectView.cylinder = {
		cylTopRadius_,
		cylBottomRadius_,
		cylHeight_,
		cylSegments_,
		cylVertDivisions_,
	};

	output.particles.activeParticleType = activeParticleType_;
	output.particles.gpuMode = ToEditorGpuParticleMode(gpuParticleDebugMode_);
	output.particles.agriculture = {
		agricultureEmitPosition_,
		agricultureParticleSize_,
		agricultureParticleCount_,
		agricultureShowKeyGuide_,
	};
	output.particles.interaction = {
		interactionGridCount_,
		interactionParticleSize_,
		interactionBrushRadius_,
		interactionBrushStrength_,
		interactionDamping_,
		interactionBrushPosition_,
		interactionParticleCount_,
	};
	if (framework_ && framework_->GetParticleManager()) {
		output.particles.alphaReference = framework_->GetParticleManager()->GetAlphaReference();
	}
	if (framework_ && framework_->GetFrameClock()) {
		const FrameClock* frameClock = framework_->GetFrameClock();
		output.simulation.paused = frameClock->IsPaused();
		output.simulation.simulationTick = frameClock->GetSimulationTick();
	}
}

bool GamePlaySessionSystem::Execute(const SimulationEditorCommand& command) {
	if (!framework_) {
		return false;
	}
	FrameClock* frameClock = framework_->GetFrameClock();
	if (!frameClock) {
		return false;
	}

	switch (command.action) {
	case SimulationEditorAction::Play:
		frameClock->SetPaused(false);
		return true;
	case SimulationEditorAction::Pause:
		frameClock->SetPaused(true);
		return true;
	case SimulationEditorAction::Step:
		if (!frameClock->IsPaused()) {
			return false;
		}
		frameClock->RequestSingleStep();
		return true;
	default:
		return false;
	}
}

bool GamePlaySessionSystem::Execute(const GamePlayEditorCommand& command) {
	if (!framework_ || command.farmGeneration != farmGrid_.GetGeneration()) {
		return false;
	}
	if (farmProgressionSystem_.IsCleared()) {
		switch (command.type) {
		case GamePlayEditorCommandType::SetFarmProgressionMode:
		case GamePlayEditorCommandType::RestartFarmSession:
		case GamePlayEditorCommandType::SelectFarmTile:
		case GamePlayEditorCommandType::SelectFarmTileAtViewport:
		case GamePlayEditorCommandType::StopFarmComparison:
		case GamePlayEditorCommandType::ResetFarmComparison:
		case GamePlayEditorCommandType::CancelFarmIrrigationPreview: break;
		default: return false;
		}
	}

	switch (command.type) {
	case GamePlayEditorCommandType::SetFarmProgressionMode:
		if (timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive()) return false;
		if (!farmProgressionSystem_.SetMode(command.progressionMode, farmEconomySystem_.GetMoney(),
			FarmContestSeasonSystem::Evaluate(farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults()))) return false;
		farmToolActionSystem_.ClearHistory();
		farmGrowthComparisonSystem_.Stop();
		farmContestDaySystem_.Reset();
		InitializeTimeline();
		farmDocumentSystem_.MarkDirty();
		return true;
	case GamePlayEditorCommandType::PinFarmComparisonA:
	case GamePlayEditorCommandType::PinFarmComparisonB:
		if (farmGameMode_ || timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive()) return false;
		return farmGrowthComparisonSystem_.Pin(farmGrid_,
			command.type == GamePlayEditorCommandType::PinFarmComparisonA ? 0 : 1, command.farmTileIndex);
	case GamePlayEditorCommandType::StartFarmComparison:
		if (farmGameMode_ || timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive() ||
			farmProgressionSystem_.IsCleared()) return false;
		return farmGrowthComparisonSystem_.Start(farmGrid_);
	case GamePlayEditorCommandType::StopFarmComparison:
		farmGrowthComparisonSystem_.ObserveBeforeStep(farmGrid_);
		return farmGrowthComparisonSystem_.Stop();
	case GamePlayEditorCommandType::ResetFarmComparison:
		farmGrowthComparisonSystem_.Reset();
		return true;
	case GamePlayEditorCommandType::SelectFarmTile:
		farmIrrigationPreviewSystem_.Cancel();
		return SelectCommandTarget(command);
	case GamePlayEditorCommandType::SelectFarmTileAtViewport:
		farmIrrigationPreviewSystem_.Cancel();
		return TrySelectFarmTileFromViewport();
	case GamePlayEditorCommandType::SelectFarmTool:
		if (command.farmTool == FarmTool::BugNet) {
			return false;
		}
		farmToolSystem_.SetTool(command.farmTool);
		return true;
	case GamePlayEditorCommandType::ApplyCurrentFarmTool: {
		if (!SelectCommandTarget(command)) {
			return false;
		}
		const auto result = farmToolActionSystem_.ApplyToolDetailed(
			farmGrid_, farmToolSystem_.GetCurrentTool(),
			farmCropSelectionSystem_.GetSelectedCrop(),
			farmEconomySystem_, farmDateSystem_.GetDay());
		RouteFarmToolFeedback(result);
		if (result.Succeeded()) {
			farmIrrigationPreviewSystem_.Cancel();
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	}
	case GamePlayEditorCommandType::ApplyFarmTool: {
		if (!SelectCommandTarget(command)) {
			return false;
		}
		farmToolSystem_.SetTool(command.farmTool);
		const auto result = farmToolActionSystem_.ApplyToolDetailed(
			farmGrid_, command.farmTool,
			farmCropSelectionSystem_.GetSelectedCrop(),
			farmEconomySystem_, farmDateSystem_.GetDay());
		RouteFarmToolFeedback(result);
		if (result.Succeeded()) {
			farmIrrigationPreviewSystem_.Cancel();
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	}
	case GamePlayEditorCommandType::CompostFarmTile:
		if (farmProgressionSystem_.IsCleared() || farmIrrigationPreviewSystem_.IsActive() ||
			!SelectCommandTarget(command)) return false;
		if (!farmToolActionSystem_.CompostSelectedTile(farmGrid_)) return false;
		farmDocumentSystem_.MarkDirty();
		return true;
	case GamePlayEditorCommandType::SubmitContestHarvest:
		if (timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive() ||
			command.contestDay != farmDateSystem_.GetDay()) return false;
		if (!farmToolActionSystem_.CommitContestSubmission(farmEconomySystem_, command.contestDay,
			command.harvestRecordId, command.inventoryGeneration)) return false;
		// Submission is a commit boundary for both edit undo and timeline rewind.
		static_cast<void>(farmProgressionSystem_.EvaluateSeason(FarmContestSeasonSystem::Evaluate(
			farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults())));
		InitializeTimeline();
		farmContestDaySystem_.Observe(farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults(), !farmProgressionSystem_.IsFreeFarming());
		farmDocumentSystem_.MarkDirty();
		return true;
	case GamePlayEditorCommandType::AcknowledgeContestDay:
		if (command.contestDay != farmDateSystem_.GetDay()) return false;
		return farmContestDaySystem_.Acknowledge(command.contestDay);
	case GamePlayEditorCommandType::SetHarvestProtection:
		if (timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive()) return false;
		if (!farmToolActionSystem_.CommitHarvestProtection(farmEconomySystem_,
			command.harvestRecordId, command.harvestProtected, command.inventoryGeneration)) return false;
		farmDocumentSystem_.MarkDirty();
		return true;
	case GamePlayEditorCommandType::ReserveContestHarvest:
	case GamePlayEditorCommandType::CancelContestReservation:
		if (timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive()) return false;
		if (!farmToolActionSystem_.CommitContestReservation(farmEconomySystem_,
			command.harvestRecordId, command.type == GamePlayEditorCommandType::ReserveContestHarvest,
			command.inventoryGeneration)) return false;
		farmDocumentSystem_.MarkDirty();
		return true;
	case GamePlayEditorCommandType::RaiseFarmTile:
		if (SelectCommandTarget(command) && farmToolActionSystem_.RaiseSelectedTile(farmGrid_)) {
			farmIrrigationPreviewSystem_.Cancel();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::LowerFarmTile:
		if (SelectCommandTarget(command) && farmToolActionSystem_.LowerSelectedTile(farmGrid_)) {
			farmIrrigationPreviewSystem_.Cancel();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::UndoFarmEdit:
		if (farmToolActionSystem_.Undo()) {
			farmHarvestVisualSystem_.Clear();
			farmIrrigationPreviewSystem_.Cancel();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::RedoFarmEdit:
		if (farmToolActionSystem_.Redo()) {
			farmHarvestVisualSystem_.Clear();
			farmIrrigationPreviewSystem_.Cancel();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::SetFarmIrrigation:
		if (farmIrrigationPreviewSystem_.IsActive() || !SelectCommandTarget(command)) return false;
		if (!farmToolActionSystem_.CanSetIrrigation(farmGrid_, command.farmTileIndex)) return false;
		if (farmGrid_.GetTile(command.farmTileIndex)->irrigationEnabled == command.irrigationEnabled) return true;
		if (!farmToolActionSystem_.SetSelectedIrrigation(farmGrid_, command.irrigationEnabled)) return false;
		farmIrrigationSystem_.Rebuild(farmGrid_);
		farmDocumentSystem_.MarkDirty();
		return true;
	case GamePlayEditorCommandType::ToggleFarmCanal:
		if (SelectCommandTarget(command) &&
			farmToolActionSystem_.ToggleSelectedCanal(farmGrid_)) {
			farmIrrigationPreviewSystem_.Cancel();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::ToggleFarmWaterSource:
		if (SelectCommandTarget(command) &&
			farmToolActionSystem_.ToggleSelectedWaterSource(farmGrid_)) {
			farmIrrigationPreviewSystem_.Cancel();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			farmDocumentSystem_.MarkDirty();
			return true;
		}
		return false;
	case GamePlayEditorCommandType::BeginFarmCanalPreview:
		return SelectCommandTarget(command) &&
			farmToolActionSystem_.CanToggleCanal(farmGrid_, command.farmTileIndex) &&
			farmIrrigationPreviewSystem_.Begin(
				farmGrid_, command.farmTileIndex,
				farm::FarmIrrigationPreviewOperation::ToggleCanal);
	case GamePlayEditorCommandType::BeginFarmWaterSourcePreview:
		return SelectCommandTarget(command) &&
			farmToolActionSystem_.CanToggleWaterSource(farmGrid_, command.farmTileIndex) &&
			farmIrrigationPreviewSystem_.Begin(
				farmGrid_, command.farmTileIndex,
				farm::FarmIrrigationPreviewOperation::ToggleWaterSource);
	case GamePlayEditorCommandType::BeginFarmRaiseTerrainPreview:
		return SelectCommandTarget(command) &&
			farmIrrigationPreviewSystem_.Begin(
				farmGrid_, command.farmTileIndex,
				farm::FarmIrrigationPreviewOperation::RaiseTerrain);
	case GamePlayEditorCommandType::BeginFarmLowerTerrainPreview:
		return SelectCommandTarget(command) &&
			farmIrrigationPreviewSystem_.Begin(
				farmGrid_, command.farmTileIndex,
				farm::FarmIrrigationPreviewOperation::LowerTerrain);
	case GamePlayEditorCommandType::BeginFarmCanalPathPreview:
		return SelectCommandTarget(command) &&
			farmIrrigationPreviewSystem_.Begin(
				farmGrid_, command.farmTileIndex,
				farm::FarmIrrigationPreviewOperation::PlaceCanalPath);
	case GamePlayEditorCommandType::AppendFarmCanalPathPreview:
		return farmIrrigationPreviewSystem_.VisitCanalPathTile(
			farmGrid_, command.farmTileIndex);
	case GamePlayEditorCommandType::AppendFarmTerrainPreview:
		return farmIrrigationPreviewSystem_.VisitTerrainTile(farmGrid_, command.farmTileIndex);
	case GamePlayEditorCommandType::EndFarmTerrainStroke:
		farmIrrigationPreviewSystem_.EndTerrainStroke();
		return true;
	case GamePlayEditorCommandType::BeginFarmCanalRemovalPathPreview:
		return SelectCommandTarget(command) &&
			farmIrrigationPreviewSystem_.Begin(
				farmGrid_, command.farmTileIndex,
				farm::FarmIrrigationPreviewOperation::RemoveCanalPath);
	case GamePlayEditorCommandType::ConfirmFarmIrrigationPreview: {
		if (!farmIrrigationPreviewSystem_.CanConfirm(farmGrid_) ||
			command.farmTileIndex != farmIrrigationPreviewSystem_.GetTileIndex() ||
			!SelectCommandTarget(command)) {
			return false;
		}
		const farm::FarmIrrigationPreviewOperation operation =
			farmIrrigationPreviewSystem_.GetOperation();
		bool changed = false;
		if (operation == farm::FarmIrrigationPreviewOperation::ToggleCanal) {
			changed = farmToolActionSystem_.ToggleSelectedCanal(farmGrid_);
		} else if (operation == farm::FarmIrrigationPreviewOperation::ToggleWaterSource) {
			changed = farmToolActionSystem_.ToggleSelectedWaterSource(farmGrid_);
		} else if (operation == farm::FarmIrrigationPreviewOperation::RaiseTerrain) {
			changed = farmToolActionSystem_.ChangeTerrainBatch(farmGrid_,
				farmIrrigationPreviewSystem_.GetChangedTileIndices(), 1);
		} else if (operation == farm::FarmIrrigationPreviewOperation::LowerTerrain) {
			changed = farmToolActionSystem_.ChangeTerrainBatch(farmGrid_,
				farmIrrigationPreviewSystem_.GetChangedTileIndices(), -1);
		} else if (operation == farm::FarmIrrigationPreviewOperation::PlaceCanalPath) {
			changed = farmToolActionSystem_.PlaceCanalPath(
				farmGrid_, farmIrrigationPreviewSystem_.GetChangedTileIndices());
		} else if (operation == farm::FarmIrrigationPreviewOperation::RemoveCanalPath) {
			changed = farmToolActionSystem_.RemoveCanalPath(
				farmGrid_, farmIrrigationPreviewSystem_.GetChangedTileIndices());
		}
		if (!changed) {
			return false;
		}
		farmIrrigationPreviewSystem_.Cancel();
		farmIrrigationSystem_.Rebuild(farmGrid_);
		farmDocumentSystem_.MarkDirty();
		return true;
	}
	case GamePlayEditorCommandType::CancelFarmIrrigationPreview:
		if (!farmIrrigationPreviewSystem_.IsActive()) {
			return false;
		}
		farmIrrigationPreviewSystem_.Cancel();
		return true;
	case GamePlayEditorCommandType::RestartFarmSession:
		return ResetFarmSession();
	case GamePlayEditorCommandType::MovePlayerToSelectedFarmTile:
		return command.farmTileIndex == farmGrid_.GetSelectedIndex() && MovePlayerToFarmTile(command.farmTileIndex);
	default:
		return false;
	}
}

bool GamePlaySessionSystem::Execute(const FarmDocumentCommand& command) {
	if (!framework_) {
		return false;
	}

	bool executed = false;
	switch (command.type) {
	case FarmDocumentCommandType::NewDocument:
		executed = farmDocumentSystem_.Reset(
			farmGrid_, farmEconomySystem_,
			farmCropSelectionSystem_);
		break;
	case FarmDocumentCommandType::Load:
		executed = farmDocumentSystem_.Load(
			command.documentId, farmGrid_, farmEconomySystem_,
			farmCropSelectionSystem_);
		break;
	case FarmDocumentCommandType::Save:
		return farmDocumentSystem_.Save(
			farmGrid_, farmEconomySystem_,
			farmCropSelectionSystem_);
	case FarmDocumentCommandType::SaveAs:
		return farmDocumentSystem_.SaveAs(
			command.displayName, farmGrid_, farmEconomySystem_,
			farmCropSelectionSystem_);
	case FarmDocumentCommandType::Rename:
		return farmDocumentSystem_.Rename(command.documentId, command.displayName);
	case FarmDocumentCommandType::Delete:
		return farmDocumentSystem_.Delete(command.documentId);
	default:
		return false;
	}

	if (executed) {
		farmToolActionSystem_.ClearHistory();
		if (command.type == FarmDocumentCommandType::NewDocument ||
			command.type == FarmDocumentCommandType::Load) {
			farmContestDaySystem_.Reset();
			farmIrrigationSystem_.Rebuild(farmGrid_);
			InitializeTimeline();
			farmFeedbackSystem_.Clear();
		}
	}
	return executed;
}

bool GamePlaySessionSystem::Execute(const VisibilityEditorCommand& command) {
	if (!framework_ || !command.apply) {
		return false;
	}
	selectedTarget_ = std::clamp(command.value.selectedTarget, 0, 4);
	showTerrain_ = command.value.showTerrain;
	farmRenderer_.SetVisible(command.value.showFarmMeshes);
	showSphere_ = command.value.showSphere;
	showPlane_ = command.value.showPlane;
	showSprite_ = command.value.showSprite;
	showParticles_ = command.value.showParticles;
	showAnimModel_ = command.value.showAnimatedModel;
	showSkeleton_ = command.value.showSkeleton;
	showLevelObjects_ = command.value.showLevelObjects;
	showLevelGizmos_ = command.value.showLevelGizmos;
	showLevelCollisionGizmos_ = command.value.showCollisionGizmos;
	cullMode_ = std::clamp(command.value.cullMode, 0, 2);
	return true;
}

bool GamePlaySessionSystem::Execute(const CameraEditorCommand& command) {
	if (!framework_) {
		return false;
	}
	if (command.reset) {
		ResetCamera();
		return true;
	}
	if (!command.apply) {
		return false;
	}
	cameraPos_ = command.value.position;
	cameraRot_ = command.value.rotation;
	cameraMoveSpeed_ = std::clamp(command.value.moveSpeed, 0.0f, 100.0f);
	cameraRotateSpeed_ = std::clamp(command.value.rotateSpeed, 0.0f, 10.0f);
	return true;
}

bool GamePlaySessionSystem::Execute(const ObjectInspectorEditorCommand& command) {
	if (!framework_) {
		return false;
	}
	bool executed = false;
	if (command.directionalLightChanged) {
		lightDirection_ = NormalizeOrKeep(command.directionalLight.direction, lightDirection_);
		lightColor_ = command.directionalLight.color;
		lightIntensity_ = std::clamp(command.directionalLight.intensity, 0.0f, 10.0f);
		executed = true;
	}
	if (command.spotLightChanged) {
		spotLightColor_ = command.spotLight.color;
		spotLightPos_ = command.spotLight.position;
		spotLightIntensity_ = std::clamp(command.spotLight.intensity, 0.0f, 20.0f);
		spotLightDir_ = NormalizeOrKeep(command.spotLight.direction, spotLightDir_);
		spotLightDistance_ = std::clamp(command.spotLight.distance, 1.0f, 100.0f);
		spotLightDecay_ = std::clamp(command.spotLight.decay, 0.1f, 10.0f);
		spotLightAngle_ = std::clamp(command.spotLight.angleRadians, 0.0f, 1.5707963f);
		spotLightFalloff_ = std::clamp(command.spotLight.falloffRadians, 0.0f, 1.5707963f);
		executed = true;
	}
	if (command.sphereGeometryChanged) {
		sphereRadius_ = std::clamp(command.sphere.radius, 0.1f, 10.0f);
		CreateSphere(sphereRadius_);
		executed = true;
	}
	if (command.sphereTransformChanged) {
		spherePos_ = command.sphere.position;
		objectRot_ = command.sphere.rotation;
		executed = true;
	}
	if (command.animationModelChanged) {
		const int modelIndex = std::clamp(command.animation.modelIndex, 0, 3);
		currentAnimModelIdx_ = modelIndex;
		ChangeAnimationModel(modelIndex);
		executed = true;
	}
	if (command.animationVisibilityChanged) {
		showAnimModel_ = command.animation.showModel;
		executed = true;
	}
	if (command.animationPlaybackChanged && animObj_) {
		animObj_->SetAnimationPlaying(command.animation.playing);
		animObj_->SetPlaybackSpeed(std::clamp(command.animation.speed, -5.0f, 5.0f));
		const float duration = animObj_->GetAnimation().duration;
		animObj_->SetAnimationTime(duration > 0.0f
			? std::clamp(command.animation.time, 0.0f, duration)
			: 0.0f);
		executed = true;
	}
	if (command.cylinderChanged) {
		cylTopRadius_ = std::clamp(command.cylinder.topRadius, 0.0f, 5.0f);
		cylBottomRadius_ = std::clamp(command.cylinder.bottomRadius, 0.0f, 5.0f);
		cylHeight_ = std::clamp(command.cylinder.height, 0.1f, 10.0f);
		cylSegments_ = std::clamp(command.cylinder.segments, 3, 64);
		cylVertDivisions_ = std::clamp(command.cylinder.verticalDivisions, 1, 16);
		RebuildCylinder();
		executed = true;
	}
	if (command.jointChanged && animObj_) {
		const std::optional<Skeleton>& skeleton = animObj_->GetSkeleton();
		if (skeleton.has_value() && command.joint.jointIndex >= 0 &&
			command.joint.jointIndex < static_cast<int>(skeleton->joints.size())) {
            animObj_->SetJointTranslationScale(static_cast<std::size_t>(command.joint.jointIndex),
                command.joint.translate, {
                    (std::max)(command.joint.scale.x, 0.01f),
                    (std::max)(command.joint.scale.y, 0.01f),
                    (std::max)(command.joint.scale.z, 0.01f)});
			executed = true;
		}
	}
	return executed;
}

bool GamePlaySessionSystem::Execute(const ParticleEditorCommand& command) {
	if (!framework_) {
		return false;
	}
	ParticleManager* particleManager = framework_ ? framework_->GetParticleManager() : nullptr;
	bool executed = false;
	if (command.activeParticleTypeChanged) {
		activeParticleType_ = std::clamp(command.activeParticleType, 0, 4);
		executed = true;
	}
	if (command.gpuModeChanged) {
		SetGPUParticleDebugMode(ToSceneGpuParticleMode(command.gpuMode));
		executed = true;
	}
	if (command.agricultureSettingsChanged) {
		agricultureEmitPosition_ = command.agriculture.emitPosition;
		agricultureParticleSize_ = std::clamp(command.agriculture.particleSize, 0.02f, 1.0f);
		agricultureParticleCount_ = std::clamp(command.agriculture.particleCount, 1, 1024);
		agricultureShowKeyGuide_ = command.agriculture.showKeyGuide;
		executed = true;
	}
	if (command.emitAgricultureParticle) {
		EmitAgricultureParticle(ToSceneAgricultureParticleType(command.agricultureParticleType));
		executed = true;
	}
	if (command.interactionSettingsChanged) {
		const int previousGridCount = interactionGridCount_;
		const float previousParticleSize = interactionParticleSize_;
		interactionGridCount_ = std::clamp(command.interaction.gridCount, 2, 10);
		interactionParticleSize_ = std::clamp(command.interaction.particleSize, 0.02f, 0.05f);
		interactionBrushRadius_ = std::clamp(command.interaction.brushRadius, 0.1f, 10.0f);
		interactionBrushStrength_ = std::clamp(command.interaction.brushStrength, 0.0f, 10.0f);
		interactionDamping_ = std::clamp(command.interaction.damping, 0.80f, 0.995f);
		interactionParticleCount_ = CalculateInteractionParticleCount();
		if (interactionGridCount_ != previousGridCount ||
			interactionParticleSize_ != previousParticleSize) {
			interactionResetRequested_ = true;
		}
		executed = true;
	}
	if (command.resetInteractionGrid && particleManager) {
		particleManager->ResetGPUParticles();
		interactionResetRequested_ = true;
		executed = true;
	}
	if (command.alphaReferenceChanged && particleManager) {
		particleManager->SetAlphaReference(std::clamp(command.alphaReference, 0.0f, 1.0f));
		executed = true;
	}
	if (command.clearAll && particleManager) {
		particleManager->ClearAll();
		executed = true;
	}
	return executed;
}

void GamePlaySessionSystem::SetViewportState(
	const Vector2& imageTopLeft,
	const Vector2& imageSize,
	const Vector2& mousePosition,
	bool hovered,
	bool focused) noexcept {
	if (!framework_) {
		return;
	}
	viewportImageTopLeft_ = imageTopLeft;
	viewportImageSize_ = imageSize;
	viewportMousePosition_ = mousePosition;
	viewportHovered_ = hovered;
	viewportFocused_ = focused;
}

Object3d* GamePlaySessionSystem::GetAnimationObjectForDebug() const noexcept {
	return framework_ ? animObj_.get() : nullptr;
}

bool GamePlaySessionSystem::SelectCommandTarget(const GamePlayEditorCommand& command) {
	return farmGrid_.SetSelectedIndex(command.farmTileIndex);
}

