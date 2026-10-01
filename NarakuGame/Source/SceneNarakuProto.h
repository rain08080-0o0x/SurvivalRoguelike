#pragma once

#include "Scene.h"
#include "NarakuMapData.h"
#include "NarakuPieceData.h"
#include "SceneNarakuInputSettings.h"
#include "ShaderList.h"

#include <DirectXMath.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

class Texture;
class Model;
class MeshBuffer;
class VertexShader;
class PixelShader;

/**
 * @brief 地上拠点から第五層までの奈落塔ゲーム進行を統括するシーンです。
 *
 * 既存プロジェクトの DirectX / ImGui / Input 初期化をそのまま使い、
 * ゲーム進行状態を保持し、責務別の実装ファイルへ更新・描画・保存処理を振り分けます。
 */
class SceneNarakuProto : public Scene
{
public:
    enum class PresentationScene
    {
        Town,
        Dive,
        Result
    };

    /** @brief プロトタイプの初期状態を作成します。 */
    SceneNarakuProto();

    /** @brief Scene 基底クラス経由で破棄されるため virtual destructor にしています。 */
    ~SceneNarakuProto() override;

#if defined(NARAKU_EDITOR_BUILD)
    /**
     * @brief 同一プレビュー設定を2回生成し、段階・接続・配置の一致を検証します。
     * @param outError 失敗理由を書き込む文字列です。
     * @return 判定結果を返します。
     */
    static bool VerifyPreviewGenerationDeterminism(std::string& outError);
    /**
     * @brief SetEditorPreviewOverview が担当する処理を現在状態へ反映します。
     * @details SetEditorPreviewOverview の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param enabled 該当する動作を有効にする場合はtrueです。
     */
    void SetEditorPreviewOverview(bool enabled);
    /**
     * @brief ReturnEditorPreviewToStart が担当する処理を現在状態へ反映します。
     * @details ReturnEditorPreviewToStart の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ReturnEditorPreviewToStart();
#endif

    /** @brief 1フレーム分の入力、移動、採掘、敵、上昇負荷を更新します。 */
    void Update() override;

    /** @brief ImGui のデバッグ表示としてフィールド、HUD、各種ウィンドウを描画します。 */
    void Draw() override;

    /**
     * @brief GetPresentationScene に必要な値を現在状態から算出して返します。
     * @details GetPresentationScene の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    PresentationScene GetPresentationScene() const;

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

        /** @brief 上昇負荷の内部ゲージです。20m分溜まると現在層の上昇負荷が発症します。 */
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

        /** @brief 最後に立っていた歩行可能セル上の位置です。 */
        Vec2 lastSafeGroundPos;

        /** @brief 最後に立っていた歩行可能セルの深度です。 */
        float lastSafeGroundDepth = 0.0f;

        /** @brief 安全地点が記録済みかどうかです。 */
        bool hasSafeGroundPos = false;

        /** @brief 歩行不可セル上に滞空している連続時間です。 */
        float blockedCellAirTime = 0.0f;

        /** @brief 歩行不可セル上で維持する水平方向の速度です。 */
        Vec2 blockedCellVelocity;
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

    enum class WaterQuality
    {
        None,
        Safe,
        Unboiled,
        Boiled
    };

    struct WaterBottle
    {
        float amount = 0.0f;
        float foodPoisoningChance = 0.0f;
        WaterQuality quality = WaterQuality::None;
        bool selectedForLoadout = false;
    };

    struct CookingKit
    {
        int remainingUses = 5;
        bool selectedForLoadout = false;
    };

    struct PortableLight
    {
        float remainingSeconds = 900.0f;
        bool broken = false;
        bool selectedForLoadout = true;
    };

    enum class CookingTarget
    {
        None,
        BoilBottle,
        HeatFood,
        CookFish,
        CookSizedFish
    };

    enum class FishSize { Small, Medium, Large, Count };

    enum class ArmorTier
    {
        Leather,
        Iron,
        RelicCovered,
        RelicHardened,
        RelicEnhanced,
        Relic,
        Unknown,
        None,
        Count
    };

    enum class WeaponTier
    {
        RustyPickaxe,
        NormalPickaxe,
        SturdyPickaxe,
        SharpPickaxe,
        RelicPickaxe,
        Unknown,
        None,
        Count
    };

    enum class AdventurerRank
    {
        Red,
        Blue,
        Black,
        Purple,
        Count
    };

    enum class QuestType
    {
        LostProperty,
        RequestedRelic,
        Hunt,
        EnemySurvey,
        MiningSurvey,
        UpgradePlan,
        ValuableRelics,
        SmallThings,
        Rescue,
        ScrapCollection,
        Count
    };

    enum class QuestStatus
    {
        Available,
        Active,
        Complete,
        Cooldown
    };

    enum class ImportantQuestType
    {
        FirstJourney,
        FirstClear,
        ReachSecond,
        ClearSecond,
        ReachThird,
        ClearThird,
        ReachFourth,
        ClearFourth,
        ReachFifth,
        ClearFifth,
        FirstHunt,
        FiveHunts,
        ThirteenHunts,
        FirstTerritoryHunt,
        ThreeTerritoryHunts,
        FindSecondBase,
        FindForwardBase,
        ReturnUniqueRelic,
        Count,
    };

    enum class ImportantQuestStatus
    {
        Locked,
        Active,
        Complete,
        Claimed,
    };

    struct ImportantQuestRecord
    {
        ImportantQuestStatus status = ImportantQuestStatus::Locked;
        int progress = 0;
    };

    enum class PromotionQuestStatus
    {
        Locked,
        Available,
        Active,
        Complete,
        FailedThisWeek,
        Claimed,
    };

    struct PromotionQuestRecord
    {
        PromotionQuestStatus status = PromotionQuestStatus::Locked;
        std::uint64_t acceptedAcquisitionOrder = 0;
        int cashRelicProgress = 0;
        int upgradeRelicProgress = 0;
    };

    struct QuestRecord
    {
        std::uint64_t id = 0;
        QuestType type = QuestType::LostProperty;
        QuestStatus status = QuestStatus::Available;
        int targetDepth = 1;
        int targetCount = 1;
        int progress = 0;
        int reward = 0;
        int rewardItemType = -1;
        int rewardItemCount = 0;
        int targetRelicType = -1;
        int targetEnemyType = -1;
        int targetAreaIndex = -1;
        float targetX = 0.0f;
        float targetZ = 0.0f;
        float targetLayerDepth = 0.0f;
        double remainingSeconds = 0.0;
        double cooldownSeconds = 0.0;
        std::uint64_t acceptedAcquisitionOrder = 0;
        bool targetPositionReady = false;
        bool targetInteracted = false;
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

        /** @brief 採掘地点由来なら元の採掘地点番号、通常の投棄品なら-1です。 */
        int sourceMiningIndex = -1;
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

    /** @brief ジャンプ開始地点で再生するエフェクトです。 */
    struct JumpEffect
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

        /** @brief 採掘完了から復活までの通常プレイ実時間です。 */
        float respawnTimer = 0.0f;

        /** @brief 週シードから再抽選列を決めるための採掘完了回数です。 */
        std::uint32_t extractionCount = 0;

        /** @brief この地点から出た遺物が未回収で、復活を待機しているかどうかです。 */
        bool outputPending = false;

        bool sensed = false;

        /** @brief 採掘完了時に発見される旧器名です。 */
        std::string relicName;
    };

    struct FishingPoint
    {
        Vec2 pos;
        float depth = 0.0f;
        bool lake = false;
        bool discovered = false;
        int remainingUses = 5;
        double rechargeGameSeconds = 0.0;
        std::string id;
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

        /** @brief 方向別スプライトの選択に使う現在の向きです。 */
        Vec2 facing = { 0.0f, 1.0f };

        /** @brief 現在、平面上を移動しているかどうかです。 */
        bool moving = false;

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

    struct RopeTraversalEndpoints
    {
        Vec2 supportPosition;
        float supportGroundWorldY = 0.0f;
        Vec2 topPosition;
        float topWorldY = 0.0f;
        Vec2 bottomPosition;
        float bottomWorldY = 0.0f;
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
        std::vector<FishingPoint> fishingPoints;
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

    struct SurfaceFacilityState
    {
        NarakuPiece::SurfaceFacilityType type = NarakuPiece::SurfaceFacilityType::None;
        Vec2 center;
        Vec2 interactionPoint;
        float depth = 0.0f;
        std::string modelPath;
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
        float hpRecoveryMaxRatioPerSecond = 0.0f;
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
        /** @brief 固定地上マップを歩行している状態です。 */
        Surface,

        /** @brief 通常の探索操作を受け付ける状態です。 */
        Explore,

        /** @brief 所持品と地図ピン操作を表示している状態です。 */
        Inventory,

        /** @brief 旧器発見時に拾うか置くかを選ばせる状態です。 */
        RelicPrompt,

        /** @brief 水場で直接飲むか採水するか選ぶ状態です。 */
        WaterPrompt,

        /** @brief 釣り地点で開始確認を表示している状態です。 */
        FishingConfirm,

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

        /** @brief 地上の通常クエスト受付を操作している状態です。 */
        QuestDesk,

        /** @brief 奈落塔入口で潜行先を選択している状態です。 */
        AbyssEntrance,

        /** @brief 第二拠点の補給・宿泊・売却画面です。 */
        SecondBase,

        /** @brief 前衛拠点の統合店舗画面です。 */
        ForwardBase,

        /** @brief 初回の層間口で接続先エリアを生成している状態です。 */
        Loading,

        /** @brief 階級の保険対象外となる深度へ降りる前の確認状態です。 */
        UninsuredDescentConfirm,

        /** @brief 2エリア間をロープで昇降している状態です。 */
        LayerTransition,

        /** @brief Scene遷移前の永続保存に失敗し、再試行を待っている状態です。 */
        SaveError
    };

    enum class ScreenFadePhase
    {
        None,
        FadeOut,
        FadeIn,
    };

    enum class ScreenFadeAction
    {
        None,
        StartDive,
        LayerTransition,
    };

private:
    /**
     * @brief 潜行全体の構造を生成し、1回の潜行を初期状態へ戻します。
     * @param generateCompleteEditorPreview Editorで全エリアを先行生成する場合はtrueです。
     * @return 必要なマップとランタイム状態を構築できた場合はtrueです。
     */
    bool ResetRun(bool generateCompleteEditorPreview = true);

    /**
     * @brief 前回潜行の一時状態を破棄し、新しい潜行に必要なプレイヤー・UI・ランタイム状態を初期化します。
     * @param preservedMoney 初期化後も維持する所持金です。
     */
    void ResetDiveRuntimeState(int preservedMoney);

    /**
     * @brief 保存済みの週間エリアから帰還可能な開始エリアを復元します。
     * @return 週間エリアを正常に復元して潜行開始状態を作れた場合はtrueです。
     */
    bool RestoreWeeklyRun();

    /**
     * @brief 毎フレーム共通の通知時間、アニメーション時間、ジャンプ演出寿命を更新します。
     * @details モード別更新より先に進める必要がある一時表示だけをまとめ、終了した演出を除去します。
     */
    void UpdateFrameEffects();

    /**
     * @brief DebugまたはEditorで当たり判定表示の切替入力を処理します。
     * @details 製品Releaseでは副作用を持たず、既存のCキー入力とメッセージだけを扱います。
     */
    void UpdateCollisionDebugToggle();

    /**
     * @brief 探索・地上・統合メニュー間の開閉入力とタブ切替を処理します。
     * @details 入力再割当中はメニュー操作を止め、閉じる際の入力ガードもこの関数で設定します。
     */
    void UpdateOverlayMenu();

    /**
     * @brief 探索または地上にいる間の携帯照明入力と燃焼時間を更新します。
     * @details メニューへ遷移したフレームでは更新せず、従来と同じモード判定順を維持します。
     */
    void UpdatePortableLightForActiveMode();

    /**
     * @brief 探索モード中の全更新を既定順序で呼び出します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateExplore(float dt);
    /**
     * @brief UpdateSurface が担当する状態を既定の更新順で進めます。
     * @details UpdateSurface の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateSurface(float dt);
    /**
     * @brief BuildSurfaceRuntime が担当する処理を現在状態へ反映します。
     * @details BuildSurfaceRuntime の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param spawnAtAbyssEntrance 該当する動作を有効にする場合はtrueです。
     * @return 処理に成功した場合はtrueです。
     */
    bool BuildSurfaceRuntime(bool spawnAtAbyssEntrance);
    /**
     * @brief EnterSurface が担当する処理を現在状態へ反映します。
     * @details EnterSurface の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param spawnAtAbyssEntrance 該当する動作を有効にする場合はtrueです。
     */
    void EnterSurface(bool spawnAtAbyssEntrance);
    /**
     * @brief TryInteractSurface の成立条件を確認し、成立した処理だけを反映します。
     * @details TryInteractSurface の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void TryInteractSurface();

    /**
     * @brief 移動、走り、ステップ、ジャンプ、ロープ昇降を更新します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateMovement(float dt);

    /** @brief 1フレームのShift入力判定結果です。 */
    struct RunInputState
    {
        /** @brief 現在Shiftが押されている場合はtrueです。 */
        bool shiftPressed = false;

        /** @brief 短押し判定を越えて走行入力として扱う場合はtrueです。 */
        bool wantsRun = false;
    };

    /**
     * @brief Shiftの押下開始・解放・長押しを判定し、ステップまたは走行意図へ変換します。
     * @param dt 今フレームの経過秒数です。
     * @param isMining 採掘中で移動入力を無効にする場合はtrueです。
     * @param inLandingRecovery 着地硬直中でステップを禁止する場合はtrueです。
     * @return 現在のShift押下状態と走行意図です。
     */
    RunInputState UpdateRunInput(float dt, bool isMining, bool inLandingRecovery);

    /**
     * @brief ロープ把持中の横離脱と昇降を更新し、端点到達時は地面へ降ろします。
     * @param dt 今フレームの経過秒数です。
     * @param inputY 前後方向の入力値です。
     * @param isMining 採掘中でロープ入力を無効にする場合はtrueです。
     * @param cameraRight カメラから見た右方向です。
     * @param frameStartPos フレーム開始時のプレイヤー位置です。
     * @return ロープ番号不正により呼び出し元の移動更新を終了させる場合はtrueです。
     */
    bool UpdateRopeMovement(float dt, float inputY, bool isMining, const Vec2& cameraRight, const Vec2& frameStartPos);

