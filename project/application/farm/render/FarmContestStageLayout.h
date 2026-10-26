#pragma once
#include "farm/core/FarmTypes.h"
#include "math/Struct.h"

// Validated comparison values only; both specimens use one volume-to-linear scale.
struct FarmContestStageLayout {
    static constexpr float kWidth = 1280, kHeight = 720;
    static constexpr float kCameraDistance = 12, kFovY = .75f, kYaw = .35f;
    static constexpr float kSpecimenHeight = 1.55f;
    struct Pose {
        Vector3 position{}, bodyScale{}, leafScale{};
        float leafY = 0;
    };
    [[nodiscard]] static float UnitsPerPixel() noexcept {
        return 2*kCameraDistance*std::tan(kFovY*.5f)/kHeight;
    }
    [[nodiscard]] static Pose Specimen(farm::CropType crop, float multiplier, float largest, int side) noexcept {
        const float scale = std::cbrt(multiplier)*kSpecimenHeight/std::cbrt((std::max)(1.f,largest));
        const bool carrot = crop==farm::CropType::Carrot;
        const bool tomato = crop==farm::CropType::Tomato;
        const bool pumpkin = crop==farm::CropType::Pumpkin;
        Pose pose;
        pose.position = {((side==0 ? 210.f : 834.f)-kWidth*.5f)*UnitsPerPixel(), (kHeight*.5f-352)*UnitsPerPixel(), 0};
        pose.bodyScale = carrot ? Vector3{.30f*scale,scale,.30f*scale} :
            tomato ? Vector3{.65f*scale,.85f*scale,.65f*scale} : Vector3{.65f*scale,.75f*scale,.65f*scale};
        pose.leafScale = tomato ? Vector3{.5f*scale,scale,.5f*scale} : pumpkin ?
            Vector3{.6f*scale,.85f*scale,.6f*scale} : Vector3{.4f*scale,.4f*scale,.4f*scale};
        pose.leafY = tomato || pumpkin ? pose.position.y : pose.position.y+pose.bodyScale.y*.96f;
        return pose;
    }
};
