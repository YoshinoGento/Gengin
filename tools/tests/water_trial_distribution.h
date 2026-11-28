#pragma once
// Included in the trial regression executable. No alternate implementation of irrigation.
namespace distribution {
using namespace farm;
using namespace farm::trial;
constexpr float tick=1.f/60;
inline void Require(bool ok,const char* why) { if(!ok) throw std::runtime_error(why); }
inline int Flip(int i) { return i/width*width+width-1-i%width; }
struct Trial {
    FarmGrid grid;
    FarmIrrigationSystem water;
    FarmGrowthSystem growth;
    FarmEconomySystem economy;
    FarmToolActionSystem tools;
    std::array<int,2> count{}, quality{};
    std::array<double,width*height> edge{};
    double input=0, delivered=0,used=0,removed=0,worst=0;
    float speed=1;
    std::array<float,2> lastPick{};
    bool flip=false;
    int Map(int i) const { return flip?Flip(i):i; }
    void Init(bool detour,bool reverse,bool mirror,bool internal,const FarmRules& rules={},SoilIntake intake=SoilIntake::PathStrength) {
        System source; Require(source.Reset(true),"fixture reset");
        std::vector<int> ditch=detour?std::vector<int>{gate,13,12,11,6,1}:std::vector<int>{gate,13,12,7,fork};
        if(reverse) std::reverse(ditch.begin(),ditch.end());
        for(int i:ditch) Require(source.Act(i,Work::Dig),"fixture excavation");
        for(int bed:{nearBed,farBed}) Require(source.Act(bed,Work::Hoe)&&source.Act(bed,Work::Seed),"fixture seed");
        FarmGrid::Snapshot board; source.Grid().CaptureSnapshot(board);
        if(internal) { board.pondBank=false; board.tiles[gate].feature=FarmTileFeature::WaterSource; }
        flip=mirror;
        if(flip) for(int i=0;i<width*height;++i) if(i<Flip(i)) std::swap(board.tiles[i],board.tiles[Flip(i)]);
        Require(grid.ReplaceSnapshot(board),"fixture board");
        water.Initialize(rules,intake); growth.Initialize(); tools.Initialize(); economy.Initialize();
        Require(economy.AddSeed(CropType::Carrot,16),"fixture inventory");
    }
    void Step(int n,bool print) {
        water.UpdateWater(grid,tick,speed);
        auto totals=water.GetLastStep(grid);
        auto flows=water.GetLastTileFlows(grid);
        Require(totals.valid && flows.size()==edge.size() && std::abs(totals.balanceError)<1.e-5,"measurement invalid");
        input+=totals.sourceRefill; delivered+=totals.soilDelivered;
        worst=(std::max)(worst,std::abs(totals.balanceError));
        for(int i=0;i<width*height;++i) edge[i]+=flows[Map(i)].incoming;
        std::array<float,2> in{},wet{};
        for(int j=0;j<2;++j) {
            int i=Map(j?farBed:nearBed);
            in[j]=flows[i].soilReceived/(tick*speed); wet[j]=grid.GetTile(i)->moisture;
        }
        growth.Update(grid,tick,speed);
        for(int j=0;j<2;++j) used+=wet[j]-grid.GetTile(Map(j?farBed:nearBed))->moisture;
        if(print && (n==60 || n==600 || n==1200 || n==1800)) {
            std::cout<<"t="<<n/60<<" input_sum="<<input<<" stock="<<totals.stockAfter<<" soil_sum="<<delivered<<" edges(parent>child:sum)=";
            for(int i=0;i<width*height;++i) if(edge[i]>0) std::cout<<water.GetUpstreamTileIndex(i)<<'>'<<i<<':'<<edge[i]<<' ';
            for(int j=0;j<2;++j) {
                int i=j?farBed:nearBed; int donor=flows[i].soilDonor;
                auto& tile=*grid.GetTile(i);
                std::cout<<" bed="<<i<<" donor="<<donor<<" store="<<(donor<0?0.f:grid.GetTile(donor)->waterAmount)
                   <<" in/use="<<in[j]<<'/'<<(wet[j]-tile.moisture)/tick<<" moisture="<<tile.moisture;
            }
            std::cout<<'\n';
        }
        for(int j=0;j<2;++j) {
            int i=Map(j?farBed:nearBed);
            auto& tile=*grid.GetTile(i);
            if(count[j]>=3 || !IsHarvestReady(tile)) continue;
            float before=tile.moisture;
            grid.SetSelectedIndex(i);
            Require(tools.ApplyToolDetailed(grid,FarmTool::Harvest,CropType::Carrot,economy).Succeeded(),"fixture harvest");
            quality[j]=economy.GetLastHarvestQuality().score;
            removed+=before-grid.GetTile(i)->moisture;
            lastPick[j]=n*tick*speed;
            if(print) std::cout<<"pick t="<<n*tick<<" bed="<<i<<" crop="<<count[j]+1<<" q="<<quality[j]
                <<" moisture="<<before<<'>'<<grid.GetTile(i)->moisture<<'\n';
            if(++count[j]<3) Require(tools.ApplyToolDetailed(grid,FarmTool::Seed,CropType::Carrot,economy).Succeeded(),"fixture replant");
            tools.ClearHistory();
        }
    }
    void Compare(const Trial& other) const {
        Require(count==other.count && quality==other.quality,"order changed harvest");
        Require(std::abs(input-other.input)<1.e-6 && std::abs(delivered-other.delivered)<1.e-6,"order changed totals");
        for(int i=0;i<width*height;++i) {
            const auto& a=*grid.GetTile(i); const auto& b=*other.grid.GetTile(other.Map(i));
            Require(a.waterAmount==b.waterAmount && a.moisture==b.moisture && a.growth==b.growth &&
                edge[i]==other.edge[i],"order or mirror changed distribution");
        }
    }
};
inline void Cases() {
    for(bool detour:{false,true}) for(float speed:{1.f,4.f}) {
        Trial f,other; f.speed=other.speed=speed;
        f.Init(detour,false,false,false,{},SoilIntake::LocalContact);
        other.Init(detour,true,true,true,{},SoilIntake::LocalContact);
        std::cout<<"local detour="<<detour<<" speed="<<speed<<'\n';
        for(int n=1;n<=6000 && (f.count[0]<3 || f.count[1]<3);++n) {
            f.Step(n,speed==1); other.Step(n,false); f.Compare(other);
            for(int i=0;i<width*height;++i) {
                float a=f.grid.GetTile(i)->waterAmount; Require(a>=0 && a<=1,"stock bounds");
            }
        }
        Require(f.count==std::array{3,3},"local harvest");
        const double soil=f.grid.GetTile(nearBed)->moisture+f.grid.GetTile(farBed)->moisture;
        Require(std::abs(.5+f.delivered-f.used-f.removed-soil)<.0001,"combined soil accounting");
        std::cout<<"local result quality="<<f.quality[0]<<','<<f.quality[1]<<" t="<<f.lastPick[0]<<','<<f.lastPick[1]
            <<" input="<<f.input<<" soilIn="<<f.delivered<<" used="<<f.used<<" harvestLoss="<<f.removed<<" worst="<<f.worst<<'\n';
    }
    for(bool detour:{false,true}) {
        Trial original,backwards,internal,mirrored;
        original.Init(detour,false,false,false);
        backwards.Init(detour,true,false,false);
        internal.Init(detour,false,false,true); mirrored.Init(detour,true,true,true);
        std::cout<<(detour?"single trunk\n":"fork\n");
        for(int n=1;n<=6000;++n) {
            original.Step(n,true); backwards.Step(n,false); internal.Step(n,false); mirrored.Step(n,false);
            original.Compare(backwards); original.Compare(internal); original.Compare(mirrored);
        }
        Require(original.quality==(detour?std::array{61,70}:std::array{47,47}),"harvest baseline");
    }
    for(int intervention=0;intervention<3;++intervention) {
        FarmRules rules{};
        if(intervention==0) rules.irrigationSourceRefillPerSecond*=2;
        if(intervention==1) rules.irrigationWaterTransferPerSecond*=2;
        if(intervention==2) rules.irrigationMoistureRecoveryPerSecond*=2;
        Trial f; f.Init(false,false,false,false,rules);
        for(int n=1;n<=6000;++n) f.Step(n,false);
        std::cout<<"fork x2 "<<std::array{"refill","edge","seep"}[intervention]<<" third quality="<<f.quality[0]<<','<<f.quality[1]<<'\n';
        Require(f.quality[0]==f.quality[1] && (intervention==2?f.quality[0]>47:f.quality[0]==47),"sensitivity");
    }
    FarmGrid loop; Require(loop.Initialize(5,4),"loop init");
    for(int i:{2,1,3,6,8,11,13,12}) {
        auto tile=*loop.GetTile(i); tile.feature=i==2?FarmTileFeature::WaterSource:FarmTileFeature::Canal;
        Require(loop.SetTile(i,tile),"loop tile");
    }
    FarmIrrigationSystem water; water.Initialize(); water.Rebuild(loop);
    const int parent=water.GetUpstreamTileIndex(12);
    Require(parent==11 && Flip(parent)==13,"diamond parent tie");
    FarmGrid mirrorLoop;Require(mirrorLoop.Initialize(5,4),"mirror network");
    for(int i=0;i<20;++i) Require(mirrorLoop.SetTile(Flip(i),*loop.GetTile(i)),"mirror tile");
    FarmIrrigationSystem mirrorWater;mirrorWater.Initialize({},SoilIntake::LocalContact);
    water.Initialize({},SoilIntake::LocalContact);
    double left=0,right=0,mirrorLeft=0,mirrorRight=0;
    for(int i=0;i<240;++i) {
        water.UpdateWater(loop,tick,1);mirrorWater.UpdateWater(mirrorLoop,tick,1);
        const auto a=water.GetLastTileFlows(loop),b=mirrorWater.GetLastTileFlows(mirrorLoop);
        left+=a[11].outgoing;right+=a[13].outgoing;
        mirrorLeft+=b[11].outgoing;mirrorRight+=b[13].outgoing;
    }
    Require(left>0 && right==0 && left==mirrorLeft && mirrorRight==0,"loop transport bias changed: reassess");
    std::cout<<"loop 4s outgoing 11/13="<<left<<'/'<<right<<" mirrored="<<mirrorLeft<<'/'<<mirrorRight<<'\n';
    System f; Require(f.Reset(true),"close reset");
    for(int i:{gate,13,12,7,fork}) Require(f.Act(i,Work::Dig),"close layout");
    Require(f.Act(nearBed,Work::Hoe)&&f.Act(nearBed,Work::Seed),"close crop");
    for(int n=0;n<600;++n) f.Step(tick);
    const float open=f.Flows()[nearBed].soilReceived/tick;
    const auto r=f.Inspect(nearBed);
    Require(r.trend==BedTrend::HoldingLow && r.donor==7 && r.donorStock>.99f &&
        std::abs(r.inRate-open)<1.e-6f && std::abs(r.usedRate-1.f/55)<1.e-5f,"bed reading");
    Require(f.Act(gate,Work::Dig),"shut"); f.Step(tick);
    const float shut=f.Flows()[nearBed].soilReceived/tick;
    Require(shut>2*open && f.Flows()[gate].sourceRefill==0,"retained-water discontinuity");
    std::cout<<"diamond parent="<<parent<<" reflected parent="<<Flip(parent)<<" shut gate seep "<<open<<'>'<<shut<<" units/s\n";
    Require(f.Act(nearBed,Work::Water) && f.Inspect(nearBed).trend==BedTrend::NoSample,"stale tool sample");
    const auto actions=f.Stats().actions;
    Require(!f.Act(nearBed,static_cast<Work>(-1)) && !f.Act(width*height,Work::Dig),"invalid work/index");
    f.CountTime(static_cast<Work>(-1),1,true);
    Require(f.Stats().actions==actions && f.Stats().pointerMovingSeconds==0 &&
        f.Inspect(-1).trend==BedTrend::NoSample && f.Reset(false) &&
        f.Inspect(farBed).trend==BedTrend::NoSample,"invalid input, reset or absent bed");

    FarmGrid equal; Require(equal.Initialize(3,1),"equal donors");
    for(int i:{0,2}) {
        auto t=*equal.GetTile(i); t.feature=FarmTileFeature::Canal; t.waterAmount=1;
        Require(equal.SetTile(i,t),"wet donor");
    }
    auto t=*equal.GetTile(1); t.state=FarmTileState::Tilled; Require(equal.SetTile(1,t),"receiving soil");
    water.Initialize(); water.UpdateWater(equal,tick,1);
    const auto eq=water.GetLastTileFlows(equal);
    Require(eq.size()==3 && eq[1].soilDonor==0 && eq[0].soilSent>0 && eq[2].soilSent==0,"equal donor selection");
    std::cout<<"symmetric retained donors sent "<<eq[0].soilSent<<','<<eq[2].soilSent<<" in one tick\n";
    FarmRules stress{}; stress.irrigationMoistureRecoveryPerSecond=100;
    FarmGrid cross; Require(cross.Initialize(3,3),"cross fixture");
    for(int i:{0,2,6,8,4}) {auto s=*cross.GetTile(i);s.state=FarmTileState::Tilled;Require(cross.SetTile(i,s),"cross soil");}
    for(int i:{1,3,5,7}) {auto s=*cross.GetTile(i);s.feature=FarmTileFeature::Canal;s.waterAmount=.07f;Require(cross.SetTile(i,s),"cross donor");}
    water.Initialize(stress,SoilIntake::LocalContact); water.UpdateWater(cross,tick,4);
    auto actual=water.GetLastTileFlows(cross);
    Require(actual[4].soilDonor==-2 && actual[4].soilReceived>0 && actual[4].soilReceived<=.96f,"four-way soil cap");
    for(int i:{1,3,5,7}) Require(std::abs(actual[i].soilSent-.07f)<1.e-6f && cross.GetTile(i)->waterAmount>=0,"shared overdraft");
    Require(std::abs(water.GetLastStep(cross).balanceError)<1.e-6,"cross conservation");
    System local; Require(local.Reset(true,SoilIntake::LocalContact),"local gate setup");
    for(int i:{gate,13,12,7,fork}) Require(local.Act(i,Work::Dig),"local gate ditch");
    Require(local.Act(nearBed,Work::Hoe)&&local.Act(nearBed,Work::Seed),"local gate crop");
    for(int i=0;i<600;++i) local.Step(tick);
    float before=local.Inspect(nearBed).inRate;
    Require(local.Act(gate,Work::Dig),"local gate closure");local.Step(tick);
    float after=local.Inspect(nearBed).inRate;
    Require(after>0 && after<=before+1.e-5f && local.Flows()[gate].sourceRefill==0,"closing increased local seep");
    std::cout<<"local gate before/after "<<before<<'/'<<after<<" U/s\n";
    const auto sameContact=[](bool open) {
        FarmGrid g; Require(g.Initialize(3,1),"matched gate layout");
        auto source=*g.GetTile(0);
        if(open) {source.feature=FarmTileFeature::WaterSource;source.waterAmount=1;}
        auto canal=*g.GetTile(1);canal.feature=FarmTileFeature::Canal;canal.waterAmount=1;
        auto bed=*g.GetTile(2);bed.state=FarmTileState::Tilled;bed.moisture=.40f;
        Require(g.SetTile(0,source) && g.SetTile(1,canal) && g.SetTile(2,bed),"matched gate setup");
        FarmIrrigationSystem s;s.Initialize({},SoilIntake::LocalContact);s.UpdateWater(g,tick,1);
        Require(s.IsSupplied(1)==open,"matched topology");
        return s.GetLastTileFlows(g)[2].soilReceived;
    };
    const float connected=sameContact(true),retained=sameContact(false);
    Require(connected>0 && connected==retained,"equal stocks with gate open and shut differ");
}
}
