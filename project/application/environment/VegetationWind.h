#pragma once
#include "math/Struct.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace vegetation {
inline constexpr float kAzimuth = .45f;
inline constexpr float kSpatialFrequency = .85f;
inline constexpr float kCrownResponse = .072f;
inline Vector2 WaveAxis() noexcept { return {std::cos(kAzimuth), std::sin(kAzimuth)}; }

struct WindFrame {
    Vector2 bend{};
    float phase = 0;

    // Same travelling crest as TitleGrass.VS, without the per-tuft phase offset.
    Vector3 Tilt(Vector3 anchor, float response) const noexcept {
        if (response == 0) return {};
        const auto axis = WaveAxis();
        const float crest = .5f + .5f * std::sin(phase - kSpatialFrequency*(anchor.x*axis.x + anchor.z*axis.y));
        const float amount = response*(.08f + .92f*crest*crest);
        return {bend.y*amount, 0, -bend.x*amount};
    }
};

class FarmWindSystem {
public:
    void Update(float seconds, float cloud) noexcept {
        constexpr float kClearBend = .32f, kOvercastBend = .54f, kRadiansPerSecond = 1.35f;
        const float dt = std::isfinite(seconds) ? std::clamp(seconds, 0.f, .1f) : 0;
        const float cover = std::isfinite(cloud) ? std::clamp(cloud, 0.f, 1.f) : 0;
        const float strength = kClearBend + (kOvercastBend-kClearBend)*cover;
        const auto axis = WaveAxis();
        frame_.bend = {axis.x*strength,axis.y*strength};
        frame_.phase = std::fmod(frame_.phase + dt*kRadiansPerSecond, 2*std::numbers::pi_v<float>);
    }
    const WindFrame& GetFrame() const noexcept { return frame_; }
private:
    WindFrame frame_{};
};
}
