#include "gameplay/GamePlaySessionSystem.h"
#include "scene/SceneManager.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "2d/Sprite.h"
#include "3d/Object3d.h"
#include "3d/Model.h"
#include "3d/Camera.h"
#include "audio/Audio.h"
#include "io/Input.h"
#include "base/DirectXCommon.h"
#include "base/ImGuiManager.h"
#include "3d/ModelManager.h"
#include "effect/ParticleManager.h"
#include "2d/SpriteCommon.h"
#include "3d/Object3dCommon.h"
#include "3d/LightingSystem.h"
#include "Game.h"
#include "3d/PrimitiveGenerator.h"
#include "3d/LineDrawer.h"
#include "level/LevelLoader.h"
#include "farm/system/FarmTerrainQuerySystem.h"
#include <dinput.h>
#include "3d/Skybox.h"
#include "2d/TextureManager.h"
#include "base/Logger.h" // 追加：外部ロガーツールのインクルード
#include "FieldManager.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <limits>
#include <random>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace {
constexpr float kMouseLookSensitivity = 0.005f;
constexpr float kMousePanSensitivity = 0.01f;
constexpr float kMouseWheelZoomSpeed = 2.0f;
constexpr float kEditorCameraFastMultiplier = 3.0f;
constexpr float kVirtualScreenWidth = 1280.0f;
constexpr float kVirtualScreenHeight = 720.0f;
constexpr std::size_t kTimelineSnapshotCapacity = 60u * 10u;
constexpr float kTimelineScrubAudioRate = 0.55f;
constexpr float kTimelineAudioEnterTransitionSeconds = 0.08f;
constexpr float kTimelineAudioExitTransitionSeconds = 0.75f;
constexpr const char* kRostockSkyboxPath = "Resources/rostock_laage_airport_4k.dds";
constexpr const char* kFarmDocumentDirectory = "Settings/farm";
constexpr const char* kSceneLevelName = "farm_scene";
constexpr const char* kMeshTypeName = "MESH";
constexpr const char* kCameraTypeName = "CAMERA";
constexpr const char* kLightTypeName = "LIGHT";
constexpr const char* kSpawnPointTypeName = "SPAWN_POINT";
constexpr const char* kEventTriggerTypeName = "EVENT_TRIGGER";
constexpr const char* kGimmickTypeName = "GIMMICK";
constexpr const char* kPatrolPointTypeName = "PATROL_POINT";
constexpr const char* kPlayerRoleName = "PLAYER";
constexpr const char* kCollectibleRoleName = "COLLECTIBLE";
constexpr const char* kGroundRoleName = "GROUND";
constexpr const char* kBoxColliderTypeName = "BOX";
constexpr Vector3 kPlayerCameraOffset = { 0.0f, 3.5f, -8.0f };
constexpr Vector3 kPlayerCameraLookOffset = { 0.0f, 0.7f, 0.0f };
constexpr Vector3 kDefaultPlayerColliderCenterOffset = { 0.0f, 0.9f, 0.0f };
constexpr Vector3 kDefaultPlayerColliderHalfExtents = { 0.45f, 0.9f, 0.45f };
constexpr float kColliderCenterEpsilon = 0.0001f;
constexpr float kPlayerAnimationStopBlendSeconds = 0.20f;
constexpr farm::FarmVisualLayout kFarmVisualLayout = {
	{ 0.0f, 0.05f, 8.0f },
	1.25f,
	0.18f,
	0.18f,
};

Object3d::SpecularType ResolveSpecularType(int selection)
{
	return selection == 0 ? Object3d::SpecularType::Phong : Object3d::SpecularType::BlinnPhong;
}

Vector4 GetLevelRoleColor(const std::string& gameplayRole)
{
	if (gameplayRole == kPlayerRoleName) {
		return { 0.15f, 0.75f, 1.0f, 1.0f };
	}
	if (gameplayRole == kCollectibleRoleName) {
		return { 1.0f, 0.82f, 0.12f, 1.0f };
	}
	if (gameplayRole == kGroundRoleName) {
		return { 0.22f, 0.62f, 0.28f, 1.0f };
	}
	return { 0.85f, 0.85f, 0.85f, 1.0f };
}

float GetLevelObjectRadius(const Vector3& scale)
{
	return (std::max)(0.5f, 0.5f * (std::max)({ std::abs(scale.x), std::abs(scale.y), std::abs(scale.z) }));
}

Vector3 AddVector3(const Vector3& a, const Vector3& b)
{
	return { a.x + b.x, a.y + b.y, a.z + b.z };
}

Vector3 LerpVector3(const Vector3& a, const Vector3& b, float t)
{
	return {
		a.x + (b.x - a.x) * t,
		a.y + (b.y - a.y) * t,
		a.z + (b.z - a.z) * t,
	};
}

[[maybe_unused]] float MoveTowards(float current, float target, float maxDelta)
{
	if (current < target) {
		return (std::min)(current + maxDelta, target);
	}
	return (std::max)(current - maxDelta, target);
}

GPUParticleEmitSettings MakeAgricultureEmitSettings(
	const Vector3& position,
	float particleSize,
	uint32_t count,
	GamePlaySessionSystem::AgricultureParticleType type) {
	GPUParticleEmitSettings settings{};
	settings.translate = position;
	settings.radius = 0.8f;
	settings.color = { 1.0f, 1.0f, 1.0f, 1.0f };
	settings.scale = { particleSize, particleSize, particleSize };
	settings.lifeTime = 1.0f;
	settings.baseVelocity = { 0.0f, 0.3f, 0.0f };
	settings.speed = 0.5f;
	settings.count = count;
	settings.emit = 1;
	settings.preset = static_cast<uint32_t>(type);

	switch (type) {
	case GamePlaySessionSystem::AgricultureParticleType::DirtDust:
		settings.radius = 0.65f;
		settings.color = { 0.45f, 0.28f, 0.12f, 1.0f };
		settings.scale = { particleSize * 0.65f, particleSize * 0.65f, particleSize * 0.65f };
		settings.lifeTime = 0.75f;
		settings.baseVelocity = { 0.0f, 0.35f, 0.0f };
		settings.speed = 0.25f;
		break;
	case GamePlaySessionSystem::AgricultureParticleType::WaterSplash:
		settings.radius = 0.45f;
		settings.color = { 0.45f, 0.85f, 1.0f, 1.0f };
		settings.scale = { particleSize * 0.55f, particleSize * 0.55f, particleSize * 0.55f };
		settings.lifeTime = 0.65f;
		settings.baseVelocity = { 0.0f, 0.15f, 0.0f };
		settings.speed = 1.25f;
		break;
	case GamePlaySessionSystem::AgricultureParticleType::HarvestSparkle:
		settings.radius = 0.9f;
		settings.color = { 1.0f, 0.9f, 0.25f, 1.0f };
		settings.scale = { particleSize * 0.45f, particleSize * 0.45f, particleSize * 0.45f };
		settings.lifeTime = 1.25f;
		settings.baseVelocity = { 0.0f, 0.85f, 0.0f };
		settings.speed = 0.35f;
		break;
	case GamePlaySessionSystem::AgricultureParticleType::PollenSpore:
		settings.radius = 1.2f;
		settings.color = { 0.6f, 0.95f, 0.25f, 1.0f };
		settings.scale = { particleSize * 0.4f, particleSize * 0.4f, particleSize * 0.4f };
		settings.lifeTime = 2.0f;
		settings.baseVelocity = { 0.15f, 0.25f, 0.0f };
		settings.speed = 0.15f;
		break;
	case GamePlaySessionSystem::AgricultureParticleType::BugSwarm:
		settings.radius = 1.0f;
		settings.color = { 0.08f, 0.25f, 0.06f, 1.0f };
		settings.scale = { particleSize * 0.35f, particleSize * 0.35f, particleSize * 0.35f };
		settings.lifeTime = 1.8f;
		settings.baseVelocity = { 0.0f, 0.1f, 0.0f };
		settings.speed = 0.45f;
		break;
	}

	return settings;
}

bool FileExistsNoThrow(const std::filesystem::path& path)
{
	std::error_code error;
	return std::filesystem::exists(path, error) && !error;
}

struct SelectedTileHUDData {
	bool valid = false;
    bool moistureRangeValid = false;
    float goodMoistureMinimum = 0.0f;
    float goodMoistureMaximum = 0.0f;
	int index = -1;
	int height = 0;
	int moisturePercent = 0;
	int storedWaterPercent = 0;
	int growthPercent = 0;
	farm::FarmTileFeature feature = farm::FarmTileFeature::None;
	bool irrigationSupplied = false;
	bool irrigationActive = false;
	farm::FarmWaterStatus waterStatus = farm::FarmWaterStatus::None;
	bool receivedIrrigation = false;
	int irrigationStrengthPercent = 0;
	farm::FarmTileState state = farm::FarmTileState::Empty;
	farm::CropType crop = farm::CropType::None;
	farm::FarmCropGrowthStage growthStage = farm::FarmCropGrowthStage::None;
	FarmHUDMoistureStatus moistureStatus = FarmHUDMoistureStatus::None;
	FarmHUDNextAction nextAction = FarmHUDNextAction::SelectTile;
};

FarmHUDMoistureStatus ToHUDMoistureStatus(FarmMoistureStatus status) noexcept
{
	switch (status) {
	case FarmMoistureStatus::Dry:
		return FarmHUDMoistureStatus::Dry;
	case FarmMoistureStatus::Low:
		return FarmHUDMoistureStatus::Low;
	case FarmMoistureStatus::Good:
		return FarmHUDMoistureStatus::Good;
	case FarmMoistureStatus::Excess:
		return FarmHUDMoistureStatus::Excess;
	case FarmMoistureStatus::Invalid:
	default:
		return FarmHUDMoistureStatus::None;
	}
}

SelectedTileHUDData BuildSelectedTileHUDData(
	const farm::FarmGrid& grid,
	const farm::FarmIrrigationSystem& irrigationSystem,
	const FarmGrowthSystem& growthSystem,
	float timeScale,
	farm::CropType selectedSeedCrop,
	int selectedSeedCount)
{
	const farm::FarmTile* selectedTile = grid.GetSelectedTile();
	if (selectedTile == nullptr) {
		return {};
	}

	const float clampedMoisture = std::isfinite(selectedTile->moisture)
		? std::clamp(selectedTile->moisture, 0.0f, 1.0f) : 0.0f;
	const float clampedGrowth = std::isfinite(selectedTile->growth)
		? std::clamp(selectedTile->growth, 0.0f, 1.0f) : 0.0f;
	const int moisturePercent = static_cast<int>(clampedMoisture * 100.0f + 0.5f);
	const int growthPercent = static_cast<int>(clampedGrowth * 100.0f + 0.5f);
	SelectedTileHUDData data;
	data.valid = true;
	data.index = grid.GetSelectedIndex();
	data.height = selectedTile->heightLevel;
	data.moisturePercent = moisturePercent;
	data.growthPercent = growthPercent;
	data.feature = selectedTile->feature;
	data.storedWaterPercent = std::isfinite(selectedTile->waterAmount)
		? static_cast<int>(std::clamp(selectedTile->waterAmount, 0.0f, 1.0f) * 100.0f + 0.5f) : 0;
	data.irrigationSupplied = irrigationSystem.IsSupplied(data.index);
	data.waterStatus = irrigationSystem.GetWaterStatus(grid, data.index);
	const auto flows = irrigationSystem.GetLastTileFlows(grid);
	data.receivedIrrigation = data.index >= 0 && static_cast<std::size_t>(data.index) < flows.size() &&
		flows[static_cast<std::size_t>(data.index)].soilReceived > 0.0f;
	const float irrigationStrength =
		selectedTile->feature == farm::FarmTileFeature::None
		? irrigationSystem.GetIrrigationStrength(data.index)
		: irrigationSystem.GetSupplyStrength(data.index);
	data.irrigationStrengthPercent = static_cast<int>(
		std::clamp(irrigationStrength, 0.0f, 1.0f) * 100.0f + 0.5f);
	data.state = selectedTile->state;
	data.crop = selectedTile->crop;
	data.growthStage = farm::GetCropGrowthStage(*selectedTile);
	if (selectedTile->feature == farm::FarmTileFeature::Canal) {
		data.nextAction = FarmHUDNextAction::Canal;
		return data;
	}
	if (selectedTile->feature == farm::FarmTileFeature::WaterSource) {
		data.nextAction = FarmHUDNextAction::WaterSource;
		return data;
	}
	const FarmGrowthForecast forecast = growthSystem.Evaluate(
		*selectedTile,
		selectedSeedCrop,
		timeScale,
		irrigationSystem.GetAvailableIrrigationStrength(grid, data.index));
	data.moistureStatus = ToHUDMoistureStatus(forecast.moistureStatus);
	data.moistureRangeValid = forecast.moistureValid;
	data.goodMoistureMinimum = forecast.goodMoistureMinimum;
	data.goodMoistureMaximum = forecast.goodMoistureMaximum;
	data.irrigationActive = forecast.irrigationActive;
	if (selectedTile->state == farm::FarmTileState::Empty) {
		data.nextAction = FarmHUDNextAction::Hoe;
	} else if (forecast.growing && forecast.moistureStatus == FarmMoistureStatus::Excess) {
		data.nextAction = FarmHUDNextAction::ReduceWater;
	} else if (selectedTile->state == farm::FarmTileState::Tilled) {
		if (selectedSeedCount <= 0) {
			data.nextAction = FarmHUDNextAction::BuySeed;
		} else {
			data.nextAction = selectedTile->moisture < 0.5f
				? FarmHUDNextAction::WaterOrSeed : FarmHUDNextAction::Seed;
		}
	} else if (farm::IsHarvestReady(*selectedTile)) {
		data.nextAction = FarmHUDNextAction::Harvest;
	} else {
		data.nextAction = (forecast.moistureStatus == FarmMoistureStatus::Dry ||
			forecast.moistureStatus == FarmMoistureStatus::Low)
			? FarmHUDNextAction::Water : FarmHUDNextAction::Growing;
	}
	return data;
}

FarmHUDFeedback ToHUDFeedback(FarmFeedbackKind kind, farm::CropType crop)
{
	switch (kind) {
	case FarmFeedbackKind::Harvest:
		return FarmHUDFeedback::Harvest;
	case FarmFeedbackKind::Sale:
		return FarmHUDFeedback::Sale;
	case FarmFeedbackKind::EmptySale:
		return FarmHUDFeedback::EmptySale;
	case FarmFeedbackKind::InputLocked:
		return FarmHUDFeedback::InputLocked;
	case FarmFeedbackKind::Restarted:
		return FarmHUDFeedback::Restarted;
	case FarmFeedbackKind::SeedPurchased:
		if (crop == farm::CropType::Tomato) return FarmHUDFeedback::SeedPurchasedTomato;
		if (crop == farm::CropType::Pumpkin) return FarmHUDFeedback::SeedPurchasedPumpkin;
		return crop == farm::CropType::Carrot
			? FarmHUDFeedback::SeedPurchasedCarrot
			: FarmHUDFeedback::SeedPurchasedTurnip;
	case FarmFeedbackKind::CropSelected:
		if (crop == farm::CropType::Tomato) return FarmHUDFeedback::CropSelectedTomato;
		if (crop == farm::CropType::Pumpkin) return FarmHUDFeedback::CropSelectedPumpkin;
		return crop == farm::CropType::Carrot
			? FarmHUDFeedback::CropSelectedCarrot
			: FarmHUDFeedback::CropSelectedTurnip;
	case FarmFeedbackKind::NoSeed:
		if (crop == farm::CropType::Tomato) return FarmHUDFeedback::NoSeedTomato;
		if (crop == farm::CropType::Pumpkin) return FarmHUDFeedback::NoSeedPumpkin;
		return crop == farm::CropType::Carrot
			? FarmHUDFeedback::NoSeedCarrot
			: FarmHUDFeedback::NoSeedTurnip;
	case FarmFeedbackKind::InsufficientMoney:
		return FarmHUDFeedback::InsufficientMoney;
	case FarmFeedbackKind::None:
	default:
		return FarmHUDFeedback::None;
	}
}

void BuildCameraGroundMoveAxes(float yaw, Vector3& right, Vector3& forward)
{
	const float sinYaw = std::sin(yaw);
	const float cosYaw = std::cos(yaw);

	right = { cosYaw, 0.0f, -sinYaw };
	forward = { sinYaw, 0.0f, cosYaw };
}

void BuildCameraViewAxes(const Vector3& rotate, Vector3& right, Vector3& up, Vector3& forward)
{
	const Matrix4x4 rotateMatrix = MatrixMath::Multiply(
		MatrixMath::MakeRotateXMatrix(rotate.x),
		MatrixMath::Multiply(
			MatrixMath::MakeRotateYMatrix(rotate.y),
			MatrixMath::MakeRotateZMatrix(rotate.z)));

	right = MatrixMath::Normalize({ rotateMatrix.m[0][0], rotateMatrix.m[0][1], rotateMatrix.m[0][2] });
	up = MatrixMath::Normalize({ rotateMatrix.m[1][0], rotateMatrix.m[1][1], rotateMatrix.m[1][2] });
	forward = MatrixMath::Normalize({ rotateMatrix.m[2][0], rotateMatrix.m[2][1], rotateMatrix.m[2][2] });
}

