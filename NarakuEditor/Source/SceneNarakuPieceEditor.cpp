#include "SceneNarakuPieceEditor.h"
#include "EditorPerformanceProfiler.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Model.h"
#include "NarakuMapData.h"
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
#include <cstdlib>
#include <queue>

using namespace DirectX;
#include "NarakuPieceEditorInternal.h"

namespace
{
    int RunPiecePublishScript(const char* action)
    {
        const char* candidates[] =
        {
            "Tools\\Publish-NarakuPieces.ps1",
            "..\\Tools\\Publish-NarakuPieces.ps1",
            "..\\..\\..\\Tools\\Publish-NarakuPieces.ps1",
        };

        for (const char* scriptPath : candidates)
        {
            if (GetFileAttributesA(scriptPath) == INVALID_FILE_ATTRIBUTES)
            {
                continue;
            }

            const std::string command =
                "powershell -NoProfile -ExecutionPolicy Bypass -File \"" +
                std::string(scriptPath) + "\" -Action " + action + " -EditorAssetsPath Assets";
            return std::system(command.c_str());
        }

        return -1;
    }
}

SceneNarakuPieceEditor::SceneNarakuPieceEditor()
    : m_piece(NarakuPiece::CreateDefaultPiece(NarakuPiece::SizePreset::Size16x16))
    , m_selectedX(0)
    , m_selectedZ(0)
    , m_selectedVertices{ VertexSelection{ 0, 0 } }
    , m_selectedCellX(0)
    , m_selectedCellZ(0)
    , m_selectedCells{ CellSelection{ 0, 0 } }
    , m_saveFileName(L"piece_0001.json")
    , m_validationDirty(true)
{
    EDITOR_PROFILE_FUNCTION();
    SyncSaveFileNameInput();
    ReloadPieceHierarchyEntries();
    LoadEnvironmentModelCatalog();
    LoadBasePreviewModels();
    InitializeWaterOverlayPreview();
    UpdateMainWindowTitle();
    ResetCamera();
    UpdateCameraMatrices();
    RefreshValidationIssues();
}

SceneNarakuPieceEditor::~SceneNarakuPieceEditor()
{
    EDITOR_PROFILE_FUNCTION();
    ReleaseWaterOverlayPreview();
    ReleaseBasePreviewModels();
    ReleaseSurfaceFacilityPreviewModel();
    ReleaseEnvironmentModelPopupPreview();
    ReleaseEnvironmentModels();
    ReleasePreviewRenderTarget();
}

void SceneNarakuPieceEditor::Update()
{
    EDITOR_PROFILE_FUNCTION();
    HandleUndoRedoShortcuts();
    ImGuiIO& io = ImGui::GetIO();
    const bool deletePressed = !io.WantTextInput && !io.WantCaptureKeyboard && IsAsyncModifierPressed(VK_DELETE);
    const bool deleteTriggered = IsShortcutTriggered(deletePressed, m_prevDeletePressed);
    UpdateCamera();

    // 現在の編集モードに対応する入力処理だけを実行します。
    if (m_editMode == EditMode::Height)
    {
        UpdateHeightMode(deleteTriggered);
    }
    // 先の条件に該当せず、この条件を満たす場合は、`UpdateGridObjectMode` の処理を実行します。
    else if (m_editMode == EditMode::GridObject)
    {
        UpdateGridObjectMode(deleteTriggered);
    }
    else
    {
        UpdateEnvironmentObjectMode(deleteTriggered);
    }
    UpdateCameraMatrices();

    // 編集で無効化された検証結果だけを必要なフレームに再計算します。
    if (m_validationDirty)
    {
        RefreshValidationIssues();
    }
}

void SceneNarakuPieceEditor::UpdateHeightMode(bool deleteTriggered)
{
    EDITOR_PROFILE_FUNCTION();
    const bool canDeleteCells =
        m_terrainSelectionMode == TerrainSelectionMode::Cell && !m_selectedCells.empty();
    // セル選択中にDeleteが押された場合だけ選択セルを削除します。
    if (deleteTriggered && canDeleteCells)
    {
        DeleteSelectedCells();
    }
    UpdateHeightEditing();
}

