#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/system/FarmStageAccessSystem.h"
#include "command/CommandHistory.h"

#include <vector>
#include <cstdint>
#include "farm/system/FarmToolActionTypes.h"

struct FarmToolPlan;

namespace farm {
class FarmGrid;
struct FarmTile;
}

class FarmEconomySystem;

class FarmToolActionSystem {
public:
	static constexpr int kMinimumHeightLevel = 0;
	static constexpr int kMaximumHeightLevel = 2;

	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	void BindStageAccess(const FarmStageAccessSystem& access) noexcept { stageAccess_=&access; }
	[[nodiscard]] int MaximumAllowedHeight() const noexcept {
		return stageAccess_ ? stageAccess_->Get().maximumHeight : kMaximumHeightLevel;
	}
	bool CommitHarvestProtection(FarmEconomySystem& economy, int recordId,
		bool protect, uint64_t inventoryGeneration);
	bool CommitContestReservation(FarmEconomySystem& economy, int recordId,
		bool reserve, uint64_t inventoryGeneration);
	bool CommitContestSubmission(FarmEconomySystem& economy, int day, int recordId, uint64_t inventoryGeneration);
	[[nodiscard]] FarmCropQualityResult EvaluateHarvestQuality(
		const farm::FarmTile& tile) const noexcept;

	[[nodiscard]] FarmToolActionResult EvaluateTool(
		const farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
		const FarmEconomySystem* economySystem = nullptr) const noexcept;
	[[nodiscard]] FarmToolActionResult EvaluateTool(
		const farm::FarmGrid& grid, int tileIndex, FarmTool tool,
		farm::CropType selectedCrop,
		const FarmEconomySystem* economySystem = nullptr) const noexcept;
	bool ApplyTool(
		farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
		FarmEconomySystem& economySystem);
	[[nodiscard]] FarmToolActionResult ApplyToolDetailed(
		farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
		FarmEconomySystem& economySystem, int harvestedDay = 0);
	bool RaiseSelectedTile(farm::FarmGrid& grid);
	bool CompostSelectedTile(farm::FarmGrid& grid);
	bool LowerSelectedTile(farm::FarmGrid& grid);
	bool ChangeTerrainBatch(farm::FarmGrid& grid, const std::vector<int>& tileIndices, int delta);
	[[nodiscard]] bool CanToggleCanal(
		const farm::FarmGrid& grid, int tileIndex) const noexcept;
	bool ToggleSelectedCanal(farm::FarmGrid& grid);
	[[nodiscard]] bool CanSetIrrigation(const farm::FarmGrid& grid, int tileIndex) const noexcept;
	bool SetSelectedIrrigation(farm::FarmGrid& grid, bool enabled);
	bool PlaceCanalPath(farm::FarmGrid& grid, const std::vector<int>& tileIndices);
	bool RemoveCanalPath(farm::FarmGrid& grid, const std::vector<int>& tileIndices);
	[[nodiscard]] bool CanToggleWaterSource(
		const farm::FarmGrid& grid, int tileIndex) const noexcept;
	bool ToggleSelectedWaterSource(farm::FarmGrid& grid);
	bool Undo() { SynchronizeHistoryAccess(); return history_.Undo(); }
	bool Redo() { SynchronizeHistoryAccess(); return history_.Redo(); }
	void ClearHistory() noexcept { history_.Clear(); historyAccessKey_=AccessKey(); }
	[[nodiscard]] const CommandHistory& GetHistory() const noexcept { return history_; }

private:
	[[nodiscard]] FarmToolPlan PlanTool(const farm::FarmGrid& grid, int tileIndex, FarmTool tool,
		farm::CropType selectedCrop, const FarmEconomySystem* economy) const noexcept;
	const FarmStageAccessSystem* stageAccess_=nullptr; // Borrowed; stage access reads live date/policy.
	int historyAccessKey_ = 0;
	int AccessKey() const noexcept {
		const auto access = stageAccess_ ? stageAccess_->Get() : FarmStageAccess{};
		return access.restricted ? 1 + access.stage : 0;
	}
	void SynchronizeHistoryAccess() noexcept { if (historyAccessKey_ != AccessKey()) ClearHistory(); }
	bool CommitCanalPath(farm::FarmGrid& grid, const std::vector<int>& tileIndices, bool remove);
	bool CommitTileChange(
		farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
		const farm::FarmTile& after, const char* commandName,
		FarmEconomySystem* economySystem = nullptr,
		const FarmCropQualityResult& harvestedQuality = {},
		int harvestedQuantity = 0,
		farm::CropType plantedCrop = farm::CropType::None,
		int plantedQuantity = 0, int harvestedDay = 0);
	CommandHistory history_{ 128 };
	FarmCropQualitySystem cropQualitySystem_{};
	float wateringMoistureIncrement_ = farm::FarmRules{}.wateringMoistureIncrement;
};
