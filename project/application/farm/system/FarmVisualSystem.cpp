#include "farm/system/FarmVisualSystem.h"

#include "farm/render/FarmSoilSurface.h"

#include "3d/LineDrawer.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmIrrigationForecast.h"

#include <algorithm>
#include <cmath>

bool farm::FarmVisualSystem::TryGetSoilHeight(const FarmGrid& grid, const Vector3& position, float& height) const noexcept {
	if (!std::isfinite(position.x) || !std::isfinite(position.z)) { return false; }
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const auto data = GetTileVisualData(grid, index);
		const float halfPitch = (layout_.tileSize + layout_.tileGap) * 0.5f;
		if (!data.valid || data.feature != FarmTileFeature::None ||
			std::abs(position.x - data.center.x) > halfPitch || std::abs(position.z - data.center.z) > halfPitch) { continue; }
		const auto surface = BuildFarmSoilSurface(grid, index, *this);
		if (!surface.valid) { continue; }
		for (int z = 0; z < 3; ++z) for (int x = 0; x < 3; ++x) {
			const auto& a = surface.At(x, z);
			const auto& d = surface.At(x + 1, z + 1);
			const float u = (position.x - a.x) / (d.x - a.x);
			const float v = (position.z - a.z) / (d.z - a.z);
			if (u < 0.0f || v < 0.0f || u > 1.0f || v > 1.0f) { continue; }
			const bool upper = u + v > 1.0f;
			const auto slope = FarmSoilPatchSlope(surface, x, z, upper);
			const auto& anchor = upper ? d : a;
			const float result = anchor.y + slope.x * (position.x - anchor.x) + slope.y * (position.z - anchor.z);
			if (!std::isfinite(result)) { return false; }
			height = result;
			return true;
		}
	}
	return false;
}
namespace farm {
namespace {
constexpr float kRayEpsilon = 0.0001f;
constexpr float kSelectionLift = 0.06f;
constexpr float kSelectionMargin = 0.08f;
constexpr float kWaterFootprintRatio = 0.62f;
constexpr float kCropAnchorLift = 0.03f;

bool IsFinite(const Vector3& value) noexcept {
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float SanitizeStrength(float value) noexcept
{
	return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

void DrawDirectionArrow(
	LineDrawer& lineDrawer,
	const Vector3& center,
	float directionX,
	float directionZ,
	float length,
	const Vector4& color)
{
	const float directionLength = std::sqrt(
		directionX * directionX + directionZ * directionZ);
	if (!std::isfinite(directionLength) || directionLength <= kRayEpsilon) {
		return;
	}
	const float unitX = directionX / directionLength;
	const float unitZ = directionZ / directionLength;
	const float halfLength = length * 0.5f;
	const Vector3 start = {
		center.x - unitX * halfLength,
		center.y,
		center.z - unitZ * halfLength,
	};
	const Vector3 end = {
		center.x + unitX * halfLength,
		center.y,
		center.z + unitZ * halfLength,
	};
	const float headLength = length * 0.28f;
	const float headWidth = length * 0.16f;
	const Vector3 headBase = {
		end.x - unitX * headLength,
		end.y,
		end.z - unitZ * headLength,
	};
	lineDrawer.DrawLine(start, end, color);
	lineDrawer.DrawLine(
		end,
		{ headBase.x - unitZ * headWidth, headBase.y, headBase.z + unitX * headWidth },
		color);
	lineDrawer.DrawLine(
		end,
		{ headBase.x + unitZ * headWidth, headBase.y, headBase.z - unitX * headWidth },
		color);
}

void DrawHorizontalRectangle(
	LineDrawer& lineDrawer,
	const Vector3& center,
	float halfExtent,
	const Vector4& color)
{
	const Vector3 topLeft = { center.x - halfExtent, center.y, center.z - halfExtent };
	const Vector3 topRight = { center.x + halfExtent, center.y, center.z - halfExtent };
	const Vector3 bottomRight = { center.x + halfExtent, center.y, center.z + halfExtent };
	const Vector3 bottomLeft = { center.x - halfExtent, center.y, center.z + halfExtent };
	lineDrawer.DrawLine(topLeft, topRight, color);
	lineDrawer.DrawLine(topRight, bottomRight, color);
	lineDrawer.DrawLine(bottomRight, bottomLeft, color);
	lineDrawer.DrawLine(bottomLeft, topLeft, color);
}

Vector4 GetWaterStatusColor(FarmWaterStatus status) noexcept
{
	switch (status) {
	case FarmWaterStatus::Available: return { 0.08f, 0.72f, 0.98f, 1.0f };
	case FarmWaterStatus::Retained: return { 1.0f, 0.72f, 0.24f, 1.0f };
	case FarmWaterStatus::Waiting: return { 0.55f, 0.58f, 0.64f, 1.0f };
	default: return { 0.10f, 0.30f, 0.48f, 1.0f };
	}
}

Vector4 GetTileColor(const FarmTile& tile, FarmWaterStatus waterStatus)
{
	if (tile.feature != FarmTileFeature::None) {
		return GetWaterStatusColor(waterStatus);
	}
	if (tile.state == FarmTileState::Planted) {
		return IsHarvestReady(tile)
			? Vector4{ 1.0f, 0.78f, 0.12f, 1.0f }
			: Vector4{ 0.25f, 0.88f, 0.32f, 1.0f };
	}
	if (tile.state == FarmTileState::Tilled) {
		return tile.moisture > 0.5f
			? Vector4{ 0.20f, 0.62f, 0.95f, 1.0f }
			: Vector4{ 0.82f, 0.48f, 0.20f, 1.0f };
	}
	return { 0.38f, 0.76f, 0.28f, 1.0f };
}

bool HasDevelopedLeaves(FarmCropGrowthStage stage) noexcept
{
	return stage == FarmCropGrowthStage::Growing ||
		stage == FarmCropGrowthStage::AlmostReady ||
		stage == FarmCropGrowthStage::Ready;
}

bool HasVisibleCropBody(FarmCropGrowthStage stage) noexcept
{
	return stage == FarmCropGrowthStage::AlmostReady ||
		stage == FarmCropGrowthStage::Ready;
}

void DrawCropSilhouette(
	LineDrawer& lineDrawer,
	const FarmTile& tile,
	const FarmTileVisualData& visual)
{
	const FarmCropGrowthStage stage = visual.cropStage;
	if (stage == FarmCropGrowthStage::None) {
		return;
	}

	const Vector3& center = visual.center;
	const float growth = SanitizeStrength(tile.growth);
	const float scale = visual.cropScale;
	const float stemHeight = (0.16f + growth * 0.68f) * scale;
	const Vector3 stemBase = visual.cropAnchor;
	const Vector3 stemTop = { center.x, stemBase.y + stemHeight, center.z };
	const Vector4 stemColor = tile.crop == CropType::Carrot
		? Vector4{ 0.12f, 0.72f, 0.24f, 1.0f }
		: Vector4{ 0.34f, 0.94f, 0.30f, 1.0f };
	lineDrawer.DrawLine(stemBase, stemTop, stemColor);

	if (HasDevelopedLeaves(stage)) {
		const float leafWidth = (0.10f + growth * 0.22f) * scale;
		const float leafY = stemBase.y + stemHeight * 0.58f;
		lineDrawer.DrawLine(
			{ center.x, leafY, center.z },
			{ center.x - leafWidth, leafY + 0.08f * scale, center.z },
			stemColor);
		lineDrawer.DrawLine(
			{ center.x, leafY + 0.05f * scale, center.z },
			{ center.x + leafWidth, leafY + 0.13f * scale, center.z },
			stemColor);
		lineDrawer.DrawLine(
			{ center.x, leafY + 0.02f * scale, center.z },
			{ center.x, leafY + 0.10f * scale, center.z + leafWidth },
			stemColor);
	}

	if (HasVisibleCropBody(stage)) {
		if (tile.crop == CropType::Carrot) {
			const Vector4 rootColor = { 1.0f, 0.40f, 0.08f, 1.0f };
			const float shoulderWidth = (0.10f + growth * 0.06f) * scale;
			const float shoulderY = stemBase.y + 0.14f * scale;
			const Vector3 rootTip = stemBase;
			lineDrawer.DrawLine(
				{ center.x - shoulderWidth, shoulderY, center.z }, rootTip, rootColor);
			lineDrawer.DrawLine(
				{ center.x + shoulderWidth, shoulderY, center.z }, rootTip, rootColor);
			lineDrawer.DrawLine(
				{ center.x, shoulderY, center.z - shoulderWidth }, rootTip, rootColor);
			lineDrawer.DrawLine(
				{ center.x, shoulderY, center.z + shoulderWidth }, rootTip, rootColor);
		} else {
			const bool tomato = tile.crop == CropType::Tomato;
			const bool pumpkin = tile.crop == CropType::Pumpkin;
			const float bulbRadius = (pumpkin ? 0.25f : 0.10f + growth * 0.07f) * scale;
			lineDrawer.DrawWireSphere(
				{ center.x, tomato ? stemBase.y + stemHeight * 0.65f : center.y + bulbRadius, center.z },
				bulbRadius,
				tomato ? Vector4{0.8f, 0.15f, 0.1f, 1.0f} :
				pumpkin ? Vector4{0.15f, 0.38f, 0.15f, 1.0f} : Vector4{0.92f, 0.48f, 0.78f, 1.0f},
				8);
		}
	}

	if (stage == FarmCropGrowthStage::Ready) {
		lineDrawer.DrawWireCube(
			{ stemTop.x, stemTop.y + 0.12f, stemTop.z },
			0.24f,
			{ 1.0f, 0.72f, 0.08f, 1.0f });
	}
}
} // namespace

void FarmVisualSystem::Initialize(const FarmVisualLayout& layout, const FarmRules& rules) noexcept
{
	cropSizeSystem_.Initialize(rules);
	layout_ = layout;
	layout_.pondLipRise = std::isfinite(layout.pondLipRise) ? std::clamp(layout.pondLipRise,0.f,1.f) : 0.f;
	const FarmVisualLayout defaults;
	if (!IsFinite(layout_.center)) { layout_.center = defaults.center; }
	layout_.tileSize = std::isfinite(layout_.tileSize) ? (std::max)(layout_.tileSize, 0.1f) : defaults.tileSize;
	layout_.tileGap = std::isfinite(layout_.tileGap) ? (std::max)(layout_.tileGap, 0.0f) : defaults.tileGap;
	layout_.heightStep = std::isfinite(layout_.heightStep) ? (std::max)(layout_.heightStep, 0.0f) : defaults.heightStep;
	layout_.waterBottomOffset = std::isfinite(layout_.waterBottomOffset) ? layout_.waterBottomOffset : defaults.waterBottomOffset;
	layout_.waterMaxDepth = std::isfinite(layout_.waterMaxDepth)
		? (std::max)(layout_.waterMaxDepth, 0.0f) : defaults.waterMaxDepth;
}

Vector3 FarmVisualSystem::GetTileCenter(const FarmGrid& grid, int tileIndex) const noexcept
{
	const FarmTile* tile = grid.GetTile(tileIndex);
	const int width = grid.GetWidth();
	const int height = grid.GetHeight();
	if (tile == nullptr || width <= 0 || height <= 0) {
		return layout_.center;
	}

	const int column = tileIndex % width;
	const int row = tileIndex / width;
	const float pitch = layout_.tileSize + layout_.tileGap;
	return {
		layout_.center.x + (static_cast<float>(column) - static_cast<float>(width - 1) * 0.5f) * pitch,
		layout_.center.y + static_cast<float>(tile->heightLevel) * layout_.heightStep,
		layout_.center.z + (static_cast<float>(row) - static_cast<float>(height - 1) * 0.5f) * pitch,
	};
}

float FarmVisualSystem::GetCropRenderScale(const FarmTile& tile) const noexcept
{
	const auto size = cropSizeSystem_.Evaluate(tile);
	const float growth = SanitizeStrength(tile.growth);
	const float forecast = size.known ? size.multiplier : 1.0f;
	const float scale = 1.0f + (forecast - 1.0f) * growth;
	constexpr float kCropFootprintFraction = 0.90f;
	const float leafWidth = 0.10f + growth * 0.22f;
	return (std::min)(scale, layout_.tileSize * 0.5f * kCropFootprintFraction / leafWidth);
}

FarmTileVisualData FarmVisualSystem::GetTileVisualData(const FarmGrid& grid, int tileIndex) const noexcept
{
	const auto* tile = grid.GetTile(tileIndex);
	if (!tile || !IsValidFarmTileFeature(tile->feature)) { return {}; }
	FarmTileVisualData data;
	data.center = GetTileCenter(grid, tileIndex);
	data.halfExtent = layout_.tileSize * 0.5f;
	data.feature = tile->feature;
	data.cropAnchor = { data.center.x, data.center.y + kCropAnchorLift, data.center.z };
	data.waterHalfExtent = data.halfExtent * kWaterFootprintRatio;
	if (tile->feature == FarmTileFeature::None) {
		data.soilWetness = SanitizeStrength(tile->moisture);
		data.crop = tile->crop;
		data.cropStage = GetCropGrowthStage(*tile);
		if (data.cropStage != FarmCropGrowthStage::None) {
			// Render-only footprint cap; never change the harvest record to fit the tile.
			data.cropScale = GetCropRenderScale(*tile);
		}
	} else {
		data.waterFill = SanitizeStrength(tile->waterAmount);
	}
	// A newly fed or draining canal can contain less water than one D24 depth
	// step at overview distance. Keep the visible film separate from the bed;
	// the simulation, HUD amount, dry tiles and full reservoir stay unchanged.
	constexpr float minVisibleDepth = .008f;
	data.showWaterSurface = data.waterFill > 0.f && layout_.waterMaxDepth >= minVisibleDepth;
	const float depth = data.showWaterSurface
		? (std::max)(minVisibleDepth, layout_.waterMaxDepth * data.waterFill) : 0.f;
	data.waterSurfaceCenter = { data.center.x,
		data.center.y + layout_.waterBottomOffset + depth, data.center.z };
	if (!IsFinite(data.center) || !IsFinite(data.cropAnchor) || !IsFinite(data.waterSurfaceCenter)) { return {}; }
	data.valid = true;
	return data;
}

bool FarmVisualSystem::TryPickTile(
	const FarmGrid& grid,
	const Vector3& rayOrigin,
	const Vector3& rayDirection,
	int& outTileIndex,float* hitDistance) const noexcept
{
	outTileIndex = -1;
	if (grid.GetTileCount() <= 0 || !IsFinite(rayOrigin) || !IsFinite(rayDirection) ||
		(std::abs(rayDirection.x) <= kRayEpsilon && std::abs(rayDirection.y) <= kRayEpsilon && std::abs(rayDirection.z) <= kRayEpsilon)) {
		return false;
	}

	float nearestDistance = 3.402823466e+38F;
	const float halfExtent = (layout_.tileSize + layout_.tileGap) * 0.5f;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const auto visual = GetTileVisualData(grid, index);
		if (!visual.valid) { continue; }
		if (visual.feature == FarmTileFeature::None) {
			float distance = 0.0f;
			if (TryPickFarmSoilSurface(BuildFarmSoilSurface(grid,index,*this),rayOrigin,rayDirection,distance) && distance < nearestDistance) {
				nearestDistance = distance;
				outTileIndex = index;
			}
			continue;
		}
		if (std::abs(rayDirection.y) <= kRayEpsilon) { continue; }
		const Vector3 center = visual.center;
		const float distance = (center.y - rayOrigin.y) / rayDirection.y;
		if (!std::isfinite(distance) || distance < 0.0f || distance >= nearestDistance) {
			continue;
		}

		const Vector3 hit = rayOrigin + rayDirection * distance;
		if (!std::isfinite(hit.x) || !std::isfinite(hit.z)) {
			continue;
		}
		if (std::abs(hit.x - center.x) <= halfExtent &&
			std::abs(hit.z - center.z) <= halfExtent) {
			nearestDistance = distance;
			outTileIndex = index;
		}
	}
	if (hitDistance) *hitDistance = nearestDistance;
	return outTileIndex >= 0;
}

int farm::FarmVisualSystem::PickWorkTarget(const FarmGrid& grid,Vector3 eye,Vector3 direction,float reach) const noexcept {
    if (!IsFinite(eye) || !IsFinite(direction) || !(reach>0.f)) return -1;
    int hit=-1; float distance=reach;
    if (!TryPickTile(grid,eye,direction,hit,&distance) || distance>reach) { hit=-1; distance=reach; }
    // A visible plant can be in front of a different soil tile at eye level.
    for (int i=0;i<grid.GetTileCount();++i) {
        const auto* tile=grid.GetTile(i);
        if (!tile || tile->crop==CropType::None) continue;
        const auto v=GetTileVisualData(grid,i);
        const float radius=(.09f+.23f*std::clamp(tile->growth,0.f,1.f))*v.cropScale;
        const auto offset=eye-(v.cropAnchor+Vector3{0,radius,0});
        const float b=offset.x*direction.x+offset.y*direction.y+offset.z*direction.z;
        const float disc=b*b-(offset.x*offset.x+offset.y*offset.y+offset.z*offset.z-radius*radius);
        if (disc<0) continue;
        const float t=-b-std::sqrt(disc);
        if(t>=0 && t<distance) {hit=i;distance=t;}
    }
    return hit;
}

void FarmVisualSystem::Draw(
	const FarmGrid& grid,
	const FarmIrrigationSystem& irrigationSystem,
	const FarmToolActionResult& selectedAction,
	LineDrawer& lineDrawer,
	const std::vector<int>* irrigationPreviewChangedTiles,
	bool debugGuides, bool wireCrops, const FarmIrrigationForecast* forecast) const
{
	const float halfExtent = layout_.tileSize * 0.5f;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile == nullptr) {
			continue;
		}

