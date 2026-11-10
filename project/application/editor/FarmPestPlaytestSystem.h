#pragma once

#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmPestSystem.h"
#include "farm/system/FarmToolActionSystem.h"

#include <utility>

namespace editor {

// This sample owns its entire farm. It has no document or scene access.
class FarmPestPlaytestSystem final {
public:
	enum class Action { Start, AdvanceDay, Cure, HarvestAndJudge };
	enum class Phase { Idle, Warning, Infested, Cured, Judged };

	struct View {
		Phase phase = Phase::Idle;
		int day = 0;
		int damageDays = 0;
		int undamagedQuality = 0;
		FarmCropQualityResult quality{};
		FarmContestJudgeResult judge{};
		bool canAdvance = false;
		bool canCure = false;
		bool canHarvest = false;
	};

	[[nodiscard]] View Read() const noexcept { return view_; }
	[[nodiscard]] bool Execute(Action action) {
		switch (action) {
		case Action::Start: return Start();
		case Action::AdvanceDay:
			if (!view_.canAdvance) return false;
			static_cast<void>(FarmPestSystem::AdvanceDays(grid_, view_.day, view_.day + 1, true));
			++view_.day;
			RefreshCrop();
			return true;
		case Action::Cure: {
			const auto* tile = grid_.GetTile(kSampleTile);
			if (!view_.canCure || !tile || !FarmPestSystem::Cure(grid_, kSampleTile, tile->pests.warningDay)) return false;
			RefreshCrop();
			return true;
		}
		case Action::HarvestAndJudge: return HarvestAndJudge();
		}
		return false;
	}

private:
	static constexpr int kSampleTile = 0;
	static constexpr int kContestDay = FarmContestEntrySystem::kContestDays.front();
	static constexpr auto kSampleCrop = farm::CropType::Carrot;

	bool Start() {
		constexpr int warningDay = farm::FarmPestState::kFirstWarningDay;
		farm::FarmGrid sample;
		if (!sample.Initialize(1, 1)) return false;
		farm::FarmTile crop;
		crop.state = farm::FarmTileState::Planted;
		crop.crop = kSampleCrop;
		crop.heightLevel = 1;
		crop.growth = 1.0f;
		crop.moisture = 0.65f;
		crop.careHistory.goodSeconds = 20.0f;
		crop.careHistory.efficiencySeconds = crop.careHistory.goodSeconds;
		crop.careHistory.nutrientGrowth = 1.0f;
		crop.careHistory.nutrientSupply = crop.careHistory.nutrientGrowth;
		if (!sample.SetTile(kSampleTile, crop) ||
			!FarmPestSystem::AdvanceDays(sample, warningDay - 1, warningDay, true).warningStarted) return false;
		quality_.Initialize();
		grid_ = std::move(sample);
		view_ = {};
		view_.day = warningDay;
		view_.undamagedQuality = quality_.Evaluate(crop).score;
		RefreshCrop();
		return true;
	}

	void RefreshCrop() {
		const auto* tile = grid_.GetTile(kSampleTile);
		if (!tile) { view_ = {}; return; }
		view_.damageDays = tile->pests.damageDays;
		view_.canCure = tile->pests.warningDay != 0;
		view_.phase = !view_.canCure ? Phase::Cured :
			view_.damageDays == 0 ? Phase::Warning : Phase::Infested;
		view_.canAdvance = view_.day < kContestDay;
		view_.quality = quality_.Evaluate(*tile);
		view_.canHarvest = farm::IsHarvestReady(*tile) && view_.quality.IsValid();
	}

	bool HarvestAndJudge() {
		if (!view_.canHarvest) return false;
		auto sample = grid_;
		FarmEconomySystem economy;
		economy.Initialize();
		FarmToolActionSystem tools;
		tools.Initialize();
		const auto harvest = tools.ApplyToolDetailed(sample, FarmTool::Harvest, kSampleCrop, economy, view_.day);
		const auto* record = economy.GetHarvestRecord(0);
		if (!harvest.Succeeded() || !record) return false;
		const int id = record->id;
		if (!economy.SetHarvestProtection(id, true, economy.GetInventoryGeneration()) ||
			!economy.SetContestReservation(id, true, economy.GetInventoryGeneration())) return false;
		const auto judge = FarmContestJudgeSystem::Evaluate(economy.GetContestReservation(), kFarmContestSubmissionRulesV1);
		if (!judge.IsValid() || !FarmContestSubmissionSystem::Submit(economy, kContestDay, id, economy.GetInventoryGeneration()))
			return false;
		// Commit only after a valid harvest and submission. The sample never saves to disk.
		view_.quality = harvest.harvestQuality;
		view_.judge = judge;
		view_.day = kContestDay;
		view_.phase = Phase::Judged;
		view_.canAdvance = view_.canCure = view_.canHarvest = false;
		return true;
	}

	farm::FarmGrid grid_;
	FarmCropQualitySystem quality_;
	View view_{};
};

} // namespace editor
