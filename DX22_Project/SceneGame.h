#ifndef __SCENE_GAME_H__
#define __SCENE_GAME_H__

#include "Scene.h"
#include "Boss.h"
#include "BossAttackScript.h"
#include "Camera.h"
#include "ParticleEmitter2D.h"
#include "Player.h"
#include "UIObjectManager.h"
#include <functional>
#include <vector>

class UIObject;
class Enemy;
class Texture;
class CameraDebug;
struct XAUDIO2_BUFFER;
struct IXAudio2SourceVoice;

/**
 * @brief ゲーム本編シーンの進行、戦闘、UI、カメラ、サウンドを統括するシーンです。
 */
class SceneGame : public Scene
{
public:
    /**
     * @brief ゲームシーンを生成し、必要なオブジェクトを初期化します。
     */
    SceneGame();

    /**
     * @brief ゲームシーンが保持するオブジェクトとリソースを解放します。
     */
    ~SceneGame();

    /**
     * @brief 戦闘進行、入力、UI、カメラを 1 フレーム分更新します。
     */
    void Update() final;

    /**
     * @brief ゲームシーンの 3D/2D 要素を描画します。
     */
    void Draw() final;

private:
    /**
     * @brief 敵1体分の実体と戦闘補助状態をまとめるスロットです。
     */
    struct EnemySlot
    {
        /** @brief 実際の敵オブジェクトです。 */
        Enemy* enemy = nullptr;
        /** @brief 敵攻撃予兆の残り時間です。 */
        float attackWindupTimer = 0.0f;
        /** @brief 次の敵攻撃までの残り時間です。 */
        float attackCooldownTimer = 0.0f;
        /** @brief 同一スイング多段ヒット防止用の最終ヒット ID です。 */
        int lastHitSwingId = -1;
        /** @brief 被弾フラッシュ演出の残り時間です。 */
        float hitFlashTimer = 0.0f;
        /** @brief 衛星スキルの再ヒット待ち時間です。 */
        float skillContactCooldownTimer = 0.0f;
        /** @brief 貫通弾スキルが最後に当てた識別 ID です。 */
        int lastSkillProjectileId = -1;
        /** @brief デバッグ表示上、攻撃レンジ内かどうかです。 */
        bool debugInAttackRange = false;
        /** @brief デバッグ表示用の攻撃レンジ半径です。 */
        float debugAttackRange = 0.0f;
        /** @brief 現在付与されている鎖数です。 */
        int chainCount = 0;
        /** @brief 残っている燃焼ダメージ総量です。 */
        float burnPool = 0.0f;
        /** @brief 1 秒あたりに減少する燃焼ダメージ量です。 */
        float burnDps = 0.0f;
        /** @brief 燃焼ダメージ端数の蓄積です。 */
        float burnCarry = 0.0f;
    };

    /**
     * @brief 瞬間的なマーカー演出を描画するためのデータです。
     */
    struct MarkerEffect
    {
        /** @brief マーカーの中心位置です。 */
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        /** @brief マーカーのサイズです。 */
        DirectX::XMFLOAT3 size = { 1.0f, 1.0f, 1.0f };
        /** @brief マーカーの色です。 */
        DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        /** @brief 表示経過時間です。 */
        float timer = 0.0f;
        /** @brief 表示継続時間です。 */
        float duration = 0.0f;
        /** @brief 時間経過で広げる拡大倍率です。 */
        float growScale = 1.0f;
        /** @brief true の場合は地面ではなくビルボードで描画します。 */
        bool billboard = false;
        /** @brief 指定時は既定以外のテクスチャで描画します。 */
        Texture* texture = nullptr;
    };

    /**
     * @brief 敵の飛び道具を管理する軽量データです。
     */
    struct EnemyProjectile
    {
        /** @brief 現在位置です。 */
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        /** @brief 毎フレーム加算する速度です。 */
        DirectX::XMFLOAT3 vel = { 0.0f, 0.0f, 0.0f };
        /** @brief 当たり判定半径です。 */
        float radius = 0.2f;
        /** @brief 消滅までの残り寿命です。 */
        float life = 0.0f;
        /** @brief プレイヤーへ与えるダメージです。 */
        float damage = 0.0f;
    };

