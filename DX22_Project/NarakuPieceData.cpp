#include "NarakuPieceData.h"

#include <Windows.h>
#undef max
#undef min

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace
{
    constexpr int kCurrentPieceVersion = 1;
    const wchar_t* kProjectRootPath = L"C:\\HAL\\個人制作\\NarakuProto";
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
     * @brief 相対パスをプロジェクトルート基準の絶対パスへ変換します。
     */
    std::wstring ResolveProjectRelativePath(const std::wstring& relativePath)
    {
        std::wstring resolvedPath(kProjectRootPath);
        if (!resolvedPath.empty() &&
            resolvedPath.back() != L'\\' &&
            resolvedPath.back() != L'/')
        {
            resolvedPath += L'\\';
        }

        resolvedPath += relativePath;
        return resolvedPath;
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
        data.abyssLayer = 1;
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
        if (data.abyssLayer < 1)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "abyssLayer は 1 以上である必要があります。");
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

        if (data.stageRole == StageRole::StartReturn && !data.startReturnCandidate.enabled)
        {
            AddIssue(issues, ValidationIssue::Severity::Error, "stageRole が StartReturn の場合は startReturnCandidate.enabled が必要です。");
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

        std::ostringstream out;
        out << "{\n";
        AppendIndent(out, 1);
        out << "\"version\": " << data.version << ",\n";
        AppendIndent(out, 1);
        out << "\"id\": \"" << EscapeJsonString(data.id) << "\",\n";
        AppendIndent(out, 1);
        out << "\"displayName\": \"" << EscapeJsonString(data.displayName) << "\",\n";
        AppendIndent(out, 1);
        out << "\"abyssLayer\": " << data.abyssLayer << ",\n";
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
                << "\"groundTextureId\": " << cell.groundTextureId
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
        out << "\"startReturnCandidate\": {\n";
        AppendIndent(out, 2);
        out << "\"enabled\": " << (data.startReturnCandidate.enabled ? "true" : "false") << ",\n";
        AppendIndent(out, 2);
        out << "\"cell\": { \"x\": " << data.startReturnCandidate.cell.x << ", \"z\": " << data.startReturnCandidate.cell.z << " },\n";
        AppendIndent(out, 2);
        out << "\"facing\": \"" << ToString(data.startReturnCandidate.facing) << "\"\n";
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
        if (GetNumber(rootValue, "abyssLayer", number))
        {
            loadedData.abyssLayer = static_cast<int>(number);
        }
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
