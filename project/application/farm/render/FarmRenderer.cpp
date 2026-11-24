#include "farm/render/FarmRenderer.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmHarvestVisualSystem.h"
#include "farm/system/FarmPestVisualSystem.h"
#include "farm/render/FarmMaterialStyle.h"
#include "3d/Object3d.h"
#include "3d/ModelManager.h"
#include <filesystem>

namespace farm {

FarmRenderer::FarmRenderer() = default;
FarmRenderer::~FarmRenderer() = default;

bool FarmRenderer::Initialize(Object3dCommon* common, ModelManager* models, Texture2DHandle whiteTexture) {
	if (IsReady()) { return true; }
	if (!common || !models) return false;
	std::array<Model*,kFarmMeshPaths.size()> loaded{};
	bool cropsReady=true;
	for (std::size_t i=0;i<loaded.size();++i) {
		const auto shape=static_cast<FarmMeshShape>(i);
		const bool crop=shape>=FarmMeshShape::Turnip && shape<=FarmMeshShape::PumpkinVines;
		const char* path=kFarmMeshPaths[i];
		std::error_code error;
		if (std::filesystem::is_regular_file(std::filesystem::path("Resources")/path,error)) {
			models->LoadModel(path);
			loaded[i]=models->GetModel(path);
			if (loaded[i] && loaded[i]->HasSkinCluster()) loaded[i]=nullptr;
		}
		if (!loaded[i]) {
			if (!crop) return false;
			cropsReady=false;
		}
	}
	meshModels_=loaded;
	cropsReady_=cropsReady;
	common_=common;
    liveCrops_.reserve(kMaximumCropParts);
	whiteTexture_=whiteTexture;
    constexpr size_t kToolParts=4+2; // Four simultaneous cans and the handle/blade of one hoe.
    constexpr size_t kReserve=kMaximumParts+kMaximumTargetParts+kMaximumCropParts+kMaximumHarvestParts+FarmPestMeshParts::kCapacity+kToolParts;
	parts_.reserve(kReserve);
	objects_.reserve(kReserve);
	return true;
}

void FarmRenderer::Prepare(const FarmGrid& grid, const FarmVisualSystem& visual, Camera* camera,
	int hoveredTileIndex, const FarmHarvestVisualSystem* harvest, bool showSelection, const FarmPestVisualSystem* pests,
	const vegetation::WindFrame& wind,const FarmBrushPlaySystem* brush,const FarmActionParticles* actions) {
    liveCrops_.clear(); preparedBoard_=nullptr;
	parts_.clear();
	lastDrawTileCount_ = 0;
	limitExceeded_ = false;
	if (!IsReady() || !visible_ || !camera) { return; }
	if (grid.GetTileCount() > static_cast<int>(kMaximumParts)) { limitExceeded_ = true; return; }
	std::size_t terrainCount = 0;
	std::size_t plantCount = 0;
    const auto* strokes=actions?actions->HoeStrokes(grid):nullptr;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		auto tileParts = BuildFarmTileMeshParts(grid, index, visual,actions?actions->PendingWater(grid,index):0.f,
            strokes && strokes->WaitingForHit(index));
        if(actions && grid.GetTile(index)->feature==FarmTileFeature::Canal) {
            const auto front=actions->WaterFront(grid,index);
            if(front.parent>=0) {
                const auto center=visual.GetTileCenter(grid,index);
                RevealFarmWater(tileParts,center,visual.GetTileCenter(grid,front.parent),
                    front.outlet>=0?visual.GetTileCenter(grid,front.outlet):center,
                    (visual.GetLayout().tileSize+visual.GetLayout().tileGap)*.5f,front.fraction);
            }
        }
		if (terrainCount + tileParts.count > kMaximumParts) {
			parts_.clear(); lastDrawTileCount_ = 0; limitExceeded_ = true; return;
		}
		parts_.insert(parts_.end(), tileParts.parts.begin(), tileParts.parts.begin() + tileParts.count);
		terrainCount += tileParts.count;
		const auto repellent = BuildFarmRepellentMeshParts(grid, index, visual);
		auto crops = HasCropMeshes() ? BuildFarmCropMeshParts(grid, index, visual) : FarmCropMeshParts{};
        if (actions && crops.count) {
            const float emergence=actions->SproutScale(grid,index,grid.GetTile(index)->crop);
            if (emergence<.02f) crops.count=0;
            else if (emergence<1.f) for (size_t i=0;i<crops.count;++i) crops.parts[i].scale=crops.parts[i].scale*emergence;
        }
		if (plantCount + repellent.count + crops.count > kMaximumCropParts) {
			parts_.clear(); lastDrawTileCount_ = 0; limitExceeded_ = true; return;
		}
		parts_.insert(parts_.end(), repellent.parts.begin(), repellent.parts.begin() + repellent.count);
        for(size_t i=0;i<crops.count;++i) liveCrops_.push_back({parts_.size()+i,index,grid.GetTile(index)->crop});
		parts_.insert(parts_.end(), crops.parts.begin(), crops.parts.begin() + crops.count);
		plantCount += repellent.count + crops.count;
		lastDrawTileCount_ += tileParts.count > 0 ? 1 : 0;
	}
	const auto livePartEnd = parts_.size();
    if(const auto* stroke=strokes?strokes->Active():nullptr) {
        const auto pose=hoeing::Pose(*stroke);
        if(pose.scale>=.005f) {
            FarmMeshPart tool{};
            tool.position=pose.grip; tool.roll=pose.roll; tool.scale={pose.scale,pose.scale,pose.scale};
            tool.shape=FarmMeshShape::HoeHandle; tool.color={.61f,.36f,.16f,1.f};
            parts_.push_back(tool);
            tool.shape=FarmMeshShape::HoeBlade; tool.color={.42f,.52f,.55f,1.f};
            parts_.push_back(tool);
        }
    }
    if (actions) for(const auto& pour:actions->Pours(grid)) {
        if(pour.tile<0 || pour.age>=watering::kReleaseEnd+watering::kRetract) continue;
        const auto pose=watering::Pose(pour,pour.age);
        if(pose.scale<.005f) continue;
        FarmMeshPart can{};
        can.shape=FarmMeshShape::WateringCan; can.position=pose.pivot; can.roll=pose.roll;
        can.scale={pose.scale,pose.scale,pose.scale}; can.color={.40f,.64f,.65f,1.f};
        parts_.push_back(can);
    }
	static_assert(kMaximumHarvestParts == FarmHarvestVisualSystem::kCapacity * harvest::kMaxParts);
	if (harvest && HasCropMeshes()) {
		for (std::size_t i = 0; i < FarmHarvestVisualSystem::kCapacity; ++i) {
			const auto effect = harvest->GetParts(grid, i);
			parts_.insert(parts_.end(), effect.parts.begin(), effect.parts.begin() + effect.count);
		}
	}
	if (pests) {
		const auto cues = pests->BuildParts(grid, visual);
		parts_.insert(parts_.end(), cues.parts.begin(), cues.parts.begin() + cues.count);
	}
	const int selectedIndex = showSelection ? grid.GetSelectedIndex() : -1;
	const auto selection = BuildFarmSelectionMeshParts(grid, selectedIndex, visual);
	static_assert(selection.parts.size() * 2 == kMaximumTargetParts);
	parts_.insert(parts_.end(), selection.parts.begin(), selection.parts.begin() + selection.count);
	if (showSelection && hoveredTileIndex != selectedIndex) {
		const auto hover = BuildFarmHoverMeshParts(grid, hoveredTileIndex, visual);
		parts_.insert(parts_.end(), hover.parts.begin(), hover.parts.begin() + hover.count);
	}
	// Retain unused objects until scene destruction; previous-frame GPU work is fenced by PostDraw.
	while (objects_.size() < parts_.size()) {
		auto object = std::make_unique<Object3d>();
		object->Initialize(common_);
		object->SetModel(meshModels_[static_cast<std::size_t>(FarmMeshShape::Box)]);
		object->SetTexture(whiteTexture_);
		objects_.push_back(std::move(object));
	}
    size_t cropCursor=0;
	for (std::size_t index = 0; index < parts_.size(); ++index) {
		const auto& part = parts_[index];
		auto& object = *objects_[index];
		const auto shape=static_cast<std::size_t>(part.shape);
		if (shape>=meshModels_.size() || !meshModels_[shape]) { parts_.clear(); lastDrawTileCount_=0; return; }
		Model* desired=meshModels_[shape];
		if (object.GetModel() != desired) { object.SetModel(desired); }
        Vector3 touch{};
        if (cropCursor<liveCrops_.size() && liveCrops_[cropCursor].part==index) {
            const auto& crop=liveCrops_[cropCursor++];
            const auto lean=brush?brush->Lean(crop.tile,crop.crop):Vector2{};
            if (part.shape==FarmMeshShape::PumpkinVines) {
                // The fruit rests on the ground. Twist the runner around its stalk instead of tipping the fruit.
                touch.y=.60f*(lean.x+.55f*lean.y);
            } else if (CropWindResponse(part.shape)>0) {
                const float amount=crop.crop==CropType::Tomato?.48f:1.f;
                touch={lean.y*amount,0,-lean.x*amount};
            }
        }
		const auto pose = index < livePartEnd ? CropWindPose(part, wind,touch) : FarmWindPose{part.position, {}};
		object.SetPosition(pose.position);
		object.SetRotation({pose.rotation.x,pose.rotation.y+part.yaw,pose.rotation.z+part.roll});
		if (!object.SetShearY(part.slope)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		if (!object.SetScale(part.scale)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		object.SetColor(part.color);
		const auto material = BuildFarmMaterialStyle(part);
		object.SetSpecularType(material.detail.x > 0.0f ?
			Object3d::SpecularType::BlinnPhong : Object3d::SpecularType::None);
		object.SetShininess(material.shininess);
		if (!object.SetSurfaceDetail(material.detail) || !object.SetSurfaceFront(part.wetFront)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		object.SetEnableLighting(!part.water && part.surface != FarmMeshSurface::Selection &&
			part.surface != FarmMeshSurface::Hover && part.surface != FarmMeshSurface::PestCue);
		object.Update(camera, 0.0f);
	}
    preparedBoard_=&grid; preparedGeneration_=grid.GetGeneration();
}

std::optional<FarmBrushContact> FarmRenderer::PickBrushContact(const FarmGrid& grid,const FarmVisualSystem& visual,
    int groundTile,Vector3 origin,Vector3 ray) const {
    if (!visible_ || preparedBoard_!=&grid || preparedGeneration_!=grid.GetGeneration()) return {};
    std::optional<FarmBrushContact> hit;
    float limit=(std::numeric_limits<float>::max)();
    if (const auto* tile=grid.GetTile(groundTile)) {
        if (tile->feature==FarmTileFeature::None) {
            if (TryPickFarmSoilSurface(BuildFarmSoilSurface(grid,groundTile,visual),origin,ray,limit) &&
                tile->state!=FarmTileState::Empty) hit=FarmBrushContact{groundTile,origin+ray*limit,false};
        } else if (ray.y<-.00001f) {
            const auto data=visual.GetTileVisualData(grid,groundTile);
            limit=(std::max)(0.f,(data.waterSurfaceCenter.y-origin.y)/ray.y);
        }
    }
    // Use last rendered poses, including wind. First reject bounded crop parts before visiting their triangles.
    for (const auto& live:liveCrops_) {
        const auto* tile=grid.GetTile(live.tile);
        if (!tile || tile->crop!=live.crop) continue;
        const auto& part=parts_[live.part];
        const float radius=1.9f*(std::max)({part.scale.x,part.scale.y,part.scale.z});
        const auto delta=part.position-origin;
        const float t=std::clamp(delta.x*ray.x+delta.y*ray.y+delta.z*ray.z,0.f,limit);
        const auto nearPoint=delta-ray*t;
        if (nearPoint.x*nearPoint.x+nearPoint.y*nearPoint.y+nearPoint.z*nearPoint.z>radius*radius) continue;
        const auto inv=MatrixMath::Inverse(objects_[live.part]->GetWorldMatrix());
        const auto local=MatrixMath::Transform(origin,inv);
        const auto d=MatrixMath::Transform(origin+ray,inv)-local;
        const auto* model=meshModels_[static_cast<size_t>(part.shape)];
        if (model && model->IntersectStaticTriangles(local,d,limit))
            hit=FarmBrushContact{live.tile,origin+ray*limit,true};
    }
    return hit;
}

void FarmRenderer::Draw() {
	if (!visible_ || !IsReady()) { return; }
	for (std::size_t index = 0; index < parts_.size(); ++index) { objects_[index]->Draw(); }
}

void FarmRenderer::DrawShadow() {
	if (!visible_ || !IsReady()) { return; }
	for (std::size_t index = 0; index < parts_.size(); ++index) {
		if (!parts_[index].water && parts_[index].surface != FarmMeshSurface::SoilBoundary &&
			parts_[index].surface != FarmMeshSurface::Selection &&
			parts_[index].surface != FarmMeshSurface::Hover &&
			parts_[index].surface != FarmMeshSurface::PestCue) { objects_[index]->DrawShadow(); }
	}
}

} // namespace farm
