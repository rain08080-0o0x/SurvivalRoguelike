#include "NarakuPieceData.h"

#include <Windows.h>
#undef max
#undef min

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace
{
    constexpr int kCurrentPieceVersion = 1;
    const wchar_t* kPieceRootDirectoryRelativePath = L"Assets\\Naraku\\Pieces";
    const wchar_t* kDraftDirectoryName = L"Drafts";
    const wchar_t* kCompletedDirectoryName = L"Completed";
    const wchar_t* kDefaultPieceFileName = L"default_piece.json";

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

    /**
     * @brief 16 進数文字を数値へ変換します。
     */
    int HexDigitValue(char c)
    {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    }

    /**
     * @brief Unicode コードポイントを UTF-8 として追記します。
     */
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

    /**
     * @brief 最小限の JSON パーサです。
     */
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
            if (!ParseValue(outValue))
            {
                return false;
            }

            SkipWs();
            return m_pos == m_text.size();
        }

    private:
        void SkipWs()
        {
            while (m_pos < m_text.size() &&
                   std::isspace(static_cast<unsigned char>(m_text[m_pos])) != 0)
            {
                ++m_pos;
            }
        }

        bool Match(const char* literal)
        {
            const size_t length = std::char_traits<char>::length(literal);
            if (m_text.compare(m_pos, length, literal) != 0)
            {
                return false;
            }

            m_pos += length;
            return true;
        }

        bool ParseValue(JsonValue& outValue)
        {
            SkipWs();
            if (m_pos >= m_text.size())
            {
                return false;
            }

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
            if (m_text[m_pos] != '{')
            {
                return false;
            }

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
                if (!ParseString(key))
                {
                    return false;
                }

                SkipWs();
                if (m_pos >= m_text.size() || m_text[m_pos] != ':')
                {
                    return false;
                }

                ++m_pos;
                JsonValue value;
                if (!ParseValue(value))
                {
                    return false;
                }

                outValue.objectValue[key] = value;
                SkipWs();
                if (m_pos >= m_text.size())
                {
                    return false;
                }

                if (m_text[m_pos] == '}')
                {
                    ++m_pos;
                    return true;
                }

                if (m_text[m_pos] != ',')
                {
                    return false;
                }

                ++m_pos;
            }

            return false;
        }

        bool ParseArray(JsonValue& outValue)
        {
            if (m_text[m_pos] != '[')
            {
                return false;
            }

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
                if (!ParseValue(value))
                {
                    return false;
                }

                outValue.arrayValue.push_back(value);
                SkipWs();
                if (m_pos >= m_text.size())
                {
                    return false;
                }

                if (m_text[m_pos] == ']')
                {
                    ++m_pos;
                    return true;
                }

                if (m_text[m_pos] != ',')
                {
                    return false;
                }

                ++m_pos;
            }

            return false;
        }

        bool ParseString(std::string& outText)
        {
            if (m_text[m_pos] != '"')
            {
                return false;
            }

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
                    if (m_pos >= m_text.size())
                    {
                        return false;
                    }

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
                        if (m_pos + 4 > m_text.size())
                        {
                            return false;
                        }

                        unsigned int codePoint = 0;
                        for (int i = 0; i < 4; ++i)
                        {
                            const int digit = HexDigitValue(m_text[m_pos + i]);
                            if (digit < 0)
                            {
                                return false;
                            }
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
            if (m_text[m_pos] == '-')
            {
                ++m_pos;
            }

            while (m_pos < m_text.size() &&
                   std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
            {
                ++m_pos;
            }

            if (m_pos < m_text.size() && m_text[m_pos] == '.')
            {
                ++m_pos;
                while (m_pos < m_text.size() &&
                       std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
                {
                    ++m_pos;
                }
            }

            if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E'))
            {
                ++m_pos;
                if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-'))
                {
                    ++m_pos;
                }

                while (m_pos < m_text.size() &&
                       std::isdigit(static_cast<unsigned char>(m_text[m_pos])) != 0)
                {
                    ++m_pos;
                }
            }

            outNumber = std::strtod(m_text.c_str() + start, nullptr);
            return true;
        }

        const std::string& m_text;
        size_t m_pos = 0;
    };

    /**
     * @brief ファイル全体を読込します。
     */
    bool ReadWholeFile(const wchar_t* path, std::string& outText)
    {
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, path, L"rb") != 0 || fp == nullptr)
        {
            return false;
        }

        std::fseek(fp, 0, SEEK_END);
        const long fileSize = std::ftell(fp);
        std::fseek(fp, 0, SEEK_SET);
        outText.resize(fileSize > 0 ? static_cast<size_t>(fileSize) : 0u);
        if (!outText.empty())
        {
            std::fread(&outText[0], 1, outText.size(), fp);
        }
        std::fclose(fp);
        if (outText.size() >= 3 &&
            static_cast<unsigned char>(outText[0]) == 0xEF &&
            static_cast<unsigned char>(outText[1]) == 0xBB &&
            static_cast<unsigned char>(outText[2]) == 0xBF)
        {
            outText.erase(0, 3);
        }
        return true;
    }

    /**
     * @brief ファイル全体を書込します。
     */
    bool WriteWholeFile(const wchar_t* path, const std::string& text)
    {
        FILE* fp = nullptr;
        if (_wfopen_s(&fp, path, L"wb") != 0 || fp == nullptr)
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

    /**
     * @brief 現在のローカル日時を保存用文字列へ整形します。
     * @return `YYYY/MM/DD HH:MM:SS` 形式の日時文字列です。
     */
    std::string BuildCurrentTimestamp()
    {
        std::time_t now = std::time(nullptr);
        std::tm localTime = {};
        localtime_s(&localTime, &now);

        char buffer[20] = {};
        std::strftime(buffer, sizeof(buffer), "%Y/%m/%d %H:%M:%S", &localTime);
        return std::string(buffer);
    }

    /** @brief 相対パスを現在の実行基準ディレクトリから解決できる形で返します。 */
    std::wstring ResolveProjectRelativePath(const std::wstring& relativePath)
    {
        return relativePath;
    }

    /**
     * @brief 絶対パスかどうかを返します。
     */
    bool IsAbsolutePath(const std::wstring& path)
    {
        if (path.size() >= 2 && path[1] == L':')
        {
            return true;
        }

        return path.size() >= 2 &&
               ((path[0] == L'\\' && path[1] == L'\\') ||
                (path[0] == L'/' && path[1] == L'/'));
    }

    /**
     * @brief ファイルの親ディレクトリ階層を作成します。
     */
    bool EnsureDirectoryTreeForFile(const wchar_t* filePath)
    {
        if (filePath == nullptr || filePath[0] == L'\0')
        {
            return false;
        }

        std::wstring currentPath(filePath);
        const size_t lastSlash = currentPath.find_last_of(L"\\/");
        if (lastSlash == std::wstring::npos)
        {
            return false;
        }

        currentPath.resize(lastSlash);
        if (currentPath.empty())
        {
            return false;
        }

        std::wstring partialPath;
        if (currentPath.size() >= 2 && currentPath[1] == L':')
        {
            partialPath.assign(currentPath.begin(), currentPath.begin() + 2);
        }

        size_t segmentStart = partialPath.empty() ? 0u : 2u;
        while (segmentStart < currentPath.size())
        {
            while (segmentStart < currentPath.size() &&
                   (currentPath[segmentStart] == L'\\' || currentPath[segmentStart] == L'/'))
            {
                partialPath.push_back(L'\\');
                ++segmentStart;
            }

            size_t segmentEnd = segmentStart;
            while (segmentEnd < currentPath.size() &&
                   currentPath[segmentEnd] != L'\\' &&
                   currentPath[segmentEnd] != L'/')
            {
                ++segmentEnd;
            }

            if (segmentEnd > segmentStart)
            {
                partialPath.append(currentPath.substr(segmentStart, segmentEnd - segmentStart));
                ::CreateDirectoryW(partialPath.c_str(), nullptr);
            }

            segmentStart = segmentEnd;
        }

        return true;
    }

    /**
     * @brief JSON 出力用のインデントを書込みます。
     */
    void AppendIndent(std::ostringstream& out, int indent)
    {
        for (int i = 0; i < indent; ++i)
        {
            out << "  ";
        }
    }

    /**
     * @brief JSON 文字列をエスケープします。
     */
    std::string EscapeJsonString(const std::string& text)
    {
        std::string escaped;
        escaped.reserve(text.size());
        for (char c : text)
        {
            switch (c)
            {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(c); break;
            }
        }
        return escaped;
    }

    template<typename T>
    bool GetObjectValue(const JsonValue& objectValue, const char* key, const T*& outValue);

    template<>
    bool GetObjectValue<JsonValue>(const JsonValue& objectValue, const char* key, const JsonValue*& outValue)
    {
        if (objectValue.type != JsonValue::TypeObject)
        {
            return false;
        }

        const auto it = objectValue.objectValue.find(key);
        if (it == objectValue.objectValue.end())
        {
            return false;
        }

        outValue = &it->second;
        return true;
    }

    /**
     * @brief JSON オブジェクトから数値を取得します。
     */
    bool GetNumber(const JsonValue& objectValue, const char* key, double& outNumber)
    {
        const JsonValue* value = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, value) || value->type != JsonValue::TypeNumber)
        {
            return false;
        }

        outNumber = value->numberValue;
        return true;
    }

    /**
     * @brief JSON オブジェクトから真偽値を取得します。
     */
    bool GetBool(const JsonValue& objectValue, const char* key, bool& outBool)
    {
        const JsonValue* value = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, value) || value->type != JsonValue::TypeBool)
        {
            return false;
        }

        outBool = value->boolValue;
        return true;
    }

    /**
     * @brief JSON オブジェクトから文字列を取得します。
     */
    bool GetString(const JsonValue& objectValue, const char* key, std::string& outText)
    {
        const JsonValue* value = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, value) || value->type != JsonValue::TypeString)
        {
            return false;
        }

        outText = value->stringValue;
        return true;
    }

    /**
     * @brief セル座標が範囲内かどうかを返します。
     */
    bool IsCellInRange(const NarakuPiece::PieceData& data, const NarakuPiece::GridPoint& cell)
    {
        return cell.x >= 0 &&
               cell.z >= 0 &&
               cell.x < data.gridWidth &&
               cell.z < data.gridDepth;
    }

    /**
     * @brief セル配列の期待要素数を取得します。
     */
    size_t GetExpectedCellCount(const NarakuPiece::PieceData& data)
    {
        return static_cast<size_t>(std::max(0, data.gridWidth - 1)) *
               static_cast<size_t>(std::max(0, data.gridDepth - 1));
    }

    /**
     * @brief レイヤーディレクトリ名を `Layer{番号}` 形式で生成します。
     */
    std::wstring FormatLayerDirectoryName(int abyssLayer)
    {
        wchar_t buffer[32] = {};
        const int safeLayer = std::max(1, abyssLayer);
        _snwprintf_s(buffer, _TRUNCATE, L"Layer%d", safeLayer);
        return std::wstring(buffer);
    }

    /**
     * @brief 小ステージ保存ルート配下の相対ディレクトリを組み立てます。
     */
    std::wstring BuildDirectoryRelativePath(int abyssLayer, const wchar_t* leafDirectory)
    {
        std::wstring relativePath(kPieceRootDirectoryRelativePath);
        relativePath += L"\\";
        relativePath += FormatLayerDirectoryName(abyssLayer);
        relativePath += L"\\";
        relativePath += leafDirectory;
        return relativePath;
    }

    /**
     * @brief パス区切りを除いたファイル名部分だけを返します。
     */
    std::wstring GetFileNamePart(const std::wstring& path)
    {
        const size_t slashPos = path.find_last_of(L"\\/");
        if (slashPos == std::wstring::npos)
        {
            return path;
        }

        return path.substr(slashPos + 1);
    }

    /**
     * @brief ディレクトリ名 1 セグメントを安全化します。
     */
    std::wstring SanitizeDirectorySegment(std::wstring segment)
    {
        for (wchar_t& ch : segment)
        {
            const bool invalid =
                ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' ||
                ch == L'*' || ch < 0x20;
            if (invalid)
            {
                ch = L'_';
            }
        }

        if (segment.empty() || segment == L"." || segment == L"..")
        {
            return L"_";
        }

        return segment;
    }

    /**
     * @brief 保存用の相対パスを安全化します。
     */
    std::wstring NormalizeRelativePiecePath(const std::wstring& path)
    {
        if (path.empty())
        {
            return BuildDirectoryRelativePath(1, kDraftDirectoryName) + L"\\" + kDefaultPieceFileName;
        }

        const std::wstring fileName = NarakuPiece::NormalizePieceFileName(GetFileNamePart(path));
        const std::wstring normalizedPath = path;
        const size_t slashPos = normalizedPath.find_last_of(L"\\/");
        if (slashPos == std::wstring::npos)
        {
            return fileName;
        }

        std::wstring directory = normalizedPath.substr(0, slashPos);
        std::replace(directory.begin(), directory.end(), L'/', L'\\');
        std::wstringstream stream(directory);
        std::wstring segment;
        std::wstring safeDirectory;
        while (std::getline(stream, segment, L'\\'))
        {
            if (segment.empty())
            {
                continue;
            }

            const std::wstring sanitized = SanitizeDirectorySegment(segment);
            if (!safeDirectory.empty())
            {
                safeDirectory += L"\\";
            }
            safeDirectory += sanitized;
        }

        return safeDirectory.empty() ? fileName : safeDirectory + L"\\" + fileName;
    }

    /**
     * @brief JSON から GridPoint を読込します。
     */
    bool LoadGridPoint(const JsonValue& objectValue, const char* key, NarakuPiece::GridPoint& outPoint)
    {
        const JsonValue* pointValue = nullptr;
        if (!GetObjectValue<JsonValue>(objectValue, key, pointValue) || pointValue->type != JsonValue::TypeObject)
        {
            return false;
        }

        double number = 0.0;
        if (GetNumber(*pointValue, "x", number))
        {
            outPoint.x = static_cast<int>(number);
        }
        if (GetNumber(*pointValue, "z", number))
        {
            outPoint.z = static_cast<int>(number);
        }
        return true;
    }

    /**
     * @brief 検証メッセージを追加します。
     */
    void AddIssue(std::vector<NarakuPiece::ValidationIssue>& issues,
                  NarakuPiece::ValidationIssue::Severity severity,
                  const char* message)
    {
        NarakuPiece::ValidationIssue issue;
        issue.severity = severity;
        issue.message = message;
        issues.push_back(issue);
    }
}

