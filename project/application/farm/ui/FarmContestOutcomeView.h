#pragma once
#include "farm/ui/FarmQualityView.h"
#include "farm/system/FarmContestOutcomeSystem.h"

namespace farmui {
inline void BuildContestMissedView(View& view, const FarmContestOutcomeSystem::Frame& frame) {
    view.modal = true; view.contestOutcome = true; view.outcomeReveal = frame.reveal;
    view.Add(Label::OutcomeMissed, {112,114,1056,56});
    view.Metric(Label::ContestEventDay, {112,202,1056,44}, 300, std::to_string(frame.missedDay));
    view.Add(Label::OutcomeNoScore, {112,272,1056,40});
    view.Add(Label::OutcomeReservation, {112,362,1056,36});
    view.Add(Label::OutcomeSubmitHint, {112,408,1056,36});
    view.Add(Label::OutcomeKeepFarm, {112,480,1056,40});
    view.Add(Label::OutcomeContinue, {752,594,416,56}, {Action::ContestMissedContinue,frame.missedDay});
}

inline void BuildContestOutcomeView(View& view, const FarmContestOutcomeSystem::Frame& frame,
    const FarmEconomySystem::ContestResults& results, bool confirmingRetry, bool failed) {
    view.modal = true; view.contestOutcome = true; view.outcomeReveal = frame.reveal;
    if (confirmingRetry) {
        view.Add(Label::OutcomeRetryTitle, {112,142,1056,56});
        view.Add(Label::OutcomeRetryCopy, {112,272,1056,40});
        view.Add(Label::OutcomeRetryNew, {112,328,1056,40});
        if (failed) view.Add(Label::OutcomeSaveFailed, {112,420,1056,40});
        view.Add(Label::Cancel, {112,570,400,56}, {Action::Cancel});
        view.Add(Label::OutcomeRetry, {672,570,496,56}, {Action::Accept});
        return;
    }
    view.seasonOutcome = true;
    view.outcomeVerdictReveal = frame.verdictReveal;
    view.outcomeRowReveal = frame.rowReveal;
    const bool controlsReady = frame.controlsReveal >= 1;
    view.outcomePositive = frame.goals.back().status == FarmContestGoalStatus::Passed;
    view.Add(Label::OutcomeFinal, {80,48,1120,40});
    view.items[view.count-1].opacity = frame.reveal;
    const bool missed = frame.season.events.back() == FarmContestEventStatus::Missed;
    const Label verdict = missed ? Label::OutcomeFinalMissed : view.outcomePositive ? Label::OutcomeFinalPassed :
        frame.goals.back().status == FarmContestGoalStatus::Failed ? Label::OutcomeFinalFailed : Label::OutcomeFinalLegacy;
    view.Add(verdict, {80,110,750,56});
    view.items[view.count-1].opacity = frame.verdictReveal;
    if (frame.season.submitted > 0) {
        view.Metric(Label::OutcomeRank, {860,116,340,44}, 250, FarmContestRatingText(frame.season.rating));
        view.items[view.count-1].opacity = frame.summaryReveal;
    }
    for (std::size_t i = 0; i < results.size(); ++i) {
        const auto firstItem = view.count;
        const float y = 218 + static_cast<float>(i)*86;
        const auto status = frame.season.events[i];
        view.Metric(Label::ContestEventDay, {80,y,224,36}, 150, std::to_string(FarmContestEntrySystem::kContestDays[i]));
        const bool submitted = status == FarmContestEventStatus::Submitted;
        const auto& goal = frame.goals[i];
        const Label state = !submitted ? Label::ContestMissed : goal.status == FarmContestGoalStatus::Passed ? Label::StagePassed :
            goal.status == FarmContestGoalStatus::Failed ? Label::StageFailed : Label::ContestSubmitted;
        view.Add(state, {316,y,224,36});
        view.Metric(Label::OutcomePoints, {540,y,420,36}, 220, submitted ?
            std::to_string(results[i].qualityPoints+results[i].sizePoints)+" / "+(goal.target ? std::to_string(goal.target) : "--") : "--");
        view.Add(Label::ContestDetails, {980,y,220,38}, {Action::ContestDetail,static_cast<int>(i)}, submitted && controlsReady);
        for (std::size_t j = firstItem; j < view.count; ++j) view.items[j].opacity = frame.rowReveal[i];
        view.outcomePoints[i] = submitted ? static_cast<float>(results[i].qualityPoints+results[i].sizePoints)/100 : 0;
        view.outcomeTargets[i] = submitted ? static_cast<float>(goal.target)/100 : 0;
        view.outcomePassed[i] = goal.status == FarmContestGoalStatus::Passed;
    }
    const auto firstSummaryItem = view.count;
    if (frame.season.submitted) {
        view.Metric(Label::ContestAggregate, {80,476,1120,32}, 620, std::to_string(frame.season.submitted)+" / "+
            std::to_string(frame.season.missed)+" / "+std::to_string(frame.season.totalPoints));
        view.Add(Label::OutcomeRankNote, {80,516,1120,32});
    } else view.Add(Label::OutcomeNoRank, {80,476,1120,32});
    view.Add(missed ? Label::OutcomeSubmitHint : Label::OutcomeReviewHint, {80,556,1120,32});
    for (std::size_t i = firstSummaryItem; i < view.count; ++i) view.items[i].opacity = frame.summaryReveal;
    if (controlsReady) {
        view.Add(Label::SeasonReview, {80,600,350,52}, {Action::FlowContinue});
        view.Add(Label::ReturnTitle, {450,600,270,52}, {Action::ReturnTitle});
        view.Add(Label::OutcomeRetry, {740,600,460,52}, {Action::SeasonRetry});
    } else {
        view.Add(Label::ContestReveal, {740,600,460,52}, {Action::ContestOutcomeReveal});
    }
}
}
