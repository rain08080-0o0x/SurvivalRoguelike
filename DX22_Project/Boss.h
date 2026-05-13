#ifndef __BOSS_H__
#define __BOSS_H__

#include <DirectXMath.h>
#include <vector>

class Texture;

/**
 * @brief Owns boss battle state, attack pattern data, and boss-side visual helpers.
 */
class BossController
{
public:
    /**
     * @brief Axis used by lane-based dash attacks.
     */
    enum AttackPattern
    {
        /** @brief Dash along the Z axis. */
        AttackPatternVertical = 0,
        /** @brief Dash along the X axis. */
        AttackPatternHorizontal
    };

    /**
     * @brief Runtime state of the current boss attack.
     */
    enum AttackState
    {
        /** @brief Waiting for the next attack selection. */
        AttackIdle = 0,
        /** @brief Telegraph is visible before the hit resolves. */
        AttackTelegraph,
        /** @brief Active execution window for the attack. */
        AttackDash
    };

    /**
     * @brief Attack kinds the boss can choose from.
     */
    enum AttackKind
    {
        /** @brief Narrow dash lane. */
        AttackKindDashNarrow = 0,
        /** @brief Wide dash lane. */
        AttackKindDashWide,
        /** @brief Random falling attack. */
        AttackKindRandomRain,
        /** @brief Summons minions. */
        AttackKindSummon,
        /** @brief Player-tracking falling attack. */
        AttackKindTrackingDrop,
        /** @brief Cross-lane ultimate. */
        AttackKindUltimateCross,
        /** @brief Repeated stomp ultimate. */
        AttackKindUltimateStomp,
        /** @brief Near full-arena ultimate. */
        AttackKindUltimateField,
        /** @brief Generic profile-driven attack. */
        AttackKindCustom
    };

    /**
     * @brief Collider zone used to draw danger zones and safe zones.
     */
    struct AttackZone
    {
        /** @brief Zone center. */
        DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
        /** @brief Zone size. */
        DirectX::XMFLOAT3 size = { 0.0f, 0.0f, 0.0f };
        /** @brief Collider shape. 0=Box 1=Circle. */
        int shape = 0;
        /** @brief Zone draw color. */
        DirectX::XMFLOAT4 color = { 1.0f, 0.15f, 0.10f, 1.0f };
        /** @brief Time offset before the zone becomes visible. */
        float revealStart = 0.0f;
        /** @brief Rotation around the vertical axis in degrees. */
        float yawDeg = 0.0f;
        /** @brief Original start point for path-based zones. */
        DirectX::XMFLOAT3 startPos = { 0.0f, 0.0f, 0.0f };
        /** @brief Original end point for path-based zones. */
        DirectX::XMFLOAT3 endPos = { 0.0f, 0.0f, 0.0f };
        /** @brief True when the zone is a sweep/path instead of a single point. */
        bool hasPath = false;
        /** @brief True when the zone is safe instead of dangerous. */
        bool safeZone = false;
    };

    /**
     * @brief Lightweight data for falling rock visuals.
     */
    struct FallingRock
    {
        /** @brief Rock position. */
        DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
        /** @brief Rock size. */
        DirectX::XMFLOAT3 size = { 1.0f, 1.0f, 1.0f };
        /** @brief Elapsed lifetime. */
        float timer = 0.0f;
        /** @brief Total lifetime. */
        float duration = 0.0f;
        /** @brief Height above the ground where the visual starts. */
        float spawnHeight = 2.8f;
        /** @brief Spin speed in degrees per second. */
        float spinDegPerSec = 0.0f;
        /** @brief Current spin angle in degrees. */
        float angleDeg = 0.0f;
        /** @brief True when the visual should face the camera. */
        bool billboard = true;
    };

    /**
     * @brief Active lane definition for dash-style attacks.
     */
    struct AttackLane
    {
        /** @brief Lane center. */
        DirectX::XMFLOAT3 center = { 0.0f, 0.0f, 0.0f };
        /** @brief Lane size. */
        DirectX::XMFLOAT3 size = { 0.0f, 0.0f, 0.0f };
        /** @brief Lane axis. */
        AttackPattern pattern = AttackPatternVertical;
    };

    /**
     * @brief Default constructor.
     */
    BossController() = default;

    /**
     * @brief Default destructor.
     */
    ~BossController() = default;

    /**
     * @brief Resets boss state for a new scene.
     * @param playerSize Player size used as the base for boss scaling.
     * @param bossSizeAreaScale Boss size multiplier.
     * @param bossMaxHp Boss maximum HP.
     * @param bossGuardInitialMax Initial guard maximum.
     */
    void ResetForScene(const DirectX::XMFLOAT3& playerSize,
                       float bossSizeAreaScale,
                       int bossMaxHp,
                       float bossGuardInitialMax);

    /**
     * @brief Loads the boss base texture.
     * @param path Texture path to load.
     */
    void LoadTexture(const char* path);

