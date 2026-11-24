#include "farm/system/FarmBrushPlaySystem.h"
#include "farm/core/FarmGrid.h"
#include <iostream>
#include <stdexcept>

namespace {
using namespace farm;
void Check(bool ok,const char* message){ if(!ok) throw std::runtime_error(message); }
void Brush(){
    FarmGrid grid; Check(grid.Initialize(4,2),"board");
    FarmTile bed{}; bed.state=FarmTileState::Tilled;
    Check(grid.SetTile(1,bed),"bed"); bed.moisture=.9f; Check(grid.SetTile(2,bed),"wet bed");
    for(auto crop:{CropType::TestCrop,CropType::Carrot,CropType::Tomato,CropType::Pumpkin}) {
        FarmTile plant{}; plant.state=FarmTileState::Planted; plant.crop=crop; plant.growth=.92f;
        Check(grid.SetTile(3,plant),"plant");
        FarmBrushPlaySystem fx;
        auto tick=[&](int n) { for(int i=0;i<n;++i) fx.Update(grid,1.f/60,true); };
        tick(1);
        Check(!fx.MovePointer(Vector2{45,50}),"entry emitted");
        Check(!fx.MovePointer(Vector2{45,50}),"stationary");
        Check(fx.MovePointer(Vector2{48,50}).has_value(),"movement missed");
        fx.Stroke(grid,{0,{},false},{1,0}); fx.Stroke(grid,{-1,{},true},{1,0});
        Check(fx.Read().empty(),"grass or OOB emitted");
        fx.Stroke(grid,{1,{},false},{1,0}); Check(fx.Read().size()==2 && fx.Read()[0].kind==FieldGrain::Dust,"dry");
        tick(50); fx.Stroke(grid,{2,{},false},{1,0}); Check(fx.Read().size()==1,"wet");
        tick(50); fx.Stroke(grid,{3,{},false},{1,0}); Check(fx.Read()[0].kind==FieldGrain::Dust,"soil below plant");
        tick(50); fx.Stroke(grid,{3,{0,.25f,0},true},{1,0});
        for(int i=0;i<20;++i) fx.Stroke(grid,{3,{0,.25f,0},true},{1,0});
        Check(fx.Read().size()==1 && fx.Read()[0].kind==FieldGrain::Leaf,"leaf burst");
        tick(4); Check(fx.Lean(3,crop).x>.1f && fx.Lean(1,crop).x==0,"wrong plant leaned");
        Check(grid.GetTile(3)->growth==plant.growth,"modified growth");
        Check(!fx.MovePointer(std::nullopt) && !fx.MovePointer(Vector2{160,90}),"HUD reentry");
        tick(60); Check(fx.Read().empty() && fx.Lean(3,crop).x==0,"did not settle");
        fx.Update(grid,.02f,false); fx.Stroke(grid,{3,{},true},{1,0}); Check(fx.Read().empty(),"paused");
    }
    FarmBrushPlaySystem fx;
    fx.Update(grid,.02f,true); fx.Stroke(grid,{3,{},true},{1,0});
    Check(grid.Initialize(2,2),"replace"); fx.Update(grid,.02f,true);
    Check(fx.Read().empty(),"particles survived grid replacement");
}
}
int main() try { Brush(); std::cout<<"PASS: soil, moisture, all crops, caps, cursor gating, decay and reset\n"; }
catch(const std::exception& ex) { std::cerr<<ex.what()<<'\n'; return 1; }
