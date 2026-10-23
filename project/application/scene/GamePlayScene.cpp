#include "scene/GamePlayScene.h"
#include "gameplay/GamePlaySessionSystem.h"
GamePlayScene::GamePlayScene() : session_(std::make_unique<GamePlaySessionSystem>(*this, nullptr)) {}
GamePlayScene::~GamePlayScene() = default;
void GamePlayScene::Initialize() { (void)InitializeIncrementally().Complete(); }
InitializationTask GamePlayScene::InitializeIncrementally() {
    session_->BindSceneManager(GetSceneManager());
    co_return co_await session_->InitializeIncrementally();
}
void GamePlayScene::Finalize() { session_->Finalize(); }
void GamePlayScene::PrepareFixedUpdate() { session_->PrepareFixedUpdate(); }
void GamePlayScene::OnScreenTransitionCovered() { session_->OnScreenTransitionCovered(); }
void GamePlayScene::Update() { session_->Update(); }
void GamePlayScene::Draw() { session_->Draw(); }
void GamePlayScene::FixedUpdate(float dt) { session_->FixedUpdate(dt); }
editor::GamePlayEditorBridge& GamePlayScene::GetEditorBridge() noexcept { return session_->GetEditorBridge(); }
const editor::GamePlayEditorBridge& GamePlayScene::GetEditorBridge() const noexcept { return session_->GetEditorBridge(); }
const Camera* GamePlayScene::GetCamera() const noexcept { return session_->GetCamera(); }
SceneClipPlanes GamePlayScene::GetPostEffectClipPlanes() const noexcept {
    const Camera* camera = session_->GetCamera();
    return camera ? SceneClipPlanes{camera->GetNearClip(), camera->GetFarClip()} : SceneClipPlanes{};
}
bool GamePlayScene::IsFarmGameMode() const noexcept { return session_->IsFarmGameMode(); }
bool GamePlayScene::SetFarmGameMode(bool enabled) { return session_->SetFarmGameMode(enabled); }
void GamePlayScene::RequestFarmLayoutLibrary() noexcept { session_->RequestFarmLayoutLibrary(); }
