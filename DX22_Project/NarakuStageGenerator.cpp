#include "NarakuStageGenerator.h"

#include "NarakuMapData.h"
#include "NarakuPieceData.h"

#include <Windows.h>
#undef max
#undef min

#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    constexpr int kStageGridSize = 3;
    constexpr int kPieceVertexSize = 16;
    const wchar_t* kDefaultOutputMapPath = L"Assets\\Maps\\generated_naraku_map.json";
    const wchar_t* kLayer1CompletedProbePath = L"Assets\\Naraku\\Pieces\\Layer1\\Completed\\_probe.json";

    struct LoadedPiece
    {
        NarakuPiece::PieceData data;
        std::wstring sourcePath;
    };

    struct GridPlacement
    {
        const LoadedPiece* piece = nullptr;
        int gridX = 0;
        int gridZ = 0;
    };

    bool HasMapValidationError(const NarakuMap::MapData& mapData, std::string* outError)
    {
        const std::vector<NarakuMap::ValidationIssue> issues = NarakuMap::ValidateMapData(mapData);
        std::ostringstream errorStream;
        bool hasError = false;
        for (const NarakuMap::ValidationIssue& issue : issues)
        {
            if (issue.severity != NarakuMap::ValidationIssue::Error)
            {
                continue;
            }

            if (hasError)
            {
                errorStream << '\n';
            }
            errorStream << issue.message;
            hasError = true;
        }

        if (hasError && outError != nullptr)
        {
            *outError = errorStream.str();
        }
        return hasError;
    }

    std::wstring GetCompletedDirectoryPath()
    {
        std::wstring probePath = NarakuPiece::ResolvePiecePathForFileSystem(kLayer1CompletedProbePath);
        const size_t slashPos = probePath.find_last_of(L"\\/");
        if (slashPos == std::wstring::npos)
        {
            return probePath;
        }
        return probePath.substr(0, slashPos);
    }

    bool EnumerateCompletedPiecePaths(std::vector<std::wstring>& outPaths, std::string* outError)
    {
        outPaths.clear();

        const std::wstring directoryPath = GetCompletedDirectoryPath();
        std::wstring searchPattern = directoryPath;
        if (!searchPattern.empty() && searchPattern.back() != L'\\' && searchPattern.back() != L'/')
        {
            searchPattern += L'\\';
        }
        searchPattern += L"*.json";

        WIN32_FIND_DATAW findData = {};
        HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
        if (findHandle == INVALID_HANDLE_VALUE)
        {
            if (outError != nullptr)
            {
                *outError = "failed to enumerate completed pieces";
            }
            return false;
        }

        do
        {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                continue;
            }

            std::wstring filePath = directoryPath;
            if (!filePath.empty() && filePath.back() != L'\\' && filePath.back() != L'/')
            {
                filePath += L'\\';
            }
            filePath += findData.cFileName;
            outPaths.push_back(filePath);
        } while (FindNextFileW(findHandle, &findData) != 0);

        FindClose(findHandle);
        std::sort(outPaths.begin(), outPaths.end());
        return true;
    }

    bool LoadCandidatePieces(std::vector<LoadedPiece>& outPieces, std::string* outError)
    {
        outPieces.clear();

        std::vector<std::wstring> piecePaths;
        if (!EnumerateCompletedPiecePaths(piecePaths, outError))
        {
            return false;
        }

        for (const std::wstring& path : piecePaths)
        {
            LoadedPiece loadedPiece;
            std::string loadError;
            if (!NarakuPiece::LoadPieceData(path, loadedPiece.data, &loadError))
            {
                continue;
            }

            if (loadedPiece.data.gridWidth != kPieceVertexSize || loadedPiece.data.gridDepth != kPieceVertexSize)
            {
                continue;
            }

            if (NarakuPiece::HasValidationError(NarakuPiece::ValidatePieceData(loadedPiece.data)))
            {
                continue;
            }

            loadedPiece.sourcePath = path;
            outPieces.push_back(loadedPiece);
        }

        if (outPieces.empty())
        {
            if (outError != nullptr)
            {
                *outError = "no valid 16x16 completed pieces found";
            }
            return false;
        }

        return true;
    }

    bool IsStartReturnPreferred(const NarakuPiece::PieceData& piece)
    {
        return piece.stageRole == NarakuPiece::StageRole::StartReturn || piece.startReturnCandidate.enabled;
    }

    bool MatchesNorthWestConstraints(
        const GridPlacement* northPlacement,
        const GridPlacement* westPlacement,
        const LoadedPiece& candidate)
    {
        if (northPlacement != nullptr &&
            northPlacement->piece->data.edgeCategories.south != candidate.data.edgeCategories.north)
        {
            return false;
        }

        if (westPlacement != nullptr &&
            westPlacement->piece->data.edgeCategories.east != candidate.data.edgeCategories.west)
        {
            return false;
        }

        return true;
    }

    bool SelectPieceForCell(
        const std::vector<LoadedPiece>& candidates,
        const std::vector<GridPlacement>& placements,
        int gridX,
        int gridZ,
        GridPlacement& outPlacement,
        std::string* outError)
    {
        const GridPlacement* northPlacement = nullptr;
        const GridPlacement* westPlacement = nullptr;
        for (const GridPlacement& placement : placements)
        {
            if (placement.gridX == gridX && placement.gridZ == gridZ - 1)
            {
                northPlacement = &placement;
            }
            else if (placement.gridX == gridX - 1 && placement.gridZ == gridZ)
            {
                westPlacement = &placement;
            }
        }

        std::vector<const LoadedPiece*> matchedCandidates;
        for (const LoadedPiece& candidate : candidates)
        {
            if (MatchesNorthWestConstraints(northPlacement, westPlacement, candidate))
            {
                matchedCandidates.push_back(&candidate);
            }
        }

        if (matchedCandidates.empty())
        {
            if (outError != nullptr)
            {
                std::ostringstream oss;
                oss << "no compatible piece at cell (" << gridX << ", " << gridZ << ")";
                *outError = oss.str();
            }
            return false;
        }

        const LoadedPiece* selectedPiece = matchedCandidates.front();
        if (gridX == 0 && gridZ == 0)
        {
            for (const LoadedPiece* candidate : matchedCandidates)
            {
                if (IsStartReturnPreferred(candidate->data))
                {
                    selectedPiece = candidate;
                    break;
                }
            }
        }

        outPlacement.piece = selectedPiece;
        outPlacement.gridX = gridX;
        outPlacement.gridZ = gridZ;
        return true;
    }

    float ComputeLayerCenterCoord(int gridIndex, int cellCount, float cellSize)
    {
        const float layerSpan = static_cast<float>(cellCount) * cellSize;
        const float wholeSpan = static_cast<float>(kStageGridSize * cellCount) * cellSize;
        return -wholeSpan * 0.5f + layerSpan * 0.5f + static_cast<float>(gridIndex) * layerSpan;
    }

    int GetCellIndex(const NarakuPiece::PieceData& piece, int cellX, int cellZ)
    {
        return cellZ * (piece.gridWidth - 1) + cellX;
    }

    int GetVertexIndex(int gridWidth, int gridX, int gridZ)
    {
        return gridZ * gridWidth + gridX;
    }

    void DisableCellVertices(NarakuMap::TerrainLayer& layer, int cellX, int cellZ)
    {
        layer.vertexEnabled[GetVertexIndex(layer.gridWidth, cellX, cellZ)] = 0u;
        layer.vertexEnabled[GetVertexIndex(layer.gridWidth, cellX + 1, cellZ)] = 0u;
        layer.vertexEnabled[GetVertexIndex(layer.gridWidth, cellX, cellZ + 1)] = 0u;
        layer.vertexEnabled[GetVertexIndex(layer.gridWidth, cellX + 1, cellZ + 1)] = 0u;
    }

    NarakuMap::Vec2 ComputeCellCenterWorld(const NarakuMap::TerrainLayer& layer, int cellX, int cellZ)
    {
        const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
        const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);

        NarakuMap::Vec2 point = {};
        point.x = minX + (static_cast<float>(cellX) + 0.5f) * layer.cellSize;
        point.z = minZ + (static_cast<float>(cellZ) + 0.5f) * layer.cellSize;
        return point;
    }

    void ApplyBoundaryClosure(const GridPlacement& placement, NarakuMap::TerrainLayer& layer)
    {
        const bool hasNorthNeighbor = placement.gridZ > 0;
        const bool hasSouthNeighbor = placement.gridZ < (kStageGridSize - 1);
        const bool hasWestNeighbor = placement.gridX > 0;
        const bool hasEastNeighbor = placement.gridX < (kStageGridSize - 1);
        const int cellWidth = layer.gridWidth - 1;
        const int cellHeight = layer.gridHeight - 1;

        if (!hasNorthNeighbor)
        {
            for (int cellX = 0; cellX < cellWidth; ++cellX)
            {
                NarakuMap::SetCellAttributeFlags(
                    layer,
                    cellX,
                    0,
                    NarakuMap::GetCellAttributeFlags(layer, cellX, 0) | NarakuMap::CellAttributeBlocked);
            }
        }

        if (!hasSouthNeighbor)
        {
            for (int cellX = 0; cellX < cellWidth; ++cellX)
            {
                NarakuMap::SetCellAttributeFlags(
                    layer,
                    cellX,
                    cellHeight - 1,
                    NarakuMap::GetCellAttributeFlags(layer, cellX, cellHeight - 1) | NarakuMap::CellAttributeBlocked);
            }
        }

        if (!hasWestNeighbor)
        {
            for (int cellZ = 0; cellZ < cellHeight; ++cellZ)
            {
                NarakuMap::SetCellAttributeFlags(
                    layer,
                    0,
                    cellZ,
                    NarakuMap::GetCellAttributeFlags(layer, 0, cellZ) | NarakuMap::CellAttributeBlocked);
            }
        }

        if (!hasEastNeighbor)
        {
            for (int cellZ = 0; cellZ < cellHeight; ++cellZ)
            {
                NarakuMap::SetCellAttributeFlags(
                    layer,
                    cellWidth - 1,
                    cellZ,
                    NarakuMap::GetCellAttributeFlags(layer, cellWidth - 1, cellZ) | NarakuMap::CellAttributeBlocked);
            }
        }
    }

    NarakuPiece::GridPoint FindFallbackStartCell(const NarakuPiece::PieceData& piece)
    {
        for (int cellZ = 0; cellZ < piece.gridDepth - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < piece.gridWidth - 1; ++cellX)
            {
                const NarakuPiece::CellData& cell = piece.cells[GetCellIndex(piece, cellX, cellZ)];
                if (!cell.deleted && cell.walkable)
                {
                    NarakuPiece::GridPoint point = {};
                    point.x = cellX;
                    point.z = cellZ;
                    return point;
                }
            }
        }

        return {};
    }

    NarakuMap::TerrainLayer BuildTerrainLayer(const GridPlacement& placement, int layerId)
    {
        const NarakuPiece::PieceData& piece = placement.piece->data;

        NarakuMap::TerrainLayer layer = {};
        layer.id = layerId;
        layer.gridWidth = piece.gridWidth;
        layer.gridHeight = piece.gridDepth;
        layer.cellSize = piece.cellSize;
        layer.center.x = ComputeLayerCenterCoord(placement.gridX, piece.gridWidth - 1, piece.cellSize);
        layer.center.z = ComputeLayerCenterCoord(placement.gridZ, piece.gridDepth - 1, piece.cellSize);
        layer.layerDepth = static_cast<float>((piece.abyssLayer - 1) * 4);
        layer.groundTextureId = piece.cells.empty() ? 0 : piece.cells.front().groundTextureId;
        layer.heights = piece.heights;

        NarakuMap::EnsureLayerHeights(layer);
        NarakuMap::EnsureLayerVertexEnabled(layer);
        NarakuMap::EnsureLayerCellAttributes(layer);
        std::fill(layer.vertexEnabled.begin(), layer.vertexEnabled.end(), static_cast<std::uint8_t>(1u));

        for (int cellZ = 0; cellZ < piece.gridDepth - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < piece.gridWidth - 1; ++cellX)
            {
                const NarakuPiece::CellData& cell = piece.cells[GetCellIndex(piece, cellX, cellZ)];
                std::uint32_t flags = NarakuMap::CellAttributeNone;
                if (cell.deleted)
                {
                    flags |= NarakuMap::CellAttributeRemoved;
                    DisableCellVertices(layer, cellX, cellZ);
                }
                if (!cell.walkable)
                {
                    flags |= NarakuMap::CellAttributeBlocked;
                }
                if (cell.ropeAllowed)
                {
                    flags |= NarakuMap::CellAttributeRopeAnchor;
                }
                NarakuMap::SetCellAttributeFlags(layer, cellX, cellZ, flags);
            }
        }

        ApplyBoundaryClosure(placement, layer);
        return layer;
    }

    void AppendMiningPoints(const GridPlacement& placement, const NarakuMap::TerrainLayer& layer, NarakuMap::MapData& mapData)
    {
        const NarakuPiece::PieceData& piece = placement.piece->data;
        for (size_t index = 0; index < piece.miningPoints.size(); ++index)
        {
            const NarakuPiece::MiningPointData& piecePoint = piece.miningPoints[index];
            NarakuMap::MiningPoint mapPoint = {};
            mapPoint.xz = ComputeCellCenterWorld(layer, piecePoint.cell.x, piecePoint.cell.z);
            mapPoint.layerId = layer.id;
            mapPoint.visualType = piecePoint.visualType;
            mapPoint.discovered = piecePoint.initiallyRecorded;
            mapPoint.enabled = true;
            mapPoint.respawnCandidate = false;
            if (!piecePoint.id.empty())
            {
                mapPoint.relicName = piecePoint.id;
            }
            else
            {
                std::ostringstream oss;
                oss << piece.id << "_mining_" << index;
                mapPoint.relicName = oss.str();
            }
            mapData.miningPoints.push_back(mapPoint);
        }
    }

    NarakuMap::LayerPoint BuildLayerPoint(const NarakuMap::TerrainLayer& layer, const NarakuPiece::GridPoint& cell)
    {
        NarakuMap::LayerPoint point = {};
        point.layerId = layer.id;
        point.xz = ComputeCellCenterWorld(layer, cell.x, cell.z);
        return point;
    }
}

