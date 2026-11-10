#include "editor/FarmWeatherPlaytestSystem.h"
#include "farm/system/FarmTimeAdvanceSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/ui/FarmWeatherView.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
bool Near(double a, double b, double epsilon = 0.0001) { return std::abs(a - b) < epsilon; }
using W = FarmWeatherSystem;
using P = W::Phase;
struct Fixture {
    farm::FarmGrid grid;
    farm::FarmIrrigationSystem irrigation;
    FarmGrowthSystem growth;
    FarmDateSystem date;
    FarmContestDaySystem contest;
    FarmEconomySystem economy;
    FarmProgressionSystem progression;
    Fixture() {
        Check(grid.Initialize(3, 1), "grid");
        irrigation.Initialize(); growth.Initialize(); date.Initialize(); economy.Initialize();
        progression.Initialize({}, FarmProgressionMode::ContestSeason, false, true);
    }
    FarmSimulationStepSystem::Context Context() { return {grid, irrigation, growth, date, contest, economy, progression}; }
};
void ScheduleAndViews() {
    for (int day : W::kRainDays) {
        Check(W::Inspect(day - 1, 1).phase == P::ForecastRain, "forecast schedule");
        Check(W::Inspect(day, 1).phase == P::Rain && W::Inspect(day, 1).soilRainPerSecond > 0, "rain schedule");
        Check(W::Inspect(day + 1, 1).phase == P::Clear, "rain ends");
    }
    Check(W::Inspect(0, 1).phase == P::Disabled && W::Inspect(8, 0).phase == P::Disabled &&
        W::Inspect(8, 2).phase == P::Disabled && W::Inspect((std::numeric_limits<int>::max)(), 1).phase == P::Clear, "invalid schedule");
    for (auto phase : {P::Disabled, P::Clear, P::ForecastRain, P::Rain}) {
        farmui::View view;
        farmui::BuildWeatherView(view, {phase, 8, 0});
        Check(view.count == (phase == P::Disabled ? 0u : 1u), "weather HUD count");
        if (!view.count) continue;
        const auto& item = view.items[0];
        const auto& label = farmui::kLabels.at(static_cast<std::size_t>(item.label));
        Check(item.rect.x >= 338 && item.rect.x + item.rect.width < 1000 && item.rect.y >= 80 &&
            item.rect.y + item.rect.height <= 124, "weather overlaps other HUD bands");
        Check(item.rect.width - 20 >= label.width * 20.0f / 26.0f, "weather label overflow");
    }
    Fixture f;
    auto snapshot = f.progression.CaptureSnapshot();
    snapshot.weatherVersion = 2;
    Check(!f.progression.RestoreSnapshot(snapshot) && f.progression.WeatherVersion() == 1, "invalid policy mutates");
    snapshot.weatherVersion = 1; snapshot.stagedUnlocks = false;
    Check(!f.progression.RestoreSnapshot(snapshot), "unstaged weather accepted");
    f.progression.Initialize({}, FarmProgressionMode::FreeFarming, true, true);
    Check(f.progression.WeatherVersion() == 0, "free mode changed");
}
void WaterAccounting() {
    Fixture f;
    farm::FarmTile soil;
    soil.state = farm::FarmTileState::Tilled; soil.moisture = .2f; soil.irrigationEnabled = false;
    Check(f.grid.SetTile(0, soil), "soil");
    auto source = soil; source.state = farm::FarmTileState::Empty;
    source.feature = farm::FarmTileFeature::WaterSource; source.moisture = 0; source.waterAmount = .5f;
    Check(f.grid.SetTile(1, source), "source");
    Check(f.irrigation.UpdateWater(f.grid, .25f, 1, W::kSoilRainPerSecond), "rain update");
    auto summary = f.irrigation.GetLastStep(f.grid);
    Check(summary.valid && Near(summary.rainDelivered, .0045) && std::abs(summary.balanceError) < .00001, "rain breaks stock balance");
    Check(Near(f.grid.GetTile(0)->moisture, .2045) && f.grid.GetTile(1)->moisture == 0 &&
        f.grid.GetTile(2)->moisture == 0, "rain includes wrong tile types");
    Check(Near(f.irrigation.GetLastTileFlows(f.grid)[0].rainReceived, .0045), "rain flow missing");
    for (float value : {-.1f, 2.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        const auto before = *f.grid.GetTile(0);
        Check(!f.irrigation.UpdateWater(f.grid, .25f, 1, value) && f.grid.GetTile(0)->moisture == before.moisture,
            "invalid rain mutates soil");
    }
    for (int i = 0; i < 200; ++i) (void)f.irrigation.UpdateWater(f.grid, .25f, 4, W::kSoilRainPerSecond);
    Check(f.grid.GetTile(0)->moisture == W::kRainMoistureLimit, "rain cap");
    soil.moisture = .9f; Check(f.grid.SetTile(0, soil), "wet soil");
    (void)f.irrigation.UpdateWater(f.grid, .25f, 4, W::kSoilRainPerSecond);
    Check(f.grid.GetTile(0)->moisture == .9f, "rain unexpectedly drains soil");
}
void BoundariesAndAdvance() {
    for (int before : {6, 7, 8, 16, 17, 18, 26, 27, 28}) for (float speed : {1.f, 2.f, 4.f}) {
        Fixture f;
        Check(f.date.RestoreSnapshot({before, 59.99f, speed}), "boundary clock");
        FarmTimeAdvanceSystem advance;
        Check(advance.Start(f.Context(), false), "weather advance start");
        Check(advance.AdvanceBatch(f.Context(), false) > 0 &&
            advance.GetStatus().reason == FarmTimeAdvanceSystem::StopReason::WeatherChanged, "weather stop reason");
        Check(f.date.GetDay() == before + 1 && Near(f.date.GetElapsedSecondsInDay(), 0), "boundary overshoot");
        Check(advance.Start(f.Context(), false) && advance.AdvanceBatch(f.Context(), false) > 0, "weather restart blocked");
        auto policy = f.progression.CaptureSnapshot(); policy.weatherVersion = 0;
        Check(f.progression.RestoreSnapshot(policy), "policy fixture");
        Check(advance.AdvanceBatch(f.Context(), false) == 0 &&
            advance.GetStatus().reason == FarmTimeAdvanceSystem::StopReason::ContextChanged, "policy invalidation");
    }
    Fixture f;
    Check(f.date.RestoreSnapshot({18, 0, 1}), "step clock");
    farm::FarmTile soil; soil.state = farm::FarmTileState::Tilled; soil.irrigationEnabled = false;
    Check(f.grid.SetTile(0, soil), "step soil");
    Check(FarmSimulationStepSystem::Step(f.Context(), 5).advanced && Near(f.date.GetElapsedSecondsInDay(), .25), "clock ignores subsystem time cap");
    Check(Near(f.grid.GetTile(0)->moisture, W::kSoilRainPerSecond * .25f), "different water/calendar delta");
    Check(!FarmSimulationStepSystem::Step(f.Context(), std::numeric_limits<float>::quiet_NaN()).advanced, "NaN delta accepted");
    Check(f.date.RestoreSnapshot({17, 59.99f, 1}), "before rain");
    const float moisture = f.grid.GetTile(0)->moisture;
    Check(FarmSimulationStepSystem::Step(f.Context(), .1f).weatherChanged && f.grid.GetTile(0)->moisture == moisture, "rain applied before beginning");
    Check(FarmSimulationStepSystem::Step(f.Context(), .1f).rainDelivered > 0, "rain not applied after beginning");
}
void DebugControls() {
    using T = editor::FarmWeatherPlaytestSystem; using A = T::Action; using S = T::Scenario;
    T t, other;
    Check(!t.Execute({A::Step}) && !t.Execute({A::Reset, S::Count}), "idle/invalid controls accepted");
    for (auto scenario : {S::Forecast, S::RainStart, S::RainEnd}) {
        Check(t.Execute({A::Reset, scenario}) && t.Execute({A::Start}), "boundary controls");
        for (int i = 0; i < 4; ++i) t.Tick(.1f, false);
        Check(t.Read().boundaryStopped && !t.Read().running && Near(t.Read().clock.elapsedSecondsInDay, 0), "debug boundary did not stop");
        Check(t.Execute({A::Step}) && !t.Read().boundaryStopped, "debug restart");
    }
    Check(t.Execute({A::Reset, S::Rain}) && t.Execute({A::Speed4}) && t.Execute({A::Start}), "rain start");
    Check(!t.Execute({A::Start}) && !t.Execute({A::Step}) && !t.Execute({A::ToggleLeft}), "running controls unlocked");
    t.Tick(.1f, false);
    Check(t.Read().simulatedSeconds > 0 && t.Read().simulatedSeconds <= 4.001f, "debug batch budget");
    t.Tick(.1f, true);
    const float stopped = t.Read().simulatedSeconds;
    t.Tick(1, false);
    Check(!t.Read().running && t.Read().simulatedSeconds == stopped, "hidden/focus advanced");
    Check(t.Execute({A::Start}), "resume rain");
    for (int i = 0; i < 100 && t.Read().running; ++i) t.Tick(.1f, false);
    const auto view = t.Read();
    Check(!view.running && view.simulatedSeconds <= 60.01f && view.simulatedSeconds >= 59.9f, "debug duration bound");
    Check(view.samples[0].rainReceived > 0 && view.samples[0].canalReceived == 0 &&
        view.samples[1].canalReceived > 0 && view.samples[1].excessSeconds > view.samples[0].excessSeconds,
        "intake comparison did not differ");
    Check(view.samples[0].mature && view.samples[0].quality > view.samples[1].quality, "rain OFF quality comparison");
    std::cout << "Rain comparison OFF/ON: moisture " << view.samples[0].moisture << '/' << view.samples[1].moisture
        << ", quality " << view.samples[0].quality << '/' << view.samples[1].quality << '\n';
    Check(!other.Read().ready, "separate instance mutated");
    for (auto scenario : {S::Clear, S::Legacy}) {
        Check(t.Execute({A::Reset, scenario}) && t.Execute({A::Step}), "clear controls");
        Check(t.Read().samples[0].rainReceived == 0 && t.Read().samples[1].rainReceived == 0, "rain in disabled/clear");
    }
    Check(t.Execute({A::ToggleLeft}) && t.Read().samples[0].intake, "toggle left");
    Check(t.Execute({A::ToggleRight}) && !t.Read().samples[1].intake, "toggle right");
    Check(!t.Execute({static_cast<A>(99)}), "invalid action accepted");
}
void SaveCompatibility() {
    Fixture f;
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDocumentSystem docs;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::path("generated/codex_checks") / ("weather_save_" + std::to_string(stamp));
    Check(docs.Initialize(root.string(), f.grid, f.economy, selection, &f.date, &f.progression), "document init");
    Check(f.date.RestoreSnapshot({17, 59.5f, 2}) && docs.SaveAs("WeatherTest", f.grid, f.economy, selection), "weather save");
    const auto id = docs.GetActiveDocumentId(), path = docs.GetPath();
    nlohmann::json saved;
    Check(JsonFile::Load(path, saved) && saved["schemaVersion"] == 25 && saved["weatherVersion"] == 1, "weather schema");
    for (int kind = 0; kind < 7; ++kind) {
        auto bad = saved;
        if (kind == 0) bad.erase("weatherVersion");
        if (kind == 1) bad["weatherVersion"] = -1;
        if (kind == 2) bad["weatherVersion"] = 4294967296ULL;
        if (kind == 3) bad["weatherVersion"] = true;
        if (kind == 4) bad["weatherVersion"] = 1.0;
        if (kind == 5) bad["weatherVersion"] = nullptr;
        if (kind == 6) bad["stagedUnlocks"] = false;
        Check(JsonFile::Save(path, bad) && !docs.Load(id, f.grid, f.economy, selection) &&
            f.date.GetDay() == 17 && f.progression.WeatherVersion() == 1, "invalid policy load changed farm");
    }
    auto legacy = saved; legacy["schemaVersion"] = 19; legacy.erase("weatherVersion");
    Check(JsonFile::Save(path, legacy) && docs.Load(id, f.grid, f.economy, selection) && f.progression.WeatherVersion() == 0, "legacy unexpectedly enabled rain");
    Check(JsonFile::Save(path, saved) && docs.Load(id, f.grid, f.economy, selection), "reload weather");
    for (int day : {17, 18, 19}) {
        Check(f.date.RestoreSnapshot({day, 20, 2}) && docs.Save(f.grid, f.economy, selection) &&
            docs.Load(id, f.grid, f.economy, selection) && f.date.GetDay() == day &&
            f.date.GetElapsedSecondsInDay() == 20 && f.progression.WeatherVersion() == 1, "weather round trip");
    }
}
}
int main() try {
    ScheduleAndViews(); WaterAccounting(); BoundariesAndAdvance(); DebugControls(); SaveCompatibility();
    std::cout << "farm_weather_test: PASS\n";
    return 0;
} catch (const std::exception& error) { std::cerr << "farm_weather_test: " << error.what() << '\n'; return 1; }
