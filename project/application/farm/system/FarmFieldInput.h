#pragma once
#include <cstdint>

class Input;

namespace farm {
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
