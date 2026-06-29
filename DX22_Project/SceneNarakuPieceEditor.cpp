#include "SceneNarakuPieceEditor.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Texture.h"
#include "imgui.h"
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <sstream>

using namespace DirectX;

namespace
{
    /**
     * @brief 座標変換に使用するウィンドウハンドルを取得します。
     */
    HWND GetPreviewHostWindow()
    {
        HWND window = ::GetActiveWindow();
        if (window == nullptr)
        {
            window = ::GetForegroundWindow();
        }
        return window;
    }

    /**
     * @brief ファイルパスから親ディレクトリ部分を取得します。
     * @param path 分解対象のファイルパスです。
     * @return 親ディレクトリのパスです。
     */
    std::wstring GetDirectoryPart(const std::wstring& path)
    {
        const size_t slashPos = path.find_last_of(L"\\/");
        return (slashPos == std::wstring::npos) ? std::wstring() : path.substr(0, slashPos);
    }

    /**
     * @brief ファイルパスからファイル名部分だけを取得します。
     * @param path 分解対象のファイルパスです。
     * @return ファイル名です。
     */
    std::wstring GetFileNamePart(const std::wstring& path)
    {
        const size_t slashPos = path.find_last_of(L"\\/");
        return (slashPos == std::wstring::npos) ? path : path.substr(slashPos + 1);
    }

    /**
     * @brief 拡張子がjsonかどうかを大文字小文字を無視して判定します。
     * @param path 判定対象のパスです。
     * @return 拡張子が.jsonならtrueを返します。
     */
    bool HasJsonExtension(const std::wstring& path)
    {
        const size_t dotPos = path.find_last_of(L'.');
        if (dotPos == std::wstring::npos)
        {
            return false;
        }

        std::wstring extension = path.substr(dotPos);
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return extension == L".json";
    }

    /**
     * @brief 保存ファイル名にjson拡張子が無い場合は補完します。
     * @param fileName 補正対象のファイル名です。
     * @return json拡張子を含むファイル名です。
     */
    std::wstring EnsureJsonFileName(std::wstring fileName)
    {
        if (fileName.empty())
        {
            return fileName;
        }

        if (!HasJsonExtension(fileName))
        {
            fileName += L".json";
        }
        return fileName;
    }

    /**
     * @brief Windowsのファイル名として使えない文字が含まれているか判定します。
     * @param fileName 判定対象のファイル名です。
     * @return 無効文字を含む場合はtrueを返します。
     */
    bool HasInvalidFileNameChar(const std::wstring& fileName)
    {
        static const wchar_t* kInvalidChars = L"\\/:*?\"<>|";
        return fileName.find_first_of(kInvalidChars) != std::wstring::npos;
    }

    /**
     * @brief パス区切り文字をスラッシュへ統一します。
     * @param path 正規化対象のパスです。
     * @return 区切り文字を `/` に統一したパスです。
     */
    std::wstring NormalizePathSeparators(std::wstring path)
    {
        std::replace(path.begin(), path.end(), L'\\', L'/');
        return path;
    }

    /**
     * @brief ワイド文字列を小文字化します。
     * @param text 変換対象の文字列です。
     * @return 小文字化した文字列です。
     */
    std::wstring ToLowerWide(std::wstring text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return text;
    }

    /**
     * @brief 小ステージHierarchy設定ファイルのヘッダー文字列です。
     */
    constexpr const char* kPieceHierarchyHeader = "NarakuPieceHierarchy\t1";

    /**
     * @brief Hierarchy設定や相対パス解決の基準にするプロジェクトルートです。
     */
    const wchar_t* kNarakuProjectRootPath = L"C:/HAL/個人制作/NarakuProto";

    /**
     * @brief 指定パスのファイルまたはディレクトリが存在するか判定します。
     * @param path 判定対象の絶対パスです。
     * @return 存在する場合はtrueを返します。
     */
    bool PathExists(const std::wstring& path)
    {
        const DWORD attributes = ::GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES;
    }

    /**
     * @brief 指定パスがWindowsの絶対パス形式か判定します。
     * @param path 判定対象のパスです。
     * @return ドライブレターまたはUNC形式ならtrueを返します。
     */
    bool IsAbsoluteWindowsPath(const std::wstring& path)
    {
        if (path.size() >= 2 && std::iswalpha(path[0]) && path[1] == L':')
        {
            return true;
        }

        return path.size() >= 2 &&
            ((path[0] == L'\\' && path[1] == L'\\') || (path[0] == L'/' && path[1] == L'/'));
    }

    /**
     * @brief Hierarchy設定用のプロジェクトルート絶対パスを取得します。
     * @return 区切りを `/` に統一したプロジェクトルートです。
     */
    std::wstring GetPieceHierarchyProjectRoot()
    {
        return NormalizePathSeparators(kNarakuProjectRootPath);
    }

    /**
     * @brief 親ディレクトリを含めて指定ディレクトリを順に作成します。
     * @param directoryPath 作成対象のディレクトリ絶対パスです。
     * @return 既存または作成成功ならtrueを返します。
     */
    bool EnsureDirectoryExists(const std::wstring& directoryPath)
    {
        if (directoryPath.empty())
        {
            return false;
        }

        std::wstring normalizedPath = NormalizePathSeparators(directoryPath);
        std::replace(normalizedPath.begin(), normalizedPath.end(), L'/', L'\\');
        if (PathExists(normalizedPath))
        {
            return true;
        }

        size_t startIndex = 0;
        if (normalizedPath.size() >= 2 && normalizedPath[1] == L':')
        {
            startIndex = 3;
        }
        else if (normalizedPath.size() >= 2 && normalizedPath[0] == L'\\' && normalizedPath[1] == L'\\')
        {
            startIndex = normalizedPath.find(L'\\', 2);
            if (startIndex == std::wstring::npos)
            {
                return false;
            }
            startIndex = normalizedPath.find(L'\\', startIndex + 1);
            if (startIndex == std::wstring::npos)
            {
                return false;
            }
            ++startIndex;
        }

        while (startIndex < normalizedPath.size())
        {
            const size_t separatorIndex = normalizedPath.find(L'\\', startIndex);
            const std::wstring partialPath = (separatorIndex == std::wstring::npos)
                ? normalizedPath
                : normalizedPath.substr(0, separatorIndex);
            if (!partialPath.empty() && !PathExists(partialPath))
            {
                if (!::CreateDirectoryW(partialPath.c_str(), nullptr))
                {
                    const DWORD error = ::GetLastError();
                    if (error != ERROR_ALREADY_EXISTS)
                    {
                        return false;
                    }
                }
            }

            if (separatorIndex == std::wstring::npos)
            {
                break;
            }
            startIndex = separatorIndex + 1;
        }

        return PathExists(normalizedPath);
    }


    constexpr float kPickThresholdPx = 16.0f;
    constexpr float kCellPickThresholdPx = 24.0f;
    constexpr float kDragSelectThresholdPx = 8.0f;
    constexpr float kSelectionMarkerHeight = 0.9f;
    constexpr float kCellOverlayYOffset = 0.08f;
    constexpr float kMinCameraPitch = -1.45f;
    constexpr float kMaxCameraPitch = 1.45f;
    constexpr float kMinCameraDistance = 4.0f;
    constexpr float kMaxCameraDistance = 160.0f;
    constexpr float kCameraOrbitSpeed = 0.010f;
    constexpr float kCameraPanScaleFactor = 0.0025f;
    constexpr float kCameraFovDegrees = 60.0f;
    constexpr float kCameraNearPlane = 0.1f;
    constexpr float kCameraFarPlane = 500.0f;
    constexpr size_t kMaxUndoHistory = 64;

    constexpr float kInitialCameraYaw = -0.927295f;
    constexpr float kInitialCameraPitch = 0.540420f;
    constexpr float kInitialCameraDistance = 34.985710f;

    /**
     * @brief ImGuiの表示サイズを基準にエディタ全体の描画領域サイズを取得します。
     * @return エディタ描画に使用するビューポートの幅と高さです。
     */
    XMFLOAT2 GetEditorViewportSize()
    {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float width = (displaySize.x > 1.0f) ? displaySize.x : static_cast<float>(SCREEN_WIDTH);
        const float height = (displaySize.y > 1.0f) ? displaySize.y : static_cast<float>(SCREEN_HEIGHT);
        return { width, height };
    }
    /**
     * @brief ステージ役割の列挙値に対応する表示ラベル一覧です。
     */
    const char* const kStageRoleLabels[] =
    {
        u8"通常",
        u8"開始・帰還地点",
        u8"拠点",
        u8"中継拠点",
    };

    /**
     * @brief ステージカテゴリの列挙値に対応する表示ラベル一覧です。
     */
    const char* const kStageCategoryLabels[] =
    {
        u8"平地接続(高)",
        u8"平地接続(低)",
        u8"崖あり",
        u8"高低差あり(高)",
        u8"高低差あり(低)",
        u8"水場",
        u8"接続不可",
    };

    /**
     * @brief 編集モードの切り替えUIに表示するラベル一覧です。
     */
    const char* const kEditModeLabels[] =
    {
        u8"地形編集",
        u8"ゲームオブジェクト配置",
    };

    /**
     * @brief 地形編集時の選択単位を表す表示ラベル一覧です。
     */
    const char* const kTerrainSelectionModeLabels[] =
    {
        u8"頂点",
        u8"セル",
    };

    /**
     * @brief 配置対象のゲームオブジェクト種別を表す表示ラベル一覧です。
     */
    const char* const kGridObjectToolLabels[] =
    {
        u8"採掘ポイント",
        u8"ロープ上端",
        u8"ロープ下端",
        u8"開始・帰還地点",
    };

    /**
     * @brief 方角の列挙値に対応する表示ラベル一覧です。
     */
    const char* const kDirectionLabels[] =
    {
        u8"北",
        u8"南",
        u8"東",
        u8"西",
    };

