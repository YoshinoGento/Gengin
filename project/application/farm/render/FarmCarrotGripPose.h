#pragma once
#include "farm/render/FarmMeshLayout.h"
#include "farm/system/FarmCarrotPull.h"

namespace farm {
inline constexpr size_t kGripParts=11; // Four splits and raised clods, palm, thumb and cuff.
template<class Emit> void VisitCarrotGrip(const FarmTileVisualData& bed, CarrotGrip grip,
                                         FarmCropMeshParts& crop, Emit emit) {
    if (!(grip.tension>0) || bed.crop!=CropType::Carrot || crop.count!=2) return;
    const float t=grip.tension;
    const float scale=std::clamp(bed.cropScale,.65f,1.65f);
    const Vector3 right{std::cos(grip.yaw),0,-std::sin(grip.yaw)};
    const Vector3 back{-std::sin(grip.yaw),0,-std::cos(grip.yaw)};
    const float lift=.037f*t*scale;
    for(size_t i=0;i<crop.count;++i) crop.parts[i].position.y+=lift;
    auto& leaves=crop.parts[1];
    leaves.scale.y*=1.f+.12f*t;
    leaves.scale.x*=1.f-.16f*t;
    leaves.scale.z*=1.f-.16f*t;
    const auto box=[&](Vector3 at,Vector3 half,Vector4 color,float yaw) {
        FarmMeshPart p{}; p.position=at; p.scale=half; p.color=color; p.yaw=yaw; emit(p);
    };
    constexpr std::array<float,4> bearings{.29f,1.77f,3.61f,5.12f};
    for (float a:bearings) {
        const Vector3 outward{std::cos(a),0,-std::sin(a)};
        auto ground=bed.center+outward*(scale*(.10f+.075f*t));
        ground.y+=kFarmSoilRidgeHeight+.003f;
        box(ground,{.095f*scale*t,.003f,.012f*t},{.16f,.094f,.052f,1},a);
        auto mound=ground+outward*(.043f*scale*t);
        const float thickness=.031f*scale*t;
        mound.y+=thickness;
        box(mound,{.045f*scale*t,thickness,.029f*scale*t},{.49f,.32f,.17f,1},a+.23f);
    }
    const Vector3 at=leaves.position+Vector3{0,.11f*scale,0}+right*(.054f+.22f*(1.f-t))+back*(.06f+.18f*(1.f-t));
    box(at,{.060f,.054f,.074f},{.78f,.63f,.40f,1},grip.yaw);
    box(at-right*.064f-back*.03f,{.030f,.040f,.040f},{.91f,.75f,.50f,1},grip.yaw);
    box(at+right*.055f+back*.095f,{.070f,.057f,.078f},{.24f,.39f,.32f,1},grip.yaw+.4f);
}
}
