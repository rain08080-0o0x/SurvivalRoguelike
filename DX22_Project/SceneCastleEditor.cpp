#include "SceneCastleEditor.h"

#include "CastleSaveData.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Model.h"
#include "ShaderList.h"
#include "Sprite.h"
#include "Texture.h"

#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#undef min
#undef max

namespace
{
    constexpr float kPi = 3.1415926535f;
    constexpr float kHalfPi = kPi * 0.5f;
    constexpr char kAssetCatalogPath[] = "Assets/castle_editor_models.cfg";
    constexpr char kAssetCatalogFormat[] = "CastleEditorAssetCatalog";
    constexpr int kAssetCatalogVersion = 1;
    constexpr int kWireEdges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };

    void TrimString(std::string& text)
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

    bool TryParseInt(const std::unordered_map<std::string, std::string>& values, const char* key, int& outValue)
    {
        const auto it = values.find(key);
        if (it == values.end())
        {
            return false;
        }

        char* end = nullptr;
        const long parsed = std::strtol(it->second.c_str(), &end, 10);
        if (!end || *end != '\0')
        {
            return false;
        }

        outValue = static_cast<int>(parsed);
        return true;
    }

    bool TryParseFloat(const std::unordered_map<std::string, std::string>& values, const char* key, float& outValue)
    {
        const auto it = values.find(key);
        if (it == values.end())
        {
            return false;
        }

        char* end = nullptr;
        const float parsed = std::strtof(it->second.c_str(), &end);
        if (!end || *end != '\0')
        {
            return false;
        }

        outValue = parsed;
        return true;
    }

    std::vector<SceneCastleEditor::AssetInfo> BuildDefaultAssetCatalog()
    {
        return {
            { "brick", "Brick", "Assets/Model/Castle/Brick.fbx", 1.0f, 0.0f },
            { "wall", "Wall", "Assets/Model/Castle/Wall.fbx", 1.0f, 0.0f },
        };
    }

    std::string ExtractFileStem(const std::string& path)
    {
        if (path.empty())
        {
            return {};
        }

        const size_t slash = path.find_last_of("\\/");
        const size_t begin = (slash == std::string::npos) ? 0 : (slash + 1);
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos || dot < begin)
        {
            return path.substr(begin);
        }

        return path.substr(begin, dot - begin);
    }

    std::string NormalizePathSlashes(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    std::string LowercaseAscii(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return text;
    }

    std::string NormalizeCatalogPath(const std::string& inputPath)
    {
        if (inputPath.empty())
        {
            return {};
        }

        char fullPath[MAX_PATH] = {};
        const DWORD fullLen = GetFullPathNameA(inputPath.c_str(), MAX_PATH, fullPath, nullptr);
        if (fullLen == 0 || fullLen >= MAX_PATH)
        {
            return NormalizePathSlashes(inputPath);
        }

        char currentDirectory[MAX_PATH] = {};
        const DWORD currentDirectoryLen = GetCurrentDirectoryA(MAX_PATH, currentDirectory);

        const std::string normalizedFullPath = NormalizePathSlashes(fullPath);
        if (currentDirectoryLen == 0 || currentDirectoryLen >= MAX_PATH)
        {
            return normalizedFullPath;
        }

        std::string normalizedCurrentDirectory = NormalizePathSlashes(currentDirectory);
        if (!normalizedCurrentDirectory.empty() && normalizedCurrentDirectory.back() != '/')
        {
            normalizedCurrentDirectory.push_back('/');
        }

        const std::string compareFullPath = LowercaseAscii(normalizedFullPath);
        const std::string compareCurrentDirectory = LowercaseAscii(normalizedCurrentDirectory);
        if (compareFullPath.rfind(compareCurrentDirectory, 0) == 0)
        {
            return normalizedFullPath.substr(normalizedCurrentDirectory.size());
        }

        return normalizedFullPath;
    }

    std::string NormalizeAssetIdToken(const std::string& source)
    {
        std::string result;
        result.reserve(source.size());
        for (unsigned char ch : source)
        {
            if (std::isalnum(ch))
            {
                result.push_back(static_cast<char>(std::tolower(ch)));
            }
            else if (ch == '_' || ch == '-' || ch == ' ')
            {
                if (result.empty() || result.back() == '_') continue;
                result.push_back('_');
            }
        }

        while (!result.empty() && result.back() == '_')
        {
            result.pop_back();
        }
        return result;
    }

    std::string MakeUniqueAssetId(
        const std::string& displayName,
        const std::string& path,
        const std::unordered_set<std::string>& usedIds)
    {
        std::string base = NormalizeAssetIdToken(!displayName.empty() ? displayName : ExtractFileStem(path));
        if (base.empty())
        {
            base = "asset";
        }

        std::string candidate = base;
        int suffix = 2;
        while (usedIds.find(candidate) != usedIds.end())
        {
            candidate = base + "_" + std::to_string(suffix++);
        }
        return candidate;
    }

    bool LoadAssetCatalogFromFile(const char* path, std::vector<SceneCastleEditor::AssetInfo>& outInfos, std::string& outError)
    {
        std::ifstream ifs(path);
        if (!ifs.is_open())
        {
            outError = "Failed to open asset catalog: ";
            outError += path;
            return false;
        }

        std::unordered_map<std::string, std::string> values;
        std::string line;
        while (std::getline(ifs, line))
        {
            TrimString(line);
            if (line.empty() || line[0] == '#') continue;

            const size_t separator = line.find('=');
            if (separator == std::string::npos) continue;

            std::string key = line.substr(0, separator);
            std::string value = line.substr(separator + 1);
            TrimString(key);
            TrimString(value);
            if (key.empty()) continue;
            values[key] = value;
        }

        const auto formatIt = values.find("format");
        if (formatIt == values.end() || formatIt->second != kAssetCatalogFormat)
        {
            outError = "Asset catalog format is invalid.";
            return false;
        }

        int version = 0;
        if (!TryParseInt(values, "version", version) || version != kAssetCatalogVersion)
        {
            outError = "Asset catalog version is invalid.";
            return false;
        }

        int assetCount = 0;
        if (!TryParseInt(values, "assetCount", assetCount) || assetCount < 0)
        {
            outError = "Asset catalog assetCount is invalid.";
            return false;
        }

        std::vector<SceneCastleEditor::AssetInfo> loaded;
        loaded.reserve(static_cast<size_t>(assetCount));
        std::unordered_set<std::string> usedIds;
        for (int i = 0; i < assetCount; ++i)
        {
            char key[64];
            SceneCastleEditor::AssetInfo info;

            sprintf_s(key, "asset.%d.id", i);
            const auto idIt = values.find(key);
            if (idIt == values.end())
            {
                outError = "Asset catalog is missing asset id.";
                return false;
            }
            info.assetId = NormalizeAssetIdToken(idIt->second);
            if (info.assetId.empty())
            {
                outError = "Asset catalog contains invalid asset id.";
                return false;
            }
            if (!usedIds.insert(info.assetId).second)
            {
                outError = "Asset catalog contains duplicate asset id: " + info.assetId;
                return false;
            }

            sprintf_s(key, "asset.%d.name", i);
            const auto nameIt = values.find(key);
            if (nameIt == values.end())
            {
                outError = "Asset catalog is missing asset name.";
                return false;
            }
            info.name = nameIt->second;
            TrimString(info.name);

            sprintf_s(key, "asset.%d.path", i);
            const auto pathIt = values.find(key);
            if (pathIt == values.end())
            {
                outError = "Asset catalog is missing asset path.";
                return false;
            }
            info.path = pathIt->second;
            TrimString(info.path);
            info.path = NormalizePathSlashes(info.path);
            if (info.path.empty())
            {
                outError = "Asset catalog contains empty asset path.";
                return false;
            }
            if (info.name.empty())
            {
                info.name = ExtractFileStem(info.path);
            }

            sprintf_s(key, "asset.%d.scale", i);
            TryParseFloat(values, key, info.scale);
            if (info.scale <= 0.0f)
            {
                outError = "Asset catalog contains invalid scale.";
                return false;
            }

            sprintf_s(key, "asset.%d.yOffset", i);
            TryParseFloat(values, key, info.yOffset);

            loaded.push_back(info);
        }

        outInfos = loaded;
        return true;
    }

    bool SaveAssetCatalogToFile(const char* path, const std::vector<SceneCastleEditor::AssetInfo>& infos, std::string& outError)
    {
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs.is_open())
        {
            outError = "Failed to write asset catalog: ";
            outError += path;
            return false;
        }

        ofs << "# DX22 castle editor asset catalog\n";
        ofs << "format=" << kAssetCatalogFormat << "\n";
        ofs << "version=" << kAssetCatalogVersion << "\n";
        ofs << "assetCount=" << infos.size() << "\n";
        for (size_t i = 0; i < infos.size(); ++i)
        {
            const SceneCastleEditor::AssetInfo& info = infos[i];
            ofs << "asset." << i << ".id=" << info.assetId << "\n";
            ofs << "asset." << i << ".name=" << info.name << "\n";
            ofs << "asset." << i << ".path=" << info.path << "\n";
            ofs << "asset." << i << ".scale=" << info.scale << "\n";
            ofs << "asset." << i << ".yOffset=" << info.yOffset << "\n";
        }

        if (!ofs.good())
        {
            outError = "Failed while saving asset catalog.";
            return false;
        }

        return true;
    }

    struct RayHitResult
    {
        bool hit = false;
        float distance = FLT_MAX;
        int gridX = 0;
        int gridY = 0;
        int gridZ = 0;
    };

    DirectX::XMMATRIX MakePlacementWorldMatrix(
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& rotate,
        const DirectX::XMFLOAT3& scale,
        const DirectX::XMFLOAT3& anchor)
    {
        using namespace DirectX;

        const XMMATRIX localOffset = XMMatrixTranslation(-anchor.x, -anchor.y, -anchor.z);
        const XMMATRIX scaling = XMMatrixScaling(scale.x, scale.y, scale.z);
        const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(rotate.x, rotate.y, rotate.z);
        const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);
        return localOffset * scaling * rotation * translation;
    }

    DirectX::XMFLOAT4X4 MakeMatrixTranspose(
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& rotate,
        const DirectX::XMFLOAT3& scale,
        const DirectX::XMFLOAT3& anchor = { 0.0f, 0.0f, 0.0f })
    {
        using namespace DirectX;

        XMFLOAT4X4 result;
        XMStoreFloat4x4(&result, XMMatrixTranspose(MakePlacementWorldMatrix(pos, rotate, scale, anchor)));
        return result;
    }

    float ClampFloat(float value, float minValue, float maxValue)
    {
        if (value < minValue) return minValue;
        if (value > maxValue) return maxValue;
        return value;
    }

    DirectX::XMFLOAT3 TransformPoint(
        const DirectX::XMFLOAT3& point,
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& rotate,
        const DirectX::XMFLOAT3& scale,
        const DirectX::XMFLOAT3& anchor)
    {
        using namespace DirectX;

        const XMMATRIX world = MakePlacementWorldMatrix(pos, rotate, scale, anchor);

        XMFLOAT3 result;
        XMStoreFloat3(&result, XMVector3TransformCoord(XMVectorSet(point.x, point.y, point.z, 1.0f), world));
        return result;
    }

    bool IntersectLocalAabb(
        const DirectX::XMFLOAT3& minValue,
        const DirectX::XMFLOAT3& maxValue,
        const DirectX::XMVECTOR& rayOrigin,
        const DirectX::XMVECTOR& rayDir,
        float& outDistance,
        DirectX::XMFLOAT3& outHitPoint)
    {
        float tMin = 0.0f;
        float tMax = FLT_MAX;

        const float origin[3] = {
            DirectX::XMVectorGetX(rayOrigin),
            DirectX::XMVectorGetY(rayOrigin),
            DirectX::XMVectorGetZ(rayOrigin)
        };
        const float dir[3] = {
            DirectX::XMVectorGetX(rayDir),
            DirectX::XMVectorGetY(rayDir),
            DirectX::XMVectorGetZ(rayDir)
        };
        const float boundsMin[3] = { minValue.x, minValue.y, minValue.z };
        const float boundsMax[3] = { maxValue.x, maxValue.y, maxValue.z };

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(dir[axis]) < 1.0e-6f)
            {
                if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis])
                {
                    return false;
                }
                continue;
            }

            float invDir = 1.0f / dir[axis];
            float t1 = (boundsMin[axis] - origin[axis]) * invDir;
            float t2 = (boundsMax[axis] - origin[axis]) * invDir;
            if (t1 > t2)
            {
                float temp = t1;
                t1 = t2;
                t2 = temp;
            }

            tMin = (t1 > tMin) ? t1 : tMin;
            tMax = (t2 < tMax) ? t2 : tMax;
            if (tMin > tMax)
            {
                return false;
            }
        }

        outDistance = tMin;
        outHitPoint = {
            origin[0] + dir[0] * tMin,
            origin[1] + dir[1] * tMin,
            origin[2] + dir[2] * tMin
        };
        return true;
    }

    DirectX::XMFLOAT3 DetermineLocalFaceNormal(
        const DirectX::XMFLOAT3& hitPoint,
        const DirectX::XMFLOAT3& minValue,
        const DirectX::XMFLOAT3& maxValue)
    {
        const float distances[6] = {
            std::fabs(hitPoint.x - minValue.x),
            std::fabs(hitPoint.x - maxValue.x),
            std::fabs(hitPoint.y - minValue.y),
            std::fabs(hitPoint.y - maxValue.y),
            std::fabs(hitPoint.z - minValue.z),
            std::fabs(hitPoint.z - maxValue.z),
        };

        int faceIndex = 0;
        float bestDistance = distances[0];
        for (int i = 1; i < 6; ++i)
        {
            if (distances[i] < bestDistance)
            {
                bestDistance = distances[i];
                faceIndex = i;
            }
        }

        switch (faceIndex)
        {
        case 0: return { -1.0f, 0.0f, 0.0f };
        case 1: return { 1.0f, 0.0f, 0.0f };
        case 2: return { 0.0f, -1.0f, 0.0f };
        case 3: return { 0.0f, 1.0f, 0.0f };
        case 4: return { 0.0f, 0.0f, -1.0f };
        default: return { 0.0f, 0.0f, 1.0f };
        }
    }

    DirectX::XMINT3 QuantizeNormalToGridOffset(const DirectX::XMFLOAT3& normal)
    {
        const float absX = std::fabs(normal.x);
        const float absY = std::fabs(normal.y);
        const float absZ = std::fabs(normal.z);

        if (absY >= absX && absY >= absZ)
        {
            return { 0, (normal.y >= 0.0f) ? 1 : -1, 0 };
        }
        if (absX >= absZ)
        {
            return { (normal.x >= 0.0f) ? 1 : -1, 0, 0 };
        }
        return { 0, 0, (normal.z >= 0.0f) ? 1 : -1 };
    }
}

