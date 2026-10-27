#pragma once
#include "farm/core/FarmTypes.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include "math/Struct.h"

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