void SceneNarakuPieceEditor::UpdateGridObjectMode(bool deleteTriggered)
{
    EDITOR_PROFILE_FUNCTION();
    // Delete入力が発生したフレームだけ選択オブジェクトを削除します。
    if (deleteTriggered)
    {
        DeleteSelectedGridObject();
    }
    UpdateGridObjectEditing();
}

void SceneNarakuPieceEditor::UpdateEnvironmentObjectMode(bool deleteTriggered)
{
    EDITOR_PROFILE_FUNCTION();
    const bool hasSelectedObject =
        m_selectedEnvironmentObjectIndex >= 0 &&
        m_selectedEnvironmentObjectIndex < static_cast<int>(m_piece.environmentObjects.size());
    // 有効な環境オブジェクトを選択している時だけDelete入力を反映します。
    if (deleteTriggered && hasSelectedObject)
    {
        DeleteSelectedEnvironmentObject();
    }
    UpdateEnvironmentObjectEditing();
}

void SceneNarakuPieceEditor::DeleteSelectedCells()
{
    EDITOR_PROFILE_FUNCTION();
    for (const CellSelection& selection : m_selectedCells)
    {
        if (FindFishingPointIndexByCell(selection.x, selection.z) >= 0)
        {
            SetMessage(u8"釣り地点の岸セルは削除できません");
            return;
        }
        if (IsFishingPointWaterCell(selection.x, selection.z))
        {
            SetMessage(u8"釣り地点の対象水面は削除できません");
            return;
        }
    }
    PushUndoSnapshot();
    // 複数選択中の全セルへ削除状態を反映します。
    for (const CellSelection& selection : m_selectedCells)
    {
        NarakuPiece::CellData* const cell = GetCellData(selection.x, selection.z);
        // 有効範囲内に存在するセルだけを書き換えます。
        if (cell != nullptr)
        {
            cell->deleted = true;
        }
    }
    MarkPieceDirty();
    SetMessage(u8"選択セルを削除しました");
}

void SceneNarakuPieceEditor::DeleteSelectedEnvironmentObject()
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    m_piece.environmentObjects.erase(
        m_piece.environmentObjects.begin() + m_selectedEnvironmentObjectIndex);
    m_selectedEnvironmentObjectIndex = -1;
    MarkPieceDirty();
    SetMessage(u8"環境オブジェクトを削除しました");
}

void SceneNarakuPieceEditor::Draw()
{
    EDITOR_PROFILE_FUNCTION();
    if (m_showPreviewWindow)
    {
        RenderTerrainPreviewToTexture();
    }
    OpenRequestedPopups();
    DrawEditorWindow();
    DrawPreviewWindow();
    DrawHeightGridWindow();
    DrawPieceHierarchyWindow();
    DrawEnvironmentAssetsWindow();
    DrawNewPiecePopup();
    DrawSavePiecePopup();
    DrawUnreachableWalkablePopup();
    DrawRenamePiecePopup();
    DrawEnvironmentModelPopup();
}

void SceneNarakuPieceEditor::OpenRequestedPopups()
{
    EDITOR_PROFILE_FUNCTION();
    // 新規作成要求を一度だけImGuiへ渡します。
    if (m_requestOpenNewPiecePopup)
    {
        ImGui::OpenPopup(u8"新規ピースを作成");
        m_requestOpenNewPiecePopup = false;
    }
    // 保存要求を一度だけImGuiへ渡します。
    if (m_requestOpenSavePiecePopup)
    {
        ImGui::OpenPopup(u8"ピースを保存");
        m_requestOpenSavePiecePopup = false;
    }
    if (m_requestOpenUnreachableWalkablePopup)
    {
        ImGui::OpenPopup(u8"到達不能な歩行可能領域");
        m_requestOpenUnreachableWalkablePopup = false;
    }
    // 名前変更要求を一度だけImGuiへ渡します。
    if (m_requestOpenRenamePiecePopup)
    {
        ImGui::OpenPopup(u8"ピース名を変更");
        m_requestOpenRenamePiecePopup = false;
    }
}

