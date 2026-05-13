#include "SceneResult.h"
#include "Defines.h"
#include "Input.h"
#include "Sound.h"
#include "Transfer.h"

namespace
{
	constexpr int kMenuRestart = 0;
	constexpr int kMenuTitle = 1;
	constexpr int kRewardContinue = 100;
	constexpr int kStageShopBuyTrait = 200;
	constexpr int kStageShopChangeSkill = 201;
	constexpr int kStageShopEnhanceSkill = 202;
	constexpr int kStageShopContinue = 203;

	bool IsResultConfirmTriggered()
	{
		return IsMenuConfirmTrigger();
	}

	bool IsSelectionPrevTriggered()
	{
		return IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A') || IsKeyTrigger(VK_UP) || IsKeyTrigger('W');
	}

	bool IsSelectionNextTriggered()
	{
		return IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D') || IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S');
	}

	bool IsRunIntermissionActive(const Transfer& tran)
	{
		return
			tran.roguelike.selectionPending != 0 ||
			tran.roguelike.intermissionMode != Transfer::RoguelikeUpgrade::IntermissionNone;
	}

}

SceneResult::SceneResult()
{
	m_pWinner = new UIObject("result/winner.png", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, SCREEN_WIDTH, SCREEN_HEIGHT);
	m_pLoser = new UIObject("result/loser.png", SCREEN_WIDTH / 2.0f, SCREEN_HEIGHT / 2.0f, SCREEN_WIDTH, SCREEN_HEIGHT);
	m_pMenuRestart = new UIObject("result/Restart.png", SCREEN_WIDTH * 0.33f, SCREEN_HEIGHT * 0.80f, 380.0f, 120.0f);
	m_pMenuTitle = new UIObject("result/Title.png", SCREEN_WIDTH * 0.67f, SCREEN_HEIGHT * 0.80f, 380.0f, 120.0f);
	m_pResultBgm = LoadSound("Assets/Sound/BGM/ResultBGM.mp3", true);
	m_pResultBgmVoice = nullptr;
	m_menuSelection = kMenuRestart;
	if (m_pResultBgm)
	{
		m_pResultBgmVoice = PlaySound(m_pResultBgm);
	}
	m_current = SceneManager::GetResultType();
	m_rewardSelection = 0;
}

SceneResult::~SceneResult()
{
	if (m_pResultBgmVoice)
	{
		m_pResultBgmVoice->Stop();
		m_pResultBgmVoice->DestroyVoice();
		m_pResultBgmVoice = nullptr;
	}
	m_pResultBgm = nullptr;

	if (m_pWinner)
	{
		delete m_pWinner;
		m_pWinner = nullptr;
	}
	if (m_pLoser)
	{
		delete m_pLoser;
		m_pLoser = nullptr;
	}
	if (m_pMenuRestart)
	{
		delete m_pMenuRestart;
		m_pMenuRestart = nullptr;
	}
	if (m_pMenuTitle)
	{
		delete m_pMenuTitle;
		m_pMenuTitle = nullptr;
	}
}

