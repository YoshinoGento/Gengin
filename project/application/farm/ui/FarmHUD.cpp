#include "farm/ui/FarmHUD.h"
#include "farm/ui/FarmHUDIcons.h"
#include "farm/ui/FarmHUDLayout.h"
#include "farm/ui/FarmRuntimeUI.h"
#include "ui/FieldSurfaceTexture.h"
#include "2d/SpriteCommon.h"
#include "base/Logger.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <new>

namespace {
using farmui::Rect;
namespace layout = farmui::HUDLayout;
constexpr const char* kFontConfigPath = "Resources/ui/font/ascii_bitmap_font.json";
constexpr const char* kPanelTexturePath = "Resources/ui/hud_panel_fill.png";
constexpr const char* kCropPieGuidePath = "Resources/generated/text/hud_crop_pie_guide.png";
constexpr Vector4 kFeedbackPanelColor{0.035f, 0.044f, 0.038f, 0.94f};
constexpr Vector4 kTrackColor{0.12f, 0.15f, 0.13f, 0.95f};
constexpr Vector4 kGrowthColor{0.36f, 0.78f, 0.42f, 1.0f};
constexpr Vector4 kTextColor{0.96f, 0.97f, 0.94f, 1.0f};
constexpr Vector4 kMutedTextColor{0.68f, 0.76f, 0.70f, 1.0f};
constexpr Vector4 kAccentColor{1.0f, 0.82f, 0.18f, 1.0f};
constexpr Vector4 kWaterColor{0.40f, 0.72f, 0.94f, 1.0f};
constexpr Vector4 kDryColor{0.94f, 0.65f, 0.24f, 1.0f};
constexpr Vector4 kExcessColor{0.96f, 0.48f, 0.40f, 1.0f};
constexpr Vector4 kRangeColor{0.58f, 0.80f, 0.73f, 1.0f};
constexpr Vector4 kSelectedSlotTextColor=fieldsign::kWheat;
constexpr Vector2 kPiePanelSize{174.0f, 126.0f};
constexpr float kPiePanelGap = 20.0f;
constexpr float kFeedbackPadding = 16.0f;
constexpr Rect kFeedbackPanel = farmui::View::kFeedbackPanel;
constexpr Rect kWaterTrack{layout::selected.x + 46, layout::selected.y + 76, layout::selected.width - 58, 6};
constexpr Rect kGrowthTrack{kWaterTrack.x, layout::selected.y + 127, kWaterTrack.width, 6};
constexpr Rect kGoalTrack{layout::goal.x + 12, layout::goal.y + 32, layout::goal.width - 24, 4};
constexpr Rect kWaterStatus{layout::selected.x + 110, layout::selected.y + 44, layout::selected.width - 122, 30};
constexpr Rect kGrowthStatus{kWaterStatus.x, layout::selected.y + 96, kWaterStatus.width, 30};
constexpr std::array<const char*, 11> kTileStatePaths = {
	"Resources/generated/text/hud_state_invalid.png",
	"Resources/generated/text/hud_state_canal.png",
	"Resources/generated/text/hud_state_canal_supplied.png",
	"Resources/generated/text/hud_state_water_source.png",
	"Resources/generated/text/hud_state_empty.png",
	"Resources/generated/text/hud_state_tilled.png",
	"Resources/generated/text/hud_state_watered.png",
	"Resources/generated/text/hud_state_sprout.png",
	"Resources/generated/text/hud_state_planted.png",
	"Resources/generated/text/hud_state_almost_ready.png",
	"Resources/generated/text/hud_state_ready.png",
};
constexpr std::array<const char*, 5> kCropPaths = {
	"Resources/generated/text/hud_crop_none.png",
	"Resources/generated/text/hud_crop_test.png",
	"Resources/generated/text/hud_crop_carrot.png",
	"Resources/generated/text/hud_crop_tomato.png",
	"Resources/generated/text/hud_crop_pumpkin.png",
};
constexpr std::array<const char*, 3> kCropPieTraitPaths = {
	"Resources/generated/text/hud_crop_trait_carrot.png",
	"Resources/generated/text/hud_crop_trait_tomato.png",
	"Resources/generated/text/hud_crop_trait_pumpkin.png",
};
constexpr std::array<const char*, 4> kMoistureStatusPaths = {
	"Resources/generated/text/hud_moisture_dry.png",
	"Resources/generated/text/hud_moisture_low.png",
	"Resources/generated/text/hud_moisture_good.png",
	"Resources/generated/text/hud_moisture_excess.png",
};
constexpr std::array<const char*, 5> kIrrigationStatusPaths = {
	"Resources/generated/text/hud_irrigation_active.png",
	"Resources/generated/text/hud_irrigation_retained.png",
	"Resources/generated/text/hud_irrigation_waiting.png",
	"Resources/generated/text/hud_irrigation_dry.png",
	"Resources/generated/text/hud_irrigation_delivered.png",
};
constexpr std::array<const char*, 2> kIrrigationPreviewPaths = {
	"Resources/generated/text/hud_irrigation_preview.png",
	"Resources/generated/text/hud_irrigation_removal_preview.png",
};
constexpr std::array<const char*, 19> kFeedbackPaths = {
	"Resources/generated/text/hud_feedback_harvest.png",
	"Resources/generated/text/hud_feedback_harvest.png",
	"Resources/generated/text/hud_feedback_sale.png",
	"Resources/generated/text/hud_feedback_empty_sale.png",
	"Resources/generated/text/hud_feedback_locked.png",
	"Resources/generated/text/hud_feedback_restarted.png",
	"Resources/generated/text/hud_feedback_seed_bought_turnip.png",
	"Resources/generated/text/hud_feedback_seed_bought_carrot.png",
	"Resources/generated/text/hud_feedback_crop_selected_turnip.png",
	"Resources/generated/text/hud_feedback_crop_selected_carrot.png",
	"Resources/generated/text/hud_feedback_no_seed_turnip.png",
	"Resources/generated/text/hud_feedback_no_seed_carrot.png",
	"Resources/generated/text/hud_feedback_no_money.png",
	"Resources/generated/text/hud_feedback_seed_bought_tomato.png",
	"Resources/generated/text/hud_feedback_seed_bought_pumpkin.png",
	"Resources/generated/text/hud_feedback_crop_selected_tomato.png",
	"Resources/generated/text/hud_feedback_crop_selected_pumpkin.png",
	"Resources/generated/text/hud_feedback_no_seed_tomato.png",
	"Resources/generated/text/hud_feedback_no_seed_pumpkin.png",
};

template <std::size_t Size>
InitializationTask LoadTextureHandles(std::array<Texture2DHandle, Size>& handles, const std::array<const char*, Size>& paths) {
    auto* manager = TextureManager::GetInstance();
    if (manager == nullptr || !manager->IsInitialized()) co_return false;
    for (std::size_t i = 0; i < Size; ++i) {
        handles[i] = manager->LoadTexture2D(paths[i]);
        if (!handles[i].IsValid()) co_return false;
        co_await std::suspend_always{};
    }
    co_return true;
}

bool HasCrop(const FarmHUDViewData& data) {
    return data.selectedTileValid && data.selectedTileFeature == farm::FarmTileFeature::None &&
        farm::IsPlantableCrop(data.selectedTileCrop) &&
        (data.selectedTileState == farm::FarmTileState::Planted || data.selectedTileState == farm::FarmTileState::ReadyToHarvest);
}
bool HasMoistureStatus(const FarmHUDViewData& data) {
    return data.selectedTileFeature == farm::FarmTileFeature::None &&
        data.selectedTileMoistureStatus != FarmHUDMoistureStatus::None;
}
bool HasIrrigationStatus(const FarmHUDViewData& data) {
    return !HasMoistureStatus(data) && (data.selectedTileReceivedIrrigation ||
        data.selectedTileWaterStatus != farm::FarmWaterStatus::None);
}
bool HasFeedbackCrop(const FarmHUDViewData& data) {
    return (data.feedback == FarmHUDFeedback::Harvest || data.feedback == FarmHUDFeedback::Sale) &&
        farm::IsPlantableCrop(data.feedbackCrop);
}
std::size_t CropTextureIndex(farm::CropType crop) {
    switch (crop) {
    case farm::CropType::TestCrop: return 1;
    case farm::CropType::Carrot: return 2;
    case farm::CropType::Tomato: return 3;
    case farm::CropType::Pumpkin: return 4;
    default: return 0;
    }
}
std::size_t TileStateTextureIndex(const FarmHUDViewData& data) {
    if (!data.selectedTileValid) return 0;
    if (data.selectedTileFeature == farm::FarmTileFeature::Canal) return data.selectedTileIrrigationSupplied ? 2 : 1;
    if (data.selectedTileFeature == farm::FarmTileFeature::WaterSource) return 3;
    if (data.selectedTileState == farm::FarmTileState::ReadyToHarvest) return 10;
    if (data.selectedTileState == farm::FarmTileState::Planted) {
        switch (data.selectedTileGrowthStage) {
        case farm::FarmCropGrowthStage::Sprout: return 7;
        case farm::FarmCropGrowthStage::AlmostReady: return 9;
        case farm::FarmCropGrowthStage::Ready: return 10;
        case farm::FarmCropGrowthStage::Growing: return 8;
        default: return 0;
        }
    }
    switch (data.selectedTileState) {
    case farm::FarmTileState::Empty: return 4;
    case farm::FarmTileState::Tilled: return 5;
    case farm::FarmTileState::Watered: return 6;
    default: return 0;
    }
}
farmui::HUDIcon SelectedIcon(const FarmHUDViewData& data) {
    using farmui::HUDIcon;
    if (data.selectedTileFeature == farm::FarmTileFeature::Canal) return HUDIcon::Canal;
    if (data.selectedTileFeature == farm::FarmTileFeature::WaterSource) return HUDIcon::Source;
    if (HasCrop(data)) {
        switch (data.selectedTileCrop) {
        case farm::CropType::Carrot: return HUDIcon::Carrot;
        case farm::CropType::Tomato: return HUDIcon::Tomato;
        case farm::CropType::Pumpkin: return HUDIcon::Pumpkin;
        default: return HUDIcon::Sprout;
        }
    }
    return HUDIcon::Terrain;
}
Vector4 MoistureColor(const FarmHUDViewData& data) {
    if (data.selectedTileFeature != farm::FarmTileFeature::None) return kWaterColor;
    switch (data.selectedTileMoistureStatus) {
    case FarmHUDMoistureStatus::Dry:
    case FarmHUDMoistureStatus::Low: return kDryColor;
    case FarmHUDMoistureStatus::Excess: return kExcessColor;
    case FarmHUDMoistureStatus::Good: return kRangeColor;
    default: return kWaterColor;
    }
}
void ConfigurePanel(Sprite& sprite, Rect bounds, const Vector4& color) {
    sprite.SetPosition({bounds.x, bounds.y});
    sprite.SetSize({bounds.width, bounds.height});
    sprite.SetColor(color);
}
void ConfigureIcon(Sprite& sprite, farmui::HUDIcon icon, Rect bounds) {
    sprite.SetTextureRect(farmui::IconUV(icon), {farmui::kHUDIconSize, farmui::kHUDIconSize});
    ConfigurePanel(sprite, bounds, {1, 1, 1, 1});
}
void ConfigureSurface(Sprite& sprite,Rect bounds,fieldsign::Region uv) {
    sprite.SetTextureRect({uv.x,uv.y},{uv.w,uv.h});
    ConfigurePanel(sprite,bounds,fieldsign::kWhite);
}
void SetLocalizedTexture(Sprite& sprite, Texture2DHandle handle, Rect bounds, float maximumScale = 0.8f) {
    // SetTexture restores natural dimensions, preventing cumulative shrinking.
    sprite.SetTexture(handle);
    const auto natural = sprite.GetSize();
    if (natural.x <= 0 || natural.y <= 0) return;
    const float scale = (std::min)({maximumScale, bounds.width / natural.x, bounds.height / natural.y});
    const Vector2 size{natural.x * scale, natural.y * scale};
    sprite.SetPosition({bounds.x, bounds.y + (bounds.height - size.y) * 0.5f});
    sprite.SetSize(size);
}
}

