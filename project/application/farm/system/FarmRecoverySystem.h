#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmEconomySystem.h"

// Aid restarts cultivation, not the farm's finances or contest results.
class FarmRecoverySystem final {
public:
    static constexpr farm::CropType kSeed = farm::CropType::Carrot;
    static constexpr int kQuantity = 1;
    enum class Status { Blocked, SeedsAvailable, CropGrowing, HarvestAvailable, CanBuy, ClaimedToday, Eligible };
    struct View {
        Status status = Status::Blocked;
        int day = 0;
        int money = 0;
        std::uint64_t inventoryGeneration = 0;
        std::uint64_t farmGeneration = 0;
        [[nodiscard]] bool CanClaim() const noexcept { return status == Status::Eligible; }
    };

    [[nodiscard]] static View Inspect(const farm::FarmGrid& grid, const FarmEconomySystem& economy,
        int day, bool blocked = false) noexcept {
        View result{Status::Blocked, day, economy.GetMoney(),
            economy.GetInventoryGeneration(), grid.GetGeneration()};
        if (blocked || day <= 0 || grid.GetTileCount() <= 0 || !economy.IsCropUnlocked(kSeed)) return result;
        // Locked and retired TestCrop seeds cannot restart normal play.
        bool canBuy = false;
        for (int i = 0; i < farm::kPlayableCropCount; ++i) {
            const auto crop = farm::PlayableCrop(i);
            if (!economy.IsCropUnlocked(crop)) continue;
            if (economy.GetSeedCount(crop) > 0) { result.status = Status::SeedsAvailable; return result; }
            const int price = economy.GetSeedPrice(crop);
            canBuy |= price > 0 && economy.GetMoney() >= price;
        }
        for (int i = 0; i < grid.GetTileCount(); ++i) {
            const auto* tile = grid.GetTile(i);
            if (tile && tile->crop != farm::CropType::None) { result.status = Status::CropGrowing; return result; }
        }
        // Include protected/reserved and legacy unrecorded harvests; never sell them implicitly.
        if (economy.GetTotalCropCount() > 0) result.status = Status::HarvestAvailable;
        else if (canBuy) result.status = Status::CanBuy;
        else if (economy.GetLastRecoveryDay() >= day) result.status = Status::ClaimedToday;
        else result.status = Status::Eligible;
        return result;
    }

    [[nodiscard]] static bool Claim(const farm::FarmGrid& grid, FarmEconomySystem& economy,
        int day, const View& request, bool blocked = false) noexcept {
        if (!request.CanClaim() || request.day != day ||
            request.inventoryGeneration != economy.GetInventoryGeneration() ||
            request.farmGeneration != grid.GetGeneration() || !Inspect(grid, economy, day, blocked).CanClaim()) return false;
        return economy.GrantRecoverySeed(kSeed, kQuantity, day);
    }
};
