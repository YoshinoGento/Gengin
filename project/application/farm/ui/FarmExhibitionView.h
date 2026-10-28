#pragma once
#include "farm/system/FarmExhibitionSystem.h"
#include "farm/ui/FarmContestDetailView.h"

namespace farmui {
inline void BuildExhibitionView(View& view, const FarmExhibitionSystem& system) {
    using Phase = FarmExhibitionSystem::Phase;
    view.modal = true;
    if (system.GetPhase() == Phase::Result || system.GetPhase() == Phase::Missed) {
        BuildContestDetailView(view, system.ContestFrame());
        view.items[0].label = Label::ExhibitionTitle;
        if (system.Complete()) {
            auto& back = view.items[view.count - 1];
            if (system.ContestFrame().page == FarmContestPresentationSystem::Page::Result) {
                back.label = Label::ExhibitionHome; back.request = {Action::ExhibitionHome};
            }
            view.Add(Label::ExhibitionAgain, {48,654,300,46}, {Action::ExhibitionWatch});
        }
        return;
    }
    view.Add(Label::ExhibitionTitle, {176,70,928,52});
    view.Add(Label::ExhibitionGoal, {176,156,928,42});
    view.Metric(Label::ContestTargetShort, {176,212,928,42}, 370,
        std::to_string(kFarmStageRulesV1.back().passingPoints) + " pt");
    view.Add(Label::ExhibitionRecorded, {176,276,928,36});
    constexpr std::array<Label,3> cases{{Label::StageFailed,Label::StagePassed,Label::ContestMissed}};
    for (int i = 0; i < FarmExhibitionSystem::kCaseCount; ++i)
        view.Add(cases[i], {176 + i * 316.f,352,296,56}, {Action::ExhibitionSelect,i},
            system.Available(i), system.Selection() == i);
    if (!system.Available(system.Selection())) view.Add(Label::ExhibitionMissing, {176,432,928,40});
    view.Add(Label::ExhibitionWatch, {330,496,620,64}, {Action::ExhibitionWatch}, system.Available(system.Selection()), true);
    view.Add(system.Startup() ? Label::ReturnTitle : Label::Close, {760,608,344,48}, {Action::ExhibitionExit});
}
}
