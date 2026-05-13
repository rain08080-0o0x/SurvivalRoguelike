#include "SceneGame.h"
#include "Enemy.h"
#include "CameraDebug.h"
#include "Geometory.h"
#include "Sprite.h"
#include "DirectX.h"
#include "Input.h"
#include "Defines.h"
#include "Transfer.h"
#include "Main.h"
#include "SceneManager.h"
#include "UIObject.h"
#include "Collision.h"
#include "ParticleEffectPreset.h"
#include "Texture.h"
#include "Sound.h"
#include "imgui.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
    // UI layout and gameplay guard values used inside SceneGame only.
    const float kUiMargin = 20.0f;
    const float kHpFrameWidth = 260.0f;
    const float kHpFrameHeight = 32.0f;
    const float kHpGaugePadding = 4.0f;
    const float kCooldownFrameWidth = 220.0f;
    const float kCooldownFrameHeight = 24.0f;
    const float kCooldownGaugePadding = 3.0f;
    const float kCooldownRowSpacing = 8.0f;
    const int kMultiHitShakeThresholdMin = 1;
    const int kMultiHitShakeThresholdMax = 16;
    const float kMultiHitShakeDurationDefault = 0.18f;
    const float kBossStompImpactFxDuration = 0.18f;
    const float kBossStompImpactShadowScale = 1.75f;
    const float kFixedDt = 1.0f / 60.0f;
    const int kEnemyCountMin = 0;
    const int kEnemyCountMax = 16;
    const int kWaveCountMin = 1;
    const int kWaveCountMax = 32;
    const float kMinDuration = 0.01f;
    const float kCameraIntroDurationMin = 0.10f;
    const float kCameraIntroDurationMax = 8.0f;
    const float kCameraIntroFocusDistanceMin = 0.50f;
    const float kCameraIntroFocusDistanceMax = 12.0f;
    const float kCameraIntroExpStrength = 5.0f;
    const float kCameraIntroEyeMoveRatio = 0.42f;
    const float kCameraIntroEyeLift = 0.45f;
    const float kPi = 3.14159265f;
    const float kEnemyHpBillboardWidthScale = 2.0f;
    const float kEnemyHpBillboardHeightScale = 0.2f;
    const float kEnemyHpBillboardPaddingScale = 0.2f;
    const float kEnemyHpBillboardOffsetScale = 0.7f;
    const float kEnemyHpBillboardMinWidth = 0.6f;
    const float kEnemyHpBillboardMinHeight = 0.1f;
    const float kBloodStockUiSpacing = 2.0f;
    const float kBloodStockUiBaseSize = 14.0f;
    const float kBloodStockUiMaxWidth = 252.0f;
    const float kChainBeamDuration = 0.16f;
    const float kChainBeamSegmentSpacing = 0.22f;
    const float kChainBeamSegmentSize = 0.28f;
    const float kDebugRangeBoxHeight = 0.05f;
    const DirectX::XMFLOAT4 kDebugEnemyColorOutRange = { 0.0f, 1.0f, 0.0f, 1.0f };
    const DirectX::XMFLOAT4 kDebugEnemyColorInRangeCooling = { 0.0f, 0.8f, 1.0f, 1.0f };
    const DirectX::XMFLOAT4 kBloodUiTint = { 0.72f, 0.08f, 0.16f, 0.96f };
    const DirectX::XMFLOAT4 kChainBeamTint = { 0.88f, 0.92f, 0.98f, 0.92f };
    const DirectX::XMFLOAT4 kBurnGaugeColor = { 1.0f, 0.55f, 0.08f, 0.92f };
    const DirectX::XMFLOAT4 kDebugEnemyColorReady = { 1.0f, 1.0f, 0.0f, 1.0f };
    const DirectX::XMFLOAT4 kDebugEnemyColorWindup = { 1.0f, 0.4f, 0.0f, 1.0f };
    const DirectX::XMFLOAT4 kDebugEnemyColorHit = { 1.0f, 0.0f, 0.0f, 1.0f };
    const DirectX::XMFLOAT4 kDebugRangeColorOut = { 0.2f, 0.2f, 0.7f, 1.0f };
    const DirectX::XMFLOAT4 kDebugRangeColorIn = { 0.0f, 0.8f, 1.0f, 1.0f };
    const DirectX::XMFLOAT4 kEnemyProjectileColor = { 0.25f, 0.95f, 1.0f, 0.90f };
    const DirectX::XMFLOAT3 kEnemySpawnPositions[] =
    {
        { 2.0f, 0.0f, 0.0f },
        {-2.0f, 0.0f, 1.5f },
        { 0.0f, 0.0f,-2.0f },
    };
    const int kEnemySpawnPresetCount = static_cast<int>(sizeof(kEnemySpawnPositions) / sizeof(kEnemySpawnPositions[0]));

    const int kCameraModeGame = 0;
    const int kCameraModeDebug = 1;
    const int kPauseMenuContinue = 0;
    const int kPauseMenuOption = 1;
    const int kPauseMenuToTitle = 2;
    const int kPauseTabGame = 0;
    const int kPauseTabUpgrade = 1;
    const int kPauseTabSettings = 2;
    const int kPauseTabCount = 3;
    const int kPauseOptionMaster = 0;
    const int kPauseOptionBgm = 1;
    const int kPauseOptionSe = 2;
    const int kPauseOptionDisplay = 3;
    const int kPauseOptionBack = 4;
    const int kPauseOptionCount = 5;
    const float kOptionVolumeStep = 0.05f;
    const float kSkillShotRadius = 0.28f;
    const float kSkillShotSpeed = 11.5f;
    const float kSkillNovaRadiusScale = 2.6f;
    const float kSkillNovaHeightScale = 1.2f;
    const float kSkillNovaDamageScale = 1.8f;
    const float kSkillOrbitDuration = 5.0f;
    const float kSkillOrbitAngularSpeed = 5.6f;
    const float kSkillOrbitRadiusScale = 1.75f;
    const float kSkillOrbitHeightOffsetScale = 0.45f;
    const float kSkillOrbitDamageScale = 1.0f;
    const float kSkillOrbitContactCooldown = 0.35f;
    const float kSkillOrbitBillboardScale = 0.75f;
    const float kWeaponCritBaseDamageScale = 1.5f;
    const float kWeaponDamageBuffDuration = 3.0f;
    const float kWeaponSkillAmpDuration = 4.0f;
    const float kWeaponProjectileRadius = 0.20f;
    const float kWeaponProjectileSpeed = 14.0f;
    const float kWeaponProjectileMaxDistance = 12.0f;
    const float kSkillWhirlCooldown = 12.0f;
    const float kSkillWhirlDamageScale = 7.5f;
    const float kSkillRushCooldown = 6.0f;
    const float kSkillRushDamageScale = 3.0f;
    const float kSkillAmbushCooldown = 8.0f;
    const float kSkillAmbushDamageScale = 5.0f;
    const float kSkillChainThrowCooldown = 8.0f;
    const float kSkillChainThrowDamageScale = 2.0f;
    const float kSkillFireballCooldown = 10.0f;
    const float kSkillFireballDamageScale = 4.5f;
    const float kSkillBloodSlashCooldown = 4.0f;
    const float kSkillBloodSlashDamageScale = 3.5f;
    const float kSkillRushDistance = 2.4f;
    const float kSkillChainThrowRange = 3.2f;
    const float kSkillFireballRadius = 0.32f;
    const float kSkillFireballSpeed = 13.0f;
    const float kSkillSlashProjectileRadius = 0.26f;
    const float kSkillSlashProjectileSpeed = 11.0f;
    const float kCombatEffectDuration = 0.22f;
    const float kCombatEffectGrowScale = 1.12f;
    const float kCombatEffectScale = 1.15f;
    const float kCombatEffectMinSize = 0.8f;
    const float kCombatEffectMaxSize = 8.0f;
    const int kCombatEffectColumns = 5;
    const int kCombatEffectRows = 1;
    const int kCombatEffectFrameCount = kCombatEffectColumns * kCombatEffectRows;
    const int kMaxHitEffectEmitters = 5;
    const float kHitEffectCullMargin = 0.25f;
    const float kBloodOrbLifetime = 10.0f;
    const int kBloodOrbFieldCap = 20;
    const float kBloodFireSynergyRadius = 5.0f;
    const DirectX::XMFLOAT4 kChallengeHazardColor = { 1.0f, 0.28f, 0.18f, 0.80f };
    const DirectX::XMFLOAT4 kChallengeHazardResolvedColor = { 1.0f, 0.75f, 0.20f, 0.45f };
    const DirectX::XMFLOAT4 kChallengeBeaconColor = { 0.20f, 1.0f, 0.65f, 0.85f };
    const DirectX::XMFLOAT4 kChallengeBeaconCoreColor = { 0.85f, 1.0f, 0.25f, 0.95f };
    const char* kDirectionMarkerTexturePath = "Assets/Texture/Chracter/triangle.png";

    /**
     * @brief カメラモード値を有効な 2 値へ正規化します。
     * @param mode 補正対象のモード値です。
     * @return Debug 指定時のみ Debug、それ以外は Game です。
     */
    int NormalizeCameraMode(int mode)
    {
        return (mode == kCameraModeDebug) ? kCameraModeDebug : kCameraModeGame;
    }

    /**
     * @brief インデックスを 0 から count-1 の範囲に循環させます。
     * @param value 補正対象の値です。
     * @param count 要素数です。
     * @return 折り返し済みインデックスです。
     */
    int WrapIndex(int value, int count)
    {
        // 項目数が無い場合は安全側で 0 を返します。
        if (count <= 0) return 0;
        int wrapped = value % count;
        // 負方向に動かした時も先頭未満にならないように折り返します。
        if (wrapped < 0) wrapped += count;
        return wrapped;
    }

    /**
     * @brief ポーズ系 UI の決定入力が押されたかを返します。
     * @return Enter / F / Space のいずれかがトリガーなら true です。
     */
    bool IsPauseConfirmTriggered()
    {
        return IsMenuConfirmTrigger();
    }

    bool IsPauseTabPrevTriggered()
    {
        return IsPadLeftShoulderTrigger();
    }

    bool IsPauseTabNextTriggered()
    {
        return IsPadRightShoulderTrigger();
    }

    /**
     * @brief 音量値を UI で扱う範囲に丸めます。
     * @param value 補正対象音量です。
     * @return 0.0 から 2.0 に丸めた音量です。
     */
    float ClampVolume(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 2.0f) return 2.0f;
        return value;
    }

    /**
     * @brief 敵スポーン位置をインデックスから決定します。
     * @param index 敵番号です。
     * @param stageSize 現在のステージサイズです。
     * @return スポーン用ワールド座標です。
     */
    DirectX::XMFLOAT3 CalcEnemySpawnPos(int index, float stageSize)
    {
        // 先頭数体は決め打ち位置を使い、序盤の見え方を安定させます。
        if (index >= 0 && index < kEnemySpawnPresetCount)
        {
            return kEnemySpawnPositions[index];
        }

        // それ以外は円周上に均等配置し、ステージ外へ出にくい半径を使います。
        const float safeStage = (stageSize > 0.5f) ? stageSize : 5.0f;
        const float radius = safeStage * 0.35f;
        const float angle = (2.0f * kPi * static_cast<float>(index)) / static_cast<float>(kEnemyCountMax);
        return { std::cos(angle) * radius, 0.0f, std::sin(angle) * radius };
    }

    /**
     * @brief カメラが存在する場合だけ Eye / Look を反映します。
     * @param camera 更新するカメラです。
     * @param eye 新しい Eye です。
     * @param look 新しい Look です。
     */
    void ApplyCameraPose(CameraDebug* camera, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& look)
    {
        if (camera)
        {
            camera->SetPose(eye, look);
        }
    }


    /**
     * @brief 浮動小数を 0.0 から 1.0 に丸めます。
     * @param v 補正対象です。
     * @return 0.0 から 1.0 に収めた値です。
     */
    float Clamp01(float v)
    {
        if (v < 0.0f) return 0.0f;
        if (v > 1.0f) return 1.0f;
        return v;
    }

    float MaxFloat3(const DirectX::XMFLOAT3& value)
    {
        float result = value.x;
        if (result < value.y) result = value.y;
        if (result < value.z) result = value.z;
        return result;
    }

    /**
     * @brief 3 次元ベクトルを線形補間します。
     * @param a 開始値です。
     * @param b 終了値です。
     * @param t 補間係数です。
     * @return a から b の間を補間したベクトルです。
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

    /**
     * @brief 3 次元ベクトルを正規化します。
     * @param v 正規化対象です。
     * @param fallback 長さ 0 に近い場合に返す代替値です。
     * @return 正規化ベクトル、または fallback です。
     */
    DirectX::XMFLOAT3 Normalize3(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT3& fallback)
    {
        const float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
        if (lenSq <= 1.0e-6f)
        {
            // 長さが無い方向は正規化できないため、呼び出し側の既定方向へ逃がします。
            return fallback;
        }
        const float invLen = 1.0f / std::sqrt(lenSq);
        return { v.x * invLen, v.y * invLen, v.z * invLen };
    }

    /**
     * @brief カメラ導入演出に使う指数イージングを返します。
     * @param t 0.0 から 1.0 の進行率です。
     * @return イージング後の進行率です。
     */
    float ExpEase01(float t)
    {
        const float clamped = Clamp01(t);
        const float denom = 1.0f - expf(-kCameraIntroExpStrength);
        if (denom <= 1.0e-6f)
        {
            // 分母が壊れる場合は素直な線形値へフォールバックします。
            return clamped;
        }
        return (1.0f - expf(-kCameraIntroExpStrength * clamped)) / denom;
    }

    /**
     * @brief 浮動小数を任意範囲へ丸めます。
     * @param v 補正対象値です。
     * @param lo 下限です。
     * @param hi 上限です。
     * @return 指定範囲に収めた値です。
     */
    float ClampRange(float v, float lo, float hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    /**
     * @brief 整数を任意範囲へ丸めます。
     * @param v 補正対象値です。
     * @param lo 下限です。
     * @param hi 上限です。
     * @return 指定範囲に収めた値です。
     */
    int ClampInt(int v, int lo, int hi)
    {
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    float MaxFloat(float a, float b)
    {
        return (a > b) ? a : b;
    }

    bool IsPointOutsideCameraClip(Camera* camera, const DirectX::XMFLOAT3& pos, float margin)
    {
        if (!camera)
        {
            return false;
        }

        using namespace DirectX;
        const XMFLOAT4X4 viewFloat = camera->GetViewMatrix(false);
        const XMFLOAT4X4 projFloat = camera->GetProjectionMatrix(false);
        const XMMATRIX view = XMLoadFloat4x4(&viewFloat);
        const XMMATRIX proj = XMLoadFloat4x4(&projFloat);
        const XMVECTOR worldPos = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
        const XMVECTOR viewPos = XMVector4Transform(worldPos, view);
        const XMVECTOR clipPos = XMVector4Transform(viewPos, proj);

        XMFLOAT4 clip{};
        XMStoreFloat4(&clip, clipPos);
        if (clip.w <= 0.001f)
        {
            return true;
        }

        const float invW = 1.0f / clip.w;
        const float ndcX = clip.x * invW;
        const float ndcY = clip.y * invW;
        const float ndcZ = clip.z * invW;
        const float bound = 1.0f + margin;
        return
            ndcX < -bound || ndcX > bound ||
            ndcY < -bound || ndcY > bound ||
            ndcZ < 0.0f || ndcZ > bound;
    }

    /**
     * @brief 難易度による基準敵数補正を返します。
     * @param preset 難易度です。
     * @return Easy は -1、Hard は +1、Normal は 0 です。
     */
    int CalcDifficultyBaseEnemyBonus(int preset)
    {
        switch (preset)
        {
        case 0: return -1; // Easy
        case 2: return 1;  // Hard
        default: return 0; // Normal
        }
    }

    /**
     * @brief 難易度による Wave ごとの追加敵数補正を返します。
     * @param preset 難易度です。
     * @return Hard のみ +1、それ以外は 0 です。
     */
    int CalcDifficultyWaveAddBonus(int preset)
    {
        switch (preset)
        {
        case 0: return 0; // Easy
        case 2: return 1; // Hard
        default: return 0; // Normal
        }
    }

    /**
     * @brief 難易度による敵攻撃ダメージ倍率を返します。
     * @param preset 難易度です。
     * @return 難易度に応じたダメージ倍率です。
     */
    float CalcDifficultyEnemyAttackDamageScale(int preset)
    {
        switch (preset)
        {
        case 0: return 0.85f; // Easy
        case 2: return 1.25f; // Hard
        default: return 1.0f; // Normal
        }
    }

    /**
     * @brief XZ 平面上の距離二乗を返します。
     * @param a 座標 A です。
     * @param b 座標 B です。
     * @return XZ 平面距離の二乗です。
     */
    float DistSqXZ(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
    {
        const float dx = a.x - b.x;
        const float dz = a.z - b.z;
        return dx * dx + dz * dz;
    }

    /**
     * @brief XZ 平面成分だけを正規化します。
     * @param v 正規化対象ベクトルです。
     * @param fallback 長さ 0 に近い場合の代替値です。
     * @return XZ 正規化ベクトル、または fallback です。
     */
    DirectX::XMFLOAT3 NormalizeXZ(const DirectX::XMFLOAT3& v, const DirectX::XMFLOAT3& fallback)
    {
        const float len = std::sqrt(v.x * v.x + v.z * v.z);
        if (len <= 1.0e-6f)
        {
            return fallback;
        }
        return { v.x / len, 0.0f, v.z / len };
    }

    /**
     * @brief 中心とサイズから AABB を生成します。
     * @param center ボックス中心です。
     * @param size ボックスサイズです。
     * @return 組み立てた AABB です。
     */
    Collision::Box MakeAabb(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size)
    {
        Collision::Box box{};
        box.center = center;
        box.size = size;
        return box;
    }

    bool IsPointInsideUiRect(const POINT& point, const DirectX::XMFLOAT2& center, const DirectX::XMFLOAT2& size)
    {
        const float halfW = size.x * 0.5f;
        const float halfH = size.y * 0.5f;
        const float minX = center.x - halfW;
        const float maxX = center.x + halfW;
        const float minY = center.y - halfH;
        const float maxY = center.y + halfH;
        return
            static_cast<float>(point.x) >= minX &&
            static_cast<float>(point.x) <= maxX &&
            static_cast<float>(point.y) >= minY &&
            static_cast<float>(point.y) <= maxY;
    }

    bool IntersectRayAabb(const DirectX::XMFLOAT3& origin,
                          const DirectX::XMFLOAT3& dir,
                          const Collision::Box& box,
                          float& outT)
    {
        const DirectX::XMFLOAT3 half = { box.size.x * 0.5f, box.size.y * 0.5f, box.size.z * 0.5f };
        const DirectX::XMFLOAT3 minP = { box.center.x - half.x, box.center.y - half.y, box.center.z - half.z };
        const DirectX::XMFLOAT3 maxP = { box.center.x + half.x, box.center.y + half.y, box.center.z + half.z };

        float tMin = 0.0f;
        float tMax = 1.0e9f;
        const float originAxis[3] = { origin.x, origin.y, origin.z };
        const float dirAxis[3] = { dir.x, dir.y, dir.z };
        const float minAxis[3] = { minP.x, minP.y, minP.z };
        const float maxAxis[3] = { maxP.x, maxP.y, maxP.z };

        for (int axis = 0; axis < 3; ++axis)
        {
            if (std::fabs(dirAxis[axis]) <= 1.0e-6f)
            {
                if (originAxis[axis] < minAxis[axis] || originAxis[axis] > maxAxis[axis])
                {
                    return false;
                }
                continue;
            }

            const float invDir = 1.0f / dirAxis[axis];
            float t1 = (minAxis[axis] - originAxis[axis]) * invDir;
            float t2 = (maxAxis[axis] - originAxis[axis]) * invDir;
            if (t1 > t2)
            {
                const float tmp = t1;
                t1 = t2;
                t2 = tmp;
            }
            if (t1 > tMin) tMin = t1;
            if (t2 < tMax) tMax = t2;
            if (tMin > tMax)
            {
                return false;
            }
        }

        outT = (tMin >= 0.0f) ? tMin : tMax;
        return outT >= 0.0f;
    }

    void BuildCursorRay(Camera* camera, const POINT& mousePos, DirectX::XMFLOAT3& outOrigin, DirectX::XMFLOAT3& outDir)
    {
        DirectX::XMFLOAT4X4 viewF = camera->GetViewMatrix(false);
        DirectX::XMFLOAT4X4 projF = camera->GetProjectionMatrix(false);
        const DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&viewF);
        const DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&projF);
        const DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

        const DirectX::XMVECTOR nearPoint = DirectX::XMVector3Unproject(
            DirectX::XMVectorSet(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y), 0.0f, 1.0f),
            0.0f,
            0.0f,
            static_cast<float>(SCREEN_WIDTH),
            static_cast<float>(SCREEN_HEIGHT),
            0.0f,
            1.0f,
            proj,
            view,
            world);
        const DirectX::XMVECTOR farPoint = DirectX::XMVector3Unproject(
            DirectX::XMVectorSet(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y), 1.0f, 1.0f),
            0.0f,
            0.0f,
            static_cast<float>(SCREEN_WIDTH),
            static_cast<float>(SCREEN_HEIGHT),
            0.0f,
            1.0f,
            proj,
            view,
            world);

        DirectX::XMStoreFloat3(&outOrigin, nearPoint);
        DirectX::XMVECTOR dirVec = DirectX::XMVector3Normalize(DirectX::XMVectorSubtract(farPoint, nearPoint));
        DirectX::XMStoreFloat3(&outDir, dirVec);
    }

    int NormalizeWeaponType(int weaponType)
    {
        return ClampInt(
            weaponType,
            Transfer::RoguelikeUpgrade::WeaponBasic,
            Transfer::RoguelikeUpgrade::WeaponRanged);
    }

    int NormalizeActionSkillType(int skillType)
    {
        return ClampInt(
            skillType,
            Transfer::RoguelikeUpgrade::ActionSkillNone,
            Transfer::RoguelikeUpgrade::ActionSkillBloodSlash);
    }

    float GetWeaponAttacksPerSecond(int weaponType)
    {
        switch (NormalizeWeaponType(weaponType))
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
            return 0.8f;
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            return 4.0f;
        case Transfer::RoguelikeUpgrade::WeaponRanged:
            return 1.0f;
        case Transfer::RoguelikeUpgrade::WeaponBasic:
        default:
            return 1.5f;
        }
    }

    int GetWeaponCritNeed(const Transfer::RoguelikeUpgrade& roguelike, int weaponType)
    {
        int critNeed = 3;
        switch (NormalizeWeaponType(weaponType))
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            critNeed = 2;
            break;
        case Transfer::RoguelikeUpgrade::WeaponRanged:
            critNeed = 1;
            break;
        default:
            critNeed = 3;
            break;
        }

        if (roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeCritNeedReduce] != 0 && critNeed > 1)
        {
            --critNeed;
        }
        if (critNeed < 1) critNeed = 1;
        return critNeed;
    }

    float GetWeaponDamageScale(const Transfer::RoguelikeUpgrade& roguelike, int weaponType)
    {
        float scale = 1.0f;
        switch (NormalizeWeaponType(weaponType))
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
            scale = 1.35f;
            break;
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            scale = 0.62f;
            break;
        case Transfer::RoguelikeUpgrade::WeaponRanged:
            scale = 1.10f;
            break;
        default:
            scale = 1.0f;
            break;
        }

        const int weaponTraitLevel = ClampInt(
            roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitWeapon],
            0,
            Transfer::RoguelikeUpgrade::kTraitLevelMax);
        scale += 0.10f * static_cast<float>(weaponTraitLevel);
        if (weaponTraitLevel >= 2)
        {
            scale += 0.25f;
        }
        return scale;
    }

    float GetWeaponAttackSpeedScale(const Transfer::RoguelikeUpgrade& roguelike)
    {
        (void)roguelike;
        return 1.0f;
    }

    float GetWeaponKnockbackScale(int weaponType)
    {
        switch (NormalizeWeaponType(weaponType))
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
            return 1.45f;
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            return 0.0f;
        case Transfer::RoguelikeUpgrade::WeaponRanged:
            return 0.85f;
        default:
            return 1.0f;
        }
    }

    bool WeaponUsesProjectile(int weaponType)
    {
        return NormalizeWeaponType(weaponType) == Transfer::RoguelikeUpgrade::WeaponRanged;
    }

    float GetActionSkillBaseCooldown(int skillType)
    {
        switch (NormalizeActionSkillType(skillType))
        {
        case Transfer::RoguelikeUpgrade::ActionSkillWhirl:
            return kSkillWhirlCooldown;
        case Transfer::RoguelikeUpgrade::ActionSkillRush:
            return kSkillRushCooldown;
        case Transfer::RoguelikeUpgrade::ActionSkillAmbush:
            return kSkillAmbushCooldown;
        case Transfer::RoguelikeUpgrade::ActionSkillChainThrow:
            return kSkillChainThrowCooldown;
        case Transfer::RoguelikeUpgrade::ActionSkillFireball:
            return kSkillFireballCooldown;
        case Transfer::RoguelikeUpgrade::ActionSkillBloodSlash:
            return kSkillBloodSlashCooldown;
        default:
            return 0.0f;
        }
    }

    DirectX::XMFLOAT3 CalcOrbitSatellitePos(float baseAngle,
                                            float radius,
                                            int count,
                                            const DirectX::XMFLOAT3& playerPos,
                                            float playerHeight,
                                            int index)
    {
        const int safeCount = (count > 0) ? count : 1;
        const float angle = baseAngle + (2.0f * kPi * static_cast<float>(index) / static_cast<float>(safeCount));
        return {
            playerPos.x + std::cos(angle) * radius,
            playerPos.y + playerHeight * kSkillOrbitHeightOffsetScale,
            playerPos.z + std::sin(angle) * radius
        };
    }

    /**
     * @brief AABB 同士のヒット判定を返します。
     * @param a 判定対象 A です。
     * @param b 判定対象 B です。
     * @return ヒットしていれば true です。
     */
    bool HitAabb(const Collision::Box& a, const Collision::Box& b)
    {
        // Gameplay collision policy: use AABB (Box vs Box) only.
        return Collision::Hit(a, b).isHit;
    }

    void AddAabbLines(const Collision::Box& box, const DirectX::XMFLOAT4& color)
    {
        const float hx = box.size.x * 0.5f;
        const float hy = box.size.y * 0.5f;
        const float hz = box.size.z * 0.5f;
        const DirectX::XMFLOAT3 c = box.center;

        DirectX::XMFLOAT3 v[8] =
        {
            {c.x - hx, c.y - hy, c.z - hz},
            {c.x + hx, c.y - hy, c.z - hz},
            {c.x - hx, c.y + hy, c.z - hz},
            {c.x + hx, c.y + hy, c.z - hz},
            {c.x - hx, c.y - hy, c.z + hz},
            {c.x + hx, c.y - hy, c.z + hz},
            {c.x - hx, c.y + hy, c.z + hz},
            {c.x + hx, c.y + hy, c.z + hz},
        };

        Geometory::AddLine(v[0], v[1], color);
        Geometory::AddLine(v[1], v[3], color);
        Geometory::AddLine(v[3], v[2], color);
        Geometory::AddLine(v[2], v[0], color);

        Geometory::AddLine(v[4], v[5], color);
        Geometory::AddLine(v[5], v[7], color);
        Geometory::AddLine(v[7], v[6], color);
        Geometory::AddLine(v[6], v[4], color);

        Geometory::AddLine(v[0], v[4], color);
        Geometory::AddLine(v[1], v[5], color);
        Geometory::AddLine(v[2], v[6], color);
        Geometory::AddLine(v[3], v[7], color);
    }

    /**
     * @brief 指定カメラのフラスタムを線で可視化します。
     * @param camera 可視化対象カメラです。
     * @param color 線色です。
     */
    void AddCameraFrustumLines(const Camera& camera, const DirectX::XMFLOAT4& color)
    {
        using namespace DirectX;

        const float nearZ = camera.GetNear();
        const float farZ = camera.GetFar();
        // near/far が不正だとフラスタムが組めないため描画しません。
        if (nearZ <= 0.0f || farZ <= 0.0f || farZ <= nearZ)
        {
            return;
        }

        const float fovy = camera.GetFovy();
        const float aspect = camera.GetAspect();

        XMFLOAT3 pos = camera.GetPos();
        XMFLOAT3 look = camera.GetLook();
        XMFLOAT3 up = camera.GetUp();

        XMVECTOR vPos = XMLoadFloat3(&pos);
        XMVECTOR vLook = XMLoadFloat3(&look);
        XMVECTOR vUp = XMLoadFloat3(&up);

        XMVECTOR forward = vLook - vPos;
        if (XMVectorGetX(XMVector3LengthSq(forward)) < 1.0e-6f)
        {
            // Eye と Look が一致している場合は前方既定値を使います。
            forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        else
        {
            forward = XMVector3Normalize(forward);
        }

        if (XMVectorGetX(XMVector3LengthSq(vUp)) < 1.0e-6f)
        {
            // Up が壊れている場合も描画を継続できるよう既定値に戻します。
            vUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        }
        else
        {
            vUp = XMVector3Normalize(vUp);
        }

        XMVECTOR right = XMVector3Cross(vUp, forward);
        if (XMVectorGetX(XMVector3LengthSq(right)) < 1.0e-6f)
        {
            right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
        }
        else
        {
            right = XMVector3Normalize(right);
        }

        vUp = XMVector3Normalize(XMVector3Cross(forward, right));

        const float tanHalfFovy = tanf(fovy * 0.5f);
        const float nearH = tanHalfFovy * nearZ;
        const float nearW = nearH * aspect;
        const float farH = tanHalfFovy * farZ;
        const float farW = farH * aspect;

        XMVECTOR nearCenter = vPos + forward * nearZ;
        XMVECTOR farCenter = vPos + forward * farZ;

        XMVECTOR upNear = vUp * nearH;
        XMVECTOR rightNear = right * nearW;
        XMVECTOR upFar = vUp * farH;
        XMVECTOR rightFar = right * farW;

        XMVECTOR ntl = nearCenter + upNear - rightNear;
        XMVECTOR ntr = nearCenter + upNear + rightNear;
        XMVECTOR nbl = nearCenter - upNear - rightNear;
        XMVECTOR nbr = nearCenter - upNear + rightNear;

        XMVECTOR ftl = farCenter + upFar - rightFar;
        XMVECTOR ftr = farCenter + upFar + rightFar;
        XMVECTOR fbl = farCenter - upFar - rightFar;
        XMVECTOR fbr = farCenter - upFar + rightFar;

        XMFLOAT3 v[8];
        XMStoreFloat3(&v[0], ntl);
        XMStoreFloat3(&v[1], ntr);
        XMStoreFloat3(&v[2], nbr);
        XMStoreFloat3(&v[3], nbl);
        XMStoreFloat3(&v[4], ftl);
        XMStoreFloat3(&v[5], ftr);
        XMStoreFloat3(&v[6], fbr);
        XMStoreFloat3(&v[7], fbl);

        Geometory::AddLine(v[0], v[1], color);
        Geometory::AddLine(v[1], v[2], color);
        Geometory::AddLine(v[2], v[3], color);
        Geometory::AddLine(v[3], v[0], color);

        Geometory::AddLine(v[4], v[5], color);
        Geometory::AddLine(v[5], v[6], color);
        Geometory::AddLine(v[6], v[7], color);
        Geometory::AddLine(v[7], v[4], color);

        Geometory::AddLine(v[0], v[4], color);
        Geometory::AddLine(v[1], v[5], color);
        Geometory::AddLine(v[2], v[6], color);
        Geometory::AddLine(v[3], v[7], color);
    }

    /**
     * @brief 足元に影スプライトを描画します。
     * @param texture 影テクスチャです。
     * @param pos 描画対象位置です。
     * @param size 対象サイズです。
     * @param scaleMultiplier 影サイズ倍率です。
     */
    void DrawShadow(Texture* texture,
                    const DirectX::XMFLOAT3& pos,
                    const DirectX::XMFLOAT3& size,
                    float scaleMultiplier = 1.0f)
    {
        // テクスチャ未設定時は描画できません。
        if (!texture) return;

        const float kShadowScale = 1.25f;
        const float kShadowY = 0.001f;

        DirectX::XMMATRIX R = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2);
        DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(pos.x, kShadowY, pos.z);
        DirectX::XMFLOAT4X4 world;
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(R * T));

        Sprite::SetWorld(world);
        // 極端に小さい倍率でも影が消え切らないよう下限を設けます。
        const float finalScale = kShadowScale * ((scaleMultiplier > 0.01f) ? scaleMultiplier : 0.01f);
        Sprite::SetSize({ size.x * finalScale, size.z * finalScale });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos({ 0.0f, 0.0f });
        Sprite::SetUVScale({ 1.0f, 1.0f });
        Sprite::SetColor({ 0.0f, 0.0f, 0.0f, 0.6f });
        Sprite::SetTexture(texture);
        Sprite::Draw();
    }

    /**
     * @brief 指定色付きの攻撃マーカーを地面へ描画します。
     * @param texture マーカー画像です。
     * @param pos 描画位置です。
     * @param size 描画サイズです。
     * @param color 表示色です。
     */
    void DrawAttackMarkerTint(Texture* texture,
                              const DirectX::XMFLOAT3& pos,
                              const DirectX::XMFLOAT3& size,
                              const DirectX::XMFLOAT4& color)
    {
        // テクスチャ未設定時は何も描画しません。
        if (!texture) return;

        const float kMarkerY = 0.002f;
        const float kMarkerScale = 1.0f;

        DirectX::XMMATRIX R = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2);
        DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(pos.x, kMarkerY, pos.z);
        DirectX::XMFLOAT4X4 world;
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(R * T));

        Sprite::SetWorld(world);
        Sprite::SetSize({ size.x * kMarkerScale, size.z * kMarkerScale });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos({ 0.0f, 0.0f });
        Sprite::SetUVScale({ 1.0f, 1.0f });
        Sprite::SetColor(color);
        Sprite::SetTexture(texture);
        Sprite::Draw();
    }

    /**
     * @brief 指定色付きスプライトをビルボードで描画します。
     * @param texture マーカー画像です。
     * @param camera 向きを合わせる対象カメラです。
     * @param pos 描画位置の基準です。
     * @param size 描画サイズです。
     * @param color 表示色です。
     */
    void DrawBillboardMarkerTint(Texture* texture,
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
            pos.y + size.y * 0.5f,
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

    void DrawBillboardMarkerTintFrame(Texture* texture,
                                      Camera* camera,
                                      const DirectX::XMFLOAT3& pos,
                                      const DirectX::XMFLOAT3& size,
                                      const DirectX::XMFLOAT4& color,
                                      const DirectX::XMFLOAT2& uvPos,
                                      const DirectX::XMFLOAT2& uvScale)
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
            pos.y + size.y * 0.5f,
            pos.z);
        XMFLOAT4X4 world{};
        XMStoreFloat4x4(&world, XMMatrixTranspose(t));

        Sprite::SetWorld(world);
        Sprite::SetSize({ size.x, size.y });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos(uvPos);
        Sprite::SetUVScale(uvScale);
        Sprite::SetColor(color);
        Sprite::SetTexture(texture);
        Sprite::Draw();
    }

    /**
     * @brief 既定色の攻撃マーカーを描画します。
     * @param texture マーカー画像です。
     * @param pos 描画位置です。
     * @param size 描画サイズです。
     */
    void DrawAttackMarker(Texture* texture, const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size)
    {
        DrawAttackMarkerTint(texture, pos, size, { 1.0f, 1.0f, 1.0f, 0.9f });
    }
}