bool SceneNarakuPieceEditor::HandleNativeMenuCommand(unsigned int commandId)
{
    EDITOR_PROFILE_FUNCTION();
    // 値の種類に対応する処理を選択します。
    switch (commandId)
    {
    case MenuNewPiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"新規作成"))
        {
            return true;
        }
        std::fill(m_newPieceFileNameInput.begin(), m_newPieceFileNameInput.end(), '\0');
        {
            const std::string fileNameUtf8 = WideToUtf8(EnsureJsonFileName(m_saveFileName));
            const size_t copyLength = std::min(fileNameUtf8.size(), m_newPieceFileNameInput.size() - 1);
            std::copy_n(fileNameUtf8.data(), copyLength, m_newPieceFileNameInput.data());
            m_newPieceFileNameInput[copyLength] = '\0';
        }
        m_newPieceFileName = EnsureJsonFileName(m_saveFileName);
        m_newPieceIsSurface = m_piece.isSurface;
        m_requestOpenNewPiecePopup = true;
        return true;
    case MenuSavePiece:
        SyncSaveFileNameInput();
        m_saveAsDraft = true;
        m_requestOpenSavePiecePopup = true;
        return true;
    case MenuLoadPiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"読込"))
        {
            return true;
        }
        OpenLoadPieceDialog();
        return true;
    case MenuRenamePiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"名前変更"))
        {
            return true;
        }
        std::fill(m_renameFileNameInput.begin(), m_renameFileNameInput.end(), '\0');
        {
            const std::string fileNameUtf8 = WideToUtf8(EnsureJsonFileName(m_saveFileName));
            const size_t copyLength = std::min(fileNameUtf8.size(), m_renameFileNameInput.size() - 1);
            std::copy_n(fileNameUtf8.data(), copyLength, m_renameFileNameInput.data());
            m_renameFileNameInput[copyLength] = '\0';
        }
        m_requestOpenRenamePiecePopup = true;
        return true;
    case MenuDeletePiece:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!ConfirmDiscardDirtyChanges(L"削除"))
        {
            return true;
        }
        DeleteCurrentPiece();
        return true;
    case MenuPublishPieces:
        if (m_isPieceDirty)
        {
            SetMessage(u8"未保存の変更があります。先に保存してください");
            return true;
        }
        {
            const int result = RunPiecePublishScript("Publish");
            SetMessage(result == 0
                ? u8"ゲーム側へ小ステージを発行しました"
                : (result == 2 ? u8"実行中なので保存できません" : u8"小ステージの発行に失敗しました"));
        }
        return true;
    case MenuRestorePublishedPieces:
        if (m_isPieceDirty)
        {
            SetMessage(u8"未保存の変更があります。先に保存してください");
            return true;
        }
        {
            const int result = RunPiecePublishScript("Restore");
            SetMessage(result == 0
                ? u8"直近の小ステージ発行バックアップを復元しました"
                : (result == 2 ? u8"実行中なので保存できません" : u8"小ステージの復元に失敗しました"));
        }
        return true;
    case MenuTogglePieceBasicWindow:
        m_showPieceBasicWindow = !m_showPieceBasicWindow;
        return true;
    case MenuTogglePieceConnectionWindow:
        m_showPieceConnectionWindow = !m_showPieceConnectionWindow;
        return true;
    case MenuToggleTerrainEditWindow:
        m_showTerrainEditWindow = !m_showTerrainEditWindow;
        return true;
    case MenuToggleGridObjectPlacementWindow:
        m_showGridObjectPlacementWindow = !m_showGridObjectPlacementWindow;
        return true;
    case MenuToggleGridObjectSelectionWindow:
        m_showGridObjectSelectionWindow = !m_showGridObjectSelectionWindow;
        return true;
    case MenuTogglePieceFileAndValidationWindow:
        m_showPieceFileAndValidationWindow = !m_showPieceFileAndValidationWindow;
        return true;
    case MenuTogglePreviewWindow:
        m_showPreviewWindow = !m_showPreviewWindow;
        return true;
    case MenuToggleHeightGridWindow:
        m_showHeightGridWindow = !m_showHeightGridWindow;
        return true;
    case MenuTogglePieceHierarchyWindow:
        m_showPieceHierarchyWindow = !m_showPieceHierarchyWindow;
        return true;
    case MenuNewEnvironmentModel:
        OpenNewEnvironmentModelDialog();
        return true;
    case MenuDeleteEnvironmentModel:
        DeleteSelectedEnvironmentModel();
        return true;
    case MenuEnvironmentModelSetting:
        OpenEnvironmentModelSetting();
        return true;
    case MenuToggleEnvironmentAssetsWindow:
        m_showEnvironmentAssetsWindow = !m_showEnvironmentAssetsWindow;
        return true;
    case MenuToggleGroundTextures:
        m_showGroundTextures = !m_showGroundTextures;
        return true;
    default:
        return false;
    }
}

