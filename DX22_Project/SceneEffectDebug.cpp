#include "SceneEffectDebug.h"

#include <cstring>

#include "CameraDebug.h"
#include "Geometory.h"
#include "Input.h"
#include "Main.h"
#include "ParticleEmitter2D.h"
#include "ParticleEffectPreset.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "Texture.h"
#include "imgui.h"

namespace
{
	const float kFixedDt = 1.0f / 60.0f;
	const char* kDefaultEffectTexture = "Assets/Texture/Star.png";

	const char* SelectLabel(bool useJapanese, const char* english, const char* japanese)
	{
		return useJapanese ? japanese : english;
	}

	const char* SelectBoolText(bool useJapanese, bool value)
	{
		if (useJapanese)
		{
			return value ? u8"\u306f\u3044" : u8"\u3044\u3044\u3048";
		}
		return value ? "Yes" : "No";
	}

	float DegreesToRadians(float degrees)
	{
		return degrees * (DirectX::XM_PI / 180.0f);
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

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	void DrawFlatSprite(Texture* texture,
						const DirectX::XMFLOAT3& position,
						const DirectX::XMFLOAT2& size,
						const DirectX::XMFLOAT4& color)
	{
		if (!texture)
		{
			return;
		}

		DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationX(DirectX::XM_PIDIV2);
		DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(position.x, position.y, position.z);
		DirectX::XMFLOAT4X4 world{};
		DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(rotation * translation));
		Sprite::SetWorld(world);
		Sprite::SetSize(size);
		Sprite::SetOffset({ 0.0f, 0.0f });
		Sprite::SetUVPos({ 0.0f, 0.0f });
		Sprite::SetUVScale({ 1.0f, 1.0f });
		Sprite::SetColor(color);
		Sprite::SetTexture(texture);
		Sprite::Draw();
	}

	void DrawBillboardSprite(Texture* texture,
							 CameraDebug* camera,
							 const DirectX::XMFLOAT3& position,
							 const DirectX::XMFLOAT2& size,
							 const DirectX::XMFLOAT4& color)
	{
		if (!texture)
		{
			return;
		}

		using namespace DirectX;
		XMMATRIX billboard = XMMatrixIdentity();
		if (camera)
		{
			const XMFLOAT4X4 viewFloat = camera->GetViewMatrix(false);
			const XMMATRIX viewMatrix = XMLoadFloat4x4(&viewFloat);
			const XMMATRIX inverseView = XMMatrixInverse(nullptr, viewMatrix);
			XMFLOAT4X4 inverseViewFloat{};
			XMStoreFloat4x4(&inverseViewFloat, inverseView);
			inverseViewFloat._41 = 0.0f;
			inverseViewFloat._42 = 0.0f;
			inverseViewFloat._43 = 0.0f;
			billboard = XMLoadFloat4x4(&inverseViewFloat);
		}

		XMFLOAT4X4 world{};
		XMStoreFloat4x4(&world, XMMatrixTranspose(
			billboard * XMMatrixTranslation(position.x, position.y, position.z)));
		Sprite::SetWorld(world);
		Sprite::SetSize(size);
		Sprite::SetOffset({ 0.0f, 0.0f });
		Sprite::SetUVPos({ 0.0f, 0.0f });
		Sprite::SetUVScale({ 1.0f, 1.0f });
		Sprite::SetColor(color);
		Sprite::SetTexture(texture);
		Sprite::Draw();
	}
}

SceneEffectDebug::SceneEffectDebug()
	: m_pCamera(new CameraDebug())
	, m_pEmitter(new ParticleEmitter2D())
	, m_pFloorTexture(new Texture())
	, m_pMarkerTexture(new Texture())
	, m_texturePathBuffer{}
	, m_useJapaneseLabels(true)
	, m_pendingRestart(true)
{
	if (m_pCamera)
	{
		m_pCamera->SetPose({ 0.0f, 2.8f, -7.5f }, { 0.0f, 1.2f, 0.0f });
	}

	if (m_pFloorTexture && FAILED(m_pFloorTexture->Create("Assets/Texture/Game/jimen.png")))
	{
		delete m_pFloorTexture;
		m_pFloorTexture = nullptr;
	}

	if (m_pMarkerTexture && FAILED(m_pMarkerTexture->Create("Assets/Texture/Star.png")))
	{
		delete m_pMarkerTexture;
		m_pMarkerTexture = nullptr;
	}

	ParticleEmitter2D::Settings settings = ParticleEffectPreset::MakeDefaultSettings();
	ParticleEffectPreset::Load(settings);
	if (m_pEmitter)
	{
		m_pEmitter->SetSettings(settings);
	}
	strncpy_s(m_texturePathBuffer, sizeof(m_texturePathBuffer), settings.texturePath.c_str(), _TRUNCATE);
	m_pendingRestart = true;
}

