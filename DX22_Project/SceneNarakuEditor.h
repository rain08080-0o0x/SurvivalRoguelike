#pragma once

#include "Scene.h"
#include "NarakuMapData.h"

#include <Windows.h>
#include <DirectXMath.h>
#include <string>

class Texture;

/**
 * @brief 奈落塔プロトタイプ用の地形編集シーンです。
 * @details
 * 固定マップ前提の地形データを編集するためのシーンです。
 * 地形レイヤー、ロープ、採掘ポイントを編集し、JSON 形式で保存・読込します。
 * 3D プレビュー表示と ImGui ベースの編集 UI を同時に管理します。
 */
class SceneNarakuEditor : public Scene
{
public:
    /**
     * @brief 地形エディタシーンを生成します。
     * @details
     * エディタ用の一時テクスチャを作成し、現在のマップデータを初期化します。
     */
    SceneNarakuEditor();

    /**
     * @brief 地形エディタシーンを破棄します。
     * @details
     * エディタ専用で生成した一時リソースを解放します。
     */
    ~SceneNarakuEditor() override;

    /**
     * @brief エディタの 1 フレーム分の更新を行います。
     * @details
     * カメラ操作、選択状態、各編集モードの入力処理を更新します。
     */
    void Update() override;

    /**
     * @brief エディタの描画を行います。
     * @details
     * 3D ワールドのプレビューを描画した後、ImGui の編集 UI を描画します。
     */
    void Draw() override;

private:
    /**
     * @brief エディタ全体の編集モードです。
     * @details
     * 何を選択し、どの種類のオブジェクトを編集するかを切り替えます。
     */
    enum class EditMode
    {
        /** @brief 地形レイヤーや頂点・面を編集するモードです。 */
        Terrain = 0,
        /** @brief ロープを編集するモードです。 */
        Rope,
        /** @brief 採掘ポイントを編集するモードです。 */
        MiningPoint
    };

    /**
     * @brief 地形モード中の編集対象です。
     * @details
     * 頂点単位で編集するか、面単位で編集するかを切り替えます。
     */
    enum class TerrainEditTarget
    {
        /** @brief グリッド頂点を 1 点ずつ編集します。 */
        Vertex = 0,
        /** @brief グリッドの 1 マスを面として編集します。 */
        Face
    };

    /**
     * @brief 開始地点・帰還地点のどちらを対象にしているかを表します。
     * @details
     * 3D 上のクリック配置やフォーカス対象の切替に使います。
     */
    enum class LayerPointSelectionTarget
    {
        /** @brief 開始地点・帰還地点のどちらも選んでいない状態です。 */
        None = 0,
        /** @brief プレイヤー開始地点を選んでいる状態です。 */
        PlayerStart,
        /** @brief 帰還地点を選んでいる状態です。 */
        ReturnPoint
    };

    /**
     * @brief 現在選択中の地形頂点を表します。
     * @details
     * どのレイヤーの、どのグリッド頂点を選んでいるかを保持します。
     */
    struct VertexSelection
    {
        /** @brief 選択中頂点が属するレイヤーの配列インデックスです。未選択時は -1 です。 */
        int layerIndex = -1;

        /** @brief レイヤー内グリッドの X 方向頂点インデックスです。 */
        int gridX = -1;

        /** @brief レイヤー内グリッドの Z 方向頂点インデックスです。 */
        int gridZ = -1;
    };

    /**
     * @brief 現在選択中の地形セルを表します。
     * @details
     * 面編集モードで、どのレイヤーのどのマスを選んでいるかを保持します。
     */
    struct CellSelection
    {
        /** @brief 選択中セルが属するレイヤーの配列インデックスです。未選択時は -1 です。 */
        int layerIndex = -1;

        /** @brief レイヤー内グリッドの X 方向セルインデックスです。 */
        int cellX = -1;

        /** @brief レイヤー内グリッドの Z 方向セルインデックスです。 */
        int cellZ = -1;
    };

    /**
     * @brief Undo / Redo 用に保持するエディタ状態のスナップショットです。
     * @details
     * マップ本体だけでなく、編集モードや選択状態も一緒に戻せるようにまとめて保持します。
     */
    struct EditorSnapshot
    {
        /** @brief 保存時点のマップデータ本体です。 */
        NarakuMap::MapData mapData;

        /** @brief 保存時点の大分類編集モードです。 */
        EditMode editMode = EditMode::Terrain;

        /** @brief 保存時点の地形編集対象です。 */
        TerrainEditTarget terrainEditTarget = TerrainEditTarget::Vertex;

        /** @brief 保存時点の選択レイヤー番号です。 */
        int selectedLayer = 0;

        /** @brief 保存時点の選択ロープ番号です。 */
        int selectedRope = -1;

        /** @brief 保存時点の選択採掘ポイント番号です。 */
        int selectedMiningPoint = -1;

        /** @brief 保存時点の開始地点・帰還地点選択状態です。 */
        LayerPointSelectionTarget selectedLayerPoint = LayerPointSelectionTarget::None;

