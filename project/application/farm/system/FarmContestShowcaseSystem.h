#pragma once
#include "farm/system/FarmContestPresentationSystem.h"

// Exhibition specimens use V1 rules and synthetic care histories, never the player's farm.
class FarmContestShowcaseSystem final {
public:
    using Pair = FarmContestCheerSystem::Pair;
    struct Specimen { farm::CropType crop; float water; float nutrients; };
    inline static constexpr std::array<Specimen, static_cast<std::size_t>(Pair::Count)> kSpecimens{{
        {farm::CropType::Pumpkin,1,1},
        {farm::CropType::Pumpkin,.98f,.8f}, {farm::CropType::Pumpkin,.96f,.92f},
        {farm::CropType::Carrot,.98f,.96f}, {farm::CropType::Carrot,.96f,.95f},
        {farm::CropType::Tomato,.96f,.94f}, {farm::CropType::Tomato,.97f,.95f},
        {farm::CropType::Pumpkin,.98f,.96f},
        {farm::CropType::Pumpkin,.96f,.96f}, {farm::CropType::Carrot,.98f,.96f},
        {farm::CropType::Tomato,1,.98f}, {farm::CropType::Carrot,1,.98f},
        {farm::CropType::Carrot,.96f,1}, {farm::CropType::Pumpkin,1,.96f},
        {farm::CropType::Tomato,.96f,.96f}, {farm::CropType::Tomato,.96f,.96f}
    }};
    static_assert([] {
        for (const auto& specimen : kSpecimens)
            if (specimen.crop != farm::CropType::Carrot && specimen.crop != farm::CropType::Tomato &&
                specimen.crop != farm::CropType::Pumpkin) return false;
        return true;
    }(), "Gallery specimens must be playable crops");
    [[nodiscard]] static FarmContestComparison Build(int index) noexcept {
        if (index < 0 || index >= static_cast<int>(kSpecimens.size())) return {};
        const farm::FarmRules rules{};
        FarmCropQualitySystem quality; quality.Initialize(rules);
        FarmEconomySystem::ContestResults results{};
        for (std::size_t stage = 0; stage < results.size(); ++stage) {
            const auto specimen = stage == results.size()-1 ? kSpecimens[index] :
                Specimen{stage == 0 ? farm::CropType::Carrot : farm::CropType::Tomato,1,1};
            farm::FarmTile tile{};
            tile.state = farm::FarmTileState::Planted; tile.crop = specimen.crop; tile.growth = 1;
            switch (specimen.crop) {
            case farm::CropType::Carrot: tile.heightLevel = rules.carrotIdealHeight; break;
            case farm::CropType::Tomato: tile.heightLevel = rules.tomatoIdealHeight; break;
            case farm::CropType::Pumpkin: tile.heightLevel = rules.pumpkinIdealHeight; break;
            default: return {};
            }
            tile.careHistory.goodSeconds = 1;
            tile.careHistory.efficiencySeconds = specimen.water;
            tile.careHistory.nutrientGrowth = 1;
            tile.careHistory.nutrientSupply = specimen.nutrients;
            auto& result = results[stage];
            result.contestDay = FarmContestEntrySystem::kContestDays[stage];
            auto& harvest = result.harvest;
            harvest.id = static_cast<int>(stage) + 1; harvest.quantity = 1; harvest.saleProtected = true;
            harvest.harvestedDay = result.contestDay;
            harvest.quality = quality.Evaluate(tile);
            const auto score = FarmContestJudgeSystem::Evaluate(&harvest, kFarmContestSubmissionRulesV1);
            if (!score.IsValid()) return {};
            result.qualityPoints = score.qualityPoints; result.sizePoints = score.sizePoints;
        }
        const auto comparison = FarmContestComparisonSystem::Build(static_cast<int>(results.size())-1, FarmContestEntrySystem::kContestDays.back(),
            results, true, quality.MaximumHarvestSize(kSpecimens[index].crop));
        return comparison.valid && comparison.passed && FarmContestCheerSystem::Eligible(static_cast<Pair>(index),
            comparison.quality, comparison.total, comparison.qualityPoints) ? comparison : FarmContestComparison{};
    }
};