SceneCastleEditor::SceneCastleEditor()
    : m_assetCatalogStatusMessage()
    , m_assetCatalogStatusIsError(false)
    , m_selectedAssetIndex(-1)
    , m_toolMode(ToolMode::PlaceSingle)
    , m_activeLayer(0)
    , m_previewGridX(0)
    , m_previewGridY(0)
    , m_previewGridZ(0)
    , m_previewRotationQuarterTurns(0)
    , m_hasPreview(false)
    , m_canPlacePreview(false)
    , m_gridSize(1.0f)
    , m_gridHalfExtent(16)
    , m_gridHeightStep(1.0f)
    , m_cameraEye(8.0f, 10.0f, -8.0f)
    , m_cameraLook(0.0f, 0.0f, 0.0f)
    , m_cameraUp(0.0f, 1.0f, 0.0f)
    , m_cameraFovy(DirectX::XMConvertToRadians(60.0f))
    , m_cameraNear(0.03f)
    , m_cameraFar(1000.0f)
    , m_cameraAspect(16.0f / 9.0f)
    , m_cameraYaw(0.0f)
    , m_cameraPitch(0.0f)
    , m_cameraDistance(1.0f)
    , m_modelViewRT(nullptr)
    , m_modelViewDS(nullptr)
    , m_modelViewSize(0)
    , m_modelViewYaw(DirectX::XMConvertToRadians(-25.0f))
    , m_modelViewPitch(DirectX::XMConvertToRadians(15.0f))
    , m_modelViewDistanceScale(1.0f)
    , m_paintStrokeActive(false)
    , m_paintStrokeStartIndex(-1)
    , m_paintStrokePreviousSelectedIndex(-1)
    , m_paintStrokePreviousSelectedIndices()
    , m_paintStrokePlacements()
    , m_fillStartActive(false)
    , m_fillStartGridX(0)
    , m_fillStartGridY(0)
    , m_fillStartGridZ(0)
    , m_fillHeightOffset(0)
    , m_selectFillStartActive(false)
    , m_selectFillStartGridX(0)
    , m_selectFillStartGridY(0)
    , m_selectFillStartGridZ(0)
    , m_selectFillHasPreview(false)
    , m_selectFillPreviewGridX(0)
    , m_selectFillPreviewGridY(0)
    , m_selectFillPreviewGridZ(0)
    , m_shiftVerticalFillMode(false)
{
    LoadAssets();
    ResetCamera();
}

SceneCastleEditor::~SceneCastleEditor()
{
    ReleaseAssets();
}

void SceneCastleEditor::Update()
{
}

void SceneCastleEditor::Draw()
{
    using namespace DirectX;

    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(m_cameraEye.x, m_cameraEye.y, m_cameraEye.z, 1.0f),
        XMVectorSet(m_cameraLook.x, m_cameraLook.y, m_cameraLook.z, 1.0f),
        XMVectorSet(m_cameraUp.x, m_cameraUp.y, m_cameraUp.z, 0.0f));
    const XMMATRIX proj = XMMatrixPerspectiveFovLH(m_cameraFovy, m_cameraAspect, m_cameraNear, m_cameraFar);

    XMFLOAT4X4 viewT;
    XMFLOAT4X4 projT;
    XMStoreFloat4x4(&viewT, XMMatrixTranspose(view));
    XMStoreFloat4x4(&projT, XMMatrixTranspose(proj));

    Geometory::SetView(viewT);
    Geometory::SetProjection(projT);
    Sprite::SetView(viewT);
    Sprite::SetProjection(projT);

    DrawGrid();

    const std::vector<PlacementInfo>& placements = m_buildMap.GetPlacements();
    for (size_t i = 0; i < placements.size(); ++i)
    {
        DrawPlacement(placements[i], false, m_selection.IsSelected(static_cast<int>(i)));
    }

    if (m_toolMode == ToolMode::SelectFill && m_selectFillStartActive)
    {
        const DirectX::XMFLOAT4 startColor(1.00f, 0.85f, 0.25f, 1.0f);
        AddGridCellFrame(m_selectFillStartGridX, m_selectFillStartGridY, m_selectFillStartGridZ, startColor, 0.08f);

        if (m_selectFillHasPreview)
        {
            ForEachBoxCell(
                m_selectFillStartGridX,
                m_selectFillStartGridY,
                m_selectFillStartGridZ,
                m_selectFillPreviewGridX,
                m_selectFillPreviewGridY,
                m_selectFillPreviewGridZ,
                [&](int gridX, int gridY, int gridZ)
                {
                    AddGridCellFrame(gridX, gridY, gridZ, startColor, 0.08f);
                });
        }

        Geometory::DrawLines();
    }

    if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(m_assets.size()))
    {
        if (m_toolMode == ToolMode::PlaceFill && m_fillStartActive && m_hasPreview)
        {
            const FillPlane plane = DetermineFillPlane(
                m_fillStartGridX,
                m_fillStartGridY,
                m_fillStartGridZ,
                m_previewGridX,
                m_previewGridY,
                m_previewGridZ);
            if (IsFillPlaneAllowed(plane, m_shiftVerticalFillMode))
            {
                const bool originalCanPlacePreview = m_canPlacePreview;
                ForEachFillCell(
                    plane,
                    m_fillStartGridX,
                    m_fillStartGridY,
                    m_fillStartGridZ,
                    m_previewGridX,
                    m_previewGridY,
                    m_previewGridZ,
                    [&](int gridX, int gridY, int gridZ)
                    {
                        PlacementInfo preview;
                        preview.assetIndex = m_selectedAssetIndex;
                        preview.gridX = gridX;
                        preview.gridY = gridY;
                        preview.gridZ = gridZ;
                        preview.rotationQuarterTurns = m_previewRotationQuarterTurns;
                        m_canPlacePreview =
                            IsInsideGrid(gridX, gridY, gridZ) &&
                            !m_buildMap.HasPlacementAt(gridX, gridY, gridZ);
                        DrawPlacement(preview, true, false);
                    });
                m_canPlacePreview = originalCanPlacePreview;
            }
        }
        else if (m_hasPreview)
        {
            PlacementInfo preview;
            preview.assetIndex = m_selectedAssetIndex;
            preview.gridX = m_previewGridX;
            preview.gridY = m_previewGridY;
            preview.gridZ = m_previewGridZ;
            preview.rotationQuarterTurns = m_previewRotationQuarterTurns;
            DrawPlacement(preview, true, false);
        }
    }
}

int SceneCastleEditor::GetAssetCount() const
{
    return static_cast<int>(m_assets.size());
}

const SceneCastleEditor::AssetInfo* SceneCastleEditor::GetAssetInfo(int index) const
{
    if (index < 0 || index >= static_cast<int>(m_assets.size())) return nullptr;
    return &m_assets[index].info;
}

int SceneCastleEditor::GetSelectedAssetIndex() const
{
    return m_selectedAssetIndex;
}

const char* SceneCastleEditor::GetAssetCatalogPath() const
{
    return kAssetCatalogPath;
}

const char* SceneCastleEditor::GetAssetCatalogStatusMessage() const
{
    return m_assetCatalogStatusMessage.c_str();
}

bool SceneCastleEditor::IsAssetCatalogStatusError() const
{
    return m_assetCatalogStatusIsError;
}

void SceneCastleEditor::SetAssetCatalogStatus(const std::string& message, bool isError)
{
    m_assetCatalogStatusMessage = message;
    m_assetCatalogStatusIsError = isError;
}

int SceneCastleEditor::FindAssetIndexById(const std::string& assetId) const
{
    if (assetId.empty())
    {
        return -1;
    }

    for (int i = 0; i < static_cast<int>(m_assets.size()); ++i)
    {
        if (m_assets[static_cast<size_t>(i)].info.assetId == assetId)
        {
            return i;
        }
    }

    return -1;
}

bool SceneCastleEditor::LoadAssetCatalogInfos(std::vector<AssetInfo>& outAssetInfos, std::string& outError) const
{
    std::ifstream ifs(kAssetCatalogPath);
    if (!ifs.is_open())
    {
        outAssetInfos = BuildDefaultAssetCatalog();
        return SaveAssetCatalogToFile(kAssetCatalogPath, outAssetInfos, outError);
    }

    return LoadAssetCatalogFromFile(kAssetCatalogPath, outAssetInfos, outError);
}

bool SceneCastleEditor::SaveAssetCatalogInfos(const std::vector<AssetInfo>& assetInfos, std::string& outError) const
{
    return SaveAssetCatalogToFile(kAssetCatalogPath, assetInfos, outError);
}

