#include "editor/EditorShell.h"

#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#include "base/WinApp.h"
#include "debug/EngineDebugWindowManager.h"
#include "debug/PostEffectDebugWindow.h"
#include "debug/SkinningDebugWindow.h"
#include "effect/PostEffectSystem.h"
#include "io/Input.h"
#include "scene/BaseScene.h"
#include "editor/GamePlayEditorScene.h"
#include "scene/SceneViewportInput.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif

namespace {
constexpr int kFarmWorkspaceLayoutVersion = 7;
}

EditorShell::EditorShell() = default;
EditorShell::~EditorShell() = default;

void EditorShell::Initialize() {
#ifdef USE_IMGUI
	engineDebugWindowManager_ = std::make_unique<EngineDebugWindowManager>();
	postEffectDebugWindow_ = std::make_unique<PostEffectDebugWindow>();
	skinningDebugWindow_ = std::make_unique<SkinningDebugWindow>();

	editorSettings_.Load();
	editorSettings_.Apply(*ImGuiManager::GetInstance());
	rebuildLayoutRequested_ =
		editorSettings_.GetWorkspaceLayoutVersion() < kFarmWorkspaceLayoutVersion;
	cg5DemoWindow_.SetOpen(false);
	farmMapWindow_.SetOpen(true);
	consoleWindow_.SetOpen(true);
	farmHierarchyWindow_.SetOpen(false);
	visibilityWindow_.SetOpen(false);
	cameraControlWindow_.SetOpen(false);
	objectInspectorWindow_.SetOpen(false);
	particleEffectWindow_.SetOpen(false);
	showPostEffectDebugWindow_ = false;
#endif
}

void EditorShell::Finalize() {
#ifdef USE_IMGUI
	if (settingsDirty_) {
		editorSettings_.Save();
		settingsDirty_ = false;
	}
#endif
	skinningDebugWindow_.reset();
	postEffectDebugWindow_.reset();
	engineDebugWindowManager_.reset();
}

