/**
 * @file SceneNarakuProto.World.cpp
 * @brief 潜行構造の生成、地上・エリア構築、および画面遷移を実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

void SceneNarakuProto::ResetDiveRuntimeState(int preservedMoney)
{
    m_player = PlayerState();
    m_player.hp = GetMaxHp();
    m_player.stamina = GetMaxStamina();
    m_player.mental = GetMaxMental();
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
    m_groundRelics.clear();
    m_runRelicAcquisitionDepths.clear();
    m_groundFoods.clear();
    m_miningPoints.clear();
    m_fishingPoints.clear();
    m_enemies.clear();
    m_attackHitEffects.clear();
    m_jumpEffects.clear();
    m_characterAnimationTime = 0.0f;
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_layerGates.clear();
    m_areas.clear();
    m_currentAreaIndex = -1;
    m_activeRope = -1;
    m_ropeProgress = 0.0f;
    m_pins.clear();
    m_surfaceFacilities.clear();
    m_messages.clear();
    m_centerNotification.clear();
    m_centerNotificationTimer = 0.0f;
    m_loadingSourceGateIndex = -1;
    m_pendingUninsuredGateIndex = -1;
    m_uninsuredDescentAcceptedThisDive = false;
    m_loadingStep = 0;
    m_loadingProgress = 0.0f;
    m_loadingStatus.clear();
    m_generationFailureSummary.clear();
    m_generationFailureDetail.clear();
    m_openGenerationFailurePopup = false;
    m_transitionSourceGateIndex = -1;
    m_transitionDestinationAreaIndex = -1;
    m_transitionDestinationGateIndex = -1;
    m_layerTransitionProgress = 0.0f;
    m_layerTransitionVisualOffset = 0.0f;
    m_layerTransitionAscending = false;
    m_heavyRunNotificationShown = false;
    m_shiftHold = 0.0f;
    m_shiftPendingStep = false;
    m_shiftWasPressed = false;
    m_shiftRunCommitted = false;
    m_qHoldTime = 0.0f;
    m_qWasPressed = false;
    m_qLongTriggered = false;
    m_upperLoadWardTimer = 0.0f;
    m_upperLoadVisionTimer = 0.0f;
    m_upperLoadVisionOcclusion = 0.0f;
    m_upperLoadFifthTimer = 0.0f;
    m_upperLoadFifthDamageRatio = 0.0f;
    m_upperLoadFifthDamageCooldown = 0.0f;
    m_miningSenseTimer = 0.0f;
    m_movementExpByDepth.fill(0.0f);
    m_cameraShakeTimer = 0.0f;
    m_miningTimer = 0.0f;
    m_miningDuration = kMiningTime;
    m_miningIndex = -1;
    m_fishingPhase = FishingPhase::None;
    m_fishingPointIndex = -1;
    m_fishingTimer = 0.0f;
    m_foodUseTimer = 0.0f;
    m_usingHeatedFood = false;
    m_unknownWeaponChargeTimer = 0.0f;
    m_unknownWeaponFiredThisHold = false;
    m_cookingTimer = 0.0f;
    m_cookingPreviousHp = 0.0f;
    m_cookingTarget = CookingTarget::None;
    m_cookingBottleIndex = -1;
    m_pendingWaterFlags = NarakuMap::CellAttributeNone;
    m_selectedInventory = -1;
    m_mapScrollOffset = { 0.0f, 0.0f };
    m_mode = Mode::Explore;
    m_result = RunResult();
    m_pendingDeathCause = DeathCause::Other;
    m_pendingRelicDepth = 0.0f;
    m_pendingRelicMiningIndex = -1;
    m_money = preservedMoney;

}
bool SceneNarakuProto::RestoreWeeklyRun()
{
#if !defined(NARAKU_EDITOR_BUILD)
    if (!m_weeklyAreas.empty())
    {
        m_areas = m_weeklyAreas;
        const auto start = std::find_if(m_areas.begin(), m_areas.end(),
            [](const AreaState& area) { return area.canReturn && area.generated; });
        if (start != m_areas.end())
        {
            const int startAreaIndex = static_cast<int>(std::distance(m_areas.begin(), start));
            ActivateArea(startAreaIndex, false);
            m_player.pos = m_startPoint;
            m_player.depth = m_startDepth;
            m_player.previousDepth = m_player.depth;
            m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
            m_player.previousWorldY = m_player.feetWorldY;
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.lastSafeGroundPos = m_player.pos;
            m_player.lastSafeGroundDepth = m_player.depth;
            m_player.hasSafeGroundPos = true;
            EnsureQuestBoard();
            return true;
        }
        m_weeklyAreas.clear();
        m_areas.clear();
    }
#endif
    return false;
}
bool SceneNarakuProto::ResetRun(bool generateCompleteEditorPreview)
{
#if !defined(NARAKU_EDITOR_BUILD)
    (void)generateCompleteEditorPreview;
#endif
    int keepMoney = m_money;
    bool generated = false;
    std::string mapError;

    LoadEnvironmentModels();

#if !defined(NARAKU_EDITOR_BUILD)
    CaptureWeeklyWorld();
    if (m_weekResetPending)
    {
        m_weeklyAreas.clear();
        m_weekResetPending = false;
    }
    m_diveWorldSeed = m_weekSeed;
#endif

    ResetDiveRuntimeState(keepMoney);

#if defined(NARAKU_EDITOR_BUILD)
    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    SeedRuntimeRandom(preview.seed);
#else
    SeedRuntimeRandom(m_weekSeed);
#endif

    if (RestoreWeeklyRun())
    {
        return true;
    }


    if (!BuildDiveStructure())
    {
        ReportGenerationFailure(
            u8"15段階の接続構成を生成できませんでした。",
            u8"各段階のエリア数と層間出入口数を満たす接続構成が、最大試行回数内に作成できませんでした。");
        m_mode = Mode::Home;
        return false;
    }

    int initialAreaIndex = -1;
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (m_areas[areaIndex].canReturn) initialAreaIndex = areaIndex;
#if defined(NARAKU_EDITOR_BUILD)
        if (!preview.spawnAtReturnArea &&
            m_areas[areaIndex].depth == preview.depth &&
            m_areas[areaIndex].sublayer == preview.sublayer &&
            m_areas[areaIndex].areaNumber == preview.area)
        {
            initialAreaIndex = areaIndex;
            break;
        }
#endif
    }

    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (areaIndex != initialAreaIndex) continue;
        m_currentAreaIndex = areaIndex;
        int entryCount = 0;
        int exitCount = 0;
        for (const PlannedLayerGate& gate : m_areas[areaIndex].plannedGates)
            gate.isEntry ? ++entryCount : ++exitCount;
        const NarakuStageGenerator::AreaGenerationContext generationContext = {
            m_areas[areaIndex].depth, m_areas[areaIndex].sublayer, m_areas[areaIndex].areaNumber };
#if !defined(NARAKU_EDITOR_BUILD)
        constexpr const wchar_t* generated4x4MapPath = L"Assets/Maps/generated_naraku_map_4x4.json";
#endif
        for (int attempt = 0; attempt < kMapGenerationMaxAttempts; ++attempt)
        {
#if defined(NARAKU_EDITOR_BUILD)
            if (NarakuStageGenerator::GenerateFixed4x4AreaMapData(
                    m_runtimeMap, entryCount, exitCount, m_areas[areaIndex].canReturn, &generationContext, &mapError))
#else
            if (NarakuStageGenerator::GenerateFixed4x4AreaMap(
                    generated4x4MapPath, entryCount, exitCount, m_areas[areaIndex].canReturn, &generationContext, &mapError) &&
                NarakuMap::LoadMap(generated4x4MapPath, m_runtimeMap, &mapError))
#endif
            {
                generated = true;
#if !defined(NARAKU_EDITOR_BUILD)
                NarakuMap::SetCurrentMapPath(generated4x4MapPath);
#endif
                break;
            }
        }
        break;
    }

    if (!generated)
    {
        std::ostringstream summary;
        if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
        {
            const AreaState& failedArea = m_areas[static_cast<std::size_t>(m_currentAreaIndex)];
            summary << u8"初期エリアの生成に失敗しました: 第" << failedArea.depth << u8"層 "
                << GetSublayerName(failedArea.sublayer) << u8" エリア" << failedArea.areaNumber;
        }
        else
        {
            summary << u8"開始エリアを特定できませんでした。";
        }
        ReportGenerationFailure(summary.str(), mapError);
        m_mode = Mode::Home;
        return false;
    }

    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_worldHalfSize = 1.0f;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        const float halfWidth = static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f;
        const float halfHeight = static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f;
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.x) + halfWidth);
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.z) + halfHeight);
    }

    auto getLayerDepthById = [this](int layerId) -> float
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, layerId);
        return (layerIndex >= 0) ? m_runtimeMap.terrainLayers[layerIndex].layerDepth : 0.0f;
    };

    auto getFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    m_startPoint = { m_runtimeMap.playerStartPoint.xz.x, m_runtimeMap.playerStartPoint.xz.z };
    m_startDepth = getLayerDepthById(m_runtimeMap.playerStartPoint.layerId);
    m_returnPoint = m_startPoint;
    m_returnDepth = m_startDepth;

    m_player.pos = m_startPoint;
    m_player.depth = m_startDepth;
    m_player.previousDepth = m_startDepth;
    m_player.facing = { 0.0f, 1.0f };
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }

        const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
        const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
        m_floorRegions.push_back({
            { layer.center.x, layer.center.z },
            { width * 0.5f, height * 0.5f },
            layer.layerDepth,
            getFloorColor(layer.groundTextureId),
            layer.id });
    }

    for (const NarakuMap::RopePoint& rope : m_runtimeMap.ropes)
    {
        const int topIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.topLayerId);
        const int bottomIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        m_ropePoints.push_back({
            { rope.topXZ.x, rope.topXZ.z },
            { rope.bottomXZ.x, rope.bottomXZ.z },
            m_runtimeMap.terrainLayers[topIndex].layerDepth,
            m_runtimeMap.terrainLayers[bottomIndex].layerDepth });
    }

    for (const NarakuMap::LayerGatePoint& gate : m_runtimeMap.layerGates)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, gate.layerId);
        if (layerIndex < 0)
        {
            continue;
        }
        LayerGateState runtimeGate;
        runtimeGate.isEntry = gate.isEntry;
        runtimeGate.ropePos = { gate.ropeXZ.x, gate.ropeXZ.z };
        runtimeGate.loadPos = { gate.loadXZ.x, gate.loadXZ.z };
        runtimeGate.depth = m_runtimeMap.terrainLayers[layerIndex].layerDepth;
        m_layerGates.push_back(runtimeGate);
    }

    if (!AssignPlannedGates(m_currentAreaIndex))
    {
        const AreaState& failedArea = m_areas[static_cast<std::size_t>(m_currentAreaIndex)];
        std::ostringstream summary;
        summary << u8"層間出入口の割り当てに失敗しました: 第" << failedArea.depth << u8"層 "
            << GetSublayerName(failedArea.sublayer) << u8" エリア" << failedArea.areaNumber;
        ReportGenerationFailure(summary.str(), u8"生成された層間出入口の数または入口・出口の種別が、接続計画と一致しません。");
        m_mode = Mode::Home;
        return false;
    }

    int fallbackRelicIndex = 0;
    for (const NarakuMap::MiningPoint& point : m_runtimeMap.miningPoints)
    {
        if (!point.enabled)
        {
            continue;
        }

        MiningPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.visualType = point.visualType;
        runtimePoint.discovered = point.discovered;
        /** 再潜行ごとに採掘状態を初期化し、今回の潜行中だけ更新します。 */
        runtimePoint.mined = false;
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.relicName = point.relicName.empty() ? kRelicNames[fallbackRelicIndex % 8] : point.relicName;

        ++fallbackRelicIndex;
        m_miningPoints.push_back(runtimePoint);
    }

    SpawnEnemiesForCurrentArea();
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
    AddMessage(u8"奈落塔プロトタイプを開始しました。");
    if (generated)
    {
        AreaState& startArea = m_areas[m_currentAreaIndex];
        startArea.generated = true;
        startArea.firstAreaExpAwarded = true;
        startArea.firstAreaRewardAwarded = true;
        m_result.firstAreaCount = 1;
        AwardExp(static_cast<int>(100.0f * GetDepthExpMultiplier(1)));
        SaveCurrentAreaState();
#if defined(NARAKU_EDITOR_BUILD)
        if (generateCompleteEditorPreview)
        {
            const int previewStartArea = m_currentAreaIndex;
            for (int previewArea = 0; previewArea < static_cast<int>(m_areas.size()); ++previewArea)
            {
                if (previewArea == previewStartArea) continue;
                std::string previewError;
                if (!GeneratePlannedArea(previewArea, previewError))
                {
                    m_mode = Mode::Home;
                    const AreaState& failedArea = m_areas[static_cast<std::size_t>(previewArea)];
                    std::ostringstream failure;
                    failure << u8"生成プレビュー失敗: 第" << failedArea.depth << u8"層 "
                        << GetSublayerName(failedArea.sublayer) << u8" エリア" << failedArea.areaNumber
                        << " / seed=" << preview.seed;
                    ReportGenerationFailure(failure.str(), previewError);
                    return false;
                }
            }
            ActivateArea(previewStartArea, false);
            m_player.pos = m_startPoint;
            m_player.depth = m_startDepth;
            m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        }
#endif
    }
    EnsureQuestBoard();
    if (!generated)
    {
        m_mode = Mode::Home;
    }
    return generated;
}