InitializationTask FarmHUD::TextRun::Initialize(SpriteCommon* common, const BitmapFont& font, std::size_t maximum) {
    count = capacity = 0;
    glyphs.reset(new (std::nothrow) Sprite[maximum]);
    if (!glyphs || !font.GetTextureHandle().IsValid()) co_return false;
    for (std::size_t i = 0; i < maximum; ++i) {
        if (!glyphs[i].Initialize(common, font.GetTexturePath())) {
            glyphs.reset();
            co_return false;
        }
        co_await std::suspend_always{};
    }
    capacity = maximum;
    co_return true;
}
void FarmHUD::TextRun::Set(const BitmapFont& font, std::string_view value, Rect bounds,
    float maximumScale, const Vector4& color) {
    count = 0;
    if (value.empty() || !glyphs || value.size() > capacity || bounds.width <= 0 || bounds.height <= 0) return;
    const Vector2 glyphSize = font.GetGlyphSize();
    if (glyphSize.x <= 0 || glyphSize.y <= 0) return;
    const float scale = (std::min)({maximumScale, bounds.height / glyphSize.y,
        bounds.width / (glyphSize.x * static_cast<float>(value.size()))});
    const float advance = glyphSize.x * scale;
    for (char character : value) {
        Vector2 leftTop{}, size{};
        if (!font.TryGetGlyphRect(character, leftTop, size)) continue;
        Sprite& glyph = glyphs[count];
        glyph.SetTextureRect(leftTop, size);
        glyph.SetPosition({bounds.x + static_cast<float>(count) * advance,
            bounds.y + (bounds.height - size.y * scale) * 0.5f});
        glyph.SetSize({size.x * scale, size.y * scale});
        glyph.SetColor(color);
        ++count;
    }
}
void FarmHUD::TextRun::Update() { for (std::size_t i = 0; i < count; ++i) glyphs[i].Update(); }
void FarmHUD::TextRun::Draw() { for (std::size_t i = 0; i < count; ++i) glyphs[i].Draw(); }

