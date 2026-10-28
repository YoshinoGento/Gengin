#include "farm/render/FarmContestRenderer.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include <filesystem>

InitializationTask FarmContestRenderer::InitializeIncrementally(Object3dCommon* common, ModelManager* models, Texture2DHandle white,
    TextureManager* textures) {
    if (ready_) co_return true;
    if (!common || !models || !textures || !white.IsValid()) co_return false;
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
    const auto palette = textures->LoadTexture2D("Resources/farm/contest/ceremony/palette.png");
    const auto review = textures->LoadTexture2D("Resources/farm/contest/ceremony/review.png");
    if (!palette.IsValid() || !review.IsValid() || palette==textures->GetFallback2D() || review==textures->GetFallback2D()) co_return false;
    constexpr std::array<const char*,5> signNames{{"judging","passed","failed","missed","recorded"}};
    for (std::size_t i=0;i<signs_.size();++i) {
        signs_[i]=textures->LoadTexture2D(std::string("Resources/farm/contest/ceremony/")+signNames[i]+".png");
        if (!signs_[i].IsValid() || signs_[i]==textures->GetFallback2D()) co_return false;
        co_await std::suspend_always{};
    }
    if (!sky_.Initialize(common->GetDxCommon(),textures)) co_return false;
    co_await std::suspend_always{};
    constexpr std::array<const char*,PartCount> partNames{{"venue","board","sign","award","tails","review","glints","contact","water"}};
    for (std::size_t i=0;i<PartCount+ceremonyMeshes_.size();++i) {
        const std::string name = i<PartCount ? partNames[i] : std::filesystem::path(paths[i-PartCount]).stem().string();
        const std::string path = "farm/contest/ceremony/"+name+".obj";
        std::error_code error;
        if (!std::filesystem::is_regular_file(std::filesystem::path("Resources")/path,error)) co_return false;
        models->LoadModel(path); auto* mesh=models->GetModel(path);
        if (!mesh || mesh->HasSkinCluster()) co_return false;
        if (i<PartCount) {
            auto& object=ceremony_[i]; object.Initialize(common); object.SetModel(mesh);
            object.SetTexture(i==Review ? review : i==Sign ? signs_[0] : palette);
            object.SetEnableLighting(i!=Glints); object.SetCullMode(0);
            object.SetOutdoorAmbient({.82f,.90f,1.f,1.f});
            object.SetShininess(i==Water ? 96.f : 32.f);
            object.SetSurfaceDetail({i==Water ? .22f : .025f,0,0,0});
        } else ceremonyMeshes_[i-PartCount]=mesh;
        co_await std::suspend_always{};
    }
    for (auto& object:ceremonyCrops_) {
        object.Initialize(common); object.SetTexture(palette); object.SetEnableLighting(true);
        object.SetOutdoorAmbient({.82f,.90f,1.f,1.f});
        object.SetShininess(48.f); object.SetSurfaceDetail({.06f,0,0,0});
        co_await std::suspend_always{};
    }
    camera_.SetTranslate({0,0,-FarmContestStageLayout::kCameraDistance}); camera_.SetRotate({0,0,0});
    camera_.SetFovY(FarmContestStageLayout::kFovY);
    camera_.SetAspectRatio(FarmContestStageLayout::kWidth/FarmContestStageLayout::kHeight); camera_.Update();
    common_ = common; ready_ = true; co_return true;
}

void FarmContestRenderer::Draw(const FarmContestPresentationSystem::Frame& frame) {
    if (!ready_ || !common_ || !frame.active) return;
    if (frame.page==FarmContestPresentationSystem::Page::Result || frame.unsubmitted) {
        PrepareCeremony(frame);
        common_->SetShadowStrength(FarmContestStageLayout::kCeremonyShadowStrength);
        common_->UpdateDirectionalShadow(FarmContestStageLayout::kCeremonyLightDirection,
            FarmContestStageLayout::kCeremonyFocus);
        if (common_->BeginShadowPass()) {
            for (std::size_t i=0;i<PartCount;++i)
                if (visible_[i] && i!=Water && i!=Glints && i!=Contact) ceremony_[i].DrawShadow();
            if (cropVisible_) for (auto& crop:ceremonyCrops_) crop.DrawShadow();
            common_->EndShadowPass();
        }
        // The prepared buffers remain unchanged between shadow and color draws.
        sky_.Draw();
        common_->BeginObjectPass();
        for (std::size_t i=0;i<PartCount;++i) if (visible_[i]) ceremony_[i].Draw();
        if (cropVisible_) for (auto& crop:ceremonyCrops_) crop.Draw();
        common_->EndObjectPass();
        return;
    }
    common_->SetShadowStrength(0);
    common_->BeginObjectPass();
    DrawComparison(frame);
    common_->EndObjectPass();
}

