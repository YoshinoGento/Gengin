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
    enum class Cue { None, Assess, Passed, Failed, Recorded };
    enum class Page { Result, Quality, Size };
    static constexpr float kSpecimenEnd = .30f;
    static constexpr float kTotalStart = kSpecimenEnd, kVerdictTime = .90f;
    static constexpr float kAdviceStart = 1.15f, kRevealSeconds = 1.50f;
    static_assert(0 < kSpecimenEnd && kTotalStart < kVerdictTime &&
        kVerdictTime < kAdviceStart && kAdviceStart < kRevealSeconds);
    struct Frame {
        FarmContestComparison comparison{};
        Page page = Page::Result;
        bool explanation = false, complete = false;
        bool verdictVisible = false;
        float specimenReveal = 0, totalReveal = 0;
        float verdictReveal = 0, adviceReveal = 0;
        int displayedTotal = 0;
    };
    void Begin(const FarmContestComparison& comparison, bool replay) noexcept {
        comparison_ = comparison; active_ = comparison.valid; elapsed_ = replay ? kRevealSeconds : 0;
        page_ = Page::Result; explanation_ = false;
        pendingCue_ = Cue::None;
    }
    void Close() noexcept { active_ = false; page_ = Page::Result; explanation_ = false; pendingCue_ = Cue::None; }
    [[nodiscard]] bool SelectPage(int page) noexcept {
        if (!Complete() || page < 0 || page > static_cast<int>(Page::Size)) return false;
        page_ = static_cast<Page>(page); explanation_ = false;
        return true;
    }
    void ToggleExplanation() noexcept { if (Complete()) explanation_ = !explanation_; }
    // Back closes the local explanation/analysis before leaving the submitted result.
    [[nodiscard]] bool Back() noexcept {
        if (!Complete()) return false;
        if (explanation_) { explanation_ = false; return true; }
        if (page_ != Page::Result) { page_ = Page::Result; return true; }
        return false;
    }
    [[nodiscard]] bool Complete() const noexcept { return active_ && elapsed_ >= kRevealSeconds; }
    void Update(float dt) noexcept {
        if (!active_ || !std::isfinite(dt) || dt <= 0) return;
        const float previous = elapsed_;
        elapsed_ = (std::min)(kRevealSeconds, elapsed_ + (std::min)(dt, .1f));
        if (previous < kTotalStart && elapsed_ >= kTotalStart) pendingCue_ = Cue::Assess;
        if (previous < kVerdictTime && elapsed_ >= kVerdictTime) pendingCue_ = VerdictCue();
    }
    void Reveal() noexcept {
        if (!active_) return;
        // Skip discards obsolete assessment cues, but never repeats an already presented verdict.
        if (elapsed_ < kVerdictTime) pendingCue_ = VerdictCue();
        elapsed_ = kRevealSeconds;
    }
    [[nodiscard]] Cue ConsumeCue() noexcept { const auto cue = pendingCue_; pendingCue_ = Cue::None; return cue; }
    [[nodiscard]] bool Active() const noexcept { return active_; }
    [[nodiscard]] Frame GetFrame() const noexcept {
        Frame frame;
        frame.comparison = comparison_;
        frame.page = page_; frame.explanation = explanation_; frame.complete = Complete();
        frame.specimenReveal = Progress(0, kSpecimenEnd);
        frame.totalReveal = Progress(kTotalStart, kVerdictTime);
        frame.verdictVisible = active_ && elapsed_ >= kVerdictTime;
        frame.verdictReveal = Progress(kVerdictTime, kAdviceStart);
        frame.adviceReveal = Progress(kAdviceStart, kRevealSeconds);
        frame.displayedTotal = static_cast<int>(comparison_.total * frame.totalReveal);
        return frame;
    }
private:
    [[nodiscard]] float Progress(float begin, float end) const noexcept {
        if (!active_) return 0;
        const float t = std::clamp((elapsed_ - begin) / (end - begin), 0.0f, 1.0f);
        return t*t*(3-2*t);
    }
    [[nodiscard]] Cue VerdictCue() const noexcept {
        return !comparison_.staged ? Cue::Recorded : comparison_.passed ? Cue::Passed : Cue::Failed;
    }
    FarmContestComparison comparison_{};
    float elapsed_ = 0;
    bool active_ = false;
    Page page_ = Page::Result;
    bool explanation_ = false;
    Cue pendingCue_ = Cue::None;
};
