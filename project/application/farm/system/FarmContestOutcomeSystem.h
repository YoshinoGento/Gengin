#pragma once
#include "farm/system/FarmContestGoalSystem.h"
#include <algorithm>
#include <cmath>

// Session-only acknowledgement and timing; authoritative scores remain in the economy.
class FarmContestOutcomeSystem final {
public:
    struct Frame {
        FarmContestSeasonSummary season{};
        FarmContestGoals goals{};
        int missedDay = 0;
        float reveal = 0;
        float verdictReveal = 0;
        std::array<float, 3> rowReveal{};
        float summaryReveal = 0;
        float controlsReveal = 0;
    };
    inline static constexpr float kRevealSeconds = .85f;
    inline static constexpr float kSeasonRevealSeconds = 1.8f;
    inline static constexpr float kHeaderEnd = .3f;
    inline static constexpr float kVerdictStart = .15f, kVerdictEnd = .5f;
    inline static constexpr float kFirstRowStart = .45f, kRowInterval = .18f, kRowDuration = .42f;
    inline static constexpr float kSummaryStart = 1.15f, kSummaryEnd = 1.5f;
    void Reset() noexcept {
        frame_ = {}; acknowledged_.fill(false); lastDay_ = 0; elapsed_ = 0; openingConsumed_ = false;
    }
    void Observe(int day, const FarmEconomySystem::ContestResults& results, bool enabled, bool staged) noexcept {
        if (!enabled || day < lastDay_) Reset();
        if (!enabled) return;
        const auto season = FarmContestSeasonSystem::Evaluate(day, results);
        if (!season.valid) { Reset(); return; }
        const bool wasFinal = frame_.season.finalized;
        const int oldMissed = frame_.missedDay;
        frame_.season = season;
        frame_.goals = FarmContestGoalSystem::Evaluate(day, results, staged);
        frame_.missedDay = 0;
        // A loaded later-day farm gets the latest missed deadline, not a backlog of popups.
        if (!season.finalized) {
            for (std::size_t i = 0; i < season.events.size(); ++i)
                if (season.events[i] == FarmContestEventStatus::Missed && !acknowledged_[i])
                    frame_.missedDay = FarmContestEntrySystem::kContestDays[i];
        }
        if (wasFinal != season.finalized || oldMissed != frame_.missedDay) {
            elapsed_ = 0; openingConsumed_ = false;
        }
        lastDay_ = day;
        Update(0);
    }
    void Update(float dt) noexcept {
        const float duration = frame_.season.finalized ? kSeasonRevealSeconds : kRevealSeconds;
        if (std::isfinite(dt) && dt > 0) elapsed_ = (std::min)(duration, elapsed_ + (std::min)(dt, .1f));
        frame_.reveal = Phase(0, frame_.season.finalized ? kHeaderEnd : kRevealSeconds);
        frame_.verdictReveal = Phase(kVerdictStart, kVerdictEnd);
        for (std::size_t i = 0; i < frame_.rowReveal.size(); ++i) {
            const float start = kFirstRowStart + static_cast<float>(i) * kRowInterval;
            frame_.rowReveal[i] = Phase(start, start + kRowDuration);
        }
        frame_.summaryReveal = Phase(kSummaryStart, kSummaryEnd);
        frame_.controlsReveal = Phase(kSummaryEnd, kSeasonRevealSeconds);
    }
    void Reveal() noexcept {
        if (!frame_.season.finalized) return;
        elapsed_ = kSeasonRevealSeconds;
        Update(0);
    }
    // Consume only when the notice is actually visible, not while another modal covers it.
    [[nodiscard]] bool ConsumeOpening() noexcept {
        if (openingConsumed_ || (!frame_.season.finalized && !frame_.missedDay)) return false;
        openingConsumed_ = true;
        return true;
    }
    bool AcknowledgeMissed(int expectedDay) noexcept {
        if (expectedDay == 0 || expectedDay != frame_.missedDay) return false;
        for (std::size_t i = 0; i < acknowledged_.size(); ++i)
            if (FarmContestEntrySystem::kContestDays[i] <= expectedDay) acknowledged_[i] = true;
        frame_.missedDay = 0;
        return true;
    }
    [[nodiscard]] const Frame& GetFrame() const noexcept { return frame_; }
private:
    [[nodiscard]] float Phase(float start, float end) const noexcept {
        const float t = std::clamp((elapsed_ - start) / (end - start), 0.0f, 1.0f);
        return t * t * (3 - 2 * t);
    }
    Frame frame_{};
    std::array<bool, 3> acknowledged_{};
    int lastDay_ = 0;
    float elapsed_ = 0;
    bool openingConsumed_ = false;
};
