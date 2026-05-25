#include "NarakuMapData.h"

#include <Windows.h>
#undef max
#undef min

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <string>

namespace
{
        constexpr int kCurrentMapVersion = 2;
    const wchar_t* kDefaultMapPath = L"C:\\HAL\\\u500b\\\u4eba\\\u5236\\\u4f5c\\NarakuProto\\Assets\\Maps\\FirstLayer.json";
    std::wstring gCurrentMapPath = kDefaultMapPath;

    struct JsonValue
    {
        enum Type
        {
            TypeNull = 0,
            TypeBool,
            TypeNumber,
            TypeString,
            TypeArray,
            TypeObject
        };

        Type type = TypeNull;
        bool boolValue = false;
        double numberValue = 0.0;
        std::string stringValue;
        std::vector<JsonValue> arrayValue;
        std::map<std::string, JsonValue> objectValue;
    };

    int HexDigitValue(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    void AppendUtf8CodePoint(std::string& outText, unsigned int codePoint)
    {
        if (codePoint <= 0x7F)
        {
            outText.push_back(static_cast<char>(codePoint));
        }
        else if (codePoint <= 0x7FF)
        {
            outText.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
            outText.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else if (codePoint <= 0xFFFF)
        {
            outText.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
            outText.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            outText.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
        else
        {
            outText.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
            outText.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            outText.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            outText.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }

    class JsonParser
    {
    public:
        explicit JsonParser(const std::string& text)
            : m_text(text)
        {
        }

        bool Parse(JsonValue& outValue)
        {
            SkipWs();
            if (!ParseValue(outValue)) return false;
            SkipWs();
            return m_pos == m_text.size();
        }

    private:
        void SkipWs()
        {
            while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos])))
            {
                ++m_pos;
            }
        }

        bool Match(const char* literal)
        {
            const size_t len = std::char_traits<char>::length(literal);
            if (m_text.compare(m_pos, len, literal) != 0) return false;
            m_pos += len;
            return true;
        }

        bool ParseValue(JsonValue& outValue)
        {
            SkipWs();
            if (m_pos >= m_text.size()) return false;
            const char c = m_text[m_pos];
            if (c == '{') return ParseObject(outValue);
            if (c == '[') return ParseArray(outValue);
            if (c == '"')
            {
                outValue.type = JsonValue::TypeString;
                return ParseString(outValue.stringValue);
            }
            if (c == '-' || (c >= '0' && c <= '9'))
            {
                outValue.type = JsonValue::TypeNumber;
                return ParseNumber(outValue.numberValue);
            }
            if (Match("true"))
            {
                outValue.type = JsonValue::TypeBool;
                outValue.boolValue = true;
                return true;
            }
            if (Match("false"))
            {
                outValue.type = JsonValue::TypeBool;
                outValue.boolValue = false;
                return true;
            }
            if (Match("null"))
            {
                outValue.type = JsonValue::TypeNull;
                return true;
            }
            return false;
        }

        bool ParseObject(JsonValue& outValue)
        {
            if (m_text[m_pos] != '{') return false;
            ++m_pos;
            outValue.type = JsonValue::TypeObject;
            outValue.objectValue.clear();
            SkipWs();
            if (m_pos < m_text.size() && m_text[m_pos] == '}')
            {
                ++m_pos;
                return true;
            }

            while (m_pos < m_text.size())
            {
                SkipWs();
                std::string key;
                if (!ParseString(key)) return false;
                SkipWs();
                if (m_pos >= m_text.size() || m_text[m_pos] != ':') return false;
                ++m_pos;
                JsonValue value;
                if (!ParseValue(value)) return false;
                outValue.objectValue[key] = value;
                SkipWs();
                if (m_pos >= m_text.size()) return false;
                if (m_text[m_pos] == '}')
                {
                    ++m_pos;
                    return true;
                }
                if (m_text[m_pos] != ',') return false;
                ++m_pos;
            }
            return false;
        }

        bool ParseArray(JsonValue& outValue)
        {
            if (m_text[m_pos] != '[') return false;
            ++m_pos;
            outValue.type = JsonValue::TypeArray;
            outValue.arrayValue.clear();
            SkipWs();
            if (m_pos < m_text.size() && m_text[m_pos] == ']')
            {
                ++m_pos;
                return true;
            }

            while (m_pos < m_text.size())
            {
                JsonValue value;
                if (!ParseValue(value)) return false;
                outValue.arrayValue.push_back(value);
                SkipWs();
                if (m_pos >= m_text.size()) return false;
                if (m_text[m_pos] == ']')
                {
                    ++m_pos;
                    return true;
                }
                if (m_text[m_pos] != ',') return false;
                ++m_pos;
            }
            return false;
        }

        bool ParseString(std::string& outText)
        {
            if (m_text[m_pos] != '"') return false;
            ++m_pos;
            outText.clear();
            while (m_pos < m_text.size())
            {
                const char c = m_text[m_pos++];
                if (c == '"')
                {
                    return true;
                }
                if (c == '\\')
                {
                    if (m_pos >= m_text.size()) return false;
                    const char escape = m_text[m_pos++];
                    switch (escape)
                    {
                    case '"': outText.push_back('"'); break;
                    case '\\': outText.push_back('\\'); break;
                    case '/': outText.push_back('/'); break;
                    case 'b': outText.push_back('\b'); break;
                    case 'f': outText.push_back('\f'); break;
                    case 'n': outText.push_back('\n'); break;
                    case 'r': outText.push_back('\r'); break;
                    case 't': outText.push_back('\t'); break;
                    case 'u':
                    {
                        if (m_pos + 4 > m_text.size()) return false;
                        unsigned int codePoint = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            const int digit = HexDigitValue(m_text[m_pos + i]);
                            if (digit < 0) return false;
                            codePoint = (codePoint << 4) | static_cast<unsigned int>(digit);
                        }
                        m_pos += 4;
                        AppendUtf8CodePoint(outText, codePoint);
                        break;
                    }
                    default:
                        return false;
                    }
                    continue;
                }
                outText.push_back(c);
            }
            return false;
        }

