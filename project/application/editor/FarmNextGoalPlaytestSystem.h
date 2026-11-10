#pragma once

#include "farm/system/FarmNextGoalSystem.h"
#include "farm/system/FarmToolActionSystem.h"

#include <array>
#include <optional>
#include <utility>

namespace editor {

// Owns a one-tile sample, never a scene/document. Commands may borrow its grid/economy.
class FarmNextGoalPlaytestSystem final {
public:
    enum class Action { Restart, NoFunds, Till, BuySeed, Plant, Water, TogglePause, Grow, Harvest, Sell, Count };
    static constexpr auto kActionCount = static_cast<std::size_t>(Action::Count);
    static constexpr int kGrowthSeconds = 5;

    struct View {
        FarmNextGoal goal{};
        farm::FarmTileState state = farm::FarmTileState::Empty;
        int money = 0;
        int seeds = 0;
        int harvests = 0;
        float moisture = 0.0f;
        float growth = 0.0f;
        bool active = false;
        bool paused = false;
        std::optional<Action> recommended;
        std::array<bool, kActionCount> enabled{};

        [[nodiscard]] bool Can(Action action) const noexcept {
            const auto index = static_cast<std::size_t>(action);
            return index < enabled.size() && enabled[index];
        }
    };

    FarmNextGoalPlaytestSystem() = default;
    FarmNextGoalPlaytestSystem(const FarmNextGoalPlaytestSystem&) = delete;
    FarmNextGoalPlaytestSystem& operator=(const FarmNextGoalPlaytestSystem&) = delete;

    [[nodiscard]] View Read() const noexcept {
        View view;
        const auto enable = [&](Action action, bool enabled) { view.enabled[static_cast<std::size_t>(action)] = enabled; };
        enable(Action::Restart, true);
        enable(Action::NoFunds, true);
        const auto* tile = grid_.GetSelectedTile();
        if (!tile) return view;
        view.active = true;
        view.state = tile->state;
        view.money = economy_.GetMoney();
        view.seeds = economy_.GetSeedCount(kCrop);
        view.harvests = economy_.GetTotalCropCount();
        view.moisture = tile->moisture;
        view.growth = tile->growth;
        view.paused = paused_;
        const auto forecast = growth_.Evaluate(*tile, kCrop);
        const auto water = FarmGrowthSystem::AnalyzeWater(tile, forecast, farm::FarmWaterStatus::None);
        FarmNextGoalContext context;
        context.waterAdvice = water.advice;
        context.selectedCrop = kCrop;
        context.paused = paused_;
        view.goal = FarmNextGoalSystem::Evaluate(FarmNextGoalSystem::Inspect(grid_, economy_, context));
        const auto canTool = [&](FarmTool tool) { return tools_.EvaluateTool(grid_, tool, kCrop, &economy_).Succeeded(); };
        enable(Action::Till, canTool(FarmTool::Hoe));
        enable(Action::Plant, canTool(FarmTool::Seed));
        enable(Action::Water, canTool(FarmTool::Water));
        enable(Action::Harvest, canTool(FarmTool::Harvest));
        enable(Action::BuySeed, economy_.GetSeedPrice(kCrop) > 0 && view.money >= economy_.GetSeedPrice(kCrop));
        enable(Action::Sell, economy_.PreviewSale().Succeeded());
        enable(Action::TogglePause, true);
        enable(Action::Grow, !paused_ && tile->state == farm::FarmTileState::Planted && !farm::IsHarvestReady(*tile));
        const auto next = Recommend(view.goal.kind);
        if (next && view.Can(*next)) view.recommended = next;
        return view;
    }

    [[nodiscard]] bool Execute(Action action) {
        if (!Read().Can(action)) return false;
        switch (action) {
        case Action::Restart: return Restart(false);
        case Action::NoFunds: return Restart(true);
        case Action::Till: return Apply(FarmTool::Hoe);
        case Action::Plant: return Apply(FarmTool::Seed);
        case Action::Water: return Apply(FarmTool::Water);
        case Action::Harvest: return Apply(FarmTool::Harvest);
        case Action::BuySeed: return economy_.BuySeed(kCrop).Succeeded();
        case Action::Sell: return economy_.SellAll().Succeeded();
        case Action::TogglePause: paused_ = !paused_; return true;
        case Action::Grow:
            // Growth-only sample, not the session's day/event time-advance path.
            tools_.ClearHistory();
            for (int step = 0; step < kGrowthSteps; ++step)
                static_cast<void>(growth_.Update(grid_, kGrowthTickSeconds, 1.0f));
            return true;
        default: return false;
        }
    }

private:
    static std::optional<Action> Recommend(FarmNextGoalKind kind) noexcept {
        switch (kind) {
        case FarmNextGoalKind::Till: return Action::Till;
        case FarmNextGoalKind::BuySeed: return Action::BuySeed;
        case FarmNextGoalKind::Plant: return Action::Plant;
        case FarmNextGoalKind::Water: return Action::Water;
        case FarmNextGoalKind::Grow: return Action::Grow;
        case FarmNextGoalKind::ResumeGrowth: return Action::TogglePause;
        case FarmNextGoalKind::Harvest: return Action::Harvest;
        case FarmNextGoalKind::SellForSeeds: return Action::Sell;
        default: return std::nullopt;
        }
    }

    static constexpr auto kCrop = farm::CropType::Carrot;
    static constexpr float kGrowthTickSeconds = farm::FarmRules{}.maxUpdateDeltaTime;
    static constexpr int kGrowthSteps = static_cast<int>(kGrowthSeconds / kGrowthTickSeconds);

    bool Restart(bool noFunds) {
        farm::FarmGrid sample;
        if (!sample.Initialize(1, 1)) return false;
        if (noFunds) {
            farm::FarmTile tile;
            tile.state = farm::FarmTileState::Tilled;
            if (!sample.SetTile(0, tile)) return false;
        }
        farm::FarmRules rules;
        rules.initialMoney = noFunds ? 0 : rules.carrotSeedPrice;
        rules.initialTestCropSeedCount = rules.initialCarrotSeedCount =
            rules.initialTomatoSeedCount = rules.initialPumpkinSeedCount = 0;
        // Discard commands borrowing the previous sample before replacing its state.
        tools_.ClearHistory();
        grid_ = std::move(sample);
        economy_.Initialize(rules);
        growth_.Initialize(rules);
        tools_.Initialize(rules);
        paused_ = false;
        return true;
    }

    bool Apply(FarmTool tool) {
        return tools_.ApplyToolDetailed(grid_, tool, kCrop, economy_, 1).Succeeded();
    }

    farm::FarmGrid grid_;
    FarmEconomySystem economy_;
    FarmGrowthSystem growth_;
    FarmToolActionSystem tools_;
    bool paused_ = false;
};

} // namespace editor
