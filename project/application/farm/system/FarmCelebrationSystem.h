#pragma once
#include "math/Struct.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Analytic, bounded visual state. No inventory, camera or GPU ownership.
namespace farm::celebration {
inline constexpr std::size_t kParticleCount=192;
inline constexpr float kEndSeconds=5.f;
inline constexpr float kCenterX=-3.3f;
inline constexpr float kTau=6.2831853f;
inline constexpr float kSpawnWindow=.35f;
inline constexpr float kFlightMin=3.15f, kFlightRange=.80f;
inline constexpr float kGravity=2.8f, kHorizontalDrag=.7f;
inline constexpr float kTerminalMin=1.2f, kTerminalRange=.45f;
inline constexpr float kLaunchHeight=1.2f, kDeckHeight=.08f;
inline constexpr float kAlignSeconds=.32f, kRestSeconds=.16f, kFadeSeconds=.36f;
inline constexpr float kFadeInSeconds=.09f;
inline constexpr float kFlutterX=.12f, kFlutterZ=.07f;
inline constexpr float kPetalFoldDepth=.20f, kSurfaceClearance=.008f;
inline constexpr float kLaunchHalfWidth=1.2f, kLaunchWidthRange=.6f, kLandingHalfWidth=2.6f;
inline constexpr std::size_t kForegroundStride=4;
struct FlightLane { float launchMin, launchRange, landingMin, landingRange; };
inline constexpr FlightLane kForeground{-2.4f,.5f,-2.5f,.6f}, kBackground{1.95f,.15f,1.96f,.1f};
static_assert(kSpawnWindow+kFlightMin+kFlightRange+kRestSeconds+kFadeSeconds<kEndSeconds);
struct Piece {
    Vector3 position{}, rotation{};
    Vector4 color{};
    float size=0;
    bool gold=false;
    bool landed=false;
};
inline float Random(uint32_t seed) noexcept {
    seed ^= seed>>16; seed *= 0x7feb352du; seed ^= seed>>15;
    seed *= 0x846ca68bu; seed ^= seed>>16;
    return static_cast<float>(seed&0xffffu)/65536.f;
}
inline Piece Sample(std::size_t index, float seconds) noexcept {
    Piece out;
    if(index>=kParticleCount || !std::isfinite(seconds) || seconds<0 || seconds>=kEndSeconds) return out;
    const auto seed=static_cast<uint32_t>(index)*17u+1u;
    const float age=seconds-Random(seed)*kSpawnWindow;
    const float flight=kFlightMin+Random(seed+1)*kFlightRange;
    if(age<0 || age>=flight+kRestSeconds+kFadeSeconds) return out;
    const float side=index%2 ? 1.f : -1.f;
    const float phase=Random(seed+2)*kTau;
    const float time=(std::min)(age,flight);
    const auto lane=(index/2)%kForegroundStride==0 ? kForeground : kBackground;
    out.gold=index%6==0;
    out.size=out.gold ? .055f+Random(seed+7)*.035f : .07f+Random(seed+7)*.045f;
    out.landed=age>=flight;
    // Clear front/rear corridors avoid the crop, name plaque and banner solids, not their screen projections.
    // One quarter of pieces cross the foreground; both sides share that budget.
    const float startX=kCenterX+side*(kLaunchHalfWidth+Random(seed+3)*kLaunchWidthRange);
    const float endX=kCenterX+(2.f*Random(seed+8)-1.f)*kLandingHalfWidth;
    const float startZ=lane.launchMin+Random(seed+5)*lane.launchRange;
    const float endZ=lane.landingMin+Random(seed+9)*lane.landingRange;
    const float travel=(1.f-std::exp(-kHorizontalDrag*time))/(1.f-std::exp(-kHorizontalDrag*flight));
    const float flutter=std::sin(kTau*.5f*time/flight);
    out.position.x=startX+(endX-startX)*travel+kFlutterX*flutter*std::sin(time*4.f+phase);
    out.position.z=startZ+(endZ-startZ)*travel+kFlutterZ*flutter*std::sin(time*3.2f+phase);
    // Solve launch velocity for ground contact at flight time under linear drag and gravity.
    // Lifetime then includes a grounded hold/fade, never an unrelated mid-air expiry.
    const float landingY=kDeckHeight+(out.gold ? 0.f : out.size*kPetalFoldDepth)+kSurfaceClearance;
    const float terminal=kTerminalMin+Random(seed+4)*kTerminalRange;
    const float verticalDrag=kGravity/terminal;
    const float lift=(terminal*flight-(kLaunchHeight-landingY))/(1.f-std::exp(-verticalDrag*flight));
    out.position.y=out.landed ? landingY : kLaunchHeight+lift*(1.f-std::exp(-verticalDrag*time))-terminal*time;
    const float spinX=2.f+Random(seed+6)*2.f, spinZ=side*2.2f;
    out.rotation={phase+time*spinX,phase*.7f+time*1.8f,phase+time*spinZ};
    const float settle=std::clamp((time-flight+kAlignSeconds)/kAlignSeconds,0.f,1.f);
    const float align=settle*settle*(3.f-2.f*settle);
    const float restX=phase+flight*spinX, restZ=phase+flight*spinZ;
    out.rotation.x+=align*(restX+std::remainder(kTau*.25f-restX,kTau)-out.rotation.x);
    out.rotation.z+=align*(restZ+std::remainder(-restZ,kTau)-out.rotation.z);
    constexpr std::array<Vector4,3> colors{{{1.f,.91f,.81f,1},{1.f,.43f,.58f,1},{1.f,.78f,.25f,1}}};
    out.color=out.gold ? Vector4{1.f,.68f,.12f,1} : colors[(index/2)%colors.size()];
    const float fade=std::clamp(age/kFadeInSeconds,0.f,1.f)*
        (1.f-std::clamp((age-flight-kRestSeconds)/kFadeSeconds,0.f,1.f));
    out.color.w=fade;
    if(fade<=0) out.size=0;
    return out;
}
}
