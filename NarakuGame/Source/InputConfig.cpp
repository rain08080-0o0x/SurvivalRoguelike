#include "InputConfig.h"
#include <fstream>
#include <sstream>
#include <Xinput.h>
#include <unordered_map>

InputConfig& InputConfig::GetInstance()
{
    static InputConfig instance;
    return instance;
}

InputConfig::InputConfig()
{
    SetDefaults();
    LoadFromFile();
}

InputBindings InputConfig::GetDefaultBindings()
{
    InputBindings b{};

    // --- キーボード＆マウス初期値 ---
    b.kbmBindings[static_cast<int>(GameAction::MoveUp)] = 'W';
    b.kbmBindings[static_cast<int>(GameAction::MoveDown)] = 'S';
    b.kbmBindings[static_cast<int>(GameAction::MoveLeft)] = 'A';
    b.kbmBindings[static_cast<int>(GameAction::MoveRight)] = 'D';
    b.kbmBindings[static_cast<int>(GameAction::Attack)] = MOUSE_BUTTON_LEFT;
    b.kbmBindings[static_cast<int>(GameAction::Interact)] = 'F';
    b.kbmBindings[static_cast<int>(GameAction::Jump)] = VK_SPACE;
    b.kbmBindings[static_cast<int>(GameAction::DashStep)] = VK_SHIFT;
    b.kbmBindings[static_cast<int>(GameAction::Skill)] = 'Q';
    b.kbmBindings[static_cast<int>(GameAction::MenuMap)] = 'E';
    b.kbmBindings[static_cast<int>(GameAction::MenuInventory)] = 'I';
    b.kbmBindings[static_cast<int>(GameAction::MenuSettings)] = VK_ESCAPE;
    b.kbmBindings[static_cast<int>(GameAction::CameraZoomHold)] = 0;
    b.kbmBindings[static_cast<int>(GameAction::ToggleLight)] = 0;

    // --- XInput初期値 ---
    b.xinputBindings[static_cast<int>(GameAction::MoveUp)] = XINPUT_GAMEPAD_DPAD_UP;
    b.xinputBindings[static_cast<int>(GameAction::MoveDown)] = XINPUT_GAMEPAD_DPAD_DOWN;
    b.xinputBindings[static_cast<int>(GameAction::MoveLeft)] = XINPUT_GAMEPAD_DPAD_LEFT;
    b.xinputBindings[static_cast<int>(GameAction::MoveRight)] = XINPUT_GAMEPAD_DPAD_RIGHT;
    b.xinputBindings[static_cast<int>(GameAction::Attack)] = XINPUT_GAMEPAD_RIGHT_SHOULDER;
    b.xinputBindings[static_cast<int>(GameAction::Interact)] = XINPUT_GAMEPAD_X;
    b.xinputBindings[static_cast<int>(GameAction::Jump)] = XINPUT_GAMEPAD_A;
    b.xinputBindings[static_cast<int>(GameAction::DashStep)] = XINPUT_GAMEPAD_B;
    b.xinputBindings[static_cast<int>(GameAction::Skill)] = XINPUT_VIRTUAL_LT;
    b.xinputBindings[static_cast<int>(GameAction::MenuMap)] = XINPUT_GAMEPAD_Y;
    b.xinputBindings[static_cast<int>(GameAction::MenuInventory)] = 0;
    b.xinputBindings[static_cast<int>(GameAction::MenuSettings)] = XINPUT_GAMEPAD_START;
    b.xinputBindings[static_cast<int>(GameAction::CameraZoomHold)] = XINPUT_GAMEPAD_LEFT_SHOULDER;
    b.xinputBindings[static_cast<int>(GameAction::ToggleLight)] = 0;

    // --- DirectInput初期値 ---
    b.dinputBindings[static_cast<int>(GameAction::MoveUp)] = DINPUT_POV_UP;
    b.dinputBindings[static_cast<int>(GameAction::MoveDown)] = DINPUT_POV_DOWN;
    b.dinputBindings[static_cast<int>(GameAction::MoveLeft)] = DINPUT_POV_LEFT;
    b.dinputBindings[static_cast<int>(GameAction::MoveRight)] = DINPUT_POV_RIGHT;
    b.dinputBindings[static_cast<int>(GameAction::Attack)] = 5;
    b.dinputBindings[static_cast<int>(GameAction::Interact)] = 2;
    b.dinputBindings[static_cast<int>(GameAction::Jump)] = 0;
    b.dinputBindings[static_cast<int>(GameAction::DashStep)] = 1;
    b.dinputBindings[static_cast<int>(GameAction::Skill)] = 6;
    b.dinputBindings[static_cast<int>(GameAction::MenuMap)] = 3;
    b.dinputBindings[static_cast<int>(GameAction::MenuInventory)] = -1;
    b.dinputBindings[static_cast<int>(GameAction::MenuSettings)] = 9;
    b.dinputBindings[static_cast<int>(GameAction::CameraZoomHold)] = 4;
    b.dinputBindings[static_cast<int>(GameAction::ToggleLight)] = -1;

    return b;
}

