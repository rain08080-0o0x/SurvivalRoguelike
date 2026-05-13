#include "Enemy.h"
#include <cmath>
#include <cstdlib>

namespace
{
    // Enemy defaults used for type presets and simple AI timing.
    const char* kEnemyTexture = "Assets/Texture/Chracter/genbaneko.png";
    const int kDefaultEnemyHp = 3;
    const float kDefaultStageSize = 5.0f;
    const float kDefaultMoveSpeed = 1.2f;
    const float kDefaultMoveSpeedScale = 1.0f;
    const float kMoveDt = 1.0f / 60.0f;
    const float kChaseStartRatio = 0.45f;
    const float kChaseEndRatio = 0.60f;
    const float kStopDistanceMin = 0.15f;
    const float kStopDistanceRange = 0.40f;
    const float kWanderSpeedScale = 0.50f;
    const float kWanderReachEps = 0.15f;
    const float kWanderTimerMin = 0.40f;
    const float kWanderTimerMax = 1.20f;

    /**
     * @brief 指定値を範囲内に丸めます。
     * @param v 補正対象の値です。
     * @param lo 下限値です。
     * @param hi 上限値です。
     * @return lo 以上 hi 以下に収めた値です。
     */
    float ClampFloat(float v, float lo, float hi)
    {
        // ステージ外へはみ出さないように範囲外の値を固定します。
        if (v < lo) return lo;
        if (v > hi) return hi;
        return v;
    }

    /**
     * @brief 0.0 から 1.0 の乱数を返します。
     * @return 正規化済み乱数です。
     */
    float Rand01()
    {
        return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    }

    /**
     * @brief 指定範囲の乱数を返します。
     * @param minV 最小値です。
     * @param maxV 最大値です。
     * @return minV 以上 maxV 以下の乱数です。
     */
    float RandRange(float minV, float maxV)
    {
        return minV + (maxV - minV) * Rand01();
    }
}

/**
 * @brief 敵を生成し、初期ステータスと描画リソースを準備します。
 */
Enemy::Enemy()
    : m_pTexture(nullptr)
    , m_size(0.5f, 1.0f, 0.5f)
    , m_color(1.0f, 0.25f, 0.25f, 1.0f)
    , m_moveSpeed(kDefaultMoveSpeed)
    , m_moveSpeedBase(kDefaultMoveSpeed)
    , m_moveSpeedScale(kDefaultMoveSpeedScale)
    , m_stageSize(kDefaultStageSize)
    , m_moveDirX(1.0f)
    , m_hp(kDefaultEnemyHp)
    , m_maxHp(kDefaultEnemyHp)
    , m_type(Type::Speed)
    , m_attackRangeScale(1.0f)
    , m_attackWindupScale(1.0f)
    , m_attackCooldownScale(1.0f)
    , m_attackDamageScale(1.0f)
    , m_targetPos({2,0,0})
    , m_wanderTarget(0.0f, 0.0f, 0.0f)
    , m_wanderTimer(0.0f)
    , m_state(MoveState::Wander)
    , m_forceChase(false)
{
    // 生成直後は原点から開始し、初期種別の補正値をまとめて適用します。
    m_pos = { 0.0f, 0.0f, 0.0f };
    SetType(Type::Speed);

    // 敵の見た目を描画するテクスチャを読み込みます。
    m_pTexture = new Texture();
    if (FAILED(m_pTexture->Create(kEnemyTexture)))
    {
        MessageBox(NULL, "Texture load failed.\nEnemy.cpp", "Error", MB_OK);
    }
}

/**
 * @brief 敵が保持する描画リソースを解放します。
 */
Enemy::~Enemy()
{
    if (m_pTexture)
    {
        delete m_pTexture;
        m_pTexture = nullptr;
    }
}

/**
 * @brief 徘徊と追跡を切り替えながら、敵を 1 フレーム分移動させます。
 */
