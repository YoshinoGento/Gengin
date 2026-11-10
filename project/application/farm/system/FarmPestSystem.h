#pragma once

#include "farm/core/FarmGrid.h"
#include <algorithm>

// All persistent state belongs to the crop on its tile, including after a cure.
class FarmPestSystem final {
public:
    struct DayResult {
        bool changed = false;
        bool warningStarted = false;
    };

    [[nodiscard]] static int FindActive(const farm::FarmGrid& grid) noexcept {
        for (int i = 0; i < grid.GetTileCount(); ++i)
            if (const auto* tile = grid.GetTile(i); tile && tile->pests.warningDay != 0) return i;
        return -1;
    }

    [[nodiscard]] static DayResult AdvanceDays(farm::FarmGrid& grid, int before, int after, bool enabled) {
        using State = farm::FarmPestState;
        if (!enabled || before < 1 || after <= before) return {};
        if (const int active = FindActive(grid); active >= 0) {
            auto tile = *grid.GetTile(active);
            // Derive elapsed exposure from the calendar; reprocessing a boundary is idempotent.
            const int days = std::clamp(after - tile.pests.warningDay, 0, State::kMaximumDamageDays);
            if (days <= tile.pests.damageDays) return {};
            tile.pests.damageDays = days;
            return {grid.SetTile(active, tile), false};
        }
        for (int day = State::kFirstWarningDay; day <= State::kLastWarningDay; day += State::kWarningInterval) {
            if (day <= before || day > after) continue;
            for (int i = 0; i < grid.GetTileCount(); ++i) {
                const auto* candidate = grid.GetTile(i);
                if (!candidate || candidate->state != farm::FarmTileState::Planted ||
                    !farm::IsPlantableCrop(candidate->crop) || candidate->pests.damageDays != 0) continue;
                auto tile = *candidate;
                tile.pests.warningDay = day;
                tile.pests.damageDays = (std::min)(after - day, State::kMaximumDamageDays);
                const bool applied = grid.SetTile(i, tile);
                return {applied, applied && day == after};
            }
            break;
        }
        return {};
    }

    [[nodiscard]] static bool Cure(farm::FarmGrid& grid, int index, int expectedWarningDay) {
        const auto* current = grid.GetTile(index);
        if (!current || expectedWarningDay <= 0 || current->pests.warningDay != expectedWarningDay) return false;
        auto tile = *current;
        tile.pests.warningDay = 0;
        return grid.SetTile(index, tile);
    }

    static void DismissActive(farm::FarmGrid& grid) {
        const int index = FindActive(grid);
        if (const auto* tile = grid.GetTile(index))
            static_cast<void>(Cure(grid, index, tile->pests.warningDay));
    }
};