void InputConfig::SetDefaults()
{
    m_bindings = GetDefaultBindings();
}

int InputConfig::GetBinding(GameAction action, int deviceType) const
{
    const int idx = static_cast<int>(action);
    if (idx < 0 || idx >= static_cast<int>(GameAction::Count)) return (deviceType == 2 ? -1 : 0);

    switch (deviceType)
    {
    case 0: return m_bindings.kbmBindings[idx];
    case 1: return m_bindings.xinputBindings[idx];
    case 2: return m_bindings.dinputBindings[idx];
    default: return (deviceType == 2 ? -1 : 0);
    }
}

void InputConfig::SetBinding(GameAction action, int deviceType, int code)
{
    const int idx = static_cast<int>(action);
    if (idx < 0 || idx >= static_cast<int>(GameAction::Count)) return;

    switch (deviceType)
    {
    case 0: m_bindings.kbmBindings[idx] = code; break;
    case 1: m_bindings.xinputBindings[idx] = code; break;
    case 2: m_bindings.dinputBindings[idx] = code; break;
    }
}

bool InputConfig::LoadFromFile(const char* filepath)
{
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;

        const size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        const std::string key = line.substr(0, eqPos);
        const std::string valStr = line.substr(eqPos + 1);
        int value = 0;
        try {
            value = std::stoi(valStr, nullptr, 0); // 16進数(0x)も対応
        } catch (...) {
            continue;
        }

        if (key == "DIN_RightStickX") { if (value >= 0 && value < 6) m_bindings.rightStickXAxis = value; continue; }
        if (key == "DIN_RightStickY") { if (value >= 0 && value < 6) m_bindings.rightStickYAxis = value; continue; }
        if (key == "DIN_InvertY") { m_bindings.rightStickInvertY = value != 0; continue; }
        try {
        // key format: "KBM_ActionIdx", "XIN_ActionIdx", "DIN_ActionIdx"
        if (key.rfind("KBM_", 0) == 0)
        {
            int act = std::stoi(key.substr(4));
            if (act >= 0 && act < static_cast<int>(GameAction::Count))
                m_bindings.kbmBindings[act] = value;
        }
        else if (key.rfind("XIN_", 0) == 0)
        {
            int act = std::stoi(key.substr(4));
            if (act >= 0 && act < static_cast<int>(GameAction::Count))
                m_bindings.xinputBindings[act] = value;
        }
        else if (key.rfind("DIN_", 0) == 0)
        {
            int act = std::stoi(key.substr(4));
            if (act >= 0 && act < static_cast<int>(GameAction::Count))
                m_bindings.dinputBindings[act] = value;
        }
        } catch (const std::exception&) { continue; }
    }
    return true;
}

