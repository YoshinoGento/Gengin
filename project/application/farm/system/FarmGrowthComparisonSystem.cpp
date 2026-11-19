#include "farm/system/FarmGrowthComparisonSystem.h"
#include "farm/core/FarmGrid.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kInitialGrowthTolerance = 0.0001f;
bool Valid(const farm::FarmTile& tile) noexcept {
	return std::isfinite(tile.moisture) && tile.moisture >= 0.0f && tile.moisture <= 1.0f &&
		std::isfinite(tile.growth) && tile.growth >= 0.0f && tile.growth <= 1.0f &&
		std::isfinite(tile.waterAmount) && tile.waterAmount == 0.0f && tile.careHistory.IsValid() &&
		std::isfinite(tile.soilNutrients) && tile.soilNutrients >= 0.0f && tile.soilNutrients <= 1.0f;
}
bool Same(const farm::FarmTile& a, const farm::FarmTile& b) noexcept {
	return a.state == b.state && a.crop == b.crop && a.feature == b.feature &&
		a.heightLevel == b.heightLevel && a.moisture == b.moisture && a.irrigationEnabled == b.irrigationEnabled &&
		a.growth == b.growth && a.waterAmount == b.waterAmount && a.soilNutrients == b.soilNutrients && a.mold == b.mold &&
		a.careHistory.nutrientGrowth == b.careHistory.nutrientGrowth &&
		a.careHistory.nutrientSupply == b.careHistory.nutrientSupply;
}
}

void FarmGrowthComparisonSystem::Initialize(const farm::FarmRules& rules) noexcept {
	Reset();
	maxDeltaTime_ = std::isfinite(rules.maxUpdateDeltaTime) && rules.maxUpdateDeltaTime > 0.0f
		? rules.maxUpdateDeltaTime : farm::FarmRules{}.maxUpdateDeltaTime;
}

void FarmGrowthComparisonSystem::Reset() noexcept {
	view_ = {};
	gridIdentity_ = nullptr;
	generation_ = 0;
}

bool FarmGrowthComparisonSystem::MatchesGrid(const farm::FarmGrid& grid) const noexcept {
	return gridIdentity_ == &grid && generation_ == grid.GetGeneration();
}

bool FarmGrowthComparisonSystem::Pin(const farm::FarmGrid& grid, int slot, int tileIndex) noexcept {
	if (slot < 0 || slot >= 2 || !grid.GetTile(tileIndex) || view_.status == FarmComparisonStatus::Running) return false;
	if (!MatchesGrid(grid)) Reset();
	gridIdentity_ = &grid;
	generation_ = grid.GetGeneration();
	view_.rows[static_cast<std::size_t>(slot)].tileIndex = tileIndex;
	view_.status = FarmComparisonStatus::Idle;
	view_.elapsedSeconds = 0.0;
	return true;
}

FarmComparisonIssue FarmGrowthComparisonSystem::CheckStart(const farm::FarmGrid& grid) const noexcept {
	if (!MatchesGrid(grid)) return FarmComparisonIssue::PickTwo;
	const auto* a = grid.GetTile(view_.rows[0].tileIndex);
	const auto* b = grid.GetTile(view_.rows[1].tileIndex);
	if (!a || !b) return FarmComparisonIssue::PickTwo;
	if (view_.rows[0].tileIndex == view_.rows[1].tileIndex) return FarmComparisonIssue::SameTile;
	if (!Valid(*a) || !Valid(*b)) return FarmComparisonIssue::InvalidData;
	for (const auto* tile : {a, b}) {
		if (tile->feature != farm::FarmTileFeature::None || tile->state != farm::FarmTileState::Planted ||
			!farm::IsPlantableCrop(tile->crop) || farm::IsHarvestReady(*tile)) return FarmComparisonIssue::NotGrowing;
	}
	if (a->crop != b->crop) return FarmComparisonIssue::Crop;
	if (a->heightLevel != b->heightLevel) return FarmComparisonIssue::Height;
	if (std::abs(a->growth - b->growth) > kInitialGrowthTolerance) return FarmComparisonIssue::Growth;
	return FarmComparisonIssue::None;
}

bool FarmGrowthComparisonSystem::Start(const farm::FarmGrid& grid) noexcept {
	if (view_.status == FarmComparisonStatus::Running || CheckStart(grid) != FarmComparisonIssue::None) return false;
	for (auto& row : view_.rows) {
		row.initial = row.current = *grid.GetTile(row.tileIndex);
		row.readySeconds = -1.0;
	}
	view_.elapsedSeconds = 0.0;
	view_.status = FarmComparisonStatus::Running;
	return true;
}

bool FarmGrowthComparisonSystem::Stop() noexcept {
	if (view_.status != FarmComparisonStatus::Running) return false;
	view_.status = FarmComparisonStatus::Stopped;
	return true;
}

void FarmGrowthComparisonSystem::ObserveBeforeStep(const farm::FarmGrid& grid) noexcept {
	if (view_.status != FarmComparisonStatus::Running) return;
	if (!MatchesGrid(grid)) { view_.status = FarmComparisonStatus::Invalidated; return; }
	for (const auto& row : view_.rows) {
		const auto* tile = grid.GetTile(row.tileIndex);
		if (!tile || !Same(*tile, row.current)) { view_.status = FarmComparisonStatus::Invalidated; return; }
	}
}

void FarmGrowthComparisonSystem::ObserveAfterStep(const farm::FarmGrid& grid, float deltaTime, float timeScale) noexcept {
	if (view_.status != FarmComparisonStatus::Running) return;
	if (!MatchesGrid(grid)) { view_.status = FarmComparisonStatus::Invalidated; return; }
	if (!std::isfinite(deltaTime) || !std::isfinite(timeScale) || deltaTime <= 0.0f || timeScale <= 0.0f) return;
	const float scaledDelta = (std::min)(deltaTime, maxDeltaTime_) * timeScale;
	if (!std::isfinite(scaledDelta)) return;
	for (const auto& row : view_.rows) {
		const auto* tile = grid.GetTile(row.tileIndex);
		if (!tile || !Valid(*tile) || tile->crop != row.initial.crop || tile->state != row.initial.state ||
			tile->heightLevel != row.initial.heightLevel || tile->feature != row.initial.feature ||
			tile->irrigationEnabled != row.initial.irrigationEnabled ||
			tile->growth < row.current.growth) { view_.status = FarmComparisonStatus::Invalidated; return; }
	}
	view_.elapsedSeconds += static_cast<double>(scaledDelta);
	bool allReady = true;
	for (auto& row : view_.rows) {
		row.current = *grid.GetTile(row.tileIndex);
		if (farm::IsHarvestReady(row.current)) {
			if (row.readySeconds < 0.0) row.readySeconds = view_.elapsedSeconds;
		} else { allReady = false; }
	}
	if (allReady) view_.status = FarmComparisonStatus::Completed;
}

FarmComparisonView FarmGrowthComparisonSystem::GetView(const farm::FarmGrid& grid) const noexcept {
	FarmComparisonView output = view_;
	output.startIssue = CheckStart(grid);
	if (!MatchesGrid(grid)) {
		if (output.status == FarmComparisonStatus::Idle) output.rows = {};
		else output.status = FarmComparisonStatus::Invalidated;
	} else if (output.status == FarmComparisonStatus::Idle) {
		for (auto& row : output.rows) {
			if (const auto* tile = grid.GetTile(row.tileIndex)) row.initial = row.current = *tile;
			row.readySeconds = -1.0;
		}
	}
	return output;
}