void SceneNarakuPieceEditor::SyncNativeMenuState(HMENU menuBar) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (menuBar == nullptr)
    {
        return;
    }

    const unsigned int stateMask =
        (m_showPieceBasicWindow ? 1U << 0 : 0U) |
        (m_showPieceConnectionWindow ? 1U << 1 : 0U) |
        (m_showTerrainEditWindow ? 1U << 2 : 0U) |
        (m_showGridObjectPlacementWindow ? 1U << 3 : 0U) |
        (m_showGridObjectSelectionWindow ? 1U << 4 : 0U) |
        (m_showPieceFileAndValidationWindow ? 1U << 5 : 0U) |
        (m_showPreviewWindow ? 1U << 6 : 0U) |
        (m_showHeightGridWindow ? 1U << 7 : 0U) |
        (m_showPieceHierarchyWindow ? 1U << 8 : 0U) |
        (m_showEnvironmentAssetsWindow ? 1U << 9 : 0U) |
        (m_showGroundTextures ? 1U << 10 : 0U);
    bool menuChanged = false;
    if (stateMask != m_lastNativeMenuStateMask)
    {
        SetMenuCheckState(menuBar, MenuTogglePieceBasicWindow, m_showPieceBasicWindow);
        SetMenuCheckState(menuBar, MenuTogglePieceConnectionWindow, m_showPieceConnectionWindow);
        SetMenuCheckState(menuBar, MenuToggleTerrainEditWindow, m_showTerrainEditWindow);
        SetMenuCheckState(menuBar, MenuToggleGridObjectPlacementWindow, m_showGridObjectPlacementWindow);
        SetMenuCheckState(menuBar, MenuToggleGridObjectSelectionWindow, m_showGridObjectSelectionWindow);
        SetMenuCheckState(menuBar, MenuTogglePieceFileAndValidationWindow, m_showPieceFileAndValidationWindow);
        SetMenuCheckState(menuBar, MenuTogglePreviewWindow, m_showPreviewWindow);
        SetMenuCheckState(menuBar, MenuToggleHeightGridWindow, m_showHeightGridWindow);
        SetMenuCheckState(menuBar, MenuTogglePieceHierarchyWindow, m_showPieceHierarchyWindow);
        SetMenuCheckState(menuBar, MenuToggleEnvironmentAssetsWindow, m_showEnvironmentAssetsWindow);
        SetMenuCheckState(menuBar, MenuToggleGroundTextures, m_showGroundTextures);
        m_lastNativeMenuStateMask = stateMask;
        menuChanged = true;
    }

    const std::wstring statusLabel = BuildEditingStatusLabel();
    if (statusLabel != m_lastNativeMenuStatusLabel)
    {
        SetMenuItemLabel(menuBar, MenuFileStatus, statusLabel);
        m_lastNativeMenuStatusLabel = statusLabel;
        menuChanged = true;
    }

    // 条件に該当する場合は、`DrawMenuBar` の処理を実行します。
    if (menuChanged)
    {
        if (HWND window = GetPreviewHostWindow())
        {
            DrawMenuBar(window);
        }
    }
}

