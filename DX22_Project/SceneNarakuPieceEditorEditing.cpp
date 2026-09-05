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

int SceneNarakuPieceEditor::GetHeightIndex(int x, int z) const
{
    return z * m_piece.gridWidth + x;
}

bool SceneNarakuPieceEditor::IsValidVertex(int x, int z) const
{
    return x >= 0 && z >= 0 && x < m_piece.gridWidth && z < m_piece.gridDepth;
}

float SceneNarakuPieceEditor::GetHeight(int x, int z) const
{
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(x, z))
    {
        return 0.0f;
    }

    const int index = GetHeightIndex(x, z);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (index < 0 || index >= static_cast<int>(m_piece.heights.size()))
    {
        return 0.0f;
    }

    return m_piece.heights[static_cast<size_t>(index)];
}

void SceneNarakuPieceEditor::SetHeight(int x, int z, float height)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(x, z))
    {
        return;
    }

    const int index = GetHeightIndex(x, z);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (index < 0 || index >= static_cast<int>(m_piece.heights.size()))
    {
        return;
    }

    m_piece.heights[static_cast<size_t>(index)] = ClampFloat(height, -10.0f, 10.0f);
    MarkPieceDirty();
}

XMFLOAT3 SceneNarakuPieceEditor::GetVertexWorldPosition(int x, int z) const
{
    const float offsetX = (static_cast<float>(m_piece.gridWidth - 1) * m_piece.cellSize) * 0.5f;
    const float offsetZ = (static_cast<float>(m_piece.gridDepth - 1) * m_piece.cellSize) * 0.5f;
    return
    {
        static_cast<float>(x) * m_piece.cellSize - offsetX,
        GetHeight(x, z),
        static_cast<float>(z) * m_piece.cellSize - offsetZ
    };
}

XMFLOAT3 SceneNarakuPieceEditor::GetCellWorldPosition(int cellX, int cellZ) const
{
    const XMFLOAT3 p00 = GetVertexWorldPosition(cellX, cellZ);
    const XMFLOAT3 p10 = GetVertexWorldPosition(cellX + 1, cellZ);
    const XMFLOAT3 p01 = GetVertexWorldPosition(cellX, cellZ + 1);
    const XMFLOAT3 p11 = GetVertexWorldPosition(cellX + 1, cellZ + 1);
    return
    {
        (p00.x + p10.x + p01.x + p11.x) * 0.25f,
        (p00.y + p10.y + p01.y + p11.y) * 0.25f,
        (p00.z + p10.z + p01.z + p11.z) * 0.25f
    };
}

bool SceneNarakuPieceEditor::IsValidCell(int cellX, int cellZ) const
{
    return cellX >= 0 && cellZ >= 0 &&
        cellX < std::max(0, m_piece.gridWidth - 1) &&
        cellZ < std::max(0, m_piece.gridDepth - 1);
}

int SceneNarakuPieceEditor::GetCellIndex(int cellX, int cellZ) const
{
    return cellZ * std::max(0, m_piece.gridWidth - 1) + cellX;
}

NarakuPiece::CellData* SceneNarakuPieceEditor::GetCellData(int cellX, int cellZ)
{
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidCell(cellX, cellZ))
    {
        return nullptr;
    }

    const int index = GetCellIndex(cellX, cellZ);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (index < 0 || index >= static_cast<int>(m_piece.cells.size()))
    {
        return nullptr;
    }

    return &m_piece.cells[static_cast<size_t>(index)];
}

const NarakuPiece::CellData* SceneNarakuPieceEditor::GetCellData(int cellX, int cellZ) const
{
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidCell(cellX, cellZ))
    {
        return nullptr;
    }

    const int index = GetCellIndex(cellX, cellZ);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (index < 0 || index >= static_cast<int>(m_piece.cells.size()))
    {
        return nullptr;
    }

    return &m_piece.cells[static_cast<size_t>(index)];
}

int SceneNarakuPieceEditor::FindMiningPointIndexByCell(int cellX, int cellZ) const
{
    EDITOR_PROFILE_FUNCTION();
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (size_t index = 0; index < m_piece.miningPoints.size(); ++index)
    {
        const NarakuPiece::MiningPointData& point = m_piece.miningPoints[index];
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (point.cell.x == cellX && point.cell.z == cellZ)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

std::string SceneNarakuPieceEditor::GenerateMiningPointId() const
{
    EDITOR_PROFILE_FUNCTION();
    int maxNumber = 0;
    // 対象コレクションの各要素を順に処理します。
    for (const NarakuPiece::MiningPointData& point : m_piece.miningPoints)
    {
        int number = 0;
        // 条件に該当する場合は、`maxNumber` の状態を更新します。
        if (sscanf_s(point.id.c_str(), "mining_%d", &number) == 1)
        {
            maxNumber = std::max(maxNumber, number);
        }
    }

    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "mining_%02d", maxNumber + 1);
    return buffer;
}

void SceneNarakuPieceEditor::ClearTerrainSelection()
{
    EDITOR_PROFILE_FUNCTION();
    m_selectedVertices.clear();
    m_selectedCells.clear();
    m_selectedX = -1;
    m_selectedZ = -1;
    m_selectedCellX = -1;
    m_selectedCellZ = -1;
    m_draggingHeight = false;
    m_dragSelecting = false;
    m_selectionDragActive = false;
}

void SceneNarakuPieceEditor::ClearGridObjectSelection()
{
    EDITOR_PROFILE_FUNCTION();
    m_selectedGridObjectKind = GridObjectKind::None;
    m_selectedMiningPointIndex = -1;
}

void SceneNarakuPieceEditor::SelectMiningPoint(int index)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`ClearGridObjectSelection` の処理を実行します。
    if (index < 0 || index >= static_cast<int>(m_piece.miningPoints.size()))
    {
        ClearGridObjectSelection();
        return;
    }

    m_selectedGridObjectKind = GridObjectKind::MiningPoint;
    m_selectedMiningPointIndex = index;
}

