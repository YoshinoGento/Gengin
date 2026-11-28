#include "farm/core/FarmGrid.h"
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmPondBank.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmWeatherSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using namespace farm;
constexpr float kStep = 1.0f / 60.0f;
constexpr int kTickLimit = 18000;
constexpr int kRainStartTick = 10 * 60;
constexpr int kRainEndTick = 30 * 60;

void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

struct Layout {
    const char* name;
    std::string_view cells;
    int soilHeightOffset = 0;
    int channelHeightOffset = 0;
};
// S = source, C = canal, F = crop. Fixed width five, height four or six.
constexpr std::array kLayouts{
    Layout{"short_two", "....." "..F.." ".SCF." "....."},
    Layout{"long_two", "CCCCF" "C...." "CF..." "S...."},
    Layout{"branch_five", "..F.." ".FCF." "FCCCF" "..S.."},
    Layout{"trunk_five", "..F.." ".FCF." ".FCF." "..S.."},
    Layout{"expanded_eight", "..C.." ".FCF." ".FCF." ".FCF." ".FCF." "..S.."},
    Layout{"expanded_two_sources", ".SC.." ".FCF." ".FCF." ".FCF." ".FCF." "..S.."},
    Layout{"separate_sources_two", ".SCF." "....." ".SCF." "....."},
    Layout{"uphill_two", "....." "..F.." ".SCF." ".....", 1, 0},
    Layout{"downhill_two", "....." "..F.." ".SCF." ".....", 0, 1},
};

enum class Policy { Open, ManagedTwo, ManagedFive, Manual, Closed };
enum class Weather { Clear, Pulse, Continuous };
constexpr std::array kWeathers{Weather::Clear, Weather::Pulse, Weather::Continuous};
constexpr std::array kPolicies{Policy::Open, Policy::ManagedTwo, Policy::ManagedFive,
    Policy::Manual, Policy::Closed};
const char* PolicyName(Policy policy) {
    switch (policy) {
    case Policy::Open: return "open";
    case Policy::ManagedTwo: return "managed_2s";
    case Policy::ManagedFive: return "managed_5s";
    case Policy::Manual: return "manual_2s";
    default: return "closed";
    }
}

struct Tuning { const char* name; float recovery; float efficiency; };
constexpr std::array kTunings{
    Tuning{"baseline", 0.125f, 0.90f},
    Tuning{"rate040_eff090", 0.040f, 0.90f},
    Tuning{"rate050_eff090", 0.050f, 0.90f},
    Tuning{"rate040_eff095", 0.040f, 0.95f},
    Tuning{"rate050_eff095", 0.050f, 0.95f},
    Tuning{"rate060_eff095", 0.060f, 0.95f},
};
constexpr std::array kCrops{CropType::Carrot, CropType::Tomato, CropType::Pumpkin};

struct Result {
    int fields = 0, sources = 0, canals = 0;
    int intakeChanges = 0, waterActions = 0;
    double readyMean = 0, readyLast = 0, goodMean = 0, excessMean = 0;
    double qualityMean = 0, delivered = 0, rainDelivered = 0;
    int qualityMinimum = 100;
};

