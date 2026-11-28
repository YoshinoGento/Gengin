#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmOverviewCamera.h"
#include "farm/system/FarmHoePull.h"
#include <algorithm>
#include <cmath>

namespace farm {
struct FarmerPose {
    Vector3 feet{};
    float yaw{}, stride{};
    float bladeDepth{}; // 0 carried, 1 in the ground.
    FurrowMark furrow{};
};

// The footprint stays four-connected, even at corners or on a slow frame.
class FarmWalkSystem {
public:
    bool Enter(const FarmGrid& board, OverviewPose camera) noexcept {
        const int selected=board.GetSelectedIndex(), w=board.GetWidth();
        if (w<=0 || !board.GetTile(selected)) return false;
        int nearest=-1, best=board.GetTileCount();
        for (int i=0;i<board.GetTileCount();++i) {
            const int distance=std::abs(i%w-selected%w)+std::abs(i/w-selected/w);
            if (Passable(board,i) && distance<best) { nearest=i; best=distance; }
        }
        if (nearest<0) return false;
        *this={}; active_=true; generation_=board.GetGeneration(); overview_=camera;
        x_=float(nearest%w); z_=float(nearest/w); pose_.yaw=3.14159265f;
        return true;
    }
    bool Active() const noexcept { return active_; }
    bool SameBoard(const FarmGrid& grid) const noexcept { return grid.GetGeneration()==generation_; }
    OverviewPose Leave() noexcept { active_=false; return overview_; }
    void Disarm() noexcept { armed_=false; pull_.Reset(); pose_.bladeDepth=0; pose_.furrow={}; }
    int Tile(const FarmGrid& grid) const noexcept {
        return int(std::floor(z_+.5f))*grid.GetWidth()+int(std::floor(x_+.5f));
    }
    float X() const noexcept { return x_; }
    float Z() const noexcept { return z_; }
    const FarmerPose& Pose() const noexcept { return pose_; }
    bool LastCutAcrossBank() const noexcept { return pull_.LastCutAcrossBank(); }
    void Ground(Vector3 feet) noexcept { pose_.feet=feet; }
    // Movement is never locked by the tool. Only a completed pull can request a tile edit.
    int Step(const FarmGrid& grid,int dx,int dz,bool dig,float dt,int destination=-1) noexcept {
        if (!active_ || !SameBoard(grid) || !std::isfinite(dt)) return -1;
        if (!dig) { armed_=true; pull_.Reset(); }
        const bool pulling=dig && armed_;
        const float step=std::clamp(dt,0.f,.08f);
        constexpr float biteSeconds=.11f;
        const float timeToContact=(1.f-pose_.bladeDepth)*biteSeconds;
        pose_.bladeDepth=std::clamp(pose_.bladeDepth+(pulling?step:-step)/biteSeconds,0.f,1.f);
        float distance=step*(pulling?kPullCellsPerSecond:kCellsPerSecond);
        const float oldX=x_,oldZ=z_;
        if (!dx && !dz && grid.GetTile(destination)) {
            const float ax=float(destination%grid.GetWidth())-x_, az=float(destination/grid.GetWidth())-z_;
            if ((std::max)(std::abs(ax),std::abs(az))>.015f) {
                if (std::abs(ax)>=std::abs(az)) { dx=ax>0?1:-1; distance=(std::min)(distance,std::abs(ax)); }
                else { dz=az>0?1:-1; distance=(std::min)(distance,std::abs(az)); }
            }
        }
        dx=std::clamp(dx,-1,1); dz=dx?0:std::clamp(dz,-1,1);
        const float nextX=std::clamp(x_+dx*distance,0.f,float(grid.GetWidth()-1));
        const float nextZ=std::clamp(z_+dz*distance,0.f,float(grid.GetHeight()-1));
        const int next=int(std::floor(nextZ+.5f))*grid.GetWidth()+int(std::floor(nextX+.5f));
        if (Passable(grid,next)) {
            const float travel=std::abs(nextX-x_)+std::abs(nextZ-z_);
            if (travel>0) { pose_.yaw=std::atan2(float(dx),float(dz)); phase_+=travel*8.f; }
            pose_.stride=travel>0 ? std::sin(phase_) : 0.f;
            x_=nextX; z_=nextZ;
        } else pose_.stride=0.f;
        const int tile=Tile(grid);
        int hit=-1;
        if (pulling && pose_.bladeDepth>=1.f) {
            // Preserve free movement during lowering, but only count travel after contact.
            const float airborne=step>0.f?std::clamp(timeToContact/step,0.f,1.f):0.f;
            const float contactX=std::lerp(oldX,x_,airborne),contactZ=std::lerp(oldZ,z_,airborne);
            const int contactTile=int(std::floor(contactZ+.5f))*grid.GetWidth()+int(std::floor(contactX+.5f));
            float entryX=contactX,entryZ=contactZ;
            if (contactTile!=tile) {
                // Split the actual path at the boundary; no travel is credited to both cells.
                if (dx) entryX=float(contactTile%grid.GetWidth())+(dx>0?.5f:-.5f);
                else entryZ=float(contactTile/grid.GetWidth())+(dz>0?.5f:-.5f);
                hit=pull_.Trace(grid,contactTile,contactX,contactZ,entryX,entryZ);
            }
            const int endHit=pull_.Trace(grid,tile,entryX,entryZ,x_,z_);
            if (endHit>=0) hit=endHit;
        }
        pose_.furrow=pull_.Mark();
        return hit;
    }
private:
    static bool Passable(const FarmGrid& board,int i) noexcept {
        const auto* t=board.GetTile(i);
        return t && t->crop==CropType::None && !t->repellentPlant && t->feature!=FarmTileFeature::WaterSource;
    }
    static constexpr float kCellsPerSecond=2.2f;
    static constexpr float kPullCellsPerSecond=1.45f;
    bool active_{},armed_{};
    uint64_t generation_{};
    float x_{},z_{},phase_{};
    FarmHoePull pull_{};
    FarmerPose pose_{};
    OverviewPose overview_{};
};
}
