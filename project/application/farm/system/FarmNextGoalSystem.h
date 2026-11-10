#pragma once

#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmPestSystem.h"

enum class FarmNextGoalKind {
    None, PestWarning, PestActive, Submit, ProtectAndReserve, Reserve, Reserved,
    SelectSoil, Till, Plant, ChooseCrop, BuySeed, SellForSeeds, NeedFunds,
    CheckWater, Water, StopIrrigation, ReduceWater, Grow, ResumeGrowth, Harvest, InventoryFull,
};

enum class FarmGoalSeedAccess { Owned, ChooseCrop, Affordable, CanSell, Insufficient };
enum class FarmGoalContestStatus { None, ProtectAndReserve, Reserve, Reserved, Today };

struct FarmGoalContest {
    FarmGoalContestStatus status = FarmGoalContestStatus::None;
    int day = 0;
};

struct FarmNextGoalFacts {
    bool blocked = false;
    int pestTile = -1;
    bool pestWarning = false;
    FarmGoalContest contest{};
    FarmWaterAdvice field = FarmWaterAdvice::Unknown;
    FarmGoalSeedAccess seeds = FarmGoalSeedAccess::Insufficient;
    bool harvestInventoryFull = false;
    bool paused = false;
};

struct FarmNextGoal {
    FarmNextGoalKind kind = FarmNextGoalKind::None;
    int pestTile = -1;
    int contestDay = 0;
};

struct FarmNextGoalContext {
    FarmWaterAdvice waterAdvice = FarmWaterAdvice::Unknown;
    farm::CropType selectedCrop = farm::CropType::None;
    int day = 1;
    bool blocked = false;
    bool pestsEnabled = false;
    bool contestSeason = false;
    bool paused = false;
};

// Read-only priorities. Advice never authorizes or performs a farm action.
class FarmNextGoalSystem final {
public:
    [[nodiscard]] static FarmNextGoalFacts Inspect(const farm::FarmGrid& grid,
        const FarmEconomySystem& economy, const FarmNextGoalContext& context) noexcept {
        FarmNextGoalFacts facts;
        facts.blocked = context.blocked;
        if (facts.blocked) return facts;
        if (context.pestsEnabled) {
            const int target = FarmPestSystem::FindActive(grid);
            if (const auto* tile = grid.GetTile(target)) {
                facts.pestTile = target;
                facts.pestWarning = tile->pests.damageDays == 0;
            }
        }
        if (facts.pestTile < 0 && context.contestSeason)
            facts.contest = InspectContest(economy, context.day);
        facts.field = InspectField(grid.GetSelectedTile(), context.waterAdvice);
        facts.paused = context.paused;
        if (facts.field == FarmWaterAdvice::Plant)
            facts.seeds = InspectSeeds(economy, context.selectedCrop);
        facts.harvestInventoryFull = economy.GetHarvestRecordCount() >= FarmEconomySystem::kMaxHarvestRecords;
        return facts;
    }

    [[nodiscard]] static FarmNextGoal Evaluate(const FarmNextGoalFacts& facts) noexcept {
        using Kind = FarmNextGoalKind;
        if (facts.blocked) return {};
        if (facts.pestTile >= 0) return {facts.pestWarning ? Kind::PestWarning : Kind::PestActive, facts.pestTile};
        switch (facts.contest.status) {
        case FarmGoalContestStatus::Today: return {Kind::Submit, -1, facts.contest.day};
        case FarmGoalContestStatus::ProtectAndReserve: return {Kind::ProtectAndReserve, -1, facts.contest.day};
        case FarmGoalContestStatus::Reserve: return {Kind::Reserve, -1, facts.contest.day};
        default: break;
        }
        // A completed reservation is a reminder, not a reason to hide field care.
        const FarmNextGoal reminder = facts.contest.status == FarmGoalContestStatus::Reserved
            ? FarmNextGoal{Kind::Reserved, -1, facts.contest.day} : FarmNextGoal{};
        switch (facts.field) {
        case FarmWaterAdvice::Till: return {Kind::Till};
        case FarmWaterAdvice::Plant:
            switch (facts.seeds) {
            case FarmGoalSeedAccess::Owned: return {Kind::Plant};
            case FarmGoalSeedAccess::ChooseCrop: return {Kind::ChooseCrop};
            case FarmGoalSeedAccess::Affordable: return {Kind::BuySeed};
            case FarmGoalSeedAccess::CanSell: return {Kind::SellForSeeds};
            default: return {Kind::NeedFunds};
            }
        case FarmWaterAdvice::CheckSupply: return {Kind::CheckWater};
        case FarmWaterAdvice::Water: return {Kind::Water};
        case FarmWaterAdvice::CloseIntake: return {Kind::StopIrrigation};
        case FarmWaterAdvice::AvoidWater: return {Kind::ReduceWater};
        case FarmWaterAdvice::Monitor:
            if (facts.paused) return {Kind::ResumeGrowth};
            return reminder.kind != Kind::None ? reminder : FarmNextGoal{Kind::Grow};
        case FarmWaterAdvice::Harvest: return {facts.harvestInventoryFull ? Kind::InventoryFull : Kind::Harvest};
        default: return reminder.kind != Kind::None ? reminder : FarmNextGoal{Kind::SelectSoil};
        }
    }

