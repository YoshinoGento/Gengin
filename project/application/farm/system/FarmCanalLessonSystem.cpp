#include "farm/system/FarmCanalLessonSystem.h"
#include <cmath>

namespace farm {
struct FarmCanalLessonSystem::LessonState {
    struct Choose;
    struct Previewing;
    struct Flowing;
    struct Reached;
    explicit constexpr LessonState(Phase id) noexcept : phase(id) {}
    virtual ~LessonState() = default;
    virtual Response Handle(FarmCanalLessonSystem& lesson, Event event) const = 0;
    static Response Unhandled(Event event) {
        return event.action == Action::Back ? Response::Exit : Response::Ignored;
    }
    template<class T> static const LessonState& Get() {
        static const T instance;
        return instance;
    }
    const Phase phase;
};

struct FarmCanalLessonSystem::LessonState::Choose final : LessonState {
    Choose() : LessonState(Phase::Choose) {}
    Response Handle(FarmCanalLessonSystem& lesson, Event event) const override {
        if (event.action != Action::Preview && event.action != Action::Primary) return Unhandled(event);
        if (lesson.BuildCandidate(event.index)) {
            lesson.state_ = &Get<Previewing>();
            return Response::Handled;
        }
        return event.action == Action::Primary ? Response::Handled : Response::Ignored;
    }
};
struct FarmCanalLessonSystem::LessonState::Previewing final : LessonState {
    Previewing() : LessonState(Phase::Preview) {}
    Response Handle(FarmCanalLessonSystem& lesson, Event event) const override {
        if (event.action == Action::Connect || (event.action == Action::Primary && lesson.WillReach())) {
            if (lesson.CommitCandidate()) {
                lesson.state_ = &Get<Flowing>();
                return Response::Handled;
            }
            return event.action == Action::Primary ? Response::Handled : Response::Ignored;
        }
        switch (event.action) {
        case Action::Preview:
        case Action::Cancel:
        case Action::Back:
        case Action::Primary:
            lesson.preview_.Cancel();
            lesson.rejected_ = false;
            lesson.state_ = &Get<Choose>();
            return event.action == Action::Preview ? lesson.state_->Handle(lesson,event) : Response::Handled;
        default: return Unhandled(event);
        }
    }
};
struct FarmCanalLessonSystem::LessonState::Flowing final : LessonState {
    Flowing() : LessonState(Phase::Flowing) {}
    Response Handle(FarmCanalLessonSystem& lesson, Event event) const override {
        if (event.action != Action::Tick) return Unhandled(event);
        if (!std::isfinite(event.seconds) || event.seconds <= 0) return Response::Ignored;
        lesson.water_.UpdateWater(lesson.grid_,event.seconds,1.0f);
        if (lesson.Moisture() >= lesson.before_ + kNoticeableGain) lesson.state_ = &Get<Reached>();
        return Response::Handled;
    }
};
struct FarmCanalLessonSystem::LessonState::Reached final : LessonState {
    Reached() : LessonState(Phase::Reached) {}
    Response Handle(FarmCanalLessonSystem& lesson, Event event) const override {
        if (event.action != Action::TrySecond && event.action != Action::Primary) return Unhandled(event);
        if (lesson.target_ != kFirst) return event.action == Action::Primary ? Response::Exit : Response::Ignored;
        lesson.target_ = kSecond;
        lesson.grid_.SetSelectedIndex(kSecond);
        lesson.rejected_ = false;
        lesson.state_ = &Get<Choose>();
        return Response::Handled;
    }
};

FarmCanalLessonSystem::FarmCanalLessonSystem() : state_(&LessonState::Get<LessonState::Choose>()) {}
bool FarmCanalLessonSystem::Begin() {
    preview_.Initialize(); water_.Initialize(); tools_.Initialize();
    state_ = &LessonState::Get<LessonState::Choose>();
    target_ = kFirst; rejected_ = false; before_ = kDry;
    if (!grid_.Initialize(kWidth,kHeight)) return false;
    // The first crop is reachable by filling the gap; the second needs an extension.
    FarmTile channel{};
    channel.feature = FarmTileFeature::WaterSource; channel.waterAmount = 1;
    if (!grid_.SetTile(kGap-2,channel)) return false;
    channel.feature = FarmTileFeature::Canal;
    if (!grid_.SetTile(kGap-1,channel)) return false;
    channel.waterAmount = 0;
    if (!grid_.SetTile(kGap+1,channel)) return false;
    FarmTile plant{};
    plant.state = FarmTileState::Planted; plant.crop = CropType::Tomato;
    plant.growth = 0.55f; plant.moisture = kDry;
    if (!grid_.SetTile(kFirst,plant) || !grid_.SetTile(kSecond,plant)) return false;
    water_.Rebuild(grid_);
    return grid_.SetSelectedIndex(kGap);
}
bool FarmCanalLessonSystem::BuildCandidate(int index) {
    rejected_ = true;
    const auto* tile = grid_.GetTile(index);
    if (!tile || tile->feature != FarmTileFeature::None || tile->state != FarmTileState::Empty ||
        (target_ == kFirst && index != kGap)) return false;
    grid_.SetSelectedIndex(index);
    if (!preview_.Begin(grid_,index,FarmIrrigationPreviewOperation::ToggleCanal)) return false;
    rejected_ = false;
    return true;
}
bool FarmCanalLessonSystem::WillReach() const {
    const auto* prediction = preview_.GetPreviewIrrigation();
    return prediction && prediction->IsInIrrigationRange(target_);
}
bool FarmCanalLessonSystem::CommitCandidate() {
    if (!WillReach() || !preview_.CanConfirm(grid_) || !tools_.ToggleSelectedCanal(grid_)) return false;
    before_ = Moisture();
    preview_.Cancel(); water_.Rebuild(grid_); grid_.SetSelectedIndex(target_);
    return true;
}
FarmCanalLessonSystem::Phase FarmCanalLessonSystem::State() const noexcept { return state_->phase; }
bool FarmCanalLessonSystem::Preview(int index) { return state_->Handle(*this,{Action::Preview,index}) == Response::Handled; }
bool FarmCanalLessonSystem::Connect() { return state_->Handle(*this,{Action::Connect}) == Response::Handled; }
bool FarmCanalLessonSystem::TrySecond() { return state_->Handle(*this,{Action::TrySecond}) == Response::Handled; }
void FarmCanalLessonSystem::Cancel() { state_->Handle(*this,{Action::Cancel}); rejected_ = false; }
void FarmCanalLessonSystem::Tick(float seconds) { state_->Handle(*this,{Action::Tick,-1,seconds}); }
FarmCanalLessonSystem::Response FarmCanalLessonSystem::Primary() { return state_->Handle(*this,{Action::Primary,Selection()}); }
FarmCanalLessonSystem::Response FarmCanalLessonSystem::Back() { return state_->Handle(*this,{Action::Back}); }
void FarmCanalLessonSystem::Move(int dx, int dy) { if (State() == Phase::Choose) grid_.MoveSelection(dx,dy); }
float FarmCanalLessonSystem::Moisture() const {
    const auto* tile = grid_.GetTile(target_);
    return tile ? tile->moisture : 0.0f;
}
}
