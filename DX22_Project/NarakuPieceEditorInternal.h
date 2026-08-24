#pragma once

#include "EditorPerformanceProfiler.h"

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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
        ModifyMenuW(menuBar, commandId, MF_BYCOMMAND | MF_STRING | MF_GRAYED, commandId, label.c_str());
    }

    /**
     * @brief 座標変換に使用するウィンドウハンドルを取得します。
     */
    HWND GetPreviewHostWindow()
    {
        EDITOR_PROFILE_FUNCTION();
        HWND window = ::GetActiveWindow();
        // 条件に該当する場合は、`window` の状態を更新します。
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
        const size_t dotPos = path.find_last_of(L'.');
        // 条件に該当する場合は、現在の処理をここで終了します。
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
        EDITOR_PROFILE_FUNCTION();
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (fileName.empty())
        {
            return fileName;
        }

        // 条件に該当する場合は、`fileName` の状態を更新します。
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
        EDITOR_PROFILE_FUNCTION();
        static const wchar_t* kInvalidChars = L"\\/:*?\"<>|";
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (fileName.empty())
        {
            return true;
        }

        // 条件に該当する場合は、現在の処理をここで終了します。
        if (fileName.find_first_of(kInvalidChars) != std::wstring::npos)
        {
            return true;
        }

        // 対象コレクションの各要素を順に処理します。
        for (wchar_t ch : fileName)
        {
            // 条件に該当する場合は、現在の処理をここで終了します。
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
        // 条件に該当する場合は、現在の処理をここで終了します。
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
        EDITOR_PROFILE_FUNCTION();
        return NormalizePathSeparators(kNarakuProjectRootPath);
    }

    /**
     * @brief 親ディレクトリを含めて指定ディレクトリを順に作成します。
     * @param directoryPath 作成対象のディレクトリ絶対パスです。
     * @return 既存または作成成功ならtrueを返します。
     */
    bool EnsureDirectoryExists(const std::wstring& directoryPath)
    {
        EDITOR_PROFILE_FUNCTION();
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (directoryPath.empty())
        {
            return false;
        }

        std::wstring normalizedPath = NormalizePathSeparators(directoryPath);
        std::replace(normalizedPath.begin(), normalizedPath.end(), L'/', L'\\');
        // 条件に該当する場合は、現在の処理をここで終了します。
        if (PathExists(normalizedPath))
        {
            return true;
        }

        size_t startIndex = 0;
        // 条件に該当する場合は、`startIndex` の状態を更新します。
        if (normalizedPath.size() >= 2 && normalizedPath[1] == L':')
        {
            startIndex = 3;
        }
        // 先の条件に該当せず、この条件を満たす場合は、`startIndex` の状態を更新します。
        else if (normalizedPath.size() >= 2 && normalizedPath[0] == L'\\' && normalizedPath[1] == L'\\')
        {
            startIndex = normalizedPath.find(L'\\', 2);
            // 条件に該当する場合は、現在の処理をここで終了します。
            if (startIndex == std::wstring::npos)
            {
                return false;
            }
            startIndex = normalizedPath.find(L'\\', startIndex + 1);
            // 条件に該当する場合は、現在の処理をここで終了します。
            if (startIndex == std::wstring::npos)
            {
                return false;
            }
            ++startIndex;
        }

        // 継続条件を満たす間、対象処理を繰り返します。
        while (startIndex < normalizedPath.size())
        {
            const size_t separatorIndex = normalizedPath.find(L'\\', startIndex);
            const std::wstring partialPath = (separatorIndex == std::wstring::npos)
                ? normalizedPath
                : normalizedPath.substr(0, separatorIndex);
            // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
            if (!partialPath.empty() && !PathExists(partialPath))
            {
                // 条件に該当する場合は、対応する編集処理を実行します。
                if (!::CreateDirectoryW(partialPath.c_str(), nullptr))
                {
                    const DWORD error = ::GetLastError();
                    // 条件に該当する場合は、現在の処理をここで終了します。
                    if (error != ERROR_ALREADY_EXISTS)
                    {
                        return false;
                    }
                }
            }

            // 条件に該当する場合は、現在の繰り返し処理を終了します。
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
        return std::max(minValue, std::min(value, maxValue));
    }

    /**
     * @brief 非同期キーボード状態から修飾キーの押下状態を判定します。
     * @param virtualKey 判定対象の仮想キーコードです。
     * @return 指定キーが押下中の場合はtrueです。
     */
    bool IsAsyncModifierPressed(int virtualKey)
    {
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
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
        EDITOR_PROFILE_FUNCTION();
        return std::max(minValue, std::min(value, maxValue));
    }

    /**
     * @brief ステージ役割の列挙値をUI選択用のインデックスへ変換します。
     * @param role 変換対象のステージ役割です。
     * @return 対応するコンボボックス用インデックスです。
     */
    int ToStageRoleIndex(NarakuPiece::StageRole role)
    {
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
        EDITOR_PROFILE_FUNCTION();
        // 値の種類に対応する処理を選択します。
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
