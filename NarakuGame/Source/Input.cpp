#include "Input.h"
#include "InputConfig.h"
#include "imgui.h"
#include "imgui_internal.h"
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <Xinput.h>
#include <cmath>
#include <algorithm>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

// --- グローバル状態 ---
static BYTE g_keyTable[256]{};
static BYTE g_oldTable[256]{};
static XINPUT_STATE g_padState{};
static XINPUT_STATE g_oldPadState{};
static bool g_padConnected = false;
static DWORD g_xinputUserIndex = XUSER_MAX_COUNT;

static LPDIRECTINPUT8 g_pDirectInput = nullptr;
static LPDIRECTINPUTDEVICE8 g_pDirectInputPad = nullptr;
static DIJOYSTATE2 g_diPadState{};
static DIJOYSTATE2 g_oldDiPadState{};
static bool g_diPadConnected = false;

static HWND g_inputWindow = nullptr;
static POINT g_mousePos{};
static POINT g_oldMousePos{};
static bool g_mouseLeftDown = false;
static bool g_oldMouseLeftDown = false;
static bool g_mouseRightDown = false;
static bool g_oldMouseRightDown = false;
static bool g_mouseMiddleDown = false;
static bool g_oldMouseMiddleDown = false;
static float g_mouseWheelDelta = 0.0f;
static float g_mouseWheelAccumulated = 0.0f;

static float g_inputKeyboardMouseMs = 0.0f;
static float g_inputXInputMs = 0.0f;
static float g_inputDirectInputMs = 0.0f;

static ActiveInputDevice g_lastActiveDevice = ActiveInputDevice::KeyboardMouse;
static bool g_inputGuardActive = false;
static bool g_inputCaptureActive = false;
static bool g_lastPadWasDirectInput = false;

namespace
{
	template<typename T>
	constexpr T ClampVal(T v, T lo, T hi)
	{
		return (v < lo) ? lo : (v > hi) ? hi : v;
	}

	const SHORT kStickDeadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
	const SHORT kRightStickDeadZone = XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE;
	const BYTE kTriggerThreshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
	const float kStickPressThreshold = 0.35f;
	const LONG kDirectInputAxisRange = 1000;
	const LONG kDirectInputDeadZone = 250;
	const ULONGLONG kDirectInputRetryIntervalMs = 1000;
	ULONGLONG g_nextDirectInputEnumTick = 0;

	double QueryPerfMs()
	{
		static LARGE_INTEGER freq = [] {
			LARGE_INTEGER f{};
			QueryPerformanceFrequency(&f);
			return f;
		}();
		LARGE_INTEGER now{};
		QueryPerformanceCounter(&now);
		return static_cast<double>(now.QuadPart) * 1000.0 / static_cast<double>(freq.QuadPart);
	}

	float NormalizeThumbAxis(SHORT value, SHORT deadZone)
	{
		if (value > deadZone)
		{
			return static_cast<float>(value - deadZone) / static_cast<float>(32767 - deadZone);
		}
		if (value < -deadZone)
		{
			return static_cast<float>(value + deadZone) / static_cast<float>(32768 - deadZone);
		}
		return 0.0f;
	}

	float NormalizeDirectInputAxis(LONG value)
	{
		if (value > kDirectInputDeadZone)
		{
			return static_cast<float>(value - kDirectInputDeadZone) /
				static_cast<float>(kDirectInputAxisRange - kDirectInputDeadZone);
		}
		if (value < -kDirectInputDeadZone)
		{
			return static_cast<float>(value + kDirectInputDeadZone) /
				static_cast<float>(kDirectInputAxisRange - kDirectInputDeadZone);
		}
		return 0.0f;
	}

	bool IsRawKeyboardPress(BYTE key)
	{
		return (g_keyTable[key] & 0x80) != 0;
	}

	bool IsRawKeyboardTrigger(BYTE key)
	{
		return ((g_keyTable[key] ^ g_oldTable[key]) & g_keyTable[key] & 0x80) != 0;
	}

	bool IsRawKeyboardRelease(BYTE key)
	{
		return ((g_keyTable[key] ^ g_oldTable[key]) & g_oldTable[key] & 0x80) != 0;
	}

	bool IsPadButtonDown(const XINPUT_STATE& state, WORD button)
	{
		return (state.Gamepad.wButtons & button) != 0;
	}

	bool IsDirectInputButtonDown(const DIJOYSTATE2& state, int buttonIndex)
	{
		if (buttonIndex < 0 || buttonIndex >= 128) return false;
		return (state.rgbButtons[buttonIndex] & 0x80) != 0;
	}

	bool IsDirectInputPovDirection(const DIJOYSTATE2& state, int direction)
	{
		const DWORD pov = state.rgdwPOV[0];
		if (LOWORD(pov) == 0xFFFF) return false;

		switch (direction)
		{
		case 0: return (pov == 0 || pov == 4500 || pov == 31500);         // 上
		case 1: return (pov == 18000 || pov == 13500 || pov == 22500);   // 下
		case 2: return (pov == 27000 || pov == 22500 || pov == 31500);   // 左
		case 3: return (pov == 9000 || pov == 4500 || pov == 13500);     // 右
		default: return false;
		}
	}

