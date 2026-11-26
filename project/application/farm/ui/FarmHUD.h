#pragma once

#include "2d/BitmapFont.h"
#include "base/InitializationTask.h"
#include "2d/Sprite.h"
#include "farm/core/FarmTypes.h"
#include "farm/ui/FarmUIRect.h"

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>

class SpriteCommon;

enum class FarmHUDNextAction : std::uint8_t {
	SelectTile,
	Canal,
	WaterSource,
	Hoe,
	WaterOrSeed,
	BuySeed,
	Seed,
	Water,
	Growing,
	Harvest,
	ReduceWater,
};

enum class FarmHUDFeedback : std::uint8_t {
	None,
	Harvest,
	Sale,
	EmptySale,
	InputLocked,
	Restarted,
	SeedPurchasedTurnip,
	SeedPurchasedCarrot,
	CropSelectedTurnip,
	CropSelectedCarrot,
	NoSeedTurnip,
	NoSeedCarrot,
	InsufficientMoney,
	SeedPurchasedTomato, SeedPurchasedPumpkin,
	CropSelectedTomato, CropSelectedPumpkin,
	NoSeedTomato, NoSeedPumpkin,
};

enum class FarmHUDMoistureStatus : std::uint8_t {
	None,
	Dry,
	Low,
	Good,
	Excess,
};

struct FarmHUDViewData {
	int day = 1;
	int money = 0;
	int rank = 1;
	int cropCount = 0;
	int saleValue = 0;
	int seedCount = 0;
	int seedPrice = 0;
	std::array<int, farm::kFarmCropTypeCount> cropInventoryCounts{};
	std::array<int, farm::kFarmCropTypeCount> cropInventoryValues{};
	std::array<int, farm::kFarmCropTypeCount> cropSeedCounts{};
	farm::CropType selectedSeedCrop = farm::CropType::Carrot;
	int cropsNeeded = 0;
	int goalMoney = 1;
	bool contestSeason = false;
	int contestFinalDay = 0;
	bool freeFarming = false;
	int currentToolIndex = -1;
	float goalProgress = 0.0f;
	float timeScale = 1.0f;
	bool goalCleared = false;
	bool irrigationPreviewActive = false;
	bool irrigationPreviewRemoval = false;
	int irrigationPreviewChangeCount = 0;
	bool selectedTileValid = false;
	int selectedTileIndex = -1;
	int selectedTileHeight = 0;
	int selectedTileMoisturePercent = 0;
	int selectedTileGrowthPercent = 0;
	farm::FarmTileFeature selectedTileFeature = farm::FarmTileFeature::None;
	bool selectedTileIrrigationSupplied = false;
	bool selectedTileIrrigationActive = false;
	int selectedTileIrrigationStrengthPercent = 0;
	int selectedTileStoredWaterPercent = 0;
	farm::FarmWaterStatus selectedTileWaterStatus = farm::FarmWaterStatus::None;
	bool selectedTileReceivedIrrigation = false;
	farm::FarmTileState selectedTileState = farm::FarmTileState::Empty;
	farm::CropType selectedTileCrop = farm::CropType::None;
	farm::FarmCropGrowthStage selectedTileGrowthStage = farm::FarmCropGrowthStage::None;
	FarmHUDMoistureStatus selectedTileMoistureStatus = FarmHUDMoistureStatus::None;
	bool selectedTileMoistureRangeValid = false;
	float selectedTileGoodMoistureMinimum = 0.0f;
	float selectedTileGoodMoistureMaximum = 0.0f;
	bool cropPieOpen = false;
	farm::CropType cropPieHovered = farm::CropType::None;
	Vector2 cropPieCenter{};
	FarmHUDNextAction nextAction = FarmHUDNextAction::SelectTile;
	FarmHUDFeedback feedback = FarmHUDFeedback::None;
	farm::CropType feedbackCrop = farm::CropType::None;
	int feedbackQualityScore = 0;
	int feedbackSaleCount = 0;
	int feedbackSaleValue = 0;
};

