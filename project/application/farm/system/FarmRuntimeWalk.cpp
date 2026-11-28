#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "io/Input.h"

bool FarmRuntimeController::UpdatePondHoe(const FarmRuntimeContext& s,const Input& input,
    farm::pond::Stroke stroke,bool inside,bool click) {
    const auto& grid=s.farmGrid;
    if (!grid.HasPondBank() || s.farmToolSystem.GetCurrentTool()!=FarmTool::Hoe ||
        s.farmCropSelectionSystem.IsOpen() || s.host.IsTimelineScrubbing() || s.farmProgressionSystem.IsCleared() ||
        input.PushMouseButton(InputMouseButton::Right) || input.GetMouseWheelDelta()!=0) return false;
    const bool starting=stroke.last<0;
    const auto mouse=input.ReadLeftDrag();
    const bool keys=starting ? !click && !mouse.began && input.TriggerKey(InputKey::Enter) : stroke.keyboard;
    if (starting && !mouse.began && !keys) return false;
    if (!starting && !(keys?input.PushKey(InputKey::Enter):(mouse.down || mouse.ended))) return false;
    if (!keys && (!inside || view_.Covers(pointer_))) return false;
    const auto& layout=s.farmVisualSystem.GetLayout();
    const float pitch=layout.tileSize+layout.tileGap;
    int target=grid.GetSelectedIndex();
    if (!grid.GetTile(target)) return false;
    auto point=s.farmVisualSystem.GetTileCenter(grid,target);
    int crossing=-1;
    bool exterior=false;
    if (keys) {
        const int x=target%grid.GetWidth(), y=target/grid.GetWidth();
        const auto step=[&](auto a,auto b){return input.TriggerKey(a)||input.TriggerKey(b)||
            (starting && (input.PushKey(a)||input.PushKey(b)));};
        const int dx=int(step(InputKey::D,InputKey::ArrowRight))-int(step(InputKey::A,InputKey::ArrowLeft));
        const int dy=int(step(InputKey::W,InputKey::ArrowUp))-int(step(InputKey::S,InputKey::ArrowDown));
        if (starting && !stroke.Begin(grid,target,point,true)) return false;
        target=std::clamp(y+dy,0,grid.GetHeight()-1)*grid.GetWidth()+std::clamp(x+dx,0,grid.GetWidth()-1);
        point=s.farmVisualSystem.GetTileCenter(grid,target);
    } else {
        const auto first=s.farmVisualSystem.GetTileCenter(grid,0);
        const auto pick=[&](Vector2 client) {
            Vector2 hud{}; Vector3 origin{},dir{};
            if (!s.host.ConvertMouseToVirtualScreen(client,hud) || view_.Covers(hud) ||
                !s.host.TryBuildViewportRayAt(client,origin,dir) || std::abs(dir.y)<.0001f) return false;
            exterior=!s.farmVisualSystem.TryPickTile(grid,origin,dir,target);
            const float y=exterior ? layout.center.y+layout.waterBottomOffset+layout.waterMaxDepth :
                s.farmVisualSystem.GetTileCenter(grid,target).y;
            const float t=(y-origin.y)/dir.y;
            if (!std::isfinite(t) || t<0.f) return false;
            point=origin+dir*t;
            if(exterior) target=farm::pond::FeederTile(grid,first,pitch,point);
            return target>=0;
        };
        const auto client=[](POINT p){return Vector2{float(p.x),float(p.y)};};
        if (starting) {
            if (!pick(client(mouse.press)) || !stroke.Begin(grid,target,point,false)) return false;
            stroke.inFeeder=exterior;
        }
        if (!pick(mouse.ended?client(mouse.release):input.GetMousePosition())) return false;
        if(stroke.inFeeder && exterior) {
            stroke.start=stroke.tip=point;
            if(!mouse.ended) pondStroke_=stroke;
            fieldInputConsumed_=true; return true;
        }
        if(stroke.inFeeder || exterior) {
            crossing=farm::pond::BankCrossing(grid,first,pitch,stroke.tip,point);
            if(crossing<0) return false;
            if(exterior) target=crossing;
            else { stroke.last=crossing; stroke.inFeeder=false; }
        }
    }
    const bool active=stroke.Pull(grid,target,point,pitch,[&](int i,bool across) {
        if (!s.farmToolActionSystem.DigCanalAt(s.farmGrid,i,across)) return false;
        s.farmDocumentSystem.MarkDirty(); walkHit_=true; return true;
    },crossing);
    if (active) {
        if (keys || (!mouse.ended && !exterior)) pondStroke_=stroke;
        s.farmGrid.SetSelectedIndex(target);
    }
    fieldInputConsumed_=true;
    return true;
}

