#pragma once

#include "Scene.h"
#include "NarakuMapData.h"

#include <DirectXMath.h>
#include <string>
#include <vector>

class Texture;

/**
 * @brief 奈落塔の第一層プロトタイプを動かす専用シーンです。
 *
 * 既存プロジェクトの DirectX / ImGui / Input 初期化をそのまま使い、
 * プロトタイプ固有の状態とルールだけをこのクラスに閉じ込めています。
 */
class SceneNarakuProto : public Scene
{
public:
    /** @brief プロトタイプの初期状態を作成します。 */
    SceneNarakuProto();

    /** @brief Scene 基底クラス経由で破棄されるため virtual destructor にしています。 */
    ~SceneNarakuProto() override;

    /** @brief 1フレーム分の入力、移動、採掘、敵、上昇負荷を更新します。 */
    void Update() override;

    /** @brief ImGui のデバッグ表示としてフィールド、HUD、各種ウィンドウを描画します。 */
    void Draw() override;

private:
    /**
     * @brief 2Dデバッグ表示と平面移動に使う簡易ベクトルです。
     *
     * 現段階では ImGui 上のトップダウン検証なので XZ 平面を x/y として扱っています。
     */
    struct Vec2
    {
        /** @brief 横方向の座標、またはベクトル成分です。 */
        float x = 0.0f;

        /** @brief 奥方向の座標、またはベクトル成分です。 */
        float y = 0.0f;
    };

    /**
     * @brief プレイヤーの探索中ステータスです。
     *
     * 体力、精神力、スタミナ、深度、行動タイマーなど、1回の潜行で変化する値をまとめています。
     */
    struct PlayerState
    {
        /** @brief フィールド上の現在位置です。 */
        Vec2 pos;

        /** @brief 攻撃やステップの向きに使う現在の向きです。 */
        Vec2 facing;

        /** @brief 現在の深度です。値が大きいほど下に潜っています。 */
        float depth = 0.0f;

        /** @brief 前フレームの深度です。上昇量を計算するために使います。 */
        float previousDepth = 0.0f;

        /** @brief 体力です。0になると死亡リザルトへ移行します。 */
        float hp = 10.0f;

        /** @brief 精神力です。上昇負荷の発症で減少します。 */
        float mental = 100.0f;

        /** @brief スタミナです。走り、ロープ、攻撃、採掘、ステップ、ジャンプで消費します。 */
        float stamina = 100.0f;

        /** @brief 上昇負荷の内部ゲージです。10m分溜まると精神力を削って0に戻ります。 */
        float upperLoad = 0.0f;

        /** @brief 攻撃行動の残り時間です。0より大きい間は攻撃中です。 */
        float attackTimer = 0.0f;

        /** @brief ステップ行動の残り時間です。無敵時間と後硬直の合計を入れています。 */
        float stepTimer = 0.0f;

        /** @brief 敵の体当たりで受けたノックバックの残り時間です。 */
        float knockbackTimer = 0.0f;

        /** @brief ジャンプ検証用の縦速度です。現段階では簡易的な着地判定に使います。 */
        float verticalSpeed = 0.0f;

        /** @brief 空中にいる時間です。簡易ジャンプと落下ダメージの検証に使います。 */
        float airTime = 0.0f;

        /** @brief プレイヤーの足元の絶対ワールド高さです。ジャンプ中の上下位置に使います。 */
        float feetWorldY = 0.0f;

        /** @brief 現在のジャンプ/落下中に到達した最高足元高さです。着地時の落下距離計算に使います。 */
        float peakFeetWorldY = 0.0f;

        /** @brief ノックバック中に1秒あたり進む速度です。 */
        Vec2 knockbackVelocity;

        /** @brief 地面にいるかどうかです。false の間はジャンプ中として扱います。 */
        bool grounded = true;

        /** @brief ロープに掴まっているかどうかです。true の間はW/Sで深度を変えます。 */
        bool onRope = false;

