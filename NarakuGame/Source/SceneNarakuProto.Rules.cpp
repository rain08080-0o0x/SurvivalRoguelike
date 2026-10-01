/**
 * @file SceneNarakuProto.Rules.cpp
 * @brief アイテム、装備、能力値、地形判定、釣り、および座標変換を実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

const char* SceneNarakuProto::GetRelicTypeName(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade: return u8"武具強化遺物";
    case RelicType::WeaponUpgrade: return u8"武器強化遺物";
    case RelicType::ArmorUpgrade: return u8"装備強化遺物";
    case RelicType::Offensive: return u8"攻撃的遺物";
    case RelicType::Survival: return u8"生存的遺物";
    case RelicType::CashLow: return u8"換金用遺物（低）";
    case RelicType::CashHigh: return u8"換金用遺物（高）";
    case RelicType::MentalRecovery: return u8"精神力回復遺物";
    case RelicType::Unique: return u8"欲望の揺籃";
    default: return u8"不明な遺物";
    }
}

const char* SceneNarakuProto::GetRelicDisplayName(const RelicItem& item) const
{
    const std::size_t index = static_cast<std::size_t>(item.type);
    return index < m_identifiedRelics.size() && m_identifiedRelics[index]
        ? GetRelicTypeName(item.type)
        : u8"未鑑定の遺物";
}

float SceneNarakuProto::GetRelicWeight(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade:
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade: return 5.0f;
    case RelicType::Offensive: return 8.0f;
    case RelicType::Survival: return 3.0f;
    case RelicType::CashLow: return 2.0f;
    case RelicType::CashHigh: return 10.0f;
    case RelicType::MentalRecovery: return 2.0f;
    case RelicType::Unique: return 100.0f;
    default: return 0.0f;
    }
}

int SceneNarakuProto::GetRelicSellValue(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade: return 60;
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade: return 70;
    case RelicType::Offensive: return 0;
    case RelicType::Survival: return 60;
    case RelicType::CashLow: return 5;
    case RelicType::CashHigh: return 60;
    case RelicType::MentalRecovery: return 5;
    case RelicType::Unique: return 0;
    default: return 0;
    }
}

SceneNarakuProto::RelicItem SceneNarakuProto::CreateRelic(RelicType type, const std::string& sourceName)
{
    RelicItem item;
    item.name = type == RelicType::Unique ? u8"欲望の揺籃" : sourceName;
    item.type = type;
    item.weight = GetRelicWeight(type);
    item.value = GetRelicSellValue(type);
    item.acquisitionOrder = m_nextRelicAcquisitionOrder++;
    if (type == RelicType::Offensive)
    {
        item.maxUses = RandomInt(15, 20);
        item.remainingUses = item.maxUses;
    }
    return item;
}

SceneNarakuProto::RelicItem SceneNarakuProto::CreateRandomRelic(const std::string& sourceName)
{
    const std::array<int, 5>& weights = GetRulesForDepth(GetCurrentDepth()).dropWeights;
    const int roll = RandomInt(1, 100);
    int cumulative = weights[0];
    if (roll <= cumulative) return CreateRelic(RelicType::CashLow, sourceName);
    cumulative += weights[1];
    if (roll <= cumulative) return CreateRelic(RelicType::CashHigh, sourceName);
    cumulative += weights[2];
    if (roll <= cumulative)
    {
        return CreateRelic(static_cast<RelicType>(RandomInt(
            static_cast<int>(RelicType::ArmamentUpgrade), static_cast<int>(RelicType::ArmorUpgrade))), sourceName);
    }
    cumulative += weights[3];
    if (roll <= cumulative)
    {
        const RelicType specials[] = { RelicType::Offensive, RelicType::Survival, RelicType::MentalRecovery };
        return CreateRelic(specials[RandomInt(0, 2)], sourceName);
    }
    return CreateRelic(RelicType::Unique, u8"欲望の揺籃");
}

int SceneNarakuProto::GetRelicActivity(const RelicItem& item) const
{
    if (item.stabilized || item.broken) return 0;
    switch (item.type)
    {
    case RelicType::CashLow: return 1;
    case RelicType::CashHigh: return 3;
    case RelicType::ArmamentUpgrade:
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade:
    case RelicType::MentalRecovery: return 2;
    case RelicType::Offensive:
    case RelicType::Survival: return 4;
    case RelicType::Unique: return 8;
    default: return 0;
    }
}

int SceneNarakuProto::GetCurrentActivity() const
{
    int activity = 0;
    for (const RelicItem& item : m_inventory) activity += GetRelicActivity(item);
    return activity;
}

bool SceneNarakuProto::IsRelicSellable(const RelicItem& item) const
{
    if (item.type == RelicType::Unique) return false;
    if (item.type == RelicType::Offensive && !item.broken) return false;
    return item.value > 0;
}

bool SceneNarakuProto::UseMentalRecoveryRelic(int inventoryIndex)
{
    if (inventoryIndex < 0 || inventoryIndex >= static_cast<int>(m_inventory.size())) return false;
    const RelicItem& item = m_inventory[inventoryIndex];
    if (item.type != RelicType::MentalRecovery || item.broken || m_player.mental >= GetMaxMental()) return false;
    m_player.mental = std::min(GetMaxMental(), m_player.mental + 10.0f * GetMentalRecoveryMultiplier());
    m_inventory.erase(m_inventory.begin() + inventoryIndex);
    AddMessage(u8"精神力回復遺物を使用しました。");
    return true;
}

bool SceneNarakuProto::TryConsumeSurvivalRelic(bool hpLethal, bool mentalLethal)
{
    for (RelicItem& item : m_inventory)
    {
        if (item.type != RelicType::Survival || item.broken || !item.autoTrigger) continue;
        item.broken = true;
        item.value = 5;
        if (hpLethal) m_player.hp = 1.0f;
        if (mentalLethal) m_player.mental = 1.0f;
        AddMessage(u8"生存的遺物が致命傷を防ぎ、破損しました。");
        return true;
    }
    return false;
}

void SceneNarakuProto::UseFood()
{
    if (m_foodCount <= 0)
    {
        AddMessage(u8"食料を持っていません。");
        ShowCenterNotification(u8"食料がない！");
        return;
    }
    if (m_foodUseTimer > 0.0f || m_cookingTarget != CookingTarget::None) return;
    if (m_player.hp >= GetMaxHp() && m_fullness >= kFullnessMaximum && m_hydration >= kHydrationMaximum)
    {
        AddMessage(u8"体力、満腹度、水分が満タンのため食料を使いませんでした。");
        return;
    }
    m_usingHeatedFood = false;
    m_foodUseTimer = 0.5f;
    AddMessage(u8"食料を食べ始めました。");
}

void SceneNarakuProto::UseHeatedFood()
{
    if (m_heatedFoodCount <= 0)
    {
        AddMessage(u8"加熱食料を持っていません。");
        return;
    }
    if (m_foodUseTimer > 0.0f || m_cookingTarget != CookingTarget::None) return;
    if (m_player.hp >= GetMaxHp() && m_fullness >= kFullnessMaximum &&
        m_hydration >= kHydrationMaximum && m_player.mental >= GetMaxMental())
    {
        AddMessage(u8"回復対象が満タンのため加熱食料を使いませんでした。");
        return;
    }
    m_usingHeatedFood = true;
    m_foodUseTimer = 0.5f;
    AddMessage(u8"加熱食料を食べ始めました。");
}

void SceneNarakuProto::UseRationOne()
{
    if (m_rationOneCount <= 0) return;
    --m_rationOneCount;
    m_fullness = kFullnessMaximum;
    m_rationFullnessWardTimer = kRationFullnessWardDuration;
    m_rationHydrationPenaltyTimer = kRationHydrationPenaltyDuration;
    AddMessage(u8"行動食1号を使用しました。5分間満腹度が減少しません。");
}

void SceneNarakuProto::UseRawFish()
{
    if (m_rawFishCount <= 0 ||
        (m_fullness >= kFullnessMaximum && m_player.mental >= GetMaxMental())) return;
    --m_rawFishCount;
    m_fullness = std::min(kFullnessMaximum, m_fullness + 10.0f);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + 5.0f);
    AddMessage(u8"見たことない魚を生で食べ、満腹度10、精神力5を回復しました。");
}

void SceneNarakuProto::UseCookedFish()
{
    if (m_cookedFishCount <= 0 ||
        (m_fullness >= kFullnessMaximum && m_player.mental >= GetMaxMental())) return;
    --m_cookedFishCount;
    m_fullness = std::min(kFullnessMaximum, m_fullness + 50.0f);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + 25.0f);
    AddMessage(u8"調理済みの魚を食べ、満腹度50、精神力25を回復しました。");
}

const char* SceneNarakuProto::GetWaterQualityName(WaterQuality quality) const
{
    switch (quality)
    {
    case WaterQuality::Safe: return u8"安全";
    case WaterQuality::Unboiled: return u8"未煮沸";
    case WaterQuality::Boiled: return u8"煮沸済";
    default: return u8"なし";
    }
}

void SceneNarakuProto::DrinkWater(float amount, WaterQuality quality, float foodPoisoningChance)
{
    if (m_hydration >= kHydrationMaximum || amount <= 0.0f) return;
    m_hydration = std::min(kHydrationMaximum, m_hydration + amount);
    if (quality == WaterQuality::Unboiled) ApplyFoodPoisoning(foodPoisoningChance);
    AddMessage(u8"水分を25回復しました。");
}

void SceneNarakuProto::ApplyFoodPoisoning(float chance)
{
    if (chance <= 0.0f || RandomFloat(0.0f, 1.0f) >= chance) return;
    const float damage = m_player.hp * 0.10f;
    m_player.hp = std::max(0.0f, m_player.hp - damage);
    AddMessage(u8"食中毒が発生し、現在HPの10%を失いました。");
}

void SceneNarakuProto::DrinkFromBottle(int bottleIndex)
{
    if (bottleIndex < 0 || bottleIndex >= static_cast<int>(m_waterBottles.size()) ||
        m_hydration >= kHydrationMaximum || m_cookingTarget != CookingTarget::None) return;
    WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(bottleIndex)];
    if (bottle.amount <= 0.0f) return;
    const float consumed = std::min(kWaterDrinkAmount, bottle.amount);
    const WaterQuality quality = bottle.quality;
    const float poisoningChance = bottle.foodPoisoningChance;
    bottle.amount -= consumed;
    if (bottle.amount <= 0.0f)
    {
        bottle.amount = 0.0f;
        bottle.quality = WaterQuality::None;
        bottle.foodPoisoningChance = 0.0f;
    }
    DrinkWater(consumed, quality, poisoningChance);
}

void SceneNarakuProto::DiscardBottleWater(int bottleIndex)
{
    if (bottleIndex < 0 || bottleIndex >= static_cast<int>(m_waterBottles.size()) ||
        m_cookingTarget != CookingTarget::None) return;
    WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(bottleIndex)];
    bottle.amount = 0.0f;
    bottle.foodPoisoningChance = 0.0f;
    bottle.quality = WaterQuality::None;
    AddMessage(u8"水筒の中身を捨てました。");
}

bool SceneNarakuProto::ConsumeCookingKitUse()
{
    if (m_cookingKits.empty()) return false;
    CookingKit& kit = m_cookingKits.front();
    --kit.remainingUses;
    if (kit.remainingUses <= 0) m_cookingKits.erase(m_cookingKits.begin());
    return true;
}

void SceneNarakuProto::StartCooking(CookingTarget target, int bottleIndex)
{
    if (target == CookingTarget::None || m_cookingTarget != CookingTarget::None) return;
    if (!m_player.grounded || m_player.onRope)
    {
        AddMessage(u8"歩行可能な足場でなければ料理セットを使用できません。");
        return;
    }
    if (target == CookingTarget::HeatFood && m_foodCount <= 0) return;
    if (target == CookingTarget::CookFish && m_rawFishCount <= 0) return;
    if (target == CookingTarget::CookSizedFish &&
        (m_cookingFishSize < 0 || m_cookingFishSize >= 3 || m_rawSizedFish[static_cast<size_t>(m_cookingFishSize)] <= 0)) return;
    if (target == CookingTarget::BoilBottle)
    {
        if (bottleIndex < 0 || bottleIndex >= static_cast<int>(m_waterBottles.size())) return;
        const WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(bottleIndex)];
        if (bottle.amount <= 0.0f || bottle.quality != WaterQuality::Unboiled) return;
    }
    if (!ConsumeCookingKitUse())
    {
        AddMessage(u8"料理セットを持っていません。");
        return;
    }
    m_cookingTarget = target;
    m_cookingBottleIndex = bottleIndex;
    m_cookingTimer = kCookingDuration;
    m_cookingPreviousHp = m_player.hp;
    m_mode = Mode::Explore;
    if (target == CookingTarget::BoilBottle)
        AddMessage(u8"水の煮沸を開始しました。60秒間その場を維持してください。");
    else if (target == CookingTarget::CookFish || target == CookingTarget::CookSizedFish)
        AddMessage(u8"魚の調理を開始しました。60秒間その場を維持してください。");
    else
        AddMessage(u8"食料の加熱を開始しました。60秒間その場を維持してください。");
}

void SceneNarakuProto::CancelCooking(const char* reason)
{
    if (m_cookingTarget == CookingTarget::None) return;
    m_cookingTarget = CookingTarget::None;
    m_cookingBottleIndex = -1;
    m_cookingFishSize = -1;
    m_cookingTimer = 0.0f;
    AddMessage(reason);
}

float SceneNarakuProto::GetWaterFoodPoisoningChance() const
{
    switch (GetCurrentDepth())
    {
    case 1: return 0.10f;
    case 2: return 0.30f;
    case 4: return 0.80f;
    default: return 0.0f;
    }
}

std::uint32_t SceneNarakuProto::GetNearbyWaterFlags() const
{
    constexpr std::uint32_t waterMask = NarakuMap::CellAttributeWaterPuddle |
        NarakuMap::CellAttributeWaterPond | NarakuMap::CellAttributeWaterLake;
    const std::uint32_t current = GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & waterMask;
    if (current != 0u) return current;

    const Vec2 offsets[] = {
        { kInteractRange, 0.0f }, { -kInteractRange, 0.0f },
        { 0.0f, kInteractRange }, { 0.0f, -kInteractRange }
    };
    for (const Vec2& offset : offsets)
    {
        const std::uint32_t flags = GetCellAttributeFlagsAt(Add(m_player.pos, offset), m_player.depth) & waterMask;
        if (flags != 0u) return flags;
    }
    return NarakuMap::CellAttributeNone;
}

bool SceneNarakuProto::TryUseRestaurant()
{
    if (m_money < kRestaurantPrice)
    {
        ShowCenterNotification(u8"所持金が足りません。");
        return false;
    }
    if (m_fullness >= kFullnessMaximum && m_hydration >= kHydrationMaximum &&
        m_player.hp >= GetMaxHp() && m_player.mental >= GetMaxMental())
    {
        ShowCenterNotification(u8"今は食事をする必要がありません。");
        return false;
    }
    m_money -= kRestaurantPrice;
    m_fullness = kFullnessMaximum;
    m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
    m_player.hp = std::min(GetMaxHp(), m_player.hp + GetMaxHp() * kRestaurantHpRatio);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + GetMaxMental() * kRestaurantMentalRatio);
    SaveProgress();
    ShowCenterNotification(u8"食事で満腹度、HP、精神力、水分を回復しました。");
    return true;
}

const char* SceneNarakuProto::GetArmorName(ArmorTier tier) const
{
    switch (tier)
    {
    case ArmorTier::Leather: return u8"革装備";
    case ArmorTier::Iron: return u8"鉄装備";
    case ArmorTier::RelicCovered: return u8"遺物で覆われたシリーズ";
    case ArmorTier::RelicHardened: return u8"遺物で固めたシリーズ";
    case ArmorTier::RelicEnhanced: return u8"遺物で強化されたシリーズ";
    case ArmorTier::Relic: return u8"遺物装備";
    case ArmorTier::Unknown: return u8"未知の装備";
    case ArmorTier::None: return u8"装備なし";
    default: return u8"不明な装備";
    }
}

const char* SceneNarakuProto::GetArmorEffectText(ArmorTier tier, bool headSlot) const
{
    switch (tier)
    {
    case ArmorTier::Leather: return headSlot ? u8"防御力+4%" : u8"防御力+6%";
    case ArmorTier::Iron: return headSlot ? u8"防御力+10%" : u8"防御力+15%";
    case ArmorTier::RelicCovered: return headSlot ? u8"攻撃力+4% / 防御力+16%" : u8"攻撃力+6% / 防御力+24%";
    case ArmorTier::RelicHardened: return headSlot ? u8"防御力+32%" : u8"防御力+48%";
    case ArmorTier::RelicEnhanced: return headSlot ? u8"攻撃力+10% / 防御力+24%" : u8"攻撃力+15% / 防御力+36%";
    case ArmorTier::Relic: return headSlot ? u8"攻撃力+10% / 防御力+30%" : u8"攻撃力+15% / 防御力+45%";
    case ArmorTier::Unknown: return u8"頭・胴セットで専用効果発動";
    default: return u8"効果なし";
    }
}

bool SceneNarakuProto::HasRelicArmorSetEffect() const
{
    return m_equippedHeadArmor == ArmorTier::Relic &&
        m_equippedBodyArmor == ArmorTier::Relic;
}

bool SceneNarakuProto::HasUnknownArmorSetEffect() const
{
    return m_equippedHeadArmor == ArmorTier::Unknown &&
        m_equippedBodyArmor == ArmorTier::Unknown;
}

const char* SceneNarakuProto::GetWeaponName(WeaponTier tier) const
{
    switch (tier)
    {
    case WeaponTier::RustyPickaxe: return u8"錆びれたつるはし";
    case WeaponTier::NormalPickaxe: return u8"普通のつるはし";
    case WeaponTier::SturdyPickaxe: return u8"丈夫なつるはし";
    case WeaponTier::SharpPickaxe: return u8"鋭利なつるはし";
    case WeaponTier::RelicPickaxe: return u8"遺物付きのつるはし";
    case WeaponTier::Unknown: return u8"未知の武器";
    case WeaponTier::None: return u8"武器なし";
    default: return u8"不明な武器";
    }
}

const char* SceneNarakuProto::GetWeaponEffectText(WeaponTier tier) const
{
    switch (tier)
    {
    case WeaponTier::RustyPickaxe: return u8"攻撃力+0% / 採掘速度+0%";
    case WeaponTier::NormalPickaxe: return u8"攻撃力+0% / 採掘速度+35%";
    case WeaponTier::SturdyPickaxe: return u8"攻撃力+0% / 採掘速度+80%";
    case WeaponTier::SharpPickaxe: return u8"攻撃力+0% / 採掘速度+100%";
    case WeaponTier::RelicPickaxe: return u8"攻撃力+0% / 採掘速度+150%";
    case WeaponTier::Unknown: return u8"1秒溜め / 前方直線即死 / CT5秒";
    default: return u8"効果なし";
    }
}

bool SceneNarakuProto::TryBuyArmor(ArmorTier tier, bool headSlot, bool useMaterials)
{
    static const int materialPrices[][2] = { { 10, 15 }, { 150, 200 }, { 200, 300 }, { 250, 350 }, { 350, 400 }, { 5000, 6000 } };
    static const int moneyPrices[][2] = { { 10, 15 }, { 150, 200 }, { 1000, 1500 }, { 1250, 1750 }, { 1750, 2000 }, { 25000, 30000 } };
    static const int armamentCosts[][2] = { { 0, 0 }, { 0, 0 }, { 3, 4 }, { 4, 6 }, { 7, 10 }, { 17, 19 } };
    static const int armorCosts[][2] = { { 0, 0 }, { 0, 0 }, { 5, 7 }, { 7, 9 }, { 5, 7 }, { 15, 19 } };
    const std::size_t tierIndex = static_cast<std::size_t>(tier);
    const int slotIndex = headSlot ? 0 : 1;
    std::array<bool, static_cast<std::size_t>(ArmorTier::Count)>& owned = headSlot ? m_ownedHeadArmor : m_ownedBodyArmor;
    if (owned[tierIndex]) return false;

    const int price = useMaterials ? materialPrices[tierIndex][slotIndex] : moneyPrices[tierIndex][slotIndex];
    const std::size_t armamentIndex = static_cast<std::size_t>(RelicType::ArmamentUpgrade);
    const std::size_t armorIndex = static_cast<std::size_t>(RelicType::ArmorUpgrade);
    if (m_money < price || (useMaterials && (CountStoredRelics(RelicType::ArmamentUpgrade) < armamentCosts[tierIndex][slotIndex] ||
        CountStoredRelics(RelicType::ArmorUpgrade) < armorCosts[tierIndex][slotIndex])))
    {
        AddMessage(u8"購入に必要な金額または遺物が不足しています。");
        return false;
    }

    m_money -= price;
    if (useMaterials)
    {
        RemoveStoredRelics(RelicType::ArmamentUpgrade, armamentCosts[tierIndex][slotIndex]);
        RemoveStoredRelics(RelicType::ArmorUpgrade, armorCosts[tierIndex][slotIndex]);
        m_loadoutRelics[armamentIndex] = std::min(m_loadoutRelics[armamentIndex], CountStoredRelics(RelicType::ArmamentUpgrade));
        m_loadoutRelics[armorIndex] = std::min(m_loadoutRelics[armorIndex], CountStoredRelics(RelicType::ArmorUpgrade));
    }
    owned[tierIndex] = true;
    AddMessage(u8"装備を購入しました。");
    SaveProgress();
    return true;
}

bool SceneNarakuProto::TryBuyWeapon(WeaponTier tier, bool useMaterials)
{
    static const int materialPrices[] = { 5, 500, 1250, 1500, 5000 };
    static const int moneyPrices[] = { 5, 500, 1250, 1500, 25000 };
    static const int armamentCosts[] = { 0, 0, 0, 0, 11 };
    static const int weaponCosts[] = { 0, 0, 0, 0, 21 };
    const std::size_t tierIndex = static_cast<std::size_t>(tier);
    if (m_ownedWeapons[tierIndex]) return false;

    const int price = useMaterials ? materialPrices[tierIndex] : moneyPrices[tierIndex];
    const std::size_t armamentIndex = static_cast<std::size_t>(RelicType::ArmamentUpgrade);
    const std::size_t weaponIndex = static_cast<std::size_t>(RelicType::WeaponUpgrade);
    if (m_money < price || (useMaterials && (CountStoredRelics(RelicType::ArmamentUpgrade) < armamentCosts[tierIndex] ||
        CountStoredRelics(RelicType::WeaponUpgrade) < weaponCosts[tierIndex])))
    {
        AddMessage(u8"購入に必要な金額または遺物が不足しています。");
        return false;
    }

    m_money -= price;
    if (useMaterials)
    {
        RemoveStoredRelics(RelicType::ArmamentUpgrade, armamentCosts[tierIndex]);
        RemoveStoredRelics(RelicType::WeaponUpgrade, weaponCosts[tierIndex]);
        m_loadoutRelics[armamentIndex] = std::min(m_loadoutRelics[armamentIndex], CountStoredRelics(RelicType::ArmamentUpgrade));
        m_loadoutRelics[weaponIndex] = std::min(m_loadoutRelics[weaponIndex], CountStoredRelics(RelicType::WeaponUpgrade));
    }
    m_ownedWeapons[tierIndex] = true;
    AddMessage(u8"武器を購入しました。");
    SaveProgress();
    return true;
}

float SceneNarakuProto::GetCurrentWeight() const
{
    // 初期装備のつるはし重量10から計算を始めます。
    float weight = 10.0f;

    // 所持している旧器の重量をすべて足します。
    for (const RelicItem& item : m_inventory) weight += item.weight;

    // 食料は1個につき重量1として扱います。
    weight += static_cast<float>(m_foodCount);
    weight += static_cast<float>(m_heatedFoodCount);
    weight += static_cast<float>(m_rationOneCount) * kRationOneWeight;
    weight += static_cast<float>(m_rawFishCount + m_cookedFishCount) * kUnknownFishWeight;
    for (size_t i = 0; i < m_rawSizedFish.size(); ++i)
        weight += static_cast<float>(m_rawSizedFish[i] + m_cookedSizedFish[i]) * kSizedFishWeights[i];
    weight += static_cast<float>(m_cartridgeCount) * kCartridgeWeight;
    weight += static_cast<float>(m_waterBottles.size()) * kWaterBottleWeight;
    weight += static_cast<float>(m_cookingKits.size()) * kCookingKitWeight;
    weight += static_cast<float>(m_portableLights.size()) * kPortableLightWeight;

    // 合計重量を返します。
    return weight;
}

float SceneNarakuProto::GetPickupWeightLimit() const
{
    return std::max(kPickupWeightLimit, GetMaxWeight());
}

float SceneNarakuProto::GetWeightRate() const
{
    return GetCurrentWeight() / GetMaxWeight();
}

float SceneNarakuProto::GetMoveSpeed() const
{
    const float levelMove = 1.0f + (2.50f - 1.0f) * GetLevelGrowth();
    float speed = m_debugPlayerParams.walkSpeed * levelMove * (1.0f + GetEquipmentBonus().walkSpeed);

    // 重量70%以上では移動速度を25%下げます。
    if (GetWeightRate() >= 0.70f) speed *= 0.75f;

    // 重量補正済みの歩行速度を返します。
    return RoundToHundredth(speed);
}

float SceneNarakuProto::GetStaminaCost(float baseCost) const
{
    float cost = baseCost * GetDepthLevelStaminaConsumptionMultiplier();
    if ((GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeWaterPuddle) != 0u)
        cost *= 1.20f;
    if (GetWeightRate() >= 0.90f) cost *= 2.0f;
    if (m_fullness <= 10.0f) cost *= 1.50f;
    else if (m_fullness <= 30.0f) cost *= 1.25f;
    return cost;
}

float SceneNarakuProto::GetDepthLevelStaminaConsumptionMultiplier() const
{
    return GetDepthLevelMultiplier(kStaminaConsumptionMultipliers, GetCurrentDepth(), m_level);
}

float SceneNarakuProto::GetDepthLevelStaminaRecoveryMultiplier() const
{
    return GetDepthLevelMultiplier(kStaminaRecoveryMultipliers, GetCurrentDepth(), m_level);
}

float SceneNarakuProto::GetDepthLevelMentalConsumptionMultiplier() const
{
    return GetDepthLevelMultiplier(kMentalConsumptionMultipliers, GetCurrentDepth(), m_level);
}

float SceneNarakuProto::GetDepthLevelFullnessConsumptionMultiplier() const
{
    return GetDepthLevelMultiplier(kFullnessConsumptionMultipliers, GetCurrentDepth(), m_level);
}

bool SceneNarakuProto::CanSpendStamina(float baseCost) const
{
    // 深度・レベル、重量、飢餓の補正後の消費量を支払えるか返します。
    return m_player.stamina >= GetStaminaCost(baseCost) && m_player.stamina > 0.0f;
}

void SceneNarakuProto::SpendStamina(float baseCost)
{
    // 各補正後の消費量を差し引き、0未満にならないようにします。
    m_player.stamina = std::max(0.0f, m_player.stamina - GetStaminaCost(baseCost));
}

int SceneNarakuProto::GetCurrentDepth() const
{
    if (m_mode == Mode::Loading && m_loadingSourceGateIndex >= 0 &&
        m_loadingSourceGateIndex < static_cast<int>(m_layerGates.size()))
    {
        const int destination = m_layerGates[m_loadingSourceGateIndex].destinationAreaIndex;
        if (destination >= 0 && destination < static_cast<int>(m_areas.size()))
            return ClampDepth(m_areas[destination].depth);
    }
    if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
    {
        return ClampDepth(m_areas[m_currentAreaIndex].depth);
    }
    return 1;
}

float SceneNarakuProto::GetDepthExpMultiplier(int depth) const { return GetRulesForDepth(depth).regularExp; }
float SceneNarakuProto::GetDepthMovementExpMultiplier(int depth) const { return GetRulesForDepth(depth).movementExp; }
float SceneNarakuProto::GetDepthRewardMultiplier(int depth) const { return GetRulesForDepth(depth).reward; }
float SceneNarakuProto::GetDepthStayRewardMultiplier(int depth) const { return GetRulesForDepth(depth).stayReward; }

float SceneNarakuProto::GetLevelGrowth() const
{
    const float t = static_cast<float>(std::max(0, std::min(99, m_level - 1))) / 99.0f;
    return (1.0f - std::exp(-0.5f * t)) / (1.0f - std::exp(-0.5f));
}

SceneNarakuProto::EquipmentBonus SceneNarakuProto::GetEquipmentBonus() const
{
    EquipmentBonus bonus;
    auto addArmor = [&bonus](ArmorTier tier, bool head)
    {
        const float slot = head ? 0.4f : 0.6f;
        switch (tier)
        {
        case ArmorTier::Leather: bonus.defense += 0.10f * slot; break;
        case ArmorTier::Iron: bonus.defense += 0.25f * slot; break;
        case ArmorTier::RelicCovered: bonus.attack += 0.10f * slot; bonus.defense += 0.40f * slot; break;
        case ArmorTier::RelicHardened: bonus.defense += 0.80f * slot; break;
        case ArmorTier::RelicEnhanced: bonus.attack += 0.25f * slot; bonus.defense += 0.60f * slot; break;
        case ArmorTier::Relic:
            bonus.attack += head ? 0.10f : 0.15f;
            bonus.defense += head ? 0.30f : 0.45f;
            break;
        case ArmorTier::Unknown:
            break;
        default: break;
        }
    };
    addArmor(m_equippedHeadArmor, true);
    addArmor(m_equippedBodyArmor, false);
    if (HasRelicArmorSetEffect())
    {
        bonus.maxWeight += 1.00f;
        bonus.walkSpeed += 0.20f;
        bonus.runSpeed += 1.00f;
        bonus.hpRecoveryPerSecond += 2.0f;
        bonus.attack += 0.25f;
        bonus.miningSpeed += 0.50f;
    }
    if (HasUnknownArmorSetEffect())
    {
        bonus.maxWeight += 3.00f;
        bonus.walkSpeed += 0.75f;
        bonus.runSpeed += 4.50f;
        bonus.hpRecoveryMaxRatioPerSecond += 0.02f;
        bonus.attack += 1.00f;
        bonus.defense += 3.45f;
        bonus.miningSpeed += 1.00f;
    }
    switch (m_equippedWeapon)
    {
    case WeaponTier::NormalPickaxe: bonus.miningSpeed += 0.35f; break;
    case WeaponTier::SturdyPickaxe: bonus.miningSpeed += 0.80f; break;
    case WeaponTier::SharpPickaxe: bonus.miningSpeed += 1.00f; break;
    case WeaponTier::RelicPickaxe: bonus.miningSpeed += 1.50f; break;
    default: break;
    }
    return bonus;
}

float SceneNarakuProto::GetMaxHp() const
{
    const float levelValue = kPlayerBaseMaxHp + (1200.0f - kPlayerBaseMaxHp) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxHp));
}

float SceneNarakuProto::GetMaxStamina() const
{
    const float levelValue = kPlayerBaseMaxStamina + (800.0f - kPlayerBaseMaxStamina) * GetLevelGrowth();
    const float hungerScale = m_fullness <= 10.0f ? 0.70f : (m_fullness <= 25.0f ? 0.90f : 1.0f);
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxStamina) * hungerScale);
}

float SceneNarakuProto::GetMaxMental() const
{
    const float levelValue = kPlayerBaseMaxMental + (700.0f - kPlayerBaseMaxMental) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxMental));
}

float SceneNarakuProto::GetMaxWeight() const
{
    const float levelValue = kMaxWeight + (500.0f - kMaxWeight) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxWeight));
}

float SceneNarakuProto::GetStaminaRecoveryMultiplier() const
{
    const float levelValue = 1.0f + (5.40f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().staminaRecovery));
}

float SceneNarakuProto::GetMentalRecoveryMultiplier() const
{
    const float levelValue = 1.0f + (2.50f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().mentalRecovery));
}

float SceneNarakuProto::GetAttackPower() const
{
    const float levelValue = kPlayerBaseAttack * (1.0f + (3.0f - 1.0f) * GetLevelGrowth());
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().attack));
}

float SceneNarakuProto::GetDefenseMultiplier() const
{
    const float levelValue = kPlayerBaseDefense * (1.0f + (4.50f - 1.0f) * GetLevelGrowth());
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().defense));
}

float SceneNarakuProto::GetRunSpeed() const
{
    const float levelMove = 1.0f + (2.50f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(m_debugPlayerParams.runSpeed * levelMove * (1.0f + GetEquipmentBonus().runSpeed));
}

float SceneNarakuProto::GetRopeSpeed(bool ascending) const
{
    const float maximum = ascending ? 2.50f : 12.0f;
    const float levelSpeed = 1.0f + (maximum - 1.0f) * GetLevelGrowth();
    const EquipmentBonus bonus = GetEquipmentBonus();
    return RoundToHundredth(m_debugPlayerParams.ropeSpeed * levelSpeed *
        (1.0f + (ascending ? bonus.ropeAscentSpeed : bonus.ropeDescentSpeed)));
}

float SceneNarakuProto::GetMiningSpeedMultiplier() const
{
    const float levelSpeed = 1.0f + (10.0f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(levelSpeed * (1.0f + GetEquipmentBonus().miningSpeed));
}

void SceneNarakuProto::PreserveResourceRatios(float oldMaxHp, float oldMaxStamina, float oldMaxMental)
{
    const float hpRatio = oldMaxHp > 0.0f ? m_player.hp / oldMaxHp : 1.0f;
    const float staminaRatio = oldMaxStamina > 0.0f ? m_player.stamina / oldMaxStamina : 1.0f;
    const float mentalRatio = oldMaxMental > 0.0f ? m_player.mental / oldMaxMental : 1.0f;
    m_player.hp = std::max(0.0f, std::min(GetMaxHp(), GetMaxHp() * hpRatio));
    m_player.stamina = std::max(0.0f, std::min(GetMaxStamina(), GetMaxStamina() * staminaRatio));
    m_player.mental = std::max(0.0f, std::min(GetMaxMental(), GetMaxMental() * mentalRatio));
}

int SceneNarakuProto::GetRequiredExp(int level) const
{
    static const int anchorLevels[] = { 1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 99 };
    static const int anchorExp[] = { 100, 1000, 4000, 7500, 12000, 25000, 87500, 156000, 468000, 785625, 1500000 };
    level = std::max(1, std::min(99, level));
    for (int i = 0; i < 10; ++i)
    {
        if (level > anchorLevels[i + 1]) continue;
        const float span = static_cast<float>(anchorLevels[i + 1] - anchorLevels[i]);
        const float t = span > 0.0f ? static_cast<float>(level - anchorLevels[i]) / span : 0.0f;
        const double ratio = static_cast<double>(anchorExp[i + 1]) / static_cast<double>(anchorExp[i]);
        return static_cast<int>(std::llround(static_cast<double>(anchorExp[i]) * std::pow(ratio, t)));
    }
    return anchorExp[10];
}

void SceneNarakuProto::AwardExp(int amount)
{
    if (amount <= 0) return;
    if (m_level >= 100)
    {
        m_level100OverflowExp += amount;
        while (m_level100OverflowExp >= kLevel100ProtectionExp && m_levelProtection < std::numeric_limits<int>::max())
        {
            m_level100OverflowExp -= kLevel100ProtectionExp;
            ++m_levelProtection;
        }
        return;
    }

    m_currentExp += amount;
    while (m_level < 100)
    {
        const int required = GetRequiredExp(m_level);
        if (m_currentExp < required) break;
        const float oldHp = GetMaxHp();
        const float oldStamina = GetMaxStamina();
        const float oldMental = GetMaxMental();
        m_currentExp -= required;
        ++m_level;
        PreserveResourceRatios(oldHp, oldStamina, oldMental);
        ShowCenterNotification(u8"レベルが上がった！");
    }
    if (m_level >= 100 && m_currentExp > 0)
    {
        m_level100OverflowExp += m_currentExp;
        m_currentExp = 0;
        while (m_level100OverflowExp >= kLevel100ProtectionExp && m_levelProtection < std::numeric_limits<int>::max())
        {
            m_level100OverflowExp -= kLevel100ProtectionExp;
            ++m_levelProtection;
        }
    }
}

std::string SceneNarakuProto::FormatExp(std::int64_t value) const
{
    if (value < 1000) return std::to_string(value);
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << static_cast<double>(value) / 1000.0 << 'k';
    return stream.str();
}

int SceneNarakuProto::GetDeathLevelLoss(DeathCause cause) const
{
    switch (cause)
    {
    case DeathCause::Fall: return 5;
    case DeathCause::UpperLoad: return 3;
    case DeathCause::Enemy:
    case DeathCause::Starvation: return 2;
    default: return 1;
    }
}

void SceneNarakuProto::ApplyDeathPenalty(DeathCause cause)
{
    const float oldHp = GetMaxHp();
    const float oldStamina = GetMaxStamina();
    const float oldMental = GetMaxMental();
    int loss = std::min(GetDeathLevelLoss(cause), std::max(0, m_level - 1));
    m_result.levelBeforeDeath = m_level;
    const int protectedLevels = std::min(loss, m_levelProtection);
    m_levelProtection -= protectedLevels;
    m_result.protectionConsumed = protectedLevels;
    loss -= protectedLevels;
    m_level = std::max(1, m_level - loss);
    m_result.levelAfterDeath = m_level;
    m_currentExp = 0;
    m_level100OverflowExp = 0;
    PreserveResourceRatios(oldHp, oldStamina, oldMental);
}

void SceneNarakuProto::ApplyAbandonPenalty()
{
    if (m_level <= 1 && m_currentExp <= 0) return;
    const float progress = static_cast<float>(m_level) +
        static_cast<float>(m_currentExp) / static_cast<float>(GetRequiredExp(m_level));
    const float result = std::max(1.0f, progress - 0.5f);
    m_level = std::max(1, std::min(100, static_cast<int>(std::floor(result))));
    if (m_level >= 100) m_currentExp = 0;
    else m_currentExp = static_cast<int>(std::round((result - std::floor(result)) * GetRequiredExp(m_level)));
}

void SceneNarakuProto::ApplyPlayerDamage(float damage, DeathCause cause, const char* reason)
{
    const float applied = std::max(1.0f, damage / std::max(1.0f, GetDefenseMultiplier()));
    if (cause != DeathCause::Fall && cause != DeathCause::Starvation &&
        m_player.hp - applied <= 0.0f && TryConsumeSurvivalRelic(true, false)) return;
    m_player.hp = std::max(0.0f, m_player.hp - applied);
    if (m_player.hp <= 0.0f) StartDeath(reason, cause);
}

void SceneNarakuProto::ApplyMentalDamage(float damage, DeathCause cause, const char* reason)
{
    if (m_player.mental - damage <= 0.0f && TryConsumeSurvivalRelic(false, true)) return;
    m_player.mental = std::max(0.0f, m_player.mental - damage);
    if (m_player.mental <= 0.0f) StartDeath(reason, cause);
}

int SceneNarakuProto::CountStoredRelics(RelicType type) const
{
    return static_cast<int>(std::count_if(m_storedInventory.begin(), m_storedInventory.end(),
        [type](const RelicItem& item) { return item.type == type; }));
}

bool SceneNarakuProto::RemoveStoredRelics(RelicType type, int count)
{
    if (count < 0 || CountStoredRelics(type) < count) return false;
    for (auto it = m_storedInventory.begin(); it != m_storedInventory.end() && count > 0;)
    {
        if (it->type == type) { it = m_storedInventory.erase(it); --count; }
        else ++it;
    }
    return count == 0;
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
    constexpr float kFloorDepthTolerance = 0.20f;

    for (const FloorRegion& floor : m_floorRegions)
    {
        if (std::fabs(floor.depth - depth) > kFloorDepthTolerance)
        {
            continue;
        }

        if (!IsInsideFloor(floor, pos))
        {
            continue;
        }

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, floor.layerId);
        if (layerIndex >= 0)
        {
            const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
            int cellX = -1;
            int cellZ = -1;
            float fracX = 0.0f;
            float fracZ = 0.0f;
            if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
            {
                continue;
            }

            if (!NarakuMap::IsCellWalkable(layer, cellX, cellZ))
            {
                continue;
            }
        }

        return &floor;
    }

    return nullptr;
}

bool SceneNarakuProto::HasFloorAt(const Vec2& pos, float depth) const
{
    // 床ポインタが見つかるかどうかだけを真偽値に変換します。
    return FindFloorAt(pos, depth) != nullptr;
}

bool SceneNarakuProto::CanStandAt(const Vec2& pos, float depth) const
{
    if (!HasFloorAt(pos, depth))
    {
        return false;
    }

    const std::uint32_t flags = GetCellAttributeFlagsAt(pos, depth);
    return (flags & (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved)) == 0u;
}

std::uint32_t SceneNarakuProto::GetCellAttributeFlagsAt(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return NarakuMap::CellAttributeNone;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
    {
        return NarakuMap::CellAttributeNone;
    }

    std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
    if (!NarakuMap::IsCellWalkable(layer, cellX, cellZ))
    {
        flags |= NarakuMap::CellAttributeBlocked;
    }
    return flags;
}

void SceneNarakuProto::UseSizedFish(FishSize fishSize, bool cooked)
{
    const size_t index = static_cast<size_t>(fishSize);
    if (index >= m_rawSizedFish.size()) return;
    std::array<int, 3>& counts = cooked ? m_cookedSizedFish : m_rawSizedFish;
    if (counts[index] <= 0 || (m_fullness >= kFullnessMaximum && m_player.mental >= GetMaxMental())) return;
    --counts[index];
    const float fullness = cooked ? kCookedSizedFishFullness[index] : kRawSizedFishFullness[index];
    const float mental = cooked ? kCookedSizedFishMental[index] : kRawSizedFishMental[index];
    m_fullness = std::min(kFullnessMaximum, m_fullness + fullness);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + mental);
    AddMessage(u8"魚を食べました。");
}

void SceneNarakuProto::UpdateFishingPointRecharge(double gameSeconds)
{
    const auto update = [gameSeconds](std::vector<FishingPoint>& points)
    {
        for (FishingPoint& point : points)
        {
            if (point.remainingUses >= kFishingMaximumUses)
            {
                point.remainingUses = kFishingMaximumUses;
                point.rechargeGameSeconds = 0.0;
                continue;
            }
            point.rechargeGameSeconds += std::max(0.0, gameSeconds);
            while (point.rechargeGameSeconds >= kFishingRechargeGameSeconds && point.remainingUses < kFishingMaximumUses)
            {
                point.rechargeGameSeconds -= kFishingRechargeGameSeconds;
                ++point.remainingUses;
            }
            if (point.remainingUses >= kFishingMaximumUses) point.rechargeGameSeconds = 0.0;
        }
    };
    update(m_fishingPoints);
    for (int index = 0; index < static_cast<int>(m_areas.size()); ++index)
        if (index != m_currentAreaIndex) update(m_areas[static_cast<size_t>(index)].fishingPoints);
}

void SceneNarakuProto::DrawFishingConfirm()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f }, ImGuiCond_Always, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 390.0f, 155.0f }, ImGuiCond_Always);
    NarakuUi::Begin(u8"釣り地点", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    if (m_fishingPointIndex >= 0 && m_fishingPointIndex < static_cast<int>(m_fishingPoints.size()))
    {
        const FishingPoint& point = m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)];
        ImGui::Text(u8"水場: %s / 残り回数: %d / %d", point.lake ? u8"湖" : u8"池",
            point.remainingUses, kFishingMaximumUses);
        ImGui::TextDisabled(u8"開始時に最大スタミナの10%%を消費します。");
        if (ImGui::Button(u8"釣る", { 120.0f, 0.0f })) StartFishing(m_fishingPointIndex);
        ImGui::SameLine();
    }
    if (NarakuUi::BackButton(u8"戻る", { 120.0f, 0.0f }))
    {
        m_fishingPointIndex = -1;
        m_mode = Mode::Explore;
    }
    ImGui::End();
}

void SceneNarakuProto::StartFishing(int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= static_cast<int>(m_fishingPoints.size())) return;
    FishingPoint& point = m_fishingPoints[static_cast<size_t>(pointIndex)];
    if (GetCurrentDepth() != 5) { AddMessage(u8"釣りは第五層の池・湖で行えます。"); return; }
    const float staminaCost = GetMaxStamina() * kFishingStartStaminaRatio;
    if (point.remainingUses <= 0) { AddMessage(u8"この釣り地点では現在釣れません。"); return; }
    if (m_player.stamina < staminaCost) { AddMessage(u8"釣りを始めるスタミナが足りません。"); return; }
    if (GetCurrentWeight() + kSizedFishWeights.back() > GetPickupWeightLimit())
    { AddMessage(u8"大きな魚を持てる重量の空きがありません。"); return; }
    m_player.stamina -= staminaCost;
    --point.remainingUses;
    if (point.remainingUses == kFishingMaximumUses - 1) point.rechargeGameSeconds = 0.0;
    SaveCurrentAreaState();
    m_fishingPointIndex = pointIndex;
    m_fishingPhase = FishingPhase::Waiting;
    m_fishingTimer = RandomFloat(5.0f, 15.0f);
    m_fishingPreviousHp = m_player.hp;
    m_mode = Mode::Explore;
    AddMessage(u8"釣りを開始しました。反応があったらFキーを押してください。");
}

void SceneNarakuProto::CancelFishing(const char* reason)
{
    if (m_fishingPhase == FishingPhase::None) return;
    m_fishingPhase = FishingPhase::None;
    m_fishingPointIndex = -1;
    m_fishingTimer = 0.0f;
    if (reason && *reason) AddMessage(reason);
}

void SceneNarakuProto::CompleteFishing()
{
    if (m_fishingPointIndex < 0 || m_fishingPointIndex >= static_cast<int>(m_fishingPoints.size()))
    { CancelFishing(nullptr); return; }
    FishingPoint& point = m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)];
    const float roll = RandomFloat(0.0f, 1.0f);
    if (point.lake && GetCurrentDepth() == 5 && roll < 0.05f)
    {
        ++m_rawFishCount;
        AddMessage(u8"見たことない魚を釣り上げました。");
    }
    else
    {
        const float sizeRoll = RandomFloat(0.0f, 1.0f);
        int size = 0;
        if (point.lake) size = sizeRoll < 0.30f ? 0 : (sizeRoll < 0.70f ? 1 : 2);
        else size = sizeRoll < 0.60f ? 0 : (sizeRoll < 0.85f ? 1 : 2);
        ++m_rawSizedFish[static_cast<size_t>(size)];
        static const char* names[] = { u8"魚（小）", u8"魚（中）", u8"魚（大）" };
        AddMessage((std::string(names[size]) + u8"を釣り上げました。").c_str());
    }
    m_fishingPhase = FishingPhase::None;
    m_fishingPointIndex = -1;
    SaveCurrentAreaState();
}

void SceneNarakuProto::UpdateFishing(float dt)
{
    if (m_fishingPhase == FishingPhase::None) return;
    if (IsActionUIBackTrigger()) { CancelFishing(u8"釣りを中断しました。"); return; }
    if (m_lastFrameMovementDistance > 0.001f || m_player.attackTimer > 0.0f ||
        m_player.stepTimer > 0.0f || m_player.hp < m_fishingPreviousHp || !m_player.grounded || m_player.onRope)
    { CancelFishing(u8"行動または被弾により釣りが中断されました。"); return; }
    m_fishingPreviousHp = m_player.hp;
    m_fishingTimer -= dt;
    if (m_fishingPhase == FishingPhase::Waiting)
    {
        if (IsActionInteractTrigger()) { CancelFishing(u8"早く引きすぎて魚を逃しました。"); return; }
        if (m_fishingTimer <= 0.0f)
        {
            m_fishingPhase = FishingPhase::Bite;
            m_fishingTimer = m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)].lake ? 1.25f : 1.0f;
            AddMessage(u8"反応あり！ Fキーで引き上げます。");
        }
    }
    else if (m_fishingPhase == FishingPhase::Bite)
    {
        if (IsActionInteractTrigger()) { m_fishingPhase = FishingPhase::Landing; m_fishingTimer = kFishingLandingDuration; }
        else if (m_fishingTimer <= 0.0f) CancelFishing(u8"魚を逃しました。");
    }
    else if (m_fishingPhase == FishingPhase::Landing && m_fishingTimer <= 0.0f) CompleteFishing();
}

bool SceneNarakuProto::IntersectsEnvironmentCollider(
    const Vec2& position,
    float feetWorldY,
    float characterRadius,
    float characterHeight) const
{
    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        const auto resourceIt = std::find_if(m_environmentModels.begin(), m_environmentModels.end(),
            [&](const EnvironmentModelResource& resource) { return resource.id == object.modelId; });
        if (resourceIt == m_environmentModels.end() || !resourceIt->colliderEnabled) continue;
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, object.layerId);
        if (layerIndex < 0) continue;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[static_cast<size_t>(layerIndex)];
        int footprintX = resourceIt->footprintX;
        int footprintZ = resourceIt->footprintZ;
        if ((object.rotationQuarterTurns & 1) != 0) std::swap(footprintX, footprintZ);
        Vec2 placement = { object.xz.x, object.xz.z };
        if (object.footprintAnchored)
        {
            placement.x += static_cast<float>(footprintX - 1) * layer.cellSize * 0.5f;
            placement.y += static_cast<float>(footprintZ - 1) * layer.cellSize * 0.5f;
        }
        const float ratioX = object.scaleX / std::max(0.001f, resourceIt->defaultScale.x);
        const float ratioY = object.scaleY / std::max(0.001f, resourceIt->defaultScale.y);
        const float ratioZ = object.scaleZ / std::max(0.001f, resourceIt->defaultScale.z);
        const float groundY = GetGroundWorldY(placement, layer.layerDepth) + object.offsetY;
        const DirectX::XMFLOAT3 center = {
            placement.x + resourceIt->colliderCenter.x * ratioX,
            groundY + resourceIt->colliderCenter.y * ratioY,
            placement.y + resourceIt->colliderCenter.z * ratioZ };
        const DirectX::XMFLOAT3 half = {
            resourceIt->colliderSize.x * ratioX * 0.5f,
            resourceIt->colliderSize.y * ratioY * 0.5f,
            resourceIt->colliderSize.z * ratioZ * 0.5f };
        if (feetWorldY >= center.y + half.y || feetWorldY + characterHeight <= center.y - half.y) continue;
        const float closestX = std::max(center.x - half.x, std::min(position.x, center.x + half.x));
        const float closestZ = std::max(center.z - half.z, std::min(position.y, center.z + half.z));
        const float dx = position.x - closestX;
        const float dz = position.y - closestZ;
        if (dx * dx + dz * dz < characterRadius * characterRadius) return true;
    }
    return false;
}

void SceneNarakuProto::ResolveEnvironmentVerticalCollision(
    Vec2& position,
    float previousFeetWorldY,
    float& feetWorldY,
    float& verticalSpeed,
    float characterRadius,
    float characterHeight) const
{
    constexpr float separation = 0.01f;
    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        const auto resourceIt = std::find_if(m_environmentModels.begin(), m_environmentModels.end(),
            [&](const EnvironmentModelResource& resource) { return resource.id == object.modelId; });
        if (resourceIt == m_environmentModels.end() || !resourceIt->colliderEnabled) continue;
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, object.layerId);
        if (layerIndex < 0) continue;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[static_cast<size_t>(layerIndex)];
        int footprintX = resourceIt->footprintX;
        int footprintZ = resourceIt->footprintZ;
        if ((object.rotationQuarterTurns & 1) != 0) std::swap(footprintX, footprintZ);
        Vec2 placement = { object.xz.x, object.xz.z };
        if (object.footprintAnchored)
        {
            placement.x += static_cast<float>(footprintX - 1) * layer.cellSize * 0.5f;
            placement.y += static_cast<float>(footprintZ - 1) * layer.cellSize * 0.5f;
        }
        const float ratioX = object.scaleX / std::max(0.001f, resourceIt->defaultScale.x);
        const float ratioY = object.scaleY / std::max(0.001f, resourceIt->defaultScale.y);
        const float ratioZ = object.scaleZ / std::max(0.001f, resourceIt->defaultScale.z);
        const float groundY = GetGroundWorldY(placement, layer.layerDepth) + object.offsetY;
        const float centerX = placement.x + resourceIt->colliderCenter.x * ratioX;
        const float centerY = groundY + resourceIt->colliderCenter.y * ratioY;
        const float centerZ = placement.y + resourceIt->colliderCenter.z * ratioZ;
        const float halfX = resourceIt->colliderSize.x * ratioX * 0.5f;
        const float halfY = resourceIt->colliderSize.y * ratioY * 0.5f;
        const float halfZ = resourceIt->colliderSize.z * ratioZ * 0.5f;
        const float minX = centerX - halfX;
        const float maxX = centerX + halfX;
        const float minY = centerY - halfY;
        const float maxY = centerY + halfY;
        const float minZ = centerZ - halfZ;
        const float maxZ = centerZ + halfZ;
        const float closestX = std::max(minX, std::min(position.x, maxX));
        const float closestZ = std::max(minZ, std::min(position.y, maxZ));
        const float dx = position.x - closestX;
        const float dz = position.y - closestZ;
        if (dx * dx + dz * dz >= characterRadius * characterRadius) continue;

        if (verticalSpeed > 0.0f && previousFeetWorldY + characterHeight <= minY && feetWorldY + characterHeight > minY)
        {
            feetWorldY = minY - characterHeight - separation;
            verticalSpeed = 0.0f;
            continue;
        }
        if (verticalSpeed <= 0.0f && previousFeetWorldY >= maxY && feetWorldY < maxY)
        {
            const float left = std::fabs(position.x - minX);
            const float right = std::fabs(maxX - position.x);
            const float north = std::fabs(position.y - minZ);
            const float south = std::fabs(maxZ - position.y);
            const float nearest = std::min(std::min(left, right), std::min(north, south));
            if (nearest == left) position.x = minX - characterRadius - separation;
            else if (nearest == right) position.x = maxX + characterRadius + separation;
            else if (nearest == north) position.y = minZ - characterRadius - separation;
            else position.y = maxZ + characterRadius + separation;
        }
    }
}

bool SceneNarakuProto::CanTraverseGround(
    const Vec2& from,
    const Vec2& to,
    float depth,
    float characterRadius,
    float characterHeight) const
{
    if (!HasFloorAt(to, depth))
    {
        return false;
    }

    const float fromHeight = SampleTerrainHeightOffsetAt(from, depth);
    const float toHeight = SampleTerrainHeightOffsetAt(to, depth);
    const float climbDelta = toHeight - fromHeight;
    const float dropDelta = fromHeight - toHeight;
    const std::uint32_t fromFlags = GetCellAttributeFlagsAt(from, depth);
    const std::uint32_t toFlags = GetCellAttributeFlagsAt(to, depth);
    const bool dropAllowed = ((fromFlags | toFlags) & NarakuMap::CellAttributeDropAllowed) != 0u;
    const bool cliffEdge = ((fromFlags | toFlags) & NarakuMap::CellAttributeCliffEdge) != 0u;

    if (climbDelta > kMaxWalkClimbHeight + kSlopeHeightTolerance)
    {
        return false;
    }

    if (dropDelta > kMaxWalkDropHeight + kSlopeHeightTolerance && !dropAllowed)
    {
        return false;
    }

    if (cliffEdge && dropDelta > kCliffEdgeBlockDropHeight + kSlopeHeightTolerance && !dropAllowed)
    {
        return false;
    }

    if (IntersectsEnvironmentCollider(to, GetGroundWorldY(to, depth), characterRadius, characterHeight))
    {
        return false;
    }

    return true;
}

bool SceneNarakuProto::CanTraverseAir(
    const Vec2& to,
    float depth,
    float characterRadius,
    float characterHeight) const
{
    const int layerIndex = FindLayerIndexAt(to, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, to, cellX, cellZ, fracX, fracZ))
    {
        return false;
    }

    const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
    if ((flags & NarakuMap::CellAttributeRemoved) != 0u)
    {
        return false;
    }

    if ((flags & NarakuMap::CellAttributeBlocked) == 0u &&
        GetGroundWorldY(to, depth) > m_player.feetWorldY + kSlopeHeightTolerance)
    {
        return false;
    }

    if (IntersectsEnvironmentCollider(to, m_player.feetWorldY, characterRadius, characterHeight))
    {
        return false;
    }

    return true;
}

SceneNarakuProto::Vec2 SceneNarakuProto::ResolveFloorMove(
    const Vec2& from,
    const Vec2& to,
    float depth,
    float characterRadius,
    float characterHeight) const
{
    auto tryResolveSingleStep = [this, depth, characterRadius, characterHeight](const Vec2& stepFrom, const Vec2& stepTo, Vec2& outResolved) -> bool
    {
        if (CanTraverseGround(stepFrom, stepTo, depth, characterRadius, characterHeight))
        {
            outResolved = stepTo;
            return true;
        }

        const Vec2 xOnly = { stepTo.x, stepFrom.y };
        if (CanTraverseGround(stepFrom, xOnly, depth, characterRadius, characterHeight))
        {
            outResolved = xOnly;
            return true;
        }

        const Vec2 yOnly = { stepFrom.x, stepTo.y };
        if (CanTraverseGround(stepFrom, yOnly, depth, characterRadius, characterHeight))
        {
            outResolved = yOnly;
            return true;
        }

        outResolved = stepFrom;
        return false;
    };

    const float totalDistance = Distance(from, to);
    if (totalDistance <= kSlopeMoveSampleStep)
    {
        Vec2 resolved = from;
        return tryResolveSingleStep(from, to, resolved) ? resolved : from;
    }

    const int stepCount = std::max(1, static_cast<int>(std::ceil(totalDistance / kSlopeMoveSampleStep)));
    const Vec2 delta = Sub(to, from);
    Vec2 current = from;

    for (int stepIndex = 1; stepIndex <= stepCount; ++stepIndex)
    {
        const float t = static_cast<float>(stepIndex) / static_cast<float>(stepCount);
        const Vec2 stepTarget = Add(from, Mul(delta, t));
        Vec2 resolved = current;
        if (!tryResolveSingleStep(current, stepTarget, resolved))
        {
            break;
        }
        current = resolved;
    }

    return current;
}

SceneNarakuProto::Vec2 SceneNarakuProto::ResolveAirMove(const Vec2& from, const Vec2& to, float depth) const
{
    auto tryResolveSingleStep = [this, depth](const Vec2& stepFrom, const Vec2& stepTo, Vec2& outResolved) -> bool
    {
        if (CanTraverseAir(stepTo, depth))
        {
            outResolved = stepTo;
            return true;
        }

        const Vec2 xOnly = { stepTo.x, stepFrom.y };
        if (CanTraverseAir(xOnly, depth))
        {
            outResolved = xOnly;
            return true;
        }

        const Vec2 yOnly = { stepFrom.x, stepTo.y };
        if (CanTraverseAir(yOnly, depth))
        {
            outResolved = yOnly;
            return true;
        }

        outResolved = stepFrom;
        return false;
    };

    const float totalDistance = Distance(from, to);
    if (totalDistance <= kSlopeMoveSampleStep)
    {
        Vec2 resolved = from;
        return tryResolveSingleStep(from, to, resolved) ? resolved : from;
    }

    const int stepCount = std::max(1, static_cast<int>(std::ceil(totalDistance / kSlopeMoveSampleStep)));
    const Vec2 delta = Sub(to, from);
    Vec2 current = from;
    for (int stepIndex = 1; stepIndex <= stepCount; ++stepIndex)
    {
        const float t = static_cast<float>(stepIndex) / static_cast<float>(stepCount);
        const Vec2 stepTarget = Add(from, Mul(delta, t));
        Vec2 resolved = current;
        if (!tryResolveSingleStep(current, stepTarget, resolved))
        {
            break;
        }
        current = resolved;
    }
    return current;
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetTerrainDownhillDirection(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return {};
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ) || layer.cellSize <= 0.0f)
    {
        return {};
    }

    const float h00 = NarakuMap::GetVertexHeight(layer, cellX, cellZ);
    const float h10 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ);
    const float h01 = NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1);
    const float h11 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1);
    const float riseX = ((h10 - h00) * (1.0f - fracZ) + (h11 - h01) * fracZ) / layer.cellSize;
    const float riseZ = ((h01 - h00) * (1.0f - fracX) + (h11 - h10) * fracX) / layer.cellSize;
    if (std::sqrt(riseX * riseX + riseZ * riseZ) <= kSlopeHeightTolerance)
    {
        return {};
    }
    return Normalize({ -riseX, -riseZ });
}

bool SceneNarakuProto::RestorePlayerToSafeGround()
{
    if (!m_player.hasSafeGroundPos)
    {
        return false;
    }

    m_player.pos = m_player.lastSafeGroundPos;
    m_player.depth = m_player.lastSafeGroundDepth;
    m_player.grounded = true;
    m_player.onRope = false;
    m_activeRope = -1;
    m_player.airTime = 0.0f;
    m_player.verticalSpeed = 0.0f;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.landingRecoveryTimer = 0.0f;
    m_player.blockedCellAirTime = 0.0f;
    m_player.blockedCellVelocity = {};
    return true;
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
        const RopePoint& rope = m_ropePoints[i];
        const float topDistance = std::fabs(m_player.depth - rope.topDepth) <= 0.35f ? Distance(m_player.pos, rope.topPos) : range + 1.0f;
        const float bottomDistance = std::fabs(m_player.depth - rope.bottomDepth) <= 0.35f ? Distance(m_player.pos, rope.bottomPos) : range + 1.0f;
        const float d = std::min(topDistance, bottomDistance);

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

int SceneNarakuProto::FindFallingRopeIndex(float radius, float& outProgress) const
{
    outProgress = 0.0f;
    const float radiusSq = radius * radius;
    float bestDistanceSq = radiusSq;
    int bestIndex = -1;

    const float playerX = m_player.pos.x;
    const float playerY = m_player.feetWorldY + kRopePlayerHangOffset;
    const float playerZ = m_player.pos.y;

    for (int i = 0; i < static_cast<int>(m_ropePoints.size()); ++i)
    {
        const RopePoint& rope = m_ropePoints[i];
        const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);
        const float segmentX = traversal.bottomPosition.x - traversal.topPosition.x;
        const float segmentY = traversal.bottomWorldY - traversal.topWorldY;
        const float segmentZ = traversal.bottomPosition.y - traversal.topPosition.y;
        const float segmentLengthSq = segmentX * segmentX + segmentY * segmentY + segmentZ * segmentZ;

        float progress = 0.0f;
        if (segmentLengthSq > 0.000001f)
        {
            const float playerFromTopX = playerX - traversal.topPosition.x;
            const float playerFromTopY = playerY - traversal.topWorldY;
            const float playerFromTopZ = playerZ - traversal.topPosition.y;
            progress = std::max(0.0f, std::min(1.0f,
                (playerFromTopX * segmentX + playerFromTopY * segmentY + playerFromTopZ * segmentZ) / segmentLengthSq));
        }

        const float closestX = traversal.topPosition.x + segmentX * progress;
        const float closestY = traversal.topWorldY + segmentY * progress;
        const float closestZ = traversal.topPosition.y + segmentZ * progress;
        const float distanceX = playerX - closestX;
        const float distanceY = playerY - closestY;
        const float distanceZ = playerZ - closestZ;
        const float distanceSq = distanceX * distanceX + distanceY * distanceY + distanceZ * distanceZ;

        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestIndex = i;
            outProgress = progress;
        }
    }

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

    const Vec2 ropePos = GetRopePosition(ropeIndex, m_ropeProgress);
    const Vec2 leavePos = Add(ropePos, Mul(cameraRight, leaveSign * 0.80f));

    // 候補位置に床があり、地形条件も満たすならそこへ降ります。
    if (CanTraverseGround(ropePos, leavePos, m_player.depth))
    {
        m_player.pos = leavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        m_player.grounded = true;
        m_player.verticalSpeed = 0.0f;
        m_player.airTime = 0.0f;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.landingRecoveryTimer = 0.0f;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 深度が端にかなり近い場合は端深度へ吸着して、降りられるかをもう一度試します。
    const bool useBottom = m_ropeProgress >= 0.5f;
    const float endpointDepth = useBottom ? rope.bottomDepth : rope.topDepth;
    const Vec2 endpointPos = useBottom ? rope.bottomPos : rope.topPos;
    const Vec2 endpointLeavePos = Add(endpointPos, Mul(cameraRight, leaveSign * 0.80f));
    if ((m_ropeProgress <= 0.05f || m_ropeProgress >= 0.95f) && CanTraverseGround(endpointPos, endpointLeavePos, endpointDepth))
    {
        m_player.depth = endpointDepth;
        m_player.pos = endpointLeavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        m_player.grounded = true;
        m_player.verticalSpeed = 0.0f;
        m_player.airTime = 0.0f;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.landingRecoveryTimer = 0.0f;
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

void SceneNarakuProto::ShowCenterNotification(const std::string& message)
{
    m_centerNotification = message;
    m_centerNotificationTimer = 1.5f;
}

void SceneNarakuProto::DiscoverNearbyMiningPoints()
{
    // すべての採掘ポイントを確認します。
    for (MiningPoint& point : m_miningPoints)
    {
        // 未発見かつプレイヤーが近いポイントだけ発見済みにします。
        if (!point.discovered && std::fabs(m_player.depth - point.depth) <= 0.35f && IsNear(m_player.pos, point.pos, kDiscoverRange))
        {
            // 採掘ポイントを発見済みにします。
            point.discovered = true;

            // HUDログに発見を出します。
            AddMessage(u8"採掘ポイントを発見しました。");
        }
    }
    for (FishingPoint& point : m_fishingPoints)
    {
        if (!point.discovered && std::fabs(m_player.depth - point.depth) <= 0.35f &&
            IsNear(m_player.pos, point.pos, kDiscoveryRange))
        {
            point.discovered = true;
            AddMessage(u8"釣り地点を発見しました。");
        }
    }
}

void SceneNarakuProto::DropInventoryItem(int index)
{
    // 範囲外の番号なら何もしません。
    if (index < 0 || index >= static_cast<int>(m_inventory.size())) return;

    // 選択旧器を現在位置の地面旧器として追加します。
    m_groundRelics.push_back({ m_inventory[index], m_player.pos, m_player.depth, true, -1 });

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
            if (m_overlayReturnMode == Mode::Surface) m_surfacePins = m_pins;

            // HUDログに削除を出します。
            AddMessage(u8"ピンを削除しました。");
            SaveProgress();

            // 削除したので追加処理は行いません。
            return;
        }
    }

    // 近くにピンがなければ新しいピンを追加します。
    m_pins.push_back(worldPos);
    if (m_overlayReturnMode == Mode::Surface) m_surfacePins = m_pins;

    // HUDログに追加を出します。
    AddMessage(u8"ピンを設置しました。");
    SaveProgress();
}

SceneNarakuProto::Vec2 SceneNarakuProto::ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos, float zoom, const Vec2& focusPos) const
{
    float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * zoom;
    float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * zoom;

    if (std::fabs(scaleX) < 0.001f) scaleX = 1.0f;
    if (std::fabs(scaleY) < 0.001f) scaleY = 1.0f;

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;

    float dx = (mousePos.x - centerX) / scaleX;
    float dy = (centerY - mousePos.y) / scaleY;

    return { focusPos.x + dx, focusPos.y + dy };
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float zoom, const Vec2& focusPos) const
{
    float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * zoom;
    float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * zoom;

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;

    float dx = worldPos.x - focusPos.x;
    float dy = worldPos.y - focusPos.y;

    return { centerX + dx * scaleX, centerY - dy * scaleY };
}

int SceneNarakuProto::FindLayerIndexByDepth(float depth, float tolerance) const
{
    for (int i = 0; i < static_cast<int>(m_runtimeMap.terrainLayers.size()); ++i)
    {
        if (std::fabs(m_runtimeMap.terrainLayers[i].layerDepth - depth) <= tolerance)
        {
            return i;
        }
    }
    return -1;
}

int SceneNarakuProto::FindLayerIndexAt(const Vec2& pos, float depth, float tolerance) const
{
    for (int i = 0; i < static_cast<int>(m_runtimeMap.terrainLayers.size()); ++i)
    {
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[i];
        if (std::fabs(layer.layerDepth - depth) > tolerance)
        {
            continue;
        }

        int cellX = -1;
        int cellZ = -1;
        float fracX = 0.0f;
        float fracZ = 0.0f;
        if (TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
        {
            return i;
        }
    }

    return -1;
}

bool SceneNarakuProto::TryGetLayerCellAt(const NarakuMap::TerrainLayer& layer, const Vec2& pos, int& outCellX, int& outCellZ, float& outFracX, float& outFracZ) const
{
    if (layer.gridWidth < 2 || layer.gridHeight < 2 || layer.cellSize <= 0.0f)
    {
        return false;
    }

    const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float localX = (pos.x - minX) / layer.cellSize;
    const float localZ = (pos.y - minZ) / layer.cellSize;
    if (localX < 0.0f || localZ < 0.0f || localX > static_cast<float>(layer.gridWidth - 1) || localZ > static_cast<float>(layer.gridHeight - 1))
    {
        return false;
    }

    outCellX = static_cast<int>(std::floor(localX));
    outCellZ = static_cast<int>(std::floor(localZ));
    outCellX = std::max(0, std::min(outCellX, layer.gridWidth - 2));
    outCellZ = std::max(0, std::min(outCellZ, layer.gridHeight - 2));
    outFracX = std::max(0.0f, std::min(localX - static_cast<float>(outCellX), 1.0f));
    outFracZ = std::max(0.0f, std::min(localZ - static_cast<float>(outCellZ), 1.0f));
    return true;
}

float SceneNarakuProto::SampleTerrainHeightOffsetAt(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return 0.0f;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
    {
        return 0.0f;
    }

    const float h00 = NarakuMap::GetVertexHeight(layer, cellX, cellZ);
    const float h10 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ);
    const float h01 = NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1);
    const float h11 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1);
    const float hx0 = h00 + (h10 - h00) * fracX;
    const float hx1 = h01 + (h11 - h01) * fracX;
    return hx0 + (hx1 - hx0) * fracZ;
}

float SceneNarakuProto::GetGroundWorldY(const Vec2& pos, float depth) const
{
    return SampleTerrainHeightOffsetAt(pos, depth) - depth * 0.35f;
}

float SceneNarakuProto::GetPlayerAirborneOffset() const
{
    if (m_player.grounded || m_player.onRope)
    {
        return 0.0f;
    }

    return std::max(0.0f, m_player.feetWorldY - GetGroundWorldY(m_player.pos, m_player.depth));
}

SceneNarakuProto::RopeTraversalEndpoints SceneNarakuProto::GetRopeTraversalEndpoints(const RopePoint& rope) const
{
    RopeTraversalEndpoints result = {};
    result.supportPosition = rope.topPos;
    result.supportGroundWorldY = GetGroundWorldY(rope.topPos, rope.topDepth);
    result.topPosition = rope.topPos;
    result.topWorldY = result.supportGroundWorldY;
    result.bottomPosition = rope.bottomPos;
    result.bottomWorldY = GetGroundWorldY(rope.bottomPos, rope.bottomDepth);

    if (m_ropeSupportModel == nullptr)
    {
        return result;
    }

    Vec2 descentDirection = Normalize(Sub(rope.bottomPos, rope.topPos));
    if (Distance({}, descentDirection) <= 0.001f)
    {
        descentDirection = { 0.0f, 1.0f };
    }

    const int topLayerIndex = FindLayerIndexAt(rope.topPos, rope.topDepth);
    if (topLayerIndex < 0 || topLayerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return result;
    }

    const NarakuMap::TerrainLayer& topLayer =
        m_runtimeMap.terrainLayers[static_cast<std::size_t>(topLayerIndex)];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(topLayer, rope.topPos, cellX, cellZ, fracX, fracZ))
    {
        return result;
    }

    const float minX = topLayer.center.x -
        static_cast<float>(topLayer.gridWidth - 1) * topLayer.cellSize * 0.5f;
    const float minZ = topLayer.center.z -
        static_cast<float>(topLayer.gridHeight - 1) * topLayer.cellSize * 0.5f;
    result.supportPosition = {
        minX + (static_cast<float>(cellX) + 0.5f) * topLayer.cellSize,
        minZ + (static_cast<float>(cellZ) + 0.5f) * topLayer.cellSize };
    result.supportGroundWorldY = GetGroundWorldY(result.supportPosition, rope.topDepth);

    const float directionMaximum = std::max(
        std::fabs(descentDirection.x),
        std::fabs(descentDirection.y));
    const float outsideDistance = topLayer.cellSize * 0.5f /
        std::max(0.001f, directionMaximum) + kRopeVisualDiameter * 0.5f;
    result.topPosition = Add(result.supportPosition, Mul(descentDirection, outsideDistance));
    result.topWorldY = result.supportGroundWorldY + kRopeAnchorHeight;
    return result;
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetRopePosition(int ropeIndex, float progress) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return m_player.pos;
    }

    const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(m_ropePoints[ropeIndex]);
    const float t = std::max(0.0f, std::min(progress, 1.0f));
    return {
        traversal.topPosition.x + (traversal.bottomPosition.x - traversal.topPosition.x) * t,
        traversal.topPosition.y + (traversal.bottomPosition.y - traversal.topPosition.y) * t };
}

float SceneNarakuProto::GetRopeWorldY(int ropeIndex, float progress) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return GetGroundWorldY(m_player.pos, m_player.depth);
    }

    const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(m_ropePoints[ropeIndex]);
    const float t = std::max(0.0f, std::min(progress, 1.0f));
    return traversal.topWorldY + (traversal.bottomWorldY - traversal.topWorldY) * t;
}

float SceneNarakuProto::GetRopePlayerFeetWorldY(int ropeIndex, float progress) const
{
    return GetRopeWorldY(ropeIndex, progress) - kRopePlayerHangOffset;
}

float SceneNarakuProto::GetBottomRopeGrabProgress(int ropeIndex) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return 1.0f;
    }

    const RopePoint& rope = m_ropePoints[ropeIndex];
    const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);
    const float targetFeetWorldY = GetGroundWorldY(rope.bottomPos, rope.bottomDepth) +
        kBottomRopeGrabClearance;
    const float targetGrabWorldY = targetFeetWorldY + kRopePlayerHangOffset;
    const float verticalDelta = traversal.bottomWorldY - traversal.topWorldY;
    if (std::fabs(verticalDelta) <= 0.0001f)
    {
        return 1.0f;
    }

    return std::max(0.0f, std::min(1.0f,
        (targetGrabWorldY - traversal.topWorldY) / verticalDelta));
}
DirectX::XMFLOAT3 SceneNarakuProto::GetTerrainVertexWorld3D(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ, float heightOffset) const
{
    const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float x = minX + static_cast<float>(gridX) * layer.cellSize;
    const float z = minZ + static_cast<float>(gridZ) * layer.cellSize;
    const float terrainHeight = NarakuMap::GetVertexHeight(layer, gridX, gridZ);
    return { x, terrainHeight + heightOffset - layer.layerDepth * 0.35f, z };
}

DirectX::XMFLOAT3 SceneNarakuProto::GetTerrainVertexNormal(
    const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const
{
    using namespace DirectX;

    const int leftX = std::max(0, gridX - 1);
    const int rightX = std::min(layer.gridWidth - 1, gridX + 1);
    const int nearZ = std::max(0, gridZ - 1);
    const int farZ = std::min(layer.gridHeight - 1, gridZ + 1);
    const XMFLOAT3 left = GetTerrainVertexWorld3D(layer, leftX, gridZ);
    const XMFLOAT3 right = GetTerrainVertexWorld3D(layer, rightX, gridZ);
    const XMFLOAT3 nearPoint = GetTerrainVertexWorld3D(layer, gridX, nearZ);
    const XMFLOAT3 farPoint = GetTerrainVertexWorld3D(layer, gridX, farZ);

    const XMVECTOR tangentX = XMVectorSubtract(XMLoadFloat3(&right), XMLoadFloat3(&left));
    const XMVECTOR tangentZ = XMVectorSubtract(XMLoadFloat3(&farPoint), XMLoadFloat3(&nearPoint));
    XMVECTOR normalVector = XMVector3Cross(tangentZ, tangentX);
    if (XMVectorGetX(XMVector3LengthSq(normalVector)) <= 0.000001f)
    {
        normalVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }
    else
    {
        normalVector = XMVector3Normalize(normalVector);
    }

    XMFLOAT3 normal = {};
    XMStoreFloat3(&normal, normalVector);
    if (normal.y < 0.0f)
    {
        normal.x = -normal.x;
        normal.y = -normal.y;
        normal.z = -normal.z;
    }
    return normal;
}

DirectX::XMFLOAT3 SceneNarakuProto::ToWorld3D(const Vec2& pos, float depth, float heightOffset) const
{
    const float terrainHeight = SampleTerrainHeightOffsetAt(pos, depth);
    return { pos.x, terrainHeight + heightOffset - depth * 0.35f, pos.y };
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

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToObliqueCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float depthOffset) const
{
    // 斜め見下ろし用にワールド座標を45度回したX成分へ変換します。
    float projectedX = worldPos.x - worldPos.y;

    // 斜め見下ろし用に奥行きを圧縮したY成分へ変換します。
    float projectedY = (worldPos.x + worldPos.y) * 0.45f;

    // 深度が大きいほど画面下へずらし、潜っている感覚を足します。
    projectedY += depthOffset * 0.35f;

    // 投影後のXをキャンバス幅に収まるよう0-1へ正規化します。
    float nx = (projectedX / (m_worldHalfSize * 2.0f) + 1.0f) * 0.5f;

    // 投影後のYをキャンバス高さに収まるよう0-1へ正規化します。
    float ny = (projectedY / (m_worldHalfSize * 1.35f) + 1.0f) * 0.5f;

    // 正規化座標をキャンバス上のスクリーン座標へ変換します。
    return { canvasPos.x + nx * canvasSize.x, canvasPos.y + ny * canvasSize.y };
}

void SceneNarakuProto::DrawMiniMap()
{
    if (m_debugPlayerParams.showMinimap < 0.5f) return;

    float posX = m_debugPlayerParams.minimapPosX;
    float posY = m_debugPlayerParams.minimapPosY;
    float size = m_debugPlayerParams.minimapSize;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(size, size), ImGuiCond_Always);

    ImGui::Begin("MiniMap#MiniMapWindow", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings);

    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };
    Vec2 canvasSize = { size, size };
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 24, 24, 180));
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(100, 120, 110, 255), 0.0f, 0, 1.5f);

    Vec2 ret = WorldToCanvas(canvasPos, canvasSize, m_returnPoint, m_mapZoom, m_player.pos);
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 6.0f, IM_COL32(80, 180, 255, 255));

    for (const RopePoint& rope : m_ropePoints)
    {
        const Vec2 top = WorldToCanvas(canvasPos, canvasSize, rope.topPos, m_mapZoom, m_player.pos);
        const Vec2 bottom = WorldToCanvas(canvasPos, canvasSize, rope.bottomPos, m_mapZoom, m_player.pos);
        draw->AddLine(ImVec2(top.x, top.y), ImVec2(bottom.x, bottom.y), IM_COL32(170, 120, 70, 255), 3.0f);
    }

    for (const MiningPoint& point : m_miningPoints)
    {
        const bool visible = point.mined || point.discovered || point.sensed;
        if (!visible) continue;

        Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, m_player.pos);
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, color);
    }

    for (const GroundRelic& relic : m_groundRelics)
    {
        if (!relic.active) continue;
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, relic.pos, m_mapZoom, m_player.pos);
        draw->AddRectFilled(ImVec2(p.x - 2.0f, p.y - 2.0f), ImVec2(p.x + 2.0f, p.y + 2.0f), IM_COL32(240, 220, 130, 255));
    }

    for (const Vec2& pin : m_pins)
    {
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin, m_mapZoom, m_player.pos);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 3.0f, IM_COL32(230, 80, 90, 255));
    }

    for (const EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive) continue;
        if (enemy.type == EnemyType::Territory)
        {
            const Vec2 center = WorldToCanvas(canvasPos, canvasSize, enemy.territoryCenter, m_mapZoom, m_player.pos);
            const Vec2 edge = WorldToCanvas(canvasPos, canvasSize,
                Add(enemy.territoryCenter, { enemy.territoryRadius, 0.0f }), m_mapZoom, m_player.pos);
            draw->AddCircleFilled(ImVec2(center.x, center.y), std::fabs(edge.x - center.x), IM_COL32(20, 15, 28, 80));
        }
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, enemy.pos, m_mapZoom, m_player.pos);
        ImU32 color = enemy.telegraphTimer > 0.0f ? IM_COL32(255, 200, 60, 255) : IM_COL32(210, 70, 70, 255);
        if (enemy.chargeTimer > 0.0f) color = IM_COL32(255, 80, 40, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 5.0f, color);
    }

    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos, m_mapZoom, m_player.pos);
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));

    Vec2 faceEnd = WorldToCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 3.0f / m_mapZoom)), m_mapZoom, m_player.pos);
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 1.5f);

    ImGui::End();
}

#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
void SceneNarakuProto::DrawPlayerPositionDebug()
{
    // デバッグウィンドウを表示します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 400.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"プレイヤー位置デバッグ", nullptr, ImGuiWindowFlags_NoNavInputs))
    {
        // プレイヤーの現在位置と高さを表示
        ImGui::Text(u8"現在位置 X: %.3f, Z: %.3f", m_player.pos.x, m_player.pos.y);
        ImGui::Text(u8"現在高さ Y: %.3f (FeetWorldY: %.3f)", GetPlayerAirborneOffset() + GetGroundWorldY(m_player.pos, m_player.depth), m_player.feetWorldY);
        ImGui::Text(u8"現在深度: %.2f", m_player.depth);
        ImGui::Separator();

        // 生成済みマップから小ステージのグリッド座標を計算
        int gridX = -1;
        int gridZ = -1;
        int stageGridSize = 0;
        if (!m_runtimeMap.pieceNames.empty() && !m_runtimeMap.terrainLayers.empty())
        {
            stageGridSize = static_cast<int>(std::sqrt(static_cast<float>(m_runtimeMap.pieceNames.size())));
            if (stageGridSize * stageGridSize == static_cast<int>(m_runtimeMap.pieceNames.size()))
            {
                const NarakuMap::TerrainLayer& firstLayer = m_runtimeMap.terrainLayers.front();
                const float stageWidth = static_cast<float>(firstLayer.gridWidth - 1) * firstLayer.cellSize;
                const float stageHeight = static_cast<float>(firstLayer.gridHeight - 1) * firstLayer.cellSize;
                const float mapHalfWidth = stageWidth * static_cast<float>(stageGridSize) * 0.5f;
                const float mapHalfHeight = stageHeight * static_cast<float>(stageGridSize) * 0.5f;
                if (stageWidth > 0.0f && m_player.pos.x >= -mapHalfWidth && m_player.pos.x <= mapHalfWidth)
                {
                    gridX = std::min(
                        stageGridSize - 1,
                        static_cast<int>((m_player.pos.x + mapHalfWidth) / stageWidth));
                }
                if (stageHeight > 0.0f && m_player.pos.y >= -mapHalfHeight && m_player.pos.y <= mapHalfHeight)
                {
                    gridZ = std::min(
                        stageGridSize - 1,
                        static_cast<int>((m_player.pos.y + mapHalfHeight) / stageHeight));
                }
            }
        }

        ImGui::Text(u8"グリッド座標: (%d, %d)", gridX, gridZ);

        std::string pieceName = u8"不明";
        if (m_runtimeMap.pieceNames.empty())
        {
            pieceName = u8"未生成(再生成してください)";
        }
        else if (gridX >= 0 && gridX < stageGridSize && gridZ >= 0 && gridZ < stageGridSize)
        {
            size_t index = static_cast<size_t>(gridZ * stageGridSize + gridX);
            if (index < m_runtimeMap.pieceNames.size())
            {
                pieceName = m_runtimeMap.pieceNames[index];
            }
        }
        ImGui::Text(u8"小ステージ名: %s", pieceName.c_str());
        ImGui::Separator();

        // デバッグの障害調査として、プレイヤーの現在位置のセル属性も表示
        int currentLayerIndex = FindLayerIndexByDepth(m_player.depth);
        if (currentLayerIndex >= 0 && currentLayerIndex < static_cast<int>(m_runtimeMap.terrainLayers.size()))
        {
            const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[currentLayerIndex];
            float halfWidth = (layer.gridWidth - 1) * layer.cellSize * 0.5f;
            float halfHeight = (layer.gridHeight - 1) * layer.cellSize * 0.5f;
            float relativeX = m_player.pos.x - (layer.center.x - halfWidth);
            float relativeZ = m_player.pos.y - (layer.center.z - halfHeight);
            int cellX = static_cast<int>(std::floor(relativeX / layer.cellSize));
            int cellZ = static_cast<int>(std::floor(relativeZ / layer.cellSize));

            if (cellX >= 0 && cellX < layer.gridWidth - 1 && cellZ >= 0 && cellZ < layer.gridHeight - 1)
            {
                std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                std::string attr = "";
                if (flags & NarakuMap::CellAttributeBlocked) attr += "Blocked ";
                if (flags & NarakuMap::CellAttributeCliffEdge) attr += "CliffEdge ";
                if (flags & NarakuMap::CellAttributeHazard) attr += "Hazard ";
                if (flags & NarakuMap::CellAttributeRemoved) attr += "Removed ";
                if (attr.empty()) attr = "None (Walkable)";
                ImGui::Text(u8"現在セル(%d, %d) 属性: %s", cellX, cellZ, attr.c_str());
            }
            else
            {
                ImGui::Text(u8"現在セル: レイヤー範囲外 (%d, %d)", cellX, cellZ);
            }
        }
        else
        {
            ImGui::Text(u8"現在レイヤー: 不明 (深度 %.2f)", m_player.depth);
        }
    }
    ImGui::End();
}

#endif

void SceneNarakuProto::DrawMiningProgressBar()
{
    if (m_miningIndex < 0) return;

    float barWidth = 260.0f;
    float barHeight = 45.0f;

    // OS画面ではなく、メインウィンドウの作業領域中央を配置基準にします。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(barWidth, barHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("MiningProgressOverlay##Overlay", nullptr, flags))
    {
        std::string text = u8"採掘中...";
        float textWidth = ImGui::CalcTextSize(text.c_str()).x;
        ImGui::SetCursorPosX((barWidth - textWidth) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), text.c_str());

        float progress = std::max(0.0f, std::min(1.0f, 1.0f - (m_miningTimer / m_miningDuration)));
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 18.0f), "");
    }
    ImGui::End();
}

void SceneNarakuProto::DrawFishingProgress() const
{
    if (m_fishingPhase == FishingPhase::None) return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.38f }, ImGuiCond_Always, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 330.0f, 62.0f }, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("FishingProgress##Overlay", nullptr, flags))
    {
        if (m_fishingPhase == FishingPhase::Waiting) ImGui::TextUnformatted(u8"魚の反応を待っています…（Escで中断）");
        else if (m_fishingPhase == FishingPhase::Bite)
        {
            ImGui::TextColored({ 1.0f, 0.8f, 0.15f, 1.0f }, u8"反応あり！ Fキー！");
            const float window = m_fishingPointIndex >= 0 && m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)].lake ? 1.25f : 1.0f;
            ImGui::ProgressBar(std::max(0.0f, m_fishingTimer / window), { -1.0f, 16.0f });
        }
        else ImGui::TextUnformatted(u8"引き上げ中…");
    }
    ImGui::End();
}

void SceneNarakuProto::DrawCenterNotification()
{
    if (m_centerNotificationTimer <= 0.0f || m_centerNotification.empty()) return;

    constexpr float overlayWidth = 420.0f;
    constexpr float overlayHeight = 48.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("CenterNotification##Overlay", nullptr, flags))
    {
        const ImVec2 textSize = ImGui::CalcTextSize(m_centerNotification.c_str());
        ImGui::SetCursorPos(ImVec2(
            std::max(0.0f, (overlayWidth - textSize.x) * 0.5f),
            std::max(0.0f, (overlayHeight - textSize.y) * 0.5f)));
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.22f, 1.0f), "%s", m_centerNotification.c_str());
    }
    ImGui::End();
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetMouseAimGroundPosition()
{
    const POINT mousePos = GetMousePosition();
    HWND hWnd = GetActiveWindow();
    if (!hWnd) hWnd = GetForegroundWindow();
    RECT clientRect{};
    if (hWnd) GetClientRect(hWnd, &clientRect);
    const float screenW = static_cast<float>(std::max(1L, clientRect.right - clientRect.left));
    const float screenH = static_cast<float>(std::max(1L, clientRect.bottom - clientRect.top));

    // スクリーン正規化座標（NDC: -1〜1、画面中央が 0）
    const float ndcX = (static_cast<float>(mousePos.x) / screenW - 0.5f) * 2.0f;
    const float ndcY = (static_cast<float>(mousePos.y) / screenH - 0.5f) * -2.0f;

    // カメラ基準の向き
    const Vec2 camForward = GetCameraForward();
    const Vec2 camRight = GetCameraRight();

    // 画面中心（プレイヤー位置）からマウスカーソル方向へのオフセット
    const float aimRange = kAttackRange * 2.5f;
    return Add(m_player.pos, Add(Mul(camRight, ndcX * aimRange), Mul(camForward, ndcY * aimRange)));
}

void SceneNarakuProto::UpdateAimDirectionFromMouse()
{
    const Vec2 targetGround = GetMouseAimGroundPosition();
    const Vec2 toTarget = Sub(targetGround, m_player.pos);
    if (Distance(targetGround, m_player.pos) > 0.05f)
    {
        m_player.facing = Normalize(toTarget);
    }
}