bool FarmHUD::Initialize(SpriteCommon* common) {
    return InitializeIncrementally(common).Complete();
}
InitializationTask FarmHUD::InitializeIncrementally(SpriteCommon* common) {
    initialized_ = false;
    auto* textures = TextureManager::GetInstance();
    if (common == nullptr || textures == nullptr || !textures->IsInitialized()) co_return false;
    if (!font_.InitializeFromJson(common, kFontConfigPath) ||
        !textures->LoadTexture2D(kPanelTexturePath).IsValid() ||
        !textures->LoadTexture2D(farmui::kHUDIconAtlas).IsValid() ||
        !co_await LoadLocalizedTextureHandles() || !co_await InitializePanels(common) ||
        !InitializeLocalizedSprites(common) || !co_await InitializeText(common)) {
        Logger::Log("FarmHUD::Initialize failed. Required textures or retained Sprite pools are unavailable.");
        co_return false;
    }
    ApplyLayout();
    initialized_ = true;
    UpdateStatus();
    UpdateGoal();
    UpdateSelectedTile();
    UpdateIrrigationPreview();
    UpdateFeedback();
    UpdateCropPieMenu();
    co_return true;
}
InitializationTask FarmHUD::InitializeText(SpriteCommon* common) {
    // Full decimal int values, including signs/separators, fit these fixed capacities.
    if (!co_await dayText_.Initialize(common, font_, 10) || !co_await rankText_.Initialize(common, font_, 10) ||
        !co_await moneyText_.Initialize(common, font_, 10) || !co_await cropCountText_.Initialize(common, font_, 10) ||
        !co_await goalText_.Initialize(common, font_, 28) || !co_await irrigationPreviewCountText_.Initialize(common, font_, 4) ||
        !co_await selectedTileHeightText_.Initialize(common, font_, 12) || !co_await selectedTileWaterText_.Initialize(common, font_, 4) ||
        !co_await selectedTileGrowthText_.Initialize(common, font_, 4) || !co_await feedbackMetricsText_.Initialize(common, font_, 26)) co_return false;
    for (auto& text : cropPieStatsText_) if (!co_await text.Initialize(common, font_, 23)) co_return false;
    for (auto& text : cropPieValueText_) if (!co_await text.Initialize(common, font_, 11)) co_return false;
    co_return true;
}
InitializationTask FarmHUD::InitializePanels(SpriteCommon* common) {
    const auto skin=fieldsign::LoadForScene();
    if (!skin.IsValid()) co_return false;
    for (Sprite* panel : {&statusPanel_, &economyPanel_, &selectedTilePanel_, &goalPanel_, &feedbackPanel_}) {
        if (!panel->Initialize(common,skin.Index())) co_return false;
        co_await std::suspend_always{};
    }
    for (Sprite* panel : {&goalBarTrack_, &goalBarFill_, &waterBarTrack_, &waterBarFill_,
        &waterRangeBand_, &waterPointer_, &growthBarTrack_, &growthBarFill_, &cropPieCenterPanel_}) {
        if (!panel->Initialize(common, kPanelTexturePath)) co_return false;
        co_await std::suspend_always{};
    }
    for (auto& panel : cropPiePanels_) {
        if (!panel.Initialize(common, skin.Index())) co_return false;
        co_await std::suspend_always{};
    }
    for (Sprite* icon : {&dayIcon_, &rankIcon_, &moneyIcon_, &inventoryIcon_, &selectedIcon_, &waterIcon_, &growthIcon_}) {
        if (!icon->Initialize(common, farmui::kHUDIconAtlas)) co_return false;
        co_await std::suspend_always{};
    }
    co_return true;
}
InitializationTask FarmHUD::LoadLocalizedTextureHandles() {
    co_return co_await LoadTextureHandles(tileStateTextureHandles_, kTileStatePaths) &&
        co_await LoadTextureHandles(cropTextureHandles_, kCropPaths) &&
        co_await LoadTextureHandles(cropPieTraitTextureHandles_, kCropPieTraitPaths) &&
        co_await LoadTextureHandles(moistureStatusTextureHandles_, kMoistureStatusPaths) &&
        co_await LoadTextureHandles(irrigationStatusTextureHandles_, kIrrigationStatusPaths) &&
        co_await LoadTextureHandles(irrigationPreviewTextureHandles_, kIrrigationPreviewPaths) &&
        co_await LoadTextureHandles(feedbackTextureHandles_, kFeedbackPaths);
}
bool FarmHUD::InitializeLocalizedSprites(SpriteCommon* common) {
    cropPieGuideTextureHandle_ = TextureManager::GetInstance()->LoadTexture2D(kCropPieGuidePath);
    if (!cropPieGuideTextureHandle_.IsValid()) return false;
    for (std::size_t i = 0; i < cropPiePanels_.size(); ++i) {
        if (!localizedCropPieNames_[i].Initialize(common, kCropPaths[i + 2]) ||
            !localizedCropPieTraits_[i].Initialize(common, kCropPieTraitPaths[i])) return false;
    }
    return localizedSelectedName_.Initialize(common, kTileStatePaths[0]) &&
        localizedTileState_.Initialize(common, kTileStatePaths[0]) &&
        localizedCropPieGuide_.Initialize(common, kCropPieGuidePath) &&
        localizedMoistureStatus_.Initialize(common, kMoistureStatusPaths[0]) &&
        localizedIrrigationStatus_.Initialize(common, kIrrigationStatusPaths[0]) &&
        localizedIrrigationPreview_.Initialize(common, kIrrigationPreviewPaths[0]) &&
        localizedFeedback_.Initialize(common, kFeedbackPaths[0]) &&
        localizedFeedbackCrop_.Initialize(common, kCropPaths[0]);
}
void FarmHUD::ApplyLayout() {
    const auto info=[](Sprite& sprite,Rect r) { ConfigureSurface(sprite,r,fieldsign::Info(r.width,r.height)); };
    info(statusPanel_,layout::status);
    info(economyPanel_,layout::economy);
    info(selectedTilePanel_,layout::selected);
    info(goalPanel_,layout::goal);
    info(feedbackPanel_,kFeedbackPanel);
    ConfigurePanel(goalBarTrack_, kGoalTrack, kTrackColor);
    ConfigurePanel(waterBarTrack_, kWaterTrack, kTrackColor);
    ConfigurePanel(growthBarTrack_, kGrowthTrack, kTrackColor);
    ConfigureIcon(dayIcon_, farmui::HUDIcon::Calendar, {layout::status.x + 10, layout::status.y + 10, 36, 36});
    ConfigureIcon(rankIcon_, farmui::HUDIcon::Rank, {layout::status.x + 132, layout::status.y + 16, 24, 24});
    ConfigureIcon(moneyIcon_, farmui::HUDIcon::Coin, {layout::economy.x + 10, layout::economy.y + 14, 28, 28});
    ConfigureIcon(inventoryIcon_, farmui::HUDIcon::Inventory, {layout::economy.x + 146, layout::economy.y + 14, 28, 28});
    ConfigureIcon(waterIcon_, farmui::HUDIcon::WaterDrop, {layout::selected.x + 12, layout::selected.y + 48, 24, 24});
    ConfigureIcon(growthIcon_, farmui::HUDIcon::Sprout, {layout::selected.x + 12, layout::selected.y + 100, 24, 24});
}

