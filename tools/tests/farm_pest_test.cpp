#include "farm/system/FarmPestSystem.h"
#include "editor/FarmPestPlaytestSystem.h"
#include "editor/FarmNextGoalDebugCases.h"
#include "editor/FarmNextGoalPlaytestSystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/ui/FarmContestDetailView.h"
#include "farm/ui/FarmPestView.h"
#include "farm/ui/FarmNextGoalView.h"
#include "io/JsonFile.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

void CheckPestLabels(const farmui::View& view) {
    for (std::size_t i = 0; i < view.count; ++i) {
        const auto& item = view.items[i];
        if (item.label < farmui::Label::PestWarning) continue;
        const auto& uv = farmui::kLabels.at(static_cast<std::size_t>(item.label));
        const float textSpace = (item.value.empty() ? item.rect.width : item.valueOffset) - 20;
        Check(textSpace >= uv.width * 20.0f / 26.0f, "pest label too narrow");
        Check(item.rect.x >= 0 && item.rect.y >= 0 && item.rect.x + item.rect.width <= 1280 &&
            item.rect.y + item.rect.height <= 720, "pest label outside HUD");
    }
}

void CheckDebugPlaytest() {
    using Test = editor::FarmPestPlaytestSystem;
    using Action = Test::Action;
    using Phase = Test::Phase;
    Test test;
    Check(!test.Execute(Action::Cure) && !test.Execute(Action::AdvanceDay) &&
        !test.Execute(Action::HarvestAndJudge), "idle debug controls");
    Check(test.Execute(Action::Start) && test.Read().phase == Phase::Warning &&
        test.Read().day == 6 && test.Read().quality.pestPenalty == 0, "debug warning");
    Check(test.Execute(Action::Cure) && !test.Execute(Action::Cure) &&
        test.Read().phase == Phase::Cured && test.Execute(Action::AdvanceDay) &&
        test.Read().quality.pestPenalty == 0, "debug early cure");
    Check(test.Execute(Action::HarvestAndJudge) && test.Read().judge.totalPoints == 100 &&
        !test.Execute(Action::HarvestAndJudge) && !test.Execute(Action::AdvanceDay), "debug judged gates");

    Check(test.Execute(Action::Start) && test.Read().day == 6 &&
        test.Read().quality.score == test.Read().undamagedQuality && !test.Read().judge.IsValid(), "debug reset");
    Check(test.Execute(Action::AdvanceDay) && test.Read().phase == Phase::Infested &&
        test.Read().quality.pestPenalty == 6, "debug day-seven damage");
    Check(test.Execute(Action::AdvanceDay) && test.Execute(Action::AdvanceDay) &&
        test.Execute(Action::AdvanceDay) && !test.Execute(Action::AdvanceDay) &&
        test.Read().damageDays == 3 && test.Read().quality.pestPenalty == 18, "debug day bounds");
    Check(test.Execute(Action::Cure) && test.Read().quality.pestPenalty == 18, "debug late cure");
    Check(test.Execute(Action::HarvestAndJudge) && test.Read().phase == Phase::Judged &&
        test.Read().quality.score == 82 && test.Read().judge.totalPoints == 89, "debug contest outcome");
}

