#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/core/FarmCanalSite.h"
#include "math/Struct.h"
#include <cmath>
#include <cstdlib>

namespace farm::pond {
// New rows are appended at the back of the board. The intake stays on the east bank.
inline constexpr int firstRow = 1, lastRow = 3, bankLevel = 0;
inline constexpr int starterWidth = 5, starterHeight = 4, practiceRow = 2;
inline constexpr float feederWidthInTiles=.45f;
inline bool IsBank(const FarmGrid& grid, int i) noexcept {
    return grid.HasPondBank() && grid.GetTile(i) && i % grid.GetWidth() == grid.GetWidth()-1 &&
        i / grid.GetWidth() >= firstRow && i / grid.GetWidth() <= lastRow;
}
inline bool IsOpen(const FarmGrid& grid, int i) noexcept {
    if (!IsBank(grid,i)) return false;
    const auto& t = *grid.GetTile(i);
    return t.feature == FarmTileFeature::Canal && !t.retainsPondBank && t.heightLevel <= bankLevel;
}
inline Vector3 FeederCenter(Vector3 inlet,float pitch) noexcept { return {inlet.x+pitch,inlet.y,inlet.z}; }
// origin is the centre of tile zero. Only the narrow strip touching the three banks is interactive.
inline int FeederTile(const FarmGrid& grid,Vector3 origin,float pitch,Vector3 point) noexcept {
    if(!grid.HasPondBank() || !(pitch>0)) return -1;
    const float offset=(point.x-origin.x)/pitch-(grid.GetWidth()-.5f);
    const float row=(point.z-origin.z)/pitch+.5f;
    if(!(offset>=0 && offset<=feederWidthInTiles && row>=firstRow && row<lastRow+1)) return -1;
    const int i=static_cast<int>(row)*grid.GetWidth()+grid.GetWidth()-1;
    return IsBank(grid,i)?i:-1;
}
inline int BankCrossing(const FarmGrid& grid,Vector3 origin,float pitch,Vector3 a,Vector3 b) noexcept {
    const float edge=origin.x+(grid.GetWidth()-.5f)*pitch;
    if(!(pitch>0) || a.x==b.x) return -1;
    const float t=(edge-a.x)/(b.x-a.x);
    if(!(t>=0 && t<=1)) return -1;
    const float row=(std::lerp(a.z,b.z,t)-origin.z)/pitch+.5f;
    if(!(row>=firstRow && row<lastRow+1)) return -1;
    const int i=static_cast<int>(row)*grid.GetWidth()+grid.GetWidth()-1;
    return IsBank(grid,i)?i:-1;
}
inline bool PrepareStarter(FarmGrid::Snapshot& board) {
    if (board.width != starterWidth || board.height != starterHeight ||
        board.tiles.size() != starterWidth*starterHeight) return false;
    board.pondBank = true;
    board.selectedX = starterWidth-1;
    board.selectedY = practiceRow;
    board.tiles[practiceRow*starterWidth + starterWidth-2].feature = FarmTileFeature::Canal;
    const auto plant = [&](int x,int y,CropType crop) {
        auto& tile = board.tiles[y*starterWidth+x];
        tile.state = FarmTileState::Planted; tile.crop = crop;
        tile.growth = 0.42f; tile.moisture = 0.12f;
    };
    plant(starterWidth-3,practiceRow,CropType::Carrot);
    plant(starterWidth-2,0,CropType::Tomato);
    return true;
}
inline bool Start(FarmGrid& grid) {
    FarmGrid::Snapshot board{};
    board.width = starterWidth; board.height = starterHeight;
    board.tiles.resize(starterWidth*starterHeight);
    return PrepareStarter(board) && grid.ReplaceSnapshot(board);
}

struct Stroke {
    int last = -1;
    uint64_t generation = 0;
    bool keyboard = false, engaged = false, inFeeder = false;
    Vector3 start{}, tip{};
    bool Begin(const FarmGrid& grid,int index,Vector3 point,bool keys) {
        const auto* tile=grid.GetTile(index);
        if (!grid.HasPondBank() || !tile || !(tile->feature==FarmTileFeature::Canal ||
            (IsBank(grid,index) && CanStartCanal(*tile)))) return false;
        last=index; generation=grid.GetGeneration(); keyboard=keys; engaged=inFeeder=false; start=tip=point;
        return true;
    }
    template<class Dig>
    bool Pull(const FarmGrid& grid,int target,Vector3 point,float pitch,Dig dig,int crossedBank=-1) {
        if (last<0 || generation!=grid.GetGeneration() || !grid.GetTile(target)) return false;
        tip=point;
        const float dx=tip.x-start.x, dz=tip.z-start.z;
        constexpr float bite=.24f;
        if (!engaged && dx*dx+dz*dz < bite*bite*pitch*pitch) return true;
        const bool firstBite=!engaged;
        engaged=true;
        const int width=grid.GetWidth();
        const int sx=last%width, sy=last/width;
        const int tx=target%width, ty=target/width;
        const int nx=std::abs(tx-sx), ny=std::abs(ty-sy);
        int ix=0, iy=0;
        // Cross cell centers in stroke order, including sparse mouse samples; crops stop the blade.
        bool enteredAcross=false;
        for (;;) {
            const bool exitsAcross=ix<nx && (iy==ny || (2*ix+1)*ny <= (2*iy+1)*nx);
            const bool across=last==crossedBank || enteredAcross || exitsAcross ||
                (firstBite && !nx && !ny && std::abs(dx)>=bite*pitch && std::abs(dx)>std::abs(dz));
            const auto& tile=*grid.GetTile(last);
            if ((tile.feature!=FarmTileFeature::Canal || (tile.retainsPondBank && across)) &&
                !dig(last,across)) return false;
            if (ix==nx && iy==ny) return true;
            enteredAcross=exitsAcross;
            if (exitsAcross) { last+=tx>sx?1:-1; ++ix; }
            else { last+=ty>sy?width:-width; ++iy; }
        }
    }
};
}
