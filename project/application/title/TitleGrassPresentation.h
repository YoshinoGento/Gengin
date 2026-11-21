#pragma once
#include "title/TitlePresentationLayout.h"
#include "environment/VegetationWind.h"
#include <algorithm>
#include <cmath>

namespace title {
inline constexpr float kGrassMaximumBend = .68f;
inline constexpr float kGrassBendHeight = .35f;
inline constexpr float kGrassFlutterPeriod = 4.0f;
inline constexpr float kGrassWaveWindReference = .0015f;
inline constexpr float kGrassWaveSpatialFrequency = vegetation::kSpatialFrequency;
inline constexpr float kGrassTuftPhaseSpread = .08f;

inline float GrassExposure(float exposure) noexcept {
    return std::isfinite(exposure) ? std::clamp(exposure,0.0f,1.0f) : 0;
}

// Sky samples atan2(z,x)+offset: positive offset moves clouds along the camera's right tangent.
inline Vector2 GrassWindFromCloudVelocity(Vector2 velocity) noexcept {
    if (!std::isfinite(velocity.x) || !std::isfinite(velocity.y)) return {};
    const float strength = std::clamp(velocity.x/kGrassWaveWindReference,-1.0f,1.0f)*kGrassMaximumBend;
    const auto axis = vegetation::WaveAxis();
    return {axis.x*strength,axis.y*strength};
}
}
