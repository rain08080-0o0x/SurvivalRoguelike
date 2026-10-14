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
#include <limits>

#undef min
#undef max

namespace
{
    void AddWireBox(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size,
        const DirectX::XMFLOAT4& color)
    {
        const DirectX::XMFLOAT3 half = {
            std::max(0.0f, size.x) * 0.5f,
            std::max(0.0f, size.y) * 0.5f,
            std::max(0.0f, size.z) * 0.5f };
        const DirectX::XMFLOAT3 corners[8] = {
            { center.x - half.x, center.y - half.y, center.z - half.z },
            { center.x + half.x, center.y - half.y, center.z - half.z },
            { center.x + half.x, center.y + half.y, center.z - half.z },
            { center.x - half.x, center.y + half.y, center.z - half.z },
            { center.x - half.x, center.y - half.y, center.z + half.z },
            { center.x + half.x, center.y - half.y, center.z + half.z },
            { center.x + half.x, center.y + half.y, center.z + half.z },
            { center.x - half.x, center.y + half.y, center.z + half.z } };
        constexpr int edges[12][2] = {
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7} };
        for (const auto& edge : edges) Geometory::AddLine(corners[edge[0]], corners[edge[1]], color);
    }

    std::string AttackDefinitionPath(const NarakuCombat::AttackDefinition& attack)
    {
        if (!attack.sourcePath.empty()) return attack.sourcePath;
        std::string fileName = attack.id;
        for (char& character : fileName)
        {
            const bool allowed = (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') ||
                (character >= '0' && character <= '9') || character == '-' || character == '_';
            if (!allowed) character = '_';
        }
        return "Assets/Data/Combat/" + fileName + ".nattack";
    }

    bool EditString(const char* label, std::string& value, std::size_t capacity = 512)
    {
        std::vector<char> buffer(capacity, 0);
        strncpy_s(buffer.data(), buffer.size(), value.c_str(), _TRUNCATE);
        if (!ImGui::InputText(label, buffer.data(), buffer.size())) return false;
        value = buffer.data();
        return true;
    }

    int RunCombatPublishScript()
    {
        const char* candidates[] = {
            "Tools\\Publish-NarakuCombat.ps1", "..\\Tools\\Publish-NarakuCombat.ps1",
            "..\\..\\..\\Tools\\Publish-NarakuCombat.ps1"};
        for (const char* path : candidates)
        {
            if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES) continue;
            const std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -File \"" +
                std::string(path) + "\" -EditorAssetsPath Assets";
            return std::system(command.c_str());
        }
        return -1;
    }
}

SceneAttackEditor::SceneAttackEditor()
{
    ResetPreviewCamera();
    Reload();
}
SceneAttackEditor::~SceneAttackEditor() { ReleasePreviewResources(); }
void SceneAttackEditor::Update()
{
    EDITOR_PROFILE_FUNCTION();
}

void SceneAttackEditor::Reload()
{
    EDITOR_PROFILE_FUNCTION();
    ReleasePreviewModel();
    std::string error;
    m_catalog = {};
    NarakuCombat::LoadCatalog("Assets/Data/Combat", m_catalog, &error);
    m_selected = m_catalog.attacks.empty() ? -1 : 0;
    m_message = error.empty() ? u8"攻撃データを読み込みました。" : error;
}

void SceneAttackEditor::SaveSelected()
{
    EDITOR_PROFILE_FUNCTION();
    if (m_selected < 0 || m_selected >= static_cast<int>(m_catalog.attacks.size())) return;
    NarakuCombat::AttackDefinition& attack = m_catalog.attacks[static_cast<std::size_t>(m_selected)];
    const std::vector<std::string> issues = NarakuCombat::Validate(m_catalog);
    if (!issues.empty()) { m_message = issues.front(); return; }
    std::string error;
    const std::string path = AttackDefinitionPath(attack);
    if (NarakuCombat::SaveAttack(path, attack, &error))
    {
        attack.sourcePath = path;
        m_message = u8"攻撃データを保存しました。";
    }
    else m_message = error;
}

