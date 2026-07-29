#pragma once

#include <string>
#include <vector>

namespace NarakuPiece
{
    /** @brief 小ステージのグリッドサイズプリセットです。 */
    enum class SizePreset
    {
        /** @brief 16x16グリッドです。 */
        Size16x16,

        /** @brief 24x24グリッドです。 */
        Size24x24,

        /** @brief 32x32グリッドです。 */
        Size32x32,
    };

    /** @brief 小ステージが生成マップ内で担う役割です。 */
    enum class StageRole
    {
        /** @brief 通常の小ステージです。 */
        Normal,

        /** @brief プレイヤーの開始・帰還地点候補を持つ小ステージです。 */
        StartReturn,

        /** @brief 拠点として使う小ステージです。 */
        Base,

        /** @brief 中継地点として使う小ステージです。 */
        Relay,
    };

    /** @brief 小ステージ本体および接続辺の地形カテゴリです。 */
    enum class StageCategory
    {
        /** @brief 高い平地です。 */
        PlainHigh,

        /** @brief 低い平地です。 */
        PlainLow,

        /** @brief 崖地形です。 */
        Cliff,

        /** @brief 高低差の高い側です。 */
        HeightHigh,

        /** @brief 高低差の低い側です。 */
        HeightLow,

        /** @brief 水場です。 */
        Water,

        /** @brief 接続不可の壁です。 */
        Blocked,
    };

    /** @brief 小ステージが層間移動で担う役割です。 */
    enum class LayerTransitionRole
    {
        /** @brief 層間移動には使用しません。 */
        None,

        /** @brief 上層から到着し、上層へ戻る層入口です。 */
        Entry,

        /** @brief 下層の生成および下降を開始する層出口です。 */
        Exit,
    };

    /** @brief 小ステージ上で使用する方角です。 */
    enum class Direction
    {
        /** @brief 北です。 */
        North,

        /** @brief 南です。 */
        South,

        /** @brief 東です。 */
        East,

        /** @brief 西です。 */
        West,
    };

    /** @brief グリッド上のセル座標です。 */
    struct GridPoint
    {
        /** @brief X方向のセル座標です。 */
        int x = 0;

        /** @brief Z方向のセル座標です。 */
        int z = 0;
    };

    /** @brief 小ステージの四辺に設定された接続カテゴリです。 */
    struct EdgeCategories
    {
        /** @brief 北辺の接続カテゴリです。 */
        StageCategory north = StageCategory::PlainLow;

        /** @brief 南辺の接続カテゴリです。 */
        StageCategory south = StageCategory::PlainLow;

        /** @brief 東辺の接続カテゴリです。 */
        StageCategory east = StageCategory::PlainLow;

        /** @brief 西辺の接続カテゴリです。 */
        StageCategory west = StageCategory::PlainLow;
    };

    /** @brief 小ステージの四辺を編集できるかどうかを保持します。 */
    struct LockedEdges
    {
        /** @brief 北辺をロックする場合はtrueです。 */
        bool north = true;

        /** @brief 南辺をロックする場合はtrueです。 */
        bool south = true;

        /** @brief 東辺をロックする場合はtrueです。 */
        bool east = true;

        /** @brief 西辺をロックする場合はtrueです。 */
        bool west = true;
    };

    /** @brief 層内の高低差移動に使うロープ配置です。 */
    struct RopeData
    {
        /** @brief ロープを使用する場合はtrueです。 */
        bool enabled = false;

        /** @brief ロープ上端のセル座標です。 */
        GridPoint top;

        /** @brief ロープ下端のセル座標です。 */
        GridPoint bottom;
    };

    /** @brief 層間口として使うロープ端点とロード地点です。 */
    struct LayerTransitionData
    {
        /** @brief この小ステージの層間移動上の役割です。 */
        LayerTransitionRole role = LayerTransitionRole::None;

        /** @brief 層間ロープ端点が設定されている場合はtrueです。 */
        bool ropePointEnabled = false;

        /** @brief 層間ロープ端点のセル座標です。 */
        GridPoint ropePoint;

        /** @brief 層出口のロード地点が設定されている場合はtrueです。 */
        bool loadPointEnabled = false;

