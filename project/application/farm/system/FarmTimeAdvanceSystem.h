#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmContestDaySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmPestSystem.h"
#include "farm/system/FarmSimulationStepSystem.h"
#include <algorithm>
#include <cmath>

// Session-only. The caller owns all Systems and exclusively schedules their updates.
class FarmTimeAdvanceSystem final {
public:
    enum class StopReason {
        None, Cancelled, Blocked, InvalidState, ContextChanged, HarvestReady,
        WaterLow, WaterExcess, ContestDay, Finished, NoTarget, BudgetLimit, TargetReached, PestWarning, WeatherChanged
    };
    using Context = FarmSimulationStepSystem::Context;
    struct Status {
        bool active = false;
        StopReason reason = StopReason::None;
        int tileIndex = -1;
        int targetDay = 0;
        int completedSteps = 0;
    };
    // Bounds CPU work per invocation; this is not a display-rate or speed promise.
    static constexpr int kStepsPerBatch = 64;
    static constexpr int kSessionStepLimit = 120000;
    static constexpr float kStepSeconds = 1.0f / 60.0f;

    [[nodiscard]] const Status& GetStatus() const noexcept { return status_; }
    void Reset() noexcept { *this = FarmTimeAdvanceSystem{}; }
    void Cancel() noexcept { if (status_.active) Stop(StopReason::Cancelled); }
    void Interrupt() noexcept { if (status_.active) Stop(StopReason::ContextChanged); }

    bool Start(Context context, bool interactionBlocked) {
        if (status_.active) return false;
        Reset();
        if (!Check(context, interactionBlocked)) return false;
        if (context.progression.IsFreeFarming()) return Stop(StopReason::NoTarget);
        for (std::size_t i = 0; i < FarmContestEntrySystem::kContestDays.size(); ++i) {
            const int day = FarmContestEntrySystem::kContestDays[i];
            if (day > context.date.GetDay() && context.economy.GetContestResults()[i].contestDay == 0) {
                status_.targetDay = day;
                break;
            }
        }
        if (!status_.targetDay && context.progression.IsContestSeason())
            status_.targetDay = FarmContestEntrySystem::kContestDays.back() + 1;
        if (status_.targetDay <= context.date.GetDay()) return Stop(StopReason::NoTarget);
        gridIdentity_ = &context.grid;
        dateIdentity_ = &context.date;
        generation_ = context.grid.GetGeneration();
        expectedClock_ = context.date.CaptureSnapshot();
        expectedMode_ = context.progression.GetMode();
        expectedPests_ = context.progression.PestsEnabled();
        expectedSpreadVersion_ = context.progression.PestSpread().version;
        expectedWeatherVersion_ = context.progression.WeatherVersion();
        initialWeather_ = FarmWeatherSystem::Inspect(context.date.GetDay(), expectedWeatherVersion_).phase;
        status_.active = true;
        return true;
    }