		const auto visual = GetTileVisualData(grid, index);
		if (!visual.valid) { continue; }
		const Vector3 center = visual.center;
		if (const auto* field = forecast ? forecast->Inspect(index) : nullptr; field && field->before != field->after) {
			const bool gain = field->after > field->before;
			constexpr Vector4 gainColor{0.22f,1.0f,0.92f,1.0f}, lossColor{1.0f,0.56f,0.18f,1.0f};
			const auto& color = gain ? gainColor : lossColor;
			// Put the sign on the front of the central plateau, clear of the crop and sloping edges.
			const Vector3 mark{center.x,center.y + kSelectionLift,center.z - halfExtent*0.60f};
			const float arm = halfExtent*0.22f;
			lineDrawer.DrawLine({mark.x-arm,mark.y,mark.z},{mark.x+arm,mark.y,mark.z},color);
			if (gain) lineDrawer.DrawLine({mark.x,mark.y,mark.z-arm},{mark.x,mark.y,mark.z+arm},color);
			DrawHorizontalRectangle(lineDrawer,{center.x,mark.y,center.z},halfExtent*0.83f,color);
		}
		const FarmWaterStatus waterStatus = irrigationSystem.GetWaterStatus(grid, index);
		const Vector4 tileColor = GetTileColor(*tile, waterStatus);
		if (debugGuides) {
		DrawHorizontalRectangle(lineDrawer, center, halfExtent, tileColor);
		if (visual.showWaterSurface) {
			const Vector4 waterColor = { 0.20f, 0.78f, 0.96f, 1.0f };
			DrawHorizontalRectangle(lineDrawer, visual.waterSurfaceCenter, visual.waterHalfExtent, waterColor);
			for (int side = -1; side <= 1; side += 2) {
				const float x = center.x + side * visual.waterHalfExtent;
				lineDrawer.DrawLine({ x, center.y + layout_.waterBottomOffset, center.z },
					{ x, visual.waterSurfaceCenter.y, center.z }, waterColor);
			}
		}
		if (tile->feature == FarmTileFeature::Canal) {
			Vector3 canalCenter = center;
			canalCenter.y += layout_.waterBottomOffset;
			const Vector4 flowColor = GetWaterStatusColor(waterStatus);
			DrawHorizontalRectangle(
				lineDrawer, canalCenter, visual.waterHalfExtent,
				flowColor);
			lineDrawer.DrawLine(
				{ canalCenter.x - halfExtent * 0.45f, canalCenter.y, canalCenter.z },
				{ canalCenter.x + halfExtent * 0.45f, canalCenter.y, canalCenter.z },
				flowColor);
			const int upstreamIndex = irrigationSystem.GetUpstreamTileIndex(index);
			if (upstreamIndex >= 0) {
				const Vector3 upstreamCenter = GetTileCenter(grid, upstreamIndex);
				canalCenter.y += 0.035f;
				DrawDirectionArrow(
					lineDrawer,
					canalCenter,
					center.x - upstreamCenter.x,
					center.z - upstreamCenter.z,
					halfExtent * 0.92f,
					flowColor);
			}
		} else if (tile->feature == FarmTileFeature::WaterSource) {
			Vector3 sourceCenter = center;
			sourceCenter.y += 0.18f;
			lineDrawer.DrawWireSphere(
				sourceCenter,
				halfExtent * 0.28f,
				{ 0.20f, 0.94f, 1.0f, 1.0f },
				12);
			lineDrawer.DrawLine(
				{ sourceCenter.x, center.y + 0.02f, sourceCenter.z },
				{ sourceCenter.x, sourceCenter.y + halfExtent * 0.58f, sourceCenter.z },
				{ 0.72f, 0.98f, 1.0f, 1.0f });
		}

		if (tile->feature == FarmTileFeature::None && tile->state != FarmTileState::Empty) {
			Vector3 moundCenter = center;
			moundCenter.y += 0.02f;
			DrawHorizontalRectangle(lineDrawer, moundCenter, halfExtent * 0.58f, tileColor);
			const int canalIndex = irrigationSystem.GetAvailableCanalIndex(grid, index);
			if (canalIndex >= 0) {
				const float irrigationStrength = SanitizeStrength(
					irrigationSystem.GetAvailableIrrigationStrength(grid, index));
				Vector3 canalCenter = GetTileCenter(grid, canalIndex);
				canalCenter.y += 0.09f;
				Vector3 irrigationCenter = center;
				irrigationCenter.y += 0.09f;
				const Vector4 irrigationColor = GetWaterStatusColor(waterStatus);
				lineDrawer.DrawLine(canalCenter, irrigationCenter, irrigationColor);
				lineDrawer.DrawWireSphere(
					irrigationCenter,
					halfExtent * (0.06f + 0.08f * irrigationStrength),
					irrigationColor,
					8);
			}
		}
		if (tile->feature == FarmTileFeature::None && visual.soilWetness > 0.05f) {
			const Vector4 moistureColor = { 0.18f, 0.68f, 1.0f, 1.0f };
			const float moistureExtent = halfExtent * std::clamp(visual.soilWetness, 0.15f, 0.85f);
			Vector3 moistureCenter = center;
			moistureCenter.y += 0.03f;
			lineDrawer.DrawLine(
				{ moistureCenter.x - moistureExtent, moistureCenter.y, moistureCenter.z },
				{ moistureCenter.x + moistureExtent, moistureCenter.y, moistureCenter.z },
				moistureColor);
		}

		}
		if (wireCrops && tile->feature == FarmTileFeature::None) {
			DrawCropSilhouette(lineDrawer, *tile, visual);
		}

		const bool irrigationPreviewChanged =
			irrigationPreviewChangedTiles != nullptr &&
			std::find(
				irrigationPreviewChangedTiles->begin(),
				irrigationPreviewChangedTiles->end(),
				index) != irrigationPreviewChangedTiles->end();
		if (irrigationPreviewChanged || (debugGuides && index == grid.GetSelectedIndex())) {
			const Vector4 selectionColor = irrigationPreviewChanged
				? Vector4{ 1.0f, 0.72f, 0.12f, 1.0f }
				: selectedAction.Succeeded()
				? Vector4{ 0.15f, 1.0f, 0.35f, 1.0f }
				: Vector4{ 1.0f, 0.22f, 0.12f, 1.0f };
			Vector3 selectionCenter = center;
			selectionCenter.y += kSelectionLift;
			DrawHorizontalRectangle(
				lineDrawer, selectionCenter, halfExtent + kSelectionMargin, selectionColor);
			DrawHorizontalRectangle(
				lineDrawer, selectionCenter, halfExtent + kSelectionMargin * 1.75f, selectionColor);
		}
	}
}

} // namespace farm
