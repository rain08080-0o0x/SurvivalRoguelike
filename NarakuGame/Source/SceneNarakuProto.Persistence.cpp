/**
 * @file SceneNarakuProto.Persistence.cpp
 * @brief デバッグ設定、進行状態の初期化、保存、および読込を実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

void SceneNarakuProto::ResetDebugPlayerParams()
{
    // 既存実装で使っていた固定値を、そのまま初期値として再設定します。
    m_debugPlayerParams.walkSpeed = 1.5f;
    m_debugPlayerParams.runSpeed = 2.5f;
    m_debugPlayerParams.ropeSpeed = 1.0f;
    m_debugPlayerParams.attackPower = kPlayerBaseAttack;
    m_debugPlayerParams.runCostPerSecond = 1.5f;
    m_debugPlayerParams.ropeCostPerSecond = 3.0f;
    m_debugPlayerParams.attackCost = 10.0f;
    m_debugPlayerParams.miningCost = 7.0f;
    m_debugPlayerParams.stepCost = 5.0f;
    m_debugPlayerParams.jumpCost = 5.0f;
    m_debugPlayerParams.staminaRecoverPerSecond = 2.0f;
    m_debugPlayerParams.upperLayerAlpha = 0.06f;
    m_cameraDistance = kCameraDefaultDistance;
    m_cameraMinPitchDegrees = kCameraDefaultMinPitchDegrees;
    m_cameraMaxPitchDegrees = kCameraDefaultMaxPitchDegrees;
    NormalizeCameraSettings();
}
bool SceneNarakuProto::LoadDebugPlayerParams()
{
    std::ifstream stream(kPlaytestConfigPath, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const std::string json = buffer.str();

    TryReadJsonFloat(json, "walkSpeed", m_debugPlayerParams.walkSpeed);
    TryReadJsonFloat(json, "runSpeed", m_debugPlayerParams.runSpeed);
    TryReadJsonFloat(json, "ropeSpeed", m_debugPlayerParams.ropeSpeed);
    m_debugPlayerParams.attackPower = kPlayerBaseAttack;
    TryReadJsonFloat(json, "runCostPerSecond", m_debugPlayerParams.runCostPerSecond);
    TryReadJsonFloat(json, "ropeCostPerSecond", m_debugPlayerParams.ropeCostPerSecond);
    TryReadJsonFloat(json, "attackCost", m_debugPlayerParams.attackCost);
    TryReadJsonFloat(json, "miningCost", m_debugPlayerParams.miningCost);
    TryReadJsonFloat(json, "stepCost", m_debugPlayerParams.stepCost);
    TryReadJsonFloat(json, "jumpCost", m_debugPlayerParams.jumpCost);
    TryReadJsonFloat(json, "staminaRecoverPerSecond", m_debugPlayerParams.staminaRecoverPerSecond);
    TryReadJsonFloat(json, "upperLayerAlpha", m_debugPlayerParams.upperLayerAlpha);
    TryReadJsonFloat(json, "minimapPosX", m_debugPlayerParams.minimapPosX);
    TryReadJsonFloat(json, "minimapPosY", m_debugPlayerParams.minimapPosY);
    TryReadJsonFloat(json, "minimapSize", m_debugPlayerParams.minimapSize);
    TryReadJsonFloat(json, "showMinimap", m_debugPlayerParams.showMinimap);
    TryReadJsonFloat(json, "cameraDistance", m_cameraDistance);
    TryReadJsonFloat(json, "cameraMinPitchDegrees", m_cameraMinPitchDegrees);
    TryReadJsonFloat(json, "cameraMaxPitchDegrees", m_cameraMaxPitchDegrees);
    ClampDebugPlayerParams();
    NormalizeCameraSettings();
    return true;
}

bool SceneNarakuProto::SaveDebugPlayerParams() const
{
    std::ofstream stream(kPlaytestConfigPath, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }
    stream << "{\n"
        << "  \"walkSpeed\": " << m_debugPlayerParams.walkSpeed << ",\n"
        << "  \"runSpeed\": " << m_debugPlayerParams.runSpeed << ",\n"
        << "  \"ropeSpeed\": " << m_debugPlayerParams.ropeSpeed << ",\n"
        << "  \"attackPower\": " << m_debugPlayerParams.attackPower << ",\n"
        << "  \"runCostPerSecond\": " << m_debugPlayerParams.runCostPerSecond << ",\n"
        << "  \"ropeCostPerSecond\": " << m_debugPlayerParams.ropeCostPerSecond << ",\n"
        << "  \"attackCost\": " << m_debugPlayerParams.attackCost << ",\n"
        << "  \"miningCost\": " << m_debugPlayerParams.miningCost << ",\n"
        << "  \"stepCost\": " << m_debugPlayerParams.stepCost << ",\n"
        << "  \"jumpCost\": " << m_debugPlayerParams.jumpCost << ",\n"
        << "  \"staminaRecoverPerSecond\": " << m_debugPlayerParams.staminaRecoverPerSecond << ",\n"
        << "  \"upperLayerAlpha\": " << m_debugPlayerParams.upperLayerAlpha << ",\n"
        << "  \"minimapPosX\": " << m_debugPlayerParams.minimapPosX << ",\n"
        << "  \"minimapPosY\": " << m_debugPlayerParams.minimapPosY << ",\n"
        << "  \"minimapSize\": " << m_debugPlayerParams.minimapSize << ",\n"
        << "  \"showMinimap\": " << m_debugPlayerParams.showMinimap << ",\n"
        << "  \"cameraDistance\": " << m_cameraDistance << ",\n"
        << "  \"cameraMinPitchDegrees\": " << m_cameraMinPitchDegrees << ",\n"
        << "  \"cameraMaxPitchDegrees\": " << m_cameraMaxPitchDegrees << "\n"
        << "}\n";
    return stream.good();
}

void SceneNarakuProto::InitializeNewProgress()
{
    m_money = 0;
    m_level = 1;
    m_currentExp = 0;
    m_levelProtection = 0;
    m_level100OverflowExp = 0;
    m_fullness = 75.0f;
    m_hydration = 70.0f;
    m_dehydrationZeroTimer = 0.0f;
    m_dehydrationVisionStrength = 0.0f;
    m_hydrationWasZero = false;
    m_storedFoodCount = 0;
    m_loadoutFoodCount = 0;
    m_storedHeatedFoodCount = 0;
    m_loadoutHeatedFoodCount = 0;
    m_storedRationOneCount = 0;
    m_loadoutRationOneCount = 0;
    m_storedRawFishCount = 0;
    m_loadoutRawFishCount = 0;
    m_storedCookedFishCount = 0;
    m_loadoutCookedFishCount = 0;
    m_storedRawSizedFish.fill(0);
    m_loadoutRawSizedFish.fill(0);
    m_storedCookedSizedFish.fill(0);
    m_loadoutCookedSizedFish.fill(0);
    m_storedCartridgeCount = 0;
    m_loadoutCartridgeCount = 0;
    m_rationFullnessWardTimer = 0.0f;
    m_rationHydrationPenaltyTimer = 0.0f;
    m_unknownWeaponChargeTimer = 0.0f;
    m_unknownWeaponCooldownTimer = 0.0f;
    m_unknownWeaponFiredThisHold = false;
    m_storedWaterBottles.clear();
    m_storedCookingKits.clear();
    m_portableLights.clear();
    m_storedPortableLights.clear();
    m_portableLightOn = false;
    m_storedInventory.clear();
    m_loadoutRelics.fill(0);
    m_identifiedRelics.fill(false);
    m_ownedHeadArmor.fill(false);
    m_ownedBodyArmor.fill(false);
    m_ownedWeapons.fill(false);
    m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedWeapons[static_cast<std::size_t>(WeaponTier::RustyPickaxe)] = true;
    m_equippedHeadArmor = ArmorTier::Leather;
    m_equippedBodyArmor = ArmorTier::Leather;
    m_equippedWeapon = WeaponTier::RustyPickaxe;
    m_nextRelicAcquisitionOrder = 1;
    m_uniqueRelicReturned = false;
    m_uniqueRelicCodexUnlocked = false;
    m_uniqueRelicAchievementUnlocked = false;
    m_uniqueRelicStoryUnlocked = false;
    m_adventurerRank = AdventurerRank::Red;
    m_maxReachedDepth = 1;
    m_questDebt = 0;
    m_deathRecoveryPending = false;
    m_deathRecoveryDiscardConfirm = false;
    m_deathRecoveryDepth = 0;
    m_deathRecoveryFee = 0;
    m_pendingDeathRecoveryRelics.clear();
    m_pendingDeathRecoveryFood = 0;
    m_pendingDeathRecoveryHeatedFood = 0;
    m_pendingDeathRecoveryRationOne = 0;
    m_pendingDeathRecoveryRawFish = 0;
    m_pendingDeathRecoveryCookedFish = 0;
    m_pendingDeathRecoveryRawSizedFish.fill(0);
    m_pendingDeathRecoveryCookedSizedFish.fill(0);
    m_pendingDeathRecoveryCartridges = 0;
    m_pendingDeathRecoveryBottles.clear();
    m_pendingDeathRecoveryCookingKits.clear();
    m_pendingDeathReason.clear();
    m_pendingDeathLevelBefore = 1;
    m_pendingDeathLevelAfter = 1;
    m_pendingDeathProtectionConsumed = 0;
    m_gameWeekSeconds = 0.0;
    m_weekSeed = (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}();
    if (m_weekSeed == 0) m_weekSeed = 1;
    m_diveWorldSeed = m_weekSeed;
    m_weekResetPending = false;
    m_weeklyAreas.clear();
    m_surfacePins.clear();
    m_directGateWeeklyUses.fill(0);
    m_directGateTargetAreas.fill(-1);
    m_directGateTargetPositions.fill({});
    m_nextQuestId = 1;
    m_quests.clear();
    m_selectedQuest = 0;
    InitializeImportantQuests();
    InitializePromotionQuests();
    m_loadedMapVersion = ReadCurrentMapVersion();
}

bool SceneNarakuProto::SaveProgress()
{
#if defined(NARAKU_EDITOR_BUILD)
    return true;
#endif
    CaptureWeeklyWorld();
    const std::wstring directory = kProgressDirectory;
    const std::wstring temporaryPath = kProgressTempPath;
    const std::wstring finalPath = kProgressPath;
    _wmkdir(directory.c_str());
    std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!stream) return false;

    auto writeBoolArray = [&stream](const char* key, const auto& values)
    {
        stream << key << '=';
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) stream << ',';
            stream << (values[i] ? 1 : 0);
        }
        stream << '\n';
    };
    auto writeIntArray = [&stream](const char* key, const auto& values)
    {
        stream << key << '=';
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) stream << ',';
            stream << values[i];
        }
        stream << '\n';
    };

    stream << "NARAKU_PROTO_SAVE\n";
    stream << "version=" << kSaveVersion << '\n';
    stream << "mapVersion=" << ReadCurrentMapVersion() << '\n';
    stream << "money=" << m_money << '\n';
    stream << "level=" << m_level << '\n';
    stream << "exp=" << m_currentExp << '\n';
    stream << "overflowExp=" << m_level100OverflowExp << '\n';
    stream << "protection=" << m_levelProtection << '\n';
    stream << std::fixed << std::setprecision(2) << "fullness=" << m_fullness << '\n';
    stream << "hydration=" << m_hydration << '\n';
    stream << "storedFood=" << m_storedFoodCount << '\n';
    stream << "loadoutFood=" << m_loadoutFoodCount << '\n';
    stream << "storedHeatedFood=" << m_storedHeatedFoodCount << '\n';
    stream << "loadoutHeatedFood=" << m_loadoutHeatedFoodCount << '\n';
    stream << "storedRationOne=" << m_storedRationOneCount << '\n';
    stream << "loadoutRationOne=" << m_loadoutRationOneCount << '\n';
    stream << "storedRawFish=" << m_storedRawFishCount << '\n';
    stream << "loadoutRawFish=" << m_loadoutRawFishCount << '\n';
    stream << "storedCookedFish=" << m_storedCookedFishCount << '\n';
    stream << "loadoutCookedFish=" << m_loadoutCookedFishCount << '\n';
    writeIntArray("storedRawSizedFish", m_storedRawSizedFish);
    writeIntArray("loadoutRawSizedFish", m_loadoutRawSizedFish);
    writeIntArray("storedCookedSizedFish", m_storedCookedSizedFish);
    writeIntArray("loadoutCookedSizedFish", m_loadoutCookedSizedFish);
    stream << "storedCartridges=" << m_storedCartridgeCount << '\n';
    stream << "loadoutCartridges=" << m_loadoutCartridgeCount << '\n';
    stream << "rationFullnessWard=" << m_rationFullnessWardTimer << '\n';
    stream << "rationHydrationPenalty=" << m_rationHydrationPenaltyTimer << '\n';
    stream << "unknownWeaponCooldown=" << m_unknownWeaponCooldownTimer << '\n';
    stream << "equippedHead=" << static_cast<int>(m_equippedHeadArmor) << '\n';
    stream << "equippedBody=" << static_cast<int>(m_equippedBodyArmor) << '\n';
    stream << "equippedWeapon=" << static_cast<int>(m_equippedWeapon) << '\n';
    stream << "nextOrder=" << m_nextRelicAcquisitionOrder << '\n';
    stream << "uniqueReturned=" << (m_uniqueRelicReturned ? 1 : 0) << '\n';
    stream << "uniqueCodex=" << (m_uniqueRelicCodexUnlocked ? 1 : 0) << '\n';
    stream << "uniqueAchievement=" << (m_uniqueRelicAchievementUnlocked ? 1 : 0) << '\n';
    stream << "uniqueStory=" << (m_uniqueRelicStoryUnlocked ? 1 : 0) << '\n';
    stream << "rank=" << static_cast<int>(m_adventurerRank) << '\n';
    stream << "maxReachedDepth=" << m_maxReachedDepth << '\n';
    stream << "questDebt=" << m_questDebt << '\n';
    stream << "deathRecoveryPending=" << (m_deathRecoveryPending ? 1 : 0) << '\n';
    stream << "deathRecoveryDepth=" << m_deathRecoveryDepth << '\n';
    stream << "deathRecoveryFee=" << m_deathRecoveryFee << '\n';
    stream << "deathRecoveryFood=" << m_pendingDeathRecoveryFood << '\n';
    stream << "deathRecoveryHeatedFood=" << m_pendingDeathRecoveryHeatedFood << '\n';
    stream << "deathRecoveryRationOne=" << m_pendingDeathRecoveryRationOne << '\n';
    stream << "deathRecoveryRawFish=" << m_pendingDeathRecoveryRawFish << '\n';
    stream << "deathRecoveryCookedFish=" << m_pendingDeathRecoveryCookedFish << '\n';
    writeIntArray("deathRecoveryRawSizedFish", m_pendingDeathRecoveryRawSizedFish);
    writeIntArray("deathRecoveryCookedSizedFish", m_pendingDeathRecoveryCookedSizedFish);
    stream << "deathRecoveryCartridges=" << m_pendingDeathRecoveryCartridges << '\n';
    stream << "deathRecoveryReason=" << m_pendingDeathReason << '\n';
    stream << "deathRecoveryLevelBefore=" << m_pendingDeathLevelBefore << '\n';
    stream << "deathRecoveryLevelAfter=" << m_pendingDeathLevelAfter << '\n';
    stream << "deathRecoveryProtection=" << m_pendingDeathProtectionConsumed << '\n';
    stream << "gameWeekSeconds=" << m_gameWeekSeconds << '\n';
    stream << "weekSeed=" << m_weekSeed << '\n';
    stream << "weekResetPending=" << (m_weekResetPending ? 1 : 0) << '\n';
    stream << "diveWorldSeed=" << m_diveWorldSeed << '\n';
    stream << "nextQuestId=" << m_nextQuestId << '\n';
    writeIntArray("directGateUses", m_directGateWeeklyUses);
    writeIntArray("directGateTargets", m_directGateTargetAreas);
    stream << "directGateTargetPositions=" << m_directGateTargetPositions[0].x << ',' << m_directGateTargetPositions[0].y
        << ',' << m_directGateTargetPositions[1].x << ',' << m_directGateTargetPositions[1].y << '\n';
    stream << "surfacePinCount=" << m_surfacePins.size() << '\n';
    for (const Vec2& pin : m_surfacePins) stream << "surfacePin=" << pin.x << '|' << pin.y << '\n';
    writeBoolArray("identified", m_identifiedRelics);
    writeBoolArray("ownedHead", m_ownedHeadArmor);
    writeBoolArray("ownedBody", m_ownedBodyArmor);
    writeBoolArray("ownedWeapon", m_ownedWeapons);
    writeIntArray("loadoutRelics", m_loadoutRelics);
    stream << "relicCount=" << m_storedInventory.size() << '\n';
    for (const RelicItem& item : m_storedInventory)
    {
        std::string safeName = item.name;
        std::replace(safeName.begin(), safeName.end(), '|', '/');
        stream << "relic=" << static_cast<int>(item.type) << '|' << item.maxUses << '|' << item.remainingUses << '|'
            << item.acquisitionOrder << '|' << (item.broken ? 1 : 0) << '|' << (item.stabilized ? 1 : 0) << '|'
            << (item.autoTrigger ? 1 : 0) << '|' << safeName << '\n';
    }
    stream << "deathRecoveryRelicCount=" << m_pendingDeathRecoveryRelics.size() << '\n';
    for (const RelicItem& item : m_pendingDeathRecoveryRelics)
    {
        std::string safeName = item.name;
        std::replace(safeName.begin(), safeName.end(), '|', '/');
        stream << "deathRecoveryRelic=" << static_cast<int>(item.type) << '|' << item.maxUses << '|'
            << item.remainingUses << '|' << item.acquisitionOrder << '|' << (item.broken ? 1 : 0) << '|'
            << (item.stabilized ? 1 : 0) << '|' << (item.autoTrigger ? 1 : 0) << '|' << safeName << '\n';
    }
    stream << "bottleCount=" << m_storedWaterBottles.size() << '\n';
    for (const WaterBottle& bottle : m_storedWaterBottles)
    {
        stream << "bottle=" << bottle.amount << '|' << bottle.foodPoisoningChance << '|'
            << static_cast<int>(bottle.quality) << '|' << (bottle.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "deathRecoveryBottleCount=" << m_pendingDeathRecoveryBottles.size() << '\n';
    for (const WaterBottle& bottle : m_pendingDeathRecoveryBottles)
    {
        stream << "deathRecoveryBottle=" << bottle.amount << '|' << bottle.foodPoisoningChance << '|'
            << static_cast<int>(bottle.quality) << '|' << (bottle.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "cookingKitCount=" << m_storedCookingKits.size() << '\n';
    for (const CookingKit& kit : m_storedCookingKits)
    {
        stream << "cookingKit=" << kit.remainingUses << '|' << (kit.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "portableLightCount=" << m_storedPortableLights.size() << '\n';
    for (const PortableLight& light : m_storedPortableLights)
    {
        stream << "portableLight=" << light.remainingSeconds << '|' << (light.broken ? 1 : 0) << '|'
            << (light.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "deathRecoveryCookingKitCount=" << m_pendingDeathRecoveryCookingKits.size() << '\n';
    for (const CookingKit& kit : m_pendingDeathRecoveryCookingKits)
    {
        stream << "deathRecoveryCookingKit=" << kit.remainingUses << '|'
            << (kit.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "questCount=" << m_quests.size() << '\n';
    for (const QuestRecord& quest : m_quests)
    {
        stream << "quest=" << quest.id << '|' << static_cast<int>(quest.type) << '|' << static_cast<int>(quest.status) << '|'
            << quest.targetDepth << '|' << quest.targetCount << '|' << quest.progress << '|' << quest.reward << '|'
            << quest.rewardItemType << '|' << quest.rewardItemCount << '|' << quest.targetRelicType << '|'
            << quest.targetEnemyType << '|' << quest.targetAreaIndex << '|'
            << quest.targetX << '|' << quest.targetZ << '|' << quest.targetLayerDepth << '|' << quest.remainingSeconds << '|'
            << quest.cooldownSeconds << '|' << quest.acceptedAcquisitionOrder << '|'
            << (quest.targetPositionReady ? 1 : 0) << '|' << (quest.targetInteracted ? 1 : 0) << '\n';
    }
    stream << "importantQuestCount=" << m_importantQuests.size() << '\n';
    for (std::size_t index = 0; index < m_importantQuests.size(); ++index)
        stream << "importantQuest=" << index << '|' << static_cast<int>(m_importantQuests[index].status)
            << '|' << m_importantQuests[index].progress << '\n';
    stream << "promotionQuestCount=" << m_promotionQuests.size() << '\n';
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        const PromotionQuestRecord& quest = m_promotionQuests[index];
        stream << "promotionQuest=" << index << '|' << static_cast<int>(quest.status) << '|'
            << quest.acceptedAcquisitionOrder << '|' << quest.cashRelicProgress << '|'
            << quest.upgradeRelicProgress << '\n';
    }
    stream.close();
    if (!stream.good()) return false;
    if (!SaveWeeklyWorld()) return false;

    std::ifstream verify(temporaryPath, std::ios::binary);
    std::ostringstream verifyBuffer;
    verifyBuffer << verify.rdbuf();
    const std::string saved = verifyBuffer.str();
    if (!verify.good() && !verify.eof()) return false;
    if (saved.find("NARAKU_PROTO_SAVE\nversion=11\n") != 0 ||
        saved.find("\nmoney=") == std::string::npos || saved.find("\nlevel=") == std::string::npos ||
        saved.find("\nexp=") == std::string::npos || saved.find("\nfullness=") == std::string::npos ||
        saved.find("\nhydration=") == std::string::npos || saved.find("\nrelicCount=") == std::string::npos ||
        saved.find("\nbottleCount=") == std::string::npos || saved.find("\ncookingKitCount=") == std::string::npos ||
        saved.find("\nportableLightCount=") == std::string::npos ||
        saved.find("\nquestCount=") == std::string::npos || saved.find("\nimportantQuestCount=") == std::string::npos ||
        saved.find("\npromotionQuestCount=") == std::string::npos ||
        saved.find("\nstoredRationOne=") == std::string::npos ||
        saved.find("\nstoredRawFish=") == std::string::npos ||
        saved.find("\nstoredCookedFish=") == std::string::npos ||
        saved.find("\nstoredRawSizedFish=") == std::string::npos ||
        saved.find("\nstoredCartridges=") == std::string::npos ||
        saved.find("\ndirectGateUses=") == std::string::npos ||
        saved.find("\nsurfacePinCount=") == std::string::npos ||
        saved.find("\ndeathRecoveryPending=") == std::string::npos ||
        saved.find("\ndeathRecoveryRelicCount=") == std::string::npos ||
        saved.find("\ndeathRecoveryBottleCount=") == std::string::npos ||
        saved.find("\ndeathRecoveryCookingKitCount=") == std::string::npos) return false;
    verify.close();
    return MoveFileExW(temporaryPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool SceneNarakuProto::CommitModeAfterSave(Mode targetMode, PresentationScene sourceScene)
{
    if (SaveProgress())
    {
        m_mode = targetMode;
        return true;
    }
    m_pendingModeAfterSave = targetMode;
    m_saveErrorPresentation = sourceScene;
    m_mode = Mode::SaveError;
    return false;
}

bool SceneNarakuProto::ReadProgressLoadData(ProgressLoadData& outData) const
{
    std::ifstream stream(kProgressPath, std::ios::binary);
    if (!stream) return false;

    std::string magic;
    std::getline(stream, magic);
    if (magic != "NARAKU_PROTO_SAVE") return false;

    std::string line;
    while (std::getline(stream, line))
    {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        if (key == "relic") outData.relicLines.push_back(value);
        else if (key == "deathRecoveryRelic") outData.deathRecoveryRelicLines.push_back(value);
        else if (key == "bottle") outData.bottleLines.push_back(value);
        else if (key == "deathRecoveryBottle") outData.deathRecoveryBottleLines.push_back(value);
        else if (key == "cookingKit") outData.cookingKitLines.push_back(value);
        else if (key == "portableLight") outData.portableLightLines.push_back(value);
        else if (key == "deathRecoveryCookingKit") outData.deathRecoveryCookingKitLines.push_back(value);
        else if (key == "quest") outData.questLines.push_back(value);
        else if (key == "importantQuest") outData.importantQuestLines.push_back(value);
        else if (key == "promotionQuest") outData.promotionQuestLines.push_back(value);
        else if (key == "surfacePin") outData.surfacePinLines.push_back(value);
        else outData.values[key] = value;
    }
    return true;
}
bool SceneNarakuProto::ValidateProgressLoadData(ProgressLoadData& data, int& outVersion) const
{
    auto& values = data.values;
    if (!values.count("version") || !values.count("money") || !values.count("level"))
        return false;
    const int version = std::stoi(values["version"]);
    outVersion = version;
    if (version != kSaveVersion && version != kPreviousSaveVersion)
        return false;
    if (!values.count("exp") || !values.count("fullness") || !values.count("hydration") || !values.count("relicCount") || !values.count("bottleCount") ||
        !values.count("cookingKitCount") || !values.count("rank") || !values.count("questCount") || !values.count("weekSeed") ||
        !values.count("importantQuestCount"))
        return false;
    if (version == kSaveVersion &&
        (!values.count("deathRecoveryPending") || !values.count("deathRecoveryDepth") || !values.count("deathRecoveryFee") ||
         !values.count("deathRecoveryFood") || !values.count("deathRecoveryHeatedFood") || !values.count("deathRecoveryRelicCount") ||
         !values.count("deathRecoveryBottleCount") || !values.count("deathRecoveryCookingKitCount")))
    {
        return false;
    }
    if (version >= 6 && !values.count("promotionQuestCount"))
        return false;
    if (version == kSaveVersion &&
        (!values.count("weekResetPending") || !values.count("diveWorldSeed") || !values.count("directGateUses") || !values.count("directGateTargets") ||
         !values.count("directGateTargetPositions") || !values.count("surfacePinCount") || !values.count("storedRationOne") ||
         !values.count("loadoutRationOne") || !values.count("storedRawFish") || !values.count("loadoutRawFish") || !values.count("storedCookedFish") ||
         !values.count("loadoutCookedFish") || !values.count("storedCartridges") || !values.count("loadoutCartridges") ||
         !values.count("deathRecoveryRationOne") || !values.count("deathRecoveryRawFish") || !values.count("deathRecoveryCookedFish") ||
         !values.count("deathRecoveryCartridges") || !values.count("storedRawSizedFish") || !values.count("loadoutRawSizedFish") ||
         !values.count("storedCookedSizedFish") || !values.count("loadoutCookedSizedFish") || !values.count("deathRecoveryRawSizedFish") ||
         !values.count("deathRecoveryCookedSizedFish") || !values.count("rationFullnessWard") || !values.count("rationHydrationPenalty") ||
         !values.count("unknownWeaponCooldown") || !values.count("portableLightCount")))
        return false;
    return true;
}

void SceneNarakuProto::ApplyProgressScalarValues(ProgressLoadData& data, int version)
{
    auto& values = data.values;
    m_money = std::max(0, std::stoi(values["money"]));
    m_level = std::max(1, std::min(100, std::stoi(values["level"])));
    m_currentExp = values.count("exp") ? std::max(0, std::stoi(values["exp"])) : 0;
    m_fullness = values.count("fullness") ? std::max(0.0f, std::min(kFullnessMaximum, std::stof(values["fullness"]))) : 75.0f;
    m_hydration = values.count("hydration") ? std::max(0.0f, std::min(kHydrationMaximum, std::stof(values["hydration"]))) : 70.0f;
    if (values.count("overflowExp"))
        m_level100OverflowExp = std::max<std::int64_t>(0, std::stoll(values["overflowExp"]));
    if (values.count("protection"))
        m_levelProtection = std::max(0, std::stoi(values["protection"]));
    if (values.count("storedFood"))
        m_storedFoodCount = std::max(0, std::stoi(values["storedFood"]));
    if (values.count("loadoutFood"))
        m_loadoutFoodCount = std::max(0, std::stoi(values["loadoutFood"]));
    if (values.count("storedHeatedFood"))
        m_storedHeatedFoodCount = std::max(0, std::stoi(values["storedHeatedFood"]));
    if (values.count("loadoutHeatedFood"))
        m_loadoutHeatedFoodCount = std::max(0, std::stoi(values["loadoutHeatedFood"]));
    if (values.count("storedRationOne"))
        m_storedRationOneCount = std::max(0, std::stoi(values["storedRationOne"]));
    if (values.count("loadoutRationOne"))
        m_loadoutRationOneCount = std::max(0, std::stoi(values["loadoutRationOne"]));
    if (values.count("storedRawFish"))
        m_storedRawFishCount = std::max(0, std::stoi(values["storedRawFish"]));
    if (values.count("loadoutRawFish"))
        m_loadoutRawFishCount = std::max(0, std::stoi(values["loadoutRawFish"]));
    if (values.count("storedCookedFish"))
        m_storedCookedFishCount = std::max(0, std::stoi(values["storedCookedFish"]));
    if (values.count("loadoutCookedFish"))
        m_loadoutCookedFishCount = std::max(0, std::stoi(values["loadoutCookedFish"]));
    if (values.count("storedCartridges"))
        m_storedCartridgeCount = std::max(0, std::stoi(values["storedCartridges"]));
    if (values.count("loadoutCartridges"))
        m_loadoutCartridgeCount = std::max(0, std::stoi(values["loadoutCartridges"]));
    if (values.count("rationFullnessWard"))
        m_rationFullnessWardTimer = std::max(0.0f, std::stof(values["rationFullnessWard"]));
    if (values.count("rationHydrationPenalty"))
        m_rationHydrationPenaltyTimer = std::max(0.0f, std::stof(values["rationHydrationPenalty"]));
    if (values.count("unknownWeaponCooldown"))
        m_unknownWeaponCooldownTimer = std::max(0.0f, std::stof(values["unknownWeaponCooldown"]));
    if (values.count("equippedHead"))
    {
        int equipped = std::stoi(values["equippedHead"]);
        if (version == 8 && equipped == 6)
            equipped = static_cast<int>(ArmorTier::None);
        m_equippedHeadArmor = static_cast<ArmorTier>(std::max(0, std::min(static_cast<int>(ArmorTier::Count) - 1, equipped)));
    }
    if (values.count("equippedBody"))
    {
        int equipped = std::stoi(values["equippedBody"]);
        if (version == 8 && equipped == 6)
            equipped = static_cast<int>(ArmorTier::None);
        m_equippedBodyArmor = static_cast<ArmorTier>(std::max(0, std::min(static_cast<int>(ArmorTier::Count) - 1, equipped)));
    }
    if (values.count("equippedWeapon"))
    {
        int equipped = std::stoi(values["equippedWeapon"]);
        if (version == 8 && equipped == 5)
            equipped = static_cast<int>(WeaponTier::None);
        m_equippedWeapon = static_cast<WeaponTier>(std::max(0, std::min(static_cast<int>(WeaponTier::Count) - 1, equipped)));
    }
    if (values.count("nextOrder"))
        m_nextRelicAcquisitionOrder = std::max<std::uint64_t>(1, std::stoull(values["nextOrder"]));
    if (values.count("uniqueReturned"))
        m_uniqueRelicReturned = std::stoi(values["uniqueReturned"]) != 0;
    if (values.count("uniqueCodex"))
        m_uniqueRelicCodexUnlocked = std::stoi(values["uniqueCodex"]) != 0;
    if (values.count("uniqueAchievement"))
        m_uniqueRelicAchievementUnlocked = std::stoi(values["uniqueAchievement"]) != 0;
    if (values.count("uniqueStory"))
        m_uniqueRelicStoryUnlocked = std::stoi(values["uniqueStory"]) != 0;
    {
        m_adventurerRank = static_cast<AdventurerRank>(std::max(0, std::min(static_cast<int>(AdventurerRank::Count) - 1, std::stoi(values["rank"]))));
        m_maxReachedDepth = std::max(1, std::min(5, std::stoi(values["maxReachedDepth"])));
        m_questDebt = std::max(0, std::stoi(values["questDebt"]));
        m_gameWeekSeconds = std::max(0.0, std::min(kGameWeekSeconds, std::stod(values["gameWeekSeconds"])));
        m_weekSeed = std::max<std::uint64_t>(1, std::stoull(values["weekSeed"]));
        m_nextQuestId = std::max<std::uint64_t>(1, std::stoull(values["nextQuestId"]));
        m_weekResetPending = version >= 7 && values.count("weekResetPending") && std::stoi(values["weekResetPending"]) != 0;
        m_diveWorldSeed = version >= 7 && values.count("diveWorldSeed") ? std::max<std::uint64_t>(1, std::stoull(values["diveWorldSeed"])) : m_weekSeed;
    }
    if (version >= 5)
    {
        m_deathRecoveryPending = std::stoi(values["deathRecoveryPending"]) != 0;
        m_deathRecoveryDepth = std::max(0, std::min(5, std::stoi(values["deathRecoveryDepth"])));
        m_deathRecoveryFee = std::max(0, std::stoi(values["deathRecoveryFee"]));
        m_pendingDeathRecoveryFood = std::max(0, std::stoi(values["deathRecoveryFood"]));
        m_pendingDeathRecoveryHeatedFood = std::max(0, std::stoi(values["deathRecoveryHeatedFood"]));
        if (version >= 9)
        {
            m_pendingDeathRecoveryRationOne = std::max(0, std::stoi(values["deathRecoveryRationOne"]));
            m_pendingDeathRecoveryRawFish = std::max(0, std::stoi(values["deathRecoveryRawFish"]));
            m_pendingDeathRecoveryCookedFish = std::max(0, std::stoi(values["deathRecoveryCookedFish"]));
            m_pendingDeathRecoveryCartridges = std::max(0, std::stoi(values["deathRecoveryCartridges"]));
        }
        m_pendingDeathReason = values.count("deathRecoveryReason") ? values["deathRecoveryReason"] : std::string();
        m_pendingDeathLevelBefore =
            values.count("deathRecoveryLevelBefore") ? std::max(1, std::min(100, std::stoi(values["deathRecoveryLevelBefore"]))) : m_level;
        m_pendingDeathLevelAfter = values.count("deathRecoveryLevelAfter") ? std::max(1, std::min(100, std::stoi(values["deathRecoveryLevelAfter"]))) : m_level;
        m_pendingDeathProtectionConsumed = values.count("deathRecoveryProtection") ? std::max(0, std::stoi(values["deathRecoveryProtection"])) : 0;
    }

    const std::string currentMapVersion = ReadCurrentMapVersion();
    const std::string savedMapVersion = values.count("mapVersion") ? values["mapVersion"] : std::string();
    if (savedMapVersion != currentMapVersion)
    {
        m_areas.clear();
        m_weeklyAreas.clear();
        m_weekResetPending = true;
        m_currentAreaIndex = -1;
        m_result = RunResult();
    }
    m_loadedMapVersion = currentMapVersion;
}

void SceneNarakuProto::ApplyProgressCollections(ProgressLoadData& data, int version)
{
    auto& values = data.values;
    const auto& relicLines = data.relicLines;
    const auto& deathRecoveryRelicLines = data.deathRecoveryRelicLines;
    const auto& bottleLines = data.bottleLines;
    const auto& deathRecoveryBottleLines = data.deathRecoveryBottleLines;
    const auto& cookingKitLines = data.cookingKitLines;
    const auto& portableLightLines = data.portableLightLines;
    const auto& deathRecoveryCookingKitLines = data.deathRecoveryCookingKitLines;
    const auto& surfacePinLines = data.surfacePinLines;
    auto readArray = [&values](const char* key, auto& target) {
        if (!values.count(key))
            return;
        std::istringstream input(values[key]);
        std::string token;
        std::size_t index = 0;
        while (std::getline(input, token, ',') && index < target.size())
        {
            target[index++] = std::stoi(token);
        }
    };
    readArray("identified", m_identifiedRelics);
    readArray("ownedHead", m_ownedHeadArmor);
    readArray("ownedBody", m_ownedBodyArmor);
    readArray("ownedWeapon", m_ownedWeapons);
    readArray("loadoutRelics", m_loadoutRelics);
    if (version >= 10)
    {
        readArray("storedRawSizedFish", m_storedRawSizedFish);
        readArray("loadoutRawSizedFish", m_loadoutRawSizedFish);
        readArray("storedCookedSizedFish", m_storedCookedSizedFish);
        readArray("loadoutCookedSizedFish", m_loadoutCookedSizedFish);
        readArray("deathRecoveryRawSizedFish", m_pendingDeathRecoveryRawSizedFish);
        readArray("deathRecoveryCookedSizedFish", m_pendingDeathRecoveryCookedSizedFish);
    }
    readArray("directGateUses", m_directGateWeeklyUses);
    readArray("directGateTargets", m_directGateTargetAreas);
    if (values.count("directGateTargetPositions"))
    {
        std::istringstream positions(values["directGateTargetPositions"]);
        std::string token;
        float parsed[4] = {};
        int index = 0;
        while (std::getline(positions, token, ',') && index < 4)
            parsed[index++] = std::stof(token);
        if (index == 4)
        {
            m_directGateTargetPositions[0] = {parsed[0], parsed[1]};
            m_directGateTargetPositions[1] = {parsed[2], parsed[3]};
        }
    }
    m_surfacePins.clear();
    for (const std::string& pinLine : surfacePinLines)
    {
        const std::size_t separator = pinLine.find('|');
        if (separator == std::string::npos)
            throw std::runtime_error("invalid surface pin");
        m_surfacePins.push_back({std::stof(pinLine.substr(0, separator)), std::stof(pinLine.substr(separator + 1))});
    }
    if (values.count("surfacePinCount") && std::stoull(values["surfacePinCount"]) != m_surfacePins.size())
        throw std::runtime_error("surface pin count mismatch");

    m_storedInventory.clear();
    for (const std::string& relicLine : relicLines)
    {
        std::vector<std::string> parts;
        std::istringstream input(relicLine);
        std::string part;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() < 8)
            throw std::runtime_error("invalid relic record");
        const int typeValue = std::stoi(parts[0]);
        if (typeValue < 0 || typeValue >= static_cast<int>(RelicType::Count))
            throw std::runtime_error("invalid relic type");
        RelicItem item = CreateRelic(static_cast<RelicType>(typeValue), parts[7]);
        item.maxUses = std::max(0, std::stoi(parts[1]));
        item.remainingUses = std::max(0, std::stoi(parts[2]));
        item.acquisitionOrder = std::stoull(parts[3]);
        item.broken = std::stoi(parts[4]) != 0;
        item.stabilized = std::stoi(parts[5]) != 0;
        item.autoTrigger = std::stoi(parts[6]) != 0;
        if (item.broken)
            item.value = 5;
        m_storedInventory.push_back(item);
    }
    if (values.count("relicCount") && static_cast<std::size_t>(std::stoull(values["relicCount"])) != m_storedInventory.size())
        throw std::runtime_error("relic count mismatch");

    m_pendingDeathRecoveryRelics.clear();
    for (const std::string& relicLine : deathRecoveryRelicLines)
    {
        std::vector<std::string> parts;
        std::istringstream input(relicLine);
        std::string part;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() < 8)
            throw std::runtime_error("invalid death recovery relic record");
        const int typeValue = std::stoi(parts[0]);
        if (typeValue < 0 || typeValue >= static_cast<int>(RelicType::Count))
            throw std::runtime_error("invalid death recovery relic type");
        RelicItem item = CreateRelic(static_cast<RelicType>(typeValue), parts[7]);
        item.maxUses = std::max(0, std::stoi(parts[1]));
        item.remainingUses = std::max(0, std::stoi(parts[2]));
        item.acquisitionOrder = std::stoull(parts[3]);
        item.broken = std::stoi(parts[4]) != 0;
        item.stabilized = true;
        item.autoTrigger = std::stoi(parts[6]) != 0;
        if (item.broken)
            item.value = 5;
        m_pendingDeathRecoveryRelics.push_back(item);
    }
    if (version >= 5 && static_cast<std::size_t>(std::stoull(values["deathRecoveryRelicCount"])) != m_pendingDeathRecoveryRelics.size())
    {
        throw std::runtime_error("death recovery relic count mismatch");
    }

    m_storedWaterBottles.clear();
    for (const std::string& bottleLine : bottleLines)
    {
        std::istringstream input(bottleLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 4)
            throw std::runtime_error("invalid bottle record");
        WaterBottle bottle;
        bottle.amount = std::max(0.0f, std::min(kWaterBottleCapacity, std::stof(parts[0])));
        bottle.foodPoisoningChance = std::max(0.0f, std::min(1.0f, std::stof(parts[1])));
        const int quality = std::stoi(parts[2]);
        if (quality < static_cast<int>(WaterQuality::None) || quality > static_cast<int>(WaterQuality::Boiled))
            throw std::runtime_error("invalid water quality");
        bottle.quality = static_cast<WaterQuality>(quality);
        bottle.selectedForLoadout = std::stoi(parts[3]) != 0;
        if (bottle.amount <= 0.0f)
        {
            bottle.quality = WaterQuality::None;
            bottle.foodPoisoningChance = 0.0f;
        }
        m_storedWaterBottles.push_back(bottle);
    }
    if (values.count("bottleCount") && static_cast<std::size_t>(std::stoull(values["bottleCount"])) != m_storedWaterBottles.size())
        throw std::runtime_error("bottle count mismatch");

    m_pendingDeathRecoveryBottles.clear();
    for (const std::string& bottleLine : deathRecoveryBottleLines)
    {
        std::istringstream input(bottleLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 4)
            throw std::runtime_error("invalid death recovery bottle record");
        WaterBottle bottle;
        bottle.amount = std::max(0.0f, std::min(kWaterBottleCapacity, std::stof(parts[0])));
        bottle.foodPoisoningChance = std::max(0.0f, std::min(1.0f, std::stof(parts[1])));
        const int quality = std::stoi(parts[2]);
        if (quality < static_cast<int>(WaterQuality::None) || quality > static_cast<int>(WaterQuality::Boiled))
            throw std::runtime_error("invalid death recovery water quality");
        bottle.quality = static_cast<WaterQuality>(quality);
        bottle.selectedForLoadout = false;
        if (bottle.amount <= 0.0f)
        {
            bottle.quality = WaterQuality::None;
            bottle.foodPoisoningChance = 0.0f;
        }
        m_pendingDeathRecoveryBottles.push_back(bottle);
    }
    if (version >= 5 && static_cast<std::size_t>(std::stoull(values["deathRecoveryBottleCount"])) != m_pendingDeathRecoveryBottles.size())
    {
        throw std::runtime_error("death recovery bottle count mismatch");
    }

    m_storedCookingKits.clear();
    for (const std::string& kitLine : cookingKitLines)
    {
        std::istringstream input(kitLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 2)
            throw std::runtime_error("invalid cooking kit record");
        CookingKit kit;
        kit.remainingUses = std::max(1, std::min(kCookingKitMaxUses, std::stoi(parts[0])));
        kit.selectedForLoadout = std::stoi(parts[1]) != 0;
        m_storedCookingKits.push_back(kit);
    }
    if (values.count("cookingKitCount") && static_cast<std::size_t>(std::stoull(values["cookingKitCount"])) != m_storedCookingKits.size())
        throw std::runtime_error("cooking kit count mismatch");

    m_storedPortableLights.clear();
    for (const std::string& lightLine : portableLightLines)
    {
        std::istringstream input(lightLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 3)
            throw std::runtime_error("invalid portable light record");
        PortableLight light;
        light.remainingSeconds = std::max(0.0f, std::min(kPortableLightDuration, std::stof(parts[0])));
        light.broken = std::stoi(parts[1]) != 0 || light.remainingSeconds <= 0.0f;
        light.selectedForLoadout = std::stoi(parts[2]) != 0;
        m_storedPortableLights.push_back(light);
    }
    if (version == kSaveVersion && static_cast<std::size_t>(std::stoull(values["portableLightCount"])) != m_storedPortableLights.size())
        throw std::runtime_error("portable light count mismatch");
    m_portableLights.clear();
    m_portableLightOn = false;

    m_pendingDeathRecoveryCookingKits.clear();
    for (const std::string& kitLine : deathRecoveryCookingKitLines)
    {
        std::istringstream input(kitLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 2)
            throw std::runtime_error("invalid death recovery cooking kit record");
        CookingKit kit;
        kit.remainingUses = std::max(1, std::min(kCookingKitMaxUses, std::stoi(parts[0])));
        kit.selectedForLoadout = false;
        m_pendingDeathRecoveryCookingKits.push_back(kit);
    }
    if (version >= 5 && static_cast<std::size_t>(std::stoull(values["deathRecoveryCookingKitCount"])) != m_pendingDeathRecoveryCookingKits.size())
    {
        throw std::runtime_error("death recovery cooking kit count mismatch");
    }
    if (m_deathRecoveryPending && (!HasPendingDeathRecoveryItems() || m_deathRecoveryDepth < 1 || m_deathRecoveryFee < 1))
    {
        throw std::runtime_error("invalid pending death recovery state");
    }
}

void SceneNarakuProto::ApplyProgressQuests(ProgressLoadData& data, int version)
{
    auto& values = data.values;
    const auto& questLines = data.questLines;
    const auto& importantQuestLines = data.importantQuestLines;
    const auto& promotionQuestLines = data.promotionQuestLines;
    m_quests.clear();
    for (const std::string& questLine : questLines)
    {
        std::istringstream input(questLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 20)
            throw std::runtime_error("invalid quest record");
        QuestRecord quest;
        quest.id = std::stoull(parts[0]);
        const int type = std::stoi(parts[1]);
        const int status = std::stoi(parts[2]);
        if (type < 0 || type >= static_cast<int>(QuestType::Count) || status < 0 || status > static_cast<int>(QuestStatus::Cooldown))
            throw std::runtime_error("invalid quest enum");
        quest.type = static_cast<QuestType>(type);
        quest.status = static_cast<QuestStatus>(status);
        quest.targetDepth = std::max(1, std::min(5, std::stoi(parts[3])));
        quest.targetCount = std::max(1, std::stoi(parts[4]));
        quest.progress = std::max(0, std::stoi(parts[5]));
        quest.reward = std::max(0, std::stoi(parts[6]));
        quest.rewardItemType = std::stoi(parts[7]);
        quest.rewardItemCount = std::max(0, std::stoi(parts[8]));
        quest.targetRelicType = std::stoi(parts[9]);
        quest.targetEnemyType = std::stoi(parts[10]);
        quest.targetAreaIndex = std::stoi(parts[11]);
        quest.targetX = std::stof(parts[12]);
        quest.targetZ = std::stof(parts[13]);
        quest.targetLayerDepth = std::stof(parts[14]);
        quest.remainingSeconds = std::max(0.0, std::stod(parts[15]));
        quest.cooldownSeconds = std::max(0.0, std::stod(parts[16]));
        quest.acceptedAcquisitionOrder = std::stoull(parts[17]);
        quest.targetPositionReady = std::stoi(parts[18]) != 0;
        quest.targetInteracted = std::stoi(parts[19]) != 0;
        m_quests.push_back(quest);
    }
    if (static_cast<std::size_t>(std::stoull(values["questCount"])) != m_quests.size())
        throw std::runtime_error("quest count mismatch");
    for (const std::string& questLine : importantQuestLines)
    {
        std::istringstream input(questLine);
        std::string part;
        std::vector<std::string> parts;
        while (std::getline(input, part, '|'))
            parts.push_back(part);
        if (parts.size() != 3)
            throw std::runtime_error("invalid important quest record");
        const std::size_t index = static_cast<std::size_t>(std::stoull(parts[0]));
        const int status = std::stoi(parts[1]);
        if (index >= m_importantQuests.size() || status < 0 || status > static_cast<int>(ImportantQuestStatus::Claimed))
            throw std::runtime_error("invalid important quest enum");
        m_importantQuests[index].status = static_cast<ImportantQuestStatus>(status);
        m_importantQuests[index].progress = std::max(0, std::stoi(parts[2]));
    }
    if (static_cast<std::size_t>(std::stoull(values["importantQuestCount"])) != importantQuestLines.size() ||
        importantQuestLines.size() != m_importantQuests.size())
        throw std::runtime_error("important quest count mismatch");
    if (version >= 6)
    {
        m_promotionQuests.fill({});
        for (const std::string& questLine : promotionQuestLines)
        {
            std::istringstream input(questLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|'))
                parts.push_back(part);
            if (parts.size() != 5)
                throw std::runtime_error("invalid promotion quest record");
            const std::size_t index = static_cast<std::size_t>(std::stoull(parts[0]));
            const int status = std::stoi(parts[1]);
            if (index >= m_promotionQuests.size() || status < 0 || status > static_cast<int>(PromotionQuestStatus::Claimed))
            {
                throw std::runtime_error("invalid promotion quest enum");
            }
            PromotionQuestRecord &quest = m_promotionQuests[index];
            quest.status = static_cast<PromotionQuestStatus>(status);
            quest.acceptedAcquisitionOrder = std::stoull(parts[2]);
            quest.cashRelicProgress = std::max(0, std::stoi(parts[3]));
            quest.upgradeRelicProgress = std::max(0, std::stoi(parts[4]));
        }
        if (static_cast<std::size_t>(std::stoull(values["promotionQuestCount"])) != promotionQuestLines.size() ||
            promotionQuestLines.size() != m_promotionQuests.size())
        {
            throw std::runtime_error("promotion quest count mismatch");
        }
        RefreshPromotionQuestAvailability();
    }
    else
    {
        InitializePromotionQuests();
    }
}

bool SceneNarakuProto::LoadProgress()
{
#if defined(NARAKU_EDITOR_BUILD)
    return false;
#endif
    ProgressLoadData data;
    if (!ReadProgressLoadData(data)) return false;

    try
    {
        int version = 0;
        if (!ValidateProgressLoadData(data, version)) return false;
        ApplyProgressScalarValues(data, version);
        ApplyProgressCollections(data, version);
        ApplyProgressQuests(data, version);
        if (version == kSaveVersion && !m_weekResetPending) LoadWeeklyWorld();
    }
    catch (...)
    {
        InitializeNewProgress();
        return false;
    }
    return true;
}
void SceneNarakuProto::ClampDebugPlayerParams()
{
    // 速度、攻撃力、各種消費量、回復量は負値にしないよう安全側へ丸めます。
    m_debugPlayerParams.walkSpeed = std::max(0.0f, m_debugPlayerParams.walkSpeed);
    m_debugPlayerParams.runSpeed = std::max(0.0f, m_debugPlayerParams.runSpeed);
    m_debugPlayerParams.ropeSpeed = std::max(0.0f, m_debugPlayerParams.ropeSpeed);
    m_debugPlayerParams.attackPower = std::max(0.0f, m_debugPlayerParams.attackPower);
    m_debugPlayerParams.runCostPerSecond = std::max(0.0f, m_debugPlayerParams.runCostPerSecond);
    m_debugPlayerParams.ropeCostPerSecond = std::max(0.0f, m_debugPlayerParams.ropeCostPerSecond);
    m_debugPlayerParams.attackCost = std::max(0.0f, m_debugPlayerParams.attackCost);
    m_debugPlayerParams.miningCost = std::max(0.0f, m_debugPlayerParams.miningCost);
    m_debugPlayerParams.stepCost = std::max(0.0f, m_debugPlayerParams.stepCost);
    m_debugPlayerParams.jumpCost = std::max(0.0f, m_debugPlayerParams.jumpCost);
    m_debugPlayerParams.staminaRecoverPerSecond = std::max(0.0f, m_debugPlayerParams.staminaRecoverPerSecond);
    m_debugPlayerParams.upperLayerAlpha = std::max(0.0f, std::min(m_debugPlayerParams.upperLayerAlpha, 0.30f));
}
