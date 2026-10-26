#pragma once
#include "farm/system/FarmContestGoalSystem.h"

enum class FarmContestSizeAdvice { None, Sufficient, MoreSize, ImproveQuality };
struct FarmContestComparison {
    bool valid = false, staged = false, passed = false, referenceAvailable = false;
    int day = 0, target = 0, total = 0, shortfall = 0;
    FarmCropQualityResult quality{};
    FarmQualityAdvice qualityAdvice{};
    int qualityPoints = 0, sizePoints = 0;
    int grams = 0, referenceGrams = 0, additionalGrams = 0;
    float referenceSize = 0;
    FarmContestSizeAdvice sizeAdvice = FarmContestSizeAdvice::None;
};

// Immutable submitted values only. Grams are authored game units, not measured physical mass.
class FarmContestComparisonSystem final {
public:
    [[nodiscard]] static int BaseGrams(farm::CropType crop) noexcept {
        constexpr std::array<int, farm::kFarmCropTypeCount> grams{{200, 150, 150, 1500}};
        const int slot = farm::ToCropSlot(crop);
        return slot >= 0 && slot < static_cast<int>(grams.size()) ? grams[slot] : 0;
    }
    [[nodiscard]] static int ToGrams(farm::CropType crop, float size) noexcept {
        if (!std::isfinite(size) || size <= 0 || size > FarmCropSizeResult::kMaximumMultiplier) return 0;
        return static_cast<int>(std::floor(BaseGrams(crop) * static_cast<double>(size)));
    }
    // Search integer display weights with the actual V1 rounding, bounded to <=12000 g.
    [[nodiscard]] static int MinimumGrams(const FarmEconomySystem::HarvestRecord& record,
        int target, float maximum) noexcept {
        const int base = BaseGrams(record.quality.crop);
        if (!base || target < 1 || target > 100 || !std::isfinite(maximum) || maximum < .01f ||
            maximum > FarmCropSizeResult::kMaximumMultiplier) return 0;
        int low = 1, high = static_cast<int>(std::floor(base * static_cast<double>(maximum)));
        auto candidate = record;
        const auto passes = [&](int grams) {
            candidate.quality.harvestSize = {static_cast<float>(grams) / base, true};
            const auto score = FarmContestJudgeSystem::Evaluate(&candidate, kFarmContestSubmissionRulesV1);
            return score.IsValid() && score.totalPoints >= target;
        };
        if (high < low || !passes(high)) return 0;
        while (low < high) {
            const int mid = low + (high - low) / 2;
            if (passes(mid)) high = mid; else low = mid + 1;
        }
        return low;
    }
    [[nodiscard]] static FarmContestComparison Build(int index, int currentDay,
        const FarmEconomySystem::ContestResults& results, bool staged, float maximumSize) noexcept {
        FarmContestComparison out;
        if (index < 0 || index >= static_cast<int>(results.size())) return out;
        const auto summary = FarmContestSeasonSystem::Evaluate(currentDay, results);
        const auto& record = results[index];
        if (!summary.valid || summary.events[index] != FarmContestEventStatus::Submitted ||
            FarmContestEntrySystem::Evaluate(record.contestDay, &record.harvest).issue != FarmContestEntryIssue::Eligible) return out;
        const auto judge = FarmContestJudgeSystem::Evaluate(&record.harvest, kFarmContestSubmissionRulesV1);
        if (!judge.IsValid() || judge.qualityPoints != record.qualityPoints || judge.sizePoints != record.sizePoints) return out;
        out.valid = true; out.staged = staged; out.day = record.contestDay;
        out.quality = record.harvest.quality;
        out.qualityAdvice = FarmCropQualitySystem::Analyze(out.quality);
        out.qualityPoints = record.qualityPoints; out.sizePoints = record.sizePoints; out.total = judge.totalPoints;
        out.grams = ToGrams(out.quality.crop, out.quality.harvestSize.multiplier);
        if (!staged) return out;
        out.target = kFarmStageRulesV1[index].passingPoints;
        out.shortfall = (std::max)(0, out.target - out.total); out.passed = out.shortfall == 0;
        // An explicit ideal-quality scoring example, never a claim of four mandatory minima.
        auto ideal = record.harvest;
        ideal.quality.maturity = ideal.quality.waterBalance = ideal.quality.terrainFit = ideal.quality.nutrientBalance = 1;
        ideal.quality.nutrientKnown = true; ideal.quality.score = 100;
        out.referenceGrams = MinimumGrams(ideal, out.target, maximumSize);
        out.referenceAvailable = out.referenceGrams > 0;
        out.referenceSize = static_cast<float>(out.referenceGrams) / BaseGrams(out.quality.crop);
        if (out.passed) out.sizeAdvice = FarmContestSizeAdvice::Sufficient;
        else {
            const int needed = MinimumGrams(record.harvest, out.target, maximumSize);
            if (needed > out.grams) {
                out.sizeAdvice = FarmContestSizeAdvice::MoreSize; out.additionalGrams = needed - out.grams;
            } else out.sizeAdvice = FarmContestSizeAdvice::ImproveQuality;
        }
        return out;
    }
};

// Value snapshot survives inventory consumption; no pointers into farm records or GPU storage.
class FarmContestPresentationSystem final {
public:
    static constexpr float kRevealSeconds = 1.4f;
    struct Frame {
        FarmContestComparison comparison{};
        float reveal = 0;
        bool verdictVisible = false;
    };
    void Begin(const FarmContestComparison& comparison, bool replay) noexcept {
        comparison_ = comparison; active_ = comparison.valid; elapsed_ = replay ? kRevealSeconds : 0;
    }
    void Close() noexcept { active_ = false; }
    void Update(float dt) noexcept {
        if (active_ && std::isfinite(dt) && dt > 0) elapsed_ = (std::min)(kRevealSeconds, elapsed_ + (std::min)(dt, .1f));
    }
    void Reveal() noexcept { if (active_) elapsed_ = kRevealSeconds; }
    [[nodiscard]] bool Active() const noexcept { return active_; }
    [[nodiscard]] Frame GetFrame() const noexcept {
        const float t = std::clamp(elapsed_ / kRevealSeconds, 0.0f, 1.0f);
        return {comparison_, t*t*(3-2*t), active_ && elapsed_ >= kRevealSeconds};
    }
private:
    FarmContestComparison comparison_{};
    float elapsed_ = 0;
    bool active_ = false;
};