/**
 * @brief ゲームシーンを生成し、戦闘に必要な各種オブジェクトを初期化します。
 */
SceneGame::SceneGame()
    : m_pCamera(nullptr)
    , m_pCameraGame(nullptr)
    , m_pCameraDebug(nullptr)
    , m_cameraMode(0)
    , m_pPlayer(nullptr)
    , m_enemies()
    , m_markerEffects()
    , m_hitEffectEmitters()
    , m_challengeHazards()
    , m_enemyProjectiles()
    , m_skillProjectiles()
      , m_bloodOrbs()
      , m_chainBeamEffects()
      , m_boss()
      , m_finalBossScript(BossAttackScript::MakeDefaultProfile(BossAttackScript::ProfileHeavyMelee))
      , m_finalBossScriptCursor(0)
      , m_pShadow(nullptr)
    , m_pAttackMarker(nullptr)
    , m_pDirectionMarkerTexture(nullptr)
    , m_pCombatEffectTexture(nullptr)
    , m_pSkillTexture(nullptr)
    , m_pBloodUiTexture(nullptr)
    , m_pBloodStockOnTexture(nullptr)
    , m_pBloodStockOffTexture(nullptr)
    , m_pChainUiTexture(nullptr)
    , m_pChainLinkTexture(nullptr)
    , m_pBossAttackRangeMarker(nullptr)
    , m_pGroundTexture(nullptr)
    , m_pAttackSe(nullptr)
    , m_pPlayerHitSe(nullptr)
    , m_pEnemyAttackSe(nullptr)
    , m_pClearSe(nullptr)
    , m_pDropSe(nullptr)
    , m_pGameBgm(nullptr)
    , m_pBossBgm(nullptr)
    , m_pGameBgmVoice(nullptr)
    , m_isBossBgmActive(false)
    , m_pEnemyHpFrame(nullptr)
    , m_pEnemyHpGauge(nullptr)
    , m_pHpFrame(nullptr)
    , m_pHpGauge(nullptr)
    , m_pCooldownFrame{ nullptr, nullptr, nullptr, nullptr }
    , m_pCooldownGauge{ nullptr, nullptr, nullptr, nullptr }
    , m_stageSize(5.0f)
    , m_requestedEnemyCount(0)
    , m_currentWave(1)
    , m_waveMax(1)
    , m_clearPortalMode(ClearPortalNone)
    , m_clearPortalSpawnTimer(0.0f)
    , m_clearPortalPulseTimer(0.0f)
    , m_clearPortalPos(0.0f, 0.0f, 0.0f)
    , m_cameraIntroActive(true)
    , m_cameraIntroTimer(0.0f)
    , m_cameraIntroStartEye(0.0f, 0.0f, 0.0f)
    , m_cameraIntroStartLook(0.0f, 0.0f, 0.0f)
    , m_cameraIntroFocusEye(0.0f, 0.0f, 0.0f)
    , m_cameraIntroFocusLook(0.0f, 0.0f, 0.0f)
    , m_attackActive(false)
    , m_attackTimer(0.0f)
    , m_attackWindupTimer(0.0f)
    , m_attackRecoveryTimer(0.0f)
    , m_attackCooldownTimer(0.0f)
    , m_attackCooldownUiTimer(0.0f)
    , m_attackCooldownUiDuration(0.0f)
    , m_skill1CooldownTimer(0.0f)
    , m_skill2CooldownTimer(0.0f)
    , m_skill1CooldownDuration(0.0f)
    , m_skill2CooldownDuration(0.0f)
    , m_attackSwingId(0)
    , m_nextHitEffectEmitter(0)
    , m_skillProjectileSerial(0)
    , m_weaponAttackCount(0)
    , m_weaponAttackStock(1)
    , m_weaponAttackStockMax(1)
    , m_weaponAttackStockRechargeTimer(0.0f)
    , m_weaponDamageBuffTimer(0.0f)
    , m_weaponDamageBuffScale(0.0f)
    , m_skillStockCount{ 1, 1 }
    , m_skillStockMax{ 1, 1 }
    , m_skillStockRechargeTimer{ 0.0f, 0.0f }
    , m_bloodStock(0)
    , m_bloodStockMax(5)
    , m_attackHitCountThisSwing(0)
    , m_hitStopTimer(0.0f)
      , m_attackTrailSpawnTimer(0.0f)
      , m_playerDamageFlashTimer(0.0f)
      , m_playerDamageInvincibleTimer(0.0f)
      , m_bossCurseTimer(0.0f)
      , m_bossStompImpactTimer(0.0f)
      , m_screenShakeTimer(0.0f)
    , m_screenShakeDuration(0.0f)
    , m_screenShakeAmplitude(0.0f)
    , m_screenShakePhase(0.0f)
    , m_enemyAttackSeGateTimer(0.0f)
    , m_enemyPerfPhase(0)
    , m_bossSkillContactCooldownTimer(0.0f)
    , m_lastMoveDir(0.0f, 0.0f, 1.0f)
    , m_attackCenter(0.0f, 0.0f, 0.0f)
    , m_attackSize(0.0f, 0.0f, 0.0f)
    , m_orbitSkill()
    , m_attackDamageThisSwing(0)
    , m_attackKnockbackScaleThisSwing(1.0f)
    , m_attackCriticalThisSwing(false)
    , m_bossChainCount(0)
    , m_bossBurnPool(0.0f)
    , m_bossBurnDps(0.0f)
    , m_bossBurnCarry(0.0f)
    , m_hitEffectPreset()
    , m_challengeType(ChallengeNone)
    , m_challengeActive(false)
    , m_challengeTimer(0.0f)
    , m_challengeDuration(0.0f)
    , m_challengeNoDamageSpawnTimer(0.0f)
    , m_challengeDefenseSpawnTimer(0.0f)
    , m_challengeBeaconHp(0.0f)
    , m_challengeBeaconMaxHp(0.0f)
    , m_challengeHitCount(0)
    , m_challengeSpawnSerial(0)
    , m_challengeBeaconPos(0.0f, 0.0f, 0.0f)
    , m_challengeBeaconSize(1.0f, 1.0f, 1.0f)
    , m_lastBossSkillProjectileId(-1)
    , m_isPaused(false)
    , m_pauseTabIndex(kPauseTabGame)
    , m_pauseMenuSelection(kPauseMenuContinue)
    , m_isPauseOptionOpen(false)
    , m_pauseOptionSelection(kPauseOptionMaster)
    , m_isBossBattleDebug(false)
{
    // カメラはゲーム用とデバッグ用を分離して生成します。
    m_pCameraGame = new CameraDebug();
    m_pCameraDebug = new CameraDebug();
    TRAN_INS;

    // Transfer 側のモード値を正規化し、どちらのカメラを使うか決めます。
    tran.cameraMode = NormalizeCameraMode(tran.cameraMode);
    m_cameraMode = tran.cameraMode;
    if (m_pCameraGame)
    {
        m_pCameraGame->LockPos(false);
        ApplyCameraPose(m_pCameraGame, tran.cameraGame.eye, tran.cameraGame.look);
    }
    if (m_pCameraDebug)
    {
        m_pCameraDebug->LockPos(false);
        ApplyCameraPose(m_pCameraDebug, tran.cameraDebug.eye, tran.cameraDebug.look);
    }
    m_pCamera = (m_cameraMode == kCameraModeDebug) ? static_cast<Camera*>(m_pCameraDebug)
        : static_cast<Camera*>(m_pCameraGame);

    // 現在アクティブなカメラ状態を共有領域へ反映します。
    tran.camera = (m_cameraMode == kCameraModeDebug) ? tran.cameraDebug : tran.cameraGame;

    // シーン開始時にリザルトやポーズ状態が残らないよう初期化します。
    SceneManager::ChangeResult(SceneManager::ResultType::None);
    tran.gameplayDebug.pauseMenuOpen = 0;
    tran.gameplayDebug.pauseTabIndex = kPauseTabGame;
    tran.gameplayDebug.pauseMenuSelection = kPauseMenuContinue;
    tran.gameplayDebug.pauseMenuRequest = 0;
    tran.gameplayDebug.pauseOptionOpen = 0;
    tran.gameplayDebug.pauseOptionSelection = kPauseOptionMaster;
    tran.gameplayDebug.pauseOptionRequestClose = 0;
    tran.gameplayDebug.titleOptionOpen = 0;
    tran.gameplayDebug.titleOptionSelection = 0;
    tran.gameplayDebug.titleOptionRequestClose = 0;
    tran.gameplayDebug.titleKeyConfigOpen = 0;
    tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
    tran.gameplayDebug.titleDifficultyOpen = 0;
    tran.gameplayDebug.titleDifficultySelection = tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset);
    {
        const int currentStageType = tran.GetCurrentRunStageType();
        m_isBossBattleDebug =
            currentStageType == Transfer::RoguelikeUpgrade::StageBoss ||
            currentStageType == Transfer::RoguelikeUpgrade::StageFinalBoss;
    }
    tran.gameplayDebug.requestBossBattle = 0;
    tran.gameplayDebug.bossBattleActive = m_isBossBattleDebug ? 1 : 0;
    tran.gameplayDebug.showBossResultTimer = 0;
    tran.gameplayDebug.runTimerRunning = 1;
    ResetChallengeState();
    tran.gameplayDebug.challengeSuccess = 0;
    tran.gameplayDebug.challengeRewardCount = 0;

    m_pPlayer = new Player(m_pCamera);
    {
        TRAN_INS;
        if (tran.player.stageSize <= 0.0f)
        {
            tran.player.stageSize = m_stageSize;
        }
    }
    {
        TRAN_INS;
        const float introFocusDistance = ClampRange(
            tran.gameplay.cameraIntroFocusDistance,
            kCameraIntroFocusDistanceMin,
            kCameraIntroFocusDistanceMax);
        tran.gameplay.cameraIntroFocusDistance = introFocusDistance;
        const DirectX::XMFLOAT3 playerFocus = {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.55f,
            tran.player.pos.z
        };
        m_cameraIntroStartEye = tran.camera.eye;
        m_cameraIntroStartLook = tran.camera.look;
        m_cameraIntroFocusLook = playerFocus;

        const DirectX::XMFLOAT3 toPlayer = {
            playerFocus.x - m_cameraIntroStartEye.x,
            playerFocus.y - m_cameraIntroStartEye.y,
            playerFocus.z - m_cameraIntroStartEye.z
        };
        m_cameraIntroFocusEye = {
            m_cameraIntroStartEye.x + toPlayer.x * kCameraIntroEyeMoveRatio,
            m_cameraIntroStartEye.y + toPlayer.y * kCameraIntroEyeMoveRatio + kCameraIntroEyeLift,
            m_cameraIntroStartEye.z + toPlayer.z * kCameraIntroEyeMoveRatio
        };

        const DirectX::XMFLOAT3 fallbackBackDir = Normalize3({
            m_cameraIntroStartEye.x - m_cameraIntroStartLook.x,
            m_cameraIntroStartEye.y - m_cameraIntroStartLook.y,
            m_cameraIntroStartEye.z - m_cameraIntroStartLook.z
        }, { 0.0f, 0.2f, -1.0f });
        const DirectX::XMFLOAT3 focusBackDir = Normalize3({
            m_cameraIntroFocusEye.x - m_cameraIntroFocusLook.x,
            m_cameraIntroFocusEye.y - m_cameraIntroFocusLook.y,
            m_cameraIntroFocusEye.z - m_cameraIntroFocusLook.z
        }, fallbackBackDir);
        m_cameraIntroFocusEye = {
            m_cameraIntroFocusLook.x + focusBackDir.x * introFocusDistance,
            m_cameraIntroFocusLook.y + focusBackDir.y * introFocusDistance + kCameraIntroEyeLift,
            m_cameraIntroFocusLook.z + focusBackDir.z * introFocusDistance
        };
        m_cameraIntroActive = true;
        m_cameraIntroTimer = 0.0f;
    }

    m_enemies.clear();
    const int baseEnemyCount = static_cast<int>(ClampRange(
        static_cast<float>(tran.gameplay.enemyCount),
        static_cast<float>(kEnemyCountMin),
        static_cast<float>(kEnemyCountMax)));
    const int waveMax = static_cast<int>(ClampRange(
        static_cast<float>(tran.gameplay.waveMax),
        static_cast<float>(kWaveCountMin),
        static_cast<float>(kWaveCountMax)));
    const int waveEnemyAddPerWave = static_cast<int>(ClampRange(
        static_cast<float>(tran.gameplay.waveEnemyAddPerWave),
        0.0f,
        static_cast<float>(kEnemyCountMax)));
	const int difficultyPreset = ClampInt(tran.gameplayDebug.difficultyPreset, 0, 2);
    const bool isTagRewardStage =
        tran.GetCurrentRunRewardType() == Transfer::RoguelikeUpgrade::RewardTag;
	const int effectiveBaseEnemyCount = ClampInt(
		baseEnemyCount + CalcDifficultyBaseEnemyBonus(difficultyPreset),
		kEnemyCountMin,
        kEnemyCountMax);
    const int effectiveWaveEnemyAdd = ClampInt(
        waveEnemyAddPerWave + CalcDifficultyWaveAddBonus(difficultyPreset),
        0,
        kEnemyCountMax);

    m_currentWave = 1;
    m_waveMax = waveMax;
	m_requestedEnemyCount = CalcWaveEnemyCount(effectiveBaseEnemyCount, m_currentWave, effectiveWaveEnemyAdd);
	if (m_isBossBattleDebug || isTagRewardStage)
	{
		if (m_isBossBattleDebug)
		{
			m_currentWave = m_waveMax;
		}
		m_requestedEnemyCount = 0;
	}

    tran.gameplay.enemyCount = baseEnemyCount;
    tran.gameplay.waveMax = m_waveMax;
    tran.gameplay.waveEnemyAddPerWave = waveEnemyAddPerWave;
    tran.roguelike.attackPowerLevel = tran.ClampUpgradeLevel(tran.roguelike.attackPowerLevel);
    tran.roguelike.attackSpeedLevel = tran.ClampUpgradeLevel(tran.roguelike.attackSpeedLevel);
    tran.roguelike.evadeCooldownLevel = tran.ClampUpgradeLevel(tran.roguelike.evadeCooldownLevel);
    tran.gameplayDebug.difficultyPreset = difficultyPreset;
    tran.gameplayDebug.effectiveEnemyBaseCount = effectiveBaseEnemyCount;
    tran.gameplayDebug.effectiveEnemyAddPerWave = effectiveWaveEnemyAdd;
    tran.gameplayDebug.stageClearCount = tran.roguelike.stageClearCount;
    tran.gameplayDebug.attackPowerLevel = tran.roguelike.attackPowerLevel;
    tran.gameplayDebug.attackSpeedLevel = tran.roguelike.attackSpeedLevel;
    tran.gameplayDebug.evadeCooldownLevel = tran.roguelike.evadeCooldownLevel;
    tran.gameplayDebug.lastUpgradeType = tran.roguelike.lastUpgradeType;
    tran.gameplayDebug.upgradeSelectionPending = tran.roguelike.selectionPending;
    tran.gameplayDebug.upgradeRerollRemain = tran.roguelike.rerollRemain;
    tran.gameplayDebug.upgradeOffer0 = tran.roguelike.offers[0];
    tran.gameplayDebug.upgradeOffer1 = tran.roguelike.offers[1];
    tran.gameplayDebug.upgradeOffer2 = tran.roguelike.offers[2];
    EnsureEnemyCount(m_requestedEnemyCount, tran.player.stageSize);
    InitializeBossForScene();
    ResetTraitCombatState();

    m_pShadow = new Texture();
    if (FAILED(m_pShadow->Create("Assets/Texture/Shadow.png")))
    {
        MessageBox(NULL, "Texture load failed.\nShadow.png", "Error", MB_OK);
    }

    m_pAttackMarker = new Texture();
    if (FAILED(m_pAttackMarker->Create("Assets/Texture/Star.png")))
    {
        MessageBox(NULL, "Texture load failed.\nStar.png", "Error", MB_OK);
    }
    m_pDirectionMarkerTexture = new Texture();
    if (FAILED(m_pDirectionMarkerTexture->Create(kDirectionMarkerTexturePath)))
    {
        MessageBox(NULL, "Texture load failed.\nChracter/triangle.png", "Error", MB_OK);
    }
    m_pSkillTexture = new Texture();
    if (FAILED(m_pSkillTexture->Create("Assets/Texture/Chracter/skill.png")))
    {
        MessageBox(NULL, "Texture load failed.\nChracter/skill.png", "Error", MB_OK);
    }
    m_pBloodUiTexture = new Texture();
    if (FAILED(m_pBloodUiTexture->Create("Assets/Texture/Skills/blood_ui.png")))
    {
        MessageBox(NULL, "Texture load failed.\nSkills/blood_ui.png", "Error", MB_OK);
    }
    m_pBloodStockOnTexture = new Texture();
    if (FAILED(m_pBloodStockOnTexture->Create("Assets/Texture/Skills/blood_stock_on.png")))
    {
        MessageBox(NULL, "Texture load failed.\nSkills/blood_stock_on.png", "Error", MB_OK);
    }
    m_pBloodStockOffTexture = new Texture();
    if (FAILED(m_pBloodStockOffTexture->Create("Assets/Texture/Skills/blood_stock_off.png")))
    {
        MessageBox(NULL, "Texture load failed.\nSkills/blood_stock_off.png", "Error", MB_OK);
    }
    m_pChainUiTexture = new Texture();
    if (FAILED(m_pChainUiTexture->Create("Assets/Texture/Skills/chain_ui.png")))
    {
        MessageBox(NULL, "Texture load failed.\nSkills/chain_ui.png", "Error", MB_OK);
    }
    m_pChainLinkTexture = new Texture();
    if (FAILED(m_pChainLinkTexture->Create("Assets/Texture/Skills/chain.png")))
    {
        MessageBox(NULL, "Texture load failed.\nSkills/chain.png", "Error", MB_OK);
    }
    m_pBossAttackRangeMarker = new Texture();
    if (FAILED(m_pBossAttackRangeMarker->Create("Assets/Texture/Game/AttackRange.png")))
    {
        MessageBox(NULL, "Texture load failed.\nGame/AttackRange.png", "Error", MB_OK);
    }
    m_pGroundTexture = new Texture();
    if (FAILED(m_pGroundTexture->Create("Assets/Texture/Game/jimen.png")))
    {
        MessageBox(NULL, "Texture load failed.\nGame/jimen.png", "Error", MB_OK);
        delete m_pGroundTexture;
        m_pGroundTexture = nullptr;
    }

    LoadBossResources();
    LoadHitEffectPreset();
    InitializeHitEffectEmitters();

    m_pAttackSe = LoadSound("Assets/Sound/SE/attack.mp3", false);
    m_pPlayerHitSe = LoadSound("Assets/Sound/SE/player_hit.mp3", false);
    m_pEnemyAttackSe = LoadSound("Assets/Sound/SE/enemy_attack.mp3", false);
    m_pClearSe = LoadSound("Assets/Sound/SE/clear.mp3", false);
    m_pDropSe = LoadSound("Assets/Sound/SE/drop.mp3", false);
    m_pGameBgm = LoadSound("Assets/Sound/BGM/GameBGM.mp3", true);
    m_pBossBgm = LoadSound("Assets/Sound/BGM/GameBGM2.mp3", true);
    if (m_pGameBgm)
    {
        m_pGameBgmVoice = PlaySound(m_pGameBgm);
    }

    m_pEnemyHpFrame = new Texture();
    if (FAILED(m_pEnemyHpFrame->Create("Assets/Texture/UIFrame.png")))
    {
        MessageBox(NULL, "Texture load failed.\nUIFrame.png", "Error", MB_OK);
    }

    m_pEnemyHpGauge = new Texture();
    if (FAILED(m_pEnemyHpGauge->Create("Assets/Texture/UIGauge.png")))
    {
        MessageBox(NULL, "Texture load failed.\nUIGauge.png", "Error", MB_OK);
    }

    const float frameX = kUiMargin + kHpFrameWidth * 0.5f;
    const float frameY = kUiMargin + kHpFrameHeight * 0.5f;
    m_pHpFrame = new UIObject("UIFrame.png", frameX, frameY, kHpFrameWidth, kHpFrameHeight);

    const float gaugeWidth = kHpFrameWidth - kHpGaugePadding * 2.0f;
    const float gaugeHeight = kHpFrameHeight - kHpGaugePadding * 2.0f;
    const float gaugeLeft = kUiMargin + kHpGaugePadding;
    const float gaugeX = gaugeLeft + gaugeWidth * 0.5f;
    const float gaugeY = frameY;
    m_pHpGauge = new UIObject("UIGauge.png", gaugeX, gaugeY, gaugeWidth, gaugeHeight);

    m_uiManager.Add(m_pHpGauge, UIObjectManager::Layer::Game);
    m_uiManager.Add(m_pHpFrame, UIObjectManager::Layer::Game);

    const DirectX::XMFLOAT4 cooldownColors[CooldownSlotCount] =
    {
        { 1.0f, 0.35f, 0.35f, 1.0f }, // Attack
        { 0.35f, 0.95f, 0.55f, 1.0f }, // Evade
        { 0.35f, 0.70f, 1.0f, 1.0f }, // Skill1
        { 1.0f, 0.80f, 0.35f, 1.0f }  // Skill2
    };
    for (int i = 0; i < CooldownSlotCount; ++i)
    {
        m_pCooldownFrame[i] = new UIObject("UIFrame.png", 0.0f, 0.0f, kCooldownFrameWidth, kCooldownFrameHeight);
        m_pCooldownGauge[i] = new UIObject("UIGauge.png", 0.0f, 0.0f,
                                           kCooldownFrameWidth - kCooldownGaugePadding * 2.0f,
                                           kCooldownFrameHeight - kCooldownGaugePadding * 2.0f);
        if (m_pCooldownGauge[i])
        {
            m_pCooldownGauge[i]->SetColor(cooldownColors[i]);
        }
        m_uiManager.Add(m_pCooldownGauge[i], UIObjectManager::Layer::Game);
        m_uiManager.Add(m_pCooldownFrame[i], UIObjectManager::Layer::Game);
    }

    UpdateHpGauge();
    UpdateCooldownGauges();
    tran.gameplayDebug.currentWave = m_currentWave;
    tran.gameplayDebug.maxWave = m_waveMax;
    tran.gameplayDebug.enemiesAlive = static_cast<int>(m_enemies.size());
    tran.gameplayDebug.enemiesTarget = m_requestedEnemyCount;
}

/**
 * @brief SceneGame が生成したリソースとオブジェクトを解放します。
 */
SceneGame::~SceneGame()
{
    // BGM 再生中なら先に停止し、ボイスを破棄します。
    if (m_pGameBgmVoice)
    {
        m_pGameBgmVoice->Stop();
        m_pGameBgmVoice->DestroyVoice();
        m_pGameBgmVoice = nullptr;
    }
    m_pClearSe = nullptr;
    m_pDropSe = nullptr;
    m_pEnemyAttackSe = nullptr;
    m_pPlayerHitSe = nullptr;
    m_pAttackSe = nullptr;
    m_pGameBgm = nullptr;
    m_pBossBgm = nullptr;
    m_isBossBgmActive = false;

    m_uiManager.ClearAll();

    if (m_pHpGauge)
    {
        delete m_pHpGauge;
        m_pHpGauge = nullptr;
    }
    if (m_pHpFrame)
    {
        delete m_pHpFrame;
        m_pHpFrame = nullptr;
    }
    for (int i = 0; i < CooldownSlotCount; ++i)
    {
        if (m_pCooldownGauge[i])
        {
            delete m_pCooldownGauge[i];
            m_pCooldownGauge[i] = nullptr;
        }
        if (m_pCooldownFrame[i])
        {
            delete m_pCooldownFrame[i];
            m_pCooldownFrame[i] = nullptr;
        }
    }
    for (auto& slot : m_enemies)
    {
        if (slot.enemy)
        {
            delete slot.enemy;
            slot.enemy = nullptr;
        }
    }
    m_enemies.clear();
    ReleaseHitEffectEmitters();
    if (m_pShadow)
    {
        delete m_pShadow;
        m_pShadow = nullptr;
    }
    if (m_pAttackMarker)
    {
        delete m_pAttackMarker;
        m_pAttackMarker = nullptr;
    }
    if (m_pDirectionMarkerTexture)
    {
        delete m_pDirectionMarkerTexture;
        m_pDirectionMarkerTexture = nullptr;
    }
    if (m_pCombatEffectTexture)
    {
        delete m_pCombatEffectTexture;
        m_pCombatEffectTexture = nullptr;
    }
    if (m_pSkillTexture)
    {
        delete m_pSkillTexture;
        m_pSkillTexture = nullptr;
    }
    if (m_pBloodUiTexture)
    {
        delete m_pBloodUiTexture;
        m_pBloodUiTexture = nullptr;
    }
    if (m_pBloodStockOnTexture)
    {
        delete m_pBloodStockOnTexture;
        m_pBloodStockOnTexture = nullptr;
    }
    if (m_pBloodStockOffTexture)
    {
        delete m_pBloodStockOffTexture;
        m_pBloodStockOffTexture = nullptr;
    }
    if (m_pChainUiTexture)
    {
        delete m_pChainUiTexture;
        m_pChainUiTexture = nullptr;
    }
    if (m_pChainLinkTexture)
    {
        delete m_pChainLinkTexture;
        m_pChainLinkTexture = nullptr;
    }
    if (m_pBossAttackRangeMarker)
    {
        delete m_pBossAttackRangeMarker;
        m_pBossAttackRangeMarker = nullptr;
    }
    if (m_pGroundTexture)
    {
        delete m_pGroundTexture;
        m_pGroundTexture = nullptr;
    }
    ReleaseBossResources();
    if (m_pEnemyHpGauge)
    {
        delete m_pEnemyHpGauge;
        m_pEnemyHpGauge = nullptr;
    }
    if (m_pEnemyHpFrame)
    {
        delete m_pEnemyHpFrame;
        m_pEnemyHpFrame = nullptr;
    }
    if (m_pPlayer)
    {
        delete m_pPlayer;
        m_pPlayer = nullptr;
    }
    if (m_pCameraGame)
    {
        delete m_pCameraGame;
        m_pCameraGame = nullptr;
    }
    if (m_pCameraDebug)
    {
        delete m_pCameraDebug;
        m_pCameraDebug = nullptr;
    }
    m_pCamera = nullptr;
}

void SceneGame::LoadHitEffectPreset()
{
    m_hitEffectPreset = ParticleEffectPreset::MakeDefaultSettings();
    ParticleEffectPreset::Load(m_hitEffectPreset);
    m_hitEffectPreset.loop = false;
    m_hitEffectPreset.worldSpace = true;
    m_hitEffectPreset.billboard = true;
}

void SceneGame::InitializeHitEffectEmitters()
{
    ReleaseHitEffectEmitters();
    m_hitEffectEmitters.reserve(kMaxHitEffectEmitters);
    for (int i = 0; i < kMaxHitEffectEmitters; ++i)
    {
        ParticleEmitter2D* emitter = new ParticleEmitter2D();
        if (!emitter)
        {
            continue;
        }
        emitter->SetSettings(m_hitEffectPreset);
        emitter->Stop(true);
        m_hitEffectEmitters.push_back(emitter);
    }
    m_nextHitEffectEmitter = 0;
}

void SceneGame::ReleaseHitEffectEmitters()
{
    for (ParticleEmitter2D* emitter : m_hitEffectEmitters)
    {
        delete emitter;
    }
    m_hitEffectEmitters.clear();
    m_nextHitEffectEmitter = 0;
}

void SceneGame::UpdateHitEffectEmitters(float dt)
{
    for (ParticleEmitter2D* emitter : m_hitEffectEmitters)
    {
        if (!emitter)
        {
            continue;
        }

        if (!emitter->IsPlaying() && emitter->GetAliveParticleCount() <= 0)
        {
            continue;
        }

        const ParticleEmitter2D::Settings& settings = emitter->GetSettings();
        if (IsPointOutsideCameraClip(m_pCamera, settings.position, kHitEffectCullMargin))
        {
            emitter->Stop(true);
            continue;
        }

        emitter->Update(dt);
        if (!emitter->IsPlaying() && emitter->GetAliveParticleCount() <= 0)
        {
            emitter->Stop(true);
        }
    }
}

void SceneGame::DrawHitEffectEmitters() const
{
    for (ParticleEmitter2D* emitter : m_hitEffectEmitters)
    {
        if (!emitter)
        {
            continue;
        }
        if (!emitter->IsPlaying() && emitter->GetAliveParticleCount() <= 0)
        {
            continue;
        }
        emitter->Draw(m_pCamera);
    }
}

void SceneGame::SpawnHitEffect(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& size, bool spawnParticle)
{
    (void)size;

    if (!spawnParticle || m_hitEffectEmitters.empty())
    {
        return;
    }

    if (IsPointOutsideCameraClip(m_pCamera, pos, kHitEffectCullMargin))
    {
        return;
    }

    ParticleEmitter2D* emitter = nullptr;
    for (ParticleEmitter2D* candidate : m_hitEffectEmitters)
    {
        if (!candidate)
        {
            continue;
        }
        if (!candidate->IsPlaying() && candidate->GetAliveParticleCount() <= 0)
        {
            emitter = candidate;
            break;
        }
    }
    if (!emitter)
    {
        emitter = m_hitEffectEmitters[m_nextHitEffectEmitter % static_cast<int>(m_hitEffectEmitters.size())];
        ++m_nextHitEffectEmitter;
    }
    if (!emitter)
    {
        return;
    }

    ParticleEmitter2D::Settings settings = m_hitEffectPreset;
    settings.position = pos;
    settings.loop = false;
    settings.worldSpace = true;
    settings.billboard = true;
    emitter->SetSettings(settings);
    emitter->Play(true);
}

