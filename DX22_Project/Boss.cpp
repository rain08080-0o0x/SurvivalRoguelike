#include "SceneGame.h"
#include "Collision.h"
#include "Defines.h"
#include "Input.h"
#include "Main.h"
#include "SceneManager.h"
#include "Sound.h"
#include "Sprite.h"
#include "Texture.h"
#include "Transfer.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{
    // Boss update uses a fixed-step simulation to stay aligned with SceneGame.
    const float kBossFixedDt = 1.0f / 60.0f;

    /**
     * @brief Clamps a float into the 0.0 to 1.0 range.
     * @param v Value to clamp.
     * @return Clamped value.
     */
    float Clamp01(float v)
    {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    /**
     * @brief Clamps a float into an arbitrary range.
     * @param v Value to clamp.
     * @param lo Lower bound.
     * @param hi Upper bound.
     * @return Clamped value.
     */
    float ClampRange(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    /**
     * @brief Clamps an integer into an arbitrary range.
     * @param v Value to clamp.
     * @param lo Lower bound.
     * @param hi Upper bound.
     * @return Clamped value.
     */
    int ClampInt(int v, int lo, int hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    /**
     * @brief Returns the greater of two floats.
     * @param a First value.
     * @param b Second value.
     * @return Larger value.
     */
    float MaxFloat(float a, float b)
    {
        return (a > b) ? a : b;
    }

    DirectX::XMFLOAT4 LerpColor(
        const DirectX::XMFLOAT4& a,
        const DirectX::XMFLOAT4& b,
        float t)
    {
        const float rate = Clamp01(t);
        return {
            a.x + (b.x - a.x) * rate,
            a.y + (b.y - a.y) * rate,
            a.z + (b.z - a.z) * rate,
            a.w + (b.w - a.w) * rate
        };
    }

    DirectX::XMFLOAT4 MulColor(
        const DirectX::XMFLOAT4& color,
        float scale)
    {
        return {
            ClampRange(color.x * scale, 0.0f, 1.0f),
            ClampRange(color.y * scale, 0.0f, 1.0f),
            ClampRange(color.z * scale, 0.0f, 1.0f),
            color.w
        };
    }

    /**
     * @brief Enforces a minimum span for area calculations.
     * @param v Span to validate.
     * @param fallback Minimum fallback when the span is too small.
     * @return v when large enough, otherwise fallback.
     */
    float SafeSpan(float v, float fallback)
    {
        return (v > 0.05f) ? v : fallback;
    }

    /**
     * @brief Returns a random float in the 0.0 to 1.0 range.
     * @return Normalized random value.
     */
    float Random01()
    {
        const int maxRand = (RAND_MAX > 0) ? RAND_MAX : 1;
        return static_cast<float>(std::rand()) / static_cast<float>(maxRand);
    }

    /**
     * @brief Returns a random float in the given range.
     * @param lo Lower bound.
     * @param hi Upper bound.
     * @return Random float in range.
     */
    float RandomRange(float lo, float hi)
    {
        return lo + (hi - lo) * Random01();
    }

    /**
     * @brief Returns a random integer in the given range.
     * @param lo Lower bound.
     * @param hi Upper bound.
     * @return Random integer in range.
     */
    int RandomRangeInt(int lo, int hi)
    {
        if (hi <= lo) return lo;
        return lo + (std::rand() % (hi - lo + 1));
    }

    /**
     * @brief ボス種別ごとの行動傾向をまとめた軽量設定です。
     */
    struct BossProfile
    {
        int archetype = Transfer::RoguelikeUpgrade::BossBalancedMid;
        DirectX::XMFLOAT4 color = { 0.75f, 0.72f, 0.48f, 1.0f };
        float hpScale = 1.0f;
        float guardScale = 1.0f;
        float sizeScale = 1.0f;
        float cooldownScale = 1.0f;
        float telegraphScale = 1.0f;
        float damageScale = 1.0f;
        float laneScale = 1.0f;
        float dashDurationScale = 1.0f;
        float jumpOutScale = 1.0f;
        float randomRainRadiusScale = 1.0f;
        float trackingRadiusScale = 1.0f;
        float fieldSafeScale = 1.0f;
        float randomRainCountScale = 1.0f;
        float summonCountScale = 1.0f;
        float trackingCountScale = 1.0f;
        float stompCountScale = 1.0f;
        int ultimateInterval = 5;
        bool startsSpecial = false;
        bool inflictsCurse = false;
        int topLevelWeights[5] = { 2, 2, 3, 2, 3 };
    };

    /**
     * @brief 指定ボス種別に対応する行動傾向を返します。
     * @param archetype ボス種別です。
     * @return ボス種別ごとの設定です。
     */
    BossProfile MakeBossProfile(int archetype)
    {
        BossProfile profile;
        profile.archetype = ClampInt(
            archetype,
            Transfer::RoguelikeUpgrade::BossHeavyMelee,
            Transfer::RoguelikeUpgrade::BossFinalBarrage);

        switch (profile.archetype)
        {
        case Transfer::RoguelikeUpgrade::BossHeavyMelee:
            profile.color = { 0.78f, 0.32f, 0.22f, 1.0f };
            profile.hpScale = 1.25f;
            profile.guardScale = 1.35f;
            profile.sizeScale = 1.18f;
            profile.cooldownScale = 1.18f;
            profile.telegraphScale = 1.10f;
            profile.damageScale = 1.20f;
            profile.laneScale = 1.20f;
            profile.dashDurationScale = 1.08f;
            profile.randomRainCountScale = 0.75f;
            profile.summonCountScale = 0.80f;
            profile.trackingCountScale = 0.70f;
            profile.stompCountScale = 1.35f;
            profile.ultimateInterval = 4;
            profile.topLevelWeights[0] = 6;
            profile.topLevelWeights[1] = 4;
            profile.topLevelWeights[2] = 1;
            profile.topLevelWeights[3] = 1;
            profile.topLevelWeights[4] = 1;
            break;

        case Transfer::RoguelikeUpgrade::BossLightRanged:
            profile.color = { 0.34f, 0.84f, 1.0f, 1.0f };
            profile.hpScale = 0.88f;
            profile.guardScale = 0.82f;
            profile.sizeScale = 0.88f;
            profile.cooldownScale = 0.82f;
            profile.telegraphScale = 0.88f;
            profile.damageScale = 0.85f;
            profile.laneScale = 0.92f;
            profile.dashDurationScale = 0.90f;
            profile.randomRainRadiusScale = 0.90f;
            profile.trackingRadiusScale = 0.90f;
            profile.randomRainCountScale = 1.55f;
            profile.summonCountScale = 1.10f;
            profile.trackingCountScale = 1.60f;
            profile.stompCountScale = 0.75f;
            profile.ultimateInterval = 4;
            profile.topLevelWeights[0] = 1;
            profile.topLevelWeights[1] = 1;
            profile.topLevelWeights[2] = 5;
            profile.topLevelWeights[3] = 2;
            profile.topLevelWeights[4] = 6;
            break;

        case Transfer::RoguelikeUpgrade::BossSwiftDebuff:
            profile.color = { 0.78f, 0.42f, 0.98f, 1.0f };
            profile.hpScale = 0.95f;
            profile.guardScale = 0.90f;
            profile.sizeScale = 0.92f;
            profile.cooldownScale = 0.68f;
            profile.telegraphScale = 0.76f;
            profile.damageScale = 0.95f;
            profile.laneScale = 0.95f;
            profile.dashDurationScale = 0.74f;
            profile.jumpOutScale = 0.75f;
            profile.randomRainCountScale = 0.70f;
            profile.summonCountScale = 0.60f;
            profile.trackingCountScale = 1.35f;
            profile.stompCountScale = 0.85f;
            profile.ultimateInterval = 3;
            profile.inflictsCurse = true;
            profile.topLevelWeights[0] = 6;
            profile.topLevelWeights[1] = 2;
            profile.topLevelWeights[2] = 1;
            profile.topLevelWeights[3] = 0;
            profile.topLevelWeights[4] = 5;
            break;

        case Transfer::RoguelikeUpgrade::BossFinalBarrage:
            profile.color = { 1.0f, 0.20f, 0.18f, 1.0f };
            profile.hpScale = 1.75f;
            profile.guardScale = 1.45f;
            profile.sizeScale = 1.08f;
            profile.cooldownScale = 0.55f;
            profile.telegraphScale = 0.65f;
            profile.damageScale = 1.15f;
            profile.laneScale = 1.05f;
            profile.dashDurationScale = 0.78f;
            profile.jumpOutScale = 0.60f;
            profile.randomRainRadiusScale = 0.92f;
            profile.trackingRadiusScale = 0.92f;
            profile.fieldSafeScale = 0.82f;
            profile.randomRainCountScale = 2.25f;
            profile.summonCountScale = 0.25f;
            profile.trackingCountScale = 2.40f;
            profile.stompCountScale = 1.55f;
            profile.ultimateInterval = 2;
            profile.startsSpecial = true;
            profile.topLevelWeights[0] = 1;
            profile.topLevelWeights[1] = 1;
            profile.topLevelWeights[2] = 7;
            profile.topLevelWeights[3] = 0;
            profile.topLevelWeights[4] = 8;
            break;

        case Transfer::RoguelikeUpgrade::BossBalancedMid:
        default:
            profile.color = { 0.86f, 0.74f, 0.40f, 1.0f };
            profile.ultimateInterval = 4;
            break;
        }

        return profile;
    }

    DirectX::XMFLOAT4 GetBossAttackAccentColor(int archetype)
    {
        switch (ClampInt(
            archetype,
            Transfer::RoguelikeUpgrade::BossHeavyMelee,
            Transfer::RoguelikeUpgrade::BossFinalBarrage))
        {
        case Transfer::RoguelikeUpgrade::BossHeavyMelee:
            return { 1.00f, 0.42f, 0.18f, 1.0f };

        case Transfer::RoguelikeUpgrade::BossLightRanged:
            return { 0.28f, 0.92f, 1.00f, 1.0f };

        case Transfer::RoguelikeUpgrade::BossSwiftDebuff:
            return { 0.90f, 0.36f, 1.00f, 1.0f };

        case Transfer::RoguelikeUpgrade::BossFinalBarrage:
            return { 1.00f, 0.12f, 0.16f, 1.0f };

        case Transfer::RoguelikeUpgrade::BossBalancedMid:
        default:
            return { 1.00f, 0.82f, 0.28f, 1.0f };
        }
    }

    DirectX::XMFLOAT4 GetBossZoneOutlineColor(int archetype)
    {
        const DirectX::XMFLOAT4 accent = GetBossAttackAccentColor(archetype);
        return LerpColor(accent, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.35f);
    }

    DirectX::XMFLOAT4 GetBossVisualTint(int archetype, bool isTelegraph)
    {
        const DirectX::XMFLOAT4 accent = GetBossAttackAccentColor(archetype);
        if (isTelegraph)
        {
            return LerpColor(accent, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.45f);
        }
        return LerpColor(accent, { 1.0f, 1.0f, 1.0f, 1.0f }, 0.20f);
    }

    void ApplyFinalBossScriptGlobals(BossProfile& profile, const BossAttackScript::Profile& script)
    {
        profile.hpScale = ClampRange(script.hpScale, 0.10f, 8.0f);
        profile.guardScale = ClampRange(script.guardScale, 0.10f, 8.0f);
        profile.sizeScale = ClampRange(script.sizeScale, 0.20f, 3.0f);
        profile.cooldownScale = ClampRange(script.cooldownScale, 0.10f, 4.0f);
        profile.telegraphScale = ClampRange(script.telegraphScale, 0.10f, 4.0f);
        profile.damageScale = ClampRange(script.damageScale, 0.10f, 8.0f);
        profile.startsSpecial = script.startsSpecial;
    }

    /**
     * @brief 現在進行中ノードに対応するボス種別を返します。
     * @param tran Transfer シングルトンです。
     * @return BossArchetype の値です。
     */
    int ResolveCurrentBossArchetype(const Transfer& tran)
    {
        const int currentStageType = tran.GetCurrentRunStageType();
        if (currentStageType == Transfer::RoguelikeUpgrade::StageFinalBoss)
        {
            return ClampInt(
                tran.roguelike.finalBossType,
                Transfer::RoguelikeUpgrade::BossHeavyMelee,
                Transfer::RoguelikeUpgrade::BossFinalBarrage);
        }

        if (currentStageType == Transfer::RoguelikeUpgrade::StageBoss)
        {
            const int stageIndex = ClampInt(tran.roguelike.currentStageIndex, 0, tran.GetRunStageCount() - 1);
            const int mapNumber = tran.GetRunStageMapNumberAt(stageIndex);
            const int bossSlot = ClampInt(
                mapNumber - 1,
                0,
                Transfer::RoguelikeUpgrade::kRegularBossSlotCount - 1);
            return ClampInt(
                tran.roguelike.regularBossOrder[bossSlot],
                Transfer::RoguelikeUpgrade::BossHeavyMelee,
                Transfer::RoguelikeUpgrade::BossSwiftDebuff);
        }

        return Transfer::RoguelikeUpgrade::BossBalancedMid;
    }

    /**
     * @brief ボス種別の重みに従って次の通常攻撃種別を抽選します。
     * @param profile ボス種別設定です。
     * @return 0:DashNarrow 1:DashWide 2:RandomRain 3:Summon 4:TrackingDrop です。
     */
    int PickWeightedBossAttack(const BossProfile& profile)
    {
        int totalWeight = 0;
        for (int i = 0; i < 5; ++i)
        {
            if (profile.topLevelWeights[i] > 0)
            {
                totalWeight += profile.topLevelWeights[i];
            }
        }
        if (totalWeight <= 0)
        {
            return 4;
        }

        int roll = RandomRangeInt(1, totalWeight);
        for (int i = 0; i < 5; ++i)
        {
            const int weight = (profile.topLevelWeights[i] > 0) ? profile.topLevelWeights[i] : 0;
            if (weight <= 0)
            {
                continue;
            }
            roll -= weight;
            if (roll <= 0)
            {
                return i;
            }
        }
        return 4;
    }

    /**
     * @brief Linearly interpolates a 3D vector.
     * @param a Start value.
     * @param b End value.
     * @param t Blend factor.
     * @return Interpolated vector.
     */
    DirectX::XMFLOAT3 LerpFloat3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t)
    {
        const float rate = Clamp01(t);
        return {
            a.x + (b.x - a.x) * rate,
            a.y + (b.y - a.y) * rate,
            a.z + (b.z - a.z) * rate
        };
    }

    float DegToRad(float deg)
    {
        return deg * (DirectX::XM_PI / 180.0f);
    }

    DirectX::XMFLOAT3 RotateOffsetY(const DirectX::XMFLOAT3& value, float yawDeg)
    {
        const float rad = DegToRad(yawDeg);
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        return {
            value.x * c + value.z * s,
            value.y,
            -value.x * s + value.z * c
        };
    }

    DirectX::XMFLOAT3 ClampArenaPos(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, float stageHalf)
    {
        const float hx = size.x * 0.5f;
        const float hz = size.z * 0.5f;
        return {
            ClampRange(pos.x, -stageHalf + hx, stageHalf - hx),
            pos.y,
            ClampRange(pos.z, -stageHalf + hz, stageHalf - hz)
        };
    }

    struct ScriptedColliderZone
    {
        DirectX::XMFLOAT3 center = { 0.0f, 0.05f, 0.0f };
        DirectX::XMFLOAT3 size = { 1.0f, 0.10f, 1.0f };
        float yawDeg = 0.0f;
        int shape = BossAttackScript::ColliderShapeBox;
        DirectX::XMFLOAT3 startPos = { 0.0f, 0.05f, 0.0f };
        DirectX::XMFLOAT3 endPos = { 0.0f, 0.05f, 0.0f };
        bool hasPath = false;
    };

    DirectX::XMFLOAT3 ResolveRuntimePoint(int mode,
                                          const DirectX::XMFLOAT3& value,
                                          float randomRadius,
                                          const DirectX::XMFLOAT3& anchor,
                                          const DirectX::XMFLOAT3& playerPos,
                                          float facingYawDeg)
    {
        switch (mode)
        {
        case BossAttackScript::ColliderStartCurrent:
        {
            const DirectX::XMFLOAT3 offset = RotateOffsetY(value, facingYawDeg);
            return {
                anchor.x + offset.x,
                anchor.y + offset.y,
                anchor.z + offset.z
            };
        }
        case BossAttackScript::ColliderStartPlayer:
            return playerPos;
        case BossAttackScript::ColliderStartPlayerAreaRandom:
        {
            const float angle = RandomRange(0.0f, DirectX::XM_2PI);
            const float distance = RandomRange(0.0f, MaxFloat(randomRadius, 0.0f));
            return {
                playerPos.x + std::cos(angle) * distance,
                playerPos.y,
                playerPos.z + std::sin(angle) * distance
            };
        }
        case BossAttackScript::ColliderStartAbsolute:
        default:
            return value;
        }
    }

    float DistancePointToSegment2D(const DirectX::XMFLOAT3& point,
                                   const DirectX::XMFLOAT3& startPos,
                                   const DirectX::XMFLOAT3& endPos)
    {
        const float segX = endPos.x - startPos.x;
        const float segZ = endPos.z - startPos.z;
        const float lenSq = segX * segX + segZ * segZ;
        if (lenSq <= 0.000001f)
        {
            const float dx = point.x - startPos.x;
            const float dz = point.z - startPos.z;
            return std::sqrt(dx * dx + dz * dz);
        }

        const float toPointX = point.x - startPos.x;
        const float toPointZ = point.z - startPos.z;
        float t = (toPointX * segX + toPointZ * segZ) / lenSq;
        t = ClampRange(t, 0.0f, 1.0f);
        const float closestX = startPos.x + segX * t;
        const float closestZ = startPos.z + segZ * t;
        const float dx = point.x - closestX;
        const float dz = point.z - closestZ;
        return std::sqrt(dx * dx + dz * dz);
    }

    ScriptedColliderZone BuildScriptedColliderZone(const BossAttackScript::Collider& collider,
                                                   const DirectX::XMFLOAT3& anchor,
                                                   const DirectX::XMFLOAT3& playerPos,
                                                   float facingYawDeg,
                                                   float stageHalf,
                                                   float minimumHeight,
                                                   bool allowEndPosition)
    {
        ScriptedColliderZone zone;
        const float baseYawDeg =
            (collider.startMode == BossAttackScript::ColliderStartCurrent) ? facingYawDeg : 0.0f;
        zone.shape = BossAttackScript::NormalizeColliderShape(collider.shape);

        DirectX::XMFLOAT3 startPos = ResolveRuntimePoint(
            collider.startMode,
            collider.startPos,
            collider.startRandomRadius,
            anchor,
            playerPos,
            facingYawDeg);
        startPos = ClampArenaPos(startPos, collider.startSize, stageHalf);

        DirectX::XMFLOAT3 endPos = startPos;
        if (allowEndPosition && collider.useEndPosition)
        {
            switch (BossAttackScript::NormalizeColliderEndMode(collider.endMode))
            {
            case BossAttackScript::ColliderEndCurrentRelative:
            {
                const DirectX::XMFLOAT3 endOffset = RotateOffsetY(collider.endPos, facingYawDeg);
                endPos = {
                    anchor.x + endOffset.x,
                    anchor.y + endOffset.y,
                    anchor.z + endOffset.z
                };
                break;
            }
            case BossAttackScript::ColliderEndPlayer:
                endPos = playerPos;
                break;
            case BossAttackScript::ColliderEndPlayerAreaRandom:
            {
                const float angle = RandomRange(0.0f, DirectX::XM_2PI);
                const float distance = RandomRange(0.0f, MaxFloat(collider.endRandomRadius, 0.0f));
                endPos = {
                    playerPos.x + std::cos(angle) * distance,
                    playerPos.y,
                    playerPos.z + std::sin(angle) * distance
                };
                break;
            }
            case BossAttackScript::ColliderEndAbsolute:
            default:
                endPos = collider.endPos;
                break;
            }
            endPos = ClampArenaPos(endPos, collider.endSize, stageHalf);
        }

        zone.startPos = startPos;
        zone.endPos = endPos;
        zone.hasPath = allowEndPosition && collider.useEndPosition;

        if (allowEndPosition && collider.useEndPosition)
        {
            const float dx = endPos.x - startPos.x;
            const float dz = endPos.z - startPos.z;
            const float distance = std::sqrt(dx * dx + dz * dz);
            if (distance > 0.001f)
            {
                zone.center = {
                    (startPos.x + endPos.x) * 0.5f,
                    (startPos.y + endPos.y) * 0.5f,
                    (startPos.z + endPos.z) * 0.5f
                };
                zone.size = {
                    std::max(collider.startSize.x, collider.endSize.x),
                    std::max(collider.startSize.y, collider.endSize.y),
                    (zone.shape == BossAttackScript::ColliderShapeCircle)
                        ? std::max(collider.startSize.x, collider.endSize.x)
                        : distance
                };
                zone.yawDeg = std::atan2(dx, dz) * (180.0f / DirectX::XM_PI);
                zone.size.y = MaxFloat(zone.size.y, minimumHeight);
                return zone;
            }
        }

        zone.center = startPos;
        zone.size = collider.startSize;
        zone.yawDeg = baseYawDeg;
        zone.size.y = MaxFloat(zone.size.y, minimumHeight);
        return zone;
    }

    int ResolveProfileTypeFromBossArchetype(int bossArchetype)
    {
        switch (bossArchetype)
        {
        case Transfer::RoguelikeUpgrade::BossHeavyMelee:
            return BossAttackScript::ProfileHeavyMelee;
        case Transfer::RoguelikeUpgrade::BossLightRanged:
            return BossAttackScript::ProfileLightRanged;
        case Transfer::RoguelikeUpgrade::BossSwiftDebuff:
            return BossAttackScript::ProfileSwiftDebuff;
        case Transfer::RoguelikeUpgrade::BossFinalBarrage:
            return BossAttackScript::ProfileFinalBarrage;
        case Transfer::RoguelikeUpgrade::BossBalancedMid:
        default:
            return BossAttackScript::ProfileBalancedMid;
        }
    }

    /**
     * @brief Builds an AABB from a center and size.
     * @param center Box center.
     * @param size Box size.
     * @return Constructed box.
     */
    Collision::Box MakeAabb(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size)
    {
        Collision::Box box{};
        box.center = center;
        box.size = size;
        return box;
    }

    /**
     * @brief Performs AABB vs AABB hit detection.
     * @param a First box.
     * @param b Second box.
     * @return True when the boxes overlap.
     */
    bool HitAabb(const Collision::Box& a, const Collision::Box& b)
    {
        return Collision::Hit(a, b).isHit;
    }

    bool HitZone(const Collision::Box& box, const BossController::AttackZone& zone)
    {
        const float halfHeightA = box.size.y * 0.5f;
        const float halfHeightB = zone.size.y * 0.5f;
        if (std::fabs(box.center.y - zone.center.y) > (halfHeightA + halfHeightB))
        {
            return false;
        }

        if (zone.shape == BossAttackScript::ColliderShapeCircle)
        {
            const float playerRadius = MaxFloat(box.size.x, box.size.z) * 0.5f;
            const float zoneRadius = zone.size.x * 0.5f;
            const float distance = zone.hasPath
                ? DistancePointToSegment2D(box.center, zone.startPos, zone.endPos)
                : DistancePointToSegment2D(box.center, zone.center, zone.center);
            return distance <= (zoneRadius + playerRadius);
        }

        if (std::fabs(zone.yawDeg) <= 0.01f)
        {
            return HitAabb(box, MakeAabb(zone.center, zone.size));
        }

        const float dx = box.center.x - zone.center.x;
        const float dz = box.center.z - zone.center.z;
        const float rad = DegToRad(zone.yawDeg);
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        const float localX = dx * c - dz * s;
        const float localZ = dx * s + dz * c;
        const float limitX = zone.size.x * 0.5f + box.size.x * 0.5f;
        const float limitZ = zone.size.z * 0.5f + box.size.z * 0.5f;
        return (std::fabs(localX) <= limitX) && (std::fabs(localZ) <= limitZ);
    }

    /**
     * @brief Draws a flat marker texture on the ground with a tint.
     * @param texture Marker texture.
     * @param pos Ground position.
     * @param size Draw size.
     * @param color Tint color.
     */
    void DrawAttackMarkerTintLocal(Texture* texture,
                                   const DirectX::XMFLOAT3& pos,
                                   const DirectX::XMFLOAT3& size,
                                   const DirectX::XMFLOAT4& color,
                                   float yawDeg = 0.0f)
    {
        if (!texture) return;

        const float markerY = 0.002f;
        DirectX::XMMATRIX r =
            DirectX::XMMatrixRotationZ(DegToRad(yawDeg)) *
            DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2);
        DirectX::XMMATRIX t = DirectX::XMMatrixTranslation(pos.x, markerY, pos.z);
        DirectX::XMFLOAT4X4 world{};
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(r * t));

        Sprite::SetWorld(world);
        Sprite::SetSize({ size.x, size.z });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos({ 0.0f, 0.0f });
        Sprite::SetUVScale({ 1.0f, 1.0f });
        Sprite::SetColor(color);
        Sprite::SetTexture(texture);
        Sprite::Draw();
    }

    /**
     * @brief Draws a billboard sprite that faces the active camera.
     * @param texture Texture to draw.
     * @param camera Camera used for the billboard orientation.
     * @param pos Bottom position.
     * @param size Sprite size.
     * @param color Tint color.
     */
    void DrawBillboardSpriteLocal(Texture* texture,
                                  Camera* camera,
                                  const DirectX::XMFLOAT3& pos,
                                  const DirectX::XMFLOAT3& size,
                                  const DirectX::XMFLOAT4& color)
    {
        if (!texture) return;
        using namespace DirectX;

        XMMATRIX billboard = XMMatrixIdentity();
        if (camera)
        {
            XMFLOAT4X4 viewFloat = camera->GetViewMatrix(false);
            XMMATRIX viewMat = XMLoadFloat4x4(&viewFloat);
            XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);
            XMFLOAT4X4 invViewFloat{};
            XMStoreFloat4x4(&invViewFloat, invView);
            invViewFloat._41 = 0.0f;
            invViewFloat._42 = 0.0f;
            invViewFloat._43 = 0.0f;
            billboard = XMLoadFloat4x4(&invViewFloat);
        }

        const XMMATRIX t = billboard * XMMatrixTranslation(
            pos.x,
            pos.y + (size.y * 0.5f),
            pos.z);
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixTranspose(t));

        Sprite::SetWorld(world);
        Sprite::SetSize({ size.x, size.y });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos({ 0.0f, 0.0f });
        Sprite::SetUVScale({ 1.0f, 1.0f });
        Sprite::SetColor(color);
        Sprite::SetTexture(texture);
        Sprite::Draw();
    }

    /**
     * @brief Draws the boss HP and guard overlay in screen space.
     * @param hpRate Boss HP ratio.
     * @param guardRate Boss guard ratio.
     * @param isBroken Whether the boss is currently guard-broken.
     * @param barWidthRate HP bar width ratio.
     * @param barHeightRate HP bar height ratio.
     * @param guardOffsetX Guard bar X offset.
     * @param guardOffsetY Guard bar Y offset.
     * @param guardWidthRate Guard bar width ratio.
     * @param guardHeightRate Guard bar height ratio.
     */
    void DrawBossHpOverlayLocal(float hpRate,
                                float burnRate,
                                float guardRate,
                                bool isBroken,
                                float barWidthRate,
                                float barHeightRate,
                                float guardOffsetX,
                                float guardOffsetY,
                                float guardWidthRate,
                                float guardHeightRate,
                                int chainCount,
                                Texture* chainTexture)
    {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        if (!dl) return;

        const float widthRate = ClampRange(barWidthRate, 0.20f, 0.90f);
        const float heightRate = ClampRange(barHeightRate, 0.01f, 0.20f);
        const float barW = static_cast<float>(SCREEN_WIDTH) * widthRate;
        const float barH = static_cast<float>(SCREEN_HEIGHT) * heightRate;
        const float x = (static_cast<float>(SCREEN_WIDTH) - barW) * 0.5f;
        const float y = 12.0f;
        const float padding = 4.0f;
        const float radius = 6.0f;

        const ImVec2 frameMin(x, y);
        const ImVec2 frameMax(x + barW, y + barH);
        dl->AddRectFilled(frameMin, frameMax, IM_COL32(20, 20, 20, 220), radius);
        dl->AddRect(frameMin, frameMax, IM_COL32(230, 230, 230, 210), radius, 0, 2.0f);

        const float fillRate = Clamp01(hpRate);
        const float burnFillRate = ClampRange(burnRate, fillRate, 1.0f);
        const float burnInnerW = (barW - padding * 2.0f) * burnFillRate;
        const float innerW = (barW - padding * 2.0f) * fillRate;
        const ImVec2 fillMin(x + padding, y + padding);
        const ImVec2 fillMax(x + padding + innerW, y + barH - padding);
        if (burnInnerW > 0.0f)
        {
            dl->AddRectFilled(
                fillMin,
                ImVec2(x + padding + burnInnerW, y + barH - padding),
                IM_COL32(255, 140, 30, 225),
                radius * 0.6f);
        }
        if (innerW > 0.0f)
        {
            dl->AddRectFilled(fillMin, fillMax, IM_COL32(180, 30, 30, 220), radius * 0.6f);
        }

        if (chainTexture && chainTexture->GetResource() && chainCount > 0)
        {
            const float bindRate = Clamp01(static_cast<float>(chainCount) * 0.05f);
            const float innerBarWidth = barW - padding * 2.0f;
            const float chainWidth = innerBarWidth * bindRate;
            const float iconHeight = MaxFloat(10.0f, barH - padding * 2.0f);
            if (chainWidth > 0.0f)
            {
                const ImVec2 iconMin(x + padding + innerBarWidth - chainWidth, y + padding);
                const ImVec2 iconMax(x + padding + innerBarWidth, y + padding + iconHeight);
                dl->AddImage(
                    reinterpret_cast<ImTextureID>(chainTexture->GetResource()),
                    iconMin,
                    iconMax);
            }
        }

        const float guardWidth = static_cast<float>(SCREEN_WIDTH) * ClampRange(guardWidthRate, 0.10f, 0.90f);
        const float guardHeight = static_cast<float>(SCREEN_HEIGHT) * ClampRange(guardHeightRate, 0.005f, 0.10f);
        const float guardX = ((static_cast<float>(SCREEN_WIDTH) - guardWidth) * 0.5f) + guardOffsetX;
        const float guardY = y + barH + guardOffsetY;
        const float guardBarH = MaxFloat(6.0f, guardHeight);
        const ImVec2 guardMin(guardX, guardY);
        const ImVec2 guardMaxV(guardX + guardWidth, guardY + guardBarH);
        dl->AddRectFilled(guardMin, guardMaxV, IM_COL32(18, 18, 18, 210), radius * 0.45f);
        dl->AddRect(guardMin, guardMaxV, IM_COL32(210, 210, 210, 180), radius * 0.45f, 0, 1.5f);

        const float guardFillRate = Clamp01(guardRate);
        const float guardInnerW = (guardWidth - padding * 2.0f) * guardFillRate;
        const ImVec2 guardFillMin(guardX + padding, guardY + padding * 0.35f);
        const ImVec2 guardFillMax(
            guardX + padding + guardInnerW,
            guardY + guardBarH - padding * 0.35f);
        if (guardInnerW > 0.0f)
        {
            const ImU32 guardColor = isBroken
                ? IM_COL32(130, 130, 130, 220)
                : IM_COL32(50, 175, 255, 220);
            dl->AddRectFilled(guardFillMin, guardFillMax, guardColor, radius * 0.3f);
        }
        dl->AddText(ImVec2(guardX + 8.0f, guardY - 16.0f), IM_COL32(180, 225, 255, 220), "GUARD");

        dl->AddText(ImVec2(x + 8.0f, y - 18.0f), IM_COL32(255, 255, 255, 230), "BOSS");
        if (isBroken)
        {
            dl->AddText(ImVec2(guardX + guardWidth - 78.0f, guardY - 16.0f), IM_COL32(255, 220, 120, 230), "BROKEN");
        }
    }

    /**
     * @brief Replaces a texture pointer with a freshly loaded texture.
     * @param texture Target texture pointer.
     * @param path Texture path.
     * @param label Error label used when loading fails.
     */
    void ReplaceTexture(Texture*& texture, const char* path, const char* label)
    {
        if (texture)
        {
            delete texture;
            texture = nullptr;
        }

        texture = new Texture();
        if (!texture || FAILED(texture->Create(path)))
        {
            MessageBox(NULL, label, "Error", MB_OK);
        }
    }

    /**
     * @brief Releases a heap-allocated texture pointer.
     * @param texture Texture pointer to release.
     */
    void ReleaseTexturePtr(Texture*& texture)
    {
        if (texture)
        {
            delete texture;
            texture = nullptr;
        }
    }

    /**
     * @brief Appends a new telegraph zone to the given list.
     * @param zones Destination vector.
     * @param center Zone center.
     * @param size Zone size.
     * @param color Zone draw color.
     * @param revealStart Normalized reveal start time.
     * @param safeZone Whether the zone is safe instead of dangerous.
     */
    void PushAttackZone(std::vector<BossController::AttackZone>& zones,
                        const DirectX::XMFLOAT3& center,
                        const DirectX::XMFLOAT3& size,
                        const DirectX::XMFLOAT4& color,
                        float revealStart,
                        float yawDeg,
                        int shape,
                        const DirectX::XMFLOAT3& startPos,
                        const DirectX::XMFLOAT3& endPos,
                        bool hasPath,
                        bool safeZone)
    {
        BossController::AttackZone zone;
        zone.center = center;
        zone.size = size;
        zone.color = color;
        zone.revealStart = revealStart;
        zone.yawDeg = yawDeg;
        zone.shape = shape;
        zone.startPos = startPos;
        zone.endPos = endPos;
        zone.hasPath = hasPath;
        zone.safeZone = safeZone;
        zones.push_back(zone);
    }

    void PushAttackZone(std::vector<BossController::AttackZone>& zones,
                        const DirectX::XMFLOAT3& center,
                        const DirectX::XMFLOAT3& size,
                        const DirectX::XMFLOAT4& color,
                        float revealStart = 0.0f,
                        float yawDeg = 0.0f,
                        bool safeZone = false)
    {
        PushAttackZone(
            zones,
            center,
            size,
            color,
            revealStart,
            yawDeg,
            BossAttackScript::ColliderShapeBox,
            center,
            center,
            false,
            safeZone);
    }

    /**
     * @brief Spawns one temporary falling rock visual.
     * @param rocks Destination vector.
     * @param center Ground position.
     * @param span Base rock size.
     */
    void SpawnRockVisual(std::vector<BossController::FallingRock>& rocks,
                         const DirectX::XMFLOAT3& center,
                         float span,
                         float duration = 0.28f,
                         float spawnHeight = 2.8f,
                         float spinDegPerSec = 0.0f,
                         bool billboard = true)
    {
        BossController::FallingRock rock;
        const float safeSpan = SafeSpan(span, 1.0f);
        rock.pos = { center.x, 0.0f, center.z };
        rock.size = { safeSpan, safeSpan, safeSpan };
        rock.duration = ClampRange(duration, 0.05f, 10.0f);
        rock.timer = rock.duration;
        rock.spawnHeight = MaxFloat(spawnHeight, 0.0f);
        rock.spinDegPerSec = spinDegPerSec;
        rock.angleDeg = 0.0f;
        rock.billboard = billboard;
        rocks.push_back(rock);
    }
}

