#include "Player.h"
#include "Input.h"
#include "Transfer.h"
#include <cmath>
#include "Defines.h"
#include "DirectX.h"

namespace
{
    // Player defaults used when Transfer does not provide an override yet.
    const char* kPlayerTexture = "Assets/Texture/Chracter/genbaneko.png";
    const char* kDirectionTexture = "Assets/Texture/Chracter/triangle.png";
    const float kDefaultStageSize = 5.0f;
    const float kDefaultMoveSpeed = 2.4f;
    const float kDefaultMaxHp = 100.0f;
    const float kDefaultDashDistance = 1.4f;
    const float kDefaultDashCooldown = 0.4f;
    const float kDefaultDashDuration = 0.50f;
    const float kMoveDt = 1.0f / 60.0f;

    /**
     * @brief 指定値を下限と上限の範囲に収めます。
     * @param v 補正対象の値です。
     * @param lo 許可する最小値です。
     * @param hi 許可する最大値です。
     * @return lo 以上 hi 以下に丸めた値を返します。
     */
    float ClampFloat(float v, float lo, float hi)
    {
        // 下限を下回る値は、そのまま使うと範囲外になるため最小値へ固定します。
        if (v < lo) return lo;
        // 上限を超える値も同様に最大値へ固定します。
        if (v > hi) return hi;
        // 範囲内なら補正せず、そのまま返します。
        return v;
    }

    float EvaluateDashTravel01(float t)
    {
        const float clamped = ClampFloat(t, 0.0f, 1.0f);
        const float inv = 1.0f - clamped;
        // 回避距離を前半へ寄せて、踏み込みの加速感を強くします。
        return 1.0f - inv * inv * inv * inv;
    }
}


/**
 * @brief プレイヤー生成時に各種初期値と描画リソースを設定します。
 * @param camera ビルボード計算とトレイル描画に使うカメラです。
 */
Player::Player(Camera*camera)
    : m_pTexture(nullptr)
    , m_pDirectionTexture(nullptr)
    , m_size(0.5f, 1.0f, 0.5f)
    , m_velocity(0.0f, 0.0f, 0.0f)
    , m_facingDir(0.0f, 0.0f, 1.0f)
    , m_color(1.0f, 1.0f, 1.0f, 1.0f)
    , m_hp(kDefaultMaxHp)
    , m_maxHp(kDefaultMaxHp)
    , m_moveSpeed(kDefaultMoveSpeed)
    , m_dashDistance(kDefaultDashDistance)
    , m_dashCooldown(kDefaultDashCooldown)
    , m_dashDuration(kDefaultDashDuration)
    , m_dashTimer(0.0f)
    , m_dashCooldownTimer(0.0f)
    , m_effectiveDashCooldown(kDefaultDashCooldown)
    , m_dashDir(0.0f, 0.0f, 1.0f)
    , m_isDashing(false)
    , m_stageSize(kDefaultStageSize)
    , m_pTrail(new TrailEffect(this))
    , m_pCamera(camera)
    , m_pTrailEffectTexture(nullptr)
{
    // トレイルは一定本数の履歴を持たせ、移動直後から描画できる状態にしておきます。
    m_pTrail->AddLine(20);
    // ステージ中央を初期位置にして、ゲーム開始時の参照位置を固定します。
    m_pos = { 0.0f, 0.0f, 0.0f };

    // プレイヤー本体の見た目を描画するテクスチャを読み込みます。
    m_pTexture = new Texture();
    if (FAILED(m_pTexture->Create(kPlayerTexture)))
    {
        MessageBox(NULL, "Texture load failed.\nPlayer.cpp", "Error", MB_OK);
    }

    // 進行方向の視認性を上げるため、専用マーカーも別テクスチャで持ちます。
    m_pDirectionTexture = new Texture();
    if (FAILED(m_pDirectionTexture->Create(kDirectionTexture)))
    {
        MessageBox(NULL, "Texture load failed.\ntriangle.png", "Error", MB_OK);
    }

    // トレイルはプレイヤー画像を流用して残像として扱います。
    m_pTrailEffectTexture = new Texture();
    if (FAILED(m_pTrailEffectTexture->Create("Assets/Texture/Chracter/genbaneko.png")))
    {
        MessageBox(NULL, "Texture load failed.\nPlayer.cpp", "Error", MB_OK);
    }

    {
        TRAN_INS;
        if (tran.player.maxHp > 0.0f)
        {
            m_hp = ClampFloat(tran.player.hp, 0.0f, tran.player.maxHp);
            m_maxHp = tran.player.maxHp;
        }
    }

    // 初期化直後の状態を Transfer へ反映し、他オブジェクトから参照できるようにします。
    SyncToTransfer();
}