void EditorShell::Draw(
	BaseScene* currentScene,
	const Input* input,
	SrvManager* srvManager,
	PostEffectSystem* postEffectSystem) {
#ifdef USE_IMGUI
	DrawMainMenuBar();
	editor::GamePlayEditorScene* playScene = dynamic_cast<editor::GamePlayEditorScene*>(currentScene);
	if (playScene) {
		playScene->GetEditorBridge().BuildViewModel(gamePlayEditorViewModel_);
		HandleFarmDocumentShortcut(*playScene);
		if (!playScene->IsFarmGameMode()) { HandleFarmHistoryShortcuts(*playScene); }
		DrawFarmToolbar(*playScene);
	}
	const bool gameMode = playScene && playScene->IsFarmGameMode();
	const ImGuiID dockspaceId = ImGui::GetID("FarmEditorDockSpace");
	if (rebuildLayoutRequested_ && !gameMode) {
		BuildDefaultFarmLayout(dockspaceId);
	}
	if (gameMode) {
		// Keep the development layout alive without an opaque fullscreen host.
		ImGui::DockSpace(dockspaceId, ImVec2(0, 0), ImGuiDockNodeFlags_KeepAliveOnly);
	} else {
		ImGui::DockSpaceOverViewport(dockspaceId, ImGui::GetMainViewport());
	}
	DrawEditorSettingsWindow();
	if (!gameMode) { DrawConsole(); }

	if (!gameMode && showEngineDebugWindow_ && engineDebugWindowManager_ && input) {
		engineDebugWindowManager_->Draw(*input);
	}

	if (playScene) {
		playScene->GetEditorBridge().SetViewportState({}, {}, {}, false, false);
	}

	if (srvManager && postEffectSystem) {
		gameViewportWindow_.Draw(
			*srvManager,
			postEffectSystem->GetFinalDisplaySrvIndex(),
			{
				static_cast<float>(WinApp::kClientWidth),
				static_cast<float>(WinApp::kClientHeight),
			},
			editorSettings_.GetLanguage(), gameMode);
	}

	if (auto* pointerTarget = dynamic_cast<scene::ViewportInputTarget*>(currentScene)) {
		const auto& viewport = gameViewportWindow_.GetFrameState();
		scene::ViewportPointerInput pointer;
		pointer.position = viewport.virtualMousePosition;
		pointer.valid = viewport.imageVisible;
		pointer.pressed = viewport.leftClicked;
		pointer.held = ImGui::IsMouseDown(ImGuiMouseButton_Left);
		pointer.focused = viewport.focused;
		pointerTarget->SetViewportPointerInput(pointer);
	}

	if (!playScene) {
		selectionSystem_.Clear();
		return;
	}

	const GameViewportFrameState& viewportState = gameViewportWindow_.GetFrameState();
	editor::GamePlayEditorBridge& bridge = playScene->GetEditorBridge();
	bridge.SetViewportState(
		viewportState.imageTopLeft,
		viewportState.imageSize,
		viewportState.mousePosition,
		viewportState.hovered,
		viewportState.focused);
	bridge.BuildViewModel(gamePlayEditorViewModel_);
	if (viewportState.leftClicked) {
		editor::GamePlayEditorCommand selectCommand;
		selectCommand.type = editor::GamePlayEditorCommandType::SelectFarmTileAtViewport;
		selectCommand.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		if (bridge.Execute(selectCommand)) {
			bridge.BuildViewModel(gamePlayEditorViewModel_);
			if (viewportState.quickApplyRequested) {
				editor::GamePlayEditorCommand applyCommand;
				applyCommand.type = editor::GamePlayEditorCommandType::ApplyCurrentFarmTool;
				applyCommand.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
				applyCommand.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
				bridge.Execute(applyCommand);
				bridge.BuildViewModel(gamePlayEditorViewModel_);
			}
		}
	}
	selectionSystem_.SynchronizeFarmSelection(
		gamePlayEditorViewModel_.selectedFarmTileIndex,
		gamePlayEditorViewModel_.farmGeneration,
		static_cast<int>(gamePlayEditorViewModel_.farmTiles.size()));

	if (gameMode) { return; }
	DrawFarmHierarchy(*playScene);
	DrawFarmController(*playScene);
	DrawFarmHistory(*playScene);
	DrawFarmMap(*playScene);
	bridge.Execute(visibilityWindow_.Draw(
		gamePlayEditorViewModel_.visibility,
		editorSettings_.GetLanguage()));
	bridge.Execute(cameraControlWindow_.Draw(gamePlayEditorViewModel_.camera));
	bridge.Execute(objectInspectorWindow_.Draw(gamePlayEditorViewModel_.objectInspector));
	bridge.Execute(particleEffectWindow_.Draw(gamePlayEditorViewModel_.particles));
	if (showSkinningDebugWindow_ && skinningDebugWindow_) {
		skinningDebugWindow_->Draw(bridge.GetAnimationObjectForDebug());
	}
	if (showPostEffectDebugWindow_ && postEffectDebugWindow_ && postEffectSystem) {
		const PostEffectDebugActions actions = postEffectDebugWindow_->Draw(
			*postEffectSystem,
			editorSettings_.GetLanguage());
		if (actions.settings.has_value()) {
			postEffectSystem->ApplySettings(*actions.settings);
		}
		if (actions.chainModeEnabled.has_value()) {
			postEffectSystem->SetChainModeEnabled(*actions.chainModeEnabled);
		}
		if (actions.chainPassChange.has_value()) {
			postEffectSystem->SetChainPassEnabled(
				actions.chainPassChange->index,
				actions.chainPassChange->enabled);
		}
		if (actions.resetSettings) {
			postEffectSystem->ResetSettings();
		}
		if (actions.preset.has_value()) {
			postEffectSystem->ApplyDemoPreset(*actions.preset);
		}
	}
	if (postEffectSystem) {
		DrawCG5Demo(*postEffectSystem);
	}
#else
	(void)currentScene;
	(void)input;
	(void)srvManager;
	(void)postEffectSystem;
#endif
}

Vector2 EditorShell::GetMousePositionInViewport() const {
	return gameViewportWindow_.GetFrameState().virtualMousePosition;
}

