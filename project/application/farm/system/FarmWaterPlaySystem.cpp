#include "farm/system/FarmWaterPlaySystem.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/system/FarmIrrigationSystem.h"
#include <algorithm>
#include <numbers>

namespace farm {
namespace {
constexpr float pi=std::numbers::pi_v<float>;
constexpr float kLifetime=.55f, kLeafFall=.62f, kLeafLifetime=12.f, kLift=.018f;
float Distance(Vector3 a,Vector3 b) { return std::hypot(a.x-b.x,a.z-b.z); }
Vector3 Mix(Vector3 a,Vector3 b,float t) { return {std::lerp(a.x,b.x,t),std::lerp(a.y,b.y,t),std::lerp(a.z,b.z,t)}; }
// Normals of the six outer faces shared by bend_water.obj and bend_fillet.obj.
constexpr std::array<Vector2,6> kArcNormals{{{.991445f,.130526f},{.923880f,.382683f},{.793353f,.608761f},
    {.608761f,.793353f},{.382683f,.923880f},{.130526f,.991445f}}};
struct Patch {
    Vector3 center, half;
    Vector2 gradient;
    float c,s;
    FarmMeshShape shape;
    bool water;
    bool Covers(float x,float z) const {
        const float dx=x-center.x,dz=z-center.z;
        x=(dx*c-dz*s)/half.x; z=(dx*s+dz*c)/half.z;
        if (std::abs(x)>1.00001f || std::abs(z)>1.00001f) return false;
        if (shape==FarmMeshShape::BendWater) {
            for (auto n:kArcNormals) if ((x+.4f)*n.x+(z+.4f)*n.y>1.4f*.991445f) return false;
        } else if (shape==FarmMeshShape::BendFillet) {
            if (x>0 || z>0) return false;
            for (auto n:kArcNormals) if ((x+1)*n.x+(z+1)*n.y>=.991445f) return true;
            return false;
        }
        return true;
    }
    float Height(float x,float z) const { return center.y+half.y+gradient.x*(x-center.x)+gradient.y*(z-center.z); }
};

// A frame-local cache: water levels can change without changing board topology.
class Surface {
public:
    Surface(const FarmGrid& g,const FarmVisualSystem& v):grid(g),visual(v),origin(v.GetTileCenter(g,0)),
        pitch(v.GetLayout().tileSize+v.GetLayout().tileGap) {}
    std::optional<Vector3> Pick(const WaterPointer& p) {
        const auto& cell=Load(p.tile);
        float nearest=1.e20f;
        std::optional<Vector3> hit;
        for (int i=0;i<cell.count;++i) {
            const auto& f=cell.faces[i];
            const float denom=p.direction.y-f.gradient.x*p.direction.x-f.gradient.y*p.direction.z;
            if (std::abs(denom)<1.e-6f) continue;
            const float t=(f.Height(p.origin.x,p.origin.z)-p.origin.y)/denom;
            if (!(t>=0 && t<nearest)) continue;
            Vector3 pos{p.origin.x+t*p.direction.x,p.origin.y+t*p.direction.y,p.origin.z+t*p.direction.z};
            if (!f.Covers(pos.x,pos.z)) continue;
            nearest=t;
            hit=f.water ? std::optional{pos} : std::nullopt;
        }
        return hit;
    }
    bool Place(Vector3& p,float lift) {
        const float col=(p.x-origin.x)/pitch+.5f,row=(p.z-origin.z)/pitch+.5f;
        if (!(col>=0 && col<grid.GetWidth() && row>=0 && row<grid.GetHeight())) return false;
        const auto& cell=Load(static_cast<int>(row)*grid.GetWidth()+static_cast<int>(col));
        for (int i=0;i<cell.count;++i) {
            const auto& f=cell.faces[i];
            if (f.water && f.Covers(p.x,p.z)) { p.y=f.Height(p.x,p.z)+lift; return true; }
        }
        return false;
    }
private:
    struct Cell { int id=-1, count=0; std::array<Patch,32> faces; };
    const Cell& Load(int i) {
        for (const auto& cell:cache) if (cell.id==i) return cell;
        auto& cell=cache[replacement++%cache.size()]; cell.id=i; cell.count=0;
        const auto* tile=grid.GetTile(i);
        if (!tile || tile->feature==FarmTileFeature::None) return cell;
        const auto mesh=BuildFarmTileMeshParts(grid,i,visual);
        for (size_t k=0;k<mesh.count;++k) {
            const auto& p=mesh.parts[k];
            if (!p.water && p.surface!=FarmMeshSurface::CanalBank) continue;
            const float c=std::cos(p.yaw),s=std::sin(p.yaw);
            cell.faces[cell.count++]={p.position,p.scale,{p.slope.x*c+p.slope.y*s,-p.slope.x*s+p.slope.y*c},
                c,s,p.shape,p.water};
        }
        return cell;
    }
    const FarmGrid& grid; const FarmVisualSystem& visual;
    Vector3 origin; float pitch;
    std::array<Cell,5> cache; size_t replacement{};
};
}

void FarmWaterPlaySystem::Update(const FarmGrid& grid,const FarmVisualSystem& visual,const FarmIrrigationSystem& water,
    const std::optional<WaterPointer>& pointer,float dt,bool active) {
    count_=0; pointerRipple_=false;
    if (!active || binding_!=&grid || generation_!=grid.GetGeneration()) {
        binding_=&grid; generation_=grid.GetGeneration(); anchor_.reset(); rings_={}; leaf_={};
        leafWait_=cooldown_=stale_=0; measurement_=water.MeasurementRevision();
    }
    if (!active || !grid.GetTileCount() || !std::isfinite(dt) || dt<=0) return;
    dt=(std::min)(dt,.1f);
    const float pitch=visual.GetLayout().tileSize+visual.GetLayout().tileGap;
    if (!(pitch>0)) return;
    Surface surface(grid,visual);
    for (auto& r:rings_) r.life=(std::max)(0.f,r.life-dt);
    cooldown_=(std::max)(0.f,cooldown_-dt);
    auto emit=[&](Vector3 pos,float angle,bool wake) {
        rings_[slot_]={pos,angle,kLifetime,wake}; slot_=(slot_+1)%kRingCount;
    };
    auto hit=pointer && pointer->tile>=0 ? surface.Pick(*pointer) : std::nullopt;
    if (!hit) anchor_.reset();
    else {
        if (!anchor_ || Distance(*anchor_,*hit)>pitch*.85f) { anchor_=hit; travel_=0; }
        else {
            const float d=Distance(*anchor_,*hit);
            const float pixels=std::hypot(pointer->screen.x-screen_.x,pointer->screen.y-screen_.y);
            if (pixels>=.25f) {
                travel_+=d;
                if (cooldown_<=0 && travel_>pitch*.048f && d>.0001f) {
                    emit(*hit,std::atan2(hit->z-anchor_->z,hit->x-anchor_->x),d>pitch*dt*1.15f);
                    cooldown_=.14f; travel_=0;
                    pointerRipple_=true;
                }
            }
        }
        anchor_=hit; screen_=pointer->screen;
    }
    stale_=measurement_==water.MeasurementRevision() ? stale_+dt : 0;
    measurement_=water.MeasurementRevision();
    const auto flows=stale_<.25f ? water.GetLastTileFlows(grid) : std::span<const FarmIrrigationTileFlow>{};
    auto child=[&](int tile) {
        const int w=grid.GetWidth(), col=tile%w;
        int best=-1; float amount=0;
        for (int i:{col>0?tile-1:-1,col+1<w?tile+1:-1,tile-w,tile+w}) {
            if (i<0 || static_cast<size_t>(i)>=flows.size() || water.GetUpstreamTileIndex(i)!=tile) continue;
            if (flows[i].incoming>amount && visual.GetTileVisualData(grid,i).showWaterSurface) {
                amount=flows[i].incoming; best=i;
            }
        }
        return best;
    };
    auto center=[&](int i) { return visual.GetTileCenter(grid,i); };
    if (leaf_.tile<0 && (leafWait_-=dt)<=0) {
        int still=-1,moving=-1;
        for (int i=0;i<grid.GetTileCount();++i) {
            const auto* tile=grid.GetTile(i);
            if (tile->feature==FarmTileFeature::None || !visual.GetTileVisualData(grid,i).showWaterSurface) continue;
            if (still<0) still=i;
            if (child(i)>=0) { moving=i; break; }
        }
        leaf_.tile=moving>=0?moving:still;
        leafWait_=2.f;
    }
    if (leaf_.tile>=0) {
        auto& l=leaf_; const auto c=center(l.tile);
        const float lastAge=l.age; l.age+=dt;
        if (l.next<0 && (l.t==0 || l.t>=1)) {
            l.next=child(l.tile);
            if (l.next>=0 && l.t>0) { l.previous=-1; l.t=0; }
        }
        bool flowing=l.next>=0 ? static_cast<size_t>(l.next)<flows.size() &&
            water.GetUpstreamTileIndex(l.next)==l.tile && flows[l.next].incoming>0 :
            l.previous>=0 && static_cast<size_t>(l.tile)<flows.size() && flows[l.tile].incoming>0;
        const auto a=l.previous>=0?Mix(center(l.previous),c,.5f):c;
        const auto b=l.next>=0?Mix(c,center(l.next),.5f):c;
        const float length=Distance(a,c)+Distance(c,b);
        if (flowing && l.age>=kLeafFall && length>.001f) l.t=(std::min)(1.f,l.t+dt*.55f*pitch/length);
        const auto p=Mix(a,c,l.t), q=Mix(c,b,l.t);
        l.position=Mix(p,q,l.t);
        if (Distance(p,q)>.001f) l.heading=std::atan2(q.z-p.z,q.x-p.x);
        if (l.age>=kLeafLifetime || !surface.Place(l.position,kLift*1.4f)) { l={}; leafWait_=2.f; }
        else {
            if (lastAge<kLeafFall && l.age>=kLeafFall) emit(l.position,0,false);
            if (l.age<kLeafFall) l.position.y+=.32f*pitch*(1-l.age/kLeafFall);
            if (l.t>=1 && l.next>=0) { l.previous=l.tile; l.tile=l.next; l.next=-1; l.t=0; }
        }
    }
    auto triangle=[&](Vector3 a,Vector3 b,Vector3 c,Vector4 color) {
        vertices_[count_++]={a,color}; vertices_[count_++]={b,color}; vertices_[count_++]={c,color};
    };
    for (const auto& r:rings_) {
        if (r.life<=0) continue;
        const float t=1-r.life/kLifetime, radius=pitch*(.054f+.174f*t*(2-t));
        const float width=pitch*(.021f-.010f*t);
        const Vector4 tint{.76f,.97f,.96f,(1-t)*.84f};
        const float cs=std::cos(r.angle),sn=std::sin(r.angle);
        auto at=[&](int i,float rad) {
            if (r.wake) {
                const float u=2.f*i/kSides-1.f;
                const float x=radius*(.26f-1.42f*u*u)-(radius-rad)*1.45f, z=radius*u*.82f;
                return Vector3{r.center.x+cs*x-sn*z,0,r.center.z+sn*x+cs*z};
            }
            const float angle=2*pi*i/kSides;
            return Vector3{r.center.x+rad*std::cos(angle),0,r.center.z+rad*std::sin(angle)};
        };
        for (int i=0;i<kSides;++i) {
            auto a=at(i,radius),b=at(i+1,radius),c=at(i,radius-width),d=at(i+1,radius-width);
            if (!surface.Place(a,kLift) || !surface.Place(b,kLift) || !surface.Place(c,kLift) || !surface.Place(d,kLift)) continue;
            triangle(a,b,c,tint); triangle(c,b,d,tint);
        }
    }
    if (leaf_.tile>=0) {
        const auto& l=leaf_; const float angle=l.heading+.26f*std::sin(l.age*2.3f)+.25f;
        constexpr float leafScale=1.16f;
        const float cs=std::cos(angle)*pitch*leafScale,sn=std::sin(angle)*pitch*leafScale;
        auto point=[&](float x,float z,float y=0.f) {
            return Vector3{l.position.x+x*cs-z*sn,l.position.y+y*pitch,l.position.z+x*sn+z*cs};
        };
        const auto tip=point(.117f,0),base=point(-.069f,0),mid=point(.015f,0,.015f);
        const auto left=point(.025f,-.052f),right=point(-.006f,.050f);
        const float alpha=std::clamp(kLeafLifetime-l.age,0.f,1.f);
        const Vector4 gold{.97f,.63f,.13f,alpha},ochre{.68f,.34f,.055f,alpha};
        triangle(tip,mid,left,gold); triangle(left,mid,base,gold);
        triangle(tip,right,mid,ochre); triangle(mid,right,base,ochre);
        triangle(base,point(-.105f,-.008f),point(-.061f,.012f),ochre);
    }
}
}
