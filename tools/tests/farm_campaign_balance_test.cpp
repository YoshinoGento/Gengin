#include "farm/system/FarmDevelopmentSystem.h"
#include "farm/system/FarmSimulationStepSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm_campaign_exhibit.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
constexpr float kFrameSeconds = 1.0f / 60.0f;
constexpr int kMaximumTicks = 120000;
constexpr double kInspectionSeconds = 5.0;
enum class Care { Managed, NoCompost };
constexpr std::array kCareNames{"managed", "no_compost"};
struct LayoutPlan { const char* name; bool twoSources; bool expand; };
constexpr std::array kPlans{
    LayoutPlan{"one_source_expand", false, true},
    LayoutPlan{"two_sources_expand", true, true},
    LayoutPlan{"one_source_no_expand", false, false},
};
struct Metrics {
    std::array<int, 3> scores{};
    std::array<int, 2> expansionDays{};
    int minimumMoney = farm::FarmRules{}.initialMoney, money = 0, spent = 0, earned = 0;
    int expansionSpent = 0;
    int harvests = 0, sold = 0, intakeChanges = 0, watering = 0, cured = 0;
    int pestWarnings = 0, rainHarvests = 0, ticks = 0, inspections = 0;
    double quality = 0, activeSeconds = 0, rain = 0;
};

