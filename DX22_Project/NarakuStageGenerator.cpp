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
#include <fstream>

namespace
{
    constexpr int kStageGridSize = 3;
    constexpr int kPieceVertexSize = 16;
    const wchar_t* kDefaultOutputMapPath = L"Assets\\Maps\\generated_naraku_map.json";
    const wchar_t* kLayer1CompletedProbePath = L"Assets\\Naraku\\Pieces\\Layer1\\Completed\\_probe.json";

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty()) return "";
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 0) return "";
        std::string result(length - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
        return result;
    }

    void WriteLogToFile(const std::string& logText)
    {
        std::wstring logPath = NarakuPiece::ResolvePiecePathForFileSystem(L"Assets\\Maps\\generation_log.txt");
        std::ofstream ofs(logPath);
        if (ofs.is_open())
        {
            ofs << logText;
            ofs.close();
        }
    }

    std::wstring GetFileNamePart(const std::wstring& path)
    {
        const size_t slashPos = path.find_last_of(L"\\/");
        return (slashPos == std::wstring::npos) ? path : path.substr(slashPos + 1);
    }

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

    struct LayerPointCandidate
    {
        NarakuMap::LayerPoint point = {};
        size_t placementIndex = 0;
        int cellX = 0;
        int cellZ = 0;
        bool preferred = false;
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

    bool MatchesGridConstraints(
        int gridX,
        int gridZ,
        const GridPlacement* northPlacement,
        const GridPlacement* westPlacement,
        const LoadedPiece& candidate,
        std::string* outMismatchReason)
    {
        const auto setReason = [&](const std::string& msg) {
            if (outMismatchReason != nullptr)
            {
                *outMismatchReason = msg;
            }
        };

        // 1. 蛹怜・縺ｮ蛻､螳・
        if (gridZ == 0)
        {
            if (candidate.data.edgeCategories.north != NarakuPiece::StageCategory::Blocked)
            {
                setReason("North edge is not Blocked (required at top boundary)");
                return false;
            }
        }
        else
        {
            if (candidate.data.edgeCategories.north == NarakuPiece::StageCategory::Blocked)
            {
                setReason("North edge is Blocked (forbidden for inner connection)");
                return false;
            }
            if (northPlacement != nullptr &&
                northPlacement->piece->data.edgeCategories.south != candidate.data.edgeCategories.north)
            {
                setReason("North edge category (" + std::string(NarakuPiece::ToString(candidate.data.edgeCategories.north)) +
                          ") does not match North neighbor's South edge category (" +
                          std::string(NarakuPiece::ToString(northPlacement->piece->data.edgeCategories.south)) + ")");
                return false;
            }
        }

        // 2. 蜊怜・縺ｮ蛻､螳・
        if (gridZ == kStageGridSize - 1)
        {
            if (candidate.data.edgeCategories.south != NarakuPiece::StageCategory::Blocked)
            {
                setReason("South edge is not Blocked (required at bottom boundary)");
                return false;
            }
        }
        else
        {
            if (candidate.data.edgeCategories.south == NarakuPiece::StageCategory::Blocked)
            {
                setReason("South edge is Blocked (forbidden for inner connection)");
                return false;
            }
        }

        // 3. 隘ｿ蛛ｴ縺ｮ蛻､螳・
        if (gridX == 0)
        {
            if (candidate.data.edgeCategories.west != NarakuPiece::StageCategory::Blocked)
            {
                setReason("West edge is not Blocked (required at left boundary)");
                return false;
            }
        }
        else
        {
            if (candidate.data.edgeCategories.west == NarakuPiece::StageCategory::Blocked)
            {
                setReason("West edge is Blocked (forbidden for inner connection)");
                return false;
            }
            if (westPlacement != nullptr &&
                westPlacement->piece->data.edgeCategories.east != candidate.data.edgeCategories.west)
            {
                setReason("West edge category (" + std::string(NarakuPiece::ToString(candidate.data.edgeCategories.west)) +
                          ") does not match West neighbor's East edge category (" +
                          std::string(NarakuPiece::ToString(westPlacement->piece->data.edgeCategories.east)) + ")");
                return false;
            }
        }

        // 4. 譚ｱ蛛ｴ縺ｮ蛻､螳・
        if (gridX == kStageGridSize - 1)
        {
            if (candidate.data.edgeCategories.east != NarakuPiece::StageCategory::Blocked)
            {
                setReason("East edge is not Blocked (required at right boundary)");
                return false;
            }
        }
        else
        {
            if (candidate.data.edgeCategories.east == NarakuPiece::StageCategory::Blocked)
            {
                setReason("East edge is Blocked (forbidden for inner connection)");
                return false;
            }
        }

        return true;
    }

    bool SelectPieceForCell(
        const std::vector<LoadedPiece>& candidates,
        const std::vector<GridPlacement>& placements,
        int gridX,
        int gridZ,
        int startX,
        int startZ,
        GridPlacement& outPlacement,
        std::stringstream& logStream,
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

        // Print constraints
        logStream << "[Cell (" << gridX << ", " << gridZ << ")] Constraints:\n";
        if (gridZ == 0) logStream << "  - North: Blocked (Top border)\n";
        else if (northPlacement != nullptr) logStream << "  - North: Connect to \"" << WideToUtf8(GetFileNamePart(northPlacement->piece->sourcePath)) << "\" South (" << NarakuPiece::ToString(northPlacement->piece->data.edgeCategories.south) << ")\n";
        else logStream << "  - North: Not Blocked\n";

        if (gridZ == kStageGridSize - 1) logStream << "  - South: Blocked (Bottom border)\n";
        else logStream << "  - South: Not Blocked\n";

        if (gridX == 0) logStream << "  - West: Blocked (Left border)\n";
        else if (westPlacement != nullptr) logStream << "  - West: Connect to \"" << WideToUtf8(GetFileNamePart(westPlacement->piece->sourcePath)) << "\" East (" << NarakuPiece::ToString(westPlacement->piece->data.edgeCategories.east) << ")\n";
        else logStream << "  - West: Not Blocked\n";

        if (gridX == kStageGridSize - 1) logStream << "  - East: Blocked (Right border)\n";
        else logStream << "  - East: Not Blocked\n";

        // Filter compatible candidates
        std::vector<const LoadedPiece*> matchedCandidates;
        std::vector<std::string> mismatchLogs;

        for (const LoadedPiece& candidate : candidates)
        {
            std::string mismatchReason;
            bool matches = MatchesGridConstraints(gridX, gridZ, northPlacement, westPlacement, candidate, &mismatchReason);

            const bool isStartCell = (gridX == startX && gridZ == startZ);
            const bool isCandidateStart = IsStartReturnPreferred(candidate.data);

            if (isStartCell)
            {
                if (matches && !isCandidateStart)
                {
                    matches = false;
                    mismatchReason = "stageRole is not StartReturn (required for designated start cell)";
                }
            }
            else
            {
                if (matches && isCandidateStart)
                {
                    matches = false;
                    mismatchReason = "stageRole is StartReturn (forbidden for non-start cell)";
                }
            }

            std::string fileName = WideToUtf8(GetFileNamePart(candidate.sourcePath));
            if (matches)
            {
                matchedCandidates.push_back(&candidate);
            }
            else
            {
                mismatchLogs.push_back("  - \"" + fileName + "\": " + mismatchReason);
            }
        }

        logStream << "  Compatibility Check Results:\n";
        for (const std::string& msg : mismatchLogs)
        {
            logStream << "    " << msg << "\n";
        }

        if (matchedCandidates.empty())
        {
            logStream << "  -> FAILURE: No compatible pieces found under these constraints.\n";
            if (outError != nullptr)
            {
                *outError = logStream.str();
            }
            return false;
        }

        // matchedCandidates縺ｮ荳ｭ縺九ｉ繝ｩ繝ｳ繝繝縺ｧ驕ｸ蜃ｺ
        int randomIndex = std::rand() % static_cast<int>(matchedCandidates.size());
        const LoadedPiece* selectedPiece = matchedCandidates[randomIndex];
        std::string selectedFileName = WideToUtf8(GetFileNamePart(selectedPiece->sourcePath));

        logStream << "  -> SUCCESS: Found " << matchedCandidates.size() << " compatible pieces. Selected: \"" << selectedFileName << "\"\n";

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
        const std::uint32_t cliffEdge = NarakuMap::CellAttributeCliffEdge;
        const std::uint32_t blocked = NarakuMap::CellAttributeBlocked;

        const auto adjustBoundaryCell = [&](const int cellX, const int cellZ)
        {
            /// マップ外移動は既存の床範囲判定で止まるため、外周セルは通行可でも外へは出ない。
            const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
            NarakuMap::SetCellAttributeFlags(layer, cellX, cellZ, flags & ~blocked);
        };

        if (!hasNorthNeighbor)
        {
            for (int cellX = 0; cellX < cellWidth; ++cellX)
            {
                adjustBoundaryCell(cellX, 0);
            }
        }

        if (!hasSouthNeighbor)
        {
            for (int cellX = 0; cellX < cellWidth; ++cellX)
            {
                adjustBoundaryCell(cellX, cellHeight - 1);
            }
        }

        if (!hasWestNeighbor)
        {
            for (int cellZ = 0; cellZ < cellHeight; ++cellZ)
            {
                adjustBoundaryCell(0, cellZ);
            }
        }

        if (!hasEastNeighbor)
        {
            for (int cellZ = 0; cellZ < cellHeight; ++cellZ)
            {
                adjustBoundaryCell(cellWidth - 1, cellZ);
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

    bool IsCellUsableForPoint(const NarakuMap::TerrainLayer& layer, int cellX, int cellZ)
    {
        const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
        if ((flags & NarakuMap::CellAttributeRemoved) != 0u)
        {
            return false;
        }

        return (flags & NarakuMap::CellAttributeBlocked) == 0u;
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

    bool TryAppendCandidateFromCell(
        const std::vector<GridPlacement>& placements,
        const std::vector<NarakuMap::TerrainLayer>& layers,
        size_t placementIndex,
        int cellX,
        int cellZ,
        bool preferred,
        std::vector<LayerPointCandidate>& outCandidates)
    {
        if (placementIndex >= placements.size() || placementIndex >= layers.size())
        {
            return false;
        }

        const NarakuMap::TerrainLayer& layer = layers[placementIndex];
        if (cellX < 0 || cellZ < 0 || cellX >= layer.gridWidth - 1 || cellZ >= layer.gridHeight - 1)
        {
            return false;
        }
        if (!IsCellUsableForPoint(layer, cellX, cellZ))
        {
            return false;
        }

        LayerPointCandidate candidate = {};
        candidate.point = BuildLayerPoint(layer, { cellX, cellZ });
        candidate.placementIndex = placementIndex;
        candidate.cellX = cellX;
        candidate.cellZ = cellZ;
        candidate.preferred = preferred;
        outCandidates.push_back(candidate);
        return true;
    }

    std::vector<LayerPointCandidate> CollectPointCandidates(
        const std::vector<GridPlacement>& placements,
        const std::vector<NarakuMap::TerrainLayer>& layers)
    {
        std::vector<LayerPointCandidate> candidates;
        for (size_t placementIndex = 0; placementIndex < placements.size() && placementIndex < layers.size(); ++placementIndex)
        {
            const NarakuPiece::PieceData& piece = placements[placementIndex].piece->data;
            const NarakuPiece::StartReturnCandidate& startReturn = piece.startReturnCandidate;
            if (startReturn.enabled)
            {
                /// 螟門捉繧ｻ繝ｫ繧る幕蟋句呵｣懊↓菴ｿ縺・ゅ・繝・・螟悶∈縺ｮ遘ｻ蜍輔・繝ｩ繝ｳ繧ｿ繧､繝縺ｮ遽・峇蛻､螳壹〒諡貞凄縺吶ｋ縲・
                // 3x3蟆上せ繝・・繧ｸ逕滓・譎ゅ・幕蟋句慍轤ｹ・医せ繧ｿ繝ｼ繝医ヴ繝ｼ繧ｹ・峨′螟也ｸ・ｼ・ridX/Z縺檎ｫｯ・峨↓縺ゅｋ蝣ｴ蜷医・
                // 螟門・縺ｮ1繝槭せ縺ｯApplyBoundaryClosure縺ｫ繧医▲縺ｦ蠑ｷ蛻ｶ逧・↓螢・ｼ・locked・峨↓縺輔ｌ縺ｾ縺吶・
                // 縺昴％縺ｫ繝励Ξ繧､繝､繝ｼ縺後せ繝昴・繝ｳ縺励※螢√・荳ｭ縺ｫ蝓九∪繧九・繧帝∩縺代ｋ縺溘ａ縲√せ繝昴・繝ｳ繧ｻ繝ｫ繧・繝槭せ蜀・・縺ｫ縺壹ｉ縺励∪縺吶・
                TryAppendCandidateFromCell(
                    placements,
                    layers,
                    placementIndex,
                    startReturn.cell.x,
                    startReturn.cell.z,
                    true,
                    candidates);
            }
        }

        for (size_t placementIndex = 0; placementIndex < placements.size() && placementIndex < layers.size(); ++placementIndex)
        {
            const NarakuPiece::PieceData& piece = placements[placementIndex].piece->data;
            const int cellCountZ = piece.gridDepth - 1;
            const int cellCountX = piece.gridWidth - 1;
            for (int cellZ = 0; cellZ < cellCountZ; ++cellZ)
            {
                for (int cellX = 0; cellX < cellCountX; ++cellX)
                {
                    TryAppendCandidateFromCell(
                        placements,
                        layers,
                        placementIndex,
                        cellX,
                        cellZ,
                        false,
                        candidates);
                }
            }
        }

        return candidates;
    }

    bool TrySelectStartAndReturnPoints(
        const std::vector<GridPlacement>& placements,
        const std::vector<NarakuMap::TerrainLayer>& layers,
        NarakuMap::LayerPoint& outStartPoint,
        NarakuMap::LayerPoint& outReturnPoint,
        std::string* outError)
    {
        const std::vector<LayerPointCandidate> candidates = CollectPointCandidates(placements, layers);
        if (candidates.empty())
        {
            if (outError != nullptr)
            {
                *outError = "no usable start/return cells found in generated map";
            }
            return false;
        }

        size_t startCandidateIndex = 0;
        for (size_t index = 0; index < candidates.size(); ++index)
        {
            if (candidates[index].preferred)
            {
                startCandidateIndex = index;
                break;
            }
        }

        const LayerPointCandidate& startCandidate = candidates[startCandidateIndex];
        outStartPoint = startCandidate.point;

        const NarakuMap::TerrainLayer& startLayer = layers[startCandidate.placementIndex];
        int bestReturnScore = -1;
        for (int cellZ = 0; cellZ < startLayer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < startLayer.gridWidth - 1; ++cellX)
            {
                if (cellX == startCandidate.cellX && cellZ == startCandidate.cellZ)
                {
                    continue;
                }
                if (!IsCellUsableForPoint(startLayer, cellX, cellZ))
                {
                    continue;
                }

                const int score = std::abs(cellX - startCandidate.cellX) + std::abs(cellZ - startCandidate.cellZ);
                if (score > bestReturnScore)
                {
                    bestReturnScore = score;
                    outReturnPoint = BuildLayerPoint(startLayer, { cellX, cellZ });
                }
            }
        }

        if (bestReturnScore < 0)
        {
            // Fallback to start when no other usable cell exists on the start layer.
            outReturnPoint = outStartPoint;
        }

        return true;
    }
}
namespace NarakuStageGenerator
{
    bool GenerateFixed3x3Map(const wchar_t* outputMapPath, std::string* outError)
    {
        std::stringstream logStream;
        logStream << "--- Start Naraku Map Generation (3x3) ---\n";

        // Outer edge cells remain valid; runtime bounds reject moves outside the map.
        struct CellPos { int x, z; };
        const std::vector<CellPos> startPositions = { {1, 0}, {0, 1}, {2, 1}, {1, 2} };
        int startPosIndex = std::rand() % 4;
        int startX = startPositions[startPosIndex].x;
        int startZ = startPositions[startPosIndex].z;
        logStream << "Designated Start Piece Location: (" << startX << ", " << startZ << ")\n";

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
                if (!SelectPieceForCell(candidates, placements, gridX, gridZ, startX, startZ, placement, logStream, outError))
                {
                    std::string fullLog = logStream.str();
                    WriteLogToFile(fullLog);
                    OutputDebugStringA(fullLog.c_str());
                    if (outError != nullptr)
                    {
                        *outError = fullLog;
                    }
                    return false;
                }
                placements.push_back(placement);
            }
        }

        NarakuMap::MapData mapData = {};
        mapData.autoFallStartHeight = 0.90f;
        mapData.pieceNames.resize(placements.size());

        for (size_t index = 0; index < placements.size(); ++index)
        {
            const GridPlacement& placement = placements[index];
            
            std::wstring wpath = placement.piece->sourcePath;
            size_t lastSlash = wpath.find_last_of(L"\\/");
            std::wstring wfilename = (lastSlash == std::wstring::npos) ? wpath : wpath.substr(lastSlash + 1);
            mapData.pieceNames[index] = std::string(wfilename.begin(), wfilename.end());

            NarakuMap::TerrainLayer layer = BuildTerrainLayer(placement, static_cast<int>(index));
            AppendMiningPoints(placement, layer, mapData);
            mapData.terrainLayers.push_back(layer);
        }

        if (!TrySelectStartAndReturnPoints(
            placements,
            mapData.terrainLayers,
            mapData.playerStartPoint,
            mapData.returnPoint,
            outError))
        {
            return false;
        }

        if (HasMapValidationError(mapData, outError))
        {
            return false;
        }

        const wchar_t* savePath = (outputMapPath == nullptr || outputMapPath[0] == L'\0')
            ? kDefaultOutputMapPath
            : outputMapPath;

        logStream << "--- Map Generation Successful! ---\n";
        std::string fullLog = logStream.str();
        WriteLogToFile(fullLog);
        OutputDebugStringA(fullLog.c_str());

        return NarakuMap::SaveMap(savePath, mapData, outError);
    }
}
