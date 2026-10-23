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