namespace NarakuPiece
{
    const char* ToString(SizePreset value)
    {
        switch (value)
        {
        case SizePreset::Size8x8: return "8x8";
        case SizePreset::Size16x16: return "16x16";
        case SizePreset::Size24x24: return "24x24";
        case SizePreset::Size32x32: return "32x32";
        default: return "16x16";
        }
    }

    const char* ToString(StageRole value)
    {
        switch (value)
        {
        case StageRole::Normal: return "normal";
        case StageRole::StartReturn: return "start_return";
        case StageRole::Base: return "base";
        case StageRole::Relay: return "relay";
        default: return "normal";
        }
    }

    const char* ToString(StageCategory value)
    {
        switch (value)
        {
        case StageCategory::PlainHigh: return "plain_high";
        case StageCategory::PlainLow: return "plain_low";
        case StageCategory::Cliff: return "cliff";
        case StageCategory::HeightHigh: return "height_high";
        case StageCategory::HeightLow: return "height_low";
        case StageCategory::Water: return "water";
        case StageCategory::Blocked: return "blocked";
        default: return "plain_low";
        }
    }

    const char* ToString(WaterDepth value)
    {
        switch (value)
        {
        case WaterDepth::Puddle: return "puddle";
        case WaterDepth::Pond: return "pond";
        case WaterDepth::Lake: return "lake";
        default: return "none";
        }
    }