std::string ResolveModelFilename(const std::string& fileName)
{
	const std::filesystem::path resourceRoot = "Resources";
	const std::filesystem::path sourcePath = fileName;

	if (sourcePath.has_extension() && FileExistsNoThrow(resourceRoot / sourcePath)) {
		return fileName;
	}

	const std::filesystem::path parentPath = sourcePath.parent_path();
	const std::string stem = sourcePath.filename().string();
	const std::filesystem::path relativeStemPath = parentPath / stem;
	const std::filesystem::path folderStemPath = sourcePath / stem;
	const std::filesystem::path candidates[] = {
		relativeStemPath.string() + ".obj",
		relativeStemPath.string() + ".gltf",
		relativeStemPath.string() + ".fbx",
		folderStemPath.string() + ".obj",
		folderStemPath.string() + ".gltf",
		folderStemPath.string() + ".fbx",
	};

	for (const std::filesystem::path& candidate : candidates) {
		if (FileExistsNoThrow(resourceRoot / candidate)) {
			return candidate.generic_string();
		}
	}

	return fileName;
}

}

GamePlaySessionSystem::GamePlaySessionSystem(BaseScene& owner, SceneManager* manager)
    : owner_(owner), sceneManager_(manager) {}
GamePlaySessionSystem::~GamePlaySessionSystem() = default;

bool GamePlaySessionSystem::SetFarmGameMode(bool enabled) {
	if (farmGameMode_ == enabled) { return true; }
	if (farmIrrigationPreviewSystem_.IsActive() || timelineScrubbing_) { return false; }
	farmCropSelectionSystem_.Cancel();
	if (enabled) {
		developmentPlayerCamera_ = usePlayerCamera_;
		SetUsePlayerCamera(true);
	} else {
		SetUsePlayerCamera(developmentPlayerCamera_);
	}
	farmGameMode_ = enabled;
	return true;
}

/**
 * Initialize: シーン開始時に一度だけ呼ばれるセットアップ関数
 */
void GamePlaySessionSystem::Initialize() {
    (void)InitializeIncrementally().Complete();
}

InitializationTask GamePlaySessionSystem::InitializeIncrementally() {
	framework_ = Framework::GetInstance();
	if (framework_ && framework_->GetAudio()) {
		framework_->GetAudio()->SetGlobalTemporalState(
			AudioPlaybackDirection::Forward,
			1.0f,
			0.0f);
	}
	farmGrid_.Initialize(5, 4);
	farmDateSystem_.Initialize();
	farmToolSystem_.Initialize();
	farmEconomySystem_.Initialize();
	farmGrowthSystem_.Initialize();
	farmGrowthComparisonSystem_.Initialize();
	farmIrrigationPreviewSystem_.Initialize();
	farmIrrigationSystem_.Initialize();
	farmToolActionSystem_.Initialize();
	farmCropSelectionSystem_.Initialize();
	farmFeedbackSystem_.Initialize();
	farmHarvestVisualSystem_.Clear();
	farmProgressionSystem_.Initialize({}, FarmProgressionMode::ContestSeason);
	farmStageAccessSystem_.Bind(farmDateSystem_, farmProgressionSystem_.StagePolicy());
	farmEconomySystem_.BindStageAccess(farmStageAccessSystem_);
	farmToolActionSystem_.BindStageAccess(farmStageAccessSystem_);
	farmIrrigationPreviewSystem_.BindStageAccess(farmStageAccessSystem_);
	farmVisualSystem_.Initialize(kFarmVisualLayout);
	farmIrrigationSystem_.Rebuild(farmGrid_);
	if (!farmDocumentSystem_.Initialize(
		kFarmDocumentDirectory, farmGrid_, farmEconomySystem_,
		farmCropSelectionSystem_, &farmDateSystem_, &farmProgressionSystem_)) {
		AddLog("Farm document initialization failed: " + farmDocumentSystem_.GetStatusMessage());
	} else {
		static_cast<void>(
			farmProgressionSystem_.EvaluateClear(farmEconomySystem_.GetMoney()));
	}
	gamePlayEditorBridge_.Bind(*this);
    co_await std::suspend_always{};

	// ログ記録：UIと外部出力の両方に行われます
	AddLog("Scene: GamePlay Initialized.");

	// ---------------------------------------------------------
	// 1. テクスチャ・リソースの読み込み
	// ---------------------------------------------------------
	textureHandles_.clear();
	std::vector<std::string> textureNames = { "farm/white.png" };
	for (const auto& name : textureNames) {
		textureHandles_.push_back(TextureManager::GetInstance()->LoadTexture2D("Resources/" + name));
	}
	levelWhiteTextureHandle_ = TextureManager::GetInstance()->LoadTexture2D("Resources/farm/white.png");
	if (!farmRenderer_.Initialize(framework_->GetObject3dCommon(), framework_->GetModelManager(), levelWhiteTextureHandle_)) {
		AddLog("Farm placeholder OBJ unavailable; keeping debug-line rendering.");
	}
    co_await std::suspend_always{};
	Texture2DHandle circle2Handle = TextureManager::GetInstance()->LoadTexture2D("Resources/farm/particle_circle.png");
	ringTexHandle_ = TextureManager::GetInstance()->LoadTexture2D("Resources/farm/particle_ring.png");
    co_await std::suspend_always{};

	// ---------------------------------------------------------
	// 2. システム・環境の初期化
	// ---------------------------------------------------------
	camera_ = std::make_unique<Camera>();
	LineDrawer::GetInstance()->Initialize(framework_->GetDxCommon());
    co_await std::suspend_always{};

	skeletonDebugger_ = std::make_unique<SkeletonDebugger>();
	skeletonDebugger_->Initialize(framework_->GetObject3dCommon(), framework_->GetModelManager());
    co_await std::suspend_always{};

	fieldManager_ = std::make_unique<FieldManager>();
	fieldManager_->Initialize(framework_);
	cropBurstSelectedIndex_ = 1;
	cropBurstEffectPosition_ = fieldManager_->GetDemoFieldWorldPosition(cropBurstSelectedIndex_);
	fieldManager_->TrySelectTileByWorldPosition(cropBurstEffectPosition_);
    co_await std::suspend_always{};

	// ---------------------------------------------------------
	// 3. モデルデータのロード
	// ---------------------------------------------------------
	std::string terrainPath = "farm/ground.obj";
	framework_->GetModelManager()->LoadModel(terrainPath);
	Model* tModel = framework_->GetModelManager()->GetModel(terrainPath);
	if (tModel) { tModel->LoadTextures(); }
    co_await std::suspend_always{};

	Model* planeModel = tModel;

	// School sample animations are development-only, never a Release dependency.
#ifdef USE_IMGUI
	// アニメーションモデルのプリロード（配布データ3種と仮モデル1種）
	std::vector<std::pair<std::string, std::string>> animModels = {
		{ "AnimatedCube", "AnimatedCube.gltf" },
		{ "simpleSkin", "simpleSkin.gltf" },
		{ "human", "walk.gltf" },
		{ "human", "sneakWalk.gltf" }
	};

	for (const auto& pair : animModels) {
		std::string path = pair.first + "/" + pair.second;
		framework_->GetModelManager()->LoadModel(path);
		Model* m = framework_->GetModelManager()->GetModel(path);
		if (m) {
			// テクスチャをSpriteCommonにロードする
			m->LoadTextures();
		}
        co_await std::suspend_always{};
	}

#endif
	// ---------------------------------------------------------
	// 4. 各種オブジェクトの実体生成
	// ---------------------------------------------------------
	terrainObj_ = std::make_unique<Object3d>();
	terrainObj_->Initialize(framework_->GetObject3dCommon());
	terrainObj_->SetModel(tModel);
	terrainObj_->SetPosition({ 0.0f, -2.0f, 0.0f });
	terrainObj_->SetScale({ 2.0f, 2.0f, 2.0f });

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(framework_->GetObject3dCommon());
	object3d_->SetModel(planeModel);

	// The human animation is assigned to the level PLAYER object, not shown as a separate sample object.
	showAnimModel_ = false;
	LoadSceneLevel();
    co_await std::suspend_always{};

	sprite_ = std::make_unique<Sprite>();
	sprite_->Initialize(framework_->GetSpriteCommon(), "Resources/" + textureNames[0]);

	CreateSphere(sphereRadius_);
    co_await std::suspend_always{};

	ringModel_ = PrimitiveGenerator::CreateRing(framework_->GetModelManager(), 0.8f, 1.0f, 32);
	// 円柱：メンバ変数のパラメータで生成（ImGuiから変更可能）
	cylinderModel_ = PrimitiveGenerator::CreateCylinder(
		framework_->GetModelManager(),
		cylTopRadius_ ,cylBottomRadius_, cylHeight_,
		static_cast<uint32_t>(cylSegments_),
		static_cast<uint32_t>(cylVertDivisions_));
    co_await std::suspend_always{};
	// ---------------------------------------------------------
	// 5. パーティクルの設定
	// ---------------------------------------------------------
	framework_->GetParticleManager()->CreateParticleGroup("Spark", circle2Handle);
	framework_->GetParticleManager()->CreateParticleGroup("RingEffect", ringTexHandle_, ringModel_.get());
	framework_->GetParticleManager()->CreateParticleGroup("CylinderEffect", ringTexHandle_, cylinderModel_.get());

	if (!co_await InitializeFarmHUD()) co_return false;
    co_await std::suspend_always{};
	InitializeStageClearHUD();
    co_await std::suspend_always{};
#ifndef USE_IMGUI
	if (!farmSeedShopRenderer_.Initialize(framework_->GetObject3dCommon(), framework_->GetModelManager(), levelWhiteTextureHandle_)) {
		AddLog("Farm seed shop presentation initialization failed.");
	}
    co_await std::suspend_always{};
	if (!co_await farmRuntimeController_.InitializeIncrementally(framework_->GetSpriteCommon())) {
		AddLog("Farm runtime menu initialization failed.");
	}
    if (!co_await farmContestRenderer_.InitializeIncrementally(framework_->GetObject3dCommon(),
        framework_->GetModelManager(), levelWhiteTextureHandle_, TextureManager::GetInstance())) co_return false;
    if (!co_await farmArrivalRenderer_.InitializeIncrementally(framework_->GetObject3dCommon(), framework_->GetModelManager(),
        TextureManager::GetInstance())) AddLog("Arrival scenery unavailable; using the safe farm overview.");
    farmRuntimeController_.InitializePresentationAudio(framework_->GetAudio());
    co_await std::suspend_always{};
	// Build the initial view before reveal; no input or simulation update is needed.
	farmRuntimeController_.Refresh(RuntimeContext());
#endif
	// Reveal can draw before the first input/simulation update.
	PrepareRenderFrame(0.0f);
	framework_->GetParticleManager()->Update(camera_.get(), 0.0f);
    co_return true;
}

void GamePlaySessionSystem::EmitSpark(const Vector3& position) {
	std::random_device seed_gen;
	std::mt19937 randomEngine(seed_gen());
	std::uniform_real_distribution<float> distRotate(-3.141592f, 3.141592f);
	std::uniform_real_distribution<float> distScale(0.8f, 1.5f);

	for (int i = 0; i < 8; ++i) {
		Particle& p = framework_->GetParticleManager()->AddParticle("Spark", position);
		p.transform.scale = { 0.05f, distScale(randomEngine), 1.0f };
		p.transform.rotate = { 0.0f, 0.0f, distRotate(randomEngine) };
		p.color = { 1.0f, 1.0f, 0.6f, 1.0f };
		p.velocity = { 0.0f, 0.0f, 0.0f };
		p.lifeTime = 0.2f;
	}
}

void GamePlaySessionSystem::EmitRingEffect(const Vector3& position) {
	Particle& p = framework_->GetParticleManager()->AddParticle("RingEffect", position);

	//地面を水平にする
	p.transform.rotate = {std::numbers::pi_v<float> /2.0f, 0.0f, 0.0f};
	p.velocity = { 0.0f,0.0f,0.0f };
	p.color = { 1.0f, 1.0f, 1.0f, 0.8f };
	p.lifeTime = 3.0f;

	// Ringが徐々に広がる設定
	p.startSize = 0.5f;
	p.endSize = 3.0f;

	// UVスクロールの設定（テクスチャが回転するように見える）
	// 資料の「U方向にScaleすれば解像度が…」の対応
	p.uvScale = { 2.0f, 1.0f };
	// V方向にテクスチャーをスクロールさせる
	p.uvVelocity = { 0.0f, -1.5f };
}
// 機能：円柱エフェクトの放出関数
void GamePlaySessionSystem::EmitCylinderEffect(const Vector3& position) {
	Particle& p = framework_->GetParticleManager()->AddParticle("CylinderEffect", position);
	p.transform.rotate = { 0.0f, 0.0f, 0.0f };
	p.color = { 0.4f, 0.7f, 1.0f, 1.0f }; // 少し青みを強めに
	p.lifeTime = 1.5f;
	p.velocity = { 0.0f, 0.01f, 0.0f };
	p.startSize = 0.5f;
	p.endSize = 3.0f;

	// 資料：V Flip (v = 1-v) と 横方向スクロール
	p.uvScale = { 2.0f, -1.0f }; // U方向に2倍(解像度アップ)、V方向に反転
	p.uvOffset = { 0.0f, 1.0f };
	p.uvVelocity = { 1.5f, 0.0f }; // U方向にスクロール
}

void GamePlaySessionSystem::Update() {
	static_cast<void>(farmProgressionSystem_.EvaluateSeason(FarmContestSeasonSystem::Evaluate(
		farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults())));
	farmContestDaySystem_.Observe(farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults(), !farmProgressionSystem_.IsFreeFarming());
	UpdateSceneDeltaTime();
    if (layoutLibraryFrame_) return;

	// Collect frame requests before scheduling Scene systems.
	Input* frameInput = framework_ ? framework_->GetInput() : nullptr;
	const bool controlHeld = frameInput && (frameInput->PushKey(DIK_LCONTROL) || frameInput->PushKey(DIK_RCONTROL));
	bool levelReloadRequested = frameInput &&
		(frameInput->TriggerKey(DIK_F5) || (controlHeld && frameInput->TriggerKey(DIK_R)));
	bool cameraModeToggleRequested = !farmGameMode_ && frameInput && frameInput->TriggerKey(DIK_F1);
	if (ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		levelReloadRequested = false;
		cameraModeToggleRequested = false;
	}
#ifndef USE_IMGUI
	if (farmRuntimeController_.ConsumesInput()) levelReloadRequested = false;
#endif
	if (levelReloadRequested && farmProgressionSystem_.IsCleared()) {
		if (farmProgressionSystem_.IsContestSeason()) {
			farmRuntimeController_.RequestSeasonRetry(RuntimeContext());
			levelReloadRequested = false;
		} else if (!ResetFarmSession()) levelReloadRequested = false;
	}
	if (levelReloadRequested) {
		LoadSceneLevel();
	}
	if (cameraModeToggleRequested) {
		ToggleCameraMode();
	}

	// Farm input has priority; editor-camera input uses unpaused real time.
	const bool farmGridInputConsumed = HandleFarmInput();
	farmIrrigationSystem_.Rebuild(farmGrid_);
#ifndef USE_IMGUI
	if (!farmRuntimeController_.ConsumesInput())
#endif
		HandleCameraInput(realDeltaTime_, farmGridInputConsumed);
	ClampCameraPitch();
	if (!farmGameMode_ && !farmGridInputConsumed) {
		HandleKeyboardMovement();
	}
	SyncLevelGameplayPresentation();
	UpdatePlayerCamera();

#ifndef USE_IMGUI
	if (!farmRuntimeController_.ConsumesInput()) HandleFarmHistoryInput();
	farmRuntimeController_.Refresh(RuntimeContext());
#endif
	if (!farmGameMode_ && viewportFocused_ && !ImGuiManager::GetInstance()->WantsTextInput()) {
		HandleFarmDateDebugInput();
	}

	farmGrowthComparisonSystem_.ObserveBeforeStep(farmGrid_);
	if (farmProgressionSystem_.IsCleared()) farmGrowthComparisonSystem_.Stop();
	farmFeedbackSystem_.Update(realDeltaTime_);
	if (timelineScrubbing_) farmHarvestVisualSystem_.Clear();
	// Committed harvest feedback must finish even when pause or clear stops world time.
	farmHarvestVisualSystem_.Update(farmGrid_, realDeltaTime_);
	PrepareRenderFrame(sceneDeltaTime_);

	// スペースキー入力時の分岐（activeParticleType_ は Game.cpp の ImGui から書き換わる）
	const bool particleTestInput = !farmGameMode_ && viewportFocused_ &&
		gpuParticleDebugMode_ != GPUParticleDebugMode::Off;
	const bool cropBurstInputHandled = particleTestInput && UpdateCropBurstDebugInput();
	if (!cropBurstInputHandled &&
		particleTestInput &&
		framework_->GetInput()->TriggerKey(DIK_SPACE)) {
		switch (activeParticleType_) {
		case 0: EmitSpark(spherePos_); break;
		case 1: EmitRingEffect(spherePos_); break;
		case 2: EmitCylinderEffect(spherePos_); break; // ★Cylinder単体
		case 3: // Combined
			EmitRingEffect(spherePos_);
			EmitCylinderEffect(spherePos_);
			break;
		case 4: // Explosion
			framework_->GetParticleManager()->Emit("Spark", spherePos_, 20);
			break;
		}
	}

	// Debug input changes particle requests; ParticleManager owns GPU state.
	SyncGPUParticleDebugModeChange();
	if (particleTestInput) {
		HandleGPUParticleDebugModeInput();
		UpdateCropBurstAutoPlayback(sceneDeltaTime_);
	}

	framework_->GetParticleManager()->Update(camera_.get(), timelineScrubbing_ ? 0.0f : sceneDeltaTime_);
}

