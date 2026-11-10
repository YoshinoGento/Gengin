#include "farm/system/FarmDevelopmentSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/render/FarmMeshLayout.h"
#include "3d/LineDrawer.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

// Pure mesh tests do not create a graphics device.
void LineDrawer::DrawLine(const Vector3&, const Vector3&, const Vector4&) {}
void LineDrawer::DrawWireCube(const Vector3&, float, const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3&, float, const Vector4&, uint32_t) {}

namespace {
using D = FarmDevelopmentSystem;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void Sales(FarmEconomySystem& economy, int sales) {
    auto snapshot = economy.CaptureSnapshot(); snapshot.lifetimeSales = sales;
    Check(economy.RestoreSnapshot(snapshot), "sales fixture");
}
FarmCropQualityResult Quality(farm::CropType crop) {
    FarmCropQualityResult q;
    q.crop=crop; q.basePrice=100; q.salePrice=150; q.score=90;
    q.maturity=1; q.waterBalance=.8f; q.terrainFit=.8f;
    q.nutrientBalance=.8f; q.nutrientKnown=true; q.harvestSize={1.25f,true};
    return q;
}
bool Same(const farm::FarmTile& a, const farm::FarmTile& b) {
    return a.heightLevel==b.heightLevel && a.feature==b.feature && a.state==b.state &&
        a.crop==b.crop && a.moisture==b.moisture && a.growth==b.growth && a.waterAmount==b.waterAmount &&
        a.irrigationEnabled==b.irrigationEnabled && a.soilNutrients==b.soilNutrients && a.pests==b.pests &&
        a.careHistory.drySeconds==b.careHistory.drySeconds && a.careHistory.lowSeconds==b.careHistory.lowSeconds &&
        a.careHistory.goodSeconds==b.careHistory.goodSeconds && a.careHistory.excessSeconds==b.careHistory.excessSeconds &&
        a.careHistory.efficiencySeconds==b.careHistory.efficiencySeconds &&
        a.careHistory.nutrientGrowth==b.careHistory.nutrientGrowth && a.careHistory.nutrientSupply==b.careHistory.nutrientSupply;
}
void Economy() {
    FarmEconomySystem economy; economy.Initialize();
    Check(!economy.SellAll().Succeeded() && economy.GetLifetimeSales()==0, "empty sale");
    Check(economy.AddHarvest(Quality(farm::CropType::Carrot),4,8), "harvest");
    Check(economy.AddHarvest(Quality(farm::CropType::Tomato),1,8), "reserved harvest");
    const int id=economy.GetHarvestRecord(1)->id;
    Check(economy.SetHarvestProtection(id,true,economy.GetInventoryGeneration()) &&
        economy.SetContestReservation(id,true,economy.GetInventoryGeneration()), "protect/reserve");
    Check(economy.PreviewSale().earnedMoney==600 && economy.GetLifetimeSales()==0, "preview adds sales");
    Check(economy.SellAll().earnedMoney==600 && economy.GetLifetimeSales()==600 &&
        economy.GetTotalCropCount()==1, "committed sale accounting");
    Check(!economy.SellAll().Succeeded() && economy.GetLifetimeSales()==600, "protected sale accounting");
    Check(economy.BuySeed(farm::CropType::Carrot).Succeeded() && economy.GetLifetimeSales()==600, "purchase reduces sales");
    auto snapshot=economy.CaptureSnapshot(); snapshot.money=INT_MAX;
    Check(economy.RestoreSnapshot(snapshot) && economy.AddHarvest(Quality(farm::CropType::Carrot)), "overflow fixture");
    Check(!economy.SellAll().Succeeded() && economy.GetLifetimeSales()==600, "failed sale added sales");
    snapshot=economy.CaptureSnapshot(); snapshot.money=0; snapshot.lifetimeSales=INT_MAX-1;
    Check(economy.RestoreSnapshot(snapshot) && economy.SellAll().Succeeded() &&
        economy.GetLifetimeSales()==INT_MAX, "sales saturation");
    snapshot=economy.CaptureSnapshot(); snapshot.lifetimeSales=-1;
    Check(!economy.RestoreSnapshot(snapshot) && economy.GetLifetimeSales()==INT_MAX, "negative sales accepted");
    economy.Initialize(); Check(economy.GetLifetimeSales()==0, "new game retains revenue");
}
void Expansion() {
    for (int sales : {0,499,500,1499,1500,INT_MAX})
        Check(D::Rank(sales)==(sales>=1500 ? 3 : sales>=500 ? 2 : 1), "rank boundary");
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "grid");
    FarmEconomySystem economy; economy.Initialize();
    farm::FarmTile crop;
    crop.state=farm::FarmTileState::Planted; crop.crop=farm::CropType::Carrot; crop.growth=.4f;
    crop.moisture=.5f; crop.soilNutrients=.7f; crop.irrigationEnabled=false;
    crop.careHistory={1,2,3,4,5,.2f,.1f}; crop.pests={6,1};
    farm::FarmTile source; source.feature=farm::FarmTileFeature::WaterSource; source.waterAmount=.7f;
    Check(grid.SetTile(18,crop) && grid.SetTile(19,source) && grid.SetSelectedIndex(19), "tile fixture");
    farm::FarmGrid::Snapshot before; grid.CaptureSnapshot(before);
    Check(!D::Expand(grid,economy,grid.GetGeneration(),4), "unearned expansion");
    Sales(economy,500);
    Check(!D::Inspect(grid,economy,true).canExpand && D::Inspect(grid,economy).remainingSales==1000, "view guards");
    const auto oldGeneration=grid.GetGeneration(); const int money=economy.GetMoney();
    Check(D::Expand(grid,economy,oldGeneration,4) && grid.GetHeight()==5 && grid.GetSelectedIndex()==19, "first expansion");
    Check(!D::Expand(grid,economy,oldGeneration,4) && !D::Expand(grid,economy,grid.GetGeneration(),5), "duplicate/unearned row");
    Sales(economy,1500);
    Check(D::Expand(grid,economy,grid.GetGeneration(),5) && grid.GetHeight()==6 && economy.GetMoney()==money, "second expansion charged money");
    Check(!D::Expand(grid,economy,grid.GetGeneration(),6), "maximum area exceeded");
    for (int i=0;i<30;++i)
        Check(Same(*grid.GetTile(i),i<20 ? before.tiles[i] : farm::FarmTile{}), "expansion changed existing cells/defaults");
    Check(!grid.RestoreSnapshot(before), "ordinary Undo changed dimensions");
    farm::FarmGrid::Snapshot expanded; grid.CaptureSnapshot(expanded);
    for (int type=0;type<5;++type) {
        auto bad=expanded;
        if(type==0) bad.width=INT_MAX;
        if(type==1) bad.selectedY=6;
        if(type==2) bad.tiles.pop_back();
        if(type==3) bad.tiles[0].moisture=std::numeric_limits<float>::quiet_NaN();
        if(type==4) bad.height=0;
        auto generation=grid.GetGeneration();
        Check(!grid.ReplaceSnapshot(bad) && grid.GetGeneration()==generation && Same(*grid.GetTile(18),crop), "invalid replace mutated grid");
    }
    farm::FarmGrid custom; Check(custom.Initialize(3,3), "custom");
    Check(!D::Expand(custom,economy,custom.GetGeneration(),3), "custom size expansion");
    farm::FarmIrrigationSystem irrigation; irrigation.Initialize(); irrigation.Rebuild(grid);
    farm::FarmTile soil; soil.state=farm::FarmTileState::Tilled;
    farm::FarmTile canal; canal.feature=farm::FarmTileFeature::Canal; canal.waterAmount=.7f;
    Check(grid.SetTile(24,canal) && grid.SetTile(29,soil), "new canal/soil");
    irrigation.Rebuild(grid);
    Check(irrigation.UpdateWater(grid,.25f,1) && grid.GetTile(29)->moisture>0, "new row not irrigated");
}
void Save() {
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "save grid");
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::FreeFarming);
    FarmDocumentSystem docs;
    const auto root=std::filesystem::path("generated/codex_checks") /
        ("development_save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Check(docs.Initialize(root.string(),grid,economy,selection,&date,&progression), "docs init");
    Check(docs.SaveAs("Initial",grid,economy,selection), "initial save");
    const auto initialId=docs.GetActiveDocumentId();
    Sales(economy,1500);
    Check(D::Expand(grid,economy,grid.GetGeneration(),4) && D::Expand(grid,economy,grid.GetGeneration(),5), "save expansion");
    farm::FarmTile tile; tile.state=farm::FarmTileState::Tilled; tile.soilNutrients=.9f;
    Check(grid.SetTile(29,tile) && grid.SetSelectedIndex(29) && docs.SaveAs("Expanded",grid,economy,selection), "expanded save");
    const auto id=docs.GetActiveDocumentId(), path=docs.GetPath();
    nlohmann::json saved;
    Check(JsonFile::Load(path,saved) && saved["schemaVersion"]==22 && saved["economy"]["lifetimeSales"]==1500, "schema22");
    Check(docs.Load(initialId,grid,economy,selection) && grid.GetHeight()==4 && economy.GetLifetimeSales()==0, "smaller save load");
    Check(docs.Load(id,grid,economy,selection) && grid.GetHeight()==6 && grid.GetSelectedIndex()==29 &&
        Same(*grid.GetTile(29),tile) && economy.GetLifetimeSales()==1500, "expanded save load");
    for (int kind=0;kind<11;++kind) {
        auto bad=saved;
        if(kind==0) bad["economy"].erase("lifetimeSales");
        if(kind==1) bad["economy"]["lifetimeSales"]=-1;
        if(kind==2) bad["economy"]["lifetimeSales"]=4294967296ULL;
        if(kind==3) bad["economy"]["lifetimeSales"]=true;
        if(kind==4) bad["economy"]["lifetimeSales"]=1500.0;
        if(kind==5) bad["economy"]["lifetimeSales"]=nullptr;
        if(kind==6) bad["grid"]["width"]=4294967301ULL;
        if(kind==7) bad["grid"]["selectedIndex"]=30;
        if(kind==8) bad["grid"]["height"]=7;
        if(kind==9) bad["tiles"].erase(bad["tiles"].begin());
        if(kind==10) bad["schemaVersion"]=23;
        const auto generation=grid.GetGeneration();
        Check(JsonFile::Save(path,bad) && !docs.Load(id,grid,economy,selection) && grid.GetGeneration()==generation &&
            grid.GetHeight()==6 && economy.GetLifetimeSales()==1500 && Same(*grid.GetTile(29),tile), "invalid load not atomic");
    }
    auto legacy=saved; legacy["schemaVersion"]=20; legacy["economy"].erase("lifetimeSales");
    Check(JsonFile::Save(path,legacy) && docs.Load(id,grid,economy,selection) && economy.GetLifetimeSales()==0, "legacy revenue invented");
    Check(JsonFile::Save(path,saved) && docs.Load(id,grid,economy,selection), "restore fixture");
    farm::FarmGrid fresh; Check(fresh.Initialize(5,4), "fresh grid");
    FarmEconomySystem freshEconomy; freshEconomy.Initialize();
    FarmDocumentSystem reopened;
    Check(reopened.Initialize(root.string(),fresh,freshEconomy,selection) && fresh.GetHeight()==6 &&
        freshEconomy.GetLifetimeSales()==1500, "startup expanded save");
    Check(docs.Reset(grid,economy,selection) && grid.GetHeight()==4 && grid.GetSelectedIndex()==0 &&
        economy.GetLifetimeSales()==0, "reset did not reset rank/area");
}
void MeshBounds() {
    farm::FarmGrid grid; Check(grid.Initialize(5,6), "mesh grid");
    farm::FarmVisualSystem visual; visual.Initialize({});
    std::size_t maximum=0;
    for (int pattern=0;pattern<8;++pattern) {
        for(int i=0;i<30;++i) {
            farm::FarmTile tile; tile.heightLevel=(i+i/5)%3;
            if(pattern<4) tile.state=pattern%2 ? farm::FarmTileState::Tilled : farm::FarmTileState::Empty;
            else if(pattern==4 || (i+i/5)%2) {
                tile.feature=farm::FarmTileFeature::Canal; tile.waterAmount=pattern==7 ? 0.f : .8f;
            }
            Check(grid.SetTile(i,tile), "mesh fixture");
        }
        std::size_t parts=0;
        for(int i=0;i<30;++i) {
            const auto mesh=farm::BuildFarmTileMeshParts(grid,i,visual);
            Check(mesh.count>0 && mesh.count<=mesh.parts.size(), "mesh capacity"); parts+=mesh.count;
            for(std::size_t j=0;j<mesh.count;++j) {
                const auto& p=mesh.parts[j];
                Check(std::isfinite(p.position.x) && std::isfinite(p.position.y) && std::isfinite(p.position.z) &&
                    p.scale.x>0 && p.scale.y>0 && p.scale.z>0, "invalid part geometry");
            }
            const auto center=visual.GetTileCenter(grid,i);
            Check(std::abs(center.x)+.715f<4 && center.z-.715f>3.2f && center.z+.715f<12.8f, "outside scenery keepout");
        }
        maximum=(std::max)(maximum,parts);
        Check(parts<=30*32, "expanded renderer budget");
    }
    std::cout << "30-cell mesh maximum in tested patterns: " << maximum << "/960\n";
}
}
int main() try {
    Economy(); Expansion(); Save(); MeshBounds();
    std::cout << "farm_development_test: PASS\n";
    return 0;
} catch(const std::exception& e) { std::cerr << "farm_development_test: " << e.what() << '\n'; return 1; }