    [[nodiscard]] static FarmWaterAdvice InspectField(const farm::FarmTile* tile, FarmWaterAdvice advice) noexcept {
        if (!tile || tile->feature != farm::FarmTileFeature::None) return FarmWaterAdvice::Unknown;
        // The water view also recognizes legacy Watered tiles, but Seed requires Tilled.
        if (advice == FarmWaterAdvice::Plant && tile->state != farm::FarmTileState::Tilled)
            return FarmWaterAdvice::Unknown;
        return advice;
    }

    [[nodiscard]] static FarmGoalSeedAccess InspectSeeds(const FarmEconomySystem& economy, farm::CropType selected) noexcept {
        using Access = FarmGoalSeedAccess;
        if (economy.IsCropUnlocked(selected) && economy.GetSeedCount(selected) > 0) return Access::Owned;
        const int price = economy.IsCropUnlocked(selected) ? economy.GetSeedPrice(selected) : 0;
        bool affordableAlternative = false;
        for (int i = 0; i < farm::kPlayableCropCount; ++i) {
            const auto crop = farm::PlayableCrop(i);
            if (crop == selected || !economy.IsCropUnlocked(crop)) continue;
            if (economy.GetSeedCount(crop) > 0) return Access::ChooseCrop;
            const int otherPrice = economy.GetSeedPrice(crop);
            affordableAlternative |= otherPrice > 0 && otherPrice <= economy.GetMoney();
        }
        if (price > 0 && price <= economy.GetMoney()) return Access::Affordable;
        if (affordableAlternative) return Access::ChooseCrop;
        return economy.PreviewSale().Succeeded() ? Access::CanSell : Access::Insufficient;
    }

    [[nodiscard]] static FarmGoalContest InspectContest(const FarmEconomySystem& economy, int day) noexcept {
        using Status = FarmGoalContestStatus;
        const auto entry = FarmContestEntrySystem::Evaluate(day, economy.GetContestReservation());
        if (!entry.HasContest() || entry.daysRemaining > kDeadlineLeadDays) return {};
        const auto& results = economy.GetContestResults();
        for (std::size_t i = 0; i < results.size(); ++i)
            if (FarmContestEntrySystem::kContestDays[i] == entry.contestDay && results[i].contestDay != 0) return {};
        if (entry.issue == FarmContestEntryIssue::Eligible)
            return {entry.daysRemaining == 0 ? Status::Today : Status::Reserved, entry.contestDay};

        bool canProtect = false;
        for (std::size_t i = 0; i < economy.GetHarvestRecordCount(); ++i) {
            const auto* record = economy.GetHarvestRecord(i);
            if (!record || record->harvestedDay < entry.firstHarvestDay || record->harvestedDay > day) continue;
            if (record->saleProtected) {
                if (FarmEconomySystem::CanReserveForContest(*record)) return {Status::Reserve, entry.contestDay};
            } else {
                auto protectedCandidate = *record;
                protectedCandidate.saleProtected = true;
                canProtect |= FarmEconomySystem::CanReserveForContest(protectedCandidate);
            }
        }
        return canProtect ? FarmGoalContest{Status::ProtectAndReserve, entry.contestDay} : FarmGoalContest{};
    }

private:
    static constexpr int kDeadlineLeadDays = 2;
};