void SceneNarakuPieceEditor::SelectRope()
{
    EDITOR_PROFILE_FUNCTION();
    m_selectedGridObjectKind = GridObjectKind::Rope;
    m_selectedMiningPointIndex = -1;
}

void SceneNarakuPieceEditor::SelectStartReturn()
{
    EDITOR_PROFILE_FUNCTION();
    m_selectedGridObjectKind = GridObjectKind::StartReturn;
    m_selectedMiningPointIndex = -1;
}

bool SceneNarakuPieceEditor::CanPlaceGridObject(GridObjectTool tool, int cellX, int cellZ, std::string& outMessage) const
{
    EDITOR_PROFILE_FUNCTION();
    const NarakuPiece::CellData* const cellData = GetCellData(cellX, cellZ);
    // 条件に該当する場合は、`outMessage` の状態を更新します。
    if (cellData == nullptr)
    {
        outMessage = u8"範囲内にセルがありません";
        return false;
    }

    // 条件に該当する場合は、`outMessage` の状態を更新します。
    if (cellData->deleted)
    {
        outMessage = u8"削除セルには配置できません";
        return false;
    }

    const bool ropeTool = tool == GridObjectTool::RopeTop ||
        tool == GridObjectTool::RopeBottom ||
        tool == GridObjectTool::LayerRopePoint;
    // 条件に該当する場合は、`outMessage` の状態を更新します。
    if (!ropeTool && HasEnvironmentObjectAt(cellX, cellZ))
    {
        outMessage = u8"環境オブジェクトと同じセルにはロープ以外を配置できません";
        return false;
    }

    // 値の種類に対応する処理を選択します。
    switch (tool)
    {
    case GridObjectTool::MiningPoint:
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (!cellData->miningAllowed)
        {
            outMessage = u8"このセルには採掘ポイントを配置できません";
            return false;
        }
        return true;

    case GridObjectTool::RopeTop:
    case GridObjectTool::RopeBottom:
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (!cellData->ropeAllowed)
        {
            outMessage = u8"このセルにはロープを配置できません";
            return false;
        }
        return true;

    case GridObjectTool::StartReturn:
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (!cellData->walkable)
        {
            outMessage = u8"このセルには開始・帰還地点を配置できません";
            return false;
        }
        return true;

    case GridObjectTool::LayerRopePoint:
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (m_piece.layerTransition.role == NarakuPiece::LayerTransitionRole::None)
        {
            outMessage = u8"先に層間口役割を設定してください";
            return false;
        }
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (!cellData->ropeAllowed)
        {
            outMessage = u8"このセルには層間口ロープを配置できません";
            return false;
        }
        return true;

    case GridObjectTool::LayerLoadPoint:
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (m_piece.layerTransition.role != NarakuPiece::LayerTransitionRole::Exit)
        {
            outMessage = u8"ロード地点は層出口にだけ配置できます";
            return false;
        }
        // 条件に該当する場合は、`outMessage` の状態を更新します。
        if (!cellData->walkable)
        {
            outMessage = u8"歩行不可セルにはロード地点を配置できません";
            return false;
        }
        return true;

    default:
        outMessage.clear();
        return true;
    }
}

bool SceneNarakuPieceEditor::DeleteSelectedGridObject()
{
    EDITOR_PROFILE_FUNCTION();
    // 値の種類に対応する処理を選択します。
    switch (m_selectedGridObjectKind)
    {
    case GridObjectKind::MiningPoint:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (m_selectedMiningPointIndex < 0 || m_selectedMiningPointIndex >= static_cast<int>(m_piece.miningPoints.size()))
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.miningPoints.erase(m_piece.miningPoints.begin() + m_selectedMiningPointIndex);
        ClearGridObjectSelection();
        MarkPieceDirty();
        SetMessage(u8"採掘ポイントを削除しました");
        return true;

    case GridObjectKind::Rope:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!m_piece.rope.enabled)
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.rope.enabled = false;
        MarkPieceDirty();
        SetMessage(u8"ロープを削除しました");
        return true;

    case GridObjectKind::StartReturn:
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (!m_piece.startReturnCandidate.enabled)
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.startReturnCandidate.enabled = false;
        MarkPieceDirty();
        SetMessage(u8"開始・帰還地点を削除しました");
        return true;

    case GridObjectKind::LayerRopePoint:
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (!m_piece.layerTransition.ropePointEnabled) return false;
        PushUndoSnapshot();
        m_piece.layerTransition.ropePointEnabled = false;
        ClearGridObjectSelection();
        MarkPieceDirty();
        SetMessage(u8"層間口ロープ端点を削除しました");
        return true;

    case GridObjectKind::LayerLoadPoint:
        // 条件に該当する場合は、`PushUndoSnapshot` の処理を実行します。
        if (!m_piece.layerTransition.loadPointEnabled) return false;
        PushUndoSnapshot();
        m_piece.layerTransition.loadPointEnabled = false;
        ClearGridObjectSelection();
        MarkPieceDirty();
        SetMessage(u8"層間口ロード地点を削除しました");
        return true;

    case GridObjectKind::None:
    default:
        return false;
    }
}



bool SceneNarakuPieceEditor::IsVertexSelected(int x, int z) const
{
    return std::any_of(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [&](const VertexSelection& selection)
        {
            return selection.x == x && selection.z == z;
        });
}

