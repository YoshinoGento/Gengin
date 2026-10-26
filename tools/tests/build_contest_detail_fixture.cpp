#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>

// Synthetic visual-QA records, not cultivation/balance evidence or a shipping save.
int main() {
    const std::filesystem::path root="generated/codex_checks/contest_detail_20261026/fixture";
    if (std::filesystem::exists(root)) throw std::runtime_error("Fixture already exists; never overwrite it");
    const auto check=[](bool ok) { if (!ok) throw std::runtime_error("Invalid QA fixture"); };
    farm::FarmGrid grid; check(grid.Initialize(5,4));
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason);
    FarmDocumentSystem docs; check(docs.Initialize(root.string(),grid,economy,selection,&date,&progression));
    auto snapshot=economy.CaptureSnapshot();
    const auto record=[](farm::CropType crop,int id,int day,int score,float size) {
        FarmEconomySystem::HarvestRecord r;
        r.id=id; r.quantity=1; r.saleProtected=true; r.harvestedDay=day;
        r.quality.crop=crop; r.quality.score=score; r.quality.basePrice=100; r.quality.salePrice=100;
        r.quality.maturity=1; r.quality.waterBalance=.6f; r.quality.terrainFit=.8f;
        r.quality.nutrientBalance=.9f; r.quality.nutrientKnown=true; r.quality.harvestSize={size,true};
        return r;
    };
    const std::array<FarmEconomySystem::HarvestRecord,3> records{{
        record(farm::CropType::Carrot,1,7,60,.9f),
        record(farm::CropType::Tomato,2,18,95,1.8f),
        record(farm::CropType::Pumpkin,3,27,90,1.6f)}};
    for (int i=0;i<2;++i) {
        const auto judge=FarmContestJudgeSystem::Evaluate(&records[i]); check(judge.IsValid());
        snapshot.contestResults[i]={records[i],(i+1)*10,judge.qualityPoints,judge.sizePoints};
    }
    snapshot.harvestRecords[0]=records[2]; snapshot.harvestRecordCount=1;
    snapshot.nextHarvestRecordId=4; snapshot.contestReservationId=3;
    const int slot=farm::ToCropSlot(farm::CropType::Pumpkin);
    snapshot.cropCounts[slot]=1; snapshot.cropValues[slot]=100;
    snapshot.lastHarvestQuality=records[2].quality;
    check(economy.RestoreSnapshot(snapshot)); check(date.RestoreSnapshot({30,0,1}));
    check(docs.SaveAs("Contest comparison QA",grid,economy,selection));
    check(docs.Load(docs.GetActiveDocumentId(),grid,economy,selection));
    check(date.GetDay()==30 && progression.StagePolicy().enabled && economy.GetContestReservation());
    std::cout << "PASS: isolated synthetic day30 fixture " << docs.GetPath() << '\n';
}
