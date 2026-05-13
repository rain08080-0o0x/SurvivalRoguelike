#include "CastleSaveData.h"

#include "SceneCastleEditor.h"

#include <DirectXMath.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    constexpr char kDefaultCastleSavePath[] = "Assets/castle_editor_save.cfg";

    struct SerializablePlacement
    {
        const char* assetId = nullptr;
        int gridX = 0;
        int gridY = 0;
        int gridZ = 0;
        int rotationQuarterTurns = 0;
    };

    const char* ResolveCastleSavePath(const char* path)
    {
        return (path && path[0]) ? path : kDefaultCastleSavePath;
    }

    struct LoadedPlacement
    {
        int assetIndex = -1;
        int gridX = 0;
        int gridY = 0;
        int gridZ = 0;
        int rotationQuarterTurns = 0;
    };

    struct LoadedState
    {
        float gridSize = 1.0f;
        int gridHalfExtent = 16;
        float gridHeightStep = 1.0f;
        int activeLayer = 0;
        DirectX::XMFLOAT3 cameraEye{ 8.0f, 10.0f, -8.0f };
        DirectX::XMFLOAT3 cameraLook{ 0.0f, 0.0f, 0.0f };
        int previewRotationQuarterTurns = 0;
        std::vector<LoadedPlacement> placements;
    };

    void Trim(std::string& text)
    {
        const char* whitespace = " \t\r\n";
        const size_t begin = text.find_first_not_of(whitespace);
        if (begin == std::string::npos)
        {
            text.clear();
            return;
        }

        const size_t end = text.find_last_not_of(whitespace);
        text = text.substr(begin, end - begin + 1);
    }

    bool TryGetValue(
        const std::unordered_map<std::string, std::string>& values,
        const char* key,
        std::string& outValue)
    {
        const auto it = values.find(key);
        if (it == values.end())
        {
            return false;
        }

        outValue = it->second;
        return true;
    }

    bool TryParseInt(
        const std::unordered_map<std::string, std::string>& values,
        const char* key,
        int& outValue)
    {
        std::string value;
        if (!TryGetValue(values, key, value))
        {
            return false;
        }

        char* end = nullptr;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (!end || *end != '\0')
        {
            return false;
        }

        outValue = static_cast<int>(parsed);
        return true;
    }

    bool TryParseFloat(
        const std::unordered_map<std::string, std::string>& values,
        const char* key,
        float& outValue)
    {
        std::string value;
        if (!TryGetValue(values, key, value))
        {
            return false;
        }

        char* end = nullptr;
        const float parsed = std::strtof(value.c_str(), &end);
        if (!end || *end != '\0')
        {
            return false;
        }

        outValue = parsed;
        return true;
    }

    int FindAssetIndexById(const SceneCastleEditor& editor, const std::string& assetId)
    {
        for (int i = 0; i < editor.GetAssetCount(); ++i)
        {
            const SceneCastleEditor::AssetInfo* asset = editor.GetAssetInfo(i);
            if (!asset || asset->assetId.empty()) continue;
            if (assetId == asset->assetId)
            {
                return i;
            }
        }

        return -1;
    }

    bool IsInsideLoadedGrid(int gridHalfExtent, int gridX, int gridY, int gridZ)
    {
        if (gridY < 0 || gridY > 16)
        {
            return false;
        }

        return std::abs(gridX) <= gridHalfExtent && std::abs(gridZ) <= gridHalfExtent;
    }

    bool LoadFromStream(SceneCastleEditor& editor, std::istream& input, LoadedState& outState)
    {
        std::unordered_map<std::string, std::string> values;
        std::string line;
        while (std::getline(input, line))
        {
            Trim(line);
            if (line.empty()) continue;
            if (line[0] == '#') continue;

            const size_t sep = line.find('=');
            if (sep == std::string::npos) continue;

            std::string key = line.substr(0, sep);
            std::string value = line.substr(sep + 1);
            Trim(key);
            Trim(value);
            if (key.empty() || value.empty()) continue;

            values[key] = value;
        }

        std::string format;
        if (!TryGetValue(values, "format", format) || format != "CastleSaveData")
        {
            return false;
        }

        int version = 0;
        if (!TryParseInt(values, "version", version) || version != 1)
        {
            return false;
        }

        LoadedState loaded;
        loaded.gridSize = editor.GetGridSize();
        loaded.gridHalfExtent = editor.GetGridHalfExtent();
        loaded.gridHeightStep = editor.GetGridHeightStep();
        loaded.activeLayer = editor.GetActiveLayer();
        loaded.cameraEye = editor.GetCameraEye();
        loaded.cameraLook = editor.GetCameraLook();
        loaded.previewRotationQuarterTurns = editor.GetPreviewRotationQuarterTurns();

        if (!TryParseFloat(values, "gridSize", loaded.gridSize)) return false;
        if (!TryParseInt(values, "gridHalfExtent", loaded.gridHalfExtent)) return false;
        if (!TryParseFloat(values, "gridHeightStep", loaded.gridHeightStep)) return false;
        if (!TryParseInt(values, "placementCount", version)) return false;

        const int placementCount = version;
        if (placementCount < 0)
        {
            return false;
        }

        TryParseInt(values, "activeLayer", loaded.activeLayer);
        TryParseFloat(values, "cameraEyeX", loaded.cameraEye.x);
        TryParseFloat(values, "cameraEyeY", loaded.cameraEye.y);
        TryParseFloat(values, "cameraEyeZ", loaded.cameraEye.z);
        TryParseFloat(values, "cameraLookX", loaded.cameraLook.x);
        TryParseFloat(values, "cameraLookY", loaded.cameraLook.y);
        TryParseFloat(values, "cameraLookZ", loaded.cameraLook.z);
        TryParseInt(values, "previewRotationQuarterTurns", loaded.previewRotationQuarterTurns);

        if (loaded.gridSize <= 0.0f) return false;
        if (loaded.gridHalfExtent < 0) return false;
        if (loaded.gridHeightStep <= 0.0f) return false;

        loaded.placements.reserve(static_cast<size_t>(placementCount));
        std::unordered_set<std::string> occupiedCells;
        occupiedCells.reserve(static_cast<size_t>(placementCount > 0 ? placementCount : 1));

        for (int i = 0; i < placementCount; ++i)
        {
            char key[64];
            std::string assetId;

            sprintf_s(key, "placement.%d.assetId", i);
            if (!TryGetValue(values, key, assetId))
            {
                return false;
            }

            LoadedPlacement placement;
            placement.assetIndex = FindAssetIndexById(editor, assetId);
            if (placement.assetIndex < 0)
            {
                return false;
            }

            sprintf_s(key, "placement.%d.gridX", i);
            if (!TryParseInt(values, key, placement.gridX)) return false;
            sprintf_s(key, "placement.%d.gridY", i);
            if (!TryParseInt(values, key, placement.gridY)) return false;
            sprintf_s(key, "placement.%d.gridZ", i);
            if (!TryParseInt(values, key, placement.gridZ)) return false;
            sprintf_s(key, "placement.%d.rotationQuarterTurns", i);
            if (!TryParseInt(values, key, placement.rotationQuarterTurns)) return false;

            placement.rotationQuarterTurns = ((placement.rotationQuarterTurns % 4) + 4) % 4;

            if (!IsInsideLoadedGrid(loaded.gridHalfExtent, placement.gridX, placement.gridY, placement.gridZ))
            {
                return false;
            }

            const std::string cellKey =
                std::to_string(placement.gridX) + "," +
                std::to_string(placement.gridY) + "," +
                std::to_string(placement.gridZ);
            if (!occupiedCells.insert(cellKey).second)
            {
                return false;
            }

            loaded.placements.push_back(placement);
        }

        outState = loaded;
        return true;
    }
}