void SceneGame::ResetChallengeState()
{
    m_challengeActive = false;
    m_challengeType = ChallengeNone;
    m_challengeTimer = 0.0f;
    m_challengeDuration = 0.0f;
    m_challengeNoDamageSpawnTimer = 0.0f;
    m_challengeDefenseSpawnTimer = 0.0f;
    m_challengeBeaconHp = 0.0f;
    m_challengeBeaconMaxHp = 0.0f;
    m_challengeHitCount = 0;
    m_challengeSpawnSerial = 0;
    m_challengeBeaconPos = { 0.0f, 0.0f, 0.0f };
    m_challengeBeaconSize = { 1.0f, 1.0f, 1.0f };
    m_challengeHazards.clear();

    auto& tran = Transfer::GetInstance();
    tran.gameplayDebug.challengeActive = 0;
    tran.gameplayDebug.challengeType = 0;
    tran.gameplayDebug.challengeHitCount = 0;
    tran.gameplayDebug.challengeElapsedSec = 0.0f;
    tran.gameplayDebug.challengeDurationSec = 0.0f;
    tran.gameplayDebug.challengeBeaconHp = 0.0f;
    tran.gameplayDebug.challengeBeaconMaxHp = 0.0f;
}

void SceneGame::UpdateChallengeDebugSetup(float stageSize)
{
    auto& tran = Transfer::GetInstance();
    const int requestType = ClampInt(tran.gameplayDebug.requestChallengeType, 0, ChallengeDefense);
    if (requestType != ChallengeNone)
    {
        tran.gameplayDebug.requestChallengeType = 0;
        StartChallenge(requestType, stageSize);
    }

    tran.gameplayDebug.challengeActive = m_challengeActive ? 1 : 0;
    tran.gameplayDebug.challengeType = m_challengeType;
    tran.gameplayDebug.challengeHitCount = m_challengeHitCount;
    tran.gameplayDebug.challengeElapsedSec = m_challengeTimer;
    tran.gameplayDebug.challengeDurationSec = m_challengeDuration;
    tran.gameplayDebug.challengeBeaconHp = m_challengeBeaconHp;
    tran.gameplayDebug.challengeBeaconMaxHp = m_challengeBeaconMaxHp;
    if (m_challengeActive)
    {
        tran.gameplayDebug.challengeRewardCount =
            (m_challengeTimer >= m_challengeDuration)
            ? 3
            : CalcChallengeFailureRewardCount();
    }
}

void SceneGame::StartChallenge(int challengeType, float stageSize)
{
    auto& tran = Transfer::GetInstance();
    ResetChallengeState();

    m_challengeType = ClampInt(challengeType, ChallengeNoDamage, ChallengeDefense);
    m_challengeActive = true;
    m_isBossBattleDebug = false;
    m_requestedEnemyCount = 0;
    m_currentWave = 1;
    m_waveMax = 1;
    m_attackActive = false;
    m_attackTimer = 0.0f;
    m_attackWindupTimer = 0.0f;
    m_attackRecoveryTimer = 0.0f;
    m_attackDamageThisSwing = 0;
    m_attackKnockbackScaleThisSwing = 1.0f;
    m_weaponAttackCount = 0;
    m_weaponAttackStock = 1;
    m_weaponAttackStockMax = 1;
    m_weaponAttackStockRechargeTimer = 0.0f;
    m_weaponDamageBuffTimer = 0.0f;
    m_weaponDamageBuffScale = 0.0f;
    m_enemyProjectiles.clear();
    m_skillProjectiles.clear();
    m_markerEffects.clear();
    m_orbitSkill.active = false;
    m_lastBossSkillProjectileId = -1;
    m_bossSkillContactCooldownTimer = 0.0f;
    m_boss.attackZones.clear();
    m_boss.fallingRocks.clear();
    m_boss.requiresArenaReset = false;
    EnsureEnemyCount(0, stageSize);
    ResetTraitCombatState();

    m_cameraIntroActive = false;
    m_cameraIntroTimer = ClampRange(tran.gameplay.cameraIntroDuration, kCameraIntroDurationMin, kCameraIntroDurationMax);

    tran.gameplayDebug.bossBattleActive = 0;
    tran.gameplayDebug.showBossResultTimer = 0;
    tran.gameplayDebug.challengeSuccess = 0;
    tran.gameplayDebug.challengeRewardCount = 0;
    tran.gameplayDebug.challengeReturnToGameAfterReward = 1;

    if (m_challengeType == ChallengeNoDamage)
    {
        m_challengeDuration = ClampRange(tran.gameplay.challengeNoDamageDuration, 5.0f, 120.0f);
        m_challengeNoDamageSpawnTimer = 0.0f;
    }
    else
    {
        m_challengeDuration = ClampRange(tran.gameplay.challengeDefenseDuration, 5.0f, 180.0f);
        m_challengeDefenseSpawnTimer = 0.0f;
        m_challengeBeaconMaxHp = ClampRange(tran.gameplay.challengeDefenseBeaconMaxHp, 1.0f, 500.0f);
        m_challengeBeaconHp = m_challengeBeaconMaxHp;
        const float beaconRadius = ClampRange(tran.gameplay.challengeDefenseBeaconRadius, 0.30f, 6.0f);
        m_challengeBeaconPos = { 0.0f, 0.0f, 0.0f };
        m_challengeBeaconSize = { beaconRadius * 2.0f, tran.player.size.y * 1.25f, beaconRadius * 2.0f };
    }

    UpdateChallengeDebugSetup(stageSize);
}

int SceneGame::CalcChallengeFailureRewardCount() const
{
    if (m_challengeDuration <= 0.0f)
    {
        return 1;
    }

    const float progress = Clamp01(m_challengeTimer / m_challengeDuration);
    return (progress >= (2.0f / 3.0f)) ? 2 : 1;
}

void SceneGame::FinishChallenge(bool success)
{
    auto& tran = Transfer::GetInstance();
    const int rewardCount = success ? 3 : CalcChallengeFailureRewardCount();

    tran.gameplayDebug.challengeSuccess = success ? 1 : 0;
    tran.gameplayDebug.challengeRewardCount = rewardCount;
    tran.gameplayDebug.challengeReturnToGameAfterReward = 1;
    tran.gameplayDebug.runTimerRunning = 0;
    tran.gameplayDebug.runRecordedSec = tran.gameplayDebug.runElapsedSec;
    tran.gameplayDebug.bossBattleActive = 0;

    EnsureEnemyCount(0, m_stageSize);
    m_enemyProjectiles.clear();
    m_skillProjectiles.clear();
    m_attackDamageThisSwing = 0;
    m_attackKnockbackScaleThisSwing = 1.0f;
    m_weaponAttackCount = 0;
    m_weaponAttackStock = 1;
    m_weaponAttackStockMax = 1;
    m_weaponAttackStockRechargeTimer = 0.0f;
    m_weaponDamageBuffTimer = 0.0f;
    m_weaponDamageBuffScale = 0.0f;
    m_markerEffects.clear();
    m_orbitSkill.active = false;
    ResetChallengeState();
    ResetTraitCombatState();

    tran.BeginChallengeRewardSelection(rewardCount);
    SceneManager::ChangeResult(SceneManager::ResultType::Win);
    SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
}

bool SceneGame::UpdateChallengeNoDamage(float stageSize)
{
    if (!m_challengeActive || m_challengeType != ChallengeNoDamage || !m_pPlayer)
    {
        return false;
    }

    auto& tran = Transfer::GetInstance();
    const float duration = ClampRange(tran.gameplay.challengeNoDamageDuration, 5.0f, 120.0f);
    const float attackInterval = ClampRange(tran.gameplay.challengeNoDamageAttackInterval, 0.20f, 10.0f);
    const float telegraph = ClampRange(tran.gameplay.challengeNoDamageTelegraph, 0.10f, 8.0f);
    const float radius = ClampRange(tran.gameplay.challengeNoDamageRadius, 0.20f, 8.0f);
    const float damage = ClampRange(tran.gameplay.challengeNoDamageDamage, 0.0f, 20.0f);
    const int burstCount = ClampInt(tran.gameplay.challengeNoDamageBurstCount, 1, 8);
    const float safeStage = (stageSize > 1.0f) ? stageSize : 5.0f;
    const float stageHalf = safeStage * 0.5f;
    const float margin = radius + 0.25f;
    const float limit = (stageHalf > margin) ? (stageHalf - margin) : 0.0f;

    m_challengeDuration = duration;
    m_requestedEnemyCount = 0;
    m_challengeTimer += kFixedDt;
    m_challengeNoDamageSpawnTimer -= kFixedDt;

    auto clampHazardPos = [&](DirectX::XMFLOAT3 pos)
    {
        pos.x = ClampRange(pos.x, -limit, limit);
        pos.z = ClampRange(pos.z, -limit, limit);
        pos.y = 0.0f;
        return pos;
    };

    if (m_challengeNoDamageSpawnTimer <= 0.0f)
    {
        for (int i = 0; i < burstCount; ++i)
        {
            DirectX::XMFLOAT3 center = tran.player.pos;
            if (i != 0)
            {
                const float rx = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
                const float rz = (static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
                center.x = rx * limit;
                center.z = rz * limit;
            }
            ChallengeHazard hazard{};
            hazard.center = clampHazardPos(center);
            hazard.radius = radius;
            hazard.telegraphDuration = telegraph;
            m_challengeHazards.push_back(hazard);
        }
        m_challengeNoDamageSpawnTimer += attackInterval;
    }

    const Collision::Box playerBox = MakeAabb(
        {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z
        },
        tran.player.size);
    const bool isPlayerInvulnerable =
        m_pPlayer->IsEvading() ||
        m_playerDamageInvincibleTimer > 0.0f;

    for (ChallengeHazard& hazard : m_challengeHazards)
    {
        hazard.timer += kFixedDt;
        if (!hazard.resolved && hazard.timer >= hazard.telegraphDuration)
        {
            hazard.resolved = true;
            const Collision::Box hazardBox = MakeAabb(
                { hazard.center.x, playerBox.center.y, hazard.center.z },
                { hazard.radius * 2.0f, playerBox.size.y, hazard.radius * 2.0f });
            if (!isPlayerInvulnerable && HitAabb(playerBox, hazardBox))
            {
                ++m_challengeHitCount;
                if (m_pPlayerHitSe) PlaySound(m_pPlayerHitSe);
                if (m_playerDamageInvincibleTimer < tran.gameplay.playerDamageInvincible)
                {
                    m_playerDamageInvincibleTimer = tran.gameplay.playerDamageInvincible;
                }
                SpawnHitEffect(
                    {
                        tran.player.pos.x,
                        tran.player.pos.y + tran.player.size.y * 0.5f,
                        tran.player.pos.z
                    },
                    tran.player.size,
                    false);
                tran.player.hp -= damage;
                if (tran.player.hp < 1.0f)
                {
                    tran.player.hp = 1.0f;
                }
                FinishChallenge(false);
                return true;
            }
        }
    }

    m_challengeHazards.erase(
        std::remove_if(
            m_challengeHazards.begin(),
            m_challengeHazards.end(),
            [](const ChallengeHazard& hazard)
            {
                return hazard.timer >= (hazard.telegraphDuration + 0.20f);
            }),
        m_challengeHazards.end());

    if (m_challengeTimer >= m_challengeDuration)
    {
        FinishChallenge(true);
        return true;
    }

    return false;
}

void SceneGame::SpawnDefenseEnemy(float stageSize)
{
    auto& tran = Transfer::GetInstance();
    Enemy* enemy = new Enemy();
    if (!enemy)
    {
        return;
    }

    const int serial = m_challengeSpawnSerial++;
    Enemy::Type enemyType = Enemy::Type::Speed;
    float hpBonusRate = ClampRange(tran.gameplay.challengeDefenseSpeedHpBonusRate, 0.0f, 5.0f);
    switch ((serial % 3 + 3) % 3)
    {
    case 0:
        enemyType = Enemy::Type::Speed;
        hpBonusRate = ClampRange(tran.gameplay.challengeDefenseSpeedHpBonusRate, 0.0f, 5.0f);
        break;
    case 1:
        enemyType = Enemy::Type::Ranged;
        hpBonusRate = ClampRange(tran.gameplay.challengeDefenseRangedHpBonusRate, 0.0f, 5.0f);
        break;
    default:
        enemyType = Enemy::Type::Tank;
        hpBonusRate = ClampRange(tran.gameplay.challengeDefenseTankHpBonusRate, 0.0f, 5.0f);
        break;
    }

    enemy->SetCamera(m_pCamera);
    enemy->SetSize(tran.player.size);
    enemy->SetStageSize(stageSize);
    enemy->SetType(enemyType);
    enemy->SetForceChase(true);
    enemy->SetMoveSpeed(ClampRange(tran.gameplay.challengeDefenseEnemyMoveSpeed, 0.05f, 4.0f));
    enemy->SetTargetPos(m_challengeBeaconPos);

    const int difficultyPreset = ClampInt(tran.gameplayDebug.difficultyPreset, 0, 2);
    const float baseHpScale =
        tran.GetEnemyHpScaleByDifficulty(difficultyPreset) *
        tran.GetEnemyHpScaleByUpgradeProgress();
    enemy->SetHpScale(baseHpScale * (1.0f + hpBonusRate));

    const float safeStage = (stageSize > 1.0f) ? stageSize : 5.0f;
    const float stageHalf = safeStage * 0.5f;
    const float halfX = tran.player.size.x * 0.5f;
    const float halfZ = tran.player.size.z * 0.5f;
    const float edgeX = MaxFloat(0.0f, stageHalf - halfX);
    const float edgeZ = MaxFloat(0.0f, stageHalf - halfZ);
    const float laneRange = safeStage * 0.35f;
    const float laneRand = ((static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f) * laneRange;

    DirectX::XMFLOAT3 spawnPos = { edgeX, 0.0f, laneRand };
    switch ((serial % 4 + 4) % 4)
    {
    case 0: spawnPos = { -edgeX, 0.0f, laneRand }; break;
    case 1: spawnPos = { edgeX, 0.0f, laneRand }; break;
    case 2: spawnPos = { laneRand, 0.0f, -edgeZ }; break;
    default: spawnPos = { laneRand, 0.0f, edgeZ }; break;
    }
    enemy->SetPos(spawnPos);

    EnemySlot slot{};
    slot.enemy = enemy;
    m_enemies.push_back(slot);
}

bool SceneGame::UpdateChallengeDefense(float stageSize)
{
    if (!m_challengeActive || m_challengeType != ChallengeDefense)
    {
        return false;
    }

    auto& tran = Transfer::GetInstance();
    m_challengeDuration = ClampRange(tran.gameplay.challengeDefenseDuration, 5.0f, 180.0f);
    const float beaconMaxHp = ClampRange(tran.gameplay.challengeDefenseBeaconMaxHp, 1.0f, 500.0f);
    const float beaconRadius = ClampRange(tran.gameplay.challengeDefenseBeaconRadius, 0.30f, 6.0f);
    const float spawnInterval = ClampRange(tran.gameplay.challengeDefenseSpawnInterval, 0.20f, 15.0f);
    const int enemyCap = ClampInt(tran.gameplay.challengeDefenseEnemyCap, 1, 16);

    m_requestedEnemyCount = enemyCap;
    if (m_challengeBeaconMaxHp <= 0.0f)
    {
        m_challengeBeaconMaxHp = beaconMaxHp;
        m_challengeBeaconHp = beaconMaxHp;
    }
    else if (std::fabs(m_challengeBeaconMaxHp - beaconMaxHp) > 0.001f)
    {
        const float hpRate = Clamp01(m_challengeBeaconHp / m_challengeBeaconMaxHp);
        m_challengeBeaconMaxHp = beaconMaxHp;
        m_challengeBeaconHp = m_challengeBeaconMaxHp * hpRate;
    }

    m_challengeBeaconPos = { 0.0f, 0.0f, 0.0f };
    m_challengeBeaconSize = { beaconRadius * 2.0f, tran.player.size.y * 1.25f, beaconRadius * 2.0f };
    m_challengeTimer += kFixedDt;
    m_challengeDefenseSpawnTimer -= kFixedDt;

    if (m_challengeDefenseSpawnTimer <= 0.0f &&
        static_cast<int>(m_enemies.size()) < enemyCap)
    {
        SpawnDefenseEnemy(stageSize);
        m_challengeDefenseSpawnTimer += spawnInterval;
    }

    if (m_challengeBeaconHp <= 0.0f)
    {
        m_challengeBeaconHp = 0.0f;
        FinishChallenge(false);
        return true;
    }

    if (m_challengeTimer >= m_challengeDuration)
    {
        FinishChallenge(true);
        return true;
    }

    return false;
}

void SceneGame::DrawChallengeMarkers() const
{
    if (!m_challengeActive)
    {
        return;
    }

    if (m_challengeType == ChallengeNoDamage)
    {
        for (const ChallengeHazard& hazard : m_challengeHazards)
        {
            const DirectX::XMFLOAT3 size = { hazard.radius * 2.0f, 0.05f, hazard.radius * 2.0f };
            DrawAttackMarkerTint(
                m_pBossAttackRangeMarker ? m_pBossAttackRangeMarker : m_pAttackMarker,
                hazard.center,
                size,
                hazard.resolved ? kChallengeHazardResolvedColor : kChallengeHazardColor);
        }
        return;
    }

    if (m_challengeType == ChallengeDefense)
    {
        DrawAttackMarkerTint(
            m_pBossAttackRangeMarker ? m_pBossAttackRangeMarker : m_pAttackMarker,
            m_challengeBeaconPos,
            m_challengeBeaconSize,
            kChallengeBeaconColor);
        DrawBillboardMarkerTint(
            m_pAttackMarker,
            m_pCamera,
            {
                m_challengeBeaconPos.x,
                m_challengeBeaconPos.y + m_challengeBeaconSize.y * 0.7f,
                m_challengeBeaconPos.z
            },
            {
                MaxFloat(0.8f, m_challengeBeaconSize.x * 0.65f),
                MaxFloat(0.8f, m_challengeBeaconSize.y * 0.65f),
                1.0f
            },
            kChallengeBeaconCoreColor);
    }
}

/**
 * @brief 指定スロット向けに敵を 1 体生成して初期化します。
 * @param index 生成対象インデックスです。
 * @param stageSize スポーン計算に使うステージサイズです。
 */
void SceneGame::SpawnEnemyByIndex(int index, float stageSize)
{
    Enemy* enemy = new Enemy();
    // 生成失敗時は初期化を継続できないため中断します。
    if (!enemy) return;

    TRAN_INS;
    // 生成直後に共通設定をまとめて反映します。
    enemy->SetCamera(m_pCamera);
    enemy->SetSize(tran.player.size);
    enemy->SetStageSize(stageSize);
    // 生成順で敵タイプをローテーションし、編成を分散させます。
    switch ((index % 3 + 3) % 3)
    {
    case 0: enemy->SetType(Enemy::Type::Speed); break;
    case 1: enemy->SetType(Enemy::Type::Tank); break;
    default: enemy->SetType(Enemy::Type::Ranged); break;
    }
    const int difficultyPreset = ClampInt(tran.gameplayDebug.difficultyPreset, 0, 2);
    const float enemyHpScale = m_isBossBattleDebug
        ? 1.0f
        : (tran.GetEnemyHpScaleByDifficulty(difficultyPreset) * tran.GetEnemyHpScaleByUpgradeProgress());
    enemy->SetHpScale(enemyHpScale);

    // スポーン候補探索で使う値は先に丸めて、異常値でも破綻しないようにします。
    const float safeStage = (stageSize > 0.5f) ? stageSize : 5.0f;
    const float ringScale = ClampRange(tran.gameplay.enemySpawnRingScale, 0.1f, 0.9f);
    const float jitterScale = ClampRange(tran.gameplay.enemySpawnJitterScale, 0.0f, 0.5f);
    const float minPlayerDist = (tran.gameplay.enemySpawnMinPlayerDist < 0.0f) ? 0.0f : tran.gameplay.enemySpawnMinPlayerDist;
    const float minEnemyDist = (tran.gameplay.enemySpawnMinEnemyDist < 0.0f) ? 0.0f : tran.gameplay.enemySpawnMinEnemyDist;
    const float minPlayerDistSq = minPlayerDist * minPlayerDist;
    const float minEnemyDistSq = minEnemyDist * minEnemyDist;
    const float stageHalf = safeStage * 0.5f;

    auto clampToStage = [&](DirectX::XMFLOAT3 pos)
    {
        const float halfX = tran.player.size.x * 0.5f;
        const float halfZ = tran.player.size.z * 0.5f;
        float minX = -stageHalf + halfX;
        float maxX = stageHalf - halfX;
        float minZ = -stageHalf + halfZ;
        float maxZ = stageHalf - halfZ;
        if (minX > maxX) { minX = 0.0f; maxX = 0.0f; }
        if (minZ > maxZ) { minZ = 0.0f; maxZ = 0.0f; }
        pos.x = ClampRange(pos.x, minX, maxX);
        pos.z = ClampRange(pos.z, minZ, maxZ);
        pos.y = 0.0f;
        return pos;
    };

    const DirectX::XMFLOAT3 playerPos = m_pPlayer ? m_pPlayer->GetPos() : DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 bestPos = clampToStage(CalcEnemySpawnPos(index, safeStage));
    float bestScore = -1.0f;
    const float baseRadius = safeStage * ringScale;
    const float jitterRadius = safeStage * jitterScale;
    const int kSpawnAttempts = 24;

    // 複数候補から、プレイヤーと既存敵に対して最も余裕がある位置を選びます。
    for (int attempt = 0; attempt < kSpawnAttempts; ++attempt)
    {
        DirectX::XMFLOAT3 candidate{};
        if (attempt == 0)
        {
            candidate = CalcEnemySpawnPos(index, safeStage);
        }
        else
        {
            const float angle = (2.0f * kPi * (static_cast<float>(index) + static_cast<float>(attempt) * 0.6180339f))
                / static_cast<float>((kEnemyCountMax > 0) ? kEnemyCountMax : 1);
            const float jitterStep = static_cast<float>((attempt % 5) - 2) * 0.5f;
            float radius = baseRadius + jitterRadius * jitterStep;
            radius = ClampRange(radius, safeStage * 0.10f, safeStage * 0.48f);
            candidate = { std::cos(angle) * radius, 0.0f, std::sin(angle) * radius };
        }
        candidate = clampToStage(candidate);

        float nearestDistSq = DistSqXZ(candidate, playerPos);
        bool farFromPlayer = nearestDistSq >= minPlayerDistSq;
        bool farFromEnemies = true;
        for (const auto& slot : m_enemies)
        {
            if (!slot.enemy) continue;
            const float d = DistSqXZ(candidate, slot.enemy->GetPos());
            if (d < nearestDistSq) nearestDistSq = d;
            if (d < minEnemyDistSq)
            {
                farFromEnemies = false;
                break;
            }
        }

        if (farFromPlayer && farFromEnemies)
        {
            bestPos = candidate;
            break;
        }
        if (nearestDistSq > bestScore)
        {
            bestScore = nearestDistSq;
            bestPos = candidate;
        }
    }

    enemy->SetTargetPos(playerPos);
    enemy->SetPos(bestPos);

    EnemySlot slot{};
    slot.enemy = enemy;
    m_enemies.push_back(slot);
}

/**
 * @brief 目標数へ合わせて敵スロットを増減させます。
 * @param targetCount 目標敵数です。
 * @param stageSize スポーン計算に使うステージサイズです。
 */
void SceneGame::EnsureEnemyCount(int targetCount, float stageSize)
{
    const int clampedTarget = static_cast<int>(ClampRange(static_cast<float>(targetCount), static_cast<float>(kEnemyCountMin), static_cast<float>(kEnemyCountMax)));

    // 足りない間は必要数に達するまで生成を繰り返します。
    while (static_cast<int>(m_enemies.size()) < clampedTarget)
    {
        SpawnEnemyByIndex(static_cast<int>(m_enemies.size()), stageSize);
    }

    // 多い間は末尾から破棄して要求数へ揃えます。
    while (static_cast<int>(m_enemies.size()) > clampedTarget)
    {
        EnemySlot& back = m_enemies.back();
        if (back.enemy)
        {
            delete back.enemy;
            back.enemy = nullptr;
        }
        m_enemies.pop_back();
    }
}

int SceneGame::GetSkillTypeForSlot(int slotIndex) const
{
    const auto& tran = Transfer::GetInstance();
    switch (slotIndex)
    {
    case 0:
        return NormalizeActionSkillType(tran.roguelike.loadoutSkills[0]);
    case 1:
        return NormalizeActionSkillType(tran.roguelike.loadoutSkills[1]);
    default:
        return Transfer::RoguelikeUpgrade::ActionSkillNone;
    }
}

bool SceneGame::TryGetAmbushTarget(DirectX::XMFLOAT3& targetCenter, DirectX::XMFLOAT3& targetSize) const
{
    if (m_isBossBattleDebug && m_boss.hp > 0)
    {
        targetCenter = {
            m_boss.pos.x,
            m_boss.pos.y + m_boss.size.y * 0.5f,
            m_boss.pos.z
        };
        targetSize = m_boss.size;
        return true;
    }

    const EnemySlot* bestSlot = nullptr;
    int bestMaxHp = -1;
    int bestHp = -1;
    for (const auto& slot : m_enemies)
    {
        if (!slot.enemy || !slot.enemy->IsAlive()) continue;
        const int currentMaxHp = slot.enemy->GetMaxHp();
        const int currentHp = slot.enemy->GetHp();
        if (!bestSlot || currentMaxHp > bestMaxHp || (currentMaxHp == bestMaxHp && currentHp > bestHp))
        {
            bestSlot = &slot;
            bestMaxHp = currentMaxHp;
            bestHp = currentHp;
        }
    }

    if (!bestSlot || !bestSlot->enemy)
    {
        return false;
    }

    const Collision::Box targetBox = bestSlot->enemy->GetCollision();
    targetCenter = targetBox.center;
    targetSize = targetBox.size;
    return true;
}

void SceneGame::BeginClearPortal(int mode, const DirectX::XMFLOAT3& preferredPos)
{
    if (mode == ClearPortalNone || m_clearPortalMode != ClearPortalNone)
    {
        return;
    }

    auto& tran = Transfer::GetInstance();
    DirectX::XMFLOAT3 portalPos = { preferredPos.x, 0.0f, preferredPos.z };
    const DirectX::XMFLOAT3 playerPos = { tran.player.pos.x, 0.0f, tran.player.pos.z };
    const float minDistance = 1.8f;
    if (DistSqXZ(portalPos, playerPos) < (minDistance * minDistance))
    {
        const DirectX::XMFLOAT3 fallbackDir = NormalizeXZ(m_lastMoveDir, { 0.0f, 0.0f, 1.0f });
        portalPos.x = playerPos.x + fallbackDir.x * minDistance;
        portalPos.z = playerPos.z + fallbackDir.z * minDistance;
    }

    const float stage = (tran.player.stageSize > 0.1f) ? tran.player.stageSize : m_stageSize;
    const float stageHalf = MaxFloat(stage * 0.5f - 0.8f, 0.5f);
    portalPos.x = ClampRange(portalPos.x, -stageHalf, stageHalf);
    portalPos.z = ClampRange(portalPos.z, -stageHalf, stageHalf);

    m_clearPortalMode = mode;
    m_clearPortalSpawnTimer = 0.5f;
    m_clearPortalPulseTimer = 0.0f;
    m_clearPortalPos = portalPos;
    m_enemyProjectiles.clear();
}

bool SceneGame::UpdateClearPortal(float dt)
{
    if (m_clearPortalMode == ClearPortalNone)
    {
        return false;
    }

    if (m_clearPortalSpawnTimer > 0.0f)
    {
        m_clearPortalSpawnTimer -= dt;
        if (m_clearPortalSpawnTimer < 0.0f) m_clearPortalSpawnTimer = 0.0f;
        return false;
    }

    m_clearPortalPulseTimer += dt;
    if (!m_pPlayer)
    {
        return false;
    }

    auto& tran = Transfer::GetInstance();
    const float playerRadius = MaxFloat(tran.player.size.x, tran.player.size.z) * 0.5f;
    const float portalRadius = 0.70f;
    const float hitRange = playerRadius + portalRadius;
    const float dx = tran.player.pos.x - m_clearPortalPos.x;
    const float dz = tran.player.pos.z - m_clearPortalPos.z;
    if ((dx * dx + dz * dz) > (hitRange * hitRange))
    {
        return false;
    }

    const int portalMode = m_clearPortalMode;
    m_clearPortalMode = ClearPortalNone;
    m_clearPortalSpawnTimer = 0.0f;
    m_clearPortalPulseTimer = 0.0f;

    switch (portalMode)
    {
    case ClearPortalReward:
        tran.BeginCurrentStageRewardSelection();
        if (tran.roguelike.selectionPending != 0 ||
            tran.roguelike.intermissionMode != Transfer::RoguelikeUpgrade::IntermissionNone)
        {
            SceneManager::ChangeResult(SceneManager::ResultType::Win);
            SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
        }
        else
        {
            SceneManager::ChangeResult(SceneManager::ResultType::None);
            SceneManager::ChangeScene(SceneManager::SCENE_GAME);
        }
        return true;

    case ClearPortalNextStage:
        if (tran.BeginNextStageSelection() &&
            (tran.roguelike.selectionPending != 0 ||
             tran.roguelike.intermissionMode != Transfer::RoguelikeUpgrade::IntermissionNone))
        {
            SceneManager::ChangeResult(SceneManager::ResultType::Win);
            SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
        }
        else
        {
            SceneManager::ChangeResult(SceneManager::ResultType::None);
            SceneManager::ChangeScene(SceneManager::SCENE_GAME);
        }
        return true;

    case ClearPortalFinalResult:
        tran.gameplayDebug.showBossResultTimer = 1;
        tran.gameplayDebug.upgradeRerollRemain = 0;
        tran.roguelike.rerollRemain = 0;
        SceneManager::ChangeResult(SceneManager::ResultType::Win);
        SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
        return true;

    default:
        break;
    }

    return false;
}

bool SceneGame::ActivateSkillSlot(int slotIndex, int playerAttackDamage, float stageSize)
{
    if (!m_pPlayer) return false;

    auto& tran = Transfer::GetInstance();
    const int skillType = GetSkillTypeForSlot(slotIndex);
    if (skillType == Transfer::RoguelikeUpgrade::ActionSkillNone)
    {
        return false;
    }

    const DirectX::XMFLOAT3 forward = NormalizeXZ(m_lastMoveDir, { 0.0f, 0.0f, 1.0f });
    const DirectX::XMFLOAT3 playerPos = {
        tran.player.pos.x,
        tran.player.pos.y + tran.player.size.y * 0.5f,
        tran.player.pos.z
    };
    const float stageHalf = stageSize * 0.5f;
    const float attackKnockback = (tran.gameplay.attackKnockback < 0.0f) ? 0.0f : tran.gameplay.attackKnockback;
    const float baseHitStop = (tran.gameplay.attackHitStop < 0.0f) ? 0.0f : tran.gameplay.attackHitStop;

    auto calcDamage = [&](float damageScale) -> int
    {
        return ClampInt(
            static_cast<int>(std::ceil(static_cast<float>(playerAttackDamage) * damageScale)),
            1,
            9999);
    };

    auto clampPlayerPosToStage = [&](DirectX::XMFLOAT3& nextPos)
    {
        const float halfX = tran.player.size.x * 0.5f;
        const float halfZ = tran.player.size.z * 0.5f;
        nextPos.x = ClampRange(nextPos.x, -stageHalf + halfX, stageHalf - halfX);
        nextPos.z = ClampRange(nextPos.z, -stageHalf + halfZ, stageHalf - halfZ);
    };

    auto applyKnockbackToEnemy = [&](EnemySlot& slot, const DirectX::XMFLOAT3& sourcePos, float knockbackScale)
    {
        const Collision::Box enemyBox = slot.enemy->GetCollision();
        DirectX::XMFLOAT3 nextPos = enemyBox.center;
        const DirectX::XMFLOAT3 knockDir = NormalizeXZ({
            enemyBox.center.x - sourcePos.x,
            0.0f,
            enemyBox.center.z - sourcePos.z
        }, forward);
        nextPos.x += knockDir.x * attackKnockback * knockbackScale;
        nextPos.z += knockDir.z * attackKnockback * knockbackScale;
        const float halfX = enemyBox.size.x * 0.5f;
        const float halfZ = enemyBox.size.z * 0.5f;
        nextPos.x = ClampRange(nextPos.x, -stageHalf + halfX, stageHalf - halfX);
        nextPos.z = ClampRange(nextPos.z, -stageHalf + halfZ, stageHalf - halfZ);
        slot.enemy->SetPos({ nextPos.x, 0.0f, nextPos.z });
    };

    const bool skillEnhanceWeapon =
        tran.roguelike.actionSkillEnhancements[skillType][Transfer::RoguelikeUpgrade::SkillEnhanceWeapon] != 0;
    const bool skillEnhanceChain =
        tran.roguelike.actionSkillEnhancements[skillType][Transfer::RoguelikeUpgrade::SkillEnhanceChain] != 0;
    const bool skillEnhanceBlood =
        tran.roguelike.actionSkillEnhancements[skillType][Transfer::RoguelikeUpgrade::SkillEnhanceBlood] != 0;
    const bool skillEnhanceFire =
        tran.roguelike.actionSkillEnhancements[skillType][Transfer::RoguelikeUpgrade::SkillEnhanceFire] != 0;
    int bloodSlashInnateRemain = 3;

    auto applyDirectHitToBoss = [&](
        const Collision::Box& hitBox,
        int damage,
        const std::function<void(const Collision::Box&)>& onHitBoss) -> bool
    {
        if (!m_isBossBattleDebug || m_boss.hp <= 0)
        {
            return false;
        }
        const Collision::Box bossBox = MakeAabb({
            m_boss.pos.x,
            m_boss.pos.y + m_boss.size.y * 0.5f,
            m_boss.pos.z
        }, m_boss.size);
        if (!HitAabb(hitBox, bossBox))
        {
            return false;
        }

        SpawnHitEffect(bossBox.center, bossBox.size);
        const bool defeated = ApplySkillDamageToBoss(damage);
        if (onHitBoss && !defeated)
        {
            onHitBoss(bossBox);
        }
        return defeated;
    };

    auto applyDirectHitToEnemies = [&](
        const Collision::Box& hitBox,
        int damage,
        const DirectX::XMFLOAT3& sourcePos,
        float knockbackScale,
        const std::function<void(EnemySlot&, const Collision::Box&)>& onHitEnemy) -> int
    {
        int hitCount = 0;
        for (auto& slot : m_enemies)
        {
            if (!slot.enemy) continue;
            const Collision::Box enemyBox = slot.enemy->GetCollision();
            if (!HitAabb(hitBox, enemyBox)) continue;
            SpawnHitEffect(enemyBox.center, enemyBox.size);
            slot.enemy->Damage(damage);
            if (onHitEnemy)
            {
                onHitEnemy(slot, enemyBox);
            }
            applyKnockbackToEnemy(slot, sourcePos, knockbackScale);
            ++hitCount;
        }
        return hitCount;
    };

    auto spawnProjectile = [&](const DirectX::XMFLOAT3& origin,
                               const DirectX::XMFLOAT3& direction,
                               float radius,
                               float speed,
                               float maxDistance,
                               int damage,
                               float knockbackScale)
    {
        SkillProjectile shot{};
        shot.pos = origin;
        shot.dir = NormalizeXZ(direction, forward);
        shot.radius = radius;
        shot.speed = speed;
        shot.remainDistance = maxDistance;
        shot.damage = damage;
        shot.projectileId = m_skillProjectileSerial++;
        if (m_skillProjectileSerial < 0) m_skillProjectileSerial = 0;
        shot.knockbackScale = knockbackScale;
        shot.sourceSkillType = skillType;
        shot.sourceDamage = damage;
        shot.sourceWeapon = false;
        shot.critical = false;
        shot.remainingInnateBloodCount =
            (skillType == Transfer::RoguelikeUpgrade::ActionSkillBloodSlash) ? 3 : 0;
        shot.innateBurnDamage =
            (skillType == Transfer::RoguelikeUpgrade::ActionSkillFireball) ? static_cast<float>(damage) : 0.0f;
        m_skillProjectiles.push_back(shot);
    };

    auto findPriorityEnemy = [&]() -> EnemySlot*
    {
        EnemySlot* bestSlot = nullptr;
        int bestMaxHp = -1;
        int bestHp = -1;
        for (auto& slot : m_enemies)
        {
            if (!slot.enemy || !slot.enemy->IsAlive()) continue;
            const int currentMaxHp = slot.enemy->GetMaxHp();
            const int currentHp = slot.enemy->GetHp();
            if (!bestSlot || currentMaxHp > bestMaxHp || (currentMaxHp == bestMaxHp && currentHp > bestHp))
            {
                bestSlot = &slot;
                bestMaxHp = currentMaxHp;
                bestHp = currentHp;
            }
        }
        return bestSlot;
    };

    auto finishImmediateSkill = [&](int hitCount, bool includeBoss)
    {
        if (hitCount > 0 || includeBoss)
        {
            if (m_pAttackSe) PlaySound(m_pAttackSe);
            if (m_hitStopTimer < baseHitStop)
            {
                m_hitStopTimer = baseHitStop;
            }
        }
    };

    auto applySkillTraitExtrasToEnemy = [&](EnemySlot& slot, const Collision::Box& enemyBox, int damage, int innateChainCount, int innateBloodCount, float innateBurnDamage)
    {
        if (skillEnhanceWeapon)
        {
            ApplySkillWeaponBuff(skillType);
        }

        int chainCount = innateChainCount;
        if (skillEnhanceChain)
        {
            chainCount += (skillType == Transfer::RoguelikeUpgrade::ActionSkillChainThrow) ? 3 : 1;
        }
        if (chainCount > 0)
        {
            ApplyChainToEnemy(slot, chainCount, damage, enemyBox.center);
        }

        int bloodCount = innateBloodCount;
        if (skillEnhanceBlood)
        {
            ++bloodCount;
        }
        if (bloodCount > 0)
        {
            SpawnBloodOrb(enemyBox.center, bloodCount);
        }

        float burnDamage = innateBurnDamage;
        if (skillEnhanceFire)
        {
            burnDamage += static_cast<float>(damage);
        }
        if (burnDamage > 0.0f)
        {
            ApplyBurnToEnemy(slot, burnDamage);
        }
    };

    auto applySkillTraitExtrasToBoss = [&](const Collision::Box& bossBox, int damage, int innateChainCount, int innateBloodCount, float innateBurnDamage)
    {
        if (skillEnhanceWeapon)
        {
            ApplySkillWeaponBuff(skillType);
        }

        int chainCount = innateChainCount;
        if (skillEnhanceChain)
        {
            chainCount += (skillType == Transfer::RoguelikeUpgrade::ActionSkillChainThrow) ? 3 : 1;
        }
        if (chainCount > 0)
        {
            ApplyChainToBoss(chainCount, damage, bossBox.center);
        }

        int bloodCount = innateBloodCount;
        if (skillEnhanceBlood)
        {
            ++bloodCount;
        }
        if (bloodCount > 0)
        {
            SpawnBloodOrb(bossBox.center, bloodCount);
        }

        float burnDamage = innateBurnDamage;
        if (skillEnhanceFire)
        {
            burnDamage += static_cast<float>(damage);
        }
        if (burnDamage > 0.0f)
        {
            ApplyBurnToBoss(burnDamage);
        }
    };

    switch (skillType)
    {
    case Transfer::RoguelikeUpgrade::ActionSkillWhirl:
    {
        const int damage = calcDamage(kSkillWhirlDamageScale);
        const DirectX::XMFLOAT3 hitSize = {
            tran.player.size.x * 3.6f,
            tran.player.size.y * 1.35f,
            tran.player.size.z * 3.6f
        };
        const Collision::Box hitBox = MakeAabb(playerPos, hitSize);
        const int hitCount = applyDirectHitToEnemies(
            hitBox,
            damage,
            playerPos,
            1.15f,
            [&](EnemySlot& slot, const Collision::Box& enemyBox)
            {
                applySkillTraitExtrasToEnemy(slot, enemyBox, damage, 0, 0, 0.0f);
            });
        const bool bossHit = applyDirectHitToBoss(
            hitBox,
            damage,
            [&](const Collision::Box& bossBox)
            {
                applySkillTraitExtrasToBoss(bossBox, damage, 0, 0, 0.0f);
            });
        finishImmediateSkill(hitCount, bossHit);
        return true;
    }
    case Transfer::RoguelikeUpgrade::ActionSkillRush:
    {
        const int damage = calcDamage(kSkillRushDamageScale);
        DirectX::XMFLOAT3 destination = {
            tran.player.pos.x + forward.x * kSkillRushDistance,
            tran.player.pos.y,
            tran.player.pos.z + forward.z * kSkillRushDistance
        };
        clampPlayerPosToStage(destination);
        const DirectX::XMFLOAT3 center = {
            (tran.player.pos.x + destination.x) * 0.5f,
            playerPos.y,
            (tran.player.pos.z + destination.z) * 0.5f
        };
        const DirectX::XMFLOAT3 hitSize = {
            tran.player.size.x * 1.8f,
            tran.player.size.y * 1.2f,
            MaxFloat(std::fabs(destination.x - tran.player.pos.x), std::fabs(destination.z - tran.player.pos.z)) + tran.player.size.z * 1.8f
        };
        const Collision::Box hitBox = MakeAabb(center, hitSize);
        const int hitCount = applyDirectHitToEnemies(
            hitBox,
            damage,
            center,
            1.35f,
            [&](EnemySlot& slot, const Collision::Box& enemyBox)
            {
                applySkillTraitExtrasToEnemy(slot, enemyBox, damage, 0, 0, 0.0f);
            });
        const bool bossHit = applyDirectHitToBoss(
            hitBox,
            damage,
            [&](const Collision::Box& bossBox)
            {
                applySkillTraitExtrasToBoss(bossBox, damage, 0, 0, 0.0f);
            });
        tran.player.pos = destination;
        m_pPlayer->SetPos(destination);
        m_lastMoveDir = forward;
        finishImmediateSkill(hitCount, bossHit);
        return true;
    }
    case Transfer::RoguelikeUpgrade::ActionSkillAmbush:
    {
        DirectX::XMFLOAT3 targetCenter = playerPos;
        DirectX::XMFLOAT3 targetSize = tran.player.size;
        TryGetAmbushTarget(targetCenter, targetSize);

        const DirectX::XMFLOAT3 approachDir = NormalizeXZ({
            targetCenter.x - tran.player.pos.x,
            0.0f,
            targetCenter.z - tran.player.pos.z
        }, forward);
        DirectX::XMFLOAT3 destination = {
            targetCenter.x - approachDir.x * (targetSize.x + tran.player.size.x) * 0.65f,
            tran.player.pos.y,
            targetCenter.z - approachDir.z * (targetSize.z + tran.player.size.z) * 0.65f
        };
        clampPlayerPosToStage(destination);
        tran.player.pos = destination;
        m_pPlayer->SetPos(destination);
        m_lastMoveDir = approachDir;

        const int damage = calcDamage(kSkillAmbushDamageScale);
        const DirectX::XMFLOAT3 ambushCenter = {
            destination.x + approachDir.x * tran.player.size.x * 1.3f,
            playerPos.y,
            destination.z + approachDir.z * tran.player.size.z * 1.3f
        };
        const Collision::Box hitBox = MakeAabb(ambushCenter, {
            tran.player.size.x * 2.2f,
            tran.player.size.y * 1.2f,
            tran.player.size.z * 2.2f
        });
        const int hitCount = applyDirectHitToEnemies(
            hitBox,
            damage,
            ambushCenter,
            1.0f,
            [&](EnemySlot& slot, const Collision::Box& enemyBox)
            {
                applySkillTraitExtrasToEnemy(slot, enemyBox, damage, 0, 0, 0.0f);
            });
        const bool bossHit = applyDirectHitToBoss(
            hitBox,
            damage,
            [&](const Collision::Box& bossBox)
            {
                applySkillTraitExtrasToBoss(bossBox, damage, 0, 0, 0.0f);
            });
        finishImmediateSkill(hitCount, bossHit);
        return true;
    }
    case Transfer::RoguelikeUpgrade::ActionSkillChainThrow:
    {
        const int damage = calcDamage(kSkillChainThrowDamageScale);
        const Collision::Box hitBox = MakeAabb(playerPos, {
            kSkillChainThrowRange * 2.0f,
            tran.player.size.y * 1.6f,
            kSkillChainThrowRange * 2.0f
        });
        const int hitCount = applyDirectHitToEnemies(
            hitBox,
            damage,
            playerPos,
            0.55f,
            [&](EnemySlot& slot, const Collision::Box& enemyBox)
            {
                applySkillTraitExtrasToEnemy(slot, enemyBox, damage, 1, 0, 0.0f);
            });
        const bool bossHit = applyDirectHitToBoss(
            hitBox,
            damage,
            [&](const Collision::Box& bossBox)
            {
                applySkillTraitExtrasToBoss(bossBox, damage, 1, 0, 0.0f);
            });
        finishImmediateSkill(hitCount, bossHit);
        return true;
    }
    case Transfer::RoguelikeUpgrade::ActionSkillFireball:
    {
        DirectX::XMFLOAT3 targetPos = {
            playerPos.x + forward.x * 6.0f,
            playerPos.y,
            playerPos.z + forward.z * 6.0f
        };
        if (m_isBossBattleDebug && m_boss.hp > 0)
        {
            targetPos = {
                m_boss.pos.x,
                playerPos.y,
                m_boss.pos.z
            };
        }
        else
        {
            EnemySlot* priorityEnemy = findPriorityEnemy();
            if (priorityEnemy && priorityEnemy->enemy)
            {
                const Collision::Box enemyBox = priorityEnemy->enemy->GetCollision();
                targetPos = enemyBox.center;
            }
        }

        DirectX::XMFLOAT3 shotDir = NormalizeXZ({
            targetPos.x - playerPos.x,
            0.0f,
            targetPos.z - playerPos.z
        }, forward);
        float maxDistance = std::sqrt(DistSqXZ(targetPos, playerPos));
        if (maxDistance < 1.2f) maxDistance = 6.0f;
        spawnProjectile(
            {
                playerPos.x + shotDir.x * tran.player.size.x,
                playerPos.y,
                playerPos.z + shotDir.z * tran.player.size.z
            },
            shotDir,
            kSkillFireballRadius,
            kSkillFireballSpeed,
            maxDistance + 0.8f,
            calcDamage(kSkillFireballDamageScale),
            0.95f);
        return true;
    }
    case Transfer::RoguelikeUpgrade::ActionSkillBloodSlash:
    {
        const DirectX::XMFLOAT3 shotOrigin = {
            playerPos.x + forward.x * tran.player.size.x,
            playerPos.y,
            playerPos.z + forward.z * tran.player.size.z
        };
        spawnProjectile(
            shotOrigin,
            forward,
            kSkillSlashProjectileRadius,
            kSkillSlashProjectileSpeed,
            stageSize,
            calcDamage(kSkillBloodSlashDamageScale),
            1.05f);
        return true;
    }
    default:
        return false;
    }
}

void SceneGame::UpdateSkillActors(float dt, float stageSize)
{
    const float safeDt = (dt > 0.0f) ? dt : 0.0f;
    const float stageHalf = stageSize * 0.5f;

    if (m_bossSkillContactCooldownTimer > 0.0f)
    {
        m_bossSkillContactCooldownTimer -= safeDt;
        if (m_bossSkillContactCooldownTimer < 0.0f) m_bossSkillContactCooldownTimer = 0.0f;
    }

    for (auto& slot : m_enemies)
    {
        if (slot.skillContactCooldownTimer > 0.0f)
        {
            slot.skillContactCooldownTimer -= safeDt;
            if (slot.skillContactCooldownTimer < 0.0f) slot.skillContactCooldownTimer = 0.0f;
        }
    }

    for (auto& shot : m_skillProjectiles)
    {
        if (shot.remainDistance <= 0.0f) continue;
        const float step = shot.speed * safeDt;
        shot.pos.x += shot.dir.x * step;
        shot.pos.z += shot.dir.z * step;
        shot.remainDistance -= step;
        if (shot.remainDistance <= 0.0f ||
            std::fabs(shot.pos.x) > stageHalf + 0.5f ||
            std::fabs(shot.pos.z) > stageHalf + 0.5f)
        {
            shot.remainDistance = 0.0f;
        }
    }

    m_skillProjectiles.erase(
        std::remove_if(
            m_skillProjectiles.begin(),
            m_skillProjectiles.end(),
            [](const SkillProjectile& shot) { return shot.remainDistance <= 0.0f; }),
        m_skillProjectiles.end());

    if (!m_orbitSkill.active)
    {
        return;
    }

    if (m_orbitSkill.timer > 0.0f)
    {
        m_orbitSkill.timer -= safeDt;
        if (m_orbitSkill.timer < 0.0f) m_orbitSkill.timer = 0.0f;
    }
    if (m_orbitSkill.timer <= 0.0f)
    {
        m_orbitSkill.active = false;
        return;
    }

    auto& tran = Transfer::GetInstance();
    m_orbitSkill.angle += m_orbitSkill.angularSpeed * safeDt;
    m_orbitSkill.pos = CalcOrbitSatellitePos(
        m_orbitSkill.angle,
        m_orbitSkill.radius,
        m_orbitSkill.count,
        tran.player.pos,
        tran.player.size.y,
        0);
}

void SceneGame::ResetTraitCombatState()
{
    m_bloodOrbs.clear();
    m_chainBeamEffects.clear();
    m_bloodStock = 0;
    m_bloodStockMax = GetBloodStockCap();
    m_skillStockCount[0] = 0;
    m_skillStockCount[1] = 0;
    m_skillStockMax[0] = 0;
    m_skillStockMax[1] = 0;
    m_skillStockRechargeTimer[0] = 0.0f;
    m_skillStockRechargeTimer[1] = 0.0f;
    RefreshSkillStockState();
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
    {
        m_skillStockCount[slotIndex] = m_skillStockMax[slotIndex];
        m_skillStockRechargeTimer[slotIndex] = 0.0f;
    }
    m_skill1CooldownTimer = 0.0f;
    m_skill2CooldownTimer = 0.0f;
    m_attackCriticalThisSwing = false;
    m_bossChainCount = 0;
    m_bossBurnPool = 0.0f;
    m_bossBurnDps = 0.0f;
    m_bossBurnCarry = 0.0f;
    for (auto& slot : m_enemies)
    {
        slot.chainCount = 0;
        slot.burnPool = 0.0f;
        slot.burnDps = 0.0f;
        slot.burnCarry = 0.0f;
    }
}

void SceneGame::RefreshSkillStockState()
{
    m_bloodStockMax = GetBloodStockCap();
    if (m_bloodStock < 0) m_bloodStock = 0;
    if (m_bloodStock > m_bloodStockMax) m_bloodStock = m_bloodStockMax;

    for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
    {
        const int oldMax = m_skillStockMax[slotIndex];
        const int newMax = GetSkillStockMaxForSlot(slotIndex);
        m_skillStockMax[slotIndex] = newMax;
        if (newMax <= 0)
        {
            m_skillStockCount[slotIndex] = 0;
            m_skillStockRechargeTimer[slotIndex] = 0.0f;
            continue;
        }

        if (oldMax <= 0 || m_skillStockCount[slotIndex] >= oldMax)
        {
            m_skillStockCount[slotIndex] = newMax;
            m_skillStockRechargeTimer[slotIndex] = 0.0f;
        }
        else
        {
            if (m_skillStockCount[slotIndex] < 0) m_skillStockCount[slotIndex] = 0;
            if (m_skillStockCount[slotIndex] > newMax) m_skillStockCount[slotIndex] = newMax;
            if (m_skillStockCount[slotIndex] >= newMax)
            {
                m_skillStockRechargeTimer[slotIndex] = 0.0f;
            }
        }
    }

    m_skill1CooldownTimer = m_skillStockRechargeTimer[0];
    m_skill2CooldownTimer = m_skillStockRechargeTimer[1];
}

void SceneGame::UpdateChainBeamEffects(float dt)
{
    const float safeDt = (dt > 0.0f) ? dt : 0.0f;
    auto it = m_chainBeamEffects.begin();
    while (it != m_chainBeamEffects.end())
    {
        it->timer -= safeDt;
        if (it->timer <= 0.0f)
        {
            it = m_chainBeamEffects.erase(it);
            continue;
        }
        ++it;
    }
}

void SceneGame::SpawnChainBeamEffect(const DirectX::XMFLOAT3& startPos, const DirectX::XMFLOAT3& endPos)
{
    ChainBeamEffect beam{};
    beam.startPos = startPos;
    beam.endPos = endPos;
    beam.duration = kChainBeamDuration;
    beam.timer = beam.duration;
    m_chainBeamEffects.push_back(beam);
    if (m_chainBeamEffects.size() > 24)
    {
        m_chainBeamEffects.erase(m_chainBeamEffects.begin(), m_chainBeamEffects.begin() + (m_chainBeamEffects.size() - 24));
    }
}

int SceneGame::GetSkillStockMaxForSlot(int slotIndex) const
{
    if (slotIndex < 0 || slotIndex >= 2)
    {
        return 0;
    }

    const auto& tran = Transfer::GetInstance();
    const int skillType = GetSkillTypeForSlot(slotIndex);
    if (skillType == Transfer::RoguelikeUpgrade::ActionSkillNone)
    {
        return 0;
    }

    int maxStock = 1;
    if (tran.roguelike.actionSkillEnhancements[skillType][Transfer::RoguelikeUpgrade::SkillEnhanceCooldown] != 0)
    {
        ++maxStock;
    }

    const int cooldownTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitCooldown],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    if (cooldownTraitLevel >= 2) ++maxStock;
    if (cooldownTraitLevel >= 4) ++maxStock;
    if (cooldownTraitLevel >= 6) ++maxStock;
    return maxStock;
}

int SceneGame::GetSkillStockCountForSlot(int slotIndex) const
{
    return (slotIndex >= 0 && slotIndex < 2) ? m_skillStockCount[slotIndex] : 0;
}

float SceneGame::GetSkillRechargeTimerForSlot(int slotIndex) const
{
    return (slotIndex >= 0 && slotIndex < 2) ? m_skillStockRechargeTimer[slotIndex] : 0.0f;
}

bool SceneGame::TryConsumeSkillResource(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 2 || m_skillStockMax[slotIndex] <= 0)
    {
        return false;
    }

    if (m_skillStockCount[slotIndex] > 0)
    {
        --m_skillStockCount[slotIndex];
        BeginSkillCooldown(slotIndex);
        return true;
    }

    const int requiredBloodStock = static_cast<int>(std::ceil(GetSkillRechargeTimerForSlot(slotIndex) - 0.0001f));
    if (requiredBloodStock <= 0)
    {
        BeginSkillCooldown(slotIndex);
        return true;
    }
    if (requiredBloodStock > m_bloodStock)
    {
        return false;
    }

    m_bloodStock -= requiredBloodStock;
    if (m_bloodStock < 0) m_bloodStock = 0;
    m_skillStockRechargeTimer[slotIndex] = 0.0f;
    BeginSkillCooldown(slotIndex);
    return true;
}

