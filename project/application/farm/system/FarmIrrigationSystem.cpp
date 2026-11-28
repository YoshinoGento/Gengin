#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmWeatherSystem.h"
#include "farm/system/FarmPondBank.h"

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

void FarmIrrigationSystem::Initialize(const FarmRules& rules,SoilIntake intake,CanalTransport transport) noexcept
{
	transport_=transport;
    intake_=intake;
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
	forwardEdges_.clear();
	waterSourceCount_ = grid.HasPondBank() ? 1 : 0;
	pondOutlets_ = 0;
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
		} else if (pond::IsOpen(grid,index)) {
			supplied_[index] = 1;
			traversalQueue_.push_back(index);
			++pondOutlets_;
			++suppliedCanalCount_;
		}
	}
	for (int index : traversalQueue_) {
		if (pond::IsOpen(grid,index)) supplyStrengths_[index] = sourceStrength_ / pondOutlets_;
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

	if(transport_==CanalTransport::LocalEdges) {
		distance_.assign(requiredSize,-1);
		for(int i:traversalQueue_) {
			const int parent=GetUpstreamTileIndex(i);
			distance_[i]=parent<0?0:distance_[parent]+1;
		}
		// BFS distance is unique even when the parent is not. Retain all admissible next-rank faces.
		for(int a:traversalQueue_) for(int axis=0;axis<2;++axis) {
			if(axis==0 ? a%grid.GetWidth()+1>=grid.GetWidth() : a>=tileCount-grid.GetWidth()) continue;
			const int b=a+(axis==0?1:grid.GetWidth());
			if(distance_[b]<0 || std::abs(distance_[a]-distance_[b])!=1) continue;
			const int from=distance_[a]<distance_[b]?a:b,to=from==a?b:a;
			if(grid.GetTile(to)->heightLevel<=grid.GetTile(from)->heightLevel)
				forwardEdges_.push_back({from,to,axis*2+int(from==a)});
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
			tile->irrigationEnabled != identity.irrigationEnabled || tile->retainsPondBank != identity.retainsPondBank) { return false; }
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

void FarmIrrigationSystem::RecordTransfer(int from,int to,int face,float volume)
{
	waterDelta_[from]-=volume;
	waterDelta_[to]+=volume;
	lastTileFlows_[from].outward[face]+=volume;
	lastTileFlows_[to].outward[face^1]-=volume;
	lastTileFlows_[from].outgoing+=volume;
	lastTileFlows_[to].incoming+=volume;
	if(volume>0 && waterBefore_[to]==0) lastTileFlows_[to].firstInflowRevision=measurementRevision_+1;
	lastStep_.transferred+=volume;
}

void FarmIrrigationSystem::MoveAcrossEdges(double dt)
{
	const double budget=transferPerSecond_*dt;
	for(const auto& e:forwardEdges_) ++canalDemand_[e.from];
	const auto offer=[&](const CanalEdge& e) {
		return static_cast<float>((std::min)(double(waterBefore_[e.from]),budget)/canalDemand_[e.from]);
	};
	for(const auto& e:forwardEdges_) soilDemand_[e.to]+=offer(e);
	for(const auto& e:forwardEdges_) {
		const float sum=soilDemand_[e.to];
		if(sum<=0) continue;
		const float room=1.f-waterBefore_[e.to];
		RecordTransfer(e.from,e.to,e.face,offer(e)*(std::min)(1.f,room/sum));
	}
	std::fill(canalDemand_.begin(),canalDemand_.end(),0.f);
	std::fill(soilDemand_.begin(),soilDemand_.end(),0.f);
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
	const bool sameBoard=measuredGrid_==&grid && measuredGeneration_==grid.GetGeneration() && lastTileFlows_.size()==size;
	lastTileFlows_.resize(size);
	for (auto& flow:lastTileFlows_) {
		const auto receipt=sameBoard?flow.firstInflowRevision:0;
		flow={};
		flow.firstInflowRevision=receipt;
	}
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
		if (tile.feature!=FarmTileFeature::Canal || !IsSupplied(i)) lastTileFlows_[i].firstInflowRevision=0;
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
		measuredTiles_[i] = { tile.heightLevel, tile.feature, tile.state, tile.crop, tile.irrigationEnabled, tile.retainsPondBank };
		measurable &= std::isfinite(oldWater) && oldWater >= 0.0f && oldWater <= 1.0f &&
			std::isfinite(tile.moisture) && tile.moisture >= 0.0f && tile.moisture <= 1.0f &&
			IsValidFarmTileFeature(tile.feature) &&
			(tile.feature != FarmTileFeature::None || oldWater == 0.0f);
		if (std::isfinite(oldWater)) { lastStep_.stockBefore += oldWater; }
		tile.waterAmount = tile.feature == FarmTileFeature::None ? 0.0f : SanitizeStrength(tile.waterAmount, 0.0f);
		const bool pondInlet = pond::IsOpen(grid,i);
		if (tile.feature == FarmTileFeature::WaterSource || pondInlet) {
			const double rate = pondInlet ? refillPerSecond_ / pondOutlets_ : refillPerSecond_;
			tile.waterAmount = static_cast<float>((std::min)(1.0, tile.waterAmount + rate * dt));
		}
		waterBefore_[i] = tile.waterAmount;
		if ((tile.feature == FarmTileFeature::WaterSource || pondInlet) && std::isfinite(oldWater)) {
			lastTileFlows_[i].sourceRefill = tile.waterAmount - oldWater;
			lastStep_.sourceRefill += lastTileFlows_[i].sourceRefill;
			if (pondInlet && oldWater == 0.f && tile.waterAmount > 0.f)
				lastTileFlows_[i].firstInflowRevision = measurementRevision_+1;
		}
		changed |= oldWater != tile.waterAmount;
        (void)grid.SetTile(i, tile);
	}
	// All transfers read the same snapshot. Incoming water cannot travel twice in this step.
	if(transport_==CanalTransport::LocalEdges) {
		MoveAcrossEdges(dt);
		changed |= lastStep_.transferred>0;
	} else for (int child : traversalQueue_) {
		const int parent = GetUpstreamTileIndex(child);
		if (parent < 0 || parent >= count) { continue; }
		const int branches = (std::max)(1, GetDownstreamCanalCount(parent));
		const float budget = static_cast<float>((std::min)(static_cast<double>(waterBefore_[parent]), transferPerSecond_ * dt));
		const float amount = (std::min)(budget / branches, 1.0f - waterBefore_[child]);
		changed |= amount > 0.0f;
		const int face=parent/grid.GetWidth()==child/grid.GetWidth() ? int(child>parent) : 2+int(child>parent);
		RecordTransfer(parent,child,face,amount);
	}
	for (int i = 0; i < count; ++i) {
		auto tile = *grid.GetTile(i);
        tile.waterAmount = std::clamp(waterBefore_[i] + waterDelta_[i], 0.0f, 1.0f);
        (void)grid.SetTile(i, tile);
		waterBefore_[i] = grid.GetTile(i)->waterAmount;
	}
    if(intake_==SoilIntake::LocalContact) {
        changed |= ApplyLocalIntake(grid,dt);
    } else {
	// Soil demands share a canal's finite stock proportionally, independently of tile iteration order.
	for (int i = 0; i < count; ++i) {
		const auto& tile = *grid.GetTile(i);
		if (tile.feature != FarmTileFeature::None || tile.state == FarmTileState::Empty ||
			!std::isfinite(tile.moisture) || tile.moisture < 0.0f || tile.moisture > 1.0f) { continue; }
		const int canal = FindWetCanal(grid, i);
		if (canal < 0) { continue; }
		const float strength = IsSupplied(canal) ? GetSupplyStrength(canal) : canalTransferEfficiency_;
		const float capacity = (std::max)(0.0f, FarmCanalSeepage::kStopsAt - tile.moisture);
		const float demand = static_cast<float>((std::min)(static_cast<double>(capacity),
			recoveryPerSecond_ * strength * FarmCanalSeepage::Fraction(tile.moisture) * dt));
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
		if (actual > 0.f) lastTileFlows_[i].soilDonor = canal;
		lastTileFlows_[canal].soilSent += actual;
		lastStep_.soilDelivered += actual;
	}
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
	if (measurable) ++measurementRevision_;
	return changed;
}

bool FarmIrrigationSystem::ApplyLocalIntake(FarmGrid& grid,double dt)
{
    // Geometry and post-transfer stocks are frozen for both passes. No edge table or second reservoir.
    const auto contacts=[&](int i,auto visit) {
        const auto& soil=*grid.GetTile(i);
        for(size_t d=0;d<kNeighborX.size();++d) {
            const int x=i%grid.GetWidth()+kNeighborX[d], y=i/grid.GetWidth()+kNeighborY[d];
            if(x<0 || y<0 || x>=grid.GetWidth() || y>=grid.GetHeight()) continue;
            const int n=y*grid.GetWidth()+x;
            const auto& canal=*grid.GetTile(n);
            if(canal.feature==FarmTileFeature::Canal && canal.heightLevel>=soil.heightLevel && waterBefore_[n]>0)
                visit(n,waterBefore_[n]);
        }
    };
    for(int i=0;i<grid.GetTileCount();++i) {
        const auto& soil=*grid.GetTile(i);
        if(!soil.irrigationEnabled || soil.feature!=FarmTileFeature::None || soil.state==FarmTileState::Empty) continue;
        double depthSum=0;
        contacts(i,[&](int,float depth){depthSum+=depth;});
        if(depthSum<=0) continue;
        const double headroom=(std::max)(0.f,FarmCanalSeepage::kStopsAt-soil.moisture);
        const double intake=recoveryPerSecond_*dt*FarmCanalSeepage::Fraction(soil.moisture)*(std::min)(depthSum,1.0);
        soilDemand_[i]=static_cast<float>((std::min)(headroom,intake)/depthSum);
        contacts(i,[&](int n,float depth){canalDemand_[n]+=soilDemand_[i]*depth;});
    }
    bool changed=false;
    for(int i=0;i<grid.GetTileCount();++i) {
        if(soilDemand_[i]<=0) continue;
        double amount=0; int donor=-1;
        contacts(i,[&](int n,float depth) {
            const float scale=canalDemand_[n]>depth?depth/canalDemand_[n]:1.f;
            const float share=soilDemand_[i]*depth*scale;
            if(share<=0) return;
            amount+=share; lastTileFlows_[n].soilSent+=share;
            donor=donor==-1?n:-2;
        });
        auto tile=*grid.GetTile(i);
        const float before=tile.moisture;
        tile.moisture=static_cast<float>((std::min)(double(FarmCanalSeepage::kStopsAt),before+amount));
        (void)grid.SetTile(i,tile);
        auto& f=lastTileFlows_[i];
        f.soilReceived=tile.moisture-before;
        if(f.soilReceived>0) { f.soilDonor=donor; changed=true; }
        lastStep_.soilDelivered+=f.soilReceived;
    }
    for(size_t i=0;i<canalDemand_.size();++i) canalDemand_[i]=lastTileFlows_[i].soilSent;
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
