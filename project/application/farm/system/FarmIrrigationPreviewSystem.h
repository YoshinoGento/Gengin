#pragma once

#include "farm/core/FarmGrid.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmIrrigationForecast.h"
#include "farm/system/FarmStageAccessSystem.h"

#include <cstdint>
#include <vector>

namespace farm {

enum class FarmIrrigationPreviewOperation : std::uint8_t {
	None,
	ToggleCanal,
	ToggleWaterSource,
	RaiseTerrain,
	LowerTerrain,
	PlaceCanalPath,
	RemoveCanalPath,
};

enum class FarmCanalPathIssue : std::uint8_t {
	None,
	NonStraight,
	BlockedTile,
};

// Owns one temporary irrigation edit. Authoritative FarmGrid mutation stays elsewhere.
class FarmIrrigationPreviewSystem final {
public:
	void Initialize(const FarmRules& rules = {}) noexcept;
	void BindStageAccess(const FarmStageAccessSystem& access) noexcept { stageAccess_=&access; }
	[[nodiscard]] bool Begin(
		const FarmGrid& sourceGrid,
		int tileIndex,
		FarmIrrigationPreviewOperation operation);
	[[nodiscard]] bool AppendCanalPathTile(
		const FarmGrid& sourceGrid,
		int tileIndex);
	[[nodiscard]] bool VisitCanalPathTile(const FarmGrid& sourceGrid, int tileIndex);
	[[nodiscard]] bool VisitTerrainTile(const FarmGrid& sourceGrid, int tileIndex);
	void EndTerrainStroke() noexcept { lastTerrainTileIndex_ = -1; }
	[[nodiscard]] FarmCanalPathIssue GetPathIssue() const noexcept { return pathIssue_; }
	[[nodiscard]] int GetBlockedTileIndex() const noexcept { return blockedTileIndex_; }
	void Cancel() noexcept;

	[[nodiscard]] bool CanConfirm(const FarmGrid& sourceGrid) const noexcept;
	[[nodiscard]] bool IsActive() const noexcept { return active_; }
	[[nodiscard]] const FarmIrrigationForecast& Forecast() const noexcept { return forecast_; }
	[[nodiscard]] std::uint64_t CandidateRevision() const noexcept { return candidateRevision_; }
	[[nodiscard]] int GetTileIndex() const noexcept { return tileIndex_; }
	[[nodiscard]] FarmIrrigationPreviewOperation GetOperation() const noexcept {
		return operation_;
	}
	[[nodiscard]] FarmTileFeature GetOriginalFeature() const noexcept {
		return originalTile_.feature;
	}
	[[nodiscard]] FarmTileFeature GetCandidateFeature() const noexcept {
		return candidateFeature_;
	}
	[[nodiscard]] int GetOriginalHeightLevel() const noexcept {
		return originalTile_.heightLevel;
	}
	[[nodiscard]] int GetCandidateHeightLevel() const noexcept {
		return candidateHeightLevel_;
	}
	[[nodiscard]] bool IsTileChanged(int tileIndex) const noexcept;
	[[nodiscard]] std::size_t GetChangeCount() const noexcept {
		return changedTileIndices_.size();
	}
	[[nodiscard]] const std::vector<int>& GetChangedTileIndices() const noexcept {
		return changedTileIndices_;
	}
	[[nodiscard]] const FarmGrid* GetPreviewGrid() const noexcept {
		return active_ ? &previewGrid_ : nullptr;
	}
	[[nodiscard]] const FarmIrrigationSystem* GetPreviewIrrigation() const noexcept {
		return active_ ? &previewIrrigation_ : nullptr;
	}

private:
	void RebuildCandidate();
	FarmIrrigationForecast forecast_;
	std::uint64_t candidateRevision_ = 0;
	const FarmStageAccessSystem* stageAccess_ = nullptr;
	[[nodiscard]] int MaximumAllowedHeight() const noexcept {
		return stageAccess_ ? stageAccess_->Get().maximumHeight : kFarmStageRulesV1.back().maximumHeight;
	}
	[[nodiscard]] bool ExtendCanalPathTo(const FarmGrid& sourceGrid, int tileIndex);
	FarmRules rules_{};
	FarmGrid previewGrid_{};
	FarmIrrigationSystem previewIrrigation_{};
	FarmGrid::Snapshot sourceSnapshot_{};
	std::uint64_t sourceGeneration_ = 0;
	FarmTile originalTile_{};
	int tileIndex_ = -1;
	FarmIrrigationPreviewOperation operation_ = FarmIrrigationPreviewOperation::None;
	FarmTileFeature candidateFeature_ = FarmTileFeature::None;
	int candidateHeightLevel_ = 0;
	std::vector<int> changedTileIndices_;
	bool active_ = false;
	FarmCanalPathIssue pathIssue_ = FarmCanalPathIssue::None;
	int blockedTileIndex_ = -1;
	int lastTerrainTileIndex_ = -1;
};

} // namespace farm