/**
 * @brief Player が保持する動的リソースを安全に破棄します。
 */
Player::~Player()
{
    // 生成した順序に依存せず、存在するものだけ破棄して二重解放を防ぎます。
    if (m_pTrail)
    {
        delete m_pTrail;
        m_pTrail = nullptr;
    }
    if (m_pTexture)
    {
        delete m_pTexture;
        m_pTexture = nullptr;
    }
    if (m_pDirectionTexture)
    {
        delete m_pDirectionTexture;
        m_pDirectionTexture = nullptr;
    }
    if (m_pTrailEffectTexture)
    {
        delete m_pTrailEffectTexture;
        m_pTrailEffectTexture = nullptr;
    }
}

/**
 * @brief プレイヤーの入力処理、移動、境界補正、Transfer 同期を 1 フレーム分進めます。
 */
void Player::Update()
{
    // 先に共有状態を取り込み、外部調整値や直前フレーム結果を更新対象へ反映します。
    SyncFromTransfer();

    // 入力に応じた移動と回避を処理します。
    ApplyMovement(kMoveDt);

    // 計算後の位置がステージ外へ出ないように補正します。
    ClampToStage();

#ifdef _DEBUG
    // デバッグ中は H キーで HP を増やして確認しやすくします。
    if (IsKeyTrigger('H'))
        m_hp += 1.0f;

    // デバッグ操作を含めても HP が負値にならないように切り上げます。
    if (m_hp < 0.0f)m_hp = 0;
#endif

    // 反映済みの結果を共有状態へ返し、UI や他オブジェクトが読めるようにします。
    SyncToTransfer();

    // トレイルは位置更新後の座標を使って残像を進めます。
    m_pTrail->Update();
}

/**
 * @brief プレイヤー本体スプライトとトレイルを描画します。
 */
void Player::Draw()
{
    // テクスチャ未読込時は描画できないため、安全に処理を抜けます。
    if (!m_pTexture) return;

    using namespace DirectX;
    // プレイヤーは常にカメラ正面を向くため、ビルボード行列を構築します。
    XMMATRIX billboard = XMMatrixIdentity();

    if (m_pCamera)
    {
        // 転置していない View 行列を取得し、画面向きの回転だけを取り出します。
        XMFLOAT4X4 viewFloat;
        viewFloat = m_pCamera->GetViewMatrix(false);

        // 行列計算用へ変換します。
        XMMATRIX viewMat = XMLoadFloat4x4(&viewFloat);

        // 逆行列にすることで、カメラの向きと逆向きの回転を得ます。
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);

        // 平行移動成分を消すため、一度書き戻して成分を直接編集します。
        XMFLOAT4X4 invViewFloat;
        XMStoreFloat4x4(&invViewFloat, invView);

        // ビルボードは回転だけ必要なので移動成分は無効化します。
        invViewFloat._41 = 0.0f;
        invViewFloat._42 = 0.0f;
        invViewFloat._43 = 0.0f;

        // 最終的に描画へ渡す回転行列として読み込み直します。
        billboard = XMLoadFloat4x4(&invViewFloat);
    }

    // プレイヤー中心が地面に埋まらないよう、半身分だけ上げてワールド行列を作ります。
    DirectX::XMMATRIX T = billboard *
        DirectX::XMMatrixTranslation(
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


    if (m_pTrail)
    {
        // トレイルはプレイヤーと同じカメラ行列を使い、追従して描画します。
        m_pTrail->SetView(m_pCamera->GetViewMatrix());
        m_pTrail->SetProjection(m_pCamera->GetProjectionMatrix());
        m_pTrail->Draw();
    }
}

/**
 * @brief プレイヤーの前方を示す地面マーカーを描画します。
 */
