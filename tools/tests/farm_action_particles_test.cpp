#include "farm/render/FarmActionParticleGeometry.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmEconomySystem.h"
#include "3d/LineDrawer.h"
#include <iostream>
#include <fstream>
#include <string_view>
#include <stdexcept>

void LineDrawer::DrawLine(const Vector3&,const Vector3&,const Vector4&) {}
void LineDrawer::DrawWireCube(const Vector3&,float,const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3&,float,const Vector4&,uint32_t) {}
namespace {
using namespace farm;
void Check(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
struct Field {
    FarmGrid grid;
    FarmActionParticles fx;
    FarmIrrigationSystem water;
    FarmVisualSystem visual;
    FarmToolActionSystem tools;
    FarmEconomySystem economy;
    Field() { Check(grid.Initialize(5,4) && grid.SetSelectedIndex(6),"grid"); visual.Initialize({}); water.Initialize(); tools.Initialize(); economy.Initialize(); }
    void Use(FarmTool tool) { fx.OnTool(tools.ApplyToolDetailed(grid,tool,CropType::Carrot,economy,1),grid,visual); }
    void Tick(float dt=.05f,float rain=0,bool visible=true) { fx.Update(grid,water,visual,dt,rain,visible); }
    int Count(FieldGrain kind) const { return static_cast<int>(std::ranges::count(fx.Read(),kind,&FieldParticle::kind)); }
    void Harvest(CropType crop,float moisture,float efficiency=-1.f) {
        FarmTile plant{}; plant.state=FarmTileState::Planted; plant.crop=crop; plant.growth=1; plant.moisture=moisture;
        if(efficiency>=0) {
            plant.careHistory.goodSeconds=10; plant.careHistory.efficiencySeconds=10*efficiency;
            plant.careHistory.nutrientGrowth=plant.careHistory.nutrientSupply=1;
        }
        Check(grid.SetTile(6,plant),"plant"); Use(FarmTool::Harvest);
        Check(grid.GetTile(6)->crop==CropType::None,"harvest did not commit");
    }
};
void ContactSounds() {
    Field f;
    f.Use(FarmTool::Harvest); f.Use(FarmTool::Hoe);
    Check(f.fx.TakeSounds().none(),"failed harvest or airborne hoe made a sound");
    int impacts=0;
    for(int i=0;i<60;++i) {
        f.Tick(1.f/60); impacts+=f.fx.TakeSounds().test(static_cast<size_t>(FieldCue::Hoe));
    }
    Check(impacts==1 && f.fx.TakeSounds().none(),"hoe contact did not sound exactly once");
    for(auto [crop,cue]:{std::pair{CropType::Carrot,FieldCue::Pull},std::pair{CropType::Pumpkin,FieldCue::Cut},
            std::pair{CropType::Tomato,FieldCue::Pick}}) {
        f.Harvest(crop,.2f);
        const auto heard=f.fx.TakeSounds();
        Check(heard.count()==1 && heard.test(static_cast<size_t>(cue)),"wrong harvest sound");
    }
    f.Harvest(CropType::Tomato,.2f); f.Tick(.05f,0,false);
    Check(f.fx.TakeSounds().none(),"hidden feedback survived");
}
void Tools() {
    for (float moisture : {.1f,.75f}) {
        Field f; FarmTile tile{}; tile.moisture=moisture;
        Check(f.grid.SetTile(6,tile),"soil"); f.Use(FarmTool::Harvest);
        Check(f.fx.Read().empty(),"failed harvest emitted");
        f.Use(FarmTool::Hoe);
        Check(f.fx.Read().empty() && f.grid.GetTile(6)->state==FarmTileState::Tilled,"early dirt or delayed commit");
        f.Use(FarmTool::Hoe);
        for (float elapsed=0; elapsed<=hoeing::kHit; elapsed+=.05f) f.Tick(.05f);
        Check(f.Count(FieldGrain::Clod)==(moisture<.42f?6:4) && f.Count(FieldGrain::Dust)==(moisture<.42f?3:0),"dry/wet hoe");
        f.Use(FarmTool::Water);
        Check(f.Count(FieldGrain::PourDrop)>0 && f.Count(FieldGrain::Soak)>0,"watering");
    }
    for (auto crop : {CropType::TestCrop,CropType::Carrot,CropType::Tomato,CropType::Pumpkin}) {
        Field f; f.Harvest(crop,.1f);
        if (crop==CropType::Tomato) Check(f.Count(FieldGrain::Leaf)>0 && !f.Count(FieldGrain::Clod) && !f.Count(FieldGrain::Drop),"dry tomato");
        else {
            Check(f.Count(FieldGrain::Clod)>0,"missing soil");
            Check((f.Count(FieldGrain::Dust)>0)==(crop!=CropType::Pumpkin),"root/pumpkin");
            if (crop==CropType::Pumpkin)
                for(auto& p:f.fx.Read()) {
                    const auto center=f.visual.GetTileCenter(f.grid,6);
                    Check(p.velocity.y<.4f && std::hypot(p.position.x-center.x,p.position.z-center.z)>.2f,
                        "pumpkin clods must leave the contact rim, low to the ground");
                }
        }
        for (int i=0;i<40;++i) f.Tick();
        Check(f.fx.Read().empty(),"particles did not expire");
    }
    Field f; f.Tick(.05f,1); f.Tick(.05f,0); f.Harvest(CropType::Tomato,.1f);
    Check(f.Count(FieldGrain::Drop)==3,"wet tomato");
    for(int i=0;i<40;++i) f.Harvest(CropType::Carrot,.1f);
    Check(f.fx.Read().size()==FarmActionParticles::kCapacity,"pool capacity");
    std::vector<rain::Vertex> mesh(actionfx::kVertexCount+1);
    const auto n=actionfx::Build(std::span(mesh).first(actionfx::kVertexCount),f.fx.Read(),{1,0,0},{0,1,0});
    Check(n && n%3==0 && n<=actionfx::kVertexCount,"mesh");
    for(std::size_t i=0;i<n;++i) Check(rain::Finite(mesh[i].position),"nonfinite vertex");
    Check(!actionfx::Build(std::span(mesh).first(2),f.fx.Read(),{1,0,0},{0,1,0}),"short buffer");
    Check(f.grid.Initialize(3,2),"replace board"); f.Tick();
    Check(f.fx.Read().empty(),"particles survived board replacement");
}
void HoeContact() {
    for(float dt : {1.f/60.f,.092f}) {
        Field f; FarmTile tile{}; tile.heightLevel=2; tile.moisture=.14f;
        Check(f.grid.SetTile(6,tile),"raised plot"); f.Use(FarmTool::Hoe);
        f.Tick(0); const auto* strokes=f.fx.HoeStrokes(f.grid);
        Check(strokes && strokes->WaitingForHit(6) && strokes->Active()->age==0,"paused stroke advanced");
        int bursts=0;
        for(int frame=0;frame<170;++frame) {
            f.Tick(dt);
            const auto* swing=strokes->Active();
            const bool waiting=strokes->WaitingForHit(6);
            const auto mesh=BuildFarmTileMeshParts(f.grid,6,f.visual,0,waiting);
            const auto ridges=std::ranges::count(std::span(mesh.parts).first(mesh.count),FarmMeshShape::SoilRidge,&FarmMeshPart::shape);
            Check(ridges==(waiting?0:3),"ridges not synchronized");
            if(swing && waiting) {
                Check(f.fx.Read().empty() && hoeing::Pose(*swing).Edge().y>swing->contact.y,"blade or particles arrived early");
            }
            const auto p=f.fx.Read();
            if(!p.empty() && p.front().age==0) {
                ++bursts;
                Check(swing && !waiting,"impact without tool");
                const auto edge=hoeing::Pose(*swing).Edge();
                Check(std::abs(edge.x-swing->contact.x)+std::abs(edge.y-swing->contact.y)+std::abs(edge.z-swing->contact.z)<.0001f,"blade missed surface");
                for(auto& g:p) Check(std::hypot(g.position.x-edge.x,g.position.z-edge.z)<.065f && std::abs(g.position.y-edge.y)<.001f,"detached soil");
            }
        }
        Check(bursts==1 && !strokes->Active() && f.fx.Read().empty(),"duplicate or missing burst");
    }
    Field f;
    f.Use(FarmTool::Hoe); f.Use(FarmTool::Hoe);
    Check(f.grid.SetSelectedIndex(7),"select next"); f.Use(FarmTool::Hoe);
    const auto* strokes=f.fx.HoeStrokes(f.grid);
    Check(strokes->Active()->tile==7 && !strokes->WaitingForHit(6) && strokes->WaitingForHit(7),"second stroke waited");
    Check(f.Count(FieldGrain::Clod)==6 && f.tools.GetHistory().GetUndoCount()==2,"interrupted contact not committed exactly once");
    Check(f.grid.SetSelectedIndex(8),"third plot"); f.Use(FarmTool::Hoe);
    Check(strokes->Active()->tile==8 && !strokes->WaitingForHit(7) && f.Count(FieldGrain::Clod)==12 &&
        f.tools.GetHistory().GetUndoCount()==3,"rapid third stroke lost or repeated");
    Check(f.grid.SetTile(8,{}),"undo before impact"); f.Tick();
    Check(!strokes->Active(),"reverted plot kept tool");
    f.Use(FarmTool::Hoe); f.Tick(.05f,0,false);
    Check(!strokes->Active() && f.fx.Read().empty(),"hidden stroke survived");
    f.grid.SetSelectedIndex(8); f.Use(FarmTool::Hoe);
    Check(f.grid.Initialize(2,2) && !f.fx.HoeStrokes(f.grid),"stale board");
    Field afterHit;
    afterHit.Use(FarmTool::Hoe);
    for (int i=0;i<7;++i) afterHit.Tick();
    const int clods=afterHit.Count(FieldGrain::Clod);
    afterHit.Use(FarmTool::Water);
    Check(clods==6 && afterHit.Count(FieldGrain::Clod)==clods && !afterHit.fx.HoeStrokes(afterHit.grid)->Active(),
        "changing tools replayed an existing impact");
}
void Sowing() {
    for(auto crop:{CropType::TestCrop,CropType::Carrot,CropType::Tomato,CropType::Pumpkin}) {
        Field f; FarmTile bed{}; bed.state=FarmTileState::Tilled; bed.heightLevel=1;
        Check(f.grid.SetTile(6,bed),"seedbed");
        const int stock=f.economy.GetSeedCount(crop);
        if(stock) Check(f.economy.RemoveSeed(crop,stock),"empty stock");
        auto use=[&] {
            const auto result=f.tools.ApplyToolDetailed(f.grid,FarmTool::Seed,crop,f.economy,1);
            f.fx.OnTool(result,f.grid,f.visual); return result.status;
        };
        Check(use()==FarmToolActionStatus::NoSeed && f.fx.Read().empty(),"no stock must not emit");
        Check(f.economy.AddSeed(crop,2),"seed stock");
        Check(use()==FarmToolActionStatus::Applied && f.economy.GetSeedCount(crop)==1,"commit once");
        Check(f.Count(FieldGrain::Seed)==4 && f.Count(FieldGrain::CoverSoil)==8,"incomplete burst");
        Check(use()!=FarmToolActionStatus::Applied && f.fx.Read().size()==12 && f.economy.GetSeedCount(crop)==1,"occupied bed emitted");
        Check(f.fx.SproutScale(f.grid,6,crop)==0.f && f.fx.SproutScale(f.grid,5,crop)==1.f,"early sprout or stale neighbor");
        std::array<rain::Vertex,600> mesh;
        const auto vertices=actionfx::Build(mesh,f.fx.Read(),{1,0,0},{0,1,0});
        Check(vertices>0 && vertices==36,"pending grains must not draw");
        std::array<Vector3,4> land{};
        bool stagger=false,allLanded=false,coverSeen=false,covered=false,emerging=false;
        for(int frame=0;frame<120;++frame) {
            f.Tick(1.f/60);
            int visible=0,down=0,earth=0,atTarget=0;
            std::array<Vector3,4> landedThisFrame{};
            for(const auto& p:f.fx.Read()) if(p.age>=0) {
                if(p.kind==FieldGrain::Seed) {
                    ++visible;
                    if(std::abs(p.position.y-p.floor)<.0001f && down<4) landedThisFrame[down++]=p.position;
                } else if(p.kind==FieldGrain::CoverSoil) {
                    ++earth; float closest=2.f;
                    for(const auto& target:land) closest=(std::min)(closest,std::hypot(p.position.x-target.x,p.position.z-target.z));
                    if(closest<.06f) ++atTarget;
                }
            }
            if(visible>0 && visible<4 && !earth) stagger=true;
            if(down==4 && !earth) { land=landedThisFrame; allLanded=true; }
            if(earth) { Check(allLanded,"soil moved before final landing"); coverSeen=true; }
            if(atTarget==8 && !visible) covered=true;
            const float sprout=f.fx.SproutScale(f.grid,6,crop);
            if(sprout>0 && sprout<1) {Check(covered,"sprout appeared before cover"); emerging=true;}
        }
        Check(stagger && allLanded && coverSeen && covered && emerging && f.fx.Read().empty(),"incomplete sowing phases");
        Check(f.grid.GetTile(6)->crop==crop && f.grid.GetTile(6)->growth==0 && f.economy.GetSeedCount(crop)==1,"feedback mutated simulation");
    }
    Field f; FarmTile bed{}; bed.state=FarmTileState::Tilled;
    Check(f.grid.SetTile(6,bed) && f.economy.AddSeed(CropType::Carrot,3),"reset fixture");
    f.Use(FarmTool::Seed); f.Tick(.08f);
    Check(f.grid.SetTile(6,bed),"revert planted tile"); f.Tick(.08f);
    Check(f.fx.Read().empty(),"removed plant left pending soil");
    f.Use(FarmTool::Seed); f.Tick(.05f,0,false); f.Tick();
    Check(f.fx.Read().empty() && f.fx.SproutScale(f.grid,6,CropType::Carrot)==1,"preview retained seed effect");
    for(int i=0;i<30;++i) f.Harvest(CropType::Carrot,.1f);
    const size_t full=f.fx.Read().size(); Check(full==FarmActionParticles::kCapacity,"fill pool");
    f.Use(FarmTool::Seed);
    Check(f.fx.Read().size()==full && f.Count(FieldGrain::Seed)==0 && f.grid.GetTile(6)->crop==CropType::Carrot,"full pool affected commit");
}
void HandWater() {
    using namespace watering;
    Field f; FarmTile bed{}; bed.state=FarmTileState::Tilled; bed.heightLevel=2; bed.moisture=.1f;
    f.Use(FarmTool::Water); Check(f.fx.Read().empty(),"water on an untilled tile");
    Check(f.grid.SetTile(6,bed),"raised bed"); f.Use(FarmTool::Water);
    const float committed=f.grid.GetTile(6)->moisture;
    const auto pending=[&]{return f.fx.PendingWater(f.grid,6);};
    Check(committed>.1f && std::abs(committed-pending()-.1f)<.0001f,"instant visual darkening");
    const auto pours=f.fx.Pours(f.grid);
    const auto it=std::ranges::find(pours,6,&Pour::tile);
    Check(it!=pours.end() && f.Count(FieldGrain::PourDrop)==kDrops,"missing can");
    for(const auto& drop:f.fx.Read()) if(drop.kind==FieldGrain::PourDrop) {
        const auto start=Pose(*it,-drop.age).Rose();
        Check(std::abs(start.x-drop.position.x)+std::abs(start.y-drop.position.y)+std::abs(start.z-drop.position.z)<.0001f,"detached nozzle");
    }
    bool falling=false,wet=false,bounced=false; float last=pending();
    for(int step=0;step<140;++step) {
        f.Tick(1.f/60);
        for(const auto& p:f.fx.Read()) if (p.age>=0) {
            if(p.kind==FieldGrain::Soak) wet=true;
            if(p.kind==FieldGrain::PourDrop) {
                falling|=!p.bounced; bounced|=p.bounced && p.position.y>p.floor+.025f;
                Check(p.position.y>=p.floor-.0001f,"drop below soil");
            }
        }
        Check(!wet || falling,"stain before shower");
        Check(wet || std::abs(committed-pending()-.1f)<.0001f,"dry ground darkened before impact");
        Check(pending()<=last+.0001f && f.fx.PendingWater(f.grid,5)==0,"moisture on wrong tile or regressed"); last=pending();
    }
    Check(wet && bounced && last==0 && f.fx.Read().empty() && f.grid.GetTile(6)->moisture==committed,"incomplete pour");
    bed.moisture=1; Check(f.grid.SetTile(6,bed),"full bed"); f.Use(FarmTool::Water);
    Check(f.fx.Read().empty(),"saturated soil emitted");
    bed.moisture=0; Check(f.grid.SetTile(6,bed),"new dry bed"); f.Use(FarmTool::Water); f.Tick(.09f);
    f.Use(FarmTool::Water); Check(f.Count(FieldGrain::PourDrop)==kDrops && std::abs(f.grid.GetTile(6)->moisture-pending())<.0001f,"repeat flash or double can");
    bed.moisture=.015f; Check(f.grid.SetTile(6,bed),"crop drew water during pour");
    Check(pending()<=bed.moisture && pending()>0,"visual water exceeded current soil water");
    for(int j=0;j<9;++j) f.Tick(.1f);
    Check(pending()>0 && pending()<bed.moisture,"remaining moisture never reached the soil");
    f.Tick(.05f,0,false); Check(f.fx.Read().empty() && pending()==0,"hidden pour survived");
    for(int j=0;j<25;++j) f.Harvest(CropType::Carrot,0);
    f.Use(FarmTool::Water); Check(f.Count(FieldGrain::PourDrop)==0 && pending()==0 && f.grid.GetTile(6)->moisture>0,"full pool changed gameplay");
}
void Water() {
    Field f; FarmTile canal{}, source{}, soil{};
    canal.feature=FarmTileFeature::Canal; source.feature=FarmTileFeature::WaterSource; source.waterAmount=1;
    soil.state=FarmTileState::Planted; soil.crop=CropType::Tomato; soil.moisture=.05f;
    Check(f.grid.SetTile(0,source) && f.grid.SetTile(1,canal) && f.grid.SetTile(6,canal) &&
        f.grid.SetTile(7,soil) && f.grid.SetTile(18,canal),"layout");
    f.water.Rebuild(f.grid); f.Tick();
    Check(f.fx.Read().empty(),"potential path emitted without transfer");
    bool seenTurn=false,seenArrival=false,seenFoam=false,seenCurve=false;
    const auto dry=f.visual.GetTileCenter(f.grid,18);
    for(int i=0;i<60;++i) {
        f.water.UpdateWater(f.grid,.05f,1); f.Tick();
        seenTurn |= f.Count(FieldGrain::Drop)>0;
        seenArrival |= f.Count(FieldGrain::WetSoil)>0 || f.Count(FieldGrain::Seep)>0;
        seenFoam |= f.Count(FieldGrain::Foam)>0;
        for (const auto& p:f.fx.Read()) if (p.kind==FieldGrain::Foam)
            seenCurve |= std::abs(p.foamAcceleration.x)>1.f && std::abs(p.foamAcceleration.z)>1.f;
        for(auto& p:f.fx.Read()) Check(std::abs(p.position.z-dry.z)>1.f || std::abs(p.position.x-dry.x)>1.f,"dry spur emitted");
    }
    Check(seenTurn,"missing bank splash"); Check(seenArrival,"missing soil delivery");
    Check(seenFoam,"missing steady foam"); Check(seenCurve,"missing curved streamline");
    for(int i=0;i<40;++i) f.Tick();
    Check(f.fx.Read().empty(),"stale measurements emitted while paused");
    f.water.UpdateWater(f.grid,.05f,1); f.Tick(.05f,0,false); f.Tick();
    Check(f.fx.Read().empty(),"preview leaked into presentation");
}
void BasketSpill() {
    Field f; f.Tick();
    for(float wet:{.1f,.8f}) {
        f.fx.Clear(); f.Tick();
        const farm::harvest::BasketLanding hit{{1,.45f,0},.03f,-1,1,wet};
        f.fx.OnBasketLanding(f.grid,hit);
        Check(f.Count(FieldGrain::Clod)==(wet<.42f?3:2),"basket spill count");
        for(const auto& p:f.fx.Read())
            Check(p.age==0 && p.position.x==hit.rim.x && p.velocity.x<0 && p.floor>=hit.floor,"soil not at lowered rim");
        for(int n=0;n<18;++n) f.Tick();
        Check(f.fx.Read().empty(),"basket dirt did not expire");
        Check(f.grid.Initialize(5,4),"replace board"); f.fx.OnBasketLanding(f.grid,hit);
        Check(f.fx.Read().empty(),"basket dirt in replacement board");
    }
}
void HarvestSizes() {
    using enum harvest::SizeCue;
    Check(harvest::CueFor({.849f,true})==Small && harvest::CueFor({.85f,true})==Regular &&
        harvest::CueFor({1.499f,true})==Regular && harvest::CueFor({1.5f,true})==Large &&
        harvest::CueFor({})==Regular && harvest::CueFor({NAN,true})==Regular,"harvest size boundaries");
    for(auto crop:{CropType::Carrot,CropType::Tomato,CropType::Pumpkin}) {
        Field light,heavy; light.Harvest(crop,.2f,.025f); heavy.Harvest(crop,.2f,1.f);
        const auto kind=crop==CropType::Tomato?FieldGrain::Leaf:FieldGrain::Clod;
        Check(light.Count(kind)>0 && heavy.Count(kind)>light.Count(kind),"size did not affect release");
    }
    for(float wet:{.15f,.8f}) for(auto size:{Small,Regular,Large}) {
        Field f; f.Tick();
        const harvest::BasketLanding hit{{1,.4f,0},0,1,1,wet,size,{0,0,0},{.75f,.48f}};
        f.fx.OnBasketLanding(f.grid,hit);
        const int fromRim=size==Small?1:wet<.42f?3:2;
        Check(f.Count(FieldGrain::Clod)==fromRim+(wet>.42f && size==Large?4:0) &&
            f.Count(FieldGrain::LandingPuff)==(wet<.42f && size==Large?4:0),"size or moisture confused at contact");
        for(const auto& p:f.fx.Read()) if(p.kind==FieldGrain::LandingPuff) {
            std::array<rain::Vertex,actionfx::kPerGrain> mesh{};
            Check(actionfx::Build(mesh,{&p,1},{1,0,0},{0,1,0})==36,"landing puff exceeds budget or missing lobes");
        }
        for(const auto& p:f.fx.Read()) if(p.position.y<.3f)
            Check(p.position.x*p.velocity.x>0 && p.position.z*p.velocity.z>0 &&
                std::abs(p.position.x)==.75f && std::abs(p.position.z)==.48f,"foot effects not moving outwards");
        for(int i=0;i<30;++i) f.Tick(); Check(f.fx.Read().empty(),"impact did not expire");
    }
}
void DirectionalSoilArrival() {
    for(int donor : {1,5,7,11}) {
        Field f;
        FarmTile source{}, pipe{}, soil{};
        source.feature=FarmTileFeature::WaterSource; source.waterAmount=1;
        pipe.feature=FarmTileFeature::Canal; soil.state=FarmTileState::Tilled;
        const int sourceIndex=donor==1?0:donor==5?10:donor==7?8:16;
        Check(f.grid.SetTile(sourceIndex,source) && f.grid.SetTile(donor,pipe) && f.grid.SetTile(6,soil),"soil fixture");
        const auto bed=f.visual.GetTileVisualData(f.grid,6);
        const auto inlet=f.visual.GetTileCenter(f.grid,donor);
        const auto axis=bed.center-inlet;
        const float length=std::hypot(axis.x,axis.z), dx=axis.x/length, dz=axis.z/length;
        float lastReach=0; int activeFrames=0;
        for(int frame=0;frame<55;++frame) {
            f.water.UpdateWater(f.grid,.05f,1); f.Tick();
            Check(f.Count(FieldGrain::Seep)<=1,"extra soil cue in unwatered bed");
            for(const auto& p:f.fx.Read()) if(p.kind==FieldGrain::Seep) {
                Check(p.flowTile==6,"wet wrong plot");
                std::array<rain::Vertex,actionfx::kPerGrain> v{};
                auto particles=std::span(&p,1);
                const auto n=actionfx::Build(v,particles,{1,0,0},{0,1,0});
                Check(n>0 && n<=v.size() && n%3==0,"soil vertex budget");
                Check(!actionfx::Build(std::span(v).first(n-1),particles,{1,0,0},{0,1,0}),"soil short buffer");
                float reach=0;
                for(size_t j=0;j<n;++j) {
                    const auto pos=v[j].position;
                    Check(rain::Finite(pos) && std::abs(pos.x-bed.center.x)<=.86f*bed.halfExtent &&
                        std::abs(pos.z-bed.center.z)<=.86f*bed.halfExtent,"soil stain crossed plateau edge");
                    const float along=(pos.x-p.position.x)*dx+(pos.z-p.position.z)*dz;
                    Check(along>=-.001f,"stain moved back onto canal");
                    reach=(std::max)(reach,along);
                }
                Check(reach+.001f>=lastReach,"stain retreats before fading");
                lastReach=reach; ++activeFrames;
            }
        }
        Check(activeFrames>5 && lastReach>bed.halfExtent && !f.Count(FieldGrain::Seep),"missing penetration or expiry");
    }
}
void FirstArrival() {
    Field f; FarmTile pipe{},source{},bed{};
    pipe.feature=FarmTileFeature::Canal; source.feature=FarmTileFeature::WaterSource; source.waterAmount=1;
    bed.state=FarmTileState::Tilled; bed.moisture=.05f;
    for(int i:{12,11,7,0}) Check(f.grid.SetTile(i,pipe),"branches");
    Check(f.grid.SetTile(18,source) && f.grid.SetTile(6,bed),"source and soil");
    for(int i=0;i<5;++i) {f.water.UpdateWater(f.grid,.05f,1); f.Tick();}
    Check(f.fx.WaterFront(f.grid,12).parent<0,"disconnected front");
    Check(f.tools.PlaceCanalPath(f.grid,{17}),"last connection");
    int first[20]{},starts[20]{},soilAt=0,donor=-1;
    bool moving[20]{};
    for(int frame=1;frame<=110;++frame) {
        if(frame==1) for(int step=0;step<5;++step) f.water.UpdateWater(f.grid,.016f,1);
        f.water.UpdateWater(f.grid,.05f,1);
        const auto flows=f.water.GetLastTileFlows(f.grid);
        if(donor<0 && flows[6].soilReceived>0) donor=flows[6].soilDonor;
        f.Tick();
        for(int i=0;i<20;++i) {
            const auto front=f.fx.WaterFront(f.grid,i);
            const bool active=front.fraction>0 && front.fraction<1;
            if(active && !moving[i]) { if(!first[i]) first[i]=frame; ++starts[i]; }
            moving[i]=active;
            if(active && i==17) Check(front.outlet==12,"missing bend outlet");
            if(active && i==12) Check(front.outlet<0,"guessed a single outlet at fork");
        }
        for(const auto& p:f.fx.Read()) {
            if(p.kind==FieldGrain::Seep && p.age>=0 && !soilAt) {
                Check(p.flowTile==6 && donor>=0,"unmeasured seep");
                const auto c=f.visual.GetTileCenter(f.grid,6), d=f.visual.GetTileCenter(f.grid,donor);
                Check((p.position.x-c.x)*(d.x-c.x)+(p.position.z-c.z)*(d.z-c.z)>0,"far bank wets first");
                soilAt=frame;
            }
        }
    }
    Check(first[17]>0 && first[12]>=first[17]+7 && first[7]>=first[12]+7 &&
        first[11]==first[7] && donor>=0 && donor<20 && soilAt>=first[donor]+6,"upstream/branch/delivery order");
    for(int i:{17,12,7,11}) Check(starts[i]==1,"repeated crest during steady flow");
    Check(starts[0]==0,"dry spur");
    for(const auto upstream: {Vector3{-1,0,0},Vector3{1,0,0},Vector3{0,0,-1},Vector3{0,0,1}}) {
        FarmTileMeshParts mesh{};
        mesh.count=3; mesh.parts[1].water=mesh.parts[2].water=true;
        mesh.parts[1].scale={1,.01f,1}; mesh.parts[1].slope={.1f,.2f};
        const Vector3 center{3,1,-4}, parent{center.x+upstream.x,1,center.z+upstream.z};
        auto dry=mesh;
        RevealFarmWater(dry,center,parent,center,1,0);
        Check(dry.count==1 && !dry.parts[0].water,"dry bed or bank removed");
        for(float u:{.1f,.5f,.9f}) for(int turn:{-1,0,1}) {
            const Vector3 out=turn ? Vector3{-upstream.z*turn,0,upstream.x*turn} : Vector3{-upstream.x,0,-upstream.z};
            const Vector3 outlet{center.x+out.x,1,center.z+out.z};
            auto revealed=mesh; RevealFarmWater(revealed,center,parent,outlet,1,u);
            const auto& p=revealed.parts[1]; const auto front=p.wetFront;
            const auto inside=[&](float x,float z){return front.z-front.x*x-front.y*z;};
            Check(revealed.count==3 && p.position.x==mesh.parts[1].position.x && p.scale.x==1 &&
                p.slope.x==.1f && front.w>0 && revealed.parts[0].wetFront.w==0 &&
                revealed.parts[2].wetFront.z==front.z,"parts not clipped by one boundary");
            Check(std::abs(front.x*front.x+front.y*front.y-1)<.001f &&
                inside(parent.x,parent.z)>0 && inside(outlet.x,outlet.z)<0,"inlet or outlet discontinuity");
            const float boundary=turn ? inside(parent.x+out.x,parent.z+out.z)
                : inside(parent.x+2*u*out.x,parent.z+2*u*out.z);
            Check(std::abs(boundary)<.001f,"front left its geometric path");
        }
        RevealFarmWater(mesh,center,parent,center,1,1);
        Check(mesh.count==3 && mesh.parts[1].wetFront.w==0,"finished front still visible");
    }
    Field undo;
    Check(undo.grid.SetTile(18,source) && undo.grid.SetTile(17,pipe) && undo.grid.SetTile(12,pipe),"undo setup");
    for(int step=0;step<3;++step) undo.water.UpdateWater(undo.grid,.016f,1);
    undo.Tick();
    Check(undo.fx.WaterFront(undo.grid,17).parent==18 && undo.fx.WaterFront(undo.grid,12).fraction==0,"visible downstream before wave");
    Check(undo.tools.RemoveCanalPath(undo.grid,{17}),"disconnect");
    undo.water.UpdateWater(undo.grid,.05f,1); undo.Tick();
    Check(undo.fx.WaterFront(undo.grid,17).parent<0 && undo.fx.WaterFront(undo.grid,12).fraction==1,"wave survived disconnection");
}
void PondArrival() {
    for (int gate:{9,19}) {
        Field f; Check(pond::Start(f.grid),"pond scene");
        Check(f.tools.DigCanalAt(f.grid,14,false),"sealed middle ditch");
        Check(f.tools.DigCanalAt(f.grid,gate),"open side mouth");
        f.Tick();
        int seen[20]{};
        for (int frame=1;frame<65;++frame) {
            f.water.UpdateWater(f.grid,.05f,1); f.Tick();
            for(int i:{gate,14,13}) {
                auto r=f.fx.WaterFront(f.grid,i);
                if(r.fraction<=0 || r.fraction>=1) continue;
                if(!seen[i]) seen[i]=frame;
                if(i==gate) Check(r.fromPond && r.parent==-1 && r.outlet==14,"missing outside-to-turn wave");
                else Check(!r.fromPond && r.parent==(i==14?gate:14),"wave coming from tool instead of donor");
            }
        }
        Check(seen[gate]>0 && seen[14]>seen[gate] && seen[13]>seen[14],"side to side arrival order");
        Check(!f.fx.WaterFront(f.grid,gate).fromPond,"pond wave repeated");
    }
}
// Optional QA trace: vertices in tile-local coordinates, exactly as submitted by the field batch.
void Trace(const char* path) {
    std::ofstream file(path,std::ios::binary);
    std::vector<rain::Vertex> mesh(actionfx::kVertexCount);
    for (int shot=0;shot<6;++shot) {
        Field f;
        if (shot<2) {
            FarmTile soil{}; soil.moisture=shot?.8f:.1f; Check(f.grid.SetTile(6,soil),"trace soil"); f.Use(FarmTool::Hoe);
        } else if (shot<5) {
            f.Tick(.05f,1);
            const CropType choices[]={CropType::Carrot,CropType::Tomato,CropType::Pumpkin};
            f.Harvest(choices[shot-2],.2f);
        } else {
            FarmTile src{},canal{},soil{}; src.feature=FarmTileFeature::WaterSource; src.waterAmount=1;
            canal.feature=FarmTileFeature::Canal; soil.state=FarmTileState::Planted; soil.crop=CropType::Tomato; soil.moisture=.05f;
            Check(f.grid.SetTile(0,src) && f.grid.SetTile(1,canal) && f.grid.SetTile(6,canal) && f.grid.SetTile(7,soil),"trace canal");
        }
        auto origin=f.visual.GetTileCenter(f.grid,6);
        if (shot==5) origin.z=(origin.z+f.visual.GetTileCenter(f.grid,1).z)*.5f;
        for (int frame=0;frame<36;++frame) {
            if (shot==5) f.water.UpdateWater(f.grid,.04f,1);
            f.Tick(.04f);
            const auto count=static_cast<uint32_t>(actionfx::Build(mesh,f.fx.Read(),{1,0,0},{0,.89f,-.46f}));
            for (uint32_t j=0;j<count;++j) {
                mesh[j].position.x-=origin.x; mesh[j].position.y-=origin.y; mesh[j].position.z-=origin.z;
            }
            file.write(reinterpret_cast<const char*>(&count),sizeof(count));
            file.write(reinterpret_cast<const char*>(mesh.data()),count*sizeof(mesh[0]));
        }
    }
    Check(static_cast<bool>(file),"trace output");
}
}
int main(int argc,char** argv) try {
    ContactSounds(); Tools(); Water(); FirstArrival(); PondArrival(); DirectionalSoilArrival(); Sowing(); HandWater(); HoeContact(); BasketSpill(); HarvestSizes();
    if (argc==3 && std::string_view(argv[1])=="--trace") Trace(argv[2]);
    std::cout<<"PASS: committed tools, sowing, can pose, drops, rebound, soaking, every crop, water paths, preview, capacity and geometry\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
