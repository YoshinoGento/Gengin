#pragma once
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmToolActionSystem.h"

namespace farm {
// Practice owns its board and command history; no campaign objects are borrowed.
class FarmCanalLessonSystem final {
public:
    enum class Phase { Choose, Preview, Flowing, Reached };
    enum class Response { Ignored, Handled, Exit };
    struct Frame { Phase phase; bool first, rejected, reaches; float before, moisture; };
    static constexpr int kWidth = 5, kHeight = 4, kGap = 12, kFirst = 8, kSecond = 9;
    static constexpr float kDry = 0.08f, kNoticeableGain = 0.04f;

    FarmCanalLessonSystem();
    bool Begin();
    bool Preview(int index);
    bool WillReach() const;
    void Cancel();
    bool Connect();
    void Tick(float seconds);
    bool TrySecond();
    Response Primary();
    Response Back();
    void Move(int dx, int dy);
    int Selection() const { return grid_.GetSelectedIndex(); }
    int Target() const { return target_; }
    Phase State() const noexcept;
    bool Rejected() const { return rejected_; }
    float Moisture() const;
    float Before() const { return before_; }
    Frame Read() const { return {State(),target_ == kFirst,rejected_,WillReach(),before_,Moisture()}; }
    const FarmGrid& Board() const { return grid_; }
    const FarmIrrigationSystem& Water() const { return water_; }
    const FarmIrrigationPreviewSystem& Prediction() const { return preview_; }
private:
    struct LessonState;
    enum class Action { Preview, Cancel, Connect, Tick, TrySecond, Primary, Back };
    struct Event { Action action; int index = -1; float seconds = 0; };
    bool BuildCandidate(int index);
    bool CommitCandidate();
    FarmGrid grid_;
    FarmIrrigationSystem water_;
    FarmIrrigationPreviewSystem preview_;
    FarmToolActionSystem tools_;
    const LessonState* state_; // Static, immutable; board and progress live in this session.
    int target_ = kFirst;
    float before_ = kDry;
    bool rejected_ = false;
};
}
