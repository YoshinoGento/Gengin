#pragma once

#include "editor/CG5DemoWindow.h"
#include "editor/CameraControlWindow.h"
#include "editor/ConsoleWindow.h"
#include "editor/EditorSettings.h"
#include "editor/EditorSettingsWindow.h"
#include "editor/EditorSelectionSystem.h"
#include "editor/FarmControllerWindow.h"
#include "editor/FarmHierarchyWindow.h"
#include "editor/FarmHistoryWindow.h"
#include "editor/FarmMapWindow.h"
#include "editor/FarmToolbarWindow.h"
#include "editor/GameViewportWindow.h"
#include "editor/GamePlayEditorBridge.h"
#include "editor/ObjectInspectorWindow.h"
#include "editor/ParticleEffectWindow.h"
#include "editor/VisibilityWindow.h"

#if defined(_DEBUG) && defined(USE_IMGUI)
#include "editor/FarmPestPlaytestSystem.h"
#include "editor/FarmNextGoalTestWindow.h"
#include "editor/FarmTimeAdvanceTestWindow.h"
#include "editor/FarmTimeAdvanceLiveWindow.h"
#include "editor/FarmWeatherTestWindow.h"
#endif

#include <memory>

class BaseScene;
class EngineDebugWindowManager;
namespace editor { class GamePlayEditorScene; }
class Input;
class PostEffectDebugWindow;
class PostEffectSystem;
class SkinningDebugWindow;
class SrvManager;

// Owns editor layout, window visibility, and persistent editor appearance settings.
class EditorShell final {
public:
	EditorShell();
	~EditorShell();

	void Initialize();
	void Finalize();
	void Draw(
		BaseScene* currentScene,
		const Input* input,
		SrvManager* srvManager,
		PostEffectSystem* postEffectSystem);

	[[nodiscard]] Vector2 GetMousePositionInViewport() const;

private:
	void DrawMainMenuBar();
	void DrawCG5Demo(PostEffectSystem& postEffectSystem);
	void DrawEditorSettingsWindow();
	void DrawConsole();
	void DrawFarmController(editor::GamePlayEditorScene& playScene);
	void DrawFarmHierarchy(editor::GamePlayEditorScene& playScene);
	void DrawFarmHistory(editor::GamePlayEditorScene& playScene);
	void DrawFarmMap(editor::GamePlayEditorScene& playScene);
	void DrawFarmToolbar(editor::GamePlayEditorScene& playScene);
	void HandleFarmDocumentShortcut(editor::GamePlayEditorScene& playScene);
	void HandleFarmHistoryShortcuts(editor::GamePlayEditorScene& playScene);
	void BuildDefaultFarmLayout(unsigned int dockspaceId);
#if defined(_DEBUG) && defined(USE_IMGUI)
	void DrawFarmPestPlaytest();
	void DrawFarmNextGoalTest();
	void DrawFarmTimeAdvanceTest();
    void DrawFarmWeatherTest();
    bool showFarmWeatherTest_ = false;
    bool farmWeatherFailed_ = false;
    editor::FarmWeatherPlaytestSystem farmWeatherPlaytest_;
    void DrawFarmTimeAdvanceLive(editor::GamePlayEditorScene& scene, bool closedFromMenu);
    bool showFarmTimeAdvanceLive_ = false;
	bool showFarmTimeAdvanceTest_ = false;
	editor::FarmTimeAdvancePlaytestSystem farmTimeAdvancePlaytest_;
	editor::FarmTimeAdvanceTestWindow farmTimeAdvanceWindow_;
	bool farmTimeAdvanceFailed_ = false;
	editor::FarmPestPlaytestSystem farmPestPlaytest_;
	bool showFarmPestPlaytest_ = false;
	bool farmPestPlaytestFailed_ = false;
	bool showFarmNextGoalTest_ = false;
	editor::FarmNextGoalPlaytestSystem farmNextGoalPlaytest_;
	editor::FarmNextGoalTestWindow farmNextGoalWindow_;
	bool farmNextGoalPlaytestFailed_ = false;
#endif

	EditorSettings editorSettings_;
	CG5DemoWindow cg5DemoWindow_;
	EditorSettingsWindow editorSettingsWindow_;
	editor::EditorSelectionSystem selectionSystem_;
	editor::GamePlayEditorViewModel gamePlayEditorViewModel_;
	FarmControllerWindow farmControllerWindow_;
	FarmHierarchyWindow farmHierarchyWindow_;
	FarmHistoryWindow farmHistoryWindow_;
	FarmMapWindow farmMapWindow_;
	FarmToolbarWindow farmToolbarWindow_;
	GameViewportWindow gameViewportWindow_;
	VisibilityWindow visibilityWindow_;
	CameraControlWindow cameraControlWindow_;
	ConsoleWindow consoleWindow_;
	ObjectInspectorWindow objectInspectorWindow_;
	ParticleEffectWindow particleEffectWindow_;
	std::unique_ptr<EngineDebugWindowManager> engineDebugWindowManager_;
	std::unique_ptr<PostEffectDebugWindow> postEffectDebugWindow_;
	std::unique_ptr<SkinningDebugWindow> skinningDebugWindow_;
	bool showEngineDebugWindow_ = false;
	bool showPostEffectDebugWindow_ = false;
	bool showSkinningDebugWindow_ = false;
	bool settingsDirty_ = false;
	bool rebuildLayoutRequested_ = false;
};
