/**
 * @file SceneNarakuProto.Town.cpp
 * @brief 所持品画面、街施設、拠点、および取引UIを実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

void SceneNarakuProto::DrawInventory()
{
    // 所持品ウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(160.0f, 80.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(620.0f, 700.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウを開始します。
    NarakuUi::Begin(u8"所持品", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(u8"← [LB] 地図", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Map; m_inventoryMapShowingMap = true; }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), u8"【 所持品 】");
    ImGui::SameLine();
    if (ImGui::Button(u8"設定 [RB] →", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Settings; m_inputSettings.OnOpen(); }
    ImGui::SameLine();
    ImGui::TextDisabled(u8" (LB/RB 切替, [B/Esc] 閉じる)");

    const float currentWeight = GetCurrentWeight();
    const float maxWeight = GetMaxWeight();
    ImGui::Text(u8"重量: %.0f / %.0f  %.0f%%", currentWeight, maxWeight, currentWeight / maxWeight * 100.0f);
    ImGui::Separator();

    // 所持旧器を一覧表示します。
    for (int i = 0; i < static_cast<int>(m_inventory.size()); ++i)
    {
        ImGui::PushID(i);

        // 表示対象の旧器を取得します。
        const RelicItem& item = m_inventory[i];

        // Selectableに渡す表示文字列を作ります。
        char label[128];

        // 未鑑定中は重量だけを見せ、売値は帰還鑑定まで伏せます。
        const std::size_t typeIndex = static_cast<std::size_t>(item.type);
        const bool identified = typeIndex < m_identifiedRelics.size() && m_identifiedRelics[typeIndex];
        if (identified)
        {
            if (item.type == RelicType::Offensive)
                std::snprintf(label, sizeof(label), u8"%s%s  重量 %.0f  使用 %d/%d", GetRelicDisplayName(item), item.broken ? u8"（破損）" : "", item.weight, item.remainingUses, item.maxUses);
            else
                std::snprintf(label, sizeof(label), u8"%s%s  重量 %.0f  売値 %d", GetRelicDisplayName(item), item.broken ? u8"（破損）" : "", item.weight, item.value);
        }
        else
        {
            std::snprintf(label, sizeof(label), u8"%s  重量 %.0f  売値 ?", GetRelicDisplayName(item), item.weight);
        }

        // クリックされた旧器を選択状態にします。
        if (ImGui::Selectable(label, m_selectedInventory == i)) m_selectedInventory = i;

        ImGui::PopID();
    }

    // 有効な旧器が選ばれている時だけ捨てるボタンを出します。
    if (m_selectedInventory >= 0 && m_selectedInventory < static_cast<int>(m_inventory.size()))
    {
        RelicItem& selected = m_inventory[m_selectedInventory];
        if (selected.type == RelicType::MentalRecovery && !selected.broken && ImGui::Button(u8"精神力を回復する"))
        {
            UseMentalRecoveryRelic(m_selectedInventory);
            m_selectedInventory = -1;
            ImGui::End();
            return;
        }
        else if (selected.type == RelicType::Survival)
        {
            if (ImGui::Checkbox(u8"致死時に自動発動", &selected.autoTrigger)) SaveProgress();
        }
        if (selected.type == RelicType::Unique)
            ImGui::TextWrapped(u8"不思議な力が込められている気がする");
        // 選択旧器を現在位置に捨てます。
        if (ImGui::Button(u8"選択した旧器を捨てる"))
        {
            // 所持品から地面旧器へ移します。
            DropInventoryItem(m_selectedInventory);

            // 削除後の添字ズレを避けるため選択を解除します。
            m_selectedInventory = -1;
        }
    }

    ImGui::Separator();
    ImGui::Text(u8"食料: %d（重量 %d）", m_foodCount, m_foodCount);
    if (m_overlayReturnMode != Mode::Surface && m_foodCount > 0 &&
        (m_player.hp < GetMaxHp() || m_fullness < kFullnessMaximum || m_hydration < kHydrationMaximum) &&
        ImGui::Button(u8"食料を使う（HP+20 / 満腹度+10 / 水分+75）"))
    {
        UseFood();
        if (m_foodUseTimer > 0.0f) m_mode = m_overlayReturnMode;
    }
    ImGui::Text(u8"加熱食料: %d（重量 %d）", m_heatedFoodCount, m_heatedFoodCount);
    if (m_overlayReturnMode != Mode::Surface && m_heatedFoodCount > 0 &&
        ImGui::Button(u8"加熱食料を使う（HP+40 / 満腹度+25 / 水分+75 / 精神力+5）"))
    {
        UseHeatedFood();
        if (m_foodUseTimer > 0.0f) m_mode = m_overlayReturnMode;
    }
    if (m_foodCount > 0 && !m_cookingKits.empty() && ImGui::Button(u8"食料を加熱する（60秒 / 料理セット1回）"))
    {
        StartCooking(CookingTarget::HeatFood);
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text(u8"行動食1号: %d（重量 %.0f）", m_rationOneCount, m_rationOneCount * kRationOneWeight);
    if (m_overlayReturnMode != Mode::Surface && m_rationOneCount > 0 &&
        ImGui::Button(u8"行動食1号を使う（満腹度最大 / 5分間減少無効）"))
    {
        UseRationOne();
    }
    if (m_rationFullnessWardTimer > 0.0f)
        ImGui::TextDisabled(u8"満腹度減少無効: %.1f秒", m_rationFullnessWardTimer);
    if (m_rationHydrationPenaltyTimer > 0.0f)
        ImGui::TextDisabled(u8"水分消費1.25倍: %.1f秒", m_rationHydrationPenaltyTimer);

    ImGui::Text(u8"見たことない魚: 生 %d / 調理済み %d（重量 各%.0f）",
        m_rawFishCount, m_cookedFishCount, kUnknownFishWeight);
    if (m_overlayReturnMode != Mode::Surface && m_rawFishCount > 0 &&
        ImGui::Button(u8"魚を生で食べる（満腹度+10 / 精神力+5）")) UseRawFish();
    if (m_overlayReturnMode != Mode::Surface && m_cookedFishCount > 0 &&
        ImGui::Button(u8"調理済みの魚を食べる（満腹度+50 / 精神力+25）")) UseCookedFish();
    if (m_overlayReturnMode != Mode::Surface && m_rawFishCount > 0 && !m_cookingKits.empty() &&
        ImGui::Button(u8"魚を調理する（60秒 / 料理セット1回）"))
    {
        StartCooking(CookingTarget::CookFish);
        ImGui::End();
        return;
    }
    static const char* fishNames[] = { u8"魚（小）", u8"魚（中）", u8"魚（大）" };
    for (int size = 0; size < 3; ++size)
    {
        ImGui::PushID(3000 + size);
        ImGui::Text(u8"%s: 生 %d / 調理済み %d（重量 %.0f）", fishNames[size],
            m_rawSizedFish[static_cast<size_t>(size)], m_cookedSizedFish[static_cast<size_t>(size)], kSizedFishWeights[static_cast<size_t>(size)]);
        if (m_overlayReturnMode != Mode::Surface && m_rawSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"生で食べる"))
            UseSizedFish(static_cast<FishSize>(size), false);
        ImGui::SameLine();
        if (m_overlayReturnMode != Mode::Surface && m_cookedSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"調理済みを食べる"))
            UseSizedFish(static_cast<FishSize>(size), true);
        ImGui::SameLine();
        if (m_overlayReturnMode != Mode::Surface && m_rawSizedFish[static_cast<size_t>(size)] > 0 && !m_cookingKits.empty() && ImGui::SmallButton(u8"調理する"))
        {
            m_cookingFishSize = size;
            StartCooking(CookingTarget::CookSizedFish);
            ImGui::PopID(); ImGui::End(); return;
        }
        ImGui::PopID();
    }
    ImGui::Text(u8"カートリッジ: %d（重量 %.0f）", m_cartridgeCount,
        m_cartridgeCount * kCartridgeWeight);

    ImGui::Separator();
    ImGui::Text(u8"水分: %.1f / 100", m_hydration);
    for (int i = 0; i < static_cast<int>(m_waterBottles.size()); ++i)
    {
        WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(i)];
        ImGui::PushID(1000 + i);
        ImGui::Text(u8"水筒%d: %.0f/100（%s）", i + 1, bottle.amount, GetWaterQualityName(bottle.quality));
        if (bottle.amount > 0.0f && m_hydration < kHydrationMaximum && ImGui::SmallButton(u8"25飲む"))
            DrinkFromBottle(i);
        if (bottle.amount > 0.0f)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"全量捨てる")) DiscardBottleWater(i);
        }
        if (bottle.amount > 0.0f && bottle.quality == WaterQuality::Unboiled && !m_cookingKits.empty())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"煮沸する"))
            {
                StartCooking(CookingTarget::BoilBottle, i);
                ImGui::PopID();
                ImGui::End();
                return;
            }
        }
        ImGui::PopID();
    }
    ImGui::Text(u8"料理セット: %d個", static_cast<int>(m_cookingKits.size()));
    for (int i = 0; i < static_cast<int>(m_cookingKits.size()); ++i)
        ImGui::TextDisabled(u8"  %d: 残り%d/5回", i + 1, m_cookingKits[static_cast<std::size_t>(i)].remainingUses);

    ImGui::Separator();
    auto& accessibleLights = GetAccessibleLights();
    ImGui::Text(u8"ライト: %d個 / %s", static_cast<int>(accessibleLights.size()),
        m_portableLightOn ? u8"点灯中" : u8"消灯中");
    ImGui::BeginDisabled(!m_portableLightOn && !IsPortableLightAvailable());
    if (ImGui::Button(m_portableLightOn ? u8"ライトを消灯" : u8"ライトを点灯")) TogglePortableLight();
    ImGui::EndDisabled();
    for (int i = 0; i < static_cast<int>(accessibleLights.size()); ++i)
    {
        PortableLight& light = accessibleLights[static_cast<std::size_t>(i)];
        ImGui::PushID(5000 + i);
        if (light.broken)
            ImGui::Text(u8"ライト%d（破損 / 重量5 / 売値5G）", i + 1);
        else
            ImGui::Text(u8"ライト%d（残り %.0f:%02.0f / 重量5）", i + 1,
                std::floor(light.remainingSeconds / 60.0f), std::fmod(light.remainingSeconds, 60.0f));
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"捨てる"))
        {
            accessibleLights.erase(accessibleLights.begin() + i);
            if (!IsPortableLightAvailable()) m_portableLightOn = false;
            if (m_overlayReturnMode == Mode::Surface) SaveProgress();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::Button(u8"探窟を放棄して自宅へ戻る")) m_mode = Mode::AbandonConfirm;

    // 所持品ウィンドウを閉じます。
    ImGui::End();
}
void SceneNarakuProto::DrawRelicPrompt()
{
    // メインウィンドウの作業領域中央へ、確認ウィンドウ自身の中央を合わせます。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    // 旧器確認ウィンドウのサイズを固定します。
    ImGui::SetNextWindowSize(ImVec2(420.0f, 180.0f), ImGuiCond_Always);

    // 旧器確認ウィンドウを開始します。
    NarakuUi::Begin(u8"旧器を発見", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // 発見した旧器名を表示します。
    ImGui::Text(u8"%s を発見", GetRelicDisplayName(m_pendingRelic));

    // 発見した旧器の重量を表示します。
    ImGui::Text(u8"重量: %.0f", m_pendingRelic.weight);
    ImGui::Text(u8"活性度: +%d（拾得後 %d）", GetRelicActivity(m_pendingRelic), GetCurrentActivity() + GetRelicActivity(m_pendingRelic));

    // 拾った後の総重量を表示します。
    ImGui::Text(u8"拾得後重量: %.0f / %.0f", GetCurrentWeight() + m_pendingRelic.weight, GetPickupWeightLimit());

    // 150%上限を超えない場合だけ拾うボタンを出します。
    if (GetCurrentWeight() + m_pendingRelic.weight <= GetPickupWeightLimit())
    {
        // 旧器を所持品に入れます。
        if (ImGui::Button(u8"拾う"))
        {
            // 発見中の旧器を所持品へ追加します。
            m_inventory.push_back(m_pendingRelic);
            if (m_pendingRelicMiningIndex >= 0 &&
                m_pendingRelicMiningIndex < static_cast<int>(m_miningPoints.size()))
            {
                m_miningPoints[static_cast<std::size_t>(m_pendingRelicMiningIndex)].outputPending = false;
            }
            m_pendingRelicMiningIndex = -1;

            // 探索モードへ戻ります。
            m_mode = Mode::Explore;

            // HUDログに取得を出します。
            AddMessage(u8"旧器を拾いました。");
            SaveProgress();
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
    if (NarakuUi::BackButton(u8"置いていく"))
    {
        // 発見中の旧器を地面旧器として再登録します。
        m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true,
            m_pendingRelicMiningIndex });
        m_pendingRelicMiningIndex = -1;

        // 探索モードへ戻ります。
        m_mode = Mode::Explore;

        // HUDログに放置を出します。
        AddMessage(u8"旧器をその場に置きました。");
        SaveProgress();
    }

    // 旧器確認ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawReturnConfirm()
{
    ImGui::SetNextWindowPos(ImVec2(460.0f, 230.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 170.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"帰還確認", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"現在の持ち物を持って帰還します。帰還後、未鑑定の遺物は自動で鑑定されます。");
    if (ImGui::Button(u8"帰還する", ImVec2(130.0f, 0.0f))) FinishReturn();
    ImGui::SameLine();
    if (NarakuUi::BackButton(u8"探索を続ける", ImVec2(130.0f, 0.0f))) m_mode = Mode::Explore;
    ImGui::End();
}

void SceneNarakuProto::DrawWaterPrompt()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(430.0f, 190.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"水場", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    const char* waterType = (m_pendingWaterFlags & NarakuMap::CellAttributeWaterLake) != 0u
        ? u8"湖" : ((m_pendingWaterFlags & NarakuMap::CellAttributeWaterPond) != 0u ? u8"池" : u8"溜り");
    ImGui::Text(u8"水場区分: %s", waterType);
    ImGui::Text(u8"現在の水分: %.1f / 100", m_hydration);
    ImGui::TextDisabled(u8"奈落の水は未煮沸です。直接飲むと食中毒の可能性があります。");

    if (m_hydration < kHydrationMaximum && ImGui::Button(u8"直接飲む（水分+25）", ImVec2(190.0f, 0.0f)))
    {
        DrinkWater(kWaterDrinkAmount, WaterQuality::Unboiled, GetWaterFoodPoisoningChance());
        m_mode = Mode::Explore;
    }

    const auto emptyBottle = std::find_if(m_waterBottles.begin(), m_waterBottles.end(),
        [](const WaterBottle& bottle) { return bottle.amount <= 0.0f; });
    if (emptyBottle != m_waterBottles.end() && ImGui::Button(u8"空の水筒へ汲む（100）", ImVec2(190.0f, 0.0f)))
    {
        emptyBottle->amount = kWaterBottleCapacity;
        emptyBottle->quality = WaterQuality::Unboiled;
        emptyBottle->foodPoisoningChance = GetWaterFoodPoisoningChance();
        AddMessage(u8"空の水筒へ未煮沸の水を汲みました。");
        m_mode = Mode::Explore;
    }
    if (emptyBottle == m_waterBottles.end()) ImGui::TextDisabled(u8"空の水筒がありません。");

    if (NarakuUi::BackButton(u8"戻る")) m_mode = Mode::Explore;
    ImGui::End();
}

void SceneNarakuProto::DrawAbandonConfirm()
{
    ImGui::SetNextWindowPos(ImVec2(440.0f, 220.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440.0f, 190.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"探窟放棄の確認", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"今回持ち込んだ物と取得物、帰還報酬を失い、レベル進行が0.5戻ります。満腹度と水分は維持されます。");
    if (ImGui::Button(u8"放棄する", ImVec2(130.0f, 0.0f))) AbandonDive();
    ImGui::SameLine();
    if (NarakuUi::BackButton(u8"戻る", ImVec2(130.0f, 0.0f))) m_mode = Mode::Inventory;
    ImGui::End();
}

void SceneNarakuProto::DrawCurrentStatus()
{
    if (!ImGui::CollapsingHeader(u8"現在のステータス", ImGuiTreeNodeFlags_DefaultOpen)) return;

    const EquipmentBonus equipment = GetEquipmentBonus();
    ImGui::Text(u8"装備  頭:%s / 胴:%s / 武器:%s", GetArmorName(m_equippedHeadArmor),
        GetArmorName(m_equippedBodyArmor), GetWeaponName(m_equippedWeapon));
    ImGui::Text(u8"Lv%d  HP %.0f / %.0f", m_level, m_player.hp, GetMaxHp());
    ImGui::Text(u8"スタミナ %.0f / %.0f  精神力 %.0f / %.0f", m_player.stamina, GetMaxStamina(), m_player.mental, GetMaxMental());
    ImGui::Text(u8"満腹度 %.0f / 100  水分 %.0f / 100  重量 %.0f / %.0f", m_fullness, m_hydration, GetCurrentWeight(), GetMaxWeight());
    ImGui::Text(u8"攻撃力 %.2f  防御倍率 %.0f%%", GetAttackPower(), GetDefenseMultiplier() * 100.0f);
    ImGui::Text(u8"歩行 %.2fm/s  走行 %.2fm/s", GetMoveSpeed(), GetRunSpeed());
    ImGui::Text(u8"スタミナ回復 %.2f/s  精神遺物回復 %.2f", m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier(),
        10.0f * GetMentalRecoveryMultiplier());
    ImGui::Text(u8"採掘速度 %.0f%%  ロープ上昇 %.2f / 降下 %.2f",
        GetMiningSpeedMultiplier() * 100.0f, GetRopeSpeed(true), GetRopeSpeed(false));
    ImGui::Text(u8"装備HP回復 %.2f/s", equipment.hpRecoveryPerSecond);
}

void SceneNarakuProto::DrawTownNavigation()
{
    ImGui::Separator();
    ImGui::TextUnformatted(u8"地上施設");
    const auto drawDestination = [this](const char* label, Mode destination)
    {
        const bool locked = destination == Mode::QuestDesk && !IsQuestDeskUnlocked();
        ImGui::BeginDisabled(m_mode == destination || locked);
        if (ImGui::Button(label, ImVec2(120.0f, 0.0f))) m_mode = destination;
        ImGui::EndDisabled();
        if (locked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(u8"第一層上層へ入ると解放されます。");
    };
    drawDestination(u8"自宅", Mode::Home);
    ImGui::SameLine();
    drawDestination(u8"商店", Mode::GeneralShop);
    ImGui::SameLine();
    drawDestination(u8"武具屋", Mode::Armory);
    ImGui::SameLine();
    drawDestination(u8"レストラン", Mode::Restaurant);
    ImGui::SameLine();
    drawDestination(u8"依頼受付", Mode::QuestDesk);
}

void SceneNarakuProto::DrawTownFrame(
    const char* title,
    const std::function<void()>& drawCenterContent,
    Mode returnMode,
    const char* returnLabel)
{
    // ###TownMainWindow を指定することで、タイトルバーの施設名は切り替えつつ、
    // ImGui内部のウィンドウIDを共通化して場所切り替え時の位置・サイズ変動を防止する
    char windowTitleWithId[256];
    std::snprintf(windowTitleWithId, sizeof(windowTitleWithId), "%s###TownMainWindow",
        (title != nullptr && title[0] != '\0') ? title : u8"拠点");

    ImGui::SetNextWindowPos(ImVec2(100.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1180.0f, 760.0f), ImGuiCond_FirstUseEver);
    if (!NarakuUi::Begin(windowTitleWithId, nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    if (NarakuUi::BackButton(returnLabel, ImVec2(140.0f, 0.0f))) m_mode = returnMode;
    if (m_mode == Mode::Restaurant || m_mode == Mode::QuestDesk)
    {
        ImGui::SameLine();
        ImGui::BeginDisabled(m_mode == Mode::Restaurant && !IsQuestDeskUnlocked());
        if (ImGui::Button(m_mode == Mode::Restaurant ? u8"依頼受付タブ" : u8"レストランタブ",
            ImVec2(150.0f, 0.0f)))
            m_mode = m_mode == Mode::Restaurant ? Mode::QuestDesk : Mode::Restaurant;
        ImGui::EndDisabled();
        if (m_mode == Mode::Restaurant && !IsQuestDeskUnlocked() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(u8"第一層上層へ到達すると解放されます。");
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("Town3PaneTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("LeftPane", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("CenterPane", ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn("RightPane", ImGuiTableColumnFlags_WidthStretch, 0.30f);

        ImGui::TableNextRow();

        // 左ペイン (30%): 装備情報・次回持ち込み品
        ImGui::TableSetColumnIndex(0);
        DrawTownLeftPane();

        // 中央ペイン (40%): 各施設情報
        ImGui::TableSetColumnIndex(1);
        if (drawCenterContent)
        {
            drawCenterContent();
        }

        // 右ペイン (30%): プレイヤーステータス表
        ImGui::TableSetColumnIndex(2);
        DrawTownRightPane();

        ImGui::EndTable();
    }

    ImGui::End();

    DrawShopTransactionModal();
    DrawArmoryPurchaseModal();
}

void SceneNarakuProto::DrawTownLeftPane()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【装備情報】");
    ImGui::Separator();
    ImGui::Text(u8"武器: %s", GetWeaponName(m_equippedWeapon));
    ImGui::Text(u8"頭  : %s", GetArmorName(m_equippedHeadArmor));
    ImGui::Text(u8"胴体: %s", GetArmorName(m_equippedBodyArmor));
    if (HasRelicArmorSetEffect())
    {
        ImGui::TextColored(ImVec4(0.75f, 0.55f, 1.0f, 1.0f), u8"※遺物装備セット発動中");
    }
    if (HasUnknownArmorSetEffect())
    {
        ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.65f, 1.0f), u8"※未知の装備セット発動中");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【次回持ち込みアイテム】");
    ImGui::Separator();

    if (ImGui::BeginChild("TownLoadoutItemsList", ImVec2(0.0f, 320.0f), true))
    {
        bool hasItem = false;
        if (m_loadoutFoodCount > 0)
        {
            ImGui::BulletText(u8"食料 x %d", m_loadoutFoodCount);
            hasItem = true;
        }
        if (m_loadoutHeatedFoodCount > 0)
        {
            ImGui::BulletText(u8"加熱食料 x %d", m_loadoutHeatedFoodCount);
            hasItem = true;
        }
        if (m_loadoutRationOneCount > 0) { ImGui::BulletText(u8"行動食1号 x %d", m_loadoutRationOneCount); hasItem = true; }
        if (m_loadoutRawFishCount > 0) { ImGui::BulletText(u8"見たことない魚 x %d", m_loadoutRawFishCount); hasItem = true; }
        if (m_loadoutCookedFishCount > 0) { ImGui::BulletText(u8"調理済みの魚 x %d", m_loadoutCookedFishCount); hasItem = true; }
        if (m_loadoutCartridgeCount > 0) { ImGui::BulletText(u8"カートリッジ x %d", m_loadoutCartridgeCount); hasItem = true; }
        for (std::size_t i = 0; i < m_storedWaterBottles.size(); ++i)
        {
            const auto& bottle = m_storedWaterBottles[i];
            if (bottle.selectedForLoadout)
            {
                ImGui::BulletText(u8"水筒%d (水量%.0f, %s)", static_cast<int>(i + 1), bottle.amount, GetWaterQualityName(bottle.quality));
                hasItem = true;
            }
        }
        for (std::size_t i = 0; i < m_storedCookingKits.size(); ++i)
        {
            const auto& kit = m_storedCookingKits[i];
            if (kit.selectedForLoadout)
            {
                ImGui::BulletText(u8"料理セット%d (残%d回)", static_cast<int>(i + 1), kit.remainingUses);
                hasItem = true;
            }
        }
        for (std::size_t i = 0; i < m_storedPortableLights.size(); ++i)
        {
            const auto& light = m_storedPortableLights[i];
            if (light.selectedForLoadout)
            {
                ImGui::BulletText(u8"ライト%d (%s, 残%.0f秒)", static_cast<int>(i + 1),
                    light.broken ? u8"破損" : u8"使用可", light.remainingSeconds);
                hasItem = true;
            }
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
        {
            if (m_loadoutRelics[i] > 0)
            {
                ImGui::BulletText(u8"%s x %d", GetRelicTypeName(static_cast<RelicType>(i)), m_loadoutRelics[i]);
                hasItem = true;
            }
        }
        if (!hasItem)
        {
            ImGui::TextDisabled(u8"（持ち込みアイテムなし）");
        }
    }
    ImGui::EndChild();

    float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount + m_loadoutHeatedFoodCount);
    loadoutWeight += static_cast<float>(m_loadoutRationOneCount) * kRationOneWeight;
    loadoutWeight += static_cast<float>(m_loadoutRawFishCount + m_loadoutCookedFishCount) * kUnknownFishWeight;
    for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
        loadoutWeight += static_cast<float>(m_loadoutRawSizedFish[i] + m_loadoutCookedSizedFish[i]) * kSizedFishWeights[i];
    loadoutWeight += static_cast<float>(m_loadoutCartridgeCount) * kCartridgeWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
        [](const WaterBottle& b) { return b.selectedForLoadout; })) * kWaterBottleWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedCookingKits.begin(), m_storedCookingKits.end(),
        [](const CookingKit& k) { return k.selectedForLoadout; })) * kCookingKitWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedPortableLights.begin(), m_storedPortableLights.end(),
        [](const PortableLight& light) { return light.selectedForLoadout; })) * kPortableLightWeight;
    for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
    {
        loadoutWeight += GetRelicWeight(static_cast<RelicType>(i)) * static_cast<float>(m_loadoutRelics[i]);
    }
    const float maxWeight = GetMaxWeight();
    ImGui::Spacing();
    ImGui::Text(u8"開始重量: %.0f / %.0f", loadoutWeight, maxWeight);
    if (loadoutWeight > maxWeight)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), u8"※重量制限オーバー");
    }
}

void SceneNarakuProto::DrawTownRightPane()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【プレイヤー情報】");
    ImGui::Separator();

    if (ImGui::BeginTable("PlayerStatusTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(u8"項目", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn(u8"内容", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto addRow = [](const char* label, const char* value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value);
        };

        char buf[128];

        std::snprintf(buf, sizeof(buf), "%d", m_level);
        addRow(u8"Lv", buf);

        addRow(u8"階級", GetRankName(m_adventurerRank));

        std::snprintf(buf, sizeof(buf), "%d G", m_money);
        addRow(u8"所持金", buf);

        std::snprintf(buf, sizeof(buf), "%.0f (%.0f)", GetMaxHp(), m_player.hp);
        addRow(u8"最大HP(現在HP)", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", GetMaxStamina());
        addRow(u8"最大スタミナ", buf);

        std::snprintf(buf, sizeof(buf), "%.0f (%.0f)", GetMaxMental(), m_player.mental);
        addRow(u8"最大精神力(現在精神力)", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", GetMaxWeight());
        addRow(u8"最大重量", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", m_fullness);
        addRow(u8"満腹度", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", m_hydration);
        addRow(u8"水分", buf);

        std::snprintf(buf, sizeof(buf), "%.1f", GetAttackPower());
        addRow(u8"攻撃力", buf);

        std::snprintf(buf, sizeof(buf), "%.0f%%", GetDefenseMultiplier() * 100.0f);
        addRow(u8"防御力", buf);

        std::snprintf(buf, sizeof(buf), "%.1f /s", m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier());
        addRow(u8"スタミナ回復速度", buf);

        std::snprintf(buf, sizeof(buf), "%.0f%%", GetMiningSpeedMultiplier() * 100.0f);
        addRow(u8"採掘速度", buf);

        std::snprintf(buf, sizeof(buf), "%.2fm/s : %.2fm/s", GetRopeSpeed(true), GetRopeSpeed(false));
        addRow(u8"ロープ昇降速度", buf);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    static const char* kWeekdayNames[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
    const int weekSecond = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
    const int day = weekSecond / static_cast<int>(kGameDaySeconds);
    const int daySecond = weekSecond % static_cast<int>(kGameDaySeconds);
    const int hour = daySecond / 3600;
    const int minute = (daySecond % 3600) / 60;

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【日時】");
    ImGui::SameLine();
    ImGui::Text(u8"%s曜日 %02d:%02d", kWeekdayNames[day], hour, minute);
    if (m_weekResetPending)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"※次回潜行時に新しい週へ更新されます");
    }
}

void SceneNarakuProto::DrawHome()
{
    DrawTownFrame(u8"自宅 - 潜行準備", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        if (m_uniqueRelicReturned)
            ImGui::TextWrapped(u8"欲望の揺籃: 図鑑登録・実績解除済み / 物語の進行条件を達成");
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【装備選択】");
        ImGui::Text(u8"頭装備:");
        for (std::size_t i = 0; i < m_ownedHeadArmor.size(); ++i)
        {
            if (!m_ownedHeadArmor[i]) continue;
            const ArmorTier tier = static_cast<ArmorTier>(i);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::RadioButton(GetArmorName(tier), m_equippedHeadArmor == tier)) { m_equippedHeadArmor = tier; SaveProgress(); }
            ImGui::SameLine();
            ImGui::TextDisabled(u8"%s", GetArmorEffectText(tier, true));
            ImGui::PopID();
        }

        ImGui::Text(u8"胴装備:");
        for (std::size_t i = 0; i < m_ownedBodyArmor.size(); ++i)
        {
            if (!m_ownedBodyArmor[i]) continue;
            const ArmorTier tier = static_cast<ArmorTier>(i);
            ImGui::PushID(100 + static_cast<int>(i));
            if (ImGui::RadioButton(GetArmorName(tier), m_equippedBodyArmor == tier)) { m_equippedBodyArmor = tier; SaveProgress(); }
            ImGui::SameLine();
            ImGui::TextDisabled(u8"%s", GetArmorEffectText(tier, false));
            ImGui::PopID();
        }

        ImGui::Text(u8"武器:");
        for (std::size_t i = 0; i < m_ownedWeapons.size(); ++i)
        {
            if (!m_ownedWeapons[i]) continue;
            const WeaponTier tier = static_cast<WeaponTier>(i);
            ImGui::PushID(200 + static_cast<int>(i));
            if (ImGui::RadioButton(GetWeaponName(tier), m_equippedWeapon == tier)) { m_equippedWeapon = tier; SaveProgress(); }
            ImGui::SameLine();
            ImGui::TextDisabled(u8"%s", GetWeaponEffectText(tier));
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【持ち込み品調整】");
        ImGui::Text(u8"食料  保管:%d  持込:%d", m_storedFoodCount, m_loadoutFoodCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"-##food")) { m_loadoutFoodCount = std::max(0, m_loadoutFoodCount - 1); SaveProgress(); }
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"+##food")) { m_loadoutFoodCount = std::min(m_storedFoodCount, m_loadoutFoodCount + 1); SaveProgress(); }

        ImGui::Text(u8"加熱食料  保管:%d  持込:%d", m_storedHeatedFoodCount, m_loadoutHeatedFoodCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"-##heatedFood")) { m_loadoutHeatedFoodCount = std::max(0, m_loadoutHeatedFoodCount - 1); SaveProgress(); }
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"+##heatedFood")) { m_loadoutHeatedFoodCount = std::min(m_storedHeatedFoodCount, m_loadoutHeatedFoodCount + 1); SaveProgress(); }

        auto drawCountLoadout = [this](const char* label, const char* id, int stored, int& loadout)
        {
            ImGui::Text(u8"%s  保管:%d  持込:%d", label, stored, loadout);
            ImGui::SameLine();
            std::string minus = std::string("-##") + id;
            if (ImGui::SmallButton(minus.c_str())) { loadout = std::max(0, loadout - 1); SaveProgress(); }
            ImGui::SameLine();
            std::string plus = std::string("+##") + id;
            if (ImGui::SmallButton(plus.c_str())) { loadout = std::min(stored, loadout + 1); SaveProgress(); }
        };
        drawCountLoadout(u8"行動食1号", "rationOne", m_storedRationOneCount, m_loadoutRationOneCount);
        drawCountLoadout(u8"見たことない魚", "rawFish", m_storedRawFishCount, m_loadoutRawFishCount);
        drawCountLoadout(u8"調理済みの魚", "cookedFish", m_storedCookedFishCount, m_loadoutCookedFishCount);
        static const char* sizedFishNames[] = { u8"魚（小）", u8"魚（中）", u8"魚（大）" };
        for (int size = 0; size < 3; ++size)
        {
            const std::string rawId = "rawSizedFish" + std::to_string(size);
            const std::string cookedId = "cookedSizedFish" + std::to_string(size);
            drawCountLoadout((std::string(sizedFishNames[size]) + u8" 生").c_str(), rawId.c_str(),
                m_storedRawSizedFish[static_cast<size_t>(size)], m_loadoutRawSizedFish[static_cast<size_t>(size)]);
            drawCountLoadout((std::string(sizedFishNames[size]) + u8" 調理済み").c_str(), cookedId.c_str(),
                m_storedCookedSizedFish[static_cast<size_t>(size)], m_loadoutCookedSizedFish[static_cast<size_t>(size)]);
        }
        drawCountLoadout(u8"カートリッジ", "cartridge", m_storedCartridgeCount, m_loadoutCartridgeCount);

        for (int i = 0; i < static_cast<int>(m_storedWaterBottles.size()); ++i)
        {
            WaterBottle& bottle = m_storedWaterBottles[static_cast<std::size_t>(i)];
            ImGui::PushID(700 + i);
            if (ImGui::Checkbox(u8"##bottleLoadout", &bottle.selectedForLoadout)) SaveProgress();
            ImGui::SameLine();
            ImGui::Text(u8"水筒%d %.0f/100（%s） 重量15", i + 1, bottle.amount, GetWaterQualityName(bottle.quality));
            ImGui::PopID();
        }
        for (int i = 0; i < static_cast<int>(m_storedCookingKits.size()); ++i)
        {
            CookingKit& kit = m_storedCookingKits[static_cast<std::size_t>(i)];
            ImGui::PushID(800 + i);
            if (ImGui::Checkbox(u8"##kitLoadout", &kit.selectedForLoadout)) SaveProgress();
            ImGui::SameLine();
            ImGui::Text(u8"料理セット%d 残り%d/5回 重量5", i + 1, kit.remainingUses);
            ImGui::PopID();
        }
        for (int i = 0; i < static_cast<int>(m_storedPortableLights.size()); ++i)
        {
            PortableLight& light = m_storedPortableLights[static_cast<std::size_t>(i)];
            ImGui::PushID(900 + i);
            if (ImGui::Checkbox(u8"##lightLoadout", &light.selectedForLoadout)) SaveProgress();
            ImGui::SameLine();
            ImGui::Text(u8"ライト%d %s 残り%.0f秒 重量5", i + 1,
                light.broken ? u8"（破損）" : "", light.remainingSeconds);
            ImGui::PopID();
        }

        for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
        {
            const RelicType type = static_cast<RelicType>(i);
            ImGui::PushID(300 + static_cast<int>(i));
            const int storedCount = CountStoredRelics(type);
            ImGui::Text(u8"%s  保管:%d  持込:%d", GetRelicTypeName(type), storedCount, m_loadoutRelics[i]);
            ImGui::SameLine();
            if (ImGui::SmallButton("-")) { m_loadoutRelics[i] = std::max(0, m_loadoutRelics[i] - 1); SaveProgress(); }
            ImGui::SameLine();
            if (ImGui::SmallButton("+")) { m_loadoutRelics[i] = std::min(storedCount, m_loadoutRelics[i] + 1); SaveProgress(); }
            ImGui::PopID();
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
        for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
        {
            const RelicType type = static_cast<RelicType>(i);
            loadoutWeight += GetRelicWeight(type) * static_cast<float>(m_loadoutRelics[i]);
        }
        const float maxWeight = GetMaxWeight();

        ImGui::Separator();
        if (ImGui::Button(u8"任意保存", ImVec2(120.0f, 0.0f)))
            ShowCenterNotification(SaveProgress() ? u8"保存しました。" : u8"保存に失敗しました。");
    });
}

void SceneNarakuProto::DrawGeneralShop()
{
    DrawTownFrame(u8"商店", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();

        if (NarakuUi::BeginTabBar("ShopTabs"))
        {
            if (ImGui::BeginTabItem(u8"購入"))
            {
                ImGui::Spacing();
                auto drawBuyRow = [this](const char* name, int price, const char* desc, ShopTransactionMode mode, RelicType rType = RelicType::CashLow)
                {
                    ImGui::Text(u8"%s  %d G", name, price);
                    if (desc != nullptr && desc[0] != '\0')
                    {
                        ImGui::TextDisabled(u8"  %s", desc);
                    }
                    ImGui::SameLine();
                    char btnLabel[64];
                    std::snprintf(btnLabel, sizeof(btnLabel), u8"購入##%s", name);
                    const int maxAffordable = price > 0 ? (m_money / price) : 0;
                    ImGui::BeginDisabled(maxAffordable <= 0);
                    if (ImGui::SmallButton(btnLabel))
                    {
                        m_shopTransaction.mode = mode;
                        m_shopTransaction.relicType = rType;
                        m_shopTransaction.itemName = name;
                        m_shopTransaction.unitPrice = price;
                        m_shopTransaction.maxCount = maxAffordable;
                        m_shopTransaction.count = 1;
                        m_shopTransaction.openModal = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                };

                drawBuyRow(u8"食料", kFoodPrice, u8"重量1 / HP+20 / 満腹度+10 / 水分+75", ShopTransactionMode::BuyFood);
                drawBuyRow(u8"水筒", kWaterBottlePrice, u8"重量15 / 容量100", ShopTransactionMode::BuyWaterBottle);
                drawBuyRow(u8"料理セット", kCookingKitPrice, u8"重量5 / 5回使用", ShopTransactionMode::BuyCookingKit);
                drawBuyRow(u8"ライト", kPortableLightPrice, u8"重量5 / 点灯可能時間15分", ShopTransactionMode::BuyPortableLight);

                const RelicType shopTypes[] = { RelicType::ArmamentUpgrade, RelicType::WeaponUpgrade, RelicType::ArmorUpgrade };
                const int shopPrices[] = { 600, 700, 700 };
                for (int i = 0; i < 3; ++i)
                {
                    drawBuyRow(GetRelicTypeName(shopTypes[i]), shopPrices[i], u8"強化用遺物素材", ShopTransactionMode::BuyRelic, shopTypes[i]);
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(u8"売却"))
            {
                ImGui::Spacing();
                bool hasSellable = false;
                for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
                {
                    const RelicType type = static_cast<RelicType>(i);
                    const int count = static_cast<int>(std::count_if(m_storedInventory.begin(), m_storedInventory.end(),
                        [type, this](const RelicItem& item) { return item.type == type && IsRelicSellable(item); }));
                    if (count <= 0) continue;
                    hasSellable = true;

                    auto sellIt = std::find_if(m_storedInventory.begin(), m_storedInventory.end(),
                        [type, this](const RelicItem& item) { return item.type == type && IsRelicSellable(item); });
                    const int saleValue = sellIt != m_storedInventory.end() ? sellIt->value : 0;

                    ImGui::Text(u8"%s (所持: %d個)  1個 %d G", GetRelicTypeName(type), count, saleValue);
                    ImGui::SameLine();
                    char btnLabel[64];
                    std::snprintf(btnLabel, sizeof(btnLabel), u8"売却##relic%d", static_cast<int>(i));
                    if (ImGui::SmallButton(btnLabel))
                    {
                        m_shopTransaction.mode = ShopTransactionMode::SellRelic;
                        m_shopTransaction.relicType = type;
                        m_shopTransaction.itemName = GetRelicTypeName(type);
                        m_shopTransaction.unitPrice = saleValue;
                        m_shopTransaction.maxCount = count;
                        m_shopTransaction.count = 1;
                        m_shopTransaction.openModal = true;
                    }
                    ImGui::Separator();
                }

                const int usableLightCount = static_cast<int>(std::count_if(
                    m_storedPortableLights.begin(), m_storedPortableLights.end(),
                    [](const PortableLight& light) { return !light.broken; }));
                const int brokenLightCount = static_cast<int>(m_storedPortableLights.size()) - usableLightCount;
                if (usableLightCount > 0 || brokenLightCount > 0) hasSellable = true;
                auto openLightSale = [this](const char* name, int count, int price, ShopTransactionMode mode)
                {
                    if (count <= 0) return;
                    ImGui::PushID(static_cast<int>(mode));
                    ImGui::Text(u8"%s (所持: %d個)  1個 %d G", name, count, price);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(u8"売却"))
                    {
                        m_shopTransaction.mode = mode;
                        m_shopTransaction.itemName = name;
                        m_shopTransaction.unitPrice = price;
                        m_shopTransaction.maxCount = count;
                        m_shopTransaction.count = 1;
                        m_shopTransaction.openModal = true;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                };
                openLightSale(u8"ライト", usableLightCount, kPortableLightSellPrice,
                    ShopTransactionMode::SellPortableLight);
                openLightSale(u8"ライト（破損）", brokenLightCount, kBrokenPortableLightSellPrice,
                    ShopTransactionMode::SellBrokenPortableLight);

                if (!hasSellable)
                {
                    ImGui::TextDisabled(u8"売却できる遺物はありません。");
                }
                ImGui::Spacing();
                ImGui::TextDisabled(u8"※食料は売却できません。");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    });
}

void SceneNarakuProto::DrawShopTransactionModal()
{
    if (m_shopTransaction.openModal)
    {
        ImGui::OpenPopup(u8"取引個数選択##ShopModal");
        m_shopTransaction.openModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 320.0f), ImGuiCond_Always);
    if (NarakuUi::BeginPopupModal(u8"取引個数選択##ShopModal", nullptr, ImGuiWindowFlags_NoResize))
    {
        const bool isBuy = m_shopTransaction.mode != ShopTransactionMode::SellRelic &&
            m_shopTransaction.mode != ShopTransactionMode::SellPortableLight &&
            m_shopTransaction.mode != ShopTransactionMode::SellBrokenPortableLight;
        ImGui::Text(u8"【%s】 %s", isBuy ? u8"購入" : u8"売却", m_shopTransaction.itemName.c_str());
        ImGui::Text(u8"単価: %d G  /  %s可能上限: %d 個",
            m_shopTransaction.unitPrice, isBuy ? u8"購入" : u8"売却", m_shopTransaction.maxCount);
        ImGui::Separator();

        auto clampQuantity = [this](int value) -> int {
            const int upper = std::max(1, m_shopTransaction.maxCount);
            return std::max(1, std::min(value, upper));
        };

        ImGui::Text(u8"数量:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("##ShopQuantity", &m_shopTransaction.count))
        {
            m_shopTransaction.count = clampQuantity(m_shopTransaction.count);
        }

        const int deltas[] = { -100, -10, -1, 1, 10, 100 };
        for (int i = 0; i < 6; ++i)
        {
            if (i > 0) ImGui::SameLine();
            char btnText[16];
            std::snprintf(btnText, sizeof(btnText), "%+d", deltas[i]);
            if (ImGui::Button(btnText, ImVec2(58.0f, 0.0f)))
            {
                m_shopTransaction.count = clampQuantity(m_shopTransaction.count + deltas[i]);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        const int totalCost = m_shopTransaction.count * m_shopTransaction.unitPrice;
        const int expectedMoney = isBuy ? (m_money - totalCost) : (m_money + totalCost);
        ImGui::Text(u8"合計金額: %d G", totalCost);
        ImGui::Text(u8"所持金  : %d G  →  %d G (%s%d G)",
            m_money, expectedMoney, isBuy ? "-" : "+", totalCost);

        ImGui::Spacing();
        ImGui::Separator();

        const bool canExecute = (m_shopTransaction.maxCount > 0) &&
                                (m_shopTransaction.count >= 1) &&
                                (m_shopTransaction.count <= m_shopTransaction.maxCount) &&
                                (!isBuy || m_money >= totalCost);

        ImGui::BeginDisabled(!canExecute);
        if (ImGui::Button(isBuy ? u8"購入する" : u8"売却する", ImVec2(150.0f, 0.0f)))
        {
            ExecuteShopTransaction();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (NarakuUi::BackButton(u8"キャンセル", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SceneNarakuProto::ExecuteShopTransaction()
{
    const int count = m_shopTransaction.count;
    if (count <= 0) return;

    const int totalCost = count * m_shopTransaction.unitPrice;

    switch (m_shopTransaction.mode)
    {
    case ShopTransactionMode::BuyFood:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            m_storedFoodCount += count;
            SaveProgress();
            ShowCenterNotification(u8"食料を購入しました。");
        }
        break;

    case ShopTransactionMode::BuyWaterBottle:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            for (int i = 0; i < count; ++i)
            {
                m_storedWaterBottles.push_back(WaterBottle());
            }
            SaveProgress();
            ShowCenterNotification(u8"水筒を購入しました。");
        }
        break;

    case ShopTransactionMode::BuyCookingKit:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            for (int i = 0; i < count; ++i)
            {
                m_storedCookingKits.push_back(CookingKit());
            }
            SaveProgress();
            ShowCenterNotification(u8"料理セットを購入しました。");
        }
        break;

    case ShopTransactionMode::BuyPortableLight:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            for (int i = 0; i < count; ++i) m_storedPortableLights.push_back(PortableLight());
            SaveProgress();
            ShowCenterNotification(u8"ライトを購入しました。");
        }
        break;

    case ShopTransactionMode::BuyRelic:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            const std::size_t typeIndex = static_cast<std::size_t>(m_shopTransaction.relicType);
            for (int i = 0; i < count; ++i)
            {
                RelicItem item = CreateRelic(m_shopTransaction.relicType, GetRelicTypeName(m_shopTransaction.relicType));
                item.stabilized = true;
                m_storedInventory.push_back(item);
            }
            m_identifiedRelics[typeIndex] = true;
            SaveProgress();
            ShowCenterNotification(u8"遺物を購入しました。");
        }
        break;

    case ShopTransactionMode::SellRelic:
    {
        m_money += totalCost;
        int remainingToSell = count;
        const RelicType rType = m_shopTransaction.relicType;
        for (auto it = m_storedInventory.begin(); it != m_storedInventory.end() && remainingToSell > 0;)
        {
            if (it->type == rType && IsRelicSellable(*it))
            {
                it = m_storedInventory.erase(it);
                --remainingToSell;
            }
            else
            {
                ++it;
            }
        }
        const std::size_t typeIndex = static_cast<std::size_t>(rType);
        m_loadoutRelics[typeIndex] = std::min(m_loadoutRelics[typeIndex], CountStoredRelics(rType));
        SaveProgress();
        ShowCenterNotification(u8"遺物を売却しました。");
        break;
    }

    case ShopTransactionMode::SellPortableLight:
    case ShopTransactionMode::SellBrokenPortableLight:
    {
        const bool sellBroken = m_shopTransaction.mode == ShopTransactionMode::SellBrokenPortableLight;
        int remaining = count;
        for (auto it = m_storedPortableLights.begin(); it != m_storedPortableLights.end() && remaining > 0;)
        {
            if (it->broken == sellBroken) { it = m_storedPortableLights.erase(it); --remaining; }
            else ++it;
        }
        m_money += (count - remaining) * m_shopTransaction.unitPrice;
        SaveProgress();
        ShowCenterNotification(u8"ライトを売却しました。");
        break;
    }

    default:
        break;
    }
}

void SceneNarakuProto::DrawArmory()
{
    static const int armorDiscountPrices[][2] = { { 10, 15 }, { 150, 200 }, { 200, 300 }, { 250, 350 }, { 350, 400 }, { 5000, 6000 } };
    static const int armorMoneyPrices[][2] = { { 10, 15 }, { 150, 200 }, { 1000, 1500 }, { 1250, 1750 }, { 1750, 2000 }, { 25000, 30000 } };
    static const int armorArmamentCosts[][2] = { { 0, 0 }, { 0, 0 }, { 3, 4 }, { 4, 6 }, { 7, 10 }, { 17, 19 } };
    static const int armorMaterialCosts[][2] = { { 0, 0 }, { 0, 0 }, { 5, 7 }, { 7, 9 }, { 5, 7 }, { 15, 19 } };
    static const int weaponDiscountPrices[] = { 5, 500, 1250, 1500, 5000 };
    static const int weaponMoneyPrices[] = { 5, 500, 1250, 1500, 25000 };
    static const int weaponArmamentCosts[] = { 0, 0, 0, 0, 11 };
    static const int weaponMaterialCosts[] = { 0, 0, 0, 0, 21 };

    DrawTownFrame(u8"武具屋", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Text(u8"所持素材  武具強化:%d  武器強化:%d  防具強化:%d",
            CountStoredRelics(RelicType::ArmamentUpgrade),
            CountStoredRelics(RelicType::WeaponUpgrade),
            CountStoredRelics(RelicType::ArmorUpgrade));
        ImGui::Separator();

        if (NarakuUi::BeginTabBar("ArmoryCategoryTabs"))
        {
            // === 防具タブ ===
            if (ImGui::BeginTabItem(u8"防具"))
            {
                if (ImGui::BeginTable("ArmorSplitTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("ArmorList", ImGuiTableColumnFlags_WidthStretch, 0.46f);
                    ImGui::TableSetupColumn("ArmorDetail", ImGuiTableColumnFlags_WidthStretch, 0.54f);
                    ImGui::TableNextRow();

                    // --- 左側: 商品一覧 ---
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【防具一覧】");
                    ImGui::Separator();

                    if (ImGui::BeginChild("ArmorListChild", ImVec2(0.0f, 480.0f), false))
                    {
                        for (std::size_t i = 0; i < static_cast<std::size_t>(ArmorTier::Unknown); ++i)
                        {
                            const ArmorTier tier = static_cast<ArmorTier>(i);
                            const bool headOwned = m_ownedHeadArmor[i];
                            const bool bodyOwned = m_ownedBodyArmor[i];
                            const bool isSelected = (m_armoryUI.selectedArmorIndex == static_cast<int>(i));

                            ImGui::PushID(static_cast<int>(i));

                            const char* statusText = (headOwned && bodyOwned) ? u8" [一式所有]" :
                                                     (headOwned ? u8" [頭所有]" :
                                                     (bodyOwned ? u8" [胴所有]" : ""));
                            char itemHeader[128];
                            std::snprintf(itemHeader, sizeof(itemHeader), "%s%s", GetArmorName(tier), statusText);

                            if (ImGui::Selectable(itemHeader, isSelected))
                            {
                                m_armoryUI.selectedArmorIndex = static_cast<int>(i);
                            }

                            if (ImGui::SmallButton(u8"購入"))
                            {
                                m_armoryUI.selectedArmorIndex = static_cast<int>(i);
                                m_armoryUI.modalArmorTier = static_cast<int>(i);
                                m_armoryUI.modalIsWeapon = false;
                                m_armoryUI.modalArmorSlot = (!headOwned) ? 0 : 1;
                                m_armoryUI.openPurchaseModal = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(u8"情報を見る"))
                            {
                                m_armoryUI.selectedArmorIndex = static_cast<int>(i);
                            }
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();

                    // --- 右側: 選択商品の効果・ステータス上昇量・購入条件 ---
                    ImGui::TableSetColumnIndex(1);
                    const std::size_t selIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.selectedArmorIndex, static_cast<int>(ArmorTier::Unknown) - 1)));
                    const ArmorTier selTier = static_cast<ArmorTier>(selIndex);

                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【%s の詳細情報】", GetArmorName(selTier));
                    ImGui::Separator();

                    if (ImGui::BeginChild("ArmorDetailChild", ImVec2(0.0f, 480.0f), false))
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 部位効果");
                        ImGui::BulletText(u8"頭: %s", GetArmorEffectText(selTier, true));
                        ImGui::BulletText(u8"胴: %s", GetArmorEffectText(selTier, false));
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 装備時のステータス上昇・性能");
                        switch (selTier)
                        {
                        case ArmorTier::Leather:
                            ImGui::BulletText(u8"頭: 重量 5 / 防御倍率 5%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 8 / 防御倍率 10%% 軽減");
                            ImGui::TextDisabled(u8"  序盤探索向けの軽量な防具。");
                            break;
                        case ArmorTier::Iron:
                            ImGui::BulletText(u8"頭: 重量 12 / 防御倍率 15%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 18 / 防御倍率 25%% 軽減");
                            ImGui::TextDisabled(u8"  頑丈な金属防具。物理防御に優れるが重量が増加。");
                            break;
                        case ArmorTier::RelicCovered:
                            ImGui::BulletText(u8"頭: 重量 6 / 防御倍率 25%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 10 / 防御倍率 35%% 軽減");
                            ImGui::TextDisabled(u8"  遺物の薄膜で保護された防具。軽量かつ高耐久。");
                            break;
                        case ArmorTier::RelicHardened:
                            ImGui::BulletText(u8"頭: 重量 15 / 防御倍率 35%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 22 / 防御倍率 45%% 軽減");
                            ImGui::TextDisabled(u8"  遺物硬化処理により重撃・突進に耐える強固な装甲。");
                            break;
                        case ArmorTier::RelicEnhanced:
                            ImGui::BulletText(u8"頭: 重量 8 / 防御倍率 45%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 12 / 防御倍率 55%% 軽減");
                            ImGui::TextDisabled(u8"  深層探窟家向けの超高密度強化装甲。");
                            break;
                        case ArmorTier::Relic:
                            ImGui::BulletText(u8"頭: 重量 10 / 防御倍率 60%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 15 / 防御倍率 70%% 軽減");
                            ImGui::TextColored(ImVec4(0.85f, 0.6f, 1.0f, 1.0f),
                                u8"★一式セット効果: 重量+100%% / 歩行+20%% / 走行+100%% / HP毎秒+2 / 攻撃+50%% / 防御+75%% / 採掘+50%%");
                            break;
                        default:
                            break;
                        }
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 購入に必要なコスト");
                        const bool hasMaterialPlan = (armorArmamentCosts[selIndex][0] > 0 || armorMaterialCosts[selIndex][0] > 0 ||
                                                      armorArmamentCosts[selIndex][1] > 0 || armorMaterialCosts[selIndex][1] > 0);
                        if (hasMaterialPlan)
                        {
                            ImGui::BulletText(u8"遺物併用（割引価格）:");
                            ImGui::Text(u8"  頭: %d G (武具遺物 %d個 / 防具遺物 %d個)",
                                armorDiscountPrices[selIndex][0], armorArmamentCosts[selIndex][0], armorMaterialCosts[selIndex][0]);
                            ImGui::Text(u8"  胴: %d G (武具遺物 %d個 / 防具遺物 %d個)",
                                armorDiscountPrices[selIndex][1], armorArmamentCosts[selIndex][1], armorMaterialCosts[selIndex][1]);
                        }
                        else
                        {
                            ImGui::BulletText(u8"素材不要（金のみで購入可能）");
                        }
                        ImGui::BulletText(u8"金のみで購入:");
                        ImGui::Text(u8"  頭: %d G / 胴: %d G", armorMoneyPrices[selIndex][0], armorMoneyPrices[selIndex][1]);

                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 現在の所有状況");
                        ImGui::BulletText(u8"頭: %s", m_ownedHeadArmor[selIndex] ? u8"所有済み" : u8"未所有");
                        ImGui::BulletText(u8"胴: %s", m_ownedBodyArmor[selIndex] ? u8"所有済み" : u8"未所有");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            // === 武器タブ ===
            if (ImGui::BeginTabItem(u8"武器"))
            {
                if (ImGui::BeginTable("WeaponSplitTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("WeaponList", ImGuiTableColumnFlags_WidthStretch, 0.46f);
                    ImGui::TableSetupColumn("WeaponDetail", ImGuiTableColumnFlags_WidthStretch, 0.54f);
                    ImGui::TableNextRow();

                    // --- 左側: 商品一覧 ---
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【武器一覧】");
                    ImGui::Separator();

                    if (ImGui::BeginChild("WeaponListChild", ImVec2(0.0f, 480.0f), false))
                    {
                        for (std::size_t i = 0; i < static_cast<std::size_t>(WeaponTier::Unknown); ++i)
                        {
                            const WeaponTier tier = static_cast<WeaponTier>(i);
                            const bool owned = m_ownedWeapons[i];
                            const bool isSelected = (m_armoryUI.selectedWeaponIndex == static_cast<int>(i));

                            ImGui::PushID(100 + static_cast<int>(i));

                            char itemHeader[128];
                            std::snprintf(itemHeader, sizeof(itemHeader), "%s%s", GetWeaponName(tier), owned ? u8" [所有済み]" : "");

                            if (ImGui::Selectable(itemHeader, isSelected))
                            {
                                m_armoryUI.selectedWeaponIndex = static_cast<int>(i);
                            }

                            if (ImGui::SmallButton(u8"購入"))
                            {
                                m_armoryUI.selectedWeaponIndex = static_cast<int>(i);
                                m_armoryUI.modalWeaponTier = static_cast<int>(i);
                                m_armoryUI.modalIsWeapon = true;
                                m_armoryUI.openPurchaseModal = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(u8"情報を見る"))
                            {
                                m_armoryUI.selectedWeaponIndex = static_cast<int>(i);
                            }
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();

                    // --- 右側: 選択商品の効果・ステータス上昇量・購入条件 ---
                    ImGui::TableSetColumnIndex(1);
                    const std::size_t selIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.selectedWeaponIndex, static_cast<int>(WeaponTier::Unknown) - 1)));
                    const WeaponTier selTier = static_cast<WeaponTier>(selIndex);

                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【%s の詳細情報】", GetWeaponName(selTier));
                    ImGui::Separator();

                    if (ImGui::BeginChild("WeaponDetailChild", ImVec2(0.0f, 480.0f), false))
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 武器効果");
                        ImGui::BulletText(u8"%s", GetWeaponEffectText(selTier));
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 装備時のステータス上昇・性能");
                        switch (selTier)
                        {
                        case WeaponTier::RustyPickaxe:
                            ImGui::BulletText(u8"攻撃力: 10 / 採掘速度: 基準(100%%)");
                            ImGui::TextDisabled(u8"  初期所持の古いピッケル。");
                            break;
                        case WeaponTier::NormalPickaxe:
                            ImGui::BulletText(u8"攻撃力: 15 / 採掘速度: +25%% (125%%)");
                            ImGui::TextDisabled(u8"  一般的な標準ピッケル。");
                            break;
                        case WeaponTier::SturdyPickaxe:
                            ImGui::BulletText(u8"攻撃力: 22 / 採掘速度: +50%% (150%%)");
                            ImGui::TextDisabled(u8"  強固な金属で鍛えられた良質なピッケル。");
                            break;
                        case WeaponTier::SharpPickaxe:
                            ImGui::BulletText(u8"攻撃力: 30 / 採掘速度: +75%% (175%%)");
                            ImGui::TextDisabled(u8"  鋭利な刃先を持ち、硬い鉱床も容易に穿つ。");
                            break;
                        case WeaponTier::RelicPickaxe:
                            ImGui::BulletText(u8"攻撃力: 45 / 採掘速度: +120%% (220%%)");
                            ImGui::TextColored(ImVec4(0.85f, 0.6f, 1.0f, 1.0f),
                                u8"★遺物ピッケル: 驚異的な採掘速度と威力を誇る伝説の逸品。");
                            break;
                        default:
                            break;
                        }
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 購入に必要なコスト");
                        const bool hasMaterialPlan = (weaponArmamentCosts[selIndex] > 0 || weaponMaterialCosts[selIndex] > 0);
                        if (hasMaterialPlan)
                        {
                            ImGui::BulletText(u8"遺物併用（割引価格）:");
                            ImGui::Text(u8"  %d G (武具遺物 %d個 / 武器遺物 %d個)",
                                weaponDiscountPrices[selIndex], weaponArmamentCosts[selIndex], weaponMaterialCosts[selIndex]);
                        }
                        else
                        {
                            ImGui::BulletText(u8"素材不要（金のみで購入可能）");
                        }
                        ImGui::BulletText(u8"金のみで購入: %d G", weaponMoneyPrices[selIndex]);

                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 現在の所有状況");
                        ImGui::BulletText(u8"%s", m_ownedWeapons[selIndex] ? u8"所有済み" : u8"未所有");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    });
}

void SceneNarakuProto::DrawArmoryPurchaseModal()
{
    static const int armorDiscountPrices[][2] = { { 10, 15 }, { 150, 200 }, { 200, 300 }, { 250, 350 }, { 350, 400 }, { 5000, 6000 } };
    static const int armorMoneyPrices[][2] = { { 10, 15 }, { 150, 200 }, { 1000, 1500 }, { 1250, 1750 }, { 1750, 2000 }, { 25000, 30000 } };
    static const int armorArmamentCosts[][2] = { { 0, 0 }, { 0, 0 }, { 3, 4 }, { 4, 6 }, { 7, 10 }, { 17, 19 } };
    static const int armorMaterialCosts[][2] = { { 0, 0 }, { 0, 0 }, { 5, 7 }, { 7, 9 }, { 5, 7 }, { 15, 19 } };
    static const int weaponDiscountPrices[] = { 5, 500, 1250, 1500, 5000 };
    static const int weaponMoneyPrices[] = { 5, 500, 1250, 1500, 25000 };
    static const int weaponArmamentCosts[] = { 0, 0, 0, 0, 11 };
    static const int weaponMaterialCosts[] = { 0, 0, 0, 0, 21 };

    if (m_armoryUI.openPurchaseModal)
    {
        ImGui::OpenPopup(u8"武具購入##ArmoryPurchasePopupModal");
        m_armoryUI.openPurchaseModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(480.0f, 380.0f), ImGuiCond_Always);
    if (NarakuUi::BeginPopupModal(u8"武具購入##ArmoryPurchasePopupModal", nullptr, ImGuiWindowFlags_NoResize))
    {
        if (m_armoryUI.modalIsWeapon)
        {
            // === 武器の購入 ===
            const std::size_t tierIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.modalWeaponTier, static_cast<int>(WeaponTier::Unknown) - 1)));
            const WeaponTier tier = static_cast<WeaponTier>(tierIndex);
            const bool owned = m_ownedWeapons[tierIndex];

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【武器購入】 %s", GetWeaponName(tier));
            ImGui::Separator();

            if (owned)
            {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"※この武器はすでに所有しています。");
            }
            else
            {
                const int discountPrice = weaponDiscountPrices[tierIndex];
                const int moneyPrice = weaponMoneyPrices[tierIndex];
                const int reqArmament = weaponArmamentCosts[tierIndex];
                const int reqWeapon = weaponMaterialCosts[tierIndex];
                const int curArmament = CountStoredRelics(RelicType::ArmamentUpgrade);
                const int curWeapon = CountStoredRelics(RelicType::WeaponUpgrade);

                const bool canBuyMaterial = (m_money >= discountPrice) && (curArmament >= reqArmament) && (curWeapon >= reqWeapon);
                const bool canBuyMoney = (m_money >= moneyPrice);

                // --- 遺物を消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 遺物を消費して購入（割引）");
                if (reqArmament > 0 || reqWeapon > 0)
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G)", discountPrice, m_money);
                    ImGui::Text(u8"必要素材: 武具強化 %d個 (所持: %d個) / 武器強化 %d個 (所持: %d個)",
                        reqArmament, curArmament, reqWeapon, curWeapon);
                }
                else
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G) ※素材不要", discountPrice, m_money);
                }

                ImGui::BeginDisabled(!canBuyMaterial);
                if (ImGui::Button(u8"遺物を消費して買う##BuyWeaponMat", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyWeapon(tier, true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();

                // --- 金のみを消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 金のみを消費して購入");
                ImGui::Text(u8"必要金: %d G (所持: %d G)", moneyPrice, m_money);

                ImGui::BeginDisabled(!canBuyMoney);
                if (ImGui::Button(u8"金のみを消費して買う##BuyWeaponMoney", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyWeapon(tier, false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
            }
        }
        else
        {
            // === 防具の購入 ===
            const std::size_t tierIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.modalArmorTier, static_cast<int>(ArmorTier::Unknown) - 1)));
            const ArmorTier tier = static_cast<ArmorTier>(tierIndex);

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【防具購入】 %s", GetArmorName(tier));
            ImGui::TextUnformatted(u8"購入する部位を選択してください:");
            ImGui::RadioButton(u8"頭防具", &m_armoryUI.modalArmorSlot, 0);
            ImGui::SameLine();
            ImGui::RadioButton(u8"胴防具", &m_armoryUI.modalArmorSlot, 1);
            ImGui::Separator();

            const int slot = m_armoryUI.modalArmorSlot; // 0: 頭, 1: 胴
            const bool owned = (slot == 0) ? m_ownedHeadArmor[tierIndex] : m_ownedBodyArmor[tierIndex];

            if (owned)
            {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"※選択中の部位（%s）はすでに所有しています。", slot == 0 ? u8"頭" : u8"胴");
            }
            else
            {
                const int discountPrice = armorDiscountPrices[tierIndex][slot];
                const int moneyPrice = armorMoneyPrices[tierIndex][slot];
                const int reqArmament = armorArmamentCosts[tierIndex][slot];
                const int reqArmor = armorMaterialCosts[tierIndex][slot];
                const int curArmament = CountStoredRelics(RelicType::ArmamentUpgrade);
                const int curArmor = CountStoredRelics(RelicType::ArmorUpgrade);

                const bool canBuyMaterial = (m_money >= discountPrice) && (curArmament >= reqArmament) && (curArmor >= reqArmor);
                const bool canBuyMoney = (m_money >= moneyPrice);

                // --- 遺物を消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 遺物を消費して購入（割引）");
                if (reqArmament > 0 || reqArmor > 0)
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G)", discountPrice, m_money);
                    ImGui::Text(u8"必要素材: 武具強化 %d個 (所持: %d個) / 防具強化 %d個 (所持: %d個)",
                        reqArmament, curArmament, reqArmor, curArmor);
                }
                else
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G) ※素材不要", discountPrice, m_money);
                }

                ImGui::BeginDisabled(!canBuyMaterial);
                if (ImGui::Button(u8"遺物を消費して買う##BuyArmorMat", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyArmor(tier, slot == 0, true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();

                // --- 金のみを消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 金のみを消費して購入");
                ImGui::Text(u8"必要金: %d G (所持: %d G)", moneyPrice, m_money);

                ImGui::BeginDisabled(!canBuyMoney);
                if (ImGui::Button(u8"金のみを消費して買う##BuyArmorMoney", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyArmor(tier, slot == 0, false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (NarakuUi::BackButton(u8"閉じる", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SceneNarakuProto::DrawRestaurant()
{
    DrawTownFrame(u8"地上レストラン", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();
        ImGui::TextWrapped(u8"50Gで満腹度を100にし、水分75、最大HPの75%%、最大精神力の50%%を回復します。");
        if (ImGui::Button(u8"食事をする（50G）", ImVec2(180.0f, 0.0f))) TryUseRestaurant();
        ImGui::Separator();
        ImGui::Text(u8"給水所（無料）");
        if (m_hydration < kHydrationMaximum && ImGui::Button(u8"安全な水を直接飲む（水分+25）"))
        {
            DrinkWater(kWaterDrinkAmount, WaterQuality::Safe, 0.0f);
            SaveProgress();
            ShowCenterNotification(u8"安全な水を飲みました。");
        }
        const auto emptyBottle = std::find_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
            [](const WaterBottle& bottle) { return bottle.amount <= 0.0f; });
        if (emptyBottle != m_storedWaterBottles.end() && ImGui::Button(u8"空の水筒1本を安全な水で満たす"))
        {
            emptyBottle->amount = kWaterBottleCapacity;
            emptyBottle->quality = WaterQuality::Safe;
            emptyBottle->foodPoisoningChance = 0.0f;
            SaveProgress();
            ShowCenterNotification(u8"水筒を安全な水で満たしました。");
        }
    });
}

void SceneNarakuProto::DrawBaseInteractionMarker() const
{
    const NarakuMap::BasePoint* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const NarakuMap::BasePoint& base : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base.layerId);
        if (layerIndex < 0 ||
            std::fabs(m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth - m_player.depth) > 0.35f)
        {
            continue;
        }
        const float distance = Distance(m_player.pos, { base.xz.x, base.xz.z });
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = &base;
        }
    }
    if (nearest == nullptr || nearestDistance > 3.5f) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y - 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("BaseInteractionPrompt", nullptr, ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text(u8"%s  %s", nearest->type == NarakuMap::BaseType::SecondBase ? u8"第二拠点" : u8"前衛拠点",
        nearestDistance <= kInteractRange ? u8"[F] 利用" : u8"もう少し近づいてください");
    ImGui::End();
}

void SceneNarakuProto::FillSafeWater(bool useStoredBottles)
{
    std::vector<WaterBottle>& bottles = useStoredBottles ? m_storedWaterBottles : m_waterBottles;
    const auto empty = std::find_if(bottles.begin(), bottles.end(),
        [](const WaterBottle& bottle) { return bottle.amount <= 0.0f; });
    if (empty == bottles.end())
    {
        ShowCenterNotification(u8"空の水筒がありません。");
        return;
    }
    empty->amount = kWaterBottleCapacity;
    empty->quality = WaterQuality::Safe;
    empty->foodPoisoningChance = 0.0f;
    SaveProgress();
    ShowCenterNotification(u8"水筒を安全な水で満たしました。");
}

bool SceneNarakuProto::TryUseSecondBaseMeal()
{
    if (m_money < kSecondBaseMealPrice)
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
    m_money -= kSecondBaseMealPrice;
    m_fullness = kFullnessMaximum;
    m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
    m_player.hp = std::min(GetMaxHp(), m_player.hp + GetMaxHp() * kRestaurantHpRatio);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + GetMaxMental() * kRestaurantMentalRatio);
    SaveProgress();
    ShowCenterNotification(u8"第二拠点で食事をしました。");
    return true;
}

bool SceneNarakuProto::TryUseForwardBaseMeal()
{
    if (m_money < kForwardBaseMealPrice)
    {
        ShowCenterNotification(u8"所持金が足りません。");
        return false;
    }
    if (m_player.hp >= GetMaxHp() && m_fullness >= kFullnessMaximum)
    {
        ShowCenterNotification(u8"今は食事をする必要がありません。");
        return false;
    }
    m_money -= kForwardBaseMealPrice;
    m_player.hp = std::min(GetMaxHp(), m_player.hp + GetMaxHp() * kForwardBaseMealRecoveryRatio);
    m_fullness = std::min(kFullnessMaximum,
        m_fullness + kFullnessMaximum * kForwardBaseMealRecoveryRatio);
    SaveProgress();
    ShowCenterNotification(u8"前衛拠点で食事をしました。深層では精神までは休まりません。");
    return true;
}

void SceneNarakuProto::AdvanceWorldTime(double gameSeconds, double realSeconds)
{
    gameSeconds = std::max(0.0, gameSeconds);
    realSeconds = std::max(0.0, realSeconds);
    UpdateRespawns(static_cast<float>(realSeconds));
    UpdateMiningRespawns(static_cast<float>(realSeconds));
    UpdateFishingPointRecharge(gameSeconds);

    for (std::size_t index = 0; index < m_quests.size(); ++index)
    {
        QuestRecord& quest = m_quests[index];
        if (quest.status == QuestStatus::Available)
        {
            quest.cooldownSeconds -= gameSeconds;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
        else if (quest.status == QuestStatus::Active)
        {
            quest.remainingSeconds -= realSeconds;
            if (quest.remainingSeconds <= 0.0) FailQuest(index);
        }
        else if (quest.status == QuestStatus::Cooldown)
        {
            quest.cooldownSeconds -= realSeconds;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
    }

    m_gameWeekSeconds += gameSeconds;
    while (m_gameWeekSeconds >= kGameWeekSeconds)
    {
        m_gameWeekSeconds -= kGameWeekSeconds;
        BeginNewGameWeek();
    }
}

void SceneNarakuProto::StayAtSecondBase()
{
    if (m_money < kSecondBaseLodgingPrice)
    {
        ShowCenterNotification(u8"宿泊料金が足りません。");
        return;
    }
    const double current = std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds);
    const double dayStart = std::floor(current / kGameDaySeconds) * kGameDaySeconds;
    const double seven = dayStart + 7.0 * 60.0 * 60.0;
    const double target = current < seven ? seven : seven + kGameDaySeconds;
    const double elapsedGameSeconds = target - current;
    const double elapsedRealSeconds = elapsedGameSeconds / kGameTimeScale;
    m_money -= kSecondBaseLodgingPrice;
    m_player.mental = std::min(GetMaxMental(), m_player.mental + GetMaxMental() * 0.40f);
    AdvanceWorldTime(elapsedGameSeconds, elapsedRealSeconds);
    SaveProgress();
    ShowCenterNotification(u8"第二拠点に宿泊し、午前7時に起床しました。");
}

float SceneNarakuProto::GetSecondBaseSaleMultiplier(const RelicItem& item) const
{
    const int day = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds) /
        kGameDaySeconds);
    enum class Category { Cash, Upgrade, Special, Broken };
    Category category = Category::Special;
    if (item.broken) category = Category::Broken;
    else if (item.type == RelicType::CashLow || item.type == RelicType::CashHigh) category = Category::Cash;
    else if (item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade ||
        item.type == RelicType::ArmorUpgrade) category = Category::Upgrade;

    static constexpr Category high[] = { Category::Upgrade, Category::Special, Category::Upgrade,
        Category::Cash, Category::Cash, Category::Broken, Category::Special };
    static constexpr Category low[] = { Category::Broken, Category::Upgrade, Category::Special,
        Category::Upgrade, Category::Broken, Category::Special, Category::Cash };
    if (category == high[day]) return 1.50f;
    if (category == low[day]) return 0.75f;
    return 1.0f;
}

bool SceneNarakuProto::IsSecondBaseSellable(const RelicItem& item) const
{
    if (!IsRelicSellable(item)) return false;
    if (item.broken) return true;
    return item.type == RelicType::CashLow || item.type == RelicType::CashHigh ||
        item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade ||
        item.type == RelicType::ArmorUpgrade || item.type == RelicType::Offensive ||
        item.type == RelicType::Survival || item.type == RelicType::MentalRecovery;
}

int SceneNarakuProto::GetSecondBaseSaleValue(const RelicItem& item) const
{
    return static_cast<int>(std::lround(static_cast<double>(item.value) * GetSecondBaseSaleMultiplier(item)));
}

void SceneNarakuProto::DrawSecondBase()
{
    DrawTownFrame(u8"第二拠点", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();
        if (NarakuUi::BeginTabBar("SecondBaseTabs"))
        {
        if (ImGui::BeginTabItem(u8"食事"))
        {
            ImGui::TextWrapped(u8"地上と同じ食事を100Gで提供します。");
            if (ImGui::Button(u8"食事をする（100G）")) TryUseSecondBaseMeal();
            ImGui::Separator();
            ImGui::Text(u8"携帯食料 20G / 個");
            if (ImGui::Button(u8"携帯食料を1個買う") && m_money >= kFoodPrice * 2)
            {
                m_money -= kFoodPrice * 2;
                ++m_foodCount;
                SaveProgress();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"給水"))
        {
            if (ImGui::Button(u8"安全な水を直接飲む（水分+25）"))
            {
                DrinkWater(kWaterDrinkAmount, WaterQuality::Safe, 0.0f);
                SaveProgress();
            }
            if (ImGui::Button(u8"空の水筒1本を満たす")) FillSafeWater(false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"宿泊"))
        {
            ImGui::TextWrapped(u8"1,000Gで次の午前7時まで休み、最大精神力の40%%分を回復します。時間依存イベントも進行します。");
            if (ImGui::Button(u8"宿泊する（1,000G）")) StayAtSecondBase();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"売却"))
        {
            static const char* weekdays[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
            const int day = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds) / kGameDaySeconds);
            ImGui::Text(u8"%s曜日の相場", weekdays[day]);
            bool found = false;
            for (int index = 0; index < static_cast<int>(m_inventory.size()); ++index)
            {
                const RelicItem& item = m_inventory[static_cast<std::size_t>(index)];
                if (!IsSecondBaseSellable(item)) continue;
                found = true;
                ImGui::PushID(index);
                ImGui::Text(u8"%s  %dG（%.2g倍）", GetRelicDisplayName(item),
                    GetSecondBaseSaleValue(item), GetSecondBaseSaleMultiplier(item));
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"売却"))
                {
                    m_money += GetSecondBaseSaleValue(item);
                    m_inventory.erase(m_inventory.begin() + index);
                    SaveProgress();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            for (int index = 0; index < static_cast<int>(m_portableLights.size()); ++index)
            {
                PortableLight& light = m_portableLights[static_cast<std::size_t>(index)];
                if (!light.broken) continue;
                found = true;
                const float multiplier = day == 5 ? 1.50f : (day == 0 || day == 4 ? 0.75f : 1.0f);
                const int price = static_cast<int>(std::lround(kBrokenPortableLightSellPrice * multiplier));
                ImGui::PushID(10000 + index);
                ImGui::Text(u8"ライト（破損）  %dG（%.2g倍）", price, multiplier);
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"売却"))
                {
                    m_money += price;
                    m_portableLights.erase(m_portableLights.begin() + index);
                    SaveProgress();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (!found) ImGui::TextDisabled(u8"売却できる遺物を所持していません。");
            ImGui::EndTabItem();
        }
            ImGui::EndTabBar();
        }
    }, Mode::Explore, u8"拠点を出る");
}

void SceneNarakuProto::DrawForwardBase()
{
    DrawTownFrame(u8"前衛拠点", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();
        auto buyCurrent = [this](const char* name, int price, int& count)
        {
        ImGui::Text(u8"%s  %dG", name, price);
        ImGui::SameLine();
        ImGui::PushID(name);
        ImGui::BeginDisabled(m_money < price);
        if (ImGui::SmallButton(u8"購入"))
        {
            m_money -= price;
            ++count;
            SaveProgress();
            ShowCenterNotification(std::string(name) + u8"を購入しました。");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        };
        auto countCarriedRelics = [this](RelicType type)
        {
        return static_cast<int>(std::count_if(m_inventory.begin(), m_inventory.end(),
            [type](const RelicItem& item) { return item.type == type; }));
        };
        auto consumeCarriedRelics = [this](RelicType type, int count)
        {
        for (auto it = m_inventory.begin(); it != m_inventory.end() && count > 0;)
        {
            if (it->type == type)
            {
                it = m_inventory.erase(it);
                --count;
            }
            else ++it;
        }
        };
        if (NarakuUi::BeginTabBar("ForwardBaseTabs"))
        {
        if (ImGui::BeginTabItem(u8"商店"))
        {
            auto buyRelic = [this](RelicType type, int price)
            {
                const char* name = GetRelicTypeName(type);
                ImGui::Text(u8"%s  %dG", name, price);
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(type));
                ImGui::BeginDisabled(m_money < price);
                if (ImGui::SmallButton(u8"購入"))
                {
                    m_money -= price;
                    RelicItem item = CreateRelic(type, name);
                    item.stabilized = true;
                    m_inventory.push_back(item);
                    m_identifiedRelics[static_cast<std::size_t>(type)] = true;
                    SaveProgress();
                    ShowCenterNotification(std::string(name) + u8"を購入しました。");
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            };
            buyCurrent(u8"携帯食料", kFoodPrice, m_foodCount);
            ImGui::Text(u8"水筒  %dG", kWaterBottlePrice); ImGui::SameLine();
            if (ImGui::SmallButton(u8"購入##forwardBottle") && m_money >= kWaterBottlePrice)
            { m_money -= kWaterBottlePrice; m_waterBottles.push_back({}); SaveProgress(); }
            ImGui::Text(u8"料理セット  %dG", kCookingKitPrice); ImGui::SameLine();
            if (ImGui::SmallButton(u8"購入##forwardKit") && m_money >= kCookingKitPrice)
            { m_money -= kCookingKitPrice; m_cookingKits.push_back({}); SaveProgress(); }
            buyRelic(RelicType::ArmamentUpgrade, 600);
            buyRelic(RelicType::WeaponUpgrade, 700);
            buyRelic(RelicType::ArmorUpgrade, 700);
            ImGui::Separator();
            buyCurrent(u8"行動食1号", kRationOnePrice, m_rationOneCount);
            buyCurrent(u8"カートリッジ", kCartridgePrice, m_cartridgeCount);
            buyCurrent(u8"見たことない魚", kUnknownFishPrice, m_rawFishCount);
            ImGui::TextDisabled(u8"魚: 重量20 / 生食 満腹度+10・精神力+5 / 調理後 満腹度+50・精神力+25");
            if (m_rawFishCount > 0 && ImGui::SmallButton(u8"未調理の魚を売却（500G）"))
            { --m_rawFishCount; m_money += 500; SaveProgress(); }
            if (m_cookedFishCount > 0 && ImGui::SmallButton(u8"調理済みの魚を売却（400G）"))
            { --m_cookedFishCount; m_money += 400; SaveProgress(); }
            static const char* sizeNames[] = { u8"小", u8"中", u8"大" };
            for (int size = 0; size < 3; ++size)
            {
                ImGui::PushID(4200 + size);
                ImGui::Text(u8"魚（%s） 生:%d / 調理済み:%d", sizeNames[size],
                    m_rawSizedFish[static_cast<size_t>(size)], m_cookedSizedFish[static_cast<size_t>(size)]);
                if (m_rawSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"生を売却"))
                { --m_rawSizedFish[static_cast<size_t>(size)]; m_money += kRawSizedFishSellValues[static_cast<size_t>(size)]; SaveProgress(); }
                ImGui::SameLine();
                if (m_cookedSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"調理済みを売却"))
                { --m_cookedSizedFish[static_cast<size_t>(size)]; m_money += kCookedSizedFishSellValues[static_cast<size_t>(size)]; SaveProgress(); }
                ImGui::PopID();
            }
            ImGui::SeparatorText(u8"遺物売却");
            bool hasSellableRelic = false;
            for (int index = 0; index < static_cast<int>(m_inventory.size()); ++index)
            {
                const RelicItem& item = m_inventory[static_cast<std::size_t>(index)];
                if (!IsRelicSellable(item)) continue;
                hasSellableRelic = true;
                ImGui::PushID(3000 + index);
                ImGui::Text(u8"%s  %dG", GetRelicDisplayName(item), item.value);
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"売却"))
                {
                    m_money += item.value;
                    m_inventory.erase(m_inventory.begin() + index);
                    SaveProgress();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (!hasSellableRelic) ImGui::TextDisabled(u8"売却できる遺物を所持していません。");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"武具屋"))
        {
            static const int armorDiscountPrices[][2] = { {10,15}, {150,200}, {200,300}, {250,350}, {350,400}, {5000,6000} };
            static const int armorMoneyPrices[][2] = { {10,15}, {150,200}, {1000,1500}, {1250,1750}, {1750,2000}, {25000,30000} };
            static const int armorArmamentCosts[][2] = { {0,0}, {0,0}, {3,4}, {4,6}, {7,10}, {17,19} };
            static const int armorMaterialCosts[][2] = { {0,0}, {0,0}, {5,7}, {7,9}, {5,7}, {15,19} };
            for (int tierValue = 0; tierValue < static_cast<int>(ArmorTier::Unknown); ++tierValue)
            {
                const ArmorTier tier = static_cast<ArmorTier>(tierValue);
                ImGui::PushID(1000 + tierValue);
                if (!m_ownedHeadArmor[static_cast<std::size_t>(tier)])
                {
                    ImGui::Text(u8"%s 頭", GetArmorName(tier)); ImGui::SameLine();
                    if (ImGui::SmallButton(u8"金のみ##headMoney")) TryBuyArmor(tier, true, false);
                    if (armorArmamentCosts[tierValue][0] > 0 || armorMaterialCosts[tierValue][0] > 0)
                    {
                        ImGui::SameLine();
                        const bool canBuy = m_money >= armorDiscountPrices[tierValue][0] &&
                            countCarriedRelics(RelicType::ArmamentUpgrade) >= armorArmamentCosts[tierValue][0] &&
                            countCarriedRelics(RelicType::ArmorUpgrade) >= armorMaterialCosts[tierValue][0];
                        ImGui::BeginDisabled(!canBuy);
                        if (ImGui::SmallButton(u8"素材併用##headMaterial"))
                        {
                            m_money -= armorDiscountPrices[tierValue][0];
                            consumeCarriedRelics(RelicType::ArmamentUpgrade, armorArmamentCosts[tierValue][0]);
                            consumeCarriedRelics(RelicType::ArmorUpgrade, armorMaterialCosts[tierValue][0]);
                            m_ownedHeadArmor[static_cast<std::size_t>(tier)] = true;
                            SaveProgress();
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::TextDisabled(u8"  金のみ %dG / 素材併用 %dG（武具%d・装備%d）",
                        armorMoneyPrices[tierValue][0], armorDiscountPrices[tierValue][0],
                        armorArmamentCosts[tierValue][0], armorMaterialCosts[tierValue][0]);
                }
                if (!m_ownedBodyArmor[static_cast<std::size_t>(tier)])
                {
                    ImGui::Text(u8"%s 胴", GetArmorName(tier)); ImGui::SameLine();
                    if (ImGui::SmallButton(u8"金のみ##bodyMoney")) TryBuyArmor(tier, false, false);
                    if (armorArmamentCosts[tierValue][1] > 0 || armorMaterialCosts[tierValue][1] > 0)
                    {
                        ImGui::SameLine();
                        const bool canBuy = m_money >= armorDiscountPrices[tierValue][1] &&
                            countCarriedRelics(RelicType::ArmamentUpgrade) >= armorArmamentCosts[tierValue][1] &&
                            countCarriedRelics(RelicType::ArmorUpgrade) >= armorMaterialCosts[tierValue][1];
                        ImGui::BeginDisabled(!canBuy);
                        if (ImGui::SmallButton(u8"素材併用##bodyMaterial"))
                        {
                            m_money -= armorDiscountPrices[tierValue][1];
                            consumeCarriedRelics(RelicType::ArmamentUpgrade, armorArmamentCosts[tierValue][1]);
                            consumeCarriedRelics(RelicType::ArmorUpgrade, armorMaterialCosts[tierValue][1]);
                            m_ownedBodyArmor[static_cast<std::size_t>(tier)] = true;
                            SaveProgress();
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::TextDisabled(u8"  金のみ %dG / 素材併用 %dG（武具%d・装備%d）",
                        armorMoneyPrices[tierValue][1], armorDiscountPrices[tierValue][1],
                        armorArmamentCosts[tierValue][1], armorMaterialCosts[tierValue][1]);
                }
                ImGui::PopID();
            }
            static const int weaponDiscountPrices[] = { 5,500,1250,1500,5000 };
            static const int weaponMoneyPrices[] = { 5,500,1250,1500,25000 };
            static const int weaponArmamentCosts[] = { 0,0,0,0,11 };
            static const int weaponMaterialCosts[] = { 0,0,0,0,21 };
            for (int tierValue = 0; tierValue < static_cast<int>(WeaponTier::Unknown); ++tierValue)
            {
                const WeaponTier tier = static_cast<WeaponTier>(tierValue);
                if (m_ownedWeapons[static_cast<std::size_t>(tier)]) continue;
                ImGui::PushID(2000 + tierValue);
                ImGui::Text(u8"%s", GetWeaponName(tier)); ImGui::SameLine();
                if (ImGui::SmallButton(u8"金のみ##weaponMoney")) TryBuyWeapon(tier, false);
                if (weaponArmamentCosts[tierValue] > 0 || weaponMaterialCosts[tierValue] > 0)
                {
                    ImGui::SameLine();
                    const bool canBuy = m_money >= weaponDiscountPrices[tierValue] &&
                        countCarriedRelics(RelicType::ArmamentUpgrade) >= weaponArmamentCosts[tierValue] &&
                        countCarriedRelics(RelicType::WeaponUpgrade) >= weaponMaterialCosts[tierValue];
                    ImGui::BeginDisabled(!canBuy);
                    if (ImGui::SmallButton(u8"素材併用##weaponMaterial"))
                    {
                        m_money -= weaponDiscountPrices[tierValue];
                        consumeCarriedRelics(RelicType::ArmamentUpgrade, weaponArmamentCosts[tierValue]);
                        consumeCarriedRelics(RelicType::WeaponUpgrade, weaponMaterialCosts[tierValue]);
                        m_ownedWeapons[static_cast<std::size_t>(tier)] = true;
                        SaveProgress();
                    }
                    ImGui::EndDisabled();
                }
                ImGui::TextDisabled(u8"  金のみ %dG / 素材併用 %dG（武具%d・武器%d）",
                    weaponMoneyPrices[tierValue], weaponDiscountPrices[tierValue],
                    weaponArmamentCosts[tierValue], weaponMaterialCosts[tierValue]);
                ImGui::PopID();
            }
            ImGui::Separator();
            if (!m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Unknown)])
            { ImGui::Text(u8"未知の装備-頭- %dG", kUnknownHeadPrice); ImGui::SameLine(); if (ImGui::SmallButton(u8"購入##unknownHead") && m_money >= kUnknownHeadPrice) { m_money -= kUnknownHeadPrice; m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Unknown)] = true; SaveProgress(); } }
            if (!m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Unknown)])
            { ImGui::Text(u8"未知の装備-胴- %dG", kUnknownBodyPrice); ImGui::SameLine(); if (ImGui::SmallButton(u8"購入##unknownBody") && m_money >= kUnknownBodyPrice) { m_money -= kUnknownBodyPrice; m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Unknown)] = true; SaveProgress(); } }
            if (!m_ownedWeapons[static_cast<std::size_t>(WeaponTier::Unknown)])
            { ImGui::Text(u8"未知の武器 %dG", kUnknownWeaponPrice); ImGui::SameLine(); if (ImGui::SmallButton(u8"購入##unknownWeapon") && m_money >= kUnknownWeaponPrice) { m_money -= kUnknownWeaponPrice; m_ownedWeapons[static_cast<std::size_t>(WeaponTier::Unknown)] = true; SaveProgress(); } }
            ImGui::TextWrapped(u8"未知の装備は頭と胴を揃えて装備した時だけ効果を発揮します。装備変更は自宅で行います。");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"レストラン"))
        {
            ImGui::TextWrapped(u8"200Gで最大HPと満腹度の80%%分を回復します。精神力は回復しません。");
            if (ImGui::Button(u8"食事をする（200G）")) TryUseForwardBaseMeal();
            ImGui::Separator();
            if (ImGui::Button(u8"安全な水を直接飲む（水分+25）"))
            { DrinkWater(kWaterDrinkAmount, WaterQuality::Safe, 0.0f); SaveProgress(); }
            if (ImGui::Button(u8"空の水筒1本を満たす")) FillSafeWater(false);
            ImGui::EndTabItem();
        }
            ImGui::EndTabBar();
        }
    }, Mode::Explore, u8"拠点を出る");
}

void SceneNarakuProto::DrawAbyssEntrance()
{
    ImGui::SetNextWindowPos(ImVec2(400.0f, 170.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 430.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"奈落塔出入口", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text(u8"階級: %s  所持金: %dG", GetRankName(m_adventurerRank), m_money);
    DrawCurrentStatus();
    ImGui::Separator();
    ImGui::TextWrapped(u8"潜行準備は自宅でのみ変更できます。通常潜行、または解放済みの直通門を選択してください。");
    if (ImGui::Button(u8"通常潜行：第一層 上層", ImVec2(260.0f, 0.0f))) StartDive(1);

    const int secondFee = 500 + 250 * m_directGateWeeklyUses[0];
    ImGui::BeginDisabled(static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Black));
    if (ImGui::Button(u8"第二層 上層へ直通", ImVec2(260.0f, 0.0f))) StartDive(2);
    ImGui::SameLine(); ImGui::Text(u8"%dG（今週%d回利用）", secondFee, m_directGateWeeklyUses[0]);
    ImGui::EndDisabled();
    if (static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Black))
        ImGui::TextDisabled(u8"黒印以上で解放");

    const int thirdFee = 2000 + 250 * m_directGateWeeklyUses[1];
    ImGui::BeginDisabled(static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Purple));
    if (ImGui::Button(u8"第三層 上層へ直通", ImVec2(260.0f, 0.0f))) StartDive(3);
    ImGui::SameLine(); ImGui::Text(u8"%dG（今週%d回利用）", thirdFee, m_directGateWeeklyUses[1]);
    ImGui::EndDisabled();
    if (static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Purple))
        ImGui::TextDisabled(u8"紫印以上で解放");

    ImGui::Separator();
    if (NarakuUi::BackButton(u8"地上へ戻る", ImVec2(140.0f, 0.0f))) m_mode = Mode::Surface;
    ImGui::End();
}

void SceneNarakuProto::DrawSurfaceFacilityMarkers()
{
    const auto facility = std::min_element(m_surfaceFacilities.begin(), m_surfaceFacilities.end(),
        [this](const SurfaceFacilityState& lhs, const SurfaceFacilityState& rhs)
        { return Distance(lhs.interactionPoint, m_player.pos) < Distance(rhs.interactionPoint, m_player.pos); });
    if (facility == m_surfaceFacilities.end() || Distance(facility->interactionPoint, m_player.pos) > 3.5f) return;
    const char* name = u8"施設";
    if (facility->type == NarakuPiece::SurfaceFacilityType::Home) name = u8"自宅";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::Shop) name = u8"商店";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::Armory) name = u8"武具屋";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk) name = u8"レストラン・受注受付";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::AbyssEntrance) name = u8"奈落塔出入口";
    ImGui::SetNextWindowPos(ImVec2(500.0f, 620.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::Begin(u8"地上施設案内", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text(u8"[F] %sを利用", name);
    ImGui::End();
}
