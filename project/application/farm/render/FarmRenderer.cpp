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
	whiteTexture_=whiteTexture;
	parts_.reserve(kMaximumParts + kMaximumTargetParts + kMaximumCropParts + kMaximumHarvestParts + FarmPestMeshParts::kCapacity);
	objects_.reserve(kMaximumParts + kMaximumTargetParts + kMaximumCropParts + kMaximumHarvestParts + FarmPestMeshParts::kCapacity);
	return true;
}

void FarmRenderer::Prepare(const FarmGrid& grid, const FarmVisualSystem& visual, Camera* camera,
	int hoveredTileIndex, const FarmHarvestVisualSystem* harvest, bool showSelection, const FarmPestVisualSystem* pests,
	const vegetation::WindFrame& wind) {
	parts_.clear();
	lastDrawTileCount_ = 0;
	limitExceeded_ = false;
	if (!IsReady() || !visible_ || !camera) { return; }
	if (grid.GetTileCount() > static_cast<int>(kMaximumParts)) { limitExceeded_ = true; return; }
	std::size_t terrainCount = 0;
	std::size_t plantCount = 0;
	for (int index = 0; index < grid.GetTileCount(); ++index) {
		const auto tileParts = BuildFarmTileMeshParts(grid, index, visual);
		if (terrainCount + tileParts.count > kMaximumParts) {
			parts_.clear(); lastDrawTileCount_ = 0; limitExceeded_ = true; return;
		}
		parts_.insert(parts_.end(), tileParts.parts.begin(), tileParts.parts.begin() + tileParts.count);
		terrainCount += tileParts.count;
		const auto repellent = BuildFarmRepellentMeshParts(grid, index, visual);
		const auto crops = HasCropMeshes() ? BuildFarmCropMeshParts(grid, index, visual) : FarmCropMeshParts{};
		if (plantCount + repellent.count + crops.count > kMaximumCropParts) {
			parts_.clear(); lastDrawTileCount_ = 0; limitExceeded_ = true; return;
		}
		parts_.insert(parts_.end(), repellent.parts.begin(), repellent.parts.begin() + repellent.count);
		parts_.insert(parts_.end(), crops.parts.begin(), crops.parts.begin() + crops.count);
		plantCount += repellent.count + crops.count;
		lastDrawTileCount_ += tileParts.count > 0 ? 1 : 0;
	}
	const auto livePartEnd = parts_.size();
	static_assert(kMaximumHarvestParts == FarmHarvestVisualSystem::kCapacity * 2);
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
	for (std::size_t index = 0; index < parts_.size(); ++index) {
		const auto& part = parts_[index];
		auto& object = *objects_[index];
		const auto shape=static_cast<std::size_t>(part.shape);
		if (shape>=meshModels_.size() || !meshModels_[shape]) { parts_.clear(); lastDrawTileCount_=0; return; }
		Model* desired=meshModels_[shape];
		if (object.GetModel() != desired) { object.SetModel(desired); }
		object.SetPosition(part.position);
		object.SetRotation(index < livePartEnd ? wind.Tilt(part.position, CropWindResponse(part.shape)) : Vector3{});
		if (!object.SetShearY(part.slope)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		if (!object.SetScale(part.scale)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		object.SetColor(part.color);
		const auto material = BuildFarmMaterialStyle(part);
		object.SetSpecularType(material.detail.x > 0.0f ?
			Object3d::SpecularType::BlinnPhong : Object3d::SpecularType::None);
		object.SetShininess(material.shininess);
		if (!object.SetSurfaceDetail(material.detail)) { parts_.clear(); lastDrawTileCount_ = 0; return; }
		object.SetEnableLighting(!part.water && part.surface != FarmMeshSurface::Selection &&
			part.surface != FarmMeshSurface::Hover && part.surface != FarmMeshSurface::PestCue);
		object.Update(camera, 0.0f);
	}
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
