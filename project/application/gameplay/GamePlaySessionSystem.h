#pragma once

#include "application/level/LevelGameplaySystem.h"
#include "editor/GamePlayEditorBridge.h"
#include "scene/BaseScene.h"
#include "farm/system/FarmRuntimeContext.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "math/Matrix.h"
#include "3d/Skybox.h"
#include "2d/TextureManager.h"
#include "3d/SkeletonDebugger.h"
#include "effect/ParticleManager.h"
#include "time/SnapshotTimeline.h"
#include "farm/system/FarmContestDaySystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDateSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmFeedbackSystem.h"
#include "farm/system/FarmHarvestVisualSystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/system/FarmInputSystem.h"
#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/render/FarmRenderer.h"
#include "farm/render/FarmArrivalRenderer.h"
#include "farm/render/FarmSeedShopRenderer.h"
#include "farm/render/FarmContestRenderer.h"
#include "farm/ui/FarmHUD.h"
#include "level/ui/StageClearHUD.h"

class Framework;
class Sprite;
class Object3d;
class Camera;
class Model;
class FieldManager;

namespace level {
struct LevelData;
}

// Owns one gameplay session. Scene lifetime callbacks only schedule this system.
class GamePlaySessionSystem final : public editor::GamePlayEditorTarget, public FarmRuntimeHost {
public:
	enum class GPUParticleDebugMode {
		Off,
		Agriculture,
		Interaction,
	};

	enum class AgricultureParticleType {
		DirtDust,
		WaterSplash,
		HarvestSparkle,
		PollenSpore,
		BugSwarm,
	};

	GamePlaySessionSystem(BaseScene& owner, SceneManager* manager);
	~GamePlaySessionSystem();

	void Initialize();
    InitializationTask InitializeIncrementally();
	void Finalize();
	void PrepareFixedUpdate();
	void OnScreenTransitionCovered();
	void FixedUpdate(float fixedDeltaTime);
	void Update();
	void Draw();
	[[nodiscard]] editor::GamePlayEditorBridge& GetEditorBridge() noexcept { return gamePlayEditorBridge_; }
	[[nodiscard]] const editor::GamePlayEditorBridge& GetEditorBridge() const noexcept { return gamePlayEditorBridge_; }
	[[nodiscard]] const Camera* GetCamera() const noexcept { return camera_.get(); }
	bool SetFarmGameMode(bool enabled);
    void RequestFarmLayoutLibrary() noexcept { layoutLibraryRequested_ = true; }
	[[nodiscard]] bool IsFarmGameMode() const noexcept { return farmGameMode_; }
	// Cylinderメッシュの再生成（パラメータ変更時に呼ぶ）
	// アニメーションモデルの動的切り替え（ImGuiからの呼び出し用）

