#pragma once
#include "CanalLabSystem.h"
#include "math/Struct.h"
class Input;
class WinApp;
namespace canal_lab {
struct Layout {
    static constexpr float W=84,H=60,X=178,Y=148,Bar=640;
    static Vector2 Origin(Point p) {return {X+p.x*W,Y+p.y*H};}
};
class Controls {
public:
    void Update(System&,const Input&,const WinApp&,float dt);
    Tool Selected() const {return mode_;}
    Point Cursor() const {return cursor_;}
    Vector2 Tip() const {return tip_;}
    float Impact() const {return impact_;}
    bool Casting() const {return actionTool_==Tool::Bank;}
private:
    void Act(System&,Point,Point,Tool);
    void Restart(System&);
    Tool mode_=Tool::Dig,actionTool_=Tool::Dig;
    Point cursor_{5,-1},anchor_{};
    Vector2 tip_{},lastMouse_{-1,-1};
    float impact_=0,repeat_=0;
    bool stroke_=false,right_=false,keyboard_=false;
};
}
