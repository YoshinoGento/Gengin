#pragma once

#include "math/Matrix.h"

namespace farm {

// Canal seepage, not a crop-specific moisture target. Rain and hand watering are separate inputs.
struct FarmCanalSeepage {
	static constexpr float kFullRateUntil = 0.70f;
	static constexpr float kStopsAt = 0.96f;
	static_assert(0.0f < kFullRateUntil && kFullRateUntil < kStopsAt && kStopsAt < 1.0f);
	[[nodiscard]] static constexpr float Fraction(float moisture) noexcept {
		if (moisture <= kFullRateUntil) return 1.0f;
		return moisture < kStopsAt ? (kStopsAt - moisture) / (kStopsAt - kFullRateUntil) : 0.0f;
	}
};

struct FarmCropGrowthProfile {
	float growthPerSecondDry = 1.0f / 25.0f;
	float growthPerSecondWet = 1.0f / 10.0f;
	float dryMoistureThreshold = 0.05f;
	float moistureDecayPerSecond = 1.0f / 60.0f;
	float goodMoistureMinimum = 0.20f;
	float goodMoistureMaximum = 0.70f;
	float saturatedGrowthMultiplier = 0.25f;
};

struct FarmRules {
	float minimumCropSizeMultiplier = 0.50f;
	float maximumCropSizeMultiplier = 2.00f;
	int gridWidth = 3;
	int gridHeight = 3;
	float tileSpacing = 2.15f;
	Vector3 origin = { 0.0f, -1.92f, 8.0f };

	float initialCropGrowth = 0.05f;
	float maxCropGrowth = 1.0f;
	FarmCropGrowthProfile testCropGrowth{};
	FarmCropGrowthProfile carrotGrowth{
		1.0f / 36.0f,
		1.0f / 15.0f,
		0.08f,
		1.0f / 55.0f,
		0.35f,
		0.80f,
		0.25f,
	};
	// Provisional game profiles: tomato rewards steady water, pumpkin grows slowly.
	FarmCropGrowthProfile tomatoGrowth{1.0f/100.0f, 1.0f/24.0f, 0.08f, 1.0f/65.0f, 0.45f, 0.65f, 0.10f};
	FarmCropGrowthProfile pumpkinGrowth{1.0f/180.0f, 1.0f/40.0f, 0.10f, 1.0f/80.0f, 0.50f, 0.80f, 0.15f};
	int tomatoSeedPrice = 90;
	int pumpkinSeedPrice = 140;
	int initialTomatoSeedCount = 0;
	int initialPumpkinSeedCount = 0;
	int tomatoHarvestPrice = 260;
	int pumpkinHarvestPrice = 420;
	float tomatoIdealHarvestMoisture = 0.55f;
	float pumpkinIdealHarvestMoisture = 0.65f;
	int tomatoIdealHeight = 1;
	int pumpkinIdealHeight = 0;
	float tomatoMaximumSizeMultiplier = 2.0f;
	float pumpkinMaximumSizeMultiplier = 3.0f;
	float maxMoisture = 1.0f;
	// Moderate soil intake leaves time to react; canal stock/transfer remain independently conserved.
	float irrigationMoistureRecoveryPerSecond = 1.0f / 20.0f;
	float irrigationSourceStrength = 1.0f;
	float irrigationCanalTransferEfficiency = 0.90f;
	float irrigationSourceRefillPerSecond = 0.5f;
	float irrigationWaterTransferPerSecond = 0.4f;
	float postHarvestMoisture = 0.35f;
	float maxUpdateDeltaTime = 0.25f;

	int initialMoney = 300;
	int initialTestCropSeedCount = 0;
	int testCropSeedPrice = 40;
	int initialCarrotSeedCount = 0;
	int carrotSeedPrice = 60;
	int carrotHarvestPrice = 170;
	int clearMoneyTarget = 540;
	int normalHarvestPrice = 120;
	int rareHarvestPrice = 500;
	int rareHarvestInterval = 3;

	float testCropIdealHarvestMoisture = 0.55f;
	float carrotIdealHarvestMoisture = 0.65f;
	int testCropIdealHeight = 0;
	int carrotIdealHeight = 1;
	float qualityMaturityWeight = 0.25f;
	float qualityWaterBalanceWeight = 0.50f;
	float qualityTerrainFitWeight = 0.25f;
	float qualityNutrientWeight = 0.25f;
	float qualityHeightTolerance = 2.0f;
	float minimumQualityPriceMultiplier = 0.50f;
	float maximumQualityPriceMultiplier = 1.50f;
	float wateringMoistureIncrement = 0.10f;
};

} // namespace farm