// Bounded scripted input through production Systems, not a human usability test.
// No direct assignment of date, moisture, growth, harvest quality or economy.
class Campaign final {
public:
    Campaign(Care care, LayoutPlan plan, float speed, FarmCampaignExhibit* exhibit = nullptr)
        : care_(care), plan_(plan), exhibit_(exhibit) {
        Check(grid_.Initialize(5, 4), "grid init");
        tools_.Initialize(); water_.Initialize(); growth_.Initialize(); date_.Initialize(); economy_.Initialize();
        progression_.Initialize({}, FarmProgressionMode::ContestSeason, true, true, true);
        access_.Bind(date_, progression_.StagePolicy());
        tools_.BindStageAccess(access_); economy_.BindStageAccess(access_);
        date_.SetTimeScale(speed);
        PrepareLayout();
    }
    Campaign(const Campaign&) = delete;
    Campaign& operator=(const Campaign&) = delete;
    Metrics Run() {
        for (int stage = 0; stage < 3; ++stage) {
            AdvanceTo(1 + stage * 10);
            crop_ = farm::PlayableCrop(stage);
            Check(economy_.IsCropUnlocked(crop_), "stage crop locked");
            MaybeExpand();
            PrepareLayout();
            GrowBatch(false);
            // Plant ten simulated seconds before the scheduled pest warning.
            AdvanceTo(5 + stage * 10);
            while (date_.GetElapsedSecondsInDay() < date_.GetDayLengthSeconds() - 10.0f) Step();
            GrowBatch(false);
            AdvanceTo(FarmWeatherSystem::kRainDays[static_cast<std::size_t>(stage)]);
            const int candidate = GrowBatch(true);
            const int contestDay = FarmContestEntrySystem::kContestDays[static_cast<std::size_t>(stage)];
            AdvanceTo(contestDay);
            Check(contest_.PendingDay() == contestDay, "contest not pending");
            if (exhibit_ && stage == 0) exhibit_->Write(4, grid_, economy_, date_, progression_, crop_);
            const auto clock = date_.CaptureSnapshot();
            const auto blocked = FarmSimulationStepSystem::Step(Context(), kFrameSeconds);
            Check(!blocked.advanced && date_.GetElapsedSecondsInDay() == clock.elapsedSecondsInDay,
                "pending contest advanced");
            Check(tools_.CommitContestSubmission(economy_, contestDay, candidate,
                economy_.GetInventoryGeneration()), "contest submission");
            const auto& result = economy_.GetContestResults()[static_cast<std::size_t>(stage)];
            metrics_.scores[static_cast<std::size_t>(stage)] = result.qualityPoints + result.sizePoints;
            Check(contest_.Acknowledge(contestDay), "contest acknowledgement");
            tools_.ClearHistory();
            static_cast<void>(progression_.EvaluateSeason(
                FarmContestSeasonSystem::Evaluate(date_.GetDay(), economy_.GetContestResults())));
            if (exhibit_ && stage == 0) exhibit_->Write(6, grid_, economy_, date_, progression_, crop_);
        }
        metrics_.money = economy_.GetMoney();
        Check(progression_.IsCleared() && date_.GetDay() == 30, "season ending");
        Check(metrics_.money == farm::FarmRules{}.initialMoney - metrics_.spent - metrics_.expansionSpent + metrics_.earned &&
            economy_.GetLifetimeSales() == metrics_.earned, "money ledger");
        Check(metrics_.minimumMoney >= 0 && economy_.GetTotalCropCount() == 0 &&
            economy_.GetTotalSeedCount() == 0 && metrics_.sold + 3 == metrics_.harvests, "inventory ledger");
        Check(metrics_.pestWarnings == 3 && metrics_.cured == 3 && metrics_.rain > 0 &&
            metrics_.rainHarvests >= 6, "calendar hazards not exercised");
        if (plan_.expand) Check(grid_.GetHeight() == FarmDevelopmentSystem::kMaximumHeight &&
            metrics_.expansionDays == std::array<int, 2>{11, 21} &&
            metrics_.expansionSpent == FarmDevelopmentSystem::kExpansionCosts[0]+FarmDevelopmentSystem::kExpansionCosts[1],
            "staged expansion unreachable");
        else Check(grid_.GetHeight() == FarmDevelopmentSystem::kInitialHeight &&
            metrics_.expansionDays == std::array<int, 2>{}, "unrequested expansion");
        return metrics_;
    }
private:
    FarmSimulationStepSystem::Context Context() {
        return {grid_, water_, growth_, date_, contest_, economy_, progression_};
    }
    void Select(int index) {
        Check(grid_.SelectTile(index) && grid_.GetSelectedTile(), "selection");
    }
    void Height(int wanted) {
        Check(wanted >= 0 && wanted <= access_.Get().maximumHeight, "height outside stage");
        while (grid_.GetSelectedTile()->heightLevel < wanted) Check(tools_.RaiseSelectedTile(grid_), "raise");
        while (grid_.GetSelectedTile()->heightLevel > wanted) Check(tools_.LowerSelectedTile(grid_), "lower");
    }
    void PrepareLayout() {
        const int channelHeight = date_.GetDay() > 10 ? 2 : 1;
        for (int row = 0; row < grid_.GetHeight(); ++row) {
            const int index = row * 5 + 2;
            Select(index); Height(channelHeight);
            const bool source = row == 0 || (plan_.twoSources && row == 3);
            const auto feature = grid_.GetSelectedTile()->feature;
            if (feature == farm::FarmTileFeature::None) {
                Check(source ? tools_.ToggleSelectedWaterSource(grid_) : tools_.ToggleSelectedCanal(grid_),
                    "water feature placement");
            }
        }
        tools_.ClearHistory();
        water_.Rebuild(grid_);
    }
    std::array<int, 6> Fields() const {
        return {6, 8, grid_.GetHeight() >= 5 ? 21 : -1, grid_.GetHeight() >= 5 ? 23 : -1,
            grid_.GetHeight() >= 6 ? 26 : -1, grid_.GetHeight() >= 6 ? 28 : -1};
    }
    void Intake(bool enabled) {
        if (grid_.GetSelectedTile()->irrigationEnabled == enabled) return;
        Check(tools_.SetSelectedIrrigation(grid_, enabled), "intake");
        ++metrics_.intakeChanges;
    }
    void Apply(FarmTool tool) {
        Check(tools_.ApplyToolDetailed(grid_, tool, crop_, economy_, date_.GetDay()).Succeeded(), "tool");
    }
    void Step() {
        Check(++metrics_.ticks <= kMaximumTicks, "simulation bound");
        const auto step = FarmSimulationStepSystem::Step(Context(), kFrameSeconds);
        Check(step.advanced, "unexpected simulation block");
        metrics_.rain += step.rainDelivered;
        if (step.pestWarning) ++metrics_.pestWarnings;
        if (exhibit_) {
            if (step.pestWarning && !exhibit_->Has(1)) exhibit_->Write(1, grid_, economy_, date_, progression_, crop_);
            const auto* first = grid_.GetTile(6);
            if (first && first->growth > 0.2f) {
                if (date_.GetDay() == 1 && !exhibit_->Has(0)) exhibit_->Write(0, grid_, economy_, date_, progression_, crop_);
                if (date_.GetDay() == 8 && !exhibit_->Has(2)) exhibit_->Write(2, grid_, economy_, date_, progression_, crop_);
            }
        }
        for (int index = 0; index < grid_.GetTileCount(); ++index) {
            const auto* tile = grid_.GetTile(index);
            Check(tile && std::isfinite(tile->moisture) && tile->moisture >= 0 && tile->moisture <= 1 &&
                std::isfinite(tile->growth) && tile->growth >= 0 && tile->growth <= 1, "finite tile bounds");
        }
        Check(economy_.GetMoney() == farm::FarmRules{}.initialMoney - metrics_.spent - metrics_.expansionSpent + metrics_.earned,
            "running ledger");
    }
    void AdvanceTo(int day) {
        while (date_.GetDay() < day) Step();
        Check(date_.GetDay() == day, "calendar skipped");
    }
    int GrowBatch(bool reserve) {
        const auto fields = Fields();
        const auto rainBefore = metrics_.rain;
        for (int index : fields) {
            if (index < 0) continue;
            Select(index);
            Height(crop_ == farm::CropType::Pumpkin ? 0 : 1);
            if (grid_.GetSelectedTile()->state == farm::FarmTileState::Empty) Apply(FarmTool::Hoe);
            Intake(false);
            if (care_ != Care::NoCompost) Check(tools_.CompostSelectedTile(grid_), "compost");
            const auto bought = economy_.BuySeed(crop_);
            if (!bought.Succeeded()) std::cerr << "unaffordable: day=" << date_.GetDay()
                << " money=" << economy_.GetMoney() << " field=" << index << '\n';
            Check(bought.Succeeded(), "next batch unaffordable");
            metrics_.spent += bought.spentMoney;
            metrics_.minimumMoney = (std::min)(metrics_.minimumMoney, economy_.GetMoney());
            const auto profile = growth_.Evaluate(*grid_.GetSelectedTile(), crop_);
            const float target = (profile.goodMoistureMinimum + profile.goodMoistureMaximum) * 0.5f;
            for (int attempt = 0; grid_.GetSelectedTile()->moisture < target && attempt < 10; ++attempt) {
                Apply(FarmTool::Water); ++metrics_.watering;
            }
            Apply(FarmTool::Seed);
            Intake(true);
        }
        tools_.ClearHistory();
        std::array<bool, 6> done{};
        int candidate = -1;
        double elapsed = 0, nextInspection = 0;
        for (;;) {
            bool unfinished = false;
            if (elapsed >= nextInspection) {
                ++metrics_.inspections;
                nextInspection += kInspectionSeconds;
                for (std::size_t i = 0; i < fields.size(); ++i) {
                    if (fields[i] < 0 || done[i]) continue;
                    Select(fields[i]);
                    const auto* tile = grid_.GetSelectedTile();
                    if (tile->pests.warningDay != 0) {
                        Check(FarmPestSystem::Cure(grid_, fields[i], tile->pests.warningDay), "pest cure");
                        ++metrics_.cured;
                    }
                    const auto profile = growth_.Evaluate(*tile, crop_);
                    const float band = profile.goodMoistureMaximum - profile.goodMoistureMinimum;
                    if (tile->moisture <= profile.goodMoistureMinimum + band * 0.25f) Intake(true);
                    else if (tile->moisture >= profile.goodMoistureMaximum - band * 0.25f) Intake(false);
                }
            }
            for (std::size_t i = 0; i < fields.size(); ++i) {
                if (fields[i] < 0 || done[i]) continue;
                Select(fields[i]);
                if (!farm::IsHarvestReady(*grid_.GetSelectedTile())) { unfinished = true; continue; }
                Apply(FarmTool::Harvest); Intake(false);
                Check(economy_.GetHarvestRecordCount() > 0, "missing harvest");
                const auto* record = economy_.GetHarvestRecord(economy_.GetHarvestRecordCount() - 1);
                Check(record && record->harvestedDay == date_.GetDay(), "harvest record");
                metrics_.quality += record->quality.score;
                ++metrics_.harvests;
                if (reserve && candidate < 0) {
                    candidate = record->id;
                    Check(tools_.CommitHarvestProtection(economy_, candidate, true, economy_.GetInventoryGeneration()),
                        "candidate protection");
                    Check(tools_.CommitContestReservation(economy_, candidate, true, economy_.GetInventoryGeneration()),
                        "candidate reservation");
                }
                if (metrics_.rain > rainBefore) ++metrics_.rainHarvests;
                done[i] = true;
            }
            tools_.ClearHistory();
            if (!unfinished) break;
            Step();
            elapsed += kFrameSeconds * date_.GetTimeScale();
        }
        metrics_.activeSeconds += elapsed;
        const auto sale = economy_.SellAll();
        Check(sale.Succeeded(), "batch sale");
        metrics_.earned += sale.earnedMoney; metrics_.sold += sale.soldCount;
        Check(economy_.GetTotalCropCount() == (reserve ? 1 : 0), "reserved crop sold");
        MaybeExpand();
        return candidate;
    }
    void MaybeExpand() {
        const auto development = FarmDevelopmentSystem::Inspect(grid_, economy_);
        if (plan_.expand && development.canExpand) {
            if (exhibit_ && grid_.GetHeight() == 5) exhibit_->Write(3, grid_, economy_, date_, progression_, crop_);
            const int oldHeight = grid_.GetHeight();
            Check(oldHeight >= FarmDevelopmentSystem::kInitialHeight &&
                oldHeight < FarmDevelopmentSystem::kMaximumHeight, "expansion index");
            Check(FarmDevelopmentSystem::Expand(grid_, economy_, development.quote), "expand");
            metrics_.expansionSpent += development.cost;
            metrics_.minimumMoney = (std::min)(metrics_.minimumMoney, economy_.GetMoney());
            metrics_.expansionDays[static_cast<std::size_t>(oldHeight - FarmDevelopmentSystem::kInitialHeight)] =
                date_.GetDay();
            tools_.ClearHistory();
            PrepareLayout();
        }
    }
    Care care_;
    LayoutPlan plan_;
    farm::CropType crop_ = farm::CropType::Carrot;
    farm::FarmGrid grid_;
    FarmDateSystem date_;
    FarmProgressionSystem progression_;
    FarmStageAccessSystem access_;
    FarmEconomySystem economy_;
    FarmToolActionSystem tools_;
    FarmGrowthSystem growth_;
    farm::FarmIrrigationSystem water_;
    FarmContestDaySystem contest_;
    Metrics metrics_;
    FarmCampaignExhibit* exhibit_ = nullptr; // Optional synchronous observer; owned by main.
};
}