        /** @brief 着地直後の硬直残り時間です。0より大きい間は通常移動や一部行動を止めます。 */
        float landingRecoveryTimer = 0.0f;
    };

    /**
     * @brief 所持、地面置き、売却対象になる旧器アイテムです。
     *
     * プロトタイプでは4級旧器のみを扱うため、効果は持たず重量と売値だけを持ちます。
     */
    struct RelicItem
    {
        /** @brief 旧器名です。現段階では英字で表示して文字コード問題を避けています。 */
        std::string name;

        /** @brief 旧器の重量です。4級旧器は固定で15にしています。 */
        float weight = 15.0f;

        /** @brief 売却価格です。4級旧器は固定で5にしています。 */
        int value = 5;
    };

    /**
     * @brief フィールド上に置かれている旧器です。
     *
     * 拾わずに置いた旧器や、所持品から捨てた旧器をフィールドに残すために使います。
     */
    struct GroundRelic
    {
        /** @brief 地面に置かれている旧器の中身です。 */
        RelicItem item;

        /** @brief 旧器が置かれているフィールド座標です。 */
        Vec2 pos;

        /** @brief 旧器が存在する深度です。下層に落ちた旧器を正しい層で拾う判定に使います。 */
        float depth = 0.0f;

        /** @brief 拾える状態かどうかです。false のものは描画や判定から外します。 */
        bool active = true;
    }; 

    /**
     * @brief 採掘ポイントの状態です。
     *
     * 10箇所固定、見た目は4種類、挙動はすべて同じというプロト仕様を表します。
     */
    struct MiningPoint
    {
        /** @brief 採掘ポイントのフィールド座標です。 */
        Vec2 pos;

        /** @brief 採掘ポイントが存在する深度です。別レイヤーのポイントを誤判定しないために使います。 */
        float depth = 0.0f;

        /** @brief 4種類の見た目を区別する番号です。挙動差はありません。 */
        int visualType = 0;

        /** @brief 地図や探索で発見済みかどうかです。false の間は表示しません。 */
        bool discovered = false;

        /** @brief すでに採掘済みかどうかです。true なら再採掘できません。 */
        bool mined = false;

        /** @brief 採掘完了時に発見される旧器名です。 */
        std::string relicName;
    };

    /**
     * @brief 第一層プロトタイプ用の弱い敵です。
     *
     * 通常接触ではダメージを与えず、予備動作後の体当たり中だけダメージ判定を持ちます。
     */
    struct EnemyState
    {
        /** @brief 敵のフィールド座標です。 */
        Vec2 pos;

        /** @brief 体当たり中に進む方向です。予備動作終了時に決定します。 */
        Vec2 chargeDir;

        /** @brief 敵が存在している深度です。別レイヤーのプレイヤーを追わない判定に使います。 */
        float depth = 0.0f;

        /** @brief 敵の体力です。つるはし3回程度で倒せるため3にしています。 */
        float hp = 3.0f;

        /** @brief 落下中の縦速度です。大きい段差から落ちた時の空中更新に使います。 */
        float verticalSpeed = 0.0f;

        /** @brief 空中にいる時間です。着地までの落下更新に使います。 */
        float airTime = 0.0f;

        /** @brief 敵の足元の絶対ワールド高さです。落下中の上下位置に使います。 */
        float feetWorldY = 0.0f;

        /** @brief 現在の落下中に到達した最高足元高さです。着地時の落下距離計算に使います。 */
        float peakFeetWorldY = 0.0f;

        /** @brief 次に体当たりを開始できるまでの残り時間です。 */
        float attackCooldown = 1.5f;

        /** @brief 体当たり前の予備動作の残り時間です。 */
        float telegraphTimer = 0.0f;

        /** @brief 体当たり移動の残り時間です。 */
        float chargeTimer = 0.0f;

        /** @brief 生存しているかどうかです。false なら更新と描画を止めます。 */
        bool alive = true;

