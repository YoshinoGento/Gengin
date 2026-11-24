#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include <array>
#include <cstdint>

namespace farm { class FarmGrid; }

enum class FarmComparisonStatus { Idle, Running, Stopped, Completed, Invalidated };
enum class FarmComparisonIssue { None, PickTwo, SameTile, NotGrowing, Crop, Height, Growth, InvalidData };

struct FarmComparisonRow {
	int tileIndex = -1;
	farm::FarmTile initial{};
	farm::FarmTile current{};
	double readySeconds = -1.0;
};

struct FarmComparisonView {
	std::array<FarmComparisonRow, 2> rows{};
	FarmComparisonStatus status = FarmComparisonStatus::Idle;
	FarmComparisonIssue startIssue = FarmComparisonIssue::PickTwo;
	double elapsedSeconds = 0.0;
};

// Observes real tiles; never writes Grid or owns its lifetime. Identity pointers are not dereferenced.
class FarmGrowthComparisonSystem final {
public:
	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	bool Pin(const farm::FarmGrid& grid, int slot, int tileIndex) noexcept;
	bool Start(const farm::FarmGrid& grid) noexcept;
	// True only on the call that ends a running comparison; reading the view never emits an event.
	bool Stop() noexcept;
	bool Reset() noexcept;
	bool ObserveBeforeStep(const farm::FarmGrid& grid) noexcept;
	bool ObserveAfterStep(const farm::FarmGrid& grid, float deltaTime, float timeScale) noexcept;
	[[nodiscard]] FarmComparisonView GetView(const farm::FarmGrid& grid) const noexcept;

private:
	bool FinishRun(FarmComparisonStatus reason) noexcept;
	[[nodiscard]] bool MatchesGrid(const farm::FarmGrid& grid) const noexcept;
	[[nodiscard]] FarmComparisonIssue CheckStart(const farm::FarmGrid& grid) const noexcept;
	const farm::FarmGrid* gridIdentity_ = nullptr;
	uint64_t generation_ = 0;
	float maxDeltaTime_ = farm::FarmRules{}.maxUpdateDeltaTime;
	FarmComparisonView view_{};
};
