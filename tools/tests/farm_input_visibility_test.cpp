#include "farm_test_edit.h"
#include "io/MouseButtonEdges.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/render/FarmFarmerModel.h"
#include "farm/system/FarmTerrainQuerySystem.h"
#include "3d/LineDrawer.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <limits>

// GPU debug drawing is outside this CPU test. Abort if a tested path accidentally uses it.
void LineDrawer::DrawLine(const Vector3&, const Vector3&, const Vector4&) { std::abort(); }
void LineDrawer::DrawWireCube(const Vector3&, float, const Vector4&) { std::abort(); }
void LineDrawer::DrawWireSphere(const Vector3&, float, const Vector4&, uint32_t) { std::abort(); }

int main() {
    MouseButtonEdges edges;
    for (int button = 0; button < MouseButtonEdges::kCount; ++button) {
        edges.Reset();
        edges.Apply(button, true); edges.Apply(button, false);
        assert(edges.Pressed(button) && edges.Released(button) && !edges.Held(button));
        edges.BeginFrame();
        assert(!edges.Pressed(button) && !edges.Released(button) && !edges.Held(button));
        edges.Apply(button, true);
        edges.BeginFrame(); edges.Apply(button, true);
        assert(edges.Held(button) && !edges.Pressed(button));
        edges.Apply(button, false);
        assert(edges.Released(button) && !edges.Held(button));
        edges.BeginFrame();
        for (int i = 0; i < 100; ++i) { edges.Apply(button, true); edges.Apply(button, false); }
        assert(edges.Pressed(button) && edges.Released(button) && !edges.Held(button));
        edges.Reset(); // Lost focus discards both held state and pending edges.
        assert(!edges.Pressed(button) && !edges.Released(button) && !edges.Held(button));
        std::array<bool, MouseButtonEdges::kCount> restored{}; restored[button] = true;
        edges.Synchronize(restored);
        assert(!edges.Held(button) && !edges.Pressed(button) && !edges.Released(button));
        edges.Apply(button, true);
        assert(!edges.Pressed(button) && !edges.Held(button));
        edges.Apply(button, false);
        assert(!edges.Released(button));
        edges.BeginFrame(); edges.Apply(button, true);
        assert(edges.Pressed(button));
    }
    for (int invalid : {-1, MouseButtonEdges::kCount, (std::numeric_limits<int>::max)()}) {
        edges.Apply(invalid, true);
        assert(!edges.Held(invalid) && !edges.Pressed(invalid) && !edges.Released(invalid));
    }
    std::cout << "PASS: short pulse, held/repeat, per-frame reset, focus resync, invalid button\n";

    farm::FarmGrid grid;
    assert(grid.Initialize(5,4));
    farm::FarmVisualSystem visual;
    visual.Initialize({});
    {
        farm::FarmerPose pose{};
        pose.furrow={0,0,0,0,.22f};
        assert(!farm::BuildFurrowMark(grid,visual,pose));
        const auto carried=farm::BuildFarmerModel(pose);
        pose.bladeDepth=1.f;
        const auto planted=farm::BuildFarmerModel(pose);
        const auto furrow=farm::BuildFurrowMark(grid,visual,pose);
        assert(furrow && (*furrow)[0].scale.z>0.1f);
        assert(planted.back().pitch!=carried.back().pitch && (*furrow)[1].position.y>(*furrow)[0].position.y);
        pose.furrow={};
        assert(!farm::BuildFurrowMark(grid,visual,pose));
        std::cout<<"PASS: planted hoe, incision and displaced lip only for active soil stroke\n";
    }
    std::size_t total = 0;
    for (int i=0; i<grid.GetTileCount(); ++i) {
        const auto parts = farm::BuildFarmTileMeshParts(grid,i,visual);
        assert(parts.count == 6); total += parts.count;
    }
    assert(total == 120); // 40 existing soil parts + 80 no-shadow markers for the default field.
    {
        farm::FarmGrid pool;
        assert(pool.Initialize(5,4));
        for (int i=0;i<20;++i) assert(EditFarmTile(pool,i,[i](auto& t){
            t.feature=i==0 ? farm::FarmTileFeature::WaterSource : farm::FarmTileFeature::Canal;
            t.waterAmount=1.f;
        }));
        const float pitch=visual.GetLayout().tileSize+visual.GetLayout().tileGap;
        for (int z=0;z<3;++z) for (int x=0;x<4;++x) {
            const int start=z*5+x;
            for (int dz=0;dz<2;++dz) for (int dx=0;dx<2;++dx) {
                const int i=start+dx+5*dz;
                const auto tile=visual.GetTileVisualData(pool,i);
                const float nearCorner=(tile.waterHalfExtent+pitch*.5f)*.5f;
                const float px=tile.center.x+(dx?-1:1)*nearCorner;
                const float pz=tile.center.z+(dz?-1:1)*nearCorner;
                const auto mesh=farm::BuildFarmTileMeshParts(pool,i,visual);
                int owners=0;
                for (size_t j=0;j<mesh.count;++j) {
                    const auto& p=mesh.parts[j];
                    if (p.water && p.shape==farm::FarmMeshShape::Box &&
                        std::abs(p.position.x-px)<p.scale.x && std::abs(p.position.z-pz)<p.scale.z) ++owners;
                }
                assert(owners==1 && mesh.count<mesh.parts.size());
            }
        }
        std::cout<<"PASS: 12 flooded junctions, one water surface on all 48 quarters\n";
        const int four[]{6,7,11,12};
        assert(EditFarmTile(pool,12,[](auto& t){t.heightLevel=1;t.waterAmount=.4f;}));
        float waterTop=0,bedTop=0;
        for (int i:four) {
            const auto data=visual.GetTileVisualData(pool,i);
            waterTop+=.25f*data.waterSurfaceCenter.y;
            bedTop+=.25f*(data.center.y+visual.GetLayout().waterBottomOffset);
        }
        auto vertex=visual.GetTileCenter(pool,6);
        vertex.x+=pitch*.5f; vertex.z+=pitch*.5f;
        for (int i:four) {
            const auto mesh=farm::BuildFarmTileMeshParts(pool,i,visual);
            int water=0,bed=0;
            for (size_t j=0;j<mesh.count;++j) {
                const auto& p=mesh.parts[j];
                const float dx=vertex.x-p.position.x,dz=vertex.z-p.position.z;
                if (std::abs(dx)>p.scale.x+.00001f || std::abs(dz)>p.scale.z+.00001f) continue;
                const float side=dx/p.scale.x+dz/p.scale.z;
                if ((p.shape==farm::FarmMeshShape::TriangleLower && side>.00001f) ||
                    (p.shape==farm::FarmMeshShape::TriangleUpper && side<-.00001f)) continue;
                const float top=p.position.y+p.scale.y+p.slope.x*dx+p.slope.y*dz;
                if (p.water) {assert(std::abs(top-waterTop)<.00001f);++water;}
                if (p.surface==farm::FarmMeshSurface::CanalBed) {assert(std::abs(top-bedTop)<.00001f);++bed;}
            }
            assert(water && bed);
        }
        assert(EditFarmTile(pool,12,[](auto& t){t.waterAmount=0.f;}));
        const auto drying=farm::BuildFarmTileMeshParts(pool,6,visual);
        for (size_t j=0;j<drying.count;++j) {
            const auto& p=drying.parts[j];
            assert(!p.water || std::abs(vertex.x-p.position.x)>p.scale.x+.00001f ||
                   std::abs(vertex.z-p.position.z)>p.scale.z+.00001f);
        }
        std::cout<<"PASS: graded bed and water junction agree on four sides; dry diagonal not flooded\n";
    }
    {
        farm::FarmGrid strip;
        assert(strip.Initialize(3,1));
        for (int i=0;i<2;++i)
            assert(EditFarmTile(strip,i,[i](auto& t){ t.feature=farm::FarmTileFeature::Canal; t.heightLevel=i; }));
        assert(EditFarmTile(strip,2,[](auto& t){ t.state=farm::FarmTileState::Tilled; }));
        const auto soil=farm::BuildFarmTileMeshParts(strip,2,visual);
        int ridges=0;
        for (std::size_t i=0;i<soil.count;++i) ridges+=soil.parts[i].shape==farm::FarmMeshShape::SoilRidge;
        assert(ridges==3);
        const auto canal=farm::BuildFarmTileMeshParts(strip,0,visual);
        const auto center=visual.GetTileCenter(strip,0);
        int banks=0;
        for (std::size_t i=0;i<canal.count;++i) {
            const auto& part=canal.parts[i];
            if (part.surface!=farm::FarmMeshSurface::CanalBank) continue;
            assert(part.shape==farm::FarmMeshShape::BankStone);
            assert(!(part.position.x>center.x && part.position.z==center.z));
            const auto ground=farm::FarmTerrainQuerySystem::SampleGround(strip,visual,part.position,{0,0});
            assert(ground.valid && ground.kind==farm::FarmGroundKind::CanalBank);
            assert(std::abs(ground.height-part.position.y-part.scale.y)<.0001f);
            ++banks;
        }
        assert(banks==9);
        assert(farm::FarmTerrainQuerySystem::SampleGround(strip,visual,center,{}).kind==farm::FarmGroundKind::CanalBed);
        std::cout<<"PASS: three soil ridges; open edge, sloped bank crests and bed support\n";
    }
    assert(farm::BuildFarmTileMeshParts(grid,-1,visual).count == 0);
    for (unsigned mask=0;mask<16;++mask) {
        farm::FarmGrid turn;
        assert(turn.Initialize(3,3));
        const int ends[]{3,5,1,7};
        const auto channel=[](auto& t) { t.feature=farm::FarmTileFeature::Canal; t.waterAmount=.5f; };
        assert(EditFarmTile(turn,4,channel));
        for (int side=0;side<4;++side)
            if ((mask>>side)&1u) assert(EditFarmTile(turn,ends[side],channel));
        const bool elbow=(mask==5 || mask==6 || mask==9 || mask==10);
        const auto mesh=farm::BuildFarmTileMeshParts(turn,4,visual);
        int water=0, lining=0, fillet=0;
        for (std::size_t i=0;i<mesh.count;++i) {
            const auto& p=mesh.parts[i];
            if (p.shape==farm::FarmMeshShape::BendWater) {
                assert(p.water);
                const float x=std::cos(p.yaw)+std::sin(p.yaw), z=std::cos(p.yaw)-std::sin(p.yaw);
                assert((x>0)==bool(mask&1) && (z>0)==bool(mask&4));
                ++water;
            }
            lining+=p.shape==farm::FarmMeshShape::BendLining;
            fillet+=p.shape==farm::FarmMeshShape::BendFillet;
        }
        const int expected=elbow?1:0;
        assert(water==expected && lining==expected && fillet==expected && mesh.count<mesh.parts.size());
        if (elbow) {
            const int ex=mask&1?0:1, ez=mask&4?2:3;
            assert(EditFarmTile(turn,ends[ex],[](auto& t){ t.heightLevel=1; t.waterAmount=.8f; }));
            assert(EditFarmTile(turn,ends[ez],[](auto& t){ t.waterAmount=.15f; }));
            const auto graded=farm::BuildFarmTileMeshParts(turn,4,visual);
            const auto mid=visual.GetTileVisualData(turn,4);
            const float run=(visual.GetLayout().tileSize+visual.GetLayout().tileGap)*.5f-mid.waterHalfExtent;
            for (std::size_t i=0;i<graded.count;++i) {
                const auto& p=graded.parts[i];
                if (p.shape!=farm::FarmMeshShape::BendFillet) continue;
                const float c=std::cos(p.yaw), s=std::sin(p.yaw);
                const float gx=c*p.slope.x+s*p.slope.y, gz=c*p.slope.y-s*p.slope.x;
                for (int edge : {ex,ez}) {
                    const auto end=visual.GetTileVisualData(turn,ends[edge]);
                    const float slopeAlong=(edge<2?gx:gz)*(edge%2?1.f:-1.f);
                    assert(std::abs(slopeAlong*run-.5f*(end.waterSurfaceCenter.y-mid.waterSurfaceCenter.y))<.00001f);
                }
            }
            assert(EditFarmTile(turn,4,[](auto& t){ t.waterAmount=0; }));
            const auto dry=farm::BuildFarmTileMeshParts(turn,4,visual);
            for (std::size_t i=0;i<dry.count;++i) assert(!dry.parts[i].water);
        }
    }
    assert(farm::BuildFarmTileMeshParts(grid,20,visual).count == 0);
    for (int invalid : {-1, 20, (std::numeric_limits<int>::max)()})
        assert(farm::BuildFarmSelectionMeshParts(grid,invalid,visual).count == 0 &&
            farm::BuildFarmHoverMeshParts(grid,invalid,visual).count == 0);
    assert(grid.Initialize(3,3));
    for (int pattern=0; pattern<243; ++pattern) {
        int levels = pattern;
        for (int i=0; i<9; ++i) {
            assert(EditFarmTile(grid,i,[&](auto& tile) { tile.heightLevel = levels%3; }));
            levels = levels/3 + (i%2);
        }
        for (int index=0; index<9; ++index) {
            const auto center = visual.GetTileCenter(grid,index);
            const auto parts = farm::BuildFarmTileMeshParts(grid,index,visual);
            assert(parts.count > 0 && parts.count <= 30);
            int borders = 0;
            for (std::size_t i=0; i<parts.count; ++i) {
                const auto& part = parts.parts[i];
                assert(std::isfinite(part.position.y) && std::isfinite(part.slope.x) && std::isfinite(part.slope.y));
                if (part.surface != farm::FarmMeshSurface::SoilBoundary) continue;
                ++borders;
                assert(!part.water && part.shape == farm::FarmMeshShape::Box);
                assert(part.slope.x == 0 && part.slope.y == 0 && part.position.y > center.y);
                const float plateau = visual.GetLayout().tileSize*0.5f*0.86f;
                assert(std::abs(part.position.x-center.x)+part.scale.x <= plateau+0.00001f);
                assert(std::abs(part.position.z-center.z)+part.scale.z <= plateau+0.00001f);
            }
            assert(borders == 4);
            const auto marker = farm::BuildFarmSelectionMeshParts(grid,index,visual);
            const auto hover = farm::BuildFarmHoverMeshParts(grid,index,visual);
            assert(marker.count == 8 && marker.count == marker.parts.size());
            assert(hover.count == 8 && hover.count == hover.parts.size());
            for (const auto& part : marker.parts) {
                assert(part.surface == farm::FarmMeshSurface::Selection && !part.water);
                assert(part.scale.x > 0 && part.scale.y > 0 && part.scale.z > 0);
                assert(std::isfinite(part.position.y) && part.position.y-part.scale.y > center.y+0.024f);
                const float plateau = visual.GetLayout().tileSize*0.5f*0.86f;
                assert(std::abs(part.position.x-center.x)+part.scale.x < plateau);
                assert(std::abs(part.position.z-center.z)+part.scale.z < plateau);
            }
            for (std::size_t i=0; i<hover.count; ++i) {
                const auto& part = hover.parts[i];
                assert(part.surface == farm::FarmMeshSurface::Hover && !part.water);
                assert(part.scale.x > 0 && part.scale.y > 0 && part.scale.z > 0);
                assert(std::isfinite(part.position.y) && part.position.y > marker.parts[i].position.y);
                assert(part.color.x < marker.parts[i].color.x && part.color.z > marker.parts[i].color.z);
            }
            int picked = -1;
            assert(visual.TryPickTile(grid,{center.x,10,center.z},{0,-1,0},picked) && picked == index);
        }
    }
    for (const auto feature : {farm::FarmTileFeature::None, farm::FarmTileFeature::Canal, farm::FarmTileFeature::WaterSource}) {
        assert(EditFarmTile(grid,4,[&](auto& tile) {
            tile.feature = feature; tile.state = feature == farm::FarmTileFeature::None ? farm::FarmTileState::Tilled : farm::FarmTileState::Empty;
        }));
        const auto* tile = grid.GetTile(4);
        const auto parts = farm::BuildFarmTileMeshParts(grid,4,visual);
        for (std::size_t i=0; i<parts.count; ++i) assert(parts.parts[i].surface != farm::FarmMeshSurface::SoilBoundary);
        const auto marker = farm::BuildFarmSelectionMeshParts(grid,4,visual);
        const auto hover = farm::BuildFarmHoverMeshParts(grid,4,visual);
        assert(marker.count == 8 && hover.count == 8 && tile->feature == feature && tile->state == (feature == farm::FarmTileFeature::None ? farm::FarmTileState::Tilled : farm::FarmTileState::Empty));
        if (feature != farm::FarmTileFeature::None) {
            const auto& layout = visual.GetLayout();
            const float rim = visual.GetTileCenter(grid,4).y + layout.waterBottomOffset + layout.waterMaxDepth + 0.045f;
            for (const auto& part : marker.parts) assert(part.position.y-part.scale.y > rim);
            for (const auto& part : hover.parts) assert(part.position.y-part.scale.y > rim);
        }
    }
    std::cout << "PASS: mesh bounds/capacity, boundaries, selection/hover corners, canal clearance, tile picking\n";
}
