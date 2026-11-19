#pragma once

#include "farm/system/FarmToolActionSystem.h"
#include "math/Struct.h"

#include <vector>

class LineDrawer;

namespace farm {

inline constexpr float kFarmSoilRidgeHeight = .024f;

class FarmGrid;
class FarmIrrigationSystem;
class FarmIrrigationForecast;

struct FarmVisualLayout {
	Vector3 center = { 0.0f, 0.05f, 8.0f };
	float tileSize = 1.25f;
	float tileGap = 0.18f;
	float heightStep = 0.18f;
	float waterBottomOffset = 0.025f;
	float waterMaxDepth = 0.12f;
};

// Value-only presentation contract. Mesh/Material ownership stays with the renderer.
struct FarmTileVisualData {
	bool valid = false;
	Vector3 center{};
	float halfExtent = 0.0f;
	float soilWetness = 0.0f;
	FarmTileFeature feature = FarmTileFeature::None;
	CropType crop = CropType::None;
	FarmCropGrowthStage cropStage = FarmCropGrowthStage::None;
	Vector3 cropAnchor{};
	float cropScale = 1.0f;
	float waterFill = 0.0f;
	bool showWaterSurface = false;
	Vector3 waterSurfaceCenter{};
	float waterHalfExtent = 0.0f;
};

// Read-only world representation shared by rendering and viewport picking.
class FarmVisualSystem final {
public:
	void Initialize(const FarmVisualLayout& layout, const FarmRules& rules = {}) noexcept;

	[[nodiscard]] Vector3 GetTileCenter(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] FarmTileVisualData GetTileVisualData(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] float GetCropRenderScale(const FarmTile& tile) const noexcept;
	[[nodiscard]] bool TryPickTile(
		const FarmGrid& grid,
		const Vector3& rayOrigin,
		const Vector3& rayDirection,
		int& outTileIndex) const noexcept;

	void Draw(
		const FarmGrid& grid,
		const FarmIrrigationSystem& irrigationSystem,
		const FarmToolActionResult& selectedAction,
		LineDrawer& lineDrawer,
		const std::vector<int>* irrigationPreviewChangedTiles = nullptr,
		bool debugGuides = true, bool wireCrops = true,
		const FarmIrrigationForecast* forecast = nullptr) const;

	[[nodiscard]] const FarmVisualLayout& GetLayout() const noexcept { return layout_; }
	[[nodiscard]] bool TryGetSoilHeight(const FarmGrid& grid, const Vector3& position, float& height) const noexcept;

private:
	FarmVisualLayout layout_{};
	FarmCropSizeSystem cropSizeSystem_{};
};

} // namespace farm