bool SceneNarakuPieceEditor::IsCellSelected(int cellX, int cellZ) const
{
    return std::any_of(
        m_selectedCells.begin(),
        m_selectedCells.end(),
        [&](const CellSelection& selection)
        {
            return selection.x == cellX && selection.z == cellZ;
        });
}

void SceneNarakuPieceEditor::SelectSingleVertex(int x, int z)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(x, z))
    {
        return;
    }

    m_selectedX = x;
    m_selectedZ = z;
    m_selectedVertices.clear();
    m_selectedVertices.push_back({ x, z });
}

void SceneNarakuPieceEditor::AddSelectedVertex(int x, int z)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(x, z) || IsVertexSelected(x, z))
    {
        return;
    }

    m_selectedVertices.push_back({ x, z });
}

void SceneNarakuPieceEditor::ToggleSelectedVertex(int x, int z)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(x, z))
    {
        return;
    }

    const auto it = std::find_if(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [&](const VertexSelection& selection)
        {
            return selection.x == x && selection.z == z;
        });

    // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
    if (it != m_selectedVertices.end())
    {
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (m_selectedVertices.size() == 1)
        {
            return;
        }

        m_selectedVertices.erase(it);
    }
    else
    {
        m_selectedVertices.push_back({ x, z });
    }

    EnsureSelectionNotEmpty();
}

void SceneNarakuPieceEditor::SelectSingleCell(int cellX, int cellZ)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidCell(cellX, cellZ))
    {
        return;
    }

    m_selectedCellX = cellX;
    m_selectedCellZ = cellZ;
    m_selectedCells.clear();
    m_selectedCells.push_back({ cellX, cellZ });
}

void SceneNarakuPieceEditor::AddSelectedCell(int cellX, int cellZ)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidCell(cellX, cellZ) || IsCellSelected(cellX, cellZ))
    {
        return;
    }

    m_selectedCells.push_back({ cellX, cellZ });
}

void SceneNarakuPieceEditor::ToggleSelectedCell(int cellX, int cellZ)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidCell(cellX, cellZ))
    {
        return;
    }

    const auto it = std::find_if(
        m_selectedCells.begin(),
        m_selectedCells.end(),
        [&](const CellSelection& selection)
        {
            return selection.x == cellX && selection.z == cellZ;
        });

    // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
    if (it != m_selectedCells.end())
    {
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (m_selectedCells.size() == 1)
        {
            return;
        }
        m_selectedCells.erase(it);
    }
    else
    {
        m_selectedCells.push_back({ cellX, cellZ });
    }

    EnsureCellSelectionValid();
}

void SceneNarakuPieceEditor::SelectCellFromInput(int cellX, int cellZ, bool ctrlPressed, bool shiftPressed)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidCell(cellX, cellZ))
    {
        return;
    }

    m_selectedCellX = cellX;
    m_selectedCellZ = cellZ;

    // 条件に該当する場合は、`ToggleSelectedCell` の処理を実行します。
    if (ctrlPressed)
    {
        ToggleSelectedCell(cellX, cellZ);
    }
    // 先の条件に該当せず、この条件を満たす場合は、`AddSelectedCell` の処理を実行します。
    else if (shiftPressed)
    {
        AddSelectedCell(cellX, cellZ);
    }
    else
    {
        SelectSingleCell(cellX, cellZ);
    }

    EnsureCellSelectionValid();
}

void SceneNarakuPieceEditor::SelectVertexFromInput(int x, int z, bool ctrlPressed, bool shiftPressed)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(x, z))
    {
        return;
    }

    m_selectedX = x;
    m_selectedZ = z;

    // 条件に該当する場合は、`ToggleSelectedVertex` の処理を実行します。
    if (ctrlPressed)
    {
        ToggleSelectedVertex(x, z);
    }
    // 先の条件に該当せず、この条件を満たす場合は、`AddSelectedVertex` の処理を実行します。
    else if (shiftPressed)
    {
        AddSelectedVertex(x, z);
    }
    else
    {
        SelectSingleVertex(x, z);
    }

    EnsureSelectionNotEmpty();
}

void SceneNarakuPieceEditor::DrawSelectionRectangle() const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_dragSelecting || !m_selectionDragActive)
    {
        return;
    }

    const long minClientX = std::min(m_selectionDragStart.x, m_selectionDragCurrent.x);
    const long minClientY = std::min(m_selectionDragStart.y, m_selectionDragCurrent.y);
    const long maxClientX = std::max(m_selectionDragStart.x, m_selectionDragCurrent.x);
    const long maxClientY = std::max(m_selectionDragStart.y, m_selectionDragCurrent.y);
    const XMFLOAT2 minPoint = ConvertClientToImGuiScreen({ minClientX, minClientY });
    const XMFLOAT2 maxPoint = ConvertClientToImGuiScreen({ maxClientX, maxClientY });

    ImDrawList* const drawList = ImGui::GetForegroundDrawList();
    drawList->AddRectFilled(
        ImVec2(minPoint.x, minPoint.y),
        ImVec2(maxPoint.x, maxPoint.y),
        IM_COL32(80, 150, 255, 48));
    drawList->AddRect(
        ImVec2(minPoint.x, minPoint.y),
        ImVec2(maxPoint.x, maxPoint.y),
        IM_COL32(80, 150, 255, 220),
        0.0f,
        0,
        1.5f);
}

