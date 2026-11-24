#include "farm/system/FarmWaterPlaySystem.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/core/FarmGrid.h"
#include "3d/LineDrawer.h"
#include <iostream>
#include <stdexcept>
void LineDrawer::DrawLine(const Vector3&,const Vector3&,const Vector4&) {}
void LineDrawer::DrawWireCube(const Vector3&,float,const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3&,float,const Vector4&,uint32_t) {}
namespace {
using namespace farm;
void Check(bool condition,const char* message) { if (!condition) throw std::runtime_error(message); }
struct Field {
    FarmGrid grid;
    FarmVisualSystem visual;
    FarmIrrigationSystem irrigation;
    FarmWaterPlaySystem system;
    Field() { Check(grid.Initialize(5,4),"grid"); visual.Initialize({}); irrigation.Initialize(); }
    void Tile(int i,FarmTileFeature feature,float amount) {
        FarmTile t{}; t.feature=feature; t.waterAmount=amount; Check(grid.SetTile(i,t),"fixture tile");
    }
    void Tick(std::optional<WaterPointer> p={},bool active=true) {
        system.Update(grid,visual,irrigation,p,1.f/60,active);
        Check(system.Vertices().size()<=FarmWaterPlaySystem::kMaxVertices,"capacity");
        for (const auto& v:system.Vertices()) Check(rain::Finite(v.position),"NaN geometry");
    }
    WaterPointer Pointer(int tile,float dx,float dz) const {
        auto c=visual.GetTileCenter(grid,tile);
        return {tile,{dx*1000,dz*1000},{c.x+dx,c.y+4,c.z+dz},{0,-1,0}};
    }
};
void Cursor() {
    Field f;
    f.Tile(6,FarmTileFeature::Canal,.65f);
    f.Tile(7,FarmTileFeature::Canal,0);
    for (int i=0;i<100;++i) f.Tick();
    auto leaf=f.system.LeafPosition(); Check(leaf.has_value(),"wet tile needs leaf");
    const auto quiet=f.system.Vertices().size();
    f.Tick(f.Pointer(6,-.12f,0)); f.Tick(f.Pointer(6,.04f,0));
    Check(f.system.Vertices().size()>quiet,"cursor did not disturb wet canal");
    auto forwardReach=[&](float endX) {
        float maximum=-1.f;
        const auto waves=f.system.Vertices().first(f.system.Vertices().size()-quiet);
        for (const auto& v:waves) maximum=(std::max)(maximum,v.position.x-endX);
        return maximum;
    };
    const float pitch=f.visual.GetLayout().tileSize+f.visual.GetLayout().tileGap;
    const float cx=f.visual.GetTileCenter(f.grid,6).x;
    Check(forwardReach(cx+.04f)<pitch*.035f,"fast sweep left a full circle");
    for (int i=0;i<55;++i) f.Tick(f.Pointer(6,.04f,0));
    Check(f.system.Vertices().size()==quiet,"stationary mouse kept emitting");
    for (int i=1;i<=12;++i) f.Tick(f.Pointer(6,.04f+.009f*i,0));
    Check(forwardReach(cx+.04f+.009f*12)>pitch*.045f,"slow pass did not form a ring");
    for (int i=0;i<55;++i) f.Tick();
    Check(f.system.LeafPosition()->x==leaf->x && f.system.LeafPosition()->z==leaf->z,"still water drifted");
    for (int i=0;i<10;++i) {
        const float x=(i%2==0?.12f:-.12f);
        f.Tick(f.Pointer(7,x,0)); f.Tick(f.Pointer(6,x,.60f)); f.Tick(f.Pointer(0,x,0));
        Check(f.system.Vertices().size()==quiet,"emission over bed, bank or soil");
    }
    f.Tick({},false); Check(f.system.Vertices().empty() && !f.system.LeafPosition(),"modal reset");
}
void Bend() {
    Field f; f.Tile(5,FarmTileFeature::WaterSource,1); f.Tile(6,FarmTileFeature::Canal,.25f);
    f.Tile(11,FarmTileFeature::Canal,.25f); f.Tile(12,FarmTileFeature::Canal,.25f);
    FarmTile bed{}; bed.state=FarmTileState::Tilled;
    Check(f.grid.SetTile(13,bed),"soil"); f.irrigation.Rebuild(f.grid);
    bool turned=false;
    auto elbow=f.visual.GetTileCenter(f.grid,6);
    for (int i=0;i<440;++i) {
        f.irrigation.UpdateWater(f.grid,1.f/60,1); f.Tick();
        auto l=f.system.LeafPosition(); Check(l.has_value(),"leaf left wetted channel");
        turned|=std::abs(l->x-elbow.x)<.25f && l->z>elbow.z+.16f;
    }
    Check(turned,"did not pass elbow");
    for (int i=0;i<30;++i) f.Tick();
    const auto stale=f.system.LeafPosition();
    for (int i=0;i<30;++i) f.Tick();
    Check(stale && f.system.LeafPosition()->x==stale->x && f.system.LeafPosition()->z==stale->z,"stale flow drift");
    Check(f.grid.Initialize(2,2),"replace"); f.Tick();
    Check(!f.system.LeafPosition() && f.system.Vertices().empty(),"stale board");
}
void ShoreAndSlope() {
    Field f;
    for(int i:{5,6,1}) f.Tile(i,FarmTileFeature::Canal,.5f);
    FarmTile raised=*f.grid.GetTile(5); raised.heightLevel=1; Check(f.grid.SetTile(5,raised),"ramp");
    for(int i=0;i<95;++i) f.Tick();
    const auto quiet=f.system.Vertices().size();
    const auto c=f.visual.GetTileVisualData(f.grid,6);
    const float h=c.waterHalfExtent;
    for(int i=0;i<25;++i) {
        f.Tick(f.Pointer(6,h*(i%2==0?.92f:.80f),h*.94f));
        Check(f.system.Vertices().size()==quiet,"outside curved lip");
    }
    const float half=(f.visual.GetLayout().tileSize+f.visual.GetLayout().tileGap)*.5f;
    const float x=-(half+h)*.5f;
    f.Tick(f.Pointer(6,x,-.10f)); f.Tick(f.Pointer(6,x,.04f));
    Check(f.system.Vertices().size()>quiet,"water ramp not picked");
    float low=1000, high=-1000;
    for(auto& v:f.system.Vertices().first(f.system.Vertices().size()-quiet)) {
        low=(std::min)(low,v.position.y); high=(std::max)(high,v.position.y);
    }
    Check(high-low>.004f,"wake floats on horizontal plane instead of slope");
}
}
int main() try { Cursor(); Bend(); ShoreAndSlope(); std::cout<<"PASS: wake, ring, curved shore, slope, rest, elbow, pause and reset\n"; }
catch (const std::exception& e) { std::cerr<<"FAIL: "<<e.what()<<'\n'; return 1; }