bool CastleSaveData::Save(const SceneCastleEditor& editor, const char* path)
{
    std::vector<SerializablePlacement> placements;
    const int placementCount = editor.GetPlacementCount();
    placements.reserve((placementCount > 0) ? static_cast<size_t>(placementCount) : 0u);

    for (int i = 0; i < placementCount; ++i)
    {
        const SceneCastleEditor::PlacementInfo* placement = editor.GetPlacement(i);
        if (!placement) return false;

        const SceneCastleEditor::AssetInfo* asset = editor.GetAssetInfo(placement->assetIndex);
        if (!asset || asset->assetId.empty())
        {
            return false;
        }

        SerializablePlacement item;
        item.assetId = asset->assetId.c_str();
        item.gridX = placement->gridX;
        item.gridY = placement->gridY;
        item.gridZ = placement->gridZ;
        item.rotationQuarterTurns = ((placement->rotationQuarterTurns % 4) + 4) % 4;
        placements.push_back(item);
    }

    std::ofstream ofs(ResolveCastleSavePath(path), std::ios::trunc);
    if (!ofs.is_open())
    {
        return false;
    }

    const DirectX::XMFLOAT3 eye = editor.GetCameraEye();
    const DirectX::XMFLOAT3 look = editor.GetCameraLook();

    ofs << "# DX22 castle save\n";
    ofs << "format=CastleSaveData\n";
    ofs << "version=1\n";
    ofs << "gridSize=" << editor.GetGridSize() << "\n";
    ofs << "gridHalfExtent=" << editor.GetGridHalfExtent() << "\n";
    ofs << "gridHeightStep=" << editor.GetGridHeightStep() << "\n";
    ofs << "activeLayer=" << editor.GetActiveLayer() << "\n";
    ofs << "cameraEyeX=" << eye.x << "\n";
    ofs << "cameraEyeY=" << eye.y << "\n";
    ofs << "cameraEyeZ=" << eye.z << "\n";
    ofs << "cameraLookX=" << look.x << "\n";
    ofs << "cameraLookY=" << look.y << "\n";
    ofs << "cameraLookZ=" << look.z << "\n";
    ofs << "previewRotationQuarterTurns=" << editor.GetPreviewRotationQuarterTurns() << "\n";
    ofs << "placementCount=" << placementCount << "\n";

    for (int i = 0; i < placementCount; ++i)
    {
        const SerializablePlacement& placement = placements[static_cast<size_t>(i)];
        ofs << "placement." << i << ".assetId=" << placement.assetId << "\n";
        ofs << "placement." << i << ".gridX=" << placement.gridX << "\n";
        ofs << "placement." << i << ".gridY=" << placement.gridY << "\n";
        ofs << "placement." << i << ".gridZ=" << placement.gridZ << "\n";
        ofs << "placement." << i << ".rotationQuarterTurns=" << placement.rotationQuarterTurns << "\n";
    }

    return ofs.good();
}

