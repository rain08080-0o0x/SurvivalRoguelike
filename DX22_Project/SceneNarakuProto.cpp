#include "SceneNarakuProto.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Sprite.h"
#include "Texture.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace
{
    // 既存プロジェクトは固定FPS前提なので、1フレーム秒数も固定値で扱います。
    constexpr float kDt = 1.0f / fFPS;
    // 通常移動速度です。
    constexpr float kWalkSpeed = 1.5f;
    // 走り移動速度です。
    constexpr float kRunSpeed = 2.5f;
    // ロープ昇降時の深度変化速度です。
    constexpr float kRopeSpeed = 1.0f;
    // 走り続けた時の1秒あたりスタミナ消費です。
    constexpr float kRunCostPerSecond = 1.5f;
    // ロープ昇降中の1秒あたりスタミナ消費です。
    constexpr float kRopeCostPerSecond = 3.0f;
    // 攻撃1回のスタミナ消費です。
    constexpr float kAttackCost = 10.0f;
    // 採掘1回のスタミナ消費です。
    constexpr float kMiningCost = 7.0f;
    // ステップ1回のスタミナ消費です。
    constexpr float kStepCost = 5.0f;
    // ジャンプ1回のスタミナ消費です。
    constexpr float kJumpCost = 5.0f;
    // スタミナを消費していない時の1秒あたり回復量です。
    constexpr float kStaminaRecoverPerSecond = 2.0f;
    // ステップで進む距離です。
    constexpr float kStepDistance = 5.0f;
    // ステップの無敵時間です。
    constexpr float kStepInvincibleTime = 0.5f;
    // ステップ後に操作を戻すまでの硬直時間です。
    constexpr float kStepRecoveryTime = 0.5f;
    // 攻撃ボタンを押してから攻撃判定が出るまでの時間です。
    constexpr float kAttackStartup = 0.25f;
    // 攻撃判定が有効な時間です。
    constexpr float kAttackActive = 0.15f;
    // 攻撃判定後の硬直時間です。
    constexpr float kAttackRecovery = 0.40f;
    // 攻撃全体の長さです。
    constexpr float kAttackTotal = kAttackStartup + kAttackActive + kAttackRecovery;
    // 採掘モーション完了までの時間です。
    constexpr float kMiningTime = 2.0f;
    // 第一層プロトタイプで上昇負荷が発症する累計上昇量です。
    constexpr float kUpperLoadLimit = 10.0f;
    // 上昇していない時に1秒あたり回復する上昇負荷ゲージ量です。
    constexpr float kUpperLoadRecoveryPerSecond = 1.0f;
    // 通常の最大重量です。100%以上でも歩けますが一部行動が制限されます。
    constexpr float kMaxWeight = 100.0f;
    // 拾える限界重量です。これを超える拾得は拒否します。
    constexpr float kPickupWeightLimit = 150.0f;
    // 敵の通常移動速度です。プレイヤー通常速度の50%です。
    constexpr float kEnemyWalkSpeed = kWalkSpeed * 0.5f;
    // 敵の体当たり速度です。敵通常移動の3倍です。
    constexpr float kEnemyChargeSpeed = kEnemyWalkSpeed * 3.0f;
    // 敵が次の体当たりを開始するまでの間隔です。
    constexpr float kEnemyAttackInterval = 5.0f;
    // 敵の体当たり前予備動作時間です。
    constexpr float kEnemyTelegraphTime = 0.55f;
    // 敵の体当たり移動時間です。
    constexpr float kEnemyChargeTime = 0.45f;
    // 敵の体当たりが命中する距離です。
    constexpr float kEnemyHitRange = 0.45f;
    // 敵の体当たり命中時に押し出す距離です。
    constexpr float kKnockbackDistance = 1.5f;
    // ノックバックが続く時間です。
    constexpr float kKnockbackTime = 0.25f;
    // 採掘、ロープ、地面旧器に反応する距離です。
    constexpr float kInteractRange = 1.0f;
    // 帰還地点に反応する距離です。
    constexpr float kReturnRange = 1.4f;
    // 未発見採掘ポイントを発見する距離です。
    constexpr float kDiscoverRange = 3.0f;
    constexpr float kNearbyMiningVisibleRange = 8.0f;
    // つるはし攻撃の射程です。
    constexpr float kAttackRange = 1.15f;
    // ImGuiデバッグフィールドの半径です。
    constexpr float kWorldHalfSize = 30.0f;
    // Shiftをこの秒数以上押し続けたら走り扱いにします。
    constexpr float kShiftRunThreshold = 0.18f;

    // プロトタイプで使う4級旧器名です。コード側では文字化け回避のため英字にしています。
    const char* kRelicNames[] =
    {
        u8"錆びた輪",
        u8"欠けた歯車",
        u8"古びた留め具",
        u8"音のしない鈴",
        u8"黒ずんだ皿片",
        u8"ひび入り硝子",
        u8"曲がった鍵片",
        u8"くすんだ小筒"
    };
}

SceneNarakuProto::SceneNarakuProto()
{
    // 半透明床のSprite描画に使う1x1白テクスチャを作成します。
    m_debugWhiteTexture = new Texture();

    // 白1ピクセルをRGBA8で用意します。
    const unsigned int whitePixel = 0xffffffff;

    // Sprite側の色指定だけで床色を変えられるよう、元テクスチャは白にします。
    m_debugWhiteTexture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &whitePixel);

    // シーン作成時に最初の潜行状態を作ります。
    ResetRun();
}

SceneNarakuProto::~SceneNarakuProto()
{
    // 半透明床用テクスチャを解放します。
    SAFE_DELETE(m_debugWhiteTexture);
}

void SceneNarakuProto::ResetRun()
{
    // 所持金は地上側の永続値として扱うため、潜行初期化前に退避します。
    int keepMoney = m_money;

    // プレイヤーの体力、精神力、位置、行動タイマーを初期値に戻します。
    m_player = PlayerState();

    // 帰還地点を原点にして、プレイヤーもそこから開始します。
    m_player.pos = { 0.0f, 0.0f };

    // 最初の向きは画面上方向にして、攻撃やステップの向きが未定にならないようにします。
    m_player.facing = { 0.0f, 1.0f };

    // 潜行中の所持旧器を空にします。
    m_inventory.clear();

    // フィールドに置かれている旧器を空にします。
    m_groundRelics.clear();

    // 採掘ポイントはこの後で固定配置を作り直します。
    m_miningPoints.clear();

    // 敵も潜行ごとに初期配置へ戻すため空にします。
    m_enemies.clear();

    // 地形床とロープを固定データから作り直すため、古い一覧を空にします。
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_activeRope = -1;

    // 死亡/帰還後の再潜行では探索中ピンを消します。
    m_pins.clear();

    // 画面ログを初期化します。
    m_messages.clear();

    // Shift長押し/短押し判定を初期化します。
    m_shiftHold = 0.0f;

    // ステップ保留状態を解除します。
    m_shiftPendingStep = false;

    // 前フレームShift状態を初期化します。
    m_shiftWasPressed = false;

    // Shift長押し走り確定状態を初期化します。
    m_shiftRunCommitted = false;

    // 採掘タイマーを止めます。
    m_miningTimer = 0.0f;

    // 採掘中ポイントなしの状態に戻します。
    m_miningIndex = -1;

    // 所持品UIの選択を解除します。
    m_selectedInventory = -1;

    // 通常探索モードから開始します。
    m_mode = Mode::Explore;

    // リザルト集計値をリセットします。
    m_result = RunResult();

    // 退避していた所持金を戻します。
    m_money = keepMoney;

    // 地上に近い大きな床と、一段下の小さな床を生成します。
    m_floorRegions.push_back({ { 0.0f, 0.0f }, { 23.0f, 20.0f }, 0.0f, { 0.18f, 0.45f, 0.30f, 0.18f } });
    m_floorRegions.push_back({ { 7.0f, 9.0f }, { 8.0f, 6.0f }, 4.0f, { 0.25f, 0.36f, 0.55f, 0.28f } });
    m_floorRegions.push_back({ { 13.5f, 11.5f }, { 5.0f, 3.5f }, 4.0f, { 0.20f, 0.32f, 0.50f, 0.24f } });

    // 地上床から一段下の床へ降りられるデバッグ用ロープを配置します。
    m_ropePoints.push_back({ { 7.0f, 9.0f }, 0.0f, 4.0f });

    // 採掘ポイントは第一層プロトタイプ用の固定配置です。
    const Vec2 miningPositions[10] =
    {
        { -6.0f, 4.0f }, { 5.5f, 6.0f }, { 9.0f, -3.0f }, { -11.0f, -6.0f }, { 15.0f, 5.0f },
        { -18.0f, 8.0f }, { 18.0f, -11.0f }, { -4.0f, -15.0f }, { 3.0f, 18.0f }, { -16.0f, -17.0f }
    };

    // 10個の採掘ポイントを作ります。
    for (int i = 0; i < 10; ++i)
    {
        // 1箇所分の採掘ポイント状態を作ります。
        MiningPoint point;

        // 固定配置から座標を設定します。
        point.pos = miningPositions[i];

        // 見た目だけ4種類で回します。
        point.visualType = i % 4;

        // 最初の3箇所だけ初期地図に載っている扱いで表示します。
        point.discovered = i < 3;

        // 採掘完了時に出る旧器名を割り当てます。
        point.relicName = kRelicNames[i % 8];

        // 採掘ポイント一覧に登録します。
        m_miningPoints.push_back(point);
    }

    // 弱い敵を3体だけ固定配置します。
    m_enemies.push_back({ { -8.0f, 11.0f } });
    m_enemies.push_back({ { 13.0f, -8.0f } });
    m_enemies.push_back({ { -18.0f, -13.0f } });

    // 起動確認用のログを出します。
    AddMessage(u8"奈落塔プロトタイプを開始しました。");
}

