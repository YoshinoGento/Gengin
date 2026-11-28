#pragma once
#include "farm/render/FarmMeshLayout.h"

namespace farm {
// Two non-overlapping reaches of the pond wrap the peninsula. The near side stays on land.
inline constexpr size_t kFarmPondParts = 18;
template<class Emit>
void VisitFarmPond(const FarmGrid& grid, const FarmVisualSystem& visual, Emit emit) {
    if (!grid.HasPondBank() || grid.GetHeight() <= pond::lastRow) return;
    const auto& layout = visual.GetLayout();
    const float pitch = layout.tileSize + layout.tileGap;
    const auto origin = visual.GetTileCenter(grid,0);
    const float east = origin.x + (grid.GetWidth()-.5f)*pitch;
    const float surface = layout.center.y + layout.waterBottomOffset + layout.waterMaxDepth + layout.pondLipRise;
    // Coordinates are in tile pitches, relative to the eastern edge and the first row.
    const auto slab = [&](float x0,float z0,float x1,float z1,float h,Vector4 color,bool water,
                          FarmMeshShape shape,FarmMeshSurface material) {
        const float bottom = water ? surface-.055f : layout.center.y-.17f;
        const float top = surface+h;
        emit(FarmMeshPart{{east+(x0+x1)*.5f*pitch,(top+bottom)*.5f,origin.z+(z0+z1)*.5f*pitch},
            {(x1-x0)*.5f*pitch,(top-bottom)*.5f,(z1-z0)*.5f*pitch},color,water,{},shape,material});
    };
    const auto water = [&](float x0,float z0,float x1,float z1) {
        if(layout.pondLipRise>0) slab(x0,z0,x1,z1,-.056f,{.31f,.32f,.22f,1},false,
            FarmMeshShape::Box,FarmMeshSurface::Earth);
        slab(x0,z0,x1,z1,0.f,{.073f,.245f,.228f,1},true,FarmMeshShape::Box,FarmMeshSurface::None);
    };
    if(layout.pondLipRise>0) emit(FarmMeshPart{{layout.center.x,layout.center.y-.41f,layout.center.z},
        {25*pitch,.18f,25*pitch},{.24f,.34f,.20f,1},false,{},FarmMeshShape::Box,FarmMeshSurface::Grass});
    const float front = pond::firstRow-.5f, back = pond::lastRow+.5f;
    const float farEdge = grid.GetHeight()-.5f;
    constexpr float feeder=pond::feederWidthInTiles, shore=.73f;
    water(0,front,feeder,back);
    water(feeder,back-.65f,shore,back-.2f);
    water(shore,-.25f,3.4f,farEdge+1.7f);
    water(-grid.GetWidth()+.4f,farEdge,shore,farEdge+1.7f);
    if(layout.pondLipRise>0) slab(-.012f,front,.025f,back,-.018f,{.36f,.39f,.34f,1},false,
        FarmMeshShape::Box,FarmMeshSurface::CanalBank);
    const auto bank=[&](float x0,float z0,float x1,float z1,bool stone) {
        slab(x0,z0,x1,z1,stone?.13f:.10f,stone?Vector4{.43f,.45f,.41f,1}:Vector4{.34f,.40f,.19f,1},false,
            stone?FarmMeshShape::BankStone:FarmMeshShape::Box,stone?FarmMeshSurface::CanalBank:FarmMeshSurface::Grass);
    };
    bank(feeder,front,shore,back-.88f,false);
    bank(feeder,back-.88f,shore,back-.65f,true);
    bank(feeder,back-.2f,shore,back,true);
    bank(0,front-.12f,feeder,front,true);
    bank(0,back,feeder,back+.12f,true);
    for(int row=pond::firstRow;row<=pond::lastRow;++row) {
        const int i=row*grid.GetWidth()+grid.GetWidth()-1;
        if(!pond::IsOpen(grid,i)) {
            const float y=visual.GetTileCenter(grid,i).y;
            emit(FarmMeshPart{{east-.09f*pitch,y+.075f+layout.pondLipRise*.5f,origin.z+row*pitch},{.09f*pitch,.11f+layout.pondLipRise*.5f,.49f*pitch},
                {.47f,.32f,.16f,1},false,{},FarmMeshShape::SoilRidge,FarmMeshSurface::Earth});
        }
    }
}
}
