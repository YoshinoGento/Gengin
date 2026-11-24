#pragma once
#include "farm/ui/FarmRuntimeUI.h"
#include "farm/ui/FarmHUD.h"

namespace farmui {
inline constexpr std::array kToolLabels{Label::Hoe, Label::Water, Label::Seed, Label::Harvest};
inline constexpr int kFirstSeedSlot = static_cast<int>(kToolLabels.size());
inline constexpr int kHotbarSlots = kFirstSeedSlot + farm::kPlayableCropCount;
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
inline void BuildHotbarView(View& view, const FarmHUDViewData& state, bool enabled, int focus = -1) {
    view.hotbarVisible = true;
    const auto first = view.count;
    constexpr std::array toolIcons{HUDIcon::Hoe, HUDIcon::WateringCan, HUDIcon::SeedBag, HUDIcon::HarvestBasket};
    for (int i = 0; i < kFirstSeedSlot; ++i)
        view.IconButton(kToolLabels[i], toolIcons[i], HUDLayout::ToolSlot(i), {Action::Tool, i},
            enabled, state.currentToolIndex == i);
    constexpr std::array cropLabels{Label::Carrot, Label::Tomato, Label::Pumpkin};
    constexpr std::array cropIcons{HUDIcon::SeedCarrot, HUDIcon::SeedTomato, HUDIcon::SeedPumpkin};
    for (int i = 0; i < farm::kPlayableCropCount; ++i) {
        const int slot = i + 1;
        view.IconButton(cropLabels[i], cropIcons[i], HUDLayout::SeedSlot(i), {Action::Crop, slot},
            enabled, state.selectedSeedCrop == farm::CropTypeFromSlot(slot),
            std::to_string((std::max)(0, state.cropSeedCounts[slot])));
    }
    if (enabled && focus >= 0)
        for (auto i = first; i < view.count; ++i) view.items[i].focused = HotbarSlot(view.items[i].request) == focus;
}
}