    /**
     * @brief プレイヤースキルの貫通弾です。
     */
    struct SkillProjectile
    {
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 dir = { 0.0f, 0.0f, 1.0f };
        float radius = 0.25f;
        float speed = 0.0f;
        float remainDistance = 0.0f;
        int damage = 0;
        int projectileId = -1;
        float knockbackScale = 1.0f;
        int sourceSkillType = 0;
        int sourceDamage = 0;
        bool sourceWeapon = false;
        bool critical = false;
        int remainingInnateBloodCount = 0;
        float innateBurnDamage = 0.0f;
    };

    /**
     * @brief 血特性で回収する血の塊です。
     */
    struct BloodOrb
    {
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        float timer = 0.0f;
    };

    /**
     * @brief 鎖付与時に一時的に表示するチェーン演出です。
     */
    struct ChainBeamEffect
    {
        DirectX::XMFLOAT3 startPos = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 endPos = { 0.0f, 0.0f, 0.0f };
        float timer = 0.0f;
        float duration = 0.0f;
    };

    /**
     * @brief プレイヤー周囲を回る衛星スキル状態です。
     */
    struct OrbitSkillState
    {
        bool active = false;
        float timer = 0.0f;
        float duration = 0.0f;
        float angle = 0.0f;
        float angularSpeed = 0.0f;
        float radius = 0.0f;
        int count = 1;
        int damage = 0;
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 size = { 0.0f, 0.0f, 0.0f };
    };

    /**
     * @brief 挑戦ステージ中に使う攻撃予兆 1 個分です。
     */
    struct ChallengeHazard
    {
        DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
        float radius = 1.0f;
        float timer = 0.0f;
        float telegraphDuration = 0.8f;
        bool resolved = false;
    };

    /**
     * @brief 距離ソートして描画するエントリです。
     */
    struct DrawEntry
    {
        /** @brief カメラからの距離二乗です。 */
        float distSq = 0.0f;
        /** @brief プレイヤー描画エントリかどうかです。 */
        bool isPlayer = false;
        /** @brief ボス描画エントリかどうかです。 */
        bool isBoss = false;
        /** @brief 描画対象の位置です。 */
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        /** @brief 描画対象のサイズです。 */
        DirectX::XMFLOAT3 size = { 0.0f, 0.0f, 0.0f };
        /** @brief 敵描画時に参照する敵実体です。 */
        Enemy* enemy = nullptr;
    };

    /**
     * @brief クールタイム UI の添字です。
     */
    enum CooldownSlot
    {
        /** @brief 通常攻撃用スロットです。 */
        CooldownAttack = 0,
        /** @brief 回避用スロットです。 */
        CooldownEvade,
        /** @brief スキル1用スロットです。 */
        CooldownSkill1,
        /** @brief スキル2用スロットです。 */
        CooldownSkill2,
        /** @brief スロット数です。 */
        CooldownSlotCount
    };

    enum ClearPortalMode
    {
        ClearPortalNone = 0,
        ClearPortalReward,
        ClearPortalNextStage,
        ClearPortalFinalResult,
    };

    /**
     * @brief デバッグ起動できる挑戦種別です。
     */
    enum ChallengeType
    {
        ChallengeNone = 0,
        ChallengeNoDamage = 1,
        ChallengeDefense = 2
    };

    /**
     * @brief プレイヤー HP ゲージ表示を更新します。
     */
    void UpdateHpGauge();

    /**
     * @brief 各クールタイムゲージの表示率を更新します。
     */
    void UpdateCooldownGauges();

    /**
     * @brief 指定インデックスに敵を生成します。
     * @param index 生成先スロット番号です。
     * @param stageSize スポーン計算に使うステージサイズです。
     */
    void SpawnEnemyByIndex(int index, float stageSize);

