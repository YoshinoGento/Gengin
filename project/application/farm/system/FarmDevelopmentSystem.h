#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmEconomySystem.h"
#include <array>
#include <algorithm>

class FarmDevelopmentSystem final {
public:
    static constexpr int kWidth = 5;
    static constexpr int kInitialHeight = 4;
    static constexpr int kMaximumHeight = 6;
    static constexpr std::array kSalesThresholds{0, 500, 1500};
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
        view.canExpand = !blocked && view.standardFarm && !view.maximumArea &&
            grid.GetHeight() < kInitialHeight + view.rank - 1;
        return view;
    }
    [[nodiscard]] static bool Expand(farm::FarmGrid& grid, const FarmEconomySystem& economy,
        std::uint64_t expectedGeneration, int expectedHeight) {
        if (grid.GetGeneration() != expectedGeneration || grid.GetHeight() != expectedHeight ||
            !Inspect(grid, economy).canExpand) return false;
        farm::FarmGrid::Snapshot next;
        grid.CaptureSnapshot(next);
        ++next.height;
        // Append rows rather than columns to preserve every existing tile index and crop.
        next.tiles.resize(static_cast<std::size_t>(next.width) * next.height);
        return grid.ReplaceSnapshot(next);
    }
};
