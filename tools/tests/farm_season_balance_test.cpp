#include "farm/core/FarmGrid.h"
#include "farm/system/FarmContestDaySystem.h"
#include "farm/system/FarmContestOutcomeSystem.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
enum class Care { Managed, Dry, Canal, IntakeHalfSecond, IntakeOneSecond, IntakeTwoSeconds };
constexpr std::array<const char*, 6> kCareNames{
    "managed", "dry", "canal", "intake_0.5s", "intake_1s", "intake_2s"};
constexpr int kTicksPerSecond = 60;
constexpr float kTickSeconds = 1.0f / static_cast<float>(kTicksPerSecond);
constexpr int kMaximumTicks = 120000;
constexpr int kField = 2;
constexpr std::array kCrops{farm::CropType::TestCrop, farm::CropType::Carrot,
    farm::CropType::Tomato, farm::CropType::Pumpkin};

int IdealHeight(farm::CropType crop) {
    constexpr farm::FarmRules rules{};
    switch (crop) {
    case farm::CropType::TestCrop: return rules.testCropIdealHeight;
    case farm::CropType::Carrot: return rules.carrotIdealHeight;
    case farm::CropType::Tomato: return rules.tomatoIdealHeight;
    case farm::CropType::Pumpkin: return rules.pumpkinIdealHeight;
    default: throw std::runtime_error("invalid scenario crop");
    }
}

struct Outcome {
    std::array<int,3> eventPoints{};
    int money = 0;
    int minimumMoney = 0;
    int seedSpent = 0;
    int saleEarned = 0;
    int points = 0;
    int qualityPoints = 0;
    int sizePoints = 0;
    int sizeCappedEntries = 0;
    double harvestSizeSum = 0;
    int watering = 0;
    int purchases = 0;
    int harvested = 0;
    int intakeChanges = 0;
    int observations = 0;
    int suppliedHarvests = 0;
    int retainedHarvests = 0;
    double growingSeconds = 0;
    double excessSeconds = 0;
    double lowSeconds = 0;
    double growingRealSeconds = 0;
    double idleRealSeconds = 0;
    double irrigationReceived = 0;
    double runningSeconds = 0;
    FarmContestRating rating = FarmContestRating::Unrated;
};

// Scripted inputs only. Never assign growth, moisture, date, quality, size or inventory directly.
class SeasonRun {
public:
    SeasonRun(farm::CropType crop, Care care, float speed, bool staged=false, int sampleSeconds=0,
        const std::filesystem::path& exportDirectory={})
        : crop_(crop), care_(care), staged_(staged), sampleSeconds_(sampleSeconds) {
        Require(grid_.Initialize(5, 4), "grid init");
        economy_.Initialize(); tools_.Initialize(); growth_.Initialize(); water_.Initialize(); date_.Initialize();
        outcome_.minimumMoney = economy_.GetMoney();
        progression_.Initialize({}, FarmProgressionMode::ContestSeason);
        if (!exportDirectory.empty()) {
            Require(!std::filesystem::exists(exportDirectory), "review destination already exists");
            reviewLabel_ = exportDirectory.parent_path().parent_path().filename().string();
            selection_.Initialize();
            Require(documents_.Initialize(exportDirectory.string(), grid_, economy_, selection_, &date_, &progression_),
                "review documents init");
            exportReview_ = true;
        }
        if (staged_) {
            access_.Bind(date_,progression_.StagePolicy());
            economy_.BindStageAccess(access_); tools_.BindStageAccess(access_);
        }
        date_.SetTimeScale(speed);
        if (UsesCanal()) {
            Require(grid_.SelectTile(0), "source select");
            RaiseToIdeal();
            Require(tools_.ToggleSelectedWaterSource(grid_), "source placement");
            Require(grid_.SelectTile(1), "canal select");
            RaiseToIdeal();
            Require(tools_.ToggleSelectedCanal(grid_), "canal placement");
        }
        Require(grid_.SelectTile(kField), "field select");
        if (care_ != Care::Dry) RaiseToIdeal();
        Apply(FarmTool::Hoe);
        if (ControlsIntake()) SetIntake(false);
    }
    SeasonRun(const SeasonRun&) = delete;
    SeasonRun& operator=(const SeasonRun&) = delete;

