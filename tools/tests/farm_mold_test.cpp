#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmMoldSystem.h"
#include "farm/ui/FarmFieldActionView.h"
#include "farm/system/FarmSeedShopSystem.h"
#include "farm/render/FarmMoldGeometry.h"
#include "3d/LineDrawer.h"
#include "farm/system/FarmTimeAdvanceSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include "farm/ui/FarmTerrainView.h"
#include "io/JsonFile.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>

void LineDrawer::DrawLine(const Vector3&,const Vector3&,const Vector4&) {}
void LineDrawer::DrawWireCube(const Vector3&,float,const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3&,float,const Vector4&,uint32_t) {}
namespace {
using M = farm::MoldPhase;
using State = farm::FarmMoldState;
using Reason = FarmTimeAdvanceSystem::StopReason;
void Check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }

void FieldEffects() {
    namespace fx = farm::mold;
    using FX = farm::FarmMoldVisualSystem;
    farm::FarmGrid grid; Check(grid.Initialize(5,6),"expanded visual farm");
    farm::FarmVisualSystem visual; visual.Initialize({});
    FX effects;
    farm::FarmTile crop; crop.state=farm::FarmTileState::Planted; crop.crop=farm::CropType::Pumpkin;
    crop.heightLevel=2; crop.mold={8,.85f,false,true};
    for(int i=0;i<grid.GetTileCount();++i) Check(grid.SetTile(i,crop),"warning recovery");
    Check(!effects.Frame(grid,visual).count,"no stain for recovery without disease");
    const auto before=FX::Capture(grid);
    crop.mold={State::kOnsetSeconds,.85f,true,false};
    for(int i=0;i<grid.GetTileCount();++i) Check(grid.SetTile(i,crop),"all sick");
    effects.AfterGrowth(grid,before);
    effects.Update(.38f); effects.AfterGrowth(grid,FX::Capture(grid));
    auto frame=effects.Frame(grid,visual);
    Check(frame.count==30 && frame.beds[0].puff>.3f,"all sick together without repeated emission");
    std::vector<fx::Vertex> vertices(fx::kVertexCount+1);
    const auto build=[&] { return fx::Build(vertices,frame,{1,0,0},{0,1,0}); };
    Check(build()==fx::kVertexCount,"simultaneous burst capacity");
    Check(fx::Build(std::span(vertices).first(8),frame,{1,0,0},{0,1,0})==0,"tiny buffer");
    for(std::size_t i=0;i<fx::kVertexCount;++i) {
        const auto& v=vertices[i]; const auto& b=frame.beds[i/(fx::kVertexCount/30)];
        Check(farm::rain::Finite(v.position) && std::abs(v.position.x-b.root.x)<b.unit &&
            std::abs(v.position.z-b.root.z)<b.unit && v.position.y>=b.root.y &&
            v.position.y<b.root.y+b.unit && v.color.w>=0 && v.color.w<=1,"bed-local geometry");
    }
    effects.Update(FX::kBurstSeconds); frame=effects.Frame(grid,visual);
    const auto stains=build(); const float opacity=vertices[0].color.w;
    Check(stains==30*fx::kMat.size()*fx::kFanVertices && opacity>.8f,"stains outlast puffs");
    crop.mold={State::kOnsetSeconds*.5f,.6f,true,true};
    Check(grid.SetTile(0,crop),"half recovered"); frame=effects.Frame(grid,visual);
    Check(build()==stains && std::abs(vertices[0].color.w-opacity*.5f)<.0001f,"recovery fades stain");
    crop.mold={}; Check(grid.SetTile(0,crop) && effects.Frame(grid,visual).count==29,"full recovery");
    const auto well=FX::Capture(grid); crop.mold={State::kOnsetSeconds,.85f,true,false};
    Check(grid.SetTile(0,crop),"fresh disease"); effects.AfterGrowth(grid,well);
    Check(effects.Frame(grid,visual).beds[0].puff==0,"new disease after full recovery must emit");
    farm::FarmGrid::Snapshot saved; grid.CaptureSnapshot(saved);
    Check(grid.RestoreSnapshot(saved) && effects.Frame(grid,visual).beds[0].puff<0,"restore must not replay active puff");
}

