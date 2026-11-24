#include "farm/system/FarmInputSystem.h"
#include "farm/system/FarmFieldInput.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolSystem.h"
#include "io/Input.h"

farm::FieldStep farm::ReadFieldStep(const Input& input, bool includeDPad) {
    const auto pressed = [&](InputKey key, InputGamepadButton pad) {
        return input.TriggerKey(key) || (includeDPad && input.TriggerGamepadButton(pad));
    };
    return FieldStep::FromArrows(pressed(InputKey::ArrowUp, InputGamepadButton::DPadUp),
        pressed(InputKey::ArrowDown, InputGamepadButton::DPadDown),
        pressed(InputKey::ArrowLeft, InputGamepadButton::DPadLeft),
        pressed(InputKey::ArrowRight, InputGamepadButton::DPadRight));
}

FarmLockedInputResult FarmInputSystem::PollLockedInput(
	const Input& input,
	const FarmInputContext& context) const
{
	FarmLockedInputResult result{};
	if (!context.keyboardEnabled) {
		return result;
	}

	result.navigationInputConsumed =
		input.PushKey(InputKey::ArrowUp) ||
		input.PushKey(InputKey::ArrowDown) ||
		input.PushKey(InputKey::ArrowLeft) ||
		input.PushKey(InputKey::ArrowRight);
	result.actionTriggered =
		input.TriggerKey(InputKey::ArrowUp) ||
		input.TriggerKey(InputKey::ArrowDown) ||
		input.TriggerKey(InputKey::ArrowLeft) ||
		input.TriggerKey(InputKey::ArrowRight) ||
		input.TriggerKey(InputKey::PageUp) ||
		input.TriggerKey(InputKey::PageDown) ||
		input.TriggerKey(InputKey::Num1) ||
		input.TriggerKey(InputKey::Num2) ||
		input.TriggerKey(InputKey::Num3) ||
		input.TriggerKey(InputKey::Num4) ||
		input.TriggerKey(InputKey::Q) ||
		input.TriggerKey(InputKey::E) ||
		input.TriggerKey(InputKey::C) ||
		input.TriggerKey(InputKey::N) ||
		input.TriggerKey(InputKey::M) ||
		input.TriggerKey(InputKey::B) ||
		input.TriggerKey(InputKey::V) ||
		input.TriggerKey(InputKey::Enter) ||
		input.TriggerKey(InputKey::F);
	return result;
}

bool FarmInputSystem::SelectToolFromKeys(const Input& input, const FarmInputContext& context, FarmToolSystem& tools) {
	if (!context.keyboardEnabled) return false;
	if (context.directToolSelectionEnabled) {
		if (input.TriggerKey(InputKey::Num1)) { tools.SetTool(FarmTool::Hoe); return true; }
		if (input.TriggerKey(InputKey::Num2)) { tools.SetTool(FarmTool::Water); return true; }
		if (input.TriggerKey(InputKey::Num3)) { tools.SetTool(FarmTool::Seed); return true; }
		if (input.TriggerKey(InputKey::Num4)) { tools.SetTool(FarmTool::Harvest); return true; }
	}
	if (context.cameraDragActive) return false;
	if (input.TriggerKey(InputKey::E)) { tools.SelectNextTool(); return true; }
	if (input.TriggerKey(InputKey::Q)) { tools.SelectPreviousTool(); return true; }
	return false;
}

FarmInputResult FarmInputSystem::Update(
	const Input& input,
	const FarmInputContext& context,
	farm::FarmGrid& grid,
	FarmToolSystem& toolSystem,
	farm::CropType selectedCrop,
	FarmEconomySystem& economySystem,
	FarmToolActionSystem& actionSystem) const
{
	FarmInputResult result{};
	if (!context.keyboardEnabled) {
		return result;
	}

	result.navigationInputConsumed =
		input.PushKey(InputKey::ArrowUp) ||
		input.PushKey(InputKey::ArrowDown) ||
		input.PushKey(InputKey::ArrowLeft) ||
		input.PushKey(InputKey::ArrowRight);

	const int previousSelection = grid.GetSelectedIndex();
	const auto step = farm::ReadFieldStep(input);
	if (step.x || step.y) grid.MoveSelection(step.x, step.y);
	result.selectionChanged = previousSelection != grid.GetSelectedIndex();

	if (input.TriggerKey(InputKey::PageUp)) {
		result.contentChanged |= actionSystem.RaiseSelectedTile(grid);
	}
	if (input.TriggerKey(InputKey::PageDown)) {
		result.contentChanged |= actionSystem.LowerSelectedTile(grid);
	}
	if (input.TriggerKey(InputKey::N)) {
		result.contentChanged |= actionSystem.ToggleSelectedCanal(grid);
	}
	if (input.TriggerKey(InputKey::M)) {
		result.contentChanged |= actionSystem.ToggleSelectedWaterSource(grid);
	}

	const FarmTool previousTool = toolSystem.GetCurrentTool();
	(void)SelectToolFromKeys(input, context, toolSystem);
	result.toolChanged = previousTool != toolSystem.GetCurrentTool();

	if (input.TriggerKey(InputKey::Enter)) {
		result.toolAction = actionSystem.ApplyToolDetailed(
			grid, toolSystem.GetCurrentTool(), selectedCrop, economySystem, context.currentDay);
		result.contentChanged |= result.toolAction.Succeeded();
	}
	result.buySeedRequested = input.TriggerKey(InputKey::B);
	result.sellSelectedRequested = input.TriggerKey(InputKey::V);
	result.sellRequested = input.TriggerKey(InputKey::F);
	return result;
}