    /**
     * @brief 目標数に合わせて敵数を増減させます。
     * @param targetCount 目標敵数です。
     * @param stageSize スポーン計算に使うステージサイズです。
     */
    void EnsureEnemyCount(int targetCount, float stageSize);

    /**
     * @brief Wave 番号からその Wave の敵数を計算します。
     * @param baseCount 基本敵数です。
     * @param waveIndex 現在 Wave 番号です。
     * @param addPerWave Wave ごとの増加数です。
     * @return 計算した敵数です。
     */
    int CalcWaveEnemyCount(int baseCount, int waveIndex, int addPerWave) const;

    /**
     * @brief 敵頭上に HP ゲージをビルボードで描画します。
     * @param headPos 頭上描画の基準位置です。
     * @param enemySize 敵サイズです。
     * @param rate 残 HP 比率です。
     */
    void DrawEnemyHpGaugeBillboard(const DirectX::XMFLOAT3& headPos,
                                   const DirectX::XMFLOAT3& enemySize,
                                   float hpRate,
                                   float burnRate,
                                   int chainCount);

    /**
     * @brief シーン開始時のボス状態を初期化します。
     */
    void InitializeBossForScene();

    /**
     * @brief ボス戦で使うテクスチャなどのリソースを読み込みます。
     */
    void LoadBossResources();

    /**
     * @brief ボス戦で使うリソースを解放します。
     */
    void ReleaseBossResources();

    /**
     * @brief デバッグ要求に応じてボス戦開始状態を整えます。
     * @param stageSize フィールド初期化に使うステージサイズです。
     * @return ボス戦へ移行した場合は true です。
     */
    bool UpdateBossDebugSetup(float stageSize);

    /**
     * @brief デバッグ要求に応じて挑戦ステージ開始状態を整えます。
     * @param stageSize フィールド初期化に使うステージサイズです。
     */
    void UpdateChallengeDebugSetup(float stageSize);

    /**
     * @brief 指定挑戦を開始し、戦闘用の一時状態を初期化します。
     * @param challengeType 開始する挑戦種別です。
     * @param stageSize 現在のステージサイズです。
     */
    void StartChallenge(int challengeType, float stageSize);

    /**
     * @brief 挑戦ステージの一時状態を全て初期化します。
     */
    void ResetChallengeState();

    /**
     * @brief ノーダメ挑戦を更新します。
     * @param stageSize 現在のステージサイズです。
     * @return シーン遷移などでこのフレームを終了した場合は true です。
     */
    bool UpdateChallengeNoDamage(float stageSize);

    /**
     * @brief 防衛挑戦を更新します。
     * @param stageSize 現在のステージサイズです。
     * @return シーン遷移などでこのフレームを終了した場合は true です。
     */
    bool UpdateChallengeDefense(float stageSize);

    /**
     * @brief 防衛挑戦用の敵を 1 体だけ追加します。
     * @param stageSize スポーン計算に使うステージサイズです。
     */
    void SpawnDefenseEnemy(float stageSize);

    /**
     * @brief 現在進行中の挑戦を報酬付きで終了します。
     * @param success クリア扱いなら true、失敗扱いなら false です。
     */
    void FinishChallenge(bool success);

    /**
     * @brief 挑戦失敗時の進行度に応じた報酬数を返します。
     * @return 1 以上 2 以下の報酬数です。
     */
    int CalcChallengeFailureRewardCount() const;

    /**
     * @brief ボス戦の行動更新とプレイヤー攻撃適用を処理します。
     * @param stageSize フィールドサイズです。
     * @param playerAttackDamage プレイヤー攻撃ダメージです。
     * @param applyPlayerDamage プレイヤー攻撃をボスへ適用するコールバックです。
     * @return ボス戦更新を行った場合は true です。
     */
    bool UpdateBossBattle(float stageSize,
                          int playerAttackDamage,
                          const std::function<bool(float)>& applyPlayerDamage);

    /**
     * @brief ボスの予兆マーカーを描画します。
     */
    void DrawBossTelegraphMarker() const;