	// Microsoft推奨: DirectInput列挙時にXInputデバイスを除外するためのチェック
	bool IsXInputDevice(const DIDEVICEINSTANCE* pDevInst)
	{
		// 1. デバイス名に Xbox や XInput が含まれているかをチェック
		if (pDevInst->tszProductName[0] != 0)
		{
			std::string prodName = pDevInst->tszProductName;
			std::transform(prodName.begin(), prodName.end(), prodName.begin(), ::tolower);
			if (prodName.find("xbox") != std::string::npos || prodName.find("xinput") != std::string::npos)
			{
				return true;
			}
		}

		// 2. RawInput APIで "IG_" を含むかチェック
		UINT nDevices = 0;
		if (GetRawInputDeviceList(nullptr, &nDevices, sizeof(RAWINPUTDEVICELIST)) != 0 || nDevices == 0)
		{
			return false;
		}

		std::vector<RAWINPUTDEVICELIST> devList(nDevices);
		if (GetRawInputDeviceList(devList.data(), &nDevices, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1))
		{
			return false;
		}

		for (const auto& dev : devList)
		{
			if (dev.dwType != RIM_TYPEHID) continue;

			UINT nameLength = 0;
			GetRawInputDeviceInfoA(dev.hDevice, RIDI_DEVICENAME, nullptr, &nameLength);
			if (nameLength == 0) continue;

			std::string devName(nameLength, '\0');
			if (GetRawInputDeviceInfoA(dev.hDevice, RIDI_DEVICENAME, &devName[0], &nameLength) != static_cast<UINT>(-1))
			{
				if (devName.find("IG_") != std::string::npos)
				{
					// DirectInputデバイスのVID/PIDとRawInputのVID/PIDが一致するか判定
					DWORD vid = LOWORD(pDevInst->guidProduct.Data1);
					DWORD pid = HIWORD(pDevInst->guidProduct.Data1);
					char vidPidStr[64];
					snprintf(vidPidStr, sizeof(vidPidStr), "VID_%04X&PID_%04X", vid, pid);
					if (devName.find(vidPidStr) != std::string::npos)
					{
						return true;
					}
				}
			}
		}

		return false;
	}

	HWND GetInputWindow()
	{
		if (g_inputWindow && IsWindow(g_inputWindow)) return g_inputWindow;
		HWND hWnd = GetActiveWindow();
		if (!hWnd) hWnd = GetForegroundWindow();
		if (!hWnd) hWnd = GetFocus();
		if (hWnd) g_inputWindow = hWnd;
		return hWnd;
	}

	POINT QueryMousePosition()
	{
		POINT pt{};
		::GetCursorPos(&pt);
		HWND hWnd = GetInputWindow();
		if (hWnd) ::ScreenToClient(hWnd, &pt);
		return pt;
	}

	bool SetDirectInputAxisRange(LPDIRECTINPUTDEVICE8 device, DWORD objectOffset)
	{
		if (!device) return false;
		DIPROPRANGE range{};
		range.diph.dwSize = sizeof(range);
		range.diph.dwHeaderSize = sizeof(range.diph);
		range.diph.dwHow = DIPH_BYOFFSET;
		range.diph.dwObj = objectOffset;
		range.lMin = -kDirectInputAxisRange;
		range.lMax = kDirectInputAxisRange;
		return SUCCEEDED(device->SetProperty(DIPROP_RANGE, &range.diph));
	}

	void ReleaseDirectInputPad()
	{
		if (g_pDirectInputPad)
		{
			g_pDirectInputPad->Unacquire();
			g_pDirectInputPad->Release();
			g_pDirectInputPad = nullptr;
		}
		g_diPadConnected = false;
		ZeroMemory(&g_diPadState, sizeof(g_diPadState));
	}

	bool EnsureDirectInputCreated()
	{
		if (g_pDirectInput) return true;
		return SUCCEEDED(DirectInput8Create(
			GetModuleHandle(nullptr),
			DIRECTINPUT_VERSION,
			IID_IDirectInput8,
			reinterpret_cast<void**>(&g_pDirectInput),
			nullptr)) && g_pDirectInput;
	}

	BOOL CALLBACK EnumGameControllerCallback(const DIDEVICEINSTANCE* instance, VOID* context)
	{
		if (!g_pDirectInput || g_pDirectInputPad) return DIENUM_STOP;

		// XInputデバイスであれば除外（二重列挙・二重入力を防ぐ）
		if (IsXInputDevice(instance))
		{
			return DIENUM_CONTINUE;
		}

		HWND hWnd = static_cast<HWND>(context);
		LPDIRECTINPUTDEVICE8 device = nullptr;
		if (FAILED(g_pDirectInput->CreateDevice(instance->guidInstance, &device, nullptr)) || !device)
		{
			return DIENUM_CONTINUE;
		}

		if (FAILED(device->SetDataFormat(&c_dfDIJoystick2)))
		{
			device->Release();
			return DIENUM_CONTINUE;
		}

		if (FAILED(device->SetCooperativeLevel(hWnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE)))
		{
			device->Release();
			return DIENUM_CONTINUE;
		}

		SetDirectInputAxisRange(device, DIJOFS_X);
		SetDirectInputAxisRange(device, DIJOFS_Y);
		SetDirectInputAxisRange(device, DIJOFS_Z);
		SetDirectInputAxisRange(device, DIJOFS_RX);
		SetDirectInputAxisRange(device, DIJOFS_RY);
		SetDirectInputAxisRange(device, DIJOFS_RZ);
		device->Acquire();

		g_pDirectInputPad = device;
		return DIENUM_STOP;
	}

