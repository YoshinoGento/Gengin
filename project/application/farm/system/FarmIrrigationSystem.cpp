#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmWeatherSystem.h"

#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace farm {
namespace {
constexpr std::array<int, 4> kNeighborX = { 0, 0, -1, 1 };
constexpr std::array<int, 4> kNeighborY = { -1, 1, 0, 0 };

float SanitizeStrength(float value, float fallback) noexcept
{
	return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
}
}

void FarmIrrigationSystem::Initialize(const FarmRules& rules) noexcept
{
	lastStep_ = {};
	lastTileFlows_.clear();
	measuredTiles_.clear();
	measuredGrid_ = nullptr;
	const FarmRules defaults{};
	auto rate = [](float value, float fallback) {
		return std::isfinite(value) && value >= 0.0f ? value : fallback;
	};
	refillPerSecond_ = rate(rules.irrigationSourceRefillPerSecond, defaults.irrigationSourceRefillPerSecond);
	transferPerSecond_ = rate(rules.irrigationWaterTransferPerSecond, defaults.irrigationWaterTransferPerSecond);
	recoveryPerSecond_ = rate(rules.irrigationMoistureRecoveryPerSecond, defaults.irrigationMoistureRecoveryPerSecond);
	maxDeltaTime_ = std::isfinite(rules.maxUpdateDeltaTime) && rules.maxUpdateDeltaTime > 0.0f
		? rules.maxUpdateDeltaTime : defaults.maxUpdateDeltaTime;
	supplied_.clear();
	irrigationRange_.clear();
	supplyStrengths_.clear();
	irrigationStrengths_.clear();
	downstreamCanalCounts_.clear();
	upstreamTileIndices_.clear();
	supplyingCanalIndices_.clear();
	traversalQueue_.clear();
	waterSourceCount_ = 0;
	suppliedCanalCount_ = 0;
	irrigationRangeTileCount_ = 0;
	sourceStrength_ = SanitizeStrength(
		rules.irrigationSourceStrength, defaults.irrigationSourceStrength);
	canalTransferEfficiency_ = SanitizeStrength(
		rules.irrigationCanalTransferEfficiency,
		defaults.irrigationCanalTransferEfficiency);
}

