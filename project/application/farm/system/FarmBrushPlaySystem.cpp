#include "farm/system/FarmBrushPlaySystem.h"
#include "farm/core/FarmGrid.h"
#include <algorithm>
#include <cmath>

namespace farm {
namespace {
constexpr float kSettle=.65f,kLeafInterval=.27f,kDustInterval=.085f;
}
float FarmBrushPlaySystem::Random() {
    rng_=1664525u*rng_+1013904223u;
    return static_cast<float>(rng_>>8)*(1.f/16777216.f);
}
void FarmBrushPlaySystem::Update(const FarmGrid& grid,float seconds,bool active) {
    if (!active || board_!=&grid || generation_!=grid.GetGeneration()) {
        board_=&grid; generation_=grid.GetGeneration(); count_=0; touches_={};
        screen_.reset(); soilDelay_=leafDelay_=0;
    }
    active_=active && std::isfinite(seconds) && seconds>0;
    if (!active_) return;
    const float dt=(std::min)(seconds,.1f);
    soilDelay_=(std::max)(0.f,soilDelay_-dt); leafDelay_=(std::max)(0.f,leafDelay_-dt);
    for (auto& s:touches_) if (s.tile>=0) {
        const auto* tile=grid.GetTile(s.tile);
        s.age+=dt;
        if (!tile || tile->crop!=s.crop || s.age>=kSettle) s={};
    }
    for (size_t i=0;i<count_;) {
        auto& p=particles_[i]; p.age+=dt;
        if (p.age>=p.life) { p=particles_[--count_]; continue; }
        p.angle+=dt*p.spin;
        if (p.kind==FieldGrain::Leaf) {
            const float curl=.34f*std::sin(2*p.angle);
            p.velocity.y-=2.3f*dt;
            p.position.x+=curl*dt; p.position.z+=.15f*std::cos(p.angle)*dt;
        } else {
            p.velocity.y*=1.f-dt;
        }
        p.position.x+=p.velocity.x*dt; p.position.y+=p.velocity.y*dt; p.position.z+=p.velocity.z*dt;
        if (p.position.y<p.floor) { p.position.y=p.floor; p.age=(std::max)(p.age,p.life-.09f); }
        ++i;
    }
}
std::optional<Vector2> FarmBrushPlaySystem::MovePointer(std::optional<Vector2> screen) {
    if (!active_ || !screen || !std::isfinite(screen->x) || !std::isfinite(screen->y)) { screen_.reset(); return {}; }
    if (!screen_) { screen_=screen; return {}; }
    Vector2 d{screen->x-screen_->x,screen->y-screen_->y};
    if (std::hypot(d.x,d.y)<1.f) return {};
    screen_=screen; return d;
}
Vector2 FarmBrushPlaySystem::Lean(int tile,CropType crop) const {
    for(const auto& s:touches_) if(s.tile>=0 && s.tile==tile && s.crop==crop) {
        const float t=s.age/kSettle;
        const float tilt=.36f*std::sin(9.f*t)*(1-t)*(1-t);
        return {s.direction.x*tilt,s.direction.y*tilt};
    }
    return {};
}
bool FarmBrushPlaySystem::Stroke(const FarmGrid& grid,FarmBrushContact hit,Vector2 dir) {
    const auto* tile=grid.GetTile(hit.tile);
    const float n=std::hypot(dir.x,dir.y);
    if (!active_ || &grid!=board_ || !tile || tile->feature!=FarmTileFeature::None ||
        tile->state==FarmTileState::Empty || !(n>.0001f) || !std::isfinite(n) ||
        !std::isfinite(hit.position.x) || !std::isfinite(hit.position.y) || !std::isfinite(hit.position.z)) return false;
    const auto before=count_;
    dir={dir.x/n,dir.y/n};
    auto p=hit.position;
    if (hit.onCrop) {
        if (!IsPlantableCrop(tile->crop)) return false;
        auto* next=&touches_[0];
        for(auto& s:touches_) {
            if(s.tile==hit.tile) { if(s.age<kLeafInterval) return false; next=&s; break; }
            if(s.tile<0 || (next->tile>=0 && s.age>next->age)) next=&s;
        }
        *next={hit.tile,tile->crop,dir,0};
        if (leafDelay_>0 || count_==kCapacity) return false;
        leafDelay_=kLeafInterval;
        auto& leaf=particles_[count_++]; leaf={};
        leaf.kind=FieldGrain::Leaf; leaf.position={p.x,p.y+.045f,p.z};
        leaf.velocity={dir.x*.64f,1.26f+.25f*Random(),dir.y*.64f};
        leaf.radius=.073f+.019f*Random(); leaf.life=.93f+.14f*Random();
        leaf.angle=6.28f*Random(); leaf.spin=3.5f+2.f*Random(); leaf.tone=Random(); leaf.floor=p.y-.30f;
        leaf.flutter=1.25f;
    } else if (soilDelay_<=0) {
        soilDelay_=kDustInterval;
        const float wet=std::clamp(tile->moisture,0.f,1.f);
        const int grains=wet>.65f ? 1 : 2;
        for (int i=0;i<grains && count_<kCapacity;++i) {
            const float side=(Random()-.5f)*.12f;
            auto& dust=particles_[count_++]; dust={}; dust.kind=FieldGrain::Dust;
            dust.position={p.x-dir.y*side,p.y+.052f,p.z+dir.x*side};
            dust.velocity={dir.x*.18f+.08f,.32f+.22f*Random(),dir.y*.18f};
            dust.radius=(.095f+.042f*Random())*(1-.40f*wet);
            dust.life=.38f+.15f*Random(); dust.angle=6.28f*Random(); dust.spin=1.f; dust.floor=p.y;
        }
    }
    return count_!=before;
}
}