    const char* ToString(LayerTransitionRole value)
    {
        switch (value)
        {
        case LayerTransitionRole::Entry: return "entry";
        case LayerTransitionRole::Exit: return "exit";
        default: return "none";
        }
    }

    const char* ToString(SublayerTag value)
    {
        switch (value)
        {
        case SublayerTag::Upper: return "upper";
        case SublayerTag::Middle: return "middle";
        case SublayerTag::Lower: return "lower";
        default: return "none";
        }
    }

    const char* ToString(BaseType value)
    {
        switch (value)
        {
        case BaseType::SecondBase: return "second_base";
        case BaseType::ForwardBase: return "forward_base";
        default: return "none";
        }
    }

    const char* ToString(SurfaceFacilityType value)
    {
        switch (value)
        {
        case SurfaceFacilityType::Home: return "home";
        case SurfaceFacilityType::Shop: return "shop";
        case SurfaceFacilityType::Armory: return "armory";
        case SurfaceFacilityType::RestaurantQuestDesk: return "restaurant_quest_desk";
        case SurfaceFacilityType::AbyssEntrance: return "abyss_entrance";
        default: return "none";
        }
    }

    const char* ToString(Direction value)
    {
        switch (value)
        {
        case Direction::North: return "north";
        case Direction::South: return "south";
        case Direction::East: return "east";
        case Direction::West: return "west";
        default: return "south";
        }
    }

    bool TryParseSizePreset(const std::string& text, SizePreset& out)
    {
        if (text == "8x8")
        {
            out = SizePreset::Size8x8;
            return true;
        }
        if (text == "16x16")
        {
            out = SizePreset::Size16x16;
            return true;
        }
        if (text == "24x24")
        {
            out = SizePreset::Size24x24;
            return true;
        }
        if (text == "32x32")
        {
            out = SizePreset::Size32x32;
            return true;
        }
        return false;
    }

    bool TryParseStageRole(const std::string& text, StageRole& out)
    {
        if (text == "normal")
        {
            out = StageRole::Normal;
            return true;
        }
        if (text == "start_return")
        {
            out = StageRole::StartReturn;
            return true;
        }
        if (text == "base")
        {
            out = StageRole::Base;
            return true;
        }
        if (text == "relay")
        {
            out = StageRole::Relay;
            return true;
        }
        return false;
    }

    bool TryParseStageCategory(const std::string& text, StageCategory& out)
    {
        if (text == "plain_high")
        {
            out = StageCategory::PlainHigh;
            return true;
        }
        if (text == "plain_low")
        {
            out = StageCategory::PlainLow;
            return true;
        }
        if (text == "cliff")
        {
            out = StageCategory::Cliff;
            return true;
        }
        if (text == "height_high")
        {
            out = StageCategory::HeightHigh;
            return true;
        }
        if (text == "height_low")
        {
            out = StageCategory::HeightLow;
            return true;
        }
        if (text == "water")
        {
            out = StageCategory::Water;
            return true;
        }
        if (text == "blocked")
        {
            out = StageCategory::Blocked;
            return true;
        }
        return false;
    }

    bool TryParseWaterDepth(const std::string& text, WaterDepth& out)
    {
        if (text == "none") { out = WaterDepth::None; return true; }
        if (text == "puddle") { out = WaterDepth::Puddle; return true; }
        if (text == "pond") { out = WaterDepth::Pond; return true; }
        if (text == "lake") { out = WaterDepth::Lake; return true; }
        return false;
    }

    bool TryParseLayerTransitionRole(const std::string& text, LayerTransitionRole& out)
    {
        if (text == "none") { out = LayerTransitionRole::None; return true; }
        if (text == "entry") { out = LayerTransitionRole::Entry; return true; }
        if (text == "exit") { out = LayerTransitionRole::Exit; return true; }
        return false;
    }

    bool TryParseSublayerTag(const std::string& text, SublayerTag& out)
    {
        if (text == "none") { out = SublayerTag::None; return true; }
        if (text == "upper") { out = SublayerTag::Upper; return true; }
        if (text == "middle") { out = SublayerTag::Middle; return true; }
        if (text == "lower") { out = SublayerTag::Lower; return true; }
        return false;
    }

    bool TryParseBaseType(const std::string& text, BaseType& out)
    {
        if (text == "none") { out = BaseType::None; return true; }
        if (text == "second_base") { out = BaseType::SecondBase; return true; }
        if (text == "forward_base") { out = BaseType::ForwardBase; return true; }
        return false;
    }

    bool TryParseSurfaceFacilityType(const std::string& text, SurfaceFacilityType& out)
    {
        if (text == "none") { out = SurfaceFacilityType::None; return true; }
        if (text == "home") { out = SurfaceFacilityType::Home; return true; }
        if (text == "shop") { out = SurfaceFacilityType::Shop; return true; }
        if (text == "armory") { out = SurfaceFacilityType::Armory; return true; }
        if (text == "restaurant_quest_desk") { out = SurfaceFacilityType::RestaurantQuestDesk; return true; }
        if (text == "abyss_entrance") { out = SurfaceFacilityType::AbyssEntrance; return true; }
        return false;
    }

    bool TryParseDirection(const std::string& text, Direction& out)
    {
        if (text == "north")
        {
            out = Direction::North;
            return true;
        }
        if (text == "south")
        {
            out = Direction::South;
            return true;
        }
        if (text == "east")
        {
            out = Direction::East;
            return true;
        }
        if (text == "west")
        {
            out = Direction::West;
            return true;
        }
        return false;
    }

    int GetGridSize(SizePreset preset)
    {
        switch (preset)
        {
        case SizePreset::Size8x8: return 8;
        case SizePreset::Size16x16: return 16;
        case SizePreset::Size24x24: return 24;
        case SizePreset::Size32x32: return 32;
        default: return 16;
        }
    }

    PieceData CreateDefaultPiece(SizePreset preset)
    {
        PieceData data;
        const int gridSize = GetGridSize(preset);
        data.version = kCurrentPieceVersion;
        data.id = "piece_0001";
        data.displayName = data.id;
        data.abyssLayer = 0;
        data.sublayerTag = SublayerTag::None;
        data.sizePreset = preset;
        data.gridWidth = gridSize;
        data.gridDepth = gridSize;
        data.cellSize = 2.0f;
        data.stageRole = StageRole::Normal;
        data.stageCategory = StageCategory::PlainLow;
        data.edgeCategories = {
            StageCategory::PlainLow,
            StageCategory::PlainLow,
            StageCategory::PlainLow,
            StageCategory::PlainLow
        };
        data.lockedEdges = { true, true, true, true };
        data.heights.resize(static_cast<size_t>(gridSize * gridSize), 0.0f);
        data.cells.resize(GetExpectedCellCount(data));
        data.rope.enabled = false;
        data.baseType = BaseType::None;
        data.isSurface = preset == SizePreset::Size8x8;
        data.startReturnCandidate.enabled = false;
        data.startReturnCandidate.facing = Direction::South;
        return data;
    }