bool SceneCastleEditor::IsAssetReferencedBySessionId(const std::string& assetId) const
{
    if (assetId.empty())
    {
        return false;
    }

    const auto placementUsesAsset = [&](const PlacementInfo& placement)
    {
        if (placement.assetIndex < 0 || placement.assetIndex >= static_cast<int>(m_assets.size()))
        {
            return false;
        }
        return m_assets[static_cast<size_t>(placement.assetIndex)].info.assetId == assetId;
    };

    for (const PlacementInfo& placement : m_buildMap.GetPlacements())
    {
        if (placementUsesAsset(placement))
        {
            return true;
        }
    }

    const auto historyUsesAsset = [&](const std::vector<HistoryEntry>& history)
    {
        for (const HistoryEntry& entry : history)
        {
            switch (entry.type)
            {
            case HistoryEntryType::AddPlacement:
                if (placementUsesAsset(entry.afterPlacement)) return true;
                break;
            case HistoryEntryType::RemovePlacement:
                if (placementUsesAsset(entry.beforePlacement)) return true;
                break;
            case HistoryEntryType::UpdatePlacement:
                if (placementUsesAsset(entry.beforePlacement) || placementUsesAsset(entry.afterPlacement)) return true;
                break;
            case HistoryEntryType::AddPlacementBatch:
            case HistoryEntryType::RemovePlacementBatch:
                for (const PlacementInfo& placement : entry.placements)
                {
                    if (placementUsesAsset(placement)) return true;
                }
                break;
            }
        }

        return false;
    };

    return historyUsesAsset(m_undoStack) || historyUsesAsset(m_redoStack);
}

bool SceneCastleEditor::CanRemoveAsset(int index) const
{
    const AssetInfo* asset = GetAssetInfo(index);
    if (!asset)
    {
        return false;
    }

    return !IsAssetReferencedBySessionId(asset->assetId);
}

