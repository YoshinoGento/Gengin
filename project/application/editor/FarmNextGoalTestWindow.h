#pragma once

#include "editor/EditorLocalization.h"
#include "editor/FarmNextGoalDebugCases.h"
#include "editor/FarmNextGoalPlaytestSystem.h"
#include "farm/ui/FarmNextGoalView.h"
#include "externals/imgui/imgui.h"

#include <iterator>
#include <optional>

namespace editor {

class FarmNextGoalTestWindow final {
public:
    [[nodiscard]] std::optional<FarmNextGoalPlaytestSystem::Action> Draw(bool& open, EditorLanguage language,
        const FarmNextGoalPlaytestSystem::View& view, bool failed) {
        using Test = FarmNextGoalPlaytestSystem;
        using Action = Test::Action;
        std::optional<Action> request;
        const auto text = [language](std::string_view key) { return Localize(language, key); };
        const auto showGoal = [&](const FarmNextGoal& goal) {
            if (const auto copy = farmui::DescribeFarmNextGoal(goal.kind)) {
                ImGui::TextWrapped("%s", farmui::FarmGoalDebugText(copy->title));
                if (goal.contestDay > 0) ImGui::Text("D%d", goal.contestDay);
                if (copy->detail) ImGui::TextWrapped("%s", farmui::FarmGoalDebugText(*copy->detail));
            } else ImGui::TextUnformatted(text("No next goal displayed"));
        };
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({viewport->WorkPos.x + 24.0f, viewport->WorkPos.y + 24.0f}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({560.0f, 590.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({520.0f, 350.0f}, {1000.0f, 1000.0f});
        if (ImGui::Begin(text("Next Goal Test###FarmNextGoalTest"), &open) && ImGui::BeginTabBar("##GoalTabs")) {
            if (ImGui::BeginTabItem(text("Goal fixed cases"))) {
                const auto& cases = kFarmNextGoalDebugCases;
                ImGui::SetNextItemWidth(-1.0f);
                if (ImGui::BeginCombo("##FarmGoalCase", text(cases[caseIndex_].label))) {
                    for (std::size_t i = 0; i < std::size(cases); ++i) {
                        if (ImGui::Selectable(text(cases[i].label), i == caseIndex_)) caseIndex_ = i;
                        if (i == caseIndex_) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::Separator();
                showGoal(FarmNextGoalSystem::Evaluate(cases[caseIndex_].facts));
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem(text("Goal action test"), nullptr, firstDraw_ ? ImGuiTabItemFlags_SetSelected : 0)) {
                firstDraw_ = false;
                ImGui::TextWrapped("%s", text("Sample carrot only. The current farm and saves are unchanged."));
                const auto button = [&](const char* label, Action action, float height = 0.0f) {
                    ImGui::BeginDisabled(!view.Can(action));
                    if (ImGui::Button(text(label), {-1.0f, height})) request = action;
                    ImGui::EndDisabled();
                };
                if (!view.active) button("Restart goal sample", Action::Restart, 36.0f);
                if (view.active) {
                    ImGui::Separator();
                    // Keep action buttons stationary when advice wraps or has no detail.
                    if (ImGui::BeginChild("##ActionGoal", {0.0f, ImGui::GetTextLineHeightWithSpacing() * 4.0f}))
                        showGoal(view.goal);
                    ImGui::EndChild();
                    ImGui::Text(text("Sample money %d / seeds %d / harvests %d"), view.money, view.seeds, view.harvests);
                    ImGui::Text(text("Sample moisture %.0f%% / growth %.0f%%"), view.moisture * 100.0f, view.growth * 100.0f);
                    ImGui::Separator();
                    ImGui::TextUnformatted(text("Recommended sample action"));
                    ImGui::PushID("Recommended");
                    if (view.recommended) button(ActionLabel(*view.recommended, view.paused), *view.recommended, 36.0f);
                    else {
                        ImGui::BeginDisabled();
                        ImGui::Button(text("No recommended sample action"), {-1.0f, 36.0f});
                        ImGui::EndDisabled();
                    }
                    ImGui::PopID();
                    ImGui::Separator();
                    if (ImGui::BeginTable("##SampleActions", 2, ImGuiTableFlags_SizingStretchSame)) {
                        for (const auto action : {Action::Till, Action::BuySeed, Action::Plant, Action::Water,
                                Action::TogglePause, Action::Grow, Action::Harvest, Action::Sell}) {
                            ImGui::TableNextColumn();
                            button(ActionLabel(action, view.paused), action, 30.0f);
                        }
                        ImGui::EndTable();
                    }
                    ImGui::TextWrapped("%s", text("Growth-only sample. Days, pests and contests do not advance."));
                    if (ImGui::CollapsingHeader(text("Sample setup"))) {
                        button("Restart goal sample", Action::Restart);
                        button("Goal sample without funds", Action::NoFunds);
                    }
                }
                if (failed) ImGui::TextWrapped("%s", text("Goal sample action failed. Restart the sample."));
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::End();
        return request;
    }

private:
    static const char* ActionLabel(FarmNextGoalPlaytestSystem::Action action, bool paused) noexcept {
        using Action = FarmNextGoalPlaytestSystem::Action;
        switch (action) {
        case Action::Till: return "Till sample soil";
        case Action::BuySeed: return "Buy one sample seed";
        case Action::Plant: return "Plant sample carrot";
        case Action::Water: return "Water sample carrot";
        case Action::TogglePause: return paused ? "Resume sample growth" : "Pause sample growth";
        case Action::Grow: return "Step sample growth five seconds";
        case Action::Harvest: return "Harvest sample carrot";
        case Action::Sell: return "Sell sample harvest";
        default: return "No recommended sample action";
        }
    }

    std::size_t caseIndex_ = 0;
    bool firstDraw_ = true;
};

} // namespace editor