void SceneGame::BeginSkillCooldown(int slotIndex)
{
    if (slotIndex < 0 || slotIndex >= 2)
    {
        return;
    }

    const float duration = (slotIndex == 0) ? m_skill1CooldownDuration : m_skill2CooldownDuration;
    if (duration > 0.0f &&
        m_skillStockMax[slotIndex] > 0 &&
        m_skillStockCount[slotIndex] < m_skillStockMax[slotIndex] &&
        m_skillStockRechargeTimer[slotIndex] <= 0.0f)
    {
        m_skillStockRechargeTimer[slotIndex] = duration;
    }

    m_skill1CooldownTimer = m_skillStockRechargeTimer[0];
    m_skill2CooldownTimer = m_skillStockRechargeTimer[1];
}

void SceneGame::SpawnBloodOrb(const DirectX::XMFLOAT3& pos, int count)
{
    const int spawnCount = ClampInt(count, 0, kBloodOrbFieldCap);
    for (int i = 0; i < spawnCount; ++i)
    {
        BloodOrb orb{};
        orb.pos = pos;
        orb.timer = kBloodOrbLifetime;
        if (spawnCount > 1)
        {
            const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>(spawnCount);
            orb.pos.x += std::cos(angle) * 0.25f;
            orb.pos.z += std::sin(angle) * 0.25f;
        }
        if (static_cast<int>(m_bloodOrbs.size()) >= kBloodOrbFieldCap)
        {
            m_bloodOrbs.erase(m_bloodOrbs.begin());
        }
        m_bloodOrbs.push_back(orb);
    }
}

void SceneGame::UpdateBloodOrbs(float dt, int playerAttackDamage)
{
    if (!m_pPlayer)
    {
        return;
    }

    const auto& tran = Transfer::GetInstance();
    const float safeDt = (dt > 0.0f) ? dt : 0.0f;
    const DirectX::XMFLOAT3 playerPos = tran.player.pos;
    const float pickupRadius = GetBloodPickupRadius();
    const float pickupRadiusSq = pickupRadius * pickupRadius;
    const int pickupBurnDamage = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(playerAttackDamage) * 3.0f)),
        1,
        9999);

    auto orbIt = m_bloodOrbs.begin();
    while (orbIt != m_bloodOrbs.end())
    {
        orbIt->timer -= safeDt;
        if (orbIt->timer <= 0.0f)
        {
            orbIt = m_bloodOrbs.erase(orbIt);
            continue;
        }

        if (DistSqXZ(orbIt->pos, playerPos) <= pickupRadiusSq)
        {
            if (m_bloodStock < m_bloodStockMax)
            {
                ++m_bloodStock;
            }
            if (HasBloodFireSynergy())
            {
                for (auto& slot : m_enemies)
                {
                    if (!slot.enemy || !slot.enemy->IsAlive()) continue;
                    if (DistSqXZ(slot.enemy->GetCollision().center, orbIt->pos) <= kBloodFireSynergyRadius * kBloodFireSynergyRadius)
                    {
                        ApplyBurnToEnemy(slot, static_cast<float>(pickupBurnDamage));
                    }
                }
                if (m_isBossBattleDebug && m_boss.hp > 0)
                {
                    const DirectX::XMFLOAT3 bossCenter = {
                        m_boss.pos.x,
                        m_boss.pos.y + m_boss.size.y * 0.5f,
                        m_boss.pos.z
                    };
                    if (DistSqXZ(bossCenter, orbIt->pos) <= kBloodFireSynergyRadius * kBloodFireSynergyRadius)
                    {
                        ApplyBurnToBoss(static_cast<float>(pickupBurnDamage));
                    }
                }
            }
            orbIt = m_bloodOrbs.erase(orbIt);
            continue;
        }

        ++orbIt;
    }
}

void SceneGame::UpdateDamageOverTime(float dt)
{
    const float safeDt = (dt > 0.0f) ? dt : 0.0f;
    for (auto& slot : m_enemies)
    {
        if (!slot.enemy || !slot.enemy->IsAlive()) continue;
        if (slot.burnPool <= 0.0f || slot.burnDps <= 0.0f) continue;

        const float damageThisFrame = std::min(slot.burnPool, slot.burnDps * safeDt);
        slot.burnPool -= damageThisFrame;
        slot.burnCarry += damageThisFrame;
        const int burnDamage = static_cast<int>(std::floor(slot.burnCarry + 0.0001f));
        if (burnDamage > 0)
        {
            slot.burnCarry -= static_cast<float>(burnDamage);
            slot.enemy->Damage(burnDamage);
        }
        if (slot.burnPool <= 0.0f)
        {
            slot.burnPool = 0.0f;
            slot.burnDps = 0.0f;
            slot.burnCarry = 0.0f;
        }
    }

    if (!m_isBossBattleDebug || m_boss.hp <= 0 || m_bossBurnPool <= 0.0f || m_bossBurnDps <= 0.0f)
    {
        return;
    }

    const float damageThisFrame = std::min(m_bossBurnPool, m_bossBurnDps * safeDt);
    m_bossBurnPool -= damageThisFrame;
    m_bossBurnCarry += damageThisFrame;
    const int burnDamage = static_cast<int>(std::floor(m_bossBurnCarry + 0.0001f));
    if (burnDamage > 0)
    {
        m_bossBurnCarry -= static_cast<float>(burnDamage);
        ApplySkillDamageToBoss(burnDamage);
    }
    if (m_bossBurnPool <= 0.0f)
    {
        m_bossBurnPool = 0.0f;
        m_bossBurnDps = 0.0f;
        m_bossBurnCarry = 0.0f;
    }
}

int SceneGame::GetChainCapPerTarget() const
{
    const auto& tran = Transfer::GetInstance();
    const int chainTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitChain],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    int cap = 3;
    if (chainTraitLevel >= 2) cap += 2;
    if (chainTraitLevel >= 4) cap += 2;
    if (chainTraitLevel >= 6) cap += 2;
    return cap;
}

int SceneGame::GetBloodStockCap() const
{
    const auto& tran = Transfer::GetInstance();
    const int bloodTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitBlood],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    return 5 + bloodTraitLevel * 2;
}

float SceneGame::GetBloodPickupRadius() const
{
    const auto& tran = Transfer::GetInstance();
    const int bloodTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitBlood],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    float radius = 3.0f;
    if (bloodTraitLevel >= 2) radius = 4.0f;
    if (bloodTraitLevel >= 4) radius = 5.0f;
    if (bloodTraitLevel >= 6) radius = 6.0f;
    return radius;
}

float SceneGame::GetBurnScale() const
{
    const auto& tran = Transfer::GetInstance();
    const int fireTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitFire],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    float scale = 1.0f + 0.10f * static_cast<float>(fireTraitLevel);
    if (fireTraitLevel >= 2) scale += 1.0f;
    if (fireTraitLevel >= 4) scale += 1.0f;
    return scale;
}

bool SceneGame::HasChainBloodSynergy() const
{
    const auto& tran = Transfer::GetInstance();
    return
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitChain] >= Transfer::RoguelikeUpgrade::kTraitLevelMax &&
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitBlood] >= Transfer::RoguelikeUpgrade::kTraitLevelMax;
}

bool SceneGame::HasBloodFireSynergy() const
{
    const auto& tran = Transfer::GetInstance();
    return
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitBlood] >= Transfer::RoguelikeUpgrade::kTraitLevelMax &&
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitFire] >= Transfer::RoguelikeUpgrade::kTraitLevelMax;
}

bool SceneGame::HasChainFireSynergy() const
{
    const auto& tran = Transfer::GetInstance();
    return
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitChain] >= Transfer::RoguelikeUpgrade::kTraitLevelMax &&
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitFire] >= Transfer::RoguelikeUpgrade::kTraitLevelMax;
}

int SceneGame::ApplyChainToEnemy(EnemySlot& slot, int requestedCount, int sourceDamage, const DirectX::XMFLOAT3& effectPos)
{
    if (!slot.enemy || requestedCount <= 0)
    {
        return 0;
    }

    const int chainCap = GetChainCapPerTarget();
    const int remain = chainCap - slot.chainCount;
    if (remain <= 0)
    {
        return 0;
    }

    const int applied = ClampInt(requestedCount, 0, remain);
    slot.chainCount += applied;
    const int bindDamage = ClampInt(
        static_cast<int>(std::round(static_cast<float>(slot.enemy->GetMaxHp()) * 0.05f * static_cast<float>(applied))),
        0,
        9999);
    if (bindDamage > 0)
    {
        slot.enemy->Damage(bindDamage);
    }
    if (applied > 0)
    {
        const auto& tran = Transfer::GetInstance();
        const DirectX::XMFLOAT3 startPos = {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.70f,
            tran.player.pos.z
        };
        SpawnChainBeamEffect(startPos, effectPos);
    }
    if (HasChainBloodSynergy())
    {
        for (int i = 0; i < applied; ++i)
        {
            if ((std::rand() % 3) == 0)
            {
                SpawnBloodOrb(effectPos, 1);
            }
        }
    }
    if (HasChainFireSynergy() && sourceDamage > 0)
    {
        ApplyBurnToEnemy(slot, static_cast<float>(sourceDamage) * 0.5f);
    }
    return applied;
}

int SceneGame::ApplyChainToBoss(int requestedCount, int sourceDamage, const DirectX::XMFLOAT3& effectPos)
{
    if (!m_isBossBattleDebug || m_boss.hp <= 0 || requestedCount <= 0)
    {
        return 0;
    }

    const int chainCap = GetChainCapPerTarget();
    const int remain = chainCap - m_bossChainCount;
    if (remain <= 0)
    {
        return 0;
    }

    const int applied = ClampInt(requestedCount, 0, remain);
    m_bossChainCount += applied;
    const int bindDamage = ClampInt(
        static_cast<int>(std::round(static_cast<float>(m_boss.maxHp) * 0.05f * static_cast<float>(applied))),
        0,
        9999);
    if (bindDamage > 0)
    {
        ApplySkillDamageToBoss(bindDamage);
    }
    if (applied > 0)
    {
        const auto& tran = Transfer::GetInstance();
        const DirectX::XMFLOAT3 startPos = {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.70f,
            tran.player.pos.z
        };
        SpawnChainBeamEffect(startPos, effectPos);
    }
    if (HasChainBloodSynergy())
    {
        for (int i = 0; i < applied; ++i)
        {
            if ((std::rand() % 3) == 0)
            {
                SpawnBloodOrb(effectPos, 1);
            }
        }
    }
    if (HasChainFireSynergy() && sourceDamage > 0)
    {
        ApplyBurnToBoss(static_cast<float>(sourceDamage) * 0.5f);
    }
    return applied;
}

