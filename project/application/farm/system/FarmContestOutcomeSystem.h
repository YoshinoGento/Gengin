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
    };
    inline static constexpr float kRevealSeconds = .85f;
    void Reset() noexcept { frame_ = {}; acknowledged_.fill(false); lastDay_ = 0; elapsed_ = 0; }
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
        if (wasFinal != season.finalized || oldMissed != frame_.missedDay) elapsed_ = 0;
        lastDay_ = day;
        Update(0);
    }
    void Update(float dt) noexcept {
        if (std::isfinite(dt) && dt > 0) elapsed_ = (std::min)(kRevealSeconds, elapsed_ + (std::min)(dt, .1f));
        const float t = elapsed_ / kRevealSeconds;
        frame_.reveal = t * t * (3 - 2 * t);
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
    Frame frame_{};
    std::array<bool, 3> acknowledged_{};
    int lastDay_ = 0;
    float elapsed_ = 0;
};