        /** @brief 保存時点の選択頂点です。 */
        VertexSelection selectedVertex;

        /** @brief 保存時点の選択セルです。 */
        CellSelection selectedCell;

        /** @brief 保存時点の複数選択頂点集合です。 */
        std::vector<VertexSelection> multiSelectedVertices;

        /** @brief 保存時点の複数選択セル集合です。 */
        std::vector<CellSelection> multiSelectedCells;

        /** @brief 保存時点の複数選択ロープ集合です。 */
        std::vector<int> multiSelectedRopes;

        /** @brief 保存時点の複数選択採掘ポイント集合です。 */
        std::vector<int> multiSelectedMiningPoints;
    };

private:
    /**
     * @brief マップデータの初期化を行います。
     * @details
     * 保存済みの JSON を読込し、存在しない場合は既定マップを生成します。
     * その後、選択状態と初期フォーカスを整えます。
     */
    void InitializeMap();

    /**
     * @brief エディタカメラを更新します。
     * @param dt 1 フレーム分の経過時間です。
     * @details
     * Orbit、Pan、Zoom、Fly の各操作を受け付け、注視点と距離を更新します。
     */
    void UpdateCamera(float dt);

    /**
     * @brief 選択処理と編集処理を更新します。
     * @details
     * 現在の編集モードに応じて、マウスによるピッキング、Gizmo のドラッグ、
     * 頂点高さ変更、面高さ変更などを処理します。
     */
    void UpdateSelectionAndEditing();

    /**
     * @brief 現在選択中の対象へカメラ注視点を移動します。
     * @details
     * 地形、ロープ、採掘ポイントなど、現在の編集モードに応じた対象へ
     * カメラの注視点を合わせます。
     */
    void FocusSelection();

    /**
     * @brief 現在選択中の保存先へマップデータを保存します。
     * @details
     * 現在のマップパスへ JSON 保存を行い、結果メッセージを UI 表示用文字列へ記録します。
     * @return 保存に成功した場合は true を返します。
     */
    bool SaveCurrentMap();

    /**
     * @brief 保存先を選んでマップデータを別名保存します。
     * @details
     * 保存ダイアログで選ばれたパスを現在のマップパスへ反映し、そのまま保存します。
     * @return 保存に成功した場合は true を返します。
     */
    bool SaveCurrentMapAs();

    /**
     * @brief 現在のマップパスからマップデータを読込します。
     * @details
     * 読込できない場合は既定マップを生成し、現在のマップパスへ保存して初期状態を作ります。
     * @return 読込または初期マップ生成に成功した場合は true を返します。
     */
    bool LoadCurrentMap();

    /**
     * @brief 読み込むマップファイルを選択して開きます。
     * @details
     * ファイルダイアログで選択した JSON を現在のマップパスへ反映し、その内容を読込します。
     * @return 読込に成功した場合は true を返します。
     */
    bool OpenMapByDialog();

    /**
     * @brief 新規マップを既定内容から作成します。
     * @details
     * 既定マップ構成を複製し、未使用の Untitled パスを現在のマップパスとして割り当てます。
     */
    void CreateNewMap();

    /**
     * @brief マップ保存先または読込元のパスをダイアログで取得します。
     * @param saveDialog true の場合は保存ダイアログ、false の場合は読込ダイアログを開きます。
     * @param[out] outPath 選択された絶対パスを受け取ります。
     * @return パス選択が確定した場合は true を返します。
     */
    bool PromptMapPath(bool saveDialog, std::wstring& outPath) const;

    /**
     * @brief 新規マップ用の未使用ファイルパスを生成します。
     * @details
     * 既定マップと同じフォルダ配下に Untitled_XX.json 形式の未使用パスを返します。
     * @return 新規マップ候補となる絶対パスを返します。
     */
    std::wstring MakeUniqueUntitledMapPath() const;

    /**
     * @brief 選択状態を有効範囲へ補正します。
     * @details
     * レイヤー、ロープ、採掘ポイント、頂点、セルの各選択値が
     * 現在のデータ範囲外に出ないよう補正します。
     */
    void EnsureSelectionValid();

    /**
     * @brief 現在のエディタ状態を Undo 用スナップショットへ積みます。
     * @details
     * 新しい編集を始める直前に呼び、Redo 履歴は破棄します。
     */
    void PushUndoSnapshot();

    /**
     * @brief 現在の状態からスナップショットを生成します。
     * @return 現在のマップと選択状態をまとめたスナップショットを返します。
     */
    EditorSnapshot MakeSnapshot() const;

    /**
     * @brief 指定スナップショットを現在のエディタ状態へ復元します。
     * @param snapshot 復元元のスナップショットです。
     */
    void RestoreSnapshot(const EditorSnapshot& snapshot);

    /**
     * @brief Undo を 1 段戻します。
     * @return Undo に成功した場合は true を返します。
     */
    bool Undo();

    /**
     * @brief Redo を 1 段進めます。
     * @return Redo に成功した場合は true を返します。
     */
    bool Redo();

