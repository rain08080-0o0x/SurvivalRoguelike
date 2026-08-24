#pragma once

#include "NarakuPieceData.h"

#include <cstddef>
#include <vector>

/**
 * @brief ピースエディタのUndo/Redo履歴と選択状態のスナップショットを管理します。
 */
class NarakuPieceEditorHistory
{
public:
    struct VertexSelection
    {
        int x = 0;
        int z = 0;
    };

    struct CellSelection
    {
        int x = 0;
        int z = 0;
    };

    enum class EditMode
    {
        Height,
        GridObject,
        EnvironmentObject,
    };

    enum class TerrainSelectionMode
    {
        Vertex,
        Cell,
    };

    enum class GridObjectTool
    {
        MiningPoint,
        RopeTop,
        RopeBottom,
        StartReturn,
        LayerRopePoint,
        LayerLoadPoint,
    };

    enum class GridObjectKind
    {
        None,
        MiningPoint,
        Rope,
        StartReturn,
        LayerRopePoint,
        LayerLoadPoint,
    };

    struct Snapshot
    {
        NarakuPiece::PieceData piece;
        int selectedX = 0;
        int selectedZ = 0;
        std::vector<VertexSelection> selectedVertices;
        EditMode editMode = EditMode::Height;
        TerrainSelectionMode terrainSelectionMode = TerrainSelectionMode::Vertex;
        int selectedCellX = -1;
        int selectedCellZ = -1;
        std::vector<CellSelection> selectedCells;
        GridObjectTool gridObjectTool = GridObjectTool::MiningPoint;
        GridObjectKind selectedGridObjectKind = GridObjectKind::None;
        int selectedMiningPointIndex = -1;
        int selectedEnvironmentObjectIndex = -1;
    };

    void Push(const Snapshot& snapshot);
    bool TryUndo(const Snapshot& currentSnapshot, Snapshot& outSnapshot);
    bool TryRedo(const Snapshot& currentSnapshot, Snapshot& outSnapshot);
    void Clear();

    bool CanUndo() const;
    bool CanRedo() const;
    size_t GetUndoCount() const;
    size_t GetRedoCount() const;

private:
    static constexpr size_t kMaximumHistoryCount = 64;

    static void Trim(std::vector<Snapshot>& stack);

    std::vector<Snapshot> m_undoStack;
    std::vector<Snapshot> m_redoStack;
};
