#pragma once

#include "farm/data/FarmRules.h"
#include "farm/system/FarmContestSeasonSystem.h"
#include "farm/system/FarmStageAccessSystem.h"

enum class FarmProgressionMode { Trial, ContestSeason, FreeFarming };

class FarmProgressionSystem final {
public:
	struct Snapshot {
		int targetMoney = 1;
		bool cleared = false;
		FarmProgressionMode mode = FarmProgressionMode::Trial;
		bool stagedUnlocks = false;
		bool pestsEnabled = false;
	};

	void Initialize(const farm::FarmRules& rules = {}, FarmProgressionMode mode = FarmProgressionMode::Trial,
		bool enablePests = false) noexcept;
	[[nodiscard]] bool EvaluateClear(int currentMoney) noexcept;
	[[nodiscard]] bool EvaluateSeason(const FarmContestSeasonSummary& summary) noexcept;
	bool SetMode(FarmProgressionMode mode, int money, const FarmContestSeasonSummary& summary) noexcept;
	[[nodiscard]] FarmProgressionMode GetMode() const noexcept { return mode_; }
	[[nodiscard]] bool IsContestSeason() const noexcept { return mode_ == FarmProgressionMode::ContestSeason; }
	[[nodiscard]] bool IsFreeFarming() const noexcept { return mode_ == FarmProgressionMode::FreeFarming; }
	[[nodiscard]] bool PestsEnabled() const noexcept { return pestsEnabled_; }
	[[nodiscard]] const FarmStagePolicy& StagePolicy() const noexcept { return stagePolicy_; }
	[[nodiscard]] static farm::FarmRules InitialRules(FarmProgressionMode mode) noexcept;
	[[nodiscard]] static bool ValidMode(FarmProgressionMode mode) noexcept {
		return mode == FarmProgressionMode::Trial || mode == FarmProgressionMode::ContestSeason ||
			mode == FarmProgressionMode::FreeFarming;
	}

	[[nodiscard]] bool IsCleared() const noexcept { return cleared_; }
	[[nodiscard]] int GetTargetMoney() const noexcept { return targetMoney_; }
	[[nodiscard]] int GetRemainingMoney(int currentMoney) const noexcept;
	[[nodiscard]] int GetRequiredCropCount(int currentMoney, int cropSellPrice) const noexcept;
	[[nodiscard]] float GetProgress(int currentMoney, int day = 1) const noexcept;
	[[nodiscard]] Snapshot CaptureSnapshot() const noexcept;
	bool RestoreSnapshot(const Snapshot& snapshot) noexcept;

private:
	int targetMoney_ = 1;
	bool cleared_ = false;
	FarmProgressionMode mode_ = FarmProgressionMode::Trial;
	FarmStagePolicy stagePolicy_{};
	bool pestsEnabled_ = false;
};
