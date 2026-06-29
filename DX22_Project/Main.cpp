#include "Main.h"
#include <memory>
#include "DirectX.h"
#include "Geometory.h"
#include "Sprite.h"
#include "Input.h"
#include "SceneManager.h"
#include "Defines.h"
#include "MainImGui.h"
#include "ShaderList.h"
#include "Transfer.h"
#include "Sound.h"
// rand初期化用
#include <cstdlib>
#include <ctime>
#include <cstdio> // 追加
#include <cmath>

// ImGui
#include "imgui.h"
#include "Easing.h"

// デバッグ用
#include "DebugUtil.h"

namespace
{
	struct FramePerfStats
	{
		float updateInputMs = 0.0f;
		float updateAudioMs = 0.0f;
		float sceneUpdateMs = 0.0f;
		float updateTotalMs = 0.0f;
		float beginDrawMs = 0.0f;
		float sceneDrawMs = 0.0f;
		float overlayDrawMs = 0.0f;
		float endDrawMs = 0.0f;
		float drawTotalMs = 0.0f;
		float frameCpuMs = 0.0f;
		SceneManager::SceneType scene = SceneManager::SceneType::SCENE_TITLE;
	};

	FramePerfStats g_perfFrameWorking{};
	FramePerfStats g_perfFrameLast{};
	FramePerfStats g_perfFrameAvg{};

	void BlendPerfValue(float& accum, float sample)
	{
		if (accum <= 0.0f)
		{
			accum = sample;
			return;
		}
		accum = accum * 0.85f + sample * 0.15f;
	}

	void CommitPerfFrame(const FramePerfStats& sample)
	{
		g_perfFrameLast = sample;
		BlendPerfValue(g_perfFrameAvg.updateInputMs, sample.updateInputMs);
		BlendPerfValue(g_perfFrameAvg.updateAudioMs, sample.updateAudioMs);
		BlendPerfValue(g_perfFrameAvg.sceneUpdateMs, sample.sceneUpdateMs);
		BlendPerfValue(g_perfFrameAvg.updateTotalMs, sample.updateTotalMs);
		BlendPerfValue(g_perfFrameAvg.beginDrawMs, sample.beginDrawMs);
		BlendPerfValue(g_perfFrameAvg.sceneDrawMs, sample.sceneDrawMs);
		BlendPerfValue(g_perfFrameAvg.overlayDrawMs, sample.overlayDrawMs);
		BlendPerfValue(g_perfFrameAvg.endDrawMs, sample.endDrawMs);
		BlendPerfValue(g_perfFrameAvg.drawTotalMs, sample.drawTotalMs);
		BlendPerfValue(g_perfFrameAvg.frameCpuMs, sample.frameCpuMs);
		g_perfFrameAvg.scene = sample.scene;
	}

	const char* GetSceneProfileName(SceneManager::SceneType scene)
	{
		switch (scene)
		{
		case SceneManager::SceneType::SCENE_TITLE:
			return "Title";
		case SceneManager::SceneType::SCENE_GAME:
			return "Game";
		case SceneManager::SceneType::SCENE_RESULT:
			return "Result";
		case SceneManager::SceneType::SCENE_ENGINE_EDITOR:
			return "CastleEditor";
		case SceneManager::SceneType::SCENE_EFFECT_DEBUG:
			return "EffectDebug";
		case SceneManager::SceneType::SCENE_BOSS_EDITOR:
			return "BossEditor";
		case SceneManager::SceneType::SCENE_FINAL_BOSS_EDITOR:
			return "FinalBossEditor";
		case SceneManager::SceneType::SCENE_NEW_BOSS_EDITOR:
			return "NewLastBoss";
		case SceneManager::SceneType::SCENE_NARAKU_EDITOR:
			return "NarakuEditor";
		case SceneManager::SceneType::SCENE_NARAKU_PIECE_EDITOR:
			return "NarakuPieceEditor";
		case SceneManager::SceneType::SCENE_NARAKU_PROTO:
			return "NarakuProto";
		default:
			return "Unknown";
		}
	}

	const char* GetWeaponTypeLabel(int weaponType)
	{
		switch (weaponType)
		{
		case Transfer::RoguelikeUpgrade::WeaponBasic:
			return u8"基本型";
		case Transfer::RoguelikeUpgrade::WeaponHeavy:
			return u8"一撃型";
		case Transfer::RoguelikeUpgrade::WeaponRapid:
			return u8"手数型";
		case Transfer::RoguelikeUpgrade::WeaponRanged:
			return u8"遠距離型";
		default:
			return u8"未設定";
		}
	}

	const char* GetActionSkillLabel(int skillType)
	{
		switch (skillType)
		{
		case Transfer::RoguelikeUpgrade::ActionSkillWhirl:
			return u8"薙ぎ払い";
		case Transfer::RoguelikeUpgrade::ActionSkillRush:
			return u8"突進";
		case Transfer::RoguelikeUpgrade::ActionSkillAmbush:
			return u8"奇襲";
		case Transfer::RoguelikeUpgrade::ActionSkillChainThrow:
			return u8"鎖投げ";
		case Transfer::RoguelikeUpgrade::ActionSkillFireball:
			return u8"火球";
		case Transfer::RoguelikeUpgrade::ActionSkillBloodSlash:
			return u8"出血斬";
		default:
			return u8"未選択";
		}
	}

	const char* GetTraitLabel(int traitType)
	{
		switch (traitType)
		{
		case Transfer::RoguelikeUpgrade::TraitCooldown:
			return u8"クールタイム";
		case Transfer::RoguelikeUpgrade::TraitWeapon:
			return u8"武器";
		case Transfer::RoguelikeUpgrade::TraitChain:
			return u8"鎖";
		case Transfer::RoguelikeUpgrade::TraitBlood:
			return u8"血";
		case Transfer::RoguelikeUpgrade::TraitFire:
			return u8"炎";
		default:
			return u8"未定";
		}
	}

	const char* GetArtifactLabel(int artifactType)
	{
		switch (artifactType)
		{
		case Transfer::RoguelikeUpgrade::ArtifactBarbarianNecklace:
			return u8"蛮族の首飾り";
		case Transfer::RoguelikeUpgrade::ArtifactGreatShieldCrest:
			return u8"大盾の紋章";
		case Transfer::RoguelikeUpgrade::ArtifactGlassShoes:
			return u8"ガラスの靴";
		case Transfer::RoguelikeUpgrade::ArtifactMagicPiggyBank:
			return u8"魔法の貯金箱";
		case Transfer::RoguelikeUpgrade::ArtifactDiceBox:
			return u8"サイコロのでる箱";
		default:
			return u8"未定";
		}
	}

	const char* GetBossArchetypeLabel(int bossType)
	{
		switch (bossType)
		{
		case Transfer::RoguelikeUpgrade::BossHeavyMelee:
			return u8"近距離鈍重型";
		case Transfer::RoguelikeUpgrade::BossLightRanged:
			return u8"遠距離軽装型";
		case Transfer::RoguelikeUpgrade::BossBalancedMid:
			return u8"中距離バランス型";
		case Transfer::RoguelikeUpgrade::BossSwiftDebuff:
			return u8"近距離俊敏デバフ型";
		case Transfer::RoguelikeUpgrade::BossFinalBarrage:
			return u8"ラスボス";
		default:
			return u8"未定";
		}
	}

	void FormatHudFixedFloat(float value, char* out, size_t outSize)
	{
		float safeValue = value;
		if (safeValue < 0.0f)
		{
			safeValue = 0.0f;
		}
		if (safeValue > 999.99f)
		{
			safeValue = 999.99f;
		}
		sprintf_s(out, outSize, "%06.2f", safeValue);
	}

	ImU32 GetPerfBorderColor(float frameMs)
	{
		const float frameBudget60 = 1000.0f / 60.0f;
		if (frameMs <= frameBudget60 * 0.90f)
		{
			return IM_COL32(90, 210, 120, 180);
		}
		if (frameMs <= frameBudget60)
		{
			return IM_COL32(255, 210, 90, 180);
		}
		return IM_COL32(255, 110, 110, 180);
	}
}

HRESULT Init(HWND hWnd, UINT width, UINT height)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	HRESULT hr;
	hr = InitSound();
	if (FAILED(hr)) { return hr; }

	// DirectX初期化
	hr = InitDirectX(hWnd, width, height, false);
	if (FAILED(hr)) { UninitSound(); return hr; }

	std::srand(static_cast<unsigned int>(std::time(NULL)));
	{
		TRAN_INS;
		tran.LoadGameplayTuning();
	}

	// 他機能初期化
	Geometory::Init();
	Sprite::Init();
	InitInput(hWnd);
	ShaderList::Init();

	// シーン
	SceneManager::Init();

	return hr;
}

void Uninit()
{
	double t0 = NowMS();
	DebugLog("App shutdown begin\n");
	{
		TRAN_INS;
		tran.SaveGameplayTuning();
	}

	SceneManager::Uninit();

	ShaderList::Uninit();
	UninitInput();
	Sprite::Uninit();
	Geometory::Uninit();
	ReleaseEngineEditorRenderTargets();
	UninitSound();
	UninitDirectX();

	DebugLog("App shutdown end : %.2f ms\n", NowMS() - t0);
}

void Update()
{
	FramePerfStats perf{};
	perf.scene = SceneManager::GetCurrent();
	const double updateStart = NowMS();

	double sectionStart = updateStart;
	UpdateInput();
	perf.updateInputMs = static_cast<float>(NowMS() - sectionStart);

	sectionStart = NowMS();
	{
		TRAN_INS;
		SetMasterVolume(tran.gameplay.volumeMaster);
		SetBgmVolume(tran.gameplay.volumeBgm);
		SetSeVolume(tran.gameplay.volumeSe);
	}
	UpdateSound();
	perf.updateAudioMs = static_cast<float>(NowMS() - sectionStart);

	sectionStart = NowMS();
	SceneManager::Update();
	perf.sceneUpdateMs = static_cast<float>(NowMS() - sectionStart);
	perf.updateTotalMs = static_cast<float>(NowMS() - updateStart);
	perf.scene = SceneManager::GetCurrent();
	g_perfFrameWorking = perf;
}

