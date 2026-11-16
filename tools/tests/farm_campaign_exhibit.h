#pragma once

#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmDevelopmentSystem.h"
#include "farm/system/FarmRecoverySystem.h"
#include "farm/system/FarmSimulationStepSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "io/JsonFile.h"
#include <filesystem>
#include <stdexcept>
#include <utility>

// Offline export only. Copies own their Systems; the measured campaign is never mutated.
class FarmCampaignExhibit final {
public:
    explicit FarmCampaignExhibit(std::filesystem::path root) : root_(std::move(root)) {
        Require(!std::filesystem::exists(root_) && std::filesystem::create_directories(root_), "export collision");
    }
    bool Has(std::size_t index) const { return written_.at(index); }
    void Write(std::size_t index, const farm::FarmGrid& sourceGrid, const FarmEconomySystem& sourceEconomy,
        const FarmDateSystem& sourceDate, const FarmProgressionSystem& sourceProgression, farm::CropType crop) {
        Require(index < written_.size() && !written_[index], "duplicate scenario");
        farm::FarmGrid grid;
        farm::FarmGrid::Snapshot snapshot;
        sourceGrid.CaptureSnapshot(snapshot);
        Require(grid.ReplaceSnapshot(snapshot), "copy grid");
        FarmEconomySystem economy; economy.Initialize();
        FarmDateSystem date; date.Initialize();
        FarmProgressionSystem progression; progression.Initialize();
        Require(economy.RestoreSnapshot(sourceEconomy.CaptureSnapshot()) &&
            date.RestoreSnapshot(sourceDate.CaptureSnapshot()) &&
            progression.RestoreSnapshot(sourceProgression.CaptureSnapshot()), "copy Systems");
        date.SetTimeScale(1);
        FarmStageAccessSystem access; access.Bind(date, progression.StagePolicy());
        economy.BindStageAccess(access);
        FarmToolActionSystem tools; tools.Initialize(); tools.BindStageAccess(access);
        if (index == 3) {
            // A deliberately prepared variant: plant two crops before the second purchase.
            crop = farm::CropType::Pumpkin;
            for (int tile : {6, 8}) {
                Require(grid.SelectTile(tile) && grid.GetSelectedTile(), "expansion field");
                while (grid.GetSelectedTile()->heightLevel > 0) Require(tools.LowerSelectedTile(grid), "lower field");
                Require(tools.CompostSelectedTile(grid) && economy.BuySeed(crop).Succeeded(), "prepare planted expansion");
                Require(tools.ApplyToolDetailed(grid, FarmTool::Seed, crop, economy, date.GetDay()).Succeeded(), "plant expansion crop");
            }
            farm::FarmIrrigationSystem water; water.Initialize(); water.Rebuild(grid);
            FarmGrowthSystem growth; growth.Initialize();
            FarmContestDaySystem contest;
            for (int tick = 0; tick < 480; ++tick)
                Require(FarmSimulationStepSystem::Step({grid, water, growth, date, contest, economy, progression},
                    1.0f / 60.0f).advanced, "prepare growing expansion");
            Require(FarmDevelopmentSystem::Inspect(grid, economy).canExpand && grid.GetHeight() == 5 &&
                grid.GetTile(6)->growth > 0 && grid.GetTile(8)->growth > 0, "expansion scenario eligibility");
        }
        const int selected = index == 1 ? FarmPestSystem::FindActive(grid) : 6;
        Require(grid.SelectTile(selected), "exhibit selection");
        Save(index, grid, economy, date, progression, crop);
    }
    void WriteRecovery() {
        farm::FarmGrid grid; Require(grid.Initialize(5, 4), "recovery grid");
        FarmEconomySystem economy; economy.Initialize();
        auto empty = economy.CaptureSnapshot(); empty.money = 0; empty.seedCounts.fill(0);
        Require(economy.RestoreSnapshot(empty), "prepared exhausted economy");
        FarmDateSystem date; date.Initialize();
        FarmProgressionSystem progression;
        progression.Initialize({}, FarmProgressionMode::ContestSeason, true, true, true);
        Require(FarmRecoverySystem::Inspect(grid, economy, 1).CanClaim(), "recovery availability");
        Save(5, grid, economy, date, progression, farm::CropType::Carrot);
    }
    void Finish() const {
        for (bool written : written_) Require(written, "missing exhibition scenario");
    }
private:
    static void Require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
    void Save(std::size_t index, farm::FarmGrid& grid, FarmEconomySystem& economy, FarmDateSystem& date,
        FarmProgressionSystem& progression, farm::CropType crop) {
        constexpr std::array names{"01_water", "02_pests", "03_rain", "04_expansion", "05_judging", "06_recovery", "07_results"};
        constexpr std::array labels{"展示01 水路と栽培", "展示02 害虫への対処", "展示03 雨と取水", "展示04 土地拡張",
            "展示05 品評会への出品", "展示06 栽培の再開支援", "展示07 審査済みの結果"};
        const auto directory = root_ / names.at(index) / "farm";
        FarmCropSelectionSystem selection; selection.Initialize();
        Require(selection.SetSelectedCrop(crop), "selection crop");
        FarmDocumentSystem documents;
        Require(documents.Initialize(directory.string(), grid, economy, selection, &date, &progression) &&
            documents.SaveAs(labels.at(index), grid, economy, selection), "export save");
        nlohmann::json before, after;
        Require(JsonFile::Load(documents.GetPath(), before), "read exported save");
        // A fresh loader must accept the complete persistent state, not just the metadata.
        farm::FarmGrid loaded; Require(loaded.Initialize(5, 4), "roundtrip grid");
        FarmEconomySystem loadedEconomy; loadedEconomy.Initialize();
        FarmDateSystem loadedDate; loadedDate.Initialize();
        FarmProgressionSystem loadedProgression; loadedProgression.Initialize();
        FarmCropSelectionSystem loadedSelection; loadedSelection.Initialize();
        FarmDocumentSystem reopened;
        Require(reopened.Initialize(directory.string(), loaded, loadedEconomy, loadedSelection, &loadedDate, &loadedProgression) &&
            reopened.GetActiveDocumentId() == documents.GetActiveDocumentId() &&
            reopened.Save(loaded, loadedEconomy, loadedSelection) && JsonFile::Load(reopened.GetPath(), after), "loader roundtrip");
        before.erase("document"); after.erase("document");
        Require(before == after, "roundtrip changed persistent state");
        if (index == 3) {
            FarmStageAccessSystem loadedAccess; loadedAccess.Bind(loadedDate, loadedProgression.StagePolicy());
            loadedEconomy.BindStageAccess(loadedAccess);
            const auto offer = FarmDevelopmentSystem::Inspect(loaded, loadedEconomy);
            const int money = loadedEconomy.GetMoney();
            Require(offer.canExpand && FarmDevelopmentSystem::Expand(loaded, loadedEconomy, offer.quote) &&
                loaded.GetTileCount() == 30 && loadedEconomy.GetMoney() == money - offer.cost &&
                loaded.GetTile(6)->crop == farm::CropType::Pumpkin, "loaded planted expansion");
        }
        written_.at(index) = true;
    }
    std::filesystem::path root_;
    std::array<bool, 7> written_{};
};
