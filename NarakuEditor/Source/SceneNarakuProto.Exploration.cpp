/**
 * @file SceneNarakuProto.Exploration.cpp
 * @brief 探索更新、移動、戦闘、生存、死亡、および帰還処理を実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

void SceneNarakuProto::UpdateFrameEffects()
{
    m_centerNotificationTimer = std::max(0.0f, m_centerNotificationTimer - kDt);
    m_characterAnimationTime += kDt;
    for (JumpEffect& effect : m_jumpEffects)
    {
        effect.remainingTime -= kDt;
    }
    m_jumpEffects.erase(
        std::remove_if(
            m_jumpEffects.begin(),
            m_jumpEffects.end(),
            [](const JumpEffect& effect) { return effect.remainingTime <= 0.0f; }),
        m_jumpEffects.end());
}
void SceneNarakuProto::UpdateCollisionDebugToggle()
{
#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
    if (IsKeyTrigger('C'))
    {
        m_showCollisionDebug = !m_showCollisionDebug;
        AddMessage(m_showCollisionDebug ? "Collision Debug: ON" : "Collision Debug: OFF");
    }
#endif
}

void SceneNarakuProto::UpdateOverlayMenu()
{
    if (m_mode == Mode::Explore || m_mode == Mode::Surface)
    {
        if (IsActionMenuMapTrigger())
        {
            m_overlayReturnMode = m_mode;
            m_activeMenuTab = MenuTab::Map;
            m_inventoryMapShowingMap = true;
            m_mode = Mode::Inventory;
        }
        else if (IsActionMenuInventoryTrigger())
        {
            m_overlayReturnMode = m_mode;
            m_activeMenuTab = MenuTab::Inventory;
            m_inventoryMapShowingMap = false;
            m_mode = Mode::Inventory;
        }
        else if (IsActionMenuSettingsTrigger())
        {
            m_overlayReturnMode = m_mode;
            m_activeMenuTab = MenuTab::Settings;
            m_inputSettings.OnOpen();
            m_mode = Mode::Inventory;
        }
        return;
    }

    if (m_mode != Mode::Inventory || m_inputSettings.IsRebinding())
    {
        return;
    }

    if (IsActionUITabPrevTrigger() && !IsActionMenuMapTrigger() && !IsActionMenuSettingsTrigger())
    {
        if (m_activeMenuTab == MenuTab::Map) m_activeMenuTab = MenuTab::Settings;
        else if (m_activeMenuTab == MenuTab::Settings) m_activeMenuTab = MenuTab::Inventory;
        else m_activeMenuTab = MenuTab::Map;
        m_inventoryMapShowingMap = (m_activeMenuTab == MenuTab::Map);
        if (m_activeMenuTab == MenuTab::Settings) m_inputSettings.OnOpen();
    }
    else if (IsActionUITabNextTrigger() && !IsActionMenuMapTrigger() && !IsActionMenuSettingsTrigger())
    {
        if (m_activeMenuTab == MenuTab::Map) m_activeMenuTab = MenuTab::Inventory;
        else if (m_activeMenuTab == MenuTab::Inventory) m_activeMenuTab = MenuTab::Settings;
        else m_activeMenuTab = MenuTab::Map;
        m_inventoryMapShowingMap = (m_activeMenuTab == MenuTab::Map);
        if (m_activeMenuTab == MenuTab::Settings) m_inputSettings.OnOpen();
    }
    else if (IsActionMenuMapTrigger())
    {
        if (m_activeMenuTab == MenuTab::Map) { m_mode = m_overlayReturnMode; SetInputGuardActive(true); }
        else { m_activeMenuTab = MenuTab::Map; m_inventoryMapShowingMap = true; }
    }
    else if (IsActionMenuInventoryTrigger())
    {
        if (m_activeMenuTab == MenuTab::Inventory) { m_mode = m_overlayReturnMode; SetInputGuardActive(true); }
        else { m_activeMenuTab = MenuTab::Inventory; m_inventoryMapShowingMap = false; }
    }
    else if (IsActionMenuSettingsTrigger())
    {
        if (IsActionUIBackTrigger() || m_activeMenuTab == MenuTab::Settings) { m_mode = m_overlayReturnMode; SetInputGuardActive(true); }
        else { m_activeMenuTab = MenuTab::Settings; m_inputSettings.OnOpen(); }
    }
    else if (IsActionUIBackTrigger())
    {
        m_mode = m_overlayReturnMode;
        SetInputGuardActive(true);
    }
}

void SceneNarakuProto::UpdatePortableLightForActiveMode()
{
    if ((m_mode == Mode::Explore || m_mode == Mode::Surface) && IsActionToggleLightTrigger())
    {
        TogglePortableLight();
    }
    if (m_mode == Mode::Explore || m_mode == Mode::Surface)
    {
        UpdatePortableLight(kDt);
    }
}

void SceneNarakuProto::Update()
{
    if (m_screenFadePhase != ScreenFadePhase::None)
    {
        if (m_screenFadeAction == ScreenFadeAction::LayerTransition && m_mode == Mode::LayerTransition)
            UpdateLayerTransition(kDt);
        else
            UpdateScreenFade(kDt);
        return;
    }

    UpdateFrameEffects();

    if (m_mode == Mode::Loading)
    {
        UpdateLoading();
        return;
    }
    if (m_mode == Mode::LayerTransition)
    {
        UpdateLayerTransition(kDt);
        return;
    }

    UpdateWorldClockAndQuests(kDt);

    UpdateCollisionDebugToggle();
    UpdateOverlayMenu();
    UpdatePortableLightForActiveMode();

    // 探索モード中だけプレイヤーや敵などのゲーム更新を進めます。
    if (m_mode == Mode::Explore)
    {
        UpdateExplore(kDt);
    }
    else if (m_mode == Mode::Surface) UpdateSurface(kDt);
}

void SceneNarakuProto::UpdateExplore(float dt)
{
#if defined(NARAKU_EDITOR_BUILD)
    UpdateCameraControls();
    if (m_editorOverviewMode)
    {
        m_player.hp = GetMaxHp();
        m_player.stamina = GetMaxStamina();
        m_player.mental = GetMaxMental();
        m_fullness = kFullnessMaximum;
        m_hydration = kHydrationMaximum;
        return;
    }
    ClampDebugPlayerParams();
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.feetWorldY;
    UpdateMovement(dt);
    UpdateAction(dt);
    DiscoverNearbyMiningPoints();
    UpdateExplorationDiscovery();
    m_player.hp = GetMaxHp();
    m_player.stamina = GetMaxStamina();
    m_player.mental = GetMaxMental();
    m_fullness = kFullnessMaximum;
    m_hydration = kHydrationMaximum;
    if (IsActionInteractTrigger()) TryInteract();
    return;
#endif
    // 探索モード中だけ右ドラッグによるカメラ回転を受け付けます。
    UpdateCameraControls();

    // ImGui からの変更値が不正でもゲーム進行が壊れないよう毎フレーム丸めます。
    ClampDebugPlayerParams();

    // 今フレームの上昇量を後で計算できるよう、更新前の深度を保存します。
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.onRope && m_activeRope >= 0
        ? GetRopeWorldY(m_activeRope, m_ropeProgress)
        : m_player.feetWorldY;

    // 入力に応じてプレイヤーの移動と行動制限を更新します。
    UpdateMovement(dt);
    UpdateMentalAbilities(dt);

    // 攻撃タイマーとスタミナ回復を更新します。
    UpdateAction(dt);

    // 採掘中なら採掘タイマーを進めます。
    UpdateMining(dt);
    UpdateHunger(dt);
    UpdateHydration(dt);
    UpdateCooking(dt);
    UpdateFishing(dt);
    if (m_mode != Mode::Explore) return;

    // 敵の追跡と体当たりを更新します。
    UpdateEnemies(dt);

    const EquipmentBonus currentEquipmentBonus = GetEquipmentBonus();
    const float hpRecoveryPerSecond = currentEquipmentBonus.hpRecoveryPerSecond +
        GetMaxHp() * currentEquipmentBonus.hpRecoveryMaxRatioPerSecond;
    if (hpRecoveryPerSecond > 0.0f && m_player.hp > 0.0f)
    {
        m_player.hp = std::min(GetMaxHp(), m_player.hp + hpRecoveryPerSecond * dt);
    }

    // 近くの未発見採掘ポイントを発見済みにします。
    DiscoverNearbyMiningPoints();
    UpdateExplorationDiscovery();

    // 深度変化から上昇負荷を更新します。
    UpdateUpperLoad(dt);
    if (m_mode != Mode::Explore) return;
    UpdateUpperLoadEffects(dt);
    if (m_mode != Mode::Explore) return;

    // リザルト用に今回の最大深度を記録します。
    m_result.maxDepth = std::max(m_result.maxDepth, GetCurrentDepth());
    const int currentDepth = GetCurrentDepth();
    if (currentDepth > m_maxReachedDepth)
    {
        m_maxReachedDepth = currentDepth;
        RefreshPromotionQuestAvailability();
        SaveProgress();
    }
    m_result.staySecondsByDepth[static_cast<std::size_t>(currentDepth - 1)] += dt;

    // Fキーで帰還、ロープ、拾う、採掘のいずれかを試します。
    if (IsActionInteractTrigger() && m_fishingPhase == FishingPhase::None) TryInteract();

    // 体力が0以下になったら死亡リザルトへ移行します。
    if (m_player.hp <= 0.0f && m_mode == Mode::Explore) StartDeath(u8"体力が0になりました。", DeathCause::Other);
}

SceneNarakuProto::PresentationScene SceneNarakuProto::GetPresentationScene() const
{
    if (m_mode == Mode::SaveError)
    {
        return m_saveErrorPresentation;
    }
    if (m_mode == Mode::Surface || m_mode == Mode::Home || m_mode == Mode::GeneralShop ||
        m_mode == Mode::Armory || m_mode == Mode::Restaurant || m_mode == Mode::QuestDesk ||
        m_mode == Mode::AbyssEntrance)
    {
        return PresentationScene::Town;
    }
    if (m_mode == Mode::ReturnResult || m_mode == Mode::DeathResult)
    {
        return PresentationScene::Result;
    }
    return PresentationScene::Dive;
}

SceneNarakuProto::RunInputState SceneNarakuProto::UpdateRunInput(
    float dt,
    bool isMining,
    bool inLandingRecovery)
{
    RunInputState result;
    result.shiftPressed = !isMining && IsShiftPress();
    const bool shiftStarted = result.shiftPressed && !m_shiftWasPressed;
    const bool shiftReleased = !result.shiftPressed && m_shiftWasPressed;

    if (shiftStarted)
    {
        m_shiftPendingStep = true;
        m_shiftHold = 0.0f;
        m_shiftRunCommitted = false;
    }

    if (m_shiftPendingStep)
    {
        m_shiftHold += dt;
        if (shiftReleased)
        {
            if (!m_shiftRunCommitted && !inLandingRecovery)
            {
                TryStartStep();
            }
            m_shiftPendingStep = false;
            m_shiftRunCommitted = false;
        }
        else if (m_shiftHold >= kShiftRunThreshold)
        {
            result.wantsRun = true;
            m_shiftRunCommitted = true;
        }
    }
    else if (result.shiftPressed)
    {
        result.wantsRun = true;
    }

    if (!result.shiftPressed)
    {
        m_shiftPendingStep = false;
        m_shiftHold = 0.0f;
        m_shiftRunCommitted = false;
    }
    return result;
}
bool SceneNarakuProto::UpdateRopeMovement(
    float dt,
    float inputY,
    bool isMining,
    const Vec2& cameraRight,
    const Vec2& frameStartPos)
{
    // ロープに掴まっている時はW/Sを深度操作として扱います。
    if (m_player.onRope)
    {
        // ロープ番号が無効なら、操作不能にならないよう即座にロープ状態を解除します。
        if (m_activeRope < 0 || m_activeRope >= static_cast<int>(m_ropePoints.size()))
        {
            m_player.onRope = false;
            m_activeRope = -1;
            m_lastFrameMovementDistance = Distance(frameStartPos, m_player.pos);
            return true;
        }

        // 現在つかまっているロープの上端/下端深度を参照します。
        const RopePoint& rope = m_ropePoints[m_activeRope];

        // 深度入力を一時的に保持します。
        float depthInput = 0.0f;

        // 昇降入力（W/Sキー、およびパッドの左スティック・十字キー）
        if (!isMining && inputY > 0.1f) depthInput -= 1.0f;
        if (!isMining && inputY < -0.1f) depthInput += 1.0f;

        // A/Dが押された瞬間はスタミナがなくてもロープから横へ離脱できるようにします。
        bool leaveLeft = false, leaveRight = false;
        GetActionMoveTriggers(leaveLeft, leaveRight);
        const bool wantsLeaveRope = !isMining && (leaveLeft || leaveRight);

        // ロープから離れる時は、掴まり状態を解除して少しだけ横へずらします。
        if (wantsLeaveRope)
        {
            // Aなら左、Dなら右へ離れる方向を決めます。
            const float leaveSign = leaveLeft ? -1.0f : 1.0f;

            // 現在深度に横へ降りられる床がある時だけロープを離します。
            if (!TryLeaveRopeSide(m_activeRope, leaveSign, cameraRight))
            {
                AddMessage(u8"足場がないためロープを離せません。");
            }
        }

        // 深度入力があり、スタミナを払える時だけ昇降します。
        if (m_player.onRope && depthInput != 0.0f && CanSpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt))
        {
            m_lastFrameRopeMoving = true;
            // ロープ昇降のスタミナを消費します。
            SpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt);

            const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);
            const float horizontalLength = Distance(traversal.topPosition, traversal.bottomPosition);
            const float verticalLength = traversal.bottomWorldY - traversal.topWorldY;
            const float ropeLength = std::max(0.001f, std::sqrt(horizontalLength * horizontalLength + verticalLength * verticalLength));
            const float newProgress = m_ropeProgress + depthInput * GetRopeSpeed(depthInput < 0.0f) * dt / ropeLength;

            if (newProgress <= 0.0f && depthInput < 0.0f)
            {
                m_ropeProgress = 0.0f;
                m_player.depth = rope.topDepth;
                m_player.pos = rope.topPos;
                m_player.onRope = false;
                m_activeRope = -1;
                m_player.grounded = true;
                m_player.verticalSpeed = 0.0f;
                m_player.airTime = 0.0f;
                m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
                m_player.peakFeetWorldY = m_player.feetWorldY;
                m_player.landingRecoveryTimer = 0.0f;
                AddMessage(u8"ロープの上端に到達し、足場へ降りました。");
            }
            else
            {
                const float targetProgress = std::max(0.0f, std::min(1.0f, newProgress));
                const Vec2 targetPosition = GetRopePosition(m_activeRope, targetProgress);
                const float targetFeetWorldY = GetRopePlayerFeetWorldY(m_activeRope, targetProgress);
                const int bottomLayerIndex = FindLayerIndexAt(targetPosition, rope.bottomDepth);
                const bool touchesBottomGround = depthInput > 0.0f && bottomLayerIndex >= 0 &&
                    targetFeetWorldY <= GetGroundWorldY(targetPosition, rope.bottomDepth) + kSlopeHeightTolerance;

                if (touchesBottomGround && CanStandAt(targetPosition, rope.bottomDepth))
                {
                    m_ropeProgress = targetProgress;
                    m_player.depth = rope.bottomDepth;
                    m_player.pos = targetPosition;
                    m_player.onRope = false;
                    m_activeRope = -1;
                    m_player.grounded = true;
                    m_player.verticalSpeed = 0.0f;
                    m_player.airTime = 0.0f;
                    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
                    m_player.peakFeetWorldY = m_player.feetWorldY;
                    m_player.landingRecoveryTimer = 0.0f;
                    AddMessage(u8"足が地面に着いたため、ロープから降りました。");
                }
                else if (!touchesBottomGround)
                {
                    m_ropeProgress = targetProgress;
                    m_player.pos = targetPosition;
                    m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
                    m_player.feetWorldY = targetFeetWorldY;
                }
            }
        }
    }

    return false;
}

void SceneNarakuProto::UpdateBlockedTerrainFall(
    float dt,
    const Vec2& groundedMoveStartPos,
    bool startedOverBlockedCell,
    bool& suppressMovementDistance)
{
    // 歩行不可セルへ飛び込んだ時の横移動速度を保存します。
    const Vec2 airborneMoveDelta = Sub(m_player.pos, groundedMoveStartPos);
    if (!m_player.grounded && !startedOverBlockedCell && dt > 0.0f &&
        Distance(groundedMoveStartPos, m_player.pos) > 0.001f)
    {
        m_player.blockedCellVelocity = Mul(airborneMoveDelta, 1.0f / dt);
    }

    std::uint32_t airborneCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
    if (!m_player.grounded && !m_player.onRope &&
        (airborneCellFlags & NarakuMap::CellAttributeBlocked) != 0u)
    {
        m_player.blockedCellAirTime += dt;

        const bool overLake = (airborneCellFlags & NarakuMap::CellAttributeWaterLake) != 0u;
        if (!overLake)
        {
            const Vec2 downhill = GetTerrainDownhillDirection(m_player.pos, m_player.depth);
            if (Distance({}, downhill) > 0.001f)
            {
                m_player.blockedCellVelocity = Mul(downhill, GetMoveSpeed());
            }
            else if (Distance({}, m_player.blockedCellVelocity) <= 0.001f)
            {
                m_player.blockedCellVelocity = Mul(m_player.facing, GetMoveSpeed());
            }

            const Vec2 slideStart = m_player.pos;
            const Vec2 slideTarget = Add(slideStart, Mul(m_player.blockedCellVelocity, dt));
            m_player.pos = ResolveAirMove(slideStart, slideTarget, m_player.depth);
            if (Distance(slideStart, m_player.pos) <= 0.001f)
            {
                m_player.blockedCellVelocity = Mul(m_player.blockedCellVelocity, -1.0f);
                m_player.pos = ResolveAirMove(
                    slideStart, Add(slideStart, Mul(m_player.blockedCellVelocity, dt)), m_player.depth);
            }
        }

        airborneCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
        const bool stillBlocked = (airborneCellFlags & NarakuMap::CellAttributeBlocked) != 0u;
        const bool stillOverLake = (airborneCellFlags & NarakuMap::CellAttributeWaterLake) != 0u;
        float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        const bool submergedInLake = stillOverLake && m_player.feetWorldY <= groundWorldY - kLakeFallDepth;
        const bool timedOut = stillBlocked && m_player.blockedCellAirTime >= kBlockedCellReturnTime;

        if (submergedInLake || timedOut)
        {
            if (RestorePlayerToSafeGround())
            {
                suppressMovementDistance = true;
                if (submergedInLake)
                {
                    const float damage = GetMaxHp() * kLakeFallDamageRatio;
                    const bool preventedByRelic =
                        m_player.hp - damage <= 0.0f && TryConsumeSurvivalRelic(true, false);
                    if (!preventedByRelic)
                    {
                        m_player.hp = std::max(0.0f, m_player.hp - damage);
                        if (m_player.hp <= 0.0f)
                        {
                            StartDeath(u8"湖への落下で死亡しました。", DeathCause::Other);
                        }
                    }
                    AddMessage(preventedByRelic
                        ? u8"湖へ落下しましたが、生存的遺物が致命傷を防ぎました。"
                        : u8"湖へ落下し、安全な場所へ戻されました。最大HPの15%のダメージを受けました。");
                }
                else
                {
                    AddMessage(u8"歩行可能な場所へ戻れなかったため、安全な場所へ復帰しました。");
                }
            }
        }
    }
    else if (!m_player.grounded)
    {
        m_player.blockedCellAirTime = 0.0f;
    }

}

void SceneNarakuProto::UpdateAirborneMotion(
    float dt,
    const Vec2& groundedMoveStartPos,
    float groundedMoveStartGroundY,
    bool suppressMovementDistance)
{
    // 地面を歩いてより低い地形へ出た時は、一定以上の落差で落下状態へ移ります。
    if (m_player.grounded && !m_player.onRope && !suppressMovementDistance)
    {
        const float currentGroundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        const float walkedDropHeight = groundedMoveStartGroundY - currentGroundWorldY;
        const bool movedHorizontally = Distance(groundedMoveStartPos, m_player.pos) > 0.01f;

        if (movedHorizontally && walkedDropHeight >= m_autoFallStartHeight)
        {
            m_player.grounded = false;
            m_player.airTime = 0.0f;
            m_player.verticalSpeed = 0.0f;
            m_player.feetWorldY = groundedMoveStartGroundY;
            m_player.peakFeetWorldY = groundedMoveStartGroundY;
            m_player.landingRecoveryTimer = 0.0f;
        }
    }

    // ジャンプ中なら実地形の高さを使って空中時間と着地を更新します。
    if (!m_player.grounded && !m_player.onRope)
    {
        float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);

        // 空中にいる時間を加算します。
        m_player.airTime += dt;

        const float previousFeetWorldY = m_player.feetWorldY;
        // 足元の絶対ワールド高さを縦速度ぶん進めます。
        m_player.feetWorldY += m_player.verticalSpeed * dt;

        ResolveEnvironmentVerticalCollision(
            m_player.pos,
            previousFeetWorldY,
            m_player.feetWorldY,
            m_player.verticalSpeed,
            0.30f,
            1.40f);
        groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);

        // 重力で縦速度を減らします。
        m_player.verticalSpeed -= 9.8f * dt;

        // 最高到達点を更新して、着地時の落下距離計算に使います。
        m_player.peakFeetWorldY = std::max(m_player.peakFeetWorldY, m_player.feetWorldY);

        const std::uint32_t landingCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
        const bool canLand = (landingCellFlags &
            (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved)) == 0u;

        // 降下中に歩行可能な地面へ届いたら着地します。
        if (canLand && m_player.verticalSpeed <= 0.0f && m_player.feetWorldY <= groundWorldY)
        {
            const float fallDistance = std::max(0.0f, m_player.peakFeetWorldY - groundWorldY);
            float landingRecovery = 0.0f;
            if (fallDistance >= 6.0f) landingRecovery = kLandingRecoveryHeavy;
            else if (fallDistance >= 4.0f) landingRecovery = kLandingRecoveryMedium;
            else if (fallDistance >= 2.0f) landingRecovery = kLandingRecoveryLight;

            constexpr float safeFallDistance = 2.0f;
            constexpr float lethalFallDistance = 20.0f;
            if (fallDistance >= lethalFallDistance)
            {
                StartDeath(u8"落下死しました。", DeathCause::Fall);
            }
            else if (fallDistance > safeFallDistance)
            {
                const float damageRatio = (fallDistance - safeFallDistance) /
                    (lethalFallDistance - safeFallDistance);
                const float fallDamage = GetMaxHp() * damageRatio;
                m_player.hp = std::max(0.0f, m_player.hp - fallDamage);
                if (m_player.hp <= 0.0f)
                {
                    StartDeath(u8"落下ダメージで死亡しました。", DeathCause::Fall);
                }
            }

            m_player.grounded = true;
            m_player.airTime = 0.0f;
            m_player.verticalSpeed = 0.0f;
            m_player.feetWorldY = groundWorldY;
            m_player.peakFeetWorldY = groundWorldY;
            m_player.landingRecoveryTimer = landingRecovery;
        }
    }
    else if (!m_player.onRope)
    {
        const float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.feetWorldY = groundWorldY;
        m_player.peakFeetWorldY = groundWorldY;
    }

}

void SceneNarakuProto::UpdateMovementBoundsAndHazard(float dt)
{
    // デバッグフィールド外へ出ないようX座標を制限します。
    m_player.pos.x = std::max(-m_worldHalfSize, std::min(m_player.pos.x, m_worldHalfSize));

    // デバッグフィールド外へ出ないようY座標を制限します。
    m_player.pos.y = std::max(-m_worldHalfSize, std::min(m_player.pos.y, m_worldHalfSize));

    // 危険地形に乗っている間は一定間隔でダメージを受けます。
    if (!m_player.onRope && (GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeHazard) != 0u)
    {
        m_hazardTickTimer -= dt;
        if (m_hazardTickTimer <= 0.0f)
        {
            m_hazardTickTimer += kHazardTickInterval;
            ApplyPlayerDamage(kHazardDamage, DeathCause::Other, u8"危険地形で死亡しました。");
            AddMessage(u8"危険地形でダメージを受けました。");
        }
    }
    else
    {
        m_hazardTickTimer = kHazardTickInterval;
    }

}

void SceneNarakuProto::UpdateMovement(float dt)
{
    const Vec2 frameStartPos = m_player.pos;
    bool suppressMovementDistance = false;
    m_lastFrameRunning = false;
    m_lastFrameRopeMoving = false;

    // 通常の床に立っている位置だけを、歩行不可セルからの復帰先として保存します。
    if (m_player.grounded && !m_player.onRope && HasFloorAt(m_player.pos, m_player.depth))
    {
        m_player.lastSafeGroundPos = m_player.pos;
        m_player.lastSafeGroundDepth = m_player.depth;
        m_player.hasSafeGroundPos = true;
        m_player.blockedCellAirTime = 0.0f;
        m_player.blockedCellVelocity = {};
    }

    const bool startedOverBlockedCell =
        (GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeBlocked) != 0u;
    // WASD入力を集めるための移動ベクトルです。
    Vec2 input;

    // 採掘中は移動やジャンプ、ステップ入力を受け付けません。
    const bool isMining = m_miningIndex >= 0;

    // 着地直後の硬直を更新します。
    m_player.landingRecoveryTimer = std::max(0.0f, m_player.landingRecoveryTimer - dt);
    const bool inLandingRecovery = m_player.landingRecoveryTimer > 0.0f;

    // Wキーでカメラから見た前方向へ進みます。
    if (!isMining) { GetActionMoveVector(input.x, input.y); }

    // Sキーでカメラから見た後ろ方向へ進みます。


    // Aキーでカメラから見た左方向へ進みます。


    // Dキーでカメラから見た右方向へ進みます。


    // カメラから見た前方向を、斜め投影で画面上方向に見えるワールド方向へ対応させます。
    const Vec2 cameraForward = GetCameraForward();

    // カメラから見た右方向を、現在の3Dカメラで画面右方向に見えるワールド方向へ対応させます。
    const Vec2 cameraRight = GetCameraRight();

    // 入力をカメラ基準方向からワールド移動方向へ変換します。
    Vec2 move = Add(Mul(cameraRight, input.x), Mul(cameraForward, input.y));

    // 斜め移動が速くならないように正規化します。
    move = Normalize(move);

    // 入力がある時だけ向きを更新して、停止中の攻撃方向を維持します。
    if (move.x != 0.0f || move.y != 0.0f) m_player.facing = move;

    // Spaceが押された瞬間にジャンプ開始を試します。
    if (!isMining && !inLandingRecovery && IsActionJumpTrigger()) TryStartJump();

    const RunInputState runInput = UpdateRunInput(dt, isMining, inLandingRecovery);
    const bool shiftPressed = runInput.shiftPressed;
    const bool wantsRun = runInput.wantsRun;

    // 地面歩行開始時点の位置と高さを覚えて、段差踏み外し時の落下開始判定に使います。
    const Vec2 groundedMoveStartPos = m_player.pos;
    const float groundedMoveStartGroundY = GetGroundWorldY(m_player.pos, m_player.depth);

    // ノックバック中は敵から押し出される移動を先に適用します。
    if (m_player.knockbackTimer > 0.0f)
    {
        // ノックバック速度ぶんプレイヤー座標をずらします。
        const Vec2 knockbackTarget = Add(m_player.pos, Mul(m_player.knockbackVelocity, dt));
        m_player.pos = m_player.grounded
            ? ResolveFloorMove(m_player.pos, knockbackTarget, m_player.depth)
            : ResolveAirMove(m_player.pos, knockbackTarget, m_player.depth);

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
            m_player.pos = m_player.grounded
                ? ResolveFloorMove(m_player.pos, stepTarget, m_player.depth)
                : ResolveAirMove(m_player.pos, stepTarget, m_player.depth);
        }
    }

    // ロープに掴まっていない時だけ平面移動を行います。
    else if (!m_player.onRope && !inLandingRecovery)
    {
        // 通常歩行速度に重量70%以上の低下を反映します。
        float speed = GetMoveSpeed();

        // 100%以上の重量では走れないため、ここで走行可否を判定します。
        const bool tooHeavyToRun = GetCurrentWeight() >= GetMaxWeight();
        const std::uint32_t currentCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
        const bool inPond = (currentCellFlags & NarakuMap::CellAttributeWaterPond) != 0u;
        bool canRun = !tooHeavyToRun && !inPond && m_player.stamina > 0.0f;

        if (wantsRun && tooHeavyToRun && (move.x != 0.0f || move.y != 0.0f))
        {
            if (!m_heavyRunNotificationShown)
            {
                ShowCenterNotification(u8"重すぎて走れない！");
                m_heavyRunNotificationShown = true;
            }
        }
        else
        {
            m_heavyRunNotificationShown = false;
        }

        // 走り入力、移動入力、重量、スタミナをすべて満たした時だけ走ります。
        if (wantsRun && canRun && (move.x != 0.0f || move.y != 0.0f))
        {
            // 走り速度へ切り替えます。
            speed = GetRunSpeed();
            m_lastFrameRunning = true;

            // 1フレームぶんの走りスタミナを消費します。
            SpendStamina(m_debugPlayerParams.runCostPerSecond * dt);
        }
        else if (inPond && (move.x != 0.0f || move.y != 0.0f))
        {
            SpendStamina(m_debugPlayerParams.runCostPerSecond * dt);
        }

        // 決まった速度で平面座標を進めます。
        if (m_player.grounded || !startedOverBlockedCell)
        {
            const Vec2 moveTarget = Add(m_player.pos, Mul(move, speed * dt));
            m_player.pos = m_player.grounded
                ? ResolveFloorMove(m_player.pos, moveTarget, m_player.depth)
                : ResolveAirMove(m_player.pos, moveTarget, m_player.depth);
        }
    }

    if (UpdateRopeMovement(dt, input.y, isMining, cameraRight, frameStartPos))
    {
        return;
    }

    UpdateBlockedTerrainFall(dt, groundedMoveStartPos, startedOverBlockedCell, suppressMovementDistance);
    UpdateAirborneMotion(dt, groundedMoveStartPos, groundedMoveStartGroundY, suppressMovementDistance);
    UpdateMovementBoundsAndHazard(dt);

    m_lastFrameMovementDistance = suppressMovementDistance ? 0.0f : Distance(frameStartPos, m_player.pos);

    // 次フレームで押下/離上を判定できるよう、現在のShift状態を保存します。
    m_shiftWasPressed = shiftPressed;
}

void SceneNarakuProto::UpdateFoodUse(float dt)
{
    if (m_foodUseTimer <= 0.0f)
    {
        return;
    }

    m_foodUseTimer = std::max(0.0f, m_foodUseTimer - dt);
    if (m_foodUseTimer > 0.0f ||
        !((m_usingHeatedFood && m_heatedFoodCount > 0) || (!m_usingHeatedFood && m_foodCount > 0)))
    {
        return;
    }

    if (m_usingHeatedFood && m_heatedFoodCount > 0)
    {
        --m_heatedFoodCount;
        m_player.hp = std::min(GetMaxHp(), m_player.hp + kHeatedFoodHpRecovery);
        m_fullness = std::min(kFullnessMaximum, m_fullness + kHeatedFoodFullnessRecovery);
        m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
        m_player.mental = std::min(GetMaxMental(), m_player.mental + kHeatedFoodMentalRecovery);
        AddMessage(u8"加熱食料でHP40、満腹度25、水分75、精神力5を回復しました。");
    }
    else if (!m_usingHeatedFood)
    {
        --m_foodCount;
        m_player.hp = std::min(GetMaxHp(), m_player.hp + kFoodHpRecovery);
        m_fullness = std::min(kFullnessMaximum, m_fullness + kFoodFullnessRecovery);
        m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
        AddMessage(u8"食料でHP20、満腹度10、水分75を回復しました。");
    }
    m_usingHeatedFood = false;
}

void SceneNarakuProto::UpdateAttackEffects(float dt)
{
    m_cameraShakeTimer = std::max(0.0f, m_cameraShakeTimer - dt);
    for (AttackHitEffect& effect : m_attackHitEffects)
    {
        effect.remainingTime -= dt;
    }
    m_attackHitEffects.erase(
        std::remove_if(
            m_attackHitEffects.begin(),
            m_attackHitEffects.end(),
            [](const AttackHitEffect& effect) { return effect.remainingTime <= 0.0f; }),
        m_attackHitEffects.end());

}

void SceneNarakuProto::UpdateMeleeAttack(float dt)
{
    if (m_player.attackTimer <= 0.0f)
    {
        return;
    }

    const float previous = m_player.attackTimer;
    m_player.attackTimer = std::max(0.0f, m_player.attackTimer - dt);
    const float elapsed = kAttackTotal - m_player.attackTimer;
    const float previousElapsed = kAttackTotal - previous;
    const bool activeThisFrame = previousElapsed < kAttackStartup + kAttackActive && elapsed >= kAttackStartup;
    if (!activeThisFrame)
    {
        return;
    }

    Vec2 relicCenter = Add(m_player.pos, Mul(m_player.facing, kAttackRange));
    float relicDepth = m_player.depth;
    bool foundPrimaryHit = false;
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive || enemy.hitByPlayerAttack) continue;
        const Vec2 toEnemy = Sub(enemy.pos, m_player.pos);
        if (Distance(enemy.pos, m_player.pos) <= kAttackRange && Dot(Normalize(toEnemy), m_player.facing) >= 0.866025f)
        {
            enemy.hitByPlayerAttack = true;
            m_attackHitEffects.push_back({ enemy.pos, enemy.depth, kAttackHitEffectDuration });
            enemy.hp -= GetAttackPower();
            if (!foundPrimaryHit)
            {
                relicCenter = enemy.pos;
                relicDepth = enemy.depth;
                foundPrimaryHit = true;
            }
        }
    }

    if (m_attackRelicTriggered)
    {
        m_attackHitEffects.push_back({ relicCenter, relicDepth, kAttackHitEffectDuration });
        for (EnemyState& enemy : m_enemies)
        {
            if (!enemy.alive || enemy.hitByRelicAttack || std::fabs(enemy.depth - relicDepth) > 0.35f ||
                Distance(enemy.pos, relicCenter) > kRelicAttackRadius) continue;
            enemy.hitByRelicAttack = true;
            enemy.hp -= GetAttackPower() * kRelicAttackDamageScale;
        }
    }

    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive || enemy.hp > 0.0f) continue;
        enemy.alive = false;
        enemy.respawnTimer = kEnemyRespawnTime;
        m_groundFoods.push_back({ enemy.pos, enemy.depth, true });
        AwardEnemyDefeat(enemy);
        AddMessage(u8"敵を倒しました。食料を落としました。");
    }
}

void SceneNarakuProto::UpdateStaminaRecovery(float dt)
{
    if (m_player.stamina >= GetMaxStamina() || m_player.attackTimer > 0.0f || m_miningIndex >= 0)
    {
        return;
    }

    const bool ropeClimbing = m_player.onRope && m_lastFrameRopeMoving;
    const bool spending = IsShiftPress() || ropeClimbing;
    if (!spending)
    {
        m_player.stamina = std::min(GetMaxStamina(), m_player.stamina +
            m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier() *
            GetDepthLevelStaminaRecoveryMultiplier() * dt);
    }
}

void SceneNarakuProto::UpdateAction(float dt)
{
    m_rationFullnessWardTimer = std::max(0.0f, m_rationFullnessWardTimer - dt);
    m_rationHydrationPenaltyTimer = std::max(0.0f, m_rationHydrationPenaltyTimer - dt);
    m_unknownWeaponCooldownTimer = std::max(0.0f, m_unknownWeaponCooldownTimer - dt);

    UpdateFoodUse(dt);
    UpdateAttackEffects(dt);
    UpdateMeleeAttack(dt);

    if (m_equippedWeapon == WeaponTier::Unknown)
    {
        UpdateUnknownWeaponAttack(dt);
    }
    else
    {
        m_unknownWeaponChargeTimer = 0.0f;
        m_unknownWeaponFiredThisHold = false;
        // 左クリックが押された瞬間に攻撃開始を試します。
        if (IsActionAttackTrigger())
        {
            if (GetLastActiveDevice() == ActiveInputDevice::KeyboardMouse)
            {
                UpdateAimDirectionFromMouse();
            }
            TryStartAttack();
        }
    }

    UpdateStaminaRecovery(dt);
}

void SceneNarakuProto::UpdateMining(float dt)
{
    // 採掘中でなければ何もしません。
    if (m_miningIndex < 0) return;

    // 被弾ノックバック、足場喪失（空中）、またはロープに掴まった場合は採掘を中断します。
    if (m_player.knockbackTimer > 0.0f)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        AddMessage(u8"ダメージを受けたため、採掘が中断されました。");
        return;
    }
    if (!m_player.grounded)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        AddMessage(u8"足場を失ったため、採掘が中断されました。");
        return;
    }
    if (m_player.onRope)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        return;
    }

    // 採掘完了までの残り時間を減らします。
    m_miningTimer -= dt;

    // まだ採掘が終わっていなければ戻ります。
    if (m_miningTimer > 0.0f) return;

    // 採掘中だったポイントを取得します。
    MiningPoint& point = m_miningPoints[m_miningIndex];

    // このポイントを採掘済みにします。
    point.mined = true;
    point.discovered = true;
    point.respawnTimer = kMiningPointRespawnTime;
    ++point.extractionCount;
    point.outputPending = true;

    // リザルト用の採掘数を増やします。
    ++m_result.minedCount;
    const int depth = GetCurrentDepth();
    ++m_result.minedByDepth[static_cast<std::size_t>(depth - 1)];
    AwardExp(static_cast<int>(std::round(20.0f * GetDepthExpMultiplier(depth))));

    // 発見確認に出す旧器を作ります。
    // 種類と重量は採掘した時点で確定し、鑑定状態は表示名だけに反映します。
    const std::uint64_t areaKey = m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size())
        ? (static_cast<std::uint64_t>(m_areas[static_cast<std::size_t>(m_currentAreaIndex)].depth) << 48) ^
            (static_cast<std::uint64_t>(m_areas[static_cast<std::size_t>(m_currentAreaIndex)].sublayer) << 40) ^
            (static_cast<std::uint64_t>(m_areas[static_cast<std::size_t>(m_currentAreaIndex)].areaNumber) << 32)
        : 0;
    SeedRuntimeRandom(m_diveWorldSeed ^ areaKey ^
        (static_cast<std::uint64_t>(m_miningIndex + 1) << 16) ^ point.extractionCount);
    m_pendingRelic = CreateRandomRelic(point.relicName);
    m_runRelicAcquisitionDepths[m_pendingRelic.acquisitionOrder] = depth;

    // 拾わず置く場合に戻す位置を採掘ポイント位置にします。
    m_pendingRelicPos = point.pos;
    m_pendingRelicDepth = point.depth;
    m_pendingRelicMiningIndex = m_miningIndex;

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
    const int activity = GetCurrentActivity();
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive) continue;
        enemy.landingRecoveryTimer = std::max(0.0f, enemy.landingRecoveryTimer - dt);
        const Vec2 moveStartPos = enemy.pos;
        enemy.moving = false;
        const float moveStartGroundY = GetGroundWorldY(enemy.pos, enemy.depth);
        if (!enemy.grounded)
        {
            float groundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
            enemy.airTime += dt;
            const float previousFeetWorldY = enemy.feetWorldY;
            enemy.feetWorldY += enemy.verticalSpeed * dt;
            const float enemyRadius = enemy.type == EnemyType::Charger ? 0.40f : 0.50f;
            const float enemyHeight = enemy.type == EnemyType::Charger ? 1.20f : 1.40f;
            ResolveEnvironmentVerticalCollision(
                enemy.pos,
                previousFeetWorldY,
                enemy.feetWorldY,
                enemy.verticalSpeed,
                enemyRadius,
                enemyHeight);
            groundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
            enemy.verticalSpeed -= 9.8f * dt;
            enemy.peakFeetWorldY = std::max(enemy.peakFeetWorldY, enemy.feetWorldY);

            if (enemy.verticalSpeed <= 0.0f && enemy.feetWorldY <= groundWorldY)
            {
                const float fallDistance = std::max(0.0f, enemy.peakFeetWorldY - groundWorldY);
                float landingRecovery = 0.0f;
                if (fallDistance >= 6.0f) landingRecovery = kLandingRecoveryHeavy;
                else if (fallDistance >= 4.0f) landingRecovery = kLandingRecoveryMedium;
                else if (fallDistance >= 2.0f) landingRecovery = kLandingRecoveryLight;

                enemy.grounded = true;
                enemy.airTime = 0.0f;
                enemy.verticalSpeed = 0.0f;
                enemy.feetWorldY = groundWorldY;
                enemy.peakFeetWorldY = groundWorldY;
                enemy.landingRecoveryTimer = landingRecovery;
            }

            continue;
        }
        if (enemy.landingRecoveryTimer > 0.0f)
        {
            enemy.feetWorldY = moveStartGroundY;
            enemy.peakFeetWorldY = moveStartGroundY;
            continue;
        }
        if (enemy.chargeTimer > 0.0f)
        {
            if (enemy.type == EnemyType::Territory && Distance(m_player.pos, enemy.territoryCenter) > enemy.territoryRadius)
            {
                enemy.chargeTimer = 0.0f;
                enemy.hasHitThisCharge = false;
                continue;
            }
            const float chargeProgress = std::max(
                0.0f,
                std::min(1.0f, 1.0f - enemy.chargeTimer / kEnemyChargeTime));
            const float chargeSpeedScale = kEnemyChargeStartSpeedScale +
                (kEnemyChargeEndSpeedScale - kEnemyChargeStartSpeedScale) * chargeProgress;
            const Vec2 chargeTarget = Add(
                enemy.pos,
                Mul(enemy.chargeDir, enemy.moveSpeed * 3.0f * chargeSpeedScale * dt));
            enemy.pos = ResolveFloorMove(
                enemy.pos,
                chargeTarget,
                enemy.depth,
                enemy.type == EnemyType::Charger ? 0.40f : 0.50f,
                enemy.type == EnemyType::Charger ? 1.20f : 1.40f);
            enemy.facing = enemy.chargeDir;
            enemy.moving = Distance(moveStartPos, enemy.pos) > 0.001f;
            enemy.chargeTimer = std::max(0.0f, enemy.chargeTimer - dt);
            if (enemy.chargeTimer <= 0.0f)
            {
                float intervalScale = 1.0f;
                if (activity >= 100) intervalScale = enemy.type == EnemyType::Charger ? 0.50f : 0.25f;
                else if (activity >= 65) intervalScale = enemy.type == EnemyType::Charger ? 0.75f : 0.70f;
                enemy.attackCooldown = enemy.attackInterval * intervalScale;
            }
            const bool sameDepthAsPlayer = std::fabs(enemy.depth - m_player.depth) <= 0.35f;
            if (sameDepthAsPlayer && !enemy.hasHitThisCharge && Distance(enemy.pos, m_player.pos) <= kEnemyHitRange)
            {
                bool invincible = m_player.stepTimer > kStepRecoveryTime;
                if (!invincible)
                {
                    ApplyPlayerDamage(enemy.attackDamage, DeathCause::Enemy, u8"敵の攻撃で死亡しました。");
                    m_cameraShakeTimer = kCameraShakeDuration;
                    m_player.knockbackTimer = kKnockbackTime;
                    Vec2 dir = Normalize(Sub(m_player.pos, enemy.pos));
                    m_player.knockbackVelocity = Mul(dir, kKnockbackDistance / kKnockbackTime);
                    AddMessage(u8"敵の体当たりを受けました。");
                }
                enemy.hasHitThisCharge = true;
            }
            continue;
        }
        if (enemy.telegraphTimer > 0.0f)
        {
            if (enemy.type == EnemyType::Territory && Distance(m_player.pos, enemy.territoryCenter) > enemy.territoryRadius)
            {
                enemy.telegraphTimer = 0.0f;
                continue;
            }
            const Vec2 telegraphDirection = Normalize(Sub(m_player.pos, enemy.pos));
            if (Distance({}, telegraphDirection) > 0.001f)
            {
                enemy.facing = telegraphDirection;
            }
            enemy.telegraphTimer = std::max(0.0f, enemy.telegraphTimer - dt);
            if (enemy.telegraphTimer <= 0.0f)
            {
                enemy.chargeDir = Normalize(Sub(m_player.pos, enemy.pos));
                enemy.chargeTimer = kEnemyChargeTime;
                enemy.hasHitThisCharge = false;
            }
            continue;
        }
        const bool sameDepthAsPlayer = std::fabs(enemy.depth - m_player.depth) <= 0.35f;
        Vec2 toPlayer = Sub(m_player.pos, enemy.pos);
        float dist = Distance(enemy.pos, m_player.pos);

        float searchScale = 1.0f;
        if (activity >= 100) searchScale = enemy.type == EnemyType::Charger ? 1.50f : 1.25f;
        else if (activity >= 40) searchScale = enemy.type == EnemyType::Charger ? 1.25f : 1.10f;
        const bool territoryHostile = enemy.type == EnemyType::Territory &&
            Distance(m_player.pos, enemy.territoryCenter) <= enemy.territoryRadius;
        const bool chargerHostile = enemy.type == EnemyType::Charger && dist <= enemy.searchRange * searchScale;
        const bool hostile = sameDepthAsPlayer && (territoryHostile || chargerHostile);

        if (hostile && dist > 0.1f)
        {
            const Vec2 moveDirection = Normalize(toPlayer);
            const Vec2 moveTarget = Add(enemy.pos, Mul(moveDirection, enemy.moveSpeed * dt));
            enemy.pos = ResolveFloorMove(
                enemy.pos,
                moveTarget,
                enemy.depth,
                enemy.type == EnemyType::Charger ? 0.40f : 0.50f,
                enemy.type == EnemyType::Charger ? 1.20f : 1.40f);
            enemy.facing = moveDirection;
        }
        else if (enemy.type == EnemyType::Territory)
        {
            Vec2 target = enemy.patrolPoints[enemy.patrolIndex];
            if (Distance(enemy.pos, enemy.territoryCenter) > enemy.territoryRadius) target = enemy.territoryCenter;
            if (Distance(enemy.pos, target) <= 0.3f)
            {
                enemy.patrolIndex = (enemy.patrolIndex + 1) % 3;
                target = enemy.patrolPoints[enemy.patrolIndex];
            }
            const Vec2 moveDirection = Normalize(Sub(target, enemy.pos));
            enemy.pos = ResolveFloorMove(
                enemy.pos,
                Add(enemy.pos, Mul(moveDirection, enemy.moveSpeed * dt)),
                enemy.depth,
                enemy.type == EnemyType::Charger ? 0.40f : 0.50f,
                enemy.type == EnemyType::Charger ? 1.20f : 1.40f);
            if (Distance({}, moveDirection) > 0.001f)
            {
                enemy.facing = moveDirection;
            }
        }

        enemy.attackCooldown -= dt;
        if (hostile && enemy.attackCooldown <= 0.0f && dist <= 3.0f)
        {
            enemy.telegraphTimer = activity >= 100 ? enemy.telegraphDuration * 0.50f : enemy.telegraphDuration;
        }
        const float currentGroundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
        const float walkedDropHeight = moveStartGroundY - currentGroundWorldY;
        const bool movedHorizontally = Distance(moveStartPos, enemy.pos) > 0.01f;
        enemy.moving = movedHorizontally;
        if (movedHorizontally && walkedDropHeight >= m_autoFallStartHeight)
        {
            enemy.grounded = false;
            enemy.airTime = 0.0f;
            enemy.verticalSpeed = 0.0f;
            enemy.feetWorldY = moveStartGroundY;
            enemy.peakFeetWorldY = moveStartGroundY;
            enemy.landingRecoveryTimer = 0.0f;
            enemy.telegraphTimer = 0.0f;
            enemy.chargeTimer = 0.0f;
            enemy.hasHitThisCharge = false;
            continue;
        }
        enemy.feetWorldY = currentGroundWorldY;
        enemy.peakFeetWorldY = currentGroundWorldY;
    }
}

void SceneNarakuProto::UpdateUpperLoad(float dt)
{
    const float currentWorldY = m_player.onRope && m_activeRope >= 0
        ? GetRopeWorldY(m_activeRope, m_ropeProgress)
        : m_player.feetWorldY;
    const float ascent = (m_player.grounded || m_player.onRope)
        ? currentWorldY - m_player.previousWorldY
        : 0.0f;

    // 上昇している場合は上昇負荷ゲージを加算します。
    if (ascent > 0.0f)
    {
        // 上昇量そのものを内部ゲージに加算します。
        m_player.upperLoad += ascent;

        // 発症目安20mに達したら現在層の上昇負荷を発症させます。
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            TriggerUpperLoad();

            // 発症後は内部ゲージを0へ戻します。
            m_player.upperLoad = 0.0f;
        }
    }

    // 上昇していない場合は停止、平地、下降のすべてで一定速度回復します。
    else
    {
        // 1秒あたり1mぶん内部ゲージを減らします。
        m_player.upperLoad = std::max(0.0f, m_player.upperLoad - kUpperLoadRecoveryPerSecond * dt);
    }
}

bool SceneNarakuProto::TryCatchRopeWhileFalling()
{
    if (m_player.grounded || m_player.onRope || m_player.verticalSpeed >= 0.0f)
    {
        return false;
    }

    float ropeProgress = 0.0f;
    const int ropeIndex = FindFallingRopeIndex(kFallingRopeGrabRadius, ropeProgress);
    if (ropeIndex < 0)
    {
        return false;
    }

    const RopePoint& rope = m_ropePoints[ropeIndex];
    m_activeRope = ropeIndex;
    m_ropeProgress = ropeProgress;
    m_player.onRope = true;
    m_player.pos = GetRopePosition(m_activeRope, m_ropeProgress);
    m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
    m_player.grounded = false;
    m_player.verticalSpeed = 0.0f;
    m_player.airTime = 0.0f;
    m_player.feetWorldY = GetRopePlayerFeetWorldY(m_activeRope, m_ropeProgress);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.landingRecoveryTimer = 0.0f;
    m_player.blockedCellAirTime = 0.0f;
    m_player.blockedCellVelocity = {};
    AddMessage(u8"落下中にロープをつかみました。");
    return true;
}

bool SceneNarakuProto::TryEnterNearbyBase()
{
#if !defined(NARAKU_EDITOR_BUILD)
    for (const NarakuMap::BasePoint& base : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base.layerId);
        if (layerIndex < 0 ||
            std::fabs(m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth - m_player.depth) > 0.35f ||
            !IsNear(m_player.pos, { base.xz.x, base.xz.z }, kInteractRange))
        {
            continue;
        }
        m_mode = base.type == NarakuMap::BaseType::SecondBase ? Mode::SecondBase : Mode::ForwardBase;
        return true;
    }
#endif
    return false;
}

bool SceneNarakuProto::TryUseNearbyLayerGate()
{
    for (int gateIndex = 0; gateIndex < static_cast<int>(m_layerGates.size()); ++gateIndex)
    {
        const LayerGateState& gate = m_layerGates[gateIndex];
        const Vec2 interactPoint = gate.isEntry ? gate.ropePos : gate.loadPos;
        if (IsNear(m_player.pos, interactPoint, kInteractRange) &&
            std::fabs(m_player.depth - gate.depth) <= 0.35f)
        {
            TryUseLayerGate(gateIndex);
            return true;
        }
    }
    return false;
}

bool SceneNarakuProto::TryOpenReturnConfirmation()
{
    const bool canReturnHere = m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()) &&
        m_areas[m_currentAreaIndex].canReturn;
    if (!canReturnHere || !IsNear(m_player.pos, m_returnPoint, kReturnRange) ||
        std::fabs(m_player.depth - m_returnDepth) > 0.35f)
    {
        return false;
    }

    m_mode = Mode::ReturnConfirm;
    return true;
}

bool SceneNarakuProto::TryToggleNearbyRope()
{
    const int ropeIndex = FindNearestRopeIndex(kInteractRange);
    if (ropeIndex < 0)
    {
        return false;
    }

    const RopePoint& rope = m_ropePoints[ropeIndex];
    if (m_player.onRope)
    {
        const bool useBottom = m_ropeProgress >= 0.5f;
        const float endpointDepth = useBottom ? rope.bottomDepth : rope.topDepth;
        const Vec2 endpointPosition = useBottom ? rope.bottomPos : rope.topPos;
        if (CanStandAt(endpointPosition, endpointDepth))
        {
            m_ropeProgress = useBottom ? 1.0f : 0.0f;
            m_player.depth = endpointDepth;
            m_player.pos = endpointPosition;
            m_player.onRope = false;
            m_activeRope = -1;
            m_player.grounded = true;
            m_player.verticalSpeed = 0.0f;
            m_player.airTime = 0.0f;
            m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.landingRecoveryTimer = 0.0f;
            AddMessage(u8"ロープを離しました。");
        }
        else
        {
            AddMessage(u8"歩行できない場所にはロープから降りられません。");
        }
        return true;
    }

    const float topDistance = Distance(m_player.pos, rope.topPos) + std::fabs(m_player.depth - rope.topDepth);
    const float bottomDistance = Distance(m_player.pos, rope.bottomPos) + std::fabs(m_player.depth - rope.bottomDepth);
    m_ropeProgress = bottomDistance < topDistance ? GetBottomRopeGrabProgress(ropeIndex) : 0.0f;
    m_player.onRope = true;
    m_activeRope = ropeIndex;
    m_player.pos = GetRopePosition(m_activeRope, m_ropeProgress);
    m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
    m_player.grounded = false;
    m_player.verticalSpeed = 0.0f;
    m_player.airTime = 0.0f;
    m_player.feetWorldY = GetRopePlayerFeetWorldY(m_activeRope, m_ropeProgress);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.landingRecoveryTimer = 0.0f;
    AddMessage(u8"ロープにつかまりました。");
    return true;
}

void SceneNarakuProto::TryInteract()
{
    // 着地直後の硬直中はインタラクトを受け付けません。
    if (m_player.landingRecoveryTimer > 0.0f)
    {
        return;
    }

    // 採掘中は他のインタラクトを受け付けません。
    if (m_miningIndex >= 0)
    {
        return;
    }

    if (TryCatchRopeWhileFalling()) return;
    if (TryEnterNearbyBase()) return;
    if (TryUseNearbyLayerGate()) return;
    if (TryOpenReturnConfirmation()) return;
    if (TryToggleNearbyRope()) return;

#if defined(NARAKU_EDITOR_BUILD)
    // Editor歩行確認では層間出入口とロープだけを操作対象にします。
    return;
#endif
    if (TryInteractWithQuestTarget()) return;

    for (int index = 0; index < static_cast<int>(m_fishingPoints.size()); ++index)
    {
        FishingPoint& point = m_fishingPoints[static_cast<size_t>(index)];
        if (IsNear(m_player.pos, point.pos, kInteractRange) && std::fabs(m_player.depth - point.depth) <= 0.35f)
        {
            m_fishingPointIndex = index;
            m_mode = Mode::FishingConfirm;
            return;
        }
    }

    // フィールドに置かれた旧器が近くにあれば拾う確認へ移ります。
    for (GroundRelic& relic : m_groundRelics)
    {
        // 無効化済みの旧器は無視します。
        if (!relic.active) continue;

        // 近くにある旧器だけ反応します。
        if (IsNear(m_player.pos, relic.pos, kInteractRange) && std::fabs(m_player.depth - relic.depth) <= 0.35f)
        {
            // 拾う確認に表示する旧器を設定します。
            m_pendingRelic = relic.item;

            // 拾わず戻す場合の位置を記録します。
            m_pendingRelicPos = relic.pos;
            m_pendingRelicDepth = relic.depth;
            m_pendingRelicMiningIndex = relic.sourceMiningIndex;

            // 一旦地面側を無効化して二重取得を避けます。
            relic.active = false;

            // 旧器確認モードへ移ります。
            m_mode = Mode::RelicPrompt;

            // 1回のF入力で複数の対象を処理しないよう戻ります。
            return;
        }
    }

    // 敵が落とした食料を拾います。
    for (GroundFood& food : m_groundFoods)
    {
        if (!food.active) continue;
        if (IsNear(m_player.pos, food.pos, kInteractRange) && std::fabs(m_player.depth - food.depth) <= 0.35f)
        {
            if (GetCurrentWeight() + 1.0f > GetPickupWeightLimit())
            {
                AddMessage(u8"これ以上は重すぎて食料を拾えません。");
                return;
            }
            food.active = false;
            ++m_foodCount;
            AddMessage(u8"食料を1個拾いました。");
            return;
        }
    }

    const std::uint32_t waterFlags = GetNearbyWaterFlags();
    if (waterFlags != NarakuMap::CellAttributeNone)
    {
        if (GetCurrentDepth() == 5)
        {
            AddMessage(u8"第五層の湖水は飲水・採水には利用できません。");
            return;
        }
        m_pendingWaterFlags = waterFlags;
        m_mode = Mode::WaterPrompt;
        return;
    }

    // 最後に採掘ポイントの開始判定を行います。
    for (int i = 0; i < static_cast<int>(m_miningPoints.size()); ++i)
    {
        // 対象採掘ポイントを取得します。
        MiningPoint& point = m_miningPoints[i];

        // 採掘済み、または遠いポイントは無視します。
        if (point.mined || std::fabs(m_player.depth - point.depth) > 0.35f || !IsNear(m_player.pos, point.pos, kInteractRange)) continue;

        // スタミナが足りない場合は採掘を開始しません。
        if (!CanSpendStamina(m_debugPlayerParams.miningCost))
        {
            // HUDログにスタミナ不足を出します。
            AddMessage(u8"スタミナが足りないため採掘できません。");

            // 近くの採掘対象を見つけたので処理を終えます。
            return;
        }

        // 採掘開始時にスタミナを消費します。
        SpendStamina(m_debugPlayerParams.miningCost);
        m_fullness = std::max(0.0f, m_fullness - GetDepthLevelFullnessConsumptionMultiplier());
        m_hydration = std::max(0.0f, m_hydration - GetDepthLevelFullnessConsumptionMultiplier());

        // 採掘中のポイント番号を記録します。
        m_miningIndex = i;

        // 装備中のつるはしによる速度倍率を採掘所要時間へ反映します。
        m_miningDuration = kMiningTime / GetMiningSpeedMultiplier();
        m_miningTimer = m_miningDuration;

        // HUDログに採掘開始を出します。
        AddMessage(u8"採掘を開始しました。");

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }
}

void SceneNarakuProto::TryStartStep()
{
    // 着地直後の硬直中はステップさせません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 重量100%以上ではステップ不可です。
    if (GetCurrentWeight() >= GetMaxWeight())
    {
        AddMessage(u8"重量が重すぎてステップできません。");
        ShowCenterNotification(u8"重すぎてステップができない！");
        return;
    }

    // スタミナ不足ならステップ不可です。
    if (!CanSpendStamina(m_debugPlayerParams.stepCost)) { AddMessage(u8"スタミナが足りないためステップできません。"); return; }

    // ステップ1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.stepCost);
    m_fullness = std::max(0.0f, m_fullness - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());

    // 無敵時間と後硬直の合計時間を設定します。
    m_player.stepTimer = kStepInvincibleTime + kStepRecoveryTime;
}

void SceneNarakuProto::TryStartJump()
{
    // 着地直後の硬直中はジャンプさせません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 地上にいない時は二段ジャンプを許可しません。
    if (!m_player.grounded) return;

    // ロープ中はジャンプさせません。
    if (m_player.onRope) return;

    // 重量100%以上ではジャンプ不可です。
    if (GetCurrentWeight() >= GetMaxWeight()) { AddMessage(u8"重量が重すぎてジャンプできません。"); return; }

    // スタミナ不足ならジャンプ不可です。
    if (!CanSpendStamina(m_debugPlayerParams.jumpCost)) { AddMessage(u8"スタミナが足りないためジャンプできません。"); return; }

    // ジャンプ1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.jumpCost);
    m_fullness = std::max(0.0f, m_fullness - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());

    // 空中状態へ切り替えます。
    m_player.grounded = false;

    // 高さ1m程度を想定した初速を入れます。
    m_player.verticalSpeed = 4.45f;

    // 現在地面の絶対高さを足元基準として記録します。
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;

    m_jumpEffects.push_back({ m_player.pos, m_player.depth, kJumpEffectDuration });

    // 空中時間を0から測ります。
    m_player.airTime = 0.0f;
}

void SceneNarakuProto::TryStartAttack()
{
    // 着地直後の硬直中は攻撃させません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 攻撃中は次の攻撃を開始しません。
    if (m_player.attackTimer > 0.0f) return;

    // 採掘中は攻撃させません。
    if (m_miningIndex >= 0) return;

    // スタミナ不足なら攻撃不可です。
    if (!CanSpendStamina(m_debugPlayerParams.attackCost)) { AddMessage(u8"スタミナが足りないため攻撃できません。"); return; }

    // 攻撃1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.attackCost);
    m_fullness = std::max(0.0f, m_fullness - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());

    m_attackRelicTriggered = false;
    int relicIndex = -1;
    int fewestUses = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(m_inventory.size()); ++i)
    {
        const RelicItem& item = m_inventory[i];
        if (item.type != RelicType::Offensive || item.broken || item.remainingUses <= 0) continue;
        if (item.remainingUses < fewestUses)
        {
            relicIndex = i;
            fewestUses = item.remainingUses;
        }
    }
    const float relicStaminaCost = GetMaxStamina() * 0.10f;
    if (relicIndex >= 0 && CanSpendStamina(relicStaminaCost))
    {
        SpendStamina(relicStaminaCost);
        RelicItem& item = m_inventory[relicIndex];
        --item.remainingUses;
        if (item.remainingUses <= 0)
        {
            item.remainingUses = 0;
            item.broken = true;
            item.value = 5;
        }
        m_attackRelicTriggered = true;
    }

    // 攻撃全体時間を設定します。
    m_player.attackTimer = kAttackTotal;

    // 連続攻撃が左右の往復に見えるよう、攻撃開始ごとに振り方向を反転します。
    m_player.attackSwingReverse = !m_player.attackSwingReverse;
    for (EnemyState& enemy : m_enemies)
    {
        enemy.hitByPlayerAttack = false;
        enemy.hitByRelicAttack = false;
    }
}

void SceneNarakuProto::UpdateUnknownWeaponAttack(float dt)
{
    if (!IsActionAttackPress())
    {
        m_unknownWeaponChargeTimer = 0.0f;
        m_unknownWeaponFiredThisHold = false;
        return;
    }
    if (m_unknownWeaponFiredThisHold || m_unknownWeaponCooldownTimer > 0.0f ||
        m_player.attackTimer > 0.0f || m_miningIndex >= 0 || m_player.landingRecoveryTimer > 0.0f)
    {
        return;
    }
    m_unknownWeaponChargeTimer += dt;
    if (m_unknownWeaponChargeTimer < kUnknownWeaponChargeDuration) return;
    m_unknownWeaponFiredThisHold = true;
    m_unknownWeaponChargeTimer = 0.0f;
    if (GetLastActiveDevice() == ActiveInputDevice::KeyboardMouse)
    {
        UpdateAimDirectionFromMouse();
    }
    FireUnknownWeapon();
}

void SceneNarakuProto::FireUnknownWeapon()
{
    const float staminaCost = m_debugPlayerParams.attackCost * 2.0f;
    if (!CanSpendStamina(staminaCost))
    {
        AddMessage(u8"スタミナが足りないため未知の武器を発射できません。");
        return;
    }
    SpendStamina(staminaCost);
    m_fullness = std::max(0.0f, m_fullness - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());
    m_player.attackTimer = kAttackTotal;
    m_attackRelicTriggered = false;
    m_unknownWeaponCooldownTimer = kUnknownWeaponCooldownDuration;

    int defeated = 0;
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive || std::fabs(enemy.depth - m_player.depth) > 0.35f) continue;
        const Vec2 relative = Sub(enemy.pos, m_player.pos);
        const float forward = Dot(relative, m_player.facing);
        const float side = std::fabs(relative.x * m_player.facing.y - relative.y * m_player.facing.x);
        if (forward <= 0.0f || side > kUnknownWeaponHalfWidth) continue;
        enemy.hp = 0.0f;
        enemy.alive = false;
        enemy.respawnTimer = kEnemyRespawnTime;
        m_attackHitEffects.push_back({ enemy.pos, enemy.depth, kAttackHitEffectDuration });
        m_groundFoods.push_back({ enemy.pos, enemy.depth, true });
        AwardEnemyDefeat(enemy);
        ++defeated;
    }
    AddMessage(defeated > 0
        ? u8"未知の武器が前方の敵を貫きました。"
        : u8"未知の武器を発射しました。");
}

void SceneNarakuProto::TriggerUpperLoad()
{
    if (TryPreventUpperLoad()) return;

    const int depth = GetCurrentDepth();
    const auto applyMentalAndVision = [this](float mentalLoss, float visionOcclusion)
    {
        if (mentalLoss > 0.0f)
        {
            ApplyMentalDamage(mentalLoss, DeathCause::UpperLoad, u8"上昇負荷で精神力が0になりました。");
        }
        m_upperLoadVisionTimer = mentalLoss;
        m_upperLoadVisionOcclusion = visionOcclusion;

        if (mentalLoss <= 0.0f)
        {
            AddMessage(u8"上昇負荷が発症しましたが、影響を受けませんでした。");
            return;
        }

        std::ostringstream message;
        message << u8"上昇負荷が発症しました。精神力-"
            << static_cast<int>(std::round(mentalLoss)) << u8"。";
        AddMessage(message.str());
    };

    switch (depth)
    {
    case 1:
        applyMentalAndVision(
            GetLevelInterpolatedValue(kUpperLoadFirstMentalLoss, m_level),
            GetLevelInterpolatedValue(kUpperLoadFirstVisionOcclusion, m_level));
        break;
    case 2:
        applyMentalAndVision(
            GetLevelInterpolatedValue(kUpperLoadSecondMentalLoss, m_level),
            GetLevelInterpolatedValue(kUpperLoadSecondVisionOcclusion, m_level));
        break;
    case 3:
        applyMentalAndVision(
            GetLevelInterpolatedValue(kUpperLoadSecondMentalLoss, m_level) * 1.5f,
            GetLevelInterpolatedValue(kUpperLoadSecondVisionOcclusion, m_level));
        break;
    case 4:
    {
        const float damageRatio = GetLevelInterpolatedValue(kUpperLoadFourthDamageRatios, m_level);
        const float damage = GetMaxHp() * damageRatio;
        if (m_player.hp - damage <= 0.0f && TryConsumeSurvivalRelic(true, false))
        {
            return;
        }
        m_player.hp = std::max(0.0f, m_player.hp - damage);
        if (m_player.hp <= 0.0f)
        {
            StartDeath(u8"上昇負荷で体力が0になりました。", DeathCause::UpperLoad);
            return;
        }
        AddMessage(u8"上昇負荷が発症し、最大HP割合ダメージを受けました。");
        break;
    }
    case 5:
    {
        const bool wasActive = m_upperLoadFifthTimer > 0.0f;
        m_upperLoadFifthTimer += GetLevelInterpolatedValue(kUpperLoadFifthDurations, m_level);
        m_upperLoadFifthDamageRatio = GetLevelInterpolatedValue(kUpperLoadFifthDamageRatios, m_level);
        if (!wasActive) m_upperLoadFifthDamageCooldown = 0.0f;
        AddMessage(u8"上昇負荷が発症し、盲目と移動負荷が発生しました。");
        break;
    }
    default:
        break;
    }
}

void SceneNarakuProto::UpdateUpperLoadEffects(float dt)
{
    if (m_upperLoadVisionTimer > 0.0f)
    {
        m_upperLoadVisionTimer = std::max(0.0f, m_upperLoadVisionTimer - dt);
        if (m_upperLoadVisionTimer <= 0.0f) m_upperLoadVisionOcclusion = 0.0f;
    }

    if (m_upperLoadFifthTimer <= 0.0f) return;

    m_upperLoadFifthDamageCooldown = std::max(0.0f, m_upperLoadFifthDamageCooldown - dt);
    float moveX = 0.0f, moveY = 0.0f;
    GetActionMoveVector(moveX, moveY);
    const bool movementInput = moveX != 0.0f || moveY != 0.0f;
    if (movementInput && m_upperLoadFifthDamageCooldown <= 0.0f)
    {
        const float damage = GetMaxHp() * m_upperLoadFifthDamageRatio;
        if (!(m_player.hp - damage <= 0.0f && TryConsumeSurvivalRelic(true, false)))
        {
            m_player.hp = std::max(0.0f, m_player.hp - damage);
            if (m_player.hp <= 0.0f)
            {
                StartDeath(u8"上昇負荷の移動ダメージで体力が0になりました。", DeathCause::UpperLoad);
                return;
            }
        }
        m_upperLoadFifthDamageCooldown = kUpperLoadFifthDamageInterval;
    }

    m_upperLoadFifthTimer = std::max(0.0f, m_upperLoadFifthTimer - dt);
    if (m_upperLoadFifthTimer <= 0.0f)
    {
        m_upperLoadFifthDamageRatio = 0.0f;
        m_upperLoadFifthDamageCooldown = 0.0f;
    }
}

void SceneNarakuProto::UpdateHunger(float dt)
{
    const int depth = GetCurrentDepth();
    float perMinute = 0.5f;
    if (m_miningIndex >= 0 || m_player.stepTimer > 0.0f) perMinute = 0.0f;
    else if (m_lastFrameRopeMoving) perMinute = 2.0f;
    else if (m_player.grounded && m_player.landingRecoveryTimer <= 0.0f && m_player.knockbackTimer <= 0.0f &&
        m_lastFrameMovementDistance > 0.001f) perMinute = m_lastFrameRunning ? 1.5f : 1.0f;
    if (m_rationFullnessWardTimer <= 0.0f)
    {
        m_fullness = std::max(0.0f,
            m_fullness - perMinute / 60.0f * GetDepthLevelFullnessConsumptionMultiplier() * dt);
    }
    m_player.stamina = std::min(m_player.stamina, GetMaxStamina());

    if (m_player.knockbackTimer <= 0.0f && m_lastFrameMovementDistance > 0.0f)
    {
        const std::size_t index = static_cast<std::size_t>(depth - 1);
        const float before = m_movementExpByDepth[index];
        const float cap = static_cast<float>(GetRulesForDepth(depth).movementExpCap);
        m_movementExpByDepth[index] = std::min(cap, before + m_lastFrameMovementDistance * GetDepthMovementExpMultiplier(depth));
        const int gained = static_cast<int>(std::floor(m_movementExpByDepth[index])) - static_cast<int>(std::floor(before));
        if (gained > 0) AwardExp(gained);
    }

    if (m_fullness <= 0.0f && m_mode == Mode::Explore)
    {
        StartDeath(u8"餓死しました。", DeathCause::Starvation);
    }
}

void SceneNarakuProto::UpdateHydration(float dt)
{
    float perMinute = 0.5f;
    if (m_miningIndex >= 0 || m_player.stepTimer > 0.0f) perMinute = 0.0f;
    else if (m_lastFrameRopeMoving) perMinute = 2.0f;
    else if (m_player.grounded && m_player.landingRecoveryTimer <= 0.0f && m_player.knockbackTimer <= 0.0f &&
        m_lastFrameMovementDistance > 0.001f) perMinute = m_lastFrameRunning ? 1.5f : 1.0f;
    const float rationPenalty = m_rationHydrationPenaltyTimer > 0.0f
        ? kRationHydrationPenaltyMultiplier : 1.0f;
    m_hydration = std::max(0.0f, m_hydration -
        perMinute / 60.0f * GetDepthLevelFullnessConsumptionMultiplier() * rationPenalty * dt);

    if (m_hydration <= 0.0f)
    {
        if (!m_hydrationWasZero)
        {
            m_hydrationWasZero = true;
            m_dehydrationZeroTimer = m_dehydrationVisionStrength > 0.0f
                ? kDehydrationDelay + m_dehydrationVisionStrength * kDehydrationTransitionDuration
                : 0.0f;
        }
        m_dehydrationZeroTimer += dt;
        const float progress = std::max(0.0f, std::min(1.0f,
            (m_dehydrationZeroTimer - kDehydrationDelay) / kDehydrationTransitionDuration));
        m_dehydrationVisionStrength = std::max(m_dehydrationVisionStrength, progress);
    }
    else
    {
        m_hydrationWasZero = false;
        m_dehydrationZeroTimer = 0.0f;
        m_dehydrationVisionStrength = std::max(0.0f,
            m_dehydrationVisionStrength - dt / kDehydrationTransitionDuration);
    }
}

void SceneNarakuProto::UpdateCooking(float dt)
{
    if (m_cookingTarget == CookingTarget::None) return;

    const bool interrupted = m_lastFrameMovementDistance > 0.001f ||
        m_player.attackTimer > 0.0f || m_player.stepTimer > 0.0f ||
        m_player.knockbackTimer > 0.0f || m_player.onRope || !m_player.grounded ||
        m_player.hp < m_cookingPreviousHp;
    if (interrupted)
    {
        CancelCooking(u8"行動または被弾により調理を中断しました。料理セットの使用回数は戻りません。");
        return;
    }

    m_cookingPreviousHp = m_player.hp;
    m_cookingTimer = std::max(0.0f, m_cookingTimer - dt);
    if (m_cookingTimer > 0.0f) return;

    if (m_cookingTarget == CookingTarget::BoilBottle &&
        m_cookingBottleIndex >= 0 && m_cookingBottleIndex < static_cast<int>(m_waterBottles.size()))
    {
        WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(m_cookingBottleIndex)];
        if (bottle.quality == WaterQuality::Unboiled && bottle.amount > 0.0f)
        {
            bottle.quality = WaterQuality::Boiled;
            bottle.foodPoisoningChance = 0.0f;
            AddMessage(u8"水筒の水を煮沸しました。");
        }
    }
    else if (m_cookingTarget == CookingTarget::HeatFood && m_foodCount > 0)
    {
        --m_foodCount;
        ++m_heatedFoodCount;
        AddMessage(u8"携帯食料を加熱しました。");
    }
    else if (m_cookingTarget == CookingTarget::CookFish && m_rawFishCount > 0)
    {
        --m_rawFishCount;
        ++m_cookedFishCount;
        AddMessage(u8"見たことない魚を調理しました。");
    }
    else if (m_cookingTarget == CookingTarget::CookSizedFish && m_cookingFishSize >= 0 && m_cookingFishSize < 3 &&
        m_rawSizedFish[static_cast<size_t>(m_cookingFishSize)] > 0)
    {
        --m_rawSizedFish[static_cast<size_t>(m_cookingFishSize)];
        ++m_cookedSizedFish[static_cast<size_t>(m_cookingFishSize)];
        AddMessage(u8"魚を調理しました。");
    }
    m_cookingTarget = CookingTarget::None;
    m_cookingFishSize = -1;
    m_cookingBottleIndex = -1;
}

void SceneNarakuProto::UpdateMentalAbilities(float dt)
{
    m_upperLoadWardTimer = std::max(0.0f, m_upperLoadWardTimer - dt);
    m_miningSenseTimer = std::max(0.0f, m_miningSenseTimer - dt);
    if (m_miningSenseTimer <= 0.0f)
    {
        for (MiningPoint& point : m_miningPoints) point.sensed = false;
        for (AreaState& area : m_areas)
            for (MiningPoint& point : area.miningPoints) point.sensed = false;
    }

    const bool pressed = IsActionSkillPress();
    if (pressed)
    {
        m_qHoldTime += dt;
        if (!m_qLongTriggered && m_qHoldTime >= kQHoldThreshold)
        {
            m_qLongTriggered = true;
            ActivateUpperLoadWard();
        }
    }
    else if (m_qWasPressed)
    {
        if (!m_qLongTriggered) ActivateMiningSense();
        m_qHoldTime = 0.0f;
        m_qLongTriggered = false;
    }
    m_qWasPressed = pressed;
}

void SceneNarakuProto::ActivateMiningSense()
{
    if (m_level < 30) { ShowCenterNotification(u8"Lv30で解放されます。"); return; }
    const float cost = 15.0f * GetMentalAbilityDepthMultiplier(GetCurrentDepth()) *
        GetDepthLevelMentalConsumptionMultiplier();
    if (m_player.mental < cost) { ShowCenterNotification(u8"精神力が足りない！"); return; }
    m_player.mental -= cost;
    m_miningSenseTimer = kMentalSenseDuration;

    std::vector<int> queue;
    std::vector<int> visited;
    if (m_currentAreaIndex >= 0) { queue.push_back(m_currentAreaIndex); visited.push_back(m_currentAreaIndex); }
    for (std::size_t cursor = 0; cursor < queue.size() && visited.size() < 4; ++cursor)
    {
        const AreaState& area = m_areas[queue[cursor]];
        for (const LayerGateState& gate : area.layerGates)
        {
            const int next = gate.destinationAreaIndex;
            if (next < 0 || next >= static_cast<int>(m_areas.size()) || m_areas[next].depth != GetCurrentDepth() ||
                std::find(visited.begin(), visited.end(), next) != visited.end()) continue;
            visited.push_back(next);
            queue.push_back(next);
            if (visited.size() >= 4) break;
        }
    }
    for (int areaIndex : visited)
    {
        if (areaIndex == m_currentAreaIndex) for (MiningPoint& point : m_miningPoints) point.sensed = true;
        else for (MiningPoint& point : m_areas[areaIndex].miningPoints) point.sensed = true;
    }
    ShowCenterNotification(u8"採掘地点を15秒間感知します。");
}

void SceneNarakuProto::ActivateUpperLoadWard()
{
    if (m_level < 30) { ShowCenterNotification(u8"Lv30で解放されます。"); return; }
    const int depth = GetCurrentDepth();
    const float cost = 30.0f * GetMentalAbilityDepthMultiplier(depth) *
        GetDepthLevelMentalConsumptionMultiplier();
    if (m_player.mental < cost) { ShowCenterNotification(u8"精神力が足りない！"); return; }
    m_player.mental -= cost;
    m_upperLoadWardTimer = static_cast<float>(1 + (std::min(m_level, 100) - 30) / 10);
    ShowCenterNotification(u8"遺物を鎮め、一定時間の上昇負荷を防ぎます。");
}

bool SceneNarakuProto::TryPreventUpperLoad()
{
    if (GetCurrentDepth() >= 6 && TryConsumeFatalUpperLoadCartridge()) return true;
    if (m_upperLoadWardTimer <= 0.0f) return false;
    AddMessage(u8"安定化の力が上昇負荷を防ぎました。");
    return true;
}

bool SceneNarakuProto::TryConsumeFatalUpperLoadCartridge()
{
    if (!HasUnknownArmorSetEffect() || m_cartridgeCount <= 0) return false;
    --m_cartridgeCount;
    AddMessage(u8"カートリッジが致命的な上昇負荷を防ぎ、精神力への影響も遮断しました。");
    return true;
}

void SceneNarakuProto::UpdateExplorationDiscovery()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size())) return;
    AreaState& area = m_areas[m_currentAreaIndex];
    int enemySeen = 0;
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.discovered && Distance(enemy.pos, m_player.pos) <= kDiscoveryRange)
        {
            enemy.discovered = true;
            UpdateQuestDiscoveryProgress(true, false, GetCurrentDepth());
        }
        if (enemy.discovered) ++enemySeen;
    }
    int miningSeen = 0;
    for (MiningPoint& point : m_miningPoints)
    {
        if (!point.discovered && Distance(point.pos, m_player.pos) <= kDiscoveryRange)
        {
            point.discovered = true;
            UpdateQuestDiscoveryProgress(false, true, GetCurrentDepth());
        }
        if (point.discovered) ++miningSeen;
    }
    area.discoveredEnemyCount = enemySeen;
    area.discoveredMiningCount = miningSeen;
    UpdateImportantQuestExploration();

    std::size_t totalCells = 0;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
        totalCells += static_cast<std::size_t>(std::max(0, layer.gridWidth - 1) * std::max(0, layer.gridHeight - 1));
    if (area.discoveredCells.size() != totalCells) area.discoveredCells.assign(totalCells, 0);
    if (area.discoveredCliffs.size() != totalCells) area.discoveredCliffs.assign(totalCells, 0);
    std::size_t offset = 0;
    int validCells = 0;
    int discoveredValid = 0;
    int totalCliffs = 0;
    int discoveredCliffs = 0;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        const int cellWidth = std::max(0, layer.gridWidth - 1);
        const int cellHeight = std::max(0, layer.gridHeight - 1);
        int playerCellX = -1, playerCellZ = -1;
        float fracX = 0.0f, fracZ = 0.0f;
        const bool onLayer = std::fabs(layer.layerDepth - m_player.depth) <= 0.35f &&
            TryGetLayerCellAt(layer, m_player.pos, playerCellX, playerCellZ, fracX, fracZ);
        for (int z = 0; z < cellHeight; ++z)
        {
            for (int x = 0; x < cellWidth; ++x)
            {
                const std::size_t index = offset + static_cast<std::size_t>(z * cellWidth + x);
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, x, z);
                if ((flags & NarakuMap::CellAttributeRemoved) != 0u) continue;
                ++validCells;
                if (onLayer && x == playerCellX && z == playerCellZ) area.discoveredCells[index] = 1;
                if (area.discoveredCells[index] != 0) ++discoveredValid;
                if ((flags & NarakuMap::CellAttributeCliffEdge) != 0u)
                {
                    ++totalCliffs;
                    const float width = static_cast<float>(cellWidth) * layer.cellSize;
                    const float height = static_cast<float>(cellHeight) * layer.cellSize;
                    const Vec2 center = {
                        layer.center.x - width * 0.5f + (static_cast<float>(x) + 0.5f) * layer.cellSize,
                        layer.center.z - height * 0.5f + (static_cast<float>(z) + 0.5f) * layer.cellSize };
                    if (std::fabs(layer.layerDepth - m_player.depth) <= 0.35f && Distance(center, m_player.pos) <= kDiscoveryRange)
                        area.discoveredCliffs[index] = 1;
                    if (area.discoveredCliffs[index] != 0) ++discoveredCliffs;
                }
            }
        }
        offset += static_cast<std::size_t>(cellWidth * cellHeight);
    }
    area.totalCliffCount = totalCliffs;
    area.discoveredCliffCount = discoveredCliffs;
    if (validCells > 0)
    {
        const float ratio = static_cast<float>(discoveredValid) / static_cast<float>(validCells);
        for (int threshold = 0; threshold < 4; ++threshold)
        {
            if (!area.cellExpThresholds[threshold] && ratio >= 0.25f * static_cast<float>(threshold + 1))
            {
                area.cellExpThresholds[threshold] = true;
                AwardExp(static_cast<int>(std::round(20.0f * GetDepthExpMultiplier(GetCurrentDepth()))));
            }
        }
    }
}

SceneNarakuProto::EnemyState SceneNarakuProto::CreateEnemy(EnemyType type, int depth, const Vec2& position) const
{
    const DepthRules& rules = GetRulesForDepth(depth);
    EnemyState enemy;
    enemy.type = type;
    enemy.pos = position;
    enemy.spawnPos = position;
    enemy.territoryCenter = position;
    enemy.depth = m_startDepth;
    for (const FloorRegion& floor : m_floorRegions)
    {
        if (IsInsideFloor(floor, position)) { enemy.depth = floor.depth; break; }
    }
    enemy.feetWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
    enemy.peakFeetWorldY = enemy.feetWorldY;
    if (type == EnemyType::Charger)
    {
        enemy.maxHp = 30.0f * rules.enemyHp;
        enemy.attackDamage = 10.0f * rules.enemyAttack;
        enemy.searchRange = 8.0f;
        enemy.moveSpeed = kEnemyWalkSpeed * rules.enemyMove;
        enemy.attackInterval = kEnemyAttackInterval * rules.enemyInterval;
    }
    else
    {
        enemy.territoryRank = static_cast<TerritoryRank>(RandomInt(0, 2));
        const int rank = static_cast<int>(enemy.territoryRank);
        const float minimumAttack[] = { 15.0f, 20.0f, 25.0f };
        const float maximumAttack[] = { 20.0f, 25.0f, 30.0f };
        const float search[] = { 3.0f, 4.0f, 5.0f };
        const float divisor[] = { 6.0f, 5.0f, 4.0f };
        float stageSide = 24.0f;
        for (const FloorRegion& floor : m_floorRegions)
        {
            if (!IsInsideFloor(floor, position)) continue;
            stageSide = std::max(4.0f, std::min(floor.halfSize.x * 2.0f, floor.halfSize.y * 2.0f));
            break;
        }
        enemy.maxHp = 120.0f * rules.enemyHp;
        enemy.attackDamage = RandomFloat(minimumAttack[rank], maximumAttack[rank]) * rules.enemyAttack;
        enemy.searchRange = search[rank];
        enemy.territoryRadius = stageSide / divisor[rank];
        const float level40Growth = (1.0f - std::exp(-0.5f * (39.0f / 99.0f))) / (1.0f - std::exp(-0.5f));
        enemy.moveSpeed = (1.5f * (1.0f + 1.5f * level40Growth) * 0.75f) * rules.enemyMove;
        enemy.attackInterval = 1.0f * rules.enemyInterval;
        for (int i = 0; i < 3; ++i)
        {
            enemy.patrolPoints[i] = position;
            for (int attempt = 0; attempt < 16; ++attempt)
            {
                const float angle = DirectX::XM_2PI * static_cast<float>(i) / 3.0f + RandomFloat(-0.35f, 0.35f);
                const float radius = enemy.territoryRadius * RandomFloat(0.35f, 0.75f);
                const Vec2 candidate = {
                    position.x + std::cos(angle) * radius,
                    position.y + std::sin(angle) * radius
                };
                if (!HasFloorAt(candidate, enemy.depth)) continue;
                enemy.patrolPoints[i] = candidate;
                break;
            }
        }
    }
    enemy.hp = enemy.maxHp;
    enemy.attackCooldown = RandomFloat(0.0f, enemy.attackInterval);
    enemy.telegraphDuration = kEnemyTelegraphTime;
    return enemy;
}

bool SceneNarakuProto::HasTerritoryTreeDensity(const Vec2& position) const
{
    int treeCount = 0;
    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        const auto resource = std::find_if(m_environmentModels.begin(), m_environmentModels.end(),
            [&object](const EnvironmentModelResource& value) { return value.id == object.modelId; });
        if (resource == m_environmentModels.end() || !resource->isTree) continue;
        if (Distance(position, { object.xz.x, object.xz.z }) <= 5.0f && ++treeCount >= 10) return true;
    }
    return false;
}

SceneNarakuProto::Vec2 SceneNarakuProto::FindEnemySpawnPoint(float minimumPlayerDistance, bool requireTerritory, bool* found) const
{
    if (found) *found = false;
    if (m_floorRegions.empty()) return m_startPoint;
    for (int attempt = 0; attempt < 96; ++attempt)
    {
        const FloorRegion& floor = m_floorRegions[static_cast<std::size_t>(RandomInt(0, static_cast<int>(m_floorRegions.size()) - 1))];
        const Vec2 point = {
            RandomFloat(floor.center.x - floor.halfSize.x * 0.85f, floor.center.x + floor.halfSize.x * 0.85f),
            RandomFloat(floor.center.y - floor.halfSize.y * 0.85f, floor.center.y + floor.halfSize.y * 0.85f) };
        if (!HasFloorAt(point, floor.depth) || Distance(point, m_player.pos) < minimumPlayerDistance) continue;
        if (requireTerritory && !HasTerritoryTreeDensity(point)) continue;
        if (found) *found = true;
        return point;
    }
    return m_startPoint;
}

void SceneNarakuProto::SpawnEnemiesForCurrentArea()
{
    m_enemies.clear();
    const int depth = GetCurrentDepth();
    const DepthRules& rules = GetRulesForDepth(depth);
    const int chargerCount = RandomInt(0, rules.chargerMax);
    const int territoryCount = RandomInt(0, rules.territoryMax);
    for (int i = 0; i < chargerCount; ++i)
    {
        bool found = false;
        const Vec2 point = FindEnemySpawnPoint(3.0f, false, &found);
        if (found) m_enemies.push_back(CreateEnemy(EnemyType::Charger, depth, point));
    }
    for (int i = 0; i < territoryCount; ++i)
    {
        bool found = false;
        const Vec2 point = FindEnemySpawnPoint(3.0f, true, &found);
        if (found) m_enemies.push_back(CreateEnemy(EnemyType::Territory, depth, point));
    }
}

bool SceneNarakuProto::RespawnEnemy(EnemyState& enemy)
{
    bool found = false;
    const Vec2 point = FindEnemySpawnPoint(kEnemyRespawnMinPlayerDistance, enemy.type == EnemyType::Territory, &found);
    if (!found)
    {
        enemy.respawnTimer = kEnemyRespawnRetry;
        return false;
    }
    enemy = CreateEnemy(enemy.type, GetCurrentDepth(), point);
    return true;
}

void SceneNarakuProto::UpdateRespawns(float dt)
{
    for (EnemyState& enemy : m_enemies)
    {
        if (enemy.alive) continue;
        enemy.respawnTimer = std::max(0.0f, enemy.respawnTimer - dt);
        if (enemy.respawnTimer <= 0.0f) RespawnEnemy(enemy);
    }
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (areaIndex == m_currentAreaIndex) continue;
        for (EnemyState& enemy : m_areas[areaIndex].enemies)
            if (!enemy.alive) enemy.respawnTimer = std::max(0.0f, enemy.respawnTimer - dt);
    }
}

void SceneNarakuProto::UpdateMiningRespawns(float dt)
{
    auto updatePoints = [dt](std::vector<MiningPoint>& points)
    {
        for (MiningPoint& point : points)
        {
            if (!point.mined) continue;
            point.respawnTimer = std::max(0.0f, point.respawnTimer - dt);
            if (point.respawnTimer <= 0.0f && !point.outputPending)
            {
                point.mined = false;
                point.sensed = false;
            }
        }
    };
    updatePoints(m_miningPoints);
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (areaIndex != m_currentAreaIndex) updatePoints(m_areas[static_cast<std::size_t>(areaIndex)].miningPoints);
    }
}

void SceneNarakuProto::AwardEnemyDefeat(const EnemyState& enemy)
{
    const int depth = GetCurrentDepth();
    const int baseExp = enemy.type == EnemyType::Territory ? 250 : 100;
    AwardExp(static_cast<int>(std::round(baseExp * GetDepthExpMultiplier(depth))));
    const std::size_t index = static_cast<std::size_t>(depth - 1);
    if (enemy.type == EnemyType::Territory) ++m_result.territoryKillsByDepth[index];
    else ++m_result.chargerKillsByDepth[index];
    UpdateImportantQuestDefeat(enemy);
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.type != QuestType::Hunt || quest.targetDepth != depth) continue;
        if (quest.targetEnemyType >= 0 && quest.targetEnemyType != static_cast<int>(enemy.type)) continue;
        quest.progress = std::min(quest.targetCount, quest.progress + 1);
        if (quest.progress >= quest.targetCount) quest.status = QuestStatus::Complete;
    }
}

int SceneNarakuProto::CalculateReturnReward() const
{
    double reward = static_cast<double>(m_result.firstAreaCount * 150 + m_result.newRelicTypeCount * 150 + m_result.uniqueReward);
    for (int i = 0; i < 5; ++i)
    {
        const int depth = i + 1;
        reward += static_cast<double>(m_result.minedByDepth[i] * 5) * GetDepthRewardMultiplier(depth);
        reward += static_cast<double>(m_result.chargerKillsByDepth[i] * 10) * GetDepthRewardMultiplier(depth);
        reward += static_cast<double>(m_result.territoryKillsByDepth[i] * 20) * GetDepthRewardMultiplier(depth);
        reward += static_cast<double>(std::min(300.0f, m_result.staySecondsByDepth[i])) * GetDepthStayRewardMultiplier(depth);
    }
    return static_cast<int>(std::llround(reward));
}

bool SceneNarakuProto::IsShiftPress() const
{
    // 左Shiftまたは右Shiftが押されているかを直接確認します。
    if (IsActionDashStepPress()) return true;
    return false;
}

void SceneNarakuProto::StartDeath(const char* reason, DeathCause cause)
{
    CancelFishing(nullptr);
#if defined(NARAKU_EDITOR_BUILD)
    m_player.hp = GetMaxHp();
    m_player.mental = GetMaxMental();
    return;
#endif
    // すでに死亡リザルト中なら二重処理を防ぎます。
    if (m_mode == Mode::DeathResult)
    {
        const int extraLoss = std::max(0, GetDeathLevelLoss(cause) - GetDeathLevelLoss(m_pendingDeathCause));
        const float oldHp = GetMaxHp();
        const float oldStamina = GetMaxStamina();
        const float oldMental = GetMaxMental();
        int applicable = std::min(extraLoss, std::max(0, m_level - 1));
        const int protectedLevels = std::min(applicable, m_levelProtection);
        m_levelProtection -= protectedLevels;
        m_result.protectionConsumed += protectedLevels;
        applicable -= protectedLevels;
        m_level = std::max(1, m_level - applicable);
        m_result.levelAfterDeath = m_level;
        PreserveResourceRatios(oldHp, oldStamina, oldMental);
        if (extraLoss > 0) { m_pendingDeathCause = cause; m_result.reason = reason; SaveProgress(); }
        return;
    }

    const int deathDepth = GetCurrentDepth();
    FailActivePromotionQuest();
    ReturnCarriedLightsToStorage();

    // 死亡理由をリザルトに記録します。
    m_result.reason = reason;
    m_pendingDeathCause = cause;
    m_diedSinceLastDive = true;
    ApplyDeathPenalty(cause);

    // 死亡時に持っていた旧器数を記録します。保険対象なら選択確定まで別領域へ退避します。
    m_result.lostRelics = static_cast<int>(m_inventory.size());

    RestoreLostPropertyQuestTargetsAfterDeath();

    m_pendingDeathRecoveryRelics.clear();
    m_pendingDeathRecoveryBottles.clear();
    m_pendingDeathRecoveryCookingKits.clear();
    m_pendingDeathRecoveryFood = 0;
    m_pendingDeathRecoveryHeatedFood = 0;
    m_pendingDeathRecoveryRationOne = 0;
    m_pendingDeathRecoveryRawFish = 0;
    m_pendingDeathRecoveryCookedFish = 0;
    m_pendingDeathRecoveryRawSizedFish.fill(0);
    m_pendingDeathRecoveryCookedSizedFish.fill(0);
    m_pendingDeathRecoveryCartridges = 0;
    m_deathRecoveryPending = false;
    m_deathRecoveryDiscardConfirm = false;
    m_deathRecoveryDepth = deathDepth;
    m_deathRecoveryFee = 0;
    m_pendingDeathReason.clear();
    m_pendingDeathLevelBefore = m_result.levelBeforeDeath;
    m_pendingDeathLevelAfter = m_result.levelAfterDeath;
    m_pendingDeathProtectionConsumed = m_result.protectionConsumed;

    if (CanRecoverDeathInventory(deathDepth) &&
        (!m_inventory.empty() || m_foodCount > 0 || m_heatedFoodCount > 0 || m_rationOneCount > 0 ||
            m_rawFishCount > 0 || m_cookedFishCount > 0 || m_cartridgeCount > 0 ||
            std::accumulate(m_rawSizedFish.begin(), m_rawSizedFish.end(), 0) > 0 ||
            std::accumulate(m_cookedSizedFish.begin(), m_cookedSizedFish.end(), 0) > 0 ||
            !m_waterBottles.empty() || !m_cookingKits.empty()))
    {
        m_pendingDeathRecoveryRelics = m_inventory;
        for (RelicItem& item : m_pendingDeathRecoveryRelics)
        {
            item.stabilized = true;
        }
        m_pendingDeathRecoveryFood = m_foodCount;
        m_pendingDeathRecoveryHeatedFood = m_heatedFoodCount;
        m_pendingDeathRecoveryRationOne = m_rationOneCount;
        m_pendingDeathRecoveryRawFish = m_rawFishCount;
        m_pendingDeathRecoveryCookedFish = m_cookedFishCount;
        m_pendingDeathRecoveryRawSizedFish = m_rawSizedFish;
        m_pendingDeathRecoveryCookedSizedFish = m_cookedSizedFish;
        m_pendingDeathRecoveryCartridges = m_cartridgeCount;
        m_pendingDeathRecoveryBottles = m_waterBottles;
        for (WaterBottle& bottle : m_pendingDeathRecoveryBottles)
        {
            bottle.selectedForLoadout = false;
        }
        m_pendingDeathRecoveryCookingKits = m_cookingKits;
        for (CookingKit& kit : m_pendingDeathRecoveryCookingKits)
        {
            kit.selectedForLoadout = false;
        }

        const float feeRatio = deathDepth <= 3 ? 0.30f : (deathDepth == 4 ? 0.40f : 0.50f);
        m_deathRecoveryFee = std::max(1, static_cast<int>(std::lround(static_cast<double>(m_money) * feeRatio)));
        m_deathRecoveryPending = true;
        m_pendingDeathReason = reason;
        m_pendingDeathLevelBefore = m_result.levelBeforeDeath;
        m_pendingDeathLevelAfter = m_result.levelAfterDeath;
        m_pendingDeathProtectionConsumed = m_result.protectionConsumed;
    }

    // 死亡結果画面へ移すため、探索中の所持枠は空にします。
    m_inventory.clear();
    m_foodCount = 0;
    m_heatedFoodCount = 0;
    m_rationOneCount = 0;
    m_rawFishCount = 0;
    m_cookedFishCount = 0;
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_cartridgeCount = 0;
    m_waterBottles.clear();
    m_cookingKits.clear();
    m_portableLightOn = false;
    if (cause == DeathCause::Starvation) m_fullness = 50.0f;

    // 死亡リザルトモードへ移行します。
    CommitModeAfterSave(Mode::DeathResult, PresentationScene::Dive);
}

bool SceneNarakuProto::CanRecoverDeathInventory(int deathDepth) const
{
    return deathDepth >= 1 && deathDepth <= GetRankMaximumDepth(m_adventurerRank);
}

bool SceneNarakuProto::HasPendingDeathRecoveryItems() const
{
    return !m_pendingDeathRecoveryRelics.empty() || m_pendingDeathRecoveryFood > 0 ||
        m_pendingDeathRecoveryHeatedFood > 0 || m_pendingDeathRecoveryRationOne > 0 ||
        m_pendingDeathRecoveryRawFish > 0 || m_pendingDeathRecoveryCookedFish > 0 ||
        std::accumulate(m_pendingDeathRecoveryRawSizedFish.begin(), m_pendingDeathRecoveryRawSizedFish.end(), 0) > 0 ||
        std::accumulate(m_pendingDeathRecoveryCookedSizedFish.begin(), m_pendingDeathRecoveryCookedSizedFish.end(), 0) > 0 ||
        m_pendingDeathRecoveryCartridges > 0 || !m_pendingDeathRecoveryBottles.empty() ||
        !m_pendingDeathRecoveryCookingKits.empty();
}

void SceneNarakuProto::RestoreLostPropertyQuestTargetsAfterDeath()
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.type != QuestType::LostProperty || quest.status != QuestStatus::Complete ||
            !quest.targetInteracted)
        {
            continue;
        }

        quest.status = QuestStatus::Active;
        quest.progress = 0;
        quest.targetInteracted = false;
    }
}

void SceneNarakuProto::ResolveDeathRecovery(bool recover)
{
    if (!m_deathRecoveryPending)
    {
        return;
    }

    if (recover)
    {
        const int paid = std::min(m_money, m_deathRecoveryFee);
        m_money -= paid;
        m_questDebt += m_deathRecoveryFee - paid;

        m_storedInventory.insert(m_storedInventory.end(),
            m_pendingDeathRecoveryRelics.begin(), m_pendingDeathRecoveryRelics.end());
        m_storedFoodCount += m_pendingDeathRecoveryFood;
        m_storedHeatedFoodCount += m_pendingDeathRecoveryHeatedFood;
        m_storedRationOneCount += m_pendingDeathRecoveryRationOne;
        m_storedRawFishCount += m_pendingDeathRecoveryRawFish;
        m_storedCookedFishCount += m_pendingDeathRecoveryCookedFish;
        for (size_t i = 0; i < m_storedRawSizedFish.size(); ++i)
        {
            m_storedRawSizedFish[i] += m_pendingDeathRecoveryRawSizedFish[i];
            m_storedCookedSizedFish[i] += m_pendingDeathRecoveryCookedSizedFish[i];
        }
        m_storedCartridgeCount += m_pendingDeathRecoveryCartridges;
        m_storedWaterBottles.insert(m_storedWaterBottles.end(),
            m_pendingDeathRecoveryBottles.begin(), m_pendingDeathRecoveryBottles.end());
        m_storedCookingKits.insert(m_storedCookingKits.end(),
            m_pendingDeathRecoveryCookingKits.begin(), m_pendingDeathRecoveryCookingKits.end());
        ShowCenterNotification(m_deathRecoveryFee > paid
            ? u8"荷物を回収し、不足分を借金へ加算しました。"
            : u8"荷物を回収して自宅へ保管しました。");
        m_result.lostRelics = 0;
    }
    else
    {
        ShowCenterNotification(u8"回収を断り、荷物を破棄しました。");
    }

    m_pendingDeathRecoveryRelics.clear();
    m_pendingDeathRecoveryBottles.clear();
    m_pendingDeathRecoveryCookingKits.clear();
    m_pendingDeathRecoveryFood = 0;
    m_pendingDeathRecoveryHeatedFood = 0;
    m_pendingDeathRecoveryRationOne = 0;
    m_pendingDeathRecoveryRawFish = 0;
    m_pendingDeathRecoveryCookedFish = 0;
    m_pendingDeathRecoveryRawSizedFish.fill(0);
    m_pendingDeathRecoveryCookedSizedFish.fill(0);
    m_pendingDeathRecoveryCartridges = 0;
    m_deathRecoveryPending = false;
    m_deathRecoveryDiscardConfirm = false;
    m_deathRecoveryDepth = 0;
    m_deathRecoveryFee = 0;
    m_pendingDeathReason.clear();
    CommitModeAfterSave(Mode::DeathResult, PresentationScene::Result);
}

void SceneNarakuProto::FinishReturn()
{
    // 帰還理由をリザルトに記録します。
    m_result.reason = u8"生還しました。";

    // 持ち帰った旧器数を記録します。
    m_result.carriedRelics = static_cast<int>(m_inventory.size());

    // 商店で全売却した場合の参考額を集計します。
    m_result.saleAmount = 0;
    m_result.identifiedRelics = 0;
    UpdatePromotionQuestReturn(m_inventory);
    UpdateQuestReturnProgress(m_inventory);

    // 採掘時に決まっていた種類を公開し、遺物を自宅保管へ移します。
    for (const RelicItem& item : m_inventory)
    {
        const std::size_t index = static_cast<std::size_t>(item.type);
        if (!m_identifiedRelics[index])
        {
            m_identifiedRelics[index] = true;
            ++m_result.identifiedRelics;
        }
        RelicItem stored = item;
        stored.stabilized = true;
        m_storedInventory.push_back(stored);
        if (IsRelicSellable(stored)) m_result.saleAmount += stored.value;
        if (item.type == RelicType::Unique)
        {
            m_result.uniqueReward += 1000;
            m_uniqueRelicReturned = true;
            m_uniqueRelicCodexUnlocked = true;
            m_uniqueRelicAchievementUnlocked = true;
            m_uniqueRelicStoryUnlocked = true;
            UpdateImportantQuestUniqueReturn();
        }
    }
    m_result.newRelicTypeCount = m_result.identifiedRelics;
    m_result.explorationReward = CalculateReturnReward();
    const int debtPayment = std::min(m_questDebt, m_result.explorationReward);
    m_questDebt -= debtPayment;
    m_money += m_result.explorationReward - debtPayment;

    m_inventory.clear();

    // 使わずに持ち帰った食料も自宅へ戻します。
    m_storedFoodCount += m_foodCount;
    m_foodCount = 0;
    m_storedHeatedFoodCount += m_heatedFoodCount;
    m_heatedFoodCount = 0;
    m_storedWaterBottles.insert(m_storedWaterBottles.end(), m_waterBottles.begin(), m_waterBottles.end());
    m_waterBottles.clear();
    m_storedCookingKits.insert(m_storedCookingKits.end(), m_cookingKits.begin(), m_cookingKits.end());
    m_cookingKits.clear();
    m_storedRationOneCount += m_rationOneCount;
    m_rationOneCount = 0;
    m_storedRawFishCount += m_rawFishCount;
    m_rawFishCount = 0;
    m_storedCookedFishCount += m_cookedFishCount;
    m_cookedFishCount = 0;
    for (size_t i = 0; i < m_rawSizedFish.size(); ++i)
    {
        m_storedRawSizedFish[i] += m_rawSizedFish[i];
        m_storedCookedSizedFish[i] += m_cookedSizedFish[i];
    }
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_storedCartridgeCount += m_cartridgeCount;
    m_cartridgeCount = 0;
    ReturnCarriedLightsToStorage();

    // 生還した時は精神力を現在の最大値まで回復します。
    m_player.mental = GetMaxMental();

    // 帰還リザルトモードへ移行します。
    CommitModeAfterSave(Mode::ReturnResult, PresentationScene::Dive);
}

void SceneNarakuProto::StartDive(int targetDepth)
{
    if (m_screenFadePhase != ScreenFadePhase::None) return;
    targetDepth = std::max(1, std::min(3, targetDepth));
    if (m_loadoutFoodCount > m_storedFoodCount || m_loadoutHeatedFoodCount > m_storedHeatedFoodCount ||
        m_loadoutRationOneCount > m_storedRationOneCount || m_loadoutRawFishCount > m_storedRawFishCount ||
        m_loadoutCookedFishCount > m_storedCookedFishCount ||
        m_loadoutCartridgeCount > m_storedCartridgeCount)
    {
        AddMessage(u8"持ち込み予定数が自宅在庫を超えています。");
        return;
    }
    for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
    {
        if (m_loadoutRawSizedFish[i] > m_storedRawSizedFish[i] ||
            m_loadoutCookedSizedFish[i] > m_storedCookedSizedFish[i])
        { AddMessage(u8"持ち込み予定数が自宅在庫を超えています。"); return; }
    }
    float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount + m_loadoutHeatedFoodCount);
    loadoutWeight += static_cast<float>(m_loadoutRationOneCount) * kRationOneWeight;
    loadoutWeight += static_cast<float>(m_loadoutRawFishCount + m_loadoutCookedFishCount) * kUnknownFishWeight;
    for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
        loadoutWeight += static_cast<float>(m_loadoutRawSizedFish[i] + m_loadoutCookedSizedFish[i]) * kSizedFishWeights[i];
    loadoutWeight += static_cast<float>(m_loadoutCartridgeCount) * kCartridgeWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
        [](const WaterBottle& bottle) { return bottle.selectedForLoadout; })) * kWaterBottleWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedCookingKits.begin(), m_storedCookingKits.end(),
        [](const CookingKit& kit) { return kit.selectedForLoadout; })) * kCookingKitWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedPortableLights.begin(), m_storedPortableLights.end(),
        [](const PortableLight& light) { return light.selectedForLoadout; })) * kPortableLightWeight;
    for (std::size_t i = 0; i < m_loadoutRelics.size(); ++i)
    {
        if (m_loadoutRelics[i] > CountStoredRelics(static_cast<RelicType>(i)))
        {
            AddMessage(u8"持ち込み予定数が自宅在庫を超えています。");
            return;
        }
        loadoutWeight += GetRelicWeight(static_cast<RelicType>(i)) * static_cast<float>(m_loadoutRelics[i]);
    }
    if (loadoutWeight > GetMaxWeight())
    {
        AddMessage(u8"持ち込み重量が最大重量を超えているため潜行できません。");
        return;
    }

    if (targetDepth > 1)
    {
        const int directIndex = targetDepth - 2;
        const AdventurerRank requiredRank = targetDepth == 2 ? AdventurerRank::Black : AdventurerRank::Purple;
        const int fee = (targetDepth == 2 ? 500 : 2000) +
            250 * m_directGateWeeklyUses[static_cast<std::size_t>(directIndex)];
        if (static_cast<int>(m_adventurerRank) < static_cast<int>(requiredRank))
        {
            AddMessage(u8"現在の階級ではこの直通門を使用できません。");
            return;
        }
        if (m_money < fee)
        {
            AddMessage(u8"直通門の利用料金が不足しています。");
            return;
        }
    }

    m_pendingDiveDepth = targetDepth;
    BeginScreenFade(ScreenFadeAction::StartDive);
}

void SceneNarakuProto::CompleteStartDive()
{
    const PresentationScene sourceScene = GetPresentationScene();
    const float previousHp = m_player.hp;
    const float previousStamina = m_player.stamina;
    const float previousMental = m_player.mental;

    // フィールド側の一時状態を初期化してから、選択済みの持ち込み品を移します。
    if (!ResetRun())
    {
        BuildSurfaceRuntime(true);
        m_mode = Mode::AbyssEntrance;
        return;
    }
    if (m_pendingDiveDepth > 1)
    {
        const int directIndex = m_pendingDiveDepth - 2;
        int destinationArea = m_directGateTargetAreas[static_cast<std::size_t>(directIndex)];
        if (destinationArea < 0 || destinationArea >= static_cast<int>(m_areas.size()) ||
            m_areas[static_cast<std::size_t>(destinationArea)].depth != m_pendingDiveDepth ||
            m_areas[static_cast<std::size_t>(destinationArea)].sublayer != 0)
        {
            const auto destination = std::find_if(m_areas.begin(), m_areas.end(),
                [this](const AreaState& area) { return area.depth == m_pendingDiveDepth && area.sublayer == 0; });
            if (destination == m_areas.end())
            {
                ReportGenerationFailure(u8"直通門の到着先を選べませんでした。", "target upper sublayer is missing");
                BuildSurfaceRuntime(true);
                m_mode = Mode::AbyssEntrance;
                return;
            }
            destinationArea = static_cast<int>(std::distance(m_areas.begin(), destination));
            m_directGateTargetAreas[static_cast<std::size_t>(directIndex)] = destinationArea;
        }
        std::string generationError;
        if (!m_areas[static_cast<std::size_t>(destinationArea)].generated &&
            !GeneratePlannedArea(destinationArea, generationError))
        {
            ReportGenerationFailure(u8"直通門の到着先を生成できませんでした。", generationError);
            BuildSurfaceRuntime(true);
            m_mode = Mode::AbyssEntrance;
            return;
        }
        ActivateArea(destinationArea, false);
        bool foundSafeArrival = false;
        float bestDistanceSquared = std::numeric_limits<float>::max();
        Vec2 safeArrival = m_startPoint;
        float safeDepth = m_startDepth;
        for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
        {
            const float minX = layer.center.x - (layer.gridWidth - 1) * layer.cellSize * 0.5f;
            const float minZ = layer.center.z - (layer.gridHeight - 1) * layer.cellSize * 0.5f;
            for (int z = 0; z < layer.gridHeight - 2; ++z)
            {
                for (int x = 0; x < layer.gridWidth - 2; ++x)
                {
                    bool valid = true;
                    float minimumHeight = std::numeric_limits<float>::max();
                    float maximumHeight = std::numeric_limits<float>::lowest();
                    for (int dz = 0; dz < 2 && valid; ++dz)
                        for (int dx = 0; dx < 2; ++dx)
                        {
                            const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, x + dx, z + dz);
                            if ((flags & (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved |
                                NarakuMap::CellAttributeWaterLake)) != 0u) { valid = false; break; }
                            const float height = NarakuMap::GetVertexHeight(layer, x + dx, z + dz);
                            minimumHeight = std::min(minimumHeight, height);
                            maximumHeight = std::max(maximumHeight, height);
                        }
                    if (!valid || maximumHeight - minimumHeight > 0.20f) continue;
                    const Vec2 candidate = { minX + (x + 1.0f) * layer.cellSize, minZ + (z + 1.0f) * layer.cellSize };
                    const bool nearGate = std::any_of(m_layerGates.begin(), m_layerGates.end(),
                        [&](const LayerGateState& gate) { return Distance(candidate, gate.ropePos) < 3.0f || Distance(candidate, gate.loadPos) < 3.0f; });
                    const bool nearMining = std::any_of(m_miningPoints.begin(), m_miningPoints.end(),
                        [&](const MiningPoint& point) { return Distance(candidate, point.pos) < 2.5f; });
                    const bool nearEnemy = std::any_of(m_enemies.begin(), m_enemies.end(),
                        [&](const EnemyState& enemy) { return Distance(candidate, enemy.pos) < 4.0f; });
                    if (nearGate || nearMining || nearEnemy) continue;
                    const float distanceSquared = candidate.x * candidate.x + candidate.y * candidate.y;
                    if (distanceSquared < bestDistanceSquared)
                    {
                        bestDistanceSquared = distanceSquared;
                        safeArrival = candidate;
                        safeDepth = layer.layerDepth;
                        foundSafeArrival = true;
                    }
                }
            }
        }
        if (!foundSafeArrival)
        {
            for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
            {
                const float minX = layer.center.x - (layer.gridWidth - 1) * layer.cellSize * 0.5f;
                const float minZ = layer.center.z - (layer.gridHeight - 1) * layer.cellSize * 0.5f;
                for (int z = 0; z < layer.gridHeight - 1 && !foundSafeArrival; ++z)
                    for (int x = 0; x < layer.gridWidth - 1; ++x)
                    {
                        const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, x, z);
                        if ((flags & (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved |
                            NarakuMap::CellAttributeWaterLake)) != 0u) continue;
                        const Vec2 candidate = { minX + (x + 0.5f) * layer.cellSize, minZ + (z + 0.5f) * layer.cellSize };
                        const bool occupied = std::any_of(m_layerGates.begin(), m_layerGates.end(),
                            [&](const LayerGateState& gate) { return Distance(candidate, gate.ropePos) < 2.0f || Distance(candidate, gate.loadPos) < 2.0f; }) ||
                            std::any_of(m_miningPoints.begin(), m_miningPoints.end(),
                                [&](const MiningPoint& point) { return Distance(candidate, point.pos) < 2.0f; }) ||
                            std::any_of(m_enemies.begin(), m_enemies.end(),
                                [&](const EnemyState& enemy) { return Distance(candidate, enemy.pos) < 3.0f; });
                        if (occupied) continue;
                        safeArrival = candidate;
                        safeDepth = layer.layerDepth;
                        foundSafeArrival = true;
                        break;
                    }
                if (foundSafeArrival) break;
            }
        }
        if (!foundSafeArrival)
        {
            ReportGenerationFailure(u8"直通門の安全な到着地点を確保できませんでした。",
                "no flat walkable arrival cell away from enemies, mining points, water and layer gates");
            BuildSurfaceRuntime(true);
            m_mode = Mode::AbyssEntrance;
            return;
        }
        m_player.pos = safeArrival;
        m_player.depth = safeDepth;
        m_player.previousDepth = safeDepth;
        m_player.feetWorldY = GetGroundWorldY(safeArrival, safeDepth);
        m_player.previousWorldY = m_player.feetWorldY;
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.lastSafeGroundPos = safeArrival;
        m_player.lastSafeGroundDepth = safeDepth;
        m_player.hasSafeGroundPos = true;
        AreaState& arrivedArea = m_areas[static_cast<std::size_t>(destinationArea)];
        if (!arrivedArea.firstAreaExpAwarded)
        {
            arrivedArea.firstAreaExpAwarded = true;
            arrivedArea.firstAreaRewardAwarded = true;
            ++m_result.firstAreaCount;
            AwardExp(static_cast<int>(std::round(100.0f * GetDepthExpMultiplier(arrivedArea.depth))));
        }
        m_directGateTargetPositions[static_cast<std::size_t>(directIndex)] = m_player.pos;
        const int fee = (m_pendingDiveDepth == 2 ? 500 : 2000) +
            250 * m_directGateWeeklyUses[static_cast<std::size_t>(directIndex)];
        m_money -= fee;
        ++m_directGateWeeklyUses[static_cast<std::size_t>(directIndex)];
    }
    if (!m_diedSinceLastDive)
    {
        m_player.hp = std::min(GetMaxHp(), previousHp);
        m_player.stamina = std::min(GetMaxStamina(), previousStamina);
        m_player.mental = std::min(GetMaxMental(), previousMental);
    }
    m_diedSinceLastDive = false;
    m_foodCount = m_loadoutFoodCount;
    m_storedFoodCount -= m_loadoutFoodCount;
    m_loadoutFoodCount = 0;
    m_heatedFoodCount = m_loadoutHeatedFoodCount;
    m_storedHeatedFoodCount -= m_loadoutHeatedFoodCount;
    m_loadoutHeatedFoodCount = 0;
    m_rationOneCount = m_loadoutRationOneCount;
    m_storedRationOneCount -= m_loadoutRationOneCount;
    m_loadoutRationOneCount = 0;
    m_rawFishCount = m_loadoutRawFishCount;
    m_storedRawFishCount -= m_loadoutRawFishCount;
    m_loadoutRawFishCount = 0;
    m_cookedFishCount = m_loadoutCookedFishCount;
    m_storedCookedFishCount -= m_loadoutCookedFishCount;
    m_loadoutCookedFishCount = 0;
    for (size_t i = 0; i < m_rawSizedFish.size(); ++i)
    {
        m_rawSizedFish[i] = m_loadoutRawSizedFish[i];
        m_storedRawSizedFish[i] -= m_loadoutRawSizedFish[i];
        m_loadoutRawSizedFish[i] = 0;
        m_cookedSizedFish[i] = m_loadoutCookedSizedFish[i];
        m_storedCookedSizedFish[i] -= m_loadoutCookedSizedFish[i];
        m_loadoutCookedSizedFish[i] = 0;
    }
    m_cartridgeCount = m_loadoutCartridgeCount;
    m_storedCartridgeCount -= m_loadoutCartridgeCount;
    m_loadoutCartridgeCount = 0;

    for (auto it = m_storedWaterBottles.begin(); it != m_storedWaterBottles.end();)
    {
        if (it->selectedForLoadout)
        {
            m_waterBottles.push_back(*it);
            it = m_storedWaterBottles.erase(it);
        }
        else ++it;
    }
    for (auto it = m_storedCookingKits.begin(); it != m_storedCookingKits.end();)
    {
        if (it->selectedForLoadout)
        {
            m_cookingKits.push_back(*it);
            it = m_storedCookingKits.erase(it);
        }
        else ++it;
    }
    for (auto it = m_storedPortableLights.begin(); it != m_storedPortableLights.end();)
    {
        if (it->selectedForLoadout)
        {
            m_portableLights.push_back(*it);
            it = m_storedPortableLights.erase(it);
        }
        else ++it;
    }

    for (std::size_t i = 0; i < m_loadoutRelics.size(); ++i)
    {
        const RelicType type = static_cast<RelicType>(i);
        const int count = m_loadoutRelics[i];
        int remaining = count;
        for (auto it = m_storedInventory.begin(); it != m_storedInventory.end() && remaining > 0;)
        {
            if (it->type == type)
            {
                RelicItem item = *it;
                item.stabilized = true;
                m_inventory.push_back(item);
                it = m_storedInventory.erase(it);
                --remaining;
            }
            else ++it;
        }
        m_loadoutRelics[i] = 0;
    }
    if (CommitModeAfterSave(Mode::Explore, sourceScene))
    {
        UpdateImportantQuestArrival();
        SaveProgress();
    }
}

void SceneNarakuProto::RestartAfterDeath()
{
    // 死亡後は持ち込みなしで再挑戦します。
    m_loadoutFoodCount = 0;
    m_loadoutHeatedFoodCount = 0;
    m_loadoutRationOneCount = 0;
    m_loadoutRawFishCount = 0;
    m_loadoutCookedFishCount = 0;
    m_loadoutRawSizedFish.fill(0);
    m_loadoutCookedSizedFish.fill(0);
    m_loadoutCartridgeCount = 0;
    m_loadoutRelics.fill(0);
    for (WaterBottle& bottle : m_storedWaterBottles) bottle.selectedForLoadout = false;
    for (CookingKit& kit : m_storedCookingKits) kit.selectedForLoadout = false;
    StartDive();
}

void SceneNarakuProto::AbandonDive()
{
    if (m_mode == Mode::Home || m_mode == Mode::GeneralShop || m_mode == Mode::Armory || m_mode == Mode::Restaurant) return;
    FailActivePromotionQuest();
    ApplyAbandonPenalty();
    if (m_mode == Mode::RelicPrompt && m_pendingRelicMiningIndex >= 0)
    {
        m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true,
            m_pendingRelicMiningIndex });
        m_pendingRelicMiningIndex = -1;
    }
    m_result.reason = u8"探窟を放棄しました。";
    m_result.lostRelics = static_cast<int>(m_inventory.size());
    m_inventory.clear();
    m_foodCount = 0;
    m_heatedFoodCount = 0;
    m_rationOneCount = 0;
    m_rawFishCount = 0;
    m_cookedFishCount = 0;
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_cartridgeCount = 0;
    m_waterBottles.clear();
    m_cookingKits.clear();
    m_portableLights.clear();
    m_portableLightOn = false;
    CaptureWeeklyWorld();
    if (BuildSurfaceRuntime(false)) CommitModeAfterSave(Mode::Home, PresentationScene::Dive);
}