void EditorShell::DrawMainMenuBar() {
#ifdef USE_IMGUI
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}
	const EditorLanguage language = editorSettings_.GetLanguage();
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};

	ImGui::TextUnformatted(text("Farm Editor"));
	ImGui::Separator();
	if (ImGui::BeginMenu(text("Display"))) {
		if (ImGui::BeginMenu(text("Language"))) {
			const bool japaneseSelected = language == EditorLanguage::Japanese;
			if (ImGui::MenuItem(
				editor::Localize(EditorLanguage::Japanese, "Japanese"),
				nullptr,
				japaneseSelected)) {
				settingsDirty_ |= editorSettings_.SetLanguage(EditorLanguage::Japanese);
			}
			const bool englishSelected = language == EditorLanguage::English;
			if (ImGui::MenuItem(
				editor::Localize(EditorLanguage::English, "English"),
				nullptr,
				englishSelected)) {
				settingsDirty_ |= editorSettings_.SetLanguage(EditorLanguage::English);
			}
			ImGui::EndMenu();
		}
		ImGui::Separator();
		ImGuiManager* imguiManager = ImGuiManager::GetInstance();
		const std::size_t selectedFontIndex = imguiManager->GetSelectedFontIndex();
		const char* selectedFontName = imguiManager->GetFontOptionName(selectedFontIndex);
		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo(text("Font"), selectedFontName)) {
			for (std::size_t index = 0; index < imguiManager->GetFontOptionCount(); ++index) {
				const bool selected = index == selectedFontIndex;
				if (ImGui::Selectable(imguiManager->GetFontOptionName(index), selected) &&
					imguiManager->SelectFont(index)) {
					settingsDirty_ |= editorSettings_.SetFontName(imguiManager->GetFontOptionName(index));
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(text("Window"))) {
		if (ImGui::MenuItem(text("CG5 Demo"), nullptr, cg5DemoWindow_.IsOpen())) {
			cg5DemoWindow_.SetOpen(!cg5DemoWindow_.IsOpen());
		}
		ImGui::Separator();
		if (ImGui::MenuItem(text("Farm Map"), nullptr, farmMapWindow_.IsOpen())) {
			farmMapWindow_.SetOpen(!farmMapWindow_.IsOpen());
		}
		if (ImGui::MenuItem(text("Farm Hierarchy"), nullptr, farmHierarchyWindow_.IsOpen())) {
			farmHierarchyWindow_.SetOpen(!farmHierarchyWindow_.IsOpen());
		}
		if (ImGui::MenuItem(text("Farm Inspector"), nullptr, farmControllerWindow_.IsOpen())) {
			farmControllerWindow_.SetOpen(!farmControllerWindow_.IsOpen());
		}
		if (ImGui::MenuItem(text("Farm History"), nullptr, farmHistoryWindow_.IsOpen())) {
			farmHistoryWindow_.SetOpen(!farmHistoryWindow_.IsOpen());
		}
		if (ImGui::MenuItem(text("Console"), nullptr, consoleWindow_.IsOpen())) {
			consoleWindow_.SetOpen(!consoleWindow_.IsOpen());
		}
		ImGui::Separator();
		if (ImGui::MenuItem(text("Scene Visibility"), nullptr, visibilityWindow_.IsOpen())) {
			visibilityWindow_.SetOpen(!visibilityWindow_.IsOpen());
		}
		if (ImGui::MenuItem("Camera Control", nullptr, cameraControlWindow_.IsOpen())) {
			cameraControlWindow_.SetOpen(!cameraControlWindow_.IsOpen());
		}
		if (ImGui::MenuItem("Object Inspector", nullptr, objectInspectorWindow_.IsOpen())) {
			objectInspectorWindow_.SetOpen(!objectInspectorWindow_.IsOpen());
		}
		if (ImGui::MenuItem("Particle Effect", nullptr, particleEffectWindow_.IsOpen())) {
			particleEffectWindow_.SetOpen(!particleEffectWindow_.IsOpen());
		}
		ImGui::Separator();
		if (ImGui::MenuItem(text("Editor Settings"), nullptr, editorSettingsWindow_.IsOpen())) {
			editorSettingsWindow_.SetOpen(!editorSettingsWindow_.IsOpen());
		}
		ImGui::Separator();
		ImGui::MenuItem("Engine Debug", nullptr, &showEngineDebugWindow_);
		ImGui::MenuItem(
			text("Fullscreen PostEffect"),
			nullptr,
			&showPostEffectDebugWindow_);
		ImGui::MenuItem("Skinning Debug", nullptr, &showSkinningDebugWindow_);
		ImGui::EndMenu();
	}
	ImGui::EndMainMenuBar();
#endif
}

void EditorShell::DrawCG5Demo(PostEffectSystem& postEffectSystem) {
#ifdef USE_IMGUI
	const PostEffectSystem::Settings& settings = postEffectSystem.GetSettings();
	CG5DemoViewData viewData;
	viewData.chainModeEnabled = postEffectSystem.IsChainModeEnabled();
	switch (settings.effect) {
	case DirectXCommon::FullscreenPostEffectType::Copy:
		viewData.effect = CG5DemoEffectState::Original;
		break;
	case DirectXCommon::FullscreenPostEffectType::Grayscale:
		viewData.effect = CG5DemoEffectState::Grayscale;
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineDepth:
		viewData.effect = CG5DemoEffectState::DepthBasedOutline;
		break;
	default:
		viewData.effect = CG5DemoEffectState::Other;
		break;
	}
	viewData.grayscaleIntensity = settings.grayscaleIntensity;
	viewData.depthOutline = {
		.threshold = settings.depthOutlineThreshold,
		.intensity = settings.depthOutlineIntensity,
		.thickness = settings.depthOutlineThickness,
		.linearize = settings.depthOutlineLinearize,
	};

	const CG5DemoActions actions = cg5DemoWindow_.Draw(
		viewData,
		editorSettings_.GetLanguage());
	if (actions.preset.has_value()) {
		PostEffectSystem::DemoPreset preset = PostEffectSystem::DemoPreset::Original;
		switch (*actions.preset) {
		case CG5DemoPresetAction::RequiredGrayscale:
			preset = PostEffectSystem::DemoPreset::RequiredGrayscale;
			break;
		case CG5DemoPresetAction::DepthBasedOutline:
			preset = PostEffectSystem::DemoPreset::DepthBasedOutline;
			break;
		case CG5DemoPresetAction::Original:
		default:
			break;
		}
		postEffectSystem.ApplyDemoPreset(preset);
	}
	if (actions.grayscaleIntensity.has_value()) {
		postEffectSystem.SetGrayscaleIntensity(*actions.grayscaleIntensity);
	}
	if (actions.depthOutline.has_value()) {
		const CG5DepthOutlineParameters& outline = *actions.depthOutline;
		postEffectSystem.SetDepthOutlineParameters({
			.threshold = outline.threshold,
			.intensity = outline.intensity,
			.thickness = outline.thickness,
			.linearize = outline.linearize,
		});
	}
#else
	(void)postEffectSystem;
#endif
}

void EditorShell::DrawEditorSettingsWindow() {
#ifdef USE_IMGUI
	ImGuiManager* imguiManager = ImGuiManager::GetInstance();
	const EditorSettingsActions actions = editorSettingsWindow_.Draw(editorSettings_, *imguiManager);
	bool appearanceChanged = false;
	if (actions.fontIndex.has_value() && imguiManager->SelectFont(*actions.fontIndex)) {
		appearanceChanged |= editorSettings_.SetFontName(imguiManager->GetFontOptionName(*actions.fontIndex));
	}
	if (actions.uiScale.has_value()) {
		appearanceChanged |= editorSettings_.SetUiScale(*actions.uiScale);
	}
	if (actions.theme.has_value()) {
		appearanceChanged |= editorSettings_.SetTheme(*actions.theme);
	}
	if (actions.language.has_value()) {
		settingsDirty_ |= editorSettings_.SetLanguage(*actions.language);
	}
	if (appearanceChanged) {
		editorSettings_.Apply(*imguiManager);
		settingsDirty_ = true;
	}
	if (actions.resetLayout) {
		if (editorSettings_.ResetLayout()) {
			rebuildLayoutRequested_ = true;
		}
	}
#endif
}

void EditorShell::DrawConsole() {
#ifdef USE_IMGUI
	const ConsoleWindowActions actions = consoleWindow_.Draw(editorSettings_.GetLanguage());
	if (actions.clearRequested) {
		Logger::Clear();
	}
#endif
}

void EditorShell::DrawFarmToolbar(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	const FarmToolbarActions actions = farmToolbarWindow_.Draw(
		gamePlayEditorViewModel_,
		editorSettings_.GetLanguage(), playScene.IsFarmGameMode());
    if (actions.layoutLibrary) playScene.RequestFarmLayoutLibrary();
	bool changed = false;
	if (actions.gameMode.has_value()) {
		changed |= playScene.SetFarmGameMode(*actions.gameMode);
	}
	if (actions.newDocument || actions.loadDocumentId.has_value() ||
		actions.saveDocument || actions.saveAsName.has_value() ||
		actions.renameDocument.has_value() || actions.deleteDocumentId.has_value()) {
		editor::FarmDocumentCommand documentCommand;
		if (actions.newDocument) {
			documentCommand.type = editor::FarmDocumentCommandType::NewDocument;
		} else if (actions.loadDocumentId.has_value()) {
			documentCommand.type = editor::FarmDocumentCommandType::Load;
			documentCommand.documentId = *actions.loadDocumentId;
		} else if (actions.saveAsName.has_value()) {
			documentCommand.type = editor::FarmDocumentCommandType::SaveAs;
			documentCommand.displayName = *actions.saveAsName;
		} else if (actions.renameDocument.has_value()) {
			documentCommand.type = editor::FarmDocumentCommandType::Rename;
			documentCommand.documentId = actions.renameDocument->documentId;
			documentCommand.displayName = actions.renameDocument->displayName;
		} else if (actions.deleteDocumentId.has_value()) {
			documentCommand.type = editor::FarmDocumentCommandType::Delete;
			documentCommand.documentId = *actions.deleteDocumentId;
		} else {
			documentCommand.type = editor::FarmDocumentCommandType::Save;
		}
		changed |= bridge.Execute(documentCommand);
	}

	if (actions.selectedTool.has_value()) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::SelectFarmTool;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTool = *actions.selectedTool;
		changed |= bridge.Execute(command);
	}
	if (actions.simulationAction.has_value()) {
		editor::SimulationEditorCommand command;
		command.action = *actions.simulationAction;
		changed |= bridge.Execute(command);
	}
	if (actions.undo) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::UndoFarmEdit;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		changed |= bridge.Execute(command);
	}
	if (actions.redo) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::RedoFarmEdit;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		changed |= bridge.Execute(command);
	}
	if (actions.resetLayout && editorSettings_.ResetLayout()) {
		rebuildLayoutRequested_ = true;
	}
	if (changed) {
		bridge.BuildViewModel(gamePlayEditorViewModel_);
	}
