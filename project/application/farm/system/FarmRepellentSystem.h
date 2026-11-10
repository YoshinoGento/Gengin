#pragma once

#include "farm/core/FarmGrid.h"
#include <array>
#include <cstdlib>

// Abstract game rule: protection uses grid adjacency, not botanical measurements.
class FarmRepellentSystem final {
public:
    static constexpr int kRadius = 1;
    enum class CellKind { Soil, Crop, Water, Plant };
    struct Cell {
        int index = -1;
        CellKind kind = CellKind::Soil;
        bool protectedNow = false;
        bool protectedAfter = false;
    };
    struct View {
        std::array<Cell, 9> cells{};
        int selected = -1;
        int tileCount = 0;
        std::uint64_t generation = 0;
        bool present = false;
        bool canPlace = false;
        bool canRemove = false;
        bool pestsEnabled = false;
    };

    [[nodiscard]] static bool CanPlace(const farm::FarmTile* tile) noexcept {
        return tile && !tile->repellentPlant && tile->feature == farm::FarmTileFeature::None &&
            tile->crop == farm::CropType::None && tile->growth == 0.0f &&
            tile->careHistory.IsEmpty() && tile->pests == farm::FarmPestState{} &&
            (tile->state == farm::FarmTileState::Empty || tile->state == farm::FarmTileState::Tilled ||
             tile->state == farm::FarmTileState::Watered);
    }

    [[nodiscard]] static bool InRange(const farm::FarmGrid& grid, int source, int target) noexcept {
        if (!grid.GetTile(source) || !grid.GetTile(target) || grid.GetWidth() <= 0) return false;
        return std::abs(source % grid.GetWidth() - target % grid.GetWidth()) +
            std::abs(source / grid.GetWidth() - target / grid.GetWidth()) <= kRadius;
    }

    [[nodiscard]] static bool IsProtected(const farm::FarmGrid& grid, int index, int excluded = -1) noexcept {
        if (!grid.GetTile(index) || grid.GetWidth() <= 0) return false;
        const int x = index % grid.GetWidth(), y = index / grid.GetWidth();
        constexpr std::array<std::array<int, 2>, 5> offsets{{{0,0},{-1,0},{1,0},{0,-1},{0,1}}};
        for (const auto& offset : offsets) {
            const int nx = x + offset[0], ny = y + offset[1];
            if (nx < 0 || nx >= grid.GetWidth() || ny < 0 || ny >= grid.GetHeight()) continue;
            const int candidate = ny * grid.GetWidth() + nx;
            const auto* tile = grid.GetTile(candidate);
            if (candidate != excluded && tile && tile->repellentPlant) return true;
        }
        return false;
    }

    static bool SetPlant(farm::FarmGrid& grid, int index, std::uint64_t generation, bool place) noexcept {
        if (grid.GetGeneration() != generation || grid.GetSelectedIndex() != index) return false;
        const auto* current = grid.GetTile(index);
        if (!current || (place ? !CanPlace(current) : !current->repellentPlant)) return false;
        auto changed = *current;
        changed.repellentPlant = place;
        return grid.SetTile(index, changed);
    }

    [[nodiscard]] static View Inspect(const farm::FarmGrid& grid, bool blocked, bool pestsEnabled) noexcept {
        View result;
        result.selected = grid.GetSelectedIndex();
        result.tileCount = grid.GetTileCount();
        result.generation = grid.GetGeneration();
        result.pestsEnabled = pestsEnabled;
        const auto* selected = grid.GetTile(result.selected);
        if (!selected || grid.GetWidth() <= 0) return result;
        result.present = selected->repellentPlant;
        result.canPlace = !blocked && CanPlace(selected);
        result.canRemove = !blocked && result.present;
        for (int row = 0; row < 3; ++row) for (int col = 0; col < 3; ++col) {
            const int x = result.selected % grid.GetWidth() + col - 1;
            const int y = result.selected / grid.GetWidth() + row - 1;
            if (x < 0 || x >= grid.GetWidth() || y < 0 || y >= grid.GetHeight()) continue;
            const int index = y * grid.GetWidth() + x;
            const auto* tile = grid.GetTile(index);
            if (!tile) continue;
            auto& cell = result.cells[static_cast<std::size_t>(row * 3 + col)];
            cell.index = index;
            cell.kind = tile->repellentPlant ? CellKind::Plant :
                tile->feature != farm::FarmTileFeature::None ? CellKind::Water :
                tile->crop != farm::CropType::None ? CellKind::Crop : CellKind::Soil;
            cell.protectedNow = IsProtected(grid, index);
            cell.protectedAfter = result.present ? IsProtected(grid, index, result.selected) :
                cell.protectedNow || (result.canPlace && InRange(grid, result.selected, index));
        }
        return result;
    }
};
