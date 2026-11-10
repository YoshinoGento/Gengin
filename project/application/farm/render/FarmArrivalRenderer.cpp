#include "farm/render/FarmArrivalRenderer.h"
#include "farm/render/FarmOutdoorLighting.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "3d/LightingSystem.h"
#include "3d/Camera.h"
#include <filesystem>

bool FarmArrivalRenderer::Initialize(Object3dCommon* common, ModelManager* models, TextureManager* textures) {
    return InitializeIncrementally(common,models,textures).Complete();
}
InitializationTask FarmArrivalRenderer::InitializeIncrementally(Object3dCommon* common, ModelManager* models, TextureManager* textures) {
    if (ready_) co_return true;
    if (!common || !models || !textures || !common->GetLightingSystem()) co_return false;
    constexpr std::array<const char*,Count> paths{"scenery","leaf","drop","wet","tools_wood","tools_metal",
        "packet_carrot","packet_tomato","packet_pumpkin","drop","drop","drop","drop"};
    std::array<Model*,Count> loaded{};
    for(std::size_t i=0;i<Count;++i) {
        const std::string path=std::string("farm/arrival/")+paths[i]+".obj";
        std::error_code error;
        if(!std::filesystem::is_regular_file(std::filesystem::path("Resources")/path,error)) co_return false;
        models->LoadModel(path); loaded[i]=models->GetModel(path);
        if(!loaded[i] || loaded[i]->HasSkinCluster()) co_return false;
        co_await std::suspend_always{};
    }
    const auto white=textures->LoadTexture2D("Resources/farm/white.png");
    const auto land=textures->LoadTexture2D("Resources/title/countryside_atlas.png");
    const auto seeds=textures->LoadTexture2D("Resources/ui/farm_seed_packets.png");
    const auto formal=textures->LoadTexture2D("Resources/ui/farm_seed_packets_formal.png");
    for(const auto texture:{white,land,seeds,formal})
        if(!texture.IsValid() || texture==textures->GetFallback2D()) co_return false;
    co_await std::suspend_always{};
    if(!grass_.Initialize(common,models,"Resources/farm/arrival/grass_layout.json")) co_return false;
    co_await std::suspend_always{};
    if(!sky_.Initialize(common->GetDxCommon(),textures)) co_return false;
    co_await std::suspend_always{};
    common_=common;
    for(std::size_t i=0;i<Count;++i) {
        auto& object=objects_[i];
        object.Initialize(common); object.SetModel(loaded[i]);
        object.SetTexture(i==Scenery || i==Leaf || i==Wood ? land : i==Carrot ? seeds :
            i==Tomato || i==Pumpkin ? formal : white);
        object.SetCullMode(0);
        object.SetSpecularType(Object3d::SpecularType::None);
        object.SetEnableLighting(i<=Metal && i!=Wet);
        co_await std::suspend_always{};
    }
    for(const auto index:{Drop,Metal}) {
        objects_[index].SetSpecularType(Object3d::SpecularType::BlinnPhong);
        objects_[index].SetShininess(index==Drop ? 96.0f : 30.0f);
    }
    ready_=true;
    co_return true;
}