void FarmIrrigationSystem::Rebuild(const FarmGrid& grid)
{
	if (lastStep_.valid && !MeasurementMatches(grid)) { lastStep_.valid = false; }
	const int tileCount = grid.GetTileCount();
	waterSourceCount_ = 0;
	suppliedCanalCount_ = 0;
	irrigationRangeTileCount_ = 0;
	if (tileCount <= 0 || grid.GetWidth() <= 0 || grid.GetHeight() <= 0) {
		supplied_.clear();
		irrigationRange_.clear();
		supplyStrengths_.clear();
		irrigationStrengths_.clear();
		downstreamCanalCounts_.clear();
		upstreamTileIndices_.clear();
		supplyingCanalIndices_.clear();
		traversalQueue_.clear();
		return;
	}

	const std::size_t requiredSize = static_cast<std::size_t>(tileCount);
	if (supplied_.size() != requiredSize) {
		supplied_.resize(requiredSize);
	}
	if (irrigationRange_.size() != requiredSize) {
		irrigationRange_.resize(requiredSize);
	}
	if (supplyStrengths_.size() != requiredSize) {
		supplyStrengths_.resize(requiredSize);
	}
	if (irrigationStrengths_.size() != requiredSize) {
		irrigationStrengths_.resize(requiredSize);
	}
	if (downstreamCanalCounts_.size() != requiredSize) {
		downstreamCanalCounts_.resize(requiredSize);
	}
	if (upstreamTileIndices_.size() != requiredSize) {
		upstreamTileIndices_.resize(requiredSize);
	}
	if (supplyingCanalIndices_.size() != requiredSize) {
		supplyingCanalIndices_.resize(requiredSize);
	}
	std::fill(supplied_.begin(), supplied_.end(), std::uint8_t{ 0 });
	std::fill(irrigationRange_.begin(), irrigationRange_.end(), std::uint8_t{ 0 });
	std::fill(supplyStrengths_.begin(), supplyStrengths_.end(), 0.0f);
	std::fill(irrigationStrengths_.begin(), irrigationStrengths_.end(), 0.0f);
	std::fill(downstreamCanalCounts_.begin(), downstreamCanalCounts_.end(), 0);
	std::fill(upstreamTileIndices_.begin(), upstreamTileIndices_.end(), -1);
	std::fill(supplyingCanalIndices_.begin(), supplyingCanalIndices_.end(), -1);
	traversalQueue_.clear();
	if (traversalQueue_.capacity() < requiredSize) {
		traversalQueue_.reserve(requiredSize);
	}

	for (int index = 0; index < tileCount; ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile != nullptr && tile->feature == FarmTileFeature::WaterSource) {
			supplied_[static_cast<std::size_t>(index)] = 1;
			supplyStrengths_[static_cast<std::size_t>(index)] = sourceStrength_;
			traversalQueue_.push_back(index);
			++waterSourceCount_;
		}
	}

	std::size_t queueHead = 0;
	while (queueHead < traversalQueue_.size()) {
		const int currentIndex = traversalQueue_[queueHead++];
		const FarmTile* currentTile = grid.GetTile(currentIndex);
		if (currentTile == nullptr) {
			continue;
		}
		const int currentX = currentIndex % grid.GetWidth();
		const int currentY = currentIndex / grid.GetWidth();
		for (std::size_t direction = 0; direction < kNeighborX.size(); ++direction) {
			const int nextX = currentX + kNeighborX[direction];
			const int nextY = currentY + kNeighborY[direction];
			if (nextX < 0 || nextX >= grid.GetWidth() ||
				nextY < 0 || nextY >= grid.GetHeight()) {
				continue;
			}
			const int nextIndex = nextY * grid.GetWidth() + nextX;
			if (supplied_[static_cast<std::size_t>(nextIndex)] != 0) {
				continue;
			}
			const FarmTile* nextTile = grid.GetTile(nextIndex);
			if (nextTile == nullptr || nextTile->feature != FarmTileFeature::Canal ||
				nextTile->heightLevel > currentTile->heightLevel) {
				continue;
			}
			supplied_[static_cast<std::size_t>(nextIndex)] = 1;
			upstreamTileIndices_[static_cast<std::size_t>(nextIndex)] = currentIndex;
			traversalQueue_.push_back(nextIndex);
			++suppliedCanalCount_;
		}
	}

	for (int index : traversalQueue_) {
		const int upstreamIndex = GetUpstreamTileIndex(index);
		if (upstreamIndex >= 0 && upstreamIndex < tileCount) {
			++downstreamCanalCounts_[static_cast<std::size_t>(upstreamIndex)];
		}
	}
	for (int index : traversalQueue_) {
		const int upstreamIndex = GetUpstreamTileIndex(index);
		if (upstreamIndex < 0 || upstreamIndex >= tileCount) {
			continue;
		}
		const int branchCount = (std::max)(
			downstreamCanalCounts_[static_cast<std::size_t>(upstreamIndex)], 1);
		const float parentStrength =
			supplyStrengths_[static_cast<std::size_t>(upstreamIndex)];
		supplyStrengths_[static_cast<std::size_t>(index)] = std::clamp(
			parentStrength * canalTransferEfficiency_ /
				static_cast<float>(branchCount),
			0.0f,
			1.0f);
	}

	for (int index = 0; index < tileCount; ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile == nullptr || tile->feature != FarmTileFeature::None) {
			continue;
		}

		const int tileX = index % grid.GetWidth();
		const int tileY = index / grid.GetWidth();
		float strongestSupply = 0.0f;
		int strongestCanalIndex = -1;
		for (std::size_t direction = 0; direction < kNeighborX.size(); ++direction) {
			const int neighborX = tileX + kNeighborX[direction];
			const int neighborY = tileY + kNeighborY[direction];
			if (neighborX < 0 || neighborX >= grid.GetWidth() ||
				neighborY < 0 || neighborY >= grid.GetHeight()) {
				continue;
			}

			const int neighborIndex = neighborY * grid.GetWidth() + neighborX;
			const FarmTile* neighbor = grid.GetTile(neighborIndex);
			if (neighbor == nullptr || neighbor->feature != FarmTileFeature::Canal ||
				!IsSupplied(neighborIndex) || tile->heightLevel > neighbor->heightLevel) {
				continue;
			}

			const float candidateStrength = GetSupplyStrength(neighborIndex);
			if (candidateStrength > strongestSupply) {
				strongestSupply = candidateStrength;
				strongestCanalIndex = neighborIndex;
			}
		}
		if (strongestCanalIndex >= 0) {
			irrigationRange_[static_cast<std::size_t>(index)] = 1;
			irrigationStrengths_[static_cast<std::size_t>(index)] = strongestSupply;
			supplyingCanalIndices_[static_cast<std::size_t>(index)] = strongestCanalIndex;
			++irrigationRangeTileCount_;
		}
	}
}