void SceneNarakuProto::Update()
{
    // Tキーで探索と所持品表示を切り替えます。
    if (IsKeyTrigger('T'))
    {
        // 探索中なら所持品画面へ移ります。
        if (m_mode == Mode::Explore) m_mode = Mode::Inventory;

        // 所持品表示中なら探索へ戻ります。
        else if (m_mode == Mode::Inventory) m_mode = Mode::Explore;
    }

    // 探索モード中だけプレイヤーや敵などのゲーム更新を進めます。
    if (m_mode == Mode::Explore) UpdateExplore(kDt);
}

void SceneNarakuProto::UpdateExplore(float dt)
{
    // 今フレームの上昇量を後で計算できるよう、更新前の深度を保存します。
    m_player.previousDepth = m_player.depth;

    // 入力に応じてプレイヤーの移動と行動制限を更新します。
    UpdateMovement(dt);

    // 攻撃タイマーとスタミナ回復を更新します。
    UpdateAction(dt);

    // 採掘中なら採掘タイマーを進めます。
    UpdateMining(dt);

    // 敵の追跡と体当たりを更新します。
    UpdateEnemies(dt);

    // 近くの未発見採掘ポイントを発見済みにします。
    DiscoverNearbyMiningPoints();

    // 深度変化から上昇負荷を更新します。
    UpdateUpperLoad(dt);

    // リザルト用に今回の最大深度を記録します。
    m_result.maxDepth = std::max(m_result.maxDepth, m_player.depth);

    // Fキーで帰還、ロープ、拾う、採掘のいずれかを試します。
    if (IsKeyTrigger('F')) TryInteract();

    // 体力が0以下になったら死亡リザルトへ移行します。
    if (m_player.hp <= 0.0f && m_mode == Mode::Explore) StartDeath(u8"体力が0になりました。");
}

void SceneNarakuProto::UpdateMovement(float dt)
{
    // WASD入力を集めるための移動ベクトルです。
    Vec2 input;

    // Wキーでカメラから見た前方向へ進みます。
    if (IsKeyPress('W')) input.y += 1.0f;

    // Sキーでカメラから見た後ろ方向へ進みます。
    if (IsKeyPress('S')) input.y -= 1.0f;

    // Aキーでカメラから見た左方向へ進みます。
    if (IsKeyPress('A')) input.x -= 1.0f;

    // Dキーでカメラから見た右方向へ進みます。
    if (IsKeyPress('D')) input.x += 1.0f;

    // カメラから見た前方向を、斜め投影で画面上方向に見えるワールド方向へ対応させます。
    const Vec2 cameraForward = Normalize({ -1.0f, -1.0f });

    // カメラから見た右方向を、現在の3Dカメラで画面右方向に見えるワールド方向へ対応させます。
    const Vec2 cameraRight = Normalize({ -1.0f, 1.0f });

    // 入力をカメラ基準方向からワールド移動方向へ変換します。
    Vec2 move = Add(Mul(cameraRight, input.x), Mul(cameraForward, input.y));

    // 斜め移動が速くならないように正規化します。
    move = Normalize(move);

    // 入力がある時だけ向きを更新して、停止中の攻撃方向を維持します。
    if (move.x != 0.0f || move.y != 0.0f) m_player.facing = move;

    // Spaceが押された瞬間にジャンプ開始を試します。
    if (IsKeyTrigger(VK_SPACE)) TryStartJump();

    // 左右Shiftの現在入力を物理キーとして取得します。
    const bool shiftPressed = IsShiftPress();

    // 前フレームは押されておらず、今フレーム押されたらShift押下開始です。
    const bool shiftStarted = shiftPressed && !m_shiftWasPressed;

    // 前フレームは押されていて、今フレーム押されていなければShiftを離した瞬間です。
    const bool shiftReleased = !shiftPressed && m_shiftWasPressed;

    // Shiftが押された瞬間に「短押しならステップ」判定を保留します。
    if (shiftStarted)
    {
        // すぐにはステップせず、短押しか長押しかの判定を待ちます。
        m_shiftPendingStep = true;

        // 押下時間を0から測り直します。
        m_shiftHold = 0.0f;

        // まだ走りとして確定していない状態に戻します。
        m_shiftRunCommitted = false;
    }

    // このフレームで走り入力として扱うかどうかです。
    bool wantsRun = false;

    // Shift短押し/長押しの判定中なら押下時間を見ます。
    if (m_shiftPendingStep)
    {
        // Shiftを押し続けている時間を加算します。
        m_shiftHold += dt;

        // 閾値前に離されたら短押しステップとして扱います。
        if (shiftReleased)
        {
            // 長押し走りに確定していない短押しだけステップを開始します。
            if (!m_shiftRunCommitted)
            {
                // スタミナや重量条件を満たす場合だけステップを開始します。
                TryStartStep();
            }

            // 判定が終わったので保留を解除します。
            m_shiftPendingStep = false;

            // 走り確定状態も解除します。
            m_shiftRunCommitted = false;
        }

        // 閾値以上押されていたら長押し走りとして扱います。
        else if (m_shiftHold >= kShiftRunThreshold)
        {
            // このフレームの移動処理で走り速度を使わせます。
            wantsRun = true;

            // 閾値を超えたので、このShift入力はステップではなく走りとして確定します。
            m_shiftRunCommitted = true;
        }
    }

    // すでに長押し扱いになった後もShiftを押している間は走り扱いにします。
    else if (shiftPressed)
    {
        // このフレームの移動処理で走り速度を使わせます。
        wantsRun = true;
    }

    // Shiftが押されていないなら短押し判定状態を完全に解除します。
    if (!shiftPressed)
    {
        // ステップ保留を解除します。
        m_shiftPendingStep = false;

        // Shift押下時間も0に戻します。
        m_shiftHold = 0.0f;

        // 走り確定状態を解除します。
        m_shiftRunCommitted = false;
    }

    // ノックバック中は敵から押し出される移動を先に適用します。
    if (m_player.knockbackTimer > 0.0f)
    {
        // ノックバック速度ぶんプレイヤー座標をずらします。
        const Vec2 knockbackTarget = Add(m_player.pos, Mul(m_player.knockbackVelocity, dt));
        m_player.pos = ResolveFloorMove(m_player.pos, knockbackTarget, m_player.depth);

        // ノックバック残り時間を減らします。
        m_player.knockbackTimer = std::max(0.0f, m_player.knockbackTimer - dt);
    }

    // ステップ中は通常移動よりステップ移動を優先します。
    if (m_player.stepTimer > 0.0f)
    {
        // このフレーム開始時点の残り時間を保存します。
        float previous = m_player.stepTimer;

        // ステップ残り時間を減らします。
        m_player.stepTimer = std::max(0.0f, m_player.stepTimer - dt);

        // 無敵時間中だけ高速移動し、後硬直中は移動しません。
        if (previous > kStepRecoveryTime)
        {
            // 0.5秒で指定距離を進むようにステップ速度を計算します。
            const Vec2 stepTarget = Add(m_player.pos, Mul(m_player.facing, (kStepDistance / kStepInvincibleTime) * dt));
            m_player.pos = ResolveFloorMove(m_player.pos, stepTarget, m_player.depth);
        }
    }

    // ロープに掴まっていない時だけ平面移動を行います。
    else if (!m_player.onRope)
    {
        // 通常歩行速度に重量70%以上の低下を反映します。
        float speed = GetMoveSpeed();

        // 100%以上の重量では走れないため、ここで走行可否を判定します。
        bool canRun = GetCurrentWeight() < kMaxWeight && m_player.stamina > 0.0f;

        // 走り入力、移動入力、重量、スタミナをすべて満たした時だけ走ります。
        if (wantsRun && canRun && (move.x != 0.0f || move.y != 0.0f))
        {
            // 走り速度へ切り替えます。
            speed = kRunSpeed;

            // 1フレームぶんの走りスタミナを消費します。
            SpendStamina(kRunCostPerSecond * dt);
        }

        // 決まった速度で平面座標を進めます。
        const Vec2 moveTarget = Add(m_player.pos, Mul(move, speed * dt));
        m_player.pos = ResolveFloorMove(m_player.pos, moveTarget, m_player.depth);
    }

    // ロープに掴まっている時はW/Sを深度操作として扱います。
    if (m_player.onRope)
    {
        // ロープ番号が無効なら、操作不能にならないよう即座にロープ状態を解除します。
        if (m_activeRope < 0 || m_activeRope >= static_cast<int>(m_ropePoints.size()))
        {
            m_player.onRope = false;
            m_activeRope = -1;
            return;
        }

        // 現在つかまっているロープの上端/下端深度を参照します。
        const RopePoint& rope = m_ropePoints[m_activeRope];

        // 深度入力を一時的に保持します。
        float depthInput = 0.0f;

        // Wは上昇なので深度を減らします。
        if (IsKeyPress('W')) depthInput -= 1.0f;

        // Sは下降なので深度を増やします。
        if (IsKeyPress('S')) depthInput += 1.0f;

        // A/Dが押された瞬間はスタミナがなくてもロープから横へ離脱できるようにします。
        const bool wantsLeaveRope = IsKeyTrigger('A') || IsKeyTrigger('D');

        // ロープから離れる時は、掴まり状態を解除して少しだけ横へずらします。
        if (wantsLeaveRope)
        {
            // Aなら左、Dなら右へ離れる方向を決めます。
            const float leaveSign = IsKeyTrigger('A') ? -1.0f : 1.0f;

            // 現在深度に横へ降りられる床がある時だけロープを離します。
            if (!TryLeaveRopeSide(m_activeRope, leaveSign, cameraRight))
            {
                AddMessage(u8"足場がないためロープを離せません。");
            }
        }

        // 深度入力があり、スタミナを払える時だけ昇降します。
        if (m_player.onRope && depthInput != 0.0f && CanSpendStamina(kRopeCostPerSecond * dt))
        {
            // ロープ昇降のスタミナを消費します。
            SpendStamina(kRopeCostPerSecond * dt);

            // ロープの上端から下端までの範囲内に深度を制限します。
            m_player.depth = std::max(rope.topDepth, std::min(m_player.depth + depthInput * kRopeSpeed * dt, rope.bottomDepth));

            // ロープ中は平面位置をロープ位置へ固定して、床外へずれないようにします。
            m_player.pos = rope.pos;
        }
    }

    // ジャンプ中なら簡易的な空中時間と着地を更新します。
    if (!m_player.grounded)
    {
        // 空中にいる時間を加算します。
        m_player.airTime += dt;

        // 重力で縦速度を減らします。
        m_player.verticalSpeed -= 9.8f * dt;

        // プロト段階では一定時間で着地した扱いにします。
        if (m_player.airTime >= 0.65f)
        {
            // 簡易落下距離を計算し、落下ダメージ表へ当てます。
            float fallDistance = std::max(0.0f, -m_player.verticalSpeed * m_player.airTime * 0.5f);

            // 8m以上は即死扱いです。
            if (fallDistance >= 8.0f) StartDeath(u8"落下死しました。");

            // 6m以上8m未満は6ダメージです。
            else if (fallDistance >= 6.0f) m_player.hp -= 6.0f;

            // 4m以上6m未満は3ダメージです。
            else if (fallDistance >= 4.0f) m_player.hp -= 3.0f;

            // 2m以上4m未満は1ダメージです。
            else if (fallDistance >= 2.0f) m_player.hp -= 1.0f;

            // 着地したのでジャンプ状態を解除します。
            m_player.grounded = true;

            // 空中時間をリセットします。
            m_player.airTime = 0.0f;

            // 縦速度をリセットします。
            m_player.verticalSpeed = 0.0f;
        }
    }

    // デバッグフィールド外へ出ないようX座標を制限します。
    m_player.pos.x = std::max(-kWorldHalfSize, std::min(m_player.pos.x, kWorldHalfSize));

    // デバッグフィールド外へ出ないようY座標を制限します。
    m_player.pos.y = std::max(-kWorldHalfSize, std::min(m_player.pos.y, kWorldHalfSize));

    // 次フレームで押下/離上を判定できるよう、現在のShift状態を保存します。
    m_shiftWasPressed = shiftPressed;
}