bool SceneNarakuPieceEditor::PrepareToOpenGeneratedPreview()
{
    EDITOR_PROFILE_FUNCTION();
    return ConfirmDiscardDirtyChanges(L"Generated Previewへの移動");
}

SceneNarakuPieceEditor::UnreachableWalkableResult
SceneNarakuPieceEditor::FindUnreachableWalkableRegions() const
{
    EDITOR_PROFILE_FUNCTION();
    UnreachableWalkableResult result;
    const int cellWidth = m_piece.gridWidth - 1;
    const int cellDepth = m_piece.gridDepth - 1;
    const std::size_t expectedHeightCount = static_cast<std::size_t>(std::max(0, m_piece.gridWidth)) *
        static_cast<std::size_t>(std::max(0, m_piece.gridDepth));
    const std::size_t expectedCellCount = static_cast<std::size_t>(std::max(0, cellWidth)) *
        static_cast<std::size_t>(std::max(0, cellDepth));
    if (cellWidth <= 0 || cellDepth <= 0 || m_piece.cellSize <= 0.0f ||
        m_piece.heights.size() != expectedHeightCount || m_piece.cells.size() != expectedCellCount)
    {
        return result;
    }

    const auto cellIndex = [cellWidth](int x, int z)
    {
        return z * cellWidth + x;
    };
    const auto isInRange = [cellWidth, cellDepth](int x, int z)
    {
        return x >= 0 && z >= 0 && x < cellWidth && z < cellDepth;
    };
    const auto isWalkable = [&](int x, int z)
    {
        if (!isInRange(x, z)) return false;
        const NarakuPiece::CellData& cell = m_piece.cells[static_cast<std::size_t>(cellIndex(x, z))];
        if (cell.deleted || !cell.walkable || cell.waterDepth == NarakuPiece::WaterDepth::Lake)
        {
            return false;
        }
        const auto height = [&](int vertexX, int vertexZ)
        {
            return m_piece.heights[static_cast<std::size_t>(vertexZ * m_piece.gridWidth + vertexX)];
        };
        return NarakuMap::CalculateMaximumSlopeDegrees(
            height(x, z), height(x + 1, z), height(x, z + 1), height(x + 1, z + 1),
            m_piece.cellSize) <= NarakuMap::MaximumWalkableSlopeDegrees;
    };

    std::vector<unsigned char> reachable(expectedCellCount, 0u);
    std::queue<CellSelection> pending;
    const auto enqueue = [&](int x, int z)
    {
        if (!isWalkable(x, z)) return;
        const int index = cellIndex(x, z);
        if (reachable[static_cast<std::size_t>(index)] != 0u) return;
        reachable[static_cast<std::size_t>(index)] = 1u;
        pending.push({ x, z });
    };

    for (int x = 0; x < cellWidth; ++x)
    {
        if (m_piece.edgeCategories.north != NarakuPiece::StageCategory::Blocked) enqueue(x, 0);
        if (m_piece.edgeCategories.south != NarakuPiece::StageCategory::Blocked) enqueue(x, cellDepth - 1);
    }
    for (int z = 0; z < cellDepth; ++z)
    {
        if (m_piece.edgeCategories.west != NarakuPiece::StageCategory::Blocked) enqueue(0, z);
        if (m_piece.edgeCategories.east != NarakuPiece::StageCategory::Blocked) enqueue(cellWidth - 1, z);
    }
    if (m_piece.startReturnCandidate.enabled)
        enqueue(m_piece.startReturnCandidate.cell.x, m_piece.startReturnCandidate.cell.z);
    if (m_piece.layerTransition.ropePointEnabled)
        enqueue(m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z);
    if (m_piece.layerTransition.loadPointEnabled)
        enqueue(m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z);
    if (m_piece.rope.enabled)
    {
        enqueue(m_piece.rope.top.x, m_piece.rope.top.z);
        enqueue(m_piece.rope.bottom.x, m_piece.rope.bottom.z);
    }

    static const int kNeighborX[] = { 0, 1, 0, -1 };
    static const int kNeighborZ[] = { -1, 0, 1, 0 };
    const auto visitReachable = [&](std::queue<CellSelection>& queue)
    {
        while (!queue.empty())
        {
            const CellSelection current = queue.front();
            queue.pop();
            for (int direction = 0; direction < 4; ++direction)
                enqueue(current.x + kNeighborX[direction], current.z + kNeighborZ[direction]);
            if (m_piece.rope.enabled)
            {
                if (current.x == m_piece.rope.top.x && current.z == m_piece.rope.top.z)
                    enqueue(m_piece.rope.bottom.x, m_piece.rope.bottom.z);
                else if (current.x == m_piece.rope.bottom.x && current.z == m_piece.rope.bottom.z)
                    enqueue(m_piece.rope.top.x, m_piece.rope.top.z);
            }
        }
    };
    visitReachable(pending);

    std::vector<unsigned char> isolatedVisited(expectedCellCount, 0u);
    for (int z = 0; z < cellDepth; ++z)
    {
        for (int x = 0; x < cellWidth; ++x)
        {
            const int startIndex = cellIndex(x, z);
            if (!isWalkable(x, z) || reachable[static_cast<std::size_t>(startIndex)] != 0u ||
                isolatedVisited[static_cast<std::size_t>(startIndex)] != 0u)
            {
                continue;
            }
            ++result.regionCount;
            std::queue<CellSelection> isolatedPending;
            isolatedPending.push({ x, z });
            isolatedVisited[static_cast<std::size_t>(startIndex)] = 1u;
            while (!isolatedPending.empty())
            {
                const CellSelection current = isolatedPending.front();
                isolatedPending.pop();
                result.cells.push_back(current);
                for (int direction = 0; direction < 4; ++direction)
                {
                    const int nextX = current.x + kNeighborX[direction];
                    const int nextZ = current.z + kNeighborZ[direction];
                    if (!isWalkable(nextX, nextZ)) continue;
                    const int nextIndex = cellIndex(nextX, nextZ);
                    if (reachable[static_cast<std::size_t>(nextIndex)] != 0u ||
                        isolatedVisited[static_cast<std::size_t>(nextIndex)] != 0u) continue;
                    isolatedVisited[static_cast<std::size_t>(nextIndex)] = 1u;
                    isolatedPending.push({ nextX, nextZ });
                }
            }
        }
    }
    return result;
}