void SceneAttackEditor::Publish()
{
    EDITOR_PROFILE_FUNCTION();
    const std::vector<std::string> issues = NarakuCombat::Validate(m_catalog);
    if (!issues.empty()) { m_message = u8"検証エラー: " + issues.front(); return; }
    for (NarakuCombat::AttackDefinition& attack : m_catalog.attacks)
    {
        std::string error;
        const std::string path = AttackDefinitionPath(attack);
        if (!NarakuCombat::SaveAttack(path, attack, &error))
        {
            m_message = u8"発行前の保存に失敗しました: " + error;
            return;
        }
        attack.sourcePath = path;
    }
    m_message = RunCombatPublishScript() == 0 ? u8"攻撃データをゲームへ発行しました。" : u8"攻撃データの発行に失敗しました。";
}

void SceneAttackEditor::CreateAttack()
{
    EDITOR_PROFILE_FUNCTION();
    ReleasePreviewModel();
    m_catalog.attacks.push_back({});
    m_selected = static_cast<int>(m_catalog.attacks.size()) - 1;
    m_previewFrame = 0.0f;
    m_message = u8"新しい攻撃データを追加しました。";
}

NarakuCombat::AttackDefinition* SceneAttackEditor::GetSelectedAttack()
{
    if (m_selected < 0 || m_selected >= static_cast<int>(m_catalog.attacks.size())) return nullptr;
    return &m_catalog.attacks[static_cast<std::size_t>(m_selected)];
}

bool SceneAttackEditor::HandleNativeMenuCommand(unsigned int commandId)
{
    EDITOR_PROFILE_FUNCTION();
    switch (commandId)
    {
    case MenuReload: Reload(); return true;
    case MenuSave: SaveSelected(); return true;
    case MenuPublish: Publish(); return true;
    case MenuNewAttack: CreateAttack(); return true;
    case MenuShowStageEditor: SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_EDITOR); return true;
    case MenuShowPieceEditor: SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_PIECE_EDITOR); return true;
    case MenuToggleDataWindow: m_showDataWindow = !m_showDataWindow; return true;
    case MenuToggleBasicWindow: m_showBasicWindow = !m_showBasicWindow; return true;
    case MenuToggleTimelineWindow: m_showTimelineWindow = !m_showTimelineWindow; return true;
    case MenuToggleCombatWindow: m_showCombatWindow = !m_showCombatWindow; return true;
    case MenuToggleHitboxWindow: m_showHitboxWindow = !m_showHitboxWindow; return true;
    case MenuToggleTransformWindow: m_showTransformWindow = !m_showTransformWindow; return true;
    case MenuTogglePreviewWindow: m_showPreviewWindow = !m_showPreviewWindow; return true;
    default: return false;
    }
}

void SceneAttackEditor::SyncNativeMenuState(void* menuHandle) const
{
    EDITOR_PROFILE_FUNCTION();
    HMENU menu = static_cast<HMENU>(menuHandle);
    if (menu == nullptr) return;
    const auto sync = [menu](unsigned int commandId, bool checked)
    {
        CheckMenuItem(menu, commandId, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    };
    sync(MenuToggleDataWindow, m_showDataWindow);
    sync(MenuToggleBasicWindow, m_showBasicWindow);
    sync(MenuToggleTimelineWindow, m_showTimelineWindow);
    sync(MenuToggleCombatWindow, m_showCombatWindow);
    sync(MenuToggleHitboxWindow, m_showHitboxWindow);
    sync(MenuToggleTransformWindow, m_showTransformWindow);
    sync(MenuTogglePreviewWindow, m_showPreviewWindow);
}

void SceneAttackEditor::DrawDataWindow()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showDataWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / データ");
    if (!ImGui::Begin(u8"攻撃データ管理###AttackDataWindow", &m_showDataWindow))
    {
        ImGui::End();
        return;
    }
    if (ImGui::Button(u8"新規作成")) CreateAttack();
    ImGui::SameLine(); if (ImGui::Button(u8"再読み込み")) Reload();
    ImGui::SameLine(); if (ImGui::Button(u8"選択中を保存")) SaveSelected();
    ImGui::SameLine(); if (ImGui::Button(u8"すべて発行")) Publish();
    ImGui::Separator();
    ImGui::TextUnformatted(u8"攻撃データ一覧");
    for (std::size_t i = 0; i < m_catalog.attacks.size(); ++i)
    {
        const NarakuCombat::AttackDefinition& attack = m_catalog.attacks[i];
        const std::string label = attack.displayName.empty() ? attack.id : attack.displayName + " (" + attack.id + ")";
        if (ImGui::Selectable(label.c_str(), m_selected == static_cast<int>(i)))
        {
            if (m_selected != static_cast<int>(i)) ReleasePreviewModel();
            m_selected = static_cast<int>(i);
            m_previewFrame = 0.0f;
        }
    }
    ImGui::Separator();
    ImGui::TextWrapped(u8"状態: %s", m_message.c_str());
    ImGui::End();
}

