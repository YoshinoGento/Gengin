#pragma once
#include "farm/ui/FarmUIRect.h"
#include <array>

namespace farmui::HUDLayout {
inline constexpr Rect status{24, 24, 216, 56};
inline constexpr Rect economy{1000, 24, 256, 56};
inline constexpr Rect selected{24, 554, 314, 142};
inline constexpr Rect tools{380, 620, 520, 76};
inline constexpr Rect goal{1000, 84, 256, 44};
inline constexpr Rect fieldActions{956, 584, 232, 112};
inline constexpr Rect waterGuidance{24, 460, 314, 86};
inline constexpr Rect shortGoal{584, 24, 400, 52};
inline constexpr Rect shortGoalDetail{24, 86, 314, 38};
inline constexpr std::array<Rect, 4> panels{{status, economy, selected, tools}};
[[nodiscard]] constexpr Rect ToolSlot(int index) noexcept {
    return {390.0f + index * 68.0f, 628, 60, 60};
}
[[nodiscard]] constexpr Rect SeedSlot(int index) noexcept {
    return {682.0f + index * 68.0f, 628, 60, 60};
}
[[nodiscard]] constexpr Rect UtilitySlot(int index) noexcept {
    return {1204, 336.0f + index * 64.0f, 52, 52};
}
}