bool InputConfig::SaveToFile(const char* filepath) const
{
    std::ofstream file(filepath);
    if (!file.is_open()) return false;

    file << "# Naraku Game Input Configuration\n\n";

    file << "[KeyboardMouse]\n";
    for (int i = 0; i < static_cast<int>(GameAction::Count); ++i)
    {
        file << "KBM_" << i << "=" << m_bindings.kbmBindings[i]
             << " # " << GetActionName(static_cast<GameAction>(i)) << "\n";
    }

    file << "\n[XInput]\n";
    for (int i = 0; i < static_cast<int>(GameAction::Count); ++i)
    {
        file << "XIN_" << i << "=" << m_bindings.xinputBindings[i]
             << " # " << GetActionName(static_cast<GameAction>(i)) << "\n";
    }

    file << "\n[DirectInput]\n";
    for (int i = 0; i < static_cast<int>(GameAction::Count); ++i)
    {
        file << "DIN_" << i << "=" << m_bindings.dinputBindings[i]
             << " # " << GetActionName(static_cast<GameAction>(i)) << "\n";
    }

    file << "DIN_RightStickX=" << m_bindings.rightStickXAxis << "\n";
    file << "DIN_RightStickY=" << m_bindings.rightStickYAxis << "\n";
    file << "DIN_InvertY=" << m_bindings.rightStickInvertY << "\n";
    file.flush();
    return file.good();
}

const char* InputConfig::GetActionName(GameAction action)
{
    switch (action)
    {
    case GameAction::MoveUp:         return u8"上移動";
    case GameAction::MoveDown:       return u8"下移動";
    case GameAction::MoveLeft:       return u8"左移動";
    case GameAction::MoveRight:      return u8"右移動";
    case GameAction::Attack:         return u8"攻撃";
    case GameAction::Interact:       return u8"調べる / 利用";
    case GameAction::Jump:           return u8"ジャンプ";
    case GameAction::DashStep:       return u8"ダッシュ / ステップ";
    case GameAction::Skill:          return u8"精神力スキル";
    case GameAction::MenuMap:        return u8"地図を開く";
    case GameAction::MenuInventory:  return u8"所持品を開く";
    case GameAction::MenuSettings:   return u8"設定 / メニュー";
    case GameAction::CameraZoomHold: return u8"カメラ拡縮ホールド";
    case GameAction::ToggleLight:    return u8"携帯ライト 点灯 / 消灯";
    default:                         return u8"不明";
    }
}

const char* InputConfig::GetActionDescription(GameAction action)
{
    switch (action)
    {
    case GameAction::Attack:         return u8"前方60度範囲へ攻撃（マウス方向または向いている方向）";
    case GameAction::Interact:       return u8"採取ポイントの調査、施設やゲートの利用、会話";
    case GameAction::Jump:           return u8"足場を飛び越える";
    case GameAction::DashStep:       return u8"短押しでステップ回避、長押しでスタミナ消費ダッシュ";
    case GameAction::Skill:          return u8"短押しでスキル1（探知）、長押しでスキル2（瞑想）";
    case GameAction::MenuMap:        return u8"統合メニューの地図タブを開く";
    case GameAction::MenuInventory:  return u8"統合メニューの所持品タブを開く";
    case GameAction::MenuSettings:   return u8"統合メニューの設定タブを開く";
    case GameAction::CameraZoomHold: return u8"押しながら右スティック前後でカメラ距離を調整";
    case GameAction::ToggleLight:    return u8"所持中の携帯ライトを点灯または消灯する";
    default:                         return "";
    }
}