void GamePlaySessionSystem::PrepareRenderFrame(float presentationDeltaTime) {
	camera_->SetTranslate(cameraPos_);
	camera_->SetRotate(cameraRot_);
	camera_->Update();
	sprite_->SetPosition(spritePos_);
	sprite_->Update();
	if (farmHudInitialized_) {
		farmHud_.SetViewData(BuildFarmHUDViewData());
		farmHud_.Update(presentationDeltaTime);
	}
	if (stageClearHudInitialized_) {
		stageClearHud_.SetVisible(
			levelGameplay_.IsStageCleared() || farmProgressionSystem_.IsCleared());
		stageClearHud_.Update();
	}
	if (skyboxEnabled_) {
		InitializeSkyboxIfNeeded();
		if (skybox_) {
			skybox_->Update(camera_.get());
		}
	}

	// LightingSystem owns the GPU upload; the Scene supplies one frame snapshot.
	LightingSystem* lightingSystem = framework_->GetLightingSystem();
	assert(lightingSystem != nullptr);
	lightingSystem->SetDirectionalLight({
		{ lightColor_.x, lightColor_.y, lightColor_.z, 1.0f },
		lightDirection_,
		lightIntensity_ });
	LightingSystem::SpotLight spotLight{};
	spotLight.color = { spotLightColor_.x, spotLightColor_.y, spotLightColor_.z, 1.0f };
	spotLight.position = spotLightPos_;
	spotLight.intensity = spotLightIntensity_;
	spotLight.direction = spotLightDir_;
	spotLight.distance = spotLightDistance_;
	spotLight.decay = spotLightDecay_;
	spotLight.cosAngle = std::cos(spotLightAngle_);
	spotLight.cosFalloffStart = std::cos(spotLightFalloff_);
	lightingSystem->SetSpotLight(spotLight);
	lightingSystem->SetCameraPosition(camera_->GetTranslate());
#ifndef USE_IMGUI
    if (farmRuntimeController_.ShowsContestDetail()) {
        const auto frame=farmRuntimeController_.ContestFrame();
        lightingSystem->SetDirectionalLight({FarmContestStageLayout::kCeremonyLightColor,
            FarmContestStageLayout::kCeremonyLightDirection,FarmContestStageLayout::kCeremonyLightIntensity});
        spotLight.intensity=0;
        lightingSystem->SetSpotLight(spotLight);
        const bool ceremony=frame.unsubmitted || frame.page==FarmContestPresentationSystem::Page::Result;
        lightingSystem->SetCameraPosition(ceremony ? FarmContestStageLayout::CeremonyView(frame).position :
            Vector3{0,0,-FarmContestStageLayout::CameraDistance(frame.specimenReveal)});
    }
#endif

	// Apply shared render settings consistently before each Object3d update.
	auto UpdateObjectState = [&](Object3d* obj, float envCoef) {
		if (!obj) return;
		obj->SetCullMode(cullMode_);
		obj->SetSpecularType(ResolveSpecularType(specularTypeSelection_));

		if (skybox_ && skyboxEnvironmentEnabled_) {
			obj->SetEnvironmentMap(skybox_->GetTextureHandle());
			obj->SetEnvironmentCoefficient(envCoef);
		} else {
			obj->SetEnvironmentCoefficient(0.0f);
		}

		obj->Update(camera_.get(), timelineScrubbing_ ? 0.0f : presentationDeltaTime);
		};

	UpdateObjectState(terrainObj_.get(), 0.0f);
	UpdateObjectState(object3d_.get(), 0.0f);
	UpdateObjectState(animObj_.get(), 0.5f);
	for (LevelObjectRuntime& levelObject : levelObjects_) {
		if (!levelObject.object || !levelObject.visible) {
			continue;
		}
		if (levelObject.gameplayRole == kCollectibleRoleName) {
			const float animationTime = levelRouteTimer_ * 2.0f + levelObject.animationPhase;
			Vector3 position = levelObject.basePosition;
			position.y += std::sin(animationTime) * 0.2f;
			levelObject.object->SetPosition(position);
			levelObject.object->SetRotation({ 0.0f, animationTime, 0.0f });
		}
		UpdateObjectState(levelObject.object.get(), 0.0f);
	}

	if (sphereObj_) {
		sphereObj_->SetPosition(spherePos_);
		sphereObj_->SetRotation(objectRot_);
		UpdateObjectState(sphereObj_.get(), 0.5f);
	}

}

void GamePlaySessionSystem::FixedUpdate(float fixedDeltaTime) {
    farmContestDaySystem_.Observe(farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults(), !farmProgressionSystem_.IsFreeFarming());
    if (farmContestDaySystem_.PendingDay()) return;
    if (layoutLibraryFrame_) return;
#ifndef USE_IMGUI
	if (farmRuntimeController_.BlocksSimulation()) return;
#endif
	// Deterministic gameplay mutation stays in fixed-step Systems.
	if (timelineScrubbing_) {
		const bool stepped = timelineForwardHeld_
			? timeline_.StepForward(timelineScratch_)
			: timeline_.StepBackward(timelineScratch_);
		if (stepped) {
			RestoreTimelineSnapshot(timelineScratch_);
		}
		timelineStepBackwardRequested_ = false;
		timelineStepForwardRequested_ = false;
		return;
	}

	levelRouteTimer_ += fixedDeltaTime;
	if (!farmProgressionSystem_.IsCleared() && !farmIrrigationPreviewSystem_.IsActive()) {
		farmGrowthComparisonSystem_.ObserveBeforeStep(farmGrid_);
		if (farmIrrigationSystem_.UpdateWater(farmGrid_, fixedDeltaTime, farmDateSystem_.GetTimeScale())) {
			farmDocumentSystem_.MarkDirty();
		}
		if (farmGrowthSystem_.Update(
			farmGrid_,
			fixedDeltaTime,
			farmDateSystem_.GetTimeScale())) {
			farmDocumentSystem_.MarkDirty();
		}
		farmGrowthComparisonSystem_.ObserveAfterStep(farmGrid_, fixedDeltaTime, farmDateSystem_.GetTimeScale());
		farmContestDaySystem_.Advance(farmDateSystem_, fixedDeltaTime, farmEconomySystem_.GetContestResults(),
			farmProgressionSystem_.IsContestSeason() ? FarmContestEntrySystem::kContestDays.back() + 1 : 0,
			!farmProgressionSystem_.IsFreeFarming());
		static_cast<void>(farmProgressionSystem_.EvaluateSeason(FarmContestSeasonSystem::Evaluate(
			farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults())));
		if (!farmDocumentSystem_.IsDirty()) farmDocumentSystem_.MarkDirty();
	}
	const auto groundQuery = BuildPlayerGroundQuery();
	levelGameplay_.UpdatePlayer(pendingPlayerCommand_, fixedDeltaTime, &groundQuery);
	pendingPlayerCommand_.jumpPressed = false;
	CaptureTimelineSnapshot(timelineScratch_);
	timeline_.Record(timelineScratch_);
}

level::LevelGameplaySystem::GroundHeightQuery GamePlaySessionSystem::BuildPlayerGroundQuery() const {
	return {
		this, [](const void* context, const Vector3& position, float& height) {
			const auto& scene = *static_cast<const GamePlaySessionSystem*>(context);
			const auto& offset = scene.levelGameplay_.GetPlayerColliderCenterOffset();
			const auto& half = scene.levelGameplay_.GetPlayerColliderHalfExtents();
			const auto sample = farm::FarmTerrainQuerySystem::SampleGround(scene.farmGrid_, scene.farmVisualSystem_,
				{position.x + offset.x, position.y, position.z + offset.z}, {half.x, half.z});
			if (!sample.valid) { return false; }
			height = sample.height;
			return true;
		}
	};
}

bool GamePlaySessionSystem::MovePlayerToFarmTile(int tileIndex) {
#ifdef USE_IMGUI
	if (farmGameMode_ || timelineScrubbing_ || farmIrrigationPreviewSystem_.IsActive() || farmProgressionSystem_.IsCleared()) { return false; }
	const auto tile = farmVisualSystem_.GetTileVisualData(farmGrid_, tileIndex);
	if (!tile.valid) { return false; }
	const auto& offset = levelGameplay_.GetPlayerColliderCenterOffset();
	const Vector3 destination{tile.center.x - offset.x, tile.center.y, tile.center.z - offset.z};
	const auto query = BuildPlayerGroundQuery();
	if (!levelGameplay_.TryPlacePlayerOnGround(destination, &query)) { return false; }
	pendingPlayerCommand_ = {};
	farmCropSelectionSystem_.Cancel();
	UpdateLevelPlayerVisual();
	UpdatePlayerCamera();
	CaptureTimelineSnapshot(timelineScratch_);
	timeline_.Record(timelineScratch_);
	return true;
#else
	(void)tileIndex;
	return false;
#endif
}

void GamePlaySessionSystem::CreateSphere(float radius) {
	sphereModel_ = PrimitiveGenerator::CreateSphere(framework_->GetModelManager(), radius, 32);
	if (!sphereObj_) {
		sphereObj_ = std::make_unique<Object3d>();
		sphereObj_->Initialize(framework_->GetObject3dCommon());
	}
	sphereObj_->SetModel(sphereModel_.get());
	sphereObj_->SetTexture(levelWhiteTextureHandle_);
	sphereObj_->SetShininess(40.0f);
}

void GamePlaySessionSystem::LoadSceneLevel() {
	levelData_ = level::LevelLoader::Load(kSceneLevelName);
	levelObjects_.clear();
	levelGameplay_.Reset();
	if (stageClearHudInitialized_) {
		stageClearHud_.SetVisible(false);
	}

	if (!levelData_) {
		timeline_.Clear();
		AddLog("Level load failed: Resources/levels/farm_scene.json");
		return;
	}

	CollectLevelRuntimeData();
	ApplyLevelCamera();
	CreateLevelObjectsFromLevel();
	farmToolActionSystem_.ClearHistory();
	InitializeTimeline();
	AddLog("Level MESH objects: " + std::to_string(levelObjects_.size()));
}

void GamePlaySessionSystem::CreateLevelObjectsFromLevel() {
	levelObjects_.clear();
	levelGameplay_.Reset();
	playerVisualConfigured_ = false;
	playerAnimationState_ = PlayerAnimationState::Idle;
	playerAnimationSpeed_ = 0.0f;
	if (!framework_ || !levelData_) {
		return;
	}

	ModelManager* modelManager = framework_->GetModelManager();
	if (!modelManager) {
		AddLog("Level object creation failed: ModelManager is null.");
		return;
	}

	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.type != kEventTriggerTypeName || objectData.disabled) {
			continue;
		}
		if (!objectData.hasCollider || objectData.collider.type != kBoxColliderTypeName) {
			AddLog("EVENT_TRIGGER skipped. BOX collider is required: " + objectData.name);
			continue;
		}

		const Vector3 triggerCenter = AddVector3(objectData.transform.translation, objectData.collider.center);
		const Vector3 triggerHalfExtents = {
			std::abs(objectData.collider.size.x) * 0.5f,
			std::abs(objectData.collider.size.y) * 0.5f,
			std::abs(objectData.collider.size.z) * 0.5f,
		};
		if (!levelGameplay_.AddEventTrigger(triggerCenter, triggerHalfExtents, objectData.eventId)) {
			AddLog("EVENT_TRIGGER skipped. Invalid collider or event_id: " + objectData.name);
		}
	}

	std::size_t meshCount = 0;
	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.type == kMeshTypeName && !objectData.disabled) {
			++meshCount;
		}
	}
	levelObjects_.reserve(meshCount);

	std::unordered_set<std::string> loadedModelNames;
	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.type != kMeshTypeName) {
			continue;
		}
		if (objectData.disabled) {
			AddLog("Level MESH disabled: " + objectData.name);
			continue;
		}
		if (objectData.fileName.empty()) {
			AddLog("Level MESH skipped. file_name is empty.");
			continue;
		}

		const std::string modelFileName = ResolveModelFilename(objectData.fileName);
		if (!FileExistsNoThrow(std::filesystem::path("Resources") / modelFileName)) {
			AddLog("Level MESH skipped. Model file not found: " + modelFileName);
			continue;
		}

		modelManager->LoadModel(modelFileName);
		Model* model = modelManager->GetModel(modelFileName);
		if (!model) {
			AddLog("Level MESH skipped. Model load failed: " + modelFileName);
			continue;
		}

		if (loadedModelNames.insert(modelFileName).second) {
			model->LoadTextures();
		}

		std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
		object->Initialize(framework_->GetObject3dCommon());
		object->SetModel(model);
		object->SetPosition(objectData.transform.translation);
		object->SetRotation(objectData.transform.rotation);
		object->SetScale(objectData.transform.scaling);
		object->SetTexture(levelWhiteTextureHandle_);
		object->SetColor(GetLevelRoleColor(objectData.gameplayRole));
		object->SetShininess(20.0f);

		const std::size_t renderIndex = levelObjects_.size();
		LevelObjectRuntime runtime{};
		runtime.object = std::move(object);
		runtime.name = objectData.name;
		runtime.gameplayRole = objectData.gameplayRole;
		runtime.basePosition = objectData.transform.translation;
		runtime.animationPhase = static_cast<float>(renderIndex) * 0.7f;
		levelObjects_.push_back(std::move(runtime));

		if (objectData.gameplayRole == kPlayerRoleName) {
			Vector3 colliderCenterOffset = kDefaultPlayerColliderCenterOffset;
			Vector3 colliderHalfExtents = kDefaultPlayerColliderHalfExtents;
			if (objectData.hasCollider && objectData.collider.type == "BOX") {
				colliderCenterOffset = objectData.collider.center;
				colliderHalfExtents = {
					std::abs(objectData.collider.size.x) * 0.5f,
					std::abs(objectData.collider.size.y) * 0.5f,
					std::abs(objectData.collider.size.z) * 0.5f,
				};
				if (std::abs(colliderCenterOffset.y) <= kColliderCenterEpsilon) {
					colliderCenterOffset.y = colliderHalfExtents.y;
				}
			}
			levelGameplay_.ConfigurePlayer(
				renderIndex,
				objectData.transform.translation,
				colliderCenterOffset,
				colliderHalfExtents);
		} else if (objectData.gameplayRole == kCollectibleRoleName) {
			levelGameplay_.AddCollectible(renderIndex, objectData.transform.translation, GetLevelObjectRadius(objectData.transform.scaling));
		} else if (objectData.gameplayRole == kGroundRoleName) {
			const Vector3& scale = objectData.transform.scaling;
			levelGameplay_.ConfigureGround(
				objectData.transform.translation,
				{ (std::max)(std::abs(scale.x), 0.5f), 0.05f, (std::max)(std::abs(scale.z), 0.5f) });
		} else {
			const Vector3& scale = objectData.transform.scaling;
			levelGameplay_.AddObstacle(
				objectData.transform.translation,
				{ (std::max)(std::abs(scale.x), 0.25f),
				  (std::max)(std::abs(scale.y), 0.25f),
				  (std::max)(std::abs(scale.z), 0.25f) });
		}
	}

	if (levelGameplay_.HasPlayer()) {
		const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
		if (playerIndex < levelObjects_.size() && levelObjects_[playerIndex].object) {
			levelObjects_[playerIndex].object->SetPosition(levelGameplay_.GetPlayerPosition());
		}
		UpdateLevelPlayerVisual();
		UpdatePlayerCamera();
	} else if (usePlayerCamera_ && hasLevelCamera_) {
		cameraPos_ = levelCameraPos_;
		cameraRot_ = levelCameraRot_;
		AddLog("PLAYER role not found. Using Blender camera as fallback.");
	}
}

void GamePlaySessionSystem::OnScreenTransitionCovered() {
	farmRuntimeController_.CommitScreenTransition(RuntimeContext());
	PrepareRenderFrame(0.0f);
}