SceneEffectDebug::~SceneEffectDebug()
{
	if (m_pMarkerTexture)
	{
		delete m_pMarkerTexture;
		m_pMarkerTexture = nullptr;
	}
	if (m_pFloorTexture)
	{
		delete m_pFloorTexture;
		m_pFloorTexture = nullptr;
	}
	if (m_pEmitter)
	{
		delete m_pEmitter;
		m_pEmitter = nullptr;
	}
	if (m_pCamera)
	{
		delete m_pCamera;
		m_pCamera = nullptr;
	}
}

void SceneEffectDebug::Update()
{
	if (IsMenuBackTrigger())
	{
		SceneManager::ChangeScene(SceneManager::SCENE_TITLE);
		return;
	}

	if (m_pCamera)
	{
		m_pCamera->Update();
	}

	if (m_pendingRestart && m_pEmitter)
	{
		m_pEmitter->Play(true);
		m_pendingRestart = false;
	}

	if (IsKeyTrigger(VK_SPACE) && m_pEmitter)
	{
		m_pEmitter->Play(true);
	}

	if (m_pEmitter)
	{
		m_pEmitter->Update(kFixedDt);
	}
}

void SceneEffectDebug::Draw()
{
	if (m_pCamera)
	{
		Sprite::SetView(m_pCamera->GetViewMatrix());
		Sprite::SetProjection(m_pCamera->GetProjectionMatrix());
	}

	DrawReferenceFloor();
	DrawEmitterMarker();
	DrawEmissionGuide();

	if (m_pEmitter)
	{
		m_pEmitter->Draw(m_pCamera);
	}

	DrawDebugWindow();
}

void SceneEffectDebug::ResetEmitterSettings()
{
	ParticleEmitter2D::Settings settings = ParticleEffectPreset::MakeDefaultSettings();
	if (m_pEmitter)
	{
		m_pEmitter->SetSettings(settings);
	}
	strncpy_s(m_texturePathBuffer, sizeof(m_texturePathBuffer), settings.texturePath.c_str(), _TRUNCATE);
	m_pendingRestart = true;
}

void SceneEffectDebug::SyncEmitterSettings()
{
	if (!m_pEmitter)
	{
		return;
	}

	ParticleEmitter2D::Settings settings = m_pEmitter->GetSettings();
	settings.texturePath = (m_texturePathBuffer[0] != '\0') ? m_texturePathBuffer : kDefaultEffectTexture;
	m_pEmitter->SetSettings(settings);
}

void SceneEffectDebug::DrawReferenceFloor()
{
	if (!m_pCamera)
	{
		return;
	}

	if (m_pFloorTexture)
	{
		DrawFlatSprite(
			m_pFloorTexture,
			{ 0.0f, 0.0f, 0.0f },
			{ 10.0f, 10.0f },
			{ 0.55f, 0.55f, 0.55f, 0.45f });
	}

	if (m_pMarkerTexture)
	{
		DrawFlatSprite(
			m_pMarkerTexture,
			{ 0.0f, 0.02f, 0.0f },
			{ 1.0f, 1.0f },
			{ 0.25f, 0.85f, 1.0f, 0.55f });
	}
}

void SceneEffectDebug::DrawEmitterMarker()
{
	if (!m_pEmitter || !m_pMarkerTexture)
	{
		return;
	}

	const ParticleEmitter2D::Settings& settings = m_pEmitter->GetSettings();
	DrawBillboardSprite(
		m_pMarkerTexture,
		m_pCamera,
		settings.position,
		{ 0.35f, 0.35f },
		{ 0.2f, 1.0f, 0.5f, 0.95f });
}

