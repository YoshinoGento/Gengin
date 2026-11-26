#pragma once
#include "farm/core/FarmGrid.h"
#include <algorithm>
#include <cmath>
#include <optional>

namespace farm::hoeing {
inline constexpr float kLift=.13f, kHang=.035f, kDown=.135f;
inline constexpr float kHit=kLift+kHang+kDown, kPlant=.105f, kPull=.38f, kEnd=kHit+kPlant+kPull;
struct Stroke {
    int tile=-1;
    Vector3 contact{};
    float scale{}, moisture{}, age{};
};
struct ToolPose {
    Vector3 grip{};
    float scale{}, roll{};
    Vector3 Edge() const noexcept {
        // Centre of the blade's cutting lip in the exported grip-local mesh.
        constexpr float x=-.385f, y=.137f-1.02f;
        return grip+Vector3{scale*(x*std::cos(roll)-y*std::sin(roll)),
            scale*(x*std::sin(roll)+y*std::cos(roll)),0};
    }
};
inline ToolPose Pose(const Stroke& s) noexcept {
    const auto ease=[](float t){t=std::clamp(t,0.f,1.f); return t*t*(3.f-2.f*t);};
    constexpr float ready=1.72f, raised=2.56f, struck=.74f;
    const float down=std::clamp((s.age-kLift-kHang)/kDown,0.f,1.f);
    const float pull=ease((s.age-kHit-kPlant)/kPull);
    const auto offset=ToolPose{{},s.scale,struck}.Edge();
    const float roll=ready+(raised-ready)*ease(s.age/kLift)+(struck-raised)*down*down+.66f*pull;
    const float scale=s.scale*ease(s.age/.105f)*(1.f-ease((s.age-kEnd+.13f)/.13f));
    return {s.contact-offset+Vector3{0,s.scale*(.08f*(1.f-down)+.12f*pull),0},scale,roll};
}

// A single physical hoe follows the latest edit; an interrupted swing settles its own soil.
class Strokes {
public:
    void Start(Stroke stroke) noexcept { active_=stroke; }
    const Stroke* Active() const noexcept { return active_?&*active_:nullptr; }
    bool WaitingForHit(int tile) const noexcept {
        return active_ && active_->tile==tile && active_->age<kHit;
    }
    std::optional<Stroke> Finish(const FarmGrid& grid) noexcept {
        auto previous=active_;
        active_.reset();
        if(!previous || previous->age>=kHit) return {};
        const auto* tile=grid.GetTile(previous->tile);
        if(!tile || tile->state==FarmTileState::Empty || tile->feature!=FarmTileFeature::None) return {};
        return previous;
    }
    std::optional<Stroke> Advance(const FarmGrid& grid,float dt) noexcept {
        if(!active_) return {};
        const auto* tile=grid.GetTile(active_->tile);
        if(tile && tile->state==FarmTileState::Tilled && tile->feature==FarmTileFeature::None) {
            const float before=active_->age;
            active_->age+=dt;
            if(before<kHit && active_->age>=kHit) return active_;
            if(active_->age<kEnd) return {};
        }
        active_.reset();
        return {};
    }
private:
    std::optional<Stroke> active_;
};
}
