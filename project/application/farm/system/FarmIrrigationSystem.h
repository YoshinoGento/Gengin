#pragma once

#include "farm/data/FarmRules.h"
#include "farm/core/FarmTypes.h"

#include <cstdint>
#include <vector>
#include <span>

namespace farm {

class FarmGrid;

struct FarmIrrigationTileFlow {
	float sourceRefill = 0.0f;
	float incoming = 0.0f;
	float outgoing = 0.0f;
	float soilReceived = 0.0f;
	float soilSent = 0.0f;
	float rainReceived = 0.0f;
};

struct FarmIrrigationStepSummary {
	bool valid = false;
	double simulatedSeconds = 0.0;
	double stockBefore = 0.0;
	double sourceRefill = 0.0;
	double transferred = 0.0;
	double soilDelivered = 0.0;
	double stockAfter = 0.0;
	double balanceError = 0.0;
	double rainDelivered = 0.0; // External soil input, excluded from canal stock balance.
};

// Owns finite reservoir flow and soil delivery; topology is potential reachability.
class FarmIrrigationSystem final {
public:
	void Initialize(const FarmRules& rules = {}) noexcept;
    [[nodiscard]] float GetMaxUpdateDeltaTime() const noexcept { return maxDeltaTime_; }
	void Rebuild(const FarmGrid& grid);
	bool UpdateWater(FarmGrid& grid, float deltaTime, float timeScale, float soilRainPerSecond = 0.0f);
	[[nodiscard]] FarmIrrigationStepSummary GetLastStep(const FarmGrid& grid) const noexcept;
	// View valid only until the next UpdateWater/Initialize; consumers must copy their view data.
	[[nodiscard]] std::span<const FarmIrrigationTileFlow> GetLastTileFlows(const FarmGrid& grid) const noexcept;
	[[nodiscard]] float GetAvailableIrrigationStrength(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] FarmWaterStatus GetWaterStatus(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] int GetAvailableCanalIndex(const FarmGrid& grid, int tileIndex) const noexcept {
		return FindWetCanal(grid, tileIndex);
	}

	[[nodiscard]] bool IsSupplied(int tileIndex) const noexcept;
	[[nodiscard]] bool IsInIrrigationRange(int tileIndex) const noexcept;
	[[nodiscard]] float GetSupplyStrength(int tileIndex) const noexcept;
	[[nodiscard]] float GetIrrigationStrength(int tileIndex) const noexcept;
	[[nodiscard]] int GetDownstreamCanalCount(int tileIndex) const noexcept;
	[[nodiscard]] int GetUpstreamTileIndex(int tileIndex) const noexcept;
	[[nodiscard]] int GetSupplyingCanalIndex(int tileIndex) const noexcept;
	[[nodiscard]] int GetWaterSourceCount() const noexcept { return waterSourceCount_; }
	[[nodiscard]] int GetSuppliedCanalCount() const noexcept { return suppliedCanalCount_; }
	[[nodiscard]] int GetIrrigationRangeTileCount() const noexcept {
		return irrigationRangeTileCount_;
	}

private:
	struct MeasuredTileIdentity {
		int height = 0;
		FarmTileFeature feature = FarmTileFeature::None;
		FarmTileState state = FarmTileState::Empty;
		CropType crop = CropType::None;
		bool irrigationEnabled = true;
	};
	[[nodiscard]] bool MeasurementMatches(const FarmGrid& grid) const noexcept;
	FarmIrrigationStepSummary lastStep_;
	std::vector<FarmIrrigationTileFlow> lastTileFlows_;
	std::vector<MeasuredTileIdentity> measuredTiles_;
	const FarmGrid* measuredGrid_ = nullptr; // Identity only, never dereferenced or owned.
	std::uint64_t measuredGeneration_ = 0;
	[[nodiscard]] int FindWetCanal(const FarmGrid& grid, int tileIndex) const noexcept;
	std::vector<float> waterBefore_, waterDelta_, soilDemand_, canalDemand_;
	std::vector<int> soilCanals_;
	float refillPerSecond_ = 0.5f;
	float transferPerSecond_ = 0.4f;
	float recoveryPerSecond_ = 0.125f;
	float maxDeltaTime_ = 0.25f;
	std::vector<std::uint8_t> supplied_;
	std::vector<std::uint8_t> irrigationRange_;
	std::vector<float> supplyStrengths_;
	std::vector<float> irrigationStrengths_;
	std::vector<int> downstreamCanalCounts_;
	std::vector<int> upstreamTileIndices_;
	std::vector<int> supplyingCanalIndices_;
	std::vector<int> traversalQueue_;
	int waterSourceCount_ = 0;
	int suppliedCanalCount_ = 0;
	int irrigationRangeTileCount_ = 0;
	float sourceStrength_ = 1.0f;
	float canalTransferEfficiency_ = 0.90f;
};

} // namespace farm
