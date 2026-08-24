#include "SceneNarakuPieceEditor.h"
#include "EditorPerformanceProfiler.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Model.h"
#include "ShaderList.h"
#include "Texture.h"
#include "imgui.h"
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX;
#include "NarakuPieceEditorInternal.h"

void SceneNarakuPieceEditor::DrawEditorWindow()
{
    EDITOR_PROFILE_FUNCTION();
    EDITOR_PROFILE_WINDOW(u8"奈落塔ピースエディタ");
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 260.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"奈落塔ピースエディタ"))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    ImGui::SeparatorText(u8"共通操作");
    ImGui::TextUnformatted(u8"Alt+左ドラッグ: カメラ回転");
    ImGui::TextUnformatted(u8"中ドラッグ: パン");
    ImGui::TextUnformatted(u8"ホイール: ズーム");

    const bool canUndo = m_history.CanUndo();
    const bool canRedo = m_history.CanRedo();
    // 条件に該当する場合は、`ImGui::BeginDisabled` の処理を実行します。
    if (!canUndo)
    {
        ImGui::BeginDisabled();
    }
    // 条件に該当する場合は、`UndoEdit` の処理を実行します。
    if (ImGui::Button(u8"元に戻す"))
    {
        UndoEdit();
    }
    // 条件に該当する場合は、`ImGui::EndDisabled` の処理を実行します。
    if (!canUndo)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`ImGui::BeginDisabled` の処理を実行します。
    if (!canRedo)
    {
        ImGui::BeginDisabled();
    }
    // 条件に該当する場合は、`RedoEdit` の処理を実行します。
    if (ImGui::Button(u8"やり直す"))
    {
        RedoEdit();
    }
    // 条件に該当する場合は、`ImGui::EndDisabled` の処理を実行します。
    if (!canRedo)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::Text("%s %d / %d", u8"履歴",
        static_cast<int>(m_history.GetUndoCount()),
        static_cast<int>(m_history.GetRedoCount()));

    ImGui::SeparatorText(u8"カメラ");
    ImGui::DragFloat(u8"ヨー", &m_cameraYaw, 0.01f);
    ImGui::DragFloat(u8"ピッチ", &m_cameraPitch, 0.01f, kMinCameraPitch, kMaxCameraPitch, "%.3f");
    ImGui::DragFloat(u8"距離", &m_cameraDistance, 0.1f, kMinCameraDistance, kMaxCameraDistance, "%.2f");
    ImGui::DragFloat3(u8"注視点", &m_cameraTarget.x, 0.05f);
    ImGui::Checkbox(u8"Y反転", &m_invertOrbitY);
    // 条件に該当する場合は、`ResetCamera` の処理を実行します。
    if (ImGui::Button(u8"カメラリセット"))
    {
        ResetCamera();
        UpdateCameraMatrices();
    }

    ImGui::End();

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_editMode != EditMode::Height ||
        m_terrainSelectionMode != TerrainSelectionMode::Vertex ||
        !m_showTerrainEditWindow)
    {
        m_heightDragFloatEditing = false;
    }

    DrawPieceBasicWindow();
    DrawPieceConnectionWindow();
    DrawTerrainEditWindow();
    DrawGridObjectPlacementWindow();
    DrawGridObjectSelectionWindow();
    DrawPieceFileAndValidationWindow();
}