    /**
     * @brief 共通ショートカット入力を処理します。
     * @details
     * Delete、Ctrl+Z、Ctrl+Y、真上ビュー切替などをここで受け付けます。
     */
    void HandleShortcuts();

    /**
     * @brief 現在の編集モードに応じた対象を削除します。
     * @return 削除できた場合は true を返します。
     */
    bool DeleteCurrentSelection();

    /**
     * @brief 面編集モードで選択中のセルを削除済みセルとして無効化します。
     * @details
     * 四角形グリッド構造を保つため、外周セルまたは既存の削除セルに隣接するセルだけ削除を許可します。
     * @return 削除に成功した場合は true を返します。
     */
    bool DeleteSelectedTerrainCells();

    /**
     * @brief 頂点編集モードで孤立頂点だけを削除済みとして無効化します。
     * @details
     * まだ有効なセルが参照している頂点は削除せず、警告メッセージを返すだけにします。
     * @return 削除に成功した場合は true を返します。
     */
    bool DeleteSelectedTerrainVertices();

    /**
     * @brief 削除済みの面や頂点を復活します。
     * @details
     * 面モードでは選択セルの削除フラグを外し、その面が使う4頂点も有効化します。
     * 頂点モードでは現在レイヤー内の無効頂点をまとめて復活します。
     * @return 復活対象があった場合は true を返します。
     */
    bool RestoreSelectedTerrainGeometry();

    /**
     * @brief 編集カメラを真上ビューへ切り替えます。
     * @details
     * 現在の注視点は維持したまま、地形全体を俯瞰しやすい角度へ寄せます。
     */
    void SnapCameraToTopView();

    /**
     * @brief 描画用のビュー行列と投影行列を適用します。
     * @details
     * カメラ状態から行列を計算し、3D 描画とピッキング用に保持・設定します。
     */
    void ApplyCameraMatrices();

    /**
     * @brief 3D ワールド側のプレビューを描画します。
     * @details
     * 地形レイヤー、ロープ、採掘ポイント、ホバー表示、Gizmo などを描画します。
     */
    void DrawWorld();

    /**
     * @brief エディタ UI 全体を描画します。
     * @details
     * マップ設定、レイヤー設定、ロープ、採掘ポイント、ヘルプの各ウィンドウを描画します。
     */
    void DrawEditorUi();

    /**
     * @brief マップ全体設定ウィンドウを描画します。
     * @details
     * 保存・読込・リセット・編集モード切替・前景透過設定などを表示します。
     */
    void DrawMapWindow();

    /**
     * @brief 地形レイヤー編集ウィンドウを描画します。
     * @details
     * レイヤー一覧、レイヤー追加削除、グリッド設定、頂点・面編集 UI を表示します。
     */
    void DrawLayerWindow();

    /**
     * @brief ロープ編集ウィンドウを描画します。
     * @details
     * ロープ一覧、追加削除、接続レイヤー、配置位置などの編集 UI を表示します。
     */
    void DrawRopeWindow();

    /**
     * @brief 採掘ポイント編集ウィンドウを描画します。
     * @details
     * 採掘ポイント一覧、追加削除、所属レイヤー、見た目種別などの編集 UI を表示します。
     */
    void DrawMiningWindow();

    /**
     * @brief 現在のエディタ機能と使い方を要約表示するウィンドウを描画します。
     */
    void DrawFeatureWindow();

    /**
     * @brief 操作説明ウィンドウを描画します。
     * @details
     * カメラ操作、選択方法、ショートカットなどのヘルプを表示します。
     */
    void DrawHelpWindow();

    /**
     * @brief 現在の選択対象をまとめて確認するインスペクタを描画します。
     * @details
     * 主選択、複数選択数、フォーカス操作、プレイテスト導線などを一箇所に集約して表示します。
     */
    void DrawInspectorWindow();

    /**
     * @brief 主要な操作履歴を表示するログウィンドウを描画します。
     */
    void DrawOperationLogWindow();

    /**
     * @brief 矩形選択中のスクリーン矩形をオーバーレイ描画します。
     * @details
     * Terrain モードで Shift + 左ドラッグ中のみ、現在の選択矩形を 2D オーバーレイで表示します。
     */
    void DrawMarqueeOverlay();

    /**
     * @brief 地形編集用の高さ Gizmo を描画します。
     * @details
     * 頂点編集時は頂点用ハンドルを、面編集時は面中央のハンドルを描画します。
     */
    void DrawVertexGizmo();

    /**
     * @brief 指定レイヤーのプレビューを描画します。
     * @param layer 描画対象の地形レイヤーデータです。
     * @param layerIndex 描画対象レイヤーの配列インデックスです。
     * @details
     * 半透明床、グリッド線、選択ハイライトなどを描画します。
     */
    void DrawTerrainLayer(const NarakuMap::TerrainLayer& layer, int layerIndex);

    /**
     * @brief カーソル直下にある頂点のホバー表示を描画します。
     * @details
     * Maya 風の強調表示として、カーソルに近い頂点を別色で表示します。
     */
    void DrawHoveredVertexHighlight();

