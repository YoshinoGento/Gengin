#pragma once
#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include <utility>

struct FarmCropSizeResult {
    float multiplier = 0.0f;
    bool known = false;
    static constexpr float kMaximumMultiplier = 8.0f;
    [[nodiscard]] bool IsConsistent() const noexcept {
        return std::isfinite(multiplier) && (known
            ? multiplier >= 0.01f && multiplier <= kMaximumMultiplier : multiplier == 0.0f);
    }
};

// Relative harvest size forecast, not current mesh scale or mass. No tile mutation.
class FarmCropSizeSystem final {
public:
    void Initialize(const farm::FarmRules& rules = {}) noexcept {
        minimum_ = Sanitize(rules.minimumCropSizeMultiplier, 0.5f);
        maximum_ = Sanitize(rules.maximumCropSizeMultiplier, 2.0f);
        if (minimum_ > maximum_) std::swap(minimum_, maximum_);
        tomatoMaximum_ = (std::max)(minimum_, Sanitize(rules.tomatoMaximumSizeMultiplier, 2.0f));
        pumpkinMaximum_ = (std::max)(minimum_, Sanitize(rules.pumpkinMaximumSizeMultiplier, 3.0f));
    }
    [[nodiscard]] FarmCropSizeResult Evaluate(const farm::FarmTile& tile) const noexcept {
        const auto& history = tile.careHistory;
        if (!farm::IsPlantableCrop(tile.crop) || !std::isfinite(tile.growth) ||
            tile.growth < 0.0f || tile.growth > 1.0f || !history.IsValid() ||
            history.GetObservedSeconds() <= 0.0 || history.nutrientGrowth <= 0.0f) return {};
        // Predict maturation under the average care observed so far; no instant-water fallback.
        const float nutrients = std::clamp(history.nutrientSupply / history.nutrientGrowth, 0.0f, 1.0f);
        const float care = history.GetAverageEfficiency() * nutrients;
        const float maximum = tile.crop == farm::CropType::Tomato ? tomatoMaximum_ :
            tile.crop == farm::CropType::Pumpkin ? pumpkinMaximum_ : maximum_;
        return {std::clamp(minimum_ + (maximum - minimum_) * care, minimum_, maximum), true};
    }
    [[nodiscard]] float MaximumFor(farm::CropType crop) const noexcept {
        if (!farm::IsPlantableCrop(crop)) return 0;
        return crop == farm::CropType::Tomato ? tomatoMaximum_ :
            crop == farm::CropType::Pumpkin ? pumpkinMaximum_ : maximum_;
    }
private:
    static float Sanitize(float value, float fallback) noexcept {
        return std::isfinite(value) && value >= 0.01f && value <= FarmCropSizeResult::kMaximumMultiplier
            ? value : fallback;
    }
    float minimum_ = 0.5f;
    float maximum_ = 2.0f;
    float tomatoMaximum_ = 2.0f;
    float pumpkinMaximum_ = 3.0f;
};