std::vector<SceneNarakuPieceEditor::VertexSelection> SceneNarakuPieceEditor::CollectVerticesInScreenRect(POINT start, POINT end) const
{
    EDITOR_PROFILE_FUNCTION();
    std::vector<VertexSelection> vertices;
    const float minX = static_cast<float>(std::min(start.x, end.x));
    const float minY = static_cast<float>(std::min(start.y, end.y));
    const float maxX = static_cast<float>(std::max(start.x, end.x));
    const float maxY = static_cast<float>(std::max(start.y, end.y));

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            XMFLOAT2 screen = {};
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (!ProjectWorldToScreen(GetVertexWorldPosition(x, z), screen))
            {
                continue;
            }

            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (screen.x < minX || screen.x > maxX || screen.y < minY || screen.y > maxY)
            {
                continue;
            }

            vertices.push_back({ x, z });
        }
    }

    return vertices;
}

std::vector<SceneNarakuPieceEditor::CellSelection> SceneNarakuPieceEditor::CollectCellsInScreenRect(POINT start, POINT end) const
{
    EDITOR_PROFILE_FUNCTION();
    std::vector<CellSelection> cells;
    const float minX = static_cast<float>(std::min(start.x, end.x));
    const float minY = static_cast<float>(std::min(start.y, end.y));
    const float maxX = static_cast<float>(std::max(start.x, end.x));
    const float maxY = static_cast<float>(std::max(start.y, end.y));

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int z = 0; z < m_piece.gridDepth - 1; ++z)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int x = 0; x < m_piece.gridWidth - 1; ++x)
        {
            XMFLOAT2 screen = {};
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (!ProjectWorldToScreen(GetCellWorldPosition(x, z), screen))
            {
                continue;
            }
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (screen.x < minX || screen.x > maxX || screen.y < minY || screen.y > maxY)
            {
                continue;
            }
            cells.push_back({ x, z });
        }
    }

    return cells;
}

void SceneNarakuPieceEditor::ApplyRectangleSelection(const std::vector<VertexSelection>& vertices, bool ctrlPressed, bool shiftPressed)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (vertices.empty())
    {
        SetMessage(u8"範囲内に頂点がありません");
        return;
    }

    const VertexSelection& primary = vertices.front();
    m_selectedX = primary.x;
    m_selectedZ = primary.z;

    // 条件に該当する場合は、`for` の処理を実行します。
    if (ctrlPressed)
    {
        // 対象コレクションの各要素を順に処理します。
        for (const VertexSelection& vertex : vertices)
        {
            ToggleSelectedVertex(vertex.x, vertex.z);
        }
    }
    // 先の条件に該当せず、この条件を満たす場合は、`for` の処理を実行します。
    else if (shiftPressed)
    {
        // 対象コレクションの各要素を順に処理します。
        for (const VertexSelection& vertex : vertices)
        {
            AddSelectedVertex(vertex.x, vertex.z);
        }
    }
    else
    {
        m_selectedVertices = vertices;
    }

    EnsureSelectionNotEmpty();
}

void SceneNarakuPieceEditor::ApplyCellRectangleSelection(const std::vector<CellSelection>& cells, bool ctrlPressed, bool shiftPressed)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (cells.empty())
    {
        SetMessage(u8"範囲内にセルがありません");
        return;
    }

    const CellSelection& primary = cells.front();
    m_selectedCellX = primary.x;
    m_selectedCellZ = primary.z;

    // 条件に該当する場合は、`for` の処理を実行します。
    if (ctrlPressed)
    {
        // 対象コレクションの各要素を順に処理します。
        for (const CellSelection& cell : cells)
        {
            ToggleSelectedCell(cell.x, cell.z);
        }
    }
    // 先の条件に該当せず、この条件を満たす場合は、`for` の処理を実行します。
    else if (shiftPressed)
    {
        // 対象コレクションの各要素を順に処理します。
        for (const CellSelection& cell : cells)
        {
            AddSelectedCell(cell.x, cell.z);
        }
    }
    else
    {
        m_selectedCells = cells;
    }

    EnsureCellSelectionValid();
}

void SceneNarakuPieceEditor::KeepOnlyActiveVertexSelected()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!IsValidVertex(m_selectedX, m_selectedZ))
    {
        return;
    }

    m_selectedVertices.clear();
    m_selectedVertices.push_back({ m_selectedX, m_selectedZ });
}

void SceneNarakuPieceEditor::EnsureSelectionNotEmpty()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`m_selectedX` の状態を更新します。
    if (!IsValidVertex(m_selectedX, m_selectedZ))
    {
        m_selectedX = ClampInt(m_selectedX, 0, std::max(0, m_piece.gridWidth - 1));
        m_selectedZ = ClampInt(m_selectedZ, 0, std::max(0, m_piece.gridDepth - 1));
    }

    std::vector<VertexSelection> validSelections;
    validSelections.reserve(m_selectedVertices.size());
    // 対象コレクションの各要素を順に処理します。
    for (const VertexSelection& selection : m_selectedVertices)
    {
        // 条件に該当する場合は、その要素を処理対象から除外します。
        if (!IsValidVertex(selection.x, selection.z))
        {
            continue;
        }

        const bool alreadyExists = std::any_of(
            validSelections.begin(),
            validSelections.end(),
            [&](const VertexSelection& existing)
            {
                return existing.x == selection.x && existing.z == selection.z;
            });
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (!alreadyExists)
        {
            validSelections.push_back(selection);
        }
    }

    m_selectedVertices = std::move(validSelections);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_selectedVertices.empty())
    {
        m_selectedVertices.push_back({ m_selectedX, m_selectedZ });
    }

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!IsVertexSelected(m_selectedX, m_selectedZ))
    {
        const VertexSelection& fallback = m_selectedVertices.back();
        m_selectedX = fallback.x;
        m_selectedZ = fallback.z;
    }
}

