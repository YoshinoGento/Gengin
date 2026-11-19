#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmEconomySystem.h"

class FarmFungicideSystem final {
public:
    enum class Status { Ready, NoStock, NoGrowingCrop, Sick, Protected };
    static Status Inspect(const farm::FarmTile* tile, const FarmEconomySystem& economy) noexcept {
        if (!tile || tile->state != farm::FarmTileState::Planted || tile->feature != farm::FarmTileFeature::None ||
            farm::ToCropSlot(tile->crop) <= 0 || farm::IsHarvestReady(*tile)) return Status::NoGrowingCrop;
        if (tile->mold.sick) return Status::Sick;
        if (tile->mold.protectedSeconds > 0) return Status::Protected;
        return economy.GetFungicideCount() > 0 ? Status::Ready : Status::NoStock;
    }
    static bool Apply(farm::FarmGrid& grid, int index, FarmEconomySystem& economy) noexcept {
        const auto* tile = grid.GetTile(index);
        if (Inspect(tile, economy) != Status::Ready) return false;
        auto next = *tile;
        next.mold = {};
        next.mold.protectedSeconds = farm::FarmMoldState::kProtectionSeconds;
        // Only a committed bed may consume inventory; both mutations run on the simulation thread.
        if (!grid.SetTile(index, next)) return false;
        economy.ConsumeFungicide();
        return true;
    }
};
