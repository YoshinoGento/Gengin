#include "farm/system/FarmPlayFlow.h"

struct FarmPlayFlow::State {
    struct Briefing;
    struct Starting;
    struct Playing;
    struct Result;
    struct Review;

    constexpr State(Phase id, bool blocks) noexcept : phase(id), blocksSimulation(blocks) {}
    virtual ~State() = default;
    virtual const State& Next(Event event) const noexcept = 0;
    template<class T> static const State& Get() noexcept {
        static const T instance;
        return instance;
    }
    const Phase phase;
    const bool blocksSimulation;
};

struct FarmPlayFlow::State::Briefing final : State {
    Briefing() noexcept : State(Phase::Briefing, true) {}
    const State& Next(Event event) const noexcept override {
        switch (event) {
        case Event::Cleared: return Get<Result>();
        case Event::Continue: return Get<Playing>();
        case Event::BeginStart: return Get<Starting>();
        default: return *this;
        }
    }
};
struct FarmPlayFlow::State::Starting final : State {
    Starting() noexcept : State(Phase::Starting, true) {}
    const State& Next(Event event) const noexcept override {
        return event == Event::FinishStart ? Get<Playing>() : *this;
    }
};
struct FarmPlayFlow::State::Playing final : State {
    Playing() noexcept : State(Phase::Playing, false) {}
    const State& Next(Event event) const noexcept override {
        return event == Event::Cleared ? Get<Result>() : *this;
    }
};
struct FarmPlayFlow::State::Result final : State {
    Result() noexcept : State(Phase::Result, true) {}
    const State& Next(Event event) const noexcept override {
        if (event == Event::NotCleared) return Get<Briefing>();
        if (event == Event::Continue) return Get<Review>();
        return *this;
    }
};
struct FarmPlayFlow::State::Review final : State {
    Review() noexcept : State(Phase::Review, false) {}
    const State& Next(Event event) const noexcept override {
        return event == Event::NotCleared ? Get<Briefing>() : *this;
    }
};

FarmPlayFlow::FarmPlayFlow() noexcept : state_(&State::Get<State::Briefing>()) {}
void FarmPlayFlow::Reset() noexcept {
    state_ = &State::Get<State::Briefing>();
    returnTitle_ = false;
}
void FarmPlayFlow::Dispatch(Event event) noexcept { state_ = &state_->Next(event); }
void FarmPlayFlow::Observe(bool cleared) noexcept { Dispatch(cleared ? Event::Cleared : Event::NotCleared); }
void FarmPlayFlow::Continue() noexcept { Dispatch(Event::Continue); }
void FarmPlayFlow::BeginStart() noexcept { Dispatch(Event::BeginStart); }
void FarmPlayFlow::FinishStart() noexcept { Dispatch(Event::FinishStart); }
FarmPlayFlow::Phase FarmPlayFlow::GetPhase() const noexcept { return state_->phase; }
bool FarmPlayFlow::BlocksSimulation() const noexcept { return returnTitle_ || state_->blocksSimulation; }