void FarmHUD::SetViewData(const FarmHUDViewData& viewData) {
    FarmHUDViewData sanitized = viewData;
    sanitized.day = (std::max)(sanitized.day, 1);
    sanitized.money = (std::max)(sanitized.money, 0);
    sanitized.rank = (std::max)(sanitized.rank, 1);
    sanitized.cropCount = (std::max)(sanitized.cropCount, 0);
    sanitized.goalMoney = (std::max)(sanitized.goalMoney, 1);
    sanitized.contestFinalDay = (std::max)(sanitized.contestFinalDay, 0);
    sanitized.goalProgress = std::isfinite(sanitized.goalProgress) ? std::clamp(sanitized.goalProgress, 0.0f, 1.0f) : 0.0f;
    if (!farm::IsPlantableCrop(sanitized.selectedSeedCrop)) sanitized.selectedSeedCrop = farm::CropType::Carrot;
    if (!farm::IsPlantableCrop(sanitized.cropPieHovered)) sanitized.cropPieHovered = farm::CropType::None;
    if (static_cast<std::size_t>(sanitized.feedback) >= feedbackTextureHandles_.size()) sanitized.feedback = FarmHUDFeedback::None;
    if (sanitized.feedback == FarmHUDFeedback::Harvest && farm::IsPlantableCrop(sanitized.feedbackCrop)) {
        sanitized.feedbackQualityScore = std::clamp(sanitized.feedbackQualityScore, 0, 100);
        sanitized.feedbackSaleCount = 0;
        sanitized.feedbackSaleValue = (std::max)(sanitized.feedbackSaleValue, 0);
    } else if (sanitized.feedback == FarmHUDFeedback::Sale) {
        if (!farm::IsPlantableCrop(sanitized.feedbackCrop)) sanitized.feedbackCrop = farm::CropType::None;
        sanitized.feedbackQualityScore = 0;
        sanitized.feedbackSaleCount = (std::max)(sanitized.feedbackSaleCount, 0);
        sanitized.feedbackSaleValue = (std::max)(sanitized.feedbackSaleValue, 0);
    } else {
        sanitized.feedbackCrop = farm::CropType::None;
        sanitized.feedbackQualityScore = sanitized.feedbackSaleCount = sanitized.feedbackSaleValue = 0;
    }
    if (!std::isfinite(sanitized.cropPieCenter.x) || !std::isfinite(sanitized.cropPieCenter.y)) {
        sanitized.cropPieOpen = false;
        sanitized.cropPieCenter = {};
    }
    if (!sanitized.irrigationPreviewActive) {
        sanitized.irrigationPreviewRemoval = false;
        sanitized.irrigationPreviewChangeCount = 0;
    } else sanitized.irrigationPreviewChangeCount = std::clamp(sanitized.irrigationPreviewChangeCount, 1, 999);
    sanitized.selectedTileValid = sanitized.selectedTileValid && sanitized.selectedTileIndex >= 0;
    if (!farm::IsValidFarmTileFeature(sanitized.selectedTileFeature)) {
        sanitized.selectedTileFeature = farm::FarmTileFeature::None;
        sanitized.selectedTileWaterStatus = farm::FarmWaterStatus::None;
        sanitized.selectedTileIrrigationSupplied = false;
    }
    if (sanitized.selectedTileWaterStatus > farm::FarmWaterStatus::Dry) sanitized.selectedTileWaterStatus = farm::FarmWaterStatus::None;
    if (sanitized.selectedTileMoistureStatus > FarmHUDMoistureStatus::Excess) sanitized.selectedTileMoistureStatus = FarmHUDMoistureStatus::None;
    if (!sanitized.selectedTileValid || sanitized.irrigationPreviewActive ||
        sanitized.selectedTileFeature != farm::FarmTileFeature::None) sanitized.selectedTileReceivedIrrigation = false;
    if (sanitized.selectedTileFeature == farm::FarmTileFeature::None) sanitized.selectedTileIrrigationSupplied = false;
    sanitized.selectedTileMoisturePercent = std::clamp(sanitized.selectedTileMoisturePercent, 0, 100);
    sanitized.selectedTileStoredWaterPercent = std::clamp(sanitized.selectedTileStoredWaterPercent, 0, 100);
    sanitized.selectedTileGrowthPercent = std::clamp(sanitized.selectedTileGrowthPercent, 0, 100);
    // Forecast owns the optimum. Invalid/reversed ranges hide the band, never invent defaults.
    sanitized.selectedTileMoistureRangeValid = sanitized.selectedTileMoistureRangeValid &&
        sanitized.selectedTileValid && sanitized.selectedTileFeature == farm::FarmTileFeature::None &&
        std::isfinite(sanitized.selectedTileGoodMoistureMinimum) && std::isfinite(sanitized.selectedTileGoodMoistureMaximum) &&
        sanitized.selectedTileGoodMoistureMinimum <= sanitized.selectedTileGoodMoistureMaximum;
    if (sanitized.selectedTileMoistureRangeValid) {
        sanitized.selectedTileGoodMoistureMinimum = std::clamp(sanitized.selectedTileGoodMoistureMinimum, 0.0f, 1.0f);
        sanitized.selectedTileGoodMoistureMaximum = std::clamp(sanitized.selectedTileGoodMoistureMaximum, 0.0f, 1.0f);
    } else sanitized.selectedTileGoodMoistureMinimum = sanitized.selectedTileGoodMoistureMaximum = 0.0f;
    for (std::size_t i = 0; i < sanitized.cropSeedCounts.size(); ++i) {
        sanitized.cropSeedCounts[i] = (std::max)(sanitized.cropSeedCounts[i], 0);
        sanitized.cropInventoryCounts[i] = (std::max)(sanitized.cropInventoryCounts[i], 0);
        sanitized.cropInventoryValues[i] = (std::max)(sanitized.cropInventoryValues[i], 0);
    }
    const bool statusChanged = viewData_.day != sanitized.day || viewData_.money != sanitized.money ||
        viewData_.rank != sanitized.rank || viewData_.cropCount != sanitized.cropCount;
    const bool goalChanged = statusChanged || viewData_.goalMoney != sanitized.goalMoney ||
        viewData_.goalProgress != sanitized.goalProgress || viewData_.goalCleared != sanitized.goalCleared ||
        viewData_.contestSeason != sanitized.contestSeason || viewData_.contestFinalDay != sanitized.contestFinalDay ||
        viewData_.freeFarming != sanitized.freeFarming;
    const bool selectedChanged = viewData_.selectedTileValid != sanitized.selectedTileValid ||
        viewData_.selectedTileHeight != sanitized.selectedTileHeight || viewData_.selectedTileFeature != sanitized.selectedTileFeature ||
        viewData_.selectedTileCrop != sanitized.selectedTileCrop || viewData_.selectedTileState != sanitized.selectedTileState ||
        viewData_.selectedTileGrowthStage != sanitized.selectedTileGrowthStage ||
        viewData_.selectedTileIrrigationSupplied != sanitized.selectedTileIrrigationSupplied ||
        viewData_.selectedTileReceivedIrrigation != sanitized.selectedTileReceivedIrrigation ||
        viewData_.selectedTileWaterStatus != sanitized.selectedTileWaterStatus ||
        viewData_.selectedTileMoistureStatus != sanitized.selectedTileMoistureStatus ||
        viewData_.selectedTileMoisturePercent != sanitized.selectedTileMoisturePercent ||
        viewData_.selectedTileStoredWaterPercent != sanitized.selectedTileStoredWaterPercent ||
        viewData_.selectedTileGrowthPercent != sanitized.selectedTileGrowthPercent ||
        viewData_.selectedTileMoistureRangeValid != sanitized.selectedTileMoistureRangeValid ||
        viewData_.selectedTileGoodMoistureMinimum != sanitized.selectedTileGoodMoistureMinimum ||
        viewData_.selectedTileGoodMoistureMaximum != sanitized.selectedTileGoodMoistureMaximum;
    const bool previewChanged = viewData_.irrigationPreviewActive != sanitized.irrigationPreviewActive ||
        viewData_.irrigationPreviewRemoval != sanitized.irrigationPreviewRemoval ||
        viewData_.irrigationPreviewChangeCount != sanitized.irrigationPreviewChangeCount;
    const bool feedbackChanged = viewData_.feedback != sanitized.feedback || viewData_.feedbackCrop != sanitized.feedbackCrop ||
        viewData_.feedbackQualityScore != sanitized.feedbackQualityScore ||
        viewData_.feedbackSaleCount != sanitized.feedbackSaleCount || viewData_.feedbackSaleValue != sanitized.feedbackSaleValue;
    const bool pieChanged = viewData_.cropPieOpen != sanitized.cropPieOpen ||
        viewData_.cropPieHovered != sanitized.cropPieHovered || viewData_.selectedSeedCrop != sanitized.selectedSeedCrop ||
        viewData_.cropPieCenter.x != sanitized.cropPieCenter.x || viewData_.cropPieCenter.y != sanitized.cropPieCenter.y ||
        viewData_.cropSeedCounts != sanitized.cropSeedCounts || viewData_.cropInventoryCounts != sanitized.cropInventoryCounts ||
        viewData_.cropInventoryValues != sanitized.cropInventoryValues;
    viewData_ = sanitized;
    if (!initialized_) return;
    if (statusChanged) UpdateStatus();
    if (goalChanged) UpdateGoal();
    if (selectedChanged) UpdateSelectedTile();
    if (previewChanged) UpdateIrrigationPreview();
    if (feedbackChanged) UpdateFeedback();
    if (pieChanged) UpdateCropPieMenu();
}
void FarmHUD::UpdateStatus() {
    char value[32]{};
    std::snprintf(value, sizeof(value), "%d", viewData_.day);
    dayText_.Set(font_, value, {layout::status.x + 50, layout::status.y + 12, 76, 32}, 0.90f, kTextColor);
    std::snprintf(value, sizeof(value), "%d", viewData_.rank);
    rankText_.Set(font_, value, {layout::status.x + 160, layout::status.y + 16, 44, 24}, 0.66f, kAccentColor);
    std::snprintf(value, sizeof(value), "%d", viewData_.money);
    moneyText_.Set(font_, value, {layout::economy.x + 42, layout::economy.y + 14, 98, 28}, 0.78f, kAccentColor);
    std::snprintf(value, sizeof(value), "%d", viewData_.cropCount);
    cropCountText_.Set(font_, value, {layout::economy.x + 178, layout::economy.y + 16, 66, 24}, 0.66f, kTextColor);
}
void FarmHUD::UpdateGoal() {
    char value[32]{};
    if (!viewData_.freeFarming) {
        if (viewData_.contestSeason && viewData_.contestFinalDay > 0)
            std::snprintf(value, sizeof(value), "DAY %d / %d", (std::min)(viewData_.day, viewData_.contestFinalDay), viewData_.contestFinalDay);
        else if (viewData_.contestSeason) std::snprintf(value, sizeof(value), "DAY %d / ?", viewData_.day);
        else if (viewData_.goalCleared) std::snprintf(value, sizeof(value), "%dG", viewData_.goalMoney);
        else std::snprintf(value, sizeof(value), "%d / %dG", viewData_.money, viewData_.goalMoney);
    }
    goalText_.Set(font_, value, {layout::goal.x + 12, layout::goal.y + 4, layout::goal.width - 24, 24}, 0.56f, kTextColor);
    ConfigurePanel(goalBarFill_, {kGoalTrack.x, kGoalTrack.y, kGoalTrack.width * viewData_.goalProgress, kGoalTrack.height},
        viewData_.goalCleared ? kAccentColor : kGrowthColor);
}
void FarmHUD::UpdateSelectedTile() {
    if (!viewData_.selectedTileValid) return;
    ConfigureIcon(selectedIcon_, SelectedIcon(viewData_), {layout::selected.x + 12, layout::selected.y + 8, 36, 36});
    const auto state = tileStateTextureHandles_[TileStateTextureIndex(viewData_)];
    SetLocalizedTexture(localizedSelectedName_, HasCrop(viewData_) ? cropTextureHandles_[CropTextureIndex(viewData_.selectedTileCrop)] : state,
        {layout::selected.x + 54, layout::selected.y + 7, 154, 34});
    SetLocalizedTexture(localizedTileState_, state, kGrowthStatus);
    char value[32]{};
    std::snprintf(value, sizeof(value), "H%d", viewData_.selectedTileHeight);
    selectedTileHeightText_.Set(font_, value, {layout::selected.x + 220, layout::selected.y + 12, 82, 24}, 0.58f, kMutedTextColor);
    const int water = viewData_.selectedTileFeature == farm::FarmTileFeature::None
        ? viewData_.selectedTileMoisturePercent : viewData_.selectedTileStoredWaterPercent;
    const Vector4 waterColor = MoistureColor(viewData_);
    std::snprintf(value, sizeof(value), "%d%%", water);
    selectedTileWaterText_.Set(font_, value, {layout::selected.x + 46, layout::selected.y + 48, 60, 24}, 0.62f, waterColor);
    ConfigurePanel(waterBarFill_, {kWaterTrack.x, kWaterTrack.y, kWaterTrack.width * static_cast<float>(water) / 100.0f, kWaterTrack.height}, waterColor);
    if (HasMoistureStatus(viewData_)) {
        const std::size_t index = static_cast<std::size_t>(viewData_.selectedTileMoistureStatus) - 1;
        SetLocalizedTexture(localizedMoistureStatus_, moistureStatusTextureHandles_[index], kWaterStatus);
        localizedMoistureStatus_.SetColor(waterColor);
    }
    if (HasIrrigationStatus(viewData_)) {
        const std::size_t index = viewData_.selectedTileReceivedIrrigation ? 4 : static_cast<std::size_t>(viewData_.selectedTileWaterStatus) - 1;
        SetLocalizedTexture(localizedIrrigationStatus_, irrigationStatusTextureHandles_[index], kWaterStatus);
    }
    if (viewData_.selectedTileMoistureRangeValid) {
        const float minimum = viewData_.selectedTileGoodMoistureMinimum;
        const float maximum = viewData_.selectedTileGoodMoistureMaximum;
        ConfigurePanel(waterRangeBand_, {kWaterTrack.x + minimum * kWaterTrack.width, kWaterTrack.y + 9,
            (maximum - minimum) * kWaterTrack.width, 3}, kRangeColor);
        constexpr float pointerWidth = 2;
        const float x = kWaterTrack.x + static_cast<float>(water) / 100.0f * kWaterTrack.width;
        ConfigurePanel(waterPointer_, {std::clamp(x - pointerWidth * 0.5f, kWaterTrack.x,
            kWaterTrack.x + kWaterTrack.width - pointerWidth), kWaterTrack.y - 2, pointerWidth, 10}, kTextColor);
    }
    std::snprintf(value, sizeof(value), "%d%%", viewData_.selectedTileGrowthPercent);
    selectedTileGrowthText_.Set(font_, value, {layout::selected.x + 46, layout::selected.y + 100, 60, 24}, 0.62f, kTextColor);
    ConfigurePanel(growthBarFill_, {kGrowthTrack.x, kGrowthTrack.y,
        kGrowthTrack.width * static_cast<float>(viewData_.selectedTileGrowthPercent) / 100.0f, kGrowthTrack.height}, kGrowthColor);
}
void FarmHUD::UpdateIrrigationPreview() {
    SetLocalizedTexture(localizedIrrigationPreview_, irrigationPreviewTextureHandles_[viewData_.irrigationPreviewRemoval ? 1 : 0],
        {500, 30, 248, 43}, 0.86f);
    char value[16]{};
    std::snprintf(value, sizeof(value), "x%d", viewData_.irrigationPreviewChangeCount);
    irrigationPreviewCountText_.Set(font_, value, {760, 36, 90, 30}, 0.70f, kAccentColor);
}
void FarmHUD::UpdateFeedback() {
    const bool crop = HasFeedbackCrop(viewData_);
    SetLocalizedTexture(localizedFeedback_, feedbackTextureHandles_[static_cast<std::size_t>(viewData_.feedback)],
        {kFeedbackPanel.x + kFeedbackPadding, kFeedbackPanel.y + 8, crop ? 160.0f : kFeedbackPanel.width - 2 * kFeedbackPadding, 40}, 0.72f);
    SetLocalizedTexture(localizedFeedbackCrop_, cropTextureHandles_[CropTextureIndex(viewData_.feedbackCrop)],
        {kFeedbackPanel.x + 182, kFeedbackPanel.y + 8, kFeedbackPanel.width - 198, 40}, 0.72f);
    char value[32]{};
    if (viewData_.feedback == FarmHUDFeedback::Harvest && crop)
        std::snprintf(value, sizeof(value), "Q%d  %dG", viewData_.feedbackQualityScore, viewData_.feedbackSaleValue);
    else if (viewData_.feedback == FarmHUDFeedback::Sale && viewData_.feedbackSaleCount > 0 && viewData_.feedbackSaleValue > 0)
        std::snprintf(value, sizeof(value), "x%d  +%dG", viewData_.feedbackSaleCount, viewData_.feedbackSaleValue);
    feedbackMetricsText_.Set(font_, value, {kFeedbackPanel.x + kFeedbackPadding, kFeedbackPanel.y + 52,
        kFeedbackPanel.width - 2 * kFeedbackPadding, 30}, 0.90f, kAccentColor);
}
void FarmHUD::UpdateCropPieMenu() {
    const Vector2 center = viewData_.cropPieCenter;
    const float halfGap = kPiePanelGap * 0.5f;
    const std::array<Vector2, 3> positions{{
        {center.x - halfGap - kPiePanelSize.x, center.y - kPiePanelSize.y * 0.5f},
        {center.x + halfGap, center.y - kPiePanelSize.y * 0.5f},
        {center.x - kPiePanelSize.x * 0.5f, center.y + 78},
    }};
    for (std::size_t i = 0; i < cropPiePanels_.size(); ++i) {
        const auto crop = farm::PlayableCrop(static_cast<int>(i));
        const bool hovered = crop == viewData_.cropPieHovered;
        const auto position = positions[i];
        ConfigureSurface(cropPiePanels_[i], {position.x, position.y, kPiePanelSize.x, kPiePanelSize.y},
            fieldsign::Button(kPiePanelSize.x,kPiePanelSize.y,crop == viewData_.selectedSeedCrop,hovered));
        SetLocalizedTexture(localizedCropPieNames_[i], cropTextureHandles_[CropTextureIndex(crop)],
            {position.x + 30, position.y + 9, kPiePanelSize.x - 48, 37}, 0.95f);
        localizedCropPieNames_[i].SetColor(hovered ? kSelectedSlotTextColor : kTextColor);
        SetLocalizedTexture(localizedCropPieTraits_[i], cropPieTraitTextureHandles_[i],
            {position.x + 18, position.y + 46, kPiePanelSize.x - 36, 28}, 0.72f);
        localizedCropPieTraits_[i].SetColor(hovered ? kSelectedSlotTextColor : kMutedTextColor);
        char value[32]{};
        const std::size_t slot = i + 1;
        std::snprintf(value, sizeof(value), "%d / %d", viewData_.cropSeedCounts[slot], viewData_.cropInventoryCounts[slot]);
        cropPieStatsText_[i].Set(font_, value, {position.x + 18, position.y + 80, kPiePanelSize.x - 36, 20},
            0.60f, hovered ? kSelectedSlotTextColor : kTextColor);
        std::snprintf(value, sizeof(value), "%dG", viewData_.cropInventoryValues[slot]);
        cropPieValueText_[i].Set(font_, value, {position.x + 18, position.y + 102, kPiePanelSize.x - 36, 18},
            0.56f, hovered ? kSelectedSlotTextColor : kAccentColor);
    }
    ConfigurePanel(cropPieCenterPanel_, {center.x - halfGap, center.y - 18, kPiePanelGap, 36}, kFeedbackPanelColor);
    SetLocalizedTexture(localizedCropPieGuide_, cropPieGuideTextureHandle_, {center.x - 180, center.y - 108, 360, 30}, 0.72f);
}