void SceneNarakuProto::UpdateAction(float dt)
{
    // 攻撃中なら攻撃タイマーを進めます。
    if (m_player.attackTimer > 0.0f)
    {
        // 前フレーム時点の残り時間を保存し、アクティブ判定の突入を検出します。
        float previous = m_player.attackTimer;

        // 攻撃残り時間を減らします。
        m_player.attackTimer = std::max(0.0f, m_player.attackTimer - dt);

        // 攻撃開始からの経過時間を計算します。
        float elapsed = kAttackTotal - m_player.attackTimer;

        // 前フレーム時点の攻撃開始からの経過時間を計算します。
        float previousElapsed = kAttackTotal - previous;

        // このフレームで攻撃判定時間に入った、または判定時間中かを見ます。
        bool activeThisFrame = previousElapsed < kAttackStartup + kAttackActive && elapsed >= kAttackStartup;

        // 攻撃判定が有効なフレームだけ敵との当たり判定を取ります。
        if (activeThisFrame)
        {
            // すべての敵に対して攻撃が当たるか調べます。
            for (EnemyState& enemy : m_enemies)
            {
                // 死んでいる敵は判定しません。
                if (!enemy.alive) continue;

                // プレイヤーから敵への方向を計算します。
                Vec2 toEnemy = Sub(enemy.pos, m_player.pos);

                // 射程内かつ前方にいる場合だけヒットさせます。
                if (Distance(enemy.pos, m_player.pos) <= kAttackRange && Dot(Normalize(toEnemy), m_player.facing) > 0.25f)
                {
                    // つるはし1回ぶんのダメージを与えます。
                    enemy.hp -= 1.0f;

                    // HPが0以下なら敵を倒します。
                    if (enemy.hp <= 0.0f)
                    {
                        // 敵を非アクティブにします。
                        enemy.alive = false;

                        // 食料素材ドロップ相当のログを出します。
                        AddMessage(u8"敵を倒しました。食料素材を落としました。");
                    }
                }
            }
        }
    }

    // 左クリックが押された瞬間に攻撃開始を試します。
    if (IsMouseLeftTrigger()) TryStartAttack();

    // スタミナが最大未満なら自然回復できるかを判定します。
    if (m_player.stamina < 100.0f && m_player.attackTimer <= 0.0f && m_miningIndex < 0)
    {
        // Shift中またはロープ中はスタミナ消費行動中として回復を止めます。
        // ロープは掴まっているだけなら回復し、実際に昇降できる入力中だけ消費行動として扱います。
        const bool ropeClimbing = m_player.onRope && (IsKeyPress('W') || IsKeyPress('S')) && CanSpendStamina(kRopeCostPerSecond * dt);

        // Shift中またはロープ昇降中はスタミナ消費行動中として回復を止めます。
        bool spending = IsShiftPress() || ropeClimbing;

        // 消費行動中でなければ1フレームぶん回復します。
        if (!spending) m_player.stamina = std::min(100.0f, m_player.stamina + kStaminaRecoverPerSecond * dt);
    }
}

void SceneNarakuProto::UpdateMining(float dt)
{
    // 採掘中でなければ何もしません。
    if (m_miningIndex < 0) return;

    // 採掘完了までの残り時間を減らします。
    m_miningTimer -= dt;

    // まだ採掘が終わっていなければ戻ります。
    if (m_miningTimer > 0.0f) return;

    // 採掘中だったポイントを取得します。
    MiningPoint& point = m_miningPoints[m_miningIndex];

    // このポイントを採掘済みにします。
    point.mined = true;

    // リザルト用の採掘数を増やします。
    ++m_result.minedCount;

    // 発見確認に出す旧器を作ります。
    m_pendingRelic = { point.relicName, 15.0f, 5 };

    // 拾わず置く場合に戻す位置を採掘ポイント位置にします。
    m_pendingRelicPos = point.pos;

    // 採掘中番号を解除します。
    m_miningIndex = -1;

    // 採掘タイマーを0に戻します。
    m_miningTimer = 0.0f;

    // 旧器を拾うかどうかの確認画面へ移ります。
    m_mode = Mode::RelicPrompt;

    // HUDログに発見を出します。
    AddMessage(u8"旧器を発見しました。");
}

void SceneNarakuProto::UpdateEnemies(float dt)
{
    // 登録されている敵を順番に更新します。
    for (EnemyState& enemy : m_enemies)
    {
        // 死んでいる敵は移動も攻撃もしません。
        if (!enemy.alive) continue;

        // 体当たり中なら専用の高速移動と命中判定を行います。
        if (enemy.chargeTimer > 0.0f)
        {
            // 体当たり方向へ通常移動の3倍速度で進みます。
            enemy.pos = Add(enemy.pos, Mul(enemy.chargeDir, kEnemyChargeSpeed * dt));

            // 体当たりの残り時間を減らします。
            enemy.chargeTimer = std::max(0.0f, enemy.chargeTimer - dt);

            // まだこの体当たりで当たっていない場合だけ命中判定を行います。
            if (!enemy.hasHitThisCharge && Distance(enemy.pos, m_player.pos) <= kEnemyHitRange)
            {
                // ステップ無敵時間中かどうかを判定します。
                bool invincible = m_player.stepTimer > kStepRecoveryTime;

                // 無敵でなければ体当たりダメージとノックバックを与えます。
                if (!invincible)
                {
                    // 体当たり1回ぶんのダメージを与えます。
                    m_player.hp -= 1.0f;

                    // ノックバック時間を設定します。
                    m_player.knockbackTimer = kKnockbackTime;

                    // 敵からプレイヤーへ押し出す方向を計算します。
                    Vec2 dir = Normalize(Sub(m_player.pos, enemy.pos));

                    // 指定距離を指定時間で押し出す速度を設定します。
                    m_player.knockbackVelocity = Mul(dir, kKnockbackDistance / kKnockbackTime);

                    // HUDログに被弾を出します。
                    AddMessage(u8"敵の体当たりを受けました。");
                }

                // この体当たりではもう追加ヒットしないようにします。
                enemy.hasHitThisCharge = true;
            }

            // 体当たり中の敵は通常追跡を行わないため、この敵の更新を終えます。
            continue;
        }

        // 予備動作中ならタイマーを進め、終了時に体当たりへ移ります。
        if (enemy.telegraphTimer > 0.0f)
        {
            // 予備動作の残り時間を減らします。
            enemy.telegraphTimer = std::max(0.0f, enemy.telegraphTimer - dt);

            // 予備動作が終わった瞬間に体当たり方向を決定します。
            if (enemy.telegraphTimer <= 0.0f)
            {
                // 現在のプレイヤー位置へ向かって突進します。
                enemy.chargeDir = Normalize(Sub(m_player.pos, enemy.pos));

                // 体当たり時間を設定します。
                enemy.chargeTimer = kEnemyChargeTime;

                // 体当たりヒット済みフラグをリセットします。
                enemy.hasHitThisCharge = false;
            }

            // 予備動作中は通常追跡を行わないため、この敵の更新を終えます。
            continue;
        }

        // 敵からプレイヤーへの方向を計算します。
        Vec2 toPlayer = Sub(m_player.pos, enemy.pos);

        // 敵とプレイヤーの距離を計算します。
        float dist = Distance(enemy.pos, m_player.pos);

        // 近すぎない場合はプレイヤーへゆっくり接近します。
        if (dist > 0.1f) enemy.pos = Add(enemy.pos, Mul(Normalize(toPlayer), kEnemyWalkSpeed * dt));

        // 体当たりの再使用待ち時間を減らします。
        enemy.attackCooldown -= dt;

        // クールダウンが終わり、十分近ければ予備動作を開始します。
        if (enemy.attackCooldown <= 0.0f && dist <= 3.0f)
        {
            // 予備動作タイマーを設定します。
            enemy.telegraphTimer = kEnemyTelegraphTime;

            // 次回攻撃までのクールダウンを5秒に戻します。
            enemy.attackCooldown = kEnemyAttackInterval;
        }
    }
}

