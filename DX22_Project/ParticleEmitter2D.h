#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

class Camera;
class Texture;

class ParticleEmitter2D
{
public:
	struct Settings
	{
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 angle = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT2 startSize = { 0.8f, 0.8f };
		DirectX::XMFLOAT2 endSize = { 0.2f, 0.2f };
		DirectX::XMFLOAT4 startColor = { 1.0f, 0.8f, 0.3f, 1.0f };
		DirectX::XMFLOAT4 endColor = { 1.0f, 0.2f, 0.1f, 0.0f };
		float emitSpeed = 2.5f;
		float duration = 1.2f;
		int emitCount = 48;
		float maxEmissionAngleDeg = 30.0f;
		float particleLifetime = 0.9f;
		float maxTravelDistance = 4.0f;
		bool alive = true;
		bool billboard = true;
		bool loop = true;
		bool worldSpace = true;
		std::string texturePath = "Assets/Texture/Star.png";
	};

	ParticleEmitter2D();
	~ParticleEmitter2D();

	void SetSettings(const Settings& settings);
	const Settings& GetSettings() const;

	bool SetTexturePath(const char* path);
	const char* GetTexturePath() const;

	void Play(bool clearParticles = true);
	void Stop(bool clearParticles = false);
	void Update(float dt);
	void Draw(Camera* camera);

	int GetAliveParticleCount() const;
	bool IsPlaying() const;

private:
	struct Particle
	{
		DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 localPosition = { 0.0f, 0.0f, 0.0f };
		DirectX::XMFLOAT3 velocity = { 0.0f, 0.0f, 0.0f };
		float age = 0.0f;
		float lifetime = 0.0f;
		float travelDistance = 0.0f;
		float rotationDeg = 0.0f;
		bool alive = false;
	};

	void EnsureTextureLoaded();
	void ReleaseTexture();
	void SpawnParticle();

private:
	Settings m_settings;
	std::vector<Particle> m_particles;
	Texture* m_pTexture;
	std::string m_loadedTexturePath;
	float m_emissionTime;
	int m_emittedThisCycle;
	bool m_isPlaying;
};
