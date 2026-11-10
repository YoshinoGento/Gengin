#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmContestJudgeSystem.h"
#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmContestSeasonSystem.h"
#include "farm/system/FarmFeedbackSystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmDateSystem.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/system/FarmTerrainQuerySystem.h"
#include "math/Struct.h"

#include <array>
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

class Object3d;

namespace farm {
class FarmGrid;
}

namespace editor {

struct FarmTileEditorViewData {
	int index = -1;
	int column = 0;
	int row = 0;
	int heightLevel = 0;
	farm::FarmTileFeature feature = farm::FarmTileFeature::None;
	farm::FarmTileState state = farm::FarmTileState::Empty;
	farm::CropType crop = farm::CropType::None;
	farm::FarmCropGrowthStage growthStage = farm::FarmCropGrowthStage::None;
	float moisture = 0.0f;
	float growth = 0.0f;
	farm::FarmCropCareHistory careHistory{};
	float soilNutrients = 0.0f;
	float nutrientTarget = 0.0f;
	bool canCompost = false;
	FarmGrowthForecast growthForecast{};
	FarmWaterGuidance waterGuidance{};
	FarmCropQualityResult quality{};
	FarmQualityAdvice qualityAdvice{};
	bool canHoe = false;
	bool canWater = false;
	bool canSeed = false;
	bool canHarvest = false;
	bool canToggleCanal = false;
	bool irrigationEnabled = true;
	bool canSetIrrigation = false;
	bool canToggleWaterSource = false;
	bool irrigationSupplied = false;
	bool irrigationInRange = false;
	float irrigationSupplyStrength = 0.0f;
	float storedWater = 0.0f;
	farm::FarmWaterStatus waterStatus = farm::FarmWaterStatus::None;
	farm::FarmIrrigationTileFlow irrigationFlow{};
	float irrigationStrength = 0.0f;
	int irrigationDownstreamCanalCount = 0;
	int irrigationSupplierTileIndex = -1;
	bool irrigationPreviewChanged = false;
};

struct VisibilityEditorViewData {
	bool showFarmMeshes = true;
	bool farmMeshesReady = false;
	bool farmMeshLimitExceeded = false;
	int farmMeshPartCount = 0;
	int selectedTarget = 0;
	bool showTerrain = true;
	bool showSphere = true;
	bool showPlane = true;
	bool showSprite = true;
	bool showParticles = true;
	bool showAnimatedModel = true;
	bool showSkeleton = false;
	bool showLevelObjects = true;
	bool showLevelGizmos = false;
	bool showCollisionGizmos = false;
	int cullMode = 2;
};

struct CameraEditorViewData {
	Vector3 position{};
	Vector3 rotation{};
	float moveSpeed = 0.0f;
	float rotateSpeed = 0.0f;
};

struct DirectionalLightEditorViewData {
	Vector3 direction{};
	Vector3 color{};
	float intensity = 0.0f;
};

struct SpotLightEditorViewData {
	Vector3 color{};
	Vector3 position{};
	float intensity = 0.0f;
	Vector3 direction{};
	float distance = 0.0f;
	float decay = 0.0f;
	float angleRadians = 0.0f;
	float falloffRadians = 0.0f;
};

struct SphereEditorViewData {
	float radius = 1.0f;
	Vector3 position{};
	Vector3 rotation{};
};

struct AnimationEditorViewData {
	int modelIndex = 0;
	bool showModel = true;
	bool hasObject = false;
	bool playing = false;
	float speed = 1.0f;
	float time = 0.0f;
	float duration = 0.0f;
};

struct CylinderEditorViewData {
	float topRadius = 1.0f;
	float bottomRadius = 1.0f;
	float height = 2.0f;
	int segments = 32;
	int verticalDivisions = 4;
};

struct JointEditorViewData {
	std::string name;
	int jointIndex = -1;
	Vector3 translate{};
	Vector3 scale{ 1.0f, 1.0f, 1.0f };
};

struct ObjectInspectorEditorViewData {
	DirectionalLightEditorViewData directionalLight;
	SpotLightEditorViewData spotLight;
	SphereEditorViewData sphere;
	AnimationEditorViewData animation;
	CylinderEditorViewData cylinder;
	std::vector<JointEditorViewData> joints;
};

enum class EditorGpuParticleMode {
	Off,
	Agriculture,
	Interaction,
};

enum class EditorAgricultureParticleType {
	DirtDust,
	WaterSplash,
	HarvestSparkle,
};

struct AgricultureParticleEditorViewData {
	Vector3 emitPosition{};
	float particleSize = 0.25f;
	int particleCount = 128;
	bool showKeyGuide = true;
};

struct InteractionParticleEditorViewData {
	int gridCount = 8;
	float particleSize = 0.03f;
	float brushRadius = 1.5f;
	float brushStrength = 1.0f;
	float damping = 0.95f;
	Vector3 brushPosition{};
	uint32_t particleCount = 0;
};

struct ParticleEditorViewData {
	int activeParticleType = 0;
	EditorGpuParticleMode gpuMode = EditorGpuParticleMode::Off;
	AgricultureParticleEditorViewData agriculture;
	InteractionParticleEditorViewData interaction;
	float alphaReference = 0.0f;
};

struct SimulationEditorViewData {
	bool paused = false;
	uint64_t simulationTick = 0;
};

struct FarmPlaytestEditorViewData {
	FarmDateSystem::Snapshot clock{};
	float dayLengthSeconds = 0.0f;
	Vector3 playerPosition{};
	bool playerGrounded = false;
	bool canPlacePlayer = false;
	bool playerMovementBlocked = false;
	farm::FarmGroundSample playerGroundSample{};
	int money = 0;
	int targetMoney = 1;
	int remainingMoney = 1;
	int cropCount = 0;
	int cropSellPrice = 0;
	int saleValue = 0;
	int seedCount = 0;
	int seedPrice = 0;
	std::array<int, farm::kFarmCropTypeCount> cropCounts{};
	std::array<int, farm::kFarmCropTypeCount> cropValues{};
	std::array<int, farm::kFarmCropTypeCount> seedCounts{};
	farm::CropType selectedSeedCrop = farm::CropType::Carrot;
	int requiredCropCount = -1;
	float progress = 0.0f;
	bool cleared = false;
	bool inputLocked = false;
	std::string feedbackMessage;
	FarmFeedbackKind lastFeedbackKind = FarmFeedbackKind::None;
	FarmFeedbackStats feedbackStats{};
	std::uint32_t restartCount = 0;
	FarmCropQualityResult lastHarvestQuality{};
	FarmEconomySystem::HarvestRecords harvestRecords{};
	std::size_t harvestRecordCount = 0;
	int unrecordedCropCount = 0;
	int protectedCropCount = 0;
	uint64_t inventoryGeneration = 0;
	bool canChangeHarvestProtection = false;
	int contestReservationId = 0;
	std::size_t activeHarvestVisuals = 0;
	FarmContestJudgeResult contestJudge{};
	FarmContestEntryResult contestEntry{};
	FarmEconomySystem::ContestResults contestResults{};
	FarmContestSeasonSummary contestSeason{};
	FarmProgressionMode progressionMode = FarmProgressionMode::Trial;
	FarmContestSubmissionStatus contestSubmissionStatus = FarmContestSubmissionStatus::NotContestDay;
	int contestNoticeDay = 0;
	std::array<bool, FarmEconomySystem::kMaxHarvestRecords> contestEligible{};
	FarmQualityAdvice lastHarvestAdvice{};
};

struct FarmDocumentEditorViewData {
	std::string id;
	std::string displayName;
	std::string savedAt;
	bool active = false;
};

struct GamePlayEditorViewModel {
	int farmWidth = 0;
	int farmHeight = 0;
	int irrigationWaterSourceCount = 0;
	int irrigationSuppliedCanalCount = 0;
	int irrigationRangeTileCount = 0;
	farm::FarmIrrigationStepSummary irrigationLastStep{};
	bool irrigationPreviewActive = false;
	bool irrigationPreviewCanConfirm = false;
	int irrigationPreviewTileIndex = -1;
	farm::FarmIrrigationPreviewOperation irrigationPreviewOperation =
		farm::FarmIrrigationPreviewOperation::None;
	farm::FarmTileFeature irrigationPreviewOriginalFeature = farm::FarmTileFeature::None;
	farm::FarmTileFeature irrigationPreviewCandidateFeature = farm::FarmTileFeature::None;
	int irrigationPreviewOriginalHeightLevel = 0;
	int irrigationPreviewCandidateHeightLevel = 0;
	int irrigationPreviewChangeCount = 0;
	farm::FarmCanalPathIssue irrigationPathIssue = farm::FarmCanalPathIssue::None;
	int irrigationBlockedTileIndex = -1;
	int irrigationPreviewWaterSourceCount = 0;
	int irrigationPreviewSuppliedCanalCount = 0;
	int irrigationPreviewRangeTileCount = 0;
	int selectedFarmTileIndex = -1;
	uint64_t farmGeneration = 0;
	bool farmDocumentDirty = false;
	bool farmDocumentExists = false;
	bool farmDocumentHasError = false;
	FarmDocumentStatus farmDocumentStatus = FarmDocumentStatus::Ready;
	std::string farmDocumentActiveId;
	std::string farmDocumentName;
	std::string farmDocumentMessage;
	std::vector<FarmDocumentEditorViewData> farmDocuments;
	std::vector<FarmTileEditorViewData> farmTiles;
	bool canUndo = false;
	bool canRedo = false;
	std::size_t undoCount = 0;
	std::size_t redoCount = 0;
	std::string undoName;
	std::string redoName;
	FarmTool currentFarmTool = FarmTool::Hoe;
	FarmToolActionResult selectedFarmAction{};
	FarmPlaytestEditorViewData farmPlaytest;
	FarmComparisonView farmGrowthComparison;
	FarmTimeAdvanceControlSystem::View farmTimeAdvance;
	bool farmTimeAdvanceBlocked = true;
	VisibilityEditorViewData visibility;
	CameraEditorViewData camera;
	ObjectInspectorEditorViewData objectInspector;
	ParticleEditorViewData particles;
	SimulationEditorViewData simulation;
};

enum class SimulationEditorAction {
	Play,
	Pause,
	Step,
};

struct SimulationEditorCommand {
	SimulationEditorAction action = SimulationEditorAction::Play;
};

enum class GamePlayEditorCommandType {
	SelectFarmTile,
	SelectFarmTileAtViewport,
	SelectFarmTool,
	ApplyCurrentFarmTool,
	ApplyFarmTool,
	RaiseFarmTile,
	CompostFarmTile,
	SetHarvestProtection,
	ReserveContestHarvest,
	CancelContestReservation,
	SubmitContestHarvest,
	AcknowledgeContestDay,
	SetFarmProgressionMode,
	LowerFarmTile,
	ToggleFarmCanal,
	SetFarmIrrigation,
	ToggleFarmWaterSource,
	BeginFarmCanalPreview,
	BeginFarmWaterSourcePreview,
	BeginFarmRaiseTerrainPreview,
	BeginFarmLowerTerrainPreview,
	BeginFarmCanalPathPreview,
	BeginFarmCanalRemovalPathPreview,
	AppendFarmCanalPathPreview,
	AppendFarmTerrainPreview,
	EndFarmTerrainStroke,
	ConfirmFarmIrrigationPreview,
	CancelFarmIrrigationPreview,
	UndoFarmEdit,
	RedoFarmEdit,
	RestartFarmSession,
	MovePlayerToSelectedFarmTile,
	PinFarmComparisonA,
	PinFarmComparisonB,
	StartFarmComparison,
	StopFarmComparison,
	ResetFarmComparison,
	ControlFarmTimeAdvance,
	ExpandFarm,
};

struct GamePlayEditorCommand {
	GamePlayEditorCommandType type = GamePlayEditorCommandType::SelectFarmTile;
	FarmTimeAdvanceControlSystem::Action timeAdvanceAction = FarmTimeAdvanceControlSystem::Action::Cancel;
	int farmTileIndex = -1;
	uint64_t farmGeneration = 0;
	FarmTool farmTool = FarmTool::Hoe;
	int expectedFarmHeight = 0;
	int harvestRecordId = 0;
	int contestDay = 0;
	FarmProgressionMode progressionMode = FarmProgressionMode::Trial;
	uint64_t inventoryGeneration = 0;
	bool harvestProtected = false;
	bool irrigationEnabled = true;
};

enum class FarmDocumentCommandType {
	NewDocument,
	Load,
	Save,
	SaveAs,
	Rename,
	Delete,
};

struct FarmDocumentCommand {
	FarmDocumentCommandType type = FarmDocumentCommandType::Save;
	std::string documentId;
	std::string displayName;
};

struct VisibilityEditorCommand {
	bool apply = false;
	VisibilityEditorViewData value;
};

struct CameraEditorCommand {
	bool apply = false;
	bool reset = false;
	CameraEditorViewData value;
};

struct ObjectInspectorEditorCommand {
	bool directionalLightChanged = false;
	bool spotLightChanged = false;
	bool sphereGeometryChanged = false;
	bool sphereTransformChanged = false;
	bool animationModelChanged = false;
	bool animationVisibilityChanged = false;
	bool animationPlaybackChanged = false;
	bool cylinderChanged = false;
	bool jointChanged = false;
	DirectionalLightEditorViewData directionalLight;
	SpotLightEditorViewData spotLight;
	SphereEditorViewData sphere;
	AnimationEditorViewData animation;
	CylinderEditorViewData cylinder;
	JointEditorViewData joint;
};

struct ParticleEditorCommand {
	bool activeParticleTypeChanged = false;
	bool gpuModeChanged = false;
	bool agricultureSettingsChanged = false;
	bool interactionSettingsChanged = false;
	bool resetInteractionGrid = false;
	bool alphaReferenceChanged = false;
	bool clearAll = false;
	bool emitAgricultureParticle = false;
	int activeParticleType = 0;
	EditorGpuParticleMode gpuMode = EditorGpuParticleMode::Off;
	AgricultureParticleEditorViewData agriculture;
	InteractionParticleEditorViewData interaction;
	EditorAgricultureParticleType agricultureParticleType = EditorAgricultureParticleType::DirtDust;
	float alphaReference = 0.0f;
};

// Snapshot/Command boundary. The target owns validation and state transitions.
class GamePlayEditorTarget {
public:
    virtual ~GamePlayEditorTarget() = default;
    virtual void BuildViewModel(GamePlayEditorViewModel& output) const = 0;
    virtual bool Execute(const GamePlayEditorCommand& command) = 0;
    virtual bool Execute(const FarmDocumentCommand& command) = 0;
    virtual bool Execute(const VisibilityEditorCommand& command) = 0;
    virtual bool Execute(const CameraEditorCommand& command) = 0;
    virtual bool Execute(const ObjectInspectorEditorCommand& command) = 0;
    virtual bool Execute(const ParticleEditorCommand& command) = 0;
    virtual bool Execute(const SimulationEditorCommand& command) = 0;
    virtual void SetViewportState(const Vector2& imageTopLeft, const Vector2& imageSize,
        const Vector2& mousePosition, bool hovered, bool focused) noexcept = 0;
    virtual Object3d* GetAnimationObjectForDebug() const noexcept = 0;
};

// Borrowed target is cleared before the owning gameplay session is finalized.
class GamePlayEditorBridge final {
public:
    void Bind(GamePlayEditorTarget& target) noexcept { target_ = &target; }
    void Unbind() noexcept { target_ = nullptr; }
    bool IsBound() const noexcept { return target_ != nullptr; }
    void BuildViewModel(GamePlayEditorViewModel& output) const;
    bool Execute(const GamePlayEditorCommand& command);
    bool Execute(const FarmDocumentCommand& command);
    bool Execute(const VisibilityEditorCommand& command);
    bool Execute(const CameraEditorCommand& command);
    bool Execute(const ObjectInspectorEditorCommand& command);
    bool Execute(const ParticleEditorCommand& command);
    bool Execute(const SimulationEditorCommand& command);
    void SetViewportState(const Vector2& imageTopLeft, const Vector2& imageSize,
        const Vector2& mousePosition, bool hovered, bool focused) noexcept;
    Object3d* GetAnimationObjectForDebug() const noexcept;
private:
    GamePlayEditorTarget* target_ = nullptr;
};
} // namespace editor