void Draw()
{
	const double drawStart = NowMS();
	double sectionStart = drawStart;
	BeginDrawDirectX();
	g_perfFrameWorking.beginDrawMs = static_cast<float>(NowMS() - sectionStart);

	static bool show_overlay = false;
	if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) show_overlay = !show_overlay;

	auto drawPauseMenuOverlay = [&](Transfer& tran)
	{
		if (SceneManager::GetCurrent() != SceneManager::SceneType::SCENE_GAME ||
			tran.gameplayDebug.pauseMenuOpen == 0)
		{
			return;
		}

		const bool isFullscreen = IsAppFullscreen();
		const OverlayScaleMetrics overlayMetrics = GetAdaptiveOverlayScaleMetrics(tran, isFullscreen);
		const float uiScale = overlayMetrics.uiScale;
		const float fontScale = overlayMetrics.fontScale;
		const float buttonScale = overlayMetrics.buttonScale;
		ImGuiViewport* vp = ImGui::GetMainViewport();
		const bool pauseOptionOpen = (tran.gameplayDebug.pauseOptionOpen != 0);
		const int selectedTab = (tran.gameplayDebug.pauseTabIndex < 0) ? 0
			: (tran.gameplayDebug.pauseTabIndex > 2 ? 2 : tran.gameplayDebug.pauseTabIndex);
		const ImVec2 windowSize = pauseOptionOpen
			? ImVec2(700.0f * uiScale, 520.0f * uiScale)
			: ImVec2(640.0f * uiScale, 500.0f * uiScale);
		const ImVec2 windowPos(vp->Pos.x + (vp->Size.x - windowSize.x) * 0.5f,
							vp->Pos.y + (vp->Size.y - windowSize.y) * 0.5f);
		ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f * uiScale, 16.0f * uiScale));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f * uiScale, 12.0f * uiScale));
		ImGui::Begin("##pause_menu_overlay", nullptr,
					ImGuiWindowFlags_NoTitleBar |
					ImGuiWindowFlags_NoCollapse |
					ImGuiWindowFlags_NoResize |
					ImGuiWindowFlags_NoMove |
					ImGuiWindowFlags_NoDocking);
		ImGui::SetWindowFontScale(fontScale);
		ImGui::TextUnformatted(u8"ポーズ");
		ImGui::Separator();
		if (ImGui::BeginTabBar("##pause_tabs", ImGuiTabBarFlags_FittingPolicyShrink))
		{
			if (ImGui::BeginTabItem(u8"ゲーム", nullptr, (selectedTab == 0) ? ImGuiTabItemFlags_SetSelected : 0))
			{
				tran.gameplayDebug.pauseTabIndex = 0;
				const char* difficultyText = GetDifficultyName(tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset));
				const int enemiesAlive = (tran.gameplayDebug.enemiesAlive < 0) ? 0 : tran.gameplayDebug.enemiesAlive;
				const int enemiesTarget = (tran.gameplayDebug.enemiesTarget < 0) ? 0 : tran.gameplayDebug.enemiesTarget;
				int defeated = enemiesTarget - enemiesAlive;
				if (defeated < 0) defeated = 0;
				if (defeated > enemiesTarget) defeated = enemiesTarget;
				const float defeatRate = (enemiesTarget > 0)
					? (static_cast<float>(defeated) / static_cast<float>(enemiesTarget)) * 100.0f
					: 0.0f;

				ImGui::Text(u8"現在Wave: %d / %d", tran.gameplayDebug.currentWave, tran.gameplayDebug.maxWave);
				ImGui::Text(u8"難易度: %s", difficultyText);
				ImGui::Text(u8"敵撃破率: %.0f%%", defeatRate);
				if (tran.gameplayDebug.bossBattleActive != 0)
				{
					ImGui::Spacing();
					ImGui::TextUnformatted(u8"現在はボス戦状態です。");
				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::TextUnformatted(u8"タブ切替: 1 / 2 / 3");
				ImGui::TextUnformatted(u8"コントローラー: Configured Prev / Next Tab");
				ImGui::TextUnformatted(u8"閉じる: Esc / Start");
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(u8"強化状態", nullptr, (selectedTab == 1) ? ImGuiTabItemFlags_SetSelected : 0))
			{
				tran.gameplayDebug.pauseTabIndex = 1;
				ImGui::Text(u8"攻撃Lv: %d", tran.roguelike.attackPowerLevel);
				ImGui::Text(u8"攻撃頻度Lv: %d", tran.roguelike.attackSpeedLevel);
				ImGui::Text(u8"回避Lv: %d", tran.roguelike.evadeCooldownLevel);
				if (tran.roguelike.lastUpgradeType >= 0)
				{
					char lastUpgradeText[128]{};
					FormatUpgradeLabel(tran, tran.roguelike.lastUpgradeType, lastUpgradeText, sizeof(lastUpgradeText));
					ImGui::Text(u8"最終取得: %s", lastUpgradeText);
				}

				const auto drawSkillStatus = [&](const char* slotLabel, int skillType)
				{
					char detail[128]{};
					switch (skillType)
					{
					case Transfer::RoguelikeUpgrade::SkillShot:
						sprintf_s(detail, sizeof(detail), u8"範囲Lv%d / 威力Lv%d / CTLv%d",
								  tran.roguelike.skillShotRangeLevel,
								  tran.roguelike.skillShotPowerLevel,
								  tran.roguelike.skillShotCooldownLevel);
						break;
					case Transfer::RoguelikeUpgrade::SkillNova:
						sprintf_s(detail, sizeof(detail), u8"範囲Lv%d / 威力Lv%d / CTLv%d",
								  tran.roguelike.skillNovaRangeLevel,
								  tran.roguelike.skillNovaPowerLevel,
								  tran.roguelike.skillNovaCooldownLevel);
						break;
					case Transfer::RoguelikeUpgrade::SkillOrbit:
						sprintf_s(detail, sizeof(detail), u8"範囲Lv%d / CTLv%d / 個数Lv%d",
								  tran.roguelike.skillOrbitRangeLevel,
								  tran.roguelike.skillOrbitCooldownLevel,
								  tran.roguelike.skillOrbitCountLevel);
						break;
					default:
						sprintf_s(detail, sizeof(detail), "%s", u8"未取得");
						break;
					}

					ImGui::SeparatorText(slotLabel);
					ImGui::Text(u8"スキル: %s", GetSkillNameByType(skillType));
					ImGui::TextUnformatted(detail);
				};

				drawSkillStatus("Q", tran.roguelike.skillSlot1);
				drawSkillStatus("E", tran.roguelike.skillSlot2);
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(u8"設定", nullptr, (selectedTab == 2) ? ImGuiTabItemFlags_SetSelected : 0))
			{
				tran.gameplayDebug.pauseTabIndex = 2;
				if (pauseOptionOpen)
				{
					const int selected = tran.gameplayDebug.pauseOptionSelection;
					const float closeButtonWidth = 88.0f * uiScale;
					ImGui::TextUnformatted(u8"Option");
					ImGui::SameLine();
					ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - closeButtonWidth);
					if (ImGui::Button("Close##pause_option_close", ImVec2(closeButtonWidth, 0.0f)))
					{
						tran.gameplayDebug.pauseOptionRequestClose = 1;
					}
					ImGui::Separator();

					const char* selectedLabel = u8"Master";
					switch (selected)
					{
					case 1: selectedLabel = u8"BGM"; break;
					case 2: selectedLabel = u8"SE"; break;
					case 3: selectedLabel = u8"表示"; break;
					case 4: selectedLabel = u8"戻る"; break;
					default: break;
					}
					ImGui::Text(u8"選択中: %s", selectedLabel);

					float master = tran.gameplay.volumeMaster;
					if (ImGui::SliderFloat(u8"Master", &master, 0.0f, 2.0f, "%.2f"))
					{
						tran.gameplay.volumeMaster = master;
					}
					float bgm = tran.gameplay.volumeBgm;
					if (ImGui::SliderFloat(u8"BGM", &bgm, 0.0f, 2.0f, "%.2f"))
					{
						tran.gameplay.volumeBgm = bgm;
					}
					float se = tran.gameplay.volumeSe;
					if (ImGui::SliderFloat(u8"SE", &se, 0.0f, 2.0f, "%.2f"))
					{
						tran.gameplay.volumeSe = se;
					}

					bool fullscreenChecked = isFullscreen;
					bool windowChecked = !isFullscreen;
					if (ImGui::Checkbox(u8"Fullscreen", &fullscreenChecked))
					{
						SetAppFullscreen(fullscreenChecked);
					}
					ImGui::SameLine();
					if (ImGui::Checkbox(u8"Window", &windowChecked))
					{
						SetAppFullscreen(!windowChecked);
					}
					ImGui::Separator();
					ImGui::TextUnformatted(u8"移動: W/S or ↑/↓");
					ImGui::TextUnformatted(u8"変更: A/D or ←/→");
					ImGui::TextUnformatted(u8"決定: Enter / F / Space / Controller Confirm");
					ImGui::TextUnformatted(u8"戻る: Esc / Start");
				}
				else
				{
					const int selected = tran.gameplayDebug.pauseMenuSelection;
					ImGui::TextUnformatted(u8"設定項目");
					ImGui::Separator();
					ImGui::TextUnformatted(u8"選択: A/D W/S ←→↑↓");
					ImGui::TextUnformatted(u8"決定: Enter / F / Space / Controller Confirm");
					ImGui::TextUnformatted(u8"タブ切替: 1 / 2 / 3 or Configured Prev / Next Tab");
					const ImVec2 buttonSize(300.0f * uiScale * buttonScale, 62.0f * uiScale * buttonScale);
					const auto drawMenuButton = [&](int index, const char* label, int request)
					{
						if (selected == index)
						{
							ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(40, 120, 210, 230));
						}
						if (ImGui::Button(label, buttonSize))
						{
							tran.gameplayDebug.pauseMenuRequest = request;
						}
						if (selected == index)
						{
							ImGui::PopStyleColor();
						}
					};
					drawMenuButton(0, u8"続行", 1);
					drawMenuButton(1, u8"Option", 3);
					drawMenuButton(2, u8"Titleへ", 2);
				}
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::SetWindowFontScale(1.0f);
		ImGui::End();
		ImGui::PopStyleVar(2);
	};

	auto drawOverlayHud = [&](const Transfer& tran)
	{
		if (!show_overlay)
		{
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
		const ImVec2 pad(8.0f, 6.0f);
		const float frameBudget60 = 1000.0f / 60.0f;
		const FramePerfStats& perfLast = g_perfFrameLast;
		const FramePerfStats& perfAvg = g_perfFrameAvg;
		const ImU32 borderColor = GetPerfBorderColor(perfAvg.frameCpuMs);
		char fpsText[16]{};
		char cpuLastText[16]{};
		char cpuAvgText[16]{};
		char budget60Text[16]{};
		char updateText[16]{};
		char updateInputText[16]{};
		char updateSoundText[16]{};
		char updateSceneText[16]{};
		char inputKbmText[16]{};
		char inputXiText[16]{};
		char inputDiText[16]{};
		char drawText[16]{};
		char beginDrawText[16]{};
		char sceneDrawText[16]{};
		char overlayDrawText[16]{};
		char endDrawText[16]{};
		FormatHudFixedFloat(io.Framerate, fpsText, sizeof(fpsText));
		FormatHudFixedFloat(perfLast.frameCpuMs, cpuLastText, sizeof(cpuLastText));
		FormatHudFixedFloat(perfAvg.frameCpuMs, cpuAvgText, sizeof(cpuAvgText));
		FormatHudFixedFloat(frameBudget60, budget60Text, sizeof(budget60Text));
		FormatHudFixedFloat(perfLast.updateTotalMs, updateText, sizeof(updateText));
		FormatHudFixedFloat(perfLast.updateInputMs, updateInputText, sizeof(updateInputText));
		FormatHudFixedFloat(perfLast.updateAudioMs, updateSoundText, sizeof(updateSoundText));
		FormatHudFixedFloat(perfLast.sceneUpdateMs, updateSceneText, sizeof(updateSceneText));
		FormatHudFixedFloat(GetInputKeyboardMouseMs(), inputKbmText, sizeof(inputKbmText));
		FormatHudFixedFloat(GetInputXInputMs(), inputXiText, sizeof(inputXiText));
		FormatHudFixedFloat(GetInputDirectInputMs(), inputDiText, sizeof(inputDiText));
		FormatHudFixedFloat(perfLast.drawTotalMs, drawText, sizeof(drawText));
		FormatHudFixedFloat(perfLast.beginDrawMs, beginDrawText, sizeof(beginDrawText));
		FormatHudFixedFloat(perfLast.sceneDrawMs, sceneDrawText, sizeof(sceneDrawText));
		FormatHudFixedFloat(perfLast.overlayDrawMs, overlayDrawText, sizeof(overlayDrawText));
		FormatHudFixedFloat(perfLast.endDrawMs, endDrawText, sizeof(endDrawText));

#ifdef _DEBUG
		const ImVec2 mouse = io.MousePos;
		const ImVec2 center(vp->Pos.x + vp->Size.x * 0.5f, vp->Pos.y + vp->Size.y * 0.5f);
		const float cross = 10.0f;
		dl->AddLine(ImVec2(mouse.x - cross, mouse.y), ImVec2(mouse.x + cross, mouse.y), IM_COL32(255, 255, 0, 255), 1.0f);
		dl->AddLine(ImVec2(mouse.x, mouse.y - cross), ImVec2(mouse.x, mouse.y + cross), IM_COL32(255, 255, 0, 255), 1.0f);
		dl->AddCircle(center, 6.0f, IM_COL32(0, 255, 255, 255), 16, 1.0f);
		char playerHpText[16]{};
		char playerMaxHpText[16]{};
		FormatHudFixedFloat(tran.player.hp, playerHpText, sizeof(playerHpText));
		FormatHudFixedFloat(tran.player.maxHp, playerMaxHpText, sizeof(playerMaxHpText));

		char hud[768];
		sprintf_s(
			hud,
			u8"FPS %s\n"
			u8"Scene %s\n"
			u8"CPU Last %s ms / Avg %s ms / 60FPS %s ms\n"
			u8"Update %s ms (Input %s / Sound %s / Scene %s)\n"
			u8"Input %s ms (KBM %s / XI %s / DI %s)\n"
			u8"Draw %s ms (Begin %s / Scene %s / Overlay %s / Present %s)\n"
			u8"マウス (%04.0f, %04.0f)\n"
			u8"プレイヤーHP %s / %s",
			fpsText,
			GetSceneProfileName(perfLast.scene),
			cpuLastText,
			cpuAvgText,
			budget60Text,
			updateText,
			updateInputText,
			updateSoundText,
			updateSceneText,
			updateInputText,
			inputKbmText,
			inputXiText,
			inputDiText,
			drawText,
			beginDrawText,
			sceneDrawText,
			overlayDrawText,
			endDrawText,
			mouse.x,
			mouse.y,
			playerHpText,
			playerMaxHpText);

		const char* hudTemplate =
			u8"FPS 888.88\n"
			u8"Scene CastleEditor\n"
			u8"CPU Last 888.88 ms / Avg 888.88 ms / 60FPS 888.88 ms\n"
			u8"Update 888.88 ms (Input 888.88 / Sound 888.88 / Scene 888.88)\n"
			u8"Input 888.88 ms (KBM 888.88 / XI 888.88 / DI 888.88)\n"
			u8"Draw 888.88 ms (Begin 888.88 / Scene 888.88 / Overlay 888.88 / Present 888.88)\n"
			u8"マウス (8888, 8888)\n"
			u8"プレイヤーHP 888.88 / 888.88";
		const ImVec2 textSize = ImGui::CalcTextSize(hudTemplate);
		const ImVec2 boxMin(vp->Pos.x + vp->Size.x - textSize.x - pad.x * 2.0f - 10.0f, vp->Pos.y + 10.0f);
		const ImVec2 boxMax(boxMin.x + textSize.x + pad.x * 2.0f, boxMin.y + textSize.y + pad.y * 2.0f);
		dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 160), 4.0f);
		dl->AddRect(boxMin, boxMax, borderColor, 4.0f);
		dl->AddText(ImVec2(boxMin.x + pad.x, boxMin.y + pad.y), IM_COL32(255, 255, 255, 255), hud);
#else
		char hud[512];
		sprintf_s(
			hud,
			"FPS %s\n"
			"Scene %s\n"
			"CPU Last %s ms / Avg %s ms / 60FPS %s ms\n"
			"Update %s ms (Input %s / Sound %s / Scene %s)\n"
			"Input %s ms (KBM %s / XI %s / DI %s)\n"
			"Draw %s ms (Begin %s / Scene %s / Overlay %s / Present %s)",
			fpsText,
			GetSceneProfileName(perfLast.scene),
			cpuLastText,
			cpuAvgText,
			budget60Text,
			updateText,
			updateInputText,
			updateSoundText,
			updateSceneText,
			updateInputText,
			inputKbmText,
			inputXiText,
			inputDiText,
			drawText,
			beginDrawText,
			sceneDrawText,
			overlayDrawText,
			endDrawText);
		const char* hudTemplate =
			"FPS 888.88\n"
			"Scene CastleEditor\n"
			"CPU Last 888.88 ms / Avg 888.88 ms / 60FPS 888.88 ms\n"
			"Update 888.88 ms (Input 888.88 / Sound 888.88 / Scene 888.88)\n"
			"Input 888.88 ms (KBM 888.88 / XI 888.88 / DI 888.88)\n"
			"Draw 888.88 ms (Begin 888.88 / Scene 888.88 / Overlay 888.88 / Present 888.88)";
		const ImVec2 textSize = ImGui::CalcTextSize(hudTemplate);
		const ImVec2 boxMin(vp->Pos.x + vp->Size.x - textSize.x - pad.x * 2.0f - 10.0f, vp->Pos.y + 10.0f);
		const ImVec2 boxMax(boxMin.x + textSize.x + pad.x * 2.0f, boxMin.y + textSize.y + pad.y * 2.0f);
		dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 160), 4.0f);
		dl->AddRect(boxMin, boxMax, borderColor, 4.0f);
		dl->AddText(ImVec2(boxMin.x + pad.x, boxMin.y + pad.y), IM_COL32(255, 255, 255, 255), hud);
#endif
	};

