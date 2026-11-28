#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace farm {
enum class PullButton { None, Mouse, Keyboard };
struct PullInput { bool pressed{}, down{}; };
struct CarrotGrip { int tile=-1; float tension{}, yaw{}; };

// Only the button that grabbed this ripe root can release it. No grid mutation here.
class FarmCarrotPull {
public:
    void Cancel() noexcept { *this={}; }
    bool Step(int ripeTile, uint64_t generation, PullInput mouse, PullInput keyboard, float dt) noexcept {
        if (button_!=PullButton::None && (tile_!=ripeTile || generation_!=generation)) {
            Cancel();
            return false;
        }
        if (button_==PullButton::None) {
            if (ripeTile<0 || (!mouse.pressed && !keyboard.pressed)) return false;
            button_=mouse.pressed?PullButton::Mouse:PullButton::Keyboard;
            tile_=ripeTile;
            generation_=generation;
        }
        if (!(button_==PullButton::Mouse?mouse:keyboard).down) { Cancel(); return true; }
        if (std::isfinite(dt)) age_=(std::min)(kFullPull,age_+(std::max)(0.f,dt));
        return false;
    }
    CarrotGrip Pose(float yaw) const noexcept {
        constexpr float kTapWindow=.12f;
        const float t=std::clamp((age_-kTapWindow)/(kFullPull-kTapWindow),0.f,1.f);
        const float tension=t*(2.f-t);
        // The narrowest soil split must have a non-singular normal transform.
        constexpr float kVisibleTension=.02f;
        return {tile_,tension>=kVisibleTension?tension:0.f,yaw};
    }
private:
    static constexpr float kFullPull=.62f;
    PullButton button_=PullButton::None;
    int tile_=-1;
    uint64_t generation_{};
    float age_{};
};
}
