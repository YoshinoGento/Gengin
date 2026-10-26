#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmCropSizeSystem.h"

#include <array>

struct FarmCropQualityResult {
	farm::CropType crop = farm::CropType::None;
	float maturity = 0.0f;
	float waterBalance = 0.0f;
	float terrainFit = 0.0f;
	float nutrientBalance = 1.0f;
	bool nutrientKnown = false;
	int score = 0;
	int basePrice = 0;
	int salePrice = 0;
	FarmCropSizeResult harvestSize{};

	[[nodiscard]] bool IsValid() const noexcept {
		return farm::IsPlantableCrop(crop) && basePrice > 0 && salePrice > 0 && harvestSize.IsConsistent();
	}
};

enum class FarmQualityFocus { None, Maturity, Water, Terrain, Nutrients, Balanced, Unknown };
struct FarmQualityAdvice {
	FarmQualityFocus focus = FarmQualityFocus::None;
	int lowestPercent = -1;
	bool partial = false;
};

class FarmCropQualitySystem final {
public:
	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	[[nodiscard]] float MaximumHarvestSize(farm::CropType crop) const noexcept { return sizeSystem_.MaximumFor(crop); }
	[[nodiscard]] FarmCropQualityResult Evaluate(
		const farm::FarmTile& tile) const noexcept;
	[[nodiscard]] static FarmQualityAdvice Analyze(const FarmCropQualityResult& quality) noexcept;

private:
	FarmCropSizeSystem sizeSystem_{};
	std::array<int, farm::kFarmCropTypeCount> basePrices_{ 120, 170 };
	std::array<float, farm::kFarmCropTypeCount> idealMoisture_{ 0.55f, 0.65f };
	std::array<int, farm::kFarmCropTypeCount> idealHeight_{ 0, 1 };
	float maturityWeight_ = 0.25f;
	float waterBalanceWeight_ = 0.50f;
	float terrainFitWeight_ = 0.25f;
	float nutrientWeight_ = 0.25f;
	float heightTolerance_ = 2.0f;
	float minimumPriceMultiplier_ = 0.50f;
	float maximumPriceMultiplier_ = 1.50f;
};