	bool TryEnumerateDirectInputPad()
	{
		if (g_pDirectInputPad) return true;

		const ULONGLONG nowTick = GetTickCount64();
		if (nowTick < g_nextDirectInputEnumTick) return false;

		HWND hWnd = GetInputWindow();
		if (!hWnd || !EnsureDirectInputCreated())
		{
			g_nextDirectInputEnumTick = nowTick + kDirectInputRetryIntervalMs;
			return false;
		}

		g_pDirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumGameControllerCallback, hWnd, DIEDFL_ATTACHEDONLY);
		g_nextDirectInputEnumTick = g_pDirectInputPad ? 0 : (nowTick + kDirectInputRetryIntervalMs);
		return g_pDirectInputPad != nullptr;
	}

	HRESULT ReadDirectInputPadState(DIJOYSTATE2* outState)
	{
		if (!g_pDirectInputPad || !outState) return E_FAIL;

		HRESULT hr = g_pDirectInputPad->Poll();
		if (FAILED(hr))
		{
			hr = g_pDirectInputPad->Acquire();
			while (hr == DIERR_INPUTLOST)
			{
				hr = g_pDirectInputPad->Acquire();
			}
			if (FAILED(hr)) return hr;
			hr = g_pDirectInputPad->Poll();
			if (FAILED(hr)) return hr;
		}

		return g_pDirectInputPad->GetDeviceState(sizeof(*outState), outState);
	}

	void UpdateDirectInputState()
	{
		const bool hadConnection = g_diPadConnected;
		if (!TryEnumerateDirectInputPad())
		{
			g_diPadConnected = false;
			ZeroMemory(&g_diPadState, sizeof(g_diPadState));
			return;
		}

		DIJOYSTATE2 state{};
		HRESULT hr = ReadDirectInputPadState(&state);
		if (FAILED(hr))
		{
			ReleaseDirectInputPad();
			if (!TryEnumerateDirectInputPad()) return;
			hr = ReadDirectInputPadState(&state);
		}

		if (SUCCEEDED(hr))
		{
			g_diPadConnected = true;
			g_diPadState = state;
			if (!hadConnection) g_oldDiPadState = state;
		}
		else
		{
			ReleaseDirectInputPad();
		}
	}

	void UpdateXInputState()
	{
		const bool hadConnection = g_padConnected;
		const DWORD previousIndex = g_xinputUserIndex;
		XINPUT_STATE resolvedState{};
		DWORD resolvedIndex = XUSER_MAX_COUNT;

		auto tryGetState = [&](DWORD userIndex) -> bool
		{
			XINPUT_STATE state{};
			if (XInputGetState(userIndex, &state) != ERROR_SUCCESS) return false;
			resolvedState = state;
			resolvedIndex = userIndex;
			return true;
		};

		if (previousIndex < XUSER_MAX_COUNT)
		{
			tryGetState(previousIndex);
		}
		if (resolvedIndex == XUSER_MAX_COUNT)
		{
			for (DWORD userIndex = 0; userIndex < XUSER_MAX_COUNT; ++userIndex)
			{
				if (userIndex == previousIndex) continue;
				if (tryGetState(userIndex)) break;
			}
		}

		if (resolvedIndex < XUSER_MAX_COUNT)
		{
			g_padConnected = true;
			g_xinputUserIndex = resolvedIndex;
			g_padState = resolvedState;
			if (!hadConnection || previousIndex != resolvedIndex) g_oldPadState = resolvedState;
		}
		else
		{
			g_padConnected = false;
			g_xinputUserIndex = XUSER_MAX_COUNT;
			ZeroMemory(&g_padState, sizeof(g_padState));
			if (!hadConnection) ZeroMemory(&g_oldPadState, sizeof(g_oldPadState));
		}
	}

	// --- 内部判定関数 ---
	bool CheckKbmCodeDown(int code)
	{
		if (code == 0) return false;
		if (code == MOUSE_BUTTON_LEFT) return g_mouseLeftDown;
		if (code == MOUSE_BUTTON_RIGHT) return g_mouseRightDown;
		if (code == MOUSE_BUTTON_MIDDLE) return g_mouseMiddleDown;
		if (code >= 1 && code < 256) return IsRawKeyboardPress(static_cast<BYTE>(code));
		return false;
	}

	bool CheckKbmCodeOld(int code)
	{
		if (code == 0) return false;
		if (code == MOUSE_BUTTON_LEFT) return g_oldMouseLeftDown;
		if (code == MOUSE_BUTTON_RIGHT) return g_oldMouseRightDown;
		if (code == MOUSE_BUTTON_MIDDLE) return g_oldMouseMiddleDown;
		if (code >= 1 && code < 256) return (g_oldTable[code] & 0x80) != 0;
		return false;
	}

	bool CheckXInputCodeDown(int code, const XINPUT_STATE& state)
	{
		if (code == 0) return false;
		if (code == XINPUT_VIRTUAL_LT) return state.Gamepad.bLeftTrigger > kTriggerThreshold;
		if (code == XINPUT_VIRTUAL_RT) return state.Gamepad.bRightTrigger > kTriggerThreshold;
		return (state.Gamepad.wButtons & static_cast<WORD>(code)) != 0;
	}

	bool CheckDInputCodeDown(int code, const DIJOYSTATE2& state)
	{
		if (code < 0) return false;
		if (code >= DINPUT_BUTTON_BASE && code < DINPUT_BUTTON_BASE + 32)
		{
			return IsDirectInputButtonDown(state, code - DINPUT_BUTTON_BASE);
		}
		if (code == DINPUT_POV_UP) return IsDirectInputPovDirection(state, 0);
		if (code == DINPUT_POV_DOWN) return IsDirectInputPovDirection(state, 1);
		if (code == DINPUT_POV_LEFT) return IsDirectInputPovDirection(state, 2);
		if (code == DINPUT_POV_RIGHT) return IsDirectInputPovDirection(state, 3);
		if (code == DINPUT_AXIS_Z_POS) return state.lZ > kDirectInputDeadZone;
		if (code == DINPUT_AXIS_Z_NEG) return state.lZ < -kDirectInputDeadZone;
		if (code == DINPUT_AXIS_RZ_POS) return state.lRz > kDirectInputDeadZone;
		if (code == DINPUT_AXIS_RZ_NEG) return state.lRz < -kDirectInputDeadZone;
		return false;
	}

	bool CheckActionPressInternal(GameAction action)
	{
		const InputConfig& cfg = InputConfig::GetInstance();
		const int kbmCode = cfg.GetBinding(action, 0);
		if (CheckKbmCodeDown(kbmCode)) return true;

		if (g_padConnected)
		{
			const int xinCode = cfg.GetBinding(action, 1);
			if (CheckXInputCodeDown(xinCode, g_padState)) return true;
		}

		if (g_diPadConnected)
		{
			const int dinCode = cfg.GetBinding(action, 2);
			if (CheckDInputCodeDown(dinCode, g_diPadState)) return true;
		}

		return false;
	}

	bool CheckActionTriggerInternal(GameAction action)
	{
		const InputConfig& cfg = InputConfig::GetInstance();
		const int kbmCode = cfg.GetBinding(action, 0);
		if (CheckKbmCodeDown(kbmCode) && !CheckKbmCodeOld(kbmCode)) return true;

		if (g_padConnected)
		{
			const int xinCode = cfg.GetBinding(action, 1);
			if (CheckXInputCodeDown(xinCode, g_padState) && !CheckXInputCodeDown(xinCode, g_oldPadState)) return true;
		}

		if (g_diPadConnected)
		{
			const int dinCode = cfg.GetBinding(action, 2);
			if (CheckDInputCodeDown(dinCode, g_diPadState) && !CheckDInputCodeDown(dinCode, g_oldDiPadState)) return true;
		}

		return false;
	}

	bool CheckActionReleaseInternal(GameAction action)
	{
		const InputConfig& cfg = InputConfig::GetInstance();
		const int kbmCode = cfg.GetBinding(action, 0);
		if (!CheckKbmCodeDown(kbmCode) && CheckKbmCodeOld(kbmCode)) return true;

		if (g_padConnected)
		{
			const int xinCode = cfg.GetBinding(action, 1);
			if (!CheckXInputCodeDown(xinCode, g_padState) && CheckXInputCodeDown(xinCode, g_oldPadState)) return true;
		}

		if (g_diPadConnected)
		{
			const int dinCode = cfg.GetBinding(action, 2);
			if (!CheckDInputCodeDown(dinCode, g_diPadState) && CheckDInputCodeDown(dinCode, g_oldDiPadState)) return true;
		}

		return false;
	}

	// 画面遷移に使うよう設定されたアクション入力がすべて離されているかをチェック
	bool AreAllMajorButtonsReleased()
	{
		const InputConfig& config = InputConfig::GetInstance();
		for (int actionIndex = 0; actionIndex < static_cast<int>(GameAction::Count); ++actionIndex)
		{
			const GameAction action = static_cast<GameAction>(actionIndex);
			if (CheckKbmCodeDown(config.GetBinding(action, 0)))
			{
				return false;
			}
			if (g_padConnected && CheckXInputCodeDown(config.GetBinding(action, 1), g_padState))
			{
				return false;
			}
			if (g_diPadConnected && CheckDInputCodeDown(config.GetBinding(action, 2), g_diPadState))
			{
				return false;
			}
		}
		return true;
	}
}

