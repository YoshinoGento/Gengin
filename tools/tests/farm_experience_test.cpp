#include "farm/system/FarmExperienceSystem.h"
#include "farm/system/FarmDevelopmentSystem.h"
#include "farm/system/FarmRecoverySystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmSimulationStepSystem.h"
#include "farm/system/FarmContestOutcomeSystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "io/JsonFile.h"
#include "farm_forecast_fixture.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
using Topic = FarmExperienceSystem::Topic;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Fixture {
    farm::FarmGrid grid;
    FarmEconomySystem economy;
    FarmDateSystem date;
    FarmProgressionSystem progression;
    FarmStageAccessSystem access;
    FarmCropSelectionSystem selection;
    FarmDocumentSystem documents;
    explicit Fixture(const fs::path& root) {
        Check(grid.Initialize(5, 4), "initialize grid");
        economy.Initialize(); date.Initialize(); selection.Initialize();
        progression.Initialize({}, FarmProgressionMode::FreeFarming);
        Check(documents.Initialize(root.string(), grid, economy, selection, &date, &progression), "initialize document");
        access.Bind(date, progression.StagePolicy()); economy.BindStageAccess(access);
    }
    bool Load(const fs::path& path) { return documents.LoadPrepared(path.string(), grid, economy, selection); }
};
fs::path FindSave(const fs::path& root, const char* name) {
    for (const auto& entry : fs::directory_iterator(root / name / "farm/saves"))
        if (entry.path().extension() == ".json") return entry.path();
    throw std::runtime_error("source scenario missing");
}
farm::FarmTile Crop(int kind, float growth, float moisture) {
    farm::FarmTile tile;
    tile.state = farm::FarmTileState::Planted; tile.crop = farm::PlayableCrop(kind);
    tile.heightLevel = kind == 2 ? 0 : 1; tile.moisture = moisture; tile.growth = growth;
    tile.soilNutrients = 0.8f; tile.irrigationEnabled = false;
    tile.careHistory.goodSeconds = 20 * growth; tile.careHistory.efficiencySeconds = 20 * growth;
    tile.careHistory.nutrientGrowth = growth; tile.careHistory.nutrientSupply = growth * 0.8f;
    return tile;
}
// Authored exhibition crops exercise real harvest/reservation/submission, not natural-play balance.
void PrepareCampaign(Fixture& f, Topic topic) {
    Check(f.grid.Initialize(5, 4), "fresh campaign grid");
    f.date.Initialize(); f.economy.Initialize();
    f.progression.Initialize({}, FarmProgressionMode::ContestSeason, true, true, true);
    Check(f.selection.SetSelectedCrop(farm::CropType::Carrot), "initial crop");
    if (topic == Topic::CampaignStart) return;
    FarmToolActionSystem tools; tools.Initialize(); tools.BindStageAccess(f.access);
    for (int stage = 0; stage < 3; ++stage) {
        if (stage == 2 && topic == Topic::FinalMissed) break;
        const int day = FarmContestEntrySystem::kContestDays[stage];
        Check(f.date.RestoreSnapshot({day, 0, 1}), "exhibit date");
        auto tile = Crop(stage, 1, stage == 1 ? .55f : .65f);
        if (stage == 2 && topic == Topic::FinalFailure) tile.careHistory.efficiencySeconds *= .3f;
        Check(f.grid.SetTile(6, tile) && f.grid.SelectTile(6), "exhibit crop");
        Check(tools.ApplyToolDetailed(f.grid, FarmTool::Harvest, tile.crop, f.economy, day).Succeeded(), "exhibit harvest");
        const auto* record = f.economy.GetHarvestRecord(f.economy.GetHarvestRecordCount() - 1);
        Check(record != nullptr, "exhibit record");
        const int id = record->id;
        Check(tools.CommitHarvestProtection(f.economy, id, true, f.economy.GetInventoryGeneration()) &&
            tools.CommitContestReservation(f.economy, id, true, f.economy.GetInventoryGeneration()), "exhibit reservation");
        if (stage < 2) Check(FarmContestSubmissionSystem::Submit(f.economy, day, id,
            f.economy.GetInventoryGeneration()), "earlier exhibit submission");
    }
    if (topic == Topic::FinalMissed) {
        Check(f.date.RestoreSnapshot({31, 0, 1}), "missed deadline");
        static_cast<void>(f.progression.EvaluateSeason(FarmContestSeasonSystem::Evaluate(31, f.economy.GetContestResults())));
    }
    for (int i = 0; i < 3; ++i) Check(f.grid.SetTile(6 + i, Crop(i, 1, .65f)), "display crops");
    auto progression = f.progression.CaptureSnapshot();
    progression.pestSpread.lastProcessedDay = f.date.GetDay();
    Check(f.progression.RestoreSnapshot(progression), "exhibit pest clock");
    Check(f.selection.SetSelectedCrop(farm::CropType::Pumpkin), "final crop");
}
// These are deliberately prepared examples, not claimed natural-play measurements.
void Prepare(Fixture& f, int index) {
    const auto topic = static_cast<Topic>(index);
    if (!FarmExperienceSystem::kEntries[index].contest) {
        Check(f.progression.SetMode(FarmProgressionMode::FreeFarming, f.economy.GetMoney(),
            FarmContestSeasonSystem::Evaluate(f.date.GetDay(), f.economy.GetContestResults())), "free mode");
        for (int i = 0; i < f.grid.GetTileCount(); ++i) {
            auto tile = *f.grid.GetTile(i); tile.pests = {};
            Check(f.grid.SetTile(i, tile), "clear inactive pests");
        }
    }
    if (topic == Topic::Plant || topic == Topic::Records) {
        for (int i : {6, 7, 8, 11, 12, 13}) {
            farm::FarmTile tile; tile.state = farm::FarmTileState::Tilled;
            tile.heightLevel = 1; tile.moisture = 0.55f;
            Check(f.grid.SetTile(i, tile), "prepare seed beds");
        }
        auto e = f.economy.CaptureSnapshot(); e.money = 300; e.seedCounts = {0, 5, 5, 5};
        Check(f.economy.RestoreSnapshot(e), "prepare seeds");
    }
    if (topic == Topic::Harvest || topic == Topic::Trade || topic == Topic::Book) {
        for (int i = 0; i < 3; ++i) {
            Check(f.grid.SetTile(6 + i, Crop(i, 1, i == 1 ? 0.55f : 0.65f)), "prepare ripe crop");
        }
        if (topic != Topic::Harvest) {
            FarmToolActionSystem tools; tools.Initialize();
            for (int i = 0; i < 3; ++i) {
                Check(f.grid.SelectTile(6 + i) && tools.ApplyToolDetailed(f.grid, FarmTool::Harvest,
                    farm::PlayableCrop(i), f.economy, f.date.GetDay()).Succeeded(), "real harvest");
            }
            const auto* first = f.economy.GetHarvestRecord(0);
            Check(first && f.economy.SetHarvestProtection(first->id, true, f.economy.GetInventoryGeneration()), "protect sample");
        }
    }
    if (topic == Topic::Soil) {
        farm::FarmTile tile; tile.state = farm::FarmTileState::Tilled;
        tile.heightLevel = 1; tile.moisture = 0.6f; tile.soilNutrients = 0.02f;
        Check(f.grid.SetTile(6, tile), "depleted soil");
    }
    if (topic == Topic::Rain) {
        Check(f.date.RestoreSnapshot({18, 0, 1}), "start of rainy tomato day");
        auto stage = f.progression.CaptureSnapshot();
        stage.pestSpread.lastProcessedDay = f.date.GetDay();
        Check(f.progression.RestoreSnapshot(stage), "rain fixture pest clock");
        auto tomato = Crop(1, 0.10f, 0.70f);
        tomato.mold = {farm::FarmMoldState::kWarningSeconds, tomato.moisture, false, false};
        tomato.irrigationEnabled = true;
        Check(f.grid.SetTile(6, tomato) && f.grid.SetTile(8, tomato) &&
            f.selection.SetSelectedCrop(tomato.crop), "matching rain beds");
    }
    if (topic == Topic::Compare) {
        Check(f.grid.SetTile(6, Crop(1, 0.1f, 0.55f)) && f.grid.SetTile(8, Crop(1, 0.1f, 0)), "comparison pair");
    }
    if (topic == Topic::Repellent) {
        const int active = FarmPestSystem::FindActive(f.grid);
        Check(active >= 0, "active pests");
        const int neighbor = active % f.grid.GetWidth() < f.grid.GetWidth() - 1 ? active + 1 : active - 1;
        Check(f.grid.SetTile(neighbor, Crop(0, 0.05f, 0.55f)), "spread target");
        farm::FarmTile empty;
        // A free tile next to the crop for the existing repellent placement action.
        const int support = neighbor + f.grid.GetWidth();
        if (f.grid.GetTile(support)) Check(f.grid.SetTile(support, empty), "repellent location");
    }
    if (topic != Topic::Pests && topic != Topic::Repellent) Check(f.grid.SelectTile(6), "selected bed");
    f.date.SetTimeScale(1);
}
void ValidateAction(Fixture& f, int index) {
    FarmToolActionSystem tools; tools.Initialize(); tools.BindStageAccess(f.access);
    switch (static_cast<Topic>(index)) {
    case Topic::CampaignStart:
        Check(f.date.GetDay() == 1 && f.economy.GetMoney() == 300 && !f.progression.IsCleared() &&
            f.progression.StagePolicy().enabled && f.progression.PestsEnabled() &&
            f.progression.WeatherVersion() == 1 && f.grid.GetTileCount() == 20 &&
            !f.economy.IsCropUnlocked(farm::CropType::Pumpkin), "fresh staged campaign");
        Check(f.grid.SelectTile(6) && tools.ApplyToolDetailed(f.grid, FarmTool::Hoe, farm::CropType::Carrot,
            f.economy, 1).Succeeded() && tools.CompostSelectedTile(f.grid) &&
            f.economy.BuySeed(farm::CropType::Carrot).Succeeded() &&
            tools.ApplyToolDetailed(f.grid, FarmTool::Seed, farm::CropType::Carrot, f.economy, 1).Succeeded(), "new game first crop");
        break;
    case Topic::FinalSubmission: case Topic::FinalFailure: {
        Check(f.date.GetDay() == 30 && !f.progression.IsCleared(), "final is not pre-cleared");
        const int id = f.economy.GetContestReservationId();
        const auto generation = f.economy.GetInventoryGeneration();
        Check(FarmContestSubmissionSystem::Submit(f.economy, 30, id, generation), "normal final submission");
        Check(!FarmContestSubmissionSystem::Submit(f.economy, 30, id, generation), "duplicate final submission");
        Check(f.progression.EvaluateSeason(FarmContestSeasonSystem::Evaluate(30, f.economy.GetContestResults())), "final session stop");
        FarmContestOutcomeSystem outcome;
        outcome.Observe(30, f.economy.GetContestResults(), true, true);
        const auto expected = static_cast<Topic>(index) == Topic::FinalSubmission ?
            FarmContestOutcomeSystem::Ending::Cleared : FarmContestOutcomeSystem::Ending::Failed;
        Check(outcome.GetFrame().ending == expected, "separate success and failure");
        auto results = f.economy.GetContestResults();
        for (int score : {84, 85}) {
            results.back().qualityPoints = score - 40; results.back().sizePoints = 40;
            outcome.Observe(30, results, true, true);
            Check(outcome.GetFrame().ending == (score == 85 ? FarmContestOutcomeSystem::Ending::Cleared :
                FarmContestOutcomeSystem::Ending::Failed), "84/85 boundary");
        }
        outcome.Observe(30, results, true, false);
        Check(outcome.GetFrame().ending == FarmContestOutcomeSystem::Ending::LegacyCompleted, "legacy not falsely cleared");
        outcome.Observe(30, results, false, false);
        Check(outcome.GetFrame().ending == FarmContestOutcomeSystem::Ending::Pending, "free has no ending");
        break;
    }
    case Topic::FinalMissed: {
        FarmContestOutcomeSystem outcome;
        outcome.Observe(f.date.GetDay(), f.economy.GetContestResults(), true, true);
        Check(f.progression.IsCleared() && outcome.GetFrame().ending == FarmContestOutcomeSystem::Ending::Missed,
            "missed is not success"); break;
    }
    case Topic::Plant: case Topic::Records:
        Check(tools.ApplyToolDetailed(f.grid, FarmTool::Seed, farm::CropType::Carrot, f.economy, f.date.GetDay()).Succeeded(), "plant action"); break;
    case Topic::Harvest:
        Check(tools.ApplyToolDetailed(f.grid, FarmTool::Harvest, farm::CropType::Carrot, f.economy, f.date.GetDay()).Succeeded(), "harvest action"); break;
    case Topic::Soil: Check(tools.CompostSelectedTile(f.grid), "compost action"); break;
    case Topic::Trade: {
        const int protectedCount = f.economy.GetProtectedCropCount();
        Check(protectedCount > 0 && f.economy.SellAll().Succeeded() &&
            f.economy.GetTotalCropCount() == protectedCount, "protected sale"); break;
    }
    case Topic::Rain: {
        const auto rain = FarmWeatherSystem::Inspect(f.date.GetDay(), f.progression.WeatherVersion());
        const auto* a = f.grid.GetTile(6);
        const auto* b = f.grid.GetTile(8);
        Check(rain.phase == FarmWeatherSystem::Phase::Rain && f.economy.IsCropUnlocked(farm::CropType::Tomato) &&
            a && b && a->crop == farm::CropType::Tomato && b->crop == a->crop && a->moisture == b->moisture &&
            a->growth == b->growth && a->irrigationEnabled && b->irrigationEnabled && f.grid.GetSelectedIndex() == 6,
            "comparable unlocked tomato beds");
        using M = farm::MoldPhase;
        Check(a->mold.Phase() == M::Warning && a->mold == b->mold, "both start at the warning, not illness");
        Check(tools.SetSelectedIrrigation(f.grid, false), "close the left intake");
        farm::FarmIrrigationSystem canal; canal.Initialize();
        FarmGrowthSystem growth; growth.Initialize();
        for (int tick = 0; tick < 300; ++tick) {
            canal.UpdateWater(f.grid, 1.f / 60, 1, rain.soilRainPerSecond);
            growth.Update(f.grid, 1.f / 60, 1);
            if (tick == 0) Check(f.grid.GetTile(6)->mold.Phase() == M::Recovering &&
                f.grid.GetTile(8)->mold.Phase() == M::Warning, "relief on first drying step");
        }
        a = f.grid.GetTile(6); b = f.grid.GetTile(8);
        Check(growth.Evaluate(*a).moistureStatus == FarmMoistureStatus::Good &&
            growth.Evaluate(*b).moistureStatus == FarmMoistureStatus::Excess && a->growth > b->growth,
            "rain-only benefit visible within five seconds");
        for (int tick = 300; tick < 21 * 60; ++tick) {
            canal.UpdateWater(f.grid, 1.f / 60, 1, rain.soilRainPerSecond);
            growth.Update(f.grid, 1.f / 60, 1);
        }
        Check(f.grid.GetTile(6)->mold.Phase() == M::Healthy && f.grid.GetTile(8)->mold.Phase() == M::Sick,
            "untreated plant sick, rain-only plant recovered");
        break;
    }
    case Topic::Pests: case Topic::Repellent: Check(FarmPestSystem::FindActive(f.grid) >= 0, "pests present"); break;
    case Topic::Judging: Check(f.economy.GetContestReservation() != nullptr && f.date.GetDay() == 10, "ready to submit"); break;
    case Topic::Results: Check(f.economy.GetContestResults()[0].contestDay == 10, "result recorded"); break;
    case Topic::Development: {
        const auto offer = FarmDevelopmentSystem::Inspect(f.grid, f.economy);
        Check(offer.canExpand && FarmDevelopmentSystem::Expand(f.grid, f.economy, offer.quote), "expansion action"); break;
    }
    case Topic::Book: Check(f.economy.GetHarvestRecordCount() >= 3, "book records"); break;
    case Topic::Recovery: Check(FarmRecoverySystem::Inspect(f.grid, f.economy, f.date.GetDay()).CanClaim(), "aid availability"); break;
    default: break;
    }
}
int main(int argc, char** argv) {
    try {
        const bool generate = argc == 4 && std::string_view(argv[1]) == "--generate";
        const bool generateCampaign = argc == 2 && std::string_view(argv[1]) == "--generate-campaign";
        const bool forecastReview = argc == 2 && std::string_view(argv[1]) == "--forecast-review";
        Check(generate || generateCampaign || forecastReview || argc == 1, "usage: farm_experience_test [--generate exportedRoot outputRoot | --generate-campaign | --forecast-review]");
        const auto temp = fs::path("generated/codex_checks") / ("experience_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (forecastReview) {
            const fs::path folder = temp / "forecast";
            Fixture f(folder);
            Check(forecast_fixture::Build(f.grid) &&
                f.documents.SaveAs("水路の分岐",f.grid,f.economy,f.selection),"save review board");
            Fixture reopened(folder);
            Check(reopened.grid.GetSelectedIndex() == forecast_fixture::branch &&
                reopened.grid.GetTile(forecast_fixture::loses)->crop == farm::CropType::Tomato &&
                reopened.progression.IsFreeFarming(), "reload review board");
            std::cout << "Forecast review: " << folder.string() << '\n';
            return 0;
        }
        const auto assets = generate ? fs::path(argv[3]) : fs::path("project/Resources/farm/experience");
        fs::create_directories(assets);
        constexpr std::array source{"01_water", "01_water", "01_water", "01_water", "01_water", "03_rain", "02_pests", "02_pests",
            "01_water", "05_judging", "07_results", "04_expansion", "01_water", "01_water", "06_recovery", "01_water"};
        for (int i = 0; i < FarmExperienceSystem::kCount; ++i) {
            const auto name = FarmExperienceSystem::kEntries[i].file;
            const auto output = assets / (std::string(name) + ".json");
            const bool campaign = i >= static_cast<int>(Topic::CampaignStart);
            if (generate || (generateCampaign && campaign)) {
                Fixture prepared(temp / "export" / name);
                if (campaign) PrepareCampaign(prepared, static_cast<Topic>(i));
                else { Check(prepared.Load(FindSave(argv[2], source[i])), "load source"); Prepare(prepared, i); }
                Check(prepared.documents.SaveAs(std::string("体験用 ") + name, prepared.grid, prepared.economy, prepared.selection), "serialize prepared");
                nlohmann::json json;
                Check(JsonFile::Load(prepared.documents.GetPath(), json), "read prepared");
                Check(JsonFile::Save(output.string(), json), "export prepared");
            }
            Fixture f(temp / "validate" / name);
            Check(f.documents.SaveAs("Original", f.grid, f.economy, f.selection), "original save");
            const auto originalId = f.documents.GetActiveDocumentId();
            const auto originalPath = f.documents.GetPath();
            nlohmann::json original, catalog, after, catalogAfter;
            const auto catalogPath = temp / "validate" / name / "farm_documents.json";
            Check(JsonFile::Load(originalPath, original) && JsonFile::Load(catalogPath.string(), catalog), "original backup");
            Check(f.Load(output) && f.documents.IsPrepared() && f.documents.IsDirty() &&
                f.documents.GetActiveDocumentId().empty() && f.documents.GetPath().empty(), "unsaved prepared state");
            Check(f.progression.IsContestSeason() == FarmExperienceSystem::kEntries[i].contest, "mode label agreement");
            Check(!f.documents.Save(f.grid, f.economy, f.selection), "must not overwrite original");
            Check(JsonFile::Load(originalPath, after) && after == original &&
                JsonFile::Load(catalogPath.string(), catalogAfter) && catalogAfter == catalog, "original/catalog unchanged");
            const auto count = f.grid.GetTileCount(); const auto money = f.economy.GetMoney();
            Check(!f.Load(temp / "missing.json") && f.grid.GetTileCount() == count && f.economy.GetMoney() == money && f.documents.IsPrepared(), "failure preserves state");
            nlohmann::json malformed;
            Check(JsonFile::Load(output.string(), malformed), "read validation sample");
            malformed["schemaVersion"] = 999999;
            const auto invalidPath = temp / "invalid.json";
            Check(JsonFile::Save(invalidPath.string(), malformed), "write invalid sample");
            Check(!f.Load(invalidPath) && f.grid.GetTileCount() == count && f.economy.GetMoney() == money &&
                f.documents.IsPrepared() && f.documents.GetPath().empty(), "invalid sample preserves prepared state");
            Check(f.documents.SaveAs("Practice copy", f.grid, f.economy, f.selection), "explicit copy");
            const auto copyId = f.documents.GetActiveDocumentId();
            Check(copyId != originalId && f.documents.Load(copyId, f.grid, f.economy, f.selection) && !f.documents.IsPrepared(), "copy reload");
            Check(f.Load(output), "reload prepared"); ValidateAction(f, i);
            Check(f.documents.Load(originalId, f.grid, f.economy, f.selection) && !f.documents.IsPrepared(), "restore original");
            std::cout << "PASS: " << name << '\n';
        }
        Fixture switching(temp / "switching");
        for (int index : {0, 11, 0, 11, 9, 10, 14}) {
            Check(switching.Load(assets / (std::string(FarmExperienceSystem::kEntries[index].file) + ".json")), "switch differently sized farms");
            Check(switching.grid.GetTileCount() == (index == 11 ? 25 : 20), "switch dimensions");
        }
        Check(!FarmExperienceSystem::Valid(-1) && !FarmExperienceSystem::Valid(FarmExperienceSystem::kCount) && FarmExperienceSystem::Path(-1).empty(), "catalog bounds");
        std::cout << "PASS: " << FarmExperienceSystem::kCount << " experiences, production actions, immutable originals, save-copy/load\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