bool SceneNarakuProto::BuildDiveStructure()
{
    constexpr int stageCount = 15;
    constexpr int maximumGatesPerArea = 4;
    constexpr int maximumAttempts = 2000;

    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    for (int attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        m_areas.clear();
        std::array<std::vector<int>, stageCount> stageAreas;
        for (int stage = 0; stage < stageCount; ++stage)
        {
            const int configuredAreaCount = preview.areaCounts[static_cast<std::size_t>(stage)];
            const int areaCount = configuredAreaCount >= 1 && configuredAreaCount <= 3
                ? configuredAreaCount + 1 : RandomInt(2, 4);
            for (int number = 0; number < areaCount; ++number)
            {
                const int areaIndex = static_cast<int>(m_areas.size());
                m_areas.emplace_back();
                AreaState& area = m_areas.back();
                area.depth = stage / 3 + 1;
                area.sublayer = stage % 3;
                area.areaNumber = number + 1;
                stageAreas[stage].push_back(areaIndex);
            }
        }

        const int startAreaIndex = stageAreas[0][RandomInt(0, static_cast<int>(stageAreas[0].size()) - 1)];
        m_areas[startAreaIndex].canReturn = true;
        int nextConnectionId = 1;
        bool failed = false;

        const auto shuffledIndices = [](int count)
        {
            std::vector<int> result(static_cast<std::size_t>(count));
            for (int index = 0; index < count; ++index) result[static_cast<std::size_t>(index)] = index;
            for (int index = count - 1; index > 0; --index)
            {
                const int swapIndex = RandomInt(0, index);
                std::swap(result[static_cast<std::size_t>(index)], result[static_cast<std::size_t>(swapIndex)]);
            }
            return result;
        };

        for (int stage = 0; stage < stageCount - 1 && !failed; ++stage)
        {
            const int upperCount = static_cast<int>(stageAreas[stage].size());
            const int lowerCount = static_cast<int>(stageAreas[stage + 1].size());
            std::vector<std::pair<int, int>> edges;

            if (stage == 0)
            {
                const int edgeCount = upperCount + lowerCount - 1 + RandomInt(0, 1);
                bool connected = false;
                for (int edgeAttempt = 0; edgeAttempt < 1000 && !connected; ++edgeAttempt)
                {
                    edges.clear();
                    std::vector<int> upperDegree(static_cast<std::size_t>(upperCount), 0);
                    std::vector<int> lowerDegree(static_cast<std::size_t>(lowerCount), 0);
                    for (int edge = 0; edge < edgeCount; ++edge)
                    {
                        const int upper = RandomInt(0, upperCount - 1);
                        const int lower = RandomInt(0, lowerCount - 1);
                        edges.push_back({ upper, lower });
                        ++upperDegree[static_cast<std::size_t>(upper)];
                        ++lowerDegree[static_cast<std::size_t>(lower)];
                    }
                    if (std::find(upperDegree.begin(), upperDegree.end(), 0) != upperDegree.end() ||
                        std::find(lowerDegree.begin(), lowerDegree.end(), 0) != lowerDegree.end()) continue;

                    std::vector<bool> reachedUpper(static_cast<std::size_t>(upperCount), false);
                    std::vector<bool> reachedLower(static_cast<std::size_t>(lowerCount), false);
                    reachedUpper[static_cast<std::size_t>(
                        std::find(stageAreas[0].begin(), stageAreas[0].end(), startAreaIndex) - stageAreas[0].begin())] = true;
                    bool changed = true;
                    while (changed)
                    {
                        changed = false;
                        for (const auto& edge : edges)
                        {
                            if (reachedUpper[static_cast<std::size_t>(edge.first)] && !reachedLower[static_cast<std::size_t>(edge.second)])
                            { reachedLower[static_cast<std::size_t>(edge.second)] = true; changed = true; }
                            if (reachedLower[static_cast<std::size_t>(edge.second)] && !reachedUpper[static_cast<std::size_t>(edge.first)])
                            { reachedUpper[static_cast<std::size_t>(edge.first)] = true; changed = true; }
                        }
                    }
                    connected = std::find(reachedUpper.begin(), reachedUpper.end(), false) == reachedUpper.end() &&
                        std::find(reachedLower.begin(), reachedLower.end(), false) == reachedLower.end();
                }
                if (!connected) failed = true;
            }
            else
            {
                const std::vector<int> upperOrder = shuffledIndices(upperCount);
                const std::vector<int> lowerOrder = shuffledIndices(lowerCount);
                if (upperCount >= lowerCount)
                {
                    for (int index = 0; index < upperCount; ++index)
                        edges.push_back({ upperOrder[static_cast<std::size_t>(index)],
                            lowerOrder[static_cast<std::size_t>(index % lowerCount)] });
                }
                else
                {
                    for (int index = 0; index < lowerCount; ++index)
                        edges.push_back({ upperOrder[static_cast<std::size_t>(index % upperCount)],
                            lowerOrder[static_cast<std::size_t>(index)] });
                }
                if (RandomInt(0, 1) != 0)
                    edges.push_back({ RandomInt(0, upperCount - 1), RandomInt(0, lowerCount - 1) });
            }

            for (const auto& edge : edges)
            {
                const int upperArea = stageAreas[stage][static_cast<std::size_t>(edge.first)];
                const int lowerArea = stageAreas[stage + 1][static_cast<std::size_t>(edge.second)];
                const int connectionId = nextConnectionId++;
                m_areas[upperArea].plannedGates.push_back({ false, lowerArea, connectionId });
                m_areas[lowerArea].plannedGates.push_back({ true, upperArea, connectionId });
            }
        }

        if (failed) continue;
        for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
        {
            const AreaState& area = m_areas[areaIndex];
            if (area.plannedGates.empty() || static_cast<int>(area.plannedGates.size()) > maximumGatesPerArea)
            { failed = true; break; }
            const bool hasEntry = std::any_of(area.plannedGates.begin(), area.plannedGates.end(),
                [](const PlannedLayerGate& gate) { return gate.isEntry; });
            const bool hasExit = std::any_of(area.plannedGates.begin(), area.plannedGates.end(),
                [](const PlannedLayerGate& gate) { return !gate.isEntry; });
            if (area.sublayer + (area.depth - 1) * 3 > 0 && !hasEntry) { failed = true; break; }
            if (area.sublayer + (area.depth - 1) * 3 < stageCount - 1 && !hasExit) { failed = true; break; }
        }
        if (failed) continue;

        std::vector<bool> reached(m_areas.size(), false);
        std::vector<int> queue = { startAreaIndex };
        reached[static_cast<std::size_t>(startAreaIndex)] = true;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        {
            for (const PlannedLayerGate& gate : m_areas[queue[cursor]].plannedGates)
            {
                if (gate.destinationAreaIndex < 0 || reached[static_cast<std::size_t>(gate.destinationAreaIndex)]) continue;
                reached[static_cast<std::size_t>(gate.destinationAreaIndex)] = true;
                queue.push_back(gate.destinationAreaIndex);
            }
        }
        if (std::find(reached.begin(), reached.end(), false) != reached.end()) continue;
        return true;
    }
    m_areas.clear();
    return false;
}