#else
	(void)playScene;
#endif
}

void EditorShell::HandleFarmDocumentShortcut(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	if (ImGui::GetIO().WantTextInput ||
		!ImGui::Shortcut(
			ImGuiMod_Ctrl | ImGuiKey_S,
			ImGuiInputFlags_RouteGlobal)) {
		return;
	}
	if (!gamePlayEditorViewModel_.farmDocumentExists) {
		farmToolbarWindow_.RequestSaveAsDialog(gamePlayEditorViewModel_.farmDocumentName);
		return;
	}
	editor::FarmDocumentCommand command;
	command.type = editor::FarmDocumentCommandType::Save;
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	if (bridge.Execute(command)) {
		bridge.BuildViewModel(gamePlayEditorViewModel_);
	}
#else
	(void)playScene;
#endif
}

void EditorShell::HandleFarmHistoryShortcuts(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	constexpr ImGuiInputFlags kShortcutFlags = ImGuiInputFlags_RouteGlobal;
	const bool redoRequested =
		ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, kShortcutFlags) ||
		ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, kShortcutFlags);
	const bool undoRequested = !redoRequested &&
		ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, kShortcutFlags);
	if (!undoRequested && !redoRequested) {
		return;
	}

	editor::GamePlayEditorCommand command;
	command.type = undoRequested
		? editor::GamePlayEditorCommandType::UndoFarmEdit
		: editor::GamePlayEditorCommandType::RedoFarmEdit;
	command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	if (bridge.Execute(command)) {
		bridge.BuildViewModel(gamePlayEditorViewModel_);
	}
