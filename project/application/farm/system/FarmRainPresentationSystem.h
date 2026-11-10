#pragma once
#include "farm/system/FarmWeatherSystem.h"
#include <algorithm>
#include <cmath>

namespace farm {
struct FarmRainFrame final {
    float rain = 0;
    float cloud = 0;
    double seconds = 0;
    [[nodiscard]] float LightScale() const noexcept { return 1.0f - .30f * cloud; }
};

// Presentation time is independent of the farm clock, including its pause and 4x modes.
class FarmRainPresentationSystem final {
public:
    static constexpr float kRainFadeSeconds = 2.4f;
    static constexpr float kCloudFadeSeconds = 3.5f;
    void Reset() noexcept { frame_ = {}; }
    void Update(FarmWeatherSystem::Phase phase, bool outdoor, float realSeconds) noexcept {
        if (!outdoor || phase == FarmWeatherSystem::Phase::Disabled) { Reset(); return; }
        const float dt = std::isfinite(realSeconds) ? std::clamp(realSeconds,0.0f,.1f) : 0;
        const bool rainy = phase == FarmWeatherSystem::Phase::Rain;
        const float cloud = rainy ? 1.0f : phase == FarmWeatherSystem::Phase::ForecastRain ? .25f : 0;
        frame_.rain = Approach(frame_.rain, rainy ? 1.0f : 0.0f,dt/kRainFadeSeconds);
        frame_.cloud = Approach(frame_.cloud,cloud,dt/kCloudFadeSeconds);
        frame_.seconds += dt;
    }
    [[nodiscard]] const FarmRainFrame& GetFrame() const noexcept { return frame_; }
private:
    static float Approach(float value, float target, float step) noexcept {
        return value + std::clamp(target-value,-step,step);
    }
    FarmRainFrame frame_{};
};
}