void Timing(float speed) {
    const float dt = speed / 60.f;
    State s;
    float warning = 0, onset = 0;
    for (int i = 1; i * dt <= 28; ++i) {
        s = FarmMoldSystem::Next(s, true, true, .85f, dt);
        if (!warning && s.Phase() == M::Warning) warning = i * dt;
        if (!onset && s.sick) onset = i * dt;
    }
    Check(std::abs(warning - State::kWarningSeconds) <= dt + .001f &&
        std::abs(onset - State::kOnsetSeconds) <= dt + .001f, "warning/onset timing");
    Check((onset-warning)/speed >= 4.9f, "insufficient reaction time");
    const State critical{State::kOnsetSeconds - .001f, .85f, false, false};
    auto rescued = FarmMoldSystem::Next(critical, true, false, .84f, dt);
    Check(rescued.Phase() == M::Recovering && !rescued.sick && rescued.exposure == critical.exposure,
        "last-moment closure must protect a drying bed immediately");
    for (int i = 0; i * dt <= FarmMoldSystem::kRecoverySeconds + dt; ++i) {
        s = FarmMoldSystem::Next(s, false, false, .60f, dt);
        rescued = FarmMoldSystem::Next(rescued, false, false, .60f, dt);
    }
    Check(s == State{} && rescued == State{}, "recovery should clear all crop history");
    for (int n = 0; n < 12; ++n) {
        for (int i = 0; i * dt < 2; ++i) s = FarmMoldSystem::Next(s, true, true, .8f, dt);
        s = FarmMoldSystem::Next(s, false, true, .60f, dt);
        Check(s == State{}, "short wet spells must not accumulate");
    }
}

