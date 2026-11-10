#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmSoilSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kMinimumPositiveValue = 0.0001f;

float SanitizeNormalized(float value, float fallback) noexcept
{
	return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
}

float SanitizePositive(float value, float fallback) noexcept
{
	return std::isfinite(value) && value > kMinimumPositiveValue ? value : fallback;
}
}

FarmQualityAdvice FarmCropQualitySystem::Analyze(const FarmCropQualityResult& quality) noexcept
{
	FarmQualityAdvice advice;
	if (!quality.IsValid()) return advice;
	const std::array<float, 4> values{quality.maturity, quality.waterBalance, quality.terrainFit, quality.nutrientBalance};
	constexpr std::array<FarmQualityFocus, 4> axes{FarmQualityFocus::Maturity, FarmQualityFocus::Water,
		FarmQualityFocus::Terrain, FarmQualityFocus::Nutrients};
	int minimum = 100;
	advice.focus = FarmQualityFocus::Balanced;
	for (std::size_t i = 0; i < values.size(); ++i) {
		if ((i == 3 && !quality.nutrientKnown) || !std::isfinite(values[i]) || values[i] < 0 || values[i] > 1) {
			advice.partial = true;
			continue;
		}
		// Match rounded radar numbers; ties keep axis order, not an implied price priority.
		const int percent = static_cast<int>(std::lround(values[i] * 100));
		if (percent < minimum) {
			minimum = percent;
			advice.focus = axes[i];
			advice.lowestPercent = percent;
		}
	}
	if (advice.focus == FarmQualityFocus::Balanced && advice.partial) advice.focus = FarmQualityFocus::Unknown;
	return advice;
}

void FarmCropQualitySystem::Initialize(const farm::FarmRules& rules) noexcept
{
	sizeSystem_.Initialize(rules);
	basePrices_ = {
		rules.normalHarvestPrice > 0 ? rules.normalHarvestPrice : 120,
		rules.carrotHarvestPrice > 0 ? rules.carrotHarvestPrice : 170,
		rules.tomatoHarvestPrice > 0 ? rules.tomatoHarvestPrice : 260,
		rules.pumpkinHarvestPrice > 0 ? rules.pumpkinHarvestPrice : 420,
	};
	idealMoisture_ = {
		SanitizeNormalized(rules.testCropIdealHarvestMoisture, 0.55f),
		SanitizeNormalized(rules.carrotIdealHarvestMoisture, 0.65f),
		SanitizeNormalized(rules.tomatoIdealHarvestMoisture, 0.55f),
		SanitizeNormalized(rules.pumpkinIdealHarvestMoisture, 0.65f),
	};
	idealHeight_ = {
		std::max(rules.testCropIdealHeight, 0),
		std::max(rules.carrotIdealHeight, 0),
		std::max(rules.tomatoIdealHeight, 0), std::max(rules.pumpkinIdealHeight, 0),
	};
	maturityWeight_ = SanitizePositive(rules.qualityMaturityWeight, 0.25f);
	waterBalanceWeight_ = SanitizePositive(rules.qualityWaterBalanceWeight, 0.50f);
	terrainFitWeight_ = SanitizePositive(rules.qualityTerrainFitWeight, 0.25f);
	nutrientWeight_ = SanitizePositive(rules.qualityNutrientWeight, 0.25f);
	heightTolerance_ = SanitizePositive(rules.qualityHeightTolerance, 2.0f);
	minimumPriceMultiplier_ = SanitizePositive(
		rules.minimumQualityPriceMultiplier, 0.50f);
	maximumPriceMultiplier_ = SanitizePositive(
		rules.maximumQualityPriceMultiplier, 1.50f);
	if (maximumPriceMultiplier_ < minimumPriceMultiplier_) {
		std::swap(maximumPriceMultiplier_, minimumPriceMultiplier_);
	}
}

FarmCropQualityResult FarmCropQualitySystem::Evaluate(
	const farm::FarmTile& tile) const noexcept
{
	FarmCropQualityResult result{};
	result.crop = tile.crop;
	const int slot = farm::ToCropSlot(tile.crop);
	if (slot < 0 || !std::isfinite(tile.growth) || !std::isfinite(tile.moisture) ||
		!tile.careHistory.IsValid() || !tile.pests.IsValid() || !FarmSoilSystem::Valid(tile.soilNutrients) ||
		basePrices_[slot] <= 0) {
		return result;
	}

	result.maturity = std::clamp(tile.growth, 0.0f, 1.0f);
	const float moisture = std::clamp(tile.moisture, 0.0f, 1.0f);
	const float idealMoisture = idealMoisture_[slot];
	const float moistureRange = (std::max)(
		(std::max)(idealMoisture, 1.0f - idealMoisture), kMinimumPositiveValue);
	const float currentWaterBalance = std::clamp(
		1.0f - std::abs(moisture - idealMoisture) / moistureRange,
		0.0f,
		1.0f);
	result.waterBalance = tile.careHistory.GetObservedSeconds() > 0.0
		? tile.careHistory.GetAverageEfficiency()
		: currentWaterBalance;
	result.terrainFit = std::clamp(
		1.0f - std::abs(static_cast<float>(tile.heightLevel - idealHeight_[slot])) /
			heightTolerance_,
		0.0f,
		1.0f);

	const float totalWeight =
		maturityWeight_ + waterBalanceWeight_ + terrainFitWeight_ + nutrientWeight_;
	result.nutrientBalance = tile.careHistory.nutrientGrowth > 0.0f
		? std::clamp(tile.careHistory.nutrientSupply / tile.careHistory.nutrientGrowth, 0.0f, 1.0f)
		: FarmSoilSystem::Satisfaction(tile.soilNutrients, tile.crop);
	result.nutrientKnown = true;
	if (!std::isfinite(totalWeight) || totalWeight <= kMinimumPositiveValue) {
		return {};
	}
	const float baseScore = std::clamp(
		(result.maturity * maturityWeight_ +
			result.waterBalance * waterBalanceWeight_ +
			result.terrainFit * terrainFitWeight_ + result.nutrientBalance * nutrientWeight_) /
			totalWeight,
		0.0f,
		1.0f);
	const int roundedBaseScore = static_cast<int>(std::lround(baseScore * 100.0f));
	result.pestPenalty = (std::min)(roundedBaseScore, tile.pests.damageDays * farm::FarmPestState::kQualityPointsPerDay);
	result.score = roundedBaseScore - result.pestPenalty;
	const float normalizedScore = (std::max)(0.0f, baseScore - result.pestPenalty / 100.0f);
	result.basePrice = basePrices_[slot];
	const double multiplier = static_cast<double>(minimumPriceMultiplier_) +
		static_cast<double>(normalizedScore) *
			static_cast<double>(maximumPriceMultiplier_ - minimumPriceMultiplier_);
	const double price = static_cast<double>(result.basePrice) * multiplier;
	if (!std::isfinite(price) || price <= 0.0 ||
		price > static_cast<double>((std::numeric_limits<int>::max)())) {
		return {};
	}
	result.salePrice = static_cast<int>(std::lround(price));
	result.harvestSize = sizeSystem_.Evaluate(tile);
	return result;
}
