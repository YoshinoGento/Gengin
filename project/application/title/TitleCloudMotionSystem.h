#pragma once
#include "math/Struct.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace title {

// Shared visual state only; no saved farm, scene navigation or rendering ownership.
class TitleCloudMotionSystem final {
public:
    void Reset(std::uint32_t seed = 0x5A17u) noexcept {
        offset_ = velocity_ = target_ = {};
        remaining_ = 0;
        seed_ = seed;
    }
    void Update(float delta) noexcept {
        if (!std::isfinite(delta) || delta <= 0) return;
        delta = (std::min)(delta, kMaximumDelta);
        remaining_ -= delta;
        if (remaining_ <= 0) {
            remaining_ = 8.f + NextValue()*6.f;
            target_ = {(NextValue()-.35f)*.003f, (NextValue()-.5f)*.00035f};
        }
        const float response = 1.f-std::exp(-delta*.45f);
        velocity_.x += (target_.x-velocity_.x)*response;
        velocity_.y += (target_.y-velocity_.y)*response;
        offset_.x = std::fmod(offset_.x+velocity_.x*delta+1.f,1.f);
        offset_.y = std::clamp(offset_.y+velocity_.y*delta,-.025f,.025f);
    }
    [[nodiscard]] Vector2 Offset() const noexcept { return offset_; }
    [[nodiscard]] Vector2 Velocity() const noexcept { return velocity_; }
private:
    static constexpr float kMaximumDelta = .25f;
    float NextValue() noexcept {
        seed_ = seed_*1664525u+1013904223u;
        return static_cast<float>(seed_ >> 8)/16777216.f;
    }
    Vector2 offset_{}, velocity_{}, target_{};
    float remaining_ = 0;
    std::uint32_t seed_ = 0x5A17u;
};
}
