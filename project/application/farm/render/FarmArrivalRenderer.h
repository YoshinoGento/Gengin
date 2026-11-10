#pragma once
#include "base/InitializationTask.h"
#include "farm/system/FarmStartPresentationSystem.h"
#include "farm/system/FarmRainPresentationSystem.h"
#include "title/TitleGrassRenderer.h"
#include "title/TitleSkyRenderer.h"
#include "3d/Object3d.h"
#include <array>

class Camera;
class ModelManager;
class Object3dCommon;

// Scene-owned decoration; all geometry is outside the supported editable plot.
class FarmArrivalRenderer final {
public:
    bool Initialize(Object3dCommon* common, ModelManager* models, TextureManager* textures);
    InitializationTask InitializeIncrementally(Object3dCommon* common, ModelManager* models, TextureManager* textures);
    void Prepare(Camera* camera, const FarmStartPresentationSystem::Frame& frame,
        bool supported, float deltaSeconds, const farm::FarmRainFrame& weather = {});
    void DrawSky();
    void Draw();
    void DrawShadow();
    [[nodiscard]] bool IsReady() const noexcept { return ready_; }
private:
    enum Part : std::size_t { Scenery, Leaf, Drop, Wet, Wood, Metal, Carrot, Tomato, Pumpkin, Shine, Splash1, Splash2, Splash3, Count };
    Object3dCommon* common_ = nullptr; // Framework-owned, outlives this renderer.
    std::array<Object3d,Count> objects_;
    std::array<bool,Count> visible_{};
    title::TitleGrassRenderer grass_;
    title::TitleSkyRenderer sky_;
    float ambientTime_ = 0;
    bool ready_ = false, visibleScene_ = false;
};
