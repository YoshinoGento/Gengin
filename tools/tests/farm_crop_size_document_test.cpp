#include "farm_test_edit.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/system/FarmContestDaySystem.h"
#include "io/JsonFile.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <limits>

static nlohmann::json Legacy(const nlohmann::json& original) {
    auto result=original;
    for(const char* field : {"cropCounts","cropValues","seedCounts"}) {
        result["economy"][field]=nlohmann::json::array({original["economy"][field][0],original["economy"][field][1]});
    }
    return result;
}

int main() {
    {
        FarmContestDaySystem notice;
        FarmDateSystem clock; clock.Initialize();
        FarmEconomySystem::ContestResults results{};
        for (float speed : {1.f,2.f,4.f}) {
            clock.Initialize(); clock.SetTimeScale(speed);
            clock.Update(1.0f);
            assert(clock.GetDay()==1 && clock.GetElapsedSecondsInDay()==speed);
            FarmDateSystem restored; restored.Initialize();
            assert(restored.RestoreSnapshot(clock.CaptureSnapshot()));
            assert(restored.GetTimeScale()==speed && restored.GetElapsedSecondsInDay()==speed);
            for (float invalid : {-1.f,0.f,3.f,8.f,std::numeric_limits<float>::quiet_NaN(),
                std::numeric_limits<float>::infinity()}) {
                restored.SetTimeScale(invalid);
                assert(restored.GetTimeScale()==speed && restored.GetElapsedSecondsInDay()==speed);
            }
        }
        clock.Initialize(); notice.Reset();
        assert(clock.RestoreSnapshot({9,59.f,1.f}));
        clock.SetTimeScale(4.f); notice.Advance(clock,0.5f,results);
        assert(clock.GetDay()==10 && clock.GetElapsedSecondsInDay()==0 && notice.PendingDay()==10);
        for (float speed : {1.f,2.f,4.f}) {
            clock.SetTimeScale(speed); notice.Advance(clock,30.f,results);
            assert(clock.GetDay()==10 && clock.GetElapsedSecondsInDay()==0 && notice.PendingDay()==10);
        }
        for (float speed : {1.f,2.f,4.f}) for(int day : {10,20,30}) {
            notice.Reset();
            assert(clock.RestoreSnapshot({day-1,59.99f,speed}));
            notice.Advance(clock,0.02f,results);
            assert(clock.GetDay()==day && clock.GetElapsedSecondsInDay()==0);
            assert(notice.PendingDay()==day);
            for(int frame=0;frame<100;++frame) notice.Advance(clock,1e30f,results);
            assert(clock.GetDay()==day && clock.GetElapsedSecondsInDay()==0);
            assert(!notice.Acknowledge(day-1) && !notice.Acknowledge(0));
            assert(notice.Acknowledge(day));
            notice.Observe(day,results); assert(notice.PendingDay()==0);
            notice.Advance(clock,1.f,results); assert(clock.GetElapsedSecondsInDay()==speed);
            notice.Reset(); notice.Observe(day,results); assert(notice.PendingDay()==day);
            results[day/10-1].contestDay=day;
            notice.Observe(day,results); assert(notice.PendingDay()==0);
            results={};
        }
        clock.Initialize(); notice.Reset();
        notice.Advance(clock,1e30f,results); assert(clock.GetDay()==10 && notice.PendingDay()==10);
        assert(notice.Acknowledge(10));
        notice.Advance(clock,1e30f,results); assert(clock.GetDay()==20 && notice.PendingDay()==20);
        assert(notice.Acknowledge(20));
        notice.Advance(clock,1e30f,results); assert(clock.GetDay()==30 && notice.PendingDay()==30);
        assert(notice.Acknowledge(30));
        notice.Advance(clock,1e30f,results); assert(clock.GetDay()==2147483647 && !notice.PendingDay());
        notice.Observe(20,results); assert(notice.PendingDay()==20); // Rewind starts a new notice pass.
        notice.Reset(); notice.Observe(21,results); assert(!notice.PendingDay()); // No retroactive day20 submission.
        notice.Observe(0,results); assert(!notice.PendingDay());
        std::cout << "PASS: contest-day pause, speed boundaries, large-step cap, acknowledgement, reload and rewind\n";
    }
    farm::FarmGrid grid; assert(grid.Initialize(2, 1));
    assert(EditFarmTile(grid, 0, [&](auto& candidate) { candidate.irrigationEnabled = false; }));
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDocumentSystem documents;
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason);
    const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string directory = "generated/codex_checks/size_documents_" + std::to_string(tick);
    assert(documents.Initialize(directory, grid, economy, selection, &date, &progression));
    assert(date.RestoreSnapshot({7,12.5f,4.f}));
    farm::FarmTile tile;
    tile.state = farm::FarmTileState::Planted; tile.crop = farm::CropType::Carrot;
    tile.growth = 1; tile.careHistory.goodSeconds = 10; tile.careHistory.efficiencySeconds = 10;
    tile.careHistory.nutrientGrowth = 0.95f; tile.careHistory.nutrientSupply = 0.475f;
    FarmCropQualitySystem quality; quality.Initialize();
    auto result = quality.Evaluate(tile);
    assert(result.harvestSize.known && result.harvestSize.multiplier == 1.25f);
    assert(economy.AddHarvest(result,1,7));
    const int harvestId = economy.GetHarvestRecord(0)->id;
    assert(economy.SetHarvestProtection(harvestId, true, economy.GetInventoryGeneration()));
    assert(economy.SetContestReservation(harvestId, true, economy.GetInventoryGeneration()));
    assert(documents.SaveAs("size roundtrip", grid, economy, selection));
    const std::string path = documents.GetPath(), id = documents.GetActiveDocumentId();
    nlohmann::json original;
    assert(JsonFile::Load(path, original) && original["schemaVersion"] == 19);
    assert(original["playMode"]=="ContestSeason");
    {
        for(int version=1;version<=13;++version) {
            auto legacy=Legacy(original); legacy["schemaVersion"]=version; legacy.erase("playMode");
            assert(JsonFile::Save(path,legacy) && documents.Load(id,grid,economy,selection));
            assert(progression.GetMode()==FarmProgressionMode::Trial);
            assert(grid.GetTile(0)->irrigationEnabled);
        }
        auto legacy14 = Legacy(original); legacy14["schemaVersion"] = 14;
        for (auto& savedTile : legacy14["tiles"]) savedTile.erase("irrigationEnabled");
        assert(JsonFile::Save(path,legacy14) && documents.Load(id,grid,economy,selection));
        assert(grid.GetTile(0)->irrigationEnabled && progression.IsContestSeason());
        assert(JsonFile::Save(path,original) && documents.Load(id,grid,economy,selection));
        assert(!grid.GetTile(0)->irrigationEnabled);
        for (int kind = 0; kind < 5; ++kind) {
            auto invalid = original;
            if (kind == 0) invalid["tiles"][0].erase("irrigationEnabled");
            if (kind == 1) invalid["tiles"][0]["irrigationEnabled"] = nullptr;
            if (kind == 2) invalid["tiles"][0]["irrigationEnabled"] = 0;
            if (kind == 3) invalid["tiles"][0]["irrigationEnabled"] = "false";
            if (kind == 4) invalid["tiles"][0]["irrigationEnabled"] = nlohmann::json::array();
            const auto generation = grid.GetGeneration();
            const auto inventory = economy.GetInventoryGeneration();
            assert(JsonFile::Save(path,invalid) && !documents.Load(id,grid,economy,selection));
            assert(grid.GetGeneration() == generation && !grid.GetTile(0)->irrigationEnabled);
            assert(economy.GetInventoryGeneration() == inventory && date.GetDay() == 7);
        }
        for(int mode=0;mode<5;++mode) {
            auto invalid=original;
            if(mode==0) invalid.erase("playMode");
            if(mode==1) invalid["playMode"]=nullptr;
            if(mode==2) invalid["playMode"]=1;
            if(mode==3) invalid["playMode"]="unknown";
            if(mode==4) invalid["playMode"]=nlohmann::json::array();
            const auto generation=economy.GetInventoryGeneration();
            assert(JsonFile::Save(path,invalid) && !documents.Load(id,grid,economy,selection));
            assert(progression.IsContestSeason() && !progression.IsCleared());
            assert(date.GetDay()==7 && economy.GetInventoryGeneration()==generation);
        }
        for(const char* mode : {"Trial","ContestSeason"}) {
            auto wealthy=original; wealthy["economy"]["money"]=999999; wealthy["playMode"]=mode;
            wealthy["stagedUnlocks"] = std::string(mode) == "ContestSeason";
            assert(JsonFile::Save(path,wealthy) && documents.Load(id,grid,economy,selection));
            assert(progression.IsCleared()==(std::string(mode)=="Trial"));
        }
        auto finished=original; finished["date"]["day"]=31;
        assert(JsonFile::Save(path,finished) && documents.Load(id,grid,economy,selection));
        assert(progression.IsContestSeason() && progression.IsCleared());
        assert(JsonFile::Save(path,original) && documents.Load(id,grid,economy,selection));
        assert(!progression.IsCleared());
        for(float speed : {1.f,2.f,4.f}) {
            FarmContestDaySystem notice;
            FarmDateSystem clock; assert(clock.RestoreSnapshot({30,59.99f,speed}));
            notice.Observe(30,{}); assert(notice.Acknowledge(30));
            notice.Advance(clock,1e30f,{},31);
            assert(clock.GetDay()==31 && clock.GetElapsedSecondsInDay()==0);
        }
        std::cout << "PASS: persisted mode, legacy migration, atomic invalid mode, restored end-state, season deadline cap\n";
    }
    date.AdvanceOneDay();
    economy.Initialize();
    assert(documents.Load(id, grid, economy, selection));
    assert(date.GetDay()==7 && date.GetElapsedSecondsInDay()==12.5f && date.GetTimeScale()==4.f);
    assert(economy.GetHarvestRecord(0)->harvestedDay==7);
    assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 1.25f);
    assert(economy.GetHarvestRecordCount() == 1 && economy.GetUnrecordedCropCount() == 0);
    assert(economy.GetHarvestRecord(0)->quality.harvestSize.multiplier == 1.25f);
    FarmDocumentSystem reopened;
    economy.Initialize();
    date.Initialize();
    assert(reopened.Initialize(directory, grid, economy, selection, &date, &progression));
    assert(progression.IsContestSeason() && !progression.IsCleared());
    assert(date.GetDay()==7 && economy.GetHarvestRecord(0)->harvestedDay==7);
    assert(!grid.GetTile(0)->irrigationEnabled);
    {
        auto legacy=Legacy(original); legacy["schemaVersion"]=11; legacy.erase("date");
        legacy["economy"]["harvestRecords"][0].erase("harvestedDay");
        assert(JsonFile::Save(path,legacy) && documents.Load(id,grid,economy,selection));
        assert(date.GetDay()==1 && economy.GetHarvestRecord(0)->harvestedDay==0);
        assert(documents.Save(grid,economy,selection));
        nlohmann::json migrated; assert(JsonFile::Load(path,migrated));
        assert(migrated["schemaVersion"]==19 && migrated["economy"]["harvestRecords"][0]["harvestedDay"]==0);
        assert(JsonFile::Save(path,original) && documents.Load(id,grid,economy,selection));
    }
    assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 1.25f);
    assert(economy.GetHarvestRecord(0)->id == harvestId && economy.GetHarvestRecord(0)->saleProtected);
    assert(economy.GetContestReservationId()==harvestId && economy.GetContestReservation()->quality.harvestSize.multiplier==1.25f);
    assert(economy.GetSalePreviewValue() == 0 && !economy.SellAll().Succeeded());
    {
        auto legacy=Legacy(original); legacy["schemaVersion"]=10; legacy["economy"].erase("contestReservationId");
        assert(JsonFile::Save(path,legacy) && documents.Load(id,grid,economy,selection));
        assert(economy.GetContestReservationId()==0 && economy.GetHarvestRecord(0)->saleProtected);
    }
    {
        auto legacy = Legacy(original); legacy["schemaVersion"] = 9;
        legacy["economy"].erase("nextHarvestRecordId");
        legacy["economy"]["harvestRecords"][0].erase("id");
        legacy["economy"]["harvestRecords"][0].erase("saleProtected");
        assert(JsonFile::Save(path, legacy) && documents.Load(id, grid, economy, selection));
        assert(economy.GetHarvestRecord(0)->id == 1 && !economy.GetHarvestRecord(0)->saleProtected);
        assert(economy.CaptureSnapshot().nextHarvestRecordId == 2);
        assert(economy.GetSalePreviewValue() == result.salePrice);
        assert(economy.GetContestReservationId()==0);
    }
    for (int version = 1; version <= 7; ++version) {
        auto legacy = Legacy(original); legacy["schemaVersion"] = version;
        auto& record = legacy["economy"]["lastHarvestQuality"];
        record.erase("sizeKnown"); record.erase("sizeMultiplier");
        assert(JsonFile::Save(path, legacy));
        assert(documents.Load(id, grid, economy, selection));
        assert(!economy.GetLastHarvestQuality().harvestSize.known);
        assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 0);
    }
    assert(JsonFile::Save(path, original) && documents.Load(id, grid, economy, selection));
    {
        auto legacy = Legacy(original); legacy["schemaVersion"] = 8; legacy["economy"].erase("harvestRecords");
        assert(JsonFile::Save(path, legacy) && documents.Load(id, grid, economy, selection));
        assert(economy.GetHarvestRecordCount() == 0 && economy.GetUnrecordedCropCount() == 1);
        assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 1.25f);
        assert(JsonFile::Save(path, original) && documents.Load(id, grid, economy, selection));
    }
    for (int mode = 0; mode < 37; ++mode) {
        auto bad = original;
        if (mode == 0) bad["economy"].erase("harvestRecords");
        if (mode == 1) bad["economy"]["harvestRecords"][0]["quantity"] = 0;
        if (mode == 2) bad["economy"]["harvestRecords"][0]["quantity"] = 2;
        if (mode == 3) bad["economy"]["harvestRecords"][0]["quantity"] = 4294967297ULL;
        if (mode == 4) bad["economy"]["harvestRecords"][0]["quality"]["salePrice"] = 4294967537ULL;
        if (mode == 5) bad["economy"]["harvestRecords"][0]["quality"]["sizeKnown"] = "yes";
        if (mode == 6) bad["economy"]["harvestRecords"] = nullptr;
        if (mode == 7) bad["economy"]["harvestRecords"][0]["quality"]["crop"] = "Unknown";
        if (mode == 8) for (int i=0; i<128; ++i)
            bad["economy"]["harvestRecords"].push_back(original["economy"]["harvestRecords"][0]);
        if (mode == 9) bad["economy"]["harvestRecords"][0].erase("id");
        if (mode == 10) bad["economy"]["harvestRecords"][0]["id"] = 0;
        if (mode == 11) bad["economy"]["harvestRecords"][0]["id"] = 4294967297ULL;
        if (mode == 12) bad["economy"]["harvestRecords"][0].erase("saleProtected");
        if (mode == 13) bad["economy"]["harvestRecords"][0]["saleProtected"] = 1;
        if (mode == 14) bad["economy"].erase("nextHarvestRecordId");
        if (mode == 15) bad["economy"]["nextHarvestRecordId"] = harvestId;
        if (mode == 16) bad["economy"]["nextHarvestRecordId"] = 4294967297ULL;
        if (mode == 17) bad["economy"].erase("contestReservationId");
        if (mode == 18) bad["economy"]["contestReservationId"] = -1;
        if (mode == 19) bad["economy"]["contestReservationId"] = 4294967297ULL;
        if (mode == 20) bad["economy"]["contestReservationId"] = harvestId+1;
        if (mode == 21) bad["economy"]["harvestRecords"][0]["saleProtected"] = false;
        if (mode == 22) {
            bad["economy"]["harvestRecords"][0]["quality"]["sizeKnown"] = false;
            bad["economy"]["harvestRecords"][0]["quality"]["sizeMultiplier"] = 0;
        }
        bad["economy"]["money"] = 999;
        if(mode==23) bad["economy"]["harvestRecords"][0].erase("harvestedDay");
        if(mode==24) bad["economy"]["harvestRecords"][0]["harvestedDay"]=-1;
        if(mode==25) bad["economy"]["harvestRecords"][0]["harvestedDay"]=4294967297ULL;
        if(mode==26) bad["economy"]["harvestRecords"][0]["harvestedDay"]=7.5;
        if(mode==27) bad["economy"]["harvestRecords"][0]["harvestedDay"]=true;
        if(mode==28) bad["economy"]["harvestRecords"][0]["harvestedDay"]=8;
        if(mode==29) bad.erase("date");
        if(mode==30) bad["date"]["day"]=0;
        if(mode==31) bad["date"]["day"]=4294967297ULL;
        if(mode==32) bad["date"]["elapsedSecondsInDay"]=60;
        if(mode==33) bad["date"]["elapsedSecondsInDay"]=-1;
        if(mode==34) bad["date"]["elapsedSecondsInDay"]=1e100;
        if(mode==35) bad["date"]["timeScale"]=3;
        if(mode==36) bad["date"]["timeScale"]="4";
        const int money = economy.GetMoney();
        const auto generation = economy.GetInventoryGeneration();
        assert(JsonFile::Save(path, bad) && !documents.Load(id, grid, economy, selection));
        assert(economy.GetMoney() == money && economy.GetHarvestRecordCount() == 1);
        assert(economy.GetInventoryGeneration() == generation && economy.GetHarvestRecord(0)->saleProtected);
        assert(economy.GetContestReservationId()==harvestId);
        assert(date.GetDay()==7 && date.GetElapsedSecondsInDay()==12.5f && economy.GetHarvestRecord(0)->harvestedDay==7);
    }
    for (int mode = 0; mode < 8; ++mode) {
        auto bad = original;
        auto& record = bad["economy"]["lastHarvestQuality"];
        if (mode == 0) record.erase("sizeKnown");
        if (mode == 1) record.erase("sizeMultiplier");
        if (mode == 2) record["sizeKnown"] = "yes";
        if (mode == 3) record["sizeMultiplier"] = -1;
        if (mode == 4) record["sizeMultiplier"] = 9;
        if (mode == 5) record["sizeMultiplier"] = nullptr;
        if (mode == 6) record["sizeKnown"] = false;
        if (mode == 7) record["sizeMultiplier"] = 1e100;
        bad["economy"]["money"] = 999;
        const auto before = economy.CaptureSnapshot();
        assert(JsonFile::Save(path, bad));
        assert(!documents.Load(id, grid, economy, selection));
        assert(economy.GetMoney() == before.money);
        assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 1.25f);
    }
    for (float invalid : {-1.f, 9.f, std::numeric_limits<float>::quiet_NaN()}) {
        result.harvestSize = {invalid, true};
        const auto before = economy.CaptureSnapshot();
        assert(!economy.AddHarvest(result));
        auto bad = before; bad.lastHarvestQuality = result;
        assert(!economy.RestoreSnapshot(bad));
        assert(economy.GetLastHarvestQuality().harvestSize.multiplier == 1.25f);
    }
    assert(JsonFile::Save(path, original) && documents.Load(id,grid,economy,selection));
    assert(date.RestoreSnapshot({10,0,1}));
    assert(FarmContestSubmissionSystem::Submit(economy,10,harvestId,economy.GetInventoryGeneration()));
    const auto ratingBeforeSave=FarmContestSeasonSystem::Evaluate(31,economy.GetContestResults());
    assert(documents.Save(grid,economy,selection));
    nlohmann::json submitted; assert(JsonFile::Load(path,submitted));
    assert(submitted["economy"]["contestResults"].size()==1);
    economy.Initialize();
    assert(documents.Load(id,grid,economy,selection));
    assert(economy.GetContestResults()[0].contestDay==10 && economy.GetHarvestRecordCount()==0);
    const auto ratingAfterLoad=FarmContestSeasonSystem::Evaluate(31,economy.GetContestResults());
    assert(ratingAfterLoad.rating==ratingBeforeSave.rating &&
        ratingAfterLoad.nextRating==ratingBeforeSave.nextRating &&
        ratingAfterLoad.pointsToNextRating==ratingBeforeSave.pointsToNextRating);
    assert(!FarmContestSubmissionSystem::Submit(economy,10,harvestId,economy.GetInventoryGeneration()));
    for (int mode=0; mode<11; ++mode) {
        auto bad=submitted;
        auto& entries=bad["economy"]["contestResults"];
        if(mode==0) entries=nullptr;
        if(mode==1) entries[0]["contestDay"]=11;
        if(mode==2) entries[0]["rulesVersion"]=2;
        if(mode==3) entries[0]["recordId"]=0;
        if(mode==4) entries[0]["harvestedDay"]=11;
        if(mode==5) entries[0]["qualityPoints"]=99;
        if(mode==6) entries[0]["sizePoints"]=-1;
        if(mode==7) entries.push_back(entries[0]);
        if(mode==8) bad["date"]["day"]=9;
        if(mode==9) entries[0]["recordId"]=4294967297ULL;
        if(mode==10) bad["economy"].erase("contestResults");
        const auto generation=economy.GetInventoryGeneration();
        assert(JsonFile::Save(path,bad) && !documents.Load(id,grid,economy,selection));
        assert(economy.GetInventoryGeneration()==generation && economy.GetContestResults()[0].contestDay==10);
        assert(economy.GetHarvestRecordCount()==0 && date.GetDay()==10);
    }
    auto legacy12=Legacy(original); legacy12["schemaVersion"]=12; legacy12["economy"].erase("contestResults");
    assert(JsonFile::Save(path,legacy12) && documents.Load(id,grid,economy,selection));
    assert(economy.GetContestResults()[0].contestDay==0 && economy.GetHarvestRecordCount()==1);
    assert(JsonFile::Save(path,submitted) && documents.Load(id,grid,economy,selection));
    assert(documents.Reset(grid,economy,selection) && date.GetDay()==1);
    assert(grid.GetTile(0)->irrigationEnabled);
    for(const auto& contest : economy.GetContestResults()) assert(contest.contestDay==0);
    assert(date.RestoreSnapshot({2147483647,0,4})); date.AdvanceOneDay(); date.Update(1e30f);
    assert(date.GetDay()==2147483647 && date.GetElapsedSecondsInDay()==0);
    date.Initialize(); date.Update(121.25f); assert(date.GetDay()==3 && date.GetElapsedSecondsInDay()==1.25f);
    std::cout << "PASS: schema19 intake/mode/result/clock roundtrip, legacy1-14, atomic invalid load and reset\n";
}
