#pragma once
#include "editor/EditorLocalization.h"
#include "editor/FarmWeatherPlaytestSystem.h"
#include "externals/imgui/imgui.h"
#include <optional>

namespace editor {
class FarmWeatherTestWindow final {
public:
    static std::optional<FarmWeatherPlaytestSystem::Request> Draw(bool& open, EditorLanguage language,
        const FarmWeatherPlaytestSystem::View& view, bool failed) {
        using Test = FarmWeatherPlaytestSystem;
        using A = Test::Action;
        const auto text = [language](const char* key) { return Localize(language, key); };
        std::optional<Test::Request> request;
        const auto* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos({viewport->WorkPos.x + 32, viewport->WorkPos.y + 32}, ImGuiCond_Appearing);
        ImGui::SetNextWindowSize({720, 600}, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSizeConstraints({650, 540}, {1200, 1000});
        if (ImGui::Begin(text("Weather Test###FarmWeatherTest"), &open)) {
            constexpr std::array cases{"Forecast boundary", "Rain starting", "Rain / intake comparison", "Rain ending", "Clear weather", "Legacy / weather disabled"};
            static_assert(cases.size() == static_cast<std::size_t>(Test::Scenario::Count));
            ImGui::TextWrapped("%s", text("Two isolated tomato farms. No current farm or save changes."));
            if (ImGui::BeginTable("WeatherCases", 3, ImGuiTableFlags_SizingStretchSame)) {
                for (int i = 0; i < static_cast<int>(cases.size()); ++i) {
                    ImGui::TableNextColumn();
                    if (ImGui::Button(text(cases[i]), {-1, 34})) request = Test::Request{A::Reset, static_cast<Test::Scenario>(i)};
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            const auto selected = static_cast<std::size_t>(view.scenario);
            if (selected < cases.size()) ImGui::TextUnformatted(text(cases[selected]));
            ImGui::Text(text("Weather day %d / %.2fs / tested %.1fs"), view.clock.day, view.clock.elapsedSecondsInDay, view.simulatedSeconds);
            using P = FarmWeatherSystem::Phase;
            const char* weather = view.weather.phase == P::Rain ? "Raining" : view.weather.phase == P::ForecastRain ?
                "Rain forecast for tomorrow" : view.weather.phase == P::Clear ? "Clear weather" : "Weather disabled";
            ImGui::TextUnformatted(text(weather));
            ImGui::TextUnformatted(text(view.boundaryStopped ? "Stopped: weather changed" :
                view.running ? "Weather test running" : "Weather test paused"));
            const auto button = [&](const char* label, A action, bool enabled) {
                ImGui::BeginDisabled(!enabled);
                if (ImGui::Button(text(label), {-1, 34})) request = Test::Request{action, view.scenario};
                ImGui::EndDisabled();
            };
            if (ImGui::BeginTable("WeatherControls", 3, ImGuiTableFlags_SizingStretchSame)) {
                ImGui::TableNextColumn(); button("Weather step 60 updates", A::Step, view.canStep && !view.running);
                ImGui::TableNextColumn(); button("Weather automatic", A::Start, view.canStep && !view.running);
                ImGui::TableNextColumn(); button("Weather pause", A::Pause, view.running);
                ImGui::TableNextColumn(); button("Weather speed 1x", A::Speed1, view.ready && !view.running);
                ImGui::TableNextColumn(); button("Weather speed 2x", A::Speed2, view.ready && !view.running);
                ImGui::TableNextColumn(); button("Weather speed 4x", A::Speed4, view.ready && !view.running);
                ImGui::EndTable();
            }
            ImGui::Text(text("Configured speed: %.0fx (no progress while paused)"), view.clock.timeScale);
            if (ImGui::BeginTable("WeatherComparison", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchSame)) {
                for (std::size_t i = 0; i < view.samples.size(); ++i) {
                    ImGui::TableNextColumn(); ImGui::PushID(static_cast<int>(i));
                    const auto& sample = view.samples[i];
                    ImGui::TextUnformatted(text(i == 0 ? "A: intake initially OFF" : "B: intake initially ON"));
                    ImGui::TextUnformatted(text(sample.intake ? "Intake ON" : "Intake OFF"));
                    button("Toggle soil intake", i == 0 ? A::ToggleLeft : A::ToggleRight, view.ready && !view.running);
                    ImGui::Text(text("Soil moisture: %.2f%%"), sample.moisture * 100);
                    ImGui::ProgressBar(sample.moisture, {-1, 14}, "");
                    ImGui::Text(text("Crop growth: %.2f%%"), sample.growth * 100);
                    ImGui::ProgressBar(sample.growth, {-1, 14}, "");
                    ImGui::Text(text("Rain received: %.2f pp"), sample.rainReceived * 100);
                    ImGui::Text(text("Canal received: %.2f pp"), sample.canalReceived * 100);
                    ImGui::Text(text("Excess water time: %.2fs"), sample.excessSeconds);
                    ImGui::Text(text("Current quality estimate: %d / 100"), sample.quality);
                    ImGui::TextUnformatted(text(sample.mature ? "Ready to harvest" : "Still growing"));
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
            ImGui::TextWrapped("%s", text("Rain and canal totals are separate inputs, not the current moisture. Quality is an estimate, not a contest score."));
            if (failed) ImGui::TextWrapped("%s", text("Weather request rejected"));
        }
        ImGui::End();
        if (!open) request = Test::Request{A::Pause};
        return request;
    }
};
}
