#pragma once

#include "NarakuPieceData.h"
#include "NarakuPieceEditorHistory.h"
#include "Scene.h"

class RenderTarget;
class DepthStencil;
class Model;
struct ImVec2;

#include <DirectXMath.h>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <array>
#include <string>
#include <vector>

/**
 * @brief 奈落ピースの16x16グリッドを編集するシーンです。
 * @details
 * ピース基本情報、接続設定、検証結果の確認に加えて、
 * 3Dプレビュー上での頂点選択と高さ調整を行います。
 */
class SceneNarakuPieceEditor : public Scene
{
public:
    /**
     * @brief 小ステージエディタ用ネイティブメニューのコマンドID一覧です。
     * @details
     * 地形エディタ側のID帯と衝突しないよう、十分に離した値を起点に定義します。
     */
    enum NativeMenuCommand : unsigned int
    {
        /** @brief 新規ピース作成ダイアログを開きます。 */
        MenuNewPiece = 2000,
        /** @brief ピース保存ダイアログを開きます。 */
        MenuSavePiece,
        /** @brief ピース読み込みダイアログを開きます。 */
        MenuLoadPiece,
        /** @brief 現在のピース名を変更します。 */
        MenuRenamePiece,
        /** @brief 現在のピースを削除します。 */
        MenuDeletePiece,
        /** @brief 現在の編集中ファイル状態を表示する無効メニュー項目です。 */
        MenuFileStatus,
        /** @brief 基本情報ウィンドウの表示状態を切り替えます。 */
        MenuTogglePieceBasicWindow,
        /** @brief 接続設定ウィンドウの表示状態を切り替えます。 */
        MenuTogglePieceConnectionWindow,
        /** @brief 地形編集ウィンドウの表示状態を切り替えます。 */
        MenuToggleTerrainEditWindow,
        /** @brief 配置ツールウィンドウの表示状態を切り替えます。 */
        MenuToggleGridObjectPlacementWindow,
        /** @brief 選択オブジェクトウィンドウの表示状態を切り替えます。 */
        MenuToggleGridObjectSelectionWindow,
        /** @brief 保存・検証ウィンドウの表示状態を切り替えます。 */
        MenuTogglePieceFileAndValidationWindow,
        /** @brief 3Dプレビューウィンドウの表示状態を切り替えます。 */
        MenuTogglePreviewWindow,
        /** @brief 高さグリッドウィンドウの表示状態を切り替えます。 */
        MenuToggleHeightGridWindow,
        /** @brief 小ステージHierarchyウィンドウの表示状態を切り替えます。 */
        MenuTogglePieceHierarchyWindow,
        /** @brief 環境モデルを登録します。 */
        MenuNewEnvironmentModel,
        /** @brief 選択中の環境モデル登録を削除します。 */
        MenuDeleteEnvironmentModel,
        /** @brief 選択中の環境モデル設定を編集します。 */
        MenuEnvironmentModelSetting,
        /** @brief Assetsウィンドウの表示状態を切り替えます。 */
        MenuToggleEnvironmentAssetsWindow
    };

    /**
     * @brief 奈落ピースエディタシーンを初期化します。
     */
    SceneNarakuPieceEditor();

    /**
     * @brief エディタが保持する描画資源や作業状態を破棄します。
     */
    ~SceneNarakuPieceEditor() override;

    /**
     * @brief エディタ全体の入力処理と編集状態の更新を行います。
     */
    void Update() override;

    /**
     * @brief エディタの各ウィンドウと3Dプレビューを描画します。
     */
    void Draw() override;

    /**
     * @brief メインウィンドウのネイティブメニューコマンドを処理します。
     * @param commandId `WM_COMMAND` から渡されたコマンド ID です。
     * @return コマンドを処理した場合は true、対象外なら false を返します。
     */
    bool HandleNativeMenuCommand(unsigned int commandId);

    /**
     * @brief ネイティブメニューのチェック状態を現在の Editor 状態へ同期します。
     * @param menuBar チェック状態を書き換える対象のメニューバーです。
     */
    void SyncNativeMenuState(HMENU menuBar) const;

private:
    using VertexSelection = NarakuPieceEditorHistory::VertexSelection;
    using CellSelection = NarakuPieceEditorHistory::CellSelection;
    using EditMode = NarakuPieceEditorHistory::EditMode;
    using TerrainSelectionMode = NarakuPieceEditorHistory::TerrainSelectionMode;
    using GridObjectTool = NarakuPieceEditorHistory::GridObjectTool;
    using GridObjectKind = NarakuPieceEditorHistory::GridObjectKind;
    using EditorSnapshot = NarakuPieceEditorHistory::Snapshot;

    /** @brief Assetsウィンドウへ登録する環境モデル情報です。 */
    struct EnvironmentModelAsset
    {
        std::string id;
        std::string name;
        std::string path;
        DirectX::XMFLOAT3 defaultScale = { 1.0f, 1.0f, 1.0f };
        Model* model = nullptr;
        DirectX::XMFLOAT3 boundsMin = { -0.5f, 0.0f, -0.5f };
        DirectX::XMFLOAT3 boundsMax = { 0.5f, 1.0f, 0.5f };
        DirectX::XMFLOAT3 previewAnchor = {};
        bool hasBounds = false;
        RenderTarget* thumbnailRenderTarget = nullptr;
        DepthStencil* thumbnailDepthStencil = nullptr;
        unsigned int thumbnailSize = 0;
        bool thumbnailDirty = true;
    };

    /**
     * @brief 小ステージHierarchyへ登録する1件分の情報です。
     */
    struct PieceHierarchyEntry
    {
        /** @brief 一覧表示と保存に使用するファイル名です。 */
        std::wstring fileName;

        /** @brief プロジェクト相対で保持するピースJSONのパスです。 */
        std::wstring relativePath;

        /** @brief 完成品として扱う場合はtrue、下書き扱いならfalseです。 */
        bool isCompleted = false;

        /** @brief 一覧表示と保存に使用する最終更新日時です。 */
        std::wstring lastModified;

        /** @brief 追加順ソートに使用する登録順序です。 */
        size_t insertionOrder = 0;
    };

    /** @brief Hierarchyウィンドウで再利用する表示用情報です。 */
    struct PieceHierarchyDisplayEntry
    {
        size_t sourceIndex = 0;
        std::wstring normalizedRelativePath;
        std::string label;
        std::string tooltip;
    };

    /**
     * @brief Hierarchy一覧の並び順種別です。
     */
    enum class PieceHierarchySortMode
    {
        /** @brief 追加順です。 */
        Insertion,

        /** @brief 更新日時順です。 */
        LastModified,

        /** @brief ファイル名順です。 */
        FileName,
    };

    /** @brief 高さ編集モードの入力更新を処理します。 */
    void UpdateHeightMode(bool deleteTriggered);

