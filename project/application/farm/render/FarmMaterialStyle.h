#pragma once

#include "farm/render/FarmMeshLayout.h"
#include <algorithm>
#include <cmath>

namespace farm {
struct FarmMaterialStyle {
	// Same units as Object3d::SetSurfaceDetail; no GPU resource ownership.
	Vector4 detail{};
	float shininess = 32.0f;
};

inline FarmMaterialStyle BuildFarmMaterialStyle(const FarmMeshPart& part) noexcept {
	constexpr float kWetSoilSpecular = 0.075f;
	constexpr float kFruitSpecular = 0.085f;
	constexpr float kFoliageSpecular = 0.018f;
	if (part.water || part.surface == FarmMeshSurface::Selection ||
		part.surface == FarmMeshSurface::Hover || part.surface == FarmMeshSurface::PestCue) { return {}; }
	const float wetness = std::isfinite(part.wetness) ? std::clamp(part.wetness, 0.0f, 1.0f) : 0.0f;
	switch (part.surface) {
	case FarmMeshSurface::Soil:
		return {{kWetSoilSpecular * wetness * wetness, 0.26f - 0.10f * wetness,
			9.0f, 0.12f - 0.08f * wetness}, 32.0f + 40.0f * wetness};
	case FarmMeshSurface::Earth:
	case FarmMeshSurface::CanalBed:
		return {{0.0f, 0.20f, 6.0f, 0.08f}, 32.0f};
	case FarmMeshSurface::CanalBank:
		return {{0.0f, 0.16f, 7.0f, 0.10f}, 32.0f};
	case FarmMeshSurface::Grass:
		return {{0.0f, 0.14f, 12.0f, 0.06f}, 32.0f};
	case FarmMeshSurface::Crop:
		switch (part.shape) {
		case FarmMeshShape::Tomato:
		case FarmMeshShape::Pumpkin:
			return {{kFruitSpecular, 0.0f, 0.0f, 0.0f}, 48.0f};
		case FarmMeshShape::Turnip:
		case FarmMeshShape::Carrot:
			return {{0.025f, 0.0f, 0.0f, 0.0f}, 24.0f};
		default:
			return {{kFoliageSpecular, 0.0f, 0.0f, 0.0f}, 16.0f};
		}
	default:
		return {};
	}
}
} // namespace farm
