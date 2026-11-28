#include "Framework.h"
#include "base/WinApp.h"
#include "base/DirectXCommon.h"
#include "io/Input.h"
#include "audio/Audio.h"
#include "base/SrvManager.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "base/FrameClock.h"
#include "base/D3D12DeviceSelection.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Object3dCommon.h"
#include "3d/LightingSystem.h"
#include "3d/ModelManager.h"
#include "effect/ParticleManager.h"
#include <cassert>
#include <stdexcept>

// 静的変数の実体を定義（最初は空っぽ）
Framework* Framework::instance = nullptr;

/**
 * どこからでも自分自身を取得できる関数
 */
void Framework::SetFieldMouseLook(bool enabled) {
    if (winApp_) winApp_->SetFieldMouseLook(enabled);
}
Framework* Framework::GetInstance() {
	return instance;
}

/**
 * コンストラクタ（生成時）
 */
Framework::Framework() {
	// 自分が作られたとき、その場所（this）を instance に保存する
	instance = this;
}

Framework::~Framework() {
	instance = nullptr;
}

/**
 * メインループの制御
 */
void Framework::Run() {
	Initialize(); // 準備

	while (true) {
		Update(); // 更新
		if (IsEndRequest()) break;
		Draw();   // 描画
	}

	Finalize(); // 終了
}

/**
 * エンジンとしての基本初期化
 */
void Framework::Initialize() {
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	dxCommon_ = std::make_unique<DirectXCommon>();
	try {
		dxCommon_->Initialize(winApp_.get());
	} catch (const graphics::InitializationError&) {
		// No GPU commands have been submitted; later subsystems are not initialized yet.
		dxCommon_.reset();
		winApp_->Finalize();
		throw;
	}

	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Initialize(dxCommon_.get());
	if (!TextureManager::GetInstance()->Initialize(dxCommon_.get(), srvManager_.get())) {
		Logger::Log("Framework::Initialize failed to initialize TextureManager.");
		assert(false && "TextureManager initialization failed");
	}

	input_ = std::make_unique<Input>();
	input_->Initialize(winApp_.get());

	audio_ = std::make_unique<Audio>();
	if (!audio_->Initialize()) {
		Logger::Log("Framework::Initialize failed to initialize Audio.");
		assert(false && "Audio initialization failed");
		throw std::runtime_error("Audio initialization failed");
	}

	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get());

	lightingSystem_ = std::make_unique<LightingSystem>();
	if (!lightingSystem_->Initialize(dxCommon_.get())) {
		Logger::Log("Framework::Initialize failed to initialize LightingSystem.");
		assert(false && "LightingSystem initialization failed");
	}

	object3dCommon_ = std::make_unique<Object3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get(), srvManager_.get(), lightingSystem_.get());

	modelManager_ = std::make_unique<ModelManager>();
	modelManager_->Initialize(dxCommon_.get(), srvManager_.get());

	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Initialize(dxCommon_.get(), srvManager_.get());

	// ImGuiの準備
	ImGuiManager::GetInstance()->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());
	frameClock_ = std::make_unique<FrameClock>();
	frameClock_->Initialize();
}

void Framework::Finalize() {
	ImGuiManager::GetInstance()->Finalize();
	frameClock_.reset();
	particleManager_.reset();
	modelManager_.reset();
	object3dCommon_.reset();
	lightingSystem_.reset();
	spriteCommon_.reset();
	TextureManager::GetInstance()->Finalize();
	audio_->Finalize();
	winApp_->Finalize();
}

TextureManager* Framework::GetTextureManager() const {
	return TextureManager::GetInstance();
}

void Framework::Update() {
	frameClock_->Tick();
	audio_->Update(frameClock_->GetRealDeltaSeconds());
	if (winApp_->ProcessMessage()) {
		endRequest_ = true;
	}
	dxCommon_->ResizeSwapChainIfNeeded();
	input_->Update();
}
