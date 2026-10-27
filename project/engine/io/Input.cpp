#include "Input.h"

#include <assert.h>
#include <cassert>
#include <Xinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

namespace {
	constexpr int kMaxAcquireAttempts = 8;
	constexpr int kMinKeyboardKeyCode = 0;
	constexpr int kMaxKeyboardKeyCode = 255;

	bool IsRecoverableInputError(HRESULT result)
	{
		return result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED;
	}

	bool TryAcquireDevice(IDirectInputDevice8* device)
	{
		if (!device) {
			return false;
		}

		for (int attempt = 0; attempt < kMaxAcquireAttempts; ++attempt) {
			const HRESULT result = device->Acquire();
			if (SUCCEEDED(result)) {
				return true;
			}
			if (!IsRecoverableInputError(result)) {
				return false;
			}
		}

		return false;
	}

	bool IsKeyboardKeyCodeValid(int keyCode)
	{
		return keyCode >= kMinKeyboardKeyCode && keyCode <= kMaxKeyboardKeyCode;
	}

	int GetMouseButtonIndex(InputMouseButton button)
	{
		switch (button) {
		case InputMouseButton::Left:
			return 0;
		case InputMouseButton::Right:
			return 1;
		case InputMouseButton::Middle:
			return 2;
		case InputMouseButton::X1:
			return 3;
		case InputMouseButton::X2:
			return 4;
		default:
			return -1;
		}
	}

	unsigned short ToXInputGamepadButton(InputGamepadButton button)
	{
		switch (button) {
		case InputGamepadButton::A:
			return XINPUT_GAMEPAD_A;
		case InputGamepadButton::B:
			return XINPUT_GAMEPAD_B;
		case InputGamepadButton::X:
			return XINPUT_GAMEPAD_X;
		case InputGamepadButton::Y:
			return XINPUT_GAMEPAD_Y;
		case InputGamepadButton::LeftShoulder:
			return XINPUT_GAMEPAD_LEFT_SHOULDER;
		case InputGamepadButton::RightShoulder:
			return XINPUT_GAMEPAD_RIGHT_SHOULDER;
		case InputGamepadButton::Back:
			return XINPUT_GAMEPAD_BACK;
		case InputGamepadButton::Start:
			return XINPUT_GAMEPAD_START;
		case InputGamepadButton::LeftThumb:
			return XINPUT_GAMEPAD_LEFT_THUMB;
		case InputGamepadButton::RightThumb:
			return XINPUT_GAMEPAD_RIGHT_THUMB;
		case InputGamepadButton::DPadUp:
			return XINPUT_GAMEPAD_DPAD_UP;
		case InputGamepadButton::DPadDown:
			return XINPUT_GAMEPAD_DPAD_DOWN;
		case InputGamepadButton::DPadLeft:
			return XINPUT_GAMEPAD_DPAD_LEFT;
		case InputGamepadButton::DPadRight:
			return XINPUT_GAMEPAD_DPAD_RIGHT;
		default:
			return 0;
		}
	}

	float NormalizeThumbAxis(short value, short deadZone)
	{
		const int axisValue = static_cast<int>(value);
		const int absValue = axisValue < 0 ? -axisValue : axisValue;
		if (absValue <= deadZone) {
			return 0.0f;
		}

		const float maxValue = axisValue < 0 ? 32768.0f : 32767.0f;
		return static_cast<float>(axisValue) / maxValue;
	}

	float NormalizeTrigger(unsigned char value)
	{
		if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
			return 0.0f;
		}

		constexpr float kMaxTriggerValue = 255.0f;
		constexpr float kTriggerThreshold = static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		return (static_cast<float>(value) - kTriggerThreshold) / (kMaxTriggerValue - kTriggerThreshold);
	}
}