#else
	(void)playScene;
#endif
}

void EditorShell::DrawFarmHierarchy(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	const FarmHierarchyActions actions = farmHierarchyWindow_.Draw(
		gamePlayEditorViewModel_,
		selectionSystem_.GetSelection(),
		editorSettings_.GetLanguage());
	if (!actions.selectedTileIndex.has_value()) {
		return;
	}

	editor::GamePlayEditorCommand command;
	command.type = editor::GamePlayEditorCommandType::SelectFarmTile;
	command.farmTileIndex = *actions.selectedTileIndex;
	command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
	if (!bridge.Execute(command)) {
		return;
	}
	selectionSystem_.SelectFarmTile(
		command.farmTileIndex,
		command.farmGeneration,
		static_cast<int>(gamePlayEditorViewModel_.farmTiles.size()));
	bridge.BuildViewModel(gamePlayEditorViewModel_);
#else
	(void)playScene;
#endif
}

void EditorShell::DrawFarmHistory(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	const FarmHistoryActions actions = farmHistoryWindow_.Draw(
		gamePlayEditorViewModel_,
		editorSettings_.GetLanguage());
	bool changed = false;
	if (actions.undo) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::UndoFarmEdit;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		changed |= bridge.Execute(command);
	}
	if (actions.redo) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::RedoFarmEdit;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		changed |= bridge.Execute(command);
	}
	if (changed) {
		bridge.BuildViewModel(gamePlayEditorViewModel_);
	}
