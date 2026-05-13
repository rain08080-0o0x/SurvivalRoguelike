#include "Input.h"
#include "Transfer.h"
#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif
#include <dinput.h>
#include <Xinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "Xinput.lib")

// Global state
BYTE g_keyTable[256]{};
BYTE g_oldTable[256]{};
XINPUT_STATE g_padState{};
XINPUT_STATE g_oldPadState{};
bool g_padConnected = false;
DWORD g_xinputUserIndex = XUSER_MAX_COUNT;
LPDIRECTINPUT8 g_pDirectInput = nullptr;
LPDIRECTINPUTDEVICE8 g_pDirectInputPad = nullptr;
DIJOYSTATE2 g_diPadState{};
DIJOYSTATE2 g_oldDiPadState{};
bool g_diPadConnected = false;
HWND g_inputWindow = nullptr;
POINT g_mousePos{};
POINT g_oldMousePos{};
bool g_mouseLeftDown = false;
bool g_oldMouseLeftDown = false;
bool g_mouseRightDown = false;
bool g_oldMouseRightDown = false;
bool g_mouseMiddleDown = false;
bool g_oldMouseMiddleDown = false;
float g_inputKeyboardMouseMs = 0.0f;
float g_inputXInputMs = 0.0f;
float g_inputDirectInputMs = 0.0f;

namespace
{
	const SHORT kStickDeadZone = XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE;
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

	BYTE ResolveConfiguredKeyboardKey(BYTE logicalKey)
	{
		const Transfer::InputConfig& input = Transfer::GetInstance().input;
		switch (logicalKey)
		{
		case 'W':
			return static_cast<BYTE>(input.moveUp);
		case 'S':
			return static_cast<BYTE>(input.moveDown);
		case 'A':
			return static_cast<BYTE>(input.moveLeft);
		case 'D':
			return static_cast<BYTE>(input.moveRight);
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			return static_cast<BYTE>(input.dash);
		case 'F':
			return static_cast<BYTE>(input.attack);
		case 'O':
			return static_cast<BYTE>(input.skill1);
		case 'P':
			return static_cast<BYTE>(input.skill2);
		case 'R':
			return static_cast<BYTE>(input.reroll);
		default:
			return logicalKey;
		}
	}

	WORD GetConfiguredXInputButtonForLogicalKey(BYTE logicalKey)
	{
		const Transfer::InputConfig& input = Transfer::GetInstance().input;
		switch (logicalKey)
		{
		case VK_RETURN:
			return static_cast<WORD>(input.xinputConfirm);
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			return static_cast<WORD>(input.xinputDash);
		case 'F':
			return static_cast<WORD>(input.xinputAttack);
		case 'Q':
		case 'O':
			return static_cast<WORD>(input.xinputSkill1);
		case 'E':
		case 'P':
			return static_cast<WORD>(input.xinputSkill2);
		case 'R':
			return static_cast<WORD>(input.xinputReroll);
		default:
			return 0;
		}
	}

	int GetConfiguredDirectInputButtonForLogicalKey(BYTE logicalKey)
	{
		const Transfer::InputConfig& input = Transfer::GetInstance().input;
		switch (logicalKey)
		{
		case VK_RETURN:
			return input.directInputConfirm;
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			return input.directInputDash;
		case 'F':
			return input.directInputAttack;
		case 'Q':
		case 'O':
			return input.directInputSkill1;
		case 'E':
		case 'P':
			return input.directInputSkill2;
		case 'R':
			return input.directInputReroll;
		default:
			return -1;
		}
	}

	bool IsPadButtonDown(const XINPUT_STATE& state, WORD button)
	{
		return (state.Gamepad.wButtons & button) != 0;
	}

	bool IsDirectInputButtonDown(const DIJOYSTATE2& state, int buttonIndex)
	{
		if (buttonIndex < 0 || buttonIndex >= 128)
		{
			return false;
		}
		return (state.rgbButtons[buttonIndex] & 0x80) != 0;
	}

