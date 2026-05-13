#include "ParticleEmitter2D.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "Camera.h"
#include "Sprite.h"
#include "Texture.h"

namespace
{
	const char* kDefaultParticleTexturePath = "Assets/Texture/Star.png";
	const float kMinDuration = 0.01f;
	const float kMinLifetime = 0.01f;
	const float kMinSize = 0.01f;

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	float LerpFloat(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	DirectX::XMFLOAT2 LerpFloat2(const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, float t)
	{
		return
		{
			LerpFloat(a.x, b.x, t),
			LerpFloat(a.y, b.y, t)
		};
	}

	DirectX::XMFLOAT4 LerpFloat4(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t)
	{
		return
		{
			LerpFloat(a.x, b.x, t),
			LerpFloat(a.y, b.y, t),
			LerpFloat(a.z, b.z, t),
			LerpFloat(a.w, b.w, t)
		};
	}

	float DegreesToRadians(float degrees)
	{
		return degrees * (DirectX::XM_PI / 180.0f);
	}

	float RandomRange(float minValue, float maxValue)
	{
		if (maxValue <= minValue)
		{
			return minValue;
		}
		const float rate = static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * rate;
	}

	float Length3(const DirectX::XMFLOAT3& value)
	{
		return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
	}

	DirectX::XMFLOAT3 AddFloat3(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
	{
		return
		{
			a.x + b.x,
			a.y + b.y,
			a.z + b.z
		};
	}

	DirectX::XMFLOAT3 ScaleFloat3(const DirectX::XMFLOAT3& value, float scale)
	{
		return
		{
			value.x * scale,
			value.y * scale,
			value.z * scale
		};
	}

	DirectX::XMFLOAT3 TransformDirection(const DirectX::XMFLOAT3& eulerDeg, const DirectX::XMFLOAT3& localDirection)
	{
		using namespace DirectX;
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
			DegreesToRadians(eulerDeg.x),
			DegreesToRadians(eulerDeg.y),
			DegreesToRadians(eulerDeg.z));
		XMVECTOR direction = XMVectorSet(localDirection.x, localDirection.y, localDirection.z, 0.0f);
		direction = XMVector3TransformNormal(direction, rotation);
		direction = XMVector3Normalize(direction);
		XMFLOAT3 result{};
		XMStoreFloat3(&result, direction);
		return result;
	}

	DirectX::XMMATRIX BuildBillboardMatrix(Camera* camera)
	{
		using namespace DirectX;
		XMMATRIX billboard = XMMatrixIdentity();
		if (!camera)
		{
			return billboard;
		}

		const XMFLOAT4X4 viewFloat = camera->GetViewMatrix(false);
		const XMMATRIX viewMatrix = XMLoadFloat4x4(&viewFloat);
		const XMMATRIX inverseView = XMMatrixInverse(nullptr, viewMatrix);
		XMFLOAT4X4 inverseViewFloat{};
		XMStoreFloat4x4(&inverseViewFloat, inverseView);
		inverseViewFloat._41 = 0.0f;
		inverseViewFloat._42 = 0.0f;
		inverseViewFloat._43 = 0.0f;
		return XMLoadFloat4x4(&inverseViewFloat);
	}
}

ParticleEmitter2D::ParticleEmitter2D()
	: m_pTexture(nullptr)
	, m_loadedTexturePath()
	, m_emissionTime(0.0f)
	, m_emittedThisCycle(0)
	, m_isPlaying(false)
{
	EnsureTextureLoaded();
}

ParticleEmitter2D::~ParticleEmitter2D()
{
	ReleaseTexture();
}

void ParticleEmitter2D::SetSettings(const Settings& settings)
{
	const std::string nextTexturePath = settings.texturePath.empty() ? kDefaultParticleTexturePath : settings.texturePath;
	m_settings = settings;
	m_settings.texturePath = nextTexturePath;
	EnsureTextureLoaded();
	if (!m_settings.alive)
	{
		m_isPlaying = false;
	}
}

const ParticleEmitter2D::Settings& ParticleEmitter2D::GetSettings() const
{
	return m_settings;
}

bool ParticleEmitter2D::SetTexturePath(const char* path)
{
	const std::string nextPath = (path && path[0] != '\0') ? path : kDefaultParticleTexturePath;
	m_settings.texturePath = nextPath;
	EnsureTextureLoaded();
	return m_pTexture != nullptr;
}

const char* ParticleEmitter2D::GetTexturePath() const
{
	return m_settings.texturePath.c_str();
}

void ParticleEmitter2D::Play(bool clearParticles)
{
	if (clearParticles)
	{
		m_particles.clear();
	}
	m_emissionTime = 0.0f;
	m_emittedThisCycle = 0;
	m_isPlaying = m_settings.alive;
}

void ParticleEmitter2D::Stop(bool clearParticles)
{
	m_isPlaying = false;
	m_emissionTime = 0.0f;
	m_emittedThisCycle = 0;
	if (clearParticles)
	{
		m_particles.clear();
	}
}

void ParticleEmitter2D::Update(float dt)
{
	if (dt <= 0.0f)
	{
		return;
	}

	if (m_settings.alive && m_isPlaying)
	{
		const float safeDuration = (m_settings.duration > kMinDuration) ? m_settings.duration : kMinDuration;
		const int safeEmitCount = (m_settings.emitCount < 0) ? 0 : m_settings.emitCount;
		float remainingDt = dt;
		while (remainingDt > 0.0f && m_isPlaying)
		{
			float cycleRemaining = safeDuration - m_emissionTime;
			if (cycleRemaining < 0.0f)
			{
				cycleRemaining = 0.0f;
			}
			float stepDt = remainingDt;
			if (stepDt > cycleRemaining)
			{
				stepDt = cycleRemaining;
			}

			m_emissionTime += stepDt;
			remainingDt -= stepDt;

			const float progress = ClampFloat(m_emissionTime / safeDuration, 0.0f, 1.0f);
			const int targetEmitCount = static_cast<int>(progress * static_cast<float>(safeEmitCount));
			while (m_emittedThisCycle < targetEmitCount)
			{
				SpawnParticle();
				++m_emittedThisCycle;
			}

			if (m_emissionTime >= safeDuration - 1.0e-6f)
			{
				if (m_settings.loop)
				{
					m_emissionTime = 0.0f;
					m_emittedThisCycle = 0;
				}
				else
				{
					m_isPlaying = false;
				}
			}

			if (stepDt <= 1.0e-6f)
			{
				break;
			}
		}
	}

	for (auto& particle : m_particles)
	{
		if (!particle.alive)
		{
			continue;
		}

		const DirectX::XMFLOAT3 delta = ScaleFloat3(particle.velocity, dt);
		if (m_settings.worldSpace)
		{
			particle.position = AddFloat3(particle.position, delta);
		}
		else
		{
			particle.localPosition = AddFloat3(particle.localPosition, delta);
		}
		particle.age += dt;
		particle.travelDistance += Length3(delta);

		const bool overLifetime = particle.age >= particle.lifetime;
		const bool overDistance = (m_settings.maxTravelDistance > 0.0f) &&
			(particle.travelDistance >= m_settings.maxTravelDistance);
		if (overLifetime || overDistance)
		{
			particle.alive = false;
		}
	}

	m_particles.erase(
		std::remove_if(m_particles.begin(), m_particles.end(),
			[](const Particle& particle) { return !particle.alive; }),
		m_particles.end());
}

void ParticleEmitter2D::Draw(Camera* camera)
{
	if (!m_settings.alive)
	{
		return;
	}

	EnsureTextureLoaded();
	if (!m_pTexture || m_particles.empty())
	{
		return;
	}

	using namespace DirectX;
	Sprite::SetView(camera ? camera->GetViewMatrix() : XMFLOAT4X4{});
	Sprite::SetProjection(camera ? camera->GetProjectionMatrix() : XMFLOAT4X4{});
	const XMMATRIX billboard = BuildBillboardMatrix(camera);
	const XMFLOAT3 cameraPos = camera ? camera->GetPos() : XMFLOAT3{ 0.0f, 0.0f, -10.0f };

	std::vector<const Particle*> drawOrder;
	drawOrder.reserve(m_particles.size());
	for (const auto& particle : m_particles)
	{
		if (particle.alive)
		{
			drawOrder.push_back(&particle);
		}
	}
	std::sort(drawOrder.begin(), drawOrder.end(),
		[&](const Particle* a, const Particle* b)
		{
			const DirectX::XMFLOAT3 aPos = m_settings.worldSpace ? a->position : AddFloat3(m_settings.position, a->localPosition);
			const DirectX::XMFLOAT3 bPos = m_settings.worldSpace ? b->position : AddFloat3(m_settings.position, b->localPosition);
			const float adx = aPos.x - cameraPos.x;
			const float ady = aPos.y - cameraPos.y;
			const float adz = aPos.z - cameraPos.z;
			const float bdx = bPos.x - cameraPos.x;
			const float bdy = bPos.y - cameraPos.y;
			const float bdz = bPos.z - cameraPos.z;
			return (adx * adx + ady * ady + adz * adz) > (bdx * bdx + bdy * bdy + bdz * bdz);
		});

	for (const Particle* particle : drawOrder)
	{
		const float lifeRate = ClampFloat(
			(particle->lifetime > kMinLifetime) ? (particle->age / particle->lifetime) : 1.0f,
			0.0f,
			1.0f);
		DirectX::XMFLOAT2 size = LerpFloat2(m_settings.startSize, m_settings.endSize, lifeRate);
		if (size.x < kMinSize) size.x = kMinSize;
		if (size.y < kMinSize) size.y = kMinSize;
		const DirectX::XMFLOAT4 color = LerpFloat4(m_settings.startColor, m_settings.endColor, lifeRate);
		const DirectX::XMFLOAT3 drawPos = m_settings.worldSpace ? particle->position : AddFloat3(m_settings.position, particle->localPosition);

		XMMATRIX worldMatrix = XMMatrixIdentity();
		if (m_settings.billboard)
		{
			const XMMATRIX rotation = XMMatrixRotationZ(DegreesToRadians(particle->rotationDeg));
			worldMatrix = rotation * billboard * XMMatrixTranslation(drawPos.x, drawPos.y, drawPos.z);
		}
		else
		{
			const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
				DegreesToRadians(m_settings.angle.x),
				DegreesToRadians(m_settings.angle.y),
				DegreesToRadians(m_settings.angle.z + particle->rotationDeg));
			worldMatrix = rotation * XMMatrixTranslation(drawPos.x, drawPos.y, drawPos.z);
		}

		XMFLOAT4X4 world{};
		XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
		Sprite::SetWorld(world);
		Sprite::SetSize(size);
		Sprite::SetOffset({ 0.0f, 0.0f });
		Sprite::SetUVPos({ 0.0f, 0.0f });
		Sprite::SetUVScale({ 1.0f, 1.0f });
		Sprite::SetColor(color);
		Sprite::SetTexture(m_pTexture);
		Sprite::Draw();
	}
}

