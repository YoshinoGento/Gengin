#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"

namespace farm {
class FarmGrid;
}

enum class FarmMoistureStatus {
	Invalid,
	Dry,
	Low,
	Good,
	Excess,
};

struct FarmGrowthForecast {
	bool moistureValid = false;
	bool growing = false;
	farm::CropType profileCrop = farm::CropType::None;
	FarmMoistureStatus moistureStatus = FarmMoistureStatus::Invalid;
	float goodMoistureMinimum = 0.0f;
	float goodMoistureMaximum = 0.0f;
	float growthEfficiency = 0.0f;
	float growthPerSecond = 0.0f;
	float moistureDecayPerSecond = 0.0f;
	float irrigationRecoveryPerSecond = 0.0f;
	float rainRecoveryPerSecond = 0.0f;
	float irrigationStrength = 0.0f;
	float netMoisturePerSecond = 0.0f;
	float secondsUntilDry = -1.0f;
	float secondsUntilFullMoisture = -1.0f;
	float secondsUntilReady = -1.0f;
	bool irrigationAvailable = false;
	bool irrigationActive = false;
};

enum class FarmWaterAdvice {
	Unknown, Till, Plant, Harvest, Water, CheckSupply, CloseIntake, AvoidWater, Monitor,
};

struct FarmWaterGuidance {
	bool visible = false;
	bool intakeClosed = false;
	farm::FarmWaterStatus supply = farm::FarmWaterStatus::None;
	FarmWaterAdvice advice = FarmWaterAdvice::Unknown;
	[[nodiscard]] bool HasExcessMoisture() const noexcept {
		return advice == FarmWaterAdvice::CloseIntake || advice == FarmWaterAdvice::AvoidWater;
	}
};

[[nodiscard]] inline const char* FarmWaterAdviceText(FarmWaterAdvice advice) noexcept
{
	switch (advice) {
	case FarmWaterAdvice::Till: return "Cultivate soil before irrigation.";
	case FarmWaterAdvice::Plant: return "Plant a crop before checking growth.";
	case FarmWaterAdvice::Harvest: return "Ready to harvest; no more watering needed.";
	case FarmWaterAdvice::Water: return "Low moisture: water with the watering can.";
	case FarmWaterAdvice::CheckSupply: return "Low moisture: check soil after irrigation.";
	case FarmWaterAdvice::CloseIntake: return "Excess moisture: close automatic intake.";
	case FarmWaterAdvice::AvoidWater: return "Excess moisture: avoid additional watering.";
	case FarmWaterAdvice::Monitor: return "Good moisture: monitor while growing.";
	default: return "Water guidance unavailable.";
	}
}

class FarmGrowthSystem final {
public:
	// Advice describes the current state; availability is not measured delivery.
	[[nodiscard]] static FarmWaterGuidance AnalyzeWater(
		const farm::FarmTile* tile, const FarmGrowthForecast& forecast,
		farm::FarmWaterStatus supply) noexcept;
	void Initialize(const farm::FarmRules& rules = {}) noexcept;
    [[nodiscard]] float GetMaxUpdateDeltaTime() const noexcept { return rules_.maxUpdateDeltaTime; }
	// Reports persistent tile changes, including repaired nonfinite values.
	bool Update(
		farm::FarmGrid& grid,
		float deltaTime,
		float timeScale) const;
	[[nodiscard]] FarmGrowthForecast Evaluate(
		const farm::FarmTile& tile,
		farm::CropType previewCrop = farm::CropType::None,
		float timeScale = 1.0f,
		float irrigationStrength = 0.0f, float soilRainPerSecond = 0.0f) const noexcept;

private:
	farm::FarmRules rules_{};
};
