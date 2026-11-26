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
	void ToggleSeedKit() noexcept;
	[[nodiscard]] bool WheelSelectsSeeds() const noexcept { return seedKit_; }
	void CycleTools(int steps);
	void SelectNextTool();
	void SelectPreviousTool();

	FarmTool GetCurrentTool() const { return currentTool_; }
	const char* GetCurrentToolName() const;

private:
	FarmTool currentTool_ = FarmTool::Hoe;
	FarmTool lastWorkTool_ = FarmTool::Hoe;
	bool seedKit_ = false;
};
