#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmToolBehavior.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmSoilSystem.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmEconomySystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <utility>

namespace {
constexpr float kFullMoisture = 1.0f;
constexpr float kMinimumWateringIncrement = 0.01f;

bool TilesEqual(const farm::FarmTile& left, const farm::FarmTile& right)
{
	return left.heightLevel == right.heightLevel &&
		left.feature == right.feature &&
		left.irrigationEnabled == right.irrigationEnabled &&
		left.state == right.state &&
		left.crop == right.crop &&
		left.moisture == right.moisture &&
		left.growth == right.growth && left.waterAmount == right.waterAmount &&
		left.careHistory.drySeconds == right.careHistory.drySeconds &&
		left.careHistory.lowSeconds == right.careHistory.lowSeconds &&
		left.careHistory.goodSeconds == right.careHistory.goodSeconds &&
		left.careHistory.excessSeconds == right.careHistory.excessSeconds &&
		left.careHistory.efficiencySeconds == right.careHistory.efficiencySeconds &&
		left.careHistory.nutrientGrowth == right.careHistory.nutrientGrowth &&
		left.careHistory.nutrientSupply == right.careHistory.nutrientSupply &&
		left.soilNutrients == right.soilNutrients;
}

// Restore only the intake setting; simulation may have changed water/growth since the edit.
class FarmIntakeCommand final : public IUndoableCommand {
public:
	FarmIntakeCommand(farm::FarmGrid& grid, int index, bool before, bool after)
		: grid_(&grid), generation_(grid.GetGeneration()), index_(index), before_(before), after_(after) {}
	bool Execute() override { return Apply(before_, after_); }
	bool Undo() override { return Apply(after_, before_); }
	std::string_view GetName() const noexcept override { return after_ ? "Open Soil Intake" : "Close Soil Intake"; }
private:
	bool Apply(bool expected, bool value) {
		if (!grid_ || grid_->GetGeneration() != generation_) return false;
		auto* tile = grid_->GetTile(index_);
		if (!tile || tile->feature != farm::FarmTileFeature::None || tile->irrigationEnabled != expected) return false;
		auto candidate = *tile;
        candidate.irrigationEnabled = value;
        return grid_->SetTile(index_, candidate);
	}
	// History is owned by the scene and cleared before its grid is destroyed.
	farm::FarmGrid* grid_ = nullptr;
	uint64_t generation_ = 0;
	int index_ = -1;
	bool before_ = true, after_ = true;
};

class FarmTileEditCommand final : public IUndoableCommand {
public:
	FarmTileEditCommand(
		farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
		const farm::FarmTile& after, const char* name,
		FarmEconomySystem* economySystem,
		const FarmCropQualityResult& harvestedQuality,
		int harvestedQuantity, farm::CropType plantedCrop, int plantedQuantity, int harvestedDay)
		: grid_(&grid), gridGeneration_(grid.GetGeneration()), tileIndex_(tileIndex),
		before_(before), after_(after), name_(name ? name : "Farm Tile Edit"),
		economySystem_(economySystem), harvestedQuality_(harvestedQuality),
		harvestedQuantity_(harvestedQuantity), plantedCrop_(plantedCrop),
		plantedQuantity_(plantedQuantity), harvestedDay_(harvestedDay),
		previousLastHarvestQuality_(economySystem != nullptr
			? economySystem->GetLastHarvestQuality() : FarmCropQualityResult{}) {}

