#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace farm {

bool FarmGrid::Initialize(int width, int height)
{
	++generation_;
	width_ = 0;
	height_ = 0;
	selectedX_ = 0;
	selectedY_ = 0;
	tiles_.clear();

	if (width <= 0 || height <= 0) {
		return false;
	}

	width_ = width;
	height_ = height;
	tiles_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), FarmTile{});
	return true;
}

void FarmGrid::MoveSelection(int dx, int dy)
{
	if (!IsValid()) {
		return;
	}

	selectedX_ = std::clamp(selectedX_ + dx, 0, width_ - 1);
	selectedY_ = std::clamp(selectedY_ + dy, 0, height_ - 1);
}

bool FarmGrid::SetSelectedIndex(int index)
{
	if (!IsValid() || index < 0 || index >= GetTileCount()) {
		return false;
	}

	selectedX_ = index % width_;
	selectedY_ = index / width_;
	return true;
}

bool FarmGrid::SelectTile(int index)
{
	return SetSelectedIndex(index);
}

void FarmGrid::UpdateGrowth(float)
{
	// Growth is owned by FarmGrowthSystem. This legacy entry point is intentionally inert.
}

int FarmGrid::GetSelectedIndex() const
{
	if (!IsValid()) {
		return -1;
	}

	return selectedY_ * width_ + selectedX_;
}

const FarmTile* FarmGrid::GetSelectedTile() const
{
	return GetTile(GetSelectedIndex());
}

const FarmTile* FarmGrid::GetTile(int index) const
{
	if (index < 0 || index >= GetTileCount()) {
		return nullptr;
	}

	return &tiles_[static_cast<std::size_t>(index)];
}

bool FarmGrid::IsValidTile(const FarmTile& tile) noexcept
{
    const auto unit = [](float value) { return std::isfinite(value) && value >= 0.0f && value <= 1.0f; };
    if (!unit(tile.moisture) || !unit(tile.growth) || !unit(tile.waterAmount) ||
        !unit(tile.soilNutrients) || !tile.careHistory.IsValid() ||
        tile.heightLevel < 0 || tile.heightLevel > 2 || !IsValidFarmTileFeature(tile.feature)) return false;
    switch (tile.state) {
    case FarmTileState::Empty: case FarmTileState::Tilled: case FarmTileState::Watered:
    case FarmTileState::Planted: case FarmTileState::ReadyToHarvest: break;
    default: return false;
    }
    if (tile.crop != CropType::None && ToCropSlot(tile.crop) < 0) return false;
    if (tile.feature == FarmTileFeature::None && tile.waterAmount != 0.0f) return false;
    if (tile.state != FarmTileState::Planted && !tile.careHistory.IsEmpty()) return false;
    if (!tile.pests.IsValid() || (tile.pests != FarmPestState{} &&
        (tile.state != FarmTileState::Planted || !IsPlantableCrop(tile.crop)))) return false;
    if (tile.feature != FarmTileFeature::None &&
        (tile.state != FarmTileState::Empty || tile.crop != CropType::None ||
         tile.moisture != 0.0f || tile.growth != 0.0f || !tile.careHistory.IsEmpty())) return false;
    return true;
}

bool FarmGrid::SetTile(int index, const FarmTile& tile)
{
    if (index < 0 || index >= GetTileCount() || !IsValidTile(tile)) return false;
    tiles_[static_cast<std::size_t>(index)] = tile;
    return true;
}

void FarmGrid::CaptureSnapshot(Snapshot& snapshot) const
{
	snapshot.width = width_;
	snapshot.height = height_;
	snapshot.selectedX = selectedX_;
	snapshot.selectedY = selectedY_;
	snapshot.tiles = tiles_;
}

bool FarmGrid::RestoreSnapshot(const Snapshot& snapshot)
{
	if (snapshot.width != width_ || snapshot.height != height_ ||
		snapshot.tiles.size() != tiles_.size() || !IsValid()) {
		return false;
	}

	for (const FarmTile& tile : snapshot.tiles) {
        if (!IsValidTile(tile)) return false;
    }

	selectedX_ = std::clamp(snapshot.selectedX, 0, width_ - 1);
	selectedY_ = std::clamp(snapshot.selectedY, 0, height_ - 1);
	tiles_ = snapshot.tiles;
	++generation_;
	return true;
}

bool FarmGrid::IsValid() const
{
	return width_ > 0 && height_ > 0 && !tiles_.empty();
}

} // namespace farm