    /** @brief グリッドオブジェクト編集モードの入力更新を処理します。 */
    void UpdateGridObjectMode(bool deleteTriggered);

    /** @brief 環境オブジェクト編集モードの入力更新を処理します。 */
    void UpdateEnvironmentObjectMode(bool deleteTriggered);

    /** @brief 選択中セルを削除状態へ変更します。 */
    void DeleteSelectedCells();

    /** @brief 選択中の環境オブジェクトを削除します。 */
    void DeleteSelectedEnvironmentObject();

    /** @brief 保留中のファイル操作ポップアップをImGuiへ通知します。 */
    void OpenRequestedPopups();

    /**
     * @brief 頂点座標から高さ配列の一次元インデックスを取得します。
     * @param x 頂点のX方向グリッド座標です。
     * @param z 頂点のZ方向グリッド座標です。
     * @return 高さ配列へアクセスするための一次元インデックスです。
     */
    int GetHeightIndex(int x, int z) const;

    /**
     * @brief 指定した頂点座標が編集可能な範囲内にあるか判定します。
     * @param x 判定する頂点のX方向グリッド座標です。
     * @param z 判定する頂点のZ方向グリッド座標です。
     * @return 頂点座標が有効ならtrueを返します。
     */
    bool IsValidVertex(int x, int z) const;

    /**
     * @brief 指定頂点の現在の高さ値を取得します。
     * @param x 頂点のX方向グリッド座標です。
     * @param z 頂点のZ方向グリッド座標です。
     * @return 指定頂点に設定されている高さ値です。
     */
    float GetHeight(int x, int z) const;

    /**
     * @brief 指定頂点の高さ値を更新します。
     * @param x 更新対象頂点のX方向グリッド座標です。
     * @param z 更新対象頂点のZ方向グリッド座標です。
     * @param height 設定する高さ値です。
     */
    void SetHeight(int x, int z, float height);

    /**
     * @brief 指定頂点のワールド座標を取得します。
     * @param x 頂点のX方向グリッド座標です。
     * @param z 頂点のZ方向グリッド座標です。
     * @return 3Dプレビュー描画やピッキングに使う頂点のワールド座標です。
     */
    DirectX::XMFLOAT3 GetVertexWorldPosition(int x, int z) const;

    /**
     * @brief エディタ全体を統括するメインウィンドウを描画します。
     */
    void DrawEditorWindow();

    /**
     * @brief 新規ピース作成用モーダルを描画します。
     */
    void DrawNewPiecePopup();

    /**
     * @brief ピース保存用モーダルを描画します。
     */
    void DrawSavePiecePopup();

    /**
     * @brief ピース名変更用モーダルを描画します。
     */
    void DrawRenamePiecePopup();

    /**
     * @brief ピース名やサイズなど基本情報を編集するウィンドウを描画します。
     */
    void DrawPieceBasicWindow();

    /**
     * @brief 隣接ピースとの接続設定を編集するウィンドウを描画します。
     */
    void DrawPieceConnectionWindow();

    /**
     * @brief 地形編集用の操作UIを描画します。
     */
    void DrawTerrainEditWindow();

    /** @brief 頂点選択時の地形編集UIを描画します。 */
    void DrawVertexTerrainControls();

    /** @brief セル選択時の地形編集UIを描画します。 */
    void DrawCellTerrainControls();

    /** @brief 主選択セルを基準に複数選択セルの属性編集UIを描画します。 */
    void DrawSelectedCellControls(const NarakuPiece::CellData& primaryCell);

    /**
     * @brief グリッドオブジェクトの配置ツールUIを描画します。
     */
    void DrawGridObjectPlacementWindow();

    /**
     * @brief 配置済みグリッドオブジェクトの選択情報UIを描画します。
     */
    void DrawGridObjectSelectionWindow();

    /** @brief 登録モデル一覧と環境オブジェクト編集UIを描画します。 */
    void DrawEnvironmentAssetsWindow();

    /** @brief モデル登録・設定用モーダルを描画します。 */
    void DrawEnvironmentModelPopup();

    /**
     * @brief ファイル操作と検証結果を表示するウィンドウを描画します。
     */
    void DrawPieceFileAndValidationWindow();

    /**
     * @brief 現在の保存ファイル名を保存モーダル入力欄へ同期します。
     */
    void SyncSaveFileNameInput();

    /**
     * @brief 保存モーダル入力欄の内容を保存用ファイル名へ反映します。
     */
    void CommitSaveFileNameInput();

    /**
     * @brief 新規作成モーダル入力欄の内容を作業中ファイル名へ整形して反映します。
     */
    void CommitNewPieceFileNameInput();

    /**
     * @brief 現在の保存ファイル名をメインウィンドウタイトルへ反映します。
     */
    void UpdateMainWindowTitle() const;

    /**
     * @brief 現在の編集状態をネイティブメニューへ反映する表示文字列を取得します。
     * @return 編集中ファイル名、dirty、下書き/完成状態を含む表示文字列です。
     */
    std::wstring BuildEditingStatusLabel() const;

    /**
     * @brief 現在表示用に使うファイル名を取得します。
     * @return 未設定時は `(unnamed)` を返します。
     */
    std::wstring GetDisplayFileName() const;

    /**
     * @brief 現在の保存種別に応じた保存先パスを取得します。
     * @return 下書き保存または完成保存に使用するフルパスです。
     */
    std::wstring GetCurrentSaveTargetPath() const;

    /**
     * @brief 現在の編集内容を下書きまたは完成データとして保存します。
     * @param saveAsDraft trueなら下書き保存、falseなら完成保存を行います。
     * @return 保存に成功した場合はtrueを返します。
     */
    bool SavePiece(bool saveAsDraft);

    /**
     * @brief 現在の編集内容について保存確認を行います。
     * @param actionName 確認ダイアログに表示する操作名です。
     * @return 操作を続行してよい場合はtrueを返します。
     */
    bool ConfirmDiscardDirtyChanges(const wchar_t* actionName);

    /**
     * @brief 指定パスのピースJSONを読み込み、エディタ状態へ反映します。
     * @param path 読み込むJSONファイルのパスです。
     * @return 読み込みと反映に成功した場合はtrueを返します。
     */
    bool LoadPieceFromPath(const std::wstring& path);

    /**
     * @brief 読み込んだピースデータをエディタ状態へ適用します。
     * @param loadedPiece 読み込み済みのピースデータです。
     */
    void ApplyLoadedPiece(const NarakuPiece::PieceData& loadedPiece);

    /**
     * @brief 新規ピースを完全初期化して編集状態へ反映します。
     * @param fileName 新規作成後に設定する作業中ファイル名です。
     */
    void CreateNewPiece(const std::wstring& fileName);