#ifdef _DEBUG
	TRAN_INS;
	auto formatRunTime = [](float sec, char* out, size_t outSize)
	{
		float safeSec = sec;
		if (safeSec < 0.0f) safeSec = 0.0f;
		const int totalMin = static_cast<int>(safeSec / 60.0f);
		const float remain = safeSec - static_cast<float>(totalMin) * 60.0f;
		int totalSec = static_cast<int>(remain);
		int centi = static_cast<int>((remain - static_cast<float>(totalSec)) * 100.0f + 0.5f);
		if (centi >= 100)
		{
			centi = 0;
			++totalSec;
		}
		if (totalSec >= 60)
		{
			totalSec -= 60;
		}
		sprintf_s(out, outSize, "%02d:%02d.%02d", totalMin, totalSec, centi);
	};

	// Docking用のルート（上下左右の吸着・分割/再結合）
	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground;
		ImGui::Begin("DockSpaceRoot", nullptr, window_flags);
		ImGui::PopStyleVar(2);
		ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		ImGui::End();
	}

	// ImGuiの描画
	static bool show_main_window = false;

	if(IsKeyTrigger(VK_TAB))show_main_window = !show_main_window;

	using namespace ImGui;
	using namespace std;
	if (show_main_window)
	{
		Begin(u8"メイン設定",&show_main_window);

		string sceneTxt;
		switch (SceneManager::GetCurrent())
		{
		case SceneManager::SceneType::SCENE_TITLE:
			sceneTxt = u8"タイトル";
			break;
		case SceneManager::SceneType::SCENE_GAME:
			sceneTxt = u8"ゲーム";
			break;
		case SceneManager::SceneType::SCENE_RESULT:
			sceneTxt = u8"リザルト";
			break;
		case SceneManager::SceneType::SCENE_ENGINE_EDITOR:
			sceneTxt = u8"城エディタ";
			break;
		case SceneManager::SceneType::SCENE_EFFECT_DEBUG:
			sceneTxt = u8"エフェクト確認";
			break;
		case SceneManager::SceneType::SCENE_BOSS_EDITOR:
			sceneTxt = u8"通常ボス攻撃エディタ";
			break;
		case SceneManager::SceneType::SCENE_FINAL_BOSS_EDITOR:
			sceneTxt = u8"ラスボス攻撃エディタ";
			break;
		case SceneManager::SceneType::SCENE_NEW_BOSS_EDITOR:
			sceneTxt = u8"NewLastBoss";
			break;
		case SceneManager::SceneType::SCENE_NARAKU_EDITOR:
			sceneTxt = u8"奈落塔地形エディタ";
			break;
		case SceneManager::SceneType::SCENE_NARAKU_PIECE_EDITOR:
			sceneTxt = u8"奈落塔小ステージエディタ";
			break;
		case SceneManager::SceneType::SCENE_NARAKU_PROTO:
			sceneTxt = u8"奈落塔プロト";
			break;
		default:
			sceneTxt = u8"不明";
			break;
		}
		sceneTxt = u8"現在シーン: " + sceneTxt;
		ImGui::Text(sceneTxt.c_str());
		static int debugSceneSelection = -1;
		static int debugSceneLastObserved = -1;
		static int debugResultSelection = static_cast<int>(SceneManager::ResultType::Win);
		const int currentSceneValue = static_cast<int>(SceneManager::GetCurrent());
		if (debugSceneSelection < 0 || debugSceneSelection >= static_cast<int>(SceneManager::SceneType::SCENE_MAX))
		{
			debugSceneSelection = currentSceneValue;
		}
		if (debugSceneLastObserved != currentSceneValue && debugSceneSelection == debugSceneLastObserved)
		{
			debugSceneSelection = currentSceneValue;
		}
		debugSceneLastObserved = currentSceneValue;

		const char* debugSceneItems[] =
		{
			u8"タイトル",
			u8"ゲーム",
			u8"リザルト",
			u8"城エディタ",
			u8"エフェクト確認",
			u8"通常ボス攻撃エディタ",
			u8"ラスボス攻撃エディタ",
			u8"NewLastBoss",
			u8"奈落塔地形エディタ",
			u8"奈落塔小ステージエディタ",
			u8"奈落塔プロト",
		};
		const char* debugResultItems[] =
		{
			"None",
			"Win",
			"Lose"
		};
		ImGui::SeparatorText(u8"シーン切り替え");
		ImGui::SetNextItemWidth(220.0f);
		ImGui::Combo(u8"遷移先", &debugSceneSelection, debugSceneItems, IM_ARRAYSIZE(debugSceneItems));
		if (debugSceneSelection == static_cast<int>(SceneManager::SceneType::SCENE_RESULT))
		{
			ImGui::SetNextItemWidth(160.0f);
			ImGui::Combo(u8"リザルト種別", &debugResultSelection, debugResultItems, IM_ARRAYSIZE(debugResultItems));
		}
		if (ImGui::Button(u8"シーンを切り替え"))
		{
			const SceneManager::SceneType targetScene = static_cast<SceneManager::SceneType>(debugSceneSelection);
			if (targetScene == SceneManager::SceneType::SCENE_RESULT)
			{
				SceneManager::ChangeResult(static_cast<SceneManager::ResultType>(debugResultSelection));
			}
			else
			{
				SceneManager::ChangeResult(SceneManager::ResultType::None);
			}
			SceneManager::ChangeScene(targetScene);
		}
		ImGui::Separator();

		if (BeginTabBar("TabBar"))
		{
			if (BeginTabItem(u8"プレイヤー"))
			{
				DragFloat3(u8"位置", reinterpret_cast<float*>(&tran.player.pos), 0.05f);
				DragFloat3(u8"サイズ", reinterpret_cast<float*>(&tran.player.size), 0.05f);
				DragFloat3(u8"速度", reinterpret_cast<float*>(&tran.player.velocity), 0.05f);
				DragFloat(u8"移動速度", &tran.player.moveSpeed, 0.01f, 0.0f, 10.0f);
				DragFloat(u8"回避距離", &tran.player.dashDistance, 0.05f, 0.0f, 10.0f);
				DragFloat(u8"回避CT", &tran.player.dashCooldown, 0.01f, 0.0f, 5.0f);
				DragFloat(u8"回避時間", &tran.player.dashDuration, 0.01f, 0.01f, 1.0f);
				DragFloat(u8"ステージサイズ", &tran.player.stageSize, 0.1f, 1.0f, 20.0f);
				DragFloat(u8"床タイルサイズ", &tran.gameplay.groundTileSize, 0.05f, 0.5f, 10.0f);

				ImGui::SeparatorText(u8"プレイヤー攻撃");
				ImGui::TextDisabled(u8"初期値: 準備0.04 / 有効0.12 / 後隙0.10 / CT0.24 / Skill1CT4.0 / Skill2CT9.0");
				DragFloat(u8"攻撃準備", &tran.gameplay.attackWindup, 0.005f, 0.0f, 1.0f);
				DragFloat(u8"攻撃有効", &tran.gameplay.attackDuration, 0.005f, 0.01f, 1.0f);
				DragFloat(u8"攻撃後隙", &tran.gameplay.attackRecovery, 0.005f, 0.0f, 1.0f);
				DragFloat(u8"攻撃CT", &tran.gameplay.attackCooldown, 0.005f, 0.0f, 2.0f);
				DragFloat(u8"スキル1 CT", &tran.gameplay.skill1Cooldown, 0.05f, 0.0f, 30.0f);
				DragFloat(u8"スキル2 CT", &tran.gameplay.skill2Cooldown, 0.05f, 0.0f, 30.0f);
				ImGui::TextDisabled(u8"初期値: 3体ヒット / 時間0.18 / 強さ0.20");
				DragInt(u8"画面揺れ発火ヒット数", &tran.gameplay.screenShakeHitThreshold, 1.0f, 1, 16);
				DragFloat(u8"画面揺れ時間", &tran.gameplay.screenShakeDuration, 0.01f, 0.0f, 1.0f);
				DragFloat(u8"画面揺れ強さ", &tran.gameplay.screenShakeAmplitude, 0.01f, 0.0f, 1.0f);
				DragFloat(u8"薙ぎ角度", &tran.gameplay.attackSweepDegrees, 1.0f, 10.0f, 240.0f);
				DragFloat(u8"攻撃半径倍率", &tran.gameplay.attackSweepRadiusScale, 0.01f, 0.1f, 3.0f);
				DragFloat(u8"攻撃幅倍率", &tran.gameplay.attackWidthScale, 0.01f, 0.1f, 3.0f);
				DragFloat(u8"攻撃奥行倍率", &tran.gameplay.attackDepthScale, 0.01f, 0.1f, 3.0f);
				DragFloat(u8"プレイヤー攻撃ヒットストップ", &tran.gameplay.attackHitStop, 0.001f, 0.0f, 0.20f);
				DragFloat(u8"ノックバック", &tran.gameplay.attackKnockback, 0.01f, 0.0f, 3.0f);
				DragFloat(u8"ヒット発光", &tran.gameplay.attackHitFlash, 0.005f, 0.0f, 0.50f);
				ImGui::TextDisabled(u8"初期値: 軌跡間隔0.02 / 軌跡残存0.16 / 軌跡倍率0.75");
				DragFloat(u8"攻撃軌跡間隔", &tran.gameplay.attackTrailInterval, 0.002f, 0.0f, 0.20f);
				DragFloat(u8"攻撃軌跡残存", &tran.gameplay.attackTrailLife, 0.005f, 0.0f, 0.50f);
				DragFloat(u8"攻撃軌跡倍率", &tran.gameplay.attackTrailScale, 0.01f, 0.1f, 3.0f);

				ImGui::SeparatorText(u8"進行方向マーカー");
				ImGui::TextDisabled(u8"初期値: 通常0.92 / 被り時0.45");
				DragFloat(u8"マーカー透明度(通常)", &tran.gameplay.directionMarkerAlpha, 0.01f, 0.0f, 1.0f);
				DragFloat(u8"マーカー透明度(被り時)", &tran.gameplay.directionMarkerOverlapAlpha, 0.01f, 0.0f, 1.0f);

				ColorEdit4(u8"色", reinterpret_cast<float*>(&tran.player.color));
				EndTabItem();
			}
			if (BeginTabItem(u8"敵・ボス"))
			{
				ImGui::TextDisabled(u8"※ 難易度プリセット適用時に敵の一部設定は上書きされる");

				ImGui::SeparatorText(u8"敵の基本");
				ImGui::TextDisabled(u8"初期値: 敵数3 / 予兆0.55 / CT1.00 / 射程最小0.80 / 射程倍率1.35 / ダメージ1.0");
				DragInt(u8"敵数(基準)", &tran.gameplay.enemyCount, 1.0f, 0, 16);
				DragFloat(u8"敵予兆時間", &tran.gameplay.enemyAttackWindup, 0.01f, 0.0f, 3.0f);
				DragFloat(u8"敵攻撃CT", &tran.gameplay.enemyAttackCooldown, 0.01f, 0.0f, 3.0f);
				DragFloat(u8"敵射程最小", &tran.gameplay.enemyAttackRangeMin, 0.01f, 0.1f, 5.0f);
				DragFloat(u8"敵射程倍率", &tran.gameplay.enemyAttackRangeScale, 0.01f, 0.1f, 5.0f);
				DragFloat(u8"敵ダメージ", &tran.gameplay.enemyAttackDamage, 0.1f, 0.0f, 20.0f);
				DragFloat(u8"敵移動速度", &tran.gameplay.enemyMoveSpeed, 0.01f, 0.1f, 8.0f);
				DragFloat(u8"Wave毎 速度加算", &tran.gameplay.waveEnemyMoveSpeedAdd, 0.01f, 0.0f, 2.0f);
				DragFloat(u8"Wave毎 ダメ倍率加算", &tran.gameplay.waveEnemyAttackDamageScalePerWave, 0.01f, 0.0f, 3.0f);

				ImGui::SeparatorText(u8"遠距離型の敵弾");
				ImGui::TextDisabled(u8"初期値: 速度6.50 / 寿命1.40 / 半径0.22 / ダメ倍率0.85");
				DragFloat(u8"敵弾速度", &tran.gameplay.enemyProjectileSpeed, 0.05f, 0.1f, 20.0f);
				DragFloat(u8"敵弾寿命", &tran.gameplay.enemyProjectileLife, 0.01f, 0.05f, 5.0f);
				DragFloat(u8"敵弾半径", &tran.gameplay.enemyProjectileRadius, 0.01f, 0.05f, 2.0f);
				DragFloat(u8"敵弾ダメ倍率", &tran.gameplay.enemyProjectileDamageScale, 0.01f, 0.0f, 3.0f);

				ImGui::SeparatorText(u8"敵の分離行動");
				ImGui::TextDisabled(u8"初期値: 半径1.10 / 重み0.80 / 最大オフセット0.80");
				DragFloat(u8"分離半径", &tran.gameplay.enemySeparationRadius, 0.01f, 0.0f, 5.0f);
				DragFloat(u8"分離重み", &tran.gameplay.enemySeparationWeight, 0.01f, 0.0f, 3.0f);
				DragFloat(u8"分離最大オフセット", &tran.gameplay.enemySeparationMaxOffset, 0.01f, 0.0f, 3.0f);

				ImGui::SeparatorText(u8"敵スポーン");
				ImGui::TextDisabled(u8"初期値: リング0.35 / ぶれ0.10 / 対プレイヤー最小1.50 / 対敵最小0.90");
				DragFloat(u8"リング倍率", &tran.gameplay.enemySpawnRingScale, 0.01f, 0.1f, 0.9f);
				DragFloat(u8"ジッタ倍率", &tran.gameplay.enemySpawnJitterScale, 0.01f, 0.0f, 0.5f);
				DragFloat(u8"プレイヤー最小距離", &tran.gameplay.enemySpawnMinPlayerDist, 0.05f, 0.0f, 8.0f);
				DragFloat(u8"敵同士最小距離", &tran.gameplay.enemySpawnMinEnemyDist, 0.05f, 0.0f, 4.0f);

				ImGui::SeparatorText(u8"ボス共通");
				ImGui::TextDisabled(u8"共通予兆倍率は各攻撃の予兆時間に乗算、予兆幅は細突進の幅かつ全攻撃の範囲倍率");
				DragFloat(u8"ボスHPバー横幅(画面比)", &tran.gameplay.bossHpBarWidthRate, 0.005f, 0.20f, 0.90f);
				DragFloat(u8"ボスHPバー縦幅(画面比)", &tran.gameplay.bossHpBarHeightRate, 0.002f, 0.01f, 0.20f);
				ImGui::TextDisabled(u8"Breakゲージ初期値: X=0 / Y=6 / 横幅0.42 / 縦幅0.018");
				DragFloat(u8"Breakゲージ Xオフセット", &tran.gameplay.bossGuardBarOffsetX, 1.0f, -960.0f, 960.0f);
				DragFloat(u8"Breakゲージ Yオフセット", &tran.gameplay.bossGuardBarOffsetY, 1.0f, -120.0f, 320.0f);
				DragFloat(u8"Breakゲージ 横幅(画面比)", &tran.gameplay.bossGuardBarWidthRate, 0.005f, 0.10f, 0.90f);
				DragFloat(u8"Breakゲージ 縦幅(画面比)", &tran.gameplay.bossGuardBarHeightRate, 0.001f, 0.005f, 0.10f);
				DragFloat(u8"ボス面積倍率", &tran.gameplay.bossSizeAreaScale, 0.05f, 4.0f, 12.0f);
				DragInt(u8"ボス最大HP", &tran.gameplay.bossMaxHp, 1.0f, 1, 9999);
				DragFloat(u8"共通予兆倍率", &tran.gameplay.bossAttackTelegraph, 0.01f, 0.10f, 4.0f);
				DragFloat(u8"画面外ジャンプ開始", &tran.gameplay.bossAttackJumpOutTime, 0.01f, 0.0f, 4.0f);
				DragFloat(u8"ボス突進時間", &tran.gameplay.bossAttackDashDuration, 0.005f, 0.05f, 2.0f);
				DragFloat(u8"ボス攻撃CT", &tran.gameplay.bossAttackCooldown, 0.01f, 0.0f, 6.0f);
				DragFloat(u8"予兆幅(プレイヤー比)", &tran.gameplay.bossAttackLanePlayerScale, 0.05f, 0.5f, 8.0f);
				DragFloat(u8"ボス攻撃ダメージ", &tran.gameplay.bossAttackDamage, 0.1f, 0.0f, 200.0f);
				DragFloat(u8"ボス攻撃被弾ヒットストップ", &tran.gameplay.bossAttackHitStop, 0.001f, 0.0f, 0.20f);
				DragFloat(u8"ボス被弾 画面揺れ時間", &tran.gameplay.bossHitShakeDuration, 0.005f, 0.0f, 1.0f);
				DragFloat(u8"ボス被弾 画面揺れ強さ", &tran.gameplay.bossHitShakeAmplitude, 0.01f, 0.0f, 1.0f);
				DragFloat(u8"Breakゲージ 初期最大", &tran.gameplay.bossGuardInitialMax, 0.1f, 1.0f, 200.0f);
				DragFloat(u8"Breakゲージ 最終最大", &tran.gameplay.bossGuardFinalMax, 0.1f, 1.0f, 200.0f);
				DragFloat(u8"Break回復毎 上限上昇量", &tran.gameplay.bossGuardRecoverStep, 0.1f, 0.0f, 50.0f);
				DragFloat(u8"通常時 被ダメ倍率", &tran.gameplay.bossDamageScaleNormal, 0.01f, 0.0f, 5.0f);
				DragFloat(u8"Broken時 被ダメ倍率", &tran.gameplay.bossDamageScaleBroken, 0.01f, 0.0f, 10.0f);
				DragFloat(u8"Broken復帰秒", &tran.gameplay.bossBreakRecoverSec, 0.1f, 1.0f, 30.0f);

				ImGui::SeparatorText(u8"ボス: 突進");
				ImGui::TextDisabled(u8"初期値: 細予兆1.0 / 広予兆2.0 / 広幅0.50");
				DragFloat(u8"細突進 予兆秒", &tran.gameplay.bossDashNarrowTelegraph, 0.01f, 0.10f, 8.0f);
				DragFloat(u8"広突進 予兆秒", &tran.gameplay.bossDashWideTelegraph, 0.01f, 0.10f, 8.0f);
				DragFloat(u8"広突進 幅(ステージ比)", &tran.gameplay.bossDashWideWidthRate, 0.01f, 0.10f, 1.00f);

				ImGui::SeparatorText(u8"ボス: 落下・召喚");
				ImGui::TextDisabled(u8"初期値: ランダム5発 / 半径1.6 / 召喚5〜10 / 追尾5回 / 半径3.0");
				DragInt(u8"ランダム落下 回数", &tran.gameplay.bossRandomRainCount, 1.0f, 1, 16);
				DragFloat(u8"ランダム落下 予兆秒", &tran.gameplay.bossRandomRainTelegraph, 0.01f, 0.10f, 8.0f);
				DragFloat(u8"ランダム落下 半径倍率", &tran.gameplay.bossRandomRainRadiusScale, 0.05f, 0.25f, 8.0f);
				DragInt(u8"従者召喚 最小数", &tran.gameplay.bossSummonMin, 1.0f, 1, 32);
				DragInt(u8"従者召喚 最大数", &tran.gameplay.bossSummonMax, 1.0f, 1, 32);
				DragFloat(u8"従者召喚 予兆秒", &tran.gameplay.bossSummonTelegraph, 0.01f, 0.10f, 8.0f);
				DragInt(u8"追尾落下 回数", &tran.gameplay.bossTrackingDropCount, 1.0f, 1, 16);
				DragFloat(u8"追尾落下 予兆秒", &tran.gameplay.bossTrackingDropTelegraph, 0.01f, 0.10f, 8.0f);
				DragFloat(u8"追尾落下 半径倍率", &tran.gameplay.bossTrackingDropRadiusScale, 0.05f, 0.5f, 8.0f);

				ImGui::SeparatorText(u8"ボス: 必殺技");
				ImGui::TextDisabled(u8"初期値: 交差予兆1.0 / 交差幅1.0 / 踏みつけ5回 / 踏みつけ初回3.0 / 踏みつけ連続3.0 / 全体予兆7.0 / 安置2.0");
				DragFloat(u8"交差攻撃 予兆秒", &tran.gameplay.bossUltimateCrossTelegraph, 0.01f, 0.10f, 8.0f);
				DragFloat(u8"交差攻撃 幅倍率", &tran.gameplay.bossUltimateCrossLaneScale, 0.05f, 0.25f, 8.0f);
				DragInt(u8"踏みつけ 回数", &tran.gameplay.bossUltimateStompCount, 1.0f, 1, 16);
				DragFloat(u8"踏みつけ 初回予兆秒", &tran.gameplay.bossUltimateStompTelegraph, 0.01f, 0.10f, 12.0f);
				DragFloat(u8"踏みつけ 連続予兆秒", &tran.gameplay.bossUltimateStompRepeatTelegraph, 0.01f, 0.10f, 12.0f);
				DragFloat(u8"踏みつけ 半径倍率", &tran.gameplay.bossUltimateStompRadiusScale, 0.05f, 0.5f, 8.0f);
				DragFloat(u8"全体攻撃 予兆秒", &tran.gameplay.bossUltimateFieldTelegraph, 0.01f, 0.10f, 12.0f);
				DragFloat(u8"安置 半径倍率", &tran.gameplay.bossUltimateFieldSafeScale, 0.05f, 0.5f, 8.0f);

				if (tran.gameplay.bossSummonMax < tran.gameplay.bossSummonMin)
				{
					tran.gameplay.bossSummonMax = tran.gameplay.bossSummonMin;
				}

				EndTabItem();
			}
			if (BeginTabItem(u8"ゲーム調整"))
			{
				const char* difficultyText = u8"Normal";
				switch (tran.gameplayDebug.difficultyPreset)
				{
				case 0: difficultyText = u8"Easy"; break;
				case 2: difficultyText = u8"Hard"; break;
				default: difficultyText = u8"Normal"; break;
				}
				auto applyDifficultyPreset = [&](int preset)
				{
					tran.ApplyDifficultyPreset(preset);
				};
				ImGui::SeparatorText(u8"難易度プリセット");
				ImGui::Text(u8"現在: %s", difficultyText);
				if (Button(u8"Easy")) applyDifficultyPreset(0);
				SameLine();
				if (Button(u8"Normal")) applyDifficultyPreset(1);
				SameLine();
				if (Button(u8"Hard")) applyDifficultyPreset(2);

				ImGui::SeparatorText(u8"難易度別 強化幅");
				ImGui::TextDisabled(u8"基準: Easy 1-2 / Normal 2-3 / Hard 3-5");
				DragInt(u8"Easy 小強化", &tran.gameplay.upgradeStepEasySmall, 1.0f, 0, 10);
				DragInt(u8"Easy 大強化", &tran.gameplay.upgradeStepEasyLarge, 1.0f, 0, 10);
				DragInt(u8"Normal 小強化", &tran.gameplay.upgradeStepNormalSmall, 1.0f, 0, 10);
				DragInt(u8"Normal 大強化", &tran.gameplay.upgradeStepNormalLarge, 1.0f, 0, 10);
				DragInt(u8"Hard 小強化", &tran.gameplay.upgradeStepHardSmall, 1.0f, 0, 10);
				DragInt(u8"Hard 大強化", &tran.gameplay.upgradeStepHardLarge, 1.0f, 0, 10);
				if (tran.gameplay.upgradeStepEasySmall < 0) tran.gameplay.upgradeStepEasySmall = 0;
				if (tran.gameplay.upgradeStepEasySmall > 10) tran.gameplay.upgradeStepEasySmall = 10;
				if (tran.gameplay.upgradeStepEasyLarge < tran.gameplay.upgradeStepEasySmall) tran.gameplay.upgradeStepEasyLarge = tran.gameplay.upgradeStepEasySmall;
				if (tran.gameplay.upgradeStepEasyLarge > 10) tran.gameplay.upgradeStepEasyLarge = 10;
				if (tran.gameplay.upgradeStepNormalSmall < 0) tran.gameplay.upgradeStepNormalSmall = 0;
				if (tran.gameplay.upgradeStepNormalSmall > 10) tran.gameplay.upgradeStepNormalSmall = 10;
				if (tran.gameplay.upgradeStepNormalLarge < tran.gameplay.upgradeStepNormalSmall) tran.gameplay.upgradeStepNormalLarge = tran.gameplay.upgradeStepNormalSmall;
				if (tran.gameplay.upgradeStepNormalLarge > 10) tran.gameplay.upgradeStepNormalLarge = 10;
				if (tran.gameplay.upgradeStepHardSmall < 0) tran.gameplay.upgradeStepHardSmall = 0;
				if (tran.gameplay.upgradeStepHardSmall > 10) tran.gameplay.upgradeStepHardSmall = 10;
				if (tran.gameplay.upgradeStepHardLarge < tran.gameplay.upgradeStepHardSmall) tran.gameplay.upgradeStepHardLarge = tran.gameplay.upgradeStepHardSmall;
				if (tran.gameplay.upgradeStepHardLarge > 10) tran.gameplay.upgradeStepHardLarge = 10;
				if (Button(u8"強化幅を基準値に戻す"))
				{
					tran.gameplay.upgradeStepEasySmall = 1;
					tran.gameplay.upgradeStepEasyLarge = 2;
					tran.gameplay.upgradeStepNormalSmall = 2;
					tran.gameplay.upgradeStepNormalLarge = 3;
					tran.gameplay.upgradeStepHardSmall = 3;
					tran.gameplay.upgradeStepHardLarge = 5;
				}

				DragInt(u8"最大Wave", &tran.gameplay.waveMax, 1.0f, 1, 32);
				DragInt(u8"Wave毎の敵追加数", &tran.gameplay.waveEnemyAddPerWave, 1.0f, 0, 16);
				DragInt(u8"進行配布リロール数(現在未使用)", &tran.roguelike.rerollMaxPerStage, 1.0f, 0, 9);
				ImGui::TextDisabled(u8"初期値: 最大Wave3 / Wave追加1");
				ImGui::SeparatorText(u8"開始カメラ演出");
				ImGui::TextDisabled(u8"初期値: 演出時間1.20秒 / フォーカス距離2.80");
				DragFloat(u8"開始演出時間", &tran.gameplay.cameraIntroDuration, 0.02f, 0.10f, 8.0f);
				DragFloat(u8"フォーカス距離", &tran.gameplay.cameraIntroFocusDistance, 0.05f, 0.50f, 12.0f);

				ImGui::SeparatorText(u8"被弾・撃破演出");
				ImGui::TextDisabled(u8"初期値: 被弾0.20 / 被弾無敵0.35 / 被弾倍率1.65 / 撃破0.28 / 撃破倍率1.60");
				DragFloat(u8"被弾フラッシュ時間", &tran.gameplay.playerDamageFlash, 0.005f, 0.0f, 1.0f);
				DragFloat(u8"被弾無敵時間", &tran.gameplay.playerDamageInvincible, 0.005f, 0.0f, 2.0f);
				DragFloat(u8"被弾フラッシュ倍率", &tran.gameplay.playerDamageFlashScale, 0.01f, 0.1f, 4.0f);
				DragFloat(u8"敵撃破フラッシュ時間", &tran.gameplay.enemyDefeatFlash, 0.005f, 0.0f, 1.0f);
				DragFloat(u8"敵撃破フラッシュ倍率", &tran.gameplay.enemyDefeatFlashScale, 0.01f, 0.1f, 4.0f);

				ImGui::SeparatorText(u8"音量");
				ImGui::TextDisabled(u8"初期値: Master1.00 / BGM0.70 / SE1.00");
				DragFloat(u8"Master音量", &tran.gameplay.volumeMaster, 0.01f, 0.0f, 2.0f);
				DragFloat(u8"BGM音量", &tran.gameplay.volumeBgm, 0.01f, 0.0f, 2.0f);
				DragFloat(u8"SE音量", &tran.gameplay.volumeSe, 0.01f, 0.0f, 2.0f);

				ImGui::SeparatorText(u8"押し合い");
				ImGui::TextDisabled(u8"初期値: 余白0.01 / プレイヤー0.55 / 敵0.45");
				DragFloat(u8"押し合い余白", &tran.gameplay.pushSlop, 0.001f, 0.0f, 0.2f);
				DragFloat(u8"プレイヤー側割合", &tran.gameplay.playerPushShare, 0.01f, 0.0f, 1.0f);
				DragFloat(u8"敵側割合", &tran.gameplay.enemyPushShare, 0.01f, 0.0f, 1.0f);

				ImGui::SeparatorText(u8"実行時デバッグ");
				ImGui::Text(u8"攻撃中: %d", tran.gameplayDebug.attackActive);
				ImGui::Text(u8"攻撃SwingID: %d", tran.gameplayDebug.attackSwingId);
				ImGui::Text(u8"このSwingヒット数: %d", tran.gameplayDebug.swingHitCount);
				ImGui::Text(u8"Wave: %d / %d", tran.gameplayDebug.currentWave, tran.gameplayDebug.maxWave);
				ImGui::Text(u8"敵数: %d / %d", tran.gameplayDebug.enemiesAlive, tran.gameplayDebug.enemiesTarget);
				ImGui::Text(u8"難易度: %s", difficultyText);
				ImGui::Text(u8"実効敵数設定: 基準%d / Wave加算%d", tran.gameplayDebug.effectiveEnemyBaseCount, tran.gameplayDebug.effectiveEnemyAddPerWave);
				ImGui::Text(u8"実効敵ダメージ: %.2f", tran.gameplayDebug.effectiveEnemyAttackDamage);
				ImGui::Text(u8"実効攻撃力: %d", tran.gameplayDebug.playerAttackDamage);
				ImGui::Text(u8"攻撃CT倍率: %.2f", tran.gameplayDebug.playerAttackCooldownScale);
				ImGui::Text(u8"回避CT倍率: %.2f", tran.gameplayDebug.playerEvadeCooldownScale);
				ImGui::Text(u8"攻撃CT進捗: %.2f", tran.gameplayDebug.cooldownRateAttack);
				ImGui::Text(u8"回避CT進捗: %.2f", tran.gameplayDebug.cooldownRateEvade);
				ImGui::Text(u8"スキル1CT進捗: %.2f", tran.gameplayDebug.cooldownRateSkill1);
				ImGui::Text(u8"スキル2CT進捗: %.2f", tran.gameplayDebug.cooldownRateSkill2);
				ImGui::Text(
					u8"装備スキル: Q=%s / E=%s",
					GetSkillNameByType(tran.roguelike.skillSlot1),
					GetSkillNameByType(tran.roguelike.skillSlot2));
				ImGui::Text(u8"回避中: %s", tran.gameplayDebug.playerEvading ? u8"はい" : u8"いいえ");
				ImGui::Text(u8"強化選択待ち: %d / リロール所持: %d", tran.gameplayDebug.upgradeSelectionPending, tran.gameplayDebug.upgradeRerollRemain);
				ImGui::Text(u8"タイマー: %.2f sec (記録 %.2f sec / 稼働 %d)", tran.gameplayDebug.runElapsedSec, tran.gameplayDebug.runRecordedSec, tran.gameplayDebug.runTimerRunning);
				ImGui::Text(u8"ボス戦中: %s", tran.gameplayDebug.bossBattleActive ? u8"ON" : u8"OFF");
				ImGui::SeparatorText(u8"HP直接操作");
				DragFloat(u8"プレイヤー現在HP", &tran.player.hp, 0.1f, 0.0f, 9999.0f);
				DragFloat(u8"プレイヤー最大HP", &tran.player.maxHp, 0.1f, 1.0f, 9999.0f);
				if (tran.player.hp < 0.0f) tran.player.hp = 0.0f;
				if (tran.player.maxHp < 1.0f) tran.player.maxHp = 1.0f;
				if (tran.player.hp > tran.player.maxHp) tran.player.hp = tran.player.maxHp;
				if (tran.gameplayDebug.bossBattleActive != 0 && tran.gameplayDebug.bossMaxHp > 0.0f)
				{
					int bossHpEdit = static_cast<int>(tran.gameplayDebug.bossHp);
					int bossMaxHpEdit = static_cast<int>(tran.gameplayDebug.bossMaxHp);
					bool bossHpEdited = false;
					bossHpEdited |= ImGui::DragInt(u8"ボス現在HP", &bossHpEdit, 1.0f, 0, 9999);
					bossHpEdited |= ImGui::DragInt(u8"ボス最大HP(実体)", &bossMaxHpEdit, 1.0f, 1, 9999);
					if (bossHpEdit < 0) bossHpEdit = 0;
					if (bossMaxHpEdit < 1) bossMaxHpEdit = 1;
					if (bossHpEdit > bossMaxHpEdit) bossHpEdit = bossMaxHpEdit;
					if (bossHpEdited)
					{
						tran.gameplayDebug.bossHpEditValue = bossHpEdit;
						tran.gameplayDebug.bossMaxHpEditValue = bossMaxHpEdit;
						tran.gameplayDebug.bossHpEditRequest = 1;
					}
				}
				else
				{
					ImGui::TextDisabled(u8"ボスHP編集はボス戦中のみ有効");
				}
				ImGui::TextDisabled(u8"ゲーム進行: 敵全滅で次Wave、最終Wave全滅で勝利");
				ImGui::TextDisabled(u8"敵タイプ差: 遠距離型は予兆後に敵弾を発射");
				ImGui::TextDisabled(u8"敵AABB色: 赤=被弾 / 橙=予兆 / 黄=攻撃可能");
				ImGui::TextDisabled(u8"敵AABB色: 水=射程内(CT中) / 緑=射程外");
				ImGui::TextDisabled(u8"射程枠(デバッグカメラ): 水=射程内 / 青=射程外");
				ImGui::SeparatorText(u8"設定保存");
				ImGui::TextDisabled("%s", tran.GetGameplayTuningPath());
				if (Button(u8"設定を保存"))
				{
					tran.SaveGameplayTuning();
				}
				SameLine();
				if (Button(u8"設定を読込"))
				{
					tran.LoadGameplayTuning();
				}

				if (Button(u8"ゲーム調整を初期値に戻す"))
				{
					tran.ResetGameplayTuningToDefault();
					tran.gameplayDebug.difficultyPreset = 1;
					tran.gameplayDebug.titleDifficultySelection = 1;
				}
				SameLine();
				if (Button(u8"強化状態をリセット"))
				{
					tran.ResetRoguelikeUpgrade();
				}

				EndTabItem();
			}
			if (BeginTabItem(u8"挑戦"))
			{
				auto beginDebugChallenge = [&](int challengeType)
				{
					tran.gameplayDebug.requestChallengeType = challengeType;
					tran.gameplayDebug.challengeReturnToGameAfterReward = 1;
					tran.gameplayDebug.challengeSuccess = 0;
					tran.gameplayDebug.challengeRewardCount = 0;
					SceneManager::ChangeResult(SceneManager::ResultType::None);
					if (SceneManager::GetCurrent() != SceneManager::SCENE_GAME)
					{
						SceneManager::ChangeScene(SceneManager::SCENE_GAME);
					}
				};

				ImGui::TextDisabled(u8"現状はデバッグ起動のみです。マップ分岐への接続は後で行います。");
				if (Button(u8"ノーダメ挑戦を開始"))
				{
					beginDebugChallenge(1);
				}
				SameLine();
				if (Button(u8"防衛挑戦を開始"))
				{
					beginDebugChallenge(2);
				}

				const char* challengeTypeText = u8"なし";
				switch (tran.gameplayDebug.challengeType)
				{
				case 1: challengeTypeText = u8"ノーダメ"; break;
				case 2: challengeTypeText = u8"防衛"; break;
				default: break;
				}
				ImGui::SeparatorText(u8"実行状態");
				ImGui::Text(u8"現在: %s / Active=%s", challengeTypeText, tran.gameplayDebug.challengeActive ? u8"ON" : u8"OFF");
				ImGui::Text(u8"経過: %.2f / %.2f", tran.gameplayDebug.challengeElapsedSec, tran.gameplayDebug.challengeDurationSec);
				ImGui::Text(u8"現在の報酬見込み: %d", tran.gameplayDebug.challengeRewardCount);
				ImGui::Text(u8"直近の結果: %s", tran.gameplayDebug.challengeSuccess ? u8"成功" : u8"未達成/失敗");
				ImGui::Text(u8"被弾回数: %d", tran.gameplayDebug.challengeHitCount);
				ImGui::Text(u8"ビーコンHP: %.1f / %.1f", tran.gameplayDebug.challengeBeaconHp, tran.gameplayDebug.challengeBeaconMaxHp);

				ImGui::SeparatorText(u8"ノーダメ挑戦");
				ImGui::TextDisabled(u8"攻撃は予兆後に発生し、被弾した時点で挑戦失敗として報酬画面へ移行します。");
				DragFloat(u8"制限時間##challenge_no_damage_duration", &tran.gameplay.challengeNoDamageDuration, 0.1f, 5.0f, 120.0f);
				DragFloat(u8"攻撃間隔##challenge_no_damage_interval", &tran.gameplay.challengeNoDamageAttackInterval, 0.01f, 0.20f, 10.0f);
				DragFloat(u8"予兆時間##challenge_no_damage_telegraph", &tran.gameplay.challengeNoDamageTelegraph, 0.01f, 0.10f, 8.0f);
				DragFloat(u8"攻撃半径##challenge_no_damage_radius", &tran.gameplay.challengeNoDamageRadius, 0.01f, 0.20f, 8.0f);
				DragFloat(u8"被弾ダメージ##challenge_no_damage_damage", &tran.gameplay.challengeNoDamageDamage, 0.1f, 0.0f, 20.0f);
				DragInt(u8"同時攻撃数##challenge_no_damage_burst", &tran.gameplay.challengeNoDamageBurstCount, 1.0f, 1, 8);

				ImGui::SeparatorText(u8"防衛挑戦");
				ImGui::TextDisabled(u8"中心ビーコンを守るモードです。敵は常にビーコンへ向かい、到達するとHPを削ります。");
				DragFloat(u8"制限時間##challenge_defense_duration", &tran.gameplay.challengeDefenseDuration, 0.1f, 5.0f, 180.0f);
				DragFloat(u8"ビーコン最大HP##challenge_defense_beacon_hp", &tran.gameplay.challengeDefenseBeaconMaxHp, 0.5f, 1.0f, 500.0f);
				DragFloat(u8"ビーコン半径##challenge_defense_beacon_radius", &tran.gameplay.challengeDefenseBeaconRadius, 0.01f, 0.30f, 6.0f);
				DragFloat(u8"到達ダメージ##challenge_defense_contact_damage", &tran.gameplay.challengeDefenseBeaconContactDamage, 0.1f, 0.1f, 100.0f);
				DragFloat(u8"敵スポーン間隔##challenge_defense_spawn_interval", &tran.gameplay.challengeDefenseSpawnInterval, 0.01f, 0.20f, 15.0f);
				DragFloat(u8"敵移動速度##challenge_defense_speed", &tran.gameplay.challengeDefenseEnemyMoveSpeed, 0.01f, 0.05f, 4.0f);
				DragFloat(u8"高速型HP加算率##challenge_defense_speed_hp", &tran.gameplay.challengeDefenseSpeedHpBonusRate, 0.01f, 0.0f, 5.0f);
				DragFloat(u8"遠距離型HP加算率##challenge_defense_ranged_hp", &tran.gameplay.challengeDefenseRangedHpBonusRate, 0.01f, 0.0f, 5.0f);
				DragFloat(u8"重量型HP加算率##challenge_defense_tank_hp", &tran.gameplay.challengeDefenseTankHpBonusRate, 0.01f, 0.0f, 5.0f);
				DragInt(u8"同時敵数上限##challenge_defense_cap", &tran.gameplay.challengeDefenseEnemyCap, 1.0f, 1, 16);

				EndTabItem();
			}
			if (BeginTabItem(u8"強化状態"))
			{
				char lastUpgradeText[64]{};
				if (tran.gameplayDebug.lastUpgradeType >= 0)
				{
					FormatUpgradeLabel(tran, tran.gameplayDebug.lastUpgradeType, lastUpgradeText, sizeof(lastUpgradeText));
				}
				else
				{
					sprintf_s(lastUpgradeText, sizeof(lastUpgradeText), "%s", u8"なし");
				}

				ImGui::Text(u8"ステージクリア回数: %d", tran.gameplayDebug.stageClearCount);
				ImGui::Text(u8"直近の強化: %s", lastUpgradeText);
				ImGui::SeparatorText(u8"強化レベル");
				ImGui::Text(u8"上限: Lv.%d", tran.GetUpgradeLevelMax());
				ImGui::Text(u8"攻撃力 Lv.%d", tran.gameplayDebug.attackPowerLevel);
				ImGui::Text(u8"攻撃頻度 Lv.%d", tran.gameplayDebug.attackSpeedLevel);
				ImGui::Text(u8"回避CT Lv.%d", tran.gameplayDebug.evadeCooldownLevel);
				ImGui::SeparatorText(u8"強化レベル編集");
				ImGui::TextDisabled(u8"ゲーム中でも直接Lvを増減できます");
				const int maxUpgradeLevel = tran.GetUpgradeLevelMax();
				int attackPowerLevelEdit = tran.roguelike.attackPowerLevel;
				int attackSpeedLevelEdit = tran.roguelike.attackSpeedLevel;
				int evadeCooldownLevelEdit = tran.roguelike.evadeCooldownLevel;
				bool upgradeLevelEdited = false;
				upgradeLevelEdited |= ImGui::DragInt(u8"攻撃力Lv##edit", &attackPowerLevelEdit, 1.0f, 0, maxUpgradeLevel);
				upgradeLevelEdited |= ImGui::DragInt(u8"攻撃頻度Lv##edit", &attackSpeedLevelEdit, 1.0f, 0, maxUpgradeLevel);
				upgradeLevelEdited |= ImGui::DragInt(u8"回避CTLv##edit", &evadeCooldownLevelEdit, 1.0f, 0, maxUpgradeLevel);
				if (Button(u8"攻撃力+1##upgrade"))
				{
					++attackPowerLevelEdit;
					upgradeLevelEdited = true;
				}
				SameLine();
				if (Button(u8"攻撃力-1##upgrade"))
				{
					--attackPowerLevelEdit;
					upgradeLevelEdited = true;
				}
				if (Button(u8"攻撃頻度+1##upgrade"))
				{
					++attackSpeedLevelEdit;
					upgradeLevelEdited = true;
				}
				SameLine();
				if (Button(u8"攻撃頻度-1##upgrade"))
				{
					--attackSpeedLevelEdit;
					upgradeLevelEdited = true;
				}
				if (Button(u8"回避CT+1##upgrade"))
				{
					++evadeCooldownLevelEdit;
					upgradeLevelEdited = true;
				}
				SameLine();
				if (Button(u8"回避CT-1##upgrade"))
				{
					--evadeCooldownLevelEdit;
					upgradeLevelEdited = true;
				}
				if (upgradeLevelEdited)
				{
					tran.roguelike.attackPowerLevel = tran.ClampUpgradeLevel(attackPowerLevelEdit);
					tran.roguelike.attackSpeedLevel = tran.ClampUpgradeLevel(attackSpeedLevelEdit);
					tran.roguelike.evadeCooldownLevel = tran.ClampUpgradeLevel(evadeCooldownLevelEdit);
					tran.gameplayDebug.attackPowerLevel = tran.roguelike.attackPowerLevel;
					tran.gameplayDebug.attackSpeedLevel = tran.roguelike.attackSpeedLevel;
					tran.gameplayDebug.evadeCooldownLevel = tran.roguelike.evadeCooldownLevel;
					tran.gameplayDebug.playerAttackDamage = tran.GetPlayerAttackDamageByLevel(tran.roguelike.attackPowerLevel);
					tran.gameplayDebug.playerAttackCooldownScale = tran.GetAttackCooldownScaleByLevel(tran.roguelike.attackSpeedLevel);
					tran.gameplayDebug.playerEvadeCooldownScale = tran.GetEvadeCooldownScaleByLevel(tran.roguelike.evadeCooldownLevel);
				}
				ImGui::SeparatorText(u8"現在の実効値");
				ImGui::Text(u8"攻撃ダメージ: %d", tran.gameplayDebug.playerAttackDamage);
				ImGui::Text(u8"攻撃CT倍率: %.2f", tran.gameplayDebug.playerAttackCooldownScale);
				ImGui::Text(u8"回避CT倍率: %.2f", tran.gameplayDebug.playerEvadeCooldownScale);
				ImGui::SeparatorText(u8"スキル強化の現在値");
				ImGui::Text(
					u8"遠距離: 範囲Lv.%d=%.2f倍 / 威力Lv.%d=%.2f倍 / CTLv.%d=-%.2f秒",
					tran.roguelike.skillShotRangeLevel,
					tran.GetSkillRangeScaleByLevel(tran.roguelike.skillShotRangeLevel),
					tran.roguelike.skillShotPowerLevel,
					tran.GetSkillDamageScaleByLevel(tran.roguelike.skillShotPowerLevel),
					tran.roguelike.skillShotCooldownLevel,
					tran.GetSkillCooldownReductionByLevel(tran.roguelike.skillShotCooldownLevel));
				ImGui::Text(
					u8"近接: 範囲Lv.%d=%.2f倍 / 威力Lv.%d=%.2f倍 / CTLv.%d=-%.2f秒",
					tran.roguelike.skillNovaRangeLevel,
					tran.GetSkillRangeScaleByLevel(tran.roguelike.skillNovaRangeLevel),
					tran.roguelike.skillNovaPowerLevel,
					tran.GetSkillDamageScaleByLevel(tran.roguelike.skillNovaPowerLevel),
					tran.roguelike.skillNovaCooldownLevel,
					tran.GetSkillCooldownReductionByLevel(tran.roguelike.skillNovaCooldownLevel));
				ImGui::Text(
					u8"衛星: 範囲Lv.%d=%.2f倍 / CTLv.%d=-%.2f秒 / 生成Lv.%d=%d個 / 威力%.2f倍",
					tran.roguelike.skillOrbitRangeLevel,
					tran.GetSkillRangeScaleByLevel(tran.roguelike.skillOrbitRangeLevel),
					tran.roguelike.skillOrbitCooldownLevel,
					tran.GetSkillCooldownReductionByLevel(tran.roguelike.skillOrbitCooldownLevel),
					tran.roguelike.skillOrbitCountLevel,
					tran.GetOrbitCountByLevel(tran.roguelike.skillOrbitCountLevel),
					tran.GetOrbitDamageScaleByCountLevel(tran.roguelike.skillOrbitCountLevel));
				ImGui::SeparatorText(u8"所持スキルのLv調整(Debug)");
				const int skillLevelMax = tran.GetSkillUpgradeLevelMax();
				bool skillUpgradeEdited = false;
				if (tran.roguelike.skillSlot1 == Transfer::RoguelikeUpgrade::SkillShot ||
					tran.roguelike.skillSlot2 == Transfer::RoguelikeUpgrade::SkillShot)
				{
					ImGui::TextDisabled(u8"遠距離スキル");
					skillUpgradeEdited |= DragInt(u8"遠距離 範囲Lv", &tran.roguelike.skillShotRangeLevel, 1.0f, 0, skillLevelMax);
					skillUpgradeEdited |= DragInt(u8"遠距離 威力Lv", &tran.roguelike.skillShotPowerLevel, 1.0f, 0, skillLevelMax);
					skillUpgradeEdited |= DragInt(u8"遠距離 CTLv", &tran.roguelike.skillShotCooldownLevel, 1.0f, 0, skillLevelMax);
				}
				if (tran.roguelike.skillSlot1 == Transfer::RoguelikeUpgrade::SkillNova ||
					tran.roguelike.skillSlot2 == Transfer::RoguelikeUpgrade::SkillNova)
				{
					ImGui::TextDisabled(u8"近接スキル");
					skillUpgradeEdited |= DragInt(u8"近接 範囲Lv", &tran.roguelike.skillNovaRangeLevel, 1.0f, 0, skillLevelMax);
					skillUpgradeEdited |= DragInt(u8"近接 威力Lv", &tran.roguelike.skillNovaPowerLevel, 1.0f, 0, skillLevelMax);
					skillUpgradeEdited |= DragInt(u8"近接 CTLv", &tran.roguelike.skillNovaCooldownLevel, 1.0f, 0, skillLevelMax);
				}
				if (tran.roguelike.skillSlot1 == Transfer::RoguelikeUpgrade::SkillOrbit ||
					tran.roguelike.skillSlot2 == Transfer::RoguelikeUpgrade::SkillOrbit)
				{
					ImGui::TextDisabled(u8"衛星スキル");
					skillUpgradeEdited |= DragInt(u8"衛星 範囲Lv", &tran.roguelike.skillOrbitRangeLevel, 1.0f, 0, skillLevelMax);
					skillUpgradeEdited |= DragInt(u8"衛星 CTLv", &tran.roguelike.skillOrbitCooldownLevel, 1.0f, 0, skillLevelMax);
					skillUpgradeEdited |= DragInt(u8"衛星 生成Lv", &tran.roguelike.skillOrbitCountLevel, 1.0f, 0, skillLevelMax);
				}
				if (tran.roguelike.skillSlot1 == Transfer::RoguelikeUpgrade::SkillNone &&
					tran.roguelike.skillSlot2 == Transfer::RoguelikeUpgrade::SkillNone)
				{
					ImGui::TextDisabled(u8"所持中のスキルがないため、ここでは調整できません");
				}
				if (skillUpgradeEdited)
				{
					tran.roguelike.skillShotRangeLevel = tran.ClampUpgradeLevel(tran.roguelike.skillShotRangeLevel);
					tran.roguelike.skillShotPowerLevel = tran.ClampUpgradeLevel(tran.roguelike.skillShotPowerLevel);
					tran.roguelike.skillShotCooldownLevel = tran.ClampUpgradeLevel(tran.roguelike.skillShotCooldownLevel);
					tran.roguelike.skillNovaRangeLevel = tran.ClampUpgradeLevel(tran.roguelike.skillNovaRangeLevel);
					tran.roguelike.skillNovaPowerLevel = tran.ClampUpgradeLevel(tran.roguelike.skillNovaPowerLevel);
					tran.roguelike.skillNovaCooldownLevel = tran.ClampUpgradeLevel(tran.roguelike.skillNovaCooldownLevel);
					tran.roguelike.skillOrbitRangeLevel = tran.ClampUpgradeLevel(tran.roguelike.skillOrbitRangeLevel);
					tran.roguelike.skillOrbitCooldownLevel = tran.ClampUpgradeLevel(tran.roguelike.skillOrbitCooldownLevel);
					tran.roguelike.skillOrbitCountLevel = tran.ClampUpgradeLevel(tran.roguelike.skillOrbitCountLevel);
				}
				ImGui::Text(u8"強化選択待ち: %s", tran.roguelike.selectionPending ? u8"あり" : u8"なし");
				ImGui::Text(u8"リロール残り: %d", tran.roguelike.rerollRemain);
				ImGui::Text(u8"復活残り: %d / %d", tran.roguelike.reviveRemain, tran.roguelike.reviveMax);
				ImGui::SeparatorText(u8"新仕様データ");
				ImGui::Text(u8"武器: %s", GetWeaponTypeLabel(tran.roguelike.loadoutWeaponType));
				ImGui::Text(u8"スキル1: %s", GetActionSkillLabel(tran.roguelike.loadoutSkills[0]));
				ImGui::Text(u8"スキル2: %s", GetActionSkillLabel(tran.roguelike.loadoutSkills[1]));
				ImGui::Text(u8"通常ボス順: 1:%s / 2:%s / 3:%s",
					GetBossArchetypeLabel(tran.roguelike.regularBossOrder[0]),
					GetBossArchetypeLabel(tran.roguelike.regularBossOrder[1]),
					GetBossArchetypeLabel(tran.roguelike.regularBossOrder[2]));
				ImGui::Text(u8"最終ボス: %s", GetBossArchetypeLabel(tran.roguelike.finalBossType));
				for (int traitType = 0; traitType < Transfer::RoguelikeUpgrade::TraitTypeCount; ++traitType)
				{
					ImGui::Text(u8"%s特性 Lv: %d", GetTraitLabel(traitType), tran.roguelike.traitLevels[traitType]);
				}
				{
					char disabledTagText[256]{};
					bool hasDisabledTag = false;
					for (int tagType = 0; tagType < Transfer::RoguelikeUpgrade::TagTypeCount; ++tagType)
					{
						if (tran.roguelike.disabledTags[tagType] == 0) continue;
						if (hasDisabledTag)
						{
							strcat_s(disabledTagText, sizeof(disabledTagText), u8", ");
						}
						strcat_s(disabledTagText, sizeof(disabledTagText), GetTraitLabel(tagType));
						hasDisabledTag = true;
					}
					ImGui::Text(u8"解除済みタグ: %s", hasDisabledTag ? disabledTagText : u8"なし");
				}
				{
					char artifactText[256]{};
					bool hasArtifact = false;
					for (int artifactType = 0; artifactType < Transfer::RoguelikeUpgrade::ArtifactTypeCount; ++artifactType)
					{
						if (tran.roguelike.ownedArtifacts[artifactType] == 0) continue;
						if (hasArtifact)
						{
							strcat_s(artifactText, sizeof(artifactText), u8", ");
						}
						strcat_s(artifactText, sizeof(artifactText), GetArtifactLabel(artifactType));
						hasArtifact = true;
					}
					ImGui::Text(u8"所持魔道具: %s", hasArtifact ? artifactText : u8"なし");
				}
				ImGui::SeparatorText(u8"現在の候補");
				char offerText0[64]{}, offerText1[64]{}, offerText2[64]{};
				FormatUpgradeLabel(tran, tran.roguelike.offers[0], offerText0, sizeof(offerText0));
				FormatUpgradeLabel(tran, tran.roguelike.offers[1], offerText1, sizeof(offerText1));
				FormatUpgradeLabel(tran, tran.roguelike.offers[2], offerText2, sizeof(offerText2));
				ImGui::Text(u8"[1] %s", offerText0);
				ImGui::Text(u8"[2] %s", offerText1);
				ImGui::Text(u8"[3] %s", offerText2);
				ImGui::TextDisabled(u8"勝利時に3候補から1つ選択（Rでリロール、回数上限あり）");
				if (Button(u8"強化状態をリセット##upgrade_tab"))
				{
					tran.ResetRoguelikeUpgrade();
				}
				EndTabItem();
			}
			if (BeginTabItem(u8"UI"))
			{
				ImGui::SeparatorText(u8"ポーズメニューUI");
				ImGui::TextDisabled(u8"Escで開くゲーム内メニューの表示倍率");
				DragFloat(u8"UI全体サイズ", &tran.gameplayDebug.pauseMenuUiScale, 0.01f, 0.5f, 2.5f);
				DragFloat(u8"メニュー文字サイズ", &tran.gameplayDebug.pauseMenuFontScale, 0.01f, 0.5f, 2.5f);
				DragFloat(u8"ボタンサイズ", &tran.gameplayDebug.pauseMenuButtonScale, 0.01f, 0.5f, 2.5f);
				if (Button(u8"UI設定を初期値に戻す"))
				{
					tran.gameplayDebug.pauseMenuUiScale = 1.0f;
					tran.gameplayDebug.pauseMenuFontScale = 1.0f;
					tran.gameplayDebug.pauseMenuButtonScale = 1.0f;
				}
				EndTabItem();
			}
			if (BeginTabItem(u8"モデル編集"))
			{
				ImGui::SeparatorText(u8"腕");
				if(TreeNode(u8"腕パーツ"))
				{
					PushID(0);
					if(TreeNode(u8"右腕"))
					{
						ImGui::DragFloat3(u8"右腕1 角度", reinterpret_cast<float*>(&tran.modelediter.armRight1.subAngle), 0.1f);
						ImGui::DragFloat3(u8"右腕1 位置", reinterpret_cast<float*>(&tran.modelediter.armRight1.pos), 0.1f);
						ImGui::DragFloat3(u8"右腕1 サイズ", reinterpret_cast<float*>(&tran.modelediter.armRight1.size), 0.1f);
						ImGui::DragFloat3(u8"右腕1 回転", reinterpret_cast<float*>(&tran.modelediter.armRight1.rotate), 0.1f);
						ImGui::SeparatorText(u8"右腕2");
						ImGui::DragFloat3(u8"右腕2 角度", reinterpret_cast<float*>(&tran.modelediter.armRight2.subAngle), 0.1f);
						ImGui::DragFloat3(u8"右腕2 位置", reinterpret_cast<float*>(&tran.modelediter.armRight2.pos), 0.1f);
						ImGui::DragFloat3(u8"右腕2 サイズ", reinterpret_cast<float*>(&tran.modelediter.armRight2.size), 0.1f);
						ImGui::DragFloat3(u8"右腕2 回転", reinterpret_cast<float*>(&tran.modelediter.armRight2.rotate), 0.1f);
						TreePop();
					}
					if(TreeNode(u8"左腕"))
					{
						ImGui::SeparatorText(u8"左腕1");
						ImGui::DragFloat3(u8"左腕1 角度", reinterpret_cast<float*>(&tran.modelediter.armLeft1.subAngle), 0.1f);
						ImGui::DragFloat3(u8"左腕1 位置", reinterpret_cast<float*>(&tran.modelediter.armLeft1.pos), 0.1f);
						ImGui::DragFloat3(u8"左腕1 サイズ", reinterpret_cast<float*>(&tran.modelediter.armLeft1.size), 0.1f);
						ImGui::DragFloat3(u8"左腕1 回転", reinterpret_cast<float*>(&tran.modelediter.armLeft1.rotate), 0.1f);
						ImGui::SeparatorText(u8"左腕2");
						ImGui::DragFloat3(u8"左腕2 角度", reinterpret_cast<float*>(&tran.modelediter.armLeft2.subAngle), 0.1f);
						ImGui::DragFloat3(u8"左腕2 位置", reinterpret_cast<float*>(&tran.modelediter.armLeft2.pos), 0.1f);
						ImGui::DragFloat3(u8"左腕2 サイズ", reinterpret_cast<float*>(&tran.modelediter.armLeft2.size), 0.1f);
						ImGui::DragFloat3(u8"左腕2 回転", reinterpret_cast<float*>(&tran.modelediter.armLeft2.rotate), 0.1f);
						TreePop();
					}
					TreePop();
					PopID();
				}
				ImGui::SeparatorText(u8"脚");
				if(TreeNode(u8"脚パーツ"))
				{
					PushID(1);
					if(TreeNode(u8"右脚"))
					{ 
						ImGui::DragFloat3(u8"右脚1 角度", reinterpret_cast<float*>(&tran.modelediter.legRight1.subAngle), 0.1f);
						ImGui::DragFloat3(u8"右脚1 位置", reinterpret_cast<float*>(&tran.modelediter.legRight1.pos), 0.1f);
						ImGui::DragFloat3(u8"右脚1 サイズ", reinterpret_cast<float*>(&tran.modelediter.legRight1.size), 0.1f);
						ImGui::DragFloat3(u8"右脚1 回転", reinterpret_cast<float*>(&tran.modelediter.legRight1.rotate), 0.1f);
						ImGui::SeparatorText(u8"右脚2");
						ImGui::DragFloat3(u8"右脚2 角度", reinterpret_cast<float*>(&tran.modelediter.legRight2.subAngle), 0.1f);
						ImGui::DragFloat3(u8"右脚2 位置", reinterpret_cast<float*>(&tran.modelediter.legRight2.pos), 0.1f);
						ImGui::DragFloat3(u8"右脚2 サイズ", reinterpret_cast<float*>(&tran.modelediter.legRight2.size), 0.1f);
						ImGui::DragFloat3(u8"右脚2 回転", reinterpret_cast<float*>(&tran.modelediter.legRight2.rotate), 0.1f);
						TreePop();
					}
					if(TreeNode(u8"左脚"))
					{ 
						ImGui::SeparatorText(u8"左脚1");
						ImGui::DragFloat3(u8"左脚1 角度", reinterpret_cast<float*>(&tran.modelediter.legLeft1.subAngle), 0.1f);
						ImGui::DragFloat3(u8"左脚1 位置", reinterpret_cast<float*>(&tran.modelediter.legLeft1.pos), 0.1f);
						ImGui::DragFloat3(u8"左脚1 サイズ", reinterpret_cast<float*>(&tran.modelediter.legLeft1.size), 0.1f);
						ImGui::DragFloat3(u8"左脚1 回転", reinterpret_cast<float*>(&tran.modelediter.legLeft1.rotate), 0.1f);
						ImGui::SeparatorText(u8"左脚2");
						ImGui::DragFloat3(u8"左脚2 角度", reinterpret_cast<float*>(&tran.modelediter.legLeft2.subAngle), 0.1f);
						ImGui::DragFloat3(u8"左脚2 位置", reinterpret_cast<float*>(&tran.modelediter.legLeft2.pos), 0.1f);
						ImGui::DragFloat3(u8"左脚2 サイズ", reinterpret_cast<float*>(&tran.modelediter.legLeft2.size), 0.1f);
						ImGui::DragFloat3(u8"左脚2 回転", reinterpret_cast<float*>(&tran.modelediter.legLeft2.rotate), 0.1f);
						TreePop();
					}
					TreePop();
					PopID();
				}

				ImGui::SeparatorText(u8"胴体");
				ImGui::DragFloat3(u8"胴体 位置", reinterpret_cast<float*>(&tran.modelediter.body.pos), 0.1f);
				ImGui::DragFloat3(u8"胴体 サイズ", reinterpret_cast<float*>(&tran.modelediter.body.size), 0.1f);
				ImGui::DragFloat3(u8"胴体 回転", reinterpret_cast<float*>(&tran.modelediter.body.angle), 0.1f);
				ImGui::DragFloat3(u8"右腕ジョイント", reinterpret_cast<float*>(&tran.modelediter.body.jointRightArmPos),0.1f);
				ImGui::DragFloat3(u8"左腕ジョイント", reinterpret_cast<float*>(&tran.modelediter.body.jointLeftArmPos),0.1f);
				ImGui::DragFloat3(u8"右脚ジョイント", reinterpret_cast<float*>(&tran.modelediter.body.jointRightLegPos),0.1f);
				ImGui::DragFloat3(u8"左脚ジョイント", reinterpret_cast<float*>(&tran.modelediter.body.jointLeftLegPos),0.1f);

				EndTabItem();
			}
			if (BeginTabItem(u8"カメラ"))
            {

                static float min = 20.0f;
                static float max = 80.0f;

                DragFloatRange2(u8"仮レンジ", &min, &max, 0.1f, 0.0f, 100.0f);

                const char* cameraModes[] = { u8"ゲーム", u8"デバッグ" };
                int mode = tran.cameraMode;
                if (Combo(u8"カメラモード", &mode, cameraModes, IM_ARRAYSIZE(cameraModes)))
                {
                    tran.cameraMode = mode;
                }

                const char* activeLabel = (tran.cameraMode == 1) ? u8"デバッグ" : u8"ゲーム";
                Text(u8"現在: %s", activeLabel);

                SeparatorText(u8"ゲームカメラ");
                float gameEye[3] = { tran.cameraGame.eye.x, tran.cameraGame.eye.y, tran.cameraGame.eye.z };
                float gameLook[3] = { tran.cameraGame.look.x, tran.cameraGame.look.y, tran.cameraGame.look.z };
                if (DragFloat3(u8"ゲーム Eye", gameEye, 0.05f))
                {
                    tran.cameraGame.eye = { gameEye[0], gameEye[1], gameEye[2] };
                }
                if (DragFloat3(u8"ゲーム Look", gameLook, 0.05f))
                {
                    tran.cameraGame.look = { gameLook[0], gameLook[1], gameLook[2] };
                }

                SeparatorText(u8"デバッグカメラ");
                float debugEye[3] = { tran.cameraDebug.eye.x, tran.cameraDebug.eye.y, tran.cameraDebug.eye.z };
                float debugLook[3] = { tran.cameraDebug.look.x, tran.cameraDebug.look.y, tran.cameraDebug.look.z };
                if (DragFloat3(u8"デバッグ Eye", debugEye, 0.05f))
                {
                    tran.cameraDebug.eye = { debugEye[0], debugEye[1], debugEye[2] };
                }
                if (DragFloat3(u8"デバッグ Look", debugLook, 0.05f))
                {
                    tran.cameraDebug.look = { debugLook[0], debugLook[1], debugLook[2] };
                }

                EndTabItem();
            }
			if (BeginTabItem(u8"グラフ"))
			{
				static float easing = 1;

				DragFloat(u8"Exp減衰", &easing,0.1f);
				Separator();

				float target = 1000;
				float current = 10;
				float frame[100];
				for (int i = 0; i < 100; i++)
				{
					current = current + (target - current) * expf(-easing);
					frame[i] = current;
				}

				PlotLines(u8"expf", frame, 100, 0, 0, 3.4028235E38F, 3.4028235E38F, { 100,100 }, 4);
				Separator();
				current = 0;
				float PlanA[100];
				for (int i = 0; i < 100; i++)
				{
					current += 0.01f;
					PlanA[i] = current * current * current * current;
				}

				PlotLines(u8"プランA",PlanA,100, 0, 0, 3.4028235E38F, 3.4028235E38F, { 100,100 }, 4);
				Separator();
				float c4 = (2 * PI) / 3;

				current = 0;
				float  PlanB[100];
				for (int i = 0; i < 100; i++)
				{
					current += 0.01f;
					PlanB[i] = current == 0.0f ? 0.0f : current == 1.0f ? 1.0f : powf(2, -10 * current) * sinf((current * 10 - 0.75f) * c4) + 1.0f;
				}
				PlotLines(u8"プランB", PlanB, 100, 0, 0, 3.4028235E38F, 3.4028235E38F, { 100,100 }, 4);

				Separator();

				current = 0;

				float n1 = 7.5625;
				float d1 = 2.75;
				float PlanC[100];
				for(int i = 0; i < 100;i++)
				{
					current += 0.01f;
					PlanC[i] = 1.0f - EaseOutBounce(current);
				}

				PlotLines(u8"プランC", PlanC, 100, 0, 0, 3.4028235E38F, 3.4028235E38F, { 100,100 }, 4);

				EndTabItem();
			}
			if (BeginTabItem(u8"シーン切替"))
			{
				static int sceneNum = 0;
				if (ArrowButton("上", ImGuiDir_Up))
					sceneNum++;
				SameLine();
				if (ArrowButton("下", ImGuiDir_Down))
					sceneNum--;
				if (sceneNum < 0)sceneNum = static_cast<int>(SceneManager::SceneType::SCENE_MAX) - 1;
				sceneNum = sceneNum % static_cast<int>(SceneManager::SceneType::SCENE_MAX);
				SceneManager::SceneType changeScene = SceneManager::GetCurrent();
				switch (sceneNum)
				{
				case 0:
					sceneTxt = u8"タイトル";
					changeScene = SceneManager::SceneType::SCENE_TITLE;
					break;
				case 1:
					sceneTxt = u8"ゲーム";
					changeScene = SceneManager::SceneType::SCENE_GAME;
					break;
				case 2:
					sceneTxt = u8"リザルト";
					changeScene = SceneManager::SceneType::SCENE_RESULT;
					break;
				case 3:
					sceneTxt = u8"城エディタ";
					changeScene = SceneManager::SceneType::SCENE_ENGINE_EDITOR;
					break;
				case 4:
					sceneTxt = u8"エフェクト確認";
					changeScene = SceneManager::SceneType::SCENE_EFFECT_DEBUG;
					break;
				case 5:
					sceneTxt = u8"通常ボス攻撃エディタ";
					changeScene = SceneManager::SceneType::SCENE_BOSS_EDITOR;
					break;
				case 6:
					sceneTxt = u8"ラスボス攻撃エディタ";
					changeScene = SceneManager::SceneType::SCENE_FINAL_BOSS_EDITOR;
					break;
				case 7:
					sceneTxt = u8"NewLastBoss";
					changeScene = SceneManager::SceneType::SCENE_NEW_BOSS_EDITOR;
					break;
				case 8:
					sceneTxt = u8"奈落塔地形エディタ";
					changeScene = SceneManager::SceneType::SCENE_NARAKU_EDITOR;
					break;
				case 9:
					sceneTxt = u8"奈落塔小ステージエディタ";
					changeScene = SceneManager::SceneType::SCENE_NARAKU_PIECE_EDITOR;
					break;
				case 10:
					sceneTxt = u8"奈落塔プロト";
					changeScene = SceneManager::SceneType::SCENE_NARAKU_PROTO;
					break;
				default:
					sceneTxt = u8"不明";
					break;
				}
				sceneTxt = u8"変更先: " + sceneTxt;
				SameLine();
				Text(sceneTxt.c_str());
				if (Button(u8"適用"))
					SceneManager::ChangeScene(changeScene);

				EndTabItem();
			}
			if (BeginTabItem(u8"マウス"))
			{
				std::string mouseInfoText;

				ImGuiIO& io = ImGui::GetIO();
				tran.mousePos.x = ImGui::GetCursorScreenPos().x;
				tran.mousePos.y = ImGui::GetCursorScreenPos().y;
				mouseInfoText = std::to_string(tran.mousePos.x) + " : " + std::to_string(tran.mousePos.y);
				ImGui::Text(mouseInfoText.c_str());

				if (ImGui::IsMousePosValid())
					ImGui::Text(u8"マウス座標: (%g, %g)", io.MousePos.x, io.MousePos.y);

				ImGui::SeparatorText(u8"DirectInput");
				ImGui::Text(u8"接続: %s", IsDirectInputConnected() ? u8"あり" : u8"なし");
				if (IsDirectInputConnected())
				{
					const LONG axisX = GetDirectInputAxisX();
					const LONG axisY = GetDirectInputAxisY();
					const DWORD pov = GetDirectInputPov();
					ImGui::Text("Axis X: %ld", axisX);
					ImGui::Text("Axis Y: %ld", axisY);
					if (LOWORD(pov) == 0xFFFF)
					{
						ImGui::TextUnformatted(u8"POV: Neutral");
					}
					else
					{
						ImGui::Text("POV: %lu", static_cast<unsigned long>(pov));
					}

					std::string pressedButtons;
					for (int buttonIndex = 0; buttonIndex < 128; ++buttonIndex)
					{
						if (!IsDirectInputButtonPressed(buttonIndex))
						{
							continue;
						}

						if (!pressedButtons.empty())
						{
							pressedButtons += ", ";
						}
						pressedButtons += std::to_string(buttonIndex);
					}
					if (pressedButtons.empty())
					{
						pressedButtons = "none";
					}

					ImGui::TextWrapped(u8"押下中ボタン: %s", pressedButtons.c_str());
					ImGui::Text(
						u8"Start候補  Btn6:%s  Btn7:%s  Btn8:%s  Btn9:%s",
						IsDirectInputButtonPressed(6) ? "ON" : "--",
						IsDirectInputButtonPressed(7) ? "ON" : "--",
						IsDirectInputButtonPressed(8) ? "ON" : "--",
						IsDirectInputButtonPressed(9) ? "ON" : "--");

					if (ImGui::BeginTable("##directinput_buttons", 4,
						ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchSame))
					{
						for (int buttonIndex = 0; buttonIndex < 16; ++buttonIndex)
						{
							if ((buttonIndex % 4) == 0)
							{
								ImGui::TableNextRow();
							}
							ImGui::TableSetColumnIndex(buttonIndex % 4);
							ImGui::Text(
								"Btn%02d : %s",
								buttonIndex,
								IsDirectInputButtonPressed(buttonIndex) ? "ON" : "--");
						}
						ImGui::EndTable();
					}
				}
				EndTabItem();
			}
			EndTabBar();
		}
		Separator();
		Text(u8"FPS: %.1f", GetIO().Framerate);
		End();
	}	// -----------------------------

	DrawEngineEditorWindows();

	// Debug Tools : Tables + DrawList
	//   - Tables : 一覧/監視用
	//   - DrawList : 画面上への簡易オーバーレイ
	// -----------------------------
	static bool show_table_window = false;

	// TABキーのメインウィンドウだけだと隠れやすいので、ここで簡易トグルも用意
	if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) show_table_window = !show_table_window;

	if (show_table_window)
	{
		ImGui::Begin(u8"インスペクタ（表）", &show_table_window);

		ImGui::Text(u8"F1:表  F2:オーバーレイ");
		ImGui::Separator();

		// 1) Key-Value 監視テーブル（縦スクロール）
		ImGuiTableFlags flags =
			ImGuiTableFlags_Borders |
			ImGuiTableFlags_RowBg |
			ImGuiTableFlags_Resizable |
			ImGuiTableFlags_SizingStretchProp |
			ImGuiTableFlags_ScrollY;

		const float table_h = 220.0f;
		if (ImGui::BeginTable("##kv", 2, flags, ImVec2(0.0f, table_h)))
		{
			ImGui::TableSetupColumn(u8"項目", ImGuiTableColumnFlags_WidthFixed, 180.0f);
			ImGui::TableSetupColumn(u8"値", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableHeadersRow();

			auto row_f3 = [](const char* name, const DirectX::XMFLOAT3& v)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1); ImGui::Text("(%.2f, %.2f, %.2f)", v.x, v.y, v.z);
				};
			auto row_f4 = [](const char* name, const DirectX::XMFLOAT4& v)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1); ImGui::Text("(%.2f, %.2f, %.2f, %.2f)", v.x, v.y, v.z, v.w);
				};
			auto row_f1 = [](const char* name, float v)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%.3f", v);
				};
			auto row_i1 = [](const char* name, int v)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1); ImGui::Text("%d", v);
				};
			auto row_s1 = [](const char* name, const char* v)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(name);
					ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(v ? v : "-");
				};
			// Transfer の中身を例として監視
			row_f3(u8"プレイヤー位置", tran.player.pos);
			row_f3(u8"プレイヤー速度", tran.player.velocity);
			row_f1(u8"プレイヤーHP", tran.player.hp);
			row_f1(u8"プレイヤー最大HP", tran.player.maxHp);

			row_i1(u8"敵存在", tran.enemy.exists);
			row_f3(u8"敵位置", tran.enemy.pos);
			row_f1(u8"敵HP", tran.enemy.hp);
			row_f1(u8"敵最大HP", tran.enemy.maxHp);
			const char* enemyStateText = u8"なし";
			switch (tran.enemy.state)
			{
			case 0: enemyStateText = u8"徘徊"; break;
			case 1: enemyStateText = u8"追跡"; break;
			default: enemyStateText = u8"なし"; break;
			}
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"敵状態");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.enemy.state, enemyStateText);
			const char* enemyTypeText = u8"なし";
			switch (tran.enemy.type)
			{
			case 0: enemyTypeText = u8"速度型"; break;
			case 1: enemyTypeText = u8"耐久型"; break;
			case 2: enemyTypeText = u8"遠距離型"; break;
			default: enemyTypeText = u8"なし"; break;
			}
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"敵タイプ");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.enemy.type, enemyTypeText);


			row_f3(u8"サイコロ位置", tran.dice.pos);
			row_f3(u8"サイコロ速度", tran.dice.velocity);
			row_f4(u8"サイコロ回転", tran.dice.rot);
			row_f1(u8"停止しきい値速度", tran.dice.underVel);

			const char* cameraModeText = (tran.cameraMode == 1) ? u8"デバッグ" : u8"ゲーム";
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"カメラモード");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.cameraMode, cameraModeText);
			row_i1(u8"現在Wave", tran.gameplayDebug.currentWave);
			row_i1(u8"最大Wave", tran.gameplayDebug.maxWave);
			row_i1(u8"生存敵数", tran.gameplayDebug.enemiesAlive);
			row_i1(u8"目標敵数", tran.gameplayDebug.enemiesTarget);
			const char* difficultyPresetText = u8"ノーマル";
			switch (tran.gameplayDebug.difficultyPreset)
			{
			case 0: difficultyPresetText = u8"イージー"; break;
			case 2: difficultyPresetText = u8"ハード"; break;
			default: difficultyPresetText = u8"ノーマル"; break;
			}
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"難易度プリセット");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.gameplayDebug.difficultyPreset, difficultyPresetText);
			row_i1(u8"実効基準敵数", tran.gameplayDebug.effectiveEnemyBaseCount);
			row_i1(u8"実効Wave追加敵数", tran.gameplayDebug.effectiveEnemyAddPerWave);
			row_f1(u8"実効敵攻撃ダメージ", tran.gameplayDebug.effectiveEnemyAttackDamage);
			row_i1(u8"実効プレイヤー攻撃力", tran.gameplayDebug.playerAttackDamage);
			row_f1(u8"実効プレイヤー攻撃CT倍率", tran.gameplayDebug.playerAttackCooldownScale);
			row_f1(u8"実効プレイヤー回避CT倍率", tran.gameplayDebug.playerEvadeCooldownScale);
			row_i1(u8"プレイヤー回避中", tran.gameplayDebug.playerEvading);
			row_i1(u8"ステージクリア回数", tran.gameplayDebug.stageClearCount);
			row_i1(u8"強化:攻撃力Lv", tran.gameplayDebug.attackPowerLevel);
			row_i1(u8"強化:攻撃頻度Lv", tran.gameplayDebug.attackSpeedLevel);
			row_i1(u8"強化:回避CTLv", tran.gameplayDebug.evadeCooldownLevel);
			row_i1(u8"強化選択待ち", tran.gameplayDebug.upgradeSelectionPending);
			row_i1(u8"リロール所持", tran.gameplayDebug.upgradeRerollRemain);
			row_f1(u8"タイマー経過秒", tran.gameplayDebug.runElapsedSec);
			row_f1(u8"タイマー記録秒", tran.gameplayDebug.runRecordedSec);
			row_i1(u8"タイマー稼働中", tran.gameplayDebug.runTimerRunning);
			row_i1(u8"ボス戦中", tran.gameplayDebug.bossBattleActive);
			row_f1(u8"ボスHP", tran.gameplayDebug.bossHp);
			row_f1(u8"ボス最大HP", tran.gameplayDebug.bossMaxHp);
			row_s1(u8"カーソル対象", tran.gameplayDebug.cursorHoverTarget);
			char debugOffer0[64]{}, debugOffer1[64]{}, debugOffer2[64]{};
			FormatUpgradeLabel(tran, tran.gameplayDebug.upgradeOffer0, debugOffer0, sizeof(debugOffer0));
			FormatUpgradeLabel(tran, tran.gameplayDebug.upgradeOffer1, debugOffer1, sizeof(debugOffer1));
			FormatUpgradeLabel(tran, tran.gameplayDebug.upgradeOffer2, debugOffer2, sizeof(debugOffer2));
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"強化候補1");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.gameplayDebug.upgradeOffer0, debugOffer0);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"強化候補2");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.gameplayDebug.upgradeOffer1, debugOffer1);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(u8"強化候補3");
			ImGui::TableSetColumnIndex(1); ImGui::Text("%d (%s)", tran.gameplayDebug.upgradeOffer2, debugOffer2);

			ImGui::EndTable();
		}

		ImGui::Spacing();

		if (ImGui::CollapsingHeader(u8"ステータス強化（表）", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled(u8"左2列固定。幅が足りない場合は横スクロール");
			DrawStatusUpgradeValueTable(tran);
		}
		if (ImGui::CollapsingHeader(u8"スキル強化（表）", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextDisabled(u8"左2列固定。列は Lv1 から Lv10 の強化段階");
			DrawSkillUpgradeValueTable(tran);
		}

		ImGui::End();
	}

	if (SceneManager::GetCurrent() == SceneManager::SceneType::SCENE_GAME)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
		if (tran.gameplayDebug.bossBattleActive != 0 && tran.gameplayDebug.bossMaxHp > 0.0f)
		{
			float widthRate = tran.gameplay.bossHpBarWidthRate;
			if (widthRate < 0.20f) widthRate = 0.20f;
			if (widthRate > 0.90f) widthRate = 0.90f;
			float heightRate = tran.gameplay.bossHpBarHeightRate;
			if (heightRate < 0.01f) heightRate = 0.01f;
			if (heightRate > 0.20f) heightRate = 0.20f;
			float guardWidthRate = tran.gameplay.bossGuardBarWidthRate;
			if (guardWidthRate < 0.10f) guardWidthRate = 0.10f;
			if (guardWidthRate > 0.90f) guardWidthRate = 0.90f;
			float guardHeightRate = tran.gameplay.bossGuardBarHeightRate;
			if (guardHeightRate < 0.005f) guardHeightRate = 0.005f;
			if (guardHeightRate > 0.10f) guardHeightRate = 0.10f;

			float hpRate = tran.gameplayDebug.bossHp / tran.gameplayDebug.bossMaxHp;
			if (hpRate < 0.0f) hpRate = 0.0f;
			if (hpRate > 1.0f) hpRate = 1.0f;
			float guardRate = 0.0f;
			if (tran.gameplayDebug.bossGuardMax > 0.0f)
			{
				guardRate = tran.gameplayDebug.bossGuard / tran.gameplayDebug.bossGuardMax;
			}
			if (guardRate < 0.0f) guardRate = 0.0f;
			if (guardRate > 1.0f) guardRate = 1.0f;

			const float barW = vp->Size.x * widthRate;
			const float barH = vp->Size.y * heightRate;
			const float x = vp->Pos.x + (vp->Size.x - barW) * 0.5f;
			const float y = vp->Pos.y + 12.0f;
			const float padding = 4.0f;
			const float radius = 6.0f;

			const ImVec2 frameMin(x, y);
			const ImVec2 frameMax(x + barW, y + barH);
			dl->AddRectFilled(frameMin, frameMax, IM_COL32(20, 20, 20, 220), radius);
			dl->AddRect(frameMin, frameMax, IM_COL32(230, 230, 230, 210), radius, 0, 2.0f);

			const float innerW = (barW - padding * 2.0f) * hpRate;
			if (innerW > 0.0f)
			{
				dl->AddRectFilled(
					ImVec2(x + padding, y + padding),
					ImVec2(x + padding + innerW, y + barH - padding),
					IM_COL32(180, 30, 30, 220),
					radius * 0.6f);
			}
			dl->AddText(ImVec2(x + 8.0f, y - 18.0f), IM_COL32(255, 255, 255, 230), "BOSS");

			const float guardBarW = vp->Size.x * guardWidthRate;
			const float guardBarH = (vp->Size.y * guardHeightRate < 6.0f) ? 6.0f : (vp->Size.y * guardHeightRate);
			const float guardX = vp->Pos.x + (vp->Size.x - guardBarW) * 0.5f + tran.gameplay.bossGuardBarOffsetX;
			const float guardY = vp->Pos.y + 12.0f + barH + tran.gameplay.bossGuardBarOffsetY;
			dl->AddRectFilled(ImVec2(guardX, guardY), ImVec2(guardX + guardBarW, guardY + guardBarH), IM_COL32(18, 18, 18, 210), radius * 0.45f);
			dl->AddRect(ImVec2(guardX, guardY), ImVec2(guardX + guardBarW, guardY + guardBarH), IM_COL32(210, 210, 210, 180), radius * 0.45f, 0, 1.5f);

			const float guardInnerW = (guardBarW - padding * 2.0f) * guardRate;
			if (guardInnerW > 0.0f)
			{
				const ImU32 guardColor = (tran.gameplayDebug.bossBroken != 0)
					? IM_COL32(130, 130, 130, 220)
					: IM_COL32(50, 175, 255, 220);
				dl->AddRectFilled(
					ImVec2(guardX + padding, guardY + padding * 0.35f),
					ImVec2(guardX + padding + guardInnerW, guardY + guardBarH - padding * 0.35f),
					guardColor,
					radius * 0.3f);
			}
			dl->AddText(ImVec2(guardX + 8.0f, guardY - 16.0f), IM_COL32(180, 225, 255, 220), "GUARD");
			if (tran.gameplayDebug.bossBroken != 0)
			{
				dl->AddText(ImVec2(guardX + guardBarW - 78.0f, guardY - 16.0f), IM_COL32(255, 220, 120, 230), "BROKEN");
			}
		}
	}
	if (SceneManager::GetCurrent() == SceneManager::SceneType::SCENE_RESULT &&
		!(SceneManager::GetResultType() == SceneManager::ResultType::Win &&
		  (tran.roguelike.selectionPending != 0 ||
		   tran.roguelike.intermissionMode != Transfer::RoguelikeUpgrade::IntermissionNone)))
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
		const char* resultGuide =
			u8"リザルト選択\n"
			u8"左: Restart (ゲームへ) / 右: Title (タイトルへ)\n"
			u8"移動: A D / W S / ← → / ↑ ↓\n"
			u8"決定: Enter / F / Space / Controller Confirm";

		const ImVec2 pad(10.0f, 8.0f);
		const ImVec2 textSize = ImGui::CalcTextSize(resultGuide);
		const ImVec2 boxMin(vp->Pos.x + vp->Size.x - textSize.x - pad.x * 2.0f - 16.0f,
							vp->Pos.y + vp->Size.y - textSize.y - pad.y * 2.0f - 16.0f);
		const ImVec2 boxMax(boxMin.x + textSize.x + pad.x * 2.0f, boxMin.y + textSize.y + pad.y * 2.0f);

		dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 170), 8.0f);
		dl->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 120), 8.0f);
		dl->AddText(ImVec2(boxMin.x + pad.x, boxMin.y + pad.y), IM_COL32(255, 255, 255, 255), resultGuide);
	}
	if (SceneManager::GetCurrent() == SceneManager::SceneType::SCENE_RESULT &&
		tran.gameplayDebug.showBossResultTimer != 0)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
		char timeText[32]{};
		formatRunTime(tran.gameplayDebug.runRecordedSec, timeText, sizeof(timeText));
		const char* timerLabel = u8"記録タイム";
		const char* timerState = u8"TIMER STOP";
		ImFont* font = ImGui::GetFont();
		const float labelSize = ImGui::GetFontSize() * 1.15f;
		const float timeSize = ImGui::GetFontSize() * 3.10f;
		const float stateSize = ImGui::GetFontSize() * 1.00f;
		const ImVec2 labelTextSize = font->CalcTextSizeA(labelSize, 10000.0f, 0.0f, timerLabel);
		const ImVec2 timeTextSize = font->CalcTextSizeA(timeSize, 10000.0f, 0.0f, timeText);
		const ImVec2 stateTextSize = font->CalcTextSizeA(stateSize, 10000.0f, 0.0f, timerState);
		const float contentWidth = (labelTextSize.x > timeTextSize.x)
			? ((labelTextSize.x > stateTextSize.x) ? labelTextSize.x : stateTextSize.x)
			: ((timeTextSize.x > stateTextSize.x) ? timeTextSize.x : stateTextSize.x);
		const float gap = 10.0f;
		const float padX = 28.0f;
		const float padY = 18.0f;
		const float contentHeight = labelTextSize.y + gap + timeTextSize.y + gap + stateTextSize.y;
		const float boxWidth = contentWidth + padX * 2.0f;
		const float boxHeight = contentHeight + padY * 2.0f;
		const ImVec2 boxMin(vp->Pos.x + (vp->Size.x - boxWidth) * 0.5f, vp->Pos.y + 52.0f);
		const ImVec2 boxMax(boxMin.x + boxWidth, boxMin.y + boxHeight);

		dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 190), 10.0f);
		dl->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 180), 10.0f);
		float y = boxMin.y + padY;
		dl->AddText(font, labelSize, ImVec2(boxMin.x + (boxWidth - labelTextSize.x) * 0.5f, y), IM_COL32(230, 230, 230, 255), timerLabel);
		y += labelTextSize.y + gap;
		dl->AddText(font, timeSize, ImVec2(boxMin.x + (boxWidth - timeTextSize.x) * 0.5f, y), IM_COL32(255, 240, 120, 255), timeText);
		y += timeTextSize.y + gap;
		dl->AddText(font, stateSize, ImVec2(boxMin.x + (boxWidth - stateTextSize.x) * 0.5f, y), IM_COL32(210, 210, 210, 255), timerState);
	}
	if (SceneManager::GetCurrent() == SceneManager::SceneType::SCENE_TITLE &&
		tran.gameplayDebug.titleOptionOpen == 0 &&
		tran.gameplayDebug.titleKeyConfigOpen == 0 &&
		tran.gameplayDebug.titleDifficultyOpen == 0 &&
		tran.gameplayDebug.titlePreparationOpen == 0)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
		const char* titleGuide =
			u8"タイトル選択\n"
			u8"Start / Option / Exit\n"
			u8"移動: W S A D / ↑ ↓ ← →\n"
			u8"決定: Enter / F / Space / Controller Confirm\n"
			u8"Esc: 終了";

		const ImVec2 pad(10.0f, 8.0f);
		const ImVec2 textSize = ImGui::CalcTextSize(titleGuide);
		const ImVec2 boxMin(vp->Pos.x + vp->Size.x - textSize.x - pad.x * 2.0f - 16.0f,
							vp->Pos.y + vp->Size.y - textSize.y - pad.y * 2.0f - 16.0f);
		const ImVec2 boxMax(boxMin.x + textSize.x + pad.x * 2.0f, boxMin.y + textSize.y + pad.y * 2.0f);

		dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 170), 8.0f);
		dl->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 120), 8.0f);
		dl->AddText(ImVec2(boxMin.x + pad.x, boxMin.y + pad.y), IM_COL32(255, 255, 255, 255), titleGuide);
	}
	if (!IsEngineEditorScene(SceneManager::GetCurrent()) &&
		SceneManager::GetCurrent() != SceneManager::SceneType::SCENE_NARAKU_PROTO &&
		SceneManager::GetCurrent() != SceneManager::SceneType::SCENE_NARAKU_EDITOR &&
		SceneManager::GetCurrent() != SceneManager::SceneType::SCENE_NARAKU_PIECE_EDITOR)
	{
		// 軸線の表示
		// グリッド
		DirectX::XMFLOAT4 lineColor(0.5f, 0.5f, 0.5f, 1.0f);
		float size = DEBUG_GRID_NUM * DEBUG_GRID_MARGIN;
		for (int i = 1; i <= DEBUG_GRID_NUM; ++i)
		{
			float grid = i * DEBUG_GRID_MARGIN;
			DirectX::XMFLOAT3 pos[2] = {
				DirectX::XMFLOAT3(grid, 0.0f, size),
				DirectX::XMFLOAT3(grid, 0.0f,-size),
			};
			Geometory::AddLine(pos[0], pos[1], lineColor);
			pos[0].x = pos[1].x = -grid;
			Geometory::AddLine(pos[0], pos[1], lineColor);
			pos[0].x = size;
			pos[1].x = -size;
			pos[0].z = pos[1].z = grid;
			Geometory::AddLine(pos[0], pos[1], lineColor);
			pos[0].z = pos[1].z = -grid;
			Geometory::AddLine(pos[0], pos[1], lineColor);
		}
		// 軸
		Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(size,0,0), DirectX::XMFLOAT4(1,0,0,1));
		Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(0,size,0), DirectX::XMFLOAT4(0,1,0,1));
		Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(0,0,size), DirectX::XMFLOAT4(0,0,1,1));
		Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(-size,0,0),  DirectX::XMFLOAT4(0,0,0,1));
		Geometory::AddLine(DirectX::XMFLOAT3(0,0,0), DirectX::XMFLOAT3(0,0,-size),  DirectX::XMFLOAT4(0,0,0,1));

		Geometory::DrawLines();

		// カメラの値
		static bool camAutoSwitch = false;
		static bool camUpDownSwitch = true;
		static float camAutoRotate = 1.0f;
		if (IsKeyTrigger(VK_RETURN) && false) {
			camAutoSwitch ^= true;
		}
		if (IsKeyTrigger(VK_SPACE)) {
			camUpDownSwitch ^= true;
		}

		DirectX::XMVECTOR camPos;
		if (camAutoSwitch) {
			camAutoRotate += 0.01f;
		}
		camPos = DirectX::XMVectorSet(
			cosf(camAutoRotate) * 5.0f,
			3.5f * (camUpDownSwitch ? 1.0f : -1.0f),
			sinf(camAutoRotate) * 5.0f,
			0.0f);

		// ジオメトリ用カメラ初期化
		DirectX::XMFLOAT4X4 mat[2];
		DirectX::XMStoreFloat4x4(&mat[0], DirectX::XMMatrixTranspose(
			DirectX::XMMatrixLookAtLH(
				camPos,
				DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
				DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
			)));
		DirectX::XMStoreFloat4x4(&mat[1], DirectX::XMMatrixTranspose(
			DirectX::XMMatrixPerspectiveFovLH(
				DirectX::XMConvertToRadians(60.0f), (float)SCREEN_WIDTH / SCREEN_HEIGHT, 0.1f, 1000.0f)
		));
		Geometory::SetView(mat[0]);
		Geometory::SetProjection(mat[1]);
	}
