#include "farm/system/FarmToolSystem.h"

namespace {
	// BugNet remains reserved until its gameplay action is implemented.
	constexpr int kSelectableToolCount = 4;

	static_assert(static_cast<int>(FarmTool::Hoe) == 0 &&
		static_cast<int>(FarmTool::Harvest) + 1 == kSelectableToolCount);
}

void FarmToolSystem::Initialize()
{
	currentTool_ = FarmTool::Hoe;
}

void FarmToolSystem::SetTool(FarmTool tool)
{
	currentTool_ = tool;
}

void FarmToolSystem::CycleTools(int steps)
{
	if (!steps) return;
	int index = static_cast<int>(currentTool_);
	if (index < 0 || index >= kSelectableToolCount)
		index = steps > 0 ? kSelectableToolCount - 1 : 0;
	const int offset = steps % kSelectableToolCount;
	currentTool_ = static_cast<FarmTool>((index + offset + kSelectableToolCount) % kSelectableToolCount);
}

void FarmToolSystem::SelectNextTool()
{
	CycleTools(1);
}

void FarmToolSystem::SelectPreviousTool()
{
	CycleTools(-1);
}

const char* FarmToolSystem::GetCurrentToolName() const
{
	switch (currentTool_) {
	case FarmTool::Hoe:
		return "Hoe";
	case FarmTool::Water:
		return "Water";
	case FarmTool::Seed:
		return "Seed";
	case FarmTool::Harvest:
		return "Harvest";
	case FarmTool::BugNet:
		return "BugNet";
	default:
		return "Unknown";
	}
}
