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
    constexpr int k3x3GridSize = 3;
    constexpr int k4x4GridSize = 4;
    constexpr int kPieceVertexSize = 16;
    const wchar_t* kDefault3x3OutputMapPath = L"Assets\\Maps\\generated_naraku_map.json";
    const wchar_t* kDefault4x4OutputMapPath = L"Assets\\Maps\\generated_naraku_map_4x4.json";
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
        bool usesCentralFallback = false;
        bool isStartReturnPlacement = false;
    };

    struct LayerPointCandidate
    {
        NarakuMap::LayerPoint point = {};
        size_t placementIndex = 0;
        int cellX = 0;
        int cellZ = 0;
        bool preferred = false;
    };

    enum class PlacementCategory
    {
        Central = 0,
        NorthWall,
        EastWall,
        SouthWall,
        WestWall,
        NorthEastCorner,
        SouthEastCorner,
        SouthWestCorner,
        NorthWestCorner,
        Count,
        Invalid,
    };

    struct CandidatePools
    {
        std::vector<const LoadedPiece*> pools[static_cast<int>(PlacementCategory::Count)];
        std::vector<std::string> exclusionLogs;
    };

    struct CellRequirement
    {
        PlacementCategory category = PlacementCategory::Central;
        bool outerNorth = false;
        bool outerSouth = false;
        bool outerEast = false;
        bool outerWest = false;
    };

    const char* ToString(PlacementCategory category)
    {
        switch (category)
        {
        case PlacementCategory::Central: return "Central";
        case PlacementCategory::NorthWall: return "NorthWall";
        case PlacementCategory::EastWall: return "EastWall";
        case PlacementCategory::SouthWall: return "SouthWall";
        case PlacementCategory::WestWall: return "WestWall";
        case PlacementCategory::NorthEastCorner: return "NorthEastCorner";
        case PlacementCategory::SouthEastCorner: return "SouthEastCorner";
        case PlacementCategory::SouthWestCorner: return "SouthWestCorner";
        case PlacementCategory::NorthWestCorner: return "NorthWestCorner";
        default: return "Invalid";
        }
    }

    int CountBlockedEdges(const NarakuPiece::EdgeCategories& edges)
    {
        int blockedCount = 0;
        blockedCount += (edges.north == NarakuPiece::StageCategory::Blocked) ? 1 : 0;
        blockedCount += (edges.south == NarakuPiece::StageCategory::Blocked) ? 1 : 0;
        blockedCount += (edges.east == NarakuPiece::StageCategory::Blocked) ? 1 : 0;
        blockedCount += (edges.west == NarakuPiece::StageCategory::Blocked) ? 1 : 0;
        return blockedCount;
    }

    bool ClassifyPlacementCategory(
        const LoadedPiece& piece,
        PlacementCategory& outCategory,
        std::string& outExclusionReason)
    {
        const NarakuPiece::EdgeCategories& edges = piece.data.edgeCategories;
        const bool northBlocked = edges.north == NarakuPiece::StageCategory::Blocked;
        const bool southBlocked = edges.south == NarakuPiece::StageCategory::Blocked;
        const bool eastBlocked = edges.east == NarakuPiece::StageCategory::Blocked;
        const bool westBlocked = edges.west == NarakuPiece::StageCategory::Blocked;
        const int blockedCount = CountBlockedEdges(edges);

        switch (blockedCount)
        {
        case 0:
            outCategory = PlacementCategory::Central;
            return true;
        case 1:
            if (northBlocked) outCategory = PlacementCategory::NorthWall;
            else if (eastBlocked) outCategory = PlacementCategory::EastWall;
            else if (southBlocked) outCategory = PlacementCategory::SouthWall;
            else outCategory = PlacementCategory::WestWall;
            return true;
        case 2:
            if (northBlocked && eastBlocked)
            {
                outCategory = PlacementCategory::NorthEastCorner;
                return true;
            }
            if (southBlocked && eastBlocked)
            {
                outCategory = PlacementCategory::SouthEastCorner;
                return true;
            }
            if (southBlocked && westBlocked)
            {
                outCategory = PlacementCategory::SouthWestCorner;
                return true;
            }
            if (northBlocked && westBlocked)
            {
                outCategory = PlacementCategory::NorthWestCorner;
                return true;
            }
            outExclusionReason = "Excluded from fixed-grid placement: opposite blocked edges are not supported";
            return false;
        case 3:
            outExclusionReason = "Excluded from fixed-grid placement: three blocked edges are not supported";
            return false;
        case 4:
            outExclusionReason = "Excluded from fixed-grid placement: four blocked edges are not supported";
            return false;
        default:
            outExclusionReason = "Excluded from fixed-grid placement: unsupported blocked edge pattern";
            return false;
        }
    }

    CellRequirement GetCellRequirement(int gridX, int gridZ, int gridSize)
    {
        CellRequirement requirement = {};
        requirement.outerNorth = (gridZ == 0);
        requirement.outerSouth = (gridZ == gridSize - 1);
        requirement.outerWest = (gridX == 0);
        requirement.outerEast = (gridX == gridSize - 1);

        if (requirement.outerNorth && requirement.outerWest) requirement.category = PlacementCategory::NorthWestCorner;
        else if (requirement.outerNorth && requirement.outerEast) requirement.category = PlacementCategory::NorthEastCorner;
        else if (requirement.outerSouth && requirement.outerEast) requirement.category = PlacementCategory::SouthEastCorner;
        else if (requirement.outerSouth && requirement.outerWest) requirement.category = PlacementCategory::SouthWestCorner;
        else if (requirement.outerNorth) requirement.category = PlacementCategory::NorthWall;
        else if (requirement.outerEast) requirement.category = PlacementCategory::EastWall;
        else if (requirement.outerSouth) requirement.category = PlacementCategory::SouthWall;
        else if (requirement.outerWest) requirement.category = PlacementCategory::WestWall;
        else requirement.category = PlacementCategory::Central;
        return requirement;
    }

    std::vector<NarakuPiece::GridPoint> BuildStartPositions(int gridSize)
    {
        std::vector<NarakuPiece::GridPoint> positions;
        for (int offset = 1; offset < gridSize - 1; ++offset)
        {
            positions.push_back({ offset, 0 });
            positions.push_back({ gridSize - 1, offset });
            positions.push_back({ offset, gridSize - 1 });
            positions.push_back({ 0, offset });
        }
        return positions;
    }

    const GridPlacement* FindPlacement(
        const std::vector<GridPlacement>& placements,
        int gridX,
        int gridZ)
    {
        for (const GridPlacement& placement : placements)
        {
            if (placement.gridX == gridX && placement.gridZ == gridZ)
            {
                return &placement;
            }
        }
        return nullptr;
    }

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

    bool LoadCandidatePieces(std::vector<LoadedPiece>& outPieces, CandidatePools& outPools, std::string* outError)
    {
        outPieces.clear();
        outPools = {};

        std::vector<std::wstring> piecePaths;
        if (!EnumerateCompletedPiecePaths(piecePaths, outError))
        {
            return false;
        }
        outPieces.reserve(piecePaths.size());

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

            PlacementCategory category = PlacementCategory::Invalid;
            std::string exclusionReason;
            const LoadedPiece& storedPiece = outPieces.back();
            if (!ClassifyPlacementCategory(storedPiece, category, exclusionReason))
            {
                outPools.exclusionLogs.push_back(
                    "Excluded piece \"" + WideToUtf8(GetFileNamePart(storedPiece.sourcePath)) + "\": " + exclusionReason);
                continue;
            }

            outPools.pools[static_cast<int>(category)].push_back(&storedPiece);
        }

        if (outPieces.empty())
        {
            if (outError != nullptr)
            {
                *outError = "no valid 16x16 completed pieces found";
            }
            return false;
        }

        bool hasSupportedCandidate = false;
        for (int index = 0; index < static_cast<int>(PlacementCategory::Count); ++index)
        {
            if (!outPools.pools[index].empty())
            {
                hasSupportedCandidate = true;
                break;
            }
        }
        if (!hasSupportedCandidate)
        {
            if (outError != nullptr)
            {
                *outError = "no candidates matched the fixed-grid blocked-edge categories";
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
        int gridSize,
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
        if (gridZ == gridSize - 1)
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
        if (gridX == gridSize - 1)
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

    /** @brief 中央ステージを仮壁として使う際に、内側接続だけを検証します。 */
    bool MatchesInnerGridConstraints(
        int gridX,
        int gridZ,
        int gridSize,
        const GridPlacement* northPlacement,
        const GridPlacement* westPlacement,
        const LoadedPiece& candidate,
        std::string* outMismatchReason)
    {
        const auto setReason = [&](const std::string& message) {
            if (outMismatchReason != nullptr)
            {
                *outMismatchReason = message;
            }
        };

        if (gridZ > 0)
        {
            if (candidate.data.edgeCategories.north == NarakuPiece::StageCategory::Blocked)
            {
                setReason("North edge is Blocked (forbidden for inner connection)");
                return false;
            }
            if (northPlacement != nullptr &&
                northPlacement->piece->data.edgeCategories.south != candidate.data.edgeCategories.north)
            {
                setReason("North inner edge category does not match");
                return false;
            }
        }

        if (gridZ < gridSize - 1 && candidate.data.edgeCategories.south == NarakuPiece::StageCategory::Blocked)
        {
            setReason("South edge is Blocked (forbidden for inner connection)");
            return false;
        }
        if (gridX > 0)
        {
            if (candidate.data.edgeCategories.west == NarakuPiece::StageCategory::Blocked)
            {
                setReason("West edge is Blocked (forbidden for inner connection)");
                return false;
            }
            if (westPlacement != nullptr &&
                westPlacement->piece->data.edgeCategories.east != candidate.data.edgeCategories.west)
            {
                setReason("West inner edge category does not match");
                return false;
            }
        }

        if (gridX < gridSize - 1 && candidate.data.edgeCategories.east == NarakuPiece::StageCategory::Blocked)
        {
            setReason("East edge is Blocked (forbidden for inner connection)");
            return false;
        }
        return true;
    }

    bool SelectPieceForCell(
        const CandidatePools& candidatePools,
        const std::vector<GridPlacement>& placements,
        int gridX,
        int gridZ,
        int gridSize,
        int startX,
        int startZ,
        bool allowCentralFallback,
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

        if (gridZ == gridSize - 1) logStream << "  - South: Blocked (Bottom border)\n";
        else logStream << "  - South: Not Blocked\n";

        if (gridX == 0) logStream << "  - West: Blocked (Left border)\n";
        else if (westPlacement != nullptr) logStream << "  - West: Connect to \"" << WideToUtf8(GetFileNamePart(westPlacement->piece->sourcePath)) << "\" East (" << NarakuPiece::ToString(westPlacement->piece->data.edgeCategories.east) << ")\n";
        else logStream << "  - West: Not Blocked\n";

        if (gridX == gridSize - 1) logStream << "  - East: Blocked (Right border)\n";
        else logStream << "  - East: Not Blocked\n";

        const bool isStartCell = (gridX == startX && gridZ == startZ);
        const CellRequirement requirement = GetCellRequirement(gridX, gridZ, gridSize);
        const std::vector<const LoadedPiece*>* pool =
            &candidatePools.pools[static_cast<int>(requirement.category)];
        const bool hasStartCandidate = std::any_of(
            pool->begin(),
            pool->end(),
            [](const LoadedPiece* piece) { return IsStartReturnPreferred(piece->data); });
        bool usesCentralFallback = allowCentralFallback &&
            (pool->empty() || (isStartCell && !hasStartCandidate));
        if (usesCentralFallback)
        {
            pool = &candidatePools.pools[static_cast<int>(PlacementCategory::Central)];
            logStream << "  - Central fallback enabled for " << ToString(requirement.category) << "\n";
        }

        // Filter compatible candidates
        std::vector<const LoadedPiece*> matchedCandidates;
        std::vector<std::string> mismatchLogs;

        for (const LoadedPiece* candidate : *pool)
        {
            std::string mismatchReason;
            bool matches = usesCentralFallback
                ? MatchesInnerGridConstraints(gridX, gridZ, gridSize, northPlacement, westPlacement, *candidate, &mismatchReason)
                : MatchesGridConstraints(gridX, gridZ, gridSize, northPlacement, westPlacement, *candidate, &mismatchReason);

            if (matches && candidate->data.layerTransition.role != NarakuPiece::LayerTransitionRole::None)
            {
                matches = false;
                mismatchReason = "layer transition piece is reserved for a required gate slot";
            }

            const bool isCandidateStart = IsStartReturnPreferred(candidate->data);

            if (isStartCell)
            {
                if (matches && !isCandidateStart && !usesCentralFallback)
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

            const bool requireDistinctCentral = gridSize == k4x4GridSize &&
                requirement.category == PlacementCategory::Central &&
                candidatePools.pools[static_cast<int>(PlacementCategory::Central)].size() >= 4;
            if (matches && requireDistinctCentral)
            {
                const bool alreadySelected = std::any_of(
                    placements.begin(),
                    placements.end(),
                    [candidate, gridSize](const GridPlacement& placement)
                    {
                        return GetCellRequirement(placement.gridX, placement.gridZ, gridSize).category == PlacementCategory::Central &&
                            placement.piece == candidate;
                    });
                if (alreadySelected)
                {
                    matches = false;
                    mismatchReason = "Central piece is already used in this 4x4 map";
                }
            }

            std::string fileName = WideToUtf8(GetFileNamePart(candidate->sourcePath));
            if (matches)
            {
                matchedCandidates.push_back(candidate);
            }
            else
            {
                mismatchLogs.push_back("  - \"" + fileName + "\": " + mismatchReason);
            }
        }

        if (matchedCandidates.empty() && allowCentralFallback && !usesCentralFallback &&
            requirement.category != PlacementCategory::Central)
        {
            logStream << "  - Central fallback enabled because no edge-category-compatible candidate remained.\n";
            usesCentralFallback = true;
            for (const LoadedPiece* candidate : candidatePools.pools[static_cast<int>(PlacementCategory::Central)])
            {
                std::string mismatchReason;
                bool matches = MatchesInnerGridConstraints(
                    gridX,
                    gridZ,
                    gridSize,
                    northPlacement,
                    westPlacement,
                    *candidate,
                    &mismatchReason);
                if (matches && candidate->data.layerTransition.role != NarakuPiece::LayerTransitionRole::None)
                {
                    matches = false;
                    mismatchReason = "layer transition piece is reserved for a required gate slot";
                }
                const bool isCandidateStart = IsStartReturnPreferred(candidate->data);
                if ((isStartCell && !isCandidateStart && !usesCentralFallback) ||
                    (!isStartCell && isCandidateStart))
                {
                    matches = false;
                    mismatchReason = isStartCell
                        ? "stageRole is not StartReturn (required for designated start cell)"
                        : "stageRole is StartReturn (forbidden for non-start cell)";
                }

                if (matches)
                {
                    matchedCandidates.push_back(candidate);
                }
                else
                {
                    mismatchLogs.push_back(
                        "  - Central fallback \"" + WideToUtf8(GetFileNamePart(candidate->sourcePath)) + "\": " + mismatchReason);
                }
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
        outPlacement.usesCentralFallback = usesCentralFallback;
        outPlacement.isStartReturnPlacement = isStartCell;
        return true;
    }

    float ComputeLayerCenterCoord(int gridIndex, int gridSize, int cellCount, float cellSize)
    {
        const float layerSpan = static_cast<float>(cellCount) * cellSize;
        const float wholeSpan = static_cast<float>(gridSize * cellCount) * cellSize;
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

    void ApplyBoundaryClosure(const GridPlacement& placement, int gridSize, NarakuMap::TerrainLayer& layer)
    {
        const bool hasNorthNeighbor = placement.gridZ > 0;
        const bool hasSouthNeighbor = placement.gridZ < (gridSize - 1);
        const bool hasWestNeighbor = placement.gridX > 0;
        const bool hasEastNeighbor = placement.gridX < (gridSize - 1);
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

    /** @brief 仮壁にした中央ステージの外周セルだけを通行不可にします。 */
    void ApplyCentralFallbackWalls(const GridPlacement& placement, int gridSize, NarakuMap::TerrainLayer& layer)
    {
        if (!placement.usesCentralFallback)
        {
            return;
        }

        const int cellWidth = layer.gridWidth - 1;
        const int cellHeight = layer.gridHeight - 1;
        const std::uint32_t blocked = NarakuMap::CellAttributeBlocked;
        const auto blockCell = [&](int cellX, int cellZ)
        {
            const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
            NarakuMap::SetCellAttributeFlags(layer, cellX, cellZ, flags | blocked);
        };

        if (placement.gridZ == 0)
        {
            for (int cellX = 0; cellX < cellWidth; ++cellX) blockCell(cellX, 0);
        }
        if (placement.gridZ == gridSize - 1)
        {
            for (int cellX = 0; cellX < cellWidth; ++cellX) blockCell(cellX, cellHeight - 1);
        }
        if (placement.gridX == 0)
        {
            for (int cellZ = 0; cellZ < cellHeight; ++cellZ) blockCell(0, cellZ);
        }
        if (placement.gridX == gridSize - 1)
        {
            for (int cellZ = 0; cellZ < cellHeight; ++cellZ) blockCell(cellWidth - 1, cellZ);
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

    NarakuMap::TerrainLayer BuildTerrainLayer(const GridPlacement& placement, int gridSize, int layerId)
    {
        const NarakuPiece::PieceData& piece = placement.piece->data;

        NarakuMap::TerrainLayer layer = {};
        layer.id = layerId;
        layer.gridWidth = piece.gridWidth;
        layer.gridHeight = piece.gridDepth;
        layer.cellSize = piece.cellSize;
        layer.center.x = ComputeLayerCenterCoord(placement.gridX, gridSize, piece.gridWidth - 1, piece.cellSize);
        layer.center.z = ComputeLayerCenterCoord(placement.gridZ, gridSize, piece.gridDepth - 1, piece.cellSize);
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

        ApplyBoundaryClosure(placement, gridSize, layer);
        ApplyCentralFallbackWalls(placement, gridSize, layer);
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

    void AppendEnvironmentObjects(const GridPlacement& placement, const NarakuMap::TerrainLayer& layer, NarakuMap::MapData& mapData)
    {
        const NarakuPiece::PieceData& piece = placement.piece->data;
        for (const NarakuPiece::EnvironmentObjectData& pieceObject : piece.environmentObjects)
        {
            NarakuMap::EnvironmentObject mapObject = {};
            mapObject.modelId = pieceObject.modelId;
            mapObject.xz = ComputeCellCenterWorld(layer, pieceObject.cell.x, pieceObject.cell.z);
            mapObject.layerId = layer.id;
            mapObject.scaleX = pieceObject.scaleX;
            mapObject.scaleY = pieceObject.scaleY;
            mapObject.scaleZ = pieceObject.scaleZ;
            mapData.environmentObjects.push_back(mapObject);
        }
    }

    void AppendRopePoint(const GridPlacement& placement, const NarakuMap::TerrainLayer& layer, NarakuMap::MapData& mapData)
    {
        const NarakuPiece::PieceData& piece = placement.piece->data;
        if (!piece.rope.enabled)
        {
            return;
        }

        NarakuMap::RopePoint rope = {};
        rope.topXZ = ComputeCellCenterWorld(layer, piece.rope.top.x, piece.rope.top.z);
        rope.bottomXZ = ComputeCellCenterWorld(layer, piece.rope.bottom.x, piece.rope.bottom.z);
        rope.topLayerId = layer.id;
        rope.bottomLayerId = layer.id;
        mapData.ropes.push_back(rope);
    }

    void AppendLayerGatePoint(const GridPlacement& placement, const NarakuMap::TerrainLayer& layer, NarakuMap::MapData& mapData)
    {
        const NarakuPiece::LayerTransitionData& transition = placement.piece->data.layerTransition;
        if (transition.role == NarakuPiece::LayerTransitionRole::None || !transition.ropePointEnabled)
        {
            return;
        }

        NarakuMap::LayerGatePoint gate = {};
        gate.isEntry = transition.role == NarakuPiece::LayerTransitionRole::Entry;
        gate.ropeXZ = ComputeCellCenterWorld(layer, transition.ropePoint.x, transition.ropePoint.z);
        gate.loadXZ = transition.loadPointEnabled
            ? ComputeCellCenterWorld(layer, transition.loadPoint.x, transition.loadPoint.z)
            : gate.ropeXZ;
        gate.layerId = layer.id;
        mapData.layerGates.push_back(gate);
    }

    bool HasSameEdgeCategories(const NarakuPiece::PieceData& lhs, const NarakuPiece::PieceData& rhs)
    {
        return lhs.edgeCategories.north == rhs.edgeCategories.north &&
            lhs.edgeCategories.south == rhs.edgeCategories.south &&
            lhs.edgeCategories.east == rhs.edgeCategories.east &&
            lhs.edgeCategories.west == rhs.edgeCategories.west;
    }

    bool InjectLayerTransitionPiece(
        NarakuPiece::LayerTransitionRole role,
        const std::vector<LoadedPiece>& candidates,
        std::vector<GridPlacement>& placements,
        std::stringstream& logStream)
    {
        struct Replacement
        {
            size_t placementIndex = 0;
            const LoadedPiece* piece = nullptr;
        };
        std::vector<Replacement> replacements;
        for (const LoadedPiece& candidate : candidates)
        {
            if (candidate.data.layerTransition.role != role)
            {
                continue;
            }
            for (size_t index = 0; index < placements.size(); ++index)
            {
                const GridPlacement& placement = placements[index];
                if (placement.isStartReturnPlacement ||
                    placement.piece->data.layerTransition.role != NarakuPiece::LayerTransitionRole::None ||
                    !HasSameEdgeCategories(placement.piece->data, candidate.data))
                {
                    continue;
                }
                replacements.push_back({ index, &candidate });
            }
        }

        if (replacements.empty())
        {
            logStream << "No compatible " << (role == NarakuPiece::LayerTransitionRole::Entry ? "entry" : "exit")
                << " transition piece was found.\n";
            return false;
        }

        const Replacement& selected = replacements[std::rand() % static_cast<int>(replacements.size())];
        placements[selected.placementIndex].piece = selected.piece;
        return true;
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
            else if (placements[placementIndex].isStartReturnPlacement)
            {
                const NarakuMap::TerrainLayer& layer = layers[placementIndex];
                bool appended = false;
                for (int cellZ = 0; cellZ < layer.gridHeight - 1 && !appended; ++cellZ)
                {
                    for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
                    {
                        appended = TryAppendCandidateFromCell(
                            placements,
                            layers,
                            placementIndex,
                            cellX,
                            cellZ,
                            true,
                            candidates);
                        if (appended)
                        {
                            break;
                        }
                    }
                }
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
        outReturnPoint = startCandidate.point;

        return true;
    }
}
namespace NarakuStageGenerator
{
    namespace
    {
    bool GenerateFixedGridMap(
        int gridSize,
        const char* gridLabel,
        const wchar_t* defaultOutputMapPath,
        const wchar_t* outputMapPath,
        bool requireLayerEntry,
        bool requireLayerExit,
        std::string* outError)
    {
        std::stringstream logStream;
        logStream << "--- Start Naraku Map Generation (" << gridLabel << ") ---\n";

        std::vector<LoadedPiece> candidates;
        CandidatePools candidatePools;
        if (!LoadCandidatePieces(candidates, candidatePools, outError))
        {
            return false;
        }

        std::vector<GridPlacement> placements;
        const std::vector<NarakuPiece::GridPoint> startPositions = BuildStartPositions(gridSize);
        const auto tryBuildPlacements = [&](bool allowCentralFallback, int attemptIndex) -> bool
        {
            placements.clear();
            placements.reserve(gridSize * gridSize);
            const NarakuPiece::GridPoint& startPosition =
                startPositions[std::rand() % static_cast<int>(startPositions.size())];
            logStream << "Attempt " << attemptIndex << ": Start piece location: ("
                      << startPosition.x << ", " << startPosition.z << ")"
                      << (allowCentralFallback ? " [central fallback enabled]\n" : "\n");

            for (int gridZ = 0; gridZ < gridSize; ++gridZ)
            {
                for (int gridX = 0; gridX < gridSize; ++gridX)
                {
                    GridPlacement placement = {};
                    if (!SelectPieceForCell(
                        candidatePools,
                        placements,
                        gridX,
                        gridZ,
                        gridSize,
                        startPosition.x,
                        startPosition.z,
                        allowCentralFallback,
                        placement,
                        logStream,
                        nullptr))
                    {
                        logStream << "Attempt " << attemptIndex << " failed at cell ("
                                  << gridX << ", " << gridZ << ").\n";
                        return false;
                    }
                    placements.push_back(placement);
                }
            }
            return true;
        };

        constexpr int kMaximumNormalAttempts = 100;
        bool built = false;
        for (int attemptIndex = 1; attemptIndex <= kMaximumNormalAttempts; ++attemptIndex)
        {
            if (tryBuildPlacements(false, attemptIndex))
            {
                built = true;
                break;
            }
        }
        if (!built)
        {
            logStream << "Normal placement failed after 100 attempts. Trying central fallback.\n";
            built = tryBuildPlacements(true, kMaximumNormalAttempts + 1);
        }
        if (!built)
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

        if (requireLayerEntry && !InjectLayerTransitionPiece(
            NarakuPiece::LayerTransitionRole::Entry, candidates, placements, logStream))
        {
            if (outError != nullptr) *outError = logStream.str();
            return false;
        }
        if (requireLayerExit && !InjectLayerTransitionPiece(
            NarakuPiece::LayerTransitionRole::Exit, candidates, placements, logStream))
        {
            if (outError != nullptr) *outError = logStream.str();
            return false;
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

            NarakuMap::TerrainLayer layer = BuildTerrainLayer(placement, gridSize, static_cast<int>(index));
            AppendMiningPoints(placement, layer, mapData);
            AppendEnvironmentObjects(placement, layer, mapData);
            AppendRopePoint(placement, layer, mapData);
            AppendLayerGatePoint(placement, layer, mapData);
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
            ? defaultOutputMapPath
            : outputMapPath;

        logStream << "--- Map Generation Successful! ---\n";
        std::string fullLog = logStream.str();
        WriteLogToFile(fullLog);
        OutputDebugStringA(fullLog.c_str());

        return NarakuMap::SaveMap(savePath, mapData, outError);
    }
    }

    bool GenerateFixed3x3Map(const wchar_t* outputMapPath, std::string* outError)
    {
        return GenerateFixedGridMap(
            k3x3GridSize,
            "3x3",
            kDefault3x3OutputMapPath,
            outputMapPath,
            false,
            false,
            outError);
    }

    bool GenerateFixed4x4Map(const wchar_t* outputMapPath, std::string* outError)
    {
        return GenerateFixedGridMap(
            k4x4GridSize,
            "4x4",
            kDefault4x4OutputMapPath,
            outputMapPath,
            false,
            false,
            outError);
    }

    bool GenerateFixed4x4AreaMap(
        const wchar_t* outputMapPath,
        bool requireLayerEntry,
        bool requireLayerExit,
        std::string* outError)
    {
        return GenerateFixedGridMap(
            k4x4GridSize,
            "4x4 area",
            kDefault4x4OutputMapPath,
            outputMapPath,
            requireLayerEntry,
            requireLayerExit,
            outError);
    }
}
