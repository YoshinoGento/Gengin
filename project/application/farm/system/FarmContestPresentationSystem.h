#pragma once
#include "farm/system/FarmContestGoalSystem.h"
#include "farm/system/FarmContestCheerSystem.h"
#include "title/TitleCloudMotionSystem.h"

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
    enum class Cue { None, Assess, Passed, Failed, Recorded, Clear, Drumroll, Silence };
    enum class Page { Result, Quality, Size };
    static constexpr float kSpecimenEnd = 1.15f;
    static constexpr float kTotalStart = kSpecimenEnd, kVerdictTime = 2.10f;
    static constexpr float kAdviceStart = 3.10f, kRevealSeconds = 4.0f;
    static constexpr float kClearAnnouncement = 1.0f, kClearCelebrationEnd = 6.0f;
    static constexpr float kCheerFadeIn = FarmContestCheerSystem::kVoiceDelay, kCheerFadeOut = .10f;
    static constexpr float kCheerReadHold = .06f, kCheerGap = .15f;
    static constexpr float kCheerVoicePadding = FarmContestCheerSystem::kVoiceDelay + kCheerFadeOut + kCheerReadHold;
    static constexpr float kCheerLeftStart = 1.7f;
    static constexpr float kClearTitleSettleStart = 2.8f, kClearTitleSettleEnd = 3.7f;
    struct ClearTiming {
        float leftEnd = 0, rightStart = 0, rightEnd = 0, resultStart = 0, resultEnd = 0;
    };
    [[nodiscard]] static constexpr ClearTiming TimingFor(FarmContestCheerSystem::Pair pair) noexcept {
        using Cheer = FarmContestCheerSystem;
        ClearTiming timing;
        const float leftSpeechEnd = kCheerLeftStart + Cheer::kVoiceDelay + Cheer::VoiceSeconds(pair, Cheer::Side::Left);
        timing.leftEnd = leftSpeechEnd + kCheerReadHold + kCheerFadeOut;
        // Response delay is measured from the end of this line, not the longest line in the catalog.
        timing.rightStart = leftSpeechEnd + Cheer::ReplyGap(pair) - Cheer::kVoiceDelay;
        timing.rightEnd = timing.rightStart + Cheer::VoiceSeconds(pair, Cheer::Side::Right) + kCheerVoicePadding;
        timing.resultStart = (std::max)(kClearCelebrationEnd, timing.rightEnd + kCheerGap);
        timing.resultEnd = timing.resultStart + 1.f;
        return timing;
    }
    [[nodiscard]] ClearTiming ClearTimeline() const noexcept { return TimingFor(cheerPair_); }
    // Conservative cap for tests/hosts; actual completion follows the selected pair's resultEnd.
    static constexpr float kClearSeconds = (std::max)(kClearCelebrationEnd,
        kCheerLeftStart + FarmContestCheerSystem::LongestVoice(FarmContestCheerSystem::Side::Left) + .45f +
        FarmContestCheerSystem::LongestVoice(FarmContestCheerSystem::Side::Right) + kCheerVoicePadding + kCheerGap) + 1.f;
    static_assert(kClearAnnouncement < kCheerLeftStart &&
        kCheerReadHold + kCheerFadeOut + FarmContestCheerSystem::kVoiceDelay < .30f);
    static_assert(0 < kSpecimenEnd && kTotalStart < kVerdictTime &&
        kVerdictTime < kAdviceStart && kAdviceStart < kRevealSeconds);
    struct Frame {
        FarmContestComparison comparison{};
        Page page = Page::Result;
        bool explanation = false, complete = false;
        bool verdictVisible = false;
        bool active = false, unsubmitted = false;
        bool finalClear = false;
        float clearReveal = 0;
        float clearSeconds = 0;
        bool celebrationActive = false;
        float celebrationSeconds = 0;
        FarmContestCheerSystem::Frame cheer{};
        float resultReveal = 1;
        float ceremonyTime = 0;
        Vector2 cloudOffset{};
        float specimenReveal = 0, totalReveal = 0;
        float verdictReveal = 0, adviceReveal = 0;
        int displayedTotal = 0;
    };
    void Begin(const FarmContestComparison& comparison, bool replay) noexcept {
        comparison_ = comparison; active_ = comparison.valid; elapsed_ = replay ? kRevealSeconds : 0;
        unsubmitted_ = false;
        clearElapsed_ = 0;
        finalClear_ = !replay && comparison.valid && comparison.staged && comparison.passed &&
            comparison.day == FarmContestEntrySystem::kContestDays.back();
        cloudMotion_.Reset();
        page_ = Page::Result; explanation_ = false;
        pendingCue_ = finalClear_ ? Cue::Drumroll : Cue::None;
        cheerPair_ = finalClear_ ? FarmContestCheerSystem::Select(comparison.quality,
            comparison.total, comparison.qualityPoints, previousCheer_) : FarmContestCheerSystem::Pair::None;
        if (cheerPair_ != FarmContestCheerSystem::Pair::None) previousCheer_ = cheerPair_;
    }
    [[nodiscard]] bool BeginPreview(const FarmContestComparison& comparison, FarmContestCheerSystem::Pair pair) noexcept {
        if (!comparison.valid || !comparison.staged || !comparison.passed ||
            comparison.day != FarmContestEntrySystem::kContestDays.back() ||
            !FarmContestCheerSystem::Eligible(pair, comparison.quality, comparison.total, comparison.qualityPoints)) return false;
        Begin(comparison, false);
        cheerPair_ = pair;
        return true;
    }
    void BeginUnsubmitted(int day) noexcept {
        Close(); comparison_ = {}; elapsed_ = 0;
        for (const int contestDay : FarmContestEntrySystem::kContestDays)
            if (day == contestDay) { active_ = true; unsubmitted_ = true; comparison_.day = day; }
    }
    void Close() noexcept { active_ = false; unsubmitted_ = false; elapsed_ = 0; page_ = Page::Result; explanation_ = false; pendingCue_ = Cue::None; cloudMotion_.Reset(); finalClear_=false; clearElapsed_=0; cheerPair_=FarmContestCheerSystem::Pair::None; }
    [[nodiscard]] bool SelectPage(int page) noexcept {
        if (!Complete() || unsubmitted_ || page < 0 || page > static_cast<int>(Page::Size)) return false;
        page_ = static_cast<Page>(page); explanation_ = false;
        return true;
    }
    void ToggleExplanation() noexcept { if (Complete() && !unsubmitted_) explanation_ = !explanation_; }
    // Back closes the local explanation/analysis before leaving the submitted result.
    [[nodiscard]] bool Back() noexcept {
        if (!Complete()) return false;
        if (explanation_) { explanation_ = false; return true; }
        if (page_ != Page::Result) { page_ = Page::Result; return true; }
        return false;
    }
    [[nodiscard]] bool Complete() const noexcept { return active_ && (finalClear_ ? clearElapsed_ >= ClearTimeline().resultEnd : elapsed_ >= kRevealSeconds); }
    void Update(float dt) noexcept {
        if (!active_ || !std::isfinite(dt) || dt <= 0) return;
        cloudMotion_.Update((std::min)(dt,.1f));
        if (finalClear_) {
            const float previous=clearElapsed_;
            clearElapsed_=(std::min)(ClearTimeline().resultEnd,clearElapsed_+(std::min)(dt,.1f));
            if (previous<kClearAnnouncement && clearElapsed_>=kClearAnnouncement) pendingCue_=Cue::Clear;
            return;
        }
        const float previous = elapsed_;
        elapsed_ = (std::min)(kRevealSeconds, elapsed_ + (std::min)(dt, .1f));
        if (!unsubmitted_ && previous < kTotalStart && elapsed_ >= kTotalStart) pendingCue_ = Cue::Assess;
        if (previous < kVerdictTime && elapsed_ >= kVerdictTime) pendingCue_ = VerdictCue();
    }
    void Reveal() noexcept {
        if (!active_) return;
        if (finalClear_) { clearElapsed_=ClearTimeline().resultEnd; pendingCue_=Cue::Silence; return; }
        // Skip discards obsolete assessment cues, but never repeats an already presented verdict.
        if (elapsed_ < kVerdictTime) pendingCue_ = VerdictCue();
        elapsed_ = kRevealSeconds;
    }
    [[nodiscard]] Cue ConsumeCue() noexcept { const auto cue = pendingCue_; pendingCue_ = Cue::None; return cue; }
    [[nodiscard]] bool Active() const noexcept { return active_; }
    [[nodiscard]] Frame GetFrame() const noexcept {
        Frame frame;
        frame.comparison = comparison_;
        frame.active = active_; frame.unsubmitted = unsubmitted_; frame.ceremonyTime = elapsed_;
        frame.cloudOffset = cloudMotion_.Offset();
        frame.page = page_; frame.explanation = explanation_; frame.complete = Complete();
        frame.finalClear=finalClear_;
        frame.clearSeconds=clearElapsed_;
        frame.specimenReveal = Progress(0, kSpecimenEnd);
        frame.totalReveal = unsubmitted_ ? 0 : Progress(kTotalStart, kVerdictTime);
        frame.verdictVisible = active_ && elapsed_ >= kVerdictTime;
        frame.verdictReveal = Progress(kVerdictTime, kAdviceStart);
        frame.adviceReveal = Progress(kAdviceStart, kRevealSeconds);
        if (finalClear_) {
            const auto timing = ClearTimeline();
            // Celebration and results share one immutable submission and one continuous clock.
            frame.ceremonyTime=clearElapsed_;
            frame.celebrationActive=clearElapsed_>=kClearAnnouncement && clearElapsed_<kClearCelebrationEnd;
            frame.celebrationSeconds=(std::max)(0.f,clearElapsed_-kClearAnnouncement);
            frame.clearReveal=Smooth(kClearTitleSettleStart,kClearTitleSettleEnd,clearElapsed_);
            frame.resultReveal=Smooth(timing.resultStart,timing.resultEnd,clearElapsed_);
            frame.specimenReveal=Smooth(0,kSpecimenEnd,clearElapsed_);
            frame.verdictVisible=clearElapsed_>=kClearAnnouncement;
            frame.verdictReveal=Smooth(kClearAnnouncement,kClearAnnouncement+.45f,clearElapsed_);
            frame.adviceReveal=Smooth(kClearAnnouncement,kClearAnnouncement+.55f,clearElapsed_);
            if (cheerPair_ != FarmContestCheerSystem::Pair::None) {
                frame.cheer.ducking = Smooth(kCheerLeftStart,kCheerLeftStart+kCheerFadeIn,clearElapsed_) *
                    (1 - Smooth(timing.rightEnd-kCheerFadeOut,timing.rightEnd,clearElapsed_));
                const bool left = clearElapsed_ >= kCheerLeftStart && clearElapsed_ < timing.leftEnd;
                const bool right = clearElapsed_ >= timing.rightStart && clearElapsed_ < timing.rightEnd;
                if (left || right) {
                    const float start = left ? kCheerLeftStart : timing.rightStart;
                    const float end = left ? timing.leftEnd : timing.rightEnd;
                    frame.cheer.pair = cheerPair_;
                    frame.cheer.side = left ? FarmContestCheerSystem::Side::Left : FarmContestCheerSystem::Side::Right;
                    frame.cheer.entrance = Smooth(start, start + kCheerFadeIn, clearElapsed_);
                    frame.cheer.opacity = frame.cheer.entrance * (1 - Smooth(end - kCheerFadeOut, end, clearElapsed_));
                    frame.cheer.voiceReady = clearElapsed_ >= start + FarmContestCheerSystem::kVoiceDelay;
                }
            }
            // Fade the result panel, not the immutable judged score or its gauge.
            frame.totalReveal=1;
        }
        frame.displayedTotal = static_cast<int>(comparison_.total * frame.totalReveal);
        return frame;
    }
private:
    [[nodiscard]] static float Smooth(float begin, float end, float seconds) noexcept {
        const float t=std::clamp((seconds-begin)/(end-begin),0.f,1.f);
        return t*t*(3-2*t);
    }
    [[nodiscard]] float Progress(float begin, float end) const noexcept {
        if (!active_) return 0;
        return Smooth(begin,end,elapsed_);
    }
    [[nodiscard]] Cue VerdictCue() const noexcept {
        return unsubmitted_ || !comparison_.staged ? Cue::Recorded : comparison_.passed ? Cue::Passed : Cue::Failed;
    }
    FarmContestComparison comparison_{};
    title::TitleCloudMotionSystem cloudMotion_;
    float elapsed_ = 0;
    bool active_ = false;
    bool unsubmitted_ = false;
    bool finalClear_ = false;
    float clearElapsed_ = 0;
    FarmContestCheerSystem::Pair cheerPair_ = FarmContestCheerSystem::Pair::None;
    FarmContestCheerSystem::Pair previousCheer_ = FarmContestCheerSystem::Pair::None;
    Page page_ = Page::Result;
    bool explanation_ = false;
    Cue pendingCue_ = Cue::None;
};
