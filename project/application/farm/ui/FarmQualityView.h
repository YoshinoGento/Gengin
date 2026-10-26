#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmContestJudgeSystem.h"
#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmContestSeasonSystem.h"
#include "farm/system/FarmContestGoalSystem.h"
#include <cstdio>

namespace farmui {
inline Label CropLabel(farm::CropType crop) noexcept {
    switch(crop) {
    case farm::CropType::TestCrop: return Label::Turnip;
    case farm::CropType::Carrot: return Label::Carrot;
    case farm::CropType::Tomato: return Label::Tomato;
    case farm::CropType::Pumpkin: return Label::Pumpkin;
    default: return Label::NoCrop;
    }
}
inline std::string CropSizeText(const FarmCropSizeResult& size) {
    if (!size.known || !size.IsConsistent()) return "--";
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "%.2fx", static_cast<double>(size.multiplier));
    return buffer;
}
struct HarvestInventoryViewState {
    static constexpr int kRows = 2;
    struct Row { FarmCropQualityResult quality{}; int quantity = 0; int id = 0; bool saleProtected = false; bool canReserve = false; int harvestedDay = 0; };
    std::array<Row, kRows> rows{};
    int rowCount = 0;
    int page = 0, pages = 1, records = 0, capacity = 0, unknownCount = 0;
    int protectedCount = 0;
    std::uint64_t inventoryGeneration = 0;
    bool canChangeProtection = false;
    int contestReservationId = 0;
    farm::CropType reservedCrop = farm::CropType::None;
};

inline Label ContestReservationLabel(farm::CropType crop) noexcept {
    switch (crop) {
    case farm::CropType::Carrot: return Label::ContestCarrot;
    case farm::CropType::TestCrop: return Label::ContestTurnip;
    case farm::CropType::Tomato: return Label::ContestTomato;
    case farm::CropType::Pumpkin: return Label::ContestPumpkin;
    default: return Label::ContestNone;
    }
}

// The HUD toast owns this area until it expires; status is rebuilt from current values.
inline void BuildPlayCropStatusView(View& view, bool feedbackVisible, int protectedCount,
    farm::CropType reservedCrop, const FarmCropSizeResult* size) {
    view.feedback = feedbackVisible;
    if (feedbackVisible) return;
    if (size) view.Metric(Label::SizeForecast, {350,82,508,38}, 340, CropSizeText(*size));
    if (protectedCount > 0)
        view.Metric(Label::ProtectedCropCount, {350,126,508,38}, 340, std::to_string(protectedCount));
    if (farm::IsPlantableCrop(reservedCrop))
        view.Add(ContestReservationLabel(reservedCrop), {350,170,508,38});
}

