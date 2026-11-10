#include "farm/system/FarmRecordBookSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmContestSubmissionSystem.h"
#include "farm/core/FarmGrid.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Crop = farm::CropType;
using Book = FarmRecordBookSystem;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
FarmCropQualityResult Quality(Crop crop, int score=80, float size=1.5f) {
    FarmCropQualityResult q;
    q.crop=crop; q.basePrice=100; q.salePrice=150; q.score=score;
    q.maturity=1; q.waterBalance=.8f; q.terrainFit=.8f; q.nutrientBalance=.8f;
    q.nutrientKnown=true; q.harvestSize={size,true};
    return q;
}
bool Same(const Book::View& a, const Book::View& b) {
    if (a.collected!=b.collected || a.nextCrop!=b.nextCrop) return false;
    for (std::size_t i=0;i<a.rows.size();++i) {
        const auto& x=a.rows[i]; const auto& y=b.rows[i];
        if (x.crop!=y.crop || x.harvested!=y.harvested || x.weightKnown!=y.weightKnown ||
            x.quality!=y.quality || x.grams!=y.grams) return false;
    }
    return true;
}
void Inventory() {
    FarmEconomySystem e; e.Initialize();
    Check(Book::Inspect(e).collected==0, "empty records");
    Check(e.AddHarvest(Crop::Carrot,3) && e.SellAll().Succeeded() && Book::Inspect(e).collected==0,
        "legacy aggregates invented quality");
    const auto high=Quality(Crop::Carrot,95,1), large=Quality(Crop::Carrot,30,2);
    Check(e.AddHarvest(high) && e.AddHarvest(large), "independent maxima fixture");
    auto view=Book::Inspect(e);
    Check(view.collected==1 && view.rows[0].quality==95 && view.rows[0].grams==300, "independent maxima");
    Check(!e.GetSoldCropRecords()[0].harvested, "harvest prematurely archived");
    Check(e.RemoveHarvest(large,1,high) && Book::Inspect(e).rows[0].grams==150, "undo retains phantom size");
    Check(e.AddHarvest(large), "redo fixture");
    auto unknown=Quality(Crop::Tomato,0); unknown.harvestSize={};
    Check(e.AddHarvest(unknown) && Book::Inspect(e).collected==2 && !Book::Inspect(e).rows[1].weightKnown,
        "unknown size/zero score lost");
    const int id=e.GetHarvestRecord(2)->id;
    Check(e.SetHarvestProtection(id,true,e.GetInventoryGeneration()), "protect");
    view=Book::Inspect(e);
    Check(e.PreviewSale().Succeeded() && !e.GetSoldCropRecords()[0].harvested, "preview mutated archive");
    Check(e.SellAll().Succeeded() && Same(view,Book::Inspect(e)) && e.GetSoldCropRecords()[0].harvested &&
        !e.GetSoldCropRecords()[1].harvested, "sale/protection retention");
    Check(!e.SellAll().Succeeded() && Same(view,Book::Inspect(e)), "empty sale changes record");
    auto snapshot=e.CaptureSnapshot(); snapshot.money=INT_MAX;
    Check(e.RestoreSnapshot(snapshot) && e.AddHarvest(Quality(Crop::Pumpkin,100,8)), "overflow fixture");
    Check(!e.SellAll().Succeeded() && !e.GetSoldCropRecords()[2].harvested, "failed sale archived harvest");
    Check(Book::Inspect(e).collected==3 && Book::Inspect(e).nextCrop==Crop::None &&
        Book::Inspect(e).rows[2].grams==12000, "full collection/maximum grams");
    Check(e.RemoveHarvest(Quality(Crop::Pumpkin,100,8)), "remove failed-sale record");
    Check(Book::Inspect(e).collected==2, "failed sale produced persistent record");
    e.Initialize();
    for (std::size_t i=0;i<FarmEconomySystem::kMaxHarvestRecords;++i)
        Check(e.AddHarvest(Quality(Crop::Carrot,static_cast<int>(i%101),1)), "capacity fixture");
    Check(!e.AddHarvest(Quality(Crop::Pumpkin)) && Book::Inspect(e).collected==1 &&
        Book::Inspect(e).rows[0].quality==100, "capacity affects book");
    Check(e.SellAll().Succeeded() && e.GetHarvestRecordCount()==0 && Book::Inspect(e).collected==1,
        "archive loses full inventory");
    e.Initialize(); Check(Book::Inspect(e).collected==0, "initialize retains records");
}
void UndoAndSubmit() {
    farm::FarmGrid grid; Check(grid.Initialize(1,1), "grid");
    FarmEconomySystem e; e.Initialize();
    FarmToolActionSystem tools; tools.Initialize();
    farm::FarmTile ready; ready.crop=Crop::Carrot; ready.state=farm::FarmTileState::Planted;
    ready.growth=1; ready.moisture=.5f;
    ready.careHistory.goodSeconds=60; ready.careHistory.efficiencySeconds=60;
    ready.careHistory.nutrientGrowth=1; ready.careHistory.nutrientSupply=1;
    Check(grid.SetTile(0,ready), "ready crop");
    Check(tools.ApplyToolDetailed(grid,FarmTool::Harvest,Crop::Carrot,e,8).Succeeded(), "real harvest");
    auto view=Book::Inspect(e); Check(view.collected==1 && view.rows[0].weightKnown, "real harvest record");
    Check(tools.Undo() && Book::Inspect(e).collected==0, "real undo record");
    Check(tools.Redo() && Same(view,Book::Inspect(e)), "real redo record");
    const int id=e.GetHarvestRecord(0)->id;
    const auto generation=e.GetInventoryGeneration();
    Check(tools.CommitHarvestProtection(e,id,true,generation) &&
        tools.CommitContestReservation(e,id,true,generation), "reservation");
    Check(!FarmContestSubmissionSystem::Submit(e,10,id,generation+1) && Same(view,Book::Inspect(e)), "stale submit");
    Check(tools.CommitContestSubmission(e,10,id,generation) && e.GetHarvestRecordCount()==0 &&
        Same(view,Book::Inspect(e)), "submission loses record");
    Check(!tools.Undo() && !tools.Redo(), "submission retains obsolete command");
    Check(!e.GetSoldCropRecords()[0].harvested, "submission counted as sale");
}
void Save() {
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "save grid");
    FarmEconomySystem e; e.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDateSystem date; date.Initialize(); Check(date.RestoreSnapshot({10,0,1}), "date");
    FarmDocumentSystem docs;
    const auto root=std::filesystem::path("generated/codex_checks") /
        ("record_book_save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(docs.Initialize(root.string(),grid,e,selection,&date), "docs init");
    Check(e.AddHarvest(Quality(Crop::Carrot),1,8) && e.SellAll().Succeeded(), "sold fixture");
    Check(e.AddHarvest(Quality(Crop::Pumpkin),1,8), "submission fixture");
    int id=e.GetHarvestRecord(0)->id;
    Check(e.SetHarvestProtection(id,true,e.GetInventoryGeneration()) &&
        e.SetContestReservation(id,true,e.GetInventoryGeneration()) &&
        FarmContestSubmissionSystem::Submit(e,10,id,e.GetInventoryGeneration()), "submit fixture");
    Check(e.AddHarvest(Quality(Crop::Tomato),1,9), "live fixture");
    const auto expected=Book::Inspect(e);
    Check(docs.SaveAs("Records",grid,e,selection), "save");
    const auto documentId=docs.GetActiveDocumentId(), path=docs.GetPath();
    nlohmann::json saved; Check(JsonFile::Load(path,saved) && saved["schemaVersion"]==22, "schema22");
    e.Initialize(); Check(docs.Load(documentId,grid,e,selection) && Same(expected,Book::Inspect(e)), "load");
    auto minimum=saved; minimum["economy"]["soldCropRecords"][0]["sizeMultiplier"]=0.01f;
    Check(JsonFile::Save(path,minimum) && docs.Load(documentId,grid,e,selection) && Book::Inspect(e).rows[0].weightKnown,
        "minimum float size cannot roundtrip");
    Check(JsonFile::Save(path,saved) && docs.Load(documentId,grid,e,selection), "restore minimum fixture");
    for (int kind=0;kind<17;++kind) {
        auto bad=saved; auto& records=bad["economy"]["soldCropRecords"];
        if(kind==0) bad["economy"].erase("soldCropRecords");
        if(kind==1) records=nullptr;
        if(kind==2) records.erase(records.begin());
        if(kind==3) records.push_back(records[0]);
        if(kind==4) records[0]["crop"]="Pumpkin";
        if(kind==5) records[0]["harvested"]=1;
        if(kind==6) records[0]["bestQuality"]=101;
        if(kind==7) records[0]["bestQuality"]=-1;
        if(kind==8) records[0]["bestQuality"]=4294967296ULL;
        if(kind==9) records[0]["bestQuality"]=80.0;
        if(kind==10) records[0]["sizeKnown"]=false;
        if(kind==11) records[0]["sizeMultiplier"]=8.1;
        if(kind==12) records[0]["sizeMultiplier"]=1e100;
        if(kind==13) records[0]["harvested"]=false;
        if(kind==14) bad["schemaVersion"]=23;
        if(kind==15) records[0]["sizeMultiplier"]=8.0000001;
        if(kind==16) { records[0]["sizeKnown"]=false; records[0]["sizeMultiplier"]=1e-100; }
        const auto generation=e.GetInventoryGeneration();
        Check(JsonFile::Save(path,bad) && !docs.Load(documentId,grid,e,selection) &&
            generation==e.GetInventoryGeneration() && Same(expected,Book::Inspect(e)), "invalid load mutated record");
    }
    auto legacy=saved; legacy["schemaVersion"]=21; legacy["economy"].erase("soldCropRecords");
    Check(JsonFile::Save(path,legacy) && docs.Load(documentId,grid,e,selection), "legacy load");
    auto view=Book::Inspect(e);
    Check(view.collected==2 && !view.rows[0].harvested && view.rows[1].harvested && view.rows[2].harvested,
        "legacy sold records invented/live and submitted lost");
    Check(JsonFile::Save(path,saved) && docs.Load(documentId,grid,e,selection), "restore saved fixture");
    Check(docs.Reset(grid,e,selection) && Book::Inspect(e).collected==0, "reset retains other farm records");
}
void Invalid() {
    FarmEconomySystem e; e.Initialize();
    for (int kind=0;kind<7;++kind) {
        auto bad=e.CaptureSnapshot(); auto& r=bad.soldCropRecords[0];
        r.harvested=true; r.bestQuality=80; r.largest={1,true};
        if(kind==0) r.largest.multiplier=std::numeric_limits<float>::quiet_NaN();
        if(kind==1) r.largest.multiplier=std::numeric_limits<float>::infinity();
        if(kind==2) r.largest.multiplier=-1;
        if(kind==3) r.largest={1,false};
        if(kind==4) r.bestQuality=101;
        if(kind==5) r.harvested=false;
        if(kind==6) r.largest={0,true};
        Check(!e.RestoreSnapshot(bad) && Book::Inspect(e).collected==0, "invalid snapshot accepted");
    }
    FarmCropRecordSystem::Records records{};
    auto bad=Quality(Crop::Carrot); bad.waterBalance=std::numeric_limits<float>::quiet_NaN();
    FarmCropRecordSystem::Accumulate(records,bad);
    FarmCropRecordSystem::Accumulate(records,Quality(static_cast<Crop>(999)));
    FarmCropRecordSystem::Accumulate(records,Quality(Crop::TestCrop));
    Check(!records[0].harvested && FarmCropRecordSystem::Validate(records), "invalid crop/quality accepted");
}
}
int main() try {
    Inventory(); UndoAndSubmit(); Save(); Invalid();
    std::cout << "farm_record_book_test: PASS\n";
    return 0;
} catch (const std::exception& e) { std::cerr << "farm_record_book_test: " << e.what() << '\n'; return 1; }
