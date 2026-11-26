#pragma once
#include "farm/render/FarmMeshLayout.h"
#include "farm/system/FarmWalkSystem.h"
#include <optional>

namespace farm {
inline constexpr float kHoeHandSide=.206f, kWristLength=.285f;
inline constexpr float kPullArmPitch=.50f, kPullHoePitch=.16f;
inline std::array<FarmMeshPart,6> BuildFarmerModel(const FarmerPose& pose) noexcept {
    const float sy=std::sin(pose.yaw),cy=std::cos(pose.yaw);
    const float lift=.018f*std::abs(pose.stride);
    const auto part=[&](FarmMeshShape shape,float x,float y,float z,float pitch=0.f) {
        FarmMeshPart p{}; p.shape=shape; p.yaw=pose.yaw; p.pitch=pitch;
        p.position=pose.feet+Vector3{x*cy+z*sy,y,-x*sy+z*cy};
        return p;
    };
    const float t=pose.bladeDepth;
    const float planted=t*t*(3.f-2.f*t);
    const float arm=std::lerp(-.23f+pose.stride*.20f,kPullArmPitch,planted);
    const float hoe=std::lerp(-1.f,kPullHoePitch,planted);
    return {part(FarmMeshShape::FarmerBody,0,lift,0),
        part(FarmMeshShape::FarmerLeg,-.087f,.417f+lift,0,pose.stride*.49f),
        part(FarmMeshShape::FarmerLeg,.087f,.417f+lift,0,-pose.stride*.49f),
        part(FarmMeshShape::FarmerArm,-.206f,.747f+lift,0,-pose.stride*.39f),
        part(FarmMeshShape::FarmerArm,kHoeHandSide,.747f+lift,0,arm),
        part(FarmMeshShape::FarmerHoe,kHoeHandSide,.747f+lift-kWristLength*std::cos(arm),-kWristLength*std::sin(arm),hoe)};
}

// The incision and lifted edge are temporary. Neither changes irrigation or history.
inline std::optional<std::array<FarmMeshPart,2>> BuildFurrowMark(const FarmGrid& grid,
    const FarmVisualSystem& visual,const FarmerPose& pose) noexcept {
    const auto& mark=pose.furrow;
    const auto* tile=grid.GetTile(mark.tile);
    if (!tile || !CanStartCanal(*tile) || pose.bladeDepth<1.f) return {};
    const auto center=visual.GetTileCenter(grid,mark.tile);
    const auto& layout=visual.GetLayout();
    const float pitch=layout.tileSize+layout.tileGap;
    const float extent=layout.tileSize*.45f;
    const float sy=std::sin(pose.yaw), cy=std::cos(pose.yaw);
    // Edge midpoint of the game hoe in its local OBJ space (metres).
    const float reach=-kWristLength*std::sin(kPullArmPitch)-.515f*std::sin(kPullHoePitch)-.129f*std::cos(kPullHoePitch);
    const auto point=[&](float x,float z) {
        Vector3 p=center;
        p.x+=std::clamp((x-mark.tile%grid.GetWidth())*pitch+kHoeHandSide*cy+reach*sy,-extent,extent);
        p.z+=std::clamp((z-mark.tile/grid.GetWidth())*pitch-kHoeHandSide*sy+reach*cy,-extent,extent);
        (void)visual.TryGetSoilHeight(grid,p,p.y);
        return p;
    };
    const auto a=point(mark.startX,mark.startZ), b=point(mark.endX,mark.endZ);
    const float dx=b.x-a.x,dz=b.z-a.z;
    const float length=std::hypot(dx,dz);
    const float yaw=length>.001f?std::atan2(dx,dz):pose.yaw;
    FarmMeshPart cut{};
    cut.position=(a+b)*.5f;
    cut.position.y+=.012f;
    cut.scale={.050f,.009f,(std::max)(.045f,length*.5f)};
    cut.yaw=yaw;
    cut.slope.y=length>.001f?(b.y-a.y)/length:0.f;
    cut.color={.19f,.105f,.052f,1.f}; cut.surface=FarmMeshSurface::Earth;
    auto lip=cut;
    lip.position.x+=.062f*std::cos(yaw); lip.position.z-=.062f*std::sin(yaw);
    lip.position.y+=.011f;
    lip.scale.x=.027f; lip.scale.y=.020f;
    lip.color={.42f,.26f,.12f,1.f};
    return std::array{cut,lip};
}
}