        bool ParseNumber(double& outNumber)
        {
            const size_t start = m_pos;
            if (m_text[m_pos] == '-') ++m_pos;
            while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            if (m_pos < m_text.size() && m_text[m_pos] == '.')
            {
                ++m_pos;
                while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            }
            if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E'))
            {
                ++m_pos;
                if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) ++m_pos;
                while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
            }
            outNumber = std::strtod(m_text.c_str() + start, nullptr);
            return true;
        }

        const std::string& m_text;
        size_t m_pos = 0;
    };

    bool ReadWholeFile(const wchar_t* path, std::string& outText)
    {
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, path, L"rb") != 0 || !fp)
        {
            return false;
        }

        std::fseek(fp, 0, SEEK_END);
        const long size = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        outText.resize(size > 0 ? static_cast<size_t>(size) : 0);
        if (!outText.empty())
        {
            std::fread(&outText[0], 1, outText.size(), fp);
        }
        std::fclose(fp);
        return true;
    }

    bool WriteWholeFile(const wchar_t* path, const std::string& text)
    {
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, path, L"wb") != 0 || !fp)
        {
            return false;
        }

        if (!text.empty())
        {
            std::fwrite(text.data(), 1, text.size(), fp);
        }
        std::fclose(fp);
        return true;
    }

        void EnsureMapDirectories()
    {
        ::CreateDirectoryW(L"C:\\HAL\\\u500b\\\u4eba\\\u5236\\\u4f5c\\NarakuProto\\Assets", nullptr);
        ::CreateDirectoryW(L"C:\\HAL\\\u500b\\\u4eba\\\u5236\\\u4f5c\\NarakuProto\\Assets\\Maps", nullptr);
    }

    void AppendIndent(std::ostringstream& out, int indent)
    {
        for (int i = 0; i < indent; ++i)
        {
            out << "  ";
        }
    }

    template<typename T>
    bool GetObjectValue(const JsonValue& objectValue, const char* key, const T*& outValue);

    template<>
    bool GetObjectValue<JsonValue>(const JsonValue& objectValue, const char* key, const JsonValue*& outValue)
    {
        if (objectValue.type != JsonValue::TypeObject) return false;
        auto it = objectValue.objectValue.find(key);
        if (it == objectValue.objectValue.end()) return false;
        outValue = &it->second;
        return true;
    }

    bool GetNumber(const JsonValue& objectValue, const char* key, double& outNumber)
    {
        const JsonValue* value = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, value)) return false;
        if (value->type != JsonValue::TypeNumber) return false;
        outNumber = value->numberValue;
        return true;
    }

    bool GetBool(const JsonValue& objectValue, const char* key, bool& outBool)
    {
        const JsonValue* value = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, value)) return false;
        if (value->type != JsonValue::TypeBool) return false;
        outBool = value->boolValue;
        return true;
    }

    bool GetString(const JsonValue& objectValue, const char* key, std::string& outText)
    {
        const JsonValue* value = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, value)) return false;
        if (value->type != JsonValue::TypeString) return false;
        outText = value->stringValue;
        return true;
    }

    std::string EscapeJsonString(const std::string& text)
    {
        std::string outText;
        outText.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '\\': outText += "\\\\"; break;
            case '"': outText += "\\\""; break;
            case '\n': outText += "\\n"; break;
            case '\r': outText += "\\r"; break;
            case '\t': outText += "\\t"; break;
            default: outText.push_back(c); break;
            }
        }
        return outText;
    }
}

namespace NarakuMap
{
    namespace
    {
        int GetLayerVertexCount(const TerrainLayer& layer)
        {
            return std::max(0, layer.gridWidth) * std::max(0, layer.gridHeight);
        }

        int GetLayerCellCount(const TerrainLayer& layer)
        {
            const int cellWidth = std::max(0, layer.gridWidth - 1);
            const int cellHeight = std::max(0, layer.gridHeight - 1);
            return cellWidth * cellHeight;
        }

        bool IsCellIndexValid(const TerrainLayer& layer, int cellX, int cellZ)
        {
            return cellX >= 0 && cellZ >= 0 && cellX < layer.gridWidth - 1 && cellZ < layer.gridHeight - 1;
        }

        void NormalizeLayerPoint(LayerPoint& point, const MapData& mapData, int fallbackLayerId)
        {
            if (FindLayerIndexById(mapData, point.layerId) < 0)
            {
                point.layerId = fallbackLayerId;
            }
        }

        void AppendLayerPoint(std::ostringstream& out, const char* name, const LayerPoint& point, int indent)
        {
            AppendIndent(out, indent);
            out << '"' << name << "\": { "
                << "\"x\": " << point.xz.x
                << ", \"z\": " << point.xz.z
                << ", \"layerId\": " << point.layerId << " }";
        }
    }

    const wchar_t* GetDefaultMapPath()
    {
        return kDefaultMapPath;
    }
    const wchar_t* GetCurrentMapPath()
    {
        return gCurrentMapPath.c_str();
    }

    void SetCurrentMapPath(const wchar_t* path)
    {
        if (path == nullptr || path[0] == L'\0')
        {
            gCurrentMapPath = kDefaultMapPath;
            return;
        }

        gCurrentMapPath = path;
    }


    void EnsureLayerHeights(TerrainLayer& layer)
    {
        const int count = GetLayerVertexCount(layer);
        if (static_cast<int>(layer.heights.size()) != count)
        {
            layer.heights.resize(count, 0.0f);
        }
    }