    /**
     * @brief 挑戦ステージ用の予兆とビーコンを描画します。
     */
    void DrawChallengeMarkers() const;

    /**
     * @brief ボス攻撃の落下物演出を描画します。
     */
    void DrawBossFallingObjects() const;

    /**
     * @brief ボス用の描画エントリを追加します。
     * @param drawEntries 追加先の描画エントリ配列です。
     * @param cam カメラ位置です。
     */
    void AddBossDrawEntry(std::vector<DrawEntry>& drawEntries,
                          const DirectX::XMFLOAT3& cam) const;

    /**
     * @brief ボス用描画エントリを実際に描画します。
     * @param entry 描画対象エントリです。
     */
    void DrawBossEntry(const DrawEntry& entry) const;

    /**
     * @brief ボス HP / ガードの UI を描画します。
     */
    void DrawBossHpUi() const;

    /**
     * @brief 指定スロットへ装備中のスキル種別を返します。
     * @param slotIndex 0 が Q、1 が E です。
     * @return RoguelikeUpgrade::SkillType の値です。
     */
    int GetSkillTypeForSlot(int slotIndex) const;

    /**
     * @brief 奇襲スキルが狙う対象の中心とサイズを返します。
     * @param targetCenter 対象中心の受け取り先です。
     * @param targetSize 対象サイズの受け取り先です。
     * @return 対象が見つかった場合は true です。
     */
    bool TryGetAmbushTarget(DirectX::XMFLOAT3& targetCenter, DirectX::XMFLOAT3& targetSize) const;

    /**
     * @brief 入力されたスキルスロットを発動します。
     * @param slotIndex 0 が Q、1 が E です。
     * @param playerAttackDamage 現在のプレイヤー攻撃力です。
     * @param stageSize 現在のステージサイズです。
     * @return 発動に成功した場合は true です。
     */
    bool ActivateSkillSlot(int slotIndex, int playerAttackDamage, float stageSize);

    /**
     * @brief 発射体と衛星の位置・寿命を更新します。
     * @param dt 更新秒数です。
     * @param stageSize 現在のステージサイズです。
     */
    void UpdateSkillActors(float dt, float stageSize);

    /**
     * @brief ヒット演出用の保存済みパーティクル設定を読み込みます。
     */
    void LoadHitEffectPreset();

    /**
     * @brief ヒット演出用 emitter プールを初期化します。
     */
    void InitializeHitEffectEmitters();

    /**
     * @brief ヒット演出用 emitter プールを解放します。
     */
    void ReleaseHitEffectEmitters();

    /**
     * @brief ヒット演出 emitter を更新します。
     * @param dt 更新秒数です。
     */
    void UpdateHitEffectEmitters(float dt);

    /**
     * @brief ヒット演出 emitter を描画します。
     */
    void DrawHitEffectEmitters() const;

    /**
     * @brief 指定座標へ保存済みヒット演出を再生します。
     * @param pos 再生位置です。
     * @param size 当たり判定サイズです。
     * @param spawnParticle true の場合は particle も再生します。
     */
    void SpawnHitEffect(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, bool spawnParticle = true);

    /**
     * @brief カーソル位置から Ray を飛ばし、現在指している対象名をデバッグ共有へ反映します。
     * @param stageSize 現在のステージサイズです。
     */
    void UpdateCursorHoverDebug(float stageSize);