#else
	(void)playScene;
#endif
}

void EditorShell::DrawFarmController(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	const FarmControllerActions actions = farmControllerWindow_.Draw(
		gamePlayEditorViewModel_,
		editorSettings_.GetLanguage());
	bool changed = false;
	if (actions.selectedTool.has_value()) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::SelectFarmTool;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTool = *actions.selectedTool;
		changed |= bridge.Execute(command);
	}
	if (actions.applyCurrentTool) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::ApplyCurrentFarmTool;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.compost) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::CompostFarmTile;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.irrigationCommand) {
		changed |= bridge.Execute(*actions.irrigationCommand);
	}
	if (actions.beginRaiseTerrainPreview) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::BeginFarmRaiseTerrainPreview;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.beginLowerTerrainPreview) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::BeginFarmLowerTerrainPreview;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.toggleCanal) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::ToggleFarmCanal;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.toggleWaterSource) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::ToggleFarmWaterSource;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.beginCanalPreview) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::BeginFarmCanalPreview;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.beginWaterSourcePreview) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::BeginFarmWaterSourcePreview;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.confirmIrrigationPreview) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::ConfirmFarmIrrigationPreview;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.irrigationPreviewTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.cancelIrrigationPreview) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::CancelFarmIrrigationPreview;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		changed |= bridge.Execute(command);
	}
	if (actions.restartFarm) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::RestartFarmSession;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		changed |= bridge.Execute(command);
	}
	if (actions.movePlayerToSelectedTile) {
		editor::GamePlayEditorCommand command;
		command.type = editor::GamePlayEditorCommandType::MovePlayerToSelectedFarmTile;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.comparisonCommand.has_value()) {
		editor::GamePlayEditorCommand command;
		command.type = *actions.comparisonCommand;
		command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		command.farmTileIndex = gamePlayEditorViewModel_.selectedFarmTileIndex;
		changed |= bridge.Execute(command);
	}
	if (actions.harvestProtectionCommand) changed |= bridge.Execute(*actions.harvestProtectionCommand);
	if (actions.contestReservationCommand) changed |= bridge.Execute(*actions.contestReservationCommand);
	if (actions.contestSubmissionCommand) changed |= bridge.Execute(*actions.contestSubmissionCommand);
	if (actions.contestDayCommand) changed |= bridge.Execute(*actions.contestDayCommand);
	if (actions.progressionModeCommand) changed |= bridge.Execute(*actions.progressionModeCommand);
	if (changed) {
		bridge.BuildViewModel(gamePlayEditorViewModel_);
	}
#else
	(void)playScene;
#endif
}

