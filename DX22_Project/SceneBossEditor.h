#pragma once

#include "BossAttackScript.h"
#include "Scene.h"
#include <string>
#include <vector>

class CameraDebug;
class Texture;

class SceneBossEditor : public Scene
{
public:
	enum EditorMode
	{
		ModeRegularBosses = 0,
		ModeFinalBoss
	};

	explicit SceneBossEditor(EditorMode mode = ModeRegularBosses);
	~SceneBossEditor() override;

	void Update() override;
	void Draw() override;

public:
	struct PreviewZone
	{
		DirectX::XMFLOAT3 center = { 0.0f, 0.05f, 0.0f };
		DirectX::XMFLOAT3 size = { 1.0f, 0.10f, 1.0f };
		float yawDeg = 0.0f;
		int shape = BossAttackScript::ColliderShapeBox;
		DirectX::XMFLOAT3 startPos = { 0.0f, 0.05f, 0.0f };
		DirectX::XMFLOAT3 endPos = { 0.0f, 0.05f, 0.0f };
		bool hasPath = false;
	};

	struct PreviewVisual
	{
		DirectX::XMFLOAT3 pos = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT2 size = { 1.0f, 1.0f };
		float timer = 0.0f;
		float duration = 0.35f;
		float spawnHeight = 2.5f;
		float spinDegPerSec = 0.0f;
		float angleDeg = 0.0f;
		float yawDeg = 0.0f;
		bool billboard = true;
		Texture* texture = nullptr;
	};

	enum PreviewState
	{
		PreviewIdle = 0,
		PreviewTelegraph,
		PreviewActive,
		PreviewCooldown
	};

private:
	void ClampSelection();
	void NormalizeDatabase();
	void UpdatePreviewPlayer(float dt);
	void UpdatePreviewPlayback(float dt);
	void ResetPreview(bool resetPlayer);
	void BeginPreviewAttack(int attackIndex);
	void AdvancePreviewAttack();
	void BuildPreviewZones(const BossAttackScript::Attack& attack,
		std::vector<PreviewZone>& outZones,
		DirectX::XMFLOAT3& outBossTargetPos,
		float& outFacingYawDeg);
	void UpdatePreviewHit();
	void UpdatePreviewVisuals(float dt);
	void DrawReferenceFloor() const;
	void DrawPreviewScene() const;
	void DrawDebugWindow();
	Texture* GetTextureForPath(const std::string& relativePath);
	BossAttackScript::Profile* GetSelectedProfile();
	const BossAttackScript::Profile* GetSelectedProfile() const;
	bool IsFinalBossEditor() const;
	const char* GetEditorPath() const;
	BossAttackScript::Database MakeEditorDefaultDatabase() const;

private:
	EditorMode m_editorMode;
	CameraDebug* m_pCamera;
	Texture* m_pFloorTexture;
	Texture* m_pMarkerTexture;
	Texture* m_pAttackRangeTexture;
	Texture* m_pBossTexture;
	Texture* m_pPlayerTexture;
	BossAttackScript::Database m_database;
	char m_pathBuffer[260];
	int m_selectedProfile;
	int m_selectedAttack;
	bool m_useJapaneseLabels;
	DirectX::XMFLOAT3 m_previewPlayerPos;
	DirectX::XMFLOAT3 m_previewPlayerSize;
	DirectX::XMFLOAT3 m_previewBossPos;
	DirectX::XMFLOAT3 m_previewBossStartPos;
	DirectX::XMFLOAT3 m_previewBossTargetPos;
	float m_previewPlayerFlashTimer;
	float m_previewFacingYawDeg;
	int m_previewAttackIndex;
	int m_previewRepeatRemaining;
	float m_previewStateTimer;
	float m_previewStateDuration;
	int m_previewState;
	bool m_previewPlaying;
	std::vector<PreviewZone> m_previewZones;
	std::vector<PreviewVisual> m_previewVisuals;
	std::vector<std::pair<std::string, Texture*>> m_textureCache;
};