bool SceneNarakuProto::AssignPlannedGates(int areaIndex)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size())) return false;
    const std::vector<PlannedLayerGate>& planned = m_areas[areaIndex].plannedGates;
    std::vector<bool> assigned(planned.size(), false);
    for (LayerGateState& gate : m_layerGates)
    {
        bool found = false;
        for (std::size_t index = 0; index < planned.size(); ++index)
        {
            if (assigned[index] || planned[index].isEntry != gate.isEntry) continue;
            gate.destinationAreaIndex = planned[index].destinationAreaIndex;
            gate.connectionId = planned[index].connectionId;
            assigned[index] = true;
            found = true;
            break;
        }
        if (!found) return false;
    }
    return m_layerGates.size() == planned.size() &&
        std::find(assigned.begin(), assigned.end(), false) == assigned.end();
}

bool SceneNarakuProto::GeneratePlannedArea(int areaIndex, std::string& outError)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size()))
    {
        outError = u8"生成対象のエリア番号が範囲外です。";
        return false;
    }
    AreaState& area = m_areas[areaIndex];
    int entryCount = 0;
    int exitCount = 0;
    for (const PlannedLayerGate& gate : area.plannedGates) gate.isEntry ? ++entryCount : ++exitCount;
    const NarakuStageGenerator::AreaGenerationContext generationContext = {
        area.depth, area.sublayer, area.areaNumber };

    wchar_t generatedPath[128] = {};
#if !defined(NARAKU_EDITOR_BUILD)
    std::swprintf(generatedPath, 128, L"Assets/Maps/generated_area_%03d.json", areaIndex);
#endif
    NarakuMap::MapData generatedMap;
    bool generated = false;
    for (int attempt = 0; attempt < kMapGenerationMaxAttempts; ++attempt)
    {
#if defined(NARAKU_EDITOR_BUILD)
        if (NarakuStageGenerator::GenerateFixed4x4AreaMapData(
                generatedMap, entryCount, exitCount, area.canReturn, &generationContext, &outError))
#else
        if (NarakuStageGenerator::GenerateFixed4x4AreaMap(
                generatedPath, entryCount, exitCount, area.canReturn, &generationContext, &outError) &&
            NarakuMap::LoadMap(generatedPath, generatedMap, &outError))
#endif
        { generated = true; break; }
    }
    if (!generated) return false;

    m_runtimeMap = std::move(generatedMap);
    m_currentAreaIndex = areaIndex;
    BuildCurrentAreaRuntime(false);
    if (!AssignPlannedGates(areaIndex))
    {
        outError = u8"生成された層間出入口の数または入口・出口の種別が、接続計画と一致しません。";
        return false;
    }
    area.generated = true;
    SaveCurrentAreaState();
    return true;
}

const char* SceneNarakuProto::GetSublayerName(int sublayer) const
{
    switch (sublayer)
    {
    case 0: return u8"上層";
    case 1: return u8"中層";
    case 2: return u8"下層";
    default: return u8"不明";
    }
}

void SceneNarakuProto::SaveCurrentAreaState()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    AreaState& area = m_areas[m_currentAreaIndex];
    area.map = m_runtimeMap;
    area.groundRelics = m_groundRelics;
    area.groundFoods = m_groundFoods;
    area.miningPoints = m_miningPoints;
    area.fishingPoints = m_fishingPoints;
    area.enemies = m_enemies;
    area.floorRegions = m_floorRegions;
    area.ropePoints = m_ropePoints;
    area.layerGates = m_layerGates;
    area.pins = m_pins;
    area.startPoint = m_startPoint;
    area.startDepth = m_startDepth;
    area.returnPoint = m_returnPoint;
    area.returnDepth = m_returnDepth;
    area.worldHalfSize = m_worldHalfSize;
}

void SceneNarakuProto::CaptureWeeklyWorld()
{
#if defined(NARAKU_EDITOR_BUILD)
    return;
#else
    SaveCurrentAreaState();
    if (!m_areas.empty() && !m_weekResetPending) m_weeklyAreas = m_areas;
#endif
}

