#pragma once
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmPondBank.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include <array>

namespace farm::trial {
enum class Work : std::uint8_t { Dig, Hoe, Seed, Water, Harvest, Count };
enum class SoilPolicy { HarvestCost, NoCost, PassiveDrain };
enum class BedTrend { NoSample, Fallow, NoDelivery, HoldingLow, Drying, Recovering, Supplied };
struct BedReading {
    BedTrend trend=BedTrend::NoSample;
    int donor=-1;
    float donorStock=0, inRate=0, usedRate=0;
};
inline constexpr int width=5, height=4, gate=14, nearBed=2, farBed=16;
inline constexpr std::array trunk{13,12,7};
inline constexpr int fork=17;
inline constexpr std::array denseExtension{0,1,3,5,6,8,10,11,15,18};
struct Metrics {
    std::array<unsigned, static_cast<size_t>(Work::Count)> actions{};
    std::array<double, static_cast<size_t>(Work::Count)> toolSeconds{};
    unsigned cursorSteps=0, harvests=0, workWhileOtherBedFed=0;
    double elapsed=0, input=0, delivered=0, drained=0, worstBalance=0, pointerMovingSeconds=0;
};
class System final {
public:
    bool Reset(bool bothBeds,SoilIntake intake=SoilIntake::PathStrength,SoilPolicy soil=SoilPolicy::HarvestCost,
        CanalTransport transport=CanalTransport::ParentTree);
    bool BeginReplay();
    bool ApplyReplayTick(int tick);
    bool Act(int i, Work work);
    bool Select(int i);
    void Step(float dt);
    void CountTime(Work work,double seconds,bool moved);
    bool IsBed(int i) const { return i==nearBed || (both_ && i==farBed); }
    bool Both() const { return both_; }
    const FarmGrid& Grid() const { return grid_; }
    const FarmIrrigationSystem& Water() const { return water_; }
    std::span<const FarmIrrigationTileFlow> Flows() const { return flows_; }
    float StepSeconds() const { return lastDt_; }
    [[nodiscard]] BedReading Inspect(int bed) const noexcept {
        return IsBed(bed)?readings_[bed==nearBed?0:1]:BedReading{};
    }
    const Metrics& Stats() const { return stats_; }
    double HarvestMoistureRemoved() const { return harvestLoss_; }
    int LastQuality() const { return economy_.GetLastHarvestQuality().score; }
    struct Pick { float moisture=0; double dry=0,low=0,good=0,excess=0; };
    const Pick& LastPick() const { return lastPick_; }
    int CompletedCrops() const { return both_?(std::min)(picked_[0],picked_[1]):picked_[0]; }
private:
    FarmGrid grid_;
    FarmIrrigationSystem water_;
    FarmGrowthSystem growth_;
    FarmToolActionSystem tools_;
    FarmEconomySystem economy_;
    Metrics stats_{};
    std::array<FarmIrrigationTileFlow,width*height> flows_{};
    std::array<BedReading,2> readings_{};
    float lastDt_=0;
    std::array<int,2> picked_{};
    Pick lastPick_{};
    bool both_=false;
    SoilPolicy soil_=SoilPolicy::HarvestCost;
    double harvestLoss_=0;
};
}