void Player::DrawDirectionMarker()
{
    // マーカー用テクスチャが無い場合は描画を行えません。
    if (!m_pDirectionTexture) return;

    TRAN_INS;
    const float dirLenSq = m_facingDir.x * m_facingDir.x + m_facingDir.z * m_facingDir.z;

    // 向きベクトルがほぼゼロなら向きが定義できないため描画しません。
    if (dirLenSq <= 1.0e-6f) return;

    // 向きベクトルを正規化し、マーカーの回転角と前方配置位置を決めます。
    const float dirLen = sqrtf(dirLenSq);
    const DirectX::XMFLOAT3 dir = { m_facingDir.x / dirLen, 0.0f, m_facingDir.z / dirLen };
    const float yaw = atan2f(dir.x, dir.z) + DirectX::XM_PI;
    const float forwardOffset = m_size.z * 0.9f;
    const DirectX::XMFLOAT3 markerPos = {
        m_pos.x + dir.x * forwardOffset,
        0.002f,
        m_pos.z + dir.z * forwardOffset
    };
    const float markerSizeX = m_size.x * 0.70f;
    const float markerSizeZ = m_size.z * 0.70f;
    const float markerRadius = ((markerSizeX > markerSizeZ) ? markerSizeX : markerSizeZ) * 0.5f;
    const float playerRadius = ((m_size.x > m_size.z) ? m_size.x : m_size.z) * 0.5f;
    const float overlapMargin = playerRadius * 0.30f;
    const float playerDx = markerPos.x - m_pos.x;
    const float playerDz = markerPos.z - m_pos.z;
    const float playerRange = playerRadius + markerRadius + overlapMargin;

    // プレイヤー本体に近すぎると見づらくなるため、重なり判定で透明度を下げます。
    bool overlapsPlayer = (playerDx * playerDx + playerDz * playerDz) <= (playerRange * playerRange);
    bool overlapsEnemy = false;
    if (tran.enemy.exists != 0)
    {
        // 敵と重なる場合も同じく視認性が落ちるため、透明度を落とす対象に含めます。
        const float enemyRadius = playerRadius;
        const float enemyDx = markerPos.x - tran.enemy.pos.x;
        const float enemyDz = markerPos.z - tran.enemy.pos.z;
        const float enemyRange = enemyRadius + markerRadius;
        overlapsEnemy = (enemyDx * enemyDx + enemyDz * enemyDz) <= (enemyRange * enemyRange);
    }
    float normalAlpha = tran.gameplay.directionMarkerAlpha;
    float overlapAlpha = tran.gameplay.directionMarkerOverlapAlpha;

    // ImGui などから不正値が入っても破綻しないよう、0.0 - 1.0 に丸めます。
    if (normalAlpha < 0.0f) normalAlpha = 0.0f;
    if (normalAlpha > 1.0f) normalAlpha = 1.0f;
    if (overlapAlpha < 0.0f) overlapAlpha = 0.0f;
    if (overlapAlpha > 1.0f) overlapAlpha = 1.0f;

    // 重なり時だけ透過率を切り替え、プレイヤー周辺の視認性を維持します。
    const float markerAlpha = (overlapsPlayer || overlapsEnemy) ? overlapAlpha : normalAlpha;

    // 地面に寝かせた板ポリとして描画するため、X 回転と向き回転を合成します。
    const DirectX::XMMATRIX R =
        DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2) *
        DirectX::XMMatrixRotationY(yaw);
    const DirectX::XMMATRIX Tr = DirectX::XMMatrixTranslation(markerPos.x, markerPos.y, markerPos.z);
    DirectX::XMFLOAT4X4 worldDir;
    DirectX::XMStoreFloat4x4(&worldDir, DirectX::XMMatrixTranspose(R * Tr));
    Sprite::SetWorld(worldDir);
    Sprite::SetSize({ markerSizeX, markerSizeZ });
    Sprite::SetOffset({ 0.0f, 0.0f });
    Sprite::SetUVPos({ 0.0f, 0.0f });
    Sprite::SetUVScale({ 1.0f, 1.0f });
    Sprite::SetColor({ 1.0f, 1.0f, 1.0f, markerAlpha });
    Sprite::SetTexture(m_pDirectionTexture);
    SetCullingMode(D3D11_CULL_NONE);
    Sprite::Draw();
    SetCullingMode(D3D11_CULL_BACK);
}