    /**
     * @brief Loads the falling rock texture.
     * @param path Texture path to load.
     */
    void LoadRockTexture(const char* path);

    /**
     * @brief Loads the broken-state texture.
     * @param path Texture path to load.
     */
    void LoadBrokenTexture(const char* path);

    /**
     * @brief Releases the boss base texture.
     */
    void ReleaseTexture();

    /**
     * @brief Releases the falling rock texture.
     */
    void ReleaseRockTexture();

    /**
     * @brief Releases the broken-state texture.
     */
    void ReleaseBrokenTexture();

    /** @brief Boss base texture. */
    Texture* texture = nullptr;
    /** @brief Falling rock texture. */
    Texture* rockTexture = nullptr;
    /** @brief Broken-state texture. */
    Texture* brokenTexture = nullptr;
    /** @brief Boss world position. */
    DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
    /** @brief Boss size used for drawing and gameplay scale. */
    DirectX::XMFLOAT3 size = { 2.0f, 2.0f, 2.0f };
    /** @brief Normal draw color. */
    DirectX::XMFLOAT4 color = { 0.07f, 0.07f, 0.07f, 1.0f };
    /** @brief Current HP. */
    int hp = 1;
    /** @brief Maximum HP. */
    int maxHp = 1;
    /** @brief Current battle phase. */
    int phase = 1;
    /** @brief Current archetype resolved from the roguelike route. */
    int archetype = 0;
    /** @brief Last swing id that damaged the boss. */
    int lastHitSwingId = -1;
    /** @brief Final-boss script entry index used by the current attack. */
    int scriptEntryIndex = -1;
    /** @brief Damage multiplier applied only to the active attack. */
    float attackDamageScale = 1.0f;
    /** @brief True when the current attack came from the scripted final-boss editor. */
    bool scriptedAttack = false;
    /** @brief Currently selected attack kind. */
    AttackKind attackKind = AttackKindDashNarrow;
    /** @brief Current lane axis. */
    AttackPattern attackPattern = AttackPatternVertical;
    /** @brief Generic attack facing yaw in degrees. */
    float attackFacingYawDeg = 0.0f;
    /** @brief Active lane data. */
    AttackLane attackLane;
    /** @brief Active telegraph and safe-zone boxes. */
    std::vector<AttackZone> attackZones;
    /** @brief Active falling rock visuals. */
    std::vector<FallingRock> fallingRocks;
    /** @brief Current guard value. */
    float guard = 0.0f;
    /** @brief Maximum guard value. */
    float guardMax = 0.0f;
    /** @brief Time remaining until guard recovers from break. */
    float breakRecoverTimer = 0.0f;
    /** @brief Fractional damage carry before converting to integer HP loss. */
    float hpDamageCarry = 0.0f;
    /** @brief Current attack state. */
    AttackState attackState = AttackIdle;
    /** @brief Elapsed time inside the current attack state. */
    float attackStateTimer = 0.0f;
    /** @brief Telegraph duration for the active attack. */
    float attackTelegraphDuration = 0.0f;
    /** @brief Execute duration for the active attack. */
    float attackExecuteDuration = 0.0f;
    /** @brief Optional per-attack jump-out override. Negative disables override. */
    float attackJumpOutOverrideSec = -1.0f;
    /** @brief Cooldown remaining before the next attack starts. */
    float attackCooldownTimer = 0.0f;
    /** @brief Remaining repeats for a repeated pattern. */
    int attackRepeatsRemaining = 0;
    /** @brief Counter used to rotate through attack patterns. */
    int attackCycleCount = 0;
    /** @brief Elapsed time since the boss battle started. */
    float battleTimer = 0.0f;
    /** @brief Remembers the previous scripted final-boss attack index. */
    int finalLastAttackIndex = -1;
    /** @brief True once the timed instant-death pattern was already used. */
    bool finalLethalTriggered = false;
    /** @brief True when a phase transition should force a special opener. */
    bool finalPhaseTransitionPending = false;
    /** @brief Dash start position. */
    DirectX::XMFLOAT3 dashStartPos = { 0.0f, 0.0f, 0.0f };
    /** @brief Dash end position. */
    DirectX::XMFLOAT3 dashEndPos = { 0.0f, 0.0f, 0.0f };
    /** @brief True when special attacks are unlocked. */
    bool specialUnlocked = false;
    /** @brief Forces the next pattern to be an ultimate. */
    bool forceUltimatePending = false;
    /** @brief True while the boss is guard-broken. */
    bool isBroken = false;
    /** @brief True once the current attack already resolved its hit. */
    bool attackResolved = false;
    /** @brief True once jump-out staging has been processed. */
    bool jumpedOut = false;
    /** @brief True when arena state should be reset on start. */
    bool requiresArenaReset = true;
};

#endif // __BOSS_H__