void FarmContestRenderer::DrawComparison(const FarmContestPresentationSystem::Frame& frame) {
    if (!frame.comparison.valid) return;
    camera_.SetTranslate({0,0,-FarmContestStageLayout::CameraDistance(frame.specimenReveal)});
    camera_.SetRotate({0,0,0});
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

void FarmContestRenderer::PrepareCeremony(const FarmContestPresentationSystem::Frame& frame) {
    const auto camera=FarmContestStageLayout::CeremonyView(frame);
    camera_.SetTranslate(camera.position); camera_.SetRotate({camera.pitch,0,0}); camera_.Update();
    sky_.Update(FarmContestStageLayout::CeremonySky(frame),camera_);
    const auto draw=[&](Object3d& object,Vector3 position,Vector3 scale,Vector3 rotation=Vector3{},Vector4 color=Vector4{1,1,1,1}) {
        if (!object.SetScale(scale)) return;
        object.SetPosition(position); object.SetRotation(rotation); object.SetColor(color);
        object.Update(&camera_,0);
    };
    visible_.fill(false);
    for (auto part:{Venue,Glints,Board,Sign,Water}) visible_[part]=true;
    draw(ceremony_[Venue],{}, {1,1,1});
    draw(ceremony_[Water],{}, {1,1,1});
    draw(ceremony_[Glints],{0,0,std::sin(frame.ceremonyTime*.5f)*.18f},{1,1,1},{},{.70f,.86f,.91f,1});
    const auto& result=frame.comparison;
    const int slot=farm::ToCropSlot(result.quality.crop);
    cropVisible_=!frame.unsubmitted && result.valid && slot>=0 && slot<farm::kFarmCropTypeCount;
    if (cropVisible_) {
        const auto pose=FarmContestStageLayout::CeremonySpecimen(result.quality.crop,result.quality.harvestSize.multiplier);
        auto& body=ceremonyCrops_[0]; auto& leaves=ceremonyCrops_[1];
        body.SetModel(ceremonyMeshes_[slot*2]); leaves.SetModel(ceremonyMeshes_[slot*2+1]);
        draw(body,pose.position,pose.bodyScale,{0,FarmContestStageLayout::kYaw,0});
        draw(leaves,{pose.position.x,pose.leafY,pose.position.z},pose.leafScale,{0,FarmContestStageLayout::kYaw,0});
    }
    const float flip=frame.verdictVisible ? 6.2831853f*frame.verdictReveal : 0;
    const Vector3 signPosition{-1.10f,1.30f,-.35f};
    draw(ceremony_[Board],signPosition,{1,1,1},{0,flip,0});
    // Change the face while the placard is turned away, never switch a front-facing word.
    const int sign = frame.verdictVisible && frame.verdictReveal>=.5f ?
        (frame.unsubmitted ? 3 : !result.staged ? 4 : result.passed ? 1 : 2) : 0;
    ceremony_[Sign].SetTexture(signs_[sign]);
    draw(ceremony_[Sign],signPosition,{1,1,1},{0,flip,0});
    const float award=frame.adviceReveal;
    if (award>0 && !frame.unsubmitted && result.staged) {
        if (result.passed) {
            visible_[Tails]=visible_[Award]=true;
            const Vector3 position{-2.15f,.51f+(1-award)*.48f,-1.20f};
            draw(ceremony_[Tails],position,{1,1,1},{},{1,1,1,award});
            draw(ceremony_[Award],position,{1,1,1},{},{1,1,1,award});
        } else {
            visible_[Review]=true;
            draw(ceremony_[Review],{-1.14f,.60f+(1-award)*.40f,-1.23f},{1,1,1},{-.12f,0,0},{1,1,1,award});
        }
    }
}
