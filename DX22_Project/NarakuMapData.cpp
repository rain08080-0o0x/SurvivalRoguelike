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
#include <sstream>

namespace
{
    constexpr int kCurrentMapVersion = 1;
    const wchar_t* kDefaultMapPath = L"C:\\HAL\\個人制作\\NarakuProto\\Assets\\Maps\\FirstLayer.json";

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
        ::CreateDirectoryW(L"C:\\HAL\\個人制作\\NarakuProto\\Assets", nullptr);
        ::CreateDirectoryW(L"C:\\HAL\\個人制作\\NarakuProto\\Assets\\Maps", nullptr);
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

    int FindLayerIndexById(const NarakuMap::MapData& mapData, int layerId)
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
}

namespace NarakuMap
{
    const wchar_t* GetDefaultMapPath()
    {
        return kDefaultMapPath;
    }

    void EnsureLayerHeights(TerrainLayer& layer)
    {
        const int count = std::max(0, layer.gridWidth) * std::max(0, layer.gridHeight);
        if (static_cast<int>(layer.heights.size()) != count)
        {
            layer.heights.resize(count, 0.0f);
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
        EnsureLayerHeights(upperLayer);

        TerrainLayer lowerLayer;
        lowerLayer.id = 1;
        lowerLayer.center = { 10.0f, 8.0f };
        lowerLayer.layerDepth = 4.0f;
        lowerLayer.gridWidth = 48;
        lowerLayer.gridHeight = 48;
        lowerLayer.cellSize = 1.0f;
        lowerLayer.groundTextureId = 2;
        EnsureLayerHeights(lowerLayer);

        mapData.terrainLayers.push_back(upperLayer);
        mapData.terrainLayers.push_back(lowerLayer);

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

        AppendIndent(out, 1);
        out << "\"layers\": [\n";
        for (int i = 0; i < static_cast<int>(mapData.terrainLayers.size()); ++i)
        {
            const TerrainLayer& layer = mapData.terrainLayers[i];
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
            out << "\"heights\": [";
            for (int h = 0; h < static_cast<int>(layer.heights.size()); ++h)
            {
                if (h > 0) out << ", ";
                out << layer.heights[h];
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
            out << "{ \"x\": " << point.xz.x << ", \"z\": " << point.xz.z
                << ", \"layerId\": " << point.layerId
                << ", \"visualType\": " << point.visualType
                << ", \"discovered\": " << (point.discovered ? "true" : "false") << " }";
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
            GetNumber(layerValue, "id", number); layer.id = static_cast<int>(number);
            GetNumber(layerValue, "layerDepth", number); layer.layerDepth = static_cast<float>(number);
            GetNumber(layerValue, "gridWidth", number); layer.gridWidth = static_cast<int>(number);
            GetNumber(layerValue, "gridHeight", number); layer.gridHeight = static_cast<int>(number);
            GetNumber(layerValue, "cellSize", number); layer.cellSize = static_cast<float>(number);
            GetNumber(layerValue, "groundTextureId", number); layer.groundTextureId = static_cast<int>(number);

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

            EnsureLayerHeights(layer);
            loadedMap.terrainLayers.push_back(layer);
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
                loadedMap.miningPoints.push_back(point);
            }
        }

        // ロープや採掘ポイントが存在しない場合でも、最低限成立するマップに補正します。
        if (loadedMap.terrainLayers.empty())
        {
            if (errorMessage) *errorMessage = "no terrain layers found";
            return false;
        }

        // 不正なレイヤー参照は先頭レイヤーへ丸めます。
        const int fallbackLayerId = loadedMap.terrainLayers.front().id;
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