void FarmArrivalRenderer::Prepare(Camera* camera, const FarmStartPresentationSystem::Frame& frame,
    bool supported, float deltaSeconds, const farm::FarmRainFrame& weather) {
    visibleScene_=ready_ && supported && camera;
    if(!visibleScene_) return;
    using namespace farm::arrival;
    const float dt=std::isfinite(deltaSeconds) ? std::clamp(deltaSeconds,0.0f,.1f) : 0;
    ambientTime_=std::fmod(ambientTime_+dt,1000.0f);
    const float time=frame.arrival ? frame.arrivalTime : ambientTime_;
    const auto arrival=Sample(frame.arrival ? time : kDuration,{});
    // One light snapshot before both shadow/color passes. No mid-pass constant rewrites.
    auto* lights=common_->GetLightingSystem();
    auto light=lights->GetDirectionalLight();
    const float warm=frame.arrival ? .24f*(1-Smooth((time-kToolsEnd)/(kSettle-kToolsEnd))) : 0;
    light.color.y*=1-.12f*warm; light.color.z*=1-.40f*warm;
    lights->SetDirectionalLight(light);
    title::Frame atmosphere;
    atmosphere.skyTop={.19f,.48f,.69f,1}; atmosphere.skyHorizon={.79f,.83f,.69f,1};
    atmosphere.cloudColor={.92f,.92f,.84f,1};
    atmosphere.groundExposure=.55f; atmosphere.grassWindXZ={.30f,.10f};
    const float overcast=std::isfinite(weather.cloud) ? std::clamp(weather.cloud,0.0f,1.0f) : 0;
    const auto tint=[overcast](Vector4 clear,Vector4 rainy) {
        return Vector4{clear.x+(rainy.x-clear.x)*overcast,clear.y+(rainy.y-clear.y)*overcast,
            clear.z+(rainy.z-clear.z)*overcast,1};
    };
    atmosphere.skyTop=tint(atmosphere.skyTop,{.29f,.36f,.41f,1});
    atmosphere.skyHorizon=tint(atmosphere.skyHorizon,{.52f,.59f,.61f,1});
    atmosphere.cloudColor=tint(atmosphere.cloudColor,{.48f,.54f,.57f,1});
    atmosphere.groundExposure*=1-.30f*overcast;
    atmosphere.grassPhase=time*1.1f; atmosphere.skyYaw=time*.001f;
    // Keep celestial discs below the terrain; this opening uses dawn illumination, not another sky clock.
    atmosphere.sun.center={.5f,3}; atmosphere.moon.center={.5f,4};
    sky_.Update(atmosphere,*camera); grass_.Update(atmosphere,*camera);
    visible_.fill(true);
    const auto pose=[&](Part part,Vector3 position,Vector3 scale,Vector4 color,Vector3 rotation=Vector3{}) {
        auto& object=objects_[part]; object.SetPosition(position); object.SetRotation(rotation);
        if(!object.SetScale(scale)) visible_[part]=false;
        object.SetColor(color);
    };
    pose(Scenery,{}, {1,1,1},{1,1,1,1});
    pose(Leaf,kAnchor,{1,1,1},{.72f,.90f,.55f,1},{0,0,arrival.leafAngle});
    pose(Drop,arrival.drop,{(std::max)(arrival.dropScale,.001f), (std::max)(arrival.dropScale,.001f), (std::max)(arrival.dropScale,.001f)},
        {.48f,.78f,.86f,1});
    visible_[Drop]=frame.arrival && arrival.dropScale>0;
    const auto impact=LeafTip(kRelease);
    const float patch=.025f+.18f*arrival.wetness;
    pose(Wet,{impact.x,kAnchor.y+.009f,impact.z},{patch,1,patch*.78f},{.13f,.095f,.055f,1});
    visible_[Wet]=frame.arrival && arrival.wetness>0;
    pose(Wood,kTools,{1,1,1},{.90f,.79f,.60f,1});
    pose(Metal,kTools,{1,1,1},{.48f,.59f,.59f,1});
    for(std::size_t i=0;i<3;++i)
        pose(static_cast<Part>(Carrot+i),{kTools.x-.33f+static_cast<float>(i)*.30f,kTools.y+.15f,kTools.z-.29f},
            {1,1,1},{1,1,1,1},{.16f,0,0});
    auto glint=arrival.drop; glint.x-=.018f; glint.y+=.014f; glint.z-=.035f;
    pose(Shine,glint,{.007f,.014f,.006f},{1,.98f,.86f,1}); visible_[Shine]=visible_[Drop];
    const float splashTime=time-kImpact;
    for(std::size_t i=0;i<3;++i) {
        const float x=static_cast<float>(i)-1;
        pose(static_cast<Part>(Splash1+i),{impact.x+x*splashTime*.48f,kAnchor.y+.015f+splashTime*.85f-splashTime*splashTime*3,
            impact.z+(.5f-std::abs(x))*splashTime}, {.012f,.018f,.012f},{.65f,.85f,.87f,1});
        visible_[Splash1+i]=frame.arrival && splashTime>=0 && splashTime<.26f;
    }
    const auto ambient=farm::BuildOutdoorAmbientTint(atmosphere.skyTop,atmosphere.skyHorizon);
    for(std::size_t i=0;i<Count;++i) if(visible_[i]) {
        objects_[i].SetOutdoorAmbient(ambient);
        objects_[i].Update(camera,0);
    }
}
void FarmArrivalRenderer::DrawSky() { if(visibleScene_) sky_.Draw(); }
void FarmArrivalRenderer::Draw() {
    if(!visibleScene_) return;
    for(std::size_t i=0;i<Count;++i) if(visible_[i]) objects_[i].Draw();
    grass_.Draw();
}
void FarmArrivalRenderer::DrawShadow() {
    if(!visibleScene_) return;
    for(std::size_t i=0;i<Count;++i) if(visible_[i] && i!=Wet && i!=Drop && i!=Shine && i<Splash1) objects_[i].DrawShadow();
    grass_.DrawShadow();
}
