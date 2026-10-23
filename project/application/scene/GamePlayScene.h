#pragma once
#include "scene/BaseScene.h"
#include "editor/GamePlayEditorScene.h"
#include "math/Struct.h"
#include <memory>
#include <cstdint>
class GamePlaySessionSystem;
class Camera;
class FieldManager;
class SkyboxManager;
namespace editor { class GamePlayEditorBridge; }
class GamePlayScene final : public BaseScene, public editor::GamePlayEditorScene {
public:
    GamePlayScene();
    ~GamePlayScene() override;
    void Initialize() override;
    InitializationTask InitializeIncrementally() override;
    void Finalize() override;
    void PrepareFixedUpdate() override;
    void OnScreenTransitionCovered() override;
    void FixedUpdate(float fixedDeltaTime) override;
    void Update() override;
    void Draw() override;
    SceneClipPlanes GetPostEffectClipPlanes() const noexcept override;
    editor::GamePlayEditorBridge& GetEditorBridge() noexcept override;
    const editor::GamePlayEditorBridge& GetEditorBridge() const noexcept;
    const Camera* GetCamera() const noexcept;
    bool IsFarmGameMode() const noexcept override;
    bool SetFarmGameMode(bool enabled) override;
    void RequestFarmLayoutLibrary() noexcept override;
private:
    std::unique_ptr<GamePlaySessionSystem> session_;
};