int FarmIrrigationSystem::FindWetCanal(const FarmGrid& grid, int tileIndex) const noexcept
{
	const FarmTile* soil = grid.GetTile(tileIndex);
	if (!soil || !soil->irrigationEnabled || soil->feature != FarmTileFeature::None || grid.GetWidth() <= 0) { return -1; }
	int best = -1;
	float bestStrength = 0.0f;
	for (std::size_t direction = 0; direction < kNeighborX.size(); ++direction) {
		const int x = tileIndex % grid.GetWidth() + kNeighborX[direction];
		const int y = tileIndex / grid.GetWidth() + kNeighborY[direction];
		if (x < 0 || x >= grid.GetWidth() || y < 0 || y >= grid.GetHeight()) { continue; }
		const int index = y * grid.GetWidth() + x;
		const FarmTile* canal = grid.GetTile(index);
		if (!canal || canal->feature != FarmTileFeature::Canal ||
			canal->heightLevel < soil->heightLevel || !std::isfinite(canal->waterAmount) ||
			canal->waterAmount <= 0.0f || canal->waterAmount > 1.0f) { continue; }
		const float strength = IsSupplied(index) ? GetSupplyStrength(index) : canalTransferEfficiency_;
		if (strength > bestStrength) { best = index; bestStrength = strength; }
	}
	return best;
}

float FarmIrrigationSystem::GetAvailableIrrigationStrength(const FarmGrid& grid, int tileIndex) const noexcept
{
	const int canal = FindWetCanal(grid, tileIndex);
	return canal < 0 ? 0.0f : (IsSupplied(canal) ? GetSupplyStrength(canal) : canalTransferEfficiency_);
}

FarmWaterStatus FarmIrrigationSystem::GetWaterStatus(const FarmGrid& grid, int tileIndex) const noexcept
{
	const FarmTile* tile = grid.GetTile(tileIndex);
	if (!tile || !IsValidFarmTileFeature(tile->feature) || grid.GetWidth() <= 0) {
		return FarmWaterStatus::None;
	}
	if (tile->feature != FarmTileFeature::None) {
		if (!std::isfinite(tile->waterAmount) || tile->waterAmount < 0.0f || tile->waterAmount > 1.0f) {
			return FarmWaterStatus::None;
		}
		const bool connected = tile->feature == FarmTileFeature::WaterSource || IsSupplied(tileIndex);
		if (tile->waterAmount > 0.0f) {
			return connected ? FarmWaterStatus::Available : FarmWaterStatus::Retained;
		}
		return connected ? FarmWaterStatus::Waiting : FarmWaterStatus::Dry;
	}
	const int wetCanal = FindWetCanal(grid, tileIndex);
	if (!tile->irrigationEnabled) { return FarmWaterStatus::None; }
	if (wetCanal >= 0) {
		return IsSupplied(wetCanal) ? FarmWaterStatus::Available : FarmWaterStatus::Retained;
	}
	FarmWaterStatus result = FarmWaterStatus::None;
	for (std::size_t direction = 0; direction < kNeighborX.size(); ++direction) {
		const int x = tileIndex % grid.GetWidth() + kNeighborX[direction];
		const int y = tileIndex / grid.GetWidth() + kNeighborY[direction];
		if (x < 0 || x >= grid.GetWidth() || y < 0 || y >= grid.GetHeight()) { continue; }
		const int index = y * grid.GetWidth() + x;
		const FarmTile* canal = grid.GetTile(index);
		if (!canal || canal->feature != FarmTileFeature::Canal || canal->heightLevel < tile->heightLevel ||
			!std::isfinite(canal->waterAmount) || canal->waterAmount != 0.0f) { continue; }
		if (IsSupplied(index)) { return FarmWaterStatus::Waiting; }
		result = FarmWaterStatus::Dry;
	}
	return result;
}

bool FarmIrrigationSystem::MeasurementMatches(const FarmGrid& grid) const noexcept
{
	if (!lastStep_.valid || measuredGrid_ != &grid || measuredGeneration_ != grid.GetGeneration() ||
		measuredTiles_.size() != static_cast<std::size_t>(grid.GetTileCount())) { return false; }
	for (int i = 0; i < grid.GetTileCount(); ++i) {
		const auto* tile = grid.GetTile(i);
		const auto& identity = measuredTiles_[i];
		if (!tile || tile->heightLevel != identity.height || tile->feature != identity.feature ||
			tile->state != identity.state || tile->crop != identity.crop ||
			tile->irrigationEnabled != identity.irrigationEnabled) { return false; }
	}
	return true;
}

