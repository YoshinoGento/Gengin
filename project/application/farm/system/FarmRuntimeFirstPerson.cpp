#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "io/Input.h"

namespace {
constexpr std::array<FarmTool,4> belt{FarmTool::Hoe,FarmTool::Seed,FarmTool::Water,FarmTool::Harvest};
constexpr std::array<farm::CropType,3> seeds{farm::CropType::Carrot,farm::CropType::Tomato,farm::CropType::Pumpkin};
enum Caption { Beyond, Bare, Tilled, Growing, Ripe, Dug, Sown, Watered, Picked, Wait, NoSeeds, WrongTool, FullWater, GripHint, ReleaseHint };
template<class T,size_t N> int Slot(const std::array<T,N>& slots,T value) {
    const auto it=std::find(slots.begin(),slots.end(),value);
    return it==slots.end()?0:static_cast<int>(it-slots.begin());
}
}

bool FarmRuntimeController::UpdateFirstPerson(const FarmRuntimeContext& s,const Input& input) {
    auto* app=Framework::GetInstance();
    const bool available=!open_ && !paused_ && !observation_ && !terrain_ && !walk_.Active() &&
        !s.host.IsTimelineScrubbing() && !flow_.BlocksSimulation() && !lesson_ && !controlGuide_ &&
        !shop_.IsOpen() && !HasBlockingContestNotice() && !s.farmProgressionSystem.IsCleared() &&
        !s.farmCropSelectionSystem.IsOpen() && !s.farmIrrigationPreviewSystem.IsActive();
    const auto pressed=[&](unsigned key){ return s.window && s.window->WindowKeyPressed(key); };
    const auto held=[&](unsigned key){ return int(s.window && s.window->WindowKeyHeld(key)); };
    const bool toggle=pressed(VK_F6) || input.TriggerMouseButton(InputMouseButton::Middle);
    if(fps_.active && (!available || toggle || pressed(VK_ESCAPE))) {
        carrotPull_.Cancel();
        app->SetFieldMouseLook(false);
        fps_.active=false; fps_.target=-1; fpsWaitRelease_=true;
        s.host.SetFieldLens(false);
        s.host.ApplyCameraPose(fps_.saved,false);
        if (!FrameFarm(s,false)) s.host.ApplyCameraPose(fps_.saved,false);
        fieldInputConsumed_=true; BuildView(s);
        return true;
    }
    if(!fps_.active) {
        if(!available || (!fpsAutoEnter_ && !toggle)) return false;
        fps_.Enter(s.farmGrid.GetWidth(),s.farmGrid.GetHeight(),s.host.ReadCameraPose());
        if (!fps_.active) return false;
        s.host.SetFieldLens(true);
        fpsAutoEnter_=false; fpsWaitRelease_=true; fpsNoticeSeconds_=0; fpsFrame_={};
        wheelNotice_.Clear(); hotbarSlot_=-1;
    }
    fieldInputConsumed_=wheelCaptured_=true;
    app->SetFieldMouseLook(true);
    const float dt=std::clamp(app->GetFrameClock()->GetRealDeltaSeconds(),0.f,.08f);
    const bool focused=s.window && GetForegroundWindow()==s.window->GetHwnd();
    if (!focused) { carrotPull_.Cancel(); fpsWaitRelease_=true; }
    const bool armed=focused && !fpsWaitRelease_;
    if (!input.PushMouseButton(InputMouseButton::Left) && !input.PushMouseButton(InputMouseButton::Right) &&
        !held(VK_RETURN) && focused && !s.window->ReadLeftDrag().down) fpsWaitRelease_=false;
    // A desktop-coordinate click is not a raw relative mouse count.
    const auto mouse=armed?s.window->SampleFieldLookDelta():POINT{};
    constexpr float keyLook=570.f;
    fps_.Step(float(held('D')-held('A')),float(held('W')-held('S')),
        mouse.x+(held(VK_RIGHT)-held(VK_LEFT))*dt*keyLook,
        mouse.y+(held(VK_DOWN)-held(VK_UP))*dt*keyLook,dt,s.farmGrid.GetWidth(),s.farmGrid.GetHeight());
    const auto& visual=s.farmVisualSystem;
    auto eye=visual.GetTileCenter(s.farmGrid,0);
    const auto& layout=visual.GetLayout();
    const float pitch=layout.tileSize+layout.tileGap;
    eye.x+=fps_.x*pitch; eye.z+=fps_.z*pitch;
    eye.y=layout.center.y;
    (void)visual.TryGetSoilHeight(s.farmGrid,eye,eye.y);
    eye.y+=farm::FirstPerson::eyeHeight;
    s.host.ApplyCameraPose({eye,{fps_.pitch,fps_.yaw,0}},false);
    fps_.target=visual.PickWorkTarget(s.farmGrid,eye,fps_.Direction(),farm::FirstPerson::reach);

    auto& tools=s.farmToolSystem;
    const int wheel=input.GetMouseWheelDelta();
    if(armed && wheel) tools.SetTool(belt[(Slot(belt,tools.GetCurrentTool())+(wheel>0?3:1))%4]);
    for(int i=0;armed && i<4;++i)
        if(pressed('1'+i)) tools.SetTool(belt[i]);
    auto crop=s.farmCropSelectionSystem.GetSelectedCrop();
    if(armed && (input.TriggerMouseButton(InputMouseButton::Right) || pressed('Q'))) {
        if (tools.GetCurrentTool()==FarmTool::Seed) crop=seeds[(Slot(seeds,crop)+1)%3];
        Execute(s,{farmui::Action::Crop,farm::ToCropSlot(crop)});
    }
    const auto tool=tools.GetCurrentTool();
    fpsFrame_.slot=tool==FarmTool::Hoe?0:tool==FarmTool::Water?1:tool==FarmTool::Harvest?5:2+Slot(seeds,crop);
    fpsFrame_.swing=(std::max)(0.f,fpsFrame_.swing-dt/.32f);
    fpsNoticeSeconds_-=dt;
    const auto* tile=s.farmGrid.GetTile(fps_.target);
    fpsFrame_.reached=tile!=nullptr;
    fpsFrame_.valid=false;
    int caption=Beyond;
    const bool ripeCarrot=armed && tool==FarmTool::Harvest && tile && tile->crop==farm::CropType::Carrot && farm::IsHarvestReady(*tile);
    const auto drag=s.window?s.window->ReadLeftDrag():WinApp::LeftDragSample{};
    const bool use=armed && (drag.began || pressed(VK_RETURN));
    const bool picked=carrotPull_.Step(ripeCarrot?fps_.target:-1,s.farmGrid.GetGeneration(),
        {armed && drag.began,drag.down},{armed && pressed(VK_RETURN),held(VK_RETURN)!=0},dt);
    fpsFrame_.gripping=CarrotGripPose().tension>0.f;
    if(tile) {
        const auto action=s.farmToolActionSystem.EvaluateTool(s.farmGrid,fps_.target,tool,crop,&s.farmEconomySystem);
        fpsFrame_.valid=action.Succeeded();
        caption=tile->crop!=farm::CropType::None?(tile->growth>=1.f?Ripe:Growing):
            tile->state==farm::FarmTileState::Tilled?Tilled:Bare;
        if(ripeCarrot) caption=fpsFrame_.gripping?ReleaseHint:GripHint;
        if(picked || (!ripeCarrot && use)) {
            s.farmGrid.SetSelectedIndex(fps_.target);
            Execute(s,{farmui::Action::ApplyTool});
            fpsFrame_.swing=1.f; fpsNoticeSeconds_=2.2f;
            if(action.Succeeded()) fpsNotice_=tool==FarmTool::Hoe?Dug:tool==FarmTool::Seed?Sown:tool==FarmTool::Water?Watered:Picked;
            else fpsNotice_=action.status==FarmToolActionStatus::NotReady?Wait:action.status==FarmToolActionStatus::NoSeed?
                NoSeeds:action.status==FarmToolActionStatus::AlreadyWatered?FullWater:WrongTool;
        }
    }
    fpsFrame_.message=fpsNoticeSeconds_>0 && !fpsFrame_.gripping?fpsNotice_:caption;
    return true;
}
