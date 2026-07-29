#include "SceneNarakuPieceEditor.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Model.h"
#include "ShaderList.h"
#include "Texture.h"
#include "imgui.h"
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX;

namespace
{
    constexpr const char* kEnvironmentModelCatalogPath = "Assets/Naraku/environment_models.cfg";
    /**
     * @brief ネイティブメニュー項目のチェック状態を更新します。
     * @param menuBar 更新対象のメニューバーです。
     * @param commandId 更新対象のコマンドIDです。
     * @param checked チェック状態にする場合はtrueです。
     */
    void SetMenuCheckState(HMENU menuBar, UINT commandId, bool checked)
    {
        CheckMenuItem(menuBar, commandId, MF_BYCOMMAND | (checked ? MF_CHECKED : MF_UNCHECKED));
    }

    /**
     * @brief ネイティブメニュー項目の表示文字列を更新します。
     * @param menuBar 更新対象のメニューバーです。
     * @param commandId 更新対象のコマンドIDです。
     * @param label 設定する表示文字列です。
     */
    void SetMenuItemLabel(HMENU menuBar, UINT commandId, const std::wstring& label)
    {
        ModifyMenuW(menuBar, commandId, MF_BYCOMMAND | MF_STRING | MF_GRAYED, commandId, label.c_str());
    }

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
        if (fileName.empty())
        {
            return true;
        }

        if (fileName.find_first_of(kInvalidChars) != std::wstring::npos)
        {
            return true;
        }

        for (wchar_t ch : fileName)
        {
            if (ch < 0x20)
            {
                return true;
            }
        }

        return fileName == L"." || fileName == L"..";
    }

    /**
     * @brief 現在のローカル日時をワイド文字列へ整形します。
     * @return `YYYY/MM/DD HH:MM:SS` 形式の日時文字列です。
     */
    std::wstring BuildCurrentDateTimeString()
    {
        std::time_t now = std::time(nullptr);
        std::tm localTime = {};
        localtime_s(&localTime, &now);

        wchar_t buffer[20] = {};
        std::wcsftime(buffer, std::size(buffer), L"%Y/%m/%d %H:%M:%S", &localTime);
        return std::wstring(buffer);
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
        u8"環境オブジェクト配置",
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
        u8"層間口ロープ端点",
        u8"層間口ロード地点",
    };

    const char* const kLayerTransitionRoleLabels[] =
    {
        u8"なし",
        u8"層入口",
        u8"層出口",
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

    /** @brief コンパス円の半径です。 */
    constexpr float kCompassRadius = 30.0f;

    /** @brief プレビュー画像端からコンパス円までの余白です。 */
    constexpr float kCompassMargin = 12.0f;

    /** @brief コンパスの方位線の太さです。 */
    constexpr float kCompassLineThickness = 1.5f;

    /** @brief コンパス円の内側で方位線を止める余白です。 */
    constexpr float kCompassLinePadding = 2.0f;

    /** @brief コンパス円の外側へ方位文字を離す距離です。 */
    constexpr float kCompassLabelDistance = 8.0f;

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
    LoadEnvironmentModelCatalog();
    UpdateMainWindowTitle();
    ResetCamera();
    UpdateCameraMatrices();
    RefreshValidationIssues();
}

SceneNarakuPieceEditor::~SceneNarakuPieceEditor()
{
    ReleaseEnvironmentModelPopupPreview();
    ReleaseEnvironmentModels();
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
            MarkPieceDirty();
            SetMessage(u8"選択セルを削除しました");
        }
        UpdateHeightEditing();
    }
    else if (m_editMode == EditMode::GridObject)
    {
        if (deleteTriggered)
        {
            DeleteSelectedGridObject();
        }
        UpdateGridObjectEditing();
    }
    else
    {
        if (deleteTriggered && m_selectedEnvironmentObjectIndex >= 0 &&
            m_selectedEnvironmentObjectIndex < static_cast<int>(m_piece.environmentObjects.size()))
        {
            PushUndoSnapshot();
            m_piece.environmentObjects.erase(
                m_piece.environmentObjects.begin() + m_selectedEnvironmentObjectIndex);
            m_selectedEnvironmentObjectIndex = -1;
            MarkPieceDirty();
            SetMessage(u8"環境オブジェクトを削除しました");
        }
        UpdateEnvironmentObjectEditing();
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
    if (m_requestOpenNewPiecePopup)
    {
        ImGui::OpenPopup(u8"新規ピースを作成");
        m_requestOpenNewPiecePopup = false;
    }
    if (m_requestOpenSavePiecePopup)
    {
        ImGui::OpenPopup(u8"ピースを保存");
        m_requestOpenSavePiecePopup = false;
    }
    if (m_requestOpenRenamePiecePopup)
    {
        ImGui::OpenPopup(u8"ピース名を変更");
        m_requestOpenRenamePiecePopup = false;
    }
    DrawEditorWindow();
    DrawPreviewWindow();
    DrawHeightGridWindow();
    DrawPieceHierarchyWindow();
    DrawEnvironmentAssetsWindow();
    DrawNewPiecePopup();
    DrawSavePiecePopup();
    DrawRenamePiecePopup();
    DrawEnvironmentModelPopup();
}

bool SceneNarakuPieceEditor::HandleNativeMenuCommand(unsigned int commandId)
{
    switch (commandId)
    {
    case MenuNewPiece:
        if (!ConfirmDiscardDirtyChanges(L"新規作成"))
        {
            return true;
        }
        std::fill(m_newPieceFileNameInput.begin(), m_newPieceFileNameInput.end(), '\0');
        {
            const std::string fileNameUtf8 = WideToUtf8(EnsureJsonFileName(m_saveFileName));
            const size_t copyLength = std::min(fileNameUtf8.size(), m_newPieceFileNameInput.size() - 1);
            std::copy_n(fileNameUtf8.data(), copyLength, m_newPieceFileNameInput.data());
            m_newPieceFileNameInput[copyLength] = '\0';
        }
        m_newPieceFileName = EnsureJsonFileName(m_saveFileName);
        m_requestOpenNewPiecePopup = true;
        return true;
    case MenuSavePiece:
        SyncSaveFileNameInput();
        m_saveAsDraft = true;
        m_requestOpenSavePiecePopup = true;
        return true;
    case MenuLoadPiece:
        if (!ConfirmDiscardDirtyChanges(L"読込"))
        {
            return true;
        }
        OpenLoadPieceDialog();
        return true;
    case MenuRenamePiece:
        if (!ConfirmDiscardDirtyChanges(L"名前変更"))
        {
            return true;
        }
        std::fill(m_renameFileNameInput.begin(), m_renameFileNameInput.end(), '\0');
        {
            const std::string fileNameUtf8 = WideToUtf8(EnsureJsonFileName(m_saveFileName));
            const size_t copyLength = std::min(fileNameUtf8.size(), m_renameFileNameInput.size() - 1);
            std::copy_n(fileNameUtf8.data(), copyLength, m_renameFileNameInput.data());
            m_renameFileNameInput[copyLength] = '\0';
        }
        m_requestOpenRenamePiecePopup = true;
        return true;
    case MenuDeletePiece:
        if (!ConfirmDiscardDirtyChanges(L"削除"))
        {
            return true;
        }
        DeleteCurrentPiece();
        return true;
    case MenuTogglePieceBasicWindow:
        m_showPieceBasicWindow = !m_showPieceBasicWindow;
        return true;
    case MenuTogglePieceConnectionWindow:
        m_showPieceConnectionWindow = !m_showPieceConnectionWindow;
        return true;
    case MenuToggleTerrainEditWindow:
        m_showTerrainEditWindow = !m_showTerrainEditWindow;
        return true;
    case MenuToggleGridObjectPlacementWindow:
        m_showGridObjectPlacementWindow = !m_showGridObjectPlacementWindow;
        return true;
    case MenuToggleGridObjectSelectionWindow:
        m_showGridObjectSelectionWindow = !m_showGridObjectSelectionWindow;
        return true;
    case MenuTogglePieceFileAndValidationWindow:
        m_showPieceFileAndValidationWindow = !m_showPieceFileAndValidationWindow;
        return true;
    case MenuTogglePreviewWindow:
        m_showPreviewWindow = !m_showPreviewWindow;
        return true;
    case MenuToggleHeightGridWindow:
        m_showHeightGridWindow = !m_showHeightGridWindow;
        return true;
    case MenuTogglePieceHierarchyWindow:
        m_showPieceHierarchyWindow = !m_showPieceHierarchyWindow;
        return true;
    case MenuNewEnvironmentModel:
        OpenNewEnvironmentModelDialog();
        return true;
    case MenuDeleteEnvironmentModel:
        DeleteSelectedEnvironmentModel();
        return true;
    case MenuEnvironmentModelSetting:
        OpenEnvironmentModelSetting();
        return true;
    case MenuToggleEnvironmentAssetsWindow:
        m_showEnvironmentAssetsWindow = !m_showEnvironmentAssetsWindow;
        return true;
    default:
        return false;
    }
}

void SceneNarakuPieceEditor::SyncNativeMenuState(HMENU menuBar) const
{
    if (menuBar == nullptr)
    {
        return;
    }

    SetMenuCheckState(menuBar, MenuTogglePieceBasicWindow, m_showPieceBasicWindow);
    SetMenuCheckState(menuBar, MenuTogglePieceConnectionWindow, m_showPieceConnectionWindow);
    SetMenuCheckState(menuBar, MenuToggleTerrainEditWindow, m_showTerrainEditWindow);
    SetMenuCheckState(menuBar, MenuToggleGridObjectPlacementWindow, m_showGridObjectPlacementWindow);
    SetMenuCheckState(menuBar, MenuToggleGridObjectSelectionWindow, m_showGridObjectSelectionWindow);
    SetMenuCheckState(menuBar, MenuTogglePieceFileAndValidationWindow, m_showPieceFileAndValidationWindow);
    SetMenuCheckState(menuBar, MenuTogglePreviewWindow, m_showPreviewWindow);
    SetMenuCheckState(menuBar, MenuToggleHeightGridWindow, m_showHeightGridWindow);
    SetMenuCheckState(menuBar, MenuTogglePieceHierarchyWindow, m_showPieceHierarchyWindow);
    SetMenuCheckState(menuBar, MenuToggleEnvironmentAssetsWindow, m_showEnvironmentAssetsWindow);
    SetMenuItemLabel(menuBar, MenuFileStatus, BuildEditingStatusLabel());

    if (HWND window = GetPreviewHostWindow())
    {
        DrawMenuBar(window);
    }
}