    /**
     * @brief 浮動小数点値を指定した最小値と最大値の範囲に収めます。
     * @param value 範囲内に収める対象の値です。
     * @param minValue 許容する最小値です。
     * @param maxValue 許容する最大値です。
     * @return 指定範囲に丸めた値です。
     */
    float ClampFloat(float value, float minValue, float maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    /**
     * @brief 非同期キーボード状態から修飾キーの押下状態を判定します。
     * @param virtualKey 判定対象の仮想キーコードです。
     * @return 指定キーが押下中の場合はtrueです。
     */
    bool IsAsyncModifierPressed(int virtualKey)
    {
        return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
    }

    /**
     * @brief 現在の押下状態と前回状態からショートカット入力の立ち上がりを判定します。
     * @param isPressed 現在フレームの押下状態です。
     * @param previousPressed 前回フレームの押下状態で、判定後に現在値へ更新されます。
     * @return 今フレームで新たに押された場合はtrueです。
     */
    bool IsShortcutTriggered(bool isPressed, bool& previousPressed)
    {
        const bool triggered = isPressed && !previousPressed;
        previousPressed = isPressed;
        return triggered;
    }

    /**
     * @brief エディタ操作用にShiftキーが押されているかを複数入力経路から判定します。
     * @param io 現在フレームのImGui入力状態です。
     * @return Shiftキー押下中の場合はtrueです。
     */
    bool IsEditorShiftPressed(const ImGuiIO& io)
    {
        return io.KeyShift ||
            IsRawKeyPress(VK_SHIFT) || IsRawKeyPress(VK_LSHIFT) || IsRawKeyPress(VK_RSHIFT) ||
            IsAsyncModifierPressed(VK_SHIFT) || IsAsyncModifierPressed(VK_LSHIFT) || IsAsyncModifierPressed(VK_RSHIFT);
    }

    /**
     * @brief エディタ操作用にCtrlキーが押されているかを複数入力経路から判定します。
     * @param io 現在フレームのImGui入力状態です。
     * @return Ctrlキー押下中の場合はtrueです。
     */
    bool IsEditorCtrlPressed(const ImGuiIO& io)
    {
        return io.KeyCtrl ||
            IsRawKeyPress(VK_CONTROL) || IsRawKeyPress(VK_LCONTROL) || IsRawKeyPress(VK_RCONTROL) ||
            IsAsyncModifierPressed(VK_CONTROL) || IsAsyncModifierPressed(VK_LCONTROL) || IsAsyncModifierPressed(VK_RCONTROL);
    }

    /**
     * @brief エディタ操作用にAltキーが押されているかを複数入力経路から判定します。
     * @param io 現在フレームのImGui入力状態です。
     * @return Altキー押下中の場合はtrueです。
     */
    bool IsEditorAltPressed(const ImGuiIO& io)
    {
        return io.KeyAlt ||
            IsRawKeyPress(VK_MENU) || IsRawKeyPress(VK_LMENU) || IsRawKeyPress(VK_RMENU) ||
            IsAsyncModifierPressed(VK_MENU) || IsAsyncModifierPressed(VK_LMENU) || IsAsyncModifierPressed(VK_RMENU);
    }

    /**
     * @brief 整数値を指定した最小値と最大値の範囲に収めます。
     * @param value 範囲内に収める対象の値です。
     * @param minValue 許容する最小値です。
     * @param maxValue 許容する最大値です。
     * @return 指定範囲に丸めた整数値です。
     */
    int ClampInt(int value, int minValue, int maxValue)
    {
        return std::max(minValue, std::min(value, maxValue));
    }

    /**
     * @brief ステージ役割の列挙値をUI選択用のインデックスへ変換します。
     * @param role 変換対象のステージ役割です。
     * @return 対応するコンボボックス用インデックスです。
     */
    int ToStageRoleIndex(NarakuPiece::StageRole role)
    {
        switch (role)
        {
        case NarakuPiece::StageRole::Normal:
            return 0;
        case NarakuPiece::StageRole::StartReturn:
            return 1;
        case NarakuPiece::StageRole::Base:
            return 2;
        case NarakuPiece::StageRole::Relay:
            return 3;
        default:
            return 0;
        }
    }

    /**
     * @brief UI選択用インデックスをステージ役割の列挙値へ変換します。
     * @param index 変換対象のコンボボックス用インデックスです。
     * @return 対応するステージ役割です。
     */
    NarakuPiece::StageRole FromStageRoleIndex(int index)
    {
        switch (index)
        {
        case 1:
            return NarakuPiece::StageRole::StartReturn;
        case 2:
            return NarakuPiece::StageRole::Base;
        case 3:
            return NarakuPiece::StageRole::Relay;
        case 0:
        default:
            return NarakuPiece::StageRole::Normal;
        }
    }

    /**
     * @brief ステージカテゴリの列挙値をUI選択用のインデックスへ変換します。
     * @param category 変換対象のステージカテゴリです。
     * @return 対応するコンボボックス用インデックスです。
     */
    int ToStageCategoryIndex(NarakuPiece::StageCategory category)
    {
        switch (category)
        {
        case NarakuPiece::StageCategory::PlainHigh:
            return 0;
        case NarakuPiece::StageCategory::PlainLow:
            return 1;
        case NarakuPiece::StageCategory::Cliff:
            return 2;
        case NarakuPiece::StageCategory::HeightHigh:
            return 3;
        case NarakuPiece::StageCategory::HeightLow:
            return 4;
        case NarakuPiece::StageCategory::Water:
            return 5;
        case NarakuPiece::StageCategory::Blocked:
            return 6;
        default:
            return 0;
        }
    }

    /**
     * @brief UI選択用インデックスをステージカテゴリの列挙値へ変換します。
     * @param index 変換対象のコンボボックス用インデックスです。
     * @return 対応するステージカテゴリです。
     */
    NarakuPiece::StageCategory FromStageCategoryIndex(int index)
    {
        switch (index)
        {
        case 0:
            return NarakuPiece::StageCategory::PlainHigh;
        case 2:
            return NarakuPiece::StageCategory::Cliff;
        case 3:
            return NarakuPiece::StageCategory::HeightHigh;
        case 4:
            return NarakuPiece::StageCategory::HeightLow;
        case 5:
            return NarakuPiece::StageCategory::Water;
        case 6:
            return NarakuPiece::StageCategory::Blocked;
        case 1:
        default:
            return NarakuPiece::StageCategory::PlainLow;
        }
    }

    /**
     * @brief バリデーション結果の重大度を表示用ラベルへ変換します。
     * @param severity 変換対象の重大度です。
     * @return 重大度に対応する日本語ラベルです。
     */
    const char* GetSeverityLabel(NarakuPiece::ValidationIssue::Severity severity)
    {
        switch (severity)
        {
        case NarakuPiece::ValidationIssue::Severity::Info:
            return u8"情報";
        case NarakuPiece::ValidationIssue::Severity::Warning:
            return u8"警告";
        case NarakuPiece::ValidationIssue::Severity::Error:
            return u8"エラー";
        default:
            return u8"不明";
        }
    }

    /**
     * @brief 方角の列挙値をUI選択用のインデックスへ変換します。
     * @param direction 変換対象の方角です。
     * @return 対応するコンボボックス用インデックスです。
     */
    int ToDirectionIndex(NarakuPiece::Direction direction)
    {
        switch (direction)
        {
        case NarakuPiece::Direction::North:
            return 0;
        case NarakuPiece::Direction::South:
            return 1;
        case NarakuPiece::Direction::East:
            return 2;
        case NarakuPiece::Direction::West:
            return 3;
        default:
            return 1;
        }
    }

    /**
     * @brief UI選択用インデックスを方角の列挙値へ変換します。
     * @param index 変換対象のコンボボックス用インデックスです。
     * @return 対応する方角です。
     */
    NarakuPiece::Direction FromDirectionIndex(int index)
    {
        switch (index)
        {
        case 0:
            return NarakuPiece::Direction::North;
        case 2:
            return NarakuPiece::Direction::East;
        case 3:
            return NarakuPiece::Direction::West;
        case 1:
        default:
            return NarakuPiece::Direction::South;
        }
    }
}

SceneNarakuPieceEditor::SceneNarakuPieceEditor()
    : m_piece(NarakuPiece::CreateDefaultPiece(NarakuPiece::SizePreset::Size16x16))
    , m_selectedX(0)
    , m_selectedZ(0)
    , m_selectedVertices{ VertexSelection{ 0, 0 } }
    , m_selectedCellX(0)
    , m_selectedCellZ(0)
    , m_selectedCells{ CellSelection{ 0, 0 } }
    , m_saveFileName(L"piece_0001.json")
    , m_validationDirty(true)
{
    SyncSaveFileNameInput();
    ReloadPieceHierarchyEntries();
    UpdateMainWindowTitle();
    ResetCamera();
    UpdateCameraMatrices();
    RefreshValidationIssues();
}

SceneNarakuPieceEditor::~SceneNarakuPieceEditor()
{
    ReleasePreviewRenderTarget();
}

void SceneNarakuPieceEditor::Update()
{
    HandleUndoRedoShortcuts();
    ImGuiIO& io = ImGui::GetIO();
    const bool deletePressed = !io.WantTextInput && !io.WantCaptureKeyboard && IsAsyncModifierPressed(VK_DELETE);
    const bool deleteTriggered = IsShortcutTriggered(deletePressed, m_prevDeletePressed);
    UpdateCamera();
    if (m_editMode == EditMode::Height)
    {
        if (deleteTriggered && m_terrainSelectionMode == TerrainSelectionMode::Cell && !m_selectedCells.empty())
        {
            PushUndoSnapshot();
            for (const CellSelection& selection : m_selectedCells)
            {
                if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                {
                    cell->deleted = true;
                }
            }
            m_validationDirty = true;
            SetMessage(u8"選択セルを削除しました");
        }
        UpdateHeightEditing();
    }
    else
    {
        if (deleteTriggered)
        {
            DeleteSelectedGridObject();
        }
        UpdateGridObjectEditing();
    }
    UpdateCameraMatrices();

    if (m_validationDirty)
    {
        RefreshValidationIssues();
    }
}

void SceneNarakuPieceEditor::Draw()
{
    RenderTerrainPreviewToTexture();
    DrawMainMenuBar();
    DrawEditorWindow();
    DrawPreviewWindow();
    DrawHeightGridWindow();
    DrawPieceHierarchyWindow();
    DrawSavePiecePopup();
}

int SceneNarakuPieceEditor::GetHeightIndex(int x, int z) const
{
    return z * m_piece.gridWidth + x;
}

bool SceneNarakuPieceEditor::IsValidVertex(int x, int z) const
{
    return x >= 0 && z >= 0 && x < m_piece.gridWidth && z < m_piece.gridDepth;
}

float SceneNarakuPieceEditor::GetHeight(int x, int z) const
{
    if (!IsValidVertex(x, z))
    {
        return 0.0f;
    }

    const int index = GetHeightIndex(x, z);
    if (index < 0 || index >= static_cast<int>(m_piece.heights.size()))
    {
        return 0.0f;
    }

    return m_piece.heights[static_cast<size_t>(index)];
}

void SceneNarakuPieceEditor::SetHeight(int x, int z, float height)
{
    if (!IsValidVertex(x, z))
    {
        return;
    }

    const int index = GetHeightIndex(x, z);
    if (index < 0 || index >= static_cast<int>(m_piece.heights.size()))
    {
        return;
    }

    m_piece.heights[static_cast<size_t>(index)] = ClampFloat(height, -10.0f, 10.0f);
    m_validationDirty = true;
}

XMFLOAT3 SceneNarakuPieceEditor::GetVertexWorldPosition(int x, int z) const
{
    const float offsetX = (static_cast<float>(m_piece.gridWidth - 1) * m_piece.cellSize) * 0.5f;
    const float offsetZ = (static_cast<float>(m_piece.gridDepth - 1) * m_piece.cellSize) * 0.5f;
    return
    {
        static_cast<float>(x) * m_piece.cellSize - offsetX,
        GetHeight(x, z),
        static_cast<float>(z) * m_piece.cellSize - offsetZ
    };
}

XMFLOAT3 SceneNarakuPieceEditor::GetCellWorldPosition(int cellX, int cellZ) const
{
    const XMFLOAT3 p00 = GetVertexWorldPosition(cellX, cellZ);
    const XMFLOAT3 p10 = GetVertexWorldPosition(cellX + 1, cellZ);
    const XMFLOAT3 p01 = GetVertexWorldPosition(cellX, cellZ + 1);
    const XMFLOAT3 p11 = GetVertexWorldPosition(cellX + 1, cellZ + 1);
    return
    {
        (p00.x + p10.x + p01.x + p11.x) * 0.25f,
        (p00.y + p10.y + p01.y + p11.y) * 0.25f,
        (p00.z + p10.z + p01.z + p11.z) * 0.25f
    };
}

bool SceneNarakuPieceEditor::IsValidCell(int cellX, int cellZ) const
{
    return cellX >= 0 && cellZ >= 0 &&
        cellX < std::max(0, m_piece.gridWidth - 1) &&
        cellZ < std::max(0, m_piece.gridDepth - 1);
}

int SceneNarakuPieceEditor::GetCellIndex(int cellX, int cellZ) const
{
    return cellZ * std::max(0, m_piece.gridWidth - 1) + cellX;
}

NarakuPiece::CellData* SceneNarakuPieceEditor::GetCellData(int cellX, int cellZ)
{
    if (!IsValidCell(cellX, cellZ))
    {
        return nullptr;
    }

    const int index = GetCellIndex(cellX, cellZ);
    if (index < 0 || index >= static_cast<int>(m_piece.cells.size()))
    {
        return nullptr;
    }

    return &m_piece.cells[static_cast<size_t>(index)];
}

const NarakuPiece::CellData* SceneNarakuPieceEditor::GetCellData(int cellX, int cellZ) const
{
    if (!IsValidCell(cellX, cellZ))
    {
        return nullptr;
    }

    const int index = GetCellIndex(cellX, cellZ);
    if (index < 0 || index >= static_cast<int>(m_piece.cells.size()))
    {
        return nullptr;
    }

    return &m_piece.cells[static_cast<size_t>(index)];
}

int SceneNarakuPieceEditor::FindMiningPointIndexByCell(int cellX, int cellZ) const
{
    for (size_t index = 0; index < m_piece.miningPoints.size(); ++index)
    {
        const NarakuPiece::MiningPointData& point = m_piece.miningPoints[index];
        if (point.cell.x == cellX && point.cell.z == cellZ)
        {
            return static_cast<int>(index);
        }
    }

    return -1;
}

std::string SceneNarakuPieceEditor::GenerateMiningPointId() const
{
    int maxNumber = 0;
    for (const NarakuPiece::MiningPointData& point : m_piece.miningPoints)
    {
        int number = 0;
        if (sscanf_s(point.id.c_str(), "mining_%d", &number) == 1)
        {
            maxNumber = std::max(maxNumber, number);
        }
    }

    char buffer[32] = {};
    std::snprintf(buffer, sizeof(buffer), "mining_%02d", maxNumber + 1);
    return buffer;
}

void SceneNarakuPieceEditor::ClearTerrainSelection()
{
    m_selectedVertices.clear();
    m_selectedCells.clear();
    m_selectedX = -1;
    m_selectedZ = -1;
    m_selectedCellX = -1;
    m_selectedCellZ = -1;
    m_draggingHeight = false;
    m_dragSelecting = false;
    m_selectionDragActive = false;
}

void SceneNarakuPieceEditor::ClearGridObjectSelection()
{
    m_selectedGridObjectKind = GridObjectKind::None;
    m_selectedMiningPointIndex = -1;
}

void SceneNarakuPieceEditor::SelectMiningPoint(int index)
{
    if (index < 0 || index >= static_cast<int>(m_piece.miningPoints.size()))
    {
        ClearGridObjectSelection();
        return;
    }

    m_selectedGridObjectKind = GridObjectKind::MiningPoint;
    m_selectedMiningPointIndex = index;
}

void SceneNarakuPieceEditor::SelectRope()
{
    m_selectedGridObjectKind = GridObjectKind::Rope;
    m_selectedMiningPointIndex = -1;
}

void SceneNarakuPieceEditor::SelectStartReturn()
{
    m_selectedGridObjectKind = GridObjectKind::StartReturn;
    m_selectedMiningPointIndex = -1;
}

bool SceneNarakuPieceEditor::CanPlaceGridObject(GridObjectTool tool, int cellX, int cellZ, std::string& outMessage) const
{
    const NarakuPiece::CellData* const cellData = GetCellData(cellX, cellZ);
    if (cellData == nullptr)
    {
        outMessage = u8"範囲内にセルがありません";
        return false;
    }

    if (cellData->deleted)
    {
        outMessage = u8"削除セルには配置できません";
        return false;
    }

    switch (tool)
    {
    case GridObjectTool::MiningPoint:
        if (!cellData->miningAllowed)
        {
            outMessage = u8"このセルには採掘ポイントを配置できません";
            return false;
        }
        return true;

    case GridObjectTool::RopeTop:
    case GridObjectTool::RopeBottom:
        if (!cellData->ropeAllowed)
        {
            outMessage = u8"このセルにはロープを配置できません";
            return false;
        }
        return true;

    case GridObjectTool::StartReturn:
        if (!cellData->walkable)
        {
            outMessage = u8"このセルには開始・帰還地点を配置できません";
            return false;
        }
        return true;

    default:
        outMessage.clear();
        return true;
    }
}

bool SceneNarakuPieceEditor::DeleteSelectedGridObject()
{
    switch (m_selectedGridObjectKind)
    {
    case GridObjectKind::MiningPoint:
        if (m_selectedMiningPointIndex < 0 || m_selectedMiningPointIndex >= static_cast<int>(m_piece.miningPoints.size()))
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.miningPoints.erase(m_piece.miningPoints.begin() + m_selectedMiningPointIndex);
        ClearGridObjectSelection();
        m_validationDirty = true;
        SetMessage(u8"採掘ポイントを削除しました");
        return true;

    case GridObjectKind::Rope:
        if (!m_piece.rope.enabled)
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.rope.enabled = false;
        m_validationDirty = true;
        SetMessage(u8"ロープを削除しました");
        return true;

    case GridObjectKind::StartReturn:
        if (!m_piece.startReturnCandidate.enabled)
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.startReturnCandidate.enabled = false;
        m_validationDirty = true;
        SetMessage(u8"開始・帰還地点を削除しました");
        return true;

    case GridObjectKind::None:
    default:
        return false;
    }
}


bool SceneNarakuPieceEditor::EnsurePreviewRenderTarget(unsigned int width, unsigned int height)
{
    const unsigned int safeWidth = (width > 0U) ? width : 1U;
    const unsigned int safeHeight = (height > 0U) ? height : 1U;
    if (m_previewRenderTarget != nullptr &&
        m_previewDepthStencil != nullptr &&
        m_previewRenderWidth == safeWidth &&
        m_previewRenderHeight == safeHeight)
    {
        return true;
    }

    ReleasePreviewRenderTarget();

    m_previewRenderTarget = new RenderTarget();
    if (FAILED(m_previewRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, safeWidth, safeHeight)))
    {
        ReleasePreviewRenderTarget();
        return false;
    }

    m_previewDepthStencil = new DepthStencil();
    if (FAILED(m_previewDepthStencil->Create(safeWidth, safeHeight, false)))
    {
        ReleasePreviewRenderTarget();
        return false;
    }

    m_previewRenderWidth = safeWidth;
    m_previewRenderHeight = safeHeight;
    return true;
}

void SceneNarakuPieceEditor::ReleasePreviewRenderTarget()
{
    SAFE_DELETE(m_previewDepthStencil);
    SAFE_DELETE(m_previewRenderTarget);
    m_previewRenderWidth = 0;
    m_previewRenderHeight = 0;
}

