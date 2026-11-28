#pragma once
#include <memory>

// 前方宣言：ビルドを速くし、お互いに読み込み合ってエラーになるのを防ぎます
class WinApp;
class DirectXCommon;
class Input;
class Audio;
class SrvManager;
class SpriteCommon;
class Object3dCommon;
class ModelManager;
class ParticleManager;
class TextureManager;
class LightingSystem;
class FrameClock;

/**
 * Frameworkクラス
 * エンジンの心臓部です。DirectXの初期化やメインループを管理します。
 * どこからでもエンジンにアクセスできるように「シングルトン」という仕組みを導入しています。
 */
class Framework {
public:
	Framework();
	virtual ~Framework();

	// ★シングルトン：
	// ゲーム内のどこからでも「Framework::GetInstance()」と書くだけで、このエンジンの機能を使えるようになります。
	static Framework* GetInstance();

	// ゲームの実行（初期化 -> ループ -> 終了）
	void Run();

	// 各フェーズの関数（仮想関数なので、MyGameなどで上書きできます）
	virtual void Initialize();
	virtual void Finalize();
	virtual void Update();
	virtual void Draw() = 0; // 描画はアプリ側（MyGame）で実装します

	// 終了リクエストが来ているか
	virtual bool IsEndRequest() { return endRequest_; }

	// --- ゲッター（部品を貸し出す窓口） ---
	Audio* GetAudio() const { return audio_.get(); }
	Input* GetInput() const { return input_.get(); }
	const WinApp* GetWinApp() const { return winApp_.get(); }
	void SetFieldMouseLook(bool enabled);
	SpriteCommon* GetSpriteCommon() const { return spriteCommon_.get(); }
	Object3dCommon* GetObject3dCommon() const { return object3dCommon_.get(); }
	ModelManager* GetModelManager() const { return modelManager_.get(); }
	ParticleManager* GetParticleManager() const { return particleManager_.get(); }
	LightingSystem* GetLightingSystem() const { return lightingSystem_.get(); }
	FrameClock* GetFrameClock() const { return frameClock_.get(); }
	DirectXCommon* GetDxCommon() const { return dxCommon_.get(); }
	SrvManager* GetSrvManager() const { return srvManager_.get(); }
	TextureManager* GetTextureManager() const;

protected:
    void RequestEnd() noexcept { endRequest_ = true; }

private:
	// 各種マネージャ（スマートポインタで安全に管理）
	std::unique_ptr<WinApp> winApp_;
	std::unique_ptr<DirectXCommon> dxCommon_;
	std::unique_ptr<SrvManager> srvManager_;
	std::unique_ptr<Input> input_;
	std::unique_ptr<Audio> audio_;

	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<Object3dCommon> object3dCommon_;
	std::unique_ptr<LightingSystem> lightingSystem_;
	std::unique_ptr<ModelManager> modelManager_;
	std::unique_ptr<ParticleManager> particleManager_;
	std::unique_ptr<FrameClock> frameClock_;

	bool endRequest_ = false;

private:
	// 唯一のインスタンスを指すためのポインタ
	static Framework* instance;
};
