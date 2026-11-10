#pragma once

#include "editor/EditorLocalization.h"
#include "editor/FarmTimeAdvancePlaytestSystem.h"
#include "externals/imgui/imgui.h"
#include <array>
#include <optional>

namespace editor {

class FarmTimeAdvanceTestWindow final {
public:
    [[nodiscard]] std::optional<FarmTimeAdvancePlaytestSystem::Request> Draw(bool& open,
        EditorLanguage language, const FarmTimeAdvancePlaytestSystem::View& view, bool failed) {
        using Test = FarmTimeAdvancePlaytestSystem;
        using Action = Test::Action;
        std::optional<Test::Request> request;
        const auto text = [language](std::string_view key) { return Localize(language, key); };
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({viewport->WorkPos.x + 32.0f, viewport->WorkPos.y + 32.0f}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({560.0f, 480.0f}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({520.0f, 400.0f}, {1000.0f, 1000.0f});
        if (ImGui::Begin(text("Time Advance Test###FarmTimeAdvanceTest"), &open)) {
            constexpr std::array labels = {"Pest boundary", "Ready to harvest", "Dry crop", "Excess water", "Contest boundary", "Empty farm / cancel"};
            static_assert(labels.size() == static_cast<std::size_t>(Test::Scenario::Count));
            ImGui::TextWrapped("%s", text("Isolated time test. The current farm and saves are unchanged."));
            if (ImGui::BeginTable("##TimeCases", 3, ImGuiTableFlags_SizingStretchSame)) {
                for (int i = 0; i < static_cast<int>(Test::Scenario::Count); ++i) {
                    ImGui::TableNextColumn();
                    if (ImGui::Button(text(labels[i]), {-1.0f, 32.0f}))
                        request = Test::Request{Action::Reset, static_cast<Test::Scenario>(i)};
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            const int selected = static_cast<int>(view.scenario);
            if (view.ready && selected >= 0 && selected < static_cast<int>(labels.size()))
                ImGui::Text(text("Selected time case: %s"), text(labels[selected]));
            const char* status = view.status.active ?
                (view.running ? "Time advance running" : "Time advance waiting for next batch") :
                (view.ready && view.status.reason == FarmTimeAdvanceSystem::StopReason::None ?
                    "Time case ready to start" : Reason(view.status.reason));
            ImGui::TextUnformatted(text(status));
            ImGui::Text(text("Test day %d / %.2f sec / steps %d"), view.clock.day,
                view.clock.elapsedSecondsInDay, view.status.completedSteps);
            ImGui::Text(text("Time sample moisture %.2f%% / growth %.2f%%"), view.moisture * 100.0f, view.growth * 100.0f);
            ImGui::Text(text("Pest warning day %d / damage days %d"), view.warningDay, view.damageDays);
            ImGui::Separator();
            const auto button = [&](const char* label, Action action, bool enabled) {
                ImGui::BeginDisabled(!enabled);
                if (ImGui::Button(text(label), {-1.0f, 34.0f})) request = Test::Request{action, view.scenario};
                ImGui::EndDisabled();
            };
            button("Start safe advance", Action::Start, view.ready && !view.status.active);
            button("Advance one bounded batch", Action::Step, view.ready && !view.running);
            button("Cancel safe advance", Action::Cancel, view.status.active);
            button("Reset this time case", Action::Reset, view.ready);
            if (failed) ImGui::TextWrapped("%s", text("Time test request rejected"));
        }
        ImGui::End();
        return request;
    }

private:
    static const char* Reason(FarmTimeAdvanceSystem::StopReason reason) noexcept {
        using R = FarmTimeAdvanceSystem::StopReason;
        switch (reason) {
        case R::None: return "Select a time test case";
        case R::Cancelled: return "Stopped: cancelled";
        case R::Blocked: return "Stopped: window hidden or focus lost";
        case R::HarvestReady: return "Stopped: harvest ready";
        case R::WaterLow: return "Stopped: water required";
        case R::WaterExcess: return "Stopped: too much water";
        case R::ContestDay: return "Stopped: contest day";
        case R::PestWarning: return "Stopped: pest warning (no skipped damage day)";
        case R::WeatherChanged: return "Stopped: weather changed";
        case R::Finished: return "Stopped: season finished";
        case R::NoTarget: return "Stopped: no target day";
        case R::BudgetLimit: return "Stopped: step budget reached";
        case R::TargetReached: return "Stopped: target reached";
        case R::ContextChanged: return "Stopped: context changed";
        default: return "Stopped: invalid state";
        }
    }
};

} // namespace editor