	bool IsConfiguredXInputButtonDown(const XINPUT_STATE& state, BYTE logicalKey)
	{
		const WORD button = GetConfiguredXInputButtonForLogicalKey(logicalKey);
		return button != 0 && IsPadButtonDown(state, button);
	}

	bool IsConfiguredDirectInputButtonDown(const DIJOYSTATE2& state, BYTE logicalKey)
	{
		const int buttonIndex = GetConfiguredDirectInputButtonForLogicalKey(logicalKey);
		return buttonIndex >= 0 && IsDirectInputButtonDown(state, buttonIndex);
	}

	bool IsDirectInputPovDirection(const DIJOYSTATE2& state, int direction)
	{
		const DWORD pov = state.rgdwPOV[0];
		if (LOWORD(pov) == 0xFFFF)
		{
			return false;
		}

		switch (direction)
		{
		case 0: return (pov == 0 || pov == 4500 || pov == 31500);
		case 1: return (pov == 18000 || pov == 13500 || pov == 22500);
		case 2: return (pov == 27000 || pov == 22500 || pov == 31500);
		case 3: return (pov == 9000 || pov == 4500 || pov == 13500);
		default: return false;
		}
	}

	HWND GetInputWindow()
	{
		if (g_inputWindow && IsWindow(g_inputWindow))
		{
			return g_inputWindow;
		}

		HWND hWnd = GetActiveWindow();
		if (!hWnd) hWnd = GetForegroundWindow();
		if (!hWnd) hWnd = GetFocus();
		if (hWnd)
		{
			g_inputWindow = hWnd;
		}
		return hWnd;
	}

	POINT QueryMousePosition()
	{
		POINT pt{};
		::GetCursorPos(&pt);
		HWND hWnd = GetInputWindow();
		if (hWnd)
		{
			::ScreenToClient(hWnd, &pt);
		}
		return pt;
	}

	bool IsMappedXInputPress(BYTE key, const XINPUT_STATE& state)
	{
		const float lx = NormalizeThumbAxis(state.Gamepad.sThumbLX, kStickDeadZone);
		const float ly = NormalizeThumbAxis(state.Gamepad.sThumbLY, kStickDeadZone);

		switch (key)
		{
		case 'W':
		case VK_UP:
			return IsPadButtonDown(state, XINPUT_GAMEPAD_DPAD_UP) || (ly > kStickPressThreshold);
		case 'S':
		case VK_DOWN:
			return IsPadButtonDown(state, XINPUT_GAMEPAD_DPAD_DOWN) || (ly < -kStickPressThreshold);
		case 'A':
		case VK_LEFT:
			return IsPadButtonDown(state, XINPUT_GAMEPAD_DPAD_LEFT) || (lx < -kStickPressThreshold);
		case 'D':
		case VK_RIGHT:
			return IsPadButtonDown(state, XINPUT_GAMEPAD_DPAD_RIGHT) || (lx > kStickPressThreshold);
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_RETURN:
		case 'F':
		case 'Q':
		case 'O':
		case 'E':
		case 'P':
		case 'R':
			return IsConfiguredXInputButtonDown(state, key);
		case VK_ESCAPE:
			return IsPadButtonDown(state, XINPUT_GAMEPAD_START);
		case VK_TAB:
			return IsPadButtonDown(state, XINPUT_GAMEPAD_BACK);
		case 'T':
			return IsPadButtonDown(state, XINPUT_GAMEPAD_BACK);
		default:
			return false;
		}
	}