void SceneNarakuPieceEditor::SelectUnreachableWalkableCells()
{
    EDITOR_PROFILE_FUNCTION();
    if (m_unreachableWalkableCells.empty()) return;
    m_selectedCells = m_unreachableWalkableCells;
    m_selectedCellX = m_selectedCells.front().x;
    m_selectedCellZ = m_selectedCells.front().z;
    m_terrainSelectionMode = TerrainSelectionMode::Cell;
}


void SceneNarakuPieceEditor::RefreshValidationIssues()
{
    EDITOR_PROFILE_FUNCTION();
    m_validationIssues = NarakuPiece::ValidatePieceData(m_piece);
    for (int cellZ = 0; cellZ < m_piece.gridDepth - 1; ++cellZ)
    {
        for (int cellX = 0; cellX < m_piece.gridWidth - 1; ++cellX)
        {
            const NarakuPiece::CellData* const cell = GetCellData(cellX, cellZ);
            if (cell == nullptr || cell->waterDepth == NarakuPiece::WaterDepth::None) continue;

            const bool hasMiningPoint = FindMiningPointIndexByCell(cellX, cellZ) >= 0;
            const bool hasRopePoint = m_piece.rope.enabled &&
                ((m_piece.rope.top.x == cellX && m_piece.rope.top.z == cellZ) ||
                    (m_piece.rope.bottom.x == cellX && m_piece.rope.bottom.z == cellZ));
            const bool hasStartReturn = m_piece.startReturnCandidate.enabled &&
                m_piece.startReturnCandidate.cell.x == cellX && m_piece.startReturnCandidate.cell.z == cellZ;
            const bool hasLayerRopePoint = m_piece.layerTransition.ropePointEnabled &&
                m_piece.layerTransition.ropePoint.x == cellX && m_piece.layerTransition.ropePoint.z == cellZ;
            const bool hasLayerLoadPoint = m_piece.layerTransition.loadPointEnabled &&
                m_piece.layerTransition.loadPoint.x == cellX && m_piece.layerTransition.loadPoint.z == cellZ;
            if (!hasMiningPoint && !hasRopePoint && !hasStartReturn && !hasLayerRopePoint &&
                !hasLayerLoadPoint && !HasEnvironmentObjectAt(cellX, cellZ)) continue;

            NarakuPiece::ValidationIssue issue;
            issue.severity = NarakuPiece::ValidationIssue::Severity::Error;
            issue.message = "水場セルに配置物があります: (" + std::to_string(cellX) + ", " + std::to_string(cellZ) + ")";
            m_validationIssues.push_back(issue);
        }
    }
    // 対象コレクションの各要素を順に処理します。
    for (const NarakuPiece::EnvironmentObjectData& object : m_piece.environmentObjects)
    {
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (FindEnvironmentModelIndexById(object.modelId) < 0)
        {
            NarakuPiece::ValidationIssue issue;
            issue.severity = NarakuPiece::ValidationIssue::Severity::Error;
            issue.message = "environmentObject が未登録モデルを参照しています: " + object.modelId;
            m_validationIssues.push_back(issue);
        }
    }
    m_validationDirty = false;
}

