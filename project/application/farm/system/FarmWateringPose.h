#pragma once
#include "math/Struct.h"
#include <algorithm>
#include <cmath>

namespace farm::watering {
inline constexpr int kDrops=18, kPatches=3;
inline constexpr float kTip=.22f, kSpacing=.035f, kDropFlight=.35f, kBounce=.24f;
inline constexpr float kReleaseEnd=kTip+(kDrops-1)*kSpacing, kRetract=.25f;
inline constexpr float kFirstHit=kTip+kDropFlight, kSoaked=kReleaseEnd+kDropFlight+.65f;
inline float Smooth(float u) noexcept {
    u=std::clamp(u,0.f,1.f); return u*u*(3.f-2.f*u);
}
struct Pour {
    int tile=-1;
    Vector3 center{};
    float half{},age{},startMoisture{};
};
struct CanPose {
    Vector3 pivot{};
    float scale{},roll{};
    Vector3 Rose() const noexcept {
        // Local centre of the perforated rose, relative to the OBJ pivot in save_hand_can.
        constexpr float x=.604f,y=.220f;
        return pivot+Vector3{scale*(x*std::cos(roll)-y*std::sin(roll)),
            scale*(x*std::sin(roll)+y*std::cos(roll)),0};
    }
};
inline CanPose Pose(const Pour& p,float age) noexcept {
    const float tip=Smooth(age/kTip), back=Smooth((age-kReleaseEnd)/kRetract);
    const float sweep=Smooth((age-kTip)/(kReleaseEnd-kTip));
    const float h=p.half;
    return {p.center+Vector3{(-1.28f+.32f*sweep)*h,(1.67f-.19f*tip+.23f*back)*h,(-.55f+.28f*sweep)*h},
        h*1.84f*Smooth(age/.15f)*(1.f-back),-.16f-.97f*tip*(1.f-back)};
}
inline float Pending(const Pour& p,float current) noexcept {
    // Crops can drink during the shower. Blend toward what is still in the soil, not the original dose.
    return (std::max)(0.f,current-p.startMoisture)*(1.f-Smooth((p.age-kFirstHit)/(kSoaked-kFirstHit)));
}
}