HRESULT InitInput(HWND hWnd)
{
	g_inputWindow = hWnd;
	GetKeyboardState(g_keyTable);
	memcpy_s(g_oldTable, sizeof(g_oldTable), g_keyTable, sizeof(g_keyTable));

	UpdateXInputState();
	g_oldPadState = g_padState;

	TryEnumerateDirectInputPad();
	UpdateDirectInputState();
	g_oldDiPadState = g_diPadState;

	g_mousePos = QueryMousePosition();
	g_oldMousePos = g_mousePos;
	g_mouseLeftDown = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	g_oldMouseLeftDown = g_mouseLeftDown;
	g_mouseRightDown = (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	g_oldMouseRightDown = g_mouseRightDown;
	g_mouseMiddleDown = (::GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	g_oldMouseMiddleDown = g_mouseMiddleDown;

	return S_OK;
}

void UninitInput()
{
	ReleaseDirectInputPad();
	if (g_pDirectInput)
	{
		g_pDirectInput->Release();
		g_pDirectInput = nullptr;
	}
	g_inputWindow = nullptr;
	g_xinputUserIndex = XUSER_MAX_COUNT;
	g_padConnected = false;
	ZeroMemory(&g_padState, sizeof(g_padState));
	ZeroMemory(&g_oldPadState, sizeof(g_oldPadState));
}

void UpdateInput()
{
	double sectionStart = QueryPerfMs();
	memcpy_s(g_oldTable, sizeof(g_oldTable), g_keyTable, sizeof(g_keyTable));
	GetKeyboardState(g_keyTable);

	g_oldMousePos = g_mousePos;
	g_oldMouseLeftDown = g_mouseLeftDown;
	g_oldMouseRightDown = g_mouseRightDown;
	g_oldMouseMiddleDown = g_mouseMiddleDown;
	g_mousePos = QueryMousePosition();
	g_mouseLeftDown = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	g_mouseRightDown = (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
	g_mouseMiddleDown = (::GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	g_mouseWheelDelta = g_mouseWheelAccumulated;
	g_mouseWheelAccumulated = 0.0f;
	g_inputKeyboardMouseMs = static_cast<float>(QueryPerfMs() - sectionStart);

	sectionStart = QueryPerfMs();
	g_oldPadState = g_padState;
	UpdateXInputState();
	g_inputXInputMs = static_cast<float>(QueryPerfMs() - sectionStart);

	sectionStart = QueryPerfMs();
	g_oldDiPadState = g_diPadState;
	UpdateDirectInputState();
	g_inputDirectInputMs = static_cast<float>(QueryPerfMs() - sectionStart);

	// 直近のアクティブデバイス判定
	bool anyKbm = (g_mousePos.x != g_oldMousePos.x || g_mousePos.y != g_oldMousePos.y ||
		g_mouseLeftDown || g_mouseRightDown || g_mouseMiddleDown || g_mouseWheelDelta != 0.0f);
	if (!anyKbm)
	{
		for (int i = 0; i < 256; ++i)
		{
			if (IsRawKeyboardTrigger(static_cast<BYTE>(i))) { anyKbm = true; break; }
		}
	}
	if (anyKbm) g_lastActiveDevice = ActiveInputDevice::KeyboardMouse;

	bool anyXInput = false;
	if (g_padConnected)
	{
		if (g_padState.Gamepad.wButtons != 0 ||
			g_padState.Gamepad.bLeftTrigger > kTriggerThreshold ||
			g_padState.Gamepad.bRightTrigger > kTriggerThreshold ||
			std::abs(g_padState.Gamepad.sThumbLX) > kStickDeadZone ||
			std::abs(g_padState.Gamepad.sThumbLY) > kStickDeadZone ||
			std::abs(g_padState.Gamepad.sThumbRX) > kRightStickDeadZone ||
			std::abs(g_padState.Gamepad.sThumbRY) > kRightStickDeadZone)
		{
			anyXInput = true;
		}
	}
	bool anyDirectInput = false;
	if (g_diPadConnected)
	{
		for (int i = 0; i < 32; ++i)
		{
			if (IsDirectInputButtonDown(g_diPadState, i)) { anyDirectInput = true; break; }
		}
		if (LOWORD(g_diPadState.rgdwPOV[0]) != 0xFFFF ||
			std::abs(g_diPadState.lX) > kDirectInputDeadZone ||
			std::abs(g_diPadState.lY) > kDirectInputDeadZone)
		{
			anyDirectInput = true;
		}
	}
	if (anyXInput || anyDirectInput)
	{
		g_lastActiveDevice = ActiveInputDevice::Gamepad;
		if (anyDirectInput) g_lastPadWasDirectInput = true;
		else if (anyXInput) g_lastPadWasDirectInput = false;
	}

	// 画面遷移に使った入力が完全に離されるまで次の画面へ渡さない。
	if (g_inputGuardActive)
	{
		if (AreAllMajorButtonsReleased())
		{
			g_inputGuardActive = false;
		}
	}
}

float GetInputKeyboardMouseMs() { return g_inputKeyboardMouseMs; }
float GetInputXInputMs() { return g_inputXInputMs; }
float GetInputDirectInputMs() { return g_inputDirectInputMs; }

bool IsPadConnected() { return g_padConnected || g_diPadConnected; }
bool IsXInputConnected() { return g_padConnected; }
bool IsDirectInputConnected() { return g_diPadConnected; }
ActiveInputDevice GetLastActiveDevice() { return g_lastActiveDevice; }
bool IsUsingDirectInput() { return g_diPadConnected && (!g_padConnected || g_lastPadWasDirectInput); }

void SetInputGuardActive(bool active)
{
	g_inputGuardActive = active;
}

bool IsInputGuarded()
{
	return g_inputGuardActive;
}

// --- ゲームアクション単位の入力取得 ---
void GetActionMoveVector(float& outX, float& outY)
{
	outX = 0.0f;
	outY = 0.0f;
	if (g_inputGuardActive || g_inputCaptureActive) return;

	// 1. キーボード
	if (CheckActionPressInternal(GameAction::MoveRight)) outX += 1.0f;
	if (CheckActionPressInternal(GameAction::MoveLeft))  outX -= 1.0f;
	if (CheckActionPressInternal(GameAction::MoveUp))    outY += 1.0f;
	if (CheckActionPressInternal(GameAction::MoveDown))  outY -= 1.0f;

	// 2. パッドの左スティック
	const float stickX = GetPadLeftStickX();
	const float stickY = GetPadLeftStickY();
	if (std::abs(stickX) > 0.001f || std::abs(stickY) > 0.001f)
	{
		outX += stickX;
		outY += stickY;
	}

	// クランプ / 正規化前の合成
	outX = ClampVal(outX, -1.0f, 1.0f);
	outY = ClampVal(outY, -1.0f, 1.0f);
}

bool IsActionAttackTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::Attack);
}

bool IsActionAttackPress()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionPressInternal(GameAction::Attack);
}

bool IsActionInteractTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::Interact);
}

