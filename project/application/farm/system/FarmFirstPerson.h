#pragma once
#include "farm/system/FarmOverviewCamera.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace farm {
// All look angles are radians; x/z and walking speed are in tile pitches.
class FirstPerson {
public:
    static constexpr float eyeHeight=1.20f, reach=3.5f;
    void Enter(int width,int height,OverviewPose previous) noexcept {
        saved=previous; active=width>0 && height>0;
        x=(width-1)*.5f; z=-.65f; yaw=0; pitch=.53f; target=-1;
    }
    void Step(float side,float forward,float mouseX,float mouseY,float dt,int width,int height) noexcept {
        if(!active || width<1 || height<1 || !std::isfinite(dt) ||
            !std::isfinite(mouseX) || !std::isfinite(mouseY)) return;
        constexpr float sensitivity=.0024f, speed=2.15f, margin=.74f;
        yaw=std::remainder(yaw+mouseX*sensitivity,2*std::numbers::pi_v<float>);
        pitch=std::clamp(pitch+mouseY*sensitivity,-1.15f,1.29f);
        const float step=speed*std::clamp(dt,0.f,.08f)/(std::max)(1.f,std::hypot(side,forward));
        x=std::clamp(x+(std::cos(yaw)*side+std::sin(yaw)*forward)*step,-margin,width-1+margin);
        z=std::clamp(z+(std::cos(yaw)*forward-std::sin(yaw)*side)*step,-margin,height-1+margin);
    }
    Vector3 Direction() const noexcept {
        return {std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch)};
    }
    OverviewPose saved{};
    float x{},z{},yaw{},pitch{};
    int target=-1;
    bool active{};
};
}