        /** @brief 下層生成を開始するロード地点のセル座標です。 */
        GridPoint loadPoint;
    };

    /** @brief セル単位の編集属性です。 */
    struct CellData
    {
        /** @brief セルが削除されている場合はtrueです。 */
        bool deleted = false;

        /** @brief プレイヤーが歩行できる場合はtrueです。 */
        bool walkable = true;

        /** @brief ロープ端点を配置できる場合はtrueです。 */
        bool ropeAllowed = true;

        /** @brief 採掘ポイントを配置できる場合はtrueです。 */
        bool miningAllowed = true;

        /** @brief 敵の出現候補にできる場合はtrueです。 */
        bool enemySpawnAllowed = true;

        /** @brief 地面描画に使用するテクスチャIDです。 */
        int groundTextureId = 0;
    };

    /** @brief 小ステージ内の採掘ポイント配置です。 */
    struct MiningPointData
    {
        /** @brief 採掘ポイントを識別するIDです。 */
        std::string id;

        /** @brief 配置先のセル座標です。 */
        GridPoint cell;

        /** @brief 表示へ使用する種類番号です。 */
        int visualType = 0;

        /** @brief 探索開始時から地図へ記録する場合はtrueです。 */
        bool initiallyRecorded = false;
    };

    /** @brief 小ステージ内に配置する環境モデルです。 */
    struct EnvironmentObjectData
    {
        /** @brief Assetsウィンドウへ登録されたモデルIDです。 */
        std::string modelId;

        /** @brief 配置先のセル座標です。 */
        GridPoint cell;

        /** @brief モデルへ適用するX方向の倍率です。 */
        float scaleX = 1.0f;

        /** @brief モデルへ適用するY方向の倍率です。 */
        float scaleY = 1.0f;

        /** @brief モデルへ適用するZ方向の倍率です。 */
        float scaleZ = 1.0f;
    };

    /** @brief 開始・帰還地点として使用できる候補です。 */
    struct StartReturnCandidate
    {
        /** @brief 候補を有効にする場合はtrueです。 */
        bool enabled = false;

        /** @brief 開始・帰還地点のセル座標です。 */
        GridPoint cell;

        /** @brief 開始時にプレイヤーが向く方角です。 */
        Direction facing = Direction::South;
    };

    /** @brief 小ステージデータの検証結果です。 */
    struct ValidationIssue
    {
        /** @brief 検証結果の重要度です。 */
        enum class Severity
        {
            /** @brief 補足情報です。 */
            Info,

            /** @brief 保存は可能ですが確認が必要な警告です。 */
            Warning,

            /** @brief 完成版として保存できないエラーです。 */
            Error,
        };

        /** @brief 検証結果の重要度です。 */
        Severity severity = Severity::Info;

        /** @brief 検証結果の説明文です。 */
        std::string message;
    };

    /** @brief 小ステージJSONで保存する全データです。 */
    struct PieceData
    {
        /** @brief 保存形式のバージョンです。 */
        int version = 1;

        /** @brief 小ステージを識別するIDです。 */
        std::string id;

        /** @brief Editorに表示する名前です。 */
        std::string displayName;

        /** @brief 最後に保存した日時です。 */
        std::string lastModified;

        /** @brief 所属する奈落階層です。 */
        int abyssLayer = 1;

        /** @brief グリッドサイズのプリセットです。 */
        SizePreset sizePreset = SizePreset::Size16x16;

        /** @brief グリッド頂点数のX方向サイズです。 */
        int gridWidth = 16;

        /** @brief グリッド頂点数のZ方向サイズです。 */
        int gridDepth = 16;

        /** @brief 1セルのワールド座標上の大きさです。 */
        float cellSize = 2.0f;

        /** @brief 生成マップ内で担う役割です。 */
        StageRole stageRole = StageRole::Normal;

        /** @brief 小ステージ全体の地形カテゴリです。 */
        StageCategory stageCategory = StageCategory::PlainLow;

        /** @brief 四辺の接続カテゴリです。 */
        EdgeCategories edgeCategories;

