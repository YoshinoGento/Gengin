#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmEconomySystem.h"
#include <array>
#include <algorithm>
#include <limits>
#include <new>

class FarmDevelopmentSystem final {
public:
    static constexpr int kWidth = 5;
    static constexpr int kInitialHeight = 4;
    static constexpr int kMaximumHeight = 6;
    static constexpr std::array kSalesThresholds{0, 500, 1500};
    static constexpr std::array kExpansionCosts{600, 1800};
    static constexpr int kReserveSeeds = 2;
    struct Quote {
        std::uint64_t farmGeneration = 0;
        std::uint64_t inventoryGeneration = 0;
        int height = 0;
        int money = 0;
        int cost = 0;
        bool operator==(const Quote&) const = default;
    };
    struct View {
        int rank = 1;
        int sales = 0;
        int remainingSales = 0;
        int tiles = 0;
        int height = 0;
        bool standardFarm = false;
        bool maximumRank = false;
        bool maximumArea = false;
        bool canExpand = false;
        int requiredSales = 0;
        int unlockDay = 0;
        int cost = 0;
        int money = 0;
        int moneyAfter = 0;
        int seedReserve = 0;
        bool stageReady = false;
        bool salesReady = false;
        bool affordable = false;
        Quote quote{};
    };
    [[nodiscard]] static bool StandardSize(int width, int height) noexcept {
        return width == kWidth && height >= kInitialHeight && height <= kMaximumHeight;
    }
    [[nodiscard]] static int Rank(int sales) noexcept {
        return sales >= kSalesThresholds[2] ? 3 : sales >= kSalesThresholds[1] ? 2 : 1;
    }
    [[nodiscard]] static View Inspect(const farm::FarmGrid& grid, const FarmEconomySystem& economy,
        bool blocked = false) noexcept {
        View view;
        view.sales = economy.GetLifetimeSales();
        view.rank = Rank(view.sales);
        view.maximumRank = view.rank == static_cast<int>(kSalesThresholds.size());
        view.remainingSales = view.maximumRank ? 0 : (std::max)(0, kSalesThresholds[view.rank] - view.sales);
        view.tiles = grid.GetTileCount();
        view.height = grid.GetHeight();
        view.standardFarm = StandardSize(grid.GetWidth(), grid.GetHeight());
        view.maximumArea = view.standardFarm && grid.GetHeight() == kMaximumHeight;
        view.money = economy.GetMoney();
        if (!view.standardFarm || view.maximumArea) return view;
        const auto row = static_cast<std::size_t>(grid.GetHeight() - kInitialHeight);
        const auto access = economy.GetStageAccess();
        view.requiredSales = kSalesThresholds[row + 1];
        view.cost = kExpansionCosts[row];
        view.unlockDay = access.restricted ? kFarmStageRulesV1[row + 1].firstDay : 0;
        view.stageReady = !access.restricted || access.stage >= static_cast<int>(row + 1);
        view.salesReady = view.sales >= view.requiredSales;
        int seedPrice = 0;
        for (int i = 0; i < farm::kPlayableCropCount; ++i) {
            const auto crop = farm::PlayableCrop(i);
            if (access.Allows(crop)) seedPrice = (std::max)(seedPrice, economy.GetSeedPrice(crop));
        }
        view.seedReserve = static_cast<int>((std::min)(static_cast<long long>(seedPrice) * kReserveSeeds,
            static_cast<long long>((std::numeric_limits<int>::max)())));
        view.moneyAfter = view.money - view.cost;
        view.affordable = view.moneyAfter >= view.seedReserve;
        view.canExpand = !blocked && view.stageReady && view.salesReady && view.affordable;
        view.quote = {grid.GetGeneration(), economy.GetInventoryGeneration(), view.height, view.money, view.cost};
        return view;
    }
    [[nodiscard]] static bool Expand(farm::FarmGrid& grid, FarmEconomySystem& economy, const Quote& quote) {
        const auto current = Inspect(grid, economy);
        if (!current.canExpand || current.quote != quote) return false;
        try {
            farm::FarmGrid::Snapshot next;
            grid.CaptureSnapshot(next);
            ++next.height;
            // Append rows to preserve tile indices. All allocations precede payment.
            next.tiles.resize(static_cast<std::size_t>(next.width) * next.height);
            if (!grid.ReplaceSnapshot(next)) return false;
        } catch (const std::bad_alloc&) { return false; }
        economy.CommitDevelopmentPayment(current.cost);
        return true;
    }
};