bool SceneNarakuProto::SaveWeeklyWorld() const
{
#if defined(NARAKU_EDITOR_BUILD)
    return true;
#else
    _wmkdir(kWeeklyWorldDirectory);
    std::ofstream stream(kWeeklyWorldTempPath, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    const auto write = [&stream](const auto& value)
    { stream.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
    const auto writeString = [&stream, &write](const std::string& value)
    {
        const std::uint64_t size = value.size();
        write(size);
        stream.write(value.data(), static_cast<std::streamsize>(size));
    };
    const auto writePodVector = [&stream, &write](const auto& values)
    {
        const std::uint64_t size = values.size();
        write(size);
        if (size > 0) stream.write(reinterpret_cast<const char*>(values.data()),
            static_cast<std::streamsize>(sizeof(values.front()) * size));
    };
    const std::uint32_t magic = 0x37574E4E;
    write(magic);
    write(m_diveWorldSeed);
    const std::uint64_t areaCount = m_weeklyAreas.size();
    write(areaCount);
    for (std::size_t areaIndex = 0; areaIndex < m_weeklyAreas.size(); ++areaIndex)
    {
        const AreaState& area = m_weeklyAreas[areaIndex];
        write(area.depth); write(area.sublayer); write(area.areaNumber); write(area.generated); write(area.canReturn);
        writePodVector(area.plannedGates);
        write(area.sensingTimer); write(area.respawnClock);
        write(area.discoveredEnemyCount); write(area.discoveredMiningCount); write(area.discoveredCliffCount);
        write(area.totalCliffCount); write(area.firstAreaExpAwarded); write(area.firstAreaRewardAwarded);
        writePodVector(area.discoveredCells); writePodVector(area.discoveredCliffs); write(area.cellExpThresholds);
        if (!area.generated) continue;
        wchar_t mapPath[160] = {};
        std::swprintf(mapPath, 160, L"Assets/Save/WeeklyWorld/area_%03zu.json", areaIndex);
        std::string mapError;
        if (!NarakuMap::SaveMap(mapPath, area.map, &mapError)) return false;
        const std::uint64_t relicCount = area.groundRelics.size(); write(relicCount);
        for (const GroundRelic& relic : area.groundRelics)
        {
            writeString(relic.item.name); write(relic.item.type); write(relic.item.weight); write(relic.item.value);
            write(relic.item.maxUses); write(relic.item.remainingUses); write(relic.item.acquisitionOrder);
            write(relic.item.broken); write(relic.item.stabilized); write(relic.item.autoTrigger);
            write(relic.pos); write(relic.depth); write(relic.active); write(relic.sourceMiningIndex);
        }
        writePodVector(area.groundFoods);
        const std::uint64_t miningCount = area.miningPoints.size(); write(miningCount);
        for (const MiningPoint& point : area.miningPoints)
        {
            write(point.pos); write(point.depth); write(point.visualType); write(point.discovered); write(point.mined);
            write(point.respawnTimer); write(point.extractionCount); write(point.outputPending); write(point.sensed);
            writeString(point.relicName);
        }
        const std::uint64_t fishingCount = area.fishingPoints.size(); write(fishingCount);
        for (const FishingPoint& point : area.fishingPoints)
        {
            write(point.pos); write(point.depth); write(point.lake); write(point.discovered);
            write(point.remainingUses); write(point.rechargeGameSeconds); writeString(point.id);
        }
        writePodVector(area.enemies); writePodVector(area.layerGates); writePodVector(area.pins);
    }
    stream.close();
    if (!stream.good()) return false;
    return MoveFileExW(kWeeklyWorldTempPath, kWeeklyWorldPath,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#endif
}

bool SceneNarakuProto::LoadWeeklyWorld()
{
#if defined(NARAKU_EDITOR_BUILD)
    return false;
#else
    std::ifstream stream(kWeeklyWorldPath, std::ios::binary);
    if (!stream) return false;
    const auto read = [&stream](auto& value)
    { stream.read(reinterpret_cast<char*>(&value), sizeof(value)); };
    const auto readString = [&stream, &read](std::string& value)
    {
        std::uint64_t size = 0; read(size);
        if (size > 1024 * 1024) { stream.setstate(std::ios::failbit); return; }
        value.resize(static_cast<std::size_t>(size));
        if (size > 0) stream.read(&value[0], static_cast<std::streamsize>(size));
    };
    const auto readPodVector = [&stream, &read](auto& values)
    {
        std::uint64_t size = 0; read(size);
        if (size > 1000000) { stream.setstate(std::ios::failbit); return; }
        values.resize(static_cast<std::size_t>(size));
        if (size > 0) stream.read(reinterpret_cast<char*>(values.data()),
            static_cast<std::streamsize>(sizeof(values.front()) * size));
    };
    std::uint32_t magic = 0; read(magic);
    std::uint64_t savedSeed = 0; read(savedSeed);
    if (magic != 0x37574E4E || savedSeed != m_weekSeed || m_weekResetPending) return false;
    std::uint64_t areaCount = 0; read(areaCount);
    if (areaCount == 0 || areaCount > 1000) return false;
    m_weeklyAreas.clear(); m_weeklyAreas.resize(static_cast<std::size_t>(areaCount));
    for (std::size_t areaIndex = 0; areaIndex < m_weeklyAreas.size(); ++areaIndex)
    {
        AreaState& area = m_weeklyAreas[areaIndex];
        read(area.depth); read(area.sublayer); read(area.areaNumber); read(area.generated); read(area.canReturn);
        readPodVector(area.plannedGates);
        read(area.sensingTimer); read(area.respawnClock);
        read(area.discoveredEnemyCount); read(area.discoveredMiningCount); read(area.discoveredCliffCount);
        read(area.totalCliffCount); read(area.firstAreaExpAwarded); read(area.firstAreaRewardAwarded);
        readPodVector(area.discoveredCells); readPodVector(area.discoveredCliffs); read(area.cellExpThresholds);
        if (!area.generated) continue;
        wchar_t mapPath[160] = {};
        std::swprintf(mapPath, 160, L"Assets/Save/WeeklyWorld/area_%03zu.json", areaIndex);
        std::string mapError;
        if (!NarakuMap::LoadMap(mapPath, area.map, &mapError)) { m_weeklyAreas.clear(); return false; }
        std::uint64_t relicCount = 0; read(relicCount);
        if (relicCount > 100000) return false;
        area.groundRelics.resize(static_cast<std::size_t>(relicCount));
        for (GroundRelic& relic : area.groundRelics)
        {
            readString(relic.item.name); read(relic.item.type); read(relic.item.weight); read(relic.item.value);
            read(relic.item.maxUses); read(relic.item.remainingUses); read(relic.item.acquisitionOrder);
            read(relic.item.broken); read(relic.item.stabilized); read(relic.item.autoTrigger);
            read(relic.pos); read(relic.depth); read(relic.active); read(relic.sourceMiningIndex);
        }
        readPodVector(area.groundFoods);
        std::uint64_t miningCount = 0; read(miningCount);
        if (miningCount > 100000) return false;
        area.miningPoints.resize(static_cast<std::size_t>(miningCount));
        for (MiningPoint& point : area.miningPoints)
        {
            read(point.pos); read(point.depth); read(point.visualType); read(point.discovered); read(point.mined);
            read(point.respawnTimer); read(point.extractionCount); read(point.outputPending); read(point.sensed);
            readString(point.relicName);
        }
        std::uint64_t fishingCount = 0; read(fishingCount);
        if (fishingCount > 100000) return false;
        area.fishingPoints.resize(static_cast<std::size_t>(fishingCount));
        for (FishingPoint& point : area.fishingPoints)
        {
            read(point.pos); read(point.depth); read(point.lake); read(point.discovered);
            read(point.remainingUses); read(point.rechargeGameSeconds); readString(point.id);
        }
        readPodVector(area.enemies); readPodVector(area.layerGates); readPodVector(area.pins);
    }
    if (!stream.good()) { m_weeklyAreas.clear(); return false; }
    m_diveWorldSeed = savedSeed;
    return RestoreWeeklyAreaRuntimes();
#endif
}

bool SceneNarakuProto::RestoreWeeklyAreaRuntimes()
{
#if defined(NARAKU_EDITOR_BUILD)
    return false;
#else
    m_areas = m_weeklyAreas;
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        AreaState& area = m_areas[static_cast<std::size_t>(areaIndex)];
        if (!area.generated) continue;
        const std::vector<GroundRelic> groundRelics = area.groundRelics;
        const std::vector<GroundFood> groundFoods = area.groundFoods;
        const std::vector<MiningPoint> miningPoints = area.miningPoints;
        const std::vector<FishingPoint> fishingPoints = area.fishingPoints;
        const std::vector<EnemyState> enemies = area.enemies;
        const std::vector<LayerGateState> layerGates = area.layerGates;
        const std::vector<Vec2> pins = area.pins;
        const int discoveredEnemyCount = area.discoveredEnemyCount;
        const int discoveredMiningCount = area.discoveredMiningCount;
        const int discoveredCliffCount = area.discoveredCliffCount;
        const int totalCliffCount = area.totalCliffCount;
        const bool firstAreaExpAwarded = area.firstAreaExpAwarded;
        const bool firstAreaRewardAwarded = area.firstAreaRewardAwarded;
        const std::vector<std::uint8_t> discoveredCells = area.discoveredCells;
        const std::vector<std::uint8_t> discoveredCliffs = area.discoveredCliffs;
        const std::array<bool, 4> cellExpThresholds = area.cellExpThresholds;
        m_currentAreaIndex = areaIndex;
        m_runtimeMap = area.map;
        BuildCurrentAreaRuntime(false);
        if (!AssignPlannedGates(areaIndex)) return false;
        m_groundRelics = groundRelics; m_groundFoods = groundFoods; m_miningPoints = miningPoints;
        m_fishingPoints = fishingPoints;
        m_enemies = enemies; m_layerGates = layerGates; m_pins = pins;
        area.discoveredEnemyCount = discoveredEnemyCount;
        area.discoveredMiningCount = discoveredMiningCount;
        area.discoveredCliffCount = discoveredCliffCount;
        area.totalCliffCount = totalCliffCount;
        area.firstAreaExpAwarded = firstAreaExpAwarded;
        area.firstAreaRewardAwarded = firstAreaRewardAwarded;
        area.discoveredCells = discoveredCells;
        area.discoveredCliffs = discoveredCliffs;
        area.cellExpThresholds = cellExpThresholds;
        SaveCurrentAreaState();
    }
    m_weeklyAreas = m_areas;
    m_currentAreaIndex = -1;
    return true;
#endif
}

void SceneNarakuProto::ActivateArea(int areaIndex, bool placeAtEntry)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    const AreaState& area = m_areas[areaIndex];
    m_currentAreaIndex = areaIndex;
    m_runtimeMap = area.map;
    m_groundRelics = area.groundRelics;
    m_groundFoods = area.groundFoods;
    m_miningPoints = area.miningPoints;
    m_fishingPoints = area.fishingPoints;
    m_enemies = area.enemies;
    m_floorRegions = area.floorRegions;
    m_ropePoints = area.ropePoints;
    m_layerGates = area.layerGates;
    m_pins = area.pins;
    m_startPoint = area.startPoint;
    m_startDepth = area.startDepth;
    m_returnPoint = area.returnPoint;
    m_returnDepth = area.returnDepth;
    m_worldHalfSize = area.worldHalfSize;
    m_cameraCollisionDistance = -1.0f;
    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_activeRope = -1;
    m_player.onRope = false;

    if (placeAtEntry)
    {
        for (const LayerGateState& gate : m_layerGates)
        {
            if (!gate.isEntry)
            {
                continue;
            }
            m_player.pos = gate.ropePos;
            m_player.depth = gate.depth;
            break;
        }
    }
    m_player.previousDepth = m_player.depth;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.grounded = true;
    m_player.verticalSpeed = 0.0f;
    for (EnemyState& enemy : m_enemies)
        if (!enemy.alive && enemy.respawnTimer <= 0.0f) RespawnEnemy(enemy);
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
#if defined(NARAKU_EDITOR_BUILD)
    if (m_editorOverviewMode) FocusEditorOverviewOnCurrentArea();
#endif
}

void SceneNarakuProto::BuildCurrentAreaRuntime(bool placeAtStart)
{
    m_groundRelics.clear();
    m_groundFoods.clear();
    m_miningPoints.clear();
    m_fishingPoints.clear();
    m_enemies.clear();
    m_attackHitEffects.clear();
    m_jumpEffects.clear();
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_layerGates.clear();
    m_pins.clear();
    m_activeRope = -1;
    m_miningIndex = -1;
    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_worldHalfSize = 1.0f;

    auto getLayerDepthById = [this](int layerId) -> float
    {
        const int index = NarakuMap::FindLayerIndexById(m_runtimeMap, layerId);
        return index >= 0 ? m_runtimeMap.terrainLayers[index].layerDepth : 0.0f;
    };
    auto getFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    m_startPoint = { m_runtimeMap.playerStartPoint.xz.x, m_runtimeMap.playerStartPoint.xz.z };
    m_startDepth = getLayerDepthById(m_runtimeMap.playerStartPoint.layerId);
    m_returnPoint = m_startPoint;
    m_returnDepth = m_startDepth;

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }
        const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
        const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.x) + width * 0.5f);
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.z) + height * 0.5f);
        m_floorRegions.push_back({ { layer.center.x, layer.center.z }, { width * 0.5f, height * 0.5f },
            layer.layerDepth, getFloorColor(layer.groundTextureId), layer.id });
    }

    for (const NarakuMap::RopePoint& rope : m_runtimeMap.ropes)
    {
        const int top = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.topLayerId);
        const int bottom = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.bottomLayerId);
        if (top >= 0 && bottom >= 0)
        {
            m_ropePoints.push_back({ { rope.topXZ.x, rope.topXZ.z }, { rope.bottomXZ.x, rope.bottomXZ.z },
                m_runtimeMap.terrainLayers[top].layerDepth, m_runtimeMap.terrainLayers[bottom].layerDepth });
        }
    }

    for (const NarakuMap::LayerGatePoint& gate : m_runtimeMap.layerGates)
    {
        const int layer = NarakuMap::FindLayerIndexById(m_runtimeMap, gate.layerId);
        if (layer < 0)
        {
            continue;
        }
        LayerGateState runtimeGate;
        runtimeGate.isEntry = gate.isEntry;
        runtimeGate.ropePos = { gate.ropeXZ.x, gate.ropeXZ.z };
        runtimeGate.loadPos = { gate.loadXZ.x, gate.loadXZ.z };
        runtimeGate.depth = m_runtimeMap.terrainLayers[layer].layerDepth;
        m_layerGates.push_back(runtimeGate);
    }

    int relicIndex = 0;
    for (const NarakuMap::MiningPoint& point : m_runtimeMap.miningPoints)
    {
        if (!point.enabled)
        {
            continue;
        }
        MiningPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.visualType = point.visualType;
        runtimePoint.discovered = point.discovered;
        runtimePoint.mined = false;
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.relicName = point.relicName.empty() ? kRelicNames[relicIndex % 8] : point.relicName;
        ++relicIndex;
        m_miningPoints.push_back(runtimePoint);
    }

    for (const NarakuMap::FishingPoint& point : m_runtimeMap.fishingPoints)
    {
        FishingPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.lake = point.lake;
        runtimePoint.id = point.id;
        m_fishingPoints.push_back(runtimePoint);
    }

    SpawnEnemiesForCurrentArea();

    if (placeAtStart)
    {
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        for (const LayerGateState& gate : m_layerGates)
        {
            if (gate.isEntry)
            {
                m_player.pos = gate.ropePos;
                m_player.depth = gate.depth;
                break;
            }
        }
        m_player.previousDepth = m_player.depth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
    }
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
}