void SceneEffectDebug::DrawEmissionGuide()
{
	if (!m_pEmitter || !m_pCamera)
	{
		return;
	}

	const ParticleEmitter2D::Settings& settings = m_pEmitter->GetSettings();
	const float guideDistance = (settings.maxTravelDistance > 0.0f)
		? settings.maxTravelDistance
		: (settings.emitSpeed * settings.particleLifetime);
	if (guideDistance <= 0.0f)
	{
		return;
	}

	using namespace DirectX;
	const XMFLOAT3 origin = settings.position;
	const XMFLOAT3 centerDirection = TransformDirection(settings.angle, { 0.0f, 0.0f, 1.0f });
	const XMFLOAT3 centerEnd = AddFloat3(origin, ScaleFloat3(centerDirection, guideDistance));
	const float spread = ClampFloat(settings.maxEmissionAngleDeg, 0.0f, 180.0f);

	const XMFLOAT3 cornerAngles[4] =
	{
		{ settings.angle.x + spread, settings.angle.y - spread, settings.angle.z },
		{ settings.angle.x + spread, settings.angle.y + spread, settings.angle.z },
		{ settings.angle.x - spread, settings.angle.y + spread, settings.angle.z },
		{ settings.angle.x - spread, settings.angle.y - spread, settings.angle.z },
	};
	XMFLOAT3 cornerEnds[4]{};
	for (int i = 0; i < 4; ++i)
	{
		const XMFLOAT3 direction = TransformDirection(cornerAngles[i], { 0.0f, 0.0f, 1.0f });
		cornerEnds[i] = AddFloat3(origin, ScaleFloat3(direction, guideDistance));
	}

	XMFLOAT4X4 identity{};
	XMStoreFloat4x4(&identity, XMMatrixIdentity());
	Geometory::SetWorld(identity);
	Geometory::SetView(m_pCamera->GetViewMatrix());
	Geometory::SetProjection(m_pCamera->GetProjectionMatrix());

	const XMFLOAT4 centerColor = { 0.25f, 0.95f, 1.0f, 1.0f };
	const XMFLOAT4 edgeColor = { 1.0f, 0.75f, 0.20f, 1.0f };
	const XMFLOAT4 frameColor = { 0.35f, 1.0f, 0.55f, 1.0f };

	Geometory::AddLine(origin, centerEnd, centerColor);
	for (int i = 0; i < 4; ++i)
	{
		Geometory::AddLine(origin, cornerEnds[i], edgeColor);
		Geometory::AddLine(cornerEnds[i], cornerEnds[(i + 1) % 4], frameColor);
	}

	Geometory::DrawLines();
}