    void BindSceneManager(SceneManager* manager) noexcept { sceneManager_ = manager; }
    farm::OverviewPose ReadCameraPose() const override { return {cameraPos_, cameraRot_}; }
    bool IsFollowingPlayer() const override { return usePlayerCamera_; }
    bool IsTimelineScrubbing() const override { return timelineScrubbing_; }
    bool IsViewportFocused() const override { return viewportFocused_; }
    bool IsViewportHovered() const override { return viewportHovered_; }
    const Camera* ReadCamera() const override { return camera_.get(); }
    void ApplyCameraPose(const farm::OverviewPose& pose, bool updateOverview) override;
    ScreenTransitionResult BeginScreenTransition(scene::RipplePoint origin,
        scene::TransitionSoundTheme sound, scene::TransitionStyle style) override;

private:
    FarmRuntimeContext RuntimeContext();
	enum class PlayerAnimationMode;
	struct GameplaySnapshot;
	void AddLog(const std::string& message);
	void UpdateSceneDeltaTime();
	void PrepareRenderFrame(float presentationDeltaTime);
	void HandleKeyboardMovement();
	void HandleCameraInput(float deltaTime, bool suppressArrowKeys);
	void ClampCameraPitch();
	void ResetCamera();
	void CreateSphere(float radius);
	void RebuildCylinder();
	void ChangeAnimationModel(int index);
	void SetUsePlayerCamera(bool usePlayerCamera) override;
	void ToggleCameraMode();
	void UpdatePlayerCamera();
	void SyncGPUParticleDebugModeChange();
	void HandleGPUParticleDebugModeInput();
	void HandleAgricultureParticleInput();
	void HandleInteractionParticleInput();
	void HandleReleaseParticleInteractionInput(float deltaTime);
	bool UpdateCropBurstDebugInput();
	void UpdateCropBurstAutoPlayback(float deltaTime);
	bool TryGetInteractionBrushPosition(Vector3& outBrushPosition) const;
	FarmHUDViewData BuildFarmHUDViewData() const override;
	void HandleFarmDateDebugInput();
	void HandleFarmHistoryInput();
	bool HandleFarmInput();
	void RouteFarmToolFeedback(const FarmToolActionResult& result) override;
	void RouteFarmSale(const FarmSaleResult& result) override;
	bool ResetFarmSession() override;
	bool RetryCompletedSeason() override;
	bool ResetFarmSessionState();
	bool MovePlayerToFarmTile(int tileIndex);
	level::LevelGameplaySystem::GroundHeightQuery BuildPlayerGroundQuery() const;
	bool TryBuildViewportRay(Vector3& outOrigin, Vector3& outDirection) const override;
	bool TrySelectFarmTileFromViewport();
	InitializationTask InitializeFarmHUD();
	void InitializeStageClearHUD();
	void InitializeSkyboxIfNeeded();
	void LoadSceneLevel();
	void CreateLevelObjectsFromLevel();
	void SyncLevelGameplayPresentation();
	void InitializeTimeline();
	void CaptureTimelineSnapshot(GameplaySnapshot& output) const;
	bool RestoreTimelineSnapshot(const GameplaySnapshot& snapshot);
	void UpdateLevelPlayerVisual();
	void ConfigureLevelPlayerAnimation(std::size_t playerRenderIndex, PlayerAnimationMode mode);
	void CollectLevelRuntimeData();
	void ApplyLevelCamera();
	void DrawLevelDebugGizmos() const;
	void DrawLevelCollisionGizmos() const;
	void DrawLevelBox(const Vector3& center, const Vector3& size, const Vector4& color) const;
	void DrawLevelCameraGizmo(const Vector3& position, const Vector4& color) const;
	Vector3 EvaluateLevelRoutePoint(float normalizedTime) const;
	void SetGPUParticleDebugMode(GPUParticleDebugMode mode);
	void HandleFieldMouseSelection();
	bool ConvertMouseToVirtualScreen(const Vector2& mouseScreenPos, Vector2& outVirtualPos) const override;
	struct Ray {
		Vector3 origin = { 0.0f, 0.0f, 0.0f };
		Vector3 direction = { 0.0f, 0.0f, 1.0f };
	};
	bool CreateRayFromVirtualScreen(const Vector2& virtualScreenPos, Ray& outRay) const;
	bool IntersectRayPlaneY(const Ray& ray, float planeY, Vector3& outHitPosition) const;
	void EmitAgricultureParticle(AgricultureParticleType type);
	uint32_t CalculateInteractionParticleCount() const;
#ifdef USE_IMGUI
	void DrawSceneDebugWindow();
#endif

	// エフェクト発生関数
	void EmitSpark(const Vector3& position);
	void EmitRingEffect(const Vector3& position);
	void EmitCylinderEffect(const Vector3& position);

private:
    void BuildViewModel(editor::GamePlayEditorViewModel& output) const override;
    bool Execute(const editor::GamePlayEditorCommand& command) override;
    bool Execute(const editor::FarmDocumentCommand& command) override;
    bool Execute(const editor::VisibilityEditorCommand& command) override;
    bool Execute(const editor::CameraEditorCommand& command) override;
    bool Execute(const editor::ObjectInspectorEditorCommand& command) override;
    bool Execute(const editor::ParticleEditorCommand& command) override;
    bool Execute(const editor::SimulationEditorCommand& command) override;
    void SetViewportState(const Vector2& imageTopLeft, const Vector2& imageSize,
        const Vector2& mousePosition, bool hovered, bool focused) noexcept override;
    Object3d* GetAnimationObjectForDebug() const noexcept override;
    bool SelectCommandTarget(const editor::GamePlayEditorCommand& command);
    BaseScene& owner_;
    SceneManager* sceneManager_ = nullptr; // Borrowed; manager outlives the session.