void Enemy::Update()
{
    // ステージサイズと敵サイズから、移動可能な最小/最大座標を求めます。
    const float stage = (m_stageSize > 0.0f) ? m_stageSize : kDefaultStageSize;
    const float half = stage * 0.5f;
    const float halfX = m_size.x * 0.5f;
    const float halfZ = m_size.z * 0.5f;

    float minX = -half + halfX;
    float maxX = half - halfX;
    float minZ = -half + halfZ;
    float maxZ = half - halfZ;
    if (minX > maxX) { minX = 0.0f; maxX = 0.0f; }
    if (minZ > maxZ) { minZ = 0.0f; maxZ = 0.0f; }

    // プレイヤーが近いと追跡、十分離れたら徘徊へ戻すためのヒステリシスです。
    const float chaseStart = stage * kChaseStartRatio;
    const float chaseEnd = stage * kChaseEndRatio;

    // 対象までの距離を毎フレーム測り、AI 状態判定に使います。
    const float toTargetX = m_targetPos.x - m_pos.x;
    const float toTargetZ = m_targetPos.z - m_pos.z;
    const float targetDistSq = toTargetX * toTargetX + toTargetZ * toTargetZ;

    if (m_forceChase)
    {
        m_state = MoveState::Chase;
    }
    else if (m_state == MoveState::Wander)
    {
        // 徘徊中に十分近づかれたら追跡へ切り替えます。
        if (targetDistSq <= chaseStart * chaseStart)
        {
            m_state = MoveState::Chase;
        }
    }
    else
    {
        // 追跡中でも十分離れたら徘徊へ戻し、徘徊目標は再抽選させます。
        if (targetDistSq >= chaseEnd * chaseEnd)
        {
            m_state = MoveState::Wander;
            m_wanderTimer = 0.0f;
        }
    }

    if (m_state == MoveState::Chase)
    {
        // 追跡中は対象へ直進しつつ、近づき過ぎたら減速して密着し過ぎないようにします。
        const float dist = std::sqrt(targetDistSq);
        if (dist > 0.0001f)
        {
            float stopDist = (m_size.x > m_size.z) ? m_size.x : m_size.z;
            stopDist *= 0.6f;
            if (stopDist < kStopDistanceMin) stopDist = kStopDistanceMin;

            if (dist > stopDist)
            {
                float speedScale = 1.0f;
                if (dist < stopDist + kStopDistanceRange)
                {
                    // 停止距離手前では段階的に減速し、急停止を避けます。
                    speedScale = (dist - stopDist) / kStopDistanceRange;
                }
                m_pos.x += (toTargetX / dist) * m_moveSpeed * speedScale * kMoveDt;
                m_pos.z += (toTargetZ / dist) * m_moveSpeed * speedScale * kMoveDt;
            }
        }
    }
    else
    {
        // 徘徊中は時間経過で目標を更新し、ランダム地点へゆっくり移動します。
        m_wanderTimer -= kMoveDt;

        float toWanderX = m_wanderTarget.x - m_pos.x;
        float toWanderZ = m_wanderTarget.z - m_pos.z;
        float wanderDistSq = toWanderX * toWanderX + toWanderZ * toWanderZ;

        if (m_wanderTimer <= 0.0f || wanderDistSq <= kWanderReachEps * kWanderReachEps)
        {
            // 時間切れまたは到達時に、新しい徘徊先と次回再抽選時間を決めます。
            float targetX = (minX == maxX) ? minX : RandRange(minX, maxX);
            float targetZ = (minZ == maxZ) ? minZ : RandRange(minZ, maxZ);
            m_wanderTarget = { targetX, 0.0f, targetZ };
            m_wanderTimer = RandRange(kWanderTimerMin, kWanderTimerMax);

            toWanderX = m_wanderTarget.x - m_pos.x;
            toWanderZ = m_wanderTarget.z - m_pos.z;
            wanderDistSq = toWanderX * toWanderX + toWanderZ * toWanderZ;
        }

        const float dist = std::sqrt(wanderDistSq);
        if (dist > 0.0001f)
        {
            // 徘徊中は追跡より遅い速度で移動し、メリハリを付けます。
            const float speed = m_moveSpeed * kWanderSpeedScale;
            m_pos.x += (toWanderX / dist) * speed * kMoveDt;
            m_pos.z += (toWanderZ / dist) * speed * kMoveDt;
        }
    }

    // 最終的な座標は必ずステージ内へ収め、地面上に固定します。
    m_pos.x = ClampFloat(m_pos.x, minX, maxX);
    m_pos.z = ClampFloat(m_pos.z, minZ, maxZ);
    m_pos.y = 0.0f;
}

/**
 * @brief 敵をビルボードスプライトとして描画します。
 */
