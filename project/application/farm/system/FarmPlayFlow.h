#pragma once

// Transient session navigation only. Money/clear/save state belongs to existing Systems.
class FarmPlayFlow final {
public:
    enum class Phase { Briefing, Starting, Playing, Result, Review };
    FarmPlayFlow() noexcept;
    void Reset() noexcept;
    void RequestTitle() noexcept { returnTitle_ = true; }
    [[nodiscard]] bool RequestsTitle() const noexcept { return returnTitle_; }
    void Observe(bool cleared) noexcept;
    void Continue() noexcept;
    void BeginStart() noexcept;
    void FinishStart() noexcept;
    [[nodiscard]] Phase GetPhase() const noexcept;
    [[nodiscard]] bool BlocksSimulation() const noexcept;
private:
    struct State;
    enum class Event { Cleared, NotCleared, Continue, BeginStart, FinishStart };
    void Dispatch(Event event) noexcept;
    const State* state_; // Borrows an immutable state for the lifetime of the process.
    bool returnTitle_ = false;
};
