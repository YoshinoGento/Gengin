#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmLayoutSystem.h"
#include "farm/ui/FarmSeedShopView.h"
#include "farm/ui/FarmPlayFlowView.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
using Crop = farm::CropType;
using Mode = FarmProgressionMode;
using Operation = farm::FarmIrrigationPreviewOperation;

void CheckView(const farmui::View& view) {
    Check(view.count < view.items.size(), "view capacity");
    for (std::size_t i = 0; i < view.count; ++i) {
        const auto& item = view.items[i];
        const auto& r = item.rect;
        Check(r.x >= 0 && r.y >= 0 && r.width > 0 && r.height > 0 &&
            r.x + r.width <= 1280 && r.y + r.height <= 720, "view bounds");
        Check(static_cast<std::size_t>(item.label) < farmui::kLabels.size(), "atlas index");
        const float space = item.value.empty() ? r.width - 20 : item.valueOffset - 30;
        const auto& uv = farmui::kLabels[static_cast<std::size_t>(item.label)];
        if ((std::min)(1.0f, space / uv.width) < 20.0f / 26.0f) {
            std::cerr << "Unreadable label=" << static_cast<int>(item.label) << " at " << r.x << ',' << r.y << " space=" << space << " width=" << uv.width << '\n';
            Check(false, "label readability");
        }
        Check(item.value.empty() || item.valueOffset + 14 * (item.value.size() - 1) + 24 <= r.width, "value readability");
    }
}

