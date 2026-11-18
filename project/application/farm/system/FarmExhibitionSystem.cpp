#include "farm/system/FarmExhibitionSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "io/JsonFile.h"
#include <filesystem>

void FarmExhibitionSystem::Initialize(const std::string& directory, const std::string& startup) {
    Close(); selected_ = 0; records_ = {}; showcases_ = {}; startup_ = false; repeatFirstCheer_ = false; clearGallery_ = false;
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
        repeatFirstCheer_ = startup_ && config.is_object() && config.contains("repeatFirstCheer") &&
            config["repeatFirstCheer"].is_boolean() && config["repeatFirstCheer"].get<bool>();
        clearGallery_ = startup_ && config.is_object() && config.contains("clearGallery") &&
            config["clearGallery"].is_boolean() && config["clearGallery"].get<bool>();
    }
    if (clearGallery_)
        for (int i=0; i<CaseCount(); ++i) showcases_[i] = FarmContestShowcaseSystem::Build(i);
    if (startup_) Open();
}
bool FarmExhibitionSystem::Available(int index) const noexcept {
    if (index < 0 || index >= CaseCount()) return false;
    return clearGallery_ ? showcases_[index].valid : records_[index].valid;
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
    if (!Available(selected_)) return farm::CropType::None;
    return clearGallery_ ? showcases_[selected_].quality.crop : records_[selected_].results.back().harvest.quality.crop;
}
bool FarmExhibitionSystem::Begin(float maximumSize) noexcept {
    if (!Active() || !Available(selected_) || !Complete()) return false;
    if (clearGallery_) {
        if (!presentation_.BeginPreview(showcases_[selected_], static_cast<FarmContestCheerSystem::Pair>(selected_))) return false;
        phase_ = Phase::Result; beginning_ = true; return true;
    }
    const auto& record = records_[selected_];
    if (selected_ == static_cast<int>(Case::Missed)) {
        presentation_.Close(); outcome_.Reset();
        outcome_.Observe(record.day, record.results, true, true);
        presentation_.BeginUnsubmitted(FarmContestEntrySystem::kContestDays.back());
        phase_ = Phase::Missed;
    } else {
        const auto comparison = FarmContestComparisonSystem::Build(2, record.day, record.results, true, maximumSize);
        if (!comparison.valid) return false;
        // Only the opt-in audition copy repeats its first eligible pair; normal play keeps rotation.
        if (repeatFirstCheer_) presentation_ = {};
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
