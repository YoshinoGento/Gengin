#pragma once

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmRepellentSystem.h"
#include <algorithm>
#include <array>

// Crop exposure lives on the tile; the spread clock belongs to Progression.
class FarmPestSystem final {
public:
	static constexpr int kSpreadDelayDays = 2;
	static constexpr int kMaximumActive = 3;
    struct DayResult {
        bool changed = false;
        bool warningStarted = false;
    };

    [[nodiscard]] static int FindActive(const farm::FarmGrid& grid) noexcept {
        for (int i = 0; i < grid.GetTileCount(); ++i)
            if (const auto* tile = grid.GetTile(i); tile && tile->pests.warningDay != 0) return i;
        return -1;
    }

    [[nodiscard]] static int CountActive(const farm::FarmGrid& grid) noexcept {
        int count = 0;
        for (int i = 0; i < grid.GetTileCount(); ++i)
            if (const auto* tile = grid.GetTile(i); tile && tile->pests.warningDay != 0) ++count;
        return count;
    }

    // Legacy campaigns retain their original single-target schedule.

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
                    !farm::IsPlantableCrop(candidate->crop) || candidate->pests.damageDays != 0 ||
                    FarmRepellentSystem::IsProtected(grid, i)) continue;
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

    [[nodiscard]] static DayResult AdvanceDays(farm::FarmGrid& grid, int before, int after,
        bool enabled, farm::FarmPestSpreadState& spread) {
        if (!spread.IsValid()) return {};
        if (spread.version == 0) return AdvanceDays(grid, before, after, enabled);
        if (!enabled || before < 1 || after <= before || after <= spread.lastProcessedDay) return {};
        DayResult result{true, false}; // A clock-only change must also be persisted.
        const int first = (std::max)(before, spread.lastProcessedDay);
        const int last = (std::min)(after, farm::FarmPestSpreadState::kLastSpreadDay);
        // Bounded by the season, even if the caller skips to a very large date.
        for (int day = first; day < last;) {
            ++day;
            std::array<int, kMaximumActive> sources{};
            int active = 0;
            for (int i = 0; i < grid.GetTileCount(); ++i) {
                const auto* tile = grid.GetTile(i);
                if (!tile || tile->pests.warningDay == 0) continue;
                if (active < kMaximumActive) sources[static_cast<std::size_t>(active)] = i;
                ++active;
            }
            int target = -1;
            if (active == 0 && farm::FarmPestState::IsWarningDay(day)) {
                for (int i = 0; i < grid.GetTileCount(); ++i)
                    if (CanInfest(grid, i)) { target = i; break; }
            } else if (active > 0 && active < kMaximumActive) {
                for (int i = 0; i < active && target < 0; ++i) {
                    const int source = sources[static_cast<std::size_t>(i)];
                    const auto* tile = grid.GetTile(source);
                    if (day - tile->pests.warningDay < kSpreadDelayDays) continue;
                    target = FindNeighbor(grid, source);
                }
            }
            if (const auto* candidate = grid.GetTile(target)) {
                auto tile = *candidate;
                tile.pests = {day, 0};
                if (grid.SetTile(target, tile)) result.warningStarted |= day == after;
            }
        }
        for (int i = 0; i < grid.GetTileCount(); ++i) {
            const auto* current = grid.GetTile(i);
            if (!current || current->pests.warningDay == 0) continue;
            auto tile = *current;
            const int days = std::clamp(after - tile.pests.warningDay, 0, farm::FarmPestState::kMaximumDamageDays);
            if (days <= tile.pests.damageDays) continue;
            tile.pests.damageDays = days;
            static_cast<void>(grid.SetTile(i, tile));
        }
        spread.lastProcessedDay = after;
        return result;
    }

    [[nodiscard]] static bool Cure(farm::FarmGrid& grid, int index, int expectedWarningDay) {
        const auto* current = grid.GetTile(index);
        if (!current || expectedWarningDay <= 0 || current->pests.warningDay != expectedWarningDay) return false;
        auto tile = *current;
        tile.pests.warningDay = 0;
        return grid.SetTile(index, tile);
    }

    static void DismissActive(farm::FarmGrid& grid) {
        for (int index = 0; index < grid.GetTileCount(); ++index)
            if (const auto* tile = grid.GetTile(index); tile && tile->pests.warningDay != 0)
                static_cast<void>(Cure(grid, index, tile->pests.warningDay));
    }

private:
    static bool CanInfest(const farm::FarmGrid& grid, int index) noexcept {
        const auto* tile = grid.GetTile(index);
        return tile && tile->state == farm::FarmTileState::Planted && farm::IsPlantableCrop(tile->crop) &&
            !farm::IsHarvestReady(*tile) && tile->pests == farm::FarmPestState{} &&
            !FarmRepellentSystem::IsProtected(grid, index);
    }
    static int FindNeighbor(const farm::FarmGrid& grid, int source) noexcept {
        const int x = source % grid.GetWidth(), y = source / grid.GetWidth();
        constexpr std::array<std::array<int, 2>, 4> offsets{{{-1,0},{1,0},{0,-1},{0,1}}};
        for (const auto& offset : offsets) {
            const int nx = x + offset[0], ny = y + offset[1];
            if (nx < 0 || nx >= grid.GetWidth() || ny < 0 || ny >= grid.GetHeight()) continue;
            const int target = ny * grid.GetWidth() + nx;
            if (CanInfest(grid, target)) return target;
        }
        return -1;
    }
};