void Input::Initialize(WinApp* winApp)
{
	this->winApp_ = winApp;

	HRESULT result;
	result = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));
	DIPROPDWORD keyboardBuffer{};
	keyboardBuffer.diph.dwSize = sizeof(keyboardBuffer);
	keyboardBuffer.diph.dwHeaderSize = sizeof(keyboardBuffer.diph);
	keyboardBuffer.diph.dwHow = DIPH_DEVICE;
	keyboardBuffer.dwData = kKeyboardEventCapacity;
	keyboardBuffered_ = SUCCEEDED(keyboard->SetProperty(DIPROP_BUFFERSIZE, &keyboardBuffer.diph));
	if (!keyboardBuffered_) OutputDebugStringA("Keyboard event buffer unavailable; immediate input fallback.\n");
	ClearKeyboardState();

	result = directInput->CreateDevice(GUID_SysMouse, &mouse, NULL);
	assert(SUCCEEDED(result));

	result = mouse->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(result));

	result = mouse->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));
	DIPROPDWORD buffer{};
	buffer.diph.dwSize = sizeof(buffer);
	buffer.diph.dwHeaderSize = sizeof(buffer.diph);
	buffer.diph.dwHow = DIPH_DEVICE;
	buffer.dwData = kMouseEventCapacity;
	mouseBuffered_ = SUCCEEDED(mouse->SetProperty(DIPROP_BUFFERSIZE, &buffer.diph));
	if (!mouseBuffered_) OutputDebugStringA("Mouse event buffer unavailable; immediate input fallback.\n");
	ClearMouseState();
}

void Input::Update()
{
	if (!UpdateKeyboardState()) {
		ClearKeyboardState();
	}

	if (!UpdateMouseState()) {
		ClearMouseState();
	}

	gamepadButtonsPre_ = gamepadButtons_;
	if (!UpdateGamepadState()) {
		ClearGamepadState();
	}

	UpdateMousePosition();
}

bool Input::UpdateKeyboardState()
{
	if (!keyboard) {
		return false;
	}

	if (!winApp_ || GetForegroundWindow() != winApp_->GetHwnd()) {
		keyboard->Unacquire();
		return false;
	}
	keyboardState_.BeginFrame();
	KeyboardState::Snapshot snapshot{};
	HRESULT result = keyboard->GetDeviceState(sizeof(snapshot), snapshot.data());
	if (FAILED(result) && IsRecoverableInputError(result)) {
		keyboardSynchronized_ = false;
		if (TryAcquireDevice(keyboard.Get())) {
			result = keyboard->GetDeviceState(sizeof(snapshot), snapshot.data());
		}
	}
	if (FAILED(result)) return false;
	if (!keyboardSynchronized_) return ResynchronizeKeyboard();
	if (keyboardBuffered_) {
		std::array<DIDEVICEOBJECTDATA, kKeyboardEventCapacity> events{};
		DWORD count = kKeyboardEventCapacity;
		result = keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), events.data(), &count, 0);
		if (FAILED(result)) return false;
		// Discard incomplete history rather than inventing a retry/submit action.
		if (result == DI_BUFFEROVERFLOW || count > events.size()) return ResynchronizeKeyboard();
		for (DWORD i = 0; i < count; ++i) {
			const auto& event = events[i];
			if (event.dwOfs < KeyboardState::kKeyCount)
				keyboardState_.Apply(static_cast<int>(event.dwOfs), (event.dwData & 0x80) != 0);
		}
	} else keyboardState_.ApplySnapshot(snapshot);
	return true;
}

bool Input::ResynchronizeKeyboard()
{
	keyboardSynchronized_ = false;
	keyboardState_.Reset();
	if (keyboardBuffered_) {
		DWORD count = INFINITE;
		if (FAILED(keyboard->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, &count, 0))) return false;
	}
	KeyboardState::Snapshot snapshot{};
	if (FAILED(keyboard->GetDeviceState(sizeof(snapshot), snapshot.data()))) return false;
	keyboardState_.Synchronize(snapshot);
	keyboardSynchronized_ = true;
	return true;
}

