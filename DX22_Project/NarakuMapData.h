#pragma once

#include <string>
#include <vector>

namespace NarakuMap
{
    /**
     * @brief XZ平面上の座標を表す2要素ベクトルです。
     */
    struct Vec2
    {
        /** @brief X軸方向の座標です。 */
        float x = 0.0f;

        /** @brief Z軸方向の座標です。 */
        float z = 0.0f;
    };

    /**
     * @brief 1枚のグリッド地形レイヤーです。
     *
     * layerDepth が層としての深さ、heights が各頂点の起伏です。
     */
    struct TerrainLayer
    {
        /** @brief レイヤー識別子です。 */
        int id = 0;

        /** @brief レイヤー中心のXZ座標です。 */
        Vec2 center;

        /** @brief 層としての深度です。値が大きいほど下層です。 */
        float layerDepth = 0.0f;

        /** @brief X方向の頂点数です。 */
        int gridWidth = 0;

        /** @brief Z方向の頂点数です。 */
        int gridHeight = 0;

        /** @brief 1セルの大きさです。 */
        float cellSize = 1.0f;

        /** @brief レイヤー単位で選ぶ地面テクスチャIDです。 */
        int groundTextureId = 0;

        /** @brief 頂点ごとのY起伏です。サイズは gridWidth * gridHeight です。 */
        std::vector<float> heights;
    };

    /**
     * @brief 上層と下層をつなぐ2点接続ロープです。
     */
    struct RopePoint
    {
        /** @brief ロープのXZ座標です。 */
        Vec2 xz;

        /** @brief 上端側のレイヤーIDです。 */
        int topLayerId = 0;

        /** @brief 下端側のレイヤーIDです。 */
        int bottomLayerId = 0;
    };

    /**
     * @brief レイヤー上に置く採掘ポイントです。
     */
    struct MiningPoint
    {
        /** @brief 採掘ポイントのXZ座標です。 */
        Vec2 xz;

        /** @brief 所属レイヤーIDです。 */
        int layerId = 0;

        /** @brief 見た目だけを区別する番号です。 */
        int visualType = 0;

        /** @brief 初期状態で記録済みかどうかです。 */
        bool discovered = false;
    };

    /**
     * @brief 奈落塔プロトタイプの固定マップ編集データ一式です。
     */
    struct MapData
    {
        /** @brief 地形レイヤー一覧です。 */
        std::vector<TerrainLayer> terrainLayers;

        /** @brief ロープ一覧です。 */
        std::vector<RopePoint> ropes;

        /** @brief 採掘ポイント一覧です。 */
        std::vector<MiningPoint> miningPoints;
    };

    /** @brief 固定保存先のマップJSONファイルパスを返します。 */
    const wchar_t* GetDefaultMapPath();

    /** @brief エディタ起動時に使う既定のマップデータを生成します。 */
    MapData CreateDefaultMap();

    /** @brief レイヤーの高さ配列サイズを gridWidth * gridHeight に合わせます。 */
    void EnsureLayerHeights(TerrainLayer& layer);

    /** @brief 指定頂点の高さを返します。範囲外は 0 を返します。 */
    float GetVertexHeight(const TerrainLayer& layer, int gridX, int gridZ);

    /** @brief 指定頂点の高さを書き換えます。 */
    void SetVertexHeight(TerrainLayer& layer, int gridX, int gridZ, float height);

    /** @brief JSON形式でマップを保存します。 */
    bool SaveMap(const wchar_t* path, const MapData& mapData, std::string* errorMessage = nullptr);

    /** @brief JSON形式のマップを読み込みます。 */
    bool LoadMap(const wchar_t* path, MapData& outMapData, std::string* errorMessage = nullptr);
}