void Run() {
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({}, Mode::ContestSeason);
    FarmStageAccessSystem access; access.Bind(date, progression.StagePolicy());
    FarmEconomySystem economy; economy.Initialize(); economy.BindStageAccess(access);
    FarmToolActionSystem tools; tools.Initialize(); tools.BindStageAccess(access);
    farm::FarmIrrigationPreviewSystem preview; preview.Initialize(); preview.BindStageAccess(access);
    farm::FarmGrid grid; Check(grid.Initialize(5, 4) && grid.SelectTile(0), "grid fixture");
    FarmCropSelectionSystem selection; selection.Initialize();
    const auto setDay = [&](int day) { Check(date.RestoreSnapshot({day, 0, 1}), "date fixture"); };
    for (const int day : {1, 10, 11, 20, 21, 30, 31}) {
        setDay(day);
        Check(economy.IsCropUnlocked(Crop::Carrot), "carrot access");
        Check(economy.IsCropUnlocked(Crop::Tomato) == (day >= 11), "tomato boundary");
        Check(economy.IsCropUnlocked(Crop::Pumpkin) == (day >= 21), "pumpkin boundary");
        Check(tools.MaximumAllowedHeight() == (day >= 11 ? 2 : 1), "height boundary");
    }
    setDay(1);
    const auto generation = economy.GetInventoryGeneration();
    Check(!economy.BuySeed(Crop::Tomato).Succeeded() && economy.GetMoney() == 300 &&
        economy.GetInventoryGeneration() == generation, "locked purchase mutated state");
    Check(!economy.BuySeed(static_cast<Crop>(255)).Succeeded(), "invalid crop");
    Check(tools.EvaluateTool(grid, FarmTool::Seed, Crop::Tomato, &economy).status == FarmToolActionStatus::Locked,
        "plant gate");
    Check(tools.RaiseSelectedTile(grid) && !tools.RaiseSelectedTile(grid) && grid.GetSelectedTile()->heightLevel == 1,
        "height gate");
    Check(!preview.Begin(grid, 0, Operation::RaiseTerrain), "locked preview");
    setDay(11);
    Check(preview.Begin(grid, 0, Operation::RaiseTerrain) && preview.CanConfirm(grid), "unlocked preview");
    setDay(10);
    Check(!preview.CanConfirm(grid), "stale preview confirmation");
    preview.Cancel();
    setDay(11);
    Check(tools.RaiseSelectedTile(grid) && tools.Undo(), "height history fixture");
    setDay(10);
    Check(!tools.Redo() && grid.GetSelectedTile()->heightLevel == 1, "history bypass");
    setDay(11);
    FarmSeedShopSystem shop; shop.Open(Crop::Tomato);
    Check(shop.BeginConfirmation(economy), "shop quote");
    setDay(10);
    Check(!shop.Confirm(economy).Succeeded() && economy.GetMoney() == 300, "stale shop confirmation");
    Check(shop.Evaluate(economy).unlockDay == 11, "shop unlock date");
    farmui::View shopView{}; farmui::BuildSeedShopView(shopView, shop, economy); CheckView(shopView);

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = std::filesystem::path("generated/codex_checks") / ("stage_" + std::to_string(stamp));
    FarmDocumentSystem docs;
    Check(docs.Initialize((root / "saves").string(), grid, economy, selection, &date, &progression), "document init");
    Check(docs.SaveAs("StageTest", grid, economy, selection), "stage save");
    nlohmann::json saved;
    Check(JsonFile::Load(docs.GetPath(), saved) && saved["schemaVersion"] == 22 && saved["stagedUnlocks"] == true,
        "stage serialization");
    const auto path = docs.GetPath(); const auto id = docs.GetActiveDocumentId();
    Check(progression.SetMode(Mode::FreeFarming, economy.GetMoney(), FarmContestSeasonSystem::Evaluate(date.GetDay(), {})), "free switch");
    Check(economy.IsCropUnlocked(Crop::Pumpkin) && tools.MaximumAllowedHeight() == 2, "free access");
    Check(docs.Load(id, grid, economy, selection) && progression.StagePolicy().enabled && !economy.IsCropUnlocked(Crop::Tomato),
        "stage roundtrip binding");
    for (int kind = 0; kind < 3; ++kind) {
        auto bad = saved;
        if (kind == 0) bad.erase("stagedUnlocks");
        else if (kind == 1) bad["stagedUnlocks"] = 1;
        else bad["playMode"] = "FreeFarming";
        const auto before = grid.GetGeneration();
        Check(JsonFile::Save(path, bad) && !docs.Load(id, grid, economy, selection), "invalid policy accepted");
        Check(date.GetDay() == 10 && progression.StagePolicy().enabled && grid.GetGeneration() == before,
            "rejected document mutated state");
    }
    auto legacy = saved; legacy["schemaVersion"] = 17; legacy.erase("stagedUnlocks");
    Check(JsonFile::Save(path, legacy) && docs.Load(id, grid, economy, selection), "legacy load");
    Check(!progression.StagePolicy().enabled && economy.IsCropUnlocked(Crop::Pumpkin), "legacy access lost");
    Check(docs.Save(grid, economy, selection), "legacy migration save");
    nlohmann::json migrated; Check(JsonFile::Load(path, migrated) && migrated["stagedUnlocks"] == false, "migration policy");
    Check(docs.Reset(grid, economy, selection), "document reset");
    Check(progression.StagePolicy().enabled && date.GetDay() == 1 && !economy.IsCropUnlocked(Crop::Tomato), "restart stages");
    Check(progression.SetMode(Mode::FreeFarming, economy.GetMoney(), FarmContestSeasonSystem::Evaluate(1, {})) &&
        progression.SetMode(Mode::ContestSeason, economy.GetMoney(), FarmContestSeasonSystem::Evaluate(1, {})) && !progression.StagePolicy().enabled,
        "existing mode switch restricted");

    FarmLayoutSystem layouts; Check(layouts.Initialize((root / "layouts").string()), "layout init");
    Check(grid.SelectTile(0), "layout selection");
    while (grid.GetSelectedTile()->heightLevel < 2) Check(tools.RaiseSelectedTile(grid), "layout fixture");
    Check(layouts.SaveNew("H2", grid) && !layouts.Entries().empty(), "layout save");
    const auto layoutId = layouts.Entries().back().id;
    progression.Initialize({}, Mode::ContestSeason); setDay(1);
    const auto beforeLayout = grid.GetGeneration();
    Check(!layouts.Load(layoutId, grid, tools.MaximumAllowedHeight()) && grid.GetGeneration() == beforeLayout,
        "layout cap/atomicity");
    Check(!tools.RaiseSelectedTile(grid) && grid.GetSelectedTile()->heightLevel == 2, "locked raise lowered H2");
    setDay(11); Check(layouts.Load(layoutId, grid, tools.MaximumAllowedHeight()), "unlocked layout rejected");

    for (std::size_t i = 0; i < kFarmStageRulesV1.size(); ++i) {
        const auto& rule = kFarmStageRulesV1[i];
        Check(rule.contestDay == FarmContestEntrySystem::kContestDays[i], "contest schedule drift");
        for (int delta : {-1, 0, 1}) {
            FarmEconomySystem::ContestResults results{};
            results[i].contestDay = rule.contestDay;
            results[i].harvest.quality.crop = rule.unlockedCrop;
            results[i].qualityPoints = 50;
            results[i].sizePoints = rule.passingPoints + delta - 50;
            const auto goals = FarmContestGoalSystem::Evaluate(rule.contestDay, results, true);
            Check(goals[i].status == (delta < 0 ? FarmContestGoalStatus::Failed : FarmContestGoalStatus::Passed), "verdict boundary");
            Check(goals[i].shortfall == (delta < 0 ? 1 : 0), "shortfall boundary");
            farmui::View view{};
            farmui::BuildContestResultsView(view, results, FarmContestSeasonSystem::Evaluate(rule.contestDay, results), goals);
            CheckView(view);
        }
        const auto missing = FarmContestGoalSystem::Evaluate(rule.contestDay + 1, {}, true);
        Check(missing[i].status == FarmContestGoalStatus::Missed && missing[i].shortfall == 0, "missing is not scored");
        Check(FarmContestGoalSystem::Evaluate(rule.contestDay, {}, false)[i].status == FarmContestGoalStatus::Unknown,
            "legacy verdict applied");
        farmui::View guide{}; farmui::BuildStageGuideView(guide, FarmStageAccessSystem::Evaluate(rule.firstDay, true)); CheckView(guide);
    }
    Check(FarmContestGoalSystem::Evaluate(0, {}, true)[0].status == FarmContestGoalStatus::Unknown, "invalid day");
    for (const auto& r : farmui::kLabels) Check(r.x + r.width <= farmui::kAtlasWidth && r.y + r.height <= farmui::kAsciiY, "atlas bounds");
    std::cout << "PASS: stage boundaries, action authority, stale confirmations/history, schema22/legacy, layout, verdicts and view bounds\n";
}
}
int main() { try { Run(); } catch (const std::exception& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; } }