    /**
     * @brief ロープのプレビューを描画します。
     * @details
     * 接続先レイヤー間を線で結び、端点をわかりやすく表示します。
     */
    void DrawRopes();

    /**
     * @brief 採掘ポイントのプレビューを描画します。
     * @details
     * 所属レイヤー上の位置にマーカーとして描画します。
     */
    void DrawMiningPoints();

    /**
     * @brief 開始地点と帰還地点のプレビューを描画します。
     * @details
     * ゲーム開始位置と帰還位置を別色で表示し、編集中のマップ遷移基準点を可視化します。
     */
    void DrawStartAndReturnPoints();

    /**
     * @brief セル属性に応じた境界オーバーレイを描画します。
     * @param layer 描画対象の地形レイヤーです。
     * @details
     * 崖境界、歩行不可、危険地形などのセル属性を色付きのラインで表示します。
     */
    void DrawBoundaryOverlay(const NarakuMap::TerrainLayer& layer) const;

    /**
     * @brief ワールド空間にデバッグ用の箱を描画します。
     * @param pos 箱の中心座標です。
     * @param scale 箱の各軸方向の大きさです。
     */
    void DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale) const;

    /**
     * @brief ワールド空間に半透明の床板を描画します。
     * @param center 床板の中心座標です。
     * @param size 床板の XZ 平面サイズです。
     * @param color 描画色とアルファ値です。
     */
    void DrawTransparentFloor3D(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color) const;

    /**
     * @brief 指定セル 1 枚分の半透明プレビュー床を描画します。
     * @param layer 対象レイヤーです。
     * @param cellX セルの X インデックスです。
     * @param cellZ セルの Z インデックスです。
     * @param color 描画色とアルファ値です。
     */
    void DrawTerrainCellPreview(const NarakuMap::TerrainLayer& layer, int cellX, int cellZ, const DirectX::XMFLOAT4& color) const;

    /**
     * @brief レイヤー深度と頂点高さからワールド Y 座標を求めます。
     * @param layerDepth レイヤーの深度値です。
     * @param height レイヤー上の相対的な高さ値です。
     * @return ワールド空間上での Y 座標を返します。
     */
    float ToWorldY(float layerDepth, float height) const;

    /**
     * @brief 地形頂点のワールド座標を求めます。
     * @param layer 対象となる地形レイヤーです。
     * @param gridX レイヤー内の X 方向頂点インデックスです。
     * @param gridZ レイヤー内の Z 方向頂点インデックスです。
     * @return 指定頂点のワールド座標を返します。
     */
    DirectX::XMFLOAT3 GetVertexWorldPosition(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const;

    /**
     * @brief レイヤープレビュー床の中心座標を求めます。
     * @param layer 対象となる地形レイヤーです。
     * @return 床板描画に使う中心座標を返します。
     */
    DirectX::XMFLOAT3 GetLayerFloorCenter(const NarakuMap::TerrainLayer& layer) const;

    /**
     * @brief レイヤープレビュー床のサイズを求めます。
     * @param layer 対象となる地形レイヤーです。
     * @return 床板描画に使う XZ サイズを返します。
     */
    DirectX::XMFLOAT2 GetLayerFloorSize(const NarakuMap::TerrainLayer& layer) const;

    /**
     * @brief ワールド座標を画面座標へ投影します。
     * @param worldPos 投影対象のワールド座標です。
     * @param[out] outScreen 投影後の画面座標を受け取ります。
     * @return 画面内で有効な位置へ投影できた場合は true を返します。
     */
    bool ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPos, DirectX::XMFLOAT2& outScreen) const;

    /**
     * @brief 選択中レイヤー上で最も近い頂点を選択候補として取得します。
     * @param mousePos 現在のマウス座標です。
     * @param[out] outGridX 選ばれた頂点の X インデックスを受け取ります。
     * @param[out] outGridZ 選ばれた頂点の Z インデックスを受け取ります。
     * @return 頂点を拾えた場合は true を返します。
     */
    bool PickTerrainVertex(POINT mousePos, int& outGridX, int& outGridZ) const;

    /**
     * @brief 選択中レイヤー上で最も近いセルを選択候補として取得します。
     * @param mousePos 現在のマウス座標です。
     * @param[out] outCellX 選ばれたセルの X インデックスを受け取ります。
     * @param[out] outCellZ 選ばれたセルの Z インデックスを受け取ります。
     * @return セルを拾えた場合は true を返します。
     */
    bool PickTerrainCell(POINT mousePos, int& outCellX, int& outCellZ) const;

    /**
     * @brief マウス位置に最も近いロープを選択候補として取得します。
     * @param mousePos 現在のマウス座標です。
     * @return ロープ配列のインデックスを返します。見つからない場合は -1 です。
     */
    int PickRope(POINT mousePos) const;

    /**
     * @brief マウス位置に最も近い採掘ポイントを選択候補として取得します。
     * @param mousePos 現在のマウス座標です。
     * @return 採掘ポイント配列のインデックスを返します。見つからない場合は -1 です。
     */
    int PickMiningPoint(POINT mousePos) const;

    /**
     * @brief マウスが選択中頂点の Gizmo ハンドル付近にあるか判定します。
     * @param mousePos 現在のマウス座標です。
     * @return ハンドル付近にある場合は true を返します。
     */
    bool IsMouseNearSelectedVertexHandle(POINT mousePos) const;

    /**
     * @brief マウスが選択中セルの Gizmo ハンドル付近にあるか判定します。
     * @param mousePos 現在のマウス座標です。
     * @return ハンドル付近にある場合は true を返します。
     */
    bool IsMouseNearSelectedCellHandle(POINT mousePos) const;

    /**
     * @brief 前景透過判定に使う注目深度を取得します。
     * @return 現在の編集対象に応じた深度値を返します。
     */
    float GetFocusDepth() const;

    /**
     * @brief レイヤー描画時のアルファ値を取得します。
     * @param layer アルファ値を求める対象レイヤーです。
     * @return 描画時に使うアルファ値を返します。
     * @details
     * 現在の注目深度より手前にあるレイヤーは、前景として透過表示します。
     */
    float GetLayerAlpha(const NarakuMap::TerrainLayer& layer) const;

    /**
     * @brief 地面テクスチャ ID に対応する表示ラベルを取得します。
     * @param textureId 地面テクスチャ ID です。
     * @return ImGui に表示するラベル文字列を返します。
     */
    const char* GetGroundTextureLabel(int textureId) const;

    /**
     * @brief 地面テクスチャ ID に対応する仮の色を取得します。
     * @param textureId 地面テクスチャ ID です。
     * @return プレビュー床に使う色とアルファ値を返します。
     */
    DirectX::XMFLOAT4 GetGroundTextureTint(int textureId) const;

    /**
     * @brief UTF-16 文字列を UTF-8 へ変換します。
     * @param text 変換元のワイド文字列です。null も許容します。
     * @return UTF-8 へ変換した文字列を返します。変換できない場合は空文字列です。
     */
    std::string WideToUtf8(const wchar_t* text) const;

    /**
     * @brief レイヤー ID から配列インデックスを検索します。
     * @param layerId 探索対象のレイヤー ID です。
     * @return 一致したレイヤーの配列インデックスを返します。見つからない場合は -1 です。
     */
    int FindLayerIndexById(int layerId) const;

    /**
     * @brief 現在の選択状態からセル座標を解決します。
     * @param layer 対象となる地形レイヤーです。
     * @param[out] outCellX 解決したセルの X インデックスを受け取ります。
     * @param[out] outCellZ 解決したセルの Z インデックスを受け取ります。
     */
    void GetSelectedCellCoords(const NarakuMap::TerrainLayer& layer, int& outCellX, int& outCellZ) const;

    /**
     * @brief 現在選択中セルの平均高さを取得します。
     * @param layer 対象となる地形レイヤーです。
     * @return 選択セルを構成する 4 頂点の平均高さを返します。
     */
    float GetSelectedCellAverageHeight(const NarakuMap::TerrainLayer& layer) const;

    /**
     * @brief 指定セルを構成する 4 頂点へ高さ差分を加算します。
     * @param layer 編集対象の地形レイヤーです。
     * @param cellX 対象セルの X インデックスです。
     * @param cellZ 対象セルの Z インデックスです。
     * @param deltaHeight 加算する高さ差分です。
     */
    void ApplyHeightDeltaToCell(NarakuMap::TerrainLayer& layer, int cellX, int cellZ, float deltaHeight);

    /**
     * @brief 現在の複数選択頂点へ一括で高さ差分を加算します。
     * @param deltaHeight 加算する高さ差分です。
     */
    void ApplyHeightDeltaToSelectedVertices(float deltaHeight);

    /**
     * @brief 現在の複数選択セルへ一括で高さ差分を加算します。
     * @param deltaHeight 加算する高さ差分です。
     */
    void ApplyHeightDeltaToSelectedCells(float deltaHeight);

    /**
     * @brief 複数選択状態をクリアします。
     * @details
     * 頂点選択集合とセル選択集合の両方を空にし、主選択だけを残す前提に戻します。
     */
    void ClearMultiSelection();

    /**
     * @brief 矩形選択の結果を現在の複数選択へ反映します。
     * @details
     * Terrain モード中に Shift + 左ドラッグで確定したスクリーン矩形を使って、
     * 頂点またはセルの複数選択集合を作り直します。
     */
    void ApplyMarqueeSelection();

    /**
     * @brief 指定頂点が現在の複数選択集合に含まれるかを返します。
     * @param layerIndex レイヤー配列インデックスです。
     * @param gridX 頂点の X インデックスです。
     * @param gridZ 頂点の Z インデックスです。
     * @return 含まれている場合は true を返します。
     */
    bool IsVertexMultiSelected(int layerIndex, int gridX, int gridZ) const;

    /**
     * @brief 指定セルが現在の複数選択集合に含まれるかを返します。
     * @param layerIndex レイヤー配列インデックスです。
     * @param cellX セルの X インデックスです。
     * @param cellZ セルの Z インデックスです。
     * @return 含まれている場合は true を返します。
     */
    bool IsCellMultiSelected(int layerIndex, int cellX, int cellZ) const;

    /**
     * @brief 指定ロープが現在の複数選択集合に含まれるかを返します。
     * @param ropeIndex ロープ配列インデックスです。
     * @return 含まれている場合は true を返します。
     */
    bool IsRopeMultiSelected(int ropeIndex) const;

    /**
     * @brief 指定採掘ポイントが現在の複数選択集合に含まれるかを返します。
     * @param pointIndex 採掘ポイント配列インデックスです。
     * @return 含まれている場合は true を返します。
     */
    bool IsMiningPointMultiSelected(int pointIndex) const;

    /**
     * @brief 指定セルが削除済みセルかどうかを返します。
     * @param layer 判定対象のレイヤーです。
     * @param cellX セルの X インデックスです。
     * @param cellZ セルの Z インデックスです。
     * @return 削除済みなら true を返します。
     */
    bool IsCellRemoved(const NarakuMap::TerrainLayer& layer, int cellX, int cellZ) const;

    /**
     * @brief 指定セルを削除してよいかを判定します。
     * @param layer 判定対象のレイヤーです。
     * @param cellX セルの X インデックスです。
     * @param cellZ セルの Z インデックスです。
     * @param removingCells 今回まとめて削除しようとしているセル集合です。
     * @return 削除しても四角形セル構造を保てる場合は true を返します。
     */
    bool CanDeleteCell(const NarakuMap::TerrainLayer& layer, int cellX, int cellZ, const std::vector<CellSelection>& removingCells) const;

    /**
     * @brief 指定頂点を削除してよいかを判定します。
     * @param layer 判定対象のレイヤーです。
     * @param gridX 頂点の X インデックスです。
     * @param gridZ 頂点の Z インデックスです。
     * @return 隣接する有効セルがなく、孤立頂点として消せる場合は true を返します。
     */
    bool CanDeleteVertex(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const;

    /**
     * @brief 複数選択集合へ頂点を追加します。
     * @param layerIndex レイヤー配列インデックスです。
     * @param gridX 頂点の X インデックスです。
     * @param gridZ 頂点の Z インデックスです。
     */
    void AddSelectedVertex(int layerIndex, int gridX, int gridZ);

    /**
     * @brief 複数選択集合へセルを追加します。
     * @param layerIndex レイヤー配列インデックスです。
     * @param cellX セルの X インデックスです。
     * @param cellZ セルの Z インデックスです。
     */
    void AddSelectedCell(int layerIndex, int cellX, int cellZ);

    /**
     * @brief 複数選択集合へロープを追加します。
     * @param ropeIndex ロープ配列インデックスです。
     */
    void AddSelectedRope(int ropeIndex);

    /**
     * @brief 複数選択集合へ採掘ポイントを追加します。
     * @param pointIndex 採掘ポイント配列インデックスです。
     */
    void AddSelectedMiningPoint(int pointIndex);

    /**
     * @brief 指定した XZ 座標へスナップを適用します。
     * @param[in,out] pointXZ スナップ対象の XZ 座標です。
     * @param layerId スナップ基準に使うレイヤー ID です。
     * @param snapToCellCenter true の時はセル中心、false の時はグリッド交点へ揃えます。
     */
    void ApplySnapToPoint(NarakuMap::Vec2& pointXZ, int layerId, bool snapToCellCenter) const;

    /**
     * @brief マウス位置を指定レイヤーの水平面へ投影します。
     * @param mousePos 現在のスクリーン座標です。
     * @param layerDepth 投影先レイヤーの深度です。
     * @param[out] outPointXZ 投影後の XZ 座標を受け取ります。
     * @return 投影に成功した場合は true を返します。
     */
    bool ProjectMouseToLayerPlane(POINT mousePos, float layerDepth, NarakuMap::Vec2& outPointXZ) const;

    /**
     * @brief マウス位置を使って指定ポイントをレイヤー平面上で移動します。
     * @param[in,out] pointXZ 更新対象の XZ 座標です。
     * @param layerId 投影基準に使うレイヤー ID です。
     */
    void DragPointOnLayerPlane(NarakuMap::Vec2& pointXZ, int layerId);

    /**
     * @brief 現在選択中の開始地点または帰還地点を取得します。
     * @return 選択対象がある場合はその LayerPoint へのポインタを返します。未選択時は nullptr を返します。
     */
    NarakuMap::LayerPoint* GetSelectedLayerPoint();

    /**
     * @brief 現在選択中の開始地点または帰還地点を取得します。
     * @return 選択対象がある場合はその LayerPoint へのポインタを返します。未選択時は nullptr を返します。
     */
    const NarakuMap::LayerPoint* GetSelectedLayerPoint() const;

    /**
     * @brief エディタの操作ログへ 1 件追加します。
     * @param message 追加するログ本文です。
     */
    void AppendOperationLog(const std::string& message);

    /**
     * @brief 検証結果キャッシュを無効化します。
     * @details
     * マップ内容が変更された後に呼び、次回参照時に再検証を走らせます。
     */
    void InvalidateValidationCache();

    /**
     * @brief 現在のマップに対する検証結果を取得します。
     * @return キャッシュ済み、または必要時に再計算した検証結果一覧を返します。
     */
    const std::vector<NarakuMap::ValidationIssue>& GetValidationIssues();

    /**
     * @brief 指定レイヤーをセル単位の高精細床プレビューで描くか判定します。
     * @param layer 判定対象のレイヤーです。
     * @param layerIndex レイヤー配列インデックスです。
     * @return 高精細床プレビューで描く場合は true を返します。
     */
    bool ShouldDrawDetailedFloorPreview(const NarakuMap::TerrainLayer& layer, int layerIndex) const;

    /**
     * @brief マウス位置から開始地点または帰還地点を拾います。
     * @param mousePos 現在のスクリーン座標です。
     * @param[out] outTarget ヒットした地点種別を受け取ります。
     * @return いずれかの地点にヒットした場合は true を返します。
     */
    bool PickLayerPoint(POINT mousePos, LayerPointSelectionTarget& outTarget) const;

    /**
     * @brief 新規追加用の既定レイヤーを生成します。
     * @return 初期値入りの地形レイヤーを返します。
     */
    NarakuMap::TerrainLayer CreateNewLayer() const;

    /**
     * @brief 新規追加用の既定ロープを生成します。
     * @return 初期値入りのロープ情報を返します。
     */
    NarakuMap::RopePoint CreateNewRope() const;

    /**
     * @brief 新規追加用の既定採掘ポイントを生成します。
     * @return 初期値入りの採掘ポイント情報を返します。
     */
    NarakuMap::MiningPoint CreateNewMiningPoint() const;

private:
    /**
     * @brief 半透明床プレビュー描画に使う 1x1 白テクスチャです。
     * @details
     * Sprite 描画時に色だけ変えて使うためのダミーテクスチャです。
     */
    Texture* m_debugWhiteTexture = nullptr;

    /**
     * @brief 現在エディタで編集中のマップデータ本体です。
     * @details
     * レイヤー、ロープ、採掘ポイントなど、保存対象となる全データを保持します。
     */
    NarakuMap::MapData m_mapData;

    /** @brief Undo 用に過去状態を積むスタックです。 */
    std::vector<EditorSnapshot> m_undoStack;

    /** @brief Redo 用に戻した状態を積むスタックです。 */
    std::vector<EditorSnapshot> m_redoStack;

    /**
     * @brief 現在の大分類の編集モードです。
     * @details
     * 地形、ロープ、採掘ポイントのどれを編集するかを表します。
     */
    EditMode m_editMode = EditMode::Terrain;

    /**
     * @brief 地形モード中の編集対象です。
     * @details
     * 頂点単位編集か、面単位編集かを切り替えるために使います。
     */
    TerrainEditTarget m_terrainEditTarget = TerrainEditTarget::Vertex;

    /**
     * @brief 現在選択中のレイヤー配列インデックスです。
     * @details
     * 地形編集の基準となる対象レイヤーを指します。
     */
    int m_selectedLayer = 0;

    /**
     * @brief 現在選択中のロープ配列インデックスです。
     * @details
     * ロープ編集モードで対象となるロープを指します。未選択時は -1 です。
     */
    int m_selectedRope = -1;

    /**
     * @brief 現在選択中の採掘ポイント配列インデックスです。
     * @details
     * 採掘ポイント編集モードで対象となるポイントを指します。未選択時は -1 です。
     */
    int m_selectedMiningPoint = -1;

    /**
     * @brief 現在選択中の頂点情報です。
     * @details
     * 頂点編集や頂点 Gizmo の基準点として使います。
     */
    VertexSelection m_selectedVertex;

    /**
     * @brief 現在選択中の面情報です。
     * @details
     * 面編集や面 Gizmo の基準セルとして使います。
     */
    CellSelection m_selectedCell;

    /** @brief 矩形選択などでまとめて選ばれている頂点集合です。 */
    std::vector<VertexSelection> m_multiSelectedVertices;

    /** @brief 矩形選択などでまとめて選ばれているセル集合です。 */
    std::vector<CellSelection> m_multiSelectedCells;

    /** @brief Shift 選択などでまとめて選ばれているロープ集合です。 */
    std::vector<int> m_multiSelectedRopes;

    /** @brief Shift 選択などでまとめて選ばれている採掘ポイント集合です。 */
    std::vector<int> m_multiSelectedMiningPoints;

    /** @brief Terrain モードで矩形選択をドラッグ中かどうかです。 */
    bool m_marqueeSelecting = false;

    /** @brief 矩形選択開始時のスクリーン座標です。 */
    POINT m_marqueeStart = {};

    /** @brief 矩形選択更新中のスクリーン座標です。 */
    POINT m_marqueeEnd = {};

    /**
     * @brief 頂点高さ Gizmo をドラッグ中かどうかを表します。
     * @details
     * true の間はマウス移動量を頂点高さの変更として扱います。
     */
    bool m_draggingVertexHeight = false;

    /**
     * @brief 面高さ Gizmo をドラッグ中かどうかを表します。
     * @details
     * true の間はマウス移動量を面全体の高さ変更として扱います。
     */
    bool m_draggingCellHeight = false;

    /**
     * @brief 直近の保存・読込結果メッセージです。
     * @details
     * UI 上に状態表示として出すために使います。
     */
    std::string m_lastIoMessage;

    /** @brief 直近の主要操作を表示するログ一覧です。 */
    std::vector<std::string> m_operationLogs;

    /** @brief 操作ログを描画時に末尾へ追従スクロールするかどうかです。 */
    bool m_autoScrollOperationLog = true;

    /** @brief マップ検証結果のキャッシュです。毎フレーム再検証を避けるために使います。 */
    std::vector<NarakuMap::ValidationIssue> m_cachedValidationIssues;

    /** @brief 検証結果キャッシュが古いかどうかです。 */
    bool m_validationDirty = true;

    /**
     * @brief 前景レイヤーを透過表示する時のアルファ値です。
     * @details
     * 現在注目している深度より手前にあるレイヤーへ適用します。
     */
    float m_frontLayerAlpha = 0.22f;

    /** @brief 地形床プレビューの表示有無です。 */
    bool m_showFloorPreview = true;

    /** @brief 床プレビューをセル単位で高精細描画するかどうかです。false の時はレイヤー単位の簡易描画を使います。 */
    bool m_useDetailedFloorPreview = false;

    /** @brief 高精細床プレビューを許可する最大セル数です。これを超える場合は自動で簡易描画へ切り替えます。 */
    int m_detailedFloorPreviewCellLimit = 512;

    /** @brief グリッド線の表示有無です。 */
    bool m_showGridLines = true;

    /** @brief 全セルのグリッド線を表示するかどうかです。false の時は外周と穴境界だけ描きます。 */
    bool m_showFullGridLines = true;

    /** @brief ロープマーカーの表示有無です。 */
    bool m_showRopePreview = true;

    /** @brief 採掘ポイントマーカーの表示有無です。 */
    bool m_showMiningPreview = true;

    /** @brief 開始地点と帰還地点の表示有無です。 */
    bool m_showStartReturnPreview = true;

    /** @brief セル属性境界の表示有無です。 */
    bool m_showBoundaryPreview = true;

    /** @brief ホバー頂点強調の表示有無です。 */
    bool m_showHoveredVertexPreview = true;

    /** @brief XZ 座標の編集時にスナップを有効にするかどうかです。 */
    bool m_enablePositionSnap = true;

    /** @brief 選択中ロープを 3D 上でドラッグ中かどうかです。 */
    bool m_draggingRopePosition = false;

    /** @brief 選択中採掘ポイントを 3D 上でドラッグ中かどうかです。 */
    bool m_draggingMiningPointPosition = false;

    /** @brief 選択中の開始地点または帰還地点を 3D 上でドラッグ中かどうかです。 */
    bool m_draggingLayerPointPosition = false;

    /** @brief 次の左クリックで開始地点または帰還地点を 3D 配置する待機状態かどうかです。 */
    bool m_armLayerPointPlacement = false;

    /** @brief 現在選択している開始地点または帰還地点です。 */
    LayerPointSelectionTarget m_selectedLayerPoint = LayerPointSelectionTarget::None;

    /** @brief スナップ先をセル中心にするかどうかです。false の時は頂点へスナップします。 */
    bool m_snapToCellCenter = true;

    /** @brief 選択一覧と 3D プレビューのフォーカス同期を自動で行うかどうかです。 */
    bool m_autoFocusSelection = true;

    /**
     * @brief カメラの注視点座標です。
     * @details
     * Orbit、Pan、Fly 操作の基準となるワールド座標です。
     */
    DirectX::XMFLOAT3 m_cameraTarget = { 0.0f, 0.0f, 0.0f };

    /**
     * @brief カメラのヨー角です。
     * @details
     * 水平方向の回転角をラジアンで保持します。
     */
    float m_cameraYaw = DirectX::XMConvertToRadians(225.0f);

    /**
     * @brief カメラのピッチ角です。
     * @details
     * 上下方向の回転角をラジアンで保持します。
     */
    float m_cameraPitch = DirectX::XMConvertToRadians(38.0f);

    /**
     * @brief カメラと注視点との距離です。
     * @details
     * ホイールズーム時に増減する値です。
     */
    float m_cameraDistance = 38.0f;

    /**
     * @brief ピッキング用に保持するビュー行列です。
     * @details
     * 転置前の行列を保持し、ワールド座標から画面座標への投影に使います。
     */
    DirectX::XMFLOAT4X4 m_viewMatrix = {};

    /**
     * @brief ピッキング用に保持する投影行列です。
     * @details
     * 転置前の行列を保持し、ワールド座標から画面座標への投影に使います。
     */
    DirectX::XMFLOAT4X4 m_projectionMatrix = {};
};

