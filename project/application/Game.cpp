#include "Game.h"

#include "base/DirectXCommon.h"
#include "base/FrameClock.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#ifdef USE_IMGUI
#include "editor/EditorShell.h"
#endif
#include "effect/PostEffectSystem.h"
#include "2d/Sprite.h"
#include "scene/SceneFactory.h"
#include "scene/SceneManager.h"

#include <cassert>

Game::Game() = default;
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();

	postEffectSystem_ = std::make_unique<PostEffectSystem>();
	if (!postEffectSystem_->Initialize(GetDxCommon(), GetSrvManager())) {
		Logger::Log("Game::Initialize failed to initialize PostEffectSystem.");
		assert(false && "PostEffectSystem initialization failed");
	}
#ifdef USE_IMGUI
	editorShell_ = std::make_unique<EditorShell>();
	editorShell_->Initialize();
#endif

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->InitializeTransitions(GetDxCommon(),GetSrvManager(),GetAudio());
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Finalize() {
#ifdef USE_IMGUI
	if (editorShell_) {
		editorShell_->Finalize();
		editorShell_.reset();
	}
#endif

	SceneManager::DeleteInstance();
	if (postEffectSystem_) {
		postEffectSystem_->Finalize();
		postEffectSystem_.reset();
	}
	Framework::Finalize();
}

void Game::Update() {
	Framework::Update();
	SceneManager* sceneManager = SceneManager::GetInstance();
	sceneManager->BeginFrame();


	postEffectSystem_->Update(GetFrameClock()->GetFrameDeltaSeconds());

	ImGuiManager::GetInstance()->Begin();
	sceneManager->PrepareFixedUpdate();
	while (GetFrameClock()->ConsumeFixedStep()) {
		sceneManager->FixedUpdate(GetFrameClock()->GetFixedDeltaSeconds());
	}

#ifdef USE_IMGUI
	if (editorShell_) {
		editorShell_->Draw(
			sceneManager->GetCurrentScene(),
			GetInput(),
			GetSrvManager(),
			postEffectSystem_.get());
	}
#endif

	sceneManager->Update();
	ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
	GetDxCommon()->PreDraw();
	GetSrvManager()->PreDraw();

	SceneManager* sceneManager = SceneManager::GetInstance();
	// The scene is rendered into the offscreen RenderTexture first.
	sceneManager->Draw();

	const BaseScene* currentScene = sceneManager->GetCurrentScene();
	const SceneClipPlanes clips = currentScene ? currentScene->GetPostEffectClipPlanes() : SceneClipPlanes{};
	postEffectSystem_->Execute(clips.nearClip, clips.farClip);


	// ImGui is composited on the swapchain after the gamma-corrected game image.
	ImGuiManager::GetInstance()->Draw();
	GetDxCommon()->RestoreRenderTextureToRenderTarget();
	GetDxCommon()->PostDraw();
}