    /**
     * @brief Windows標準のファイル選択ダイアログからピースJSONを読み込みます。
     */
    void OpenLoadPieceDialog();

    /**
     * @brief 現在のピース名を変更します。
     * @return 変更に成功した場合はtrueを返します。
     */
    bool RenameCurrentPiece();

    /**
     * @brief 現在のピースを削除します。
     * @return 削除または新規状態への移行に成功した場合はtrueを返します。
     */
    bool DeleteCurrentPiece();

    /**
     * @brief 小ステージHierarchyウィンドウを描画します。
     */
    void DrawPieceHierarchyWindow();

    /**
     * @brief Hierarchy登録情報の保存先CFGパスを取得します。
     * @return プロジェクト相対で扱うHierarchy設定ファイルパスです。
     */
    std::wstring GetPieceHierarchyConfigPath() const;

    /**
     * @brief 指定パスをHierarchy保存用の正規化済み相対パスへ変換します。
     * @param path 絶対または相対のピースJSONパスです。
     * @return プロジェクト相対へ正規化したパスです。変換不能時は区切りのみ正規化して返します。
     */
    std::wstring NormalizePieceHierarchyPath(const std::wstring& path) const;

    /**
     * @brief Hierarchy項目の相対パスからファイルシステム用絶対パスを解決します。
     * @param relativePath Hierarchyに保持しているプロジェクト相対パスです。
     * @return 読込や存在確認に使用する絶対パスです。
     */
    std::wstring ResolvePieceHierarchyPath(const std::wstring& relativePath) const;

    /**
     * @brief パス文字列から完成品ディレクトリ配下かどうかを推定します。
     * @param path 完成判定したい相対または絶対パスです。
     * @return Completed 配下と判断できる場合はtrueを返します。
     */
    bool IsCompletedPiecePath(const std::wstring& path) const;

    /**
     * @brief 現在編集中ピースを指す正規化済み相対パスを取得します。
     * @return Hierarchy選択判定に使用する現在ピースの相対パスです。
     */
    std::wstring GetCurrentEditingPieceRelativePath() const;

    /**
     * @brief Hierarchy登録一覧を設定ファイルから読み込み直します。
     * @return 読込処理が完了した場合はtrueを返します。設定ファイル未作成時もtrueです。
     */
    bool ReloadPieceHierarchyEntries();

    /**
     * @brief Hierarchy登録一覧を設定ファイルへ保存します。
     * @return 保存に成功した場合はtrueを返します。
     */
    bool SavePieceHierarchyEntries() const;

