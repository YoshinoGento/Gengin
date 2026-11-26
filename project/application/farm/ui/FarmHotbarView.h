#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmHUD.h"

namespace farmui {
inline constexpr std::array kToolLabels{Label::Hoe, Label::Water, Label::Seed, Label::Harvest};
inline constexpr int kFirstSeedSlot = static_cast<int>(kToolLabels.size());
inline constexpr int kHotbarSlots = kFirstSeedSlot + farm::kPlayableCropCount;
inline constexpr std::array kToolIcons{HUDIcon::Hoe, HUDIcon::WateringCan, HUDIcon::SeedBag, HUDIcon::HarvestBasket};
inline constexpr std::array kCropIcons{HUDIcon::SeedCarrot, HUDIcon::SeedTomato, HUDIcon::SeedPumpkin};
inline constexpr std::array kCropLabels{Label::Carrot, Label::Tomato, Label::Pumpkin};
[[nodiscard]] constexpr std::optional<WheelItem> WheelItemForSlot(int slot, bool candidate) noexcept {
    constexpr std::array names{Label::WheelHoe, Label::WheelWater, Label::WheelBag, Label::WheelBasket};
    constexpr std::array candidates{Label::WheelCandidateCarrot, Label::WheelCandidateTomato, Label::WheelCandidatePumpkin};
    static_assert(names.size() == kFirstSeedSlot && kToolIcons.size() == names.size());
    static_assert(kCropLabels.size() == farm::kPlayableCropCount && candidates.size() == kCropIcons.size() && kCropIcons.size() == kCropLabels.size());
    if (slot < 0 || slot >= kHotbarSlots) return {};
    if (slot < kFirstSeedSlot) {
        if (candidate) return {};
        return WheelItem{names[slot], kToolIcons[slot]};
    }
    const auto crop = slot - kFirstSeedSlot;
    return WheelItem{candidate ? candidates[crop] : kCropLabels[crop], kCropIcons[crop]};
}
// Never register the floating hint as an item: the ground underneath must stay clickable.
inline constexpr float kWheelHintRowHeight = 30.f;
[[nodiscard]] inline Rect PlaceFieldWheelHint(const View& view, Vector2 pointer, float width,
    float height = kWheelHintRowHeight) noexcept {
    constexpr Rect canvas{0, 0, 1280, 720};
    constexpr float gapX = 16, gapY = 12, inset = 4;
    if (view.fieldWheelHint == WheelHint::Hidden || !canvas.Contains(pointer) || view.Covers(pointer)) return {};
    for (float y : {pointer.y + gapY, pointer.y - height - gapY}) {
        for (float x : {pointer.x + gapX, pointer.x - width - gapX}) {
            if (x < inset || y < inset || x + width > canvas.width - inset || y + height > canvas.height - inset) continue;
            if (!view.AnyHUDRegion([=](Rect r) {
                    return x < r.x + r.width && x + width > r.x && y < r.y + r.height && y + height > r.y;
                })) return {x, y, width, height};
        }
    }
    return {};
}
[[nodiscard]] constexpr int ScrollSeed(int oneBasedSlot, int steps) noexcept {
    constexpr int count = farm::kPlayableCropCount;
    return 1 + (oneBasedSlot - 1 + steps % count + count) % count;
}
[[nodiscard]] inline int HotbarSlot(Request r) noexcept {
    if (r.action == Action::Tool && r.argument >= 0 && r.argument < kFirstSeedSlot) return r.argument;
    if (r.action == Action::Crop && r.argument > 0 && r.argument <= farm::kPlayableCropCount)
        return kFirstSeedSlot + r.argument - 1;
    return -1;
}
[[nodiscard]] inline Request FocusedHotbarRequest(const View& view) noexcept {
    if (view.hotbarVisible && !view.modal)
        for (std::size_t i = 0; i < view.count; ++i) {
            const auto& item = view.items[i];
            if (item.enabled && item.focused && HotbarSlot(item.request) >= 0) return item.request;
        }
    return {};
}
[[nodiscard]] inline Label HoverCaption(const Item& item) noexcept {
    constexpr std::array labels{Label::HoverHoe, Label::HoverWater, Label::HoverBag, Label::HoverBasket,
        Label::HoverCarrot, Label::HoverTomato, Label::HoverPumpkin};
    static_assert(labels.size() == kHotbarSlots);
    const int slot = HotbarSlot(item.request);
    return slot >= 0 && item.enabled ? labels[slot] : item.label;
}
inline void BuildHotbarView(View& view, const FarmHUDViewData& state, bool enabled, int focus = -1) {
    view.hotbarVisible = true;
    const auto first = view.count;
    for (int i = 0; i < kFirstSeedSlot; ++i)
        view.IconButton(kToolLabels[i], kToolIcons[i], HUDLayout::ToolSlot(i), {Action::Tool, i},
            enabled, state.currentToolIndex == i);
    for (int i = 0; i < farm::kPlayableCropCount; ++i) {
        const int slot = i + 1;
        view.IconButton(kCropLabels[i], kCropIcons[i], HUDLayout::SeedSlot(i), {Action::Crop, slot},
            enabled, state.selectedSeedCrop == farm::CropTypeFromSlot(slot),
            std::to_string((std::max)(0, state.cropSeedCounts[slot])));
    }
    if (enabled && focus >= 0)
        for (auto i = first; i < view.count; ++i) view.items[i].focused = HotbarSlot(view.items[i].request) == focus;
}
}