    void EnsureLayerVertexEnabled(TerrainLayer& layer)
    {
        const int count = GetLayerVertexCount(layer);
        if (static_cast<int>(layer.vertexEnabled.size()) != count)
        {
            layer.vertexEnabled.resize(count, 1u);
        }
    }

    void EnsureLayerCellAttributes(TerrainLayer& layer)
    {
        const int count = GetLayerCellCount(layer);
        if (static_cast<int>(layer.cellAttributeFlags.size()) != count)
        {
            layer.cellAttributeFlags.resize(count, CellAttributeNone);
        }
    }

    float GetVertexHeight(const TerrainLayer& layer, int gridX, int gridZ)
    {
        if (gridX < 0 || gridZ < 0 || gridX >= layer.gridWidth || gridZ >= layer.gridHeight)
        {
            return 0.0f;
        }

        const int index = gridZ * layer.gridWidth + gridX;
        if (index < 0 || index >= static_cast<int>(layer.heights.size()))
        {
            return 0.0f;
        }
        return layer.heights[index];
    }

    void SetVertexHeight(TerrainLayer& layer, int gridX, int gridZ, float height)
    {
        EnsureLayerHeights(layer);
        if (gridX < 0 || gridZ < 0 || gridX >= layer.gridWidth || gridZ >= layer.gridHeight)
        {
            return;
        }
        layer.heights[gridZ * layer.gridWidth + gridX] = height;
    }

    bool IsVertexEnabled(const TerrainLayer& layer, int gridX, int gridZ)
    {
        if (gridX < 0 || gridZ < 0 || gridX >= layer.gridWidth || gridZ >= layer.gridHeight)
        {
            return false;
        }

        if (layer.vertexEnabled.empty())
        {
            return true;
        }

        const int index = gridZ * layer.gridWidth + gridX;
        if (index < 0 || index >= static_cast<int>(layer.vertexEnabled.size()))
        {
            return false;
        }

        return layer.vertexEnabled[index] != 0u;
    }

    void SetVertexEnabled(TerrainLayer& layer, int gridX, int gridZ, bool enabled)
    {
        EnsureLayerVertexEnabled(layer);
        if (gridX < 0 || gridZ < 0 || gridX >= layer.gridWidth || gridZ >= layer.gridHeight)
        {
            return;
        }

        layer.vertexEnabled[gridZ * layer.gridWidth + gridX] = enabled ? 1u : 0u;
    }

    std::uint32_t GetCellAttributeFlags(const TerrainLayer& layer, int cellX, int cellZ)
    {
        if (!IsCellIndexValid(layer, cellX, cellZ))
        {
            return CellAttributeNone;
        }

        const int index = cellZ * (layer.gridWidth - 1) + cellX;
        if (index < 0 || index >= static_cast<int>(layer.cellAttributeFlags.size()))
        {
            return CellAttributeNone;
        }
        return layer.cellAttributeFlags[index];
    }

    void SetCellAttributeFlags(TerrainLayer& layer, int cellX, int cellZ, std::uint32_t flags)
    {
        EnsureLayerCellAttributes(layer);
        if (!IsCellIndexValid(layer, cellX, cellZ))
        {
            return;
        }

        const int index = cellZ * (layer.gridWidth - 1) + cellX;
        layer.cellAttributeFlags[index] = flags;
    }

    bool HasCellAttributeFlag(const TerrainLayer& layer, int cellX, int cellZ, std::uint32_t flag)
    {
        return (GetCellAttributeFlags(layer, cellX, cellZ) & flag) != 0u;
    }

    int FindLayerIndexById(const MapData& mapData, int layerId)
    {
        for (int i = 0; i < static_cast<int>(mapData.terrainLayers.size()); ++i)
        {
            if (mapData.terrainLayers[i].id == layerId)
            {
                return i;
            }
        }
        return -1;
    }

    bool TryGetCellFromPosition(const TerrainLayer& layer, const Vec2& position, int& outCellX, int& outCellZ)
    {
        const int cellWidth = std::max(0, layer.gridWidth - 1);
        const int cellHeight = std::max(0, layer.gridHeight - 1);
        if (cellWidth <= 0 || cellHeight <= 0 || layer.cellSize <= 0.0f)
        {
            return false;
        }

        const float minX = layer.center.x - (static_cast<float>(cellWidth) * layer.cellSize * 0.5f);
        const float minZ = layer.center.z - (static_cast<float>(cellHeight) * layer.cellSize * 0.5f);
        const float localX = position.x - minX;
        const float localZ = position.z - minZ;
        if (localX < 0.0f || localZ < 0.0f)
        {
            return false;
        }

        const int cellX = static_cast<int>(localX / layer.cellSize);
        const int cellZ = static_cast<int>(localZ / layer.cellSize);
        if (!IsCellIndexValid(layer, cellX, cellZ))
        {
            return false;
        }

        outCellX = cellX;
        outCellZ = cellZ;
        return true;
    }

    bool TryGetLayerAndCell(const MapData& mapData, const LayerPoint& point, int& outLayerIndex, int& outCellX, int& outCellZ)
    {
        outLayerIndex = FindLayerIndexById(mapData, point.layerId);
        if (outLayerIndex < 0)
        {
            return false;
        }
        return TryGetCellFromPosition(mapData.terrainLayers[outLayerIndex], point.xz, outCellX, outCellZ);
    }

    bool TryGetLayerAndCell(const MapData& mapData, int layerId, const Vec2& position, int& outLayerIndex, int& outCellX, int& outCellZ)
    {
        outLayerIndex = FindLayerIndexById(mapData, layerId);
        if (outLayerIndex < 0)
        {
            return false;
        }
        return TryGetCellFromPosition(mapData.terrainLayers[outLayerIndex], position, outCellX, outCellZ);
    }

    bool IsBlockedCell(const TerrainLayer& layer, int cellX, int cellZ)
    {
        const std::uint32_t flags = GetCellAttributeFlags(layer, cellX, cellZ);
        return (flags & (CellAttributeBlocked | CellAttributeRemoved)) != 0u;
    }

