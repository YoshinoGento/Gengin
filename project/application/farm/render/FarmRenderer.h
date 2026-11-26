#pragma once

#include "2d/TextureManager.h"
#include "environment/VegetationWind.h"
#include "farm/render/FarmMeshLayout.h"
#include "farm/system/FarmBrushPlaySystem.h"
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
class FarmPestVisualSystem;
struct FarmerPose;

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
        int hoveredTileIndex = -1, const FarmHarvestVisualSystem* harvest = nullptr, bool showSelection = true,
        const FarmPestVisualSystem* pests = nullptr, const vegetation::WindFrame& wind = {},
        const FarmBrushPlaySystem* brush = nullptr,const FarmActionParticles* actions = nullptr,const FarmerPose* farmer = nullptr);
    std::optional<FarmBrushContact> PickBrushContact(const FarmGrid& grid,const FarmVisualSystem& visual,
        int groundTile,Vector3 origin,Vector3 unitRay) const;
	void Draw();
	void DrawShadow();
	int GetLastDrawTileCount() const { return lastDrawTileCount_; }
	int GetPartCount() const { return static_cast<int>(parts_.size()); }
	bool IsReady() const { return common_ != nullptr; }
	bool IsLimitExceeded() const { return limitExceeded_; }
	bool HasCropMeshes() const { return cropsReady_; }

private:
	bool visible_ = true;
	int lastDrawTileCount_ = 0;
	// Thirty editable cells, including the worst-case 32 surface parts per cell.
	static constexpr std::size_t kMaximumParts = 30 * 32;
	static constexpr std::size_t kMaximumTargetParts = 16;
	static constexpr std::size_t kMaximumCropParts = kMaximumParts * 2;
	static constexpr std::size_t kMaximumHarvestParts = 40;
	Object3dCommon* common_ = nullptr; // Framework-owned, outlives scene.
	std::array<Model*,kFarmMeshPaths.size()> meshModels_{}; // ModelManager-owned; shared by all tiles.
	bool cropsReady_ = false;
	Texture2DHandle whiteTexture_{};
    Texture2DHandle farmerPalette_{};
	std::vector<std::unique_ptr<Object3d>> objects_;
	std::vector<FarmMeshPart> parts_;
    struct LiveCrop { size_t part; int tile; CropType crop; };
    std::vector<LiveCrop> liveCrops_;
    const FarmGrid* preparedBoard_{};
    uint64_t preparedGeneration_{};
	bool limitExceeded_ = false;
};

} // namespace farm