int ParticleEmitter2D::GetAliveParticleCount() const
{
	return static_cast<int>(m_particles.size());
}

bool ParticleEmitter2D::IsPlaying() const
{
	return m_isPlaying;
}

void ParticleEmitter2D::EnsureTextureLoaded()
{
	const std::string nextPath = m_settings.texturePath.empty() ? kDefaultParticleTexturePath : m_settings.texturePath;
	if (m_pTexture && m_loadedTexturePath == nextPath)
	{
		return;
	}

	ReleaseTexture();
	m_pTexture = new Texture();
	if (FAILED(m_pTexture->Create(nextPath.c_str())))
	{
		delete m_pTexture;
		m_pTexture = nullptr;
		if (nextPath != kDefaultParticleTexturePath)
		{
			m_pTexture = new Texture();
			if (FAILED(m_pTexture->Create(kDefaultParticleTexturePath)))
			{
				delete m_pTexture;
				m_pTexture = nullptr;
				m_loadedTexturePath.clear();
				return;
			}
			m_loadedTexturePath = kDefaultParticleTexturePath;
			return;
		}
		m_loadedTexturePath.clear();
		return;
	}
	m_loadedTexturePath = nextPath;
}

void ParticleEmitter2D::ReleaseTexture()
{
	if (m_pTexture)
	{
		delete m_pTexture;
		m_pTexture = nullptr;
	}
}

void ParticleEmitter2D::SpawnParticle()
{
	if (!m_settings.alive)
	{
		return;
	}

	const float spread = ClampFloat(m_settings.maxEmissionAngleDeg, 0.0f, 180.0f);
	const DirectX::XMFLOAT3 spreadEuler =
	{
		RandomRange(-spread, spread),
		RandomRange(-spread, spread),
		0.0f
	};
	const DirectX::XMFLOAT3 direction = TransformDirection(
		{
			m_settings.angle.x + spreadEuler.x,
			m_settings.angle.y + spreadEuler.y,
			m_settings.angle.z
		},
		{ 0.0f, 0.0f, 1.0f });

	Particle particle{};
	particle.position = m_settings.position;
	particle.localPosition = { 0.0f, 0.0f, 0.0f };
	particle.velocity = ScaleFloat3(direction, m_settings.emitSpeed);
	particle.age = 0.0f;
	particle.lifetime = (m_settings.particleLifetime > kMinLifetime) ? m_settings.particleLifetime : kMinLifetime;
	particle.travelDistance = 0.0f;
	particle.rotationDeg = m_settings.angle.z;
	particle.alive = true;
	m_particles.push_back(particle);
}
