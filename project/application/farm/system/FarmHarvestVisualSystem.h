#pragma once
#include "farm/render/FarmHarvestPose.h"
#include <array>

namespace farm {
// Transient presentation only. Inventory is already updated before Start is called.
class FarmHarvestVisualSystem final {
public:
    static constexpr std::size_t kCapacity = 8;
    static constexpr float kDuration = harvest::kDuration;

    bool Start(const FarmToolActionResult& result, const FarmGrid& grid, const FarmVisualSystem& visualSystem) noexcept {
        if (result.status != FarmToolActionStatus::Harvested || !result.harvestedTile ||
            !IsHarvestReady(*result.harvestedTile)) return false;
        const auto* current = grid.GetTile(result.tileIndex);
        const auto& before = *result.harvestedTile;
        if (!current || current->state != FarmTileState::Tilled || current->crop != CropType::None ||
            current->feature != FarmTileFeature::None || current->heightLevel != before.heightLevel) return false;
        auto visual = visualSystem.GetTileVisualData(grid, result.tileIndex);
        visual.crop = before.crop;
        visual.cropStage = FarmCropGrowthStage::Ready;
        visual.cropScale = visualSystem.GetCropRenderScale(before);
        auto meshes = BuildFarmCropMeshParts(visual, before.growth);
        if (meshes.count != 2) return false;
        for (const auto& mesh : meshes.parts) if (!IsValidPart(mesh)) return false;
        std::size_t slot = kCapacity;
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            if (Matches(entries_[i], grid) && entries_[i].tileIndex == result.tileIndex) return false;
            if (slot == kCapacity && !Matches(entries_[i], grid)) slot = i;
        }
        if (slot == kCapacity) {
            slot = 0;
            for (std::size_t i = 1; i < entries_.size(); ++i)
                if (entries_[i].age > entries_[slot].age) slot = i;
        }
        // Anchor the basket on the bed's plateau, toward the interior of the board.
        const float side=result.tileIndex%grid.GetWidth()<grid.GetWidth()/2?1.f:-1.f;
        const auto basket=visual.center+Vector3{side*.70f*visual.halfExtent,kFarmSoilRidgeHeight,-.57f*visual.halfExtent};
        entries_[slot] = {{meshes,visual.center,basket,1.30f*visual.halfExtent,before.moisture,harvest::CueFor(result.harvestQuality.harvestSize)}, &grid,grid.GetGeneration(),
            result.tileIndex,before.heightLevel,0.f,true};
        return true;
    }

    void Clear() noexcept { entries_ = {}; }
    void Update(const FarmGrid& grid, float deltaTime) noexcept {
        Update(grid,deltaTime,[](const harvest::BasketLanding&){});
    }
    template<class OnContact> void Update(const FarmGrid& grid,float deltaTime,OnContact onContact) noexcept {
        const float step = std::isfinite(deltaTime) && deltaTime > 0 ? (std::min)(deltaTime, kDuration) : 0;
        for (auto& entry : entries_) {
            if (!Matches(entry, grid)) { entry.active = false; continue; }
            const float previous=entry.age;
            entry.age += step;
            if(previous<harvest::kContact && entry.age>=harvest::kContact && entry.age<harvest::kDuration &&
                entry.pickup.crop.parts[0].shape==FarmMeshShape::Pumpkin) onContact(harvest::Contact(entry.pickup));
            if (entry.age >= kDuration) entry.active = false;
        }
    }
    [[nodiscard]] std::size_t GetActiveCount(const FarmGrid& grid) const noexcept {
        std::size_t count = 0;
        for (const auto& entry : entries_) count += Matches(entry, grid) ? 1 : 0;
        return count;
    }
    [[nodiscard]] harvest::Parts GetParts(const FarmGrid& grid, std::size_t index) const noexcept {
        if (index >= entries_.size() || !Matches(entries_[index], grid)) return {};
        const auto& entry = entries_[index];
        return harvest::Sample(entry.pickup,entry.age);
    }
private:
    struct Entry {
        harvest::Pickup pickup{};
        const FarmGrid* board=nullptr; // Identity only; the caller owns the live grid.
        uint64_t generation = 0;
        int tileIndex = -1, height = 0;
        float age = 0;
        bool active = false;
    };
    static bool IsValidPart(const FarmMeshPart& part) noexcept {
        return std::isfinite(part.position.x) && std::isfinite(part.position.y) && std::isfinite(part.position.z) &&
            std::isfinite(part.scale.x) && std::isfinite(part.scale.y) && std::isfinite(part.scale.z) &&
            part.scale.x >= 0.0001f && part.scale.y >= 0.0001f && part.scale.z >= 0.0001f;
    }
    static bool Matches(const Entry& entry, const FarmGrid& grid) noexcept {
        if (!entry.active || entry.board!=&grid || entry.generation != grid.GetGeneration()) return false;
        const auto* tile = grid.GetTile(entry.tileIndex);
        return tile && tile->crop == CropType::None && tile->feature == FarmTileFeature::None &&
            tile->state == FarmTileState::Tilled && tile->heightLevel == entry.height;
    }
    std::array<Entry, kCapacity> entries_{};
};
} // namespace farm
