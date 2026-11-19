#pragma once
#include "farm/render/FarmRainGeometry.h"
#include "farm/system/FarmMoldVisualSystem.h"

namespace farm::mold {
using rain::Vertex;
inline constexpr int kPuffs = 7;
inline constexpr std::array<Vector3,6> kRim{{{1,0,0},{.5f,.866f,0},{-.5f,.866f,0},
    {-1,0,0},{-.5f,-.866f,0},{.5f,-.866f,0}}};
// Narrow lobes lie either in the furrow or on the ridge top, never suspended over a furrow.
struct Lobe { float x,z,rx,rz; bool ridge; };
inline constexpr std::array<Lobe,7> kMat{{
    {-.26f,-.13f,.13f,.26f,false},{-.25f,.12f,.11f,.21f,false},
    {.25f,-.19f,.12f,.22f,false},{.26f,.11f,.13f,.25f,false},
    {0,.27f,.115f,.24f,true},{-.51f,-.04f,.10f,.17f,true},{.51f,.18f,.09f,.15f,true}}};
inline constexpr std::size_t kFanVertices = kRim.size()*3;
inline constexpr std::size_t kVertexCount = FarmMoldFrame::kBeds*(kMat.size()+kPuffs)*kFanVertices;

// Feather to transparent at the edges using vertex alpha; no texture or billboard descriptor.
inline void Fan(std::span<Vertex> out, std::size_t& n, Vector3 center, Vector3 u, Vector3 v, float opacity) noexcept {
    const Vector4 ivory{.94f,.93f,.85f,opacity};
    const auto edge = [&](const Vector3& r) -> Vertex {
        return {{center.x+u.x*r.x+v.x*r.y,center.y+u.y*r.x+v.y*r.y,center.z+u.z*r.x+v.z*r.y},
            {.89f,.89f,.83f,0}};
    };
    for (std::size_t i=0; i<kRim.size(); ++i) {
        out[n++]={center,ivory}; out[n++]=edge(kRim[i]); out[n++]=edge(kRim[(i+1)%kRim.size()]);
    }
}

inline std::size_t Build(std::span<Vertex> out, const FarmMoldFrame& frame, Vector3 right, Vector3 up) noexcept {
    if (frame.count > frame.beds.size() || !rain::Finite(right) || !rain::Finite(up)) return 0;
    std::size_t n = 0;
    for (std::size_t i=0; i<frame.count; ++i) {
        const auto& b = frame.beds[i];
        if (!rain::Finite(b.root) || !std::isfinite(b.unit) || b.unit <= 0 ||
            !std::isfinite(b.stain) || b.stain <= 0 || b.stain > 1) continue;
        const bool puff = b.puff >= 0 && b.puff < 1;
        if (out.size()-n < (kMat.size()+(puff?kPuffs:0))*kFanVertices) break;
        const float hand = b.index%2 ? 1.f : -1.f;
        constexpr float lift = .003f; // Avoid coplanar depth on soil and ridges.
        for (const auto& l : kMat)
            Fan(out,n,{b.root.x+l.x*b.unit,b.root.y+lift+(l.ridge?kFarmSoilRidgeHeight:0),b.root.z+hand*l.z*b.unit},
                {l.rx*b.unit,0,0},{0,0,l.rz*b.unit},b.stain*.93f);
        if (!puff) continue;
        for (unsigned j=0; j<kPuffs; ++j) {
            const unsigned seed = static_cast<unsigned>(b.index)*43u + j*5u;
            const float delay = rain::Hash(seed+1)*.14f;
            const float t = (b.puff-delay)/(1.f-delay);
            if (t < 0) continue;
            const float ease = t*(2.f-t);
            // Low slow lift, slight sideways drift and independently staggered arrival.
            const float angle = (static_cast<float>(j)+rain::Hash(seed+2)*.65f)*(6.2831853f/kPuffs);
            const float radius = b.unit*(.13f+(.21f+.20f*rain::Hash(seed+3))*ease);
            const float wobble = .12f*std::sin(t*4.f+j);
            const Vector3 center{b.root.x+std::cos(angle+wobble)*radius,
                b.root.y+kFarmSoilRidgeHeight+b.unit*(.035f+(.22f+.22f*rain::Hash(seed+4))*ease),
                b.root.z+std::sin(angle+wobble)*radius};
            const float size = b.unit*(.064f+.057f*ease);
            const float alpha = .9f*(std::min)(1.f,t/.13f)*(1.f-t*t);
            Fan(out,n,center,{right.x*size,right.y*size,right.z*size},
                {up.x*size*.87f,up.y*size*.87f,up.z*size*.87f},alpha);
        }
    }
    return n;
}
}
