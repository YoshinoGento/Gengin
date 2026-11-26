#pragma once
#include <chrono>
#include <cstdint>

class Input;

namespace farm {
enum class FieldWheelRoute { Tools, SeedKit, SeedPicker };
[[nodiscard]] constexpr FieldWheelRoute ResolveFieldWheelRoute(bool pickingSeeds, bool seedKit) noexcept {
    if (pickingSeeds) return FieldWheelRoute::SeedPicker;
    return seedKit ? FieldWheelRoute::SeedKit : FieldWheelRoute::Tools;
}

// Wall time keeps the acknowledgement brief even while the simulation is paused.
class FieldWheelNotice {
public:
    using Clock = std::chrono::steady_clock;
    void Record(FieldWheelRoute route, int before, int after, Clock::time_point now) noexcept {
        if (before == after || after < 0) return;
        route_ = route; slot_ = after; expires_ = now + kLifetime;
    }
    bool Visible(FieldWheelRoute route, int slot, Clock::time_point now) noexcept {
        if (now >= expires_ || route != route_ || slot != slot_) Clear();
        return slot_ >= 0;
    }
    void Clear() noexcept { slot_ = -1; }
private:
    static constexpr auto kLifetime = std::chrono::seconds(1);
    FieldWheelRoute route_ = FieldWheelRoute::Tools;
    int slot_ = -1;
    Clock::time_point expires_{};
};
struct FieldStep {
    int x{}, y{};
    // Rows grow away from the standard farm camera. One key edge moves one cell.
    static constexpr FieldStep FromArrows(bool up, bool down, bool left, bool right) noexcept {
        if (up != down) return {0, up ? 1 : -1};
        if (left != right) return {left ? -1 : 1, 0};
        return {};
    }
};

[[nodiscard]] FieldStep ReadFieldStep(const Input& input, bool includeDPad = false);

// Once a right press becomes a camera gesture, returning to its origin cannot turn it into a tap.
class FieldRightTap {
public:
    using Clock = std::chrono::steady_clock;
    enum class Result { Pass, Pending, Tap };
    Result Sample(bool press, bool held, bool release, bool eligible, float x, float y,
        std::uint64_t generation, Clock::time_point now) noexcept {
        if (press) { pending_=eligible; x_=x; y_=y; generation_=generation; start_=now; }
        const float dx=x-x_, dy=y-y_;
        if (!eligible || generation!=generation_ || dx*dx+dy*dy>kSlop*kSlop || now-start_>kHoldLimit)
            pending_=false;
        if (!pending_) return Result::Pass;
        if (release) { pending_=false; return Result::Tap; }
        if (!held) { pending_=false; return Result::Pass; }
        return Result::Pending;
    }
private:
    static constexpr float kSlop=5.f; // Virtual 1280x720 HUD pixels, independent of window size.
    static constexpr auto kHoldLimit=std::chrono::milliseconds(400);
    Clock::time_point start_{};
    std::uint64_t generation_{};
    float x_{}, y_{};
    bool pending_{};
};

class FieldToolWheel final {
public:
    [[nodiscard]] bool HasRemainder() const noexcept { return remainder_ != 0; }
    [[nodiscard]] int Sample(int delta) noexcept {
        // One Windows wheel detent is 120 units. Down advances the hotbar.
        constexpr int kUnitsPerDetent = 120;
        const std::int64_t total = static_cast<std::int64_t>(delta) + remainder_;
        remainder_ = static_cast<int>(total % kUnitsPerDetent);
        return -static_cast<int>(total / kUnitsPerDetent);
    }
private:
    int remainder_{};
};

class FieldClick final {
public:
    [[nodiscard]] bool Active() const noexcept { return index_ >= 0; }
    // An invalid hit (HUD/outside) or a replacement board cancels the press.
    [[nodiscard]] int Sample(int hit, std::uint64_t generation, bool pressed, bool held, bool released) noexcept {
        if (pressed) { index_ = hit; generation_ = generation; }
        if (hit != index_ || generation != generation_) index_ = -1;
        const int commit = released ? index_ : -1;
        if (released || !held) index_ = -1;
        return commit;
    }
private:
    int index_ = -1;
    std::uint64_t generation_{};
};
}