void SceneNarakuProto::UpdateUpperLoad(float dt)
{
    // 前フレームからどれだけ上昇したかを計算します。
    float ascent = m_player.previousDepth - m_player.depth;

    // 上昇している場合は上昇負荷ゲージを加算します。
    if (ascent > 0.0f)
    {
        // 上昇量そのものを内部ゲージに加算します。
        m_player.upperLoad += ascent;

        // 第一層の発症目安10mに達したら発症します。
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            // プロト仕様として精神力を10減らします。
            m_player.mental = std::max(0.0f, m_player.mental - 10.0f);

            // 発症後は内部ゲージを0へ戻します。
            m_player.upperLoad = 0.0f;

            // HUDログに発症を出します。
            AddMessage(u8"上昇負荷が発症しました。精神力-10。");

            // 精神力が0になったら精神崩壊扱いで死亡リザルトへ移します。
            if (m_player.mental <= 0.0f) StartDeath(u8"精神崩壊");
        }
    }

    // 上昇していない場合は停止、平地、下降のすべてで一定速度回復します。
    else
    {
        // 1秒あたり1mぶん内部ゲージを減らします。
        m_player.upperLoad = std::max(0.0f, m_player.upperLoad - kUpperLoadRecoveryPerSecond * dt);
    }
}

void SceneNarakuProto::TryInteract()
{
    // 帰還地点が最優先です。原点付近でFを押すと帰還します。
    if (IsNear(m_player.pos, { 0.0f, 0.0f }, kReturnRange))
    {
        // 帰還リザルトへ移行します。
        FinishReturn();

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }

    // 次にロープへの乗り降りを判定します。
    const int ropeIndex = FindNearestRopeIndex(kInteractRange);
    if (ropeIndex >= 0)
    {
        // 対象ロープを取得します。
        const RopePoint& rope = m_ropePoints[ropeIndex];

        // すでにロープ中なら、近い端へ吸着してロープを離します。
        if (m_player.onRope)
        {
            m_player.depth = std::fabs(m_player.depth - rope.bottomDepth) < std::fabs(m_player.depth - rope.topDepth) ? rope.bottomDepth : rope.topDepth;
            m_player.pos = rope.pos;
            m_player.onRope = false;
            m_activeRope = -1;
            AddMessage(u8"ロープを離しました。");
        }

        // ロープ外なら、近い端へ吸着してロープにつかまります。
        else
        {
            m_player.onRope = true;
            m_activeRope = ropeIndex;
            m_player.pos = rope.pos;
            m_player.depth = std::fabs(m_player.depth - rope.bottomDepth) < std::fabs(m_player.depth - rope.topDepth) ? rope.bottomDepth : rope.topDepth;
            AddMessage(u8"ロープにつかまりました。");
        }

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }

    // フィールドに置かれた旧器が近くにあれば拾う確認へ移ります。
    for (GroundRelic& relic : m_groundRelics)
    {
        // 無効化済みの旧器は無視します。
        if (!relic.active) continue;

        // 近くにある旧器だけ反応します。
        if (IsNear(m_player.pos, relic.pos, kInteractRange))
        {
            // 拾う確認に表示する旧器を設定します。
            m_pendingRelic = relic.item;

            // 拾わず戻す場合の位置を記録します。
            m_pendingRelicPos = relic.pos;

            // 一旦地面側を無効化して二重取得を避けます。
            relic.active = false;

            // 旧器確認モードへ移ります。
            m_mode = Mode::RelicPrompt;

            // 1回のF入力で複数の対象を処理しないよう戻ります。
            return;
        }
    }

    // 最後に採掘ポイントの開始判定を行います。
    for (int i = 0; i < static_cast<int>(m_miningPoints.size()); ++i)
    {
        // 対象採掘ポイントを取得します。
        MiningPoint& point = m_miningPoints[i];

        // 採掘済み、または遠いポイントは無視します。
        if (point.mined || !IsNear(m_player.pos, point.pos, kInteractRange)) continue;

        // スタミナが足りない場合は採掘を開始しません。
        if (!CanSpendStamina(kMiningCost))
        {
            // HUDログにスタミナ不足を出します。
            AddMessage(u8"スタミナが足りないため採掘できません。");

            // 近くの採掘対象を見つけたので処理を終えます。
            return;
        }

        // 採掘開始時にスタミナを消費します。
        SpendStamina(kMiningCost);

        // 採掘中のポイント番号を記録します。
        m_miningIndex = i;

        // 採掘完了までの2秒タイマーを開始します。
        m_miningTimer = kMiningTime;

        // HUDログに採掘開始を出します。
        AddMessage(u8"採掘を開始しました。");

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }
}

void SceneNarakuProto::TryStartStep()
{
    // 重量100%以上ではステップ不可です。
    if (GetCurrentWeight() >= kMaxWeight) { AddMessage(u8"重量が重すぎてステップできません。"); return; }

    // スタミナ不足ならステップ不可です。
    if (!CanSpendStamina(kStepCost)) { AddMessage(u8"スタミナが足りないためステップできません。"); return; }

    // ステップ1回ぶんのスタミナを消費します。
    SpendStamina(kStepCost);

    // 無敵時間と後硬直の合計時間を設定します。
    m_player.stepTimer = kStepInvincibleTime + kStepRecoveryTime;
}

void SceneNarakuProto::TryStartJump()
{
    // 地上にいない時は二段ジャンプを許可しません。
    if (!m_player.grounded) return;

    // 重量100%以上ではジャンプ不可です。
    if (GetCurrentWeight() >= kMaxWeight) { AddMessage(u8"重量が重すぎてジャンプできません。"); return; }

    // スタミナ不足ならジャンプ不可です。
    if (!CanSpendStamina(kJumpCost)) { AddMessage(u8"スタミナが足りないためジャンプできません。"); return; }

    // ジャンプ1回ぶんのスタミナを消費します。
    SpendStamina(kJumpCost);

    // 空中状態へ切り替えます。
    m_player.grounded = false;

    // 高さ1m程度を想定した初速を入れます。
    m_player.verticalSpeed = 4.45f;

    // 空中時間を0から測ります。
    m_player.airTime = 0.0f;
}

void SceneNarakuProto::TryStartAttack()
{
    // 攻撃中は次の攻撃を開始しません。
    if (m_player.attackTimer > 0.0f) return;

    // スタミナ不足なら攻撃不可です。
    if (!CanSpendStamina(kAttackCost)) { AddMessage(u8"スタミナが足りないため攻撃できません。"); return; }

    // 攻撃1回ぶんのスタミナを消費します。
    SpendStamina(kAttackCost);

    // 攻撃全体時間を設定します。
    m_player.attackTimer = kAttackTotal;
}

bool SceneNarakuProto::IsShiftPress() const
{
    // 左Shiftまたは右Shiftが押されているかを直接確認します。
    return IsRawKeyPress(VK_LSHIFT) || IsRawKeyPress(VK_RSHIFT) || IsRawKeyPress(VK_SHIFT);
}

void SceneNarakuProto::StartDeath(const char* reason)
{
    // すでに死亡リザルト中なら二重処理を防ぎます。
    if (m_mode == Mode::DeathResult) return;

    // 死亡理由をリザルトに記録します。
    m_result.reason = reason;

    // 死亡時に失った旧器数を記録します。
    m_result.lostRelics = static_cast<int>(m_inventory.size());

    // 死亡ペナルティとして所持旧器を全ロストします。
    m_inventory.clear();

    // 死亡ペナルティとして探索中ピンを失わせます。
    m_pins.clear();

    // 死亡リザルトモードへ移行します。
    m_mode = Mode::DeathResult;
}

void SceneNarakuProto::FinishReturn()
{
    // 帰還理由をリザルトに記録します。
    m_result.reason = u8"生還しました。";

    // 持ち帰った旧器数を記録します。
    m_result.carriedRelics = static_cast<int>(m_inventory.size());

    // 売却額を集計する前に0へ戻します。
    m_result.saleAmount = 0;

    // 所持旧器の売値を合計します。
    for (const RelicItem& item : m_inventory) m_result.saleAmount += item.value;

    // 帰還リザルトモードへ移行します。
    m_mode = Mode::ReturnResult;
}

void SceneNarakuProto::SellAllAndRestart()
{
    // リザルトで計算した売却額を所持金に加算します。
    m_money += m_result.saleAmount;

    // 次の潜行を開始します。
    ResetRun();
}

void SceneNarakuProto::RestartAfterDeath()
{
    // 死亡後の再挑戦として新しい潜行を開始します。
    ResetRun();
}