void Enemy::Draw()
{
    // テクスチャ未初期化時は描画できないため処理しません。
    if (!m_pTexture) return;
    using namespace DirectX;

    // 敵もカメラ正面を向くため、プレイヤーと同じくビルボード回転を組み立てます。
    XMMATRIX billboard = XMMatrixIdentity();

    if (m_pCamera)
    {
        // View 行列からカメラの向きだけを取り出します。
        XMFLOAT4X4 viewFloat;
        viewFloat = m_pCamera->GetViewMatrix(false);

        XMMATRIX viewMat = XMLoadFloat4x4(&viewFloat);

        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);

        XMFLOAT4X4 invViewFloat;
        XMStoreFloat4x4(&invViewFloat, invView);

        // 位置は別途平行移動で与えるので、ここでは回転のみ残します。
        invViewFloat._41 = 0.0f;
        invViewFloat._42 = 0.0f;
        invViewFloat._43 = 0.0f;

        billboard = XMLoadFloat4x4(&invViewFloat);
    }

    // 足元を基準に配置するため、半身分上げた位置へ描画します。
    DirectX::XMMATRIX T = billboard * DirectX::XMMatrixTranslation(
        m_pos.x,
        m_pos.y + (m_size.y * 0.5f),
        m_pos.z
    );
    DirectX::XMFLOAT4X4 world;
    DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(T));

    Sprite::SetWorld(world);
    Sprite::SetSize({ m_size.x, m_size.y });
    Sprite::SetOffset({ 0.0f, 0.0f });
    Sprite::SetUVPos({ 0.0f, 0.0f });
    Sprite::SetUVScale({ 1.0f, 1.0f });
    Sprite::SetColor(m_color);
    Sprite::SetTexture(m_pTexture);
    Sprite::Draw();
}

/**
 * @brief 敵サイズを更新します。
 * @param size 新しいサイズです。
 */
void Enemy::SetSize(const DirectX::XMFLOAT3& size)
{
    m_size = size;
}

/**
 * @brief ステージサイズを更新します。
 * @param size ステージ一辺の長さです。
 */
void Enemy::SetStageSize(float size)
{
    m_stageSize = size;
}

/**
 * @brief 基準移動速度を更新し、現在速度へ反映します。
 * @param speed タイプ補正前の速度です。
 */
void Enemy::SetMoveSpeed(float speed)
{
    // 基準値を保存し、種別倍率込みの実効速度へ再計算します。
    m_moveSpeedBase = speed;
    m_moveSpeed = m_moveSpeedBase * m_moveSpeedScale;
}

/**
 * @brief 敵種別を切り替え、種別ごとの能力と色を設定します。
 * @param type 適用する敵種別です。
 */
void Enemy::SetType(Type type)
{
    m_type = type;
    switch (m_type)
    {
    case Type::Speed:
        // 手数重視: 低耐久だが素早く、攻撃テンポも速くします。
        m_maxHp = 2;
        m_hp = m_maxHp;
        m_moveSpeedScale = 1.45f;
        m_attackRangeScale = 0.95f;
        m_attackWindupScale = 0.75f;
        m_attackCooldownScale = 0.75f;
        m_attackDamageScale = 0.85f;
        m_color = { 1.0f, 0.50f, 0.25f, 1.0f };
        break;
    case Type::Tank:
        // 重量型: 耐久と火力を上げる代わりに足を遅くします。
        m_maxHp = 6;
        m_hp = m_maxHp;
        m_moveSpeedScale = 0.75f;
        m_attackRangeScale = 0.90f;
        m_attackWindupScale = 1.20f;
        m_attackCooldownScale = 1.10f;
        m_attackDamageScale = 1.40f;
        m_color = { 0.35f, 0.60f, 1.0f, 1.0f };
        break;
    case Type::Ranged:
    default:
        // 遠距離型: 射程を伸ばしつつ、火力は控えめにします。
        m_maxHp = 3;
        m_hp = m_maxHp;
        m_moveSpeedScale = 0.95f;
        m_attackRangeScale = 1.80f;
        m_attackWindupScale = 1.05f;
        m_attackCooldownScale = 1.25f;
        m_attackDamageScale = 0.75f;
        m_color = { 0.45f, 1.0f, 0.45f, 1.0f };
        break;
    }
    // 種別変更後は基準速度に倍率を掛け直し、移動挙動を即反映します。
    m_moveSpeed = m_moveSpeedBase * m_moveSpeedScale;
}

/**
 * @brief 最大 HP を倍率で再計算し、現在 HP も比率維持で補正します。
 * @param scale 最大 HP に掛ける倍率です。
 */
