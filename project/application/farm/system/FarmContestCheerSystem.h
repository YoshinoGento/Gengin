#pragma once
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/data/FarmContestCheerAssets.h"

// Eligibility uses submitted facts only; no claims about taste, pesticides or irrigation method.
class FarmContestCheerSystem final {
public:
    // TurnipStar is a stable recording/provenance key; its roundness praise now belongs to Pumpkin.
    enum class Pair {
        Perfect, PumpkinPot, PumpkinFestival, CarrotCurry, CarrotBundle,
        TomatoCenter, TomatoLunch, TurnipStar, LargeDelivery, LargeShop,
        QualityMuseum, QualityKnife, NutrientSoil, WaterCare, Portrait, Guests,
        Count, None = -1
    };
    enum class Side { None, Left, Right };
    struct Frame {
        Pair pair = Pair::None;
        Side side = Side::None;
        float opacity = 0;
        float entrance = 0;
        bool voiceReady = false;
        float ducking = 0;
    };
    static constexpr float kVoiceDelay = .10f, kMaximumVoiceSeconds = farm::cheerassets::kMaximumVoiceSeconds;
    [[nodiscard]] static constexpr int ClipIndex(Pair pair, Side side) noexcept {
        const int index = static_cast<int>(pair);
        if (index < 0 || index >= static_cast<int>(Pair::Count) ||
            (side != Side::Left && side != Side::Right)) return -1;
        return index * 2 + (side == Side::Right ? 1 : 0);
    }
    [[nodiscard]] static constexpr float LongestVoice(Side side) noexcept {
        float longest = 0;
        for (int pair=0; pair<static_cast<int>(Pair::Count); ++pair) {
            const int index = ClipIndex(static_cast<Pair>(pair),side);
            if (index < 0) return 0;
            longest = (std::max)(longest,farm::cheerassets::kClips[index].seconds);
        }
        return longest;
    }
    [[nodiscard]] static constexpr float VoiceSeconds(Pair pair, Side side) noexcept {
        const int index = ClipIndex(pair, side);
        return index < 0 ? 0 : farm::cheerassets::kClips[index].seconds;
    }
    [[nodiscard]] static constexpr float ReplyGap(Pair pair) noexcept {
        const int index = static_cast<int>(pair);
        return index < 0 || index >= static_cast<int>(Pair::Count) ? .36f : farm::cheerassets::kReplyGaps[index];
    }
    static_assert(farm::cheerassets::kClips.size() == static_cast<std::size_t>(Pair::Count)*2);
    static_assert(farm::cheerassets::kReplyGaps.size() == static_cast<std::size_t>(Pair::Count));
    static_assert([] {
        for (const auto& clip : farm::cheerassets::kClips)
            if (!(clip.seconds > 0 && clip.seconds < kMaximumVoiceSeconds)) return false;
        for (const auto gap : farm::cheerassets::kReplyGaps)
            if (!(gap >= .30f && gap <= .45f)) return false;
        return true;
    }());
    struct Rule {
        farm::CropType crop = farm::CropType::None;
        bool large = false;
        int qualityPoints = 0;
        bool nutrients = false, water = false, perfect = false;
    };
    static constexpr float kLargeSize = 1.5f, kCareQuality = .9f;
    static constexpr int kHighQualityPoints = 54;
    static constexpr std::array<Rule, static_cast<std::size_t>(Pair::Count)> kRules{{
        {farm::CropType::None,false,0,false,false,true},
        {farm::CropType::Pumpkin,true}, {farm::CropType::Pumpkin,true},
        {farm::CropType::Carrot,false,kHighQualityPoints}, {farm::CropType::Carrot,true},
        {farm::CropType::Tomato}, {farm::CropType::Tomato,true}, {farm::CropType::Pumpkin},
        {farm::CropType::None,true}, {farm::CropType::None,true},
        {farm::CropType::None,false,kHighQualityPoints}, {farm::CropType::None,false,kHighQualityPoints},
        {farm::CropType::None,false,0,true}, {farm::CropType::None,false,0,false,true}, {}, {}
    }};
    [[nodiscard]] static bool Eligible(Pair pair, const FarmCropQualityResult& quality,
        int total, int qualityPoints) noexcept {
        const int index = static_cast<int>(pair);
        if (index < 0 || index >= static_cast<int>(kRules.size()) || !quality.IsValid() ||
            total < 0 || total > 100 || qualityPoints < 0 || qualityPoints > 60 ||
            total < qualityPoints || total > qualityPoints + 40) return false;
        const auto& rule = kRules[index];
        if (rule.crop != farm::CropType::None && rule.crop != quality.crop) return false;
        if (rule.large && (!quality.harvestSize.known || quality.harvestSize.multiplier < kLargeSize)) return false;
        if (qualityPoints < rule.qualityPoints || (rule.perfect && total != 100)) return false;
        const auto goodCare = [](float value) { return std::isfinite(value) && value >= kCareQuality && value <= 1; };
        if (rule.nutrients && (!quality.nutrientKnown || !goodCare(quality.nutrientBalance))) return false;
        if (rule.water && !goodCare(quality.waterBalance)) return false;
        return true;
    }
    [[nodiscard]] static Pair Select(const FarmCropQualityResult& quality, int total,
        int qualityPoints, Pair previous) noexcept {
        const int old = static_cast<int>(previous);
        const int start = old >= 0 && old < static_cast<int>(kRules.size()) ? old + 1 : 0;
        // A bounded rotation avoids adjacent repeats without frame-dependent randomness or allocations.
        for (int step = 0; step < static_cast<int>(kRules.size()); ++step) {
            const auto pair = static_cast<Pair>((start + step) % static_cast<int>(kRules.size()));
            if (Eligible(pair, quality, total, qualityPoints)) return pair;
        }
        return Pair::None;
    }
};
