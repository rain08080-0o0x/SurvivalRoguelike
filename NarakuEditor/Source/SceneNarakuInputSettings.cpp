#include "SceneNarakuInputSettings.h"
#include "Input.h"
#include "NarakuUiNavigation.h"
#include "imgui.h"
#include <Xinput.h>

SceneNarakuInputSettings::SceneNarakuInputSettings()
{
    OnOpen();
}

void SceneNarakuInputSettings::OnOpen()
{
    m_workingBindings = InputConfig::GetInstance().GetBindings();
    m_isRebinding = false;
    SetInputCaptureActive(false);
    m_rebindingAction = GameAction::Count;
    m_bButtonHoldTimer = 0.0f;
    m_statusMessage.clear();
    m_statusMessageTimer = 0.0f;

    // 現在接続されているデバイスをデフォルトタブにする
    if (IsPadConnected())
    {
        m_selectedDevice = IsUsingDirectInput() ? 2 : 1;
    }
    else
    {
        m_selectedDevice = 0;
    }
}

void SceneNarakuInputSettings::DrawSettingsTab()
{
    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;

    if (m_statusMessageTimer > 0.0f)
    {
        m_statusMessageTimer -= dt;
        if (m_statusMessageTimer <= 0.0f) m_statusMessage.clear();
    }

    ImGui::TextDisabled(u8"ゲームパッドおよびキーボードの操作割り当てを変更できます。");
    ImGui::Spacing();

    // デバイス選択タブ / ラジオボタン
    ImGui::Text(u8"設定対象デバイス:");
    ImGui::SameLine();
    if (ImGui::RadioButton(u8"キーボード＆マウス", m_selectedDevice == 0)) m_selectedDevice = 0;
    ImGui::SameLine();
    if (ImGui::RadioButton(u8"GamePad (XInput)", m_selectedDevice == 1)) m_selectedDevice = 1;
    ImGui::SameLine();
    if (ImGui::RadioButton(u8"GamePad (DirectInput)", m_selectedDevice == 2)) m_selectedDevice = 2;

    ImGui::Separator();
    ImGui::Spacing();

    // アクション一覧テーブル
    if (ImGui::BeginTable("InputBindingsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 320.0f)))
    {
        ImGui::TableSetupColumn(u8"操作項目", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn(u8"現在の割り当て", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn(u8"変更", ImGuiTableColumnFlags_WidthFixed, 120.0f);
        ImGui::TableSetupColumn(u8"説明", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(GameAction::Count); ++i)
        {
            const GameAction act = static_cast<GameAction>(i);
            ImGui::TableNextRow();

            // 1. 操作名
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", InputConfig::GetActionName(act));

            // 2. 現在の割り当て
            ImGui::TableSetColumnIndex(1);
            std::string bindName;
            int code = 0;
            if (m_selectedDevice == 0)
            {
                code = m_workingBindings.kbmBindings[i];
                bindName = InputConfig::GetKbmKeyName(code);
            }
            else if (m_selectedDevice == 1)
            {
                code = m_workingBindings.xinputBindings[i];
                bindName = InputConfig::GetXInputButtonName(code);
            }
            else
            {
                code = m_workingBindings.dinputBindings[i];
                bindName = InputConfig::GetDInputButtonName(code);
            }
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), "%s", bindName.c_str());

            // 3. 変更ボタン
            ImGui::TableSetColumnIndex(2);
            char btnLabel[32];
            snprintf(btnLabel, sizeof(btnLabel), u8"変更##%d", i);
            if (ImGui::Button(btnLabel))
            {
                m_isRebinding = true;
                m_rebindingAction = act;
                m_rebindingDevice = m_selectedDevice;
                m_bButtonHoldTimer = 0.0f;
                m_rebindDelayTimer = 0.2f;
                SetInputGuardActive(true);
                SetInputCaptureActive(true);
            }

            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton(u8"解除"))
            {
                if (m_selectedDevice == 0) m_workingBindings.kbmBindings[i] = 0;
                else if (m_selectedDevice == 1) m_workingBindings.xinputBindings[i] = 0;
                else m_workingBindings.dinputBindings[i] = -1;
            }
            ImGui::PopID();

            // 4. 説明
            ImGui::TableSetColumnIndex(3);
            ImGui::TextDisabled("%s", InputConfig::GetActionDescription(act));
        }

        ImGui::EndTable();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (m_selectedDevice == 2)
    {
        const char* axes[] = { "X", "Y", "Z", "Rx", "Ry", "Rz" };
        ImGui::Combo(u8"右スティック横軸", &m_workingBindings.rightStickXAxis, axes, 6);
        ImGui::Combo(u8"右スティック縦軸", &m_workingBindings.rightStickYAxis, axes, 6);
        ImGui::Checkbox(u8"縦軸を反転", &m_workingBindings.rightStickInvertY);
        for (int axis = 0; axis < 6; ++axis)
        {
            if (axis) ImGui::SameLine();
            ImGui::Text("%s: %+.2f", axes[axis], GetDirectInputAxis(axis));
        }
    }
    std::string conflict;
    for (int device = 0; device < 3 && conflict.empty(); ++device)
    {
        const int* bindings = device == 0 ? m_workingBindings.kbmBindings :
            (device == 1 ? m_workingBindings.xinputBindings : m_workingBindings.dinputBindings);
        for (int i = 0; i < static_cast<int>(GameAction::Count) && conflict.empty(); ++i)
            for (int j = i + 1; j < static_cast<int>(GameAction::Count); ++j)
                if ((device == 2 ? bindings[i] >= 0 : bindings[i] != 0) && bindings[i] == bindings[j])
                {
                    conflict = std::string(device == 0 ? "Keyboard: " : device == 1 ? "XInput: " : "DirectInput: ") +
                        InputConfig::GetActionName(static_cast<GameAction>(i)) + u8" と " +
                        InputConfig::GetActionName(static_cast<GameAction>(j)) + u8" が重複しています。";
                    break;
                }
    }
    if (!conflict.empty()) ImGui::TextWrapped("%s", conflict.c_str());
    ImGui::BeginDisabled(!conflict.empty());
    // 下部アクションボタン
    if (ImGui::Button(u8"適用して保存", ImVec2(120.0f, 32.0f)))
    {
        auto& config = InputConfig::GetInstance();
        const auto previous = config.GetBindings();
        config.SetBindings(m_workingBindings);
        if (config.SaveToFile()) m_statusMessage = u8"設定を保存し、ゲームに適用しました。";
        else
        {
            config.SetBindings(previous);
            m_statusMessage = u8"保存できませんでした。書き込み先を確認してください。変更は未適用です。";
        }
        m_statusMessageTimer = 3.0f;
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button(u8"初期設定に戻す", ImVec2(120.0f, 32.0f)))
    {
        m_workingBindings = InputConfig::GetDefaultBindings();
        m_statusMessage = u8"すべての割り当てを初期設定に戻しました（適用で確定）。";
        m_statusMessageTimer = 3.0f;
    }

    ImGui::SameLine();
    if (ImGui::Button(u8"変更を破棄", ImVec2(100.0f, 32.0f)))
    {
        m_workingBindings = InputConfig::GetInstance().GetBindings();
        m_statusMessage = u8"変更を破棄し、現在の設定に戻しました。";
        m_statusMessageTimer = 3.0f;
    }

    if (!m_statusMessage.empty())
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.4f, 1.0f), "%s", m_statusMessage.c_str());
    }

    // コントローラー入力モニター
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), u8"【入力デバイス・リアルタイムモニター】");
    const bool xcon = IsXInputConnected();
    const bool dicon = IsDirectInputConnected();
    ImGui::Text(u8"接続状態: XInput: %s  |  DirectInput: %s",
        xcon ? u8"接続中" : u8"未接続",
        dicon ? u8"接続中" : u8"未接続");
    const float lx = GetPadLeftStickX();
    const float ly = GetPadLeftStickY();
    const float rx = GetPadRightStickX();
    const float ry = GetPadRightStickY();
    ImGui::Text(u8"左スティック: X: %+.2f, Y: %+.2f   |   右スティック: X: %+.2f, Y: %+.2f", lx, ly, rx, ry);

    // リバインドモーダルの処理
    if (m_isRebinding)
    {
        DrawRebindModal();
    }
}

