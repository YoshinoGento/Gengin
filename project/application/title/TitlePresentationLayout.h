#pragma once

#include "math/Struct.h"
#include <cmath>
#include <optional>

namespace title {
inline constexpr float kWidth = 1280.0f;
inline constexpr float kHeight = 720.0f;
inline constexpr Vector3 kCameraPosition{6.5f, 1.9f, -7.5f};
inline constexpr Vector3 kCameraRotation{.12f, -.45f, 0};
inline constexpr float kCameraFovY = .72f;
inline constexpr Vector3 kFarmCenter{0,0,5};
inline constexpr Vector2 kStartCenter{640, 650};
inline constexpr Vector2 kStartSize{280, 80};
inline constexpr Vector2 kLogoCenter{300, 152};
inline constexpr Vector2 kLogoSize{456, 168};

// Input and drawing share virtual pixels, regardless of client size or editor letterboxing.
inline bool HitTestTitleCanvas(Vector2 position) noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
        position.x >= 0 && position.x <= kWidth && position.y >= 0 && position.y <= kHeight;
}

inline std::optional<Vector2> MapTitleCanvasPointer(Vector2 position, Vector2 virtualResolution) noexcept {
    if (!std::isfinite(virtualResolution.x) || !std::isfinite(virtualResolution.y) ||
        virtualResolution.x <= 0 || virtualResolution.y <= 0 ||
        !std::isfinite(position.x) || !std::isfinite(position.y) ||
        position.x < 0 || position.y < 0 || position.x > virtualResolution.x ||
        position.y > virtualResolution.y) return std::nullopt;
    return Vector2{(position.x / virtualResolution.x) * kWidth,
        (position.y / virtualResolution.y) * kHeight};
}

inline bool HitTestTitleCanvasInViewport(Vector2 position, Vector2 virtualResolution) noexcept {
    return MapTitleCanvasPointer(position, virtualResolution).has_value();
}
}