Result Run(const Layout& layout, CropType crop, Policy policy, Weather weather, float speed,
    const FarmRules& rules, float initialMoisture = 0.5f, bool fixedFrame = false) {
    FarmGrid grid;
    Check(layout.cells.size() % 5 == 0 && grid.Initialize(5, static_cast<int>(layout.cells.size() / 5)), "layout dimensions");
    FarmIrrigationSystem water; water.Initialize(rules);
    FarmGrowthSystem growth; growth.Initialize(rules);
    FarmCropQualitySystem quality; quality.Initialize(rules);
    FarmToolActionSystem tools; tools.Initialize(rules);
    FarmEconomySystem economy; economy.Initialize(rules);
    const int ideal = crop == CropType::Carrot ? rules.carrotIdealHeight :
        crop == CropType::Tomato ? rules.tomatoIdealHeight : rules.pumpkinIdealHeight;
    std::vector<int> fields;
    std::vector<bool> harvested(layout.cells.size(), false);
    Result result;
    // Synthetic fixtures isolate irrigation, not seed prices, stage unlocks or a human playthrough.
    // Moisture is an explicit starting condition; nutrients are full, growth/stock start at zero.
    for (int i = 0; i < grid.GetTileCount(); ++i) {
        FarmTile tile;
        const auto cell = layout.cells[static_cast<std::size_t>(i)];
        tile.heightLevel = ideal + (cell == 'F' ? layout.soilHeightOffset : layout.channelHeightOffset);
        if (cell == 'S') { tile.feature = FarmTileFeature::WaterSource; ++result.sources; }
        else if (cell == 'C') { tile.feature = FarmTileFeature::Canal; ++result.canals; }
        else if (cell == 'F') {
            tile.state = FarmTileState::Planted; tile.crop = crop; tile.moisture = initialMoisture;
            tile.irrigationEnabled = policy != Policy::Manual && policy != Policy::Closed;
            fields.push_back(i);
        }
        Check(grid.SetTile(i, tile), "fixture tile");
    }
    result.fields = static_cast<int>(fields.size());
    Check(result.fields > 0, "missing fields");
    water.Rebuild(grid);
    for (int i = 0; i < grid.GetTileCount(); ++i) {
        if (grid.GetTile(i)->feature == FarmTileFeature::WaterSource)
            Check(water.GetDownstreamCanalCount(i) > 0, "unused source in layout");
    }
    for (int i : fields) Check(water.IsInIrrigationRange(i) == (layout.soilHeightOffset <= layout.channelHeightOffset),
        "unexpected field reachability");
    int completed = 0;
    const int inspectionTicks = (policy == Policy::ManagedFive ? 5 : 2) * 60;
    const int tickStride = fixedFrame ? static_cast<int>(speed) : 1;
    const float frameDelta = fixedFrame ? kStep : kStep / speed;
    for (int tick = 0; tick < kTickLimit && completed < result.fields; tick += tickStride) {
        if (tick % inspectionTicks == 0) {
            for (int i : fields) {
                if (harvested[static_cast<std::size_t>(i)]) continue;
                Check(grid.SelectTile(i), "select field");
                const auto forecast = growth.Evaluate(*grid.GetTile(i), crop);
                const float band = forecast.goodMoistureMaximum - forecast.goodMoistureMinimum;
                const float openAt = forecast.goodMoistureMinimum + band * 0.25f;
                const float closeAt = forecast.goodMoistureMaximum - band * 0.25f;
                if (policy == Policy::ManagedTwo || policy == Policy::ManagedFive) {
                    const auto* tile = grid.GetTile(i);
                    const bool wanted = tile->moisture <= openAt ? true :
                        tile->moisture >= closeAt ? false : tile->irrigationEnabled;
                    if (wanted != tile->irrigationEnabled) {
                        Check(tools.SetSelectedIrrigation(grid, wanted), "intake change");
                        ++result.intakeChanges;
                    }
                } else if (policy == Policy::Manual && grid.GetTile(i)->moisture <= openAt) {
                    for (int attempt = 0; attempt < 10 && grid.GetTile(i)->moisture < closeAt; ++attempt) {
                        Check(tools.ApplyTool(grid, FarmTool::Water, crop, economy), "manual watering");
                        ++result.waterActions;
                    }
                }
                tools.ClearHistory();
            }
        }
        // Same simulated tick at 1/2/4x separates numerical equivalence from human reaction speed.
        const bool raining = weather == Weather::Continuous ||
            (weather == Weather::Pulse && tick >= kRainStartTick && tick < kRainEndTick);
        const float rainRate = raining ?
            FarmWeatherSystem::kSoilRainPerSecond : 0.0f;
        water.UpdateWater(grid, frameDelta, speed, rainRate);
        const auto flow = water.GetLastStep(grid);
        Check(flow.valid && std::isfinite(flow.balanceError) && std::abs(flow.balanceError) < 0.0001,
            "water conservation");
        result.delivered += flow.soilDelivered;
        result.rainDelivered += flow.rainDelivered;
        growth.Update(grid, frameDelta, speed);
        for (int i : fields) {
            const auto* tile = grid.GetTile(i);
            Check(tile && std::isfinite(tile->moisture) && tile->moisture >= 0 && tile->moisture <= 1 &&
                std::isfinite(tile->growth) && tile->growth >= 0 && tile->growth <= 1, "finite bounds");
            if (harvested[static_cast<std::size_t>(i)] || !IsHarvestReady(*tile)) continue;
            const auto judged = quality.Evaluate(*tile);
            Check(judged.IsValid(), "quality result");
            const auto& h = tile->careHistory;
            const double total = h.drySeconds + h.lowSeconds + h.goodSeconds + h.excessSeconds;
            Check(total > 0, "care history duration");
            result.goodMean += 100.0 * h.goodSeconds / total;
            result.excessMean += 100.0 * h.excessSeconds / total;
            result.qualityMean += judged.score;
            result.qualityMinimum = (std::min)(result.qualityMinimum, judged.score);
            const double ready = (tick + tickStride) * static_cast<double>(kStep);
            result.readyMean += ready;
            result.readyLast = (std::max)(result.readyLast, ready);
            Check(grid.SelectTile(i), "harvest select");
            Check(tools.ApplyToolDetailed(grid, FarmTool::Harvest, crop, economy).Succeeded(), "harvest");
            if (grid.GetTile(i)->irrigationEnabled) Check(tools.SetSelectedIrrigation(grid, false), "stop harvested intake");
            tools.ClearHistory();
            harvested[static_cast<std::size_t>(i)] = true;
            ++completed;
        }
    }
    Check(completed == result.fields, "crop timeout");
    result.readyMean /= result.fields; result.goodMean /= result.fields;
    result.excessMean /= result.fields; result.qualityMean /= result.fields;
    return result;
}

