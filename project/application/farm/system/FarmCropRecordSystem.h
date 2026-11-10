#pragma once
#include "farm/system/FarmCropQualitySystem.h"
#include <algorithm>
#include <array>
#include <cmath>

class FarmCropRecordSystem final {
public:
    inline static constexpr std::array kCrops{
        farm::CropType::Carrot, farm::CropType::Tomato, farm::CropType::Pumpkin};
    struct Record {
        bool harvested = false;
        int bestQuality = 0;
        FarmCropSizeResult largest{};
    };
    using Records = std::array<Record, kCrops.size()>;

    [[nodiscard]] static bool Validate(const Records& records) noexcept {
        for (const auto& r : records) {
            if (r.bestQuality < 0 || r.bestQuality > 100 || !r.largest.IsConsistent() ||
                (!r.harvested && (r.bestQuality != 0 || r.largest.known))) return false;
        }
        return true;
    }
    static void Accumulate(Records& records, const FarmCropQualityResult& quality) noexcept {
        const auto unit = [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; };
        if (!quality.IsValid() || !unit(quality.maturity) || !unit(quality.waterBalance) ||
            !unit(quality.terrainFit) || !unit(quality.nutrientBalance)) return;
        for (std::size_t i = 0; i < kCrops.size(); ++i) {
            if (quality.crop != kCrops[i]) continue;
            auto& r = records[i];
            r.harvested = true;
            r.bestQuality = (std::max)(r.bestQuality, quality.score);
            if (quality.harvestSize.known &&
                (!r.largest.known || quality.harvestSize.multiplier > r.largest.multiplier))
                r.largest = quality.harvestSize;
            return;
        }
    }
};