    std::vector<ValidationIssue> ValidatePieceData(const PieceData& data)
    {
        std::vector<ValidationIssue> issues;
        const int expectedGridSize = GetGridSize(data.sizePreset);

        if (data.id.empty())
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "id が空です。");
        }
        if (data.displayName.empty())
        {
            AddIssue(issues, ValidationIssue::Severity::Warning, "displayName が空です。");
        }
        if (data.abyssLayer < 0 || data.abyssLayer > 7)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "abyssLayer は 0（指定なし）から 7 の範囲である必要があります。");
        }
        if (data.gridWidth != expectedGridSize || data.gridDepth != expectedGridSize)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "gridWidth / gridDepth が sizePreset と一致していません。");
        }
        if (data.gridWidth <= 0 || data.gridDepth <= 0)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "gridWidth / gridDepth は正の値である必要があります。");
        }

        const size_t expectedHeightCount =
            static_cast<size_t>(std::max(0, data.gridWidth)) *
            static_cast<size_t>(std::max(0, data.gridDepth));
        if (data.heights.size() != expectedHeightCount)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "heights の要素数が gridWidth * gridDepth と一致していません。");
        }

        const size_t expectedCellCount = GetExpectedCellCount(data);
        if (data.cells.size() != expectedCellCount)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "cells の要素数が (gridWidth - 1) * (gridDepth - 1) と一致していません。");
        }
        for (const CellData& cell : data.cells)
        {
            if (cell.groundTextureId < 0)
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "cells の groundTextureId は 0 以上である必要があります。");
                break;
            }
            if (cell.waterDepth == WaterDepth::Lake &&
                (cell.walkable || cell.ropeAllowed || cell.miningAllowed || cell.enemySpawnAllowed))
            {
                AddIssue(issues, ValidationIssue::Severity::Error,
                    "湖セルは歩行、ロープ、採掘、敵スポーンを無効にする必要があります。");
                break;
            }
        }

        if (data.miningPoints.empty())
        {
            AddIssue(issues, ValidationIssue::Severity::Warning, "miningPoints が 0 件です。1〜5 件を推奨します。");
        }
        else if (data.miningPoints.size() >= 6)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "miningPoints は 5 件以下である必要があります。");
        }

        for (size_t index = 0; index < data.miningPoints.size(); ++index)
        {
            const MiningPointData& point = data.miningPoints[index];
            if (!IsCellInRange(data, point.cell))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "miningPoint の cell が範囲外です。");
            }
            if (point.id.empty())
            {
                AddIssue(issues, ValidationIssue::Severity::Warning, "miningPoint の id が空です。");
            }
        }

        if (data.rope.enabled)
        {
            if (!IsCellInRange(data, data.rope.top) || !IsCellInRange(data, data.rope.bottom))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "rope の top または bottom が範囲外です。");
            }
        }

        if (data.layerTransition.role != LayerTransitionRole::None)
        {
            if (!data.layerTransition.ropePointEnabled || !IsCellInRange(data, data.layerTransition.ropePoint))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "layerTransition の ropePoint が必要です。");
            }
            if (data.layerTransition.role == LayerTransitionRole::Exit &&
                (!data.layerTransition.loadPointEnabled || !IsCellInRange(data, data.layerTransition.loadPoint)))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "層出口には loadPoint が必要です。");
            }
        }

        for (size_t index = 0; index < data.fishingPoints.size(); ++index)
        {
            const FishingPointData& point = data.fishingPoints[index];
            if (!IsCellInRange(data, point.shoreCell) || !IsCellInRange(data, point.waterCell))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "fishingPoint のセルが範囲外です。");
                continue;
            }
            const int cellWidth = data.gridWidth - 1;
            const CellData& shore = data.cells[static_cast<size_t>(point.shoreCell.z * cellWidth + point.shoreCell.x)];
            const CellData& water = data.cells[static_cast<size_t>(point.waterCell.z * cellWidth + point.waterCell.x)];
            const int distance = std::abs(point.shoreCell.x - point.waterCell.x) +
                std::abs(point.shoreCell.z - point.waterCell.z);
            if (shore.deleted || !shore.walkable ||
                (shore.waterDepth != WaterDepth::None && shore.waterDepth != WaterDepth::Puddle))
                AddIssue(issues, ValidationIssue::Severity::Error, "釣り地点の岸セルは歩行可能な陸地または溜りである必要があります。");
            if (water.deleted)
                AddIssue(issues, ValidationIssue::Severity::Error, "釣り地点の対象水面は削除セルにできません。");
            if (water.waterDepth != WaterDepth::Pond && water.waterDepth != WaterDepth::Lake)
                AddIssue(issues, ValidationIssue::Severity::Error, "釣り地点の対象水面は池または湖である必要があります。");
            if (distance != 1)
                AddIssue(issues, ValidationIssue::Severity::Error, "釣り地点の岸セルと水面セルは隣接している必要があります。");
            if (point.id.empty())
                AddIssue(issues, ValidationIssue::Severity::Warning, "fishingPoint の id が空です。");
            const bool overlapsMining = std::any_of(data.miningPoints.begin(), data.miningPoints.end(),
                [&](const MiningPointData& mining)
                { return mining.cell.x == point.shoreCell.x && mining.cell.z == point.shoreCell.z; });
            const bool overlapsEnvironment = std::any_of(data.environmentObjects.begin(), data.environmentObjects.end(),
                [&](const EnvironmentObjectData& object)
                {
                    return (object.cell.x == point.shoreCell.x && object.cell.z == point.shoreCell.z) ||
                        (object.cell.x == point.waterCell.x && object.cell.z == point.waterCell.z);
                });
            const auto overlapsShore = [&](const GridPoint& cell)
            {
                return cell.x == point.shoreCell.x && cell.z == point.shoreCell.z;
            };
            const bool overlapsRope = data.rope.enabled &&
                (overlapsShore(data.rope.top) || overlapsShore(data.rope.bottom));
            const bool overlapsStartReturn = data.startReturnCandidate.enabled &&
                overlapsShore(data.startReturnCandidate.cell);
            const bool overlapsLayerTransition =
                (data.layerTransition.ropePointEnabled && overlapsShore(data.layerTransition.ropePoint)) ||
                (data.layerTransition.loadPointEnabled && overlapsShore(data.layerTransition.loadPoint));
            if (overlapsMining || overlapsEnvironment || overlapsRope ||
                overlapsStartReturn || overlapsLayerTransition)
                AddIssue(issues, ValidationIssue::Severity::Error, "釣り地点が別の配置物と重なっています。");
            for (size_t other = index + 1; other < data.fishingPoints.size(); ++other)
            {
                if (data.fishingPoints[other].shoreCell.x == point.shoreCell.x &&
                    data.fishingPoints[other].shoreCell.z == point.shoreCell.z)
                    AddIssue(issues, ValidationIssue::Severity::Error, "同じ岸セルに複数の釣り地点は配置できません。");
            }
        }

        for (size_t index = 0; index < data.environmentObjects.size(); ++index)
        {
            const EnvironmentObjectData& object = data.environmentObjects[index];
            if (object.modelId.empty())
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "environmentObject の modelId が空です。");
            }
            if (!IsCellInRange(data, object.cell))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "environmentObject の cell が範囲外です。");
                continue;
            }
            const int cellWidth = std::max(0, data.gridWidth - 1);
            const int cellIndex = object.cell.z * cellWidth + object.cell.x;
            if (cellIndex >= 0 && cellIndex < static_cast<int>(data.cells.size()) &&
                data.cells[static_cast<size_t>(cellIndex)].deleted)
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "environmentObject は削除セルへ配置できません。");
            }
            if (object.scaleX <= 0.0f || object.scaleY <= 0.0f || object.scaleZ <= 0.0f)
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "environmentObject の scale は 0 より大きい値が必要です。");
            }

            for (size_t otherIndex = index + 1; otherIndex < data.environmentObjects.size(); ++otherIndex)
            {
                const EnvironmentObjectData& other = data.environmentObjects[otherIndex];
                if (object.cell.x == other.cell.x && object.cell.z == other.cell.z)
                {
                    AddIssue(issues, ValidationIssue::Severity::Error, "同じセルに複数の environmentObject は配置できません。");
                    break;
                }
            }

            const bool overlapsMining = std::any_of(
                data.miningPoints.begin(), data.miningPoints.end(),
                [&](const MiningPointData& point)
                {
                    return point.cell.x == object.cell.x && point.cell.z == object.cell.z;
                });
            const bool overlapsStartReturn = data.startReturnCandidate.enabled &&
                data.startReturnCandidate.cell.x == object.cell.x &&
                data.startReturnCandidate.cell.z == object.cell.z;
            const bool overlapsLoadPoint = data.layerTransition.loadPointEnabled &&
                data.layerTransition.loadPoint.x == object.cell.x &&
                data.layerTransition.loadPoint.z == object.cell.z;
            if (overlapsMining || overlapsStartReturn || overlapsLoadPoint)
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "environmentObject がロープ以外のゲームオブジェクトと重なっています。");
            }
        }

        if (data.stageRole == StageRole::StartReturn && !data.startReturnCandidate.enabled)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "stageRole が StartReturn の場合は startReturnCandidate.enabled が必要です。");
        }
        if (data.baseType != BaseType::None && data.stageRole != StageRole::Base)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "baseType を設定する場合は stageRole を Base にする必要があります。");
        }
        if (data.stageRole == StageRole::Base && data.baseType == BaseType::None)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "stageRole が Base の場合は baseType が必要です。");
        }

        if (data.isSurface)
        {
            if (data.sizePreset != SizePreset::Size8x8 || data.gridWidth != 8 || data.gridDepth != 8)
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "地上小ステージは8x8である必要があります。");
            }
            const GridPoint& cell = data.surfaceFacility.cell;
            if (data.surfaceFacility.type != SurfaceFacilityType::None &&
                (cell.x < 0 || cell.z < 0 || cell.x + 1 >= data.gridWidth - 1 || cell.z + 1 >= data.gridDepth - 1))
            {
                AddIssue(issues, ValidationIssue::Severity::Error, "地上施設の2x2範囲が小ステージ外です。");
            }
            if (data.surfaceFacility.type != SurfaceFacilityType::None)
            {
                const std::string& modelPath = data.surfaceFacility.modelPath;
                if (!modelPath.empty() && (modelPath.find(':') != std::string::npos ||
                    modelPath.rfind("..", 0) == 0 || modelPath.front() == '/' || modelPath.front() == '\\'))
                {
                    AddIssue(issues, ValidationIssue::Severity::Error, "地上施設モデルはAssets基準の相対パスで指定してください。");
                }
                const bool overlapsMining = std::any_of(data.miningPoints.begin(), data.miningPoints.end(),
                    [&](const MiningPointData& point)
                    {
                        return point.cell.x >= cell.x && point.cell.x < cell.x + 2 &&
                            point.cell.z >= cell.z && point.cell.z < cell.z + 2;
                    });
                const bool overlapsEnvironment = std::any_of(data.environmentObjects.begin(), data.environmentObjects.end(),
                    [&](const EnvironmentObjectData& object)
                    {
                        return object.cell.x >= cell.x && object.cell.x < cell.x + 2 &&
                            object.cell.z >= cell.z && object.cell.z < cell.z + 2;
                    });
                if (overlapsMining || overlapsEnvironment)
                    AddIssue(issues, ValidationIssue::Severity::Error, "地上施設の2x2範囲に別の配置物があります。");
            }
        }

        if (data.startReturnCandidate.enabled && !IsCellInRange(data, data.startReturnCandidate.cell))
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "startReturnCandidate.cell が範囲外です。");
        }

        return issues;
    }

    bool HasValidationError(const std::vector<ValidationIssue>& issues)
    {
        for (const ValidationIssue& issue : issues)
        {
            if (issue.severity == ValidationIssue::Severity::Error)
            {
                return true;
            }
        }
        return false;
    }

    std::wstring GetDraftsDirectoryRelativePath(int abyssLayer)
    {
        return BuildDirectoryRelativePath(abyssLayer, kDraftDirectoryName);
    }

    std::wstring GetCompletedDirectoryRelativePath(int abyssLayer)
    {
        return BuildDirectoryRelativePath(abyssLayer, kCompletedDirectoryName);
    }

    std::wstring GetSurfaceDraftsDirectoryRelativePath()
    {
        return L"Assets\\Naraku\\Pieces\\Surface\\Drafts";
    }

    std::wstring GetSurfaceCompletedDirectoryRelativePath()
    {
        return L"Assets\\Naraku\\Pieces\\Surface\\Completed";
    }

    std::wstring NormalizePieceFileName(std::wstring fileName)
    {
        fileName = GetFileNamePart(fileName);
        if (fileName.empty())
        {
            fileName = kDefaultPieceFileName;
        }

        for (wchar_t& ch : fileName)
        {
            const bool invalid =
                ch == L'<' || ch == L'>' || ch == L':' || ch == L'"' ||
                ch == L'/' || ch == L'\\' || ch == L'|' || ch == L'?' ||
                ch == L'*' || ch < 0x20;
            if (invalid)
            {
                ch = L'_';
            }
        }

        if (fileName == L"." || fileName == L"..")
        {
            fileName = kDefaultPieceFileName;
        }

        const std::wstring extension = L".json";
        if (fileName.size() < extension.size() ||
            _wcsicmp(fileName.c_str() + fileName.size() - extension.size(), extension.c_str()) != 0)
        {
            fileName += extension;
        }

        return fileName;
    }

    std::wstring MakeDraftPiecePath(int abyssLayer, const std::wstring& fileName)
    {
        return GetDraftsDirectoryRelativePath(abyssLayer) + L"\\" + NormalizePieceFileName(fileName);
    }

    std::wstring MakeCompletedPiecePath(int abyssLayer, const std::wstring& fileName)
    {
        return GetCompletedDirectoryRelativePath(abyssLayer) + L"\\" + NormalizePieceFileName(fileName);
    }

    std::wstring MakeSurfaceDraftPiecePath(const std::wstring& fileName)
    {
        return GetSurfaceDraftsDirectoryRelativePath() + L"\\" + NormalizePieceFileName(fileName);
    }

    std::wstring MakeSurfaceCompletedPiecePath(const std::wstring& fileName)
    {
        return GetSurfaceCompletedDirectoryRelativePath() + L"\\" + NormalizePieceFileName(fileName);
    }

    std::wstring ResolvePiecePathForFileSystem(const std::wstring& relativeOrAbsolutePath)
    {
        if (relativeOrAbsolutePath.empty())
        {
            return ResolveProjectRelativePath(MakeDraftPiecePath(1, L"").c_str());
        }

        if (IsAbsolutePath(relativeOrAbsolutePath))
        {
            return relativeOrAbsolutePath;
        }

        return ResolveProjectRelativePath(NormalizeRelativePiecePath(relativeOrAbsolutePath));
    }

    bool SavePieceData(const PieceData& data, const std::wstring& relativeOrAbsolutePath, std::string* outError)
    {
        const std::wstring fileSystemPath = ResolvePiecePathForFileSystem(relativeOrAbsolutePath);
        if (!EnsureDirectoryTreeForFile(fileSystemPath.c_str()))
        {
            if (outError != nullptr)
            {
                *outError = "failed to create piece directories";
            }
            return false;
        }

        const std::string lastModified = data.lastModified.empty() ? BuildCurrentTimestamp() : data.lastModified;
        std::ostringstream out;
        out << "{\n";
        AppendIndent(out, 1);
        out << "\"version\": " << data.version << ",\n";
        AppendIndent(out, 1);
        out << "\"id\": \"" << EscapeJsonString(data.id) << "\",\n";
        AppendIndent(out, 1);
        out << "\"displayName\": \"" << EscapeJsonString(data.displayName) << "\",\n";
        AppendIndent(out, 1);
        out << "\"lastModified\": \"" << EscapeJsonString(lastModified) << "\",\n";
        AppendIndent(out, 1);
        out << "\"abyssLayer\": " << data.abyssLayer << ",\n";
        AppendIndent(out, 1);
        out << "\"sublayerTag\": \"" << ToString(data.sublayerTag) << "\",\n";
        AppendIndent(out, 1);
        out << "\"isSurface\": " << (data.isSurface ? "true" : "false") << ",\n";
        AppendIndent(out, 1);
        out << "\"sizePreset\": \"" << ToString(data.sizePreset) << "\",\n";
        AppendIndent(out, 1);
        out << "\"gridWidth\": " << data.gridWidth << ",\n";
        AppendIndent(out, 1);
        out << "\"gridDepth\": " << data.gridDepth << ",\n";
        AppendIndent(out, 1);
        out << "\"cellSize\": " << data.cellSize << ",\n";
        AppendIndent(out, 1);
        out << "\"stageRole\": \"" << ToString(data.stageRole) << "\",\n";
        AppendIndent(out, 1);
        out << "\"stageCategory\": \"" << ToString(data.stageCategory) << "\",\n";
        AppendIndent(out, 1);
        out << "\"baseType\": \"" << ToString(data.baseType) << "\",\n";
        AppendIndent(out, 1);
        out << "\"baseModelOffset\": { \"x\": " << data.baseModelOffsetX
            << ", \"y\": " << data.baseModelOffsetY
            << ", \"z\": " << data.baseModelOffsetZ << " },\n";
        AppendIndent(out, 1);
        out << "\"edgeCategories\": {\n";
        AppendIndent(out, 2);
        out << "\"north\": \"" << ToString(data.edgeCategories.north) << "\",\n";
        AppendIndent(out, 2);
        out << "\"south\": \"" << ToString(data.edgeCategories.south) << "\",\n";
        AppendIndent(out, 2);
        out << "\"east\": \"" << ToString(data.edgeCategories.east) << "\",\n";
        AppendIndent(out, 2);
        out << "\"west\": \"" << ToString(data.edgeCategories.west) << "\"\n";
        AppendIndent(out, 1);
        out << "},\n";
        AppendIndent(out, 1);
        out << "\"lockedEdges\": {\n";
        AppendIndent(out, 2);
        out << "\"north\": " << (data.lockedEdges.north ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"south\": " << (data.lockedEdges.south ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"east\": " << (data.lockedEdges.east ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"west\": " << (data.lockedEdges.west ? "true" : "false") << "\n";
        AppendIndent(out, 1);
        out << "},\n";
        AppendIndent(out, 1);
        out << "\"heights\": [";
        for (size_t i = 0; i < data.heights.size(); ++i)
        {
            if (i > 0)
            {
                out << ", ";
            }
        out << data.heights[i];
        }
        out << "],\n";
        AppendIndent(out, 1);
        out << "\"cells\": [\n";
        for (size_t i = 0; i < data.cells.size(); ++i)
        {
            const CellData& cell = data.cells[i];
            AppendIndent(out, 2);
            out << "{ "
                << "\"deleted\": " << (cell.deleted ? "true" : "false") << ", "
                << "\"walkable\": " << (cell.walkable ? "true" : "false") << ", "
                << "\"ropeAllowed\": " << (cell.ropeAllowed ? "true" : "false") << ", "
                << "\"miningAllowed\": " << (cell.miningAllowed ? "true" : "false") << ", "
                << "\"enemySpawnAllowed\": " << (cell.enemySpawnAllowed ? "true" : "false") << ", "
                << "\"groundTextureId\": " << cell.groundTextureId << ", "
                << "\"waterDepth\": \"" << ToString(cell.waterDepth) << "\""
                << " }";
            out << (i + 1 < data.cells.size() ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "],\n";
        AppendIndent(out, 1);
        out << "\"rope\": {\n";
        AppendIndent(out, 2);
        out << "\"enabled\": " << (data.rope.enabled ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"top\": { \"x\": " << data.rope.top.x << ", \"z\": " << data.rope.top.z << " },\n";
        AppendIndent(out, 2);
        out << "\"bottom\": { \"x\": " << data.rope.bottom.x << ", \"z\": " << data.rope.bottom.z << " }\n";
        AppendIndent(out, 1);
        out << "},\n";
        AppendIndent(out, 1);
        out << "\"layerTransition\": {\n";
        AppendIndent(out, 2);
        out << "\"role\": \"" << ToString(data.layerTransition.role) << "\",\n";
        AppendIndent(out, 2);
        out << "\"ropePointEnabled\": " << (data.layerTransition.ropePointEnabled ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"ropePoint\": { \"x\": " << data.layerTransition.ropePoint.x << ", \"z\": " << data.layerTransition.ropePoint.z << " },\n";
        AppendIndent(out, 2);
        out << "\"loadPointEnabled\": " << (data.layerTransition.loadPointEnabled ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"loadPoint\": { \"x\": " << data.layerTransition.loadPoint.x << ", \"z\": " << data.layerTransition.loadPoint.z << " }\n";
        AppendIndent(out, 1);
        out << "},\n";
        AppendIndent(out, 1);
        out << "\"miningPoints\": [\n";
        for (size_t i = 0; i < data.miningPoints.size(); ++i)
        {
            const MiningPointData& point = data.miningPoints[i];
            AppendIndent(out, 2);
            out << "{ "
                << "\"id\": \"" << EscapeJsonString(point.id) << "\", "
                << "\"cell\": { \"x\": " << point.cell.x << ", \"z\": " << point.cell.z << " }, "
                << "\"visualType\": " << point.visualType << ", "
                << "\"initiallyRecorded\": " << (point.initiallyRecorded ? "true" : "false")
                << " }";
            out << (i + 1 < data.miningPoints.size() ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "],\n";
        AppendIndent(out, 1);
        out << "\"fishingPoints\": [\n";
        for (size_t i = 0; i < data.fishingPoints.size(); ++i)
        {
            const FishingPointData& point = data.fishingPoints[i];
            AppendIndent(out, 2);
            out << "{ \"id\": \"" << EscapeJsonString(point.id) << "\", "
                << "\"shoreCell\": { \"x\": " << point.shoreCell.x << ", \"z\": " << point.shoreCell.z << " }, "
                << "\"waterCell\": { \"x\": " << point.waterCell.x << ", \"z\": " << point.waterCell.z << " } }";
            out << (i + 1 < data.fishingPoints.size() ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "],\n";
        AppendIndent(out, 1);
        out << "\"environmentObjects\": [\n";
        for (size_t i = 0; i < data.environmentObjects.size(); ++i)
        {
            const EnvironmentObjectData& object = data.environmentObjects[i];
            AppendIndent(out, 2);
            out << "{ "
                << "\"modelId\": \"" << EscapeJsonString(object.modelId) << "\", "
                << "\"cell\": { \"x\": " << object.cell.x << ", \"z\": " << object.cell.z << " }, "
                << "\"scale\": { \"x\": " << object.scaleX << ", \"y\": " << object.scaleY << ", \"z\": " << object.scaleZ << " }"
                << " }";
            out << (i + 1 < data.environmentObjects.size() ? ",\n" : "\n");
        }
        AppendIndent(out, 1);
        out << "],\n";
        AppendIndent(out, 1);
        out << "\"startReturnCandidate\": {\n";
        AppendIndent(out, 2);
        out << "\"enabled\": " << (data.startReturnCandidate.enabled ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"cell\": { \"x\": " << data.startReturnCandidate.cell.x << ", \"z\": " << data.startReturnCandidate.cell.z << " },\n";
        AppendIndent(out, 2);
        out << "\"facing\": \"" << ToString(data.startReturnCandidate.facing) << "\"\n";
        AppendIndent(out, 1);
        out << "},\n";
        AppendIndent(out, 1);
        out << "\"surfaceFacility\": {\n";
        AppendIndent(out, 2);
        out << "\"type\": \"" << ToString(data.surfaceFacility.type) << "\",\n";
        AppendIndent(out, 2);
        out << "\"cell\": { \"x\": " << data.surfaceFacility.cell.x << ", \"z\": " << data.surfaceFacility.cell.z << " },\n";
        AppendIndent(out, 2);
        out << "\"facing\": \"" << ToString(data.surfaceFacility.facing) << "\",\n";
        AppendIndent(out, 2);
        out << "\"modelPath\": \"" << EscapeJsonString(data.surfaceFacility.modelPath) << "\",\n";
        AppendIndent(out, 2);
        out << "\"offset\": { \"x\": " << data.surfaceFacility.offsetX << ", \"y\": " << data.surfaceFacility.offsetY << ", \"z\": " << data.surfaceFacility.offsetZ << " },\n";
        AppendIndent(out, 2);
        out << "\"scale\": { \"x\": " << data.surfaceFacility.scaleX << ", \"y\": " << data.surfaceFacility.scaleY << ", \"z\": " << data.surfaceFacility.scaleZ << " },\n";
        AppendIndent(out, 2);
        out << "\"rotationQuarterTurns\": " << data.surfaceFacility.rotationQuarterTurns << "\n";
        AppendIndent(out, 1);
        out << "}\n";
        out << "}\n";

        if (!WriteWholeFile(fileSystemPath.c_str(), out.str()))
        {
            if (outError != nullptr)
            {
                *outError = "failed to open piece file for writing";
            }
            return false;
        }

        return true;
    }

    bool LoadPieceData(const std::wstring& relativeOrAbsolutePath, PieceData& outData, std::string* outError)
    {
        std::string jsonText;
        const std::wstring fileSystemPath = ResolvePiecePathForFileSystem(relativeOrAbsolutePath);
        if (!ReadWholeFile(fileSystemPath.c_str(), jsonText))
        {
            if (outError != nullptr)
            {
                *outError = "failed to open piece file for reading";
            }
            return false;
        }

        JsonValue rootValue;
        JsonParser parser(jsonText);
        if (!parser.Parse(rootValue))
        {
            if (outError != nullptr)
            {
                *outError = "failed to parse piece json";
            }
            return false;
        }

        if (rootValue.type != JsonValue::TypeObject)
        {
            if (outError != nullptr)
            {
                *outError = "piece root is not object";
            }
            return false;
        }

        PieceData loadedData = CreateDefaultPiece();
        double number = 0.0;
        std::string text;
        bool boolValue = false;

        if (GetNumber(rootValue, "version", number))
        {
            loadedData.version = static_cast<int>(number);
        }
        if (GetString(rootValue, "id", text))
        {
            loadedData.id = text;
        }
        if (GetString(rootValue, "displayName", text))
        {
            loadedData.displayName = text;
        }
        if (GetString(rootValue, "lastModified", text))
        {
            loadedData.lastModified = text;
        }
        if (GetNumber(rootValue, "abyssLayer", number))
        {
            loadedData.abyssLayer = static_cast<int>(number);
        }
        if (GetString(rootValue, "sublayerTag", text))
        {
            TryParseSublayerTag(text, loadedData.sublayerTag);
        }
        if (GetBool(rootValue, "isSurface", boolValue)) loadedData.isSurface = boolValue;
        if (GetString(rootValue, "sizePreset", text))
        {
            SizePreset preset = loadedData.sizePreset;
            if (TryParseSizePreset(text, preset))
            {
                loadedData.sizePreset = preset;
            }
        }
        if (GetNumber(rootValue, "gridWidth", number))
        {
            loadedData.gridWidth = static_cast<int>(number);
        }
        if (GetNumber(rootValue, "gridDepth", number))
        {
            loadedData.gridDepth = static_cast<int>(number);
        }
        if (GetNumber(rootValue, "cellSize", number))
        {
            loadedData.cellSize = static_cast<float>(number);
        }
        if (GetString(rootValue, "stageRole", text))
        {
            StageRole role = loadedData.stageRole;
            if (TryParseStageRole(text, role))
            {
                loadedData.stageRole = role;
            }
        }
        if (GetString(rootValue, "stageCategory", text))
        {
            StageCategory category = loadedData.stageCategory;
            if (TryParseStageCategory(text, category))
            {
                loadedData.stageCategory = category;
            }
        }
        if (GetString(rootValue, "baseType", text))
        {
            TryParseBaseType(text, loadedData.baseType);
        }

        const JsonValue* baseModelOffsetValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "baseModelOffset", baseModelOffsetValue) &&
            baseModelOffsetValue->type == JsonValue::TypeObject)
        {
            if (GetNumber(*baseModelOffsetValue, "x", number)) loadedData.baseModelOffsetX = static_cast<float>(number);
            if (GetNumber(*baseModelOffsetValue, "y", number)) loadedData.baseModelOffsetY = static_cast<float>(number);
            if (GetNumber(*baseModelOffsetValue, "z", number)) loadedData.baseModelOffsetZ = static_cast<float>(number);
        }

        const JsonValue* edgeCategoriesValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "edgeCategories", edgeCategoriesValue) &&
            edgeCategoriesValue->type == JsonValue::TypeObject)
        {
            if (GetString(*edgeCategoriesValue, "north", text))
            {
                TryParseStageCategory(text, loadedData.edgeCategories.north);
            }
            if (GetString(*edgeCategoriesValue, "south", text))
            {
                TryParseStageCategory(text, loadedData.edgeCategories.south);
            }
            if (GetString(*edgeCategoriesValue, "east", text))
            {
                TryParseStageCategory(text, loadedData.edgeCategories.east);
            }
            if (GetString(*edgeCategoriesValue, "west", text))
            {
                TryParseStageCategory(text, loadedData.edgeCategories.west);
            }
        }

        const JsonValue* lockedEdgesValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "lockedEdges", lockedEdgesValue) &&
            lockedEdgesValue->type == JsonValue::TypeObject)
        {
            if (GetBool(*lockedEdgesValue, "north", boolValue)) loadedData.lockedEdges.north = boolValue;
            if (GetBool(*lockedEdgesValue, "south", boolValue)) loadedData.lockedEdges.south = boolValue;
            if (GetBool(*lockedEdgesValue, "east", boolValue)) loadedData.lockedEdges.east = boolValue;
            if (GetBool(*lockedEdgesValue, "west", boolValue)) loadedData.lockedEdges.west = boolValue;
        }

        const JsonValue* heightsValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "heights", heightsValue) &&
            heightsValue->type == JsonValue::TypeArray)
        {
            loadedData.heights.clear();
            loadedData.heights.reserve(heightsValue->arrayValue.size());
            for (const JsonValue& heightValue : heightsValue->arrayValue)
            {
                loadedData.heights.push_back(
                    heightValue.type == JsonValue::TypeNumber ? static_cast<float>(heightValue.numberValue) : 0.0f);
            }
        }

        const JsonValue* cellsValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "cells", cellsValue) &&
            cellsValue->type == JsonValue::TypeArray)
        {
            loadedData.cells.clear();
            loadedData.cells.reserve(cellsValue->arrayValue.size());
            for (const JsonValue& cellValue : cellsValue->arrayValue)
            {
                if (cellValue.type != JsonValue::TypeObject)
                {
                    loadedData.cells.push_back(CellData());
                    continue;
                }

                CellData cell;
                if (GetBool(cellValue, "deleted", boolValue))
                {
                    cell.deleted = boolValue;
                }
                if (GetBool(cellValue, "walkable", boolValue))
                {
                    cell.walkable = boolValue;
                }
                if (GetBool(cellValue, "ropeAllowed", boolValue))
                {
                    cell.ropeAllowed = boolValue;
                }
                if (GetBool(cellValue, "miningAllowed", boolValue))
                {
                    cell.miningAllowed = boolValue;
                }
                if (GetBool(cellValue, "enemySpawnAllowed", boolValue))
                {
                    cell.enemySpawnAllowed = boolValue;
                }
                if (GetNumber(cellValue, "groundTextureId", number))
                {
                    cell.groundTextureId = static_cast<int>(number);
                }
                if (GetString(cellValue, "waterDepth", text))
                {
                    TryParseWaterDepth(text, cell.waterDepth);
                }
                loadedData.cells.push_back(cell);
            }
        }

        const JsonValue* ropeValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "rope", ropeValue) && ropeValue->type == JsonValue::TypeObject)
        {
            if (GetBool(*ropeValue, "enabled", boolValue))
            {
                loadedData.rope.enabled = boolValue;
            }
            LoadGridPoint(*ropeValue, "top", loadedData.rope.top);
            LoadGridPoint(*ropeValue, "bottom", loadedData.rope.bottom);
        }

        const JsonValue* layerTransitionValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "layerTransition", layerTransitionValue) &&
            layerTransitionValue->type == JsonValue::TypeObject)
        {
            if (GetString(*layerTransitionValue, "role", text))
            {
                TryParseLayerTransitionRole(text, loadedData.layerTransition.role);
            }
            if (GetBool(*layerTransitionValue, "ropePointEnabled", boolValue))
            {
                loadedData.layerTransition.ropePointEnabled = boolValue;
            }
            LoadGridPoint(*layerTransitionValue, "ropePoint", loadedData.layerTransition.ropePoint);
            if (GetBool(*layerTransitionValue, "loadPointEnabled", boolValue))
            {
                loadedData.layerTransition.loadPointEnabled = boolValue;
            }
            LoadGridPoint(*layerTransitionValue, "loadPoint", loadedData.layerTransition.loadPoint);
        }

        const JsonValue* miningPointsValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "miningPoints", miningPointsValue) &&
            miningPointsValue->type == JsonValue::TypeArray)
        {
            loadedData.miningPoints.clear();
            loadedData.miningPoints.reserve(miningPointsValue->arrayValue.size());
            for (const JsonValue& pointValue : miningPointsValue->arrayValue)
            {
                if (pointValue.type != JsonValue::TypeObject)
                {
                    continue;
                }

                MiningPointData point;
                if (GetString(pointValue, "id", text))
                {
                    point.id = text;
                }
                LoadGridPoint(pointValue, "cell", point.cell);
                if (GetNumber(pointValue, "visualType", number))
                {
                    point.visualType = static_cast<int>(number);
                }
                if (GetBool(pointValue, "initiallyRecorded", boolValue))
                {
                    point.initiallyRecorded = boolValue;
                }
                loadedData.miningPoints.push_back(point);
            }
        }

        const JsonValue* fishingPointsValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "fishingPoints", fishingPointsValue) &&
            fishingPointsValue->type == JsonValue::TypeArray)
        {
            loadedData.fishingPoints.clear();
            for (const JsonValue& pointValue : fishingPointsValue->arrayValue)
            {
                if (pointValue.type != JsonValue::TypeObject) continue;
                FishingPointData point;
                if (GetString(pointValue, "id", text)) point.id = text;
                LoadGridPoint(pointValue, "shoreCell", point.shoreCell);
                LoadGridPoint(pointValue, "waterCell", point.waterCell);
                loadedData.fishingPoints.push_back(point);
            }
        }

        const JsonValue* environmentObjectsValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "environmentObjects", environmentObjectsValue) &&
            environmentObjectsValue->type == JsonValue::TypeArray)
        {
            loadedData.environmentObjects.clear();
            loadedData.environmentObjects.reserve(environmentObjectsValue->arrayValue.size());
            for (const JsonValue& objectValue : environmentObjectsValue->arrayValue)
            {
                if (objectValue.type != JsonValue::TypeObject)
                {
                    continue;
                }

                EnvironmentObjectData object;
                if (GetString(objectValue, "modelId", text))
                {
                    object.modelId = text;
                }
                LoadGridPoint(objectValue, "cell", object.cell);
                const JsonValue* scaleValue = nullptr;
                if (GetObjectValue<JsonValue>(objectValue, "scale", scaleValue) &&
                    scaleValue->type == JsonValue::TypeObject)
                {
                    if (GetNumber(*scaleValue, "x", number)) object.scaleX = static_cast<float>(number);
                    if (GetNumber(*scaleValue, "y", number)) object.scaleY = static_cast<float>(number);
                    if (GetNumber(*scaleValue, "z", number)) object.scaleZ = static_cast<float>(number);
                }
                loadedData.environmentObjects.push_back(object);
            }
        }

        const JsonValue* startReturnValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "startReturnCandidate", startReturnValue) &&
            startReturnValue->type == JsonValue::TypeObject)
        {
            if (GetBool(*startReturnValue, "enabled", boolValue))
            {
                loadedData.startReturnCandidate.enabled = boolValue;
            }
            LoadGridPoint(*startReturnValue, "cell", loadedData.startReturnCandidate.cell);
            if (GetString(*startReturnValue, "facing", text))
            {
                TryParseDirection(text, loadedData.startReturnCandidate.facing);
            }
        }

        const JsonValue* surfaceFacilityValue = nullptr;
        if (GetObjectValue<JsonValue>(rootValue, "surfaceFacility", surfaceFacilityValue) &&
            surfaceFacilityValue->type == JsonValue::TypeObject)
        {
            if (GetString(*surfaceFacilityValue, "type", text))
                TryParseSurfaceFacilityType(text, loadedData.surfaceFacility.type);
            LoadGridPoint(*surfaceFacilityValue, "cell", loadedData.surfaceFacility.cell);
            if (GetString(*surfaceFacilityValue, "facing", text))
                TryParseDirection(text, loadedData.surfaceFacility.facing);
            if (GetString(*surfaceFacilityValue, "modelPath", text))
                loadedData.surfaceFacility.modelPath = text;
            const JsonValue* offsetValue = nullptr;
            if (GetObjectValue<JsonValue>(*surfaceFacilityValue, "offset", offsetValue) &&
                offsetValue->type == JsonValue::TypeObject)
            {
                if (GetNumber(*offsetValue, "x", number)) loadedData.surfaceFacility.offsetX = static_cast<float>(number);
                if (GetNumber(*offsetValue, "y", number)) loadedData.surfaceFacility.offsetY = static_cast<float>(number);
                if (GetNumber(*offsetValue, "z", number)) loadedData.surfaceFacility.offsetZ = static_cast<float>(number);
            }
            const JsonValue* scaleValue = nullptr;
            if (GetObjectValue<JsonValue>(*surfaceFacilityValue, "scale", scaleValue) &&
                scaleValue->type == JsonValue::TypeObject)
            {
                if (GetNumber(*scaleValue, "x", number)) loadedData.surfaceFacility.scaleX = static_cast<float>(number);
                if (GetNumber(*scaleValue, "y", number)) loadedData.surfaceFacility.scaleY = static_cast<float>(number);
                if (GetNumber(*scaleValue, "z", number)) loadedData.surfaceFacility.scaleZ = static_cast<float>(number);
            }
            if (GetNumber(*surfaceFacilityValue, "rotationQuarterTurns", number))
                loadedData.surfaceFacility.rotationQuarterTurns = static_cast<int>(number);
        }

        const size_t expectedCellCount = GetExpectedCellCount(loadedData);
        if (loadedData.cells.size() < expectedCellCount)
        {
            loadedData.cells.resize(expectedCellCount);
        }
        else if (loadedData.cells.size() > expectedCellCount)
        {
            loadedData.cells.resize(expectedCellCount);
        }

        outData = loadedData;
        return true;
    }
}
