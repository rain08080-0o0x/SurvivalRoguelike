#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace NarakuMap
{
    /**
     * @brief XZ 平面上の座標を表します。
     */
    struct Vec2
    {
        /** @brief X 方向の座標値です。 */
        float x = 0.0f;

        /** @brief Z 方向の座標値です。 */
        float z = 0.0f;
    };

    /**
     * @brief レイヤーに属する点オブジェクトの共通情報です。
     * @details
     * 開始地点、帰還地点、将来のスポーン地点など、
     * 「どのレイヤーのどこにあるか」を持つ軽量なポイント情報として使います。
     */
    struct LayerPoint
    {
        /** @brief ポイントの XZ 座標です。 */
        Vec2 xz;

        /** @brief ポイントが属するレイヤー ID です。 */
        int layerId = 0;
    };

    /**
     * @brief 1 マス単位の地形セルに付与する属性フラグです。
     * @details
     * 複数の属性をビットフラグとして同時に持てます。
     */
    enum CellAttributeFlags : std::uint32_t
    {
        /** @brief 属性なしです。 */
        CellAttributeNone = 0u,

        /** @brief このセルを歩行不可として扱います。 */
        CellAttributeBlocked = 1u << 0,

        /** @brief 崖境界セルとして扱い、境界表示にも使います。 */
        CellAttributeCliffEdge = 1u << 1,

        /** @brief 落下可能セルとして扱います。 */
        CellAttributeDropAllowed = 1u << 2,

        /** @brief ロープ接続候補セルとして扱います。 */
        CellAttributeRopeAnchor = 1u << 3,

        /** @brief 危険地形セルとして扱います。 */
        CellAttributeHazard = 1u << 4,

        /** @brief 削除済みセルとして扱い、描画と床判定から除外します。 */
        CellAttributeRemoved = 1u << 5,
    };

    /**
     * @brief 1 枚分のグリッド地形レイヤーです。
     * @details
     * layerDepth が層としての深度、heights が頂点ごとの相対高さです。
     * cellAttributeFlags は各セルの属性を保持し、visible / locked はエディタの表示制御に使います。
     */
    struct TerrainLayer
    {
        /** @brief レイヤー固有の ID です。 */
        int id = 0;

        /** @brief レイヤー中心の XZ 座標です。 */
        Vec2 center;

        /** @brief 層としての深度値です。大きいほど下層を表します。 */
        float layerDepth = 0.0f;

        /** @brief X 方向の頂点数です。 */
        int gridWidth = 0;

        /** @brief Z 方向の頂点数です。 */
        int gridHeight = 0;

        /** @brief 1 セルの大きさです。 */
        float cellSize = 1.0f;

        /** @brief レイヤー単位で選ぶ地面テクスチャ ID です。 */
        int groundTextureId = 0;

        /** @brief エディタ上でこのレイヤーを表示するかどうかです。 */
        bool visible = true;

        /** @brief エディタ上でこのレイヤーを編集ロックするかどうかです。 */
        bool locked = false;

        /** @brief 頂点ごとの Y 相対高さです。サイズは gridWidth * gridHeight です。 */
        std::vector<float> heights;

        /** @brief 頂点が有効かどうかです。1 は有効、0 は削除済み頂点を表します。 */
        std::vector<std::uint8_t> vertexEnabled;

        /** @brief セルごとの属性フラグです。サイズは (gridWidth - 1) * (gridHeight - 1) です。 */
        std::vector<std::uint32_t> cellAttributeFlags;
    };

    /**
     * @brief 上層と下層をつなぐロープ配置情報です。
     */
    struct RopePoint
    {
        /** @brief ロープの XZ 座標です。 */
        Vec2 xz;

        /** @brief 上側に接続するレイヤー ID です。 */
        int topLayerId = 0;

        /** @brief 下側に接続するレイヤー ID です。 */
        int bottomLayerId = 0;
    };

    /**
     * @brief レイヤー上に置く採掘ポイント情報です。
     * @details
     * visualType や discovered に加えて、採掘物名や有効状態などの編集情報も持ちます。
     */
    struct MiningPoint
    {
        /** @brief 採掘ポイントの XZ 座標です。 */
        Vec2 xz;

        /** @brief 所属するレイヤー ID です。 */
        int layerId = 0;

        /** @brief 見た目種別です。 */
        int visualType = 0;

        /** @brief 初期状態で発見済みとして扱うかどうかです。 */
        bool discovered = false;

        /** @brief この採掘ポイントを有効として扱うかどうかです。 */
        bool enabled = true;

        /** @brief 再出現候補ポイントとして扱うかどうかです。 */
        bool respawnCandidate = false;

        /** @brief 採掘時に得られる旧器名や識別名です。 */
        std::string relicName;
    };

    /**
     * @brief マップ検証時に返すエラー / 警告情報です。
     */
    struct ValidationIssue
    {
        /**
         * @brief 検証メッセージの重大度です。
         */
        enum Severity
        {
            /** @brief 注意扱いの警告です。 */
            Warning = 0,
            /** @brief 実行に影響するエラーです。 */
            Error,
        };

        /** @brief 重大度です。 */
        Severity severity = Warning;

        /** @brief ユーザーへ表示するメッセージです。 */
        std::string message;
    };

    /**
     * @brief 奈落塔プロトタイプ用のマップ全体データです。
     */
    struct MapData
    {
        /** @brief 地形レイヤー一覧です。 */
        std::vector<TerrainLayer> terrainLayers;

        /** @brief ロープ一覧です。 */
        std::vector<RopePoint> ropes;

        /** @brief 採掘ポイント一覧です。 */
        std::vector<MiningPoint> miningPoints;

        /** @brief プレイヤー開始地点です。 */
        LayerPoint playerStartPoint;

        /** @brief 帰還地点です。 */
        LayerPoint returnPoint;

        /** @brief 歩行中にこの下り落差以上を踏み越えたら落下状態へ移る閾値です。 */
        float autoFallStartHeight = 0.90f;
    };

    /** @brief 既定のマップ JSON ファイルパスを返します。 */
    const wchar_t* GetDefaultMapPath();

    /** @brief 現在の編集対象 / 実行対象として使うマップ JSON ファイルパスを返します。 */
    const wchar_t* GetCurrentMapPath();

    /** @brief 現在の編集対象 / 実行対象として使うマップ JSON ファイルパスを更新します。 */
    void SetCurrentMapPath(const wchar_t* path);

    /** @brief エディタ起動時に使う既定マップデータを作成します。 */
    MapData CreateDefaultMap();

    /** @brief レイヤーの高さ配列サイズを gridWidth * gridHeight に揃えます。 */
    void EnsureLayerHeights(TerrainLayer& layer);

    /** @brief レイヤーの頂点有効配列サイズを gridWidth * gridHeight に揃えます。 */
    void EnsureLayerVertexEnabled(TerrainLayer& layer);

    /** @brief レイヤーのセル属性配列サイズを (gridWidth - 1) * (gridHeight - 1) に揃えます。 */
    void EnsureLayerCellAttributes(TerrainLayer& layer);

    /** @brief 指定頂点の高さを返します。範囲外なら 0 を返します。 */
    float GetVertexHeight(const TerrainLayer& layer, int gridX, int gridZ);

    /** @brief 指定頂点の高さを設定します。 */
    void SetVertexHeight(TerrainLayer& layer, int gridX, int gridZ, float height);

    /** @brief 指定頂点が有効なら true を返します。範囲外なら false を返します。 */
    bool IsVertexEnabled(const TerrainLayer& layer, int gridX, int gridZ);

    /** @brief 指定頂点の有効 / 無効状態を設定します。 */
    void SetVertexEnabled(TerrainLayer& layer, int gridX, int gridZ, bool enabled);

    /** @brief 指定セルの属性フラグを返します。範囲外なら CellAttributeNone を返します。 */
    std::uint32_t GetCellAttributeFlags(const TerrainLayer& layer, int cellX, int cellZ);

    /** @brief 指定セルの属性フラグを設定します。 */
    void SetCellAttributeFlags(TerrainLayer& layer, int cellX, int cellZ, std::uint32_t flags);

    /** @brief 指定セルが特定属性を持つかどうかを返します。 */
    bool HasCellAttributeFlag(const TerrainLayer& layer, int cellX, int cellZ, std::uint32_t flag);

    /** @brief 指定したレイヤー ID の配列インデックスを返します。見つからない場合は -1 を返します。 */
    int FindLayerIndexById(const MapData& mapData, int layerId);

    /** @brief マップデータの整合性を検証し、エラー / 警告一覧を返します。 */
    std::vector<ValidationIssue> ValidateMapData(const MapData& mapData);

    /** @brief JSON 形式でマップを保存します。 */
    bool SaveMap(const wchar_t* path, const MapData& mapData, std::string* errorMessage = nullptr);

    /** @brief JSON 形式のマップを読込します。 */
    bool LoadMap(const wchar_t* path, MapData& outMapData, std::string* errorMessage = nullptr);
}