bool SceneNarakuProto::BuildSurfaceRuntime(bool spawnAtAbyssEntrance)
{
    std::array<NarakuPiece::PieceData, kSurfaceFacilityOrder.size()> surfacePieces;
    std::array<bool, kSurfaceFacilityOrder.size()> foundSurfacePieces = {};
    const std::wstring completedDirectory = NarakuPiece::GetSurfaceCompletedDirectoryRelativePath();
    const std::wstring searchPattern = completedDirectory + L"\\*.json";
    WIN32_FIND_DATAW findData = {};
    HANDLE findHandle = FindFirstFileW(searchPattern.c_str(), &findData);
    if (findHandle == INVALID_HANDLE_VALUE)
    {
        m_generationFailureSummary = u8"地上マップを読み込めませんでした。";
        m_generationFailureDetail = "completed surface piece directory is missing or empty";
        m_openGenerationFailurePopup = true;
        return false;
    }
    do
    {
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
        NarakuPiece::PieceData piece;
        std::string error;
        const std::wstring path = completedDirectory + L"\\" + findData.cFileName;
        if (!NarakuPiece::LoadPieceData(path, piece, &error) || !piece.isSurface)
        {
            FindClose(findHandle);
            m_generationFailureSummary = u8"地上マップを読み込めませんでした。";
            m_generationFailureDetail = WideToUtf8(findData.cFileName) + ": " +
                (error.empty() ? "piece is not tagged as surface" : error);
            m_openGenerationFailurePopup = true;
            return false;
        }
        const auto typeIt = std::find(kSurfaceFacilityOrder.begin(), kSurfaceFacilityOrder.end(), piece.surfaceFacility.type);
        if (typeIt == kSurfaceFacilityOrder.end())
        {
            FindClose(findHandle);
            m_generationFailureSummary = u8"地上マップを読み込めませんでした。";
            m_generationFailureDetail = WideToUtf8(findData.cFileName) + ": required surface facility type is not set";
            m_openGenerationFailurePopup = true;
            return false;
        }
        const std::size_t index = static_cast<std::size_t>(std::distance(kSurfaceFacilityOrder.begin(), typeIt));
        if (foundSurfacePieces[index])
        {
            FindClose(findHandle);
            m_generationFailureSummary = u8"地上マップを読み込めませんでした。";
            m_generationFailureDetail = std::string("duplicate surface facility type: ") + NarakuPiece::ToString(piece.surfaceFacility.type);
            m_openGenerationFailurePopup = true;
            return false;
        }
        surfacePieces[index] = std::move(piece);
        foundSurfacePieces[index] = true;
    } while (FindNextFileW(findHandle, &findData));
    FindClose(findHandle);

    const auto missingType = std::find(foundSurfacePieces.begin(), foundSurfacePieces.end(), false);
    if (missingType != foundSurfacePieces.end())
    {
        const std::size_t index = static_cast<std::size_t>(std::distance(foundSurfacePieces.begin(), missingType));
        m_generationFailureSummary = u8"地上マップを読み込めませんでした。";
        m_generationFailureDetail = std::string("missing surface facility type: ") + NarakuPiece::ToString(kSurfaceFacilityOrder[index]);
        m_openGenerationFailurePopup = true;
        return false;
    }

    NarakuMap::MapData surfaceMap;
    NarakuMap::TerrainLayer layer;
    layer.id = 1;
    layer.center = { 0.0f, 0.0f };
    layer.layerDepth = 0.0f;
    layer.gridWidth = 33;
    layer.gridHeight = 17;
    layer.cellSize = 2.0f;
    layer.groundTextureId = 0;
    layer.heights.assign(static_cast<std::size_t>(layer.gridWidth * layer.gridHeight), 0.0f);
    layer.vertexEnabled.assign(layer.heights.size(), 0);
    layer.cellAttributeFlags.assign(static_cast<std::size_t>((layer.gridWidth - 1) * (layer.gridHeight - 1)),
        NarakuMap::CellAttributeRemoved);
    layer.cellGroundTextureIds.assign(static_cast<std::size_t>((layer.gridWidth - 1) * (layer.gridHeight - 1)),
        layer.groundTextureId);

    const std::array<NarakuPiece::GridPoint, 5> offsets = {
        NarakuPiece::GridPoint{ 0, 0 }, NarakuPiece::GridPoint{ 8, 0 },
        NarakuPiece::GridPoint{ 16, 0 }, NarakuPiece::GridPoint{ 24, 0 },
        NarakuPiece::GridPoint{ 12, 8 } };
    const float originX = -32.0f;
    const float originZ = -16.0f;
    m_surfaceFacilities.clear();

    for (std::size_t pieceIndex = 0; pieceIndex < surfacePieces.size(); ++pieceIndex)
    {
        const NarakuPiece::PieceData& piece = surfacePieces[pieceIndex];
        const int offsetX = offsets[pieceIndex].x;
        const int offsetZ = offsets[pieceIndex].z;
        for (int z = 0; z <= 8; ++z)
        {
            for (int x = 0; x <= 8; ++x)
            {
                const int globalX = offsetX + x;
                const int globalZ = offsetZ + z;
                const int sourceX = std::min(7, x);
                const int sourceZ = std::min(7, z);
                const std::size_t globalVertex = static_cast<std::size_t>(globalZ * layer.gridWidth + globalX);
                const std::size_t sourceVertex = static_cast<std::size_t>(sourceZ * piece.gridWidth + sourceX);
                layer.vertexEnabled[globalVertex] = 1;
                if (sourceVertex < piece.heights.size()) layer.heights[globalVertex] = piece.heights[sourceVertex];
            }
        }
        for (int z = 0; z < 8; ++z)
        {
            for (int x = 0; x < 8; ++x)
            {
                const int globalX = offsetX + x;
                const int globalZ = offsetZ + z;
                const int sourceX = std::min(6, x);
                const int sourceZ = std::min(6, z);
                const std::size_t globalCell = static_cast<std::size_t>(globalZ * (layer.gridWidth - 1) + globalX);
                const std::size_t sourceCell = static_cast<std::size_t>(sourceZ * std::max(1, piece.gridWidth - 1) + sourceX);
                std::uint32_t flags = NarakuMap::CellAttributeNone;
                int groundTextureId = layer.groundTextureId;
                if (sourceCell < piece.cells.size())
                {
                    const NarakuPiece::CellData& cell = piece.cells[sourceCell];
                    groundTextureId = cell.groundTextureId;
                    if (cell.deleted) flags |= NarakuMap::CellAttributeRemoved;
                    if (!cell.walkable) flags |= NarakuMap::CellAttributeBlocked;
                    if (cell.waterDepth == NarakuPiece::WaterDepth::Puddle) flags |= NarakuMap::CellAttributeWaterPuddle;
                    if (cell.waterDepth == NarakuPiece::WaterDepth::Pond) flags |= NarakuMap::CellAttributeWaterPond;
                    if (cell.waterDepth == NarakuPiece::WaterDepth::Lake) flags |= NarakuMap::CellAttributeWaterLake | NarakuMap::CellAttributeBlocked;
                }
                layer.cellAttributeFlags[globalCell] = flags;
                layer.cellGroundTextureIds[globalCell] = groundTextureId;
            }
        }

        for (const NarakuPiece::EnvironmentObjectData& pieceObject : piece.environmentObjects)
        {
            NarakuMap::EnvironmentObject object;
            object.modelId = pieceObject.modelId;
            object.xz = {
                originX + (static_cast<float>(offsetX + pieceObject.cell.x) + 0.5f) * layer.cellSize,
                originZ + (static_cast<float>(offsetZ + pieceObject.cell.z) + 0.5f) * layer.cellSize };
            object.layerId = layer.id;
            object.scaleX = pieceObject.scaleX;
            object.scaleY = pieceObject.scaleY;
            object.scaleZ = pieceObject.scaleZ;
            object.rotationQuarterTurns = pieceObject.rotationQuarterTurns;
            object.footprintAnchored = true;
            surfaceMap.environmentObjects.push_back(object);
        }

        if (piece.surfaceFacility.type != NarakuPiece::SurfaceFacilityType::None)
        {
            SurfaceFacilityState facility;
            facility.type = piece.surfaceFacility.type;
            facility.center = {
                originX + static_cast<float>(offsetX + piece.surfaceFacility.cell.x + 1) * layer.cellSize,
                originZ + static_cast<float>(offsetZ + piece.surfaceFacility.cell.z + 1) * layer.cellSize };
            facility.interactionPoint = facility.center;
            const float frontOffset = 3.0f;
            if (piece.surfaceFacility.facing == NarakuPiece::Direction::North) facility.interactionPoint.y -= frontOffset;
            else if (piece.surfaceFacility.facing == NarakuPiece::Direction::South) facility.interactionPoint.y += frontOffset;
            else if (piece.surfaceFacility.facing == NarakuPiece::Direction::East) facility.interactionPoint.x += frontOffset;
            else facility.interactionPoint.x -= frontOffset;
            facility.modelPath = piece.surfaceFacility.modelPath;
            m_surfaceFacilities.push_back(facility);

            for (int z = 0; z < 2; ++z)
                for (int x = 0; x < 2; ++x)
                {
                    const int gx = offsetX + piece.surfaceFacility.cell.x + x;
                    const int gz = offsetZ + piece.surfaceFacility.cell.z + z;
                    if (gx >= 0 && gx < layer.gridWidth - 1 && gz >= 0 && gz < layer.gridHeight - 1)
                        layer.cellAttributeFlags[static_cast<std::size_t>(gz * (layer.gridWidth - 1) + gx)] |= NarakuMap::CellAttributeBlocked;
                }

            if (!piece.surfaceFacility.modelPath.empty())
            {
                NarakuMap::EnvironmentObject object;
                object.modelId = GetSurfaceFacilityModelId(piece.surfaceFacility.type);
                object.xz = { facility.center.x + piece.surfaceFacility.offsetX, facility.center.y + piece.surfaceFacility.offsetZ };
                object.layerId = layer.id;
                object.scaleX = piece.surfaceFacility.scaleX;
                object.scaleY = piece.surfaceFacility.scaleY;
                object.scaleZ = piece.surfaceFacility.scaleZ;
                object.offsetY = piece.surfaceFacility.offsetY;
                object.rotationQuarterTurns = piece.surfaceFacility.rotationQuarterTurns;
                surfaceMap.environmentObjects.push_back(object);
            }
        }
    }

    surfaceMap.terrainLayers.push_back(std::move(layer));
    const NarakuPiece::SurfaceFacilityType spawnType = spawnAtAbyssEntrance
        ? NarakuPiece::SurfaceFacilityType::AbyssEntrance : NarakuPiece::SurfaceFacilityType::Home;
    auto spawn = std::find_if(m_surfaceFacilities.begin(), m_surfaceFacilities.end(),
        [spawnType](const SurfaceFacilityState& value) { return value.type == spawnType; });
    if (spawn == m_surfaceFacilities.end()) return false;
    surfaceMap.playerStartPoint = { { spawn->interactionPoint.x, spawn->interactionPoint.y }, 1 };
    surfaceMap.returnPoint = surfaceMap.playerStartPoint;
    m_runtimeMap = std::move(surfaceMap);
    m_currentAreaIndex = -1;
    BuildCurrentAreaRuntime(true);
    m_enemies.clear();
    m_miningPoints.clear();
    m_fishingPoints.clear();
    m_groundRelics.clear();
    m_groundFoods.clear();
    m_pins = m_surfacePins;
    return true;
}