	bool IsMappedDirectInputPress(BYTE key, const DIJOYSTATE2& state)
	{
		const float lx = NormalizeDirectInputAxis(state.lX);
		const float ly = NormalizeDirectInputAxis(state.lY);

		switch (key)
		{
		case 'W':
		case VK_UP:
			return IsDirectInputPovDirection(state, 0) || (ly < -kStickPressThreshold);
		case 'S':
		case VK_DOWN:
			return IsDirectInputPovDirection(state, 1) || (ly > kStickPressThreshold);
		case 'A':
		case VK_LEFT:
			return IsDirectInputPovDirection(state, 2) || (lx < -kStickPressThreshold);
		case 'D':
		case VK_RIGHT:
			return IsDirectInputPovDirection(state, 3) || (lx > kStickPressThreshold);
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
		case VK_RETURN:
		case 'F':
		case 'Q':
		case 'O':
		case 'E':
		case 'P':
		case 'R':
			return IsConfiguredDirectInputButtonDown(state, key);
		case VK_ESCAPE:
			return IsDirectInputButtonDown(state, 9);
		case VK_TAB:
			return IsDirectInputButtonDown(state, 6);
		case 'T':
			return IsDirectInputButtonDown(state, 6);
		default:
			return false;
		}
	}

	bool SetDirectInputAxisRange(LPDIRECTINPUTDEVICE8 device, DWORD objectOffset)
	{
		if (!device)
		{
			return false;
		}

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
		if (g_pDirectInput)
		{
			return true;
		}

		return SUCCEEDED(DirectInput8Create(
			GetModuleHandle(nullptr),
			DIRECTINPUT_VERSION,
			IID_IDirectInput8,
			reinterpret_cast<void**>(&g_pDirectInput),
			nullptr)) && g_pDirectInput;
	}

	BOOL CALLBACK EnumGameControllerCallback(const DIDEVICEINSTANCE* instance, VOID* context)
	{
		if (!g_pDirectInput || g_pDirectInputPad)
		{
			return DIENUM_STOP;
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
		device->Acquire();

		g_pDirectInputPad = device;
		return DIENUM_STOP;
	}

	bool TryEnumerateDirectInputPad()
	{
		if (g_pDirectInputPad)
		{
			return true;
		}

		const ULONGLONG nowTick = GetTickCount64();
		if (nowTick < g_nextDirectInputEnumTick)
		{
			return false;
		}

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
		if (!g_pDirectInputPad || !outState)
		{
			return E_FAIL;
		}

		HRESULT hr = g_pDirectInputPad->Poll();
		if (FAILED(hr))
		{
			hr = g_pDirectInputPad->Acquire();
			while (hr == DIERR_INPUTLOST)
			{
				hr = g_pDirectInputPad->Acquire();
			}
			if (FAILED(hr))
			{
				return hr;
			}
			hr = g_pDirectInputPad->Poll();
			if (FAILED(hr))
			{
				return hr;
			}
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
			if (!TryEnumerateDirectInputPad())
			{
				return;
			}

			hr = ReadDirectInputPadState(&state);
		}

		if (SUCCEEDED(hr))
		{
			g_diPadConnected = true;
			g_diPadState = state;
			if (!hadConnection)
			{
				g_oldDiPadState = state;
			}
		}
		else
		{
			ReleaseDirectInputPad();
		}
	}

	float SelectStrongerAxis(float xinputValue, float directInputValue)
	{
		const float absXInput = (xinputValue < 0.0f) ? -xinputValue : xinputValue;
		const float absDirectInput = (directInputValue < 0.0f) ? -directInputValue : directInputValue;
		return (absDirectInput > absXInput) ? directInputValue : xinputValue;
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
			if (XInputGetState(userIndex, &state) != ERROR_SUCCESS)
			{
				return false;
			}

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
				if (tryGetState(userIndex))
				{
					break;
				}
			}
		}

		if (resolvedIndex < XUSER_MAX_COUNT)
		{
			g_padConnected = true;
			g_xinputUserIndex = resolvedIndex;
			g_padState = resolvedState;
			if (!hadConnection || previousIndex != resolvedIndex)
			{
				g_oldPadState = resolvedState;
			}
		}
		else
		{
			g_padConnected = false;
			g_xinputUserIndex = XUSER_MAX_COUNT;
			ZeroMemory(&g_padState, sizeof(g_padState));
			if (!hadConnection)
			{
				ZeroMemory(&g_oldPadState, sizeof(g_oldPadState));
			}
		}
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
	g_inputKeyboardMouseMs = static_cast<float>(QueryPerfMs() - sectionStart);