void SceneNarakuPieceEditor::ApplyHeightDeltaToSelectedVertices(float delta)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (std::fabs(delta) <= 0.0f)
    {
        return;
    }

    // 対象コレクションの各要素を順に処理します。
    for (const VertexSelection& selection : m_selectedVertices)
    {
        SetHeight(selection.x, selection.z, GetHeight(selection.x, selection.z) + delta);
    }
}

void SceneNarakuPieceEditor::SetSelectedVerticesHeight(float height)
{
    EDITOR_PROFILE_FUNCTION();
    // 選択中の全頂点を同じ高さへ揃えます。
    for (const VertexSelection& selection : m_selectedVertices)
    {
        SetHeight(selection.x, selection.z, height);
    }
}

void SceneNarakuPieceEditor::ResetSelectedVertexHeights()
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    SetSelectedVerticesHeight(0.0f);
    SetMessage(u8"選択頂点の高さを0に戻しました");
}

void SceneNarakuPieceEditor::ResetAllVertexHeights()
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    // ピース内の全頂点を初期高さへ戻します。
    for (float& height : m_piece.heights)
    {
        height = 0.0f;
    }
    MarkPieceDirty();
    SetMessage(u8"全頂点の高さを0に戻しました");
}

void SceneNarakuPieceEditor::ApplySelectedCellFlag(
    bool NarakuPiece::CellData::* field,
    bool value)
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    // 選択中の全セルへ指定されたbool属性を反映します。
    for (const CellSelection& selection : m_selectedCells)
    {
        NarakuPiece::CellData* const cell = GetCellData(selection.x, selection.z);
        // 有効範囲内にあるセルだけを更新します。
        if (cell != nullptr)
        {
            cell->*field = value;
        }
    }
    MarkPieceDirty();
}

void SceneNarakuPieceEditor::ApplySelectedCellGroundTextureId(int groundTextureId)
{
    EDITOR_PROFILE_FUNCTION();
    PushUndoSnapshot();
    const int normalizedTextureId = std::max(0, groundTextureId);
    // 選択中の全セルへ正規化済みのテクスチャIDを反映します。
    for (const CellSelection& selection : m_selectedCells)
    {
        NarakuPiece::CellData* const cell = GetCellData(selection.x, selection.z);
        // 有効範囲内にあるセルだけを更新します。
        if (cell != nullptr)
        {
            cell->groundTextureId = normalizedTextureId;
        }
    }
    MarkPieceDirty();
}

void SceneNarakuPieceEditor::EnsureCellSelectionValid()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`m_selectedCellX` の状態を更新します。
    if (!IsValidCell(m_selectedCellX, m_selectedCellZ))
    {
        m_selectedCellX = ClampInt(m_selectedCellX, 0, std::max(0, m_piece.gridWidth - 2));
        m_selectedCellZ = ClampInt(m_selectedCellZ, 0, std::max(0, m_piece.gridDepth - 2));
    }

    std::vector<CellSelection> validSelections;
    validSelections.reserve(m_selectedCells.size());
    // 対象コレクションの各要素を順に処理します。
    for (const CellSelection& selection : m_selectedCells)
    {
        // 条件に該当する場合は、その要素を処理対象から除外します。
        if (!IsValidCell(selection.x, selection.z))
        {
            continue;
        }

        const bool alreadyExists = std::any_of(
            validSelections.begin(),
            validSelections.end(),
            [&](const CellSelection& existing)
            {
                return existing.x == selection.x && existing.z == selection.z;
            });
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (!alreadyExists)
        {
            validSelections.push_back(selection);
        }
    }

    m_selectedCells = std::move(validSelections);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_selectedCells.empty() && IsValidCell(m_selectedCellX, m_selectedCellZ))
    {
        m_selectedCells.push_back({ m_selectedCellX, m_selectedCellZ });
    }

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!m_selectedCells.empty() && !IsCellSelected(m_selectedCellX, m_selectedCellZ))
    {
        const CellSelection& fallback = m_selectedCells.back();
        m_selectedCellX = fallback.x;
        m_selectedCellZ = fallback.z;
    }
}

SceneNarakuPieceEditor::EditorSnapshot SceneNarakuPieceEditor::CreateEditorSnapshot() const
{
    EDITOR_PROFILE_FUNCTION();
    EditorSnapshot snapshot = {};
    snapshot.piece = m_piece;
    snapshot.selectedX = m_selectedX;
    snapshot.selectedZ = m_selectedZ;
    snapshot.selectedVertices = m_selectedVertices;
    snapshot.editMode = m_editMode;
    snapshot.terrainSelectionMode = m_terrainSelectionMode;
    snapshot.selectedCellX = m_selectedCellX;
    snapshot.selectedCellZ = m_selectedCellZ;
    snapshot.selectedCells = m_selectedCells;
    snapshot.gridObjectTool = m_gridObjectTool;
    snapshot.selectedGridObjectKind = m_selectedGridObjectKind;
    snapshot.selectedMiningPointIndex = m_selectedMiningPointIndex;
    snapshot.selectedEnvironmentObjectIndex = m_selectedEnvironmentObjectIndex;
    return snapshot;
}

void SceneNarakuPieceEditor::PushUndoSnapshot()
{
    EDITOR_PROFILE_FUNCTION();
    m_history.Push(CreateEditorSnapshot());
}

