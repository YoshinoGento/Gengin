#pragma once
#include "farm/system/FarmContestDaySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmPestSystem.h"
#include <algorithm>
#include <cmath>
#include <limits>

// Synchronous borrows only. Normal play and waiting use the same mutation order.
class FarmSimulationStepSystem final {
public:
    struct Context {
        farm::FarmGrid& grid;
        farm::FarmIrrigationSystem& irrigation;
        const FarmGrowthSystem& growth;
        FarmDateSystem& date;
        FarmContestDaySystem& contest;
        const FarmEconomySystem& economy;
        FarmProgressionSystem& progression;
        FarmGrowthComparisonSystem* comparison = nullptr;
    };
    struct Result {
        bool advanced = false, dayChanged = false, pestWarning = false, weatherChanged = false;
        double rainDelivered = 0, canalDelivered = 0;
    };

    [[nodiscard]] static Result Step(Context c, float requestedDelta, int targetDay = 0) {
        const auto clock = c.date.CaptureSnapshot();
        if (!std::isfinite(requestedDelta) || requestedDelta <= 0 || !std::isfinite(clock.timeScale) ||
            clock.timeScale <= 0 || c.progression.IsCleared()) return {};
        c.contest.Observe(clock.day, c.economy.GetContestResults(), !c.progression.IsFreeFarming());
        if (c.contest.PendingDay()) return {};
        int boundary = targetDay;
        if ((c.progression.PestsEnabled() || c.progression.WeatherVersion() != 0) && clock.day < (std::numeric_limits<int>::max)() &&
            (boundary <= clock.day || clock.day + 1 < boundary)) boundary = clock.day + 1;
        if (!c.progression.IsFreeFarming()) {
            for (int day : FarmContestEntrySystem::kContestDays)
                if (day > clock.day && (boundary <= clock.day || day < boundary)) boundary = day;
        }
        // All subsystems consume the same bounded time, including the calendar.
        float delta = (std::min)({requestedDelta, c.growth.GetMaxUpdateDeltaTime(), c.irrigation.GetMaxUpdateDeltaTime()});
        if (boundary > clock.day) {
            const double remaining = static_cast<double>(boundary - clock.day) * c.date.GetDayLengthSeconds() - clock.elapsedSecondsInDay;
            delta = static_cast<float>((std::min)(static_cast<double>(delta), remaining / clock.timeScale));
        }
        if (!std::isfinite(delta) || delta <= 0) return {};
        if (c.comparison) c.comparison->ObserveBeforeStep(c.grid);
        const auto weather = FarmWeatherSystem::Inspect(clock.day, c.progression.WeatherVersion());
        c.irrigation.UpdateWater(c.grid, delta, clock.timeScale, weather.soilRainPerSecond);
        // Growth can change tile identity at maturity; capture flow while it is still valid.
        const auto water = c.irrigation.GetLastStep(c.grid);
        c.growth.Update(c.grid, delta, clock.timeScale);
        if (c.comparison) c.comparison->ObserveAfterStep(c.grid, delta, clock.timeScale);
        c.contest.Advance(c.date, delta, c.economy.GetContestResults(), boundary, !c.progression.IsFreeFarming());
        const auto pests = c.progression.AdvancePests(c.grid, clock.day, c.date.GetDay());
        static_cast<void>(c.progression.EvaluateSeason(FarmContestSeasonSystem::Evaluate(c.date.GetDay(), c.economy.GetContestResults())));
        return {true, c.date.GetDay() != clock.day, pests.warningStarted,
            FarmWeatherSystem::Changed(clock.day, c.date.GetDay(), c.progression.WeatherVersion()),
            water.rainDelivered, water.soilDelivered};
    }
};
