#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmHUD.h"
#include "farm/system/FarmPlayFlow.h"
#include "farm/system/FarmStartPresentationSystem.h"
#include <cstdio>

namespace farmui {
inline void BuildFarmStartView(View& view, const FarmStartPresentationSystem::Frame& frame, const FarmHUDViewData& data) {
    view.modal = view.starting = true;
    view.cinematicBars = frame.bars;
    view.cinematicCover = frame.cover;
    using Subject = FarmStartPresentationSystem::Subject;
    if (!frame.arrival) view.Add(frame.subject == Subject::Source ? Label::StartSource :
        frame.subject == Subject::Canal ? Label::StartCanal : Label::StartFarm, {40, 22, 760, 42});
    if (!frame.arrival || frame.showGoal) view.Metric(Label::TrialDay,
        frame.arrival ? Rect{490, 130, 332, 42} : Rect{900, 22, 332, 42}, 170, std::to_string(data.day));
    if (frame.showGoal) {
        if (data.freeFarming || data.contestSeason)
            view.Add(data.freeFarming ? Label::FreeGoal : Label::SeasonGoal, {40, 658, 960, 42});
        else view.Metric(Label::TrialGoal, {40, 658, 960, 42}, 650, std::to_string(data.goalMoney) + "G");
    }
    view.Add(Label::StartSkip, {1024, 652, 216, 48}, {Action::StartSkip}, frame.canSkip);
}
inline void BuildPlayFlowView(View& view, FarmPlayFlow::Phase phase, const FarmHUDViewData& data, bool staged = false, bool mainGameOnly = false, bool canalLesson = false) {
    view.modal = true;
    const bool result = phase == FarmPlayFlow::Phase::Result;
    if (phase == FarmPlayFlow::Phase::Briefing) {
        view.briefing = true;
        if (mainGameOnly) {
            view.Add(Label::SeasonMode, {40,28,760,44});
            view.Metric(Label::TrialDay, {900,28,332,44}, 170, std::to_string(data.day));
            view.Add(Label::MainPurpose, {40,506,1184,42});
            view.Add(canalLesson ? Label::LessonIntro : data.day == 1 ? Label::MainFirstPlot : Label::CampaignGoal, {40,553,1184,42});
            view.Add(canalLesson ? Label::LessonBegin : Label::MainEnter, {40,630,570,50}, {Action::FlowContinue});
            view.Add(Label::Records, {630,630,602,50}, {Action::FlowRecords});
            return;
        }
        view.Add(data.freeFarming ? Label::FreeMode : data.contestSeason ? Label::SeasonMode : Label::TrialStart,
            {40, 28, staged ? 530.0f : 810.0f, 44});
        if (staged) view.Add(Label::StageGuide, {600,28,272,44}, {Action::StageGuide});
        view.Metric(Label::TrialDay, {900, 28, 332, 44}, 170, std::to_string(data.day));
        view.Add(Label::CampaignStart, {40, 380, 570, 48}, {Action::CampaignStart});
        view.Add(Label::CampaignExhibit, {630, 380, 602, 48}, {Action::CampaignExhibit});
        view.Add(Label::Experience, {40, 444, 440, 48}, {Action::ExperienceOpen});
        if (data.freeFarming || data.contestSeason)
            view.Add(data.freeFarming ? Label::FreeGoal : staged ? Label::CampaignGoal : Label::SeasonGoal, {40, 510, 1184, 42});
        else view.Metric(Label::TrialGoal, {40, 510, 1184, 42}, 650, std::to_string(data.goalMoney) + "G");
        view.Add(canalLesson ? Label::LessonBegin : Label::TrialBegin, {40, 572, 570, 48}, {Action::FlowContinue});
        view.Add(Label::Records, {630, 572, 602, 48}, {Action::FlowRecords});
        view.Add(data.freeFarming ? Label::SeasonMode : Label::FreeMode, {40, 636, 570, 48},
            {Action::ChangePlayMode, data.freeFarming ? 1 : 2});
        view.Add(Label::Restart, {630, 636, 602, 48}, {Action::Restart});
        return;
    }
    view.Add(data.freeFarming ? Label::FreeMode : data.contestSeason ? Label::SeasonMode : result ? Label::TrialComplete : Label::TrialStart, {166, 66, 940, 52});
    view.Add(data.freeFarming ? Label::FreeGoal : data.contestSeason ? Label::SeasonGoal : Label::TrialGoal, {166, 130, 940, 42});
    char text[64]{};
    if (data.contestSeason || data.freeFarming) std::snprintf(text, sizeof(text), "%d", data.day);
    else std::snprintf(text, sizeof(text), "%dG / %dG", data.money, data.goalMoney);
    view.Value(data.contestSeason || data.freeFarming ? Label::TrialDay : Label::TrialMoney, 188, text);
    std::snprintf(text, sizeof(text), "%d", data.day);
    if (!data.contestSeason && !data.freeFarming) view.Value(Label::TrialDay, 234, text);
    if (result) {
        view.Add(Label::TrialResultNote, {166, 302, 940, 42});
        std::snprintf(text, sizeof(text), "%d / %dG", data.cropCount, data.saleValue);
        view.Value(Label::TrialRemaining, 354, text);
    } else {
        view.Add(Label::TrialGrow, {166, 302, 940, 42});
        view.Add(Label::TrialWater, {166, 354, 940, 42});
        view.Add(data.freeFarming ? Label::FreePrepare : data.contestSeason ? Label::SeasonPrepare : Label::TrialSell, {166, 406, 940, 42});
    }
    view.Add(result ? Label::TrialReview : Label::TrialBegin, {166, 492, 452, 48}, {Action::FlowContinue});
    view.Add(Label::Restart, {638, 492, 452, 48}, {Action::Restart});
    view.Add(Label::Records, {166, 552, 452, 48}, {Action::FlowRecords});
    if (!mainGameOnly) view.Add(data.freeFarming ? Label::SeasonMode : Label::FreeMode, {638,552,452,48},
        {Action::ChangePlayMode,data.freeFarming ? 1 : 2});
    view.Add(data.freeFarming ? Label::FreeEndRule : data.contestSeason ? Label::SeasonEndRule : Label::TrialGoalOnly, {166, 616, 940, 38});
}
}
