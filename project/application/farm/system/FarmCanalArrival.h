#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmPondBank.h"

namespace farm::arrival {
inline constexpr float kTravel=.37f, kWick=1.25f;
enum class Beat { Bank, Soil };
struct Reveal { int parent=-1; float fraction=1; int outlet=-1; bool fromPond=false; };

// The hydraulic step is instantaneous to the player. Only presentation waits for the upstream front.
class Timeline final {
    struct Cell {
        int parent=-1,outlet=-1;
        double start=-1, end=-1, drySince=-1, donorStamp=-1;
        bool wet=false, soil=false, fired=false, fromPond=false;
    };
    std::vector<Cell> cells_;
    double now_=0;
    bool busy_=false;
public:
    void Clear() noexcept { cells_.clear(); now_=0; busy_=false; }
    void Advance(float dt) noexcept { now_+=dt; }
    bool Playing(int i) const noexcept {
        return i>=0 && static_cast<size_t>(i)<cells_.size() && cells_[i].end>now_;
    }
    Reveal Surface(int i) const noexcept {
        if(i<0 || static_cast<size_t>(i)>=cells_.size()) return {};
        const auto& c=cells_[i];
        if(c.soil || c.start<0) return {};
        const float t=static_cast<float>((now_-c.start)/kTravel);
        return t>=1 ? Reveal{} : Reveal{c.parent,(std::max)(0.f,t),c.outlet,c.fromPond};
    }
    bool StillConnected(int index,const FarmGrid& grid,const FarmIrrigationSystem& water) const noexcept {
        if(index<0 || static_cast<size_t>(index)>=cells_.size()) return false;
        const auto& c=cells_[index];
        if(c.fromPond) return pond::IsOpen(grid,index) && water.IsSupplied(index);
        const auto* tile=grid.GetTile(index); const auto* donor=grid.GetTile(c.parent);
        if(!tile || !donor || donor->feature==FarmTileFeature::None) return false;
        return c.soil ? tile->feature==FarmTileFeature::None && tile->state!=FarmTileState::Empty &&
            tile->irrigationEnabled && water.IsSupplied(c.parent) :
            tile->feature==FarmTileFeature::Canal && water.GetUpstreamTileIndex(index)==c.parent;
    }
    void Observe(const FarmGrid& grid,const FarmIrrigationSystem& water,std::span<const FarmIrrigationTileFlow> flows,
        std::uint64_t previousRevision) {
        if (flows.empty()) { Clear(); return; }
        cells_.resize(flows.size());
        // Parent-first even when several fixed steps complete before a single render.
        for(int index:water.GetSupplyOrder()) {
            const int parent=water.GetUpstreamTileIndex(index);
            const bool fromPond=parent<0 && pond::IsOpen(grid,index);
            if(parent<0 && !fromPond) continue;
            const size_t i=static_cast<size_t>(index);
            auto& c=cells_[i];
            if(c.soil || parent!=c.parent || c.fromPond!=fromPond) { c={}; c.parent=parent; c.fromPond=fromPond; }
            c.outlet=-1;
            if(parent>=0 && water.GetDownstreamCanalCount(parent)==1) cells_[parent].outlet=index;
            const auto& flow=flows[i];
            const bool first=flow.firstInflowRevision>previousRevision;
            if(!first && grid.GetTile(index)->waterAmount==0 && flow.incoming<=0 && flow.sourceRefill<=0) {
                if(c.drySince<0) c.drySince=now_;
                if(now_-c.drySince>.8 && c.end<=now_) c.wet=false;
                continue;
            }
            c.drySince=-1;
            if(!c.wet && first) {
                c.start=fromPond ? now_ : (std::max)(now_,cells_[parent].end);
                c.end=c.start+kTravel; c.fired=false; busy_=true;
            }
            c.wet=true;
        }
        // Soil can receive in the very same step. Resolve it after all canal timestamps exist.
        for(size_t i=0;i<flows.size();++i) {
            auto& c=cells_[i]; const auto& flow=flows[i];
            const auto& tile=*grid.GetTile(static_cast<int>(i));
            if(!c.soil && (c.fromPond || c.parent>=0) && !StillConnected(static_cast<int>(i),grid,water)) c={};
            if(c.soil && (tile.feature!=FarmTileFeature::None || tile.state==FarmTileState::Empty || !tile.irrigationEnabled)) c={};
            const int donor=flow.soilDonor;
            if(flow.soilReceived<=0 || !Playing(donor)) continue;
            const auto stamp=cells_[donor].end;
            if(c.soil && c.parent==donor && c.donorStamp==stamp) continue;
            c={}; c.soil=true; c.parent=donor; c.donorStamp=stamp;
            c.start=stamp; c.end=c.start+kWick; busy_=true;
        }
    }
    template<class Emit> void Dispatch(const FarmGrid& grid,const FarmIrrigationSystem& water,Emit emit) {
        if(!busy_) return;
        busy_=false;
        for(size_t i=0;i<cells_.size();++i) {
            auto& c=cells_[i];
            if(c.start<0 || c.end<=now_) continue;
            const int index=static_cast<int>(i);
            if(!StillConnected(index,grid,water)) { c={}; continue; }
            busy_=true;
            if(!c.fired && now_>=c.start+(c.soil?0.:.65*kTravel)) {
                emit(c.soil?Beat::Soil:Beat::Bank,index,c.parent); c.fired=true;
            }
        }
    }
};
}
