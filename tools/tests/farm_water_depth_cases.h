#pragma once
#include "farm_test_edit.h"
#include "farm/render/FarmMeshLayout.h"
#include <cassert>
#include <cmath>

namespace water_depth_cases {
// Compare the surfaces at the same screen sample, not two unrelated world points.
inline int D24(double eyeZ) {
    constexpr double nearZ=.1,farZ=1000,levels=16777215;
    return static_cast<int>(std::lround(levels*farZ/(farZ-nearZ)*(1.-nearZ/eyeZ)));
}

inline void CheckClearance(const farm::FarmTileMeshParts& mesh,const farm::FarmMeshPart& p,float u,float v) {
    const float c=std::cos(p.yaw),s=std::sin(p.yaw);
    const float lx=u*p.scale.x,lz=v*p.scale.z;
    const float x=p.position.x+c*lx+s*lz,z=p.position.z-s*lx+c*lz;
    const float water=p.position.y+p.scale.y+lx*p.slope.x+lz*p.slope.y;
    bool found=false;
    for (size_t k=0;k<mesh.count;++k) {
        const auto& bed=mesh.parts[k];
        if (bed.surface!=farm::FarmMeshSurface::CanalBed) continue;
        const float dx=x-bed.position.x,dz=z-bed.position.z;
        if (std::abs(dx)>bed.scale.x || std::abs(dz)>bed.scale.z) continue;
        const float side=dx/bed.scale.x+dz/bed.scale.z;
        if ((bed.shape==farm::FarmMeshShape::TriangleLower && side>0) ||
            (bed.shape==farm::FarmMeshShape::TriangleUpper && side<0)) continue;
        found=true;
        const float gap=water-(bed.position.y+bed.scale.y+dx*bed.slope.x+dz*bed.slope.y);
        assert(gap>0.f);
        // Intersect the center camera ray with the underlying bed plane. If it
        // recedes faster than the ray, it cannot fight this water sample.
        for (double pitch:{.35,.60,.70}) for (double along:{double(bed.slope.y),double(-bed.slope.x)}) {
            const double closing=std::sin(pitch)+along*std::cos(pitch);
            if (closing<=0.) continue;
            for (double distance:{6.,16.,40.}) {
                const double bedZ=distance+gap/closing;
                assert(D24(bedZ)-D24(distance)>=2);
            }
        }
    }
    assert(found);
}

inline void Run(const farm::FarmVisualSystem& visual) {
    farm::FarmGrid board;
    assert(board.Initialize(3,3));
    for (float fill:{0.f,.00000001f,.5f,1.f,0.f}) {
        assert(EditFarmTile(board,4,[&](auto& t){t.feature=farm::FarmTileFeature::WaterSource;t.waterAmount=fill;}));
        const auto v=visual.GetTileVisualData(board,4);
        assert(v.waterFill==fill && v.showWaterSurface==(fill>0.f));
        const float physicalDepth=visual.GetLayout().waterMaxDepth*fill;
        if (fill==0.f || fill>=.5f) {
            assert(std::abs(v.waterSurfaceCenter.y-(v.center.y+visual.GetLayout().waterBottomOffset+physicalDepth))<1.e-6f);
        }
        const auto mesh=farm::BuildFarmTileMeshParts(board,4,visual);
        int surfaces=0;
        for (size_t j=0;j<mesh.count;++j) if (mesh.parts[j].water) {
            ++surfaces; CheckClearance(mesh,mesh.parts[j],0.f,0.f);
        }
        assert(surfaces==(fill==0.f?0:1));
    }
    constexpr int around[]{0,1,2,3,5,6,7,8};
    constexpr float fills[]{1.e-8f,.00002f,.017f,.17f,1.f};
    for (int mask=0;mask<256;++mask) for (int variant=0;variant<5;++variant) {
        for (int i=0;i<9;++i) {
            bool canal=i==4;
            for (int b=0;b<8;++b) if (around[b]==i) canal=(mask&(1<<b))!=0;
            assert(EditFarmTile(board,i,[&](auto& tile){
                tile.feature=canal?farm::FarmTileFeature::Canal:farm::FarmTileFeature::None;
                tile.heightLevel=variant==0?0:(i*7+variant*i*i)%3;
                tile.waterAmount=canal?fills[(i+variant+1)%5]:0.f;
                if (canal && variant!=0 && (i+variant)%5==0 && i!=4) tile.waterAmount=0;
            }));
        }
        const auto mesh=farm::BuildFarmTileMeshParts(board,4,visual);
        assert(mesh.count>0 && mesh.count<mesh.parts.size());
        int wet=0;
        for (size_t j=0;j<mesh.count;++j) {
            const auto& p=mesh.parts[j];
            if (!p.water) continue;
            ++wet;
            if (p.shape==farm::FarmMeshShape::BendFillet) {
                const int ex=(mask&(1<<3))?-1:1;
                const int ez=(mask&(1<<1))?-3:3;
                // A dry fourth canal still has a shared triangular bed; only a
                // genuine three-cell bend can own a separate rounded water lip.
                assert(board.GetTile(4+ex+ez)->feature==farm::FarmTileFeature::None);
                CheckClearance(mesh,p,-.22f,-.22f);
            } else if (p.shape==farm::FarmMeshShape::BendWater) {
                CheckClearance(mesh,p,0,0);
            } else {
                for (float u:{-.95f,-.23f,.25f,.94f}) for(float v:{-.94f,-.25f,.23f,.95f}) {
                    if ((p.shape==farm::FarmMeshShape::TriangleLower && u+v>0) ||
                        (p.shape==farm::FarmMeshShape::TriangleUpper && u+v<0)) continue;
                    CheckClearance(mesh,p,u,v);
                }
            }
        }
        assert(wet>0);
    }
}
}
