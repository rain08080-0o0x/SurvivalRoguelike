#pragma once

#include "NarakuPieceData.h"
#include "Scene.h"

class RenderTarget;
class DepthStencil;
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

private:
    /**
     * @brief 高さ編集で選択中の頂点座標を保持します。
     */
    struct VertexSelection
    {
        /** @brief 選択中頂点のX方向グリッド座標です。 */
        int x = 0;

        /** @brief 選択中頂点のZ方向グリッド座標です。 */
        int z = 0;
    };

    /**
     * @brief オブジェクト配置やセル選択で使用するセル座標を保持します。
     */
    struct CellSelection
    {
        /** @brief 対象セルのX方向グリッド座標です。 */
        int x = 0;

        /** @brief 対象セルのZ方向グリッド座標です。 */
        int z = 0;
    };

    /**
     * @brief エディタで有効な主編集モードです。
     */
    enum class EditMode
    {
        /** @brief 地形頂点やセルを対象に地形情報を編集するモードです。 */
        Height,

        /** @brief 採掘ポイントやロープなどのグリッドオブジェクトを編集するモードです。 */
        GridObject,
    };

    /**
     * @brief 地形編集モード中に何を選択するかを表します。
     */
    enum class TerrainSelectionMode
    {
        /** @brief 頂点単位で選択し、高さ編集を行うモードです。 */
        Vertex,

        /** @brief セル単位で選択し、セル情報や配置判定に使うモードです。 */
        Cell,
    };

    /**
     * @brief グリッドオブジェクト配置ウィンドウで選択できる配置ツールです。
     */
    enum class GridObjectTool
    {
        /** @brief 選択セルに採掘ポイントを配置するツールです。 */
        MiningPoint,

        /** @brief ロープの上端セルを指定して配置するツールです。 */
        RopeTop,

        /** @brief ロープの下端セルを指定して配置するツールです。 */
        RopeBottom,

        /** @brief 開始・帰還地点を配置するツールです。 */
        StartReturn,
    };

    /**
     * @brief 現在選択中のグリッドオブジェクト種別を表します。
     */
    enum class GridObjectKind
    {
        /** @brief 何も選択されていない状態です。 */
        None,

        /** @brief 採掘ポイントが選択されている状態です。 */
        MiningPoint,

        /** @brief ロープが選択されている状態です。 */
        Rope,

        /** @brief 開始・帰還地点が選択されている状態です。 */
        StartReturn,
    };

    /**
     * @brief Undo/Redo用にエディタの編集状態を退避するスナップショットです。
     */
    struct EditorSnapshot
    {
        /** @brief 退避対象となる奈落ピース本体のデータです。 */
        NarakuPiece::PieceData piece;

        /** @brief 退避時点の単一選択頂点X座標です。 */
        int selectedX = 0;

        /** @brief 退避時点の単一選択頂点Z座標です。 */
        int selectedZ = 0;

        /** @brief 退避時点の複数選択頂点一覧です。 */
        std::vector<VertexSelection> selectedVertices;

        /** @brief 退避時点の主編集モードです。 */
        EditMode editMode = EditMode::Height;

        /** @brief 退避時点の地形選択単位です。 */
        TerrainSelectionMode terrainSelectionMode = TerrainSelectionMode::Vertex;

        /** @brief 退避時点の単一選択セルX座標です。 */
        int selectedCellX = -1;

        /** @brief 退避時点の単一選択セルZ座標です。 */
        int selectedCellZ = -1;

        /** @brief 退避時点の複数選択セル一覧です。 */
        std::vector<CellSelection> selectedCells;

        /** @brief 退避時点の配置ツール種別です。 */
        GridObjectTool gridObjectTool = GridObjectTool::MiningPoint;

        /** @brief 退避時点で選択されていたグリッドオブジェクト種別です。 */
        GridObjectKind selectedGridObjectKind = GridObjectKind::None;

        /** @brief 退避時点で選択されていた採掘ポイントの配列インデックスです。 */
        int selectedMiningPointIndex = -1;
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
    };

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
     * @brief メインメニューバーを描画します。
     */
    void DrawMainMenuBar();

    /**
     * @brief メニューバー込みの作業領域を基準にウィンドウの初回配置を設定します。
     * @param debugName 補正状態の管理に使うImGuiウィンドウ名です。
     * @param defaultOffset 作業領域左上からの初期オフセットです。
     * @param defaultSize 初回表示時に適用する初期サイズです。
     */
    void ApplySafeWindowPlacement(const char* debugName, const ImVec2& defaultOffset, const ImVec2& defaultSize);

    /**
     * @brief 保存済み位置がメニューバーへ被っている場合に一度だけ下方向へ補正します。
     * @param debugName 補正済み管理に使うImGuiウィンドウ名です。
     */
    void EnsureWindowBelowMainMenuBar(const char* debugName);

    /**
     * @brief ピース保存用モーダルを描画します。
     */
    void DrawSavePiecePopup();

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

    /**
     * @brief グリッドオブジェクトの配置ツールUIを描画します。
     */
    void DrawGridObjectPlacementWindow();

    /**
     * @brief 配置済みグリッドオブジェクトの選択情報UIを描画します。
     */
    void DrawGridObjectSelectionWindow();

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
     * @brief 現在の保存ファイル名をメインウィンドウタイトルへ反映します。
     */
    void UpdateMainWindowTitle() const;

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
     * @brief Windows標準のファイル選択ダイアログからピースJSONを読み込みます。
     */
    void OpenLoadPieceDialog();

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

    /**
     * @brief グリッドオブジェクト編集モード中の配置と選択入力を処理します。
     */
    void UpdateGridObjectEditing();

    /**
     * @brief 現在のカメラ状態からビュー行列と射影行列を再計算します。
     */
    void UpdateCameraMatrices();

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
     * @brief Undo履歴数が上限を超えないよう古い履歴を整理します。
     */
    void TrimUndoHistory();

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

    /** @brief 保存モーダルで下書き保存を選択しているかどうかです。 */
    bool m_saveAsDraft = true;

    /** @brief 最新検証で検出された問題一覧です。 */
    std::vector<NarakuPiece::ValidationIssue> m_validationIssues;

    /** @brief 検証結果の再計算が必要かどうかを示すフラグです。 */
    bool m_validationDirty = true;

    /** @brief 画面へ通知する最新メッセージです。 */
    std::string m_message;

    /** @brief Undo用に保持する編集履歴スタックです。 */
    std::vector<EditorSnapshot> m_undoStack;

    /** @brief Redo用に保持する編集履歴スタックです。 */
    std::vector<EditorSnapshot> m_redoStack;

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

    /** @brief 保存済み小ステージの登録一覧です。 */
    std::vector<PieceHierarchyEntry> m_pieceHierarchyEntries;

    /** @brief メニューバー干渉補正を一度適用したウィンドウ名一覧です。 */
    std::vector<std::string> m_menuBarAdjustedWindowNames;
};



