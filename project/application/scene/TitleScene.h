#pragma once

#include "BaseScene.h"
#include "scene/SceneViewportInput.h"
#include "title/TitlePresentationSystem.h"
#include "title/TitleFarmRenderer.h"
#include "title/TitleView.h"
#include "title/TitleAudioSystem.h"
#include "title/TitleAudioSettingsSystem.h"
#include "title/TitleAudioSettingsView.h"
#include "title/TitlePondRenderer.h"

class TitleScene final : public BaseScene, public scene::ViewportInputTarget {
public:
	void Initialize() override;
    InitializationTask InitializeIncrementally() override;
	void Finalize() override;
	void Update() override;
	void UpdateTransitionPresentation() override { Update(); }
	void Draw() override;
    void RequestStart() noexcept;
	void SetViewportPointerInput(const scene::ViewportPointerInput& input) noexcept override {
		viewportInput_ = {};
		viewportInput_.pointer = input.position;
		viewportInput_.pointerValid = input.valid;
		viewportInput_.pressed = input.pressed;
		viewportInput_.held = input.held;
		viewportFocused_ = input.focused;
	}

private:
	title::TitlePresentationSystem presentation_;
	title::TitleFarmRenderer renderer_;
	title::TitleView view_;
	title::TitleAudioSystem audio_;
	title::TitleAudioSettingsSystem settings_;
	title::TitleAudioSettingsView settingsView_;
    title::TitlePondSystem pond_;
    title::TitlePondRenderer pondRenderer_;
	title::AudioSettingsInput viewportInput_{};
	bool viewportFocused_ = false;
	bool ready_ = false;
    bool pondReady_ = false;
};
