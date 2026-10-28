#pragma once
#include "farm/core/FarmTypes.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include "math/Struct.h"
#include "title/TitlePresentationSystem.h"

// Validated comparison values only; both specimens use one volume-to-linear scale.
struct FarmContestStageLayout {
    static constexpr float kWidth = 1280, kHeight = 720;
    static constexpr float kCameraDistance = 12, kFovY = .75f, kYaw = .35f;
    static constexpr float kCameraPullback = .35f;
    [[nodiscard]] static float CameraDistance(float reveal) noexcept {
        return kCameraDistance + kCameraPullback*(1-std::clamp(reveal,0.f,1.f));
    }
    static constexpr float kSpecimenHeight = 1.35f, kSummarySpecimenHeight = 1.55f, kSpecimenBaseline = 480;
    using Page = FarmContestPresentationSystem::Page;
    struct Pose {
        Vector3 position{}, bodyScale{}, leafScale{};
        float leafY = 0;
    };
    struct CeremonyCamera { Vector3 position; float pitch; };
    static constexpr Vector3 kCeremonyLightDirection{.45f,-.82f,.35f};
    static constexpr Vector4 kCeremonyLightColor{1.f,.96f,.88f,1.f};
    static constexpr float kCeremonyLightIntensity=1.35f, kCeremonyShadowStrength=.82f;
    static constexpr Vector3 kCeremonyFocus{-3.3f,1.2f,0};
    static constexpr float kCeremonySpecimenHeight=2.40f, kCeremonyPitch=.205f;
    [[nodiscard]] static title::Frame CeremonySky(const FarmContestPresentationSystem::Frame& frame) noexcept {
        title::Frame sky;
        sky.skyTop = {.10f,.43f,.75f,1};
        sky.skyHorizon = {.60f,.79f,.86f,1};
        sky.cloudColor = {.92f,.96f,1.f,1};
        sky.skyYaw = frame.cloudOffset.x;
        sky.cloudLift = frame.cloudOffset.y;
        // The ceremony's daylight source is behind the camera, outside this authored sky view.
        sky.sun.center = sky.moon.center = {.5f,-2.f};
        return sky;
    }
    [[nodiscard]] static CeremonyCamera CeremonyView(const FarmContestPresentationSystem::Frame& frame) noexcept {
        const float settle = frame.adviceReveal;
        const float distance = 17.5f - 4.5f * frame.specimenReveal + .7f * settle;
        return {{0,4.6f-.5f*frame.specimenReveal+.1f*settle,-distance}, kCeremonyPitch};
    }
    [[nodiscard]] static Pose CeremonySpecimen(farm::CropType crop, float multiplier) noexcept {
        auto pose = Specimen(crop,multiplier,(std::max)(3.f,multiplier),0,Page::Result);
        const float fit = kCeremonySpecimenHeight/kSummarySpecimenHeight;
        pose.bodyScale = {pose.bodyScale.x*fit,pose.bodyScale.y*fit,pose.bodyScale.z*fit};
        pose.leafScale = {pose.leafScale.x*fit,pose.leafScale.y*fit,pose.leafScale.z*fit};
        pose.position = {-3.3f,.94f,0};
        pose.leafY = crop==farm::CropType::Tomato || crop==farm::CropType::Pumpkin ?
            pose.position.y : pose.position.y+pose.bodyScale.y*.96f;
        return pose;
    }
    [[nodiscard]] static float UnitsPerPixel() noexcept {
        return 2*kCameraDistance*std::tan(kFovY*.5f)/kHeight;
    }
    [[nodiscard]] static Pose Specimen(farm::CropType crop, float multiplier, float largest, int side,
        Page page = Page::Size) noexcept {
        const float height = page==Page::Result ? kSummarySpecimenHeight : kSpecimenHeight;
        const float scale = std::cbrt(multiplier)*height/std::cbrt((std::max)(1.f,largest));
        const bool carrot = crop==farm::CropType::Carrot;
        const bool tomato = crop==farm::CropType::Tomato;
        const bool pumpkin = crop==farm::CropType::Pumpkin;
        Pose pose;
        const float x = side==0 ? 320.f : 960.f;
        const float baseline = kSpecimenBaseline;
        pose.position = {(x-kWidth*.5f)*UnitsPerPixel(), (kHeight*.5f-baseline)*UnitsPerPixel(), 0};
        pose.bodyScale = carrot ? Vector3{.30f*scale,scale,.30f*scale} :
            tomato ? Vector3{.65f*scale,.85f*scale,.65f*scale} : Vector3{.65f*scale,.75f*scale,.65f*scale};
        pose.leafScale = tomato ? Vector3{.5f*scale,scale,.5f*scale} : pumpkin ?
            Vector3{.6f*scale,.85f*scale,.6f*scale} : Vector3{.4f*scale,.4f*scale,.4f*scale};
        pose.leafY = tomato || pumpkin ? pose.position.y : pose.position.y+pose.bodyScale.y*.96f;
        return pose;
    }
};
