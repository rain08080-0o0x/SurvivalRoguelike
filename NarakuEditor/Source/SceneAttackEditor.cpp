#include "SceneAttackEditor.h"
#include "DirectX.h"
#include "EditorPerformanceProfiler.h"
#include "Geometory.h"
#include "Model.h"
#include "SceneManager.h"
#include "ShaderList.h"
#include "Texture.h"
#include "imgui.h"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cfloat>

#undef min
#undef max

namespace
{
    const char* PhaseNames[] = { u8"発生", u8"攻撃", u8"中間隙", u8"後隙", u8"無敵" };
    const ImU32 PhaseColors[] = {
        IM_COL32(46, 204, 133, 255), IM_COL32(235, 55, 55, 255),
        IM_COL32(46, 204, 133, 255), IM_COL32(55, 110, 235, 255), IM_COL32(165, 75, 225, 255) };

    bool EditString(const char* label, std::string& value)
    {
        char buffer[1024] = {};
        strncpy_s(buffer, value.c_str(), _TRUNCATE);
        if (!ImGui::InputText(label, buffer, sizeof(buffer))) return false;
        value = buffer;
        return true;
    }

    std::string AttackPath(const NarakuCombat::AttackDefinition& attack)
    {
        if (!attack.sourcePath.empty())
        {
            std::string path = attack.sourcePath;
            std::replace(path.begin(), path.end(), '\\', '/');
            return path;
        }
        std::string name = attack.id;
        for (char& c : name)
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-')) c = '_';
        return "Assets/Data/Combat/" + name + ".nattack";
    }

    int PublishData()
    {
        const char* paths[] = { "Tools\\Publish-NarakuCombat.ps1", "..\\Tools\\Publish-NarakuCombat.ps1",
            "..\\..\\..\\Tools\\Publish-NarakuCombat.ps1" };
        for (const char* path : paths)
        {
            if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
            const std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -File \"" +
                std::string(path) + "\" -EditorAssetsPath Assets";
            return std::system(command.c_str());
        }
        return -1;
    }

    int PhaseStart(const NarakuCombat::AttackDefinition& attack, int index)
    {
        int start = 0;
        for (int i = 0; i < index; ++i) start += attack.phases[i].frames;
        return start;
    }

    NarakuCombat::FrameKey& FrameOverride(NarakuCombat::AttackDefinition& attack, int index, int frame)
    {
        auto& phase = attack.phases[index];
        const int local = frame - PhaseStart(attack, index);
        auto found = std::lower_bound(phase.keys.begin(), phase.keys.end(), local,
            [](const NarakuCombat::FrameKey& key, int f) { return key.frame < f; });
        if (found != phase.keys.end() && found->frame == local) return *found;
        const auto state = NarakuCombat::EvaluateFrame(attack, static_cast<float>(frame));
        NarakuCombat::FrameKey key;
        key.frame = local; key.hurtbox = state.hurtbox; key.attackBox = state.attackBox;
        return *phase.keys.insert(found, key);
    }