        /** @brief 四辺の編集ロック状態です。 */
        LockedEdges lockedEdges;

        /** @brief 各グリッド頂点の高さです。 */
        std::vector<float> heights;

        /** @brief 各セルの編集属性です。 */
        std::vector<CellData> cells;

        /** @brief 層内移動用のロープ配置です。 */
        RopeData rope;

        /** @brief 層入口または層出口として使う配置情報です。 */
        LayerTransitionData layerTransition;

        /** @brief 採掘ポイント一覧です。 */
        std::vector<MiningPointData> miningPoints;

        /** @brief 配置済みの環境オブジェクト一覧です。 */
        std::vector<EnvironmentObjectData> environmentObjects;

        /** @brief 開始・帰還地点候補です。 */
        StartReturnCandidate startReturnCandidate;
    };

    /** @brief サイズプリセットをJSON用文字列へ変換します。 */
    const char* ToString(SizePreset value);

    /** @brief ステージ役割をJSON用文字列へ変換します。 */
    const char* ToString(StageRole value);

    /** @brief ステージカテゴリをJSON用文字列へ変換します。 */
    const char* ToString(StageCategory value);

    /** @brief 層間口役割をJSON用文字列へ変換します。 */
    const char* ToString(LayerTransitionRole value);

    /** @brief 方角をJSON用文字列へ変換します。 */
    const char* ToString(Direction value);

    /** @brief JSON文字列からサイズプリセットを読み取ります。 */
    bool TryParseSizePreset(const std::string& text, SizePreset& out);

    /** @brief JSON文字列からステージ役割を読み取ります。 */
    bool TryParseStageRole(const std::string& text, StageRole& out);

    /** @brief JSON文字列からステージカテゴリを読み取ります。 */
    bool TryParseStageCategory(const std::string& text, StageCategory& out);

    /** @brief JSON文字列から層間口役割を読み取ります。 */
    bool TryParseLayerTransitionRole(const std::string& text, LayerTransitionRole& out);

    /** @brief JSON文字列から方角を読み取ります。 */
    bool TryParseDirection(const std::string& text, Direction& out);

    /** @brief サイズプリセットに対応するグリッドサイズを返します。 */
    int GetGridSize(SizePreset preset);

    /** @brief 指定サイズの初期小ステージデータを作成します。 */
    PieceData CreateDefaultPiece(SizePreset preset = SizePreset::Size16x16);

    /** @brief 小ステージデータを検証し、問題一覧を返します。 */
    std::vector<ValidationIssue> ValidatePieceData(const PieceData& data);

    /** @brief 検証結果にエラーが含まれる場合はtrueを返します。 */
    bool HasValidationError(const std::vector<ValidationIssue>& issues);

    /** @brief 指定階層のDraftsフォルダへの相対パスを返します。 */
    std::wstring GetDraftsDirectoryRelativePath(int abyssLayer);

    /** @brief 指定階層のCompletedフォルダへの相対パスを返します。 */
    std::wstring GetCompletedDirectoryRelativePath(int abyssLayer);

    /** @brief 小ステージのファイル名を正規化し、json拡張子を付与します。 */
    std::wstring NormalizePieceFileName(std::wstring fileName);

    /** @brief Draftsフォルダ内の小ステージパスを作成します。 */
    std::wstring MakeDraftPiecePath(int abyssLayer, const std::wstring& fileName);

    /** @brief Completedフォルダ内の小ステージパスを作成します。 */
    std::wstring MakeCompletedPiecePath(int abyssLayer, const std::wstring& fileName);

    /** @brief 相対パスを実際のファイルシステム上のパスへ解決します。 */
    std::wstring ResolvePiecePathForFileSystem(const std::wstring& relativeOrAbsolutePath);

    /** @brief 小ステージデータをJSONファイルへ保存します。 */
    bool SavePieceData(const PieceData& data, const std::wstring& relativeOrAbsolutePath, std::string* outError = nullptr);

    /** @brief JSONファイルから小ステージデータを読み込みます。 */
    bool LoadPieceData(const std::wstring& relativeOrAbsolutePath, PieceData& outData, std::string* outError = nullptr);
}