    /**
     * @brief 指定したピース情報をHierarchyへ追加または更新します。
     * @param path 登録対象ピースの絶対または相対パスです。
     * @param isCompleted 完成品として扱う場合はtrueです。
     * @return 登録内容に変更があった場合はtrueを返します。
     */
    bool RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted);

    /**
     * @brief 更新日時付きで指定したピース情報をHierarchyへ追加または更新します。
     * @param path 登録対象ピースの絶対または相対パスです。
     * @param isCompleted 完成品として扱う場合はtrueです。
     * @param lastModified 最終更新日時です。
     * @return 登録内容に変更があった場合はtrueを返します。
     */
    bool RegisterPieceHierarchyEntry(const std::wstring& path, bool isCompleted, const std::wstring& lastModified);

    /**
     * @brief 指定したHierarchy項目を削除します。
     * @param path 削除対象ピースの絶対または相対パスです。
     * @return 削除された場合はtrueを返します。
     */
    bool RemovePieceHierarchyEntry(const std::wstring& path);

    /**
     * @brief 指定したHierarchy項目を検索します。
     * @param path 検索対象ピースの絶対または相対パスです。
     * @return 見つかった項目へのポインタです。未検出ならnullptrです。
     */
    PieceHierarchyEntry* FindPieceHierarchyEntry(const std::wstring& path);

    /**
     * @brief 現在のソート設定に従ってHierarchy表示順を取得します。
     * @return 表示順に並べ替えた項目ポインタ一覧です。
     */
    std::vector<const PieceHierarchyEntry*> BuildSortedPieceHierarchyEntries() const;

    /** @brief 登録内容とソート設定からHierarchy表示キャッシュを再構築します。 */
    void RebuildPieceHierarchyDisplayCache();

    /**
     * @brief Hierarchy用の日時表示ラベルを作成します。
     * @param lastModified 最終更新日時文字列です。
     * @return 一覧表示用の日付文字列です。
     */
    std::wstring BuildHierarchyDateLabel(const std::wstring& lastModified) const;

    /**
     * @brief Hierarchy選択時の読込失敗処理を行います。
     * @param entry 対象のHierarchy項目です。
     */
    void HandleMissingHierarchyEntry(const PieceHierarchyEntry& entry);

    /**
     * @brief 現在の保存状態から編集中ピースをHierarchyへ登録します。
     * @return 登録に成功した場合はtrueを返します。
     */
    bool RegisterCurrentPieceToHierarchy();

    /**
     * @brief 高さ一覧を表形式で確認・編集するグリッドウィンドウを描画します。
     */
    void DrawHeightGridWindow();

    /**
     * @brief 指定サイズに対応した3Dプレビュー用レンダーターゲットを確保します。
     * @param width 必要な描画先の幅です。
     * @param height 必要な描画先の高さです。
     * @return 描画先の確保または再利用に成功した場合はtrueを返します。
     */
    bool EnsurePreviewRenderTarget(unsigned int width, unsigned int height);

    /**
     * @brief 3Dプレビュー用レンダーターゲットと深度バッファを解放します。
     */
    void ReleasePreviewRenderTarget();

    /**
     * @brief 3Dプレビュー画像を表示するウィンドウを描画します。
     */
    void DrawPreviewWindow();

    /** @brief 環境オブジェクト配置モードの入力を処理します。 */
    void UpdateEnvironmentObjectEditing();

    /** @brief 環境モデル登録簿を読み込みます。 */
    void LoadEnvironmentModelCatalog();

    /** @brief 環境モデル登録簿を保存します。 */
    bool SaveEnvironmentModelCatalog();

    /** @brief 登録済み環境モデルを解放します。 */
    void ReleaseEnvironmentModels();

    /** @brief ファイル選択後に新規モデル設定モーダルを開きます。 */
    void OpenNewEnvironmentModelDialog();

    /** @brief 選択中モデル設定モーダルを開きます。 */
    void OpenEnvironmentModelSetting();

    /** @brief 選択中モデルを登録簿から削除します。 */
    void DeleteSelectedEnvironmentModel();

    /** @brief モデル設定モーダルの内容を登録簿へ反映します。 */
    void ApplyEnvironmentModelPopup();

    /** @brief 環境モデル設定入力が登録可能か判定します。 */
    bool IsEnvironmentModelPopupInputValid(const std::string& name, const std::string& path) const;

    /** @brief ポップアップ入力から新しい環境モデルを一覧へ追加します。 */
    bool AddEnvironmentModelFromPopup(const std::string& name, const std::string& path);

    /** @brief 選択中の環境モデル設定を更新し、復元用の旧値を返します。 */
    bool UpdateEnvironmentModelFromPopup(
        const std::string& name,
        std::string& outPreviousName,
        DirectX::XMFLOAT3& outPreviousScale,
        bool& outPreviousThumbnailDirty);

    /** @brief カタログ保存失敗時にポップアップ適用前の状態へ戻します。 */
    void RollbackEnvironmentModelPopup(
        int previousSelectedIndex,
        const std::string& previousName,
        const DirectX::XMFLOAT3& previousScale,
        bool previousThumbnailDirty);

    /** @brief ポップアップのプレビュー用モデルを登録用として取得します。 */
    Model* AcquireEnvironmentModelPopupModel(const std::string& path);

    /** @brief 既存登録と衝突しない環境モデルIDを生成します。 */
    std::string GenerateEnvironmentModelId() const;

    /** @brief 読み込み済みモデルの頂点境界とプレビュー基準点を計算します。 */
    void UpdateEnvironmentModelBounds(EnvironmentModelAsset& asset);

    /** @brief 指定サイズのモデルサムネイル描画先を確保します。 */
    bool EnsureEnvironmentModelThumbnail(EnvironmentModelAsset& asset, unsigned int size);

    /** @brief 固定カメラでモデルサムネイルを描画します。 */
    void RenderEnvironmentModelThumbnail(EnvironmentModelAsset& asset, unsigned int size);

    /** @brief Assets表示用のサムネイルテクスチャを取得します。 */
    void* GetEnvironmentModelThumbnailTextureId(int index, unsigned int size);

    /** @brief モデル設定画面専用のプレビュー描画先を確保します。 */
    bool EnsureEnvironmentModelPopupPreview(unsigned int size);

    /** @brief 入力中サイズとセル基準グリッドをモデル設定画面へ描画します。 */
    void RenderEnvironmentModelPopupPreview(unsigned int size);

    /** @brief モデル設定画面のプレビューテクスチャを取得します。 */
    void* GetEnvironmentModelPopupPreviewTextureId(unsigned int size);

    /** @brief 新規登録用の先行読込モデルと設定画面の描画資源を解放します。 */
    void ReleaseEnvironmentModelPopupPreview();

    /** @brief 指定IDの登録モデル位置を返します。 */
    int FindEnvironmentModelIndexById(const std::string& modelId) const;

    /** @brief 指定セルの環境オブジェクト位置を返します。 */
    int FindEnvironmentObjectIndexByCell(int cellX, int cellZ) const;

    /** @brief 環境オブジェクトを指定セルへ配置できるか確認します。 */
    bool CanPlaceEnvironmentObject(int cellX, int cellZ, std::string& outMessage) const;

    /** @brief 指定セルに環境オブジェクトがあるか返します。 */
    bool HasEnvironmentObjectAt(int cellX, int cellZ) const;

    /** @brief 登録済み環境モデルを3Dプレビューへ描画します。 */
    void DrawEnvironmentObjects3D() const;

    /**
     * @brief 3Dプレビュー画像上にカメラ追従の方位コンパスを描画します。
     */
    void DrawPreviewCompass() const;

    /**
     * @brief 現在のピース形状をオフスクリーンのテクスチャへ描画します。
     */
    void RenderTerrainPreviewToTexture();

    /**
     * @brief 3Dプレビューの有効なビューポートサイズを取得します。
     * @return レンダリングと投影計算に使用するプレビュー領域サイズです。
     */
    DirectX::XMFLOAT2 GetPreviewViewportSize() const;

    /**
     * @brief ImGuiのスクリーン座標をメインウィンドウのクライアント座標へ変換します。
     * @param screenPos ImGuiが返すスクリーン座標です。
     * @return ピッキング判定に使用するクライアント座標です。
     */
    DirectX::XMFLOAT2 ConvertImGuiScreenToClient(const ImVec2& screenPos) const;
    /**
     * @brief メインウィンドウのクライアント座標をImGui描画用のスクリーン座標へ変換します。
     * @param clientPos ピッキング処理で使用しているクライアント座標です。
     * @return ImGuiの描画リストへ渡すスクリーン座標です。
     */
    DirectX::XMFLOAT2 ConvertClientToImGuiScreen(const POINT& clientPos) const;


    /**
     * @brief 現在のマウス座標が3Dプレビュー画像内にあるか判定します。
     * @return マウスカーソルがプレビュー画像上にある場合はtrueを返します。
     */
    bool IsMouseInsidePreviewImage() const;

    /**
     * @brief 3Dプレビューに地形と選択中情報を描画します。
     */
    void DrawTerrainPreview3D() const;

    /**
     * @brief マウス入力に応じてプレビューカメラの回転やズームを更新します。
     */
    void UpdateCamera();

    /**
     * @brief 地形編集モード中の選択操作と高さ変更入力を処理します。
     */
    void UpdateHeightEditing();

    /** @brief セル選択モードの入力とドラッグ選択を更新します。 */
    void UpdateCellHeightEditing();

    /** @brief 頂点選択モードの入力、高さドラッグ、範囲選択を更新します。 */
    void UpdateVertexHeightEditing();

    /** @brief セルのドラッグ選択中状態を更新します。 */
    void UpdateCellSelectionDrag(POINT mousePos);

    /** @brief 頂点のドラッグ選択中状態を更新します。 */
    void UpdateVertexSelectionDrag(POINT mousePos);

    /** @brief ドラッグ距離から矩形選択開始を判定します。 */
    void UpdateSelectionDragActivation();

    /** @brief セルのクリック相当ドラッグ終了位置を選択へ反映します。 */
    void SelectCellFromDragEndpoint();

    /** @brief 頂点のクリック相当ドラッグ終了位置を選択へ反映します。 */
    void SelectVertexFromDragEndpoint();

    /** @brief 選択ドラッグ状態を開始します。 */
    void BeginSelectionDrag(POINT mousePos, bool ctrlPressed, bool shiftPressed);

    /** @brief 選択ドラッグ状態を終了します。 */
    void EndSelectionDrag();

    /** @brief 頂点の高さドラッグ操作を更新します。 */
    void UpdateVertexHeightDrag(bool altPressed);

    /** @brief プレビュー上のホバーセルを更新します。 */
    void UpdateHoveredCell(POINT mousePos, bool allowPreviewInput);

    /**
     * @brief グリッドオブジェクト編集モード中の配置と選択入力を処理します。
     */
    void UpdateGridObjectEditing();

    /**
     * @brief 現在のカメラ状態からビュー行列と射影行列を再計算します。
     */
    void UpdateCameraMatrices();

    /**
     * @brief ワールド方位ベクトルをプレビュー上の画面方向へ変換します。
     * @param worldDirection 変換対象のワールド方位ベクトルです。
     * @return プレビュー画面上の単位方向です。
     */
    DirectX::XMFLOAT2 GetCompassScreenDirection(const DirectX::XMFLOAT3& worldDirection) const;

    /**
     * @brief 3Dプレビューカメラを既定の向きと距離に戻します。
     */
    void ResetCamera();

    /**
     * @brief ワールド座標をプレビュー画像上の2D座標へ投影します。
     * @param worldPos 投影する3Dワールド座標です。
     * @param outScreen 投影結果のスクリーン座標を受け取ります。
     * @return 投影に成功し、画面座標が算出できた場合はtrueを返します。
     */
    bool ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPos, DirectX::XMFLOAT2& outScreen) const;

    /**
     * @brief マウス座標から最も近い地形頂点をピッキングします。
     * @param mousePos メインウィンドウ基準のクライアント座標です。
     * @param outX 選択された頂点のX方向グリッド座標を受け取ります。
     * @param outZ 選択された頂点のZ方向グリッド座標を受け取ります。
     * @return 頂点を特定できた場合はtrueを返します。
     */
    bool PickTerrainVertex(POINT mousePos, int& outX, int& outZ) const;

    /**
     * @brief マウス座標から対応する地形セルをピッキングします。
     * @param mousePos メインウィンドウ基準のクライアント座標です。
     * @param outX 選択されたセルのX方向グリッド座標を受け取ります。
     * @param outZ 選択されたセルのZ方向グリッド座標を受け取ります。
     * @return セルを特定できた場合はtrueを返します。
     */
    bool PickTerrainCell(POINT mousePos, int& outX, int& outZ) const;

    /**
     * @brief 3Dプレビュー上にデバッグ用の塗りつぶしボックスを描画します。
     * @param pos ボックス中心のワールド座標です。
     * @param scale ボックスの各軸方向スケールです。
     */
    void DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale) const;

    /**
     * @brief 3Dプレビュー上にデバッグ用のワイヤーボックスを描画します。
     * @param pos ボックス中心のワールド座標です。
     * @param scale ボックスの各軸方向スケールです。
     * @param color 線色として使用するRGBA値です。
     */
    void DrawDebugWireBox3D(
        const DirectX::XMFLOAT3& pos,
        const DirectX::XMFLOAT3& scale,
        const DirectX::XMFLOAT4& color) const;

    /**
     * @brief 指定頂点が現在の複数選択対象に含まれているか判定します。
     * @param x 判定する頂点のX方向グリッド座標です。
     * @param z 判定する頂点のZ方向グリッド座標です。
     * @return 指定頂点が選択済みならtrueを返します。
     */
    bool IsVertexSelected(int x, int z) const;

    /**
     * @brief 指定頂点だけを選択状態にします。
     * @param x 選択する頂点のX方向グリッド座標です。
     * @param z 選択する頂点のZ方向グリッド座標です。
     */
    void SelectSingleVertex(int x, int z);

    /**
     * @brief 指定頂点を現在の複数選択へ追加します。
     * @param x 追加する頂点のX方向グリッド座標です。
     * @param z 追加する頂点のZ方向グリッド座標です。
     */
    void AddSelectedVertex(int x, int z);

    /**
     * @brief 指定頂点の選択状態を反転します。
     * @param x 対象頂点のX方向グリッド座標です。
     * @param z 対象頂点のZ方向グリッド座標です。
     */
    void ToggleSelectedVertex(int x, int z);

    /**
     * @brief 修飾キー状態に応じて頂点選択の確定方法を切り替えます。
     * @param x 入力対象頂点のX方向グリッド座標です。
     * @param z 入力対象頂点のZ方向グリッド座標です。
     * @param ctrlPressed Ctrlキーが押下中かどうかです。
     * @param shiftPressed Shiftキーが押下中かどうかです。
     */
    void SelectVertexFromInput(int x, int z, bool ctrlPressed, bool shiftPressed);

    /**
     * @brief ドラッグ矩形に含まれる頂点一覧を収集します。
     * @param start ドラッグ開始位置のクライアント座標です。
     * @param end ドラッグ終了位置のクライアント座標です。
     * @return 矩形内に投影された頂点の一覧です。
     */
    std::vector<VertexSelection> CollectVerticesInScreenRect(POINT start, POINT end) const;

    /**
     * @brief 矩形選択で得られた頂点一覧を現在の選択状態へ反映します。
     * @param vertices 矩形内に含まれた頂点一覧です。
     * @param ctrlPressed Ctrlキーが押下中かどうかです。
     * @param shiftPressed Shiftキーが押下中かどうかです。
     */
    void ApplyRectangleSelection(const std::vector<VertexSelection>& vertices, bool ctrlPressed, bool shiftPressed);

    /**
     * @brief 頂点またはセルのドラッグ選択矩形をプレビュー上に描画します。
     */
    void DrawSelectionRectangle() const;

    /**
     * @brief 単一アクティブ頂点以外の頂点選択を解除します。
     */
    void KeepOnlyActiveVertexSelected();

    /**
     * @brief 高さ編集時に選択頂点が空にならないよう整えます。
     */
    void EnsureSelectionNotEmpty();

    /**
     * @brief 選択中頂点すべての高さへ差分値を加算します。
     * @param delta 加算する高さ差分です。
     */
    void ApplyHeightDeltaToSelectedVertices(float delta);
    /**
     * @brief 選択中頂点すべての高さを同じ値に設定します。
     * @param height 設定する高さ値です。
     */
    void SetSelectedVerticesHeight(float height);

    /** @brief 選択頂点の高さを0へ戻します。 */
    void ResetSelectedVertexHeights();

    /** @brief 全頂点の高さを0へ戻します。 */
    void ResetAllVertexHeights();

    /** @brief 選択セルのbool属性を一括更新します。 */
    void ApplySelectedCellFlag(bool NarakuPiece::CellData::* field, bool value);

    /** @brief 選択セルの地面テクスチャIDを一括更新します。 */
    void ApplySelectedCellGroundTextureId(int groundTextureId);

    /**
     * @brief 指定セル中心のワールド座標を取得します。
     * @param cellX セルのX方向グリッド座標です。
     * @param cellZ セルのZ方向グリッド座標です。
     * @return セル中心の3Dワールド座標です。
     */
    DirectX::XMFLOAT3 GetCellWorldPosition(int cellX, int cellZ) const;

    /**
     * @brief 指定セル座標がピース内で有効か判定します。
     * @param cellX 判定するセルのX方向グリッド座標です。
     * @param cellZ 判定するセルのZ方向グリッド座標です。
     * @return セル座標が有効ならtrueを返します。
     */
    bool IsValidCell(int cellX, int cellZ) const;

    /**
     * @brief セル座標からセル配列の一次元インデックスを取得します。
     * @param cellX セルのX方向グリッド座標です。
     * @param cellZ セルのZ方向グリッド座標です。
     * @return セル配列へアクセスするための一次元インデックスです。
     */
    int GetCellIndex(int cellX, int cellZ) const;

    /**
     * @brief 編集用のセルデータを取得します。
     * @param cellX 対象セルのX方向グリッド座標です。
     * @param cellZ 対象セルのZ方向グリッド座標です。
     * @return 対象セルの可変データへのポインタです。無効座標時はnullptrを返します。
     */
    NarakuPiece::CellData* GetCellData(int cellX, int cellZ);

    /**
     * @brief 読み取り専用のセルデータを取得します。
     * @param cellX 対象セルのX方向グリッド座標です。
     * @param cellZ 対象セルのZ方向グリッド座標です。
     * @return 対象セルの読み取り専用データへのポインタです。無効座標時はnullptrを返します。
     */
    const NarakuPiece::CellData* GetCellData(int cellX, int cellZ) const;

    /**
     * @brief 指定セルが現在の複数選択対象に含まれているか判定します。
     * @param cellX 判定するセルのX方向グリッド座標です。
     * @param cellZ 判定するセルのZ方向グリッド座標です。
     * @return 指定セルが選択済みならtrueを返します。
     */
    bool IsCellSelected(int cellX, int cellZ) const;

    /**
     * @brief 指定セルだけを選択状態にします。
     * @param cellX 選択するセルのX方向グリッド座標です。
     * @param cellZ 選択するセルのZ方向グリッド座標です。
     */
    void SelectSingleCell(int cellX, int cellZ);

    /**
     * @brief 指定セルを現在の複数選択へ追加します。
     * @param cellX 追加するセルのX方向グリッド座標です。
     * @param cellZ 追加するセルのZ方向グリッド座標です。
     */
    void AddSelectedCell(int cellX, int cellZ);

    /**
     * @brief 指定セルの選択状態を反転します。
     * @param cellX 対象セルのX方向グリッド座標です。
     * @param cellZ 対象セルのZ方向グリッド座標です。
     */
    void ToggleSelectedCell(int cellX, int cellZ);

    /**
     * @brief 修飾キー状態に応じてセル選択の確定方法を切り替えます。
     * @param cellX 入力対象セルのX方向グリッド座標です。
     * @param cellZ 入力対象セルのZ方向グリッド座標です。
     * @param ctrlPressed Ctrlキーが押下中かどうかです。
     * @param shiftPressed Shiftキーが押下中かどうかです。
     */
    void SelectCellFromInput(int cellX, int cellZ, bool ctrlPressed, bool shiftPressed);

    /**
     * @brief 現在のセル選択状態が有効範囲内に収まるよう補正します。
     */
    void EnsureCellSelectionValid();

    /**
     * @brief 頂点選択とセル選択をまとめて解除します。
     */
    void ClearTerrainSelection();

    /**
     * @brief ドラッグ矩形に含まれるセル一覧を収集します。
     * @param start ドラッグ開始位置のクライアント座標です。
     * @param end ドラッグ終了位置のクライアント座標です。
     * @return 矩形内に投影されたセルの一覧です。
     */
    std::vector<CellSelection> CollectCellsInScreenRect(POINT start, POINT end) const;

    /**
     * @brief 矩形選択で得られたセル一覧を現在の選択状態へ反映します。
     * @param cells 矩形内に含まれたセル一覧です。
     * @param ctrlPressed Ctrlキーが押下中かどうかです。
     * @param shiftPressed Shiftキーが押下中かどうかです。
     */
    void ApplyCellRectangleSelection(const std::vector<CellSelection>& cells, bool ctrlPressed, bool shiftPressed);

    /**
     * @brief 指定セルに対応する採掘ポイントのインデックスを検索します。
     * @param cellX 検索対象セルのX方向グリッド座標です。
     * @param cellZ 検索対象セルのZ方向グリッド座標です。
     * @return 見つかった採掘ポイントのインデックスです。未配置なら-1を返します。
     */
    int FindMiningPointIndexByCell(int cellX, int cellZ) const;

    /**
     * @brief 新規採掘ポイント用の一意なID文字列を生成します。
     * @return 保存データへ設定する採掘ポイントIDです。
     */
    std::string GenerateMiningPointId() const;

    /**
     * @brief 現在選択中のグリッドオブジェクト情報を解除します。
     */
    void ClearGridObjectSelection();

    /**
     * @brief 指定インデックスの採掘ポイントを選択状態にします。
     * @param index 選択する採掘ポイントの配列インデックスです。
     */
    void SelectMiningPoint(int index);

    /**
     * @brief 配置済みロープを選択状態にします。
     */
    void SelectRope();

    /**
     * @brief 配置済み開始・帰還地点を選択状態にします。
     */
    void SelectStartReturn();

    /**
     * @brief 指定セルへグリッドオブジェクトを配置可能か検証します。
     * @param tool 配置しようとしているツール種別です。
     * @param cellX 配置候補セルのX方向グリッド座標です。
     * @param cellZ 配置候補セルのZ方向グリッド座標です。
     * @param outMessage 配置不可時の理由メッセージを受け取ります。
     * @return 配置可能であればtrueを返します。
     */
    bool CanPlaceGridObject(GridObjectTool tool, int cellX, int cellZ, std::string& outMessage) const;

    /**
     * @brief 現在選択中のグリッドオブジェクトを削除します。
     * @return 削除が実行された場合はtrueを返します。
     */
    bool DeleteSelectedGridObject();

    /**
     * @brief 現在の編集状態をUndo/Redo用スナップショットへ変換します。
     * @return 現在状態を複製したエディタスナップショットです。
     */
    EditorSnapshot CreateEditorSnapshot() const;

    /**
     * @brief 現在の編集状態をUndo履歴へ追加します。
     */
    void PushUndoSnapshot();

    /**
     * @brief 保存済みスナップショットから編集状態を復元します。
     * @param snapshot 復元元となるエディタスナップショットです。
     */
    void RestoreEditorSnapshot(const EditorSnapshot& snapshot);

    /**
     * @brief 直前の編集状態へ戻します。
     */
    void UndoEdit();

    /**
     * @brief Undoした編集状態を再適用します。
     */
    void RedoEdit();

    /**
     * @brief キーボードショートカットによるUndo/Redo入力を処理します。
     */
    void HandleUndoRedoShortcuts();


    /**
     * @brief 現在のピースデータを再検証して問題一覧を更新します。
     */
    void RefreshValidationIssues();

    /**
     * @brief 検証再計算が必要な状態に更新します。
     */
    void InvalidateValidationState();

    /**
     * @brief 編集内容を未保存扱いに更新します。
     */
    void MarkPieceDirty();

    /**
     * @brief 編集内容を保存済み扱いに更新します。
     */
    void MarkPieceClean();

    /**
     * @brief エディタ下部などへ表示するメッセージを更新します。
     * @param message ユーザーへ通知するメッセージ文字列です。
     */
    void SetMessage(const std::string& message);

    /**
     * @brief UTF-8文字列をWindows API向けのワイド文字列へ変換します。
     * @param text 変換元のUTF-8文字列です。
     * @return 変換後のワイド文字列です。
     */
    std::wstring Utf8ToWide(const std::string& text) const;

    /**
     * @brief ワイド文字列を保存用のUTF-8文字列へ変換します。
     * @param text 変換元のワイド文字列です。
     * @return 変換後のUTF-8文字列です。
     */
    std::string WideToUtf8(const std::wstring& text) const;