    // Returns completed steps. A positive count requires caller dirty/timeline handling.
    // blocked must cover menus, observation, previews, focus loss and other edits.
    int AdvanceBatch(Context context, bool interactionBlocked) {
        int steps = 0;
        while (status_.active && steps < kStepsPerBatch) {
            if (!Check(context, interactionBlocked)) break;
            if (status_.completedSteps >= kSessionStepLimit) {
                Stop(StopReason::BudgetLimit);
                break;
            }
            if (!FarmSimulationStepSystem::Step(context, kStepSeconds, status_.targetDay).advanced) {
                Stop(StopReason::InvalidState); break;
            }
            expectedClock_ = context.date.CaptureSnapshot();
            ++steps;
            ++status_.completedSteps;
            if (!Check(context, interactionBlocked)) break;
        }
        return steps;
    }

private:
    bool Stop(StopReason reason, int tileIndex = -1) noexcept {
        status_.active = false;
        status_.reason = reason;
        status_.tileIndex = tileIndex;
        return false;
    }
    bool Check(Context context, bool blocked) {
        if (blocked) return Stop(StopReason::Blocked);
        const auto clock = context.date.CaptureSnapshot();
        if (status_.active && (gridIdentity_ != &context.grid || dateIdentity_ != &context.date ||
            generation_ != context.grid.GetGeneration() || clock.day != expectedClock_.day ||
            clock.elapsedSecondsInDay != expectedClock_.elapsedSecondsInDay || clock.timeScale != expectedClock_.timeScale ||
            context.progression.GetMode() != expectedMode_ || context.progression.PestsEnabled() != expectedPests_ ||
            context.progression.PestSpread().version != expectedSpreadVersion_ ||
            context.progression.WeatherVersion() != expectedWeatherVersion_))
            return Stop(StopReason::ContextChanged);
        if (context.grid.GetTileCount() <= 0 || clock.day < 1 || !std::isfinite(clock.elapsedSecondsInDay) ||
            clock.elapsedSecondsInDay < 0 || clock.elapsedSecondsInDay >= context.date.GetDayLengthSeconds() ||
            !std::isfinite(clock.timeScale) || clock.timeScale <= 0)
            return Stop(StopReason::InvalidState);
        if (context.progression.IsCleared() || (context.progression.IsContestSeason() &&
            FarmContestSeasonSystem::Evaluate(clock.day, context.economy.GetContestResults()).finalized))
            return Stop(StopReason::Finished);
        if (context.progression.PestsEnabled())
            if (const int index = FarmPestSystem::FindActive(context.grid); index >= 0)
                return Stop(StopReason::PestWarning, index);
        context.contest.Observe(clock.day, context.economy.GetContestResults(), !context.progression.IsFreeFarming());
        if (context.contest.PendingDay()) return Stop(StopReason::ContestDay);
        if (status_.active && FarmWeatherSystem::Inspect(clock.day, expectedWeatherVersion_).phase != initialWeather_)
            return Stop(StopReason::WeatherChanged);
        if (status_.active && clock.day >= status_.targetDay) return Stop(StopReason::TargetReached);
        for (int index = 0; index < context.grid.GetTileCount(); ++index) {
            const auto* tile = context.grid.GetTile(index);
            if (!tile) return Stop(StopReason::InvalidState, index);
            if (tile->crop == farm::CropType::None && tile->state != farm::FarmTileState::Planted &&
                tile->state != farm::FarmTileState::ReadyToHarvest) continue;
            if (!farm::IsPlantableCrop(tile->crop) || tile->feature != farm::FarmTileFeature::None ||
                (tile->state != farm::FarmTileState::Planted && tile->state != farm::FarmTileState::ReadyToHarvest) ||
                !std::isfinite(tile->growth) || tile->growth < 0 || tile->growth > 1 ||
                !std::isfinite(tile->moisture) || tile->moisture < 0 || tile->moisture > 1)
                return Stop(StopReason::InvalidState, index);
            if (tile->state == farm::FarmTileState::ReadyToHarvest || farm::IsHarvestReady(*tile))
                return Stop(StopReason::HarvestReady, index);
            const auto forecast = context.growth.Evaluate(*tile, tile->crop);
            if (!forecast.moistureValid) return Stop(StopReason::InvalidState, index);
            switch (forecast.moistureStatus) {
            case FarmMoistureStatus::Dry:
            case FarmMoistureStatus::Low: return Stop(StopReason::WaterLow, index);
            case FarmMoistureStatus::Excess: return Stop(StopReason::WaterExcess, index);
            case FarmMoistureStatus::Good: break;
            default: return Stop(StopReason::InvalidState, index);
            }
        }
        return true;
    }
    Status status_{};
    // Identity only; never dereferenced, no ownership. Reset on load/restart is still required.
    const farm::FarmGrid* gridIdentity_ = nullptr;
    const FarmDateSystem* dateIdentity_ = nullptr;
    std::uint64_t generation_ = 0;
    FarmDateSystem::Snapshot expectedClock_{};
    FarmProgressionMode expectedMode_ = FarmProgressionMode::Trial;
    bool expectedPests_ = false;
    int expectedSpreadVersion_ = 0;
    int expectedWeatherVersion_ = 0;
    FarmWeatherSystem::Phase initialWeather_ = FarmWeatherSystem::Phase::Disabled;
};