void SceneNarakuProto::EnterSurface(bool spawnAtAbyssEntrance)
{
    if (!BuildSurfaceRuntime(spawnAtAbyssEntrance)) return;
    m_mode = Mode::Surface;
}

void SceneNarakuProto::TryInteractSurface()
{
    const auto facility = std::min_element(m_surfaceFacilities.begin(), m_surfaceFacilities.end(),
        [this](const SurfaceFacilityState& lhs, const SurfaceFacilityState& rhs)
        { return Distance(lhs.interactionPoint, m_player.pos) < Distance(rhs.interactionPoint, m_player.pos); });
    if (facility == m_surfaceFacilities.end() || Distance(facility->interactionPoint, m_player.pos) > 2.5f) return;
    switch (facility->type)
    {
    case NarakuPiece::SurfaceFacilityType::Home: m_mode = Mode::Home; break;
    case NarakuPiece::SurfaceFacilityType::Shop: m_mode = Mode::GeneralShop; break;
    case NarakuPiece::SurfaceFacilityType::Armory: m_mode = Mode::Armory; break;
    case NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk: m_mode = Mode::Restaurant; break;
    case NarakuPiece::SurfaceFacilityType::AbyssEntrance: m_mode = Mode::AbyssEntrance; break;
    default: break;
    }
}

