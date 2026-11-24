#pragma once

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/render/FarmSoilSurface.h"
#include "environment/VegetationWind.h"
#include <array>
#include <cmath>

namespace farm {
enum class FarmMeshShape { Box, TriangleLower, TriangleUpper, Turnip, Carrot, Leaves, CarrotLeaves,
    Tomato, TomatoStems, Pumpkin, PumpkinVines, SoilRidge, BankStone, BendWater, BendLining, BendFillet, WateringCan, HoeHandle, HoeBlade,
    HarvestBasket, HarvestBasketRim, PickedTomatoLarge, PickedTomatoSmall, Count };
inline constexpr std::array kFarmMeshPaths{"farm/unit_box.obj","farm/triangle_lower.obj","farm/triangle_upper.obj",
    "farm/crop_turnip.obj","farm/crop_carrot.obj","farm/crop_turnip_leaves.obj","farm/crop_carrot_leaves.obj",
    "farm/crop_tomato.obj","farm/crop_tomato_stems.obj","farm/crop_pumpkin.obj","farm/crop_pumpkin_vines.obj",
    "farm/soil_ridge.obj","farm/bank_stone.obj","farm/bend_water.obj","farm/bend_lining.obj","farm/bend_fillet.obj","farm/arrival/hand_can.obj",
    "farm/arrival/hoe_handle.obj","farm/arrival/hoe_blade.obj","farm/arrival/harvest_basket.obj","farm/arrival/harvest_basket_rim.obj",
    "farm/arrival/picked_tomato_0.obj","farm/arrival/picked_tomato_1.obj"};
static_assert(kFarmMeshPaths.size()==static_cast<std::size_t>(FarmMeshShape::Count));
enum class FarmMeshSurface { None, CanalBed, CanalBank, SoilBoundary, Selection, Hover, Crop,
	Earth, Soil, Grass, PestCue };
inline constexpr float CropWindResponse(FarmMeshShape shape) noexcept {
    switch (shape) {
    case FarmMeshShape::Leaves:
    case FarmMeshShape::CarrotLeaves: return .31f;
    case FarmMeshShape::Tomato:
    case FarmMeshShape::TomatoStems: return .14f;
    case FarmMeshShape::Pumpkin:
    case FarmMeshShape::PumpkinVines: return .065f;
    default: return 0;
    }
}
struct FarmMeshPart {
	Vector3 position{};
	Vector3 scale{1.0f, 1.0f, 1.0f}; // Box: half-size. Crop: radius/height in its base-anchored OBJ space.
	Vector4 color{1.0f, 1.0f, 1.0f, 1.0f};
	bool water = false;
	Vector2 slope{}; // dY/dX and dY/dZ before rotation, applied after scale.
	FarmMeshShape shape = FarmMeshShape::Box;
	FarmMeshSurface surface = FarmMeshSurface::None;
	float wetness = 0.0f; // Presentation snapshot, never written back to the simulation.
	float heightAboveWindPivot = 0.0f;
	float yaw = 0.0f;
    float roll = 0.0f;
    Vector4 wetFront{}; // World XZ half-plane; w=0 disables the transient material mask.
};

struct FarmWindPose { Vector3 position, rotation; };
inline FarmWindPose CropWindPose(const FarmMeshPart& part, const vegetation::WindFrame& wind,Vector3 touch={}) noexcept {
	FarmWindPose pose{part.position, wind.Tilt(part.position, CropWindResponse(part.shape))};
    pose.rotation=pose.rotation+touch;
	const float h = part.heightAboveWindPivot;
	if (h != 0.0f) {
		// Rotate the offset from the stalk's pivot, in the engine's row-vector X then Z order.
		const float y = h*std::cos(pose.rotation.x);
		pose.position.x -= y*std::sin(pose.rotation.z);
		pose.position.y += y*std::cos(pose.rotation.z)-h;
		pose.position.z += h*std::sin(pose.rotation.x);
	}
	return pose;
}
struct FarmTileMeshParts {
	// Soil: at most 26 patches plus either three ridges or four boundary strips.
	std::array<FarmMeshPart, 32> parts{};
	std::size_t count = 0;
};

inline void RevealFarmWater(FarmTileMeshParts& mesh,Vector3 center,Vector3 upstream,Vector3 downstream,float cellHalf,float fraction) noexcept {
    if(fraction>=1.f) return;
    const auto axis=[](float v){return v>0.f?1.f:v<0.f?-1.f:0.f;};
    const Vector2 in{axis(center.x-upstream.x),axis(center.z-upstream.z)};
    const Vector2 out{axis(downstream.x-center.x),axis(downstream.z-center.z)};
    auto normal=in;
    float x=center.x+in.x*(2.f*fraction-1.f)*cellHalf, z=center.z+in.y*(2.f*fraction-1.f)*cellHalf;
    if(in.x*out.y!=0 || in.y*out.x!=0) {
        // A radial cut about the inner corner meets both neighbouring cuts at their shared edges.
        const float angle=1.570796327f*fraction;
        normal={in.x*std::cos(angle)+out.x*std::sin(angle),in.y*std::cos(angle)+out.y*std::sin(angle)};
        x=center.x+(out.x-in.x)*cellHalf; z=center.z+(out.y-in.y)*cellHalf;
    }
    const Vector4 front{normal.x,normal.y,x*normal.x+z*normal.y,.095f*cellHalf};
    size_t kept=0;
    for(size_t j=0;j<mesh.count;++j) {
        auto& part=mesh.parts[j];
        if(part.water && fraction<=0) continue;
        if(part.water) part.wetFront=front;
        if(kept!=j) mesh.parts[kept]=part;
        ++kept;
    }
    mesh.count=kept;
}

struct FarmSelectionMeshParts {
	std::array<FarmMeshPart, 8> parts{};
	std::size_t count = 0;
};

struct FarmCropMeshParts {
	std::array<FarmMeshPart, 2> parts{};
	std::size_t count = 0;
};

struct FarmRepellentMeshParts {
	std::array<FarmMeshPart, 8> parts{};
	std::size_t count = 0;
};

inline FarmRepellentMeshParts BuildFarmRepellentMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& system) noexcept {
	FarmRepellentMeshParts result;
	const auto* tile = grid.GetTile(index);
	const auto visual = system.GetTileVisualData(grid, index);
	if (!tile || !tile->repellentPlant || !visual.valid || !FarmGrid::IsValidTile(*tile)) return result;
	// Voxel shrub anchored on the shared flat plateau; dimensions follow tile scale.
	const float unit = visual.halfExtent;
	const auto add = [&](Vector3 offset, Vector3 halfSize, Vector4 color) {
		result.parts[result.count++] = {{visual.center.x + offset.x * unit,
			visual.center.y + offset.y * unit, visual.center.z + offset.z * unit},
			{halfSize.x * unit, halfSize.y * unit, halfSize.z * unit}, color, false, {},
			FarmMeshShape::Box, FarmMeshSurface::Crop};
	};
	add({0,.30f,0}, {.045f,.30f,.045f}, {.21f,.37f,.09f,1});
	add({-.15f,.27f,0}, {.22f,.08f,.15f}, {.20f,.43f,.13f,1});
	add({.15f,.42f,.03f}, {.24f,.08f,.16f}, {.28f,.51f,.15f,1});
	add({0,.49f,-.13f}, {.15f,.10f,.23f}, {.33f,.55f,.17f,1});
	add({-.17f,.64f,-.08f}, {.12f,.09f,.12f}, {.93f,.64f,.15f,1});
	add({.17f,.68f,.08f}, {.13f,.08f,.13f}, {1,.77f,.22f,1});
	add({-.17f,.74f,-.08f}, {.065f,.025f,.065f}, {.62f,.30f,.06f,1});
	add({.17f,.77f,.08f}, {.07f,.025f,.07f}, {.71f,.36f,.07f,1});
	return result;
}

