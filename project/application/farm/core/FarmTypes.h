#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace farm {

enum class FarmTileState {
	Empty,
	Tilled,
	Watered,
	Planted,
	ReadyToHarvest,
};

enum class FarmTileFeature : std::uint8_t {
	None,
	Canal,
	WaterSource,
};

enum class CropType {
	None,
	TestCrop,
	Carrot,
	Tomato,
	Pumpkin,
};

// Availability now, not a measurement of water delivered during the previous step.
enum class FarmWaterStatus : std::uint8_t {
	None, Available, Retained, Waiting, Dry,
};

inline const char* ToString(FarmWaterStatus status) noexcept
{
	switch (status) {
	case FarmWaterStatus::Available: return "Water available";
	case FarmWaterStatus::Retained: return "Retained water";
	case FarmWaterStatus::Waiting: return "Waiting for water";
	case FarmWaterStatus::Dry: return "Water depleted";
	default: return "No irrigation access";
	}
}

enum class FarmCropGrowthStage : std::uint8_t {
	None,
	Sprout,
	Growing,
	AlmostReady,
	Ready,
};

inline constexpr float kFarmGrowthStageGrowingMinimum = 0.25f;
inline constexpr float kFarmGrowthStageAlmostReadyMinimum = 0.70f;

// Slot0 is reserved for legacy turnips; never reinterpret saved IDs.
inline constexpr int kFarmCropTypeCount = 4;
inline constexpr int kPlayableCropCount = 3;

struct FarmCropCareHistory {
	float drySeconds = 0.0f;
	float lowSeconds = 0.0f;
	float goodSeconds = 0.0f;
	float excessSeconds = 0.0f;
	float efficiencySeconds = 0.0f;
	float nutrientGrowth = 0.0f;
	float nutrientSupply = 0.0f;

	[[nodiscard]] double GetObservedSeconds() const noexcept {
		return static_cast<double>(drySeconds) + static_cast<double>(lowSeconds) +
			static_cast<double>(goodSeconds) + static_cast<double>(excessSeconds);
	}

	[[nodiscard]] bool IsEmpty() const noexcept {
		return drySeconds == 0.0f && lowSeconds == 0.0f &&
			goodSeconds == 0.0f && excessSeconds == 0.0f &&
			efficiencySeconds == 0.0f && nutrientGrowth == 0.0f && nutrientSupply == 0.0f;
	}

	[[nodiscard]] bool IsValid() const noexcept {
		if (!std::isfinite(nutrientGrowth) || !std::isfinite(nutrientSupply) ||
			nutrientGrowth < 0.0f || nutrientGrowth > 1.0f ||
			nutrientSupply < 0.0f || nutrientSupply > nutrientGrowth + 0.0001f) return false;
		if (!std::isfinite(drySeconds) || !std::isfinite(lowSeconds) ||
			!std::isfinite(goodSeconds) || !std::isfinite(excessSeconds) ||
			!std::isfinite(efficiencySeconds) || drySeconds < 0.0f ||
			lowSeconds < 0.0f || goodSeconds < 0.0f || excessSeconds < 0.0f ||
			efficiencySeconds < 0.0f) {
			return false;
		}
		const double observedSeconds = GetObservedSeconds();
		const double tolerance = (std::max)(0.001, observedSeconds * 0.0001);
		return static_cast<double>(efficiencySeconds) <= observedSeconds + tolerance;
	}

	[[nodiscard]] float GetAverageEfficiency() const noexcept {
		const double observedSeconds = GetObservedSeconds();
		if (!IsValid() || observedSeconds <= 0.0) {
			return 0.0f;
		}
		return static_cast<float>(std::clamp(
			static_cast<double>(efficiencySeconds) / observedSeconds, 0.0, 1.0));
	}
};

inline int ToCropSlot(CropType crop) noexcept
{
	switch (crop) {
	case CropType::TestCrop:
		return 0;
	case CropType::Carrot:
		return 1;
	case CropType::Tomato: return 2;
	case CropType::Pumpkin: return 3;
	case CropType::None:
	default:
		return -1;
	}
}

inline CropType CropTypeFromSlot(int slot) noexcept
{
	switch (slot) {
	case 0:
		return CropType::TestCrop;
	case 1:
		return CropType::Carrot;
	case 2: return CropType::Tomato;
	case 3: return CropType::Pumpkin;
	default:
		return CropType::None;
	}
}