void CheckNextGoals() {
    using Kind = FarmNextGoalKind;
    using Access = FarmGoalSeedAccess;
    using Status = FarmGoalContestStatus;
    constexpr auto carrot = farm::CropType::Carrot;
    farm::FarmTile legacyTile;
    legacyTile.state = farm::FarmTileState::Watered;
    Check(FarmNextGoalSystem::InspectField(nullptr, FarmWaterAdvice::Plant) == FarmWaterAdvice::Unknown &&
        FarmNextGoalSystem::InspectField(&legacyTile, FarmWaterAdvice::Plant) == FarmWaterAdvice::Unknown,
        "no planting goal on incompatible terrain");
    for (const auto& sample : editor::kFarmNextGoalDebugCases) {
        const auto result = FarmNextGoalSystem::Evaluate(sample.facts);
        Check(result.kind == sample.expected, sample.label);
        farmui::View view;
        if (result.pestTile >= 0) farmui::BuildPestView(view, result.pestTile, true, result.kind == Kind::PestWarning);
        else farmui::BuildFarmNextGoalView(view, result, true);
        Check(view.count <= 2 && (view.count == 0) == (result.kind == Kind::None), "one next goal and optional detail");
        CheckPestLabels(view);
    }
    FarmEconomySystem economy;
    FarmNextGoalFacts reminder;
    reminder.contest = {Status::Reserved, 20};
    reminder.field = FarmWaterAdvice::Harvest;
    Check(FarmNextGoalSystem::Evaluate(reminder).kind == Kind::Harvest, "reservation must not hide harvest");
    reminder.harvestInventoryFull = true;
    Check(FarmNextGoalSystem::Evaluate(reminder).kind == Kind::InventoryFull, "reservation must not hide full storage");
    reminder.field = FarmWaterAdvice::Monitor;
    const auto waiting = FarmNextGoalSystem::Evaluate(reminder);
    Check(waiting.kind == Kind::Reserved && waiting.contestDay == 20, "remind while crops grow normally");
    reminder.contest = {Status::Today, 20};
    reminder.paused = true;
    Check(FarmNextGoalSystem::Evaluate(reminder).kind == Kind::Submit, "deadline must precede time resume");
    reminder.pestTile = 2;
    Check(FarmNextGoalSystem::Evaluate(reminder).kind == Kind::PestActive, "active pest must precede deadline");
    reminder.blocked = true;
    Check(FarmNextGoalSystem::Evaluate(reminder).kind == Kind::None, "blocked state suppresses every goal");
    economy.Initialize();
    Check(FarmNextGoalSystem::InspectSeeds(economy, carrot) == Access::Affordable, "affordable seed");
    farm::FarmRules rules;
    rules.initialMoney = 0;
    rules.initialTomatoSeedCount = 1;
    economy.Initialize(rules);
    Check(FarmNextGoalSystem::InspectSeeds(economy, carrot) == Access::ChooseCrop, "other owned seed");
    rules.initialTomatoSeedCount = 0;
    economy.Initialize(rules);
    Check(FarmNextGoalSystem::InspectSeeds(economy, carrot) == Access::Insufficient, "unaffordable seed with nothing to sell");
    Check(FarmNextGoalSystem::InspectContest(economy, 10).status == Status::None, "no impossible submission");
    FarmCropQualityResult quality;
    quality.crop = carrot;
    quality.score = 90;
    quality.basePrice = quality.salePrice = 170;
    quality.harvestSize = {1.5f, true};
    Check(economy.AddHarvest(quality, 1, 6), "contest fixture");
    Check(FarmNextGoalSystem::InspectSeeds(economy, carrot) == Access::CanSell, "funds through sale");
    Check(FarmNextGoalSystem::InspectContest(economy, 7).status == Status::None &&
        FarmNextGoalSystem::InspectContest(economy, 8).status == Status::ProtectAndReserve &&
        FarmNextGoalSystem::InspectContest(economy, 20).status == Status::None, "contest lead time and harvest period");
    const auto* record = economy.GetHarvestRecord(0);
    Check(record != nullptr, "missing contest candidate");
    const int id = record->id;
    Check(economy.SetHarvestProtection(id, true, economy.GetInventoryGeneration()), "protect goal fixture");
    Check(FarmNextGoalSystem::InspectSeeds(economy, carrot) == Access::Insufficient, "protected crop is not sellable");
    Check(FarmNextGoalSystem::InspectContest(economy, 9).status == Status::Reserve, "reserve protected candidate");
    Check(economy.SetContestReservation(id, true, economy.GetInventoryGeneration()), "reserve goal fixture");
    Check(FarmNextGoalSystem::InspectContest(economy, 9).status == Status::Reserved &&
        FarmNextGoalSystem::InspectContest(economy, 10).status == Status::Today, "reservation and deadline");
    Check(FarmContestSubmissionSystem::Submit(economy, 10, id, economy.GetInventoryGeneration()) &&
        FarmNextGoalSystem::InspectContest(economy, 10).status == Status::None, "already submitted");
    Check(FarmNextGoalSystem::InspectContest(economy, 0).status == Status::None &&
        FarmNextGoalSystem::InspectContest(economy, 31).status == Status::None, "invalid or ended contest date");
    economy.Initialize(rules);
    Check(economy.AddHarvest(quality, 1, 10), "future harvest fixture");
    Check(FarmNextGoalSystem::InspectContest(economy, 8).status == Status::None &&
        FarmNextGoalSystem::InspectContest(economy, 10).status == Status::ProtectAndReserve,
        "future record must not be recommended before its harvest day");
    economy.Initialize(rules);
    Check(economy.AddHarvest(quality, 1, 0), "unknown harvest date fixture");
    Check(FarmNextGoalSystem::InspectContest(economy, 10).status == Status::None, "unknown harvest date is not eligible");
    FarmDateSystem date;
    FarmStagePolicy policy{true};
    FarmStageAccessSystem access;
    access.Bind(date, policy);
    rules.initialTomatoSeedCount = 1;
    economy.Initialize(rules);
    economy.BindStageAccess(access);
    Check(FarmNextGoalSystem::InspectSeeds(economy, carrot) == Access::Insufficient &&
        FarmNextGoalSystem::InspectSeeds(economy, farm::CropType::Tomato) == Access::Insufficient,
        "locked seed stock must not be recommended");
    std::cout << "Next goal priorities and eligible actions passed.\n";
}