/**
 * @brief Resets all boss battle state for a new scene start.
 * @param playerSize Player size used as the base for boss scaling.
 * @param bossSizeAreaScale Size multiplier for the boss.
 * @param bossMaxHp Maximum HP to assign.
 * @param bossGuardInitialMax Initial guard value.
 */
void BossController::ResetForScene(const DirectX::XMFLOAT3& playerSize,
                                   float bossSizeAreaScale,
                                   int bossMaxHp,
                                   float bossGuardInitialMax)
{
    const float areaScale = ClampRange(bossSizeAreaScale, 4.0f, 12.0f);
    const float bossScale = std::sqrt(areaScale);
    size = {
        playerSize.x * bossScale,
        playerSize.y * bossScale,
        playerSize.z * bossScale
    };
    pos = { 0.0f, 0.0f, 0.0f };
    color = { 0.86f, 0.74f, 0.40f, 1.0f };
    maxHp = ClampInt(bossMaxHp, 1, 9999);
    hp = maxHp;
    phase = 1;
    archetype = Transfer::RoguelikeUpgrade::BossBalancedMid;
    lastHitSwingId = -1;
    scriptEntryIndex = -1;
    attackDamageScale = 1.0f;
    scriptedAttack = false;
    attackKind = AttackKindDashNarrow;
    attackPattern = AttackPatternVertical;
    attackFacingYawDeg = 0.0f;
    attackLane.center = { 0.0f, 0.0f, 0.0f };
    attackLane.size = { 0.0f, 0.0f, 0.0f };
    attackLane.pattern = attackPattern;
    attackZones.clear();
    fallingRocks.clear();
    guardMax = ClampRange(bossGuardInitialMax, 1.0f, 200.0f);
    guard = guardMax;
    breakRecoverTimer = 0.0f;
    hpDamageCarry = 0.0f;
    attackState = AttackIdle;
    attackStateTimer = 0.0f;
    attackTelegraphDuration = 0.0f;
    attackExecuteDuration = 0.0f;
    attackJumpOutOverrideSec = -1.0f;
    attackCooldownTimer = 0.0f;
    attackRepeatsRemaining = 0;
    attackCycleCount = 0;
    battleTimer = 0.0f;
    finalLastAttackIndex = -1;
    finalLethalTriggered = false;
    finalPhaseTransitionPending = false;
    dashStartPos = { 0.0f, 0.0f, 0.0f };
    dashEndPos = { 0.0f, 0.0f, 0.0f };
    specialUnlocked = false;
    forceUltimatePending = false;
    isBroken = false;
    attackResolved = false;
    jumpedOut = false;
    requiresArenaReset = true;
}