void SceneNarakuPieceEditor::DrawPieceBasicWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showPieceBasicWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"基本情報");
    ImGui::SetNextWindowPos(ImVec2(16.0f, 292.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 230.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"基本情報", &m_showPieceBasicWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    char idBuffer[128] = {};
    std::snprintf(idBuffer, sizeof(idBuffer), "%s", m_piece.id.c_str());
    const bool idChanged = ImGui::InputText(u8"ID", idBuffer, sizeof(idBuffer));
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    // 条件に該当する場合は、`m_piece.id` の状態を更新します。
    if (idChanged)
    {
        m_piece.id = idBuffer;
        MarkPieceDirty();
    }

    char displayNameBuffer[128] = {};
    std::snprintf(displayNameBuffer, sizeof(displayNameBuffer), "%s", m_piece.displayName.c_str());
    const bool displayNameChanged = ImGui::InputText(u8"表示名", displayNameBuffer, sizeof(displayNameBuffer));
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    // 条件に該当する場合は、`m_piece.displayName` の状態を更新します。
    if (displayNameChanged)
    {
        m_piece.displayName = displayNameBuffer;
        MarkPieceDirty();
    }

    int abyssLayer = m_piece.abyssLayer;
    const bool abyssLayerChanged = ImGui::DragInt(u8"奈落階層", &abyssLayer, 0.1f, 1, 999);
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    // 条件に該当する場合は、`m_piece.abyssLayer` の状態を更新します。
    if (abyssLayerChanged)
    {
        m_piece.abyssLayer = (abyssLayer < 1) ? 1 : abyssLayer;
        MarkPieceDirty();
    }

    int layerTransitionRole = static_cast<int>(m_piece.layerTransition.role);
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Combo(u8"層間口役割", &layerTransitionRole, kLayerTransitionRoleLabels, IM_ARRAYSIZE(kLayerTransitionRoleLabels)))
    {
        PushUndoSnapshot();
        m_piece.layerTransition.role = static_cast<NarakuPiece::LayerTransitionRole>(layerTransitionRole);
        // 条件に該当する場合は、`m_piece.layerTransition.loadPointEnabled` の状態を更新します。
        if (m_piece.layerTransition.role != NarakuPiece::LayerTransitionRole::Exit)
        {
            m_piece.layerTransition.loadPointEnabled = false;
        }
        MarkPieceDirty();
    }

    ImGui::Text("%s %s", u8"サイズプリセット:", NarakuPiece::ToString(m_piece.sizePreset));
    ImGui::Text("%s %dx%d", u8"グリッド:", m_piece.gridWidth, m_piece.gridDepth);

    int editModeIndex = static_cast<int>(m_editMode);
    // 条件に該当する場合は、`m_editMode` の状態を更新します。
    if (ImGui::Combo(u8"編集モード", &editModeIndex, kEditModeLabels, IM_ARRAYSIZE(kEditModeLabels)))
    {
        m_editMode = static_cast<EditMode>(editModeIndex);
        ClearTerrainSelection();
        ClearGridObjectSelection();
        m_selectedEnvironmentObjectIndex = -1;
        m_hoverCellX = -1;
        m_hoverCellZ = -1;
    }

    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (m_editMode == EditMode::Height)
    {
        int terrainSelectionModeIndex = static_cast<int>(m_terrainSelectionMode);
        // 条件に該当する場合は、`m_terrainSelectionMode` の状態を更新します。
        if (ImGui::Combo(u8"地形選択", &terrainSelectionModeIndex, kTerrainSelectionModeLabels, IM_ARRAYSIZE(kTerrainSelectionModeLabels)))
        {
            m_terrainSelectionMode = static_cast<TerrainSelectionMode>(terrainSelectionModeIndex);
            ClearTerrainSelection();
        }
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawPieceConnectionWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showPieceConnectionWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"接続設定");
    ImGui::SetNextWindowPos(ImVec2(16.0f, 536.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 230.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"接続設定", &m_showPieceConnectionWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    int stageRoleIndex = ToStageRoleIndex(m_piece.stageRole);
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Combo(u8"ステージ役割", &stageRoleIndex, kStageRoleLabels, IM_ARRAYSIZE(kStageRoleLabels)))
    {
        PushUndoSnapshot();
        m_piece.stageRole = FromStageRoleIndex(stageRoleIndex);
        MarkPieceDirty();
    }

    int stageCategoryIndex = ToStageCategoryIndex(m_piece.stageCategory);
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Combo(u8"ステージカテゴリ", &stageCategoryIndex, kStageCategoryLabels, IM_ARRAYSIZE(kStageCategoryLabels)))
    {
        PushUndoSnapshot();
        m_piece.stageCategory = FromStageCategoryIndex(stageCategoryIndex);
        MarkPieceDirty();
    }

    const auto drawEdgeCategoryCombo = [&](const char* label, NarakuPiece::StageCategory& category)
    {
        int edgeIndex = ToStageCategoryIndex(category);
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (ImGui::Combo(label, &edgeIndex, kStageCategoryLabels, IM_ARRAYSIZE(kStageCategoryLabels)))
        {
            PushUndoSnapshot();
            category = FromStageCategoryIndex(edgeIndex);
            MarkPieceDirty();
        }
    };

    ImGui::SeparatorText(u8"接続辺カテゴリ");
    drawEdgeCategoryCombo(u8"北", m_piece.edgeCategories.north);
    drawEdgeCategoryCombo(u8"南", m_piece.edgeCategories.south);
    drawEdgeCategoryCombo(u8"東", m_piece.edgeCategories.east);
    drawEdgeCategoryCombo(u8"西", m_piece.edgeCategories.west);

    ImGui::SeparatorText(u8"接続辺ロック");
    bool northLocked = m_piece.lockedEdges.north;
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Checkbox(u8"北をロック", &northLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.north = northLocked;
        MarkPieceDirty();
    }
    bool southLocked = m_piece.lockedEdges.south;
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Checkbox(u8"南をロック", &southLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.south = southLocked;
        MarkPieceDirty();
    }
    bool eastLocked = m_piece.lockedEdges.east;
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Checkbox(u8"東をロック", &eastLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.east = eastLocked;
        MarkPieceDirty();
    }
    bool westLocked = m_piece.lockedEdges.west;
    // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
    if (ImGui::Checkbox(u8"西をロック", &westLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.west = westLocked;
        MarkPieceDirty();
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawTerrainEditWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showTerrainEditWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"地形編集");
    ImGui::SetNextWindowPos(ImVec2(392.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 340.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"地形編集", &m_showTerrainEditWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    // 条件に該当する場合は、`ImGui::TextUnformatted` の処理を実行します。
    if (m_editMode != EditMode::Height)
    {
        ImGui::TextUnformatted(u8"地形編集モードを高さ編集にすると操作できます。");
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    // 現在の選択単位に対応する地形編集UIだけを描画します。
    if (m_terrainSelectionMode == TerrainSelectionMode::Vertex)
    {
        DrawVertexTerrainControls();
    }
    else
    {
        DrawCellTerrainControls();
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawVertexTerrainControls()
{
    EDITOR_PROFILE_FUNCTION();
    ImGui::TextUnformatted(u8"左クリック: 単一選択");
    ImGui::TextUnformatted(u8"左ドラッグ: 選択頂点の高さ編集");
    ImGui::TextUnformatted(u8"Shift+左クリック/左ドラッグ: 追加選択");
    ImGui::TextUnformatted(u8"Ctrl+左クリック/左ドラッグ: トグル選択");
    ImGui::Checkbox(u8"高さグリッドを表示", &m_showHeightGridWindow);
    ImGui::Text("%s (%d, %d)", u8"主選択頂点", m_selectedX, m_selectedZ);
    ImGui::Text("%s %d", u8"選択数", static_cast<int>(m_selectedVertices.size()));

    float selectedHeight = GetHeight(m_selectedX, m_selectedZ);
    // 高さ入力のドラッグ開始時に一度だけUndo状態を保存します。
    if (ImGui::DragFloat(u8"選択頂点の高さ", &selectedHeight, 0.05f, -10.0f, 10.0f, "%.2f"))
    {
        // 同じドラッグ操作で履歴を重複作成しないよう開始時だけ保存します。
        if (!m_heightDragFloatEditing)
        {
            PushUndoSnapshot();
            m_heightDragFloatEditing = true;
        }
        SetSelectedVerticesHeight(selectedHeight);
    }
    // 入力終了後は次のドラッグ操作を新しい編集として扱います。
    if (ImGui::IsItemDeactivatedAfterEdit())
    {
        m_heightDragFloatEditing = false;
    }
    // 項目が操作中でない場合もドラッグ状態を確実に解除します。
    else if (!ImGui::IsItemActive())
    {
        m_heightDragFloatEditing = false;
    }

    // 主選択だけを残す操作が押された時に複数選択を整理します。
    if (ImGui::Button(u8"主選択だけにする"))
    {
        KeepOnlyActiveVertexSelected();
    }
    // 選択頂点のリセット操作を履歴付きで実行します。
    if (ImGui::Button(u8"選択頂点を0に戻す"))
    {
        ResetSelectedVertexHeights();
    }
    // 全頂点のリセット操作を履歴付きで実行します。
    if (ImGui::Button(u8"全頂点を0に戻す"))
    {
        ResetAllVertexHeights();
    }
}

void SceneNarakuPieceEditor::DrawCellTerrainControls()
{
    EDITOR_PROFILE_FUNCTION();
    m_heightDragFloatEditing = false;
    ImGui::TextUnformatted(u8"左クリック: 単一選択");
    ImGui::TextUnformatted(u8"Shift+左クリック: 追加選択");
    ImGui::TextUnformatted(u8"Ctrl+左クリック: トグル選択");
    ImGui::TextUnformatted(u8"Shift+左ドラッグ: 範囲選択");
    ImGui::TextUnformatted(u8"Delete: 選択セルを削除");
    ImGui::Text("%s (%d, %d)", u8"主選択セル", m_selectedCellX, m_selectedCellZ);
    ImGui::Text("%s %d", u8"選択数", static_cast<int>(m_selectedCells.size()));

    // セルが一つも選択されていない場合は属性UIを表示しません。
    if (m_selectedCells.empty())
    {
        return;
    }

    const NarakuPiece::CellData* const primaryCell = GetCellData(m_selectedCellX, m_selectedCellZ);
    // 主選択セルが有効範囲外の場合は属性参照を行いません。
    if (primaryCell == nullptr)
    {
        return;
    }
    DrawSelectedCellControls(*primaryCell);
}

void SceneNarakuPieceEditor::DrawSelectedCellControls(const NarakuPiece::CellData& primaryCell)
{
    EDITOR_PROFILE_FUNCTION();
    bool deleted = primaryCell.deleted;
    // 削除状態の変更を選択中の全セルへ反映します。
    if (ImGui::Checkbox(u8"削除済みセル", &deleted))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::deleted, deleted);
    }

    bool walkable = primaryCell.walkable;
    // 歩行可否の変更を選択中の全セルへ反映します。
    if (ImGui::Checkbox(u8"歩行可能", &walkable))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::walkable, walkable);
    }

    bool ropeAllowed = primaryCell.ropeAllowed;
    // ロープ設置可否の変更を選択中の全セルへ反映します。
    if (ImGui::Checkbox(u8"ロープ設置可", &ropeAllowed))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::ropeAllowed, ropeAllowed);
    }

    bool miningAllowed = primaryCell.miningAllowed;
    // 採掘ポイント設置可否の変更を選択中の全セルへ反映します。
    if (ImGui::Checkbox(u8"採掘ポイント設置可", &miningAllowed))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::miningAllowed, miningAllowed);
    }

    bool enemySpawnAllowed = primaryCell.enemySpawnAllowed;
    // 敵スポーン可否の変更を選択中の全セルへ反映します。
    if (ImGui::Checkbox(u8"敵スポーン可", &enemySpawnAllowed))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::enemySpawnAllowed, enemySpawnAllowed);
    }

    int groundTextureId = std::max(0, primaryCell.groundTextureId);
    // 地面テクスチャIDの変更を選択中の全セルへ反映します。
    if (ImGui::DragInt(u8"地面テクスチャID", &groundTextureId, 0.1f, 0, 999))
    {
        ApplySelectedCellGroundTextureId(groundTextureId);
    }

    // 削除ボタンで選択中の全セルを削除状態へ変更します。
    if (ImGui::Button(u8"削除"))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::deleted, true);
    }
    ImGui::SameLine();
    // 復元ボタンで選択中の全セルを有効状態へ戻します。
    if (ImGui::Button(u8"復元"))
    {
        ApplySelectedCellFlag(&NarakuPiece::CellData::deleted, false);
    }
}

