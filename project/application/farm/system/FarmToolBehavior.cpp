#include "farm/system/FarmToolBehavior.h"
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmStageAccessSystem.h"
#include "farm/system/FarmCropQualitySystem.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr float kFullMoisture = 1.0f;
constexpr float kInitialGrowth = 0.0f;
constexpr float kHarvestReadyGrowth = 1.0f;
constexpr float kHarvestMoistureCost = 0.25f;

FarmToolPlan BeginPlan(FarmTool tool, const FarmToolContext& context) noexcept {
    FarmToolPlan plan;
    plan.result.tool = tool;
    plan.result.status = FarmToolActionStatus::InvalidState;
    plan.after = context.tile;
    return plan;
}

class HoeBehavior final : public FarmToolBehavior {
public:
    FarmToolPlan Plan(const FarmToolContext& context) const noexcept override {
        auto plan = BeginPlan(FarmTool::Hoe, context);
        if (context.tile.feature != farm::FarmTileFeature::None ||
            context.tile.state != farm::FarmTileState::Empty) return plan;
        plan.result.status = FarmToolActionStatus::Applied;
        plan.after.state = farm::FarmTileState::Tilled;
        plan.commandName = "Hoe Tile";
        return plan;
    }
};

class WaterBehavior final : public FarmToolBehavior {
public:
    FarmToolPlan Plan(const FarmToolContext& context) const noexcept override {
        auto plan = BeginPlan(FarmTool::Water, context);
        const auto& tile = context.tile;
        if (tile.feature != farm::FarmTileFeature::None ||
            (tile.state != farm::FarmTileState::Tilled && tile.state != farm::FarmTileState::Planted) ||
            !std::isfinite(tile.moisture) || tile.moisture < 0.0f || tile.moisture > kFullMoisture ||
            !std::isfinite(context.wateringIncrement) || context.wateringIncrement <= 0.0f ||
            context.wateringIncrement > kFullMoisture) return plan;
        plan.result.moistureBefore = tile.moisture;
        plan.result.moistureAfter = (std::min)(kFullMoisture, tile.moisture + context.wateringIncrement);
        plan.result.status = tile.moisture >= kFullMoisture
            ? FarmToolActionStatus::AlreadyWatered : FarmToolActionStatus::Applied;
        plan.after.moisture = plan.result.moistureAfter;
        plan.commandName = "Water Tile";
        return plan;
    }
};

class SeedBehavior final : public FarmToolBehavior {
public:
    FarmToolPlan Plan(const FarmToolContext& context) const noexcept override {
        auto plan = BeginPlan(FarmTool::Seed, context);
        if (context.tile.feature != farm::FarmTileFeature::None ||
            !farm::IsPlantableCrop(context.selectedCrop)) return plan;
        if ((context.stageAccess && !context.stageAccess->Get().Allows(context.selectedCrop)) ||
            (context.economy && !context.economy->IsCropUnlocked(context.selectedCrop))) {
            plan.result.status = FarmToolActionStatus::Locked;
        } else if (context.tile.state != farm::FarmTileState::Tilled) {
            return plan;
        } else if (context.economy && context.economy->GetSeedCount(context.selectedCrop) <= 0) {
            plan.result.status = FarmToolActionStatus::NoSeed;
        } else {
            plan.result.status = FarmToolActionStatus::Applied;
            plan.after.state = farm::FarmTileState::Planted;
            plan.after.crop = context.selectedCrop;
            plan.after.growth = kInitialGrowth;
            plan.after.careHistory = {};
            plan.commandName = "Plant Seed";
            plan.plantedCrop = context.selectedCrop;
            plan.plantedQuantity = 1;
        }
        return plan;
    }
};

class HarvestBehavior final : public FarmToolBehavior {
public:
    FarmToolPlan Plan(const FarmToolContext& context) const noexcept override {
        auto plan = BeginPlan(FarmTool::Harvest, context);
        const auto& tile = context.tile;
        if (tile.feature != farm::FarmTileFeature::None ||
            tile.state != farm::FarmTileState::Planted || tile.crop == farm::CropType::None) return plan;
        if (tile.growth < kHarvestReadyGrowth) {
            plan.result.status = FarmToolActionStatus::NotReady;
        } else if (context.economy && context.economy->GetHarvestRecordCount() >= FarmEconomySystem::kMaxHarvestRecords) {
            plan.result.status = FarmToolActionStatus::InventoryFull;
        } else {
            plan.result.harvestQuality = context.quality.Evaluate(tile);
            if (!plan.result.harvestQuality.IsValid()) return plan;
            plan.result.status = FarmToolActionStatus::Harvested;
            plan.result.reward = plan.result.harvestQuality.salePrice;
            plan.after.state = farm::FarmTileState::Tilled;
            plan.after.crop = farm::CropType::None;
            plan.after.growth = kInitialGrowth;
            plan.after.moisture = (std::max)(0.0f, tile.moisture - kHarvestMoistureCost);
            plan.after.careHistory = {};
            plan.commandName = "Harvest Crop";
            plan.harvestedQuantity = 1;
        }
        return plan;
    }
};
}

const FarmToolBehavior* FindFarmToolBehavior(FarmTool tool) noexcept {
    static const HoeBehavior hoe;
    static const WaterBehavior water;
    static const SeedBehavior seed;
    static const HarvestBehavior harvest;
    // This is the registration boundary; callers dispatch behavior through the base interface.
    switch (tool) {
    case FarmTool::Hoe: return &hoe;
    case FarmTool::Water: return &water;
    case FarmTool::Seed: return &seed;
    case FarmTool::Harvest: return &harvest;
    default: return nullptr;
    }
}