bool SceneCastleEditor::AddAssetFromPath(const char* displayName, const char* path)
{
    std::string name = displayName ? displayName : "";
    std::string assetPath = path ? path : "";
    TrimString(name);
    TrimString(assetPath);
    assetPath = NormalizeCatalogPath(assetPath);

    if (assetPath.empty())
    {
        SetAssetCatalogStatus(u8"モデルパスを入力してください。", true);
        return false;
    }

    if (name.empty())
    {
        name = ExtractFileStem(assetPath);
    }
    if (name.empty())
    {
        name = u8"New Model";
    }

    std::vector<AssetInfo> assetInfos;
    std::string error;
    if (!LoadAssetCatalogInfos(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    for (const AssetInfo& existing : assetInfos)
    {
        if (existing.path == assetPath)
        {
            SetAssetCatalogStatus(u8"同じパスのモデルは既に登録されています。", true);
            return false;
        }
    }

    std::unordered_set<std::string> usedIds;
    for (const AssetInfo& info : assetInfos)
    {
        usedIds.insert(info.assetId);
    }

    AssetInfo newAsset;
    newAsset.assetId = MakeUniqueAssetId(name, assetPath, usedIds);
    newAsset.name = name;
    newAsset.path = assetPath;
    assetInfos.push_back(newAsset);

    if (!ApplyAssetCatalog(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    if (!SaveAssetCatalogInfos(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    const int newAssetIndex = FindAssetIndexById(newAsset.assetId);
    if (newAssetIndex >= 0)
    {
        SetSelectedAssetIndex(newAssetIndex);
    }
    SetAssetCatalogStatus(u8"モデルを追加しました。", false);
    return true;
}

bool SceneCastleEditor::RemoveAsset(int index)
{
    const AssetInfo* asset = GetAssetInfo(index);
    if (!asset)
    {
        SetAssetCatalogStatus(u8"削除対象のモデルが選ばれていません。", true);
        return false;
    }

    if (!CanRemoveAsset(index))
    {
        SetAssetCatalogStatus(u8"配置中または履歴で参照中のモデルは削除できません。", true);
        return false;
    }

    std::vector<AssetInfo> assetInfos;
    std::string error;
    if (!LoadAssetCatalogInfos(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    assetInfos.erase(
        std::remove_if(
            assetInfos.begin(),
            assetInfos.end(),
            [&](const AssetInfo& info) { return info.assetId == asset->assetId; }),
        assetInfos.end());

    if (!ApplyAssetCatalog(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    if (!SaveAssetCatalogInfos(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    SetAssetCatalogStatus(u8"モデルを削除しました。", false);
    return true;
}

bool SceneCastleEditor::ReloadAssetCatalog()
{
    std::vector<AssetInfo> assetInfos;
    std::string error;
    if (!LoadAssetCatalogInfos(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    if (!ApplyAssetCatalog(assetInfos, error))
    {
        SetAssetCatalogStatus(error, true);
        return false;
    }

    SetAssetCatalogStatus(u8"モデル一覧を再読込しました。", false);
    return true;
}

void SceneCastleEditor::SetSelectedAssetIndex(int index)
{
    if (index < 0 || index >= static_cast<int>(m_assets.size())) return;
    m_selectedAssetIndex = index;
    if (m_toolMode == ToolMode::SelectSingle || m_toolMode == ToolMode::SelectFill)
    {
        m_toolMode = ToolMode::PlaceSingle;
    }
    ClearPlacementToolState();
}

SceneCastleEditor::ToolMode SceneCastleEditor::GetToolMode() const
{
    return m_toolMode;
}

void SceneCastleEditor::SetToolMode(ToolMode mode)
{
    m_toolMode = mode;
    ClearPlacementToolState();
}

void* SceneCastleEditor::GetAssetThumbnailTextureId(int index, unsigned int size)
{
    if (index < 0 || index >= static_cast<int>(m_assets.size())) return nullptr;
    AssetState& asset = m_assets[index];
    if (!asset.model) return nullptr;

    RenderAssetThumbnail(asset, size);
    return asset.thumbnailRT ? reinterpret_cast<void*>(asset.thumbnailRT->GetResource()) : nullptr;
}

void* SceneCastleEditor::GetModelViewTextureId(unsigned int size)
{
    RenderModelView(size);
    return m_modelViewRT ? reinterpret_cast<void*>(m_modelViewRT->GetResource()) : nullptr;
}

void SceneCastleEditor::HandleModelViewInput(bool hovered, bool rightDragging, float mouseDeltaX, float mouseDeltaY, float mouseWheel)
{
    if (!hovered) return;

    if (rightDragging)
    {
        m_modelViewYaw -= mouseDeltaX * 0.01f;
        m_modelViewPitch = ClampFloat(m_modelViewPitch + mouseDeltaY * 0.01f, -1.2f, 1.2f);
    }
    if (std::fabs(mouseWheel) > 0.001f)
    {
        m_modelViewDistanceScale = ClampFloat(m_modelViewDistanceScale - mouseWheel * 0.1f, 0.45f, 3.0f);
    }
}

void SceneCastleEditor::ResetModelViewCamera()
{
    m_modelViewYaw = DirectX::XMConvertToRadians(-25.0f);
    m_modelViewPitch = DirectX::XMConvertToRadians(15.0f);
    m_modelViewDistanceScale = 1.0f;
}

int SceneCastleEditor::GetPlacementCount() const
{
    return m_buildMap.GetPlacementCount();
}

const SceneCastleEditor::PlacementInfo* SceneCastleEditor::GetPlacement(int index) const
{
    return m_buildMap.GetPlacement(index);
}

int SceneCastleEditor::GetSelectedPlacementIndex() const
{
    return m_selection.GetSelectedPlacementIndex();
}

int SceneCastleEditor::GetSelectedPlacementCount() const
{
    return m_selection.GetSelectedPlacementCount();
}

int SceneCastleEditor::GetSelectedPlacementIndexAt(int order) const
{
    return m_selection.GetSelectedPlacementIndexAt(order);
}

bool SceneCastleEditor::IsPlacementSelected(int index) const
{
    return m_selection.IsSelected(index);
}

void SceneCastleEditor::ClearSelection()
{
    m_selection.ClearSelection();
}

void SceneCastleEditor::SetSelectedPlacementIndex(int index)
{
    m_selection.SetSelectedPlacementIndex(index, m_buildMap.GetPlacementCount());
}

void SceneCastleEditor::AddSelectedPlacementIndex(int index)
{
    m_selection.AddSelectedPlacementIndex(index, m_buildMap.GetPlacementCount());
}

void SceneCastleEditor::ToggleSelectedPlacementIndex(int index)
{
    m_selection.ToggleSelectedPlacementIndex(index, m_buildMap.GetPlacementCount());
}

void SceneCastleEditor::DeleteSelectedPlacement()
{
    const std::vector<int>& selectedIndices = m_selection.GetSelectedPlacementIndices();
    if (selectedIndices.empty()) return;

    if (selectedIndices.size() == 1)
    {
        const int selectedIndex = selectedIndices[0];
        const PlacementInfo* selectedPlacement = m_buildMap.GetPlacement(selectedIndex);
        if (!selectedPlacement) return;

        HistoryEntry entry;
        entry.type = HistoryEntryType::RemovePlacement;
        entry.index = selectedIndex;
        entry.beforePlacement = *selectedPlacement;
        entry.beforeSelectedPlacementIndex = selectedIndex;
        entry.beforeSelectedPlacementIndices = selectedIndices;
        entry.afterSelectedPlacementIndex = -1;

        RemovePlacementInternal(selectedIndex);
        m_selection.ClearSelection();
        PushHistory(entry);
        return;
    }

    HistoryEntry entry;
    entry.type = HistoryEntryType::RemovePlacementBatch;
    entry.indices = selectedIndices;
    entry.beforeSelectedPlacementIndices = selectedIndices;
    entry.beforeSelectedPlacementIndex = m_selection.GetSelectedPlacementIndex();
    entry.afterSelectedPlacementIndex = -1;

    for (int selectedIndex : selectedIndices)
    {
        const PlacementInfo* selectedPlacement = m_buildMap.GetPlacement(selectedIndex);
        if (!selectedPlacement) return;
        entry.placements.push_back(*selectedPlacement);
    }

    for (int order = static_cast<int>(selectedIndices.size()) - 1; order >= 0; --order)
    {
        RemovePlacementInternal(selectedIndices[static_cast<size_t>(order)]);
    }

    m_selection.ClearSelection();
    PushHistory(entry);
}

void SceneCastleEditor::UpdateSelectedPlacement(int gridX, int gridY, int gridZ, int rotationQuarterTurns)
{
    const int selectedIndex = m_selection.GetSelectedPlacementIndex();
    if (selectedIndex < 0 || selectedIndex >= m_buildMap.GetPlacementCount()) return;
    if (!IsInsideGrid(gridX, gridY, gridZ)) return;

    const int currentIndex = m_buildMap.FindPlacementIndexAt(gridX, gridY, gridZ);
    if (currentIndex != -1 && currentIndex != selectedIndex) return;

    const PlacementInfo* currentPlacement = m_buildMap.GetPlacement(selectedIndex);
    if (!currentPlacement) return;

    PlacementInfo updatedPlacement = *currentPlacement;
    updatedPlacement.gridX = gridX;
    updatedPlacement.gridY = gridY;
    updatedPlacement.gridZ = gridZ;
    updatedPlacement.rotationQuarterTurns = ((rotationQuarterTurns % 4) + 4) % 4;
    if (updatedPlacement.assetIndex == currentPlacement->assetIndex &&
        updatedPlacement.gridX == currentPlacement->gridX &&
        updatedPlacement.gridY == currentPlacement->gridY &&
        updatedPlacement.gridZ == currentPlacement->gridZ &&
        updatedPlacement.rotationQuarterTurns == currentPlacement->rotationQuarterTurns)
    {
        return;
    }

    HistoryEntry entry;
    entry.type = HistoryEntryType::UpdatePlacement;
    entry.index = selectedIndex;
    entry.beforePlacement = *currentPlacement;
    entry.afterPlacement = updatedPlacement;
    entry.beforeSelectedPlacementIndices = m_selection.GetSelectedPlacementIndices();
    entry.beforeSelectedPlacementIndex = selectedIndex;
    entry.afterSelectedPlacementIndices = m_selection.GetSelectedPlacementIndices();
    entry.afterSelectedPlacementIndex = selectedIndex;

    UpdatePlacementInternal(selectedIndex, updatedPlacement);
    PushHistory(entry);
}

bool SceneCastleEditor::CanUndo() const
{
    return !m_undoStack.empty();
}

bool SceneCastleEditor::CanRedo() const
{
    return !m_redoStack.empty();
}

void SceneCastleEditor::Undo()
{
    if (m_undoStack.empty()) return;

    const HistoryEntry entry = m_undoStack.back();
    m_undoStack.pop_back();

    switch (entry.type)
    {
    case HistoryEntryType::AddPlacement:
        RemovePlacementInternal(entry.index);
        ApplySelectionState(entry.beforeSelectedPlacementIndices);
        break;
    case HistoryEntryType::AddPlacementBatch:
        for (int i = static_cast<int>(entry.placements.size()) - 1; i >= 0; --i)
        {
            RemovePlacementInternal(entry.index + i);
        }
        ApplySelectionState(entry.beforeSelectedPlacementIndices);
        break;
    case HistoryEntryType::RemovePlacement:
        InsertPlacementInternal(entry.beforePlacement, entry.index);
        ApplySelectionState(entry.beforeSelectedPlacementIndices);
        break;
    case HistoryEntryType::RemovePlacementBatch:
        for (size_t i = 0; i < entry.placements.size(); ++i)
        {
            InsertPlacementInternal(entry.placements[i], entry.indices[static_cast<size_t>(i)]);
        }
        ApplySelectionState(entry.beforeSelectedPlacementIndices);
        break;
    case HistoryEntryType::UpdatePlacement:
        UpdatePlacementInternal(entry.index, entry.beforePlacement);
        ApplySelectionState(entry.beforeSelectedPlacementIndices);
        break;
    }

    m_redoStack.push_back(entry);
}

void SceneCastleEditor::Redo()
{
    if (m_redoStack.empty()) return;

    const HistoryEntry entry = m_redoStack.back();
    m_redoStack.pop_back();

    switch (entry.type)
    {
    case HistoryEntryType::AddPlacement:
        InsertPlacementInternal(entry.afterPlacement, entry.index);
        ApplySelectionState(entry.afterSelectedPlacementIndices);
        break;
    case HistoryEntryType::AddPlacementBatch:
        for (size_t i = 0; i < entry.placements.size(); ++i)
        {
            InsertPlacementInternal(entry.placements[i], entry.index + static_cast<int>(i));
        }
        ApplySelectionState(entry.afterSelectedPlacementIndices);
        break;
    case HistoryEntryType::RemovePlacement:
        RemovePlacementInternal(entry.index);
        ApplySelectionState(entry.afterSelectedPlacementIndices);
        break;
    case HistoryEntryType::RemovePlacementBatch:
        for (int order = static_cast<int>(entry.indices.size()) - 1; order >= 0; --order)
        {
            RemovePlacementInternal(entry.indices[static_cast<size_t>(order)]);
        }
        ApplySelectionState(entry.afterSelectedPlacementIndices);
        break;
    case HistoryEntryType::UpdatePlacement:
        UpdatePlacementInternal(entry.index, entry.afterPlacement);
        ApplySelectionState(entry.afterSelectedPlacementIndices);
        break;
    }

    m_undoStack.push_back(entry);
}

int SceneCastleEditor::GetActiveLayer() const
{
    return m_activeLayer;
}

void SceneCastleEditor::SetActiveLayer(int layer)
{
    if (layer < 0) layer = 0;
    if (layer > 16) layer = 16;
    m_activeLayer = layer;
}

float SceneCastleEditor::GetGridSize() const
{
    return m_gridSize;
}

int SceneCastleEditor::GetGridHalfExtent() const
{
    return m_gridHalfExtent;
}

void SceneCastleEditor::RotatePreview(int deltaQuarterTurns)
{
    m_previewRotationQuarterTurns = ((m_previewRotationQuarterTurns + deltaQuarterTurns) % 4 + 4) % 4;
}

bool SceneCastleEditor::HasPreview() const
{
    return m_hasPreview;
}

bool SceneCastleEditor::CanPlacePreview() const
{
    return m_hasPreview && m_canPlacePreview;
}

int SceneCastleEditor::GetPreviewRotationQuarterTurns() const
{
    return m_previewRotationQuarterTurns;
}

bool SceneCastleEditor::IsFillStartActive() const
{
    return m_fillStartActive;
}

bool SceneCastleEditor::IsSelectFillStartActive() const
{
    return m_selectFillStartActive;
}

DirectX::XMFLOAT3 SceneCastleEditor::GetCameraEye() const
{
    return m_cameraEye;
}

DirectX::XMFLOAT3 SceneCastleEditor::GetCameraLook() const
{
    return m_cameraLook;
}

void SceneCastleEditor::SetCameraEye(const DirectX::XMFLOAT3& eye)
{
    m_cameraEye = eye;
    SyncCameraAnglesFromPose();
}

void SceneCastleEditor::SetCameraLook(const DirectX::XMFLOAT3& look)
{
    m_cameraLook = look;
    SyncCameraAnglesFromPose();
}

void SceneCastleEditor::ResetCamera()
{
    m_cameraLook = { 0.0f, 0.0f, 0.0f };
    m_cameraEye = { 8.0f, 10.0f, -8.0f };
    SyncCameraAnglesFromPose();
}

float SceneCastleEditor::GetGridHeightStep() const
{
    return m_gridHeightStep;
}

bool SceneCastleEditor::SaveCastleData(const char* path) const
{
    return CastleSaveData::Save(*this, path);
}

bool SceneCastleEditor::LoadCastleData(const char* path)
{
    return CastleSaveData::Load(*this, path);
}

bool SceneCastleEditor::ApplyAssetCatalog(const std::vector<AssetInfo>& assetInfos, std::string& outError)
{
    std::vector<AssetInfo> normalizedInfos = assetInfos;
    std::unordered_set<std::string> usedIds;
    for (AssetInfo& info : normalizedInfos)
    {
        TrimString(info.assetId);
        TrimString(info.name);
        TrimString(info.path);
        info.assetId = NormalizeAssetIdToken(info.assetId);
        if (info.assetId.empty())
        {
            outError = "Asset id is empty.";
            return false;
        }
        if (!usedIds.insert(info.assetId).second)
        {
            outError = "Duplicate asset id: " + info.assetId;
            return false;
        }
        if (info.name.empty())
        {
            info.name = ExtractFileStem(info.path);
        }
        if (info.name.empty())
        {
            info.name = info.assetId;
        }
        if (info.path.empty())
        {
            outError = "Asset path is empty.";
            return false;
        }
        if (info.scale <= 0.0f)
        {
            outError = "Asset scale must be positive.";
            return false;
        }
    }

    auto releaseAssetStates = [](std::vector<AssetState>& assets)
    {
        for (AssetState& asset : assets)
        {
            SAFE_DELETE(asset.thumbnailDS);
            SAFE_DELETE(asset.thumbnailRT);
            SAFE_DELETE(asset.model);
        }
        assets.clear();
    };

    std::vector<AssetState> loadedAssets;
    loadedAssets.reserve(normalizedInfos.size());
    for (const AssetInfo& info : normalizedInfos)
    {
        AssetState asset;
        asset.info = info;
        asset.model = new Model();
        if (!asset.model->Load(asset.info.path.c_str(), asset.info.scale, Model::ZFlip))
        {
            outError = "Failed to load model: " + asset.info.path;
            SAFE_DELETE(asset.model);
            releaseAssetStates(loadedAssets);
            return false;
        }

        DirectX::XMFLOAT3 minValue(FLT_MAX, FLT_MAX, FLT_MAX);
        DirectX::XMFLOAT3 maxValue(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        bool hasVertex = false;
        for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
            if (!mesh) continue;
            for (const Model::Vertex& vertex : mesh->vertices)
            {
                minValue.x = (vertex.pos.x < minValue.x) ? vertex.pos.x : minValue.x;
                minValue.y = (vertex.pos.y < minValue.y) ? vertex.pos.y : minValue.y;
                minValue.z = (vertex.pos.z < minValue.z) ? vertex.pos.z : minValue.z;
                maxValue.x = (vertex.pos.x > maxValue.x) ? vertex.pos.x : maxValue.x;
                maxValue.y = (vertex.pos.y > maxValue.y) ? vertex.pos.y : maxValue.y;
                maxValue.z = (vertex.pos.z > maxValue.z) ? vertex.pos.z : maxValue.z;
                hasVertex = true;
            }
        }
        if (hasVertex)
        {
            asset.boundsMin = minValue;
            asset.boundsMax = maxValue;
            asset.placementAnchor = {
                (minValue.x + maxValue.x) * 0.5f,
                minValue.y,
                (minValue.z + maxValue.z) * 0.5f
            };
            asset.hasBounds = true;
        }

        loadedAssets.push_back(std::move(asset));
    }

    std::vector<std::string> oldAssetIds;
    oldAssetIds.reserve(m_assets.size());
    for (const AssetState& asset : m_assets)
    {
        oldAssetIds.push_back(asset.info.assetId);
    }

    std::unordered_map<std::string, int> newAssetIndexById;
    for (int i = 0; i < static_cast<int>(loadedAssets.size()); ++i)
    {
        newAssetIndexById[loadedAssets[static_cast<size_t>(i)].info.assetId] = i;
    }

    const auto remapPlacement = [&](PlacementInfo& placement) -> bool
    {
        if (placement.assetIndex < 0 || placement.assetIndex >= static_cast<int>(oldAssetIds.size()))
        {
            outError = "Placement refers to an invalid asset index.";
            return false;
        }

        const std::string& assetId = oldAssetIds[static_cast<size_t>(placement.assetIndex)];
        const auto it = newAssetIndexById.find(assetId);
        if (it == newAssetIndexById.end())
        {
            outError = "Asset is still referenced by the current scene or history: " + assetId;
            return false;
        }

        placement.assetIndex = it->second;
        return true;
    };

    std::vector<PlacementInfo> remappedPlacements = m_buildMap.GetPlacements();
    for (PlacementInfo& placement : remappedPlacements)
    {
        if (!remapPlacement(placement))
        {
            releaseAssetStates(loadedAssets);
            return false;
        }
    }

    const auto remapHistory = [&](std::vector<HistoryEntry>& history) -> bool
    {
        for (HistoryEntry& entry : history)
        {
            switch (entry.type)
            {
            case HistoryEntryType::AddPlacement:
                if (!remapPlacement(entry.afterPlacement)) return false;
                break;
            case HistoryEntryType::RemovePlacement:
                if (!remapPlacement(entry.beforePlacement)) return false;
                break;
            case HistoryEntryType::UpdatePlacement:
                if (!remapPlacement(entry.beforePlacement)) return false;
                if (!remapPlacement(entry.afterPlacement)) return false;
                break;
            case HistoryEntryType::AddPlacementBatch:
            case HistoryEntryType::RemovePlacementBatch:
                for (PlacementInfo& placement : entry.placements)
                {
                    if (!remapPlacement(placement)) return false;
                }
                break;
            }
        }

        return true;
    };

    std::vector<HistoryEntry> remappedUndo = m_undoStack;
    std::vector<HistoryEntry> remappedRedo = m_redoStack;
    if (!remapHistory(remappedUndo) || !remapHistory(remappedRedo))
    {
        releaseAssetStates(loadedAssets);
        return false;
    }

    const std::string selectedAssetId =
        (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(oldAssetIds.size()))
        ? oldAssetIds[static_cast<size_t>(m_selectedAssetIndex)]
        : std::string{};

    ReleaseAssets();
    m_assets = std::move(loadedAssets);
    m_buildMap.SetPlacements(remappedPlacements);
    m_undoStack = std::move(remappedUndo);
    m_redoStack = std::move(remappedRedo);
    m_selection.ClampSelection(m_buildMap.GetPlacementCount());

    if (!selectedAssetId.empty())
    {
        m_selectedAssetIndex = FindAssetIndexById(selectedAssetId);
    }
    if (m_selectedAssetIndex < 0 && !m_assets.empty())
    {
        m_selectedAssetIndex = 0;
    }
    if (m_assets.empty())
    {
        m_selectedAssetIndex = -1;
        if (m_toolMode != ToolMode::SelectSingle && m_toolMode != ToolMode::SelectFill)
        {
            m_toolMode = ToolMode::SelectSingle;
        }
    }

    m_hasPreview = false;
    m_canPlacePreview = false;
    ClearPlacementToolState();
    return true;
}

void SceneCastleEditor::HandleSceneViewInput(
    float localMouseX,
    float localMouseY,
    float viewWidth,
    float viewHeight,
    bool hovered,
    bool leftClicked,
    bool leftDown,
    bool ctrlDown,
    bool shiftDown,
    bool rightDragging,
    bool middleDragging,
    float mouseDeltaX,
    float mouseDeltaY,
    float mouseWheel)
{
    if (viewWidth > 1.0f && viewHeight > 1.0f)
    {
        m_cameraAspect = viewWidth / viewHeight;
    }

    if (hovered)
    {
        if (rightDragging)
        {
            OrbitCamera(mouseDeltaX, mouseDeltaY);
        }
        if (middleDragging)
        {
            PanCamera(mouseDeltaX, mouseDeltaY);
        }
        if (std::fabs(mouseWheel) > 0.001f)
        {
            if (shiftDown && m_toolMode == ToolMode::PlaceFill && m_fillStartActive)
            {
                m_fillHeightOffset += (mouseWheel > 0.0f) ? 1 : -1;
            }
            else if (shiftDown)
            {
                SetActiveLayer(m_activeLayer + ((mouseWheel > 0.0f) ? 1 : -1));
            }
            else
            {
                ZoomCamera(mouseWheel);
            }
        }
    }

    m_shiftVerticalFillMode = shiftDown;

    DirectX::XMVECTOR rayOrigin;
    DirectX::XMVECTOR rayDir;
    const bool hasRay = hovered && BuildMouseRay(localMouseX, localMouseY, viewWidth, viewHeight, rayOrigin, rayDir);

    if (m_toolMode == ToolMode::SelectSingle)
    {
        if (!leftDown)
        {
            FinishPaintStroke();
        }
        m_hasPreview = false;
        m_canPlacePreview = false;
        if (hasRay && leftClicked)
        {
            const int hitIndex = FindPlacementByRay(rayOrigin, rayDir);
            if (hitIndex >= 0)
            {
                if (ctrlDown)
                {
                    m_selectFillStartActive = false;
                    m_selection.ToggleSelectedPlacementIndex(hitIndex, m_buildMap.GetPlacementCount());
                }
                else if (shiftDown)
                {
                    m_selectFillStartActive = false;
                    m_selection.AddSelectedPlacementIndex(hitIndex, m_buildMap.GetPlacementCount());
                }
                else
                {
                    m_selectFillStartActive = false;
                    m_selection.SetSelectedPlacementIndex(hitIndex, m_buildMap.GetPlacementCount());
                }
            }
            else if (!ctrlDown && !shiftDown)
            {
                m_selectFillStartActive = false;
                m_selection.ClearSelection();
            }
        }
        return;
    }

    if (m_toolMode == ToolMode::SelectFill)
    {
        if (!leftDown)
        {
            FinishPaintStroke();
        }
        m_hasPreview = false;
        m_canPlacePreview = false;
        m_selectFillHasPreview = false;
        int cursorGridX = 0;
        int cursorGridY = 0;
        int cursorGridZ = 0;
        bool hasCursorGrid = false;
        if (shiftDown || !hasRay)
        {
            int gridX = 0;
            int gridZ = 0;
            hasCursorGrid = hovered && RaycastToActiveLayer(localMouseX, localMouseY, viewWidth, viewHeight, gridX, gridZ);
            if (hasCursorGrid)
            {
                cursorGridX = gridX;
                cursorGridY = m_activeLayer;
                cursorGridZ = gridZ;
            }
        }
        else
        {
            const int hitIndex = FindPlacementByRay(rayOrigin, rayDir);
            const PlacementInfo* hitPlacement = m_buildMap.GetPlacement(hitIndex);
            if (hitPlacement)
            {
                hasCursorGrid = true;
                cursorGridX = hitPlacement->gridX;
                cursorGridY = hitPlacement->gridY;
                cursorGridZ = hitPlacement->gridZ;
            }
            else
            {
                int gridX = 0;
                int gridZ = 0;
                hasCursorGrid = hovered && RaycastToActiveLayer(localMouseX, localMouseY, viewWidth, viewHeight, gridX, gridZ);
                if (hasCursorGrid)
                {
                    cursorGridX = gridX;
                    cursorGridY = m_activeLayer;
                    cursorGridZ = gridZ;
                }
            }
        }

        if (hasCursorGrid && leftClicked)
        {
            if (!m_selectFillStartActive)
            {
                m_selectFillStartActive = true;
                m_selectFillStartGridX = cursorGridX;
                m_selectFillStartGridY = cursorGridY;
                m_selectFillStartGridZ = cursorGridZ;
            }
            else
            {
                SelectPlacementsInFillArea(cursorGridX, cursorGridY, cursorGridZ, ctrlDown);
            }
        }
        if (m_selectFillStartActive && hasCursorGrid)
        {
            m_selectFillHasPreview = true;
            m_selectFillPreviewGridX = cursorGridX;
            m_selectFillPreviewGridY = cursorGridY;
            m_selectFillPreviewGridZ = cursorGridZ;
        }
        return;
    }

    int previewX = 0;
    int previewY = 0;
    int previewZ = 0;
    const bool useActiveLayerSnap = shiftDown && m_toolMode != ToolMode::PlaceFill;
    if (!useActiveLayerSnap && hasRay && ComputePlacementPreviewFromRay(rayOrigin, rayDir, previewX, previewY, previewZ))
    {
        m_hasPreview = true;
        m_previewGridX = previewX;
        m_previewGridY = previewY;
        m_previewGridZ = previewZ;
        m_canPlacePreview =
            IsInsideGrid(m_previewGridX, m_previewGridY, m_previewGridZ) &&
            !m_buildMap.HasPlacementAt(m_previewGridX, m_previewGridY, m_previewGridZ);
    }
    else
    {
        int gridX = 0;
        int gridZ = 0;
        const int previewLayer = (m_toolMode == ToolMode::PlaceFill) ? 0 : m_activeLayer;
        m_hasPreview = hovered && RaycastToLayer(localMouseX, localMouseY, viewWidth, viewHeight, previewLayer, gridX, gridZ);
        if (m_hasPreview)
        {
            m_previewGridX = gridX;
            m_previewGridY = previewLayer;
            m_previewGridZ = gridZ;
            m_canPlacePreview =
                IsInsideGrid(m_previewGridX, m_previewGridY, m_previewGridZ) &&
                !m_buildMap.HasPlacementAt(m_previewGridX, m_previewGridY, m_previewGridZ);
        }
    }

    if (m_toolMode == ToolMode::PlaceFill && m_fillStartActive && shiftDown && m_hasPreview)
    {
        m_previewGridY = m_fillStartGridY + m_fillHeightOffset;
        m_canPlacePreview =
            IsInsideGrid(m_previewGridX, m_previewGridY, m_previewGridZ) &&
            !m_buildMap.HasPlacementAt(m_previewGridX, m_previewGridY, m_previewGridZ);
    }

    if (!m_hasPreview)
    {
        m_canPlacePreview = false;
        if (!leftDown)
        {
            FinishPaintStroke();
        }
        return;
    }

    if (m_toolMode == ToolMode::PlacePaint)
    {
        if (!leftDown)
        {
            FinishPaintStroke();
            return;
        }

        if (!hovered || !m_canPlacePreview || m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size())) return;

        PlacementInfo placement;
        placement.assetIndex = m_selectedAssetIndex;
        placement.gridX = m_previewGridX;
        placement.gridY = m_previewGridY;
        placement.gridZ = m_previewGridZ;
        placement.rotationQuarterTurns = m_previewRotationQuarterTurns;

        if (!m_paintStrokeActive)
        {
            m_paintStrokeActive = true;
            m_paintStrokeStartIndex = m_buildMap.GetPlacementCount();
            m_paintStrokePreviousSelectedIndices = m_selection.GetSelectedPlacementIndices();
            m_paintStrokePreviousSelectedIndex = m_selection.GetSelectedPlacementIndex();
            m_paintStrokePlacements.clear();
        }

        const int insertIndex = m_buildMap.GetPlacementCount();
        InsertPlacementInternal(placement, insertIndex);
        m_selection.SetSelectedPlacementIndex(insertIndex, m_buildMap.GetPlacementCount());
        m_paintStrokePlacements.push_back(placement);
        return;
    }

    if (m_toolMode == ToolMode::PlaceFill)
    {
        if (!hovered || !leftClicked || !m_canPlacePreview || m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size()))
        {
            return;
        }

        if (!m_fillStartActive)
        {
            m_fillStartActive = true;
            m_fillStartGridX = m_previewGridX;
            m_fillStartGridY = m_previewGridY;
            m_fillStartGridZ = m_previewGridZ;
            m_fillHeightOffset = 0;
            return;
        }

        const FillPlane plane = DetermineFillPlane(
            m_fillStartGridX,
            m_fillStartGridY,
            m_fillStartGridZ,
            m_previewGridX,
            m_previewGridY,
            m_previewGridZ);
        if (!IsFillPlaneAllowed(plane, m_shiftVerticalFillMode))
        {
            return;
        }

        std::vector<PlacementInfo> placementsToInsert;
        ForEachFillCell(
            plane,
            m_fillStartGridX,
            m_fillStartGridY,
            m_fillStartGridZ,
            m_previewGridX,
            m_previewGridY,
            m_previewGridZ,
            [&](int gridX, int gridY, int gridZ)
            {
                if (!IsInsideGrid(gridX, gridY, gridZ)) return;
                if (m_buildMap.HasPlacementAt(gridX, gridY, gridZ)) return;

                PlacementInfo placement;
                placement.assetIndex = m_selectedAssetIndex;
                placement.gridX = gridX;
                placement.gridY = gridY;
                placement.gridZ = gridZ;
                placement.rotationQuarterTurns = m_previewRotationQuarterTurns;
                placementsToInsert.push_back(placement);
            });

        if (!placementsToInsert.empty())
        {
            const std::vector<int> previousSelectedIndices = m_selection.GetSelectedPlacementIndices();
            const int previousSelectedIndex = m_selection.GetSelectedPlacementIndex();
            const int insertIndex = m_buildMap.GetPlacementCount();
            int lastInsertedIndex = insertIndex;
            for (size_t i = 0; i < placementsToInsert.size(); ++i)
            {
                lastInsertedIndex = InsertPlacementInternal(placementsToInsert[i], m_buildMap.GetPlacementCount());
            }
            m_selection.SetSelectedPlacementIndex(lastInsertedIndex, m_buildMap.GetPlacementCount());

        HistoryEntry entry;
        entry.type = HistoryEntryType::AddPlacementBatch;
        entry.index = insertIndex;
        entry.placements = placementsToInsert;
        entry.beforeSelectedPlacementIndices = previousSelectedIndices;
        entry.beforeSelectedPlacementIndex = previousSelectedIndex;
        entry.afterSelectedPlacementIndices = m_selection.GetSelectedPlacementIndices();
        entry.afterSelectedPlacementIndex = lastInsertedIndex;
        PushHistory(entry);
        }

        m_fillStartActive = false;
        m_fillHeightOffset = 0;
        return;
    }

    if (!hovered || !leftClicked) return;

    if (!m_canPlacePreview || m_selectedAssetIndex < 0 || m_selectedAssetIndex >= static_cast<int>(m_assets.size())) return;

    PlacementInfo placement;
    placement.assetIndex = m_selectedAssetIndex;
    placement.gridX = m_previewGridX;
    placement.gridY = m_previewGridY;
    placement.gridZ = m_previewGridZ;
    placement.rotationQuarterTurns = m_previewRotationQuarterTurns;

    const std::vector<int> previousSelectedIndices = m_selection.GetSelectedPlacementIndices();
    const int previousSelectedIndex = m_selection.GetSelectedPlacementIndex();
    const int insertIndex = m_buildMap.GetPlacementCount();
    InsertPlacementInternal(placement, insertIndex);
    m_selection.SetSelectedPlacementIndex(insertIndex, m_buildMap.GetPlacementCount());

    HistoryEntry entry;
    entry.type = HistoryEntryType::AddPlacement;
    entry.index = insertIndex;
    entry.afterPlacement = placement;
    entry.beforeSelectedPlacementIndices = previousSelectedIndices;
    entry.beforeSelectedPlacementIndex = previousSelectedIndex;
    entry.afterSelectedPlacementIndices = m_selection.GetSelectedPlacementIndices();
    entry.afterSelectedPlacementIndex = insertIndex;
    PushHistory(entry);
}

void SceneCastleEditor::FinishPaintStroke()
{
    if (!m_paintStrokeActive)
    {
        return;
    }

    if (!m_paintStrokePlacements.empty())
    {
        HistoryEntry entry;
        entry.type = HistoryEntryType::AddPlacementBatch;
        entry.index = m_paintStrokeStartIndex;
        entry.placements = m_paintStrokePlacements;
        entry.beforeSelectedPlacementIndices = m_paintStrokePreviousSelectedIndices;
        entry.beforeSelectedPlacementIndex = m_paintStrokePreviousSelectedIndex;
        entry.afterSelectedPlacementIndices = m_selection.GetSelectedPlacementIndices();
        entry.afterSelectedPlacementIndex = m_selection.GetSelectedPlacementIndex();
        PushHistory(entry);
    }

    m_paintStrokeActive = false;
    m_paintStrokeStartIndex = -1;
    m_paintStrokePreviousSelectedIndex = -1;
    m_paintStrokePreviousSelectedIndices.clear();
    m_paintStrokePlacements.clear();
}

void SceneCastleEditor::ClearPlacementToolState()
{
    m_paintStrokeActive = false;
    m_paintStrokeStartIndex = -1;
    m_paintStrokePreviousSelectedIndex = -1;
    m_paintStrokePreviousSelectedIndices.clear();
    m_paintStrokePlacements.clear();
    m_fillStartActive = false;
    m_fillHeightOffset = 0;
    m_selectFillStartActive = false;
    m_selectFillHasPreview = false;
}

void SceneCastleEditor::ApplySelectionState(const std::vector<int>& indices)
{
    if (!indices.empty())
    {
        m_selection.SetSelectedPlacementIndices(indices, m_buildMap.GetPlacementCount());
    }
    else
    {
        m_selection.ClearSelection();
    }
}

void SceneCastleEditor::SelectPlacementsInFillArea(int endGridX, int endGridY, int endGridZ, bool additive)
{
    if (!m_selectFillStartActive)
    {
        return;
    }

    std::vector<int> selectedIndices = additive
        ? m_selection.GetSelectedPlacementIndices()
        : std::vector<int>{};

    ForEachBoxCell(
        m_selectFillStartGridX,
        m_selectFillStartGridY,
        m_selectFillStartGridZ,
        endGridX,
        endGridY,
        endGridZ,
        [&](int gridX, int gridY, int gridZ)
        {
            const int placementIndex = m_buildMap.FindPlacementIndexAt(gridX, gridY, gridZ);
            if (placementIndex >= 0)
            {
                selectedIndices.push_back(placementIndex);
            }
        });

    m_selection.SetSelectedPlacementIndices(selectedIndices, m_buildMap.GetPlacementCount());
    m_selectFillStartActive = false;
    m_selectFillHasPreview = false;
}

bool SceneCastleEditor::IsFillPlaneAllowed(FillPlane plane, bool verticalOnly) const
{
    (void)verticalOnly;
    if (plane == FillPlane::None)
    {
        return false;
    }

    return true;
}

SceneCastleEditor::FillPlane SceneCastleEditor::DetermineFillPlane(
    int startGridX,
    int startGridY,
    int startGridZ,
    int endGridX,
    int endGridY,
    int endGridZ) const
{
    if (startGridY == endGridY) return FillPlane::XZ;
    if (startGridX == endGridX) return FillPlane::YZ;
    if (startGridZ == endGridZ) return FillPlane::XY;
    return FillPlane::None;
}

void SceneCastleEditor::ForEachFillCell(
    FillPlane plane,
    int startGridX,
    int startGridY,
    int startGridZ,
    int endGridX,
    int endGridY,
    int endGridZ,
    const std::function<void(int, int, int)>& visitor) const
{
    if (!visitor) return;

    const int minGridX = (startGridX < endGridX) ? startGridX : endGridX;
    const int maxGridX = (startGridX > endGridX) ? startGridX : endGridX;
    const int minGridY = (startGridY < endGridY) ? startGridY : endGridY;
    const int maxGridY = (startGridY > endGridY) ? startGridY : endGridY;
    const int minGridZ = (startGridZ < endGridZ) ? startGridZ : endGridZ;
    const int maxGridZ = (startGridZ > endGridZ) ? startGridZ : endGridZ;

    switch (plane)
    {
    case FillPlane::XZ:
        for (int gridZ = minGridZ; gridZ <= maxGridZ; ++gridZ)
        {
            for (int gridX = minGridX; gridX <= maxGridX; ++gridX)
            {
                visitor(gridX, startGridY, gridZ);
            }
        }
        break;
    case FillPlane::YZ:
        for (int gridY = minGridY; gridY <= maxGridY; ++gridY)
        {
            for (int gridZ = minGridZ; gridZ <= maxGridZ; ++gridZ)
            {
                visitor(startGridX, gridY, gridZ);
            }
        }
        break;
    case FillPlane::XY:
        for (int gridY = minGridY; gridY <= maxGridY; ++gridY)
        {
            for (int gridX = minGridX; gridX <= maxGridX; ++gridX)
            {
                visitor(gridX, gridY, startGridZ);
            }
        }
        break;
    default:
        break;
    }
}

void SceneCastleEditor::ForEachBoxCell(
    int startGridX,
    int startGridY,
    int startGridZ,
    int endGridX,
    int endGridY,
    int endGridZ,
    const std::function<void(int, int, int)>& visitor) const
{
    if (!visitor) return;

    const int minGridX = (startGridX < endGridX) ? startGridX : endGridX;
    const int maxGridX = (startGridX > endGridX) ? startGridX : endGridX;
    const int minGridY = (startGridY < endGridY) ? startGridY : endGridY;
    const int maxGridY = (startGridY > endGridY) ? startGridY : endGridY;
    const int minGridZ = (startGridZ < endGridZ) ? startGridZ : endGridZ;
    const int maxGridZ = (startGridZ > endGridZ) ? startGridZ : endGridZ;

    for (int gridY = minGridY; gridY <= maxGridY; ++gridY)
    {
        for (int gridZ = minGridZ; gridZ <= maxGridZ; ++gridZ)
        {
            for (int gridX = minGridX; gridX <= maxGridX; ++gridX)
            {
                visitor(gridX, gridY, gridZ);
            }
        }
    }
}

bool SceneCastleEditor::BuildMouseRay(
    float localMouseX,
    float localMouseY,
    float viewWidth,
    float viewHeight,
    DirectX::XMVECTOR& outOrigin,
    DirectX::XMVECTOR& outDirection) const
{
    using namespace DirectX;

    if (viewWidth <= 1.0f || viewHeight <= 1.0f) return false;

    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(m_cameraEye.x, m_cameraEye.y, m_cameraEye.z, 1.0f),
        XMVectorSet(m_cameraLook.x, m_cameraLook.y, m_cameraLook.z, 1.0f),
        XMVectorSet(m_cameraUp.x, m_cameraUp.y, m_cameraUp.z, 0.0f));
    const XMMATRIX proj = XMMatrixPerspectiveFovLH(m_cameraFovy, m_cameraAspect, m_cameraNear, m_cameraFar);
    const XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);
    const float ndcX = (localMouseX / viewWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (localMouseY / viewHeight) * 2.0f;

    outOrigin = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProj);
    const XMVECTOR rayFar = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProj);
    outDirection = XMVector3Normalize(rayFar - outOrigin);
    return true;
}

bool SceneCastleEditor::ComputePlacementPreviewFromRay(
    const DirectX::XMVECTOR& rayOrigin,
    const DirectX::XMVECTOR& rayDirection,
    int& outGridX,
    int& outGridY,
    int& outGridZ) const
{
    using namespace DirectX;

    RayHitResult bestHit;
    for (const PlacementInfo& placement : m_buildMap.GetPlacements())
    {
        if (placement.assetIndex < 0 || placement.assetIndex >= static_cast<int>(m_assets.size())) continue;
        const AssetState& asset = m_assets[placement.assetIndex];
        if (!asset.model) continue;

        const XMFLOAT3 worldPos = GridToWorld(placement.gridX, placement.gridY, placement.gridZ, asset.info.yOffset);
        const XMFLOAT3 worldRot = { 0.0f, static_cast<float>(placement.rotationQuarterTurns) * kHalfPi, 0.0f };
        const XMFLOAT3 worldScale = { asset.info.scale, asset.info.scale, asset.info.scale };
        const XMMATRIX world = MakePlacementWorldMatrix(worldPos, worldRot, worldScale, asset.placementAnchor);
        const XMMATRIX invWorld = XMMatrixInverse(nullptr, world);

        const XMVECTOR localOrigin = XMVector3TransformCoord(rayOrigin, invWorld);
        XMVECTOR localDir = XMVector3TransformNormal(rayDirection, invWorld);
        if (XMVectorGetX(XMVector3LengthSq(localDir)) < 1.0e-8f) continue;
        localDir = XMVector3Normalize(localDir);

        float hitDistance = 0.0f;
        XMFLOAT3 localHitPoint{};
        if (!IntersectLocalAabb(asset.boundsMin, asset.boundsMax, localOrigin, localDir, hitDistance, localHitPoint)) continue;
        if (hitDistance < 0.0f || hitDistance >= bestHit.distance) continue;

        const XMFLOAT3 localNormal = DetermineLocalFaceNormal(localHitPoint, asset.boundsMin, asset.boundsMax);
        XMFLOAT3 worldNormal;
        XMStoreFloat3(&worldNormal, XMVector3Normalize(XMVector3TransformNormal(
            XMVectorSet(localNormal.x, localNormal.y, localNormal.z, 0.0f), world)));
        const XMINT3 offset = QuantizeNormalToGridOffset(worldNormal);

        bestHit.hit = true;
        bestHit.distance = hitDistance;
        bestHit.gridX = placement.gridX + offset.x;
        bestHit.gridY = placement.gridY + offset.y;
        bestHit.gridZ = placement.gridZ + offset.z;
    }

    if (!bestHit.hit) return false;

    outGridX = bestHit.gridX;
    outGridY = bestHit.gridY;
    outGridZ = bestHit.gridZ;
    return true;
}

int SceneCastleEditor::FindPlacementByRay(
    const DirectX::XMVECTOR& rayOrigin,
    const DirectX::XMVECTOR& rayDirection) const
{
    using namespace DirectX;

    float bestDistance = FLT_MAX;
    int bestIndex = -1;
    const std::vector<PlacementInfo>& placements = m_buildMap.GetPlacements();
    for (size_t i = 0; i < placements.size(); ++i)
    {
        const PlacementInfo& placement = placements[i];
        if (placement.assetIndex < 0 || placement.assetIndex >= static_cast<int>(m_assets.size())) continue;
        const AssetState& asset = m_assets[placement.assetIndex];
        if (!asset.model) continue;

        const XMFLOAT3 worldPos = GridToWorld(placement.gridX, placement.gridY, placement.gridZ, asset.info.yOffset);
        const XMFLOAT3 worldRot = { 0.0f, static_cast<float>(placement.rotationQuarterTurns) * kHalfPi, 0.0f };
        const XMFLOAT3 worldScale = { asset.info.scale, asset.info.scale, asset.info.scale };
        const XMMATRIX world = MakePlacementWorldMatrix(worldPos, worldRot, worldScale, asset.placementAnchor);
        const XMMATRIX invWorld = XMMatrixInverse(nullptr, world);

        const XMVECTOR localOrigin = XMVector3TransformCoord(rayOrigin, invWorld);
        XMVECTOR localDir = XMVector3TransformNormal(rayDirection, invWorld);
        if (XMVectorGetX(XMVector3LengthSq(localDir)) < 1.0e-8f) continue;
        localDir = XMVector3Normalize(localDir);

        float hitDistance = 0.0f;
        DirectX::XMFLOAT3 localHitPoint{};
        if (!IntersectLocalAabb(asset.boundsMin, asset.boundsMax, localOrigin, localDir, hitDistance, localHitPoint)) continue;
        if (hitDistance < 0.0f || hitDistance >= bestDistance) continue;

        bestDistance = hitDistance;
        bestIndex = static_cast<int>(i);
    }

    return bestIndex;
}

void SceneCastleEditor::LoadAssets()
{
    std::vector<AssetInfo> assetInfos;
    std::string error;
    if (LoadAssetCatalogInfos(assetInfos, error) && ApplyAssetCatalog(assetInfos, error))
    {
        return;
    }

    std::vector<AssetInfo> fallbackInfos = BuildDefaultAssetCatalog();
    std::string fallbackError;
    if (ApplyAssetCatalog(fallbackInfos, fallbackError))
    {
        std::string saveError;
        if (!SaveAssetCatalogInfos(fallbackInfos, saveError) && error.empty())
        {
            error = saveError;
        }
    }
    else if (error.empty())
    {
        error = fallbackError;
    }

    if (!error.empty())
    {
        SetAssetCatalogStatus(error, true);
        MessageBoxA(nullptr, error.c_str(), "SceneCastleEditor", MB_OK | MB_ICONWARNING);
    }
}

void SceneCastleEditor::ReleaseAssets()
{
    SAFE_DELETE(m_modelViewDS);
    SAFE_DELETE(m_modelViewRT);
    for (AssetState& asset : m_assets)
    {
        SAFE_DELETE(asset.thumbnailDS);
        SAFE_DELETE(asset.thumbnailRT);
        SAFE_DELETE(asset.model);
    }
    m_assets.clear();
    m_modelViewSize = 0;
}

bool SceneCastleEditor::EnsureThumbnailTargets(AssetState& asset, unsigned int size)
{
    if (size < 32) size = 32;
    if (asset.thumbnailRT &&
        asset.thumbnailDS &&
        asset.thumbnailSize == size)
    {
        return true;
    }

    SAFE_DELETE(asset.thumbnailDS);
    SAFE_DELETE(asset.thumbnailRT);

    asset.thumbnailRT = new RenderTarget();
    if (FAILED(asset.thumbnailRT->Create(DXGI_FORMAT_R8G8B8A8_UNORM, size, size)))
    {
        SAFE_DELETE(asset.thumbnailRT);
        return false;
    }

    asset.thumbnailDS = new DepthStencil();
    if (FAILED(asset.thumbnailDS->Create(size, size, false)))
    {
        SAFE_DELETE(asset.thumbnailDS);
        SAFE_DELETE(asset.thumbnailRT);
        return false;
    }

    asset.thumbnailSize = size;
    asset.thumbnailDirty = true;
    return true;
}

void SceneCastleEditor::RenderAssetThumbnail(AssetState& asset, unsigned int size)
{
    if (!EnsureThumbnailTargets(asset, size)) return;
    if (!asset.thumbnailDirty && asset.thumbnailRT && asset.thumbnailRT->GetResource()) return;

    RenderTarget* targets[1] = { asset.thumbnailRT };
    SetRenderTargets(1, targets, asset.thumbnailDS);
    const float clearColor[4] = { 0.11f, 0.13f, 0.16f, 1.0f };
    asset.thumbnailRT->Clear(clearColor);
    asset.thumbnailDS->Clear();

    const DirectX::XMFLOAT3 sizeVec = {
        (asset.boundsMax.x - asset.boundsMin.x) * asset.info.scale,
        (asset.boundsMax.y - asset.boundsMin.y) * asset.info.scale,
        (asset.boundsMax.z - asset.boundsMin.z) * asset.info.scale
    };
    const float radius = std::max(1.0f, std::max(sizeVec.x, std::max(sizeVec.y, sizeVec.z)));
    const DirectX::XMFLOAT3 worldPos = { 0.0f, 0.0f, 0.0f };
    const DirectX::XMFLOAT3 worldRot = { 0.0f, DirectX::XMConvertToRadians(-25.0f), 0.0f };
    const DirectX::XMFLOAT3 worldScale = { asset.info.scale, asset.info.scale, asset.info.scale };
    const DirectX::XMFLOAT3 eye = { radius * 1.4f, radius * 1.2f, -radius * 1.8f };
    const DirectX::XMFLOAT3 look = { 0.0f, sizeVec.y * 0.35f, 0.0f };

    DirectX::XMFLOAT4X4 wvp[3];
    wvp[0] = MakeMatrixTranspose(worldPos, worldRot, worldScale, asset.placementAnchor);
    DirectX::XMStoreFloat4x4(&wvp[1], DirectX::XMMatrixTranspose(
        DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
            DirectX::XMVectorSet(look.x, look.y, look.z, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
    DirectX::XMStoreFloat4x4(&wvp[2], DirectX::XMMatrixTranspose(
        DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(35.0f), 1.0f, 0.03f, 1000.0f)));

    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        if (!mesh) continue;
        Model::Material material = *asset.model->GetMaterial(mesh->materialID);
        material.ambient = { 0.72f, 0.72f, 0.72f, 1.0f };
        ShaderList::SetMaterial(material);
        asset.model->Draw(static_cast<int>(meshIndex));
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
    asset.thumbnailDirty = false;
}

bool SceneCastleEditor::EnsureModelViewTargets(unsigned int size)
{
    if (size < 64) size = 64;
    if (m_modelViewRT &&
        m_modelViewDS &&
        m_modelViewSize == size)
    {
        return true;
    }

    SAFE_DELETE(m_modelViewDS);
    SAFE_DELETE(m_modelViewRT);

    m_modelViewRT = new RenderTarget();
    if (FAILED(m_modelViewRT->Create(DXGI_FORMAT_R8G8B8A8_UNORM, size, size)))
    {
        SAFE_DELETE(m_modelViewRT);
        return false;
    }

    m_modelViewDS = new DepthStencil();
    if (FAILED(m_modelViewDS->Create(size, size, false)))
    {
        SAFE_DELETE(m_modelViewDS);
        SAFE_DELETE(m_modelViewRT);
        return false;
    }

    m_modelViewSize = size;
    return true;
}

int SceneCastleEditor::GetModelViewAssetIndex() const
{
    if (m_selectedAssetIndex >= 0 && m_selectedAssetIndex < static_cast<int>(m_assets.size()))
    {
        return m_selectedAssetIndex;
    }

    const int selectedIndex = m_selection.GetSelectedPlacementIndex();
    if (selectedIndex >= 0 && selectedIndex < m_buildMap.GetPlacementCount())
    {
        const PlacementInfo* placement = m_buildMap.GetPlacement(selectedIndex);
        return placement ? placement->assetIndex : -1;
    }

    return -1;
}

void SceneCastleEditor::PushHistory(const HistoryEntry& entry)
{
    m_undoStack.push_back(entry);
    m_redoStack.clear();
}

int SceneCastleEditor::InsertPlacementInternal(const PlacementInfo& placement, int index)
{
    const int insertedIndex = m_buildMap.InsertPlacement(placement, index);
    m_selection.OnPlacementInserted(insertedIndex);
    return insertedIndex;
}

void SceneCastleEditor::RemovePlacementInternal(int index)
{
    m_buildMap.RemovePlacement(index);
    m_selection.OnPlacementRemoved(index, m_buildMap.GetPlacementCount());
}

void SceneCastleEditor::UpdatePlacementInternal(int index, const PlacementInfo& placement)
{
    m_buildMap.UpdatePlacement(index, placement);
    m_selection.ClampSelection(m_buildMap.GetPlacementCount());
}

void SceneCastleEditor::RenderModelView(unsigned int size)
{
    const int assetIndex = GetModelViewAssetIndex();
    if (assetIndex < 0 || assetIndex >= static_cast<int>(m_assets.size())) return;
    if (!EnsureModelViewTargets(size)) return;

    AssetState& asset = m_assets[assetIndex];
    if (!asset.model) return;

    RenderTarget* targets[1] = { m_modelViewRT };
    SetRenderTargets(1, targets, m_modelViewDS);
    const float clearColor[4] = { 0.10f, 0.12f, 0.15f, 1.0f };
    m_modelViewRT->Clear(clearColor);
    m_modelViewDS->Clear();

    const DirectX::XMFLOAT3 sizeVec = {
        (asset.boundsMax.x - asset.boundsMin.x) * asset.info.scale,
        (asset.boundsMax.y - asset.boundsMin.y) * asset.info.scale,
        (asset.boundsMax.z - asset.boundsMin.z) * asset.info.scale
    };
    const float radius = std::max(1.0f, std::max(sizeVec.x, std::max(sizeVec.y, sizeVec.z)));
    const float distance = radius * 2.2f * m_modelViewDistanceScale;
    const DirectX::XMFLOAT3 look = { 0.0f, sizeVec.y * 0.35f, 0.0f };
    const DirectX::XMFLOAT3 eye = {
        look.x + std::cos(m_modelViewPitch) * std::sin(m_modelViewYaw) * distance,
        look.y + std::sin(m_modelViewPitch) * distance,
        look.z - std::cos(m_modelViewPitch) * std::cos(m_modelViewYaw) * distance
    };

    DirectX::XMFLOAT4X4 wvp[3];
    wvp[0] = MakeMatrixTranspose({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { asset.info.scale, asset.info.scale, asset.info.scale }, asset.placementAnchor);
    DirectX::XMStoreFloat4x4(&wvp[1], DirectX::XMMatrixTranspose(
        DirectX::XMMatrixLookAtLH(
            DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f),
            DirectX::XMVectorSet(look.x, look.y, look.z, 1.0f),
            DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
    DirectX::XMStoreFloat4x4(&wvp[2], DirectX::XMMatrixTranspose(
        DirectX::XMMatrixPerspectiveFovLH(
            DirectX::XMConvertToRadians(35.0f), 1.0f, 0.03f, 1000.0f)));

    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        if (!mesh) continue;
        Model::Material material = *asset.model->GetMaterial(mesh->materialID);
        material.ambient = { 0.72f, 0.72f, 0.72f, 1.0f };
        ShaderList::SetMaterial(material);
        asset.model->Draw(static_cast<int>(meshIndex));
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
}

void SceneCastleEditor::DrawGrid() const
{
    const float planeY = 0.0f;
    const float extent = static_cast<float>(m_gridHalfExtent) * m_gridSize;
    const DirectX::XMFLOAT4 baseColor(0.35f, 0.38f, 0.42f, 1.0f);
    const DirectX::XMFLOAT4 axisXColor(0.90f, 0.20f, 0.20f, 1.0f);
    const DirectX::XMFLOAT4 axisZColor(0.20f, 0.35f, 0.90f, 1.0f);

    for (int i = -m_gridHalfExtent; i <= m_gridHalfExtent; ++i)
    {
        const float x = static_cast<float>(i) * m_gridSize;
        const float z = static_cast<float>(i) * m_gridSize;
        Geometory::AddLine({ x, planeY, -extent }, { x, planeY, extent }, (i == 0) ? axisXColor : baseColor);
        Geometory::AddLine({ -extent, planeY, z }, { extent, planeY, z }, (i == 0) ? axisZColor : baseColor);
    }
    Geometory::DrawLines();
}

void SceneCastleEditor::DrawPlacement(const PlacementInfo& placement, bool isPreview, bool isSelected) const
{
    if (placement.assetIndex < 0 || placement.assetIndex >= static_cast<int>(m_assets.size())) return;

    const AssetState& asset = m_assets[placement.assetIndex];
    if (!asset.model) return;

    const float rotationY = static_cast<float>(placement.rotationQuarterTurns) * kHalfPi;
    const DirectX::XMFLOAT3 worldPos = GridToWorld(
        placement.gridX,
        placement.gridY,
        placement.gridZ,
        asset.info.yOffset);
    const DirectX::XMFLOAT3 worldRot = { 0.0f, rotationY, 0.0f };
    const DirectX::XMFLOAT3 worldScale = { asset.info.scale, asset.info.scale, asset.info.scale };
    DirectX::XMFLOAT4X4 wvp[3];
    wvp[0] = MakeMatrixTranspose(worldPos, worldRot, worldScale, asset.placementAnchor);

    using namespace DirectX;
    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(m_cameraEye.x, m_cameraEye.y, m_cameraEye.z, 1.0f),
        XMVectorSet(m_cameraLook.x, m_cameraLook.y, m_cameraLook.z, 1.0f),
        XMVectorSet(m_cameraUp.x, m_cameraUp.y, m_cameraUp.z, 0.0f));
    const XMMATRIX proj = XMMatrixPerspectiveFovLH(m_cameraFovy, m_cameraAspect, m_cameraNear, m_cameraFar);
    XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(view));
    XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(proj));

    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(m_cameraEye);
    asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));

    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        if (!mesh) continue;

        Model::Material material = *asset.model->GetMaterial(mesh->materialID);
        if (isPreview)
        {
            if (m_canPlacePreview)
            {
                material.diffuse = { 0.45f, 0.90f, 0.55f, 1.0f };
                material.ambient = { 0.35f, 0.65f, 0.40f, 1.0f };
            }
            else
            {
                material.diffuse = { 0.95f, 0.35f, 0.35f, 1.0f };
                material.ambient = { 0.65f, 0.20f, 0.20f, 1.0f };
            }
        }
        else if (isSelected)
        {
            material.diffuse = { 0.65f, 0.85f, 1.00f, 1.0f };
            material.ambient = { 0.40f, 0.55f, 0.70f, 1.0f };
        }
        else
        {
            material.ambient = { 0.65f, 0.65f, 0.65f, 1.0f };
        }

        ShaderList::SetMaterial(material);
        asset.model->Draw(static_cast<int>(meshIndex));
    }

    if (!isPreview && isSelected)
    {
        DrawSelectionFrame(placement);
    }
}

void SceneCastleEditor::DrawSelectionFrame(const PlacementInfo& placement) const
{
    DrawPlacementBounds(placement, { 0.25f, 0.85f, 1.0f, 1.0f }, 0.05f);
}

void SceneCastleEditor::DrawPlacementBounds(const PlacementInfo& placement, const DirectX::XMFLOAT4& color, float inflate) const
{
    if (placement.assetIndex < 0 || placement.assetIndex >= static_cast<int>(m_assets.size())) return;

    const AssetState& asset = m_assets[placement.assetIndex];
    const DirectX::XMFLOAT3 worldPos = GridToWorld(
        placement.gridX,
        placement.gridY,
        placement.gridZ,
        asset.info.yOffset);
    const DirectX::XMFLOAT3 worldRot = { 0.0f, static_cast<float>(placement.rotationQuarterTurns) * kHalfPi, 0.0f };
    const DirectX::XMFLOAT3 worldScale = { asset.info.scale, asset.info.scale, asset.info.scale };

    DirectX::XMFLOAT3 minValue = asset.boundsMin;
    DirectX::XMFLOAT3 maxValue = asset.boundsMax;
    minValue.x -= inflate;
    minValue.y -= inflate;
    minValue.z -= inflate;
    maxValue.x += inflate;
    maxValue.y += inflate;
    maxValue.z += inflate;

    const DirectX::XMFLOAT3 corners[8] = {
        { minValue.x, minValue.y, minValue.z },
        { maxValue.x, minValue.y, minValue.z },
        { maxValue.x, maxValue.y, minValue.z },
        { minValue.x, maxValue.y, minValue.z },
        { minValue.x, minValue.y, maxValue.z },
        { maxValue.x, minValue.y, maxValue.z },
        { maxValue.x, maxValue.y, maxValue.z },
        { minValue.x, maxValue.y, maxValue.z },
    };

    DirectX::XMFLOAT3 transformed[8];
    for (int i = 0; i < 8; ++i)
    {
        transformed[i] = TransformPoint(corners[i], worldPos, worldRot, worldScale, asset.placementAnchor);
    }

    for (const auto& edge : kWireEdges)
    {
        Geometory::AddLine(transformed[edge[0]], transformed[edge[1]], color);
    }
    Geometory::DrawLines();
}

void SceneCastleEditor::AddGridCellFrame(int gridX, int gridY, int gridZ, const DirectX::XMFLOAT4& color, float inset) const
{
    const float halfSize = m_gridSize * 0.5f - inset;
    const float minY = static_cast<float>(gridY) * m_gridHeightStep + inset;
    const float maxY = static_cast<float>(gridY + 1) * m_gridHeightStep - inset;
    const float centerX = static_cast<float>(gridX) * m_gridSize;
    const float centerZ = static_cast<float>(gridZ) * m_gridSize;

    const DirectX::XMFLOAT3 corners[8] = {
        { centerX - halfSize, minY, centerZ - halfSize },
        { centerX + halfSize, minY, centerZ - halfSize },
        { centerX + halfSize, maxY, centerZ - halfSize },
        { centerX - halfSize, maxY, centerZ - halfSize },
        { centerX - halfSize, minY, centerZ + halfSize },
        { centerX + halfSize, minY, centerZ + halfSize },
        { centerX + halfSize, maxY, centerZ + halfSize },
        { centerX - halfSize, maxY, centerZ + halfSize },
    };

    for (const auto& edge : kWireEdges)
    {
        Geometory::AddLine(corners[edge[0]], corners[edge[1]], color);
    }
}

bool SceneCastleEditor::IsInsideGrid(int gridX, int gridY, int gridZ) const
{
    if (gridY < 0 || gridY > 16) return false;
    return std::abs(gridX) <= m_gridHalfExtent && std::abs(gridZ) <= m_gridHalfExtent;
}

int SceneCastleEditor::FindPlacementAt(int gridX, int gridY, int gridZ) const
{
    return m_buildMap.FindPlacementIndexAt(gridX, gridY, gridZ);
}


bool SceneCastleEditor::RaycastToLayer(float localMouseX, float localMouseY, float viewWidth, float viewHeight, int layer, int& outGridX, int& outGridZ) const
{
    using namespace DirectX;

    if (viewWidth <= 1.0f || viewHeight <= 1.0f) return false;

    const float ndcX = (localMouseX / viewWidth) * 2.0f - 1.0f;
    const float ndcY = 1.0f - (localMouseY / viewHeight) * 2.0f;

    const XMMATRIX view = XMMatrixLookAtLH(
        XMVectorSet(m_cameraEye.x, m_cameraEye.y, m_cameraEye.z, 1.0f),
        XMVectorSet(m_cameraLook.x, m_cameraLook.y, m_cameraLook.z, 1.0f),
        XMVectorSet(m_cameraUp.x, m_cameraUp.y, m_cameraUp.z, 0.0f));
    const XMMATRIX proj = XMMatrixPerspectiveFovLH(m_cameraFovy, m_cameraAspect, m_cameraNear, m_cameraFar);
    const XMMATRIX invViewProj = XMMatrixInverse(nullptr, view * proj);

    const XMVECTOR nearPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProj);
    const XMVECTOR farPoint = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProj);
    const XMVECTOR rayDir = XMVector3Normalize(farPoint - nearPoint);
    const float dirY = XMVectorGetY(rayDir);
    if (std::fabs(dirY) < 1.0e-5f) return false;

    const float planeY = static_cast<float>(layer) * m_gridHeightStep;
    const float originY = XMVectorGetY(nearPoint);
    const float t = (planeY - originY) / dirY;
    if (t < 0.0f) return false;

    const XMVECTOR hitPoint = nearPoint + rayDir * t;
    const float hitX = XMVectorGetX(hitPoint);
    const float hitZ = XMVectorGetZ(hitPoint);

    outGridX = static_cast<int>(std::round(hitX / m_gridSize));
    outGridZ = static_cast<int>(std::round(hitZ / m_gridSize));
    return IsInsideGrid(outGridX, layer, outGridZ);
}

bool SceneCastleEditor::RaycastToActiveLayer(float localMouseX, float localMouseY, float viewWidth, float viewHeight, int& outGridX, int& outGridZ) const
{
    return RaycastToLayer(localMouseX, localMouseY, viewWidth, viewHeight, m_activeLayer, outGridX, outGridZ);
}

DirectX::XMFLOAT3 SceneCastleEditor::GridToWorld(int gridX, int gridY, int gridZ, float yOffset) const
{
    return {
        static_cast<float>(gridX) * m_gridSize,
        static_cast<float>(gridY) * m_gridHeightStep + yOffset,
        static_cast<float>(gridZ) * m_gridSize
    };
}

void SceneCastleEditor::OrbitCamera(float deltaX, float deltaY)
{
    m_cameraYaw += deltaX * 0.01f;
    m_cameraPitch = ClampFloat(m_cameraPitch - deltaY * 0.01f, -1.35f, 1.35f);

    using namespace DirectX;
    XMVECTOR offset = XMVectorSet(
        std::cos(m_cameraPitch) * std::sin(m_cameraYaw),
        std::sin(m_cameraPitch),
        std::cos(m_cameraPitch) * std::cos(m_cameraYaw),
        0.0f);
    offset *= m_cameraDistance;

    const XMVECTOR look = XMLoadFloat3(&m_cameraLook);
    XMVECTOR eye = look - offset;
    XMStoreFloat3(&m_cameraEye, eye);
}

void SceneCastleEditor::PanCamera(float deltaX, float deltaY)
{
    using namespace DirectX;

    const XMVECTOR eye = XMLoadFloat3(&m_cameraEye);
    const XMVECTOR look = XMLoadFloat3(&m_cameraLook);
    XMVECTOR forward = look - eye;
    if (XMVectorGetX(XMVector3LengthSq(forward)) < 1.0e-6f)
    {
        forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }
    forward = XMVector3Normalize(forward);

    const XMVECTOR worldUp = XMVectorSet(m_cameraUp.x, m_cameraUp.y, m_cameraUp.z, 0.0f);
    XMVECTOR right = XMVector3Cross(worldUp, forward);
    if (XMVectorGetX(XMVector3LengthSq(right)) < 1.0e-6f)
    {
        right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    right = XMVector3Normalize(right);
    XMVECTOR cameraUp = XMVector3Normalize(XMVector3Cross(forward, right));

    const float panScaleX = (m_cameraDistance * 0.15f > 1.0f) ? (m_cameraDistance * 0.15f) : 1.0f;
    const float panScaleY = (m_cameraDistance * 0.10f > 1.0f) ? (m_cameraDistance * 0.10f) : 1.0f;
    const XMVECTOR delta =
        right * (-deltaX * 0.01f * panScaleX) +
        cameraUp * (deltaY * 0.01f * panScaleY);

    XMVECTOR newEye = eye + delta;
    XMVECTOR newLook = look + delta;
    XMStoreFloat3(&m_cameraEye, newEye);
    XMStoreFloat3(&m_cameraLook, newLook);
    SyncCameraAnglesFromPose();
}

void SceneCastleEditor::ZoomCamera(float delta)
{
    m_cameraDistance = ClampFloat(m_cameraDistance - delta, 2.5f, 80.0f);
    OrbitCamera(0.0f, 0.0f);
}

void SceneCastleEditor::SyncCameraAnglesFromPose()
{
    const float dx = m_cameraLook.x - m_cameraEye.x;
    const float dy = m_cameraLook.y - m_cameraEye.y;
    const float dz = m_cameraLook.z - m_cameraEye.z;
    const float distSq = dx * dx + dy * dy + dz * dz;
    if (distSq < 1.0e-6f)
    {
        m_cameraDistance = 1.0f;
        return;
    }

    m_cameraDistance = std::sqrt(distSq);
    m_cameraPitch = std::asin(ClampFloat(dy / m_cameraDistance, -1.0f, 1.0f));
    m_cameraYaw = std::atan2(dx, dz);
}