void SceneResult::Update()
{
	TRAN_INS;
	tran.gameplayDebug.runTimerRunning = 0;
	tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
	tran.gameplayDebug.upgradeSelectionPending = tran.roguelike.selectionPending;
	tran.gameplayDebug.upgradeRerollRemain = tran.roguelike.rerollRemain;
	tran.gameplayDebug.upgradeOffer0 = tran.roguelike.offers[0];
	tran.gameplayDebug.upgradeOffer1 = tran.roguelike.offers[1];
	tran.gameplayDebug.upgradeOffer2 = tran.roguelike.offers[2];
	if (m_pResultBgmVoice)
	{
		float masterVolume = tran.gameplay.volumeMaster;
		if (masterVolume < 0.0f) masterVolume = 0.0f;
		if (masterVolume > 2.0f) masterVolume = 2.0f;
		float bgmVolume = tran.gameplay.volumeBgm;
		if (bgmVolume < 0.0f) bgmVolume = 0.0f;
		if (bgmVolume > 2.0f) bgmVolume = 2.0f;
		m_pResultBgmVoice->SetVolume(masterVolume * bgmVolume);
	}

	if (m_current == SceneManager::ResultType::None)
	{
		SceneManager::ChangeResult(SceneManager::ResultType::None);
		SceneManager::ChangeScene(SceneManager::SCENE_TITLE);
		return;
	}
	if (m_current == SceneManager::ResultType::Win && IsRunIntermissionActive(tran))
	{
		if (tran.roguelike.selectionPending != 0)
		{
			const bool returnToShopAfterSelection =
				tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionShop ||
				tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionRest;
			tran.RefreshUpgradeSelectionState();
			const bool hasAnyOffer = tran.HasAnyUpgradeOffer();
			int optionIds[Transfer::RoguelikeUpgrade::kOfferCount + 1] = {};
			int optionCount = 0;
			if (hasAnyOffer)
			{
				for (int i = 0; i < Transfer::RoguelikeUpgrade::kOfferCount; ++i)
				{
					if (tran.roguelike.offers[i] >= 0)
					{
						optionIds[optionCount++] = i;
					}
				}
			}
			else
			{
				optionIds[optionCount++] = kRewardContinue;
			}
			if (optionCount <= 0)
			{
				optionIds[0] = kRewardContinue;
				optionCount = 1;
			}
			if (m_rewardSelection < 0) m_rewardSelection = 0;
			if (m_rewardSelection >= optionCount) m_rewardSelection = optionCount - 1;
			tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;

			if (IsSelectionPrevTriggered())
			{
				--m_rewardSelection;
				if (m_rewardSelection < 0) m_rewardSelection = optionCount - 1;
				tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
			}
			else if (IsSelectionNextTriggered())
			{
				++m_rewardSelection;
				if (m_rewardSelection >= optionCount) m_rewardSelection = 0;
				tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
			}

			if (hasAnyOffer && IsKeyTrigger('R'))
			{
				if (tran.RerollUpgradeSelection())
				{
					m_rewardSelection = 0;
					tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
				}
			}

			if (IsResultConfirmTriggered())
			{
				const int selectedOption = optionIds[m_rewardSelection];
				bool handled = false;
				if (selectedOption == kRewardContinue)
				{
					handled = tran.AdvanceUpgradeSelectionPhase();
				}
				else if (selectedOption >= 0)
				{
					handled = tran.ApplyUpgradeSelection(selectedOption);
				}
				if (handled)
				{
					m_rewardSelection = 0;
					tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
					if (tran.roguelike.selectionPending == 0)
					{
						if (tran.gameplayDebug.challengeReturnToGameAfterReward != 0)
						{
							tran.gameplayDebug.challengeReturnToGameAfterReward = 0;
							SceneManager::ChangeResult(SceneManager::ResultType::None);
							SceneManager::ChangeScene(SceneManager::SCENE_GAME);
							return;
						}
						if (returnToShopAfterSelection)
						{
							return;
						}
						const bool advanced = tran.BeginNextStageSelection();
						const bool shouldReturnToGame =
							tran.roguelike.selectionPending == 0 &&
							tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionNone;
						if (!advanced || shouldReturnToGame)
						{
							SceneManager::ChangeResult(SceneManager::ResultType::None);
							SceneManager::ChangeScene(SceneManager::SCENE_GAME);
						}
					}
					return;
				}
			}
			return;
		}

		if (tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionMapSelect)
		{
			const int nextStageIndex = tran.roguelike.currentStageIndex + 1;
			const int optionCount = tran.GetRunStageOptionCount(nextStageIndex);
			if (m_rewardSelection < 0) m_rewardSelection = 0;
			if (m_rewardSelection >= optionCount) m_rewardSelection = optionCount - 1;
			tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;

			if (IsSelectionPrevTriggered())
			{
				--m_rewardSelection;
				if (m_rewardSelection < 0) m_rewardSelection = optionCount - 1;
				tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
			}
			else if (IsSelectionNextTriggered())
			{
				++m_rewardSelection;
				if (m_rewardSelection >= optionCount) m_rewardSelection = 0;
				tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
			}

			if (IsResultConfirmTriggered() && tran.SelectNextStage(m_rewardSelection))
			{
				m_rewardSelection = 0;
				tran.gameplayDebug.rewardSelectionIndex = 0;
				if (tran.roguelike.selectionPending == 0 &&
					tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionNone)
				{
					SceneManager::ChangeResult(SceneManager::ResultType::None);
					SceneManager::ChangeScene(SceneManager::SCENE_GAME);
				}
			}
			return;
		}

		if (tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionShop ||
			tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionRest)
		{
			const int optionIds[4] =
			{
				kStageShopBuyTrait,
				kStageShopChangeSkill,
				kStageShopEnhanceSkill,
				kStageShopContinue
			};
			const int optionCount = 4;
			if (m_rewardSelection < 0) m_rewardSelection = 0;
			if (m_rewardSelection >= optionCount) m_rewardSelection = optionCount - 1;
			tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;

			if (IsSelectionPrevTriggered())
			{
				--m_rewardSelection;
				if (m_rewardSelection < 0) m_rewardSelection = optionCount - 1;
				tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
			}
			else if (IsSelectionNextTriggered())
			{
				++m_rewardSelection;
				if (m_rewardSelection >= optionCount) m_rewardSelection = 0;
				tran.gameplayDebug.rewardSelectionIndex = m_rewardSelection;
			}

			if (IsResultConfirmTriggered())
			{
				const int selectedOption = optionIds[m_rewardSelection];
				const int shopCost = tran.GetCurrentShopCost();
				if (selectedOption == kStageShopBuyTrait)
				{
					if (tran.PurchaseRandomUpgrade(Transfer::RoguelikeUpgrade::SelectionShopTrait, shopCost))
					{
						m_rewardSelection = 0;
						tran.gameplayDebug.rewardSelectionIndex = 0;
					}
				}
				else if (selectedOption == kStageShopChangeSkill)
				{
					if (tran.PurchaseRandomUpgrade(Transfer::RoguelikeUpgrade::SelectionShopSkillChange, shopCost))
					{
						m_rewardSelection = 0;
						tran.gameplayDebug.rewardSelectionIndex = 0;
					}
				}
				else if (selectedOption == kStageShopEnhanceSkill)
				{
					if (tran.PurchaseRandomUpgrade(Transfer::RoguelikeUpgrade::SelectionShopSkillEnhance, shopCost))
					{
						m_rewardSelection = 0;
						tran.gameplayDebug.rewardSelectionIndex = 0;
					}
				}
				else if (selectedOption == kStageShopContinue && tran.ContinueFromCurrentNonCombatStage())
				{
					m_rewardSelection = 0;
					tran.gameplayDebug.rewardSelectionIndex = 0;
					if (tran.roguelike.selectionPending == 0 &&
						tran.roguelike.intermissionMode == Transfer::RoguelikeUpgrade::IntermissionNone)
					{
						SceneManager::ChangeResult(SceneManager::ResultType::None);
						SceneManager::ChangeScene(SceneManager::SCENE_GAME);
					}
				}
			}
			return;
		}
	}

	if (IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A') || IsKeyTrigger(VK_UP) || IsKeyTrigger('W'))
	{
		m_menuSelection = kMenuRestart;
	}
	if (IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D') || IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S'))
	{
		m_menuSelection = kMenuTitle;
	}

	const bool restartSelected = (m_menuSelection == kMenuRestart);
	const float selectedScale = 1.18f;
	if (m_pMenuRestart)
	{
		m_pMenuRestart->SetSize(restartSelected ? 380.0f * selectedScale : 380.0f,
			restartSelected ? 120.0f * selectedScale : 120.0f);
		m_pMenuRestart->SetColor(restartSelected ? 1.0f : 0.65f, restartSelected ? 1.0f : 0.65f, restartSelected ? 1.0f : 0.65f, 1.0f);
	}
	if (m_pMenuTitle)
	{
		m_pMenuTitle->SetSize(!restartSelected ? 380.0f * selectedScale : 380.0f,
			!restartSelected ? 120.0f * selectedScale : 120.0f);
		m_pMenuTitle->SetColor(!restartSelected ? 1.0f : 0.65f, !restartSelected ? 1.0f : 0.65f, !restartSelected ? 1.0f : 0.65f, 1.0f);
	}

	if (IsResultConfirmTriggered())
	{
		if (restartSelected)
		{
			tran.ResetRoguelikeUpgrade();
			tran.gameplayDebug.requestBossBattle = 0;
			tran.gameplayDebug.bossBattleActive = 0;
			tran.gameplayDebug.showBossResultTimer = 0;
			tran.gameplayDebug.runElapsedSec = 0.0f;
			tran.gameplayDebug.runRecordedSec = 0.0f;
			tran.gameplayDebug.runTimerRunning = 0;
			SceneManager::ChangeResult(SceneManager::ResultType::None);
			SceneManager::ChangeScene(SceneManager::SCENE_GAME);
		}
		else
		{
			tran.gameplayDebug.requestBossBattle = 0;
			tran.gameplayDebug.bossBattleActive = 0;
			tran.gameplayDebug.showBossResultTimer = 0;
			tran.gameplayDebug.runTimerRunning = 0;
			SceneManager::ChangeResult(SceneManager::ResultType::None);
			SceneManager::ChangeScene(SceneManager::SCENE_TITLE);
		}
		return;
	}
}

void SceneResult::Draw()
{
	switch (m_current)
	{
	case SceneManager::Win:
		m_pWinner->Draw();
		break;
	case SceneManager::Lose:
		m_pLoser->Draw();
		break;
	default:
		return;
	}

	TRAN_INS;
	if (m_current == SceneManager::ResultType::Win && IsRunIntermissionActive(tran))
	{
		return;
	}

	if (m_pMenuRestart) m_pMenuRestart->Draw();
	if (m_pMenuTitle) m_pMenuTitle->Draw();
}