	sectionStart = QueryPerfMs();
	g_oldPadState = g_padState;
	UpdateXInputState();
	g_inputXInputMs = static_cast<float>(QueryPerfMs() - sectionStart);

	sectionStart = QueryPerfMs();
	g_oldDiPadState = g_diPadState;
	UpdateDirectInputState();
	g_inputDirectInputMs = static_cast<float>(QueryPerfMs() - sectionStart);
}

float GetInputKeyboardMouseMs()
{
	return g_inputKeyboardMouseMs;
}

float GetInputXInputMs()
{
	return g_inputXInputMs;
}

float GetInputDirectInputMs()
{
	return g_inputDirectInputMs;
}

bool IsKeyPress(BYTE key)
{
	const bool keyPress = IsRawKeyboardPress(ResolveConfiguredKeyboardKey(key));
	const bool xinputPress = g_padConnected && IsMappedXInputPress(key, g_padState);
	const bool directInputPress = g_diPadConnected && IsMappedDirectInputPress(key, g_diPadState);
	const bool padPress = xinputPress || directInputPress;
	return keyPress || padPress;
}

bool IsKeyTrigger(BYTE key)
{
	const bool keyTrigger = IsRawKeyboardTrigger(ResolveConfiguredKeyboardKey(key));
	const bool xinputNow = g_padConnected && IsMappedXInputPress(key, g_padState);
	const bool xinputOld = IsMappedXInputPress(key, g_oldPadState);
	const bool directInputNow = g_diPadConnected && IsMappedDirectInputPress(key, g_diPadState);
	const bool directInputOld = IsMappedDirectInputPress(key, g_oldDiPadState);
	const bool padTrigger = (xinputNow && !xinputOld) || (directInputNow && !directInputOld);
	return keyTrigger || padTrigger;
}

bool IsKeyRelease(BYTE key)
{
	const bool keyRelease = IsRawKeyboardRelease(ResolveConfiguredKeyboardKey(key));
	const bool xinputNow = g_padConnected && IsMappedXInputPress(key, g_padState);
	const bool xinputOld = IsMappedXInputPress(key, g_oldPadState);
	const bool directInputNow = g_diPadConnected && IsMappedDirectInputPress(key, g_diPadState);
	const bool directInputOld = IsMappedDirectInputPress(key, g_oldDiPadState);
	const bool padRelease = (!xinputNow && xinputOld) || (!directInputNow && directInputOld);
	return keyRelease || padRelease;
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
	const bool keyboardTrigger =
		IsRawKeyboardTrigger(VK_RETURN) ||
		IsRawKeyboardTrigger(VK_SPACE) ||
		IsRawKeyboardTrigger('F');
	const bool xinputNow = g_padConnected && IsConfiguredXInputButtonDown(g_padState, VK_RETURN);
	const bool xinputOld = IsConfiguredXInputButtonDown(g_oldPadState, VK_RETURN);
	const bool directInputNow = g_diPadConnected && IsConfiguredDirectInputButtonDown(g_diPadState, VK_RETURN);
	const bool directInputOld = IsConfiguredDirectInputButtonDown(g_oldDiPadState, VK_RETURN);
	const bool padTrigger = (xinputNow && !xinputOld) || (directInputNow && !directInputOld);
	return keyboardTrigger || padTrigger;
}

bool IsMenuBackTrigger()
{
	return IsKeyTrigger(VK_ESCAPE);
}

bool IsPadConnected()
{
	return g_padConnected || g_diPadConnected;
}

bool IsDirectInputConnected()
{
	return g_diPadConnected;
}

bool IsDirectInputButtonPressed(int buttonIndex)
{
	return g_diPadConnected && IsDirectInputButtonDown(g_diPadState, buttonIndex);
}