void Enemy::SetHpScale(float scale)
{
    // 極端な低倍率で最大 HP が 0 付近にならないように下限を設けます。
    if (scale < 0.1f) scale = 0.1f;

    // 変更前の HP 比率を維持し、強化後も減り具合が極端に変わらないようにします。
    const float hpRate = (m_maxHp > 0)
        ? static_cast<float>(m_hp) / static_cast<float>(m_maxHp)
        : 1.0f;
    const int scaledMaxHp = static_cast<int>(std::ceil(static_cast<float>(m_maxHp) * scale));
    m_maxHp = (scaledMaxHp > 0) ? scaledMaxHp : 1;
    m_hp = static_cast<int>(std::ceil(hpRate * static_cast<float>(m_maxHp)));

    // 丸め結果で範囲外になった場合に備えて補正します。
    if (m_hp < 0) m_hp = 0;
    if (m_hp > m_maxHp) m_hp = m_maxHp;
}

/**
 * @brief 現在サイズを返します。
 * @return 敵サイズです。
 */
DirectX::XMFLOAT3 Enemy::GetSize() const
{
    return m_size;
}

/**
 * @brief 現在位置とサイズから AABB を組み立てます。
 * @return 衝突判定用ボックスです。
 */
Collision::Box Enemy::GetCollision() const
{
    Collision::Box box{};

    // 判定サイズは見た目サイズに合わせ、中心は足元基準から半身上へずらします。
    box.size = m_size;
    box.center = {
        m_pos.x,
        m_pos.y + m_size.y * 0.5f,
        m_pos.z
    };
    return box;
}

/**
 * @brief 敵 HP を減算します。
 * @param amount 減らす HP 値です。
 */
void Enemy::Damage(int amount)
{
    // ダメージ無効値や既に撃破済みの個体には何もしません。
    if (amount <= 0 || m_hp <= 0) return;
    m_hp -= amount;
    // 0 未満にならないように補正します。
    if (m_hp < 0) m_hp = 0;
}

/**
 * @brief 生存判定を返します。
 * @return HP が 1 以上なら true です。
 */
bool Enemy::IsAlive() const
{
    return m_hp > 0;
}

/**
 * @brief 現在 HP を返します。
 * @return 現在 HP です。
 */
int Enemy::GetHp() const
{
    return m_hp;
}

/**
 * @brief 最大 HP を返します。
 * @return 最大 HP です。
 */
int Enemy::GetMaxHp() const
{
    return m_maxHp;
}

/**
 * @brief 現在の移動状態を数値化して返します。
 * @return Chase 中は 1、Wander 中は 0 です。
 */
int Enemy::GetState() const
{
    return (m_state == MoveState::Chase) ? 1 : 0;
}

/**
 * @brief 現在の敵種別を数値で返します。
 * @return Type の列挙値を int へ変換した値です。
 */
int Enemy::GetType() const
{
    return static_cast<int>(m_type);
}

/**
 * @brief 攻撃射程倍率を返します。
 * @return 射程倍率です。
 */
float Enemy::GetAttackRangeScale() const
{
    return m_attackRangeScale;
}

/**
 * @brief 攻撃予兆倍率を返します。
 * @return 予兆倍率です。
 */
float Enemy::GetAttackWindupScale() const
{
    return m_attackWindupScale;
}

/**
 * @brief 攻撃クールタイム倍率を返します。
 * @return クールタイム倍率です。
 */
float Enemy::GetAttackCooldownScale() const
{
    return m_attackCooldownScale;
}

/**
 * @brief 攻撃ダメージ倍率を返します。
 * @return ダメージ倍率です。
 */
float Enemy::GetAttackDamageScale() const
{
    return m_attackDamageScale;
}


/**
 * @brief 描画に使うカメラを設定します。
 * @param camera 新しく参照するカメラです。
 */
void Enemy::SetCamera(Camera* camera)
{
    m_pCamera = camera;
}

/**
 * @brief 追跡対象の座標を更新します。
 * @param targetPos 追跡先のワールド座標です。
 */
void Enemy::SetTargetPos(DirectX::XMFLOAT3 targetPos)
{
    m_targetPos = targetPos;
}

void Enemy::SetForceChase(bool forceChase)
{
    m_forceChase = forceChase;
    if (m_forceChase)
    {
        m_state = MoveState::Chase;
    }
}