void SceneNarakuProto::Draw()
{
    // デバッグ用3Dフィールドを最初に描画します。
    Draw3DField();

    // 常時確認するステータスHUDを描画します。
    DrawHud();

    // 所持品モードでは所持品と地図ピンUIを重ねます。
    if (m_mode == Mode::Inventory) { DrawInventory(); DrawMapControls(); }

    // 旧器発見中は拾う/置く確認を重ねます。
    else if (m_mode == Mode::RelicPrompt) DrawRelicPrompt();

    // 帰還または死亡後はリザルトを重ねます。
    else if (m_mode == Mode::ReturnResult || m_mode == Mode::DeathResult) DrawResult();
}

void SceneNarakuProto::Draw3DField()
{
    using namespace DirectX;

    // プレイヤー位置を3D描画用の注視点に変換します。
    const XMFLOAT3 playerCenter = ToWorld3D(m_player.pos, m_player.depth, 0.7f);

    // 斜め見下ろしになるように、プレイヤーの右後ろ上方へカメラを置きます。
    const XMVECTOR eye = XMVectorSet(playerCenter.x + 8.0f, playerCenter.y + 8.0f, playerCenter.z + 8.0f, 0.0f);

    // カメラは常にプレイヤー付近を向くようにします。
    const XMVECTOR target = XMVectorSet(playerCenter.x, playerCenter.y, playerCenter.z, 0.0f);

    // DirectXの標準的な上方向を使ってビュー行列を作ります。
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // 既存Geometoryはビュー/射影を転置して渡す設計なので、Main.cpp側の使い方に合わせます。
    XMFLOAT4X4 view;
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMMatrixLookAtLH(eye, target, up)));
    Geometory::SetView(view);
    Sprite::SetView(view);

    // 画面比率を使って、デバッグ用の遠近投影を設定します。
    XMFLOAT4X4 projection;
    const float aspect = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), aspect, 0.1f, 500.0f)));
    Geometory::SetProjection(projection);
    Sprite::SetProjection(projection);

    // 線描画に使う色をまとめます。
    const XMFLOAT4 gridColor = { 0.22f, 0.24f, 0.26f, 1.0f };
    const XMFLOAT4 playerColor = { 0.1f, 0.7f, 1.0f, 1.0f };
    const XMFLOAT4 enemyColor = { 1.0f, 0.25f, 0.2f, 1.0f };
    const XMFLOAT4 miningColor = { 1.0f, 0.85f, 0.2f, 1.0f };
    const XMFLOAT4 minedColor = { 0.45f, 0.45f, 0.45f, 1.0f };
    const XMFLOAT4 relicColor = { 0.4f, 1.0f, 0.65f, 1.0f };
    const XMFLOAT4 ropeColor = { 0.6f, 0.45f, 0.25f, 1.0f };
    const XMFLOAT4 returnColor = { 0.25f, 1.0f, 0.3f, 1.0f };
    const XMFLOAT4 pinColor = { 1.0f, 0.35f, 0.9f, 1.0f };

    // 3Dデバッグ描画は深度テストを有効にして、前後関係を分かりやすくします。
    SetDepthTest(true);

    // 半透明床は両面から見える方がデバッグしやすいので、カリングを切ります。
    SetCullingMode(D3D11_CULL_NONE);

    // Spriteのアルファ値を効かせるため、アルファブレンドを有効にします。
    SetBlendMode(BLEND_ALPHA);

    // 実際の床判定に使っている地形を、そのまま半透明床として描画します。
    for (const FloorRegion& floor : m_floorRegions)
    {
        const XMFLOAT3 floorCenter = ToWorld3D(floor.center, floor.depth, -0.05f);
        DrawTransparentFloor3D(floorCenter, { floor.halfSize.x * 2.0f, floor.halfSize.y * 2.0f }, floor.color);
    }

    // ロープ穴の目印として、地上側に暗い半透明板を重ねます。
    for (const RopePoint& rope : m_ropePoints)
    {
        DrawTransparentFloor3D(ToWorld3D(rope.pos, rope.topDepth, -0.04f), { 3.0f, 3.0f }, { 0.02f, 0.03f, 0.04f, 0.35f });
    }

    // 地面グリッドを描いて、現在の移動方向と距離感を見やすくします。
    for (int i = -30; i <= 30; i += 2)
    {
        // X方向に伸びる線を追加します。
        Geometory::AddLine({ -30.0f, 0.0f, static_cast<float>(i) }, { 30.0f, 0.0f, static_cast<float>(i) }, gridColor);

        // Z方向に伸びる線を追加します。
        Geometory::AddLine({ static_cast<float>(i), 0.0f, -30.0f }, { static_cast<float>(i), 0.0f, 30.0f }, gridColor);
    }

    // 帰還地点を緑の柱で示します。
    const XMFLOAT3 returnBase = ToWorld3D({ 0.0f, 0.0f }, 0.0f, 0.05f);
    DrawDebugBox3D({ returnBase.x, returnBase.y + 0.25f, returnBase.z }, { 0.9f, 0.5f, 0.9f });
    Geometory::AddLine({ returnBase.x, returnBase.y, returnBase.z }, { returnBase.x, returnBase.y + 2.0f, returnBase.z }, returnColor);

    // ロープは接続深度に合わせて上端から下端まで線で描画します。
    for (const RopePoint& rope : m_ropePoints)
    {
        // ロープ上端を3D座標へ変換します。
        const XMFLOAT3 top = ToWorld3D(rope.pos, rope.topDepth, 1.2f);

        // ロープ下端を3D座標へ変換します。
        const XMFLOAT3 bottom = ToWorld3D(rope.pos, rope.bottomDepth, 0.0f);

        // ロープ本体をラインで描画します。
        Geometory::AddLine(top, bottom, ropeColor);

        // 触れる位置が分かるように小さい箱を置きます。
        DrawDebugBox3D({ top.x, top.y, top.z }, { 0.35f, 0.35f, 0.35f });
        DrawDebugBox3D({ bottom.x, bottom.y, bottom.z }, { 0.30f, 0.30f, 0.30f });
    }

    // 採掘ポイントを黄色系の箱と縦マーカーで描画します。
    for (const MiningPoint& point : m_miningPoints)
    {
        // 未記録でも、近くまで来た採掘ポイントは現地で見えるようにします。
        const bool visibleInField = point.discovered || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField)
        {
            continue;
        }

        // 採掘ポイントの位置を深度0の地表として扱います。
        const XMFLOAT3 base = ToWorld3D(point.pos, 0.0f, 0.15f);

        // 見た目4種類は箱の横幅だけ少し変えて区別します。
        const float width = 0.45f + 0.08f * static_cast<float>(point.visualType);
        DrawDebugBox3D({ base.x, base.y + 0.2f, base.z }, { width, 0.4f, width });

        // 採掘済みかどうかが分かる色の縦線を足します。
        const XMFLOAT4 markerColor = point.mined ? minedColor : miningColor;
        Geometory::AddLine({ base.x, base.y, base.z }, { base.x, base.y + 1.4f, base.z }, markerColor);
    }

    // 地面に落ちている旧器を小さい箱と緑の縦線で示します。
    for (const GroundRelic& relic : m_groundRelics)
    {
        // 回収不能になった旧器は描画しません。
        if (!relic.active)
        {
            continue;
        }

        // 旧器の位置を3D座標へ変換します。
        const XMFLOAT3 base = ToWorld3D(relic.pos, 0.0f, 0.12f);

        // 小さな箱で旧器の本体を描きます。
        DrawDebugBox3D({ base.x, base.y + 0.12f, base.z }, { 0.35f, 0.25f, 0.35f });

        // 近くから見失わないように縦マーカーを追加します。
        Geometory::AddLine({ base.x, base.y, base.z }, { base.x, base.y + 1.0f, base.z }, relicColor);
    }

    // 敵を赤い球と進行方向ラインで描画します。
    for (const EnemyState& enemy : m_enemies)
    {
        // 死亡済みの敵は描画しません。
        if (!enemy.alive)
        {
            continue;
        }

        // 敵の位置を地表上の3D座標へ変換します。
        const XMFLOAT3 center = ToWorld3D(enemy.pos, 0.0f, 0.45f);

        // 球で敵本体を描きます。
        DrawDebugSphere3D(center, 0.45f);

        // 赤い縦線を足して、灰色球でも敵だと分かるようにします。
        Geometory::AddLine({ center.x, center.y - 0.45f, center.z }, { center.x, center.y + 0.8f, center.z }, enemyColor);

        // 体当たり準備中または体当たり中は、突進方向を赤い線で表示します。
        if (enemy.telegraphTimer > 0.0f || enemy.chargeTimer > 0.0f)
        {
            const XMFLOAT3 end = { center.x + enemy.chargeDir.x * 2.0f, center.y, center.z + enemy.chargeDir.y * 2.0f };
            Geometory::AddLine(center, end, enemyColor);
        }
    }

    // プレイヤーを青い箱で描きます。
    DrawDebugBox3D(playerCenter, { 0.55f, 1.2f, 0.55f });

    // プレイヤーの向きが分かるように、前方へ青いラインを伸ばします。
    const XMFLOAT3 facingEnd =
    {
        playerCenter.x + m_player.facing.x * 1.4f,
        playerCenter.y + 0.2f,
        playerCenter.z + m_player.facing.y * 1.4f
    };
    Geometory::AddLine({ playerCenter.x, playerCenter.y + 0.2f, playerCenter.z }, facingEnd, playerColor);

    // 地図ピンをピンクの縦線で表示します。
    for (const Vec2& pin : m_pins)
    {
        // ピン位置を地表の3D座標へ変換します。
        const XMFLOAT3 base = ToWorld3D(pin, 0.0f, 0.05f);

        // ピンの立っている位置をラインで示します。
        Geometory::AddLine({ base.x, base.y, base.z }, { base.x, base.y + 1.6f, base.z }, pinColor);
    }

    // ここまで積んだラインをまとめて描画します。
    Geometory::DrawLines();
}

