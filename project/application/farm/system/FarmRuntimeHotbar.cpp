#include "farm/system/FarmRuntimeController.h"
#include "farm/system/FarmRuntimeContext.h"
#include "farm/system/FarmCropSelectionSystem.h"
#include "farm/system/FarmInputSystem.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmTimeAdvanceControlSystem.h"
#include "farm/system/FarmToolSystem.h"
#include "farm/ui/FarmHotbarView.h"
#include "io/Input.h"

farm::FieldWheelRoute FarmRuntimeController::WheelRoute(const FarmRuntimeContext& s) const {
    return farm::ResolveFieldWheelRoute(hotbarSlot_ >= farmui::kFirstSeedSlot, s.farmToolSystem.WheelSelectsSeeds());
}

void FarmRuntimeController::UpdateFieldWheelHint(const FarmRuntimeContext& s, const Input& input, int hoveredTile) {
    view_.fieldWheelHint = farmui::WheelHint::Hidden;
    view_.wheelItem.reset();
    if (hoveredTile < 0 || !view_.hotbarVisible || view_.modal || HidesFarmHUD() ||
        s.host.IsTimelineScrubbing() || s.farmTimeAdvance.Read().status.active ||
        input.PushMouseButton(InputMouseButton::Right) || input.PushMouseButton(InputMouseButton::Middle) ||
        input.PushMouseButton(InputMouseButton::Left)) {
        wheelNotice_.Clear();
        return;
    }
    const auto route = WheelRoute(s);
    view_.fieldWheelHint = route == farm::FieldWheelRoute::Tools ?
        farmui::WheelHint::Tools : farmui::WheelHint::Seeds;
    const int slot = CurrentWheelSlot(s, route);
    if (wheelNotice_.Visible(route, slot, farm::FieldWheelNotice::Clock::now()))
        view_.wheelItem = farmui::WheelItemForSlot(slot, route == farm::FieldWheelRoute::SeedPicker);
}

int FarmRuntimeController::CurrentWheelSlot(const FarmRuntimeContext& s, farm::FieldWheelRoute route) const {
    if (route == farm::FieldWheelRoute::SeedPicker) return hotbarSlot_;
    if (route == farm::FieldWheelRoute::SeedKit)
        return farmui::HotbarSlot({farmui::Action::Crop, farm::ToCropSlot(s.farmCropSelectionSystem.GetSelectedCrop())});
    return farmui::HotbarSlot({farmui::Action::Tool, static_cast<int>(s.farmToolSystem.GetCurrentTool())});
}

bool FarmRuntimeController::UpdateRightTap(const FarmRuntimeContext& s, const Input& input,
    farm::FieldRightTap gesture, bool insideViewport) {
    using Button = InputMouseButton;
    const bool press = input.TriggerMouseButton(Button::Right), held = input.PushMouseButton(Button::Right),
        release = input.ReleaseMouseButton(Button::Right);
    if (!(press || held || release) || !view_.hotbarVisible || view_.modal || !insideViewport ||
        !AllowsFieldHover(s) || s.host.IsTimelineScrubbing() || s.farmTimeAdvance.Read().status.active) return false;
    // Camera chords take ownership immediately, even without pointer motion.
    const bool chord = input.GetMouseWheelDelta() != 0 || input.PushMouseButton(Button::Left) ||
        input.TriggerMouseButton(Button::Left) || input.ReleaseMouseButton(Button::Left) ||
        input.PushMouseButton(Button::Middle) || input.TriggerMouseButton(Button::Middle) ||
        input.PushKey(InputKey::W) || input.PushKey(InputKey::A) || input.PushKey(InputKey::S) ||
        input.PushKey(InputKey::D) || input.PushKey(InputKey::Q) || input.PushKey(InputKey::E);
    const bool eligible = !chord && (!(press || release) || ResolveHoveredTile(s) >= 0);
    const auto now = farm::FieldRightTap::Clock::now();
    const auto result = gesture.Sample(press, held, release, eligible, pointer_.x, pointer_.y,
        s.farmGrid.GetGeneration(), now);
    if (result == farm::FieldRightTap::Result::Pass) return false;
    if (result == farm::FieldRightTap::Result::Pending) rightTap_ = gesture;
    else {
        hotbarSlot_ = -1;
        const int previousSlot = CurrentWheelSlot(s, WheelRoute(s));
        s.farmToolSystem.ToggleSeedKit();
        const auto route = WheelRoute(s);
        wheelNotice_.Record(route, previousSlot, CurrentWheelSlot(s, route), now);
        BuildView(s);
    }
    return true;
}

bool FarmRuntimeController::UpdateHotbarFocus(const FarmRuntimeContext& s, const Input& input,
    farm::FieldToolWheel previousWheel, bool insideViewport) {
    using farmui::Action;
    if (!view_.hotbarVisible || view_.modal || s.host.IsTimelineScrubbing() ||
        s.farmProgressionSystem.IsCleared() || s.farmTimeAdvance.Read().status.active ||
        input.PushMouseButton(InputMouseButton::Right) ||
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
    if (input.TriggerMouseButton(InputMouseButton::Left)) {
        const auto hit = insideViewport ? view_.Hit(pointer_) : farmui::Request{};
        if (hit.action == Action::Tool && hit.argument == static_cast<int>(FarmTool::Seed)) {
            hotbarSlot_ = seedSlot();
            BuildView(s);
            return true;
        }
        hotbarSlot_ = -1;
        return false;
    }
    // Holding or releasing the click that opened the picker must not close it or work the field.
    if (input.PushMouseButton(InputMouseButton::Left) || input.ReleaseMouseButton(InputMouseButton::Left))
        return hotbarSlot_ >= 0;
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
    if (WheelRoute(s) == farm::FieldWheelRoute::SeedPicker && insideViewport && s.host.IsViewportHovered() &&
        (farmui::HUDLayout::tools.Contains(pointer_) || !view_.Covers(pointer_))) {
        if (hotbarSlot_ == previous) fieldWheel_ = previousWheel;
        const int beforeWheel = hotbarSlot_;
        hotbarSlot_ = firstSeed + farmui::ScrollSeed(hotbarSlot_ - firstSeed + 1, fieldWheel_.Sample(delta)) - 1;
        if (beforeWheel != hotbarSlot_)
            wheelNotice_.Record(farm::FieldWheelRoute::SeedPicker, beforeWheel, hotbarSlot_, farm::FieldWheelNotice::Clock::now());
    }
    wheelCaptured_ = delta != 0;
    if (hotbarSlot_ != previous) BuildView(s);
    return true;
}
