#pragma once
#include <wrl.h>
#include <Windows.h>
#include <stdint.h>
#include <bitset>


using namespace Microsoft::WRL;

// WindowsAPI
class WinApp {
public:// 静的メンバー関数
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
	// 初期化
	void Initialize();

	//更新
	void Update();

	// 終了
	void Finalize();
	void ToggleBorderlessFullscreen();
	void SetBorderlessFullscreen(bool enabled);
	void SetFieldMouseLook(bool enabled);
	POINT SampleFieldLookDelta() const noexcept;
	bool IsBorderlessFullscreen() const { return isBorderlessFullscreen_; }

#ifdef _DEBUG
	static const int32_t kClientWidth = 1600;
	static const int32_t kClientHeight = 900;
#else
	static const int32_t kClientWidth = 1280;
	static const int32_t kClientHeight = 720;
#endif
	// ウィンドウサイズを表す構造体体にクライアント領域を入れる
	RECT wrc = { 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };

	//　メッセージの処理
	bool ProcessMessage();

public:
	// Getter
	HWND GetHwnd() const { return hwnd; }
	struct LeftDragSample {
		POINT press{}, release{};
		bool began=false, ended=false, down=false;
	};
	const LeftDragSample& ReadLeftDrag() const noexcept { return leftDrag_; }
	bool WindowKeyPressed(unsigned key) const noexcept { return key<256 && windowPressed_[key]; }
	bool WindowKeyHeld(unsigned key) const noexcept { return key<256 && windowHeld_[key]; }
	uint32_t GetClientWidth() const;
	uint32_t GetClientHeight() const;

	// getter
	HINSTANCE GetHInstance() const { return wc.hInstance; }
private:

	HWND hwnd= nullptr;  // ← ウィンドウハンドルを保持するメンバ変数
	bool isBorderlessFullscreen_ = false;
	LONG_PTR windowedStyle_ = 0;
	LONG_PTR windowedExStyle_ = 0;
	RECT windowedRect_ = {};

	WNDCLASS wc{};
	LeftDragSample leftDrag_{};
	bool fieldMouseLook_ = false;
	POINT fieldLookOrigin_{};
	bool restoreFieldCursor_=false;
	std::bitset<256> windowPressed_,windowHeld_;
};
