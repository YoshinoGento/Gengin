#pragma once
#include "farm/render/FarmPestMeshLayout.h"
#include "farm/system/FarmPestSystem.h"

namespace farm {
// Session-owned presentation. Never mutates gameplay or serializes transient cues.
class FarmPestVisualSystem final {
public:
    static constexpr float kCureSeconds = .55f;
    static constexpr float kPhasePeriod = 6.28318531f;
    static_assert(FarmPestMeshParts::kActiveLimit == FarmPestSystem::kMaximumActive);

    void Clear() noexcept { entries_ = {}; phase_ = 0; }
    bool OnCured(const FarmGrid& grid, int index, const FarmVisualSystem& visual) noexcept {
        const auto* tile = grid.GetTile(index);
        const auto data = visual.GetTileVisualData(grid, index);
        if (!tile || !data.valid || !IsPlantableCrop(tile->crop) || tile->pests.warningDay != 0) return false;
        std::size_t slot = entries_.size();
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (Matches(entries_[i], grid) && entries_[i].index == index) return false;
            if (slot == entries_.size() && !Matches(entries_[i], grid)) slot = i;
        }
        if (slot == entries_.size()) {
            slot = 0;
            for (std::size_t i = 1; i < entries_.size(); ++i)
                if (entries_[i].age > entries_[slot].age) slot = i;
        }
        entries_[slot] = {grid.GetGeneration(), index, tile->crop, tile->heightLevel,
            FarmPestCropTop(grid,index,visual), data.halfExtent, 0, true};
        return true;
    }
    void Update(const FarmGrid& grid, float deltaTime) noexcept {
        const float dt = std::isfinite(deltaTime) && deltaTime > 0 ? (std::min)(deltaTime, 1.0f) : 0;
        phase_ = std::fmod(phase_ + dt * 1.6f, kPhasePeriod);
        for (auto& entry : entries_) {
            if (!Matches(entry,grid)) { entry.active = false; continue; }
            entry.age += dt;
            if (entry.age >= kCureSeconds) entry.active = false;
        }
    }
    [[nodiscard]] FarmPestMeshParts BuildParts(const FarmGrid& grid, const FarmVisualSystem& visual) const noexcept {
        FarmPestMeshParts output;
        std::size_t active = 0;
        for (int i = 0; i < grid.GetTileCount() && active < FarmPestMeshParts::kActiveLimit; ++i) {
            const auto* tile = grid.GetTile(i);
            if (!tile || !tile->pests.warningDay) continue;
            AppendFarmPestMeshes(output,grid,i,visual,phase_);
            ++active;
        }
        const int selected = grid.GetSelectedIndex();
        if (const auto* tile = grid.GetTile(selected); tile && tile->repellentPlant) {
            const auto coverage = FarmRepellentSystem::Inspect(grid,false,true);
            for (const auto& cell : coverage.cells)
                if (FarmRepellentSystem::InRange(grid,selected,cell.index))
                    AppendFarmProtectionMeshes(output,grid,cell.index,visual);
        }
        for (const auto& entry : entries_)
            if (Matches(entry,grid)) AppendFarmCureMeshes(output,entry.top,entry.unit,entry.age/kCureSeconds);
        return output;
    }
private:
    struct Entry {
        std::uint64_t generation = 0;
        int index = -1;
        CropType crop = CropType::None;
        int height = 0;
        Vector3 top{};
        float unit = 0, age = 0;
        bool active = false;
    };
    static bool Matches(const Entry& entry, const FarmGrid& grid) noexcept {
        if (!entry.active || entry.generation != grid.GetGeneration()) return false;
        const auto* tile = grid.GetTile(entry.index);
        return tile && tile->crop == entry.crop && tile->heightLevel == entry.height && tile->pests.warningDay == 0;
    }
    std::array<Entry, FarmPestMeshParts::kActiveLimit> entries_{};
    float phase_ = 0;
};
} // namespace farm