    bool EditBox(const char* id, NarakuCombat::HitboxKey& box)
    {
        ImGui::PushID(id);
        bool changed = ImGui::DragFloat3(u8"位置", &box.center.x, 0.01f);
        changed |= ImGui::DragFloat3(u8"サイズ（半幅）", &box.size.x, 0.01f, 0.0f, 1000.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
        box.size.x = std::max(0.0f, box.size.x);
        box.size.y = std::max(0.0f, box.size.y);
        box.size.z = std::max(0.0f, box.size.z);
        ImGui::PopID();
        return changed;
    }

    void WindowLayout(int column, int row)
    {
        const auto* viewport = ImGui::GetMainViewport();
        const float left = viewport->WorkSize.x * 0.72f;
        const float top = viewport->WorkSize.y * 0.66f;
        ImGui::SetNextWindowPos({ viewport->WorkPos.x + (column ? left : 0),
            viewport->WorkPos.y + (row ? top : 0) }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize({ column ? viewport->WorkSize.x - left : left,
            row ? viewport->WorkSize.y - top : top }, ImGuiCond_FirstUseEver);
    }

    void WireBox(const NarakuCombat::HitboxKey& box, const DirectX::XMFLOAT4& color)
    {
        const auto c = box.center, s = box.size;
        DirectX::XMFLOAT3 corners[8];
        for (int i = 0; i < 8; ++i)
            corners[i] = { c.x + ((i & 1) ? s.x : -s.x), c.y + ((i & 2) ? s.y : -s.y),
                c.z + ((i & 4) ? s.z : -s.z) };
        for (int i = 0; i < 8; ++i)
            for (int axis = 0; axis < 3; ++axis)
                if (!(i & (1 << axis))) Geometory::AddLine(corners[i], corners[i | (1 << axis)], color);
    }

    void EditRange(const char* label, int& start, int& end)
    {
        int range[] = { start < 0 ? 0 : start + 1, end < 0 ? 0 : end + 1 };
        if (ImGui::InputInt2(label, range))
        {
            start = range[0] <= 0 ? -1 : range[0] - 1;
            end = range[1] <= 0 ? -1 : range[1] - 1;
        }
    }
}

SceneAttackEditor::SceneAttackEditor() { ResetPreviewCamera(); Reload(); }
SceneAttackEditor::~SceneAttackEditor() { ReleasePreviewResources(); }
void SceneAttackEditor::Update() { EDITOR_PROFILE_FUNCTION(); }

NarakuCombat::AttackDefinition* SceneAttackEditor::GetSelectedAttack()
{
    return m_selected < 0 || m_selected >= static_cast<int>(m_catalog.attacks.size())
        ? nullptr : &m_catalog.attacks[m_selected];
}

void SceneAttackEditor::ChangeSelection(int index)
{
    ReleasePreviewModel(); m_selected = index; m_selectedPhase = -1; m_previewFrame = 0;
    m_playing = false; m_dragFace = -1; m_editDefaultBox = false;
}

void SceneAttackEditor::Reload()
{
    m_catalog = {}; std::string error;
    NarakuCombat::LoadCatalog("Assets/Data/Combat", m_catalog, &error);
    ChangeSelection(m_catalog.attacks.empty() ? -1 : 0);
    m_message = error.empty() ? u8"攻撃データを読み込みました。" : error;
}

void SceneAttackEditor::CreateAttack()
{
    NarakuCombat::AttackDefinition attack;
    int number = 1;
    do { attack.id = "attack_" + std::to_string(number++); }
    while (NarakuCombat::FindAttack(m_catalog, attack.id));
    attack.displayName = u8"新しい攻撃";
    m_catalog.attacks.push_back(attack);
    ChangeSelection(static_cast<int>(m_catalog.attacks.size()) - 1);
    m_message = u8"新しい攻撃データを作成しました。";
}

void SceneAttackEditor::SaveSelected()
{
    auto* attack = GetSelectedAttack(); if (!attack) return;
    m_saveWarnings = NarakuCombat::SaveWarnings(*attack);
    if (m_saveWarnings.empty()) SaveNow(false);
    else { m_pendingSave = 1; m_openSaveConfirmation = true; }
}

void SceneAttackEditor::Publish()
{
    m_saveWarnings.clear();
    for (const auto& attack : m_catalog.attacks)
        for (const auto& warning : NarakuCombat::SaveWarnings(attack))
            m_saveWarnings.push_back(attack.displayName + ": " + warning);
    if (m_saveWarnings.empty()) SaveNow(true);
    else { m_pendingSave = 2; m_openSaveConfirmation = true; }
}

void SceneAttackEditor::SaveNow(bool publish)
{
    for (std::size_t i = 0; i < m_catalog.attacks.size(); ++i)
        for (std::size_t j = i + 1; j < m_catalog.attacks.size(); ++j)
        {
            const auto& a = m_catalog.attacks[i]; const auto& b = m_catalog.attacks[j];
            if (!publish && &a != GetSelectedAttack() && &b != GetSelectedAttack()) continue;
            if (a.id == b.id || _stricmp(AttackPath(a).c_str(), AttackPath(b).c_str()) == 0)
            { m_message = u8"攻撃IDまたは保存先が重複しています。"; return; }
        }
    NarakuCombat::Catalog validation = m_catalog;
    if (!publish)
    {
        auto* attack = GetSelectedAttack(); if (!attack) return;
        if (!attack->nextAttackId.empty() && !NarakuCombat::FindAttack(m_catalog, attack->nextAttackId))
        { m_message = u8"次の攻撃IDが見つかりません。"; return; }
        validation.attacks = { *attack };
        // 他の攻撃へのコンボ参照はカタログ全体で確認します。
        validation.attacks[0].nextAttackId.clear();
    }
    const auto issues = NarakuCombat::Validate(validation);
    if (!issues.empty()) { m_message = issues.front(); return; }
    for (auto& attack : m_catalog.attacks)
    {
        if (!publish && &attack != GetSelectedAttack()) continue;
        const std::string path = AttackPath(attack);
        std::string error;
        if (!NarakuCombat::SaveAttack(path, attack, &error)) { m_message = error; return; }
        attack.sourcePath = path;
    }
    m_message = publish ? (PublishData() == 0 ? u8"ゲームへ発行しました。" : u8"ゲームへの発行に失敗しました。")
        : u8"保存しました。";
}

void SceneAttackEditor::DrawSaveConfirmation()
{
    if (m_openSaveConfirmation)
    {
        ImGui::OpenPopup(u8"保存前の警告"); m_openSaveConfirmation = false;
    }
    if (!ImGui::BeginPopupModal(u8"保存前の警告", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
    for (const auto& warning : m_saveWarnings) ImGui::TextUnformatted(warning.c_str());
    if (ImGui::Button(u8"そのまま保存"))
    {
        SaveNow(m_pendingSave == 2); m_pendingSave = 0; ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"キャンセル")) { m_pendingSave = 0; ImGui::CloseCurrentPopup(); }
    ImGui::EndPopup();
}

bool SceneAttackEditor::HandleNativeMenuCommand(unsigned int id)
{
    switch (id)
    {
    case MenuReload: Reload(); return true;
    case MenuSave: SaveSelected(); return true;
    case MenuPublish: Publish(); return true;
    case MenuNewAttack: CreateAttack(); return true;
    case MenuShowStageEditor: SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_EDITOR); return true;
    case MenuShowPieceEditor: SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_PIECE_EDITOR); return true;
    case MenuToggleDataWindow: m_showDataWindow = !m_showDataWindow; return true;
    case MenuToggleTimelineWindow: m_showTimelineWindow = !m_showTimelineWindow; return true;
    case MenuToggleHitboxWindow: m_showHitboxWindow = !m_showHitboxWindow; return true;
    case MenuTogglePreviewWindow: m_showPreviewWindow = !m_showPreviewWindow; return true;
    default: return false;
    }
}

void SceneAttackEditor::SyncNativeMenuState(void* handle) const
{
    const auto menu = static_cast<HMENU>(handle); if (!menu) return;
    const auto sync = [menu](int id, bool on) { CheckMenuItem(menu, id, MF_BYCOMMAND | (on ? MF_CHECKED : MF_UNCHECKED)); };
    sync(MenuToggleDataWindow, m_showDataWindow); sync(MenuToggleTimelineWindow, m_showTimelineWindow);
    sync(MenuToggleHitboxWindow, m_showHitboxWindow); sync(MenuTogglePreviewWindow, m_showPreviewWindow);
}

void SceneAttackEditor::DrawSettingsWindow()
{
    if (!m_showDataWindow) return;
    WindowLayout(1, 0);
    if (!ImGui::Begin(u8"設定###AttackSettings", &m_showDataWindow)) { ImGui::End(); return; }
    if (ImGui::Button(u8"新規")) CreateAttack();
    ImGui::SameLine(); if (ImGui::Button(u8"保存")) SaveSelected();
    ImGui::SameLine(); if (ImGui::Button(u8"発行")) Publish();
    if (ImGui::BeginCombo(u8"データ", GetSelectedAttack() ? GetSelectedAttack()->displayName.c_str() : u8"未選択"))
    {
        for (int i = 0; i < static_cast<int>(m_catalog.attacks.size()); ++i)
        {
            ImGui::PushID(i);
            if (ImGui::Selectable((m_catalog.attacks[i].displayName + " (" + m_catalog.attacks[i].id + ")").c_str(), m_selected == i))
                ChangeSelection(i);
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    auto* attack = GetSelectedAttack();
    if (attack && ImGui::BeginTabBar("SettingsTabs"))
    {
        auto& a = *attack;
        if (ImGui::BeginTabItem("Camera"))
        {
            float angles[] = { DirectX::XMConvertToDegrees(m_previewCameraPitch), DirectX::XMConvertToDegrees(m_previewCameraYaw) };
            if (ImGui::DragFloat2(u8"回転（度）", angles, 0.5f))
            {
                m_previewCameraPitch = DirectX::XMConvertToRadians(std::max(-89.0f, std::min(89.0f, angles[0])));
                m_previewCameraYaw = DirectX::XMConvertToRadians(angles[1]);
            }
            float cp = std::cos(m_previewCameraPitch);
            float eye[] = { m_previewCameraTarget.x + std::sin(m_previewCameraYaw) * cp * m_previewCameraZoom,
                m_previewCameraTarget.y + std::sin(m_previewCameraPitch) * m_previewCameraZoom,
                m_previewCameraTarget.z + std::cos(m_previewCameraYaw) * cp * m_previewCameraZoom };
            if (ImGui::DragFloat3(u8"位置", eye, 0.01f))
                m_previewCameraTarget = { eye[0] - std::sin(m_previewCameraYaw) * cp * m_previewCameraZoom,
                    eye[1] - std::sin(m_previewCameraPitch) * m_previewCameraZoom,
                    eye[2] - std::cos(m_previewCameraYaw) * cp * m_previewCameraZoom };
            ImGui::DragFloat3(u8"注視点", &m_previewCameraTarget.x, 0.01f);
            ImGui::DragFloat(u8"距離", &m_previewCameraZoom, 0.01f, 0.01f, 1000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
            if (ImGui::Button(u8"初期化")) ResetPreviewCamera();
            ImGui::TextWrapped(u8"Alt+左: 回転 / 中: 平行移動 / ホイール: ズーム / 右ドラッグ+WASD: 移動");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Add"))
        {
            for (int type = 0; type < 5; ++type)
                if (ImGui::Button(PhaseNames[type]))
                {
                    if (NarakuCombat::PhaseFrames(a) >= a.totalFrames) m_message = u8"区間を追加するには最大フレーム数を増やしてください。";
                    else
                    {
                        NarakuCombat::Phase phase;
                        phase.type = static_cast<NarakuCombat::PhaseType>(type);
                        phase.hurtbox = NarakuCombat::EvaluateFrame(a, static_cast<float>(NarakuCombat::PhaseFrames(a))).hurtbox;
                        a.phases.push_back(phase);
                        m_selectedPhase = static_cast<int>(a.phases.size()) - 1;
                        m_previewFrame = static_cast<float>(PhaseStart(a, m_selectedPhase));
                        m_playing = false; m_editDefaultBox = false;
                    }
                }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Frame"))
        {
            int maximum = a.totalFrames;
            if (ImGui::InputInt(u8"最大フレーム数", &maximum))
            {
                const auto count = a.phases.size();
                NarakuCombat::ResizeTimeline(a, maximum);
                if (a.phases.size() != count) m_message = u8"最大フレーム数を超えた区間を削除しました。";
                if (m_selectedPhase >= static_cast<int>(a.phases.size())) m_selectedPhase = -1;
            }
            ImGui::InputInt(u8"フレームレート", &a.framesPerSecond);
            a.framesPerSecond = std::max(1, a.framesPerSecond);
            if (ImGui::CollapsingHeader(u8"基本情報"))
            {
                EditString(u8"攻撃ID", a.id); EditString(u8"表示名", a.displayName);
                int type = static_cast<int>(a.dataType);
                if (ImGui::Combo(u8"攻撃データタイプ", &type, u8"全て\0攻撃\0移動\0ガード\0無敵\0"))
                    a.dataType = static_cast<NarakuCombat::DataType>(type);
                ImGui::Checkbox(u8"完成品（未チェックは作業中）", &a.completed);
                EditString(u8"使用者ID", a.userId);
                int user = static_cast<int>(a.userKind);
                if (ImGui::Combo(u8"使用者", &user, u8"プレイヤー\0敵\0")) a.userKind = static_cast<NarakuCombat::UserKind>(user);
                int presentation = static_cast<int>(a.presentation);
                if (ImGui::Combo(u8"表示方式", &presentation, u8"武器変形\0スプライト連番\0モデルアニメーション\0"))
                { a.presentation = static_cast<NarakuCombat::PresentationKind>(presentation); ReleasePreviewModel(); }
                if (EditString(u8"モデルID", a.modelId)) ReleasePreviewModel();
                if (EditString(u8"アニメーションファイル", a.animationPath)) ReleasePreviewModel();
                ImGui::TextWrapped(u8"アニメーションファイルの先頭クリップを再生します。");
                ImGui::InputInt(u8"スプライト開始", &a.spriteStartFrame);
                ImGui::InputInt(u8"スプライト枚数", &a.spriteFrameCount);
            }
            if (ImGui::CollapsingHeader(u8"戦闘設定"))
            {
                EditString(u8"次の攻撃ID", a.nextAttackId);
                ImGui::InputFloat(u8"ダメージ倍率", &a.damageMultiplier);
                ImGui::InputFloat(u8"ノックバック", &a.knockback);
                ImGui::InputFloat(u8"スタミナ消費", &a.staminaCost);
                ImGui::InputFloat(u8"再使用時間（秒）", &a.cooldownSeconds);
                ImGui::Checkbox(u8"壁を貫通", &a.piercesWalls);
                ImGui::Checkbox(u8"敵を貫通", &a.piercesEnemies);
                ImGui::Checkbox(u8"同じ対象には一度だけ命中", &a.singleHitPerTarget);
                ImGui::TextWrapped(u8"期間は1始まり。0は未設定です。");
                EditRange(u8"パリィ開始・終了", a.parryStartFrame, a.parryEndFrame);
                EditRange(u8"移動開始・終了", a.movementStartFrame, a.movementEndFrame);
                EditRange(u8"方向転換開始・終了", a.turnStartFrame, a.turnEndFrame);
                EditRange(u8"コンボ開始・終了", a.comboInputStartFrame, a.comboInputEndFrame);
                EditRange(u8"回避開始・終了", a.dodgeCancelStartFrame, a.dodgeCancelEndFrame);
            }
            if (ImGui::CollapsingHeader(u8"武器変形"))
            {
                for (int i = 0; i < static_cast<int>(a.transformKeys.size()); ++i)
                {
                    auto& key = a.transformKeys[i]; ImGui::PushID(i);
                    int frame = key.frame + 1;
                    if (ImGui::InputInt(u8"フレーム", &frame)) key.frame = std::max(0, frame - 1);
                    ImGui::DragFloat3(u8"位置", &key.position.x, 0.01f);
                    ImGui::DragFloat3(u8"回転", &key.rotationDegrees.x, 0.5f);
                    ImGui::DragFloat3(u8"拡大率", &key.scale.x, 0.01f);
                    if (ImGui::Button(u8"削除")) { a.transformKeys.erase(a.transformKeys.begin() + i); ImGui::PopID(); break; }
                    ImGui::Separator(); ImGui::PopID();
                }
                if (ImGui::Button(u8"変形キー追加"))
                {
                    auto key = NarakuCombat::EvaluateTransform(a, m_previewFrame);
                    key.frame = static_cast<int>(m_previewFrame); a.transformKeys.push_back(key);
                }
                std::stable_sort(a.transformKeys.begin(), a.transformKeys.end(), [](const auto& x, const auto& y) { return x.frame < y.frame; });
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Hierarchy"))
        {
            int start = 0;
            for (int i = 0; i < static_cast<int>(a.phases.size()); ++i)
            {
                ImGui::PushID(i);
                const auto& phase = a.phases[i];
                const std::string label = std::string(PhaseNames[static_cast<int>(phase.type)]) + "  " +
                    std::to_string(start + 1) + u8"～" + std::to_string(start + phase.frames);
                if (ImGui::Selectable(label.c_str(), m_selectedPhase == i))
                { m_selectedPhase = i; m_previewFrame = static_cast<float>(start); m_playing = false; m_editDefaultBox = false; }
                start += phase.frames; ImGui::PopID();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("HitBox"))
        {
            ImGui::TextWrapped(u8"区間を通過する前の通常被弾判定です。区間終了後は、その区間の最後の通常ヒットボックスを維持します。");
            EditBox("Default", a.defaultHurtbox);
            ImGui::Checkbox(u8"ビューで既存ヒットボックスを編集", &m_editDefaultBox);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::Separator(); ImGui::TextWrapped("%s", m_message.c_str());
    ImGui::End();
}

int SceneAttackEditor::PlaybackFrames() const
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_catalog.attacks.size())) return 1;
    const auto& attack = m_catalog.attacks[m_selected];
    return std::max(attack.totalFrames, static_cast<int>(std::ceil(m_animationSeconds * attack.framesPerSecond - 0.0001f)));
}

void SceneAttackEditor::DrawTimelineWindow()
{
    if (!m_showTimelineWindow) return;
    WindowLayout(0, 1);
    if (!ImGui::Begin(u8"フレームメータ###AttackMeter", &m_showTimelineWindow)) { ImGui::End(); return; }
    auto* a = GetSelectedAttack();
    if (a)
    {
        const int maximum = PlaybackFrames();
        if (ImGui::Button(m_playing ? u8"□ 停止" : u8"▷ 再生"))
        {
            if (!m_playing && m_previewFrame >= maximum - 1) m_previewFrame = 0;
            m_playing = !m_playing;
        }
        ImGui::SameLine(); ImGui::Checkbox(u8"ループ", &m_loopPlayback);
        ImGui::SameLine(); ImGui::SetNextItemWidth(95);
        if (ImGui::InputFloat(u8"倍速", &m_playbackSpeed, 0.1f, 0.1f, "%.1f"))
            m_playbackSpeed = std::isfinite(m_playbackSpeed)
                ? std::max(0.1f, std::min(2.0f, std::round(m_playbackSpeed * 10) / 10)) : 1.0f;
        ImGui::SameLine(); ImGui::Text("Frame:%d/%d", static_cast<int>(m_previewFrame) + 1, maximum);
        int frame = static_cast<int>(m_previewFrame) + 1;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderInt("##Frame", &frame, 1, maximum))
        {
            m_previewFrame = static_cast<float>(frame - 1); m_playing = false;
            m_selectedPhase = NarakuCombat::EvaluateFrame(*a, m_previewFrame).phaseIndex;
        }
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const float width = std::max(1.0f, ImGui::GetContentRegionAvail().x);
        ImGui::InvisibleButton("##Meter", { width, 30 });
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(origin, { origin.x + width, origin.y + 30 }, IM_COL32(40, 40, 45, 255));
        int start = 0;
        for (int i = 0; i < static_cast<int>(a->phases.size()); ++i)
        {
            const auto& p = a->phases[i];
            draw->AddRectFilled({ origin.x + width * start / maximum, origin.y },
                { origin.x + width * (start + p.frames) / maximum, origin.y + 30 }, PhaseColors[static_cast<int>(p.type)]);
            if (m_selectedPhase == i) draw->AddRect({ origin.x + width * start / maximum, origin.y },
                { origin.x + width * (start + p.frames) / maximum, origin.y + 30 }, IM_COL32_WHITE, 0, 0, 2);
            start += p.frames;
        }
        if (width / maximum >= 3)
            for (int i = 1; i < maximum; ++i)
                draw->AddLine({ origin.x + width * i / maximum, origin.y }, { origin.x + width * i / maximum, origin.y + 30 }, IM_COL32(0, 0, 0, 90));
        const float cursor = origin.x + width * (m_previewFrame + 0.5f) / maximum;
        draw->AddLine({ cursor, origin.y - 3 }, { cursor, origin.y + 33 }, IM_COL32_WHITE, 2);
        if (ImGui::IsItemActive())
        {
            m_previewFrame = static_cast<float>(std::max(0, std::min(maximum - 1,
                static_cast<int>((ImGui::GetIO().MousePos.x - origin.x) / width * maximum))));
            m_selectedPhase = NarakuCombat::EvaluateFrame(*a, m_previewFrame).phaseIndex; m_playing = false;
        }
        if (ImGui::Button(u8"前フレーム")) { m_previewFrame = std::max(0.0f, std::floor(m_previewFrame) - 1); m_playing = false; }
        ImGui::SameLine();
        if (ImGui::Button(u8"次フレーム")) { m_previewFrame = std::min(static_cast<float>(maximum - 1), std::floor(m_previewFrame) + 1); m_playing = false; }
        for (int i = 0; i < 5; ++i)
        {
            if (i) ImGui::SameLine();
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(PhaseColors[i]), "%s", PhaseNames[i]);
        }
    }
    ImGui::End();
}

void SceneAttackEditor::DrawFrameWindow()
{
    if (!m_showHitboxWindow) return;
    WindowLayout(1, 1);
    if (!ImGui::Begin(u8"フレーム設定###AttackFrame", &m_showHitboxWindow)) { ImGui::End(); return; }
    auto* a = GetSelectedAttack();
    if (a && m_selectedPhase >= 0 && m_selectedPhase < static_cast<int>(a->phases.size()))
    {
        auto& phase = a->phases[m_selectedPhase];
        int type = static_cast<int>(phase.type);
        if (ImGui::Combo(u8"タイプ", &type, u8"発生\0攻撃\0中間隙\0後隙\0無敵\0"))
            phase.type = static_cast<NarakuCombat::PhaseType>(type);
        int frames = phase.frames;
        if (ImGui::InputInt(u8"フレーム数", &frames))
        {
            phase.frames = std::max(1, std::min(a->totalFrames - NarakuCombat::PhaseFrames(*a) + phase.frames, frames));
            phase.keys.erase(std::remove_if(phase.keys.begin(), phase.keys.end(), [&](const auto& k) { return k.frame >= phase.frames; }), phase.keys.end());
        }
        const int start = PhaseStart(*a, m_selectedPhase);
        ImGui::Text(u8"区間: %d～%d", start + 1, start + phase.frames);
        ImGui::TextUnformatted(u8"通常ヒットボックス（区間共通）");
        EditBox("Hurtbox", phase.hurtbox);
        ImGui::Checkbox(u8"専用ヒットボックス", &phase.hasAttackBox);
        if (phase.hasAttackBox) EditBox("AttackBox", phase.attackBox);
        ImGui::Separator();
        const int current = static_cast<int>(m_previewFrame);
        if (current >= start && current < start + phase.frames)
        {
            const int local = current - start;
            auto found = std::find_if(phase.keys.begin(), phase.keys.end(), [local](const auto& k) { return k.frame == local; });
            bool overrideFrame = found != phase.keys.end();
            if (ImGui::Checkbox(u8"現在フレームを個別設定", &overrideFrame))
            {
                if (overrideFrame) FrameOverride(*a, m_selectedPhase, current);
                else phase.keys.erase(found);
            }
            if (overrideFrame)
            {
                auto& key = FrameOverride(*a, m_selectedPhase, current);
                EditBox("FrameHurtbox", key.hurtbox);
                if (phase.hasAttackBox) EditBox("FrameAttackBox", key.attackBox);
            }
            ImGui::TextWrapped(u8"未設定フレームは直前の値を維持します。");
        }
        else ImGui::TextWrapped(u8"個別編集するフレームへメータを移動してください。");
        ImGui::Checkbox(u8"ビューで専用ヒットボックスを編集", &m_editAttackBox);
        if (ImGui::Button(u8"区間を削除"))
        { a->phases.erase(a->phases.begin() + m_selectedPhase); m_selectedPhase = -1; }
    }
    else ImGui::TextUnformatted(u8"区間を選択してください。");
    ImGui::End();
}

bool SceneAttackEditor::EnsurePreviewRenderTarget(unsigned int width, unsigned int height)
{
    if (m_previewRenderTarget && m_previewDepthStencil && width == m_previewWidth && height == m_previewHeight) return true;
    SAFE_DELETE(m_previewRenderTarget); SAFE_DELETE(m_previewDepthStencil);
    m_previewRenderTarget = new RenderTarget();
    if (FAILED(m_previewRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, width, height))) { SAFE_DELETE(m_previewRenderTarget); return false; }
    m_previewDepthStencil = new DepthStencil();
    if (FAILED(m_previewDepthStencil->Create(width, height, false)))
    { SAFE_DELETE(m_previewDepthStencil); SAFE_DELETE(m_previewRenderTarget); return false; }
    m_previewWidth = width; m_previewHeight = height; return true;
}

void SceneAttackEditor::EnsurePreviewModel(const NarakuCombat::AttackDefinition& attack)
{
    const std::string key = attack.modelId + "|" + attack.animationPath + "|" + std::to_string(static_cast<int>(attack.presentation));
    if (m_previewModelAttempted && m_previewModelId == key) return;
    ReleasePreviewModel(); m_previewModelAttempted = true; m_previewModelId = key;
    const auto* definition = NarakuCombat::FindModel(m_catalog, attack.modelId);
    if (!definition) return;
    m_previewModel = new Model();
    const bool animated = attack.presentation == NarakuCombat::PresentationKind::ModelAnimation;
    const auto flip = definition->zFlip ? (animated ? Model::ZFlipUseAnime : Model::ZFlip) : Model::None;
    const bool loaded = animated ? m_previewModel->Load(definition->filePath.c_str(), 1, flip)
        : m_previewModel->LoadStatic(definition->filePath.c_str(), 1, flip);
    if (!loaded) { SAFE_DELETE(m_previewModel); m_message = u8"モデルを読み込めませんでした。"; return; }
    if (animated)
    {
        m_previewAnimation = m_previewModel->AddAnimation((attack.animationPath.empty() ? definition->filePath : attack.animationPath).c_str());
        const auto* animation = m_previewModel->GetAnimation(m_previewAnimation);
        if (!animation) m_message = u8"アニメーションを読み込めませんでした。";
        else { m_animationSeconds = animation->totalTime; m_previewModel->Play(m_previewAnimation, false); }
    }
    DirectX::XMFLOAT3 minimum = { FLT_MAX, FLT_MAX, FLT_MAX }, maximum = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    for (unsigned int i = 0; i < m_previewModel->GetMeshNum(); ++i)
        for (const auto& v : m_previewModel->GetMesh(i)->vertices)
        {
            minimum = { std::min(minimum.x, v.pos.x), std::min(minimum.y, v.pos.y), std::min(minimum.z, v.pos.z) };
            maximum = { std::max(maximum.x, v.pos.x), std::max(maximum.y, v.pos.y), std::max(maximum.z, v.pos.z) };
        }
    m_previewModelCenter = { (minimum.x + maximum.x) / 2, (minimum.y + maximum.y) / 2, (minimum.z + maximum.z) / 2 };
    m_previewModelLongestSize = std::max(0.001f, std::max(maximum.x - minimum.x, std::max(maximum.y - minimum.y, maximum.z - minimum.z)));
}

void SceneAttackEditor::RenderPreview3D(const NarakuCombat::AttackDefinition& attack, unsigned int width, unsigned int height)
{
    using namespace DirectX;
    if (!EnsurePreviewRenderTarget(width, height)) return;
    RenderTarget* targets[] = { m_previewRenderTarget }; SetRenderTargets(1, targets, m_previewDepthStencil);
    const float clear[] = { 0.035f, 0.045f, 0.06f, 1 }; m_previewRenderTarget->Clear(clear); m_previewDepthStencil->Clear();
    SetDepthTest(true); SetBlendMode(BLEND_NONE); SetCullingMode(D3D11_CULL_BACK);
    const float cp = std::cos(m_previewCameraPitch);
    const XMFLOAT3 eye = { m_previewCameraTarget.x + std::sin(m_previewCameraYaw) * cp * m_previewCameraZoom,
        m_previewCameraTarget.y + std::sin(m_previewCameraPitch) * m_previewCameraZoom,
        m_previewCameraTarget.z + std::cos(m_previewCameraYaw) * cp * m_previewCameraZoom };
    const auto view = XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&m_previewCameraTarget), XMVectorSet(0, 1, 0, 0));
    const auto projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(42), static_cast<float>(width) / height, 0.01f, 1000);
    XMStoreFloat4x4(&m_viewMatrix, view); XMStoreFloat4x4(&m_projectionMatrix, projection);
    XMFLOAT4X4 identity, vt, pt; XMStoreFloat4x4(&identity, XMMatrixIdentity());
    XMStoreFloat4x4(&vt, XMMatrixTranspose(view)); XMStoreFloat4x4(&pt, XMMatrixTranspose(projection));
    Geometory::SetWorld(identity); Geometory::SetView(vt); Geometory::SetProjection(pt);
    for (int i = -10; i <= 10; ++i)
    {
        const XMFLOAT4 color = i == 0 ? XMFLOAT4{0.3f, 0.45f, 0.6f, 1} : XMFLOAT4{0.14f, 0.17f, 0.2f, 1};
        Geometory::AddLine({ i * 0.5f, 0, -5 }, { i * 0.5f, 0, 5 }, color);
        Geometory::AddLine({ -5, 0, i * 0.5f }, { 5, 0, i * 0.5f }, color);
    }
    const auto state = NarakuCombat::EvaluateFrame(attack, m_previewFrame);
    WireBox(state.hurtbox, {0.25f, 0.85f, 1, 1});
    if (state.hasAttackBox) WireBox(state.attackBox, {1, 0.15f, 0.15f, 1});
    if (m_editDefaultBox) WireBox(attack.defaultHurtbox, {1, 1, 0.25f, 1});
    Geometory::DrawLines();
    if (m_previewModel)
    {
        const auto* definition = NarakuCombat::FindModel(m_catalog, attack.modelId);
        const auto transform = NarakuCombat::EvaluateTransform(attack, m_previewFrame);
        const bool animated = m_previewAnimation != Model::ANIME_NONE;
        if (animated)
        {
            m_previewModel->SetAnimationTime(m_previewAnimation, m_previewFrame / attack.framesPerSecond);
            m_previewModel->Step(0);
        }
        const float scale = definition->scale * (animated ? 1.0f : 0.6f / m_previewModelLongestSize);
        const auto center = animated ? XMFLOAT3{} : m_previewModelCenter;
        const auto world = XMMatrixTranslation(-center.x, -center.y, -center.z) *
            XMMatrixScaling(scale * transform.scale.x, scale * transform.scale.y, scale * transform.scale.z) *
            XMMatrixRotationRollPitchYaw(XMConvertToRadians(transform.rotationDegrees.x), XMConvertToRadians(transform.rotationDegrees.y), XMConvertToRadians(transform.rotationDegrees.z)) *
            XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
        XMFLOAT4X4 wvp[3]; XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(world)); wvp[1] = vt; wvp[2] = pt;
        ShaderList::SetWVP(wvp); ShaderList::SetLight({1, 1, 1, 1}, {-0.35f, -0.85f, 0.3f}); ShaderList::SetCameraPos(eye);
        m_previewModel->SetVertexShader(ShaderList::GetVS(animated ? ShaderList::VS_ANIME : ShaderList::VS_WORLD));
        m_previewModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int i = 0; i < m_previewModel->GetMeshNum(); ++i)
        {
            const auto* mesh = m_previewModel->GetMesh(i);
            if (animated)
            {
                XMFLOAT4X4 bones[200];
                for (auto& bone : bones) XMStoreFloat4x4(&bone, XMMatrixIdentity());
                for (std::size_t j = 0; j < std::min<std::size_t>(200, mesh->bones.size()); ++j)
                    XMStoreFloat4x4(&bones[j], XMMatrixTranspose(mesh->bones[j].invOffset * m_previewModel->GetBone(mesh->bones[j].index)));
                ShaderList::SetBones(bones);
            }
            const auto* material = m_previewModel->GetMaterial(mesh->materialID);
            if (material) ShaderList::SetMaterial(*material);
            m_previewModel->Draw(static_cast<int>(i));
        }
    }
    RenderTarget* defaults[] = { GetDefaultRTV() }; SetRenderTargets(1, defaults, GetDefaultDSV());
}