/**
 * @brief Loads the boss base texture.
 * @param path Texture path to load.
 */
void BossController::LoadTexture(const char* path)
{
    ReplaceTexture(texture, path, "Texture load failed.\nBoss texture");
}

/**
 * @brief Loads the falling rock texture.
 * @param path Texture path to load.
 */
void BossController::LoadRockTexture(const char* path)
{
    ReplaceTexture(rockTexture, path, "Texture load failed.\nBoss rock texture");
}

/**
 * @brief Loads the broken-state texture.
 * @param path Texture path to load.
 */
void BossController::LoadBrokenTexture(const char* path)
{
    ReplaceTexture(brokenTexture, path, "Texture load failed.\nBoss broken texture");
}

/**
 * @brief Releases the boss base texture.
 */
void BossController::ReleaseTexture()
{
    ReleaseTexturePtr(texture);
}

/**
 * @brief Releases the falling rock texture.
 */
void BossController::ReleaseRockTexture()
{
    ReleaseTexturePtr(rockTexture);
}

/**
 * @brief Releases the broken-state texture.
 */
void BossController::ReleaseBrokenTexture()
{
    ReleaseTexturePtr(brokenTexture);
}

/**
 * @brief Reinitializes the boss using the current gameplay tuning.
 */
void SceneGame::InitializeBossForScene()
{
    auto& tran = Transfer::GetInstance();
    const int bossArchetype = ResolveCurrentBossArchetype(tran);
    const int profileType = ResolveProfileTypeFromBossArchetype(bossArchetype);
    const char* scriptPath = (profileType == BossAttackScript::ProfileFinalBarrage)
        ? BossAttackScript::GetFinalBossPath()
        : BossAttackScript::GetDefaultPath();
    m_finalBossScript = BossAttackScript::MakeDefaultProfile(profileType);
    BossAttackScript::LoadProfile(m_finalBossScript, profileType, scriptPath);
    BossProfile bossProfile = MakeBossProfile(bossArchetype);
    ApplyFinalBossScriptGlobals(bossProfile, m_finalBossScript);
    m_finalBossScriptCursor = 0;
    // Difficulty changes the effective HP before the boss is reset.
    const int difficultyPreset = tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset);
    const float bossHpScale =
        tran.GetBossHpScaleByDifficulty(difficultyPreset) *
        tran.GetBossHpScaleByUpgradeProgress();
    const int effectiveBossMaxHp = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(tran.gameplay.bossMaxHp) * bossHpScale * bossProfile.hpScale)),
        1,
        9999);
    const float initialGuardMax = ClampRange(tran.gameplay.bossGuardInitialMax * bossProfile.guardScale, 1.0f, 200.0f);
    m_boss.ResetForScene(
        tran.player.size,
        ClampRange(tran.gameplay.bossSizeAreaScale * bossProfile.sizeScale, 4.0f, 12.0f),
        effectiveBossMaxHp,
        initialGuardMax);
    m_boss.archetype = bossArchetype;
    m_boss.color = bossProfile.color;
    m_boss.specialUnlocked = bossProfile.startsSpecial;
    m_boss.forceUltimatePending = bossProfile.startsSpecial;
    if (bossProfile.startsSpecial)
    {
        m_boss.phase = 2;
    }
    m_lastBossSkillProjectileId = -1;
    m_bossSkillContactCooldownTimer = 0.0f;
    m_bossCurseTimer = 0.0f;
}

/**
 * @brief Loads textures used only during the boss battle.
 */
void SceneGame::LoadBossResources()
{
    m_boss.LoadTexture("Assets/Texture/Chracter/genbaneko.png");
    m_boss.LoadRockTexture("Assets/Texture/Game/rock.png");
    m_boss.LoadBrokenTexture("Assets/Texture/Game/Broken.png");
}

/**
 * @brief Releases boss-only textures.
 */
void SceneGame::ReleaseBossResources()
{
    m_boss.ReleaseBrokenTexture();
    m_boss.ReleaseRockTexture();
    m_boss.ReleaseTexture();
}

/**
 * @brief Keeps boss debug state and boss tuning values in a valid range.
 * @param stageSize Current stage size.
 * @return Always false here; the return value matches the caller's flow contract.
 */
bool SceneGame::UpdateBossDebugSetup(float stageSize)
{
    // Outside boss debug mode, there is nothing to maintain.
    if (!m_isBossBattleDebug)
    {
        return false;
    }

    auto& tran = Transfer::GetInstance();
    const int bossArchetype = ResolveCurrentBossArchetype(tran);
    BossProfile bossProfile = MakeBossProfile(bossArchetype);
    ApplyFinalBossScriptGlobals(bossProfile, m_finalBossScript);
    m_boss.archetype = bossArchetype;
    m_boss.color = bossProfile.color;
    if (m_boss.requiresArenaReset)
    {
        // Entering boss mode clears regular enemies and projectiles exactly once.
        EnsureEnemyCount(0, stageSize);
        m_enemyProjectiles.clear();
        m_skillProjectiles.clear();
        m_orbitSkill.active = false;
        m_lastBossSkillProjectileId = -1;
        m_bossSkillContactCooldownTimer = 0.0f;
        m_requestedEnemyCount = 0;
        m_boss.requiresArenaReset = false;
    }
    tran.gameplayDebug.bossBattleActive = 1;
    tran.gameplayDebug.bossHp = static_cast<float>(m_boss.hp);
    tran.gameplayDebug.bossMaxHp = static_cast<float>(m_boss.maxHp);
    tran.gameplayDebug.bossGuard = m_boss.guard;
    tran.gameplayDebug.bossGuardMax = m_boss.guardMax;
    tran.gameplayDebug.bossBroken = m_boss.isBroken ? 1 : 0;

    // Clamp all exposed tuning values so live ImGui edits cannot push the boss into invalid state.
    tran.gameplay.bossHpBarWidthRate = ClampRange(tran.gameplay.bossHpBarWidthRate, 0.20f, 0.90f);
    tran.gameplay.bossHpBarHeightRate = ClampRange(tran.gameplay.bossHpBarHeightRate, 0.01f, 0.20f);
    tran.gameplay.bossGuardBarOffsetX = ClampRange(tran.gameplay.bossGuardBarOffsetX, -960.0f, 960.0f);
    tran.gameplay.bossGuardBarOffsetY = ClampRange(tran.gameplay.bossGuardBarOffsetY, -120.0f, 320.0f);
    tran.gameplay.bossGuardBarWidthRate = ClampRange(tran.gameplay.bossGuardBarWidthRate, 0.10f, 0.90f);
    tran.gameplay.bossGuardBarHeightRate = ClampRange(tran.gameplay.bossGuardBarHeightRate, 0.005f, 0.10f);
    tran.gameplay.bossSizeAreaScale = ClampRange(tran.gameplay.bossSizeAreaScale, 4.0f, 12.0f);
    tran.gameplay.bossAttackJumpOutTime = ClampRange(tran.gameplay.bossAttackJumpOutTime, 0.0f, 4.0f);
    tran.gameplay.bossAttackDashDuration = ClampRange(tran.gameplay.bossAttackDashDuration, 0.05f, 2.0f);
    tran.gameplay.bossAttackCooldown = ClampRange(tran.gameplay.bossAttackCooldown, 0.0f, 6.0f);
    tran.gameplay.bossAttackTelegraph = ClampRange(tran.gameplay.bossAttackTelegraph, 0.10f, 4.0f);
    tran.gameplay.bossAttackLanePlayerScale = ClampRange(tran.gameplay.bossAttackLanePlayerScale, 0.5f, 8.0f);
    if (tran.gameplay.bossAttackDamage < 0.0f) tran.gameplay.bossAttackDamage = 0.0f;
    tran.gameplay.bossGuardInitialMax = ClampRange(tran.gameplay.bossGuardInitialMax, 1.0f, 200.0f);
    tran.gameplay.bossGuardFinalMax = ClampRange(tran.gameplay.bossGuardFinalMax, 1.0f, 200.0f);
    if (tran.gameplay.bossGuardFinalMax < tran.gameplay.bossGuardInitialMax)
    {
        tran.gameplay.bossGuardFinalMax = tran.gameplay.bossGuardInitialMax;
    }
    tran.gameplay.bossGuardRecoverStep = ClampRange(tran.gameplay.bossGuardRecoverStep, 0.0f, 50.0f);
    tran.gameplay.bossDamageScaleNormal = ClampRange(tran.gameplay.bossDamageScaleNormal, 0.0f, 5.0f);
    tran.gameplay.bossDamageScaleBroken = ClampRange(tran.gameplay.bossDamageScaleBroken, 0.0f, 10.0f);
    tran.gameplay.bossBreakRecoverSec = ClampRange(tran.gameplay.bossBreakRecoverSec, 1.0f, 30.0f);
    tran.gameplay.bossDashNarrowTelegraph = ClampRange(tran.gameplay.bossDashNarrowTelegraph, 0.10f, 8.0f);
    tran.gameplay.bossDashWideTelegraph = ClampRange(tran.gameplay.bossDashWideTelegraph, 0.10f, 8.0f);
    tran.gameplay.bossDashWideWidthRate = ClampRange(tran.gameplay.bossDashWideWidthRate, 0.10f, 1.00f);
    tran.gameplay.bossRandomRainCount = ClampInt(tran.gameplay.bossRandomRainCount, 1, 16);
    tran.gameplay.bossRandomRainTelegraph = ClampRange(tran.gameplay.bossRandomRainTelegraph, 0.10f, 8.0f);
    if (tran.gameplay.bossRandomRainRadiusScale < 0.25f) tran.gameplay.bossRandomRainRadiusScale = 0.25f;
    tran.gameplay.bossSummonMin = ClampInt(tran.gameplay.bossSummonMin, 1, 32);
    tran.gameplay.bossSummonMax = ClampInt(tran.gameplay.bossSummonMax, 1, 32);
    if (tran.gameplay.bossSummonMax < tran.gameplay.bossSummonMin) tran.gameplay.bossSummonMax = tran.gameplay.bossSummonMin;
    tran.gameplay.bossSummonTelegraph = ClampRange(tran.gameplay.bossSummonTelegraph, 0.10f, 8.0f);
    tran.gameplay.bossTrackingDropCount = ClampInt(tran.gameplay.bossTrackingDropCount, 1, 16);
    tran.gameplay.bossTrackingDropTelegraph = ClampRange(tran.gameplay.bossTrackingDropTelegraph, 0.10f, 8.0f);
    if (tran.gameplay.bossTrackingDropRadiusScale < 0.5f) tran.gameplay.bossTrackingDropRadiusScale = 0.5f;
    tran.gameplay.bossUltimateCrossTelegraph = ClampRange(tran.gameplay.bossUltimateCrossTelegraph, 0.10f, 8.0f);
    if (tran.gameplay.bossUltimateCrossLaneScale < 0.25f) tran.gameplay.bossUltimateCrossLaneScale = 0.25f;
    tran.gameplay.bossUltimateStompCount = ClampInt(tran.gameplay.bossUltimateStompCount, 1, 16);
    tran.gameplay.bossUltimateStompTelegraph = ClampRange(tran.gameplay.bossUltimateStompTelegraph, 0.10f, 12.0f);
    tran.gameplay.bossUltimateStompRepeatTelegraph = ClampRange(tran.gameplay.bossUltimateStompRepeatTelegraph, 0.10f, 12.0f);
    if (tran.gameplay.bossUltimateStompRadiusScale < 0.5f) tran.gameplay.bossUltimateStompRadiusScale = 0.5f;
    tran.gameplay.bossUltimateFieldTelegraph = ClampRange(tran.gameplay.bossUltimateFieldTelegraph, 0.10f, 12.0f);
    if (tran.gameplay.bossUltimateFieldSafeScale < 0.5f) tran.gameplay.bossUltimateFieldSafeScale = 0.5f;
    tran.gameplay.bossMaxHp = ClampInt(tran.gameplay.bossMaxHp, 1, 9999);
    const int difficultyPreset = tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset);
    const float bossHpScale =
        tran.GetBossHpScaleByDifficulty(difficultyPreset) *
        tran.GetBossHpScaleByUpgradeProgress();
    const int effectiveBossMaxHp = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(tran.gameplay.bossMaxHp) * bossHpScale * bossProfile.hpScale)),
        1,
        9999);

    // Preserve the current HP ratio when the effective maximum HP changes.
    if (m_boss.maxHp <= 0)
    {
        m_boss.maxHp = effectiveBossMaxHp;
        m_boss.hp = m_boss.maxHp;
    }
    else if (m_boss.maxHp != effectiveBossMaxHp)
    {
        const float hpRate = Clamp01(static_cast<float>(m_boss.hp) / static_cast<float>(m_boss.maxHp));
        m_boss.maxHp = effectiveBossMaxHp;
        m_boss.hp = ClampInt(static_cast<int>(std::ceil(hpRate * static_cast<float>(m_boss.maxHp))), 0, m_boss.maxHp);
    }
    else if (m_boss.hp > m_boss.maxHp)
    {
        m_boss.hp = m_boss.maxHp;
    }

    const float guardMin = ClampRange(tran.gameplay.bossGuardInitialMax * bossProfile.guardScale, 1.0f, 200.0f);
    const float guardMaxLimit = ClampRange(tran.gameplay.bossGuardFinalMax * bossProfile.guardScale, guardMin, 200.0f);
    const float clampedGuardMax = ClampRange(
        (m_boss.guardMax > 0.0f) ? m_boss.guardMax : guardMin,
        guardMin,
        guardMaxLimit);

    // Guard is also kept proportional when the cap changes live.
    if (std::fabs(clampedGuardMax - m_boss.guardMax) > 0.001f)
    {
        const float guardRate = (m_boss.guardMax > 0.01f)
            ? Clamp01(m_boss.guard / m_boss.guardMax)
            : 1.0f;
        m_boss.guardMax = clampedGuardMax;
        m_boss.guard = m_boss.isBroken ? 0.0f : (m_boss.guardMax * guardRate);
    }
    if (!m_boss.isBroken && m_boss.guard > m_boss.guardMax)
    {
        m_boss.guard = m_boss.guardMax;
    }

    const float bossScale = std::sqrt(ClampRange(tran.gameplay.bossSizeAreaScale * bossProfile.sizeScale, 4.0f, 12.0f));
    m_boss.size =
    {
        tran.player.size.x * bossScale,
        tran.player.size.y * bossScale,
        tran.player.size.z * bossScale
    };
    if (bossProfile.startsSpecial && m_boss.phase < 2)
    {
        m_boss.phase = 2;
        m_boss.specialUnlocked = true;
        m_boss.forceUltimatePending = true;
    }

    return false;
}

/**
 * @brief Updates boss combat state, applies boss attacks, and processes player hits on the boss.
 * @param stageSize Current stage size.
 * @param playerAttackDamage Player damage per hit before boss-side scaling.
 * @param applyPlayerDamage Callback used to damage the player.
 * @return True when the function triggers an immediate scene-flow exit.
 */
