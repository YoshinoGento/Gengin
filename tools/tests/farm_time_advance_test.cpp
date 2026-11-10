#include <cassert>
#include "farm_test_edit.h"
#include "farm/system/FarmTimeAdvanceSystem.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"
#include "editor/FarmTimeAdvancePlaytestSystem.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Advance = FarmTimeAdvanceSystem;
using Reason = Advance::StopReason;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    farm::FarmGrid grid;
    farm::FarmIrrigationSystem irrigation;
    FarmGrowthSystem growth;
    FarmDateSystem date;
    FarmContestDaySystem contest;
    FarmEconomySystem economy;
    FarmProgressionSystem progression;
    Advance advance;
    Fixture() {
        Require(grid.Initialize(5, 4), "grid");
        irrigation.Initialize(); growth.Initialize(); date.Initialize(); economy.Initialize();
        progression.Initialize({}, FarmProgressionMode::ContestSeason);
    }
    Advance::Context Context() { return {grid, irrigation, growth, date, contest, economy, progression}; }
    void Crop(farm::CropType crop, float water, float growthValue = 0.2f, int index = 19) {
        farm::FarmTile tile;
        tile.state = farm::FarmTileState::Planted; tile.crop = crop;
        tile.moisture = water; tile.growth = growthValue;
        Require(grid.SetTile(index, tile), "crop fixture");
    }
    void RunToStop() {
        int batches = 0;
        while (advance.GetStatus().active) {
            const int steps = advance.AdvanceBatch(Context(), false);
            Require(steps >= 0 && steps <= Advance::kStepsPerBatch, "batch bound");
            Require(++batches <= Advance::kSessionStepLimit, "termination");
        }
    }
};
void SameClock(const FarmDateSystem::Snapshot& a, const FarmDateSystem::Snapshot& b) {
    Require(a.day == b.day && a.elapsedSecondsInDay == b.elapsedSecondsInDay && a.timeScale == b.timeScale,
        "clock mismatch");
}
void CheckBlocked(Fixture& f, Reason reason, int index = -1) {
    const auto before = f.date.CaptureSnapshot();
    Require(!f.advance.Start(f.Context(), false), "unsafe start accepted");
    Require(f.advance.GetStatus().reason == reason && f.advance.GetStatus().tileIndex == index, "stop explanation");
    Require(f.advance.AdvanceBatch(f.Context(), false) == 0, "stopped session advanced");
    SameClock(before, f.date.CaptureSnapshot());
}
}