/**
 * @brief プレイヤーが参照するカメラを更新します。
 * @param set 新しく参照するカメラです。
 */
void Player::SetCamera(Camera* set)
{
    m_pCamera = set;
}

/**
 * @brief Transfer からプレイヤーに関する共有状態を読み戻します。
 */
void Player::SyncFromTransfer()
{
    TRAN_INS;

    // 他システムで調整された最新値を読み込み、描画と挙動を一致させます。
    m_pos = tran.player.pos;
    m_size = tran.player.size;
    m_velocity = tran.player.velocity;
    m_color = tran.player.color;
    m_hp = tran.player.hp;
    m_maxHp = tran.player.maxHp;
    m_moveSpeed = tran.player.moveSpeed;
    m_dashDistance = tran.player.dashDistance;
    m_dashCooldown = tran.player.dashCooldown;
    m_dashDuration = tran.player.dashDuration;
    m_stageSize = tran.player.stageSize;
}

/**
 * @brief Player 内で更新した値を Transfer へ書き戻します。
 */
void Player::SyncToTransfer()
{
    TRAN_INS;

    // 外部 UI、当たり判定、デバッグ表示が現在状態を読めるよう、全主要値を共有します。
    tran.player.pos = m_pos;
    tran.player.size = m_size;
    tran.player.velocity = m_velocity;
    tran.player.color = m_color;
    tran.player.hp = m_hp;
    tran.player.maxHp = m_maxHp;
    tran.player.moveSpeed = m_moveSpeed;
    tran.player.dashDistance = m_dashDistance;
    tran.player.dashCooldown = m_dashCooldown;
    tran.player.dashDuration = m_dashDuration;
    tran.player.stageSize = m_stageSize;
    tran.gameplayDebug.playerEvading = m_isDashing ? 1 : 0;
    tran.gameplayDebug.playerEvadeCooldownScale = tran.GetEvadeCooldownScaleByLevel(tran.roguelike.evadeCooldownLevel);
}

/**
 * @brief 入力に応じて通常移動または回避移動を処理します。
 * @param dt 今フレーム分の経過秒数です。
 */