void Print(const char* tuning, const Layout& layout, CropType crop, Policy policy, Weather weather,
    float speed, const Result& r) {
    std::cout << tuning << ',' << layout.name << ',' << ToString(crop) << ',' << PolicyName(policy) << ','
        << static_cast<int>(weather) << ',' << speed << ',' << r.fields << ',' << r.sources << ',' << r.canals << ','
        << r.readyMean << ',' << r.readyLast << ',' << r.goodMean << ',' << r.excessMean << ','
        << r.qualityMean << ',' << r.qualityMinimum << ',' << r.intakeChanges << ',' << r.waterActions << ','
        << r.delivered << ',' << r.rainDelivered << '\n';
}

void VerifyBalanceTargets(const Layout& layout, CropType crop, Policy policy, Weather weather, const Result& r) {
    if (layout.soilHeightOffset > layout.channelHeightOffset) return;
    if (policy == Policy::ManagedFive) {
        // Rain may briefly overshoot before the next inspection, but must not ruin the crop.
        const bool continuousRain = weather == Weather::Continuous;
        const double limit = crop == CropType::Carrot ? 16 : crop == CropType::Tomato ? 25 : continuousRain ? 42 : 41;
        const double minimumGood = continuousRain ? 75 : 90;
        Check(r.goodMean >= minimumGood && r.qualityMinimum >= 85 && r.readyLast <= limit,
            "five-second management target");
    }
    if (std::string_view(layout.name) == "short_two" && policy == Policy::Open && weather == Weather::Clear) {
        if (crop == CropType::Carrot) Check(r.readyLast <= 20 && r.qualityMinimum >= 85,
            "starter crop overwatering penalty");
        else if (crop == CropType::Tomato)
            Check(r.readyLast <= 55 && r.readyLast > 25 && r.excessMean > 0 && r.qualityMean < 90,
                "short open tomato should mature within 55s but still reward management");
        else Check(r.readyLast <= 80 && r.excessMean > 0, "short open pumpkin pacing");
    }
}

