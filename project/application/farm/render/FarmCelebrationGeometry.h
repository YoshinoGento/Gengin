#pragma once
#include "farm/system/FarmCelebrationSystem.h"
#include <span>

namespace farm::celebration {
struct Vertex { Vector3 position{}; Vector4 color{}; };
static_assert(sizeof(Vertex)==28);
inline constexpr std::size_t kVerticesPerPiece=12;
inline constexpr std::size_t kVertexCount=kParticleCount*kVerticesPerPiece;
inline Vector3 Rotate(Vector3 p, Vector3 r) noexcept {
    const float cx=std::cos(r.x),sx=std::sin(r.x),cy=std::cos(r.y),sy=std::sin(r.y),cz=std::cos(r.z),sz=std::sin(r.z);
    const Vector3 x{p.x,cx*p.y-sx*p.z,sx*p.y+cx*p.z};
    const Vector3 y{cy*x.x+sy*x.z,x.y,-sy*x.x+cy*x.z};
    return {cz*y.x-sz*y.y,sz*y.x+cz*y.y,y.z};
}
inline bool Finite(Vector3 v) noexcept { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
// Fixed scratch storage and vertex capacity. Sort by view depth, not camera-facing orientation.
inline std::size_t Build(std::span<Vertex> output, bool active, float seconds, Vector3 camera, Vector3 forward) noexcept {
    if(!active || output.size()<kVertexCount || !Finite(camera) || !Finite(forward) ||
        !std::isfinite(seconds) || seconds<0 || seconds>=kEndSeconds) return 0;
    struct Ordered { Piece piece; float depth; };
    std::array<Ordered,kParticleCount> pieces{};
    std::size_t live=0;
    for(std::size_t i=0;i<kParticleCount;++i) {
        const auto p=Sample(i,seconds);
        if(p.size<=0) continue;
        const auto fold=Rotate({0,0,p.gold?0.f:p.size*kPetalFoldDepth/3.f},p.rotation);
        const float depth=(p.position.x+fold.x-camera.x)*forward.x+
            (p.position.y+fold.y-camera.y)*forward.y+(p.position.z+fold.z-camera.z)*forward.z;
        pieces[live++]={p,depth};
    }
    std::sort(pieces.begin(),pieces.begin()+live,[](const Ordered& a,const Ordered& b){return a.depth>b.depth;});
    std::size_t count=0;
    for(std::size_t i=0;i<live;++i) {
        const auto& p=pieces[i].piece;
        const float width=p.gold?.55f:.72f;
        const std::array<Vector3,5> shape{{{0,0,p.gold?0.f:kPetalFoldDepth},{-width,0,0},{0,1,0},{width,0,0},{0,-1,0}}};
        for(int face=0;face<4;++face) {
            Vector4 color=p.color;
            const float shade=.82f+.06f*static_cast<float>(face);
            color.x*=shade; color.y*=shade; color.z*=shade;
            for(int corner : {0,face+1,(face+1)%4+1}) {
                const auto v=Rotate(shape[corner],p.rotation);
                output[count++]={{p.position.x+v.x*p.size,p.position.y+v.y*p.size,p.position.z+v.z*p.size},color};
            }
        }
    }
    return count;
}
}
