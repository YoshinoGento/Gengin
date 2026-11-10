#include "farm/system/FarmDocumentSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmContestDaySystem.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
}

int main() {
    try {
        constexpr auto freeMode = FarmProgressionMode::FreeFarming;
        const auto rules = FarmProgressionSystem::InitialRules(freeMode);
        FarmEconomySystem economy; economy.Initialize(rules);
        Check(economy.GetMoney() == 300 && economy.GetTotalSeedCount() == 15, "Free restart rules");
        for (const auto crop : {farm::CropType::Carrot, farm::CropType::Tomato, farm::CropType::Pumpkin})
            Check(economy.GetSeedCount(crop) == 5, "Free initial crop stock");
        FarmEconomySystem ordinary; ordinary.Initialize(FarmProgressionSystem::InitialRules(FarmProgressionMode::Trial));
        Check(ordinary.GetTotalSeedCount() == 0, "Trial restart changed");
        FarmProgressionSystem progression; progression.Initialize(rules, freeMode);
        const auto ended = FarmContestSeasonSystem::Evaluate(31, {});
        for (int money : {0, 300, 540, 1000, (std::numeric_limits<int>::max)()}) {
            Check(!progression.EvaluateClear(money) && !progression.EvaluateSeason(ended) && !progression.IsCleared(), "Free clear");
            Check(progression.GetProgress(money, 31) == 0 && progression.GetRemainingMoney(money) == 0, "Free money target");
        }
        auto invalid = progression.CaptureSnapshot(); invalid.cleared = true;
        Check(!progression.RestoreSnapshot(invalid) && progression.IsFreeFarming(), "Invalid free snapshot accepted");
        Check(!progression.SetMode(static_cast<FarmProgressionMode>(99), 300, ended), "Invalid enum accepted");
        Check(progression.SetMode(FarmProgressionMode::ContestSeason, 300, ended) && progression.IsCleared(), "Season deadline lost");
        Check(progression.SetMode(freeMode, 300, ended) && !progression.IsCleared(), "Ended season cannot become free");
        Check(progression.SetMode(FarmProgressionMode::Trial, 540, ended) && progression.IsCleared(), "Trial goal lost");
        Check(progression.SetMode(freeMode, 540, ended) && !progression.IsCleared(), "Trial clear leaked to free");

        FarmDateSystem date; date.Initialize(); FarmContestDaySystem notices;
        for (int day : {9, 19, 29, 30, 31, 100}) {
            Check(date.RestoreSnapshot({day, 59.5f, 1.0f}), "Date fixture");
            notices.Advance(date, 1, economy.GetContestResults(), 31, false);
            Check(date.GetDay() == day + 1 && !notices.PendingDay(), "Free date stopped");
        }
        notices.Observe(10, economy.GetContestResults());
        Check(notices.PendingDay() == 10, "Existing notice changed");
        notices.Observe(10, economy.GetContestResults(), false);
        Check(!notices.PendingDay(), "Stale notice blocks free mode");
        Check(date.RestoreSnapshot({(std::numeric_limits<int>::max)(), 59, 4}), "Max date fixture");
        notices.Advance(date, 1000, {}, 0, false);
        Check(date.GetDay() == (std::numeric_limits<int>::max)() && !progression.IsCleared(), "Date overflow");

        farm::FarmGrid grid; Check(grid.Initialize(5, 4), "Grid init");
        FarmCropSelectionSystem selection; selection.Initialize();
        Check(date.RestoreSnapshot({32, 12, 2}), "Save date fixture");
        auto rich = economy.CaptureSnapshot(); rich.money = 1000;
        Check(economy.RestoreSnapshot(rich), "Money fixture");
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto root = std::filesystem::path("generated/codex_checks") / ("free_mode_" + std::to_string(stamp));
        FarmDocumentSystem docs;
        Check(docs.Initialize(root.string(), grid, economy, selection, &date, &progression), "Docs init");
        Check(docs.SaveAs("FreeTest", grid, economy, selection), "Save free");
        nlohmann::json saved; Check(JsonFile::Load(docs.GetPath(), saved), "Read saved JSON");
        Check(saved["schemaVersion"] == 22 && saved["playMode"] == "FreeFarming", "Wrong serialization");
        FarmDateSystem reloadedDate; reloadedDate.Initialize();
        FarmProgressionSystem reloadedMode; reloadedMode.Initialize();
        FarmDocumentSystem reload;
        Check(reload.Initialize(root.string(), grid, economy, selection, &reloadedDate, &reloadedMode), "Reload failed");
        Check(reloadedDate.GetDay() == 32 && reloadedMode.IsFreeFarming() && !reloadedMode.IsCleared(), "Mode/date lost");
        const auto path = docs.GetPath();
        const auto id = docs.GetActiveDocumentId();
        for (const int version : {14, 15, 16, 23}) {
            auto bad = saved; bad["schemaVersion"] = version;
            Check(JsonFile::Save(path, bad), "Write invalid fixture");
            Check(!reload.Load(id, grid, economy, selection), "Unsupported free schema accepted");
            Check(reloadedMode.IsFreeFarming() && reloadedDate.GetDay() == 32 && economy.GetTotalSeedCount() == 15,
                "Rejected save mutated state");
        }
        for (const char* name : {"Trial", "ContestSeason"}) {
            auto legacy = saved; legacy["schemaVersion"] = 16; legacy["playMode"] = name;
            Check(JsonFile::Save(path, legacy) && reload.Load(id, grid, economy, selection), "Legacy16 mode rejected");
            Check(!reloadedMode.IsFreeFarming(), "Legacy16 mode changed");
        }
        Check(JsonFile::Save(path, saved) && reload.Load(id, grid, economy, selection) && reloadedMode.IsFreeFarming(), "Final free roundtrip");
        std::cout << "PASS: FreeFarming rules, money/day limits, notices, overflow, mode transitions, schema22 roundtrip and legacy rejection\n";
        std::cout << "Boundary fixture: " << root.string() << '\n';
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
