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
	std::error_code error;
	if (!common || !models || !std::filesystem::is_regular_file("Resources/farm/unit_box.obj", error)) { return false; }
	if (!std::filesystem::is_regular_file("Resources/farm/triangle_lower.obj", error) ||
		!std::filesystem::is_regular_file("Resources/farm/triangle_upper.obj", error)) { return false; }
	models->LoadModel("farm/unit_box.obj");
	Model* model = models->GetModel("farm/unit_box.obj");
	if (!model || model->HasSkinCluster()) { return false; }
	models->LoadModel("farm/triangle_lower.obj");
	models->LoadModel("farm/triangle_upper.obj");
	Model* lower = models->GetModel("farm/triangle_lower.obj");
	Model* upper = models->GetModel("farm/triangle_upper.obj");
	if (!lower || !upper || lower->HasSkinCluster() || upper->HasSkinCluster()) { return false; }
	common_ = common;
	model_ = model;
	triangleLower_ = lower;
	triangleUpper_ = upper;
	whiteTexture_ = whiteTexture;
	constexpr std::array<const char*,8> cropPaths{"farm/crop_turnip.obj", "farm/crop_carrot.obj",
		"farm/crop_turnip_leaves.obj", "farm/crop_carrot_leaves.obj", "farm/crop_tomato.obj",
		"farm/crop_tomato_stems.obj", "farm/crop_pumpkin.obj", "farm/crop_pumpkin_vines.obj"};
	static_assert(cropPaths.size() == std::tuple_size_v<decltype(cropModels_)>);
	bool cropFilesPresent = true;
	for (const auto* path : cropPaths) cropFilesPresent &= std::filesystem::is_regular_file(std::filesystem::path("Resources") / path, error);
	if (cropFilesPresent) {
		for (std::size_t i=0; i<cropPaths.size(); ++i) {
			models->LoadModel(cropPaths[i]);
			cropModels_[i] = models->GetModel(cropPaths[i]);
			if (cropModels_[i] && cropModels_[i]->HasSkinCluster()) cropModels_[i] = nullptr;
		}
	}
	parts_.reserve(kMaximumParts + kMaximumTargetParts + kMaximumCropParts + kMaximumHarvestParts + FarmPestMeshParts::kCapacity);
	objects_.reserve(kMaximumParts + kMaximumTargetParts + kMaximumCropParts + kMaximumHarvestParts + FarmPestMeshParts::kCapacity);
	return true;
}

void FarmRenderer::Prepare(const FarmGrid& grid, const FarmVisualSystem& visual, Camera* camera,
	int hoveredTileIndex, const FarmHarvestVisualSystem* harvest, bool showSelection, const FarmPestVisualSystem* pests) {
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
		object->SetModel(model_);
		object->SetTexture(whiteTexture_);
		objects_.push_back(std::move(object));
	}
	for (std::size_t index = 0; index < parts_.size(); ++index) {
		const auto& part = parts_[index];
		auto& object = *objects_[index];
		Model* desired = part.shape == FarmMeshShape::TriangleLower ? triangleLower_ :
			(part.shape == FarmMeshShape::TriangleUpper ? triangleUpper_ : model_);
		if (part.shape == FarmMeshShape::Turnip) desired = cropModels_[0];
		if (part.shape == FarmMeshShape::Carrot) desired = cropModels_[1];
		if (part.shape == FarmMeshShape::Leaves) desired = cropModels_[2];
		if (part.shape == FarmMeshShape::CarrotLeaves) desired = cropModels_[3];
		if (part.shape == FarmMeshShape::Tomato) desired = cropModels_[4];
		if (part.shape == FarmMeshShape::TomatoStems) desired = cropModels_[5];
		if (part.shape == FarmMeshShape::Pumpkin) desired = cropModels_[6];
		if (part.shape == FarmMeshShape::PumpkinVines) desired = cropModels_[7];
		if (object.GetModel() != desired) { object.SetModel(desired); }
		object.SetPosition(part.position);
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
