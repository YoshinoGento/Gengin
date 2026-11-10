#pragma once
#include "editor/EditorLocalization.h"
#include "editor/GamePlayEditorBridge.h"
#include "farm/ui/FarmTimeAdvanceView.h"
#include "externals/imgui/imgui.h"
#include <optional>

namespace editor {
// Displays the active document; all simulation commands go through the bridge.
class FarmTimeAdvanceLiveWindow final {
public:
    static std::optional<FarmTimeAdvanceControlSystem::Action> Draw(bool& open,
        EditorLanguage language, const GamePlayEditorViewModel& view) {
        using A = FarmTimeAdvanceControlSystem::Action;
        std::optional<A> action;
        const auto text = [language](std::string_view key) { return Localize(language, key); };
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({viewport->WorkPos.x + 36, viewport->WorkPos.y + 36}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({520, 400}, ImGuiCond_FirstUseEver);
        if (ImGui::Begin(text("Current Farm Time###FarmTimeLive"), &open)) {
            const auto& state = view.farmTimeAdvance;
            ImGui::TextWrapped("%s", text("Live farm. Changes stay in memory until saved."));
            const auto label = farmui::TimeAdvanceLabel(state);
            const auto labelIndex = static_cast<std::size_t>(label) - static_cast<std::size_t>(farmui::Label::PestWarning);
            if (state.held && labelIndex < farmui::kGoalDebugText.size())
                ImGui::TextUnformatted(reinterpret_cast<const char*>(farmui::kGoalDebugText[labelIndex].data()));
            else ImGui::TextUnformatted(text("Normal farm simulation"));
            const auto& clock = view.farmPlaytest.clock;
            ImGui::Text(text("Test day %d / %.2f sec / steps %d"), clock.day, clock.elapsedSecondsInDay, state.status.completedSteps);
            ImGui::Text(text("Target day %d / tile %d / dirty %d"), state.status.targetDay, state.status.tileIndex, view.farmDocumentDirty ? 1 : 0);
            // Keep command positions stable when a blocking explanation appears.
            ImGui::BeginChild("BlockedHint", {0, ImGui::GetTextLineHeightWithSpacing() * 2.0f},
                ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
            if (view.farmTimeAdvanceBlocked) ImGui::TextWrapped("%s", text("Close menus or previews and focus the game."));
            ImGui::EndChild();
            ImGui::Separator();
            const auto button = [&](const char* labelText, A request, bool enabled) {
                ImGui::BeginDisabled(!enabled);
                if (ImGui::Button(text(labelText), {-1, 34})) action = request;
                ImGui::EndDisabled();
            };
            button("Start safe advance", A::Start, !state.automatic && !view.farmTimeAdvanceBlocked);
            button("Advance one bounded batch", A::Step, !state.automatic && !view.farmTimeAdvanceBlocked);
            button("Cancel safe advance", A::Cancel, state.status.active);
            button("Focus stopped tile", A::Focus, state.status.tileIndex >= 0);
            button("Resume normal farm time", A::Resume, state.held);
            if (state.status.active && !state.automatic)
                ImGui::TextWrapped("%s", text("Time advance waiting for next batch"));
        }
        ImGui::End();
        if (!open) action = A::Cancel;
        return action;
    }
};
}
