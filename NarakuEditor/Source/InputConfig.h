#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#undef max
#undef min
#include <string>
#include <vector>

// 特殊入力コード（マウスボタン）
constexpr int MOUSE_BUTTON_LEFT = 0x101;
constexpr int MOUSE_BUTTON_RIGHT = 0x102;
constexpr int MOUSE_BUTTON_MIDDLE = 0x103;

// XInput仮想コード（トリガー用）
constexpr int XINPUT_VIRTUAL_LT = 0x1001;
constexpr int XINPUT_VIRTUAL_RT = 0x1002;

// DirectInput仮想コード（POVおよび軸用）
constexpr int DINPUT_BUTTON_BASE = 0;       // 0〜31: ボタン
constexpr int DINPUT_POV_UP = 100;
constexpr int DINPUT_POV_RIGHT = 101;
constexpr int DINPUT_POV_DOWN = 102;
constexpr int DINPUT_POV_LEFT = 103;
constexpr int DINPUT_AXIS_Z_POS = 110;     // トリガー正
constexpr int DINPUT_AXIS_Z_NEG = 111;     // トリガー負
constexpr int DINPUT_AXIS_RZ_POS = 112;
constexpr int DINPUT_AXIS_RZ_NEG = 113;

enum class GameAction
{
    MoveUp,
    MoveDown,
    MoveLeft,
    MoveRight,
    Attack,          // RB / 左クリック（マウス方向または向いている方向へ60度扇状攻撃）
    Interact,        // X / F（採取、会話、利用）
    Jump,            // A / Space
    DashStep,        // B / Shift（短押し: ステップ / 長押し: 走行）
    Skill,           // LT / Q（短押し: 探知 / 長押し: 瞑想）
    MenuMap,         // Y / E（統合メニュー 地図タブ）
    MenuInventory,   // I（統合メニュー 所持品タブ）
    MenuSettings,    // START / Esc（統合メニュー 設定タブ）
    CameraZoomHold,  // LB（押しながら右スティック上下でカメラ拡縮）
    ToggleLight,     // 携帯ライトの点灯／消灯（初期割り当てなし）
    Count
};

enum class ActiveInputDevice
{
    KeyboardMouse,
    Gamepad
};

struct InputBindings
{
    // キーボード＆マウス割り当て (仮想キーコード または MOUSE_BUTTON_*)
    int kbmBindings[static_cast<int>(GameAction::Count)]{};

    // XInput割り当て (XINPUT_GAMEPAD_* または XINPUT_VIRTUAL_*)
    int xinputBindings[static_cast<int>(GameAction::Count)]{};

    // DirectInput割り当て (DINPUT_*)：未割り当ては -1
    int dinputBindings[static_cast<int>(GameAction::Count)];
    int rightStickXAxis = 3; // 0:X, 1:Y, 2:Z, 3:Rx, 4:Ry, 5:Rz (標準右スティックはRx/Ry)
    int rightStickYAxis = 4;
    bool rightStickInvertY = true;

    InputBindings()
    {
        for (int i = 0; i < static_cast<int>(GameAction::Count); ++i)
        {
            kbmBindings[i] = 0;
            xinputBindings[i] = 0;
            dinputBindings[i] = -1;
        }
    }
};

class InputConfig
{
public:
    static InputConfig& GetInstance();

    void SetDefaults();
    static InputBindings GetDefaultBindings();
    bool LoadFromFile(const char* filepath = "InputConfig.ini");
    bool SaveToFile(const char* filepath = "InputConfig.ini") const;

    int GetBinding(GameAction action, int deviceType) const;
    void SetBinding(GameAction action, int deviceType, int code);

    const InputBindings& GetBindings() const { return m_bindings; }
    void SetBindings(const InputBindings& bindings) { m_bindings = bindings; }

    static const char* GetActionName(GameAction action);
    static const char* GetActionDescription(GameAction action);

    static std::string GetKbmKeyName(int code);
    static std::string GetXInputButtonName(int code);
    static std::string GetDInputButtonName(int code);

    // 現在の割り当てに応じた表示文字列（例: "[F]", "[X]", "[RB]" など）を取得
    std::string GetActionPromptText(GameAction action, ActiveInputDevice activeDevice, bool isDirectInput = false) const;

private:
    InputConfig();
    InputBindings m_bindings;
};