inline void BuildHarvestInventoryView(View& view, const HarvestInventoryViewState& state) {
    char value[96]{};
    view.Add(Label::HarvestInventory, {176,130,480,42});
    view.Add(ContestReservationLabel(state.reservedCrop), {670,130,426,42});
    std::snprintf(value, sizeof(value), "%d / %d", state.records, state.capacity);
    view.Metric(Label::HarvestCount, {176,174,920,38}, 480, value);
    view.Add(Label::HarvestColumns, {296,216,800,32});
    for (int i = 0; i < std::clamp(state.rowCount, 0, HarvestInventoryViewState::kRows); ++i) {
        const auto& row = state.rows[i];
        std::snprintf(value, sizeof(value), "%d / %s / %d / %dG", row.quantity,
            CropSizeText(row.quality.harvestSize).c_str(), row.quality.score, row.quality.salePrice);
        view.Metric(CropLabel(row.quality.crop),
            {176,248.0f+i*88,920,38}, 120, value);
        const bool reserved = row.id > 0 && row.id == state.contestReservationId;
        view.Add(row.saleProtected ? Label::HarvestProtected : Label::HarvestProtect,
            {176,286.0f+i*88,200,38},
            {row.saleProtected ? Action::UnprotectHarvest : Action::ProtectHarvest, row.id, state.inventoryGeneration},
            state.canChangeProtection && row.id > 0 && !reserved, row.saleProtected);
        view.Add(reserved ? Label::ContestCancel : row.canReserve ? Label::ContestReserve : Label::ContestEligibility,
            {396,286.0f+i*88,340,38},
            {reserved ? Action::CancelContestReservation : Action::ReserveContestHarvest, row.id, state.inventoryGeneration},
            state.canChangeProtection && row.id > 0 && (reserved || row.canReserve), reserved);
        if (row.harvestedDay > 0)
            view.Metric(Label::HarvestDay, {756,286.0f+i*88,340,38}, 96, std::to_string(row.harvestedDay));
        else view.Add(Label::HarvestDayUnknown, {756,286.0f+i*88,340,38});
    }
    if (state.rowCount == 0) view.Add(Label::HarvestEmpty, {176,276,920,38});
    view.Add(Label::HarvestPrevious, {176,438,300,44}, {Action::HarvestPage,-1}, state.page > 0);
    view.Add(Label::HarvestNext, {796,438,300,44}, {Action::HarvestPage,1}, state.page + 1 < state.pages);
    std::snprintf(value, sizeof(value), "%d / %d", state.page + 1, state.pages);
    view.Metric(Label::HarvestPageNumber, {496,438,280,44}, 140, value);
    view.Metric(Label::HarvestUnknown, {176,490,460,38}, 260, std::to_string(state.unknownCount));
    view.Metric(Label::ProtectedCropCount, {676,490,420,38}, 250, std::to_string(state.protectedCount));
    view.Add(Label::ContestPreview, {176,534,920,38}, {Action::ContestPreview});
    view.Add(state.records >= state.capacity ? Label::HarvestFull :
        state.unknownCount > 0 ? Label::HarvestLegacyNotice : Label::ContestNotice, {176,578,920,38});
}

inline void BuildContestJudgeView(View& view, const FarmContestJudgeResult& result) {
    view.Add(Label::ContestPreviewTitle, {176,130,550,42});
    view.Add(Label::ContestPeriod, {766,130,330,42}, {Action::ContestPreview,1});
    view.Add(ContestReservationLabel(result.crop), {176,184,550,38});
    view.Add(Label::HarvestInventory, {766,184,330,38}, {Action::HarvestInventory});
    if (!result.IsValid()) {
        view.Add(result.issue == FarmContestJudgeIssue::NoReservation ? Label::ContestNotReserved : Label::ContestInvalid,
            {176,270,920,42});
    } else {
        char value[96]{};
        std::snprintf(value, sizeof(value), "%d / 100", result.recordedQuality);
        view.Metric(Label::ContestRecordedQuality, {176,238,920,38}, 540, value);
        std::snprintf(value, sizeof(value), "%d / %d", result.qualityPoints, result.rules.qualityPoints);
        view.Metric(Label::ContestQualityPoints, {176,282,920,38}, 540, value);
        std::snprintf(value, sizeof(value), "%.2fx", static_cast<double>(result.recordedSize));
        view.Metric(Label::SizeRecorded, {176,326,920,38}, 540, value);
        std::snprintf(value, sizeof(value), "%d / %d", result.sizePoints, result.rules.sizePoints);
        view.Metric(Label::ContestSizePoints, {176,370,920,38}, 540, value);
        std::snprintf(value, sizeof(value), "%d / 100", result.totalPoints);
        view.Metric(Label::ContestTotal, {176,414,920,42}, 540, value);
        std::snprintf(value, sizeof(value), "%.2fx - %.2fx", static_cast<double>(result.rules.sizeMinimum),
            static_cast<double>(result.rules.sizeMaximum));
        view.Metric(Label::ContestSizeRange, {176,466,920,38}, 540, value);
        view.Add(result.partialQuality ? Label::HintPartialRecord : Label::ContestRounding, {176,510,920,38});
    }
    view.Add(Label::ContestPreviewNotice, {176,568,920,38});
}

inline Label ContestEntryLabel(FarmContestEntryIssue issue) noexcept {
    switch (issue) {
    case FarmContestEntryIssue::Eligible: return Label::EntryEligible;
    case FarmContestEntryIssue::InvalidDay: return Label::EntryInvalidDay;
    case FarmContestEntryIssue::SeasonEnded: return Label::EntrySeasonEnded;
    case FarmContestEntryIssue::NoReservation: return Label::ContestNotReserved;
    case FarmContestEntryIssue::InvalidRecord: return Label::ContestInvalid;
    case FarmContestEntryIssue::UnknownHarvestDay: return Label::EntryUnknownDay;
    case FarmContestEntryIssue::FutureHarvestDay: return Label::EntryFutureDay;
    case FarmContestEntryIssue::OutsidePeriod: return Label::EntryOutsidePeriod;
    default: return Label::ContestInvalid;
    }
}