void PersistenceAndPlay() {
    namespace fs = std::filesystem;
    const auto root = fs::path("generated/codex_checks") / ("mold_save_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    farm::FarmGrid grid; Check(grid.Initialize(5,4), "grid");
    FarmDateSystem date; date.Initialize();
    FarmEconomySystem economy; economy.Initialize();
    FarmProgressionSystem progress; progress.Initialize({}, FarmProgressionMode::ContestSeason, false, true);
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDocumentSystem docs;
    Check(docs.Initialize(root.string(),grid,economy,selection,&date,&progress) &&
        docs.LoadPrepared("project/Resources/farm/experience/rain.json",grid,economy,selection) &&
        docs.SaveAs("Mold comparison",grid,economy,selection), "prepare independent farm");
    const auto path = docs.GetPath(), id = docs.GetActiveDocumentId();
    farm::FarmIrrigationSystem canal; canal.Initialize();
    FarmGrowthSystem growth; growth.Initialize();
    FarmContestDaySystem contest;
    farm::FarmMoldVisualSystem effects;
    farm::FarmVisualSystem visual; visual.Initialize({});
    FarmSimulationStepSystem::Context ctx{grid,canal,growth,date,contest,economy,progress,nullptr,&effects};
    FarmToolActionSystem tools; tools.Initialize();
    FarmTimeAdvanceSystem wait;
    Check(!wait.Start(ctx,false) && wait.GetStatus().reason == Reason::MoldAlert &&
        wait.GetStatus().tileIndex == 6, "time advance must identify the affected bed");
    auto almost = *grid.GetTile(6);
    // The current sample is still in range; rain + canal cross the threshold during the first fast step.
    almost.moisture = .65f; almost.mold = {State::kWarningSeconds-.001f,.65f,false,false};
    date.SetTimeScale(4);
    Check(grid.SetTile(6,almost) && grid.SetTile(8,{}) &&
        wait.Start(ctx,false) && wait.AdvanceBatch(ctx,false)==1 &&
        wait.GetStatus().reason==Reason::MoldAlert && grid.GetTile(6)->mold.Phase()==M::Warning,
        "4x advance must stop on the first warning step, without running the remaining batch");
    Check(docs.Load(id,grid,economy,selection),"restore original pair");
    Check(tools.SetSelectedIrrigation(grid,false), "no-cost intake closure");
    for(int i=0;i<21*60;++i) {
        effects.Update(1.f/60);
        Check(FarmSimulationStepSystem::Step(ctx,1.f/60).advanced,"normal step");
    }
    const auto pair=effects.Frame(grid,visual);
    Check(pair.count==1 && pair.beds[0].index==8 && pair.beds[0].stain==1 &&
        pair.beds[0].puff>.70f && pair.beds[0].puff<1,"single real onset on open intake bed");
    Check(grid.GetTile(6)->mold == State{} && grid.GetTile(8)->mold.sick, "normal rain pair");
    const auto sick = *grid.GetTile(8);
    auto well = sick; well.mold = {};
    const auto healthyRate = growth.Evaluate(well).growthPerSecond;
    Check(healthyRate > 0 && std::abs(growth.Evaluate(sick).growthPerSecond -
        healthyRate*FarmMoldSystem::kSickGrowthMultiplier) < .000001f, "growth-only slowdown");
    for (const State state : {State{7,.82f,false,false}, sick.mold, State{4,.61f,true,true}}) {
        auto tile = sick; tile.mold = state;
        Check(grid.SetTile(8,tile) && docs.Save(grid,economy,selection) && grid.SetTile(8,well) &&
            docs.Load(id,grid,economy,selection) && grid.GetTile(8)->mold == state, "exact phase roundtrip");
        const auto loaded=effects.Frame(grid,visual);
        Check(state.sick ? (loaded.count==1 && loaded.beds[0].puff<0) : loaded.count==0,"load stain only");
        farmui::View hud;
        const auto water = FarmGrowthSystem::AnalyzeWater(&tile,growth.Evaluate(tile),farm::FarmWaterStatus::Available);
        farmui::BuildWaterGuidanceView(hud,water,true,true);
        const auto label = hud.items[1].label;
        Check(label == (state.recovering ? farmui::Label::MoldRecovery : state.sick ? farmui::Label::MoldSick :
            farmui::Label::MoldWarning) && farmui::kLabels.at(static_cast<size_t>(label)).width <= hud.items[1].rect.width - 20,
            "mold guidance must fit existing panel");
    }
    nlohmann::json saved; Check(JsonFile::Load(path,saved),"read snapshot");
    const State before = grid.GetTile(8)->mold;
    for(int kind=0;kind<4;++kind) {
        auto bad=saved;
        if(kind==0) bad["tiles"][8].erase("mold");
        if(kind==1) bad["tiles"][8]["mold"]["exposure"]=-1;
        if(kind==2) bad["tiles"][8]["mold"]["sick"]=1;
        if(kind==3) bad["tiles"][0]["mold"]=bad["tiles"][8]["mold"];
        Check(JsonFile::Save(path,bad) && !docs.Load(id,grid,economy,selection) &&
            grid.GetTile(8)->mold == before, "invalid save must not replace current farm");
    }
    auto legacy=saved; legacy["schemaVersion"]=25;
    for(auto& tile : legacy["tiles"]) tile.erase("mold");
    Check(JsonFile::Save(path,legacy) && docs.Load(id,grid,economy,selection) && grid.GetTile(8)->mold==State{},
        "legacy crops must not inherit disease from care history");
    auto ripe=sick; ripe.growth=1;
    Check(grid.SetTile(8,ripe) && grid.SelectTile(8) && tools.ApplyTool(grid,FarmTool::Harvest,ripe.crop,economy) &&
        grid.GetTile(8)->mold == State{} && !effects.Frame(grid,visual).count, "harvest clears stain");
}

void Prevention() {
    using S=FarmFungicideSystem;
    const auto root=std::filesystem::path("generated/codex_checks") / ("prevention_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    farm::FarmGrid grid; Check(grid.Initialize(5,4),"prevention farm");
    FarmDateSystem date; date.Initialize();
    FarmEconomySystem eco; eco.Initialize(); FarmCropSelectionSystem seeds; seeds.Initialize();
    FarmProgressionSystem progression; progression.Initialize({},FarmProgressionMode::ContestSeason,false,true);
    FarmDocumentSystem docs;
    Check(docs.Initialize(root.string(),grid,eco,seeds,&date,&progression) &&
        docs.LoadPrepared("project/Resources/farm/experience/rain.json",grid,eco,seeds) &&
        docs.SaveAs("防カビ剤の比較",grid,eco,seeds),"clean before-purchase fixture");
    const auto startingSave=docs.GetPath();
    const int i=6, neighbor=8;
    const auto startingMoney=eco.GetMoney(); const auto before=*grid.GetTile(i);
    Check(!S::Apply(grid,i,eco) && !S::Apply(grid,-1,eco),"reject no stock or invalid bed");
    FarmSeedShopSystem shop; shop.Open(before.crop);
    Check(shop.Select(shop.kFungicideProduct) && shop.ChangeQuantity(1) && shop.BeginConfirmation(eco) &&
        shop.Confirm(eco).Succeeded() && eco.GetMoney()==startingMoney-2*eco.kFungicidePrice,"purchase through shop");
    const auto target=[&] {
        farmui::View v; auto t=grid.GetTile(i);
        farmui::BuildFungicideView(v,S::Inspect(t,eco),eco.GetFungicideCount(),t->mold.protectedSeconds,i,true);
        return v.Hit({60,425});
    };
    auto hit=target(); Check(hit.action==farmui::Action::ApplyFungicide && hit.argument==i,"HUD target");
    Check(S::Apply(grid,i,eco) && eco.GetFungicideCount()==1 && !S::Apply(grid,i,eco) &&
        target().action==farmui::Action::None && eco.GetFungicideCount()==1,"apply exactly once");
    const auto protectedTile=*grid.GetTile(i);
    Check(protectedTile.moisture==before.moisture && protectedTile.growth==before.growth &&
        protectedTile.mold.protectedSeconds==State::kProtectionSeconds && grid.GetTile(neighbor)->mold.protectedSeconds==0,
        "only chosen plant protected; not watering or advancing growth");
    FarmGrowthSystem growth; growth.Initialize(); farm::FarmIrrigationSystem canal; canal.Initialize();
    FarmContestDaySystem contest;
    FarmSimulationStepSystem::Context ctx{grid,canal,growth,date,contest,eco,progression};
    for(int j=0;j<60;++j) Check(FarmSimulationStepSystem::Step(ctx,1.f/60).advanced,"normal simulation");
    const auto remaining=grid.GetTile(i)->mold.protectedSeconds;
    Check(std::abs(remaining-19)<.002f && !grid.GetTile(i)->mold.sick,"game clock countdown");
    Check(!growth.Update(grid,.1f,0) && grid.GetTile(i)->mold.protectedSeconds==remaining,"pause freezes effect");
    Check(docs.SaveAs("確認中",grid,eco,seeds) && docs.Load(docs.GetActiveDocumentId(),grid,eco,seeds) &&
        grid.GetTile(i)->mold.protectedSeconds==remaining && eco.GetFungicideCount()==1,"timer and inventory exact on reload");
    const auto path=docs.GetPath(),id=docs.GetActiveDocumentId();
    nlohmann::json saved; Check(JsonFile::Load(path,saved),"read protection save");
    for(int k=0;k<4;++k) {
        auto bad=saved;
        if(k==0) bad["economy"]["fungicideCount"]=-1;
        if(k==1) bad["tiles"][i]["mold"]["protectedSeconds"]=21;
        if(k==2) bad["tiles"][i]["mold"]["sick"]=true;
        if(k==3) bad["economy"].erase("fungicideCount");
        Check(JsonFile::Save(path,bad) && !docs.Load(id,grid,eco,seeds) && eco.GetFungicideCount()==1 &&
            grid.GetTile(i)->mold.protectedSeconds==remaining,"invalid saves rejected atomically");
    }
    for(int j=0;j<20*60;++j) Check(FarmSimulationStepSystem::Step(ctx,1.f/60).advanced,"comparison step");
    Check(!grid.GetTile(i)->mold.sick && grid.GetTile(i)->mold.protectedSeconds==0 &&
        grid.GetTile(neighbor)->mold.sick && !S::Apply(grid,neighbor,eco) && eco.GetFungicideCount()==1,
        "untreated neighbor sick; fungicide cannot cure");
    auto recovering=*grid.GetTile(neighbor); recovering.mold.recovering=true;
    Check(grid.SetTile(neighbor,recovering) && !S::Apply(grid,neighbor,eco),"no treatment during sick recovery");
    auto ripe=before; ripe.growth=1;
    Check(grid.SetTile(i,ripe) && !S::Apply(grid,i,eco) && !S::Apply(grid,0,eco) && eco.GetFungicideCount()==1,
        "no consumption on mature or empty");
    auto legacy=saved; legacy["schemaVersion"]=26; legacy["economy"].erase("fungicideCount");
    for(auto& t:legacy["tiles"]) t["mold"].erase("protectedSeconds");
    Check(JsonFile::Save(path,legacy) && docs.Load(id,grid,eco,seeds) &&
        eco.GetFungicideCount()==0 && grid.GetTile(i)->mold.protectedSeconds==0,"schema26 defaults");
    auto edge=protectedTile.mold; edge.protectedSeconds=.125f;
    edge=FarmMoldSystem::Next(edge,true,true,.85f,.25f);
    Check(edge.protectedSeconds==0 && edge.exposure==.125f,"only elapsed uncovered fraction counts");
    for(int j=0;j<104;++j) edge=FarmMoldSystem::Next(edge,true,true,.85f,.25f);
    Check(edge.sick,"recurring excess can cause disease after expiry");
    std::cout<<"PASS fungicide: shop, selected bed, timer, expiry, no cure, pause, save and legacy. Initial file: "<<startingSave<<'\n';
}

void VerifyViewingFarm() {
    const auto dir=std::filesystem::path("generated/codex_checks") / ("mold_viewing_farm_" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    farm::FarmGrid grid; Check(grid.Initialize(5,4),"viewing grid");
    FarmDateSystem date; date.Initialize();
    FarmProgressionSystem progress; progress.Initialize({},FarmProgressionMode::ContestSeason,false,true);
    FarmEconomySystem economy; economy.Initialize();
    FarmCropSelectionSystem selection; selection.Initialize();
    FarmDocumentSystem docs;
    Check(docs.Initialize(dir.string(),grid,economy,selection,&date,&progress) &&
        docs.LoadPrepared("project/Resources/farm/experience/rain.json",grid,economy,selection),"viewing baseline");
    FarmToolActionSystem tools; tools.Initialize();
    Check(grid.SelectTile(6) && tools.SetSelectedIrrigation(grid,false) && grid.SelectTile(8),"viewing intakes");
    auto tomato=*grid.GetTile(8);
    tomato.growth=.015f; tomato.mold.exposure=State::kOnsetSeconds-5.f;
    Check(grid.SetTile(8,tomato) && docs.SaveAs("水カビの発病と回復",grid,economy,selection) &&
        docs.Load(docs.GetActiveDocumentId(),grid,economy,selection),"save viewing farm before onset");
    farm::FarmIrrigationSystem canal; canal.Initialize();
    FarmGrowthSystem growth; growth.Initialize();
    FarmContestDaySystem contest;
    farm::FarmMoldVisualSystem fx; farm::FarmVisualSystem visual; visual.Initialize({});
    FarmSimulationStepSystem::Context ctx{grid,canal,growth,date,contest,economy,progress,nullptr,&fx};
    bool burst=false, faded=false, cleared=false;
    float last=1;
    for(int i=0;i<60*40;++i) {
        if (i==60*9) Check(tools.SetSelectedIrrigation(grid,false),"close exhibited tomato");
        fx.Update(1.f/60); Check(FarmSimulationStepSystem::Step(ctx,1.f/60).advanced,"viewing step");
        const auto frame=fx.Frame(grid,visual);
        if (!frame.count) { if (burst) { cleared=true; break; } continue; }
        Check(frame.count==1 && frame.beds[0].index==8,"only open bed has mold");
        burst |= frame.beds[0].puff>0;
        const float opacity=frame.beds[0].stain;
        Check(opacity<=last,"viewing stain must not brighten");
        faded |= opacity>0 && opacity<.8f;
        last=opacity;
    }
    Check(burst && faded && cleared && !farm::IsHarvestReady(*grid.GetTile(8)),"exhibition: onset to full recovery before maturity");
    std::cout<<"Verified exhibition starting save: "<<dir.string()<<'\n';
}
}
int main() try {
    for (float speed : {1.f,2.f,4.f}) Timing(speed);
    PersistenceAndPlay();
    FieldEffects();
    Prevention();
    VerifyViewingFarm();
    std::cout<<"PASS mold effects: normal onset, 30 concurrent beds, bed bounds, no replay, fading and harvest\n";
    std::cout<<"PASS mold: timing, relief, forecast, normal rain, wait, HUD, harvest, save migration and rejection\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