void SceneNarakuProto::DrawField()
{
    // フィールドウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

    // フィールドウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(760.0f, 620.0f), ImGuiCond_FirstUseEver);

    // フィールド描画用ウィンドウを開始します。
    ImGui::Begin(u8"奈落塔プロト フィールド");

    // フィールドキャンバス左上のスクリーン座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // フィールドキャンバスの表示サイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 520.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // フィールド背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(30, 36, 34, 255));

    // フィールド外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 150, 140, 255));

    // 帰還地点のワールド座標を斜め見下ろし座標に変換します。
    Vec2 ret = WorldToObliqueCanvas(canvasPos, canvasSize, { 0.0f, 0.0f });

    // 帰還地点を青い円で描きます。
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 9.0f, IM_COL32(80, 180, 255, 255));

    // すべてのロープを描画します。
    for (const RopePoint& rope : m_ropePoints)
    {
        // ロープ座標を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, rope.pos, rope.topDepth);

        // ロープを縦長の茶色い矩形で描きます。
        draw->AddRectFilled(ImVec2(p.x - 4.0f, p.y - 12.0f), ImVec2(p.x + 4.0f, p.y + 12.0f), IM_COL32(170, 120, 70, 255));
    }

    // 発見済み、または近くまで来た採掘ポイントを描画します。
    for (const MiningPoint& point : m_miningPoints)
    {
        // 未記録でも近くまで来たポイントは、初期記録済みと同じ色で見せます。
        const bool visibleInField = point.discovered || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField) continue;

        // 採掘ポイント座標を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, point.pos);

        // 採掘済みは暗色、未採掘は黄色系にします。
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);

        // 見た目4種類は半径差だけで表現します。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 5.0f + static_cast<float>(point.visualType), color);
    }

    // 地面に置かれた旧器を描画します。
    for (const GroundRelic& relic : m_groundRelics)
    {
        // 非アクティブな旧器は描画しません。
        if (!relic.active) continue;

        // 旧器位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, relic.pos);

        // 旧器を小さな四角で描きます。
        draw->AddRectFilled(ImVec2(p.x - 4.0f, p.y - 4.0f), ImVec2(p.x + 4.0f, p.y + 4.0f), IM_COL32(240, 220, 130, 255));
    }

    // プレイヤーが置いたピンを描画します。
    for (const Vec2& pin : m_pins)
    {
        // ピン位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, pin);

        // ピンを赤い三角形で描きます。
        draw->AddTriangleFilled(ImVec2(p.x, p.y - 10.0f), ImVec2(p.x - 6.0f, p.y + 5.0f), ImVec2(p.x + 6.0f, p.y + 5.0f), IM_COL32(230, 80, 90, 255));
    }

    // 敵を描画します。
    for (const EnemyState& enemy : m_enemies)
    {
        // 死んだ敵は描画しません。
        if (!enemy.alive) continue;

        // 敵位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, enemy.pos);

        // 通常時の敵色を赤にします。
        ImU32 color = enemy.telegraphTimer > 0.0f ? IM_COL32(255, 200, 60, 255) : IM_COL32(210, 70, 70, 255);

        // 体当たり中はより強い赤で表示します。
        if (enemy.chargeTimer > 0.0f) color = IM_COL32(255, 80, 40, 255);

        // 敵を円で描きます。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 7.0f, color);
    }

    // プレイヤー位置を斜め見下ろし座標へ変換します。
    Vec2 player = WorldToObliqueCanvas(canvasPos, canvasSize, m_player.pos, m_player.depth);

    // プレイヤー本体を緑の円で描きます。
    draw->AddCircleFilled(ImVec2(player.x, player.y), 7.5f, IM_COL32(90, 220, 150, 255));

    // 向き表示の終点を計算します。
    Vec2 faceEnd = WorldToObliqueCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 1.2f)), m_player.depth);

    // プレイヤーの向きを短い線で描きます。
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 2.0f);

    // キャンバスぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    // 操作確認用の短い説明を表示します。
    ImGui::Text(u8"WASD 移動 / Shift短押し ステップ / Shift長押し 走り / Space ジャンプ / 左クリック 攻撃 / F 調べる / T 所持品 / ロープ中A/D 離脱");

    // フィールドウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawHud()
{
    // HUDウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(800.0f, 20.0f), ImGuiCond_FirstUseEver);

    // HUDウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(430.0f, 620.0f), ImGuiCond_FirstUseEver);

    // HUDウィンドウを開始します。
    ImGui::Begin(u8"奈落塔プロト HUD");

    // 地上で保持している所持金を表示します。
    ImGui::Text(u8"所持金: %d", m_money);

    // ステータスとログの間を区切ります。
    ImGui::Separator();

    // 体力の数値を表示します。
    ImGui::Text(u8"体力: %.1f / 10", m_player.hp);

    // 体力バーを表示します。
    ImGui::ProgressBar(m_player.hp / 10.0f, ImVec2(-1.0f, 0.0f));

    // 精神力の数値を表示します。
    ImGui::Text(u8"精神力: %.1f / 100", m_player.mental);

    // 精神力バーを表示します。
    ImGui::ProgressBar(m_player.mental / 100.0f, ImVec2(-1.0f, 0.0f));

    // スタミナの数値を表示します。
    ImGui::Text(u8"スタミナ: %.1f / 100", m_player.stamina);

    // スタミナバーを表示します。
    ImGui::ProgressBar(m_player.stamina / 100.0f, ImVec2(-1.0f, 0.0f));

    // 現在重量と通常最大重量を表示します。
    ImGui::Text(u8"重量: %.1f / %.1f", GetCurrentWeight(), kMaxWeight);

    // 150%まで拾える仕様なので、バーは150%を上限として正規化します。
    ImGui::ProgressBar(std::min(1.5f, GetCurrentWeight() / kMaxWeight) / 1.5f, ImVec2(-1.0f, 0.0f));

    // 現在深度を表示します。
    ImGui::Text(u8"深度: %.2fm", m_player.depth);

    // 上昇負荷ゲージを表示します。プロトでは内部確認用に見せています。
    ImGui::Text(u8"上昇負荷: %.2f / %.2f", m_player.upperLoad, kUpperLoadLimit);

    // ロープ状態を表示します。
    ImGui::Text(u8"ロープ: %s", m_player.onRope ? u8"使用中" : u8"未使用");

    // 所持旧器数を表示します。
    ImGui::Text(u8"所持旧器: %d", static_cast<int>(m_inventory.size()));

    // ステータスとログの間を区切ります。
    ImGui::Separator();

    // 新しいログから最大7件だけ表示します。
    for (int i = static_cast<int>(m_messages.size()) - 1; i >= 0 && i >= static_cast<int>(m_messages.size()) - 7; --i)
    {
        // 1件分のログを折り返しありで表示します。
        ImGui::TextWrapped("%s", m_messages[i].c_str());
    }

    // HUDウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawInventory()
{
    // 所持品ウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(160.0f, 80.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(500.0f, 460.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウを開始します。
    ImGui::Begin(u8"所持品", nullptr, ImGuiWindowFlags_NoCollapse);

    // 所持旧器を一覧表示します。
    for (int i = 0; i < static_cast<int>(m_inventory.size()); ++i)
    {
        // 表示対象の旧器を取得します。
        const RelicItem& item = m_inventory[i];

        // Selectableに渡す表示文字列を作ります。
        char label[128];

        // 旧器名、重量、売値を1行にまとめます。
        std::snprintf(label, sizeof(label), u8"%s  重量 %.0f  売値 %d", item.name.c_str(), item.weight, item.value);

        // クリックされた旧器を選択状態にします。
        if (ImGui::Selectable(label, m_selectedInventory == i)) m_selectedInventory = i;
    }

    // 有効な旧器が選ばれている時だけ捨てるボタンを出します。
    if (m_selectedInventory >= 0 && m_selectedInventory < static_cast<int>(m_inventory.size()))
    {
        // 選択旧器を現在位置に捨てます。
        if (ImGui::Button(u8"選択した旧器を捨てる"))
        {
            // 所持品から地面旧器へ移します。
            DropInventoryItem(m_selectedInventory);

            // 削除後の添字ズレを避けるため選択を解除します。
            m_selectedInventory = -1;
        }
    }

    // タブ切り替えは今後の実装予定として表示だけ残します。
    ImGui::Text(u8"Q/Eのタブ切り替えは後で実装します。現在は地図ピン用ウィンドウを使います。");

    // 所持品ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawRelicPrompt()
{
    // 旧器確認ウィンドウは毎回画面中央付近に固定します。
    ImGui::SetNextWindowPos(ImVec2(420.0f, 220.0f), ImGuiCond_Always);

    // 旧器確認ウィンドウのサイズを固定します。
    ImGui::SetNextWindowSize(ImVec2(420.0f, 180.0f), ImGuiCond_Always);

    // 旧器確認ウィンドウを開始します。
    ImGui::Begin(u8"旧器を発見", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // 発見した旧器名を表示します。
    ImGui::Text(u8"%s を発見", m_pendingRelic.name.c_str());

    // 発見した旧器の重量を表示します。
    ImGui::Text(u8"重量: %.0f", m_pendingRelic.weight);

    // 拾った後の総重量を表示します。
    ImGui::Text(u8"拾得後重量: %.0f / 150", GetCurrentWeight() + m_pendingRelic.weight);

    // 150%上限を超えない場合だけ拾うボタンを出します。
    if (GetCurrentWeight() + m_pendingRelic.weight <= kPickupWeightLimit)
    {
        // 旧器を所持品に入れます。
        if (ImGui::Button(u8"拾う"))
        {
            // 発見中の旧器を所持品へ追加します。
            m_inventory.push_back(m_pendingRelic);

            // 探索モードへ戻ります。
            m_mode = Mode::Explore;

            // HUDログに取得を出します。
            AddMessage(u8"旧器を拾いました。");
        }

        // Leaveボタンを横並びにします。
        ImGui::SameLine();
    }

    // 150%上限を超える場合は拾えない理由を表示します。
    else
    {
        // 重量上限超過メッセージを表示します。
        ImGui::Text(u8"これ以上は重すぎて拾えません。");
    }

    // 旧器を拾わず地面に置いたままにします。
    if (ImGui::Button(u8"置いていく"))
    {
        // 発見中の旧器を地面旧器として再登録します。
        m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, true });

        // 探索モードへ戻ります。
        m_mode = Mode::Explore;

        // HUDログに放置を出します。
        AddMessage(u8"旧器をその場に置きました。");
    }

    // 旧器確認ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawResult()
{
    // リザルトウィンドウは毎回画面中央付近に固定します。
    ImGui::SetNextWindowPos(ImVec2(390.0f, 160.0f), ImGuiCond_Always);

    // リザルトウィンドウのサイズを固定します。
    ImGui::SetNextWindowSize(ImVec2(480.0f, 300.0f), ImGuiCond_Always);

    // 死亡と帰還でウィンドウタイトルを切り替えます。
    ImGui::Begin(m_mode == Mode::DeathResult ? u8"死亡リザルト" : u8"帰還リザルト", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // 帰還理由または死亡理由を表示します。
    ImGui::Text(u8"結果: %s", m_result.reason.c_str());

    // 最大到達深度を表示します。
    ImGui::Text(u8"最大深度: %.2fm", m_result.maxDepth);

    // 採掘数を表示します。
    ImGui::Text(u8"採掘した旧器: %d", m_result.minedCount);

    // 帰還リザルトなら売却情報を表示します。
    if (m_mode == Mode::ReturnResult)
    {
        // 持ち帰った旧器数を表示します。
        ImGui::Text(u8"持ち帰った旧器: %d", m_result.carriedRelics);

        // 売却額を表示します。
        ImGui::Text(u8"売却額: %d", m_result.saleAmount);

        // 売却して次の潜行へ進むボタンです。
        if (ImGui::Button(u8"すべて売却して再潜行")) SellAllAndRestart();
    }

    // 死亡リザルトならロスト情報を表示します。
    else
    {
        // 失った旧器数を表示します。
        ImGui::Text(u8"失った旧器: %d", m_result.lostRelics);

        // 再挑戦ボタンです。
        if (ImGui::Button(u8"再挑戦")) RestartAfterDeath();
    }

    // リザルトウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawMapControls()
{
    // マップピンウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(690.0f, 80.0f), ImGuiCond_FirstUseEver);

    // マップピンウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(360.0f, 360.0f), ImGuiCond_FirstUseEver);

    // マップピンウィンドウを開始します。
    ImGui::Begin(u8"地図ピン", nullptr, ImGuiWindowFlags_NoCollapse);

    // ミニマップ描画領域の左上座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // ミニマップ描画領域のサイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 260.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // ミニマップ背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 28, 28, 255));

    // ミニマップ外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 145, 145, 255));

    // プレイヤー位置をミニマップ座標へ変換します。
    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos);

    // プレイヤーを緑の点で描きます。
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));

    // 登録済みピンをミニマップに描きます。
    for (const Vec2& pin : m_pins)
    {
        // ピン座標をミニマップ座標へ変換します。
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin);

        // ピンを赤い点で描きます。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, IM_COL32(230, 80, 90, 255));
    }

    // マウスがミニマップ領域内にあるか調べます。
    bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));

    // ミニマップ上で右クリックされたらピン設置/削除を行います。
    if (hovered && IsMouseRightTrigger())
    {
        // 現在のマウス座標を取得します。
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // スクリーン座標をワールド座標へ変換してピン操作します。
        TogglePinAt(ScreenToWorld(canvasPos, canvasSize, { mouse.x, mouse.y }));
    }

    // ミニマップ描画領域ぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    // ピン操作説明を表示します。
    ImGui::Text(u8"右クリック: ピン設置/削除");

    // マップピンウィンドウを閉じます。
    ImGui::End();
}

