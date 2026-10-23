#pragma once
#include "base/InitializationTask.h"

// 前方宣言
class SceneManager;

struct SceneClipPlanes {
    float nearClip = 0.1f;
    float farClip = 1000.0f;
};

/**
 * BaseSceneクラス
 * すべてのシーンの親クラスです（スライド25,26準拠）。
 */
class BaseScene {
private:
	// シーンマネージャを借りてくる（前方宣言を利用）
	SceneManager* sceneManager_ = nullptr;

protected:
    SceneManager* GetSceneManager() const noexcept { return sceneManager_; }

public:
	virtual ~BaseScene() = default;

	// 各シーンで必ず実装する関数
	virtual void Initialize() = 0;
    virtual InitializationTask InitializeIncrementally() { Initialize(); co_return true; }
	virtual void Finalize() = 0;
	virtual void PrepareFixedUpdate() {}
	virtual void FixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
	virtual void Update() = 0;
	// Optional presentation-only update; gameplay/input remain frozen during the cover.
	virtual void UpdateTransitionPresentation() {}
	// Called once after the opaque cover was presented; no Scene resource exchange occurs.
	virtual void OnScreenTransitionCovered() {}
	virtual void Draw() = 0;
	// Default depth range preserves scenes that do not supply a custom posteffect camera.
	[[nodiscard]] virtual SceneClipPlanes GetPostEffectClipPlanes() const noexcept { return {}; }

	// シーンマネージャをセットするSetter（スライド25準拠）
	virtual void SetSceneManager(SceneManager* sceneManager) {
		sceneManager_ = sceneManager;
	}
};