bool Input::UpdateMouseState()
{
	if (!mouse) {
		return false;
	}
	if (!winApp_ || GetForegroundWindow() != winApp_->GetHwnd()) {
		mouse->Unacquire();
		return false;
	}

	mouseEdges_.BeginFrame();
	mouseDelta_ = { 0.0f, 0.0f };
	mouseWheelDelta_ = 0.0f;

	DIMOUSESTATE2 mouseState{};
	HRESULT result = mouse->GetDeviceState(sizeof(mouseState), &mouseState);
	if (FAILED(result) && IsRecoverableInputError(result)) {
		mouseSynchronized_ = false;
		if (TryAcquireDevice(mouse.Get())) {
			result = mouse->GetDeviceState(sizeof(mouseState), &mouseState);
		}
	}

	if (FAILED(result)) {
		return false;
	}

	if (!mouseSynchronized_) return ResynchronizeMouse();
	if (mouseBuffered_) {
		std::array<DIDEVICEOBJECTDATA, kMouseEventCapacity> events{};
		DWORD count = kMouseEventCapacity;
		result = mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), events.data(), &count, 0);
		if (FAILED(result)) return false;
		// Missing events cannot safely be reconstructed as gameplay actions.
		if (result == DI_BUFFEROVERFLOW || count > events.size()) return ResynchronizeMouse();
		for (DWORD i = 0; i < count; ++i) {
			const auto& event = events[i];
			if (event.dwOfs >= DIMOFS_BUTTON0 && event.dwOfs <= DIMOFS_BUTTON4)
				mouseEdges_.Apply(static_cast<int>(event.dwOfs - DIMOFS_BUTTON0), (event.dwData & 0x80) != 0);
		}
	} else {
		for (int i = 0; i < kMouseButtonCount; ++i) mouseEdges_.Apply(i, (mouseState.rgbButtons[i] & 0x80) != 0);
	}
	mouseDelta_ = {
		static_cast<float>(mouseState.lX),
		static_cast<float>(mouseState.lY)
	};
	mouseWheelDelta_ = static_cast<float>(mouseState.lZ);
	return true;
}

bool Input::ResynchronizeMouse()
{
	mouseSynchronized_ = false;
	mouseEdges_.Reset();
	if (mouseBuffered_) {
		DWORD count = INFINITE;
		if (FAILED(mouse->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), nullptr, &count, 0))) return false;
	}
	DIMOUSESTATE2 state{};
	if (FAILED(mouse->GetDeviceState(sizeof(state), &state))) return false;
	std::array<bool, kMouseButtonCount> held{};
	for (int i = 0; i < kMouseButtonCount; ++i) held[i] = (state.rgbButtons[i] & 0x80) != 0;
	// Focus restoration never synthesizes a click or a camera-motion jump.
	mouseEdges_.Synchronize(held);
	mouseSynchronized_ = true;
	return true;
}