FarmIrrigationStepSummary FarmIrrigationSystem::GetLastStep(const FarmGrid& grid) const noexcept
{
	return MeasurementMatches(grid) ? lastStep_ : FarmIrrigationStepSummary{};
}

std::span<const FarmIrrigationTileFlow> FarmIrrigationSystem::GetLastTileFlows(const FarmGrid& grid) const noexcept
{
	return MeasurementMatches(grid) ? std::span<const FarmIrrigationTileFlow>(lastTileFlows_)
		: std::span<const FarmIrrigationTileFlow>{};
}

bool FarmIrrigationSystem::UpdateWater(FarmGrid& grid, float deltaTime, float timeScale, float soilRainPerSecond)
{
	lastStep_ = {};
	if (!std::isfinite(deltaTime) || !std::isfinite(timeScale) || deltaTime <= 0.0f || timeScale <= 0.0f ||
		!std::isfinite(soilRainPerSecond) || soilRainPerSecond < 0.0f || soilRainPerSecond > FarmWeatherSystem::kSoilRainPerSecond) { return false; }
	bool changed = false;
	const double dt = static_cast<double>((std::min)(deltaTime, maxDeltaTime_)) * timeScale;
	Rebuild(grid);
	const int count = grid.GetTileCount();
	const auto size = static_cast<std::size_t>(count);
	if (count <= 0) { return false; }
	lastTileFlows_.assign(size, {});
	measuredTiles_.resize(size);
	measuredGrid_ = &grid;
	measuredGeneration_ = grid.GetGeneration();
	lastStep_.simulatedSeconds = dt;
	bool measurable = true;
	waterBefore_.resize(size);
	waterDelta_.assign(size, 0.0f);
	soilDemand_.assign(size, 0.0f);
	canalDemand_.assign(size, 0.0f);
	soilCanals_.assign(size, -1);
	for (int i = 0; i < count; ++i) {
		auto tile = *grid.GetTile(i);
		const float oldWater = tile.waterAmount;
		if (soilRainPerSecond > 0.0f && tile.feature == FarmTileFeature::None &&
			tile.state != FarmTileState::Empty && std::isfinite(tile.moisture) && tile.moisture >= 0.0f &&
			tile.moisture < FarmWeatherSystem::kRainMoistureLimit) {
			const float previous = tile.moisture;
			tile.moisture = static_cast<float>((std::min)(static_cast<double>(FarmWeatherSystem::kRainMoistureLimit),
				tile.moisture + soilRainPerSecond * dt));
			lastTileFlows_[i].rainReceived = tile.moisture - previous;
			lastStep_.rainDelivered += lastTileFlows_[i].rainReceived;
			changed |= tile.moisture != previous;
		}
		measuredTiles_[i] = { tile.heightLevel, tile.feature, tile.state, tile.crop, tile.irrigationEnabled };
		measurable &= std::isfinite(oldWater) && oldWater >= 0.0f && oldWater <= 1.0f &&
			std::isfinite(tile.moisture) && tile.moisture >= 0.0f && tile.moisture <= 1.0f &&
			IsValidFarmTileFeature(tile.feature) &&
			(tile.feature != FarmTileFeature::None || oldWater == 0.0f);
		if (std::isfinite(oldWater)) { lastStep_.stockBefore += oldWater; }
		tile.waterAmount = tile.feature == FarmTileFeature::None ? 0.0f : SanitizeStrength(tile.waterAmount, 0.0f);
		if (tile.feature == FarmTileFeature::WaterSource) {
			tile.waterAmount = static_cast<float>((std::min)(1.0, tile.waterAmount + refillPerSecond_ * dt));
		}
		waterBefore_[i] = tile.waterAmount;
		if (tile.feature == FarmTileFeature::WaterSource && std::isfinite(oldWater)) {
			lastTileFlows_[i].sourceRefill = tile.waterAmount - oldWater;
			lastStep_.sourceRefill += lastTileFlows_[i].sourceRefill;
		}
		changed |= oldWater != tile.waterAmount;
        (void)grid.SetTile(i, tile);
	}
	// All transfers read the same snapshot. Incoming water cannot travel twice in this step.
	for (int child : traversalQueue_) {
		const int parent = GetUpstreamTileIndex(child);
		if (parent < 0 || parent >= count) { continue; }
		const int branches = (std::max)(1, GetDownstreamCanalCount(parent));
		const float budget = static_cast<float>((std::min)(static_cast<double>(waterBefore_[parent]), transferPerSecond_ * dt));
		const float amount = (std::min)(budget / branches, 1.0f - waterBefore_[child]);
		changed |= amount > 0.0f;
		waterDelta_[parent] -= amount;
		waterDelta_[child] += amount;
		lastTileFlows_[parent].outgoing += amount;
		lastTileFlows_[child].incoming += amount;
		lastStep_.transferred += amount;
	}
	for (int i = 0; i < count; ++i) {
		auto tile = *grid.GetTile(i);
        tile.waterAmount = std::clamp(waterBefore_[i] + waterDelta_[i], 0.0f, 1.0f);
        (void)grid.SetTile(i, tile);
		waterBefore_[i] = grid.GetTile(i)->waterAmount;
	}
	// Soil demands share a canal's finite stock proportionally, independently of tile iteration order.
	for (int i = 0; i < count; ++i) {
		const auto& tile = *grid.GetTile(i);
		if (tile.feature != FarmTileFeature::None || tile.state == FarmTileState::Empty ||
			!std::isfinite(tile.moisture) || tile.moisture < 0.0f || tile.moisture > 1.0f) { continue; }
		const int canal = FindWetCanal(grid, i);
		if (canal < 0) { continue; }
		const float strength = IsSupplied(canal) ? GetSupplyStrength(canal) : canalTransferEfficiency_;
		const float demand = static_cast<float>((std::min)(
			static_cast<double>(1.0f - std::clamp(tile.moisture, 0.0f, 1.0f)), recoveryPerSecond_ * strength * dt));
		soilCanals_[i] = canal;
		soilDemand_[i] = demand;
		canalDemand_[canal] += demand;
	}
	for (int i = 0; i < count; ++i) {
		const int canal = soilCanals_[i];
		if (canal < 0 || canalDemand_[canal] <= 0.0f) { continue; }
		const float ratio = (std::min)(1.0f, waterBefore_[canal] / canalDemand_[canal]);
		const float delivered = soilDemand_[i] * ratio;
		changed |= delivered > 0.0f;
		auto soil = *grid.GetTile(i);
		const float moistureBefore = soil.moisture;
		soil.moisture = std::clamp(soil.moisture + delivered, 0.0f, 1.0f);
		(void)grid.SetTile(i, soil);
        const float actual = soil.moisture - moistureBefore;
		lastTileFlows_[i].soilReceived = actual;
		lastTileFlows_[canal].soilSent += actual;
		lastStep_.soilDelivered += actual;
	}
	for (int i = 0; i < count; ++i) {
		auto tile = *grid.GetTile(i);
        tile.waterAmount = (std::max)(0.0f, waterBefore_[i] - canalDemand_[i]);
        (void)grid.SetTile(i, tile);
		lastStep_.stockAfter += grid.GetTile(i)->waterAmount;
	}
	lastStep_.balanceError = lastStep_.stockBefore + lastStep_.sourceRefill -
		lastStep_.soilDelivered - lastStep_.stockAfter;
	lastStep_.valid = measurable;
	return changed;
}