float SceneNarakuProto::GetCurrentWeight() const
{
    // 初期装備のつるはし重量10から計算を始めます。
    float weight = 10.0f;

    // 所持している旧器の重量をすべて足します。
    for (const RelicItem& item : m_inventory) weight += item.weight;

    // 合計重量を返します。
    return weight;
}

float SceneNarakuProto::GetWeightRate() const
{
    // 最大重量100に対する割合を返します。
    return GetCurrentWeight() / kMaxWeight;
}

float SceneNarakuProto::GetMoveSpeed() const
{
    // まず通常歩行速度を基準にします。
    float speed = kWalkSpeed;

    // 重量70%以上では移動速度を25%下げます。
    if (GetWeightRate() >= 0.70f) speed *= 0.75f;

    // 重量補正済みの歩行速度を返します。
    return speed;
}

float SceneNarakuProto::GetStaminaCost(float baseCost) const
{
    // 重量90%以上ではスタミナ消費を2倍にします。
    return GetWeightRate() >= 0.90f ? baseCost * 2.0f : baseCost;
}

bool SceneNarakuProto::CanSpendStamina(float baseCost) const
{
    // 重量補正後の消費量を支払えるかどうかを返します。
    return m_player.stamina >= GetStaminaCost(baseCost) && m_player.stamina > 0.0f;
}

void SceneNarakuProto::SpendStamina(float baseCost)
{
    // 重量補正後の消費量を差し引き、0未満にならないようにします。
    m_player.stamina = std::max(0.0f, m_player.stamina - GetStaminaCost(baseCost));
}

bool SceneNarakuProto::IsNear(const Vec2& a, const Vec2& b, float range) const
{
    // 2点間距離が指定範囲内かを返します。
    return Distance(a, b) <= range;
}

SceneNarakuProto::Vec2 SceneNarakuProto::Normalize(const Vec2& value) const
{
    // ベクトルの長さを計算します。
    float length = std::sqrt(value.x * value.x + value.y * value.y);

    // 長さがほぼ0ならゼロ除算を避けてゼロベクトルを返します。
    if (length <= 0.0001f) return { 0.0f, 0.0f };

    // 各成分を長さで割って正規化します。
    return { value.x / length, value.y / length };
}

float SceneNarakuProto::Distance(const Vec2& a, const Vec2& b) const
{
    // X成分の差を計算します。
    float dx = a.x - b.x;

    // Y成分の差を計算します。
    float dy = a.y - b.y;

    // ピタゴラスの定理で距離を返します。
    return std::sqrt(dx * dx + dy * dy);
}

float SceneNarakuProto::Dot(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトルの内積を返します。
    return a.x * b.x + a.y * b.y;
}

SceneNarakuProto::Vec2 SceneNarakuProto::Add(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトル同士を加算します。
    return { a.x + b.x, a.y + b.y };
}

SceneNarakuProto::Vec2 SceneNarakuProto::Sub(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトル同士を減算します。
    return { a.x - b.x, a.y - b.y };
}

SceneNarakuProto::Vec2 SceneNarakuProto::Mul(const Vec2& a, float scalar) const
{
    // 2Dベクトルにスカラーを掛けます。
    return { a.x * scalar, a.y * scalar };
}

bool SceneNarakuProto::IsInsideFloor(const FloorRegion& floor, const Vec2& pos) const
{
    // 床矩形の左端から右端までに入っているかを調べます。
    const bool insideX = pos.x >= floor.center.x - floor.halfSize.x && pos.x <= floor.center.x + floor.halfSize.x;

    // 床矩形の奥端から手前端までに入っているかを調べます。
    const bool insideY = pos.y >= floor.center.y - floor.halfSize.y && pos.y <= floor.center.y + floor.halfSize.y;

    // XとYの両方が範囲内なら床上として扱います。
    return insideX && insideY;
}

const SceneNarakuProto::FloorRegion* SceneNarakuProto::FindFloorAt(const Vec2& pos, float depth) const
{
    // 深度がぴったり一致しなくても、ロープ移動直後の誤差を許容します。
    constexpr float kFloorDepthTolerance = 0.20f;

    // 登録された床を順番に調べ、位置と深度の両方が合う床を探します。
    for (const FloorRegion& floor : m_floorRegions)
    {
        // 深度差が許容範囲外なら、この床は候補から外します。
        if (std::fabs(floor.depth - depth) > kFloorDepthTolerance)
        {
            continue;
        }

        // 矩形内に入っている床を見つけたら、その床を返します。
        if (IsInsideFloor(floor, pos))
        {
            return &floor;
        }
    }

    // 対応する床がない場合は nullptr を返します。
    return nullptr;
}

bool SceneNarakuProto::HasFloorAt(const Vec2& pos, float depth) const
{
    // 床ポインタが見つかるかどうかだけを真偽値に変換します。
    return FindFloorAt(pos, depth) != nullptr;
}

SceneNarakuProto::Vec2 SceneNarakuProto::ResolveFloorMove(const Vec2& from, const Vec2& to, float depth) const
{
    // 目的地に床があるなら、そのまま移動します。
    if (HasFloorAt(to, depth))
    {
        return to;
    }

    // 斜め移動で角に当たった時は、X方向だけの移動を試します。
    const Vec2 xOnly = { to.x, from.y };
    if (HasFloorAt(xOnly, depth))
    {
        return xOnly;
    }

    // X方向が駄目なら、Y方向だけの移動を試します。
    const Vec2 yOnly = { from.x, to.y };
    if (HasFloorAt(yOnly, depth))
    {
        return yOnly;
    }

    // どの方向にも床がなければ、その場に留めます。
    return from;
}