bool CastleSaveData::Load(SceneCastleEditor& editor, const char* path)
{
    std::ifstream ifs(ResolveCastleSavePath(path));
    if (!ifs.is_open())
    {
        return false;
    }

    LoadedState loaded;
    if (!LoadFromStream(editor, ifs, loaded))
    {
        return false;
    }

    std::vector<CastlePlacementInfo> placements;
    placements.reserve(loaded.placements.size());
    for (const LoadedPlacement& loadedPlacement : loaded.placements)
    {
        CastlePlacementInfo placement;
        placement.assetIndex = loadedPlacement.assetIndex;
        placement.gridX = loadedPlacement.gridX;
        placement.gridY = loadedPlacement.gridY;
        placement.gridZ = loadedPlacement.gridZ;
        placement.rotationQuarterTurns = loadedPlacement.rotationQuarterTurns;
        placements.push_back(placement);
    }

    editor.m_gridSize = loaded.gridSize;
    editor.m_gridHalfExtent = loaded.gridHalfExtent;
    editor.m_gridHeightStep = loaded.gridHeightStep;
    editor.m_activeLayer = (loaded.activeLayer < 0) ? 0 : loaded.activeLayer;
    editor.m_cameraEye = loaded.cameraEye;
    editor.m_cameraLook = loaded.cameraLook;
    editor.SyncCameraAnglesFromPose();
    editor.m_previewRotationQuarterTurns = ((loaded.previewRotationQuarterTurns % 4) + 4) % 4;
    editor.m_buildMap.SetPlacements(placements);
    editor.m_selection.ClearSelection();
    editor.m_hasPreview = false;
    editor.m_canPlacePreview = false;
    editor.m_undoStack.clear();
    editor.m_redoStack.clear();
    editor.ClearPlacementToolState();
    return true;
}

const char* CastleSaveData::GetDefaultPath()
{
    return kDefaultCastleSavePath;
}