	bool Execute() override { return Apply(after_, true); }
	bool Undo() override { return Apply(before_, false); }
	std::string_view GetName() const noexcept override { return name_; }

private:
	bool Apply(const farm::FarmTile& tile, bool addHarvest) {
		if (grid_ == nullptr || grid_->GetGeneration() != gridGeneration_) {
			return false;
		}

		const bool updatesHarvest = economySystem_ != nullptr &&
			harvestedQuality_.IsValid() && harvestedQuantity_ > 0;
		const bool updatesSeed = economySystem_ != nullptr &&
			plantedCrop_ != farm::CropType::None && plantedQuantity_ > 0;
		if (updatesHarvest) {
			const bool harvestChanged = addHarvest
				? economySystem_->AddHarvest(harvestedQuality_, harvestedQuantity_, harvestedDay_)
				: economySystem_->RemoveHarvest(
					harvestedQuality_, harvestedQuantity_, previousLastHarvestQuality_);
			if (!harvestChanged) {
				return false;
			}
		}
		if (updatesSeed) {
			const bool seedChanged = addHarvest
				? economySystem_->RemoveSeed(plantedCrop_, plantedQuantity_)
				: economySystem_->AddSeed(plantedCrop_, plantedQuantity_);
			if (!seedChanged) {
				if (updatesHarvest) {
					if (addHarvest) {
						static_cast<void>(economySystem_->RemoveHarvest(
							harvestedQuality_, harvestedQuantity_,
							previousLastHarvestQuality_));
					} else {
						static_cast<void>(economySystem_->AddHarvest(
							harvestedQuality_, harvestedQuantity_, harvestedDay_));
					}
				}
				return false;
			}
		}

		if (grid_->SetTile(tileIndex_, tile)) {
			return true;
		}

		if (updatesSeed) {
			if (addHarvest) {
				static_cast<void>(economySystem_->AddSeed(plantedCrop_, plantedQuantity_));
			} else {
				static_cast<void>(economySystem_->RemoveSeed(plantedCrop_, plantedQuantity_));
			}
		}
		if (updatesHarvest) {
			if (addHarvest) {
				static_cast<void>(economySystem_->RemoveHarvest(
					harvestedQuality_, harvestedQuantity_,
					previousLastHarvestQuality_));
			} else {
				static_cast<void>(economySystem_->AddHarvest(
					harvestedQuality_, harvestedQuantity_, harvestedDay_));
			}
		}
		return false;
	}

	// FarmToolActionSystem history is destroyed before the scene-owned FarmGrid.
	farm::FarmGrid* grid_ = nullptr;
	uint64_t gridGeneration_ = 0;
	int tileIndex_ = -1;
	farm::FarmTile before_{};
	farm::FarmTile after_{};
	std::string_view name_;
	FarmEconomySystem* economySystem_ = nullptr;
	FarmCropQualityResult harvestedQuality_{};
	int harvestedQuantity_ = 0;
	farm::CropType plantedCrop_ = farm::CropType::None;
	int plantedQuantity_ = 0;
	int harvestedDay_ = 0; // Redo retains the original harvest date, not the current clock.
	FarmCropQualityResult previousLastHarvestQuality_{};
};

struct FarmTileBatchEntry {
	int tileIndex = -1;
	farm::FarmTile before{};
	farm::FarmTile after{};
};

class FarmTileBatchEditCommand final : public IUndoableCommand {
public:
	FarmTileBatchEditCommand(
		farm::FarmGrid& grid,
		std::vector<FarmTileBatchEntry> entries,
		const char* name)
		: grid_(&grid), gridGeneration_(grid.GetGeneration()),
		entries_(std::move(entries)), name_(name ? name : "Farm Tile Batch Edit") {}

	bool Execute() override { return Apply(true); }
	bool Undo() override { return Apply(false); }
	std::string_view GetName() const noexcept override { return name_; }

private:
	bool Apply(bool useAfter) {
		if (grid_ == nullptr || grid_->GetGeneration() != gridGeneration_ || entries_.empty()) {
			return false;
		}

		std::size_t appliedCount = 0;
		for (; appliedCount < entries_.size(); ++appliedCount) {
			const FarmTileBatchEntry& entry = entries_[appliedCount];
			const farm::FarmTile& target = useAfter ? entry.after : entry.before;
			if (!grid_->SetTile(entry.tileIndex, target)) {
				break;
			}
		}
		if (appliedCount == entries_.size()) {
			return true;
		}

		while (appliedCount > 0) {
			--appliedCount;
			const FarmTileBatchEntry& entry = entries_[appliedCount];
			const farm::FarmTile& rollback = useAfter ? entry.before : entry.after;
			static_cast<void>(grid_->SetTile(entry.tileIndex, rollback));
		}
		return false;
	}

	// The scene-owned Grid outlives FarmToolActionSystem and its command history.
	farm::FarmGrid* grid_ = nullptr;
	uint64_t gridGeneration_ = 0;
	std::vector<FarmTileBatchEntry> entries_;
	std::string_view name_;
};
}

void FarmToolActionSystem::Initialize(const farm::FarmRules& rules) noexcept
{
	cropQualitySystem_.Initialize(rules);
	wateringMoistureIncrement_ = std::isfinite(rules.wateringMoistureIncrement) &&
		rules.wateringMoistureIncrement >= kMinimumWateringIncrement &&
		rules.wateringMoistureIncrement <= kFullMoisture
		? rules.wateringMoistureIncrement : farm::FarmRules{}.wateringMoistureIncrement;
	history_.Clear();
}