    /**
     * @brief スキル攻撃をボスへ適用します。
     * @param damage ボスへ与えるダメージです。
     * @return 撃破でシーン遷移した場合は true です。
     */
    bool ApplySkillDamageToBoss(int damage);
    void ResetTraitCombatState();
    void RefreshSkillStockState();
    void UpdateChainBeamEffects(float dt);
    void SpawnChainBeamEffect(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& endPos);
    int GetSkillStockMaxForSlot(int slotIndex) const;
    int GetSkillStockCountForSlot(int slotIndex) const;
    float GetSkillRechargeTimerForSlot(int slotIndex) const;
    bool TryConsumeSkillResource(int slotIndex);
    void BeginSkillCooldown(int slotIndex);
    void SpawnBloodOrb(const DirectX::XMFLOAT3& pos, int count = 1);
    void UpdateBloodOrbs(float dt, int playerAttackDamage);
    void UpdateDamageOverTime(float dt);
    int GetChainCapPerTarget() const;
    int GetBloodStockCap() const;
    float GetBloodPickupRadius() const;
    float GetBurnScale() const;
    bool HasChainBloodSynergy() const;
    bool HasBloodFireSynergy() const;
    bool HasChainFireSynergy() const;
    int ApplyChainToEnemy(EnemySlot& slot, int requestedCount, int sourceDamage, const DirectX::XMFLOAT3& effectPos);
    int ApplyChainToBoss(int requestedCount, int sourceDamage, const DirectX::XMFLOAT3& effectPos);
    void ApplyBurnToEnemy(EnemySlot& slot, float burnBaseDamage);
    void ApplyBurnToBoss(float burnBaseDamage);
    void ApplySkillWeaponBuff(int skillType);
    void DrawSkillHud() const;
    /** @brief 奇襲対象の頭上に反転三角マーカーを描画します。 */
    void DrawAmbushTargetMarker() const;
    void DrawBloodStockHud() const;
    void DrawChainBeamEffects() const;
    /** @brief クリア後ポータルの出現待ちと接触遷移を更新します。 */
    bool UpdateClearPortal(float dt);
    /** @brief クリア後ポータルの見た目を描画します。 */
    void DrawClearPortal() const;
    /** @brief 指定用途のクリア後ポータルを開始します。 */
    void BeginClearPortal(int mode, const DirectX::XMFLOAT3& preferredPos);

