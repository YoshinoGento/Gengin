#include "WaterTrialSystem.h"

namespace farm::trial {
bool System::Reset(bool both,SoilIntake intake,SoilPolicy soil,CanalTransport transport) {
    FarmGrid::Snapshot board{};
    board.width=width; board.height=height; board.pondBank=true;
    board.selectedX=gate%width; board.selectedY=gate/width;
    board.tiles.resize(width*height);
    for(int i:{nearBed,farBed}) if(i==nearBed || both) board.tiles[i].moisture=.25f;
    if(!grid_.ReplaceSnapshot(board)) return false;
    both_=both; picked_={}; stats_={}; flows_={}; readings_={}; lastDt_=0; lastPick_={};
    harvestLoss_=0; soil_=soil;
    water_.Initialize({},intake,transport); growth_.Initialize(); tools_.Initialize(); tools_.ClearHistory();
    economy_.Initialize();
    if(!economy_.AddSeed(CropType::Carrot,64)) return false;
    water_.Rebuild(grid_);
    return true;
}
bool System::Select(int i) {
    if(!grid_.GetTile(i)) return false;
    if(i!=grid_.GetSelectedIndex()) ++stats_.cursorSteps;
    return grid_.SetSelectedIndex(i);
}
void System::CountTime(Work work,double dt,bool moved) {
    if(work>=Work::Count || !std::isfinite(dt) || dt<=0) return;
    stats_.toolSeconds[static_cast<size_t>(work)]+=dt;
    if(moved) stats_.pointerMovingSeconds+=dt;
}
bool System::Act(int i,Work work) {
    if(!grid_.GetTile(i) || work>=Work::Count) return false;
    Select(i);
    bool ok=false;
    if(work==Work::Dig) {
        if(IsBed(i) || (pond::IsBank(grid_,i) && i!=gate)) return false;
        if(i==gate && pond::IsOpen(grid_,i)) {
            auto tile=*grid_.GetTile(i); tile.retainsPondBank=true;
            ok=grid_.SetTile(i,tile); // No removal of stored water on either side of the closure.
        } else ok=tools_.DigCanalAt(grid_,i,true);
        if(ok) { water_.Rebuild(grid_); flows_={}; readings_={}; lastDt_=0; }
    } else {
        if(!IsBed(i)) return false;
        constexpr std::array kinds{FarmTool::Hoe,FarmTool::Seed,FarmTool::Water,FarmTool::Harvest};
        Pick pick{};
        if(work==Work::Harvest) {
            const auto& tile=*grid_.GetTile(i); const auto& care=tile.careHistory;
            pick={tile.moisture,care.drySeconds,care.lowSeconds,care.goodSeconds,care.excessSeconds};
        }
        ok=tools_.ApplyToolDetailed(grid_,kinds[static_cast<size_t>(work)-1],CropType::Carrot,economy_).Succeeded();
        if(ok) readings_[i==nearBed?0:1]={};
        if(ok && both_ && work!=Work::Water && flows_[i==nearBed?farBed:nearBed].soilReceived>0)
            ++stats_.workWhileOtherBedFed;
        if(ok && work==Work::Harvest) {
            if(soil_!=SoilPolicy::HarvestCost) {
                auto preview=*grid_.GetTile(i);
                preview.moisture=pick.moisture;
                if(!grid_.SetTile(i,preview)) return false;
            }
            harvestLoss_+=pick.moisture-grid_.GetTile(i)->moisture;
            lastPick_=pick; ++picked_[i==nearBed?0:1]; ++stats_.harvests;
        }
    }
    tools_.ClearHistory();
    if(ok) ++stats_.actions[static_cast<size_t>(work)];
    return ok;
}
bool System::BeginReplay() {
    for(int i:{nearBed,farBed}) if(!Act(i,Work::Hoe) || !Act(i,Work::Seed)) return false;
    for(int i:trunk) if(!Act(i,Work::Dig)) return false;
    return true;
}
bool System::ApplyReplayTick(int tick) {
    if(tick==60 || tick==480) return Act(gate,Work::Dig);
    if(tick==240) return Act(fork,Work::Dig);
    return true;
}
void System::Step(float dt) {
    if(!std::isfinite(dt) || dt<=0) return;
    water_.UpdateWater(grid_,dt,1);
    const auto flow=water_.GetLastStep(grid_);
    const auto samples=water_.GetLastTileFlows(grid_);
    lastDt_=static_cast<float>(flow.simulatedSeconds);
    if(samples.size()==flows_.size()) std::copy(samples.begin(),samples.end(),flows_.begin());
    else flows_={};
    if(flow.valid) {
        stats_.elapsed+=flow.simulatedSeconds;
        stats_.input+=flow.sourceRefill; stats_.delivered+=flow.soilDelivered;
        stats_.worstBalance=(std::max)(stats_.worstBalance,std::abs(flow.balanceError));
    }
    // Sample either side of the growth update. Harvest and soil delivery are not crop use.
    constexpr std::array beds{nearBed,farBed};
    std::array<float,2> beforeGrowth{};
    std::array<FarmGrowthForecast,2> forecast{};
    readings_={};
    if(flow.valid && samples.size()==flows_.size() && lastDt_>0) for(size_t j=0;j<beds.size();++j) {
        const auto* tile=grid_.GetTile(beds[j]);
        if(!tile || !IsBed(beds[j])) continue;
        if(soil_==SoilPolicy::PassiveDrain && tile->state!=FarmTileState::Empty) {
            // Trial soil constants, independent of crop, harvest and gate. Drain only gravitational excess.
            constexpr float fieldCapacity=.60f, drainagePerSecond=.25f;
            if(tile->moisture>fieldCapacity) {
                auto next=*tile;
                next.moisture-=(tile->moisture-fieldCapacity)*(std::min)(1.f,drainagePerSecond*lastDt_);
                const float removed=tile->moisture-next.moisture;
                if(grid_.SetTile(beds[j],next)) stats_.drained+=removed;
                tile=grid_.GetTile(beds[j]);
            }
        }
        beforeGrowth[j]=tile->moisture;
        forecast[j]=growth_.Evaluate(*tile);
    }
    growth_.Update(grid_,dt,1);
    if(!flow.valid || samples.size()!=flows_.size() || lastDt_<=0) return;
    for(size_t j=0;j<beds.size();++j) {
        const int i=beds[j];
        const auto* tile=grid_.GetTile(i);
        if(!tile || !IsBed(i)) continue;
        auto& r=readings_[j];
        r.donor=flows_[i].soilDonor;
        if(const auto* donor=grid_.GetTile(r.donor)) r.donorStock=donor->waterAmount;
        r.inRate=flows_[i].soilReceived/lastDt_;
        r.usedRate=(std::max)(0.f,beforeGrowth[j]-tile->moisture)/lastDt_;
        // A change smaller than one moisture percentage point per simulated minute
        // is effectively flat at the resolution of the field labels.
        constexpr float flat=.01f/60.f;
        if(!forecast[j].growing) r.trend=BedTrend::Fallow;
        else if(r.inRate<=0) r.trend=BedTrend::NoDelivery;
        else if(tile->moisture>=forecast[j].goodMoistureMinimum) r.trend=BedTrend::Supplied;
        else if(r.inRate-r.usedRate < -flat) r.trend=BedTrend::Drying;
        else if(r.inRate-r.usedRate > flat) r.trend=BedTrend::Recovering;
        else r.trend=BedTrend::HoldingLow;
    }
}
}