bool FarmToolActionSystem::CommitHarvestProtection(FarmEconomySystem& economy,
	int recordId, bool protect, uint64_t inventoryGeneration) {
	if (!economy.SetHarvestProtection(recordId, protect, inventoryGeneration)) return false;
	// Earlier tile commands must not remove a harvest committed to retention.
	ClearHistory();
	return true;
}

bool FarmToolActionSystem::CommitContestReservation(FarmEconomySystem& economy,
	int recordId, bool reserve, uint64_t inventoryGeneration) {
	if (!economy.SetContestReservation(recordId, reserve, inventoryGeneration)) return false;
	ClearHistory();
	return true;
}

bool FarmToolActionSystem::CommitContestSubmission(FarmEconomySystem& economy,
	int day, int recordId, uint64_t inventoryGeneration) {
	if (!FarmContestSubmissionSystem::Submit(economy, day, recordId, inventoryGeneration)) return false;
	ClearHistory();
	return true;
}

FarmCropQualityResult FarmToolActionSystem::EvaluateHarvestQuality(
	const farm::FarmTile& tile) const noexcept
{
	return cropQualitySystem_.Evaluate(tile);
}

bool FarmToolActionSystem::ApplyTool(
	farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
	FarmEconomySystem& economySystem)
{
	return ApplyToolDetailed(grid, tool, selectedCrop, economySystem).Succeeded();
}

FarmToolActionResult FarmToolActionSystem::EvaluateTool(
	const farm::FarmGrid& grid, FarmTool tool,
	farm::CropType selectedCrop,
	const FarmEconomySystem* economySystem) const noexcept
{
	return EvaluateTool(grid, grid.GetSelectedIndex(), tool, selectedCrop, economySystem);
}

FarmToolActionResult FarmToolActionSystem::EvaluateTool(
    const farm::FarmGrid& grid, int tileIndex, FarmTool tool,
    farm::CropType selectedCrop, const FarmEconomySystem* economySystem) const noexcept
{
    return PlanTool(grid, tileIndex, tool, selectedCrop, economySystem).result;
}

FarmToolPlan FarmToolActionSystem::PlanTool(
    const farm::FarmGrid& grid, int tileIndex, FarmTool tool,
    farm::CropType selectedCrop, const FarmEconomySystem* economy) const noexcept
{
    FarmToolPlan plan;
    plan.result.tool = tool;
    plan.result.tileIndex = tileIndex;
    const auto* tile = grid.GetTile(tileIndex);
    if (!tile) {
        plan.result.status = FarmToolActionStatus::InvalidTarget;
    } else if (tile->feature != farm::FarmTileFeature::None) {
        plan.result.status = FarmToolActionStatus::InvalidState;
    } else if (const FarmToolBehavior* behavior = FindFarmToolBehavior(tool)) {
        plan = behavior->Plan({*tile, selectedCrop, economy, stageAccess_,
            cropQualitySystem_, wateringMoistureIncrement_});
        plan.result.tileIndex = tileIndex;
    } else {
        plan.result.status = FarmToolActionStatus::UnsupportedTool;
    }
    return plan;
}

FarmToolActionResult FarmToolActionSystem::ApplyToolDetailed(
    farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
    FarmEconomySystem& economySystem, int harvestedDay)
{
    const FarmToolPlan plan = PlanTool(grid, grid.GetSelectedIndex(), tool, selectedCrop, &economySystem);
    FarmToolActionResult result = plan.result;
    if (!result.Succeeded()) return result;

    const auto* selectedTile = grid.GetTile(result.tileIndex);
    if (!selectedTile) {
        result.status = FarmToolActionStatus::InvalidTarget;
        return result;
    }
    const farm::FarmTile before = *selectedTile;
    if (!CommitTileChange(grid, result.tileIndex, before, plan.after, plan.commandName,
        &economySystem, result.harvestQuality, plan.harvestedQuantity,
        plan.plantedCrop, plan.plantedQuantity, harvestedDay)) {
        result.status = FarmToolActionStatus::InvalidState;
        return result;
    }
    if (result.status == FarmToolActionStatus::Harvested) result.harvestedTile = before;
    return result;
}

