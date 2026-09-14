#ifndef __INPUT_H__
#define __INPUT_H__

#include <Windows.h>
#include <string>
#include "InputConfig.h"

#undef max
#undef min

// --- 基本初期化・更新 ---
HRESULT InitInput(HWND hWnd);
void UninitInput();
void UpdateInput();

float GetInputKeyboardMouseMs();
float GetInputXInputMs();
float GetInputDirectInputMs();

// --- 接続・デバイス状態 ---
bool IsPadConnected();
bool IsXInputConnected();
bool IsDirectInputConnected();
ActiveInputDevice GetLastActiveDevice();
bool IsUsingDirectInput();

// --- 入力ガード（UIを閉じた直後の誤爆防止） ---
void SetInputGuardActive(bool active);
bool IsInputGuarded();
void SetInputCaptureActive(bool active);
bool IsInputCaptureActive();
void FeedImGuiGamepadInput();
float GetDirectInputAxis(int axis);
void GetActionMoveTriggers(bool& left, bool& right);

// --- ゲームアクション単位の入力取得 ---
void GetActionMoveVector(float& outX, float& outY);
bool IsActionAttackTrigger();
bool IsActionAttackPress();
bool IsActionInteractTrigger();
bool IsActionJumpTrigger();

bool IsActionDashStepPress();
bool IsActionDashStepTrigger();
bool IsActionDashStepRelease();

bool IsActionSkillPress();
bool IsActionSkillTrigger();
bool IsActionSkillRelease();

bool IsActionMenuMapTrigger();
bool IsActionMenuInventoryTrigger();
bool IsActionMenuSettingsTrigger();
bool IsActionToggleLightTrigger();

bool IsActionCameraZoomHoldPress();
void GetActionCameraRotation(float& outYaw, float& outPitch);
float GetActionCameraZoomDelta();

// --- UI操作用入力 ---
bool IsActionUIConfirmTrigger();
bool IsActionUIBackTrigger();
bool IsActionUITabPrevTrigger();
bool IsActionUITabNextTrigger();
bool IsActionUINavUpTrigger();
bool IsActionUINavDownTrigger();
bool IsActionUINavLeftTrigger();
bool IsActionUINavRightTrigger();

// --- 地図操作用入力 ---
void GetActionMapScroll(float& outX, float& outY);
float GetActionMapZoom();
bool IsActionMapPinTrigger();
bool IsActionMapFocusPlayerTrigger();

// --- リバインド（キーコンフィグ用入力検知） ---
bool GetAnyKbmTrigger(int& outCode);
bool GetAnyXInputTrigger(int& outCode);
bool GetAnyDInputTrigger(int& outCode);

// --- 従来の個別API（互換性維持） ---
bool IsKeyPress(BYTE key);
bool IsKeyTrigger(BYTE key);
bool IsKeyRelease(BYTE key);
bool IsKeyRepeat(BYTE key);
bool IsRawKeyPress(BYTE key);
bool IsRawKeyTrigger(BYTE key);
bool IsMenuConfirmTrigger();
bool IsMenuBackTrigger();

bool IsDirectInputButtonPressed(int buttonIndex);
LONG GetDirectInputAxisX();
LONG GetDirectInputAxisY();
DWORD GetDirectInputPov();

float GetPadLeftStickX();
float GetPadLeftStickY();
float GetPadRightStickX();
float GetPadRightStickY();
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