void SceneAttackEditor::EditPreviewBox(const DirectX::XMFLOAT2& origin, const DirectX::XMFLOAT2& size)
{
    using namespace DirectX;
    auto* a = GetSelectedAttack(); if (!a) return;
    const auto state = NarakuCombat::EvaluateFrame(*a, m_previewFrame);
    if (!m_editDefaultBox && (state.phaseIndex < 0 || (m_editAttackBox && !state.hasAttackBox))) return;
    auto box = m_editDefaultBox ? a->defaultHurtbox : (m_editAttackBox ? state.attackBox : state.hurtbox);
    const auto project = [&](XMFLOAT3 point)
    {
        XMFLOAT3 out;
        XMStoreFloat3(&out, XMVector3Project(XMLoadFloat3(&point), origin.x, origin.y, size.x, size.y, 0, 1,
            XMLoadFloat4x4(&m_projectionMatrix), XMLoadFloat4x4(&m_viewMatrix), XMMatrixIdentity()));
        return out;
    };
    auto* draw = ImGui::GetWindowDrawList();
    auto& io = ImGui::GetIO();
    const auto center = project({ box.center.x, box.center.y, box.center.z });
    for (int face = 0; face < 6; ++face)
    {
        const int axis = face / 2; const float sign = face % 2 ? 1.0f : -1.0f;
        XMFLOAT3 point = {box.center.x, box.center.y, box.center.z};
        (&point.x)[axis] += sign * (&box.size.x)[axis];
        const auto screen = project(point);
        if (screen.z < 0 || screen.z > 1) continue;
        const ImVec2 p = { screen.x, screen.y };
        draw->AddRectFilled({p.x - 4, p.y - 4}, {p.x + 4, p.y + 4}, IM_COL32(255, 230, 65, 255));
        // 辺からも、その面の軸方向へサイズを変更できます。
        float distance = std::hypot(io.MousePos.x - p.x, io.MousePos.y - p.y);
        const int otherA = (axis + 1) % 3, otherB = (axis + 2) % 3;
        XMFLOAT3 corners[4];
        for (int n = 0; n < 4; ++n)
        {
            auto v = point;
            (&v.x)[otherA] += ((n == 0 || n == 3) ? -1 : 1) * (&box.size.x)[otherA];
            (&v.x)[otherB] += ((n < 2) ? -1 : 1) * (&box.size.x)[otherB];
            corners[n] = project(v);
        }
        for (int n = 0; n < 4; ++n)
        {
            const auto u = corners[n], v = corners[(n + 1) % 4];
            const float dx = v.x - u.x, dy = v.y - u.y, length = dx * dx + dy * dy;
            if (length < 1 || u.z < 0 || v.z < 0) continue;
            const float t = std::max(0.0f, std::min(1.0f, ((io.MousePos.x - u.x) * dx + (io.MousePos.y - u.y) * dy) / length));
            distance = std::min(distance, std::hypot(io.MousePos.x - u.x - t * dx, io.MousePos.y - u.y - t * dy));
        }
        const float axisLength = std::hypot(screen.x - center.x, screen.y - center.y);
        if (ImGui::IsItemHovered() && !(GetAsyncKeyState(VK_MENU) & 0x8000) &&
            ImGui::IsMouseClicked(0) && distance < 6 && axisLength > 3 && m_dragFace < 0)
        {
            m_dragFace = face; m_playing = false;
            m_dragStartMouse = { io.MouseClickedPos[0].x, io.MouseClickedPos[0].y };
            m_dragStartHalfSize = (&box.size.x)[axis];
            auto unit = XMFLOAT3{box.center.x, box.center.y, box.center.z};
            (&unit.x)[axis] += sign;
            const auto projected = project(unit);
            m_dragScreenAxis = { projected.x - center.x, projected.y - center.y };
        }
    }
    if (m_dragFace >= 0)
    {
        const int axis = m_dragFace / 2;
        const float dx = m_dragScreenAxis.x, dy = m_dragScreenAxis.y, length = dx * dx + dy * dy;
        if (length > 1)
        {
            (&box.size.x)[axis] = std::max(0.0f, m_dragStartHalfSize +
                ((io.MousePos.x - m_dragStartMouse.x) * dx + (io.MousePos.y - m_dragStartMouse.y) * dy) / length);
            if (m_editDefaultBox) a->defaultHurtbox = box;
            else
            {
                auto& key = FrameOverride(*a, state.phaseIndex, static_cast<int>(m_previewFrame));
                if (m_editAttackBox) key.attackBox = box; else key.hurtbox = box;
                m_selectedPhase = state.phaseIndex;
            }
        }
    }
    if (!ImGui::IsMouseDown(0)) m_dragFace = -1;
}

