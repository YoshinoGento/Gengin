#pragma once

#include "2d/TextureManager.h"
#include "farm/render/FarmMeshLayout.h"
#include <memory>
#include <vector>

class Object3d;
class Object3dCommon;
class ModelManager;
class Model;
class Camera;

namespace farm {

class FarmGrid;
class FarmHarvestVisualSystem;

class FarmRenderer {
public:
	FarmRenderer();
	~FarmRenderer();
	FarmRenderer(const FarmRenderer&) = delete;
	FarmRenderer& operator=(const FarmRenderer&) = delete;
	bool Initialize(Object3dCommon* common, ModelManager* models, Texture2DHandle whiteTexture);
	void SetVisible(bool visible) { visible_ = visible; }
	bool IsVisible() const { return visible_; }
	void Prepare(const FarmGrid& grid, const FarmVisualSystem& visual, Camera* camera,
        int hoveredTileIndex = -1, const FarmHarvestVisualSystem* harvest = nullptr, bool showSelection = true);
	void Draw();
	void DrawShadow();
	int GetLastDrawTileCount() const { return lastDrawTileCount_; }
	int GetPartCount() const { return static_cast<int>(parts_.size()); }
	bool IsReady() const { return common_ != nullptr && model_ != nullptr; }
	bool IsLimitExceeded() const { return limitExceeded_; }
	bool HasCropMeshes() const { for (const auto* model : cropModels_) if (!model) return false; return true; }

private:
	bool visible_ = true;
	int lastDrawTileCount_ = 0;
	// Thirty editable cells, including the worst-case 32 surface parts per cell.
	static constexpr std::size_t kMaximumParts = 30 * 32;
	static constexpr std::size_t kMaximumTargetParts = 16;
	static constexpr std::size_t kMaximumCropParts = kMaximumParts * 2;
	static constexpr std::size_t kMaximumHarvestParts = 16;
	Object3dCommon* common_ = nullptr; // Framework-owned, outlives scene.
	Model* model_ = nullptr; // ModelManager-owned immutable static mesh.
	Model* triangleLower_ = nullptr; // Same ModelManager lifetime as the box.
	Model* triangleUpper_ = nullptr;
	std::array<Model*, 8> cropModels_{}; // ModelManager-owned; shared by all tiles.
	Texture2DHandle whiteTexture_{};
	std::vector<std::unique_ptr<Object3d>> objects_;
	std::vector<FarmMeshPart> parts_;
	bool limitExceeded_ = false;
};

} // namespace farm