// Crop OBJ space: root/leaf base at y=0, top at y=1, horizontal extent <=1.
inline FarmCropMeshParts BuildFarmCropMeshParts(
	const FarmTileVisualData& visual, float cropGrowth) noexcept {
	FarmCropMeshParts result;
	if (!visual.valid || visual.feature != FarmTileFeature::None ||
		visual.cropStage == FarmCropGrowthStage::None || !IsPlantableCrop(visual.crop) ||
		!std::isfinite(cropGrowth) || !std::isfinite(visual.cropScale) || visual.cropScale <= 0) return result;
	const float growth = std::clamp(cropGrowth, 0.0f, 1.0f);
	const float scale = visual.cropScale;
	if (visual.crop == CropType::Tomato || visual.crop == CropType::Pumpkin) {
		const bool tomato = visual.crop == CropType::Tomato;
		const bool fruit = growth >= kFarmGrowthStageAlmostReadyMinimum;
		const float size = (0.25f + 0.75f*growth)*scale;
		// Tomato fruit hangs above soil; pumpkin sits on soil. Neither is a buried root.
		if (fruit) result.parts[result.count++] = {
			{visual.center.x,visual.center.y + (tomato ? .22f : .015f)*size,visual.center.z},
			{(tomato ? .42f : .46f)*size,(tomato ? .60f : .65f)*size,(tomato ? .42f : .46f)*size},
			tomato ? Vector4{.87f,.12f,.07f,1} : Vector4{.12f,.36f,.16f,1},false,{},
			tomato ? FarmMeshShape::Tomato : FarmMeshShape::Pumpkin,FarmMeshSurface::Crop};
		result.parts[result.count++] = {{visual.center.x,visual.center.y+.02f*size,visual.center.z},
			{.58f*size,(tomato ? 1.05f : .80f)*size,.58f*size},{.24f,.50f,.18f,1},false,{},
			tomato ? FarmMeshShape::TomatoStems : FarmMeshShape::PumpkinVines,FarmMeshSurface::Crop};
		if (fruit) result.parts[0].heightAboveWindPivot = result.parts[0].position.y-result.parts[1].position.y;
		return result;
	}
	const bool carrot = visual.crop == CropType::Carrot;
	const bool bodyVisible = visual.cropStage == FarmCropGrowthStage::AlmostReady ||
		visual.cropStage == FarmCropGrowthStage::Ready;
	const float bodyHeight = bodyVisible ? (carrot ? 0.55f : 0.40f) * growth * scale : 0;
	// These fractions refer to the full y=0..1 root OBJ, not its bounding-box center.
	constexpr float kCarrotBuriedFraction = 0.84f;
	constexpr float kTurnipBuriedFraction = 0.50f;
	constexpr float kLeafAttachmentFraction = 0.96f;
	const float buriedFraction = carrot ? kCarrotBuriedFraction : kTurnipBuriedFraction;
	const Vector3 rootBase{visual.center.x, visual.center.y - bodyHeight * buriedFraction, visual.center.z};
	if (bodyVisible) {
		const float radius = (carrot ? 0.18f : 0.28f) * growth * scale;
		result.parts[result.count++] = {rootBase, {radius, bodyHeight, radius},
			carrot ? Vector4{0.94f,0.43f,0.10f,1} : Vector4{0.91f,0.79f,0.87f,1},
			false, {}, carrot ? FarmMeshShape::Carrot : FarmMeshShape::Turnip, FarmMeshSurface::Crop};
	}
	const float leafWidth = (0.10f + growth * 0.22f) * scale;
	// Sprouts emerge at the soil anchor; mature leaves attach inside the exposed root shoulder.
	const Vector3 leafBase = bodyVisible
		? Vector3{rootBase.x, rootBase.y + bodyHeight * kLeafAttachmentFraction, rootBase.z}
		: visual.cropAnchor;
	result.parts[result.count++] = {leafBase, {leafWidth, (0.12f + growth*0.25f)*scale, leafWidth},
		carrot ? Vector4{0.25f,0.54f,0.18f,1} : Vector4{0.40f,0.65f,0.22f,1},
		false, {}, carrot ? FarmMeshShape::CarrotLeaves : FarmMeshShape::Leaves, FarmMeshSurface::Crop};
	return result;
}