void SceneNarakuPieceEditor::RestoreEditorSnapshot(const EditorSnapshot& snapshot)
{
    EDITOR_PROFILE_FUNCTION();
    m_piece = snapshot.piece;
    m_selectedX = snapshot.selectedX;
    m_selectedZ = snapshot.selectedZ;
    m_selectedVertices = snapshot.selectedVertices;
    m_editMode = snapshot.editMode;
    m_terrainSelectionMode = snapshot.terrainSelectionMode;
    m_selectedCellX = snapshot.selectedCellX;
    m_selectedCellZ = snapshot.selectedCellZ;
    m_selectedCells = snapshot.selectedCells;
    m_gridObjectTool = snapshot.gridObjectTool;
    m_selectedGridObjectKind = snapshot.selectedGridObjectKind;
    m_selectedMiningPointIndex = snapshot.selectedMiningPointIndex;
    m_selectedEnvironmentObjectIndex = snapshot.selectedEnvironmentObjectIndex;
    EnsureSelectionNotEmpty();
    EnsureCellSelectionValid();
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_selectedGridObjectKind == GridObjectKind::MiningPoint &&
        (m_selectedMiningPointIndex < 0 || m_selectedMiningPointIndex >= static_cast<int>(m_piece.miningPoints.size())))
    {
        ClearGridObjectSelection();
    }
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_selectedEnvironmentObjectIndex < 0 ||
        m_selectedEnvironmentObjectIndex >= static_cast<int>(m_piece.environmentObjects.size()))
    {
        m_selectedEnvironmentObjectIndex = -1;
    }
    MarkPieceDirty();
    m_heightDragFloatEditing = false;
    m_draggingHeight = false;
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
}

void SceneNarakuPieceEditor::UndoEdit()
{
    EDITOR_PROFILE_FUNCTION();
    EditorSnapshot snapshot = {};
    // 履歴管理クラスがUndo状態を返した場合だけ編集状態へ復元します。
    if (!m_history.TryUndo(CreateEditorSnapshot(), snapshot))
    {
        return;
    }
    RestoreEditorSnapshot(snapshot);
    SetMessage(u8"元に戻しました");
}

void SceneNarakuPieceEditor::RedoEdit()
{
    EDITOR_PROFILE_FUNCTION();
    EditorSnapshot snapshot = {};
    // 履歴管理クラスがRedo状態を返した場合だけ編集状態へ復元します。
    if (!m_history.TryRedo(CreateEditorSnapshot(), snapshot))
    {
        return;
    }
    RestoreEditorSnapshot(snapshot);
    SetMessage(u8"やり直しました");
}

void SceneNarakuPieceEditor::HandleUndoRedoShortcuts()
{
    EDITOR_PROFILE_FUNCTION();
    ImGuiIO& io = ImGui::GetIO();
    // 条件に該当する場合は、`m_prevUndoShortcutPressed` の状態を更新します。
    if (io.WantTextInput)
    {
        m_prevUndoShortcutPressed = false;
        m_prevRedoShortcutPressed = false;
        return;
    }

    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool undoPressed = ctrlPressed && IsAsyncModifierPressed('Z');
    const bool redoPressed = ctrlPressed && IsAsyncModifierPressed('Y');

    // 条件に該当する場合は、`UndoEdit` の処理を実行します。
    if (IsShortcutTriggered(undoPressed, m_prevUndoShortcutPressed))
    {
        UndoEdit();
    }
    // 条件に該当する場合は、`RedoEdit` の処理を実行します。
    if (IsShortcutTriggered(redoPressed, m_prevRedoShortcutPressed))
    {
        RedoEdit();
    }
}


void SceneNarakuPieceEditor::UpdateHeightEditing()
{
    EDITOR_PROFILE_FUNCTION();
    // 地形の選択単位に対応する入力処理へ委譲します。
    if (m_terrainSelectionMode == TerrainSelectionMode::Cell)
    {
        UpdateCellHeightEditing();
        return;
    }
    UpdateVertexHeightEditing();
}

void SceneNarakuPieceEditor::UpdateHoveredCell(POINT mousePos, bool allowPreviewInput)
{
    EDITOR_PROFILE_FUNCTION();
    // プレビュー内の有効なセルを指している間はホバー座標を保持します。
    if (allowPreviewInput && PickTerrainCell(mousePos, m_hoverCellX, m_hoverCellZ))
    {
        return;
    }
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
}

