#pragma once
#include "3d/Object3d.h"
#include "base/InitializationTask.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include "farm/render/FarmContestStageLayout.h"
#include "title/TitleSkyRenderer.h"

class ModelManager;
// Session-owned fixed comparison stage. ModelManager owns meshes beyond this renderer's lifetime.
class FarmContestRenderer final {
public:
    InitializationTask InitializeIncrementally(Object3dCommon* common, ModelManager* models, Texture2DHandle white,
        TextureManager* textures);
    void Draw(const FarmContestPresentationSystem::Frame& frame);
private:
    enum CeremonyPart : std::size_t { Venue, Board, Sign, Award, Tails, Review, Glints, Contact, Water, PartCount };
    void PrepareCeremony(const FarmContestPresentationSystem::Frame& frame);
    void DrawComparison(const FarmContestPresentationSystem::Frame& frame);
    Object3dCommon* common_ = nullptr; // Borrowed; session finalizes before the framework.
    std::array<bool,PartCount> visible_{};
    bool cropVisible_ = false;
    std::array<Model*,8> meshes_{};
    std::array<Model*,8> ceremonyMeshes_{};
    std::array<Object3d,PartCount> ceremony_{};
    std::array<Object3d,2> ceremonyCrops_{};
    std::array<Texture2DHandle,5> signs_{};
    std::array<Object3d,4> crops_{};
    std::array<Object3d,3> stage_{};
    Camera camera_;
    title::TitleSkyRenderer sky_;
    bool ready_ = false;
};