void GamePlaySessionSystem::PrepareFixedUpdate()
{
    layoutLibraryFrame_ = layoutLibraryRequested_;
    if (layoutLibraryRequested_) {
        layoutLibraryRequested_ = false;
        pendingPlayerCommand_ = {};
        farmRuntimeController_.OpenLayoutLibrary(RuntimeContext());
        return;
    }
	pendingPlayerCommand_.moveDirection = {};
	pendingPlayerCommand_.sneakHeld = false;
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	bool keyboardCaptured = ImGuiManager::GetInstance()->WantsCaptureKeyboard();
#ifndef USE_IMGUI
	const WinApp* runtimeWindow = framework_ ? framework_->GetWinApp() : nullptr;
	viewportHovered_ = viewportFocused_ = runtimeWindow && GetForegroundWindow() == runtimeWindow->GetHwnd();
	viewportImageTopLeft_ = {0, 0};
	// Mouse coordinates must use the actual client size after F11 or a window resize.
	viewportImageSize_ = runtimeWindow ? Vector2{static_cast<float>(runtimeWindow->GetClientWidth()), static_cast<float>(runtimeWindow->GetClientHeight())} : Vector2{};
	if (input) {
		viewportMousePosition_ = input->GetMousePosition();
		keyboardCaptured = farmRuntimeController_.Update(RuntimeContext(), *input) || keyboardCaptured;
		if (farmRuntimeController_.RequestsTitle()) {
			if (sceneManager_) sceneManager_->ChangeSceneWithRipple("TITLE");
			return;
		}
	}
#endif
	const bool controlHeld = input && (input->PushKey(DIK_LCONTROL) || input->PushKey(DIK_RCONTROL));
	const bool rewindKeyHeld = input && !keyboardCaptured && !controlHeld && input->PushKey(DIK_R);
	const bool shiftHeld = input && (input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT));
	const bool wasScrubbing = timelineScrubbing_;
	timelineForwardHeld_ = (rewindKeyHeld && shiftHeld) || timelineStepForwardRequested_;
	timelineRewindHeld_ = (rewindKeyHeld && !shiftHeld) || timelineStepBackwardRequested_;
	timelineScrubbing_ = timeline_.IsInitialized() && (timelineRewindHeld_ || timelineForwardHeld_);
	if (framework_ && framework_->GetAudio()) {
		if (timelineScrubbing_) {
			framework_->GetAudio()->SetGlobalTemporalState(
				timelineForwardHeld_
					? AudioPlaybackDirection::Forward
					: AudioPlaybackDirection::Reverse,
				kTimelineScrubAudioRate,
				kTimelineAudioEnterTransitionSeconds);
		} else if (wasScrubbing) {
			framework_->GetAudio()->SetGlobalTemporalState(
				AudioPlaybackDirection::Forward,
				1.0f,
				kTimelineAudioExitTransitionSeconds);
		}
	}
	if (timelineScrubbing_ && !wasScrubbing) {
		farmToolActionSystem_.ClearHistory();
		if (framework_ && framework_->GetParticleManager()) {
			framework_->GetParticleManager()->ClearAll();
			framework_->GetParticleManager()->ResetGPUParticles();
		}
	}
	if (timelineScrubbing_) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}

	if (!framework_ || !levelGameplay_.HasPlayer() || keyboardCaptured) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}

	if (!input) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}
	if (!usePlayerCamera_ && input->PushMouseButton(InputMouseButton::Right)) {
		pendingPlayerCommand_.jumpPressed = false;
		return;
	}

	if (input->PushKey(InputKey::W)) { pendingPlayerCommand_.moveDirection.z += 1.0f; }
	if (input->PushKey(InputKey::S)) { pendingPlayerCommand_.moveDirection.z -= 1.0f; }
	if (input->PushKey(InputKey::D)) { pendingPlayerCommand_.moveDirection.x += 1.0f; }
	if (input->PushKey(InputKey::A)) { pendingPlayerCommand_.moveDirection.x -= 1.0f; }
	pendingPlayerCommand_.jumpPressed = pendingPlayerCommand_.jumpPressed || input->TriggerKey(DIK_SPACE);
	pendingPlayerCommand_.sneakHeld =
		input->PushKey(InputKey::LeftShift) ||
		input->PushKey(InputKey::RightShift);
}

void GamePlaySessionSystem::SyncLevelGameplayPresentation()
{
	UpdateLevelPlayerVisual();

	for (const std::size_t renderIndex : levelGameplay_.ConsumeCollectedRenderIndices()) {
		if (renderIndex >= levelObjects_.size()) {
			continue;
		}
		LevelObjectRuntime& collectible = levelObjects_[renderIndex];
		if (!collectible.visible) {
			continue;
		}
		collectible.visible = false;
		EmitSpark(collectible.basePosition);
		AddLog("Collected: " + collectible.name);
	}

	for (const int32_t eventId : levelGameplay_.ConsumeTriggeredEventIds()) {
		if (eventId == level::LevelGameplaySystem::kStageClearEventId) {
			AddLog("Stage clear trigger activated.");
		} else {
			AddLog("Event trigger activated: " + std::to_string(eventId));
		}
	}
}

void GamePlaySessionSystem::InitializeTimeline()
{
	timeline_.Clear();
	CaptureTimelineSnapshot(timelineScratch_);
	timeline_.Initialize(kTimelineSnapshotCapacity, timelineScratch_);
	timelineRewindHeld_ = false;
	timelineForwardHeld_ = false;
	timelineScrubbing_ = false;
	timelineStepBackwardRequested_ = false;
	timelineStepForwardRequested_ = false;
}

void GamePlaySessionSystem::CaptureTimelineSnapshot(GameplaySnapshot& snapshot) const
{
	levelGameplay_.CaptureSnapshot(snapshot.levelGameplay);
	farmGrid_.CaptureSnapshot(snapshot.farmGrid);
	snapshot.farmDate = farmDateSystem_.CaptureSnapshot();
	snapshot.farmEconomy = farmEconomySystem_.CaptureSnapshot();
	snapshot.farmCropSelection = farmCropSelectionSystem_.CaptureSnapshot();
	snapshot.farmProgression = farmProgressionSystem_.CaptureSnapshot();
	snapshot.farmTool = farmToolSystem_.GetCurrentTool();
	snapshot.levelRouteTimer = levelRouteTimer_;
	snapshot.playerAnimationSpeed = playerAnimationSpeed_;
	snapshot.playerAnimationTime = 0.0f;
	if (levelGameplay_.HasPlayer()) {
		const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
		if (playerIndex < levelObjects_.size() && levelObjects_[playerIndex].object) {
			snapshot.playerAnimationTime = levelObjects_[playerIndex].object->GetAnimationTime();
		}
	}
}

bool GamePlaySessionSystem::RestoreTimelineSnapshot(const GameplaySnapshot& snapshot)
{
	if (!std::isfinite(snapshot.levelRouteTimer) || snapshot.levelRouteTimer < 0.0f ||
		!std::isfinite(snapshot.playerAnimationTime) || snapshot.playerAnimationTime < 0.0f ||
		!std::isfinite(snapshot.playerAnimationSpeed)) {
		return false;
	}
	if (!levelGameplay_.RestoreSnapshot(snapshot.levelGameplay) ||
		!farmGrid_.RestoreSnapshot(snapshot.farmGrid) ||
		!farmDateSystem_.RestoreSnapshot(snapshot.farmDate) ||
		!farmEconomySystem_.RestoreSnapshot(snapshot.farmEconomy) ||
		!farmCropSelectionSystem_.RestoreSnapshot(snapshot.farmCropSelection) ||
		!farmProgressionSystem_.RestoreSnapshot(snapshot.farmProgression)) {
		return false;
	}

	farmToolSystem_.SetTool(snapshot.farmTool);
	farmToolActionSystem_.ClearHistory();
	farmFeedbackSystem_.Clear();
	farmDocumentSystem_.MarkDirty();
	levelRouteTimer_ = snapshot.levelRouteTimer;
	playerAnimationSpeed_ = snapshot.playerAnimationSpeed;
	pendingPlayerCommand_.jumpPressed = false;

	for (const auto& collectible : levelGameplay_.GetCollectibleColliders()) {
		if (collectible.renderIndex < levelObjects_.size()) {
			levelObjects_[collectible.renderIndex].visible = !collectible.collected;
		}
	}
	if (levelGameplay_.HasPlayer()) {
		const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
		if (playerIndex < levelObjects_.size() && levelObjects_[playerIndex].object) {
			Object3d* playerObject = levelObjects_[playerIndex].object.get();
			playerObject->SetPosition(levelGameplay_.GetPlayerPosition());
			playerObject->SetAnimationTime(snapshot.playerAnimationTime);
			playerObject->SetPlaybackSpeed(snapshot.playerAnimationSpeed);
		}
	}
	return true;
}

void GamePlaySessionSystem::UpdateLevelPlayerVisual()
{
	if (!levelGameplay_.HasPlayer()) {
		return;
	}

	const std::size_t playerIndex = levelGameplay_.GetPlayerRenderIndex();
	if (playerIndex >= levelObjects_.size() || !levelObjects_[playerIndex].object) {
		return;
	}

	if (!levelGameplay_.IsPlayerGrounded()) {
		playerAnimationState_ = PlayerAnimationState::Airborne;
	} else if (!levelGameplay_.IsPlayerMoving()) {
		playerAnimationState_ = PlayerAnimationState::Idle;
	} else if (levelGameplay_.IsPlayerSneaking()) {
		playerAnimationState_ = PlayerAnimationState::Sneak;
	} else {
		playerAnimationState_ = PlayerAnimationState::Walk;
	}

#ifdef USE_IMGUI
	PlayerAnimationMode desiredMode = playerAnimationMode_;
	if (playerAnimationState_ == PlayerAnimationState::Walk) {
		desiredMode = PlayerAnimationMode::Walk;
	} else if (playerAnimationState_ == PlayerAnimationState::Sneak) {
		desiredMode = PlayerAnimationMode::Sneak;
	}
	if (!playerVisualConfigured_ || desiredMode != playerAnimationMode_) {
		ConfigureLevelPlayerAnimation(playerIndex, desiredMode);
	}
#endif

	Object3d* playerObject = levelObjects_[playerIndex].object.get();
	playerObject->SetPosition(levelGameplay_.GetPlayerPosition());
	if (levelGameplay_.IsPlayerMoving()) {
		const Vector3& facing = levelGameplay_.GetPlayerFacingDirection();
		playerObject->SetRotation({ 0.0f, std::atan2(facing.x, facing.z), 0.0f });
	}
#ifdef USE_IMGUI
	const PlayerAnimationClip& clip = GetPlayerAnimationClip(playerAnimationMode_);
	float targetSpeed = clip.playbackSpeed;
	if (playerAnimationState_ == PlayerAnimationState::Idle) {
		targetSpeed = 0.0f;
	} else if (playerAnimationState_ == PlayerAnimationState::Airborne) {
		targetSpeed *= 0.35f;
	}
	const float maxSpeedChange = sceneDeltaTime_ / kPlayerAnimationStopBlendSeconds;
	playerAnimationSpeed_ = MoveTowards(playerAnimationSpeed_, targetSpeed, maxSpeedChange);
	playerObject->SetPlaybackSpeed(playerAnimationSpeed_);
	playerObject->SetAnimationPlaying(playerAnimationSpeed_ > 0.001f);
#else
	playerObject->SetAnimationPlaying(false);
#endif

}

const GamePlaySessionSystem::PlayerAnimationClip& GamePlaySessionSystem::GetPlayerAnimationClip(PlayerAnimationMode mode)
{
	static constexpr PlayerAnimationClip walk{ "walk.gltf", 0.65f };
	static constexpr PlayerAnimationClip sneak{ "sneakWalk.gltf", 0.45f };
	return mode == PlayerAnimationMode::Sneak ? sneak : walk;
}

void GamePlaySessionSystem::ConfigureLevelPlayerAnimation(std::size_t playerRenderIndex, PlayerAnimationMode mode)
{
	if (!framework_ || playerRenderIndex >= levelObjects_.size() || !levelObjects_[playerRenderIndex].object) {
		return;
	}

	const PlayerAnimationClip& clip = GetPlayerAnimationClip(mode);
	const char* animationFileName = clip.fileName;
	Model* playerModel = framework_->GetModelManager()->GetModel(std::string("human/") + animationFileName);
	if (!playerModel) {
		AddLog("Player model is not loaded: " + std::string(animationFileName));
		return;
	}

	Object3d* playerObject = levelObjects_[playerRenderIndex].object.get();
	float normalizedTime = 0.0f;
	if (playerVisualConfigured_ && playerObject->GetAnimation().duration > 0.0f) {
		normalizedTime = playerObject->GetAnimationTime() / playerObject->GetAnimation().duration;
	}
	playerObject->SetModel(playerModel);
	playerObject->SetTexture(levelWhiteTextureHandle_);
	playerObject->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	playerObject->SetScale({ 0.75f, 0.75f, 0.75f });
	playerObject->SetAnimation(playerModel->LoadAnimation("Resources/human", animationFileName));
	playerObject->SetAnimationTime(playerObject->GetAnimation().duration * std::clamp(normalizedTime, 0.0f, 1.0f));
	playerObject->SetAnimationPlaying(true);
	playerAnimationMode_ = mode;
	playerVisualConfigured_ = true;
	AddLog(std::string("Player animation: ") + (mode == PlayerAnimationMode::Sneak ? "sneak walk" : "walk"));
}

void GamePlaySessionSystem::CollectLevelRuntimeData() {
	levelRoutePoints_.clear();
	if (!levelData_) {
		return;
	}

	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.disabled) {
			continue;
		}
		if (objectData.type == kPatrolPointTypeName) {
			levelRoutePoints_.push_back(objectData.transform.translation);
		}
	}
}

void GamePlaySessionSystem::ApplyLevelCamera() {
	hasLevelCamera_ = false;
	if (!levelData_) {
		return;
	}

	for (const level::ObjectData& objectData : levelData_->objects) {
		if (objectData.disabled || objectData.type != kCameraTypeName) {
			continue;
		}

		levelCameraPos_ = objectData.transform.translation;
		levelCameraRot_ = objectData.transform.rotation;
		levelCameraRot_.x = std::clamp(levelCameraRot_.x, -1.5f, 1.5f);
		hasLevelCamera_ = true;
		AddLog("Blender camera loaded as fallback: " + objectData.name);
		return;
	}

	AddLog("Blender camera not found. PLAYER camera requires a PLAYER role.");
}

void GamePlaySessionSystem::DrawLevelBox(const Vector3& center, const Vector3& size, const Vector4& color) const {
	const Vector3 half = {
		size.x * 0.5f,
		size.y * 0.5f,
		size.z * 0.5f,
	};
	const Vector3 v[8] = {
		{ center.x - half.x, center.y - half.y, center.z - half.z },
		{ center.x + half.x, center.y - half.y, center.z - half.z },
		{ center.x + half.x, center.y + half.y, center.z - half.z },
		{ center.x - half.x, center.y + half.y, center.z - half.z },
		{ center.x - half.x, center.y - half.y, center.z + half.z },
		{ center.x + half.x, center.y - half.y, center.z + half.z },
		{ center.x + half.x, center.y + half.y, center.z + half.z },
		{ center.x - half.x, center.y + half.y, center.z + half.z },
	};
	const int edges[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0},
		{4, 5}, {5, 6}, {6, 7}, {7, 4},
		{0, 4}, {1, 5}, {2, 6}, {3, 7},
	};

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	for (const auto& edge : edges) {
		lineDrawer->DrawLine(v[edge[0]], v[edge[1]], color);
	}
}

void GamePlaySessionSystem::DrawLevelCameraGizmo(const Vector3& position, const Vector4& color) const {
	const Vector3 nearCenter = AddVector3(position, { 0.0f, 0.0f, 0.6f });
	const Vector3 farCenter = AddVector3(position, { 0.0f, 0.0f, 1.8f });
	const Vector3 farCorners[4] = {
		{ farCenter.x - 0.8f, farCenter.y + 0.5f, farCenter.z },
		{ farCenter.x + 0.8f, farCenter.y + 0.5f, farCenter.z },
		{ farCenter.x + 0.8f, farCenter.y - 0.5f, farCenter.z },
		{ farCenter.x - 0.8f, farCenter.y - 0.5f, farCenter.z },
	};

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	lineDrawer->DrawWireSphere(position, 0.25f, color, 12);
	for (const Vector3& corner : farCorners) {
		lineDrawer->DrawLine(position, corner, color);
	}
	lineDrawer->DrawLine(farCorners[0], farCorners[1], color);
	lineDrawer->DrawLine(farCorners[1], farCorners[2], color);
	lineDrawer->DrawLine(farCorners[2], farCorners[3], color);
	lineDrawer->DrawLine(farCorners[3], farCorners[0], color);
	lineDrawer->DrawLine(position, nearCenter, color);
}