void SceneGame::ApplyBurnToEnemy(EnemySlot& slot, float burnBaseDamage)
{
    if (!slot.enemy || burnBaseDamage <= 0.0f)
    {
        return;
    }

    const auto& tran = Transfer::GetInstance();
    const float burnAmount = burnBaseDamage * GetBurnScale();
    slot.burnPool += burnAmount;
    slot.burnDps += burnAmount;
    const int fireTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitFire],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    if (fireTraitLevel >= 6)
    {
        const float currentHp = static_cast<float>(slot.enemy->GetHp());
        if (slot.burnPool > currentHp)
        {
            const int overflowDamage = static_cast<int>(std::floor(slot.burnPool - currentHp + 0.0001f));
            if (overflowDamage > 0)
            {
                slot.enemy->Damage(overflowDamage);
                slot.burnPool -= static_cast<float>(overflowDamage);
                if (slot.burnPool < 0.0f) slot.burnPool = 0.0f;
            }
        }
    }
}

void SceneGame::ApplyBurnToBoss(float burnBaseDamage)
{
    if (!m_isBossBattleDebug || m_boss.hp <= 0 || burnBaseDamage <= 0.0f)
    {
        return;
    }

    const auto& tran = Transfer::GetInstance();
    const float burnAmount = burnBaseDamage * GetBurnScale();
    m_bossBurnPool += burnAmount;
    m_bossBurnDps += burnAmount;
    const int fireTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitFire],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    if (fireTraitLevel >= 6)
    {
        const float currentHp = static_cast<float>(m_boss.hp);
        if (m_bossBurnPool > currentHp)
        {
            const int overflowDamage = static_cast<int>(std::floor(m_bossBurnPool - currentHp + 0.0001f));
            if (overflowDamage > 0)
            {
                m_boss.hp -= overflowDamage;
                if (m_boss.hp < 0) m_boss.hp = 0;
                m_bossBurnPool -= static_cast<float>(overflowDamage);
                if (m_bossBurnPool < 0.0f) m_bossBurnPool = 0.0f;
            }
        }
    }
}

void SceneGame::ApplySkillWeaponBuff(int skillType)
{
    float buffScale = 0.25f;
    switch (NormalizeActionSkillType(skillType))
    {
    case Transfer::RoguelikeUpgrade::ActionSkillWhirl:
        buffScale = 0.30f;
        break;
    case Transfer::RoguelikeUpgrade::ActionSkillRush:
        buffScale = 0.20f;
        break;
    default:
        buffScale = 0.25f;
        break;
    }

    const auto& tran = Transfer::GetInstance();
    if (tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitWeapon] >= 4)
    {
        buffScale *= 3.0f;
    }
    if (m_weaponDamageBuffScale < buffScale)
    {
        m_weaponDamageBuffScale = buffScale;
    }
    m_weaponDamageBuffTimer = kWeaponDamageBuffDuration;
}

void SceneGame::UpdateCursorHoverDebug(float stageSize)
{
    auto& tran = Transfer::GetInstance();
    sprintf_s(tran.gameplayDebug.cursorHoverTarget, sizeof(tran.gameplayDebug.cursorHoverTarget), "%s", "-");

    const POINT mousePos = GetMousePosition();
    if (mousePos.x < 0 || mousePos.y < 0 || mousePos.x >= SCREEN_WIDTH || mousePos.y >= SCREEN_HEIGHT)
    {
        return;
    }

    if (m_pHpFrame && IsPointInsideUiRect(mousePos, m_pHpFrame->GetPosition(), m_pHpFrame->GetSize()))
    {
        sprintf_s(tran.gameplayDebug.cursorHoverTarget, sizeof(tran.gameplayDebug.cursorHoverTarget), "%s", "UI:HPBar");
        return;
    }

    for (int i = 0; i < CooldownSlotCount; ++i)
    {
        if (!m_pCooldownFrame[i]) continue;
        if (!IsPointInsideUiRect(mousePos, m_pCooldownFrame[i]->GetPosition(), m_pCooldownFrame[i]->GetSize())) continue;

        const char* label = "UI:Cooldown";
        switch (i)
        {
        case 0: label = "UI:AttackGauge"; break;
        case 1: label = "UI:EvadeGauge"; break;
        case 2: label = "UI:Skill1Gauge"; break;
        case 3: label = "UI:Skill2Gauge"; break;
        default: break;
        }
        sprintf_s(tran.gameplayDebug.cursorHoverTarget, sizeof(tran.gameplayDebug.cursorHoverTarget), "%s", label);
        return;
    }

    if (m_isBossBattleDebug && m_boss.maxHp > 0)
    {
        const float widthRate = ClampRange(tran.gameplay.bossHpBarWidthRate, 0.20f, 0.90f);
        const float heightRate = ClampRange(tran.gameplay.bossHpBarHeightRate, 0.01f, 0.20f);
        const float barW = static_cast<float>(SCREEN_WIDTH) * widthRate;
        const float barH = static_cast<float>(SCREEN_HEIGHT) * heightRate;
        const float x = (static_cast<float>(SCREEN_WIDTH) - barW) * 0.5f;
        const float y = 12.0f;
        const DirectX::XMFLOAT2 hpCenter = { x + barW * 0.5f, y + barH * 0.5f };
        const DirectX::XMFLOAT2 hpSize = { barW, barH };
        if (IsPointInsideUiRect(mousePos, hpCenter, hpSize))
        {
            sprintf_s(tran.gameplayDebug.cursorHoverTarget, sizeof(tran.gameplayDebug.cursorHoverTarget), "%s", "UI:BossHPBar");
            return;
        }

        const float guardWidth = static_cast<float>(SCREEN_WIDTH) * ClampRange(tran.gameplay.bossGuardBarWidthRate, 0.10f, 0.90f);
        const float guardHeight = static_cast<float>(SCREEN_HEIGHT) * ClampRange(tran.gameplay.bossGuardBarHeightRate, 0.005f, 0.10f);
        const float guardX = ((static_cast<float>(SCREEN_WIDTH) - guardWidth) * 0.5f) + tran.gameplay.bossGuardBarOffsetX;
        const float guardY = y + barH + tran.gameplay.bossGuardBarOffsetY;
        const float guardBarH = MaxFloat(6.0f, guardHeight);
        const DirectX::XMFLOAT2 guardCenter = { guardX + guardWidth * 0.5f, guardY + guardBarH * 0.5f };
        const DirectX::XMFLOAT2 guardSize = { guardWidth, guardBarH };
        if (IsPointInsideUiRect(mousePos, guardCenter, guardSize))
        {
            sprintf_s(tran.gameplayDebug.cursorHoverTarget, sizeof(tran.gameplayDebug.cursorHoverTarget), "%s", "UI:BossGuardBar");
            return;
        }
    }

    if (!m_pCamera)
    {
        return;
    }

    DirectX::XMFLOAT3 rayOrigin{};
    DirectX::XMFLOAT3 rayDir{};
    BuildCursorRay(m_pCamera, mousePos, rayOrigin, rayDir);

    float closestT = 1.0e9f;
    auto recordHit = [&](float t, const char* label)
    {
        if (t < 0.0f || t >= closestT) return;
        closestT = t;
        sprintf_s(tran.gameplayDebug.cursorHoverTarget, sizeof(tran.gameplayDebug.cursorHoverTarget), "%s", label);
    };

    Collision::Box playerBox = MakeAabb(
        {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z
        },
        tran.player.size);
    float hitT = 0.0f;
    if (IntersectRayAabb(rayOrigin, rayDir, playerBox, hitT))
    {
        recordHit(hitT, "Player");
    }

    for (const auto& slot : m_enemies)
    {
        if (!slot.enemy) continue;
        if (IntersectRayAabb(rayOrigin, rayDir, slot.enemy->GetCollision(), hitT))
        {
            recordHit(hitT, "Enemy");
        }
    }

    if (m_isBossBattleDebug && m_boss.hp > 0)
    {
        const Collision::Box bossBox = MakeAabb(
            {
                m_boss.pos.x,
                m_boss.pos.y + m_boss.size.y * 0.5f,
                m_boss.pos.z
            },
            m_boss.size);
        if (IntersectRayAabb(rayOrigin, rayDir, bossBox, hitT))
        {
            recordHit(hitT, "Boss");
        }
    }

    if (std::fabs(rayDir.y) > 1.0e-6f)
    {
        const float floorT = -rayOrigin.y / rayDir.y;
        if (floorT >= 0.0f && floorT < closestT)
        {
            const float hitX = rayOrigin.x + rayDir.x * floorT;
            const float hitZ = rayOrigin.z + rayDir.z * floorT;
            const float stageHalf = stageSize * 0.5f;
            if (hitX >= -stageHalf && hitX <= stageHalf && hitZ >= -stageHalf && hitZ <= stageHalf)
            {
                recordHit(floorT, "Floor");
            }
        }
    }
}

bool SceneGame::ApplySkillDamageToBoss(int damage)
{
    if (!m_isBossBattleDebug || !m_pPlayer || m_boss.hp <= 0 || damage <= 0)
    {
        return false;
    }

    auto& tran = Transfer::GetInstance();
    const float normalDamageScale = ClampRange(tran.gameplay.bossDamageScaleNormal, 0.0f, 5.0f);
    const float brokenDamageScale = ClampRange(tran.gameplay.bossDamageScaleBroken, 0.0f, 10.0f);
    const float breakRecoverDurationSec = ClampRange(tran.gameplay.bossBreakRecoverSec, 1.0f, 30.0f);
    const float guardDamagePerHit = 1.0f;
    const float playerDamage = MaxFloat(static_cast<float>(damage), 0.0f);

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
    m_boss.hpDamageCarry += playerDamage * appliedDamageScale;
    const int hpDamage = static_cast<int>(std::floor(m_boss.hpDamageCarry + 0.0001f));
    if (hpDamage > 0)
    {
        m_boss.hpDamageCarry -= static_cast<float>(hpDamage);
        m_boss.hp -= hpDamage;
    }
    if (m_boss.hp < 0) m_boss.hp = 0;

    if (m_boss.maxHp > 0)
    {
        const float hpRate = Clamp01(static_cast<float>(m_boss.hp) / static_cast<float>(m_boss.maxHp));
        if (m_boss.phase < 2 && hpRate <= 0.50f)
        {
            m_boss.phase = 2;
            m_boss.specialUnlocked = true;
            m_boss.forceUltimatePending = true;
            if (m_boss.attackState == BossController::AttackIdle)
            {
                m_boss.attackCooldownTimer = 0.0f;
            }
        }
        if (m_boss.phase < 3 && hpRate <= 0.25f)
        {
            m_boss.phase = 3;
            if (m_boss.attackState == BossController::AttackIdle &&
                m_boss.attackCooldownTimer > 0.25f)
            {
                m_boss.attackCooldownTimer = 0.25f;
            }
        }
    }

    tran.gameplayDebug.bossHp = static_cast<float>(m_boss.hp);
    tran.gameplayDebug.bossMaxHp = static_cast<float>(m_boss.maxHp);
    tran.gameplayDebug.bossGuard = m_boss.guard;
    tran.gameplayDebug.bossGuardMax = m_boss.guardMax;
    tran.gameplayDebug.bossBroken = m_boss.isBroken ? 1 : 0;

    if (m_boss.hp <= 0)
    {
        m_boss.attackZones.clear();
        m_boss.fallingRocks.clear();
        m_enemyProjectiles.clear();
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
            tran.gameplayDebug.showBossResultTimer = 0;
            BeginClearPortal(ClearPortalFinalResult, m_boss.pos);
            return true;
        }

        tran.gameplayDebug.showBossResultTimer = 0;
        tran.gameplayDebug.runTimerRunning = 0;
        tran.gameplayDebug.runRecordedSec = tran.gameplayDebug.runElapsedSec;
        BeginClearPortal(ClearPortalNextStage, m_boss.pos);
        return true;
    }

    return false;
}

/**
 * @brief Wave 番号から出現敵数を計算します。
 * @param baseCount 基本敵数です。
 * @param waveIndex 現在 Wave 番号です。
 * @param addPerWave Wave ごとの増加数です。
 * @return 計算後の敵数です。
 */
int SceneGame::CalcWaveEnemyCount(int baseCount, int waveIndex, int addPerWave) const
{
    // 基本値と Wave 値を安全範囲に補正し、負方向の増加は許可しません。
    int safeBase = baseCount;
    if (safeBase < kEnemyCountMin) safeBase = kEnemyCountMin;
    if (safeBase > kEnemyCountMax) safeBase = kEnemyCountMax;

    int safeWave = waveIndex;
    if (safeWave < kWaveCountMin) safeWave = kWaveCountMin;

    int safeAdd = addPerWave;
    if (safeAdd < 0) safeAdd = 0;

    // Wave 1 を基点に、追加数を累積して最終敵数を決めます。
    const int waveStep = safeWave - 1;
    const int expanded = safeBase + waveStep * safeAdd;
    if (expanded < kEnemyCountMin) return kEnemyCountMin;
    if (expanded > kEnemyCountMax) return kEnemyCountMax;
    return expanded;
}

/**
 * @brief 戦闘、UI、カメラ、ポーズ状態を 1 フレーム分更新します。
 */