std::string InputConfig::GetKbmKeyName(int code)
{
    if (code == 0) return u8"なし";
    if (code == MOUSE_BUTTON_LEFT) return u8"左クリック";
    if (code == MOUSE_BUTTON_RIGHT) return u8"右クリック";
    if (code == MOUSE_BUTTON_MIDDLE) return u8"中クリック";
    if (code == VK_SPACE) return "Space";
    if (code == VK_SHIFT || code == VK_LSHIFT || code == VK_RSHIFT) return "Shift";
    if (code == VK_ESCAPE) return "Esc";
    if (code == VK_RETURN) return "Enter";
    if (code == VK_TAB) return "Tab";
    if (code == VK_CONTROL || code == VK_LCONTROL || code == VK_RCONTROL) return "Ctrl";
    if (code == VK_UP) return u8"↑ (Up)";
    if (code == VK_DOWN) return u8"↓ (Down)";
    if (code == VK_LEFT) return u8"← (Left)";
    if (code == VK_RIGHT) return u8"→ (Right)";

    if (code >= 'A' && code <= 'Z')
    {
        return std::string(1, static_cast<char>(code));
    }
    if (code >= '0' && code <= '9')
    {
        return std::string(1, static_cast<char>(code));
    }

    char buf[32];
    snprintf(buf, sizeof(buf), "Key(0x%X)", code);
    return buf;
}

std::string InputConfig::GetXInputButtonName(int code)
{
    if (code == 0) return u8"なし";
    if (code == XINPUT_GAMEPAD_A) return "A";
    if (code == XINPUT_GAMEPAD_B) return "B";
    if (code == XINPUT_GAMEPAD_X) return "X";
    if (code == XINPUT_GAMEPAD_Y) return "Y";
    if (code == XINPUT_GAMEPAD_LEFT_SHOULDER) return "LB";
    if (code == XINPUT_GAMEPAD_RIGHT_SHOULDER) return "RB";
    if (code == XINPUT_VIRTUAL_LT) return "LT";
    if (code == XINPUT_VIRTUAL_RT) return "RT";
    if (code == XINPUT_GAMEPAD_START) return "START";
    if (code == XINPUT_GAMEPAD_BACK) return "BACK";
    if (code == XINPUT_GAMEPAD_LEFT_THUMB) return "L-Thumb";
    if (code == XINPUT_GAMEPAD_RIGHT_THUMB) return "R-Thumb";
    if (code == XINPUT_GAMEPAD_DPAD_UP) return u8"十字キー上";
    if (code == XINPUT_GAMEPAD_DPAD_DOWN) return u8"十字キー下";
    if (code == XINPUT_GAMEPAD_DPAD_LEFT) return u8"十字キー左";
    if (code == XINPUT_GAMEPAD_DPAD_RIGHT) return u8"十字キー右";

    char buf[32];
    snprintf(buf, sizeof(buf), "Btn(0x%X)", code);
    return buf;
}

std::string InputConfig::GetDInputButtonName(int code)
{
    if (code < 0) return u8"なし";
    if (code == DINPUT_POV_UP) return u8"POV 上";
    if (code == DINPUT_POV_DOWN) return u8"POV 下";
    if (code == DINPUT_POV_LEFT) return u8"POV 左";
    if (code == DINPUT_POV_RIGHT) return u8"POV 右";
    if (code == DINPUT_AXIS_Z_POS) return "Z Axis +";
    if (code == DINPUT_AXIS_Z_NEG) return "Z Axis -";
    if (code == DINPUT_AXIS_RZ_POS) return "RZ Axis +";
    if (code == DINPUT_AXIS_RZ_NEG) return "RZ Axis -";

    char buf[32];
    snprintf(buf, sizeof(buf), "Button %d", code + 1);
    return buf;
}

std::string InputConfig::GetActionPromptText(GameAction action, ActiveInputDevice activeDevice, bool isDirectInput) const
{
    const int idx = static_cast<int>(action);
    if (idx < 0 || idx >= static_cast<int>(GameAction::Count)) return "";

    if (activeDevice == ActiveInputDevice::KeyboardMouse)
    {
        return "[" + GetKbmKeyName(m_bindings.kbmBindings[idx]) + "]";
    }
    else
    {
        if (isDirectInput)
        {
            return "[" + GetDInputButtonName(m_bindings.dinputBindings[idx]) + "]";
        }
        else
        {
            return "[" + GetXInputButtonName(m_bindings.xinputBindings[idx]) + "]";
        }
    }
}