int main() try {
    {
        Fixture f;
        FarmTimeAdvanceControlSystem control;
        Require(!control.HoldsSimulation(), "initial transport held");
        Require(control.Start(f.Context(), false, true), "live start");
        Require(control.HoldsSimulation(), "live start did not hold fixed updates");
        control.BeginFrame();
        Require(control.Tick(f.Context(), false) == Advance::kStepsPerBatch, "live batch budget");
        const auto clock = f.date.CaptureSnapshot();
        Require(control.Tick(f.Context(), false) == 0, "duplicate batch in render frame");
        control.Cancel();
        Require(!control.Read().status.active && control.Read().held && control.Read().status.reason == Reason::Cancelled, "cancel must hold");
        SameClock(clock, f.date.CaptureSnapshot());
        control.Reset();
        Require(control.HoldsSimulation(), "resume advanced twice in consumed frame");
        control.BeginFrame();
        Require(!control.HoldsSimulation(), "resume remained held next frame");
        Require(control.Start(f.Context(), false, false), "manual start");
        Require(control.Tick(f.Context(), false) == Advance::kStepsPerBatch, "manual batch");
        control.BeginFrame();
        Require(control.Tick(f.Context(), false) == 0 && control.Read().status.active, "manual continued without request");
        Require(control.Start(f.Context(), false, false), "manual second request");
        Require(control.Tick(f.Context(), false) == Advance::kStepsPerBatch, "manual second batch");
        control.BeginFrame();
        Require(control.Start(f.Context(), false, true) && control.Read().automatic, "manual to automatic");
        Require(control.Tick(f.Context(), false) == Advance::kStepsPerBatch, "automatic after manual");
        Require(!control.Start(f.Context(), false, true), "duplicate automatic accepted");
        control.BeginFrame();
        Require(control.Tick(f.Context(), true) == 0 && control.Read().status.reason == Reason::Blocked, "focus/menu stop");
        Require(control.HoldsSimulation(), "blocked did not hold");
        Require(control.Start(f.Context(), false, true), "restart after blocked");
        control.Interrupt();
        Require(control.Read().status.reason == Reason::ContextChanged && control.HoldsSimulation(), "edit invalidation");
        control.Reset(); control.BeginFrame();
        f.Crop(farm::CropType::Carrot, .01f);
        Require(!control.Start(f.Context(), false, true), "unsafe live start");
        Require(control.Read().status.reason == Reason::WaterLow && control.HoldsSimulation(), "unsafe hold reason");
    }
    {
        Fixture f;
        f.progression.Initialize({}, FarmProgressionMode::FreeFarming, true);
        Require(f.date.RestoreSnapshot({(std::numeric_limits<int>::max)(), 59.9f, 1}), "max day fixture");
        Require(!FarmSimulationStepSystem::Step(f.Context(), std::numeric_limits<float>::quiet_NaN()).advanced, "NaN step");
        (void)FarmSimulationStepSystem::Step(f.Context(), .2f);
        Require(f.date.GetDay() == (std::numeric_limits<int>::max)(), "day overflow");
    }
    {
        Fixture f;
        FarmTimeAdvanceControlSystem control;
        Require(f.date.RestoreSnapshot({9, 59.99f, 1}), "live contest clock");
        Require(control.Start(f.Context(), false, true), "live contest start");
        control.BeginFrame();
        Require(control.Tick(f.Context(), false) == 1 && control.Read().status.reason == Reason::ContestDay,
            "live contest boundary");
        Require(f.contest.Acknowledge(10), "live contest acknowledgement");
        control.BeginFrame();
        Require(control.HoldsSimulation(), "acknowledgement must not implicitly resume");
        control.Reset();
        Require(!control.HoldsSimulation(), "explicit contest resume remains held");
        Require(FarmSimulationStepSystem::Step(f.Context(), Advance::kStepSeconds).advanced &&
            f.date.GetElapsedSecondsInDay() > 0, "normal simulation after contest resume");
    }
    {
        Fixture batch, ordinary;
        FarmGrowthComparisonSystem batchComparison, normalComparison;
        batchComparison.Initialize(); normalComparison.Initialize();
        for (Fixture* fixture : {&batch, &ordinary}) {
            fixture->Crop(farm::CropType::Carrot, .55f, .2f, 18);
            fixture->Crop(farm::CropType::Carrot, .55f, .2f, 19);
        }
        Require(batchComparison.Pin(batch.grid, 0, 18) && batchComparison.Pin(batch.grid, 1, 19) &&
            batchComparison.Start(batch.grid), "batch observer start");
        Require(normalComparison.Pin(ordinary.grid, 0, 18) && normalComparison.Pin(ordinary.grid, 1, 19) &&
            normalComparison.Start(ordinary.grid), "normal observer start");
        auto batchContext = batch.Context(); batchContext.comparison = &batchComparison;
        auto normalContext = ordinary.Context(); normalContext.comparison = &normalComparison;
        Require(batch.advance.Start(batchContext, false), "observed batch start");
        const int steps = batch.advance.AdvanceBatch(batchContext, false);
        for (int i = 0; i < steps; ++i)
            Require(FarmSimulationStepSystem::Step(normalContext, Advance::kStepSeconds).advanced, "shared normal step");
        const auto observedBatch = batchComparison.GetView(batch.grid);
        const auto observedNormal = normalComparison.GetView(ordinary.grid);
        Require(observedBatch.status == FarmComparisonStatus::Running &&
            observedBatch.elapsedSeconds > 0 && observedBatch.elapsedSeconds == observedNormal.elapsedSeconds,
            "comparison observer parity");
        SameClock(batch.date.CaptureSnapshot(), ordinary.date.CaptureSnapshot());
        for (int i = 0; i < 2; ++i)
            Require(observedBatch.rows[i].current.growth == observedNormal.rows[i].current.growth &&
                observedBatch.rows[i].current.moisture == observedNormal.rows[i].current.moisture,
                "comparison row parity");
    }
    for (const float speed : {1.0f, 2.0f, 4.0f}) {
        for (const int warningDay : {6, 16, 26}) {
            Fixture f, ordinary;
            for (Fixture* sample : {&f, &ordinary}) {
                sample->progression.Initialize({}, FarmProgressionMode::ContestSeason, true);
                sample->Crop(farm::CropType::Carrot, .55f);
                Require(sample->date.RestoreSnapshot({warningDay - 1, 59.99f, speed}), "pest clock");
            }
            Require(f.advance.Start(f.Context(), false), "pest-enabled start refused");
            const float dt = (f.date.GetDayLengthSeconds() - f.date.GetElapsedSecondsInDay()) / speed;
            ordinary.irrigation.UpdateWater(ordinary.grid, dt, speed);
            ordinary.growth.Update(ordinary.grid, dt, speed);
            ordinary.contest.Advance(ordinary.date, dt, ordinary.economy.GetContestResults(), warningDay);
            static_cast<void>(FarmPestSystem::AdvanceDays(ordinary.grid, warningDay - 1, ordinary.date.GetDay(), true));
            Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "warning did not stop first step");
            Require(f.advance.GetStatus().reason == Reason::PestWarning && f.advance.GetStatus().tileIndex == 19,
                "pest stop reason");
            SameClock(f.date.CaptureSnapshot(), ordinary.date.CaptureSnapshot());
            const auto* a = f.grid.GetTile(19); const auto* b = ordinary.grid.GetTile(19);
            Require(a && b && a->pests.warningDay == warningDay && a->pests.damageDays == 0 &&
                a->growth == b->growth && a->moisture == b->moisture &&
                a->careHistory.GetObservedSeconds() == b->careHistory.GetObservedSeconds(), "pest step parity");
            CheckBlocked(f, Reason::PestWarning, 19);
            Require(FarmPestSystem::Cure(f.grid, 19, warningDay), "cure sample");
            Require(f.advance.Start(f.Context(), false), "resume after cure");
            Require(f.advance.AdvanceBatch(f.Context(), false) > 0, "resume frozen");
            Require(f.grid.GetTile(19)->pests.warningDay == 0, "same day re-warning");
        }
    }
    {
        Fixture f;
        Require(f.advance.Start(f.Context(), false), "policy fixture start");
        f.progression.Initialize({}, FarmProgressionMode::ContestSeason, true);
        Require(f.advance.AdvanceBatch(f.Context(), false) == 0 &&
            f.advance.GetStatus().reason == Reason::ContextChanged, "policy change not stopped");
    }
    {
        using Test = editor::FarmTimeAdvancePlaytestSystem;
        using Action = Test::Action;
        using Scenario = Test::Scenario;
        Test sample, untouched;
        Require(!sample.Execute({Action::Start}), "uninitialized sample accepted");
        Require(!sample.Execute({Action::Reset, Scenario::Count}), "invalid scenario accepted");
        constexpr Reason expected[] = {Reason::PestWarning, Reason::HarvestReady, Reason::WaterLow,
            Reason::WaterExcess, Reason::ContestDay};
        for (int i = 0; i < 5; ++i) {
            Require(sample.Execute({Action::Reset, static_cast<Scenario>(i)}), "sample reset");
            Require(sample.Execute({Action::Start}), "sample start request");
            sample.Tick(false);
            const auto view = sample.Read();
            Require(!view.running && !view.status.active && view.status.reason == expected[i], "sample stop");
            Require(view.status.completedSteps <= Advance::kStepsPerBatch, "sample bound");
            if (i == 0) Require(view.warningDay == 6 && view.damageDays == 0, "sample pest exposure");
            const auto clock = view.clock;
            sample.Tick(false);
            SameClock(clock, sample.Read().clock);
        }
        Require(sample.Execute({Action::Reset, Scenario::Empty}), "empty sample");
        Require(sample.Execute({Action::Step}), "single batch request");
        Require(sample.Read().status.completedSteps == Advance::kStepsPerBatch && !sample.Read().running,
            "manual batch result");
        const auto clock = sample.Read().clock;
        sample.Tick(false);
        SameClock(clock, sample.Read().clock);
        Require(sample.Execute({Action::Cancel}) && sample.Read().status.reason == Reason::Cancelled, "cancel request");
        Require(sample.Execute({Action::Reset, Scenario::Empty}) && sample.Execute({Action::Start}), "auto start");
        Require(!sample.Execute({Action::Start}) && !sample.Execute({Action::Step}), "double update enabled");
        sample.Tick(true);
        Require(sample.Read().status.reason == Reason::Blocked && sample.Read().status.completedSteps == 0, "hidden advance");
        Require(!sample.Execute({static_cast<Action>(999)}), "invalid action accepted");
        Require(!untouched.Read().ready && untouched.Read().clock.day == 1, "sample leakage");
    }
    for (const float speed : {1.0f, 2.0f, 4.0f}) {
        for (const int day : {10, 20, 30}) {
            Fixture f;
            Require(f.date.RestoreSnapshot({day - 1, 59.99f, speed}), "near event fixture");
            Require(f.advance.Start(f.Context(), false), "event start");
            f.RunToStop();
            Require(f.date.GetDay() == day && f.date.GetElapsedSecondsInDay() == 0, "event overshoot");
            Require(f.advance.GetStatus().reason == Reason::ContestDay && f.contest.PendingDay() == day, "event reason");
            const auto flow = f.irrigation.GetLastStep(f.grid);
            Require(flow.valid && flow.simulatedSeconds <= 0.01001, "irrigation stepped beyond date boundary");
            CheckBlocked(f, Reason::ContestDay);
        }
        for (const auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot, farm::CropType::Tomato, farm::CropType::Pumpkin}) {
            Fixture profileFixture; profileFixture.Crop(crop, .55f);
            const auto forecast=profileFixture.growth.Evaluate(*profileFixture.grid.GetTile(19));
            const float minimum=forecast.goodMoistureMinimum;
            const float maximum=forecast.goodMoistureMaximum;
            for (const float moisture : {0.0f, minimum - 0.001f, maximum + 0.001f, 1.0f}) {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, moisture);
                CheckBlocked(f, moisture < minimum ? Reason::WaterLow : Reason::WaterExcess, 19);
            }
            {
                Fixture f; f.Crop(crop, 0.55f, 1.0f);
                CheckBlocked(f, Reason::HarvestReady, 19);
            }
            {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, 0.55f, 0.9999f);
                Require(f.advance.Start(f.Context(), false), "near-ready start");
                Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "harvest stop not immediate");
                Require(f.advance.GetStatus().reason == Reason::HarvestReady &&
                    f.advance.GetStatus().tileIndex == 19, "harvest reason");
            }
            {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, minimum + 0.000001f);
                Require(f.advance.Start(f.Context(), false), "near-low start");
                Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "low stop not immediate");
                Require(f.advance.GetStatus().reason == Reason::WaterLow, "low reason");
            }
            {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, maximum - 0.000001f, 0.2f, 2);
                farm::FarmTile source; source.feature = farm::FarmTileFeature::WaterSource; source.waterAmount = 1;
                farm::FarmTile canal; canal.feature = farm::FarmTileFeature::Canal; canal.waterAmount = 1;
                Require(f.grid.SetTile(0, source) && f.grid.SetTile(1, canal), "water fixture");
                Require(f.advance.Start(f.Context(), false), "near-excess start");
                Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "excess stop not immediate");
                Require(f.advance.GetStatus().reason == Reason::WaterExcess &&
                    f.advance.GetStatus().tileIndex == 2, "excess reason");
            }
            {
                Fixture batch, ordinary;
                const float midpoint=(minimum+maximum)*.5f;
                batch.Crop(crop, midpoint); ordinary.Crop(crop, midpoint);
                batch.date.SetTimeScale(speed); ordinary.date.SetTimeScale(speed);
                Require(batch.advance.Start(batch.Context(), false), "equivalence start");
                const int steps = batch.advance.AdvanceBatch(batch.Context(), false);
                Require(steps == Advance::kStepsPerBatch && batch.advance.GetStatus().active, "batch budget");
                for (int i = 0; i < steps; ++i) {
                    ordinary.irrigation.UpdateWater(ordinary.grid, Advance::kStepSeconds, speed);
                    ordinary.growth.Update(ordinary.grid, Advance::kStepSeconds, speed);
                    ordinary.contest.Advance(ordinary.date, Advance::kStepSeconds, ordinary.economy.GetContestResults(), 31);
                }
                SameClock(batch.date.CaptureSnapshot(), ordinary.date.CaptureSnapshot());
                const auto* a = batch.grid.GetTile(19); const auto* b = ordinary.grid.GetTile(19);
                Require(a && b && a->growth == b->growth && a->moisture == b->moisture &&
                    a->careHistory.GetObservedSeconds() == b->careHistory.GetObservedSeconds(), "growth equivalence");
                const auto before = batch.date.CaptureSnapshot();
                Require(!batch.advance.Start(batch.Context(), false), "double start accepted");
                batch.advance.Cancel();
                Require(batch.advance.GetStatus().reason == Reason::Cancelled &&
                    batch.advance.AdvanceBatch(batch.Context(), false) == 0, "cancel ignored");
                SameClock(before, batch.date.CaptureSnapshot());
            }
        }
    }
    for (int change = 0; change < 4; ++change) {
        Fixture f;
        Require(f.advance.Start(f.Context(), false), "invalidation start");
        if (change == 0) { farm::FarmGrid::Snapshot saved; f.grid.CaptureSnapshot(saved); Require(f.grid.RestoreSnapshot(saved), "restore"); }
        if (change == 1) f.date.AdvanceOneDay();
        if (change == 2) f.date.SetTimeScale(2);
        const auto before = f.date.CaptureSnapshot();
        Require(f.advance.AdvanceBatch(f.Context(), change == 3) == 0, "invalidation advanced");
        Require(f.advance.GetStatus().reason == (change == 3 ? Reason::Blocked : Reason::ContextChanged), "invalidation reason");
        SameClock(before, f.date.CaptureSnapshot());
    }
    {
        Fixture f, other;
        Require(f.advance.Start(f.Context(), false), "identity start");
        Require(f.advance.AdvanceBatch(other.Context(), false) == 0 &&
            f.advance.GetStatus().reason == Reason::ContextChanged, "other farm accepted");
    }
    {
        Fixture f;
        Require(!f.advance.Start(f.Context(), true) && f.advance.GetStatus().reason == Reason::Blocked, "blocked start");
        f.advance.Reset();
        Require(f.advance.GetStatus().reason == Reason::None && f.advance.GetStatus().completedSteps == 0, "reset");
        Require(f.date.RestoreSnapshot({30, 59.99f, 4}), "deadline fixture");
        f.contest.Observe(30, f.economy.GetContestResults());
        Require(f.contest.Acknowledge(30), "acknowledge deadline");
        Require(f.advance.Start(f.Context(), false), "deadline start");
        f.RunToStop();
        Require(f.date.GetDay() == 31 && f.progression.IsCleared() &&
            f.advance.GetStatus().reason == Reason::Finished, "deadline did not stop");
        CheckBlocked(f, Reason::Finished);
    }
    {
        Fixture f; f.progression.Initialize({}, FarmProgressionMode::Trial);
        Require(f.date.RestoreSnapshot({31, 0, 1}), "no-target fixture");
        CheckBlocked(f, Reason::NoTarget);
    }
    for (const float value : {std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -0.1f, 1.1f}) {
        for (const bool corruptGrowth : {false, true}) {
            Fixture f; f.Crop(farm::CropType::Carrot, 0.55f);
            // Corrupted values must be rejected before reaching the simulation.
            auto candidate = *f.grid.GetTile(19);
            auto* tile = &candidate;
            Require(tile != nullptr, "corrupt fixture pointer");
            if (corruptGrowth) tile->growth = value;
            else tile->moisture = value;
            Require(!f.grid.SetTile(19, candidate), "invalid write accepted");
            Require(f.grid.GetTile(19)->growth == 0.2f && f.grid.GetTile(19)->moisture == 0.55f, "invalid write changed state");
        }
    }
    {
        Fixture f; f.Crop(farm::CropType::Carrot, 0.55f);
        auto candidate = *f.grid.GetTile(19);
            auto* tile = &candidate;
        Require(tile != nullptr, "invalid crop fixture pointer");
        tile->crop = static_cast<farm::CropType>(99);
        Require(!f.grid.SetTile(19, candidate), "invalid write accepted");
            Require(f.grid.GetTile(19)->growth == 0.2f && f.grid.GetTile(19)->moisture == 0.55f, "invalid write changed state");
    }
    {
        Fixture f;
        Require(f.advance.Start(f.Context(), false), "empty farm start");
        f.RunToStop();
        Require(f.date.GetDay() == 10 && f.advance.GetStatus().reason == Reason::ContestDay, "empty farm long run");
        Require(f.advance.GetStatus().completedSteps < Advance::kSessionStepLimit, "long run bounded");
    }
    std::cout << "PASS: bounded time advance, all speeds/crops, pest boundaries/parity, Debug scenarios, cancellation and invalidation\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
