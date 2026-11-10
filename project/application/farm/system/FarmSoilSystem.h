#pragma once
#include "farm/core/FarmTypes.h"

struct FarmSoilProfile {
    float target;
    float consumptionPerGrowth;
};

// Abstract game balance, not agricultural or dietary measurements.
class FarmSoilSystem final {
public:
    inline static constexpr FarmSoilProfile kTurnip{0.45f, 0.30f};
    inline static constexpr FarmSoilProfile kCarrot{0.65f, 0.45f};
    inline static constexpr FarmSoilProfile kTomato{0.70f, 0.50f};
    inline static constexpr FarmSoilProfile kPumpkin{0.80f, 0.65f};
    [[nodiscard]] static const FarmSoilProfile* Profile(farm::CropType crop) noexcept {
        switch (crop) {
        case farm::CropType::TestCrop: return &kTurnip;
        case farm::CropType::Carrot: return &kCarrot;
        case farm::CropType::Tomato: return &kTomato;
        case farm::CropType::Pumpkin: return &kPumpkin;
        default: return nullptr;
        }
    }
    [[nodiscard]] static bool Valid(float nutrients) noexcept {
        return std::isfinite(nutrients) && nutrients >= 0.0f && nutrients <= 1.0f;
    }
    [[nodiscard]] static float Satisfaction(float nutrients, farm::CropType crop) noexcept {
        const auto* profile = Profile(crop);
        return profile && Valid(nutrients) ? std::clamp(nutrients / profile->target, 0.0f, 1.0f) : 0.0f;
    }
    [[nodiscard]] static bool CanCompost(const farm::FarmTile& tile) noexcept {
        return !tile.repellentPlant && tile.feature == farm::FarmTileFeature::None && tile.crop == farm::CropType::None &&
            (tile.state == farm::FarmTileState::Tilled || tile.state == farm::FarmTileState::Watered) &&
            Valid(tile.soilNutrients) && tile.soilNutrients < 1.0f;
    }
    static bool ApplyCompost(farm::FarmTile& tile) noexcept {
        if (!CanCompost(tile)) return false;
        tile.soilNutrients = 1.0f;
        return true;
    }
    static void Grow(farm::FarmTile& tile, float growthDelta) noexcept {
        const auto* profile = Profile(tile.crop);
        if (!profile || tile.feature != farm::FarmTileFeature::None ||
            tile.state != farm::FarmTileState::Planted || !Valid(tile.soilNutrients) ||
            !tile.careHistory.IsValid() || !std::isfinite(growthDelta) || growthDelta <= 0.0f) return;
        const float amount = (std::min)(growthDelta, 1.0f - tile.careHistory.nutrientGrowth);
        const float before = tile.soilNutrients;
        const float after = (std::max)(0.0f, before - profile->consumptionPerGrowth * amount);
        // Integrate the linear shortage curve across threshold crossings; independent of tick size.
        const auto integral = [profile](float value) {
            return value <= profile->target ? value * value / (2.0f * profile->target)
                : value - profile->target * 0.5f;
        };
        const float supplied = (integral(before) - integral(after)) / profile->consumptionPerGrowth;
        tile.careHistory.nutrientGrowth += amount;
        tile.careHistory.nutrientSupply = std::clamp(tile.careHistory.nutrientSupply + supplied,
            0.0f, tile.careHistory.nutrientGrowth);
        tile.soilNutrients = after;
    }
};
