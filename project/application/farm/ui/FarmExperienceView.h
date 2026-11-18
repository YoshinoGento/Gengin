#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmExperienceSystem.h"

namespace farmui {
inline constexpr std::array<Label, FarmExperienceSystem::kCount> kExperienceTitles{{
    Label::ExperiencePlant, Label::ExperienceHarvest, Label::ExperienceSoil, Label::ExperienceTrade,
    Label::ExperienceWater, Label::ExperienceRain, Label::ExperiencePests, Label::ExperienceRepellent,
    Label::ExperienceTime, Label::ExperienceJudging, Label::ExperienceResults, Label::ExperienceDevelopment,
    Label::ExperienceBook, Label::ExperienceCompare, Label::ExperienceRecovery, Label::ExperienceRecords,
    Label::CampaignStart, Label::FinalSubmission, Label::FinalFailure, Label::FinalMissed
}};
inline constexpr std::array<Label, FarmExperienceSystem::kCount> kExperienceGoals{{
    Label::ExperiencePlantGoal, Label::ExperienceHarvestGoal, Label::ExperienceSoilGoal, Label::ExperienceTradeGoal,
    Label::ExperienceWaterGoal, Label::ExperienceRainGoal, Label::ExperiencePestsGoal, Label::ExperienceRepellentGoal,
    Label::ExperienceTimeGoal, Label::ExperienceJudgingGoal, Label::ExperienceResultsGoal, Label::ExperienceDevelopmentGoal,
    Label::ExperienceBookGoal, Label::ExperienceCompareGoal, Label::ExperienceRecoveryGoal, Label::ExperienceRecordsGoal,
    Label::CampaignGoal, Label::FinalSubmissionGoal, Label::FinalFailureGoal, Label::FinalMissedGoal
}};
inline constexpr std::array<Label, FarmExperienceSystem::kCount> kExperienceOperations{{
    Label::ExperienceSeedTool, Label::ExperienceHarvestTool, Label::SoilCare, Label::TradeOpen,
    Label::MenuTerrain, Label::MenuTerrain, Label::ExperiencePestFocus, Label::Repellent,
    Label::TimeAdvance, Label::ContestPreview, Label::ContestDetails, Label::Development,
    Label::CropBook, Label::MenuObserve, Label::Recovery, Label::Records,
    Label::StageGuide, Label::ContestPreview, Label::ContestPreview, Label::OutcomeFinal
}};
inline constexpr std::array<Label, FarmExperienceSystem::kCount> kExperienceFirstSteps{{
    Label::ExperiencePlantStep, Label::ExperienceHarvestStep, Label::ExperienceSoilStep, Label::ExperienceTradeStep,
    Label::ExperienceWaterStep, Label::ExperienceRainStep, Label::ExperiencePestsStep, Label::ExperienceRepellentStep,
    Label::ExperienceTimeStep, Label::ExperienceJudgingStep, Label::ExperienceResultsStep, Label::ExperienceDevelopmentStep,
    Label::ExperienceBookStep, Label::ExperienceCompareStep, Label::ExperienceRecoveryStep, Label::ExperienceRecordsStep,
    Label::CampaignStep, Label::FinalSubmissionStep, Label::FinalFailureStep, Label::FinalMissedStep
}};
inline constexpr std::array<Label, FarmExperienceSystem::kCount> kExperienceObservations{{
    Label::ExperiencePlantWatch, Label::ExperienceHarvestWatch, Label::ExperienceSoilWatch, Label::ExperienceTradeWatch,
    Label::ExperienceWaterWatch, Label::ExperienceRainWatch, Label::ExperiencePestsWatch, Label::ExperienceRepellentWatch,
    Label::ExperienceTimeWatch, Label::ExperienceJudgingWatch, Label::ExperienceResultsWatch, Label::ExperienceDevelopmentWatch,
    Label::ExperienceBookWatch, Label::ExperienceCompareWatch, Label::ExperienceRecoveryWatch, Label::ExperienceRecordsWatch,
    Label::CampaignWatch, Label::FinalSubmissionWatch, Label::FinalFailureWatch, Label::FinalMissedWatch
}};
inline void BuildExperienceView(View& view, int selection, bool confirming, bool failed) {
    if (!FarmExperienceSystem::Valid(selection)) return;
    view.modal = view.menuShell = true;
    constexpr std::array tabs{Label::ExperienceCrops, Label::ExperienceEnvironment, Label::ExperienceSeason,
        Label::ExperienceSupport, Label::CampaignTab};
    const int page = selection / FarmExperienceSystem::kPageSize;
    for (int i = 0; i < FarmExperienceSystem::kPages; ++i)
        view.Add(tabs[i], {150.0f + i * 196, 52, 188, 48}, {Action::ExperiencePage, i}, !confirming, i == page);
    view.Add(Label::Experience, {170, 114, 900, 40});
    if (confirming) {
        view.Add(Label::ExperienceConfirm, {220, 222, 840, 48});
        view.Add(Label::ExperienceDiscard, {220, 286, 840, 40});
        view.Add(Label::ExperienceKeep, {220, 340, 840, 40});
        view.Add(kExperienceTitles[selection], {220, 408, 840, 44});
        view.Add(Label::ExperienceBegin, {220, 504, 400, 56}, {Action::ExperienceConfirm});
        view.Add(Label::Cancel, {652, 504, 400, 56}, {Action::ExperienceCancel});
        return;
    }
    view.Add(Label::ExperienceIntroduction, {170, 162, 940, 38});
    for (int row = 0; row < FarmExperienceSystem::kPageSize; ++row) {
        const int index = page * FarmExperienceSystem::kPageSize + row;
        view.Add(kExperienceTitles[index], {170, 232.0f + row * 78, 348, 64},
            {Action::ExperienceSelect, index}, true, index == selection);
    }
    view.Add(kExperienceTitles[selection], {550, 232, 560, 44});
    view.Add(kExperienceGoals[selection], {550, 290, 560, 38});
    view.Add(kExperienceFirstSteps[selection], {550, 340, 560, 38});
    view.Add(kExperienceObservations[selection], {550, 390, 560, 38});
    view.Add(selection == static_cast<int>(FarmExperienceSystem::Topic::CampaignStart) ? Label::CampaignFresh :
        FarmExperienceSystem::kEntries[selection].contest ? Label::ExperienceContest : Label::ExperienceFree,
        {550, 444, 560, 36});
    view.Add(Label::ExperienceBegin, {550, 494, 560, 56}, {Action::ExperienceBegin});
    if (failed) view.Add(Label::ExperienceMissing, {170, 570, 940, 38});
    view.Add(Label::Settings, {166, 636, 300, 48}, {Action::Tab, 4});
    view.Add(Label::Records, {492, 636, 300, 48}, {Action::Tab, 2});
    view.Add(Label::Resume, {818, 636, 300, 48}, {Action::Close});
}
inline void BuildExperienceHUD(View& view, int active, bool enabled) {
    view.Add(Label::Experience, {24, 138, 240, 44}, {Action::ExperienceOpen}, enabled);
    if (FarmExperienceSystem::Valid(active))
        view.Add(kExperienceOperations[active], {24, 190, 240, 44}, {Action::ExperienceAction}, enabled);
}
}