void SceneNarakuPieceEditor::ReleaseEnvironmentModels()
{
    for (EnvironmentModelAsset& asset : m_environmentModels)
    {
        SAFE_DELETE(asset.thumbnailDepthStencil);
        SAFE_DELETE(asset.thumbnailRenderTarget);
        SAFE_DELETE(asset.model);
    }
    m_environmentModels.clear();
    m_selectedEnvironmentModelIndex = -1;
}

void SceneNarakuPieceEditor::UpdateEnvironmentModelBounds(EnvironmentModelAsset& asset)
{
    asset.hasBounds = false;
    asset.boundsMin = { -0.5f, 0.0f, -0.5f };
    asset.boundsMax = { 0.5f, 1.0f, 0.5f };
    asset.previewAnchor = {};
    if (asset.model == nullptr) return;

    XMFLOAT3 minValue = { FLT_MAX, FLT_MAX, FLT_MAX };
    XMFLOAT3 maxValue = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool hasVertex = false;
    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        for (const Model::Vertex& vertex : mesh->vertices)
        {
            minValue.x = std::min(minValue.x, vertex.pos.x);
            minValue.y = std::min(minValue.y, vertex.pos.y);
            minValue.z = std::min(minValue.z, vertex.pos.z);
            maxValue.x = std::max(maxValue.x, vertex.pos.x);
            maxValue.y = std::max(maxValue.y, vertex.pos.y);
            maxValue.z = std::max(maxValue.z, vertex.pos.z);
            hasVertex = true;
        }
    }
    if (!hasVertex) return;

    asset.boundsMin = minValue;
    asset.boundsMax = maxValue;
    asset.previewAnchor = {
        (minValue.x + maxValue.x) * 0.5f,
        minValue.y,
        (minValue.z + maxValue.z) * 0.5f };
    asset.hasBounds = true;
    asset.thumbnailDirty = true;
}

bool SceneNarakuPieceEditor::EnsureEnvironmentModelThumbnail(EnvironmentModelAsset& asset, unsigned int size)
{
    size = std::max(64U, size);
    if (asset.thumbnailRenderTarget != nullptr && asset.thumbnailDepthStencil != nullptr && asset.thumbnailSize == size)
    {
        return true;
    }

    SAFE_DELETE(asset.thumbnailDepthStencil);
    SAFE_DELETE(asset.thumbnailRenderTarget);
    asset.thumbnailRenderTarget = new RenderTarget();
    if (FAILED(asset.thumbnailRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, size, size)))
    {
        SAFE_DELETE(asset.thumbnailRenderTarget);
        return false;
    }
    asset.thumbnailDepthStencil = new DepthStencil();
    if (FAILED(asset.thumbnailDepthStencil->Create(size, size, false)))
    {
        SAFE_DELETE(asset.thumbnailDepthStencil);
        SAFE_DELETE(asset.thumbnailRenderTarget);
        return false;
    }
    asset.thumbnailSize = size;
    asset.thumbnailDirty = true;
    return true;
}

void SceneNarakuPieceEditor::RenderEnvironmentModelThumbnail(EnvironmentModelAsset& asset, unsigned int size)
{
    if (asset.model == nullptr || !EnsureEnvironmentModelThumbnail(asset, size)) return;
    if (!asset.thumbnailDirty && asset.thumbnailRenderTarget->GetResource() != nullptr) return;

    RenderTarget* targets[1] = { asset.thumbnailRenderTarget };
    SetRenderTargets(1, targets, asset.thumbnailDepthStencil);
    const float clearColor[4] = { 0.055f, 0.065f, 0.080f, 1.0f };
    asset.thumbnailRenderTarget->Clear(clearColor);
    asset.thumbnailDepthStencil->Clear();

    const XMFLOAT3 modelSize = {
        std::max(0.001f, (asset.boundsMax.x - asset.boundsMin.x) * asset.defaultScale.x),
        std::max(0.001f, (asset.boundsMax.y - asset.boundsMin.y) * asset.defaultScale.y),
        std::max(0.001f, (asset.boundsMax.z - asset.boundsMin.z) * asset.defaultScale.z) };
    const float extent = std::max(0.25f, std::max(modelSize.x, std::max(modelSize.y, modelSize.z)));
    const XMFLOAT3 eye = { extent * 1.45f, extent * 1.10f, -extent * 1.80f };
    const XMFLOAT3 look = { 0.0f, modelSize.y * 0.45f, 0.0f };

    XMFLOAT4X4 wvp[3] = {};
    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
        XMMatrixTranslation(-asset.previewAnchor.x, -asset.previewAnchor.y, -asset.previewAnchor.z) *
        XMMatrixScaling(asset.defaultScale.x, asset.defaultScale.y, asset.defaultScale.z)));
    XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(XMMatrixLookAtLH(
        XMLoadFloat3(&eye), XMLoadFloat3(&look), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
    XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(XMMatrixPerspectiveFovLH(
        XMConvertToRadians(35.0f), 1.0f, 0.01f, std::max(100.0f, extent * 12.0f))));
    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        const Model::Material* sourceMaterial = asset.model->GetMaterial(mesh->materialID);
        if (sourceMaterial != nullptr)
        {
            Model::Material material = *sourceMaterial;
            material.ambient = { 0.72f, 0.72f, 0.72f, 1.0f };
            ShaderList::SetMaterial(material);
        }
        asset.model->Draw(static_cast<int>(meshIndex));
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
    asset.thumbnailDirty = false;
}

void* SceneNarakuPieceEditor::GetEnvironmentModelThumbnailTextureId(int index, unsigned int size)
{
    if (index < 0 || index >= static_cast<int>(m_environmentModels.size())) return nullptr;
    EnvironmentModelAsset& asset = m_environmentModels[static_cast<size_t>(index)];
    RenderEnvironmentModelThumbnail(asset, size);
    return asset.thumbnailRenderTarget != nullptr
        ? reinterpret_cast<void*>(asset.thumbnailRenderTarget->GetResource())
        : nullptr;
}

bool SceneNarakuPieceEditor::EnsureEnvironmentModelPopupPreview(unsigned int size)
{
    size = std::max(128U, size);
    if (m_environmentModelPopupRenderTarget != nullptr &&
        m_environmentModelPopupDepthStencil != nullptr &&
        m_environmentModelPopupPreviewSize == size)
    {
        return true;
    }

    SAFE_DELETE(m_environmentModelPopupDepthStencil);
    SAFE_DELETE(m_environmentModelPopupRenderTarget);
    m_environmentModelPopupRenderTarget = new RenderTarget();
    if (FAILED(m_environmentModelPopupRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, size, size)))
    {
        SAFE_DELETE(m_environmentModelPopupRenderTarget);
        return false;
    }
    m_environmentModelPopupDepthStencil = new DepthStencil();
    if (FAILED(m_environmentModelPopupDepthStencil->Create(size, size, false)))
    {
        SAFE_DELETE(m_environmentModelPopupDepthStencil);
        SAFE_DELETE(m_environmentModelPopupRenderTarget);
        return false;
    }
    m_environmentModelPopupPreviewSize = size;
    return true;
}

void SceneNarakuPieceEditor::RenderEnvironmentModelPopupPreview(unsigned int size)
{
    Model* model = nullptr;
    XMFLOAT3 boundsMin = m_environmentModelPopupBoundsMin;
    XMFLOAT3 boundsMax = m_environmentModelPopupBoundsMax;
    XMFLOAT3 anchor = m_environmentModelPopupPreviewAnchor;
    if (m_environmentModelPopupIsNew)
    {
        model = m_environmentModelPopupPreviewModel;
    }
    else if (m_selectedEnvironmentModelIndex >= 0 &&
        m_selectedEnvironmentModelIndex < static_cast<int>(m_environmentModels.size()))
    {
        const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
        model = asset.model;
        boundsMin = asset.boundsMin;
        boundsMax = asset.boundsMax;
        anchor = asset.previewAnchor;
    }
    if (model == nullptr || !EnsureEnvironmentModelPopupPreview(size)) return;

    RenderTarget* targets[1] = { m_environmentModelPopupRenderTarget };
    SetRenderTargets(1, targets, m_environmentModelPopupDepthStencil);
    const float clearColor[4] = { 0.055f, 0.065f, 0.080f, 1.0f };
    m_environmentModelPopupRenderTarget->Clear(clearColor);
    m_environmentModelPopupDepthStencil->Clear();

    const XMFLOAT3 scale = {
        std::max(0.01f, m_environmentModelScaleInput.x),
        std::max(0.01f, m_environmentModelScaleInput.y),
        std::max(0.01f, m_environmentModelScaleInput.z) };
    const XMFLOAT3 modelSize = {
        std::max(0.001f, (boundsMax.x - boundsMin.x) * scale.x),
        std::max(0.001f, (boundsMax.y - boundsMin.y) * scale.y),
        std::max(0.001f, (boundsMax.z - boundsMin.z) * scale.z) };
    const float cellSize = std::max(0.1f, m_piece.cellSize);
    const float extent = std::max(
        cellSize * 2.25f,
        std::max(modelSize.y, std::max(modelSize.x, modelSize.z)));
    const XMFLOAT3 eye = { extent * 1.45f, extent * 1.10f, -extent * 1.80f };
    const XMFLOAT3 look = { 0.0f, modelSize.y * 0.35f, 0.0f };
    const XMMATRIX view = XMMatrixLookAtLH(
        XMLoadFloat3(&eye), XMLoadFloat3(&look), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(35.0f), 1.0f, 0.01f, std::max(100.0f, extent * 12.0f));

    XMFLOAT4X4 wvp[3] = {};
    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(XMMatrixIdentity()));
    XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(view));
    XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(projection));
    ShaderList::SetWVP(wvp);
    const float gridExtent = cellSize * 2.0f;
    for (int line = -2; line <= 2; ++line)
    {
        const float offset = static_cast<float>(line) * cellSize;
        const XMFLOAT4 color = line == 0
            ? XMFLOAT4{ 0.45f, 0.65f, 0.85f, 1.0f }
            : XMFLOAT4{ 0.30f, 0.34f, 0.40f, 1.0f };
        Geometory::AddLine({ offset, 0.0f, -gridExtent }, { offset, 0.0f, gridExtent }, color);
        Geometory::AddLine({ -gridExtent, 0.0f, offset }, { gridExtent, 0.0f, offset }, color);
    }
    Geometory::DrawLines();

    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
        XMMatrixTranslation(-anchor.x, -anchor.y, -anchor.z) *
        XMMatrixScaling(scale.x, scale.y, scale.z)));
    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int meshIndex = 0; meshIndex < model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = model->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        const Model::Material* sourceMaterial = model->GetMaterial(mesh->materialID);
        if (sourceMaterial != nullptr)
        {
            Model::Material material = *sourceMaterial;
            material.ambient = { 0.72f, 0.72f, 0.72f, 1.0f };
            ShaderList::SetMaterial(material);
        }
        model->Draw(static_cast<int>(meshIndex));
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
}