void SceneAttackEditor::DrawPreview()
{
    if (!m_showPreviewWindow) return;
    WindowLayout(0, 0);
    if (!ImGui::Begin(u8"ビュー###AttackView", &m_showPreviewWindow)) { ImGui::End(); return; }
    auto* a = GetSelectedAttack();
    if (a)
    {
        ImVec2 available = ImGui::GetContentRegionAvail();
        available.x = std::max(1.0f, available.x); available.y = std::max(1.0f, available.y);
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        RenderPreview3D(*a, static_cast<unsigned int>(available.x), static_cast<unsigned int>(available.y));
        if (m_previewRenderTarget && m_previewRenderTarget->GetResource())
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(m_previewRenderTarget->GetResource()), available);
            ImGui::SetCursorScreenPos(origin);
            ImGui::InvisibleButton("##ViewInteraction", available, ImGuiButtonFlags_MouseButtonLeft |
                ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
            EditPreviewBox({origin.x, origin.y}, {available.x, available.y});
            if ((ImGui::IsItemHovered() || ImGui::IsItemActive()) && m_dragFace < 0)
            {
                auto& io = ImGui::GetIO();
                if (((GetAsyncKeyState(VK_MENU) & 0x8000) && ImGui::IsMouseDragging(0, 0)) || ImGui::IsMouseDragging(1, 0))
                {
                    m_previewCameraYaw -= io.MouseDelta.x * 0.01f;
                    m_previewCameraPitch = std::max(-1.55f, std::min(1.55f, m_previewCameraPitch + io.MouseDelta.y * 0.01f));
                }
                const DirectX::XMFLOAT3 forward = { -std::sin(m_previewCameraYaw) * std::cos(m_previewCameraPitch),
                    -std::sin(m_previewCameraPitch), -std::cos(m_previewCameraYaw) * std::cos(m_previewCameraPitch) };
                const auto f = DirectX::XMLoadFloat3(&forward);
                const auto right = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMVectorSet(0, 1, 0, 0), f));
                const auto up = DirectX::XMVector3Cross(f, right);
                auto target = DirectX::XMLoadFloat3(&m_previewCameraTarget);
                if (ImGui::IsMouseDragging(2, 0))
                    target = DirectX::XMVectorAdd(target, DirectX::XMVectorAdd(
                        DirectX::XMVectorScale(right, -io.MouseDelta.x * m_previewCameraZoom * 0.0025f),
                        DirectX::XMVectorScale(up, io.MouseDelta.y * m_previewCameraZoom * 0.0025f)));
                if (ImGui::IsMouseDown(1))
                {
                    const float speed = io.DeltaTime * (io.KeyShift ? 8 : 2);
                    const float z = (ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0) - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0);
                    const float x = (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0) - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0);
                    target = DirectX::XMVectorAdd(target, DirectX::XMVectorAdd(DirectX::XMVectorScale(f, z * speed), DirectX::XMVectorScale(right, x * speed)));
                }
                DirectX::XMStoreFloat3(&m_previewCameraTarget, target);
                m_previewCameraZoom = std::max(0.01f, std::min(1000.0f, m_previewCameraZoom * std::pow(0.85f, io.MouseWheel)));
            }
        }
    }
    ImGui::End();
}

