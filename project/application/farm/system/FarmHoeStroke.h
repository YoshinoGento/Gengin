#pragma once
#include "farm/core/FarmGrid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>

namespace farm::hoeing {
inline constexpr float kLift=.22f, kHang=.07f, kDown=.24f;
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

// One visible tool and one waiting stroke. Overflow skips presentation, never the tile edit.
class Strokes {
public:
    void Enqueue(Stroke stroke) noexcept {
        for(size_t i=0;i<count_;++i) if(queue_[i].tile==stroke.tile) return;
        if(count_<queue_.size()) queue_[count_++]=stroke;
    }
    const Stroke* Active() const noexcept { return count_?&queue_[0]:nullptr; }
    bool WaitingForHit(int tile) const noexcept {
        for(size_t i=0;i<count_;++i) if(queue_[i].tile==tile) return queue_[i].age<kHit;
        return false;
    }
    std::optional<Stroke> Advance(const FarmGrid& grid,float dt) noexcept {
        while(count_) {
            auto& s=queue_[0];
            const auto* tile=grid.GetTile(s.tile);
            if(tile && tile->state==FarmTileState::Tilled && tile->feature==FarmTileFeature::None) {
                const float before=s.age;
                s.age+=dt;
                if(before<kHit && s.age>=kHit) return s;
                if(s.age<kEnd) break;
                dt=s.age-kEnd;
            }
            queue_[0]=queue_[1]; --count_;
        }
        return {};
    }
private:
    std::array<Stroke,2> queue_{};
    size_t count_=0;
};
}
