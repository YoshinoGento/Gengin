#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmNextGoalSystem.h"
#include "farm/system/FarmTimeAdvanceSystem.h"
#include "farm/ui/FarmPestView.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Grid = farm::FarmGrid;
using Tile = farm::FarmTile;
using Spread = farm::FarmPestSpreadState;
using Pest = FarmPestSystem;
void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
Tile Crop() { Tile tile; tile.state=farm::FarmTileState::Planted; tile.crop=farm::CropType::Carrot; tile.moisture=.6f; return tile; }
void Fill(Grid& grid, int width=5, int height=4) {
    Check(grid.Initialize(width,height), "grid");
    for(int i=0;i<grid.GetTileCount();++i) Check(grid.SetTile(i,Crop()), "crop");
}
void SamePests(const Grid& a, const Grid& b) {
    Check(a.GetTileCount()==b.GetTileCount(), "grid count");
    for(int i=0;i<a.GetTileCount();++i) Check(a.GetTile(i)->pests==b.GetTile(i)->pests, "pest parity");
}
void SpreadRules() {
    Grid grid; Fill(grid);
    Spread spread{1,1};
    Check(Pest::AdvanceDays(grid,1,6,true,spread).warningStarted && Pest::CountActive(grid)==1, "initial warning");
    Check(!Pest::AdvanceDays(grid,6,7,true,spread).warningStarted && Pest::CountActive(grid)==1 &&
        grid.GetTile(0)->pests.damageDays==1, "grace period");
    Check(Pest::AdvanceDays(grid,7,8,true,spread).warningStarted && Pest::CountActive(grid)==2 &&
        grid.GetTile(1)->pests==farm::FarmPestState{8,0} && grid.GetTile(0)->pests.damageDays==2, "one new neighbor, no same-day damage");
    Check(Pest::AdvanceDays(grid,8,9,true,spread).warningStarted && Pest::CountActive(grid)==3 &&
        grid.GetTile(5)->pests==farm::FarmPestState{9,0}, "daily limit/deterministic direction");
    Check(Pest::AdvanceDays(grid,9,10,true,spread).changed && Pest::CountActive(grid)==3 &&
        grid.GetTile(0)->pests.damageDays==3 && grid.GetTile(1)->pests.damageDays==2 &&
        grid.GetTile(5)->pests.damageDays==1, "cap/all exposures");
    Check(!Pest::Cure(grid,-1,8) && !Pest::Cure(grid,1,6), "invalid cure");
    Check(Pest::Cure(grid,1,8) && grid.GetTile(1)->pests.damageDays==2 && Pest::CountActive(grid)==2, "individual cure retains damage");
    Check(!Pest::AdvanceDays(grid,9,10,true,spread).changed && Pest::CountActive(grid)==2, "duplicate boundary after cure");
    Check(!Pest::AdvanceDays(grid,10,9,true,spread).changed && spread.lastProcessedDay==10, "backward clock");
    Check(Pest::AdvanceDays(grid,10,11,true,spread).warningStarted && grid.GetTile(1)->pests.warningDay==0, "damaged crop reinfested");
    Pest::DismissActive(grid);
    Check(Pest::CountActive(grid)==0 && grid.GetTile(0)->pests.damageDays==3, "disable clears all, retains damage");
    Check(!Pest::AdvanceDays(grid,11,16,false,spread).changed && spread.lastProcessedDay==11, "disabled mutated");
    for(int day=2;day<=35;++day) {
        Grid daily,skip; Fill(daily); Fill(skip);
        Spread a{1,1},b{1,1};
        for(int d=2;d<=day;++d) static_cast<void>(Pest::AdvanceDays(daily,d-1,d,true,a));
        static_cast<void>(Pest::AdvanceDays(skip,1,day,true,b));
        SamePests(daily,skip); Check(a==b, "skip-day clock parity");
    }
    Grid large; Fill(large); Spread clock{1,1};
    Check(Pest::AdvanceDays(large,1,(std::numeric_limits<int>::max)(),true,clock).changed &&
        Pest::CountActive(large)==3, "large jump bounded");
    Check(!Pest::AdvanceDays(large,1,(std::numeric_limits<int>::max)(),true,clock).changed, "max-day duplicate");
    Grid empty; Spread invalid{2,1};
    Check(!Pest::AdvanceDays(empty,1,6,true,invalid).changed, "invalid version");
    Spread valid{1,1}; Check(Pest::AdvanceDays(empty,1,6,true,valid).changed && Pest::CountActive(empty)==0, "empty grid");
}
void Targets() {
    // Only the source and the candidate are planted: no second scheduled source can mask adjacency errors.
    for(int target : {0,1,2,3,5,6,7,8}) {
        Grid grid; Check(grid.Initialize(3,3), "adjacent grid");
        auto source=Crop(); source.pests={6,0};
        Check(grid.SetTile(4,source) && grid.SetTile(target,Crop()), "adjacent fixtures");
        Spread state{1,6}; static_cast<void>(Pest::AdvanceDays(grid,6,8,true,state));
        Check((grid.GetTile(target)->pests.warningDay==8)==(target==1||target==3||target==5||target==7), "diagonal spread");
    }
    Grid edge; Check(edge.Initialize(3,2), "edge grid");
    auto source=Crop(); source.pests={6,0};
    Check(edge.SetTile(2,source) && edge.SetTile(3,Crop()), "edge fixtures");
    Spread clock{1,6}; static_cast<void>(Pest::AdvanceDays(edge,6,8,true,clock));
    Check(Pest::CountActive(edge)==1, "row wrapping");
    for(int kind=0;kind<5;++kind) {
        Grid grid; Check(grid.Initialize(3,1) && grid.SetTile(0,source), "protected grid");
        auto target=Crop();
        if(kind==0) target.growth=1;
        if(kind==1) { target={}; target.feature=farm::FarmTileFeature::Canal; }
        if(kind==2) target.pests.damageDays=1;
        if(kind==3) target={};
        Check(grid.SetTile(1,target), "target fixture");
        if(kind==4) {
            Check(grid.SelectTile(2) && FarmRepellentSystem::SetPlant(grid,2,grid.GetGeneration(),true), "protection");
        }
        Spread s{1,6}; static_cast<void>(Pest::AdvanceDays(grid,6,8,true,s));
        Check(Pest::CountActive(grid)==1, "ineligible/protected target infested");
        if(kind==4) {
            Check(FarmRepellentSystem::SetPlant(grid,2,grid.GetGeneration(),false), "remove protection");
            Check(!Pest::AdvanceDays(grid,7,8,true,s).changed, "retroactive spread after removal");
            Check(Pest::AdvanceDays(grid,8,9,true,s).warningStarted, "next-day spread after removal");
        }
    }
    Grid late; Check(late.Initialize(2,1), "late grid");
    auto crop=Crop(); crop.pests={29,0}; Check(late.SetTile(0,crop) && late.SetTile(1,Crop()), "late fixture");
    Spread state{1,29}; static_cast<void>(Pest::AdvanceDays(late,29,35,true,state));
    Check(Pest::CountActive(late)==1 && late.GetTile(0)->pests.damageDays==3, "season-end spread/damage");
}
void Persistence() {
    Grid grid; Fill(grid);
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason,true,true,true);
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    const auto root=std::filesystem::path("generated/codex_checks") /
        ("pest_spread_save_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    FarmDocumentSystem docs;
    Check(docs.Initialize(root.string(),grid,economy,selection,&date,&progression), "document init");
    Check(progression.AdvancePests(grid,1,9).changed && date.RestoreSnapshot({9,0,1}) &&
        docs.SaveAs("Spread",grid,economy,selection), "spread save");
    const auto id=docs.GetActiveDocumentId(),path=docs.GetPath();
    nlohmann::json saved; Check(JsonFile::Load(path,saved) && saved["schemaVersion"]==25, "schema25");
    Pest::DismissActive(grid);
    Check(docs.Load(id,grid,economy,selection) && Pest::CountActive(grid)==3 &&
        progression.PestSpread()==Spread{1,9}, "multiple warning roundtrip");
    Check(Pest::Cure(grid,5,9) && docs.Save(grid,economy,selection) && docs.Load(id,grid,economy,selection) &&
        !progression.AdvancePests(grid,8,9).changed && Pest::CountActive(grid)==2, "reload repeated same-day spread");
    for(int kind=0;kind<15;++kind) {
        auto bad=saved;
        if(kind==0) bad.erase("pestSpread");
        if(kind==1) bad["pestSpread"]=nullptr;
        if(kind==2) bad["pestSpread"]["version"]=2;
        if(kind==3) bad["pestSpread"]["version"]=1.0;
        if(kind==4) bad["pestSpread"]["lastProcessedDay"]=8;
        if(kind==5) bad["pestSpread"]["lastProcessedDay"]=10;
        if(kind==6) bad["pestSpread"]["lastProcessedDay"]=4294967296LL;
        if(kind==7) bad["pestsEnabled"]=false;
        if(kind==8) bad["pestSpread"]["version"]=0;
        if(kind==9) bad["tiles"][7]["pests"]={{"warningDay",9},{"damageDays",0}};
        if(kind==10) bad["tiles"][5]["pests"]["warningDay"]=31;
        if(kind==11) bad["tiles"][5]["pests"]["damageDays"]=1;
        if(kind==12) bad["pestSpread"].erase("lastProcessedDay");
        if(kind==13) bad["pestSpread"]["lastProcessedDay"]=-1;
        if(kind==14) bad["schemaVersion"]=26;
        const auto generation=grid.GetGeneration(), inventory=economy.GetInventoryGeneration();
        const auto previous=progression.CaptureSnapshot();
        Check(JsonFile::Save(path,bad) && !docs.Load(id,grid,economy,selection) &&
            grid.GetGeneration()==generation && economy.GetInventoryGeneration()==inventory && date.GetDay()==9 &&
            progression.PestSpread()==previous.pestSpread && Pest::CountActive(grid)==2, "invalid save changed state");
    }
    auto legacy=saved; legacy["schemaVersion"]=23; legacy.erase("pestSpread");
    for(auto& tile:legacy["tiles"]) tile["pests"]={{"warningDay",0},{"damageDays",0}};
    legacy["tiles"][0]["pests"]={{"warningDay",6},{"damageDays",3}};
    Check(JsonFile::Save(path,legacy) && docs.Load(id,grid,economy,selection) &&
        progression.PestSpread()==Spread{}, "legacy23 policy migration");
    static_cast<void>(progression.AdvancePests(grid,9,16));
    Check(Pest::CountActive(grid)==1, "legacy spread enabled");
    legacy["tiles"][0]["pests"]={{"warningDay",8},{"damageDays",1}};
    Check(JsonFile::Save(path,legacy) && !docs.Load(id,grid,economy,selection), "legacy non-scheduled date accepted");
    Check(JsonFile::Save(path,saved) && docs.Load(id,grid,economy,selection), "restore new save");
    const auto snapshot=progression.CaptureSnapshot();
    Check(progression.AdvancePests(grid,9,10).changed && progression.RestoreSnapshot(snapshot) &&
        progression.PestSpread()==Spread{1,9}, "snapshot omitted spread clock");
    for(int kind=0;kind<4;++kind) {
        auto bad=snapshot;
        if(kind==0) bad.pestSpread.version=2;
        if(kind==1) bad.pestSpread.lastProcessedDay=0;
        if(kind==2) bad.pestsEnabled=false;
        if(kind==3) bad.pestSpread.version=0;
        Check(!progression.RestoreSnapshot(bad) && progression.PestSpread()==snapshot.pestSpread, "invalid snapshot mutation");
    }
    Check(docs.Reset(grid,economy,selection) && progression.PestSpread()==Spread{1,1} && date.GetDay()==1, "new contest reset policy");
    Check(progression.SetMode(FarmProgressionMode::FreeFarming,0,
        FarmContestSeasonSystem::Evaluate(date.GetDay(),economy.GetContestResults())) &&
        progression.PestSpread()==Spread{}, "free policy");
}
struct Simulation {
    Grid grid; farm::FarmIrrigationSystem irrigation; FarmGrowthSystem growth; FarmDateSystem date;
    FarmContestDaySystem contest; FarmEconomySystem economy; FarmProgressionSystem progression;
    Simulation() {
        Fill(grid); irrigation.Initialize(); growth.Initialize(); date.Initialize(); economy.Initialize();
        progression.Initialize({},FarmProgressionMode::ContestSeason,true,false,true);
    }
    FarmSimulationStepSystem::Context Context() { return {grid,irrigation,growth,date,contest,economy,progression}; }
};
void IntegrationAndView() {
    for(float speed : {1.f,2.f,4.f}) {
        Simulation ordinary,waiting;
        for(auto* s : {&ordinary,&waiting}) Check(s->date.RestoreSnapshot({5,59.99f,speed}), "simulation clock");
        FarmTimeAdvanceSystem advance;
        Check(advance.Start(waiting.Context(),false), "time advance start");
        const int steps=advance.AdvanceBatch(waiting.Context(),false);
        Check(steps==1 && advance.GetStatus().reason==FarmTimeAdvanceSystem::StopReason::PestWarning, "stop at warning");
        Check(FarmSimulationStepSystem::Step(ordinary.Context(),FarmTimeAdvanceSystem::kStepSeconds).pestWarning, "normal warning");
        SamePests(ordinary.grid,waiting.grid);
        Check(ordinary.progression.PestSpread()==waiting.progression.PestSpread(), "transport clock parity");
        Check(ordinary.date.RestoreSnapshot({7,59.99f,speed}), "spread clock fixture");
        const auto result=FarmSimulationStepSystem::Step(ordinary.Context(),FarmTimeAdvanceSystem::kStepSeconds);
        Check(result.pestWarning && Pest::CountActive(ordinary.grid)==2 && ordinary.grid.GetTile(1)->pests.damageDays==0, "normal spread boundary");
        Check(!advance.Start(ordinary.Context(),false), "waiting ignored active infestations");
    }
    Simulation paused; Check(paused.date.RestoreSnapshot({7,59.99f,1}), "pause fixture");
    FarmTimeAdvanceSystem blocked;
    Check(!blocked.Start(paused.Context(),true) && blocked.AdvanceBatch(paused.Context(),true)==0 &&
        !FarmSimulationStepSystem::Step(paused.Context(),0).advanced &&
        paused.progression.PestSpread()==Spread{1,1}, "blocked/zero-delta spread");
    Simulation policy; FarmTimeAdvanceSystem waiting;
    Check(waiting.Start(policy.Context(),false), "policy wait");
    auto snapshot=policy.progression.CaptureSnapshot(); snapshot.pestSpread={};
    Check(policy.progression.RestoreSnapshot(snapshot) && waiting.AdvanceBatch(policy.Context(),false)==0 &&
        waiting.GetStatus().reason==FarmTimeAdvanceSystem::StopReason::ContextChanged, "spread policy invalidation");
    Grid grid; Fill(grid); Spread state{1,1}; static_cast<void>(Pest::AdvanceDays(grid,1,9,true,state));
    FarmEconomySystem economy; economy.Initialize(); FarmNextGoalContext context; context.pestsEnabled=true;
    Check(grid.SelectTile(5), "select infested tile");
    Check(FarmNextGoalSystem::Inspect(grid,economy,context).pestTile==5, "selected target ignored");
    Check(Pest::Cure(grid,5,9) && FarmNextGoalSystem::Inspect(grid,economy,context).pestTile==0, "next target not offered");
    for(int count=1;count<=3;++count) for(bool selected : {false,true}) for(bool warning : {false,true}) {
        farmui::View view; farmui::BuildPestView(view,5,selected,warning,count,true);
        const auto hit=view.Hit({600,40});
        Check(hit.argument==5 && hit.action==(selected?farmui::Action::PestCure:farmui::Action::PestFocus), "view action");
        for(std::size_t i=0;i<view.count;++i) {
            const auto& item=view.items[i]; const auto& glyph=farmui::kLabels[static_cast<std::size_t>(item.label)];
            Check(item.rect.x>=0 && item.rect.y>=0 && item.rect.x+item.rect.width<=1280 && item.rect.y+item.rect.height<=720, "view bounds");
            Check(item.rect.width-20>=glyph.width*20.f/26.f, "label too small");
        }
    }
}
}
int main() try {
    SpreadRules(); Targets(); Persistence(); IntegrationAndView();
    std::cout << "farm_pest_spread_test: PASS\n"; return 0;
} catch(const std::exception& e) { std::cerr << "farm_pest_spread_test: " << e.what() << '\n'; return 1; }