void SceneAttackEditor::ResetPreviewCamera()
{
    m_previewCameraYaw = DirectX::XMConvertToRadians(145);
    m_previewCameraPitch = DirectX::XMConvertToRadians(22);
    m_previewCameraZoom = 4.8f; m_previewCameraTarget = {0, 0.65f, 0.45f};
}

void SceneAttackEditor::ReleasePreviewModel()
{
    SAFE_DELETE(m_previewModel); m_previewModelAttempted = false; m_previewModelId.clear();
    m_previewAnimation = -1; m_animationSeconds = 0; m_previewModelCenter = {}; m_previewModelLongestSize = 1;
}

void SceneAttackEditor::ReleasePreviewResources()
{
    ReleasePreviewModel(); SAFE_DELETE(m_previewDepthStencil); SAFE_DELETE(m_previewRenderTarget);
}

void SceneAttackEditor::Draw()
{
    EDITOR_PROFILE_FUNCTION();
    DrawSettingsWindow();
    if (auto* a = GetSelectedAttack())
    {
        EnsurePreviewModel(*a);
        const int maximum = PlaybackFrames();
        if (m_playing)
        {
            m_previewFrame += ImGui::GetIO().DeltaTime * a->framesPerSecond * m_playbackSpeed;
            if (m_previewFrame >= maximum)
            {
                if (m_loopPlayback) m_previewFrame = std::fmod(m_previewFrame, static_cast<float>(maximum));
                else { m_previewFrame = static_cast<float>(maximum) - 0.0001f; m_playing = false; }
            }
        }
        m_previewFrame = std::max(0.0f, std::min(static_cast<float>(maximum) - 0.0001f, m_previewFrame));
    }
    DrawTimelineWindow(); DrawFrameWindow(); DrawPreview(); DrawSaveConfirmation();
}
