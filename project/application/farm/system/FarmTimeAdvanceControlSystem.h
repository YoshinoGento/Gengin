#pragma once
#include "farm/system/FarmTimeAdvanceSystem.h"

// Session-only transport. Never restores the old speed automatically after a stop.
class FarmTimeAdvanceControlSystem final {
public:
    enum class Action { Start, Step, Cancel, Resume, Focus };
    struct View {
        FarmTimeAdvanceSystem::Status status{};
        bool held = false;
        bool automatic = false;
    };
    [[nodiscard]] View Read() const noexcept { return {advance_.GetStatus(), held_, automatic_}; }
    [[nodiscard]] bool HoldsSimulation() const noexcept { return held_ || consumedFrame_; }
    void BeginFrame() noexcept { consumedFrame_ = false; }
    void Reset() noexcept { advance_.Reset(); held_ = automatic_ = pendingBatch_ = false; }
    void Interrupt() noexcept { advance_.Interrupt(); automatic_ = pendingBatch_ = false; }
    void Cancel() noexcept { advance_.Cancel(); automatic_ = pendingBatch_ = false; }
    bool Start(FarmTimeAdvanceSystem::Context context, bool blocked, bool automatic) {
        if (advance_.GetStatus().active && automatic_) return false;
        held_ = true;
        if (!advance_.GetStatus().active && !advance_.Start(context, blocked)) return false;
        automatic_ = automatic;
        pendingBatch_ = true;
        return true;
    }
    int Tick(FarmTimeAdvanceSystem::Context context, bool blocked) {
        if (!advance_.GetStatus().active) return 0;
        if (blocked) {
            static_cast<void>(advance_.AdvanceBatch(context, true));
            automatic_ = pendingBatch_ = false;
            return 0;
        }
        if (consumedFrame_ || (!automatic_ && !pendingBatch_)) return 0;
        consumedFrame_ = true;
        pendingBatch_ = false;
        const int steps = advance_.AdvanceBatch(context, false);
        if (!advance_.GetStatus().active) automatic_ = false;
        return steps;
    }
private:
    FarmTimeAdvanceSystem advance_;
    bool held_ = false;
    bool automatic_ = false;
    bool pendingBatch_ = false;
    bool consumedFrame_ = false;
};