void GamePlaySessionSystem::DrawLevelCollisionGizmos() const {
	if (!showLevelCollisionGizmos_) {
		return;
	}

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	if (levelGameplay_.HasPlayer()) {
		const Vector3& half = levelGameplay_.GetPlayerColliderHalfExtents();
		DrawLevelBox(
			levelGameplay_.GetPlayerColliderCenter(),
			{ half.x * 2.0f, half.y * 2.0f, half.z * 2.0f },
			{ 0.1f, 1.0f, 1.0f, 1.0f });
	}
	if (levelGameplay_.HasGround()) {
		const Vector3& half = levelGameplay_.GetGroundHalfExtents();
		DrawLevelBox(levelGameplay_.GetGroundCenter(), { half.x * 2.0f, half.y * 2.0f, half.z * 2.0f }, { 0.2f, 1.0f, 0.25f, 1.0f });
	}
	for (const level::LevelGameplaySystem::ObstacleCollider& obstacle : levelGameplay_.GetObstacleColliders()) {
		DrawLevelBox(obstacle.center, { obstacle.halfExtents.x * 2.0f, obstacle.halfExtents.y * 2.0f, obstacle.halfExtents.z * 2.0f }, { 1.0f, 0.2f, 0.2f, 1.0f });
	}
	for (const level::LevelGameplaySystem::CollectibleCollider& collectible : levelGameplay_.GetCollectibleColliders()) {
		if (!collectible.collected) {
			lineDrawer->DrawWireSphere(collectible.position, collectible.radius, { 1.0f, 0.85f, 0.1f, 1.0f }, 16);
		}
	}
	for (const level::LevelGameplaySystem::EventTriggerCollider& trigger : levelGameplay_.GetEventTriggerColliders()) {
		const Vector4 color = trigger.activated
			? Vector4{ 0.35f, 0.35f, 0.35f, 1.0f }
			: Vector4{ 0.2f, 1.0f, 0.45f, 1.0f };
		DrawLevelBox(
			trigger.center,
			{ trigger.halfExtents.x * 2.0f, trigger.halfExtents.y * 2.0f, trigger.halfExtents.z * 2.0f },
			color);
	}
}

Vector3 GamePlaySessionSystem::EvaluateLevelRoutePoint(float normalizedTime) const {
	if (levelRoutePoints_.empty()) {
		return { 0.0f, 0.0f, 0.0f };
	}
	if (levelRoutePoints_.size() == 1) {
		return levelRoutePoints_.front();
	}

	const float clampedTime = normalizedTime - std::floor(normalizedTime);
	const float scaledTime = clampedTime * static_cast<float>(levelRoutePoints_.size());
	const std::size_t currentIndex = static_cast<std::size_t>(std::floor(scaledTime)) % levelRoutePoints_.size();
	const std::size_t nextIndex = (currentIndex + 1) % levelRoutePoints_.size();
	const float localTime = scaledTime - std::floor(scaledTime);
	return LerpVector3(levelRoutePoints_[currentIndex], levelRoutePoints_[nextIndex], localTime);
}

void GamePlaySessionSystem::DrawLevelDebugGizmos() const {
	if (!showLevelGizmos_ || !levelData_) {
		return;
	}

	const Vector4 meshColliderColor = { 0.1f, 0.75f, 1.0f, 1.0f };
	const Vector4 spawnColor = { 0.1f, 1.0f, 0.35f, 1.0f };
	const Vector4 triggerColor = { 1.0f, 0.55f, 0.05f, 1.0f };
	const Vector4 gimmickColor = { 0.85f, 0.35f, 1.0f, 1.0f };
	const Vector4 disabledColor = { 1.0f, 0.1f, 0.1f, 1.0f };
	const Vector4 routeColor = { 0.15f, 0.9f, 1.0f, 1.0f };
	const Vector4 routeActorColor = { 1.0f, 0.9f, 0.15f, 1.0f };
	const Vector4 cameraColor = { 1.0f, 0.35f, 0.9f, 1.0f };
	const Vector4 lightColor = { 1.0f, 0.95f, 0.2f, 1.0f };

	LineDrawer* lineDrawer = LineDrawer::GetInstance();
	for (const level::ObjectData& objectData : levelData_->objects) {
		const Vector3 position = objectData.transform.translation;
		if (objectData.disabled) {
			DrawLevelBox(position, { 1.2f, 1.2f, 1.2f }, disabledColor);
			lineDrawer->DrawLine(
				AddVector3(position, { -0.8f, -0.8f, -0.8f }),
				AddVector3(position, { 0.8f, 0.8f, 0.8f }),
				disabledColor);
			lineDrawer->DrawLine(
				AddVector3(position, { -0.8f, 0.8f, -0.8f }),
				AddVector3(position, { 0.8f, -0.8f, 0.8f }),
				disabledColor);
			continue;
		}

		if (objectData.hasCollider) {
			DrawLevelBox(AddVector3(position, objectData.collider.center), objectData.collider.size, meshColliderColor);
		}
		if (objectData.type == kSpawnPointTypeName) {
			lineDrawer->DrawWireSphere(position, 0.5f, spawnColor, 16);
			lineDrawer->DrawLine(position, AddVector3(position, { 0.0f, 1.6f, 0.0f }), spawnColor);
		} else if (objectData.type == kEventTriggerTypeName) {
			const Vector3 size = objectData.hasCollider ? objectData.collider.size : Vector3{ 2.0f, 2.0f, 2.0f };
			const Vector3 center = objectData.hasCollider ? AddVector3(position, objectData.collider.center) : position;
			DrawLevelBox(center, size, triggerColor);
		} else if (objectData.type == kGimmickTypeName) {
			const Vector3 size = objectData.hasCollider ? objectData.collider.size : Vector3{ 1.0f, 2.5f, 0.3f };
			const Vector3 center = objectData.hasCollider ? AddVector3(position, objectData.collider.center) : position;
			DrawLevelBox(center, size, gimmickColor);
		} else if (objectData.type == kPatrolPointTypeName) {
			lineDrawer->DrawWireSphere(position, 0.25f, routeColor, 12);
		} else if (objectData.type == kCameraTypeName) {
			DrawLevelCameraGizmo(position, cameraColor);
		} else if (objectData.type == kLightTypeName) {
			lineDrawer->DrawWireSphere(position, 0.35f, lightColor, 12);
			lineDrawer->DrawLine(position, AddVector3(position, { 0.0f, -1.0f, 0.0f }), lightColor);
		}
	}

	if (levelRoutePoints_.size() >= 2) {
		for (std::size_t i = 0; i < levelRoutePoints_.size(); ++i) {
			const Vector3& current = levelRoutePoints_[i];
			const Vector3& next = levelRoutePoints_[(i + 1) % levelRoutePoints_.size()];
			lineDrawer->DrawLine(current, next, routeColor);
		}

		const Vector3 actorPosition = EvaluateLevelRoutePoint(levelRouteTimer_ * 0.12f);
		lineDrawer->DrawWireSphere(actorPosition, 0.45f, routeActorColor, 16);
		lineDrawer->DrawLine(actorPosition, AddVector3(actorPosition, { 0.0f, 1.0f, 0.0f }), routeActorColor);
	}
}

/**
 * RebuildCylinder: Cylinderメッシュの再生成
 * ImGuiでパラメータを変更したときに呼ばれる
 * 新しいメッシュを生成し、ParticleGroupのモデルポインタも更新する
 */
void GamePlaySessionSystem::RebuildCylinder() {
	cylinderModel_ = PrimitiveGenerator::CreateCylinder(
		framework_->GetModelManager(),
		cylTopRadius_, cylBottomRadius_, cylHeight_,
		static_cast<uint32_t>(cylSegments_),
		static_cast<uint32_t>(cylVertDivisions_));
	// ParticleGroupのモデルポインタも更新しないと古いメッシュを参照し続ける
	framework_->GetParticleManager()->CreateParticleGroup(
		"CylinderEffect",
		ringTexHandle_,
		cylinderModel_.get());
}

void GamePlaySessionSystem::Draw() {
	auto objCommon = framework_->GetObject3dCommon();
	auto spriteCommon = framework_->GetSpriteCommon();
#ifndef USE_IMGUI
    if (farmRuntimeController_.ShowsContestDetail()) {
        farmContestRenderer_.Draw(farmRuntimeController_.ContestFrame());
        spriteCommon->PreDraw(); farmRuntimeController_.Draw();
        return;
    }
    if (farmRuntimeController_.ShowsSeedShop() || farmRuntimeController_.ShowsHarvestDisplay()) {
		objCommon->BeginObjectPass();
        farmSeedShopRenderer_.Draw(farmRuntimeController_.ShowsSeedShop() ? farmRuntimeController_.SeedShopSelection() % 2 : farmRuntimeController_.HarvestDisplaySelection());
		objCommon->EndObjectPass();
		spriteCommon->PreDraw();
		farmRuntimeController_.Draw();
		return;
	}
#endif
	const farm::FarmGrid* previewGrid = farmIrrigationPreviewSystem_.GetPreviewGrid();
	const farm::FarmIrrigationSystem* previewIrrigation = farmIrrigationPreviewSystem_.GetPreviewIrrigation();
	const bool irrigationPreviewActive = previewGrid != nullptr && previewIrrigation != nullptr;
	const farm::FarmGrid& displayedFarmGrid = irrigationPreviewActive ? *previewGrid : farmGrid_;
    const bool arrivalScenery = farmGameMode_ && farmArrivalRenderer_.IsReady() &&
        displayedFarmGrid.GetWidth()==5 && displayedFarmGrid.GetHeight()==4;
    farmArrivalRenderer_.Prepare(camera_.get(),farmRuntimeController_.StartFrame(),arrivalScenery,realDeltaTime_);
	const farm::FarmIrrigationSystem& displayedIrrigation = irrigationPreviewActive ? *previewIrrigation : farmIrrigationSystem_;
	farmRenderer_.Prepare(displayedFarmGrid, farmVisualSystem_, camera_.get(),
		farmRuntimeController_.ResolveHoveredTile(RuntimeContext()), irrigationPreviewActive ? nullptr : &farmHarvestVisualSystem_,
        !farmRuntimeController_.PresentsIntroduction());

	auto DrawLevelObjects = [&]() {
		if (!showLevelObjects_) {
			return;
		}
		for (const LevelObjectRuntime& levelObject : levelObjects_) {
			if (arrivalScenery && levelObject.gameplayRole == kGroundRoleName) continue;
			if (levelObject.visible && levelObject.object) {
				levelObject.object->Draw();
			}
		}
		};
	auto DrawLevelObjectShadows = [&]() {
		if (!showLevelObjects_) {
			return;
		}
		for (const LevelObjectRuntime& levelObject : levelObjects_) {
			if (arrivalScenery && levelObject.gameplayRole == kGroundRoleName) continue;
			if (levelObject.visible && levelObject.object) {
				levelObject.object->DrawShadow();
			}
		}
		};

	// Shadow pass must complete before the color pass reads shadow data.
	objCommon->SetShadowStrength(directionalShadowsEnabled_ ? directionalShadowStrength_ : 0.0f);
	if (directionalShadowsEnabled_) {
		Vector3 shadowFocus = levelGameplay_.HasPlayer()
			? levelGameplay_.GetPlayerPosition()
			: Vector3{ 0.0f, 0.0f, 0.0f };
		shadowFocus.y += 4.0f;
		objCommon->UpdateDirectionalShadow(framework_->GetLightingSystem()->GetDirectionalLight().direction, shadowFocus);
		if (objCommon->BeginShadowPass()) {
			if (showTerrain_ && terrainObj_) terrainObj_->DrawShadow();
			if (showSphere_ && sphereObj_) sphereObj_->DrawShadow();
			if (showPlane_ && object3d_) object3d_->DrawShadow();
			if (showAnimModel_ && animObj_) animObj_->DrawShadow();
			DrawLevelObjectShadows();
			farmRenderer_.DrawShadow();
            farmArrivalRenderer_.DrawShadow();
			objCommon->EndShadowPass();
		}
	}
	// === 地面のグリッド（格子線）の描画 ===
	const float gridScale = 20.0f; // グリッドの広さ
	const int divCount = 10;       // 分割数
	const Vector4 gridColor = { 0.5f, 0.5f, 0.5f, 1.0f }; // グレー

	for (int i = -divCount; !farmGameMode_ && showDebugGrid_ && i <= divCount; ++i) {
		float f = (float)i / (float)divCount * gridScale;
		LineDrawer::GetInstance()->DrawLine({ -gridScale, -2.0f, f }, { gridScale, -2.0f, f }, gridColor);
		LineDrawer::GetInstance()->DrawLine({ f, -2.0f, -gridScale }, { f, -2.0f, gridScale }, gridColor);
	}

	farmVisualSystem_.Draw(
		displayedFarmGrid,
		displayedIrrigation,
		farmToolActionSystem_.EvaluateTool(
			displayedFarmGrid, farmToolSystem_.GetCurrentTool(),
			farmCropSelectionSystem_.GetSelectedCrop(), &farmEconomySystem_),
		*LineDrawer::GetInstance(),
		irrigationPreviewActive
			? &farmIrrigationPreviewSystem_.GetChangedTileIndices()
			: nullptr,
		!farmGameMode_ || !farmRenderer_.IsReady(),
		!farmRenderer_.IsReady() || !farmRenderer_.IsVisible() || !farmRenderer_.HasCropMeshes() || farmRenderer_.IsLimitExceeded());

	if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction &&
		interactionBrushOperation_ != InteractionBrushOperation::None) {
		const Vector4 brushColor =
			interactionBrushOperation_ == InteractionBrushOperation::Pull
			? Vector4{ 0.1f, 0.75f, 1.0f, 1.0f }
			: Vector4{ 1.0f, 0.35f, 0.05f, 1.0f };
		LineDrawer::GetInstance()->DrawWireSphere(
			interactionBrushPosition_,
			(std::max)(interactionBrushRadius_, 0.01f),
			brushColor,
			24);
	}

	if (!farmGameMode_) {
		DrawLevelDebugGizmos();
		DrawLevelCollisionGizmos();
	}

	if (modelPriority_ == 0) {
        farmArrivalRenderer_.DrawSky();
		objCommon->BeginObjectPass();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		DrawLevelObjects();
		farmRenderer_.Draw();
        farmArrivalRenderer_.Draw();
		objCommon->EndObjectPass();
		if (skyboxEnabled_ && skybox_) skybox_->Draw();

		if (showSkeleton_ && animObj_ && animObj_->GetSkeleton()) {
			// 【修正】モデルルート行列の二重掛けを防ぐため、GetWorldMatrix() ではなくルートを含まない GetObjectWorldMatrix() を渡します
			skeletonDebugger_->Draw(*animObj_->GetSkeleton(), animObj_->GetObjectWorldMatrix(), LineDrawer::GetInstance(), camera_.get());
		}
		// その直後に呼ばれる
		LineDrawer::GetInstance()->Draw(camera_->GetViewProjectionMatrix());
	}
	else {
		spriteCommon->PreDraw();
		if (showSprite_) sprite_->Draw();

        farmArrivalRenderer_.DrawSky();
		objCommon->BeginObjectPass();
		if (showTerrain_ && terrainObj_) terrainObj_->Draw();
		if (showSphere_ && sphereObj_)   sphereObj_->Draw();
		if (showPlane_ && object3d_)    object3d_->Draw();
		if (showAnimModel_ && animObj_) animObj_->Draw();
		DrawLevelObjects();
		farmRenderer_.Draw();
        farmArrivalRenderer_.Draw();
		objCommon->EndObjectPass();
		if (skyboxEnabled_ && skybox_) skybox_->Draw();

		if (showSkeleton_ && animObj_ && animObj_->GetSkeleton()) {
			// 【修正】モデルルート行列の二重掛けを防ぐため、GetWorldMatrix() ではなくルートを含まない GetObjectWorldMatrix() を渡します
			skeletonDebugger_->Draw(*animObj_->GetSkeleton(), animObj_->GetObjectWorldMatrix(), LineDrawer::GetInstance(), camera_.get());
		}
		LineDrawer::GetInstance()->Draw(camera_->GetViewProjectionMatrix());
	}

	// ParticleManager performs compute updates before its graphics SRV read.
	if (showParticles_) {
		ParticleManager* particleManager = framework_->GetParticleManager();
		if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
			particleManager->Draw(false);

			GPUParticleInteractionSettings settings{};
			const uint32_t gridCount = static_cast<uint32_t>(std::clamp(interactionGridCount_, 1, 10));
			settings.gridCenter = interactionGridCenter_;
			settings.particleSize = std::clamp(interactionParticleSize_, 0.02f, 0.05f);
			settings.gridCountX = gridCount;
			settings.gridCountY = gridCount;
			settings.gridCountZ = gridCount;
			settings.particleCount = CalculateInteractionParticleCount();
			settings.brushPosition = interactionBrushPosition_;
			settings.brushRadius = (std::max)(interactionBrushRadius_, 0.01f);
			settings.brushStrength = (std::max)(interactionBrushStrength_, 0.0f);
			settings.operation = static_cast<uint32_t>(interactionBrushOperation_);
			settings.isPressed = settings.operation != static_cast<uint32_t>(InteractionBrushOperation::None) ? 1u : 0u;
			settings.deltaTime = sceneDeltaTime_;
			settings.damping = std::clamp(interactionDamping_, 0.80f, 0.995f);
			if (interactionResetRequested_) {
				particleManager->InitializeGPUParticleInteraction(settings);
				interactionResetRequested_ = false;
			}
			particleManager->UpdateGPUParticleInteraction(settings);
			particleManager->DrawGPUParticleBuffer();
		} else {
			particleManager->Draw();
		}
	}

	// HUD is the final Scene overlay and does not mutate gameplay state.
	if (farmHudInitialized_ || stageClearHudInitialized_) {
		spriteCommon->PreDraw();
#ifndef USE_IMGUI
        if (!farmRuntimeController_.HidesFarmHUD()) {
#endif
		if (farmHudInitialized_) {
			farmHud_.Draw();
		}
		if (stageClearHudInitialized_) {
#ifndef USE_IMGUI
            // The native result/review UI already owns the farm-clear presentation.
            if (!farmRuntimeController_.PresentsFarmResult())
#endif
			stageClearHud_.Draw();
		}
#ifndef USE_IMGUI
		}
		farmRuntimeController_.Draw();
#endif
	}
}