void CheckNextGoalActions() {
    using Test = editor::FarmNextGoalPlaytestSystem;
    using Action = Test::Action;
    using Kind = FarmNextGoalKind;
    Test sample;
    Test untouched;
    Check(!sample.Read().active && !sample.Execute(Action::Plant) && !sample.Execute(Action::Grow), "inactive goal sample");
    Check(sample.Execute(Action::Restart) && sample.Read().goal.kind == Kind::Till, "sample starts with till goal");
    Check(sample.Read().recommended == Action::Till, "recommendation follows actual goal");
    const int seedPrice = sample.Read().money;
    Check(seedPrice == farm::FarmRules{}.carrotSeedPrice, "sample has exactly one seed budget");
    Check(!sample.Execute(Action::Harvest) && !sample.Execute(Action::Plant) &&
        sample.Read().money == seedPrice && sample.Read().seeds == 0, "premature actions preserve inventory");
    Check(sample.Execute(Action::Till) && sample.Read().goal.kind == Kind::BuySeed &&
        !sample.Execute(Action::Till), "actual till leads to buy");
    Check(sample.Execute(Action::BuySeed) && sample.Read().goal.kind == Kind::Plant &&
        sample.Read().money == 0 && sample.Read().seeds == 1 && !sample.Execute(Action::BuySeed), "purchase debits once");
    Check(sample.Execute(Action::Plant) && sample.Read().seeds == 0 && sample.Read().goal.kind == Kind::Water &&
        !sample.Execute(Action::Plant), "plant consumes seed once");
    for (int i = 0; i < 4; ++i) Check(sample.Execute(Action::Water), "watering through real tool");
    Check(sample.Read().goal.kind == Kind::Grow, "moisture guidance updates from real forecast");
    Check(sample.Execute(Action::TogglePause) && sample.Read().goal.kind == Kind::ResumeGrowth, "paused goal");
    const auto paused = sample.Read();
    Check(!sample.Execute(Action::Grow) && sample.Read().growth == paused.growth &&
        sample.Read().moisture == paused.moisture, "pause prevents simulated growth");
    Check(sample.Execute(Action::TogglePause) && sample.Read().goal.kind == Kind::Grow, "resume goal");
    for (int step = 0; step < 32 && sample.Read().goal.kind != Kind::Harvest; ++step) {
        for (int dose = 0; dose < 6 && sample.Read().moisture < 0.6f; ++dose)
            Check(sample.Execute(Action::Water), "sample moisture upkeep");
        Check(sample.Execute(Action::Grow), "bounded real growth stepping");
    }
    Check(sample.Read().goal.kind == Kind::Harvest && !sample.Execute(Action::Grow), "mature sample stops stepping");
    Check(sample.Execute(Action::Harvest) && sample.Read().harvests == 1 &&
        sample.Read().goal.kind == Kind::SellForSeeds && !sample.Execute(Action::Harvest), "harvest returns seed-funding advice");
    Check(sample.Execute(Action::Sell) && sample.Read().harvests == 0 && sample.Read().money >= seedPrice &&
        sample.Read().goal.kind == Kind::BuySeed && !sample.Execute(Action::Sell), "sale makes next planting possible");
    Check(sample.Execute(Action::NoFunds) && sample.Read().goal.kind == Kind::NeedFunds &&
        !sample.Execute(Action::BuySeed) && !sample.Execute(Action::Plant) && !sample.Execute(Action::Sell), "no impossible funding action");
    Check(!sample.Read().recommended, "no recommendation for unavailable action");
    Check(!sample.Execute(Action::Count) && !sample.Execute(static_cast<Action>(-1)) && sample.Read().money == 0,
        "invalid actions rejected without indexing");
    Check(sample.Execute(Action::Restart) && sample.Read().goal.kind == Kind::Till &&
        sample.Read().seeds == 0 && sample.Read().harvests == 0 && sample.Read().growth == 0.0f &&
        sample.Read().money == seedPrice && !sample.Read().paused && !untouched.Read().active, "reset and sample ownership");

    Test guided;
    Check(guided.Execute(Action::Restart), "guided sample starts");
    bool sold = false;
    for (int i = 0; i < 160 && !sold; ++i) {
        const auto view = guided.Read();
        Check(view.recommended && view.Can(*view.recommended), "recommended action is enabled");
        if (!view.recommended) break;
        sold = *view.recommended == Action::Sell;
        Check(guided.Execute(*view.recommended), "one-button guided action executes");
    }
    Check(sold && guided.Read().harvests == 0 && guided.Read().money >= seedPrice,
        "guided sequence reaches sale without manual state edits");

    farm::FarmGrid grid;
    FarmEconomySystem economy;
    economy.Initialize();
    FarmNextGoalContext context;
    context.waterAdvice = FarmWaterAdvice::Plant;
    context.selectedCrop = farm::CropType::Carrot;
    Check(FarmNextGoalSystem::Evaluate(FarmNextGoalSystem::Inspect(grid, economy, context)).kind == Kind::SelectSoil,
        "shared collection rejects nonexistent selection");
    Check(grid.Initialize(1, 1), "collector grid");
    farm::FarmTile tile;
    tile.state = farm::FarmTileState::Planted;
    tile.crop = context.selectedCrop;
    Check(grid.SetTile(0, tile) && FarmPestSystem::AdvanceDays(grid, 5, 6, true).warningStarted, "collector pest setup");
    context.pestsEnabled = true;
    auto goal = FarmNextGoalSystem::Evaluate(FarmNextGoalSystem::Inspect(grid, economy, context));
    Check(goal.kind == Kind::PestWarning && goal.pestTile == 0, "shared collection sees active pest");
    context.blocked = true;
    Check(FarmNextGoalSystem::Evaluate(FarmNextGoalSystem::Inspect(grid, economy, context)).kind == Kind::None,
        "shared collection suppresses locked state");
    context.blocked = context.pestsEnabled = false;
    context.waterAdvice = FarmWaterAdvice::Water;
    Check(FarmNextGoalSystem::Evaluate(FarmNextGoalSystem::Inspect(grid, economy, context)).kind == Kind::Water,
        "disabled pest policy preserves ordinary guidance");
    std::cout << "Next goal real-action sequence and shared collection passed.\n";
}

