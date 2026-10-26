#pragma once
#include "farm/ui/FarmHUD.h"
#include "farm/system/FarmOverviewCamera.h"
#include "scene/SceneTransitionSystem.h"
#include "scene/TransitionSound.h"
class Camera;
class WinApp;
class FarmArrivalRenderer;
class FarmSeedShopRenderer;
class FarmContestDaySystem;
class FarmCropSelectionSystem;
class FarmDateSystem;
class FarmDocumentSystem;
class FarmEconomySystem;
class FarmFeedbackSystem;
class FarmGrowthComparisonSystem;
class FarmGrowthSystem;
class FarmInputSystem;
class FarmProgressionSystem;
class FarmStageAccessSystem;
class FarmToolActionSystem;
class FarmToolSystem;
struct FarmSaleResult;
struct FarmToolActionResult;
namespace farm { class FarmGrid; class FarmIrrigationSystem; class FarmIrrigationPreviewSystem; class FarmVisualSystem; }
namespace editor { class GamePlayEditorBridge; }
namespace level { class LevelGameplaySystem; }
enum class ScreenTransitionResult;

// Only named commands can change host state; query results are values or read-only borrows.
class FarmRuntimeHost {
public:
    virtual ~FarmRuntimeHost() = default;
    virtual FarmHUDViewData BuildFarmHUDViewData() const = 0;
    virtual farm::OverviewPose ReadCameraPose() const = 0;
    virtual const Camera* ReadCamera() const = 0;
    virtual bool IsFollowingPlayer() const = 0;
    virtual bool IsTimelineScrubbing() const = 0;
    virtual bool IsViewportFocused() const = 0;
    virtual bool IsViewportHovered() const = 0;
    virtual void ApplyCameraPose(const farm::OverviewPose& pose, bool updateOverview) = 0;
    virtual void SetUsePlayerCamera(bool enabled) = 0;
    virtual void InitializeTimeline() = 0;
    virtual bool ResetFarmSession() = 0;
    virtual bool RetryCompletedSeason() = 0;
    virtual void RouteFarmSale(const FarmSaleResult& result) = 0;
    virtual void RouteFarmToolFeedback(const FarmToolActionResult& result) = 0;
    virtual bool ConvertMouseToVirtualScreen(const Vector2& position, Vector2& output) const = 0;
    virtual bool TryBuildViewportRay(Vector3& origin, Vector3& direction) const = 0;
    virtual ScreenTransitionResult BeginScreenTransition(scene::RipplePoint origin,
        scene::TransitionSoundTheme sound, scene::TransitionStyle style) = 0;
};

// Dependencies are borrowed for one synchronous controller call, never retained by it.
struct FarmRuntimeContext {
    FarmRuntimeHost& host;
    const WinApp* window;
    const FarmArrivalRenderer& farmArrivalRenderer;
    FarmContestDaySystem& farmContestDaySystem;
    FarmCropSelectionSystem& farmCropSelectionSystem;
    FarmDateSystem& farmDateSystem;
    FarmDocumentSystem& farmDocumentSystem;
    FarmEconomySystem& farmEconomySystem;
    FarmFeedbackSystem& farmFeedbackSystem;
    farm::FarmGrid& farmGrid;
    FarmGrowthComparisonSystem& farmGrowthComparisonSystem;
    FarmGrowthSystem& farmGrowthSystem;
    FarmInputSystem& farmInputSystem;
    farm::FarmIrrigationPreviewSystem& farmIrrigationPreviewSystem;
    farm::FarmIrrigationSystem& farmIrrigationSystem;
    FarmProgressionSystem& farmProgressionSystem;
    const FarmSeedShopRenderer& farmSeedShopRenderer;
    FarmStageAccessSystem& farmStageAccessSystem;
    FarmToolActionSystem& farmToolActionSystem;
    FarmToolSystem& farmToolSystem;
    const farm::FarmVisualSystem& farmVisualSystem;
    editor::GamePlayEditorBridge& gamePlayEditorBridge;
    const level::LevelGameplaySystem& levelGameplay;
};
