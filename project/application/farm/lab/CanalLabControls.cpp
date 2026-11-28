#include "CanalLabControls.h"
#include "io/Input.h"
#include <algorithm>
#include <cmath>
namespace canal_lab {
namespace {
Point Hit(Vector2 p) {return {int(std::floor((p.x-Layout::X)/Layout::W)),int(std::floor((p.y-Layout::Y)/Layout::H))};}
bool Reachable(Point p) {return Inside(p)||InPondRing(p);}
}
void Controls::Restart(System& field) {field.Reset();*this=Controls{};}
void Controls::Act(System& field,Point a,Point b,Tool tool) {
    if(a==b) return;
    if(!field.Stroke(a,b,tool)) {stroke_=false;return;}
    if(!Inside(a) && !Inside(b)) return;
    const auto last=Layout::Origin(tool==Tool::Bank?a:b);
    tip_={last.x+Layout::W*.5f,last.y+Layout::H*.5f};
    if(tool==Tool::Bank) {
        tip_.x+=std::clamp(b.x-a.x,-1,1)*Layout::W*.4f;
        tip_.y+=std::clamp(b.y-a.y,-1,1)*Layout::H*.4f;
        stroke_=false;
    }
    impact_=.36f;actionTool_=tool;
}
void Controls::Update(System& field,const Input& input,const WinApp& window,float dt) {
    impact_=(std::max)(0.f,impact_-dt);
    if(GetForegroundWindow()!=window.GetHwnd() || !window.GetClientWidth() || !window.GetClientHeight()) {
        stroke_=false;repeat_=0;return;
    }
    const auto screen=[&](float x,float y)->Vector2{return {1280*x/window.GetClientWidth(),720*y/window.GetClientHeight()};};
    const auto mouse=input.GetMousePosition();
    const auto pos=screen(mouse.x,mouse.y);
    const auto edges=input.ReadLeftDrag();
    const bool rStart=input.TriggerMouseButton(InputMouseButton::Right);
    const bool moved=pos.x!=lastMouse_.x || pos.y!=lastMouse_.y;lastMouse_=pos;
    if(input.TriggerKey(InputKey::R)){Restart(field);return;}
    if(input.TriggerKey(InputKey::Tab)){mode_=mode_==Tool::Dig?Tool::Bank:Tool::Dig;stroke_=false;}
    if(moved || edges.began || rStart) keyboard_=false;
    if(!keyboard_) cursor_=Hit(pos);
    if(edges.began || rStart) {
        right_=rStart && !edges.began;stroke_=false;
        const auto press=right_?pos:screen(float(edges.press.x),float(edges.press.y));
        if(!right_ && press.y>=Layout::Bar+11 && press.y<=Layout::Bar+65) {
            if(press.x>=35 && press.x<=203) mode_=Tool::Dig;
            if(press.x>=220 && press.x<=388) mode_=Tool::Bank;
            if(press.x>=1050 && press.x<=1237) Restart(field);
            return;
        }
        anchor_=Hit(press);stroke_=Reachable(anchor_);
    }
    if(stroke_) {
        const bool ended=right_?input.ReleaseMouseButton(InputMouseButton::Right):edges.ended;
        const auto end=(!right_&&edges.ended)?screen(float(edges.release.x),float(edges.release.y)):pos;
        const auto target=Hit(end);
        if(Reachable(target)) {Act(field,anchor_,target,right_?Tool::Bank:mode_);anchor_=target;}
        else stroke_=false;
        if(ended || !(right_?input.PushMouseButton(InputMouseButton::Right):edges.down)) stroke_=false;
    }
    constexpr InputKey keys[]={InputKey::ArrowUp,InputKey::ArrowRight,InputKey::ArrowDown,InputKey::ArrowLeft};
    constexpr InputKey wasd[]={InputKey::W,InputKey::D,InputKey::S,InputKey::A};
    int direction=-1;bool edge=false;
    for(int d=0;d<4;++d) {
        edge=input.TriggerKey(keys[d])||input.TriggerKey(wasd[d]);
        if(edge || input.PushKey(keys[d]) || input.PushKey(wasd[d])) {direction=d;break;}
    }
    if(direction<0) {repeat_=0;return;}
    if(!edge && (repeat_-=dt)>0) return;
    if(!Reachable(cursor_)) cursor_={5,-1};
    keyboard_=true;stroke_=false;
    const auto next=Next(cursor_,direction);
    if(Reachable(next)) {
        if(input.PushKey(InputKey::Space)||input.TriggerKey(InputKey::Space)) Act(field,cursor_,next,Tool::Bank);
        else if(input.PushKey(InputKey::Enter)||input.TriggerKey(InputKey::Enter)) Act(field,cursor_,next,Tool::Dig);
        cursor_=next;
    }
    repeat_=edge?.26f:.13f;
}
}
