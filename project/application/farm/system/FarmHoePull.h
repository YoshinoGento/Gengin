#pragma once
#include "farm/core/FarmCanalSite.h"
#include "farm/core/FarmGrid.h"
#include <cmath>

namespace farm {
struct FurrowMark {
    int tile=-1;
    float startX{},startZ{},endX{},endZ{}; // Grid coordinates, not metres.
};

class FarmHoePull {
public:
    void Reset() noexcept { mark_={}; completed_=-1; }
    const FurrowMark& Mark() const noexcept { return mark_; }

    // A partial furrow exists only during this press. Net displacement keeps
    // oscillating or pushing against an obstacle from completing a tile.
    int Trace(const FarmGrid& grid,int tile,float x0,float z0,float x1,float z1) noexcept {
        const auto* soil=grid.GetTile(tile);
        if (!soil || !CanStartCanal(*soil) || tile==completed_) { mark_={}; return -1; }
        if (mark_.tile!=tile) mark_={tile,x0,z0,x0,z0};
        mark_.endX=x1; mark_.endZ=z1;
        constexpr float pullLengthCells=.30f;
        const float dx=x1-mark_.startX,dz=z1-mark_.startZ;
        if (dx*dx+dz*dz<pullLengthCells*pullLengthCells) return -1;
        completed_=tile; mark_={};
        return tile;
    }
private:
    FurrowMark mark_{};
    int completed_=-1;
};
}