inline FarmCropMeshParts BuildFarmCropMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& system) noexcept {
	const auto* tile = grid.GetTile(index);
	if (!tile) return {};
	auto result = BuildFarmCropMeshParts(system.GetTileVisualData(grid, index), tile->growth);
	for (std::size_t i = 0; i < result.count; ++i) {
		auto& part = result.parts[i];
		if (part.shape != FarmMeshShape::CarrotLeaves && part.shape != FarmMeshShape::TomatoStems &&
			part.shape != FarmMeshShape::PumpkinVines) continue;
		switch (tile->mold.Phase()) {
		case MoldPhase::Warning: part.color = {.80f,.64f,.21f,1}; break;
		case MoldPhase::Sick:
			part.color = {.71f,.68f,.58f,1};
			// A fruiting stalk includes its calyx; shortening only that mesh drives it through the fruit.
			if (result.count == 1 || part.shape == FarmMeshShape::CarrotLeaves) part.scale.y *= .70f;
			break;
		case MoldPhase::Recovering: part.color = {.52f,.67f,.30f,1}; break;
		default: break;
		}
	}
	return result;
}

// Target markers are transient presentation, not part of terrain/save or its per-tile budget.
inline FarmSelectionMeshParts BuildFarmTargetMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& visualSystem,
	bool hover) noexcept {
	FarmSelectionMeshParts result;
	const auto tile = visualSystem.GetTileVisualData(grid, index);
	if (!tile.valid) { return result; }
	const auto& layout = visualSystem.GetLayout();
	const float edge = tile.halfExtent * (hover ? 0.69f : 0.76f);
	const float arm = tile.halfExtent * (hover ? 0.20f : 0.28f);
	const float width = tile.halfExtent * (hover ? 0.022f : 0.035f);
	const float halfThickness = hover ? 0.006f : 0.01f;
	const float lift = tile.feature == FarmTileFeature::None ? 0.045f :
		layout.waterBottomOffset + layout.waterMaxDepth + 0.065f;
	const float y = tile.center.y + lift + (hover ? 0.014f : 0.0f);
	if (!std::isfinite(tile.center.x) || !std::isfinite(y) || !std::isfinite(tile.center.z) ||
		!std::isfinite(edge) || width < 0.0001f) { return result; }
	const Vector4 color = hover ? Vector4{0.32f, 0.92f, 1.0f, 1.0f} : Vector4{1.0f, 0.86f, 0.18f, 1.0f};
	const auto surface = hover ? FarmMeshSurface::Hover : FarmMeshSurface::Selection;
	for (int x : {-1, 1}) for (int z : {-1, 1}) {
		result.parts[result.count++] = {{tile.center.x+x*(edge-arm*0.5f), y, tile.center.z+z*edge},
			{arm*0.5f+width, halfThickness, width}, color, false, {}, FarmMeshShape::Box, surface};
		result.parts[result.count++] = {{tile.center.x+x*edge, y, tile.center.z+z*(edge-arm*0.5f-width)},
			{width, halfThickness, arm*0.5f-width}, color, false, {}, FarmMeshShape::Box, surface};
	}
	return result;
}