inline bool IsPlantableCrop(CropType crop) noexcept
{
	return ToCropSlot(crop) >= 0;
}

inline CropType PlayableCrop(int index) noexcept {
	return index >= 0 && index < kPlayableCropCount ? CropTypeFromSlot(index + 1) : CropType::None;
}

struct FarmPestState {
	static constexpr int kFirstWarningDay = 6;
	static constexpr int kWarningInterval = 10;
	static constexpr int kLastWarningDay = 26;
	static constexpr int kMaximumDamageDays = 3;
	static constexpr int kQualityPointsPerDay = 6;
	static constexpr int kMaximumPenalty = kMaximumDamageDays * kQualityPointsPerDay;

	int warningDay = 0;
	int damageDays = 0;

	static constexpr bool IsWarningDay(int day) noexcept {
		return day >= kFirstWarningDay && day <= kLastWarningDay &&
			(day - kFirstWarningDay) % kWarningInterval == 0;
	}
	constexpr bool IsValid() const noexcept {
		return (warningDay == 0 || IsWarningDay(warningDay)) &&
			damageDays >= 0 && damageDays <= kMaximumDamageDays;
	}
	bool operator==(const FarmPestState&) const noexcept = default;
};

struct FarmTile {
	int heightLevel = 0;
	FarmTileFeature feature = FarmTileFeature::None;
	FarmTileState state = FarmTileState::Empty;
	CropType crop = CropType::None;
	float moisture = 0.0f;
	float growth = 0.0f;
	// Reservoir capacity is one; soil moisture is stored separately.
	float waterAmount = 0.0f;
	// Soil intake preference; dormant on canal/source tiles, independent of manual watering.
	bool irrigationEnabled = true;
	float soilNutrients = 0.60f;
	FarmCropCareHistory careHistory{};
	FarmPestState pests{};
};

inline bool IsValidFarmTileFeature(FarmTileFeature feature) noexcept
{
	return feature == FarmTileFeature::None ||
		feature == FarmTileFeature::Canal ||
		feature == FarmTileFeature::WaterSource;
}

inline bool IsHarvestReady(const FarmTile& tile)
{
	return tile.state == FarmTileState::Planted &&
		tile.crop != CropType::None && tile.growth >= 1.0f;
}

inline FarmCropGrowthStage GetCropGrowthStage(const FarmTile& tile) noexcept
{
	if ((tile.state != FarmTileState::Planted &&
		tile.state != FarmTileState::ReadyToHarvest) ||
		!IsPlantableCrop(tile.crop) || !std::isfinite(tile.growth)) {
		return FarmCropGrowthStage::None;
	}
	if (tile.state == FarmTileState::ReadyToHarvest || tile.growth >= 1.0f) {
		return FarmCropGrowthStage::Ready;
	}
	if (tile.growth >= kFarmGrowthStageAlmostReadyMinimum) {
		return FarmCropGrowthStage::AlmostReady;
	}
	if (tile.growth >= kFarmGrowthStageGrowingMinimum) {
		return FarmCropGrowthStage::Growing;
	}
	return FarmCropGrowthStage::Sprout;
}

inline const char* ToString(FarmTileState state)
{
	switch (state) {
	case FarmTileState::Empty:
		return "Empty";
	case FarmTileState::Tilled:
		return "Tilled";
	case FarmTileState::Watered:
		return "Watered";
	case FarmTileState::Planted:
		return "Planted";
	case FarmTileState::ReadyToHarvest:
		return "ReadyToHarvest";
	default:
		return "Unknown";
	}
}

inline const char* ToString(FarmTileFeature feature)
{
	switch (feature) {
	case FarmTileFeature::None:
		return "None";
	case FarmTileFeature::Canal:
		return "Canal";
	case FarmTileFeature::WaterSource:
		return "WaterSource";
	default:
		return "Unknown";
	}
}

inline const char* ToString(CropType crop)
{
	switch (crop) {
	case CropType::None:
		return "None";
	case CropType::TestCrop:
		return "TestCrop";
	case CropType::Carrot:
		return "Carrot";
	case CropType::Tomato: return "Tomato";
	case CropType::Pumpkin: return "Pumpkin";
	default:
		return "Unknown";
	}
}

} // namespace farm