    /**
     * @brief 歩行不可セル上の空中移動、斜面滑落、湖落下、安全地点復帰を更新します。
     * @param dt 今フレームの経過秒数です。
     * @param groundedMoveStartPos 地上移動開始時の位置です。
     * @param startedOverBlockedCell フレーム開始時から歩行不可セル上にいた場合はtrueです。
     * @param suppressMovementDistance 安全地点へ戻した場合に移動距離集計を抑止する出力値です。
     */
    void UpdateBlockedTerrainFall(
        float dt,
        const Vec2& groundedMoveStartPos,
        bool startedOverBlockedCell,
        bool& suppressMovementDistance);

    /**
     * @brief 地上からの踏み外し判定と、空中の重力・落下ダメージ・着地硬直を更新します。
     * @param dt 今フレームの経過秒数です。
     * @param groundedMoveStartPos 地上移動開始時の位置です。
     * @param groundedMoveStartGroundY 地上移動開始時のワールド高さです。
     * @param suppressMovementDistance 安全地点復帰直後の踏み外し判定を抑止する場合はtrueです。
     */
    void UpdateAirborneMotion(
        float dt,
        const Vec2& groundedMoveStartPos,
        float groundedMoveStartGroundY,
        bool suppressMovementDistance);

    /**
     * @brief プレイヤー位置をフィールド範囲へ制限し、危険地形の継続ダメージを更新します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateMovementBoundsAndHazard(float dt);

    /**
     * @brief 攻撃タイマー、攻撃判定、スタミナ自然回復を更新します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateAction(float dt);

    /**
     * @brief 使用待機中の通常食料または加熱食料を確定し、回復値と所持数へ反映します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateFoodUse(float dt);

    /**
     * @brief カメラ揺れと攻撃ヒット演出の残り時間を更新し、終了した演出を除去します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateAttackEffects(float dt);

    /**
     * @brief 通常攻撃の発生フレームを判定し、敵への通常・遺物攻撃と撃破処理を適用します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateMeleeAttack(float dt);

    /**
     * @brief 攻撃・採掘・走行・ロープ昇降中でなければスタミナを自然回復します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateStaminaRecovery(float dt);

    /**
     * @brief 採掘中タイマーを進め、完了時に旧器発見確認へ移行します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateMining(float dt);
    /**
     * @brief UpdateFishing が担当する状態を既定の更新順で進めます。
     * @details UpdateFishing の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateFishing(float dt);
    /**
     * @brief UpdateFishingPointRecharge が担当する状態を既定の更新順で進めます。
     * @details UpdateFishingPointRecharge の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param gameSeconds 進めるゲーム内時間の秒数です。
     */
    void UpdateFishingPointRecharge(double gameSeconds);
    /**
     * @brief StartFishing が担当する処理を現在状態へ反映します。
     * @details StartFishing の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param pointIndex 処理対象を示す配列番号です。
     */
    void StartFishing(int pointIndex);
    /**
     * @brief CancelFishing の条件を現在状態から判定します。
     * @details CancelFishing の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param reason 表示または記録に使用する理由・内容です。
     */
    void CancelFishing(const char* reason);
    /**
     * @brief CompleteFishing が担当する処理を現在状態へ反映します。
     * @details CompleteFishing の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void CompleteFishing();

    /**
     * @brief 敵の追跡、予備動作、体当たり、ヒット判定を更新します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateEnemies(float dt);

    /**
     * @brief ワールド上の上昇量から上昇負荷ゲージを加算または回復します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateUpperLoad(float dt);

    /** @brief 現在層とレベルに応じた上昇負荷を1回発症させます。 */
    void TriggerUpperLoad();

    /**
     * @brief 上昇負荷の持続時間と第五層の移動ダメージを更新します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateUpperLoadEffects(float dt);

    /**
     * @brief UpdateHunger が担当する状態を既定の更新順で進めます。
     * @details UpdateHunger の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateHunger(float dt);
    /**
     * @brief UpdateHydration が担当する状態を既定の更新順で進めます。
     * @details UpdateHydration の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateHydration(float dt);
    /**
     * @brief UpdateCooking が担当する状態を既定の更新順で進めます。
     * @details UpdateCooking の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateCooking(float dt);
    /**
     * @brief UpdateMentalAbilities が担当する状態を既定の更新順で進めます。
     * @details UpdateMentalAbilities の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateMentalAbilities(float dt);
    /**
     * @brief UpdateExplorationDiscovery が担当する状態を既定の更新順で進めます。
     * @details UpdateExplorationDiscovery の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UpdateExplorationDiscovery();
    /**
     * @brief UpdateRespawns が担当する状態を既定の更新順で進めます。
     * @details UpdateRespawns の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateRespawns(float dt);
    /**
     * @brief CaptureWeeklyWorld が担当する処理を現在状態へ反映します。
     * @details CaptureWeeklyWorld の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void CaptureWeeklyWorld();
    /**
     * @brief UpdateMiningRespawns が担当する状態を既定の更新順で進めます。
     * @details UpdateMiningRespawns の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateMiningRespawns(float dt);
    /**
     * @brief SaveWeeklyWorld の対象状態を既存形式で永続化します。
     * @details SaveWeeklyWorld の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool SaveWeeklyWorld() const;
    /**
     * @brief LoadWeeklyWorld の入力元を読み込み、検証済みの値を状態へ反映します。
     * @details LoadWeeklyWorld の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool LoadWeeklyWorld();
    /**
     * @brief RestoreWeeklyAreaRuntimes が担当する処理を現在状態へ反映します。
     * @details RestoreWeeklyAreaRuntimes の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool RestoreWeeklyAreaRuntimes();
    /**
     * @brief BeginNewGameWeek が担当する処理を現在状態へ反映します。
     * @details BeginNewGameWeek の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void BeginNewGameWeek();
    /**
     * @brief UpdateWorldClockAndQuests が担当する状態を既定の更新順で進めます。
     * @details UpdateWorldClockAndQuests の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateWorldClockAndQuests(float dt);
    /**
     * @brief EnsureQuestBoard が担当する処理を現在状態へ反映します。
     * @details EnsureQuestBoard の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void EnsureQuestBoard();
    /**
     * @brief ResetAvailableQuestsForNewWeek が担当する状態を既定値へ初期化します。
     * @details ResetAvailableQuestsForNewWeek の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ResetAvailableQuestsForNewWeek();
    /**
     * @brief GenerateQuestForSlot が担当するデータを既存ルールに従って生成します。
     * @details GenerateQuestForSlot の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param slotIndex 処理対象を示す配列番号です。
     */
    void GenerateQuestForSlot(std::size_t slotIndex);
    /**
     * @brief AcceptQuest が担当する処理を現在状態へ反映します。
     * @details AcceptQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param slotIndex 処理対象を示す配列番号です。
     * @return 処理に成功した場合はtrueです。
     */
    bool AcceptQuest(std::size_t slotIndex);
    /**
     * @brief ReportQuest が担当する処理を現在状態へ反映します。
     * @details ReportQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param slotIndex 処理対象を示す配列番号です。
     */
    void ReportQuest(std::size_t slotIndex);
    /**
     * @brief FailQuest が担当する処理を現在状態へ反映します。
     * @details FailQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param slotIndex 処理対象を示す配列番号です。
     */
    void FailQuest(std::size_t slotIndex);
    /**
     * @brief UpdateQuestDiscoveryProgress が担当する状態を既定の更新順で進めます。
     * @details UpdateQuestDiscoveryProgress の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param enemyDiscovered enemyDiscovered に指定する処理条件または対象値です。
     * @param miningDiscovered miningDiscovered に指定する処理条件または対象値です。
     * @param depth 判定または処理対象の深度です。
     */
    void UpdateQuestDiscoveryProgress(bool enemyDiscovered, bool miningDiscovered, int depth);
    /**
     * @brief UpdateQuestReturnProgress が担当する状態を既定の更新順で進めます。
     * @details UpdateQuestReturnProgress の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param returnedItems returnedItems に指定する処理条件または対象値です。
     */
    void UpdateQuestReturnProgress(const std::vector<RelicItem>& returnedItems);
    /**
     * @brief TryInteractWithQuestTarget の成立条件を確認し、成立した処理だけを反映します。
     * @details TryInteractWithQuestTarget の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryInteractWithQuestTarget();
    /**
     * @brief EnsureQuestTargetPosition が担当する処理を現在状態へ反映します。
     * @details EnsureQuestTargetPosition の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param quest 参照または更新する対象データです。
     */
    void EnsureQuestTargetPosition(QuestRecord& quest);
    /**
     * @brief DrawQuestTargets3D が担当する表示要素を現在状態から描画します。
     * @details DrawQuestTargets3D の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawQuestTargets3D();
    /**
     * @brief InitializeImportantQuests が担当する状態を既定値へ初期化します。
     * @details InitializeImportantQuests の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void InitializeImportantQuests();
    /**
     * @brief UnlockImportantQuest が担当する処理を現在状態へ反映します。
     * @details UnlockImportantQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     */
    void UnlockImportantQuest(ImportantQuestType type);
    /**
     * @brief UpdateImportantQuestArrival が担当する状態を既定の更新順で進めます。
     * @details UpdateImportantQuestArrival の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UpdateImportantQuestArrival();
    /**
     * @brief UpdateImportantQuestExploration が担当する状態を既定の更新順で進めます。
     * @details UpdateImportantQuestExploration の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UpdateImportantQuestExploration();
    /**
     * @brief UpdateImportantQuestDefeat が担当する状態を既定の更新順で進めます。
     * @details UpdateImportantQuestDefeat の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param enemy 参照または更新する対象データです。
     */
    void UpdateImportantQuestDefeat(const EnemyState& enemy);
    /**
     * @brief UpdateImportantQuestUniqueReturn が担当する状態を既定の更新順で進めます。
     * @details UpdateImportantQuestUniqueReturn の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UpdateImportantQuestUniqueReturn();
    /**
     * @brief ReportImportantQuest が担当する処理を現在状態へ反映します。
     * @details ReportImportantQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     */
    void ReportImportantQuest(std::size_t questIndex);
    /**
     * @brief IsQuestDeskUnlocked の条件を現在状態から判定します。
     * @details IsQuestDeskUnlocked の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsQuestDeskUnlocked() const;
    /**
     * @brief GetImportantQuestName に必要な値を現在状態から算出して返します。
     * @details GetImportantQuestName の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetImportantQuestName(ImportantQuestType type) const;
    /**
     * @brief GetImportantQuestDescription に必要な値を現在状態から算出して返します。
     * @details GetImportantQuestDescription の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetImportantQuestDescription(ImportantQuestType type) const;
    /**
     * @brief GetImportantQuestMoneyReward に必要な値を現在状態から算出して返します。
     * @details GetImportantQuestMoneyReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetImportantQuestMoneyReward(ImportantQuestType type) const;
    /**
     * @brief GetImportantQuestExpReward に必要な値を現在状態から算出して返します。
     * @details GetImportantQuestExpReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetImportantQuestExpReward(ImportantQuestType type) const;
    /**
     * @brief GetImportantQuestFoodReward に必要な値を現在状態から算出して返します。
     * @details GetImportantQuestFoodReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetImportantQuestFoodReward(ImportantQuestType type) const;
    /**
     * @brief GetImportantQuestWaterBottleReward に必要な値を現在状態から算出して返します。
     * @details GetImportantQuestWaterBottleReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetImportantQuestWaterBottleReward(ImportantQuestType type) const;
    /**
     * @brief InitializePromotionQuests が担当する状態を既定値へ初期化します。
     * @details InitializePromotionQuests の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void InitializePromotionQuests();
    /**
     * @brief RefreshPromotionQuestAvailability が担当する処理を現在状態へ反映します。
     * @details RefreshPromotionQuestAvailability の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void RefreshPromotionQuestAvailability();
    /**
     * @brief ResetPromotionQuestFailuresForNewWeek が担当する状態を既定値へ初期化します。
     * @details ResetPromotionQuestFailuresForNewWeek の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ResetPromotionQuestFailuresForNewWeek();
    /**
     * @brief AcceptPromotionQuest が担当する処理を現在状態へ反映します。
     * @details AcceptPromotionQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 処理に成功した場合はtrueです。
     */
    bool AcceptPromotionQuest(std::size_t questIndex);
    /**
     * @brief UpdatePromotionQuestReturn が担当する状態を既定の更新順で進めます。
     * @details UpdatePromotionQuestReturn の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param returnedItems returnedItems に指定する処理条件または対象値です。
     */
    void UpdatePromotionQuestReturn(const std::vector<RelicItem>& returnedItems);
    /**
     * @brief FailActivePromotionQuest が担当する処理を現在状態へ反映します。
     * @details FailActivePromotionQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void FailActivePromotionQuest();
    /**
     * @brief ReportPromotionQuest が担当する処理を現在状態へ反映します。
     * @details ReportPromotionQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     */
    void ReportPromotionQuest(std::size_t questIndex);
    /**
     * @brief GetPromotionQuestTargetDepth に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestTargetDepth の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestTargetDepth(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestRequiredLevel に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestRequiredLevel の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestRequiredLevel(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestRequiredCash に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestRequiredCash の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestRequiredCash(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestRequiredUpgrade に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestRequiredUpgrade の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestRequiredUpgrade(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestMoneyReward に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestMoneyReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestMoneyReward(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestExpReward に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestExpReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestExpReward(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestMaterialReward に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestMaterialReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetPromotionQuestMaterialReward(std::size_t questIndex) const;
    /**
     * @brief GetPromotionQuestName に必要な値を現在状態から算出して返します。
     * @details GetPromotionQuestName の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param questIndex 処理対象を示す配列番号です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetPromotionQuestName(std::size_t questIndex) const;

    /**
     * @brief UpdateLoading が担当する状態を既定の更新順で進めます。
     * @details UpdateLoading の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UpdateLoading();
#if defined(NARAKU_EDITOR_BUILD)
    /**
     * @brief UpdateEditorPreviewGeneration が担当する状態を既定の更新順で進めます。
     * @details UpdateEditorPreviewGeneration の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UpdateEditorPreviewGeneration();
#endif
    /**
     * @brief UpdateLayerTransition が担当する状態を既定の更新順で進めます。
     * @details UpdateLayerTransition の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateLayerTransition(float dt);
    /**
     * @brief BeginScreenFade が担当する処理を現在状態へ反映します。
     * @details BeginScreenFade の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param action action に指定する処理条件または対象値です。
     */
    void BeginScreenFade(ScreenFadeAction action);
    /**
     * @brief UpdateScreenFade が担当する状態を既定の更新順で進めます。
     * @details UpdateScreenFade の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateScreenFade(float dt);
    /**
     * @brief DrawScreenFade が担当する表示要素を現在状態から描画します。
     * @details DrawScreenFade の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawScreenFade() const;
    /**
     * @brief CompleteLayerTransition が担当する処理を現在状態へ反映します。
     * @details CompleteLayerTransition の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void CompleteLayerTransition();

    /** @brief Fキーで行う帰還、ロープ、旧器拾い、採掘開始を近い順に処理します。 */
    void TryInteract();