bool FarmToolActionSystem::CompostSelectedTile(farm::FarmGrid& grid)
{
	const auto* tile = grid.GetSelectedTile();
	if (!tile) return false;
	auto after = *tile;
	if (!FarmSoilSystem::ApplyCompost(after)) return false;
	return CommitTileChange(grid, grid.GetSelectedIndex(), *tile, after, "Apply Compost");
}

bool FarmToolActionSystem::RaiseSelectedTile(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr || selectedTile->heightLevel >= MaximumAllowedHeight()) {
		return false;
	}
	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	after.heightLevel = std::clamp(
		after.heightLevel + 1,
		FarmToolActionSystem::kMinimumHeightLevel,
		FarmToolActionSystem::kMaximumHeightLevel);
	return CommitTileChange(grid, tileIndex, before, after, "Raise Tile");
}

bool FarmToolActionSystem::LowerSelectedTile(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr) {
		return false;
	}
	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	after.heightLevel = std::clamp(
		after.heightLevel - 1,
		FarmToolActionSystem::kMinimumHeightLevel,
		FarmToolActionSystem::kMaximumHeightLevel);
	return CommitTileChange(grid, tileIndex, before, after, "Lower Tile");
}

bool FarmToolActionSystem::CanSetIrrigation(const farm::FarmGrid& grid, int tileIndex) const noexcept
{
	const auto* tile = grid.GetTile(tileIndex);
	return tile && tile->feature == farm::FarmTileFeature::None;
}

bool FarmToolActionSystem::SetSelectedIrrigation(farm::FarmGrid& grid, bool enabled)
{
	const int index = grid.GetSelectedIndex();
	if (!CanSetIrrigation(grid, index)) return false;
	const bool before = grid.GetTile(index)->irrigationEnabled;
	if (before == enabled) return false;
	SynchronizeHistoryAccess();
	return history_.Execute(std::make_unique<FarmIntakeCommand>(grid, index, before, enabled));
}

bool FarmToolActionSystem::ToggleSelectedCanal(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr || !CanToggleCanal(grid, tileIndex)) {
		return false;
	}

	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	const char* commandName = "Remove Canal";
	if (before.feature == farm::FarmTileFeature::Canal) {
		after.feature = farm::FarmTileFeature::None;
		after.waterAmount = 0.0f;
	} else {
		after.feature = farm::FarmTileFeature::Canal;
		commandName = "Place Canal";
	}
	return CommitTileChange(grid, tileIndex, before, after, commandName);
}

bool FarmToolActionSystem::PlaceCanalPath(
	farm::FarmGrid& grid,
	const std::vector<int>& tileIndices)
{
	return CommitCanalPath(grid, tileIndices, false);
}

bool FarmToolActionSystem::RemoveCanalPath(
	farm::FarmGrid& grid, const std::vector<int>& tileIndices)
{
	return CommitCanalPath(grid, tileIndices, true);
}

bool FarmToolActionSystem::CommitCanalPath(
	farm::FarmGrid& grid, const std::vector<int>& tileIndices, bool remove)
{
	if (tileIndices.empty() || grid.GetWidth() <= 0 ||
		tileIndices.size() > static_cast<std::size_t>(grid.GetTileCount())) {
		return false;
	}

	std::vector<FarmTileBatchEntry> entries;
	entries.reserve(tileIndices.size());
	for (std::size_t pathIndex = 0; pathIndex < tileIndices.size(); ++pathIndex) {
		const int tileIndex = tileIndices[pathIndex];
		const farm::FarmTile* tile = grid.GetTile(tileIndex);
		const auto requiredFeature = remove
			? farm::FarmTileFeature::Canal : farm::FarmTileFeature::None;
		if (tile == nullptr || tile->feature != requiredFeature ||
			tile->state != farm::FarmTileState::Empty || tile->crop != farm::CropType::None ||
			tile->moisture != 0.0f || tile->growth != 0.0f) {
			return false;
		}
		if (std::find(tileIndices.begin(), tileIndices.begin() + pathIndex, tileIndex) !=
			tileIndices.begin() + pathIndex) {
			return false;
		}
		if (pathIndex > 0) {
			const int previousIndex = tileIndices[pathIndex - 1];
			const int columnDelta = std::abs(
				tileIndex % grid.GetWidth() - previousIndex % grid.GetWidth());
			const int rowDelta = std::abs(
				tileIndex / grid.GetWidth() - previousIndex / grid.GetWidth());
			if (columnDelta + rowDelta != 1) {
				return false;
			}
		}

		FarmTileBatchEntry entry;
		entry.tileIndex = tileIndex;
		entry.before = *tile;
		entry.after = *tile;
		entry.after.feature = remove ? farm::FarmTileFeature::None : farm::FarmTileFeature::Canal;
		entry.after.waterAmount = 0.0f;
		entries.push_back(entry);
	}

	SynchronizeHistoryAccess();
	return history_.Execute(std::make_unique<FarmTileBatchEditCommand>(
		grid, std::move(entries), remove ? "Remove Canal Path" : "Place Canal Path"));
}