bool Input::UpdateGamepadState()
{
	XINPUT_STATE state{};
	const DWORD result = XInputGetState(0, &state);
	if (result != ERROR_SUCCESS) {
		return false;
	}

	isGamepadConnected_ = true;
	gamepadButtons_ = state.Gamepad.wButtons;
	leftStick_ = {
		NormalizeThumbAxis(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),
		NormalizeThumbAxis(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	};
	rightStick_ = {
		NormalizeThumbAxis(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE),
		NormalizeThumbAxis(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
	};
	leftTrigger_ = NormalizeTrigger(state.Gamepad.bLeftTrigger);
	rightTrigger_ = NormalizeTrigger(state.Gamepad.bRightTrigger);
	return true;
}

void Input::UpdateMousePosition()
{
	POINT cursorPosition{};
	if (GetCursorPos(&cursorPosition)) {
		ScreenToClient(winApp_->GetHwnd(), &cursorPosition);
		mousePosition_ = {
			static_cast<float>(cursorPosition.x),
			static_cast<float>(cursorPosition.y)
		};
	}
}

void Input::ClearKeyboardState()
{
	keyboardState_.Reset();
	keyboardSynchronized_ = false;
}

void Input::ClearMouseState()
{
	mouseEdges_.Reset();
	mouseSynchronized_ = false;
	mouseDelta_ = { 0.0f, 0.0f };
	mouseWheelDelta_ = 0.0f;
}

void Input::ClearGamepadState()
{
	isGamepadConnected_ = false;
	gamepadButtonsPre_ = 0;
	gamepadButtons_ = 0;
	leftStick_ = { 0.0f, 0.0f };
	rightStick_ = { 0.0f, 0.0f };
	leftTrigger_ = 0.0f;
	rightTrigger_ = 0.0f;
}

bool Input::PushKey(BYTE keyNumber) const
{
	return keyboardState_.Held(keyNumber);
}

bool Input::PushKey(InputKey inputKey) const
{
	const int keyCode = ToDirectInputKey(inputKey);
	return IsKeyboardKeyCodeValid(keyCode) && PushKey(static_cast<BYTE>(keyCode));
}

bool Input::TriggerKey(BYTE keyNumber) const
{
	return keyboardState_.Pressed(keyNumber);
}

bool Input::TriggerKey(InputKey inputKey) const
{
	const int keyCode = ToDirectInputKey(inputKey);
	return IsKeyboardKeyCodeValid(keyCode) && TriggerKey(static_cast<BYTE>(keyCode));
}

bool Input::ConsumeTriggerKey(BYTE keyNumber)
{
	return keyboardState_.Consume(keyNumber);
}

bool Input::ConsumeTriggerKey(InputKey inputKey)
{
	const int keyCode = ToDirectInputKey(inputKey);
	return IsKeyboardKeyCodeValid(keyCode) &&
		ConsumeTriggerKey(static_cast<BYTE>(keyCode));
}

bool Input::ReleaseKey(BYTE keyNumber) const
{
	return keyboardState_.Released(keyNumber);
}

bool Input::ReleaseKey(InputKey inputKey) const
{
	const int keyCode = ToDirectInputKey(inputKey);
	return IsKeyboardKeyCodeValid(keyCode) && ReleaseKey(static_cast<BYTE>(keyCode));
}

bool Input::PushMouseButton(InputMouseButton button) const
{
	const int index = GetMouseButtonIndex(button);
	return mouseEdges_.Held(index);
}

bool Input::TriggerMouseButton(InputMouseButton button) const
{
	const int index = GetMouseButtonIndex(button);
	return mouseEdges_.Pressed(index);
}

bool Input::ReleaseMouseButton(InputMouseButton button) const
{
	const int index = GetMouseButtonIndex(button);
	return mouseEdges_.Released(index);
}

Vector2 Input::GetMousePosition() const
{
	return mousePosition_;
}

Vector2 Input::GetMouseDelta() const
{
	return mouseDelta_;
}

float Input::GetMouseWheelDelta() const
{
	return mouseWheelDelta_;
}

bool Input::IsGamepadConnected() const
{
	return isGamepadConnected_;
}

bool Input::PushGamepadButton(InputGamepadButton button) const
{
	const unsigned short buttonMask = ToXInputGamepadButton(button);
	return buttonMask != 0 && (gamepadButtons_ & buttonMask) != 0;
}

bool Input::TriggerGamepadButton(InputGamepadButton button) const
{
	const unsigned short buttonMask = ToXInputGamepadButton(button);
	return buttonMask != 0 && (gamepadButtons_ & buttonMask) != 0 && (gamepadButtonsPre_ & buttonMask) == 0;
}

bool Input::ReleaseGamepadButton(InputGamepadButton button) const
{
	const unsigned short buttonMask = ToXInputGamepadButton(button);
	return buttonMask != 0 && (gamepadButtons_ & buttonMask) == 0 && (gamepadButtonsPre_ & buttonMask) != 0;
}

Vector2 Input::GetLeftStick() const
{
	return leftStick_;
}

Vector2 Input::GetRightStick() const
{
	return rightStick_;
}

float Input::GetLeftTrigger() const
{
	return leftTrigger_;
}

float Input::GetRightTrigger() const
{
	return rightTrigger_;
}