void SceneNarakuProto::UpdateSurface(float dt)
{
    const Vec2 previousPosition = m_player.pos;
    UpdateCameraControls();
    ClampDebugPlayerParams();
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.feetWorldY;
    UpdateMovement(dt);
    m_surfaceWasMoving = Distance(previousPosition, m_player.pos) > 0.0001f;
    if (m_player.stamina < GetMaxStamina())
        m_player.stamina = std::min(GetMaxStamina(), m_player.stamina +
            m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier() * dt);
    if (IsActionInteractTrigger()) TryInteractSurface();
}

void SceneNarakuProto::TryUseLayerGate(int gateIndex)
{
    if (gateIndex < 0 || gateIndex >= static_cast<int>(m_layerGates.size()))
    {
        return;
    }

    LayerGateState& gate = m_layerGates[gateIndex];
    if (gate.disabled)
    {
        ShowCenterNotification(u8"この層間口は使用できない！");
        return;
    }
    if (gate.destinationAreaIndex >= 0)
    {
        if (gate.destinationAreaIndex < static_cast<int>(m_areas.size()) && m_areas[gate.destinationAreaIndex].generated)
        {
#if !defined(NARAKU_EDITOR_BUILD)
            const int destinationDepth = m_areas[static_cast<std::size_t>(gate.destinationAreaIndex)].depth;
            if (destinationDepth > GetCurrentDepth() &&
                destinationDepth > GetRankMaximumDepth(m_adventurerRank) &&
                !m_uninsuredDescentAcceptedThisDive)
            {
                m_pendingUninsuredGateIndex = gateIndex;
                m_mode = Mode::UninsuredDescentConfirm;
                return;
            }
#endif
            BeginLayerTransition(gateIndex, gate.destinationAreaIndex);
            return;
        }
        SaveCurrentAreaState();
        m_loadingSourceGateIndex = gateIndex;
        m_loadingStep = 0;
        m_loadingProgress = 0.05f;
        const AreaState& destinationArea = m_areas[static_cast<std::size_t>(gate.destinationAreaIndex)];
        std::ostringstream loadingStatus;
        loadingStatus << u8"準備中: 第" << destinationArea.depth << u8"層 "
            << GetSublayerName(destinationArea.sublayer) << u8" エリア" << destinationArea.areaNumber;
        m_loadingStatus = loadingStatus.str();
        m_mode = Mode::Loading;
        return;
    }
    ShowCenterNotification(u8"接続先がありません。");
}