    /**
     * @brief 落下中のプレイヤーが近傍のロープを掴めるか判定し、掴んだ状態へ遷移します。
     * @return ロープを掴んでインタラクトを完了した場合はtrueです。
     */
    bool TryCatchRopeWhileFalling();

    /**
     * @brief 現在位置にある第二拠点または前衛拠点を検索し、対応する画面へ遷移します。
     * @return 拠点へ入ってインタラクトを完了した場合はtrueです。Editorでは常にfalseです。
     */
    bool TryEnterNearbyBase();

    /**
     * @brief 現在位置にある層間口を検索し、該当する接続先の利用処理を開始します。
     * @return 層間口を見つけてインタラクトを完了した場合はtrueです。
     */
    bool TryUseNearbyLayerGate();

    /**
     * @brief 帰還可能エリアの帰還地点にいるか判定し、帰還確認画面を開きます。
     * @return 帰還確認を開いてインタラクトを完了した場合はtrueです。
     */
    bool TryOpenReturnConfirmation();

    /**
     * @brief 最寄りのロープを検索し、現在状態に応じてロープへの把持または端点での離脱を行います。
     * @return 対象ロープを処理してインタラクトを完了した場合はtrueです。
     */
    bool TryToggleNearbyRope();

    /** @brief Shift短押しでステップを開始できるか判定して開始します。 */
    void TryStartStep();

    /** @brief Spaceでジャンプを開始できるか判定して開始します。 */
    void TryStartJump();

    /** @brief 左クリックで攻撃を開始できるか判定して開始します。 */
    void TryStartAttack();

    /** @brief マウスカーソル位置から地面ワールド座標と攻撃向きを計算・更新します。 */
    void UpdateAimDirectionFromMouse();
    /**
     * @brief GetMouseAimGroundPosition に必要な値を現在状態から算出して返します。
     * @details GetMouseAimGroundPosition の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 GetMouseAimGroundPosition();

    /**
     * @brief 左右Shiftのどちらかが押されているかを物理キーとして判定します。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsShiftPress() const;

    /**
     * @brief 死亡リザルトへ移行し、所持旧器とピンを失わせます。
     * @param reason 表示または記録に使用する理由・内容です。
     * @param cause cause に指定する処理条件または対象値です。
     */
    void StartDeath(const char* reason, DeathCause cause = DeathCause::Other);

    /** @brief 帰還処理を確定し、持ち帰った遺物を鑑定して自宅在庫へ移します。 */
    void FinishReturn();

