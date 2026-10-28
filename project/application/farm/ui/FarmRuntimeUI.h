#pragma once
#include "base/InitializationTask.h"
#include "farm/ui/FarmRuntimeLabels.h"
#include "farm/ui/FarmHUDLayout.h"
#include "farm/ui/FarmHUDIcons.h"
#include "farm/system/FarmContestPresentationSystem.h"
#include "2d/Sprite.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace farmui {
enum class Action {
    None, Menu, Close, Tab, Tool, Crop, Buy, Sell, SellAll,
    Raise, Lower, Canal, Source, Path, RemovePath, Confirm, Cancel, Undo, Redo, SetIrrigation,
    Save, SaveCopy, Load, Previous, Next, Restart, Accept, ReturnTitle,
    PinA, PinB, Start, Stop, Clear, Pause, Speed, Follow, Overview, Exit, LayoutLibrary,
    ObserveField, ObserveExit, PickSlot, JumpSlot, FlowContinue, FlowRecords,
    ApplyTool, OpenSeedShop, BuyAndReturn, SoilCare, Compost, Quality, TerrainField, TerrainExit,
    HarvestInventory, HarvestPage, ProtectHarvest, UnprotectHarvest, ReserveContestHarvest, CancelContestReservation,
    ContestPreview, SubmitContest, ContestResults, ContestDayReview, ContestDayPrepare, ContestDayResume, ChangePlayMode,
    ShopSelect, ShopQuantity, ShopReview, ShopConfirm, ShopCancel, ShopClose,
    HarvestDisplay, HarvestDisplaySelect, HarvestDisplayPage, StartSkip, StageGuide,
    ContestDetail, ContestDetailClose, ContestReveal, ContestMissedContinue, ContestOutcome, SeasonRetry, ContestOutcomeReveal,
    ContestTab, ContestExplanation,
    ExhibitionOpen, ExhibitionSelect, ExhibitionWatch, ExhibitionHome, ExhibitionExit
};
struct Request { Action action = Action::None; int argument = 0; std::uint64_t inventoryGeneration = 0; };
struct Item {
    Label label = Label::Menu;
    Rect rect{};
    Request request{};
    bool enabled = true;
    bool selected = false;
    std::string value;
    bool focused = false;
    float valueOffset = 540;
    bool darkInk = false;
    bool warning = false;
    HUDIcon icon = HUDIcon::None;
    float opacity = 1;
};
[[nodiscard]] inline Rect IconCaptionRect(const Item& item) noexcept {
    const auto index = static_cast<std::size_t>(item.label);
    if (index >= kLabels.size()) return {};
    const float width = (std::min)(360.0f, kLabels[index].width * 0.78f + 24);
    // One reserved caption line keeps hover names off the field and other controls.
    return {640.0f-width*0.5f, 576, width, 38};
}
struct QualityRadar {
    bool visible = false;
    bool numberedAxes = true;
    float opacity = 1;
    Vector2 offset{};
    float scale = 1;
    Vector4 color{1, .81f, .25f, 1};
    std::array<float, 4> values{};
    std::array<bool, 4> known{};
    inline static constexpr Vector2 center{366, 312};
    inline static constexpr float radius = 90;
    // Include the complete glyph rectangle when reserving space around the diagram.
    inline static constexpr std::array<Rect, 4> axisLabels{{
        {center.x-12, center.y-radius-40, 24, 36},
        {center.x+radius+12, center.y-18, 24, 36},
        {center.x-12, center.y+radius+4, 24, 36},
        {center.x-radius-36, center.y-18, 24, 36}
    }};
    inline static constexpr std::array<Vector2, 4> directions{{{0,-1}, {1,0}, {0,1}, {-1,0}}};
    [[nodiscard]] static Vector2 Point(std::size_t axis, float value) noexcept {
        if (axis >= directions.size() || !std::isfinite(value)) return center;
        const float distance = radius * std::clamp(value, 0.0f, 1.0f);
        return {center.x + directions[axis].x * distance, center.y + directions[axis].y * distance};
    }
};
struct View {
    static constexpr std::size_t kCapacity = 40;
    std::array<Item, kCapacity> items{};
    std::size_t count = 0;
    bool modal = false;
    bool briefing = false;
    bool starting = false;
    float cinematicBars = 1.0f;
    float cinematicCover = 0.0f;
    bool seedShop = false;
    bool harvestDisplay = false;
    int counterSelection = -1;
    std::array<int, 2> harvestFigures{{-1, -1}};
    bool observation = false;
    bool terrain = false;
    bool fieldActions = false;
    bool feedback = false;
    bool waterGuidance = false;
    bool compactHUD = false;
    bool hotbarVisible = false;
    bool selectedTile = false;
    bool goalVisible = false;
    QualityRadar radar{};
    QualityRadar referenceRadar{};
    bool contestDetail = false;
    FarmContestPresentationSystem::Frame contestFrame{};
    bool contestOutcome = false;
    bool seasonOutcome = false;
    bool outcomePositive = false;
    float outcomeReveal = 0;
    float outcomeVerdictReveal = 1;
    std::array<float, 3> outcomeRowReveal{{1,1,1}};
    std::array<float, 3> outcomePoints{};
    std::array<float, 3> outcomeTargets{};
    std::array<bool, 3> outcomePassed{};
    inline static constexpr Rect kFeedbackPanel{352, 82, 504, 92};
    inline static constexpr Rect kFieldActionsPanel = HUDLayout::fieldActions;
    inline static constexpr Rect kWaterGuidancePanel = HUDLayout::waterGuidance;
    inline static constexpr Rect kObservationTop{16, 16, 1248, 52};
    inline static constexpr Rect kObservationBottom{16, 408, 1248, 280};
    inline static constexpr Rect kTerrainTop{16, 16, 1248, 96};
    inline static constexpr Rect kTerrainBottom{16, 520, 1248, 184};
    inline static constexpr auto kFarmHudPanels = HUDLayout::panels;
    std::array<std::string, 2> recordNames{};
    void Add(Label label, Rect rect, Request request = {}, bool enabled = true, bool selected = false) {
        if (count < items.size()) items[count++] = {label, rect, request, enabled, selected, {}};
    }
    void IconButton(Label label, HUDIcon icon, Rect rect, Request request,
        bool enabled = true, bool selected = false, std::string badge = {}) {
        if (count >= items.size()) return;
        Add(label, rect, request, enabled, selected);
        items[count - 1].icon = icon;
        items[count - 1].value = std::move(badge);
    }
    void Value(Label label, float y, std::string value) {
        if (count < items.size()) items[count++] = {label, {176, y, 900, 38}, {}, true, false, std::move(value)};
    }
    void Metric(Label label, Rect rect, float offset, std::string value) {
        if (count >= items.size()) return;
        items[count++] = {label, rect, {}, true, false, std::move(value), false, offset};
    }
    [[nodiscard]] bool Covers(Vector2 point) const noexcept {
        if (compactHUD && (HUDLayout::status.Contains(point) || HUDLayout::economy.Contains(point) ||
            (hotbarVisible && HUDLayout::tools.Contains(point)) || (selectedTile && HUDLayout::selected.Contains(point)) ||
            (goalVisible && HUDLayout::goal.Contains(point)))) return true;
        if (modal || (observation && (kObservationTop.Contains(point) || kObservationBottom.Contains(point))) ||
            (terrain && (kTerrainTop.Contains(point) || kTerrainBottom.Contains(point))) ||
            (fieldActions && kFieldActionsPanel.Contains(point)) ||
            (waterGuidance && kWaterGuidancePanel.Contains(point)) ||
            (feedback && kFeedbackPanel.Contains(point))) return true;
        // Disabled controls and labels still own their screen area.
        for (std::size_t i = 0; i < count && i < items.size(); ++i)
            if (items[i].rect.Contains(point) ||
                (compactHUD && items[i].selected && items[i].request.action == Action::Tool &&
                    IsValidIcon(items[i].icon) && IconCaptionRect(items[i]).Contains(point))) return true;
        return false;
    }
    [[nodiscard]] Request Hit(Vector2 point) const noexcept {
        for (std::size_t i = count < items.size() ? count : items.size(); i > 0; --i) {
            const auto& item = items[i - 1];
            if (item.enabled && item.request.action != Action::None && item.rect.Contains(point)) return item.request;
        }
        return {};
    }
    [[nodiscard]] static bool FarmHUDCovers(Vector2 point) noexcept {
        for (const auto& panel : kFarmHudPanels) if (panel.Contains(point)) return true;
        return false;
    }
    [[nodiscard]] int NextActionable(int current, int direction) const noexcept {
        const int size = static_cast<int>(count < items.size() ? count : items.size());
        if (size == 0 || direction == 0) return -1;
        int index = current >= 0 && current < size ? current : (direction > 0 ? size - 1 : 0);
        for (int i = 0; i < size; ++i) {
            index = (index + (direction > 0 ? 1 : size - 1)) % size;
            if (items[index].enabled && items[index].request.action != Action::None) return index;
        }
        return -1;
    }
};