void SceneEffectDebug::DrawDebugWindow()
{
	if (!m_pEmitter)
	{
		return;
	}

	ParticleEmitter2D::Settings settings = m_pEmitter->GetSettings();
	ImGui::SetNextWindowPos(ImVec2(24.0f, 24.0f), ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(ImVec2(430.0f, 700.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(SelectLabel(
		m_useJapaneseLabels,
		"Effect Debug",
		u8"\u30a8\u30d5\u30a7\u30af\u30c8\u78ba\u8a8d")))
	{
		ImGui::End();
		return;
	}

	int languageSelection = m_useJapaneseLabels ? 1 : 0;
	const char* languageItems[] =
	{
		"English",
		u8"\u65e5\u672c\u8a9e"
	};
	if (ImGui::Combo(
		SelectLabel(m_useJapaneseLabels, "Language", u8"\u8a00\u8a9e"),
		&languageSelection,
		languageItems,
		IM_ARRAYSIZE(languageItems)))
	{
		m_useJapaneseLabels = (languageSelection == 1);
	}

	ImGui::TextUnformatted(SelectLabel(
		m_useJapaneseLabels,
		"2D Particle Preview",
		u8"2D\u30d1\u30fc\u30c6\u30a3\u30af\u30eb\u78ba\u8a8d"));
	ImGui::TextDisabled(SelectLabel(
		m_useJapaneseLabels,
		"Esc / Start / DirectInput Button 9: Back to Title",
		u8"Esc / Start / DirectInput Button 9: \u30bf\u30a4\u30c8\u30eb\u3078\u623b\u308b"));
	ImGui::TextDisabled(SelectLabel(
		m_useJapaneseLabels,
		"Space: Play",
		u8"Space: \u518d\u751f"));
	ImGui::Separator();

	bool changed = false;
	changed |= ImGui::Checkbox(SelectLabel(m_useJapaneseLabels, "Enabled", u8"\u6709\u52b9"), &settings.alive);
	changed |= ImGui::Checkbox(SelectLabel(m_useJapaneseLabels, "Loop", u8"\u30eb\u30fc\u30d7"), &settings.loop);
	changed |= ImGui::Checkbox(SelectLabel(m_useJapaneseLabels, "Billboard", u8"\u30d3\u30eb\u30dc\u30fc\u30c9"), &settings.billboard);
	changed |= ImGui::Checkbox(SelectLabel(m_useJapaneseLabels, "World Space", u8"\u30ef\u30fc\u30eb\u30c9\u5ea7\u6a19"), &settings.worldSpace);
	changed |= ImGui::DragFloat3(SelectLabel(m_useJapaneseLabels, "Position", u8"\u4f4d\u7f6e"), reinterpret_cast<float*>(&settings.position), 0.05f);
	changed |= ImGui::DragFloat3(SelectLabel(m_useJapaneseLabels, "Angle", u8"\u89d2\u5ea6"), reinterpret_cast<float*>(&settings.angle), 1.0f);
	changed |= ImGui::DragFloat2(SelectLabel(m_useJapaneseLabels, "Start Size", u8"\u958b\u59cb\u30b5\u30a4\u30ba"), reinterpret_cast<float*>(&settings.startSize), 0.01f, 0.01f, 8.0f);
	changed |= ImGui::DragFloat2(SelectLabel(m_useJapaneseLabels, "End Size", u8"\u7d42\u4e86\u30b5\u30a4\u30ba"), reinterpret_cast<float*>(&settings.endSize), 0.01f, 0.01f, 8.0f);
	changed |= ImGui::ColorEdit4(SelectLabel(m_useJapaneseLabels, "Start Color", u8"\u958b\u59cb\u8272"), reinterpret_cast<float*>(&settings.startColor));
	changed |= ImGui::ColorEdit4(SelectLabel(m_useJapaneseLabels, "End Color", u8"\u7d42\u4e86\u8272"), reinterpret_cast<float*>(&settings.endColor));
	changed |= ImGui::DragFloat(SelectLabel(m_useJapaneseLabels, "Emit Speed", u8"\u5c04\u51fa\u901f\u5ea6"), &settings.emitSpeed, 0.05f, 0.0f, 20.0f);
	changed |= ImGui::DragFloat(SelectLabel(m_useJapaneseLabels, "Duration", u8"\u7d99\u7d9a\u6642\u9593"), &settings.duration, 0.01f, 0.01f, 20.0f);
	changed |= ImGui::DragInt(SelectLabel(m_useJapaneseLabels, "Emit Count", u8"\u5c04\u51fa\u6570"), &settings.emitCount, 1.0f, 0, 512);
	changed |= ImGui::DragFloat(SelectLabel(m_useJapaneseLabels, "Max Emit Angle", u8"\u6700\u5927\u5c04\u51fa\u89d2"), &settings.maxEmissionAngleDeg, 0.5f, 0.0f, 180.0f);
	changed |= ImGui::DragFloat(SelectLabel(m_useJapaneseLabels, "Particle Lifetime", u8"\u7c92\u5b50\u5bff\u547d"), &settings.particleLifetime, 0.01f, 0.01f, 20.0f);
	changed |= ImGui::DragFloat(SelectLabel(m_useJapaneseLabels, "Max Travel Distance", u8"\u6700\u5927\u79fb\u52d5\u8ddd\u96e2"), &settings.maxTravelDistance, 0.05f, 0.0f, 50.0f);

	settings.duration = ClampFloat(settings.duration, 0.01f, 20.0f);
	settings.particleLifetime = ClampFloat(settings.particleLifetime, 0.01f, 20.0f);
	settings.emitSpeed = ClampFloat(settings.emitSpeed, 0.0f, 20.0f);
	settings.maxEmissionAngleDeg = ClampFloat(settings.maxEmissionAngleDeg, 0.0f, 180.0f);
	settings.maxTravelDistance = ClampFloat(settings.maxTravelDistance, 0.0f, 50.0f);
	if (settings.emitCount < 0) settings.emitCount = 0;
	if (settings.emitCount > 512) settings.emitCount = 512;
	if (settings.startSize.x < 0.01f) settings.startSize.x = 0.01f;
	if (settings.startSize.y < 0.01f) settings.startSize.y = 0.01f;
	if (settings.endSize.x < 0.01f) settings.endSize.x = 0.01f;
	if (settings.endSize.y < 0.01f) settings.endSize.y = 0.01f;

	ImGui::SeparatorText(SelectLabel(m_useJapaneseLabels, "Texture", u8"\u30c6\u30af\u30b9\u30c1\u30e3"));
	ImGui::InputText("##effect_texture_path", m_texturePathBuffer, sizeof(m_texturePathBuffer));
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Reload Texture", u8"\u518d\u8aad\u8fbc")))
	{
		settings.texturePath = (m_texturePathBuffer[0] != '\0') ? m_texturePathBuffer : kDefaultEffectTexture;
		changed = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Default Texture", u8"\u65e2\u5b9a\u30c6\u30af\u30b9\u30c1\u30e3")))
	{
		strncpy_s(m_texturePathBuffer, sizeof(m_texturePathBuffer), kDefaultEffectTexture, _TRUNCATE);
		settings.texturePath = kDefaultEffectTexture;
		changed = true;
	}

	ImGui::SeparatorText(SelectLabel(m_useJapaneseLabels, "Preset", u8"\u4fdd\u5b58\u30d7\u30ea\u30bb\u30c3\u30c8"));
	ImGui::TextDisabled("%s", ParticleEffectPreset::GetDefaultPath());
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Save Preset", u8"\u4fdd\u5b58")))
	{
		settings.texturePath = (m_texturePathBuffer[0] != '\0') ? m_texturePathBuffer : kDefaultEffectTexture;
		m_pEmitter->SetSettings(settings);
		ParticleEffectPreset::Save(settings);
	}
	ImGui::SameLine();
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Load Preset", u8"\u8aad\u8fbc")))
	{
		ParticleEmitter2D::Settings loadedSettings = ParticleEffectPreset::MakeDefaultSettings();
		if (ParticleEffectPreset::Load(loadedSettings))
		{
			settings = loadedSettings;
			strncpy_s(m_texturePathBuffer, sizeof(m_texturePathBuffer), settings.texturePath.c_str(), _TRUNCATE);
			changed = true;
			m_pendingRestart = true;
		}
	}

	ImGui::SeparatorText(SelectLabel(m_useJapaneseLabels, "Controls", u8"\u64cd\u4f5c"));
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Play", u8"\u518d\u751f")))
	{
		m_pEmitter->SetSettings(settings);
		m_pEmitter->Play(true);
		m_pendingRestart = false;
	}
	ImGui::SameLine();
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Stop", u8"\u505c\u6b62")))
	{
		m_pEmitter->Stop(false);
		m_pendingRestart = false;
	}
	ImGui::SameLine();
	if (ImGui::Button(SelectLabel(m_useJapaneseLabels, "Reset Defaults", u8"\u521d\u671f\u5024\u306b\u623b\u3059")))
	{
		ResetEmitterSettings();
		ImGui::End();
		return;
	}

	if (changed)
	{
		m_pEmitter->SetSettings(settings);
	}

	ImGui::SeparatorText(SelectLabel(m_useJapaneseLabels, "Status", u8"\u72b6\u614b"));
	ImGui::Text(
		"%s: %s",
		SelectLabel(m_useJapaneseLabels, "Playing", u8"\u518d\u751f\u4e2d"),
		SelectBoolText(m_useJapaneseLabels, m_pEmitter->IsPlaying()));
	ImGui::Text(
		"%s: %d",
		SelectLabel(m_useJapaneseLabels, "Alive Particles", u8"\u751f\u5b58\u30d1\u30fc\u30c6\u30a3\u30af\u30eb\u6570"),
		m_pEmitter->GetAliveParticleCount());
	ImGui::Text(
		"%s: %s",
		SelectLabel(m_useJapaneseLabels, "Current Texture", u8"\u73fe\u5728\u30c6\u30af\u30b9\u30c1\u30e3"),
		m_pEmitter->GetTexturePath());
	ImGui::End();
}