void SceneNarakuPieceEditor::DrawPreviewWindow()
{
    if (!m_showPreviewWindow)
    {
        m_previewImageHovered = false;
        m_previewImageTopLeft = {};
        m_previewImageScreenTopLeft = {};
        m_previewImageSize = {};
        return;
    }

    m_previewImageHovered = false;
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    const ImVec2 workSize = (viewport != nullptr) ? viewport->WorkSize : ImVec2(1280.0f, 720.0f);
    ApplySafeWindowPlacement(
        "3Dプレビュー",
        ImVec2(392.0f, 16.0f),
        ImVec2(std::max(520.0f, workSize.x - 408.0f), std::max(360.0f, workSize.y * 0.62f)));
    if (!ImGui::Begin(u8"3Dプレビュー", &m_showPreviewWindow))
    {
        m_previewImageTopLeft = {};
        m_previewImageScreenTopLeft = {};
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("3Dプレビュー");

    ImVec2 area = ImGui::GetContentRegionAvail();
    area.x = std::max(area.x, 320.0f);
    area.y = std::max(area.y, 220.0f);

    m_previewRequestWidth = static_cast<unsigned int>(std::max(1.0f, area.x));
    m_previewRequestHeight = static_cast<unsigned int>(std::max(1.0f, area.y));

    const ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
    if (m_previewRenderTarget != nullptr)
    {
        ImGui::Image(reinterpret_cast<ImTextureID>(m_previewRenderTarget->GetResource()), area);
        m_previewImageHovered = ImGui::IsItemHovered();
    }
    else
    {
        ImGui::InvisibleButton("##NarakuPiecePreviewPlaceholder", area);
        m_previewImageHovered = ImGui::IsItemHovered();
        ImDrawList* const drawList = ImGui::GetWindowDrawList();
        const ImVec2 rectMin = imageTopLeft;
        const ImVec2 rectMax(imageTopLeft.x + area.x, imageTopLeft.y + area.y);
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(10, 12, 14, 255));
        drawList->AddRect(rectMin, rectMax, IM_COL32(90, 98, 110, 255), 0.0f, 0, 1.5f);
    }

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    m_previewImageScreenTopLeft = { itemMin.x, itemMin.y };
    m_previewImageTopLeft = ConvertImGuiScreenToClient(itemMin);
    m_previewImageSize =
    {
        std::max(0.0f, itemMax.x - itemMin.x),
        std::max(0.0f, itemMax.y - itemMin.y)
    };

    DrawSelectionRectangle();
    ImGui::End();
}

void SceneNarakuPieceEditor::RenderTerrainPreviewToTexture()
{
    if (!EnsurePreviewRenderTarget(m_previewRequestWidth, m_previewRequestHeight))
    {
        return;
    }

    RenderTarget* previewTarget[] = { m_previewRenderTarget };
    SetRenderTargets(1, previewTarget, m_previewDepthStencil);

    const float clearColor[] = { 0.02f, 0.03f, 0.04f, 1.0f };
    m_previewRenderTarget->Clear(clearColor);
    m_previewDepthStencil->Clear();
    DrawTerrainPreview3D();

    RenderTarget* defaultTarget[] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
}

XMFLOAT2 SceneNarakuPieceEditor::GetPreviewViewportSize() const
{
    if (m_previewImageSize.x >= 1.0f && m_previewImageSize.y >= 1.0f)
    {
        return m_previewImageSize;
    }

    return GetEditorViewportSize();
}

XMFLOAT2 SceneNarakuPieceEditor::ConvertImGuiScreenToClient(const ImVec2& screenPos) const
{
    POINT client =
    {
        static_cast<LONG>(screenPos.x),
        static_cast<LONG>(screenPos.y)
    };

    HWND window = GetPreviewHostWindow();
    if (window != nullptr)
    {
        ::ScreenToClient(window, &client);
        return { static_cast<float>(client.x), static_cast<float>(client.y) };
    }

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    if (viewport != nullptr)
    {
        return
        {
            screenPos.x - viewport->Pos.x,
            screenPos.y - viewport->Pos.y
        };
    }

    return { static_cast<float>(client.x), static_cast<float>(client.y) };
}

XMFLOAT2 SceneNarakuPieceEditor::ConvertClientToImGuiScreen(const POINT& clientPos) const
{
    POINT screen = clientPos;
    HWND window = GetPreviewHostWindow();
    if (window != nullptr)
    {
        ::ClientToScreen(window, &screen);
        return { static_cast<float>(screen.x), static_cast<float>(screen.y) };
    }

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    if (viewport != nullptr)
    {
        return
        {
            static_cast<float>(clientPos.x) + viewport->Pos.x,
            static_cast<float>(clientPos.y) + viewport->Pos.y
        };
    }

    return { static_cast<float>(clientPos.x), static_cast<float>(clientPos.y) };
}

bool SceneNarakuPieceEditor::IsMouseInsidePreviewImage() const
{
    if (!m_showPreviewWindow || m_previewImageSize.x < 1.0f || m_previewImageSize.y < 1.0f)
    {
        return false;
    }

    const POINT mousePos = GetMousePosition();
    const float mouseX = static_cast<float>(mousePos.x);
    const float mouseY = static_cast<float>(mousePos.y);
    const float minX = m_previewImageTopLeft.x;
    const float minY = m_previewImageTopLeft.y;
    const float maxX = minX + m_previewImageSize.x;
    const float maxY = minY + m_previewImageSize.y;
    return mouseX >= minX && mouseX <= maxX && mouseY >= minY && mouseY <= maxY;
}

void SceneNarakuPieceEditor::DrawEditorWindow()
{
    ApplySafeWindowPlacement("奈落塔ピースエディタ", ImVec2(16.0f, 16.0f), ImVec2(360.0f, 260.0f));
    if (!ImGui::Begin(u8"奈落塔ピースエディタ"))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("奈落塔ピースエディタ");

    ImGui::SeparatorText(u8"共通操作");
    ImGui::TextUnformatted(u8"Alt+左ドラッグ: カメラ回転");
    ImGui::TextUnformatted(u8"中ドラッグ: パン");
    ImGui::TextUnformatted(u8"ホイール: ズーム");

    const bool canUndo = !m_undoStack.empty();
    const bool canRedo = !m_redoStack.empty();
    if (!canUndo)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(u8"元に戻す"))
    {
        UndoEdit();
    }
    if (!canUndo)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (!canRedo)
    {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button(u8"やり直す"))
    {
        RedoEdit();
    }
    if (!canRedo)
    {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::Text("%s %d / %d", u8"履歴", static_cast<int>(m_undoStack.size()), static_cast<int>(m_redoStack.size()));

    ImGui::SeparatorText(u8"カメラ");
    ImGui::DragFloat(u8"ヨー", &m_cameraYaw, 0.01f);
    ImGui::DragFloat(u8"ピッチ", &m_cameraPitch, 0.01f, kMinCameraPitch, kMaxCameraPitch, "%.3f");
    ImGui::DragFloat(u8"距離", &m_cameraDistance, 0.1f, kMinCameraDistance, kMaxCameraDistance, "%.2f");
    ImGui::DragFloat3(u8"注視点", &m_cameraTarget.x, 0.05f);
    ImGui::Checkbox(u8"Y反転", &m_invertOrbitY);
    if (ImGui::Button(u8"カメラリセット"))
    {
        ResetCamera();
        UpdateCameraMatrices();
    }

    ImGui::End();

    if (m_editMode != EditMode::Height ||
        m_terrainSelectionMode != TerrainSelectionMode::Vertex ||
        !m_showTerrainEditWindow)
    {
        m_heightDragFloatEditing = false;
    }

    DrawPieceBasicWindow();
    DrawPieceConnectionWindow();
    DrawTerrainEditWindow();
    DrawGridObjectPlacementWindow();
    DrawGridObjectSelectionWindow();
    DrawPieceFileAndValidationWindow();
}