void SceneGame::Update()
{
    TRAN_INS;
    // ポーズ UI の拡大率は許容範囲に丸め、異常値で表示が崩れないようにします。
    tran.gameplayDebug.pauseMenuUiScale = ClampRange(tran.gameplayDebug.pauseMenuUiScale, 0.5f, 2.5f);
    tran.gameplayDebug.pauseMenuFontScale = ClampRange(tran.gameplayDebug.pauseMenuFontScale, 0.5f, 2.5f);
    tran.gameplayDebug.pauseMenuButtonScale = ClampRange(tran.gameplayDebug.pauseMenuButtonScale, 0.5f, 2.5f);
    if (m_pGameBgmVoice)
    {
        // ポーズ中の Option 変更も即時反映するため、毎フレーム音量を適用します。
        const float masterVolume = ClampRange(tran.gameplay.volumeMaster, 0.0f, 2.0f);
        const float bgmVolume = ClampRange(tran.gameplay.volumeBgm, 0.0f, 2.0f);
        m_pGameBgmVoice->SetVolume(masterVolume * bgmVolume);
    }
    if (tran.gameplayDebug.pauseOptionRequestClose != 0)
    {
        tran.gameplayDebug.pauseOptionRequestClose = 0;
        m_isPauseOptionOpen = false;
    }

    if (IsMenuBackTrigger())
    {
        if (!m_isPaused)
        {
            m_isPaused = true;
            m_pauseTabIndex = kPauseTabGame;
            m_isPauseOptionOpen = false;
            m_pauseMenuSelection = kPauseMenuContinue;
        }
        else if (m_isPauseOptionOpen)
        {
            m_isPauseOptionOpen = false;
        }
        else
        {
            m_isPaused = false;
        }
        if (!m_isPaused)
        {
            m_pauseTabIndex = kPauseTabGame;
        }
        tran.gameplayDebug.pauseMenuRequest = 0;
    }

    if (m_isPaused)
    {
        // Pause state splits into menu navigation and option sub-menu navigation.
        if (m_isPauseOptionOpen)
        {
            if (IsKeyTrigger(VK_UP) || IsKeyTrigger('W'))
            {
                m_pauseOptionSelection = WrapIndex(m_pauseOptionSelection - 1, kPauseOptionCount);
            }
            if (IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S'))
            {
                m_pauseOptionSelection = WrapIndex(m_pauseOptionSelection + 1, kPauseOptionCount);
            }

            const bool decrease = IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A');
            const bool increase = IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D');
            if (decrease || increase)
            {
                const float delta = decrease ? -kOptionVolumeStep : kOptionVolumeStep;
                switch (m_pauseOptionSelection)
                {
                case kPauseOptionMaster:
                    tran.gameplay.volumeMaster = ClampVolume(tran.gameplay.volumeMaster + delta);
                    break;
                case kPauseOptionBgm:
                    tran.gameplay.volumeBgm = ClampVolume(tran.gameplay.volumeBgm + delta);
                    break;
                case kPauseOptionSe:
                    tran.gameplay.volumeSe = ClampVolume(tran.gameplay.volumeSe + delta);
                    break;
                case kPauseOptionDisplay:
                    SetAppFullscreen(increase);
                    break;
                default:
                    break;
                }
            }

            if (IsPauseConfirmTriggered())
            {
                if (m_pauseOptionSelection == kPauseOptionDisplay)
                {
                    ToggleAppFullscreen();
                }
                else if (m_pauseOptionSelection == kPauseOptionBack)
                {
                    m_isPauseOptionOpen = false;
                }
            }
        }
        else
        {
            if (IsPauseTabPrevTriggered())
            {
                m_pauseTabIndex = WrapIndex(m_pauseTabIndex - 1, kPauseTabCount);
            }
            if (IsPauseTabNextTriggered())
            {
                m_pauseTabIndex = WrapIndex(m_pauseTabIndex + 1, kPauseTabCount);
            }
            if (IsRawKeyTrigger('1')) m_pauseTabIndex = kPauseTabGame;
            if (IsRawKeyTrigger('2')) m_pauseTabIndex = kPauseTabUpgrade;
            if (IsRawKeyTrigger('3')) m_pauseTabIndex = kPauseTabSettings;

            if (m_pauseTabIndex == kPauseTabSettings)
            {
                if (IsKeyTrigger(VK_UP) || IsKeyTrigger('W') || IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A'))
                {
                    m_pauseMenuSelection = WrapIndex(m_pauseMenuSelection - 1, 3);
                }
                if (IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S') || IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D'))
                {
                    m_pauseMenuSelection = WrapIndex(m_pauseMenuSelection + 1, 3);
                }

                int actionRequest = tran.gameplayDebug.pauseMenuRequest;
                if (actionRequest < 0 || actionRequest > 3) actionRequest = 0;
                const bool confirmByKey = IsPauseConfirmTriggered();
                if (confirmByKey || actionRequest != 0)
                {
                    const int action = (actionRequest != 0)
                        ? actionRequest
                        : ((m_pauseMenuSelection == kPauseMenuContinue) ? 1
                            : (m_pauseMenuSelection == kPauseMenuOption ? 3 : 2));
                    tran.gameplayDebug.pauseMenuRequest = 0;

                    if (action == 1)
                    {
                        // Continue closes pause and resumes gameplay immediately.
                        m_isPaused = false;
                        m_pauseTabIndex = kPauseTabGame;
                        m_isPauseOptionOpen = false;
                    }
                    else if (action == 2)
                    {
                        // Returning to title also resets transient run/boss state.
                        m_isPaused = false;
                        m_pauseTabIndex = kPauseTabGame;
                        m_isPauseOptionOpen = false;
                        tran.gameplayDebug.pauseMenuOpen = 0;
                        tran.gameplayDebug.pauseTabIndex = kPauseTabGame;
                        tran.gameplayDebug.pauseMenuSelection = kPauseMenuContinue;
                        tran.gameplayDebug.pauseOptionOpen = 0;
                        tran.gameplayDebug.pauseOptionSelection = kPauseOptionMaster;
                        tran.gameplayDebug.pauseOptionRequestClose = 0;
                        tran.gameplayDebug.runTimerRunning = 0;
                        tran.gameplayDebug.runRecordedSec = tran.gameplayDebug.runElapsedSec;
                        tran.gameplayDebug.requestBossBattle = 0;
                        tran.gameplayDebug.bossBattleActive = 0;
                        tran.gameplayDebug.showBossResultTimer = 0;
                        tran.ResetRoguelikeUpgrade();
                        SceneManager::ChangeResult(SceneManager::ResultType::None);
                        SceneManager::ChangeScene(SceneManager::SCENE_TITLE);
                        return;
                    }
                    else if (action == 3)
                    {
                        // Option opens the nested pause option menu.
                        m_pauseTabIndex = kPauseTabSettings;
                        m_isPauseOptionOpen = true;
                        m_pauseOptionSelection = kPauseOptionMaster;
                        tran.gameplayDebug.pauseOptionRequestClose = 0;
                    }
                }
            }
            else
            {
                tran.gameplayDebug.pauseMenuRequest = 0;
            }
        }
    }

    tran.gameplayDebug.pauseMenuOpen = m_isPaused ? 1 : 0;
    tran.gameplayDebug.pauseTabIndex = m_pauseTabIndex;
    tran.gameplayDebug.pauseMenuSelection = m_pauseMenuSelection;
    tran.gameplayDebug.pauseOptionOpen = m_isPauseOptionOpen ? 1 : 0;
    tran.gameplayDebug.pauseOptionSelection = m_pauseOptionSelection;
    if (m_isPaused)
    {
        // Gameplay update stops completely while paused.
        return;
    }

    tran.gameplayDebug.pauseMenuRequest = 0;
    tran.gameplayDebug.pauseOptionOpen = 0;
    tran.gameplayDebug.pauseOptionSelection = kPauseOptionMaster;
    tran.gameplayDebug.pauseOptionRequestClose = 0;
    tran.gameplayDebug.bossBattleActive = m_isBossBattleDebug ? 1 : 0;
    int baseEnemyCount = tran.gameplay.enemyCount;
    if (baseEnemyCount < kEnemyCountMin) baseEnemyCount = kEnemyCountMin;
    if (baseEnemyCount > kEnemyCountMax) baseEnemyCount = kEnemyCountMax;
    tran.gameplay.enemyCount = baseEnemyCount;

    int waveMax = tran.gameplay.waveMax;
    if (waveMax < kWaveCountMin) waveMax = kWaveCountMin;
    if (waveMax > kWaveCountMax) waveMax = kWaveCountMax;
    tran.gameplay.waveMax = waveMax;
    m_waveMax = waveMax;

    int waveEnemyAddPerWave = tran.gameplay.waveEnemyAddPerWave;
    if (waveEnemyAddPerWave < 0) waveEnemyAddPerWave = 0;
    if (waveEnemyAddPerWave > kEnemyCountMax) waveEnemyAddPerWave = kEnemyCountMax;
    tran.gameplay.waveEnemyAddPerWave = waveEnemyAddPerWave;
    const int difficultyPreset = ClampInt(tran.gameplayDebug.difficultyPreset, 0, 2);
    tran.gameplayDebug.difficultyPreset = difficultyPreset;
    const int effectiveBaseEnemyCount = ClampInt(
        baseEnemyCount + CalcDifficultyBaseEnemyBonus(difficultyPreset),
        kEnemyCountMin,
        kEnemyCountMax);
    const int effectiveWaveEnemyAdd = ClampInt(
        waveEnemyAddPerWave + CalcDifficultyWaveAddBonus(difficultyPreset),
        0,
        kEnemyCountMax);
    if (m_bossCurseTimer > 0.0f)
    {
        m_bossCurseTimer -= kFixedDt;
        if (m_bossCurseTimer < 0.0f) m_bossCurseTimer = 0.0f;
    }
    const float bossCurseAttackScale = (m_bossCurseTimer > 0.0f) ? 1.25f : 1.0f;
    const float bossCurseSkillScale = (m_bossCurseTimer > 0.0f) ? 1.30f : 1.0f;
    const int legacyAttackDamage = tran.GetPlayerAttackDamageByLevel(tran.roguelike.attackPowerLevel);
    const float legacyAttackCooldownScale = tran.GetAttackCooldownScaleByLevel(tran.roguelike.attackSpeedLevel);
    const float playerEvadeCooldownScale = tran.GetEvadeCooldownScaleByLevel(tran.roguelike.evadeCooldownLevel);
    const int weaponType = NormalizeWeaponType(tran.roguelike.loadoutWeaponType);
    const float weaponAttackRate = GetWeaponAttacksPerSecond(weaponType) * GetWeaponAttackSpeedScale(tran.roguelike);
    const float attackCycle = MaxFloat((1.0f / MaxFloat(weaponAttackRate, 0.1f)) * legacyAttackCooldownScale * bossCurseAttackScale, 0.05f);
    float weaponDamageScale = GetWeaponDamageScale(tran.roguelike, weaponType);
    if (m_weaponDamageBuffTimer > 0.0f)
    {
        weaponDamageScale += m_weaponDamageBuffScale;
    }
    const int playerAttackDamage = ClampInt(
        static_cast<int>(std::ceil(static_cast<float>(legacyAttackDamage) * weaponDamageScale)),
        1,
        9999);
    const float difficultyEnemyAttackDamageScale = CalcDifficultyEnemyAttackDamageScale(difficultyPreset);
    const float roguelikeEnemyAttackScale = m_isBossBattleDebug ? 1.0f : tran.GetEnemyAttackScaleByUpgradeProgress();

    if (m_currentWave < kWaveCountMin) m_currentWave = kWaveCountMin;
    if (m_currentWave > m_waveMax) m_currentWave = m_waveMax;

    const int waveEnemyTarget = CalcWaveEnemyCount(effectiveBaseEnemyCount, m_currentWave, effectiveWaveEnemyAdd);

    const float cameraIntroDuration = ClampRange(
        tran.gameplay.cameraIntroDuration,
        kCameraIntroDurationMin,
        kCameraIntroDurationMax);
    tran.gameplay.cameraIntroDuration = cameraIntroDuration;
    const float attackWindupBase = (tran.gameplay.attackWindup < 0.0f) ? 0.0f : tran.gameplay.attackWindup;
    const float attackDurationBase = (tran.gameplay.attackDuration < kMinDuration) ? kMinDuration : tran.gameplay.attackDuration;
    const float attackRecoveryBase = (tran.gameplay.attackRecovery < 0.0f) ? 0.0f : tran.gameplay.attackRecovery;
    const float attackPhaseTotal = attackWindupBase + attackDurationBase + attackRecoveryBase;
    const float attackPhaseScale = (attackPhaseTotal > 1.0e-4f) ? (attackCycle / attackPhaseTotal) : 1.0f;
    const float attackWindup = attackWindupBase * attackPhaseScale;
    const float attackDuration = MaxFloat(attackDurationBase * attackPhaseScale, kMinDuration * 0.5f);
    const float attackRecovery = attackRecoveryBase * attackPhaseScale;
    const float attackCooldown = 0.0f;
    int multiHitShakeThreshold = tran.gameplay.screenShakeHitThreshold;
    if (multiHitShakeThreshold < kMultiHitShakeThresholdMin) multiHitShakeThreshold = kMultiHitShakeThresholdMin;
    if (multiHitShakeThreshold > kMultiHitShakeThresholdMax) multiHitShakeThreshold = kMultiHitShakeThresholdMax;
    tran.gameplay.screenShakeHitThreshold = multiHitShakeThreshold;
    const float multiHitShakeDuration = (tran.gameplay.screenShakeDuration < 0.0f)
        ? 0.0f : tran.gameplay.screenShakeDuration;
    const float multiHitShakeAmplitude = (tran.gameplay.screenShakeAmplitude < 0.0f)
        ? 0.0f : tran.gameplay.screenShakeAmplitude;
    const float attackSweepDegrees = tran.gameplay.attackSweepDegrees;
    const float attackSweepRadiusScale = (tran.gameplay.attackSweepRadiusScale < 0.1f) ? 0.1f : tran.gameplay.attackSweepRadiusScale;
    const float attackWidthScale = (tran.gameplay.attackWidthScale < 0.1f) ? 0.1f : tran.gameplay.attackWidthScale;
    const float attackDepthScale = (tran.gameplay.attackDepthScale < 0.1f) ? 0.1f : tran.gameplay.attackDepthScale;
    const float attackHitStop = (tran.gameplay.attackHitStop < 0.0f) ? 0.0f : tran.gameplay.attackHitStop;
    const float attackKnockback = (tran.gameplay.attackKnockback < 0.0f) ? 0.0f : tran.gameplay.attackKnockback;
    const float attackTrailInterval = (tran.gameplay.attackTrailInterval < 0.0f) ? 0.0f : tran.gameplay.attackTrailInterval;
    const float attackTrailLife = (tran.gameplay.attackTrailLife < 0.0f) ? 0.0f : tran.gameplay.attackTrailLife;
    const float playerDamageInvincible = (tran.gameplay.playerDamageInvincible < 0.0f) ? 0.0f : tran.gameplay.playerDamageInvincible;
    const float enemyDefeatFlash = (tran.gameplay.enemyDefeatFlash < 0.0f) ? 0.0f : tran.gameplay.enemyDefeatFlash;

    const float enemyAttackWindup = (tran.gameplay.enemyAttackWindup < 0.0f) ? 0.0f : tran.gameplay.enemyAttackWindup;
    const float enemyAttackCooldown = (tran.gameplay.enemyAttackCooldown < 0.0f) ? 0.0f : tran.gameplay.enemyAttackCooldown;
    const float enemyAttackRangeMin = (tran.gameplay.enemyAttackRangeMin < 0.1f) ? 0.1f : tran.gameplay.enemyAttackRangeMin;
    const float enemyAttackRangeScale = (tran.gameplay.enemyAttackRangeScale < 0.1f) ? 0.1f : tran.gameplay.enemyAttackRangeScale;
    const float enemyAttackDamageBase = (tran.gameplay.enemyAttackDamage < 0.0f) ? 0.0f : tran.gameplay.enemyAttackDamage;
    const float enemyMoveSpeedBase = (tran.gameplay.enemyMoveSpeed < 0.1f) ? 0.1f : tran.gameplay.enemyMoveSpeed;
    const float waveEnemyMoveSpeedAdd = (tran.gameplay.waveEnemyMoveSpeedAdd < 0.0f) ? 0.0f : tran.gameplay.waveEnemyMoveSpeedAdd;
    const float waveEnemyAttackDamageScale = (tran.gameplay.waveEnemyAttackDamageScalePerWave < 0.0f)
        ? 0.0f : tran.gameplay.waveEnemyAttackDamageScalePerWave;
    const float waveStep = static_cast<float>((m_currentWave > 0) ? (m_currentWave - 1) : 0);
    const float enemyAttackDamage =
        enemyAttackDamageBase *
        difficultyEnemyAttackDamageScale *
        roguelikeEnemyAttackScale *
        (1.0f + waveEnemyAttackDamageScale * waveStep);
    const float enemyMoveSpeed = enemyMoveSpeedBase + waveEnemyMoveSpeedAdd * waveStep;
    const float enemySeparationRadius = (tran.gameplay.enemySeparationRadius < 0.0f) ? 0.0f : tran.gameplay.enemySeparationRadius;
    const float enemySeparationWeight = (tran.gameplay.enemySeparationWeight < 0.0f) ? 0.0f : tran.gameplay.enemySeparationWeight;
    const float enemySeparationMaxOffset = (tran.gameplay.enemySeparationMaxOffset < 0.0f) ? 0.0f : tran.gameplay.enemySeparationMaxOffset;
    const float enemyProjectileSpeed = (tran.gameplay.enemyProjectileSpeed < 0.1f) ? 0.1f : tran.gameplay.enemyProjectileSpeed;
    const float enemyProjectileLife = (tran.gameplay.enemyProjectileLife < 0.05f) ? 0.05f : tran.gameplay.enemyProjectileLife;
    const float enemyProjectileRadius = (tran.gameplay.enemyProjectileRadius < 0.05f) ? 0.05f : tran.gameplay.enemyProjectileRadius;
    const float enemyProjectileDamageScale = (tran.gameplay.enemyProjectileDamageScale < 0.0f) ? 0.0f : tran.gameplay.enemyProjectileDamageScale;

    const float pushSlop = (tran.gameplay.pushSlop < 0.0f) ? 0.0f : tran.gameplay.pushSlop;
    float playerPushShare = Clamp01(tran.gameplay.playerPushShare);
    float enemyPushShare = Clamp01(tran.gameplay.enemyPushShare);
    const float pushShareSum = playerPushShare + enemyPushShare;
    if (pushShareSum <= 1.0e-6f)
    {
        playerPushShare = 0.5f;
        enemyPushShare = 0.5f;
    }
    else
    {
        playerPushShare /= pushShareSum;
        enemyPushShare /= pushShareSum;
    }

    tran.roguelike.attackPowerLevel = tran.ClampUpgradeLevel(tran.roguelike.attackPowerLevel);
    tran.roguelike.attackSpeedLevel = tran.ClampUpgradeLevel(tran.roguelike.attackSpeedLevel);
    tran.roguelike.evadeCooldownLevel = tran.ClampUpgradeLevel(tran.roguelike.evadeCooldownLevel);
    const int skill1Type = GetSkillTypeForSlot(0);
    const int skill2Type = GetSkillTypeForSlot(1);
    const int cooldownTraitLevel = ClampInt(
        tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitCooldown],
        0,
        Transfer::RoguelikeUpgrade::kTraitLevelMax);
    const float cooldownTraitScale = MaxFloat(1.0f - 0.05f * static_cast<float>(cooldownTraitLevel), 0.35f);
    if (skill1Type != Transfer::RoguelikeUpgrade::ActionSkillNone)
    {
        m_skill1CooldownDuration = MaxFloat(GetActionSkillBaseCooldown(skill1Type) * cooldownTraitScale * bossCurseSkillScale, 0.2f);
    }
    else
    {
        m_skill1CooldownDuration = 0.0f;
    }
    if (skill2Type != Transfer::RoguelikeUpgrade::ActionSkillNone)
    {
        m_skill2CooldownDuration = MaxFloat(GetActionSkillBaseCooldown(skill2Type) * cooldownTraitScale * bossCurseSkillScale, 0.2f);
    }
    else
    {
        m_skill2CooldownDuration = 0.0f;
    }
    RefreshSkillStockState();
    m_weaponAttackStockMax = WeaponUsesProjectile(weaponType) ? 3 : 1;
    if (m_weaponAttackCount == 0 && m_weaponAttackStock < m_weaponAttackStockMax)
    {
        m_weaponAttackStock = m_weaponAttackStockMax;
    }
    if (m_weaponAttackStock > m_weaponAttackStockMax)
    {
        m_weaponAttackStock = m_weaponAttackStockMax;
    }
    if (m_weaponAttackStock < 0)
    {
        m_weaponAttackStock = 0;
    }

    tran.gameplayDebug.effectiveEnemyBaseCount = effectiveBaseEnemyCount;
    tran.gameplayDebug.effectiveEnemyAddPerWave = effectiveWaveEnemyAdd;
    tran.gameplayDebug.effectiveEnemyAttackDamage = enemyAttackDamage;
    tran.gameplayDebug.playerAttackDamage = playerAttackDamage;
    tran.gameplayDebug.playerAttackCooldownScale = attackCycle;
    tran.gameplayDebug.playerEvadeCooldownScale = playerEvadeCooldownScale;
    tran.gameplayDebug.stageClearCount = tran.roguelike.stageClearCount;
    tran.gameplayDebug.attackPowerLevel = tran.roguelike.attackPowerLevel;
    tran.gameplayDebug.attackSpeedLevel = tran.roguelike.attackSpeedLevel;
    tran.gameplayDebug.evadeCooldownLevel = tran.roguelike.evadeCooldownLevel;
    tran.gameplayDebug.lastUpgradeType = tran.roguelike.lastUpgradeType;
    tran.gameplayDebug.upgradeSelectionPending = tran.roguelike.selectionPending;
    tran.gameplayDebug.upgradeRerollRemain = tran.roguelike.rerollRemain;
    tran.gameplayDebug.upgradeOffer0 = tran.roguelike.offers[0];
    tran.gameplayDebug.upgradeOffer1 = tran.roguelike.offers[1];
    tran.gameplayDebug.upgradeOffer2 = tran.roguelike.offers[2];

    if (!m_isBossBgmActive && m_currentWave >= m_waveMax && m_pBossBgm)
    {
        if (m_pGameBgmVoice)
        {
            m_pGameBgmVoice->Stop();
            m_pGameBgmVoice->DestroyVoice();
            m_pGameBgmVoice = nullptr;
        }
        m_pGameBgmVoice = PlaySound(m_pBossBgm);
        if (m_pGameBgmVoice)
        {
            m_isBossBgmActive = true;
        }
    }

    float stageSize = m_stageSize;
    if (tran.player.stageSize > 0.0f)
    {
        stageSize = tran.player.stageSize;
    }

    if (!m_isBossBattleDebug && waveEnemyTarget != m_requestedEnemyCount)
    {
        // Regular mode keeps the requested enemy count aligned with current wave rules.
        m_requestedEnemyCount = waveEnemyTarget;
        EnsureEnemyCount(m_requestedEnemyCount, stageSize);
    }
    else if (m_isBossBattleDebug && m_requestedEnemyCount != 0)
    {
        // Boss debug mode forces the normal enemy population to zero.
        m_requestedEnemyCount = 0;
        EnsureEnemyCount(0, stageSize);
    }

    const int nextMode = NormalizeCameraMode(tran.cameraMode);
    if (nextMode != m_cameraMode)
    {
        m_cameraMode = nextMode;
        m_pCamera = (m_cameraMode == kCameraModeDebug) ? static_cast<Camera*>(m_pCameraDebug)
            : static_cast<Camera*>(m_pCameraGame);
        if (m_pPlayer) m_pPlayer->SetCamera(m_pCamera);
        for (auto& slot : m_enemies)
        {
            if (slot.enemy) slot.enemy->SetCamera(m_pCamera);
        }
    }

    if (m_cameraMode == kCameraModeDebug)
    {
        tran.camera = tran.cameraDebug;
        ApplyCameraPose(m_pCameraDebug, tran.cameraDebug.eye, tran.cameraDebug.look);
    }
    else
    {
        tran.camera = tran.cameraGame;
        ApplyCameraPose(m_pCameraGame, tran.cameraGame.eye, tran.cameraGame.look);
    }

    if (m_pCamera) m_pCamera->Update();

    const DirectX::XMFLOAT3 baseEye = tran.camera.eye;
    const DirectX::XMFLOAT3 baseLook = tran.camera.look;

    if (m_cameraMode == kCameraModeDebug)
    {
        tran.cameraDebug = tran.camera;
    }
    else
    {
        tran.cameraGame = tran.camera;
    }

    if (m_cameraIntroActive)
    {
        // The intro camera briefly pushes toward the player, then returns to the base pose.
        const float safeIntroDuration = (cameraIntroDuration > 0.0f) ? cameraIntroDuration : kFixedDt;
        m_cameraIntroTimer += kFixedDt;
        const float introT = Clamp01(m_cameraIntroTimer / safeIntroDuration);
        const bool returnPhase = (introT >= 0.5f);
        const float phaseT = returnPhase ? (introT - 0.5f) * 2.0f : introT * 2.0f;
        const float easedT = ExpEase01(phaseT);

        const DirectX::XMFLOAT3 introEye = returnPhase
            ? LerpFloat3(m_cameraIntroFocusEye, m_cameraIntroStartEye, easedT)
            : LerpFloat3(m_cameraIntroStartEye, m_cameraIntroFocusEye, easedT);
        const DirectX::XMFLOAT3 introLook = returnPhase
            ? LerpFloat3(m_cameraIntroFocusLook, m_cameraIntroStartLook, easedT)
            : LerpFloat3(m_cameraIntroStartLook, m_cameraIntroFocusLook, easedT);

        tran.camera.eye = introEye;
        tran.camera.look = introLook;
        if (m_cameraMode == kCameraModeDebug)
        {
            tran.cameraDebug = tran.camera;
            ApplyCameraPose(m_pCameraDebug, introEye, introLook);
        }
        else
        {
            tran.cameraGame = tran.camera;
            ApplyCameraPose(m_pCameraGame, introEye, introLook);
        }

        if (introT >= 1.0f)
        {
            m_cameraIntroActive = false;
            m_cameraIntroTimer = 0.0f;
            tran.camera.eye = m_cameraIntroStartEye;
            tran.camera.look = m_cameraIntroStartLook;
            if (m_cameraMode == kCameraModeDebug)
            {
                tran.cameraDebug = tran.camera;
                ApplyCameraPose(m_pCameraDebug, m_cameraIntroStartEye, m_cameraIntroStartLook);
            }
            else
            {
                tran.cameraGame = tran.camera;
                ApplyCameraPose(m_pCameraGame, m_cameraIntroStartEye, m_cameraIntroStartLook);
            }
        }

        tran.gameplayDebug.attackSwingId = m_attackSwingId;
        tran.gameplayDebug.swingHitCount = m_attackHitCountThisSwing;
        tran.gameplayDebug.attackActive = m_attackActive ? 1 : 0;
        tran.gameplayDebug.currentWave = m_currentWave;
        tran.gameplayDebug.maxWave = m_waveMax;
        tran.gameplayDebug.enemiesAlive = static_cast<int>(m_enemies.size());
        tran.gameplayDebug.enemiesTarget = m_requestedEnemyCount;
        UpdateChallengeDebugSetup(stageSize);
        UpdateHpGauge();
        UpdateCooldownGauges();
        m_uiManager.Update(UIObjectManager::Layer::Game);
        UpdateCursorHoverDebug(stageSize);
        // During the intro, gameplay itself stays frozen after UI state is refreshed.
        return;
    }

    if (tran.gameplayDebug.runTimerRunning != 0)
    {
        tran.gameplayDebug.runElapsedSec += kFixedDt;
        if (tran.gameplayDebug.runElapsedSec < 0.0f)
        {
            tran.gameplayDebug.runElapsedSec = 0.0f;
        }
    }

    if (UpdateBossDebugSetup(stageSize))
    {
        return;
    }
    UpdateChallengeDebugSetup(stageSize);

    if (m_hitStopTimer > 0.0f)
    {
        m_hitStopTimer -= kFixedDt;
        if (m_hitStopTimer < 0.0f) m_hitStopTimer = 0.0f;
        tran.gameplayDebug.attackSwingId = m_attackSwingId;
        tran.gameplayDebug.swingHitCount = m_attackHitCountThisSwing;
        tran.gameplayDebug.attackActive = m_attackActive ? 1 : 0;
        tran.gameplayDebug.currentWave = m_currentWave;
        tran.gameplayDebug.maxWave = m_waveMax;
        tran.gameplayDebug.enemiesAlive = static_cast<int>(m_enemies.size());
        tran.gameplayDebug.enemiesTarget = m_requestedEnemyCount;
        UpdateChallengeDebugSetup(stageSize);
        UpdateHpGauge();
        UpdateCooldownGauges();
        m_uiManager.Update(UIObjectManager::Layer::Game);
        UpdateCursorHoverDebug(stageSize);
        return;
    }

    if (m_screenShakeTimer > 0.0f)
    {
        m_screenShakeTimer -= kFixedDt;
        if (m_screenShakeTimer < 0.0f) m_screenShakeTimer = 0.0f;
    }

    if (m_screenShakeTimer > 0.0f && m_cameraMode == kCameraModeGame && m_pCameraGame)
    {
        const float safeDuration = (m_screenShakeDuration > 0.0f) ? m_screenShakeDuration : kMultiHitShakeDurationDefault;
        const float t = Clamp01(m_screenShakeTimer / safeDuration);
        const float strength = m_screenShakeAmplitude * t;
        m_screenShakePhase += kFixedDt * 60.0f;
        const float shakeX = static_cast<float>(std::sin(m_screenShakePhase * 1.73f)) * strength;
        const float shakeY = static_cast<float>(std::cos(m_screenShakePhase * 2.41f)) * strength * 0.45f;

        DirectX::XMFLOAT3 shakenEye = baseEye;
        DirectX::XMFLOAT3 shakenLook = baseLook;
        shakenEye.x += shakeX;
        shakenEye.y += shakeY;
        shakenLook.x += shakeX;
        shakenLook.y += shakeY;
        tran.camera.eye = shakenEye;
        tran.camera.look = shakenLook;
        ApplyCameraPose(m_pCameraGame, shakenEye, shakenLook);
    }
    else
    {
        tran.camera.eye = baseEye;
        tran.camera.look = baseLook;
    }

    if (m_attackTrailSpawnTimer > 0.0f)
    {
        m_attackTrailSpawnTimer -= kFixedDt;
        if (m_attackTrailSpawnTimer < 0.0f) m_attackTrailSpawnTimer = 0.0f;
    }
    if (m_bossStompImpactTimer > 0.0f)
    {
        m_bossStompImpactTimer -= kFixedDt;
        if (m_bossStompImpactTimer < 0.0f) m_bossStompImpactTimer = 0.0f;
    }
    if (m_playerDamageFlashTimer > 0.0f)
    {
        m_playerDamageFlashTimer -= kFixedDt;
        if (m_playerDamageFlashTimer < 0.0f) m_playerDamageFlashTimer = 0.0f;
    }
    if (m_playerDamageInvincibleTimer > 0.0f)
    {
        m_playerDamageInvincibleTimer -= kFixedDt;
        if (m_playerDamageInvincibleTimer < 0.0f) m_playerDamageInvincibleTimer = 0.0f;
    }
    if (m_weaponDamageBuffTimer > 0.0f)
    {
        m_weaponDamageBuffTimer -= kFixedDt;
        if (m_weaponDamageBuffTimer <= 0.0f)
        {
            m_weaponDamageBuffTimer = 0.0f;
            m_weaponDamageBuffScale = 0.0f;
        }
    }
    UpdateHitEffectEmitters(kFixedDt);

    if (m_attackCooldownUiTimer > 0.0f)
    {
        m_attackCooldownUiTimer -= kFixedDt;
        if (m_attackCooldownUiTimer < 0.0f) m_attackCooldownUiTimer = 0.0f;
    }
    for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
    {
        const float cooldownDuration = (slotIndex == 0) ? m_skill1CooldownDuration : m_skill2CooldownDuration;
        if (m_skillStockMax[slotIndex] <= 0 || cooldownDuration <= 0.0f)
        {
            m_skillStockCount[slotIndex] = 0;
            m_skillStockRechargeTimer[slotIndex] = 0.0f;
            continue;
        }
        if (m_skillStockCount[slotIndex] >= m_skillStockMax[slotIndex])
        {
            m_skillStockRechargeTimer[slotIndex] = 0.0f;
            continue;
        }
        if (m_skillStockRechargeTimer[slotIndex] > 0.0f)
        {
            m_skillStockRechargeTimer[slotIndex] -= kFixedDt;
        }
        if (m_skillStockRechargeTimer[slotIndex] <= 0.0f)
        {
            ++m_skillStockCount[slotIndex];
            if (m_skillStockCount[slotIndex] > m_skillStockMax[slotIndex])
            {
                m_skillStockCount[slotIndex] = m_skillStockMax[slotIndex];
            }
            if (m_skillStockCount[slotIndex] < m_skillStockMax[slotIndex])
            {
                m_skillStockRechargeTimer[slotIndex] = cooldownDuration;
            }
            else
            {
                m_skillStockRechargeTimer[slotIndex] = 0.0f;
            }
        }
    }
    m_skill1CooldownTimer = m_skillStockRechargeTimer[0];
    m_skill2CooldownTimer = m_skillStockRechargeTimer[1];
    if (m_weaponAttackStockMax > 1 && m_weaponAttackStock < m_weaponAttackStockMax)
    {
        if (m_weaponAttackStockRechargeTimer > 0.0f)
        {
            m_weaponAttackStockRechargeTimer -= kFixedDt;
        }
        if (m_weaponAttackStockRechargeTimer <= 0.0f)
        {
            ++m_weaponAttackStock;
            if (m_weaponAttackStock > m_weaponAttackStockMax)
            {
                m_weaponAttackStock = m_weaponAttackStockMax;
            }
            if (m_weaponAttackStock < m_weaponAttackStockMax)
            {
                m_weaponAttackStockRechargeTimer = attackCycle;
            }
            else
            {
                m_weaponAttackStockRechargeTimer = 0.0f;
            }
        }
    }
    else if (m_weaponAttackStockMax <= 1)
    {
        m_weaponAttackStock = 1;
        m_weaponAttackStockRechargeTimer = 0.0f;
    }
    const bool requestSkill1 =
        (GetSkillTypeForSlot(0) != Transfer::RoguelikeUpgrade::ActionSkillNone) &&
        m_skill1CooldownDuration > 0.0f &&
        IsKeyTrigger('O');
    const bool requestSkill2 =
        (GetSkillTypeForSlot(1) != Transfer::RoguelikeUpgrade::ActionSkillNone) &&
        m_skill2CooldownDuration > 0.0f &&
        IsKeyTrigger('P');

    if (m_pPlayer) m_pPlayer->Update();
    UpdateSkillActors(kFixedDt, stageSize);
    UpdateBloodOrbs(kFixedDt, playerAttackDamage);
    UpdateChainBeamEffects(kFixedDt);
    if (UpdateClearPortal(kFixedDt))
    {
        return;
    }
    UpdateDamageOverTime(kFixedDt);
    if (requestSkill1 && TryConsumeSkillResource(0) && ActivateSkillSlot(0, legacyAttackDamage, stageSize))
    {
        if (SceneManager::GetCurrent() != SceneManager::SCENE_GAME) return;
    }
    if (requestSkill2 && TryConsumeSkillResource(1) && ActivateSkillSlot(1, legacyAttackDamage, stageSize))
    {
        if (SceneManager::GetCurrent() != SceneManager::SCENE_GAME) return;
    }
    const bool isPlayerEvading = (tran.gameplayDebug.playerEvading != 0);
    const auto tryRevivePlayer = [&]() -> bool
    {
        if (!tran.TryConsumePlayerRevive())
        {
            return false;
        }

        m_playerDamageInvincibleTimer = MaxFloat(m_playerDamageInvincibleTimer, MaxFloat(playerDamageInvincible, 1.0f));
        m_playerDamageFlashTimer = MaxFloat(m_playerDamageFlashTimer, tran.gameplay.playerDamageFlash);
        tran.gameplayDebug.runTimerRunning = 1;
        return true;
    };
    const auto commitLose = [&]()
    {
        tran.gameplayDebug.attackSwingId = m_attackSwingId;
        tran.gameplayDebug.swingHitCount = m_attackHitCountThisSwing;
        tran.gameplayDebug.attackActive = m_attackActive ? 1 : 0;
        tran.gameplayDebug.currentWave = m_currentWave;
        tran.gameplayDebug.maxWave = m_waveMax;
        tran.gameplayDebug.enemiesAlive = static_cast<int>(m_enemies.size());
        tran.gameplayDebug.enemiesTarget = m_requestedEnemyCount;
        tran.gameplayDebug.runTimerRunning = 0;
        tran.gameplayDebug.runRecordedSec = tran.gameplayDebug.runElapsedSec;
        tran.gameplayDebug.bossBattleActive = 0;
        tran.gameplayDebug.showBossResultTimer = 0;
        SceneManager::ChangeResult(SceneManager::ResultType::Lose);
        SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
    };
    const auto applyPlayerDamage = [&](float damage) -> bool
    {
        if (damage <= 0.0f || isPlayerEvading || m_playerDamageInvincibleTimer > 0.0f)
        {
            return false;
        }
        if (m_pPlayerHitSe) PlaySound(m_pPlayerHitSe);
        tran.player.hp -= damage;
        if (m_playerDamageInvincibleTimer < playerDamageInvincible)
        {
            m_playerDamageInvincibleTimer = playerDamageInvincible;
        }
        SpawnHitEffect(
            {
                tran.player.pos.x,
                tran.player.pos.y + tran.player.size.y * 0.5f,
                tran.player.pos.z
            },
            tran.player.size,
            false);
        if (tran.player.hp < 0.0f) tran.player.hp = 0.0f;
        if (tran.player.hp <= 0.0f)
        {
            if (tryRevivePlayer())
            {
                return false;
            }
            commitLose();
            return true;
        }
        return false;
    };

    if (m_pPlayer && !m_enemyProjectiles.empty())
    {
        // Enemy projectiles are stepped first so their hits resolve before the rest of the melee update.
        Collision::Box playerBox = MakeAabb({
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z
        }, tran.player.size);
        const float stageHalf = stageSize * 0.5f;
        const float outsideMargin = enemyProjectileRadius + 0.5f;

        for (auto& shot : m_enemyProjectiles)
        {
            if (shot.life <= 0.0f) continue;
            shot.pos.x += shot.vel.x * kFixedDt;
            shot.pos.y += shot.vel.y * kFixedDt;
            shot.pos.z += shot.vel.z * kFixedDt;
            shot.life -= kFixedDt;
            if (shot.life <= 0.0f)
            {
                shot.life = 0.0f;
                continue;
            }
            if (std::fabs(shot.pos.x) > stageHalf + outsideMargin ||
                std::fabs(shot.pos.z) > stageHalf + outsideMargin)
            {
                shot.life = 0.0f;
                continue;
            }

            if (!isPlayerEvading)
            {
                Collision::Box shotBox = MakeAabb(
                {
                    shot.pos.x,
                    shot.pos.y,
                    shot.pos.z
                },
                {
                    shot.radius * 2.0f,
                    shot.radius * 2.0f,
                    shot.radius * 2.0f
                });
                if (HitAabb(shotBox, playerBox))
                {
                    shot.life = 0.0f;
                    if (applyPlayerDamage(shot.damage))
                    {
                        return;
                    }
                }
            }
        }

        m_enemyProjectiles.erase(
            std::remove_if(m_enemyProjectiles.begin(), m_enemyProjectiles.end(),
                           [](const EnemyProjectile& shot) { return shot.life <= 0.0f; }),
            m_enemyProjectiles.end());
    }

    if (m_challengeActive && m_challengeType == ChallengeDefense)
    {
        if (UpdateChallengeDefense(stageSize))
        {
            return;
        }
    }
    if (m_challengeActive && m_challengeType == ChallengeNoDamage)
    {
        if (UpdateChallengeNoDamage(stageSize))
        {
            return;
        }
    }

    if (!m_challengeActive && UpdateBossBattle(stageSize, playerAttackDamage, applyPlayerDamage))
    {
        // Boss update can end the run immediately, so stop further processing when it does.
        return;
    }

    const int enemyTotal = static_cast<int>(m_enemies.size());
    const bool isDefenseChallenge = (m_challengeActive && m_challengeType == ChallengeDefense);
    const bool heavyEnemyLoad = (enemyTotal >= 8);
    const bool runFullEnemyPairWork = !heavyEnemyLoad || ((m_enemyPerfPhase & 1u) == 0u);
    const int perfPhaseParity = static_cast<int>(m_enemyPerfPhase & 1u);
    ++m_enemyPerfPhase;
    const float separationRadiusSq = enemySeparationRadius * enemySeparationRadius;
    for (int i = 0; i < enemyTotal; ++i)
    {
        EnemySlot& slot = m_enemies[i];
        if (!slot.enemy) continue;

        // Enemy tuning is refreshed every frame so live ImGui edits take effect immediately.
        slot.enemy->SetSize(tran.player.size);
        slot.enemy->SetStageSize(stageSize);
        slot.enemy->SetMoveSpeed(isDefenseChallenge
            ? ClampRange(tran.gameplay.challengeDefenseEnemyMoveSpeed, 0.05f, 4.0f)
            : enemyMoveSpeed);
        slot.enemy->SetForceChase(isDefenseChallenge);

        DirectX::XMFLOAT3 targetPos =
            isDefenseChallenge
            ? m_challengeBeaconPos
            : (m_pPlayer ? m_pPlayer->GetPos() : slot.enemy->GetPos());
        if (!isDefenseChallenge && enemySeparationRadius > 0.0f && enemySeparationWeight > 0.0f)
        {
            const bool runSeparationThisEnemy = runFullEnemyPairWork || (((i + perfPhaseParity) & 1) == 0);
            if (runSeparationThisEnemy)
            {
                const Collision::Box selfBox = slot.enemy->GetCollision();
                DirectX::XMFLOAT3 separation = { 0.0f, 0.0f, 0.0f };
                int separationHitCount = 0;

                for (int j = 0; j < enemyTotal; ++j)
                {
                    if (j == i) continue;
                    Enemy* other = m_enemies[j].enemy;
                    if (!other) continue;

                    const Collision::Box otherBox = other->GetCollision();
                    const float dx = selfBox.center.x - otherBox.center.x;
                    const float dz = selfBox.center.z - otherBox.center.z;
                    if (std::fabs(dx) >= enemySeparationRadius || std::fabs(dz) >= enemySeparationRadius)
                    {
                        continue;
                    }

                    const float distSq = dx * dx + dz * dz;

                    if (distSq <= 1.0e-6f)
                    {
                        const float angle = (2.0f * kPi * static_cast<float>(i)) / static_cast<float>((enemyTotal > 0) ? enemyTotal : 1);
                        separation.x += std::cos(angle);
                        separation.z += std::sin(angle);
                        ++separationHitCount;
                    }
                    else if (distSq < separationRadiusSq)
                    {
                        const float dist = std::sqrt(distSq);
                        const float influence = 1.0f - (dist / enemySeparationRadius);
                        separation.x += (dx / dist) * influence;
                        separation.z += (dz / dist) * influence;
                        ++separationHitCount;
                    }

                    if (separationHitCount >= 4)
                    {
                        break;
                    }
                }

                const float sepLen = std::sqrt(separation.x * separation.x + separation.z * separation.z);
                if (sepLen > 1.0e-6f)
                {
                    DirectX::XMFLOAT3 sepDir = { separation.x / sepLen, 0.0f, separation.z / sepLen };
                    float sepOffset = enemySeparationWeight;
                    if (sepOffset > enemySeparationMaxOffset) sepOffset = enemySeparationMaxOffset;
                    targetPos.x += sepDir.x * sepOffset;
                    targetPos.z += sepDir.z * sepOffset;
                }
            }
        }

        slot.enemy->SetTargetPos(targetPos);
        slot.enemy->Update();
    }

    {
        TRAN_INS;
        const float vLen = std::sqrt(tran.player.velocity.x * tran.player.velocity.x + tran.player.velocity.z * tran.player.velocity.z);
        if (vLen > 0.001f)
        {
            m_lastMoveDir = {
                tran.player.velocity.x / vLen,
                0.0f,
                tran.player.velocity.z / vLen
            };
        }
    }

    if (m_attackCooldownTimer > 0.0f)
    {
        m_attackCooldownTimer -= kFixedDt;
        if (m_attackCooldownTimer < 0.0f) m_attackCooldownTimer = 0.0f;
    }

    if (m_attackRecoveryTimer > 0.0f)
    {
        m_attackRecoveryTimer -= kFixedDt;
        if (m_attackRecoveryTimer < 0.0f) m_attackRecoveryTimer = 0.0f;
    }
    if (m_enemyAttackSeGateTimer > 0.0f)
    {
        m_enemyAttackSeGateTimer -= kFixedDt;
        if (m_enemyAttackSeGateTimer < 0.0f) m_enemyAttackSeGateTimer = 0.0f;
    }

    auto applyCooldownBurst = [&](float scale)
    {
        for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
        {
            if (m_skillStockRechargeTimer[slotIndex] > 0.0f)
            {
                m_skillStockRechargeTimer[slotIndex] *= scale;
            }
        }
        m_skill1CooldownTimer = m_skillStockRechargeTimer[0];
        m_skill2CooldownTimer = m_skillStockRechargeTimer[1];
    };

    auto applyCriticalWeaponEffects = [&](bool isCritical)
    {
        if (!isCritical)
        {
            return;
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeCooldownBurst] != 0)
        {
            applyCooldownBurst(0.9f);
        }
    };

    auto applyWeaponHitExtrasToEnemy = [&](EnemySlot& slot, const DirectX::XMFLOAT3& effectPos, int damage, bool isCritical)
    {
        if (!isCritical)
        {
            return;
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeChainOnCrit] != 0)
        {
            ApplyChainToEnemy(slot, 1, damage, effectPos);
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeBloodOnCrit] != 0)
        {
            SpawnBloodOrb(effectPos, 1);
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeFireOnCrit] != 0)
        {
            ApplyBurnToEnemy(slot, static_cast<float>(damage));
        }
    };

    auto applyWeaponHitExtrasToBoss = [&](const DirectX::XMFLOAT3& effectPos, int damage, bool isCritical)
    {
        if (!isCritical)
        {
            return;
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeChainOnCrit] != 0)
        {
            ApplyChainToBoss(1, damage, effectPos);
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeBloodOnCrit] != 0)
        {
            SpawnBloodOrb(effectPos, 1);
        }
        if (tran.roguelike.weaponUpgradeOwned[Transfer::RoguelikeUpgrade::WeaponUpgradeFireOnCrit] != 0)
        {
            ApplyBurnToBoss(static_cast<float>(damage));
        }
    };

    auto applySkillProjectileExtrasToEnemy = [&](EnemySlot& slot, const Collision::Box& enemyBox, SkillProjectile& shot)
    {
        const int sourceDamage = (shot.sourceDamage > 0) ? shot.sourceDamage : shot.damage;
        if (shot.sourceWeapon)
        {
            applyWeaponHitExtrasToEnemy(slot, enemyBox.center, sourceDamage, shot.critical);
            return;
        }

        const int sourceSkillType = NormalizeActionSkillType(shot.sourceSkillType);
        if (sourceSkillType == Transfer::RoguelikeUpgrade::ActionSkillNone)
        {
            return;
        }

        const bool enhanceWeapon =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceWeapon] != 0;
        const bool enhanceChain =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceChain] != 0;
        const bool enhanceBlood =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceBlood] != 0;
        const bool enhanceFire =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceFire] != 0;

        if (enhanceWeapon)
        {
            ApplySkillWeaponBuff(sourceSkillType);
        }

        int chainCount = 0;
        int bloodCount = 0;
        float burnDamage = 0.0f;
        if (sourceSkillType == Transfer::RoguelikeUpgrade::ActionSkillFireball)
        {
            burnDamage += shot.innateBurnDamage;
        }
        else if (sourceSkillType == Transfer::RoguelikeUpgrade::ActionSkillBloodSlash && shot.remainingInnateBloodCount > 0)
        {
            bloodCount = 1;
            --shot.remainingInnateBloodCount;
        }

        if (enhanceChain)
        {
            chainCount += 1;
        }
        if (enhanceBlood)
        {
            ++bloodCount;
        }
        if (enhanceFire)
        {
            burnDamage += static_cast<float>(sourceDamage);
        }

        if (chainCount > 0)
        {
            ApplyChainToEnemy(slot, chainCount, sourceDamage, enemyBox.center);
        }
        if (bloodCount > 0)
        {
            SpawnBloodOrb(enemyBox.center, bloodCount);
        }
        if (burnDamage > 0.0f)
        {
            ApplyBurnToEnemy(slot, burnDamage);
        }
    };

    auto applySkillProjectileExtrasToBoss = [&](const DirectX::XMFLOAT3& bossCenter, SkillProjectile& shot)
    {
        const int sourceDamage = (shot.sourceDamage > 0) ? shot.sourceDamage : shot.damage;
        if (shot.sourceWeapon)
        {
            applyWeaponHitExtrasToBoss(bossCenter, sourceDamage, shot.critical);
            return;
        }

        const int sourceSkillType = NormalizeActionSkillType(shot.sourceSkillType);
        if (sourceSkillType == Transfer::RoguelikeUpgrade::ActionSkillNone)
        {
            return;
        }

        const bool enhanceWeapon =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceWeapon] != 0;
        const bool enhanceChain =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceChain] != 0;
        const bool enhanceBlood =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceBlood] != 0;
        const bool enhanceFire =
            tran.roguelike.actionSkillEnhancements[sourceSkillType][Transfer::RoguelikeUpgrade::SkillEnhanceFire] != 0;

        if (enhanceWeapon)
        {
            ApplySkillWeaponBuff(sourceSkillType);
        }

        int chainCount = 0;
        int bloodCount = 0;
        float burnDamage = 0.0f;
        if (sourceSkillType == Transfer::RoguelikeUpgrade::ActionSkillFireball)
        {
            burnDamage += shot.innateBurnDamage;
        }
        else if (sourceSkillType == Transfer::RoguelikeUpgrade::ActionSkillBloodSlash && shot.remainingInnateBloodCount > 0)
        {
            bloodCount = 1;
            --shot.remainingInnateBloodCount;
        }

        if (enhanceChain)
        {
            chainCount += 1;
        }
        if (enhanceBlood)
        {
            ++bloodCount;
        }
        if (enhanceFire)
        {
            burnDamage += static_cast<float>(sourceDamage);
        }

        if (chainCount > 0)
        {
            ApplyChainToBoss(chainCount, sourceDamage, bossCenter);
        }
        if (bloodCount > 0)
        {
            SpawnBloodOrb(bossCenter, bloodCount);
        }
        if (burnDamage > 0.0f)
        {
            ApplyBurnToBoss(burnDamage);
        }
    };

    auto calcWeaponHitDamage = [&](bool& outIsCritical) -> int
    {
        ++m_weaponAttackCount;
        const int critNeed = GetWeaponCritNeed(tran.roguelike, weaponType);
        outIsCritical = (critNeed > 0) ? ((m_weaponAttackCount % critNeed) == 0) : false;
        float damageScale = 1.0f;
        if (outIsCritical)
        {
            damageScale *= kWeaponCritBaseDamageScale;
            if (tran.roguelike.traitLevels[Transfer::RoguelikeUpgrade::TraitWeapon] >= 6)
            {
                damageScale *= 1.25f;
            }
        }
        return ClampInt(
            static_cast<int>(std::ceil(static_cast<float>(playerAttackDamage) * damageScale)),
            1,
            9999);
    };

    if (IsKeyTrigger('F') &&
        !m_attackActive &&
        m_attackWindupTimer <= 0.0f &&
        m_attackRecoveryTimer <= 0.0f &&
        m_attackCooldownTimer <= 0.0f)
    {
        bool isCritical = false;
        const int swingDamage = calcWeaponHitDamage(isCritical);
        applyCriticalWeaponEffects(isCritical);

        if (WeaponUsesProjectile(weaponType))
        {
            m_attackCriticalThisSwing = false;
            if (m_weaponAttackStock > 0)
            {
                --m_weaponAttackStock;
                if (m_weaponAttackStockRechargeTimer <= 0.0f)
                {
                    m_weaponAttackStockRechargeTimer = attackCycle;
                }

                DirectX::XMFLOAT3 targetPos = {
                    tran.player.pos.x + m_lastMoveDir.x * kWeaponProjectileMaxDistance,
                    tran.player.pos.y + tran.player.size.y * 0.5f,
                    tran.player.pos.z + m_lastMoveDir.z * kWeaponProjectileMaxDistance
                };
                EnemySlot* bestSlot = nullptr;
                int bestMaxHp = -1;
                int bestHp = -1;
                for (auto& slot : m_enemies)
                {
                    if (!slot.enemy || !slot.enemy->IsAlive()) continue;
                    const int currentMaxHp = slot.enemy->GetMaxHp();
                    const int currentHp = slot.enemy->GetHp();
                    if (!bestSlot || currentMaxHp > bestMaxHp || (currentMaxHp == bestMaxHp && currentHp > bestHp))
                    {
                        bestSlot = &slot;
                        bestMaxHp = currentMaxHp;
                        bestHp = currentHp;
                    }
                }
                if (m_isBossBattleDebug && m_boss.hp > 0)
                {
                    targetPos = {
                        m_boss.pos.x,
                        tran.player.pos.y + tran.player.size.y * 0.5f,
                        m_boss.pos.z
                    };
                }
                else if (bestSlot && bestSlot->enemy)
                {
                    targetPos = bestSlot->enemy->GetCollision().center;
                }

                DirectX::XMFLOAT3 shotDir = NormalizeXZ({
                    targetPos.x - tran.player.pos.x,
                    0.0f,
                    targetPos.z - tran.player.pos.z
                }, m_lastMoveDir);

                SkillProjectile shot{};
                shot.pos = {
                    tran.player.pos.x + shotDir.x * tran.player.size.x,
                    tran.player.pos.y + tran.player.size.y * 0.5f,
                    tran.player.pos.z + shotDir.z * tran.player.size.z
                };
                shot.dir = shotDir;
                shot.radius = kWeaponProjectileRadius;
                shot.speed = kWeaponProjectileSpeed;
                shot.remainDistance = kWeaponProjectileMaxDistance;
                shot.damage = swingDamage;
                shot.projectileId = m_skillProjectileSerial++;
                if (m_skillProjectileSerial < 0) m_skillProjectileSerial = 0;
                shot.knockbackScale = GetWeaponKnockbackScale(weaponType);
                shot.sourceSkillType = Transfer::RoguelikeUpgrade::ActionSkillNone;
                shot.sourceDamage = swingDamage;
                shot.sourceWeapon = true;
                shot.critical = isCritical;
                shot.remainingInnateBloodCount = 0;
                shot.innateBurnDamage = 0.0f;
                m_skillProjectiles.push_back(shot);
                m_attackCooldownUiDuration = attackCycle;
                m_attackCooldownUiTimer = attackCycle;
            }
        }
        else
        {
            // Attack can only start when no other attack phase is still running.
            m_attackDamageThisSwing = swingDamage;
            m_attackKnockbackScaleThisSwing = GetWeaponKnockbackScale(weaponType);
            m_attackCriticalThisSwing = isCritical;
            m_attackWindupTimer = attackWindup;
            m_attackCooldownUiDuration = attackCycle;
            if (m_attackCooldownUiDuration < kMinDuration) m_attackCooldownUiDuration = kMinDuration;
            m_attackCooldownUiTimer = m_attackCooldownUiDuration;
        }
    }

    if (!m_attackActive && m_attackWindupTimer > 0.0f)
    {
        // Windup counts down first, then flips into the active hit window.
        m_attackWindupTimer -= kFixedDt;
        if (m_attackWindupTimer <= 0.0f)
        {
            m_attackWindupTimer = 0.0f;
            m_attackActive = true;
            m_attackTimer = attackDuration;
            ++m_attackSwingId;
            m_attackHitCountThisSwing = 0;
            if (m_attackSwingId < 0) m_attackSwingId = 0;
        }
    }

    if (m_attackActive)
    {
        // Active attack expires into recovery and cooldown.
        m_attackTimer -= kFixedDt;
        if (m_attackTimer <= 0.0f)
        {
            m_attackTimer = 0.0f;
            m_attackActive = false;
            m_attackRecoveryTimer = attackRecovery;
            m_attackCooldownTimer = attackCooldown;
            m_attackCriticalThisSwing = false;
        }
    }

    if (m_attackActive && !WeaponUsesProjectile(weaponType))
    {
        TRAN_INS;
        const DirectX::XMFLOAT3 forward = NormalizeXZ(m_lastMoveDir, { 0.0f, 0.0f, 1.0f });
        const DirectX::XMFLOAT3 right = { forward.z, 0.0f, -forward.x };
        const float progress = Clamp01(1.0f - (m_attackTimer / attackDuration));
        float weaponSweepDegrees = attackSweepDegrees;
        float weaponSweepRadius = attackSweepRadiusScale;
        float weaponWidth = attackWidthScale;
        float weaponDepth = attackDepthScale;
        switch (weaponType)
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
            weaponSweepDegrees *= 0.75f;
            weaponSweepRadius *= 1.15f;
            weaponWidth *= 2.0f;
            weaponDepth *= 2.1f;
            break;
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            weaponSweepDegrees *= 1.15f;
            weaponSweepRadius *= 0.90f;
            weaponWidth *= 0.78f;
            weaponDepth *= 0.85f;
            break;
        default:
            break;
        }
        const float sweepRad = weaponSweepDegrees * (kPi / 180.0f);
        const float angle = (progress - 0.5f) * sweepRad;
        const float c = std::cos(angle);
        const float s = std::sin(angle);
        DirectX::XMFLOAT3 slashDir = {
            (forward.x * c) + (right.x * s),
            0.0f,
            (forward.z * c) + (right.z * s)
        };
        slashDir = NormalizeXZ(slashDir, forward);

        m_attackSize = {
            tran.player.size.x * weaponWidth,
            tran.player.size.y,
            tran.player.size.z * weaponDepth
        };
        const float radius = tran.player.size.z * weaponSweepRadius;
        m_attackCenter = {
            tran.player.pos.x + slashDir.x * radius,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z + slashDir.z * radius
        };

        if (attackTrailLife > 0.0f)
        {
            if (attackTrailInterval <= 0.0f || m_attackTrailSpawnTimer <= 0.0f)
            {
                m_attackTrailSpawnTimer = attackTrailInterval;
            }
        }
    }
    else
    {
        m_attackTrailSpawnTimer = 0.0f;
    }

    if (m_isBossBattleDebug && m_pPlayer && m_boss.hp > 0)
    {
        const Collision::Box bossBox = MakeAabb({
            m_boss.pos.x,
            m_boss.pos.y + m_boss.size.y * 0.5f,
            m_boss.pos.z
        }, m_boss.size);

        for (auto& shot : m_skillProjectiles)
        {
            if (shot.remainDistance <= 0.0f || shot.projectileId == m_lastBossSkillProjectileId) continue;
            const Collision::Box shotBox = MakeAabb(
                { shot.pos.x, shot.pos.y, shot.pos.z },
                { shot.radius * 2.0f, shot.radius * 2.0f, shot.radius * 2.0f });
            if (!HitAabb(shotBox, bossBox)) continue;

            m_lastBossSkillProjectileId = shot.projectileId;
            SpawnHitEffect(bossBox.center, bossBox.size);
            if (ApplySkillDamageToBoss(shot.damage))
            {
                return;
            }
            applySkillProjectileExtrasToBoss(bossBox.center, shot);
            if (m_pAttackSe) PlaySound(m_pAttackSe);
            if (m_hitStopTimer < attackHitStop)
            {
                m_hitStopTimer = attackHitStop;
            }
        }

        if (m_orbitSkill.active && m_bossSkillContactCooldownTimer <= 0.0f)
        {
            TRAN_INS;
            for (int orbitIndex = 0; orbitIndex < m_orbitSkill.count; ++orbitIndex)
            {
                const DirectX::XMFLOAT3 orbitPos = CalcOrbitSatellitePos(
                    m_orbitSkill.angle,
                    m_orbitSkill.radius,
                    m_orbitSkill.count,
                    tran.player.pos,
                    tran.player.size.y,
                    orbitIndex);
                const Collision::Box orbitBox = MakeAabb(orbitPos, m_orbitSkill.size);
                if (!HitAabb(orbitBox, bossBox)) continue;

                m_bossSkillContactCooldownTimer = kSkillOrbitContactCooldown;
                SpawnHitEffect(bossBox.center, bossBox.size);
                if (ApplySkillDamageToBoss(m_orbitSkill.damage))
                {
                    return;
                }
                if (m_pAttackSe) PlaySound(m_pAttackSe);
                if (m_hitStopTimer < attackHitStop * 0.75f)
                {
                    m_hitStopTimer = attackHitStop * 0.75f;
                }
                break;
            }
        }
    }

    if (m_pPlayer && !m_enemies.empty())
    {
        TRAN_INS;

        Collision::Box playerBox = MakeAabb({
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z
        }, tran.player.size);
        DirectX::XMFLOAT3 playerCenter = playerBox.center;
        bool pushedPlayer = false;
        const float stageHalf = stageSize * 0.5f;
        auto clampCenterToStage = [&](DirectX::XMFLOAT3& center, const DirectX::XMFLOAT3& size)
        {
            const float halfX = size.x * 0.5f;
            const float halfZ = size.z * 0.5f;
            float minX = -stageHalf + halfX;
            float maxX = stageHalf - halfX;
            float minZ = -stageHalf + halfZ;
            float maxZ = stageHalf - halfZ;
            if (minX > maxX) { minX = 0.0f; maxX = 0.0f; }
            if (minZ > maxZ) { minZ = 0.0f; maxZ = 0.0f; }
            center.x = ClampRange(center.x, minX, maxX);
            center.z = ClampRange(center.z, minZ, maxZ);
        };

        const int enemyCountNow = static_cast<int>(m_enemies.size());
        const int pairIStart = runFullEnemyPairWork ? 0 : perfPhaseParity;
        const int pairIStep = runFullEnemyPairWork ? 1 : 2;
        for (int i = pairIStart; i < enemyCountNow - 1; i += pairIStep)
        {
            Enemy* enemyA = m_enemies[i].enemy;
            if (!enemyA) continue;

            for (int j = i + 1; j < enemyCountNow; ++j)
            {
                Enemy* enemyB = m_enemies[j].enemy;
                if (!enemyB) continue;

                Collision::Box boxA = enemyA->GetCollision();
                Collision::Box boxB = enemyB->GetCollision();

                const float diffX = boxB.center.x - boxA.center.x;
                const float diffZ = boxB.center.z - boxA.center.z;
                const float overlapX = (boxA.size.x + boxB.size.x) * 0.5f - std::fabs(diffX);
                const float overlapZ = (boxA.size.z + boxB.size.z) * 0.5f - std::fabs(diffZ);
                if (overlapX <= 0.0f || overlapZ <= 0.0f)
                {
                    continue;
                }

                float pushDirX = 0.0f;
                float pushDirZ = 0.0f;
                float penetration = overlapX;
                if (overlapX <= overlapZ)
                {
                    pushDirX = (diffX >= 0.0f) ? 1.0f : -1.0f;
                    if (std::fabs(diffX) < 1.0e-4f)
                    {
                        pushDirX = ((i + j) & 1) ? -1.0f : 1.0f;
                    }
                }
                else
                {
                    pushDirZ = (diffZ >= 0.0f) ? 1.0f : -1.0f;
                    if (std::fabs(diffZ) < 1.0e-4f)
                    {
                        pushDirZ = ((i + j) & 1) ? -1.0f : 1.0f;
                    }
                    penetration = overlapZ;
                }

                const float pushDist = penetration + pushSlop;
                boxA.center.x -= pushDirX * pushDist * 0.5f;
                boxA.center.z -= pushDirZ * pushDist * 0.5f;
                boxB.center.x += pushDirX * pushDist * 0.5f;
                boxB.center.z += pushDirZ * pushDist * 0.5f;

                clampCenterToStage(boxA.center, boxA.size);
                clampCenterToStage(boxB.center, boxB.size);

                enemyA->SetPos({ boxA.center.x, 0.0f, boxA.center.z });
                enemyB->SetPos({ boxB.center.x, 0.0f, boxB.center.z });
            }
        }

        for (auto& slot : m_enemies)
        {
            if (!slot.enemy) continue;
            if (slot.hitFlashTimer > 0.0f)
            {
                slot.hitFlashTimer -= kFixedDt;
                if (slot.hitFlashTimer < 0.0f) slot.hitFlashTimer = 0.0f;
            }

            Collision::Box enemyBox = slot.enemy->GetCollision();

            // Resolve overlap with a simple push response so player/enemy do not stack.
            const float diffX = enemyBox.center.x - playerCenter.x;
            const float diffZ = enemyBox.center.z - playerCenter.z;
            const float overlapX = (playerBox.size.x + enemyBox.size.x) * 0.5f - std::fabs(diffX);
            const float overlapZ = (playerBox.size.z + enemyBox.size.z) * 0.5f - std::fabs(diffZ);
            if (!isPlayerEvading && overlapX > 0.0f && overlapZ > 0.0f)
            {
                float pushDirX = 0.0f;
                float pushDirZ = 0.0f;
                float penetration = overlapX;
                if (overlapX <= overlapZ)
                {
                    pushDirX = (diffX >= 0.0f) ? 1.0f : -1.0f;
                    if (std::fabs(diffX) < 1.0e-4f)
                    {
                        pushDirX = (m_lastMoveDir.x >= 0.0f) ? 1.0f : -1.0f;
                    }
                }
                else
                {
                    pushDirZ = (diffZ >= 0.0f) ? 1.0f : -1.0f;
                    if (std::fabs(diffZ) < 1.0e-4f)
                    {
                        pushDirZ = (m_lastMoveDir.z >= 0.0f) ? 1.0f : -1.0f;
                    }
                    penetration = overlapZ;
                }

                const float pushDist = penetration + pushSlop;
                playerCenter.x -= pushDirX * pushDist * playerPushShare;
                playerCenter.z -= pushDirZ * pushDist * playerPushShare;
                enemyBox.center.x += pushDirX * pushDist * enemyPushShare;
                enemyBox.center.z += pushDirZ * pushDist * enemyPushShare;

                clampCenterToStage(playerCenter, playerBox.size);
                clampCenterToStage(enemyBox.center, enemyBox.size);

                playerBox.center = playerCenter;
                slot.enemy->SetPos({ enemyBox.center.x, 0.0f, enemyBox.center.z });
                pushedPlayer = true;
            }

            enemyBox = slot.enemy->GetCollision();
            if (isDefenseChallenge)
            {
                const float beaconReach =
                    (m_challengeBeaconSize.x > m_challengeBeaconSize.z)
                    ? m_challengeBeaconSize.x * 0.5f
                    : m_challengeBeaconSize.z * 0.5f;
                if (DistSqXZ(enemyBox.center, m_challengeBeaconPos) <= beaconReach * beaconReach)
                {
                    m_challengeBeaconHp -= ClampRange(tran.gameplay.challengeDefenseBeaconContactDamage, 0.1f, 100.0f);
                    if (m_challengeBeaconHp < 0.0f)
                    {
                        m_challengeBeaconHp = 0.0f;
                    }
                    delete slot.enemy;
                    slot.enemy = nullptr;
                    slot.attackWindupTimer = 0.0f;
                    slot.attackCooldownTimer = 0.0f;
                    slot.debugInAttackRange = false;
                    slot.debugAttackRange = 0.0f;
                    if (m_challengeBeaconHp <= 0.0f)
                    {
                        FinishChallenge(false);
                        return;
                    }
                    continue;
                }
            }
            if (slot.attackCooldownTimer > 0.0f)
            {
                slot.attackCooldownTimer -= kFixedDt;
                if (slot.attackCooldownTimer < 0.0f) slot.attackCooldownTimer = 0.0f;
            }

            const float dx = enemyBox.center.x - playerBox.center.x;
            const float dz = enemyBox.center.z - playerBox.center.z;
            const float distSq = dx * dx + dz * dz;
            const float typeRangeScale = slot.enemy->GetAttackRangeScale();
            const float typeWindupScale = slot.enemy->GetAttackWindupScale();
            const float typeCooldownScale = slot.enemy->GetAttackCooldownScale();
            const float typeDamageScale = slot.enemy->GetAttackDamageScale();
            const bool isRangedEnemy = (slot.enemy->GetType() == static_cast<int>(Enemy::Type::Ranged));
            const float enemyRangeSize = (enemyBox.size.x > enemyBox.size.z) ? enemyBox.size.x : enemyBox.size.z;
            const float playerRangeSize = (playerBox.size.x > playerBox.size.z) ? playerBox.size.x : playerBox.size.z;
            float attackRange = (enemyRangeSize + playerRangeSize) * enemyAttackRangeScale * typeRangeScale;
            if (attackRange < enemyAttackRangeMin) attackRange = enemyAttackRangeMin;
            const bool inAttackRange = distSq <= attackRange * attackRange;
            slot.debugInAttackRange = inAttackRange;
            slot.debugAttackRange = attackRange;

            if (isDefenseChallenge)
            {
                slot.attackWindupTimer = 0.0f;
                slot.attackCooldownTimer = 0.0f;
            }
            else if (slot.attackWindupTimer > 0.0f)
            {
                slot.attackWindupTimer -= kFixedDt;
                if (slot.attackWindupTimer <= 0.0f)
                {
                    slot.attackWindupTimer = 0.0f;
                    slot.attackCooldownTimer = enemyAttackCooldown * typeCooldownScale;

                    if (inAttackRange)
                    {
                        if (isRangedEnemy)
                        {
                            DirectX::XMFLOAT3 dir = {
                                playerBox.center.x - enemyBox.center.x,
                                0.0f,
                                playerBox.center.z - enemyBox.center.z
                            };
                            dir = NormalizeXZ(dir, { 0.0f, 0.0f, 1.0f });

                            EnemyProjectile shot{};
                            shot.pos = {
                                enemyBox.center.x,
                                playerBox.center.y,
                                enemyBox.center.z
                            };
                            shot.vel = {
                                dir.x * enemyProjectileSpeed,
                                0.0f,
                                dir.z * enemyProjectileSpeed
                            };
                            shot.radius = enemyProjectileRadius;
                            shot.life = enemyProjectileLife;
                            shot.damage = enemyAttackDamage * typeDamageScale * enemyProjectileDamageScale;
                            if (m_enemyProjectiles.size() < 128)
                            {
                                m_enemyProjectiles.push_back(shot);
                            }
                        }
                        else if (applyPlayerDamage(enemyAttackDamage * typeDamageScale))
                        {
                            return;
                        }
                    }
                }
            }
            else if (slot.attackCooldownTimer <= 0.0f && inAttackRange)
            {
                slot.attackWindupTimer = enemyAttackWindup * typeWindupScale;
                if (m_pEnemyAttackSe && m_enemyAttackSeGateTimer <= 0.0f)
                {
                    PlaySound(m_pEnemyAttackSe);
                    m_enemyAttackSeGateTimer = 0.08f;
                }
            }

            if (m_attackActive && slot.lastHitSwingId != m_attackSwingId)
            {
                Collision::Box attackBox{};
                attackBox.center = m_attackCenter;
                attackBox.size = m_attackSize;
                if (HitAabb(attackBox, enemyBox))
                {
                    SpawnHitEffect(enemyBox.center, enemyBox.size);
                    slot.enemy->Damage((m_attackDamageThisSwing > 0) ? m_attackDamageThisSwing : playerAttackDamage);
                    applyWeaponHitExtrasToEnemy(
                        slot,
                        enemyBox.center,
                        (m_attackDamageThisSwing > 0) ? m_attackDamageThisSwing : playerAttackDamage,
                        m_attackCriticalThisSwing);
                    slot.lastHitSwingId = m_attackSwingId;
                    ++m_attackHitCountThisSwing;
                    if (m_attackHitCountThisSwing == multiHitShakeThreshold &&
                        multiHitShakeDuration > 0.0f &&
                        multiHitShakeAmplitude > 0.0f)
                    {
                        m_screenShakeDuration = multiHitShakeDuration;
                        if (m_screenShakeTimer < multiHitShakeDuration)
                        {
                            m_screenShakeTimer = multiHitShakeDuration;
                        }
                        if (m_screenShakeAmplitude < multiHitShakeAmplitude)
                        {
                            m_screenShakeAmplitude = multiHitShakeAmplitude;
                        }
                        m_screenShakePhase = 0.0f;
                    }
                    if (m_pAttackSe) PlaySound(m_pAttackSe);

                    const DirectX::XMFLOAT3 knockDir = NormalizeXZ({
                        enemyBox.center.x - m_attackCenter.x,
                        0.0f,
                        enemyBox.center.z - m_attackCenter.z
                    }, m_lastMoveDir);
                    enemyBox.center.x += knockDir.x * attackKnockback * m_attackKnockbackScaleThisSwing;
                    enemyBox.center.z += knockDir.z * attackKnockback * m_attackKnockbackScaleThisSwing;
                    clampCenterToStage(enemyBox.center, enemyBox.size);
                    slot.enemy->SetPos({ enemyBox.center.x, 0.0f, enemyBox.center.z });

                    if (m_hitStopTimer < attackHitStop)
                    {
                        m_hitStopTimer = attackHitStop;
                    }
                }
            }

            for (auto& skillShot : m_skillProjectiles)
            {
                if (skillShot.remainDistance <= 0.0f || slot.lastSkillProjectileId == skillShot.projectileId) continue;
                const Collision::Box shotBox = MakeAabb(
                    { skillShot.pos.x, skillShot.pos.y, skillShot.pos.z },
                    { skillShot.radius * 2.0f, skillShot.radius * 2.0f, skillShot.radius * 2.0f });
                if (!HitAabb(shotBox, enemyBox)) continue;

                slot.lastSkillProjectileId = skillShot.projectileId;
                SpawnHitEffect(enemyBox.center, enemyBox.size);
                slot.enemy->Damage(skillShot.damage);
                applySkillProjectileExtrasToEnemy(slot, enemyBox, skillShot);
                const DirectX::XMFLOAT3 knockDir = NormalizeXZ(skillShot.dir, m_lastMoveDir);
                enemyBox.center.x += knockDir.x * attackKnockback * skillShot.knockbackScale;
                enemyBox.center.z += knockDir.z * attackKnockback * skillShot.knockbackScale;
                clampCenterToStage(enemyBox.center, enemyBox.size);
                slot.enemy->SetPos({ enemyBox.center.x, 0.0f, enemyBox.center.z });
                if (m_pAttackSe) PlaySound(m_pAttackSe);
                if (m_hitStopTimer < attackHitStop * 0.7f)
                {
                    m_hitStopTimer = attackHitStop * 0.7f;
                }
            }

            if (m_orbitSkill.active && slot.skillContactCooldownTimer <= 0.0f)
            {
                bool orbitHit = false;
                DirectX::XMFLOAT3 orbitHitPos = m_orbitSkill.pos;
                for (int orbitIndex = 0; orbitIndex < m_orbitSkill.count; ++orbitIndex)
                {
                    const DirectX::XMFLOAT3 orbitPos = CalcOrbitSatellitePos(
                        m_orbitSkill.angle,
                        m_orbitSkill.radius,
                        m_orbitSkill.count,
                        tran.player.pos,
                        tran.player.size.y,
                        orbitIndex);
                    const Collision::Box orbitBox = MakeAabb(orbitPos, m_orbitSkill.size);
                    if (!HitAabb(orbitBox, enemyBox)) continue;
                    orbitHit = true;
                    orbitHitPos = orbitPos;
                    break;
                }
                if (orbitHit)
                {
                    slot.skillContactCooldownTimer = kSkillOrbitContactCooldown;
                    SpawnHitEffect(enemyBox.center, enemyBox.size);
                    slot.enemy->Damage(m_orbitSkill.damage);
                    const DirectX::XMFLOAT3 knockDir = NormalizeXZ({
                        enemyBox.center.x - orbitHitPos.x,
                        0.0f,
                        enemyBox.center.z - orbitHitPos.z
                    }, m_lastMoveDir);
                    enemyBox.center.x += knockDir.x * attackKnockback * 0.5f;
                    enemyBox.center.z += knockDir.z * attackKnockback * 0.5f;
                    clampCenterToStage(enemyBox.center, enemyBox.size);
                    slot.enemy->SetPos({ enemyBox.center.x, 0.0f, enemyBox.center.z });
                    if (m_pAttackSe) PlaySound(m_pAttackSe);
                    if (m_hitStopTimer < attackHitStop * 0.5f)
                    {
                        m_hitStopTimer = attackHitStop * 0.5f;
                    }

                }
            }

            if (!slot.enemy->IsAlive())
            {
                if (enemyDefeatFlash > 0.0f)
                {
                }
                delete slot.enemy;
                slot.enemy = nullptr;
                slot.attackWindupTimer = 0.0f;
                slot.attackCooldownTimer = 0.0f;
                slot.debugInAttackRange = false;
                slot.debugAttackRange = 0.0f;
            }
        }

        if (pushedPlayer)
        {
            tran.player.pos.x = playerCenter.x;
            tran.player.pos.z = playerCenter.z;
            if (m_pPlayer)
            {
                m_pPlayer->SetPos({ playerCenter.x, tran.player.pos.y, playerCenter.z });
            }
        }

        m_enemies.erase(
            std::remove_if(m_enemies.begin(), m_enemies.end(), [](const EnemySlot& slot) { return slot.enemy == nullptr; }),
            m_enemies.end());

		const int currentStageType = tran.GetCurrentRunStageType();
		const bool isTagRewardStage =
			tran.GetCurrentRunRewardType() == Transfer::RoguelikeUpgrade::RewardTag;
		const bool isCombatStage =
			currentStageType != Transfer::RoguelikeUpgrade::StageShop &&
			currentStageType != Transfer::RoguelikeUpgrade::StageRest &&
			currentStageType != Transfer::RoguelikeUpgrade::StageBoss &&
			currentStageType != Transfer::RoguelikeUpgrade::StageFinalBoss &&
			!isTagRewardStage;
		if (isTagRewardStage &&
			tran.roguelike.selectionPending == 0 &&
			tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionNone)
		{
			tran.gameplayDebug.runTimerRunning = 0;
			tran.BeginCurrentStageRewardSelection();
			if (tran.roguelike.selectionPending != 0)
			{
				SceneManager::ChangeResult(SceneManager::ResultType::Win);
				SceneManager::ChangeScene(SceneManager::SCENE_RESULT);
				return;
			}
		}
		if (isCombatStage &&
			m_clearPortalMode == ClearPortalNone &&
			!m_isBossBattleDebug &&
			!m_challengeActive &&
			m_enemies.empty())
		{
            // Clearing all enemies advances the wave, or finishes the stage on the final wave.
            m_enemyProjectiles.clear();
            if (m_currentWave < m_waveMax)
            {
                ++m_currentWave;
                m_requestedEnemyCount = CalcWaveEnemyCount(effectiveBaseEnemyCount, m_currentWave, effectiveWaveEnemyAdd);
                EnsureEnemyCount(m_requestedEnemyCount, stageSize);
            }
            else
            {
                if (m_pClearSe) PlaySound(m_pClearSe);
                tran.gameplayDebug.attackSwingId = m_attackSwingId;
                tran.gameplayDebug.swingHitCount = m_attackHitCountThisSwing;
                tran.gameplayDebug.attackActive = m_attackActive ? 1 : 0;
                tran.gameplayDebug.currentWave = m_currentWave;
                tran.gameplayDebug.maxWave = m_waveMax;
                tran.gameplayDebug.enemiesAlive = static_cast<int>(m_enemies.size());
                tran.gameplayDebug.enemiesTarget = m_requestedEnemyCount;
                tran.gameplayDebug.runTimerRunning = 0;
                tran.gameplayDebug.runRecordedSec = tran.gameplayDebug.runElapsedSec;
                tran.gameplayDebug.bossBattleActive = 0;
                tran.gameplayDebug.showBossResultTimer = 0;
                BeginClearPortal(ClearPortalReward, { 0.0f, 0.0f, 0.0f });
            }
        }
    }

    {
        TRAN_INS;
        Enemy* trackedEnemy = nullptr;
        float nearestDistSq = 1.0e30f;

        // Publish the nearest enemy into Transfer for UI and direction-marker overlap checks.
        for (const auto& slot : m_enemies)
        {
            if (!slot.enemy) continue;
            const DirectX::XMFLOAT3 pos = slot.enemy->GetPos();
            const float dx = pos.x - tran.player.pos.x;
            const float dz = pos.z - tran.player.pos.z;
            const float distSq = dx * dx + dz * dz;
            if (!trackedEnemy || distSq < nearestDistSq)
            {
                trackedEnemy = slot.enemy;
                nearestDistSq = distSq;
            }
        }

        if (trackedEnemy)
        {
            tran.enemy.exists = 1;
            tran.enemy.pos = trackedEnemy->GetPos();
            tran.enemy.hp = static_cast<float>(trackedEnemy->GetHp());
            tran.enemy.maxHp = static_cast<float>(trackedEnemy->GetMaxHp());
            tran.enemy.state = trackedEnemy->GetState();
            tran.enemy.type = trackedEnemy->GetType();
        }
        else
        {
            tran.enemy.exists = 0;
            tran.enemy.pos = { 0.0f, 0.0f, 0.0f };
            tran.enemy.hp = 0.0f;
            tran.enemy.maxHp = 0.0f;
            tran.enemy.state = -1;
            tran.enemy.type = -1;
        }
    }

    tran.gameplayDebug.attackSwingId = m_attackSwingId;
    tran.gameplayDebug.swingHitCount = m_attackHitCountThisSwing;
    tran.gameplayDebug.attackActive = m_attackActive ? 1 : 0;
    tran.gameplayDebug.currentWave = m_currentWave;
    tran.gameplayDebug.maxWave = m_waveMax;
    tran.gameplayDebug.enemiesAlive = static_cast<int>(m_enemies.size());
    tran.gameplayDebug.enemiesTarget = m_requestedEnemyCount;
    tran.gameplayDebug.bossBattleActive = m_isBossBattleDebug ? 1 : 0;
    UpdateChallengeDebugSetup(stageSize);

    // Final UI state is refreshed after all gameplay mutations for the frame are complete.
    UpdateHpGauge();
    UpdateCooldownGauges();
    m_uiManager.Update(UIObjectManager::Layer::Game);
    UpdateCursorHoverDebug(stageSize);
}