void VerifySharedRate() {
    for (bool initialize : {false, true}) for (bool custom : {false, true}) {
        if (!initialize && custom) continue;
        FarmRules rules;
        if (custom) rules.irrigationMoistureRecoveryPerSecond = 0.03f;
        for (float speed : {1.f, 2.f, 4.f}) {
            FarmGrid grid;
            Check(grid.Initialize(3, 1), "rate grid");
            FarmTile source; source.feature = FarmTileFeature::WaterSource; source.waterAmount = 1;
            FarmTile canal; canal.feature = FarmTileFeature::Canal; canal.waterAmount = 1;
            FarmTile soil; soil.state = FarmTileState::Planted; soil.crop = CropType::Tomato; soil.moisture = 0.5f;
            Check(grid.SetTile(0, source) && grid.SetTile(1, canal) && grid.SetTile(2, soil), "rate fixture");
            FarmIrrigationSystem water;
            if (initialize) water.Initialize(rules);
            FarmGrowthSystem growth; growth.Initialize(rules);
            water.Rebuild(grid);
            const auto forecast = growth.Evaluate(soil, soil.crop, speed, water.GetAvailableIrrigationStrength(grid, 2));
            const float expectedRate = rules.irrigationMoistureRecoveryPerSecond *
                rules.irrigationCanalTransferEfficiency * speed;
            Check(std::abs(forecast.irrigationRecoveryPerSecond - expectedRate) < 0.000001f,
                "forecast rate differs from rules");
            water.UpdateWater(grid, kStep, speed);
            const auto flow = water.GetLastStep(grid);
            Check(flow.valid && std::abs(flow.soilDelivered - expectedRate * kStep) < 0.000001,
                "runtime rate differs from rules");
            growth.Update(grid, kStep, speed);
            Check(std::abs(grid.GetTile(2)->moisture - (soil.moisture + forecast.netMoisturePerSecond * kStep)) < 0.000001f,
                "stock-sufficient forecast/runtime mismatch");
            for (float wet : {FarmCanalSeepage::kFullRateUntil, 0.80f, FarmCanalSeepage::kStopsAt, 0.99f}) {
                soil.moisture = wet;
                Check(grid.SetTile(2, soil), "wet fixture");
                const auto prediction = growth.Evaluate(soil, soil.crop, speed, water.GetAvailableIrrigationStrength(grid, 2));
                water.UpdateWater(grid, kStep, speed);
                Check(std::abs(water.GetLastStep(grid).soilDelivered - prediction.irrigationRecoveryPerSecond * kStep) < 0.000001 &&
                    prediction.secondsUntilFullMoisture < 0, "wet seepage forecast");
                if (wet > FarmCanalSeepage::kFullRateUntil && wet < FarmCanalSeepage::kStopsAt)
                    Check(prediction.irrigationRecoveryPerSecond > 0 && prediction.irrigationRecoveryPerSecond < expectedRate,
                        "wet soil must still absorb, but more slowly than dry soil");
                if (wet >= FarmCanalSeepage::kStopsAt)
                    Check(water.GetLastStep(grid).soilDelivered == 0, "saturated soil spent water");
            }
        }
    }
    FarmRules invalid;
    invalid.irrigationMoistureRecoveryPerSecond = std::numeric_limits<float>::quiet_NaN();
    FarmGrowthSystem growth; growth.Initialize(invalid);
    FarmTile tile; tile.state = FarmTileState::Planted; tile.crop = CropType::Tomato; tile.moisture = 0.5f;
    Check(growth.Evaluate(tile, tile.crop, 1, 1).irrigationRecoveryPerSecond == FarmRules{}.irrigationMoistureRecoveryPerSecond,
        "invalid forecast rate fallback");
}
}