    /**
     * @brief 自宅で選択した持ち込み品を引き出し、新しい潜行を開始します。
     * @param targetDepth 判定または処理対象の深度です。
     */
    void StartDive(int targetDepth = 1);
    /**
     * @brief CompleteStartDive が担当する処理を現在状態へ反映します。
     * @details CompleteStartDive の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void CompleteStartDive();

    /** @brief 死亡後に再挑戦用の新しい潜行を開始します。 */
    void RestartAfterDeath();
    /**
     * @brief 死亡時に退避した荷物を回収するか破棄するか確定します。
     * @param recover 該当する動作を有効にする場合はtrueです。
     */
    void ResolveDeathRecovery(bool recover);
    /**
     * @brief 死亡地点と階級から荷物回収を利用できるか返します。
     * @param deathDepth 判定または処理対象の深度です。
     * @return 条件を満たす場合はtrueです。
     */
    bool CanRecoverDeathInventory(int deathDepth) const;
    /**
     * @brief 回収待ち荷物が1つ以上あるか返します。
     * @return 条件を満たす場合はtrueです。
     */
    bool HasPendingDeathRecoveryItems() const;
    /** @brief 死亡時に取得済みだった落とし物依頼品を同じ地点へ戻します。 */
    void RestoreLostPropertyQuestTargetsAfterDeath();
    /**
     * @brief AbandonDive が担当する処理を現在状態へ反映します。
     * @details AbandonDive の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void AbandonDive();

    /** @brief DirectXのデバッグ形状で3Dフィールドを描画します。 */
    void Draw3DField();
    /**
     * @brief 3Dフィールド描画で共有するカメラ情報を構築します。
     * @details プレイヤー追従、地形との衝突補正、画面振動を順に適用し、描画用行列を設定します。
     * @param[out] view 描画に使用するビュー行列です。
     * @param[out] projection 描画に使用する射影行列です。
     * @param[out] cameraPosition 補正適用後のカメラ位置です。
     * @param[out] playerCenter 3D描画上のプレイヤー中心位置です。
     */
    void SetupFieldCamera3D(
        DirectX::XMFLOAT4X4& view,
        DirectX::XMFLOAT4X4& projection,
        DirectX::XMFLOAT3& cameraPosition,
        DirectX::XMFLOAT3& playerCenter);
    /**
     * @brief 3Dフィールドの背景モデルを描画します。
     * @param view 描画に使用するビュー行列です。
     * @param projection 描画に使用する射影行列です。
     * @param cameraPosition 背景モデルを追従させるカメラ位置です。
     */
    void DrawFieldSky3D(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /**
     * @brief 3Dフィールドの環境モデル、地形、施設範囲を描画します。
     * @details 不透明モデル、半透明地形、ライン表示の既存描画順を維持します。
     * @param view 描画に使用するビュー行列です。
     * @param projection 描画に使用する射影行列です。
     * @param cameraPosition 環境モデルの描画に使用するカメラ位置です。
     */
    void DrawFieldWorldGeometry3D(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /**
     * @brief フィールド上の採取物、敵、プレイヤー、デバッグ範囲を描画します。
     * @param view 描画に使用するビュー行列です。
     * @param projection 描画に使用する射影行列です。
     * @param playerCenter プレイヤースプライトを配置する3D中心位置です。
     */
    void DrawFieldActors3D(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& playerCenter);
    /**
     * @brief 攻撃命中エフェクトをカメラ正対のスプライトとして描画します。
     * @param view ビルボード行列の算出に使用するビュー行列です。
     */
    void DrawAttackHitEffects3D(const DirectX::XMFLOAT4X4& view);
    /** @brief 半透明床のバッチ描画に使うシェーダーを作成します。 */
    void InitializeTerrainFloorBatch();
    /** @brief 半透明床のバッチ描画資源を解放します。 */
    void ReleaseTerrainFloorBatch();
    /** @brief 現在エリアの最大床数に合わせて動的頂点バッファを再構築します。 */
    void RebuildTerrainFloorBatch();
    /**
     * @brief 1枚の水平床を今フレームのバッチへ追加します。
     * @param center center に指定する処理条件または対象値です。
     * @param size size に指定する処理条件または対象値です。
     * @param color color に指定する処理条件または対象値です。
     */
    void AppendTerrainFloorQuad(
        const DirectX::XMFLOAT3& center,
        const DirectX::XMFLOAT2& size,
        const DirectX::XMFLOAT4& color);
    /**
     * @brief 地形セルの4頂点を高さどおりに今フレームのテクスチャ別バッチへ追加します。
     * @param topLeft topLeft に指定する処理条件または対象値です。
     * @param topRight topRight に指定する処理条件または対象値です。
     * @param bottomLeft bottomLeft に指定する処理条件または対象値です。
     * @param bottomRight bottomRight に指定する処理条件または対象値です。
     * @param topLeftNormal topLeftNormal に指定する処理条件または対象値です。
     * @param topRightNormal topRightNormal に指定する処理条件または対象値です。
     * @param bottomLeftNormal bottomLeftNormal に指定する処理条件または対象値です。
     * @param bottomRightNormal bottomRightNormal に指定する処理条件または対象値です。
     * @param color color に指定する処理条件または対象値です。
     * @param textureId textureId に指定する処理条件または対象値です。
     */
    void AppendTerrainFloorCell(
        const DirectX::XMFLOAT3& topLeft,
        const DirectX::XMFLOAT3& topRight,
        const DirectX::XMFLOAT3& bottomLeft,
        const DirectX::XMFLOAT3& bottomRight,
        const DirectX::XMFLOAT3& topLeftNormal,
        const DirectX::XMFLOAT3& topRightNormal,
        const DirectX::XMFLOAT3& bottomLeftNormal,
        const DirectX::XMFLOAT3& bottomRightNormal,
        const DirectX::XMFLOAT4& color,
        int textureId);
    /**
     * @brief 地形セルの4頂点へ半透明の補助面を追加します。
     * @param topLeft topLeft に指定する処理条件または対象値です。
     * @param topRight topRight に指定する処理条件または対象値です。
     * @param bottomLeft bottomLeft に指定する処理条件または対象値です。
     * @param bottomRight bottomRight に指定する処理条件または対象値です。
     * @param color color に指定する処理条件または対象値です。
     */
    void AppendTerrainFloorOverlayCell(
        const DirectX::XMFLOAT3& topLeft,
        const DirectX::XMFLOAT3& topRight,
        const DirectX::XMFLOAT3& bottomLeft,
        const DirectX::XMFLOAT3& bottomRight,
        const DirectX::XMFLOAT4& color);
    /**
     * @brief 今フレームに追加された半透明床を1回のドローで描画します。
     * @param view view に指定する処理条件または対象値です。
     * @param projection projection に指定する処理条件または対象値です。
     */
    void DrawTerrainFloorBatch(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection);
    /** @brief 敵スプライトのビルボード描画に使う資源を作成します。 */
    void InitializeEnemyBillboardBatch();
    /** @brief 敵スプライトのビルボード描画資源を解放します。 */
    void ReleaseEnemyBillboardBatch();
    /** @brief 現在エリアの敵数に合わせて動的頂点バッファを再構築します。 */
    void RebuildEnemyBillboardBatch();
    /**
     * @brief 生存中の敵スプライトをビルボードとして1回のドローで描画します。
     * @param view view に指定する処理条件または対象値です。
     * @param projection projection に指定する処理条件または対象値です。
     * @param enemyType 処理対象の種類です。
     * @param texture texture に指定する処理条件または対象値です。
     */
    void DrawEnemyBillboardBatch(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        EnemyType enemyType,
        Texture* texture);
    /** @brief 環境モデル登録簿を読み込み、プロト描画用モデルを構築します。 */
    void LoadEnvironmentModels();
    /** @brief プロト描画用の環境モデルを解放します。 */
    void ReleaseEnvironmentModels();
    /**
     * @brief 生成マップに配置された環境オブジェクトを描画します。
     * @param view view に指定する処理条件または対象値です。
     * @param projection projection に指定する処理条件または対象値です。
     * @param cameraPosition 判定または処理対象の位置です。
     */
    void DrawEnvironmentObjects(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /** @brief 前衛拠点と第二拠点のモデルを読み込みます。 */
    void LoadBaseModels();
    /** @brief 拠点モデルを解放します。 */
    void ReleaseBaseModels();
    /**
     * @brief 生成マップ上の拠点を種別に対応するモデルで描画します。
     * @param view view に指定する処理条件または対象値です。
     * @param projection projection に指定する処理条件または対象値です。
     * @param cameraPosition 判定または処理対象の位置です。
     */
    void DrawBaseModels(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /** @brief ロープ表示用モデルとテクスチャを読み込みます。 */
    void LoadRopeModel();
    /** @brief ロープ表示用モデルとテクスチャを解放します。 */
    void ReleaseRopeModel();
    /**
     * @brief 配置済みロープの上下端に合わせてモデルを描画します。
     * @param view view に指定する処理条件または対象値です。
     * @param projection projection に指定する処理条件または対象値です。
     * @param cameraPosition 判定または処理対象の位置です。
     */
    void DrawRopeModels(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /** @brief 採掘ポイント表示用モデルを読み込み、セル内へ収めるための境界を取得します。 */
    void LoadMiningPointModel();
    /** @brief 採掘ポイント表示用モデルを解放します。 */
    void ReleaseMiningPointModel();
    /**
     * @brief 発見済みまたは近距離の採掘ポイントをモデルで描画します。
     * @param view view に指定する処理条件または対象値です。
     * @param projection projection に指定する処理条件または対象値です。
     * @param cameraPosition 判定または処理対象の位置です。
     */
    void DrawMiningPointModels(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& cameraPosition);
    /** @brief 攻撃中に表示するつるはしモデルを読み込みます。 */
    void LoadPickaxeModel();
    /** @brief つるはしモデルを解放します。 */
    void ReleasePickaxeModel();
    /** @brief 攻撃タイマーに同期してつるはしを描画します。 */
    void DrawPickaxeModel(
        const DirectX::XMFLOAT4X4& view,
        const DirectX::XMFLOAT4X4& projection,
        const DirectX::XMFLOAT3& playerCenter);
    /** @brief カメラ方向に追従する方位コンパスを画面右上へ描画します。 */
    void DrawCompass() const;
    /** @brief 探索中の右ドラッグ入力から軌道カメラの角度を更新します。 */
    void UpdateCameraControls();
    /**
     * @brief 現在のカメラの水平前方向を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 GetCameraForward() const;
    /**
     * @brief 現在のカメラの水平右方向を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 GetCameraRight() const;
    /** @brief カメラ距離とY方向オフセットを安全な範囲へ正規化します。 */
    void NormalizeCameraSettings();

    /** @brief ImGui のトップダウンフィールドを描画します。現在は補助用で、通常描画からは呼びません。 */
    void DrawField();

    /** @brief 体力、精神力、スタミナ、重量、深度、ログを描画します。 */
    void DrawHud();

    /** @brief 上昇負荷によるアイリスアウトまたは盲目をHUDの背面へ描画します。 */
    void DrawUpperLoadVisionEffect() const;
    /**
     * @brief DrawDehydrationVisionEffect が担当する表示要素を現在状態から描画します。
     * @details DrawDehydrationVisionEffect の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawDehydrationVisionEffect() const;

    /** @brief 所持品一覧と捨てる操作を描画します。 */
    void DrawInventory();
    /**
     * @brief TogglePortableLight が担当する処理を現在状態へ反映します。
     * @details TogglePortableLight の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void TogglePortableLight();
    /**
     * @brief UpdatePortableLight が担当する状態を既定の更新順で進めます。
     * @details UpdatePortableLight の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdatePortableLight(float dt);
    /**
     * @brief IsPortableLightAvailable の条件を現在状態から判定します。
     * @details IsPortableLightAvailable の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsPortableLightAvailable() const;
    /**
     * @brief GetAccessibleLights に必要な値を現在状態から算出して返します。
     * @details GetAccessibleLights の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    std::vector<PortableLight>& GetAccessibleLights();
    /**
     * @brief GetAccessibleLights に必要な値を現在状態から算出して返します。
     * @details GetAccessibleLights の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const std::vector<PortableLight>& GetAccessibleLights() const;
    /**
     * @brief ReturnCarriedLightsToStorage が担当する処理を現在状態へ反映します。
     * @details ReturnCarriedLightsToStorage の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ReturnCarriedLightsToStorage();
    /**
     * @brief BuildSceneLight が担当する処理を現在状態へ反映します。
     * @details BuildSceneLight の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    ShaderList::ExtendedLight BuildSceneLight() const;
    /**
     * @brief ApplySceneLighting が担当する処理を現在状態へ反映します。
     * @details ApplySceneLighting の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ApplySceneLighting() const;
    /**
     * @brief CalculateSceneLightIntensity に必要な値を現在状態から算出して返します。
     * @details CalculateSceneLightIntensity の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param position 判定または処理対象の位置です。
     * @param layerDepth 判定または処理対象の深度です。
     * @param worldY 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float CalculateSceneLightIntensity(const Vec2& position, float layerDepth, float worldY) const;
    /**
     * @brief CalculateSceneLightColor に必要な値を現在状態から算出して返します。
     * @details CalculateSceneLightColor の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param position 判定または処理対象の位置です。
     * @param layerDepth 判定または処理対象の深度です。
     * @param worldY 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    DirectX::XMFLOAT3 CalculateSceneLightColor(const Vec2& position, float layerDepth, float worldY) const;

    /** @brief 旧器発見時の拾う/置く確認ウィンドウを描画します。 */
    void DrawRelicPrompt();
    /**
     * @brief DrawWaterPrompt が担当する表示要素を現在状態から描画します。
     * @details DrawWaterPrompt の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawWaterPrompt();
    /**
     * @brief DrawFishingConfirm が担当する表示要素を現在状態から描画します。
     * @details DrawFishingConfirm の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawFishingConfirm();
    /**
     * @brief DrawFishingProgress が担当する表示要素を現在状態から描画します。
     * @details DrawFishingProgress の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawFishingProgress() const;

    /** @brief 帰還または死亡のリザルトウィンドウを描画します。 */
    void DrawResult();
    /**
     * @brief DrawTransitionSaveError が担当する表示要素を現在状態から描画します。
     * @details DrawTransitionSaveError の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawTransitionSaveError();

    /** @brief 帰還地点での確認ウィンドウを描画します。 */
    void DrawReturnConfirm();
    /**
     * @brief DrawAbandonConfirm が担当する表示要素を現在状態から描画します。
     * @details DrawAbandonConfirm の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawAbandonConfirm();

    /** @brief 自宅の装備変更、持ち込み品選択、潜行開始UIを描画します。 */
    void DrawHome();

    /** @brief 商店の購入・売却UIを描画します。 */
    void DrawGeneralShop();

    /** @brief 武具屋の武具購入UIを描画します。 */
    void DrawArmory();
    /**
     * @brief DrawCurrentStatus が担当する表示要素を現在状態から描画します。
     * @details DrawCurrentStatus の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawCurrentStatus();
    /**
     * @brief DrawTownNavigation が担当する表示要素を現在状態から描画します。
     * @details DrawTownNavigation の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawTownNavigation();
    /**
     * @brief DrawAbyssEntrance が担当する表示要素を現在状態から描画します。
     * @details DrawAbyssEntrance の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawAbyssEntrance();
    /**
     * @brief DrawSurfaceFacilityMarkers が担当する表示要素を現在状態から描画します。
     * @details DrawSurfaceFacilityMarkers の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawSurfaceFacilityMarkers();
    /**
     * @brief DrawTownFrame が担当する表示要素を現在状態から描画します。
     * @details DrawTownFrame の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param title 処理または表示に使用する文字列です。
     * @param drawCenterContent drawCenterContent に指定する処理条件または対象値です。
     * @param returnMode returnMode に指定する処理条件または対象値です。
     * @param returnLabel 処理または表示に使用する文字列です。
     */
    void DrawTownFrame(
        const char* title,
        const std::function<void()>& drawCenterContent,
        Mode returnMode = Mode::Surface,
        const char* returnLabel = u8"地上へ戻る");
    /**
     * @brief DrawTownLeftPane が担当する表示要素を現在状態から描画します。
     * @details DrawTownLeftPane の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawTownLeftPane();
    /**
     * @brief DrawTownRightPane が担当する表示要素を現在状態から描画します。
     * @details DrawTownRightPane の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawTownRightPane();
    /**
     * @brief DrawShopTransactionModal が担当する表示要素を現在状態から描画します。
     * @details DrawShopTransactionModal の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawShopTransactionModal();
    /**
     * @brief ExecuteShopTransaction が担当する処理を現在状態へ反映します。
     * @details ExecuteShopTransaction の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ExecuteShopTransaction();
    /**
     * @brief DrawArmoryPurchaseModal が担当する表示要素を現在状態から描画します。
     * @details DrawArmoryPurchaseModal の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawArmoryPurchaseModal();
    /**
     * @brief DrawRestaurant が担当する表示要素を現在状態から描画します。
     * @details DrawRestaurant の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawRestaurant();
    /**
     * @brief DrawQuestDesk が担当する表示要素を現在状態から描画します。
     * @details DrawQuestDesk の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawQuestDesk();
    /**
     * @brief DrawUninsuredDescentConfirm が担当する表示要素を現在状態から描画します。
     * @details DrawUninsuredDescentConfirm の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawUninsuredDescentConfirm();
    /**
     * @brief DrawRouteInfo が担当する表示要素を現在状態から描画します。
     * @details DrawRouteInfo の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawRouteInfo();

    /** @brief 所持品表示中に使う簡易地図とピン操作を描画します。 */
    void DrawMapControls();

    /** @brief runtime map と同期した常時表示ミニマップを描画します。 */
    void DrawMiniMap();

    /** @brief プレイテスト用のプレイヤー調整UIを描画します。 */
    void DrawDebugPlayerTuning();

#if defined(_DEBUG) && !defined(NARAKU_EDITOR_BUILD)
    /**
     * @brief Debug版ゲームで指定した生成計画上のエリアへ直接移動します。
     * @param depth 判定または処理対象の深度です。
     * @param sublayer sublayer に指定する処理条件または対象値です。
     * @param areaNumber areaNumber に指定する処理条件または対象値です。
     * @return 判定結果を返します。
     */
    bool DebugWarpToArea(int depth, int sublayer, int areaNumber);

    /**
     * @brief 実時間系タイマーを進めず、現在以後の指定ゲーム内日時へ進めます。
     * @param weekday weekday に指定する処理条件または対象値です。
     * @param hour hour に指定する処理条件または対象値です。
     * @param minute minute に指定する処理条件または対象値です。
     */
    void DebugAdvanceGameDate(int weekday, int hour, int minute);
#endif

    /** @brief プレイヤーの現在位置、高さ、および現在いる小ステージ名を表示するデバッグウィンドウを描画します。 */
    void DrawPlayerPositionDebug();

    /** @brief 採掘中の進行度バーを画面中央にオーバーレイ表示します。 */
    void DrawMiningProgressBar();

    /**
     * @brief DrawLoadingScreen が担当する表示要素を現在状態から描画します。
     * @details DrawLoadingScreen の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawLoadingScreen();
    /**
     * @brief DrawGenerationFailurePopup が担当する表示要素を現在状態から描画します。
     * @details DrawGenerationFailurePopup の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawGenerationFailurePopup();
    /**
     * @brief ReportGenerationFailure が担当する処理を現在状態へ反映します。
     * @details ReportGenerationFailure の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param summary 表示または記録に使用する理由・内容です。
     * @param detail 表示または記録に使用する理由・内容です。
     */
    void ReportGenerationFailure(const std::string& summary, const std::string& detail);

    /** @brief 操作不能理由などの短い通知をメイン表示領域の中央に描画します。 */
    void DrawCenterNotification();

    /**
     * @brief 現在の総重量を計算します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetCurrentWeight() const;

    /**
     * @brief 現在の装備効果を反映した重量上限を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMaxWeight() const;

    /**
     * @brief 地面から取得できる重量の上限を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetPickupWeightLimit() const;

    /**
     * @brief 装備中のつるはしによる採掘速度倍率を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMiningSpeedMultiplier() const;

    /**
     * @brief GetCurrentDepth に必要な値を現在状態から算出して返します。
     * @details GetCurrentDepth の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetCurrentDepth() const;
    /**
     * @brief GetDepthExpMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthExpMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthExpMultiplier(int depth) const;
    /**
     * @brief GetDepthMovementExpMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthMovementExpMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthMovementExpMultiplier(int depth) const;
    /**
     * @brief GetDepthRewardMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthRewardMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthRewardMultiplier(int depth) const;
    /**
     * @brief GetDepthStayRewardMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthStayRewardMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthStayRewardMultiplier(int depth) const;

    /**
     * @brief 遺物種類に対応する表示名を返します。
     * @param type 処理対象の種類です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetRelicTypeName(RelicType type) const;

    /**
     * @brief 鑑定状態を考慮した遺物表示名を返します。
     * @param item 参照または更新する対象データです。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetRelicDisplayName(const RelicItem& item) const;

    /**
     * @brief 遺物種類に対応する重量を返します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetRelicWeight(RelicType type) const;

    /**
     * @brief 遺物種類に対応する売却価格を返します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetRelicSellValue(RelicType type) const;

    /**
     * @brief 指定種類の遺物アイテムを作成します。
     * @param type 処理対象の種類です。
     * @param sourceName 処理または表示に使用する文字列です。
     * @return 現在状態と引数から算出した値を返します。
     */
    RelicItem CreateRelic(RelicType type, const std::string& sourceName);

    /**
     * @brief 7種類から均等抽選した遺物を作成します。
     * @param sourceName 処理または表示に使用する文字列です。
     * @return 現在状態と引数から算出した値を返します。
     */
    RelicItem CreateRandomRelic(const std::string& sourceName);

    /**
     * @brief GetRelicActivity に必要な値を現在状態から算出して返します。
     * @details GetRelicActivity の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param item 参照または更新する対象データです。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetRelicActivity(const RelicItem& item) const;
    /**
     * @brief GetCurrentActivity に必要な値を現在状態から算出して返します。
     * @details GetCurrentActivity の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetCurrentActivity() const;
    /**
     * @brief IsRelicSellable の条件を現在状態から判定します。
     * @details IsRelicSellable の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param item 参照または更新する対象データです。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsRelicSellable(const RelicItem& item) const;
    /**
     * @brief GetRankName に必要な値を現在状態から算出して返します。
     * @details GetRankName の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param rank rank に指定する処理条件または対象値です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetRankName(AdventurerRank rank) const;
    /**
     * @brief GetRankMaximumDepth に必要な値を現在状態から算出して返します。
     * @details GetRankMaximumDepth の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param rank rank に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetRankMaximumDepth(AdventurerRank rank) const;
    /**
     * @brief GetQuestName に必要な値を現在状態から算出して返します。
     * @details GetQuestName の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetQuestName(QuestType type) const;
    /**
     * @brief GetQuestDescription に必要な値を現在状態から算出して返します。
     * @details GetQuestDescription の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param quest 参照または更新する対象データです。
     * @return 現在状態と引数から算出した値を返します。
     */
    std::string GetQuestDescription(const QuestRecord& quest) const;
    /**
     * @brief ConsumeStoredRelicsForQuest が担当する処理を現在状態へ反映します。
     * @details ConsumeStoredRelicsForQuest の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param quest 参照または更新する対象データです。
     * @return 判定結果を返します。
     */
    bool ConsumeStoredRelicsForQuest(const QuestRecord& quest);
    /**
     * @brief ApplyQuestPenalty が担当する処理を現在状態へ反映します。
     * @details ApplyQuestPenalty の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param amount 計算または判定に使用する値です。
     */
    void ApplyQuestPenalty(int amount);
    /**
     * @brief ForceSellOneLowestValueItem が担当する処理を現在状態へ反映します。
     * @details ForceSellOneLowestValueItem の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param remainingPenalty remainingPenalty に指定する処理条件または対象値です。
     * @return 判定結果を返します。
     */
    bool ForceSellOneLowestValueItem(int& remainingPenalty);
    /**
     * @brief UseMentalRecoveryRelic が担当する処理を現在状態へ反映します。
     * @details UseMentalRecoveryRelic の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param inventoryIndex 処理対象を示す配列番号です。
     * @return 判定結果を返します。
     */
    bool UseMentalRecoveryRelic(int inventoryIndex);
    /**
     * @brief TryConsumeSurvivalRelic の成立条件を確認し、成立した処理だけを反映します。
     * @details TryConsumeSurvivalRelic の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param hpLethal hpLethal に指定する処理条件または対象値です。
     * @param mentalLethal mentalLethal に指定する処理条件または対象値です。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryConsumeSurvivalRelic(bool hpLethal, bool mentalLethal);

    /** @brief 食料を1個使い、HP20・満腹度10・水分75を回復します。 */
    void UseFood();
    /**
     * @brief UseHeatedFood が担当する処理を現在状態へ反映します。
     * @details UseHeatedFood の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UseHeatedFood();
    /**
     * @brief DrinkFromBottle が担当する処理を現在状態へ反映します。
     * @details DrinkFromBottle の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param bottleIndex 処理対象を示す配列番号です。
     */
    void DrinkFromBottle(int bottleIndex);
    /**
     * @brief DiscardBottleWater が担当する処理を現在状態へ反映します。
     * @details DiscardBottleWater の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param bottleIndex 処理対象を示す配列番号です。
     */
    void DiscardBottleWater(int bottleIndex);
    /**
     * @brief StartCooking が担当する処理を現在状態へ反映します。
     * @details StartCooking の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param target target に指定する処理条件または対象値です。
     * @param bottleIndex 処理対象を示す配列番号です。
     */
    void StartCooking(CookingTarget target, int bottleIndex = -1);
    /**
     * @brief CancelCooking の条件を現在状態から判定します。
     * @details CancelCooking の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param reason 表示または記録に使用する理由・内容です。
     */
    void CancelCooking(const char* reason);
    /**
     * @brief ConsumeCookingKitUse が担当する処理を現在状態へ反映します。
     * @details ConsumeCookingKitUse の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 判定結果を返します。
     */
    bool ConsumeCookingKitUse();
    /**
     * @brief DrinkWater が担当する処理を現在状態へ反映します。
     * @details DrinkWater の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param amount 計算または判定に使用する値です。
     * @param quality quality に指定する処理条件または対象値です。
     * @param foodPoisoningChance foodPoisoningChance に指定する処理条件または対象値です。
     */
    void DrinkWater(float amount, WaterQuality quality, float foodPoisoningChance);
    /**
     * @brief ApplyFoodPoisoning が担当する処理を現在状態へ反映します。
     * @details ApplyFoodPoisoning の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param chance chance に指定する処理条件または対象値です。
     */
    void ApplyFoodPoisoning(float chance);
    /**
     * @brief GetNearbyWaterFlags に必要な値を現在状態から算出して返します。
     * @details GetNearbyWaterFlags の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    std::uint32_t GetNearbyWaterFlags() const;
    /**
     * @brief GetWaterFoodPoisoningChance に必要な値を現在状態から算出して返します。
     * @details GetWaterFoodPoisoningChance の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetWaterFoodPoisoningChance() const;
    /**
     * @brief GetWaterQualityName に必要な値を現在状態から算出して返します。
     * @details GetWaterQualityName の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param quality quality に指定する処理条件または対象値です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetWaterQualityName(WaterQuality quality) const;

    /**
     * @brief TryUseRestaurant の成立条件を確認し、成立した処理だけを反映します。
     * @details TryUseRestaurant の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryUseRestaurant();
    /**
     * @brief TryUseSecondBaseMeal の成立条件を確認し、成立した処理だけを反映します。
     * @details TryUseSecondBaseMeal の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryUseSecondBaseMeal();
    /**
     * @brief TryUseForwardBaseMeal の成立条件を確認し、成立した処理だけを反映します。
     * @details TryUseForwardBaseMeal の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryUseForwardBaseMeal();
    /**
     * @brief DrawSecondBase が担当する表示要素を現在状態から描画します。
     * @details DrawSecondBase の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawSecondBase();
    /**
     * @brief DrawForwardBase が担当する表示要素を現在状態から描画します。
     * @details DrawForwardBase の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawForwardBase();
    /**
     * @brief DrawBaseInteractionMarker が担当する表示要素を現在状態から描画します。
     * @details DrawBaseInteractionMarker の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void DrawBaseInteractionMarker() const;
    /**
     * @brief FillSafeWater が担当する処理を現在状態へ反映します。
     * @details FillSafeWater の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param useStoredBottles 該当する動作を有効にする場合はtrueです。
     */
    void FillSafeWater(bool useStoredBottles);
    /**
     * @brief AdvanceWorldTime が担当する処理を現在状態へ反映します。
     * @details AdvanceWorldTime の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param gameSeconds 進めるゲーム内時間の秒数です。
     * @param realSeconds 対応する実時間の秒数です。
     */
    void AdvanceWorldTime(double gameSeconds, double realSeconds);
    /**
     * @brief StayAtSecondBase が担当する処理を現在状態へ反映します。
     * @details StayAtSecondBase の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void StayAtSecondBase();
    /**
     * @brief GetSecondBaseSaleMultiplier に必要な値を現在状態から算出して返します。
     * @details GetSecondBaseSaleMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param item 参照または更新する対象データです。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetSecondBaseSaleMultiplier(const RelicItem& item) const;
    /**
     * @brief GetSecondBaseSaleValue に必要な値を現在状態から算出して返します。
     * @details GetSecondBaseSaleValue の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param item 参照または更新する対象データです。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetSecondBaseSaleValue(const RelicItem& item) const;
    /**
     * @brief IsSecondBaseSellable の条件を現在状態から判定します。
     * @details IsSecondBaseSellable の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param item 参照または更新する対象データです。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsSecondBaseSellable(const RelicItem& item) const;
    /**
     * @brief UseRationOne が担当する処理を現在状態へ反映します。
     * @details UseRationOne の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UseRationOne();
    /**
     * @brief UseRawFish が担当する処理を現在状態へ反映します。
     * @details UseRawFish の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UseRawFish();
    /**
     * @brief UseCookedFish が担当する処理を現在状態へ反映します。
     * @details UseCookedFish の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void UseCookedFish();
    /**
     * @brief UseSizedFish が担当する処理を現在状態へ反映します。
     * @details UseSizedFish の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param size size に指定する処理条件または対象値です。
     * @param cooked 該当する動作を有効にする場合はtrueです。
     */
    void UseSizedFish(FishSize size, bool cooked);
    /**
     * @brief HasUnknownArmorSetEffect の条件を現在状態から判定します。
     * @details HasUnknownArmorSetEffect の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 条件を満たす場合はtrueです。
     */
    bool HasUnknownArmorSetEffect() const;
    /**
     * @brief TryConsumeFatalUpperLoadCartridge の成立条件を確認し、成立した処理だけを反映します。
     * @details TryConsumeFatalUpperLoadCartridge の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryConsumeFatalUpperLoadCartridge();
    /**
     * @brief UpdateUnknownWeaponAttack が担当する状態を既定の更新順で進めます。
     * @details UpdateUnknownWeaponAttack の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param dt 今フレームの経過秒数です。
     */
    void UpdateUnknownWeaponAttack(float dt);
    /**
     * @brief FireUnknownWeapon が担当する処理を現在状態へ反映します。
     * @details FireUnknownWeapon の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void FireUnknownWeapon();

    /**
     * @brief 装備名を返します。
     * @param tier tier に指定する処理条件または対象値です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetArmorName(ArmorTier tier) const;
    /**
     * @brief GetArmorEffectText に必要な値を現在状態から算出して返します。
     * @details GetArmorEffectText の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param tier tier に指定する処理条件または対象値です。
     * @param headSlot 該当する動作を有効にする場合はtrueです。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetArmorEffectText(ArmorTier tier, bool headSlot) const;

    /**
     * @brief 頭と胴に遺物装備を揃えているか判定します。
     * @return 条件を満たす場合はtrueです。
     */
    bool HasRelicArmorSetEffect() const;

    /**
     * @brief 武器名を返します。
     * @param tier tier に指定する処理条件または対象値です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetWeaponName(WeaponTier tier) const;
    /**
     * @brief GetWeaponEffectText に必要な値を現在状態から算出して返します。
     * @details GetWeaponEffectText の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param tier tier に指定する処理条件または対象値です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetWeaponEffectText(WeaponTier tier) const;

    /**
     * @brief 所持金と素材を確認して装備を購入します。
     * @param tier tier に指定する処理条件または対象値です。
     * @param headSlot 該当する動作を有効にする場合はtrueです。
     * @param useMaterials 該当する動作を有効にする場合はtrueです。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryBuyArmor(ArmorTier tier, bool headSlot, bool useMaterials);

    /**
     * @brief 所持金と素材を確認して武器を購入します。
     * @param tier tier に指定する処理条件または対象値です。
     * @param useMaterials 該当する動作を有効にする場合はtrueです。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryBuyWeapon(WeaponTier tier, bool useMaterials);

    /**
     * @brief CountStoredRelics に必要な値を現在状態から算出して返します。
     * @details CountStoredRelics の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int CountStoredRelics(RelicType type) const;
    /**
     * @brief RemoveStoredRelics が担当する処理を現在状態へ反映します。
     * @details RemoveStoredRelics の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @param count count に指定する処理条件または対象値です。
     * @return 判定結果を返します。
     */
    bool RemoveStoredRelics(RelicType type, int count);

    /**
     * @brief 現在の重量上限に対する現在重量の割合を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetWeightRate() const;

    /**
     * @brief 重量70%以上の速度低下を反映した歩行速度を返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMoveSpeed() const;

    /**
     * @brief 深度・レベル、重量、飢餓の各補正を反映したスタミナ消費量を返します。
     * @param baseCost 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetStaminaCost(float baseCost) const;

    /**
     * @brief GetDepthLevelStaminaConsumptionMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthLevelStaminaConsumptionMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthLevelStaminaConsumptionMultiplier() const;
    /**
     * @brief GetDepthLevelStaminaRecoveryMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthLevelStaminaRecoveryMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthLevelStaminaRecoveryMultiplier() const;
    /**
     * @brief GetDepthLevelMentalConsumptionMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthLevelMentalConsumptionMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthLevelMentalConsumptionMultiplier() const;
    /**
     * @brief GetDepthLevelFullnessConsumptionMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDepthLevelFullnessConsumptionMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDepthLevelFullnessConsumptionMultiplier() const;

    /**
     * @brief 指定した基礎消費量を各補正込みで支払えるか判定します。
     * @param baseCost 計算または判定に使用する値です。
     * @return 条件を満たす場合はtrueです。
     */
    bool CanSpendStamina(float baseCost) const;

    /**
     * @brief 指定した基礎消費量を各補正込みで実際に消費します。
     * @param baseCost 計算または判定に使用する値です。
     */
    void SpendStamina(float baseCost);

    /**
     * @brief GetEquipmentBonus に必要な値を現在状態から算出して返します。
     * @details GetEquipmentBonus の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    EquipmentBonus GetEquipmentBonus() const;
    /**
     * @brief GetLevelGrowth に必要な値を現在状態から算出して返します。
     * @details GetLevelGrowth の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetLevelGrowth() const;
    /**
     * @brief GetMaxHp に必要な値を現在状態から算出して返します。
     * @details GetMaxHp の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMaxHp() const;
    /**
     * @brief GetMaxStamina に必要な値を現在状態から算出して返します。
     * @details GetMaxStamina の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMaxStamina() const;
    /**
     * @brief GetMaxMental に必要な値を現在状態から算出して返します。
     * @details GetMaxMental の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMaxMental() const;
    /**
     * @brief GetStaminaRecoveryMultiplier に必要な値を現在状態から算出して返します。
     * @details GetStaminaRecoveryMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetStaminaRecoveryMultiplier() const;
    /**
     * @brief GetMentalRecoveryMultiplier に必要な値を現在状態から算出して返します。
     * @details GetMentalRecoveryMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetMentalRecoveryMultiplier() const;
    /**
     * @brief GetAttackPower に必要な値を現在状態から算出して返します。
     * @details GetAttackPower の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetAttackPower() const;
    /**
     * @brief GetDefenseMultiplier に必要な値を現在状態から算出して返します。
     * @details GetDefenseMultiplier の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetDefenseMultiplier() const;
    /**
     * @brief GetRunSpeed に必要な値を現在状態から算出して返します。
     * @details GetRunSpeed の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetRunSpeed() const;
    /**
     * @brief GetRopeSpeed に必要な値を現在状態から算出して返します。
     * @details GetRopeSpeed の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param ascending 該当する動作を有効にする場合はtrueです。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetRopeSpeed(bool ascending) const;
    /**
     * @brief PreserveResourceRatios が担当する処理を現在状態へ反映します。
     * @details PreserveResourceRatios の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param oldMaxHp oldMaxHp に指定する処理条件または対象値です。
     * @param oldMaxStamina oldMaxStamina に指定する処理条件または対象値です。
     * @param oldMaxMental oldMaxMental に指定する処理条件または対象値です。
     */
    void PreserveResourceRatios(float oldMaxHp, float oldMaxStamina, float oldMaxMental);
    /**
     * @brief GetRequiredExp に必要な値を現在状態から算出して返します。
     * @details GetRequiredExp の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param level level に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetRequiredExp(int level) const;
    /**
     * @brief AwardExp が担当する処理を現在状態へ反映します。
     * @details AwardExp の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param amount 計算または判定に使用する値です。
     */
    void AwardExp(int amount);
    /**
     * @brief FormatExp に必要な値を現在状態から算出して返します。
     * @details FormatExp の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param value value に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    std::string FormatExp(std::int64_t value) const;
    /**
     * @brief ApplyPlayerDamage が担当する処理を現在状態へ反映します。
     * @details ApplyPlayerDamage の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param damage 計算または判定に使用する値です。
     * @param cause cause に指定する処理条件または対象値です。
     * @param reason 表示または記録に使用する理由・内容です。
     */
    void ApplyPlayerDamage(float damage, DeathCause cause, const char* reason);
    /**
     * @brief ApplyMentalDamage が担当する処理を現在状態へ反映します。
     * @details ApplyMentalDamage の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param damage 計算または判定に使用する値です。
     * @param cause cause に指定する処理条件または対象値です。
     * @param reason 表示または記録に使用する理由・内容です。
     */
    void ApplyMentalDamage(float damage, DeathCause cause, const char* reason);
    /**
     * @brief ApplyDeathPenalty が担当する処理を現在状態へ反映します。
     * @details ApplyDeathPenalty の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param cause cause に指定する処理条件または対象値です。
     */
    void ApplyDeathPenalty(DeathCause cause);
    /**
     * @brief ApplyAbandonPenalty が担当する処理を現在状態へ反映します。
     * @details ApplyAbandonPenalty の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ApplyAbandonPenalty();
    /**
     * @brief GetDeathLevelLoss に必要な値を現在状態から算出して返します。
     * @details GetDeathLevelLoss の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param cause cause に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int GetDeathLevelLoss(DeathCause cause) const;

    /**
     * @brief 2点が指定距離以内かどうかを判定します。
     * @param a 計算または判定に使用するベクトルです。
     * @param b 計算または判定に使用するベクトルです。
     * @param range 計算または判定に使用する値です。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsNear(const Vec2& a, const Vec2& b, float range) const;

    /**
     * @brief ベクトルを長さ1に正規化します。ゼロ長ならゼロベクトルを返します。
     * @param value value に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 Normalize(const Vec2& value) const;

    /**
     * @brief 2点間距離を返します。
     * @param a 計算または判定に使用するベクトルです。
     * @param b 計算または判定に使用するベクトルです。
     * @return 現在状態と引数から算出した値を返します。
     */
    float Distance(const Vec2& a, const Vec2& b) const;

    /**
     * @brief 2つのベクトルの内積を返します。
     * @param a 計算または判定に使用するベクトルです。
     * @param b 計算または判定に使用するベクトルです。
     * @return 現在状態と引数から算出した値を返します。
     */
    float Dot(const Vec2& a, const Vec2& b) const;

    /**
     * @brief 2つのベクトルを加算します。
     * @param a 計算または判定に使用するベクトルです。
     * @param b 計算または判定に使用するベクトルです。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 Add(const Vec2& a, const Vec2& b) const;

    /**
     * @brief 2つのベクトルを減算します。
     * @param a 計算または判定に使用するベクトルです。
     * @param b 計算または判定に使用するベクトルです。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 Sub(const Vec2& a, const Vec2& b) const;

    /**
     * @brief ベクトルにスカラーを掛けます。
     * @param a 計算または判定に使用するベクトルです。
     * @param scalar 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 Mul(const Vec2& a, float scalar) const;

    /**
     * @brief HUDに表示する短いログを追加します。
     * @param message 処理または表示に使用する文字列です。
     */
    void AddMessage(const std::string& message);

    /**
     * @brief メイン表示領域中央へ一定時間表示する通知を設定します。
     * @param message 処理または表示に使用する文字列です。
     */
    void ShowCenterNotification(const std::string& message);

    /**
     * @brief SaveCurrentAreaState の対象状態を既存形式で永続化します。
     * @details SaveCurrentAreaState の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void SaveCurrentAreaState();
    /**
     * @brief ActivateArea が担当する処理を現在状態へ反映します。
     * @details ActivateArea の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param areaIndex 処理対象を示す配列番号です。
     * @param placeAtEntry 該当する動作を有効にする場合はtrueです。
     */
    void ActivateArea(int areaIndex, bool placeAtEntry);
    /**
     * @brief BuildCurrentAreaRuntime が担当する処理を現在状態へ反映します。
     * @details BuildCurrentAreaRuntime の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param placeAtStart 該当する動作を有効にする場合はtrueです。
     */
    void BuildCurrentAreaRuntime(bool placeAtStart);
#if defined(NARAKU_EDITOR_BUILD)
    /**
     * @brief FocusEditorOverviewOnCurrentArea が担当する処理を現在状態へ反映します。
     * @details FocusEditorOverviewOnCurrentArea の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void FocusEditorOverviewOnCurrentArea();
#endif
    /**
     * @brief BuildDiveStructure が担当する処理を現在状態へ反映します。
     * @details BuildDiveStructure の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool BuildDiveStructure();
    /**
     * @brief GeneratePlannedArea が担当するデータを既存ルールに従って生成します。
     * @details GeneratePlannedArea の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param areaIndex 処理対象を示す配列番号です。
     * @param outError 失敗理由を書き込む文字列です。
     * @return 処理に成功した場合はtrueです。
     */
    bool GeneratePlannedArea(int areaIndex, std::string& outError);
    /**
     * @brief AssignPlannedGates が担当する処理を現在状態へ反映します。
     * @details AssignPlannedGates の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param areaIndex 処理対象を示す配列番号です。
     * @return 処理に成功した場合はtrueです。
     */
    bool AssignPlannedGates(int areaIndex);
    /**
     * @brief GetSublayerName に必要な値を現在状態から算出して返します。
     * @details GetSublayerName の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param sublayer sublayer に指定する処理条件または対象値です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const char* GetSublayerName(int sublayer) const;
    /**
     * @brief TryUseLayerGate の成立条件を確認し、成立した処理だけを反映します。
     * @details TryUseLayerGate の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param gateIndex 処理対象を示す配列番号です。
     */
    void TryUseLayerGate(int gateIndex);
    /**
     * @brief BeginLayerTransition が担当する処理を現在状態へ反映します。
     * @details BeginLayerTransition の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param sourceGateIndex 処理対象を示す配列番号です。
     * @param destinationAreaIndex 処理対象を示す配列番号です。
     */
    void BeginLayerTransition(int sourceGateIndex, int destinationAreaIndex);
    /**
     * @brief LoadDebugPlayerParams の入力元を読み込み、検証済みの値を状態へ反映します。
     * @details LoadDebugPlayerParams の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool LoadDebugPlayerParams();
    /**
     * @brief SaveDebugPlayerParams の対象状態を既存形式で永続化します。
     * @details SaveDebugPlayerParams の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool SaveDebugPlayerParams() const;
    /**
     * @brief SaveProgress の対象状態を既存形式で永続化します。
     * @details SaveProgress の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool SaveProgress();
    /**
     * @brief CommitModeAfterSave の対象状態を既存形式で永続化します。
     * @details CommitModeAfterSave の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param targetMode targetMode に指定する処理条件または対象値です。
     * @param sourceScene sourceScene に指定する処理条件または対象値です。
     * @return 処理に成功した場合はtrueです。
     */
    bool CommitModeAfterSave(Mode targetMode, PresentationScene sourceScene);
    /**
     * @brief LoadProgress の入力元を読み込み、検証済みの値を状態へ反映します。
     * @details LoadProgress の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理に成功した場合はtrueです。
     */
    bool LoadProgress();

    /** @brief セーブファイルから読み取った単一値と複数行レコードを分類して保持します。 */
    struct ProgressLoadData
    {
        std::map<std::string, std::string> values;
        std::vector<std::string> relicLines;
        std::vector<std::string> deathRecoveryRelicLines;
        std::vector<std::string> bottleLines;
        std::vector<std::string> deathRecoveryBottleLines;
        std::vector<std::string> cookingKitLines;
        std::vector<std::string> portableLightLines;
        std::vector<std::string> deathRecoveryCookingKitLines;
        std::vector<std::string> questLines;
        std::vector<std::string> importantQuestLines;
        std::vector<std::string> promotionQuestLines;
        std::vector<std::string> surfacePinLines;
    };

    /**
     * @brief セーブファイルを読み込み、キー値と複数行レコードへ分類します。
     * @param outData 読み取ったデータを書き込む出力先です。
     * @return ファイルを開き、正しいマジック文字列を確認できた場合はtrueです。
     */
    bool ReadProgressLoadData(ProgressLoadData& outData) const;

    /**
     * @brief セーブバージョンと、そのバージョンに必須のキーが揃っているか検証します。
     * @param data 検証する分類済みセーブデータです。
     * @param outVersion 検証に成功したセーブバージョンを書き込む出力先です。
     * @return 対応バージョンで必須項目が揃っている場合はtrueです。
     */
    bool ValidateProgressLoadData(ProgressLoadData& data, int& outVersion) const;

    /**
     * @brief 所持金、レベル、装備、週状態、死亡回収などの単一値をシーン状態へ反映します。
     * @param data 分類済みセーブデータです。
     * @param version 読込対象のセーブバージョンです。
     */
    void ApplyProgressScalarValues(ProgressLoadData& data, int version);

    /**
     * @brief 配列値、地上ピン、遺物、容器、調理器具、携帯照明を復元して件数を検証します。
     * @param data 分類済みセーブデータです。
     * @param version 読込対象のセーブバージョンです。
     */
    void ApplyProgressCollections(ProgressLoadData& data, int version);

    /**
     * @brief 通常依頼、重要依頼、昇格依頼を復元して列挙値と件数を検証します。
     * @param data 分類済みセーブデータです。
     * @param version 読込対象のセーブバージョンです。
     */
    void ApplyProgressQuests(ProgressLoadData& data, int version);
    /**
     * @brief InitializeNewProgress が担当する状態を既定値へ初期化します。
     * @details InitializeNewProgress の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void InitializeNewProgress();
    /**
     * @brief SpawnEnemiesForCurrentArea が担当する処理を現在状態へ反映します。
     * @details SpawnEnemiesForCurrentArea の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void SpawnEnemiesForCurrentArea();
    /**
     * @brief RespawnEnemy が担当する処理を現在状態へ反映します。
     * @details RespawnEnemy の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param enemy 参照または更新する対象データです。
     * @return 処理に成功した場合はtrueです。
     */
    bool RespawnEnemy(EnemyState& enemy);
    /**
     * @brief CreateEnemy が担当するデータを既存ルールに従って生成します。
     * @details CreateEnemy の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param type 処理対象の種類です。
     * @param depth 判定または処理対象の深度です。
     * @param position 判定または処理対象の位置です。
     * @return 現在状態と引数から算出した値を返します。
     */
    EnemyState CreateEnemy(EnemyType type, int depth, const Vec2& position) const;
    /**
     * @brief FindEnemySpawnPoint に必要な値を現在状態から算出して返します。
     * @details FindEnemySpawnPoint の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param minimumPlayerDistance minimumPlayerDistance に指定する処理条件または対象値です。
     * @param requireTerritory 該当する動作を有効にする場合はtrueです。
     * @param found found に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 FindEnemySpawnPoint(float minimumPlayerDistance, bool requireTerritory, bool* found) const;
    /**
     * @brief HasTerritoryTreeDensity の条件を現在状態から判定します。
     * @details HasTerritoryTreeDensity の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param position 判定または処理対象の位置です。
     * @return 条件を満たす場合はtrueです。
     */
    bool HasTerritoryTreeDensity(const Vec2& position) const;
    /**
     * @brief AwardEnemyDefeat が担当する処理を現在状態へ反映します。
     * @details AwardEnemyDefeat の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @param enemy 参照または更新する対象データです。
     */
    void AwardEnemyDefeat(const EnemyState& enemy);
    /**
     * @brief CalculateReturnReward に必要な値を現在状態から算出して返します。
     * @details CalculateReturnReward の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 現在状態と引数から算出した値を返します。
     */
    int CalculateReturnReward() const;
    /**
     * @brief ActivateMiningSense が担当する処理を現在状態へ反映します。
     * @details ActivateMiningSense の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ActivateMiningSense();
    /**
     * @brief ActivateUpperLoadWard が担当する処理を現在状態へ反映します。
     * @details ActivateUpperLoadWard の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     */
    void ActivateUpperLoadWard();
    /**
     * @brief TryPreventUpperLoad の成立条件を確認し、成立した処理だけを反映します。
     * @details TryPreventUpperLoad の責務を呼び出し元から分離し、既存の判定順と状態反映順を維持します。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryPreventUpperLoad();

    /** @brief プレイヤーの近くにある未発見採掘ポイントを発見済みにします。 */
    void DiscoverNearbyMiningPoints();

    /**
     * @brief 所持品の指定旧器を現在位置に捨てます。
     * @param index 処理対象を示す配列番号です。
     */
    void DropInventoryItem(int index);

    /**
     * @brief 指定位置にピンを置くか、近くの既存ピンを削除します。
     * @param worldPos 判定または処理対象の位置です。
     */
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
     * @param zoom 計算または判定に使用する値です。
     */
    MapCanvasTransform BuildMapCanvasTransform(const Vec2& canvasPos, const Vec2& canvasSize, float padding = 0.0f, float zoom = 1.0f) const;

    /**
     * @brief ImGui上の座標をフィールド座標へ変換します。
     * @param canvasPos 描画キャンバス左上の座標です。
     * @param canvasSize 描画キャンバスの大きさです。
     * @param mousePos マウスの画面座標です。
     * @param zoom 計算または判定に使用する値です。
     * @param focusPos focusPos に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos, float zoom = 1.0f, const Vec2& focusPos = { 0.0f, 0.0f }) const;

    /**
     * @brief フィールド座標を地図用の真上視点ImGui座標へ変換します。
     * @param canvasPos 描画キャンバス左上の座標です。
     * @param canvasSize 描画キャンバスの大きさです。
     * @param worldPos 判定または処理対象の位置です。
     * @param zoom 計算または判定に使用する値です。
     * @param focusPos focusPos に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float zoom = 1.0f, const Vec2& focusPos = { 0.0f, 0.0f }) const;

    /**
     * @brief フィールド座標をゲーム画面用の斜め見下ろしImGui座標へ変換します。
     * @param canvasPos 描画キャンバス左上の座標です。
     * @param canvasSize 描画キャンバスの大きさです。
     * @param worldPos 判定または処理対象の位置です。
     * @param depthOffset 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 WorldToObliqueCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float depthOffset = 0.0f) const;

    /**
     * @brief 指定した深度に対応するレイヤー配列番号を返します。見つからなければ -1 を返します。
     * @param depth 判定または処理対象の深度です。
     * @param tolerance 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int FindLayerIndexByDepth(float depth, float tolerance = 0.20f) const;

    /**
     * @brief レイヤー上の任意XZ座標が属するセルとセル内補間率を返します。
     * @param layer 参照または更新する対象データです。
     * @param pos 判定または処理対象の位置です。
     * @param outCellX 算出した結果を書き込む出力先です。
     * @param outCellZ 算出した結果を書き込む出力先です。
     * @param outFracX 算出した結果を書き込む出力先です。
     * @param outFracZ 算出した結果を書き込む出力先です。
     * @return 処理を成立させた場合はtrueです。
     */
    bool TryGetLayerCellAt(const NarakuMap::TerrainLayer& layer, const Vec2& pos, int& outCellX, int& outCellZ, float& outFracX, float& outFracZ) const;

    /**
     * @brief レイヤー上の任意XZ座標から地形の相対高さを補間して返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float SampleTerrainHeightOffsetAt(const Vec2& pos, float depth) const;

    /**
     * @brief 指定した位置と深度における地面の絶対ワールド高さを返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetGroundWorldY(const Vec2& pos, float depth) const;

    /**
     * @brief 現在のプレイヤーのジャンプ高さを考慮した追加オフセットを返します。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetPlayerAirborneOffset() const;

    /**
     * @brief 描画と昇降で共用するロープ両端の座標を返します。
     * @param rope 参照または更新する対象データです。
     * @return 現在状態と引数から算出した値を返します。
     */
    RopeTraversalEndpoints GetRopeTraversalEndpoints(const RopePoint& rope) const;

    /**
     * @brief ロープ番号と補間率から、ロープ上の平面位置を返します。
     * @param ropeIndex 処理対象を示す配列番号です。
     * @param progress 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 GetRopePosition(int ropeIndex, float progress) const;

    /**
     * @brief ロープ番号と補間率から、ロープ上の絶対ワールド高さを返します。
     * @param ropeIndex 処理対象を示す配列番号です。
     * @param progress 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetRopeWorldY(int ropeIndex, float progress) const;

    /**
     * @brief ロープの把持位置から、ぶら下がっているプレイヤーの足元高さを返します。
     * @param ropeIndex 処理対象を示す配列番号です。
     * @param progress 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetRopePlayerFeetWorldY(int ropeIndex, float progress) const;

    /**
     * @brief 下端側から掴む際に足元を地面より少し上へ置く進捗を返します。
     * @param ropeIndex 処理対象を示す配列番号です。
     * @return 現在状態と引数から算出した値を返します。
     */
    float GetBottomRopeGrabProgress(int ropeIndex) const;

    /**
     * @brief レイヤー頂点を地形高さ込みの3D座標へ変換します。
     * @param layer 参照または更新する対象データです。
     * @param gridX gridX に指定する処理条件または対象値です。
     * @param gridZ gridZ に指定する処理条件または対象値です。
     * @param heightOffset 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    DirectX::XMFLOAT3 GetTerrainVertexWorld3D(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ, float heightOffset = 0.0f) const;
    /**
     * @brief 周囲の高さから隣接セル間で共有する滑らかな地形頂点法線を返します。
     * @param layer 参照または更新する対象データです。
     * @param gridX gridX に指定する処理条件または対象値です。
     * @param gridZ gridZ に指定する処理条件または対象値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    DirectX::XMFLOAT3 GetTerrainVertexNormal(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const;

    /**
     * @brief 2Dフィールド座標と深度を3D座標へ変換します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @param heightOffset 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    DirectX::XMFLOAT3 ToWorld3D(const Vec2& pos, float depth = 0.0f, float heightOffset = 0.0f) const;

    /**
     * @brief 指定位置、サイズ、回転でデバッグ箱を描画します。
     * @param pos 判定または処理対象の位置です。
     * @param scale scale に指定する処理条件または対象値です。
     * @param yawRad yawRad に指定する処理条件または対象値です。
     */
    void DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale, float yawRad = 0.0f) const;

    /**
     * @brief 指定位置とサイズでデバッグ球を描画します。
     * @param pos 判定または処理対象の位置です。
     * @param radius 計算または判定に使用する値です。
     */
    void DrawDebugSphere3D(const DirectX::XMFLOAT3& pos, float radius) const;

    /**
     * @brief 指定座標が床矩形内に入っているかを返します。
     * @param floor 参照または更新する対象データです。
     * @param pos 判定または処理対象の位置です。
     * @return 条件を満たす場合はtrueです。
     */
    bool IsInsideFloor(const FloorRegion& floor, const Vec2& pos) const;

    /**
     * @brief 指定座標と深度に対応する床を返します。見つからなければ nullptr を返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 条件に一致する対象を返します。見つからない場合の扱いは各呼び出し規約に従います。
     */
    const FloorRegion* FindFloorAt(const Vec2& pos, float depth) const;

    /**
     * @brief 指定座標と深度に歩ける床があるかを返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 条件を満たす場合はtrueです。
     */
    bool HasFloorAt(const Vec2& pos, float depth) const;

    /**
     * @brief 指定位置でプレイヤーがロープから降りて立てるかを返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 条件を満たす場合はtrueです。
     */
    bool CanStandAt(const Vec2& pos, float depth) const;

    /**
     * @brief 指定座標と深度に対応するセル属性フラグを返します。範囲外なら CellAttributeNone を返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    std::uint32_t GetCellAttributeFlagsAt(const Vec2& pos, float depth) const;

    /**
     * @brief 深度と位置の両方に一致するレイヤー配列番号を返します。見つからなければ -1 を返します。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @param tolerance 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int FindLayerIndexAt(const Vec2& pos, float depth, float tolerance = 0.20f) const;

    /**
     * @brief 2点間の地形高低差と属性を見て、その移動を通してよいかを返します。
     * @param from 移動開始位置です。
     * @param to 移動先の候補位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 条件を満たす場合はtrueです。
     */
    bool CanTraverseGround(
        const Vec2& from,
        const Vec2& to,
        float depth,
        float characterRadius = 0.30f,
        float characterHeight = 1.40f) const;

    /**
     * @brief 空中移動として、歩行不可セルを許可しつつ削除セルと地形外への侵入を拒否します。
     * @param to 移動先の候補位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 条件を満たす場合はtrueです。
     */
    bool CanTraverseAir(
        const Vec2& to,
        float depth,
        float characterRadius = 0.30f,
        float characterHeight = 1.40f) const;

    /**
     * @brief 床外へ出る移動を止め、可能ならX方向またはY方向だけの移動に分解して通します。
     * @param from 移動開始位置です。
     * @param to 移動先の候補位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 ResolveFloorMove(
        const Vec2& from,
        const Vec2& to,
        float depth,
        float characterRadius = 0.30f,
        float characterHeight = 1.40f) const;

    /**
     * @brief 空中移動を分割し、削除セルと地形外へ入る直前で止めます。
     * @param from 移動開始位置です。
     * @param to 移動先の候補位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 ResolveAirMove(const Vec2& from, const Vec2& to, float depth) const;

    /** @brief 登録された有限高AABBとキャラクター円柱が重なるか返します。 */
    bool IntersectsEnvironmentCollider(
        const Vec2& position,
        float feetWorldY,
        float characterRadius,
        float characterHeight) const;

    /** @brief 環境コライダーへの上下衝突を解決します。 */
    void ResolveEnvironmentVerticalCollision(
        Vec2& position,
        float previousFeetWorldY,
        float& feetWorldY,
        float& verticalSpeed,
        float characterRadius,
        float characterHeight) const;

    /**
     * @brief 現在セルの四隅から最も低くなる方向を返します。平坦ならゼロ方向です。
     * @param pos 判定または処理対象の位置です。
     * @param depth 判定または処理対象の深度です。
     * @return 現在状態と引数から算出した値を返します。
     */
    Vec2 GetTerrainDownhillDirection(const Vec2& pos, float depth) const;

    /**
     * @brief 最後に記録した歩行可能地点へプレイヤーを戻します。
     * @return 処理に成功した場合はtrueです。
     */
    bool RestorePlayerToSafeGround();

    /**
     * @brief プレイヤー付近のロープ番号を返します。近くにない場合は -1 を返します。
     * @param range 計算または判定に使用する値です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int FindNearestRopeIndex(float range) const;

    /**
     * @brief 落下中のプレイヤーから指定半径内にあるロープ線分と、その最近接位置を返します。
     * @param radius 計算または判定に使用する値です。
     * @param outProgress 算出した結果を書き込む出力先です。
     * @return 現在状態と引数から算出した値を返します。
     */
    int FindFallingRopeIndex(float radius, float& outProgress) const;

    /**
     * @brief 指定したロープから横へ降りられる床があればロープを離します。
     * @param ropeIndex 処理対象を示す配列番号です。
     * @param leaveSign leaveSign に指定する処理条件または対象値です。
     * @param cameraRight 計算または判定に使用するベクトルです。
     * @return 処理を成立させた場合はtrueです。
     */
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

    /** @brief 死亡結果で回収または破棄を選ぶまで退避する遺物です。 */
    std::vector<RelicItem> m_pendingDeathRecoveryRelics;

    /** @brief フィールド上に置かれている旧器一覧です。 */
    std::vector<GroundRelic> m_groundRelics;

    /** @brief 敵が落とした拾得可能な食料一覧です。 */
    std::vector<GroundFood> m_groundFoods;

    /** @brief 第一層プロトタイプ用の採掘ポイント一覧です。 */
    std::vector<MiningPoint> m_miningPoints;
    std::vector<FishingPoint> m_fishingPoints;

    /** @brief 第一層プロトタイプ用の敵一覧です。 */
    std::vector<EnemyState> m_enemies;

    /** @brief 再生中のプレイヤー攻撃命中エフェクト一覧です。 */
    std::vector<AttackHitEffect> m_attackHitEffects;

    /** @brief 再生中のジャンプ開始エフェクト一覧です。 */
    std::vector<JumpEffect> m_jumpEffects;

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

    /** @brief 同じゲーム内週で潜行をまたいで維持するエリア状態です。 */
    std::vector<AreaState> m_weeklyAreas;

    /** @brief 週境界後、次回潜行から新しい奈落へ切り替える必要があるかどうかです。 */
    bool m_weekResetPending = false;

    /** @brief 現在潜行中の構成と採掘抽選に使用している週シードです。 */
    std::uint64_t m_diveWorldSeed = 1;

    /** @brief 現在表示しているエリア番号です。 */
    int m_currentAreaIndex = -1;

    /** @brief 現在つかまっているロープ番号です。未使用時は -1 です。 */
    int m_activeRope = -1;

    /** @brief 現在つかまっているロープの上端0、下端1の補間率です。 */
    float m_ropeProgress = 0.0f;

    /** @brief プレイヤーが置いた地図ピン一覧です。 */
    std::vector<Vec2> m_pins;
    std::vector<Vec2> m_surfacePins;
    std::vector<SurfaceFacilityState> m_surfaceFacilities;

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

    /** @brief ロード画面へ表示する現在の生成工程です。 */
    std::string m_loadingStatus;

    /** @brief 直近の生成失敗の概要と生成器から返された詳細です。 */
    std::string m_generationFailureSummary;
    std::string m_generationFailureDetail;
    bool m_openGenerationFailurePopup = false;

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

    ScreenFadePhase m_screenFadePhase = ScreenFadePhase::None;
    ScreenFadeAction m_screenFadeAction = ScreenFadeAction::None;
    float m_screenFadeAlpha = 0.0f;

    /** @brief 重量超過中の走行通知をキー押下ごとに一度だけ出すための状態です。 */
    bool m_heavyRunNotificationShown = false;

    /** @brief 現在のシーン内モードです。 */
    Mode m_mode = Mode::Explore;

    /** @brief 統合メニューのタブ種別です。 */
    enum class MenuTab
    {
        Map,
        Inventory,
        Settings
    };
    MenuTab m_activeMenuTab = MenuTab::Map;
    SceneNarakuInputSettings m_inputSettings;

    /** @brief E/Iキー画面で地図タブを表示しているかどうかです（旧コード互換用）。 */
    bool m_inventoryMapShowingMap = false;
    Mode m_overlayReturnMode = Mode::Explore;

    /** @brief 現在の潜行結果です。帰還/死亡リザルトで表示します。 */
    RunResult m_result;

    /** @brief 保存再試行成功後に確定するモードです。 */
    Mode m_pendingModeAfterSave = Mode::Home;
    /** @brief 保存失敗中にSceneManagerへ維持させる表示Sceneです。 */
    PresentationScene m_saveErrorPresentation = PresentationScene::Town;

    /** @brief 発見確認中の旧器です。 */
    RelicItem m_pendingRelic;

    /** @brief 発見確認中の旧器を置く場合の位置です。 */
    Vec2 m_pendingRelicPos;

    /** @brief 発見確認中の旧器を置く場合の深度です。 */
    float m_pendingRelicDepth = 0.0f;

    /** @brief 発見確認中の遺物を生成した採掘地点番号です。 */
    int m_pendingRelicMiningIndex = -1;

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
    AdventurerRank m_adventurerRank = AdventurerRank::Red;
    int m_maxReachedDepth = 1;
    int m_questDebt = 0;
    bool m_deathRecoveryPending = false;
    bool m_deathRecoveryDiscardConfirm = false;
    int m_deathRecoveryDepth = 0;
    int m_deathRecoveryFee = 0;
    int m_pendingDeathRecoveryFood = 0;
    int m_pendingDeathRecoveryHeatedFood = 0;
    int m_pendingDeathRecoveryRationOne = 0;
    int m_pendingDeathRecoveryRawFish = 0;
    int m_pendingDeathRecoveryCookedFish = 0;
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_pendingDeathRecoveryRawSizedFish = {};
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_pendingDeathRecoveryCookedSizedFish = {};
    int m_pendingDeathRecoveryCartridges = 0;
    std::vector<WaterBottle> m_pendingDeathRecoveryBottles;
    std::vector<CookingKit> m_pendingDeathRecoveryCookingKits;
    std::string m_pendingDeathReason;
    int m_pendingDeathLevelBefore = 1;
    int m_pendingDeathLevelAfter = 1;
    int m_pendingDeathProtectionConsumed = 0;
    double m_gameWeekSeconds = 0.0;
    std::uint64_t m_weekSeed = 1;
    std::uint64_t m_nextQuestId = 1;
    std::vector<QuestRecord> m_quests;
    int m_selectedQuest = 0;
    std::array<ImportantQuestRecord, static_cast<std::size_t>(ImportantQuestType::Count)> m_importantQuests = {};
    int m_selectedImportantQuest = 0;
    std::array<PromotionQuestRecord, 3> m_promotionQuests = {};
    int m_selectedPromotionQuest = 0;
    std::array<int, 2> m_directGateWeeklyUses = {};
    std::array<int, 2> m_directGateTargetAreas = { -1, -1 };
    std::array<Vec2, 2> m_directGateTargetPositions = {};
    int m_pendingDiveDepth = 1;
    bool m_surfaceWasMoving = false;

    /** @brief 今回の潜行で新規取得した遺物の取得親深度です。 */
    std::unordered_map<std::uint64_t, int> m_runRelicAcquisitionDepths;
    bool m_uninsuredDescentAcceptedThisDive = false;
    int m_pendingUninsuredGateIndex = -1;

    int m_level = 1;
    int m_currentExp = 0;
    int m_levelProtection = 0;
    std::int64_t m_level100OverflowExp = 0;
    float m_fullness = 75.0f;
    float m_hydration = 70.0f;
    float m_dehydrationZeroTimer = 0.0f;
    float m_dehydrationVisionStrength = 0.0f;
    bool m_hydrationWasZero = false;
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
    float m_upperLoadVisionTimer = 0.0f;
    float m_upperLoadVisionOcclusion = 0.0f;
    float m_upperLoadFifthTimer = 0.0f;
    float m_upperLoadFifthDamageRatio = 0.0f;
    float m_upperLoadFifthDamageCooldown = 0.0f;
    float m_miningSenseTimer = 0.0f;
    DeathCause m_pendingDeathCause = DeathCause::Other;
    bool m_attackRelicTriggered = false;
    float m_foodUseTimer = 0.0f;
    bool m_usingHeatedFood = false;
    float m_rationFullnessWardTimer = 0.0f;
    float m_rationHydrationPenaltyTimer = 0.0f;
    float m_unknownWeaponChargeTimer = 0.0f;
    float m_unknownWeaponCooldownTimer = 0.0f;
    bool m_unknownWeaponFiredThisHold = false;
    float m_cookingTimer = 0.0f;
    float m_cookingPreviousHp = 0.0f;
    CookingTarget m_cookingTarget = CookingTarget::None;
    int m_cookingBottleIndex = -1;
    int m_cookingFishSize = -1;
    enum class FishingPhase { None, Waiting, Bite, Landing };
    FishingPhase m_fishingPhase = FishingPhase::None;
    int m_fishingPointIndex = -1;
    float m_fishingTimer = 0.0f;
    float m_fishingPreviousHp = 0.0f;
    std::uint32_t m_pendingWaterFlags = NarakuMap::CellAttributeNone;
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

    int m_heatedFoodCount = 0;
    int m_storedHeatedFoodCount = 0;
    int m_loadoutHeatedFoodCount = 0;
    int m_rationOneCount = 0;
    int m_storedRationOneCount = 0;
    int m_loadoutRationOneCount = 0;
    int m_rawFishCount = 0;
    int m_storedRawFishCount = 0;
    int m_loadoutRawFishCount = 0;
    int m_cookedFishCount = 0;
    int m_storedCookedFishCount = 0;
    int m_loadoutCookedFishCount = 0;
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_rawSizedFish = {};
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_storedRawSizedFish = {};
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_loadoutRawSizedFish = {};
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_cookedSizedFish = {};
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_storedCookedSizedFish = {};
    std::array<int, static_cast<std::size_t>(FishSize::Count)> m_loadoutCookedSizedFish = {};
    int m_cartridgeCount = 0;
    int m_storedCartridgeCount = 0;
    int m_loadoutCartridgeCount = 0;
    std::vector<WaterBottle> m_waterBottles;
    std::vector<WaterBottle> m_storedWaterBottles;
    std::vector<CookingKit> m_cookingKits;
    std::vector<CookingKit> m_storedCookingKits;
    std::vector<PortableLight> m_portableLights;
    std::vector<PortableLight> m_storedPortableLights;
    bool m_portableLightOn = false;

    /** @brief 自宅に保管している鑑定済み遺物数です。 */
    /** @brief 次回潜行へ持ち込む鑑定済み遺物数です。 */
    std::array<int, static_cast<std::size_t>(RelicType::Count)> m_loadoutRelics = {};

    /** @brief 一度でも鑑定して正体を記憶した遺物種類です。 */
    std::array<bool, static_cast<std::size_t>(RelicType::Count)> m_identifiedRelics = {};

    /** @brief セーブ読込時に確認した発行Mapのバージョンです。 */
    std::string m_loadedMapVersion;

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

    enum class ShopTransactionMode
    {
        None,
        BuyFood,
        BuyWaterBottle,
        BuyCookingKit,
        BuyPortableLight,
        BuyRelic,
        SellRelic,
        SellPortableLight,
        SellBrokenPortableLight
    };

    struct ShopTransactionState
    {
        bool openModal = false;
        ShopTransactionMode mode = ShopTransactionMode::None;
        RelicType relicType = RelicType::CashLow;
        std::string itemName;
        int unitPrice = 0;
        int count = 1;
        int maxCount = 1;
    };
    ShopTransactionState m_shopTransaction;

    struct ArmoryUIState
    {
        int selectedArmorIndex = 0;
        int selectedWeaponIndex = 0;
        bool openPurchaseModal = false;
        bool modalIsWeapon = false;
        int modalArmorTier = 0;
        int modalWeaponTier = 0;
        int modalArmorSlot = 0; // 0: 頭, 1: 胴
    };
    ArmoryUIState m_armoryUI;

    /** @brief 半透明床バッチの1頂点です。現在の床色を維持するためRGBAを頂点へ持たせます。 */
    struct TerrainFloorVertex
    {
        DirectX::XMFLOAT3 position = {};
        DirectX::XMFLOAT3 normal = { 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT2 uv = {};
        DirectX::XMFLOAT4 color = {};
    };

    static constexpr std::size_t TerrainTextureCount = 4;
    static constexpr std::size_t TerrainBatchCount = TerrainTextureCount + 1;
    /** @brief 地形4種と補助表示をテクスチャ別にまとめて送る動的頂点バッファです。 */
    std::array<MeshBuffer*, TerrainBatchCount> m_terrainFloorMeshes = {};
    /** @brief 半透明床のワールド座標と頂点色を変換する頂点シェーダーです。 */
    VertexShader* m_terrainFloorVS = nullptr;
    /** @brief 半透明床の頂点色をそのまま出力するピクセルシェーダーです。 */
    PixelShader* m_terrainFloorPS = nullptr;
    /** @brief 草地、土、岩、湿地と補助表示に使うテクスチャです。 */
    std::array<Texture*, TerrainBatchCount> m_terrainFloorTextures = {};
    /** @brief エリア内の最大床頂点数を確保し、フレーム間で再利用するCPU側配列です。 */
    std::array<std::vector<TerrainFloorVertex>, TerrainBatchCount> m_terrainFloorVertices;
    /** @brief 今フレームに実際に描画する床頂点数です。 */
    std::array<unsigned int, TerrainBatchCount> m_terrainFloorVertexCounts = {};

    /** @brief 敵ビルボードバッチの1頂点です。 */
    struct EnemyBillboardVertex
    {
        DirectX::XMFLOAT3 position = {};
        DirectX::XMFLOAT2 uv = {};
        DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };
    };

    /** @brief 全敵ビルボードをまとめて送る動的頂点バッファです。 */
    MeshBuffer* m_enemyBillboardMesh = nullptr;
    /** @brief 敵ビルボードのワールド座標を変換する頂点シェーダーです。 */
    VertexShader* m_enemyBillboardVS = nullptr;
    /** @brief 敵画像を透過付きで描画するピクセルシェーダーです。 */
    PixelShader* m_enemyBillboardPS = nullptr;
    /** @brief 突撃型と縄張り型の敵スプライトシートです。 */
    std::array<Texture*, 2> m_enemyTextures = {};
    /** @brief 現在エリアの最大敵数分を確保して再利用するCPU側配列です。 */
    std::vector<EnemyBillboardVertex> m_enemyBillboardVertices;
    /** @brief 今フレームに実際に描画する敵ビルボード頂点数です。 */
    unsigned int m_enemyBillboardVertexCount = 0;

    /** @brief プレイヤー攻撃命中時に再生する10コマのスプライトシートです。 */
    Texture* m_attackHitTexture = nullptr;

    /** @brief プレイヤー移動用スプライトシートです。 */
    Texture* m_playerTexture = nullptr;

    /** @brief ジャンプ開始地点で再生するスプライトシートです。 */
    Texture* m_jumpEffectTexture = nullptr;

    /** @brief キャラクタースプライトの共通アニメーション経過時間です。 */
    float m_characterAnimationTime = 0.0f;

    /** @brief カメラ位置へ追従して描画するスカイスフィアです。 */
    Model* m_skyModel = nullptr;

    /** @brief 環境モデル登録簿から読み込んだモデル資源です。 */
    struct EnvironmentModelResource
    {
        std::string id;
        bool isTree = false;
        Model* model = nullptr;
        DirectX::XMFLOAT3 defaultScale = { 1.0f, 1.0f, 1.0f };
        int footprintX = 1;
        int footprintZ = 1;
        bool colliderEnabled = false;
        DirectX::XMFLOAT3 colliderCenter = {};
        DirectX::XMFLOAT3 colliderSize = { 1.0f, 1.0f, 1.0f };
        DirectX::XMFLOAT3 placementAnchor = {};
        float horizontalSize = 1.0f;
    };
    std::vector<EnvironmentModelResource> m_environmentModels;

    struct BaseModelResource
    {
        Model* model = nullptr;
        DirectX::XMFLOAT3 placementAnchor = {};
        float horizontalSize = 1.0f;
    };
    /** @brief 第二拠点、前衛拠点の順に保持するモデル資源です。 */
    std::array<BaseModelResource, 2> m_baseModels = {};

    /** @brief ロープ表示用の静的モデルです。 */
    Model* m_ropeModel = nullptr;
    /** @brief FBX内の旧テクスチャ参照を置き換える拡散テクスチャです。 */
    Texture* m_ropeTexture = nullptr;
    /** @brief ロープモデルの境界中心です。 */
    DirectX::XMFLOAT3 m_ropeModelCenter = {};
    /** @brief ロープモデルのXYZ方向の境界寸法です。 */
    DirectX::XMFLOAT3 m_ropeModelSize = { 1.0f, 1.0f, 1.0f };
    /** @brief ロープモデルで最も長い軸です。0:X、1:Y、2:Zです。 */
    int m_ropeModelLengthAxis = 1;
    /** @brief ロープ上端へ配置する支柱モデルです。 */
    Model* m_ropeSupportModel = nullptr;
    /** @brief 支柱モデルへ適用する木材テクスチャです。 */
    Texture* m_ropeSupportTexture = nullptr;
    /** @brief 支柱モデル底面中央の配置基準です。 */
    DirectX::XMFLOAT3 m_ropeSupportAnchor = {};
    /** @brief 支柱モデルのXYZ方向の境界寸法です。 */
    DirectX::XMFLOAT3 m_ropeSupportSize = { 1.0f, 1.0f, 1.0f };
    /** @brief 支柱モデルで長い水平軸です。0:X、2:Zです。 */
    int m_ropeSupportForwardAxis = 2;

    /** @brief 全採掘ポイントへ使用する静的モデルです。 */
    Model* m_miningPointModel = nullptr;
    /** @brief 採掘ポイントモデル底面中央の配置基準です。 */
    DirectX::XMFLOAT3 m_miningPointModelAnchor = {};
    /** @brief 採掘ポイントモデルのX・Z方向の最大寸法です。 */
    float m_miningPointModelHorizontalSize = 1.0f;

    /** @brief 通常攻撃中に振るつるはしモデルです。 */
    Model* m_pickaxeModel = nullptr;
    /** @brief つるはしモデル境界の中心です。 */
    DirectX::XMFLOAT3 m_pickaxeModelCenter = {};
    /** @brief 表示寸法を揃えるために使用するモデル境界の最大寸法です。 */
    float m_pickaxeModelLongestSize = 1.0f;

    /** @brief 敵攻撃命中後にカメラを揺らす残り時間です。 */
    float m_cameraShakeTimer = 0.0f;

    /** @brief プレイテスト中に編集するプレイヤー調整値です。 */
    PlayerDebugParams m_debugPlayerParams;

    /** @brief 帰還範囲と採掘範囲の当たり判定形状を表示するかどうかです。 */
    bool m_showCollisionDebug = true;

    /** @brief Eキー地図専用の表示倍率です。 */
    float m_mapZoom = 3.0f;
    /** @brief 読み込んだマップ全体を収めるワールド半径です。 */
    float m_worldHalfSize = 45.0f;
    /** @brief 探索カメラの水平回転角（ラジアン）です。 */
    float m_cameraYaw = DirectX::XMConvertToRadians(45.0f);
    /** @brief 探索カメラの上下回転角（ラジアン）です。 */
    float m_cameraPitch = DirectX::XMConvertToRadians(35.264f);
    /** @brief 探索カメラとプレイヤーの距離です。 */
    float m_cameraDistance = 13.8564f;
    /** @brief 地形との衝突を考慮した実際のカメラ距離です。 */
    float m_cameraCollisionDistance = -1.0f;
    /** @brief 探索カメラのY方向オフセット下限です。 */
    /** @brief 探索カメラの仰角下限です。仰角は真横を0度、真上を90度とします。 */
    float m_cameraMinPitchDegrees = 10.0f;
    /** @brief 探索カメラのY方向オフセット上限です。 */
    /** @brief 探索カメラの仰角上限です。仰角は真横を0度、真上を90度とします。 */
    float m_cameraMaxPitchDegrees = 60.0f;

#if defined(NARAKU_EDITOR_BUILD)
    bool m_editorOverviewMode = false;
    PlayerState m_editorWalkPlayerState = {};
    float m_editorWalkCameraPitch = 0.0f;
    float m_editorWalkCameraDistance = 0.0f;
    float m_editorWalkCameraMaxPitch = 60.0f;
    bool m_editorPreviewGenerationActive = false;
    bool m_editorPreviewGenerationFailed = false;
    int m_editorPreviewGenerationCursor = 0;
    int m_editorPreviewGenerationCompleted = 0;
    int m_editorPreviewGenerationTotal = 0;
    int m_editorPreviewStartArea = -1;
    unsigned long long m_editorPreviewGenerationSeed = 1;
#endif

    /** @brief Eキー地図専用のスクロールオフセット（ワールド座標系）です。 */
    Vec2 m_mapScrollOffset = { 0.0f, 0.0f };

    /** @brief メニューやUIが開いた際にImGuiウィンドウへ初期フォーカスを要求するフラグです。 */
    bool m_menuFocusPending = false;

};
