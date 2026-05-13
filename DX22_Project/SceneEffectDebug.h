#pragma once

#include "Scene.h"

class CameraDebug;
class ParticleEmitter2D;
class Texture;

class SceneEffectDebug : public Scene
{
public:
	SceneEffectDebug();
	~SceneEffectDebug() override;

	void Update() override;
	void Draw() override;

private:
	void ResetEmitterSettings();
	void SyncEmitterSettings();
	void DrawReferenceFloor();
	void DrawEmitterMarker();
	void DrawEmissionGuide();
	void DrawDebugWindow();

private:
	CameraDebug* m_pCamera;
	ParticleEmitter2D* m_pEmitter;
	Texture* m_pFloorTexture;
	Texture* m_pMarkerTexture;
	char m_texturePathBuffer[260];
	bool m_useJapaneseLabels;
	bool m_pendingRestart;
};