void* SceneNarakuPieceEditor::GetEnvironmentModelPopupPreviewTextureId(unsigned int size)
{
    RenderEnvironmentModelPopupPreview(size);
    return m_environmentModelPopupRenderTarget != nullptr
        ? reinterpret_cast<void*>(m_environmentModelPopupRenderTarget->GetResource())
        : nullptr;
}

void SceneNarakuPieceEditor::ReleaseEnvironmentModelPopupPreview()
{
    SAFE_DELETE(m_environmentModelPopupPreviewModel);
    SAFE_DELETE(m_environmentModelPopupDepthStencil);
    SAFE_DELETE(m_environmentModelPopupRenderTarget);
    m_environmentModelPopupPreviewSize = 0;
    m_environmentModelPopupBoundsMin = { -0.5f, 0.0f, -0.5f };
    m_environmentModelPopupBoundsMax = { 0.5f, 1.0f, 0.5f };
    m_environmentModelPopupPreviewAnchor = {};
}

void SceneNarakuPieceEditor::LoadEnvironmentModelCatalog()
{
    ReleaseEnvironmentModels();
    const std::wstring catalogPath = ResolvePieceHierarchyPath(Utf8ToWide(kEnvironmentModelCatalogPath));
    std::ifstream input(catalogPath, std::ios::binary);
    if (!input)
    {
        SetMessage(u8"環境モデルは未登録です");
        return;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        EnvironmentModelAsset asset;
        if (!(row >> std::quoted(asset.id) >> std::quoted(asset.name) >> std::quoted(asset.path)
            >> asset.defaultScale.x >> asset.defaultScale.y >> asset.defaultScale.z))
        {
            continue;
        }
        const std::wstring modelPath = ResolvePieceHierarchyPath(Utf8ToWide(asset.path));
        const std::string modelPathUtf8 = WideToUtf8(modelPath);
        asset.model = new Model();
        if (!asset.model->Load(modelPathUtf8.c_str(), 1.0f, Model::ZFlip))
        {
            SAFE_DELETE(asset.model);
            continue;
        }
        UpdateEnvironmentModelBounds(asset);
        m_environmentModels.push_back(asset);
    }
    if (!m_environmentModels.empty()) m_selectedEnvironmentModelIndex = 0;
}

bool SceneNarakuPieceEditor::SaveEnvironmentModelCatalog()
{
    const std::wstring catalogPath = ResolvePieceHierarchyPath(Utf8ToWide(kEnvironmentModelCatalogPath));
    if (!EnsureDirectoryExists(GetDirectoryPart(catalogPath)))
    {
        SetMessage(u8"環境モデル登録簿の保存先を作成できませんでした");
        return false;
    }
    std::ofstream output(catalogPath, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        SetMessage(u8"環境モデル登録簿を保存できませんでした");
        return false;
    }
    output << "# id name path scaleX scaleY scaleZ\n";
    for (const EnvironmentModelAsset& asset : m_environmentModels)
    {
        output << std::quoted(asset.id) << ' '
            << std::quoted(asset.name) << ' '
            << std::quoted(asset.path) << ' '
            << asset.defaultScale.x << ' '
            << asset.defaultScale.y << ' '
            << asset.defaultScale.z << '\n';
    }
    if (!output.good())
    {
        SetMessage(u8"環境モデル登録簿を書き込めませんでした");
        return false;
    }
    return true;
}

int SceneNarakuPieceEditor::FindEnvironmentModelIndexById(const std::string& modelId) const
{
    for (size_t index = 0; index < m_environmentModels.size(); ++index)
    {
        if (m_environmentModels[index].id == modelId) return static_cast<int>(index);
    }
    return -1;
}

int SceneNarakuPieceEditor::FindEnvironmentObjectIndexByCell(int cellX, int cellZ) const
{
    for (size_t index = 0; index < m_piece.environmentObjects.size(); ++index)
    {
        const NarakuPiece::EnvironmentObjectData& object = m_piece.environmentObjects[index];
        if (object.cell.x == cellX && object.cell.z == cellZ) return static_cast<int>(index);
    }
    return -1;
}

bool SceneNarakuPieceEditor::HasEnvironmentObjectAt(int cellX, int cellZ) const
{
    return FindEnvironmentObjectIndexByCell(cellX, cellZ) >= 0;
}

bool SceneNarakuPieceEditor::CanPlaceEnvironmentObject(int cellX, int cellZ, std::string& outMessage) const
{
    const NarakuPiece::CellData* cell = GetCellData(cellX, cellZ);
    if (cell == nullptr || cell->deleted)
    {
        outMessage = u8"削除セルまたは範囲外には配置できません";
        return false;
    }
    if (HasEnvironmentObjectAt(cellX, cellZ))
    {
        outMessage = u8"このセルには環境オブジェクトが配置済みです";
        return false;
    }
    if (FindMiningPointIndexByCell(cellX, cellZ) >= 0 ||
        (m_piece.startReturnCandidate.enabled && m_piece.startReturnCandidate.cell.x == cellX && m_piece.startReturnCandidate.cell.z == cellZ) ||
        (m_piece.layerTransition.loadPointEnabled && m_piece.layerTransition.loadPoint.x == cellX && m_piece.layerTransition.loadPoint.z == cellZ))
    {
        outMessage = u8"ロープ以外のゲームオブジェクトと同じセルには配置できません";
        return false;
    }
    return true;
}

void SceneNarakuPieceEditor::OpenNewEnvironmentModelDialog()
{
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetPreviewHostWindow();
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"3D Model Files\0*.fbx;*.obj;*.gltf;*.glb;*.pmx;*.pmd\0All Files\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&dialog)) return;

    const std::wstring normalizedPath = NormalizePieceHierarchyPath(filePath);
    const std::string path = WideToUtf8(normalizedPath);
    std::string name = path;
    const size_t slash = name.find_last_of('/');
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name.resize(dot);

    ReleaseEnvironmentModelPopupPreview();
    const std::string resolvedPath = WideToUtf8(ResolvePieceHierarchyPath(normalizedPath));
    m_environmentModelPopupPreviewModel = new Model();
    if (!m_environmentModelPopupPreviewModel->Load(resolvedPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(m_environmentModelPopupPreviewModel);
    }
    else
    {
        EnvironmentModelAsset previewAsset;
        previewAsset.model = m_environmentModelPopupPreviewModel;
        UpdateEnvironmentModelBounds(previewAsset);
        m_environmentModelPopupBoundsMin = previewAsset.boundsMin;
        m_environmentModelPopupBoundsMax = previewAsset.boundsMax;
        m_environmentModelPopupPreviewAnchor = previewAsset.previewAnchor;
    }

    std::snprintf(m_environmentModelNameInput.data(), m_environmentModelNameInput.size(), "%s", name.c_str());
    std::snprintf(m_environmentModelPathInput.data(), m_environmentModelPathInput.size(), "%s", path.c_str());
    m_environmentModelScaleInput = { 1.0f, 1.0f, 1.0f };
    m_environmentModelPopupIsNew = true;
    m_requestOpenEnvironmentModelPopup = true;
}

void SceneNarakuPieceEditor::OpenEnvironmentModelSetting()
{
    if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size()))
    {
        SetMessage(u8"設定するモデルをAssetsから選択してください");
        return;
    }
    ReleaseEnvironmentModelPopupPreview();
    const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
    std::snprintf(m_environmentModelNameInput.data(), m_environmentModelNameInput.size(), "%s", asset.name.c_str());
    std::snprintf(m_environmentModelPathInput.data(), m_environmentModelPathInput.size(), "%s", asset.path.c_str());
    m_environmentModelScaleInput = asset.defaultScale;
    m_environmentModelPopupIsNew = false;
    m_requestOpenEnvironmentModelPopup = true;
}

void SceneNarakuPieceEditor::DeleteSelectedEnvironmentModel()
{
    if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size()))
    {
        SetMessage(u8"削除するモデルをAssetsから選択してください");
        return;
    }
    const std::string modelId = m_environmentModels[m_selectedEnvironmentModelIndex].id;
    const bool inUse = std::any_of(m_piece.environmentObjects.begin(), m_piece.environmentObjects.end(),
        [&](const NarakuPiece::EnvironmentObjectData& object) { return object.modelId == modelId; });
    if (inUse)
    {
        SetMessage(u8"現在の小ステージで使用中のモデルは削除できません");
        return;
    }
    const int removedIndex = m_selectedEnvironmentModelIndex;
    EnvironmentModelAsset removedAsset = m_environmentModels[removedIndex];
    m_environmentModels.erase(m_environmentModels.begin() + removedIndex);
    m_selectedEnvironmentModelIndex = m_environmentModels.empty()
        ? -1 : std::min(removedIndex, static_cast<int>(m_environmentModels.size()) - 1);
    if (!SaveEnvironmentModelCatalog())
    {
        m_environmentModels.insert(m_environmentModels.begin() + removedIndex, removedAsset);
        m_selectedEnvironmentModelIndex = removedIndex;
        return;
    }
    SAFE_DELETE(removedAsset.model);
    SAFE_DELETE(removedAsset.thumbnailDepthStencil);
    SAFE_DELETE(removedAsset.thumbnailRenderTarget);
    SetMessage(u8"環境モデルの登録を削除しました");
}

