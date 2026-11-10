#include "farm/system/FarmPestVisualSystem.h"
#include "farm/render/FarmMaterialStyle.h"
#include "3d/LineDrawer.h"
#include <iostream>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <string>

void LineDrawer::DrawLine(const Vector3&, const Vector3&, const Vector4&) {}
void LineDrawer::DrawWireCube(const Vector3&, float, const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3&, float, const Vector4&, uint32_t) {}
namespace {
using namespace farm;
void Check(bool value, std::source_location at=std::source_location::current()) {
    if (!value) throw std::runtime_error("pest visual line " + std::to_string(at.line()));
}
FarmTile Crop(CropType type=CropType::Carrot, int damage=0) {
    FarmTile tile; tile.crop=type; tile.state=FarmTileState::Planted; tile.growth=.8f; tile.pests={6,damage}; return tile;
}
void Valid(const FarmPestMeshParts& mesh) {
    Check(mesh.count<=mesh.parts.size());
    for(std::size_t i=0;i<mesh.count;++i) {
        const auto& p=mesh.parts[i];
        Check(std::isfinite(p.position.x) && std::isfinite(p.position.y) && std::isfinite(p.position.z));
        Check(p.scale.x>=.0001f && p.scale.y>=.0001f && p.scale.z>=.0001f && p.color.w==1);
        Check(!p.water && p.shape==FarmMeshShape::Box);
        if(p.surface==FarmMeshSurface::PestCue) {
            const auto style=BuildFarmMaterialStyle(p);
            Check(style.detail.x==0 && style.detail.y==0 && style.detail.z==0 && style.detail.w==0);
        }
    }
}
void WarningAndCure() {
    FarmGrid grid; Check(grid.Initialize(5,4));
    FarmVisualSystem visual; visual.Initialize({}); FarmPestVisualSystem fx;
    Check(fx.BuildParts(grid,visual).count==0 && !fx.OnCured(grid,-1,visual));
    Check(grid.SetTile(0,Crop()));
    auto before=*grid.GetTile(0);
    auto mesh=fx.BuildParts(grid,visual); Check(mesh.count==2); Valid(mesh);
    Check(!fx.OnCured(grid,0,visual));
    const float initial=mesh.parts[0].position.y;
    fx.Update(grid,.1f); mesh=fx.BuildParts(grid,visual);
    Check(mesh.parts[0].position.y!=initial);
    for(float dt:{-1.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        const auto previous=fx.BuildParts(grid,visual);
        fx.Update(grid,dt); mesh=fx.BuildParts(grid,visual); Valid(mesh);
        Check(mesh.parts[0].position.y==previous.parts[0].position.y);
    }
    Check(grid.GetTile(0)->pests==before.pests && grid.GetTile(0)->growth==before.growth);
    Check(FarmPestSystem::AdvanceDays(grid,6,7,true).changed);
    mesh=fx.BuildParts(grid,visual); Check(mesh.count==10); Valid(mesh);
    Check(FarmPestSystem::Cure(grid,0,6) && fx.OnCured(grid,0,visual));
    Check(!FarmPestSystem::Cure(grid,0,6) && !fx.OnCured(grid,0,visual));
    Check(grid.GetTile(0)->pests.damageDays==1);
    Check(fx.BuildParts(grid,visual).count==6);
    fx.Update(grid,.3f); mesh=fx.BuildParts(grid,visual); Check(mesh.count==6); Valid(mesh);
    fx.Update(grid,.3f); Check(fx.BuildParts(grid,visual).count==0);
    Check(fx.OnCured(grid,0,visual));
    FarmGrid::Snapshot snapshot; grid.CaptureSnapshot(snapshot); Check(grid.RestoreSnapshot(snapshot));
    Check(fx.BuildParts(grid,visual).count==0);
    fx.Update(grid,1); Check(fx.BuildParts(grid,visual).count==0);
    Check(fx.OnCured(grid,0,visual));
    auto raised=*grid.GetTile(0); raised.heightLevel=1; Check(grid.SetTile(0,raised));
    Check(fx.BuildParts(grid,visual).count==0);
    fx.Update(grid,0);
    Check(fx.OnCured(grid,0,visual));
    Check(grid.SetTile(0,FarmTile{})); Check(fx.BuildParts(grid,visual).count==0);
    fx.Clear(); Check(fx.BuildParts(grid,visual).count==0);
}
void CoverageAndBudget() {
    FarmGrid grid; Check(grid.Initialize(5,6)); FarmVisualSystem visual; visual.Initialize({}); FarmPestVisualSystem fx;
    Check(grid.SelectTile(6) && FarmRepellentSystem::SetPlant(grid,6,grid.GetGeneration(),true));
    auto mesh=fx.BuildParts(grid,visual); Check(mesh.count==40); Valid(mesh);
    for(std::size_t i=0;i<mesh.count;i+=8) {
        const auto& part=mesh.parts[i]; int matched=-1;
        for(int cell=0;cell<grid.GetTileCount();++cell) {
            const auto data=visual.GetTileVisualData(grid,cell);
            if(std::abs(part.position.x-data.center.x)<data.halfExtent && std::abs(part.position.z-data.center.z)<data.halfExtent)
                matched=cell;
        }
        Check(FarmRepellentSystem::InRange(grid,6,matched));
    }
    Check(grid.SelectTile(0)); Check(fx.BuildParts(grid,visual).count==0);
    Check(FarmRepellentSystem::SetPlant(grid,0,grid.GetGeneration(),true)); Check(fx.BuildParts(grid,visual).count==24);
    Check(grid.SelectTile(4) && FarmRepellentSystem::SetPlant(grid,4,grid.GetGeneration(),true));
    Check(fx.BuildParts(grid,visual).count==24);
    Check(grid.SelectTile(6));
    for(auto feature:{FarmTileFeature::Canal,FarmTileFeature::WaterSource}) {
        FarmTile water; water.feature=feature; Check(grid.SetTile(1,water));
        Check(fx.BuildParts(grid,visual).count==32);
    }
    Check(grid.SetTile(1,FarmTile{})); Check(fx.BuildParts(grid,visual).count==40);
    for(int i=12;i<15;++i) {
        auto tile=Crop(CropType::Tomato,2); tile.heightLevel=i-12; Check(grid.SetTile(i,tile));
    }
    Check(fx.BuildParts(grid,visual).count==70);
    for(int i=12;i<15;++i) Check(FarmPestSystem::Cure(grid,i,6) && fx.OnCured(grid,i,visual));
    for(int i=15;i<18;++i) Check(grid.SetTile(i,Crop(CropType::Pumpkin,1)));
    mesh=fx.BuildParts(grid,visual); Check(mesh.count==FarmPestMeshParts::kCapacity); Valid(mesh);
    for(int i=0;i<1000;++i) { fx.Update(grid,.016f); Valid(fx.BuildParts(grid,visual)); }
    Check(fx.BuildParts(grid,visual).count==70);
    fx.Clear(); Check(fx.BuildParts(grid,visual).count==70);
}
void ShapesAndInvalid() {
    FarmGrid grid; Check(grid.Initialize(3,1)); FarmVisualSystem visual; visual.Initialize({});
    FarmPestMeshParts mesh;
    AppendFarmPestMeshes(mesh,grid,-1,visual,0); AppendFarmProtectionMeshes(mesh,grid,3,visual);
    AppendFarmCureMeshes(mesh,{},1,std::numeric_limits<float>::quiet_NaN()); Check(mesh.count==0);
    for(auto crop:{CropType::Carrot,CropType::Tomato,CropType::Pumpkin}) for(int height=0;height<3;++height) {
        auto tile=Crop(crop,2); tile.heightLevel=height; Check(grid.SetTile(1,tile));
        mesh={}; AppendFarmPestMeshes(mesh,grid,1,visual,0); Check(mesh.count==10); Valid(mesh);
        const auto top=FarmPestCropTop(grid,1,visual);
        Check(mesh.parts[1].position.y-mesh.parts[1].scale.y>top.y);
    }
    mesh={}; AppendFarmPestMeshes(mesh,grid,1,visual,std::numeric_limits<float>::infinity()); Check(mesh.count==0);
    for(int i=0;i<200;++i) mesh.Add({}, {1,1,1}, {1,1,1,1});
    Check(mesh.count==FarmPestMeshParts::kCapacity);
}
}
int main() {
    WarningAndCure(); CoverageAndBudget(); ShapesAndInvalid();
    std::cout << "PASS: pest warning/damage/cure, finite meshes, lifetime, coverage, 88-part budget, rules unchanged\n";
}