void SceneNarakuPieceEditor::InvalidateValidationState()
{
    EDITOR_PROFILE_FUNCTION();
    m_validationDirty = true;
}

void SceneNarakuPieceEditor::MarkPieceDirty()
{
    EDITOR_PROFILE_FUNCTION();
    const bool wasDirty = m_isPieceDirty;
    m_isPieceDirty = true;
    InvalidateValidationState();
    // 条件に該当する場合は、`UpdateMainWindowTitle` の処理を実行します。
    if (!wasDirty)
    {
        UpdateMainWindowTitle();
    }
}

void SceneNarakuPieceEditor::MarkPieceClean()
{
    EDITOR_PROFILE_FUNCTION();
    const bool wasDirty = m_isPieceDirty;
    m_isPieceDirty = false;
    // 条件に該当する場合は、`UpdateMainWindowTitle` の処理を実行します。
    if (wasDirty)
    {
        UpdateMainWindowTitle();
    }
}

void SceneNarakuPieceEditor::SetMessage(const std::string& message)
{
    EDITOR_PROFILE_FUNCTION();
    m_message = message;
}

std::wstring SceneNarakuPieceEditor::Utf8ToWide(const std::string& text) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (text.empty())
    {
        return std::wstring();
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (length <= 0)
    {
        std::wstring fallback;
        fallback.reserve(text.size());
        // 対象コレクションの各要素を順に処理します。
        for (unsigned char ch : text)
        {
            fallback.push_back(static_cast<wchar_t>(ch));
        }
        return fallback;
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!result.empty())
    {
        result.pop_back();
    }
    return result;
}

std::string SceneNarakuPieceEditor::WideToUtf8(const std::wstring& text) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (text.empty())
    {
        return std::string();
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (length <= 0)
    {
        std::string fallback;
        fallback.reserve(text.size());
        // 対象コレクションの各要素を順に処理します。
        for (wchar_t ch : text)
        {
            fallback.push_back((ch >= 0 && ch <= 0x7f) ? static_cast<char>(ch) : '?');
        }
        return fallback;
    }

    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!result.empty())
    {
        result.pop_back();
    }
    return result;
}