namespace NarakuStageGenerator
{
    bool GenerateFixed3x3Map(const wchar_t* outputMapPath, std::string* outError)
    {
        std::vector<LoadedPiece> candidates;
        if (!LoadCandidatePieces(candidates, outError))
        {
            return false;
        }

        std::vector<GridPlacement> placements;
        placements.reserve(kStageGridSize * kStageGridSize);
        for (int gridZ = 0; gridZ < kStageGridSize; ++gridZ)
        {
            for (int gridX = 0; gridX < kStageGridSize; ++gridX)
            {
                GridPlacement placement = {};
                if (!SelectPieceForCell(candidates, placements, gridX, gridZ, placement, outError))
                {
                    return false;
                }
                placements.push_back(placement);
            }
        }

        NarakuMap::MapData mapData = {};
        mapData.autoFallStartHeight = 0.90f;

        const GridPlacement* startPlacement = nullptr;
        for (size_t index = 0; index < placements.size(); ++index)
        {
            const GridPlacement& placement = placements[index];
            NarakuMap::TerrainLayer layer = BuildTerrainLayer(placement, static_cast<int>(index));
            if (startPlacement == nullptr && placement.piece->data.startReturnCandidate.enabled)
            {
                startPlacement = &placement;
            }

            AppendMiningPoints(placement, layer, mapData);
            mapData.terrainLayers.push_back(layer);
        }

        if (startPlacement == nullptr)
        {
            startPlacement = &placements.front();
        }

        const size_t startIndex = static_cast<size_t>(startPlacement->gridZ * kStageGridSize + startPlacement->gridX);
        NarakuPiece::GridPoint startCell = startPlacement->piece->data.startReturnCandidate.enabled
            ? startPlacement->piece->data.startReturnCandidate.cell
            : FindFallbackStartCell(startPlacement->piece->data);
        mapData.playerStartPoint = BuildLayerPoint(mapData.terrainLayers[startIndex], startCell);
        mapData.returnPoint = mapData.playerStartPoint;

        if (HasMapValidationError(mapData, outError))
        {
            return false;
        }

        const wchar_t* savePath = (outputMapPath == nullptr || outputMapPath[0] == L'\0')
            ? kDefaultOutputMapPath
            : outputMapPath;
        return NarakuMap::SaveMap(savePath, mapData, outError);
    }
}