	struct LevelObjectRuntime {
		std::unique_ptr<Object3d> object;
		std::string name;
		std::string gameplayRole;
		Vector3 basePosition{};
		float animationPhase = 0.0f;
		bool visible = true;
	};
	struct GameplaySnapshot {
		level::LevelGameplaySystem::Snapshot levelGameplay;
		farm::FarmGrid::Snapshot farmGrid;
		FarmDateSystem::Snapshot farmDate;
		FarmEconomySystem::Snapshot farmEconomy;
		FarmCropSelectionSystem::Snapshot farmCropSelection;
		FarmProgressionSystem::Snapshot farmProgression;
		FarmTool farmTool = FarmTool::Hoe;
		float levelRouteTimer = 0.0f;
		float playerAnimationTime = 0.0f;
		float playerAnimationSpeed = 0.0f;
	};

	enum class PlayerAnimationMode {
		Walk,
		Sneak,
	};
	struct PlayerAnimationClip {
		const char* fileName = nullptr;
		float playbackSpeed = 1.0f;
	};
	enum class PlayerAnimationState {
		Idle,
		Walk,
		Sneak,
		Airborne,
	};
	static const PlayerAnimationClip& GetPlayerAnimationClip(PlayerAnimationMode mode);

	// Framework and render objects are Scene-lifetime dependencies.
	Framework* framework_ = nullptr;
	ParticleManager* particleManager_ = nullptr;
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Camera> camera_;

	Vector3 cameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 cameraRot_ = { 0.3f, 0.0f, 0.0f };
	Vector3 debugCameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 debugCameraRot_ = { 0.3f, 0.0f, 0.0f };
	Vector3 levelCameraPos_ = { 0.0f, 5.0f, -15.0f };
	Vector3 levelCameraRot_ = { 0.3f, 0.0f, 0.0f };
	float cameraMoveSpeed_ = 5.0f;
	float cameraRotateSpeed_ = 1.5f;
	float sceneDeltaTime_ = 1.0f / 60.0f; // Pausable simulation time.
	float realDeltaTime_ = 1.0f / 60.0f;  // Unscaled editor-camera time.

	std::unique_ptr<Model> sphereModel_;
	std::unique_ptr<Object3d> sphereObj_;
	std::unique_ptr<Model> terrainModel_;
	std::unique_ptr<Object3d> terrainObj_;

	std::unique_ptr<Object3d> animObj_;
	std::unique_ptr<FieldManager> fieldManager_;
	std::unique_ptr<level::LevelData> levelData_;
	std::vector<LevelObjectRuntime> levelObjects_;
	level::LevelGameplaySystem levelGameplay_;
	level::LevelGameplaySystem::PlayerCommand pendingPlayerCommand_{};
	SnapshotTimeline<GameplaySnapshot> timeline_;
	GameplaySnapshot timelineScratch_{};
	bool timelineRewindHeld_ = false;
	bool timelineForwardHeld_ = false;
	bool timelineScrubbing_ = false;
	bool timelineStepBackwardRequested_ = false;
	bool timelineStepForwardRequested_ = false;
	Texture2DHandle levelWhiteTextureHandle_{};
	PlayerAnimationMode playerAnimationMode_ = PlayerAnimationMode::Walk;
	PlayerAnimationState playerAnimationState_ = PlayerAnimationState::Idle;
	float playerAnimationSpeed_ = 0.0f;
	bool playerVisualConfigured_ = false;
	bool directionalShadowsEnabled_ = true;
	float directionalShadowStrength_ = 0.82f;
	std::vector<Vector3> levelRoutePoints_;
	float levelRouteTimer_ = 0.0f;

