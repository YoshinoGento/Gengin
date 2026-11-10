#include "farm_test_edit.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmSoilSystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/ui/FarmQualityView.h"
#include "io/JsonFile.h"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>

int main() {
    using farm::CropType;
    constexpr std::array crops{CropType::Carrot,CropType::Tomato,CropType::Pumpkin};
    FarmCropSelectionSystem selection; selection.Initialize();
    assert(selection.GetSelectedCrop()==CropType::Carrot);
    constexpr std::array<Vector2,3> pointers{{{100,200},{300,200},{200,300}}};
    for(int i=0;i<3;++i) {
        assert(farm::PlayableCrop(i)==crops[i]);
        assert(farm::CropTypeFromSlot(farm::ToCropSlot(crops[i]))==crops[i]);
        assert(selection.Open({200,200})); selection.UpdatePointer(pointers[i]);
        assert(selection.GetHoveredCrop()==crops[i]); selection.Confirm();
        assert(!selection.IsOpen());
        assert(selection.GetSelectedCrop()==crops[i]);
    }
    assert(selection.Open({200,200})); selection.UpdatePointer({200,100});
    assert(!selection.Confirm() && selection.GetSelectedCrop()==CropType::Pumpkin);
    assert(!selection.SetSelectedCrop(static_cast<CropType>(999)));
    assert(farm::PlayableCrop(-1)==CropType::None && farm::PlayableCrop(3)==CropType::None);
    FarmGrowthSystem growth; growth.Initialize();
    FarmCropQualitySystem quality; quality.Initialize();
    std::array<float,3> durations{};
    for(int i=0;i<3;++i) {
        const auto crop=crops[i];
        farm::FarmGrid grid; assert(grid.Initialize(1,1));
        farm::FarmRules rules; rules.initialMoney=10000;
        FarmEconomySystem economy; economy.Initialize(rules);
        FarmToolActionSystem actions; actions.Initialize(rules);
        assert(economy.BuySeed(crop).Succeeded());
        assert(actions.ApplyTool(grid,FarmTool::Hoe,crop,economy));
        assert(actions.CompostSelectedTile(grid));
        assert(actions.ApplyTool(grid,FarmTool::Seed,crop,economy));
        assert(economy.GetSeedCount(crop)==0 && grid.GetTile(0)->crop==crop);
        auto candidate=*grid.GetTile(0);
        auto* tile=&candidate;
        tile->heightLevel=i==2 ? 0 : 1;
        tile->moisture=.55f;
        const auto good=growth.Evaluate(*tile);
        assert(good.profileCrop==crop && good.moistureStatus==FarmMoistureStatus::Good);
        tile->moisture=0; const auto dry=growth.Evaluate(*tile);
        tile->moisture=1; const auto excess=growth.Evaluate(*tile);
        assert(dry.growthPerSecond<good.growthPerSecond && excess.growthPerSecond<good.growthPerSecond);
        assert(FarmGrowthSystem::AnalyzeWater(tile,excess,farm::FarmWaterStatus::Available).advice==FarmWaterAdvice::CloseIntake);
        for(int step=0;step<1000 && !farm::IsHarvestReady(*tile);++step) {
            tile->moisture=.55f;
            assert(grid.SetTile(0,*tile));
            assert(growth.Update(grid,.1f,1.f)); durations[i]+=.1f;
            candidate=*grid.GetTile(0);
        }
        assert(farm::IsHarvestReady(*tile) && tile->careHistory.IsValid());
        assert(tile->soilNutrients<1 && tile->careHistory.GetAverageEfficiency()>.99f);
        const auto well=quality.Evaluate(*tile);
        assert(well.IsValid() && well.harvestSize.known && well.crop==crop);
        auto poor=*tile; poor.careHistory.goodSeconds=0;
        poor.careHistory.drySeconds=10; poor.careHistory.efficiencySeconds=1;
        const auto stressed=quality.Evaluate(poor);
        assert(stressed.score<well.score && stressed.salePrice<well.salePrice);
        assert(stressed.harvestSize.multiplier<well.harvestSize.multiplier);
        farm::FarmTileVisualData visual{}; visual.valid=true; visual.crop=crop;
        visual.cropStage=farm::FarmCropGrowthStage::Ready; visual.cropScale=1; visual.center={0,2,0};
        const auto mesh=farm::BuildFarmCropMeshParts(visual,1);
        assert(mesh.count==2);
        assert(mesh.parts[0].shape==(i==0 ? farm::FarmMeshShape::Carrot : i==1 ? farm::FarmMeshShape::Tomato : farm::FarmMeshShape::Pumpkin));
        assert((mesh.parts[0].position.y<2)==(crop==CropType::Carrot));
        const auto harvest=actions.ApplyToolDetailed(grid,FarmTool::Harvest,crop,economy,1);
        assert(harvest.Succeeded() && economy.GetCropCount(crop)==1);
        assert(!actions.ApplyTool(grid,FarmTool::Harvest,crop,economy));
        assert(economy.GetHarvestRecord(0)->quality.crop==crop);
        const int money=economy.GetMoney();
        assert(economy.SellAll().Succeeded() && economy.GetMoney()==money+well.salePrice);
        assert(economy.GetCropCount(crop)==0);
        assert(economy.AddHarvest(well,1,1));
        const int record=economy.GetHarvestRecord(0)->id;
        assert(economy.SetHarvestProtection(record,true,economy.GetInventoryGeneration()));
        assert(economy.SetContestReservation(record,true,economy.GetInventoryGeneration()));
        assert(FarmContestSubmissionSystem::Submit(economy,10,record,economy.GetInventoryGeneration()));
        assert(economy.GetContestResults()[0].harvest.quality.crop==crop);
        assert(!FarmContestSubmissionSystem::Submit(economy,10,record,economy.GetInventoryGeneration()));
    }
    assert(durations[0]<durations[1] && durations[1]<durations[2]);
    assert(FarmSoilSystem::kCarrot.consumptionPerGrowth<FarmSoilSystem::kTomato.consumptionPerGrowth);
    assert(FarmSoilSystem::kTomato.consumptionPerGrowth<FarmSoilSystem::kPumpkin.consumptionPerGrowth);
    farm::FarmTile ideal; ideal.crop=CropType::Pumpkin; ideal.growth=1;
    ideal.careHistory.goodSeconds=10; ideal.careHistory.efficiencySeconds=10;
    ideal.careHistory.nutrientGrowth=1; ideal.careHistory.nutrientSupply=1;
    assert(quality.Evaluate(ideal).harvestSize.multiplier==3.f);

    farm::FarmGrid grid; assert(grid.Initialize(5,4));
    FarmEconomySystem economy; farm::FarmRules rules; rules.initialMoney=10000; economy.Initialize(rules);
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason);
    FarmDocumentSystem documents;
    const auto tick=std::chrono::steady_clock::now().time_since_epoch().count();
    const std::string directory="generated/codex_checks/formal_documents_"+std::to_string(tick);
    assert(documents.Initialize(directory,grid,economy,selection,&date,&progression));
    for(int i=0;i<3;++i) {
        assert(economy.BuySeed(crops[i]).Succeeded());
        for(int stage=0;stage<4;++stage) {
            auto candidate=ideal; auto* tile=&candidate; tile->crop=crops[i];
            tile->state=farm::FarmTileState::Planted; tile->heightLevel=i==2 ? 0 : 1;
            tile->growth=std::array{.10f,.45f,.8f,1.f}[stage]; tile->moisture=.55f;
            tile->careHistory.nutrientGrowth=tile->growth; tile->careHistory.nutrientSupply=tile->growth;
            assert(grid.SetTile(stage*5+i,*tile));
        }
        ideal.crop=crops[i]; assert(economy.AddHarvest(quality.Evaluate(ideal),1,1));
    }
    assert(selection.SetSelectedCrop(CropType::Pumpkin));
    assert(documents.SaveAs("Formal crops QA",grid,economy,selection));
    const auto path=documents.GetPath(), id=documents.GetActiveDocumentId();
    nlohmann::json original; assert(JsonFile::Load(path,original));
    assert(original["schemaVersion"]==22 && original["economy"]["seedCounts"].size()==4);
    economy.Initialize(); selection.Initialize();
    assert(documents.Load(id,grid,economy,selection));
    assert(selection.GetSelectedCrop()==CropType::Pumpkin);
    for(int i=0;i<3;++i) {
        assert(grid.GetTile(i)->crop==crops[i]);
        assert(economy.GetSeedCount(crops[i])==1 && economy.GetCropCount(crops[i])==1);
        assert(economy.GetHarvestRecord(i)->quality.crop==crops[i]);
    }
    for(int mode=0;mode<6;++mode) {
        auto bad=original;
        if(mode==0) bad["economy"]["seedCounts"]=nlohmann::json::array({1,2});
        if(mode==1) bad["economy"]["seedCounts"][2]=4294967297ULL;
        if(mode==2) bad["economy"]["seedCounts"][3]=-1;
        if(mode==3) bad["tiles"][0]["crop"]="Unknown";
        if(mode==4) bad["schemaVersion"]=4294967312ULL;
        if(mode==5) bad["cropSelection"]["crop"]="Unknown";
        const auto generation=economy.GetInventoryGeneration();
        assert(JsonFile::Save(path,bad) && !documents.Load(id,grid,economy,selection));
        assert(economy.GetInventoryGeneration()==generation && selection.GetSelectedCrop()==CropType::Pumpkin);
    }
    assert(JsonFile::Save(path,original));

    // Legacy IDs stay readable, but playable state migrates without losing inventory or results.
    FarmDocumentSystem oldDocs; farm::FarmGrid oldGrid; assert(oldGrid.Initialize(1,1));
    FarmEconomySystem oldEconomy; oldEconomy.Initialize();
    FarmCropSelectionSystem oldSelection; oldSelection.Initialize();
    assert(oldSelection.SetSelectedCrop(CropType::TestCrop));
    auto legacyTile=*oldGrid.GetTile(0); auto* oldTile=&legacyTile; oldTile->crop=CropType::TestCrop;
    oldTile->state=farm::FarmTileState::Planted; oldTile->growth=.4f;
    assert(oldGrid.SetTile(0,*oldTile));
    assert(oldEconomy.BuySeed(CropType::TestCrop).Succeeded());
    assert(oldEconomy.AddSeed(CropType::Carrot,2));
    farm::FarmTile mature; mature.state=farm::FarmTileState::Planted; mature.growth=1;
    mature.careHistory.goodSeconds=10; mature.careHistory.efficiencySeconds=10;
    mature.careHistory.nutrientGrowth=1; mature.careHistory.nutrientSupply=1;
    mature.crop=CropType::Carrot; assert(oldEconomy.AddHarvest(quality.Evaluate(mature),1,1));
    mature.crop=CropType::TestCrop; assert(oldEconomy.AddHarvest(quality.Evaluate(mature),1,1));
    int reservedId=oldEconomy.GetHarvestRecord(1)->id;
    assert(oldEconomy.SetHarvestProtection(reservedId,true,oldEconomy.GetInventoryGeneration()));
    assert(oldEconomy.SetContestReservation(reservedId,true,oldEconomy.GetInventoryGeneration()));
    assert(FarmContestSubmissionSystem::Submit(oldEconomy,10,reservedId,oldEconomy.GetInventoryGeneration()));
    assert(oldEconomy.AddHarvest(quality.Evaluate(mature),1,1));
    reservedId=oldEconomy.GetHarvestRecord(1)->id;
    assert(oldEconomy.SetHarvestProtection(reservedId,true,oldEconomy.GetInventoryGeneration()));
    assert(oldEconomy.SetContestReservation(reservedId,true,oldEconomy.GetInventoryGeneration()));
    const auto beforeMigration=oldEconomy.CaptureSnapshot();
    FarmDateSystem oldDate; oldDate.Initialize(); assert(oldDate.RestoreSnapshot({10,0,1}));
    assert(oldDocs.Initialize(directory+"_legacy",oldGrid,oldEconomy,oldSelection,&oldDate));
    assert(oldDocs.SaveAs("Legacy turnip",oldGrid,oldEconomy,oldSelection));
    nlohmann::json legacy; assert(JsonFile::Load(oldDocs.GetPath(),legacy)); legacy["schemaVersion"]=15;
    for(const char* key : {"seedCounts","cropCounts","cropValues"}) {
        const auto a=legacy["economy"][key]; legacy["economy"][key]=nlohmann::json::array({a[0],a[1]});
    }
    for(int version : {15,16}) {
        auto source=legacy; source["schemaVersion"]=version;
        if(version==16) for(const char* key : {"seedCounts","cropCounts","cropValues"}) {
            source["economy"][key].push_back(0); source["economy"][key].push_back(0);
        }
        assert(JsonFile::Save(oldDocs.GetPath(),source));
        assert(oldDocs.Load(oldDocs.GetActiveDocumentId(),oldGrid,oldEconomy,oldSelection));
        assert(oldDocs.IsDirty());
        assert(oldGrid.GetTile(0)->crop==CropType::Carrot && oldSelection.GetSelectedCrop()==CropType::Carrot);
        assert(oldGrid.GetTile(0)->growth==.4f);
        assert(oldEconomy.GetSeedCount(CropType::TestCrop)==0 && oldEconomy.GetSeedCount(CropType::Carrot)==3);
        assert(oldEconomy.GetCropCount(CropType::TestCrop)==0 && oldEconomy.GetCropCount(CropType::Carrot)==2);
        assert(oldEconomy.GetMoney()==beforeMigration.money);
        assert(oldEconomy.GetCropInventoryValue(CropType::Carrot)==beforeMigration.cropValues[0]+beforeMigration.cropValues[1]);
        assert(oldEconomy.GetContestReservationId()==reservedId && oldEconomy.GetProtectedCropCount()==1);
        assert(oldEconomy.GetLastHarvestQuality().crop==CropType::Carrot);
        assert(oldEconomy.GetLastHarvestQuality().salePrice==beforeMigration.lastHarvestQuality.salePrice);
        for(std::size_t i=0;i<oldEconomy.GetHarvestRecordCount();++i) {
            assert(oldEconomy.GetHarvestRecord(i)->quality.crop==CropType::Carrot);
            assert(oldEconomy.GetHarvestRecord(i)->id==beforeMigration.harvestRecords[i].id);
        }
        const auto& result=oldEconomy.GetContestResults()[0];
        assert(result.harvest.quality.crop==CropType::Carrot && result.contestDay==10);
        assert(result.qualityPoints==beforeMigration.contestResults[0].qualityPoints);
        assert(result.sizePoints==beforeMigration.contestResults[0].sizePoints);
        nlohmann::json onDisk; assert(JsonFile::Load(oldDocs.GetPath(),onDisk) && onDisk==source);
        assert(oldDocs.Save(oldGrid,oldEconomy,oldSelection));
        assert(oldDocs.Load(oldDocs.GetActiveDocumentId(),oldGrid,oldEconomy,oldSelection) && !oldDocs.IsDirty());
        assert(oldEconomy.GetSeedCount(CropType::Carrot)==3); // Repeated loads never duplicate the migration.
    }
    for(int mode=0;mode<3;++mode) {
        auto overflow=legacy;
        if(mode==0) overflow["economy"]["seedCounts"]=nlohmann::json::array({(std::numeric_limits<int>::max)(),1});
        else {
            overflow["economy"]["harvestRecords"]=nlohmann::json::array();
            overflow["economy"]["contestReservationId"]=0;
            overflow["economy"]["cropCounts"]=nlohmann::json::array({mode==1 ? (std::numeric_limits<int>::max)() : 1,1});
            overflow["economy"]["cropValues"]=nlohmann::json::array({(std::numeric_limits<int>::max)(),1});
        }
        const auto generation=oldEconomy.GetInventoryGeneration(), gridGeneration=oldGrid.GetGeneration();
        assert(JsonFile::Save(oldDocs.GetPath(),overflow));
        assert(!oldDocs.Load(oldDocs.GetActiveDocumentId(),oldGrid,oldEconomy,oldSelection));
        assert(oldEconomy.GetInventoryGeneration()==generation && oldGrid.GetGeneration()==gridGeneration);
        assert(oldEconomy.GetSeedCount(CropType::Carrot)==3 && oldEconomy.GetContestReservationId()==reservedId);
    }
    legacy["tiles"][0]["crop"]="Tomato";
    assert(JsonFile::Save(oldDocs.GetPath(),legacy) && !oldDocs.Load(oldDocs.GetActiveDocumentId(),oldGrid,oldEconomy,oldSelection));
    // Old standalone farm_stage imports follow the same playable crop policy.
    legacy["tiles"][0]["crop"]="TestCrop";
    const auto importDirectory=directory+"_import";
    std::filesystem::create_directories(importDirectory);
    assert(JsonFile::Save(importDirectory+"/farm_stage.json",legacy));
    FarmDocumentSystem imported; farm::FarmGrid importedGrid; assert(importedGrid.Initialize(1,1));
    FarmEconomySystem importedEconomy; importedEconomy.Initialize();
    FarmCropSelectionSystem importedSelection; importedSelection.Initialize();
    assert(imported.Initialize(importDirectory,importedGrid,importedEconomy,importedSelection));
    assert(importedGrid.GetTile(0)->crop==CropType::Carrot);
    std::cout<<"PASS: formal crop selection/water/soil/quality/size/mesh/trade/schema22 and legacy15 migration\n";
    std::cout<<"Synthetic visual fixture: "<<directory<<'\n';
}
