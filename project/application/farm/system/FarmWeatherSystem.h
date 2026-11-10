#pragma once
#include <array>

// Version + saved day fully determine the schedule; opening a view never rerolls it.
class FarmWeatherSystem final {
public:
    enum class Phase { Disabled, Clear, ForecastRain, Rain };
    struct View {
        Phase phase = Phase::Disabled;
        int rainDay = 0;
        float soilRainPerSecond = 0.0f;
    };
    static constexpr int kRulesVersion = 1;
    static constexpr std::array kRainDays{8, 18, 28};
    static constexpr float kSoilRainPerSecond = 0.018f;
    // Effective infiltration limit, not surface flooding or canal reservoir volume.
    static constexpr float kRainMoistureLimit = 0.65f;

    [[nodiscard]] static bool ValidVersion(int version) noexcept { return version == 0 || version == kRulesVersion; }
    [[nodiscard]] static View Inspect(int day, int version) noexcept {
        if (version != kRulesVersion || day < 1) return {};
        for (int rainDay : kRainDays) {
            if (day == rainDay) return {Phase::Rain, rainDay, kSoilRainPerSecond};
            if (day == rainDay - 1) return {Phase::ForecastRain, rainDay, 0.0f};
        }
        return {Phase::Clear, 0, 0.0f};
    }
    [[nodiscard]] static bool Changed(int before, int after, int version) noexcept {
        return Inspect(before, version).phase != Inspect(after, version).phase;
    }
};
