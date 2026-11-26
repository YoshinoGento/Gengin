#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmDocumentSystem.h"
#include "base/Framework.h"
#include "base/FrameClock.h"
#include "io/Input.h"

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
    if (dig>=0 && s.farmToolActionSystem.DigCanalAt(s.farmGrid,dig)) {
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