bool SceneGame::UpdateBossBattle(float stageSize,
                                 int playerAttackDamage,
                                 const std::function<bool(float)>& applyPlayerDamage)
{
    // When boss mode is inactive, clear debug values so the HUD does not show stale data.
    if (!m_isBossBattleDebug || !m_pPlayer)
    {
        auto& tran = Transfer::GetInstance();
        tran.gameplayDebug.bossHp = 0.0f;
        tran.gameplayDebug.bossMaxHp = 0.0f;
        tran.gameplayDebug.bossGuard = 0.0f;
        tran.gameplayDebug.bossGuardMax = 0.0f;
        tran.gameplayDebug.bossBroken = 0;
        return false;
    }

    auto& tran = Transfer::GetInstance();
    BossProfile bossProfile = MakeBossProfile(m_boss.archetype);
    ApplyFinalBossScriptGlobals(bossProfile, m_finalBossScript);
    const bool useFinalBossScript = !m_finalBossScript.attacks.empty();
    if (tran.gameplayDebug.bossHpEditRequest != 0)
    {
        const int requestedMaxHp = ClampInt(tran.gameplayDebug.bossMaxHpEditValue, 1, 9999);
        const int requestedHp = ClampInt(tran.gameplayDebug.bossHpEditValue, 0, requestedMaxHp);
        m_boss.maxHp = requestedMaxHp;
        m_boss.hp = requestedHp;
        if (m_boss.guardMax < 1.0f) m_boss.guardMax = 1.0f;
        if (m_boss.guard > m_boss.guardMax) m_boss.guard = m_boss.guardMax;
        tran.gameplayDebug.bossHpEditRequest = 0;
    }
    tran.gameplayDebug.bossHp = static_cast<float>(m_boss.hp);
    tran.gameplayDebug.bossMaxHp = static_cast<float>(m_boss.maxHp);
    tran.gameplayDebug.bossGuard = m_boss.guard;
    tran.gameplayDebug.bossGuardMax = m_boss.guardMax;
    tran.gameplayDebug.bossBroken = m_boss.isBroken ? 1 : 0;

    // Temporary rock visuals decay independently from the actual gameplay hit timing.
    for (auto& rock : m_boss.fallingRocks)
    {
        if (rock.timer > 0.0f)
        {
            rock.timer -= kBossFixedDt;
            if (rock.timer < 0.0f) rock.timer = 0.0f;
        }
    }
    m_boss.fallingRocks.erase(
        std::remove_if(
            m_boss.fallingRocks.begin(),
            m_boss.fallingRocks.end(),
            [](const BossController::FallingRock& rock) { return rock.timer <= 0.0f; }),
        m_boss.fallingRocks.end());

    // Snapshot all runtime tuning values into locals so the rest of the update uses one consistent frame view.
    const float stageHalf = stageSize * 0.5f;
    const float dashSec = ClampRange(tran.gameplay.bossAttackDashDuration * bossProfile.dashDurationScale, 0.05f, 2.0f);
    const float jumpOutSec = ClampRange(tran.gameplay.bossAttackJumpOutTime * bossProfile.jumpOutScale, 0.0f, 4.0f);
    const int difficultyPreset = tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset);
    const float difficultyBossCooldownScale = tran.GetBossCooldownScaleByDifficulty(difficultyPreset);
    const float phaseLowHpCooldownScale = (m_boss.phase >= 3) ? 0.60f : 1.0f;
    const float brokenCooldownScale = m_boss.isBroken ? 2.40f : 1.0f;
    const float cooldownSec = ClampRange(
        tran.gameplay.bossAttackCooldown * difficultyBossCooldownScale * phaseLowHpCooldownScale * brokenCooldownScale * bossProfile.cooldownScale,
        0.0f,
        6.0f);
    const float brokenTelegraphScale = m_boss.isBroken ? 1.35f : 1.0f;
    const float telegraphMultiplier = ClampRange(tran.gameplay.bossAttackTelegraph, 0.10f, 4.0f) * brokenTelegraphScale * bossProfile.telegraphScale;
    const float bossDamage = ((tran.gameplay.bossAttackDamage < 0.0f) ? 0.0f : tran.gameplay.bossAttackDamage) * bossProfile.damageScale;
    const float lanePlayerRatio = ClampRange(tran.gameplay.bossAttackLanePlayerScale, 0.5f, 8.0f);
    const float globalLaneScale = (lanePlayerRatio / 3.0f) * bossProfile.laneScale;
    const float playerWidth = SafeSpan(tran.player.size.x, 0.8f);
    const float playerDepth = SafeSpan(tran.player.size.z, playerWidth);
    const float playerSpan = MaxFloat(playerWidth, playerDepth);
    const float zoneHeight = MaxFloat(m_boss.size.y, tran.player.size.y);
    const float dashNarrowTelegraphSec = ClampRange(tran.gameplay.bossDashNarrowTelegraph, 0.10f, 8.0f) * telegraphMultiplier;
    const float dashWideTelegraphSec = ClampRange(tran.gameplay.bossDashWideTelegraph, 0.10f, 8.0f) * telegraphMultiplier;
    const float dashWideWidthRate = ClampRange(tran.gameplay.bossDashWideWidthRate, 0.10f, 1.00f);
    const int randomRainCount = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(tran.gameplay.bossRandomRainCount) * bossProfile.randomRainCountScale)),
        1,
        32);
    const float randomRainTelegraphSec = ClampRange(tran.gameplay.bossRandomRainTelegraph, 0.10f, 8.0f) * telegraphMultiplier;
    const float randomRainRadiusScale = ((tran.gameplay.bossRandomRainRadiusScale < 0.25f) ? 0.25f : tran.gameplay.bossRandomRainRadiusScale) * bossProfile.randomRainRadiusScale;
    const int summonBaseMin = ClampInt(tran.gameplay.bossSummonMin, 1, 32);
    const int summonBaseMax = ClampInt(
        (tran.gameplay.bossSummonMax < summonBaseMin) ? summonBaseMin : tran.gameplay.bossSummonMax,
        summonBaseMin,
        32);
    const int summonMin = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(summonBaseMin) * bossProfile.summonCountScale)),
        1,
        32);
    const int summonMax = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(summonBaseMax) * bossProfile.summonCountScale)),
        summonMin,
        32);
    const float summonTelegraphSec = ClampRange(tran.gameplay.bossSummonTelegraph, 0.10f, 8.0f) * telegraphMultiplier;
    const int trackingDropCount = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(tran.gameplay.bossTrackingDropCount) * bossProfile.trackingCountScale)),
        1,
        32);
    const float trackingDropTelegraphSec = ClampRange(tran.gameplay.bossTrackingDropTelegraph, 0.10f, 8.0f) * telegraphMultiplier;
    const float trackingDropRadiusScale = ((tran.gameplay.bossTrackingDropRadiusScale < 0.5f) ? 0.5f : tran.gameplay.bossTrackingDropRadiusScale) * bossProfile.trackingRadiusScale;
    const float ultimateCrossTelegraphSec = ClampRange(tran.gameplay.bossUltimateCrossTelegraph, 0.10f, 8.0f) * telegraphMultiplier;
    const float ultimateCrossLaneScale = (tran.gameplay.bossUltimateCrossLaneScale < 0.25f) ? 0.25f : tran.gameplay.bossUltimateCrossLaneScale;
    const int ultimateStompCount = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(tran.gameplay.bossUltimateStompCount) * bossProfile.stompCountScale)),
        1,
        32);
    const float ultimateStompTelegraphSec = ClampRange(tran.gameplay.bossUltimateStompTelegraph, 0.10f, 12.0f) * telegraphMultiplier;
    const float ultimateStompRepeatTelegraphSec = ClampRange(tran.gameplay.bossUltimateStompRepeatTelegraph, 0.10f, 12.0f) * telegraphMultiplier;
    const float ultimateStompRadiusScale = (tran.gameplay.bossUltimateStompRadiusScale < 0.5f) ? 0.5f : tran.gameplay.bossUltimateStompRadiusScale;
    const float ultimateFieldTelegraphSec = ClampRange(tran.gameplay.bossUltimateFieldTelegraph, 0.10f, 12.0f) * telegraphMultiplier;
    const float ultimateFieldSafeScale = ((tran.gameplay.bossUltimateFieldSafeScale < 0.5f) ? 0.5f : tran.gameplay.bossUltimateFieldSafeScale) * bossProfile.fieldSafeScale;
    const DirectX::XMFLOAT4 archetypeAccentColor = GetBossAttackAccentColor(m_boss.archetype);
    const DirectX::XMFLOAT4 dangerColor = LerpColor(
        archetypeAccentColor,
        { 1.0f, 0.18f, 0.12f, 1.0f },
        0.30f);
    const DirectX::XMFLOAT4 safeColor = { 0.20f, 0.95f, 0.35f, 1.0f };
    const DirectX::XMFLOAT4 safeOutlineColor = GetBossZoneOutlineColor(m_boss.archetype);
    const DirectX::XMFLOAT4 summonColor = LerpColor(
        archetypeAccentColor,
        { 1.0f, 0.78f, 0.20f, 1.0f },
        0.42f);
    const float normalDamageScale = ClampRange(tran.gameplay.bossDamageScaleNormal, 0.0f, 5.0f);
    const float brokenDamageScale = ClampRange(tran.gameplay.bossDamageScaleBroken, 0.0f, 10.0f);
    const float breakRecoverDurationSec = ClampRange(tran.gameplay.bossBreakRecoverSec, 1.0f, 30.0f);
    const float guardInitialMax = ClampRange(tran.gameplay.bossGuardInitialMax, 1.0f, 200.0f);
    const float guardFinalMax = ClampRange(
        (tran.gameplay.bossGuardFinalMax < guardInitialMax) ? guardInitialMax : tran.gameplay.bossGuardFinalMax,
        guardInitialMax,
        200.0f);
    const float guardRecoverStep = ClampRange(tran.gameplay.bossGuardRecoverStep, 0.0f, 50.0f);
    const float guardDamagePerHit = 1.0f;
    const int ultimateInterval = (m_boss.phase >= 3)
        ? ((bossProfile.ultimateInterval > 1) ? (bossProfile.ultimateInterval - 1) : 1)
        : bossProfile.ultimateInterval;
    const float bossHitStop = ClampRange(tran.gameplay.bossAttackHitStop, 0.0f, 0.20f);

    // Local helpers keep the long attack state machine readable.
    auto clampCenterToStage = [&](DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size)
    {
        const float halfX = size.x * 0.5f;
        const float halfZ = size.z * 0.5f;
        center.x = ClampRange(center.x, -stageHalf + halfX, stageHalf - halfX);
        center.z = ClampRange(center.z, -stageHalf + halfZ, stageHalf - halfZ);
    };

    auto clampPointToStage = [&](DirectX::XMFLOAT3& point, float margin = 0.10f)
    {
        point.x = ClampRange(point.x, -stageHalf + margin, stageHalf - margin);
        point.z = ClampRange(point.z, -stageHalf + margin, stageHalf - margin);
    };

    auto clampZoneSizeToStage = [&](DirectX::XMFLOAT3& size)
    {
        const float maxSpan = (stageSize > 0.10f) ? (stageSize - 0.05f) : 0.05f;
        size.x = ClampRange(size.x, 0.10f, maxSpan);
        size.z = ClampRange(size.z, 0.10f, maxSpan);
    };

    auto startTelegraph = [&](BossController::AttackKind kind, float telegraphDuration, float executeDuration)
    {
        m_boss.attackKind = kind;
        m_boss.attackState = BossController::AttackTelegraph;
        m_boss.attackStateTimer = 0.0f;
        m_boss.attackTelegraphDuration = telegraphDuration;
        m_boss.attackExecuteDuration = executeDuration;
        m_boss.attackJumpOutOverrideSec = -1.0f;
        m_boss.attackResolved = false;
        m_boss.jumpedOut = false;
        m_boss.attackDamageScale = 1.0f;
        m_boss.scriptEntryIndex = -1;
        m_boss.scriptedAttack = false;
        m_boss.attackZones.clear();
        m_boss.attackLane.center = { 0.0f, 0.0f, 0.0f };
        m_boss.attackLane.size = { 0.0f, 0.0f, 0.0f };
        m_boss.pos = { 0.0f, 0.0f, 0.0f };
    };

    auto beginDashAttack = [&](BossController::AttackKind kind, bool wideLane)
    {
        startTelegraph(kind, wideLane ? dashWideTelegraphSec : dashNarrowTelegraphSec, dashSec);

        m_boss.attackPattern = ((std::rand() & 1) == 0)
            ? BossController::AttackPatternVertical
            : BossController::AttackPatternHorizontal;
        const bool verticalLane = (m_boss.attackPattern == BossController::AttackPatternVertical);

        float laneThickness = wideLane
            ? (stageSize * dashWideWidthRate * globalLaneScale)
            : (playerSpan * lanePlayerRatio);
        if (verticalLane)
        {
            laneThickness = ClampRange(laneThickness, playerWidth, stageSize - playerWidth);
        }
        else
        {
            laneThickness = ClampRange(laneThickness, playerDepth, stageSize - playerDepth);
        }

        m_boss.attackLane.pattern = m_boss.attackPattern;
        m_boss.attackLane.center = {
            verticalLane ? tran.player.pos.x : 0.0f,
            tran.player.pos.y + zoneHeight * 0.5f,
            verticalLane ? 0.0f : tran.player.pos.z
        };
        m_boss.attackLane.size = verticalLane
            ? DirectX::XMFLOAT3{ laneThickness, zoneHeight, stageSize }
            : DirectX::XMFLOAT3{ stageSize, zoneHeight, laneThickness };
        clampCenterToStage(m_boss.attackLane.center, m_boss.attackLane.size);

        PushAttackZone(m_boss.attackZones, m_boss.attackLane.center, m_boss.attackLane.size, dangerColor);

        const float outMargin = 0.65f;
        if (verticalLane)
        {
            m_boss.dashStartPos = { m_boss.attackLane.center.x, 0.0f, stageHalf + m_boss.size.z * outMargin };
            m_boss.dashEndPos = { m_boss.attackLane.center.x, 0.0f, -stageHalf - m_boss.size.z * outMargin };
        }
        else
        {
            m_boss.dashStartPos = { stageHalf + m_boss.size.x * outMargin, 0.0f, m_boss.attackLane.center.z };
            m_boss.dashEndPos = { -stageHalf - m_boss.size.x * outMargin, 0.0f, m_boss.attackLane.center.z };
        }
    };

    auto beginRandomRain = [&]()
    {
        startTelegraph(BossController::AttackKindRandomRain, randomRainTelegraphSec, 0.30f);
        const float rockSpan = ClampRange(playerSpan * randomRainRadiusScale * globalLaneScale, 0.10f, stageSize - 0.05f);
        const float halfRockSpan = rockSpan * 0.5f;
        for (int i = 0; i < randomRainCount; ++i)
        {
            DirectX::XMFLOAT3 center = {
                RandomRange(-stageHalf + halfRockSpan, stageHalf - halfRockSpan),
                tran.player.pos.y + zoneHeight * 0.5f,
                RandomRange(-stageHalf + halfRockSpan, stageHalf - halfRockSpan)
            };
            PushAttackZone(m_boss.attackZones, center, { rockSpan, zoneHeight, rockSpan }, dangerColor);
        }
    };

    auto beginSummon = [&]()
    {
        startTelegraph(BossController::AttackKindSummon, summonTelegraphSec, 0.20f);
        m_boss.attackRepeatsRemaining = RandomRangeInt(summonMin, summonMax);
        DirectX::XMFLOAT3 zoneSize = { playerSpan * 4.0f * globalLaneScale, zoneHeight, playerSpan * 4.0f * globalLaneScale };
        clampZoneSizeToStage(zoneSize);
        PushAttackZone(
            m_boss.attackZones,
            { 0.0f, tran.player.pos.y + zoneHeight * 0.5f, 0.0f },
            zoneSize,
            summonColor);
    };

    auto beginTrackingDrop = [&](bool startNewSequence)
    {
        if (startNewSequence)
        {
            m_boss.attackRepeatsRemaining = trackingDropCount;
        }

        startTelegraph(BossController::AttackKindTrackingDrop, trackingDropTelegraphSec, 0.28f);
        DirectX::XMFLOAT3 center = {
            tran.player.pos.x,
            tran.player.pos.y + zoneHeight * 0.5f,
            tran.player.pos.z
        };
        DirectX::XMFLOAT3 size = {
            playerSpan * trackingDropRadiusScale * globalLaneScale,
            zoneHeight,
            playerSpan * trackingDropRadiusScale * globalLaneScale
        };
        clampZoneSizeToStage(size);
        clampCenterToStage(center, size);
        PushAttackZone(m_boss.attackZones, center, size, dangerColor);
    };

    auto beginUltimateCross = [&]()
    {
        startTelegraph(BossController::AttackKindUltimateCross, ultimateCrossTelegraphSec, 0.22f);
        const float laneWidth = ClampRange(
            playerSpan * ultimateCrossLaneScale * globalLaneScale,
            0.10f,
            stageSize - 0.05f);
        const float offsets[2] = { -stageSize / 6.0f, stageSize / 6.0f };
        for (int i = 0; i < 2; ++i)
        {
            PushAttackZone(
                m_boss.attackZones,
                { offsets[i], tran.player.pos.y + zoneHeight * 0.5f, 0.0f },
                { laneWidth, zoneHeight, stageSize },
                dangerColor);
            PushAttackZone(
                m_boss.attackZones,
                { 0.0f, tran.player.pos.y + zoneHeight * 0.5f, offsets[i] },
                { stageSize, zoneHeight, laneWidth },
                dangerColor);
        }
    };

    auto beginUltimateStomp = [&](bool startNewSequence)
    {
        if (startNewSequence)
        {
            m_boss.attackRepeatsRemaining = ultimateStompCount;
        }

        const float stompTelegraphSec = startNewSequence
            ? ultimateStompTelegraphSec
            : ultimateStompRepeatTelegraphSec;
        startTelegraph(BossController::AttackKindUltimateStomp, stompTelegraphSec, 0.32f);
        DirectX::XMFLOAT3 center = {
            tran.player.pos.x,
            tran.player.pos.y + zoneHeight * 0.5f,
            tran.player.pos.z
        };
        DirectX::XMFLOAT3 size = {
            playerSpan * ultimateStompRadiusScale * globalLaneScale,
            zoneHeight,
            playerSpan * ultimateStompRadiusScale * globalLaneScale
        };
        clampZoneSizeToStage(size);
        clampCenterToStage(center, size);
        PushAttackZone(m_boss.attackZones, center, size, dangerColor);
        m_boss.dashStartPos = m_boss.pos;
        m_boss.dashEndPos = { center.x, 0.0f, center.z };
    };

    auto beginUltimateField = [&]()
    {
        startTelegraph(BossController::AttackKindUltimateField, ultimateFieldTelegraphSec, 0.35f);

        const float safeSpan = ClampRange(
            playerSpan * ultimateFieldSafeScale * globalLaneScale,
            0.10f,
            stageSize - 0.05f);
        const DirectX::XMFLOAT3 safeSize = { safeSpan, zoneHeight, safeSpan };
        const DirectX::XMFLOAT3 safeCandidates[3] =
        {
            { 0.0f, tran.player.pos.y + zoneHeight * 0.5f, 0.0f },
            { -stageHalf * 0.45f, tran.player.pos.y + zoneHeight * 0.5f, stageHalf * 0.45f },
            { stageHalf * 0.45f, tran.player.pos.y + zoneHeight * 0.5f, -stageHalf * 0.45f }
        };
        DirectX::XMFLOAT3 safeCenter = safeCandidates[std::rand() % 3];
        clampCenterToStage(safeCenter, safeSize);

        PushAttackZone(
            m_boss.attackZones,
            safeCenter,
            { safeSize.x * 1.18f, safeSize.y, safeSize.z * 1.18f },
            safeOutlineColor,
            0.0f,
            0.0f,
            true);
        PushAttackZone(m_boss.attackZones, safeCenter, safeSize, safeColor, 0.0f, 0.0f, true);

        const int gridCount = 7;
        const float cellSize = stageSize / static_cast<float>(gridCount);
        std::vector<DirectX::XMFLOAT3> fillCenters;
        fillCenters.reserve(gridCount * gridCount);
        for (int z = 0; z < gridCount; ++z)
        {
            for (int x = 0; x < gridCount; ++x)
            {
                const DirectX::XMFLOAT3 center = {
                    -stageHalf + cellSize * (0.5f + static_cast<float>(x)),
                    tran.player.pos.y + zoneHeight * 0.5f,
                    -stageHalf + cellSize * (0.5f + static_cast<float>(z))
                };
                const bool overlapsSafe =
                    (std::fabs(center.x - safeCenter.x) < (cellSize * 0.5f + safeSize.x * 0.5f)) &&
                    (std::fabs(center.z - safeCenter.z) < (cellSize * 0.5f + safeSize.z * 0.5f));
                if (!overlapsSafe)
                {
                    fillCenters.push_back(center);
                }
            }
        }

        const int fillCount = static_cast<int>(fillCenters.size());
        for (int i = 0; i < fillCount; ++i)
        {
            float revealStart = 0.0f;
            if (fillCount > 1)
            {
                revealStart = (static_cast<float>(i) / static_cast<float>(fillCount - 1)) * (6.0f / 7.0f);
            }
            PushAttackZone(
                m_boss.attackZones,
                fillCenters[i],
                { cellSize * 1.02f, zoneHeight, cellSize * 1.02f },
                dangerColor,
                revealStart,
                false);
        }
    };

    const char* finalAttackAbyssRain = u8"\u5948\u843d\u843d\u4e0b";
    const char* finalAttackLine = u8"\u7d2b\u76f4\u7dda\u65ac\u308a";
    const char* finalAttackCross = u8"\u7d2b\u5341\u5b57\u65ac\u308a";
    const char* finalAttackBladeA = u8"\u6ed1\u8d70\u5203A";
    const char* finalAttackBladeB = u8"\u6ed1\u8d70\u5203B";
    const char* finalAttackRing = u8"\u5186\u74b0\u53ce\u675f";
    const char* finalAttackBurst = u8"\u74b0\u72b6\u7206\u7834";
    const char* finalAttackRadial = u8"\u653e\u5c04\u6383\u5c04";
    const char* finalAttackFan = u8"\u8d64\u6247\u6383\u5c04";
    const char* finalAttackLargeRing = u8"\u5927\u74b0";
    const char* finalAttackLethal = u8"\u7d42\u7109\u67d3\u3081";

    auto findFinalAttackIndexByName = [&](const char* name) -> int
    {
        const int entryCount = static_cast<int>(m_finalBossScript.attacks.size());
        for (int i = 0; i < entryCount; ++i)
        {
            if (m_finalBossScript.attacks[i].name == name)
            {
                return i;
            }
        }
        return -1;
    };

    auto makeDirectionFromYaw = [&](float yawDeg) -> DirectX::XMFLOAT3
    {
        const float rad = DegToRad(yawDeg);
        return { std::sin(rad), 0.0f, std::cos(rad) };
    };

    auto makeNormalizedVector = [&](const DirectX::XMFLOAT3& from, const DirectX::XMFLOAT3& to) -> DirectX::XMFLOAT3
    {
        const float dx = to.x - from.x;
        const float dz = to.z - from.z;
        const float lenSq = dx * dx + dz * dz;
        if (lenSq <= 0.000001f)
        {
            return { 0.0f, 0.0f, 1.0f };
        }
        const float invLen = 1.0f / std::sqrt(lenSq);
        return { dx * invLen, 0.0f, dz * invLen };
    };

    auto pushFinalCircleZone = [&](const DirectX::XMFLOAT3& sourceCenter,
                                   float diameter,
                                   const DirectX::XMFLOAT4& color,
                                   float revealStart = 0.0f,
                                   bool safeZone = false)
    {
        DirectX::XMFLOAT3 center = sourceCenter;
        DirectX::XMFLOAT3 size = {
            MaxFloat(diameter, 0.10f),
            zoneHeight,
            MaxFloat(diameter, 0.10f)
        };
        clampZoneSizeToStage(size);
        clampCenterToStage(center, size);
        center.y = tran.player.pos.y + zoneHeight * 0.5f;
        PushAttackZone(
            m_boss.attackZones,
            center,
            size,
            color,
            revealStart,
            0.0f,
            BossAttackScript::ColliderShapeCircle,
            center,
            center,
            false,
            safeZone);
    };

    auto pushFinalLineZone = [&](const DirectX::XMFLOAT3& rawStart,
                                 const DirectX::XMFLOAT3& rawEnd,
                                 float width,
                                 const DirectX::XMFLOAT4& color,
                                 float revealStart = 0.0f,
                                 bool safeZone = false)
    {
        DirectX::XMFLOAT3 start = rawStart;
        DirectX::XMFLOAT3 end = rawEnd;
        clampPointToStage(start);
        clampPointToStage(end);
        start.y = tran.player.pos.y + zoneHeight * 0.5f;
        end.y = tran.player.pos.y + zoneHeight * 0.5f;

        const float dx = end.x - start.x;
        const float dz = end.z - start.z;
        const float distance = std::sqrt(dx * dx + dz * dz);
        if (distance <= 0.05f)
        {
            return;
        }

        const DirectX::XMFLOAT3 center = {
            (start.x + end.x) * 0.5f,
            tran.player.pos.y + zoneHeight * 0.5f,
            (start.z + end.z) * 0.5f
        };
        const DirectX::XMFLOAT3 size = {
            MaxFloat(width, 0.10f),
            zoneHeight,
            MaxFloat(distance, 0.10f)
        };
        const float yawDeg = std::atan2(dx, dz) * (180.0f / DirectX::XM_PI);
        PushAttackZone(
            m_boss.attackZones,
            center,
            size,
            color,
            revealStart,
            yawDeg,
            BossAttackScript::ColliderShapeBox,
            start,
            end,
            false,
            safeZone);
    };

    auto pushFinalRing = [&](const DirectX::XMFLOAT3& center,
                             float ringRadius,
                             int count,
                             float nodeDiameter,
                             const DirectX::XMFLOAT4& color,
                             float revealStart = 0.0f)
    {
        const int clampedCount = ClampInt(count, 4, 64);
        for (int i = 0; i < clampedCount; ++i)
        {
            const float angle = (static_cast<float>(i) / static_cast<float>(clampedCount)) * DirectX::XM_2PI;
            DirectX::XMFLOAT3 node = {
                center.x + std::cos(angle) * ringRadius,
                center.y,
                center.z + std::sin(angle) * ringRadius
            };
            pushFinalCircleZone(node, nodeDiameter, color, revealStart);
        }
    };

    auto pushFinalRingBand = [&](const DirectX::XMFLOAT3& center,
                                 float ringRadius,
                                 float thickness,
                                 int count,
                                 const DirectX::XMFLOAT4& color,
                                 float revealStart = 0.0f)
    {
        const int clampedCount = ClampInt(count, 8, 64);
        const float safeRadius = MaxFloat(ringRadius, thickness * 0.5f);
        const float circumference = DirectX::XM_2PI * safeRadius;
        const float segmentLength = MaxFloat((circumference / static_cast<float>(clampedCount)) * 1.08f, thickness);
        for (int i = 0; i < clampedCount; ++i)
        {
            const float angle = (static_cast<float>(i) / static_cast<float>(clampedCount)) * DirectX::XM_2PI;
            const DirectX::XMFLOAT3 radial = { std::cos(angle), 0.0f, std::sin(angle) };
            const DirectX::XMFLOAT3 tangent = { -radial.z, 0.0f, radial.x };
            const DirectX::XMFLOAT3 bandCenter = {
                center.x + radial.x * ringRadius,
                center.y,
                center.z + radial.z * ringRadius
            };
            const DirectX::XMFLOAT3 start = {
                bandCenter.x - tangent.x * segmentLength * 0.5f,
                center.y,
                bandCenter.z - tangent.z * segmentLength * 0.5f
            };
            const DirectX::XMFLOAT3 end = {
                bandCenter.x + tangent.x * segmentLength * 0.5f,
                center.y,
                bandCenter.z + tangent.z * segmentLength * 0.5f
            };
            pushFinalLineZone(start, end, thickness, color, revealStart);
        }
    };

    auto pushFinalConvergeRing = [&](const DirectX::XMFLOAT3& center,
                                     float outerRadius,
                                     float innerRadius,
                                     int count,
                                     float width,
                                     const DirectX::XMFLOAT4& color)
    {
        const int clampedCount = ClampInt(count, 6, 32);
        const float safeOuter = MaxFloat(outerRadius, innerRadius + 0.20f);
        const float safeInner = ClampRange(innerRadius, 0.20f, safeOuter - 0.10f);
        for (int i = 0; i < clampedCount; ++i)
        {
            const float angle = (static_cast<float>(i) / static_cast<float>(clampedCount)) * DirectX::XM_2PI;
            const DirectX::XMFLOAT3 dir = { std::cos(angle), 0.0f, std::sin(angle) };
            const DirectX::XMFLOAT3 start = {
                center.x + dir.x * safeOuter,
                center.y,
                center.z + dir.z * safeOuter
            };
            const DirectX::XMFLOAT3 end = {
                center.x + dir.x * safeInner,
                center.y,
                center.z + dir.z * safeInner
            };
            const float reveal = (static_cast<float>(i % 4) / 4.0f) * 0.10f;
            pushFinalLineZone(start, end, width, color, reveal);
        }
    };

    auto pushFinalRadial = [&](const DirectX::XMFLOAT3& center,
                               int count,
                               float length,
                               float width,
                               float baseYawDeg,
                               const DirectX::XMFLOAT4& color)
    {
        const int clampedCount = ClampInt(count, 2, 32);
        for (int i = 0; i < clampedCount; ++i)
        {
            const float yawDeg = baseYawDeg + (360.0f * static_cast<float>(i) / static_cast<float>(clampedCount));
            const DirectX::XMFLOAT3 dir = makeDirectionFromYaw(yawDeg);
            DirectX::XMFLOAT3 start = center;
            DirectX::XMFLOAT3 end = {
                center.x + dir.x * length,
                center.y,
                center.z + dir.z * length
            };
            pushFinalLineZone(start, end, width, color);
        }
    };

    auto pushFinalFan = [&](const DirectX::XMFLOAT3& origin,
                            float forwardYawDeg,
                            int count,
                            float spreadDeg,
                            float length,
                            float width,
                            const DirectX::XMFLOAT4& color)
    {
        const int clampedCount = ClampInt(count, 1, 16);
        if (clampedCount == 1)
        {
            const DirectX::XMFLOAT3 dir = makeDirectionFromYaw(forwardYawDeg);
            pushFinalLineZone(
                origin,
                { origin.x + dir.x * length, origin.y, origin.z + dir.z * length },
                width,
                color);
            return;
        }

        const float startYawDeg = forwardYawDeg - spreadDeg * 0.5f;
        const float stepYawDeg = spreadDeg / static_cast<float>(clampedCount - 1);
        for (int i = 0; i < clampedCount; ++i)
        {
            const float yawDeg = startYawDeg + stepYawDeg * static_cast<float>(i);
            const DirectX::XMFLOAT3 dir = makeDirectionFromYaw(yawDeg);
            pushFinalLineZone(
                origin,
                { origin.x + dir.x * length, origin.y, origin.z + dir.z * length },
                width,
                color);
        }
    };

    auto beginScriptedAttack = [&](const BossAttackScript::Attack& scriptEntry, int scriptIndex, bool continuingRepeat)
    {
        const int deliveryMode = BossAttackScript::NormalizeAttackDeliveryMode(scriptEntry.deliveryMode);
        const bool isBossSelfAttack = (deliveryMode == BossAttackScript::AttackDeliveryBossSelf);
        const bool isRemoteFallingAttack = (deliveryMode == BossAttackScript::AttackDeliveryRemoteFalling);
        startTelegraph(BossController::AttackKindCustom,
            ClampRange(scriptEntry.telegraphSec * bossProfile.telegraphScale, 0.05f, 20.0f),
            ClampRange(scriptEntry.activeSec, 0.05f, 10.0f));

        m_boss.scriptedAttack = true;
        m_boss.scriptEntryIndex = scriptIndex;
        m_boss.attackDamageScale = ClampRange(scriptEntry.damageScale, 0.0f, 20.0f);
        m_boss.attackJumpOutOverrideSec = -1.0f;
        m_boss.attackFacingYawDeg = 0.0f;
        m_boss.dashStartPos = { 0.0f, 0.0f, 0.0f };
        m_boss.dashEndPos = { 0.0f, 0.0f, 0.0f };
        if (!continuingRepeat)
        {
            m_boss.attackRepeatsRemaining = ClampInt(scriptEntry.repeatCount, 1, 64);
        }
        if (m_boss.archetype == Transfer::RoguelikeUpgrade::BossFinalBarrage)
        {
            m_boss.finalLastAttackIndex = scriptIndex;
        }

        const DirectX::XMFLOAT3 bossOrigin = { 0.0f, 0.0f, 0.0f };
        const DirectX::XMFLOAT3 toPlayer = {
            tran.player.pos.x - bossOrigin.x,
            0.0f,
            tran.player.pos.z - bossOrigin.z
        };
        const float toPlayerLenSq = toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z;
        const float toPlayerInvLen = (toPlayerLenSq > 0.000001f) ? (1.0f / std::sqrt(toPlayerLenSq)) : 0.0f;
        const DirectX::XMFLOAT3 moveDir = (toPlayerInvLen > 0.0f)
            ? DirectX::XMFLOAT3{ toPlayer.x * toPlayerInvLen, 0.0f, toPlayer.z * toPlayerInvLen }
            : DirectX::XMFLOAT3{ 0.0f, 0.0f, 1.0f };

        std::vector<DirectX::XMFLOAT3> origins;
        const int spawnMode = BossAttackScript::NormalizeSpawnMode(scriptEntry.spawnMode);
        const int spawnCount = ClampInt(scriptEntry.spawnCount, 1, 64);
        if (spawnMode == BossAttackScript::SpawnArenaRandom)
        {
            for (int i = 0; i < spawnCount; ++i)
            {
                origins.push_back({
                    RandomRange(-stageHalf, stageHalf),
                    0.0f,
                    RandomRange(-stageHalf, stageHalf)
                });
            }
        }
        else if (spawnMode == BossAttackScript::SpawnPlayerAreaRandom)
        {
            const float randomRadius = ClampRange(scriptEntry.randomRadius, 0.0f, stageHalf);
            for (int i = 0; i < spawnCount; ++i)
            {
                const float angle = RandomRange(0.0f, DirectX::XM_2PI);
                const float distance = RandomRange(0.0f, randomRadius);
                origins.push_back({
                    ClampRange(tran.player.pos.x + std::cos(angle) * distance, -stageHalf, stageHalf),
                    0.0f,
                    ClampRange(tran.player.pos.z + std::sin(angle) * distance, -stageHalf, stageHalf)
                });
            }
        }
        else if (spawnMode == BossAttackScript::SpawnPlayerPosition)
        {
            for (int i = 0; i < spawnCount; ++i)
            {
                origins.push_back(tran.player.pos);
            }
        }
        else
        {
            origins.push_back(bossOrigin);
        }

        DirectX::XMFLOAT3 bossTargetPos = bossOrigin;
        if (isBossSelfAttack)
        {
            switch (BossAttackScript::NormalizeMovePreset(scriptEntry.movePreset))
            {
            case BossAttackScript::MoveForward:
            case BossAttackScript::MoveChargePlayer:
                bossTargetPos = {
                    ClampRange(bossOrigin.x + moveDir.x * scriptEntry.moveDistance, -stageHalf, stageHalf),
                    0.0f,
                    ClampRange(bossOrigin.z + moveDir.z * scriptEntry.moveDistance, -stageHalf, stageHalf)
                };
                break;
            case BossAttackScript::MoveRetreat:
                bossTargetPos = {
                    ClampRange(bossOrigin.x - moveDir.x * scriptEntry.moveDistance, -stageHalf, stageHalf),
                    0.0f,
                    ClampRange(bossOrigin.z - moveDir.z * scriptEntry.moveDistance, -stageHalf, stageHalf)
                };
                break;
            case BossAttackScript::MoveWarpBehindPlayer:
                bossTargetPos = {
                    ClampRange(tran.player.pos.x - moveDir.x * scriptEntry.moveDistance, -stageHalf, stageHalf),
                    0.0f,
                    ClampRange(tran.player.pos.z - moveDir.z * scriptEntry.moveDistance, -stageHalf, stageHalf)
                };
                break;
            case BossAttackScript::MoveToAttackOrigin:
                if (!origins.empty())
                {
                    bossTargetPos = origins.front();
                }
                break;
            default:
                break;
            }
        }

        const DirectX::XMFLOAT3 faceToPlayer = {
            tran.player.pos.x - bossTargetPos.x,
            0.0f,
            tran.player.pos.z - bossTargetPos.z
        };
        const float faceLenSq = faceToPlayer.x * faceToPlayer.x + faceToPlayer.z * faceToPlayer.z;
        if (faceLenSq > 0.000001f)
        {
            const float faceInvLen = 1.0f / std::sqrt(faceLenSq);
            m_boss.attackFacingYawDeg = std::atan2(faceToPlayer.x * faceInvLen, faceToPlayer.z * faceInvLen) * (180.0f / DirectX::XM_PI);
        }

        m_boss.dashStartPos = bossOrigin;
        m_boss.dashEndPos = bossTargetPos;

        if (scriptEntry.visual.enabled && !scriptEntry.visual.texturePath.empty() && scriptEntry.visual.texturePath.find("Assets/") == 0)
        {
            m_boss.LoadRockTexture(scriptEntry.visual.texturePath.c_str());
        }

        if (m_boss.archetype == Transfer::RoguelikeUpgrade::BossFinalBarrage)
        {
            const DirectX::XMFLOAT3 arenaCenter = { 0.0f, tran.player.pos.y + zoneHeight * 0.5f, 0.0f };
            const DirectX::XMFLOAT3 playerCenter = { tran.player.pos.x, tran.player.pos.y + zoneHeight * 0.5f, tran.player.pos.z };
            const DirectX::XMFLOAT3 playerDir = makeNormalizedVector(arenaCenter, playerCenter);
            const DirectX::XMFLOAT3 playerRight = { playerDir.z, 0.0f, -playerDir.x };
            const float playerYawDeg = std::atan2(playerDir.x, playerDir.z) * (180.0f / DirectX::XM_PI);
            const DirectX::XMFLOAT4 purpleColor = { 0.85f, 0.34f, 1.00f, 1.0f };
            const DirectX::XMFLOAT4 blueColor = { 0.32f, 0.82f, 1.00f, 1.0f };
            const DirectX::XMFLOAT4 redColor = { 1.00f, 0.18f, 0.16f, 1.0f };
            const DirectX::XMFLOAT4 orangeColor = { 1.00f, 0.46f, 0.14f, 1.0f };

            if (scriptEntry.name == finalAttackCross)
            {
                const float halfLength = stageHalf * 1.05f;
                pushFinalLineZone(
                    { arenaCenter.x - playerDir.x * halfLength, arenaCenter.y, arenaCenter.z - playerDir.z * halfLength },
                    { arenaCenter.x + playerDir.x * halfLength, arenaCenter.y, arenaCenter.z + playerDir.z * halfLength },
                    0.95f,
                    purpleColor);
                pushFinalLineZone(
                    { arenaCenter.x - playerRight.x * halfLength, arenaCenter.y, arenaCenter.z - playerRight.z * halfLength },
                    { arenaCenter.x + playerRight.x * halfLength, arenaCenter.y, arenaCenter.z + playerRight.z * halfLength },
                    0.95f,
                    purpleColor,
                    0.08f);
                return;
            }
            if (scriptEntry.name == finalAttackRing)
            {
                pushFinalConvergeRing(playerCenter, 4.2f, 1.2f, 12, 0.90f, blueColor);
                return;
            }
            if (scriptEntry.name == finalAttackBurst)
            {
                pushFinalCircleZone(playerCenter, 1.95f, safeOutlineColor, 0.0f, true);
                pushFinalCircleZone(playerCenter, 1.55f, safeColor, 0.0f, true);
                pushFinalRingBand(playerCenter, 2.65f, 1.05f, 18, blueColor, 0.04f);
                return;
            }
            if (scriptEntry.name == finalAttackRadial)
            {
                pushFinalRadial(arenaCenter, 8, stageHalf * 1.15f, 0.90f, 0.0f, purpleColor);
                return;
            }
            if (scriptEntry.name == finalAttackFan)
            {
                pushFinalFan(arenaCenter, playerYawDeg, 5, 90.0f, stageHalf * 1.12f, 1.30f, redColor);
                return;
            }
            if (scriptEntry.name == finalAttackLargeRing)
            {
                pushFinalCircleZone(arenaCenter, 5.00f, safeOutlineColor, 0.0f, true);
                pushFinalCircleZone(arenaCenter, 4.45f, safeColor, 0.0f, true);
                pushFinalRingBand(arenaCenter, 5.35f, 1.55f, 28, redColor);
                return;
            }
            if (scriptEntry.name == finalAttackLethal)
            {
                PushAttackZone(
                    m_boss.attackZones,
                    arenaCenter,
                    { stageSize * 0.98f, zoneHeight, stageSize * 0.98f },
                    redColor,
                    0.0f,
                    0.0f,
                    BossAttackScript::ColliderShapeBox,
                    arenaCenter,
                    arenaCenter,
                    false,
                    false);
                return;
            }
            if (scriptEntry.name == finalAttackLine)
            {
                const float halfLength = stageHalf * 1.05f;
                pushFinalLineZone(
                    { arenaCenter.x - playerDir.x * halfLength, arenaCenter.y, arenaCenter.z - playerDir.z * halfLength },
                    { arenaCenter.x + playerDir.x * halfLength, arenaCenter.y, arenaCenter.z + playerDir.z * halfLength },
                    0.95f,
                    purpleColor);
                return;
            }
        }

        for (const DirectX::XMFLOAT3& origin : origins)
        {
            const DirectX::XMFLOAT3 anchor = (spawnMode == BossAttackScript::SpawnFixed) ? bossTargetPos : origin;
            BossAttackScript::Collider workingCollider = scriptEntry.hitbox;
            if (!workingCollider.enabled)
            {
                continue;
            }

            if (workingCollider.usePlayerDirection)
            {
                const float targetDistance = std::sqrt(toPlayerLenSq);
                const float limitedDistance = (workingCollider.maxDistance > 0.001f)
                    ? std::min(targetDistance, workingCollider.maxDistance)
                    : targetDistance;
                const DirectX::XMFLOAT3 right = { moveDir.z, 0.0f, -moveDir.x };
                const DirectX::XMFLOAT3 lateralOffset = {
                    right.x * workingCollider.lateralOffset,
                    0.0f,
                    right.z * workingCollider.lateralOffset
                };
                workingCollider.useEndPosition = true;
                workingCollider.startMode = BossAttackScript::ColliderStartAbsolute;
                workingCollider.startPos = {
                    anchor.x + lateralOffset.x,
                    workingCollider.startPos.y,
                    anchor.z + lateralOffset.z
                };
                workingCollider.endMode = BossAttackScript::ColliderEndAbsolute;
                workingCollider.endPos = {
                    anchor.x + lateralOffset.x + moveDir.x * limitedDistance,
                    workingCollider.endPos.y,
                    anchor.z + lateralOffset.z + moveDir.z * limitedDistance
                };
            }
            if (isRemoteFallingAttack)
            {
                workingCollider.useEndPosition = false;
            }

            ScriptedColliderZone zone = BuildScriptedColliderZone(
                workingCollider,
                { anchor.x, tran.player.pos.y + zoneHeight * 0.5f, anchor.z },
                tran.player.pos,
                m_boss.attackFacingYawDeg,
                stageHalf,
                zoneHeight,
                !isRemoteFallingAttack);
            clampZoneSizeToStage(zone.size);
            clampCenterToStage(zone.center, zone.size);
            PushAttackZone(
                m_boss.attackZones,
                zone.center,
                zone.size,
                dangerColor,
                0.0f,
                zone.yawDeg,
                zone.shape,
                zone.startPos,
                zone.endPos,
                zone.hasPath,
                false);
        }
    };

    auto beginNextTopLevelAttack = [&]()
    {
        ++m_boss.attackCycleCount;
        if (useFinalBossScript &&
            m_boss.archetype == Transfer::RoguelikeUpgrade::BossFinalBarrage)
        {
            const int attackAbyssRain = findFinalAttackIndexByName(finalAttackAbyssRain);
            const int attackLine = findFinalAttackIndexByName(finalAttackLine);
            const int attackCross = findFinalAttackIndexByName(finalAttackCross);
            const int attackBladeA = findFinalAttackIndexByName(finalAttackBladeA);
            const int attackBladeB = findFinalAttackIndexByName(finalAttackBladeB);
            const int attackRing = findFinalAttackIndexByName(finalAttackRing);
            const int attackBurst = findFinalAttackIndexByName(finalAttackBurst);
            const int attackRadial = findFinalAttackIndexByName(finalAttackRadial);
            const int attackFan = findFinalAttackIndexByName(finalAttackFan);
            const int attackLargeRing = findFinalAttackIndexByName(finalAttackLargeRing);
            const int attackLethal = findFinalAttackIndexByName(finalAttackLethal);

            auto beginNamedFinalAttack = [&](int attackIndex) -> bool
            {
                if (attackIndex < 0 || attackIndex >= static_cast<int>(m_finalBossScript.attacks.size()))
                {
                    return false;
                }
                const BossAttackScript::Attack& scriptEntry = m_finalBossScript.attacks[attackIndex];
                if (!scriptEntry.enabled)
                {
                    return false;
                }
                beginScriptedAttack(scriptEntry, attackIndex, false);
                return true;
            };

            auto pickFromPool = [&](const int* pool, int count) -> int
            {
                std::vector<int> valid;
                valid.reserve(count);
                for (int i = 0; i < count; ++i)
                {
                    const int candidateIndex = pool[i];
                    if (candidateIndex < 0 || candidateIndex >= static_cast<int>(m_finalBossScript.attacks.size()))
                    {
                        continue;
                    }
                    if (!m_finalBossScript.attacks[candidateIndex].enabled)
                    {
                        continue;
                    }
                    valid.push_back(candidateIndex);
                }
                if (valid.empty())
                {
                    return -1;
                }
                if (valid.size() == 1)
                {
                    return valid.front();
                }

                std::vector<int> filtered;
                filtered.reserve(valid.size());
                for (int candidateIndex : valid)
                {
                    if (candidateIndex != m_boss.finalLastAttackIndex)
                    {
                        filtered.push_back(candidateIndex);
                    }
                }
                const std::vector<int>& choices = filtered.empty() ? valid : filtered;
                return choices[RandomRangeInt(0, static_cast<int>(choices.size()) - 1)];
            };

            const float lethalDelaySec = 105.0f;
            if (!m_boss.finalLethalTriggered &&
                m_boss.phase >= 2 &&
                m_boss.battleTimer >= lethalDelaySec &&
                beginNamedFinalAttack(attackLethal))
            {
                m_boss.finalLethalTriggered = true;
                return;
            }

            if (m_boss.finalPhaseTransitionPending)
            {
                m_boss.finalPhaseTransitionPending = false;
                const int openerIndex = (m_boss.phase >= 3) ? attackFan : attackLargeRing;
                if (beginNamedFinalAttack(openerIndex))
                {
                    return;
                }
            }

            const int phaseOnePool[] = {
                attackAbyssRain,
                attackAbyssRain,
                attackLine,
                attackCross,
                attackBladeA,
                attackBladeB,
                attackRing,
                attackBurst
            };
            const int phaseTwoPool[] = {
                attackAbyssRain,
                attackLine,
                attackCross,
                attackBladeA,
                attackBladeB,
                attackRing,
                attackBurst,
                attackRadial,
                attackLargeRing
            };
            const int phaseThreePool[] = {
                attackAbyssRain,
                attackBladeA,
                attackBladeB,
                attackBladeA,
                attackBladeB,
                attackBurst,
                attackRadial,
                attackFan,
                attackLargeRing
            };

            int pickedIndex = -1;
            if (m_boss.phase >= 3)
            {
                pickedIndex = pickFromPool(phaseThreePool, static_cast<int>(sizeof(phaseThreePool) / sizeof(phaseThreePool[0])));
            }
            else if (m_boss.phase >= 2)
            {
                pickedIndex = pickFromPool(phaseTwoPool, static_cast<int>(sizeof(phaseTwoPool) / sizeof(phaseTwoPool[0])));
            }
            else
            {
                pickedIndex = pickFromPool(phaseOnePool, static_cast<int>(sizeof(phaseOnePool) / sizeof(phaseOnePool[0])));
            }

            if (beginNamedFinalAttack(pickedIndex))
            {
                return;
            }
        }
        if (useFinalBossScript)
        {
            const int entryCount = static_cast<int>(m_finalBossScript.attacks.size());
            if (entryCount > 0)
            {
                const int maxEntryIndex = entryCount - 1;
                const int startIndex = ClampInt(m_finalBossScriptCursor, 0, maxEntryIndex);
                const int attemptCount = entryCount;
                for (int attempt = 0; attempt < attemptCount; ++attempt)
                {
                    const int candidateIndex = (startIndex + attempt) % entryCount;
                    const BossAttackScript::Attack& scriptEntry = m_finalBossScript.attacks[candidateIndex];
                    if (!scriptEntry.enabled)
                    {
                        continue;
                    }
                    m_finalBossScriptCursor = (candidateIndex + 1) % entryCount;
                    beginScriptedAttack(scriptEntry, candidateIndex, false);
                    return;
                }
            }
        }
        if (m_boss.forceUltimatePending && m_boss.specialUnlocked)
        {
            m_boss.forceUltimatePending = false;
            beginUltimateCross();
            return;
        }

        if (m_boss.specialUnlocked &&
            ultimateInterval > 0 &&
            (m_boss.attackCycleCount % ultimateInterval) == 0)
        {
            beginUltimateCross();
            return;
        }

        switch (PickWeightedBossAttack(bossProfile))
        {
        case 0: beginDashAttack(BossController::AttackKindDashNarrow, false); break;
        case 1: beginDashAttack(BossController::AttackKindDashWide, true); break;
        case 2: beginRandomRain(); break;
        case 3: beginSummon(); break;
        default: beginTrackingDrop(true); break;
        }
    };

    auto applyBossDamageToPlayer = [&]() -> bool
    {
        const float hpBefore = tran.player.hp;
        const float attackDamageScale = MaxFloat(m_boss.attackDamageScale, 0.0f);
        const float appliedBossDamage = MaxFloat(bossDamage * attackDamageScale, 0.0f);
        const bool endedRun = applyPlayerDamage && applyPlayerDamage(appliedBossDamage);
        if (tran.player.hp < hpBefore && m_hitStopTimer < bossHitStop)
        {
            m_hitStopTimer = bossHitStop;
        }
        if (tran.player.hp < hpBefore)
        {
            const float bossHitShakeDuration = ClampRange(
                tran.gameplay.bossHitShakeDuration,
                0.0f,
                1.0f);
            const float bossHitShakeAmplitude = ClampRange(
                tran.gameplay.bossHitShakeAmplitude,
                0.0f,
                1.0f);
            m_screenShakeDuration = bossHitShakeDuration;
            if (m_screenShakeTimer < bossHitShakeDuration)
            {
                m_screenShakeTimer = bossHitShakeDuration;
            }
            if (m_screenShakeAmplitude < bossHitShakeAmplitude)
            {
                m_screenShakeAmplitude = bossHitShakeAmplitude;
            }
            m_screenShakePhase = 0.0f;
            if (bossProfile.inflictsCurse)
            {
                const float curseDuration = 2.5f;
                const float immediateAttackPenalty = 0.30f;
                const float immediateSkillPenalty = 0.60f;
                m_bossCurseTimer = MaxFloat(m_bossCurseTimer, curseDuration);
                if (m_attackCooldownTimer < immediateAttackPenalty)
                {
                    m_attackCooldownTimer = immediateAttackPenalty;
                }
                if (m_attackCooldownUiTimer < immediateAttackPenalty)
                {
                    m_attackCooldownUiTimer = immediateAttackPenalty;
                }
                if (m_attackCooldownUiDuration < immediateAttackPenalty)
                {
                    m_attackCooldownUiDuration = immediateAttackPenalty;
                }
                for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
                {
                    if (m_skillStockCount[slotIndex] < m_skillStockMax[slotIndex])
                    {
                        m_skillStockRechargeTimer[slotIndex] += immediateSkillPenalty;
                    }
                }
                m_skill1CooldownTimer = m_skillStockRechargeTimer[0];
                m_skill2CooldownTimer = m_skillStockRechargeTimer[1];
            }
        }
        return endedRun;
    };

    auto applyAttackDamage = [&]() -> bool
    {
        Collision::Box playerBox = MakeAabb(
            {
                tran.player.pos.x,
                tran.player.pos.y + tran.player.size.y * 0.5f,
                tran.player.pos.z
            },
            tran.player.size);

        bool safeFromField = false;
        for (const auto& zone : m_boss.attackZones)
        {
            if (!zone.safeZone)
            {
                continue;
            }
            if (HitZone(playerBox, zone))
            {
                safeFromField = true;
                break;
            }
        }

        switch (m_boss.attackKind)
        {
        case BossController::AttackKindSummon:
            for (int i = 0; i < m_boss.attackRepeatsRemaining; ++i)
            {
                SpawnEnemyByIndex(static_cast<int>(m_enemies.size()), stageSize);
            }
            break;

        case BossController::AttackKindUltimateField:
            if (!safeFromField && applyBossDamageToPlayer())
            {
                return true;
            }
            break;

        case BossController::AttackKindRandomRain:
        case BossController::AttackKindTrackingDrop:
        case BossController::AttackKindCustom:
            if (safeFromField)
            {
                break;
            }
            for (const auto& zone : m_boss.attackZones)
            {
                if (zone.safeZone) continue;
                if (HitZone(playerBox, zone))
                {
                    if (applyBossDamageToPlayer())
                    {
                        return true;
                    }
                    break;
                }
            }
            for (const auto& zone : m_boss.attackZones)
            {
                if (!zone.safeZone)
                {
                    bool spawnedVisual = false;
                    if (m_boss.scriptedAttack &&
                        m_boss.scriptEntryIndex >= 0 &&
                        m_boss.scriptEntryIndex < static_cast<int>(m_finalBossScript.attacks.size()))
                    {
                        const BossAttackScript::Attack& activeAttack = m_finalBossScript.attacks[m_boss.scriptEntryIndex];
                        spawnedVisual = activeAttack.visual.enabled;
                    }
                    if (!spawnedVisual)
                    {
                        SpawnRockVisual(
                            m_boss.fallingRocks,
                            zone.center,
                            MaxFloat(zone.size.x, zone.size.z),
                            0.28f,
                            MaxFloat(zone.size.x, zone.size.z),
                            0.0f,
                            true);
                    }
                }
            }
            break;

        case BossController::AttackKindUltimateStomp:
            for (const auto& zone : m_boss.attackZones)
            {
                if (zone.safeZone) continue;
                if (HitZone(playerBox, zone))
                {
                    if (applyBossDamageToPlayer())
                    {
                        return true;
                    }
                    break;
                }
            }
            break;

        default:
            for (const auto& zone : m_boss.attackZones)
            {
                if (zone.safeZone) continue;
                if (HitZone(playerBox, zone))
                {
                    if (applyBossDamageToPlayer())
                    {
                        return true;
                    }
                    break;
                }
            }
            break;
        }

        return false;
    };

    auto finishCurrentAttack = [&]()
    {
        m_boss.attackZones.clear();
        m_boss.attackLane.center = { 0.0f, 0.0f, 0.0f };
        m_boss.attackLane.size = { 0.0f, 0.0f, 0.0f };
        m_boss.attackState = BossController::AttackIdle;
        m_boss.attackStateTimer = 0.0f;
        m_boss.attackTelegraphDuration = 0.0f;
        m_boss.attackExecuteDuration = 0.0f;
        m_boss.attackResolved = false;
        m_boss.jumpedOut = false;
        m_boss.scriptEntryIndex = -1;
        m_boss.scriptedAttack = false;
        m_boss.attackDamageScale = 1.0f;
        m_boss.attackJumpOutOverrideSec = -1.0f;
        m_boss.pos = { 0.0f, 0.0f, 0.0f };
        m_boss.attackCooldownTimer = m_boss.forceUltimatePending ? 0.0f : cooldownSec;
    };

    auto advanceBossSequence = [&]()
    {
        const DirectX::XMFLOAT3 previousPos = m_boss.pos;
        m_boss.attackZones.clear();
        m_boss.attackLane.center = { 0.0f, 0.0f, 0.0f };
        m_boss.attackLane.size = { 0.0f, 0.0f, 0.0f };
        m_boss.attackStateTimer = 0.0f;
        m_boss.attackTelegraphDuration = 0.0f;
        m_boss.attackExecuteDuration = 0.0f;
        m_boss.attackResolved = false;
        m_boss.jumpedOut = false;
        m_boss.pos = { 0.0f, 0.0f, 0.0f };

        if (m_boss.scriptedAttack &&
            m_boss.scriptEntryIndex >= 0 &&
            m_boss.scriptEntryIndex < static_cast<int>(m_finalBossScript.attacks.size()))
        {
            const BossAttackScript::Attack& scriptEntry = m_finalBossScript.attacks[m_boss.scriptEntryIndex];
            --m_boss.attackRepeatsRemaining;
            if (m_boss.attackRepeatsRemaining > 0)
            {
                beginScriptedAttack(scriptEntry, m_boss.scriptEntryIndex, true);
                m_boss.dashStartPos = previousPos;
                m_boss.pos = previousPos;
                return;
            }

            const int chainedIndex = BossAttackScript::ChooseNextAttackIndex(m_finalBossScript, scriptEntry);
            if (chainedIndex >= 0 && chainedIndex < static_cast<int>(m_finalBossScript.attacks.size()))
            {
                beginScriptedAttack(m_finalBossScript.attacks[chainedIndex], chainedIndex, false);
                m_boss.dashStartPos = previousPos;
                m_boss.pos = previousPos;
                return;
            }

            finishCurrentAttack();
            return;
        }

        switch (m_boss.attackKind)
        {
        case BossController::AttackKindTrackingDrop:
            --m_boss.attackRepeatsRemaining;
            if (m_boss.attackRepeatsRemaining > 0)
            {
                beginTrackingDrop(false);
                return;
            }
            break;

        case BossController::AttackKindUltimateCross:
            beginUltimateStomp(true);
            m_boss.dashStartPos = previousPos;
            m_boss.pos = previousPos;
            return;

        case BossController::AttackKindUltimateStomp:
            --m_boss.attackRepeatsRemaining;
            if (m_boss.attackRepeatsRemaining > 0)
            {
                beginUltimateStomp(false);
                m_boss.dashStartPos = previousPos;
                m_boss.pos = previousPos;
                return;
            }
            beginUltimateField();
            return;

        default:
            break;
        }

        finishCurrentAttack();
    };

    // Main boss state machine: broken recovery, idle selection, telegraph, then execution.
    if (m_boss.hp > 0)
    {
        m_boss.battleTimer += kBossFixedDt;
        if (m_boss.isBroken)
        {
            m_boss.breakRecoverTimer -= kBossFixedDt;
            if (m_boss.breakRecoverTimer <= 0.0f)
            {
                m_boss.breakRecoverTimer = 0.0f;
                m_boss.isBroken = false;
                m_boss.guardMax = ClampRange(m_boss.guardMax + guardRecoverStep, guardInitialMax, guardFinalMax);
                m_boss.guard = m_boss.guardMax;
            }
        }

        if (m_boss.attackState == BossController::AttackIdle)
        {
            m_boss.pos = { 0.0f, 0.0f, 0.0f };
            if (m_boss.attackCooldownTimer > 0.0f)
            {
                m_boss.attackCooldownTimer -= kBossFixedDt;
                if (m_boss.attackCooldownTimer < 0.0f) m_boss.attackCooldownTimer = 0.0f;
            }
            else
            {
                beginNextTopLevelAttack();
            }
        }
        else if (m_boss.attackState == BossController::AttackTelegraph)
        {
            m_boss.attackStateTimer += kBossFixedDt;

            const bool isDashAttack =
                (m_boss.attackKind == BossController::AttackKindDashNarrow) ||
                (m_boss.attackKind == BossController::AttackKindDashWide);
            const bool isStompAttack = (m_boss.attackKind == BossController::AttackKindUltimateStomp);
            const bool isCustomAttack = (m_boss.attackKind == BossController::AttackKindCustom);
            const float jumpOutOverrideSec = (m_boss.attackJumpOutOverrideSec >= 0.0f)
                ? m_boss.attackJumpOutOverrideSec
                : jumpOutSec;
            const float effectiveJumpOutSec = (jumpOutOverrideSec < m_boss.attackTelegraphDuration)
                ? jumpOutOverrideSec
                : m_boss.attackTelegraphDuration;
            const float stompJumpStartLimit = (m_boss.attackTelegraphDuration > kBossFixedDt)
                ? (m_boss.attackTelegraphDuration - kBossFixedDt)
                : 0.0f;
            const float stompJumpStartSec = ClampRange(
                MaxFloat(effectiveJumpOutSec, m_boss.attackTelegraphDuration * 0.60f),
                0.0f,
                stompJumpStartLimit);

            if (isDashAttack)
            {
                if (!m_boss.jumpedOut && m_boss.attackStateTimer >= effectiveJumpOutSec)
                {
                    m_boss.pos = m_boss.dashStartPos;
                    m_boss.jumpedOut = true;
                }
            }
            else if (isStompAttack && !m_boss.attackZones.empty())
            {
                if (m_boss.attackStateTimer < stompJumpStartSec)
                {
                    m_boss.pos = m_boss.dashStartPos;
                }
                else
                {
                    const float jumpDuration = (m_boss.attackTelegraphDuration > stompJumpStartSec)
                        ? (m_boss.attackTelegraphDuration - stompJumpStartSec)
                        : kBossFixedDt;
                    const float jumpRate = Clamp01((m_boss.attackStateTimer - stompJumpStartSec) / jumpDuration);
                    const float heightRate = jumpRate * jumpRate * (3.0f - 2.0f * jumpRate);
                    DirectX::XMFLOAT3 jumpPos = LerpFloat3(m_boss.dashStartPos, m_boss.dashEndPos, jumpRate);
                    jumpPos.y = m_boss.size.y * 2.4f * heightRate;
                    m_boss.pos = jumpPos;
                }
            }

            if (m_boss.attackStateTimer >= m_boss.attackTelegraphDuration)
            {
                m_boss.attackState = BossController::AttackDash;
                m_boss.attackStateTimer = 0.0f;
                m_boss.attackResolved = (isStompAttack || isCustomAttack) ? false : true;

                if (isDashAttack || isCustomAttack)
                {
                    m_boss.pos = m_boss.dashStartPos;
                }
                else if (isStompAttack && !m_boss.attackZones.empty())
                {
                    m_boss.pos = {
                        m_boss.dashEndPos.x,
                        m_boss.size.y * 2.4f,
                        m_boss.dashEndPos.z
                    };
                }
                else
                {
                    m_boss.pos = { 0.0f, 0.0f, 0.0f };
                }

                if (!isStompAttack && applyAttackDamage())
                {
                    return true;
                }
            }
        }
        else
        {
            m_boss.attackStateTimer += kBossFixedDt;

            if (m_boss.attackKind == BossController::AttackKindDashNarrow ||
                m_boss.attackKind == BossController::AttackKindDashWide)
            {
                const float duration = (m_boss.attackExecuteDuration > 0.01f) ? m_boss.attackExecuteDuration : 0.01f;
                const float dashRate = Clamp01(m_boss.attackStateTimer / duration);
                m_boss.pos = LerpFloat3(m_boss.dashStartPos, m_boss.dashEndPos, dashRate);
            }
            else if (m_boss.attackKind == BossController::AttackKindCustom)
            {
                const float duration = (m_boss.attackExecuteDuration > 0.01f) ? m_boss.attackExecuteDuration : 0.01f;
                const float moveRate = Clamp01(m_boss.attackStateTimer / duration);
                m_boss.pos = LerpFloat3(m_boss.dashStartPos, m_boss.dashEndPos, moveRate);
                if (m_boss.scriptEntryIndex >= 0 && m_boss.scriptEntryIndex < static_cast<int>(m_finalBossScript.attacks.size()))
                {
                    const BossAttackScript::Attack& activeAttack = m_finalBossScript.attacks[m_boss.scriptEntryIndex];
                    bool hasMovingPathZone = false;
                    for (const auto& zone : m_boss.attackZones)
                    {
                        if (!zone.safeZone && zone.hasPath)
                        {
                            hasMovingPathZone = true;
                            break;
                        }
                    }
                    if (activeAttack.hitbox.useEndPosition &&
                        hasMovingPathZone &&
                        BossAttackScript::NormalizeAttackDeliveryMode(activeAttack.deliveryMode) != BossAttackScript::AttackDeliveryRemoteFalling)
                    {
                        for (auto& zone : m_boss.attackZones)
                        {
                            if (zone.safeZone || !zone.hasPath) continue;
                            const float pathYawDeg = std::atan2(
                                zone.endPos.x - zone.startPos.x,
                                zone.endPos.z - zone.startPos.z) * (180.0f / DirectX::XM_PI);
                            zone.center = {
                                zone.startPos.x + (zone.endPos.x - zone.startPos.x) * moveRate,
                                zone.startPos.y + (zone.endPos.y - zone.startPos.y) * moveRate,
                                zone.startPos.z + (zone.endPos.z - zone.startPos.z) * moveRate
                            };
                            zone.hasPath = false;
                            zone.yawDeg = pathYawDeg;
                            zone.size = activeAttack.hitbox.startSize;
                            if (BossAttackScript::NormalizeColliderShape(activeAttack.hitbox.shape) == BossAttackScript::ColliderShapeCircle)
                            {
                                zone.size.z = zone.size.x;
                            }
                        }
                    }
                    if (applyAttackDamage())
                    {
                        return true;
                    }
                }
            }
            else if (m_boss.attackKind == BossController::AttackKindUltimateStomp &&
                     !m_boss.attackZones.empty())
            {
                const float duration = (m_boss.attackExecuteDuration > 0.01f) ? m_boss.attackExecuteDuration : 0.01f;
                const float fallRate = Clamp01(m_boss.attackStateTimer / duration);
                const DirectX::XMFLOAT3 fallStartPos = {
                    m_boss.dashEndPos.x,
                    m_boss.size.y * 2.4f,
                    m_boss.dashEndPos.z
                };
                m_boss.pos = LerpFloat3(fallStartPos, m_boss.dashEndPos, fallRate);
            }
            else
            {
                m_boss.pos = { 0.0f, 0.0f, 0.0f };
            }

            if (m_boss.attackStateTimer >= m_boss.attackExecuteDuration)
            {
                if (m_boss.attackKind == BossController::AttackKindUltimateStomp && !m_boss.attackResolved)
                {
                    const float stompImpactDuration = 0.18f;
                    const float stompShakeDuration = ClampRange(
                        MaxFloat(tran.gameplay.screenShakeDuration, 0.14f),
                        0.08f,
                        0.40f);
                    const float stompShakeAmplitude = ClampRange(
                        MaxFloat(tran.gameplay.screenShakeAmplitude, 0.16f) * 1.35f,
                        0.10f,
                        0.80f);
                    m_boss.pos = m_boss.dashEndPos;
                    m_boss.attackResolved = true;
                    m_bossStompImpactTimer = stompImpactDuration;
                    m_screenShakeDuration = stompShakeDuration;
                    if (m_screenShakeTimer < stompShakeDuration)
                    {
                        m_screenShakeTimer = stompShakeDuration;
                    }
                    if (m_screenShakeAmplitude < stompShakeAmplitude)
                    {
                        m_screenShakeAmplitude = stompShakeAmplitude;
                    }
                    m_screenShakePhase += 1.0f;
                    if (m_pDropSe) PlaySound(m_pDropSe);
                    if (applyAttackDamage())
                    {
                        return true;
                    }
                }
                advanceBossSequence();
            }
        }
    }
    else
    {
        m_boss.pos = { 0.0f, 0.0f, 0.0f };
        m_boss.attackZones.clear();
        m_boss.attackState = BossController::AttackIdle;
        m_boss.attackStateTimer = 0.0f;
        m_boss.attackTelegraphDuration = 0.0f;
        m_boss.attackExecuteDuration = 0.0f;
        m_boss.attackCooldownTimer = 0.0f;
        m_boss.attackResolved = false;
        m_boss.jumpedOut = false;
    }

    // Player attacks only apply once per swing, using the shared attack AABB from SceneGame.
    if (m_boss.hp > 0 && m_attackActive && m_boss.lastHitSwingId != m_attackSwingId)
    {
        Collision::Box attackBox{};
        attackBox.center = m_attackCenter;
        attackBox.size = m_attackSize;
        Collision::Box bossBox = MakeAabb({
            m_boss.pos.x,
            m_boss.pos.y + m_boss.size.y * 0.5f,
            m_boss.pos.z
        }, m_boss.size);
        if (HitAabb(attackBox, bossBox))
        {
            SpawnHitEffect(bossBox.center, bossBox.size);
            const float playerDamage = MaxFloat(static_cast<float>(playerAttackDamage), 0.0f);
            if (!m_boss.isBroken)
            {
                m_boss.guard -= guardDamagePerHit;
                if (m_boss.guard <= 0.0f)
                {
                    m_boss.guard = 0.0f;
                    m_boss.isBroken = true;
                    m_boss.breakRecoverTimer = breakRecoverDurationSec;
                    if (m_boss.attackState == BossController::AttackIdle &&
                        m_boss.attackCooldownTimer < 1.10f)
                    {
                        m_boss.attackCooldownTimer = 1.10f;
                    }
                }
            }

            const float appliedDamageScale = m_boss.isBroken ? brokenDamageScale : normalDamageScale;
            const float appliedPlayerDamage = static_cast<float>((m_attackDamageThisSwing > 0) ? m_attackDamageThisSwing : playerDamage);
            m_boss.hpDamageCarry += appliedPlayerDamage * appliedDamageScale;
            const int hpDamage = static_cast<int>(std::floor(m_boss.hpDamageCarry + 0.0001f));
            if (hpDamage > 0)
            {
                m_boss.hpDamageCarry -= static_cast<float>(hpDamage);
                m_boss.hp -= hpDamage;
            }
            if (m_boss.hp < 0) m_boss.hp = 0;
            m_boss.lastHitSwingId = m_attackSwingId;
            ++m_attackHitCountThisSwing;
            if (m_pAttackSe) PlaySound(m_pAttackSe);

            if (m_boss.maxHp > 0)
            {
                const float hpRate = Clamp01(static_cast<float>(m_boss.hp) / static_cast<float>(m_boss.maxHp));
                if (m_boss.phase < 2 && hpRate <= 0.50f)
                {
                    m_boss.phase = 2;
                    m_boss.specialUnlocked = true;
                    m_boss.forceUltimatePending = true;
                    if (m_boss.archetype == Transfer::RoguelikeUpgrade::BossFinalBarrage)
                    {
                        m_boss.finalPhaseTransitionPending = true;
                    }
                    if (m_boss.attackState == BossController::AttackIdle)
                    {
                        m_boss.attackCooldownTimer = 0.0f;
                    }
                }
                if (m_boss.phase < 3 && hpRate <= 0.25f)
                {
                    m_boss.phase = 3;
                    if (m_boss.archetype == Transfer::RoguelikeUpgrade::BossFinalBarrage)
                    {
                        m_boss.finalPhaseTransitionPending = true;
                    }
                    if (m_boss.attackState == BossController::AttackIdle &&
                        m_boss.attackCooldownTimer > 0.25f)
                    {
                        m_boss.attackCooldownTimer = 0.25f;
                    }
                }
            }

            if (m_boss.hp <= 0)
            {
                m_boss.attackZones.clear();
                m_boss.fallingRocks.clear();
                tran.gameplayDebug.bossBattleActive = 0;
                tran.gameplayDebug.upgradeSelectionPending = 0;
                tran.roguelike.selectionPending = 0;
                if (m_pClearSe) PlaySound(m_pClearSe);
                const int currentStageType = tran.GetCurrentRunStageType();
                const bool isFinalBossStage =
                    currentStageType == Transfer::RoguelikeUpgrade::StageFinalBoss ||
                    tran.roguelike.currentStageIndex >= (tran.GetRunStageCount() - 1);
                if (isFinalBossStage)
                {
                    tran.gameplayDebug.runTimerRunning = 0;
                    tran.gameplayDebug.runRecordedSec = tran.gameplayDebug.runElapsedSec;
                    tran.gameplayDebug.showBossResultTimer = 1;
                    tran.gameplayDebug.upgradeRerollRemain = 0;
                    tran.roguelike.rerollRemain = 0;
                    SceneManager::ChangeResult(SceneManager::ResultType::Win);
                    SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
                    return true;
                }

                tran.gameplayDebug.showBossResultTimer = 0;
                tran.BeginNextStageSelection();
                SceneManager::ChangeResult(SceneManager::ResultType::None);
                SceneManager::ChangeScene(SceneManager::SCENE_GAME);
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Draws boss telegraph zones on the floor during the telegraph state.
 */
void SceneGame::DrawBossTelegraphMarker() const
{
    // Telegraph markers are only shown during boss debug battle while an attack is charging.
    if (!m_pBossAttackRangeMarker ||
        !m_isBossBattleDebug ||
        m_boss.hp <= 0 ||
        m_boss.attackState != BossController::AttackTelegraph ||
        m_boss.attackZones.empty())
    {
        return;
    }

    const float telegraphSec = (m_boss.attackTelegraphDuration > 0.01f)
        ? m_boss.attackTelegraphDuration
        : 0.01f;
    const float telegraphRate = Clamp01(m_boss.attackStateTimer / telegraphSec);
    const DirectX::XMFLOAT4 outlineColor = GetBossZoneOutlineColor(m_boss.archetype);

    // Each zone can reveal at a different normalized time for staggered patterns.
    for (const auto& zone : m_boss.attackZones)
    {
        if (telegraphRate + 0.0001f < zone.revealStart)
        {
            continue;
        }

        float localRate = 1.0f;
        if (zone.revealStart < 0.999f)
        {
            localRate = Clamp01((telegraphRate - zone.revealStart) / (1.0f - zone.revealStart));
        }
        const float pulse = 0.50f + 0.50f * static_cast<float>(std::sin((telegraphRate + localRate) * DirectX::XM_PI * 6.0f));

        if (zone.safeZone)
        {
            // Safe zones get a faint outline so players can read the intended dodge spot.
            const DirectX::XMFLOAT3 outlineSize = { zone.size.x * 1.06f, zone.size.y, zone.size.z * 1.06f };
            DrawAttackMarkerTintLocal(
                m_pBossAttackRangeMarker,
                zone.center,
                outlineSize,
                { outlineColor.x, outlineColor.y, outlineColor.z, 0.14f + 0.12f * pulse },
                zone.yawDeg);
        }
        else
        {
            const DirectX::XMFLOAT3 outlineSize = { zone.size.x * 1.04f, zone.size.y, zone.size.z * 1.04f };
            DrawAttackMarkerTintLocal(
                m_pBossAttackRangeMarker,
                zone.center,
                outlineSize,
                { outlineColor.x, outlineColor.y, outlineColor.z, 0.10f + 0.10f * pulse },
                zone.yawDeg);
        }

        if (!zone.safeZone && zone.hasPath)
        {
            const DirectX::XMFLOAT3 endpointSize = {
                MaxFloat(zone.size.x * 1.15f, 0.35f),
                zone.size.y,
                MaxFloat(zone.size.x * 1.15f, 0.35f)
            };
            const DirectX::XMFLOAT4 endpointColor = {
                outlineColor.x,
                outlineColor.y,
                outlineColor.z,
                0.18f + 0.16f * pulse
            };
            DrawAttackMarkerTintLocal(
                m_pBossAttackRangeMarker,
                zone.startPos,
                endpointSize,
                endpointColor,
                zone.yawDeg);
            DrawAttackMarkerTintLocal(
                m_pBossAttackRangeMarker,
                zone.endPos,
                endpointSize,
                endpointColor,
                zone.yawDeg);
        }

        DirectX::XMFLOAT4 color = zone.color;
        if (!zone.safeZone)
        {
            color = LerpColor(color, GetBossAttackAccentColor(m_boss.archetype), 0.28f);
        }
        const float baseAlpha = zone.safeZone ? 0.12f : 0.22f;
        const float pulseAlpha = zone.safeZone ? 0.24f : 0.42f;
        color.w = baseAlpha + pulseAlpha * pulse;
        DrawAttackMarkerTintLocal(
            m_pBossAttackRangeMarker,
            zone.center,
            zone.size,
            color,
            zone.yawDeg);
    }
}

/**
 * @brief Draws boss falling-object visuals and ultimate-field drop previews.
 */
void SceneGame::DrawBossFallingObjects() const
{
    if (!m_isBossBattleDebug || !m_boss.rockTexture)
    {
        return;
    }

    if (m_boss.hp > 0 &&
        m_boss.scriptedAttack &&
        m_boss.attackKind == BossController::AttackKindCustom &&
        m_boss.scriptEntryIndex >= 0 &&
        m_boss.scriptEntryIndex < static_cast<int>(m_finalBossScript.attacks.size()))
    {
        const BossAttackScript::Attack& activeAttack = m_finalBossScript.attacks[m_boss.scriptEntryIndex];
        if (activeAttack.visual.enabled)
        {
            const int deliveryMode = BossAttackScript::NormalizeAttackDeliveryMode(activeAttack.deliveryMode);
            const bool isRemoteFalling = (deliveryMode == BossAttackScript::AttackDeliveryRemoteFalling);
            const bool isTelegraph = (m_boss.attackState == BossController::AttackTelegraph);
            const bool isActive = (m_boss.attackState == BossController::AttackDash);
            if (isTelegraph || isActive)
            {
                const float stateDuration = (isTelegraph ? m_boss.attackTelegraphDuration : m_boss.attackExecuteDuration);
                const float stateRate = Clamp01(m_boss.attackStateTimer / ((stateDuration > 0.01f) ? stateDuration : 0.01f));
                for (const auto& zone : m_boss.attackZones)
                {
                    if (zone.safeZone) continue;
                    if (isTelegraph)
                    {
                        continue;
                    }
                    DirectX::XMFLOAT3 drawSize = { activeAttack.visual.size.x, activeAttack.visual.size.y, activeAttack.visual.size.x };
                    float drawYawDeg = zone.yawDeg + activeAttack.visual.spinDegPerSec * m_boss.attackStateTimer;
                    if (!activeAttack.visual.billboard)
                    {
                        drawSize.x = MaxFloat(zone.size.x, 0.10f);
                        drawSize.y = MaxFloat(activeAttack.visual.size.y, 0.10f);
                        drawSize.z = MaxFloat(zone.size.z, 0.10f);
                        const bool isLaserLineVisual =
                            activeAttack.visual.texturePath.find("laser_line") != std::string::npos;
                        const bool isLongLineZone = (zone.size.z > zone.size.x * 1.10f);
                        if (isLaserLineVisual && isLongLineZone)
                        {
                            drawSize.x = MaxFloat(zone.size.z, 0.10f);
                            drawSize.z = MaxFloat(zone.size.x, 0.10f);
                            drawYawDeg += 90.0f;
                        }
                        if (activeAttack.visual.texturePath.find("metal_blade") != std::string::npos)
                        {
                            drawYawDeg += 180.0f;
                        }
                    }
                    DirectX::XMFLOAT3 drawPos = zone.center;
                    if (isRemoteFalling)
                    {
                        const float liftRate = isTelegraph ? (1.0f - stateRate) : (1.0f - stateRate);
                        drawPos.y = ClampRange(activeAttack.visual.spawnHeight * liftRate, 0.0f, 50.0f);
                    }
                    else
                    {
                        drawPos.y = zone.center.y + (activeAttack.visual.billboard ? (drawSize.y * 0.5f) : 0.02f);
                    }

                    const float alpha = isTelegraph ? (0.30f + 0.50f * stateRate) : 1.0f;
                    DirectX::XMFLOAT4 drawColor = LerpColor(
                        zone.color,
                        { 1.0f, 1.0f, 1.0f, 1.0f },
                        isTelegraph ? 0.32f : 0.12f);
                    drawColor.w = alpha;
                    if (activeAttack.visual.billboard)
                    {
                        DrawBillboardSpriteLocal(
                            m_boss.rockTexture,
                            m_pCamera,
                            drawPos,
                            drawSize,
                            drawColor);
                    }
                    else
                    {
                        DrawAttackMarkerTintLocal(
                            m_boss.rockTexture,
                            { drawPos.x, 0.02f, drawPos.z },
                            drawSize,
                            drawColor,
                            drawYawDeg);
                    }
                }
            }
        }
    }

    if (m_boss.hp > 0 &&
        m_boss.attackState == BossController::AttackTelegraph &&
        m_boss.attackKind == BossController::AttackKindUltimateField)
    {
        const float telegraphSec = (m_boss.attackTelegraphDuration > 0.01f)
            ? m_boss.attackTelegraphDuration
            : 0.01f;
        const float telegraphRate = Clamp01(m_boss.attackStateTimer / telegraphSec);
        const float finalDropStartRate = 6.0f / 7.0f;
        if (telegraphRate >= finalDropStartRate)
        {
            // The field ultimate shows its rocks only near the end of the telegraph.
            const float dropRate = Clamp01((telegraphRate - finalDropStartRate) / (1.0f - finalDropStartRate));
            const DirectX::XMFLOAT4 drawColor = GetBossVisualTint(m_boss.archetype, true);
            for (const auto& zone : m_boss.attackZones)
            {
                if (zone.safeZone)
                {
                    continue;
                }

                const float rockSpan = MaxFloat(zone.size.x, zone.size.z) * 0.82f;
                DirectX::XMFLOAT3 drawPos = zone.center;
                drawPos.y = rockSpan * (0.35f + 2.20f * (1.0f - dropRate));

                DrawBillboardSpriteLocal(
                    m_boss.rockTexture,
                    m_pCamera,
                    drawPos,
                    { rockSpan, rockSpan, rockSpan },
                    { drawColor.x, drawColor.y, drawColor.z, 0.35f + 0.55f * dropRate });
            }
        }
    }

    // Active temporary rock sprites keep falling until their timer expires.
    for (const auto& rock : m_boss.fallingRocks)
    {
        if (rock.timer <= 0.0f || rock.duration <= 0.0f)
        {
            continue;
        }

        const float t = Clamp01(rock.timer / rock.duration);
        DirectX::XMFLOAT3 drawPos = rock.pos;
        drawPos.y = rock.spawnHeight * t;

        const DirectX::XMFLOAT4 visualTint = GetBossVisualTint(m_boss.archetype, false);
        const DirectX::XMFLOAT4 color = {
            visualTint.x,
            visualTint.y,
            visualTint.z,
            0.45f + 0.55f * (1.0f - t)
        };
        if (rock.billboard)
        {
            DrawBillboardSpriteLocal(
                m_boss.rockTexture,
                m_pCamera,
                drawPos,
                rock.size,
                color);
        }
        else
        {
            DrawAttackMarkerTintLocal(
                m_boss.rockTexture,
                { drawPos.x, 0.02f, drawPos.z },
                rock.size,
                color,
                rock.angleDeg);
        }
    }
}

/**
 * @brief Adds the boss to the distance-sorted draw list.
 * @param drawEntries Draw entry list to append to.
 * @param cam Camera position.
 */
void SceneGame::AddBossDrawEntry(std::vector<DrawEntry>& drawEntries, const DirectX::XMFLOAT3& cam) const
{
    if (!m_isBossBattleDebug || m_boss.hp <= 0 || !m_boss.texture)
    {
        return;
    }

    const DirectX::XMFLOAT3 bossCenter = {
        m_boss.pos.x,
        m_boss.pos.y + m_boss.size.y * 0.5f,
        m_boss.pos.z
    };
    const float dx = bossCenter.x - cam.x;
    const float dy = bossCenter.y - cam.y;
    const float dz = bossCenter.z - cam.z;
    drawEntries.push_back({ dx * dx + dy * dy + dz * dz, false, true, bossCenter, m_boss.size, nullptr });
}

/**
 * @brief Draws the boss entry with state-based visual modulation.
 * @param entry Sorted draw entry to render.
 */
void SceneGame::DrawBossEntry(const DrawEntry& entry) const
{
    if (!entry.isBoss || !m_boss.texture)
    {
        return;
    }

    const DirectX::XMFLOAT3 bossBottom = {
        entry.pos.x,
        entry.pos.y - entry.size.y * 0.5f,
        entry.pos.z
    };

    DirectX::XMFLOAT3 drawBottom = bossBottom;
    DirectX::XMFLOAT3 drawSize = entry.size;
    DirectX::XMFLOAT4 drawColor = m_boss.color;
    const DirectX::XMFLOAT4 accentColor = GetBossAttackAccentColor(m_boss.archetype);

    switch (m_boss.archetype)
    {
    case Transfer::RoguelikeUpgrade::BossHeavyMelee:
        drawSize.x *= 1.06f;
        drawSize.z *= 1.06f;
        drawColor = MulColor(drawColor, 0.96f);
        break;

    case Transfer::RoguelikeUpgrade::BossLightRanged:
        drawBottom.y += entry.size.y * 0.04f;
        drawSize.x *= 0.94f;
        drawSize.z *= 0.94f;
        drawColor = LerpColor(drawColor, accentColor, 0.16f);
        break;

    case Transfer::RoguelikeUpgrade::BossSwiftDebuff:
        drawBottom.y += entry.size.y * 0.02f;
        drawSize.y *= 0.96f;
        drawColor = LerpColor(drawColor, accentColor, 0.20f);
        break;

    case Transfer::RoguelikeUpgrade::BossFinalBarrage:
        drawSize.x *= 1.02f;
        drawSize.z *= 1.02f;
        drawColor = LerpColor(drawColor, accentColor, 0.24f);
        break;

    case Transfer::RoguelikeUpgrade::BossBalancedMid:
    default:
        drawColor = LerpColor(drawColor, accentColor, 0.10f);
        break;
    }

    // During telegraph, the boss pose and tint shift to hint at the next attack type.
    if (m_boss.attackState == BossController::AttackTelegraph)
    {
        const float telegraphSec = (m_boss.attackTelegraphDuration > 0.01f)
            ? m_boss.attackTelegraphDuration
            : 0.01f;
        const float telegraphRate = Clamp01(m_boss.attackStateTimer / telegraphSec);
        const float pulse = 0.5f + 0.5f * static_cast<float>(std::sin(telegraphRate * DirectX::XM_PI * 6.0f));

        switch (m_boss.attackKind)
        {
        case BossController::AttackKindDashNarrow:
        case BossController::AttackKindDashWide:
            drawSize.x *= 1.08f + 0.10f * telegraphRate;
            drawSize.z *= 1.08f + 0.10f * telegraphRate;
            drawSize.y *= 1.0f - 0.14f * telegraphRate;
            drawColor = LerpColor(accentColor, { 1.0f, 0.16f, 0.12f, 1.0f }, 0.28f + 0.28f * pulse);
            break;

        case BossController::AttackKindSummon:
            drawSize.x *= 1.0f + 0.08f * pulse;
            drawSize.y *= 1.0f + 0.05f * pulse;
            drawSize.z *= 1.0f + 0.08f * pulse;
            drawColor = LerpColor(accentColor, { 1.0f, 0.78f, 0.20f, 1.0f }, 0.36f + 0.20f * pulse);
            break;

        case BossController::AttackKindRandomRain:
        case BossController::AttackKindTrackingDrop:
            drawBottom.y += entry.size.y * (0.06f + 0.10f * telegraphRate);
            drawSize.x *= 1.0f + 0.04f * pulse;
            drawSize.z *= 1.0f + 0.04f * pulse;
            drawColor = LerpColor(accentColor, { 0.88f, 0.94f, 1.0f, 1.0f }, 0.24f + 0.30f * pulse);
            break;

        case BossController::AttackKindUltimateCross:
        case BossController::AttackKindUltimateField:
            drawSize.x *= 1.05f + 0.08f * telegraphRate;
            drawSize.y *= 1.03f + 0.05f * telegraphRate;
            drawSize.z *= 1.05f + 0.08f * telegraphRate;
            drawColor = LerpColor(accentColor, { 1.0f, 0.18f, 0.16f, 1.0f }, 0.42f + 0.24f * pulse);
            break;

        case BossController::AttackKindUltimateStomp:
            drawColor = LerpColor(accentColor, { 1.0f, 0.28f, 0.18f, 1.0f }, 0.28f + 0.22f * pulse);
            break;

        default:
            drawColor = LerpColor(drawColor, accentColor, 0.12f + 0.18f * pulse);
            break;
        }
    }

    // Broken state dims the base sprite and adds a separate broken icon overlay.
    if (m_boss.isBroken)
    {
        drawColor.x = MaxFloat(drawColor.x, 0.30f);
        drawColor.y *= 0.75f;
        drawColor.z *= 0.75f;
    }

    DrawBillboardSpriteLocal(m_boss.texture, m_pCamera, drawBottom, drawSize, drawColor);

    if (!m_boss.isBroken)
    {
        const float auraPulse = 0.70f + 0.30f *
            static_cast<float>(std::sin(m_boss.attackStateTimer * 4.0f + static_cast<float>(m_boss.archetype)));
        const DirectX::XMFLOAT3 auraPos = {
            entry.pos.x,
            entry.pos.y + entry.size.y * 0.54f,
            entry.pos.z
        };
        const float auraScale = MaxFloat(entry.size.x, entry.size.z) * (1.10f + 0.05f * auraPulse);
        DrawBillboardSpriteLocal(
            m_boss.texture,
            m_pCamera,
            auraPos,
            { auraScale, auraScale, auraScale },
            { accentColor.x, accentColor.y, accentColor.z, 0.08f + 0.08f * auraPulse });
    }

    if (m_boss.isBroken && m_boss.brokenTexture)
    {
        const float pulse = 0.65f + 0.35f * static_cast<float>(std::sin(m_boss.breakRecoverTimer * 8.0f));
        DirectX::XMFLOAT3 brokenPos = {
            entry.pos.x,
            entry.pos.y + entry.size.y * 0.82f,
            entry.pos.z
        };
        const float brokenSize = MaxFloat(entry.size.x, entry.size.z) * 0.72f;
        DrawBillboardSpriteLocal(
            m_boss.brokenTexture,
            m_pCamera,
            brokenPos,
            { brokenSize, brokenSize, brokenSize },
            { 1.0f, 1.0f, 1.0f, 0.60f + 0.40f * pulse });
    }
}

/**
 * @brief Draws the screen-space boss HP and guard UI.
 */
void SceneGame::DrawBossHpUi() const
{
    if (!m_isBossBattleDebug || m_boss.maxHp <= 0)
    {
        return;
    }

    auto& tran = Transfer::GetInstance();
    const float hpRate = Clamp01(static_cast<float>(m_boss.hp) / static_cast<float>(m_boss.maxHp));
    const float burnRate = Clamp01((static_cast<float>(m_boss.hp) + MaxFloat(0.0f, m_bossBurnPool)) / static_cast<float>(m_boss.maxHp));
    const float guardRate = (m_boss.guardMax > 0.01f)
        ? Clamp01(m_boss.guard / m_boss.guardMax)
        : 0.0f;
    DrawBossHpOverlayLocal(
        hpRate,
        burnRate,
        guardRate,
        m_boss.isBroken,
        tran.gameplay.bossHpBarWidthRate,
        tran.gameplay.bossHpBarHeightRate,
        tran.gameplay.bossGuardBarOffsetX,
        tran.gameplay.bossGuardBarOffsetY,
        tran.gameplay.bossGuardBarWidthRate,
        tran.gameplay.bossGuardBarHeightRate,
        m_bossChainCount,
        m_pChainUiTexture);
}