FarmHUDViewData GamePlaySessionSystem::BuildFarmHUDViewData() const {
	FarmHUDViewData viewData;
	const farm::FarmGrid* previewGrid = farmIrrigationPreviewSystem_.GetPreviewGrid();
	const farm::FarmIrrigationSystem* previewIrrigation =
		farmIrrigationPreviewSystem_.GetPreviewIrrigation();
	viewData.irrigationPreviewActive = previewGrid != nullptr && previewIrrigation != nullptr;
	viewData.irrigationPreviewRemoval = farmIrrigationPreviewSystem_.GetOperation() ==
		farm::FarmIrrigationPreviewOperation::RemoveCanalPath;
	viewData.irrigationPreviewChangeCount = static_cast<int>(
		farmIrrigationPreviewSystem_.GetChangeCount());
	const farm::FarmGrid& displayedFarmGrid = viewData.irrigationPreviewActive
		? *previewGrid : farmGrid_;
	const farm::FarmIrrigationSystem& displayedIrrigation = viewData.irrigationPreviewActive
		? *previewIrrigation : farmIrrigationSystem_;
	viewData.day = farmDateSystem_.GetDay();
	viewData.money = farmEconomySystem_.GetMoney();
	viewData.rank = 1;
	viewData.cropCount = farmEconomySystem_.GetTotalCropCount();
	viewData.saleValue = farmEconomySystem_.GetSalePreviewValue();
	viewData.selectedSeedCrop = farmCropSelectionSystem_.GetSelectedCrop();
	viewData.seedCount = farmEconomySystem_.GetSeedCount(viewData.selectedSeedCrop);
	viewData.seedPrice = farmEconomySystem_.GetSeedPrice(viewData.selectedSeedCrop);
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		const std::size_t index = static_cast<std::size_t>(slot);
		const farm::CropType crop = farm::CropTypeFromSlot(slot);
		viewData.cropInventoryCounts[index] = farmEconomySystem_.GetCropCount(crop);
		viewData.cropInventoryValues[index] =
			farmEconomySystem_.GetCropInventoryValue(crop);
		viewData.cropSeedCounts[index] = farmEconomySystem_.GetSeedCount(crop);
	}
	viewData.timeScale = farmDateSystem_.GetTimeScale();
	viewData.currentToolIndex = static_cast<int>(farmToolSystem_.GetCurrentTool());
	const int cropsNeeded = farmProgressionSystem_.GetRequiredCropCount(
		farmEconomySystem_.GetMoney(),
		farmEconomySystem_.GetSellPrice(viewData.selectedSeedCrop));
	viewData.cropsNeeded = (std::max)(cropsNeeded, 0);
	viewData.goalMoney = farmProgressionSystem_.GetTargetMoney();
	viewData.goalProgress = farmProgressionSystem_.GetProgress(farmEconomySystem_.GetMoney(), farmDateSystem_.GetDay());
	viewData.goalCleared = farmProgressionSystem_.IsCleared();
	viewData.contestSeason = farmProgressionSystem_.IsContestSeason();
	viewData.contestFinalDay = FarmContestEntrySystem::kContestDays.back();
	viewData.freeFarming = farmProgressionSystem_.IsFreeFarming();
	const SelectedTileHUDData selectedTileData = BuildSelectedTileHUDData(
		displayedFarmGrid, displayedIrrigation, farmGrowthSystem_,
		farmDateSystem_.GetTimeScale(),
		viewData.selectedSeedCrop, viewData.seedCount);
	viewData.selectedTileValid = selectedTileData.valid;
	viewData.selectedTileMoistureRangeValid = selectedTileData.moistureRangeValid;
	viewData.selectedTileGoodMoistureMinimum = selectedTileData.goodMoistureMinimum;
	viewData.selectedTileGoodMoistureMaximum = selectedTileData.goodMoistureMaximum;
	viewData.selectedTileIndex = selectedTileData.index;
	viewData.selectedTileHeight = selectedTileData.height;
	viewData.selectedTileMoisturePercent = selectedTileData.moisturePercent;
	viewData.selectedTileStoredWaterPercent = selectedTileData.storedWaterPercent;
	viewData.selectedTileGrowthPercent = selectedTileData.growthPercent;
	viewData.selectedTileFeature = selectedTileData.feature;
	viewData.selectedTileIrrigationSupplied = selectedTileData.irrigationSupplied;
	viewData.selectedTileIrrigationActive = selectedTileData.irrigationActive;
	viewData.selectedTileWaterStatus = selectedTileData.waterStatus;
	viewData.selectedTileReceivedIrrigation = selectedTileData.receivedIrrigation;
	viewData.selectedTileIrrigationStrengthPercent =
		selectedTileData.irrigationStrengthPercent;
	viewData.selectedTileState = selectedTileData.state;
	viewData.selectedTileCrop = selectedTileData.crop;
	viewData.selectedTileGrowthStage = selectedTileData.growthStage;
	viewData.selectedTileMoistureStatus = selectedTileData.moistureStatus;
	viewData.cropPieOpen = farmCropSelectionSystem_.IsOpen();
	viewData.cropPieHovered = farmCropSelectionSystem_.GetHoveredCrop();
	viewData.cropPieCenter = farmCropSelectionSystem_.GetCenter();
	viewData.nextAction = selectedTileData.nextAction;
	viewData.feedback = farmFeedbackSystem_.GetCurrentMessage().empty()
		? FarmHUDFeedback::None
		: ToHUDFeedback(
			farmFeedbackSystem_.GetLastKind(),
			farmFeedbackSystem_.GetLastCrop());
	if (viewData.feedback == FarmHUDFeedback::Harvest ||
		viewData.feedback == FarmHUDFeedback::Sale) {
		viewData.feedbackCrop = farmFeedbackSystem_.GetLastCrop();
		viewData.feedbackQualityScore = farmFeedbackSystem_.GetLastQualityScore();
		viewData.feedbackSaleCount = farmFeedbackSystem_.GetLastSaleCount();
		viewData.feedbackSaleValue = farmFeedbackSystem_.GetLastSaleValue();
	}
	return viewData;
}

void GamePlaySessionSystem::HandleFarmDateDebugInput() {
	if (farmContestDaySystem_.PendingDay()) return;
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (!input) {
		return;
	}
	if (input->PushKey(DIK_LCONTROL) || input->PushKey(DIK_RCONTROL)) {
		return;
	}
	if (farmProgressionSystem_.IsCleared()) {
		if (input->TriggerKey(InputKey::T) || input->TriggerKey(InputKey::Y)) {
			farmFeedbackSystem_.ShowClearLocked();
		}
		return;
	}

	if (input->TriggerKey(InputKey::T)) {
		farmDateSystem_.CycleTimeScale();
		farmDocumentSystem_.MarkDirty();
	}
	if (input->TriggerKey(InputKey::Y)) {
		farmDateSystem_.AdvanceOneDay();
		farmDocumentSystem_.MarkDirty();
	}
}

bool GamePlaySessionSystem::HandleFarmInput() {
#ifndef USE_IMGUI
	if (farmRuntimeController_.ConsumesInput()) return true;
#endif
	if (!framework_) {
		return false;
	}

	Input* input = framework_->GetInput();
	if (!input) {
		return false;
	}
	FarmInputContext context{};
	context.currentDay = farmDateSystem_.GetDay();
	context.keyboardEnabled = viewportFocused_ &&
		!ImGuiManager::GetInstance()->WantsTextInput();
	context.cameraDragActive = viewportHovered_ &&
		input->PushMouseButton(InputMouseButton::Right);
	context.directToolSelectionEnabled =
		gpuParticleDebugMode_ != GPUParticleDebugMode::Agriculture;
	if (farmProgressionSystem_.IsCleared()) {
		farmCropSelectionSystem_.Cancel();
		const FarmLockedInputResult lockedInput =
			farmInputSystem_.PollLockedInput(*input, context);
		if (lockedInput.actionTriggered) {
			farmFeedbackSystem_.ShowClearLocked();
		}
		return lockedInput.navigationInputConsumed;
	}

	if (context.keyboardEnabled && viewportHovered_ &&
		input->TriggerKey(InputKey::C)) {
		Vector2 virtualMouse{};
		if (ConvertMouseToVirtualScreen(viewportMousePosition_, virtualMouse)) {
			virtualMouse.x = std::clamp(virtualMouse.x, 190.0f, 1090.0f);
			virtualMouse.y = std::clamp(virtualMouse.y, 130.0f, 500.0f);
			static_cast<void>(farmCropSelectionSystem_.Open(virtualMouse));
		}
	}
	if (farmCropSelectionSystem_.IsOpen()) {
		if (!context.keyboardEnabled || !viewportHovered_ ||
			input->TriggerKey(InputKey::Escape)) {
			farmCropSelectionSystem_.Cancel();
			return true;
		}
		Vector2 virtualMouse{};
		if (ConvertMouseToVirtualScreen(viewportMousePosition_, virtualMouse)) {
			farmCropSelectionSystem_.UpdatePointer(virtualMouse);
		}
		if (input->ReleaseKey(InputKey::C)) {
			const farm::CropType hoveredCrop = farmCropSelectionSystem_.GetHoveredCrop();
			const bool selectionChanged = farmCropSelectionSystem_.Confirm();
			if (selectionChanged) {
				farmDocumentSystem_.MarkDirty();
			}
			if (farm::IsPlantableCrop(hoveredCrop)) {
				farmFeedbackSystem_.ShowCropSelected(
					farmCropSelectionSystem_.GetSelectedCrop());
			}
		}
		return true;
	}

	const FarmInputResult result = farmInputSystem_.Update(
		*input, context, farmGrid_, farmToolSystem_,
		farmCropSelectionSystem_.GetSelectedCrop(),
		farmEconomySystem_, farmToolActionSystem_);
	if (result.contentChanged) {
		farmDocumentSystem_.MarkDirty();
	}
	RouteFarmToolFeedback(result.toolAction);
	if (result.buySeedRequested) {
		const FarmSeedPurchaseResult purchaseResult = farmEconomySystem_.BuySeed(
			farmCropSelectionSystem_.GetSelectedCrop());
		if (purchaseResult.Succeeded()) {
			farmDocumentSystem_.MarkDirty();
			AddLog(
				"Bought " + std::to_string(purchaseResult.purchasedCount) +
				" seed(s) for " + std::to_string(purchaseResult.spentMoney) + "G.");
			farmFeedbackSystem_.ShowSeedPurchased(
				purchaseResult.crop, purchaseResult.purchasedCount,
				purchaseResult.spentMoney);
		} else if (purchaseResult.status == FarmSeedPurchaseStatus::InsufficientMoney) {
			farmFeedbackSystem_.ShowInsufficientMoney();
		} else {
			AddLog("Seed purchase rejected by FarmEconomySystem.");
		}
	}
	if (result.sellRequested) {
		RouteFarmSale(farmEconomySystem_.SellAll());
	} else if (result.sellSelectedRequested) {
		RouteFarmSale(farmEconomySystem_.SellCrop(
			farmCropSelectionSystem_.GetSelectedCrop()));
	}

#ifndef USE_IMGUI
	if (viewportHovered_ && input->TriggerMouseButton(InputMouseButton::Left) &&
		TrySelectFarmTileFromViewport()) {
		const bool quickApply = input->PushKey(InputKey::LeftShift) ||
			input->PushKey(InputKey::RightShift);
		if (quickApply) {
			const FarmToolActionResult actionResult = farmToolActionSystem_.ApplyToolDetailed(
				farmGrid_, farmToolSystem_.GetCurrentTool(),
				farmCropSelectionSystem_.GetSelectedCrop(), farmEconomySystem_, farmDateSystem_.GetDay());
			if (actionResult.Succeeded()) {
				farmDocumentSystem_.MarkDirty();
			}
			RouteFarmToolFeedback(actionResult);
		}
	}
#endif
	return result.navigationInputConsumed;
}

void GamePlaySessionSystem::RouteFarmToolFeedback(const FarmToolActionResult& result)
{
	if (result.status == FarmToolActionStatus::Harvested) {
		farmHarvestVisualSystem_.Start(result, farmGrid_, farmVisualSystem_);
		farmFeedbackSystem_.ShowHarvest(
			result.harvestQuality.crop, 1, result.harvestQuality.score,
			result.harvestQuality.salePrice);
		AddLog(
			"Harvest quality " + std::to_string(result.harvestQuality.score) +
			"/100, value " + std::to_string(result.harvestQuality.salePrice) + "G.");
	} else if (result.status == FarmToolActionStatus::NoSeed) {
		farmFeedbackSystem_.ShowNoSeed(farmCropSelectionSystem_.GetSelectedCrop());
	}
}

void GamePlaySessionSystem::RouteFarmSale(const FarmSaleResult& result)
{
	if (!result.Succeeded()) {
		farmFeedbackSystem_.ShowEmptySale(result.crop);
		return;
	}

	farmDocumentSystem_.MarkDirty();
	// A sale commits harvested items, so earlier tile-only history cannot safely cross it.
	farmToolActionSystem_.ClearHistory();
	const std::string cropName = farm::IsPlantableCrop(result.crop)
		? std::string(farm::ToString(result.crop)) + " "
		: std::string{};
	AddLog(
		"Sold " + cropName + std::to_string(result.soldCount) +
			" crop(s) for " + std::to_string(result.earnedMoney) + "G.");
	if (farm::IsPlantableCrop(result.crop)) {
		farmFeedbackSystem_.ShowSale(
			result.crop, result.soldCount, result.earnedMoney);
	} else {
		farmFeedbackSystem_.ShowSale(result.soldCount, result.earnedMoney);
	}
	if (farmProgressionSystem_.EvaluateClear(farmEconomySystem_.GetMoney())) {
		AddLog("Farm clear target reached.");
		farmFeedbackSystem_.RecordGoalReached();
	}
}

bool GamePlaySessionSystem::RetryCompletedSeason()
{
	if (!farmDocumentSystem_.ArchiveCompletedSeason(farmGrid_, farmEconomySystem_, farmCropSelectionSystem_))
		return false;
	if (!ResetFarmSessionState()) return false;
	farmDocumentSystem_.BeginUnsavedSession();
	return true;
}

bool GamePlaySessionSystem::ResetFarmSession()
{
	const auto season = FarmContestSeasonSystem::Evaluate(farmDateSystem_.GetDay(), farmEconomySystem_.GetContestResults());
	if (farmProgressionSystem_.IsContestSeason() && season.valid && season.finalized) return RetryCompletedSeason();
	return ResetFarmSessionState();
}

bool GamePlaySessionSystem::ResetFarmSessionState()
{
	const auto mode = farmProgressionSystem_.GetMode();
	const auto rules = FarmProgressionSystem::InitialRules(mode);
	if (!farmGrid_.Initialize(5, 4)) {
		AddLog("Farm restart failed: invalid grid dimensions.");
		return false;
	}
	farmContestDaySystem_.Reset();
	farmDateSystem_.Initialize();
	farmToolSystem_.Initialize();
	farmEconomySystem_.Initialize(rules);
	farmIrrigationPreviewSystem_.Initialize();
	farmIrrigationSystem_.Initialize();
	farmToolActionSystem_.Initialize();
	farmCropSelectionSystem_.Initialize();
	farmProgressionSystem_.Initialize(rules, mode);
	farmFeedbackSystem_.Initialize(false);
	farmHarvestVisualSystem_.Clear();
	farmIrrigationSystem_.Rebuild(farmGrid_);
	farmDocumentSystem_.MarkDirty();
	if (farmRestartCount_ < (std::numeric_limits<std::uint32_t>::max)()) {
		++farmRestartCount_;
	}
	farmFeedbackSystem_.ShowRestarted();
	AddLog("Farm session restarted.");
	return true;
}

bool GamePlaySessionSystem::TryBuildViewportRay(
	Vector3& outOrigin, Vector3& outDirection) const
{
	outOrigin = {};
	outDirection = {};
	if (!camera_ || !viewportHovered_ ||
		viewportImageSize_.x <= 0.0f || viewportImageSize_.y <= 0.0f) {
		return false;
	}

	const float u = (viewportMousePosition_.x - viewportImageTopLeft_.x) / viewportImageSize_.x;
	const float v = (viewportMousePosition_.y - viewportImageTopLeft_.y) / viewportImageSize_.y;
	if (!std::isfinite(u) || !std::isfinite(v) ||
		u < 0.0f || u > 1.0f || v < 0.0f || v > 1.0f) {
		return false;
	}

	const float ndcX = u * 2.0f - 1.0f;
	const float ndcY = 1.0f - v * 2.0f;
	const Matrix4x4 inverseViewProjection = MatrixMath::Inverse(camera_->GetViewProjectionMatrix());
	const Vector3 nearPoint = MatrixMath::Transform({ ndcX, ndcY, 0.0f }, inverseViewProjection);
	const Vector3 farPoint = MatrixMath::Transform({ ndcX, ndcY, 1.0f }, inverseViewProjection);
	const Vector3 direction = MatrixMath::Normalize(farPoint - nearPoint);
	if (MatrixMath::Length(direction) <= 0.0001f ||
		!std::isfinite(nearPoint.x) || !std::isfinite(nearPoint.y) || !std::isfinite(nearPoint.z) ||
		!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z)) {
		return false;
	}

	outOrigin = nearPoint;
	outDirection = direction;
	return true;
}

