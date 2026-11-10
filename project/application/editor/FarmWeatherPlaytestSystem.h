#pragma once
#include "farm/system/FarmSimulationStepSystem.h"
#include "farm/system/FarmCropQualitySystem.h"
#include <array>

namespace editor {
// Owns two separate farms; the only controlled difference is each soil's intake switch.
class FarmWeatherPlaytestSystem final {
public:
    enum class Scenario { Forecast, RainStart, Rain, RainEnd, Clear, Legacy, Count };
    enum class Action { Reset, Step, Start, Pause, ToggleLeft, ToggleRight, Speed1, Speed2, Speed4 };
    struct Request { Action action; Scenario scenario = Scenario::Rain; };
    struct Sample {
        float moisture = 0, growth = 0, excessSeconds = 0;
        double rainReceived = 0, canalReceived = 0;
        int quality = 0;
        bool intake = false, mature = false;
    };
    struct View {
        Scenario scenario = Scenario::Rain;
        FarmDateSystem::Snapshot clock{};
        FarmWeatherSystem::View weather{};
        std::array<Sample, 2> samples{};
        float simulatedSeconds = 0;
        bool ready = false, running = false, canStep = false, boundaryStopped = false;
    };
    FarmWeatherPlaytestSystem() = default;
    FarmWeatherPlaytestSystem(const FarmWeatherPlaytestSystem&) = delete;
    FarmWeatherPlaytestSystem& operator=(const FarmWeatherPlaytestSystem&) = delete;
    [[nodiscard]] View Read() const {
        View view;
        view.scenario = scenario_;
        view.ready = ready_;
        view.running = running_;
        view.canStep = ready_ && elapsed_ < kDurationLimit;
        view.boundaryStopped = boundaryStopped_;
        view.simulatedSeconds = elapsed_;
        if (!ready_) return view;
        view.clock = lanes_[0].date.CaptureSnapshot();
        view.weather = FarmWeatherSystem::Inspect(view.clock.day, lanes_[0].progression.WeatherVersion());
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            const auto& lane = lanes_[i];
            const auto* tile = lane.grid.GetTile(kCropTile);
            if (!tile) continue;
            view.samples[i] = {tile->moisture, tile->growth, tile->careHistory.excessSeconds,
                lane.rain, lane.canal, quality_.Evaluate(*tile).score, tile->irrigationEnabled, farm::IsHarvestReady(*tile)};
        }
        return view;
    }
    [[nodiscard]] bool Execute(Request request) {
        if (request.action == Action::Reset) return Reset(request.scenario);
        if (!ready_) return false;
        switch (request.action) {
        case Action::Step: if (running_ || elapsed_ >= kDurationLimit) return false; Batch(); return true;
        case Action::Start:
            if (running_ || elapsed_ >= kDurationLimit) return false;
            running_ = true; boundaryStopped_ = false; cadence_ = 0; return true;
        case Action::Pause: running_ = false; cadence_ = 0; return true;
        case Action::ToggleLeft: return Toggle(0);
        case Action::ToggleRight: return Toggle(1);
        case Action::Speed1: return Speed(1);
        case Action::Speed2: return Speed(2);
        case Action::Speed4: return Speed(4);
        default: return false;
        }
    }
    void Tick(float delta, bool blocked) {
        if (blocked) { running_ = false; cadence_ = 0; return; }
        if (!running_ || !std::isfinite(delta) || delta <= 0) return;
        cadence_ += (std::min)(delta, 0.25f);
        if (cadence_ >= kBatchInterval) { cadence_ = 0; Batch(); }
    }
private:
    static constexpr int kCropTile = 2, kStepsPerBatch = 60;
    static constexpr float kStepSeconds = 1.0f / 60.0f, kDurationLimit = 60.0f, kBatchInterval = 0.1f;
    struct Lane {
        farm::FarmGrid grid;
        farm::FarmIrrigationSystem irrigation;
        FarmGrowthSystem growth;
        FarmDateSystem date;
        FarmContestDaySystem contest;
        FarmEconomySystem economy;
        FarmProgressionSystem progression;
        double rain = 0, canal = 0;
        FarmSimulationStepSystem::Context Context() { return {grid, irrigation, growth, date, contest, economy, progression}; }
    };
    bool Reset(Scenario scenario) {
        if (scenario < Scenario::Forecast || scenario >= Scenario::Count) return false;
        running_ = ready_ = boundaryStopped_ = false;
        elapsed_ = cadence_ = 0;
        scenario_ = scenario;
        quality_.Initialize();
        for (std::size_t i = 0; i < lanes_.size(); ++i) {
            auto& lane = lanes_[i];
            if (!lane.grid.Initialize(3, 1)) return false;
            lane.irrigation.Initialize(); lane.growth.Initialize(); lane.date.Initialize();
            lane.contest.Reset(); lane.economy.Initialize();
            lane.progression.Initialize({}, FarmProgressionMode::ContestSeason, false, scenario != Scenario::Legacy);
            const int day = scenario == Scenario::Forecast ? 16 : scenario == Scenario::RainStart ? 17 :
                scenario == Scenario::Clear ? 15 : 18;
            const float seconds = scenario == Scenario::Forecast || scenario == Scenario::RainStart || scenario == Scenario::RainEnd ? 59.5f : 0.0f;
            if (!lane.date.RestoreSnapshot({day, seconds, 1})) return false;
            farm::FarmTile source;
            source.feature = farm::FarmTileFeature::WaterSource; source.heightLevel = 2; source.waterAmount = 1;
            farm::FarmTile canal;
            canal.feature = farm::FarmTileFeature::Canal; canal.heightLevel = 1; canal.waterAmount = 1;
            farm::FarmTile crop;
            crop.state = farm::FarmTileState::Planted; crop.crop = farm::CropType::Tomato;
            crop.heightLevel = 1; crop.moisture = 0.55f; crop.growth = 0.05f;
            crop.irrigationEnabled = i != 0;
            if (!lane.grid.SetTile(0, source) || !lane.grid.SetTile(1, canal) || !lane.grid.SetTile(kCropTile, crop)) return false;
            lane.rain = lane.canal = 0;
        }
        ready_ = true;
        return true;
    }
    bool Toggle(std::size_t index) {
        if (running_ || index >= lanes_.size()) return false;
        auto& grid = lanes_[index].grid;
        const auto* current = grid.GetTile(kCropTile);
        if (!current) return false;
        auto tile = *current;
        tile.irrigationEnabled = !tile.irrigationEnabled;
        return grid.SetTile(kCropTile, tile);
    }
    bool Speed(float speed) {
        if (running_) return false;
        for (auto& lane : lanes_) lane.date.SetTimeScale(speed);
        return true;
    }
    void Batch() {
        boundaryStopped_ = false;
        for (int step = 0; step < kStepsPerBatch && elapsed_ < kDurationLimit; ++step) {
            const auto before = lanes_[0].date.CaptureSnapshot();
            const float delta = (std::min)(kStepSeconds, (kDurationLimit - elapsed_) / before.timeScale);
            bool advanced = true, weatherChanged = false;
            for (auto& lane : lanes_) {
                const auto result = FarmSimulationStepSystem::Step(lane.Context(), delta);
                advanced &= result.advanced;
                weatherChanged |= result.weatherChanged;
                if (result.advanced) {
                    lane.rain += result.rainDelivered;
                    lane.canal += result.canalDelivered;
                }
            }
            if (!advanced) { running_ = false; return; }
            const auto after = lanes_[0].date.CaptureSnapshot();
            elapsed_ += (after.day - before.day) * lanes_[0].date.GetDayLengthSeconds() + after.elapsedSecondsInDay - before.elapsedSecondsInDay;
            if (weatherChanged) { running_ = false; boundaryStopped_ = true; return; }
        }
        if (elapsed_ >= kDurationLimit) running_ = false;
    }
    std::array<Lane, 2> lanes_{};
    FarmCropQualitySystem quality_;
    Scenario scenario_ = Scenario::Rain;
    float elapsed_ = 0, cadence_ = 0;
    bool ready_ = false, running_ = false, boundaryStopped_ = false;
};
}
