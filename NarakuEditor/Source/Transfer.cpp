#include "Transfer.h"
#include <fstream>
#include <string>
#include <unordered_set>
#include <cstdlib>

namespace
{
	// Default config paths used depending on launch location.
	const char* kDefaultGameplayTuningPath = "Assets/gameplay_tuning.cfg";
	const char* kMirrorGameplayTuningPath = "DX22_Project/Assets/gameplay_tuning.cfg";
	const char* kDebugGameplayTuningPath = "x64/Debug/Assets/gameplay_tuning.cfg";
	const char* kUpstreamMirrorGameplayTuningPath = "../../DX22_Project/Assets/gameplay_tuning.cfg";
	constexpr int kInitialRunRerollCount = 5;
	constexpr int kDefaultRunReviveCount = 1;
	constexpr float kRestStageHealRatio = 0.25f;
	constexpr int kDiceRewardGain = 3;
	constexpr int kShopBaseCost = 2;
	constexpr int kShopCostPerPurchase = 1;
	constexpr float kDefaultPlayerMaxHp = 100.0f;

	/**
	 * @brief 呼び出し側が既定パスを使いたい指定かどうかを判定します。
	 * @param path 呼び出し側が渡したパスです。
	 * @return nullptr または空文字なら true です。
	 */
	bool IsDefaultPathArgument(const char* path)
	{
		return !(path && path[0] != '\0');
	}

	/**
	 * @brief 実際に使う設定ファイルパスを決定します。
	 * @param path 呼び出し側指定パスです。
	 * @return 指定があればそのパス、無ければ既定パスです。
	 */
	const char* ResolvePath(const char* path)
	{
		// 明示指定がある場合は、そのパスを優先します。
		if (!IsDefaultPathArgument(path))
		{
			return path;
		}
		// 未指定時だけ既定パスを返します。
		return kDefaultGameplayTuningPath;
	}