void SceneNarakuPieceEditor::DrawMainMenuBar()
{
    if (!ImGui::BeginMainMenuBar())
    {
        return;
    }

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Save..."))
        {
            SyncSaveFileNameInput();
            m_saveAsDraft = true;
            ImGui::OpenPopup(u8"ピースを保存");
        }
        if (ImGui::MenuItem("Load..."))
        {
            OpenLoadPieceDialog();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View"))
    {
        ImGui::MenuItem(u8"基本情報", nullptr, &m_showPieceBasicWindow);
        ImGui::MenuItem(u8"接続設定", nullptr, &m_showPieceConnectionWindow);
        ImGui::MenuItem(u8"地形編集", nullptr, &m_showTerrainEditWindow);
        ImGui::MenuItem(u8"配置ツール", nullptr, &m_showGridObjectPlacementWindow);
        ImGui::MenuItem(u8"選択オブジェクト", nullptr, &m_showGridObjectSelectionWindow);
        ImGui::MenuItem(u8"保存・検証", nullptr, &m_showPieceFileAndValidationWindow);
        ImGui::MenuItem(u8"3Dプレビュー", nullptr, &m_showPreviewWindow);
        ImGui::MenuItem(u8"高さグリッド", nullptr, &m_showHeightGridWindow);
        ImGui::MenuItem(u8"小ステージHierarchy", nullptr, &m_showPieceHierarchyWindow);
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void SceneNarakuPieceEditor::ApplySafeWindowPlacement(const char* debugName, const ImVec2& defaultOffset, const ImVec2& defaultSize)
{
    (void)debugName;

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    if (viewport == nullptr)
    {
        ImGui::SetNextWindowPos(defaultOffset, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
        return;
    }

    const ImVec2 initialPos(viewport->WorkPos.x + defaultOffset.x, viewport->WorkPos.y + defaultOffset.y);
    ImGui::SetNextWindowPos(initialPos, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(defaultSize, ImGuiCond_FirstUseEver);
}

void SceneNarakuPieceEditor::EnsureWindowBelowMainMenuBar(const char* debugName)
{
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    if (viewport == nullptr || debugName == nullptr)
    {
        return;
    }

    if (std::find(m_menuBarAdjustedWindowNames.begin(), m_menuBarAdjustedWindowNames.end(), debugName) !=
        m_menuBarAdjustedWindowNames.end())
    {
        return;
    }

    const ImVec2 currentPos = ImGui::GetWindowPos();
    if (currentPos.y >= viewport->WorkPos.y)
    {
        return;
    }

    ImGui::SetWindowPos(ImVec2(currentPos.x, viewport->WorkPos.y), ImGuiCond_Always);
    m_menuBarAdjustedWindowNames.push_back(debugName);
}

void SceneNarakuPieceEditor::DrawPieceBasicWindow()
{
    if (!m_showPieceBasicWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("基本情報", ImVec2(16.0f, 292.0f), ImVec2(360.0f, 230.0f));
    if (!ImGui::Begin(u8"基本情報", &m_showPieceBasicWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("基本情報");

    char idBuffer[128] = {};
    std::snprintf(idBuffer, sizeof(idBuffer), "%s", m_piece.id.c_str());
    const bool idChanged = ImGui::InputText(u8"ID", idBuffer, sizeof(idBuffer));
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (idChanged)
    {
        m_piece.id = idBuffer;
        m_validationDirty = true;
    }

    char displayNameBuffer[128] = {};
    std::snprintf(displayNameBuffer, sizeof(displayNameBuffer), "%s", m_piece.displayName.c_str());
    const bool displayNameChanged = ImGui::InputText(u8"表示名", displayNameBuffer, sizeof(displayNameBuffer));
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (displayNameChanged)
    {
        m_piece.displayName = displayNameBuffer;
        m_validationDirty = true;
    }

    int abyssLayer = m_piece.abyssLayer;
    const bool abyssLayerChanged = ImGui::DragInt(u8"奈落階層", &abyssLayer, 0.1f, 1, 999);
    if (ImGui::IsItemActivated())
    {
        PushUndoSnapshot();
    }
    if (abyssLayerChanged)
    {
        m_piece.abyssLayer = (abyssLayer < 1) ? 1 : abyssLayer;
        m_validationDirty = true;
    }

    ImGui::Text("%s %s", u8"サイズプリセット:", NarakuPiece::ToString(m_piece.sizePreset));
    ImGui::Text("%s %dx%d", u8"グリッド:", m_piece.gridWidth, m_piece.gridDepth);

    int editModeIndex = static_cast<int>(m_editMode);
    if (ImGui::Combo(u8"編集モード", &editModeIndex, kEditModeLabels, IM_ARRAYSIZE(kEditModeLabels)))
    {
        m_editMode = static_cast<EditMode>(editModeIndex);
        ClearTerrainSelection();
        ClearGridObjectSelection();
        m_hoverCellX = -1;
        m_hoverCellZ = -1;
    }

    if (m_editMode == EditMode::Height)
    {
        int terrainSelectionModeIndex = static_cast<int>(m_terrainSelectionMode);
        if (ImGui::Combo(u8"地形選択", &terrainSelectionModeIndex, kTerrainSelectionModeLabels, IM_ARRAYSIZE(kTerrainSelectionModeLabels)))
        {
            m_terrainSelectionMode = static_cast<TerrainSelectionMode>(terrainSelectionModeIndex);
            ClearTerrainSelection();
        }
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawPieceConnectionWindow()
{
    if (!m_showPieceConnectionWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("接続設定", ImVec2(16.0f, 536.0f), ImVec2(360.0f, 230.0f));
    if (!ImGui::Begin(u8"接続設定", &m_showPieceConnectionWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("接続設定");

    int stageRoleIndex = ToStageRoleIndex(m_piece.stageRole);
    if (ImGui::Combo(u8"ステージ役割", &stageRoleIndex, kStageRoleLabels, IM_ARRAYSIZE(kStageRoleLabels)))
    {
        PushUndoSnapshot();
        m_piece.stageRole = FromStageRoleIndex(stageRoleIndex);
        m_validationDirty = true;
    }

    int stageCategoryIndex = ToStageCategoryIndex(m_piece.stageCategory);
    if (ImGui::Combo(u8"ステージカテゴリ", &stageCategoryIndex, kStageCategoryLabels, IM_ARRAYSIZE(kStageCategoryLabels)))
    {
        PushUndoSnapshot();
        m_piece.stageCategory = FromStageCategoryIndex(stageCategoryIndex);
        m_validationDirty = true;
    }

    const auto drawEdgeCategoryCombo = [&](const char* label, NarakuPiece::StageCategory& category)
    {
        int edgeIndex = ToStageCategoryIndex(category);
        if (ImGui::Combo(label, &edgeIndex, kStageCategoryLabels, IM_ARRAYSIZE(kStageCategoryLabels)))
        {
            PushUndoSnapshot();
            category = FromStageCategoryIndex(edgeIndex);
            m_validationDirty = true;
        }
    };

    ImGui::SeparatorText(u8"接続辺カテゴリ");
    drawEdgeCategoryCombo(u8"北", m_piece.edgeCategories.north);
    drawEdgeCategoryCombo(u8"南", m_piece.edgeCategories.south);
    drawEdgeCategoryCombo(u8"東", m_piece.edgeCategories.east);
    drawEdgeCategoryCombo(u8"西", m_piece.edgeCategories.west);

    ImGui::SeparatorText(u8"接続辺ロック");
    bool northLocked = m_piece.lockedEdges.north;
    if (ImGui::Checkbox(u8"北をロック", &northLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.north = northLocked;
        m_validationDirty = true;
    }
    bool southLocked = m_piece.lockedEdges.south;
    if (ImGui::Checkbox(u8"南をロック", &southLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.south = southLocked;
        m_validationDirty = true;
    }
    bool eastLocked = m_piece.lockedEdges.east;
    if (ImGui::Checkbox(u8"東をロック", &eastLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.east = eastLocked;
        m_validationDirty = true;
    }
    bool westLocked = m_piece.lockedEdges.west;
    if (ImGui::Checkbox(u8"西をロック", &westLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.west = westLocked;
        m_validationDirty = true;
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawTerrainEditWindow()
{
    if (!m_showTerrainEditWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("地形編集", ImVec2(392.0f, 16.0f), ImVec2(420.0f, 340.0f));
    if (!ImGui::Begin(u8"地形編集", &m_showTerrainEditWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("地形編集");

    if (m_editMode != EditMode::Height)
    {
        ImGui::TextUnformatted(u8"地形編集モードを高さ編集にすると操作できます。");
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    if (m_terrainSelectionMode == TerrainSelectionMode::Vertex)
    {
        ImGui::TextUnformatted(u8"左クリック: 単一選択");
        ImGui::TextUnformatted(u8"左ドラッグ: 選択頂点の高さ編集");
        ImGui::TextUnformatted(u8"Shift+左クリック/左ドラッグ: 追加選択");
        ImGui::TextUnformatted(u8"Ctrl+左クリック/左ドラッグ: トグル選択");
        ImGui::Checkbox(u8"高さグリッドを表示", &m_showHeightGridWindow);
        ImGui::Text("%s (%d, %d)", u8"主選択頂点", m_selectedX, m_selectedZ);
        ImGui::Text("%s %d", u8"選択数", static_cast<int>(m_selectedVertices.size()));

        float selectedHeight = GetHeight(m_selectedX, m_selectedZ);
        if (ImGui::DragFloat(u8"選択頂点の高さ", &selectedHeight, 0.05f, -10.0f, 10.0f, "%.2f"))
        {
            if (!m_heightDragFloatEditing)
            {
                PushUndoSnapshot();
                m_heightDragFloatEditing = true;
            }
            SetSelectedVerticesHeight(selectedHeight);
        }
        if (ImGui::IsItemDeactivatedAfterEdit())
        {
            m_heightDragFloatEditing = false;
        }
        else if (!ImGui::IsItemActive())
        {
            m_heightDragFloatEditing = false;
        }

        if (ImGui::Button(u8"主選択だけにする"))
        {
            KeepOnlyActiveVertexSelected();
        }

        if (ImGui::Button(u8"選択頂点を0に戻す"))
        {
            PushUndoSnapshot();
            for (const VertexSelection& selection : m_selectedVertices)
            {
                SetHeight(selection.x, selection.z, 0.0f);
            }
            SetMessage(u8"選択頂点の高さを0に戻しました");
        }

        if (ImGui::Button(u8"全頂点を0に戻す"))
        {
            PushUndoSnapshot();
            for (float& height : m_piece.heights)
            {
                height = 0.0f;
            }
            m_validationDirty = true;
            SetMessage(u8"全頂点の高さを0に戻しました");
        }
    }
    else
    {
        m_heightDragFloatEditing = false;
        ImGui::TextUnformatted(u8"左クリック: 単一選択");
        ImGui::TextUnformatted(u8"Shift+左クリック: 追加選択");
        ImGui::TextUnformatted(u8"Ctrl+左クリック: トグル選択");
        ImGui::TextUnformatted(u8"Shift+左ドラッグ: 範囲選択");
        ImGui::TextUnformatted(u8"Delete: 選択セルを削除");
        ImGui::Text("%s (%d, %d)", u8"主選択セル", m_selectedCellX, m_selectedCellZ);
        ImGui::Text("%s %d", u8"選択数", static_cast<int>(m_selectedCells.size()));

        if (!m_selectedCells.empty())
        {
            NarakuPiece::CellData* const primaryCell = GetCellData(m_selectedCellX, m_selectedCellZ);
            if (primaryCell != nullptr)
            {
                bool deleted = primaryCell->deleted;
                if (ImGui::Checkbox(u8"削除済みセル", &deleted))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->deleted = deleted;
                        }
                    }
                    m_validationDirty = true;
                }

                bool walkable = primaryCell->walkable;
                if (ImGui::Checkbox(u8"歩行可能", &walkable))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->walkable = walkable;
                        }
                    }
                    m_validationDirty = true;
                }

                bool ropeAllowed = primaryCell->ropeAllowed;
                if (ImGui::Checkbox(u8"ロープ設置可", &ropeAllowed))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->ropeAllowed = ropeAllowed;
                        }
                    }
                    m_validationDirty = true;
                }

                bool miningAllowed = primaryCell->miningAllowed;
                if (ImGui::Checkbox(u8"採掘ポイント設置可", &miningAllowed))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->miningAllowed = miningAllowed;
                        }
                    }
                    m_validationDirty = true;
                }

                bool enemySpawnAllowed = primaryCell->enemySpawnAllowed;
                if (ImGui::Checkbox(u8"敵スポーン可", &enemySpawnAllowed))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->enemySpawnAllowed = enemySpawnAllowed;
                        }
                    }
                    m_validationDirty = true;
                }

                int groundTextureId = std::max(0, primaryCell->groundTextureId);
                if (ImGui::DragInt(u8"地面テクスチャID", &groundTextureId, 0.1f, 0, 999))
                {
                    PushUndoSnapshot();
                    groundTextureId = std::max(0, groundTextureId);
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->groundTextureId = groundTextureId;
                        }
                    }
                    m_validationDirty = true;
                }

                if (ImGui::Button(u8"削除"))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->deleted = true;
                        }
                    }
                    m_validationDirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button(u8"復元"))
                {
                    PushUndoSnapshot();
                    for (const CellSelection& selection : m_selectedCells)
                    {
                        if (NarakuPiece::CellData* cell = GetCellData(selection.x, selection.z))
                        {
                            cell->deleted = false;
                        }
                    }
                    m_validationDirty = true;
                }
            }
        }
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawGridObjectPlacementWindow()
{
    if (!m_showGridObjectPlacementWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("配置ツール", ImVec2(16.0f, 780.0f), ImVec2(360.0f, 190.0f));
    if (!ImGui::Begin(u8"配置ツール", &m_showGridObjectPlacementWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("配置ツール");

    if (m_editMode != EditMode::GridObject)
    {
        ImGui::TextUnformatted(u8"編集モードをゲームオブジェクト配置にすると操作できます。");
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    int toolIndex = static_cast<int>(m_gridObjectTool);
    if (ImGui::Combo(u8"配置ツール", &toolIndex, kGridObjectToolLabels, IM_ARRAYSIZE(kGridObjectToolLabels)))
    {
        m_gridObjectTool = static_cast<GridObjectTool>(toolIndex);
    }
    ImGui::TextUnformatted(u8"左クリックでセルにゲームオブジェクトを配置または選択します。");
    ImGui::TextUnformatted(u8"採掘ポイントは同一セルに重複配置できません。");
    ImGui::TextUnformatted(u8"ロープ上端と下端はロープ用セルに配置します。");
    ImGui::TextUnformatted(u8"開始帰還候補は候補セルに配置します。");
    ImGui::Text("%s (%d, %d)", u8"ホバーセル", m_hoverCellX, m_hoverCellZ);

    if (m_gridObjectTool == GridObjectTool::MiningPoint)
    {
        ImGui::SeparatorText(u8"新規採掘ポイント設定");
        ImGui::SliderInt(u8"見た目タイプ", &m_newMiningVisualType, 0, 3);
        ImGui::Checkbox(u8"初回採取済み", &m_newMiningInitiallyRecorded);
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawGridObjectSelectionWindow()
{
    if (!m_showGridObjectSelectionWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("選択オブジェクト", ImVec2(392.0f, 372.0f), ImVec2(360.0f, 280.0f));
    if (!ImGui::Begin(u8"選択オブジェクト", &m_showGridObjectSelectionWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("選択オブジェクト");

    switch (m_selectedGridObjectKind)
    {
    case GridObjectKind::MiningPoint:
        if (m_selectedMiningPointIndex >= 0 &&
            m_selectedMiningPointIndex < static_cast<int>(m_piece.miningPoints.size()))
        {
            NarakuPiece::MiningPointData& point = m_piece.miningPoints[static_cast<size_t>(m_selectedMiningPointIndex)];
            ImGui::Text("%s (%d, %d)", u8"採掘ポイント", point.cell.x, point.cell.z);

            char miningIdBuffer[128] = {};
            std::snprintf(miningIdBuffer, sizeof(miningIdBuffer), "%s", point.id.c_str());
            const bool miningIdChanged = ImGui::InputText(u8"ID", miningIdBuffer, sizeof(miningIdBuffer));
            if (ImGui::IsItemActivated())
            {
                PushUndoSnapshot();
            }
            if (miningIdChanged)
            {
                point.id = miningIdBuffer;
                m_validationDirty = true;
            }

            int visualType = point.visualType;
            if (ImGui::SliderInt(u8"見た目タイプ", &visualType, 0, 3))
            {
                PushUndoSnapshot();
                point.visualType = visualType;
                m_validationDirty = true;
            }

            bool initiallyRecorded = point.initiallyRecorded;
            if (ImGui::Checkbox(u8"初期記録済み", &initiallyRecorded))
            {
                PushUndoSnapshot();
                point.initiallyRecorded = initiallyRecorded;
                m_validationDirty = true;
            }

            if (ImGui::Button(u8"採掘ポイントを削除"))
            {
                DeleteSelectedGridObject();
            }
        }
        else
        {
            ClearGridObjectSelection();
            ImGui::TextUnformatted(u8"採掘ポイントが未選択です");
        }
        break;

    case GridObjectKind::Rope:
    {
        bool enabled = m_piece.rope.enabled;
        if (ImGui::Checkbox(u8"ロープを有効化", &enabled))
        {
            PushUndoSnapshot();
            m_piece.rope.enabled = enabled;
            m_validationDirty = true;
        }
        ImGui::Text("%s (%d, %d)", u8"ロープ上端", m_piece.rope.top.x, m_piece.rope.top.z);
        ImGui::Text("%s (%d, %d)", u8"ロープ下端", m_piece.rope.bottom.x, m_piece.rope.bottom.z);
        if (ImGui::Button(u8"ロープを削除"))
        {
            DeleteSelectedGridObject();
        }
        break;
    }

    case GridObjectKind::StartReturn:
    {
        bool enabled = m_piece.startReturnCandidate.enabled;
        if (ImGui::Checkbox(u8"開始帰還候補を有効化", &enabled))
        {
            PushUndoSnapshot();
            m_piece.startReturnCandidate.enabled = enabled;
            m_validationDirty = true;
        }
        ImGui::Text("%s (%d, %d)", u8"セル", m_piece.startReturnCandidate.cell.x, m_piece.startReturnCandidate.cell.z);
        int facingIndex = ToDirectionIndex(m_piece.startReturnCandidate.facing);
        if (ImGui::Combo(u8"向き", &facingIndex, kDirectionLabels, IM_ARRAYSIZE(kDirectionLabels)))
        {
            PushUndoSnapshot();
            m_piece.startReturnCandidate.facing = FromDirectionIndex(facingIndex);
            m_validationDirty = true;
        }
        if (ImGui::Button(u8"開始帰還候補を削除"))
        {
            DeleteSelectedGridObject();
        }
        break;
    }

    case GridObjectKind::None:
    default:
        ImGui::TextUnformatted(u8"ゲームオブジェクトが未選択です");
        break;
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawPieceFileAndValidationWindow()
{
    if (!m_showPieceFileAndValidationWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("保存・検証", ImVec2(768.0f, 372.0f), ImVec2(420.0f, 280.0f));
    if (!ImGui::Begin(u8"保存・検証", &m_showPieceFileAndValidationWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("保存・検証");

    if (ImGui::Button(u8"検証を実行"))
    {
        RefreshValidationIssues();
    }

    ImGui::SeparatorText(u8"検証結果");
    if (m_validationIssues.empty())
    {
        ImGui::TextUnformatted(u8"問題はありません");
    }
    else
    {
        for (const NarakuPiece::ValidationIssue& issue : m_validationIssues)
        {
            ImVec4 color = ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
            if (issue.severity == NarakuPiece::ValidationIssue::Severity::Warning)
            {
                color = ImVec4(0.95f, 0.80f, 0.15f, 1.0f);
            }
            else if (issue.severity == NarakuPiece::ValidationIssue::Severity::Error)
            {
                color = ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
            }

            ImGui::TextColored(color, "[%s] %s", GetSeverityLabel(issue.severity), issue.message.c_str());
        }
    }

    ImGui::SeparatorText(u8"メッセージ");
    ImGui::TextWrapped("%s", m_message.empty() ? u8"メッセージはありません" : m_message.c_str());

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawSavePiecePopup()
{
    bool keepOpen = true;
    if (!ImGui::BeginPopupModal(u8"ピースを保存", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::TextUnformatted(u8"保存ファイル名");
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::InputText(u8"##SavePieceFileName", m_saveFileNameInput.data(), m_saveFileNameInput.size()))
    {
        CommitSaveFileNameInput();
    }

    if (ImGui::RadioButton(u8"下書き保存", m_saveAsDraft))
    {
        m_saveAsDraft = true;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(u8"完成保存", !m_saveAsDraft))
    {
        m_saveAsDraft = false;
    }

    const std::wstring saveTargetPath = GetCurrentSaveTargetPath();
    ImGui::SeparatorText(u8"保存先");
    ImGui::TextWrapped("%s", WideToUtf8(saveTargetPath).c_str());

    if (ImGui::Button(u8"保存", ImVec2(120.0f, 0.0f)))
    {
        if (SavePiece(m_saveAsDraft))
        {
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"キャンセル", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void SceneNarakuPieceEditor::SyncSaveFileNameInput()
{
    m_saveFileName = EnsureJsonFileName(m_saveFileName);
    std::fill(m_saveFileNameInput.begin(), m_saveFileNameInput.end(), '\0');

    const std::string fileNameUtf8 = WideToUtf8(m_saveFileName);
    const size_t copyLength = std::min(fileNameUtf8.size(), m_saveFileNameInput.size() - 1);
    std::copy_n(fileNameUtf8.data(), copyLength, m_saveFileNameInput.data());
    m_saveFileNameInput[copyLength] = '\0';
    UpdateMainWindowTitle();
}

void SceneNarakuPieceEditor::CommitSaveFileNameInput()
{
    m_saveFileName = EnsureJsonFileName(Utf8ToWide(m_saveFileNameInput.data()));
    UpdateMainWindowTitle();
}

void SceneNarakuPieceEditor::UpdateMainWindowTitle() const
{
    HWND mainWindow = ::GetActiveWindow();
    if (mainWindow == nullptr)
    {
        mainWindow = ::GetForegroundWindow();
    }
    if (mainWindow == nullptr)
    {
        return;
    }

    const std::wstring fileName = m_saveFileName.empty() ? L"(unnamed)" : m_saveFileName;
    const std::wstring title = L"NarakuProto - PieceEditor - " + fileName;
    ::SetWindowTextW(mainWindow, title.c_str());
}

std::wstring SceneNarakuPieceEditor::GetCurrentSaveTargetPath() const
{
    const std::wstring fileName = EnsureJsonFileName(m_saveFileName);
    return m_saveAsDraft
        ? NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, fileName)
        : NarakuPiece::MakeCompletedPiecePath(m_piece.abyssLayer, fileName);
}

bool SceneNarakuPieceEditor::SavePiece(bool saveAsDraft)
{
    CommitSaveFileNameInput();
    if (m_saveFileName.empty())
    {
        SetMessage(u8"保存ファイル名を入力してください");
        return false;
    }
    if (HasInvalidFileNameChar(m_saveFileName))
    {
        SetMessage(u8"保存ファイル名に使用できない文字が含まれています");
        return false;
    }

    if (!saveAsDraft)
    {
        RefreshValidationIssues();
        if (NarakuPiece::HasValidationError(m_validationIssues))
        {
            SetMessage(u8"検証エラーがあるため、完成保存を中止しました");
            return false;
        }
    }

    std::string error;
    const std::wstring savePath = saveAsDraft
        ? NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, m_saveFileName)
        : NarakuPiece::MakeCompletedPiecePath(m_piece.abyssLayer, m_saveFileName);
    if (!NarakuPiece::SavePieceData(m_piece, savePath, &error))
    {
        SetMessage(std::string(saveAsDraft ? u8"下書き保存失敗: " : u8"完成保存失敗: ") + error);
        return false;
    }

    SetMessage(saveAsDraft ? u8"下書き保存に成功しました" : u8"完成保存に成功しました");
    m_saveAsDraft = saveAsDraft;
    RegisterPieceHierarchyEntry(savePath, !saveAsDraft);
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"ピース保存には成功しましたがHierarchy登録ファイルの保存に失敗しました");
    }
    SyncSaveFileNameInput();
    return true;
}

bool SceneNarakuPieceEditor::LoadPieceFromPath(const std::wstring& path)
{
    NarakuPiece::PieceData loadedPiece;
    std::string error;
    if (!NarakuPiece::LoadPieceData(path, loadedPiece, &error))
    {
        SetMessage(std::string(u8"下書き読込失敗: ") + error);
        return false;
    }

    if (loadedPiece.sizePreset != NarakuPiece::SizePreset::Size16x16 ||
        loadedPiece.gridWidth != 16 ||
        loadedPiece.gridDepth != 16)
    {
        SetMessage(u8"16x16 以外のピースはこのエディターでは読込できません");
        return false;
    }

    ApplyLoadedPiece(loadedPiece);
    m_saveFileName = EnsureJsonFileName(GetFileNamePart(path));
    m_saveAsDraft = !IsCompletedPiecePath(path);
    SyncSaveFileNameInput();
    RegisterPieceHierarchyEntry(path, IsCompletedPiecePath(path));
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"ピース読込には成功しましたがHierarchy登録ファイルの保存に失敗しました");
        return true;
    }
    SetMessage(u8"下書き読込に成功しました");
    return true;
}

void SceneNarakuPieceEditor::ApplyLoadedPiece(const NarakuPiece::PieceData& loadedPiece)
{
    PushUndoSnapshot();
    m_piece = loadedPiece;
    ClearTerrainSelection();
    m_selectedX = ClampInt(0, 0, m_piece.gridWidth - 1);
    m_selectedZ = ClampInt(0, 0, m_piece.gridDepth - 1);
    m_selectedCellX = ClampInt(0, 0, std::max(0, m_piece.gridWidth - 2));
    m_selectedCellZ = ClampInt(0, 0, std::max(0, m_piece.gridDepth - 2));
    EnsureSelectionNotEmpty();
    EnsureCellSelectionValid();
    ClearGridObjectSelection();
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
    m_validationDirty = true;
    RefreshValidationIssues();
}

void SceneNarakuPieceEditor::OpenLoadPieceDialog()
{
    wchar_t filePath[MAX_PATH] = {};
    const std::wstring initialPath = NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, L"sample.json");
    const std::wstring initialDirectory = GetDirectoryPart(initialPath);

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = ::GetActiveWindow();
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = static_cast<DWORD>(std::size(filePath));
    ofn.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = L"json";

    if (!::GetOpenFileNameW(&ofn))
    {
        return;
    }

    LoadPieceFromPath(filePath);
}

void SceneNarakuPieceEditor::DrawPieceHierarchyWindow()
{
    if (!m_showPieceHierarchyWindow)
    {
        return;
    }

    ApplySafeWindowPlacement("小ステージHierarchy", ImVec2(1204.0f, 16.0f), ImVec2(320.0f, 300.0f));
    if (!ImGui::Begin(u8"小ステージHierarchy", &m_showPieceHierarchyWindow))
    {
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("小ステージHierarchy");

    if (ImGui::Button(u8"再読込"))
    {
        ReloadPieceHierarchyEntries();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"現在のピースを登録"))
    {
        RegisterCurrentPieceToHierarchy();
    }

    ImGui::Separator();
    if (m_pieceHierarchyEntries.empty())
    {
        ImGui::TextUnformatted(u8"登録済みの小ステージはありません");
        ImGui::End();
        return;
    }

    const std::wstring currentPath = ToLowerWide(NormalizePathSeparators(GetCurrentEditingPieceRelativePath()));
    for (const PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        const std::string label = std::string(entry.isCompleted ? u8"[完成] " : u8"[下書き] ") + WideToUtf8(entry.fileName);
        const bool isSelected = ToLowerWide(NormalizePathSeparators(entry.relativePath)) == currentPath;
        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            const std::wstring absolutePath = ResolvePieceHierarchyPath(entry.relativePath);
            if (!PathExists(absolutePath))
            {
                SetMessage(std::string(u8"読込失敗: ファイルが見つかりません: ") + WideToUtf8(entry.relativePath));
            }
            else
            {
                LoadPieceFromPath(absolutePath);
            }
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", WideToUtf8(entry.relativePath).c_str());
        }
    }

    ImGui::End();
}

std::wstring SceneNarakuPieceEditor::GetPieceHierarchyConfigPath() const
{
    return L"Assets/Naraku/Pieces/piece_hierarchy.cfg";
}

std::wstring SceneNarakuPieceEditor::NormalizePieceHierarchyPath(const std::wstring& path) const
{
    const std::wstring normalizedPath = NormalizePathSeparators(path);
    if (!IsAbsoluteWindowsPath(normalizedPath))
    {
        return normalizedPath;
    }

    std::wstring projectRoot = GetPieceHierarchyProjectRoot();
    const std::wstring absoluteLower = ToLowerWide(normalizedPath);
    std::wstring projectRootLower = ToLowerWide(projectRoot);
    if (!projectRootLower.empty() && projectRootLower.back() != L'/')
    {
        projectRoot += L"/";
        projectRootLower += L"/";
    }

    if (absoluteLower.find(projectRootLower) == 0)
    {
        return normalizedPath.substr(projectRoot.size());
    }

    return normalizedPath;
}

std::wstring SceneNarakuPieceEditor::ResolvePieceHierarchyPath(const std::wstring& relativePath) const
{
    const std::wstring normalizedPath = NormalizePathSeparators(relativePath);
    if (IsAbsoluteWindowsPath(normalizedPath))
    {
        return normalizedPath;
    }

    const std::wstring projectRoot = GetPieceHierarchyProjectRoot();
    if (normalizedPath.empty())
    {
        return projectRoot;
    }
    if (!projectRoot.empty() && projectRoot.back() == L'/')
    {
        return projectRoot + normalizedPath;
    }
    return projectRoot + L"/" + normalizedPath;
}

bool SceneNarakuPieceEditor::IsCompletedPiecePath(const std::wstring& path) const
{
    const std::wstring normalizedPath = ToLowerWide(NormalizePathSeparators(path));
    return normalizedPath.find(L"/completed/") != std::wstring::npos;
}

std::wstring SceneNarakuPieceEditor::GetCurrentEditingPieceRelativePath() const
{
    return NormalizePieceHierarchyPath(GetCurrentSaveTargetPath());
}

bool SceneNarakuPieceEditor::ReloadPieceHierarchyEntries()
{
    m_pieceHierarchyEntries.clear();
    const std::wstring configPath = ResolvePieceHierarchyPath(GetPieceHierarchyConfigPath());
    if (!PathExists(configPath))
    {
        SetMessage(u8"Hierarchy登録ファイルが未作成のため空一覧で開始しました");
        return true;
    }

    std::ifstream stream(configPath, std::ios::binary);
    if (!stream)
    {
        SetMessage(u8"Hierarchy登録ファイルを開けませんでした");
        return false;
    }

    std::string line;
    bool headerChecked = false;
    while (std::getline(stream, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }
        if (line.empty())
        {
            continue;
        }
        if (!headerChecked)
        {
            headerChecked = true;
            if (line == kPieceHierarchyHeader)
            {
                continue;
            }
        }

        std::istringstream lineStream(line);
        std::string fileNameUtf8;
        std::string relativePathUtf8;
        std::string completedFlag;
        if (!std::getline(lineStream, fileNameUtf8, '\t') ||
            !std::getline(lineStream, relativePathUtf8, '\t') ||
            !std::getline(lineStream, completedFlag))
        {
            continue;
        }

        const std::wstring relativePath = NormalizePathSeparators(Utf8ToWide(relativePathUtf8));
        const bool isCompleted = (completedFlag == "1");
        RegisterPieceHierarchyEntry(relativePath, isCompleted);
        const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(NormalizePieceHierarchyPath(relativePath)));
        for (PieceHierarchyEntry& registeredEntry : m_pieceHierarchyEntries)
        {
            if (ToLowerWide(NormalizePathSeparators(registeredEntry.relativePath)) == normalizedKey)
            {
                const std::wstring explicitFileName = Utf8ToWide(fileNameUtf8);
                registeredEntry.fileName = explicitFileName.empty() ? GetFileNamePart(relativePath) : explicitFileName;
                break;
            }
        }
    }

    SetMessage(u8"Hierarchy登録ファイルを再読込しました");
    return true;
}

bool SceneNarakuPieceEditor::SavePieceHierarchyEntries() const
{
    const std::wstring configPath = ResolvePieceHierarchyPath(GetPieceHierarchyConfigPath());
    if (!EnsureDirectoryExists(GetDirectoryPart(configPath)))
    {
        return false;
    }
    std::ofstream stream(configPath, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }

    stream << kPieceHierarchyHeader << "\n";
    for (const PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        stream << WideToUtf8(entry.fileName) << '\t'
            << WideToUtf8(entry.relativePath) << '\t'
            << (entry.isCompleted ? '1' : '0') << "\n";
    }
    return static_cast<bool>(stream);
}

bool SceneNarakuPieceEditor::RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted)
{
    const std::wstring normalizedPath = NormalizePieceHierarchyPath(path);
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(normalizedPath));
    const std::wstring fileName = EnsureJsonFileName(GetFileNamePart(normalizedPath));

    for (PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        if (ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey)
        {
            const bool changed = entry.fileName != fileName ||
                entry.relativePath != normalizedPath ||
                entry.isCompleted != isCompleted;
            entry.fileName = fileName;
            entry.relativePath = normalizedPath;
            entry.isCompleted = isCompleted;
            return changed;
        }
    }

    PieceHierarchyEntry entry;
    entry.fileName = fileName;
    entry.relativePath = normalizedPath;
    entry.isCompleted = isCompleted;
    m_pieceHierarchyEntries.push_back(entry);
    return true;
}

bool SceneNarakuPieceEditor::RegisterCurrentPieceToHierarchy()
{
    CommitSaveFileNameInput();
    if (m_saveFileName.empty())
    {
        SetMessage(u8"登録対象の保存ファイル名が未設定です");
        return false;
    }

    const std::wstring targetPath = GetCurrentSaveTargetPath();
    const bool changed = RegisterPieceHierarchyEntry(targetPath, !m_saveAsDraft);
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"Hierarchy登録ファイルの保存に失敗しました");
        return false;
    }

    SetMessage(changed ? u8"現在のピースをHierarchyへ登録しました" : u8"現在のピースのHierarchy登録情報を更新しました");
    return true;
}

void SceneNarakuPieceEditor::DrawHeightGridWindow()
{
    if (!m_showHeightGridWindow || m_terrainSelectionMode != TerrainSelectionMode::Vertex)
    {
        return;
    }

    ApplySafeWindowPlacement("高さグリッド", ImVec2(1204.0f, 332.0f), ImVec2(320.0f, 360.0f));
    if (!ImGui::Begin(u8"高さグリッド", &m_showHeightGridWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }
    EnsureWindowBelowMainMenuBar("高さグリッド");

    const ImGuiIO& io = ImGui::GetIO();
    ImGui::TextUnformatted(u8"Ctrl+クリック: トグル / Shift+クリック: 追加");
    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool shiftPressed = IsEditorShiftPressed(io);

    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            const bool isPrimarySelected = (x == m_selectedX && z == m_selectedZ);
            const bool isMultiSelected = IsVertexSelected(x, z);
            const float height = GetHeight(x, z);
            const bool hasHeight = std::fabs(height) > 0.001f;

            ImVec4 buttonColor = ImVec4(0.20f, 0.20f, 0.22f, 1.0f);
            ImVec4 hoveredColor = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
            ImVec4 activeColor = ImVec4(0.35f, 0.35f, 0.40f, 1.0f);
            if (hasHeight)
            {
                buttonColor = ImVec4(0.20f, 0.35f, 0.30f, 1.0f);
                hoveredColor = ImVec4(0.25f, 0.45f, 0.38f, 1.0f);
                activeColor = ImVec4(0.28f, 0.52f, 0.43f, 1.0f);
            }
            if (isMultiSelected)
            {
                buttonColor = ImVec4(0.22f, 0.42f, 0.78f, 1.0f);
                hoveredColor = ImVec4(0.30f, 0.50f, 0.88f, 1.0f);
                activeColor = ImVec4(0.18f, 0.36f, 0.70f, 1.0f);
            }
            if (isPrimarySelected)
            {
                buttonColor = ImVec4(0.85f, 0.55f, 0.15f, 1.0f);
                hoveredColor = ImVec4(0.92f, 0.64f, 0.22f, 1.0f);
                activeColor = ImVec4(0.98f, 0.72f, 0.28f, 1.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Button, buttonColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoveredColor);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, activeColor);

            char buttonLabel[16] = {};
            std::snprintf(buttonLabel, sizeof(buttonLabel), "%d,%d", x, z);
            if (ImGui::Button(buttonLabel, ImVec2(44.0f, 24.0f)))
            {
                SelectVertexFromInput(x, z, ctrlPressed, shiftPressed);
            }

            ImGui::PopStyleColor(3);
            if (x + 1 < m_piece.gridWidth)
            {
                ImGui::SameLine();
            }
        }
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawTerrainPreview3D() const
{
    XMFLOAT4X4 world = {};
    XMFLOAT4X4 view = {};
    XMFLOAT4X4 projection = {};

    XMStoreFloat4x4(&world, XMMatrixTranspose(XMMatrixIdentity()));
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);

    const XMFLOAT4 axisColor = { 0.25f, 0.25f, 0.28f, 1.0f };
    const XMFLOAT4 selectedColor = { 0.95f, 0.70f, 0.20f, 1.0f };
    const XMFLOAT4 multiSelectedColor = { 0.30f, 0.55f, 0.95f, 1.0f };
    const XMFLOAT4 raisedColor = { 0.30f, 0.75f, 0.55f, 1.0f };
    const XMFLOAT4 flatColor = { 0.65f, 0.65f, 0.68f, 1.0f };
    const XMFLOAT4 cellHoverColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT4 cellSelectedColor = { 1.0f, 0.85f, 0.20f, 1.0f };
    const XMFLOAT4 cellDeletedColor = { 0.85f, 0.40f, 0.40f, 1.0f };
    const XMFLOAT4 cellBlockedColor = { 0.95f, 0.15f, 0.15f, 1.0f };

    const float extentX = (static_cast<float>(m_piece.gridWidth - 1) * m_piece.cellSize) * 0.5f;
    const float extentZ = (static_cast<float>(m_piece.gridDepth - 1) * m_piece.cellSize) * 0.5f;
    Geometory::AddLine({ -extentX - 2.0f, 0.0f, 0.0f }, { extentX + 2.0f, 0.0f, 0.0f }, axisColor);
    Geometory::AddLine({ 0.0f, 0.0f, -extentZ - 2.0f }, { 0.0f, 0.0f, extentZ + 2.0f }, axisColor);

    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            const XMFLOAT3 current = GetVertexWorldPosition(x, z);
            const bool isPrimarySelected = (x == m_selectedX && z == m_selectedZ);
            const bool isMultiSelected = IsVertexSelected(x, z);
            const bool hasHeight = std::fabs(current.y) > 0.001f;
            const XMFLOAT4 lineColor = isPrimarySelected ? selectedColor :
                (isMultiSelected ? multiSelectedColor : (hasHeight ? raisedColor : flatColor));

            if (x + 1 < m_piece.gridWidth)
            {
                Geometory::AddLine(current, GetVertexWorldPosition(x + 1, z), lineColor);
            }
            if (z + 1 < m_piece.gridDepth)
            {
                Geometory::AddLine(current, GetVertexWorldPosition(x, z + 1), lineColor);
            }
            if (isMultiSelected)
            {
                const XMFLOAT4 markerColor = isPrimarySelected ? selectedColor : multiSelectedColor;
                Geometory::AddLine(
                    { current.x, current.y + 0.1f, current.z },
                    { current.x, current.y + kSelectionMarkerHeight, current.z },
                    markerColor);
            }
        }
    }

    for (int cellZ = 0; cellZ < m_piece.gridDepth - 1; ++cellZ)
    {
        for (int cellX = 0; cellX < m_piece.gridWidth - 1; ++cellX)
        {
            const NarakuPiece::CellData* const cellData = GetCellData(cellX, cellZ);
            if (cellData == nullptr)
            {
                continue;
            }

            const XMFLOAT3 p00 = GetVertexWorldPosition(cellX, cellZ);
            const XMFLOAT3 p10 = GetVertexWorldPosition(cellX + 1, cellZ);
            const XMFLOAT3 p01 = GetVertexWorldPosition(cellX, cellZ + 1);
            const XMFLOAT3 p11 = GetVertexWorldPosition(cellX + 1, cellZ + 1);
            const auto raise = [](const XMFLOAT3& pos)
            {
                return XMFLOAT3{ pos.x, pos.y + kCellOverlayYOffset, pos.z };
            };
            const XMFLOAT3 e00 = raise(p00);
            const XMFLOAT3 e10 = raise(p10);
            const XMFLOAT3 e01 = raise(p01);
            const XMFLOAT3 e11 = raise(p11);
            const XMFLOAT3 center = GetCellWorldPosition(cellX, cellZ);
            const XMFLOAT3 raisedCenter = { center.x, center.y + kCellOverlayYOffset, center.z };

            if (cellData->deleted)
            {
                Geometory::AddLine(e00, e11, cellDeletedColor);
                Geometory::AddLine(e10, e01, cellDeletedColor);
            }
            if (!cellData->walkable)
            {
                Geometory::AddLine(
                    { raisedCenter.x - 0.30f, raisedCenter.y, raisedCenter.z },
                    { raisedCenter.x + 0.30f, raisedCenter.y, raisedCenter.z },
                    cellBlockedColor);
                Geometory::AddLine(
                    { raisedCenter.x, raisedCenter.y, raisedCenter.z - 0.30f },
                    { raisedCenter.x, raisedCenter.y, raisedCenter.z + 0.30f },
                    cellBlockedColor);
            }
            if (IsCellSelected(cellX, cellZ))
            {
                Geometory::AddLine(e00, e10, cellSelectedColor);
                Geometory::AddLine(e10, e11, cellSelectedColor);
                Geometory::AddLine(e11, e01, cellSelectedColor);
                Geometory::AddLine(e01, e00, cellSelectedColor);
            }
            else if (cellX == m_hoverCellX && cellZ == m_hoverCellZ)
            {
                Geometory::AddLine(e00, e10, cellHoverColor);
                Geometory::AddLine(e10, e11, cellHoverColor);
                Geometory::AddLine(e11, e01, cellHoverColor);
                Geometory::AddLine(e01, e00, cellHoverColor);
            }
        }
    }

    Geometory::DrawLines();

    for (const VertexSelection& selection : m_selectedVertices)
    {
        const XMFLOAT3 selectedPos = GetVertexWorldPosition(selection.x, selection.z);
        const XMFLOAT3 boxScale = (selection.x == m_selectedX && selection.z == m_selectedZ)
            ? XMFLOAT3{ 0.45f, 0.45f, 0.45f }
            : XMFLOAT3{ 0.25f, 0.25f, 0.25f };
        DrawDebugBox3D({ selectedPos.x, selectedPos.y + 0.2f, selectedPos.z }, boxScale);
    }

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);

    static const XMFLOAT4 kMiningColors[] =
    {
        { 0.30f, 0.90f, 0.95f, 1.0f },
        { 0.25f, 0.95f, 0.45f, 1.0f },
        { 0.95f, 0.75f, 0.25f, 1.0f },
        { 0.95f, 0.45f, 0.70f, 1.0f },
    };

    for (size_t index = 0; index < m_piece.miningPoints.size(); ++index)
    {
        const NarakuPiece::MiningPointData& point = m_piece.miningPoints[index];
        if (!IsValidCell(point.cell.x, point.cell.z))
        {
            continue;
        }

        const XMFLOAT3 center = GetCellWorldPosition(point.cell.x, point.cell.z);
        const XMFLOAT4 color = kMiningColors[ClampInt(point.visualType, 0, 3)];
        const XMFLOAT3 scale = (m_selectedGridObjectKind == GridObjectKind::MiningPoint &&
            m_selectedMiningPointIndex == static_cast<int>(index))
            ? XMFLOAT3{ 0.60f, 0.60f, 0.60f }
            : XMFLOAT3{ 0.38f, 0.38f, 0.38f };
        DrawDebugWireBox3D({ center.x, center.y + 0.25f, center.z }, scale, color);
        Geometory::AddLine({ center.x, center.y, center.z }, { center.x, center.y + 0.9f, center.z }, color);
    }

    if (m_piece.rope.enabled && IsValidCell(m_piece.rope.top.x, m_piece.rope.top.z) && IsValidCell(m_piece.rope.bottom.x, m_piece.rope.bottom.z))
    {
        const XMFLOAT4 ropeColor = (m_selectedGridObjectKind == GridObjectKind::Rope)
            ? XMFLOAT4{ 1.0f, 0.82f, 0.35f, 1.0f }
            : XMFLOAT4{ 0.95f, 0.55f, 0.25f, 1.0f };
        const XMFLOAT3 top = GetCellWorldPosition(m_piece.rope.top.x, m_piece.rope.top.z);
        const XMFLOAT3 bottom = GetCellWorldPosition(m_piece.rope.bottom.x, m_piece.rope.bottom.z);
        Geometory::AddLine({ top.x, top.y, top.z }, { top.x, top.y + 1.2f, top.z }, ropeColor);
        Geometory::AddLine({ bottom.x, bottom.y, bottom.z }, { bottom.x, bottom.y + 1.2f, bottom.z }, ropeColor);
        Geometory::AddLine({ top.x, top.y + 1.2f, top.z }, { bottom.x, bottom.y + 1.2f, bottom.z }, ropeColor);
    }

    if (m_piece.startReturnCandidate.enabled && IsValidCell(m_piece.startReturnCandidate.cell.x, m_piece.startReturnCandidate.cell.z))
    {
        const XMFLOAT4 startColor = (m_selectedGridObjectKind == GridObjectKind::StartReturn)
            ? XMFLOAT4{ 0.95f, 0.95f, 0.40f, 1.0f }
            : XMFLOAT4{ 0.90f, 0.25f, 0.85f, 1.0f };
        const XMFLOAT3 center = GetCellWorldPosition(
            m_piece.startReturnCandidate.cell.x,
            m_piece.startReturnCandidate.cell.z);
        DrawDebugWireBox3D({ center.x, center.y + 0.4f, center.z }, { 0.72f, 0.72f, 0.72f }, startColor);
        Geometory::AddLine({ center.x - 0.5f, center.y + 0.1f, center.z }, { center.x + 0.5f, center.y + 0.1f, center.z }, startColor);
        Geometory::AddLine({ center.x, center.y + 0.1f, center.z - 0.5f }, { center.x, center.y + 0.1f, center.z + 0.5f }, startColor);
    }

    if (m_editMode == EditMode::GridObject && IsValidCell(m_hoverCellX, m_hoverCellZ))
    {
        const XMFLOAT4 hoverColor = { 1.0f, 1.0f, 1.0f, 0.95f };
        const XMFLOAT3 p00 = GetVertexWorldPosition(m_hoverCellX, m_hoverCellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(m_hoverCellX + 1, m_hoverCellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(m_hoverCellX, m_hoverCellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(m_hoverCellX + 1, m_hoverCellZ + 1);
        Geometory::AddLine(p00, p10, hoverColor);
        Geometory::AddLine(p10, p11, hoverColor);
        Geometory::AddLine(p11, p01, hoverColor);
        Geometory::AddLine(p01, p00, hoverColor);
    }

    Geometory::DrawLines();
}

bool SceneNarakuPieceEditor::IsVertexSelected(int x, int z) const
{
    return std::any_of(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [&](const VertexSelection& selection)
        {
            return selection.x == x && selection.z == z;
        });
}

bool SceneNarakuPieceEditor::IsCellSelected(int cellX, int cellZ) const
{
    return std::any_of(
        m_selectedCells.begin(),
        m_selectedCells.end(),
        [&](const CellSelection& selection)
        {
            return selection.x == cellX && selection.z == cellZ;
        });
}

void SceneNarakuPieceEditor::SelectSingleVertex(int x, int z)
{
    if (!IsValidVertex(x, z))
    {
        return;
    }

    m_selectedX = x;
    m_selectedZ = z;
    m_selectedVertices.clear();
    m_selectedVertices.push_back({ x, z });
}

void SceneNarakuPieceEditor::AddSelectedVertex(int x, int z)
{
    if (!IsValidVertex(x, z) || IsVertexSelected(x, z))
    {
        return;
    }

    m_selectedVertices.push_back({ x, z });
}

void SceneNarakuPieceEditor::ToggleSelectedVertex(int x, int z)
{
    if (!IsValidVertex(x, z))
    {
        return;
    }

    const auto it = std::find_if(
        m_selectedVertices.begin(),
        m_selectedVertices.end(),
        [&](const VertexSelection& selection)
        {
            return selection.x == x && selection.z == z;
        });

    if (it != m_selectedVertices.end())
    {
        if (m_selectedVertices.size() == 1)
        {
            return;
        }

        m_selectedVertices.erase(it);
    }
    else
    {
        m_selectedVertices.push_back({ x, z });
    }

    EnsureSelectionNotEmpty();
}

void SceneNarakuPieceEditor::SelectSingleCell(int cellX, int cellZ)
{
    if (!IsValidCell(cellX, cellZ))
    {
        return;
    }

    m_selectedCellX = cellX;
    m_selectedCellZ = cellZ;
    m_selectedCells.clear();
    m_selectedCells.push_back({ cellX, cellZ });
}

void SceneNarakuPieceEditor::AddSelectedCell(int cellX, int cellZ)
{
    if (!IsValidCell(cellX, cellZ) || IsCellSelected(cellX, cellZ))
    {
        return;
    }

    m_selectedCells.push_back({ cellX, cellZ });
}

void SceneNarakuPieceEditor::ToggleSelectedCell(int cellX, int cellZ)
{
    if (!IsValidCell(cellX, cellZ))
    {
        return;
    }

    const auto it = std::find_if(
        m_selectedCells.begin(),
        m_selectedCells.end(),
        [&](const CellSelection& selection)
        {
            return selection.x == cellX && selection.z == cellZ;
        });

    if (it != m_selectedCells.end())
    {
        if (m_selectedCells.size() == 1)
        {
            return;
        }
        m_selectedCells.erase(it);
    }
    else
    {
        m_selectedCells.push_back({ cellX, cellZ });
    }

    EnsureCellSelectionValid();
}

void SceneNarakuPieceEditor::SelectCellFromInput(int cellX, int cellZ, bool ctrlPressed, bool shiftPressed)
{
    if (!IsValidCell(cellX, cellZ))
    {
        return;
    }

    m_selectedCellX = cellX;
    m_selectedCellZ = cellZ;

    if (ctrlPressed)
    {
        ToggleSelectedCell(cellX, cellZ);
    }
    else if (shiftPressed)
    {
        AddSelectedCell(cellX, cellZ);
    }
    else
    {
        SelectSingleCell(cellX, cellZ);
    }

    EnsureCellSelectionValid();
}

void SceneNarakuPieceEditor::SelectVertexFromInput(int x, int z, bool ctrlPressed, bool shiftPressed)
{
    if (!IsValidVertex(x, z))
    {
        return;
    }

    m_selectedX = x;
    m_selectedZ = z;

    if (ctrlPressed)
    {
        ToggleSelectedVertex(x, z);
    }
    else if (shiftPressed)
    {
        AddSelectedVertex(x, z);
    }
    else
    {
        SelectSingleVertex(x, z);
    }

    EnsureSelectionNotEmpty();
}

void SceneNarakuPieceEditor::DrawSelectionRectangle() const
{
    if (!m_dragSelecting || !m_selectionDragActive)
    {
        return;
    }

    const long minClientX = std::min(m_selectionDragStart.x, m_selectionDragCurrent.x);
    const long minClientY = std::min(m_selectionDragStart.y, m_selectionDragCurrent.y);
    const long maxClientX = std::max(m_selectionDragStart.x, m_selectionDragCurrent.x);
    const long maxClientY = std::max(m_selectionDragStart.y, m_selectionDragCurrent.y);
    const XMFLOAT2 minPoint = ConvertClientToImGuiScreen({ minClientX, minClientY });
    const XMFLOAT2 maxPoint = ConvertClientToImGuiScreen({ maxClientX, maxClientY });

    ImDrawList* const drawList = ImGui::GetForegroundDrawList();
    drawList->AddRectFilled(
        ImVec2(minPoint.x, minPoint.y),
        ImVec2(maxPoint.x, maxPoint.y),
        IM_COL32(80, 150, 255, 48));
    drawList->AddRect(
        ImVec2(minPoint.x, minPoint.y),
        ImVec2(maxPoint.x, maxPoint.y),
        IM_COL32(80, 150, 255, 220),
        0.0f,
        0,
        1.5f);
}

std::vector<SceneNarakuPieceEditor::VertexSelection> SceneNarakuPieceEditor::CollectVerticesInScreenRect(POINT start, POINT end) const
{
    std::vector<VertexSelection> vertices;
    const float minX = static_cast<float>(std::min(start.x, end.x));
    const float minY = static_cast<float>(std::min(start.y, end.y));
    const float maxX = static_cast<float>(std::max(start.x, end.x));
    const float maxY = static_cast<float>(std::max(start.y, end.y));

    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(GetVertexWorldPosition(x, z), screen))
            {
                continue;
            }

            if (screen.x < minX || screen.x > maxX || screen.y < minY || screen.y > maxY)
            {
                continue;
            }

            vertices.push_back({ x, z });
        }
    }

    return vertices;
}

std::vector<SceneNarakuPieceEditor::CellSelection> SceneNarakuPieceEditor::CollectCellsInScreenRect(POINT start, POINT end) const
{
    std::vector<CellSelection> cells;
    const float minX = static_cast<float>(std::min(start.x, end.x));
    const float minY = static_cast<float>(std::min(start.y, end.y));
    const float maxX = static_cast<float>(std::max(start.x, end.x));
    const float maxY = static_cast<float>(std::max(start.y, end.y));

    for (int z = 0; z < m_piece.gridDepth - 1; ++z)
    {
        for (int x = 0; x < m_piece.gridWidth - 1; ++x)
        {
            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(GetCellWorldPosition(x, z), screen))
            {
                continue;
            }
            if (screen.x < minX || screen.x > maxX || screen.y < minY || screen.y > maxY)
            {
                continue;
            }
            cells.push_back({ x, z });
        }
    }

    return cells;
}

void SceneNarakuPieceEditor::ApplyRectangleSelection(const std::vector<VertexSelection>& vertices, bool ctrlPressed, bool shiftPressed)
{
    if (vertices.empty())
    {
        SetMessage(u8"範囲内に頂点がありません");
        return;
    }

    const VertexSelection& primary = vertices.front();
    m_selectedX = primary.x;
    m_selectedZ = primary.z;

    if (ctrlPressed)
    {
        for (const VertexSelection& vertex : vertices)
        {
            ToggleSelectedVertex(vertex.x, vertex.z);
        }
    }
    else if (shiftPressed)
    {
        for (const VertexSelection& vertex : vertices)
        {
            AddSelectedVertex(vertex.x, vertex.z);
        }
    }
    else
    {
        m_selectedVertices = vertices;
    }

    EnsureSelectionNotEmpty();
}

void SceneNarakuPieceEditor::ApplyCellRectangleSelection(const std::vector<CellSelection>& cells, bool ctrlPressed, bool shiftPressed)
{
    if (cells.empty())
    {
        SetMessage(u8"範囲内にセルがありません");
        return;
    }

    const CellSelection& primary = cells.front();
    m_selectedCellX = primary.x;
    m_selectedCellZ = primary.z;

    if (ctrlPressed)
    {
        for (const CellSelection& cell : cells)
        {
            ToggleSelectedCell(cell.x, cell.z);
        }
    }
    else if (shiftPressed)
    {
        for (const CellSelection& cell : cells)
        {
            AddSelectedCell(cell.x, cell.z);
        }
    }
    else
    {
        m_selectedCells = cells;
    }

    EnsureCellSelectionValid();
}

void SceneNarakuPieceEditor::KeepOnlyActiveVertexSelected()
{
    if (!IsValidVertex(m_selectedX, m_selectedZ))
    {
        return;
    }

    m_selectedVertices.clear();
    m_selectedVertices.push_back({ m_selectedX, m_selectedZ });
}

void SceneNarakuPieceEditor::EnsureSelectionNotEmpty()
{
    if (!IsValidVertex(m_selectedX, m_selectedZ))
    {
        m_selectedX = ClampInt(m_selectedX, 0, std::max(0, m_piece.gridWidth - 1));
        m_selectedZ = ClampInt(m_selectedZ, 0, std::max(0, m_piece.gridDepth - 1));
    }

    std::vector<VertexSelection> validSelections;
    validSelections.reserve(m_selectedVertices.size());
    for (const VertexSelection& selection : m_selectedVertices)
    {
        if (!IsValidVertex(selection.x, selection.z))
        {
            continue;
        }

        const bool alreadyExists = std::any_of(
            validSelections.begin(),
            validSelections.end(),
            [&](const VertexSelection& existing)
            {
                return existing.x == selection.x && existing.z == selection.z;
            });
        if (!alreadyExists)
        {
            validSelections.push_back(selection);
        }
    }

    m_selectedVertices = std::move(validSelections);
    if (m_selectedVertices.empty())
    {
        m_selectedVertices.push_back({ m_selectedX, m_selectedZ });
    }

    if (!IsVertexSelected(m_selectedX, m_selectedZ))
    {
        const VertexSelection& fallback = m_selectedVertices.back();
        m_selectedX = fallback.x;
        m_selectedZ = fallback.z;
    }
}

void SceneNarakuPieceEditor::ApplyHeightDeltaToSelectedVertices(float delta)
{
    if (std::fabs(delta) <= 0.0f)
    {
        return;
    }

    for (const VertexSelection& selection : m_selectedVertices)
    {
        SetHeight(selection.x, selection.z, GetHeight(selection.x, selection.z) + delta);
    }
}

void SceneNarakuPieceEditor::SetSelectedVerticesHeight(float height)
{
    for (const VertexSelection& selection : m_selectedVertices)
    {
        SetHeight(selection.x, selection.z, height);
    }
}

void SceneNarakuPieceEditor::EnsureCellSelectionValid()
{
    if (!IsValidCell(m_selectedCellX, m_selectedCellZ))
    {
        m_selectedCellX = ClampInt(m_selectedCellX, 0, std::max(0, m_piece.gridWidth - 2));
        m_selectedCellZ = ClampInt(m_selectedCellZ, 0, std::max(0, m_piece.gridDepth - 2));
    }

    std::vector<CellSelection> validSelections;
    validSelections.reserve(m_selectedCells.size());
    for (const CellSelection& selection : m_selectedCells)
    {
        if (!IsValidCell(selection.x, selection.z))
        {
            continue;
        }

        const bool alreadyExists = std::any_of(
            validSelections.begin(),
            validSelections.end(),
            [&](const CellSelection& existing)
            {
                return existing.x == selection.x && existing.z == selection.z;
            });
        if (!alreadyExists)
        {
            validSelections.push_back(selection);
        }
    }

    m_selectedCells = std::move(validSelections);
    if (m_selectedCells.empty() && IsValidCell(m_selectedCellX, m_selectedCellZ))
    {
        m_selectedCells.push_back({ m_selectedCellX, m_selectedCellZ });
    }

    if (!m_selectedCells.empty() && !IsCellSelected(m_selectedCellX, m_selectedCellZ))
    {
        const CellSelection& fallback = m_selectedCells.back();
        m_selectedCellX = fallback.x;
        m_selectedCellZ = fallback.z;
    }
}

SceneNarakuPieceEditor::EditorSnapshot SceneNarakuPieceEditor::CreateEditorSnapshot() const
{
    EditorSnapshot snapshot = {};
    snapshot.piece = m_piece;
    snapshot.selectedX = m_selectedX;
    snapshot.selectedZ = m_selectedZ;
    snapshot.selectedVertices = m_selectedVertices;
    snapshot.editMode = m_editMode;
    snapshot.terrainSelectionMode = m_terrainSelectionMode;
    snapshot.selectedCellX = m_selectedCellX;
    snapshot.selectedCellZ = m_selectedCellZ;
    snapshot.selectedCells = m_selectedCells;
    snapshot.gridObjectTool = m_gridObjectTool;
    snapshot.selectedGridObjectKind = m_selectedGridObjectKind;
    snapshot.selectedMiningPointIndex = m_selectedMiningPointIndex;
    return snapshot;
}

void SceneNarakuPieceEditor::PushUndoSnapshot()
{
    m_undoStack.push_back(CreateEditorSnapshot());
    TrimUndoHistory();
    m_redoStack.clear();
}

void SceneNarakuPieceEditor::TrimUndoHistory()
{
    if (m_undoStack.size() <= kMaxUndoHistory)
    {
        return;
    }

    const size_t overflowCount = m_undoStack.size() - kMaxUndoHistory;
    m_undoStack.erase(m_undoStack.begin(), m_undoStack.begin() + overflowCount);
}

void SceneNarakuPieceEditor::RestoreEditorSnapshot(const EditorSnapshot& snapshot)
{
    m_piece = snapshot.piece;
    m_selectedX = snapshot.selectedX;
    m_selectedZ = snapshot.selectedZ;
    m_selectedVertices = snapshot.selectedVertices;
    m_editMode = snapshot.editMode;
    m_terrainSelectionMode = snapshot.terrainSelectionMode;
    m_selectedCellX = snapshot.selectedCellX;
    m_selectedCellZ = snapshot.selectedCellZ;
    m_selectedCells = snapshot.selectedCells;
    m_gridObjectTool = snapshot.gridObjectTool;
    m_selectedGridObjectKind = snapshot.selectedGridObjectKind;
    m_selectedMiningPointIndex = snapshot.selectedMiningPointIndex;
    EnsureSelectionNotEmpty();
    EnsureCellSelectionValid();
    if (m_selectedGridObjectKind == GridObjectKind::MiningPoint &&
        (m_selectedMiningPointIndex < 0 || m_selectedMiningPointIndex >= static_cast<int>(m_piece.miningPoints.size())))
    {
        ClearGridObjectSelection();
    }
    m_validationDirty = true;
    m_heightDragFloatEditing = false;
    m_draggingHeight = false;
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
}

void SceneNarakuPieceEditor::UndoEdit()
{
    if (m_undoStack.empty())
    {
        return;
    }

    m_redoStack.push_back(CreateEditorSnapshot());
    if (m_redoStack.size() > kMaxUndoHistory)
    {
        m_redoStack.erase(m_redoStack.begin(), m_redoStack.begin() + (m_redoStack.size() - kMaxUndoHistory));
    }
    const EditorSnapshot snapshot = m_undoStack.back();
    m_undoStack.pop_back();
    RestoreEditorSnapshot(snapshot);
    SetMessage(u8"元に戻しました");
}

void SceneNarakuPieceEditor::RedoEdit()
{
    if (m_redoStack.empty())
    {
        return;
    }

    m_undoStack.push_back(CreateEditorSnapshot());
    TrimUndoHistory();
    const EditorSnapshot snapshot = m_redoStack.back();
    m_redoStack.pop_back();
    RestoreEditorSnapshot(snapshot);
    SetMessage(u8"やり直しました");
}

void SceneNarakuPieceEditor::HandleUndoRedoShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
    {
        m_prevUndoShortcutPressed = false;
        m_prevRedoShortcutPressed = false;
        return;
    }

    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool undoPressed = ctrlPressed && IsAsyncModifierPressed('Z');
    const bool redoPressed = ctrlPressed && IsAsyncModifierPressed('Y');

    if (IsShortcutTriggered(undoPressed, m_prevUndoShortcutPressed))
    {
        UndoEdit();
    }
    if (IsShortcutTriggered(redoPressed, m_prevRedoShortcutPressed))
    {
        RedoEdit();
    }
}

void SceneNarakuPieceEditor::UpdateCamera()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool mouseInPreview = IsMouseInsidePreviewImage();
    if ((!mouseInPreview && !m_previewImageHovered) ||
        (io.WantCaptureMouse && !m_previewImageHovered))
    {
        return;
    }

    const POINT mouseDelta = GetMouseDelta();
    const bool altPressed = IsEditorAltPressed(io);

    if (altPressed && IsMouseLeftPress())
    {
        m_cameraYaw -= static_cast<float>(mouseDelta.x) * kCameraOrbitSpeed;
        const float pitchSign = m_invertOrbitY ? -1.0f : 1.0f;
        m_cameraPitch += static_cast<float>(mouseDelta.y) * kCameraOrbitSpeed * pitchSign;
    }

    m_cameraPitch = ClampFloat(m_cameraPitch, kMinCameraPitch, kMaxCameraPitch);

    if (IsMouseMiddlePress())
    {
        const float cosPitch = std::cos(m_cameraPitch);
        const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
            -std::cos(m_cameraYaw) * cosPitch,
            -std::sin(m_cameraPitch),
            -std::sin(m_cameraYaw) * cosPitch,
            0.0f));
        const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
        const XMVECTOR cameraUp = XMVector3Normalize(XMVector3Cross(forward, right));

        XMFLOAT3 right3 = {};
        XMFLOAT3 up3 = {};
        XMStoreFloat3(&right3, right);
        XMStoreFloat3(&up3, cameraUp);

        const float panScale = std::max(0.05f, m_cameraDistance * kCameraPanScaleFactor);
        const float horizontalDelta = static_cast<float>(mouseDelta.x);
        const float verticalDelta = static_cast<float>(mouseDelta.y);

        m_cameraTarget.x -= right3.x * horizontalDelta * panScale;
        m_cameraTarget.y -= right3.y * horizontalDelta * panScale;
        m_cameraTarget.z -= right3.z * horizontalDelta * panScale;

        m_cameraTarget.x += up3.x * verticalDelta * panScale;
        m_cameraTarget.y += up3.y * verticalDelta * panScale;
        m_cameraTarget.z += up3.z * verticalDelta * panScale;
    }

    const float wheelDelta = GetMouseWheelDelta();
    if (wheelDelta != 0.0f)
    {
        m_cameraDistance -= wheelDelta * std::max(1.0f, m_cameraDistance * 0.10f);
        m_cameraDistance = ClampFloat(m_cameraDistance, kMinCameraDistance, kMaxCameraDistance);
    }
}

void SceneNarakuPieceEditor::UpdateHeightEditing()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const bool ctrlPressed = IsEditorCtrlPressed(io);
    const bool shiftPressed = IsEditorShiftPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool mouseInPreview = IsMouseInsidePreviewImage();
    const bool allowPreviewInput = mouseInPreview || m_previewImageHovered;

    if (m_terrainSelectionMode == TerrainSelectionMode::Cell)
    {
        if (allowPreviewInput && PickTerrainCell(mousePos, m_hoverCellX, m_hoverCellZ))
        {
        }
        else
        {
            m_hoverCellX = -1;
            m_hoverCellZ = -1;
        }

        if (m_dragSelecting)
        {
            m_selectionDragCurrent = mousePos;
            const float deltaX = static_cast<float>(m_selectionDragCurrent.x - m_selectionDragStart.x);
            const float deltaY = static_cast<float>(m_selectionDragCurrent.y - m_selectionDragStart.y);
            const float dragDistanceSq = deltaX * deltaX + deltaY * deltaY;
            const float thresholdSq = kDragSelectThresholdPx * kDragSelectThresholdPx;
            if (!m_selectionDragActive && dragDistanceSq >= thresholdSq)
            {
                m_selectionDragActive = true;
            }

            if (IsMouseLeftRelease())
            {
                if (m_selectionDragActive)
                {
                    const std::vector<CellSelection> cells =
                        CollectCellsInScreenRect(m_selectionDragStart, m_selectionDragCurrent);
                    ApplyCellRectangleSelection(cells, m_selectionDragCtrl, m_selectionDragShift);
                }
                else
                {
                    int pickedCellX = -1;
                    int pickedCellZ = -1;
                    POINT pickPoint = m_selectionDragCurrent;
                    if (!PickTerrainCell(pickPoint, pickedCellX, pickedCellZ))
                    {
                        pickPoint = m_selectionDragStart;
                    }
                    if (PickTerrainCell(pickPoint, pickedCellX, pickedCellZ))
                    {
                        SelectCellFromInput(pickedCellX, pickedCellZ, m_selectionDragCtrl, m_selectionDragShift);
                    }
                }

                m_dragSelecting = false;
                m_selectionDragActive = false;
            }
            return;
        }

        if (!allowPreviewInput)
        {
            return;
        }

        if ((io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger())
        {
            return;
        }

        int pickedCellX = -1;
        int pickedCellZ = -1;
        if (!PickTerrainCell(mousePos, pickedCellX, pickedCellZ))
        {
            return;
        }

        if (shiftPressed)
        {
            m_dragSelecting = true;
            m_selectionDragActive = false;
            m_selectionDragStart = mousePos;
            m_selectionDragCurrent = mousePos;
            m_selectionDragCtrl = ctrlPressed;
            m_selectionDragShift = shiftPressed;
            return;
        }

        SelectCellFromInput(pickedCellX, pickedCellZ, ctrlPressed, false);
        return;
    }

    if (!allowPreviewInput && !m_draggingHeight && !m_dragSelecting)
    {
        return;
    }

    if (m_draggingHeight)
    {
        if (!IsMouseLeftPress())
        {
            m_draggingHeight = false;
            return;
        }

        if (!altPressed)
        {
            const POINT mouseDelta = GetMouseDelta();
            if (mouseDelta.y != 0)
            {
                ApplyHeightDeltaToSelectedVertices(-static_cast<float>(mouseDelta.y) * m_heightDragScale);
            }
        }
        return;
    }

    if (m_dragSelecting)
    {
        m_selectionDragCurrent = mousePos;

        const float deltaX = static_cast<float>(m_selectionDragCurrent.x - m_selectionDragStart.x);
        const float deltaY = static_cast<float>(m_selectionDragCurrent.y - m_selectionDragStart.y);
        const float dragDistanceSq = deltaX * deltaX + deltaY * deltaY;
        const float thresholdSq = kDragSelectThresholdPx * kDragSelectThresholdPx;
        if (!m_selectionDragActive && dragDistanceSq >= thresholdSq)
        {
            m_selectionDragActive = true;
        }

        if (IsMouseLeftRelease())
        {
            if (m_selectionDragActive)
            {
                const std::vector<VertexSelection> vertices =
                    CollectVerticesInScreenRect(m_selectionDragStart, m_selectionDragCurrent);
                ApplyRectangleSelection(vertices, m_selectionDragCtrl, m_selectionDragShift);
            }
            else
            {
                int pickedX = -1;
                int pickedZ = -1;
                POINT pickPoint = m_selectionDragCurrent;
                if (!PickTerrainVertex(pickPoint, pickedX, pickedZ))
                {
                    pickPoint = m_selectionDragStart;
                }
                if (PickTerrainVertex(pickPoint, pickedX, pickedZ))
                {
                    SelectVertexFromInput(pickedX, pickedZ, m_selectionDragCtrl, m_selectionDragShift);
                }
            }

            m_dragSelecting = false;
            m_selectionDragActive = false;
        }
        return;
    }

    if (!allowPreviewInput)
    {
        return;
    }

    if ((io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger())
    {
        return;
    }

    int pickedX = -1;
    int pickedZ = -1;
    if (!PickTerrainVertex(mousePos, pickedX, pickedZ))
    {
        return;
    }

    if (ctrlPressed || shiftPressed)
    {
        m_dragSelecting = true;
        m_selectionDragActive = false;
        m_selectionDragStart = mousePos;
        m_selectionDragCurrent = mousePos;
        m_selectionDragCtrl = ctrlPressed;
        m_selectionDragShift = shiftPressed;
        return;
    }

    if (IsVertexSelected(pickedX, pickedZ))
    {
        m_selectedX = pickedX;
        m_selectedZ = pickedZ;
    }
    else
    {
        SelectVertexFromInput(pickedX, pickedZ, false, false);
    }
    PushUndoSnapshot();
    m_draggingHeight = true;
}

void SceneNarakuPieceEditor::UpdateGridObjectEditing()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool mouseInPreview = IsMouseInsidePreviewImage();
    const bool allowPreviewInput = mouseInPreview || m_previewImageHovered;

    if (allowPreviewInput && PickTerrainCell(mousePos, m_hoverCellX, m_hoverCellZ))
    {
    }
    else
    {
        m_hoverCellX = -1;
        m_hoverCellZ = -1;
    }

    if (!allowPreviewInput)
    {
        return;
    }

    if ((io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger())
    {
        return;
    }

    int cellX = -1;
    int cellZ = -1;
    if (!PickTerrainCell(mousePos, cellX, cellZ))
    {
        return;
    }

    std::string placeError;
    if (!CanPlaceGridObject(m_gridObjectTool, cellX, cellZ, placeError))
    {
        SetMessage(placeError);
        return;
    }

    switch (m_gridObjectTool)
    {
    case GridObjectTool::MiningPoint:
    {
        const int existingIndex = FindMiningPointIndexByCell(cellX, cellZ);
        if (existingIndex >= 0)
        {
            SelectMiningPoint(existingIndex);
            SetMessage(u8"既存の採掘ポイントを選択しました");
            return;
        }

        if (m_piece.miningPoints.size() >= 5)
        {
            SetMessage(u8"採掘ポイントは最大5件までです");
            return;
        }

        PushUndoSnapshot();
        NarakuPiece::MiningPointData point = {};
        point.id = GenerateMiningPointId();
        point.cell.x = cellX;
        point.cell.z = cellZ;
        point.visualType = ClampInt(m_newMiningVisualType, 0, 3);
        point.initiallyRecorded = m_newMiningInitiallyRecorded;
        m_piece.miningPoints.push_back(point);
        SelectMiningPoint(static_cast<int>(m_piece.miningPoints.size()) - 1);
        m_validationDirty = true;
        SetMessage(u8"採掘ポイントを追加しました");
        return;
    }

    case GridObjectTool::RopeTop:
        PushUndoSnapshot();
        m_piece.rope.enabled = true;
        m_piece.rope.top.x = cellX;
        m_piece.rope.top.z = cellZ;
        SelectRope();
        m_validationDirty = true;
        SetMessage(u8"ロープ上端を設定しました");
        return;

    case GridObjectTool::RopeBottom:
        PushUndoSnapshot();
        m_piece.rope.enabled = true;
        m_piece.rope.bottom.x = cellX;
        m_piece.rope.bottom.z = cellZ;
        SelectRope();
        m_validationDirty = true;
        SetMessage(u8"ロープ下端を設定しました");
        return;

    case GridObjectTool::StartReturn:
        PushUndoSnapshot();
        m_piece.startReturnCandidate.enabled = true;
        m_piece.startReturnCandidate.cell.x = cellX;
        m_piece.startReturnCandidate.cell.z = cellZ;
        SelectStartReturn();
        m_validationDirty = true;
        SetMessage(u8"開始・帰還地点を設定しました");
        return;

    default:
        return;
    }
}

void SceneNarakuPieceEditor::UpdateCameraMatrices()
{
    m_cameraPitch = ClampFloat(m_cameraPitch, kMinCameraPitch, kMaxCameraPitch);
    m_cameraDistance = ClampFloat(m_cameraDistance, kMinCameraDistance, kMaxCameraDistance);

    const float cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 eyePos =
    {
        m_cameraTarget.x + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance
    };

    const XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 1.0f);
    const XMVECTOR target = XMVectorSet(m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z, 1.0f);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(eye, target, up));

    const XMFLOAT2 viewportSize = GetPreviewViewportSize();
    const float aspect = viewportSize.x / viewportSize.y;
    XMStoreFloat4x4(
        &m_projectionMatrix,
        XMMatrixPerspectiveFovLH(XMConvertToRadians(kCameraFovDegrees), aspect, kCameraNearPlane, kCameraFarPlane));
}

void SceneNarakuPieceEditor::ResetCamera()
{
    m_cameraTarget = { 0.0f, 0.0f, 0.0f };
    m_cameraYaw = kInitialCameraYaw;
    m_cameraPitch = kInitialCameraPitch;
    m_cameraDistance = kInitialCameraDistance;
}

bool SceneNarakuPieceEditor::ProjectWorldToScreen(const XMFLOAT3& worldPos, XMFLOAT2& outScreen) const
{
    const XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    const XMVECTOR clip = XMVector3TransformCoord(
        XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f),
        view * projection);

    XMFLOAT3 ndc = {};
    XMStoreFloat3(&ndc, clip);
    if (ndc.z < 0.0f || ndc.z > 1.0f)
    {
        return false;
    }

    const XMFLOAT2 viewportSize = GetPreviewViewportSize();
    outScreen.x = (ndc.x * 0.5f + 0.5f) * viewportSize.x + m_previewImageTopLeft.x;
    outScreen.y = (-ndc.y * 0.5f + 0.5f) * viewportSize.y + m_previewImageTopLeft.y;
    return true;
}

bool SceneNarakuPieceEditor::PickTerrainVertex(POINT mousePos, int& outX, int& outZ) const
{
    float bestDistance = kPickThresholdPx;
    bool found = false;

    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(GetVertexWorldPosition(x, z), screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outX = x;
                outZ = z;
                found = true;
            }
        }
    }

    return found;
}