void Player::ApplyMovement(float dt)
{
    TRAN_INS;
    float dirX = 0.0f;
    float dirZ = 0.0f;

    // キーボード入力を 2 軸移動ベクトルへ変換します。
    if (IsKeyPress('W')) dirZ += 1.0f;
    if (IsKeyPress('S')) dirZ -= 1.0f;
    if (IsKeyPress('A')) dirX -= 1.0f;
    if (IsKeyPress('D')) dirX += 1.0f;

    // パッド入力がキーボードより強い場合は、アナログ入力を優先します。
    const float padX = GetPadLeftStickX();
    const float padZ = GetPadLeftStickY();
    if (fabsf(padX) > fabsf(dirX)) dirX = padX;
    if (fabsf(padZ) > fabsf(dirZ)) dirZ = padZ;

    float len = sqrtf(dirX * dirX + dirZ * dirZ);
    if (len > 1.0f)
    {
        // 斜め移動で速度超過しないよう、長さ 1 に正規化します。
        dirX /= len;
        dirZ /= len;
        len = 1.0f;
    }
    else if (len <= 0.0001f)
    {
        // 微小入力は停止扱いにして、向き更新や誤移動を防ぎます。
        dirX = 0.0f;
        dirZ = 0.0f;
        len = 0.0f;
    }
    if (len > 0.0001f)
    {
        // 有効入力時だけ向きを更新し、停止時は直前の向きを維持します。
        m_facingDir = { dirX, 0.0f, dirZ };
    }

    // 強化レベル込みの実効クールタイムを毎フレーム再計算し、UI 表示と挙動を一致させます。
    const float evadeCooldownScale = tran.GetEvadeCooldownScaleByLevel(tran.roguelike.evadeCooldownLevel);
    const float effectiveDashCooldown = m_dashCooldown * evadeCooldownScale;
    m_effectiveDashCooldown = effectiveDashCooldown;
    tran.gameplayDebug.playerEvadeCooldownScale = evadeCooldownScale;

    if (m_dashCooldownTimer > 0.0f)
    {
        // クールタイムは時間経過で減らし、負値までは落としません。
        m_dashCooldownTimer -= dt;
        if (m_dashCooldownTimer < 0.0f) m_dashCooldownTimer = 0.0f;
    }

    // 回避開始条件:
    // まだ回避中ではない、Shift が押された、クールタイムが終わっている、有効な移動入力がある。
    if (!m_isDashing && IsKeyTrigger(VK_SHIFT) && m_dashCooldownTimer <= 0.0f && len > 0.0001f)
    {
        if (m_dashDistance > 0.0f && m_dashDuration > 0.0f)
        {
            // 回避開始時に残り時間とクールタイムを設定し、途中で入力が変わっても方向を固定します。
            m_isDashing = true;
            m_dashTimer = m_dashDuration;
            m_dashCooldownTimer = effectiveDashCooldown;
            const float dashLen = sqrtf(dirX * dirX + dirZ * dirZ);
            if (dashLen > 0.0001f)
            {
                // 正規化した方向を回避用に保持し、描画向きもその方向へ揃えます。
                m_dashDir = DirectX::XMFLOAT3(dirX / dashLen, 0.0f, dirZ / dashLen);
                m_facingDir = m_dashDir;
            }
        }
    }

    if (m_isDashing)
    {
        // 回避中は移動量を前半へ寄せ、見た目にも分かる加速感を出します。
        const float duration = m_dashDuration;
        const float distance = m_dashDistance;
        if (duration > 0.0f && distance > 0.0f)
        {
            const float prevTimer = m_dashTimer;
            float nextTimer = m_dashTimer - dt;
            if (nextTimer < 0.0f) nextTimer = 0.0f;

            const float prevProgress = 1.0f - ClampFloat(prevTimer / duration, 0.0f, 1.0f);
            const float nextProgress = 1.0f - ClampFloat(nextTimer / duration, 0.0f, 1.0f);
            const float traveled =
                (EvaluateDashTravel01(nextProgress) - EvaluateDashTravel01(prevProgress)) * distance;
            const float speed = (dt > 0.0f) ? (traveled / dt) : 0.0f;

            m_velocity.x = m_dashDir.x * speed;
            m_velocity.y = 0.0f;
            m_velocity.z = m_dashDir.z * speed;
            m_facingDir = m_dashDir;
            m_pos.x += m_dashDir.x * traveled;
            m_pos.z += m_dashDir.z * traveled;
        }
        else
        {
            m_velocity = { 0.0f, 0.0f, 0.0f };
        }

        // 残り時間が尽きたら回避終了へ切り替えます。
        m_dashTimer -= dt;
        if (m_dashTimer <= 0.0f)
        {
            m_dashTimer = 0.0f;
            m_isDashing = false;
            m_velocity = { 0.0f, 0.0f, 0.0f };
        }
        return;
    }

    // 通常時は入力方向に移動速度を掛けて位置を更新します。
    m_velocity.x = dirX * m_moveSpeed;
    m_velocity.y = 0.0f;
    m_velocity.z = dirZ * m_moveSpeed;
    m_pos.x += m_velocity.x * dt;
    m_pos.z += m_velocity.z * dt;
}

/**
 * @brief プレイヤー座標をステージ内に収まるよう補正します。
 */
void Player::ClampToStage()
{
    // ステージサイズ未設定時は既定値を使い、計算不能にならないようにします。
    const float stage = (m_stageSize > 0.0f) ? m_stageSize : kDefaultStageSize;
    const float half = stage * 0.5f;
    const float halfX = m_size.x * 0.5f;
    const float halfZ = m_size.z * 0.5f;

    float minX = -half + halfX;
    float maxX = half - halfX;
    float minZ = -half + halfZ;
    float maxZ = half - halfZ;

    // プレイヤーサイズがステージより大きい場合でも、中央に収まるよう退避値へします。
    if (minX > maxX) { minX = 0.0f; maxX = 0.0f; }
    if (minZ > maxZ) { minZ = 0.0f; maxZ = 0.0f; }

    // 許可範囲へ丸め、地面上に固定します。
    m_pos.x = ClampFloat(m_pos.x, minX, maxX);
    m_pos.z = ClampFloat(m_pos.z, minZ, maxZ);
    m_pos.y = 0.0f;
}


