#pragma once

#include <cstddef>

#include "SceneManager.h"

class Transfer;

struct OverlayScaleMetrics
{
	float uiScale;
	float fontScale;
	float buttonScale;
};

const char* GetSkillNameByType(int skillType);
const char* GetDifficultyName(int difficultyPreset);
void FormatUpgradeLabel(const Transfer& tran, int upgradeType, char* out, size_t outSize);
void DrawStatusUpgradeValueTable(const Transfer& tran);
void DrawSkillUpgradeValueTable(const Transfer& tran);
void DrawUpgradeSelectionOverlay(const Transfer& tran, bool showBossDebugHint);
OverlayScaleMetrics GetAdaptiveOverlayScaleMetrics(const Transfer& tran, bool isFullscreen);
void DrawTitleOptionOverlay(Transfer& tran);
void DrawTitleKeyConfigWindow(Transfer& tran);
bool IsEngineEditorScene(SceneManager::SceneType sceneType);
void ReleaseEngineEditorRenderTargets();
void DrawEngineEditorWindows();
void DrawSceneToEngineEditorRenderTarget();
