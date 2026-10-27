#include "farm/render/FarmContestRenderer.h"
#include "3d/ModelManager.h"
#include <filesystem>

InitializationTask FarmContestRenderer::InitializeIncrementally(Object3dCommon* common, ModelManager* models, Texture2DHandle white) {
    if (ready_) co_return true;
    if (!common || !models || !white.IsValid()) co_return false;
    constexpr std::array<const char*,8> paths{{"farm/crop_turnip.obj","farm/crop_turnip_leaves.obj",
        "farm/crop_carrot.obj","farm/crop_carrot_leaves.obj","farm/crop_tomato.obj","farm/crop_tomato_stems.obj",
        "farm/crop_pumpkin.obj","farm/crop_pumpkin_vines.obj"}};
    for (std::size_t i=0; i<paths.size(); ++i) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(std::filesystem::path("Resources")/paths[i],error)) co_return false;
        models->LoadModel(paths[i]); meshes_[i] = models->GetModel(paths[i]);
        if (!meshes_[i] || meshes_[i]->HasSkinCluster()) co_return false;
        co_await std::suspend_always{};
    }
    models->LoadModel("farm/unit_box.obj");
    auto* box = models->GetModel("farm/unit_box.obj");
    if (!box || box->HasSkinCluster()) co_return false;
    for (auto& object : stage_) {
        object.Initialize(common); object.SetModel(box); object.SetTexture(white); object.SetEnableLighting(false);
        co_await std::suspend_always{};
    }
    for (auto& object : crops_) {
        object.Initialize(common); object.SetTexture(white); object.SetEnableLighting(true);
        object.SetSpecularType(Object3d::SpecularType::None);
        co_await std::suspend_always{};
    }
    camera_.SetTranslate({0,0,-FarmContestStageLayout::kCameraDistance}); camera_.SetRotate({0,0,0});
    camera_.SetFovY(FarmContestStageLayout::kFovY);
    camera_.SetAspectRatio(FarmContestStageLayout::kWidth/FarmContestStageLayout::kHeight); camera_.Update();
    ready_ = true; co_return true;
}

void FarmContestRenderer::Draw(const FarmContestPresentationSystem::Frame& frame) {
    if (!ready_ || !frame.comparison.valid) return;
    camera_.SetTranslate({0,0,-FarmContestStageLayout::CameraDistance(frame.specimenReveal)});
    camera_.Update();
    const auto& comparison = frame.comparison;
    const int crop = farm::ToCropSlot(comparison.quality.crop);
    if (crop < 0 || crop >= farm::kFarmCropTypeCount) return;
    const auto draw = [&](Object3d& object, Vector3 p, Vector3 scale, Vector4 color) {
        object.SetPosition(p); if (!object.SetScale(scale)) return;
        object.SetColor(color); object.Update(&camera_,0); object.Draw();
    };
    draw(stage_[0],{0,0,4},{18,11,.1f},{.075f,.09f,.10f,1});
    if (frame.page == FarmContestPresentationSystem::Page::Quality) return;
    const float largest = (std::max)({1.0f, comparison.quality.harvestSize.multiplier, comparison.referenceSize});
    for (int side=0; side<2; ++side) {
        if (side==1 && (frame.page != FarmContestPresentationSystem::Page::Size || !comparison.referenceAvailable)) continue;
        const float multiplier = side==0 ? comparison.quality.harvestSize.multiplier : comparison.referenceSize;
        const auto pose = FarmContestStageLayout::Specimen(comparison.quality.crop,multiplier,largest,side,frame.page);
        draw(stage_[side+1],{pose.position.x,pose.position.y-.12f,0},{1.35f,.09f,.65f},side==0 ?
            Vector4{.55f,.42f,.19f,1} : Vector4{.19f,.44f,.46f,1});
        auto& body = crops_[side*2]; auto& leaves = crops_[side*2+1];
        body.SetModel(meshes_[crop*2]); leaves.SetModel(meshes_[crop*2+1]);
        body.SetRotation({0,FarmContestStageLayout::kYaw,0}); leaves.SetRotation({0,FarmContestStageLayout::kYaw,0});
        const bool carrot = comparison.quality.crop==farm::CropType::Carrot;
        const bool tomato = comparison.quality.crop==farm::CropType::Tomato;
        const bool pumpkin = comparison.quality.crop==farm::CropType::Pumpkin;
        draw(body,pose.position,pose.bodyScale,carrot ? Vector4{.94f,.43f,.10f,1} : tomato ?
            Vector4{.87f,.12f,.07f,1} : pumpkin ? Vector4{.12f,.36f,.16f,1} : Vector4{.91f,.79f,.87f,1});
        draw(leaves,{pose.position.x,pose.leafY,0},pose.leafScale,{.28f,.56f,.20f,1});
    }
}
