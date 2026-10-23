#pragma once
#include "farm/core/FarmTypes.h"
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmToolSystem.h"
#include <optional>

enum class FarmToolActionStatus {
	None,
	Applied,
	Harvested,
	InvalidTarget,
	InvalidState,
	AlreadyWatered,
	NoSeed,
	InventoryFull,
	NotReady,
	UnsupportedTool,
	Locked,
};

struct FarmToolActionResult {
	FarmToolActionStatus status = FarmToolActionStatus::None;
	FarmTool tool = FarmTool::Hoe;
	int tileIndex = -1;
	int reward = 0;
	FarmCropQualityResult harvestQuality{};
	float moistureBefore = 0.0f;
	float moistureAfter = 0.0f;
	std::optional<farm::FarmTile> harvestedTile; // Present only after the harvest command commits.

	[[nodiscard]] bool Succeeded() const noexcept {
		return status == FarmToolActionStatus::Applied ||
			status == FarmToolActionStatus::Harvested;
	}
};