bool FarmIrrigationSystem::IsSupplied(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplied_.size() &&
		supplied_[static_cast<std::size_t>(tileIndex)] != 0;
}

bool FarmIrrigationSystem::IsInIrrigationRange(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < irrigationRange_.size() &&
		irrigationRange_[static_cast<std::size_t>(tileIndex)] != 0;
}

float FarmIrrigationSystem::GetSupplyStrength(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplyStrengths_.size()
		? supplyStrengths_[static_cast<std::size_t>(tileIndex)]
		: 0.0f;
}

float FarmIrrigationSystem::GetIrrigationStrength(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < irrigationStrengths_.size()
		? irrigationStrengths_[static_cast<std::size_t>(tileIndex)]
		: 0.0f;
}

int FarmIrrigationSystem::GetDownstreamCanalCount(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < downstreamCanalCounts_.size()
		? downstreamCanalCounts_[static_cast<std::size_t>(tileIndex)]
		: 0;
}

int FarmIrrigationSystem::GetUpstreamTileIndex(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < upstreamTileIndices_.size()
		? upstreamTileIndices_[static_cast<std::size_t>(tileIndex)]
		: -1;
}

int FarmIrrigationSystem::GetSupplyingCanalIndex(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplyingCanalIndices_.size()
		? supplyingCanalIndices_[static_cast<std::size_t>(tileIndex)]
		: -1;
}

} // namespace farm