	// エフェクト用モデル
	std::unique_ptr<Model> ringModel_;
	std::unique_ptr<Model> cylinderModel_;

	std::unique_ptr<Skybox> skybox_;
	bool skyboxEnabled_ = false;
	bool skyboxEnvironmentEnabled_ = false;
	int skyboxSelection_ = 0;
	std::vector<Texture2DHandle> textureHandles_;
	Texture2DHandle ringTexHandle_{};

	std::unique_ptr<SkeletonDebugger> skeletonDebugger_;
	// Farm Systems own mutation; the Scene only schedules them.
	farm::FarmGrid farmGrid_;
	FarmDateSystem farmDateSystem_;
	FarmProgressionSystem farmProgressionSystem_;
	FarmStageAccessSystem farmStageAccessSystem_;
	FarmContestDaySystem farmContestDaySystem_;
	FarmDocumentSystem farmDocumentSystem_;
    bool layoutLibraryRequested_ = false;
    bool layoutLibraryFrame_ = false;
	FarmEconomySystem farmEconomySystem_;
	FarmCropSelectionSystem farmCropSelectionSystem_;
	FarmFeedbackSystem farmFeedbackSystem_;
	farm::FarmHarvestVisualSystem farmHarvestVisualSystem_;
	FarmGrowthSystem farmGrowthSystem_;
	FarmGrowthComparisonSystem farmGrowthComparisonSystem_;
	FarmInputSystem farmInputSystem_;
	farm::FarmIrrigationPreviewSystem farmIrrigationPreviewSystem_;
	farm::FarmIrrigationSystem farmIrrigationSystem_;
	FarmToolSystem farmToolSystem_;
	FarmToolActionSystem farmToolActionSystem_;
	farm::FarmVisualSystem farmVisualSystem_;
	farm::FarmRenderer farmRenderer_;
	FarmSeedShopRenderer farmSeedShopRenderer_;
	FarmContestRenderer farmContestRenderer_;
	editor::GamePlayEditorBridge gamePlayEditorBridge_;
	FarmHUD farmHud_;
	FarmRuntimeController farmRuntimeController_;
    FarmArrivalRenderer farmArrivalRenderer_;
	bool farmHudInitialized_ = false;
	StageClearHUD stageClearHud_;
	bool stageClearHudInitialized_ = false;
	std::uint32_t farmRestartCount_ = 0;

	bool showTerrain_ = false;
#ifdef USE_IMGUI
	bool farmGameMode_ = false;
#else
	bool farmGameMode_ = true;
#endif
	bool developmentPlayerCamera_ = true;
	bool showSphere_ = false;
	bool showPlane_ = false;
	bool showSprite_ = false;
	bool showParticles_ = true;
	bool showAnimModel_ = true;
	bool showLevelObjects_ = true;
	bool showLevelGizmos_ = false;
	bool showLevelCollisionGizmos_ = false;
	bool usePlayerCamera_ = true;
	bool hasLevelCamera_ = false;
	bool showSkeleton_ = false;
	bool showDebugGrid_ = false;

	Vector2 spritePos_ = { 640.0f, 360.0f };
	Vector3 objectPos_ = { 0.0f, 0.0f, 0.0f };
	Vector3 objectRot_ = { 0.0f, 0.0f, 0.0f };
	Vector3 spherePos_ = { 0.0f, 0.0f, 10.0f };
	float sphereRadius_ = 1.0f;

	int selectedTarget_ = 4;
	int activeParticleType_ = 0; // これを Game.cpp と同期
	int currentAnimModelIdx_ = 1; // 現在のアニメーションモデルのインデックス（0: AnimatedCube, 1: simpleSkin, 2: human/walk, 3: human/sneakWalk）
	int cullMode_ = 2;
	int specularTypeSelection_ = 1;
	int modelPriority_ = 0;