void SceneNarakuPieceEditor::UpdateCellHeightEditing()
{
    EDITOR_PROFILE_FUNCTION();
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool shiftPressed = IsEditorShiftPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool allowPreviewInput = IsMouseInsidePreviewImage() || m_previewImageHovered;
    UpdateHoveredCell(mousePos, allowPreviewInput);

    // 継続中のセル選択ドラッグを通常クリックより先に処理します。
    if (m_dragSelecting)
    {
        UpdateCellSelectionDrag(mousePos);
        return;
    }
    // プレビュー外のクリックは地形編集へ渡しません。
    if (!allowPreviewInput)
    {
        return;
    }
    // ImGui操作中、カメラ操作中、またはクリック開始でない入力を除外します。
    if ((io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger())
    {
        return;
    }

    int pickedCellX = -1;
    int pickedCellZ = -1;
    // クリック位置からセルを取得できない場合は選択を変えません。
    if (!PickTerrainCell(mousePos, pickedCellX, pickedCellZ))
    {
        return;
    }
    // Shift操作はクリック確定まで範囲選択として追跡します。
    if (shiftPressed)
    {
        BeginSelectionDrag(mousePos, ctrlPressed, true);
        return;
    }
    SelectCellFromInput(pickedCellX, pickedCellZ, ctrlPressed, false);
}

void SceneNarakuPieceEditor::UpdateVertexHeightEditing()
{
    EDITOR_PROFILE_FUNCTION();
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool shiftPressed = IsEditorShiftPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool allowPreviewInput = IsMouseInsidePreviewImage() || m_previewImageHovered;

    // 高さ変更または範囲選択の継続中でなければプレビュー外入力を除外します。
    if (!allowPreviewInput && !m_draggingHeight && !m_dragSelecting)
    {
        return;
    }
    // 継続中の高さドラッグを新しい選択入力より先に処理します。
    if (m_draggingHeight)
    {
        UpdateVertexHeightDrag(altPressed);
        return;
    }
    // 継続中の頂点選択ドラッグを通常クリックより先に処理します。
    if (m_dragSelecting)
    {
        UpdateVertexSelectionDrag(mousePos);
        return;
    }
    // 新しい選択操作はプレビュー内でだけ開始します。
    if (!allowPreviewInput)
    {
        return;
    }
    // ImGui操作中、カメラ操作中、またはクリック開始でない入力を除外します。
    if ((io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger())
    {
        return;
    }

    int pickedX = -1;
    int pickedZ = -1;
    // クリック位置から頂点を取得できない場合は選択を変えません。
    if (!PickTerrainVertex(mousePos, pickedX, pickedZ))
    {
        return;
    }
    // 修飾キー付き操作はクリック確定まで範囲選択として追跡します。
    if (ctrlPressed || shiftPressed)
    {
        BeginSelectionDrag(mousePos, ctrlPressed, shiftPressed);
        return;
    }
    // 既に選択済みの頂点は主選択だけを切り替えます。
    if (IsVertexSelected(pickedX, pickedZ))
    {
        m_selectedX = pickedX;
        m_selectedZ = pickedZ;
    }
    else
    {
        SelectVertexFromInput(pickedX, pickedZ, false, false);
    }
    PushUndoSnapshot();
    m_draggingHeight = true;
}

void SceneNarakuPieceEditor::BeginSelectionDrag(
    POINT mousePos,
    bool ctrlPressed,
    bool shiftPressed)
{
    EDITOR_PROFILE_FUNCTION();
    m_dragSelecting = true;
    m_selectionDragActive = false;
    m_selectionDragStart = mousePos;
    m_selectionDragCurrent = mousePos;
    m_selectionDragCtrl = ctrlPressed;
    m_selectionDragShift = shiftPressed;
}

void SceneNarakuPieceEditor::EndSelectionDrag()
{
    EDITOR_PROFILE_FUNCTION();
    m_dragSelecting = false;
    m_selectionDragActive = false;
}

void SceneNarakuPieceEditor::UpdateSelectionDragActivation()
{
    EDITOR_PROFILE_FUNCTION();
    const float deltaX = static_cast<float>(m_selectionDragCurrent.x - m_selectionDragStart.x);
    const float deltaY = static_cast<float>(m_selectionDragCurrent.y - m_selectionDragStart.y);
    const float dragDistanceSq = deltaX * deltaX + deltaY * deltaY;
    const float thresholdSq = kDragSelectThresholdPx * kDragSelectThresholdPx;
    // しきい値以上移動した操作だけを矩形選択として扱います。
    if (!m_selectionDragActive && dragDistanceSq >= thresholdSq)
    {
        m_selectionDragActive = true;
    }
}

void SceneNarakuPieceEditor::UpdateCellSelectionDrag(POINT mousePos)
{
    EDITOR_PROFILE_FUNCTION();
    m_selectionDragCurrent = mousePos;
    UpdateSelectionDragActivation();
    // ボタンを離すまではドラッグ終点だけを更新します。
    if (!IsMouseLeftRelease())
    {
        return;
    }
    // 移動量がしきい値以上なら矩形範囲へ選択を反映します。
    if (m_selectionDragActive)
    {
        const std::vector<CellSelection> cells =
            CollectCellsInScreenRect(m_selectionDragStart, m_selectionDragCurrent);
        ApplyCellRectangleSelection(cells, m_selectionDragCtrl, m_selectionDragShift);
    }
    else
    {
        SelectCellFromDragEndpoint();
    }
    EndSelectionDrag();
}

void SceneNarakuPieceEditor::UpdateVertexSelectionDrag(POINT mousePos)
{
    EDITOR_PROFILE_FUNCTION();
    m_selectionDragCurrent = mousePos;
    UpdateSelectionDragActivation();
    // ボタンを離すまではドラッグ終点だけを更新します。
    if (!IsMouseLeftRelease())
    {
        return;
    }
    // 移動量がしきい値以上なら矩形範囲へ選択を反映します。
    if (m_selectionDragActive)
    {
        const std::vector<VertexSelection> vertices =
            CollectVerticesInScreenRect(m_selectionDragStart, m_selectionDragCurrent);
        ApplyRectangleSelection(vertices, m_selectionDragCtrl, m_selectionDragShift);
    }
    else
    {
        SelectVertexFromDragEndpoint();
    }
    EndSelectionDrag();
}

void SceneNarakuPieceEditor::SelectCellFromDragEndpoint()
{
    EDITOR_PROFILE_FUNCTION();
    int pickedCellX = -1;
    int pickedCellZ = -1;
    POINT pickPoint = m_selectionDragCurrent;
    // 終点で取得できなければドラッグ開始位置をクリック位置として再試行します。
    if (!PickTerrainCell(pickPoint, pickedCellX, pickedCellZ))
    {
        pickPoint = m_selectionDragStart;
    }
    // どちらの位置でもセルを取得できない場合は選択を変えません。
    if (!PickTerrainCell(pickPoint, pickedCellX, pickedCellZ))
    {
        return;
    }
    SelectCellFromInput(pickedCellX, pickedCellZ, m_selectionDragCtrl, m_selectionDragShift);
}

void SceneNarakuPieceEditor::SelectVertexFromDragEndpoint()
{
    EDITOR_PROFILE_FUNCTION();
    int pickedX = -1;
    int pickedZ = -1;
    POINT pickPoint = m_selectionDragCurrent;
    // 終点で取得できなければドラッグ開始位置をクリック位置として再試行します。
    if (!PickTerrainVertex(pickPoint, pickedX, pickedZ))
    {
        pickPoint = m_selectionDragStart;
    }
    // どちらの位置でも頂点を取得できない場合は選択を変えません。
    if (!PickTerrainVertex(pickPoint, pickedX, pickedZ))
    {
        return;
    }
    SelectVertexFromInput(pickedX, pickedZ, m_selectionDragCtrl, m_selectionDragShift);
}

void SceneNarakuPieceEditor::UpdateVertexHeightDrag(bool altPressed)
{
    EDITOR_PROFILE_FUNCTION();
    // 左ボタンを離したフレームで高さドラッグを終了します。
    if (!IsMouseLeftPress())
    {
        m_draggingHeight = false;
        return;
    }
    // Altによるカメラ操作中は高さを変更しません。
    if (altPressed)
    {
        return;
    }

    const POINT mouseDelta = GetMouseDelta();
    // 垂直方向に移動したフレームだけ高さ差分を適用します。
    if (mouseDelta.y != 0)
    {
        ApplyHeightDeltaToSelectedVertices(-static_cast<float>(mouseDelta.y) * m_heightDragScale);
    }
}
void SceneNarakuPieceEditor::UpdateGridObjectEditing()
{
    EDITOR_PROFILE_FUNCTION();
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool mouseInPreview = IsMouseInsidePreviewImage();
    const bool allowPreviewInput = mouseInPreview || m_previewImageHovered;

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (allowPreviewInput && PickTerrainCell(mousePos, m_hoverCellX, m_hoverCellZ))
    {
    }
    else
    {
        m_hoverCellX = -1;
        m_hoverCellZ = -1;
    }

    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!allowPreviewInput)
    {
        return;
    }

    // 条件に該当する場合は、現在の処理をここで終了します。
    if ((io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger())
    {
        return;
    }

    int cellX = -1;
    int cellZ = -1;
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!PickTerrainCell(mousePos, cellX, cellZ))
    {
        return;
    }

    std::string placeError;
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!CanPlaceGridObject(m_gridObjectTool, cellX, cellZ, placeError))
    {
        SetMessage(placeError);
        return;
    }

    // 値の種類に対応する処理を選択します。
    switch (m_gridObjectTool)
    {
    case GridObjectTool::MiningPoint:
    {
        const int existingIndex = FindMiningPointIndexByCell(cellX, cellZ);
        // 条件に該当する場合は、`SelectMiningPoint` の処理を実行します。
        if (existingIndex >= 0)
        {
            SelectMiningPoint(existingIndex);
            SetMessage(u8"既存の採掘ポイントを選択しました");
            return;
        }

        // 条件に該当する場合は、`SetMessage` の処理を実行します。
        if (m_piece.miningPoints.size() >= 5)
        {
            SetMessage(u8"採掘ポイントは最大5件までです");
            return;
        }

        PushUndoSnapshot();
        NarakuPiece::MiningPointData point = {};
        point.id = GenerateMiningPointId();
        point.cell.x = cellX;
        point.cell.z = cellZ;
        point.visualType = ClampInt(m_newMiningVisualType, 0, 3);
        point.initiallyRecorded = m_newMiningInitiallyRecorded;
        m_piece.miningPoints.push_back(point);
        SelectMiningPoint(static_cast<int>(m_piece.miningPoints.size()) - 1);
        MarkPieceDirty();
        SetMessage(u8"採掘ポイントを追加しました");
        return;
    }

    case GridObjectTool::RopeTop:
        PushUndoSnapshot();
        m_piece.rope.enabled = true;
        m_piece.rope.top.x = cellX;
        m_piece.rope.top.z = cellZ;
        SelectRope();
        MarkPieceDirty();
        SetMessage(u8"ロープ上端を設定しました");
        return;

    case GridObjectTool::RopeBottom:
        PushUndoSnapshot();
        m_piece.rope.enabled = true;
        m_piece.rope.bottom.x = cellX;
        m_piece.rope.bottom.z = cellZ;
        SelectRope();
        MarkPieceDirty();
        SetMessage(u8"ロープ下端を設定しました");
        return;

    case GridObjectTool::StartReturn:
        PushUndoSnapshot();
        m_piece.startReturnCandidate.enabled = true;
        m_piece.startReturnCandidate.cell.x = cellX;
        m_piece.startReturnCandidate.cell.z = cellZ;
        SelectStartReturn();
        MarkPieceDirty();
        SetMessage(u8"開始・帰還地点を設定しました");
        return;

    case GridObjectTool::LayerRopePoint:
        PushUndoSnapshot();
        m_piece.layerTransition.ropePointEnabled = true;
        m_piece.layerTransition.ropePoint = { cellX, cellZ };
        m_selectedGridObjectKind = GridObjectKind::LayerRopePoint;
        m_selectedMiningPointIndex = -1;
        MarkPieceDirty();
        SetMessage(u8"層間口ロープ端点を設定しました");
        return;

    case GridObjectTool::LayerLoadPoint:
        PushUndoSnapshot();
        m_piece.layerTransition.loadPointEnabled = true;
        m_piece.layerTransition.loadPoint = { cellX, cellZ };
        m_selectedGridObjectKind = GridObjectKind::LayerLoadPoint;
        m_selectedMiningPointIndex = -1;
        MarkPieceDirty();
        SetMessage(u8"層間口ロード地点を設定しました");
        return;

    default:
        return;
    }
}