static void PondBankChecks() {
    FarmGrid grid;
    Check(pond::Start(grid),"pond starter");
    FarmToolActionSystem tools; tools.Initialize();
    FarmIrrigationSystem water; water.Initialize();
    const int gate=grid.GetSelectedIndex(), fork=gate-1, carrot=fork-1, tomato=3;
    Check(pond::FeederTile(grid,{},1,{4.8f,0,1})==9 && pond::FeederTile(grid,{},1,{5.3f,0,2})==-1 &&
        pond::FeederTile(grid,{},1,{4.8f,0,0})==-1,"off-bank pointer accepted");
    const Vector3 inner{3,0,3}, outer{4.8f,0,1.1f};
    Check(pond::BankCrossing(grid,{},1,inner,outer)==9 && pond::BankCrossing(grid,{},1,outer,inner)==9 &&
        pond::BankCrossing(grid,{},1,{4.7f,0,2},{4.7f,0,3})==-1,"oblique or parallel shoreline intersection");
    const auto advance=[&] { for(int n=0;n<90;++n) water.UpdateWater(grid,kStep,1); };
    const float dry=grid.GetTile(carrot)->moisture;
    advance();
    Check(water.GetWaterSourceCount()==1 && grid.GetTile(fork)->waterAmount==0 &&
        grid.GetTile(carrot)->moisture==dry,"intact bank leaked");
    Check(!tools.CanToggleWaterSource(grid,0),"placed an interior source");
    pond::Stroke hoe;
    Check(hoe.Begin(grid,gate,{},false),"enter bank");
    auto dig=[&](int i,bool across=true){return tools.DigCanalAt(grid,i,across);};
    Check(hoe.Pull(grid,gate,{.01f,0,0},1,dig) && !pond::IsOpen(grid,gate),"click opened inlet");
    Check(hoe.Pull(grid,fork,{-1,0,0},1,dig) && pond::IsOpen(grid,gate),"first opening");
    water.UpdateWater(grid,kStep,1);
    const auto budget=water.GetLastStep(grid).sourceRefill;
    Check(budget>0,"missing external inflow");
    advance();
    Check(grid.GetTile(carrot)->moisture>dry && grid.GetTile(tomato)->moisture==dry,"first irrigation");
    Check(!hoe.Pull(grid,carrot-1,{-3,0,0},1,dig) && grid.GetTile(carrot)->crop==CropType::Carrot &&
        grid.GetTile(carrot-1)->feature==FarmTileFeature::None,"stroke passed through crop");
    Check(dig(gate-5),"second opening");
    water.UpdateWater(grid,kStep,1);
    Check(water.GetLastStep(grid).sourceRefill<=budget+.00001,"duplicated inlet capacity");
    Check(dig(fork-5),"second branch");
    advance(); Check(grid.GetTile(tomato)->moisture>dry,"tomato branch remained dry");
    FarmGrid::Snapshot snapshot; grid.CaptureSnapshot(snapshot);
    FarmGrid restored;
    Check(restored.ReplaceSnapshot(snapshot) && restored.HasPondBank(),"pond round trip");
    snapshot.pondBank=false; Check(restored.ReplaceSnapshot(snapshot),"legacy restore");
    water.Rebuild(restored); Check(water.GetWaterSourceCount()==0 && !water.IsSupplied(gate),"legacy edge fed");
    // Same final ditch, dug from either side. The direction of the tool is not a water source.
    for (bool reverse:{false,true}) {
        Check(pond::Start(grid),"new bank");
        pond::Stroke along;
        Check(along.Begin(grid,gate,{},false) && along.Pull(grid,gate+5,{0,0,1},1,dig),"longitudinal ditch");
        advance();
        Check(!pond::IsOpen(grid,gate) && !pond::IsOpen(grid,gate+5) &&
            grid.GetTile(gate)->waterAmount==0 && grid.GetTile(fork)->waterAmount==0,"parallel cut leaked");
        pond::Stroke cross;
        Check(cross.Begin(grid,reverse?fork:gate,{},false) &&
            cross.Pull(grid,reverse?gate:fork,{reverse?1.f:-1.f,0,0},1,dig),"lateral breach");
        advance();
        Check(pond::IsOpen(grid,gate) && !pond::IsOpen(grid,gate+5) &&
            water.GetUpstreamTileIndex(gate+5)==gate && water.GetUpstreamTileIndex(fork)==gate &&
            grid.GetTile(gate+5)->waterAmount>0 && grid.GetTile(fork)->waterAmount>0,
            "reverse digging changed hydraulic direction");
    }
}