void FarmHUD::Update(float deltaTime) {
    (void)deltaTime;
    if (!initialized_) return;
    statusPanel_.Update();
    economyPanel_.Update();
    dayIcon_.Update();
    rankIcon_.Update();
    moneyIcon_.Update();
    inventoryIcon_.Update();
    dayText_.Update();
    rankText_.Update();
    moneyText_.Update();
    cropCountText_.Update();
    if (!viewData_.freeFarming) {
        goalPanel_.Update();
        goalBarTrack_.Update();
        if (viewData_.goalProgress > 0) goalBarFill_.Update();
        goalText_.Update();
    }
    if (viewData_.selectedTileValid) {
        selectedTilePanel_.Update();
        selectedIcon_.Update();
        localizedSelectedName_.Update();
        selectedTileHeightText_.Update();
        waterIcon_.Update();
        selectedTileWaterText_.Update();
        waterBarTrack_.Update();
        const int water = viewData_.selectedTileFeature == farm::FarmTileFeature::None
            ? viewData_.selectedTileMoisturePercent : viewData_.selectedTileStoredWaterPercent;
        if (water > 0) waterBarFill_.Update();
        if (HasMoistureStatus(viewData_)) localizedMoistureStatus_.Update();
        if (HasIrrigationStatus(viewData_)) localizedIrrigationStatus_.Update();
        if (viewData_.selectedTileMoistureRangeValid) {
            if (viewData_.selectedTileGoodMoistureMaximum > viewData_.selectedTileGoodMoistureMinimum) waterRangeBand_.Update();
            waterPointer_.Update();
        }
        if (HasCrop(viewData_)) {
            growthIcon_.Update();
            selectedTileGrowthText_.Update();
            localizedTileState_.Update();
            growthBarTrack_.Update();
            if (viewData_.selectedTileGrowthPercent > 0) growthBarFill_.Update();
        }
    }
    if (viewData_.irrigationPreviewActive) {
        localizedIrrigationPreview_.Update();
        irrigationPreviewCountText_.Update();
    }
    if (viewData_.cropPieOpen) {
        cropPieCenterPanel_.Update();
        for (auto& panel : cropPiePanels_) panel.Update();
        for (auto& name : localizedCropPieNames_) name.Update();
        for (auto& trait : localizedCropPieTraits_) trait.Update();
        for (auto& text : cropPieStatsText_) text.Update();
        for (auto& text : cropPieValueText_) text.Update();
        localizedCropPieGuide_.Update();
    }
    if (viewData_.feedback != FarmHUDFeedback::None) {
        feedbackPanel_.Update();
        localizedFeedback_.Update();
        if (HasFeedbackCrop(viewData_)) localizedFeedbackCrop_.Update();
        feedbackMetricsText_.Update();
    }
}