        /** @brief 地面にいるかどうかです。false の間は落下中として扱います。 */
        bool grounded = true;

        /** @brief 1回の体当たりで複数回ヒットしないようにするフラグです。 */
        bool hasHitThisCharge = false;

        /** @brief 着地直後の硬直残り時間です。0より大きい間は通常追跡と体当たり開始を止めます。 */
        float landingRecoveryTimer = 0.0f;
    };

    /**
     * @brief プレイヤーが歩ける床領域です。
     *
     * center と halfSize はXZ平面上の矩形範囲を表し、depth がその床の深度を表します。
     * color はデバッグ3D描画で半透明床として表示するために使います。
     */
    struct FloorRegion
    {
        /** @brief 床矩形の中心座標です。 */
        Vec2 center;

        /** @brief 床矩形の半径サイズです。x が横幅半分、y が奥行き半分です。 */
        Vec2 halfSize;

        /** @brief 床が存在する深度です。0が地上側、値が大きいほど下層です。 */
        float depth = 0.0f;

        /** @brief デバッグ表示用の半透明色です。 */
        DirectX::XMFLOAT4 color = { 0.18f, 0.45f, 0.30f, 0.18f };

        /** @brief 元になったマップレイヤー ID です。 */
        int layerId = 0;
    };

    /**
     * @brief 上層床と下層床をつなぐロープです。
     *
     * pos はXZ平面上の位置で、topDepth と bottomDepth の間だけ昇降できます。
     */
    struct RopePoint
    {
        /** @brief ロープの平面位置です。 */
        Vec2 pos;

        /** @brief ロープ上端の深度です。 */
        float topDepth = 0.0f;

        /** @brief ロープ下端の深度です。 */
        float bottomDepth = 4.0f;
    };

    /**
     * @brief 帰還または死亡時に表示する今回の潜行結果です。
     *
     * 採掘数、最大深度、売却額、ロスト数など、リザルト画面に必要な値を保持します。
     */
    struct RunResult
    {
        /** @brief 帰還理由または死亡理由の表示文です。 */
        std::string reason;

        /** @brief 今回の潜行で到達した最大深度です。 */
        float maxDepth = 0.0f;

        /** @brief 今回の潜行で採掘した旧器数です。 */
        int minedCount = 0;

        /** @brief 帰還時に持ち帰った旧器数です。 */
        int carriedRelics = 0;

        /** @brief 死亡時に失った旧器数です。 */
        int lostRelics = 0;

        /** @brief 帰還時に全売却した場合の合計金額です。 */
        int saleAmount = 0;
    };

    /**
     * @brief プロトタイプシーン内の現在モードです。
     *
     * 探索中、所持品、発見確認、帰還結果、死亡結果を切り替えるために使います。
     */
    enum class Mode
    {
        /** @brief 通常の探索操作を受け付ける状態です。 */
        Explore,

        /** @brief 所持品と地図ピン操作を表示している状態です。 */
        Inventory,

        /** @brief 旧器発見時に拾うか置くかを選ばせる状態です。 */
        RelicPrompt,

        /** @brief 生還後の売却リザルトを表示している状態です。 */
        ReturnResult,

        /** @brief 死亡後のロストリザルトを表示している状態です。 */
        DeathResult
    };

private:
    /** @brief 1回の潜行を初期状態に戻します。所持金だけは維持します。 */
    void ResetRun();

    /** @brief 探索モード中の全更新をまとめて呼び出します。 */
    void UpdateExplore(float dt);

    /** @brief 移動、走り、ステップ、ジャンプ、ロープ昇降を更新します。 */
    void UpdateMovement(float dt);

    /** @brief 攻撃タイマー、攻撃判定、スタミナ自然回復を更新します。 */
    void UpdateAction(float dt);

    /** @brief 採掘中タイマーを進め、完了時に旧器発見確認へ移行します。 */
    void UpdateMining(float dt);

