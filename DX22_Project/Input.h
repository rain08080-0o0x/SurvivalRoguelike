#ifndef __INPUT_H__
#define __INPUT_H__

#include <Windows.h>
#undef max
#undef min

HRESULT InitInput(HWND hWnd);
void UninitInput();
void UpdateInput();
float GetInputKeyboardMouseMs();
float GetInputXInputMs();
float GetInputDirectInputMs();

bool IsKeyPress(BYTE key);
bool IsKeyTrigger(BYTE key);
bool IsKeyRelease(BYTE key);
bool IsKeyRepeat(BYTE key);
bool IsRawKeyPress(BYTE key);
bool IsRawKeyTrigger(BYTE key);
bool IsMenuConfirmTrigger();
bool IsMenuBackTrigger();
bool IsPadConnected();
bool IsDirectInputConnected();
bool IsDirectInputButtonPressed(int buttonIndex);
LONG GetDirectInputAxisX();
LONG GetDirectInputAxisY();
DWORD GetDirectInputPov();
float GetPadLeftStickX();
float GetPadLeftStickY();
bool IsPadLeftShoulderTrigger();
bool IsPadRightShoulderTrigger();
bool IsMouseLeftPress();
bool IsMouseLeftTrigger();
bool IsMouseLeftRelease();
bool IsMouseRightPress();
bool IsMouseRightTrigger();
bool IsMouseRightRelease();
bool IsMouseMiddlePress();
bool IsMouseMiddleTrigger();
bool IsMouseMiddleRelease();
POINT GetMousePosition();
POINT GetMouseDelta();
float GetMouseWheelDelta();
void PushMouseWheelDelta(float delta);

#endif // __INPUT_H__
