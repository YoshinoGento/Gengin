#pragma once
#include "farm/system/FarmEconomySystem.h"
#include "farm/system/FarmContestPresentationSystem.h"

class FarmRecordBookSystem final {
public:
    struct Row {
        farm::CropType crop = farm::CropType::None;
        bool harvested = false, weightKnown = false;
        int quality = 0, grams = 0;
    };
    struct View {
        std::array<Row, FarmCropRecordSystem::kCrops.size()> rows{};
        int collected = 0;
        farm::CropType nextCrop = farm::CropType::None;
    };
    [[nodiscard]] static View Inspect(const FarmEconomySystem& economy) noexcept {
        auto records = economy.GetSoldCropRecords();
        // Unsold harvests stay reversible; submitted harvests already have durable snapshots.
        for (std::size_t i = 0; i < economy.GetHarvestRecordCount(); ++i)
            if (const auto* r = economy.GetHarvestRecord(i)) FarmCropRecordSystem::Accumulate(records, r->quality);
        for (const auto& r : economy.GetContestResults())
            if (r.contestDay != 0) FarmCropRecordSystem::Accumulate(records, r.harvest.quality);
        View view;
        for (std::size_t i = 0; i < records.size(); ++i) {
            const auto& r = records[i];
            const auto crop = FarmCropRecordSystem::kCrops[i];
            view.rows[i] = {crop, r.harvested, r.largest.known, r.bestQuality,
                r.largest.known ? FarmContestComparisonSystem::ToGrams(crop, r.largest.multiplier) : 0};
            if (r.harvested) ++view.collected;
            else if (view.nextCrop == farm::CropType::None) view.nextCrop = crop;
        }
        return view;
    }
};
