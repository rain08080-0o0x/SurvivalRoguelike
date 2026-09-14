#include "SceneNarakuProto.h"
#include "NarakuUiNavigation.h"

#include "imgui.h"

#include <algorithm>
#include <cmath>

void SceneNarakuProto::DrawTransitionSaveError()
{
    ImGui::SetNextWindowPos(ImVec2(440.0f, 250.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440.0f, 180.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"保存失敗", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"Scene切替前の保存に失敗しました。現在のSceneを維持しています。");
    if (ImGui::Button(u8"保存を再試行", ImVec2(160.0f, 0.0f)) && SaveProgress())
    {
        m_mode = m_pendingModeAfterSave;
    }
    ImGui::End();
}

void SceneNarakuProto::DrawResult()
{
    ImGui::SetNextWindowPos(ImVec2(390.0f, 160.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_Always);
    NarakuUi::Begin(m_mode == Mode::DeathResult ? u8"死亡リザルト" : u8"帰還リザルト", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text(u8"結果: %s", m_result.reason.c_str());
    ImGui::Text(u8"最大到達深度: 第%d層", m_result.maxDepth);
    ImGui::Text(u8"採掘した遺物: %d", m_result.minedCount);
    ImGui::Separator();

    if (m_mode == Mode::ReturnResult)
    {
        ImGui::Text(u8"持ち帰った遺物: %d", m_result.carriedRelics);
        ImGui::Text(u8"今回初めて鑑定した種類: %d", m_result.identifiedRelics);
        int miningReward = 0;
        int defeatReward = 0;
        int stayReward = 0;
        for (int i = 0; i < 5; ++i)
        {
            const int depth = i + 1;
            miningReward += static_cast<int>(std::llround(m_result.minedByDepth[i] * 5.0 * GetDepthRewardMultiplier(depth)));
            defeatReward += static_cast<int>(std::llround((m_result.chargerKillsByDepth[i] * 10.0 + m_result.territoryKillsByDepth[i] * 20.0) * GetDepthRewardMultiplier(depth)));
            stayReward += static_cast<int>(std::llround(std::min(300.0f, m_result.staySecondsByDepth[i]) * GetDepthStayRewardMultiplier(depth)));
        }
        ImGui::Text(u8"内訳 採掘:%dG / 撃破:%dG / 滞在:%dG", miningReward, defeatReward, stayReward);
        ImGui::Text(u8"内訳 初到達:%dG / 新種:%dG", m_result.firstAreaCount * 150, m_result.newRelicTypeCount * 150);
        ImGui::Text(u8"探索報酬: %dG", m_result.explorationReward);
        if (m_result.uniqueReward > 0) ImGui::Text(u8"欲望の揺籃 持ち帰り報酬: %dG", m_result.uniqueReward);
        ImGui::Text(u8"遺物の売却見込額: %d", m_result.saleAmount);
        ImGui::Spacing();
        if (ImGui::Button(u8"地上へ戻る", ImVec2(160.0f, 0.0f))) EnterSurface(true);
    }
    else
    {
        ImGui::Text(u8"失った遺物: %d", m_result.lostRelics);
        ImGui::Text(u8"レベル: %d -> %d", m_result.levelBeforeDeath, m_result.levelAfterDeath);
        ImGui::Text(u8"消費した保護: %d", m_result.protectionConsumed);

        if (m_deathRecoveryPending)
        {
            ImGui::Separator();
            ImGui::Text(u8"%sの保険対象内です（死亡地点: 第%d層）。",
                GetRankName(m_adventurerRank), m_deathRecoveryDepth);
            ImGui::Text(u8"回収費: %dG / 所持金: %dG", m_deathRecoveryFee, m_money);
            if (m_money < m_deathRecoveryFee)
            {
                ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.20f, 1.0f),
                    u8"不足分%dGは借金へ加算されます。", m_deathRecoveryFee - m_money);
            }
            ImGui::Text(u8"食料: %d / 加熱食料: %d / 水筒: %d / 料理セット: %d",
                m_pendingDeathRecoveryFood, m_pendingDeathRecoveryHeatedFood,
                static_cast<int>(m_pendingDeathRecoveryBottles.size()),
                static_cast<int>(m_pendingDeathRecoveryCookingKits.size()));

            ImGui::BeginChild("DeathRecoveryItems", ImVec2(0.0f, 180.0f), true);
            for (std::size_t index = 0; index < m_pendingDeathRecoveryRelics.size(); ++index)
            {
                const RelicItem& item = m_pendingDeathRecoveryRelics[index];
                if (item.maxUses > 0)
                {
                    ImGui::Text(u8"%d. %s  %s  使用回数%d/%d", static_cast<int>(index + 1),
                        GetRelicDisplayName(item), item.broken ? u8"破損" : u8"正常",
                        item.remainingUses, item.maxUses);
                }
                else
                {
                    ImGui::Text(u8"%d. %s  %s", static_cast<int>(index + 1),
                        GetRelicDisplayName(item), item.broken ? u8"破損" : u8"正常");
                }
            }
            for (std::size_t index = 0; index < m_pendingDeathRecoveryBottles.size(); ++index)
            {
                const WaterBottle& bottle = m_pendingDeathRecoveryBottles[index];
                ImGui::Text(u8"水筒%d  水量%.0f/100  水質:%s", static_cast<int>(index + 1),
                    bottle.amount, GetWaterQualityName(bottle.quality));
            }
            for (std::size_t index = 0; index < m_pendingDeathRecoveryCookingKits.size(); ++index)
            {
                ImGui::Text(u8"料理セット%d  残り%d/5回", static_cast<int>(index + 1),
                    m_pendingDeathRecoveryCookingKits[index].remainingUses);
            }
            ImGui::EndChild();

            if (!m_deathRecoveryDiscardConfirm)
            {
                if (ImGui::Button(u8"回収する", ImVec2(150.0f, 0.0f))) ResolveDeathRecovery(true);
                ImGui::SameLine();
                if (ImGui::Button(u8"回収せず破棄", ImVec2(150.0f, 0.0f)))
                    m_deathRecoveryDiscardConfirm = true;
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
                    u8"回収しない荷物は永久に失われます。");
                if (ImGui::Button(u8"永久に破棄する", ImVec2(150.0f, 0.0f))) ResolveDeathRecovery(false);
                ImGui::SameLine();
                if (NarakuUi::BackButton(u8"戻る", ImVec2(150.0f, 0.0f)))
                    m_deathRecoveryDiscardConfirm = false;
            }
        }
        else if (m_deathRecoveryDepth > 0 && !CanRecoverDeathInventory(m_deathRecoveryDepth))
        {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f),
                u8"第%d層は%sの保険対象外です。探索中の荷物は失われました。",
                m_deathRecoveryDepth, GetRankName(m_adventurerRank));
        }
        else
        {
            ImGui::Separator();
            ImGui::TextDisabled(u8"回収待ちの荷物はありません。");
        }

        ImGui::BeginDisabled(m_deathRecoveryPending);
        if (ImGui::Button(u8"自宅へ"))
        {
            EnterSurface(false);
            m_mode = Mode::Home;
        }
        ImGui::EndDisabled();
    }
    ImGui::End();
}