bool IsActionJumpTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::Jump);
}

bool IsActionDashStepPress()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionPressInternal(GameAction::DashStep);
}

bool IsActionDashStepTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::DashStep);
}

bool IsActionDashStepRelease()
{
	// リリースはガード中でも通す（状態リセットのため）
	return CheckActionReleaseInternal(GameAction::DashStep);
}

bool IsActionSkillPress()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionPressInternal(GameAction::Skill);
}

bool IsActionSkillTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::Skill);
}

bool IsActionSkillRelease()
{
	return CheckActionReleaseInternal(GameAction::Skill);
}

bool IsActionMenuMapTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::MenuMap);
}

bool IsActionMenuInventoryTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::MenuInventory);
}

bool IsActionMenuSettingsTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::MenuSettings);
}

bool IsActionCameraZoomHoldPress()
{
	return CheckActionPressInternal(GameAction::CameraZoomHold);
}

void GetActionCameraRotation(float& outYaw, float& outPitch)
{
	outYaw = 0.0f;
	outPitch = 0.0f;

	// マウス右ドラッグ
	if (IsMouseRightPress())
	{
		constexpr float mouseSensitivity = 0.006f;
		const POINT delta = GetMouseDelta();
		outYaw += static_cast<float>(delta.x) * mouseSensitivity;
		outPitch += static_cast<float>(delta.y) * mouseSensitivity;
	}

	// 右スティック（常時回転受付、LBズーム中であっても左右旋回は常に有効）
	constexpr float stickSensitivity = 0.06f;
	const float rx = GetPadRightStickX();
	const float ry = GetPadRightStickY();
	if (std::abs(rx) > 0.01f || std::abs(ry) > 0.01f)
	{
		outYaw += rx * stickSensitivity;
		// LBホールドズーム中でなければ上下仰角も調整
		if (!IsActionCameraZoomHoldPress())
		{
			outPitch -= ry * stickSensitivity;
		}
	}
}

float GetActionCameraZoomDelta()
{
	float zoom = 0.0f;

	// マウスホイール
	zoom += g_mouseWheelDelta;

	// LBを押しながら右スティック上下
	if (IsActionCameraZoomHoldPress())
	{
		const float ry = GetPadRightStickY();
		if (std::abs(ry) > 0.1f)
		{
			zoom += ry * 0.2f; // 前後倒しでズーム
		}
	}

	return zoom;
}

