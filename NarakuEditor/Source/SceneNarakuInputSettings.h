#pragma once

#include "InputConfig.h"

class SceneNarakuInputSettings
{
public:
    SceneNarakuInputSettings();
    ~SceneNarakuInputSettings() = default;

    // 統合メニューの「設定」タブ内部で呼び出す描画関数
    void DrawSettingsTab();

    // 設定画面を開いた時の初期化（現在の設定をワークスペースにコピー）
    void OnOpen();

    // リバインド中かどうか（リバインド中は他の全メニュー操作を遮断するため）
    bool IsRebinding() const { return m_isRebinding; }

private:
    void DrawRebindModal();

    InputBindings m_workingBindings;   // 編集中の設定
    int m_selectedDevice = 0;          // 0: KeyboardMouse, 1: XInput, 2: DirectInput
    
    // リバインド待ち受け状態
    bool m_isRebinding = false;
    GameAction m_rebindingAction = GameAction::Count;
    int m_rebindingDevice = 0;
    float m_bButtonHoldTimer = 0.0f;   // B長押しキャンセル用タイマー
    static constexpr float kBCancelHoldThreshold = 1.0f; // 1秒長押しでキャンセル
    float m_rebindDelayTimer = 0.0f;   // リバインド開始直後の余波入力無視タイマー

    std::string m_statusMessage;
    float m_statusMessageTimer = 0.0f;
};