int SceneNarakuProto::FindNearestRopeIndex(float range) const
{
    // 近いロープを選ぶため、現在の最短距離を範囲上限から始めます。
    float bestDistance = range;

    // 見つかったロープ番号です。未発見なら -1 のままにします。
    int bestIndex = -1;

    // 全ロープを調べ、範囲内で一番近いものを探します。
    for (int i = 0; i < static_cast<int>(m_ropePoints.size()); ++i)
    {
        // プレイヤーとロープ位置の平面距離を計算します。
        const float d = Distance(m_player.pos, m_ropePoints[i].pos);

        // 現在の候補より近ければ採用します。
        if (d <= bestDistance)
        {
            bestDistance = d;
            bestIndex = i;
        }
    }

    // 範囲内にロープがなければ -1 を返します。
    return bestIndex;
}

bool SceneNarakuProto::TryLeaveRopeSide(int ropeIndex, float leaveSign, const Vec2& cameraRight)
{
    // 無効なロープ番号なら何もしません。
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return false;
    }

    // 対象ロープを取得します。
    const RopePoint& rope = m_ropePoints[ropeIndex];

    // 現在深度の床へ横に降りる候補位置を作ります。
    const Vec2 leavePos = Add(rope.pos, Mul(cameraRight, leaveSign * 0.80f));

    // 候補位置に床があるなら、そこへ降ります。
    if (HasFloorAt(leavePos, m_player.depth))
    {
        m_player.pos = leavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 深度が端にかなり近い場合は端深度へ吸着して、降りられるかをもう一度試します。
    const float endpointDepth = std::fabs(m_player.depth - rope.bottomDepth) < std::fabs(m_player.depth - rope.topDepth) ? rope.bottomDepth : rope.topDepth;
    if (std::fabs(m_player.depth - endpointDepth) <= 0.35f && HasFloorAt(leavePos, endpointDepth))
    {
        m_player.depth = endpointDepth;
        m_player.pos = leavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 周囲に足場がない場合はロープから離れません。
    return false;
}

void SceneNarakuProto::AddMessage(const std::string& message)
{
    // 新しいログを末尾に追加します。
    m_messages.push_back(message);

    // ログが増えすぎないよう古いものを削除します。
    if (m_messages.size() > 24) m_messages.erase(m_messages.begin());
}

void SceneNarakuProto::DiscoverNearbyMiningPoints()
{
    // すべての採掘ポイントを確認します。
    for (MiningPoint& point : m_miningPoints)
    {
        // 未発見かつプレイヤーが近いポイントだけ発見済みにします。
        if (!point.discovered && IsNear(m_player.pos, point.pos, kDiscoverRange))
        {
            // 採掘ポイントを発見済みにします。
            point.discovered = true;

            // HUDログに発見を出します。
            AddMessage(u8"採掘ポイントを発見しました。");
        }
    }
}

void SceneNarakuProto::DropInventoryItem(int index)
{
    // 範囲外の番号なら何もしません。
    if (index < 0 || index >= static_cast<int>(m_inventory.size())) return;

    // 選択旧器を現在位置の地面旧器として追加します。
    m_groundRelics.push_back({ m_inventory[index], m_player.pos, true });

    // 所持品から選択旧器を削除します。
    m_inventory.erase(m_inventory.begin() + index);

    // HUDログに捨てたことを出します。
    AddMessage(u8"旧器を捨てました。");
}

void SceneNarakuProto::TogglePinAt(const Vec2& worldPos)
{
    // 既存ピンの近くなら削除扱いにします。
    for (int i = 0; i < static_cast<int>(m_pins.size()); ++i)
    {
        // クリック位置から1m以内のピンを削除対象にします。
        if (Distance(m_pins[i], worldPos) <= 1.0f)
        {
            // 対象ピンを削除します。
            m_pins.erase(m_pins.begin() + i);

            // HUDログに削除を出します。
            AddMessage(u8"ピンを削除しました。");

            // 削除したので追加処理は行いません。
            return;
        }
    }

    // 近くにピンがなければ新しいピンを追加します。
    m_pins.push_back(worldPos);

    // HUDログに追加を出します。
    AddMessage(u8"ピンを設置しました。");
}

SceneNarakuProto::Vec2 SceneNarakuProto::ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos) const
{
    // キャンバス内のX位置を0-1へ正規化します。
    float nx = (mousePos.x - canvasPos.x) / canvasSize.x;

    // キャンバス内のY位置を0-1へ正規化します。
    float ny = (mousePos.y - canvasPos.y) / canvasSize.y;

    // 正規化座標をワールド座標へ戻します。Yは画面上下とワールド上下が逆なので反転します。
    return { (nx * 2.0f - 1.0f) * kWorldHalfSize, ((1.0f - ny) * 2.0f - 1.0f) * kWorldHalfSize };
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos) const
{
    // ワールドX座標を0-1へ正規化します。
    float nx = (worldPos.x / kWorldHalfSize + 1.0f) * 0.5f;

    // ワールドY座標を0-1へ正規化します。画面Yは下向きなので反転します。
    float ny = 1.0f - (worldPos.y / kWorldHalfSize + 1.0f) * 0.5f;

    // 正規化座標をキャンバス上のスクリーン座標へ変換します。
    return { canvasPos.x + nx * canvasSize.x, canvasPos.y + ny * canvasSize.y };
}

DirectX::XMFLOAT3 SceneNarakuProto::ToWorld3D(const Vec2& pos, float depth, float heightOffset) const
{
    // 深度1mを3D空間の高さ0.35mぶんとして扱い、深く潜るほど下に表示します。
    constexpr float kDepthScale = 0.35f;

    // 2DのXは3DのXへ、2DのYは3DのZへ割り当てます。
    return { pos.x, heightOffset - depth * kDepthScale, pos.y };
}

void SceneNarakuProto::DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale, float yawRad) const
{
    using namespace DirectX;

    // 指定サイズへ拡大する行列を作ります。
    const XMMATRIX scaling = XMMatrixScaling(scale.x, scale.y, scale.z);

    // Y軸回転で向きを調整する行列を作ります。
    const XMMATRIX rotation = XMMatrixRotationY(yawRad);

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 拡大、回転、移動の順でワールド行列を組み立てます。
    const XMMATRIX worldMatrix = scaling * rotation * translation;

    // 既存Geometoryは行列を転置して渡す前提なので、ワールド行列も転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);

    // デバッグ箱を描画します。
    Geometory::DrawBox();
}

void SceneNarakuProto::DrawDebugSphere3D(const DirectX::XMFLOAT3& pos, float radius) const
{
    using namespace DirectX;

    // 半径をXYZ同じ倍率として扱い、球サイズを調整します。
    const XMMATRIX scaling = XMMatrixScaling(radius, radius, radius);

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 拡大してから移動するワールド行列を組み立てます。
    const XMMATRIX worldMatrix = scaling * translation;

    // 既存Geometoryは行列を転置して渡す前提なので、ワールド行列も転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);

    // デバッグ球を描画します。
    Geometory::DrawSphere();
}

void SceneNarakuProto::DrawTransparentFloor3D(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color) const
{
    using namespace DirectX;

    // 白テクスチャが作れていない場合は描画できないので抜けます。
    if (!m_debugWhiteTexture)
    {
        return;
    }

    // Spriteのデフォルトシェーダーを使うように戻します。
    Sprite::SetVertexShader(nullptr);
    Sprite::SetPixelShader(nullptr);

    // 半透明床は1x1白テクスチャに色を掛けて描きます。
    Sprite::SetTexture(m_debugWhiteTexture);

    // Spriteのローカルサイズを床サイズに合わせます。
    Sprite::SetSize(size);

    // Spriteの中心をローカル原点に置きます。
    Sprite::SetOffset({ 0.0f, 0.0f });

    // 1x1白テクスチャ全体を使います。
    Sprite::SetUVPos({ 0.0f, 0.0f });
    Sprite::SetUVScale({ 1.0f, 1.0f });

    // 呼び出し側から受け取ったRGBA色を適用します。
    Sprite::SetColor(color);

    // SpriteはXY平面なので、X軸回転でXZ水平面へ倒します。
    const XMMATRIX rotation = XMMatrixRotationX(XMConvertToRadians(90.0f));

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(center.x, center.y, center.z);

    // 回転してから移動するワールド行列を作ります。
    const XMMATRIX worldMatrix = rotation * translation;

    // 既存Spriteも行列を転置して渡す前提なので、転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Sprite::SetWorld(world);

    // 現在のGeometory用ビュー/射影と同じものがSpriteにも入っている前提で描画します。
    Sprite::Draw();
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToObliqueCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float depthOffset) const
{
    // 斜め見下ろし用にワールド座標を45度回したX成分へ変換します。
    float projectedX = worldPos.x - worldPos.y;

    // 斜め見下ろし用に奥行きを圧縮したY成分へ変換します。
    float projectedY = (worldPos.x + worldPos.y) * 0.45f;

    // 深度が大きいほど画面下へずらし、潜っている感覚を足します。
    projectedY += depthOffset * 0.35f;

    // 投影後のXをキャンバス幅に収まるよう0-1へ正規化します。
    float nx = (projectedX / (kWorldHalfSize * 2.0f) + 1.0f) * 0.5f;

    // 投影後のYをキャンバス高さに収まるよう0-1へ正規化します。
    float ny = (projectedY / (kWorldHalfSize * 1.35f) + 1.0f) * 0.5f;

    // 正規化座標をキャンバス上のスクリーン座標へ変換します。
    return { canvasPos.x + nx * canvasSize.x, canvasPos.y + ny * canvasSize.y };
}
