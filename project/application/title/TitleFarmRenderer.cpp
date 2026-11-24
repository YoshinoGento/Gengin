#include "title/TitleFarmRenderer.h"
#include "environment/VegetationWind.h"
#include "title/TitleCelestialGeometry.h"
#include "title/TitleCropPresentation.h"
#include "farm/render/FarmMaterialStyle.h"
#include "farm/render/FarmOutdoorLighting.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "base/Logger.h"
#include <algorithm>
#include <filesystem>
#include <string>

namespace title {
TitleFarmRenderer::TitleFarmRenderer() = default;
TitleFarmRenderer::~TitleFarmRenderer() { Finalize(); }

bool TitleFarmRenderer::Initialize(Object3dCommon* common, ModelManager* modelManager,
    TextureManager* textures, const farm::FarmGrid& grid, const PondLayout* pond) {
    return InitializeIncrementally(common,modelManager,textures,grid,pond).Complete();
}
InitializationTask TitleFarmRenderer::InitializeIncrementally(Object3dCommon* common, ModelManager* modelManager,
    TextureManager* textures, const farm::FarmGrid& grid, const PondLayout* pond) {
    if (common_ || !common || !modelManager || !textures || !common->GetLightingSystem() ||
        grid.GetTileCount() != static_cast<int>(kTitleTileCount)) co_return false;
    constexpr auto& paths=farm::kFarmMeshPaths;
    std::error_code error;
    for (std::size_t i=0; i<paths.size(); ++i) {
        // Turnip is not displayed by the staged farm; avoid loading an excluded submission asset.
        const auto shape=static_cast<farm::FarmMeshShape>(i);
        if (shape==farm::FarmMeshShape::Turnip || shape==farm::FarmMeshShape::Leaves) continue;
        if (!std::filesystem::is_regular_file(std::filesystem::path("Resources")/paths[i],error)) co_return false;
        modelManager->LoadModel(paths[i]); models_[i] = modelManager->GetModel(paths[i]);
        if (!models_[i] || models_[i]->HasSkinCluster()) co_return false;
        co_await std::suspend_always{};
    }
    const auto white = textures->LoadTexture2D("Resources/farm/white.png");
    if (!white.IsValid() || white == textures->GetFallback2D()) co_return false;
    whiteTexture_ = white;
    if (!std::filesystem::is_regular_file("Resources/title/countryside.obj",error) ||
        !std::filesystem::is_regular_file("Resources/title/countryside_atlas.png",error)) co_return false;
    modelManager->LoadModel("title/countryside.obj");
    co_await std::suspend_always{};
    auto* landscapeModel = modelManager->GetModel("title/countryside.obj");
    const auto landscapeTexture = textures->LoadTexture2D("Resources/title/countryside_atlas.png");
    landscapeTexture_ = landscapeTexture;
    if (!landscapeModel || landscapeModel->HasSkinCluster() ||
        !landscapeTexture.IsValid() || landscapeTexture == textures->GetFallback2D()) co_return false;
    landscape_ = std::make_unique<Object3d>();
    landscape_->Initialize(common);
    landscape_->SetModel(landscapeModel);
    landscape_->SetTexture(landscapeTexture);
    landscape_->SetSpecularType(Object3d::SpecularType::None);
    landscape_->SetEnableLighting(true);
    if (!std::filesystem::is_regular_file("Resources/title/pond_shore.obj",error) ||
        !std::filesystem::is_regular_file("Resources/title/pond_stone.png",error)) co_return false;
    modelManager->LoadModel("title/pond_shore.obj");
    co_await std::suspend_always{};
    auto* shoreModel = modelManager->GetModel("title/pond_shore.obj");
    if (!shoreModel || shoreModel->HasSkinCluster()) co_return false;
    const auto shoreTexture = textures->LoadTexture2D("Resources/title/pond_stone.png");
    if (!shoreTexture.IsValid() || shoreTexture == textures->GetFallback2D()) co_return false;
    pondShore_ = std::make_unique<Object3d>();
    pondShore_->Initialize(common); pondShore_->SetModel(shoreModel); pondShore_->SetTexture(shoreTexture);
    pondShore_->SetEnableLighting(true); pondShore_->SetSpecularType(Object3d::SpecularType::None);
    co_await std::suspend_always{};
    if (!sky_.Initialize(common->GetDxCommon(),textures)) co_return false;
    co_await std::suspend_always{};
    if (!grass_.Initialize(common,modelManager)) co_return false;
    co_await std::suspend_always{};
    common_ = common; lights_ = common->GetLightingSystem();
    previousDirectional_ = lights_->GetDirectionalLight(); previousSpot_ = lights_->GetSpotLight();
    previousShadowStrength_ = common_->GetShadowStrength();
    camera_.SetTranslate(kCameraPosition); camera_.SetRotate(kCameraRotation);
    camera_.SetAspectRatio(kWidth/kHeight); camera_.SetFovY(kCameraFovY);
    camera_.SetNearClip(.1f); camera_.SetFarClip(180); camera_.Update();
    if (pond) {
        pond_ = *pond;
        activeGlintCount_ = kGlintCount;
        if (std::filesystem::is_regular_file("Resources/title/reflection_scenery.obj",error)) {
            modelManager->LoadModel("title/reflection_scenery.obj");
            co_await std::suspend_always{};
            reflectedScenery_ = modelManager->GetModel("title/reflection_scenery.obj");
        }
        if (!reflectedScenery_ || reflectedScenery_->HasSkinCluster() ||
            !reflection_.Initialize(common->GetDxCommon(),common->GetSrvManager(),textures))
            Logger::Warning("Title planar reflection unavailable; pond uses the snapshot fallback.");
    }
    co_await std::suspend_always{};
    farm::FarmVisualLayout layout;
    layout.center = kFarmCenter; layout.tileGap = .05f; layout.heightStep = .27f;
    farm::FarmVisualSystem visual; visual.Initialize(layout);
    parts_.reserve(kMaximumParts);
    crops_.reserve(kTitleTileCount);
    for (int index=0; index<grid.GetTileCount(); ++index) {
        const auto terrain = farm::BuildFarmTileMeshParts(grid,index,visual);
        auto cropVisual = visual.GetTileVisualData(grid,index);
        cropVisual.cropStage = farm::FarmCropGrowthStage::Ready;
        const auto crops = farm::BuildFarmCropMeshParts(cropVisual,1.0f);
        if (parts_.size()+terrain.count+crops.count+4 > kMaximumParts) co_return false;
        for (std::size_t p=0; p<terrain.count; ++p) {
            const auto& part = terrain.parts[p];
            // Open only the staged canal's downhill end; playable Farm geometry is unchanged.
            const bool outfallBank = pond && index == 2 && part.surface == farm::FarmMeshSurface::CanalBank &&
                part.position.z < cropVisual.center.z-.5f && part.scale.z < .12f;
            if (!outfallBank) parts_.push_back(part);
        }
        if (crops.count) {
            CropInstance instance;
            instance.tileIndex = static_cast<std::size_t>(index);
            instance.visual = cropVisual;
            instance.count = crops.count;
            for (std::size_t i=0; i<crops.count; ++i) instance.partIndices[i] = parts_.size()+i;
            crops_.push_back(instance);
        }
        parts_.insert(parts_.end(),crops.parts.begin(),crops.parts.begin()+crops.count);
    }
    for (std::size_t row=0; row<waterCenters_.size(); ++row)
        waterCenters_[row] = visual.GetTileVisualData(grid,static_cast<int>(row)*5+2).waterSurfaceCenter;
    const auto source = waterCenters_.back();
    const Vector4 wood{.37f,.25f,.12f,1};
    for (int side : {-1,1}) parts_.push_back({{source.x+side*.40f,source.y+.26f,source.z},
        {.045f,.28f,.045f},wood});
    parts_.push_back({{source.x,source.y+.48f,source.z},{.50f,.045f,.07f},wood});
    if (pond) {
        for (std::size_t i=1; i<pond->outfall.size(); ++i) {
            const auto a = pond->outfall[i-1], b = pond->outfall[i];
            const float slope = (a.y-b.y)/(a.z-b.z);
            parts_.push_back({{a.x,(a.y+b.y)*.5f-.004f,(a.z+b.z)*.5f},
                {pond->outfallHalfWidth,.004f,(a.z-b.z)*.5f}, {},true,{0,slope}});
        }
    }
    if (parts_.size()>kMaximumParts) co_return false;
    objects_.reserve(parts_.size());
    visible_.assign(parts_.size(),1);
    drawColors_.resize(parts_.size());
    for (const auto& part : parts_) {
        const auto shape = static_cast<std::size_t>(part.shape);
        if (shape >= models_.size() || !models_[shape]) co_return false;
        auto object = std::make_unique<Object3d>(); object->Initialize(common_);
        object->SetModel(models_[shape]); object->SetTexture(white);
        object->SetPosition(part.position);
        object->SetRotation({0,part.yaw,0});
        if (!object->SetScale(part.scale) || !object->SetShearY(part.slope)) co_return false;
        const auto material = farm::BuildFarmMaterialStyle(part);
        object->SetSpecularType(material.detail.x > 0.0f ?
            Object3d::SpecularType::BlinnPhong : Object3d::SpecularType::None);
        object->SetShininess(material.shininess);
        if (!object->SetSurfaceDetail(material.detail)) co_return false;
        object->SetEnableLighting(!part.water);
        objects_.push_back(std::move(object));
        co_await std::suspend_always{};
    }
    for (auto& glint : glints_) {
        glint = std::make_unique<Object3d>(); glint->Initialize(common_);
        glint->SetModel(models_[0]); glint->SetTexture(white);
        if (!glint->SetScale({.10f,.002f,.012f})) co_return false;
        glint->SetEnableLighting(false); glint->SetSpecularType(Object3d::SpecularType::None);
        co_await std::suspend_always{};
    }
    Logger::Log("TitleFarmRenderer: retained objects="+std::to_string(objects_.size())+", glints="+std::to_string(kGlintCount)+".\n");
    ready_ = true;
    co_return true;
}

void TitleFarmRenderer::Finalize() noexcept {
    if (lights_) { lights_->SetDirectionalLight(previousDirectional_); lights_->SetSpotLight(previousSpot_); }
    if (common_) common_->SetShadowStrength(previousShadowStrength_);
    lights_ = nullptr; common_ = nullptr; ready_ = false;
    activeGlintCount_ = kCanalGlintCount;
    reflection_.Finalize(); reflectedScenery_ = nullptr;
    objects_.clear(); parts_.clear(); visible_.clear(); drawColors_.clear(); crops_.clear();
    landscape_.reset();
    pondShore_.reset();
    grass_.Finalize();
    sky_.Finalize();
    for (auto& glint : glints_) glint.reset();
}

void TitleFarmRenderer::Update(const Frame& frame) {
    if (!ready_) return;
    const auto& projection = camera_.GetProjectionMatrix();
    const auto& cameraWorld = camera_.GetWorldMatrix();
    const auto lighting = MakeCelestialLighting(CelestialDirection(frame.sun, projection, cameraWorld),
        CelestialDirection(frame.moon, projection, cameraWorld),frame.groundExposure);
    lights_->SetDirectionalLight({frame.lightColor,lighting.direction,lighting.intensity});
    common_->SetShadowStrength(lighting.shadowStrength);
    auto spot = previousSpot_; spot.intensity = 0; lights_->SetSpotLight(spot);
    lights_->SetCameraPosition(camera_.GetTranslate());
    sky_.Update(frame,camera_);
    grass_.Update(frame,camera_);
    reflection_.Update(frame,camera_,pond_.center.y,lights_->GetDirectionalLight());
    const auto ambient = farm::BuildOutdoorAmbientTint(frame.skyTop,frame.skyHorizon);
    landscape_->SetOutdoorAmbient(ambient);
    pondShore_->SetOutdoorAmbient(ambient);
    landscape_->Update(&camera_,0);
    pondShore_->Update(&camera_,0);
    // Unlit canal surfaces still follow daylight; otherwise they glow against the night pond.
    const float waterExposure = .08f+.42f*std::clamp(frame.groundExposure,0.0f,1.0f);
    const Vector4 canalColor{
        (.70f*frame.waterColor.x+.30f*frame.skyHorizon.x)*waterExposure,
        (.70f*frame.waterColor.y+.30f*frame.skyHorizon.y)*waterExposure,
        (.70f*frame.waterColor.z+.30f*frame.skyHorizon.z)*waterExposure,1};
    for (std::size_t i=0; i<parts_.size(); ++i) {
        const auto& part = parts_[i]; auto& object = *objects_[i];
        object.SetOutdoorAmbient(ambient);
        if (part.surface == farm::FarmMeshSurface::Crop) continue;
        drawColors_[i] = part.water ? canalColor : part.color;
        object.SetColor(drawColors_[i]);
        object.Update(&camera_,0);
    }
    for (const auto& crop : crops_) {
        const auto meshes = BuildTitleCropParts(crop.visual,frame.crops[crop.tileIndex]);
        for (std::size_t retained=0; retained<crop.count; ++retained) {
            const auto index = crop.partIndices[retained];
            visible_[index] = 0;
            for (std::size_t current=0; current<meshes.count; ++current) {
                const auto& mesh = meshes.parts[current];
                if (mesh.shape != parts_[index].shape) continue;
                auto& object = *objects_[index];
                if (!object.SetScale(mesh.scale)) break;
                object.SetColor(mesh.color);
                drawColors_[index] = mesh.color;
                const vegetation::WindFrame wind{frame.grassWindXZ,frame.grassPhase};
                const auto pose = farm::CropWindPose(mesh, wind);
                object.SetPosition(pose.position);
                object.SetRotation(pose.rotation);
                object.Update(&camera_,0);
                visible_[index] = 1;
                break;
            }
        }
    }
    for (std::size_t i=0; i<activeGlintCount_; ++i) {
        const float phase = std::fmod(frame.waterPhase + static_cast<float>(i%2)*.5f,1.0f);
        Vector3 position{};
        if (i < kCanalGlintCount) {
            position = waterCenters_[i/2];
            position.x += i%2 ? .12f : -.17f;
            position.z -= (phase-.5f)*.64f;
        } else {
            const std::size_t segment = (i-kCanalGlintCount)/2;
            const auto a = pond_.outfall[segment], b = pond_.outfall[segment+1];
            position = {a.x+(i%2 ? .10f : -.10f),a.y+(b.y-a.y)*phase,a.z+(b.z-a.z)*phase};
        }
        position.y += .014f;
        glints_[i]->SetPosition(position);
        const float brightness = (.45f+.38f*std::sin(phase*3.14159265f))*waterExposure;
        glints_[i]->SetColor({brightness*(1-frame.nightAmount*.4f),brightness,brightness,1});
        glints_[i]->Update(&camera_,0);
    }
}

void TitleFarmRenderer::Draw() {
    if (!ready_) return;
    common_->UpdateDirectionalShadow(lights_->GetDirectionalLight().direction,kFarmCenter);
    if (common_->BeginShadowPass()) {
        landscape_->DrawShadow();
        pondShore_->DrawShadow();
        for (std::size_t i=0; i<parts_.size(); ++i)
            if (visible_[i] && !parts_[i].water) objects_[i]->DrawShadow();
        grass_.DrawShadow();
        common_->EndShadowPass();
    }
    if (reflection_.Begin()) {
        if (reflectedScenery_) reflection_.Draw(reflectedScenery_,MatrixMath::MakeIdentity4x4(),landscapeTexture_,{1,1,1,1});
        for (std::size_t i=0; i<objects_.size(); ++i)
            if (visible_[i] && !parts_[i].water)
                reflection_.Draw(objects_[i]->GetModel(),objects_[i]->GetWorldMatrix(),whiteTexture_,drawColors_[i]);
        grass_.DrawReflection(pond_.center.y);
        reflection_.End();
    }
    sky_.Draw();
    common_->BeginObjectPass();
    landscape_->Draw();
    pondShore_->Draw();
    for (std::size_t i=0; i<objects_.size(); ++i) if (visible_[i]) objects_[i]->Draw();
    for (std::size_t i=0; i<activeGlintCount_; ++i) glints_[i]->Draw();
    grass_.Draw();
    common_->EndObjectPass();
}
}