int main(int argc, char** argv) try {
    PondBankChecks();
    const bool candidates = argc == 2 && std::string_view(argv[1]) == "--candidates";
    Check(argc == 1 || candidates, "usage: farm_irrigation_balance_test [--candidates]");
    std::cout << "tuning,layout,crop,policy,rain,speed,fields,sources,canals,ready_mean,ready_last,good_pct,excess_pct,"
        "quality_mean,quality_min,intake_changes,water_actions,soil_delivered,rain_delivered\n";
    std::cout << std::fixed << std::setprecision(3);
    int cases = 0;
    for (std::size_t profile = 0; profile < (candidates ? kTunings.size() : 1); ++profile) {
        FarmRules rules;
        const char* name = "production";
        if (candidates) {
            rules.irrigationMoistureRecoveryPerSecond = kTunings[profile].recovery;
            rules.irrigationCanalTransferEfficiency = kTunings[profile].efficiency;
            name = kTunings[profile].name;
        }
        for (const auto& layout : kLayouts) for (auto crop : kCrops) for (auto policy : kPolicies) {
            for (auto weather : kWeathers) {
                const auto baseline = Run(layout, crop, policy, weather, 1.0f, rules);
                Print(name, layout, crop, policy, weather, 1.0f, baseline); ++cases;
                if (!candidates) VerifyBalanceTargets(layout, crop, policy, weather, baseline);
                if (layout.soilHeightOffset > 0 || policy == Policy::Manual || policy == Policy::Closed)
                    Check(baseline.delivered == 0, "blocked delivery");
                if (!candidates) for (float speed : {2.0f, 4.0f}) {
                    const auto other = Run(layout, crop, policy, weather, speed, rules);
                    Check(std::abs(other.readyLast - baseline.readyLast) < 0.05 &&
                        std::abs(other.qualityMean - baseline.qualityMean) < 0.1 &&
                        other.intakeChanges == baseline.intakeChanges, "scaled tick parity");
                    Print(name, layout, crop, policy, weather, speed, other); ++cases;
                }
            }
        }
    }
    if (!candidates) {
        VerifySharedRate();
        const FarmRules rules;
        for (const auto& layout : kLayouts) for (auto crop : kCrops) for (auto weather : kWeathers) {
            const auto baseline = Run(layout, crop, Policy::ManagedTwo, weather, 1, rules, 0.0f);
            for (float speed : {2.f, 4.f}) {
                const auto other = Run(layout, crop, Policy::ManagedTwo, weather, speed, rules, 0.0f, true);
                Check(std::abs(other.readyLast - baseline.readyLast) < 1.0 &&
                    std::abs(other.qualityMean - baseline.qualityMean) < 2, "fixed-frame dry-start drift");
            }
        }
        std::cerr << "PASS: " << kLayouts.size() * kCrops.size() * kWeathers.size() * 3
            << " dry-start runs including actual 1/2/4x frame steps\n";
        std::cerr << "PASS: five-second management targets and shared runtime/forecast rates\n";
    }
    std::cerr << "PASS: " << cases << " irrigation layout runs; scripted fixtures, not a human playthrough\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n'; return 1;
}