inline void BuildContestDayNoticeView(View& view, int day) {
    view.modal = true;
    view.Add(Label::ContestDayTitle, {176,150,920,48});
    view.Metric(Label::EntryContestDay, {176,216,920,42}, 540, std::to_string(day));
    view.Add(Label::ContestDayStopped, {176,278,920,42});
    view.Add(Label::ContestDayDeadline, {176,338,920,42});
    view.Add(Label::ContestDayReview, {176,422,450,48}, {Action::ContestDayReview,day});
    view.Add(Label::ContestDayPrepare, {646,422,450,48}, {Action::ContestDayPrepare,day});
    view.Add(Label::ContestDayResume, {176,496,920,48}, {Action::ContestDayResume,day});
}

inline Label ContestSubmissionLabel(FarmContestSubmissionStatus status) noexcept {
    switch (status) {
    case FarmContestSubmissionStatus::Ready: return Label::SubmitContest;
    case FarmContestSubmissionStatus::AlreadySubmitted: return Label::ContestSubmitted;
    case FarmContestSubmissionStatus::NotContestDay: return Label::ContestDayOnly;
    default: return Label::ContestCannotSubmit;
    }
}

inline void BuildContestEntryView(View& view, const FarmContestEntryResult& result,
    FarmContestSubmissionStatus submission = FarmContestSubmissionStatus::NotContestDay, Request submitRequest = {}) {
    view.Add(Label::ContestPeriod, {176,130,550,42});
    view.Add(Label::EntryBackToScore, {766,130,330,42}, {Action::ContestPreview,0});
    view.Metric(Label::TrialDay, {176,184,920,38}, 540,
        result.currentDay > 0 ? std::to_string(result.currentDay) : "--");
    view.Metric(Label::EntryContestDay, {176,228,920,38}, 540,
        result.HasContest() ? std::to_string(result.contestDay) : "--");
    view.Metric(Label::EntryPeriodRange, {176,272,920,38}, 540,
        result.HasContest() ? std::to_string(result.firstHarvestDay)+" - "+std::to_string(result.contestDay) : "--");
    view.Metric(Label::EntryDaysRemaining, {176,316,920,38}, 540,
        result.HasContest() ? std::to_string(result.daysRemaining) : "--");
    view.Add(ContestReservationLabel(result.crop), {176,360,550,38});
    view.Add(Label::HarvestInventory, {766,360,330,38}, {Action::HarvestInventory});
    view.Metric(Label::HarvestDay, {176,404,920,38}, 540,
        result.harvestedDay > 0 ? std::to_string(result.harvestedDay) : "--");
    view.Add(submission == FarmContestSubmissionStatus::AlreadySubmitted ? Label::ContestSubmitted :
        ContestEntryLabel(result.issue), {176,456,920,42});
    view.Add(ContestSubmissionLabel(submission), {176,512,450,42}, submitRequest,
        submission == FarmContestSubmissionStatus::Ready);
    view.Add(Label::ContestResults, {646,512,450,42}, {Action::ContestResults});
    view.Add(Label::ContestSubmissionNotice, {176,568,920,38});
}

inline Label ContestEventLabel(FarmContestEventStatus status) noexcept {
    switch (status) {
    case FarmContestEventStatus::Upcoming: return Label::ContestUpcoming;
    case FarmContestEventStatus::Open: return Label::ContestOpen;
    case FarmContestEventStatus::Submitted: return Label::ContestEntered;
    case FarmContestEventStatus::Missed: return Label::ContestMissed;
    default: return Label::ContestUnknown;
    }
}