void SceneNarakuPieceEditor::ApplyEnvironmentModelPopup()
{
    const std::string name = m_environmentModelNameInput.data();
    const std::string path = m_environmentModelPathInput.data();
    if (name.empty() || path.empty() || m_environmentModelScaleInput.x <= 0.0f ||
        m_environmentModelScaleInput.y <= 0.0f || m_environmentModelScaleInput.z <= 0.0f)
    {
        SetMessage(u8"モデル名、パス、0より大きいサイズが必要です");
        return;
    }

    const int previousSelectedIndex = m_selectedEnvironmentModelIndex;
    std::string previousName;
    XMFLOAT3 previousScale = {};
    bool previousThumbnailDirty = false;
    if (m_environmentModelPopupIsNew)
    {
        Model* model = m_environmentModelPopupPreviewModel;
        if (model == nullptr)
        {
            const std::string resolvedPath = WideToUtf8(ResolvePieceHierarchyPath(Utf8ToWide(path)));
            model = new Model();
            if (!model->Load(resolvedPath.c_str(), 1.0f, Model::ZFlip))
            {
                SAFE_DELETE(model);
                SetMessage(u8"モデルを読み込めませんでした");
                return;
            }
        }
        unsigned int suffix = 0;
        std::string id;
        do
        {
            char buffer[64] = {};
            std::snprintf(
                buffer,
                sizeof(buffer),
                "environment_model_%lld_%u",
                static_cast<long long>(std::time(nullptr)),
                suffix++);
            id = buffer;
        } while (FindEnvironmentModelIndexById(id) >= 0);

        EnvironmentModelAsset asset;
        asset.id = id;
        asset.name = name;
        asset.path = path;
        asset.defaultScale = m_environmentModelScaleInput;
        asset.model = model;
        UpdateEnvironmentModelBounds(asset);
        m_environmentModels.push_back(asset);
        m_environmentModelPopupPreviewModel = nullptr;
        m_selectedEnvironmentModelIndex = static_cast<int>(m_environmentModels.size()) - 1;
    }
    else
    {
        if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size())) return;
        EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
        previousName = asset.name;
        previousScale = asset.defaultScale;
        previousThumbnailDirty = asset.thumbnailDirty;
        asset.name = name;
        asset.defaultScale = m_environmentModelScaleInput;
        asset.thumbnailDirty = true;
    }
    if (!SaveEnvironmentModelCatalog())
    {
        if (m_environmentModelPopupIsNew)
        {
            EnvironmentModelAsset& addedAsset = m_environmentModels.back();
            m_environmentModelPopupPreviewModel = addedAsset.model;
            addedAsset.model = nullptr;
            SAFE_DELETE(addedAsset.thumbnailDepthStencil);
            SAFE_DELETE(addedAsset.thumbnailRenderTarget);
            m_environmentModels.pop_back();
            m_selectedEnvironmentModelIndex = previousSelectedIndex;
        }
        else
        {
            EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
            asset.name = previousName;
            asset.defaultScale = previousScale;
            asset.thumbnailDirty = previousThumbnailDirty;
        }
        return;
    }
    ImGui::CloseCurrentPopup();
    SetMessage(m_environmentModelPopupIsNew ? u8"環境モデルを登録しました" : u8"モデル設定を更新しました");
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
    MarkPieceDirty();
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

    const bool ropeTool = tool == GridObjectTool::RopeTop ||
        tool == GridObjectTool::RopeBottom ||
        tool == GridObjectTool::LayerRopePoint;
    if (!ropeTool && HasEnvironmentObjectAt(cellX, cellZ))
    {
        outMessage = u8"環境オブジェクトと同じセルにはロープ以外を配置できません";
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

    case GridObjectTool::LayerRopePoint:
        if (m_piece.layerTransition.role == NarakuPiece::LayerTransitionRole::None)
        {
            outMessage = u8"先に層間口役割を設定してください";
            return false;
        }
        if (!cellData->ropeAllowed)
        {
            outMessage = u8"このセルには層間口ロープを配置できません";
            return false;
        }
        return true;

    case GridObjectTool::LayerLoadPoint:
        if (m_piece.layerTransition.role != NarakuPiece::LayerTransitionRole::Exit)
        {
            outMessage = u8"ロード地点は層出口にだけ配置できます";
            return false;
        }
        if (!cellData->walkable)
        {
            outMessage = u8"歩行不可セルにはロード地点を配置できません";
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
        MarkPieceDirty();
        SetMessage(u8"採掘ポイントを削除しました");
        return true;

    case GridObjectKind::Rope:
        if (!m_piece.rope.enabled)
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.rope.enabled = false;
        MarkPieceDirty();
        SetMessage(u8"ロープを削除しました");
        return true;

    case GridObjectKind::StartReturn:
        if (!m_piece.startReturnCandidate.enabled)
        {
            return false;
        }
        PushUndoSnapshot();
        m_piece.startReturnCandidate.enabled = false;
        MarkPieceDirty();
        SetMessage(u8"開始・帰還地点を削除しました");
        return true;

    case GridObjectKind::LayerRopePoint:
        if (!m_piece.layerTransition.ropePointEnabled) return false;
        PushUndoSnapshot();
        m_piece.layerTransition.ropePointEnabled = false;
        ClearGridObjectSelection();
        MarkPieceDirty();
        SetMessage(u8"層間口ロープ端点を削除しました");
        return true;

    case GridObjectKind::LayerLoadPoint:
        if (!m_piece.layerTransition.loadPointEnabled) return false;
        PushUndoSnapshot();
        m_piece.layerTransition.loadPointEnabled = false;
        ClearGridObjectSelection();
        MarkPieceDirty();
        SetMessage(u8"層間口ロード地点を削除しました");
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
    ImGui::SetNextWindowPos(ImVec2(392.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::max(520.0f, workSize.x - 408.0f), std::max(360.0f, workSize.y * 0.62f)),
        ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"3Dプレビュー", &m_showPreviewWindow))
    {
        m_previewImageTopLeft = {};
        m_previewImageScreenTopLeft = {};
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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
    DrawPreviewCompass();
    ImGui::End();
}

DirectX::XMFLOAT2 SceneNarakuPieceEditor::GetCompassScreenDirection(const XMFLOAT3& worldDirection) const
{
    const XMVECTOR viewDirection = XMVector3TransformNormal(
        XMVectorSet(worldDirection.x, worldDirection.y, worldDirection.z, 0.0f),
        XMLoadFloat4x4(&m_viewMatrix));

    XMFLOAT3 viewSpaceDirection = {};
    XMStoreFloat3(&viewSpaceDirection, viewDirection);
    const float length = std::sqrt(
        viewSpaceDirection.x * viewSpaceDirection.x +
        viewSpaceDirection.y * viewSpaceDirection.y);
    if (length <= 0.0001f)
    {
        return {};
    }

    return
    {
        viewSpaceDirection.x / length,
        -viewSpaceDirection.y / length
    };
}

void SceneNarakuPieceEditor::DrawPreviewCompass() const
{
    const float minimumCompassSize =
        kCompassRadius * 2.0f + kCompassMargin * 2.0f + kCompassLabelDistance * 2.0f;
    if (m_previewImageSize.x < minimumCompassSize ||
        m_previewImageSize.y < minimumCompassSize)
    {
        return;
    }

    const ImVec2 imageMin(m_previewImageScreenTopLeft.x, m_previewImageScreenTopLeft.y);
    const ImVec2 imageMax(
        imageMin.x + m_previewImageSize.x,
        imageMin.y + m_previewImageSize.y);
    const ImVec2 center(
        imageMax.x - kCompassMargin - kCompassRadius,
        imageMin.y + kCompassMargin + kCompassRadius);
    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imageMin, imageMax, true);

    drawList->AddCircle(center, kCompassRadius, IM_COL32(220, 230, 240, 220), 32, kCompassLineThickness);
    drawList->AddCircleFilled(center, 2.5f, IM_COL32(220, 230, 240, 230));

    const XMFLOAT3 worldDirections[] =
    {
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
    };

    for (int index = 0; index < static_cast<int>(std::size(worldDirections)); ++index)
    {
        const XMFLOAT2 direction = GetCompassScreenDirection(worldDirections[index]);
        if (direction.x == 0.0f && direction.y == 0.0f)
        {
            continue;
        }

        const ImVec2 endpoint(
            center.x + direction.x * (kCompassRadius - kCompassLinePadding),
            center.y + direction.y * (kCompassRadius - kCompassLinePadding));
        const ImU32 lineColor = (index == 0) ? IM_COL32(245, 95, 95, 230) : IM_COL32(220, 230, 240, 220);
        drawList->AddLine(center, endpoint, lineColor, kCompassLineThickness);

        const ImVec2 textSize = ImGui::CalcTextSize(kDirectionLabels[index]);
        const ImVec2 labelCenter(
            center.x + direction.x * (kCompassRadius + kCompassLabelDistance),
            center.y + direction.y * (kCompassRadius + kCompassLabelDistance));
        ImVec2 textPosition(
            labelCenter.x - textSize.x * 0.5f,
            labelCenter.y - textSize.y * 0.5f);
        const float maxTextX = std::max(imageMin.x, imageMax.x - textSize.x);
        const float maxTextY = std::max(imageMin.y, imageMax.y - textSize.y);
        textPosition.x = ClampFloat(textPosition.x, imageMin.x, maxTextX);
        textPosition.y = ClampFloat(textPosition.y, imageMin.y, maxTextY);
        drawList->AddText(textPosition, lineColor, kDirectionLabels[index]);
    }

    drawList->PopClipRect();
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
    ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"奈落塔ピースエディタ"))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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

