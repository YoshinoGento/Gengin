#include "farm/system/FarmTerrainQuerySystem.h"
#include "farm/render/FarmMeshLayout.h"
#include <algorithm>
#include <cmath>

farm::FarmGroundSample farm::FarmTerrainQuerySystem::SampleGround(
	const FarmGrid& grid, const FarmVisualSystem& visual,
	const Vector3& footCenter, const Vector2& halfFootprint) noexcept {
	FarmGroundSample result;
	if (!std::isfinite(footCenter.x) || !std::isfinite(footCenter.y) || !std::isfinite(footCenter.z) ||
		!std::isfinite(halfFootprint.x) || !std::isfinite(halfFootprint.y) ||
		halfFootprint.x < 0.0f || halfFootprint.y < 0.0f) { return result; }
	float soilHeight = 0.0f;
	if (visual.TryGetSoilHeight(grid, footCenter, soilHeight)) {
		result = {true, soilHeight, -1, FarmGroundKind::Soil};
	}
	const float halfPitch = (visual.GetLayout().tileSize + visual.GetLayout().tileGap) * 0.5f;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const auto data = visual.GetTileVisualData(grid, index);
		if (!data.valid) { continue; }
		const float dx = std::abs(footCenter.x - data.center.x);
		const float dz = std::abs(footCenter.z - data.center.z);
		if (data.feature == FarmTileFeature::None) {
			if (result.kind == FarmGroundKind::Soil && result.tileIndex < 0 && dx <= halfPitch && dz <= halfPitch) { result.tileIndex = index; }
			continue;
		}
		if (dx > halfPitch + halfFootprint.x || dz > halfPitch + halfFootprint.y) { continue; }
		const auto mesh = BuildFarmTileMeshParts(grid, index, visual);
		for (std::size_t i = 0; i < mesh.count; ++i) {
			const auto& part = mesh.parts[i];
			if (part.surface != FarmMeshSurface::CanalBank && part.surface != FarmMeshSurface::CanalBed) { continue; }
			// Banks support the whole AABB foot area; bed/soil use the center to avoid floating over slopes.
			const bool bank = part.surface == FarmMeshSurface::CanalBank;
			const float extentX = bank ? halfFootprint.x : 0.0f;
			const float extentZ = bank ? halfFootprint.y : 0.0f;
			const float minX = (std::max)(footCenter.x - extentX, part.position.x - part.scale.x);
			const float maxX = (std::min)(footCenter.x + extentX, part.position.x + part.scale.x);
			const float minZ = (std::max)(footCenter.z - extentZ, part.position.z - part.scale.z);
			const float maxZ = (std::min)(footCenter.z + extentZ, part.position.z + part.scale.z);
			if (minX > maxX || minZ > maxZ) { continue; }
			if (!bank && (part.shape==FarmMeshShape::TriangleLower || part.shape==FarmMeshShape::TriangleUpper)) {
				const float side=(footCenter.x-part.position.x)/part.scale.x+(footCenter.z-part.position.z)/part.scale.z;
				if (part.shape==FarmMeshShape::TriangleLower ? side>0.f : side<0.f) continue;
			}
			const float sampleX = part.slope.x >= 0.0f ? maxX : minX;
			const float sampleZ = part.slope.y >= 0.0f ? maxZ : minZ;
			const float height = part.position.y + part.scale.y + part.slope.x * (sampleX - part.position.x) + part.slope.y * (sampleZ - part.position.z);
			if (!std::isfinite(height)) { continue; }
			if (!result.valid || height > result.height) {
				result = {true, height, index, bank ? FarmGroundKind::CanalBank : FarmGroundKind::CanalBed};
			}
		}
	}
	return result;
}