	/**
	 * @brief 文字列前後の空白を除去します。
	 * @param s 前後空白を除去する文字列です。
	 */
	void Trim(std::string& s)
	{
		size_t begin = 0;
		// 先頭側の空白を読み飛ばします。
		while (begin < s.size() && (s[begin] == ' ' || s[begin] == '\t' || s[begin] == '\r' || s[begin] == '\n'))
		{
			++begin;
		}

		size_t end = s.size();
		// 末尾側の空白も同様に削ります。
		while (end > begin && (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
		{
			--end;
		}

		// 空白を除いた範囲だけ残します。
		s = s.substr(begin, end - begin);
	}

	/**
	 * @brief 文字列を整数へ変換します。
	 * @param s 変換元文字列です。
	 * @param fallback 変換できない場合に返す値です。
	 * @return 変換成功時は整数値、失敗時は fallback です。
	 */
	int ToInt(const std::string& s, int fallback)
	{
		char* endPtr = nullptr;
		const long v = std::strtol(s.c_str(), &endPtr, 10);
		// 数字として 1 文字も読めなかった場合は既定値へ戻します。
		if (endPtr == s.c_str())
		{
			return fallback;
		}
		return static_cast<int>(v);
	}

	/**
	 * @brief 文字列を浮動小数へ変換します。
	 * @param s 変換元文字列です。
	 * @param fallback 変換できない場合に返す値です。
	 * @return 変換成功時は浮動小数値、失敗時は fallback です。
	 */
	float ToFloat(const std::string& s, float fallback)
	{
		char* endPtr = nullptr;
		const float v = std::strtof(s.c_str(), &endPtr);
		// 数値解釈できない場合は既定値を維持します。
		if (endPtr == s.c_str())
		{
			return fallback;
		}
		return v;
	}

	/**
	 * @brief 整数値を指定範囲に丸めます。
	 * @param v 補正対象値です。
	 * @param lo 下限です。
	 * @param hi 上限です。
	 * @return lo 以上 hi 以下に丸めた値です。
	 */
	int ClampInt(int v, int lo, int hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	float ClampFloat(float v, float lo, float hi)
	{
		if (v < lo) return lo;
		if (v > hi) return hi;
		return v;
	}

	bool IsSupportedXInputBindingValue(int value)
	{
		switch (value)
		{
		case Transfer::InputConfig::XInputBack:
		case Transfer::InputConfig::XInputLeftThumb:
		case Transfer::InputConfig::XInputRightThumb:
		case Transfer::InputConfig::XInputLeftShoulder:
		case Transfer::InputConfig::XInputRightShoulder:
		case Transfer::InputConfig::XInputA:
		case Transfer::InputConfig::XInputB:
		case Transfer::InputConfig::XInputX:
		case Transfer::InputConfig::XInputY:
			return true;
		default:
			return false;
		}
	}

	/**
	 * @brief 難易度プリセット値を有効範囲へ補正します。
	 * @param preset 補正対象値です。
	 * @return 0 から 2 の範囲に収めた値です。
	 */
	int NormalizeDifficultyPresetValue(int preset)
	{
		return ClampInt(preset, 0, 2);
	}

	/**
	 * @brief 難易度に応じた小/大強化幅を返します。
	 * @param preset 難易度です。
	 * @param smallStep 小強化の増加量出力先です。
	 * @param largeStep 大強化の増加量出力先です。
	 */
	void GetUpgradeStepsForDifficulty(int preset, int& smallStep, int& largeStep)
	{
		const auto& gameplay = Transfer::GetInstance().gameplay;
		// 難易度が高いほど、1回あたりの強化量を大きくします。
		switch (NormalizeDifficultyPresetValue(preset))
		{
		case 0:
			smallStep = gameplay.upgradeStepEasySmall;
			largeStep = gameplay.upgradeStepEasyLarge;
			break;
		case 2:
			smallStep = gameplay.upgradeStepHardSmall;
			largeStep = gameplay.upgradeStepHardLarge;
			break;
		default:
			smallStep = gameplay.upgradeStepNormalSmall;
			largeStep = gameplay.upgradeStepNormalLarge;
			break;
		}
	}

	void InitializeRunStageRoute(Transfer::RoguelikeUpgrade& roguelike);

	template <typename GameplayTuningT, typename RoguelikeT>
	void ApplyGameplayTuningFileDefaults(GameplayTuningT& gameplay,
										 int& difficultyPreset,
										 RoguelikeT& roguelike)
	{
		gameplay = GameplayTuningT{};
		gameplay.enemyCount = 3;
		gameplay.waveMax = 3;
		gameplay.waveEnemyAddPerWave = 1;
		gameplay.groundTileSize = 1.0f;
		gameplay.cameraIntroDuration = 3.0f;
		gameplay.cameraIntroFocusDistance = 0.5f;
		gameplay.attackWindup = 0.04f;
		gameplay.attackDuration = 0.12f;
		gameplay.attackRecovery = 0.10f;
		gameplay.attackCooldown = 0.24f;
		gameplay.skill1Cooldown = 4.0f;
		gameplay.skill2Cooldown = 9.0f;
		gameplay.screenShakeHitThreshold = 3;
		gameplay.screenShakeDuration = 0.18f;
		gameplay.screenShakeAmplitude = 0.20f;
		gameplay.attackSweepDegrees = 120.0f;
		gameplay.attackSweepRadiusScale = 2.0f;
		gameplay.attackWidthScale = 2.0f;
		gameplay.attackDepthScale = 2.0f;
		gameplay.attackHitStop = 0.08f;
		gameplay.attackKnockback = 1.5f;
		gameplay.attackHitFlash = 0.405f;
		gameplay.attackTrailInterval = 0.02f;
		gameplay.attackTrailLife = 0.16f;
		gameplay.attackTrailScale = 0.75f;
		gameplay.playerDamageFlash = 0.20f;
		gameplay.playerDamageInvincible = 0.35f;
		gameplay.playerDamageFlashScale = 1.65f;
		gameplay.enemyDefeatFlash = 0.28f;
		gameplay.enemyDefeatFlashScale = 1.60f;
		gameplay.volumeMaster = 0.52f;
		gameplay.volumeBgm = 0.0f;
		gameplay.volumeSe = 0.67f;
		gameplay.directionMarkerAlpha = 0.92f;
		gameplay.directionMarkerOverlapAlpha = 0.45f;
		gameplay.bossHpBarWidthRate = 0.42f;
		gameplay.bossHpBarHeightRate = 0.045f;
		gameplay.bossGuardBarOffsetX = 0.0f;
		gameplay.bossGuardBarOffsetY = 6.0f;
		gameplay.bossGuardBarWidthRate = 0.42f;
		gameplay.bossGuardBarHeightRate = 0.018f;
		gameplay.bossSizeAreaScale = 6.0f;
		gameplay.bossMaxHp = 300;
		gameplay.bossAttackTelegraph = 1.0f;
		gameplay.bossAttackJumpOutTime = 0.5f;
		gameplay.bossAttackDashDuration = 0.35f;
		gameplay.bossAttackCooldown = 1.15f;
		gameplay.bossAttackLanePlayerScale = 3.0f;
		gameplay.bossAttackDamage = 20.0f;
		gameplay.bossAttackHitStop = 0.3f;
		gameplay.bossHitShakeDuration = 0.18f;
		gameplay.bossHitShakeAmplitude = 0.23f;
		gameplay.bossGuardInitialMax = 14.0f;
		gameplay.bossGuardFinalMax = 24.0f;
		gameplay.bossGuardRecoverStep = 2.0f;
		gameplay.bossDamageScaleNormal = 0.2f;
		gameplay.bossDamageScaleBroken = 2.2f;
		gameplay.bossBreakRecoverSec = 8.0f;
		gameplay.bossDashNarrowTelegraph = 1.0f;
		gameplay.bossDashWideTelegraph = 2.0f;
		gameplay.bossDashWideWidthRate = 0.5f;
		gameplay.bossRandomRainCount = 5;
		gameplay.bossRandomRainTelegraph = 1.0f;
		gameplay.bossRandomRainRadiusScale = 1.6f;
		gameplay.bossSummonMin = 5;
		gameplay.bossSummonMax = 10;
		gameplay.bossSummonTelegraph = 1.0f;
		gameplay.bossTrackingDropCount = 5;
		gameplay.bossTrackingDropTelegraph = 1.0f;
		gameplay.bossTrackingDropRadiusScale = 3.0f;
		gameplay.bossUltimateCrossTelegraph = 1.0f;
		gameplay.bossUltimateCrossLaneScale = 1.0f;
		gameplay.bossUltimateStompCount = 5;
		gameplay.bossUltimateStompTelegraph = 3.0f;
		gameplay.bossUltimateStompRepeatTelegraph = 1.0f;
		gameplay.bossUltimateStompRadiusScale = 3.0f;
		gameplay.bossUltimateFieldTelegraph = 7.0f;
		gameplay.bossUltimateFieldSafeScale = 2.0f;
		gameplay.enemyAttackWindup = 0.55f;
		gameplay.enemyAttackCooldown = 1.0f;
		gameplay.enemyAttackRangeMin = 0.8f;
		gameplay.enemyAttackRangeScale = 1.35f;
		gameplay.enemyAttackDamage = 1.0f;
		gameplay.enemyMoveSpeed = 1.2f;
		gameplay.waveEnemyMoveSpeedAdd = 0.15f;
		gameplay.waveEnemyAttackDamageScalePerWave = 0.2f;
		gameplay.enemyProjectileSpeed = 6.5f;
		gameplay.enemyProjectileLife = 1.4f;
		gameplay.enemyProjectileRadius = 0.22f;
		gameplay.enemyProjectileDamageScale = 0.85f;
		gameplay.enemySeparationRadius = 1.1f;
		gameplay.enemySeparationWeight = 0.8f;
		gameplay.enemySeparationMaxOffset = 0.8f;
		gameplay.enemySpawnRingScale = 0.35f;
		gameplay.enemySpawnJitterScale = 0.1f;
		gameplay.enemySpawnMinPlayerDist = 1.5f;
		gameplay.enemySpawnMinEnemyDist = 0.9f;
		gameplay.pushSlop = 0.01f;
		gameplay.playerPushShare = 0.55f;
		gameplay.enemyPushShare = 0.45f;
		gameplay.challengeNoDamageDuration = 18.0f;
		gameplay.challengeNoDamageAttackInterval = 1.10f;
		gameplay.challengeNoDamageTelegraph = 0.90f;
		gameplay.challengeNoDamageRadius = 1.20f;
		gameplay.challengeNoDamageDamage = 1.0f;
		gameplay.challengeNoDamageBurstCount = 2;
		gameplay.challengeDefenseDuration = 25.0f;
		gameplay.challengeDefenseBeaconMaxHp = 25.0f;
		gameplay.challengeDefenseBeaconRadius = 0.90f;
		gameplay.challengeDefenseBeaconContactDamage = 2.0f;
		gameplay.challengeDefenseSpawnInterval = 1.40f;
		gameplay.challengeDefenseEnemyMoveSpeed = 0.32f;
		gameplay.challengeDefenseSpeedHpBonusRate = 0.10f;
		gameplay.challengeDefenseRangedHpBonusRate = 0.30f;
		gameplay.challengeDefenseTankHpBonusRate = 0.50f;
		gameplay.challengeDefenseEnemyCap = 8;

		difficultyPreset = 1;

		roguelike = RoguelikeT{};
		roguelike.stageClearCount = 37;
		roguelike.attackPowerLevel = 5;
		roguelike.attackSpeedLevel = 5;
		roguelike.evadeCooldownLevel = 5;
		roguelike.lastUpgradeType = 17;
		roguelike.skillSlot1 = Transfer::RoguelikeUpgrade::SkillOrbit;
		roguelike.skillSlot2 = Transfer::RoguelikeUpgrade::SkillNova;
		roguelike.skillShotRangeLevel = 0;
		roguelike.skillShotPowerLevel = 0;
		roguelike.skillShotCooldownLevel = 0;
		roguelike.skillNovaRangeLevel = 5;
		roguelike.skillNovaPowerLevel = 0;
		roguelike.skillNovaCooldownLevel = 5;
		roguelike.skillOrbitRangeLevel = 0;
		roguelike.skillOrbitCooldownLevel = 5;
		roguelike.skillOrbitCountLevel = 5;
		roguelike.rerollMaxPerStage = 0;
		InitializeRunStageRoute(roguelike);
	}

	const int kGameplayTuningRequiredKeyCount = 134;

	const int kUpgradeTierMax = Transfer::RoguelikeUpgrade::kLevelMax;

	/**
	 * @brief 強化レベルを有効範囲へ丸めます。
	 * @param level 補正対象レベルです。
	 * @return 0 から上限までに丸めたレベルです。
	 */
	int ClampUpgradeTier(int level)
	{
		return ClampInt(level, 0, kUpgradeTierMax);
	}

	// Level tables used to convert upgrade tiers into actual runtime values.
	const int kAttackDamageByTier[kUpgradeTierMax + 1] =
	{
		1, 2, 3, 5, 7, 10
	};

	const float kAttackCooldownScaleByTier[kUpgradeTierMax + 1] =
	{
		1.00f, 0.92f, 0.84f, 0.76f, 0.68f, 0.60f
	};

	const float kEvadeCooldownScaleByTier[kUpgradeTierMax + 1] =
	{
		1.00f, 0.90f, 0.78f, 0.66f, 0.54f, 0.42f
	};

	int RandRangeInt(int minValue, int maxValue)
	{
		if (maxValue <= minValue) return minValue;
		const int span = maxValue - minValue + 1;
		return minValue + (std::rand() % span);
	}

	const int kUpgradeOfferCount = 3;
	const int kUpgradeTypeCount = Transfer::RoguelikeUpgrade::UpgradeTypeCount;
	const int kUpgradeOfferNone = -1;
	const int kSkillUpgradeTierMax = Transfer::RoguelikeUpgrade::kLevelMax;

	int NormalizeSkillTypeValue(int skillType)
	{
		return ClampInt(
			skillType,
			Transfer::RoguelikeUpgrade::SkillNone,
			Transfer::RoguelikeUpgrade::SkillTypeCount - 1);
	}

	int NormalizeSelectionPhaseValue(int selectionPhase)
	{
		return ClampInt(
			selectionPhase,
			Transfer::RoguelikeUpgrade::SelectionNone,
			Transfer::RoguelikeUpgrade::SelectionMixed);
	}

	int NormalizeStageTypeValue(int stageType)
	{
		return ClampInt(
			stageType,
			Transfer::RoguelikeUpgrade::StageCombat,
			Transfer::RoguelikeUpgrade::StageFinalBoss);
	}

	int NormalizeRewardTypeValue(int rewardType)
	{
		return ClampInt(
			rewardType,
			Transfer::RoguelikeUpgrade::RewardUnknown,
			Transfer::RoguelikeUpgrade::RewardFinalBoss);
	}

	int PackOfferValue(int offerType, int primaryValue, int secondaryValue = 0)
	{
		return
			(offerType * Transfer::RoguelikeUpgrade::kOfferTypeStride) +
			(primaryValue * Transfer::RoguelikeUpgrade::kOfferPrimaryStride) +
			secondaryValue;
	}

	int GetOfferTypeValue(int packedOffer)
	{
		if (packedOffer < 0)
		{
			return Transfer::RoguelikeUpgrade::OfferNone;
		}
		return packedOffer / Transfer::RoguelikeUpgrade::kOfferTypeStride;
	}

	int GetOfferPrimaryValue(int packedOffer)
	{
		if (packedOffer < 0)
		{
			return 0;
		}
		return (packedOffer % Transfer::RoguelikeUpgrade::kOfferTypeStride) /
			Transfer::RoguelikeUpgrade::kOfferPrimaryStride;
	}

	int GetOfferSecondaryValue(int packedOffer)
	{
		if (packedOffer < 0)
		{
			return 0;
		}
		return packedOffer % Transfer::RoguelikeUpgrade::kOfferPrimaryStride;
	}

	int GetWeaponUpgradeTagType(int weaponUpgradeType)
	{
		switch (weaponUpgradeType)
		{
		case Transfer::RoguelikeUpgrade::WeaponUpgradeCooldownBurst:
			return Transfer::RoguelikeUpgrade::TagCooldown;
		case Transfer::RoguelikeUpgrade::WeaponUpgradeChainOnCrit:
			return Transfer::RoguelikeUpgrade::TagChain;
		case Transfer::RoguelikeUpgrade::WeaponUpgradeBloodOnCrit:
			return Transfer::RoguelikeUpgrade::TagBlood;
		case Transfer::RoguelikeUpgrade::WeaponUpgradeFireOnCrit:
			return Transfer::RoguelikeUpgrade::TagFire;
		case Transfer::RoguelikeUpgrade::WeaponUpgradeCritNeedReduce:
		default:
			return Transfer::RoguelikeUpgrade::TagWeapon;
		}
	}

	int GetSkillEnhancementTagType(int enhancementType)
	{
		switch (enhancementType)
		{
		case Transfer::RoguelikeUpgrade::SkillEnhanceCooldown:
			return Transfer::RoguelikeUpgrade::TagCooldown;
		case Transfer::RoguelikeUpgrade::SkillEnhanceChain:
			return Transfer::RoguelikeUpgrade::TagChain;
		case Transfer::RoguelikeUpgrade::SkillEnhanceBlood:
			return Transfer::RoguelikeUpgrade::TagBlood;
		case Transfer::RoguelikeUpgrade::SkillEnhanceFire:
			return Transfer::RoguelikeUpgrade::TagFire;
		case Transfer::RoguelikeUpgrade::SkillEnhanceWeapon:
		default:
			return Transfer::RoguelikeUpgrade::TagWeapon;
		}
	}

	bool HasLoadoutActionSkill(const Transfer::RoguelikeUpgrade& roguelike, int actionSkillType)
	{
		for (int i = 0; i < Transfer::RoguelikeUpgrade::kSkillSlotCount; ++i)
		{
			if (roguelike.loadoutSkills[i] == actionSkillType)
			{
				return true;
			}
		}
		return false;
	}

	int GetSkillEnhancementTypeFromTraitType(int traitType)
	{
		switch (traitType)
		{
		case Transfer::RoguelikeUpgrade::TraitCooldown:
			return Transfer::RoguelikeUpgrade::SkillEnhanceCooldown;
		case Transfer::RoguelikeUpgrade::TraitWeapon:
			return Transfer::RoguelikeUpgrade::SkillEnhanceWeapon;
		case Transfer::RoguelikeUpgrade::TraitChain:
			return Transfer::RoguelikeUpgrade::SkillEnhanceChain;
		case Transfer::RoguelikeUpgrade::TraitBlood:
			return Transfer::RoguelikeUpgrade::SkillEnhanceBlood;
		case Transfer::RoguelikeUpgrade::TraitFire:
			return Transfer::RoguelikeUpgrade::SkillEnhanceFire;
		default:
			return -1;
		}
	}

	void RebuildDerivedTraitLevelsInternal(Transfer::RoguelikeUpgrade& roguelike)
	{
		for (int traitType = 0; traitType < Transfer::RoguelikeUpgrade::TraitTypeCount; ++traitType)
		{
			int skillContribution = 0;
			const int enhancementType = GetSkillEnhancementTypeFromTraitType(traitType);
			if (enhancementType >= 0)
			{
				for (int slotIndex = 0; slotIndex < Transfer::RoguelikeUpgrade::kSkillSlotCount; ++slotIndex)
				{
					const int actionSkillType = roguelike.loadoutSkills[slotIndex];
					if (actionSkillType <= Transfer::RoguelikeUpgrade::ActionSkillNone ||
						actionSkillType >= Transfer::RoguelikeUpgrade::ActionSkillTypeCount)
					{
						continue;
					}
					if (roguelike.actionSkillEnhancements[actionSkillType][enhancementType] != 0)
					{
						++skillContribution;
					}
				}
			}

			int weaponContribution = 0;
			for (int weaponUpgradeType = 0;
				 weaponUpgradeType < Transfer::RoguelikeUpgrade::kWeaponUpgradeTypeCount;
				 ++weaponUpgradeType)
			{
				if (roguelike.weaponUpgradeOwned[weaponUpgradeType] == 0)
				{
					continue;
				}
				if (GetWeaponUpgradeTagType(weaponUpgradeType) == traitType)
				{
					weaponContribution = 1;
					break;
				}
			}

			const int nodeContribution = ClampInt(roguelike.traitNodeLevels[traitType], 0, 4);
			roguelike.traitLevels[traitType] = ClampInt(
				nodeContribution + skillContribution + weaponContribution,
				0,
				Transfer::RoguelikeUpgrade::kTraitLevelMax);
		}
	}

	int CountOwnedWeaponUpgrades(const Transfer::RoguelikeUpgrade& roguelike)
	{
		int count = 0;
		for (int i = 0; i < Transfer::RoguelikeUpgrade::kWeaponUpgradeTypeCount; ++i)
		{
			if (roguelike.weaponUpgradeOwned[i] != 0)
			{
				++count;
			}
		}
		return count;
	}

	int CountOwnedSkillEnhancements(
		const Transfer::RoguelikeUpgrade& roguelike,
		int actionSkillType)
	{
		if (actionSkillType <= Transfer::RoguelikeUpgrade::ActionSkillNone ||
			actionSkillType >= Transfer::RoguelikeUpgrade::ActionSkillTypeCount)
		{
			return 0;
		}

		int count = 0;
		for (int i = 0; i < Transfer::RoguelikeUpgrade::kSkillEnhancementTypeCount; ++i)
		{
			if (roguelike.actionSkillEnhancements[actionSkillType][i] != 0)
			{
				++count;
			}
		}
		return count;
	}

	void AddPackedOfferUnique(int packedOffer, int available[], int& availableCount, int maxCount)
	{
		if (packedOffer < 0 || !available || availableCount < 0 || availableCount >= maxCount)
		{
			return;
		}

		for (int i = 0; i < availableCount; ++i)
		{
			if (available[i] == packedOffer)
			{
				return;
			}
		}

		available[availableCount++] = packedOffer;
	}

	bool CanOfferTraitLevel(const Transfer::RoguelikeUpgrade& roguelike, int traitType)
	{
		if (traitType < 0 || traitType >= Transfer::RoguelikeUpgrade::TraitTypeCount)
		{
			return false;
		}

		if (roguelike.disabledTags[traitType] != 0)
		{
			return false;
		}

		return ClampInt(roguelike.traitNodeLevels[traitType], 0, 4) < 4;
	}

	bool CanOfferWeaponUpgradeType(const Transfer::RoguelikeUpgrade& roguelike, int weaponUpgradeType)
	{
		if (weaponUpgradeType < 0 || weaponUpgradeType >= Transfer::RoguelikeUpgrade::kWeaponUpgradeTypeCount)
		{
			return false;
		}
		if (CountOwnedWeaponUpgrades(roguelike) >= 4)
		{
			return false;
		}
		if (roguelike.weaponUpgradeOwned[weaponUpgradeType] != 0)
		{
			return false;
		}

		const int tagType = GetWeaponUpgradeTagType(weaponUpgradeType);
		if (tagType >= 0 &&
			tagType < Transfer::RoguelikeUpgrade::TagTypeCount &&
			roguelike.disabledTags[tagType] != 0)
		{
			return false;
		}

		return true;
	}

	bool CanOfferSkillEnhancementType(
		const Transfer::RoguelikeUpgrade& roguelike,
		int actionSkillType,
		int enhancementType)
	{
		if (actionSkillType <= Transfer::RoguelikeUpgrade::ActionSkillNone ||
			actionSkillType >= Transfer::RoguelikeUpgrade::ActionSkillTypeCount)
		{
			return false;
		}
		if (enhancementType < 0 ||
			enhancementType >= Transfer::RoguelikeUpgrade::kSkillEnhancementTypeCount)
		{
			return false;
		}
		if (!HasLoadoutActionSkill(roguelike, actionSkillType))
		{
			return false;
		}
		if (CountOwnedSkillEnhancements(roguelike, actionSkillType) >= 4)
		{
			return false;
		}
		if (roguelike.actionSkillEnhancements[actionSkillType][enhancementType] != 0)
		{
			return false;
		}

		const int tagType = GetSkillEnhancementTagType(enhancementType);
		if (tagType >= 0 &&
			tagType < Transfer::RoguelikeUpgrade::TagTypeCount &&
			roguelike.disabledTags[tagType] != 0)
		{
			return false;
		}

		return true;
	}

	bool CanOfferSkillChangeType(
		const Transfer::RoguelikeUpgrade& roguelike,
		int slotIndex,
		int actionSkillType)
	{
		if (slotIndex < 0 || slotIndex >= Transfer::RoguelikeUpgrade::kSkillSlotCount)
		{
			return false;
		}
		if (actionSkillType <= Transfer::RoguelikeUpgrade::ActionSkillNone ||
			actionSkillType >= Transfer::RoguelikeUpgrade::ActionSkillTypeCount)
		{
			return false;
		}

		const int currentSkill = roguelike.loadoutSkills[slotIndex];
		if (currentSkill <= Transfer::RoguelikeUpgrade::ActionSkillNone)
		{
			return false;
		}
		if (currentSkill == actionSkillType)
		{
			return false;
		}

		for (int i = 0; i < Transfer::RoguelikeUpgrade::kSkillSlotCount; ++i)
		{
			if (i != slotIndex && roguelike.loadoutSkills[i] == actionSkillType)
			{
				return false;
			}
		}

		return true;
	}

	bool CanOfferTagDisableType(const Transfer::RoguelikeUpgrade& roguelike, int tagType)
	{
		if (tagType < 0 || tagType >= Transfer::RoguelikeUpgrade::TagTypeCount)
		{
			return false;
		}
		return roguelike.disabledTags[tagType] == 0;
	}

	bool CanOfferArtifactType(const Transfer::RoguelikeUpgrade& roguelike, int artifactType)
	{
		if (artifactType < 0 || artifactType >= Transfer::RoguelikeUpgrade::ArtifactTypeCount)
		{
			return false;
		}
		return roguelike.ownedArtifacts[artifactType] == 0;
	}

	void AppendTraitOffers(
		const Transfer::RoguelikeUpgrade& roguelike,
		int available[],
		int& availableCount,
		int maxCount)
	{
		for (int traitType = 0; traitType < Transfer::RoguelikeUpgrade::TraitTypeCount; ++traitType)
		{
			if (CanOfferTraitLevel(roguelike, traitType))
			{
				AddPackedOfferUnique(
					PackOfferValue(Transfer::RoguelikeUpgrade::OfferTrait, traitType),
					available,
					availableCount,
					maxCount);
			}
		}
	}

	void AppendWeaponUpgradeOffers(
		const Transfer::RoguelikeUpgrade& roguelike,
		int available[],
		int& availableCount,
		int maxCount)
	{
		for (int weaponUpgradeType = 0;
			 weaponUpgradeType < Transfer::RoguelikeUpgrade::kWeaponUpgradeTypeCount;
			 ++weaponUpgradeType)
		{
			if (CanOfferWeaponUpgradeType(roguelike, weaponUpgradeType))
			{
				AddPackedOfferUnique(
					PackOfferValue(Transfer::RoguelikeUpgrade::OfferWeaponUpgrade, weaponUpgradeType),
					available,
					availableCount,
					maxCount);
			}
		}
	}

	void AppendSkillEnhancementOffers(
		const Transfer::RoguelikeUpgrade& roguelike,
		int available[],
		int& availableCount,
		int maxCount)
	{
		for (int slotIndex = 0; slotIndex < Transfer::RoguelikeUpgrade::kSkillSlotCount; ++slotIndex)
		{
			const int actionSkillType = roguelike.loadoutSkills[slotIndex];
			for (int enhancementType = 0;
				 enhancementType < Transfer::RoguelikeUpgrade::kSkillEnhancementTypeCount;
				 ++enhancementType)
			{
				if (CanOfferSkillEnhancementType(roguelike, actionSkillType, enhancementType))
				{
					AddPackedOfferUnique(
						PackOfferValue(
							Transfer::RoguelikeUpgrade::OfferSkillEnhance,
							actionSkillType,
							enhancementType),
						available,
						availableCount,
						maxCount);
				}
			}
		}
	}

	void AppendSkillChangeOffers(
		const Transfer::RoguelikeUpgrade& roguelike,
		int available[],
		int& availableCount,
		int maxCount)
	{
		for (int slotIndex = 0; slotIndex < Transfer::RoguelikeUpgrade::kSkillSlotCount; ++slotIndex)
		{
			for (int actionSkillType = Transfer::RoguelikeUpgrade::ActionSkillWhirl;
				 actionSkillType < Transfer::RoguelikeUpgrade::ActionSkillTypeCount;
				 ++actionSkillType)
			{
				if (CanOfferSkillChangeType(roguelike, slotIndex, actionSkillType))
				{
					AddPackedOfferUnique(
						PackOfferValue(
							Transfer::RoguelikeUpgrade::OfferSkillChange,
							slotIndex,
							actionSkillType),
						available,
						availableCount,
						maxCount);
				}
			}
		}
	}

	void AppendTagDisableOffers(
		const Transfer::RoguelikeUpgrade& roguelike,
		int available[],
		int& availableCount,
		int maxCount)
	{
		for (int tagType = 0; tagType < Transfer::RoguelikeUpgrade::TagTypeCount; ++tagType)
		{
			if (CanOfferTagDisableType(roguelike, tagType))
			{
				AddPackedOfferUnique(
					PackOfferValue(Transfer::RoguelikeUpgrade::OfferTagDisable, tagType),
					available,
					availableCount,
					maxCount);
			}
		}
	}

	void AppendArtifactOffers(
		const Transfer::RoguelikeUpgrade& roguelike,
		int available[],
		int& availableCount,
		int maxCount)
	{
		for (int artifactType = 0; artifactType < Transfer::RoguelikeUpgrade::ArtifactTypeCount; ++artifactType)
		{
			if (CanOfferArtifactType(roguelike, artifactType))
			{
				AddPackedOfferUnique(
					PackOfferValue(Transfer::RoguelikeUpgrade::OfferArtifact, artifactType),
					available,
					availableCount,
					maxCount);
			}
		}
	}

	void FillPackedOfferArrayFromAvailable(
		int offers[kUpgradeOfferCount],
		int available[],
		int availableCount)
	{
		if (availableCount <= 0)
		{
			for (int i = 0; i < kUpgradeOfferCount; ++i)
			{
				offers[i] = kUpgradeOfferNone;
			}
			return;
		}

		for (int i = 0; i < availableCount; ++i)
		{
			const int j = RandRangeInt(i, availableCount - 1);
			const int tmp = available[i];
			available[i] = available[j];
			available[j] = tmp;
		}

		for (int i = 0; i < kUpgradeOfferCount; ++i)
		{
			offers[i] = (i < availableCount)
				? available[i]
				: available[RandRangeInt(0, availableCount - 1)];
		}
	}

	void BuildOffersForSelectionPhase(
		const Transfer::RoguelikeUpgrade& roguelike,
		int selectionPhase,
		int offers[kUpgradeOfferCount])
	{
		constexpr int kMaxAvailableOffers = 64;
		int available[kMaxAvailableOffers]{};
		int availableCount = 0;

		switch (NormalizeSelectionPhaseValue(selectionPhase))
		{
		case Transfer::RoguelikeUpgrade::SelectionRewardSkill:
		case Transfer::RoguelikeUpgrade::SelectionShopSkillEnhance:
			AppendSkillEnhancementOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			if (availableCount <= 0)
			{
				AppendWeaponUpgradeOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			if (availableCount <= 0)
			{
				AppendTraitOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			break;

		case Transfer::RoguelikeUpgrade::SelectionRewardTrait:
		case Transfer::RoguelikeUpgrade::SelectionShopTrait:
			AppendTraitOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			if (availableCount <= 0)
			{
				AppendWeaponUpgradeOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			if (availableCount <= 0)
			{
				AppendSkillEnhancementOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			break;

		case Transfer::RoguelikeUpgrade::SelectionRewardTag:
			AppendTagDisableOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			if (availableCount <= 0)
			{
				AppendTraitOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			break;

		case Transfer::RoguelikeUpgrade::SelectionRewardWeapon:
			AppendWeaponUpgradeOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			if (availableCount <= 0)
			{
				AppendTraitOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			break;

		case Transfer::RoguelikeUpgrade::SelectionRewardArtifact:
			AppendArtifactOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			if (availableCount <= 0)
			{
				AppendTraitOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			}
			break;

		case Transfer::RoguelikeUpgrade::SelectionShopSkillChange:
			AppendSkillChangeOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			break;

		case Transfer::RoguelikeUpgrade::SelectionMixed:
			AppendSkillEnhancementOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			AppendTraitOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			AppendWeaponUpgradeOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			AppendTagDisableOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			AppendArtifactOffers(roguelike, available, availableCount, kMaxAvailableOffers);
			break;

		default:
			for (int i = 0; i < kUpgradeOfferCount; ++i)
			{
				offers[i] = kUpgradeOfferNone;
			}
			return;
		}

		FillPackedOfferArrayFromAvailable(offers, available, availableCount);
	}

	void MoveSkillEnhancementsToNewSkill(
		Transfer::RoguelikeUpgrade& roguelike,
		int oldSkillType,
		int newSkillType)
	{
		if (oldSkillType <= Transfer::RoguelikeUpgrade::ActionSkillNone ||
			oldSkillType >= Transfer::RoguelikeUpgrade::ActionSkillTypeCount ||
			newSkillType <= Transfer::RoguelikeUpgrade::ActionSkillNone ||
			newSkillType >= Transfer::RoguelikeUpgrade::ActionSkillTypeCount ||
			oldSkillType == newSkillType)
		{
			return;
		}

		for (int enhancementType = 0;
			 enhancementType < Transfer::RoguelikeUpgrade::kSkillEnhancementTypeCount;
			 ++enhancementType)
		{
			if (roguelike.actionSkillEnhancements[oldSkillType][enhancementType] != 0)
			{
				roguelike.actionSkillEnhancements[newSkillType][enhancementType] = 1;
				roguelike.actionSkillEnhancements[oldSkillType][enhancementType] = 0;
			}
		}
	}

	bool ApplyPackedOffer(Transfer::RoguelikeUpgrade& roguelike, int packedOffer)
	{
		const int offerType = GetOfferTypeValue(packedOffer);
		const int primaryValue = GetOfferPrimaryValue(packedOffer);
		const int secondaryValue = GetOfferSecondaryValue(packedOffer);

		switch (offerType)
		{
		case Transfer::RoguelikeUpgrade::OfferTrait:
			if (!CanOfferTraitLevel(roguelike, primaryValue))
			{
				return false;
			}
			roguelike.traitNodeLevels[primaryValue] = ClampInt(
				roguelike.traitNodeLevels[primaryValue] + 1,
				0,
				4);
			RebuildDerivedTraitLevelsInternal(roguelike);
			return true;

		case Transfer::RoguelikeUpgrade::OfferWeaponUpgrade:
			if (!CanOfferWeaponUpgradeType(roguelike, primaryValue))
			{
				return false;
			}
			roguelike.weaponUpgradeOwned[primaryValue] = 1;
			RebuildDerivedTraitLevelsInternal(roguelike);
			return true;

		case Transfer::RoguelikeUpgrade::OfferSkillEnhance:
			if (!CanOfferSkillEnhancementType(roguelike, primaryValue, secondaryValue))
			{
				return false;
			}
			roguelike.actionSkillEnhancements[primaryValue][secondaryValue] = 1;
			RebuildDerivedTraitLevelsInternal(roguelike);
			return true;

		case Transfer::RoguelikeUpgrade::OfferSkillChange:
			if (!CanOfferSkillChangeType(roguelike, primaryValue, secondaryValue))
			{
				return false;
			}
			MoveSkillEnhancementsToNewSkill(
				roguelike,
				roguelike.loadoutSkills[primaryValue],
				secondaryValue);
			roguelike.loadoutSkills[primaryValue] = secondaryValue;
			RebuildDerivedTraitLevelsInternal(roguelike);
			return true;

		case Transfer::RoguelikeUpgrade::OfferTagDisable:
			if (!CanOfferTagDisableType(roguelike, primaryValue))
			{
				return false;
			}
			roguelike.disabledTags[primaryValue] = 1;
			return true;

		case Transfer::RoguelikeUpgrade::OfferArtifact:
			if (!CanOfferArtifactType(roguelike, primaryValue))
			{
				return false;
			}
			roguelike.ownedArtifacts[primaryValue] = 1;
			return true;

		default:
			return false;
		}
	}

	void ResetStageOptionRow(int row[Transfer::RoguelikeUpgrade::kOfferCount])
	{
		for (int i = 0; i < Transfer::RoguelikeUpgrade::kOfferCount; ++i)
		{
			row[i] = Transfer::RoguelikeUpgrade::RewardUnknown;
		}
	}

	void FillWeightedRewardOptions(
		int out[Transfer::RoguelikeUpgrade::kOfferCount],
		int optionCount,
		int traitWeight,
		int tagWeight,
		int weaponWeight,
		int diceWeight)
	{
		ResetStageOptionRow(out);
		optionCount = ClampInt(optionCount, 1, Transfer::RoguelikeUpgrade::kOfferCount);

		const int weights[][2] =
		{
			{ Transfer::RoguelikeUpgrade::RewardTrait, traitWeight },
			{ Transfer::RoguelikeUpgrade::RewardTag, tagWeight },
			{ Transfer::RoguelikeUpgrade::RewardWeapon, weaponWeight },
			{ Transfer::RoguelikeUpgrade::RewardDice, diceWeight }
		};

		int pool[64]{};
		int poolCount = 0;
		for (const auto& entry : weights)
		{
			const int rewardType = entry[0];
			const int weight = (entry[1] < 0) ? 0 : entry[1];
			for (int i = 0; i < weight && poolCount < static_cast<int>(std::size(pool)); ++i)
			{
				pool[poolCount++] = rewardType;
			}
		}

		for (int i = 0; i < poolCount; ++i)
		{
			const int j = RandRangeInt(i, poolCount - 1);
			const int tmp = pool[i];
			pool[i] = pool[j];
			pool[j] = tmp;
		}

		int uniqueCount = 0;
		for (int i = 0; i < poolCount && uniqueCount < optionCount; ++i)
		{
			const int rewardType = pool[i];
			bool exists = false;
			for (int j = 0; j < uniqueCount; ++j)
			{
				if (out[j] == rewardType)
				{
					exists = true;
					break;
				}
			}
			if (!exists)
			{
				out[uniqueCount++] = rewardType;
			}
		}

		const int fallbacks[] =
		{
			Transfer::RoguelikeUpgrade::RewardTrait,
			Transfer::RoguelikeUpgrade::RewardTag,
			Transfer::RoguelikeUpgrade::RewardWeapon,
			Transfer::RoguelikeUpgrade::RewardDice
		};
		for (int rewardType : fallbacks)
		{
			if (uniqueCount >= optionCount) break;
			bool exists = false;
			for (int j = 0; j < uniqueCount; ++j)
			{
				if (out[j] == rewardType)
				{
					exists = true;
					break;
				}
			}
			if (!exists)
			{
				out[uniqueCount++] = rewardType;
			}
		}

		while (uniqueCount < optionCount)
		{
			out[uniqueCount++] = Transfer::RoguelikeUpgrade::RewardTrait;
		}
	}

	void SetRunNode(
		Transfer::RoguelikeUpgrade& roguelike,
		int index,
		int stageType,
		int rewardType,
		int mapNumber,
		int stepNumber,
		int optionCount)
	{
		if (index < 0 || index >= Transfer::RoguelikeUpgrade::kRunStageCount)
		{
			return;
		}

		roguelike.stageTypes[index] = NormalizeStageTypeValue(stageType);
		roguelike.stageRewardTypes[index] = NormalizeRewardTypeValue(rewardType);
		roguelike.stageMapNumbers[index] = ClampInt(mapNumber, 1, 3);
		roguelike.stageStepNumbers[index] = (stepNumber < 0) ? 0 : stepNumber;
		roguelike.stageOptionCounts[index] = ClampInt(optionCount, 1, Transfer::RoguelikeUpgrade::kOfferCount);
		ResetStageOptionRow(roguelike.stageOptions[index]);
		roguelike.stageOptions[index][0] = roguelike.stageRewardTypes[index];
	}

	void InitializeRunStageRoute(Transfer::RoguelikeUpgrade& roguelike)
	{
		for (int i = 0; i < Transfer::RoguelikeUpgrade::kRunStageCount; ++i)
		{
			SetRunNode(
				roguelike,
				i,
				Transfer::RoguelikeUpgrade::StageCombat,
				Transfer::RoguelikeUpgrade::RewardUnknown,
				1,
				0,
				1);
		}

		auto setupFiveStepMap = [&](int baseIndex, int mapNumber)
		{
			SetRunNode(roguelike, baseIndex + 0, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardSkill, mapNumber, 0, 1);
			SetRunNode(roguelike, baseIndex + 1, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, mapNumber, 1, 2);
			SetRunNode(roguelike, baseIndex + 2, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, mapNumber, 2, 2);
			SetRunNode(roguelike, baseIndex + 3, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardArtifact, mapNumber, 3, 1);
			SetRunNode(roguelike, baseIndex + 4, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, mapNumber, 4, 2);
			SetRunNode(roguelike, baseIndex + 5, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, mapNumber, 5, 2);
			SetRunNode(roguelike, baseIndex + 6, Transfer::RoguelikeUpgrade::StageRest, Transfer::RoguelikeUpgrade::RewardRest, mapNumber, 0, 1);
			SetRunNode(roguelike, baseIndex + 7, Transfer::RoguelikeUpgrade::StageBoss, Transfer::RoguelikeUpgrade::RewardBoss, mapNumber, 0, 1);

			FillWeightedRewardOptions(roguelike.stageOptions[baseIndex + 1], 2, 6, 2, 1, 1);
			FillWeightedRewardOptions(roguelike.stageOptions[baseIndex + 2], 2, 6, 2, 1, 1);
			FillWeightedRewardOptions(roguelike.stageOptions[baseIndex + 4], 2, 6, 2, 1, 1);
			FillWeightedRewardOptions(roguelike.stageOptions[baseIndex + 5], 2, 6, 2, 1, 1);
		};

		setupFiveStepMap(0, 1);
		setupFiveStepMap(8, 2);

		SetRunNode(roguelike, 16, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardSkill, 3, 0, 1);
		SetRunNode(roguelike, 17, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardSkill, 3, 0, 1);
		SetRunNode(roguelike, 18, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, 3, 1, 2);
		SetRunNode(roguelike, 19, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, 3, 2, 2);
		SetRunNode(roguelike, 20, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, 3, 3, 2);
		SetRunNode(roguelike, 21, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, 3, 4, 2);
		SetRunNode(roguelike, 22, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, 3, 5, 2);
		SetRunNode(roguelike, 23, Transfer::RoguelikeUpgrade::StageCombat, Transfer::RoguelikeUpgrade::RewardUnknown, 3, 6, 2);
		SetRunNode(roguelike, 24, Transfer::RoguelikeUpgrade::StageShop, Transfer::RoguelikeUpgrade::RewardShop, 3, 0, 1);
		SetRunNode(roguelike, 25, Transfer::RoguelikeUpgrade::StageRest, Transfer::RoguelikeUpgrade::RewardRest, 3, 0, 1);
		SetRunNode(roguelike, 26, Transfer::RoguelikeUpgrade::StageBoss, Transfer::RoguelikeUpgrade::RewardBoss, 3, 0, 1);
		SetRunNode(roguelike, 27, Transfer::RoguelikeUpgrade::StageFinalBoss, Transfer::RoguelikeUpgrade::RewardFinalBoss, 3, 0, 1);

		FillWeightedRewardOptions(roguelike.stageOptions[18], 2, 7, 2, 2, 1);
		FillWeightedRewardOptions(roguelike.stageOptions[19], 2, 7, 2, 2, 1);
		FillWeightedRewardOptions(roguelike.stageOptions[20], 2, 7, 2, 2, 1);
		FillWeightedRewardOptions(roguelike.stageOptions[21], 2, 7, 2, 2, 1);
		FillWeightedRewardOptions(roguelike.stageOptions[22], 2, 7, 2, 2, 1);
		FillWeightedRewardOptions(roguelike.stageOptions[23], 2, 7, 2, 2, 1);

		const bool artifactAtStep3 = (RandRangeInt(0, 1) == 0);
		const int artifactIndex = artifactAtStep3 ? 20 : 21;
		roguelike.stageRewardTypes[artifactIndex] = Transfer::RoguelikeUpgrade::RewardArtifact;
		roguelike.stageOptionCounts[artifactIndex] = 1;
		ResetStageOptionRow(roguelike.stageOptions[artifactIndex]);
		roguelike.stageOptions[artifactIndex][0] = Transfer::RoguelikeUpgrade::RewardArtifact;

		int bossPool[4] =
		{
			Transfer::RoguelikeUpgrade::BossHeavyMelee,
			Transfer::RoguelikeUpgrade::BossLightRanged,
			Transfer::RoguelikeUpgrade::BossBalancedMid,
			Transfer::RoguelikeUpgrade::BossSwiftDebuff
		};
		for (int i = 0; i < 4; ++i)
		{
			const int j = RandRangeInt(i, 3);
			const int tmp = bossPool[i];
			bossPool[i] = bossPool[j];
			bossPool[j] = tmp;
		}
		for (int i = 0; i < Transfer::RoguelikeUpgrade::kRegularBossSlotCount; ++i)
		{
			roguelike.regularBossOrder[i] = bossPool[i];
		}
		roguelike.finalBossType = Transfer::RoguelikeUpgrade::BossFinalBarrage;

		roguelike.currentStageIndex = 0;
		roguelike.currentStageType = roguelike.stageTypes[0];
		roguelike.intermissionMode = Transfer::RoguelikeUpgrade::IntermissionNone;
		roguelike.shopPurchaseCount = 0;
	}

	bool HasSkillType(int skillSlot1, int skillSlot2, int skillType)
	{
		return skillSlot1 == skillType || skillSlot2 == skillType;
	}

	bool HasEmptySkillSlot(int skillSlot1, int skillSlot2)
	{
		return skillSlot1 == Transfer::RoguelikeUpgrade::SkillNone ||
			   skillSlot2 == Transfer::RoguelikeUpgrade::SkillNone;
	}

	int UpgradeTypeToSkillType(int upgradeType)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeSkillShot:
			return Transfer::RoguelikeUpgrade::SkillShot;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNova:
			return Transfer::RoguelikeUpgrade::SkillNova;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbit:
			return Transfer::RoguelikeUpgrade::SkillOrbit;
		default:
			return Transfer::RoguelikeUpgrade::SkillNone;
		}
	}

	/**
	 * @brief 指定強化タイプがまだ提示可能かどうかを返します。
	 * @param upgradeType 判定する強化タイプです。
	 * @param attackPowerLevel 現在の攻撃力レベルです。
	 * @param attackSpeedLevel 現在の攻撃速度レベルです。
	 * @param evadeCooldownLevel 現在の回避短縮レベルです。
	 * @return 上限未到達なら true です。
	 */
	bool IsUpgradeTypeAvailable(int upgradeType,
								int attackPowerLevel,
								int attackSpeedLevel,
								int evadeCooldownLevel,
								int skillSlot1,
								int skillSlot2)
	{
		switch (upgradeType)
		{
		case 0:
		case 3:
			return attackPowerLevel < kUpgradeTierMax;
		case 1:
		case 4:
			return attackSpeedLevel < kUpgradeTierMax;
		case 2:
		case 5:
			return evadeCooldownLevel < kUpgradeTierMax;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShot:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNova:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbit:
		{
			const int skillType = UpgradeTypeToSkillType(upgradeType);
			return skillType != Transfer::RoguelikeUpgrade::SkillNone &&
				   HasEmptySkillSlot(skillSlot1, skillSlot2) &&
				   !HasSkillType(skillSlot1, skillSlot2, skillType);
		}
		default:
			return false;
		}
	}

	/**
	 * @brief 現在レベルに応じて提示可能な強化候補を生成します。
	 * @param offers 生成した候補の書き込み先です。
	 * @param attackPowerLevel 現在の攻撃力レベルです。
	 * @param attackSpeedLevel 現在の攻撃速度レベルです。
	 * @param evadeCooldownLevel 現在の回避短縮レベルです。
	 */
	void GenerateUpgradeOffers(int offers[kUpgradeOfferCount],
							   int attackPowerLevel,
							   int attackSpeedLevel,
							   int evadeCooldownLevel,
							   int skillSlot1,
							   int skillSlot2)
	{
		int available[kUpgradeTypeCount]{};
		int availableCount = 0;
		for (int t = 0; t < kUpgradeTypeCount; ++t)
		{
			// 上限に達していない候補だけ抽出します。
			if (IsUpgradeTypeAvailable(t, attackPowerLevel, attackSpeedLevel, evadeCooldownLevel, skillSlot1, skillSlot2))
			{
				available[availableCount++] = t;
			}
		}

		if (availableCount == 0)
		{
			// 提示可能な強化が無い場合は全スロットを空にします。
			for (int i = 0; i < kUpgradeOfferCount; ++i)
			{
				offers[i] = kUpgradeOfferNone;
			}
			return;
		}

		// Fisher-Yates 風に並びを崩し、提示順を毎回変えます。
		for (int i = 0; i < availableCount; ++i)
		{
			const int j = RandRangeInt(i, availableCount - 1);
			const int tmp = available[i];
			available[i] = available[j];
			available[j] = tmp;
		}

		// 候補数が足りない場合は、利用可能候補から重複許可で埋めます。
		for (int i = 0; i < kUpgradeOfferCount; ++i)
		{
			if (i < availableCount)
			{
				offers[i] = available[i];
			}
			else
			{
				offers[i] = available[RandRangeInt(0, availableCount - 1)];
			}
		}
	}

	/**
	 * @brief 強化タイプに応じて対象レベルを増やします。
	 * @param attackPowerLevel 攻撃力レベル参照です。
	 * @param attackSpeedLevel 攻撃速度レベル参照です。
	 * @param evadeCooldownLevel 回避短縮レベル参照です。
	 * @param upgradeType 適用する強化タイプです。
	 * @param smallStep 小強化量です。
	 * @param largeStep 大強化量です。
	 */
	void ApplyUpgradeType(int& attackPowerLevel,
						  int& attackSpeedLevel,
						  int& evadeCooldownLevel,
						  int& skillSlot1,
						  int& skillSlot2,
						  int upgradeType,
						  int smallStep,
						  int largeStep)
	{
		switch (upgradeType)
		{
		case 0:
			attackPowerLevel = ClampUpgradeTier(attackPowerLevel + smallStep);
			break;
		case 1:
			attackSpeedLevel = ClampUpgradeTier(attackSpeedLevel + smallStep);
			break;
		case 2:
			evadeCooldownLevel = ClampUpgradeTier(evadeCooldownLevel + smallStep);
			break;
		case 3:
			attackPowerLevel = ClampUpgradeTier(attackPowerLevel + largeStep);
			break;
		case 4:
			attackSpeedLevel = ClampUpgradeTier(attackSpeedLevel + largeStep);
			break;
		case 5:
			evadeCooldownLevel = ClampUpgradeTier(evadeCooldownLevel + largeStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShot:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNova:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbit:
		{
			const int skillType = UpgradeTypeToSkillType(upgradeType);
			if (skillType == Transfer::RoguelikeUpgrade::SkillNone ||
				HasSkillType(skillSlot1, skillSlot2, skillType))
			{
				break;
			}
			if (skillSlot1 == Transfer::RoguelikeUpgrade::SkillNone)
			{
				skillSlot1 = skillType;
			}
			else if (skillSlot2 == Transfer::RoguelikeUpgrade::SkillNone)
			{
				skillSlot2 = skillType;
			}
			break;
		}
		default:
			break;
		}
	}

	void ResetOffers(int offers[kUpgradeOfferCount])
	{
		for (int i = 0; i < kUpgradeOfferCount; ++i)
		{
			offers[i] = kUpgradeOfferNone;
		}
	}

	bool HasAnyOffer(const int offers[kUpgradeOfferCount])
	{
		for (int i = 0; i < kUpgradeOfferCount; ++i)
		{
			if (offers[i] != kUpgradeOfferNone)
			{
				return true;
			}
		}
		return false;
	}

	int GetTotalUpgradeProgressValue(const Transfer::RoguelikeUpgrade& roguelike)
	{
		int total = 0;
		total += ClampUpgradeTier(roguelike.attackPowerLevel);
		total += ClampUpgradeTier(roguelike.attackSpeedLevel);
		total += ClampUpgradeTier(roguelike.evadeCooldownLevel);
		total += (roguelike.skillSlot1 != Transfer::RoguelikeUpgrade::SkillNone) ? 1 : 0;
		total += (roguelike.skillSlot2 != Transfer::RoguelikeUpgrade::SkillNone) ? 1 : 0;
		total += ClampInt(roguelike.skillShotRangeLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillShotPowerLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillShotCooldownLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillNovaRangeLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillNovaPowerLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillNovaCooldownLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillOrbitRangeLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillOrbitCooldownLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		total += ClampInt(roguelike.skillOrbitCountLevel, 0, Transfer::RoguelikeUpgrade::kLevelMax);
		return total;
	}

	float EvaluateUpgradeProgressScale(int progressValue,
									   float scaleAt20,
									   float scaleAt40,
									   float scaleAt60)
	{
		const float progress = static_cast<float>((progressValue < 0) ? 0 : progressValue);
		if (progress <= 20.0f)
		{
			return 1.0f + (scaleAt20 - 1.0f) * (progress / 20.0f);
		}
		if (progress <= 40.0f)
		{
			return scaleAt20 + (scaleAt40 - scaleAt20) * ((progress - 20.0f) / 20.0f);
		}
		if (progress <= 60.0f)
		{
			return scaleAt40 + (scaleAt60 - scaleAt40) * ((progress - 40.0f) / 20.0f);
		}

		return scaleAt60 + (scaleAt60 - scaleAt40) * ((progress - 60.0f) / 20.0f);
	}

	int* GetSkillUpgradeLevelPtr(Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotRange: return &roguelike.skillShotRangeLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotPower: return &roguelike.skillShotPowerLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotCooldown: return &roguelike.skillShotCooldownLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaRange: return &roguelike.skillNovaRangeLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaPower: return &roguelike.skillNovaPowerLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaCooldown: return &roguelike.skillNovaCooldownLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitRange: return &roguelike.skillOrbitRangeLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCooldown: return &roguelike.skillOrbitCooldownLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCount: return &roguelike.skillOrbitCountLevel;
		default: return nullptr;
		}
	}

	const int* GetSkillUpgradeLevelPtr(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotRange: return &roguelike.skillShotRangeLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotPower: return &roguelike.skillShotPowerLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotCooldown: return &roguelike.skillShotCooldownLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaRange: return &roguelike.skillNovaRangeLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaPower: return &roguelike.skillNovaPowerLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaCooldown: return &roguelike.skillNovaCooldownLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitRange: return &roguelike.skillOrbitRangeLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCooldown: return &roguelike.skillOrbitCooldownLevel;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCount: return &roguelike.skillOrbitCountLevel;
		default: return nullptr;
		}
	}

	bool IsStatusUpgradeTypeAvailable(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeAttackPower:
		case Transfer::RoguelikeUpgrade::UpgradeAttackPowerLarge:
			return roguelike.attackPowerLevel < kUpgradeTierMax;
		case Transfer::RoguelikeUpgrade::UpgradeAttackSpeed:
		case Transfer::RoguelikeUpgrade::UpgradeAttackSpeedLarge:
			return roguelike.attackSpeedLevel < kUpgradeTierMax;
		case Transfer::RoguelikeUpgrade::UpgradeEvadeCooldown:
		case Transfer::RoguelikeUpgrade::UpgradeEvadeCooldownLarge:
			return roguelike.evadeCooldownLevel < kUpgradeTierMax;
		default:
			return false;
		}
	}

	bool IsSkillRewardTypeAvailable(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeSkillShot:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNova:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbit:
		{
			const int skillType = UpgradeTypeToSkillType(upgradeType);
			return skillType != Transfer::RoguelikeUpgrade::SkillNone &&
				   HasEmptySkillSlot(roguelike.skillSlot1, roguelike.skillSlot2) &&
				   !HasSkillType(roguelike.skillSlot1, roguelike.skillSlot2, skillType);
		}
		default:
			break;
		}

		return false;
	}

	bool IsSkillUpgradeTypeAvailable(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotRange:
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotPower:
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotCooldown:
			return HasSkillType(roguelike.skillSlot1, roguelike.skillSlot2, Transfer::RoguelikeUpgrade::SkillShot) &&
				   GetSkillUpgradeLevelPtr(roguelike, upgradeType) &&
				   *GetSkillUpgradeLevelPtr(roguelike, upgradeType) < kSkillUpgradeTierMax;
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaRange:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaPower:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaCooldown:
			return HasSkillType(roguelike.skillSlot1, roguelike.skillSlot2, Transfer::RoguelikeUpgrade::SkillNova) &&
				   GetSkillUpgradeLevelPtr(roguelike, upgradeType) &&
				   *GetSkillUpgradeLevelPtr(roguelike, upgradeType) < kSkillUpgradeTierMax;
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitRange:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCooldown:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCount:
			return HasSkillType(roguelike.skillSlot1, roguelike.skillSlot2, Transfer::RoguelikeUpgrade::SkillOrbit) &&
				   GetSkillUpgradeLevelPtr(roguelike, upgradeType) &&
				   *GetSkillUpgradeLevelPtr(roguelike, upgradeType) < kSkillUpgradeTierMax;
		default:
			return false;
		}
	}

	bool IsRewardFollowupTypeAvailable(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		if (HasEmptySkillSlot(roguelike.skillSlot1, roguelike.skillSlot2))
		{
			return IsSkillRewardTypeAvailable(roguelike, upgradeType);
		}

		return IsStatusUpgradeTypeAvailable(roguelike, upgradeType) ||
			   IsSkillUpgradeTypeAvailable(roguelike, upgradeType);
	}

	bool IsShopSkillTypeAvailable(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		return IsSkillRewardTypeAvailable(roguelike, upgradeType) ||
			   IsSkillUpgradeTypeAvailable(roguelike, upgradeType);
	}

	bool IsMixedRewardTypeAvailable(const Transfer::RoguelikeUpgrade& roguelike, int upgradeType)
	{
		return IsStatusUpgradeTypeAvailable(roguelike, upgradeType) ||
			   IsSkillRewardTypeAvailable(roguelike, upgradeType) ||
			   IsSkillUpgradeTypeAvailable(roguelike, upgradeType);
	}

	void FillOfferArrayFromAvailable(int offers[kUpgradeOfferCount], int available[kUpgradeTypeCount], int availableCount)
	{
		if (availableCount <= 0)
		{
			ResetOffers(offers);
			return;
		}

		for (int i = 0; i < availableCount; ++i)
		{
			const int j = RandRangeInt(i, availableCount - 1);
			const int tmp = available[i];
			available[i] = available[j];
			available[j] = tmp;
		}

		for (int i = 0; i < kUpgradeOfferCount; ++i)
		{
			if (i < availableCount)
			{
				offers[i] = available[i];
			}
			else
			{
				offers[i] = available[RandRangeInt(0, availableCount - 1)];
			}
		}
	}

	void GenerateStatusOffers(int offers[kUpgradeOfferCount], const Transfer::RoguelikeUpgrade& roguelike)
	{
		int available[kUpgradeTypeCount]{};
		int availableCount = 0;
		for (int t = Transfer::RoguelikeUpgrade::UpgradeAttackPower;
			 t <= Transfer::RoguelikeUpgrade::UpgradeEvadeCooldownLarge;
			 ++t)
		{
			if (IsStatusUpgradeTypeAvailable(roguelike, t))
			{
				available[availableCount++] = t;
			}
		}
		FillOfferArrayFromAvailable(offers, available, availableCount);
	}

	void GenerateSkillOffers(int offers[kUpgradeOfferCount], const Transfer::RoguelikeUpgrade& roguelike)
	{
		int available[kUpgradeTypeCount]{};
		int availableCount = 0;
		for (int t = Transfer::RoguelikeUpgrade::UpgradeSkillShot;
			 t < Transfer::RoguelikeUpgrade::UpgradeTypeCount;
			 ++t)
		{
			if (IsShopSkillTypeAvailable(roguelike, t))
			{
				available[availableCount++] = t;
			}
		}
		FillOfferArrayFromAvailable(offers, available, availableCount);
	}

	void GenerateRewardFollowupOffers(int offers[kUpgradeOfferCount], const Transfer::RoguelikeUpgrade& roguelike)
	{
		int available[kUpgradeTypeCount]{};
		int availableCount = 0;
		for (int t = Transfer::RoguelikeUpgrade::UpgradeAttackPower;
			 t < Transfer::RoguelikeUpgrade::UpgradeTypeCount;
			 ++t)
		{
			if (IsRewardFollowupTypeAvailable(roguelike, t))
			{
				available[availableCount++] = t;
			}
		}
		FillOfferArrayFromAvailable(offers, available, availableCount);
	}

	void GenerateMixedOffers(int offers[kUpgradeOfferCount], const Transfer::RoguelikeUpgrade& roguelike)
	{
		int available[kUpgradeTypeCount]{};
		int availableCount = 0;
		for (int t = Transfer::RoguelikeUpgrade::UpgradeAttackPower;
			 t < Transfer::RoguelikeUpgrade::UpgradeTypeCount;
			 ++t)
		{
			if (IsMixedRewardTypeAvailable(roguelike, t))
			{
				available[availableCount++] = t;
			}
		}
		FillOfferArrayFromAvailable(offers, available, availableCount);
	}

	void GenerateUpgradeOffers(int offers[kUpgradeOfferCount],
							   const Transfer::RoguelikeUpgrade& roguelike,
							   int selectionPhase)
	{
		BuildOffersForSelectionPhase(roguelike, selectionPhase, offers);
	}

	void ApplyUpgradeType(Transfer::RoguelikeUpgrade& roguelike,
						  int upgradeType,
						  int smallStep,
						  int largeStep)
	{
		switch (upgradeType)
		{
		case Transfer::RoguelikeUpgrade::UpgradeAttackPower:
			roguelike.attackPowerLevel = ClampUpgradeTier(roguelike.attackPowerLevel + smallStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeAttackSpeed:
			roguelike.attackSpeedLevel = ClampUpgradeTier(roguelike.attackSpeedLevel + smallStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeEvadeCooldown:
			roguelike.evadeCooldownLevel = ClampUpgradeTier(roguelike.evadeCooldownLevel + smallStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeAttackPowerLarge:
			roguelike.attackPowerLevel = ClampUpgradeTier(roguelike.attackPowerLevel + largeStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeAttackSpeedLarge:
			roguelike.attackSpeedLevel = ClampUpgradeTier(roguelike.attackSpeedLevel + largeStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeEvadeCooldownLarge:
			roguelike.evadeCooldownLevel = ClampUpgradeTier(roguelike.evadeCooldownLevel + largeStep);
			break;
		case Transfer::RoguelikeUpgrade::UpgradeSkillShot:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNova:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbit:
		{
			const int skillType = UpgradeTypeToSkillType(upgradeType);
			if (skillType == Transfer::RoguelikeUpgrade::SkillNone ||
				HasSkillType(roguelike.skillSlot1, roguelike.skillSlot2, skillType))
			{
				break;
			}
			if (roguelike.skillSlot1 == Transfer::RoguelikeUpgrade::SkillNone)
			{
				roguelike.skillSlot1 = skillType;
			}
			else if (roguelike.skillSlot2 == Transfer::RoguelikeUpgrade::SkillNone)
			{
				roguelike.skillSlot2 = skillType;
			}
			break;
		}
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotRange:
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotPower:
		case Transfer::RoguelikeUpgrade::UpgradeSkillShotCooldown:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaRange:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaPower:
		case Transfer::RoguelikeUpgrade::UpgradeSkillNovaCooldown:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitRange:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCooldown:
		case Transfer::RoguelikeUpgrade::UpgradeSkillOrbitCount:
		{
			int* levelPtr = GetSkillUpgradeLevelPtr(roguelike, upgradeType);
			if (levelPtr)
			{
				*levelPtr = ClampInt(*levelPtr + 1, 0, kSkillUpgradeTierMax);
			}
			break;
		}
		default:
			break;
		}
	}
}

/**
 * @brief ゲームプレイ調整値を既定値へ戻します。
 */
void Transfer::ResetGameplayTuningToDefault()
{
	// 既定構築子で丸ごと初期値へ戻します。
	gameplay = GameplayTuning{};
}

/**
 * @brief ローグライク強化状態を初期値へ戻します。
 */
void Transfer::ResetRoguelikeUpgrade()
{
	// 選択候補や残リロールも含めて、すべて初期化します。
	roguelike = RoguelikeUpgrade{};
	InitializeRunStageRoute(roguelike);
	RebuildDerivedTraitLevels();
	roguelike.rerollMaxPerStage = 0;
	roguelike.rerollRemain = kInitialRunRerollCount;
	roguelike.reviveMax = ClampInt(roguelike.reviveMax, 0, 99);
	if (roguelike.reviveMax <= 0)
	{
		roguelike.reviveMax = kDefaultRunReviveCount;
	}
	roguelike.reviveRemain = roguelike.reviveMax;

	const float defaultMaxHp = (player.maxHp > 0.0f) ? player.maxHp : kDefaultPlayerMaxHp;
	player.maxHp = defaultMaxHp;
	player.hp = defaultMaxHp;
}

void Transfer::RebuildDerivedTraitLevels()
{
	RebuildDerivedTraitLevelsInternal(roguelike);
}

/**
 * @brief キーコンフィグを初期値へ戻します。
 */
void Transfer::ResetInputConfigToDefault()
{
	input = InputConfig{};
}

/**
 * @brief 難易度プリセットに応じた基準値を反映します。
 * @param preset 0:Easy 1:Normal 2:Hard の難易度値です。
 */
void Transfer::ApplyDifficultyPreset(int preset)
{
	const int normalizedPreset = NormalizeDifficultyPresetValue(preset);
	// 難易度ごとに敵数、Wave 数、敵性能の基準値を切り替えます。
	switch (normalizedPreset)
	{
	case 0: // Easy
		gameplay.enemyCount = 2;
		gameplay.waveMax = 3;
		gameplay.waveEnemyAddPerWave = 1;
		gameplay.enemyAttackWindup = 0.65f;
		gameplay.enemyAttackCooldown = 1.20f;
		gameplay.enemyAttackRangeMin = 0.70f;
		gameplay.enemyAttackRangeScale = 1.20f;
		gameplay.enemyAttackDamage = 0.8f;
		gameplay.enemyMoveSpeed = 1.00f;
		gameplay.waveEnemyMoveSpeedAdd = 0.08f;
		gameplay.waveEnemyAttackDamageScalePerWave = 0.10f;
		break;
	case 2: // Hard
		gameplay.enemyCount = 4;
		gameplay.waveMax = 3;
		gameplay.waveEnemyAddPerWave = 2;
		gameplay.enemyAttackWindup = 0.45f;
		gameplay.enemyAttackCooldown = 0.80f;
		gameplay.enemyAttackRangeMin = 0.90f;
		gameplay.enemyAttackRangeScale = 1.45f;
		gameplay.enemyAttackDamage = 1.3f;
		gameplay.enemyMoveSpeed = 1.35f;
		gameplay.waveEnemyMoveSpeedAdd = 0.22f;
		gameplay.waveEnemyAttackDamageScalePerWave = 0.32f;
		break;
	default: // Normal
		gameplay.enemyCount = 3;
		gameplay.waveMax = 3;
		gameplay.waveEnemyAddPerWave = 1;
		gameplay.enemyAttackWindup = 0.55f;
		gameplay.enemyAttackCooldown = 1.00f;
		gameplay.enemyAttackRangeMin = 0.80f;
		gameplay.enemyAttackRangeScale = 1.35f;
		gameplay.enemyAttackDamage = 1.0f;
		gameplay.enemyMoveSpeed = 1.20f;
		gameplay.waveEnemyMoveSpeedAdd = 0.15f;
		gameplay.waveEnemyAttackDamageScalePerWave = 0.20f;
		break;
	}

	// 実際に適用した難易度はデバッグ表示とタイトル UI にも反映します。
	gameplayDebug.difficultyPreset = normalizedPreset;
	gameplayDebug.titleDifficultySelection = normalizedPreset;
}

/**
 * @brief ステージクリア時の自動強化を適用します。
 */
void Transfer::ApplyStageClearUpgrade()
{
	int smallStep = 1;
	int largeStep = 2;
	GetUpgradeStepsForDifficulty(gameplayDebug.difficultyPreset, smallStep, largeStep);

	// 旧自動付与ルートでは 3 種を順番に回して、強化の偏りを抑えます。
	const int nextType = roguelike.stageClearCount % 3;
	++roguelike.stageClearCount;
	roguelike.lastUpgradeType = nextType;
	ApplyUpgradeType(roguelike, nextType, smallStep, largeStep);
}

/**
 * @brief 三択強化候補の提示を開始します。
 */
void Transfer::BeginUpgradeSelection()
{
	++roguelike.stageClearCount;
	roguelike.selectionPending = 1;
	roguelike.selectionPhase = RoguelikeUpgrade::SelectionMixed;
	roguelike.selectionRoundsRemaining = 1;
	ResetOffers(roguelike.offers);
	gameplayDebug.rewardSelectionIndex = 0;
	RefreshUpgradeSelectionState();
}

void Transfer::BeginCurrentStageRewardSelection()
{
	++roguelike.stageClearCount;

	switch (GetCurrentRunRewardType())
	{
	case RoguelikeUpgrade::RewardDice:
		FinishUpgradeSelection();
		roguelike.rerollRemain = ClampInt(roguelike.rerollRemain + kDiceRewardGain, 0, 999);
		gameplayDebug.rewardSelectionIndex = 0;
		BeginNextStageSelection();
		break;
	case RoguelikeUpgrade::RewardSkill:
		roguelike.selectionPending = 1;
		roguelike.selectionPhase = RoguelikeUpgrade::SelectionRewardSkill;
		roguelike.selectionRoundsRemaining = 1;
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
		break;
	case RoguelikeUpgrade::RewardTrait:
		roguelike.selectionPending = 1;
		roguelike.selectionPhase = RoguelikeUpgrade::SelectionRewardTrait;
		roguelike.selectionRoundsRemaining = 1;
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
		break;
	case RoguelikeUpgrade::RewardWeapon:
		roguelike.selectionPending = 1;
		roguelike.selectionPhase = RoguelikeUpgrade::SelectionRewardWeapon;
		roguelike.selectionRoundsRemaining = 1;
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
		break;
	case RoguelikeUpgrade::RewardTag:
		roguelike.selectionPending = 1;
		roguelike.selectionPhase = RoguelikeUpgrade::SelectionRewardTag;
		roguelike.selectionRoundsRemaining = 1;
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
		break;
	case RoguelikeUpgrade::RewardArtifact:
		roguelike.selectionPending = 1;
		roguelike.selectionPhase = RoguelikeUpgrade::SelectionRewardArtifact;
		roguelike.selectionRoundsRemaining = 1;
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
		break;
	case RoguelikeUpgrade::RewardUnknown:
		BeginUpgradeSelection();
		break;
	default:
		FinishUpgradeSelection();
		gameplayDebug.rewardSelectionIndex = 0;
		BeginNextStageSelection();
		break;
	}
}

void Transfer::BeginChallengeRewardSelection(int rewardCount)
{
	roguelike.selectionPending = 1;
	roguelike.selectionPhase = RoguelikeUpgrade::SelectionMixed;
	roguelike.selectionRoundsRemaining = ClampInt(rewardCount, 1, 3);
	ResetOffers(roguelike.offers);
	gameplayDebug.rewardSelectionIndex = 0;
	RefreshUpgradeSelectionState();
}

/**
 * @brief 現在の強化候補を再抽選します。
 * @return 再抽選できた場合は true です。
 */
bool Transfer::RerollUpgradeSelection()
{
	// 選択待ちでなければ、そもそも再抽選する候補がありません。
	if (roguelike.selectionPending == 0) return false;
	RefreshUpgradeSelectionState();
	if (!HasAnyOffer(roguelike.offers)) return false;
	// 残回数が無い場合も再抽選できません。
	if (roguelike.rerollRemain <= 0) return false;
	--roguelike.rerollRemain;
	ResetOffers(roguelike.offers);
	gameplayDebug.rewardSelectionIndex = 0;
	RefreshUpgradeSelectionState();
	return true;
}

bool Transfer::AdvanceUpgradeSelectionPhase()
{
	if (roguelike.selectionPending == 0) return false;

	if (roguelike.selectionRoundsRemaining > 0)
	{
		--roguelike.selectionRoundsRemaining;
	}

	if (roguelike.selectionRoundsRemaining > 0)
	{
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
		return true;
	}

	FinishUpgradeSelection();
	return true;
}

/**
 * @brief 指定した候補番号の強化を適用します。
 * @param offerIndex 選択した候補の添字です。
 * @return 適用に成功した場合は true です。
 */
bool Transfer::ApplyUpgradeSelection(int offerIndex)
{
	// 選択待ちでない状態では適用しません。
	if (roguelike.selectionPending == 0) return false;
	RefreshUpgradeSelectionState();
	if (!HasAnyOffer(roguelike.offers)) return false;
	// 候補範囲外の番号は無効です。
	if (offerIndex < 0 || offerIndex >= RoguelikeUpgrade::kOfferCount) return false;

	const int selectedType = roguelike.offers[offerIndex];
	// 候補が空か壊れている場合は適用しません。
	if (selectedType < 0) return false;
	if (!ApplyPackedOffer(roguelike, selectedType)) return false;
	roguelike.lastUpgradeType = selectedType;
	if (roguelike.selectionRoundsRemaining > 0)
	{
		--roguelike.selectionRoundsRemaining;
	}

	if (roguelike.selectionRoundsRemaining > 0)
	{
		ResetOffers(roguelike.offers);
		gameplayDebug.rewardSelectionIndex = 0;
		RefreshUpgradeSelectionState();
	}
	else
	{
		FinishUpgradeSelection();
	}
	return true;
}

bool Transfer::RefreshUpgradeSelectionState()
{
	if (roguelike.selectionPending == 0)
	{
		return false;
	}

	roguelike.selectionPhase = NormalizeSelectionPhaseValue(roguelike.selectionPhase);
	if (roguelike.selectionPhase == RoguelikeUpgrade::SelectionNone)
	{
		roguelike.selectionPhase = RoguelikeUpgrade::SelectionMixed;
	}

	if (HasAnyOffer(roguelike.offers))
	{
		return true;
	}

	GenerateOffersForSelectionPhase(roguelike.selectionPhase, roguelike.offers);
	return HasAnyOffer(roguelike.offers);
}

bool Transfer::HasAnyUpgradeOffer() const
{
	return HasAnyOffer(roguelike.offers);
}

void Transfer::FinishUpgradeSelection()
{
	roguelike.selectionPending = 0;
	roguelike.selectionPhase = RoguelikeUpgrade::SelectionNone;
	roguelike.selectionRoundsRemaining = 0;
	ResetOffers(roguelike.offers);
	gameplayDebug.rewardSelectionIndex = 0;
}

int Transfer::GetRunStageCount() const
{
	return RoguelikeUpgrade::kRunStageCount;
}

int Transfer::GetRunStageTypeAt(int stageIndex) const
{
	const int clampedStageIndex = ClampInt(stageIndex, 0, RoguelikeUpgrade::kRunStageCount - 1);
	return NormalizeStageTypeValue(roguelike.stageTypes[clampedStageIndex]);
}

int Transfer::GetCurrentRunStageType() const
{
	return NormalizeStageTypeValue(roguelike.currentStageType);
}

int Transfer::GetRunRewardTypeAt(int stageIndex) const
{
	const int clampedStageIndex = ClampInt(stageIndex, 0, RoguelikeUpgrade::kRunStageCount - 1);
	return NormalizeRewardTypeValue(roguelike.stageRewardTypes[clampedStageIndex]);
}

int Transfer::GetCurrentRunRewardType() const
{
	return GetRunRewardTypeAt(roguelike.currentStageIndex);
}

int Transfer::GetRunStageMapNumberAt(int stageIndex) const
{
	const int clampedStageIndex = ClampInt(stageIndex, 0, RoguelikeUpgrade::kRunStageCount - 1);
	return ClampInt(roguelike.stageMapNumbers[clampedStageIndex], 1, 3);
}

int Transfer::GetRunStageStepNumberAt(int stageIndex) const
{
	const int clampedStageIndex = ClampInt(stageIndex, 0, RoguelikeUpgrade::kRunStageCount - 1);
	return (roguelike.stageStepNumbers[clampedStageIndex] < 0) ? 0 : roguelike.stageStepNumbers[clampedStageIndex];
}

int Transfer::GetRunStageOptionCount(int stageIndex) const
{
	const int clampedStageIndex = ClampInt(stageIndex, 0, RoguelikeUpgrade::kRunStageCount - 1);
	return ClampInt(roguelike.stageOptionCounts[clampedStageIndex], 1, RoguelikeUpgrade::kOfferCount);
}

bool Transfer::BeginNextStageSelection()
{
	FinishUpgradeSelection();
	if (roguelike.currentStageIndex >= RoguelikeUpgrade::kRunStageCount - 1)
	{
		roguelike.intermissionMode = RoguelikeUpgrade::IntermissionNone;
		return false;
	}

	const int nextStageIndex = roguelike.currentStageIndex + 1;
	const int optionCount = GetRunStageOptionCount(nextStageIndex);
	const int nextRewardType = GetRunRewardTypeAt(nextStageIndex);
	const bool forceMapSelect =
		nextRewardType == RoguelikeUpgrade::RewardArtifact ||
		nextRewardType == RoguelikeUpgrade::RewardShop ||
		nextRewardType == RoguelikeUpgrade::RewardRest;
	if (optionCount <= 1 && !forceMapSelect)
	{
		return SelectNextStage(0);
	}

	roguelike.intermissionMode = RoguelikeUpgrade::IntermissionMapSelect;
	gameplayDebug.rewardSelectionIndex = 0;
	return true;
}

bool Transfer::SelectNextStage(int optionIndex)
{
	const int nextStageIndex = roguelike.currentStageIndex + 1;
	if (nextStageIndex < 0 || nextStageIndex >= RoguelikeUpgrade::kRunStageCount)
	{
		return false;
	}

	const int optionCount = GetRunStageOptionCount(nextStageIndex);
	const bool allowImplicitAdvance =
		optionCount <= 1 &&
		optionIndex == 0 &&
		(roguelike.intermissionMode == RoguelikeUpgrade::IntermissionNone ||
		 roguelike.intermissionMode == RoguelikeUpgrade::IntermissionShop ||
		 roguelike.intermissionMode == RoguelikeUpgrade::IntermissionRest);
	if (roguelike.intermissionMode != RoguelikeUpgrade::IntermissionMapSelect && !allowImplicitAdvance)
	{
		return false;
	}
	if (optionIndex < 0 || optionIndex >= optionCount)
	{
		return false;
	}

	roguelike.stageRewardTypes[nextStageIndex] =
		NormalizeRewardTypeValue(roguelike.stageOptions[nextStageIndex][optionIndex]);
	roguelike.currentStageIndex = nextStageIndex;
	roguelike.currentStageType = GetRunStageTypeAt(nextStageIndex);
	roguelike.shopPurchaseCount = 0;
	switch (roguelike.currentStageType)
	{
	case RoguelikeUpgrade::StageShop:
		roguelike.intermissionMode = RoguelikeUpgrade::IntermissionShop;
		break;
	case RoguelikeUpgrade::StageRest:
		roguelike.intermissionMode = RoguelikeUpgrade::IntermissionRest;
		HealPlayerByRatio(kRestStageHealRatio);
		break;
	default:
		roguelike.intermissionMode = RoguelikeUpgrade::IntermissionNone;
		break;
	}

	if (GetCurrentRunRewardType() == RoguelikeUpgrade::RewardTag)
	{
		BeginCurrentStageRewardSelection();
	}

	gameplayDebug.rewardSelectionIndex = 0;
	return true;
}

bool Transfer::ContinueFromCurrentNonCombatStage()
{
	const int currentStageType = GetCurrentRunStageType();
	if (currentStageType != RoguelikeUpgrade::StageShop &&
		currentStageType != RoguelikeUpgrade::StageRest)
	{
		return false;
	}

	roguelike.shopPurchaseCount = 0;
	if (roguelike.currentStageIndex >= RoguelikeUpgrade::kRunStageCount - 1)
	{
		roguelike.intermissionMode = RoguelikeUpgrade::IntermissionNone;
		return false;
	}

	const int nextStageIndex = roguelike.currentStageIndex + 1;
	const int optionCount = GetRunStageOptionCount(nextStageIndex);
	const int nextRewardType = GetRunRewardTypeAt(nextStageIndex);
	const bool forceMapSelect =
		nextRewardType == RoguelikeUpgrade::RewardArtifact ||
		nextRewardType == RoguelikeUpgrade::RewardShop ||
		nextRewardType == RoguelikeUpgrade::RewardRest;
	if (optionCount <= 1 && !forceMapSelect)
	{
		return SelectNextStage(0);
	}

	roguelike.intermissionMode = RoguelikeUpgrade::IntermissionMapSelect;
	gameplayDebug.rewardSelectionIndex = 0;
	return true;
}

int Transfer::GetRunStageOptionRewardType(int stageIndex, int optionIndex) const
{
	const int clampedStageIndex = ClampInt(stageIndex, 0, RoguelikeUpgrade::kRunStageCount - 1);
	const int clampedOptionIndex = ClampInt(optionIndex, 0, RoguelikeUpgrade::kOfferCount - 1);
	return NormalizeRewardTypeValue(roguelike.stageOptions[clampedStageIndex][clampedOptionIndex]);
}

int Transfer::GetCurrentShopCost() const
{
	const int purchaseCount = (roguelike.shopPurchaseCount < 0) ? 0 : roguelike.shopPurchaseCount;
	return kShopBaseCost + (purchaseCount * kShopCostPerPurchase);
}

void Transfer::HealPlayerByRatio(float ratio)
{
	if (player.maxHp <= 0.0f)
	{
		player.maxHp = kDefaultPlayerMaxHp;
	}

	const float clampedRatio = ClampFloat(ratio, 0.0f, 1.0f);
	player.hp = ClampFloat(player.hp + player.maxHp * clampedRatio, 0.0f, player.maxHp);
}

bool Transfer::TryConsumePlayerRevive()
{
	if (roguelike.reviveRemain <= 0)
	{
		return false;
	}
	if (player.maxHp <= 0.0f)
	{
		player.maxHp = kDefaultPlayerMaxHp;
	}

	--roguelike.reviveRemain;
	player.hp = player.maxHp;
	return true;
}

void Transfer::GrantStageProgressRerollItems()
{
	const int gain = ClampInt(roguelike.rerollMaxPerStage, 0, 9);
	roguelike.rerollRemain = ClampInt(roguelike.rerollRemain + gain, 0, 999);
}

bool Transfer::GenerateOffersForSelectionPhase(int selectionPhase, int offers[RoguelikeUpgrade::kOfferCount]) const
{
	if (!offers)
	{
		return false;
	}

	BuildOffersForSelectionPhase(roguelike, selectionPhase, offers);
	return HasAnyOffer(offers);
}

bool Transfer::ApplyUpgradeTypeImmediate(int upgradeType)
{
	if (upgradeType < 0 || upgradeType >= RoguelikeUpgrade::UpgradeTypeCount)
	{
		return false;
	}

	int smallStep = 1;
	int largeStep = 2;
	GetUpgradeStepsForDifficulty(gameplayDebug.difficultyPreset, smallStep, largeStep);
	ApplyUpgradeType(roguelike, upgradeType, smallStep, largeStep);
	roguelike.lastUpgradeType = upgradeType;
	return true;
}

bool Transfer::PurchaseRandomUpgrade(int selectionPhase, int cost)
{
	if (cost <= 0 || roguelike.rerollRemain < cost)
	{
		return false;
	}

	const int normalizedPhase = NormalizeSelectionPhaseValue(selectionPhase);
	if (normalizedPhase == RoguelikeUpgrade::SelectionNone)
	{
		return false;
	}

	int offers[RoguelikeUpgrade::kOfferCount]{};
	if (!GenerateOffersForSelectionPhase(normalizedPhase, offers))
	{
		return false;
	}

	roguelike.rerollRemain = ClampInt(roguelike.rerollRemain - cost, 0, 999);
	++roguelike.shopPurchaseCount;
	roguelike.selectionPending = 1;
	roguelike.selectionPhase = normalizedPhase;
	roguelike.selectionRoundsRemaining = 1;
	ResetOffers(roguelike.offers);
	gameplayDebug.rewardSelectionIndex = 0;
	RefreshUpgradeSelectionState();
	return true;
}

/**
 * @brief 難易度値を有効範囲へ丸めます。
 * @param preset 補正対象の難易度値です。
 * @return 0 から 2 に丸めた値です。
 */
int Transfer::NormalizeDifficultyPreset(int preset) const
{
	return NormalizeDifficultyPresetValue(preset);
}

/**
 * @brief 強化レベルを有効範囲へ丸めます。
 * @param level 補正対象レベルです。
 * @return 0 から上限までに丸めた値です。
 */
int Transfer::ClampUpgradeLevel(int level) const
{
	return ClampUpgradeTier(level);
}

/**
 * @brief 強化レベル上限を返します。
 * @return 強化レベル上限です。
 */
int Transfer::GetUpgradeLevelMax() const
{
	return RoguelikeUpgrade::kLevelMax;
}

int Transfer::GetSkillUpgradeLevelMax() const
{
	return RoguelikeUpgrade::kLevelMax;
}

/**
 * @brief 強化タイプと難易度から実際の増加量を返します。
 * @param upgradeType 強化種別です。
 * @param difficultyPreset 難易度です。
 * @return その強化で増えるレベル数です。
 */
int Transfer::GetUpgradeStepForType(int upgradeType, int difficultyPreset) const
{
	int smallStep = 1;
	int largeStep = 2;
	GetUpgradeStepsForDifficulty(difficultyPreset, smallStep, largeStep);

	// Small / Large の別に応じて、難易度別の増加量を返します。
	switch (upgradeType)
	{
	case RoguelikeUpgrade::UpgradeAttackPower:
	case RoguelikeUpgrade::UpgradeAttackSpeed:
	case RoguelikeUpgrade::UpgradeEvadeCooldown:
		return smallStep;
	case RoguelikeUpgrade::UpgradeAttackPowerLarge:
	case RoguelikeUpgrade::UpgradeAttackSpeedLarge:
	case RoguelikeUpgrade::UpgradeEvadeCooldownLarge:
		return largeStep;
	default:
		return 0;
	}
}

float Transfer::GetSkillRangeScaleByLevel(int level) const
{
	const float t = static_cast<float>(ClampInt(level, 0, GetSkillUpgradeLevelMax())) /
		static_cast<float>(GetSkillUpgradeLevelMax());
	return 1.0f + 2.0f * t;
}

float Transfer::GetSkillDamageScaleByLevel(int level) const
{
	const float t = static_cast<float>(ClampInt(level, 0, GetSkillUpgradeLevelMax())) /
		static_cast<float>(GetSkillUpgradeLevelMax());
	return 1.0f + t;
}

float Transfer::GetOrbitDamageScaleByCountLevel(int level) const
{
	const int clampedLevel = ClampInt(level, 0, GetSkillUpgradeLevelMax());
	if (clampedLevel <= 5)
	{
		return 1.0f;
	}
	const float overflowT = static_cast<float>(clampedLevel - 5) / 5.0f;
	return 1.0f + 0.5f * overflowT;
}

float Transfer::GetSkillCooldownReductionByLevel(int level) const
{
	const float t = static_cast<float>(ClampInt(level, 0, GetSkillUpgradeLevelMax())) /
		static_cast<float>(GetSkillUpgradeLevelMax());
	return 4.0f * t;
}

int Transfer::GetOrbitCountByLevel(int level) const
{
	return 1 + ClampInt(level, 0, 5);
}

/**
 * @brief 3 系統の強化レベル合計を返します。
 * @return 合計強化レベルです。
 */
int Transfer::GetTotalUpgradeLevels() const
{
	return
		ClampUpgradeTier(roguelike.attackPowerLevel) +
		ClampUpgradeTier(roguelike.attackSpeedLevel) +
		ClampUpgradeTier(roguelike.evadeCooldownLevel);
}

/**
 * @brief 攻撃力レベルから実ダメージ値を返します。
 * @param level 攻撃力レベルです。
 * @return 実ダメージ値です。
 */
int Transfer::GetPlayerAttackDamageByLevel(int level) const
{
	return kAttackDamageByTier[ClampUpgradeTier(level)];
}

/**
 * @brief 攻撃速度レベルから攻撃クールタイム倍率を返します。
 * @param level 攻撃速度レベルです。
 * @return クールタイム倍率です。
 */
float Transfer::GetAttackCooldownScaleByLevel(int level) const
{
	return kAttackCooldownScaleByTier[ClampUpgradeTier(level)];
}

/**
 * @brief 回避強化レベルから回避クールタイム倍率を返します。
 * @param level 回避強化レベルです。
 * @return クールタイム倍率です。
 */
float Transfer::GetEvadeCooldownScaleByLevel(int level) const
{
	return kEvadeCooldownScaleByTier[ClampUpgradeTier(level)];
}

/**
 * @brief 強化進行度に応じた通常敵 HP 倍率を返します。
 * @return 敵 HP 倍率です。
 */
float Transfer::GetEnemyHpScaleByUpgradeProgress() const
{
	const int progressTier = GetTotalUpgradeProgressValue(roguelike) / 10;
	return 1.0f + 0.20f * static_cast<float>((progressTier < 0) ? 0 : progressTier);
}

/**
 * @brief 強化進行度に応じたボス HP 倍率を返します。
 * @return ボス HP 倍率です。
 */
float Transfer::GetBossHpScaleByUpgradeProgress() const
{
	return EvaluateUpgradeProgressScale(
		GetTotalUpgradeProgressValue(roguelike),
		10.0f,
		20.0f,
		40.0f);
}

/**
 * @brief 強化進行度に応じた通常敵攻撃倍率を返します。
 * @return 敵攻撃倍率です。
 */
float Transfer::GetEnemyAttackScaleByUpgradeProgress() const
{
	const int progressTier = GetTotalUpgradeProgressValue(roguelike) / 10;
	if (progressTier < 0)
	{
		return 1.0f;
	}
	return 1.0f + 0.10f * static_cast<float>(progressTier);
}

/**
 * @brief 難易度に応じた通常敵 HP 倍率を返します。
 * @param preset 難易度です。
 * @return 通常敵 HP 倍率です。
 */
float Transfer::GetEnemyHpScaleByDifficulty(int preset) const
{
	switch (NormalizeDifficultyPresetValue(preset))
	{
	case 0: return 0.85f;
	case 2: return 1.25f;
	default: return 1.0f;
	}
}

/**
 * @brief 難易度に応じたボス HP 倍率を返します。
 * @param preset 難易度です。
 * @return ボス HP 倍率です。
 */
float Transfer::GetBossHpScaleByDifficulty(int preset) const
{
	switch (NormalizeDifficultyPresetValue(preset))
	{
	case 0: return 0.85f;
	case 2: return 1.25f;
	default: return 1.0f;
	}
}

/**
 * @brief 難易度に応じたボス行動頻度倍率を返します。
 * @param preset 難易度です。
 * @return ボスのクールタイム倍率です。
 */
float Transfer::GetBossCooldownScaleByDifficulty(int preset) const
{
	switch (NormalizeDifficultyPresetValue(preset))
	{
	case 0: return 1.15f;
	case 2: return 0.85f;
	default: return 1.0f;
	}
}


/**
 * @brief 既定のゲームプレイ設定ファイルパスを返します。
 * @return 既定設定ファイルパスです。
 */
const char* Transfer::GetGameplayTuningPath() const
{
	return kDefaultGameplayTuningPath;
}

/**
 * @brief 設定ファイルからゲームプレイ調整値を読み込みます。
 * @param path 読み込むファイルパスです。nullptr の場合は既定パスです。
 * @return 読み込みに成功した場合は true です。
 */
bool Transfer::LoadGameplayTuning(const char* path)
{
	const char* resolvedPath = ResolvePath(path);
	std::ifstream ifs(resolvedPath);

	// 既定パスで開けない場合は、実行場所ごとの代表パスも順に試します。
	if (!ifs.is_open() && IsDefaultPathArgument(path))
	{
		const char* fallbackPaths[] =
		{
			kDebugGameplayTuningPath,
			kMirrorGameplayTuningPath,
			kUpstreamMirrorGameplayTuningPath
		};
		for (const char* fallback : fallbackPaths)
		{
			ifs.clear();
			ifs.open(fallback);
			if (ifs.is_open())
			{
				resolvedPath = fallback;
				break;
			}
		}
	}

	if (!ifs.is_open())
	{
		return false;
	}

	// 読み込み途中で失敗しても既存値を壊さないよう、一旦ローカルへ集めます。
	GameplayTuning loaded{};
	int loadedPreset = 1;
	RoguelikeUpgrade loadedRogue{};
	InputConfig loadedInput{};
	ApplyGameplayTuningFileDefaults(loaded, loadedPreset, loadedRogue);
	std::unordered_set<std::string> loadedKeys;
	loadedKeys.reserve(kGameplayTuningRequiredKeyCount);

	std::string line;
	while (std::getline(ifs, line))
	{
		// 空行とコメント行は設定値ではないので無視します。
		Trim(line);
		if (line.empty()) continue;
		if (line[0] == '#') continue;

		const size_t sep = line.find('=');
		if (sep == std::string::npos) continue;

		std::string key = line.substr(0, sep);
		std::string value = line.substr(sep + 1);
		Trim(key);
		Trim(value);
		if (key.empty() || value.empty()) continue;

		if (key == "bindMoveUp") { loadedInput.moveUp = ToInt(value, loadedInput.moveUp); continue; }
		if (key == "bindMoveDown") { loadedInput.moveDown = ToInt(value, loadedInput.moveDown); continue; }
		if (key == "bindMoveLeft") { loadedInput.moveLeft = ToInt(value, loadedInput.moveLeft); continue; }
		if (key == "bindMoveRight") { loadedInput.moveRight = ToInt(value, loadedInput.moveRight); continue; }
		if (key == "bindDash") { loadedInput.dash = ToInt(value, loadedInput.dash); continue; }
		if (key == "bindAttack") { loadedInput.attack = ToInt(value, loadedInput.attack); continue; }
		if (key == "bindSkill1") { loadedInput.skill1 = ToInt(value, loadedInput.skill1); continue; }
		if (key == "bindSkill2") { loadedInput.skill2 = ToInt(value, loadedInput.skill2); continue; }
		if (key == "bindReroll") { loadedInput.reroll = ToInt(value, loadedInput.reroll); continue; }
		if (key == "upgradeStepEasySmall") { loaded.upgradeStepEasySmall = ToInt(value, loaded.upgradeStepEasySmall); continue; }
		if (key == "upgradeStepEasyLarge") { loaded.upgradeStepEasyLarge = ToInt(value, loaded.upgradeStepEasyLarge); continue; }
		if (key == "upgradeStepNormalSmall") { loaded.upgradeStepNormalSmall = ToInt(value, loaded.upgradeStepNormalSmall); continue; }
		if (key == "upgradeStepNormalLarge") { loaded.upgradeStepNormalLarge = ToInt(value, loaded.upgradeStepNormalLarge); continue; }
		if (key == "upgradeStepHardSmall") { loaded.upgradeStepHardSmall = ToInt(value, loaded.upgradeStepHardSmall); continue; }
		if (key == "upgradeStepHardLarge") { loaded.upgradeStepHardLarge = ToInt(value, loaded.upgradeStepHardLarge); continue; }
		if (key == "bindXInputConfirm") { loadedInput.xinputConfirm = ToInt(value, loadedInput.xinputConfirm); continue; }
		if (key == "bindXInputDash") { loadedInput.xinputDash = ToInt(value, loadedInput.xinputDash); continue; }
		if (key == "bindXInputAttack") { loadedInput.xinputAttack = ToInt(value, loadedInput.xinputAttack); continue; }
		if (key == "bindXInputSkill1") { loadedInput.xinputSkill1 = ToInt(value, loadedInput.xinputSkill1); continue; }
		if (key == "bindXInputSkill2") { loadedInput.xinputSkill2 = ToInt(value, loadedInput.xinputSkill2); continue; }
		if (key == "bindXInputReroll") { loadedInput.xinputReroll = ToInt(value, loadedInput.xinputReroll); continue; }
		if (key == "bindXInputTabPrev") { loadedInput.xinputTabPrev = ToInt(value, loadedInput.xinputTabPrev); continue; }
		if (key == "bindXInputTabNext") { loadedInput.xinputTabNext = ToInt(value, loadedInput.xinputTabNext); continue; }
		if (key == "bindDirectInputConfirm") { loadedInput.directInputConfirm = ToInt(value, loadedInput.directInputConfirm); continue; }
		if (key == "bindDirectInputDash") { loadedInput.directInputDash = ToInt(value, loadedInput.directInputDash); continue; }
		if (key == "bindDirectInputAttack") { loadedInput.directInputAttack = ToInt(value, loadedInput.directInputAttack); continue; }
		if (key == "bindDirectInputSkill1") { loadedInput.directInputSkill1 = ToInt(value, loadedInput.directInputSkill1); continue; }
		if (key == "bindDirectInputSkill2") { loadedInput.directInputSkill2 = ToInt(value, loadedInput.directInputSkill2); continue; }
		if (key == "bindDirectInputReroll") { loadedInput.directInputReroll = ToInt(value, loadedInput.directInputReroll); continue; }
		if (key == "bindDirectInputTabPrev") { loadedInput.directInputTabPrev = ToInt(value, loadedInput.directInputTabPrev); continue; }
		if (key == "bindDirectInputTabNext") { loadedInput.directInputTabNext = ToInt(value, loadedInput.directInputTabNext); continue; }
		if (key == "challengeNoDamageDuration") { loaded.challengeNoDamageDuration = ToFloat(value, loaded.challengeNoDamageDuration); loadedKeys.insert(key); continue; }
		if (key == "challengeNoDamageAttackInterval") { loaded.challengeNoDamageAttackInterval = ToFloat(value, loaded.challengeNoDamageAttackInterval); loadedKeys.insert(key); continue; }
		if (key == "challengeNoDamageTelegraph") { loaded.challengeNoDamageTelegraph = ToFloat(value, loaded.challengeNoDamageTelegraph); loadedKeys.insert(key); continue; }
		if (key == "challengeNoDamageRadius") { loaded.challengeNoDamageRadius = ToFloat(value, loaded.challengeNoDamageRadius); loadedKeys.insert(key); continue; }
		if (key == "challengeNoDamageDamage") { loaded.challengeNoDamageDamage = ToFloat(value, loaded.challengeNoDamageDamage); loadedKeys.insert(key); continue; }
		if (key == "challengeNoDamageBurstCount") { loaded.challengeNoDamageBurstCount = ToInt(value, loaded.challengeNoDamageBurstCount); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseDuration") { loaded.challengeDefenseDuration = ToFloat(value, loaded.challengeDefenseDuration); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseBeaconMaxHp") { loaded.challengeDefenseBeaconMaxHp = ToFloat(value, loaded.challengeDefenseBeaconMaxHp); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseBeaconRadius") { loaded.challengeDefenseBeaconRadius = ToFloat(value, loaded.challengeDefenseBeaconRadius); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseBeaconContactDamage") { loaded.challengeDefenseBeaconContactDamage = ToFloat(value, loaded.challengeDefenseBeaconContactDamage); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseSpawnInterval") { loaded.challengeDefenseSpawnInterval = ToFloat(value, loaded.challengeDefenseSpawnInterval); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseEnemyMoveSpeed") { loaded.challengeDefenseEnemyMoveSpeed = ToFloat(value, loaded.challengeDefenseEnemyMoveSpeed); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseSpeedHpBonusRate") { loaded.challengeDefenseSpeedHpBonusRate = ToFloat(value, loaded.challengeDefenseSpeedHpBonusRate); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseRangedHpBonusRate") { loaded.challengeDefenseRangedHpBonusRate = ToFloat(value, loaded.challengeDefenseRangedHpBonusRate); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseTankHpBonusRate") { loaded.challengeDefenseTankHpBonusRate = ToFloat(value, loaded.challengeDefenseTankHpBonusRate); loadedKeys.insert(key); continue; }
		if (key == "challengeDefenseEnemyCap") { loaded.challengeDefenseEnemyCap = ToInt(value, loaded.challengeDefenseEnemyCap); loadedKeys.insert(key); continue; }

		// キー名に応じて、対象項目だけを個別に復元します。
		bool matchedKey = true;
		if (key == "enemyCount") loaded.enemyCount = ToInt(value, loaded.enemyCount);
		else if (key == "waveMax") loaded.waveMax = ToInt(value, loaded.waveMax);
		else if (key == "waveEnemyAddPerWave") loaded.waveEnemyAddPerWave = ToInt(value, loaded.waveEnemyAddPerWave);
		else if (key == "groundTileSize") loaded.groundTileSize = ToFloat(value, loaded.groundTileSize);
		else if (key == "cameraIntroDuration") loaded.cameraIntroDuration = ToFloat(value, loaded.cameraIntroDuration);
		else if (key == "cameraIntroFocusDistance") loaded.cameraIntroFocusDistance = ToFloat(value, loaded.cameraIntroFocusDistance);
		else if (key == "attackWindup") loaded.attackWindup = ToFloat(value, loaded.attackWindup);
		else if (key == "attackDuration") loaded.attackDuration = ToFloat(value, loaded.attackDuration);
		else if (key == "attackRecovery") loaded.attackRecovery = ToFloat(value, loaded.attackRecovery);
		else if (key == "attackCooldown") loaded.attackCooldown = ToFloat(value, loaded.attackCooldown);
		else if (key == "skill1Cooldown") loaded.skill1Cooldown = ToFloat(value, loaded.skill1Cooldown);
		else if (key == "skill2Cooldown") loaded.skill2Cooldown = ToFloat(value, loaded.skill2Cooldown);
		else if (key == "screenShakeHitThreshold") loaded.screenShakeHitThreshold = ToInt(value, loaded.screenShakeHitThreshold);
		else if (key == "screenShakeDuration") loaded.screenShakeDuration = ToFloat(value, loaded.screenShakeDuration);
		else if (key == "screenShakeAmplitude") loaded.screenShakeAmplitude = ToFloat(value, loaded.screenShakeAmplitude);
		else if (key == "attackSweepDegrees") loaded.attackSweepDegrees = ToFloat(value, loaded.attackSweepDegrees);
		else if (key == "attackSweepRadiusScale") loaded.attackSweepRadiusScale = ToFloat(value, loaded.attackSweepRadiusScale);
		else if (key == "attackWidthScale") loaded.attackWidthScale = ToFloat(value, loaded.attackWidthScale);
		else if (key == "attackDepthScale") loaded.attackDepthScale = ToFloat(value, loaded.attackDepthScale);
		else if (key == "attackHitStop") loaded.attackHitStop = ToFloat(value, loaded.attackHitStop);
		else if (key == "attackKnockback") loaded.attackKnockback = ToFloat(value, loaded.attackKnockback);
		else if (key == "attackHitFlash") loaded.attackHitFlash = ToFloat(value, loaded.attackHitFlash);
		else if (key == "attackTrailInterval") loaded.attackTrailInterval = ToFloat(value, loaded.attackTrailInterval);
		else if (key == "attackTrailLife") loaded.attackTrailLife = ToFloat(value, loaded.attackTrailLife);
		else if (key == "attackTrailScale") loaded.attackTrailScale = ToFloat(value, loaded.attackTrailScale);
		else if (key == "playerDamageFlash") loaded.playerDamageFlash = ToFloat(value, loaded.playerDamageFlash);
		else if (key == "playerDamageInvincible") loaded.playerDamageInvincible = ToFloat(value, loaded.playerDamageInvincible);
		else if (key == "playerDamageFlashScale") loaded.playerDamageFlashScale = ToFloat(value, loaded.playerDamageFlashScale);
		else if (key == "enemyDefeatFlash") loaded.enemyDefeatFlash = ToFloat(value, loaded.enemyDefeatFlash);
		else if (key == "enemyDefeatFlashScale") loaded.enemyDefeatFlashScale = ToFloat(value, loaded.enemyDefeatFlashScale);
		else if (key == "volumeMaster") loaded.volumeMaster = ToFloat(value, loaded.volumeMaster);
		else if (key == "volumeBgm") loaded.volumeBgm = ToFloat(value, loaded.volumeBgm);
		else if (key == "volumeSe") loaded.volumeSe = ToFloat(value, loaded.volumeSe);
		else if (key == "directionMarkerAlpha") loaded.directionMarkerAlpha = ToFloat(value, loaded.directionMarkerAlpha);
		else if (key == "directionMarkerOverlapAlpha") loaded.directionMarkerOverlapAlpha = ToFloat(value, loaded.directionMarkerOverlapAlpha);
		else if (key == "bossHpBarWidthRate") loaded.bossHpBarWidthRate = ToFloat(value, loaded.bossHpBarWidthRate);
		else if (key == "bossHpBarHeightRate") loaded.bossHpBarHeightRate = ToFloat(value, loaded.bossHpBarHeightRate);
		else if (key == "bossGuardBarOffsetX") loaded.bossGuardBarOffsetX = ToFloat(value, loaded.bossGuardBarOffsetX);
		else if (key == "bossGuardBarOffsetY") loaded.bossGuardBarOffsetY = ToFloat(value, loaded.bossGuardBarOffsetY);
		else if (key == "bossGuardBarWidthRate") loaded.bossGuardBarWidthRate = ToFloat(value, loaded.bossGuardBarWidthRate);
		else if (key == "bossGuardBarHeightRate") loaded.bossGuardBarHeightRate = ToFloat(value, loaded.bossGuardBarHeightRate);
		else if (key == "bossSizeAreaScale") loaded.bossSizeAreaScale = ToFloat(value, loaded.bossSizeAreaScale);
		else if (key == "bossMaxHp") loaded.bossMaxHp = ToInt(value, loaded.bossMaxHp);
		else if (key == "bossAttackTelegraph") loaded.bossAttackTelegraph = ToFloat(value, loaded.bossAttackTelegraph);
		else if (key == "bossAttackJumpOutTime") loaded.bossAttackJumpOutTime = ToFloat(value, loaded.bossAttackJumpOutTime);
		else if (key == "bossAttackDashDuration") loaded.bossAttackDashDuration = ToFloat(value, loaded.bossAttackDashDuration);
		else if (key == "bossAttackCooldown") loaded.bossAttackCooldown = ToFloat(value, loaded.bossAttackCooldown);
		else if (key == "bossAttackLanePlayerScale") loaded.bossAttackLanePlayerScale = ToFloat(value, loaded.bossAttackLanePlayerScale);
		else if (key == "bossAttackDamage") loaded.bossAttackDamage = ToFloat(value, loaded.bossAttackDamage);
		else if (key == "bossAttackHitStop") loaded.bossAttackHitStop = ToFloat(value, loaded.bossAttackHitStop);
		else if (key == "bossHitShakeDuration") loaded.bossHitShakeDuration = ToFloat(value, loaded.bossHitShakeDuration);
		else if (key == "bossHitShakeAmplitude") loaded.bossHitShakeAmplitude = ToFloat(value, loaded.bossHitShakeAmplitude);
		else if (key == "bossGuardInitialMax") loaded.bossGuardInitialMax = ToFloat(value, loaded.bossGuardInitialMax);
		else if (key == "bossGuardFinalMax") loaded.bossGuardFinalMax = ToFloat(value, loaded.bossGuardFinalMax);
		else if (key == "bossGuardRecoverStep") loaded.bossGuardRecoverStep = ToFloat(value, loaded.bossGuardRecoverStep);
		else if (key == "bossDamageScaleNormal") loaded.bossDamageScaleNormal = ToFloat(value, loaded.bossDamageScaleNormal);
		else if (key == "bossDamageScaleBroken") loaded.bossDamageScaleBroken = ToFloat(value, loaded.bossDamageScaleBroken);
		else if (key == "bossBreakRecoverSec") loaded.bossBreakRecoverSec = ToFloat(value, loaded.bossBreakRecoverSec);
		else if (key == "bossDashNarrowTelegraph") loaded.bossDashNarrowTelegraph = ToFloat(value, loaded.bossDashNarrowTelegraph);
		else if (key == "bossDashWideTelegraph") loaded.bossDashWideTelegraph = ToFloat(value, loaded.bossDashWideTelegraph);
		else if (key == "bossDashWideWidthRate") loaded.bossDashWideWidthRate = ToFloat(value, loaded.bossDashWideWidthRate);
		else if (key == "bossRandomRainCount") loaded.bossRandomRainCount = ToInt(value, loaded.bossRandomRainCount);
		else if (key == "bossRandomRainTelegraph") loaded.bossRandomRainTelegraph = ToFloat(value, loaded.bossRandomRainTelegraph);
		else if (key == "bossRandomRainRadiusScale") loaded.bossRandomRainRadiusScale = ToFloat(value, loaded.bossRandomRainRadiusScale);
		else if (key == "bossSummonMin") loaded.bossSummonMin = ToInt(value, loaded.bossSummonMin);
		else if (key == "bossSummonMax") loaded.bossSummonMax = ToInt(value, loaded.bossSummonMax);
		else if (key == "bossSummonTelegraph") loaded.bossSummonTelegraph = ToFloat(value, loaded.bossSummonTelegraph);
		else if (key == "bossTrackingDropCount") loaded.bossTrackingDropCount = ToInt(value, loaded.bossTrackingDropCount);
		else if (key == "bossTrackingDropTelegraph") loaded.bossTrackingDropTelegraph = ToFloat(value, loaded.bossTrackingDropTelegraph);
		else if (key == "bossTrackingDropRadiusScale") loaded.bossTrackingDropRadiusScale = ToFloat(value, loaded.bossTrackingDropRadiusScale);
		else if (key == "bossUltimateCrossTelegraph") loaded.bossUltimateCrossTelegraph = ToFloat(value, loaded.bossUltimateCrossTelegraph);
		else if (key == "bossUltimateCrossLaneScale") loaded.bossUltimateCrossLaneScale = ToFloat(value, loaded.bossUltimateCrossLaneScale);
		else if (key == "bossUltimateStompCount") loaded.bossUltimateStompCount = ToInt(value, loaded.bossUltimateStompCount);
		else if (key == "bossUltimateStompTelegraph") loaded.bossUltimateStompTelegraph = ToFloat(value, loaded.bossUltimateStompTelegraph);
		else if (key == "bossUltimateStompRepeatTelegraph") loaded.bossUltimateStompRepeatTelegraph = ToFloat(value, loaded.bossUltimateStompRepeatTelegraph);
		else if (key == "bossUltimateStompRadiusScale") loaded.bossUltimateStompRadiusScale = ToFloat(value, loaded.bossUltimateStompRadiusScale);
		else if (key == "bossUltimateFieldTelegraph") loaded.bossUltimateFieldTelegraph = ToFloat(value, loaded.bossUltimateFieldTelegraph);
		else if (key == "bossUltimateFieldSafeScale") loaded.bossUltimateFieldSafeScale = ToFloat(value, loaded.bossUltimateFieldSafeScale);
		else if (key == "enemyAttackWindup") loaded.enemyAttackWindup = ToFloat(value, loaded.enemyAttackWindup);
		else if (key == "enemyAttackCooldown") loaded.enemyAttackCooldown = ToFloat(value, loaded.enemyAttackCooldown);
		else if (key == "enemyAttackRangeMin") loaded.enemyAttackRangeMin = ToFloat(value, loaded.enemyAttackRangeMin);
		else if (key == "enemyAttackRangeScale") loaded.enemyAttackRangeScale = ToFloat(value, loaded.enemyAttackRangeScale);
		else if (key == "enemyAttackDamage") loaded.enemyAttackDamage = ToFloat(value, loaded.enemyAttackDamage);
		else if (key == "enemyMoveSpeed") loaded.enemyMoveSpeed = ToFloat(value, loaded.enemyMoveSpeed);
		else if (key == "waveEnemyMoveSpeedAdd") loaded.waveEnemyMoveSpeedAdd = ToFloat(value, loaded.waveEnemyMoveSpeedAdd);
		else if (key == "waveEnemyAttackDamageScalePerWave") loaded.waveEnemyAttackDamageScalePerWave = ToFloat(value, loaded.waveEnemyAttackDamageScalePerWave);
		else if (key == "enemyProjectileSpeed") loaded.enemyProjectileSpeed = ToFloat(value, loaded.enemyProjectileSpeed);
		else if (key == "enemyProjectileLife") loaded.enemyProjectileLife = ToFloat(value, loaded.enemyProjectileLife);
		else if (key == "enemyProjectileRadius") loaded.enemyProjectileRadius = ToFloat(value, loaded.enemyProjectileRadius);
		else if (key == "enemyProjectileDamageScale") loaded.enemyProjectileDamageScale = ToFloat(value, loaded.enemyProjectileDamageScale);
		else if (key == "enemySeparationRadius") loaded.enemySeparationRadius = ToFloat(value, loaded.enemySeparationRadius);
		else if (key == "enemySeparationWeight") loaded.enemySeparationWeight = ToFloat(value, loaded.enemySeparationWeight);
		else if (key == "enemySeparationMaxOffset") loaded.enemySeparationMaxOffset = ToFloat(value, loaded.enemySeparationMaxOffset);
		else if (key == "enemySpawnRingScale") loaded.enemySpawnRingScale = ToFloat(value, loaded.enemySpawnRingScale);
		else if (key == "enemySpawnJitterScale") loaded.enemySpawnJitterScale = ToFloat(value, loaded.enemySpawnJitterScale);
		else if (key == "enemySpawnMinPlayerDist") loaded.enemySpawnMinPlayerDist = ToFloat(value, loaded.enemySpawnMinPlayerDist);
		else if (key == "enemySpawnMinEnemyDist") loaded.enemySpawnMinEnemyDist = ToFloat(value, loaded.enemySpawnMinEnemyDist);
		else if (key == "pushSlop") loaded.pushSlop = ToFloat(value, loaded.pushSlop);
		else if (key == "playerPushShare") loaded.playerPushShare = ToFloat(value, loaded.playerPushShare);
		else if (key == "enemyPushShare") loaded.enemyPushShare = ToFloat(value, loaded.enemyPushShare);
		else if (key == "difficultyPreset") loadedPreset = ToInt(value, loadedPreset);
		else if (key == "stageClearCount") loadedRogue.stageClearCount = ToInt(value, loadedRogue.stageClearCount);
		else if (key == "attackPowerLevel") loadedRogue.attackPowerLevel = ToInt(value, loadedRogue.attackPowerLevel);
		else if (key == "attackSpeedLevel") loadedRogue.attackSpeedLevel = ToInt(value, loadedRogue.attackSpeedLevel);
		else if (key == "evadeCooldownLevel") loadedRogue.evadeCooldownLevel = ToInt(value, loadedRogue.evadeCooldownLevel);
		else if (key == "lastUpgradeType") loadedRogue.lastUpgradeType = ToInt(value, loadedRogue.lastUpgradeType);
		else if (key == "skillSlot1") loadedRogue.skillSlot1 = ToInt(value, loadedRogue.skillSlot1);
		else if (key == "skillSlot2") loadedRogue.skillSlot2 = ToInt(value, loadedRogue.skillSlot2);
		else if (key == "skillShotRangeLevel") loadedRogue.skillShotRangeLevel = ToInt(value, loadedRogue.skillShotRangeLevel);
		else if (key == "skillShotPowerLevel") loadedRogue.skillShotPowerLevel = ToInt(value, loadedRogue.skillShotPowerLevel);
		else if (key == "skillShotCooldownLevel") loadedRogue.skillShotCooldownLevel = ToInt(value, loadedRogue.skillShotCooldownLevel);
		else if (key == "skillNovaRangeLevel") loadedRogue.skillNovaRangeLevel = ToInt(value, loadedRogue.skillNovaRangeLevel);
		else if (key == "skillNovaPowerLevel") loadedRogue.skillNovaPowerLevel = ToInt(value, loadedRogue.skillNovaPowerLevel);
		else if (key == "skillNovaCooldownLevel") loadedRogue.skillNovaCooldownLevel = ToInt(value, loadedRogue.skillNovaCooldownLevel);
		else if (key == "skillOrbitRangeLevel") loadedRogue.skillOrbitRangeLevel = ToInt(value, loadedRogue.skillOrbitRangeLevel);
		else if (key == "skillOrbitCooldownLevel") loadedRogue.skillOrbitCooldownLevel = ToInt(value, loadedRogue.skillOrbitCooldownLevel);
		else if (key == "skillOrbitCountLevel") loadedRogue.skillOrbitCountLevel = ToInt(value, loadedRogue.skillOrbitCountLevel);
		else if (key == "upgradeRerollMax") loadedRogue.rerollMaxPerStage = ToInt(value, loadedRogue.rerollMaxPerStage);
		else matchedKey = false;

		if (matchedKey)
		{
			loadedKeys.insert(key);
		}
	}

	if (static_cast<int>(loadedKeys.size()) != kGameplayTuningRequiredKeyCount)
	{
		// 1項目でも欠けている場合は、部分適用せずファイル既定値へ全体を戻します。
		ApplyGameplayTuningFileDefaults(loaded, loadedPreset, loadedRogue);
		loadedInput = InputConfig{};
	}

	// 読み込んだ値はここで全体的にクランプし、壊れた設定を防ぎます。
	loaded.screenShakeHitThreshold = ClampInt(loaded.screenShakeHitThreshold, 1, 16);
	if (loaded.groundTileSize < 0.5f) loaded.groundTileSize = 0.5f;
	if (loaded.groundTileSize > 10.0f) loaded.groundTileSize = 10.0f;
	if (loaded.cameraIntroDuration < 0.10f) loaded.cameraIntroDuration = 0.10f;
	if (loaded.cameraIntroFocusDistance < 0.50f) loaded.cameraIntroFocusDistance = 0.50f;
	if (loaded.playerDamageInvincible < 0.0f) loaded.playerDamageInvincible = 0.0f;
	if (loaded.screenShakeDuration < 0.0f) loaded.screenShakeDuration = 0.0f;
	if (loaded.screenShakeAmplitude < 0.0f) loaded.screenShakeAmplitude = 0.0f;
	if (loaded.directionMarkerAlpha < 0.0f) loaded.directionMarkerAlpha = 0.0f;
	if (loaded.directionMarkerAlpha > 1.0f) loaded.directionMarkerAlpha = 1.0f;
	if (loaded.directionMarkerOverlapAlpha < 0.0f) loaded.directionMarkerOverlapAlpha = 0.0f;
	if (loaded.directionMarkerOverlapAlpha > 1.0f) loaded.directionMarkerOverlapAlpha = 1.0f;
	if (loaded.bossHpBarWidthRate < 0.20f) loaded.bossHpBarWidthRate = 0.20f;
	if (loaded.bossHpBarWidthRate > 0.90f) loaded.bossHpBarWidthRate = 0.90f;
	if (loaded.bossHpBarHeightRate < 0.01f) loaded.bossHpBarHeightRate = 0.01f;
	if (loaded.bossHpBarHeightRate > 0.20f) loaded.bossHpBarHeightRate = 0.20f;
	if (loaded.bossGuardBarOffsetX < -960.0f) loaded.bossGuardBarOffsetX = -960.0f;
	if (loaded.bossGuardBarOffsetX > 960.0f) loaded.bossGuardBarOffsetX = 960.0f;
	if (loaded.bossGuardBarOffsetY < -120.0f) loaded.bossGuardBarOffsetY = -120.0f;
	if (loaded.bossGuardBarOffsetY > 320.0f) loaded.bossGuardBarOffsetY = 320.0f;
	if (loaded.bossGuardBarWidthRate < 0.10f) loaded.bossGuardBarWidthRate = 0.10f;
	if (loaded.bossGuardBarWidthRate > 0.90f) loaded.bossGuardBarWidthRate = 0.90f;
	if (loaded.bossGuardBarHeightRate < 0.005f) loaded.bossGuardBarHeightRate = 0.005f;
	if (loaded.bossGuardBarHeightRate > 0.10f) loaded.bossGuardBarHeightRate = 0.10f;
	if (loaded.bossSizeAreaScale < 4.0f) loaded.bossSizeAreaScale = 4.0f;
	if (loaded.bossSizeAreaScale > 12.0f) loaded.bossSizeAreaScale = 12.0f;
	loaded.bossMaxHp = ClampInt(loaded.bossMaxHp, 1, 9999);
	if (loaded.bossHitShakeDuration < 0.0f) loaded.bossHitShakeDuration = 0.0f;
	if (loaded.bossHitShakeDuration > 1.0f) loaded.bossHitShakeDuration = 1.0f;
	if (loaded.bossHitShakeAmplitude < 0.0f) loaded.bossHitShakeAmplitude = 0.0f;
	if (loaded.bossHitShakeAmplitude > 1.0f) loaded.bossHitShakeAmplitude = 1.0f;
	if (loaded.bossAttackTelegraph < 0.10f) loaded.bossAttackTelegraph = 0.10f;
	if (loaded.bossAttackJumpOutTime < 0.0f) loaded.bossAttackJumpOutTime = 0.0f;
	if (loaded.bossAttackDashDuration < 0.05f) loaded.bossAttackDashDuration = 0.05f;
	if (loaded.bossAttackCooldown < 0.0f) loaded.bossAttackCooldown = 0.0f;
	if (loaded.bossAttackLanePlayerScale < 0.5f) loaded.bossAttackLanePlayerScale = 0.5f;
	if (loaded.bossAttackLanePlayerScale > 8.0f) loaded.bossAttackLanePlayerScale = 8.0f;
	if (loaded.bossAttackDamage < 0.0f) loaded.bossAttackDamage = 0.0f;
	if (loaded.bossGuardInitialMax < 1.0f) loaded.bossGuardInitialMax = 1.0f;
	if (loaded.bossGuardInitialMax > 200.0f) loaded.bossGuardInitialMax = 200.0f;
	if (loaded.bossGuardFinalMax < 1.0f) loaded.bossGuardFinalMax = 1.0f;
	if (loaded.bossGuardFinalMax > 200.0f) loaded.bossGuardFinalMax = 200.0f;
	if (loaded.bossGuardFinalMax < loaded.bossGuardInitialMax) loaded.bossGuardFinalMax = loaded.bossGuardInitialMax;
	if (loaded.bossGuardRecoverStep < 0.0f) loaded.bossGuardRecoverStep = 0.0f;
	if (loaded.bossGuardRecoverStep > 50.0f) loaded.bossGuardRecoverStep = 50.0f;
	if (loaded.bossDamageScaleNormal < 0.0f) loaded.bossDamageScaleNormal = 0.0f;
	if (loaded.bossDamageScaleNormal > 5.0f) loaded.bossDamageScaleNormal = 5.0f;
	if (loaded.bossDamageScaleBroken < 0.0f) loaded.bossDamageScaleBroken = 0.0f;
	if (loaded.bossDamageScaleBroken > 10.0f) loaded.bossDamageScaleBroken = 10.0f;
	if (loaded.bossBreakRecoverSec < 1.0f) loaded.bossBreakRecoverSec = 1.0f;
	if (loaded.bossBreakRecoverSec > 30.0f) loaded.bossBreakRecoverSec = 30.0f;
	if (loaded.bossDashNarrowTelegraph < 0.10f) loaded.bossDashNarrowTelegraph = 0.10f;
	if (loaded.bossDashWideTelegraph < 0.10f) loaded.bossDashWideTelegraph = 0.10f;
	if (loaded.bossDashWideWidthRate < 0.10f) loaded.bossDashWideWidthRate = 0.10f;
	if (loaded.bossDashWideWidthRate > 1.00f) loaded.bossDashWideWidthRate = 1.00f;
	loaded.bossRandomRainCount = ClampInt(loaded.bossRandomRainCount, 1, 16);
	if (loaded.bossRandomRainTelegraph < 0.10f) loaded.bossRandomRainTelegraph = 0.10f;
	if (loaded.bossRandomRainRadiusScale < 0.25f) loaded.bossRandomRainRadiusScale = 0.25f;
	loaded.bossSummonMin = ClampInt(loaded.bossSummonMin, 1, 32);
	loaded.bossSummonMax = ClampInt(loaded.bossSummonMax, 1, 32);
	if (loaded.bossSummonMax < loaded.bossSummonMin) loaded.bossSummonMax = loaded.bossSummonMin;
	if (loaded.bossSummonTelegraph < 0.10f) loaded.bossSummonTelegraph = 0.10f;
	loaded.bossTrackingDropCount = ClampInt(loaded.bossTrackingDropCount, 1, 16);
	if (loaded.bossTrackingDropTelegraph < 0.10f) loaded.bossTrackingDropTelegraph = 0.10f;
	if (loaded.bossTrackingDropRadiusScale < 0.5f) loaded.bossTrackingDropRadiusScale = 0.5f;
	if (loaded.bossUltimateCrossTelegraph < 0.10f) loaded.bossUltimateCrossTelegraph = 0.10f;
	if (loaded.bossUltimateCrossLaneScale < 0.25f) loaded.bossUltimateCrossLaneScale = 0.25f;
	loaded.bossUltimateStompCount = ClampInt(loaded.bossUltimateStompCount, 1, 16);
	if (loaded.bossUltimateStompTelegraph < 0.10f) loaded.bossUltimateStompTelegraph = 0.10f;
	if (loaded.bossUltimateStompRepeatTelegraph < 0.10f) loaded.bossUltimateStompRepeatTelegraph = 0.10f;
	if (loaded.bossUltimateStompRadiusScale < 0.5f) loaded.bossUltimateStompRadiusScale = 0.5f;
	if (loaded.bossUltimateFieldTelegraph < 0.10f) loaded.bossUltimateFieldTelegraph = 0.10f;
	if (loaded.bossUltimateFieldSafeScale < 0.5f) loaded.bossUltimateFieldSafeScale = 0.5f;
	loaded.upgradeStepEasySmall = ClampInt(loaded.upgradeStepEasySmall, 0, 10);
	loaded.upgradeStepEasyLarge = ClampInt(loaded.upgradeStepEasyLarge, loaded.upgradeStepEasySmall, 10);
	loaded.upgradeStepNormalSmall = ClampInt(loaded.upgradeStepNormalSmall, 0, 10);
	loaded.upgradeStepNormalLarge = ClampInt(loaded.upgradeStepNormalLarge, loaded.upgradeStepNormalSmall, 10);
	loaded.upgradeStepHardSmall = ClampInt(loaded.upgradeStepHardSmall, 0, 10);
	loaded.upgradeStepHardLarge = ClampInt(loaded.upgradeStepHardLarge, loaded.upgradeStepHardSmall, 10);
	loaded.challengeNoDamageDuration = ClampFloat(loaded.challengeNoDamageDuration, 5.0f, 120.0f);
	loaded.challengeNoDamageAttackInterval = ClampFloat(loaded.challengeNoDamageAttackInterval, 0.20f, 10.0f);
	loaded.challengeNoDamageTelegraph = ClampFloat(loaded.challengeNoDamageTelegraph, 0.10f, 8.0f);
	loaded.challengeNoDamageRadius = ClampFloat(loaded.challengeNoDamageRadius, 0.20f, 8.0f);
	loaded.challengeNoDamageDamage = ClampFloat(loaded.challengeNoDamageDamage, 0.0f, 20.0f);
	loaded.challengeNoDamageBurstCount = ClampInt(loaded.challengeNoDamageBurstCount, 1, 8);
	loaded.challengeDefenseDuration = ClampFloat(loaded.challengeDefenseDuration, 5.0f, 180.0f);
	loaded.challengeDefenseBeaconMaxHp = ClampFloat(loaded.challengeDefenseBeaconMaxHp, 1.0f, 500.0f);
	loaded.challengeDefenseBeaconRadius = ClampFloat(loaded.challengeDefenseBeaconRadius, 0.30f, 6.0f);
	loaded.challengeDefenseBeaconContactDamage = ClampFloat(loaded.challengeDefenseBeaconContactDamage, 0.1f, 100.0f);
	loaded.challengeDefenseSpawnInterval = ClampFloat(loaded.challengeDefenseSpawnInterval, 0.20f, 15.0f);
	loaded.challengeDefenseEnemyMoveSpeed = ClampFloat(loaded.challengeDefenseEnemyMoveSpeed, 0.05f, 4.0f);
	loaded.challengeDefenseSpeedHpBonusRate = ClampFloat(loaded.challengeDefenseSpeedHpBonusRate, 0.0f, 5.0f);
	loaded.challengeDefenseRangedHpBonusRate = ClampFloat(loaded.challengeDefenseRangedHpBonusRate, 0.0f, 5.0f);
	loaded.challengeDefenseTankHpBonusRate = ClampFloat(loaded.challengeDefenseTankHpBonusRate, 0.0f, 5.0f);
	loaded.challengeDefenseEnemyCap = ClampInt(loaded.challengeDefenseEnemyCap, 1, 16);

	// 正常化済みの値だけを本体へ反映します。
	gameplay = loaded;
	loadedPreset = ClampInt(loadedPreset, 0, 2);
	gameplayDebug.difficultyPreset = loadedPreset;
	gameplayDebug.titleDifficultySelection = loadedPreset;
	loadedRogue.stageClearCount = ClampInt(loadedRogue.stageClearCount, 0, 9999);
	loadedRogue.attackPowerLevel = ClampUpgradeTier(loadedRogue.attackPowerLevel);
	loadedRogue.attackSpeedLevel = ClampUpgradeTier(loadedRogue.attackSpeedLevel);
	loadedRogue.evadeCooldownLevel = ClampUpgradeTier(loadedRogue.evadeCooldownLevel);
	loadedRogue.lastUpgradeType = ClampInt(loadedRogue.lastUpgradeType, -1, 999999);
	loadedRogue.skillSlot1 = NormalizeSkillTypeValue(loadedRogue.skillSlot1);
	loadedRogue.skillSlot2 = NormalizeSkillTypeValue(loadedRogue.skillSlot2);
	loadedRogue.skillShotRangeLevel = ClampInt(loadedRogue.skillShotRangeLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillShotPowerLevel = ClampInt(loadedRogue.skillShotPowerLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillShotCooldownLevel = ClampInt(loadedRogue.skillShotCooldownLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillNovaRangeLevel = ClampInt(loadedRogue.skillNovaRangeLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillNovaPowerLevel = ClampInt(loadedRogue.skillNovaPowerLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillNovaCooldownLevel = ClampInt(loadedRogue.skillNovaCooldownLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillOrbitRangeLevel = ClampInt(loadedRogue.skillOrbitRangeLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillOrbitCooldownLevel = ClampInt(loadedRogue.skillOrbitCooldownLevel, 0, RoguelikeUpgrade::kLevelMax);
	loadedRogue.skillOrbitCountLevel = ClampInt(loadedRogue.skillOrbitCountLevel, 0, RoguelikeUpgrade::kLevelMax);
	if (loadedRogue.skillSlot2 != RoguelikeUpgrade::SkillNone &&
		loadedRogue.skillSlot2 == loadedRogue.skillSlot1)
	{
		loadedRogue.skillSlot2 = RoguelikeUpgrade::SkillNone;
	}
	loadedRogue.rerollMaxPerStage = ClampInt(loadedRogue.rerollMaxPerStage, 0, 9);
	loadedRogue.rerollRemain = 0;
	loadedRogue.selectionPending = 0;
	loadedRogue.selectionPhase = RoguelikeUpgrade::SelectionNone;
	ResetOffers(loadedRogue.offers);
	InitializeRunStageRoute(loadedRogue);
	roguelike = loadedRogue;
	auto sanitizeBinding = [](int value, int fallback) -> int
	{
		if (value <= 0 || value > 255 || value == VK_ESCAPE)
		{
			return fallback;
		}
		return value;
	};
	loadedInput.moveUp = sanitizeBinding(loadedInput.moveUp, InputConfig{}.moveUp);
	loadedInput.moveDown = sanitizeBinding(loadedInput.moveDown, InputConfig{}.moveDown);
	loadedInput.moveLeft = sanitizeBinding(loadedInput.moveLeft, InputConfig{}.moveLeft);
	loadedInput.moveRight = sanitizeBinding(loadedInput.moveRight, InputConfig{}.moveRight);
	loadedInput.dash = sanitizeBinding(loadedInput.dash, InputConfig{}.dash);
	loadedInput.attack = sanitizeBinding(loadedInput.attack, InputConfig{}.attack);
	loadedInput.skill1 = sanitizeBinding(loadedInput.skill1, InputConfig{}.skill1);
	loadedInput.skill2 = sanitizeBinding(loadedInput.skill2, InputConfig{}.skill2);
	loadedInput.reroll = sanitizeBinding(loadedInput.reroll, InputConfig{}.reroll);
	auto sanitizeXInputBinding = [](int value, int fallback) -> int
	{
		return IsSupportedXInputBindingValue(value) ? value : fallback;
	};
	auto sanitizeDirectInputBinding = [](int value, int fallback) -> int
	{
		if (value < 0 || value > 31)
		{
			return fallback;
		}
		return value;
	};
	loadedInput.xinputConfirm = sanitizeXInputBinding(loadedInput.xinputConfirm, InputConfig{}.xinputConfirm);
	loadedInput.xinputDash = sanitizeXInputBinding(loadedInput.xinputDash, InputConfig{}.xinputDash);
	loadedInput.xinputAttack = sanitizeXInputBinding(loadedInput.xinputAttack, InputConfig{}.xinputAttack);
	loadedInput.xinputSkill1 = sanitizeXInputBinding(loadedInput.xinputSkill1, InputConfig{}.xinputSkill1);
	loadedInput.xinputSkill2 = sanitizeXInputBinding(loadedInput.xinputSkill2, InputConfig{}.xinputSkill2);
	loadedInput.xinputReroll = sanitizeXInputBinding(loadedInput.xinputReroll, InputConfig{}.xinputReroll);
	loadedInput.xinputTabPrev = sanitizeXInputBinding(loadedInput.xinputTabPrev, InputConfig{}.xinputTabPrev);
	loadedInput.xinputTabNext = sanitizeXInputBinding(loadedInput.xinputTabNext, InputConfig{}.xinputTabNext);
	loadedInput.directInputConfirm = sanitizeDirectInputBinding(loadedInput.directInputConfirm, InputConfig{}.directInputConfirm);
	loadedInput.directInputDash = sanitizeDirectInputBinding(loadedInput.directInputDash, InputConfig{}.directInputDash);
	loadedInput.directInputAttack = sanitizeDirectInputBinding(loadedInput.directInputAttack, InputConfig{}.directInputAttack);
	loadedInput.directInputSkill1 = sanitizeDirectInputBinding(loadedInput.directInputSkill1, InputConfig{}.directInputSkill1);
	loadedInput.directInputSkill2 = sanitizeDirectInputBinding(loadedInput.directInputSkill2, InputConfig{}.directInputSkill2);
	loadedInput.directInputReroll = sanitizeDirectInputBinding(loadedInput.directInputReroll, InputConfig{}.directInputReroll);
	loadedInput.directInputTabPrev = sanitizeDirectInputBinding(loadedInput.directInputTabPrev, InputConfig{}.directInputTabPrev);
	loadedInput.directInputTabNext = sanitizeDirectInputBinding(loadedInput.directInputTabNext, InputConfig{}.directInputTabNext);
	input = loadedInput;

	if (IsDefaultPathArgument(path))
	{
		// Keep runtime/source copies aligned after loading default configuration.
		SaveGameplayTuning();
	}

	return true;
}

/**
 * @brief 現在のゲームプレイ調整値を設定ファイルへ保存します。
 * @param path 保存先パスです。nullptr の場合は既定同期パス群へ保存します。
 * @return 1 つ以上保存に成功した場合は true です。
 */
bool Transfer::SaveGameplayTuning(const char* path) const
{
	// 単一パスへ書き出すためのローカル関数です。
	auto writeToPath = [&](const char* targetPath) -> bool
	{
		std::ofstream ofs(targetPath, std::ios::trunc);
		if (!ofs.is_open())
		{
			return false;
		}

		// すべての調整値を key=value 形式で書き出し、次回起動時に復元できるようにします。
		ofs << "# DX22 gameplay tuning\n";
		ofs << "enemyCount=" << gameplay.enemyCount << "\n";
		ofs << "waveMax=" << gameplay.waveMax << "\n";
		ofs << "waveEnemyAddPerWave=" << gameplay.waveEnemyAddPerWave << "\n";
		ofs << "upgradeStepEasySmall=" << gameplay.upgradeStepEasySmall << "\n";
		ofs << "upgradeStepEasyLarge=" << gameplay.upgradeStepEasyLarge << "\n";
		ofs << "upgradeStepNormalSmall=" << gameplay.upgradeStepNormalSmall << "\n";
		ofs << "upgradeStepNormalLarge=" << gameplay.upgradeStepNormalLarge << "\n";
		ofs << "upgradeStepHardSmall=" << gameplay.upgradeStepHardSmall << "\n";
		ofs << "upgradeStepHardLarge=" << gameplay.upgradeStepHardLarge << "\n";
		ofs << "groundTileSize=" << gameplay.groundTileSize << "\n";
		ofs << "cameraIntroDuration=" << gameplay.cameraIntroDuration << "\n";
		ofs << "cameraIntroFocusDistance=" << gameplay.cameraIntroFocusDistance << "\n";
		ofs << "attackWindup=" << gameplay.attackWindup << "\n";
		ofs << "attackDuration=" << gameplay.attackDuration << "\n";
		ofs << "attackRecovery=" << gameplay.attackRecovery << "\n";
		ofs << "attackCooldown=" << gameplay.attackCooldown << "\n";
		ofs << "skill1Cooldown=" << gameplay.skill1Cooldown << "\n";
		ofs << "skill2Cooldown=" << gameplay.skill2Cooldown << "\n";
		ofs << "screenShakeHitThreshold=" << gameplay.screenShakeHitThreshold << "\n";
		ofs << "screenShakeDuration=" << gameplay.screenShakeDuration << "\n";
		ofs << "screenShakeAmplitude=" << gameplay.screenShakeAmplitude << "\n";
		ofs << "attackSweepDegrees=" << gameplay.attackSweepDegrees << "\n";
		ofs << "attackSweepRadiusScale=" << gameplay.attackSweepRadiusScale << "\n";
		ofs << "attackWidthScale=" << gameplay.attackWidthScale << "\n";
		ofs << "attackDepthScale=" << gameplay.attackDepthScale << "\n";
		ofs << "attackHitStop=" << gameplay.attackHitStop << "\n";
		ofs << "attackKnockback=" << gameplay.attackKnockback << "\n";
		ofs << "attackHitFlash=" << gameplay.attackHitFlash << "\n";
		ofs << "attackTrailInterval=" << gameplay.attackTrailInterval << "\n";
		ofs << "attackTrailLife=" << gameplay.attackTrailLife << "\n";
		ofs << "attackTrailScale=" << gameplay.attackTrailScale << "\n";
		ofs << "playerDamageFlash=" << gameplay.playerDamageFlash << "\n";
		ofs << "playerDamageInvincible=" << gameplay.playerDamageInvincible << "\n";
		ofs << "playerDamageFlashScale=" << gameplay.playerDamageFlashScale << "\n";
		ofs << "enemyDefeatFlash=" << gameplay.enemyDefeatFlash << "\n";
		ofs << "enemyDefeatFlashScale=" << gameplay.enemyDefeatFlashScale << "\n";
		ofs << "volumeMaster=" << gameplay.volumeMaster << "\n";
		ofs << "volumeBgm=" << gameplay.volumeBgm << "\n";
		ofs << "volumeSe=" << gameplay.volumeSe << "\n";
		ofs << "directionMarkerAlpha=" << gameplay.directionMarkerAlpha << "\n";
		ofs << "directionMarkerOverlapAlpha=" << gameplay.directionMarkerOverlapAlpha << "\n";
		ofs << "bossHpBarWidthRate=" << gameplay.bossHpBarWidthRate << "\n";
		ofs << "bossHpBarHeightRate=" << gameplay.bossHpBarHeightRate << "\n";
		ofs << "bossGuardBarOffsetX=" << gameplay.bossGuardBarOffsetX << "\n";
		ofs << "bossGuardBarOffsetY=" << gameplay.bossGuardBarOffsetY << "\n";
		ofs << "bossGuardBarWidthRate=" << gameplay.bossGuardBarWidthRate << "\n";
		ofs << "bossGuardBarHeightRate=" << gameplay.bossGuardBarHeightRate << "\n";
		ofs << "bossSizeAreaScale=" << gameplay.bossSizeAreaScale << "\n";
		ofs << "bossMaxHp=" << gameplay.bossMaxHp << "\n";
		ofs << "bossAttackTelegraph=" << gameplay.bossAttackTelegraph << "\n";
		ofs << "bossAttackJumpOutTime=" << gameplay.bossAttackJumpOutTime << "\n";
		ofs << "bossAttackDashDuration=" << gameplay.bossAttackDashDuration << "\n";
		ofs << "bossAttackCooldown=" << gameplay.bossAttackCooldown << "\n";
		ofs << "bossAttackLanePlayerScale=" << gameplay.bossAttackLanePlayerScale << "\n";
		ofs << "bossAttackDamage=" << gameplay.bossAttackDamage << "\n";
		ofs << "bossAttackHitStop=" << gameplay.bossAttackHitStop << "\n";
		ofs << "bossHitShakeDuration=" << gameplay.bossHitShakeDuration << "\n";
		ofs << "bossHitShakeAmplitude=" << gameplay.bossHitShakeAmplitude << "\n";
		ofs << "bossGuardInitialMax=" << gameplay.bossGuardInitialMax << "\n";
		ofs << "bossGuardFinalMax=" << gameplay.bossGuardFinalMax << "\n";
		ofs << "bossGuardRecoverStep=" << gameplay.bossGuardRecoverStep << "\n";
		ofs << "bossDamageScaleNormal=" << gameplay.bossDamageScaleNormal << "\n";
		ofs << "bossDamageScaleBroken=" << gameplay.bossDamageScaleBroken << "\n";
		ofs << "bossBreakRecoverSec=" << gameplay.bossBreakRecoverSec << "\n";
		ofs << "bossDashNarrowTelegraph=" << gameplay.bossDashNarrowTelegraph << "\n";
		ofs << "bossDashWideTelegraph=" << gameplay.bossDashWideTelegraph << "\n";
		ofs << "bossDashWideWidthRate=" << gameplay.bossDashWideWidthRate << "\n";
		ofs << "bossRandomRainCount=" << gameplay.bossRandomRainCount << "\n";
		ofs << "bossRandomRainTelegraph=" << gameplay.bossRandomRainTelegraph << "\n";
		ofs << "bossRandomRainRadiusScale=" << gameplay.bossRandomRainRadiusScale << "\n";
		ofs << "bossSummonMin=" << gameplay.bossSummonMin << "\n";
		ofs << "bossSummonMax=" << gameplay.bossSummonMax << "\n";
		ofs << "bossSummonTelegraph=" << gameplay.bossSummonTelegraph << "\n";
		ofs << "bossTrackingDropCount=" << gameplay.bossTrackingDropCount << "\n";
		ofs << "bossTrackingDropTelegraph=" << gameplay.bossTrackingDropTelegraph << "\n";
		ofs << "bossTrackingDropRadiusScale=" << gameplay.bossTrackingDropRadiusScale << "\n";
		ofs << "bossUltimateCrossTelegraph=" << gameplay.bossUltimateCrossTelegraph << "\n";
		ofs << "bossUltimateCrossLaneScale=" << gameplay.bossUltimateCrossLaneScale << "\n";
		ofs << "bossUltimateStompCount=" << gameplay.bossUltimateStompCount << "\n";
		ofs << "bossUltimateStompTelegraph=" << gameplay.bossUltimateStompTelegraph << "\n";
		ofs << "bossUltimateStompRepeatTelegraph=" << gameplay.bossUltimateStompRepeatTelegraph << "\n";
		ofs << "bossUltimateStompRadiusScale=" << gameplay.bossUltimateStompRadiusScale << "\n";
		ofs << "bossUltimateFieldTelegraph=" << gameplay.bossUltimateFieldTelegraph << "\n";
		ofs << "bossUltimateFieldSafeScale=" << gameplay.bossUltimateFieldSafeScale << "\n";
		ofs << "enemyAttackWindup=" << gameplay.enemyAttackWindup << "\n";
		ofs << "enemyAttackCooldown=" << gameplay.enemyAttackCooldown << "\n";
		ofs << "enemyAttackRangeMin=" << gameplay.enemyAttackRangeMin << "\n";
		ofs << "enemyAttackRangeScale=" << gameplay.enemyAttackRangeScale << "\n";
		ofs << "enemyAttackDamage=" << gameplay.enemyAttackDamage << "\n";
		ofs << "enemyMoveSpeed=" << gameplay.enemyMoveSpeed << "\n";
		ofs << "waveEnemyMoveSpeedAdd=" << gameplay.waveEnemyMoveSpeedAdd << "\n";
		ofs << "waveEnemyAttackDamageScalePerWave=" << gameplay.waveEnemyAttackDamageScalePerWave << "\n";
		ofs << "enemyProjectileSpeed=" << gameplay.enemyProjectileSpeed << "\n";
		ofs << "enemyProjectileLife=" << gameplay.enemyProjectileLife << "\n";
		ofs << "enemyProjectileRadius=" << gameplay.enemyProjectileRadius << "\n";
		ofs << "enemyProjectileDamageScale=" << gameplay.enemyProjectileDamageScale << "\n";
		ofs << "enemySeparationRadius=" << gameplay.enemySeparationRadius << "\n";
		ofs << "enemySeparationWeight=" << gameplay.enemySeparationWeight << "\n";
		ofs << "enemySeparationMaxOffset=" << gameplay.enemySeparationMaxOffset << "\n";
		ofs << "enemySpawnRingScale=" << gameplay.enemySpawnRingScale << "\n";
		ofs << "enemySpawnJitterScale=" << gameplay.enemySpawnJitterScale << "\n";
		ofs << "enemySpawnMinPlayerDist=" << gameplay.enemySpawnMinPlayerDist << "\n";
		ofs << "enemySpawnMinEnemyDist=" << gameplay.enemySpawnMinEnemyDist << "\n";
		ofs << "pushSlop=" << gameplay.pushSlop << "\n";
		ofs << "playerPushShare=" << gameplay.playerPushShare << "\n";
		ofs << "enemyPushShare=" << gameplay.enemyPushShare << "\n";
		ofs << "challengeNoDamageDuration=" << gameplay.challengeNoDamageDuration << "\n";
		ofs << "challengeNoDamageAttackInterval=" << gameplay.challengeNoDamageAttackInterval << "\n";
		ofs << "challengeNoDamageTelegraph=" << gameplay.challengeNoDamageTelegraph << "\n";
		ofs << "challengeNoDamageRadius=" << gameplay.challengeNoDamageRadius << "\n";
		ofs << "challengeNoDamageDamage=" << gameplay.challengeNoDamageDamage << "\n";
		ofs << "challengeNoDamageBurstCount=" << gameplay.challengeNoDamageBurstCount << "\n";
		ofs << "challengeDefenseDuration=" << gameplay.challengeDefenseDuration << "\n";
		ofs << "challengeDefenseBeaconMaxHp=" << gameplay.challengeDefenseBeaconMaxHp << "\n";
		ofs << "challengeDefenseBeaconRadius=" << gameplay.challengeDefenseBeaconRadius << "\n";
		ofs << "challengeDefenseBeaconContactDamage=" << gameplay.challengeDefenseBeaconContactDamage << "\n";
		ofs << "challengeDefenseSpawnInterval=" << gameplay.challengeDefenseSpawnInterval << "\n";
		ofs << "challengeDefenseEnemyMoveSpeed=" << gameplay.challengeDefenseEnemyMoveSpeed << "\n";
		ofs << "challengeDefenseSpeedHpBonusRate=" << gameplay.challengeDefenseSpeedHpBonusRate << "\n";
		ofs << "challengeDefenseRangedHpBonusRate=" << gameplay.challengeDefenseRangedHpBonusRate << "\n";
		ofs << "challengeDefenseTankHpBonusRate=" << gameplay.challengeDefenseTankHpBonusRate << "\n";
		ofs << "challengeDefenseEnemyCap=" << gameplay.challengeDefenseEnemyCap << "\n";
		ofs << "bindMoveUp=" << input.moveUp << "\n";
		ofs << "bindMoveDown=" << input.moveDown << "\n";
		ofs << "bindMoveLeft=" << input.moveLeft << "\n";
		ofs << "bindMoveRight=" << input.moveRight << "\n";
		ofs << "bindDash=" << input.dash << "\n";
		ofs << "bindAttack=" << input.attack << "\n";
		ofs << "bindSkill1=" << input.skill1 << "\n";
		ofs << "bindSkill2=" << input.skill2 << "\n";
		ofs << "bindReroll=" << input.reroll << "\n";
		ofs << "bindXInputConfirm=" << input.xinputConfirm << "\n";
		ofs << "bindXInputDash=" << input.xinputDash << "\n";
		ofs << "bindXInputAttack=" << input.xinputAttack << "\n";
		ofs << "bindXInputSkill1=" << input.xinputSkill1 << "\n";
		ofs << "bindXInputSkill2=" << input.xinputSkill2 << "\n";
		ofs << "bindXInputReroll=" << input.xinputReroll << "\n";
		ofs << "bindXInputTabPrev=" << input.xinputTabPrev << "\n";
		ofs << "bindXInputTabNext=" << input.xinputTabNext << "\n";
		ofs << "bindDirectInputConfirm=" << input.directInputConfirm << "\n";
		ofs << "bindDirectInputDash=" << input.directInputDash << "\n";
		ofs << "bindDirectInputAttack=" << input.directInputAttack << "\n";
		ofs << "bindDirectInputSkill1=" << input.directInputSkill1 << "\n";
		ofs << "bindDirectInputSkill2=" << input.directInputSkill2 << "\n";
		ofs << "bindDirectInputReroll=" << input.directInputReroll << "\n";
		ofs << "bindDirectInputTabPrev=" << input.directInputTabPrev << "\n";
		ofs << "bindDirectInputTabNext=" << input.directInputTabNext << "\n";
		ofs << "difficultyPreset=" << gameplayDebug.difficultyPreset << "\n";
		ofs << "stageClearCount=" << roguelike.stageClearCount << "\n";
		ofs << "attackPowerLevel=" << roguelike.attackPowerLevel << "\n";
		ofs << "attackSpeedLevel=" << roguelike.attackSpeedLevel << "\n";
		ofs << "evadeCooldownLevel=" << roguelike.evadeCooldownLevel << "\n";
		ofs << "lastUpgradeType=" << roguelike.lastUpgradeType << "\n";
		ofs << "skillSlot1=" << roguelike.skillSlot1 << "\n";
		ofs << "skillSlot2=" << roguelike.skillSlot2 << "\n";
		ofs << "skillShotRangeLevel=" << roguelike.skillShotRangeLevel << "\n";
		ofs << "skillShotPowerLevel=" << roguelike.skillShotPowerLevel << "\n";
		ofs << "skillShotCooldownLevel=" << roguelike.skillShotCooldownLevel << "\n";
		ofs << "skillNovaRangeLevel=" << roguelike.skillNovaRangeLevel << "\n";
		ofs << "skillNovaPowerLevel=" << roguelike.skillNovaPowerLevel << "\n";
		ofs << "skillNovaCooldownLevel=" << roguelike.skillNovaCooldownLevel << "\n";
		ofs << "skillOrbitRangeLevel=" << roguelike.skillOrbitRangeLevel << "\n";
		ofs << "skillOrbitCooldownLevel=" << roguelike.skillOrbitCooldownLevel << "\n";
		ofs << "skillOrbitCountLevel=" << roguelike.skillOrbitCountLevel << "\n";
		ofs << "upgradeRerollMax=" << roguelike.rerollMaxPerStage << "\n";
		return ofs.good();
	};

	// 明示パス指定時は、そのパスだけを更新します。
	if (!IsDefaultPathArgument(path))
	{
		return writeToPath(ResolvePath(path));
	}

	bool anySaved = false;
	// 既定保存時は実行側/ソース側の代表パスへ同期保存します。
	const char* syncPaths[] =
	{
		kDefaultGameplayTuningPath,
		kDebugGameplayTuningPath,
		kMirrorGameplayTuningPath,
		kUpstreamMirrorGameplayTuningPath
	};
	for (const char* syncPath : syncPaths)
	{
		if (writeToPath(syncPath))
		{
			anySaved = true;
		}
	}
	return anySaved;
}