    /** @brief 現在アクティブとして扱うカメラです。 */
    Camera* m_pCamera;
    /** @brief ゲーム用カメラです。 */
    CameraDebug* m_pCameraGame;
    /** @brief デバッグ用カメラです。 */
    CameraDebug* m_pCameraDebug;
    /** @brief 現在のカメラモードです。 */
    int m_cameraMode;
    /** @brief プレイヤー本体です。 */
    Player* m_pPlayer;
    /** @brief 生成中の敵スロット一覧です。 */
    std::vector<EnemySlot> m_enemies;
    /** @brief 一時マーカー演出一覧です。 */
    std::vector<MarkerEffect> m_markerEffects;
    /** @brief ヒット演出用 emitter プールです。 */
    std::vector<ParticleEmitter2D*> m_hitEffectEmitters;
    /** @brief 挑戦ステージ用の予兆一覧です。 */
    std::vector<ChallengeHazard> m_challengeHazards;
    /** @brief 敵弾一覧です。 */
    std::vector<EnemyProjectile> m_enemyProjectiles;
    /** @brief スキル弾一覧です。 */
    std::vector<SkillProjectile> m_skillProjectiles;
    /** @brief 血特性で生成される血の塊一覧です。 */
    std::vector<BloodOrb> m_bloodOrbs;
    /** @brief 鎖付与時に一瞬だけ表示するチェーン演出一覧です。 */
    std::vector<ChainBeamEffect> m_chainBeamEffects;
    /** @brief ボス戦専用の状態コントローラです。 */
    BossController m_boss;
    /** @brief 現在のボス戦で使用する攻撃プロファイルです。 */
    BossAttackScript::Profile m_finalBossScript;
    /** @brief 現在のボス攻撃プロファイルで次に使う攻撃の位置です。 */
    int m_finalBossScriptCursor;
    /** @brief キャラ足元の影描画に使うテクスチャです。 */
    Texture* m_pShadow;
    /** @brief プレイヤー攻撃範囲可視化用テクスチャです。 */
    Texture* m_pAttackMarker;
    /** @brief プレイヤー向き/奇襲対象表示用の三角テクスチャです。 */
    Texture* m_pDirectionMarkerTexture;
    /** @brief 攻撃/被弾のスプライトシート演出に使うテクスチャです。 */
    Texture* m_pCombatEffectTexture;
    /** @brief スキル描画用テクスチャです。 */
    Texture* m_pSkillTexture;
    /** @brief 血スキル用 UI/血の塊描画テクスチャです。 */
    Texture* m_pBloodUiTexture;
    /** @brief 血ストック ON 表示テクスチャです。 */
    Texture* m_pBloodStockOnTexture;
    /** @brief 血ストック OFF 表示テクスチャです。 */
    Texture* m_pBloodStockOffTexture;
    /** @brief 鎖状態を HP バーに重ねるテクスチャです。 */
    Texture* m_pChainUiTexture;
    /** @brief プレイヤーから敵へ伸びる鎖演出テクスチャです。 */
    Texture* m_pChainLinkTexture;
    /** @brief ボス攻撃予兆用テクスチャです。 */
    Texture* m_pBossAttackRangeMarker;
    /** @brief 床タイル描画に使うテクスチャです。 */
    Texture* m_pGroundTexture;
    /** @brief 攻撃ヒット SE バッファです。 */
    XAUDIO2_BUFFER* m_pAttackSe;
    /** @brief プレイヤー被弾 SE バッファです。 */
    XAUDIO2_BUFFER* m_pPlayerHitSe;
    /** @brief 敵攻撃 SE バッファです。 */
    XAUDIO2_BUFFER* m_pEnemyAttackSe;
    /** @brief クリア SE バッファです。 */
    XAUDIO2_BUFFER* m_pClearSe;
    /** @brief ボス踏みつけ SE バッファです。 */
    XAUDIO2_BUFFER* m_pDropSe;
    /** @brief 通常戦闘 BGM バッファです。 */
    XAUDIO2_BUFFER* m_pGameBgm;
    /** @brief ボス戦 BGM バッファです。 */
    XAUDIO2_BUFFER* m_pBossBgm;
    /** @brief 再生中の BGM 音声ボイスです。 */
    IXAudio2SourceVoice* m_pGameBgmVoice;
    /** @brief 現在ボス BGM を再生中かどうかです。 */
    bool m_isBossBgmActive;
    /** @brief 敵 HP フレーム画像です。 */
    Texture* m_pEnemyHpFrame;
    /** @brief 敵 HP ゲージ画像です。 */
    Texture* m_pEnemyHpGauge;
    /** @brief プレイヤー HP フレーム UI です。 */
    UIObject* m_pHpFrame;
    /** @brief プレイヤー HP ゲージ UI です。 */
    UIObject* m_pHpGauge;
    /** @brief クールタイム用フレーム UI 配列です。 */
    UIObject* m_pCooldownFrame[CooldownSlotCount];
    /** @brief クールタイム用ゲージ UI 配列です。 */
    UIObject* m_pCooldownGauge[CooldownSlotCount];
    /** @brief 2D UI 全体をまとめて管理するマネージャです。 */
    UIObjectManager m_uiManager;
    /** @brief ステージ一辺の長さです。 */
    float m_stageSize;
    /** @brief 現在要求している敵総数です。 */
    int m_requestedEnemyCount;
    /** @brief 現在 Wave です。 */
    int m_currentWave;
    /** @brief 最終 Wave 数です。 */
    int m_waveMax;
    /** @brief クリア後ポータルの用途です。 */
    int m_clearPortalMode;
    /** @brief ポータルが出現するまでの待機時間です。 */
    float m_clearPortalSpawnTimer;
    /** @brief ポータル演出の経過時間です。 */
    float m_clearPortalPulseTimer;
    /** @brief クリア後ポータルの中心位置です。 */
    DirectX::XMFLOAT3 m_clearPortalPos;
    /** @brief カメラ導入演出中かどうかです。 */
    bool m_cameraIntroActive;
    /** @brief カメラ導入演出の経過時間です。 */
    float m_cameraIntroTimer;
    /** @brief カメラ導入演出の開始 Eye です。 */
    DirectX::XMFLOAT3 m_cameraIntroStartEye;
    /** @brief カメラ導入演出の開始 Look です。 */
    DirectX::XMFLOAT3 m_cameraIntroStartLook;
    /** @brief カメラ導入演出で寄る先の Eye です。 */
    DirectX::XMFLOAT3 m_cameraIntroFocusEye;
    /** @brief カメラ導入演出で寄る先の Look です。 */
    DirectX::XMFLOAT3 m_cameraIntroFocusLook;