bool GamePlaySessionSystem::TrySelectFarmTileFromViewport()
{
	Vector3 rayOrigin{};
	Vector3 rayDirection{};
	if (!TryBuildViewportRay(rayOrigin, rayDirection)) {
		return false;
	}

	int tileIndex = -1;
	return farmVisualSystem_.TryPickTile(
		farmGrid_, rayOrigin, rayDirection, tileIndex) &&
		farmGrid_.SetSelectedIndex(tileIndex);
}

InitializationTask GamePlaySessionSystem::InitializeFarmHUD() {
	farmHudInitialized_ = co_await farmHud_.InitializeIncrementally(framework_->GetSpriteCommon());
	if (!farmHudInitialized_) {
		AddLog("FarmHUD initialization failed.");
		co_return false;
	}

	farmHud_.SetViewData(BuildFarmHUDViewData());
	co_return true;
}

void GamePlaySessionSystem::InitializeStageClearHUD() {
	stageClearHudInitialized_ = stageClearHud_.Initialize(framework_->GetSpriteCommon());
	if (!stageClearHudInitialized_) {
		AddLog("StageClearHUD initialization failed.");
		return;
	}
	stageClearHud_.SetVisible(
		levelGameplay_.IsStageCleared() || farmProgressionSystem_.IsCleared());
}

void GamePlaySessionSystem::InitializeSkyboxIfNeeded() {
	if (skybox_) {
		return;
	}

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(framework_->GetDxCommon(), kRostockSkyboxPath);
}

#ifdef USE_IMGUI
void GamePlaySessionSystem::DrawSceneDebugWindow() {
	if (ImGui::Begin("Scene Debug")) {
		ImGui::TextUnformatted("Rendering");
		ImGui::Checkbox("Skybox", &skyboxEnabled_);
		ImGui::Checkbox("Skybox Environment Map", &skyboxEnvironmentEnabled_);
		ImGui::Text("Skybox Loaded: %s", skybox_ ? "Yes" : "No");
		ImGui::Separator();
		ImGui::TextDisabled("Object visibility: Window > Scene Visibility");
		ImGui::Checkbox("Directional Shadows", &directionalShadowsEnabled_);
		ImGui::SliderFloat("Shadow Strength", &directionalShadowStrength_, 0.0f, 1.0f, "%.2f");
		ImGui::DragFloat3("Sun Direction", &lightDirection_.x, 0.01f, -1.0f, 1.0f, "%.2f");
		const char* specularItems[] = { "Phong", "Blinn-Phong" };
		ImGui::Combo("Specular Type", &specularTypeSelection_, specularItems, IM_ARRAYSIZE(specularItems));
		bool usePlayerCamera = usePlayerCamera_;
		if (ImGui::Checkbox("Use Player Camera (Third Person)", &usePlayerCamera)) {
			SetUsePlayerCamera(usePlayerCamera);
		}
		ImGui::TextUnformatted("F1: Toggle player/debug camera");
		ImGui::TextUnformatted("Hold R: Rewind  |  Shift+R: Forward Scrub");
		ImGui::TextUnformatted("Debug: hold right mouse + WASD, Q/E, wheel");
		if (ImGui::Button("Reload Level (F5 / Ctrl+R)")) {
			LoadSceneLevel();
		}
		ImGui::Text("Timeline: %zu / %zu snapshots  cursor=%zu",
			timeline_.GetSnapshotCount(), timeline_.GetCapacity(), timeline_.GetCursor());
		ImGui::BeginDisabled(!timeline_.CanStepBackward());
		if (ImGui::Button("Step Rewind")) {
			timelineStepBackwardRequested_ = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!timeline_.CanStepForward());
		if (ImGui::Button("Step Forward")) {
			timelineStepForwardRequested_ = true;
		}
		ImGui::EndDisabled();
		ImGui::Text("Level Object3d: %zu", levelObjects_.size());
		ImGui::Text("Collected: %zu / %zu", levelGameplay_.GetCollectedCount(), levelGameplay_.GetCollectibleCount());
		ImGui::Text("Patrol Points: %zu", levelRoutePoints_.size());
	}
	ImGui::End();
}
#endif

void GamePlaySessionSystem::UpdateSceneDeltaTime() {
	const FrameClock* frameClock = framework_ ? framework_->GetFrameClock() : nullptr;
	sceneDeltaTime_ = frameClock
		? frameClock->GetFrameDeltaSeconds()
		: FrameClock::kDefaultFixedDeltaSeconds;
	realDeltaTime_ = frameClock
		? frameClock->GetRealDeltaSeconds()
		: FrameClock::kDefaultFixedDeltaSeconds;
}

void GamePlaySessionSystem::HandleFarmHistoryInput() {
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (!input || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}
	const bool controlHeld = input->PushKey(DIK_LCONTROL) || input->PushKey(DIK_RCONTROL);
	if (!controlHeld) {
		return;
	}
	const bool shiftHeld = input->PushKey(DIK_LSHIFT) || input->PushKey(DIK_RSHIFT);
	if (farmProgressionSystem_.IsCleared()) {
		if (input->TriggerKey(InputKey::Z) || input->TriggerKey(InputKey::Y)) {
			farmFeedbackSystem_.ShowClearLocked();
		}
		return;
	}
	bool changed = false;
	if (input->TriggerKey(InputKey::Z) && shiftHeld) {
		changed = farmToolActionSystem_.Redo();
	} else if (input->TriggerKey(InputKey::Z)) {
		changed = farmToolActionSystem_.Undo();
	} else if (input->TriggerKey(InputKey::Y)) {
		changed = farmToolActionSystem_.Redo();
	}
	if (changed) {
		farmHarvestVisualSystem_.Clear();
		farmIrrigationPreviewSystem_.Cancel();
		farmIrrigationSystem_.Rebuild(farmGrid_);
		farmDocumentSystem_.MarkDirty();
		AddLog(input->TriggerKey(InputKey::Z) && !shiftHeld ? "Farm Undo" : "Farm Redo");
	}
}

void GamePlaySessionSystem::ResetCamera() {
	debugCameraPos_ = { 0.0f, 5.0f, -15.0f };
	debugCameraRot_ = { 0.3f, 0.0f, 0.0f };
	if (usePlayerCamera_ && levelGameplay_.HasPlayer()) {
		UpdatePlayerCamera();
	} else if (hasLevelCamera_) {
		cameraPos_ = levelCameraPos_;
		cameraRot_ = levelCameraRot_;
	} else {
		cameraPos_ = debugCameraPos_;
		cameraRot_ = debugCameraRot_;
	}
}

void GamePlaySessionSystem::SetUsePlayerCamera(bool usePlayerCamera) {
	if (usePlayerCamera == usePlayerCamera_) {
		return;
	}

	if (usePlayerCamera) {
		if (!levelGameplay_.HasPlayer()) {
			AddLog("Player camera is not available. PLAYER role is required.");
			return;
		}
		debugCameraPos_ = cameraPos_;
		debugCameraRot_ = cameraRot_;
		usePlayerCamera_ = true;
		UpdatePlayerCamera();
		AddLog("Camera mode: player third-person camera");
	} else {
		cameraPos_ = debugCameraPos_;
		cameraRot_ = debugCameraRot_;
		usePlayerCamera_ = false;
		AddLog("Camera mode: debug camera");
	}
	ClampCameraPitch();
}

void GamePlaySessionSystem::ToggleCameraMode() {
	SetUsePlayerCamera(!usePlayerCamera_);
}

void GamePlaySessionSystem::UpdatePlayerCamera() {
	if (!usePlayerCamera_ || !levelGameplay_.HasPlayer()) {
		return;
	}
	if (farmGameMode_) {
		farmRuntimeController_.UpdateFollowCamera(RuntimeContext());
		return;
	}

	const Vector3 target = levelGameplay_.GetPlayerPosition() + kPlayerCameraLookOffset;
	cameraPos_ = target + kPlayerCameraOffset;
	const Vector3 toTarget = target - cameraPos_;
	const float distance = MatrixMath::Length(toTarget);
	if (distance <= 0.0001f) {
		return;
	}

	const float normalizedY = std::clamp(toTarget.y / distance, -1.0f, 1.0f);
	cameraRot_.x = -std::asin(normalizedY);
	cameraRot_.y = std::atan2(toTarget.x, toTarget.z);
	cameraRot_.z = 0.0f;
}

void GamePlaySessionSystem::ClampCameraPitch() {
	cameraRot_.x = std::clamp(cameraRot_.x, -1.45f, 1.30f);
}

void GamePlaySessionSystem::HandleCameraInput(float deltaTime, bool suppressArrowKeys) {
	if (!cameraInputEnabled_ || usePlayerCamera_ || !framework_ || !viewportHovered_ || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return;
	}

	Input* input = framework_->GetInput();
	if (!input) {
		return;
	}
	(void)suppressArrowKeys;

	Vector3 right{};
	Vector3 forward{};
	BuildCameraGroundMoveAxes(cameraRot_.y, right, forward);

	Vector3 viewRight{};
	Vector3 viewUp{};
	Vector3 viewForward{};
	BuildCameraViewAxes(cameraRot_, viewRight, viewUp, viewForward);

	const bool isFastMove =
		input->PushKey(InputKey::LeftShift) ||
		input->PushKey(InputKey::RightShift);
	const float speedMultiplier = isFastMove ? kEditorCameraFastMultiplier : 1.0f;

	const bool isRightMouseDown = input->PushMouseButton(InputMouseButton::Right);
	const bool isMiddleMouseDown = input->PushMouseButton(InputMouseButton::Middle);
	const Vector2 mouseDelta = input->GetMouseDelta();

	if (isRightMouseDown) {
		cameraRot_.y += mouseDelta.x * kMouseLookSensitivity;
		cameraRot_.x += mouseDelta.y * kMouseLookSensitivity;

		Vector3 move = { 0.0f, 0.0f, 0.0f };
		if (input->PushKey(InputKey::W)) { move += forward; }
		if (input->PushKey(InputKey::S)) { move -= forward; }
		if (input->PushKey(InputKey::D)) { move += right; }
		if (input->PushKey(InputKey::A)) { move -= right; }
		if (input->PushKey(InputKey::E)) { move.y += 1.0f; }
		if (input->PushKey(InputKey::Q)) { move.y -= 1.0f; }

		const float moveLength = MatrixMath::Length(move);
		if (moveLength > 0.0001f) {
			const float moveStep = (std::max)(cameraMoveSpeed_, 0.0f) * speedMultiplier * deltaTime;
			cameraPos_ += (move / moveLength) * moveStep;
		}
	}

	if (isMiddleMouseDown) {
		const float panScale = kMousePanSensitivity * speedMultiplier;
		cameraPos_ += viewRight * (mouseDelta.x * panScale);
		cameraPos_ -= viewUp * (mouseDelta.y * panScale);
	}

	const float wheelDelta = input->GetMouseWheelDelta();
	if (std::abs(wheelDelta) > 0.0f) {
		const float wheelStep = kMouseWheelZoomSpeed * speedMultiplier * deltaTime * wheelDelta;
		cameraPos_ += viewForward * wheelStep;
	}
	ClampCameraPitch();
	debugCameraPos_ = cameraPos_;
	debugCameraRot_ = cameraRot_;
}

void GamePlaySessionSystem::HandleKeyboardMovement() {
	if (selectedTarget_ == 0 || viewportHovered_ || ImGuiManager::GetInstance()->WantsCaptureKeyboard()) return;
	Input* input = framework_->GetInput();
	float speed = input->PushKey(DIK_LSHIFT) ? 5.0f : 0.5f;
	Vector3 move = { 0, 0, 0 };
	if (input->PushKey(DIK_UP))    move.y += speed;
	if (input->PushKey(DIK_DOWN))  move.y -= speed;
	if (input->PushKey(DIK_LEFT))  move.x -= speed;
	if (input->PushKey(DIK_RIGHT)) move.x += speed;

	switch (selectedTarget_) {
	case 1: spritePos_.x += move.x; spritePos_.y -= move.y; break;
	case 2: objectPos_.x += move.x * 0.1f; objectPos_.y += move.y * 0.1f; break;
	case 4: spherePos_.x += move.x * 0.1f; spherePos_.y += move.y * 0.1f; break;
	}
}

/**
 * AddLog: デバッグログを記録する関数
 */
void GamePlaySessionSystem::HandleFieldMouseSelection() {
	fieldMouseInViewport_ = false;
	fieldMouseRayValid_ = false;
	fieldMouseHit_ = false;
	fieldMouseVirtualPosition_ = { -1.0f, -1.0f };
	fieldMouseSelectedIndex_ = fieldManager_ ? fieldManager_->GetSelectedIndex() : -1;

	if (!fieldSelectionEnabled_ ||
		!fieldManager_ ||
		!camera_ ||
		gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
		return;
	}

	Vector2 virtualPosition{};
	fieldMouseInViewport_ = ConvertMouseToVirtualScreen(viewportMousePosition_, virtualPosition);
	if (!fieldMouseInViewport_) {
		return;
	}
	fieldMouseVirtualPosition_ = virtualPosition;

	Ray ray{};
	fieldMouseRayValid_ = CreateRayFromVirtualScreen(virtualPosition, ray);
	fieldMouseRayOrigin_ = ray.origin;
	fieldMouseRayDirection_ = ray.direction;
	if (!fieldMouseRayValid_) {
		return;
	}

	Vector3 hitPosition{};
	fieldMouseHit_ = IntersectRayPlaneY(ray, fieldManager_->GetGroundY(), hitPosition);
	if (!fieldMouseHit_) {
		return;
	}
	fieldMouseHitPosition_ = hitPosition;

#ifdef USE_IMGUI
	if (viewportHovered_ && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		if (fieldManager_->TrySelectTileByWorldPosition(hitPosition)) {
			fieldMouseSelectedIndex_ = fieldManager_->GetSelectedIndex();
		}
	}
#endif
}

bool GamePlaySessionSystem::ConvertMouseToVirtualScreen(const Vector2& mouseScreenPos, Vector2& outVirtualPos) const {
	outVirtualPos = { -1.0f, -1.0f };
	if (viewportImageSize_.x <= 1.0f || viewportImageSize_.y <= 1.0f) {
		return false;
	}

	const float localX = mouseScreenPos.x - viewportImageTopLeft_.x;
	const float localY = mouseScreenPos.y - viewportImageTopLeft_.y;
	if (localX < 0.0f || localY < 0.0f ||
		localX > viewportImageSize_.x || localY > viewportImageSize_.y) {
		return false;
	}

	outVirtualPos.x = localX / viewportImageSize_.x * kVirtualScreenWidth;
	outVirtualPos.y = localY / viewportImageSize_.y * kVirtualScreenHeight;
	return std::isfinite(outVirtualPos.x) && std::isfinite(outVirtualPos.y);
}

bool GamePlaySessionSystem::CreateRayFromVirtualScreen(const Vector2& virtualScreenPos, Ray& outRay) const {
	outRay = {};
	if (!camera_ ||
		!std::isfinite(virtualScreenPos.x) ||
		!std::isfinite(virtualScreenPos.y)) {
		return false;
	}

	const float ndcX = (virtualScreenPos.x / kVirtualScreenWidth) * 2.0f - 1.0f;
	const float ndcY = 1.0f - (virtualScreenPos.y / kVirtualScreenHeight) * 2.0f;
	const Matrix4x4 inverseViewProjection = MatrixMath::Inverse(camera_->GetViewProjectionMatrix());
	const Vector3 nearPoint = MatrixMath::Transform({ ndcX, ndcY, 0.0f }, inverseViewProjection);
	const Vector3 farPoint = MatrixMath::Transform({ ndcX, ndcY, 1.0f }, inverseViewProjection);
	const Vector3 direction = MatrixMath::Normalize(farPoint - nearPoint);
	if (MatrixMath::Length(direction) <= 0.0001f ||
		!std::isfinite(nearPoint.x) ||
		!std::isfinite(nearPoint.y) ||
		!std::isfinite(nearPoint.z) ||
		!std::isfinite(direction.x) ||
		!std::isfinite(direction.y) ||
		!std::isfinite(direction.z)) {
		return false;
	}

	outRay.origin = nearPoint;
	outRay.direction = direction;
	return true;
}

bool GamePlaySessionSystem::IntersectRayPlaneY(const Ray& ray, float planeY, Vector3& outHitPosition) const {
	outHitPosition = {};
	if (std::abs(ray.direction.y) <= 0.0001f || !std::isfinite(planeY)) {
		return false;
	}

	const float t = (planeY - ray.origin.y) / ray.direction.y;
	if (t < 0.0f || !std::isfinite(t)) {
		return false;
	}

	outHitPosition = ray.origin + ray.direction * t;
	return
		std::isfinite(outHitPosition.x) &&
		std::isfinite(outHitPosition.y) &&
		std::isfinite(outHitPosition.z);
}