/**
 * @brief ゲームシーンの 3D と UI を描画します。
 */
void SceneGame::Draw()
{
    // カメラが無いと描画行列を作れないため何も描画しません。
    if (!m_pCamera) return;

    DirectX::XMFLOAT4X4 view = m_pCamera->GetViewMatrix();
    DirectX::XMFLOAT4X4 proj = m_pCamera->GetProjectionMatrix();

    // Geometry と Sprite の両方へ同じカメラ行列を設定します。
    Geometory::SetView(view);
    Geometory::SetProjection(proj);
    Sprite::SetView(view);
    Sprite::SetProjection(proj);

    SetDepthTest(true);

    float stage = m_stageSize;
    float groundTileSize = 1.0f;
    {
        TRAN_INS;
        if (tran.player.stageSize > 0.0f) stage = tran.player.stageSize;
        groundTileSize = tran.gameplay.groundTileSize;
    }

    if (m_pGroundTexture)
    {
        // The floor is drawn as tiled sprites so arbitrary stage sizes are covered without stretching.
        const float groundY = -0.001f;
        const float safeStage = (stage > 0.1f) ? stage : 0.1f;
        const float baseTileSize = ClampRange(groundTileSize, 0.5f, 10.0f);
        const int tileCountX = ClampInt(static_cast<int>(std::ceil(safeStage / baseTileSize)), 1, 64);
        const int tileCountZ = ClampInt(static_cast<int>(std::ceil(safeStage / baseTileSize)), 1, 64);
        const float stageHalf = safeStage * 0.5f;
        const DirectX::XMMATRIX rotate = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2);

        float cursorZ = -stageHalf;
        for (int z = 0; z < tileCountZ; ++z)
        {
            float tileDepth = baseTileSize;
            if (z == tileCountZ - 1)
            {
                tileDepth = safeStage - baseTileSize * static_cast<float>(tileCountZ - 1);
            }
            if (tileDepth <= 0.0f) continue;

            float cursorX = -stageHalf;
            for (int x = 0; x < tileCountX; ++x)
            {
                float tileWidth = baseTileSize;
                if (x == tileCountX - 1)
                {
                    tileWidth = safeStage - baseTileSize * static_cast<float>(tileCountX - 1);
                }
                if (tileWidth <= 0.0f) continue;

                const float centerX = cursorX + tileWidth * 0.5f;
                const float centerZ = cursorZ + tileDepth * 0.5f;
                DirectX::XMMATRIX translate = DirectX::XMMatrixTranslation(centerX, groundY, centerZ);
                DirectX::XMFLOAT4X4 world;
                DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(rotate * translate));

                Sprite::SetWorld(world);
                Sprite::SetSize({ tileWidth, tileDepth });
                Sprite::SetOffset({ 0.0f, 0.0f });
                Sprite::SetUVPos({ 0.0f, 0.0f });
                Sprite::SetUVScale({ tileWidth / baseTileSize, tileDepth / baseTileSize });
                Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
                Sprite::SetTexture(m_pGroundTexture);
                Sprite::Draw();

                cursorX += tileWidth;
            }
            cursorZ += tileDepth;
        }
    }

    SetDepthTest(false);

    if (m_attackActive)
    {
        // Active player swing is visualized as a floor marker.
        DrawAttackMarker(m_pAttackMarker, m_attackCenter, m_attackSize);
    }
    DrawClearPortal();
    if (m_pAttackMarker)
    {
        for (const auto& shot : m_enemyProjectiles)
        {
            if (shot.life <= 0.0f) continue;
            const float size = shot.radius * 2.0f;
            DrawAttackMarkerTint(
                m_pAttackMarker,
                shot.pos,
                { size, size, size },
                kEnemyProjectileColor);
        }
    }
    if (m_pSkillTexture)
    {
        for (const auto& shot : m_skillProjectiles)
        {
            if (shot.remainDistance <= 0.0f) continue;
            const float size = shot.radius * 2.6f;
            DrawBillboardMarkerTint(
                m_pSkillTexture,
                m_pCamera,
                shot.pos,
                { size, size, size },
                { 0.55f, 0.95f, 1.0f, 0.92f });
        }
        if (m_orbitSkill.active)
        {
            TRAN_INS;
            for (int orbitIndex = 0; orbitIndex < m_orbitSkill.count; ++orbitIndex)
            {
                const DirectX::XMFLOAT3 orbitPos = CalcOrbitSatellitePos(
                    m_orbitSkill.angle,
                    m_orbitSkill.radius,
                    m_orbitSkill.count,
                    tran.player.pos,
                    tran.player.size.y,
                    orbitIndex);
                DrawBillboardMarkerTint(
                    m_pSkillTexture,
                    m_pCamera,
                    orbitPos,
                    m_orbitSkill.size,
                    { 1.0f, 0.92f, 0.40f, 0.95f });
            }
        }
    }
    if (m_pBloodUiTexture || m_pSkillTexture)
    {
        Texture* bloodTexture = m_pBloodUiTexture ? m_pBloodUiTexture : m_pSkillTexture;
        for (const auto& orb : m_bloodOrbs)
        {
            const float size = 0.55f;
            DrawBillboardMarkerTint(
                bloodTexture,
                m_pCamera,
                {
                    orb.pos.x,
                    orb.pos.y + 0.35f,
                    orb.pos.z
                },
                { size, size, size },
                kBloodUiTint);
        }
    }
    DrawChainBeamEffects();
    DrawBossTelegraphMarker();
    DrawChallengeMarkers();

    static std::vector<DrawEntry> drawEntries;
    drawEntries.clear();
    const size_t drawEntryReserve = m_enemies.size() + 1;
    if (drawEntries.capacity() < drawEntryReserve)
    {
        drawEntries.reserve(drawEntryReserve);
    }
    DirectX::XMFLOAT3 cam = m_pCamera->GetPos();

    if (m_pPlayer)
    {
        TRAN_INS;
        const DirectX::XMFLOAT3 playerPos = {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z
        };
        const float dx = playerPos.x - cam.x;
        const float dy = playerPos.y - cam.y;
        const float dz = playerPos.z - cam.z;
        drawEntries.push_back({ dx * dx + dy * dy + dz * dz, true, false, playerPos, tran.player.size, nullptr });
    }
    AddBossDrawEntry(drawEntries, cam);

    for (auto& slot : m_enemies)
    {
        if (!slot.enemy) continue;
        const Collision::Box enemyBox = slot.enemy->GetCollision();
        const DirectX::XMFLOAT3 enemyPos = enemyBox.center;
        const DirectX::XMFLOAT3 enemySize = slot.enemy->GetSize();
        const float dx = enemyPos.x - cam.x;
        const float dy = enemyPos.y - cam.y;
        const float dz = enemyPos.z - cam.z;
        drawEntries.push_back({ dx * dx + dy * dy + dz * dz, false, false, enemyPos, enemySize, slot.enemy });
    }

    std::sort(drawEntries.begin(), drawEntries.end(), [](const DrawEntry& a, const DrawEntry& b)
    {
        return a.distSq > b.distSq;
    });

    // Transparent-ish billboard sprites are drawn back-to-front for more natural overlap.
    for (const auto& entry : drawEntries)
    {
        float shadowScale = 1.0f;
        if (entry.isBoss && m_bossStompImpactTimer > 0.0f)
        {
            const float impactRate = Clamp01(m_bossStompImpactTimer / kBossStompImpactFxDuration);
            shadowScale = 1.0f + (kBossStompImpactShadowScale - 1.0f) * impactRate;
        }
        DrawShadow(m_pShadow, entry.pos, entry.size, shadowScale);
        if (entry.isPlayer)
        {
            if (m_pPlayer) m_pPlayer->Draw();
        }
        else if (entry.isBoss)
        {
            DrawBossEntry(entry);
        }
        else if (entry.enemy)
        {
            entry.enemy->Draw();
        }
    }
    DrawBossFallingObjects();
    DrawHitEffectEmitters();
    if (m_pPlayer)
    {
        m_pPlayer->DrawDirectionMarker();
    }
    DrawAmbushTargetMarker();

    if (!m_markerEffects.empty())
    {
        TRAN_INS;
        for (const auto& fx : m_markerEffects)
        {
            if (fx.timer <= 0.0f || fx.duration <= 0.0f) continue;
            const float t = Clamp01(fx.timer / fx.duration);
            const float growT = 1.0f + (1.0f - t) * (fx.growScale - 1.0f);
            Texture* effectTexture = fx.texture ? fx.texture : m_pAttackMarker;
            const bool isCombatEffect = (effectTexture == m_pCombatEffectTexture);
            DirectX::XMFLOAT4 color = fx.color;
            color.w *= isCombatEffect ? 1.0f : t;
            if (fx.billboard)
            {
                if (isCombatEffect)
                {
                    const float frameProgress = 1.0f - t;
                    int frameIndex = static_cast<int>(frameProgress * static_cast<float>(kCombatEffectFrameCount));
                    if (frameIndex < 0) frameIndex = 0;
                    if (frameIndex >= kCombatEffectFrameCount) frameIndex = kCombatEffectFrameCount - 1;
                    const DirectX::XMFLOAT2 uvScale =
                    {
                        1.0f / static_cast<float>(kCombatEffectColumns),
                        1.0f / static_cast<float>(kCombatEffectRows)
                    };
                    const DirectX::XMFLOAT2 uvPos =
                    {
                        uvScale.x * static_cast<float>(frameIndex % kCombatEffectColumns),
                        uvScale.y * static_cast<float>(frameIndex / kCombatEffectColumns)
                    };
                    DrawBillboardMarkerTintFrame(
                        effectTexture,
                        m_pCamera,
                        fx.pos,
                        { fx.size.x * growT, fx.size.y * growT, fx.size.z },
                        color,
                        uvPos,
                        uvScale);
                }
                else
                {
                    DrawBillboardMarkerTint(
                        effectTexture,
                        m_pCamera,
                        fx.pos,
                        { fx.size.x * growT, fx.size.y * growT, fx.size.z },
                        color);
                }
            }
            else
            {
                DrawAttackMarkerTint(
                    effectTexture,
                    fx.pos,
                    { fx.size.x * growT, fx.size.y, fx.size.z * growT },
                    color);
            }
        }
    }

    if (m_cameraMode == kCameraModeDebug)
    {
        TRAN_INS;
        // Debug mode overlays all important gameplay AABBs and attack ranges.
        Collision::Box playerBox{};
        playerBox.size = tran.player.size;
        playerBox.center = {
            tran.player.pos.x,
            tran.player.pos.y + tran.player.size.y * 0.5f,
            tran.player.pos.z
        };
        const float enemyAttackRangeMin = (tran.gameplay.enemyAttackRangeMin < 0.1f) ? 0.1f : tran.gameplay.enemyAttackRangeMin;
        const float enemyAttackRangeScale = (tran.gameplay.enemyAttackRangeScale < 0.1f) ? 0.1f : tran.gameplay.enemyAttackRangeScale;
        const bool showRangeDebug = true;
        if (m_pCameraGame)
        {
            AddCameraFrustumLines(*m_pCameraGame, { 1.0f, 1.0f, 0.0f, 1.0f });
        }

        AddAabbLines(playerBox, { 0.0f, 1.0f, 0.0f, 1.0f });

        for (const auto& slot : m_enemies)
        {
            if (!slot.enemy) continue;
            const Collision::Box enemyBox = slot.enemy->GetCollision();
            DirectX::XMFLOAT4 enemyAabbColor = kDebugEnemyColorOutRange;
            if (slot.hitFlashTimer > 0.0f)
            {
                enemyAabbColor = kDebugEnemyColorHit;
            }
            else if (slot.attackWindupTimer > 0.0f)
            {
                enemyAabbColor = kDebugEnemyColorWindup;
            }
            else if (slot.debugInAttackRange && slot.attackCooldownTimer <= 0.0f)
            {
                enemyAabbColor = kDebugEnemyColorReady;
            }
            else if (slot.debugInAttackRange)
            {
                enemyAabbColor = kDebugEnemyColorInRangeCooling;
            }
            AddAabbLines(enemyBox, enemyAabbColor);

            if (showRangeDebug)
            {
                float attackRange = slot.debugAttackRange;
                if (attackRange <= 0.0f)
                {
                    const float enemyRangeSize = (enemyBox.size.x > enemyBox.size.z) ? enemyBox.size.x : enemyBox.size.z;
                    const float playerRangeSize = (playerBox.size.x > playerBox.size.z) ? playerBox.size.x : playerBox.size.z;
                    attackRange = (enemyRangeSize + playerRangeSize) * enemyAttackRangeScale;
                    if (attackRange < enemyAttackRangeMin) attackRange = enemyAttackRangeMin;
                }

                Collision::Box attackRangeBox{};
                attackRangeBox.center = { enemyBox.center.x, playerBox.center.y, enemyBox.center.z };
                attackRangeBox.size = { attackRange * 2.0f, kDebugRangeBoxHeight, attackRange * 2.0f };
                const DirectX::XMFLOAT4 rangeColor = slot.debugInAttackRange ? kDebugRangeColorIn : kDebugRangeColorOut;
                AddAabbLines(attackRangeBox, rangeColor);
            }
        }

        for (const auto& shot : m_enemyProjectiles)
        {
            if (shot.life <= 0.0f) continue;
            Collision::Box shotBox{};
            shotBox.center = shot.pos;
            shotBox.size = { shot.radius * 2.0f, shot.radius * 2.0f, shot.radius * 2.0f };
            AddAabbLines(shotBox, { 0.25f, 0.95f, 1.0f, 1.0f });
        }

        if (m_attackActive)
        {
            Collision::Box attackBox{};
            attackBox.center = m_attackCenter;
            attackBox.size = m_attackSize;
            AddAabbLines(attackBox, { 1.0f, 0.0f, 0.0f, 1.0f });
        }
    }

    Geometory::DrawLines();

    if (m_pEnemyHpFrame && m_pEnemyHpGauge && m_pCamera)
    {
        // Heavy enemy counts throttle HP billboard draws to reduce per-frame cost.
        const int enemyCount = static_cast<int>(m_enemies.size());
        const bool reduceHpBillboardLoad = (enemyCount >= 10);
        const int hpStep = reduceHpBillboardLoad ? 2 : 1;
        const int hpStart = reduceHpBillboardLoad ? static_cast<int>(m_enemyPerfPhase & 1u) : 0;
        for (int i = hpStart; i < enemyCount; i += hpStep)
        {
            const auto& slot = m_enemies[i];
            if (!slot.enemy) continue;
            if (slot.enemy->GetType() != static_cast<int>(Enemy::Type::Tank)) continue;
            const Collision::Box enemyBox = slot.enemy->GetCollision();
            const DirectX::XMFLOAT3 headPos = {
                enemyBox.center.x,
                enemyBox.center.y + enemyBox.size.y * kEnemyHpBillboardOffsetScale,
                enemyBox.center.z
            };

            float maxHp = static_cast<float>(slot.enemy->GetMaxHp());
            float hp = static_cast<float>(slot.enemy->GetHp());
            if (maxHp <= 0.0f) maxHp = 1.0f;
            const float rate = Clamp01(hp / maxHp);
            const float burnRate = Clamp01((hp + MaxFloat(0.0f, slot.burnPool)) / maxHp);

            DrawEnemyHpGaugeBillboard(headPos, enemyBox.size, rate, burnRate, slot.chainCount);
        }
    }

    m_uiManager.Draw(UIObjectManager::Layer::Game);
    DrawSkillHud();
    DrawBloodStockHud();
    DrawBossHpUi();
}

