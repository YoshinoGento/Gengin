#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmInputSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/ui/FarmHotbarView.h"
#include "io/Input.h"

bool FarmRuntimeController::UpdateHotbarFocus(const FarmRuntimeContext& s, const Input& input,
    farm::FieldToolWheel previousWheel, bool insideViewport) {
    using farmui::Action;
    if (!view_.hotbarVisible || view_.modal || s.host.IsTimelineScrubbing() ||
        s.farmProgressionSystem.IsCleared() || s.farmTimeAdvance.Read().status.active ||
        input.TriggerMouseButton(InputMouseButton::Left) || input.ReleaseMouseButton(InputMouseButton::Left) ||
        input.PushMouseButton(InputMouseButton::Left) || input.PushMouseButton(InputMouseButton::Right) ||
        input.TriggerMouseButton(InputMouseButton::Right) || input.PushMouseButton(InputMouseButton::Middle) ||
        input.TriggerMouseButton(InputMouseButton::Middle)) {
        hotbarSlot_ = -1;
        return false;
    }
    const bool seedShortcut = input.TriggerKey(InputKey::C);
    const auto previous = hotbarSlot_;
    const auto seedSlot = [&] {
        return farmui::HotbarSlot({Action::Crop, farm::ToCropSlot(s.farmCropSelectionSystem.GetSelectedCrop())});
    };
    constexpr int firstSeed = farmui::kFirstSeedSlot;
    const int delta = input.GetMouseWheelDelta();
    if (delta && previous < firstSeed && !seedShortcut) {
        hotbarSlot_ = -1;
        return false; // Normal scrolling equips tools.
    }
    if (previous < 0) {
        if (seedShortcut) hotbarSlot_ = seedSlot();
        else if (view_.hotbarAtFieldEdge && farm::ReadFieldStep(input).y < 0)
            hotbarSlot_ = farmui::HotbarSlot({Action::Tool, static_cast<int>(s.farmToolSystem.GetCurrentTool())});
        if (hotbarSlot_ < 0) return false;
    } else if (input.TriggerKey(InputKey::Escape) || input.TriggerKey(InputKey::ArrowUp)) {
        hotbarSlot_ = -1;
    } else if (FarmInputSystem::SelectToolFromKeys(input, {}, s.farmToolSystem)) {
        hotbarSlot_ = -1;
    } else if (input.TriggerKey(InputKey::Enter) || (seedShortcut && previous >= firstSeed)) {
        const auto request = farmui::FocusedHotbarRequest(view_);
        hotbarSlot_ = -1;
        if (farmui::HotbarSlot(request) == previous) {
            if (request.action == Action::Tool && request.argument == static_cast<int>(FarmTool::Seed)) hotbarSlot_ = seedSlot();
            else Execute(s, request);
        }
    } else if (seedShortcut) {
        hotbarSlot_ = seedSlot();
    } else {
        const int dx = static_cast<int>(input.TriggerKey(InputKey::ArrowRight)) - input.TriggerKey(InputKey::ArrowLeft);
        hotbarSlot_ = (previous + dx + farmui::kHotbarSlots) % farmui::kHotbarSlots;
    }
    // Keep partial detents only within the same picker, never between seed and tool selection.
    if (hotbarSlot_ >= firstSeed && insideViewport && s.host.IsViewportHovered() &&
        (farmui::HUDLayout::tools.Contains(pointer_) || !view_.Covers(pointer_))) {
        if (hotbarSlot_ == previous) fieldWheel_ = previousWheel;
        constexpr int count = farm::kPlayableCropCount;
        const int step = fieldWheel_.Sample(delta) % count;
        hotbarSlot_ = firstSeed + (hotbarSlot_ - firstSeed + step + count) % count;
    }
    wheelCaptured_ = delta != 0;
    if (hotbarSlot_ != previous) BuildView(s);
    return true;
}
