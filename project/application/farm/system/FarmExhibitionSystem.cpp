#include "farm/system/FarmExhibitionSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "io/JsonFile.h"
#include <filesystem>

void FarmExhibitionSystem::Initialize(const std::string& directory, const std::string& startup) {
    Close(); selected_ = 0; records_ = {}; startup_ = false;
    constexpr std::array<const char*, kCaseCount> names{{"failed.json", "passed.json", "missed.json"}};
    constexpr std::array<FarmContestGoalStatus, kCaseCount> expected{{
        FarmContestGoalStatus::Failed, FarmContestGoalStatus::Passed, FarmContestGoalStatus::Missed}};
    for (std::size_t i = 0; i < records_.size(); ++i) {
        auto& record = records_[i];
        record.valid = FarmDocumentSystem::ReadExhibitionResults(
            (std::filesystem::path(directory) / names[i]).string(), record.results, record.day);
        if (record.valid) record.valid = FarmContestGoalSystem::Evaluate(record.day, record.results, true).back().status == expected[i];
    }
    if (JsonFile::Exists(startup)) {
        // A damaged exhibition config stays in a safe menu, never falls through into farming.
        nlohmann::json config;
        startup_ = !JsonFile::Load(startup, config) || !config.is_object() ||
            !config.contains("enabled") || !config["enabled"].is_boolean() || config["enabled"].get<bool>();
    }
    if (startup_) Open();
}
bool FarmExhibitionSystem::Available(int index) const noexcept {
    return index >= 0 && index < kCaseCount && records_[index].valid;
}
void FarmExhibitionSystem::Open() noexcept { Home(); }
void FarmExhibitionSystem::Home() noexcept {
    presentation_.Close(); outcome_.Reset(); beginning_ = false; phase_ = Phase::Menu;
}
void FarmExhibitionSystem::Close() noexcept { Home(); phase_ = Phase::Closed; }
bool FarmExhibitionSystem::Select(int index) noexcept {
    if (phase_ != Phase::Menu || !Available(index)) return false;
    selected_ = index; return true;
}
farm::CropType FarmExhibitionSystem::Crop() const noexcept {
    return Available(selected_) ? records_[selected_].results.back().harvest.quality.crop : farm::CropType::None;
}
bool FarmExhibitionSystem::Begin(float maximumSize) noexcept {
    if (!Active() || !Available(selected_) || !Complete()) return false;
    const auto& record = records_[selected_];
    if (selected_ == static_cast<int>(Case::Missed)) {
        presentation_.Close(); outcome_.Reset();
        outcome_.Observe(record.day, record.results, true, true);
        presentation_.BeginUnsubmitted(FarmContestEntrySystem::kContestDays.back());
        phase_ = Phase::Missed;
    } else {
        const auto comparison = FarmContestComparisonSystem::Build(2, record.day, record.results, true, maximumSize);
        if (!comparison.valid) return false;
        presentation_.Begin(comparison, false); phase_ = Phase::Result;
    }
    beginning_ = true; return true;
}
void FarmExhibitionSystem::Update(float dt) noexcept {
    if (phase_ == Phase::Result || phase_ == Phase::Missed) presentation_.Update(dt);
}
bool FarmExhibitionSystem::Complete() const noexcept {
    return phase_ == Phase::Result || phase_ == Phase::Missed ? presentation_.Complete() : true;
}
void FarmExhibitionSystem::Reveal() noexcept {
    if (phase_ == Phase::Result || phase_ == Phase::Missed) presentation_.Reveal();
}
void FarmExhibitionSystem::Back() noexcept {
    if (!Complete()) { Reveal(); return; }
    if (phase_ == Phase::Result && presentation_.Back()) return;
    if (phase_ != Phase::Menu) Home();
}
bool FarmExhibitionSystem::ConsumeBeginning() noexcept {
    const bool result = beginning_; beginning_ = false; return result;
}
FarmContestPresentationSystem::Cue FarmExhibitionSystem::ConsumeCue() noexcept {
    return presentation_.ConsumeCue();
}