bool SceneNarakuPieceEditor::PickTerrainCell(POINT mousePos, int& outX, int& outZ) const
{
    if (m_piece.gridWidth < 2 || m_piece.gridDepth < 2)
    {
        return false;
    }

    float bestDistance = kCellPickThresholdPx;
    bool found = false;

    for (int z = 0; z < m_piece.gridDepth - 1; ++z)
    {
        for (int x = 0; x < m_piece.gridWidth - 1; ++x)
        {
            const XMFLOAT3 center = GetCellWorldPosition(x, z);
            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(center, screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distance = std::sqrt(dx * dx + dy * dy);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outX = x;
                outZ = z;
                found = true;
            }
        }
    }

    return found;
}

void SceneNarakuPieceEditor::DrawDebugBox3D(const XMFLOAT3& pos, const XMFLOAT3& scale) const
{
    const XMMATRIX worldMatrix = XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 world = {};
    XMFLOAT4X4 view = {};
    XMFLOAT4X4 projection = {};

    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);
    Geometory::DrawBox();
}

void SceneNarakuPieceEditor::DrawDebugWireBox3D(const XMFLOAT3& pos, const XMFLOAT3& scale, const XMFLOAT4& color) const
{
    const float halfX = scale.x * 0.5f;
    const float halfY = scale.y * 0.5f;
    const float halfZ = scale.z * 0.5f;
    const XMFLOAT3 corners[8] =
    {
        { pos.x - halfX, pos.y - halfY, pos.z - halfZ },
        { pos.x + halfX, pos.y - halfY, pos.z - halfZ },
        { pos.x - halfX, pos.y - halfY, pos.z + halfZ },
        { pos.x + halfX, pos.y - halfY, pos.z + halfZ },
        { pos.x - halfX, pos.y + halfY, pos.z - halfZ },
        { pos.x + halfX, pos.y + halfY, pos.z - halfZ },
        { pos.x - halfX, pos.y + halfY, pos.z + halfZ },
        { pos.x + halfX, pos.y + halfY, pos.z + halfZ },
    };
    const int edges[12][2] =
    {
        { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
        { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };

    for (const auto& edge : edges)
    {
        Geometory::AddLine(corners[edge[0]], corners[edge[1]], color);
    }
}

void SceneNarakuPieceEditor::RefreshValidationIssues()
{
    m_validationIssues = NarakuPiece::ValidatePieceData(m_piece);
    m_validationDirty = false;
}

void SceneNarakuPieceEditor::SetMessage(const std::string& message)
{
    m_message = message;
}

std::wstring SceneNarakuPieceEditor::Utf8ToWide(const std::string& text) const
{
    if (text.empty())
    {
        return std::wstring();
    }

    const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    if (length <= 0)
    {
        std::wstring fallback;
        fallback.reserve(text.size());
        for (unsigned char ch : text)
        {
            fallback.push_back(static_cast<wchar_t>(ch));
        }
        return fallback;
    }

    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
    if (!result.empty())
    {
        result.pop_back();
    }
    return result;
}

std::string SceneNarakuPieceEditor::WideToUtf8(const std::wstring& text) const
{
    if (text.empty())
    {
        return std::string();
    }

    const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (length <= 0)
    {
        std::string fallback;
        fallback.reserve(text.size());
        for (wchar_t ch : text)
        {
            fallback.push_back((ch >= 0 && ch <= 0x7f) ? static_cast<char>(ch) : '?');
        }
        return fallback;
    }

    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
    if (!result.empty())
    {
        result.pop_back();
    }
    return result;
}