inline void BuildContestResultsView(View& view, const FarmEconomySystem::ContestResults& results,
    const FarmContestSeasonSummary& summary, const FarmContestGoals& goals = {}) {
    view.Add(summary.finalized ? Label::ContestSeasonFinal : Label::ContestResults, {176,130,550,42});
    view.Add(Label::ContestPeriod, {766,130,330,42}, {Action::ContestPreview,1});
    view.Add(Label::ContestResultColumns, {176,184,920,38});
    for (std::size_t i=0; i<results.size(); ++i) {
        const auto& result=results[i];
        const float y=238+static_cast<float>(i)*90;
        char value[80]{};
        if (!summary.valid || summary.events[i] != FarmContestEventStatus::Submitted) {
            std::snprintf(value,sizeof(value),"%d / -- / -- / --",FarmContestEntrySystem::kContestDays[i]);
            view.Metric(ContestEventLabel(summary.events[i]),{176,y,920,38},220,value);
        } else {
            std::snprintf(value,sizeof(value),"%d / %d / %d / %d",result.contestDay,
                result.qualityPoints,result.sizePoints,result.qualityPoints+result.sizePoints);
            view.Metric(CropLabel(result.harvest.quality.crop),
                {176,y,700,38},220,value);
            view.Add(Label::ContestDetails,{896,y,200,38},{Action::ContestDetail,static_cast<int>(i)});
            std::snprintf(value,sizeof(value),"%d / %.2fx",result.harvest.harvestedDay,
                static_cast<double>(result.harvest.quality.harvestSize.multiplier));
            if (goals[i].status == FarmContestGoalStatus::Unknown) {
                view.Add(Label::ContestEntered,{176,y+40,190,32});
                view.Metric(Label::ContestResultHarvest,{376,y+40,720,32},340,value);
            }
        }
        if (goals[i].status != FarmContestGoalStatus::Unknown) {
            const bool passed = goals[i].status == FarmContestGoalStatus::Passed;
            const bool failed = goals[i].status == FarmContestGoalStatus::Failed;
            view.Metric(passed ? Label::StagePassed : failed ? Label::StageFailed : Label::StageThreshold,
                {176,y+40,450,32}, 240, std::to_string(goals[i].target) + " pt");
            if (failed) view.Metric(Label::StageShortfall, {646,y+40,450,32}, 250,
                std::to_string(goals[i].shortfall) + " pt");
        }
    }
    char aggregate[64]{};
    if (summary.valid) std::snprintf(aggregate,sizeof(aggregate),"%d / %d / %d",summary.submitted,summary.missed,summary.totalPoints);
    else std::snprintf(aggregate,sizeof(aggregate),"-- / -- / --");
    view.Metric(Label::ContestAggregate,{176,500,920,38},540,aggregate);
    if (summary.valid && summary.finalized) {
        view.Metric(summary.submitted==0 ? Label::SeasonUnrated : Label::SeasonRating,
            {176,542,420,36},300,FarmContestRatingText(summary.rating));
        char next[32]{};
        if (summary.nextRating != FarmContestRating::Unrated)
            std::snprintf(next,sizeof(next),"%s / %d pt",FarmContestRatingText(summary.nextRating),summary.pointsToNextRating);
        else std::snprintf(next,sizeof(next),"--");
        view.Metric(Label::SeasonNextRating,{616,542,480,36},240,next);
        char thresholds[64]{};
        std::snprintf(thresholds,sizeof(thresholds),"S%d A%d B%d C%d",
            kFarmContestRatingV1[0].points,kFarmContestRatingV1[1].points,
            kFarmContestRatingV1[2].points,kFarmContestRatingV1[3].points);
        view.Metric(Label::SeasonRatingRules,{176,582,920,30},380,thresholds);
    } else view.Add(Label::ContestResultNotice,{176,568,920,38});
}

inline void BuildSeasonEndView(View& view, const FarmEconomySystem::ContestResults& results,
    const FarmContestSeasonSummary& summary, const FarmContestGoals& goals = {}) {
    view.modal = true;
    BuildContestResultsView(view, results, summary, goals);
    view.items[1].label = Label::Records;
    view.items[1].request = {Action::FlowRecords};
    view.Add(Label::SeasonReview,{176,620,450,42},{Action::FlowContinue});
    view.Add(Label::Restart,{646,620,450,42},{Action::Restart});
}

