#include "farm/system/FarmRepellentSystem.h"
#include "farm/system/FarmPestSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmLayoutSystem.h"
#include "farm/system/FarmDevelopmentSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/system/FarmSoilSystem.h"
#include "farm/system/FarmNextGoalSystem.h"
#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/render/FarmMeshLayout.h"
#include "3d/LineDrawer.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void LineDrawer::DrawLine(const Vector3&, const Vector3&, const Vector4&) {}
void LineDrawer::DrawWireCube(const Vector3&, float, const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3&, float, const Vector4&, uint32_t) {}

namespace {
using R = FarmRepellentSystem;
using Grid = farm::FarmGrid;
using Tile = farm::FarmTile;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void Plant(Grid& grid, int index, bool enabled=true) {
    Check(grid.SelectTile(index) && R::SetPlant(grid,index,grid.GetGeneration(),enabled), "place/remove");
}
Tile Crop() { Tile tile; tile.crop=farm::CropType::Carrot; tile.state=farm::FarmTileState::Planted; return tile; }
void RangeAndActions() {
    Grid grid; Check(grid.Initialize(5,4), "grid");
    Plant(grid,6);
    for(int i=0;i<20;++i)
        Check(R::IsProtected(grid,i)==(i==1 || i==5 || i==6 || i==7 || i==11), "cross/diagonal range");
    Check(!R::IsProtected(grid,-1) && !R::IsProtected(grid,20), "invalid range");
    Check(!R::SetPlant(grid,6,grid.GetGeneration(),true), "duplicate placement");
    Check(!R::SetPlant(grid,0,grid.GetGeneration(),true), "changed selection accepted");
    Check(!R::SetPlant(grid,6,grid.GetGeneration()+1,false), "stale removal accepted");
    Plant(grid,4);
    Check(!R::IsProtected(grid,10) && !R::InRange(grid,4,5), "row wrapping");
    Plant(grid,8); Plant(grid,6,false);
    Check(R::IsProtected(grid,7) && !R::IsProtected(grid,11), "overlapping protection lost");
    Grid::Snapshot snapshot; grid.CaptureSnapshot(snapshot);
    const auto generation=grid.GetGeneration();
    Check(grid.RestoreSnapshot(snapshot) && !R::SetPlant(grid,6,generation,true), "stale snapshot request");
    for(int kind=0;kind<5;++kind) {
        Tile invalid=*grid.GetTile(8);
        if(kind==0) { invalid=Crop(); invalid.repellentPlant=true; }
        if(kind==1) invalid.feature=farm::FarmTileFeature::Canal;
        if(kind==2) invalid.growth=.1f;
        if(kind==3) invalid.pests={6,0};
        if(kind==4) invalid.state=farm::FarmTileState::ReadyToHarvest;
        Check(!grid.SetTile(8,invalid), "invalid occupancy accepted");
        snapshot.tiles[8]=invalid;
        Check(!grid.ReplaceSnapshot(snapshot), "invalid snapshot accepted");
    }
    Grid empty; Check(!R::CanPlace(nullptr) && R::Inspect(empty,false,true).selected<0, "empty grid");
    Check(grid.SelectTile(0), "selected soil");
    Tile soil; soil.state=farm::FarmTileState::Tilled; soil.heightLevel=2;
    soil.moisture=.35f; soil.soilNutrients=.4f; soil.irrigationEnabled=false;
    Check(grid.SetTile(0,soil), "soil fixture"); Plant(grid,0);
    FarmEconomySystem economy; economy.Initialize();
    FarmToolActionSystem tools; tools.Initialize();
    for(const auto tool : {FarmTool::Hoe,FarmTool::Water,FarmTool::Seed,FarmTool::Harvest})
        Check(!tools.ApplyToolDetailed(grid,tool,farm::CropType::Carrot,economy).Succeeded(), "tool overwrote plant");
    Check(!tools.CanToggleCanal(grid,0) && !tools.CanToggleWaterSource(grid,0) &&
        !tools.CanSetIrrigation(grid,0) && !FarmSoilSystem::CanCompost(*grid.GetTile(0)), "terrain/soil overlap");
    Check(FarmNextGoalSystem::InspectField(grid.GetTile(0),FarmWaterAdvice::Plant)==FarmWaterAdvice::Unknown,
        "crop advice on repellent");
    Plant(grid,0,false);
    const auto* restored=grid.GetTile(0);
    Check(restored->state==soil.state && restored->heightLevel==soil.heightLevel &&
        restored->moisture==soil.moisture && restored->soilNutrients==soil.soilNutrients &&
        !restored->irrigationEnabled && R::CanPlace(restored), "removal changed soil");
    auto e=economy.CaptureSnapshot(); e.lifetimeSales=1500; Check(economy.RestoreSnapshot(e), "rank");
    Check(FarmDevelopmentSystem::Expand(grid,economy,grid.GetGeneration(),4) && grid.GetTile(8)->repellentPlant,
        "expansion lost plants");
}
void PestsAndPreview() {
    for(int day : {6,16,26}) {
        Grid grid; Check(grid.Initialize(3,3), "pest grid"); Plant(grid,4);
        Check(grid.SetTile(1,Crop()) && grid.SetTile(2,Crop()), "pest fixture");
        Check(FarmPestSystem::AdvanceDays(grid,day-1,day,true).warningStarted &&
            FarmPestSystem::FindActive(grid)==2 && grid.GetTile(1)->pests.warningDay==0, "protected target selected");
        Plant(grid,5);
        Check(FarmPestSystem::AdvanceDays(grid,day,day+1,true).changed &&
            grid.GetTile(2)->pests.damageDays==1 && R::IsProtected(grid,2), "plant cured active pests");
        Check(FarmPestSystem::Cure(grid,2,day) && grid.GetTile(2)->pests.damageDays==1, "cure lost damage");
        auto view=R::Inspect(grid,false,true);
        Check(view.present && view.canRemove && !view.canPlace && view.cells[4].index==5,
            "selected preview");
        Check(!view.cells[1].protectedAfter && view.cells[1].protectedNow, "remove preview retained plant");
        Check(!R::Inspect(grid,true,true).canRemove, "blocked removal view");
    }
    Grid grid; Check(grid.Initialize(3,1), "all protected grid"); Plant(grid,1);
    Check(grid.SetTile(0,Crop()) && grid.SetTile(2,Crop()), "protected crops");
    Check(!FarmPestSystem::AdvanceDays(grid,5,6,true).changed, "all protected still spawns");
    Plant(grid,1,false);
    Check(!FarmPestSystem::AdvanceDays(grid,6,7,true).changed, "removed protection retriggered old day");
    Check(!FarmPestSystem::AdvanceDays(grid,15,16,false).changed &&
        FarmPestSystem::AdvanceDays(grid,15,16,true).warningStarted, "next warning/free mode");
}
void TerrainPreview() {
    Grid grid; Check(grid.Initialize(3,1), "preview grid"); Plant(grid,1);
    farm::FarmIrrigationPreviewSystem preview; preview.Initialize();
    using Op = farm::FarmIrrigationPreviewOperation;
    for(const auto op : {Op::ToggleCanal,Op::ToggleWaterSource,Op::PlaceCanalPath})
        Check(!preview.Begin(grid,1,op) && !preview.IsActive(), "water preview on plant");
    Check(preview.Begin(grid,0,Op::PlaceCanalPath) && !preview.VisitCanalPathTile(grid,2) &&
        grid.GetTile(1)->repellentPlant, "canal path crossed plant");
    preview.Cancel();
    Check(preview.Begin(grid,1,Op::RaiseTerrain) && preview.CanConfirm(grid), "plant height preview");
    Plant(grid,1,false);
    Check(!preview.CanConfirm(grid), "removed plant accepted by stale preview");
    Check(preview.Begin(grid,1,Op::RaiseTerrain), "empty height preview");
    Plant(grid,1);
    Check(!preview.CanConfirm(grid), "new plant accepted by stale preview");
}
void SaveAndMesh() {
    Grid grid; Check(grid.Initialize(5,4), "save grid");
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    const auto root=std::filesystem::path("generated/codex_checks") /
        ("repellent_save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    FarmDocumentSystem docs; Check(docs.Initialize(root.string(),grid,economy,selection), "docs init");
    Plant(grid,6);
    Check(docs.SaveAs("Repellent",grid,economy,selection), "save");
    const auto id=docs.GetActiveDocumentId(), path=docs.GetPath();
    nlohmann::json saved; Check(JsonFile::Load(path,saved) && saved["schemaVersion"]==25, "schema25");
    Plant(grid,6,false);
    Check(docs.Load(id,grid,economy,selection) && R::IsProtected(grid,7), "save roundtrip");
    for(int kind=0;kind<8;++kind) {
        auto bad=saved; auto& tile=bad["tiles"][6];
        if(kind==0) tile.erase("repellentPlant");
        if(kind==1) tile["repellentPlant"]=1;
        if(kind==2) tile["repellentPlant"]=nullptr;
        if(kind==3) tile["feature"]="Canal";
        if(kind==4) { tile["state"]="Planted"; tile["crop"]="Carrot"; }
        if(kind==5) tile["growth"]=.1;
        if(kind==6) tile["pests"]["warningDay"]=6;
        if(kind==7) bad["schemaVersion"]=26;
        const auto generation=grid.GetGeneration(), inventory=economy.GetInventoryGeneration();
        Check(JsonFile::Save(path,bad) && !docs.Load(id,grid,economy,selection) &&
            grid.GetGeneration()==generation && economy.GetInventoryGeneration()==inventory &&
            grid.GetTile(6)->repellentPlant, "invalid save mutated live farm");
    }
    auto legacy=saved; legacy["schemaVersion"]=22;
    for(auto& tile:legacy["tiles"]) tile.erase("repellentPlant");
    Check(JsonFile::Save(path,legacy) && docs.Load(id,grid,economy,selection) &&
        !grid.GetTile(6)->repellentPlant, "legacy22 migration");
    Check(JsonFile::Save(path,saved) && docs.Load(id,grid,economy,selection), "restore fixture");
    FarmLayoutSystem layouts; Check(layouts.Initialize((root/"layouts").string()) && layouts.SaveNew("Plant",grid), "layout save");
    const auto layoutId=layouts.Entries().front().id;
    Plant(grid,6,false);
    Check(layouts.Load(layoutId,grid,2) && grid.GetTile(6)->repellentPlant, "layout roundtrip");
    Check(docs.Reset(grid,economy,selection) && !grid.GetTile(6)->repellentPlant, "reset");
    Check(grid.Initialize(5,6), "mesh grid");
    farm::FarmVisualSystem visual; visual.Initialize({});
    std::size_t terrain=0, plants=0;
    for(int i=0;i<30;++i) {
        Tile tile; tile.heightLevel=(i+i/5)%3; tile.repellentPlant=true;
        Check(grid.SetTile(i,tile), "mesh fixture");
    }
    for(int i=0;i<30;++i) {
        const auto ground=farm::BuildFarmTileMeshParts(grid,i,visual);
        const auto mesh=farm::BuildFarmRepellentMeshParts(grid,i,visual);
        Check(mesh.count==8 && ground.count<=ground.parts.size(), "mesh capacity");
        terrain+=ground.count; plants+=mesh.count;
        const auto center=visual.GetTileCenter(grid,i);
        for(std::size_t j=0;j<mesh.count;++j) {
            const auto& p=mesh.parts[j];
            Check(std::isfinite(p.position.y) && p.scale.x>0 && p.scale.y>0 && p.scale.z>0 &&
                p.position.y-p.scale.y>=center.y-1e-5f && std::abs(p.position.x-center.x)+p.scale.x<.6f,
                "plant not anchored/finite/inside tile");
        }
    }
    Check(terrain<=960 && plants==240, "renderer budgets");
    Check(farm::BuildFarmRepellentMeshParts(grid,-1,visual).count==0, "invalid render index");
}
}
int main() try {
    RangeAndActions(); PestsAndPreview(); TerrainPreview(); SaveAndMesh();
    std::cout << "farm_repellent_test: PASS\n"; return 0;
} catch(const std::exception& e) { std::cerr << "farm_repellent_test: " << e.what() << '\n'; return 1; }