int ScorePath(bool cured) {
    using Crop = farm::CropType;
    using Mode = FarmProgressionMode;
    farm::FarmGrid grid;
    Check(grid.Initialize(5, 4), "initialize grid");
    farm::FarmTile plant;
    plant.state = farm::FarmTileState::Planted;
    plant.crop = Crop::Carrot;
    plant.growth = 1.0f;
    plant.moisture = 0.65f;
    plant.heightLevel = 1;
    plant.careHistory.goodSeconds = 20.0f;
    plant.careHistory.efficiencySeconds = 20.0f;
    plant.careHistory.nutrientGrowth = 1.0f;
    plant.careHistory.nutrientSupply = 1.0f;
    Check(grid.SetTile(2, plant) && grid.SetTile(3, plant), "set crop fixtures");
    Check(!FarmPestSystem::AdvanceDays(grid, 5, 6, false).changed, "legacy farm spawned pest");
    Check(!FarmPestSystem::AdvanceDays(grid, 5, 5, true).changed, "same-day update mutated");
    const auto warning = FarmPestSystem::AdvanceDays(grid, 5, 6, true);
    Check(warning.changed && warning.warningStarted && FarmPestSystem::FindActive(grid) == 2 &&
        grid.GetTile(2)->pests.damageDays == 0, "single warning without damage");

    farmui::View view;
    farmui::BuildPestView(view, 2, false, true);
    const auto focus = view.Hit({600, 40});
    Check(focus.action == farmui::Action::PestFocus && focus.argument == 2, "warning hit target");
    CheckPestLabels(view);
    view = {};
    farmui::BuildPestView(view, 2, true, true);
    Check(view.Hit({600, 40}).action == farmui::Action::PestCure, "cure hit target");
    CheckPestLabels(view);
    Check(!FarmPestSystem::Cure(grid, -1, 6) && !FarmPestSystem::Cure(grid, 3, 6) &&
        !FarmPestSystem::Cure(grid, 2, 16), "invalid cure");

    FarmDateSystem date;
    Check(date.RestoreSnapshot({6, 0, 4}), "set clock");
    FarmProgressionSystem progression;
    progression.Initialize({}, Mode::ContestSeason, true);
    FarmEconomySystem economy;
    economy.Initialize();
    FarmCropSelectionSystem selection;
    selection.Initialize();
    FarmDocumentSystem docs;
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::path("generated/codex_checks") /
        ("pests_" + std::to_string(stamp) + (cured ? "_cured" : "_ignored"));
    Check(docs.Initialize(root.string(), grid, economy, selection, &date, &progression) &&
        docs.SaveAs("PestTest", grid, economy, selection), "save warning");
    const auto id = docs.GetActiveDocumentId();
    const auto path = docs.GetPath();
    Check(FarmPestSystem::Cure(grid, 2, 6) && docs.Load(id, grid, economy, selection) &&
        progression.PestsEnabled() && date.GetDay() == 6 && grid.GetTile(2)->pests.warningDay == 6 &&
        grid.GetTile(2)->pests.damageDays == 0, "warning round-trip");

    nlohmann::json saved;
    Check(JsonFile::Load(path, saved), "read test save");
    for (int kind = 0; kind < 5; ++kind) {
        auto bad = saved;
        if (kind == 0) bad["tiles"][2]["pests"]["warningDay"] = 16;
        if (kind == 1) bad["tiles"][2]["pests"]["damageDays"] = 1;
        if (kind == 2) bad["tiles"][3]["pests"] = bad["tiles"][2]["pests"];
        if (kind == 3) bad["pestsEnabled"] = false;
        if (kind == 4) bad["tiles"][2]["pests"]["damageDays"] = 4294967296LL;
        Check(JsonFile::Save(path, bad) && !docs.Load(id, grid, economy, selection) &&
            grid.GetTile(2)->pests.warningDay == 6 && date.GetDay() == 6, "invalid save accepted or mutated");
    }
    auto legacy = saved;
    legacy["schemaVersion"] = 18;
    legacy.erase("pestsEnabled");
    for (auto& tile : legacy["tiles"]) tile.erase("pests");
    Check(JsonFile::Save(path, legacy) && docs.Load(id, grid, economy, selection) &&
        !progression.PestsEnabled() && progression.StagePolicy().enabled &&
        FarmPestSystem::FindActive(grid) < 0, "schema 18 migration");
    Check(JsonFile::Save(path, saved) && docs.Load(id, grid, economy, selection), "restore test save");

    if (cured) Check(FarmPestSystem::Cure(grid, 2, 6) && !FarmPestSystem::Cure(grid, 2, 6), "duplicate cure");
    date.Update(15.0f);
    Check(date.GetDay() == 7, "4x boundary");
    Check(FarmPestSystem::AdvanceDays(grid, 6, 7, true).changed == !cured, "first damage");
    Check(!FarmPestSystem::AdvanceDays(grid, 6, 7, true).changed, "duplicate damage");
    Check(docs.Save(grid, economy, selection) && docs.Load(id, grid, economy, selection) &&
        grid.GetTile(2)->pests.damageDays == (cured ? 0 : 1), "damage round-trip");
    Check(FarmPestSystem::AdvanceDays(grid, 7, 10, true).changed == !cured &&
        grid.GetTile(2)->pests.damageDays == (cured ? 0 : 3), "multi-day damage cap");
    if (!cured) Check(FarmPestSystem::Cure(grid, 2, 6) &&
        grid.GetTile(2)->pests.damageDays == 3, "late cure erased damage");

    FarmToolActionSystem tools;
    tools.Initialize();
    Check(grid.SetSelectedIndex(2), "select crop");
    const auto harvest = tools.ApplyToolDetailed(grid, FarmTool::Harvest, Crop::Carrot, economy, 10);
    Check(harvest.Succeeded() && harvest.harvestQuality.pestPenalty == (cured ? 0 : 18) &&
        grid.GetTile(2)->pests == farm::FarmPestState{}, "harvest consumes pest state");
    const auto* record = economy.GetHarvestRecord(0);
    Check(record != nullptr, "missing harvest record");
    const int recordId = record->id;
    Check(economy.SetHarvestProtection(recordId, true, economy.GetInventoryGeneration()) &&
        economy.SetContestReservation(recordId, true, economy.GetInventoryGeneration()) &&
        FarmContestSubmissionSystem::Submit(economy, 10, recordId,
        economy.GetInventoryGeneration()), "submit crop");
    const auto result = economy.GetContestResults()[0];
    Check(date.RestoreSnapshot({10, 0, 1}) && docs.Save(grid, economy, selection) &&
        docs.Load(id, grid, economy, selection), "round-trip contest");
    const auto& reloaded = economy.GetContestResults()[0];
    Check(reloaded.qualityPoints == result.qualityPoints && reloaded.sizePoints == result.sizePoints &&
        reloaded.harvest.quality.pestPenalty == result.harvest.quality.pestPenalty, "contest result changed after reload");
    FarmContestPresentationSystem::Frame frame;
    frame.comparison.quality = reloaded.harvest.quality;
    frame.adviceReveal = 1.0f;
    view = {};
    farmui::BuildContestDetailView(view, frame);
    bool showedPenalty = false;
    for (std::size_t i = 0; i < view.count; ++i)
        if (view.items[i].label == farmui::Label::PestPenalty) showedPenalty = true;
    Check(showedPenalty == !cured, "contest omitted or fabricated pest penalty");
    CheckPestLabels(view);
    std::cout << (cured ? "Cured" : "Ignored") << ": crop quality " << harvest.harvestQuality.score
        << ", contest " << result.qualityPoints + result.sizePoints << '\n';
    return result.qualityPoints + result.sizePoints;
}
}

int main() {
    try {
        CheckDebugPlaytest();
        CheckNextGoals();
        CheckNextGoalActions();
        const int cured = ScorePath(true);
        const int ignored = ScorePath(false);
        Check(cured > ignored, "damage has no contest impact");
        std::cout << "Farm pest tests passed.\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