int main(int argc, char* argv[]) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--export-exhibit") {
            const auto root = std::filesystem::path("generated/codex_checks") / ("system_exhibit_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            FarmCampaignExhibit exhibit(root);
            Campaign campaign(Care::Managed, kPlans.front(), 1.0f, &exhibit);
            static_cast<void>(campaign.Run());
            exhibit.WriteRecovery(); exhibit.Finish();
            std::cout << "PASS: seven exhibition saves, production loader roundtrip and planted second expansion\n"
                << "EXHIBIT_ROOT=" << root.generic_string() << '\n';
            return 0;
        }
        Check(argc == 1, "usage: farm_campaign_balance_test [--export-exhibit]");
        int cases = 0;
        std::cout << "care,layout,speed,day10,day20,day30,minimum_money,final_money,seed_spent,expansion_spent,sales,"
            "expand25_day,expand30_day,harvests,quality_mean,intake_changes,preparation_watering,"
            "pest_warnings,cured,rain_harvests,active_sim_seconds,inspections\n";
        for (const auto care : {Care::Managed, Care::NoCompost}) {
            for (const auto& plan : kPlans) {
                Metrics baseline;
                for (float speed : {1.0f, 2.0f, 4.0f}) {
                    std::cerr << "RUN: " << kCareNames[static_cast<std::size_t>(care)] << '/' << plan.name << '/' << speed << '\n';
                    Campaign run(care, plan, speed);
                    const auto m = run.Run();
                    if (speed == 1) baseline = m;
                    Check(m.harvests == baseline.harvests && m.expansionDays == baseline.expansionDays &&
                        std::abs(m.money - baseline.money) <= 30, "speed-dependent economy drift");
                    for (std::size_t stage = 0; stage < m.scores.size(); ++stage) {
                        Check(std::abs(m.scores[stage] - baseline.scores[stage]) <= 2, "score drift");
                        if (care == Care::Managed) Check(m.scores[stage] >= kFarmStageRulesV1[stage].passingPoints,
                            "managed campaign misses stage goal");
                        else Check(m.scores[stage] < kFarmStageRulesV1[stage].passingPoints,
                            "repeated cultivation ignores soil depletion");
                    }
                    std::cout << kCareNames[static_cast<std::size_t>(care)] << ',' << plan.name << ',' << speed;
                    for (int score : m.scores) std::cout << ',' << score;
                    std::cout << ',' << m.minimumMoney << ',' << m.money << ',' << m.spent << ',' << m.expansionSpent << ',' << m.earned
                        << ',' << m.expansionDays[0] << ',' << m.expansionDays[1] << ',' << m.harvests << ','
                        << std::fixed << std::setprecision(2) << m.quality / m.harvests << ',' << m.intakeChanges
                        << ',' << m.watering << ',' << m.pestWarnings << ',' << m.cured << ',' << m.rainHarvests
                        << ',' << m.activeSeconds << ',' << m.inspections << '\n';
                    ++cases;
                }
            }
        }
        Check(cases == 18, "case coverage");
        std::cout << "PASS: 18 calendar campaigns with weather, pests, staged crops, development and real transactions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