// Presentation-only Farm overlay. Gameplay state remains owned by Farm Systems.
class FarmHUD final {
public:
	bool Initialize(SpriteCommon* spriteCommon);
	InitializationTask InitializeIncrementally(SpriteCommon* spriteCommon);
	void SetViewData(const FarmHUDViewData& viewData);
	void Update(float deltaTime);
	void Draw(bool simpleField = false);

private:
	// Fixed after initialization; each live glyph owns distinct GPU constants.
	struct TextRun {
		InitializationTask Initialize(SpriteCommon* common, const BitmapFont& font, std::size_t capacity);
		void Set(const BitmapFont& font, std::string_view value, farmui::Rect bounds,
			float maximumScale, const Vector4& color);
		void Update();
		void Draw();
		std::unique_ptr<Sprite[]> glyphs;
		std::size_t capacity = 0;
		std::size_t count = 0;
	};

	InitializationTask InitializePanels(SpriteCommon* spriteCommon);
	InitializationTask InitializeText(SpriteCommon* spriteCommon);
	bool InitializeLocalizedSprites(SpriteCommon* spriteCommon);
	InitializationTask LoadLocalizedTextureHandles();
	void ApplyLayout();
	void UpdateStatus();
	void UpdateGoal();
	void UpdateIrrigationPreview();
	void UpdateSelectedTile();
	void UpdateFeedback();
	void UpdateCropPieMenu();

private:
	FarmHUDViewData viewData_;
	bool initialized_ = false;

	BitmapFont font_;
	Sprite statusPanel_;
	Sprite economyPanel_;
	Sprite selectedTilePanel_;
	Sprite goalPanel_;
	Sprite feedbackPanel_;
	Sprite goalBarTrack_;
	Sprite goalBarFill_;
	Sprite waterBarTrack_;
	Sprite waterBarFill_;
	Sprite waterRangeBand_;
	Sprite waterPointer_;
	Sprite growthBarTrack_;
	Sprite growthBarFill_;
	Sprite dayIcon_;
	Sprite rankIcon_;
	Sprite moneyIcon_;
	Sprite inventoryIcon_;
	Sprite selectedIcon_;
	Sprite waterIcon_;
	Sprite growthIcon_;
	Sprite cropPieCenterPanel_;
	std::array<Sprite, 3> cropPiePanels_;
	Sprite localizedSelectedName_;
	Sprite localizedTileState_;
	std::array<Sprite, 3> localizedCropPieNames_;
	std::array<Sprite, 3> localizedCropPieTraits_;
	Sprite localizedCropPieGuide_;
	Sprite localizedMoistureStatus_;
	Sprite localizedIrrigationStatus_;
	Sprite localizedIrrigationPreview_;
	Sprite localizedFeedback_;
	Sprite localizedFeedbackCrop_;

	std::array<Texture2DHandle, 11> tileStateTextureHandles_;
	std::array<Texture2DHandle, 5> cropTextureHandles_;
	std::array<Texture2DHandle, 3> cropPieTraitTextureHandles_;
	Texture2DHandle cropPieGuideTextureHandle_{};
	std::array<Texture2DHandle, 4> moistureStatusTextureHandles_;
	std::array<Texture2DHandle, 5> irrigationStatusTextureHandles_;
	std::array<Texture2DHandle, 2> irrigationPreviewTextureHandles_;
	std::array<Texture2DHandle, 19> feedbackTextureHandles_;

	TextRun dayText_;
	TextRun moneyText_;
	TextRun rankText_;
	TextRun cropCountText_;
	TextRun goalText_;
	TextRun irrigationPreviewCountText_;
	TextRun selectedTileHeightText_;
	TextRun selectedTileWaterText_;
	TextRun selectedTileGrowthText_;
	TextRun feedbackMetricsText_;
	std::array<TextRun, farm::kPlayableCropCount> cropPieStatsText_;
	std::array<TextRun, farm::kPlayableCropCount> cropPieValueText_;
};