    /** @brief 敵の追跡、予備動作、体当たり、ヒット判定を更新します。 */
    void UpdateEnemies(float dt);

    /** @brief 深度差から上昇負荷ゲージを加算または回復します。 */
    void UpdateUpperLoad(float dt);

    /** @brief Fキーで行う帰還、ロープ、旧器拾い、採掘開始を近い順に処理します。 */
    void TryInteract();

    /** @brief Shift短押しでステップを開始できるか判定して開始します。 */
    void TryStartStep();

    /** @brief Spaceでジャンプを開始できるか判定して開始します。 */
    void TryStartJump();

    /** @brief 左クリックで攻撃を開始できるか判定して開始します。 */
    void TryStartAttack();

    /** @brief 左右Shiftのどちらかが押されているかを物理キーとして判定します。 */
    bool IsShiftPress() const;

    /** @brief 死亡リザルトへ移行し、所持旧器とピンを失わせます。 */
    void StartDeath(const char* reason);

    /** @brief 帰還リザルトへ移行し、持ち帰り旧器と売却額を集計します。 */
    void FinishReturn();

    /** @brief 帰還リザルトの旧器を全売却し、次の潜行を開始します。 */
    void SellAllAndRestart();

    /** @brief 死亡後に再挑戦用の新しい潜行を開始します。 */
    void RestartAfterDeath();

    /** @brief DirectXのデバッグ形状で3Dフィールドを描画します。 */
    void Draw3DField();

    /** @brief ImGui のトップダウンフィールドを描画します。現在は補助用で、通常描画からは呼びません。 */
    void DrawField();

    /** @brief 体力、精神力、スタミナ、重量、深度、ログを描画します。 */
    void DrawHud();

    /** @brief 所持品一覧と捨てる操作を描画します。 */
    void DrawInventory();

    /** @brief 旧器発見時の拾う/置く確認ウィンドウを描画します。 */
    void DrawRelicPrompt();

    /** @brief 帰還または死亡のリザルトウィンドウを描画します。 */
    void DrawResult();

    /** @brief 所持品表示中に使う簡易地図とピン操作を描画します。 */
    void DrawMapControls();

    /** @brief 現在の総重量を計算します。 */
    float GetCurrentWeight() const;

    /** @brief 最大重量100に対する現在重量の割合を返します。 */
    float GetWeightRate() const;

    /** @brief 重量70%以上の速度低下を反映した歩行速度を返します。 */
    float GetMoveSpeed() const;

    /** @brief 重量90%以上のスタミナ消費2倍を反映した消費量を返します。 */
    float GetStaminaCost(float baseCost) const;

    /** @brief 指定した基礎消費量ぶんのスタミナを支払えるか判定します。 */
    bool CanSpendStamina(float baseCost) const;

    /** @brief 指定した基礎消費量を重量補正込みで実際に消費します。 */
    void SpendStamina(float baseCost);

    /** @brief 2点が指定距離以内かどうかを判定します。 */
    bool IsNear(const Vec2& a, const Vec2& b, float range) const;

    /** @brief ベクトルを長さ1に正規化します。ゼロ長ならゼロベクトルを返します。 */
    Vec2 Normalize(const Vec2& value) const;

    /** @brief 2点間距離を返します。 */
    float Distance(const Vec2& a, const Vec2& b) const;

    /** @brief 2つのベクトルの内積を返します。 */
    float Dot(const Vec2& a, const Vec2& b) const;

    /** @brief 2つのベクトルを加算します。 */
    Vec2 Add(const Vec2& a, const Vec2& b) const;

    /** @brief 2つのベクトルを減算します。 */
    Vec2 Sub(const Vec2& a, const Vec2& b) const;

    /** @brief ベクトルにスカラーを掛けます。 */
    Vec2 Mul(const Vec2& a, float scalar) const;

    /** @brief HUDに表示する短いログを追加します。 */
    void AddMessage(const std::string& message);

