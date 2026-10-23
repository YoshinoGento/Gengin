#pragma once

#include "farm/system/FarmToolActionTypes.h"

class FarmEconomySystem;
class FarmStageAccessSystem;
class FarmCropQualitySystem;

// These borrows last only for Plan(); a behavior cannot mutate the live grid or inventory.
struct FarmToolContext {
    const farm::FarmTile& tile;
    farm::CropType selectedCrop;
    const FarmEconomySystem* economy;
    const FarmStageAccessSystem* stageAccess;
    const FarmCropQualitySystem& quality;
    float wateringIncrement;
};

struct FarmToolPlan {
    FarmToolActionResult result{};
    farm::FarmTile after{};
    const char* commandName = "Farm Tool"; // Borrows a string literal, retained by undo history.
    farm::CropType plantedCrop = farm::CropType::None;
    int plantedQuantity = 0;
    int harvestedQuantity = 0;
};

// A behavior plans one soil-tile operation; committing/undoing belongs to FarmToolActionSystem.
class FarmToolBehavior {
public:
    virtual ~FarmToolBehavior() = default;
    [[nodiscard]] virtual FarmToolPlan Plan(const FarmToolContext& context) const noexcept = 0;
};

// Immutable process-lifetime instances; unsupported tools (including BugNet) return nullptr.
[[nodiscard]] const FarmToolBehavior* FindFarmToolBehavior(FarmTool tool) noexcept;
