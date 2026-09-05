#pragma once

#include "Scene.h"
#include "NarakuMapData.h"

#include <DirectXMath.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class Texture;
class Model;
class MeshBuffer;
class VertexShader;
class PixelShader;

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

        /** @brief 前フレームの物理高さYです。高さベースの上昇量を計算するために使います。 */
        float previousWorldY = 0.0f;

        /** @brief 体力です。0になると死亡リザルトへ移行します。 */
        float hp = 100.0f;

        /** @brief 精神力です。上昇負荷の発症で減少します。 */
        float mental = 100.0f;

        /** @brief スタミナです。走り、ロープ、攻撃、採掘、ステップ、ジャンプで消費します。 */
        float stamina = 100.0f;

        /** @brief 上昇負荷の内部ゲージです。10m分溜まると精神力を削って0に戻ります。 */
        float upperLoad = 0.0f;

        /** @brief 攻撃行動の残り時間です。0より大きい間は攻撃中です。 */
        float attackTimer = 0.0f;

        /** @brief 横振り攻撃の向きです。攻撃を開始するたびに反転します。 */
        bool attackSwingReverse = false;

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

    enum class RelicType
    {
        ArmamentUpgrade,
        WeaponUpgrade,
        ArmorUpgrade,
        Offensive,
        Survival,
        CashLow,
        CashHigh,
        MentalRecovery,
        Unique,
        Count
    };

    enum class EnemyType
    {
        Charger,
        Territory
    };

    enum class TerritoryRank
    {
        Low,
        Middle,
        High
    };

    enum class DeathCause
    {
        Other,
        Enemy,
        Starvation,
        UpperLoad,
        Fall
    };

    enum class ArmorTier
    {
        Leather,
        Iron,
        RelicCovered,
        RelicHardened,
        RelicEnhanced,
        Relic,
        Count
    };

    enum class WeaponTier
    {
        RustyPickaxe,
        NormalPickaxe,
        SturdyPickaxe,
        SharpPickaxe,
        RelicPickaxe,
        Count
    };

    /** @brief 所持、地面置き、鑑定、売却対象になる遺物です。 */
    struct RelicItem
    {
        /** @brief 採掘ポイント側で設定された発見物名です。 */
        std::string name;

        /** @brief 採掘時に確定する遺物種類です。 */
        RelicType type = RelicType::CashLow;

        /** @brief 遺物種類に応じた重量です。 */
        float weight = 10.0f;

        /** @brief 遺物種類に応じた売却価格です。 */
        int value = 30;

        int maxUses = 0;
        int remainingUses = 0;
        std::uint64_t acquisitionOrder = 0;
        bool broken = false;
        bool stabilized = false;
        bool autoTrigger = true;
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

    /** @brief 敵が落とし、フィールド上で拾える食料です。 */
    struct GroundFood
    {
        Vec2 pos;
        float depth = 0.0f;
        bool active = true;
    };

    /** @brief プレイヤー攻撃が命中した位置で再生するビルボードエフェクトです。 */
    struct AttackHitEffect
    {
        Vec2 pos;
        float depth = 0.0f;
        float remainingTime = 0.0f;
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

        bool sensed = false;

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

        EnemyType type = EnemyType::Charger;
        TerritoryRank territoryRank = TerritoryRank::Low;
        Vec2 spawnPos;
        Vec2 territoryCenter;
        std::array<Vec2, 3> patrolPoints = {};
        int patrolIndex = 0;

        /** @brief 敵が存在している深度です。別レイヤーのプレイヤーを追わない判定に使います。 */
        float depth = 0.0f;

        /** @brief 敵の体力です。つるはし3回程度で倒せるため3にしています。 */
        float hp = 3.0f;
        float maxHp = 3.0f;
        float attackDamage = 10.0f;
        float searchRange = 8.0f;
        float territoryRadius = 0.0f;
        float moveSpeed = 0.75f;
        float attackInterval = 5.0f;
        float telegraphDuration = 0.55f;
        float respawnTimer = 0.0f;

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
        bool discovered = false;

        /** @brief 地面にいるかどうかです。false の間は落下中として扱います。 */
        bool grounded = true;

        /** @brief 1回の体当たりで複数回ヒットしないようにするフラグです。 */
        bool hasHitThisCharge = false;

        /** @brief プレイヤーの現在の1攻撃で既に命中したかどうかです。 */
        bool hitByPlayerAttack = false;
        bool hitByRelicAttack = false;

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
     * 上端と下端は別々のXZ座標を持ち、両端の間を補間して昇降できます。
     */
    struct RopePoint
    {
        /** @brief ロープ上端の平面位置です。 */
        Vec2 topPos;

        /** @brief ロープ下端の平面位置です。 */
        Vec2 bottomPos;

        /** @brief ロープ上端の深度です。 */
        float topDepth = 0.0f;

        /** @brief ロープ下端の深度です。 */
        float bottomDepth = 4.0f;
    };

    struct LayerGateState
    {
        bool isEntry = false;
        Vec2 ropePos;
        Vec2 loadPos;
        float depth = 0.0f;
        int destinationAreaIndex = -1;
        int connectionId = -1;
        int generationFailures = 0;
        bool disabled = false;
        bool routeDiscovered = false;
        bool previewReady = false;
    };

    struct PlannedLayerGate
    {
        bool isEntry = false;
        int destinationAreaIndex = -1;
        int connectionId = -1;
    };

    struct AreaState
    {
        int depth = 1;
        int sublayer = 0;
        int areaNumber = 1;
        bool generated = false;
        bool canReturn = false;
        std::vector<PlannedLayerGate> plannedGates;
        NarakuMap::MapData map;
        std::vector<GroundRelic> groundRelics;
        std::vector<GroundFood> groundFoods;
        std::vector<MiningPoint> miningPoints;
        std::vector<EnemyState> enemies;
        std::vector<FloorRegion> floorRegions;
        std::vector<RopePoint> ropePoints;
        std::vector<LayerGateState> layerGates;
        std::vector<Vec2> pins;
        Vec2 startPoint;
        float startDepth = 0.0f;
        Vec2 returnPoint;
        float returnDepth = 0.0f;
        float worldHalfSize = 1.0f;
        float sensingTimer = 0.0f;
        float respawnClock = 0.0f;
        int discoveredEnemyCount = 0;
        int discoveredMiningCount = 0;
        int discoveredCliffCount = 0;
        int totalCliffCount = 0;
        bool firstAreaExpAwarded = false;
        bool firstAreaRewardAwarded = false;
        std::vector<std::uint8_t> discoveredCells;
        std::vector<std::uint8_t> discoveredCliffs;
        std::array<bool, 4> cellExpThresholds = {};
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
        int maxDepth = 1;

        /** @brief 今回の潜行で採掘した旧器数です。 */
        int minedCount = 0;

        /** @brief 帰還時に持ち帰った旧器数です。 */
        int carriedRelics = 0;

        /** @brief 死亡時に失った旧器数です。 */
        int lostRelics = 0;

        /** @brief 帰還時に全売却した場合の合計金額です。 */
        int saleAmount = 0;

        /** @brief 今回の帰還で初めて鑑定した遺物種類数です。 */
        int identifiedRelics = 0;

        int explorationReward = 0;
        int uniqueReward = 0;
        std::array<int, 5> minedByDepth = {};
        std::array<int, 5> chargerKillsByDepth = {};
        std::array<int, 5> territoryKillsByDepth = {};
        std::array<float, 5> staySecondsByDepth = {};
        int firstAreaCount = 0;
        int newRelicTypeCount = 0;
        int levelBeforeDeath = 1;
        int levelAfterDeath = 1;
        int protectionConsumed = 0;
    };

    struct EquipmentBonus
    {
        float maxHp = 0.0f;
        float maxStamina = 0.0f;
        float maxMental = 0.0f;
        float maxWeight = 0.0f;
        float staminaRecovery = 0.0f;
        float mentalRecovery = 0.0f;
        float attack = 0.0f;
        float defense = 0.0f;
        float walkSpeed = 0.0f;
        float runSpeed = 0.0f;
        float miningSpeed = 0.0f;
        float ropeAscentSpeed = 0.0f;
        float ropeDescentSpeed = 0.0f;
        float hpRecoveryPerSecond = 0.0f;
    };

    /**
     * @brief プレイテスト中に調整するプレイヤー用デバッグパラメータです。
     *
     * 既存の固定定数を大きく崩さず、移動、攻撃力、スタミナ消費量だけを
     * ランタイム編集可能な値としてまとめています。
     */
public:
    struct PlayerDebugParams
    {
        /** @brief 通常移動速度です。 */
        float walkSpeed = 1.5f;

        /** @brief 走り移動速度です。 */
        float runSpeed = 2.5f;

        /** @brief ロープ昇降時の深度変化速度です。 */
        float ropeSpeed = 1.0f;

        /** @brief 1回の攻撃で与えるダメージ量です。 */
        float attackPower = 1.0f;

        /** @brief 走り続けた時の1秒あたりスタミナ消費です。 */
        float runCostPerSecond = 1.5f;

        /** @brief ロープ昇降中の1秒あたりスタミナ消費です。 */
        float ropeCostPerSecond = 3.0f;

        /** @brief 攻撃1回のスタミナ消費です。 */
        float attackCost = 10.0f;

        /** @brief 採掘1回のスタミナ消費です。 */
        float miningCost = 7.0f;

        /** @brief ステップ1回のスタミナ消費です。 */
        float stepCost = 5.0f;

        /** @brief ジャンプ1回のスタミナ消費です。 */
        float jumpCost = 5.0f;

        /** @brief スタミナを消費していない時の1秒あたり回復量です。 */
        float staminaRecoverPerSecond = 2.0f;

        /** @brief プレイヤー現在深度より上にある地形レイヤーの描画アルファ値です。 */
        float upperLayerAlpha = 0.06f;

        /** @brief ミニマップの表示位置Xです。 */
        float minimapPosX = 20.0f;

        /** @brief ミニマップの表示位置Yです。 */
        float minimapPosY = 20.0f;

        /** @brief ミニマップのサイズです。 */
        float minimapSize = 220.0f;

        /** @brief ミニマップを表示するかどうかです。 */
        float showMinimap = 1.0f;
    };
private:

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

        /** @brief 帰還地点で帰還するか確認している状態です。 */
        ReturnConfirm,

        /** @brief 探窟放棄前の確認状態です。 */
        AbandonConfirm,

        /** @brief 生還後の鑑定結果と帰還先を表示している状態です。 */
        ReturnResult,

        /** @brief 死亡後のロストリザルトを表示している状態です。 */
        DeathResult,

        /** @brief 自宅で装備と次回持ち込み品を整える状態です。 */
        Home,

        /** @brief 食料、遺物の購入と遺物売却を行う状態です。 */
        GeneralShop,

        /** @brief 頭、胴装備とつるはしを購入する状態です。 */
        Armory,

        /** @brief 地上で満腹度と体力を回復する状態です。 */
        Restaurant,

        /** @brief 初回の層間口で接続先エリアを生成している状態です。 */
        Loading,

        /** @brief 2エリア間をロープで昇降している状態です。 */
        LayerTransition
    };

private:
    /** @brief 3x3マップを生成し、1回の潜行を初期状態に戻します。 */
    bool ResetRun();

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

    void UpdateHunger(float dt);
    void UpdateMentalAbilities(float dt);
    void UpdateExplorationDiscovery();
    void UpdateRespawns(float dt);

    void UpdateLoading();
    void UpdateLayerTransition(float dt);

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
    void StartDeath(const char* reason, DeathCause cause = DeathCause::Other);

    /** @brief 帰還処理を確定し、持ち帰った遺物を鑑定して自宅在庫へ移します。 */
    void FinishReturn();

    /** @brief 自宅で選択した持ち込み品を引き出し、新しい潜行を開始します。 */
    void StartDive();

    /** @brief 死亡後に再挑戦用の新しい潜行を開始します。 */
    void RestartAfterDeath();
    void AbandonDive();

    /** @brief DirectXのデバッグ形状で3Dフィールドを描画します。 */
    void Draw3DField();
    /** @brief 半透明床のバッチ描画に使うシェーダーを作成します。 */
    void InitializeTerrainFloorBatch();
    /** @brief 半透明床のバッチ描画資源を解放します。 */
    void ReleaseTerrainFloorBatch();
    /** @brief 現在エリアの最大床数に合わせて動的頂点バッファを再構築します。 */
    void RebuildTerrainFloorBatch();
    /** @brief 1枚の水平床を今フレームのバッチへ追加します。 */
    void AppendTerrainFloorQuad(
        const DirectX::XMFLOAT3& center,
        const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT4& color);
    /** @brief 今フレームに追加された半透明床を1回のドローで描画します。 */
    void DrawTerrainFloorBatch(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection);
    /** @brief 敵スプライトのビルボード描画に使う資源を作成します。 */
    void InitializeEnemyBillboardBatch();
    /** @brief 敵スプライトのビルボード描画資源を解放します。 */
    void ReleaseEnemyBillboardBatch();
    /** @brief 現在エリアの敵数に合わせて動的頂点バッファを再構築します。 */
    void RebuildEnemyBillboardBatch();
    /** @brief 生存中の敵スプライトをビルボードとして1回のドローで描画します。 */
    void DrawEnemyBillboardBatch(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection);
    /** @brief 環境モデル登録簿を読み込み、プロト描画用モデルを構築します。 */
    void LoadEnvironmentModels();
    /** @brief プロト描画用の環境モデルを解放します。 */
    void ReleaseEnvironmentModels();
    /** @brief 生成マップに配置された環境オブジェクトを描画します。 */
    void DrawEnvironmentObjects(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /** @brief カメラ方向に追従する方位コンパスを画面右上へ描画します。 */
    void DrawCompass() const;
    /** @brief 探索中の右ドラッグ入力から軌道カメラの角度を更新します。 */
    void UpdateCameraControls();
    /** @brief 現在のカメラの水平前方向を返します。 */
    Vec2 GetCameraForward() const;
    /** @brief 現在のカメラの水平右方向を返します。 */
    Vec2 GetCameraRight() const;
    /** @brief カメラ距離とY方向オフセットを安全な範囲へ正規化します。 */
    void NormalizeCameraSettings();

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

    /** @brief 帰還地点での確認ウィンドウを描画します。 */
    void DrawReturnConfirm();
    void DrawAbandonConfirm();

    /** @brief 自宅の装備変更、持ち込み品選択、潜行開始UIを描画します。 */
    void DrawHome();

    /** @brief 商店の購入・売却UIを描画します。 */
    void DrawGeneralShop();

    /** @brief 武具屋の武具購入UIを描画します。 */
    void DrawArmory();
    void DrawCurrentStatus();
    void DrawRestaurant();
    void DrawRouteInfo();

    /** @brief 所持品表示中に使う簡易地図とピン操作を描画します。 */
    void DrawMapControls();

    /** @brief runtime map と同期した常時表示ミニマップを描画します。 */
    void DrawMiniMap();

    /** @brief プレイテスト用のプレイヤー調整UIを描画します。 */
    void DrawDebugPlayerTuning();

    /** @brief プレイヤーの現在位置、高さ、および現在いる小ステージ名を表示するデバッグウィンドウを描画します。 */
    void DrawPlayerPositionDebug();

    /** @brief 採掘中の進行度バーを画面中央にオーバーレイ表示します。 */
    void DrawMiningProgressBar();

    void DrawLoadingScreen();

    /** @brief 操作不能理由などの短い通知をメイン表示領域の中央に描画します。 */
    void DrawCenterNotification();

    /** @brief 現在の総重量を計算します。 */
    float GetCurrentWeight() const;

    /** @brief 現在の装備効果を反映した重量上限を返します。 */
    float GetMaxWeight() const;

    /** @brief 地面から取得できる重量の上限を返します。 */
    float GetPickupWeightLimit() const;

    /** @brief 装備中のつるはしによる採掘速度倍率を返します。 */
    float GetMiningSpeedMultiplier() const;

    int GetCurrentDepth() const;
    float GetDepthExpMultiplier(int depth) const;
    float GetDepthMovementExpMultiplier(int depth) const;
    float GetDepthHungerMultiplier(int depth) const;
    float GetDepthRewardMultiplier(int depth) const;
    float GetDepthStayRewardMultiplier(int depth) const;

    /** @brief 遺物種類に対応する表示名を返します。 */
    const char* GetRelicTypeName(RelicType type) const;

    /** @brief 鑑定状態を考慮した遺物表示名を返します。 */
    const char* GetRelicDisplayName(const RelicItem& item) const;

    /** @brief 遺物種類に対応する重量を返します。 */
    float GetRelicWeight(RelicType type) const;

    /** @brief 遺物種類に対応する売却価格を返します。 */
    int GetRelicSellValue(RelicType type) const;

    /** @brief 指定種類の遺物アイテムを作成します。 */
    RelicItem CreateRelic(RelicType type, const std::string& sourceName);

    /** @brief 7種類から均等抽選した遺物を作成します。 */
    RelicItem CreateRandomRelic(const std::string& sourceName);

    int GetRelicActivity(const RelicItem& item) const;
    int GetCurrentActivity() const;
    bool IsRelicSellable(const RelicItem& item) const;
    bool UseMentalRecoveryRelic(int inventoryIndex);
    bool TryConsumeSurvivalRelic(bool hpLethal, bool mentalLethal);

    /** @brief 食料を1個使い、体力を2回復します。 */
    void UseFood();

    bool TryUseRestaurant();

    /** @brief 装備名を返します。 */
    const char* GetArmorName(ArmorTier tier) const;
    const char* GetArmorEffectText(ArmorTier tier, bool headSlot) const;

    /** @brief 頭と胴に遺物装備を揃えているか判定します。 */
    bool HasRelicArmorSetEffect() const;

    /** @brief 武器名を返します。 */
    const char* GetWeaponName(WeaponTier tier) const;
    const char* GetWeaponEffectText(WeaponTier tier) const;

    /** @brief 所持金と素材を確認して装備を購入します。 */
    bool TryBuyArmor(ArmorTier tier, bool headSlot, bool useMaterials);

    /** @brief 所持金と素材を確認して武器を購入します。 */
    bool TryBuyWeapon(WeaponTier tier, bool useMaterials);

    int CountStoredRelics(RelicType type) const;
    bool RemoveStoredRelics(RelicType type, int count);

    /** @brief 現在の重量上限に対する現在重量の割合を返します。 */
    float GetWeightRate() const;

    /** @brief 重量70%以上の速度低下を反映した歩行速度を返します。 */
    float GetMoveSpeed() const;

    /** @brief 重量90%以上のスタミナ消費2倍を反映した消費量を返します。 */
    float GetStaminaCost(float baseCost) const;

    /** @brief 指定した基礎消費量ぶんのスタミナを支払えるか判定します。 */
    bool CanSpendStamina(float baseCost) const;

    /** @brief 指定した基礎消費量を重量補正込みで実際に消費します。 */
    void SpendStamina(float baseCost);

    EquipmentBonus GetEquipmentBonus() const;
    float GetLevelGrowth() const;
    float GetMaxHp() const;
    float GetMaxStamina() const;
    float GetMaxMental() const;
    float GetStaminaRecoveryMultiplier() const;
    float GetMentalRecoveryMultiplier() const;
    float GetAttackPower() const;
    float GetDefenseMultiplier() const;
    float GetRunSpeed() const;
    float GetRopeSpeed(bool ascending) const;
    void PreserveResourceRatios(float oldMaxHp, float oldMaxStamina, float oldMaxMental);
    int GetRequiredExp(int level) const;
    void AwardExp(int amount);
    std::string FormatExp(std::int64_t value) const;
    void ApplyPlayerDamage(float damage, DeathCause cause, const char* reason);
    void ApplyMentalDamage(float damage, DeathCause cause, const char* reason);
    void ApplyDeathPenalty(DeathCause cause);
    void ApplyAbandonPenalty();
    int GetDeathLevelLoss(DeathCause cause) const;

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

    /** @brief メイン表示領域中央へ一定時間表示する通知を設定します。 */
    void ShowCenterNotification(const std::string& message);

    void SaveCurrentAreaState();
    void ActivateArea(int areaIndex, bool placeAtEntry);
    void BuildCurrentAreaRuntime(bool placeAtStart);
    bool BuildDiveStructure();
    bool GeneratePlannedArea(int areaIndex, std::string& outError);
    bool AssignPlannedGates(int areaIndex);
    const char* GetSublayerName(int sublayer) const;
    void TryUseLayerGate(int gateIndex);
    void BeginLayerTransition(int sourceGateIndex, int destinationAreaIndex);
    bool LoadDebugPlayerParams();
    bool SaveDebugPlayerParams() const;
    bool SaveProgress() const;
    bool LoadProgress();
    void InitializeNewProgress();
    void SpawnEnemiesForCurrentArea();
    bool RespawnEnemy(EnemyState& enemy);
    EnemyState CreateEnemy(EnemyType type, int depth, const Vec2& position) const;
    Vec2 FindEnemySpawnPoint(float minimumPlayerDistance, bool requireTerritory, bool* found) const;
    bool HasTerritoryTreeDensity(const Vec2& position) const;
    void AwardEnemyDefeat(const EnemyState& enemy);
    int CalculateReturnReward() const;
    void ActivateMiningSense();
    void ActivateUpperLoadWard();
    bool TryPreventUpperLoad();

    /** @brief プレイヤーの近くにある未発見採掘ポイントを発見済みにします。 */
    void DiscoverNearbyMiningPoints();

    /** @brief 所持品の指定旧器を現在位置に捨てます。 */
    void DropInventoryItem(int index);

    /** @brief 指定位置にピンを置くか、近くの既存ピンを削除します。 */
    void TogglePinAt(const Vec2& worldPos);

    /**
     * @brief マップ描画用のワールド範囲とキャンバス内配置をまとめた変換情報です。
     * @details 実際の terrainLayers 全体をアスペクト比維持で収めるために使います。
     */
    struct MapCanvasTransform
    {
        Vec2 worldMin;
        Vec2 worldMax;
        Vec2 drawPos;
        Vec2 drawSize;
        bool valid = false;
    };

    /**
     * @brief terrainLayers 全体が収まるキャンバス変換情報を計算します。
     * @param canvasPos キャンバス左上のスクリーン座標です。
     * @param canvasSize キャンバス全体のサイズです。
     * @param padding キャンバス内側へ確保する余白量です。
     * @return 有効な地形があれば変換情報を返し、なければ valid が false のまま返します。
     */
    MapCanvasTransform BuildMapCanvasTransform(const Vec2& canvasPos, const Vec2& canvasSize, float padding = 0.0f, float zoom = 1.0f) const;

    /** @brief ImGui上の座標をフィールド座標へ変換します。 */
    Vec2 ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos, float zoom = 1.0f, const Vec2& focusPos = { 0.0f, 0.0f }) const;

    /** @brief フィールド座標を地図用の真上視点ImGui座標へ変換します。 */
    Vec2 WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float zoom = 1.0f, const Vec2& focusPos = { 0.0f, 0.0f }) const;

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

    /** @brief ロープ番号と補間率から、ロープ上の平面位置を返します。 */
    Vec2 GetRopePosition(int ropeIndex, float progress) const;

    /** @brief ロープ番号と補間率から、ロープ上の絶対ワールド高さを返します。 */
    float GetRopeWorldY(int ropeIndex, float progress) const;

    /** @brief レイヤー頂点を地形高さ込みの3D座標へ変換します。 */
    DirectX::XMFLOAT3 GetTerrainVertexWorld3D(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ, float heightOffset = 0.0f) const;

    /** @brief 2Dフィールド座標と深度を3D座標へ変換します。 */
    DirectX::XMFLOAT3 ToWorld3D(const Vec2& pos, float depth = 0.0f, float heightOffset = 0.0f) const;

    /** @brief 指定位置、サイズ、回転でデバッグ箱を描画します。 */
    void DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale, float yawRad = 0.0f) const;

    /** @brief 指定位置とサイズでデバッグ球を描画します。 */
    void DrawDebugSphere3D(const DirectX::XMFLOAT3& pos, float radius) const;

    /** @brief 指定座標が床矩形内に入っているかを返します。 */
    bool IsInsideFloor(const FloorRegion& floor, const Vec2& pos) const;

    /** @brief 指定座標と深度に対応する床を返します。見つからなければ nullptr を返します。 */
    const FloorRegion* FindFloorAt(const Vec2& pos, float depth) const;

    /** @brief 指定座標と深度に歩ける床があるかを返します。 */
    bool HasFloorAt(const Vec2& pos, float depth) const;

    /** @brief 指定座標と深度に対応するセル属性フラグを返します。範囲外なら CellAttributeNone を返します。 */
    std::uint32_t GetCellAttributeFlagsAt(const Vec2& pos, float depth) const;

    /** @brief 深度と位置の両方に一致するレイヤー配列番号を返します。見つからなければ -1 を返します。 */
    int FindLayerIndexAt(const Vec2& pos, float depth, float tolerance = 0.20f) const;

    /** @brief 2点間の地形高低差と属性を見て、その移動を通してよいかを返します。 */
    bool CanTraverseGround(const Vec2& from, const Vec2& to, float depth) const;

    /** @brief 床外へ出る移動を止め、可能ならX方向またはY方向だけの移動に分解して通します。 */
    Vec2 ResolveFloorMove(const Vec2& from, const Vec2& to, float depth) const;

    /** @brief プレイヤー付近のロープ番号を返します。近くにない場合は -1 を返します。 */
    int FindNearestRopeIndex(float range) const;

    /** @brief 指定したロープから横へ降りられる床があればロープを離します。 */
    bool TryLeaveRopeSide(int ropeIndex, float leaveSign, const Vec2& cameraRight);

    /** @brief プレイヤー調整値を初期値へ戻します。 */
    void ResetDebugPlayerParams();

    /** @brief プレイヤー調整値が危険な値にならないよう丸めます。 */
    void ClampDebugPlayerParams();