void FarmHUD::Draw(bool simpleField) {
    if (!initialized_) return;
    statusPanel_.Draw();
    dayIcon_.Draw();
    rankIcon_.Draw();
    dayText_.Draw();
    rankText_.Draw();
    if (!simpleField) {
        economyPanel_.Draw();
        moneyIcon_.Draw();
        inventoryIcon_.Draw();
        moneyText_.Draw();
        cropCountText_.Draw();
    }
    if (!simpleField && !viewData_.freeFarming) {
        goalPanel_.Draw();
        goalBarTrack_.Draw();
        if (viewData_.goalProgress > 0) goalBarFill_.Draw();
        goalText_.Draw();
    }
    if (!simpleField && viewData_.selectedTileValid) {
        selectedTilePanel_.Draw();
        selectedIcon_.Draw();
        localizedSelectedName_.Draw();
        selectedTileHeightText_.Draw();
        waterIcon_.Draw();
        selectedTileWaterText_.Draw();
        waterBarTrack_.Draw();
        const int water = viewData_.selectedTileFeature == farm::FarmTileFeature::None
            ? viewData_.selectedTileMoisturePercent : viewData_.selectedTileStoredWaterPercent;
        if (water > 0) waterBarFill_.Draw();
        if (HasMoistureStatus(viewData_)) localizedMoistureStatus_.Draw();
        if (HasIrrigationStatus(viewData_)) localizedIrrigationStatus_.Draw();
        if (viewData_.selectedTileMoistureRangeValid) {
            if (viewData_.selectedTileGoodMoistureMaximum > viewData_.selectedTileGoodMoistureMinimum) waterRangeBand_.Draw();
            waterPointer_.Draw();
        }
        if (HasCrop(viewData_)) {
            growthIcon_.Draw();
            selectedTileGrowthText_.Draw();
            localizedTileState_.Draw();
            growthBarTrack_.Draw();
            if (viewData_.selectedTileGrowthPercent > 0) growthBarFill_.Draw();
        }
    }
    if (viewData_.irrigationPreviewActive) {
        localizedIrrigationPreview_.Draw();
        irrigationPreviewCountText_.Draw();
    }
    if (viewData_.cropPieOpen) {
        cropPieCenterPanel_.Draw();
        for (auto& panel : cropPiePanels_) panel.Draw();
        for (auto& name : localizedCropPieNames_) name.Draw();
        for (auto& trait : localizedCropPieTraits_) trait.Draw();
        for (auto& text : cropPieStatsText_) text.Draw();
        for (auto& text : cropPieValueText_) text.Draw();
        localizedCropPieGuide_.Draw();
    }
    if (viewData_.feedback != FarmHUDFeedback::None) {
        feedbackPanel_.Draw();
        localizedFeedback_.Draw();
        if (HasFeedbackCrop(viewData_)) localizedFeedbackCrop_.Draw();
        feedbackMetricsText_.Draw();
    }
}