bool FarmToolActionSystem::ChangeTerrainBatch(
	farm::FarmGrid& grid, const std::vector<int>& tileIndices, int delta)
{
	if ((delta != 1 && delta != -1) || tileIndices.empty() ||
		tileIndices.size() > static_cast<std::size_t>(grid.GetTileCount())) return false;
	std::vector<FarmTileBatchEntry> entries;
	entries.reserve(tileIndices.size());
	for (std::size_t i = 0; i < tileIndices.size(); ++i) {
		const int index = tileIndices[i];
		const auto* tile = grid.GetTile(index);
		if (!tile || tile->heightLevel < kMinimumHeightLevel || tile->heightLevel > kMaximumHeightLevel ||
			(delta > 0 && tile->heightLevel >= MaximumAllowedHeight()) ||
			(delta < 0 && tile->heightLevel == kMinimumHeightLevel) ||
			std::find(tileIndices.begin(), tileIndices.begin() + i, index) != tileIndices.begin() + i) return false;
		FarmTileBatchEntry entry{index, *tile, *tile};
		entry.after.heightLevel += delta;
		entries.push_back(entry);
	}
	SynchronizeHistoryAccess();
	return history_.Execute(std::make_unique<FarmTileBatchEditCommand>(
		grid, std::move(entries), delta > 0 ? "Raise Terrain Batch" : "Lower Terrain Batch"));
}

bool FarmToolActionSystem::CanToggleCanal(
	const farm::FarmGrid& grid, int tileIndex) const noexcept
{
	const farm::FarmTile* tile = grid.GetTile(tileIndex);
	if (tile == nullptr || !farm::IsValidFarmTileFeature(tile->feature)) {
		return false;
	}
	if (tile->feature == farm::FarmTileFeature::Canal) {
		return true;
	}
	if (tile->feature != farm::FarmTileFeature::None) {
		return false;
	}
	return tile->state == farm::FarmTileState::Empty &&
		tile->crop == farm::CropType::None &&
		tile->moisture == 0.0f && tile->growth == 0.0f;
}

bool FarmToolActionSystem::ToggleSelectedWaterSource(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr || !CanToggleWaterSource(grid, tileIndex)) {
		return false;
	}

	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	const char* commandName = "Remove Water Source";
	if (before.feature == farm::FarmTileFeature::WaterSource) {
		after.feature = farm::FarmTileFeature::None;
		after.waterAmount = 0.0f;
	} else {
		after.feature = farm::FarmTileFeature::WaterSource;
		commandName = "Place Water Source";
	}
	return CommitTileChange(grid, tileIndex, before, after, commandName);
}

bool FarmToolActionSystem::CanToggleWaterSource(
	const farm::FarmGrid& grid, int tileIndex) const noexcept
{
	const farm::FarmTile* tile = grid.GetTile(tileIndex);
	if (tile == nullptr || !farm::IsValidFarmTileFeature(tile->feature)) {
		return false;
	}
	if (tile->feature == farm::FarmTileFeature::WaterSource) {
		return true;
	}
	if (tile->feature != farm::FarmTileFeature::None) {
		return false;
	}
	return tile->state == farm::FarmTileState::Empty &&
		tile->crop == farm::CropType::None &&
		tile->moisture == 0.0f && tile->growth == 0.0f;
}

bool FarmToolActionSystem::CommitTileChange(
	farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
	const farm::FarmTile& after, const char* commandName,
	FarmEconomySystem* economySystem,
	const FarmCropQualityResult& harvestedQuality,
	int harvestedQuantity, farm::CropType plantedCrop, int plantedQuantity, int harvestedDay)
{
	if (TilesEqual(before, after)) {
		return false;
	}
	SynchronizeHistoryAccess();
	return history_.Execute(std::make_unique<FarmTileEditCommand>(
		grid, tileIndex, before, after, commandName,
		economySystem, harvestedQuality, harvestedQuantity,
		plantedCrop, plantedQuantity, harvestedDay));
}
