#pragma once
#include "farm/system/FarmContestPresentationSystem.h"
#include "farm/system/FarmContestOutcomeSystem.h"
#include <string>

// Presentation-only copies: this System never owns or mutates the active farm.
class FarmExhibitionSystem final {
public:
    enum class Phase { Closed, Menu, Result, Missed };
    enum class Case { Failed, Passed, Missed };
    static constexpr int kCaseCount = 3;
    void Initialize(const std::string& directory = "Resources/exhibition",
        const std::string& startup = "Settings/runtime/contest_exhibition.json");
    void Open() noexcept;
    void Close() noexcept;
    void Home() noexcept;
    bool Select(int index) noexcept;
    bool Begin(float maximumSize) noexcept;
    void Update(float dt) noexcept;
    void Reveal() noexcept;
    void Back() noexcept;
    bool SelectPage(int page) noexcept { return presentation_.SelectPage(page); }
    void ToggleExplanation() noexcept { presentation_.ToggleExplanation(); }
    [[nodiscard]] bool Active() const noexcept { return phase_ != Phase::Closed; }
    [[nodiscard]] bool Startup() const noexcept { return startup_; }
    [[nodiscard]] Phase GetPhase() const noexcept { return phase_; }
    [[nodiscard]] int Selection() const noexcept { return selected_; }
    [[nodiscard]] bool Available(int index) const noexcept;
    [[nodiscard]] bool Complete() const noexcept;
    [[nodiscard]] bool ConsumeBeginning() noexcept;
    [[nodiscard]] FarmContestPresentationSystem::Cue ConsumeCue() noexcept;
    [[nodiscard]] FarmContestPresentationSystem::Frame ContestFrame() const noexcept { return presentation_.GetFrame(); }
    [[nodiscard]] const FarmContestOutcomeSystem::Frame& OutcomeFrame() const noexcept { return outcome_.GetFrame(); }
    [[nodiscard]] farm::CropType Crop() const noexcept;
private:
    struct Record { FarmEconomySystem::ContestResults results{}; int day = 0; bool valid = false; };
    std::array<Record, kCaseCount> records_{};
    FarmContestPresentationSystem presentation_;
    FarmContestOutcomeSystem outcome_;
    Phase phase_ = Phase::Closed;
    int selected_ = 0;
    bool startup_ = false, beginning_ = false;
};