// --- UI操作用入力 ---
bool IsActionUIConfirmTrigger()
{
	if (IsRawKeyboardTrigger(VK_RETURN) || IsRawKeyboardTrigger(VK_SPACE)) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_A) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_A)) return true;
	if (g_diPadConnected && IsDirectInputButtonDown(g_diPadState, 0) && !IsDirectInputButtonDown(g_oldDiPadState, 0)) return true;
	return false;
}

bool IsActionUIBackTrigger()
{
	if (IsRawKeyboardTrigger(VK_ESCAPE)) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_B) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_B)) return true;
	if (g_diPadConnected && IsDirectInputButtonDown(g_diPadState, 1) && !IsDirectInputButtonDown(g_oldDiPadState, 1)) return true;
	return false;
}

bool IsActionUITabPrevTrigger()
{
	if (IsRawKeyboardTrigger('Q')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER)) return true;
	if (g_diPadConnected && IsDirectInputButtonDown(g_diPadState, 4) && !IsDirectInputButtonDown(g_oldDiPadState, 4)) return true;
	return false;
}

bool IsActionUITabNextTrigger()
{
	if (IsRawKeyboardTrigger('E')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER)) return true;
	if (g_diPadConnected && IsDirectInputButtonDown(g_diPadState, 5) && !IsDirectInputButtonDown(g_oldDiPadState, 5)) return true;
	return false;
}

bool IsActionUINavUpTrigger()
{
	if (IsRawKeyboardTrigger(VK_UP) || IsRawKeyboardTrigger('W')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP)) return true;
	if (g_diPadConnected && IsDirectInputPovDirection(g_diPadState, 0) && !IsDirectInputPovDirection(g_oldDiPadState, 0)) return true;
	return false;
}

bool IsActionUINavDownTrigger()
{
	if (IsRawKeyboardTrigger(VK_DOWN) || IsRawKeyboardTrigger('S')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)) return true;
	if (g_diPadConnected && IsDirectInputPovDirection(g_diPadState, 1) && !IsDirectInputPovDirection(g_oldDiPadState, 1)) return true;
	return false;
}

bool IsActionUINavLeftTrigger()
{
	if (IsRawKeyboardTrigger(VK_LEFT) || IsRawKeyboardTrigger('A')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)) return true;
	if (g_diPadConnected && IsDirectInputPovDirection(g_diPadState, 2) && !IsDirectInputPovDirection(g_oldDiPadState, 2)) return true;
	return false;
}

bool IsActionUINavRightTrigger()
{
	if (IsRawKeyboardTrigger(VK_RIGHT) || IsRawKeyboardTrigger('D')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)) return true;
	if (g_diPadConnected && IsDirectInputPovDirection(g_diPadState, 3) && !IsDirectInputPovDirection(g_oldDiPadState, 3)) return true;
	return false;
}

// --- 地図操作用入力 ---
void GetActionMapScroll(float& outX, float& outY)
{
	outX = GetPadLeftStickX();
	outY = GetPadLeftStickY();
	if (g_padConnected)
	{
		if (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) outX -= 1.0f;
		if (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) outX += 1.0f;
		if (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) outY += 1.0f;
		if (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) outY -= 1.0f;
	}
	if (g_diPadConnected)
	{
		if (IsDirectInputPovDirection(g_diPadState, 2)) outX -= 1.0f;
		if (IsDirectInputPovDirection(g_diPadState, 3)) outX += 1.0f;
		if (IsDirectInputPovDirection(g_diPadState, 0)) outY += 1.0f;
		if (IsDirectInputPovDirection(g_diPadState, 1)) outY -= 1.0f;
	}
	outX = ClampVal(outX, -1.0f, 1.0f);
	outY = ClampVal(outY, -1.0f, 1.0f);
}

float GetActionMapZoom()
{
	float zoom = 0.0f;
	const float ry = GetPadRightStickY();
	if (std::abs(ry) > 0.1f) zoom += ry * 0.1f;
	return zoom;
}

bool IsActionMapPinTrigger()
{
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_X) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_X)) return true;
	if (g_diPadConnected && IsDirectInputButtonDown(g_diPadState, 2) && !IsDirectInputButtonDown(g_oldDiPadState, 2)) return true;
	return false;
}

bool IsActionMapFocusPlayerTrigger()
{
	if (IsRawKeyboardTrigger('F')) return true;
	if (g_padConnected && (g_padState.Gamepad.wButtons & XINPUT_GAMEPAD_Y) && !(g_oldPadState.Gamepad.wButtons & XINPUT_GAMEPAD_Y)) return true;
	if (g_diPadConnected && IsDirectInputButtonDown(g_diPadState, 3) && !IsDirectInputButtonDown(g_oldDiPadState, 3)) return true;
	return false;
}

// --- リバインド（キーコンフィグ用入力検知） ---
bool GetAnyKbmTrigger(int& outCode)
{
	if (IsMouseLeftTrigger()) { outCode = MOUSE_BUTTON_LEFT; return true; }
	if (IsMouseRightTrigger()) { outCode = MOUSE_BUTTON_RIGHT; return true; }
	if (IsMouseMiddleTrigger()) { outCode = MOUSE_BUTTON_MIDDLE; return true; }

    for (int i = 1; i < 256; ++i)
    {
        if (i == VK_LBUTTON || i == VK_RBUTTON || i == VK_MBUTTON) continue;
        if (IsRawKeyboardTrigger(static_cast<BYTE>(i))) { outCode = i; return true; }
    }
	return false;
}

bool GetAnyXInputTrigger(int& outCode)
{
	if (!g_padConnected) return false;

	const WORD btns[] = {
		XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
		XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
		XINPUT_GAMEPAD_START, XINPUT_GAMEPAD_BACK,
		XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB,
		XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN,
		XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT
	};
	for (WORD b : btns)
	{
		if ((g_padState.Gamepad.wButtons & b) && !(g_oldPadState.Gamepad.wButtons & b))
		{
			outCode = b;
			return true;
		}
	}

	if (g_padState.Gamepad.bLeftTrigger > kTriggerThreshold && g_oldPadState.Gamepad.bLeftTrigger <= kTriggerThreshold)
	{
		outCode = XINPUT_VIRTUAL_LT;
		return true;
	}
	if (g_padState.Gamepad.bRightTrigger > kTriggerThreshold && g_oldPadState.Gamepad.bRightTrigger <= kTriggerThreshold)
	{
		outCode = XINPUT_VIRTUAL_RT;
		return true;
	}

	return false;
}