void SceneAttackEditor::DrawBasicWindow()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showBasicWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / 基本設定");
    if (!ImGui::Begin(u8"基本情報###AttackBasicWindow", &m_showBasicWindow))
    {
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition* selected = GetSelectedAttack();
    if (selected == nullptr)
    {
        ImGui::TextUnformatted(u8"攻撃データを選択してください。");
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition& a = *selected;
    EditString(u8"攻撃ID", a.id);
    EditString(u8"表示名", a.displayName);
    EditString(u8"使用者ID", a.userId);
    int user = static_cast<int>(a.userKind);
    ImGui::Combo(u8"使用者", &user, u8"プレイヤー\0敵\0");
    a.userKind = static_cast<NarakuCombat::UserKind>(user);
    int presentation = static_cast<int>(a.presentation);
    ImGui::Combo(u8"表示方式", &presentation,
        u8"武器変形\0スプライト連番\0" u8"3Dモデルアニメーション\0");
    a.presentation = static_cast<NarakuCombat::PresentationKind>(presentation);
    EditString(u8"モデルID", a.modelId);
    EditString(u8"アニメーションファイル", a.animationPath);
    EditString(u8"アニメーションクリップ", a.animationClip);
    ImGui::End();
}

void SceneAttackEditor::DrawTimelineWindow()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showTimelineWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / タイムライン");
    if (!ImGui::Begin(u8"タイムライン###AttackTimelineWindow", &m_showTimelineWindow))
    {
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition* selected = GetSelectedAttack();
    if (selected == nullptr)
    {
        ImGui::TextUnformatted(u8"攻撃データを選択してください。");
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition& a = *selected;
    ImGui::InputInt(u8"全体フレーム数", &a.totalFrames);
    ImGui::InputInt(u8"フレームレート", &a.framesPerSecond);
    ImGui::Separator();
    ImGui::InputInt(u8"予兆開始", &a.telegraphStartFrame);
    ImGui::InputInt(u8"攻撃判定開始", &a.attackStartFrame);
    ImGui::InputInt(u8"攻撃判定終了", &a.attackEndFrame);
    ImGui::InputInt(u8"パリィ開始", &a.parryStartFrame);
    ImGui::InputInt(u8"パリィ終了", &a.parryEndFrame);
    ImGui::InputInt(u8"移動可能開始", &a.movementStartFrame);
    ImGui::InputInt(u8"移動可能終了", &a.movementEndFrame);
    ImGui::InputInt(u8"方向転換開始", &a.turnStartFrame);
    ImGui::InputInt(u8"方向転換終了", &a.turnEndFrame);
    ImGui::InputInt(u8"コンボ入力開始", &a.comboInputStartFrame);
    ImGui::InputInt(u8"コンボ入力終了", &a.comboInputEndFrame);
    ImGui::InputInt(u8"回避キャンセル開始", &a.dodgeCancelStartFrame);
    ImGui::InputInt(u8"回避キャンセル終了", &a.dodgeCancelEndFrame);
    ImGui::End();
}

void SceneAttackEditor::DrawCombatWindow()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showCombatWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / 戦闘設定");
    if (!ImGui::Begin(u8"戦闘設定###AttackCombatWindow", &m_showCombatWindow))
    {
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition* selected = GetSelectedAttack();
    if (selected == nullptr)
    {
        ImGui::TextUnformatted(u8"攻撃データを選択してください。");
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition& a = *selected;
    EditString(u8"次の攻撃ID", a.nextAttackId);
    ImGui::InputFloat(u8"ダメージ倍率", &a.damageMultiplier);
    ImGui::InputFloat(u8"ノックバック", &a.knockback);
    ImGui::InputFloat(u8"スタミナ消費", &a.staminaCost);
    ImGui::InputFloat(u8"再使用時間（秒）", &a.cooldownSeconds);
    ImGui::Checkbox(u8"壁を貫通", &a.piercesWalls);
    ImGui::Checkbox(u8"敵を貫通", &a.piercesEnemies);
    ImGui::Checkbox(u8"同じ対象には一度だけ命中", &a.singleHitPerTarget);
    ImGui::End();
}

void SceneAttackEditor::DrawHitboxWindow()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showHitboxWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / 攻撃判定");
    if (!ImGui::Begin(u8"攻撃判定###AttackHitboxWindow", &m_showHitboxWindow))
    {
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition* selected = GetSelectedAttack();
    if (selected == nullptr)
    {
        ImGui::TextUnformatted(u8"攻撃データを選択してください。");
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition& a = *selected;
    for (std::size_t i = 0; i < a.hitboxKeys.size(); ++i)
    {
        NarakuCombat::HitboxKey& key = a.hitboxKeys[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text(u8"判定キー %d", static_cast<int>(i + 1));
        ImGui::InputInt(u8"フレーム", &key.frame);
        ImGui::DragFloat3(u8"中心 XYZ", &key.center.x,0.1f);
        ImGui::DragFloat3(u8"大きさ XYZ", &key.size.x, 0.1f);
        if (ImGui::Button(u8"この判定キーを削除"))
        {
            a.hitboxKeys.erase(a.hitboxKeys.begin() + i);
            ImGui::PopID();
            break;
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button(u8"攻撃判定キーを追加")) a.hitboxKeys.push_back({});
    ImGui::End();
}

void SceneAttackEditor::DrawTransformWindow()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showTransformWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / 武器変形");
    if (!ImGui::Begin(u8"武器変形###AttackTransformWindow", &m_showTransformWindow))
    {
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition* selected = GetSelectedAttack();
    if (selected == nullptr)
    {
        ImGui::TextUnformatted(u8"攻撃データを選択してください。");
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition& a = *selected;
    for (std::size_t i = 0; i < a.transformKeys.size(); ++i)
    {
        NarakuCombat::TransformKey& key = a.transformKeys[i];
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text(u8"変形キー %d", static_cast<int>(i + 1));
        ImGui::InputInt(u8"フレーム", &key.frame);
        ImGui::DragFloat3(u8"位置 XYZ", &key.position.x, 0.1f);
        ImGui::DragFloat3(u8"回転角 XYZ", &key.rotationDegrees.x, 0.1f);
        ImGui::DragFloat3(u8"拡大率 XYZ", &key.scale.x, 0.1f);
        if (ImGui::Button(u8"この変形キーを削除"))
        {
            a.transformKeys.erase(a.transformKeys.begin() + i);
            ImGui::PopID();
            break;
        }
        ImGui::Separator();
        ImGui::PopID();
    }
    if (ImGui::Button(u8"武器変形キーを追加")) a.transformKeys.push_back({});
    ImGui::End();
}

bool SceneAttackEditor::EnsurePreviewRenderTarget(unsigned int width, unsigned int height)
{
    EDITOR_PROFILE_FUNCTION();
    width = std::max(1U, width);
    height = std::max(1U, height);
    if (m_previewRenderTarget != nullptr && m_previewDepthStencil != nullptr &&
        m_previewWidth == width && m_previewHeight == height)
    {
        return true;
    }

    SAFE_DELETE(m_previewDepthStencil);
    SAFE_DELETE(m_previewRenderTarget);
    m_previewRenderTarget = new RenderTarget();
    if (FAILED(m_previewRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, width, height)))
    {
        SAFE_DELETE(m_previewRenderTarget);
        return false;
    }
    m_previewDepthStencil = new DepthStencil();
    if (FAILED(m_previewDepthStencil->Create(width, height, false)))
    {
        SAFE_DELETE(m_previewDepthStencil);
        SAFE_DELETE(m_previewRenderTarget);
        return false;
    }
    m_previewWidth = width;
    m_previewHeight = height;
    return true;
}

void SceneAttackEditor::EnsurePreviewModel(const NarakuCombat::AttackDefinition& attack)
{
    EDITOR_PROFILE_FUNCTION();
    if (m_previewModelAttempted && m_previewModelId == attack.modelId) return;
    ReleasePreviewModel();
    m_previewModelAttempted = true;
    m_previewModelId = attack.modelId;
    if (attack.modelId.empty()) return;

    const NarakuCombat::ModelDefinition* definition = NarakuCombat::FindModel(m_catalog, attack.modelId);
    if (definition == nullptr || definition->filePath.empty()) return;
    m_previewModel = new Model();
    if (!m_previewModel->LoadStatic(definition->filePath.c_str(), 1.0f,
        definition->zFlip ? Model::ZFlip : Model::None))
    {
        SAFE_DELETE(m_previewModel);
        return;
    }

    DirectX::XMFLOAT3 minimum = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max() };
    DirectX::XMFLOAT3 maximum = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest() };
    bool hasVertex = false;
    for (unsigned int meshIndex = 0; meshIndex < m_previewModel->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = m_previewModel->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        for (const Model::Vertex& vertex : mesh->vertices)
        {
            minimum.x = std::min(minimum.x, vertex.pos.x);
            minimum.y = std::min(minimum.y, vertex.pos.y);
            minimum.z = std::min(minimum.z, vertex.pos.z);
            maximum.x = std::max(maximum.x, vertex.pos.x);
            maximum.y = std::max(maximum.y, vertex.pos.y);
            maximum.z = std::max(maximum.z, vertex.pos.z);
            hasVertex = true;
        }
    }
    if (!hasVertex)
    {
        SAFE_DELETE(m_previewModel);
        return;
    }
    m_previewModelCenter = {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f };
    m_previewModelLongestSize = std::max(0.001f,
        std::max(maximum.x - minimum.x,
            std::max(maximum.y - minimum.y, maximum.z - minimum.z)));
}

void SceneAttackEditor::RenderPreview3D(const NarakuCombat::AttackDefinition& attack,
    unsigned int width, unsigned int height)
{
    EDITOR_PROFILE_FUNCTION();
    using namespace DirectX;
    if (!EnsurePreviewRenderTarget(width, height)) return;
    EnsurePreviewModel(attack);

    RenderTarget* targets[1] = { m_previewRenderTarget };
    SetRenderTargets(1, targets, m_previewDepthStencil);
    const float clearColor[4] = { 0.035f, 0.045f, 0.060f, 1.0f };
    m_previewRenderTarget->Clear(clearColor);
    m_previewDepthStencil->Clear();
    SetDepthTest(true);
    SetBlendMode(BLEND_NONE);
    SetCullingMode(D3D11_CULL_BACK);

    const float aspect = static_cast<float>(width) / static_cast<float>(std::max(1U, height));
    const XMFLOAT3 look = m_previewCameraTarget;
    const float cosPitch = std::cos(m_previewCameraPitch);
    const XMFLOAT3 eye = {
        look.x + std::sin(m_previewCameraYaw) * cosPitch * m_previewCameraZoom,
        look.y + std::sin(m_previewCameraPitch) * m_previewCameraZoom,
        look.z + std::cos(m_previewCameraYaw) * cosPitch * m_previewCameraZoom };
    const XMMATRIX viewMatrix = XMMatrixLookAtLH(
        XMLoadFloat3(&eye), XMLoadFloat3(&look), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(42.0f), aspect, 0.01f, 100.0f);
    XMFLOAT4X4 world = {};
    XMFLOAT4X4 view = {};
    XMFLOAT4X4 projection = {};
    XMStoreFloat4x4(&world, XMMatrixTranspose(XMMatrixIdentity()));
    XMStoreFloat4x4(&view, XMMatrixTranspose(viewMatrix));
    XMStoreFloat4x4(&projection, XMMatrixTranspose(projectionMatrix));

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);
    for (int line = -4; line <= 4; ++line)
    {
        const float offset = static_cast<float>(line) * 0.5f;
        const XMFLOAT4 color = line == 0
            ? XMFLOAT4{ 0.30f, 0.48f, 0.68f, 1.0f }
            : XMFLOAT4{ 0.16f, 0.19f, 0.24f, 1.0f };
        Geometory::AddLine({ offset, 0.0f, -2.0f }, { offset, 0.0f, 2.0f }, color);
        Geometory::AddLine({ -2.0f, 0.0f, offset }, { 2.0f, 0.0f, offset }, color);
    }
    AddWireBox({ 0.0f, 0.6f, 0.0f }, { 0.5f, 1.2f, 0.5f },
        { 0.30f, 0.70f, 1.0f, 1.0f });
    const NarakuCombat::HitboxKey hitbox = NarakuCombat::EvaluateHitbox(attack, m_previewFrame);
    AddWireBox({ hitbox.center.x, hitbox.center.y, hitbox.center.z },
        { hitbox.size.x, hitbox.size.y, hitbox.size.z },
        { 1.0f, 0.18f, 0.12f, 1.0f });
    Geometory::DrawLines();

    if (m_previewModel != nullptr)
    {
        const NarakuCombat::TransformKey transform = NarakuCombat::EvaluateTransform(attack, m_previewFrame);
        const NarakuCombat::ModelDefinition* definition = NarakuCombat::FindModel(m_catalog, attack.modelId);
        const float definitionScale = definition != nullptr ? definition->scale : 1.0f;
        const float modelScale = 0.6f / m_previewModelLongestSize * definitionScale;
        const XMMATRIX modelBase =
            XMMatrixTranslation(-m_previewModelCenter.x, -m_previewModelCenter.y, -m_previewModelCenter.z) *
            XMMatrixScaling(modelScale * transform.scale.x, modelScale * transform.scale.y,
                modelScale * transform.scale.z);
        const XMMATRIX modelWorld =
            modelBase *
            XMMatrixRotationRollPitchYaw(
                XMConvertToRadians(transform.rotationDegrees.x),
                XMConvertToRadians(transform.rotationDegrees.y),
                XMConvertToRadians(transform.rotationDegrees.z)) *
            XMMatrixTranslation(transform.position.x, transform.position.y, transform.position.z);
        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(modelWorld));
        wvp[1] = view;
        wvp[2] = projection;
        ShaderList::SetWVP(wvp);
        ShaderList::SetLight({ 0.95f, 0.95f, 0.95f, 1.0f }, { -0.35f, -0.85f, 0.30f });
        ShaderList::SetCameraPos(eye);
        m_previewModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        m_previewModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < m_previewModel->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = m_previewModel->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = m_previewModel->GetMaterial(mesh->materialID);
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                material.ambient = { 0.65f, 0.65f, 0.65f, 1.0f };
                ShaderList::SetMaterial(material);
            }
            m_previewModel->Draw(static_cast<int>(meshIndex));
        }
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
}

void SceneAttackEditor::ResetPreviewCamera()
{
    EDITOR_PROFILE_FUNCTION();
    m_previewCameraYaw = DirectX::XMConvertToRadians(145.0f);
    m_previewCameraPitch = DirectX::XMConvertToRadians(22.0f);
    m_previewCameraZoom = 4.8f;
    m_previewCameraTarget = { 0.0f, 0.65f, 0.45f };
}

void SceneAttackEditor::ReleasePreviewModel()
{
    EDITOR_PROFILE_FUNCTION();
    SAFE_DELETE(m_previewModel);
    m_previewModelAttempted = false;
    m_previewModelId.clear();
    m_previewModelCenter = {};
    m_previewModelLongestSize = 1.0f;
}

void SceneAttackEditor::ReleasePreviewResources()
{
    EDITOR_PROFILE_FUNCTION();
    ReleasePreviewModel();
    SAFE_DELETE(m_previewDepthStencil);
    SAFE_DELETE(m_previewRenderTarget);
    m_previewWidth = 0;
    m_previewHeight = 0;
}

void SceneAttackEditor::DrawPreview()
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showPreviewWindow) return;
    EDITOR_PROFILE_WINDOW(u8"Attack Editor / 3Dプレビュー");
    if (!ImGui::Begin(u8"攻撃プレビュー###AttackPreviewWindow", &m_showPreviewWindow))
    {
        ImGui::End();
        return;
    }
    NarakuCombat::AttackDefinition* selected = GetSelectedAttack();
    if (selected != nullptr)
    {
        NarakuCombat::AttackDefinition& a = *selected;
        const float maximum = static_cast<float>(std::max(1, a.totalFrames - 1));
        if (m_playing)
        {
            m_previewFrame += ImGui::GetIO().DeltaTime * std::max(1, a.framesPerSecond);
            if (m_previewFrame > maximum)
            {
                if (m_loopPlayback) m_previewFrame = 0.0f;
                else { m_previewFrame = maximum; m_playing = false; }
            }
        }
        if (ImGui::Button(m_playing ? u8"停止" : u8"再生"))
        {
            if (m_playing) m_playing = false;
            else
            {
                if (m_previewFrame >= maximum) m_previewFrame = 0.0f;
                m_playing = true;
            }
        }
        ImGui::SameLine();
        ImGui::Checkbox(u8"ループ再生", &m_loopPlayback);
        ImGui::SliderFloat(u8"現在フレーム", &m_previewFrame, 0.0f, maximum, "%.1f");
        ImGui::TextUnformatted(u8"青: 攻撃者 / 赤: 攻撃判定");
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const ImVec2 previewSize(std::max(320.0f, available.x), std::max(240.0f, available.y - 55.0f));
        const unsigned int previewWidth = static_cast<unsigned int>(std::min(2048.0f, previewSize.x));
        const unsigned int previewHeight = static_cast<unsigned int>(std::min(2048.0f, previewSize.y));
        RenderPreview3D(a, previewWidth, previewHeight);
        if (m_previewRenderTarget != nullptr && m_previewRenderTarget->GetResource() != nullptr)
        {
            ImGui::Image(reinterpret_cast<ImTextureID>(m_previewRenderTarget->GetResource()), previewSize);
            if (ImGui::IsItemHovered())
            {
                ImGuiIO& io = ImGui::GetIO();
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
                {
                    m_previewCameraYaw -= io.MouseDelta.x * 0.01f;
                    m_previewCameraPitch = std::max(
                        DirectX::XMConvertToRadians(-80.0f),
                        std::min(
                            DirectX::XMConvertToRadians(80.0f),
                            m_previewCameraPitch - io.MouseDelta.y * 0.01f));
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f))
                {
                    const float panSpeed = m_previewCameraZoom * 0.0025f;
                    const DirectX::XMFLOAT3 radial = {
                        std::sin(m_previewCameraYaw) * std::cos(m_previewCameraPitch),
                        std::sin(m_previewCameraPitch),
                        std::cos(m_previewCameraYaw) * std::cos(m_previewCameraPitch) };
                    const DirectX::XMVECTOR forward = DirectX::XMVector3Normalize(
                        DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&radial)));
                    const DirectX::XMVECTOR right = DirectX::XMVector3Normalize(
                        DirectX::XMVector3Cross(
                            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), forward));
                    const DirectX::XMVECTOR up = DirectX::XMVector3Normalize(
                        DirectX::XMVector3Cross(forward, right));
                    DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&m_previewCameraTarget);
                    target = DirectX::XMVectorAdd(
                        target,
                        DirectX::XMVectorAdd(
                            DirectX::XMVectorScale(right, -io.MouseDelta.x * panSpeed),
                            DirectX::XMVectorScale(up, io.MouseDelta.y * panSpeed)));
                    DirectX::XMStoreFloat3(&m_previewCameraTarget, target);
                }
                if (io.MouseWheel != 0.0f)
                {
                    m_previewCameraZoom = std::max(
                        0.75f,
                        std::min(12.0f,
                            m_previewCameraZoom * std::pow(0.85f, io.MouseWheel)));
                }
            }
        }
        else
        {
            ImGui::Dummy(previewSize);
            ImGui::TextUnformatted(u8"3Dプレビューを作成できませんでした。");
        }
        ImGui::TextUnformatted(u8"左ドラッグ: 回転 / 中ドラッグ: 平行移動 / ホイール: ズーム");
        if (ImGui::Button(u8"カメラをリセット")) ResetPreviewCamera();
    }
    ImGui::End();
}

void SceneAttackEditor::Draw()
{
    EDITOR_PROFILE_FUNCTION();
    DrawDataWindow();
    DrawBasicWindow();
    DrawTimelineWindow();
    DrawCombatWindow();
    DrawHitboxWindow();
    DrawTransformWindow();
    DrawPreview();
}