private:
    /** @brief 編集中の奈落ピース本体データです。 */
    NarakuPiece::PieceData m_piece;

    /** @brief 単一選択中頂点のX方向グリッド座標です。 */
    int m_selectedX = 0;

    /** @brief 単一選択中頂点のZ方向グリッド座標です。 */
    int m_selectedZ = 0;

    /** @brief 複数選択中の頂点一覧です。 */
    std::vector<VertexSelection> m_selectedVertices;

    /** @brief 地形編集時に頂点選択かセル選択かを表す状態です。 */
    TerrainSelectionMode m_terrainSelectionMode = TerrainSelectionMode::Vertex;

    /** @brief 単一選択中セルのX方向グリッド座標です。 */
    int m_selectedCellX = -1;

    /** @brief 単一選択中セルのZ方向グリッド座標です。 */
    int m_selectedCellZ = -1;

    /** @brief 複数選択中のセル一覧です。 */
    std::vector<CellSelection> m_selectedCells;

    /** @brief 現在の主編集モードです。 */
    EditMode m_editMode = EditMode::Height;

    /** @brief 配置ウィンドウで現在選択中のグリッドオブジェクトツールです。 */
    GridObjectTool m_gridObjectTool = GridObjectTool::MiningPoint;

    /** @brief 現在選択中グリッドオブジェクトの種別です。 */
    GridObjectKind m_selectedGridObjectKind = GridObjectKind::None;

    /** @brief 現在選択中の採掘ポイントインデックスです。 */
    int m_selectedMiningPointIndex = -1;

    /** @brief Assetsウィンドウへ登録済みの環境モデル一覧です。 */
    std::vector<EnvironmentModelAsset> m_environmentModels;

    /** @brief Assetsウィンドウで選択中の登録モデルです。 */
    int m_selectedEnvironmentModelIndex = -1;

    /** @brief 小ステージ上で選択中の環境オブジェクトです。 */
    int m_selectedEnvironmentObjectIndex = -1;

    /** @brief モデル登録・設定モーダルで編集する名前です。 */
    std::array<char, 128> m_environmentModelNameInput = {};

    /** @brief モデル登録・設定モーダルで保持するモデルパスです。 */
    std::array<char, 512> m_environmentModelPathInput = {};

    /** @brief モデル登録・設定モーダルで編集する既定サイズです。 */
    DirectX::XMFLOAT3 m_environmentModelScaleInput = { 1.0f, 1.0f, 1.0f };

    /** @brief 次回描画でモデル設定モーダルを開く要求です。 */
    bool m_requestOpenEnvironmentModelPopup = false;

    /** @brief モデル設定モーダルが新規登録用かどうかです。 */
    bool m_environmentModelPopupIsNew = false;

    /** @brief 新規登録の確定前にモデルビューへ表示する読込済みモデルです。 */
    Model* m_environmentModelPopupPreviewModel = nullptr;

    /** @brief 新規登録モデルの頂点境界とプレビュー基準点です。 */
    DirectX::XMFLOAT3 m_environmentModelPopupBoundsMin = { -0.5f, 0.0f, -0.5f };
    DirectX::XMFLOAT3 m_environmentModelPopupBoundsMax = { 0.5f, 1.0f, 0.5f };
    DirectX::XMFLOAT3 m_environmentModelPopupPreviewAnchor = {};

    /** @brief モデル設定画面のモデルビュー描画先です。 */
    RenderTarget* m_environmentModelPopupRenderTarget = nullptr;
    DepthStencil* m_environmentModelPopupDepthStencil = nullptr;
    unsigned int m_environmentModelPopupPreviewSize = 0;

    /** @brief Assetsウィンドウのモデルタイル表示寸法です。 */
    float m_environmentAssetTileSize = 96.0f;

    /** @brief プレビュー上でホバー中セルのX方向グリッド座標です。 */
    int m_hoverCellX = -1;

    /** @brief プレビュー上でホバー中セルのZ方向グリッド座標です。 */
    int m_hoverCellZ = -1;

    /** @brief 新規採掘ポイント配置時に設定する見た目種別です。 */
    int m_newMiningVisualType = 0;

    /** @brief 新規採掘ポイント配置時に初期記録済みとして作成するかを表すフラグです。 */
    bool m_newMiningInitiallyRecorded = false;

    /** @brief 保存ダイアログやファイル名入力欄で使用する保存先ファイル名です。 */
    std::wstring m_saveFileName;

    /** @brief 保存モーダルのファイル名入力欄に保持するUTF-8文字列バッファです。 */
    std::array<char, 260> m_saveFileNameInput = {};

    /** @brief 名前変更モーダルの入力欄に保持するUTF-8文字列バッファです。 */
    std::array<char, 260> m_renameFileNameInput = {};

    /** @brief 保存モーダルで下書き保存を選択しているかどうかです。 */
    bool m_saveAsDraft = true;

    /** @brief 新規作成モーダルで確定前に保持する作業用ファイル名です。 */
    std::wstring m_newPieceFileName;

    /** @brief 新規作成モーダルのファイル名入力欄に保持するUTF-8文字列バッファです。 */
    std::array<char, 260> m_newPieceFileNameInput = {};

    /** @brief 次回描画で新規作成モーダルを安全に開くための要求フラグです。 */
    bool m_requestOpenNewPiecePopup = false;

    /** @brief 次回描画で保存モーダルを安全に開くための要求フラグです。 */
    bool m_requestOpenSavePiecePopup = false;

    /** @brief 次回描画で名前変更モーダルを安全に開くための要求フラグです。 */
    bool m_requestOpenRenamePiecePopup = false;

    /** @brief 現在の編集中データが未保存かどうかです。 */
    bool m_isPieceDirty = false;

    /** @brief 最新検証で検出された問題一覧です。 */
    std::vector<NarakuPiece::ValidationIssue> m_validationIssues;

    /** @brief 検証結果の再計算が必要かどうかを示すフラグです。 */
    bool m_validationDirty = true;

    /** @brief 画面へ通知する最新メッセージです。 */
    std::string m_message;

    /** @brief Undo/Redo履歴の保存と上限管理を担当します。 */
    NarakuPieceEditorHistory m_history;

    /** @brief プレビューカメラが注視するワールド座標です。 */
    DirectX::XMFLOAT3 m_cameraTarget = {};

    /** @brief プレビューカメラの水平方向回転角です。 */
    float m_cameraYaw = 0.0f;

    /** @brief プレビューカメラの垂直方向回転角です。 */
    float m_cameraPitch = 0.0f;

    /** @brief プレビューカメラと注視点の距離です。 */
    float m_cameraDistance = 1.0f;

    /** @brief プレビュー描画に使用するビュー行列です。 */
    DirectX::XMFLOAT4X4 m_viewMatrix = {};

    /** @brief プレビュー描画に使用する射影行列です。 */
    DirectX::XMFLOAT4X4 m_projectionMatrix = {};

    /** @brief ピッキングの座標変換で再利用するビュー射影行列です。 */
    DirectX::XMFLOAT4X4 m_viewProjectionMatrix = {};

    /** @brief カメラ回転時にY方向操作を反転するかどうかです。 */
    bool m_invertOrbitY = false;

    /** @brief ドラッグによる高さ変更量へ掛ける感度係数です。 */
    float m_heightDragScale = 0.03f;

    /** @brief 高さドラッグ編集中かどうかを表すフラグです。 */
    bool m_draggingHeight = false;

    /** @brief 3Dプレビューの描画先レンダーターゲットです。 */
    RenderTarget* m_previewRenderTarget = nullptr;

    /** @brief 3Dプレビュー用の深度ステンシルです。 */
    DepthStencil* m_previewDepthStencil = nullptr;

    /** @brief 実際に確保済みのプレビュー描画幅です。 */
    unsigned int m_previewRenderWidth = 0;

    /** @brief 実際に確保済みのプレビュー描画高さです。 */
    unsigned int m_previewRenderHeight = 0;

    /** @brief 次回確保を要求するプレビュー描画幅です。 */
    unsigned int m_previewRequestWidth = 960;

    /** @brief 次回確保を要求するプレビュー描画高さです。 */
    unsigned int m_previewRequestHeight = 540;

    /** @brief 3Dプレビュー画像左上のメインウィンドウ基準クライアント座標です。 */
    DirectX::XMFLOAT2 m_previewImageTopLeft = {};

    /** @brief 3Dプレビュー画像左上のImGui/OSスクリーン座標です。 */
    DirectX::XMFLOAT2 m_previewImageScreenTopLeft = {};

    /** @brief 3Dプレビュー画像の表示サイズです。 */
    DirectX::XMFLOAT2 m_previewImageSize = {};

    /** @brief 3Dプレビューウィンドウを表示するかどうかです。 */
    bool m_showPreviewWindow = true;

    /** @brief 現在フレームでプレビュー画像がホバーされているかどうかです。 */
    bool m_previewImageHovered = false;

    /** @brief 高さ入力欄を直接編集中でドラッグ更新を抑止するためのフラグです。 */
    bool m_heightDragFloatEditing = false;

    /** @brief Undoショートカットの前フレーム押下状態です。 */
    bool m_prevUndoShortcutPressed = false;

    /** @brief Redoショートカットの前フレーム押下状態です。 */
    bool m_prevRedoShortcutPressed = false;

    /** @brief Deleteキーの前フレーム押下状態です。 */
    bool m_prevDeletePressed = false;

    /** @brief ドラッグ選択処理の入力監視中かどうかです。 */
    bool m_dragSelecting = false;

    /** @brief 選択矩形が有効に展開中かどうかです。 */
    bool m_selectionDragActive = false;

    /** @brief 選択ドラッグ開始位置のクライアント座標です。 */
    POINT m_selectionDragStart = {};

    /** @brief 選択ドラッグ現在位置のクライアント座標です。 */
    POINT m_selectionDragCurrent = {};

    /** @brief 選択ドラッグ開始時にCtrlキーが押されていたかどうかです。 */
    bool m_selectionDragCtrl = false;

    /** @brief 選択ドラッグ開始時にShiftキーが押されていたかどうかです。 */
    bool m_selectionDragShift = false;

    /** @brief 高さ一覧グリッドウィンドウの表示状態です。 */
    bool m_showHeightGridWindow = true;

    /** @brief ピース基本情報ウィンドウの表示状態です。 */
    bool m_showPieceBasicWindow = true;

    /** @brief ピース接続設定ウィンドウの表示状態です。 */
    bool m_showPieceConnectionWindow = true;

    /** @brief 地形編集ウィンドウの表示状態です。 */
    bool m_showTerrainEditWindow = true;

    /** @brief グリッドオブジェクト配置ウィンドウの表示状態です。 */
    bool m_showGridObjectPlacementWindow = true;

    /** @brief グリッドオブジェクト選択情報ウィンドウの表示状態です。 */
    bool m_showGridObjectSelectionWindow = true;

    /** @brief ファイル操作と検証結果ウィンドウの表示状態です。 */
    bool m_showPieceFileAndValidationWindow = true;

    /** @brief 小ステージHierarchyウィンドウの表示状態です。 */
    bool m_showPieceHierarchyWindow = true;

    /** @brief 環境モデルAssetsウィンドウの表示状態です。 */
    bool m_showEnvironmentAssetsWindow = true;

    /** @brief ネイティブメニューへ最後に反映したウィンドウ表示状態です。 */
    mutable unsigned int m_lastNativeMenuStateMask = 0xffffffffU;

    /** @brief ネイティブメニューへ最後に反映した編集状態ラベルです。 */
    mutable std::wstring m_lastNativeMenuStatusLabel;

    /** @brief 保存済み小ステージの登録一覧です。 */
    std::vector<PieceHierarchyEntry> m_pieceHierarchyEntries;

    /** @brief 毎フレームのソートと文字列変換を避けるHierarchy表示キャッシュです。 */
    std::vector<PieceHierarchyDisplayEntry> m_pieceHierarchyDisplayEntries;

    /** @brief Hierarchy表示キャッシュの再構築が必要かどうかです。 */
    bool m_pieceHierarchyDisplayDirty = true;

    /** @brief Hierarchy一覧の並び順です。 */
    PieceHierarchySortMode m_pieceHierarchySortMode = PieceHierarchySortMode::Insertion;

    /** @brief Hierarchy一覧を降順表示するかどうかです。 */
    bool m_pieceHierarchySortDescending = false;

    /** @brief Hierarchy項目の次回登録順です。 */
    size_t m_nextPieceHierarchyInsertionOrder = 0;

};