inline void BuildStageGuideView(View& view, const FarmStageAccess& access) {
    view.Add(Label::StageGuide, {176,130,920,42});
    if (!access.restricted) {
        view.Add(Label::StageLegacy, {176,220,920,42});
        return;
    }
    view.Metric(Label::StageCurrent, {176,184,450,38}, 220, std::to_string(access.stage + 1));
    view.Metric(Label::StageHeight, {646,184,450,38}, 250, "H" + std::to_string(access.maximumHeight));
    view.Add(Label::StageGuideColumns, {376,236,720,32});
    for (std::size_t i = 0; i < kFarmStageRulesV1.size(); ++i) {
        const auto& rule = kFarmStageRulesV1[i];
        char value[64]{};
        std::snprintf(value, sizeof(value), "%d / %d / %d pt / H%d", rule.firstDay, rule.contestDay, rule.passingPoints, rule.maximumHeight);
        view.Metric(CropLabel(rule.unlockedCrop), {176,278+static_cast<float>(i)*52,920,40}, 240, value);
    }
    view.Add(Label::StageCropChoice, {176,444,920,38});
    view.Add(Label::StageBasicTools, {176,492,920,38});
    view.Add(Label::StageDateUnlock, {176,540,920,38});
    view.Add(Label::ContestResults, {176,584,920,32}, {Action::ContestResults});
}

inline Label QualityHintLabel(FarmQualityFocus focus, bool harvested) noexcept {
    switch (focus) {
    case FarmQualityFocus::Maturity: return harvested ? Label::HintMaturityNext : Label::HintMaturity;
    case FarmQualityFocus::Water: return Label::HintWater;
    case FarmQualityFocus::Terrain: return Label::HintTerrain;
    case FarmQualityFocus::Nutrients: return Label::HintNutrients;
    case FarmQualityFocus::Balanced: return Label::HintBalanced;
    default: return Label::HintUnknown;
    }
}
// Read-only presentation: scores and prices always come from the gameplay System.
inline void BuildQualityView(View& view, const FarmCropQualityResult& quality, const FarmQualityAdvice& advice,
    int tileIndex, bool harvested) {
    view.Add(Label::QualityCurrent, {170,130,450,48}, {Action::Quality,0}, true, !harvested);
    view.Add(Label::QualityHarvest, {646,130,450,48}, {Action::Quality,1}, true, harvested);
    if (!quality.IsValid()) {
        view.Add(harvested ? Label::QualityNoHarvest : Label::NoCrop, {176,254,920,44});
        return;
    }
    view.Add(CropLabel(quality.crop), {176,192,150,38});
    if (!harvested) view.Metric(Label::Selected, {646,192,450,38}, 160, tileIndex >= 0 ? "#" + std::to_string(tileIndex) : "--");
    view.radar.visible = true;
    const std::array<float,4> values{{quality.maturity, quality.waterBalance, quality.terrainFit, quality.nutrientBalance}};
    constexpr std::array<Label,4> labels{{Label::QualityMaturity, Label::QualityWater, Label::QualityTerrain, Label::QualityNutrient}};
    for (std::size_t i=0; i<values.size(); ++i) {
        const bool known = std::isfinite(values[i]) && (i != 3 || quality.nutrientKnown);
        view.radar.known[i] = known;
        view.radar.values[i] = known ? std::clamp(values[i], 0.0f, 1.0f) : 0.0f;
        view.Metric(labels[i], {600,242 + static_cast<float>(i)*54,496,42}, 280,
            known ? std::to_string(static_cast<int>(std::lround(view.radar.values[i]*100))) + " / 100" : "--");
    }
    view.Add(Label::QualityScale, {600,448,496,38});
    view.Metric(harvested ? Label::SizeRecorded : Label::SizeForecast,
        {176,450,400,38}, 270, CropSizeText(quality.harvestSize));
    const Label context = advice.partial ? (harvested ? Label::HintPartialRecord : Label::HintPartialEstimate)
        : (harvested ? Label::QualityRecorded : Label::QualityEstimate);
    view.Add(context, {176,536,920,38});
    view.Metric(harvested ? Label::QualityFinalPrice : Label::Score, {176,492,920,38}, 600,
        std::to_string(quality.score) + " / " + std::to_string(quality.salePrice) + "G");
    view.Add(QualityHintLabel(advice.focus, harvested), {176,578,920,38});
}
}
