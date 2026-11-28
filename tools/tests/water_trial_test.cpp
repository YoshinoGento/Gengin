#include "farm/lab/WaterTrialFlowView.h"
#include "farm/system/FarmCanalArrival.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include "water_trial_distribution.h"

using namespace farm;
using namespace farm::trial;
namespace {
constexpr float dt=1.f/60;
void Check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
void Plant(System& f,int bed) {
    if(f.Grid().GetTile(bed)->state==FarmTileState::Empty) Check(f.Act(bed,Work::Hoe),"hoe");
    Check(f.Act(bed,Work::Seed),"seed");
}
SurfaceFrame Frame() {
    SurfaceFrame f; f.lipY=.6f;
    for(int i=0;i<width*height;++i) f.centers[i]={float(i%width),.05f,float(i/width)};
    return f;
}
void ArrivalAndClosure() {
    System f; Check(f.Reset(true),"reset");
    Check(!f.Act(-1,Work::Dig) && !f.Act(nearBed,Work::Dig) && !f.Act(gate-width,Work::Dig),"illegal dig");
    Plant(f,nearBed); Plant(f,farBed);
    for(int i:trunk) Check(f.Act(i,Work::Dig),"L");
    f.Step(dt); Check(f.Stats().input==0 && f.Stats().delivered==0,"closed leaked");
    Check(f.Act(gate,Work::Dig),"open");
    arrival::Timeline legacy;
    FlowView view;
    auto frame=Frame();
    int first=-1,full=-1,donor=-1;
    size_t extra=0;
    for(int tick=0;tick<180;++tick) {
        const auto previous=f.Water().MeasurementRevision();
        f.Step(dt); const auto flows=f.Flows();
        legacy.Advance(dt); legacy.Observe(f.Grid(),f.Water(),flows,previous);
        if(flows[nearBed].soilReceived>0 && first<0) {
            first=tick; donor=flows[nearBed].soilDonor;
            Check(donor>=0 && (f.Grid().GetTile(donor)->waterAmount>0 || flows[donor].soilSent>0),"invisible donor");
        }
        if(first>=0 && full<0 && legacy.Surface(donor).fraction>=1.f) full=tick;
        Check(flows[farBed].soilReceived==0,"far bed reached without fork");
        view.Step(dt,flows[gate].sourceRefill);
        const auto a=view.Build(f,frame,Style::Surface).size(), b=view.Build(f,frame,Style::Fall).size();
        const auto c=view.Build(f,frame,Style::Spray);
        Check(a<=b && b<=c.size() && c.size()<=360,"vertex budget");
        for(const auto& v:c) Check(rain::Finite(v.position),"nonfinite vertex");
        if(tick==6) Check(b>a,"missing falling sheet");
        extra=(std::max)(extra,c.size()-b);
    }
    Check(first>=0 && full>first && extra>0,"arrival or missing spray");
    Check(f.Act(fork,Work::Dig),"fork"); bool farWet=false,split=false;
    Check(f.Water().GetDownstreamCanalCount(trunk[1])==2,"not a hydraulic branch");
    for(int i=0;i<90;++i) {
        f.Step(dt); farWet|=f.Flows()[farBed].soilReceived>0;
        split|=f.Flows()[trunk.back()].incoming>0 && f.Flows()[fork].incoming>0;
        view.Step(dt,f.Flows()[gate].sourceRefill);
    }
    Check(farWet && split,"fork failed to supply both branches");
    const double before=f.Stats().input, soil=f.Stats().delivered;
    Check(f.Act(gate,Work::Dig),"close");
    bool airborne=false;
    for(int i=0;i<100;++i) {
        f.Step(dt); Check(f.Flows()[gate].sourceRefill==0 && f.Stats().input==before,"closed inlet emitted");
        view.Step(dt,0);
        airborne|=view.Build(f,frame,Style::Spray).size()>view.Build(f,frame,Style::Surface).size();
    }
    Check(airborne && f.Stats().delivered>soil,"drained downstream or in-flight effects");
    Check(view.Build(f,frame,Style::Spray).size()==view.Build(f,frame,Style::Surface).size(),"permanent spray");
    Check(f.Act(gate,Work::Dig),"reopen"); f.Step(dt); Check(f.Flows()[gate].sourceRefill>0,"reopen dry");
    Check(f.Stats().worstBalance<1.e-5,"balance");
    std::cout<<"near first delivery (s, end of tick)="<<(first+1)*dt<<" legacy donor fully visible="<<(full+1)*dt
        <<" gap="<<(full-first)*dt<<" max spray vertices="<<extra<<" max residual="<<f.Stats().worstBalance<<'\n';
}
struct HarvestRun { std::array<unsigned,3> hands{}; double stock=0,input=0,elapsed=0; };
void LocalViewCases() {
    const auto frame=Frame();
    for(bool enhanced:{false,true}) {
        System f,control; FlowView view;
        for(auto* s:{&f,&control}) {
            Check(s->Reset(true,SoilIntake::LocalContact),"local reset");
            for(int i:trunk) Check(s->Act(i,Work::Dig),"local trunk");
            Check(s->Act(fork,Work::Dig) && s->Act(gate,Work::Dig),"local fork");
            Plant(*s,nearBed); Plant(*s,farBed);
        }
        double cpu=0,visual=0; bool observedFlow=false,observedHit=false;
        for(int i=0;i<1800;++i) {
            if(i==1500) for(auto* s:{&f,&control}) Check(s->Act(gate,Work::Dig),"close local");
            const auto t=std::chrono::steady_clock::now();
            f.Step(dt);
            const auto t1=std::chrono::steady_clock::now();
            view.Step(dt,f.Flows()[gate].sourceRefill); view.Observe(f);
            const auto verts=view.Build(f,frame,Style::Spray,enhanced);
            const auto t2=std::chrono::steady_clock::now();
            cpu+=std::chrono::duration<double,std::micro>(t1-t).count();
            visual+=std::chrono::duration<double,std::micro>(t2-t1).count();
            Check(verts.size()<=420,"local visual geometry limit");
            for(const auto& v:verts) Check(rain::Finite(v.position),"local surface nonfinite");
            if(enhanced && i>1650) Check(verts.empty(),"stale fall or impact after closure");
            observedFlow|=view.Samples()[gate].x<0;
            observedHit|=i<1500 && !verts.empty();
            control.Step(dt);
            if(IsHarvestReady(*f.Grid().GetTile(nearBed))) {
                for(auto* s:{&f,&control}) {Check(s->Act(nearBed,Work::Harvest),"local visual harvest");Plant(*s,nearBed);}
                Check(f.LastQuality()==control.LastQuality(),"local visual changed quality");
            }
            for(int j=0;j<width*height;++j) {
                const auto& a=*f.Grid().GetTile(j);const auto& b=*control.Grid().GetTile(j);
                Check(a.waterAmount==b.waterAmount && a.moisture==b.moisture && a.growth==b.growth,"visual changed field");
            }
        }
        Check(observedFlow && observedHit,"missing local flow or cascade");
        std::cout<<"trial_cpu enhanced="<<enhanced<<" n=1800 mean_sim_us="<<cpu/1800<<" mean_view_us="<<visual/1800<<'\n';
    }
}

void RouteReplay(bool alternate,CanalTransport transport=CanalTransport::ParentTree) {
    System shown,control;
    FlowView view;view.Observe(shown);
    for(auto* s:{&shown,&control}) {
        Check(s->Reset(true,SoilIntake::LocalContact,SoilPolicy::HarvestCost,transport)&&s->BeginReplay(),"route replay setup");
        if(alternate) for(int i:{6,11,8}) Check(s->Act(i,Work::Dig),"alternate network");
    }
    const auto frame=Frame();
    bool turned=false,branched=false,after=false;
    for(int tick=0;tick<720;++tick) {
        for(auto* s:{&shown,&control}) {
            Check(s->ApplyReplayTick(tick),"replay work");
            if(s==&shown && s->StepSeconds()==0) {
                view.Observe(*s);
                for(auto p:view.Ports()) Check(p.x==0 && p.y==0 && p.z==0 && p.w==0,"stale ports after excavation");
            }
            s->Step(dt);
        }
        view.Step(dt,shown.Flows()[gate].sourceRefill);view.Observe(shown);
        auto ports=view.Ports();const auto verts=view.Build(shown,frame,Style::Spray,true,true,true);
        Check(verts.size()<=372,"routed capacity");
        for(const auto& v:verts) Check(rain::Finite(v.position),"invalid routed geometry");
        for(int i=0;i<width*height;++i) {
            const auto& p=ports[i];const auto& a=*shown.Grid().GetTile(i);const auto& b=*control.Grid().GetTile(i);
            Check(a.waterAmount==b.waterAmount && a.moisture==b.moisture && a.growth==b.growth,"routed view mutated field");
            if(i%width+1<width) Check(p.y==-ports[i+1].x,"east face not shared");
            if(i/width+1<height) Check(p.w==-ports[i+width].z,"south face not shared");
            if(a.feature==FarmTileFeature::None) Check(p.x==0&&p.y==0&&p.z==0&&p.w==0,"dry land emitted a flow");
            float out=(std::max)(0.f,p.x)+(std::max)(0.f,p.y)+(std::max)(0.f,p.z)+(std::max)(0.f,p.w);
            Check(std::abs(out*dt-shown.Flows()[i].outgoing)<1.e-6f,"false outlet or unaccounted transfer");
        }
        if(tick<60 || tick>=480) Check(ports[gate].y==0,"closed bank displayed new inflow");
        if(tick<240) Check(ports[12].w==0,"unexcavated branch");
        for(auto p:ports) {
            turned|=((p.x<0 || p.y<0) && (p.z>0 || p.w>0)) || ((p.z<0 || p.w<0) && (p.x>0 || p.y>0));
            branched|=(int(p.x>0)+int(p.y>0)+int(p.z>0)+int(p.w>0))>=2;
        }
        after|=tick>480 && !verts.empty();
        if(tick>540) Check(verts.empty(),"stale splash");
    }
    std::cout<<"replay alternate="<<alternate<<" turn="<<turned<<" split="<<branched<<" residual_spray="<<after<<'\n';
    Check(turned&&branched,"missing real turn or split");
    // An already full inlet need not have anything airborne when the player closes it.
}
void HarvestCostComparison() {
    for(auto transport:{CanalTransport::ParentTree,CanalTransport::LocalEdges})
    for(bool detour:{false,true}) for(auto policy:{SoilPolicy::HarvestCost,SoilPolicy::NoCost,SoilPolicy::PassiveDrain}) {
        const bool cost=policy==SoilPolicy::HarvestCost;
        System f;Check(f.Reset(true,SoilIntake::LocalContact,policy,transport),"harvest policy reset");
        if(detour) {for(int i:{13,12,11,6,1}) Check(f.Act(i,Work::Dig),"long ditch");}
        else {for(int i:trunk) Check(f.Act(i,Work::Dig),"short ditch");Check(f.Act(fork,Work::Dig),"fork");}
        Check(f.Act(gate,Work::Dig),"gate");Plant(f,nearBed);Plant(f,farBed);
        std::array<int,2> counts{},quality{}; std::array<double,2> times{},excess{};
        double consumed=0;
        for(int i=0;i<7200 && f.CompletedCrops()<3;++i) {
            f.Step(dt);
            for(int bed:{nearBed,farBed}) consumed+=f.Inspect(bed).usedRate*f.StepSeconds();
            for(int j=0;j<2;++j) {
                int bed=j?farBed:nearBed;
                if(counts[j]>=3 || !IsHarvestReady(*f.Grid().GetTile(bed))) continue;
                Check(f.Act(bed,Work::Harvest),"policy harvest");
                quality[j]=f.LastQuality();times[j]=f.Stats().elapsed;excess[j]=f.LastPick().excess;
                if(++counts[j]<3) Plant(f,bed);
            }
        }
        Check(f.CompletedCrops()==3,"three harvests within trial horizon");
        Check(cost?(f.HarvestMoistureRemoved()>0 && f.HarvestMoistureRemoved()<=1.50001):
            f.HarvestMoistureRemoved()==0,"trial policy loss");
        if(cost && transport==CanalTransport::ParentTree) Check(quality==(detour?std::array{68,69}:std::array{68,68}),"baseline changed");
        unsigned actions=0;for(auto n:f.Stats().actions) actions+=n;
        const double residual=.5+f.Stats().delivered-consumed-f.HarvestMoistureRemoved()-f.Stats().drained
            -f.Grid().GetTile(nearBed)->moisture-f.Grid().GetTile(farBed)->moisture;
        Check(std::abs(residual)<1.e-4,"soil ledger including passive drainage");
        Check((policy==SoilPolicy::PassiveDrain || f.Stats().drained==0) && f.Stats().worstBalance<1.e-5,"policy accounting");
        std::cout<<"harvest_case edges="<<(transport==CanalTransport::LocalEdges)<<" detour="<<detour<<" policy="<<int(policy)
            <<" actions="<<actions<<" drained="<<f.Stats().drained<<" soil_error="<<residual<<" third_q="<<quality[0]<<','<<quality[1]
            <<" third_s="<<times[0]<<','<<times[1]<<" excess_s="<<excess[0]<<','<<excess[1]
            <<" total_soil="<<f.Stats().delivered<<" loss="<<f.HarvestMoistureRemoved()<<'\n';
    }
}
void RingTransport() {
    for(int scale:{1,4}) {
        FarmGrid g;Check(g.Initialize(5,4),"ring grid");
        for(int i:{2,1,3,6,8,11,13,12}) {
            auto t=*g.GetTile(i);t.feature=i==2?FarmTileFeature::WaterSource:FarmTileFeature::Canal;
            Check(g.SetTile(i,t),"ring geometry");
        }
        FarmIrrigationSystem w;w.Initialize({},SoilIntake::LocalContact,CanalTransport::LocalEdges);
        double left=0,right=0,worst=0,range=1;
        for(int tick=0;tick<1200/scale;++tick) {
            if(tick==240/scale) {
                auto source=*g.GetTile(2);source.feature=FarmTileFeature::Canal;
                Check(g.SetTile(2,source),"remove refill");
            }
            std::array<float,20> before{};
            for(int i=0;i<20;++i) before[i]=g.GetTile(i)->waterAmount;
            w.UpdateWater(g,dt,float(scale));
            const auto f=w.GetLastTileFlows(g);const auto s=w.GetLastStep(g);
            Check(f.size()==20 && s.valid,"missing ring measurements");
            worst=(std::max)(worst,std::abs(s.balanceError));
            if(tick<240/scale) {left+=f[11].outward[1];right+=f[13].outward[0];}
            else Check(s.sourceRefill==0,"closed source emitted");
            double lo=1,hi=0;
            for(int i=0;i<20;++i) {
                const float stock=g.GetTile(i)->waterAmount;
                Check(std::isfinite(stock) && stock>=0 && stock<=1,"ring bounds");
                Check(f[i].outgoing<=before[i]+f[i].sourceRefill+1.e-6 &&
                    f[i].outgoing<=FarmRules{}.irrigationWaterTransferPerSecond*dt*scale+1.e-6 &&
                    f[i].incoming<=1.-before[i]-f[i].sourceRefill+1.e-6,"edge cap");
                Check(std::abs(stock-g.GetTile(i/5*5+4-i%5)->waterAmount)<2.e-6f,"ring mirror bias");
                if(g.GetTile(i)->feature!=FarmTileFeature::None){lo=(std::min)(lo,double(stock));hi=(std::max)(hi,double(stock));}
            }
            if(tick>240/scale) Check(hi-lo<=range+1.e-6,"growing free-surface oscillation");
            range=hi-lo;
        }
        Check(worst<1.e-6 && left>0 && std::abs(left-right)<1.e-6,"diamond inflow");
        auto source=*g.GetTile(2);source.feature=FarmTileFeature::WaterSource;
        Check(g.SetTile(2,source),"restore ring inlet");
        FarmToolActionSystem tool;tool.Initialize();
        Check(tool.DigCanalAt(g,7,true) && tool.Undo(),"undo excavation");
        w.UpdateWater(g,dt,1); Check(g.GetTile(7)->feature==FarmTileFeature::None &&
            w.GetLastTileFlows(g)[7].incoming==0,"undo left edge");
        Check(tool.Redo(),"redo excavation");w.UpdateWater(g,dt,1);
        Check(w.GetLastTileFlows(g)[7].incoming>0,"redo did not reconnect");
        std::cout<<"ring edges scale="<<scale<<" 4s left/right="<<left<<'/'<<right<<" final_range="<<range<<" max_error="<<worst<<'\n';
    }
}
void ReflectedNetwork() {
    FarmGrid a,b;Check(a.Initialize(5,4)&&b.Initialize(5,4),"network grids");
    const auto flip=[](int i){return (3-i/5)*5+4-i%5;};
    for(int i=0;i<20;++i) {
        auto t=*a.GetTile(i);
        t.feature=(i==0 || i==14)?FarmTileFeature::WaterSource:
            (i==6 || i==16)?FarmTileFeature::None:FarmTileFeature::Canal;
        t.heightLevel=i<5?1:0;
        t.waterAmount=t.feature==FarmTileFeature::None?0:float(i%7)/8;
        Check(a.SetTile(i,t)&&b.SetTile(flip(i),t),"reflected setup");
    }
    FarmIrrigationSystem first,second;
    for(auto* w:{&first,&second}) w->Initialize({},SoilIntake::LocalContact,CanalTransport::LocalEdges);
    for(int tick=0;tick<600;++tick) {
        first.UpdateWater(a,dt,1); second.UpdateWater(b,dt,1);
        const auto fa=first.GetLastTileFlows(a),fb=second.GetLastTileFlows(b);
        Check(fa.size()==20 && fb.size()==20,"reflected measurements");
        for(int i=0;i<20;++i) {
            Check(std::abs(a.GetTile(i)->waterAmount-b.GetTile(flip(i))->waterAmount)<2.e-6f,"iteration order stock");
            for(int d=0;d<4;++d) Check(std::abs(fa[i].outward[d]-fb[flip(i)].outward[d^1])<2.e-6f,"iteration order face");
            if(i<5) Check(fa[i].outward[3]>=0,"uphill flow");
        }
    }
}
void BenchRouteCPU() {
    const auto frame=Frame();
    for(auto transport:{CanalTransport::ParentTree,CanalTransport::LocalEdges})
    for(bool dense:{false,true}) for(int run=0;run<3;++run) for(bool routed:{false,true}) {
        System f;FlowView v;
        Check(f.Reset(true,SoilIntake::LocalContact,SoilPolicy::HarvestCost,transport)&&f.BeginReplay(),"cpu layout");
        for(int j:{fork,gate}) Check(f.Act(j,Work::Dig),"cpu open");
        if(dense) for(int j:denseExtension) Check(f.Act(j,Work::Dig),"dense dig");
        double sim=0,visual=0;size_t emitted=0;
        for(int i=0;i<720;++i) {
            auto a=std::chrono::steady_clock::now();f.Step(dt);
            auto b=std::chrono::steady_clock::now();v.Step(dt,f.Flows()[gate].sourceRefill);v.Observe(f);
            auto mesh=v.Build(f,frame,Style::Spray,true,routed,routed);
            auto c=std::chrono::steady_clock::now();
            if(i>=120) {
                sim+=std::chrono::duration<double,std::micro>(b-a).count();
                visual+=std::chrono::duration<double,std::micro>(c-b).count();
                emitted+=mesh.size();
            }
        }
        std::cout<<"route_cpu edges="<<(transport==CanalTransport::LocalEdges)<<" dense="<<dense<<" run="<<run+1<<" routed="<<routed<<" sim_us="<<sim/600
            <<" view_us="<<visual/600<<" vertices="<<emitted<<'\n';
    }
}
void BenchLargerGrids() {
    for(int side:{32,128}) for(int run=0;run<3;++run) {
        FarmGrid g;Check(g.Initialize(side,side),"scale grid");
        for(int i=0;i<side*side;++i) {
            auto t=*g.GetTile(i);t.feature=i==0?FarmTileFeature::WaterSource:FarmTileFeature::Canal;
            t.waterAmount=.5f;
            Check(g.SetTile(i,t),"scale channel");
        }
        FarmIrrigationSystem w;w.Initialize({},SoilIntake::LocalContact,CanalTransport::LocalEdges);
        constexpr int warm=30, samples=120;
        double us=0,worst=0;
        for(int tick=0;tick<warm+samples;++tick) {
            const auto start=std::chrono::steady_clock::now();
            w.UpdateWater(g,dt,1);
            const auto stop=std::chrono::steady_clock::now();
            const auto s=w.GetLastStep(g); Check(s.valid,"scale measurement");
            if(tick>=warm) us+=std::chrono::duration<double,std::micro>(stop-start).count();
            worst=(std::max)(worst,std::abs(s.balanceError));
        }
        std::cout<<"synthetic_transport side="<<side<<" run="<<run+1<<" mean_us="<<us/samples<<" max_error="<<worst<<'\n';
    }
}
HarvestRun Run(bool both,bool ditch,Style style,bool report,bool singleTrunk=false) {
    System f; Check(f.Reset(both),"farm"); FlowView view; const auto frame=Frame();
    if(ditch) {
        Check(f.Act(gate,Work::Dig),"open");
        if(singleTrunk) {
            for(int i:{13,12,11,6,1}) Check(f.Act(i,Work::Dig),"detour");
        } else {
            for(int i:trunk) Check(f.Act(i,Work::Dig),"dig");
            if(both) Check(f.Act(fork,Work::Dig),"branch");
        }
    }
    const int beds=both?2:1; const std::array order{nearBed,farBed};
    for(int i=0;i<beds;++i) Plant(f,order[i]);
    HarvestRun result;
    std::array<int,2> counts{};
    unsigned ticks=0; const auto start=std::chrono::steady_clock::now();
    for(;ticks<18000 && f.CompletedCrops()<3;++ticks) {
        for(int j=0;j<beds;++j) {
            const auto* tile=f.Grid().GetTile(order[j]);
            if(!ditch && counts[j]<3 && ticks%120==0 && tile->moisture<.4f)
                Check(f.Act(order[j],Work::Water),"can");
        }
        f.Step(dt); view.Step(dt,f.Flows()[gate].sourceRefill);
        Check(view.Build(f,frame,style).size()<=360,"size");
        const int before=f.CompletedCrops();
        for(int j=0;j<beds;++j) if(counts[j]<3 && IsHarvestReady(*f.Grid().GetTile(order[j]))) {
            Check(f.Act(order[j],Work::Harvest),"harvest"); ++counts[j];
            if(report && both && ditch && counts[j]==3) {
                const auto& p=f.LastPick();
                std::cout<<"bed="<<order[j]<<" third harvest_s="<<f.Stats().elapsed<<" quality="<<f.LastQuality()
                    <<" dry/low/good/excess_s="<<p.dry<<'/'<<p.low<<'/'<<p.good<<'/'<<p.excess<<'\n';
            }
            if(counts[j]<3) Plant(f,order[j]);
        }
        if(f.CompletedCrops()>before) result.hands[before]=f.Stats().actions[0]+f.Stats().actions[3];
    }
    const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
    Check(f.CompletedCrops()==3 && f.Stats().worstBalance<1.e-5,"three crops or balance");
    for(int i=0;i<width*height;++i) result.stock+=f.Grid().GetTile(i)->waterAmount;
    result.input=f.Stats().input; result.elapsed=f.Stats().elapsed;
    Check(std::abs(result.input-result.stock-f.Stats().delivered)<.001,"run stock balance");
    if(report) std::cout<<"beds="<<beds<<" ditch="<<ditch<<" detour="<<singleTrunk<<" dig+can cumulative="<<result.hands[0]<<','<<result.hands[1]<<','<<result.hands[2]
        <<" sim_s="<<result.elapsed<<" last_quality="<<f.LastQuality()<<" inflow="<<result.input<<" soil="<<f.Stats().delivered
        <<" stock="<<result.stock<<" max_balance_error="<<f.Stats().worstBalance<<" loop_us/tick="<<us/ticks<<'\n';
    return result;
}
}
int main() try {
    distribution::Cases();
    ArrivalAndClosure();
    LocalViewCases();
    RouteReplay(false);RouteReplay(true);
    RouteReplay(false,CanalTransport::LocalEdges);RouteReplay(true,CanalTransport::LocalEdges);
    RingTransport();ReflectedNetwork();HarvestCostComparison();
    BenchRouteCPU();BenchLargerGrids();
    for(bool both:{false,true}) for(bool ditch:{false,true}) {
        const auto a=Run(both,ditch,Style::Surface,true);
        for(auto style:{Style::Fall,Style::Spray}) {
            const auto b=Run(both,ditch,style,false);
            Check(a.stock==b.stock && a.input==b.input && a.hands==b.hands && a.elapsed==b.elapsed,"render mode affected simulation");
        }
    }
    (void)Run(true,true,Style::Spray,true,true);
    std::cout<<"PASS water trial\n";
} catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n'; return 1;}