void EditorShell::DrawFarmMap(editor::GamePlayEditorScene& playScene) {
#ifdef USE_IMGUI
	editor::GamePlayEditorBridge& bridge = playScene.GetEditorBridge();
	const FarmMapActions actions = farmMapWindow_.Draw(
		gamePlayEditorViewModel_,
		selectionSystem_.GetSelection(),
		editorSettings_.GetLanguage());
	if (actions.endTerrainStroke) {
		editor::GamePlayEditorCommand endStroke;
		endStroke.type = editor::GamePlayEditorCommandType::EndFarmTerrainStroke;
		endStroke.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
		static_cast<void>(bridge.Execute(endStroke));
	}
	editor::GamePlayEditorCommandType commandType =
		editor::GamePlayEditorCommandType::SelectFarmTile;
	std::optional<int> commandTileIndex;
	bool synchronizeSelection = false;
	if (actions.beginCanalPathTileIndex.has_value()) {
		commandType = actions.removeCanalPath
			? editor::GamePlayEditorCommandType::BeginFarmCanalRemovalPathPreview
			: editor::GamePlayEditorCommandType::BeginFarmCanalPathPreview;
		commandTileIndex = actions.beginCanalPathTileIndex;
		synchronizeSelection = true;
	} else if (actions.appendTerrainTileIndex.has_value()) {
		commandType = editor::GamePlayEditorCommandType::AppendFarmTerrainPreview;
		commandTileIndex = actions.appendTerrainTileIndex;
	} else if (actions.appendCanalPathTileIndex.has_value()) {
		commandType = editor::GamePlayEditorCommandType::AppendFarmCanalPathPreview;
		commandTileIndex = actions.appendCanalPathTileIndex;
	} else if (actions.selectedTileIndex.has_value()) {
		commandTileIndex = actions.selectedTileIndex;
		synchronizeSelection = true;
	}
	if (!commandTileIndex.has_value()) {
		return;
	}

	editor::GamePlayEditorCommand command;
	command.type = commandType;
	command.farmTileIndex = *commandTileIndex;
	command.farmGeneration = gamePlayEditorViewModel_.farmGeneration;
	if (!bridge.Execute(command)) {
		return;
	}
	if (synchronizeSelection) {
		selectionSystem_.SelectFarmTile(
			command.farmTileIndex,
			command.farmGeneration,
			static_cast<int>(gamePlayEditorViewModel_.farmTiles.size()));
	}
	bridge.BuildViewModel(gamePlayEditorViewModel_);
#else
	(void)playScene;
#endif
}

void EditorShell::BuildDefaultFarmLayout(unsigned int dockspaceId) {
#ifdef USE_IMGUI
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (viewport == nullptr || dockspaceId == 0 ||
		viewport->WorkSize.x <= 1.0f || viewport->WorkSize.y <= 1.0f) {
		return;
	}

	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodePos(dockspaceId, viewport->WorkPos);
	ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

	const ImGuiID bodyNode = dockspaceId;
	ImGuiID historyNode = 0;
	ImGuiID mainNode = 0;
	ImGui::DockBuilderSplitNode(
		bodyNode,
		ImGuiDir_Down,
		0.15f,
		&historyNode,
		&mainNode);
	ImGuiID hierarchyNode = 0;
	ImGuiID centerAndInspectorNode = 0;
	ImGui::DockBuilderSplitNode(
		mainNode,
		ImGuiDir_Left,
		0.18f,
		&hierarchyNode,
		&centerAndInspectorNode);
	ImGuiID inspectorNode = 0;
	ImGuiID viewportNode = 0;
	ImGui::DockBuilderSplitNode(
		centerAndInspectorNode,
		ImGuiDir_Right,
		0.22f,
		&inspectorNode,
		&viewportNode);

	ImGui::DockBuilderDockWindow("FarmMap", hierarchyNode);
	ImGui::DockBuilderDockWindow("FarmHierarchy", hierarchyNode);
	ImGui::DockBuilderDockWindow("GameViewport", viewportNode);
	ImGui::DockBuilderDockWindow("FarmInspector", inspectorNode);
	ImGui::DockBuilderDockWindow("CG5Demo", inspectorNode);
	ImGui::DockBuilderDockWindow("FarmHistory", historyNode);
	ImGui::DockBuilderDockWindow("Console", historyNode);
	ImGui::DockBuilderDockWindow("Particle Effect", historyNode);
	ImGui::DockBuilderDockWindow("Camera Control", inspectorNode);
	ImGui::DockBuilderDockWindow("Object Inspector", inspectorNode);
	ImGui::DockBuilderDockWindow("Visibility & Cull", inspectorNode);
	ImGui::DockBuilderDockWindow("EditorSettings", inspectorNode);
	ImGui::DockBuilderDockWindow("Engine Debug", historyNode);
	ImGui::DockBuilderDockWindow("Fullscreen PostEffect", historyNode);
	ImGui::DockBuilderDockWindow("Skinning Debug", historyNode);
	ImGui::DockBuilderFinish(dockspaceId);

	rebuildLayoutRequested_ = false;
	settingsDirty_ |= editorSettings_.SetWorkspaceLayoutVersion(kFarmWorkspaceLayoutVersion);
#else
	(void)dockspaceId;
#endif
}
