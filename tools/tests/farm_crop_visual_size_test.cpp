#include "farm_test_edit.h"
#include "farm/system/FarmVisualSystem.h"
#include "farm/system/FarmFirstPerson.h"
#include "farm/render/FarmCarrotGripPose.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/render/FarmMaterialStyle.h"
#include "farm/system/FarmHarvestVisualSystem.h"
#include "3d/LineDrawer.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <limits>
#include <vector>

namespace {
struct Line { Vector3 a, b; Vector4 color; };
std::vector<Line> lines;
float sphereRadius = 0;
Vector3 sphereCenter{};
bool Near(float a, float b) { return std::fabs(a-b) < 0.0001f; }
void CheckTomatoInCrate(const farm::FarmMeshPart& fruit,const farm::FarmMeshPart& bin,bool large) {
    std::ifstream obj(large?"project/Resources/farm/arrival/picked_tomato_0.obj":"project/Resources/farm/arrival/picked_tomato_1.obj");
    assert(obj);
    std::string line;
    size_t checked=0;
    while(std::getline(obj,line)) {
        if(!line.starts_with("v ")) continue;
        std::istringstream row(line.substr(2)); Vector3 v{};
        assert(row>>v.x>>v.y>>v.z);
        const auto local=fruit.position-bin.position+
            farm::harvest::Roll({-v.x*fruit.scale.x,v.y*fruit.scale.y,v.z*fruit.scale.z},fruit.roll);
        assert(std::abs(local.x)<.50f*bin.scale.x && std::abs(local.z)<.324f*bin.scale.z);
        assert(local.y>=.075f*bin.scale.y-.0001f); ++checked;
    }
    assert(checked>0);
}
farm::FarmMeshPart FindPart(const farm::harvest::Parts& pose,farm::FarmMeshShape shape) {
    for(size_t i=0;i<pose.count;++i) if(pose.parts[i].shape==shape) return pose.parts[i];
    assert(false && "missing harvest mesh"); return {};
}
}
LineDrawer* LineDrawer::GetInstance() { static LineDrawer instance; return &instance; }
void LineDrawer::DrawLine(const Vector3& a, const Vector3& b, const Vector4& c) { lines.push_back({a,b,c}); }
void LineDrawer::DrawWireCube(const Vector3&, float, const Vector4&) {}
void LineDrawer::DrawWireSphere(const Vector3& p, float r, const Vector4&, uint32_t) { sphereCenter=p; sphereRadius=r; }