	GPUParticleDebugMode gpuParticleDebugMode_ = GPUParticleDebugMode::Off;
	GPUParticleDebugMode previousGPUParticleDebugMode_ = GPUParticleDebugMode::Off;
	Vector3 agricultureEmitPosition_ = { 0.0f, 1.5f, 8.0f };
	float agricultureParticleSize_ = 0.25f;
	int agricultureParticleCount_ = 128;
	bool agricultureShowKeyGuide_ = true;
	int interactionGridCount_ = 8;
	float interactionParticleSize_ = 0.03f;
	float interactionBrushRadius_ = 1.5f;
	float interactionBrushStrength_ = 1.0f;
	float interactionDamping_ = 0.95f;
	Vector3 interactionGridCenter_ = { 0.0f, 0.0f, 8.0f };
	Vector3 interactionBrushPosition_ = { 0.0f, 0.0f, 8.0f };
	uint32_t interactionParticleCount_ = 512;
	bool interactionResetRequested_ = true;
	InteractionBrushOperation interactionBrushOperation_ = InteractionBrushOperation::None;
	InteractionBrushOperation releaseInteractionOperation_ = InteractionBrushOperation::None;
	float releaseInteractionTimer_ = 0.0f;
	float releaseInteractionDuration_ = 0.35f;
	Vector2 viewportImageTopLeft_ = { 0.0f, 0.0f };
	Vector2 viewportImageSize_ = { 0.0f, 0.0f };
	Vector2 viewportMousePosition_ = { 0.0f, 0.0f };
	bool viewportHovered_ = false;
	bool viewportFocused_ = false;
	bool fieldSelectionEnabled_ = true;
	bool skyboxInputEnabled_ = true;
	bool fieldInputEnabled_ = true;
	bool cameraInputEnabled_ = true;
	bool fieldMouseInViewport_ = false;
	bool fieldMouseRayValid_ = false;
	bool fieldMouseHit_ = false;
	Vector2 fieldMouseVirtualPosition_ = { -1.0f, -1.0f };
	Vector3 fieldMouseRayOrigin_ = { 0.0f, 0.0f, 0.0f };
	Vector3 fieldMouseRayDirection_ = { 0.0f, 0.0f, 1.0f };
	Vector3 fieldMouseHitPosition_ = { 0.0f, 0.0f, 0.0f };
	int fieldMouseSelectedIndex_ = -1;
	int cropBurstSelectedIndex_ = 0;
	Vector3 cropBurstEffectPosition_ = { 0.0f, 1.0f, 8.0f };
	float cropBurstAutoTimer_ = 0.0f;
	float cropBurstLoopTimer_ = 0.0f;
	bool cropBurstAutoPlayed_ = false;
	bool cropBurstAutoLoop_ = false;

	// Cylinderパラメータ（ImGuiで操作可能）
	float cylTopRadius_ = 1.0f;       // 上面の半径
	float cylBottomRadius_ = 1.0f;    // 下面の半径
	float cylHeight_ = 2.0f;          // 高さ
	int cylSegments_ = 32;            // 円周方向の分割数
	int cylVertDivisions_ = 4;        // 高さ方向の分割数

	// ライト設定 (GamePlayScene.cpp で使用しているもの)
	Vector3 lightDirection_ = { 0.0f, -1.0f, 1.0f };
	Vector3 lightColor_ = { 1.0f, 1.0f, 1.0f };
	float lightIntensity_ = 1.0f;
	Vector3 spotLightColor_ = { 1.0f, 1.0f, 1.0f };
	Vector3 spotLightPos_ = { 0.0f, 4.0f, 10.0f };
	float spotLightIntensity_ = 0.0f;
	Vector3 spotLightDir_ = { 0.0f, -1.0f, 0.0f };
	float spotLightDistance_ = 15.0f;
	float spotLightDecay_ = 1.0f;
	float spotLightAngle_ = 0.5f;
	float spotLightFalloff_ = 0.1f;

	std::vector<std::string> debugLogs_;
};