    /** @brief プレイヤーの近くにある未発見採掘ポイントを発見済みにします。 */
    void DiscoverNearbyMiningPoints();

    /** @brief 所持品の指定旧器を現在位置に捨てます。 */
    void DropInventoryItem(int index);

    /** @brief 指定位置にピンを置くか、近くの既存ピンを削除します。 */
    void TogglePinAt(const Vec2& worldPos);

    /** @brief ImGui上の座標をフィールド座標へ変換します。 */
    Vec2 ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos) const;

    /** @brief フィールド座標を地図用の真上視点ImGui座標へ変換します。 */
    Vec2 WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos) const;

    /** @brief フィールド座標をゲーム画面用の斜め見下ろしImGui座標へ変換します。 */
    Vec2 WorldToObliqueCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float depthOffset = 0.0f) const;

    /** @brief 指定した深度に対応するレイヤー配列番号を返します。見つからなければ -1 を返します。 */
    int FindLayerIndexByDepth(float depth, float tolerance = 0.20f) const;

    /** @brief レイヤー上の任意XZ座標が属するセルとセル内補間率を返します。 */
    bool TryGetLayerCellAt(const NarakuMap::TerrainLayer& layer, const Vec2& pos, int& outCellX, int& outCellZ, float& outFracX, float& outFracZ) const;

    /** @brief レイヤー上の任意XZ座標から地形の相対高さを補間して返します。 */
    float SampleTerrainHeightOffsetAt(const Vec2& pos, float depth) const;

    /** @brief 指定した位置と深度における地面の絶対ワールド高さを返します。 */
    float GetGroundWorldY(const Vec2& pos, float depth) const;

    /** @brief 現在のプレイヤーのジャンプ高さを考慮した追加オフセットを返します。 */
    float GetPlayerAirborneOffset() const;

    /** @brief ロープ番号と深度から、ロープ上の絶対ワールド高さを線形補間して返します。 */
    float GetRopeWorldY(int ropeIndex, float depth) const;

    /** @brief レイヤー頂点を地形高さ込みの3D座標へ変換します。 */
    DirectX::XMFLOAT3 GetTerrainVertexWorld3D(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ, float heightOffset = 0.0f) const;

    /** @brief 2Dフィールド座標と深度を3D座標へ変換します。 */
    DirectX::XMFLOAT3 ToWorld3D(const Vec2& pos, float depth = 0.0f, float heightOffset = 0.0f) const;

    /** @brief 指定位置、サイズ、回転でデバッグ箱を描画します。 */
    void DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale, float yawRad = 0.0f) const;

    /** @brief 指定位置とサイズでデバッグ球を描画します。 */
    void DrawDebugSphere3D(const DirectX::XMFLOAT3& pos, float radius) const;

    /** @brief 指定位置に半透明の水平床を描画します。 */
    void DrawTransparentFloor3D(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color) const;

    /** @brief 指定座標が床矩形内に入っているかを返します。 */
    bool IsInsideFloor(const FloorRegion& floor, const Vec2& pos) const;

    /** @brief 指定座標と深度に対応する床を返します。見つからなければ nullptr を返します。 */
    const FloorRegion* FindFloorAt(const Vec2& pos, float depth) const;

    /** @brief 指定座標と深度に歩ける床があるかを返します。 */
    bool HasFloorAt(const Vec2& pos, float depth) const;

    /** @brief 指定座標と深度に対応するセル属性フラグを返します。範囲外なら CellAttributeNone を返します。 */
    std::uint32_t GetCellAttributeFlagsAt(const Vec2& pos, float depth) const;

    /** @brief 2点間の地形高低差と属性を見て、その移動を通してよいかを返します。 */
    bool CanTraverseGround(const Vec2& from, const Vec2& to, float depth) const;

    /** @brief 床外へ出る移動を止め、可能ならX方向またはY方向だけの移動に分解して通します。 */
    Vec2 ResolveFloorMove(const Vec2& from, const Vec2& to, float depth) const;

    /** @brief プレイヤー付近のロープ番号を返します。近くにない場合は -1 を返します。 */
    int FindNearestRopeIndex(float range) const;

    /** @brief 指定したロープから横へ降りられる床があればロープを離します。 */
    bool TryLeaveRopeSide(int ropeIndex, float leaveSign, const Vec2& cameraRight);

