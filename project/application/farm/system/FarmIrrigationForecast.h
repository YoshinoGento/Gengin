#pragma once
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace farm {
// Relative supply from a connected source, not soil moisture or retained canal water.
class FarmIrrigationForecast final {
public:
    struct Field {
        int before = -1, after = -1;
        bool Valid() const noexcept { return before >= 0 && after >= 0; }
    };
    struct Counts { int fields = 0, stronger = 0, weaker = 0; };
    void Capture(const FarmGrid& board, const FarmIrrigationSystem& water) {
        fields_.resize(static_cast<std::size_t>(board.GetTileCount()));
        for (int i = 0; i < board.GetTileCount(); ++i)
            fields_[i] = {Strength(board,water,i), -1};
        counts_ = {};
    }
    void Compare(const FarmGrid& candidate, const FarmIrrigationSystem& water) noexcept {
        counts_ = {};
        if (fields_.size() != static_cast<std::size_t>(candidate.GetTileCount())) { Clear(); return; }
        for (int i = 0; i < candidate.GetTileCount(); ++i) {
            auto& field = fields_[i];
            field.after = Strength(candidate,water,i);
            if (!field.Valid()) continue;
            ++counts_.fields;
            counts_.stronger += field.after > field.before;
            counts_.weaker += field.after < field.before;
        }
    }
    void Clear() noexcept { fields_.clear(); counts_ = {}; }
    const Field* Inspect(int i) const noexcept {
        if (i < 0 || static_cast<std::size_t>(i) >= fields_.size() || !fields_[i].Valid()) return nullptr;
        return &fields_[i];
    }
    Counts Summary() const noexcept { return counts_; }
private:
    static int Strength(const FarmGrid& grid, const FarmIrrigationSystem& water, int i) noexcept {
        const auto* t = grid.GetTile(i);
        if (!t || t->feature != FarmTileFeature::None || t->state == FarmTileState::Empty) return -1;
        if (!t->irrigationEnabled || !water.IsInIrrigationRange(i)) return 0;
        // Zero is reserved for disconnected or closed fields; match markers to the displayed precision.
        return std::clamp(static_cast<int>(std::lround(water.GetIrrigationStrength(i)*100.0f)),1,100);
    }
    std::vector<Field> fields_;
    Counts counts_;
};
}
