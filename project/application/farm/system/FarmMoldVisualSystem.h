#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmVisualSystem.h"
#include <array>
#include <bitset>

namespace farm {
// One entry per bed in the largest playable farm (5x6). Nothing here is serialized.
struct FarmMoldFrame {
    static constexpr int kBeds = 30;
    struct Bed { Vector3 root{}; float unit{}, stain{}, puff{}; int index{}; };
    std::array<Bed, kBeds> beds{};
    std::size_t count{};
};

class FarmMoldVisualSystem final {
public:
    static constexpr float kBurstSeconds = 1.15f;
    using Cases = std::bitset<FarmMoldFrame::kBeds>;

    static Cases Capture(const FarmGrid& grid) noexcept {
        Cases cases;
        for (int i = 0; i < Limit(grid); ++i) cases[i] = grid.GetTile(i)->mold.sick;
        return cases;
    }

    // Called on either side of committed growth only, never around a load, undo or preview.
    void AfterGrowth(const FarmGrid& grid, Cases before) noexcept {
        const auto started = Capture(grid) & ~before;
        if (started.none()) return;
        for (int i = 0; i < Limit(grid); ++i) if (started[i])
            bursts_[i] = {grid.GetGeneration(), grid.GetTile(i)->crop, kBurstSeconds};
    }

    void Clear() noexcept { bursts_ = {}; }
    void Update(float seconds) noexcept {
        if (!std::isfinite(seconds) || seconds <= 0) return;
        for (auto& b : bursts_) b.remaining = (std::max)(0.f, b.remaining - seconds);
    }

    [[nodiscard]] FarmMoldFrame Frame(const FarmGrid& grid, const FarmVisualSystem& visual) const noexcept {
        FarmMoldFrame frame;
        for (int i = 0; i < Limit(grid); ++i) {
            const auto& tile = *grid.GetTile(i);
            if (!tile.mold.sick || !tile.mold.Valid() || tile.state != FarmTileState::Planted ||
                tile.feature != FarmTileFeature::None || ToCropSlot(tile.crop) <= 0) continue;
            const auto& b = bursts_[i];
            const bool active = b.remaining > 0 && b.generation == grid.GetGeneration() && tile.crop == b.crop;
            frame.beds[frame.count++] = {visual.GetTileCenter(grid,i), visual.GetLayout().tileSize*.5f,
                tile.mold.exposure / FarmMoldState::kOnsetSeconds,
                active ? 1.f - b.remaining/kBurstSeconds : -1.f, i};
        }
        return frame;
    }
private:
    struct Burst { std::uint64_t generation{}; CropType crop{}; float remaining{}; };
    static int Limit(const FarmGrid& grid) noexcept { return (std::min)(FarmMoldFrame::kBeds, grid.GetTileCount()); }
    std::array<Burst, FarmMoldFrame::kBeds> bursts_{};
};
}