    bool IsHazardCell(const TerrainLayer& layer, int cellX, int cellZ)
    {
        return HasCellAttributeFlag(layer, cellX, cellZ, CellAttributeHazard);
    }

    bool IsRopeAnchorCell(const TerrainLayer& layer, int cellX, int cellZ)
    {
        return HasCellAttributeFlag(layer, cellX, cellZ, CellAttributeRopeAnchor);
    }

    struct ReachableCellKey
    {
        int layerIndex = -1;
        int cellX = -1;
        int cellZ = -1;

        bool operator<(const ReachableCellKey& other) const
        {
            if (layerIndex != other.layerIndex) return layerIndex < other.layerIndex;
            if (cellX != other.cellX) return cellX < other.cellX;
            return cellZ < other.cellZ;
        }
    };

    std::set<ReachableCellKey> BuildReachableCellSet(const MapData& mapData)
    {
        std::set<ReachableCellKey> visited;
        int startLayerIndex = -1;
        int startCellX = -1;
        int startCellZ = -1;
        if (!TryGetLayerAndCell(mapData, mapData.playerStartPoint, startLayerIndex, startCellX, startCellZ))
        {
            return visited;
        }

        const TerrainLayer& startLayer = mapData.terrainLayers[startLayerIndex];
        if (IsBlockedCell(startLayer, startCellX, startCellZ))
        {
            return visited;
        }

        std::map<ReachableCellKey, std::vector<ReachableCellKey>> ropeLinks;
        for (const RopePoint& rope : mapData.ropes)
        {
            int topLayerIndex = -1;
            int topCellX = -1;
            int topCellZ = -1;
            int bottomLayerIndex = -1;
            int bottomCellX = -1;
            int bottomCellZ = -1;
            if (!TryGetLayerAndCell(mapData, rope.topLayerId, rope.xz, topLayerIndex, topCellX, topCellZ))
            {
                continue;
            }
            if (!TryGetLayerAndCell(mapData, rope.bottomLayerId, rope.xz, bottomLayerIndex, bottomCellX, bottomCellZ))
            {
                continue;
            }

            ReachableCellKey topKey{ topLayerIndex, topCellX, topCellZ };
            ReachableCellKey bottomKey{ bottomLayerIndex, bottomCellX, bottomCellZ };
            ropeLinks[topKey].push_back(bottomKey);
            ropeLinks[bottomKey].push_back(topKey);
        }

        std::queue<ReachableCellKey> pending;
        ReachableCellKey startKey{ startLayerIndex, startCellX, startCellZ };
        visited.insert(startKey);
        pending.push(startKey);

        const int offsets[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
        while (!pending.empty())
        {
            const ReachableCellKey current = pending.front();
            pending.pop();

            const TerrainLayer& currentLayer = mapData.terrainLayers[current.layerIndex];
            for (int offsetIndex = 0; offsetIndex < 4; ++offsetIndex)
            {
                const int nextCellX = current.cellX + offsets[offsetIndex][0];
                const int nextCellZ = current.cellZ + offsets[offsetIndex][1];
                if (!IsCellIndexValid(currentLayer, nextCellX, nextCellZ))
                {
                    continue;
                }
                if (IsBlockedCell(currentLayer, nextCellX, nextCellZ))
                {
                    continue;
                }

                ReachableCellKey nextKey{ current.layerIndex, nextCellX, nextCellZ };
                if (visited.insert(nextKey).second)
                {
                    pending.push(nextKey);
                }
            }

            const auto ropeIt = ropeLinks.find(current);
            if (ropeIt == ropeLinks.end())
            {
                continue;
            }

            for (const ReachableCellKey& linkedCell : ropeIt->second)
            {
                const TerrainLayer& linkedLayer = mapData.terrainLayers[linkedCell.layerIndex];
                if (IsBlockedCell(linkedLayer, linkedCell.cellX, linkedCell.cellZ))
                {
                    continue;
                }

                if (visited.insert(linkedCell).second)
                {
                    pending.push(linkedCell);
                }
            }
        }

        return visited;
    }

    std::vector<ValidationIssue> ValidateMapData(const MapData& mapData)
    {
        std::vector<ValidationIssue> issues;
        if (mapData.terrainLayers.empty())
        {
            issues.push_back({ ValidationIssue::Error, u8"地形レイヤーが 1 枚もありません。" });
            return issues;
        }

        bool hasVisibleLayer = false;
        for (int i = 0; i < static_cast<int>(mapData.terrainLayers.size()); ++i)
        {
            const TerrainLayer& layer = mapData.terrainLayers[i];
            hasVisibleLayer |= layer.visible;
            if (layer.gridWidth < 2 || layer.gridHeight < 2)
            {
                issues.push_back({ ValidationIssue::Error, u8"グリッドサイズが 2 未満のレイヤーがあります。" });
            }
            if (layer.cellSize <= 0.0f)
            {
                issues.push_back({ ValidationIssue::Error, u8"セルサイズが 0 以下のレイヤーがあります。" });
            }
            if (static_cast<int>(layer.heights.size()) != GetLayerVertexCount(layer))
            {
                issues.push_back({ ValidationIssue::Warning, u8"頂点高さ配列サイズがグリッドと一致していません。保存時に補正されます。" });
            }
            if (!layer.vertexEnabled.empty() && static_cast<int>(layer.vertexEnabled.size()) != GetLayerVertexCount(layer))
            {
                issues.push_back({ ValidationIssue::Warning, u8"頂点有効配列サイズがグリッドと一致していません。保存時に補正されます。" });
            }
            if (static_cast<int>(layer.cellAttributeFlags.size()) != GetLayerCellCount(layer))
            {
                issues.push_back({ ValidationIssue::Warning, u8"セル属性配列サイズがグリッドと一致していません。保存時に補正されます。" });
            }
            for (int j = i + 1; j < static_cast<int>(mapData.terrainLayers.size()); ++j)
            {
                if (mapData.terrainLayers[j].id == layer.id)
                {
                    issues.push_back({ ValidationIssue::Error, u8"重複したレイヤー ID があります。" });
                }
            }
        }

        if (!hasVisibleLayer)
        {
            issues.push_back({ ValidationIssue::Warning, u8"可視状態のレイヤーが 1 枚もありません。" });
        }

        int startLayerIndex = -1;
        int startCellX = -1;
        int startCellZ = -1;
        if (!TryGetLayerAndCell(mapData, mapData.playerStartPoint, startLayerIndex, startCellX, startCellZ))
        {
            issues.push_back({ ValidationIssue::Error, u8"プレイヤー開始地点が有効なセル範囲外にあります。" });
        }
        else
        {
            const TerrainLayer& startLayer = mapData.terrainLayers[startLayerIndex];
            if (IsBlockedCell(startLayer, startCellX, startCellZ))
            {
                issues.push_back({ ValidationIssue::Error, u8"プレイヤー開始地点が歩行不可セル上にあります。" });
            }
            if (IsHazardCell(startLayer, startCellX, startCellZ))
            {
                issues.push_back({ ValidationIssue::Warning, u8"プレイヤー開始地点が危険地形セル上にあります。" });
            }
        }

        int returnLayerIndex = -1;
        int returnCellX = -1;
        int returnCellZ = -1;
        if (!TryGetLayerAndCell(mapData, mapData.returnPoint, returnLayerIndex, returnCellX, returnCellZ))
        {
            issues.push_back({ ValidationIssue::Error, u8"帰還地点が有効なセル範囲外にあります。" });
        }
        else
        {
            const TerrainLayer& returnLayer = mapData.terrainLayers[returnLayerIndex];
            if (IsBlockedCell(returnLayer, returnCellX, returnCellZ))
            {
                issues.push_back({ ValidationIssue::Error, u8"帰還地点が歩行不可セル上にあります。" });
            }
            if (IsHazardCell(returnLayer, returnCellX, returnCellZ))
            {
                issues.push_back({ ValidationIssue::Warning, u8"帰還地点が危険地形セル上にあります。" });
            }
        }

        bool hasEnabledMiningPoint = false;
        for (const RopePoint& rope : mapData.ropes)
        {
            int topLayerIndex = -1;
            int topCellX = -1;
            int topCellZ = -1;
            int bottomLayerIndex = -1;
            int bottomCellX = -1;
            int bottomCellZ = -1;
            if (!TryGetLayerAndCell(mapData, rope.topLayerId, rope.xz, topLayerIndex, topCellX, topCellZ) ||
                !TryGetLayerAndCell(mapData, rope.bottomLayerId, rope.xz, bottomLayerIndex, bottomCellX, bottomCellZ))
            {
                issues.push_back({ ValidationIssue::Error, u8"ロープの接続位置がレイヤー範囲外です。" });
                continue;
            }

            const TerrainLayer& topLayer = mapData.terrainLayers[topLayerIndex];
            const TerrainLayer& bottomLayer = mapData.terrainLayers[bottomLayerIndex];
            if (rope.topLayerId == rope.bottomLayerId)
            {
                issues.push_back({ ValidationIssue::Warning, u8"同じレイヤー同士を接続するロープがあります。" });
            }
            if (topLayer.layerDepth >= bottomLayer.layerDepth)
            {
                issues.push_back({ ValidationIssue::Warning, u8"ロープの上層 / 下層深度関係が不自然です。" });
            }
            if (IsBlockedCell(topLayer, topCellX, topCellZ) || IsBlockedCell(bottomLayer, bottomCellX, bottomCellZ))
            {
                issues.push_back({ ValidationIssue::Error, u8"ロープ接続先に歩行不可セルがあります。" });
            }
            if (!IsRopeAnchorCell(topLayer, topCellX, topCellZ) || !IsRopeAnchorCell(bottomLayer, bottomCellX, bottomCellZ))
            {
                issues.push_back({ ValidationIssue::Warning, u8"ロープ接続先にロープアンカー属性が付いていません。" });
            }
        }

        for (const MiningPoint& point : mapData.miningPoints)
        {
            int layerIndex = -1;
            int cellX = -1;
            int cellZ = -1;
            if (!TryGetLayerAndCell(mapData, point.layerId, point.xz, layerIndex, cellX, cellZ))
            {
                issues.push_back({ ValidationIssue::Error, u8"採掘ポイントが有効なセル範囲外にあります。" });
                continue;
            }

            hasEnabledMiningPoint |= point.enabled;
            const TerrainLayer& layer = mapData.terrainLayers[layerIndex];
            if (IsBlockedCell(layer, cellX, cellZ))
            {
                issues.push_back({ ValidationIssue::Error, u8"採掘ポイントが歩行不可セル上にあります。" });
            }
            if (IsHazardCell(layer, cellX, cellZ))
            {
                issues.push_back({ ValidationIssue::Warning, u8"採掘ポイントが危険地形セル上にあります。" });
            }
        }

        if (!hasEnabledMiningPoint)
        {
            issues.push_back({ ValidationIssue::Warning, u8"有効な採掘ポイントが 1 つもありません。" });
        }

        const std::set<ReachableCellKey> reachableCells = BuildReachableCellSet(mapData);
        if (reachableCells.empty())
        {
            issues.push_back({ ValidationIssue::Error, u8"プレイヤー開始地点から移動可能なセルを構築できません。" });
            return issues;
        }

        if (returnLayerIndex >= 0)
        {
            const ReachableCellKey returnKey{ returnLayerIndex, returnCellX, returnCellZ };
            if (reachableCells.find(returnKey) == reachableCells.end())
            {
                issues.push_back({ ValidationIssue::Error, u8"プレイヤー開始地点から帰還地点へ到達できません。" });
            }
        }

        int reachableMiningPointCount = 0;
        for (const MiningPoint& point : mapData.miningPoints)
        {
            if (!point.enabled)
            {
                continue;
            }

            int layerIndex = -1;
            int cellX = -1;
            int cellZ = -1;
            if (!TryGetLayerAndCell(mapData, point.layerId, point.xz, layerIndex, cellX, cellZ))
            {
                continue;
            }

            const ReachableCellKey miningKey{ layerIndex, cellX, cellZ };
            if (reachableCells.find(miningKey) != reachableCells.end())
            {
                ++reachableMiningPointCount;
            }
        }

        if (hasEnabledMiningPoint && reachableMiningPointCount == 0)
        {
            issues.push_back({ ValidationIssue::Warning, u8"プレイヤー開始地点から到達可能な有効採掘ポイントがありません。" });
        }

        return issues;
    }
    MapData CreateDefaultMap()
    {
        MapData mapData;

        TerrainLayer upperLayer;
        upperLayer.id = 0;
        upperLayer.center = { 0.0f, 0.0f };
        upperLayer.layerDepth = 0.0f;
        upperLayer.gridWidth = 64;
        upperLayer.gridHeight = 64;
        upperLayer.cellSize = 1.0f;
        upperLayer.groundTextureId = 0;
        upperLayer.visible = true;
        upperLayer.locked = false;
        EnsureLayerHeights(upperLayer);
        EnsureLayerCellAttributes(upperLayer);

        TerrainLayer lowerLayer;
        lowerLayer.id = 1;
        lowerLayer.center = { 10.0f, 8.0f };
        lowerLayer.layerDepth = 4.0f;
        lowerLayer.gridWidth = 48;
        lowerLayer.gridHeight = 48;
        lowerLayer.cellSize = 1.0f;
        lowerLayer.groundTextureId = 2;
        lowerLayer.visible = true;
        lowerLayer.locked = false;
        EnsureLayerHeights(lowerLayer);
        EnsureLayerCellAttributes(lowerLayer);

        for (int x = 0; x < upperLayer.gridWidth - 1; ++x)
        {
            SetCellAttributeFlags(upperLayer, x, 0, CellAttributeCliffEdge);
            SetCellAttributeFlags(upperLayer, x, upperLayer.gridHeight - 2, CellAttributeCliffEdge);
        }
        for (int z = 0; z < upperLayer.gridHeight - 1; ++z)
        {
            SetCellAttributeFlags(upperLayer, 0, z, CellAttributeCliffEdge);
            SetCellAttributeFlags(upperLayer, upperLayer.gridWidth - 2, z, CellAttributeCliffEdge);
        }

        mapData.terrainLayers.push_back(upperLayer);
        mapData.terrainLayers.push_back(lowerLayer);

        mapData.playerStartPoint = { { 0.0f, 0.0f }, 0 };
        mapData.returnPoint = { { 0.0f, 0.0f }, 0 };
        mapData.autoFallStartHeight = 0.90f;

        mapData.ropes.push_back({ { 7.0f, 9.0f }, 0, 1 });

        const Vec2 miningPositions[10] =
        {
            { -6.0f, 4.0f }, { 5.5f, 6.0f }, { 9.0f, -3.0f }, { -11.0f, -6.0f }, { 15.0f, 5.0f },
            { -18.0f, 8.0f }, { 18.0f, -11.0f }, { -4.0f, -15.0f }, { 3.0f, 18.0f }, { -16.0f, -17.0f }
        };

        for (int i = 0; i < 10; ++i)
        {
            MiningPoint point;
            point.xz = miningPositions[i];
            point.layerId = 0;
            point.visualType = i % 4;
            point.discovered = i < 3;
            point.enabled = true;
            point.respawnCandidate = i >= 5;
            point.relicName = "relic_" + std::to_string(i);
            mapData.miningPoints.push_back(point);
        }

        return mapData;
    }

    bool SaveMap(const wchar_t* path, const MapData& mapData, std::string* errorMessage)
    {
        EnsureMapDirectories();

        std::ostringstream out;
        out << "{\n";
        AppendIndent(out, 1);
        out << "\"version\": " << kCurrentMapVersion << ",\n";
        AppendLayerPoint(out, "playerStartPoint", mapData.playerStartPoint, 1);
        out << ",\n";
        AppendLayerPoint(out, "returnPoint", mapData.returnPoint, 1);
        out << ",\n";
        AppendIndent(out, 1);
        out << "\"autoFallStartHeight\": " << mapData.autoFallStartHeight << ",\n";

        AppendIndent(out, 1);
        out << "\"layers\": [\n";
        for (int i = 0; i < static_cast<int>(mapData.terrainLayers.size()); ++i)
        {
            TerrainLayer layer = mapData.terrainLayers[i];
            EnsureLayerHeights(layer);
            EnsureLayerVertexEnabled(layer);
            EnsureLayerCellAttributes(layer);

            AppendIndent(out, 2);
            out << "{\n";
            AppendIndent(out, 3);
            out << "\"id\": " << layer.id << ",\n";
            AppendIndent(out, 3);
            out << "\"center\": { \"x\": " << layer.center.x << ", \"z\": " << layer.center.z << " },\n";
            AppendIndent(out, 3);
            out << "\"layerDepth\": " << layer.layerDepth << ",\n";
            AppendIndent(out, 3);
            out << "\"gridWidth\": " << layer.gridWidth << ",\n";
            AppendIndent(out, 3);
            out << "\"gridHeight\": " << layer.gridHeight << ",\n";
            AppendIndent(out, 3);
            out << "\"cellSize\": " << layer.cellSize << ",\n";
            AppendIndent(out, 3);
            out << "\"groundTextureId\": " << layer.groundTextureId << ",\n";
            AppendIndent(out, 3);
            out << "\"visible\": " << (layer.visible ? "true" : "false") << ",\n";
            AppendIndent(out, 3);
            out << "\"locked\": " << (layer.locked ? "true" : "false") << ",\n";
            AppendIndent(out, 3);
            out << "\"heights\": [";
            for (int h = 0; h < static_cast<int>(layer.heights.size()); ++h)
            {
                if (h > 0) out << ", ";
                out << layer.heights[h];
            }
            out << "],\n";
            AppendIndent(out, 3);
            out << "\"vertexEnabled\": [";
            for (int v = 0; v < static_cast<int>(layer.vertexEnabled.size()); ++v)
            {
                if (v > 0) out << ", ";
                out << static_cast<int>(layer.vertexEnabled[v]);
            }
            out << "],\n";
            AppendIndent(out, 3);
            out << "\"cellAttributeFlags\": [";
            for (int c = 0; c < static_cast<int>(layer.cellAttributeFlags.size()); ++c)
            {
                if (c > 0) out << ", ";
                out << layer.cellAttributeFlags[c];
            }
            out << "]\n";
            AppendIndent(out, 2);
            out << "}";
            out << (i + 1 < static_cast<int>(mapData.terrainLayers.size()) ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "],\n";

        AppendIndent(out, 1);
        out << "\"ropes\": [\n";
        for (int i = 0; i < static_cast<int>(mapData.ropes.size()); ++i)
        {
            const RopePoint& rope = mapData.ropes[i];
            AppendIndent(out, 2);
            out << "{ \"x\": " << rope.xz.x << ", \"z\": " << rope.xz.z
                << ", \"topLayerId\": " << rope.topLayerId
                << ", \"bottomLayerId\": " << rope.bottomLayerId << " }";
            out << (i + 1 < static_cast<int>(mapData.ropes.size()) ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "],\n";

        AppendIndent(out, 1);
        out << "\"miningPoints\": [\n";
        for (int i = 0; i < static_cast<int>(mapData.miningPoints.size()); ++i)
        {
            const MiningPoint& point = mapData.miningPoints[i];
            AppendIndent(out, 2);
            out << "{ \"x\": " << point.xz.x
                << ", \"z\": " << point.xz.z
                << ", \"layerId\": " << point.layerId
                << ", \"visualType\": " << point.visualType
                << ", \"discovered\": " << (point.discovered ? "true" : "false")
                << ", \"enabled\": " << (point.enabled ? "true" : "false")
                << ", \"respawnCandidate\": " << (point.respawnCandidate ? "true" : "false")
                << ", \"relicName\": \"" << EscapeJsonString(point.relicName) << "\" }";
            out << (i + 1 < static_cast<int>(mapData.miningPoints.size()) ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "]\n";
        out << "}\n";

        if (!WriteWholeFile(path, out.str()))
        {
            if (errorMessage) *errorMessage = "failed to open map file for writing";
            return false;
        }
        return true;
    }

    bool LoadMap(const wchar_t* path, MapData& outMapData, std::string* errorMessage)
    {
        std::string jsonText;
        if (!ReadWholeFile(path, jsonText))
        {
            if (errorMessage) *errorMessage = "failed to open map file for reading";
            return false;
        }

        JsonValue rootValue;
        JsonParser parser(jsonText);
        if (!parser.Parse(rootValue))
        {
            if (errorMessage) *errorMessage = "failed to parse map json";
            return false;
        }

        if (rootValue.type != JsonValue::TypeObject)
        {
            if (errorMessage) *errorMessage = "map root is not object";
            return false;
        }

        MapData loadedMap;
        const JsonValue* layersValue = nullptr;
        if (!GetObjectValue<JsonValue>(rootValue, "layers", layersValue) || layersValue->type != JsonValue::TypeArray)
        {
            if (errorMessage) *errorMessage = "layers array is missing";
            return false;
        }

        for (const JsonValue& layerValue : layersValue->arrayValue)
        {
            if (layerValue.type != JsonValue::TypeObject) continue;

            TerrainLayer layer;
            double number = 0.0;
            bool boolValue = false;
            GetNumber(layerValue, "id", number); layer.id = static_cast<int>(number);
            GetNumber(layerValue, "layerDepth", number); layer.layerDepth = static_cast<float>(number);
            GetNumber(layerValue, "gridWidth", number); layer.gridWidth = static_cast<int>(number);
            GetNumber(layerValue, "gridHeight", number); layer.gridHeight = static_cast<int>(number);
            GetNumber(layerValue, "cellSize", number); layer.cellSize = static_cast<float>(number);
            GetNumber(layerValue, "groundTextureId", number); layer.groundTextureId = static_cast<int>(number);
            if (GetBool(layerValue, "visible", boolValue)) layer.visible = boolValue;
            if (GetBool(layerValue, "locked", boolValue)) layer.locked = boolValue;

            const JsonValue* centerValue = nullptr;
            if (GetObjectValue<JsonValue>(layerValue, "center", centerValue) && centerValue->type == JsonValue::TypeObject)
            {
                GetNumber(*centerValue, "x", number); layer.center.x = static_cast<float>(number);
                GetNumber(*centerValue, "z", number); layer.center.z = static_cast<float>(number);
            }

            const JsonValue* heightsValue = nullptr;
            if (GetObjectValue<JsonValue>(layerValue, "heights", heightsValue) && heightsValue->type == JsonValue::TypeArray)
            {
                layer.heights.reserve(heightsValue->arrayValue.size());
                for (const JsonValue& heightValue : heightsValue->arrayValue)
                {
                    layer.heights.push_back(heightValue.type == JsonValue::TypeNumber ? static_cast<float>(heightValue.numberValue) : 0.0f);
                }
            }

            const JsonValue* cellFlagsValue = nullptr;
            if (GetObjectValue<JsonValue>(layerValue, "cellAttributeFlags", cellFlagsValue) && cellFlagsValue->type == JsonValue::TypeArray)
            {
                layer.cellAttributeFlags.reserve(cellFlagsValue->arrayValue.size());
                for (const JsonValue& cellFlagValue : cellFlagsValue->arrayValue)
                {
                    layer.cellAttributeFlags.push_back(cellFlagValue.type == JsonValue::TypeNumber ? static_cast<std::uint32_t>(cellFlagValue.numberValue) : CellAttributeNone);
                }
            }

            const JsonValue* vertexEnabledValue = nullptr;
            if (GetObjectValue<JsonValue>(layerValue, "vertexEnabled", vertexEnabledValue) && vertexEnabledValue->type == JsonValue::TypeArray)
            {
                layer.vertexEnabled.reserve(vertexEnabledValue->arrayValue.size());
                for (const JsonValue& enabledValue : vertexEnabledValue->arrayValue)
                {
                    layer.vertexEnabled.push_back((enabledValue.type == JsonValue::TypeNumber && enabledValue.numberValue == 0.0) ? 0u : 1u);
                }
            }

            EnsureLayerHeights(layer);
            EnsureLayerVertexEnabled(layer);
            EnsureLayerCellAttributes(layer);
            loadedMap.terrainLayers.push_back(layer);
        }

        if (loadedMap.terrainLayers.empty())
        {
            if (errorMessage) *errorMessage = "no terrain layers found";
            return false;
        }

        const int fallbackLayerId = loadedMap.terrainLayers.front().id;

        const JsonValue* startPointValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "playerStartPoint", startPointValue) && startPointValue->type == JsonValue::TypeObject)
        {
            double number = 0.0;
            GetNumber(*startPointValue, "x", number); loadedMap.playerStartPoint.xz.x = static_cast<float>(number);
            GetNumber(*startPointValue, "z", number); loadedMap.playerStartPoint.xz.z = static_cast<float>(number);
            GetNumber(*startPointValue, "layerId", number); loadedMap.playerStartPoint.layerId = static_cast<int>(number);
        }
        else
        {
            loadedMap.playerStartPoint = { { 0.0f, 0.0f }, fallbackLayerId };
        }

        const JsonValue* returnPointValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "returnPoint", returnPointValue) && returnPointValue->type == JsonValue::TypeObject)
        {
            double number = 0.0;
            GetNumber(*returnPointValue, "x", number); loadedMap.returnPoint.xz.x = static_cast<float>(number);
            GetNumber(*returnPointValue, "z", number); loadedMap.returnPoint.xz.z = static_cast<float>(number);
            GetNumber(*returnPointValue, "layerId", number); loadedMap.returnPoint.layerId = static_cast<int>(number);
        }
        else
        {
            loadedMap.returnPoint = loadedMap.playerStartPoint;
        }

        double autoFallStartHeight = 0.0;
        if (GetNumber(rootValue, "autoFallStartHeight", autoFallStartHeight))
        {
            loadedMap.autoFallStartHeight = static_cast<float>(autoFallStartHeight);
        }

        const JsonValue* ropesValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "ropes", ropesValue) && ropesValue->type == JsonValue::TypeArray)
        {
            for (const JsonValue& ropeValue : ropesValue->arrayValue)
            {
                if (ropeValue.type != JsonValue::TypeObject) continue;

                RopePoint rope;
                double number = 0.0;
                GetNumber(ropeValue, "x", number); rope.xz.x = static_cast<float>(number);
                GetNumber(ropeValue, "z", number); rope.xz.z = static_cast<float>(number);
                GetNumber(ropeValue, "topLayerId", number); rope.topLayerId = static_cast<int>(number);
                GetNumber(ropeValue, "bottomLayerId", number); rope.bottomLayerId = static_cast<int>(number);
                loadedMap.ropes.push_back(rope);
            }
        }

        const JsonValue* miningPointsValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "miningPoints", miningPointsValue) && miningPointsValue->type == JsonValue::TypeArray)
        {
            for (const JsonValue& pointValue : miningPointsValue->arrayValue)
            {
                if (pointValue.type != JsonValue::TypeObject) continue;

                MiningPoint point;
                double number = 0.0;
                bool boolValue = false;
                GetNumber(pointValue, "x", number); point.xz.x = static_cast<float>(number);
                GetNumber(pointValue, "z", number); point.xz.z = static_cast<float>(number);
                GetNumber(pointValue, "layerId", number); point.layerId = static_cast<int>(number);
                GetNumber(pointValue, "visualType", number); point.visualType = static_cast<int>(number);
                if (GetBool(pointValue, "discovered", boolValue)) point.discovered = boolValue;
                if (GetBool(pointValue, "enabled", boolValue)) point.enabled = boolValue;
                if (GetBool(pointValue, "respawnCandidate", boolValue)) point.respawnCandidate = boolValue;
                GetString(pointValue, "relicName", point.relicName);
                loadedMap.miningPoints.push_back(point);
            }
        }

        NormalizeLayerPoint(loadedMap.playerStartPoint, loadedMap, fallbackLayerId);
        NormalizeLayerPoint(loadedMap.returnPoint, loadedMap, fallbackLayerId);

        for (RopePoint& rope : loadedMap.ropes)
        {
            if (FindLayerIndexById(loadedMap, rope.topLayerId) < 0) rope.topLayerId = fallbackLayerId;
            if (FindLayerIndexById(loadedMap, rope.bottomLayerId) < 0) rope.bottomLayerId = fallbackLayerId;
        }
        for (MiningPoint& point : loadedMap.miningPoints)
        {
            if (FindLayerIndexById(loadedMap, point.layerId) < 0) point.layerId = fallbackLayerId;
        }

        outMapData = loadedMap;
        return true;
    }
}