#endif

	if (IsEngineEditorScene(SceneManager::GetCurrent()))
	{
		sectionStart = NowMS();
		DrawSceneToEngineEditorRenderTarget();
		g_perfFrameWorking.sceneDrawMs = static_cast<float>(NowMS() - sectionStart);
	}
	else
	{
		sectionStart = NowMS();
		SceneManager::Draw();
		g_perfFrameWorking.sceneDrawMs = static_cast<float>(NowMS() - sectionStart);
	}
	sectionStart = NowMS();
	{
		TRAN_INS;
		const bool showBossDebugHint = false;
		DrawUpgradeSelectionOverlay(tran, showBossDebugHint);
		DrawTitleOptionOverlay(tran);
		DrawTitleKeyConfigWindow(tran);
		drawPauseMenuOverlay(tran);
		drawOverlayHud(tran);
	}
#ifndef _DEBUG
	{
		TRAN_INS;
		if (SceneManager::GetCurrent() == SceneManager::SceneType::SCENE_TITLE &&
			tran.gameplayDebug.titleOptionOpen == 0 &&
			tran.gameplayDebug.titleKeyConfigOpen == 0 &&
			tran.gameplayDebug.titleDifficultyOpen == 0 &&
			tran.gameplayDebug.titlePreparationOpen == 0)
		{
			ImGuiViewport* vp = ImGui::GetMainViewport();
			ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
			const char* titleGuide =
				u8"タイトル選択\n"
				u8"Start / Option / Exit\n"
				u8"移動: W S A D / ↑ ↓ ← →\n"
				u8"決定: Enter / F / Space / Controller Confirm\n"
				u8"Esc: 終了";

			const ImVec2 pad(10.0f, 8.0f);
			const ImVec2 textSize = ImGui::CalcTextSize(titleGuide);
			const ImVec2 boxMin(vp->Pos.x + vp->Size.x - textSize.x - pad.x * 2.0f - 16.0f,
								vp->Pos.y + vp->Size.y - textSize.y - pad.y * 2.0f - 16.0f);
			const ImVec2 boxMax(boxMin.x + textSize.x + pad.x * 2.0f, boxMin.y + textSize.y + pad.y * 2.0f);

			dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 170), 8.0f);
			dl->AddRect(boxMin, boxMax, IM_COL32(255, 255, 255, 120), 8.0f);
			dl->AddText(ImVec2(boxMin.x + pad.x, boxMin.y + pad.y), IM_COL32(255, 255, 255, 255), titleGuide);
		}
	}
#endif
	g_perfFrameWorking.overlayDrawMs = static_cast<float>(NowMS() - sectionStart);
	sectionStart = NowMS();
	EndDrawDirectX();
	g_perfFrameWorking.endDrawMs = static_cast<float>(NowMS() - sectionStart);
	g_perfFrameWorking.drawTotalMs = static_cast<float>(NowMS() - drawStart);
	g_perfFrameWorking.frameCpuMs = g_perfFrameWorking.updateTotalMs + g_perfFrameWorking.drawTotalMs;
	g_perfFrameWorking.scene = SceneManager::GetCurrent();
	CommitPerfFrame(g_perfFrameWorking);
}

// EOF