void GamePlaySessionSystem::SyncGPUParticleDebugModeChange() {
	if (gpuParticleDebugMode_ == previousGPUParticleDebugMode_) {
		return;
	}

	if (framework_ && framework_->GetParticleManager()) {
		framework_->GetParticleManager()->ResetGPUParticles();
	}
	if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
		interactionResetRequested_ = true;
		interactionBrushOperation_ = InteractionBrushOperation::None;
	}
	previousGPUParticleDebugMode_ = gpuParticleDebugMode_;
}

void GamePlaySessionSystem::SetGPUParticleDebugMode(GPUParticleDebugMode mode) {
	gpuParticleDebugMode_ = mode;
}

void GamePlaySessionSystem::HandleGPUParticleDebugModeInput() {
	switch (gpuParticleDebugMode_) {
	case GPUParticleDebugMode::Agriculture:
		HandleAgricultureParticleInput();
		break;
	case GPUParticleDebugMode::Interaction:
		HandleInteractionParticleInput();
		break;
	case GPUParticleDebugMode::Off:
	default:
		break;
	}
}

void GamePlaySessionSystem::HandleAgricultureParticleInput() {
	if (ImGuiManager::GetInstance()->WantsCaptureKeyboard()) {
		return;
	}

	Input* input = framework_->GetInput();
	if (input->TriggerKey(DIK_1)) {
		EmitAgricultureParticle(AgricultureParticleType::DirtDust);
	}
	if (input->TriggerKey(DIK_2)) {
		EmitAgricultureParticle(AgricultureParticleType::WaterSplash);
	}
	if (input->TriggerKey(DIK_3)) {
		EmitAgricultureParticle(AgricultureParticleType::HarvestSparkle);
	}
	if (input->TriggerKey(DIK_4)) {
		EmitAgricultureParticle(AgricultureParticleType::PollenSpore);
	}
	if (input->TriggerKey(DIK_5)) {
		EmitAgricultureParticle(AgricultureParticleType::BugSwarm);
	}
}

void GamePlaySessionSystem::HandleReleaseParticleInteractionInput(float deltaTime) {
	if (!framework_ || !framework_->GetInput() || !fieldManager_) {
		return;
	}

	Input* input = framework_->GetInput();

	auto setupInteractionAtSelectedField = [this]() {
		const Vector3 fieldPosition = fieldManager_->GetDemoFieldWorldPosition(cropBurstSelectedIndex_);
		interactionGridCenter_ = fieldPosition + Vector3{ 0.0f, 1.0f, 0.0f };
		interactionBrushPosition_ = interactionGridCenter_;
		interactionGridCount_ = std::clamp(interactionGridCount_, 5, 8);
		interactionParticleSize_ = std::clamp(interactionParticleSize_, 0.025f, 0.05f);
		interactionBrushRadius_ = (std::max)(interactionBrushRadius_, 1.8f);
		interactionBrushStrength_ = (std::max)(interactionBrushStrength_, 7.0f);
		interactionDamping_ = std::clamp(interactionDamping_, 0.90f, 0.985f);
		interactionParticleCount_ = CalculateInteractionParticleCount();
		SetGPUParticleDebugMode(GPUParticleDebugMode::Interaction);
		interactionResetRequested_ = true;
	};

	if (input->TriggerKey(DIK_K)) {
		if (gpuParticleDebugMode_ == GPUParticleDebugMode::Interaction) {
			SetGPUParticleDebugMode(GPUParticleDebugMode::Off);
			releaseInteractionOperation_ = InteractionBrushOperation::None;
			releaseInteractionTimer_ = 0.0f;
			AddLog("[ParticleInteraction] release mode off");
		} else {
			setupInteractionAtSelectedField();
			AddLog("[ParticleInteraction] release mode on");
		}
	}

	auto triggerInteraction = [&](InteractionBrushOperation operation, const char* logMessage) {
		setupInteractionAtSelectedField();
		releaseInteractionOperation_ = operation;
		releaseInteractionTimer_ = releaseInteractionDuration_;
		AddLog(logMessage);
	};

	if (input->TriggerKey(DIK_O)) {
		triggerInteraction(InteractionBrushOperation::Push, "[ParticleInteraction] push");
	}
	if (input->TriggerKey(DIK_P)) {
		triggerInteraction(InteractionBrushOperation::Pull, "[ParticleInteraction] pull");
	}

	if (releaseInteractionTimer_ > 0.0f) {
		releaseInteractionTimer_ = (std::max)(0.0f, releaseInteractionTimer_ - deltaTime);
	} else {
		releaseInteractionOperation_ = InteractionBrushOperation::None;
	}
}

void GamePlaySessionSystem::HandleInteractionParticleInput() {
	interactionGridCount_ = std::clamp(interactionGridCount_, 1, 10);
	interactionParticleSize_ = std::clamp(interactionParticleSize_, 0.02f, 0.05f);
	interactionBrushRadius_ = (std::max)(interactionBrushRadius_, 0.01f);
	interactionBrushStrength_ = (std::max)(interactionBrushStrength_, 0.0f);
	interactionDamping_ = std::clamp(interactionDamping_, 0.80f, 0.995f);
	interactionParticleCount_ = CalculateInteractionParticleCount();

	Vector3 brushPosition{};
	const bool hasBrushPosition = TryGetInteractionBrushPosition(brushPosition);
	if (hasBrushPosition) {
		interactionBrushPosition_ = brushPosition;
	}

	interactionBrushOperation_ = InteractionBrushOperation::None;
	Input* input = framework_ ? framework_->GetInput() : nullptr;
	if (input && hasBrushPosition && viewportHovered_ &&
		input->PushMouseButton(InputMouseButton::Left) &&
		!ImGuiManager::GetInstance()->WantsCaptureMouse()) {
		const bool isShiftPressed = input->PushKey(InputKey::LeftShift) || input->PushKey(InputKey::RightShift);
		interactionBrushOperation_ = isShiftPressed
			? InteractionBrushOperation::Pull
			: InteractionBrushOperation::Push;
	}
}

bool GamePlaySessionSystem::UpdateCropBurstDebugInput() {
	if (!framework_ || !framework_->GetInput() || !fieldManager_) {
		return false;
	}

	Input* input = framework_->GetInput();
	bool handledCropBurstPlay = false;
	auto selectCropBurstField = [this](int demoIndex) {
		cropBurstSelectedIndex_ = std::clamp(demoIndex, 0, 2);
		cropBurstEffectPosition_ = fieldManager_->GetDemoFieldWorldPosition(cropBurstSelectedIndex_);
		fieldManager_->TrySelectTileByWorldPosition(cropBurstEffectPosition_);
		fieldMouseSelectedIndex_ = fieldManager_->GetSelectedIndex();

		std::ostringstream log;
		log << "[CropBurst] select field index=" << cropBurstSelectedIndex_;
		AddLog(log.str());
	};

	if (input->TriggerKey(DIK_1)) {
		selectCropBurstField(0);
	}
	if (input->TriggerKey(DIK_2)) {
		selectCropBurstField(1);
	}
	if (input->TriggerKey(DIK_3)) {
		selectCropBurstField(2);
	}

	if (input->TriggerKey(DIK_SPACE)) {
		AddLog("[CropBurst] Space pressed");
		Vector3 playPosition = cropBurstEffectPosition_;
		playPosition.y += 0.5f;
		handledCropBurstPlay = true;

		if (!particleManager_) {
			AddLog("[CropBurst] ParticleManager is null");
			return handledCropBurstPlay;
		}

		particleManager_->PlayCropBurst(playPosition, CropBurstLevel::Rare);

		std::ostringstream log;
		log << "[CropBurst] play crop burst pos=("
			<< playPosition.x << ", "
			<< playPosition.y << ", "
			<< playPosition.z << ")";
		AddLog(log.str());
	}

#ifdef USE_IMGUI
	if (viewportHovered_ &&
		fieldMouseHit_ &&
		particleManager_ &&
		ImGui::GetIO().KeyShift &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		Vector3 playPosition = fieldMouseHitPosition_;
		playPosition.y += 0.5f;
		cropBurstEffectPosition_ = fieldMouseHitPosition_;
		fieldManager_->TrySelectTileByWorldPosition(cropBurstEffectPosition_);
		fieldMouseSelectedIndex_ = fieldManager_->GetSelectedIndex();
		particleManager_->PlayCropBurst(
			playPosition,
			CropBurstLevel::Strong);
	}
#endif

	return handledCropBurstPlay;
}

void GamePlaySessionSystem::UpdateCropBurstAutoPlayback(float deltaTime) {
	if (!fieldManager_ || !particleManager_ || farmProgressionSystem_.IsCleared()) {
		return;
	}

	cropBurstEffectPosition_ = fieldManager_->GetDemoFieldWorldPosition(cropBurstSelectedIndex_);

	if (!cropBurstAutoPlayed_) {
		cropBurstAutoTimer_ += deltaTime;
		if (cropBurstAutoTimer_ >= 0.25f) {
			Vector3 playPosition = cropBurstEffectPosition_;
			playPosition.y += 0.8f;
			particleManager_->PlayCropBurst(playPosition, CropBurstLevel::Rare);
			cropBurstAutoPlayed_ = true;
			cropBurstLoopTimer_ = 0.0f;
			AddLog("[CropBurst] auto play on scene start");
		}
		return;
	}

	if (!cropBurstAutoLoop_) {
		return;
	}

	cropBurstLoopTimer_ += deltaTime;
	if (cropBurstLoopTimer_ >= 1.5f) {
		cropBurstLoopTimer_ = 0.0f;
		Vector3 playPosition = cropBurstEffectPosition_;
		playPosition.y += 0.8f;
		particleManager_->PlayCropBurst(playPosition, CropBurstLevel::Rare);
		AddLog("[CropBurst] auto loop play");
	}
}

void GamePlaySessionSystem::EmitAgricultureParticle(AgricultureParticleType type) {
	if (gpuParticleDebugMode_ != GPUParticleDebugMode::Agriculture) {
		return;
	}

	const uint32_t count = static_cast<uint32_t>(std::clamp(agricultureParticleCount_, 1, 1024));
	const float particleSize = (std::max)(agricultureParticleSize_, 0.01f);
	GPUParticleEmitSettings settings = MakeAgricultureEmitSettings(
		agricultureEmitPosition_,
		particleSize,
		count,
		type);
	framework_->GetParticleManager()->RequestGPUParticleEmit(settings);
}

uint32_t GamePlaySessionSystem::CalculateInteractionParticleCount() const {
	const uint32_t gridCount = static_cast<uint32_t>(std::clamp(interactionGridCount_, 1, 10));
	const uint64_t particleCount =
		static_cast<uint64_t>(gridCount) *
		static_cast<uint64_t>(gridCount) *
		static_cast<uint64_t>(gridCount);
	return static_cast<uint32_t>(particleCount > 1024 ? 1024 : particleCount);
}

bool GamePlaySessionSystem::TryGetInteractionBrushPosition(Vector3& outBrushPosition) const {
	Vector3 nearPoint{};
	Vector3 rayDirection{};
	if (!TryBuildViewportRay(nearPoint, rayDirection)) {
		return false;
	}
	if (std::abs(rayDirection.y) <= 0.0001f) {
		return false;
	}

	const float t = (interactionGridCenter_.y - nearPoint.y) / rayDirection.y;
	if (t < 0.0f || !std::isfinite(t)) {
		return false;
	}

	outBrushPosition = nearPoint + rayDirection * t;
	return
		std::isfinite(outBrushPosition.x) &&
		std::isfinite(outBrushPosition.y) &&
		std::isfinite(outBrushPosition.z);
}

void GamePlaySessionSystem::AddLog(const std::string& message) {
	// 1. 外部ロガーツールを使用して Visual Studio の出力ウィンドウへ出す
	Logger::Log(message);

	// 2. ゲーム内 UI 用のリストに追加
	debugLogs_.push_back(message);

	// 3. 履歴の制限
	if (debugLogs_.size() > 50) {
		debugLogs_.erase(debugLogs_.begin());
	}
}

void GamePlaySessionSystem::Finalize()
{
    farmRuntimeController_.FinalizePresentationAudio();
	gamePlayEditorBridge_.Unbind();
	if (framework_ && framework_->GetParticleManager()) {
		// The shared manager must not retain this scene-owned camera after destruction.
		framework_->GetParticleManager()->SetCamera(nullptr);
	}
	if (framework_ && framework_->GetAudio()) {
		framework_->GetAudio()->SetGlobalTemporalState(
			AudioPlaybackDirection::Forward,
			1.0f,
			0.0f);
	}
}

/**
 * ChangeAnimationModel: インデックスに応じて再生するアニメーションモデルを動的に切り替える
 * 各モデル固有のサイズ（スケール）や初期位置の自動調整もここで行います
 */
void GamePlaySessionSystem::ChangeAnimationModel(int index) {
	std::vector<std::pair<std::string, std::string>> animModels = {
		{ "AnimatedCube", "AnimatedCube.gltf" },
		{ "simpleSkin", "simpleSkin.gltf" },
		{ "human", "walk.gltf" },
		{ "human", "sneakWalk.gltf" }
	};

	if (index < 0 || index >= static_cast<int>(animModels.size())) return;

	std::string path = animModels[index].first + "/" + animModels[index].second;
	Model* animModel = framework_->GetModelManager()->GetModel(path);
	if (animModel) {
		if (!animObj_) {
			animObj_ = std::make_unique<Object3d>();
			animObj_->Initialize(framework_->GetObject3dCommon());
		}
		// オブジェクトにモデルをセット
		animObj_->SetModel(animModel);
		
		// スケールと座標の調整 (モデルごとに適した表示サイズと位置へ自動的にアジャスト)
		if (index == 0) { // AnimatedCube (仮キューブモデル)
			animObj_->SetPosition({ 5.0f, 0.0f, 0.0f });
			animObj_->SetScale({ 1.0f, 1.0f, 1.0f });
		} else if (index == 1) { // simpleSkin (シンプルな関節スキン)
			animObj_->SetPosition({ 0.0f, 0.0f, 0.0f });
			animObj_->SetScale({ 0.5f, 0.5f, 0.5f }); // 画面に収まるように少し小さめにする
		} else { // human (人間モデル)
			animObj_->SetPosition({ 0.0f, -2.0f, 0.0f }); // 地面(Y=-2.0)にピッタリ乗るように設定
			animObj_->SetScale({ 1.0f, 1.0f, 1.0f });
		}
		
		// 環境マップ設定の引き継ぎ
		if (skybox_ && skyboxEnvironmentEnabled_) {
			animObj_->SetEnvironmentMap(skybox_->GetTextureHandle());
		}
		
		// アニメーションデータをアセットフォルダから読み込んでオブジェクトにセット
		Animation anim = animModel->LoadAnimation("Resources/" + animModels[index].first, animModels[index].second);
		animObj_->SetAnimation(anim);
		
		// 再生制御パラメータの初期化
		animObj_->SetAnimationTime(0.0f);
		animObj_->SetAnimationPlaying(true);
	}
}

FarmRuntimeContext GamePlaySessionSystem::RuntimeContext() {
    return {
        .host = *this,
        .window = framework_ ? framework_->GetWinApp() : nullptr,
        .farmArrivalRenderer = farmArrivalRenderer_,
        .farmContestDaySystem = farmContestDaySystem_,
        .farmCropSelectionSystem = farmCropSelectionSystem_,
        .farmDateSystem = farmDateSystem_,
        .farmDocumentSystem = farmDocumentSystem_,
        .farmEconomySystem = farmEconomySystem_,
        .farmFeedbackSystem = farmFeedbackSystem_,
        .farmGrid = farmGrid_,
        .farmGrowthComparisonSystem = farmGrowthComparisonSystem_,
        .farmGrowthSystem = farmGrowthSystem_,
        .farmInputSystem = farmInputSystem_,
        .farmIrrigationPreviewSystem = farmIrrigationPreviewSystem_,
        .farmIrrigationSystem = farmIrrigationSystem_,
        .farmProgressionSystem = farmProgressionSystem_,
        .farmSeedShopRenderer = farmSeedShopRenderer_,
        .farmStageAccessSystem = farmStageAccessSystem_,
        .farmToolActionSystem = farmToolActionSystem_,
        .farmToolSystem = farmToolSystem_,
        .farmVisualSystem = farmVisualSystem_,
        .gamePlayEditorBridge = gamePlayEditorBridge_,
        .levelGameplay = levelGameplay_,
    };
}
void GamePlaySessionSystem::ApplyCameraPose(const farm::OverviewPose& pose, bool updateOverview) {
    const auto finite = [](const Vector3& v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); };
    if (!finite(pose.position) || !finite(pose.rotation)) return;
    cameraPos_ = pose.position;
    cameraRot_ = pose.rotation;
    if (updateOverview) { debugCameraPos_ = pose.position; debugCameraRot_ = pose.rotation; }
}
ScreenTransitionResult GamePlaySessionSystem::BeginScreenTransition(scene::RipplePoint origin,
    scene::TransitionSoundTheme sound, scene::TransitionStyle style) {
    return sceneManager_ ? sceneManager_->BeginScreenTransition(&owner_, origin, sound, style)
        : ScreenTransitionResult::Unavailable;
}
