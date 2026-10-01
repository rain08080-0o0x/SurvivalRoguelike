/**
 * @file SceneNarakuProto.Quests.cpp
 * @brief ランク、通常依頼、重要依頼、昇格依頼、および地図UIを実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

const char* SceneNarakuProto::GetRankName(AdventurerRank rank) const
{
    switch (rank)
    {
    case AdventurerRank::Red: return u8"赤印";
    case AdventurerRank::Blue: return u8"青印";
    case AdventurerRank::Black: return u8"黒印";
    case AdventurerRank::Purple: return u8"紫印";
    default: return u8"不明";
    }
}
int SceneNarakuProto::GetRankMaximumDepth(AdventurerRank rank) const
{
    return std::min(5, static_cast<int>(rank) + 2);
}

const char* SceneNarakuProto::GetQuestName(QuestType type) const
{
    switch (type)
    {
    case QuestType::LostProperty: return u8"落とし物探し";
    case QuestType::RequestedRelic: return u8"これが欲しい！";
    case QuestType::Hunt: return u8"討伐";
    case QuestType::EnemySurvey: return u8"敵影調査";
    case QuestType::MiningSurvey: return u8"採掘地調査";
    case QuestType::UpgradePlan: return u8"強化計画";
    case QuestType::ValuableRelics: return u8"金目の物";
    case QuestType::SmallThings: return u8"塵も積もれば";
    case QuestType::Rescue: return u8"あの人が帰ってこない……";
    case QuestType::ScrapCollection: return u8"廃品回収";
    default: return u8"不明な依頼";
    }
}

std::string SceneNarakuProto::GetQuestDescription(const QuestRecord& quest) const
{
    std::ostringstream text;
    text << u8"対象: 第" << quest.targetDepth << u8"層\n";
    switch (quest.type)
    {
    case QuestType::LostProperty: text << u8"現地に落ちている依頼品を回収する。"; break;
    case QuestType::RequestedRelic: text << GetRelicTypeName(static_cast<RelicType>(quest.targetRelicType)) << u8"を1個譲渡する。"; break;
    case QuestType::Hunt: text << (quest.targetEnemyType == static_cast<int>(EnemyType::Territory) ? u8"縄張り型" : u8"突進型") << u8"を" << quest.targetCount << u8"体討伐する。"; break;
    case QuestType::EnemySurvey: text << u8"敵を" << quest.targetCount << u8"体発見する。"; break;
    case QuestType::MiningSurvey: text << u8"採掘地点を" << quest.targetCount << u8"箇所発見する。"; break;
    case QuestType::UpgradePlan: text << u8"受注後に強化素材を" << quest.targetCount << u8"個持ち帰る。"; break;
    case QuestType::ValuableRelics: text << u8"換金用遺物（高）を3個譲渡する。"; break;
    case QuestType::SmallThings: text << u8"換金用遺物（低）を10個譲渡する。"; break;
    case QuestType::Rescue: text << u8"食料" << 3 * quest.targetDepth << u8"個と水" << 20 * quest.targetDepth << u8"を届ける。"; break;
    case QuestType::ScrapCollection: text << u8"壊れた遺物を5個譲渡する。"; break;
    default: break;
    }
    return text.str();
}

void SceneNarakuProto::EnsureQuestBoard()
{
#if defined(NARAKU_EDITOR_BUILD)
    return;
#endif
    if (m_weekSeed == 0) m_weekSeed = 1;
    if (m_quests.size() < kQuestBoardSize) m_quests.resize(kQuestBoardSize);
    for (std::size_t index = 0; index < kQuestBoardSize; ++index)
        if (m_quests[index].id == 0) GenerateQuestForSlot(index);
    if (m_selectedQuest >= static_cast<int>(m_quests.size())) m_selectedQuest = 0;
}

void SceneNarakuProto::GenerateQuestForSlot(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size()) return;
    std::mt19937 random(static_cast<unsigned int>(m_weekSeed ^ (m_nextQuestId * 0x9E3779B97F4A7C15ull)));
    auto range = [&random](int minimum, int maximum)
    { return std::uniform_int_distribution<int>(minimum, maximum)(random); };
    std::array<int, static_cast<std::size_t>(QuestType::Count)> typeCounts = {};
    for (std::size_t index = 0; index < m_quests.size(); ++index)
        if (index != slotIndex && m_quests[index].id != 0 && m_quests[index].status != QuestStatus::Cooldown)
            ++typeCounts[static_cast<std::size_t>(m_quests[index].type)];
    int typeValue = range(0, static_cast<int>(QuestType::Count) - 1);
    for (int attempt = 0; attempt < 32 && typeCounts[static_cast<std::size_t>(typeValue)] >= 2; ++attempt)
        typeValue = range(0, static_cast<int>(QuestType::Count) - 1);
    if (typeCounts[static_cast<std::size_t>(typeValue)] >= 2)
        for (int candidate = 0; candidate < static_cast<int>(QuestType::Count); ++candidate)
            if (typeCounts[static_cast<std::size_t>(candidate)] < 2) { typeValue = candidate; break; }

    QuestRecord quest;
    quest.id = m_nextQuestId++;
    quest.type = static_cast<QuestType>(typeValue);
    quest.targetDepth = range(1, std::max(1, std::min(m_maxReachedDepth, GetRankMaximumDepth(m_adventurerRank))));
    const float depthRatio = static_cast<float>(quest.targetDepth - 1) / 4.0f;
    auto interpolate = [depthRatio](int minimum, int maximum)
    { return static_cast<int>(std::llround(minimum + (maximum - minimum) * depthRatio)); };
    switch (quest.type)
    {
    case QuestType::LostProperty: quest.reward = interpolate(1000, 5000); quest.remainingSeconds = interpolate(30, 60) * 60.0; break;
    case QuestType::RequestedRelic: quest.reward = interpolate(1000, 3000); quest.remainingSeconds = 20.0 * 60.0; quest.targetRelicType = range(0, static_cast<int>(RelicType::MentalRecovery)); break;
    case QuestType::Hunt: quest.reward = interpolate(2000, 6000); quest.remainingSeconds = interpolate(30, 90) * 60.0; quest.targetCount = range(1, 5); quest.targetEnemyType = range(0, 1); break;
    case QuestType::EnemySurvey: quest.reward = interpolate(300, 500); quest.remainingSeconds = interpolate(30, 60) * 60.0; quest.targetCount = range(2, 8); break;
    case QuestType::MiningSurvey: quest.reward = interpolate(100, 300); quest.remainingSeconds = interpolate(30, 45) * 60.0; quest.targetCount = range(2, 8); break;
    case QuestType::UpgradePlan: quest.reward = interpolate(1050, 2450); quest.remainingSeconds = interpolate(60, 120) * 60.0; quest.targetCount = range(3, 7); break;
    case QuestType::ValuableRelics: quest.reward = 900 * quest.targetDepth; quest.remainingSeconds = interpolate(30, 45) * 60.0; quest.targetCount = 3; quest.targetRelicType = static_cast<int>(RelicType::CashHigh); break;
    case QuestType::SmallThings: quest.reward = 1000 * quest.targetDepth; quest.remainingSeconds = interpolate(20, 40) * 60.0; quest.targetCount = 10; quest.targetRelicType = static_cast<int>(RelicType::CashLow); break;
    case QuestType::Rescue: quest.reward = interpolate(1000, 3000); quest.remainingSeconds = interpolate(60, 300) * 60.0; break;
    case QuestType::ScrapCollection: quest.reward = 100 * quest.targetDepth; quest.remainingSeconds = 30.0 * 60.0; quest.targetCount = 5; break;
    default: break;
    }
    std::vector<int> candidates;
    for (int area = 0; area < static_cast<int>(m_areas.size()); ++area)
        if (m_areas[area].depth == quest.targetDepth) candidates.push_back(area);
    if (!candidates.empty()) quest.targetAreaIndex = candidates[static_cast<std::size_t>(range(0, static_cast<int>(candidates.size()) - 1))];
    if (quest.type == QuestType::Rescue)
    {
        quest.targetAreaIndex = -1;
        quest.targetPositionReady = false;
    }
    const bool smallReset = quest.type == QuestType::RequestedRelic || quest.type == QuestType::UpgradePlan ||
        quest.type == QuestType::SmallThings || quest.type == QuestType::ScrapCollection;
    const bool endReset = quest.type == QuestType::Rescue;
    quest.cooldownSeconds = endReset ? std::max(1.0, kGameWeekSeconds - m_gameWeekSeconds)
        : (smallReset ? 3600.0 : (quest.type == QuestType::Hunt ? 18000.0 : 10800.0));
    if (quest.type == QuestType::LostProperty) quest.cooldownSeconds = quest.targetDepth <= 2 ? 3600.0 : 10800.0;
    m_quests[slotIndex] = quest;
}

void SceneNarakuProto::ResetAvailableQuestsForNewWeek()
{
    for (std::size_t index = 0; index < m_quests.size(); ++index)
        if (m_quests[index].status == QuestStatus::Available || m_quests[index].status == QuestStatus::Cooldown)
            GenerateQuestForSlot(index);
    ResetPromotionQuestFailuresForNewWeek();
}

void SceneNarakuProto::BeginNewGameWeek()
{
    m_weekSeed = m_weekSeed * 6364136223846793005ull + 1442695040888963407ull;
    m_weekResetPending = true;
    m_directGateWeeklyUses.fill(0);
    m_directGateTargetAreas.fill(-1);
    m_directGateTargetPositions.fill({});
    ResetAvailableQuestsForNewWeek();
    ShowCenterNotification(u8"週が変わりました。次回潜行から奈落の構成が更新されます。");
}

void SceneNarakuProto::UpdateWorldClockAndQuests(float dt)
{
#if defined(NARAKU_EDITOR_BUILD)
    return;
#endif
    if (m_mode != Mode::Explore && m_mode != Mode::Surface) return;
    if (m_mode == Mode::Surface && !m_surfaceWasMoving) return;
    if (m_mode == Mode::Explore)
    {
        UpdateRespawns(dt);
        UpdateMiningRespawns(dt);
    }
    const double worldDelta = dt * kGameTimeScale * (m_mode == Mode::Explore
        ? kWorldTimeDepthMultipliers[static_cast<std::size_t>(GetCurrentDepth() - 1)] : 1.0);
    UpdateFishingPointRecharge(worldDelta);
    m_gameWeekSeconds += worldDelta;
    while (m_gameWeekSeconds >= kGameWeekSeconds)
    {
        m_gameWeekSeconds -= kGameWeekSeconds;
        BeginNewGameWeek();
    }
    for (std::size_t index = 0; index < m_quests.size(); ++index)
    {
        QuestRecord& quest = m_quests[index];
        if (quest.status == QuestStatus::Available)
        {
            quest.cooldownSeconds -= worldDelta;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
        else if (quest.status == QuestStatus::Active)
        {
            quest.remainingSeconds -= dt;
            if (quest.remainingSeconds <= 0.0) FailQuest(index);
        }
        else if (quest.status == QuestStatus::Cooldown)
        {
            quest.cooldownSeconds -= dt;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
    }
}

bool SceneNarakuProto::ConsumeStoredRelicsForQuest(const QuestRecord& quest)
{
    int required = 0;
    int type = quest.targetRelicType;
    bool brokenOnly = false;
    if (quest.type == QuestType::RequestedRelic) required = 1;
    else if (quest.type == QuestType::ValuableRelics) required = 3;
    else if (quest.type == QuestType::SmallThings) required = 10;
    else if (quest.type == QuestType::ScrapCollection) { required = 5; brokenOnly = true; }
    else return true;
    auto matches = [type, brokenOnly](const RelicItem& item)
    { return brokenOnly ? item.broken : static_cast<int>(item.type) == type; };
    const int available = static_cast<int>(std::count_if(m_inventory.begin(), m_inventory.end(), matches) +
        std::count_if(m_storedInventory.begin(), m_storedInventory.end(), matches));
    if (available < required) return false;
    auto consume = [&](std::vector<RelicItem>& items)
    {
        for (auto it = items.begin(); it != items.end() && required > 0;)
        {
            if (matches(*it)) { it = items.erase(it); --required; }
            else ++it;
        }
    };
    consume(m_inventory);
    consume(m_storedInventory);
    for (std::size_t index = 0; index < m_loadoutRelics.size(); ++index)
        m_loadoutRelics[index] = std::min(m_loadoutRelics[index], CountStoredRelics(static_cast<RelicType>(index)));
    return true;
}

bool SceneNarakuProto::AcceptQuest(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size() || m_quests[slotIndex].status != QuestStatus::Available) return false;
    const int activeCount = static_cast<int>(std::count_if(m_quests.begin(), m_quests.end(), [](const QuestRecord& quest)
    { return quest.status == QuestStatus::Active || quest.status == QuestStatus::Complete; }));
    if (activeCount >= kMaximumActiveQuests) { ShowCenterNotification(u8"受注上限は3件です。"); return false; }
    QuestRecord& quest = m_quests[slotIndex];
    if (!ConsumeStoredRelicsForQuest(quest)) { ShowCenterNotification(u8"譲渡に必要な遺物が足りません。"); return false; }
    quest.status = QuestStatus::Active;
    quest.progress = 0;
    quest.acceptedAcquisitionOrder = m_nextRelicAcquisitionOrder;
    if (quest.type == QuestType::RequestedRelic || quest.type == QuestType::ValuableRelics ||
        quest.type == QuestType::SmallThings || quest.type == QuestType::ScrapCollection)
    {
        quest.progress = quest.targetCount;
        quest.status = QuestStatus::Complete;
    }
    if (quest.type == QuestType::Rescue)
    {
        std::vector<int> candidates;
        for (int area = 0; area < static_cast<int>(m_areas.size()); ++area)
            if (m_areas[area].depth == quest.targetDepth) candidates.push_back(area);
        if (!candidates.empty()) quest.targetAreaIndex = candidates[static_cast<std::size_t>(quest.id % candidates.size())];
        quest.targetPositionReady = false;
    }
    SaveProgress();
    return true;
}

void SceneNarakuProto::ReportQuest(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size() || m_quests[slotIndex].status != QuestStatus::Complete) return;
    m_money += m_quests[slotIndex].reward;
    if (m_quests[slotIndex].rewardItemType >= 0 && m_quests[slotIndex].rewardItemType < static_cast<int>(RelicType::Count))
    {
        for (int count = 0; count < m_quests[slotIndex].rewardItemCount; ++count)
        {
            RelicItem item = CreateRelic(static_cast<RelicType>(m_quests[slotIndex].rewardItemType), u8"依頼報酬");
            item.stabilized = true;
            item.acquisitionOrder = m_nextRelicAcquisitionOrder++;
            m_storedInventory.push_back(item);
        }
    }
    m_quests[slotIndex].status = QuestStatus::Cooldown;
    m_quests[slotIndex].cooldownSeconds = kQuestSlotCooldownSeconds;
    ShowCenterNotification(u8"依頼を報告し、報酬を受け取りました。");
    SaveProgress();
}

bool SceneNarakuProto::ForceSellOneLowestValueItem(int& remainingPenalty)
{
    enum class SaleKind { None, StoredRelic, CarriedRelic, Head, Body, Weapon };
    SaleKind kind = SaleKind::None;
    std::size_t selected = 0;
    int value = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < m_storedInventory.size(); ++index)
        if (IsRelicSellable(m_storedInventory[index]) && m_storedInventory[index].value > 0 && m_storedInventory[index].value < value)
        { kind = SaleKind::StoredRelic; selected = index; value = m_storedInventory[index].value; }
    for (std::size_t index = 0; index < m_inventory.size(); ++index)
        if (IsRelicSellable(m_inventory[index]) && m_inventory[index].value > 0 && m_inventory[index].value < value)
        { kind = SaleKind::CarriedRelic; selected = index; value = m_inventory[index].value; }
    static const int armorMoneyPrices[7][2] = { {10,15},{150,200},{1000,1500},{1250,1750},{1750,2000},{25000,30000},{kUnknownHeadPrice,kUnknownBodyPrice} };
    static const int weaponMoneyPrices[6] = { 5,500,1250,1500,25000,kUnknownWeaponPrice };
    for (std::size_t index = 0; index < 7; ++index)
    {
        const int headValue = armorMoneyPrices[index][0] / 10;
        if (m_ownedHeadArmor[index] && headValue > 0 && headValue < value) { kind = SaleKind::Head; selected = index; value = headValue; }
        const int bodyValue = armorMoneyPrices[index][1] / 10;
        if (m_ownedBodyArmor[index] && bodyValue > 0 && bodyValue < value) { kind = SaleKind::Body; selected = index; value = bodyValue; }
    }
    for (std::size_t index = 0; index < 6; ++index)
    {
        const int weaponValue = weaponMoneyPrices[index] / 10;
        if (m_ownedWeapons[index] && weaponValue > 0 && weaponValue < value) { kind = SaleKind::Weapon; selected = index; value = weaponValue; }
    }
    if (kind == SaleKind::None) return false;
    if (kind == SaleKind::StoredRelic)
    {
        const RelicType type = m_storedInventory[selected].type;
        m_storedInventory.erase(m_storedInventory.begin() + selected);
        const std::size_t typeIndex = static_cast<std::size_t>(type);
        m_loadoutRelics[typeIndex] = std::min(m_loadoutRelics[typeIndex], CountStoredRelics(type));
    }
    else if (kind == SaleKind::CarriedRelic) m_inventory.erase(m_inventory.begin() + selected);
    else if (kind == SaleKind::Head) { m_ownedHeadArmor[selected] = false; if (m_equippedHeadArmor == static_cast<ArmorTier>(selected)) m_equippedHeadArmor = ArmorTier::None; }
    else if (kind == SaleKind::Body) { m_ownedBodyArmor[selected] = false; if (m_equippedBodyArmor == static_cast<ArmorTier>(selected)) m_equippedBodyArmor = ArmorTier::None; }
    else { m_ownedWeapons[selected] = false; if (m_equippedWeapon == static_cast<WeaponTier>(selected)) m_equippedWeapon = WeaponTier::None; }
    remainingPenalty = std::max(0, remainingPenalty - value);
    return true;
}

void SceneNarakuProto::ApplyQuestPenalty(int penalty)
{
    const int paid = std::min(m_money, penalty);
    m_money -= paid;
    int remaining = penalty - paid;
    while (remaining > 0 && ForceSellOneLowestValueItem(remaining)) {}
    m_questDebt += remaining;
}

void SceneNarakuProto::FailQuest(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size() || m_quests[slotIndex].status != QuestStatus::Active) return;
    ApplyQuestPenalty(static_cast<int>(std::llround(m_quests[slotIndex].reward * 0.1)));
    m_quests[slotIndex].status = QuestStatus::Cooldown;
    m_quests[slotIndex].cooldownSeconds = kQuestSlotCooldownSeconds;
    ShowCenterNotification(u8"依頼の期限が切れ、違約金が発生しました。");
    SaveProgress();
}

void SceneNarakuProto::UpdateQuestDiscoveryProgress(bool enemyDiscovered, bool miningDiscovered, int depth)
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.targetDepth != depth) continue;
        if ((enemyDiscovered && quest.type == QuestType::EnemySurvey) ||
            (miningDiscovered && quest.type == QuestType::MiningSurvey))
        {
            quest.progress = std::min(quest.targetCount, quest.progress + 1);
            if (quest.progress >= quest.targetCount) quest.status = QuestStatus::Complete;
        }
    }
}

void SceneNarakuProto::UpdateQuestReturnProgress(const std::vector<RelicItem>& returnedItems)
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.type != QuestType::UpgradePlan) continue;
        for (const RelicItem& item : returnedItems)
        {
            const bool upgrade = item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade || item.type == RelicType::ArmorUpgrade;
            if (upgrade && item.acquisitionOrder >= quest.acceptedAcquisitionOrder) ++quest.progress;
        }
        quest.progress = std::min(quest.progress, quest.targetCount);
        if (quest.progress >= quest.targetCount) quest.status = QuestStatus::Complete;
    }
}

void SceneNarakuProto::InitializeImportantQuests()
{
    m_importantQuests.fill({});
    UnlockImportantQuest(ImportantQuestType::FirstJourney);
    UnlockImportantQuest(ImportantQuestType::FirstHunt);
    UnlockImportantQuest(ImportantQuestType::FirstTerritoryHunt);
    UnlockImportantQuest(ImportantQuestType::FindSecondBase);
    UnlockImportantQuest(ImportantQuestType::FindForwardBase);
    UnlockImportantQuest(ImportantQuestType::ReturnUniqueRelic);
    m_selectedImportantQuest = 0;
}

void SceneNarakuProto::UnlockImportantQuest(ImportantQuestType type)
{
    ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
    if (quest.status == ImportantQuestStatus::Locked) quest.status = ImportantQuestStatus::Active;
}

const char* SceneNarakuProto::GetImportantQuestName(ImportantQuestType type) const
{
    switch (type)
    {
    case ImportantQuestType::FirstJourney: return u8"初めての旅立ち";
    case ImportantQuestType::FirstClear: return u8"最初が肝心";
    case ImportantQuestType::ReachSecond: return u8"出発！第二層";
    case ImportantQuestType::ClearSecond: return u8"第二層制覇";
    case ImportantQuestType::ReachThird: return u8"中層到達";
    case ImportantQuestType::ClearThird: return u8"折り返し地点？";
    case ImportantQuestType::ReachFourth: return u8"先の見えない遠足";
    case ImportantQuestType::ClearFourth: return u8"ここが中間地点";
    case ImportantQuestType::ReachFifth: return u8"旅の終着点。そして始まり";
    case ImportantQuestType::ClearFifth: return u8"奈落への入り口";
    case ImportantQuestType::FirstHunt: return u8"初めての討伐";
    case ImportantQuestType::FiveHunts: return u8"連続討伐";
    case ImportantQuestType::ThirteenHunts: return u8"掃討";
    case ImportantQuestType::FirstTerritoryHunt: return u8"縄張り争いの勝者";
    case ImportantQuestType::ThreeTerritoryHunts: return u8"ここを拠点とする！";
    case ImportantQuestType::FindSecondBase: return u8"第二拠点";
    case ImportantQuestType::FindForwardBase: return u8"前衛拠点。到着";
    case ImportantQuestType::ReturnUniqueRelic: return u8"特異な物体";
    default: return u8"不明";
    }
}

const char* SceneNarakuProto::GetImportantQuestDescription(ImportantQuestType type) const
{
    switch (type)
    {
    case ImportantQuestType::FirstJourney: return u8"第一層上層へ到達する。";
    case ImportantQuestType::FirstClear: return u8"第一層の全エリアと採掘地点の7割を発見する。";
    case ImportantQuestType::ReachSecond: return u8"第二層上層へ到達する。";
    case ImportantQuestType::ClearSecond: return u8"第二層を踏破する。";
    case ImportantQuestType::ReachThird: return u8"第三層上層へ到達する。";
    case ImportantQuestType::ClearThird: return u8"第三層を踏破する。";
    case ImportantQuestType::ReachFourth: return u8"第四層上層へ到達する。";
    case ImportantQuestType::ClearFourth: return u8"第四層を踏破する。";
    case ImportantQuestType::ReachFifth: return u8"第五層上層へ到達する。";
    case ImportantQuestType::ClearFifth: return u8"第五層を踏破する。";
    case ImportantQuestType::FirstHunt: return u8"種類を問わず敵を1体討伐する。";
    case ImportantQuestType::FiveHunts: return u8"1回の潜行で敵を5体討伐する。";
    case ImportantQuestType::ThirteenHunts: return u8"1回の潜行で敵を13体討伐する。";
    case ImportantQuestType::FirstTerritoryHunt: return u8"縄張り型を1体討伐する。";
    case ImportantQuestType::ThreeTerritoryHunts: return u8"縄張り型を3体討伐する。";
    case ImportantQuestType::FindSecondBase: return u8"第二層中層の第二拠点を発見する。";
    case ImportantQuestType::FindForwardBase: return u8"第五層下層の前衛拠点へ到達する。";
    case ImportantQuestType::ReturnUniqueRelic: return u8"固有遺物を初めて生還して持ち帰る。";
    default: return "";
    }
}

int SceneNarakuProto::GetImportantQuestMoneyReward(ImportantQuestType type) const
{
    static constexpr int values[] = { 100,1000,100,2500,100,5000,100,7500,1000,10000,200,1000,2000,700,3000,100,1000,1000 };
    return values[static_cast<std::size_t>(type)];
}

int SceneNarakuProto::GetImportantQuestExpReward(ImportantQuestType type) const
{
    static constexpr int values[] = { 10,10,20,3000,20,5000,20,7500,200,17500,0,0,0,0,0,0,0,0 };
    return values[static_cast<std::size_t>(type)];
}

int SceneNarakuProto::GetImportantQuestFoodReward(ImportantQuestType type) const
{
    static constexpr int values[] = { 10,20,15,15,15,15,15,25,15,30,5,10,20,4,20,0,0,0 };
    return values[static_cast<std::size_t>(type)];
}

int SceneNarakuProto::GetImportantQuestWaterBottleReward(ImportantQuestType type) const
{
    return type == ImportantQuestType::FirstJourney ? 1 : 0;
}

bool SceneNarakuProto::IsQuestDeskUnlocked() const
{
    return m_importantQuests[static_cast<std::size_t>(ImportantQuestType::FirstJourney)].status >=
        ImportantQuestStatus::Complete;
}

void SceneNarakuProto::UpdateImportantQuestArrival()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size())) return;
    const AreaState& area = m_areas[static_cast<std::size_t>(m_currentAreaIndex)];
    const auto complete = [this](ImportantQuestType type)
    {
        ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
        if (quest.status != ImportantQuestStatus::Active) return;
        quest.progress = 1;
        quest.status = ImportantQuestStatus::Complete;
        ShowCenterNotification(u8"重要クエストの条件を達成しました。");
    };
    if (area.sublayer == 0)
    {
        if (area.depth == 1) complete(ImportantQuestType::FirstJourney);
        else if (area.depth == 2) complete(ImportantQuestType::ReachSecond);
        else if (area.depth == 3) complete(ImportantQuestType::ReachThird);
        else if (area.depth == 4) complete(ImportantQuestType::ReachFourth);
        else if (area.depth == 5) complete(ImportantQuestType::ReachFifth);
    }
    UpdateImportantQuestExploration();
}

void SceneNarakuProto::UpdateImportantQuestExploration()
{
    const auto complete = [this](ImportantQuestType type)
    {
        ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
        if (quest.status != ImportantQuestStatus::Active) return;
        quest.progress = 1;
        quest.status = ImportantQuestStatus::Complete;
        ShowCenterNotification(u8"重要クエストの条件を達成しました。");
    };

    for (int depth = 1; depth <= 5; ++depth)
    {
        bool allReached = true;
        int miningTotal = 0;
        int miningDiscovered = 0;
        for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
        {
            const AreaState& area = m_areas[static_cast<std::size_t>(areaIndex)];
            if (area.depth != depth) continue;
            if (!area.generated) allReached = false;
            const std::vector<MiningPoint>& points = areaIndex == m_currentAreaIndex ? m_miningPoints : area.miningPoints;
            miningTotal += static_cast<int>(points.size());
            miningDiscovered += static_cast<int>(std::count_if(points.begin(), points.end(), [](const MiningPoint& point) { return point.discovered; }));
        }
        if (!allReached || miningTotal <= 0 || miningDiscovered * 10 < miningTotal * 7) continue;
        if (depth == 1) complete(ImportantQuestType::FirstClear);
        else if (depth == 2) complete(ImportantQuestType::ClearSecond);
        else if (depth == 3) complete(ImportantQuestType::ClearThird);
        else if (depth == 4) complete(ImportantQuestType::ClearFourth);
        else complete(ImportantQuestType::ClearFifth);
    }

    for (const NarakuMap::BasePoint& base : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base.layerId);
        if (layerIndex < 0 || std::fabs(m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth - m_player.depth) > 0.35f ||
            Distance({ base.xz.x, base.xz.z }, m_player.pos) > kDiscoveryRange) continue;
        complete(base.type == NarakuMap::BaseType::SecondBase
            ? ImportantQuestType::FindSecondBase : ImportantQuestType::FindForwardBase);
    }
}

void SceneNarakuProto::UpdateImportantQuestDefeat(const EnemyState& enemy)
{
    const int totalKills = std::accumulate(m_result.chargerKillsByDepth.begin(), m_result.chargerKillsByDepth.end(), 0) +
        std::accumulate(m_result.territoryKillsByDepth.begin(), m_result.territoryKillsByDepth.end(), 0);
    const int territoryKills = std::accumulate(m_result.territoryKillsByDepth.begin(), m_result.territoryKillsByDepth.end(), 0);
    const auto update = [this](ImportantQuestType type, int progress, int required)
    {
        ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
        if (quest.status != ImportantQuestStatus::Active) return;
        quest.progress = std::min(required, progress);
        if (quest.progress >= required) quest.status = ImportantQuestStatus::Complete;
    };
    update(ImportantQuestType::FirstHunt, totalKills, 1);
    update(ImportantQuestType::FiveHunts, totalKills, 5);
    update(ImportantQuestType::ThirteenHunts, totalKills, 13);
    if (enemy.type == EnemyType::Territory)
    {
        update(ImportantQuestType::FirstTerritoryHunt, territoryKills, 1);
        update(ImportantQuestType::ThreeTerritoryHunts, territoryKills, 3);
    }
}

void SceneNarakuProto::UpdateImportantQuestUniqueReturn()
{
    ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(ImportantQuestType::ReturnUniqueRelic)];
    if (quest.status == ImportantQuestStatus::Active)
    {
        quest.progress = 1;
        quest.status = ImportantQuestStatus::Complete;
    }
}

void SceneNarakuProto::ReportImportantQuest(std::size_t questIndex)
{
    if (questIndex >= m_importantQuests.size()) return;
    ImportantQuestRecord& quest = m_importantQuests[questIndex];
    if (quest.status != ImportantQuestStatus::Complete) return;
    const ImportantQuestType type = static_cast<ImportantQuestType>(questIndex);
    m_money += GetImportantQuestMoneyReward(type);
    AwardExp(GetImportantQuestExpReward(type));
    m_storedFoodCount += GetImportantQuestFoodReward(type);
    for (int count = 0; count < GetImportantQuestWaterBottleReward(type); ++count)
        m_storedWaterBottles.push_back({});
    quest.status = ImportantQuestStatus::Claimed;

    if (type == ImportantQuestType::FirstJourney) { UnlockImportantQuest(ImportantQuestType::FirstClear); UnlockImportantQuest(ImportantQuestType::ReachSecond); }
    else if (type == ImportantQuestType::ReachSecond) { UnlockImportantQuest(ImportantQuestType::ClearSecond); UnlockImportantQuest(ImportantQuestType::ReachThird); }
    else if (type == ImportantQuestType::ReachThird) { UnlockImportantQuest(ImportantQuestType::ClearThird); UnlockImportantQuest(ImportantQuestType::ReachFourth); }
    else if (type == ImportantQuestType::ReachFourth) { UnlockImportantQuest(ImportantQuestType::ClearFourth); UnlockImportantQuest(ImportantQuestType::ReachFifth); }
    else if (type == ImportantQuestType::ReachFifth) UnlockImportantQuest(ImportantQuestType::ClearFifth);
    else if (type == ImportantQuestType::FirstHunt) UnlockImportantQuest(ImportantQuestType::FiveHunts);
    else if (type == ImportantQuestType::FiveHunts) UnlockImportantQuest(ImportantQuestType::ThirteenHunts);
    else if (type == ImportantQuestType::FirstTerritoryHunt) UnlockImportantQuest(ImportantQuestType::ThreeTerritoryHunts);
    SaveProgress();
}

void SceneNarakuProto::InitializePromotionQuests()
{
    m_promotionQuests.fill({});
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        if (static_cast<int>(m_adventurerRank) >= static_cast<int>(index) + 1)
        {
            m_promotionQuests[index].status = PromotionQuestStatus::Claimed;
        }
    }
    m_selectedPromotionQuest = 0;
    RefreshPromotionQuestAvailability();
}

int SceneNarakuProto::GetPromotionQuestTargetDepth(std::size_t questIndex) const
{
    static constexpr int values[] = { 3, 4, 5 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestRequiredLevel(std::size_t questIndex) const
{
    static constexpr int values[] = { 20, 35, 55 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestRequiredCash(std::size_t questIndex) const
{
    static constexpr int values[] = { 3, 2, 3 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestRequiredUpgrade(std::size_t questIndex) const
{
    static constexpr int values[] = { 1, 5, 10 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestMoneyReward(std::size_t questIndex) const
{
    static constexpr int values[] = { 1000, 1500, 1000 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestExpReward(std::size_t questIndex) const
{
    static constexpr int values[] = { 1500, 3500, 5500 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestMaterialReward(std::size_t questIndex) const
{
    static constexpr int values[] = { 10, 5, 15 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

const char* SceneNarakuProto::GetPromotionQuestName(std::size_t questIndex) const
{
    static constexpr const char* values[] = { u8"昇格試験！-青-", u8"昇格試験！-黒-", u8"昇格試験！-紫-" };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : u8"不明";
}

void SceneNarakuProto::RefreshPromotionQuestAvailability()
{
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        PromotionQuestRecord& quest = m_promotionQuests[index];
        const int targetRank = static_cast<int>(index) + 1;
        if (static_cast<int>(m_adventurerRank) >= targetRank)
        {
            quest.status = PromotionQuestStatus::Claimed;
            continue;
        }
        if (quest.status != PromotionQuestStatus::Locked) continue;
        if (static_cast<int>(m_adventurerRank) == targetRank - 1 &&
            m_level >= GetPromotionQuestRequiredLevel(index) &&
            m_maxReachedDepth >= GetPromotionQuestTargetDepth(index))
        {
            quest.status = PromotionQuestStatus::Available;
        }
    }
}

void SceneNarakuProto::ResetPromotionQuestFailuresForNewWeek()
{
    for (PromotionQuestRecord& quest : m_promotionQuests)
    {
        if (quest.status == PromotionQuestStatus::FailedThisWeek)
        {
            quest = PromotionQuestRecord();
        }
    }
    RefreshPromotionQuestAvailability();
}

bool SceneNarakuProto::AcceptPromotionQuest(std::size_t questIndex)
{
    RefreshPromotionQuestAvailability();
    if (questIndex >= m_promotionQuests.size() ||
        m_promotionQuests[questIndex].status != PromotionQuestStatus::Available)
    {
        return false;
    }
    PromotionQuestRecord& quest = m_promotionQuests[questIndex];
    quest.status = PromotionQuestStatus::Active;
    quest.acceptedAcquisitionOrder = m_nextRelicAcquisitionOrder;
    quest.cashRelicProgress = 0;
    quest.upgradeRelicProgress = 0;
    ShowCenterNotification(u8"昇格試験を受注しました。指定深度から生還してください。");
    SaveProgress();
    return true;
}

void SceneNarakuProto::UpdatePromotionQuestReturn(const std::vector<RelicItem>& returnedItems)
{
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        PromotionQuestRecord& quest = m_promotionQuests[index];
        if (quest.status != PromotionQuestStatus::Active) continue;
        int cash = 0;
        int upgrade = 0;
        for (const RelicItem& item : returnedItems)
        {
            const auto depth = m_runRelicAcquisitionDepths.find(item.acquisitionOrder);
            if (item.acquisitionOrder < quest.acceptedAcquisitionOrder ||
                depth == m_runRelicAcquisitionDepths.end() ||
                depth->second != GetPromotionQuestTargetDepth(index))
            {
                continue;
            }
            if (item.type == RelicType::CashLow || item.type == RelicType::CashHigh) ++cash;
            else if (item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade ||
                item.type == RelicType::ArmorUpgrade) ++upgrade;
        }
        quest.cashRelicProgress = std::min(cash, GetPromotionQuestRequiredCash(index));
        quest.upgradeRelicProgress = std::min(upgrade, GetPromotionQuestRequiredUpgrade(index));
        if (cash >= GetPromotionQuestRequiredCash(index) && upgrade >= GetPromotionQuestRequiredUpgrade(index))
        {
            quest.status = PromotionQuestStatus::Complete;
            ShowCenterNotification(u8"昇格試験の条件を達成しました。受付で報告できます。");
        }
        else
        {
            quest.status = PromotionQuestStatus::FailedThisWeek;
            ShowCenterNotification(u8"昇格試験に失敗しました。翌週まで再受注できません。");
        }
    }
}

void SceneNarakuProto::FailActivePromotionQuest()
{
    for (PromotionQuestRecord& quest : m_promotionQuests)
    {
        if (quest.status != PromotionQuestStatus::Active) continue;
        quest.status = PromotionQuestStatus::FailedThisWeek;
        ShowCenterNotification(u8"昇格試験に失敗しました。翌週まで再受注できません。");
    }
}

void SceneNarakuProto::ReportPromotionQuest(std::size_t questIndex)
{
    if (questIndex >= m_promotionQuests.size() ||
        m_promotionQuests[questIndex].status != PromotionQuestStatus::Complete)
    {
        return;
    }
    m_money += GetPromotionQuestMoneyReward(questIndex);
    AwardExp(GetPromotionQuestExpReward(questIndex));
    for (int count = 0; count < GetPromotionQuestMaterialReward(questIndex); ++count)
    {
        const RelicType type = static_cast<RelicType>(RandomInt(
            static_cast<int>(RelicType::ArmamentUpgrade), static_cast<int>(RelicType::ArmorUpgrade)));
        RelicItem item = CreateRelic(type, u8"昇格試験報酬");
        item.stabilized = true;
        m_storedInventory.push_back(item);
    }
    m_adventurerRank = static_cast<AdventurerRank>(static_cast<int>(questIndex) + 1);
    m_promotionQuests[questIndex].status = PromotionQuestStatus::Claimed;
    RefreshPromotionQuestAvailability();
    ShowCenterNotification(u8"昇格試験に合格し、階級が上がりました。");
    SaveProgress();
}

void SceneNarakuProto::EnsureQuestTargetPosition(QuestRecord& quest)
{
    if (quest.targetPositionReady || quest.targetAreaIndex != m_currentAreaIndex) return;
    quest.targetX = m_startPoint.x;
    quest.targetZ = m_startPoint.y;
    quest.targetLayerDepth = m_startDepth;
    if (!m_floorRegions.empty())
    {
        const FloorRegion& floor = m_floorRegions[static_cast<std::size_t>(quest.id % m_floorRegions.size())];
        quest.targetX = floor.center.x;
        quest.targetZ = floor.center.y;
        quest.targetLayerDepth = floor.depth;
    }
    quest.targetPositionReady = true;
}

bool SceneNarakuProto::TryInteractWithQuestTarget()
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.targetAreaIndex != m_currentAreaIndex ||
            (quest.type != QuestType::LostProperty && quest.type != QuestType::Rescue)) continue;
        EnsureQuestTargetPosition(quest);
        if (!IsNear(m_player.pos, { quest.targetX, quest.targetZ }, kInteractRange) ||
            std::fabs(m_player.depth - quest.targetLayerDepth) > 0.35f) continue;
        if (quest.type == QuestType::Rescue)
        {
            const int foodRequired = 3 * quest.targetDepth;
            const float waterRequired = static_cast<float>(20 * quest.targetDepth);
            float water = 0.0f;
            for (const WaterBottle& bottle : m_waterBottles) if (bottle.quality != WaterQuality::None) water += bottle.amount;
            if (m_foodCount + m_heatedFoodCount < foodRequired || water < waterRequired)
            { ShowCenterNotification(u8"救助に必要な食料または水が足りません。"); return true; }
            int food = foodRequired;
            const int normal = std::min(food, m_foodCount); m_foodCount -= normal; food -= normal;
            m_heatedFoodCount -= food;
            float consumeWater = waterRequired;
            for (WaterBottle& bottle : m_waterBottles)
            {
                if (bottle.quality == WaterQuality::None || consumeWater <= 0.0f) continue;
                const float amount = std::min(consumeWater, bottle.amount);
                bottle.amount -= amount; consumeWater -= amount;
                if (bottle.amount <= 0.0f) { bottle.amount = 0.0f; bottle.quality = WaterQuality::None; bottle.foodPoisoningChance = 0.0f; }
            }
        }
        quest.progress = quest.targetCount;
        quest.targetInteracted = true;
        quest.status = QuestStatus::Complete;
        ShowCenterNotification(u8"依頼条件を達成しました。受付で報告できます。");
        SaveProgress();
        return true;
    }
    return false;
}

void SceneNarakuProto::DrawQuestTargets3D()
{
    for (QuestRecord& quest : m_quests)
    {
        const bool lostVisible = quest.type == QuestType::LostProperty && quest.status == QuestStatus::Available;
        const bool activeVisible = quest.status == QuestStatus::Active &&
            (quest.type == QuestType::LostProperty || quest.type == QuestType::Rescue);
        if ((!lostVisible && !activeVisible) || quest.targetAreaIndex != m_currentAreaIndex) continue;
        EnsureQuestTargetPosition(quest);
        const DirectX::XMFLOAT3 base = ToWorld3D({ quest.targetX, quest.targetZ }, quest.targetLayerDepth, 0.05f);
        const DirectX::XMFLOAT3 scale = quest.type == QuestType::Rescue
            ? DirectX::XMFLOAT3{ 0.6f, 1.4f, 0.6f } : DirectX::XMFLOAT3{ 0.4f, 0.3f, 0.4f };
        DrawDebugBox3D({ base.x, base.y + scale.y * 0.5f, base.z }, scale);
    }
}

void SceneNarakuProto::DrawQuestDesk()
{
    EnsureQuestBoard();
    RefreshPromotionQuestAvailability();

    DrawTownFrame(u8"依頼受付", [this]()
    {
        ImGui::Text(u8"階級: %s  保険対象: 第%d層まで  受注: %d/%d", GetRankName(m_adventurerRank),
            GetRankMaximumDepth(m_adventurerRank), static_cast<int>(std::count_if(m_quests.begin(), m_quests.end(), [](const QuestRecord& quest)
            { return quest.status == QuestStatus::Active || quest.status == QuestStatus::Complete; })), kMaximumActiveQuests);
        ImGui::Text(u8"所持金: %dG  借金: %dG", m_money, m_questDebt);
        static const char* kWeekdayNames[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
        const int questDeskWeekSecond = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
        const int questDeskDay = questDeskWeekSecond / static_cast<int>(kGameDaySeconds);
        const int questDeskDaySecond = questDeskWeekSecond % static_cast<int>(kGameDaySeconds);
        ImGui::Text(u8"ゲーム内時刻: %s曜日 %02d:%02d", kWeekdayNames[questDeskDay],
            questDeskDaySecond / 3600, (questDeskDaySecond % 3600) / 60);
        if (m_weekResetPending)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"週が更新されました。次回潜行時に奈落が再生成されます。");

        if (NarakuUi::BeginTabBar("QuestTabs"))
        {
            if (ImGui::BeginTabItem(u8"通常クエスト"))
            {
                ImGui::BeginChild("QuestList", ImVec2(0.0f, 160.0f), true);
                for (std::size_t index = 0; index < m_quests.size(); ++index)
                {
                    const QuestRecord& quest = m_quests[index];
                    std::ostringstream label;
                    label << GetQuestName(quest.type) << "##quest" << index;
                    if (ImGui::Selectable(label.str().c_str(), m_selectedQuest == static_cast<int>(index))) m_selectedQuest = static_cast<int>(index);
                    const char* status = quest.status == QuestStatus::Available ? u8"募集中" : quest.status == QuestStatus::Active ? u8"受注中" :
                        quest.status == QuestStatus::Complete ? u8"報告可" : u8"更新待ち";
                    ImGui::TextDisabled(u8"%s", status);
                }
                ImGui::EndChild();

                ImGui::BeginChild("QuestDetail", ImVec2(0.0f, 260.0f), true);
                if (!m_quests.empty())
                {
                    QuestRecord& quest = m_quests[static_cast<std::size_t>(std::max(0, std::min(m_selectedQuest, static_cast<int>(m_quests.size()) - 1)))];
                    ImGui::Text(u8"%s", GetQuestName(quest.type));
                    ImGui::Separator();
                    ImGui::TextWrapped(u8"%s", GetQuestDescription(quest).c_str());
                    ImGui::Text(u8"進捗: %d / %d", quest.progress, quest.targetCount);
                    ImGui::Text(u8"報酬: %dG", quest.reward);
                    if (quest.status == QuestStatus::Active) ImGui::Text(u8"残り期限: %.0f分", std::max(0.0, quest.remainingSeconds) / 60.0);
                    if (quest.status == QuestStatus::Available && ImGui::Button(u8"受注する")) AcceptQuest(static_cast<std::size_t>(m_selectedQuest));
                    if (quest.status == QuestStatus::Complete && ImGui::Button(u8"報告する")) ReportQuest(static_cast<std::size_t>(m_selectedQuest));
                    if (quest.status == QuestStatus::Cooldown) ImGui::TextDisabled(u8"再募集まで %.0f秒", std::max(0.0, quest.cooldownSeconds));
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(u8"重要クエスト"))
            {
                ImGui::BeginChild("ImportantQuestList", ImVec2(0.0f, 160.0f), true);
                for (std::size_t index = 0; index < m_importantQuests.size(); ++index)
                {
                    const ImportantQuestRecord& quest = m_importantQuests[index];
                    if (quest.status == ImportantQuestStatus::Locked) continue;
                    const ImportantQuestType type = static_cast<ImportantQuestType>(index);
                    std::ostringstream label;
                    label << GetImportantQuestName(type) << "##important" << index;
                    if (ImGui::Selectable(label.str().c_str(), m_selectedImportantQuest == static_cast<int>(index)))
                        m_selectedImportantQuest = static_cast<int>(index);
                    ImGui::TextDisabled(u8"%s", quest.status == ImportantQuestStatus::Active ? u8"進行中" :
                        quest.status == ImportantQuestStatus::Complete ? u8"報告可" : u8"達成済み");
                }
                ImGui::EndChild();

                ImGui::BeginChild("ImportantQuestDetail", ImVec2(0.0f, 260.0f), true);
                const std::size_t index = static_cast<std::size_t>(std::max(0, std::min(m_selectedImportantQuest, static_cast<int>(m_importantQuests.size()) - 1)));
                const ImportantQuestRecord& quest = m_importantQuests[index];
                if (quest.status != ImportantQuestStatus::Locked)
                {
                    const ImportantQuestType type = static_cast<ImportantQuestType>(index);
                    int required = 1;
                    if (type == ImportantQuestType::FiveHunts) required = 5;
                    else if (type == ImportantQuestType::ThirteenHunts) required = 13;
                    else if (type == ImportantQuestType::ThreeTerritoryHunts) required = 3;
                    ImGui::TextUnformatted(GetImportantQuestName(type));
                    ImGui::Separator();
                    ImGui::TextWrapped(u8"%s", GetImportantQuestDescription(type));
                    ImGui::Text(u8"進捗: %d / %d", quest.progress, required);
                    ImGui::Text(u8"報酬: %dG / EXP %d / 食料 %d / 水筒 %d",
                        GetImportantQuestMoneyReward(type), GetImportantQuestExpReward(type),
                        GetImportantQuestFoodReward(type), GetImportantQuestWaterBottleReward(type));
                    if (quest.status == ImportantQuestStatus::Complete && ImGui::Button(u8"報告して報酬を受け取る"))
                        ReportImportantQuest(index);
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(u8"緊急クエスト"))
            {
                ImGui::BeginChild("PromotionQuestList", ImVec2(0.0f, 160.0f), true);
                for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
                {
                    const PromotionQuestRecord& quest = m_promotionQuests[index];
                    std::ostringstream label;
                    label << GetPromotionQuestName(index) << "##promotion" << index;
                    if (ImGui::Selectable(label.str().c_str(), m_selectedPromotionQuest == static_cast<int>(index)))
                        m_selectedPromotionQuest = static_cast<int>(index);
                    const char* status = quest.status == PromotionQuestStatus::Locked ? u8"未解放" :
                        quest.status == PromotionQuestStatus::Available ? u8"受注可能" :
                        quest.status == PromotionQuestStatus::Active ? u8"受注中" :
                        quest.status == PromotionQuestStatus::Complete ? u8"報告可" :
                        quest.status == PromotionQuestStatus::FailedThisWeek ? u8"翌週まで受注不可" : u8"合格済み";
                    ImGui::TextDisabled(u8"%s", status);
                }
                ImGui::EndChild();

                ImGui::BeginChild("PromotionQuestDetail", ImVec2(0.0f, 260.0f), true);
                const std::size_t index = static_cast<std::size_t>(std::max(0,
                    std::min(m_selectedPromotionQuest, static_cast<int>(m_promotionQuests.size()) - 1)));
                const PromotionQuestRecord& quest = m_promotionQuests[index];
                ImGui::TextUnformatted(GetPromotionQuestName(index));
                ImGui::Separator();
                ImGui::TextWrapped(u8"適正な実力を持って受け、指定深度から生還すること。");
                ImGui::Text(u8"解放条件: Lv%d以上 / 第%d層へ到達済み",
                    GetPromotionQuestRequiredLevel(index), GetPromotionQuestTargetDepth(index));
                ImGui::Text(u8"達成条件: 第%d層で受注後に取得した遺物を持って生還",
                    GetPromotionQuestTargetDepth(index));
                ImGui::Text(u8"換金用遺物: %d / %d", quest.cashRelicProgress,
                    GetPromotionQuestRequiredCash(index));
                ImGui::Text(u8"強化系遺物: %d / %d", quest.upgradeRelicProgress,
                    GetPromotionQuestRequiredUpgrade(index));
                ImGui::Text(u8"報酬: %dG / EXP %d / 強化系遺物 %d個",
                    GetPromotionQuestMoneyReward(index), GetPromotionQuestExpReward(index),
                    GetPromotionQuestMaterialReward(index));
                ImGui::TextWrapped(u8"対象遺物は報告時に消費されません。通常帰還で不足、死亡、探索放棄の場合は失敗です。");
                if (quest.status == PromotionQuestStatus::Available && ImGui::Button(u8"昇格試験を受注する"))
                    AcceptPromotionQuest(index);
                else if (quest.status == PromotionQuestStatus::Complete && ImGui::Button(u8"報告して昇格する"))
                    ReportPromotionQuest(index);
                else if (quest.status == PromotionQuestStatus::FailedThisWeek)
                    ImGui::TextDisabled(u8"次の週の開始時に再受注可能になります。");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if (m_questDebt > 0 && CountStoredRelics(RelicType::Unique) > 0 && ImGui::Button(u8"欲望の揺籃を1個消費して借金1000Gを返済"))
        {
            RemoveStoredRelics(RelicType::Unique, 1);
            m_questDebt = std::max(0, m_questDebt - 1000);
            SaveProgress();
        }
    });
}

void SceneNarakuProto::DrawRouteInfo()
{
    for (const LayerGateState& gate : m_layerGates)
    {
        if (gate.isEntry || !IsNear(m_player.pos, gate.loadPos, 3.0f)) continue;
        ImGui::SetNextWindowPos(ImVec2(840.0f, 20.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_Always);
        NarakuUi::Begin(u8"ルート傾向", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
        if (gate.destinationAreaIndex < 0 || gate.destinationAreaIndex >= static_cast<int>(m_areas.size()) ||
            !m_areas[gate.destinationAreaIndex].generated)
        {
            ImGui::TextWrapped(u8"Fでルートを調査します。調査後、進入前に傾向を確認できます。");
        }
        else
        {
            const AreaState& area = m_areas[gate.destinationAreaIndex];
            ImGui::Text(u8"接続先: 第%d層（%s） エリア%d", area.depth,
                GetSublayerName(area.sublayer), area.areaNumber);
            const DepthRules& rules = GetRulesForDepth(area.depth);
            const int enemyMaximum = rules.chargerMax + rules.territoryMax;
            const int miningMaximum = static_cast<int>(area.map.pieceNames.size()) * 5;
            if (static_cast<int>(area.enemies.size()) == enemyMaximum) ImGui::BulletText(u8"敵が多め");
            if (!area.miningPoints.empty() && static_cast<int>(area.miningPoints.size()) > miningMaximum - 5) ImGui::BulletText(u8"採掘地点が多め");
            const bool hasCliff = std::any_of(area.map.terrainLayers.begin(), area.map.terrainLayers.end(), [](const NarakuMap::TerrainLayer& layer)
            {
                return std::any_of(layer.cellAttributeFlags.begin(), layer.cellAttributeFlags.end(), [](std::uint32_t flags)
                { return (flags & NarakuMap::CellAttributeCliffEdge) != 0u; });
            });
            if (hasCliff) ImGui::BulletText(u8"崖あり");
            if (std::any_of(area.enemies.begin(), area.enemies.end(), [](const EnemyState& enemy) { return enemy.type == EnemyType::Territory; }))
                ImGui::BulletText(u8"縄張り型あり");
            if (area.enemies.empty() && area.miningPoints.empty() && !hasCliff) ImGui::TextDisabled(u8"目立った傾向なし");
            const int enemyRequired = static_cast<int>(std::ceil(static_cast<float>(area.enemies.size()) * 0.75f));
            const int miningRequired = static_cast<int>(std::ceil(static_cast<float>(area.miningPoints.size()) * 0.75f));
            if (area.discoveredEnemyCount >= enemyRequired)
                ImGui::Text(u8"記録済みの敵: %d体", static_cast<int>(area.enemies.size()));
            if (area.discoveredMiningCount >= miningRequired)
                ImGui::Text(u8"記録済みの採掘地点: %d箇所", static_cast<int>(area.miningPoints.size()));
            if (area.discoveredCliffCount > 0) ImGui::Text(u8"視認済みの崖あり");
            ImGui::Separator();
            ImGui::Text(u8"もう一度Fで進入");
        }
        ImGui::End();
        break;
    }
}

void SceneNarakuProto::DrawMapControls()
{
    // マップウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(690.0f, 80.0f), ImGuiCond_FirstUseEver);

    // マップウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);

    // マップウィンドウを開始します。
    NarakuUi::Begin(u8"地図", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(u8"← [LB] 設定", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Settings; m_inputSettings.OnOpen(); }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), u8"【 地図 】");
    ImGui::SameLine();
    if (ImGui::Button(u8"所持品 [RB] →", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Inventory; m_inventoryMapShowingMap = false; }
    ImGui::SameLine();
    ImGui::TextDisabled(u8" (LB/RB 切替, [B/Esc] 閉じる)");
    if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
    {
        const AreaState& currentArea = m_areas[m_currentAreaIndex];
        ImGui::Text(u8"現在深度: 第%d層（%s） エリア%d", currentArea.depth,
            GetSublayerName(currentArea.sublayer), currentArea.areaNumber);
    }
    else ImGui::TextUnformatted(u8"現在地: 地上");
    static const char* kWeekdayNames[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
    const int mapWeekSecond = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
    const int mapDay = mapWeekSecond / static_cast<int>(kGameDaySeconds);
    const int mapDaySecond = mapWeekSecond % static_cast<int>(kGameDaySeconds);
    ImGui::Text(u8"ゲーム内時刻: %s曜日 %02d:%02d", kWeekdayNames[mapDay],
        mapDaySecond / 3600, (mapDaySecond % 3600) / 60);
    if (m_weekResetPending)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"次回潜行時に新しい週のマップへ更新されます。");
    ImGui::Separator();

    if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
    {
        const AreaState& area = m_areas[m_currentAreaIndex];
        const int enemyRequired = static_cast<int>(std::ceil(static_cast<float>(m_enemies.size()) * 0.75f));
        const int miningRequired = static_cast<int>(std::ceil(static_cast<float>(m_miningPoints.size()) * 0.75f));
        ImGui::Text(u8"情報開示  敵:%d/%d  採掘:%d/%d", area.discoveredEnemyCount, enemyRequired,
            area.discoveredMiningCount, miningRequired);
        if (area.discoveredEnemyCount >= enemyRequired)
            ImGui::Text(u8"敵情報: 突進型%d / 縄張り型%d",
                static_cast<int>(std::count_if(m_enemies.begin(), m_enemies.end(), [](const EnemyState& enemy) { return enemy.type == EnemyType::Charger; })),
                static_cast<int>(std::count_if(m_enemies.begin(), m_enemies.end(), [](const EnemyState& enemy) { return enemy.type == EnemyType::Territory; })));
        if (area.discoveredMiningCount >= miningRequired) ImGui::Text(u8"採掘地点: %d", static_cast<int>(m_miningPoints.size()));
        if (area.discoveredCliffCount > 0) ImGui::Text(u8"崖: あり");
        ImGui::Separator();
    }

    // 拡大縮小操作のUIを追加します。
    if (ImGui::Button("-"))
    {
        m_mapZoom = std::max(0.5f, m_mapZoom - 0.25f);
    }
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        m_mapZoom = std::min(5.0f, m_mapZoom + 0.25f);
    }
    ImGui::SameLine();
    ImGui::Text("Zoom %.2fx", m_mapZoom);
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        m_mapZoom = 2.0f;
        m_mapScrollOffset = { 0.0f, 0.0f };
    }

    // ミニマップ描画領域の左上座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // ミニマップ描画領域のサイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 460.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const Vec2 mapFocus = { m_player.pos.x + m_mapScrollOffset.x, m_player.pos.y + m_mapScrollOffset.y };

    // ミニマップ背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 28, 28, 255));

    // キャンバス外へのはみ出しを防ぐため、クリッピングを設定します。
    draw->PushClipRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    // 1. 地形セルの描画 (プレイヤー現在深度付近のレイヤーのみ)
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2) continue;
        if (std::fabs(layer.layerDepth - m_player.depth) > 0.5f) continue;

        const float halfWidth = (layer.gridWidth - 1) * layer.cellSize * 0.5f;
        const float halfHeight = (layer.gridHeight - 1) * layer.cellSize * 0.5f;
        const float minX = layer.center.x - halfWidth;
        const float minZ = layer.center.z - halfHeight;

        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                if (flags & NarakuMap::CellAttributeRemoved) continue;

                Vec2 c00 = { minX + cellX * layer.cellSize, minZ + cellZ * layer.cellSize };
                Vec2 c11 = { c00.x + layer.cellSize, c00.y + layer.cellSize };

                Vec2 p00 = WorldToCanvas(canvasPos, canvasSize, c00, m_mapZoom, mapFocus);
                Vec2 p11 = WorldToCanvas(canvasPos, canvasSize, c11, m_mapZoom, mapFocus);

                ImU32 color = IM_COL32(35, 55, 45, 200); // 通常歩行可能
                if (flags & NarakuMap::CellAttributeBlocked)
                {
                    color = IM_COL32(95, 38, 38, 220); // 通行不可（赤系）
                }
                else if (flags & NarakuMap::CellAttributeCliffEdge)
                {
                    color = IM_COL32(85, 75, 40, 200); // 崖端（黄系）
                }
                else if (flags & NarakuMap::CellAttributeHazard)
                {
                    color = IM_COL32(100, 38, 100, 200); // 危険地形（紫系）
                }

                draw->AddRectFilled(ImVec2(p00.x, p00.y), ImVec2(p11.x, p11.y), color);
                draw->AddRect(ImVec2(p00.x, p00.y), ImVec2(p11.x, p11.y), IM_COL32(30, 36, 36, 80));
            }
        }
    }

    // 2. 帰還地点の描画
    if (m_overlayReturnMode != Mode::Surface)
    {
        Vec2 ret = WorldToCanvas(canvasPos, canvasSize, m_returnPoint, m_mapZoom, mapFocus);
        draw->AddCircleFilled(ImVec2(ret.x, ret.y), 6.0f, IM_COL32(80, 180, 255, 255));
    }

    // 3. 採掘ポイントの描画 (採掘済み、または発見済みのみ)
    for (const MiningPoint& point : m_miningPoints)
    {
        if (!(point.mined || point.discovered || point.sensed))
        {
            continue;
        }

        Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, mapFocus);
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.5f, color);
    }

    for (const FishingPoint& point : m_fishingPoints)
    {
        if (!point.discovered || std::fabs(point.depth - m_player.depth) > 0.5f) continue;
        const Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, mapFocus);
        const ImU32 color = point.remainingUses > 0 ? IM_COL32(55, 175, 235, 255) : IM_COL32(95, 95, 95, 255);
        draw->AddCircle(ImVec2(p.x, p.y), 6.0f, color, 12, 2.0f);
        draw->AddLine(ImVec2(p.x - 4.0f, p.y), ImVec2(p.x + 4.0f, p.y), color, 1.5f);
    }

    // 4. マップピンの描画
    for (const Vec2& pin : m_pins)
    {
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin, m_mapZoom, mapFocus);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, IM_COL32(230, 80, 90, 255));
    }

    // 5. プレイヤー位置と向きの描画
    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos, m_mapZoom, mapFocus);
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));
    Vec2 faceEnd = WorldToCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 3.0f / m_mapZoom)), m_mapZoom, mapFocus);
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 1.5f);

    // クリッピングを終了します。
    draw->PopClipRect();

    // ミニマップ外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 145, 145, 255));

    // マウスがミニマップ領域内にあるか調べます。
    bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));

    // 右クリックの長押しドラッグによるマップ移動
    static bool s_isDraggingMap = false;
    if (ImGui::IsMouseClicked(1) && hovered)
    {
        s_isDraggingMap = true;
    }

    if (m_overlayReturnMode == Mode::Surface)
    {
        for (const SurfaceFacilityState& facility : m_surfaceFacilities)
        {
            const Vec2 p = WorldToCanvas(canvasPos, canvasSize, facility.center, m_mapZoom, mapFocus);
            draw->AddRectFilled(ImVec2(p.x - 6.0f, p.y - 6.0f), ImVec2(p.x + 6.0f, p.y + 6.0f),
                facility.type == NarakuPiece::SurfaceFacilityType::AbyssEntrance
                    ? IM_COL32(190, 80, 230, 255) : IM_COL32(80, 220, 140, 255));
            if (ImGui::IsMouseHoveringRect(ImVec2(p.x - 8.0f, p.y - 8.0f), ImVec2(p.x + 8.0f, p.y + 8.0f)))
            {
                const char* description = u8"施設";
                if (facility.type == NarakuPiece::SurfaceFacilityType::Home) description = u8"自宅：装備・在庫・次回持ち込みを管理";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::Shop) description = u8"商店：消耗品と強化素材の売買";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::Armory) description = u8"武具屋：武器と防具の購入";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk) description = u8"レストラン・受付：回復と依頼の管理";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::AbyssEntrance) description = u8"奈落塔出入口：通常潜行と直通門";
                ImGui::SetTooltip(u8"%s", description);
            }
        }
    }
    if (!ImGui::IsMouseDown(1))
    {
        s_isDraggingMap = false;
    }

    if (s_isDraggingMap)
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * m_mapZoom;
            float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * m_mapZoom;
            if (std::fabs(scaleX) > 0.001f && std::fabs(scaleY) > 0.001f)
            {
                m_mapScrollOffset.x -= delta.x / scaleX;
                m_mapScrollOffset.y += delta.y / scaleY;
            }
        }
    }

    // マウスホイールによるスクロール拡縮
    if (hovered)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            m_mapZoom = std::max(0.5f, std::min(5.0f, m_mapZoom + wheel * 0.25f));
        }
    }

    // ミニマップ上で左クリックされたらピン設置/削除を行います。
    if (hovered && ImGui::IsMouseClicked(0))
    {
        // 現在のマウス座標を取得します。
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // スクリーン座標をワールド座標へ変換してピン操作します。
        TogglePinAt(ScreenToWorld(canvasPos, canvasSize, { mouse.x, mouse.y }, m_mapZoom, mapFocus));
    }

    // ミニマップ描画領域ぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    float mapScrollX = 0.0f, mapScrollY = 0.0f;
    GetActionMapScroll(mapScrollX, mapScrollY);
    if (std::abs(mapScrollX) > 0.01f || std::abs(mapScrollY) > 0.01f)
    {
        const float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * m_mapZoom;
        const float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * m_mapZoom;
        if (std::fabs(scaleX) > 0.001f && std::fabs(scaleY) > 0.001f)
        {
            m_mapScrollOffset.x += (mapScrollX * 15.0f) / scaleX;
            m_mapScrollOffset.y += (mapScrollY * 15.0f) / scaleY;
        }
    }
    const float padMapZoom = GetActionMapZoom();
    if (std::abs(padMapZoom) > 0.01f)
    {
        m_mapZoom = std::max(0.5f, std::min(5.0f, m_mapZoom + padMapZoom * 0.05f));
    }
    if (IsActionMapPinTrigger())
    {
        TogglePinAt(mapFocus);
    }
    if (IsActionMapFocusPlayerTrigger())
    {
        m_mapScrollOffset = { 0.0f, 0.0f };
    }

    // ピン操作説明を表示します。
    ImGui::Text(u8"右ドラッグ: マップ移動   左クリック: ピン設置/削除   ホイール: 拡大縮小");

    // マップピンウィンドウを閉じます。
    ImGui::End();
}