bool GetAnyDInputTrigger(int& outCode)
{
	if (!g_diPadConnected) return false;

	for (int i = 0; i < 32; ++i)
	{
		if (IsDirectInputButtonDown(g_diPadState, i) && !IsDirectInputButtonDown(g_oldDiPadState, i))
		{
			outCode = i;
			return true;
		}
	}

	if (IsDirectInputPovDirection(g_diPadState, 0) && !IsDirectInputPovDirection(g_oldDiPadState, 0)) { outCode = DINPUT_POV_UP; return true; }
	if (IsDirectInputPovDirection(g_diPadState, 1) && !IsDirectInputPovDirection(g_oldDiPadState, 1)) { outCode = DINPUT_POV_DOWN; return true; }
	if (IsDirectInputPovDirection(g_diPadState, 2) && !IsDirectInputPovDirection(g_oldDiPadState, 2)) { outCode = DINPUT_POV_LEFT; return true; }
	if (IsDirectInputPovDirection(g_diPadState, 3) && !IsDirectInputPovDirection(g_oldDiPadState, 3)) { outCode = DINPUT_POV_RIGHT; return true; }

	return false;
}

// --- 従来の個別API（互換性維持） ---
bool IsKeyPress(BYTE key)
{
	return IsRawKeyboardPress(key);
}

bool IsKeyTrigger(BYTE key)
{
	return IsRawKeyboardTrigger(key);
}

bool IsKeyRelease(BYTE key)
{
	return IsRawKeyboardRelease(key);
}

bool IsKeyRepeat(BYTE key)
{
	return false;
}

bool IsRawKeyPress(BYTE key)
{
	return IsRawKeyboardPress(key);
}

bool IsRawKeyTrigger(BYTE key)
{
	return IsRawKeyboardTrigger(key);
}

bool IsMenuConfirmTrigger()
{
	return IsActionUIConfirmTrigger();
}

bool IsMenuBackTrigger()
{
	return IsActionUIBackTrigger();
}

bool IsDirectInputButtonPressed(int buttonIndex)
{
	return g_diPadConnected && IsDirectInputButtonDown(g_diPadState, buttonIndex);
}

LONG GetDirectInputAxisX() { return g_diPadConnected ? g_diPadState.lX : 0; }
LONG GetDirectInputAxisY() { return g_diPadConnected ? g_diPadState.lY : 0; }
DWORD GetDirectInputPov() { return g_diPadConnected ? g_diPadState.rgdwPOV[0] : 0xFFFFFFFFu; }

float GetPadLeftStickX()
{
	if (g_diPadConnected && IsUsingDirectInput()) return NormalizeDirectInputAxis(g_diPadState.lX);
	if (g_padConnected) return NormalizeThumbAxis(g_padState.Gamepad.sThumbLX, kStickDeadZone);
	return 0.0f;
}

float GetPadLeftStickY()
{
	if (g_diPadConnected && IsUsingDirectInput()) return -NormalizeDirectInputAxis(g_diPadState.lY);
	if (g_padConnected) return NormalizeThumbAxis(g_padState.Gamepad.sThumbLY, kStickDeadZone);
	return 0.0f;
}

float GetPadRightStickX()
{
	if (g_diPadConnected && IsUsingDirectInput())
	{
        return GetDirectInputAxis(InputConfig::GetInstance().GetBindings().rightStickXAxis);
	}
	if (g_padConnected) return NormalizeThumbAxis(g_padState.Gamepad.sThumbRX, kRightStickDeadZone);
	return 0.0f;
}

float GetPadRightStickY()
{
	if (g_diPadConnected && IsUsingDirectInput())
	{
        const auto& bindings = InputConfig::GetInstance().GetBindings();
        return GetDirectInputAxis(bindings.rightStickYAxis) * (bindings.rightStickInvertY ? -1.0f : 1.0f);
	}
	if (g_padConnected) return NormalizeThumbAxis(g_padState.Gamepad.sThumbRY, kRightStickDeadZone);
	return 0.0f;
}

bool IsPadLeftShoulderTrigger() { return IsActionUITabPrevTrigger(); }
bool IsPadRightShoulderTrigger() { return IsActionUITabNextTrigger(); }

bool IsMouseLeftPress() { return g_mouseLeftDown; }
bool IsMouseLeftTrigger() { return g_mouseLeftDown && !g_oldMouseLeftDown; }
bool IsMouseLeftRelease() { return !g_mouseLeftDown && g_oldMouseLeftDown; }
bool IsMouseRightPress() { return g_mouseRightDown; }
bool IsMouseRightTrigger() { return g_mouseRightDown && !g_oldMouseRightDown; }
bool IsMouseRightRelease() { return !g_mouseRightDown && g_oldMouseRightDown; }
bool IsMouseMiddlePress() { return g_mouseMiddleDown; }
bool IsMouseMiddleTrigger() { return g_mouseMiddleDown && !g_oldMouseMiddleDown; }
bool IsMouseMiddleRelease() { return !g_mouseMiddleDown && g_oldMouseMiddleDown; }
POINT GetMousePosition() { return g_mousePos; }
POINT GetMouseDelta()
{
	POINT delta{};
	delta.x = g_mousePos.x - g_oldMousePos.x;
	delta.y = g_mousePos.y - g_oldMousePos.y;
	return delta;
}
float GetMouseWheelDelta() { return g_mouseWheelDelta; }
void PushMouseWheelDelta(float delta) { g_mouseWheelAccumulated += delta; }