/**
 * @brief プレイヤー HP ゲージの位置と残量表示を更新します。
 */
void SceneGame::UpdateHpGauge()
{
    // UI 未生成時は更新先がないため何もしません。
    if (!m_pHpGauge || !m_pHpFrame) return;

    TRAN_INS;
    float maxHp = tran.player.maxHp;
    float hp = tran.player.hp;
    if (maxHp <= 0.0f) maxHp = 1.0f;

    const float rate = Clamp01(hp / maxHp);
    const float gaugeWidth = kHpFrameWidth - kHpGaugePadding * 2.0f;
    const float gaugeHeight = kHpFrameHeight - kHpGaugePadding * 2.0f;
    const float gaugeLeft = kUiMargin + kHpGaugePadding;
    const float gaugeX = gaugeLeft + gaugeWidth * rate * 0.5f;
    const float gaugeY = kUiMargin + kHpFrameHeight * 0.5f;

    // ゲージは左詰め表示にするため、中心位置と UV 幅を残量に合わせて調整します。
    m_pHpGauge->SetPosition(gaugeX, gaugeY);
    m_pHpGauge->SetSize(gaugeWidth * rate, gaugeHeight);
    m_pHpGauge->SetUVPosition(0.0f, 0.0f);
    m_pHpGauge->SetUVScale(rate, 1.0f);
}

/**
 * @brief 攻撃・回避・スキルのクールタイムゲージを更新します。
 */
void SceneGame::UpdateCooldownGauges()
{
    float attackRate = 1.0f;
    if (m_weaponAttackStockMax > 1)
    {
        if (m_weaponAttackStock > 0)
        {
            attackRate = 1.0f;
        }
        else if (m_attackCooldownUiDuration > 0.0f)
        {
            attackRate = Clamp01(1.0f - (m_weaponAttackStockRechargeTimer / m_attackCooldownUiDuration));
        }
        else
        {
            attackRate = 0.0f;
        }
    }
    else if (m_attackCooldownUiDuration > 0.0f)
    {
        attackRate = Clamp01(1.0f - (m_attackCooldownUiTimer / m_attackCooldownUiDuration));
    }

    float evadeRate = 1.0f;
    if (m_pPlayer)
    {
        // 回避は Player 側の実効クールタイムを参照して進行率を出します。
        const float evadeDuration = m_pPlayer->GetEvadeCooldownDuration();
        const float evadeRemain = m_pPlayer->GetEvadeCooldownRemain();
        if (evadeDuration > 0.0f)
        {
            evadeRate = Clamp01(1.0f - (evadeRemain / evadeDuration));
        }
    }

    const bool hasSkill1 = (GetSkillTypeForSlot(0) != Transfer::RoguelikeUpgrade::ActionSkillNone);
    float skill1Rate = hasSkill1 ? 1.0f : 0.0f;
    if (hasSkill1)
    {
        if (GetSkillStockCountForSlot(0) > 0)
        {
            skill1Rate = 1.0f;
        }
        else if (m_skill1CooldownDuration > 0.0f)
        {
            skill1Rate = Clamp01(1.0f - (GetSkillRechargeTimerForSlot(0) / m_skill1CooldownDuration));
        }
        else
        {
            skill1Rate = 0.0f;
        }
    }

    const bool hasSkill2 = (GetSkillTypeForSlot(1) != Transfer::RoguelikeUpgrade::ActionSkillNone);
    float skill2Rate = hasSkill2 ? 1.0f : 0.0f;
    if (hasSkill2)
    {
        if (GetSkillStockCountForSlot(1) > 0)
        {
            skill2Rate = 1.0f;
        }
        else if (m_skill2CooldownDuration > 0.0f)
        {
            skill2Rate = Clamp01(1.0f - (GetSkillRechargeTimerForSlot(1) / m_skill2CooldownDuration));
        }
        else
        {
            skill2Rate = 0.0f;
        }
    }

    {
        TRAN_INS;
        // デバッグ UI からも見えるように、各ゲージ進行率を共有します。
        tran.gameplayDebug.cooldownRateAttack = attackRate;
        tran.gameplayDebug.cooldownRateEvade = evadeRate;
        tran.gameplayDebug.cooldownRateSkill1 = skill1Rate;
        tran.gameplayDebug.cooldownRateSkill2 = skill2Rate;
    }

    const float rates[CooldownSlotCount] = { attackRate, evadeRate, skill1Rate, skill2Rate };
    const float totalHeight =
        static_cast<float>(CooldownSlotCount) * kCooldownFrameHeight +
        static_cast<float>(CooldownSlotCount - 1) * kCooldownRowSpacing;
    const float frameX = static_cast<float>(SCREEN_WIDTH) - kUiMargin - kCooldownFrameWidth * 0.5f;
    const float startY = static_cast<float>(SCREEN_HEIGHT) - kUiMargin - totalHeight + kCooldownFrameHeight * 0.5f;
    const float gaugeWidth = kCooldownFrameWidth - kCooldownGaugePadding * 2.0f;
    const float gaugeHeight = kCooldownFrameHeight - kCooldownGaugePadding * 2.0f;

    for (int i = 0; i < CooldownSlotCount; ++i)
    {
        const float y = startY + static_cast<float>(i) * (kCooldownFrameHeight + kCooldownRowSpacing);
        if (m_pCooldownFrame[i])
        {
            m_pCooldownFrame[i]->SetPosition(frameX, y);
            m_pCooldownFrame[i]->SetSize(kCooldownFrameWidth, kCooldownFrameHeight);
        }
        if (m_pCooldownGauge[i])
        {
            // 各ゲージも HP ゲージと同様、左詰め表示になるよう中心を調整します。
            const float rate = Clamp01(rates[i]);
            const float gaugeLeft = frameX - kCooldownFrameWidth * 0.5f + kCooldownGaugePadding;
            const float gaugeX = gaugeLeft + gaugeWidth * rate * 0.5f;
            m_pCooldownGauge[i]->SetPosition(gaugeX, y);
            m_pCooldownGauge[i]->SetSize(gaugeWidth * rate, gaugeHeight);
            m_pCooldownGauge[i]->SetUVPosition(0.0f, 0.0f);
            m_pCooldownGauge[i]->SetUVScale(rate, 1.0f);
        }
    }
}

void SceneGame::DrawSkillHud() const
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl || !m_pBloodUiTexture || !m_pBloodUiTexture->GetResource())
    {
        return;
    }

    for (int slotIndex = 0; slotIndex < 2; ++slotIndex)
    {
        if (GetSkillTypeForSlot(slotIndex) != Transfer::RoguelikeUpgrade::ActionSkillBloodSlash)
        {
            continue;
        }

        UIObject* frame = m_pCooldownFrame[CooldownSkill1 + slotIndex];
        if (!frame) continue;

        const DirectX::XMFLOAT2 framePos = frame->GetPosition();
        const DirectX::XMFLOAT2 frameSize = frame->GetSize();
        const float iconSize = ClampRange(frameSize.y - 6.0f, 12.0f, 28.0f);
        const float left = framePos.x - frameSize.x * 0.5f + 4.0f;
        const float top = framePos.y - frameSize.y * 0.5f + 3.0f;
        const ImVec2 min(left, top);
        const ImVec2 max(left + iconSize, top + iconSize);
        dl->AddImage(
            reinterpret_cast<ImTextureID>(m_pBloodUiTexture->GetResource()),
            min,
            max,
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            IM_COL32(
                static_cast<int>(kBloodUiTint.x * 255.0f),
                static_cast<int>(kBloodUiTint.y * 255.0f),
                static_cast<int>(kBloodUiTint.z * 255.0f),
                static_cast<int>(kBloodUiTint.w * 255.0f)));
    }
}

void SceneGame::DrawAmbushTargetMarker() const
{
    if (!m_pCamera || !m_pDirectionMarkerTexture || !m_pDirectionMarkerTexture->GetResource())
    {
        return;
    }

    const bool hasAmbushSkill =
        (GetSkillTypeForSlot(0) == Transfer::RoguelikeUpgrade::ActionSkillAmbush) ||
        (GetSkillTypeForSlot(1) == Transfer::RoguelikeUpgrade::ActionSkillAmbush);
    if (!hasAmbushSkill)
    {
        return;
    }

    DirectX::XMFLOAT3 targetCenter{};
    DirectX::XMFLOAT3 targetSize{};
    if (!TryGetAmbushTarget(targetCenter, targetSize))
    {
        return;
    }

    const float baseSize = MaxFloat(targetSize.x, targetSize.z);
    const float markerWidth = ClampRange(baseSize * 0.85f, 0.45f, 1.10f);
    const float markerHeight = markerWidth;
    const DirectX::XMFLOAT3 markerPos = {
        targetCenter.x,
        targetCenter.y + targetSize.y * 0.5f + markerHeight * 0.20f,
        targetCenter.z
    };

    DrawBillboardMarkerTintFrame(
        m_pDirectionMarkerTexture,
        m_pCamera,
        markerPos,
        { markerWidth, markerHeight, markerWidth },
        { 1.0f, 1.0f, 1.0f, 0.95f },
        { 0.0f, 1.0f },
        { 1.0f, -1.0f });
}

void SceneGame::DrawBloodStockHud() const
{
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    if (!dl ||
        !m_pBloodStockOnTexture || !m_pBloodStockOnTexture->GetResource() ||
        !m_pBloodStockOffTexture || !m_pBloodStockOffTexture->GetResource() ||
        m_bloodStockMax <= 0)
    {
        return;
    }

    const float availableWidth = kBloodStockUiMaxWidth;
    const float iconSize = ClampRange(
        std::min(
            kBloodStockUiBaseSize,
            (availableWidth - kBloodStockUiSpacing * static_cast<float>(m_bloodStockMax - 1)) / static_cast<float>(m_bloodStockMax)),
        8.0f,
        kBloodStockUiBaseSize);
    const float totalWidth =
        static_cast<float>(m_bloodStockMax) * iconSize +
        static_cast<float>(m_bloodStockMax - 1) * kBloodStockUiSpacing;
    const float startX = kUiMargin + (kHpFrameWidth - totalWidth) * 0.5f;
    const float topY = kUiMargin + kHpFrameHeight + 8.0f;

    for (int i = 0; i < m_bloodStockMax; ++i)
    {
        Texture* texture = (i < m_bloodStock) ? m_pBloodStockOnTexture : m_pBloodStockOffTexture;
        if (!texture || !texture->GetResource()) continue;
        const float x = startX + static_cast<float>(i) * (iconSize + kBloodStockUiSpacing);
        dl->AddImage(
            reinterpret_cast<ImTextureID>(texture->GetResource()),
            ImVec2(x, topY),
            ImVec2(x + iconSize, topY + iconSize));
    }
}

void SceneGame::DrawChainBeamEffects() const
{
    if (!m_pChainLinkTexture || !m_pChainLinkTexture->GetResource() || !m_pCamera)
    {
        return;
    }

    for (const ChainBeamEffect& beam : m_chainBeamEffects)
    {
        if (beam.duration <= 0.0f || beam.timer <= 0.0f)
        {
            continue;
        }

        const float alpha = Clamp01(beam.timer / beam.duration);
        const float dx = beam.endPos.x - beam.startPos.x;
        const float dy = beam.endPos.y - beam.startPos.y;
        const float dz = beam.endPos.z - beam.startPos.z;
        const float length = std::sqrt(dx * dx + dy * dy + dz * dz);
        const int segmentCount = ClampInt(
            static_cast<int>(std::ceil(length / kChainBeamSegmentSpacing)) + 1,
            1,
            48);

        for (int i = 0; i < segmentCount; ++i)
        {
            const float t = (segmentCount <= 1) ? 0.0f : static_cast<float>(i) / static_cast<float>(segmentCount - 1);
            const DirectX::XMFLOAT3 pos = {
                beam.startPos.x + dx * t,
                beam.startPos.y + dy * t,
                beam.startPos.z + dz * t
            };
            DrawBillboardMarkerTint(
                m_pChainLinkTexture,
                m_pCamera,
                pos,
                { kChainBeamSegmentSize, kChainBeamSegmentSize, kChainBeamSegmentSize },
                { kChainBeamTint.x, kChainBeamTint.y, kChainBeamTint.z, kChainBeamTint.w * alpha });
        }
    }
}

void SceneGame::DrawClearPortal() const
{
    if (m_clearPortalMode == ClearPortalNone || m_clearPortalSpawnTimer > 0.0f)
    {
        return;
    }

    const float pulse = 0.5f + 0.5f * std::sin(m_clearPortalPulseTimer * 4.8f);
    const float floorRadius = 1.10f + pulse * 0.20f;
    const float coreSize = 0.80f + pulse * 0.16f;
    const DirectX::XMFLOAT3 floorPos = { m_clearPortalPos.x, 0.001f, m_clearPortalPos.z };
    const DirectX::XMFLOAT4 floorColor = { 0.22f, 0.86f, 1.0f, 0.90f };
    const DirectX::XMFLOAT4 coreColor = { 0.82f, 0.96f, 1.0f, 0.88f };
    Texture* floorTexture = m_pBossAttackRangeMarker ? m_pBossAttackRangeMarker : m_pAttackMarker;
    if (floorTexture)
    {
        DrawAttackMarkerTint(
            floorTexture,
            floorPos,
            { floorRadius, 0.05f, floorRadius },
            floorColor);
    }

    if (m_pAttackMarker && m_pCamera)
    {
        DrawBillboardMarkerTint(
            m_pAttackMarker,
            m_pCamera,
            { m_clearPortalPos.x, 0.15f, m_clearPortalPos.z },
            { coreSize, coreSize, coreSize },
            coreColor);
    }
}

/**
 * @brief 敵の頭上にビルボード HP ゲージを描画します。
 * @param headPos 頭上基準位置です。
 * @param enemySize 敵サイズです。
 * @param hpRate 残 HP 比率です。
 * @param burnRate 燃焼分込みの表示比率です。
 * @param chainCount 鎖本数です。
 */
void SceneGame::DrawEnemyHpGaugeBillboard(const DirectX::XMFLOAT3& headPos,
                                          const DirectX::XMFLOAT3& enemySize,
                                          float hpRate,
                                          float burnRate,
                                          int chainCount)
{
    // 必要な描画リソースかカメラが欠けている場合は描画しません。
    if (!m_pEnemyHpGauge || !m_pEnemyHpFrame || !m_pCamera) return;

    const float clampedRate = Clamp01(hpRate);
    const float clampedBurnRate = ClampRange(burnRate, clampedRate, 1.0f);
    float width = enemySize.x * kEnemyHpBillboardWidthScale;
    float height = enemySize.y * kEnemyHpBillboardHeightScale;
    if (width < kEnemyHpBillboardMinWidth) width = kEnemyHpBillboardMinWidth;
    if (height < kEnemyHpBillboardMinHeight) height = kEnemyHpBillboardMinHeight;

    // フレームとゲージの余白を敵サイズ比から決めます。
    const float padding = height * kEnemyHpBillboardPaddingScale;
    const float gaugeWidth = width - padding * 2.0f;
    const float gaugeHeight = height - padding * 2.0f;

    DirectX::XMFLOAT3 camPos = m_pCamera->GetPos();
    DirectX::XMFLOAT3 camLook = m_pCamera->GetLook();
    DirectX::XMVECTOR camV = DirectX::XMLoadFloat3(&camPos);
    DirectX::XMVECTOR lookV = DirectX::XMLoadFloat3(&camLook);
    DirectX::XMVECTOR forward = DirectX::XMVectorSubtract(lookV, camV);
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(forward)) < 1.0e-6f)
    {
        // カメラ向きが壊れている場合は既定の前方で代用します。
        forward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }
    else
    {
        forward = DirectX::XMVector3Normalize(forward);
    }

    DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    DirectX::XMVECTOR right = DirectX::XMVector3Cross(up, forward);
    if (DirectX::XMVectorGetX(DirectX::XMVector3LengthSq(right)) < 1.0e-6f)
    {
        // 真上/真下に近い視線でも右軸を作れるよう、Up を差し替えます。
        up = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        right = DirectX::XMVector3Cross(up, forward);
    }
    right = DirectX::XMVector3Normalize(right);
    up = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(forward, right));

    DirectX::XMFLOAT3 rightAxis;
    DirectX::XMStoreFloat3(&rightAxis, right);

    // 残量が減っても左端が固定されるよう、ゲージ中心だけを横へずらします。
    const float gaugeCenterShift = (clampedRate - 1.0f) * gaugeWidth * 0.5f;
    DirectX::XMFLOAT3 gaugePos = {
        headPos.x + rightAxis.x * gaugeCenterShift,
        headPos.y + rightAxis.y * gaugeCenterShift,
        headPos.z + rightAxis.z * gaugeCenterShift
    };

    DirectX::XMMATRIX R(right, up, forward, DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f));

    DirectX::XMFLOAT4X4 world;
    DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(R * DirectX::XMMatrixTranslation(headPos.x, headPos.y, headPos.z)));
    Sprite::SetWorld(world);
    Sprite::SetSize({ width, height });
    Sprite::SetOffset({ 0.0f, 0.0f });
    Sprite::SetUVPos({ 0.0f, 0.0f });
    Sprite::SetUVScale({ 1.0f, 1.0f });
    Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    Sprite::SetTexture(m_pEnemyHpFrame);
    Sprite::Draw();

    if (clampedBurnRate > clampedRate)
    {
        DirectX::XMFLOAT3 burnPos = {
            headPos.x + rightAxis.x * ((clampedBurnRate - 1.0f) * gaugeWidth * 0.5f),
            headPos.y + rightAxis.y * ((clampedBurnRate - 1.0f) * gaugeWidth * 0.5f),
            headPos.z + rightAxis.z * ((clampedBurnRate - 1.0f) * gaugeWidth * 0.5f)
        };
        DirectX::XMFLOAT4X4 burnWorld;
        DirectX::XMStoreFloat4x4(&burnWorld, DirectX::XMMatrixTranspose(R * DirectX::XMMatrixTranslation(burnPos.x, burnPos.y, burnPos.z)));
        Sprite::SetWorld(burnWorld);
        Sprite::SetSize({ gaugeWidth * clampedBurnRate, gaugeHeight });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos({ 0.0f, 0.0f });
        Sprite::SetUVScale({ clampedBurnRate, 1.0f });
        Sprite::SetColor(kBurnGaugeColor);
        Sprite::SetTexture(m_pEnemyHpGauge);
        Sprite::Draw();
    }

    if (clampedRate > 0.0f)
    {
        // 残量 0 より大きい時だけ中身ゲージを重ねます。
        DirectX::XMFLOAT4X4 gaugeWorld;
        DirectX::XMStoreFloat4x4(&gaugeWorld, DirectX::XMMatrixTranspose(R * DirectX::XMMatrixTranslation(gaugePos.x, gaugePos.y, gaugePos.z)));
        Sprite::SetWorld(gaugeWorld);
        Sprite::SetSize({ gaugeWidth * clampedRate, gaugeHeight });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVPos({ 0.0f, 0.0f });
        Sprite::SetUVScale({ clampedRate, 1.0f });
        Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        Sprite::SetTexture(m_pEnemyHpGauge);
        Sprite::Draw();
    }

    if (m_pChainUiTexture && chainCount > 0)
    {
        const float bindRate = Clamp01(static_cast<float>(chainCount) * 0.05f);
        const float chainWidth = gaugeWidth * bindRate;
        if (chainWidth > 0.0f)
        {
            const float xShift = (gaugeWidth - chainWidth) * 0.5f;
            const DirectX::XMFLOAT3 iconPos = {
                headPos.x + rightAxis.x * xShift,
                headPos.y + rightAxis.y * xShift,
                headPos.z + rightAxis.z * xShift
            };
            DirectX::XMFLOAT4X4 chainWorld;
            DirectX::XMStoreFloat4x4(&chainWorld, DirectX::XMMatrixTranspose(R * DirectX::XMMatrixTranslation(iconPos.x, iconPos.y, iconPos.z)));
            Sprite::SetWorld(chainWorld);
            Sprite::SetSize({ chainWidth, gaugeHeight });
            Sprite::SetOffset({ 0.0f, 0.0f });
            Sprite::SetUVPos({ 0.0f, 0.0f });
            Sprite::SetUVScale({ 1.0f, 1.0f });
            Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 0.95f });
            Sprite::SetTexture(m_pChainUiTexture);
            Sprite::Draw();
        }
    }
}