static void CheckPumpkinLanding() {
    using S=farm::FarmMeshShape;
    farm::FarmGrid grid; assert(grid.Initialize(3,1));
    farm::FarmVisualSystem visual; visual.Initialize({});
    farm::FarmTile bed{}; bed.state=farm::FarmTileState::Tilled;
    farm::FarmTile crop{}; crop.state=farm::FarmTileState::Planted; crop.growth=1;
    FarmToolActionResult action; action.status=FarmToolActionStatus::Harvested;
    for(int index:{0,2}) for(auto kind:{farm::CropType::Pumpkin,farm::CropType::Tomato}) {
        crop.crop=kind; crop.moisture=.21f;
        crop.careHistory.goodSeconds=crop.careHistory.efficiencySeconds=10;
        crop.careHistory.nutrientGrowth=crop.careHistory.nutrientSupply=1;
        assert(grid.SetTile(index,bed));
        action.tileIndex=index; action.harvestedTile=crop;
        farm::FarmHarvestVisualSystem fx; assert(fx.Start(action,grid,visual));
        assert(!fx.Start(action,grid,visual));
        const auto original=fx.GetParts(grid,0);
        int hits=0; const float side=index==0?-1.f:1.f;
        farm::harvest::BasketLanding at{};
        const auto advance=[&](float dt){fx.Update(grid,dt,[&](const auto& hit){++hits; at=hit;});};
        advance(farm::harvest::kContact-.065f);
        assert(hits==0 && Near(FindPart(fx.GetParts(grid,0),S::HarvestBasket).roll,0));
        advance(.065f+.002f); advance(0); advance(.098f);
        const auto pose=fx.GetParts(grid,0);
        const auto bin=FindPart(pose,S::HarvestBasket),rim=FindPart(pose,S::HarvestBasketRim);
        assert(Near(bin.roll,rim.roll) && Near(bin.position.x,rim.position.x));
        if(kind==farm::CropType::Pumpkin) {
            assert(hits==1 && at.side==side && Near(at.moisture,crop.moisture));
            assert(bin.roll*side<-.015f && std::abs(bin.roll)<.045f && (at.rim.x-bin.position.x)*side>0);
            const auto fruit=FindPart(pose,S::Pumpkin);
            const auto local=farm::harvest::Roll(fruit.position-bin.position,-bin.roll);
            assert(Near(fruit.roll,bin.roll) && Near(local.y,.075f*bin.scale.y));
            assert(Near(local.x,side*.055f*bin.scale.x));
            assert(fruit.scale.x>.60f*FindPart(original,S::Pumpkin).scale.x);
            assert(std::abs(local.x)+.951f*fruit.scale.x<.45f*bin.scale.x && .819f*fruit.scale.z<.30f*bin.scale.z);
            const auto foot=bin.position+farm::harvest::Roll({side*.53f*bin.scale.x,.015f*bin.scale.y,0},bin.roll);
            assert(Near(foot.y,at.floor+.015f*bin.scale.y) && Near(at.rim.y,at.floor+.345f*bin.scale.y));
        } else assert(hits==1 && !at.spillsSoil && Near(bin.roll,0));
        advance(.14f);
        if(kind==farm::CropType::Pumpkin) assert(FindPart(fx.GetParts(grid,0),S::HarvestBasket).roll*side>0);
        advance(.22f); assert(Near(FindPart(fx.GetParts(grid,0),S::HarvestBasket).roll,0));
        advance(2); assert(hits==(kind==farm::CropType::Tomato?2:1));
        crop.crop=farm::CropType::Pumpkin; action.harvestedTile=crop;
        assert(fx.Start(action,grid,visual)); assert(grid.SetTile(index,crop));
        const int before=hits; advance(1); assert(hits==before);
    }
    std::cout<<"PASS: pumpkin contact once, mirrored footing, seated fruit, recoil and cancelled pickup\n";
}
static void CheckSizedMotion() {
    using S=farm::FarmMeshShape;
    farm::FarmGrid grid; assert(grid.Initialize(1,1));
    farm::FarmVisualSystem visual; visual.Initialize({});
    farm::FarmTile bed{}; bed.state=farm::FarmTileState::Tilled; assert(grid.SetTile(0,bed));
    for(auto kind:{farm::CropType::Tomato,farm::CropType::Carrot,farm::CropType::Pumpkin}) {
        float motion[2]{};
        for(int big=0;big<2;++big) {
            farm::FarmTile crop{}; crop.state=farm::FarmTileState::Planted; crop.crop=kind; crop.growth=1.f;
            crop.careHistory.goodSeconds=10; crop.careHistory.efficiencySeconds=big?10.f:.25f;
            crop.careHistory.nutrientGrowth=crop.careHistory.nutrientSupply=1;
            FarmToolActionResult res; res.status=FarmToolActionStatus::Harvested; res.tileIndex=0; res.harvestedTile=crop;
            res.harvestQuality.harvestSize=FarmCropSizeSystem{}.Evaluate(crop);
            farm::FarmHarvestVisualSystem fx; assert(fx.Start(res,grid,visual));
            if(kind==farm::CropType::Carrot) {
                fx.Update(grid,farm::harvest::kRelease);
                motion[big]=FindPart(fx.GetParts(grid,0),S::Carrot).position.y;
            } else if(kind==farm::CropType::Tomato) {
                fx.Update(grid,farm::harvest::kContact);
                const float x=FindPart(fx.GetParts(grid,0),S::PickedTomatoLarge).position.x;
                for(int j=0;j<36;++j) {
                    const auto p=fx.GetParts(grid,0); const auto bin=FindPart(p,S::HarvestBasket);
                    const auto fruit=FindPart(p,S::PickedTomatoLarge); CheckTomatoInCrate(fruit,bin,true);
                    if(j>=19) CheckTomatoInCrate(FindPart(p,S::PickedTomatoSmall),bin,false);
                    motion[big]=x-fruit.position.x; fx.Update(grid,.01f);
                }
            } else {
                int hits=0;
                auto count=[&](const auto& hit){++hits; assert(hit.size==(big?farm::harvest::SizeCue::Large:farm::harvest::SizeCue::Small));};
                fx.Update(grid,farm::harvest::kContact+.10f,count);
                motion[big]=std::abs(FindPart(fx.GetParts(grid,0),S::HarvestBasket).roll);
                fx.Update(grid,0,count); fx.Update(grid,1.f,count); assert(hits==1 && motion[big]<.052f);
            }
        }
        if(kind==farm::CropType::Tomato) assert(motion[0]>2.f*motion[1] && motion[1]>0);
        else assert(motion[1]>motion[0]);
    }
}
int main() {
    {
        using namespace farm;
        FarmCarrotPull g;
        PullInput start{true,true},down{false,true},up{};
        assert(g.Step(2,1,{true,false},up,0));
        assert(!g.Step(2,1,up,up,0));
        assert(!g.Step(2,1,start,up,.120001f) && g.Pose(0).tension==0); g.Cancel();
        assert(!g.Step(2,1,start,up,.04f) && g.Pose(0).tension==0);
        assert(!g.Step(2,1,down,{true,false},1) && Near(g.Pose(0).tension,1));
        assert(!g.Step(3,1,down,up,.01f) && !g.Step(3,1,up,up,.01f));
        assert(!g.Step(2,1,up,start,.3f) && !g.Step(2,2,up,up,0));
        assert(!g.Step(2,2,up,start,.3f));
        assert(g.Step(2,2,down,up,0) && !g.Step(2,2,up,up,0));
        assert(!g.Step(2,2,start,up,1)); g.Cancel(); assert(!g.Step(2,2,up,up,0));
        FarmTileVisualData bed{}; bed.valid=true; bed.crop=CropType::Carrot; bed.cropStage=FarmCropGrowthStage::Ready;
        float previous=0;
        for(float scale:{.7f,1.5f}) {
            bed.cropScale=scale;
            auto crop=BuildFarmCropMeshParts(bed,1); const auto original=crop;
            size_t count=0; float thickness=0;
            VisitCarrotGrip(bed,{2,1.f,.7f},crop,[&](const FarmMeshPart& p){
                assert(p.scale.x>0 && p.scale.y>0 && p.scale.z>0);
                if(count==1) thickness=p.scale.y;
                ++count;
            });
            assert(count==kGripParts && thickness>previous); previous=thickness;
            assert(Near(crop.parts[0].position.y-original.parts[0].position.y,crop.parts[1].position.y-original.parts[1].position.y));
        }
        std::cout<<"PASS: carrot tap, hold, release once, cancel, target/board/input identity and soil response\n";
    }
    {
        farm::FirstPerson fps;
        fps.Enter(5,4,{});
        auto diagonal=fps,straight=fps;
        diagonal.Step(1,1,0,0,.05f,5,4);
        straight.Step(0,1,0,0,.05f,5,4);
        assert(Near(std::hypot(diagonal.x-fps.x,diagonal.z-fps.z),straight.z-fps.z));
        for(int i=0;i<200;++i) fps.Step(1,1,0,0,.08f,5,4);
        assert(fps.x<5 && fps.z<4);
        farm::FarmGrid board; assert(board.Initialize(3,3));
        farm::FarmVisualSystem visual; visual.Initialize({});
        auto eye=visual.GetTileCenter(board,4)+Vector3{0,1.2f,0};
        assert(visual.PickWorkTarget(board,eye,{0,-1,0},2)==4);
        assert(visual.PickWorkTarget(board,eye,{0,-1,0},.25f)==-1);
        assert(visual.PickWorkTarget(board,eye,{0,1,0},3.5f)==-1);
        farm::FarmTile plant{}; plant.state=farm::FarmTileState::Planted; plant.crop=farm::CropType::Tomato; plant.growth=1;
        assert(board.SetTile(4,plant));
        eye=visual.GetTileCenter(board,4)+Vector3{0,.24f,-1.5f};
        assert(visual.PickWorkTarget(board,eye,{0,0,1},2)==4);
        std::cout<<"PASS: normalized FPS walk, board edges, soil reach and plant targeting\n";
    }
    for (float scale : {.5f,1.f,2.f}) {
        farm::FarmTileVisualData v{}; v.valid=true; v.crop=farm::CropType::Pumpkin;
        v.cropStage=farm::FarmCropGrowthStage::Ready; v.cropScale=scale;
        auto parts=farm::BuildFarmCropMeshParts(v,1); assert(parts.count==2);
        for (Vector2 lean : {Vector2{.4f,-.6f},Vector2{-.9f,.7f}}) {
            const auto pose=[&](const auto& part){return farm::CropWindPose(part,{},farm::CropBrushTilt(part.shape,lean));};
            const auto a=pose(parts.parts[0]), b=pose(parts.parts[1]);
            assert(Near(a.rotation.x,b.rotation.x) && Near(a.rotation.y,b.rotation.y) && Near(a.rotation.z,b.rotation.z));
            const float h=parts.parts[0].heightAboveWindPivot;
            const float dy=h*std::cos(b.rotation.x);
            assert(Near(a.position.x,b.position.x-dy*std::sin(b.rotation.z)) &&
                Near(a.position.y,b.position.y+dy*std::cos(b.rotation.z)) && Near(a.position.z,b.position.z+h*std::sin(b.rotation.x)));
        }
    }
    CheckPumpkinLanding();
    CheckSizedMotion();
    {
        farm::FarmGrid field; assert(field.Initialize(1,1));
        farm::FarmVisualSystem visual; visual.Initialize({});
        for (auto state : {farm::FarmTileState::Empty, farm::FarmTileState::Tilled}) {
            for (float moisture : {0.0f,0.5f,1.0f}) {
                farm::FarmTile tile; tile.state=state; tile.moisture=moisture;
                assert(field.SetTile(0,tile));
                const auto parts=farm::BuildFarmTileMeshParts(field,0,visual);
                assert(parts.count==(state==farm::FarmTileState::Empty ? 6u : 5u));
                const auto top=state==farm::FarmTileState::Empty ?
                    farm::FarmMeshSurface::Grass : farm::FarmMeshSurface::Soil;
                assert(parts.parts[0].surface==farm::FarmMeshSurface::Earth);
                assert(parts.parts[1].surface==top && Near(parts.parts[1].wetness,moisture));
                const auto material=farm::BuildFarmMaterialStyle(parts.parts[1]);
                assert(material.detail.z>0.0f);
                if(state==farm::FarmTileState::Tilled && moisture>0.0f) assert(material.detail.x>0.0f);
                assert(field.GetTile(0)->moisture==moisture && field.GetTile(0)->state==state);
            }
        }
        std::cout << "PASS: material surface tags, wetness snapshot, unchanged mesh budget and simulation\n";
    }
    // Presentation progresses with real time even while world time is zero (pause or clear).
    for(auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot, farm::CropType::Tomato, farm::CropType::Pumpkin}) {
        farm::FarmGrid field; assert(field.Initialize(1,1));
        farm::FarmTile before; before.crop=crop; before.state=farm::FarmTileState::Planted; before.growth=1;
        before.heightLevel=2; auto candidate=*field.GetTile(0); auto* after=&candidate;
        after->state=farm::FarmTileState::Tilled; after->heightLevel=2;
        assert(field.SetTile(0,*after));
        const auto saved=*after;
        FarmToolActionResult action; action.status=FarmToolActionStatus::Harvested;
        action.tileIndex=0; action.harvestedTile=before;
        farm::FarmVisualSystem visual; visual.Initialize({});
        farm::FarmHarvestVisualSystem effect; assert(effect.Start(action,field,visual));
        for(int frame=0;frame<20;++frame) effect.Update(field,1.f/60.f);
        const auto pulled=effect.GetParts(field,0);
        using S=farm::FarmMeshShape;
        const auto shape=crop==farm::CropType::TestCrop?S::Turnip:crop==farm::CropType::Carrot?S::Carrot:
            crop==farm::CropType::Tomato?S::PickedTomatoLarge:S::Pumpkin;
        assert(FindPart(pulled,shape).position.y>visual.GetTileVisualData(field,0).center.y);
        assert(FindPart(pulled,S::HarvestBasket).scale.x>0);
        for(int frame=0;frame<100;++frame) effect.Update(field,1.f/60.f);
        assert(effect.GetActiveCount(field)==0);
        const auto* actual=field.GetTile(0);
        assert(actual->state==saved.state && actual->crop==saved.crop && actual->growth==saved.growth && actual->moisture==saved.moisture);
    }
    for(auto crop : {farm::CropType::Tomato, farm::CropType::Pumpkin}) {
        farm::FarmGrid grid; assert(grid.Initialize(1,1));
        farm::FarmTile before; before.crop=crop; before.state=farm::FarmTileState::Planted; before.growth=1;
        assert(EditFarmTile(grid,0,[](auto& current) { current.state=farm::FarmTileState::Tilled; }));
        FarmToolActionResult result; result.status=FarmToolActionStatus::Harvested; result.tileIndex=0; result.harvestedTile=before;
        farm::FarmVisualSystem visual; visual.Initialize({});
        farm::FarmHarvestVisualSystem harvest; assert(harvest.Start(result,grid,visual));
        using S=farm::FarmMeshShape;
        const auto first=harvest.GetParts(grid,0);
        if (crop==farm::CropType::Tomato) {
            const auto largeFruit=FindPart(first,S::PickedTomatoLarge),smallFruit=FindPart(first,S::PickedTomatoSmall);
            const auto vine=FindPart(first,S::TomatoStems);
            const float size=visual.GetCropRenderScale(before);
            const auto center=visual.GetTileCenter(grid,0);
            assert(Near(largeFruit.position.x,center.x-.171f*size) && Near(smallFruit.position.y,center.y+.540f*size));
            harvest.Update(grid,.14f); const auto loose=harvest.GetParts(grid,0);
            assert(FindPart(loose,S::PickedTomatoLarge).position.y<largeFruit.position.y && Near(FindPart(loose,S::PickedTomatoSmall).position.y,smallFruit.position.y));
            assert(Near(FindPart(loose,S::TomatoStems).position.y,vine.position.y));
            harvest.Update(grid,farm::harvest::kContact-.14f);
            const auto contact=harvest.GetParts(grid,0);
            assert(Near(FindPart(contact,S::PickedTomatoLarge).roll,0.f));
            harvest.Update(grid,.09f);
            const auto rolling=harvest.GetParts(grid,0);
            assert(FindPart(rolling,S::PickedTomatoLarge).position.x<FindPart(contact,S::PickedTomatoLarge).position.x);
            assert(FindPart(rolling,S::PickedTomatoLarge).roll>0.f && Near(FindPart(rolling,S::PickedTomatoSmall).roll,0.f));
            harvest.Update(grid,.31f); const auto packed=harvest.GetParts(grid,0);
            const auto basket=FindPart(packed,S::HarvestBasket);
            const auto left=FindPart(packed,S::PickedTomatoLarge),right=FindPart(packed,S::PickedTomatoSmall);
            assert(left.position.x<basket.position.x && right.position.x>basket.position.x);
            assert(left.roll>0.f && right.roll<0.f && Near(basket.roll,0.f));
            assert(basket.position.x-left.position.x+.505f*left.scale.x<.5f*basket.scale.x);
            assert(right.position.x-basket.position.x+.382f*right.scale.x<.5f*basket.scale.x);
            assert(right.position.x-left.position.x>left.scale.x*.505f+right.scale.x*.382f);
            for(const auto& fruit:{left,right}) {
                assert(Near(fruit.position.z,basket.position.z) && fruit.position.y>basket.position.y);
                assert(fruit.position.y<basket.position.y+farm::harvest::kGripHeight*basket.scale.y);
            }
        } else {
            const auto start=FindPart(first,S::Pumpkin);
            harvest.Update(grid,.3f);
            assert(FindPart(harvest.GetParts(grid,0),S::Pumpkin).position.y>start.position.y);
        }
        harvest.Update(grid,2.f); assert(harvest.GetActiveCount(grid)==0);
    }
    {
        farm::FarmGrid field; assert(field.Initialize(3,3));
        farm::FarmVisualSystem visual; visual.Initialize({});
        farm::FarmHarvestVisualSystem effects;
        farm::FarmTile before; before.state=farm::FarmTileState::Planted;
        before.crop=farm::CropType::Carrot; before.growth=1; before.heightLevel=1;
        farm::FarmTile after; after.state=farm::FarmTileState::Tilled; after.heightLevel=1;
        for(int i=0;i<9;++i) assert(field.SetTile(i,after));
        FarmToolActionResult result; result.status=FarmToolActionStatus::Harvested; result.tileIndex=0;
        assert(!effects.Start(result,field,visual)); // Read-only evaluation has no committed payload.
        result.harvestedTile=before;
        assert(effects.Start(result,field,visual) && !effects.Start(result,field,visual));
        const auto initial=effects.GetParts(field,0); assert(initial.count==2);
        using S=farm::FarmMeshShape;
        effects.Update(field,0); effects.Update(field,-1);
        effects.Update(field,std::numeric_limits<float>::quiet_NaN());
        assert(Near(effects.GetParts(field,0).parts[0].position.y,initial.parts[0].position.y));
        effects.Update(field,farm::harvest::kRelease);
        const auto pulled=effects.GetParts(field,0);
        auto root=FindPart(pulled,S::Carrot);
        assert(root.position.y>visual.GetTileCenter(field,0).y && Near(root.position.x,initial.parts[0].position.x));
        assert(Near(root.scale.y,initial.parts[0].scale.y));
        effects.Update(field,farm::harvest::kFlight+farm::harvest::kSettle);
        auto packed=effects.GetParts(field,0);
        auto basket=FindPart(packed,S::HarvestBasket);
        root=FindPart(packed,S::Carrot);
        const auto leaf=FindPart(packed,S::CarrotLeaves);
        assert(Near(root.position.x,basket.position.x) && Near(root.position.z,basket.position.z));
        assert(root.position.y>basket.position.y && root.position.y<basket.position.y+farm::harvest::kGripHeight*basket.scale.y);
        assert(Near(leaf.position.y,root.position.y+std::cos(root.roll)*root.scale.y*.96f));
        assert(Near(leaf.position.x,root.position.x-std::sin(root.roll)*root.scale.y*.96f));
        effects.Update(field,farm::harvest::kPack+.125f-farm::harvest::kRelease-farm::harvest::kFlight-farm::harvest::kSettle);
        const auto shrinking=effects.GetParts(field,0);
        assert(Near(FindPart(shrinking,S::Carrot).scale.y,root.scale.y*.5f));
        assert(Near(FindPart(shrinking,S::HarvestBasket).scale.x,basket.scale.x*.5f));
        effects.Update(field,effects.kDuration); assert(effects.GetActiveCount(field)==0);
        for(int i=0;i<9;++i) {
            result.tileIndex=i; assert(effects.Start(result,field,visual)); effects.Update(field,0.01f);
        }
        assert(effects.GetActiveCount(field)==effects.kCapacity);
        assert(effects.GetParts(field,effects.kCapacity).count==0);
        assert(field.SetTile(8,before)); // Undo/replant invalidates that tile immediately.
        assert(effects.GetActiveCount(field)==effects.kCapacity-1);
        assert(field.Initialize(3,3)); assert(effects.GetActiveCount(field)==0);
        assert(field.SetTile(0,after)); result.tileIndex=0;
        assert(effects.Start(result,field,visual)); effects.Clear(); assert(effects.GetActiveCount(field)==0);
        result.status=FarmToolActionStatus::InventoryFull; assert(!effects.Start(result,field,visual));
        result.status=FarmToolActionStatus::Harvested; result.tileIndex=-1; assert(!effects.Start(result,field,visual));
        std::cout << "PASS: root pull, staggered fruit detachment, basket seating, pause, payload, capacity and stale tile guards\n";
    }
    farm::FarmGrid grid; assert(grid.Initialize(1,1));
    farm::FarmVisualSystem visual; visual.Initialize({});
    farm::FarmIrrigationSystem irrigation; irrigation.Initialize();
    for(auto crop:{farm::CropType::Carrot,farm::CropType::Tomato}) {
        farm::FarmTile mature; mature.crop=crop; mature.state=farm::FarmTileState::Planted; mature.growth=1;
        mature.careHistory.goodSeconds=mature.careHistory.efficiencySeconds=10;
        mature.careHistory.nutrientGrowth=mature.careHistory.nutrientSupply=1;
        farm::FarmTile bed{}; bed.state=farm::FarmTileState::Tilled;
        assert(grid.SetTile(0,bed) && visual.GetCropRenderScale(mature)>1.f);
        FarmToolActionResult action{}; action.status=FarmToolActionStatus::Harvested; action.tileIndex=0; action.harvestedTile=mature;
        farm::FarmHarvestVisualSystem fx; assert(fx.Start(action,grid,visual));
        fx.Update(grid,1.35f);
        using S=farm::FarmMeshShape;
        const auto settled=fx.GetParts(grid,0); const auto bin=FindPart(settled,S::HarvestBasket);
        if(crop==farm::CropType::Tomato) {
            for(auto shape:{S::PickedTomatoLarge,S::PickedTomatoSmall}) {
                const auto fruit=FindPart(settled,shape); const bool big=shape==S::PickedTomatoLarge;
                const auto radius=fruit.scale.x*(big?.505f:.382f);
                assert(std::abs(fruit.position.x-bin.position.x)+radius<.50f*bin.scale.x);
                CheckTomatoInCrate(fruit,bin,big);
                assert(fruit.position.y-(big?.282f:.215f)*fruit.scale.y>=bin.position.y+.075f*bin.scale.y);
            }
        } else {
            const auto root=FindPart(settled,S::Carrot);
            assert(Near(root.position.x,bin.position.x) && root.scale.y<=.811f*bin.scale.y);
        }
    }
    for (auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot}) {
        farm::FarmTile tile;
        tile.state = farm::FarmTileState::Planted; tile.crop = crop; tile.growth = 1;
        tile.heightLevel = 2;
        tile.careHistory.goodSeconds = 10; tile.careHistory.efficiencySeconds = 10;
        tile.careHistory.nutrientGrowth = 0.95f; tile.careHistory.nutrientSupply = 0.95f;
        assert(grid.SetTile(0,tile));
        const auto good = visual.GetTileVisualData(grid,0);
        const auto mesh = farm::BuildFarmCropMeshParts(grid,0,visual);
        const auto leafShape = crop == farm::CropType::Carrot ? farm::FarmMeshShape::CarrotLeaves : farm::FarmMeshShape::Leaves;
        assert(mesh.count == 2 && mesh.parts[1].shape == leafShape);
        assert(mesh.parts[0].shape == (crop == farm::CropType::Carrot ? farm::FarmMeshShape::Carrot : farm::FarmMeshShape::Turnip));
        const float buriedFraction = crop == farm::CropType::Carrot ? 0.84f : 0.50f;
        assert(Near(mesh.parts[0].position.y + mesh.parts[0].scale.y * buriedFraction, good.center.y));
        assert(mesh.parts[0].position.y < good.center.y);
        assert(mesh.parts[0].position.y + mesh.parts[0].scale.y > good.center.y);
        assert(Near(mesh.parts[1].position.y, mesh.parts[0].position.y + mesh.parts[0].scale.y * 0.96f));
        for (std::size_t i=0; i<mesh.count; ++i) {
            const auto& p = mesh.parts[i];
            assert(p.scale.x > 0 && p.scale.y > 0 && p.scale.z > 0);
            assert(p.scale.x <= good.halfExtent*0.90f+0.0001f);
            assert(p.scale.z <= good.halfExtent*0.90f+0.0001f);
            assert(p.surface == farm::FarmMeshSurface::Crop && !p.water);
        }
        assert(good.cropScale > 1 && good.cropScale * 0.32f <= good.halfExtent * 0.90f + 0.0001f);
        assert(FarmCropSizeSystem{}.Evaluate(tile).multiplier == 2); // Rendering cap does not change gameplay.
        lines.clear(); sphereRadius=0;
        visual.Draw(grid,irrigation,{},*LineDrawer::GetInstance(),nullptr,false);
        bool foundStem = false;
        for (const auto& line : lines) {
            assert(std::isfinite(line.a.x) && std::isfinite(line.a.y) && std::isfinite(line.b.y));
            if (Near(line.a.y,good.cropAnchor.y) && Near(line.a.x,good.center.x) &&
                Near(line.b.y,good.cropAnchor.y+0.84f*good.cropScale)) {
                foundStem = true;
            }
        }
        assert(foundStem);
        if (crop == farm::CropType::TestCrop) {
            assert(Near(sphereRadius,0.17f*good.cropScale));
            assert(Near(sphereCenter.y-sphereRadius,good.center.y));
        }
        lines.clear(); sphereRadius = 0;
        visual.Draw(grid,irrigation,{},*LineDrawer::GetInstance(),nullptr,false,false);
        assert(lines.empty() && sphereRadius == 0); // Solid rendering disables the duplicate wire crop.
        assert(grid.GetTile(0)->growth == tile.growth && grid.GetTile(0)->careHistory.nutrientSupply == tile.careHistory.nutrientSupply);
        tile.careHistory.nutrientSupply = 0;
        assert(grid.SetTile(0,tile));
        const auto poor = visual.GetTileVisualData(grid,0);
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).parts[0].scale.x < mesh.parts[0].scale.x);
        assert(poor.cropScale == 0.5f && poor.cropScale < good.cropScale);
        assert(poor.cropAnchor.y == good.cropAnchor.y);
        tile.careHistory = {}; assert(grid.SetTile(0,tile));
        assert(visual.GetTileVisualData(grid,0).cropScale == 1);
        tile.growth = 0.05f;
        tile.careHistory.goodSeconds=1; tile.careHistory.efficiencySeconds=1;
        tile.careHistory.nutrientGrowth=0.01f; tile.careHistory.nutrientSupply=0.01f;
        assert(grid.SetTile(0,tile));
        assert(Near(visual.GetTileVisualData(grid,0).cropScale,1.05f));
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 1);
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).parts[0].shape == leafShape);
        assert(!EditFarmTile(grid, 0, [&](auto& candidate) { candidate.growth = std::numeric_limits<float>::quiet_NaN(); }));
        assert(std::isfinite(visual.GetTileVisualData(grid,0).cropScale));
        assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 1);
    }
    assert(!visual.GetTileVisualData(grid,-1).valid && !visual.GetTileVisualData(grid,1).valid);
    assert(farm::BuildFarmCropMeshParts(grid,-1,visual).count == 0);
    assert(farm::BuildFarmCropMeshParts(grid,1,visual).count == 0);
    {
        farm::FarmGrid slopes; assert(slopes.Initialize(3,3));
        for (auto crop : {farm::CropType::Carrot, farm::CropType::TestCrop})
        for (int height : {0,1,2}) for (float growth : {0.05f,0.4f,0.8f,1.f})
        for (float care : {0.f,1.f}) for (float tileSize : {0.1f,1.f,2.f}) {
            farm::FarmVisualLayout layout; layout.tileSize=tileSize;
            farm::FarmVisualSystem scaled; scaled.Initialize(layout);
            farm::FarmTile cropTile; cropTile.state=farm::FarmTileState::Planted; cropTile.crop=crop;
            cropTile.growth=growth; cropTile.heightLevel=height;
            cropTile.careHistory.goodSeconds=10; cropTile.careHistory.efficiencySeconds=10;
            cropTile.careHistory.nutrientGrowth=1; cropTile.careHistory.nutrientSupply=care;
            assert(slopes.SetTile(4,cropTile));
            farm::FarmTile neighbor; neighbor.heightLevel=height==0 ? 2 : 0;
            assert(slopes.SetTile(3,neighbor));
            assert(slopes.GetTile(3)->heightLevel==neighbor.heightLevel);
            const auto surface=farm::BuildFarmSoilSurface(slopes,4,scaled);
            const auto data=scaled.GetTileVisualData(slopes,4);
            const auto parts=farm::BuildFarmCropMeshParts(slopes,4,scaled);
            assert(surface.valid && parts.count>0 && parts.count<=2);
            // Neighbour height changes only the border slopes; planting remains on the central plateau.
            assert(Near(surface.At(1,1).y,data.center.y) && Near(surface.At(2,2).y,data.center.y));
            const auto& leaves=parts.parts[parts.count-1];
            assert(leaves.position.y>=data.center.y);
            if(parts.count==2) {
                const auto& root=parts.parts[0];
                const float buried=crop==farm::CropType::Carrot ? 0.84f : 0.5f;
                assert(Near((data.center.y-root.position.y)/root.scale.y,buried));
                assert(leaves.position.y<=root.position.y+root.scale.y);
                assert(Near(leaves.position.y,root.position.y+root.scale.y*0.96f));
                assert(root.scale.x< data.halfExtent*0.86f); // Root footprint stays away from ramps.
            } else assert(Near(leaves.position.y,data.cropAnchor.y));
            for(std::size_t i=0;i<parts.count;++i) {
                const auto& part=parts.parts[i];
                assert(std::isfinite(part.position.x) && std::isfinite(part.position.y) && std::isfinite(part.position.z));
                assert(part.scale.x>0 && part.scale.y>0 && part.scale.z>0);
            }
            assert(slopes.GetTile(4)->growth==growth && slopes.GetTile(4)->heightLevel==height);
        }
    }
    farm::FarmTile empty; assert(grid.SetTile(0,empty));
    assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 0);
    empty.feature = farm::FarmTileFeature::Canal; assert(grid.SetTile(0,empty));
    assert(farm::BuildFarmCropMeshParts(grid,0,visual).count == 0);
    std::cout << "PASS: planted root burial, leaf attachment, slope plateau, growth/size bounds, legacy wire and invalid inputs\n";
}
