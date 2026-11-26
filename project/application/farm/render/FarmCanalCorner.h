#pragma once
#include <array>
#include <cmath>

namespace farm {
// Four adjacent cells share one vertex. Each cell owns only its quarter, from
// its own plateau to the two edge midpoints and this common vertex.
template<class Emit>
inline void VisitFarmCanalCorner(float here,float acrossX,float acrossZ,float diagonal,
    bool positiveX,bool positiveZ,float run,Emit emit) noexcept {
    std::array<float,4> h{};
    const unsigned origin=(positiveX?0u:1u)|(positiveZ?0u:2u);
    h[origin]=here;
    h[origin^1]=(here+acrossX)*.5f;
    h[origin^2]=(here+acrossZ)*.5f;
    h[origin^3]=(here+diagonal+acrossX+acrossZ)*.25f;
    const float mid=(h[1]+h[2])*.5f;
    const bool split=std::abs(h[0]+h[3]-h[1]-h[2])>.000001f;
    emit(mid,(h[1]-h[0])/run,(h[2]-h[0])/run,split,false);
    if (split) emit(mid,(h[3]-h[2])/run,(h[3]-h[1])/run,true,true);
}
}