    Outcome Run(bool skipLast) {
        for (int period = 0; period < 3; ++period) {
            AdvanceTo(1 + period * 10);
            if (staged_) {
                crop_ = farm::PlayableCrop(period);
                Require(economy_.IsCropUnlocked(crop_), "period crop locked");
                const int height = care_ == Care::Dry ? 0 : IdealHeight(crop_);
                for (int tile : {0,1,kField}) {
                    Require(grid_.SelectTile(tile), "stage layout select");
                    const int target = tile != kField && period > 0 && UsesCanal() ? 2 : height;
                    while (grid_.GetSelectedTile()->heightLevel < target) Require(tools_.RaiseSelectedTile(grid_), "stage raise");
                    while (grid_.GetSelectedTile()->heightLevel > target) Require(tools_.LowerSelectedTile(grid_), "stage lower");
                }
                Require(grid_.SelectTile(kField), "stage field select");
            }
            const int candidate = GrowAndHarvest();
            Require(tools_.CommitHarvestProtection(economy_, candidate, true, economy_.GetInventoryGeneration()), "protect");
            Require(tools_.CommitContestReservation(economy_, candidate, true, economy_.GetInventoryGeneration()), "reserve");
            GrowAndHarvest();
            const auto sold = economy_.SellAll();
            Require(sold.Succeeded() && sold.soldCount == 1 && economy_.GetTotalCropCount() == 1, "protected sale");
            earned_ += sold.earnedMoney;
            tools_.ClearHistory();
            const int contestDay = FarmContestEntrySystem::kContestDays[static_cast<std::size_t>(period)];
            AdvanceTo(contestDay);
            Require(notice_.PendingDay() == contestDay, "event notice absent");
            const auto pausedDate = date_.CaptureSnapshot();
            for (int i = 0; i < 120; ++i) Step();
            Require(date_.GetDay() == pausedDate.day && date_.GetElapsedSecondsInDay() == pausedDate.elapsedSecondsInDay,
                "pending event advanced clock");
            if (skipLast && period == 2) {
                Require(notice_.Acknowledge(contestDay), "skip acknowledgement");
                AdvanceTo(31);
                Require(progression_.IsCleared(), "deadline did not end season");
                break;
            }
            if (exportReview_ && period == 2) {
                Require(documents_.SaveAs(reviewLabel_+" / Judging Ready",grid_,economy_,selection_), "save pending judgement");
                readyDocumentId_ = documents_.GetActiveDocumentId();
            }
            const int moneyBefore = economy_.GetMoney();
            const auto generation = economy_.GetInventoryGeneration();
            Require(!tools_.CommitContestSubmission(economy_, contestDay, candidate, generation - 1), "stale submit accepted");
            Require(tools_.CommitContestSubmission(economy_, contestDay, candidate, generation), "submit");
            const auto& entry=economy_.GetContestResults()[period];
            outcome_.eventPoints[period]=entry.qualityPoints+entry.sizePoints;
            VerifyPresentation(period);
            Require(!tools_.CommitContestSubmission(economy_, contestDay, candidate, economy_.GetInventoryGeneration()), "duplicate submit");
            Require(economy_.GetTotalCropCount() == 0 && economy_.GetMoney() == moneyBefore, "submission ledger");
            Observe();
            Require(progression_.IsCleared() == (period == 2), "wrong completion day");
        }
        const auto summary = FarmContestSeasonSystem::Evaluate(date_.GetDay(), economy_.GetContestResults());
        Require(summary.valid && summary.finalized && summary.submitted == (skipLast ? 2 : 3), "final summary");
        Require(summary.missed == (skipLast ? 1 : 0), "missed count");
        Require(economy_.GetMoney() == farm::FarmRules{}.initialMoney - spent_ + earned_, "money ledger");
        Require(economy_.GetMoney() >= 0 && economy_.GetTotalSeedCount() == 0, "invalid final funds/seeds");
        const auto stopped = date_.CaptureSnapshot();
        for (int i = 0; i < 120; ++i) Step();
        Require(date_.GetDay() == stopped.day && date_.GetElapsedSecondsInDay() == stopped.elapsedSecondsInDay,
            "end clock advanced");
        outcome_.money = economy_.GetMoney(); outcome_.points = summary.totalPoints; outcome_.rating = summary.rating;
        outcome_.seedSpent = spent_; outcome_.saleEarned = earned_;
        for (const auto& result : economy_.GetContestResults()) {
            if (result.contestDay == 0) continue;
            outcome_.qualityPoints += result.qualityPoints;
            outcome_.sizePoints += result.sizePoints;
            if (result.harvest.quality.harvestSize.multiplier >= kFarmContestSubmissionRulesV1.sizeMaximum)
                ++outcome_.sizeCappedEntries;
        }
        Require(outcome_.qualityPoints + outcome_.sizePoints == outcome_.points, "score breakdown");
        Require(outcome_.minimumMoney >= 0 && outcome_.minimumMoney <= outcome_.money, "minimum funds");
        VerifyEnding(skipLast);
        if (exportReview_) ExportAndReload(skipLast);
        return outcome_;
    }
private:
    void VerifyPresentation(int index) const {
        const auto& entry=economy_.GetContestResults()[index];
        const auto comparison=FarmContestComparisonSystem::Build(index,date_.GetDay(),economy_.GetContestResults(),
            staged_,tools_.MaximumHarvestSize(entry.harvest.quality.crop));
        Require(comparison.valid && comparison.total==entry.qualityPoints+entry.sizePoints, "grown score/display mismatch");
        Require(comparison.quality.score==entry.harvest.quality.score &&
            comparison.quality.harvestSize.multiplier==entry.harvest.quality.harvestSize.multiplier,
            "presentation changed grown crop");
        if (staged_) Require(comparison.passed==(comparison.total>=kFarmStageRulesV1[index].passingPoints),
            "grown verdict mismatch");
        using Cue=FarmContestPresentationSystem::Cue;
        const Cue expected=!staged_ ? Cue::Recorded : comparison.passed ? Cue::Passed : Cue::Failed;
        FarmContestPresentationSystem presentation;
        presentation.Begin(comparison,false);
        int assessments=0, verdicts=0;
        for (int tick=0;tick<180;++tick) {
            presentation.Update(kTickSeconds);
            const auto cue=presentation.ConsumeCue();
            if (cue==Cue::Assess) ++assessments;
            else if (cue!=Cue::None) { Require(cue==expected,"wrong grown verdict sound"); ++verdicts; }
            Require(presentation.ConsumeCue()==Cue::None,"duplicate grown sound");
        }
        Require(assessments==1 && verdicts==1,"grown judging cue sequence");
        const auto frame=presentation.GetFrame();
        Require(frame.verdictVisible && frame.adviceReveal==1 && frame.displayedTotal==comparison.total,
            "grown judging did not complete");
        presentation.Close();
        presentation.Begin(comparison,true);
        presentation.Update(kTickSeconds);
        Require(presentation.GetFrame().adviceReveal==1 && presentation.ConsumeCue()==Cue::None,
            "grown detail replay repeated judging");
    }
    void VerifyEnding(bool skipLast) const {
        FarmContestOutcomeSystem ending;
        ending.Observe(date_.GetDay(),economy_.GetContestResults(),true,staged_);
        Require(ending.GetFrame().season.finalized && !ending.GetFrame().missedDay,
            "grown ending blocked by missed popup");
        Require(ending.ConsumeOpening() && !ending.ConsumeOpening(),"grown ending duplicate cue");
        for (int tick=0;tick<120;++tick) {
            ending.Update(kTickSeconds);
            ending.Observe(date_.GetDay(),economy_.GetContestResults(),true,staged_);
        }
        const auto& frame=ending.GetFrame();
        Require(frame.controlsReveal==1 && frame.season.totalPoints==outcome_.points,"grown ending total/timeline");
        Require((frame.season.events.back()==FarmContestEventStatus::Missed)==skipLast,"grown missing-entry state");
        if (staged_ && !skipLast) Require((frame.goals.back().status==FarmContestGoalStatus::Passed)==
            (outcome_.eventPoints.back()>=kFarmStageRulesV1.back().passingPoints),"grown final verdict");
    }
    void ExportAndReload(bool skipLast) {
        const int finalDay=date_.GetDay();
        Require(documents_.SaveAs(reviewLabel_+" / Season Result",grid_,economy_,selection_),"save grown result");
        const auto finalId=documents_.GetActiveDocumentId();
        Require(documents_.Load(finalId,grid_,economy_,selection_),"reload grown result");
        Require(date_.GetDay()==finalDay && progression_.StagePolicy().enabled,"result reload mode/day");
        const auto summary=FarmContestSeasonSystem::Evaluate(date_.GetDay(),economy_.GetContestResults());
        Require(summary.finalized && summary.totalPoints==outcome_.points,"result reload score");
        for (int index=0;index<(skipLast ? 2 : 3);++index) VerifyPresentation(index);
        if (!readyDocumentId_.empty()) {
            Require(documents_.Load(readyDocumentId_,grid_,economy_,selection_),"reload pending judgement");
            Require(date_.GetDay()==30 && !progression_.IsCleared(),"pending reload ended season");
            Require(economy_.GetContestResults().back().contestDay==0 && economy_.GetTotalCropCount()==1,
                "pending reload lost entry");
        }
    }
    void RaiseToIdeal() {
        for (int height = 0; height < IdealHeight(crop_); ++height)
            Require(tools_.RaiseSelectedTile(grid_), "ideal height command");
    }
    bool ControlsIntake() const noexcept {
        return care_ == Care::IntakeHalfSecond || care_ == Care::IntakeOneSecond || care_ == Care::IntakeTwoSeconds;
    }
    bool UsesCanal() const noexcept { return care_ == Care::Canal || ControlsIntake(); }
    int ObservationTicks() const noexcept {
        if (sampleSeconds_ > 0) return sampleSeconds_*kTicksPerSecond;
        if (care_ == Care::IntakeHalfSecond) return kTicksPerSecond / 2;
        if (care_ == Care::IntakeTwoSeconds) return kTicksPerSecond * 2;
        return kTicksPerSecond;
    }
    void SetIntake(bool enabled) {
        const auto* tile = grid_.GetSelectedTile();
        Require(tile != nullptr, "intake selection");
        if (tile->irrigationEnabled == enabled) return;
        Require(tools_.SetSelectedIrrigation(grid_, enabled), "intake command");
        ++outcome_.intakeChanges;
    }
    void SampleIntake() {
        const auto* tile = grid_.GetSelectedTile();
        Require(tile != nullptr, "sample selection");
        const auto forecast = growth_.Evaluate(*tile, crop_);
        Require(forecast.moistureValid, "intake moisture profile");
        // Test policy: hysteresis within the good band, sampled in unscaled simulation ticks.
        // This is not an in-game automatic controller or a measured human reaction time.
        const float band = forecast.goodMoistureMaximum - forecast.goodMoistureMinimum;
        const float openAt = forecast.goodMoistureMinimum + band * 0.25f;
        const float closeAt = forecast.goodMoistureMaximum - band * 0.25f;
        ++outcome_.observations;
        if (tile->moisture <= openAt) SetIntake(true);
        else if (tile->moisture >= closeAt) SetIntake(false);
    }
    void Apply(FarmTool tool) {
        Require(tools_.ApplyToolDetailed(grid_, tool, crop_, economy_, date_.GetDay()).Succeeded(), "tool action");
    }
    void Observe() {
        notice_.Observe(date_.GetDay(), economy_.GetContestResults());
        static_cast<void>(progression_.EvaluateSeason(FarmContestSeasonSystem::Evaluate(date_.GetDay(), economy_.GetContestResults())));
        Require(!progression_.EvaluateClear(economy_.GetMoney()), "money ended season");
    }
    void Step() {
        Observe();
        if (notice_.PendingDay() || progression_.IsCleared()) return;
        Require(++ticks_ <= kMaximumTicks, "season timed out");
        static_cast<void>(water_.UpdateWater(grid_, kTickSeconds, date_.GetTimeScale()));
        const auto balance = water_.GetLastStep(grid_);
        Require(balance.valid && std::isfinite(balance.balanceError) && std::abs(balance.balanceError) < 0.0001,
            "water conservation");
        const auto* tile = grid_.GetTile(kField);
        Require(tile != nullptr, "field pointer");
        Require(std::isfinite(tile->moisture) && tile->moisture >= 0 && tile->moisture <= 1 &&
            std::isfinite(tile->growth) && tile->growth >= 0 && tile->growth <= 1, "finite tile state");
        if (UsesCanal()) Require(water_.IsInIrrigationRange(kField), "canal scenario not connected");
        if (ControlsIntake()) {
            const auto flows = water_.GetLastTileFlows(grid_);
            Require(flows.size() > static_cast<std::size_t>(kField), "flow bounds");
            Require(std::isfinite(flows[kField].soilReceived) && flows[kField].soilReceived >= 0, "finite soil delivery");
            Require(tile->irrigationEnabled || flows[kField].soilReceived == 0, "closed intake delivered water");
            outcome_.irrigationReceived += flows[kField].soilReceived;
        }
        if (tile->crop != farm::CropType::None && tile->growth < 1.0f) {
            outcome_.growingSeconds += static_cast<double>(kTickSeconds) * date_.GetTimeScale();
            outcome_.growingRealSeconds += kTickSeconds;
        } else {
            outcome_.idleRealSeconds += kTickSeconds;
        }
        static_cast<void>(growth_.Update(grid_, kTickSeconds, date_.GetTimeScale()));
        notice_.Advance(date_, kTickSeconds, economy_.GetContestResults(), 31);
        outcome_.runningSeconds += kTickSeconds;
        Observe();
    }
    void AdvanceTo(int day) {
        while (date_.GetDay() < day) {
            Require(!progression_.IsCleared() && !notice_.PendingDay(), "blocked before target day");
            Step();
        }
        Require(date_.GetDay() == day, "day skipped");
    }
    int GrowAndHarvest() {
        const auto bought = economy_.BuySeed(crop_);
        Require(bought.Succeeded(), "cannot afford next seed");
        spent_ += bought.spentMoney; ++outcome_.purchases;
        outcome_.minimumMoney = (std::min)(outcome_.minimumMoney, economy_.GetMoney());
        if (care_ != Care::Dry) Require(tools_.CompostSelectedTile(grid_), "compost");
        const auto* selected = grid_.GetSelectedTile();
        Require(selected != nullptr, "planting selection");
        const auto profile = growth_.Evaluate(*selected, crop_);
        Require(profile.moistureValid, "managed moisture profile");
        const float band = profile.goodMoistureMaximum - profile.goodMoistureMinimum;
        // Same policy for every crop. One watering command must fit above the trigger without flooding.
        const float target = profile.goodMoistureMinimum + band * 0.5f;
        const float threshold = profile.goodMoistureMinimum + band * 0.25f;
        Require(threshold + farm::FarmRules{}.wateringMoistureIncrement <= profile.goodMoistureMaximum,
            "watering step does not fit managed policy");
        if (care_ == Care::Managed) {
            while (grid_.GetSelectedTile()->moisture < target) { Apply(FarmTool::Water); ++outcome_.watering; }
        }
        Apply(FarmTool::Seed);
        const int start = ticks_;
        const double receivedBefore = outcome_.irrigationReceived;
        const float initialMoisture = grid_.GetSelectedTile()->moisture;
        int nextObservation = ticks_;
        while (tools_.EvaluateTool(grid_, FarmTool::Harvest, crop_, &economy_).status != FarmToolActionStatus::Harvested) {
            Require(ticks_ - start < 36000 && !notice_.PendingDay(), "growth blocked");
            if (care_ == Care::Managed) {
                if (grid_.GetSelectedTile()->moisture < threshold) { Apply(FarmTool::Water); ++outcome_.watering; }
            }
            if (ControlsIntake() && ticks_ >= nextObservation) {
                SampleIntake();
                nextObservation += ObservationTicks();
            }
            Step();
        }
        if (ControlsIntake()) {
            SetIntake(false);
            if (outcome_.irrigationReceived > receivedBefore) ++outcome_.suppliedHarvests;
            else {
                Require(initialMoisture > 0 && outcome_.irrigationReceived > 0, "no irrigation or retained water");
                ++outcome_.retainedHarvests;
            }
        }
        const auto harvested = tools_.ApplyToolDetailed(grid_, FarmTool::Harvest, crop_, economy_, date_.GetDay());
        Require(harvested.Succeeded() && harvested.harvestedTile.has_value(), "harvest action");
        outcome_.excessSeconds += harvested.harvestedTile->careHistory.excessSeconds;
        outcome_.lowSeconds += harvested.harvestedTile->careHistory.lowSeconds + harvested.harvestedTile->careHistory.drySeconds;
        ++outcome_.harvested;
        const auto* record = economy_.GetHarvestRecord(economy_.GetHarvestRecordCount() - 1);
        Require(record && record->quality.harvestSize.known && record->harvestedDay == date_.GetDay(), "harvest record");
        Require(std::isfinite(record->quality.harvestSize.multiplier) && record->quality.harvestSize.multiplier > 0,
            "finite harvest size");
        outcome_.harvestSizeSum += record->quality.harvestSize.multiplier;
        return record->id;
    }
    farm::CropType crop_;
    Care care_;
    bool staged_=false;
    int sampleSeconds_=0;
    farm::FarmGrid grid_;
    FarmEconomySystem economy_;
    FarmToolActionSystem tools_;
    FarmGrowthSystem growth_;
    farm::FarmIrrigationSystem water_;
    FarmDateSystem date_;
    FarmContestDaySystem notice_;
    FarmProgressionSystem progression_;
    FarmStageAccessSystem access_;
    FarmCropSelectionSystem selection_;
    FarmDocumentSystem documents_;
    bool exportReview_ = false;
    std::string reviewLabel_;
    std::string readyDocumentId_;
    Outcome outcome_;
    int spent_ = 0, earned_ = 0, ticks_ = 0;
};

