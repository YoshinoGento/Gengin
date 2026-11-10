#include "farm/system/FarmRecoverySystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using R = FarmRecoverySystem;
using S = R::Status;
using Crop = farm::CropType;
void Check(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
void Empty(FarmEconomySystem& e, int money = 0) {
    e.Initialize(); auto snapshot = e.CaptureSnapshot();
    snapshot.money = money; snapshot.seedCounts.fill(0);
    Check(e.RestoreSnapshot(snapshot), "empty economy");
}
bool Claim(farm::FarmGrid& grid, FarmEconomySystem& e, int day) {
    return R::Claim(grid,e,day,R::Inspect(grid,e,day));
}
void Rules() {
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "grid");
    FarmEconomySystem e; Empty(e);
    auto request = R::Inspect(grid,e,1);
    Check(request.CanClaim() && !R::Claim(grid,e,1,request,true), "blocked claim");
    Check(R::Claim(grid,e,1,request), "initial claim");
    Check(e.GetSeedCount(Crop::Carrot)==1 && e.GetMoney()==0 && e.GetLifetimeSales()==0 &&
        e.GetLastRecoveryDay()==1, "grant mutated money or quantity");
    Check(!R::Claim(grid,e,1,request), "double click");
    Check(e.RemoveSeed(Crop::Carrot) && R::Inspect(grid,e,1).status==S::ClaimedToday && !Claim(grid,e,1), "daily cap");
    Check(Claim(grid,e,2) && e.RemoveSeed(Crop::Carrot) && !Claim(grid,e,1), "next/reversed day");
    Check(!R::Inspect(grid,e,0).CanClaim() && !R::Inspect(grid,e,-1).CanClaim(), "invalid day");
    Check(Claim(grid,e,(std::numeric_limits<int>::max)()) && e.RemoveSeed(Crop::Carrot) &&
        !Claim(grid,e,(std::numeric_limits<int>::max)()), "maximum day overflow");
    Empty(e);
    auto bad=e.CaptureSnapshot(); bad.lastRecoveryDay=-1;
    Check(!e.RestoreSnapshot(bad) && e.GetLastRecoveryDay()==0, "negative snapshot");
    farm::FarmGrid empty;
    Check(!R::Inspect(empty,e,1).CanClaim(), "uninitialized grid");
    for (int i=0;i<farm::kPlayableCropCount;++i) {
        Empty(e); const auto crop=farm::PlayableCrop(i);
        Check(e.AddSeed(crop) && R::Inspect(grid,e,1).status==S::SeedsAvailable && !Claim(grid,e,1), "owned crop seed");
        Empty(e); auto affordable=e.CaptureSnapshot();
        affordable.money=e.GetSeedPrice(crop); affordable.seedPrices.fill(0);
        affordable.seedPrices[farm::ToCropSlot(crop)]=affordable.money;
        Check(e.RestoreSnapshot(affordable) && R::Inspect(grid,e,1).status==S::CanBuy, "any affordable seed");
        affordable.money-=1;
        Check(e.RestoreSnapshot(affordable) && R::Inspect(grid,e,1).CanClaim(), "price boundary");
    }
    Empty(e,1);
    FarmDateSystem date; FarmStagePolicy policy{true}; FarmStageAccessSystem access;
    access.Bind(date,policy); e.BindStageAccess(access);
    Check(e.AddSeed(Crop::Pumpkin) && e.AddSeed(Crop::TestCrop) && R::Inspect(grid,e,1).CanClaim(), "locked/retired seed blocks aid");
    auto prices=e.CaptureSnapshot(); prices.seedPrices[farm::ToCropSlot(Crop::Pumpkin)]=1;
    Check(e.RestoreSnapshot(prices) && R::Inspect(grid,e,1).CanClaim(), "locked affordable seed blocks aid");
    Check(date.RestoreSnapshot({21,0,1}) && R::Inspect(grid,e,21).status==S::SeedsAvailable, "unlock eligibility");
    for (auto state : {farm::FarmTileState::Planted, farm::FarmTileState::ReadyToHarvest}) {
        Empty(e); farm::FarmTile tile; tile.crop=Crop::Carrot; tile.state=state;
        tile.growth=state==farm::FarmTileState::ReadyToHarvest ? 1.0f : 0.0f;
        Check(grid.SetTile(19,tile) && R::Inspect(grid,e,21).status==S::CropGrowing && !Claim(grid,e,21), "offscreen/growing/mature crop");
        Check(grid.SetTile(19,{}), "clear tile");
    }
    Empty(e); Check(e.AddHarvest(Crop::Carrot) && R::Inspect(grid,e,21).status==S::HarvestAvailable, "unrecorded harvest");
    Empty(e);
    FarmCropQualityResult q; q.crop=Crop::Carrot; q.maturity=1; q.score=80; q.basePrice=100; q.salePrice=150;
    q.waterBalance=.8f; q.terrainFit=.8f; q.nutrientBalance=.8f; q.nutrientKnown=true; q.harvestSize={1,true};
    Check(e.AddHarvest(q,1,21), "record fixture"); const int id=e.GetHarvestRecord(0)->id;
    Check(e.SetHarvestProtection(id,true,e.GetInventoryGeneration()) &&
        e.SetContestReservation(id,true,e.GetInventoryGeneration()), "protected reserved fixture");
    Check(!e.PreviewSale().Succeeded() && R::Inspect(grid,e,21).status==S::HarvestAvailable && !Claim(grid,e,21) &&
        e.GetContestReservationId()==id && e.GetProtectedCropCount()==1, "protected reservation consumed");
    Empty(e); request=R::Inspect(grid,e,21);
    Check(!R::Claim(grid,e,22,request), "stale day");
    Check(e.RestoreSnapshot(e.CaptureSnapshot()) && !R::Claim(grid,e,21,request), "stale inventory");
    request=R::Inspect(grid,e,21);
    farm::FarmGrid::Snapshot snapshot; grid.CaptureSnapshot(snapshot);
    Check(grid.RestoreSnapshot(snapshot) && !R::Claim(grid,e,21,request), "stale farm");
    request=R::Inspect(grid,e,21);
    Check(e.AddSeed(Crop::Tomato) && !R::Claim(grid,e,21,request), "revalidate actual resources");
}
void CultivationAndHistory() {
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "tool grid");
    FarmEconomySystem e; Empty(e);
    FarmToolActionSystem tools; tools.Initialize();
    Check(Claim(grid,e,1), "tool aid");
    Check(tools.ApplyToolDetailed(grid,FarmTool::Hoe,Crop::Carrot,e).Succeeded() &&
        tools.ApplyToolDetailed(grid,FarmTool::Seed,Crop::Carrot,e).Succeeded(), "aid cannot be planted");
    Check(R::Inspect(grid,e,1).status==S::CropGrowing && e.GetLastRecoveryDay()==1, "plant removes limit");
    Check(tools.Undo() && e.GetSeedCount(Crop::Carrot)==1 && e.GetLastRecoveryDay()==1, "plant undo removes limit");
    Check(tools.Redo() && e.GetSeedCount(Crop::Carrot)==0 && e.GetLastRecoveryDay()==1, "plant redo changes aid");
}
void Save() {
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "save grid");
    FarmEconomySystem e; Empty(e);
    FarmCropSelectionSystem selection; selection.Initialize();
    const auto root=std::filesystem::path("generated/codex_checks") /
        ("recovery_save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    FarmDocumentSystem docs; Check(docs.Initialize(root.string(),grid,e,selection), "docs init");
    Check(Claim(grid,e,1) && e.RemoveSeed(Crop::Carrot), "save aid");
    Check(docs.SaveAs("Recovery",grid,e,selection), "save aid state");
    const auto path=docs.GetPath(), id=docs.GetActiveDocumentId();
    nlohmann::json saved; Check(JsonFile::Load(path,saved) && saved["schemaVersion"]==25 &&
        saved["economy"]["lastRecoveryDay"]==1, "schema25");
    Empty(e);
    Check(docs.Load(id,grid,e,selection) && e.GetLastRecoveryDay()==1 && !Claim(grid,e,1), "reload bypassed daily cap");
    for (int kind=0;kind<8;++kind) {
        auto bad=saved;
        switch(kind) {
        case 0: bad["economy"].erase("lastRecoveryDay"); break;
        case 1: bad["economy"]["lastRecoveryDay"]=-1; break;
        case 2: bad["economy"]["lastRecoveryDay"]=2; break;
        case 3: bad["economy"]["lastRecoveryDay"]=true; break;
        case 4: bad["economy"]["lastRecoveryDay"]=1.0; break;
        case 5: bad["economy"]["lastRecoveryDay"]=nullptr; break;
        case 6: bad["economy"]["lastRecoveryDay"]=std::numeric_limits<std::uint64_t>::max(); break;
        default: bad["schemaVersion"]=26; break;
        }
        const auto generation=grid.GetGeneration(), inventory=e.GetInventoryGeneration();
        Check(JsonFile::Save(path,bad) && !docs.Load(id,grid,e,selection) &&
            grid.GetGeneration()==generation && e.GetInventoryGeneration()==inventory && e.GetLastRecoveryDay()==1,
            "invalid save mutated live state");
    }
    auto legacy=saved; legacy["schemaVersion"]=24; legacy["economy"].erase("lastRecoveryDay");
    Check(JsonFile::Save(path,legacy) && docs.Load(id,grid,e,selection) && e.GetLastRecoveryDay()==0 && Claim(grid,e,1), "legacy24 migration");
    auto future=e.CaptureSnapshot(); future.lastRecoveryDay=2;
    Check(e.RestoreSnapshot(future) && !docs.Save(grid,e,selection), "future-day save accepted");
    Check(docs.Reset(grid,e,selection) && e.GetLastRecoveryDay()==0, "reset retained aid limit");
    e.Initialize(); Check(e.GetLastRecoveryDay()==0, "new farm retained limit");
}
}
int main() try {
    Rules(); CultivationAndHistory(); Save();
    std::cout << "farm_recovery_test: PASS\n"; return 0;
} catch(const std::exception& e) { std::cerr << "farm_recovery_test: " << e.what() << '\n'; return 1; }
