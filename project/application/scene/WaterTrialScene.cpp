#include "WaterTrialScene.h"
#include "farm/lab/TrialWaterPass.h"
#include "farm/system/FarmOverviewCamera.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "3d/Object3dCommon.h"
#include "2d/SpriteCommon.h"
#include "io/Input.h"
#include <filesystem>
#include <fstream>
#include <chrono>
#include <format>

using namespace farm::trial;
namespace {
constexpr std::array names{"Ditch","Hoe","Carrot seed","Can","Basket"};
constexpr std::array styles{"A  Surface","B  Cascade","C  Spray"};
constexpr std::array workKeys{DIK_1,DIK_2,DIK_3,DIK_4,DIK_5};
constexpr float tabX=24,tabY=92,tabWidth=82,tabHeight=34,tabPitch=90;
}
bool WaterTrialScene::Reset(bool both) {
    replayTick_=-1;captureLeadIn_=0;
    if(!field_.Reset(both,localIntake_?farm::SoilIntake::LocalContact:farm::SoilIntake::PathStrength,soil_,transport_)) return false;
    flowView_={}; paused_=false; tool_=Work::Dig;
    run_=std::chrono::system_clock::now().time_since_epoch().count();
    Record("start");
    return true;
}
void WaterTrialScene::Initialize() { (void)InitializeIncrementally().Complete(); }
InitializationTask WaterTrialScene::InitializeIncrementally() {
    auto* app=Framework::GetInstance();
    farm::FarmVisualLayout layout;
    layout.pondLipRise=.46f;
    visual_.Initialize(layout);
    auto white=TextureManager::GetInstance()->LoadTexture2D("Resources/farm/white.png");
    if(!white.IsValid() || !renderer_.Initialize(app->GetObject3dCommon(),app->GetModelManager(),white)) co_return false;
    co_await std::suspend_always{};
    if(!effects_.Initialize(app->GetDxCommon()) ||
        !font_.InitializeFromJson(app->GetSpriteCommon(),"Resources/ui/font/ascii_bitmap_font.json")) co_return false;
    for(auto& label:labels_) {
        label.Initialize(app->GetSpriteCommon(),&font_); label.SetScale(.65f); label.SetCharacterSpacing(-4);
    }
    labels_[0].SetPosition({24,18}); labels_[1].SetPosition({24,655}); labels_[2].SetPosition({24,685});
    labels_[1].SetText("1 Ditch   2 Hoe   3 Seed   4 Can   5 Basket     Click / Space to use");
    labels_[2].SetText("F1-F3 Style   G Look   L Intake/reset   P Pause   F8 Replay   R Reset");
    for(size_t i=3;i<labels_.size();++i) { labels_[i].SetColor({1,.9f,.56f,1}); labels_[i].SetScale(.55f); }
    for(size_t i=0;i<inspectionLines_.size();++i) {
        auto& label=inspectionLines_[i]; label.Initialize(app->GetSpriteCommon(),&font_);
        label.SetScale(.68f); label.SetCharacterSpacing(-8);
        label.SetPosition({24,156.f+24.f*(i%3)+97.f*(i/3)});
        if(i%3==0) label.SetColor({1,.90f,.59f,1});
    }
    for(size_t i=0;i<tabs_.size();++i) {
        const float x=tabX+i*tabPitch;
        if(!tabs_[i].Initialize(app->GetSpriteCommon(),white.Index())) co_return false;
        tabs_[i].SetSize({tabWidth,tabHeight}); tabs_[i].SetPosition({x,tabY});
        auto& label=tabLabels_[i]; label.Initialize(app->GetSpriteCommon(),&font_);
        label.SetScale(.65f); label.SetCharacterSpacing(-6); label.SetPosition({x+10,tabY+7});
        if(i==3) { tabs_[i].SetSize({132,tabHeight}); label.SetText("I  Soil"); }
        else label.SetText(std::format("F{} {}",i+1,char('A'+i)));
        label.Update();
    }
    wchar_t ref[2]{};
    reference_=GetEnvironmentVariableW(L"SUIRO_WATER_TRIAL_COMPARE",ref,2)==1 && ref[0]>=L'A' && ref[0]<=L'C';
    const auto flag=[](const wchar_t* name) {wchar_t value[2]{};return GetEnvironmentVariableW(name,value,2)==1 && value[0]==L'1';};
    waterPass_=CreateWaterPass(*app->GetObject3dCommon());
    const bool activeOnly=flag(L"SUIRO_WATER_TRIAL_ACTIVE_FACES"), noLanes=flag(L"SUIRO_WATER_TRIAL_NO_LANES");
    velocityShape_=!flag(L"SUIRO_WATER_TRIAL_OLD_SPLASH");
    routedPass_=CreateWaterPass(*app->GetObject3dCommon(),noLanes?WaterShader::NoLanes:activeOnly?WaterShader::ActiveFaces:WaterShader::Routed);
    if(!waterPass_ || !routedPass_) co_return false;
    enhanced_=!flag(L"SUIRO_WATER_TRIAL_CLASSIC"); localIntake_=!flag(L"SUIRO_WATER_TRIAL_LEGACY");
    routed_=enhanced_ && !flag(L"SUIRO_WATER_TRIAL_PREVIOUS");
    soil_=flag(L"SUIRO_WATER_TRIAL_DRAIN")?farm::trial::SoilPolicy::PassiveDrain:
        flag(L"SUIRO_WATER_TRIAL_NO_HARVEST_COST")?farm::trial::SoilPolicy::NoCost:farm::trial::SoilPolicy::HarvestCost;
    transport_=flag(L"SUIRO_WATER_TRIAL_TREE")?farm::CanalTransport::ParentTree:farm::CanalTransport::LocalEdges;
    if(reference_ && flag(L"SUIRO_WATER_TRIAL_PROFILE"))
        probe_.Initialize(*app->GetDxCommon(),routed_?(noLanes?"nolanes":activeOnly?"active":"routed"):enhanced_?"shaded":"classic");
    ready_=Reset(reference_);
    if(ready_ && reference_) {
        style_=static_cast<Style>(ref[0]-L'A');
        for(int bed:{nearBed,farBed}) { field_.Act(bed,Work::Hoe); field_.Act(bed,Work::Seed); }
        for(int i:trunk) field_.Act(i,Work::Dig);
        field_.Act(fork,Work::Dig); field_.Act(gate,Work::Dig);
        const bool dense=flag(L"SUIRO_WATER_TRIAL_DENSE");
        if(dense) for(int i:denseExtension) if(!field_.Act(i,Work::Dig)) co_return false;
        wchar_t inspect[2]{};
        inspecting_=GetEnvironmentVariableW(L"SUIRO_WATER_TRIAL_INSPECT",inspect,2)==1 && inspect[0]==L'1';
        const int referenceTicks=inspecting_?600:dense?240:48;
        for(int i=0;i<referenceTicks;++i) {
            field_.Step(1.f/60); flowView_.Step(field_.StepSeconds(),field_.Flows()[gate].sourceRefill);
            flowView_.Observe(field_);
        }
        paused_=true;
    }
    if(ready_ && flag(L"SUIRO_WATER_TRIAL_REPLAY")) {
        reference_=true;ready_=Reset(true) && field_.BeginReplay();
        style_=Style::Spray;replayTick_=0;inspecting_=false;
        captureLeadIn_=120;
    }
    co_return ready_;
}
void WaterTrialScene::Record(const char* event) {
    if(reference_) return;
    std::error_code error;
    auto path=std::filesystem::temp_directory_path(error);
    if(error) { logOk_=false; return; }
    path/="SuiroWaterTrial";
    std::filesystem::create_directories(path,error);
    if(error) { logOk_=false; return; }
    std::ofstream out(path/(std::to_string(run_)+".csv"),std::ios::app);
    const auto& s=field_.Stats();
    if(out.tellp()==0) out<<"event,beds,sim_s,crops,harvests,last_quality,selections,digs,hoes,seeds,cans,picks,dig_tool_s,hoe_tool_s,seed_tool_s,can_tool_s,basket_tool_s,inflow,soil_delivery,max_balance_error,pointer_moving_s,work_while_other_bed_fed,tile,last_pick_moisture,last_dry_s,last_low_s,last_good_s,last_excess_s,harvest_moisture_removed\n";
    out<<event<<','<<(field_.Both()?2:1)<<','<<s.elapsed<<','<<field_.CompletedCrops()<<','<<s.harvests
        <<','<<field_.LastQuality()<<','<<s.cursorSteps;
    for(auto x:s.actions) out<<','<<x;
    for(auto x:s.toolSeconds) out<<','<<x;
    const auto& p=field_.LastPick();
    out<<','<<s.input<<','<<s.delivered<<','<<s.worstBalance<<','<<s.pointerMovingSeconds<<','<<s.workWhileOtherBedFed<<','<<field_.Grid().GetSelectedIndex()
       <<','<<p.moisture<<','<<p.dry<<','<<p.low<<','<<p.good<<','<<p.excess<<','<<field_.HarvestMoistureRemoved()<<'\n';
    out.close(); logOk_=static_cast<bool>(out);
}
void WaterTrialScene::Finalize() { if(ready_) Record("end"); ready_=false; }
void WaterTrialScene::FixedUpdate(float dt) {
    auto* window=Framework::GetInstance()->GetWinApp();
    if(!ready_ || paused_ || (replayTick_<0 && GetForegroundWindow()!=window->GetHwnd())) return;
    if(replayTick_>=0) {
        // The external recorder waits for a complete frozen-frame sample before attaching.
        if(captureLeadIn_>0) {if(!probe_.Sampling()) --captureLeadIn_;return;}
        if(replayTick_>=720 || !field_.ApplyReplayTick(replayTick_++)) {replayTick_=-1;paused_=true;return;}
    }
    field_.Step(dt); flowView_.Step(field_.StepSeconds(),field_.Flows()[gate].sourceRefill);
    flowView_.Observe(field_);
}
void WaterTrialScene::Fit(float aspect) {
    if(aspect_==aspect) return;
    aspect_=aspect;
    camera_.SetAspectRatio(aspect); camera_.Update();
    const auto& l=visual_.GetLayout();
    const float halfPitch=(l.tileGap+l.tileSize)*.5f;
    const auto& m=camera_.GetProjectionMatrix();
    farm::OverviewPose pose;
    if(farm::FitOverviewCamera({l.center.x-halfPitch*width,l.center.y,l.center.z-halfPitch*height},
        {l.center.x+halfPitch*width,l.center.y+FarmToolActionSystem::kMaximumHeightLevel*l.heightStep+.8f,l.center.z+halfPitch*height},
        m.m[0][0],m.m[1][1],camera_.GetNearClip(),camera_.GetFarClip(),farm::kFarmOverviewFrame,pose)) {
        camera_.SetTranslate(pose.position); camera_.SetRotate(pose.rotation); camera_.Update();
    }
}
void WaterTrialScene::Update() {
    if(!ready_) return;
    auto* app=Framework::GetInstance();
    const auto* win=app->GetWinApp(); const auto* input=app->GetInput();
    const int w=win->GetClientWidth(),h=win->GetClientHeight();
    if(w<=0 || h<=0) return;
    Fit(float(w)/h);
    if(GetForegroundWindow()==win->GetHwnd()) {
        if(input->TriggerKey(DIK_G)) {enhanced_=true;routed_=!routed_;}
        if(input->TriggerKey(DIK_F8)) {
            reference_=true;ready_=Reset(true) && field_.BeginReplay();
            if(!ready_) return;
            replayTick_=0;style_=Style::Spray;inspecting_=false;
        }
        const bool changeRule=input->TriggerKey(DIK_L);
        if(changeRule) localIntake_=!localIntake_;
        if(changeRule || input->TriggerKey(DIK_F6) || input->TriggerKey(DIK_R)) {
            Record("reset"); reference_=false; ready_=Reset(input->TriggerKey(DIK_F6)?!field_.Both():field_.Both());
            if(!ready_) return;
        }
        if(input->TriggerKey(DIK_P)) paused_=!paused_;
        if(input->TriggerKey(DIK_I)) inspecting_=!inspecting_;
        for(int i=0;i<3;++i) if(input->TriggerKey(static_cast<BYTE>(DIK_F1+i))) style_=static_cast<Style>(i);
        const auto press=input->ReadLeftDrag();
        bool hudClick=false;
        if(press.began) {
            const float x=1280.f*press.press.x/w,y=720.f*press.press.y/h;
            for(size_t i=0;i<tabs_.size();++i) if(x>=tabX+i*tabPitch && x<tabX+i*tabPitch+(i==3?132:tabWidth) && y>=tabY && y<tabY+tabHeight) {
                if(i==3) inspecting_=!inspecting_; else style_=static_cast<Style>(i);
                hudClick=true;
            }
        }
        if(!paused_ && replayTick_<0) {
            const auto mouse=input->GetMousePosition();
            hover_=-1;
            if(mouse.x>=0 && mouse.x<w && mouse.y>=0 && mouse.y<h) {
                const auto inv=MatrixMath::Inverse(camera_.GetViewProjectionMatrix());
                const float x=2*mouse.x/w-1,y=1-2*mouse.y/h;
                const auto a=MatrixMath::Transform({x,y,0},inv), b=MatrixMath::Transform({x,y,1},inv);
                (void)visual_.TryPickTile(field_.Grid(),a,MatrixMath::Normalize(b-a),hover_);
            }
            for(size_t i=0;i<workKeys.size();++i) if(input->TriggerKey(static_cast<BYTE>(workKeys[i]))) tool_=static_cast<Work>(i);
            const int wheel=input->GetMouseWheelDelta();
            if(wheel) tool_=static_cast<Work>((static_cast<int>(tool_)+(wheel>0?4:1))%5);
            const int cur=field_.Grid().GetSelectedIndex();
            if(input->TriggerKey(DIK_LEFT) && cur%width>0) field_.Select(cur-1);
            else if(input->TriggerKey(DIK_RIGHT) && cur%width<width-1) field_.Select(cur+1);
            else if(input->TriggerKey(DIK_UP)) field_.Select(cur+width);
            else if(input->TriggerKey(DIK_DOWN)) field_.Select(cur-width);
            const auto delta=input->GetMouseDelta();
            field_.CountTime(tool_,app->GetFrameClock()->GetRealDeltaSeconds(),
                cur!=field_.Grid().GetSelectedIndex() || (hover_>=0 && (delta.x!=0 || delta.y!=0)));
            const bool click=!hudClick && input->TriggerMouseButton(InputMouseButton::Left);
            if(click || input->TriggerKey(DIK_SPACE)) {
                if(field_.Act(click?hover_:field_.Grid().GetSelectedIndex(),tool_)) {
                    if(tool_==Work::Dig) flowView_.Observe(field_);
                    constexpr std::array events{"earthwork","till","sow","water","harvest"};
                    Record(events[static_cast<size_t>(tool_)]);
                }
            }
        }
    }
    PrepareView();
}
void WaterTrialScene::PrepareView() {
    const auto& g=field_.Grid(); const auto& l=visual_.GetLayout();
    surface_.pitch=l.tileSize+l.tileGap; surface_.lipY=l.center.y+l.waterBottomOffset+l.waterMaxDepth+l.pondLipRise;
    surface_.sourceDepth=l.waterMaxDepth;
    surface_.openingWidth=2.f*visual_.GetTileVisualData(g,gate).waterHalfExtent;
    for(int i=0;i<g.GetTileCount();++i)
        surface_.centers[i]=visual_.GetTileVisualData(g,i).waterSurfaceCenter;
    renderer_.Prepare(g,visual_,&camera_,hover_,nullptr,true,nullptr,{},nullptr,nullptr,nullptr,nullptr,{},field_.Flows(),
        enhanced_?flowView_.Samples():std::span<const Vector4>{},routed_?flowView_.Ports():std::span<const Vector4>{},flowView_.Clock());
    labels_[0].SetText(std::format("{}  /  {}  /  {}  /  {} crops  {}{}",styles[static_cast<int>(style_)],
        field_.Both()?"Near + Far":"Near",names[static_cast<int>(tool_)],field_.CompletedCrops(),paused_?"PAUSED":"",logOk_?"":" CSV ERROR"));
    const auto inlet=MatrixMath::Transform(visual_.GetTileCenter(g,gate),camera_.GetViewProjectionMatrix());
    labels_[5].SetPosition({(inlet.x+1)*640+40,(1-inlet.y)*360-35});
    labels_[5].SetText(farm::pond::IsOpen(g,gate)?"Intake: open":"Intake: shut");
    labels_[6].SetPosition({24,52});
    labels_[6].SetText(std::format("{} / {}   Ditch {}   Can {}   Harvest {}",localIntake_?"Local":"Legacy",routed_?"Routed":enhanced_?"Water":"Classic",
        field_.Stats().actions[0],field_.Stats().actions[3],field_.Stats().harvests));
    for(int n=0;n<2;++n) {
        const int i=n?farBed:nearBed;
        const auto* tile=g.GetTile(i);
        auto& label=labels_[3+n];
        if(n && !field_.Both()) { label.SetText(""); continue; }
        const auto p=MatrixMath::Transform(visual_.GetTileCenter(g,i),camera_.GetViewProjectionMatrix());
        label.SetPosition({(p.x+1)*640-28,(1-p.y)*360+9});
        label.SetText(std::format("{} {}%",n?"Far":"Near",int(tile->moisture*100)));
    }
    if(inspecting_) PrepareInspector();
    for(auto& label:labels_) label.Update();
    for(size_t i=0;i<tabs_.size();++i) {
        const bool selected=i==3?inspecting_:i==static_cast<size_t>(style_);
        tabs_[i].SetColor(selected?Vector4{.31f,.42f,.26f,.98f}:Vector4{.10f,.18f,.15f,.90f});
        tabs_[i].Update();
    }
}
void WaterTrialScene::PrepareInspector() {
    constexpr std::array trends{"No reading","No growing crop","No delivery","Low, holding","Low, drying","Recovering","Receiving"};
    for(size_t j=0;j<(field_.Both()?2u:1u);++j) {
        const auto r=field_.Inspect(j?farBed:nearBed);
        const std::array key{1+static_cast<int>(r.trend),r.donor,static_cast<int>(std::lround(100*r.donorStock)),
            static_cast<int>(std::lround(10000*r.inRate)),static_cast<int>(std::lround(10000*r.usedRate))};
        if(shown_[j]!=key) {
            shown_[j]=key;
            auto* line=&inspectionLines_[3*j];
            line[0].SetText(std::format("{}: {}",j?"Far":"Near",trends[static_cast<size_t>(r.trend)]));
            line[1].SetText(r.donor==-2?"Several ditches":r.donor<0?"No active donor":std::format("Ditch: {}% full",key[2]));
            line[2].SetText(r.trend==BedTrend::NoSample?"":std::format("In {:.2f}  Use {:.2f} %/s",key[3]*.01,key[4]*.01));
            for(int k=0;k<3;++k) line[k].Update();
        }
    }
}
void WaterTrialScene::Draw() {
    if(!ready_ || aspect_<=0) return;
    auto* app=Framework::GetInstance();
    auto* common=app->GetObject3dCommon();
    const auto vertices=flowView_.Build(field_,surface_,style_,enhanced_,routed_,velocityShape_);
    common->BeginObjectPass(); renderer_.DrawLayer(false);
    auto* dx=app->GetDxCommon();
    const bool profile=probe_.Begin(*dx);
    const size_t waterDraws=renderer_.DrawLayer(true,routed_?routedPass_.Get():enhanced_?waterPass_.Get():nullptr);
    common->EndObjectPass();
    effects_.Draw({}, {},camera_,{},0,0,{},vertices,{});
    if(profile) probe_.End(*dx,waterDraws+(vertices.empty()?0:1));
    app->GetSpriteCommon()->PreDraw();
    for(size_t i=0;i<labels_.size();++i)
        if(inspecting_ || i<3 || i>5) labels_[i].Draw();
    if(inspecting_) for(size_t i=0;i<(field_.Both()?6u:3u);++i) inspectionLines_[i].Draw();
    for(size_t i=0;i<tabs_.size();++i) { tabs_[i].Draw(); tabLabels_[i].Draw(); }
}