private:
    /** @brief 現在のプレイヤー状態です。 */
    PlayerState m_player;

    /** @brief 現在所持している旧器一覧です。 */
    std::vector<RelicItem> m_inventory;

    std::vector<RelicItem> m_storedInventory;

    /** @brief フィールド上に置かれている旧器一覧です。 */
    std::vector<GroundRelic> m_groundRelics;

    /** @brief 敵が落とした拾得可能な食料一覧です。 */
    std::vector<GroundFood> m_groundFoods;

    /** @brief 第一層プロトタイプ用の採掘ポイント一覧です。 */
    std::vector<MiningPoint> m_miningPoints;

    /** @brief 第一層プロトタイプ用の敵一覧です。 */
    std::vector<EnemyState> m_enemies;

    /** @brief 再生中のプレイヤー攻撃命中エフェクト一覧です。 */
    std::vector<AttackHitEffect> m_attackHitEffects;

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

    /** @brief 現在のエリアに配置された層間口一覧です。 */
    std::vector<LayerGateState> m_layerGates;

    /** @brief 今回の潜行中に生成済みの全エリア状態です。 */
    std::vector<AreaState> m_areas;

    /** @brief 現在表示しているエリア番号です。 */
    int m_currentAreaIndex = -1;

    /** @brief 現在つかまっているロープ番号です。未使用時は -1 です。 */
    int m_activeRope = -1;

    /** @brief 現在つかまっているロープの上端0、下端1の補間率です。 */
    float m_ropeProgress = 0.0f;

    /** @brief プレイヤーが置いた地図ピン一覧です。 */
    std::vector<Vec2> m_pins;

    /** @brief HUDに表示する短いログ一覧です。 */
    std::vector<std::string> m_messages;

    /** @brief メイン表示領域中央へ表示する短い通知です。 */
    std::string m_centerNotification;

    /** @brief 中央通知の残り表示時間です。 */
    float m_centerNotificationTimer = 0.0f;

    /** @brief 初回接続先生成を要求した層間口番号です。 */
    int m_loadingSourceGateIndex = -1;

    /** @brief ロード画面を最低1フレーム表示するための処理段階です。 */
    int m_loadingStep = 0;

    /** @brief ロード画面へ表示する進捗率です。 */
    float m_loadingProgress = 0.0f;

    /** @brief 遷移元の層間口番号です。 */
    int m_transitionSourceGateIndex = -1;

    /** @brief 遷移先エリア番号です。 */
    int m_transitionDestinationAreaIndex = -1;

    /** @brief 遷移先で接続する層間口番号です。 */
    int m_transitionDestinationGateIndex = -1;

    /** @brief 層間ロープ移動の進行率です。 */
    float m_layerTransitionProgress = 0.0f;

    /** @brief 層間ロープ移動中の描画用高さです。 */
    float m_layerTransitionVisualOffset = 0.0f;

    /** @brief 現在の層間移動が上昇かどうかです。 */
    bool m_layerTransitionAscending = false;

    /** @brief 重量超過中の走行通知をキー押下ごとに一度だけ出すための状態です。 */
    bool m_heavyRunNotificationShown = false;

    /** @brief 現在のシーン内モードです。 */
    Mode m_mode = Mode::Explore;

    /** @brief Tキー画面で地図タブを表示しているかどうかです。 */
    bool m_inventoryMapShowingMap = false;

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

    /** @brief 現在装備中のつるはしを反映した今回の採掘所要時間です。 */
    float m_miningDuration = 2.0f;

    /** @brief 採掘中の採掘ポイント番号です。-1なら採掘していません。 */
    int m_miningIndex = -1;

    /** @brief 地上で持っている所持金です。 */
    int m_money = 0;

    int m_level = 1;
    int m_currentExp = 0;
    int m_levelProtection = 0;
    std::int64_t m_level100OverflowExp = 0;
    float m_fullness = 75.0f;
    std::array<float, 5> m_movementExpByDepth = {};
    std::uint64_t m_nextRelicAcquisitionOrder = 1;
    bool m_uniqueRelicReturned = false;
    bool m_uniqueRelicCodexUnlocked = false;
    bool m_uniqueRelicAchievementUnlocked = false;
    bool m_uniqueRelicStoryUnlocked = false;
    float m_qHoldTime = 0.0f;
    bool m_qWasPressed = false;
    bool m_qLongTriggered = false;
    float m_upperLoadWardTimer = 0.0f;
    float m_miningSenseTimer = 0.0f;
    DeathCause m_pendingDeathCause = DeathCause::Other;
    bool m_attackRelicTriggered = false;
    float m_foodUseTimer = 0.0f;
    float m_lastFrameMovementDistance = 0.0f;
    bool m_lastFrameRunning = false;
    bool m_lastFrameRopeMoving = false;
    bool m_diedSinceLastDive = false;

    /** @brief 潜行中に持っている食料数です。 */
    int m_foodCount = 0;

    /** @brief 自宅に保管している食料数です。 */
    int m_storedFoodCount = 0;

    /** @brief 次回潜行へ持ち込む食料数です。 */
    int m_loadoutFoodCount = 0;

    /** @brief 自宅に保管している鑑定済み遺物数です。 */
    /** @brief 次回潜行へ持ち込む鑑定済み遺物数です。 */
    std::array<int, static_cast<std::size_t>(RelicType::Count)> m_loadoutRelics = {};

    /** @brief 一度でも鑑定して正体を記憶した遺物種類です。 */
    std::array<bool, static_cast<std::size_t>(RelicType::Count)> m_identifiedRelics = {};

    /** @brief 所有している頭装備です。 */
    std::array<bool, static_cast<std::size_t>(ArmorTier::Count)> m_ownedHeadArmor = {};

    /** @brief 所有している胴装備です。 */
    std::array<bool, static_cast<std::size_t>(ArmorTier::Count)> m_ownedBodyArmor = {};

    /** @brief 所有している武器です。 */
    std::array<bool, static_cast<std::size_t>(WeaponTier::Count)> m_ownedWeapons = {};

    /** @brief 現在装備中の頭装備です。 */
    ArmorTier m_equippedHeadArmor = ArmorTier::Leather;

    /** @brief 現在装備中の胴装備です。 */
    ArmorTier m_equippedBodyArmor = ArmorTier::Leather;

    /** @brief 現在装備中の武器です。 */
    WeaponTier m_equippedWeapon = WeaponTier::RustyPickaxe;

    /** @brief 所持品UIで選択中の旧器番号です。-1なら未選択です。 */
    int m_selectedInventory = -1;

    /** @brief 半透明床バッチの1頂点です。現在の床色を維持するためRGBAを頂点へ持たせます。 */
    struct TerrainFloorVertex
    {
        DirectX::XMFLOAT3 position = {};
        DirectX::XMFLOAT4 color = {};
    };

    /** @brief 半透明床をまとめて送る動的頂点バッファです。 */
    MeshBuffer* m_terrainFloorMesh = nullptr;
    /** @brief 半透明床のワールド座標と頂点色を変換する頂点シェーダーです。 */
    VertexShader* m_terrainFloorVS = nullptr;
    /** @brief 半透明床の頂点色をそのまま出力するピクセルシェーダーです。 */
    PixelShader* m_terrainFloorPS = nullptr;
    /** @brief エリア内の最大床頂点数を確保し、フレーム間で再利用するCPU側配列です。 */
    std::vector<TerrainFloorVertex> m_terrainFloorVertices;
    /** @brief 今フレームに実際に描画する床頂点数です。 */
    unsigned int m_terrainFloorVertexCount = 0;

    /** @brief 敵ビルボードバッチの1頂点です。 */
    struct EnemyBillboardVertex
    {
        DirectX::XMFLOAT3 position = {};
        DirectX::XMFLOAT2 uv = {};
    };

    /** @brief 全敵ビルボードをまとめて送る動的頂点バッファです。 */
    MeshBuffer* m_enemyBillboardMesh = nullptr;
    /** @brief 敵ビルボードのワールド座標を変換する頂点シェーダーです。 */
    VertexShader* m_enemyBillboardVS = nullptr;
    /** @brief 敵画像を透過付きで描画するピクセルシェーダーです。 */
    PixelShader* m_enemyBillboardPS = nullptr;
    /** @brief 左上の緑スライムを含む敵スプライトシートです。 */
    Texture* m_enemyTexture = nullptr;
    /** @brief 現在エリアの最大敵数分を確保して再利用するCPU側配列です。 */
    std::vector<EnemyBillboardVertex> m_enemyBillboardVertices;
    /** @brief 今フレームに実際に描画する敵ビルボード頂点数です。 */
    unsigned int m_enemyBillboardVertexCount = 0;

    /** @brief プレイヤー攻撃命中時に再生する10コマのスプライトシートです。 */
    Texture* m_attackHitTexture = nullptr;

    /** @brief カメラ位置へ追従して描画するスカイスフィアです。 */
    Model* m_skyModel = nullptr;

    /** @brief 環境モデル登録簿から読み込んだモデル資源です。 */
    struct EnvironmentModelResource
    {
        std::string id;
        bool isTree = false;
        Model* model = nullptr;
        DirectX::XMFLOAT3 placementAnchor = {};
    };
    std::vector<EnvironmentModelResource> m_environmentModels;

    /** @brief 敵攻撃命中後にカメラを揺らす残り時間です。 */
    float m_cameraShakeTimer = 0.0f;

    /** @brief プレイテスト中に編集するプレイヤー調整値です。 */
    PlayerDebugParams m_debugPlayerParams;

    /** @brief 帰還範囲と採掘範囲の当たり判定形状を表示するかどうかです。 */
    bool m_showCollisionDebug = true;

    /** @brief Tキー地図専用の表示倍率です。 */
    float m_mapZoom = 3.0f;
    /** @brief 読み込んだマップ全体を収めるワールド半径です。 */
    float m_worldHalfSize = 45.0f;
    /** @brief 探索カメラの水平回転角（ラジアン）です。 */
    float m_cameraYaw = DirectX::XMConvertToRadians(45.0f);
    /** @brief 探索カメラの上下回転角（ラジアン）です。 */
    float m_cameraPitch = DirectX::XMConvertToRadians(35.264f);
    /** @brief 探索カメラとプレイヤーの距離です。 */
    float m_cameraDistance = 13.8564f;
    /** @brief 探索カメラのY方向オフセット下限です。 */
    /** @brief 探索カメラの仰角下限です。仰角は真横を0度、真上を90度とします。 */
    float m_cameraMinPitchDegrees = 10.0f;
    /** @brief 探索カメラのY方向オフセット上限です。 */
    /** @brief 探索カメラの仰角上限です。仰角は真横を0度、真上を90度とします。 */
    float m_cameraMaxPitchDegrees = 60.0f;

    /** @brief Tキー地図専用のスクロールオフセット（ワールド座標系）です。 */
    Vec2 m_mapScrollOffset = { 0.0f, 0.0f };

};