void SceneNarakuProto::BeginLayerTransition(int sourceGateIndex, int destinationAreaIndex)
{
    if (sourceGateIndex < 0 || sourceGateIndex >= static_cast<int>(m_layerGates.size()) ||
        destinationAreaIndex < 0 || destinationAreaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    int destinationGateIndex = -1;
    const std::vector<LayerGateState>& destinationGates = m_areas[destinationAreaIndex].layerGates;
    for (int i = 0; i < static_cast<int>(destinationGates.size()); ++i)
    {
        if (destinationGates[i].connectionId == m_layerGates[sourceGateIndex].connectionId)
        {
            destinationGateIndex = i;
            break;
        }
    }
    if (destinationGateIndex < 0)
    {
        ShowCenterNotification(u8"層間口の接続情報が不正です。");
        return;
    }
    if (m_cookingTarget != CookingTarget::None)
    {
        CancelCooking(u8"層間移動を開始したため調理を中断しました。料理セットの使用回数は戻りません。");
    }

    m_transitionSourceGateIndex = sourceGateIndex;
    m_transitionDestinationAreaIndex = destinationAreaIndex;
    m_transitionDestinationGateIndex = destinationGateIndex;
    m_layerTransitionProgress = 0.0f;
    m_layerTransitionVisualOffset = 0.0f;
    m_layerTransitionAscending = m_layerGates[sourceGateIndex].isEntry;
    m_player.pos = m_layerGates[sourceGateIndex].ropePos;
    m_player.depth = m_layerGates[sourceGateIndex].depth;
    m_player.onRope = false;
    const bool routeAlreadyDiscovered = m_layerGates[sourceGateIndex].routeDiscovered ||
        m_areas[destinationAreaIndex].layerGates[destinationGateIndex].routeDiscovered;
    m_layerGates[sourceGateIndex].routeDiscovered = true;
    m_areas[destinationAreaIndex].layerGates[destinationGateIndex].routeDiscovered = true;
    if (!routeAlreadyDiscovered)
    {
        const int destinationDepth = m_areas[destinationAreaIndex].depth;
        AwardExp(static_cast<int>(std::round(100.0f * GetDepthExpMultiplier(destinationDepth))));
    }
    SaveCurrentAreaState();
    m_mode = Mode::LayerTransition;
}

void SceneNarakuProto::ReportGenerationFailure(const std::string& summary, const std::string& detail)
{
    m_generationFailureSummary = summary;
    m_generationFailureDetail = detail.empty() ? u8"生成器から詳細理由が返されませんでした。" : detail;
    m_openGenerationFailurePopup = true;

    _mkdir("Assets");
    _mkdir("Assets/Logs");
    std::ofstream log("Assets/Logs/naraku_generation_failure.log", std::ios::app);
    if (log)
    {
        log << "=== Generation failure ===\n" << m_generationFailureSummary << '\n'
            << m_generationFailureDetail << "\n\n";
    }
}

#if defined(NARAKU_EDITOR_BUILD)
void SceneNarakuProto::UpdateEditorPreviewGeneration()
{
    if (m_editorPreviewGenerationFailed) return;

    if (m_loadingStep == 0)
    {
        m_loadingStep = 1;
        m_loadingProgress = 0.01f;
        m_loadingStatus = u8"15段階の接続構成を生成しています...";
        return;
    }

    if (m_loadingStep == 1)
    {
        m_generationFailureSummary.clear();
        m_generationFailureDetail.clear();
        m_openGenerationFailurePopup = false;
        if (!ResetRun(false))
        {
            m_editorPreviewGenerationFailed = true;
            m_mode = Mode::Loading;
            m_loadingStatus = u8"生成に失敗しました。";
            return;
        }

        m_editorPreviewStartArea = m_currentAreaIndex;
        m_editorPreviewGenerationCursor = 0;
        m_editorPreviewGenerationCompleted = 1;
        m_editorPreviewGenerationTotal = static_cast<int>(m_areas.size());
        m_loadingProgress = m_editorPreviewGenerationTotal > 0
            ? static_cast<float>(m_editorPreviewGenerationCompleted) /
                static_cast<float>(m_editorPreviewGenerationTotal)
            : 0.0f;
        m_loadingStep = 2;
        m_mode = Mode::Loading;
        return;
    }

    while (m_editorPreviewGenerationCursor < static_cast<int>(m_areas.size()) &&
        m_areas[static_cast<std::size_t>(m_editorPreviewGenerationCursor)].generated)
    {
        ++m_editorPreviewGenerationCursor;
    }

    if (m_editorPreviewGenerationCursor >= static_cast<int>(m_areas.size()))
    {
        ActivateArea(m_editorPreviewStartArea, false);
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_loadingProgress = 1.0f;
        m_loadingStatus = u8"生成完了";
        m_editorPreviewGenerationActive = false;
        m_mode = Mode::Explore;
        return;
    }

    const int areaIndex = m_editorPreviewGenerationCursor;
    const AreaState& area = m_areas[static_cast<std::size_t>(areaIndex)];
    std::ostringstream status;
    status << u8"生成中: 第" << area.depth << u8"層 " << GetSublayerName(area.sublayer)
        << u8" エリア" << area.areaNumber << "  ("
        << m_editorPreviewGenerationCompleted << '/' << m_editorPreviewGenerationTotal << ')';
    m_loadingStatus = status.str();

    std::string error;
    if (!GeneratePlannedArea(areaIndex, error))
    {
        std::ostringstream summary;
        summary << u8"15段階生成プレビューに失敗しました: 第" << area.depth << u8"層 "
            << GetSublayerName(area.sublayer) << u8" エリア" << area.areaNumber
            << " / seed=" << m_editorPreviewGenerationSeed;
        ReportGenerationFailure(summary.str(), error);
        m_editorPreviewGenerationFailed = true;
        m_loadingStatus = u8"生成に失敗しました。";
        return;
    }

    ++m_editorPreviewGenerationCompleted;
    ++m_editorPreviewGenerationCursor;
    m_loadingProgress = m_editorPreviewGenerationTotal > 0
        ? static_cast<float>(m_editorPreviewGenerationCompleted) /
            static_cast<float>(m_editorPreviewGenerationTotal)
        : 1.0f;
}
#endif

void SceneNarakuProto::UpdateLoading()
{
#if defined(NARAKU_EDITOR_BUILD)
    if (m_editorPreviewGenerationActive)
    {
        UpdateEditorPreviewGeneration();
        return;
    }
#endif
    if (m_loadingStep == 0)
    {
        m_loadingStep = 1;
        m_loadingProgress = 0.15f;
        return;
    }
    if (m_loadingStep != 1 || m_currentAreaIndex < 0 ||
        m_loadingSourceGateIndex < 0 || m_loadingSourceGateIndex >= static_cast<int>(m_layerGates.size()))
    {
        m_mode = Mode::Explore;
        return;
    }

    const int parentAreaIndex = m_currentAreaIndex;
    const int sourceGateIndex = m_loadingSourceGateIndex;
    const int destinationAreaIndex = m_layerGates[sourceGateIndex].destinationAreaIndex;
    if (destinationAreaIndex < 0 || destinationAreaIndex >= static_cast<int>(m_areas.size()))
    {
        m_mode = Mode::Explore;
        ReportGenerationFailure(u8"接続先エリアを生成できませんでした。", u8"層間出入口に有効な接続先エリアが設定されていません。" );
        ShowCenterNotification(u8"接続先がありません。詳細を表示します。");
        return;
    }
    std::string error;
    m_loadingProgress = 0.45f;
    const AreaState& destinationArea = m_areas[static_cast<std::size_t>(destinationAreaIndex)];
    std::ostringstream loadingStatus;
    loadingStatus << u8"生成中: 第" << destinationArea.depth << u8"層 "
        << GetSublayerName(destinationArea.sublayer) << u8" エリア" << destinationArea.areaNumber;
    m_loadingStatus = loadingStatus.str();
    if (!GeneratePlannedArea(destinationAreaIndex, error))
    {
        ActivateArea(parentAreaIndex, false);
        ++m_layerGates[sourceGateIndex].generationFailures;
        SaveCurrentAreaState();
        m_mode = Mode::Explore;
        m_loadingProgress = 0.0f;
        m_loadingStep = 0;
        std::ostringstream summary;
        summary << u8"接続先の生成に失敗しました: 第" << destinationArea.depth << u8"層 "
            << GetSublayerName(destinationArea.sublayer) << u8" エリア" << destinationArea.areaNumber;
        ReportGenerationFailure(summary.str(), error);
        ShowCenterNotification(u8"接続先の生成に失敗しました。詳細を表示します。");
        return;
    }
    ActivateArea(parentAreaIndex, false);
    m_layerGates[sourceGateIndex].previewReady = true;
    SaveCurrentAreaState();
    m_loadingProgress = 1.0f;
    m_loadingStep = 0;
    m_mode = Mode::Explore;
    ShowCenterNotification(u8"ルート情報を取得しました。もう一度Fで進入します。");
}

void SceneNarakuProto::UpdateLayerTransition(float dt)
{
    m_layerTransitionProgress = std::min(1.0f, m_layerTransitionProgress + dt / kLayerTransitionDuration);
    const float direction = m_layerTransitionAscending ? 1.0f : -1.0f;
    m_layerTransitionVisualOffset = direction * kLayerTransitionHeight * m_layerTransitionProgress;

    if (m_layerTransitionAscending)
    {
        m_player.upperLoad += kLayerTransitionHeight * dt / kLayerTransitionDuration;
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            TriggerUpperLoad();
            m_player.upperLoad = 0.0f;
            if (m_mode != Mode::LayerTransition) return;
        }
    }

    const float fadeStartProgress = 1.0f - kScreenFadeDuration / kLayerTransitionDuration;
    if (m_screenFadePhase == ScreenFadePhase::None && m_layerTransitionProgress >= fadeStartProgress)
    {
        BeginScreenFade(ScreenFadeAction::LayerTransition);
    }
    if (m_screenFadePhase != ScreenFadePhase::None) UpdateScreenFade(dt);
}

void SceneNarakuProto::BeginScreenFade(ScreenFadeAction action)
{
    if (action == ScreenFadeAction::None || m_screenFadePhase != ScreenFadePhase::None) return;
    m_screenFadeAction = action;
    m_screenFadePhase = ScreenFadePhase::FadeOut;
    m_screenFadeAlpha = 0.0f;
}

void SceneNarakuProto::UpdateScreenFade(float dt)
{
    if (m_screenFadePhase == ScreenFadePhase::None) return;
    const float delta = dt / kScreenFadeDuration;
    if (m_screenFadePhase == ScreenFadePhase::FadeOut)
    {
        m_screenFadeAlpha = std::min(1.0f, m_screenFadeAlpha + delta);
        if (m_screenFadeAlpha < 1.0f) return;

        const ScreenFadeAction action = m_screenFadeAction;
        if (action == ScreenFadeAction::StartDive) CompleteStartDive();
        else if (action == ScreenFadeAction::LayerTransition) CompleteLayerTransition();
        m_screenFadeAction = ScreenFadeAction::None;
        m_screenFadePhase = ScreenFadePhase::FadeIn;
        return;
    }

    m_screenFadeAlpha = std::max(0.0f, m_screenFadeAlpha - delta);
    if (m_screenFadeAlpha <= 0.0f)
    {
        m_screenFadeAlpha = 0.0f;
        m_screenFadePhase = ScreenFadePhase::None;
    }
}

void SceneNarakuProto::CompleteLayerTransition()
{

    SaveCurrentAreaState();
    const int destinationArea = m_transitionDestinationAreaIndex;
    const int destinationGate = m_transitionDestinationGateIndex;
    ActivateArea(destinationArea, false);
    AreaState& arrivedArea = m_areas[destinationArea];
    if (!arrivedArea.firstAreaExpAwarded)
    {
        arrivedArea.firstAreaExpAwarded = true;
        arrivedArea.firstAreaRewardAwarded = true;
        ++m_result.firstAreaCount;
        AwardExp(static_cast<int>(std::round(100.0f * GetDepthExpMultiplier(arrivedArea.depth))));
    }
    if (destinationGate >= 0 && destinationGate < static_cast<int>(m_layerGates.size()))
    {
        m_player.pos = m_layerGates[destinationGate].ropePos;
        m_player.depth = m_layerGates[destinationGate].depth;
        m_player.previousDepth = m_player.depth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
    }
    m_layerTransitionVisualOffset = 0.0f;
    m_transitionSourceGateIndex = -1;
    m_transitionDestinationAreaIndex = -1;
    m_transitionDestinationGateIndex = -1;
    m_mode = Mode::Explore;
    UpdateImportantQuestArrival();
    SaveProgress();
}
