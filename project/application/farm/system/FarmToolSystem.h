#pragma once

enum class FarmTool {
	Hoe,
	Water,
	Seed,
	Harvest,
	BugNet,
};

class FarmToolSystem {
public:
	void Initialize();

	void SetTool(FarmTool tool);
	void CycleTools(int steps);
	void SelectNextTool();
	void SelectPreviousTool();

	FarmTool GetCurrentTool() const { return currentTool_; }
	const char* GetCurrentToolName() const;

private:
	FarmTool currentTool_ = FarmTool::Hoe;
};