void SceneNarakuPieceEditor::DrawPieceBasicWindow()
{
    if (!m_showPieceBasicWindow)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(16.0f, 292.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 230.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"基本情報", &m_showPieceBasicWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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
        MarkPieceDirty();
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
        MarkPieceDirty();
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
        MarkPieceDirty();
    }

    int layerTransitionRole = static_cast<int>(m_piece.layerTransition.role);
    if (ImGui::Combo(u8"層間口役割", &layerTransitionRole, kLayerTransitionRoleLabels, IM_ARRAYSIZE(kLayerTransitionRoleLabels)))
    {
        PushUndoSnapshot();
        m_piece.layerTransition.role = static_cast<NarakuPiece::LayerTransitionRole>(layerTransitionRole);
        if (m_piece.layerTransition.role != NarakuPiece::LayerTransitionRole::Exit)
        {
            m_piece.layerTransition.loadPointEnabled = false;
        }
        MarkPieceDirty();
    }

    ImGui::Text("%s %s", u8"サイズプリセット:", NarakuPiece::ToString(m_piece.sizePreset));
    ImGui::Text("%s %dx%d", u8"グリッド:", m_piece.gridWidth, m_piece.gridDepth);

    int editModeIndex = static_cast<int>(m_editMode);
    if (ImGui::Combo(u8"編集モード", &editModeIndex, kEditModeLabels, IM_ARRAYSIZE(kEditModeLabels)))
    {
        m_editMode = static_cast<EditMode>(editModeIndex);
        ClearTerrainSelection();
        ClearGridObjectSelection();
        m_selectedEnvironmentObjectIndex = -1;
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

    ImGui::SetNextWindowPos(ImVec2(16.0f, 536.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 230.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"接続設定", &m_showPieceConnectionWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    int stageRoleIndex = ToStageRoleIndex(m_piece.stageRole);
    if (ImGui::Combo(u8"ステージ役割", &stageRoleIndex, kStageRoleLabels, IM_ARRAYSIZE(kStageRoleLabels)))
    {
        PushUndoSnapshot();
        m_piece.stageRole = FromStageRoleIndex(stageRoleIndex);
        MarkPieceDirty();
    }

    int stageCategoryIndex = ToStageCategoryIndex(m_piece.stageCategory);
    if (ImGui::Combo(u8"ステージカテゴリ", &stageCategoryIndex, kStageCategoryLabels, IM_ARRAYSIZE(kStageCategoryLabels)))
    {
        PushUndoSnapshot();
        m_piece.stageCategory = FromStageCategoryIndex(stageCategoryIndex);
        MarkPieceDirty();
    }

    const auto drawEdgeCategoryCombo = [&](const char* label, NarakuPiece::StageCategory& category)
    {
        int edgeIndex = ToStageCategoryIndex(category);
        if (ImGui::Combo(label, &edgeIndex, kStageCategoryLabels, IM_ARRAYSIZE(kStageCategoryLabels)))
        {
            PushUndoSnapshot();
            category = FromStageCategoryIndex(edgeIndex);
            MarkPieceDirty();
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
        MarkPieceDirty();
    }
    bool southLocked = m_piece.lockedEdges.south;
    if (ImGui::Checkbox(u8"南をロック", &southLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.south = southLocked;
        MarkPieceDirty();
    }
    bool eastLocked = m_piece.lockedEdges.east;
    if (ImGui::Checkbox(u8"東をロック", &eastLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.east = eastLocked;
        MarkPieceDirty();
    }
    bool westLocked = m_piece.lockedEdges.west;
    if (ImGui::Checkbox(u8"西をロック", &westLocked))
    {
        PushUndoSnapshot();
        m_piece.lockedEdges.west = westLocked;
        MarkPieceDirty();
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawTerrainEditWindow()
{
    if (!m_showTerrainEditWindow)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(392.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 340.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"地形編集", &m_showTerrainEditWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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
            MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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
                    MarkPieceDirty();
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

    ImGui::SetNextWindowPos(ImVec2(16.0f, 780.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 190.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"配置ツール", &m_showGridObjectPlacementWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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

    ImGui::SetNextWindowPos(ImVec2(392.0f, 372.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 280.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"選択オブジェクト", &m_showGridObjectSelectionWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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
                MarkPieceDirty();
            }

            int visualType = point.visualType;
            if (ImGui::SliderInt(u8"見た目タイプ", &visualType, 0, 3))
            {
                PushUndoSnapshot();
                point.visualType = visualType;
                MarkPieceDirty();
            }

            bool initiallyRecorded = point.initiallyRecorded;
            if (ImGui::Checkbox(u8"初期記録済み", &initiallyRecorded))
            {
                PushUndoSnapshot();
                point.initiallyRecorded = initiallyRecorded;
                MarkPieceDirty();
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
            MarkPieceDirty();
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
            MarkPieceDirty();
        }
        ImGui::Text("%s (%d, %d)", u8"セル", m_piece.startReturnCandidate.cell.x, m_piece.startReturnCandidate.cell.z);
        int facingIndex = ToDirectionIndex(m_piece.startReturnCandidate.facing);
        if (ImGui::Combo(u8"向き", &facingIndex, kDirectionLabels, IM_ARRAYSIZE(kDirectionLabels)))
        {
            PushUndoSnapshot();
            m_piece.startReturnCandidate.facing = FromDirectionIndex(facingIndex);
            MarkPieceDirty();
        }
        if (ImGui::Button(u8"開始帰還候補を削除"))
        {
            DeleteSelectedGridObject();
        }
        break;
    }

    case GridObjectKind::LayerRopePoint:
        ImGui::Text("%s (%d, %d)", u8"層間口ロープ端点", m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z);
        if (ImGui::Button(u8"層間口ロープ端点を削除")) DeleteSelectedGridObject();
        break;

    case GridObjectKind::LayerLoadPoint:
        ImGui::Text("%s (%d, %d)", u8"層間口ロード地点", m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z);
        if (ImGui::Button(u8"層間口ロード地点を削除")) DeleteSelectedGridObject();
        break;

    case GridObjectKind::None:
    default:
        ImGui::TextUnformatted(u8"ゲームオブジェクトが未選択です");
        break;
    }

    ImGui::End();
}

void SceneNarakuPieceEditor::DrawEnvironmentAssetsWindow()
{
    if (!m_showEnvironmentAssetsWindow) return;

    ImGui::SetNextWindowPos(ImVec2(768.0f, 668.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(460.0f, 330.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Assets", &m_showEnvironmentAssetsWindow))
    {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(160.0f);
    ImGui::SliderFloat(u8"表示サイズ", &m_environmentAssetTileSize, 72.0f, 160.0f, "%.0f px");
    ImGui::Separator();

    if (m_environmentModels.empty())
    {
        ImGui::TextUnformatted(u8"登録モデルなし");
    }
    else
    {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float tileSize = std::max(48.0f, std::min(m_environmentAssetTileSize, availableWidth));
        const int columnCount = std::max(1, static_cast<int>((availableWidth + spacing) / (tileSize + spacing)));
        const float padding = 5.0f;
        if (ImGui::BeginTable("EnvironmentAssetTiles", columnCount, ImGuiTableFlags_SizingStretchSame))
        {
            for (size_t index = 0; index < m_environmentModels.size(); ++index)
            {
                ImGui::TableNextColumn();
                ImGui::PushID(static_cast<int>(index));
                const bool selected = m_selectedEnvironmentModelIndex == static_cast<int>(index);
                const ImVec2 tileMin = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##EnvironmentAssetTile", ImVec2(tileSize, tileSize));
                const bool hovered = ImGui::IsItemHovered();
                const bool clicked = ImGui::IsItemClicked();
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImVec2 tileMax(tileMin.x + tileSize, tileMin.y + tileSize);
                const ImU32 backgroundColor = selected
                    ? IM_COL32(48, 102, 156, 255)
                    : hovered ? IM_COL32(47, 55, 67, 255) : IM_COL32(31, 36, 45, 255);
                const ImU32 borderColor = selected ? IM_COL32(125, 200, 255, 255) : IM_COL32(76, 85, 101, 255);
                drawList->AddRectFilled(tileMin, tileMax, backgroundColor, 6.0f);
                drawList->AddRect(tileMin, tileMax, borderColor, 6.0f, 0, selected ? 2.0f : 1.0f);

                const ImVec2 imageMin(tileMin.x + padding, tileMin.y + padding);
                const ImVec2 imageMax(tileMax.x - padding, tileMax.y - 25.0f);
                void* textureId = GetEnvironmentModelThumbnailTextureId(static_cast<int>(index), 128U);
                if (textureId != nullptr)
                {
                    drawList->AddImage(textureId, imageMin, imageMax);
                }
                else
                {
                    drawList->AddRectFilled(imageMin, imageMax, IM_COL32(22, 26, 33, 255), 4.0f);
                }

                const std::string& name = m_environmentModels[index].name;
                const ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
                const float textX = textSize.x <= tileSize - padding * 2.0f
                    ? tileMin.x + (tileSize - textSize.x) * 0.5f
                    : tileMin.x + padding;
                drawList->PushClipRect(
                    ImVec2(tileMin.x + padding, tileMax.y - 23.0f),
                    ImVec2(tileMax.x - padding, tileMax.y - 3.0f),
                    true);
                drawList->AddText(ImVec2(textX, tileMax.y - 20.0f), IM_COL32(235, 239, 245, 255), name.c_str());
                drawList->PopClipRect();

                if (hovered) ImGui::SetTooltip("%s", name.c_str());
                if (clicked)
                {
                    m_selectedEnvironmentModelIndex = static_cast<int>(index);
                    m_editMode = EditMode::EnvironmentObject;
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    if (m_selectedEnvironmentModelIndex >= 0 && m_selectedEnvironmentModelIndex < static_cast<int>(m_environmentModels.size()))
    {
        const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
        ImGui::SeparatorText(u8"選択モデル");
        ImGui::TextUnformatted(asset.name.c_str());
        ImGui::TextWrapped("%s", asset.path.c_str());
        ImGui::Text("%s %.3f, %.3f, %.3f", u8"既定サイズ", asset.defaultScale.x, asset.defaultScale.y, asset.defaultScale.z);
    }

    ImGui::SeparatorText(u8"配置オブジェクト");
    if (m_selectedEnvironmentObjectIndex >= 0 &&
        m_selectedEnvironmentObjectIndex < static_cast<int>(m_piece.environmentObjects.size()))
    {
        NarakuPiece::EnvironmentObjectData& object = m_piece.environmentObjects[m_selectedEnvironmentObjectIndex];
        const int assetIndex = FindEnvironmentModelIndexById(object.modelId);
        const char* modelName = assetIndex >= 0 ? m_environmentModels[assetIndex].name.c_str() : object.modelId.c_str();
        ImGui::Text("%s: %s", u8"モデル", modelName);
        ImGui::Text("%s: (%d, %d)", u8"セル", object.cell.x, object.cell.z);
        float scale[3] = { object.scaleX, object.scaleY, object.scaleZ };
        const bool scaleChanged = ImGui::DragFloat3(u8"サイズ", scale, 0.01f, 0.01f, 100.0f, "%.3f");
        if (ImGui::IsItemActivated()) PushUndoSnapshot();
        if (scaleChanged)
        {
            object.scaleX = std::max(0.01f, scale[0]);
            object.scaleY = std::max(0.01f, scale[1]);
            object.scaleZ = std::max(0.01f, scale[2]);
            MarkPieceDirty();
        }
        if (ImGui::Button(u8"環境オブジェクトを削除"))
        {
            PushUndoSnapshot();
            m_piece.environmentObjects.erase(m_piece.environmentObjects.begin() + m_selectedEnvironmentObjectIndex);
            m_selectedEnvironmentObjectIndex = -1;
            MarkPieceDirty();
            SetMessage(u8"環境オブジェクトを削除しました");
        }
    }
    else
    {
        ImGui::TextUnformatted(u8"配置オブジェクトが未選択です");
    }
    ImGui::End();
}

void SceneNarakuPieceEditor::DrawEnvironmentModelPopup()
{
    if (m_requestOpenEnvironmentModelPopup)
    {
        ImGui::OpenPopup(u8"環境モデル設定");
        m_requestOpenEnvironmentModelPopup = false;
    }
    if (!ImGui::BeginPopupModal(u8"環境モデル設定", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ReleaseEnvironmentModelPopupPreview();
        return;
    }

    ImGui::InputText(u8"モデル名", m_environmentModelNameInput.data(), m_environmentModelNameInput.size());
    ImGui::InputText(u8"モデルパス", m_environmentModelPathInput.data(), m_environmentModelPathInput.size(), ImGuiInputTextFlags_ReadOnly);
    ImGui::DragFloat3(u8"既定サイズ", &m_environmentModelScaleInput.x, 0.01f, 0.01f, 100.0f, "%.3f");
    ImGui::SeparatorText(u8"サイズプレビュー");
    constexpr unsigned int previewSize = 320U;
    if (void* textureId = GetEnvironmentModelPopupPreviewTextureId(previewSize))
    {
        ImGui::Image(textureId, ImVec2(static_cast<float>(previewSize), static_cast<float>(previewSize)));
    }
    else
    {
        ImGui::Dummy(ImVec2(static_cast<float>(previewSize), 1.0f));
        ImGui::TextUnformatted(u8"モデルを表示できません");
    }
    if (ImGui::Button(m_environmentModelPopupIsNew ? u8"追加" : u8"更新"))
    {
        ApplyEnvironmentModelPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"キャンセル")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

void SceneNarakuPieceEditor::DrawPieceFileAndValidationWindow()
{
    if (!m_showPieceFileAndValidationWindow)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(768.0f, 372.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 280.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"保存・検証", &m_showPieceFileAndValidationWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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

void SceneNarakuPieceEditor::DrawNewPiecePopup()
{
    bool keepOpen = true;
    if (!ImGui::BeginPopupModal(u8"新規ピースを作成", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::TextUnformatted(u8"新規ファイル名");
    ImGui::SetNextItemWidth(320.0f);
    if (ImGui::InputText(u8"##NewPieceFileName", m_newPieceFileNameInput.data(), m_newPieceFileNameInput.size()))
    {
        CommitNewPieceFileNameInput();
    }

    if (ImGui::Button(u8"作成", ImVec2(120.0f, 0.0f)))
    {
        CommitNewPieceFileNameInput();
        if (m_newPieceFileName.empty())
        {
            SetMessage(u8"新規ファイル名を入力してください");
        }
        else if (HasInvalidFileNameChar(m_newPieceFileName))
        {
            SetMessage(u8"新規ファイル名に使用できない文字が含まれています");
        }
        else
        {
            CreateNewPiece(m_newPieceFileName);
            SetMessage(u8"新規ピースを作成しました");
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

void SceneNarakuPieceEditor::DrawRenamePiecePopup()
{
    bool keepOpen = true;
    if (!ImGui::BeginPopupModal(u8"ピース名を変更", &keepOpen, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::TextUnformatted(u8"新しいファイル名");
    ImGui::SetNextItemWidth(320.0f);
    ImGui::InputText(u8"##RenamePieceFileName", m_renameFileNameInput.data(), m_renameFileNameInput.size());

    if (ImGui::Button(u8"変更", ImVec2(120.0f, 0.0f)))
    {
        if (RenameCurrentPiece())
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

void SceneNarakuPieceEditor::CommitNewPieceFileNameInput()
{
    m_newPieceFileName = EnsureJsonFileName(Utf8ToWide(m_newPieceFileNameInput.data()));
}

std::wstring SceneNarakuPieceEditor::GetDisplayFileName() const
{
    return m_saveFileName.empty() ? L"(unnamed)" : m_saveFileName;
}

std::wstring SceneNarakuPieceEditor::BuildEditingStatusLabel() const
{
    std::wstring label = L"\x7DE8\x96C6\x4E2D: ";
    label += GetDisplayFileName();
    if (m_isPieceDirty)
    {
        label += L" *";
    }
    label += m_isPieceDirty ? L" [\x672A\x4FDD\x5B58]" : L" [\x4FDD\x5B58\x6E08\x307F]";
    label += m_saveAsDraft ? L" [\x4E0B\x66F8\x304D]" : L" [\x5B8C\x6210]";
    return label;
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

    std::wstring title = L"NarakuProto - PieceEditor - ";
    title += GetDisplayFileName();
    if (m_isPieceDirty)
    {
        title += L" *";
    }
    title += m_saveAsDraft ? L" [Draft]" : L" [Completed]";
    ::SetWindowTextW(mainWindow, title.c_str());
    if (HMENU menuBar = ::GetMenu(mainWindow))
    {
        SyncNativeMenuState(menuBar);
    }
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

    m_piece.lastModified = WideToUtf8(BuildCurrentDateTimeString());
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
    RegisterPieceHierarchyEntry(savePath, !saveAsDraft, Utf8ToWide(m_piece.lastModified));
    if (!SavePieceHierarchyEntries())
    {
        SetMessage(u8"ピース保存には成功しましたがHierarchy登録ファイルの保存に失敗しました");
    }
    SyncSaveFileNameInput();
    MarkPieceClean();
    return true;
}

bool SceneNarakuPieceEditor::ConfirmDiscardDirtyChanges(const wchar_t* actionName)
{
    if (!m_isPieceDirty)
    {
        return true;
    }

    std::wstring message = L"現在のピースは未保存です。";
    if (actionName != nullptr && actionName[0] != L'\0')
    {
        message += actionName;
        message += L"の前に保存しますか?";
    }
    else
    {
        message += L"保存しますか?";
    }

    const int result = ::MessageBoxW(
        ::GetActiveWindow(),
        message.c_str(),
        L"PieceEditor",
        MB_ICONQUESTION | MB_YESNOCANCEL | MB_DEFBUTTON1);
    if (result == IDYES)
    {
        return SavePiece(m_saveAsDraft);
    }
    if (result == IDNO)
    {
        return true;
    }
    return false;
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
    MarkPieceClean();
    RegisterPieceHierarchyEntry(path, IsCompletedPiecePath(path), Utf8ToWide(m_piece.lastModified));
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
    m_piece = loadedPiece;
    ClearTerrainSelection();
    m_selectedX = ClampInt(0, 0, m_piece.gridWidth - 1);
    m_selectedZ = ClampInt(0, 0, m_piece.gridDepth - 1);
    m_selectedCellX = ClampInt(0, 0, std::max(0, m_piece.gridWidth - 2));
    m_selectedCellZ = ClampInt(0, 0, std::max(0, m_piece.gridDepth - 2));
    EnsureSelectionNotEmpty();
    EnsureCellSelectionValid();
    ClearGridObjectSelection();
    m_selectedEnvironmentObjectIndex = -1;
    m_undoStack.clear();
    m_redoStack.clear();
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
    InvalidateValidationState();
    RefreshValidationIssues();
}

void SceneNarakuPieceEditor::CreateNewPiece(const std::wstring& fileName)
{
    m_piece = NarakuPiece::CreateDefaultPiece(NarakuPiece::SizePreset::Size16x16);
    for (NarakuPiece::CellData& cell : m_piece.cells)
    {
        cell.deleted = false;
    }

    m_selectedX = 0;
    m_selectedZ = 0;
    m_selectedVertices.clear();
    m_selectedVertices.push_back(VertexSelection{ 0, 0 });
    m_selectedCellX = 0;
    m_selectedCellZ = 0;
    m_selectedCells.clear();
    m_selectedCells.push_back(CellSelection{ 0, 0 });
    m_editMode = EditMode::Height;
    m_terrainSelectionMode = TerrainSelectionMode::Vertex;
    m_gridObjectTool = GridObjectTool::MiningPoint;
    m_selectedGridObjectKind = GridObjectKind::None;
    m_selectedMiningPointIndex = -1;
    m_selectedEnvironmentObjectIndex = -1;
    m_hoverCellX = -1;
    m_hoverCellZ = -1;
    m_newMiningVisualType = 0;
    m_newMiningInitiallyRecorded = false;
    m_saveFileName = EnsureJsonFileName(fileName);
    m_saveAsDraft = true;
    m_undoStack.clear();
    m_redoStack.clear();
    m_validationIssues.clear();
    m_message.clear();
    m_heightDragFloatEditing = false;
    m_draggingHeight = false;
    SyncSaveFileNameInput();
    InvalidateValidationState();
    RefreshValidationIssues();
    MarkPieceDirty();
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

bool SceneNarakuPieceEditor::RenameCurrentPiece()
{
    const std::wstring newFileName = EnsureJsonFileName(Utf8ToWide(m_renameFileNameInput.data()));
    if (newFileName.empty() || HasInvalidFileNameChar(newFileName))
    {
        SetMessage(u8"変更後ファイル名に使用できない文字が含まれています");
        return false;
    }

    const std::wstring oldPath = GetCurrentSaveTargetPath();
    const std::wstring oldRelativePath = NormalizePieceHierarchyPath(oldPath);
    const std::wstring newPath = m_saveAsDraft
        ? NarakuPiece::MakeDraftPiecePath(m_piece.abyssLayer, newFileName)
        : NarakuPiece::MakeCompletedPiecePath(m_piece.abyssLayer, newFileName);
    const std::wstring newRelativePath = NormalizePieceHierarchyPath(newPath);

    if (ToLowerWide(NormalizePathSeparators(oldRelativePath)) ==
        ToLowerWide(NormalizePathSeparators(newRelativePath)))
    {
        m_saveFileName = newFileName;
        SyncSaveFileNameInput();
        SetMessage(u8"ピース名を更新しました");
        return true;
    }

    const std::wstring oldAbsolutePath = ResolvePieceHierarchyPath(oldRelativePath);
    const std::wstring newAbsolutePath = ResolvePieceHierarchyPath(newRelativePath);
    const bool hadExistingFile = PathExists(oldAbsolutePath);
    if (hadExistingFile)
    {
        if (PathExists(newAbsolutePath))
        {
            SetMessage(u8"変更先のファイル名は既に存在します");
            return false;
        }

        if (!::MoveFileExW(oldAbsolutePath.c_str(), newAbsolutePath.c_str(), MOVEFILE_COPY_ALLOWED))
        {
            SetMessage(u8"ファイル名の変更に失敗しました");
            return false;
        }
    }

    const std::wstring lastModified = hadExistingFile
        ? BuildCurrentDateTimeString()
        : Utf8ToWide(m_piece.lastModified);
    RemovePieceHierarchyEntry(oldRelativePath);
    RegisterPieceHierarchyEntry(newRelativePath, !m_saveAsDraft, lastModified);
    SavePieceHierarchyEntries();

    m_saveFileName = newFileName;
    if (!lastModified.empty())
    {
        m_piece.lastModified = WideToUtf8(lastModified);
    }
    SyncSaveFileNameInput();
    if (hadExistingFile)
    {
        MarkPieceClean();
    }
    else
    {
        MarkPieceDirty();
    }

    SetMessage(u8"ピース名を変更しました");
    return true;
}

bool SceneNarakuPieceEditor::DeleteCurrentPiece()
{
    const std::wstring targetPath = GetCurrentSaveTargetPath();
    const std::wstring targetRelativePath = NormalizePieceHierarchyPath(targetPath);
    const std::wstring targetAbsolutePath = ResolvePieceHierarchyPath(targetRelativePath);
    const std::wstring displayName = GetDisplayFileName();

    std::wstring confirmMessage = L"";
    confirmMessage += displayName;
    confirmMessage += L" を削除しますか?";
    if (::MessageBoxW(::GetActiveWindow(), confirmMessage.c_str(), L"PieceEditor", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES)
    {
        return false;
    }

    if (PathExists(targetAbsolutePath) && !::DeleteFileW(targetAbsolutePath.c_str()))
    {
        SetMessage(u8"ファイル削除に失敗しました");
        return false;
    }

    RemovePieceHierarchyEntry(targetRelativePath);
    SavePieceHierarchyEntries();
    CreateNewPiece(displayName);
    SetMessage(u8"ピースを削除して新規状態へ移行しました");
    return true;
}

void SceneNarakuPieceEditor::DrawPieceHierarchyWindow()
{
    if (!m_showPieceHierarchyWindow)
    {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(1204.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 300.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"小ステージHierarchy", &m_showPieceHierarchyWindow))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button(u8"再読込"))
    {
        ReloadPieceHierarchyEntries();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"現在のピースを登録"))
    {
        RegisterCurrentPieceToHierarchy();
    }

    const char* sortModeLabels[] = { u8"デフォルト", u8"日付", u8"名前" };
    int sortMode = static_cast<int>(m_pieceHierarchySortMode);
    ImGui::SetNextItemWidth(150.0f);
    if (ImGui::Combo(u8"並び順", &sortMode, sortModeLabels, IM_ARRAYSIZE(sortModeLabels)))
    {
        m_pieceHierarchySortMode = static_cast<PieceHierarchySortMode>(sortMode);
    }
    ImGui::SameLine();
    ImGui::Checkbox(u8"降順", &m_pieceHierarchySortDescending);

    ImGui::Separator();
    if (m_pieceHierarchyEntries.empty())
    {
        ImGui::TextUnformatted(u8"登録済みの小ステージはありません");
        ImGui::End();
        return;
    }

    const std::wstring currentPath = ToLowerWide(NormalizePathSeparators(GetCurrentEditingPieceRelativePath()));
    const std::vector<const PieceHierarchyEntry*> sortedEntries = BuildSortedPieceHierarchyEntries();
    for (const PieceHierarchyEntry* entry : sortedEntries)
    {
        const std::wstring prefix =
            L"[" + BuildHierarchyDateLabel(entry->lastModified) + L" : " + (entry->isCompleted ? L"完成" : L"下書き") + L"] ";
        const std::string label = WideToUtf8(prefix + entry->fileName);
        const bool isSelected = ToLowerWide(NormalizePathSeparators(entry->relativePath)) == currentPath;
        if (ImGui::Selectable(label.c_str(), isSelected))
        {
            if (!ConfirmDiscardDirtyChanges(L"読込"))
            {
                continue;
            }

            const std::wstring absolutePath = ResolvePieceHierarchyPath(entry->relativePath);
            if (!PathExists(absolutePath))
            {
                HandleMissingHierarchyEntry(*entry);
                ImGui::End();
                return;
            }
            else
            {
                if (!LoadPieceFromPath(absolutePath))
                {
                    HandleMissingHierarchyEntry(*entry);
                    ImGui::End();
                    return;
                }
                ImGui::End();
                return;
            }
        }

        if (ImGui::IsItemHovered())
        {
            ImGui::SetTooltip("%s", WideToUtf8(entry->relativePath).c_str());
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
    m_nextPieceHierarchyInsertionOrder = 0;
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
        std::string lastModifiedUtf8;
        if (!std::getline(lineStream, fileNameUtf8, '\t') ||
            !std::getline(lineStream, relativePathUtf8, '\t') ||
            !std::getline(lineStream, completedFlag))
        {
            continue;
        }
        std::getline(lineStream, lastModifiedUtf8);

        const std::wstring relativePath = NormalizePathSeparators(Utf8ToWide(relativePathUtf8));
        const bool isCompleted = (completedFlag == "1");
        RegisterPieceHierarchyEntry(relativePath, isCompleted, Utf8ToWide(lastModifiedUtf8));
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
            << (entry.isCompleted ? '1' : '0') << '\t'
            << WideToUtf8(entry.lastModified) << "\n";
    }
    return static_cast<bool>(stream);
}

bool SceneNarakuPieceEditor::RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted)
{
    return RegisterPieceHierarchyEntry(path, isCompleted, std::wstring());
}

bool SceneNarakuPieceEditor::RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted, const std::wstring& lastModified)
{
    const std::wstring normalizedPath = NormalizePieceHierarchyPath(path);
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(normalizedPath));
    const std::wstring fileName = EnsureJsonFileName(GetFileNamePart(normalizedPath));

    for (PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        if (ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey)
        {
            const std::wstring nextLastModified = lastModified.empty() ? entry.lastModified : lastModified;
            const bool changed = entry.fileName != fileName ||
                entry.relativePath != normalizedPath ||
                entry.isCompleted != isCompleted ||
                entry.lastModified != nextLastModified;
            entry.fileName = fileName;
            entry.relativePath = normalizedPath;
            entry.isCompleted = isCompleted;
            entry.lastModified = nextLastModified;
            return changed;
        }
    }

    PieceHierarchyEntry entry;
    entry.fileName = fileName;
    entry.relativePath = normalizedPath;
    entry.isCompleted = isCompleted;
    entry.lastModified = lastModified;
    entry.insertionOrder = m_nextPieceHierarchyInsertionOrder++;
    m_pieceHierarchyEntries.push_back(entry);
    return true;
}

bool SceneNarakuPieceEditor::RemovePieceHierarchyEntry(const std::wstring& path)
{
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(NormalizePieceHierarchyPath(path)));
    const auto it = std::remove_if(
        m_pieceHierarchyEntries.begin(),
        m_pieceHierarchyEntries.end(),
        [&normalizedKey](const PieceHierarchyEntry& entry)
        {
            return ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey;
        });
    if (it == m_pieceHierarchyEntries.end())
    {
        return false;
    }

    m_pieceHierarchyEntries.erase(it, m_pieceHierarchyEntries.end());
    return true;
}

SceneNarakuPieceEditor::PieceHierarchyEntry* SceneNarakuPieceEditor::FindPieceHierarchyEntry(const std::wstring& path)
{
    const std::wstring normalizedKey = ToLowerWide(NormalizePathSeparators(NormalizePieceHierarchyPath(path)));
    for (PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        if (ToLowerWide(NormalizePathSeparators(entry.relativePath)) == normalizedKey)
        {
            return &entry;
        }
    }
    return nullptr;
}

std::vector<const SceneNarakuPieceEditor::PieceHierarchyEntry*> SceneNarakuPieceEditor::BuildSortedPieceHierarchyEntries() const
{
    std::vector<const PieceHierarchyEntry*> entries;
    entries.reserve(m_pieceHierarchyEntries.size());
    for (const PieceHierarchyEntry& entry : m_pieceHierarchyEntries)
    {
        entries.push_back(&entry);
    }

    auto comparator = [this](const PieceHierarchyEntry* lhs, const PieceHierarchyEntry* rhs)
    {
        switch (m_pieceHierarchySortMode)
        {
        case PieceHierarchySortMode::LastModified:
            if (lhs->lastModified != rhs->lastModified)
            {
                return lhs->lastModified < rhs->lastModified;
            }
            break;
        case PieceHierarchySortMode::FileName:
            if (lhs->fileName != rhs->fileName)
            {
                return lhs->fileName < rhs->fileName;
            }
            break;
        case PieceHierarchySortMode::Insertion:
        default:
            if (lhs->insertionOrder != rhs->insertionOrder)
            {
                return lhs->insertionOrder < rhs->insertionOrder;
            }
            break;
        }

        return lhs->relativePath < rhs->relativePath;
    };
    std::sort(entries.begin(), entries.end(), comparator);
    if (m_pieceHierarchySortDescending)
    {
        std::reverse(entries.begin(), entries.end());
    }
    return entries;
}

std::wstring SceneNarakuPieceEditor::BuildHierarchyDateLabel(const std::wstring& lastModified) const
{
    if (lastModified.size() >= 10)
    {
        return lastModified.substr(0, 10);
    }
    return L"未記録";
}

void SceneNarakuPieceEditor::HandleMissingHierarchyEntry(const PieceHierarchyEntry& entry)
{
    const std::wstring message =
        entry.fileName + L" の読込に失敗しました。Hierarchyから削除しますか?";
    const int result = ::MessageBoxW(::GetActiveWindow(), message.c_str(), L"PieceEditor", MB_ICONWARNING | MB_YESNO);
    if (result == IDYES)
    {
        RemovePieceHierarchyEntry(entry.relativePath);
        SavePieceHierarchyEntries();
        SetMessage(u8"読込不能なHierarchy項目を削除しました");
        return;
    }

    m_saveFileName = EnsureJsonFileName(entry.fileName);
    m_saveAsDraft = !entry.isCompleted;
    CreateNewPiece(m_saveFileName);
    m_saveAsDraft = !entry.isCompleted;
    SyncSaveFileNameInput();
    if (!entry.lastModified.empty())
    {
        m_piece.lastModified = WideToUtf8(entry.lastModified);
    }
    SetMessage(u8"読込不能のため同名新規ピース状態へ移行しました");
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
    const bool changed = RegisterPieceHierarchyEntry(targetPath, !m_saveAsDraft, Utf8ToWide(m_piece.lastModified));
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

    ImGui::SetNextWindowPos(ImVec2(1204.0f, 332.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"高さグリッド", &m_showHeightGridWindow))
    {
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

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

    if (m_piece.layerTransition.ropePointEnabled &&
        IsValidCell(m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z))
    {
        const XMFLOAT3 center = GetCellWorldPosition(m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z);
        const XMFLOAT4 color = (m_selectedGridObjectKind == GridObjectKind::LayerRopePoint)
            ? XMFLOAT4{ 1.0f, 0.85f, 0.25f, 1.0f }
            : XMFLOAT4{ 0.95f, 0.65f, 0.15f, 1.0f };
        DrawDebugWireBox3D({ center.x, center.y + 0.55f, center.z }, { 0.65f, 1.10f, 0.65f }, color);
    }

    if (m_piece.layerTransition.loadPointEnabled &&
        IsValidCell(m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z))
    {
        const XMFLOAT3 center = GetCellWorldPosition(m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z);
        const XMFLOAT4 color = (m_selectedGridObjectKind == GridObjectKind::LayerLoadPoint)
            ? XMFLOAT4{ 0.35f, 0.95f, 1.0f, 1.0f }
            : XMFLOAT4{ 0.20f, 0.70f, 0.95f, 1.0f };
        DrawDebugWireBox3D({ center.x, center.y + 0.20f, center.z }, { 1.0f, 0.35f, 1.0f }, color);
    }

    if ((m_editMode == EditMode::GridObject || m_editMode == EditMode::EnvironmentObject) &&
        IsValidCell(m_hoverCellX, m_hoverCellZ))
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

    DrawEnvironmentObjects3D();
    Geometory::DrawLines();
}

void SceneNarakuPieceEditor::DrawEnvironmentObjects3D() const
{
    const float cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 eye =
    {
        m_cameraTarget.x + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance
    };

    for (size_t index = 0; index < m_piece.environmentObjects.size(); ++index)
    {
        const NarakuPiece::EnvironmentObjectData& object = m_piece.environmentObjects[index];
        const int assetIndex = FindEnvironmentModelIndexById(object.modelId);
        if (assetIndex < 0 || !IsValidCell(object.cell.x, object.cell.z)) continue;
        const EnvironmentModelAsset& asset = m_environmentModels[assetIndex];
        if (asset.model == nullptr) continue;

        const XMFLOAT3 center = GetCellWorldPosition(object.cell.x, object.cell.z);
        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(-asset.previewAnchor.x, -asset.previewAnchor.y, -asset.previewAnchor.z) *
            XMMatrixScaling(object.scaleX, object.scaleY, object.scaleZ) *
            XMMatrixTranslation(center.x, center.y, center.z)));
        XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
        XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
        ShaderList::SetWVP(wvp);
        ShaderList::SetCameraPos(eye);
        asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = asset.model->GetMaterial(mesh->materialID);
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                ShaderList::SetMaterial(material);
            }
            asset.model->Draw(static_cast<int>(meshIndex));
        }

        if (m_selectedEnvironmentObjectIndex == static_cast<int>(index))
        {
            DrawDebugWireBox3D(
                { center.x, center.y + 0.5f * object.scaleY, center.z },
                { std::max(0.5f, object.scaleX), std::max(0.5f, object.scaleY), std::max(0.5f, object.scaleZ) },
                { 0.25f, 0.85f, 1.0f, 1.0f });
        }
    }
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
    snapshot.selectedEnvironmentObjectIndex = m_selectedEnvironmentObjectIndex;
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
    m_selectedEnvironmentObjectIndex = snapshot.selectedEnvironmentObjectIndex;
    EnsureSelectionNotEmpty();
    EnsureCellSelectionValid();
    if (m_selectedGridObjectKind == GridObjectKind::MiningPoint &&
        (m_selectedMiningPointIndex < 0 || m_selectedMiningPointIndex >= static_cast<int>(m_piece.miningPoints.size())))
    {
        ClearGridObjectSelection();
    }
    if (m_selectedEnvironmentObjectIndex < 0 ||
        m_selectedEnvironmentObjectIndex >= static_cast<int>(m_piece.environmentObjects.size()))
    {
        m_selectedEnvironmentObjectIndex = -1;
    }
    MarkPieceDirty();
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
        MarkPieceDirty();
        SetMessage(u8"採掘ポイントを追加しました");
        return;
    }

    case GridObjectTool::RopeTop:
        PushUndoSnapshot();
        m_piece.rope.enabled = true;
        m_piece.rope.top.x = cellX;
        m_piece.rope.top.z = cellZ;
        SelectRope();
        MarkPieceDirty();
        SetMessage(u8"ロープ上端を設定しました");
        return;

    case GridObjectTool::RopeBottom:
        PushUndoSnapshot();
        m_piece.rope.enabled = true;
        m_piece.rope.bottom.x = cellX;
        m_piece.rope.bottom.z = cellZ;
        SelectRope();
        MarkPieceDirty();
        SetMessage(u8"ロープ下端を設定しました");
        return;

    case GridObjectTool::StartReturn:
        PushUndoSnapshot();
        m_piece.startReturnCandidate.enabled = true;
        m_piece.startReturnCandidate.cell.x = cellX;
        m_piece.startReturnCandidate.cell.z = cellZ;
        SelectStartReturn();
        MarkPieceDirty();
        SetMessage(u8"開始・帰還地点を設定しました");
        return;

    case GridObjectTool::LayerRopePoint:
        PushUndoSnapshot();
        m_piece.layerTransition.ropePointEnabled = true;
        m_piece.layerTransition.ropePoint = { cellX, cellZ };
        m_selectedGridObjectKind = GridObjectKind::LayerRopePoint;
        m_selectedMiningPointIndex = -1;
        MarkPieceDirty();
        SetMessage(u8"層間口ロープ端点を設定しました");
        return;

    case GridObjectTool::LayerLoadPoint:
        PushUndoSnapshot();
        m_piece.layerTransition.loadPointEnabled = true;
        m_piece.layerTransition.loadPoint = { cellX, cellZ };
        m_selectedGridObjectKind = GridObjectKind::LayerLoadPoint;
        m_selectedMiningPointIndex = -1;
        MarkPieceDirty();
        SetMessage(u8"層間口ロード地点を設定しました");
        return;

    default:
        return;
    }
}

void SceneNarakuPieceEditor::UpdateEnvironmentObjectEditing()
{
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool allowPreviewInput = IsMouseInsidePreviewImage() || m_previewImageHovered;

    if (allowPreviewInput && PickTerrainCell(mousePos, m_hoverCellX, m_hoverCellZ))
    {
    }
    else
    {
        m_hoverCellX = -1;
        m_hoverCellZ = -1;
    }
    if (!allowPreviewInput || (io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger()) return;

    int cellX = -1;
    int cellZ = -1;
    if (!PickTerrainCell(mousePos, cellX, cellZ))
    {
        m_selectedEnvironmentObjectIndex = -1;
        return;
    }

    const int existingIndex = FindEnvironmentObjectIndexByCell(cellX, cellZ);
    if (existingIndex >= 0)
    {
        m_selectedEnvironmentObjectIndex = existingIndex;
        const int modelIndex = FindEnvironmentModelIndexById(m_piece.environmentObjects[existingIndex].modelId);
        if (modelIndex >= 0) m_selectedEnvironmentModelIndex = modelIndex;
        SetMessage(u8"環境オブジェクトを選択しました");
        return;
    }
    if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size()))
    {
        SetMessage(u8"Assetsから配置するモデルを選択してください");
        return;
    }

    std::string placeError;
    if (!CanPlaceEnvironmentObject(cellX, cellZ, placeError))
    {
        SetMessage(placeError);
        return;
    }

    const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
    PushUndoSnapshot();
    NarakuPiece::EnvironmentObjectData object;
    object.modelId = asset.id;
    object.cell = { cellX, cellZ };
    object.scaleX = asset.defaultScale.x;
    object.scaleY = asset.defaultScale.y;
    object.scaleZ = asset.defaultScale.z;
    m_piece.environmentObjects.push_back(object);
    m_selectedEnvironmentObjectIndex = static_cast<int>(m_piece.environmentObjects.size()) - 1;
    MarkPieceDirty();
    SetMessage(u8"環境オブジェクトを配置しました");
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
    for (const NarakuPiece::EnvironmentObjectData& object : m_piece.environmentObjects)
    {
        if (FindEnvironmentModelIndexById(object.modelId) < 0)
        {
            NarakuPiece::ValidationIssue issue;
            issue.severity = NarakuPiece::ValidationIssue::Severity::Error;
            issue.message = "environmentObject が未登録モデルを参照しています: " + object.modelId;
            m_validationIssues.push_back(issue);
        }
    }
    m_validationDirty = false;
}

void SceneNarakuPieceEditor::InvalidateValidationState()
{
    m_validationDirty = true;
}

void SceneNarakuPieceEditor::MarkPieceDirty()
{
    const bool wasDirty = m_isPieceDirty;
    m_isPieceDirty = true;
    InvalidateValidationState();
    if (!wasDirty)
    {
        UpdateMainWindowTitle();
    }
}

void SceneNarakuPieceEditor::MarkPieceClean()
{
    const bool wasDirty = m_isPieceDirty;
    m_isPieceDirty = false;
    if (wasDirty)
    {
        UpdateMainWindowTitle();
    }
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
