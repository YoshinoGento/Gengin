#pragma once

#include "farm/system/FarmTimeAdvanceSystem.h"

namespace editor {

// Owns isolated deterministic samples. No document, save file or command history is borrowed.
class FarmTimeAdvancePlaytestSystem final {
public:
    enum class Scenario { Pest, Harvest, Dry, Excess, Contest, Empty, Count };
    enum class Action { Reset, Start, Step, Cancel };
    struct Request { Action action; Scenario scenario = Scenario::Pest; };
    struct View {
        Scenario scenario = Scenario::Pest;
        FarmTimeAdvanceSystem::Status status{};
        FarmDateSystem::Snapshot clock{};
        float moisture = 0.0f;
        float growth = 0.0f;
        int warningDay = 0;
        int damageDays = 0;
        bool ready = false;
        bool running = false;
    };

    FarmTimeAdvancePlaytestSystem() = default;
    FarmTimeAdvancePlaytestSystem(const FarmTimeAdvancePlaytestSystem&) = delete;
    FarmTimeAdvancePlaytestSystem& operator=(const FarmTimeAdvancePlaytestSystem&) = delete;

    [[nodiscard]] View Read() const noexcept {
        View view;
        view.scenario = scenario_;
        view.status = advance_.GetStatus();
        view.clock = date_.CaptureSnapshot();
        view.ready = ready_;
        view.running = running_;
        if (const auto* tile = grid_.GetTile(0)) {
            view.moisture = tile->moisture;
            view.growth = tile->growth;
            view.warningDay = tile->pests.warningDay;
            view.damageDays = tile->pests.damageDays;
        }
        return view;
    }

    [[nodiscard]] bool Execute(Request request) {
        if (request.action == Action::Reset) return Reset(request.scenario);
        if (!ready_) return false;
        switch (request.action) {
        case Action::Start:
            if (advance_.GetStatus().active) return false;
            running_ = advance_.Start(Context(), false);
            return true; // A refused start is a test result, not a command failure.
        case Action::Step:
            if (running_) return false;
            if (!advance_.GetStatus().active) static_cast<void>(advance_.Start(Context(), false));
            static_cast<void>(advance_.AdvanceBatch(Context(), false));
            return true;
        case Action::Cancel:
            if (!advance_.GetStatus().active) return false;
            advance_.Cancel();
            running_ = false;
            return true;
        default: return false;
        }
    }

    // Once per editor frame, never from the UI. Hidden or unfocused tests cannot advance.
    void Tick(bool blocked) {
        if (blocked && advance_.GetStatus().active) {
            static_cast<void>(advance_.AdvanceBatch(Context(), true));
            running_ = false;
        } else if (running_) {
            static_cast<void>(advance_.AdvanceBatch(Context(), false));
            running_ = advance_.GetStatus().active;
        }
    }

private:
    bool Reset(Scenario scenario) {
        if (scenario < Scenario::Pest || scenario >= Scenario::Count) return false;
        advance_.Reset();
        running_ = ready_ = false;
        if (!grid_.Initialize(1, 1)) return false;
        irrigation_.Initialize();
        growth_.Initialize();
        economy_.Initialize();
        date_.Initialize();
        contest_.Reset();
        progression_.Initialize({}, FarmProgressionMode::ContestSeason, true);
        farm::FarmTile tile;
        if (scenario != Scenario::Empty && scenario != Scenario::Contest) {
            tile.crop = farm::CropType::Carrot;
            tile.state = farm::FarmTileState::Planted;
            tile.moisture = kGoodWater;
            tile.growth = kInitialGrowth;
        }
        switch (scenario) {
        case Scenario::Pest:
            if (!date_.RestoreSnapshot({farm::FarmPestState::kFirstWarningDay - 1, kNearDayEnd, 1.0f})) return false;
            break;
        case Scenario::Harvest: tile.growth = kNearHarvest; break;
        case Scenario::Dry: tile.moisture = 0.0f; break;
        case Scenario::Excess: tile.moisture = 1.0f; break;
        case Scenario::Contest:
            if (!date_.RestoreSnapshot({FarmContestEntrySystem::kContestDays.front() - 1, kNearDayEnd, 1.0f})) return false;
            break;
        default: break;
        }
        if (!grid_.SetTile(0, tile)) return false;
        scenario_ = scenario;
        ready_ = true;
        return true;
    }

    FarmTimeAdvanceSystem::Context Context() {
        return {grid_, irrigation_, growth_, date_, contest_, economy_, progression_};
    }
    // Fixture values, not production balance parameters.
    static constexpr float kGoodWater = 0.55f;
    static constexpr float kInitialGrowth = 0.2f;
    static constexpr float kNearHarvest = 0.9999f;
    static constexpr float kNearDayEnd = 59.99f;
    farm::FarmGrid grid_;
    farm::FarmIrrigationSystem irrigation_;
    FarmGrowthSystem growth_;
    FarmDateSystem date_;
    FarmContestDaySystem contest_;
    FarmEconomySystem economy_;
    FarmProgressionSystem progression_;
    FarmTimeAdvanceSystem advance_;
    Scenario scenario_ = Scenario::Pest;
    bool ready_ = false;
    bool running_ = false;
};

} // namespace editor
