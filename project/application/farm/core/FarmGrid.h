#pragma once

#include "farm/core/FarmTypes.h"

#include <vector>
#include <cstdint>

namespace farm {

class FarmGrid {
public:
	struct Snapshot {
		int width = 0;
		int height = 0;
		int selectedX = 0;
		int selectedY = 0;
		std::vector<FarmTile> tiles;
	};

	bool Initialize(int width, int height);
	void MoveSelection(int dx, int dy);
	bool SetSelectedIndex(int index);
	int GetWidth() const { return width_; }
	int GetHeight() const { return height_; }
	int GetTileCount() const { return static_cast<int>(tiles_.size()); }
	int GetSelectedIndex() const;

	void UpdateGrowth(float deltaTime);
	bool SelectTile(int index);

	const FarmTile* GetTile(int index) const;
	const FarmTile* GetSelectedTile() const;
	bool SetTile(int index, const FarmTile& tile);
	[[nodiscard]] static bool IsValidTile(const FarmTile& tile) noexcept;
	void CaptureSnapshot(Snapshot& output) const;
	bool RestoreSnapshot(const Snapshot& snapshot);
	// Explicit document/development replacement; ordinary Undo remains dimension-strict.
	bool ReplaceSnapshot(const Snapshot& snapshot);
	[[nodiscard]] uint64_t GetGeneration() const noexcept { return generation_; }

private:
	bool IsValid() const;

private:
	int width_ = 0;
	int height_ = 0;
	int selectedX_ = 0;
	int selectedY_ = 0;
	std::vector<FarmTile> tiles_;
	uint64_t generation_ = 0;
};

} // namespace farm