LONG GetDirectInputAxisX()
{
	return g_diPadConnected ? g_diPadState.lX : 0;
}

LONG GetDirectInputAxisY()
{
	return g_diPadConnected ? g_diPadState.lY : 0;
}

DWORD GetDirectInputPov()
{
	return g_diPadConnected ? g_diPadState.rgdwPOV[0] : 0xFFFFFFFFu;
}

float GetPadLeftStickX()
{
	const float xinputValue = g_padConnected ? NormalizeThumbAxis(g_padState.Gamepad.sThumbLX, kStickDeadZone) : 0.0f;
	const float directInputValue = g_diPadConnected ? NormalizeDirectInputAxis(g_diPadState.lX) : 0.0f;
	return SelectStrongerAxis(xinputValue, directInputValue);
}

float GetPadLeftStickY()
{
	const float xinputValue = g_padConnected ? NormalizeThumbAxis(g_padState.Gamepad.sThumbLY, kStickDeadZone) : 0.0f;
	const float directInputValue = g_diPadConnected ? -NormalizeDirectInputAxis(g_diPadState.lY) : 0.0f;
	return SelectStrongerAxis(xinputValue, directInputValue);
}

bool IsPadLeftShoulderTrigger()
{
	const Transfer::InputConfig& input = Transfer::GetInstance().input;
	const bool xinputNow = g_padConnected && IsPadButtonDown(g_padState, static_cast<WORD>(input.xinputTabPrev));
	const bool xinputOld = IsPadButtonDown(g_oldPadState, static_cast<WORD>(input.xinputTabPrev));
	const bool directInputNow = g_diPadConnected && IsDirectInputButtonDown(g_diPadState, input.directInputTabPrev);
	const bool directInputOld = IsDirectInputButtonDown(g_oldDiPadState, input.directInputTabPrev);
	return (xinputNow && !xinputOld) || (directInputNow && !directInputOld);
}

bool IsPadRightShoulderTrigger()
{
	const Transfer::InputConfig& input = Transfer::GetInstance().input;
	const bool xinputNow = g_padConnected && IsPadButtonDown(g_padState, static_cast<WORD>(input.xinputTabNext));
	const bool xinputOld = IsPadButtonDown(g_oldPadState, static_cast<WORD>(input.xinputTabNext));
	const bool directInputNow = g_diPadConnected && IsDirectInputButtonDown(g_diPadState, input.directInputTabNext);
	const bool directInputOld = IsDirectInputButtonDown(g_oldDiPadState, input.directInputTabNext);
	return (xinputNow && !xinputOld) || (directInputNow && !directInputOld);
}

bool IsMouseLeftPress()
{
	return g_mouseLeftDown;
}

bool IsMouseLeftTrigger()
{
	return g_mouseLeftDown && !g_oldMouseLeftDown;
}

bool IsMouseLeftRelease()
{
	return !g_mouseLeftDown && g_oldMouseLeftDown;
}

bool IsMouseRightPress()
{
	return g_mouseRightDown;
}

bool IsMouseRightTrigger()
{
	return g_mouseRightDown && !g_oldMouseRightDown;
}

bool IsMouseRightRelease()
{
	return !g_mouseRightDown && g_oldMouseRightDown;
}

bool IsMouseMiddlePress()
{
	return g_mouseMiddleDown;
}

bool IsMouseMiddleTrigger()
{
	return g_mouseMiddleDown && !g_oldMouseMiddleDown;
}

bool IsMouseMiddleRelease()
{
	return !g_mouseMiddleDown && g_oldMouseMiddleDown;
}

POINT GetMousePosition()
{
	return g_mousePos;
}

POINT GetMouseDelta()
{
	POINT delta{};
	delta.x = g_mousePos.x - g_oldMousePos.x;
	delta.y = g_mousePos.y - g_oldMousePos.y;
	return delta;
}