// Presentation only. Pools are allocated once; every submitted quad has distinct constants.
class RuntimeUI final {
public:
    bool Initialize(SpriteCommon* common);
    InitializationTask InitializeIncrementally(SpriteCommon* common);
    void PrepareNames(const View& view);
    void Draw(const View& view, Vector2 pointer);
private:
    void Panel(Rect rect, Vector4 color);
    void LabelQuad(Label label, Rect rect, Vector4 color);
    void ValueText(const std::string& value, Vector2 position, float right,
        Vector4 color = {1.0f, 0.87f, 0.50f, 1}, float scale = 1.0f);
    void IconQuad(HUDIcon icon, Rect rect, Vector4 color);
    void DrawRadar(const QualityRadar& radar);
    std::array<Sprite, 84> panels_{};
    std::array<Sprite, 40> labels_{};
    std::array<Sprite, 160> digits_{};
    std::array<Sprite, 56> radarLines_{};
    std::array<Sprite, View::kCapacity> icons_{};
    std::array<Sprite, 4> seedPackets_{};
    // Distinct per displayed slot: do not rewrite a submitted Sprite constant buffer.
    std::array<Sprite, 8> harvestFigures_{};
    Sprite names_;
    std::array<std::string, 2> cachedNames_{};
    bool namesReady_ = false;
    std::size_t panelCount_ = 0, labelCount_ = 0, digitCount_ = 0, iconCount_ = 0;
    std::size_t radarLineCount_ = 0;
    bool ready_ = false;
    float opacity_ = 1.0f;
};
}