void ExportReview() {
    const auto root=std::filesystem::path("generated/codex_checks") / ("contest_cultivated_"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directories(root),"review root collision");
    constexpr std::array<const char*,3> names{{"01_passed","02_failed","03_missed"}};
    for (std::size_t i=0;i<names.size();++i) {
        SeasonRun run(farm::CropType::Carrot,i==1 ? Care::Canal : Care::IntakeOneSecond,1,true,2,
            root/names[i]/"Settings/farm");
        const auto result=run.Run(i==2);
        const int finalTarget=kFarmStageRulesV1.back().passingPoints;
        if (i==0) Require(result.eventPoints.back()>=finalTarget,"passed review is not passed");
        if (i==1) Require(result.eventPoints.back()<finalTarget,"failed review is not failed");
        std::cout << names[i] << ": " << result.eventPoints[0] << ',' << result.eventPoints[1] << ','
            << (i==2 ? "not submitted" : std::to_string(result.eventPoints[2])) << " points; money=" << result.money << '\n';
    }
    std::cout << "PASS: grown review saves and production-loader roundtrip\nREVIEW_ROOT=" << root.string() << '\n';
}
}

int main(int argc,char* argv[]) {
    try {
        if (argc==2 && std::string_view(argv[1])=="--export-review") { ExportReview(); return 0; }
        Require(argc==1,"usage: farm_season_balance_test [--export-review]");
        int cases = 0;
        std::cout << "crop,care,speed,end,money,points,rating,watering,harvests,growing_seconds,excess_seconds,running_seconds,"
            "intake_changes,observations,low_dry_seconds,growing_real_seconds,idle_real_seconds,irrigation_received,"
            "supplied_harvests,retained_harvests,minimum_money,seed_spent,sale_earned,quality_points,size_points,"
            "size_capped_entries,mean_harvest_size\n";
        for (const auto crop : kCrops) {
            std::array<Outcome, 3> managed{};
            for (const auto care : {Care::Managed, Care::Dry, Care::Canal,
                Care::IntakeHalfSecond, Care::IntakeOneSecond, Care::IntakeTwoSeconds}) {
                for (const bool skipLast : {false, true}) {
                    Outcome baseline;
                    std::size_t speedIndex = 0;
                    for (const float speed : {1.0f, 2.0f, 4.0f}) {
                        SeasonRun run(crop, care, speed);
                        std::cerr << "RUN: " << farm::ToString(crop) << '/' << kCareNames[static_cast<std::size_t>(care)]
                            << '/' << speed << '/' << (skipLast ? "deadline" : "submit") << '\n';
                        const auto result = run.Run(skipLast);
                        Require(result.points >= 0 && result.points <= 300, "season score range");
                        if (!skipLast) {
                            if (care == Care::Managed) managed[speedIndex] = result;
                            else if (care == Care::Dry || care == Care::Canal) {
                                Require(result.points < managed[speedIndex].points, "neglect beats managed score");
                                Require(result.growingSeconds > managed[speedIndex].growingSeconds,
                                    "neglect beats managed growth time");
                            }
                        }
                        if (speed == 1.0f) baseline = result;
                        Require(result.harvested == 6 && result.purchases == 6, "missing core loop");
                        if (care == Care::Managed || care == Care::Dry || care == Care::Canal) {
                            Require(std::abs(result.points - baseline.points) <= 3, "speed changed contest score");
                            Require(std::abs(result.money - baseline.money) <= 6, "speed changed finances");
                            Require(result.rating == baseline.rating, "speed changed rating");
                        } else {
                            Require(result.intakeChanges >= 3 && result.observations >= 6 && result.watering == 0 &&
                                result.suppliedHarvests > 0 && result.suppliedHarvests + result.retainedHarvests == 6,
                                "unexercised intake-only policy");
                        }
                        Require(std::abs(result.growingRealSeconds + result.idleRealSeconds - result.runningSeconds) < 0.001,
                            "time accounting");
                        Require(result.growingSeconds > 0 && result.idleRealSeconds > 0, "unmeasured pacing");
                        if (care == Care::Managed && !skipLast) Require(result.rating == FarmContestRating::S, "S unreachable on managed route");
                        if (care == Care::Canal) Require(result.excessSeconds > 0, "unmeasured excess-water scenario");
                        std::cout << farm::ToString(crop) << ','
                            << kCareNames[static_cast<std::size_t>(care)] << ',' << speed << ',' << (skipLast ? "deadline" : "submit")
                            << ',' << result.money << ',' << result.points << ',' << FarmContestRatingText(result.rating)
                            << ',' << result.watering << ',' << result.harvested << ',' << std::fixed << std::setprecision(2)
                            << result.growingSeconds << ',' << result.excessSeconds << ',' << result.runningSeconds
                            << ',' << result.intakeChanges << ',' << result.observations << ',' << result.lowSeconds
                            << ',' << result.growingRealSeconds << ',' << result.idleRealSeconds
                            << ',' << result.irrigationReceived << ',' << result.suppliedHarvests
                            << ',' << result.retainedHarvests << ',' << result.minimumMoney << ',' << result.seedSpent
                            << ',' << result.saleEarned << ',' << result.qualityPoints << ',' << result.sizePoints
                            << ',' << result.sizeCappedEntries << ',' << result.harvestSizeSum / result.harvested << '\n';
                        ++cases;
                        ++speedIndex;
                    }
                }
            }
        }
        Require(cases == 144, "scenario coverage");
        std::cout << "PASS: 144 full-season System scenarios; automated evidence, not a human playthrough\n";
        int stagedCases = 0;
        std::cout << "stage_care,sample_seconds,speed,day10,day20,day30,minimum_money\n";
        for (const auto care : {Care::Managed, Care::Dry, Care::Canal, Care::IntakeOneSecond}) {
            for (const int seconds : {1, 2, 3}) {
                if (care != Care::IntakeOneSecond && seconds != 1) continue;
                for (const float speed : {1.0f, 2.0f, 4.0f}) {
                    SeasonRun run(farm::CropType::Carrot, care, speed, true, seconds);
                    const auto result = run.Run(false);
                    Require(result.harvested == 6 && result.minimumMoney >= 0, "staged core loop");
                    for (std::size_t period = 0; period < result.eventPoints.size(); ++period) {
                        const int target = kFarmStageRulesV1[period].passingPoints;
                        if (care == Care::Managed || (care == Care::IntakeOneSecond && seconds <= 2 && speed <= 2))
                            Require(result.eventPoints[period] >= target + 8, "managed route has insufficient score margin");
                        if (care == Care::Dry || care == Care::Canal)
                            Require(result.eventPoints[period] < target, "neglect meets staged target");
                    }
                    std::cout << "stage_" << kCareNames[static_cast<std::size_t>(care)] << ',' << seconds << ',' << speed;
                    for (const auto points : result.eventPoints) std::cout << ',' << points;
                    std::cout << ',' << result.minimumMoney << '\n';
                    ++stagedCases;
                }
            }
        }
        Require(stagedCases == 18, "staged coverage");
        std::cout << "PASS: 18 restricted mixed-crop seasons\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