void SceneNarakuInputSettings::DrawRebindModal()
{
    ImGuiIO& io = ImGui::GetIO();
    const float dt = io.DeltaTime;

    if (m_rebindDelayTimer > 0.0f)
    {
        m_rebindDelayTimer -= dt;
    }

    // リバインド中は、ImGuiのナビゲーション（SpaceキーやAボタンでのボタン決定）を一時遮断


    ImGui::OpenPopup(u8"キー / ボタンの割り当て");

    // 中央にモーダル配置
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 230.0f));

    if (NarakuUi::BeginPopupModal(u8"キー / ボタンの割り当て", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        ImGui::Text(u8"【%s】の割り当てを変更します。", InputConfig::GetActionName(m_rebindingAction));
        ImGui::Spacing();
        if (m_rebindDelayTimer > 0.0f)
        {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), u8"入力を待機しています...");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"割り当てたいキー、またはボタンを押してください...");
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool cancelRequested = false;
        if (m_rebindDelayTimer <= 0.0f && IsRawKeyTrigger(VK_ESCAPE))
        {
            cancelRequested = true;
        }

        int detectedCode = 0;
        bool inputDetected = false;

        if (m_rebindDelayTimer <= 0.0f)
        {
            if (m_rebindingDevice == 0) // KeyboardMouse
            {
                if (GetAnyKbmTrigger(detectedCode))
                {
                    if (detectedCode != VK_ESCAPE)
                    {
                        inputDetected = true;
                    }
                    else
                    {
                        cancelRequested = true;
                    }
                }
            }
            else if (m_rebindingDevice == 1) // XInput
            {
                if (GetAnyXInputTrigger(detectedCode))
                {
                    inputDetected = true;
                }
            }
            else if (m_rebindingDevice == 2) // DirectInput
            {
                if (GetAnyDInputTrigger(detectedCode))
                {
                    inputDetected = true;
                }
            }
        }

        if (m_rebindDelayTimer <= 0.0f &&
            (ImGui::IsKeyPressed(ImGuiKey_Escape) ||
             (m_rebindingDevice == 1 && inputDetected && detectedCode == XINPUT_GAMEPAD_BACK) ||
             (m_rebindingDevice == 2 && inputDetected && detectedCode == 8)))
        {
            cancelRequested = true;
            inputDetected = false;
        }
        if (inputDetected)
        {
            const int actIdx = static_cast<int>(m_rebindingAction);
            if (m_rebindingDevice == 0) m_workingBindings.kbmBindings[actIdx] = detectedCode;
            else if (m_rebindingDevice == 1) m_workingBindings.xinputBindings[actIdx] = detectedCode;
            else if (m_rebindingDevice == 2) m_workingBindings.dinputBindings[actIdx] = detectedCode;

            m_isRebinding = false;
            SetInputCaptureActive(false);
            SetInputGuardActive(true);
            io.ConfigFlags |= (ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
            ImGui::CloseCurrentPopup();
        }

        if (cancelRequested || (m_rebindDelayTimer <= 0.0f && ImGui::Button(u8"キャンセル", ImVec2(100.0f, 28.0f))))
        {
            m_isRebinding = false;
            SetInputCaptureActive(false);
            SetInputGuardActive(true);
            io.ConfigFlags |= (ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();
        ImGui::TextDisabled(u8"[Esc / Back / Button 9] またはクリックで中断");

        ImGui::EndPopup();
    }
    else
    {
        io.ConfigFlags |= (ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
    }

}
