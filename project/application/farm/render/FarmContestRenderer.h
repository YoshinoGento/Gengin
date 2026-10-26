#pragma once
#include "3d/Object3d.h"
#include "base/InitializationTask.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include "farm/render/FarmContestStageLayout.h"

class ModelManager;
// Session-owned fixed comparison stage. ModelManager owns meshes beyond this renderer's lifetime.
class FarmContestRenderer final {
public:
    InitializationTask InitializeIncrementally(Object3dCommon* common, ModelManager* models, Texture2DHandle white);
    void Draw(const FarmContestPresentationSystem::Frame& frame);
private:
    std::array<Model*,8> meshes_{};
    std::array<Object3d,4> crops_{};
    std::array<Object3d,3> stage_{};
    Camera camera_;
    bool ready_ = false;
};