inline FarmSelectionMeshParts BuildFarmSelectionMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& visualSystem) noexcept {
	return BuildFarmTargetMeshParts(grid, index, visualSystem, false);
}

inline FarmSelectionMeshParts BuildFarmHoverMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& visualSystem) noexcept {
	return BuildFarmTargetMeshParts(grid, index, visualSystem, true);
}

// Pure presentation conversion, usable by CPU tests without allocating GPU resources.
inline FarmTileMeshParts BuildFarmTileMeshParts(
	const FarmGrid& grid, int index, const FarmVisualSystem& visualSystem,float pendingWater=0.f,bool awaitingHoe=false) noexcept {
	constexpr float kEarthDepth = 0.24f;
	constexpr float kTopThickness = 0.04f;
	constexpr float kWaterHalfThickness = 0.005f;
	constexpr float kRimFreeboard = 0.045f;
	const Vector4 waterColor{0.095f, 0.39f, 0.43f, 1.0f};
	FarmTileMeshParts result;
	auto visual = visualSystem.GetTileVisualData(grid, index);
    visual.soilWetness=std::clamp(visual.soilWetness-pendingWater,0.f,1.f);
	const auto* tile = grid.GetTile(index);
	if (!visual.valid || !tile) { return result; }
	const auto& layout = visualSystem.GetLayout();
	const float cellHalf = (layout.tileSize + layout.tileGap) * 0.5f;
	const float groundBottom = layout.center.y - kEarthDepth;
	const float half = visual.halfExtent;
	const auto add = [&](Vector3 position, Vector3 scale, Vector4 color, bool water = false, Vector2 slope = {}, FarmMeshShape shape = FarmMeshShape::Box, FarmMeshSurface surface = FarmMeshSurface::None, float rotation = 0.f) {
		if (result.count >= result.parts.size() || !std::isfinite(position.x) ||
			!std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(scale.x) ||
			!std::isfinite(scale.y) || !std::isfinite(scale.z) || !std::isfinite(slope.x) || !std::isfinite(slope.y) ||
			scale.x < 0.0001f || scale.y < 0.0001f || scale.z < 0.0001f) { return; }
		result.parts[result.count++] = {position, scale, color, water, slope, shape, surface,
			visual.soilWetness, 0.f, rotation};
	};
	if (visual.feature == FarmTileFeature::None) {
        const bool uncultivated=tile->state==FarmTileState::Empty || (awaitingHoe && tile->state==FarmTileState::Tilled);
		const float wetness = visual.soilWetness;
		const Vector4 soil = uncultivated
			? Vector4{0.27f - 0.08f * wetness, 0.57f - 0.15f * wetness, 0.13f, 1.0f}
			: Vector4{0.52f - 0.28f * wetness, 0.32f - 0.18f * wetness, 0.16f - 0.09f * wetness, 1.0f};
		const Vector4 earth{0.43f - 0.16f * wetness, 0.24f - 0.10f * wetness, 0.10f, 1.0f};
		const auto topSurface = uncultivated ? FarmMeshSurface::Grass : FarmMeshSurface::Soil;
		const auto surface = BuildFarmSoilSurface(grid, index, visualSystem);
		if (!surface.valid) { return {}; }
		bool flat = true;
		for (const auto& point : surface.points) { flat &= std::abs(point.y - visual.center.y) < 0.000001f; }
		if (flat) {
			const float bodyTop = visual.center.y - kTopThickness;
			add({visual.center.x, (groundBottom + bodyTop) * 0.5f, visual.center.z},
				{cellHalf, (bodyTop - groundBottom) * 0.5f, cellHalf}, earth, false, {}, FarmMeshShape::Box, FarmMeshSurface::Earth);
			add({visual.center.x, visual.center.y - kTopThickness * 0.5f, visual.center.z},
				{cellHalf, kTopThickness * 0.5f, cellHalf}, soil, false, {}, FarmMeshShape::Box, topSurface);
		} else {
			// Each triangle retains a buried earth body; no high full-tile block hides the ramp.
			const float bodyHalfDepth = (visual.center.y - groundBottom + 2.0f * layout.heightStep) * 0.5f;
			for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) {
				const auto& a = surface.At(x,z);
				const auto& b = surface.At(x+1,z);
				const auto& c = surface.At(x,z+1);
				const auto& d = surface.At(x+1,z+1);
				const bool planar = std::abs(a.y + d.y - b.y - c.y) < 0.000001f;
				for (int upper = 0; upper < (planar ? 1 : 2); ++upper) {
					const auto slope = FarmSoilPatchSlope(surface,x,z,upper != 0);
					const auto& anchor = upper ? d : a;
					const float centerX = (a.x + d.x) * 0.5f;
					const float centerZ = (a.z + d.z) * 0.5f;
					const float top = anchor.y + slope.x * (centerX-anchor.x) + slope.y * (centerZ-anchor.z);
					const auto shape = planar ? FarmMeshShape::Box : (upper ? FarmMeshShape::TriangleUpper : FarmMeshShape::TriangleLower);
					add({centerX, top - kTopThickness - bodyHalfDepth, centerZ},
						{(d.x-a.x)*0.5f,bodyHalfDepth,(d.z-a.z)*0.5f},earth,false,slope,shape,FarmMeshSurface::Earth);
					add({centerX, top - kTopThickness*0.5f, centerZ},
						{(d.x-a.x)*0.5f,kTopThickness*0.5f,(d.z-a.z)*0.5f},soil,false,slope,shape,topSurface);
				}
			}
		}
		if (uncultivated) {
			// Inset within the shared flat plateau: ramps and water connections remain untouched.
			const float edge = half * 0.80f;
			const float width = half * 0.032f;
			constexpr float lift = 0.002f;
			const Vector4 boundary{0.12f, 0.28f, 0.08f, 1.0f};
			for (int side : {-1, 1}) {
				add({visual.center.x + side*edge, visual.center.y+lift, visual.center.z},
					{width, lift, edge+width}, boundary, false, {}, FarmMeshShape::Box, FarmMeshSurface::SoilBoundary);
				add({visual.center.x, visual.center.y+lift, visual.center.z + side*edge},
					{edge-width, lift, width}, boundary, false, {}, FarmMeshShape::Box, FarmMeshSurface::SoilBoundary);
			}
		} else {
			for (int ridge = -1; ridge <= 1; ++ridge) {
				add({visual.center.x + ridge * half * 0.52f, visual.center.y + kFarmSoilRidgeHeight*.5f, visual.center.z},
					{half * 0.12f, kFarmSoilRidgeHeight*.5f, half * 0.83f}, soil, false, {}, FarmMeshShape::SoilRidge, FarmMeshSurface::Soil);
			}
		}
		return result;
	}
	const float bottom = visual.center.y + layout.waterBottomOffset;
	const float rimHeight = layout.waterMaxDepth + kRimFreeboard;
	const int width = grid.GetWidth();
	const int column = index % width;
	const int row = index / width;
	const int neighbors[4] = {column > 0 ? index-1 : -1, column+1 < width ? index+1 : -1,
		row > 0 ? index-width : -1, row+1 < grid.GetHeight() ? index+width : -1};
	std::array<FarmTileVisualData, 4> neighborData{};
	std::array<float, 4> bedSlope{};
	const float inner = visual.waterHalfExtent;
	const float run = cellHalf - inner;
	if (!std::isfinite(run) || run < 0.0001f) { return {}; }
	for (int edge = 0; edge < 4; ++edge) {
		neighborData[edge] = visualSystem.GetTileVisualData(grid, neighbors[edge]);
		const auto& neighbor = neighborData[edge];
		if (neighbor.valid && neighbor.feature != FarmTileFeature::None) {
			// Both halves meet at the mean height, regardless of traversal order.
			bedSlope[edge] = (neighbor.center.y - visual.center.y) * 0.5f / run;
		}
	}
	const auto connected = [&](int side) { return neighborData[side].valid && neighborData[side].feature != FarmTileFeature::None; };
	const bool bend = visual.feature == FarmTileFeature::Canal &&
		connected(0) != connected(1) && connected(2) != connected(3);
	constexpr float quarterTurn = 1.570796327f;
	const float yaw = connected(0) ? (connected(2) ? 0.f : quarterTurn)
		: (connected(2) ? -quarterTurn : 2.f*quarterTurn);
	const float offsets[3] = {-(cellHalf + inner) * 0.5f, 0.0f, (cellHalf + inner) * 0.5f};
	const float extents[3] = {run * 0.5f, inner, run * 0.5f};
	const float xSlopes[3] = {-bedSlope[0], 0.0f, bedSlope[1]};
	const float zSlopes[3] = {-bedSlope[2], 0.0f, bedSlope[3]};
	const float xRise[3] = {bedSlope[0] * run * 0.5f, 0.0f, bedSlope[1] * run * 0.5f};
	const float zRise[3] = {bedSlope[2] * run * 0.5f, 0.0f, bedSlope[3] * run * 0.5f};
	// Remove the old full-height bed: it would occlude a descending water ramp.
	// Deep overlapping undersides stay below the field; each top section has one owner.
	const float bodyHalfDepth = (bottom - groundBottom + 2.0f * layout.heightStep) * 0.5f;
	for (int z = 0; z < 3; ++z) {
		for (int x = 0; x < 3; ++x) {
			add({visual.center.x + offsets[x], bottom + xRise[x] + zRise[z] - bodyHalfDepth, visual.center.z + offsets[z]},
				{extents[x], bodyHalfDepth, extents[z]}, {0.29f, 0.25f, 0.15f, 1.0f}, false, {xSlopes[x], zSlopes[z]}, FarmMeshShape::Box, FarmMeshSurface::CanalBed);
		}
	}
	if (visual.showWaterSurface) {
		add({visual.center.x, visual.waterSurfaceCenter.y - kWaterHalfThickness, visual.center.z},
			{inner, kWaterHalfThickness, inner}, waterColor, true, {}, bend ? FarmMeshShape::BendWater : FarmMeshShape::Box,
			FarmMeshSurface::None, bend ? yaw : 0.f);
	}
	if (bend) {
		// The lining is a low toe inside the load-bearing bank; its mouth matches both ramp arms.
		constexpr float apronScale = 1.25f;
		const float height = rimHeight*.93f;
		add({visual.center.x,bottom+height*.50f,visual.center.z},
			{inner*apronScale,height*.5f,inner*apronScale},{.37f,.35f,.27f,1},false,{},FarmMeshShape::BendLining,
			FarmMeshSurface::None,yaw);
		const int xEdge=connected(0)?0:1, zEdge=connected(2)?2:3;
		if (visual.showWaterSurface && neighborData[xEdge].showWaterSurface && neighborData[zEdge].showWaterSurface) {
			const float sx=xEdge==0?-1.f:1.f, sz=zEdge==2?-1.f:1.f;
			const float level=visual.waterSurfaceCenter.y;
			const float gx=sx*(neighborData[xEdge].waterSurfaceCenter.y-level)*.5f/run;
			const float gz=sz*(neighborData[zEdge].waterSurfaceCenter.y-level)*.5f/run;
			const float c=std::cos(yaw), s=std::sin(yaw);
			const float reach=run*.83f;
			add({visual.center.x+sx*inner,level-kWaterHalfThickness,visual.center.z+sz*inner},
				{reach,kWaterHalfThickness,reach},waterColor,true,{gx*c-gz*s,gx*s+gz*c},
				FarmMeshShape::BendFillet,FarmMeshSurface::None,yaw);
		}
	}
	for (int edge = 0; edge < 4; ++edge) {
		const auto& neighbor = neighborData[edge];
		const bool open = neighbor.valid && neighbor.feature != FarmTileFeature::None;
		const float sign = edge % 2 == 0 ? -1.0f : 1.0f;
		const bool alongX = edge < 2;
		if (!open) {
			const float rimWidth = run * .40f;
			// Split side banks with the bed so their ends follow the same transition.
			for (int section = 0; section < 3; ++section) {
				constexpr std::array<Vector4,3> stoneTones{{{.40f,.43f,.39f,1},{.46f,.46f,.40f,1},{.43f,.45f,.42f,1}}};
				const auto& stone=stoneTones[(index+edge+section)%stoneTones.size()];
				// Only one wall owns each closed corner, even on a dead end.
				const float trim = !alongX && section!=1 && !connected(section/2) ? rimWidth : 0.f;
				const float shift = section==0 ? trim : -trim;
				float along = offsets[section] + shift, span = extents[section]-trim;
				// Flat banks use a running bond; on a grade each stone stays on its own ramp plane.
				const int parallel = alongX ? 2 : 0;
				if (bedSlope[parallel]==0.f && bedSlope[parallel+1]==0.f) {
					const float lo=-cellHalf+(!alongX&&!connected(0)?2*rimWidth:0);
					const float hi=cellHalf-(!alongX&&!connected(1)?2*rimWidth:0);
					const float jog=((index+edge)%2?.045f:-.045f);
					const float joints[]{lo,lo+(hi-lo)*(.30f+jog),lo+(hi-lo)*(.68f+jog),hi};
					along=(joints[section]+joints[section+1])*.5f;
					span=(joints[section+1]-joints[section])*.5f;
				}
				const float rise = alongX ? zRise[section] : xRise[section];
				const Vector2 slope = alongX ? Vector2{0.0f, zSlopes[section]} : Vector2{xSlopes[section], 0.0f};
				add({visual.center.x + (alongX ? sign * (cellHalf - rimWidth) : along), bottom + rise + rimHeight * 0.5f,
					visual.center.z + (alongX ? along : sign * (cellHalf - rimWidth))},
					{alongX ? rimWidth : span, rimHeight * 0.5f, alongX ? span : rimWidth}, stone, false, slope, FarmMeshShape::BankStone, FarmMeshSurface::CanalBank);
			}
			continue;
		}
		if (!visual.showWaterSurface || !neighbor.showWaterSurface) { continue; }
		const float armHalf = run * 0.5f;
		const float armOffset = (cellHalf + inner) * 0.5f;
		const float boundaryRise = (neighbor.waterSurfaceCenter.y - visual.waterSurfaceCenter.y) * 0.5f;
		const float slope = sign * boundaryRise / run;
		add({visual.center.x + (alongX ? sign * armOffset : 0.0f), visual.waterSurfaceCenter.y + boundaryRise * 0.5f - kWaterHalfThickness,
			visual.center.z + (alongX ? 0.0f : sign * armOffset)},
			{alongX ? armHalf : inner, kWaterHalfThickness, alongX ? inner : armHalf}, waterColor, true,
			alongX ? Vector2{slope, 0.0f} : Vector2{0.0f, slope});
	}
	return result;
}
} // namespace farm