    /** @brief プレイヤー攻撃判定が有効中かどうかです。 */
    bool m_attackActive;
    /** @brief 攻撃有効時間の残りです。 */
    float m_attackTimer;
    /** @brief 攻撃 windup の残り時間です。 */
    float m_attackWindupTimer;
    /** @brief 攻撃 recovery の残り時間です。 */
    float m_attackRecoveryTimer;
    /** @brief 攻撃再使用までの残り時間です。 */
    float m_attackCooldownTimer;
    /** @brief 攻撃 UI 表示用の残り時間です。 */
    float m_attackCooldownUiTimer;
    /** @brief 攻撃 UI 表示用の総時間です。 */
    float m_attackCooldownUiDuration;
    /** @brief スキル1残りクールタイムです。 */
    float m_skill1CooldownTimer;
    /** @brief スキル2残りクールタイムです。 */
    float m_skill2CooldownTimer;
    /** @brief スキル1クールタイム総時間です。 */
    float m_skill1CooldownDuration;
    /** @brief スキル2クールタイム総時間です。 */
    float m_skill2CooldownDuration;
    /** @brief 現在スイングの識別 ID です。 */
    int m_attackSwingId;
    /** @brief 次に使うヒット演出 emitter の添字です。 */
    int m_nextHitEffectEmitter;
    /** @brief 次に発行するスキル弾識別 ID です。 */
    int m_skillProjectileSerial;
    /** @brief 武器攻撃の累計回数です。CRT 判定に使います。 */
    int m_weaponAttackCount;
    /** @brief 通常攻撃の現在ストック数です。遠距離武器で使用します。 */
    int m_weaponAttackStock;
    /** @brief 通常攻撃の最大ストック数です。 */
    int m_weaponAttackStockMax;
    /** @brief 通常攻撃ストックの次回回復までの残り時間です。 */
    float m_weaponAttackStockRechargeTimer;
    /** @brief スキル命中由来の武器ダメージ上昇残り時間です。 */
    float m_weaponDamageBuffTimer;
    /** @brief スキル命中由来の武器ダメージ倍率です。 */
    float m_weaponDamageBuffScale;
    /** @brief スキルごとの現在ストック数です。 */
    int m_skillStockCount[2];
    /** @brief スキルごとの最大ストック数です。 */
    int m_skillStockMax[2];
    /** @brief 次のスキルストック回復までの残り時間です。 */
    float m_skillStockRechargeTimer[2];
    /** @brief 血ストック現在値です。 */
    int m_bloodStock;
    /** @brief 血ストック上限です。 */
    int m_bloodStockMax;
    /** @brief 今回スイングで当てた敵数です。 */
    int m_attackHitCountThisSwing;
    /** @brief ヒットストップ残り時間です。 */
    float m_hitStopTimer;
    /** @brief 攻撃軌跡を次に出すまでの残り時間です。 */
    float m_attackTrailSpawnTimer;
    /** @brief プレイヤー被弾フラッシュ残り時間です。 */
    float m_playerDamageFlashTimer;
    /** @brief 被弾後の無敵残り時間です。 */
    float m_playerDamageInvincibleTimer;
    /** @brief 俊敏デバフ型ボスから受ける一時クールダウン弱体の残り時間です。 */
    float m_bossCurseTimer;
    /** @brief ボス踏みつけ着地演出の残り時間です。 */
    float m_bossStompImpactTimer;
    /** @brief 画面揺れの残り時間です。 */
    float m_screenShakeTimer;
    /** @brief 現在の画面揺れ継続時間設定です。 */
    float m_screenShakeDuration;
    /** @brief 現在の画面揺れ強度設定です。 */
    float m_screenShakeAmplitude;
    /** @brief 画面揺れの位相です。 */
    float m_screenShakePhase;
    /** @brief 敵攻撃 SE の連打防止タイマーです。 */
    float m_enemyAttackSeGateTimer;
    /** @brief 敵パフォーマンスタイミング分散用の位相です。 */
    unsigned int m_enemyPerfPhase;
    /** @brief ボスへの衛星スキル再ヒット待ち時間です。 */
    float m_bossSkillContactCooldownTimer;
    /** @brief 直近の有効移動方向です。 */
    DirectX::XMFLOAT3 m_lastMoveDir;
    /** @brief 現在攻撃判定の中心です。 */
    DirectX::XMFLOAT3 m_attackCenter;
    /** @brief 現在攻撃判定のサイズです。 */
    DirectX::XMFLOAT3 m_attackSize;
    /** @brief 現在の衛星スキル状態です。 */
    OrbitSkillState m_orbitSkill;
    /** @brief 現在スイング中の通常攻撃ダメージです。 */
    int m_attackDamageThisSwing;
    /** @brief 現在スイング中の通常攻撃ノックバック倍率です。 */
    float m_attackKnockbackScaleThisSwing;
    /** @brief 現在スイングが CRT かどうかです。 */
    bool m_attackCriticalThisSwing;
    /** @brief ボスへ付与されている鎖数です。 */
    int m_bossChainCount;
    /** @brief ボスに残っている燃焼ダメージ総量です。 */
    float m_bossBurnPool;
    /** @brief ボスに対する毎秒燃焼ダメージ量です。 */
    float m_bossBurnDps;
    /** @brief ボス燃焼ダメージ端数の蓄積です。 */
    float m_bossBurnCarry;
    /** @brief 保存済みヒット演出の設定です。 */
    ParticleEmitter2D::Settings m_hitEffectPreset;
    /** @brief 現在有効な挑戦種別です。 */
    int m_challengeType;
    /** @brief 挑戦が進行中かどうかです。 */
    bool m_challengeActive;
    /** @brief 挑戦の経過時間です。 */
    float m_challengeTimer;
    /** @brief 挑戦全体の制限時間です。 */
    float m_challengeDuration;
    /** @brief ノーダメ挑戦の次回攻撃生成までの残り時間です。 */
    float m_challengeNoDamageSpawnTimer;
    /** @brief 防衛挑戦の次回敵生成までの残り時間です。 */
    float m_challengeDefenseSpawnTimer;
    /** @brief 防衛挑戦ビーコンの現在 HP です。 */
    float m_challengeBeaconHp;
    /** @brief 防衛挑戦ビーコンの最大 HP です。 */
    float m_challengeBeaconMaxHp;
    /** @brief ノーダメ挑戦で被弾した回数です。 */
    int m_challengeHitCount;
    /** @brief 防衛挑戦のスポーン順を決める連番です。 */
    int m_challengeSpawnSerial;
    /** @brief 防衛挑戦ビーコンの中心位置です。 */
    DirectX::XMFLOAT3 m_challengeBeaconPos;
    /** @brief 防衛挑戦ビーコンの描画サイズです。 */
    DirectX::XMFLOAT3 m_challengeBeaconSize;
    /** @brief 最後にボスへ当てたスキル弾識別 ID です。 */
    int m_lastBossSkillProjectileId;
    /** @brief ポーズ中かどうかです。 */
    bool m_isPaused;
    /** @brief ポーズ画面の現在タブです。 */
    int m_pauseTabIndex;
    /** @brief ポーズメニュー選択番号です。 */
    int m_pauseMenuSelection;
    /** @brief ポーズ中のオプション画面を開いているかどうかです。 */
    bool m_isPauseOptionOpen;
    /** @brief ポーズオプション選択番号です。 */
    int m_pauseOptionSelection;
    /** @brief ボス戦デバッグ起動かどうかです。 */
    bool m_isBossBattleDebug;
};

#endif // __SCENE_GAME_H__

