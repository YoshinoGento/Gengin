#include "farm/system/FarmIrrigationPreviewSystem.h"

#include "farm/system/FarmToolActionSystem.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdlib>

namespace farm {
namespace {
bool TilesEqual(const FarmTile& left, const FarmTile& right) noexcept
{
	return left.heightLevel == right.heightLevel &&
		left.feature == right.feature &&
		left.irrigationEnabled == right.irrigationEnabled &&
		left.repellentPlant == right.repellentPlant &&
		left.state == right.state &&
		left.crop == right.crop &&
		left.moisture == right.moisture &&
		left.growth == right.growth && left.waterAmount == right.waterAmount &&
		left.soilNutrients == right.soilNutrients &&
		left.careHistory.drySeconds == right.careHistory.drySeconds &&
		left.careHistory.lowSeconds == right.careHistory.lowSeconds &&
		left.careHistory.goodSeconds == right.careHistory.goodSeconds &&
		left.careHistory.excessSeconds == right.careHistory.excessSeconds &&
		left.careHistory.efficiencySeconds == right.careHistory.efficiencySeconds &&
		left.careHistory.nutrientGrowth == right.careHistory.nutrientGrowth &&
		left.careHistory.nutrientSupply == right.careHistory.nutrientSupply;
}

bool IsUnusedTile(const FarmTile& tile) noexcept
{
	return !tile.repellentPlant && tile.state == FarmTileState::Empty &&
		tile.crop == CropType::None &&
		std::isfinite(tile.moisture) && tile.moisture == 0.0f &&
		std::isfinite(tile.growth) && tile.growth == 0.0f;
}

bool TryBuildCandidateTile(
	const FarmTile& source,
	FarmIrrigationPreviewOperation operation,
	FarmTile& output, int maximumHeight) noexcept
{
	output = source;
	switch (operation) {
	case FarmIrrigationPreviewOperation::ToggleCanal:
		if (source.feature == FarmTileFeature::Canal) {
			output.feature = FarmTileFeature::None;
			output.waterAmount = 0.0f;
			return true;
		}
		if (source.feature == FarmTileFeature::None && IsUnusedTile(source)) {
			output.feature = FarmTileFeature::Canal;
			return true;
		}
		return false;
	case FarmIrrigationPreviewOperation::ToggleWaterSource:
		if (source.feature == FarmTileFeature::WaterSource) {
			output.feature = FarmTileFeature::None;
			output.waterAmount = 0.0f;
			return true;
		}
		if (source.feature == FarmTileFeature::None && IsUnusedTile(source)) {
			output.feature = FarmTileFeature::WaterSource;
			return true;
		}
		return false;
	case FarmIrrigationPreviewOperation::RaiseTerrain:
		if (source.heightLevel >= maximumHeight) {
			return false;
		}
		++output.heightLevel;
		return true;
	case FarmIrrigationPreviewOperation::LowerTerrain:
		if (source.heightLevel <= FarmToolActionSystem::kMinimumHeightLevel) {
			return false;
		}
		--output.heightLevel;
		return true;
	case FarmIrrigationPreviewOperation::PlaceCanalPath:
		if (source.feature == FarmTileFeature::None && IsUnusedTile(source)) {
			output.feature = FarmTileFeature::Canal;
			return true;
		}
		return false;
	case FarmIrrigationPreviewOperation::RemoveCanalPath:
		if (source.feature == FarmTileFeature::Canal && IsUnusedTile(source)) {
			output.feature = FarmTileFeature::None;
			output.waterAmount = 0.0f;
			return true;
		}
		return false;
	case FarmIrrigationPreviewOperation::None:
	default:
		return false;
	}
}
} // namespace

void FarmIrrigationPreviewSystem::Initialize(const FarmRules& rules) noexcept
{
	rules_ = rules;
	Cancel();
}

bool FarmIrrigationPreviewSystem::Begin(
	const FarmGrid& sourceGrid,
	int tileIndex,
	FarmIrrigationPreviewOperation operation)
{
	Cancel();
	const FarmTile* sourceTile = sourceGrid.GetTile(tileIndex);
	if (sourceTile == nullptr || !IsValidFarmTileFeature(sourceTile->feature)) {
		return false;
	}

	FarmTile candidateTile{};
	if (!TryBuildCandidateTile(*sourceTile, operation, candidateTile, MaximumAllowedHeight())) {
		return false;
	}

	previewGrid_ = sourceGrid;
	if (!previewGrid_.SetTile(tileIndex, candidateTile) ||
		!previewGrid_.SetSelectedIndex(tileIndex)) {
		Cancel();
		return false;
	}

	previewIrrigation_.Initialize(rules_);
	previewIrrigation_.Rebuild(previewGrid_);
	sourceGrid.CaptureSnapshot(sourceSnapshot_);
	sourceGeneration_ = sourceGrid.GetGeneration();
	originalTile_ = *sourceTile;
	tileIndex_ = tileIndex;
	operation_ = operation;
	candidateFeature_ = candidateTile.feature;
	candidateHeightLevel_ = candidateTile.heightLevel;
	changedTileIndices_.push_back(tileIndex);
	active_ = true;
	return true;
}

bool FarmIrrigationPreviewSystem::AppendCanalPathTile(
	const FarmGrid& sourceGrid,
	int tileIndex)
{
	if ((operation_ != FarmIrrigationPreviewOperation::PlaceCanalPath &&
		operation_ != FarmIrrigationPreviewOperation::RemoveCanalPath) ||
		!CanConfirm(sourceGrid) || changedTileIndices_.empty()) {
		return false;
	}
	if (IsTileChanged(tileIndex)) {
		return true;
	}
	if (changedTileIndices_.size() >= static_cast<std::size_t>(sourceGrid.GetTileCount())) {
		return false;
	}

	const int previousIndex = changedTileIndices_.back();
	const int width = sourceGrid.GetWidth();
	if (width <= 0 || sourceGrid.GetTile(tileIndex) == nullptr) {
		return false;
	}
	const int columnDelta = std::abs(tileIndex % width - previousIndex % width);
	const int rowDelta = std::abs(tileIndex / width - previousIndex / width);
	if (columnDelta + rowDelta != 1) {
		return false;
	}

	const FarmTile* sourceTile = sourceGrid.GetTile(tileIndex);
	FarmTile candidateTile{};
	if (sourceTile == nullptr || !TryBuildCandidateTile(
		*sourceTile, operation_, candidateTile, MaximumAllowedHeight())) {
		return false;
	}
	if (!previewGrid_.SetTile(tileIndex, candidateTile) ||
		!previewGrid_.SetSelectedIndex(tileIndex)) {
		return false;
	}

	changedTileIndices_.push_back(tileIndex);
	previewIrrigation_.Rebuild(previewGrid_);
	return true;
}

bool FarmIrrigationPreviewSystem::VisitCanalPathTile(
	const FarmGrid& sourceGrid, int tileIndex)
{
	if ((operation_ != FarmIrrigationPreviewOperation::PlaceCanalPath &&
		operation_ != FarmIrrigationPreviewOperation::RemoveCanalPath) ||
		!CanConfirm(sourceGrid)) {
		return false;
	}
	const auto found = std::find(changedTileIndices_.begin(), changedTileIndices_.end(), tileIndex);
	if (found == changedTileIndices_.end()) {
		return ExtendCanalPathTo(sourceGrid, tileIndex);
	}
	pathIssue_ = FarmCanalPathIssue::None;
	blockedTileIndex_ = -1;
	const auto keepCount = static_cast<std::size_t>(found - changedTileIndices_.begin()) + 1;
	if (keepCount == changedTileIndices_.size()) {
		return true;
	}
	// A visited earlier candidate keeps its prefix; restore the suffix from the source snapshot.
	for (std::size_t index = keepCount; index < changedTileIndices_.size(); ++index) {
		const int restoreIndex = changedTileIndices_[index];
		if (!previewGrid_.SetTile(restoreIndex, sourceSnapshot_.tiles[restoreIndex])) {
			Cancel();
			return false;
		}
	}
	changedTileIndices_.resize(keepCount);
	static_cast<void>(previewGrid_.SetSelectedIndex(tileIndex));
	previewIrrigation_.Rebuild(previewGrid_);
	return true;
}

bool FarmIrrigationPreviewSystem::ExtendCanalPathTo(const FarmGrid& sourceGrid, int tileIndex)
{
	if (changedTileIndices_.empty() || sourceGrid.GetTile(tileIndex) == nullptr) {
		return false;
	}
	const int from = changedTileIndices_.back();
	const int width = sourceGrid.GetWidth();
	if (width <= 0) {
		return false;
	}
	const bool sameRow = from / width == tileIndex / width;
	const bool sameColumn = from % width == tileIndex % width;
	if (!sameRow && !sameColumn) {
		pathIssue_ = FarmCanalPathIssue::NonStraight;
		blockedTileIndex_ = -1;
		return false;
	}
	const int step = (tileIndex > from ? 1 : -1) * (sameRow ? 1 : width);
	const int length = sameRow ? std::abs(tileIndex - from) : std::abs(tileIndex - from) / width;
	if (length <= 0 || changedTileIndices_.size() + static_cast<std::size_t>(length) >
		static_cast<std::size_t>(sourceGrid.GetTileCount())) {
		return false;
	}
	struct Candidate {
		int index;
		FarmTile tile;
	};
	std::vector<Candidate> segment;
	segment.reserve(static_cast<std::size_t>(length));
	// Validate the complete stroke before editing the preview, including skipped interior tiles.
	for (int offset = 1; offset <= length; ++offset) {
		const int index = from + step * offset;
		const FarmTile* source = sourceGrid.GetTile(index);
		FarmTile candidate{};
		if (source == nullptr || IsTileChanged(index) ||
			!TryBuildCandidateTile(*source, operation_, candidate, MaximumAllowedHeight())) {
			pathIssue_ = FarmCanalPathIssue::BlockedTile;
			blockedTileIndex_ = index;
			return false;
		}
		segment.push_back({index, candidate});
	}
	changedTileIndices_.reserve(changedTileIndices_.size() + segment.size());
	std::size_t applied = 0;
	for (; applied < segment.size(); ++applied) {
		if (!previewGrid_.SetTile(segment[applied].index, segment[applied].tile)) {
			break;
		}
	}
	if (applied != segment.size()) {
		pathIssue_ = FarmCanalPathIssue::BlockedTile;
		blockedTileIndex_ = segment[applied].index;
		while (applied > 0) {
			const int index = segment[--applied].index;
			static_cast<void>(previewGrid_.SetTile(index, sourceSnapshot_.tiles[index]));
		}
		return false;
	}
	for (const Candidate& candidate : segment) {
		changedTileIndices_.push_back(candidate.index);
	}
	static_cast<void>(previewGrid_.SetSelectedIndex(tileIndex));
	previewIrrigation_.Rebuild(previewGrid_);
	pathIssue_ = FarmCanalPathIssue::None;
	blockedTileIndex_ = -1;
	return true;
}

bool FarmIrrigationPreviewSystem::VisitTerrainTile(const FarmGrid& sourceGrid, int tileIndex)
{
	if ((operation_ != FarmIrrigationPreviewOperation::RaiseTerrain &&
		operation_ != FarmIrrigationPreviewOperation::LowerTerrain) ||
		!CanConfirm(sourceGrid) || sourceGrid.GetTile(tileIndex) == nullptr) {
		EndTerrainStroke();
		return false;
	}
	if (lastTerrainTileIndex_ == tileIndex) return pathIssue_ == FarmCanalPathIssue::None;
	const int from = lastTerrainTileIndex_ >= 0 ? lastTerrainTileIndex_ : tileIndex;
	lastTerrainTileIndex_ = tileIndex;
	const int width = sourceGrid.GetWidth();
	int x = from % width, y = from / width;
	const int targetX = tileIndex % width, targetY = tileIndex / width;
	const int dx = std::abs(targetX - x), dy = -std::abs(targetY - y);
	const int sx = x < targetX ? 1 : -1, sy = y < targetY ? 1 : -1;
	int error = dx + dy;
	std::vector<int> additions;
	additions.reserve(static_cast<std::size_t>((std::max)(dx, -dy)) + 1);
	// Rasterize sampled movement; validate the whole segment before changing candidates.
	for (;;) {
		const int index = y * width + x;
		if (!IsTileChanged(index)) {
			FarmTile candidate{};
			const auto* source = sourceGrid.GetTile(index);
			if (!source || !TryBuildCandidateTile(*source, operation_, candidate, MaximumAllowedHeight())) {
				pathIssue_ = FarmCanalPathIssue::BlockedTile;
				blockedTileIndex_ = index;
				return false;
			}
			additions.push_back(index);
		}
		if (x == targetX && y == targetY) break;
		const int twiceError = 2 * error;
		if (twiceError >= dy) { error += dy; x += sx; }
		if (twiceError <= dx) { error += dx; y += sy; }
	}
	changedTileIndices_.reserve(changedTileIndices_.size() + additions.size());
	for (const int index : additions) {
		FarmTile candidate{};
		if (!TryBuildCandidateTile(*sourceGrid.GetTile(index), operation_, candidate, MaximumAllowedHeight()) ||
			!previewGrid_.SetTile(index, candidate)) {
			Cancel();
			return false;
		}
		changedTileIndices_.push_back(index);
	}
	static_cast<void>(previewGrid_.SetSelectedIndex(tileIndex));
	if (!additions.empty()) previewIrrigation_.Rebuild(previewGrid_);
	pathIssue_ = FarmCanalPathIssue::None;
	blockedTileIndex_ = -1;
	return true;
}

void FarmIrrigationPreviewSystem::Cancel() noexcept
{
	previewGrid_ = {};
	previewIrrigation_.Initialize(rules_);
	sourceSnapshot_ = {};
	sourceGeneration_ = 0;
	originalTile_ = {};
	tileIndex_ = -1;
	operation_ = FarmIrrigationPreviewOperation::None;
	candidateFeature_ = FarmTileFeature::None;
	candidateHeightLevel_ = 0;
	changedTileIndices_.clear();
	active_ = false;
	pathIssue_ = FarmCanalPathIssue::None;
	blockedTileIndex_ = -1;
	EndTerrainStroke();
}

bool FarmIrrigationPreviewSystem::IsTileChanged(int tileIndex) const noexcept
{
	return active_ && std::find(
		changedTileIndices_.begin(), changedTileIndices_.end(), tileIndex) !=
		changedTileIndices_.end();
}

bool FarmIrrigationPreviewSystem::CanConfirm(const FarmGrid& sourceGrid) const noexcept
{
	if (!active_) {
		return false;
	}
	if (operation_ == FarmIrrigationPreviewOperation::RaiseTerrain && stageAccess_) {
		for (int index : changedTileIndices_) {
			const auto* candidate = previewGrid_.GetTile(index);
			if (!candidate || candidate->heightLevel > stageAccess_->Get().maximumHeight) return false;
		}
	}
	if (sourceGrid.GetGeneration() != sourceGeneration_ ||
		sourceGrid.GetWidth() != sourceSnapshot_.width ||
		sourceGrid.GetHeight() != sourceSnapshot_.height ||
		sourceGrid.GetTileCount() != static_cast<int>(sourceSnapshot_.tiles.size())) {
		return false;
	}
	for (int index = 0; index < sourceGrid.GetTileCount(); ++index) {
		const FarmTile* currentTile = sourceGrid.GetTile(index);
		if (currentTile == nullptr ||
			!TilesEqual(*currentTile, sourceSnapshot_.tiles[static_cast<std::size_t>(index)])) {
			return false;
		}
	}
	return true;
}

} // namespace farm