void SceneNarakuPieceEditor::DrawGridObjectPlacementWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showGridObjectPlacementWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"配置ツール");
    ImGui::SetNextWindowPos(ImVec2(16.0f, 780.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 190.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"配置ツール", &m_showGridObjectPlacementWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    // 条件に該当する場合は、`ImGui::TextUnformatted` の処理を実行します。
    if (m_editMode != EditMode::GridObject)
    {
        ImGui::TextUnformatted(u8"編集モードをゲームオブジェクト配置にすると操作できます。");
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    int toolIndex = static_cast<int>(m_gridObjectTool);
    // 条件に該当する場合は、`m_gridObjectTool` の状態を更新します。
    if (ImGui::Combo(u8"配置ツール", &toolIndex, kGridObjectToolLabels, IM_ARRAYSIZE(kGridObjectToolLabels)))
    {
        m_gridObjectTool = static_cast<GridObjectTool>(toolIndex);
    }
    ImGui::TextUnformatted(u8"左クリックでセルにゲームオブジェクトを配置または選択します。");
    ImGui::TextUnformatted(u8"採掘ポイントは同一セルに重複配置できません。");
    ImGui::TextUnformatted(u8"ロープ上端と下端はロープ用セルに配置します。");
    ImGui::TextUnformatted(u8"開始帰還候補は候補セルに配置します。");
    ImGui::Text("%s (%d, %d)", u8"ホバーセル", m_hoverCellX, m_hoverCellZ);

    // 条件に該当する場合は、`ImGui::SeparatorText` の処理を実行します。
    if (m_gridObjectTool == GridObjectTool::MiningPoint)
    {
        ImGui::SeparatorText(u8"新規採掘ポイント設定");
        ImGui::SliderInt(u8"見た目タイプ", &m_newMiningVisualType, 0, 3);
        ImGui::Checkbox(u8"初回採取済み", &m_newMiningInitiallyRecorded);
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawGridObjectSelectionWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showGridObjectSelectionWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"選択オブジェクト");
    ImGui::SetNextWindowPos(ImVec2(392.0f, 372.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 280.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"選択オブジェクト", &m_showGridObjectSelectionWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    // 値の種類に対応する処理を選択します。
    switch (m_selectedGridObjectKind)
    {
    case GridObjectKind::MiningPoint:
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (m_selectedMiningPointIndex >= 0 &&
            m_selectedMiningPointIndex < static_cast<int>(m_piece.miningPoints.size()))
        {
            NarakuPiece::MiningPointData& point = m_piece.miningPoints[static_cast<size_t>(m_selectedMiningPointIndex)];
            ImGui::Text("%s (%d, %d)", u8"採掘ポイント", point.cell.x, point.cell.z);

            char miningIdBuffer[128] = {};
            std::snprintf(miningIdBuffer, sizeof(miningIdBuffer), "%s", point.id.c_str());
            const bool miningIdChanged = ImGui::InputText(u8"ID", miningIdBuffer, sizeof(miningIdBuffer));
            // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            // 条件に該当する場合は、`point.id` の状態を更新します。
            if (miningIdChanged)
            {
                point.id = miningIdBuffer;
                MarkPieceDirty();
            }

            int visualType = point.visualType;
            // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
            if (ImGui::SliderInt(u8"見た目タイプ", &visualType, 0, 3))
            {
                PushUndoSnapshot();
                point.visualType = visualType;
                MarkPieceDirty();
            }

            bool initiallyRecorded = point.initiallyRecorded;
            // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
            if (ImGui::Checkbox(u8"初期記録済み", &initiallyRecorded))
            {
                PushUndoSnapshot();
                point.initiallyRecorded = initiallyRecorded;
                MarkPieceDirty();
            }

            // 条件に該当する場合は、`DeleteSelectedGridObject` の処理を実行します。
            if (ImGui::Button(u8"採掘ポイントを削除"))
            {
                DeleteSelectedGridObject();
            }
        }
        else
        {
            ClearGridObjectSelection();
            ImGui::TextUnformatted(u8"採掘ポイントが未選択です");
        }
        break;

    case GridObjectKind::Rope:
    {
        bool enabled = m_piece.rope.enabled;
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (ImGui::Checkbox(u8"ロープを有効化", &enabled))
        {
            PushUndoSnapshot();
            m_piece.rope.enabled = enabled;
            MarkPieceDirty();
        }
        ImGui::Text("%s (%d, %d)", u8"ロープ上端", m_piece.rope.top.x, m_piece.rope.top.z);
        ImGui::Text("%s (%d, %d)", u8"ロープ下端", m_piece.rope.bottom.x, m_piece.rope.bottom.z);
        // 条件に該当する場合は、`DeleteSelectedGridObject` の処理を実行します。
        if (ImGui::Button(u8"ロープを削除"))
        {
            DeleteSelectedGridObject();
        }
        break;
    }

    case GridObjectKind::StartReturn:
    {
        bool enabled = m_piece.startReturnCandidate.enabled;
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (ImGui::Checkbox(u8"開始帰還候補を有効化", &enabled))
        {
            PushUndoSnapshot();
            m_piece.startReturnCandidate.enabled = enabled;
            MarkPieceDirty();
        }
        ImGui::Text("%s (%d, %d)", u8"セル", m_piece.startReturnCandidate.cell.x, m_piece.startReturnCandidate.cell.z);
        int facingIndex = ToDirectionIndex(m_piece.startReturnCandidate.facing);
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (ImGui::Combo(u8"向き", &facingIndex, kDirectionLabels, IM_ARRAYSIZE(kDirectionLabels)))
        {
            PushUndoSnapshot();
            m_piece.startReturnCandidate.facing = FromDirectionIndex(facingIndex);
            MarkPieceDirty();
        }
        // 条件に該当する場合は、`DeleteSelectedGridObject` の処理を実行します。
        if (ImGui::Button(u8"開始帰還候補を削除"))
        {
            DeleteSelectedGridObject();
        }
        break;
    }

    case GridObjectKind::LayerRopePoint:
        ImGui::Text("%s (%d, %d)", u8"層間口ロープ端点", m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z);
        // 条件に該当する場合は、現在の繰り返し処理を終了します。
        if (ImGui::Button(u8"層間口ロープ端点を削除")) DeleteSelectedGridObject();
        break;

    case GridObjectKind::LayerLoadPoint:
        ImGui::Text("%s (%d, %d)", u8"層間口ロード地点", m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z);
        // 条件に該当する場合は、現在の繰り返し処理を終了します。
        if (ImGui::Button(u8"層間口ロード地点を削除")) DeleteSelectedGridObject();
        break;

    case GridObjectKind::None:
    default:
        ImGui::TextUnformatted(u8"ゲームオブジェクトが未選択です");
        break;
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawEnvironmentAssetsWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`EDITOR_PROFILE_WINDOW` の処理を実行します。
    if (!m_showEnvironmentAssetsWindow) return;

    EDITOR_PROFILE_WINDOW(u8"Assets");
    ImGui::SetNextWindowPos(ImVec2(768.0f, 668.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460.0f, 330.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`ImGui::End` の処理を実行します。
    if (!ImGui::Begin("Assets", &m_showEnvironmentAssetsWindow))
    {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat(u8"表示サイズ", &m_environmentAssetTileSize, 72.0f, 160.0f, "%.0f px");
    ImGui::Separator();

    // 条件に該当する場合は、`ImGui::TextUnformatted` の処理を実行します。
    if (m_environmentModels.empty())
    {
        ImGui::TextUnformatted(u8"登録モデルなし");
    }
    else
    {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float tileSize = std::max(48.0f, std::min(m_environmentAssetTileSize, availableWidth));
        const int columnCount = std::max(1, static_cast<int>((availableWidth + spacing) / (tileSize + spacing)));
        const float padding = 5.0f;
        // 条件に該当する場合は、`for` の処理を実行します。
        if (ImGui::BeginTable("EnvironmentAssetTiles", columnCount, ImGuiTableFlags_SizingStretchSame))
        {
            // 指定した範囲を順に走査し、対象要素を処理します。
            for (size_t index = 0; index < m_environmentModels.size(); ++index)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(index));
                const bool selected = m_selectedEnvironmentModelIndex == static_cast<int>(index);
                const ImVec2 tileMin = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##EnvironmentAssetTile", ImVec2(tileSize, tileSize));
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 tileMax(tileMin.x + tileSize, tileMin.y + tileSize);
                const ImU32 backgroundColor = selected
                    ? IM_COL32(48, 102, 156, 255)
                    : hovered ? IM_COL32(47, 55, 67, 255) : IM_COL32(31, 36, 45, 255);
                const ImU32 borderColor = selected ? IM_COL32(125, 200, 255, 255) : IM_COL32(76, 85, 101, 255);
                drawList->AddRectFilled(tileMin, tileMax, backgroundColor, 6.0f);
                drawList->AddRect(tileMin, tileMax, borderColor, 6.0f, 0, selected ? 2.0f : 1.0f);

                const ImVec2 imageMin(tileMin.x + padding, tileMin.y + padding);
                const ImVec2 imageMax(tileMax.x - padding, tileMax.y - 25.0f);
                void* textureId = GetEnvironmentModelThumbnailTextureId(static_cast<int>(index), 128U);
                // 条件に該当する場合は、対応する編集処理を実行します。
                if (textureId != nullptr)
                {
                    drawList->AddImage(textureId, imageMin, imageMax);
                }
                else
                {
                    drawList->AddRectFilled(imageMin, imageMax, IM_COL32(22, 26, 33, 255), 4.0f);
                }

                const std::string& name = m_environmentModels[index].name;
                const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
                const float textX = textSize.x <= tileSize - padding * 2.0f
                    ? tileMin.x + (tileSize - textSize.x) * 0.5f
                    : tileMin.x + padding;
                drawList->PushClipRect(
                    ImVec2(tileMin.x + padding, tileMax.y - 23.0f),
                    ImVec2(tileMax.x - padding, tileMax.y - 3.0f),
                    true);
                drawList->AddText(ImVec2(textX, tileMax.y - 20.0f), IM_COL32(235, 239, 245, 255), name.c_str());
                drawList->PopClipRect();

                // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
                if (hovered) ImGui::SetTooltip("%s", name.c_str());
                // 条件に該当する場合は、`m_selectedEnvironmentModelIndex` の状態を更新します。
                if (clicked)
                {
                    m_selectedEnvironmentModelIndex = static_cast<int>(index);
                    m_editMode = EditMode::EnvironmentObject;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_selectedEnvironmentModelIndex >= 0 && m_selectedEnvironmentModelIndex < static_cast<int>(m_environmentModels.size()))
    {
        const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
        ImGui::SeparatorText(u8"選択モデル");
        ImGui::TextUnformatted(asset.name.c_str());
        ImGui::TextWrapped("%s", asset.path.c_str());
        ImGui::Text("%s %.3f, %.3f, %.3f", u8"既定サイズ", asset.defaultScale.x, asset.defaultScale.y, asset.defaultScale.z);
    }

    ImGui::SeparatorText(u8"配置オブジェクト");
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_selectedEnvironmentObjectIndex >= 0 &&
        m_selectedEnvironmentObjectIndex < static_cast<int>(m_piece.environmentObjects.size()))
    {
        NarakuPiece::EnvironmentObjectData& object = m_piece.environmentObjects[m_selectedEnvironmentObjectIndex];
        const int assetIndex = FindEnvironmentModelIndexById(object.modelId);
        const char* modelName = assetIndex >= 0 ? m_environmentModels[assetIndex].name.c_str() : object.modelId.c_str();
        ImGui::Text("%s: %s", u8"モデル", modelName);
        ImGui::Text("%s: (%d, %d)", u8"セル", object.cell.x, object.cell.z);
        float scale[3] = { object.scaleX, object.scaleY, object.scaleZ };
        const bool scaleChanged = ImGui::DragFloat3(u8"サイズ", scale, 0.01f, 0.01f, 100.0f, "%.3f");
        // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
        if (ImGui::IsItemActivated()) PushUndoSnapshot();
        // 条件に該当する場合は、`object.scaleX` の状態を更新します。
        if (scaleChanged)
        {
            object.scaleX = std::max(0.01f, scale[0]);
            object.scaleY = std::max(0.01f, scale[1]);
            object.scaleZ = std::max(0.01f, scale[2]);
            MarkPieceDirty();
        }
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (ImGui::Button(u8"環境オブジェクトを削除"))
        {
            PushUndoSnapshot();
            m_piece.environmentObjects.erase(m_piece.environmentObjects.begin() + m_selectedEnvironmentObjectIndex);
            m_selectedEnvironmentObjectIndex = -1;
            MarkPieceDirty();
            SetMessage(u8"環境オブジェクトを削除しました");
        }
    }
    else
    {
        ImGui::TextUnformatted(u8"配置オブジェクトが未選択です");
    }
    ImGui::End();
}

void SceneNarakuPieceEditor::DrawEnvironmentModelPopup()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`ImGui::OpenPopup` の処理を実行します。
    if (m_requestOpenEnvironmentModelPopup)
    {
        ImGui::OpenPopup(u8"環境モデル設定");
        m_requestOpenEnvironmentModelPopup = false;
    }
    // 条件に該当する場合は、`ReleaseEnvironmentModelPopupPreview` の処理を実行します。
    if (!ImGui::BeginPopupModal(u8"環境モデル設定", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ReleaseEnvironmentModelPopupPreview();
        return;
    }
    EDITOR_PROFILE_WINDOW(u8"環境モデル設定");

    ImGui::InputText(u8"モデル名", m_environmentModelNameInput.data(), m_environmentModelNameInput.size());
    ImGui::InputText(u8"モデルパス", m_environmentModelPathInput.data(), m_environmentModelPathInput.size(), ImGuiInputTextFlags_ReadOnly);
    ImGui::DragFloat3(u8"既定サイズ", &m_environmentModelScaleInput.x, 0.01f, 0.01f, 100.0f, "%.3f");
    ImGui::SeparatorText(u8"サイズプレビュー");
    constexpr unsigned int previewSize = 320U;
    // 条件に該当する場合は、`ImGui::Image` の処理を実行します。
    if (void* textureId = GetEnvironmentModelPopupPreviewTextureId(previewSize))
    {
        ImGui::Image(textureId, ImVec2(static_cast<float>(previewSize), static_cast<float>(previewSize)));
    }
    else
    {
        ImGui::Dummy(ImVec2(static_cast<float>(previewSize), 1.0f));
        ImGui::TextUnformatted(u8"モデルを表示できません");
    }
    // 条件に該当する場合は、`ApplyEnvironmentModelPopup` の処理を実行します。
    if (ImGui::Button(m_environmentModelPopupIsNew ? u8"追加" : u8"更新"))
    {
        ApplyEnvironmentModelPopup();
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`ImGui::EndPopup` の処理を実行します。
    if (ImGui::Button(u8"キャンセル")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void SceneNarakuPieceEditor::DrawPieceFileAndValidationWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showPieceFileAndValidationWindow)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"保存・検証");
    ImGui::SetNextWindowPos(ImVec2(768.0f, 372.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 280.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"保存・検証", &m_showPieceFileAndValidationWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    // 条件に該当する場合は、`RefreshValidationIssues` の処理を実行します。
    if (ImGui::Button(u8"検証を実行"))
    {
        RefreshValidationIssues();
    }

    ImGui::SeparatorText(u8"検証結果");
    // 条件に該当する場合は、`ImGui::TextUnformatted` の処理を実行します。
    if (m_validationIssues.empty())
    {
        ImGui::TextUnformatted(u8"問題はありません");
    }
    else
    {
        // 対象コレクションの各要素を順に処理します。
        for (const NarakuPiece::ValidationIssue& issue : m_validationIssues)
        {
            ImVec4 color = ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
            // 条件に該当する場合は、`color` の状態を更新します。
            if (issue.severity == NarakuPiece::ValidationIssue::Severity::Warning)
            {
                color = ImVec4(0.95f, 0.80f, 0.15f, 1.0f);
            }
            // 先の条件に該当せず、この条件を満たす場合は、`color` の状態を更新します。
            else if (issue.severity == NarakuPiece::ValidationIssue::Severity::Error)
            {
                color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            }

            ImGui::TextColored(color, "[%s] %s", GetSeverityLabel(issue.severity), issue.message.c_str());
        }
    }

    ImGui::SeparatorText(u8"メッセージ");
    ImGui::TextWrapped("%s", m_message.empty() ? u8"メッセージはありません" : m_message.c_str());

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawNewPiecePopup()
{
    EDITOR_PROFILE_FUNCTION();
    bool keepOpen = true;
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!ImGui::BeginPopupModal(u8"新規ピースを作成", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    EDITOR_PROFILE_WINDOW(u8"新規ピースを作成");

    ImGui::TextUnformatted(u8"新規ファイル名");
    ImGui::SetNextItemWidth(320.0f);
    // 条件に該当する場合は、`CommitNewPieceFileNameInput` の処理を実行します。
    if (ImGui::InputText(u8"##NewPieceFileName", m_newPieceFileNameInput.data(), m_newPieceFileNameInput.size()))
    {
        CommitNewPieceFileNameInput();
    }

    // 条件に該当する場合は、`CommitNewPieceFileNameInput` の処理を実行します。
    if (ImGui::Button(u8"作成", ImVec2(120.0f, 0.0f)))
    {
        CommitNewPieceFileNameInput();
        // 条件に該当する場合は、`SetMessage` の処理を実行します。
        if (m_newPieceFileName.empty())
        {
            SetMessage(u8"新規ファイル名を入力してください");
        }
        // 先の条件に該当せず、この条件を満たす場合は、`SetMessage` の処理を実行します。
        else if (HasInvalidFileNameChar(m_newPieceFileName))
        {
            SetMessage(u8"新規ファイル名に使用できない文字が含まれています");
        }
        else
        {
            CreateNewPiece(m_newPieceFileName);
            SetMessage(u8"新規ピースを作成しました");
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`ImGui::CloseCurrentPopup` の処理を実行します。
    if (ImGui::Button(u8"キャンセル", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void SceneNarakuPieceEditor::DrawSavePiecePopup()
{
    EDITOR_PROFILE_FUNCTION();
    bool keepOpen = true;
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!ImGui::BeginPopupModal(u8"ピースを保存", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    EDITOR_PROFILE_WINDOW(u8"ピースを保存");

    ImGui::TextUnformatted(u8"保存ファイル名");
    ImGui::SetNextItemWidth(320.0f);
    // 条件に該当する場合は、`CommitSaveFileNameInput` の処理を実行します。
    if (ImGui::InputText(u8"##SavePieceFileName", m_saveFileNameInput.data(), m_saveFileNameInput.size()))
    {
        CommitSaveFileNameInput();
    }

    // 条件に該当する場合は、`m_saveAsDraft` の状態を更新します。
    if (ImGui::RadioButton(u8"下書き保存", m_saveAsDraft))
    {
        m_saveAsDraft = true;
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`m_saveAsDraft` の状態を更新します。
    if (ImGui::RadioButton(u8"完成保存", !m_saveAsDraft))
    {
        m_saveAsDraft = false;
    }

    const std::wstring saveTargetPath = GetCurrentSaveTargetPath();
    ImGui::SeparatorText(u8"保存先");
    ImGui::TextWrapped("%s", WideToUtf8(saveTargetPath).c_str());

    // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
    if (ImGui::Button(u8"保存", ImVec2(120.0f, 0.0f)))
    {
        // 条件に該当する場合は、`ImGui::CloseCurrentPopup` の処理を実行します。
        if (SavePiece(m_saveAsDraft))
        {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`ImGui::CloseCurrentPopup` の処理を実行します。
    if (ImGui::Button(u8"キャンセル", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void SceneNarakuPieceEditor::DrawRenamePiecePopup()
{
    EDITOR_PROFILE_FUNCTION();
    bool keepOpen = true;
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!ImGui::BeginPopupModal(u8"ピース名を変更", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }
    EDITOR_PROFILE_WINDOW(u8"ピース名を変更");

    ImGui::TextUnformatted(u8"新しいファイル名");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputText(u8"##RenamePieceFileName", m_renameFileNameInput.data(), m_renameFileNameInput.size());

    // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
    if (ImGui::Button(u8"変更", ImVec2(120.0f, 0.0f)))
    {
        // 条件に該当する場合は、`ImGui::CloseCurrentPopup` の処理を実行します。
        if (RenameCurrentPiece())
        {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    // 条件に該当する場合は、`ImGui::CloseCurrentPopup` の処理を実行します。
    if (ImGui::Button(u8"キャンセル", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}


void SceneNarakuPieceEditor::DrawHeightGridWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showHeightGridWindow || m_terrainSelectionMode != TerrainSelectionMode::Vertex)
    {
        return;
    }

    EDITOR_PROFILE_WINDOW(u8"高さグリッド");
    ImGui::SetNextWindowPos(ImVec2(1204.0f, 332.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 360.0f), ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageSize` の状態を更新します。
    if (!ImGui::Begin(u8"高さグリッド", &m_showHeightGridWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::TextUnformatted(u8"Ctrl+クリック: トグル / Shift+クリック: 追加");
    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool shiftPressed = IsEditorShiftPressed(io);

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            const bool isPrimarySelected = (x == m_selectedX && z == m_selectedZ);
            const bool isMultiSelected = IsVertexSelected(x, z);
            const float height = GetHeight(x, z);
            const bool hasHeight = std::fabs(height) > 0.001f;

            ImVec4 buttonColor = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
            ImVec4 hoveredColor = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
            ImVec4 activeColor = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
            // 条件に該当する場合は、`buttonColor` の状態を更新します。
            if (hasHeight)
            {
                buttonColor = ImVec4(0.20f, 0.35f, 0.30f, 1.0f);
                hoveredColor = ImVec4(0.25f, 0.45f, 0.38f, 1.0f);
                activeColor = ImVec4(0.28f, 0.52f, 0.43f, 1.0f);
            }
            // 条件に該当する場合は、`buttonColor` の状態を更新します。
            if (isMultiSelected)
            {
                buttonColor = ImVec4(0.22f, 0.42f, 0.78f, 1.0f);
                hoveredColor = ImVec4(0.30f, 0.50f, 0.88f, 1.0f);
                activeColor = ImVec4(0.18f, 0.36f, 0.70f, 1.0f);
            }
            // 条件に該当する場合は、`buttonColor` の状態を更新します。
            if (isPrimarySelected)
            {
                buttonColor = ImVec4(0.85f, 0.55f, 0.15f, 1.0f);
                hoveredColor = ImVec4(0.92f, 0.64f, 0.22f, 1.0f);
                activeColor = ImVec4(0.98f, 0.72f, 0.28f, 1.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

            char buttonLabel[16] = {};
            std::snprintf(buttonLabel, sizeof(buttonLabel), "%d,%d", x, z);
            // 条件に該当する場合は、`SelectVertexFromInput` の処理を実行します。
            if (ImGui::Button(buttonLabel, ImVec2(44.0f, 24.0f)))
            {
                SelectVertexFromInput(x, z, ctrlPressed, shiftPressed);
            }

            ImGui::PopStyleColor(3);
            // 条件に該当する場合は、`ImGui::SameLine` の処理を実行します。
            if (x + 1 < m_piece.gridWidth)
            {
                ImGui::SameLine();
            }
        }
    }

    ImGui::End();
}