private:
    /** @brief 現在のプレイヤー状態です。 */
    PlayerState m_player;

    /** @brief 現在所持している旧器一覧です。 */
    std::vector<RelicItem> m_inventory;

    /** @brief フィールド上に置かれている旧器一覧です。 */
    std::vector<GroundRelic> m_groundRelics;

    /** @brief 第一層プロトタイプ用の採掘ポイント一覧です。 */
    std::vector<MiningPoint> m_miningPoints;

    /** @brief 第一層プロトタイプ用の敵一覧です。 */
    std::vector<EnemyState> m_enemies;

    /** @brief 実際の移動判定に使う床領域一覧です。 */
    NarakuMap::MapData m_runtimeMap;

    /** @brief プレイヤー開始地点の平面座標です。 */
    Vec2 m_startPoint;

    /** @brief プレイヤー開始地点の深度です。 */
    float m_startDepth = 0.0f;

    /** @brief 帰還地点の平面座標です。 */
    Vec2 m_returnPoint;

    /** @brief 帰還地点の深度です。 */
    float m_returnDepth = 0.0f;

    /** @brief プレイヤーが立てる地形一覧です。 */
    std::vector<FloorRegion> m_floorRegions;

    /** @brief ロープの位置と接続深度の一覧です。 */
    std::vector<RopePoint> m_ropePoints;

    /** @brief 現在つかまっているロープ番号です。未使用時は -1 です。 */
    int m_activeRope = -1;

    /** @brief プレイヤーが置いた地図ピン一覧です。 */
    std::vector<Vec2> m_pins;

    /** @brief HUDに表示する短いログ一覧です。 */
    std::vector<std::string> m_messages;

    /** @brief 現在のシーン内モードです。 */
    Mode m_mode = Mode::Explore;

    /** @brief 現在の潜行結果です。帰還/死亡リザルトで表示します。 */
    RunResult m_result;

    /** @brief 発見確認中の旧器です。 */
    RelicItem m_pendingRelic;

    /** @brief 発見確認中の旧器を置く場合の位置です。 */
    Vec2 m_pendingRelicPos;

    /** @brief 発見確認中の旧器を置く場合の深度です。 */
    float m_pendingRelicDepth = 0.0f;

    /** @brief 危険地形の継続ダメージを刻むまでの残り時間です。 */
    float m_hazardTickTimer = 0.0f;

    /** @brief 歩行中にこの下り落差以上を踏み越えたら落下状態へ移る閾値です。 */
    float m_autoFallStartHeight = 0.90f;

    /** @brief Shift長押し判定用の押下時間です。 */
    float m_shiftHold = 0.0f;

    /** @brief Shift短押しステップを保留しているかどうかです。 */
    bool m_shiftPendingStep = false;

    /** @brief 前フレームにShiftが押されていたかどうかです。 */
    bool m_shiftWasPressed = false;

    /** @brief 現在のShift入力が走りとして確定済みかどうかです。 */
    bool m_shiftRunCommitted = false;

    /** @brief 採掘完了までの残り時間です。 */
    float m_miningTimer = 0.0f;

    /** @brief 採掘中の採掘ポイント番号です。-1なら採掘していません。 */
    int m_miningIndex = -1;

    /** @brief 地上で持っている所持金です。 */
    int m_money = 0;

    /** @brief 所持品UIで選択中の旧器番号です。-1なら未選択です。 */
    int m_selectedInventory = -1;

    /** @brief 半透明床をSpriteで描くための1x1白テクスチャです。 */
    Texture* m_debugWhiteTexture = nullptr;
};