void SetInputCaptureActive(bool active) { g_inputCaptureActive = active; }
bool IsInputCaptureActive() { return g_inputCaptureActive; }

float GetDirectInputAxis(int axis)
{
    if (!g_diPadConnected) return 0.0f;
    const LONG axes[] = { g_diPadState.lX, g_diPadState.lY, g_diPadState.lZ,
        g_diPadState.lRx, g_diPadState.lRy, g_diPadState.lRz };
    return axis >= 0 && axis < 6 ? NormalizeDirectInputAxis(axes[axis]) : 0.0f;
}

void GetActionMoveTriggers(bool& left, bool& right)
{
    left = right = false;
    if (g_inputGuardActive || g_inputCaptureActive) return;
    left = CheckActionTriggerInternal(GameAction::MoveLeft);
    right = CheckActionTriggerInternal(GameAction::MoveRight);
    if (g_padConnected)
    {
        left |= NormalizeThumbAxis(g_padState.Gamepad.sThumbLX, kStickDeadZone) < -kStickPressThreshold &&
            NormalizeThumbAxis(g_oldPadState.Gamepad.sThumbLX, kStickDeadZone) >= -kStickPressThreshold;
        right |= NormalizeThumbAxis(g_padState.Gamepad.sThumbLX, kStickDeadZone) > kStickPressThreshold &&
            NormalizeThumbAxis(g_oldPadState.Gamepad.sThumbLX, kStickDeadZone) <= kStickPressThreshold;
    }
    else if (g_diPadConnected)
    {
        left |= NormalizeDirectInputAxis(g_diPadState.lX) < -kStickPressThreshold && NormalizeDirectInputAxis(g_oldDiPadState.lX) >= -kStickPressThreshold;
        right |= NormalizeDirectInputAxis(g_diPadState.lX) > kStickPressThreshold && NormalizeDirectInputAxis(g_oldDiPadState.lX) <= kStickPressThreshold;
    }
}

void FeedImGuiGamepadInput()
{
    ImGuiIO& io = ImGui::GetIO();
    if (GImGui) GImGui->ConfigNavWindowingWithGamepad = false;
    const bool connected = IsPadConnected();
    if (connected) io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
    else io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;

    const bool enabled = connected && !g_inputCaptureActive && !g_inputGuardActive;
    const bool useDirectInput = enabled && IsUsingDirectInput();

    const auto button = [&](ImGuiKey key, WORD xi, int di) {
        bool down = false;
        if (useDirectInput && di >= 0) down = IsDirectInputButtonDown(g_diPadState, di);
        else if (enabled && g_padConnected && xi != 0) down = IsPadButtonDown(g_padState, xi);
        io.AddKeyEvent(key, down);
    };

    button(ImGuiKey_GamepadFaceDown,  XINPUT_GAMEPAD_A,              0); // 決定 (A / Button 1)
    button(ImGuiKey_GamepadFaceRight, XINPUT_GAMEPAD_B,              1); // キャンセル (B / Button 2)
    button(ImGuiKey_GamepadFaceLeft,  XINPUT_GAMEPAD_X,              2); // X / Button 3
    button(ImGuiKey_GamepadFaceUp,    XINPUT_GAMEPAD_Y,              3); // Y / Button 4
    button(ImGuiKey_GamepadL1,        XINPUT_GAMEPAD_LEFT_SHOULDER,  4); // LB / Button 5
    button(ImGuiKey_GamepadR1,        XINPUT_GAMEPAD_RIGHT_SHOULDER, 5); // RB / Button 6
    button(ImGuiKey_GamepadStart,     XINPUT_GAMEPAD_START,          9); // Start / Button 10
    button(ImGuiKey_GamepadBack,      XINPUT_GAMEPAD_BACK,           8); // Back / Button 9
    button(ImGuiKey_GamepadL3,        XINPUT_GAMEPAD_LEFT_THUMB,     10);
    button(ImGuiKey_GamepadR3,        XINPUT_GAMEPAD_RIGHT_THUMB,    11);

    const ImGuiKey directions[] = { ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadDpadRight };
    const WORD masks[] = { XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT };
    for (int i = 0; i < 4; ++i)
    {
        bool down = false;
        if (useDirectInput) down = IsDirectInputPovDirection(g_diPadState, i);
        else if (enabled && g_padConnected) down = IsPadButtonDown(g_padState, masks[i]);
        io.AddKeyEvent(directions[i], down);
    }

    const auto axis = [&](ImGuiKey negative, ImGuiKey positive, float value) {
        if (!enabled) value = 0.0f;
        io.AddKeyAnalogEvent(negative, value < -0.3f, std::max(0.0f, -value));
        io.AddKeyAnalogEvent(positive, value > 0.3f, std::max(0.0f, value));
    };
    axis(ImGuiKey_GamepadLStickLeft, ImGuiKey_GamepadLStickRight, GetPadLeftStickX());
    axis(ImGuiKey_GamepadLStickDown, ImGuiKey_GamepadLStickUp, GetPadLeftStickY());
    axis(ImGuiKey_GamepadRStickLeft, ImGuiKey_GamepadRStickRight, GetPadRightStickX());
    axis(ImGuiKey_GamepadRStickDown, ImGuiKey_GamepadRStickUp, GetPadRightStickY());

    if (enabled && g_padConnected && !useDirectInput)
    {
        io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, g_padState.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
            static_cast<float>(g_padState.Gamepad.bLeftTrigger) / 255.0f);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, g_padState.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD,
            static_cast<float>(g_padState.Gamepad.bRightTrigger) / 255.0f);
    }
    else
    {
        io.AddKeyAnalogEvent(ImGuiKey_GamepadL2, false, 0.0f);
        io.AddKeyAnalogEvent(ImGuiKey_GamepadR2, false, 0.0f);
    }
}

bool IsActionToggleLightTrigger()
{
	if (g_inputGuardActive || g_inputCaptureActive) return false;
	return CheckActionTriggerInternal(GameAction::ToggleLight);
}