bool FarmRuntimeController::UpdateWalk(const FarmRuntimeContext& s,const Input& input,bool inside,bool click) {
    using farmui::Action;
    bool offered=false;
    for (size_t i=0;i<view_.count;++i) if (view_.items[i].request.action==Action::WalkToggle) { offered=true; break; }
    const bool toggle=offered && (input.TriggerKey(DIK_F4) ||
        (inside && click && view_.Hit(pointer_).action==Action::WalkToggle));
    if (walk_.Active() && (!offered || !walk_.SameBoard(s.farmGrid) || toggle || input.TriggerKey(InputKey::Escape))) {
        s.host.ApplyCameraPose(walk_.Leave(),false);
        walkDestination_=-1; fieldInputConsumed_=true; BuildView(s); return true;
    }
    const bool entering=!walk_.Active();
    if (entering && (!toggle || !walk_.Enter(s.farmGrid,s.host.ReadCameraPose()))) return false;
    fieldInputConsumed_=true;
    const bool left=input.PushMouseButton(InputMouseButton::Left), right=input.PushMouseButton(InputMouseButton::Right);
    if (entering) walkWaitRelease_=true;
    if (!left && !right && !input.PushKey(InputKey::Enter)) walkWaitRelease_=false;
    const bool world=!walkWaitRelease_ && inside && !view_.Covers(pointer_);
    if (entering || !world || (!left && !right)) walkDestination_=-1;
    else if (click || input.TriggerMouseButton(InputMouseButton::Right) ||
        pointer_.x!=walkPointer_.x || pointer_.y!=walkPointer_.y) {
        Vector3 origin{},direction{}; walkDestination_=-1;
        if (s.host.TryBuildViewportRay(origin,direction))
            (void)s.farmVisualSystem.TryPickTile(s.farmGrid,origin,direction,walkDestination_);
    }
    walkPointer_=pointer_;
    const auto key=[&](auto a,auto b){ return input.PushKey(a)||input.PushKey(b); };
    const int dx=int(key(InputKey::D,InputKey::ArrowRight))-int(key(InputKey::A,InputKey::ArrowLeft));
    const int dz=int(key(InputKey::W,InputKey::ArrowUp))-int(key(InputKey::S,InputKey::ArrowDown));
    const auto* clock=Framework::GetInstance()->GetFrameClock();
    const float dt=clock?clock->GetRealDeltaSeconds():0.f;
    const int dig=walk_.Step(s.farmGrid,dx,dz,!walkWaitRelease_ && ((left && world)||input.PushKey(InputKey::Enter)),dt,walkDestination_);
    if (dig>=0 && s.farmToolActionSystem.DigCanalAt(s.farmGrid,dig,walk_.LastCutAcrossBank())) {
        s.farmDocumentSystem.MarkDirty(); walkHit_=true;
    }
    const int tile=walk_.Tile(s.farmGrid);
    auto feet=s.farmVisualSystem.GetTileCenter(s.farmGrid,tile);
    const auto first=s.farmVisualSystem.GetTileCenter(s.farmGrid,0);
    const auto& layout=s.farmVisualSystem.GetLayout();
    const float pitch=layout.tileSize+layout.tileGap;
    feet.x=first.x+walk_.X()*pitch; feet.z=first.z+walk_.Z()*pitch;
    (void)s.farmVisualSystem.TryGetSoilHeight(s.farmGrid,feet,feet.y);
    walk_.Ground(feet);
    // Fixed azimuth keeps screen directions stable when the player turns a corner.
    constexpr float back=5.7f, rise=4.5f, downwardPitch=.60f;
    s.host.ApplyCameraPose({feet+Vector3{0,rise,-back},{downwardPitch,0,0}},false);
    if (entering) { hotbarSlot_=-1; wheelNotice_.Clear(); BuildView(s); }
    return true;
}
