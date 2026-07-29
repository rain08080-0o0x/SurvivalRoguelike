#include "SceneNarakuProto.h"

#include "NarakuStageGenerator.h"
#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "MeshBuffer.h"
#include "Model.h"
#include "Shader.h"
#include "ShaderList.h"
#include "Sprite.h"
#include "Texture.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>

namespace
{
    constexpr const char* kPlaytestConfigPath = "Assets/Config/naraku_proto_playtest.json";
    constexpr const wchar_t* kEnvironmentModelCatalogRelativePath = L"Assets/Naraku/environment_models.cfg";

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (length <= 1) return {};
        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
        result.pop_back();
        return result;
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1) return {};
        std::string result(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
        result.pop_back();
        return result;
    }

    std::wstring GetNarakuProjectRoot()
    {
        std::wstring mapPath = NarakuMap::ResolveMapPathForFileSystem(NarakuMap::GetDefaultMapPath());
        std::replace(mapPath.begin(), mapPath.end(), L'\\', L'/');
        const std::wstring marker = L"/Assets/Maps/";
        const size_t markerPos = mapPath.find(marker);
        return markerPos == std::wstring::npos ? std::wstring() : mapPath.substr(0, markerPos);
    }

    std::wstring ResolveProjectPath(const std::wstring& path)
    {
        if (path.size() >= 2 && path[1] == L':') return path;
        const std::wstring root = GetNarakuProjectRoot();
        if (root.empty()) return path;
        return root + L"/" + path;
    }

    bool TryReadJsonFloat(const std::string& json, const char* key, float& outValue)
    {
        const std::string token = std::string("\"") + key + "\"";
        const std::size_t keyPos = json.find(token);
        if (keyPos == std::string::npos)
        {
            return false;
        }
        const std::size_t colonPos = json.find(':', keyPos + token.size());
        if (colonPos == std::string::npos)
        {
            return false;
        }
        char* end = nullptr;
        const float value = std::strtof(json.c_str() + colonPos + 1, &end);
        if (end == json.c_str() + colonPos + 1 || !std::isfinite(value))
        {
            return false;
        }
        outValue = value;
        return true;
    }

    // 既存プロジェクトは固定FPS前提なので、1フレーム秒数も固定値で扱います。
    constexpr float kDt = 1.0f / fFPS;
    // ステップで進む距離です。
    constexpr float kStepDistance = 5.0f;
    // ステップの無敵時間です。
    constexpr float kStepInvincibleTime = 0.5f;
    // ステップ後に操作を戻すまでの硬直時間です。
    constexpr float kStepRecoveryTime = 0.5f;
    // 攻撃ボタンを押してから攻撃判定が出るまでの時間です。
    constexpr float kAttackStartup = 0.25f;
    // 攻撃判定が有効な時間です。
    constexpr float kAttackActive = 0.15f;
    // 攻撃判定後の硬直時間です。
    constexpr float kAttackRecovery = 0.40f;
    // 攻撃全体の長さです。
    constexpr float kAttackTotal = kAttackStartup + kAttackActive + kAttackRecovery;
    // 採掘モーション完了までの時間です。
    constexpr float kMiningTime = 2.0f;
    // 第一層プロトタイプで上昇負荷が発症する累計上昇量です。
    constexpr float kUpperLoadLimit = 10.0f;
    // 上昇していない時に1秒あたり回復する上昇負荷ゲージ量です。
    constexpr float kUpperLoadRecoveryPerSecond = 1.0f;
    // 通常の最大重量です。100%以上でも歩けますが一部行動が制限されます。
    constexpr float kMaxWeight = 100.0f;
    // 拾える限界重量です。これを超える拾得は拒否します。
    constexpr float kPickupWeightLimit = 150.0f;
    // 敵の通常移動速度です。既定プレイヤー通常速度1.5m/sの50%です。
    constexpr float kEnemyWalkSpeed = 0.75f;
    // 敵の体当たり速度です。敵通常移動の3倍です。
    constexpr float kEnemyChargeSpeed = kEnemyWalkSpeed * 3.0f;
    // 敵が次の体当たりを開始するまでの間隔です。
    constexpr float kEnemyAttackInterval = 5.0f;
    // 敵の体当たり前予備動作時間です。
    constexpr float kEnemyTelegraphTime = 0.55f;
    // 敵の体当たり移動時間です。
    constexpr float kEnemyChargeTime = 0.45f;
    constexpr float kEnemyChargeStartSpeedScale = 0.35f;
    constexpr float kEnemyChargeEndSpeedScale = 1.65f;
    // 敵の体当たりが命中する距離です。
    constexpr float kEnemyHitRange = 0.45f;
    // 敵の体当たり命中時に押し出す距離です。
    constexpr float kKnockbackDistance = 1.5f;
    // ノックバックが続く時間です。
    constexpr float kKnockbackTime = 0.25f;
    // 採掘、ロープ、地面旧器に反応する距離です。
    constexpr float kInteractRange = 1.0f;
    // 帰還地点に反応する距離です。
    constexpr float kReturnRange = 1.4f;
    // 未発見採掘ポイントを発見する距離です。
    constexpr float kDiscoverRange = 3.0f;
    constexpr float kNearbyMiningVisibleRange = 8.0f;
    // つるはし攻撃の射程です。
    constexpr float kAttackRange = 1.15f;
    constexpr int kAttackHitEffectFrameCount = 10;
    constexpr float kAttackHitEffectFrameTime = 1.0f / 30.0f;
    constexpr float kAttackHitEffectDuration = kAttackHitEffectFrameCount * kAttackHitEffectFrameTime;
    constexpr float kCameraShakeDuration = 0.24f;
    constexpr float kCameraShakeAmplitude = 0.18f;
    constexpr float kSkySphereRadius = 180.0f;
    /** @brief 探索カメラの注視点からの固定距離です。 */
    constexpr float kCameraDefaultDistance = 13.8564f;
    /** @brief カメラ仰角の下限（真横を0度、真上を90度）です。 */
    constexpr float kCameraMinPitchDegrees = 1.0f;
    constexpr float kCameraMaxPitchDegrees = 89.0f;
    constexpr float kCameraDefaultMinPitchDegrees = 10.0f;
    constexpr float kCameraDefaultMaxPitchDegrees = 60.0f;
    constexpr float kCameraMinDistance = 6.0f;
    constexpr float kCameraMaxDistance = kCameraDefaultDistance;
    constexpr float kRelicArmorWalkMultiplier = 1.2f;
    constexpr float kRelicArmorRunMultiplier = 2.0f;
    constexpr float kRelicArmorHpRecoveryPerSecond = 0.2f;
    constexpr int kMapGenerationMaxAttempts = 5;
    constexpr int kLayerGateFailureLimit = 5;
    constexpr int kMaximumAreaDepth = 3;
    constexpr float kLayerTransitionDuration = 1.5f;
    constexpr float kLayerTransitionHeight = 6.0f;
    constexpr float kCompassRadius = 30.0f;
    constexpr float kCompassMargin = 12.0f;
    constexpr float kCompassLineThickness = 1.5f;
    constexpr float kCompassLinePadding = 2.0f;
    constexpr float kCompassLabelDistance = 8.0f;
    const char* const kCompassDirectionLabels[] = { u8"北", u8"南", u8"東", u8"西" };
    // Shiftをこの秒数以上押し続けたら走り扱いにします。
    constexpr float kShiftRunThreshold = 0.18f;
    // 通常歩行で乗り越えられる上り段差です。
    constexpr float kMaxWalkClimbHeight = 0.55f;
    // 通常歩行でそのまま降りられる下り段差です。これを超える下りは落下可属性が必要です。
    constexpr float kMaxWalkDropHeight = 0.80f;
    // 歩行移動後に地面へ即吸着せず、そのまま落下へ移る下り落差です。
    constexpr float kAutoFallStartHeight = 0.90f;
    // 崖境界セルで通常歩行を止める下り段差です。
    constexpr float kCliffEdgeBlockDropHeight = 0.20f;
    // 危険地形の継続ダメージ間隔です。
    constexpr float kHazardTickInterval = 0.50f;
    // 危険地形1回ぶんのダメージです。
    constexpr float kHazardDamage = 0.5f;
    // 斜面移動時に経路を分割する1区間の基準長です。
    constexpr float kSlopeMoveSampleStep = 0.25f;
    // セル境界の浮動小数誤差で通行不可になりにくくするための許容値です。
    constexpr float kSlopeHeightTolerance = 0.03f;
    // 軽い落下で発生する着地硬直時間です。
    constexpr float kLandingRecoveryLight = 0.14f;
    // 中程度の落下で発生する着地硬直時間です。
    constexpr float kLandingRecoveryMedium = 0.24f;
    // 重い落下で発生する着地硬直時間です。
    constexpr float kLandingRecoveryHeavy = 0.38f;

    // プロトタイプで使う4級旧器名です。コード側では文字化け回避のため英字にしています。
    const char* kRelicNames[] =
    {
        u8"錆びた輪",
        u8"欠けた歯車",
        u8"古びた留め具",
        u8"音のしない鈴",
        u8"黒ずんだ皿片",
        u8"ひび入り硝子",
        u8"曲がった鍵片",
        u8"くすんだ小筒"
    };
}

SceneNarakuProto::SceneNarakuProto()
{
    InitializeTerrainFloorBatch();
    InitializeEnemyBillboardBatch();

    m_attackHitTexture = new Texture();
    if (FAILED(m_attackHitTexture->Create("Assets/Texture/Effect/yellow_bom.png")))
    {
        SAFE_DELETE(m_attackHitTexture);
    }

    m_skyModel = new Model();
    if (!m_skyModel->Load("Assets/Model/sky/sky.obj"))
    {
        SAFE_DELETE(m_skyModel);
    }

    // プレイテスト用の調整値を既定値で初期化します。
    ResetDebugPlayerParams();
    LoadDebugPlayerParams();

    // 初期装備を所有・装備済みにします。
    m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedWeapons[static_cast<std::size_t>(WeaponTier::RustyPickaxe)] = true;

    // フィールドを準備し、最初は自宅で持ち物を決められる状態にします。
    ResetRun();
    m_mode = Mode::Home;
}

SceneNarakuProto::~SceneNarakuProto()
{
    ReleaseEnvironmentModels();
    ReleaseEnemyBillboardBatch();
    ReleaseTerrainFloorBatch();
    SAFE_DELETE(m_skyModel);
    SAFE_DELETE(m_attackHitTexture);
}

void SceneNarakuProto::InitializeTerrainFloorBatch()
{
    const char* vertexShaderCode = R"HLSL(
struct VS_IN {
    float3 position : POSITION0;
    float4 color : COLOR0;
};
struct VS_OUT {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};
cbuffer Matrix : register(b0) {
    float4x4 view;
    float4x4 projection;
};
VS_OUT main(VS_IN input) {
    VS_OUT output;
    output.position = mul(float4(input.position, 1.0f), view);
    output.position = mul(output.position, projection);
    output.color = input.color;
    return output;
})HLSL";

    const char* pixelShaderCode = R"HLSL(
struct PS_IN {
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};
float4 main(PS_IN input) : SV_TARGET {
    return input.color;
})HLSL";

    m_terrainFloorVS = new VertexShader();
    if (FAILED(m_terrainFloorVS->Compile(vertexShaderCode)))
    {
        SAFE_DELETE(m_terrainFloorVS);
    }

    m_terrainFloorPS = new PixelShader();
    if (FAILED(m_terrainFloorPS->Compile(pixelShaderCode)))
    {
        SAFE_DELETE(m_terrainFloorPS);
    }
}

void SceneNarakuProto::ReleaseTerrainFloorBatch()
{
    SAFE_DELETE(m_terrainFloorMesh);
    SAFE_DELETE(m_terrainFloorPS);
    SAFE_DELETE(m_terrainFloorVS);
    m_terrainFloorVertices.clear();
    m_terrainFloorVertexCount = 0;
}

void SceneNarakuProto::RebuildTerrainFloorBatch()
{
    std::size_t quadCapacity = m_ropePoints.size() * 2u + m_layerGates.size();
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }
        quadCapacity += static_cast<std::size_t>(layer.gridWidth - 1) *
            static_cast<std::size_t>(layer.gridHeight - 1);
    }

    SAFE_DELETE(m_terrainFloorMesh);
    m_terrainFloorVertices.clear();
    m_terrainFloorVertexCount = 0;
    if (quadCapacity == 0 || m_terrainFloorVS == nullptr || m_terrainFloorPS == nullptr)
    {
        return;
    }

    m_terrainFloorVertices.resize(quadCapacity * 6u);
    MeshBuffer::Description desc = {};
    desc.pVtx = m_terrainFloorVertices.data();
    desc.vtxSize = sizeof(TerrainFloorVertex);
    desc.vtxCount = static_cast<UINT>(m_terrainFloorVertices.size());
    desc.isWrite = true;
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_terrainFloorMesh = new MeshBuffer();
    if (FAILED(m_terrainFloorMesh->Create(desc)))
    {
        SAFE_DELETE(m_terrainFloorMesh);
        m_terrainFloorVertices.clear();
    }
}

void SceneNarakuProto::AppendTerrainFloorQuad(
    const DirectX::XMFLOAT3& center,
    const DirectX::XMFLOAT2& size,
    const DirectX::XMFLOAT4& color)
{
    if (m_terrainFloorVertexCount + 6u > m_terrainFloorVertices.size())
    {
        return;
    }

    const float halfWidth = size.x * 0.5f;
    const float halfDepth = size.y * 0.5f;
    const TerrainFloorVertex topLeft = { { center.x - halfWidth, center.y, center.z + halfDepth }, color };
    const TerrainFloorVertex topRight = { { center.x + halfWidth, center.y, center.z + halfDepth }, color };
    const TerrainFloorVertex bottomLeft = { { center.x - halfWidth, center.y, center.z - halfDepth }, color };
    const TerrainFloorVertex bottomRight = { { center.x + halfWidth, center.y, center.z - halfDepth }, color };

    TerrainFloorVertex* destination = m_terrainFloorVertices.data() + m_terrainFloorVertexCount;
    destination[0] = topLeft;
    destination[1] = topRight;
    destination[2] = bottomLeft;
    destination[3] = bottomLeft;
    destination[4] = topRight;
    destination[5] = bottomRight;
    m_terrainFloorVertexCount += 6u;
}

void SceneNarakuProto::DrawTerrainFloorBatch(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection)
{
    if (m_terrainFloorMesh == nullptr || m_terrainFloorVS == nullptr ||
        m_terrainFloorPS == nullptr || m_terrainFloorVertexCount == 0)
    {
        return;
    }

    DirectX::XMFLOAT4X4 matrices[2] = { view, projection };
    m_terrainFloorVS->WriteBuffer(0, matrices);
    m_terrainFloorVS->Bind();
    m_terrainFloorPS->Bind();
    m_terrainFloorMesh->Write(m_terrainFloorVertices.data());
    m_terrainFloorMesh->Draw(static_cast<int>(m_terrainFloorVertexCount));
}

void SceneNarakuProto::InitializeEnemyBillboardBatch()
{
    const char* vertexShaderCode = R"HLSL(
struct VS_IN {
    float3 position : POSITION0;
    float2 uv : TEXCOORD0;
};
struct VS_OUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};
cbuffer Matrix : register(b0) {
    float4x4 view;
    float4x4 projection;
};
VS_OUT main(VS_IN input) {
    VS_OUT output;
    output.position = mul(float4(input.position, 1.0f), view);
    output.position = mul(output.position, projection);
    output.uv = input.uv;
    return output;
})HLSL";

    const char* pixelShaderCode = R"HLSL(
struct PS_IN {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};
Texture2D enemyTexture : register(t0);
SamplerState enemySampler : register(s0);
float4 main(PS_IN input) : SV_TARGET {
    float4 color = enemyTexture.Sample(enemySampler, input.uv);
    clip(color.a - 0.01f);
    return color;
})HLSL";

    m_enemyBillboardVS = new VertexShader();
    if (FAILED(m_enemyBillboardVS->Compile(vertexShaderCode)))
    {
        SAFE_DELETE(m_enemyBillboardVS);
    }

    m_enemyBillboardPS = new PixelShader();
    if (FAILED(m_enemyBillboardPS->Compile(pixelShaderCode)))
    {
        SAFE_DELETE(m_enemyBillboardPS);
    }

    m_enemyTexture = new Texture();
    if (FAILED(m_enemyTexture->Create("Assets/Texture/Character/Enemy/enemy.png")))
    {
        SAFE_DELETE(m_enemyTexture);
    }

    if (m_enemyBillboardPS != nullptr && m_enemyTexture != nullptr)
    {
        m_enemyBillboardPS->SetTexture(0, m_enemyTexture);
    }
}

void SceneNarakuProto::ReleaseEnemyBillboardBatch()
{
    SAFE_DELETE(m_enemyBillboardMesh);
    SAFE_DELETE(m_enemyBillboardPS);
    SAFE_DELETE(m_enemyBillboardVS);
    SAFE_DELETE(m_enemyTexture);
    m_enemyBillboardVertices.clear();
    m_enemyBillboardVertexCount = 0;
}

void SceneNarakuProto::RebuildEnemyBillboardBatch()
{
    SAFE_DELETE(m_enemyBillboardMesh);
    m_enemyBillboardVertices.clear();
    m_enemyBillboardVertexCount = 0;
    if (m_enemies.empty() || m_enemyBillboardVS == nullptr ||
        m_enemyBillboardPS == nullptr || m_enemyTexture == nullptr)
    {
        return;
    }

    m_enemyBillboardVertices.resize(m_enemies.size() * 6u);
    MeshBuffer::Description desc = {};
    desc.pVtx = m_enemyBillboardVertices.data();
    desc.vtxSize = sizeof(EnemyBillboardVertex);
    desc.vtxCount = static_cast<UINT>(m_enemyBillboardVertices.size());
    desc.isWrite = true;
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_enemyBillboardMesh = new MeshBuffer();
    if (FAILED(m_enemyBillboardMesh->Create(desc)))
    {
        SAFE_DELETE(m_enemyBillboardMesh);
        m_enemyBillboardVertices.clear();
    }
}

void SceneNarakuProto::DrawEnemyBillboardBatch(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection)
{
    using namespace DirectX;

    if (m_enemyBillboardMesh == nullptr || m_enemyBillboardVS == nullptr ||
        m_enemyBillboardPS == nullptr || m_enemyTexture == nullptr)
    {
        return;
    }

    constexpr float billboardWidth = 1.2f;
    constexpr float billboardHeight = 1.2f;
    constexpr float uvLeft = 1.0f / 9.0f;
    constexpr float uvRight = 2.0f / 9.0f;
    constexpr float uvTop = 0.0f;
    constexpr float uvBottom = 1.0f / 16.0f;

    const XMMATRIX viewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&view));
    XMMATRIX billboard = XMMatrixInverse(nullptr, viewMatrix);
    billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    m_enemyBillboardVertexCount = 0;
    for (const EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive ||
            m_enemyBillboardVertexCount + 6u > m_enemyBillboardVertices.size())
        {
            continue;
        }

        const XMFLOAT3 center = ToWorld3D(
            enemy.pos,
            enemy.depth,
            billboardHeight * 0.5f);
        const XMMATRIX world =
            XMMatrixScaling(billboardWidth, billboardHeight, 1.0f) *
            billboard *
            XMMatrixTranslation(center.x, center.y, center.z);

        XMFLOAT3 topLeft = {};
        XMFLOAT3 topRight = {};
        XMFLOAT3 bottomLeft = {};
        XMFLOAT3 bottomRight = {};
        XMStoreFloat3(&topLeft, XMVector3TransformCoord(XMVectorSet(-0.5f, 0.5f, 0.0f, 1.0f), world));
        XMStoreFloat3(&topRight, XMVector3TransformCoord(XMVectorSet(0.5f, 0.5f, 0.0f, 1.0f), world));
        XMStoreFloat3(&bottomLeft, XMVector3TransformCoord(XMVectorSet(-0.5f, -0.5f, 0.0f, 1.0f), world));
        XMStoreFloat3(&bottomRight, XMVector3TransformCoord(XMVectorSet(0.5f, -0.5f, 0.0f, 1.0f), world));

        EnemyBillboardVertex* destination =
            m_enemyBillboardVertices.data() + m_enemyBillboardVertexCount;
        destination[0] = { topLeft, { uvLeft, uvTop } };
        destination[1] = { topRight, { uvRight, uvTop } };
        destination[2] = { bottomLeft, { uvLeft, uvBottom } };
        destination[3] = { bottomLeft, { uvLeft, uvBottom } };
        destination[4] = { topRight, { uvRight, uvTop } };
        destination[5] = { bottomRight, { uvRight, uvBottom } };
        m_enemyBillboardVertexCount += 6u;
    }

    if (m_enemyBillboardVertexCount == 0)
    {
        return;
    }

    XMFLOAT4X4 matrices[2] = { view, projection };
    m_enemyBillboardVS->WriteBuffer(0, matrices);
    m_enemyBillboardVS->Bind();
    m_enemyBillboardPS->Bind();
    m_enemyBillboardMesh->Write(m_enemyBillboardVertices.data());

    SetCullingMode(D3D11_CULL_NONE);
    SetBlendMode(BLEND_ALPHA);
    SetSamplerState(SAMPLER_POINT);
    m_enemyBillboardMesh->Draw(static_cast<int>(m_enemyBillboardVertexCount));
    SetSamplerState(SAMPLER_LINEAR);
}

bool SceneNarakuProto::ResetRun()
{
    int keepMoney = m_money;

    constexpr const wchar_t* generated4x4MapPath = L"Assets/Maps/generated_naraku_map_4x4.json";
    constexpr const wchar_t* generated3x3MapPath = L"Assets/Maps/generated_naraku_map.json";
    bool generated = false;
    std::string mapError;
    for (int attempt = 0; attempt < kMapGenerationMaxAttempts; ++attempt)
    {
        if (NarakuStageGenerator::GenerateFixed4x4AreaMap(generated4x4MapPath, false, true, &mapError) &&
            NarakuMap::LoadMap(generated4x4MapPath, m_runtimeMap, &mapError))
        {
            generated = true;
            NarakuMap::SetCurrentMapPath(generated4x4MapPath);
            break;
        }
    }

    if (!generated)
    {
        if (NarakuMap::LoadMap(generated3x3MapPath, m_runtimeMap, &mapError))
        {
            NarakuMap::SetCurrentMapPath(generated3x3MapPath);
        }
        else
        {
            NarakuMap::SetCurrentMapPath(NarakuMap::GetDefaultMapPath());
            if (!NarakuMap::LoadMap(NarakuMap::GetDefaultMapPath(), m_runtimeMap, &mapError))
            {
                m_runtimeMap = NarakuMap::CreateDefaultMap();
            }
        }
    }

    LoadEnvironmentModels();

    m_player = PlayerState();
    m_inventory.clear();
    m_foodCount = 0;
    m_groundRelics.clear();
    m_groundFoods.clear();
    m_miningPoints.clear();
    m_enemies.clear();
    m_attackHitEffects.clear();
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_layerGates.clear();
    m_areas.clear();
    m_currentAreaIndex = -1;
    m_activeRope = -1;
    m_ropeProgress = 0.0f;
    m_pins.clear();
    m_messages.clear();
    m_centerNotification.clear();
    m_centerNotificationTimer = 0.0f;
    m_loadingSourceGateIndex = -1;
    m_loadingStep = 0;
    m_loadingProgress = 0.0f;
    m_transitionSourceGateIndex = -1;
    m_transitionDestinationAreaIndex = -1;
    m_transitionDestinationGateIndex = -1;
    m_layerTransitionProgress = 0.0f;
    m_layerTransitionVisualOffset = 0.0f;
    m_layerTransitionAscending = false;
    m_heavyRunNotificationShown = false;
    m_shiftHold = 0.0f;
    m_shiftPendingStep = false;
    m_shiftWasPressed = false;
    m_shiftRunCommitted = false;
    m_cameraShakeTimer = 0.0f;
    m_miningTimer = 0.0f;
    m_miningDuration = kMiningTime;
    m_miningIndex = -1;
    m_selectedInventory = -1;
    m_mapScrollOffset = { 0.0f, 0.0f };
    m_mode = Mode::Explore;
    m_result = RunResult();
    m_pendingRelicDepth = 0.0f;
    m_money = keepMoney;

    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_worldHalfSize = 1.0f;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        const float halfWidth = static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f;
        const float halfHeight = static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f;
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.x) + halfWidth);
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.z) + halfHeight);
    }

    auto getLayerDepthById = [this](int layerId) -> float
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, layerId);
        return (layerIndex >= 0) ? m_runtimeMap.terrainLayers[layerIndex].layerDepth : 0.0f;
    };

    auto getFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    m_startPoint = { m_runtimeMap.playerStartPoint.xz.x, m_runtimeMap.playerStartPoint.xz.z };
    m_startDepth = getLayerDepthById(m_runtimeMap.playerStartPoint.layerId);
    m_returnPoint = m_startPoint;
    m_returnDepth = m_startDepth;

    m_player.pos = m_startPoint;
    m_player.depth = m_startDepth;
    m_player.previousDepth = m_startDepth;
    m_player.facing = { 0.0f, 1.0f };
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }

        const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
        const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
        m_floorRegions.push_back({
            { layer.center.x, layer.center.z },
            { width * 0.5f, height * 0.5f },
            layer.layerDepth,
            getFloorColor(layer.groundTextureId),
            layer.id });
    }

    for (const NarakuMap::RopePoint& rope : m_runtimeMap.ropes)
    {
        const int topIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.topLayerId);
        const int bottomIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        m_ropePoints.push_back({
            { rope.topXZ.x, rope.topXZ.z },
            { rope.bottomXZ.x, rope.bottomXZ.z },
            m_runtimeMap.terrainLayers[topIndex].layerDepth,
            m_runtimeMap.terrainLayers[bottomIndex].layerDepth });
    }

    for (const NarakuMap::LayerGatePoint& gate : m_runtimeMap.layerGates)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, gate.layerId);
        if (layerIndex < 0)
        {
            continue;
        }
        LayerGateState runtimeGate;
        runtimeGate.isEntry = gate.isEntry;
        runtimeGate.ropePos = { gate.ropeXZ.x, gate.ropeXZ.z };
        runtimeGate.loadPos = { gate.loadXZ.x, gate.loadXZ.z };
        runtimeGate.depth = m_runtimeMap.terrainLayers[layerIndex].layerDepth;
        m_layerGates.push_back(runtimeGate);
    }

    int fallbackRelicIndex = 0;
    for (const NarakuMap::MiningPoint& point : m_runtimeMap.miningPoints)
    {
        if (!point.enabled)
        {
            continue;
        }

        MiningPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.visualType = point.visualType;
        runtimePoint.discovered = point.discovered;
        /** 再潜行ごとに採掘状態を初期化し、今回の潜行中だけ更新します。 */
        runtimePoint.mined = false;
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.relicName = point.relicName.empty() ? kRelicNames[fallbackRelicIndex % 8] : point.relicName;

        ++fallbackRelicIndex;
        m_miningPoints.push_back(runtimePoint);
    }

    auto pickEnemyDepth = [this](const Vec2& pos) -> float
    {
        float bestDepth = m_startDepth;
        bool found = false;
        for (const FloorRegion& floor : m_floorRegions)
        {
            if (!IsInsideFloor(floor, pos))
            {
                continue;
            }
            if (!found || floor.depth < bestDepth)
            {
                bestDepth = floor.depth;
                found = true;
            }
        }
        return bestDepth;
    };

    EnemyState enemyA = {};
    enemyA.pos = { -8.0f, 11.0f };
    enemyA.depth = pickEnemyDepth(enemyA.pos);
    m_enemies.push_back(enemyA);

    EnemyState enemyB = {};
    enemyB.pos = { 13.0f, -8.0f };
    enemyB.depth = pickEnemyDepth(enemyB.pos);
    m_enemies.push_back(enemyB);

    EnemyState enemyC = {};
    enemyC.pos = { -18.0f, -13.0f };
    enemyC.depth = pickEnemyDepth(enemyC.pos);
    m_enemies.push_back(enemyC);

    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
    AddMessage(u8"奈落塔プロトタイプを開始しました。");
    if (generated)
    {
        m_areas.emplace_back();
        m_areas.front().depth = 1;
        m_currentAreaIndex = 0;
        SaveCurrentAreaState();
    }
    if (!generated)
    {
        m_mode = Mode::Home;
    }
    return generated;
}

void SceneNarakuProto::SaveCurrentAreaState()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    AreaState& area = m_areas[m_currentAreaIndex];
    area.map = m_runtimeMap;
    area.groundRelics = m_groundRelics;
    area.groundFoods = m_groundFoods;
    area.miningPoints = m_miningPoints;
    area.enemies = m_enemies;
    area.floorRegions = m_floorRegions;
    area.ropePoints = m_ropePoints;
    area.layerGates = m_layerGates;
    area.pins = m_pins;
    area.startPoint = m_startPoint;
    area.startDepth = m_startDepth;
    area.returnPoint = m_returnPoint;
    area.returnDepth = m_returnDepth;
    area.worldHalfSize = m_worldHalfSize;
}

void SceneNarakuProto::ActivateArea(int areaIndex, bool placeAtEntry)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    const AreaState& area = m_areas[areaIndex];
    m_currentAreaIndex = areaIndex;
    m_runtimeMap = area.map;
    m_groundRelics = area.groundRelics;
    m_groundFoods = area.groundFoods;
    m_miningPoints = area.miningPoints;
    m_enemies = area.enemies;
    m_floorRegions = area.floorRegions;
    m_ropePoints = area.ropePoints;
    m_layerGates = area.layerGates;
    m_pins = area.pins;
    m_startPoint = area.startPoint;
    m_startDepth = area.startDepth;
    m_returnPoint = area.returnPoint;
    m_returnDepth = area.returnDepth;
    m_worldHalfSize = area.worldHalfSize;
    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_activeRope = -1;
    m_player.onRope = false;

    if (placeAtEntry)
    {
        for (const LayerGateState& gate : m_layerGates)
        {
            if (!gate.isEntry)
            {
                continue;
            }
            m_player.pos = gate.ropePos;
            m_player.depth = gate.depth;
            break;
        }
    }
    m_player.previousDepth = m_player.depth;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.grounded = true;
    m_player.verticalSpeed = 0.0f;
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
}

void SceneNarakuProto::BuildCurrentAreaRuntime(bool placeAtStart)
{
    m_groundRelics.clear();
    m_groundFoods.clear();
    m_miningPoints.clear();
    m_enemies.clear();
    m_attackHitEffects.clear();
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_layerGates.clear();
    m_pins.clear();
    m_activeRope = -1;
    m_miningIndex = -1;
    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_worldHalfSize = 1.0f;

    auto getLayerDepthById = [this](int layerId) -> float
    {
        const int index = NarakuMap::FindLayerIndexById(m_runtimeMap, layerId);
        return index >= 0 ? m_runtimeMap.terrainLayers[index].layerDepth : 0.0f;
    };
    auto getFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    m_startPoint = { m_runtimeMap.playerStartPoint.xz.x, m_runtimeMap.playerStartPoint.xz.z };
    m_startDepth = getLayerDepthById(m_runtimeMap.playerStartPoint.layerId);
    m_returnPoint = m_startPoint;
    m_returnDepth = m_startDepth;

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }
        const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
        const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.x) + width * 0.5f);
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.z) + height * 0.5f);
        m_floorRegions.push_back({ { layer.center.x, layer.center.z }, { width * 0.5f, height * 0.5f },
            layer.layerDepth, getFloorColor(layer.groundTextureId), layer.id });
    }

    for (const NarakuMap::RopePoint& rope : m_runtimeMap.ropes)
    {
        const int top = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.topLayerId);
        const int bottom = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.bottomLayerId);
        if (top >= 0 && bottom >= 0)
        {
            m_ropePoints.push_back({ { rope.topXZ.x, rope.topXZ.z }, { rope.bottomXZ.x, rope.bottomXZ.z },
                m_runtimeMap.terrainLayers[top].layerDepth, m_runtimeMap.terrainLayers[bottom].layerDepth });
        }
    }

    for (const NarakuMap::LayerGatePoint& gate : m_runtimeMap.layerGates)
    {
        const int layer = NarakuMap::FindLayerIndexById(m_runtimeMap, gate.layerId);
        if (layer < 0)
        {
            continue;
        }
        LayerGateState runtimeGate;
        runtimeGate.isEntry = gate.isEntry;
        runtimeGate.ropePos = { gate.ropeXZ.x, gate.ropeXZ.z };
        runtimeGate.loadPos = { gate.loadXZ.x, gate.loadXZ.z };
        runtimeGate.depth = m_runtimeMap.terrainLayers[layer].layerDepth;
        m_layerGates.push_back(runtimeGate);
    }

    int relicIndex = 0;
    for (const NarakuMap::MiningPoint& point : m_runtimeMap.miningPoints)
    {
        if (!point.enabled)
        {
            continue;
        }
        MiningPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.visualType = point.visualType;
        runtimePoint.discovered = point.discovered;
        runtimePoint.mined = false;
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.relicName = point.relicName.empty() ? kRelicNames[relicIndex % 8] : point.relicName;
        ++relicIndex;
        m_miningPoints.push_back(runtimePoint);
    }

    const Vec2 enemyOffsets[] = { { -8.0f, 11.0f }, { 13.0f, -8.0f }, { -18.0f, -13.0f } };
    for (const Vec2& offset : enemyOffsets)
    {
        EnemyState enemy = {};
        enemy.pos = Add(m_startPoint, offset);
        enemy.depth = m_startDepth;
        m_enemies.push_back(enemy);
    }

    if (placeAtStart)
    {
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        for (const LayerGateState& gate : m_layerGates)
        {
            if (gate.isEntry)
            {
                m_player.pos = gate.ropePos;
                m_player.depth = gate.depth;
                break;
            }
        }
        m_player.previousDepth = m_player.depth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
    }
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
}

void SceneNarakuProto::TryUseLayerGate(int gateIndex)
{
    if (gateIndex < 0 || gateIndex >= static_cast<int>(m_layerGates.size()))
    {
        return;
    }

    LayerGateState& gate = m_layerGates[gateIndex];
    if (gate.disabled)
    {
        ShowCenterNotification(u8"この層間口は使用できない！");
        return;
    }
    if (gate.destinationAreaIndex >= 0)
    {
        BeginLayerTransition(gateIndex, gate.destinationAreaIndex);
        return;
    }
    if (gate.isEntry)
    {
        ShowCenterNotification(u8"接続先がありません。");
        return;
    }
    if (m_currentAreaIndex < 0 || m_areas[m_currentAreaIndex].depth >= kMaximumAreaDepth)
    {
        ShowCenterNotification(u8"これ以上深くは進めない！");
        return;
    }

    SaveCurrentAreaState();
    m_loadingSourceGateIndex = gateIndex;
    m_loadingStep = 0;
    m_loadingProgress = 0.05f;
    m_mode = Mode::Loading;
}

void SceneNarakuProto::BeginLayerTransition(int sourceGateIndex, int destinationAreaIndex)
{
    if (sourceGateIndex < 0 || sourceGateIndex >= static_cast<int>(m_layerGates.size()) ||
        destinationAreaIndex < 0 || destinationAreaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    int destinationGateIndex = -1;
    const std::vector<LayerGateState>& destinationGates = m_areas[destinationAreaIndex].layerGates;
    for (int i = 0; i < static_cast<int>(destinationGates.size()); ++i)
    {
        if (destinationGates[i].destinationAreaIndex == m_currentAreaIndex)
        {
            destinationGateIndex = i;
            break;
        }
    }
    if (destinationGateIndex < 0)
    {
        ShowCenterNotification(u8"層間口の接続情報が不正です。");
        return;
    }

    m_transitionSourceGateIndex = sourceGateIndex;
    m_transitionDestinationAreaIndex = destinationAreaIndex;
    m_transitionDestinationGateIndex = destinationGateIndex;
    m_layerTransitionProgress = 0.0f;
    m_layerTransitionVisualOffset = 0.0f;
    m_layerTransitionAscending = m_layerGates[sourceGateIndex].isEntry;
    m_player.pos = m_layerGates[sourceGateIndex].ropePos;
    m_player.depth = m_layerGates[sourceGateIndex].depth;
    m_player.onRope = false;
    m_mode = Mode::LayerTransition;
}

void SceneNarakuProto::UpdateLoading()
{
    if (m_loadingStep == 0)
    {
        m_loadingStep = 1;
        m_loadingProgress = 0.15f;
        return;
    }
    if (m_loadingStep != 1 || m_currentAreaIndex < 0 ||
        m_loadingSourceGateIndex < 0 || m_loadingSourceGateIndex >= static_cast<int>(m_layerGates.size()))
    {
        m_mode = Mode::Explore;
        return;
    }

    const int parentAreaIndex = m_currentAreaIndex;
    const int sourceGateIndex = m_loadingSourceGateIndex;
    const int childDepth = m_areas[parentAreaIndex].depth + 1;
    wchar_t generatedPath[128] = {};
    std::swprintf(generatedPath, 128, L"Assets/Maps/generated_area_%03d.json", static_cast<int>(m_areas.size()));

    NarakuMap::MapData childMap;
    std::string error;
    m_loadingProgress = 0.45f;
    const bool requireExit = childDepth < kMaximumAreaDepth;
    if (!NarakuStageGenerator::GenerateFixed4x4AreaMap(generatedPath, true, requireExit, &error) ||
        !NarakuMap::LoadMap(generatedPath, childMap, &error))
    {
        LayerGateState& sourceGate = m_layerGates[sourceGateIndex];
        ++sourceGate.generationFailures;
        sourceGate.disabled = sourceGate.generationFailures >= kLayerGateFailureLimit;
        SaveCurrentAreaState();
        m_mode = Mode::Explore;
        m_loadingProgress = 0.0f;
        m_loadingStep = 0;
        ShowCenterNotification(sourceGate.disabled
            ? u8"生成に5回失敗したため、この層間口は使用不能になりました。"
            : u8"下層の生成に失敗しました。もう一度Fで試せます。");
        return;
    }

    SaveCurrentAreaState();
    m_runtimeMap = std::move(childMap);
    BuildCurrentAreaRuntime(true);

    const int childAreaIndex = static_cast<int>(m_areas.size());
    m_areas.emplace_back();
    m_areas.back().depth = childDepth;
    m_currentAreaIndex = childAreaIndex;

    int childEntryIndex = -1;
    for (int i = 0; i < static_cast<int>(m_layerGates.size()); ++i)
    {
        if (m_layerGates[i].isEntry)
        {
            childEntryIndex = i;
            m_layerGates[i].destinationAreaIndex = parentAreaIndex;
            break;
        }
    }
    if (childEntryIndex < 0)
    {
        m_areas.pop_back();
        ActivateArea(parentAreaIndex, false);
        LayerGateState& sourceGate = m_layerGates[sourceGateIndex];
        ++sourceGate.generationFailures;
        sourceGate.disabled = sourceGate.generationFailures >= kLayerGateFailureLimit;
        SaveCurrentAreaState();
        m_mode = Mode::Explore;
        ShowCenterNotification(u8"生成結果に層入口がありません。もう一度Fで試せます。");
        return;
    }

    SaveCurrentAreaState();
    m_areas[parentAreaIndex].layerGates[sourceGateIndex].destinationAreaIndex = childAreaIndex;
    ActivateArea(parentAreaIndex, false);
    m_layerGates[sourceGateIndex].destinationAreaIndex = childAreaIndex;
    SaveCurrentAreaState();
    m_loadingProgress = 1.0f;
    m_loadingStep = 2;
    BeginLayerTransition(sourceGateIndex, childAreaIndex);
}

void SceneNarakuProto::UpdateLayerTransition(float dt)
{
    m_layerTransitionProgress = std::min(1.0f, m_layerTransitionProgress + dt / kLayerTransitionDuration);
    const float direction = m_layerTransitionAscending ? 1.0f : -1.0f;
    m_layerTransitionVisualOffset = direction * kLayerTransitionHeight * m_layerTransitionProgress;

    if (m_layerTransitionAscending)
    {
        m_player.upperLoad += kLayerTransitionHeight * dt / kLayerTransitionDuration;
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            m_player.mental = std::max(0.0f, m_player.mental - 10.0f);
            m_player.upperLoad = 0.0f;
            AddMessage(u8"上昇負荷が発症しました。精神力-10。");
        }
    }

    if (m_layerTransitionProgress < 1.0f)
    {
        return;
    }

    SaveCurrentAreaState();
    const int destinationArea = m_transitionDestinationAreaIndex;
    const int destinationGate = m_transitionDestinationGateIndex;
    ActivateArea(destinationArea, false);
    if (destinationGate >= 0 && destinationGate < static_cast<int>(m_layerGates.size()))
    {
        m_player.pos = m_layerGates[destinationGate].ropePos;
        m_player.depth = m_layerGates[destinationGate].depth;
        m_player.previousDepth = m_player.depth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
    }
    m_layerTransitionVisualOffset = 0.0f;
    m_transitionSourceGateIndex = -1;
    m_transitionDestinationAreaIndex = -1;
    m_transitionDestinationGateIndex = -1;
    m_mode = Mode::Explore;
}

void SceneNarakuProto::Update()
{
    m_centerNotificationTimer = std::max(0.0f, m_centerNotificationTimer - kDt);

    if (m_mode == Mode::Loading)
    {
        UpdateLoading();
        return;
    }
    if (m_mode == Mode::LayerTransition)
    {
        UpdateLayerTransition(kDt);
        return;
    }

    // Cキーで当たり判定デバッグ表示を切り替えます。
    if (IsKeyTrigger('C'))
    {
        m_showCollisionDebug = !m_showCollisionDebug;
        AddMessage(m_showCollisionDebug ? "Collision Debug: ON" : "Collision Debug: OFF");
    }

    // Tキーで探索と所持品表示を切り替えます。
    if (IsKeyTrigger('T'))
    {
        // 探索中なら所持品画面へ移ります。
        if (m_mode == Mode::Explore) m_mode = Mode::Inventory;

        // 所持品表示中なら探索へ戻ります。
        else if (m_mode == Mode::Inventory) m_mode = Mode::Explore;
    }

    // 探索モード中だけプレイヤーや敵などのゲーム更新を進めます。
    if (m_mode == Mode::Explore)
    {
        if (IsKeyTrigger('E')) UseFood();
        UpdateExplore(kDt);
    }
}

void SceneNarakuProto::UpdateExplore(float dt)
{
    // 探索モード中だけ右ドラッグによるカメラ回転を受け付けます。
    UpdateCameraControls();

    // ImGui からの変更値が不正でもゲーム進行が壊れないよう毎フレーム丸めます。
    ClampDebugPlayerParams();

    // 今フレームの上昇量を後で計算できるよう、更新前の深度を保存します。
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.onRope && m_activeRope >= 0
        ? GetRopeWorldY(m_activeRope, m_ropeProgress)
        : m_player.feetWorldY;

    // 入力に応じてプレイヤーの移動と行動制限を更新します。
    UpdateMovement(dt);

    // 攻撃タイマーとスタミナ回復を更新します。
    UpdateAction(dt);

    // 採掘中なら採掘タイマーを進めます。
    UpdateMining(dt);

    // 敵の追跡と体当たりを更新します。
    UpdateEnemies(dt);

    if (HasRelicArmorSetEffect() && m_player.hp > 0.0f)
    {
        m_player.hp = std::min(10.0f, m_player.hp + kRelicArmorHpRecoveryPerSecond * dt);
    }

    // 近くの未発見採掘ポイントを発見済みにします。
    DiscoverNearbyMiningPoints();

    // 深度変化から上昇負荷を更新します。
    UpdateUpperLoad(dt);

    // リザルト用に今回の最大深度を記録します。
    m_result.maxDepth = std::max(m_result.maxDepth, m_player.depth);

    // Fキーで帰還、ロープ、拾う、採掘のいずれかを試します。
    if (IsKeyTrigger('F')) TryInteract();

    // 体力が0以下になったら死亡リザルトへ移行します。
    if (m_player.hp <= 0.0f && m_mode == Mode::Explore) StartDeath(u8"体力が0になりました。");
}

void SceneNarakuProto::UpdateMovement(float dt)
{
    // WASD入力を集めるための移動ベクトルです。
    Vec2 input;

    // 採掘中は移動やジャンプ、ステップ入力を受け付けません。
    const bool isMining = m_miningIndex >= 0;

    // 着地直後の硬直を更新します。
    m_player.landingRecoveryTimer = std::max(0.0f, m_player.landingRecoveryTimer - dt);
    const bool inLandingRecovery = m_player.landingRecoveryTimer > 0.0f;

    // Wキーでカメラから見た前方向へ進みます。
    if (!isMining && IsKeyPress('W')) input.y += 1.0f;

    // Sキーでカメラから見た後ろ方向へ進みます。
    if (!isMining && IsKeyPress('S')) input.y -= 1.0f;

    // Aキーでカメラから見た左方向へ進みます。
    if (!isMining && IsKeyPress('A')) input.x -= 1.0f;

    // Dキーでカメラから見た右方向へ進みます。
    if (!isMining && IsKeyPress('D')) input.x += 1.0f;

    // カメラから見た前方向を、斜め投影で画面上方向に見えるワールド方向へ対応させます。
    const Vec2 cameraForward = GetCameraForward();

    // カメラから見た右方向を、現在の3Dカメラで画面右方向に見えるワールド方向へ対応させます。
    const Vec2 cameraRight = GetCameraRight();

    // 入力をカメラ基準方向からワールド移動方向へ変換します。
    Vec2 move = Add(Mul(cameraRight, input.x), Mul(cameraForward, input.y));

    // 斜め移動が速くならないように正規化します。
    move = Normalize(move);

    // 入力がある時だけ向きを更新して、停止中の攻撃方向を維持します。
    if (move.x != 0.0f || move.y != 0.0f) m_player.facing = move;

    // Spaceが押された瞬間にジャンプ開始を試します。
    if (!isMining && !inLandingRecovery && IsKeyTrigger(VK_SPACE)) TryStartJump();

    // 左右Shiftの現在入力を物理キーとして取得します。
    const bool shiftPressed = !isMining && IsShiftPress();

    // 前フレームは押されておらず、今フレーム押されたらShift押下開始です。
    const bool shiftStarted = shiftPressed && !m_shiftWasPressed;

    // 前フレームは押されていて、今フレーム押されていなければShiftを離した瞬間です。
    const bool shiftReleased = !shiftPressed && m_shiftWasPressed;

    // Shiftが押された瞬間に「短押しならステップ」判定を保留します。
    if (shiftStarted)
    {
        // すぐにはステップせず、短押しか長押しかの判定を待ちます。
        m_shiftPendingStep = true;

        // 押下時間を0から測り直します。
        m_shiftHold = 0.0f;

        // まだ走りとして確定していない状態に戻します。
        m_shiftRunCommitted = false;
    }

    // このフレームで走り入力として扱うかどうかです。
    bool wantsRun = false;

    // Shift短押し/長押しの判定中なら押下時間を見ます。
    if (m_shiftPendingStep)
    {
        // Shiftを押し続けている時間を加算します。
        m_shiftHold += dt;

        // 閾値前に離されたら短押しステップとして扱います。
        if (shiftReleased)
        {
            // 長押し走りに確定していない短押しだけステップを開始します。
            if (!m_shiftRunCommitted && !inLandingRecovery)
            {
                // スタミナや重量条件を満たす場合だけステップを開始します。
                TryStartStep();
            }

            // 判定が終わったので保留を解除します。
            m_shiftPendingStep = false;

            // 走り確定状態も解除します。
            m_shiftRunCommitted = false;
        }

        // 閾値以上押されていたら長押し走りとして扱います。
        else if (m_shiftHold >= kShiftRunThreshold)
        {
            // このフレームの移動処理で走り速度を使わせます。
            wantsRun = true;

            // 閾値を超えたので、このShift入力はステップではなく走りとして確定します。
            m_shiftRunCommitted = true;
        }
    }

    // すでに長押し扱いになった後もShiftを押している間は走り扱いにします。
    else if (shiftPressed)
    {
        // このフレームの移動処理で走り速度を使わせます。
        wantsRun = true;
    }

    // Shiftが押されていないなら短押し判定状態を完全に解除します。
    if (!shiftPressed)
    {
        // ステップ保留を解除します。
        m_shiftPendingStep = false;

        // Shift押下時間も0に戻します。
        m_shiftHold = 0.0f;

        // 走り確定状態を解除します。
        m_shiftRunCommitted = false;
    }

    // 地面歩行開始時点の位置と高さを覚えて、段差踏み外し時の落下開始判定に使います。
    const Vec2 groundedMoveStartPos = m_player.pos;
    const float groundedMoveStartGroundY = GetGroundWorldY(m_player.pos, m_player.depth);

    // ノックバック中は敵から押し出される移動を先に適用します。
    if (m_player.knockbackTimer > 0.0f)
    {
        // ノックバック速度ぶんプレイヤー座標をずらします。
        const Vec2 knockbackTarget = Add(m_player.pos, Mul(m_player.knockbackVelocity, dt));
        m_player.pos = ResolveFloorMove(m_player.pos, knockbackTarget, m_player.depth);

        // ノックバック残り時間を減らします。
        m_player.knockbackTimer = std::max(0.0f, m_player.knockbackTimer - dt);
    }

    // ステップ中は通常移動よりステップ移動を優先します。
    if (m_player.stepTimer > 0.0f)
    {
        // このフレーム開始時点の残り時間を保存します。
        float previous = m_player.stepTimer;

        // ステップ残り時間を減らします。
        m_player.stepTimer = std::max(0.0f, m_player.stepTimer - dt);

        // 無敵時間中だけ高速移動し、後硬直中は移動しません。
        if (previous > kStepRecoveryTime)
        {
            // 0.5秒で指定距離を進むようにステップ速度を計算します。
            const Vec2 stepTarget = Add(m_player.pos, Mul(m_player.facing, (kStepDistance / kStepInvincibleTime) * dt));
            m_player.pos = ResolveFloorMove(m_player.pos, stepTarget, m_player.depth);
        }
    }

    // ロープに掴まっていない時だけ平面移動を行います。
    else if (!m_player.onRope && !inLandingRecovery)
    {
        // 通常歩行速度に重量70%以上の低下を反映します。
        float speed = GetMoveSpeed();

        // 100%以上の重量では走れないため、ここで走行可否を判定します。
        const bool tooHeavyToRun = GetCurrentWeight() >= GetMaxWeight();
        bool canRun = !tooHeavyToRun && m_player.stamina > 0.0f;

        if (wantsRun && tooHeavyToRun && (move.x != 0.0f || move.y != 0.0f))
        {
            if (!m_heavyRunNotificationShown)
            {
                ShowCenterNotification(u8"重すぎて走れない！");
                m_heavyRunNotificationShown = true;
            }
        }
        else
        {
            m_heavyRunNotificationShown = false;
        }

        // 走り入力、移動入力、重量、スタミナをすべて満たした時だけ走ります。
        if (wantsRun && canRun && (move.x != 0.0f || move.y != 0.0f))
        {
            // 走り速度へ切り替えます。
            speed = m_debugPlayerParams.runSpeed *
                (HasRelicArmorSetEffect() ? kRelicArmorRunMultiplier : 1.0f);

            // 1フレームぶんの走りスタミナを消費します。
            SpendStamina(m_debugPlayerParams.runCostPerSecond * dt);
        }

        // 決まった速度で平面座標を進めます。
        const Vec2 moveTarget = Add(m_player.pos, Mul(move, speed * dt));
        m_player.pos = ResolveFloorMove(m_player.pos, moveTarget, m_player.depth);
    }

    // ロープに掴まっている時はW/Sを深度操作として扱います。
    if (m_player.onRope)
    {
        // ロープ番号が無効なら、操作不能にならないよう即座にロープ状態を解除します。
        if (m_activeRope < 0 || m_activeRope >= static_cast<int>(m_ropePoints.size()))
        {
            m_player.onRope = false;
            m_activeRope = -1;
            return;
        }

        // 現在つかまっているロープの上端/下端深度を参照します。
        const RopePoint& rope = m_ropePoints[m_activeRope];

        // 深度入力を一時的に保持します。
        float depthInput = 0.0f;

        // Wは上昇なので深度を減らします。
        if (!isMining && IsKeyPress('W')) depthInput -= 1.0f;

        // Sは下降なので深度を増やします。
        if (!isMining && IsKeyPress('S')) depthInput += 1.0f;

        // A/Dが押された瞬間はスタミナがなくてもロープから横へ離脱できるようにします。
        const bool wantsLeaveRope = !isMining && (IsKeyTrigger('A') || IsKeyTrigger('D'));

        // ロープから離れる時は、掴まり状態を解除して少しだけ横へずらします。
        if (wantsLeaveRope)
        {
            // Aなら左、Dなら右へ離れる方向を決めます。
            const float leaveSign = IsKeyTrigger('A') ? -1.0f : 1.0f;

            // 現在深度に横へ降りられる床がある時だけロープを離します。
            if (!TryLeaveRopeSide(m_activeRope, leaveSign, cameraRight))
            {
                AddMessage(u8"足場がないためロープを離せません。");
            }
        }

        // 深度入力があり、スタミナを払える時だけ昇降します。
        if (m_player.onRope && depthInput != 0.0f && CanSpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt))
        {
            // ロープ昇降のスタミナを消費します。
            SpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt);

            const float topWorldY = GetGroundWorldY(rope.topPos, rope.topDepth);
            const float bottomWorldY = GetGroundWorldY(rope.bottomPos, rope.bottomDepth);
            const float horizontalLength = Distance(rope.topPos, rope.bottomPos);
            const float verticalLength = bottomWorldY - topWorldY;
            const float ropeLength = std::max(0.001f, std::sqrt(horizontalLength * horizontalLength + verticalLength * verticalLength));
            m_ropeProgress = std::max(0.0f, std::min(1.0f,
                m_ropeProgress + depthInput * m_debugPlayerParams.ropeSpeed * dt / ropeLength));
            m_player.pos = GetRopePosition(m_activeRope, m_ropeProgress);
            m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
            m_player.feetWorldY = GetRopeWorldY(m_activeRope, m_ropeProgress);
        }
    }

    // 地面を歩いてより低い地形へ出た時は、一定以上の落差で落下状態へ移ります。
    if (m_player.grounded && !m_player.onRope)
    {
        const float currentGroundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        const float walkedDropHeight = groundedMoveStartGroundY - currentGroundWorldY;
        const bool movedHorizontally = Distance(groundedMoveStartPos, m_player.pos) > 0.01f;

        if (movedHorizontally && walkedDropHeight >= m_autoFallStartHeight)
        {
            m_player.grounded = false;
            m_player.airTime = 0.0f;
            m_player.verticalSpeed = 0.0f;
            m_player.feetWorldY = groundedMoveStartGroundY;
            m_player.peakFeetWorldY = groundedMoveStartGroundY;
            m_player.landingRecoveryTimer = 0.0f;
        }
    }

    // ジャンプ中なら実地形の高さを使って空中時間と着地を更新します。
    if (!m_player.grounded)
    {
        const float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);

        // 空中にいる時間を加算します。
        m_player.airTime += dt;

        // 足元の絶対ワールド高さを縦速度ぶん進めます。
        m_player.feetWorldY += m_player.verticalSpeed * dt;

        // 重力で縦速度を減らします。
        m_player.verticalSpeed -= 9.8f * dt;

        // 最高到達点を更新して、着地時の落下距離計算に使います。
        m_player.peakFeetWorldY = std::max(m_player.peakFeetWorldY, m_player.feetWorldY);

        // 降下中に地面へ届いたら着地します。
        if (m_player.verticalSpeed <= 0.0f && m_player.feetWorldY <= groundWorldY)
        {
            const float fallDistance = std::max(0.0f, m_player.peakFeetWorldY - groundWorldY);
            float landingRecovery = 0.0f;
            if (fallDistance >= 6.0f) landingRecovery = kLandingRecoveryHeavy;
            else if (fallDistance >= 4.0f) landingRecovery = kLandingRecoveryMedium;
            else if (fallDistance >= 2.0f) landingRecovery = kLandingRecoveryLight;

            if (fallDistance >= 8.0f) StartDeath(u8"落下死しました。");
            else if (fallDistance >= 6.0f) m_player.hp -= 6.0f;
            else if (fallDistance >= 4.0f) m_player.hp -= 3.0f;
            else if (fallDistance >= 2.0f) m_player.hp -= 1.0f;

            m_player.grounded = true;
            m_player.airTime = 0.0f;
            m_player.verticalSpeed = 0.0f;
            m_player.feetWorldY = groundWorldY;
            m_player.peakFeetWorldY = groundWorldY;
            m_player.landingRecoveryTimer = landingRecovery;
        }
    }
    else if (!m_player.onRope)
    {
        const float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.feetWorldY = groundWorldY;
        m_player.peakFeetWorldY = groundWorldY;
    }

    // デバッグフィールド外へ出ないようX座標を制限します。
    m_player.pos.x = std::max(-m_worldHalfSize, std::min(m_player.pos.x, m_worldHalfSize));

    // デバッグフィールド外へ出ないようY座標を制限します。
    m_player.pos.y = std::max(-m_worldHalfSize, std::min(m_player.pos.y, m_worldHalfSize));

    // 危険地形に乗っている間は一定間隔でダメージを受けます。
    if (!m_player.onRope && (GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeHazard) != 0u)
    {
        m_hazardTickTimer -= dt;
        if (m_hazardTickTimer <= 0.0f)
        {
            m_hazardTickTimer += kHazardTickInterval;
            m_player.hp = std::max(0.0f, m_player.hp - kHazardDamage);
            AddMessage(u8"危険地形でダメージを受けました。");
        }
    }
    else
    {
        m_hazardTickTimer = kHazardTickInterval;
    }

    // 次フレームで押下/離上を判定できるよう、現在のShift状態を保存します。
    m_shiftWasPressed = shiftPressed;
}

void SceneNarakuProto::UpdateAction(float dt)
{
    m_cameraShakeTimer = std::max(0.0f, m_cameraShakeTimer - dt);
    for (AttackHitEffect& effect : m_attackHitEffects)
    {
        effect.remainingTime -= dt;
    }
    m_attackHitEffects.erase(
        std::remove_if(
            m_attackHitEffects.begin(),
            m_attackHitEffects.end(),
            [](const AttackHitEffect& effect) { return effect.remainingTime <= 0.0f; }),
        m_attackHitEffects.end());

    // 攻撃中なら攻撃タイマーを進めます。
    if (m_player.attackTimer > 0.0f)
    {
        // 前フレーム時点の残り時間を保存し、アクティブ判定の突入を検出します。
        float previous = m_player.attackTimer;

        // 攻撃残り時間を減らします。
        m_player.attackTimer = std::max(0.0f, m_player.attackTimer - dt);

        // 攻撃開始からの経過時間を計算します。
        float elapsed = kAttackTotal - m_player.attackTimer;

        // 前フレーム時点の攻撃開始からの経過時間を計算します。
        float previousElapsed = kAttackTotal - previous;

        // このフレームで攻撃判定時間に入った、または判定時間中かを見ます。
        bool activeThisFrame = previousElapsed < kAttackStartup + kAttackActive && elapsed >= kAttackStartup;

        // 攻撃判定が有効なフレームだけ敵との当たり判定を取ります。
        if (activeThisFrame)
        {
            // すべての敵に対して攻撃が当たるか調べます。
            for (EnemyState& enemy : m_enemies)
            {
                // 死んでいる敵は判定しません。
                if (!enemy.alive || enemy.hitByPlayerAttack) continue;

                // プレイヤーから敵への方向を計算します。
                Vec2 toEnemy = Sub(enemy.pos, m_player.pos);

                // 射程内かつ前方にいる場合だけヒットさせます。
                if (Distance(enemy.pos, m_player.pos) <= kAttackRange && Dot(Normalize(toEnemy), m_player.facing) > 0.25f)
                {
                    enemy.hitByPlayerAttack = true;
                    m_attackHitEffects.push_back({ enemy.pos, enemy.depth, kAttackHitEffectDuration });

                    // つるはし1回ぶんのダメージを与えます。
                    enemy.hp -= m_debugPlayerParams.attackPower;

                    // HPが0以下なら敵を倒します。
                    if (enemy.hp <= 0.0f)
                    {
                        // 敵を非アクティブにします。
                        enemy.alive = false;

                        // プロトでは商店販売品と同じ食料を敵位置へ落とします。
                        m_groundFoods.push_back({ enemy.pos, enemy.depth, true });
                        AddMessage(u8"敵を倒しました。食料を落としました。");
                    }
                }
            }
        }
    }

    // 左クリックが押された瞬間に攻撃開始を試します。
    if (IsMouseLeftTrigger()) TryStartAttack();

    // スタミナが最大未満なら自然回復できるかを判定します。
    if (m_player.stamina < 100.0f && m_player.attackTimer <= 0.0f && m_miningIndex < 0)
    {
        // Shift中またはロープ中はスタミナ消費行動中として回復を止めます。
        // ロープは掴まっているだけなら回復し、実際に昇降できる入力中だけ消費行動として扱います。
        const bool ropeClimbing = m_player.onRope && (IsKeyPress('W') || IsKeyPress('S')) && CanSpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt);

        // Shift中またはロープ昇降中はスタミナ消費行動中として回復を止めます。
        bool spending = IsShiftPress() || ropeClimbing;

        // 消費行動中でなければ1フレームぶん回復します。
        if (!spending) m_player.stamina = std::min(100.0f, m_player.stamina + m_debugPlayerParams.staminaRecoverPerSecond * dt);
    }
}

void SceneNarakuProto::UpdateMining(float dt)
{
    // 採掘中でなければ何もしません。
    if (m_miningIndex < 0) return;

    // 被弾ノックバック、足場喪失（空中）、またはロープに掴まった場合は採掘を中断します。
    if (m_player.knockbackTimer > 0.0f)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        AddMessage(u8"ダメージを受けたため、採掘が中断されました。");
        return;
    }
    if (!m_player.grounded)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        AddMessage(u8"足場を失ったため、採掘が中断されました。");
        return;
    }
    if (m_player.onRope)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        return;
    }

    // 採掘完了までの残り時間を減らします。
    m_miningTimer -= dt;

    // まだ採掘が終わっていなければ戻ります。
    if (m_miningTimer > 0.0f) return;

    // 採掘中だったポイントを取得します。
    MiningPoint& point = m_miningPoints[m_miningIndex];

    // このポイントを採掘済みにします。
    point.mined = true;
    point.discovered = true;

    // リザルト用の採掘数を増やします。
    ++m_result.minedCount;

    // 発見確認に出す旧器を作ります。
    // 種類と重量は採掘した時点で確定し、鑑定状態は表示名だけに反映します。
    m_pendingRelic = CreateRandomRelic(point.relicName);

    // 拾わず置く場合に戻す位置を採掘ポイント位置にします。
    m_pendingRelicPos = point.pos;
    m_pendingRelicDepth = point.depth;

    // 採掘中番号を解除します。
    m_miningIndex = -1;

    // 採掘タイマーを0に戻します。
    m_miningTimer = 0.0f;

    // 旧器を拾うかどうかの確認画面へ移ります。
    m_mode = Mode::RelicPrompt;

    // HUDログに発見を出します。
    AddMessage(u8"旧器を発見しました。");
}

void SceneNarakuProto::UpdateEnemies(float dt)
{
    // 登録されている敵を順番に更新します。
    for (EnemyState& enemy : m_enemies)
    {
        // 死んでいる敵は移動も攻撃もしません。
        if (!enemy.alive) continue;

        enemy.landingRecoveryTimer = std::max(0.0f, enemy.landingRecoveryTimer - dt);

        // 現在フレームの移動前の地面高さを覚えて、段差踏み外し時の落下開始判定に使います。
        const Vec2 moveStartPos = enemy.pos;
        const float moveStartGroundY = GetGroundWorldY(enemy.pos, enemy.depth);

        // 空中にいる敵は落下更新だけ行い、着地するまでは通常AIを止めます。
        if (!enemy.grounded)
        {
            const float groundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
            enemy.airTime += dt;
            enemy.feetWorldY += enemy.verticalSpeed * dt;
            enemy.verticalSpeed -= 9.8f * dt;
            enemy.peakFeetWorldY = std::max(enemy.peakFeetWorldY, enemy.feetWorldY);

            if (enemy.verticalSpeed <= 0.0f && enemy.feetWorldY <= groundWorldY)
            {
                const float fallDistance = std::max(0.0f, enemy.peakFeetWorldY - groundWorldY);
                float landingRecovery = 0.0f;
                if (fallDistance >= 6.0f) landingRecovery = kLandingRecoveryHeavy;
                else if (fallDistance >= 4.0f) landingRecovery = kLandingRecoveryMedium;
                else if (fallDistance >= 2.0f) landingRecovery = kLandingRecoveryLight;

                enemy.grounded = true;
                enemy.airTime = 0.0f;
                enemy.verticalSpeed = 0.0f;
                enemy.feetWorldY = groundWorldY;
                enemy.peakFeetWorldY = groundWorldY;
                enemy.landingRecoveryTimer = landingRecovery;
            }

            continue;
        }

        if (enemy.landingRecoveryTimer > 0.0f)
        {
            enemy.feetWorldY = moveStartGroundY;
            enemy.peakFeetWorldY = moveStartGroundY;
            continue;
        }

        // 体当たり中なら専用の高速移動と命中判定を行います。
        if (enemy.chargeTimer > 0.0f)
        {
            const float chargeProgress = std::max(
                0.0f,
                std::min(1.0f, 1.0f - enemy.chargeTimer / kEnemyChargeTime));
            const float chargeSpeedScale = kEnemyChargeStartSpeedScale +
                (kEnemyChargeEndSpeedScale - kEnemyChargeStartSpeedScale) * chargeProgress;
            const Vec2 chargeTarget = Add(
                enemy.pos,
                Mul(enemy.chargeDir, kEnemyChargeSpeed * chargeSpeedScale * dt));
            enemy.pos = ResolveFloorMove(enemy.pos, chargeTarget, enemy.depth);

            // 体当たりの残り時間を減らします。
            enemy.chargeTimer = std::max(0.0f, enemy.chargeTimer - dt);

            // 同じ深度帯にいる場合だけ命中判定を行います。
            const bool sameDepthAsPlayer = std::fabs(enemy.depth - m_player.depth) <= 0.35f;
            if (sameDepthAsPlayer && !enemy.hasHitThisCharge && Distance(enemy.pos, m_player.pos) <= kEnemyHitRange)
            {
                // ステップ無敵時間中かどうかを判定します。
                bool invincible = m_player.stepTimer > kStepRecoveryTime;

                // 無敵でなければ体当たりダメージとノックバックを与えます。
                if (!invincible)
                {
                    // 体当たり1回ぶんのダメージを与えます。
                    m_player.hp -= 1.0f;
                    m_cameraShakeTimer = kCameraShakeDuration;

                    // ノックバック時間を設定します。
                    m_player.knockbackTimer = kKnockbackTime;

                    // 敵からプレイヤーへ押し出す方向を計算します。
                    Vec2 dir = Normalize(Sub(m_player.pos, enemy.pos));

                    // 指定距離を指定時間で押し出す速度を設定します。
                    m_player.knockbackVelocity = Mul(dir, kKnockbackDistance / kKnockbackTime);

                    // HUDログに被弾を出します。
                    AddMessage(u8"敵の体当たりを受けました。");
                }

                // この体当たりではもう追加ヒットしないようにします。
                enemy.hasHitThisCharge = true;
            }

            // 体当たり中の敵は通常追跡を行わないため、この敵の更新を終えます。
            continue;
        }

        // 予備動作中ならタイマーを進め、終了時に体当たりへ移ります。
        if (enemy.telegraphTimer > 0.0f)
        {
            // 予備動作の残り時間を減らします。
            enemy.telegraphTimer = std::max(0.0f, enemy.telegraphTimer - dt);

            // 予備動作が終わった瞬間に体当たり方向を決定します。
            if (enemy.telegraphTimer <= 0.0f)
            {
                // 現在のプレイヤー位置へ向かって突進します。
                enemy.chargeDir = Normalize(Sub(m_player.pos, enemy.pos));

                // 体当たり時間を設定します。
                enemy.chargeTimer = kEnemyChargeTime;

                // 体当たりヒット済みフラグをリセットします。
                enemy.hasHitThisCharge = false;
            }

            // 予備動作中は通常追跡を行わないため、この敵の更新を終えます。
            continue;
        }

        // 同じ深度帯のプレイヤーだけ追跡対象にします。
        const bool sameDepthAsPlayer = std::fabs(enemy.depth - m_player.depth) <= 0.35f;

        // 敵からプレイヤーへの方向を計算します。
        Vec2 toPlayer = Sub(m_player.pos, enemy.pos);

        // 敵とプレイヤーの距離を計算します。
        float dist = Distance(enemy.pos, m_player.pos);

        // 同じ深度帯で近すぎない場合はプレイヤーへゆっくり接近します。
        if (sameDepthAsPlayer && dist > 0.1f)
        {
            const Vec2 moveTarget = Add(enemy.pos, Mul(Normalize(toPlayer), kEnemyWalkSpeed * dt));
            enemy.pos = ResolveFloorMove(enemy.pos, moveTarget, enemy.depth);
        }

        // 体当たりの再使用待ち時間を減らします。
        enemy.attackCooldown -= dt;

        // クールダウンが終わり、同じ深度帯で十分近ければ予備動作を開始します。
        if (sameDepthAsPlayer && enemy.attackCooldown <= 0.0f && dist <= 3.0f)
        {
            // 予備動作タイマーを設定します。
            enemy.telegraphTimer = kEnemyTelegraphTime;

            // 次回攻撃までのクールダウンを5秒に戻します。
            enemy.attackCooldown = kEnemyAttackInterval;
        }

        // 地面歩行でより低い地形へ出た時は、一定以上の落差で敵も落下状態へ移ります。
        const float currentGroundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
        const float walkedDropHeight = moveStartGroundY - currentGroundWorldY;
        const bool movedHorizontally = Distance(moveStartPos, enemy.pos) > 0.01f;
        if (movedHorizontally && walkedDropHeight >= m_autoFallStartHeight)
        {
            enemy.grounded = false;
            enemy.airTime = 0.0f;
            enemy.verticalSpeed = 0.0f;
            enemy.feetWorldY = moveStartGroundY;
            enemy.peakFeetWorldY = moveStartGroundY;
            enemy.landingRecoveryTimer = 0.0f;
            enemy.telegraphTimer = 0.0f;
            enemy.chargeTimer = 0.0f;
            enemy.hasHitThisCharge = false;
            continue;
        }

        enemy.feetWorldY = currentGroundWorldY;
        enemy.peakFeetWorldY = currentGroundWorldY;
    }
}

void SceneNarakuProto::UpdateUpperLoad(float dt)
{
    const float currentWorldY = m_player.onRope && m_activeRope >= 0
        ? GetRopeWorldY(m_activeRope, m_ropeProgress)
        : m_player.feetWorldY;
    const float ascent = (m_player.grounded || m_player.onRope)
        ? currentWorldY - m_player.previousWorldY
        : 0.0f;

    // 上昇している場合は上昇負荷ゲージを加算します。
    if (ascent > 0.0f)
    {
        // 上昇量そのものを内部ゲージに加算します。
        m_player.upperLoad += ascent;

        // 第一層の発症目安10mに達したら発症します。
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            // プロト仕様として精神力を10減らします。
            m_player.mental = std::max(0.0f, m_player.mental - 10.0f);

            // 発症後は内部ゲージを0へ戻します。
            m_player.upperLoad = 0.0f;

            // HUDログに発症を出します。
            AddMessage(u8"上昇負荷が発症しました。精神力-10。");

            // 精神力が0になったら精神崩壊扱いで死亡リザルトへ移します。
            if (m_player.mental <= 0.0f) StartDeath(u8"精神崩壊");
        }
    }

    // 上昇していない場合は停止、平地、下降のすべてで一定速度回復します。
    else
    {
        // 1秒あたり1mぶん内部ゲージを減らします。
        m_player.upperLoad = std::max(0.0f, m_player.upperLoad - kUpperLoadRecoveryPerSecond * dt);
    }
}

void SceneNarakuProto::TryInteract()
{
    // 着地直後の硬直中はインタラクトを受け付けません。
    if (m_player.landingRecoveryTimer > 0.0f)
    {
        return;
    }

    // 採掘中は他のインタラクトを受け付けません。
    if (m_miningIndex >= 0)
    {
        return;
    }

    for (int gateIndex = 0; gateIndex < static_cast<int>(m_layerGates.size()); ++gateIndex)
    {
        const LayerGateState& gate = m_layerGates[gateIndex];
        const Vec2 interactPoint = gate.isEntry ? gate.ropePos : gate.loadPos;
        if (IsNear(m_player.pos, interactPoint, kInteractRange) &&
            std::fabs(m_player.depth - gate.depth) <= 0.35f)
        {
            TryUseLayerGate(gateIndex);
            return;
        }
    }

    // 帰還地点が最優先です。原点付近でFを押すと帰還します。
    if (IsNear(m_player.pos, m_returnPoint, kReturnRange) && std::fabs(m_player.depth - m_returnDepth) <= 0.35f)
    {
        // 誤操作を避けるため、帰還処理の前に確認を表示します。
        m_mode = Mode::ReturnConfirm;

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }

    // 次にロープへの乗り降りを判定します。
    const int ropeIndex = FindNearestRopeIndex(kInteractRange);
    if (ropeIndex >= 0)
    {
        // 対象ロープを取得します。
        const RopePoint& rope = m_ropePoints[ropeIndex];

        // すでにロープ中なら、近い端へ吸着してロープを離します。
        if (m_player.onRope)
        {
            const bool useBottom = m_ropeProgress >= 0.5f;
            m_ropeProgress = useBottom ? 1.0f : 0.0f;
            m_player.depth = useBottom ? rope.bottomDepth : rope.topDepth;
            m_player.pos = useBottom ? rope.bottomPos : rope.topPos;
            m_player.onRope = false;
            m_activeRope = -1;
            m_player.grounded = true;
            m_player.verticalSpeed = 0.0f;
            m_player.airTime = 0.0f;
            m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.landingRecoveryTimer = 0.0f;
            AddMessage(u8"ロープを離しました。");
        }

        // ロープ外なら、近い端へ吸着してロープにつかまります。
        else
        {
            const float topDistance = Distance(m_player.pos, rope.topPos) + std::fabs(m_player.depth - rope.topDepth);
            const float bottomDistance = Distance(m_player.pos, rope.bottomPos) + std::fabs(m_player.depth - rope.bottomDepth);
            m_ropeProgress = bottomDistance < topDistance ? 1.0f : 0.0f;
            m_player.onRope = true;
            m_activeRope = ropeIndex;
            m_player.pos = GetRopePosition(m_activeRope, m_ropeProgress);
            m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
            m_player.grounded = false;
            m_player.verticalSpeed = 0.0f;
            m_player.airTime = 0.0f;
            m_player.feetWorldY = GetRopeWorldY(m_activeRope, m_ropeProgress);
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.landingRecoveryTimer = 0.0f;
            AddMessage(u8"ロープにつかまりました。");
        }

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }

    // フィールドに置かれた旧器が近くにあれば拾う確認へ移ります。
    for (GroundRelic& relic : m_groundRelics)
    {
        // 無効化済みの旧器は無視します。
        if (!relic.active) continue;

        // 近くにある旧器だけ反応します。
        if (IsNear(m_player.pos, relic.pos, kInteractRange) && std::fabs(m_player.depth - relic.depth) <= 0.35f)
        {
            // 拾う確認に表示する旧器を設定します。
            m_pendingRelic = relic.item;

            // 拾わず戻す場合の位置を記録します。
            m_pendingRelicPos = relic.pos;
            m_pendingRelicDepth = relic.depth;

            // 一旦地面側を無効化して二重取得を避けます。
            relic.active = false;

            // 旧器確認モードへ移ります。
            m_mode = Mode::RelicPrompt;

            // 1回のF入力で複数の対象を処理しないよう戻ります。
            return;
        }
    }

    // 敵が落とした食料を拾います。
    for (GroundFood& food : m_groundFoods)
    {
        if (!food.active) continue;
        if (IsNear(m_player.pos, food.pos, kInteractRange) && std::fabs(m_player.depth - food.depth) <= 0.35f)
        {
            if (GetCurrentWeight() + 1.0f > GetPickupWeightLimit())
            {
                AddMessage(u8"これ以上は重すぎて食料を拾えません。");
                return;
            }
            food.active = false;
            ++m_foodCount;
            AddMessage(u8"食料を1個拾いました。");
            return;
        }
    }

    // 最後に採掘ポイントの開始判定を行います。
    for (int i = 0; i < static_cast<int>(m_miningPoints.size()); ++i)
    {
        // 対象採掘ポイントを取得します。
        MiningPoint& point = m_miningPoints[i];

        // 採掘済み、または遠いポイントは無視します。
        if (point.mined || std::fabs(m_player.depth - point.depth) > 0.35f || !IsNear(m_player.pos, point.pos, kInteractRange)) continue;

        // スタミナが足りない場合は採掘を開始しません。
        if (!CanSpendStamina(m_debugPlayerParams.miningCost))
        {
            // HUDログにスタミナ不足を出します。
            AddMessage(u8"スタミナが足りないため採掘できません。");

            // 近くの採掘対象を見つけたので処理を終えます。
            return;
        }

        // 採掘開始時にスタミナを消費します。
        SpendStamina(m_debugPlayerParams.miningCost);

        // 採掘中のポイント番号を記録します。
        m_miningIndex = i;

        // 装備中のつるはしによる速度倍率を採掘所要時間へ反映します。
        m_miningDuration = kMiningTime / GetMiningSpeedMultiplier();
        m_miningTimer = m_miningDuration;

        // HUDログに採掘開始を出します。
        AddMessage(u8"採掘を開始しました。");

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }
}

void SceneNarakuProto::TryStartStep()
{
    // 着地直後の硬直中はステップさせません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 重量100%以上ではステップ不可です。
    if (GetCurrentWeight() >= GetMaxWeight())
    {
        AddMessage(u8"重量が重すぎてステップできません。");
        ShowCenterNotification(u8"重すぎてステップができない！");
        return;
    }

    // スタミナ不足ならステップ不可です。
    if (!CanSpendStamina(m_debugPlayerParams.stepCost)) { AddMessage(u8"スタミナが足りないためステップできません。"); return; }

    // ステップ1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.stepCost);

    // 無敵時間と後硬直の合計時間を設定します。
    m_player.stepTimer = kStepInvincibleTime + kStepRecoveryTime;
}

void SceneNarakuProto::TryStartJump()
{
    // 着地直後の硬直中はジャンプさせません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 地上にいない時は二段ジャンプを許可しません。
    if (!m_player.grounded) return;

    // ロープ中はジャンプさせません。
    if (m_player.onRope) return;

    // 重量100%以上ではジャンプ不可です。
    if (GetCurrentWeight() >= GetMaxWeight()) { AddMessage(u8"重量が重すぎてジャンプできません。"); return; }

    // スタミナ不足ならジャンプ不可です。
    if (!CanSpendStamina(m_debugPlayerParams.jumpCost)) { AddMessage(u8"スタミナが足りないためジャンプできません。"); return; }

    // ジャンプ1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.jumpCost);

    // 空中状態へ切り替えます。
    m_player.grounded = false;

    // 高さ1m程度を想定した初速を入れます。
    m_player.verticalSpeed = 4.45f;

    // 現在地面の絶対高さを足元基準として記録します。
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;

    // 空中時間を0から測ります。
    m_player.airTime = 0.0f;
}

void SceneNarakuProto::TryStartAttack()
{
    // 着地直後の硬直中は攻撃させません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 攻撃中は次の攻撃を開始しません。
    if (m_player.attackTimer > 0.0f) return;

    // 採掘中は攻撃させません。
    if (m_miningIndex >= 0) return;

    // スタミナ不足なら攻撃不可です。
    if (!CanSpendStamina(m_debugPlayerParams.attackCost)) { AddMessage(u8"スタミナが足りないため攻撃できません。"); return; }

    // 攻撃1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.attackCost);

    // 攻撃全体時間を設定します。
    m_player.attackTimer = kAttackTotal;

    // 連続攻撃が左右の往復に見えるよう、攻撃開始ごとに振り方向を反転します。
    m_player.attackSwingReverse = !m_player.attackSwingReverse;
    for (EnemyState& enemy : m_enemies)
    {
        enemy.hitByPlayerAttack = false;
    }
}

bool SceneNarakuProto::IsShiftPress() const
{
    // 左Shiftまたは右Shiftが押されているかを直接確認します。
    return IsRawKeyPress(VK_LSHIFT) || IsRawKeyPress(VK_RSHIFT) || IsRawKeyPress(VK_SHIFT);
}

void SceneNarakuProto::StartDeath(const char* reason)
{
    // すでに死亡リザルト中なら二重処理を防ぎます。
    if (m_mode == Mode::DeathResult) return;

    // 死亡理由をリザルトに記録します。
    m_result.reason = reason;

    // 死亡時に失った旧器数を記録します。
    m_result.lostRelics = static_cast<int>(m_inventory.size());

    // 死亡ペナルティとして所持旧器を全ロストします。
    m_inventory.clear();
    m_foodCount = 0;

    // 死亡ペナルティとして探索中ピンを失わせます。
    m_pins.clear();

    // 死亡リザルトモードへ移行します。
    m_mode = Mode::DeathResult;
}

void SceneNarakuProto::FinishReturn()
{
    // 帰還理由をリザルトに記録します。
    m_result.reason = u8"生還しました。";

    // 持ち帰った旧器数を記録します。
    m_result.carriedRelics = static_cast<int>(m_inventory.size());

    // 商店で全売却した場合の参考額を集計します。
    m_result.saleAmount = 0;
    m_result.identifiedRelics = 0;

    // 採掘時に決まっていた種類を公開し、遺物を自宅保管へ移します。
    for (const RelicItem& item : m_inventory)
    {
        const std::size_t index = static_cast<std::size_t>(item.type);
        if (!m_identifiedRelics[index])
        {
            m_identifiedRelics[index] = true;
            ++m_result.identifiedRelics;
        }
        ++m_storedRelics[index];
        m_result.saleAmount += item.value;
    }

    m_inventory.clear();

    // 使わずに持ち帰った食料も自宅へ戻します。
    m_storedFoodCount += m_foodCount;
    m_foodCount = 0;

    // 帰還リザルトモードへ移行します。
    m_mode = Mode::ReturnResult;
}

void SceneNarakuProto::StartDive()
{
    float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount);
    for (std::size_t i = 0; i < m_loadoutRelics.size(); ++i)
    {
        if (m_loadoutRelics[i] > m_storedRelics[i])
        {
            AddMessage(u8"持ち込み予定数が自宅在庫を超えています。");
            return;
        }
        loadoutWeight += GetRelicWeight(static_cast<RelicType>(i)) * static_cast<float>(m_loadoutRelics[i]);
    }
    if (loadoutWeight > GetMaxWeight())
    {
        AddMessage(u8"持ち込み重量が100を超えているため潜行できません。");
        return;
    }

    // フィールド側の一時状態を初期化してから、選択済みの持ち込み品を移します。
    if (!ResetRun())
    {
        return;
    }
    m_foodCount = m_loadoutFoodCount;
    m_storedFoodCount -= m_loadoutFoodCount;
    m_loadoutFoodCount = 0;

    for (std::size_t i = 0; i < m_loadoutRelics.size(); ++i)
    {
        const RelicType type = static_cast<RelicType>(i);
        const int count = m_loadoutRelics[i];
        m_storedRelics[i] -= count;
        for (int itemIndex = 0; itemIndex < count; ++itemIndex)
        {
            m_inventory.push_back(CreateRelic(type, GetRelicTypeName(type)));
        }
        m_loadoutRelics[i] = 0;
    }
}

void SceneNarakuProto::RestartAfterDeath()
{
    // 死亡後は持ち込みなしで再挑戦します。
    m_loadoutFoodCount = 0;
    m_loadoutRelics.fill(0);
    StartDive();
}

void SceneNarakuProto::Draw()
{
    if (m_mode == Mode::Loading)
    {
        DrawLoadingScreen();
        return;
    }

    // デバッグ用3Dフィールドを最初に描画します。
    Draw3DField();

    DrawCompass();

    // 常時確認するステータスHUDを描画します。
    DrawHud();

    // プレイテスト中にプレイヤー性能を調整するデバッグUIを描画します。
    DrawDebugPlayerTuning();

    // プレイヤーの位置・高さ・現在小ステージ名デバッグウィンドウを描画します。
    DrawPlayerPositionDebug();

    // 所持品モードでは所持品と地図ピンUIを重ねます。
    if (m_mode == Mode::Inventory) { DrawInventory(); DrawMapControls(); }

    // 旧器発見中は拾う/置く確認を重ねます。
    else if (m_mode == Mode::RelicPrompt) DrawRelicPrompt();

    else if (m_mode == Mode::ReturnConfirm) DrawReturnConfirm();

    // 帰還または死亡後はリザルトを重ねます。
    else if (m_mode == Mode::ReturnResult || m_mode == Mode::DeathResult) DrawResult();

    else if (m_mode == Mode::Home) DrawHome();

    else if (m_mode == Mode::GeneralShop) DrawGeneralShop();

    else if (m_mode == Mode::Armory) DrawArmory();

    DrawCenterNotification();

    // 採掘（探窟）進行度バーを画面中央にオーバーレイ表示します。
    DrawMiningProgressBar();
}

void SceneNarakuProto::DrawLoadingScreen()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(1.0f);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("LayerLoading##NarakuProto", nullptr, flags))
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const char* loadingText = u8"ロード中...";
        const ImVec2 textSize = ImGui::CalcTextSize(loadingText);
        ImGui::SetCursorScreenPos(ImVec2(
            windowPos.x + (windowSize.x - textSize.x) * 0.5f,
            windowPos.y + (windowSize.y - textSize.y) * 0.5f));
        ImGui::TextUnformatted(loadingText);

        constexpr float barWidth = 260.0f;
        constexpr float margin = 28.0f;
        ImGui::SetCursorScreenPos(ImVec2(
            windowPos.x + windowSize.x - barWidth - margin,
            windowPos.y + windowSize.y - 24.0f - margin));
        ImGui::ProgressBar(std::max(0.0f, std::min(1.0f, m_loadingProgress)), ImVec2(barWidth, 18.0f), "");
    }
    ImGui::End();
}

void SceneNarakuProto::UpdateCameraControls()
{
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.0f)
    {
        m_cameraDistance -= wheel;
    }

    NormalizeCameraSettings();

    if (!IsMouseRightPress())
    {
        return;
    }

    constexpr float mouseSensitivity = 0.006f;
    const POINT mouseDelta = GetMouseDelta();

    m_cameraYaw += static_cast<float>(mouseDelta.x) * mouseSensitivity;
    m_cameraPitch += static_cast<float>(mouseDelta.y) * mouseSensitivity;
    NormalizeCameraSettings();

    constexpr float twoPi = DirectX::XM_2PI;
    if (m_cameraYaw > twoPi || m_cameraYaw < -twoPi)
    {
        m_cameraYaw = std::fmod(m_cameraYaw, twoPi);
    }
}

void SceneNarakuProto::NormalizeCameraSettings()
{
    m_cameraDistance = std::max(kCameraMinDistance, std::min(m_cameraDistance, kCameraMaxDistance));
    m_cameraMinPitchDegrees = std::max(kCameraMinPitchDegrees, std::min(m_cameraMinPitchDegrees, kCameraMaxPitchDegrees));
    m_cameraMaxPitchDegrees = std::max(kCameraMinPitchDegrees, std::min(m_cameraMaxPitchDegrees, kCameraMaxPitchDegrees));
    m_cameraMinPitchDegrees = std::min(m_cameraMinPitchDegrees, m_cameraMaxPitchDegrees);
    m_cameraMaxPitchDegrees = std::max(m_cameraMaxPitchDegrees, m_cameraMinPitchDegrees);
    const float minPitch = DirectX::XMConvertToRadians(m_cameraMinPitchDegrees);
    const float maxPitch = DirectX::XMConvertToRadians(m_cameraMaxPitchDegrees);
    m_cameraPitch = std::max(minPitch, std::min(m_cameraPitch, maxPitch));
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetCameraForward() const
{
    return Normalize({ -std::sin(m_cameraYaw), -std::cos(m_cameraYaw) });
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetCameraRight() const
{
    const Vec2 forward = GetCameraForward();
    return Normalize({ forward.y, -forward.x });
}

void SceneNarakuProto::ReleaseEnvironmentModels()
{
    for (EnvironmentModelResource& resource : m_environmentModels)
    {
        SAFE_DELETE(resource.model);
    }
    m_environmentModels.clear();
}

void SceneNarakuProto::LoadEnvironmentModels()
{
    ReleaseEnvironmentModels();
    const std::wstring catalogPath = ResolveProjectPath(kEnvironmentModelCatalogRelativePath);
    std::ifstream input(catalogPath, std::ios::binary);
    if (!input) return;

    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream row(line);
        std::string id;
        std::string name;
        std::string modelPath;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        if (!(row >> std::quoted(id) >> std::quoted(name) >> std::quoted(modelPath)
            >> scaleX >> scaleY >> scaleZ))
        {
            continue;
        }

        const std::string resolvedModelPath = WideToUtf8(ResolveProjectPath(Utf8ToWide(modelPath)));
        Model* model = new Model();
        if (!model->Load(resolvedModelPath.c_str(), 1.0f, Model::ZFlip))
        {
            SAFE_DELETE(model);
            continue;
        }

        DirectX::XMFLOAT3 minValue = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max() };
        DirectX::XMFLOAT3 maxValue = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest() };
        bool hasVertex = false;
        for (unsigned int meshIndex = 0; meshIndex < model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            for (const Model::Vertex& vertex : mesh->vertices)
            {
                minValue.x = std::min(minValue.x, vertex.pos.x);
                minValue.y = std::min(minValue.y, vertex.pos.y);
                minValue.z = std::min(minValue.z, vertex.pos.z);
                maxValue.x = std::max(maxValue.x, vertex.pos.x);
                maxValue.y = std::max(maxValue.y, vertex.pos.y);
                maxValue.z = std::max(maxValue.z, vertex.pos.z);
                hasVertex = true;
            }
        }

        EnvironmentModelResource resource;
        resource.id = id;
        resource.model = model;
        if (hasVertex)
        {
            resource.placementAnchor = {
                (minValue.x + maxValue.x) * 0.5f,
                minValue.y,
                (minValue.z + maxValue.z) * 0.5f };
        }
        m_environmentModels.push_back(resource);
    }
}

void SceneNarakuProto::DrawEnvironmentObjects(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;
    ShaderList::SetCameraPos(cameraPosition);
    ShaderList::SetLight({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f });

    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        const auto resourceIt = std::find_if(
            m_environmentModels.begin(),
            m_environmentModels.end(),
            [&](const EnvironmentModelResource& resource) { return resource.id == object.modelId; });
        if (resourceIt == m_environmentModels.end() || resourceIt->model == nullptr) continue;

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, object.layerId);
        if (layerIndex < 0) continue;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
        const Vec2 position = { object.xz.x, object.xz.z };
        const float groundY = GetGroundWorldY(position, layer.layerDepth);

        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(
                -resourceIt->placementAnchor.x,
                -resourceIt->placementAnchor.y,
                -resourceIt->placementAnchor.z) *
            XMMatrixScaling(object.scaleX, object.scaleY, object.scaleZ) *
            XMMatrixTranslation(object.xz.x, groundY, object.xz.z)));
        wvp[1] = view;
        wvp[2] = projection;
        ShaderList::SetWVP(wvp);
        resourceIt->model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        resourceIt->model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < resourceIt->model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = resourceIt->model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = resourceIt->model->GetMaterial(mesh->materialID);
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                ShaderList::SetMaterial(material);
            }
            resourceIt->model->Draw(static_cast<int>(meshIndex));
        }
    }
}

void SceneNarakuProto::Draw3DField()
{
    using namespace DirectX;

    // プレイヤー位置を3D描画用の注視点に変換します。
    const float playerHeightOffset = (m_player.onRope && m_activeRope >= 0
        ? (GetRopeWorldY(m_activeRope, m_ropeProgress) - GetGroundWorldY(m_player.pos, m_player.depth))
        : GetPlayerAirborneOffset()) + m_layerTransitionVisualOffset;
    const XMFLOAT3 playerCenter = ToWorld3D(m_player.pos, m_player.depth, 0.7f + playerHeightOffset);

    // 初期状態でも同じ制約を適用し、最初の右ドラッグ前にカメラが高くなりすぎないようにします。
    NormalizeCameraSettings();
    const float verticalOffset = std::sin(m_cameraPitch) * m_cameraDistance;

    // 斜め見下ろしになるように、プレイヤーの右後ろ上方へカメラを置きます。
    // プレイヤーを注視点に固定し、yaw/pitchから一定距離の軌道位置を算出します。
    const float horizontalDistance = std::sqrt(std::max(
        0.0f,
        m_cameraDistance * m_cameraDistance - verticalOffset * verticalOffset));
    XMVECTOR eye = XMVectorSet(
        playerCenter.x + std::sin(m_cameraYaw) * horizontalDistance,
        playerCenter.y + verticalOffset,
        playerCenter.z + std::cos(m_cameraYaw) * horizontalDistance,
        0.0f);

    // カメラは常にプレイヤー付近を向くようにします。
    XMVECTOR target = XMVectorSet(playerCenter.x, playerCenter.y, playerCenter.z, 0.0f);

    if (m_cameraShakeTimer > 0.0f)
    {
        const float elapsed = kCameraShakeDuration - m_cameraShakeTimer;
        const float attenuation = m_cameraShakeTimer / kCameraShakeDuration;
        const float shakeX = std::sin(elapsed * 92.0f) * kCameraShakeAmplitude * attenuation;
        const float shakeY = std::cos(elapsed * 117.0f) * kCameraShakeAmplitude * attenuation;
        eye = XMVectorAdd(eye, XMVectorSet(shakeX, shakeY, 0.0f, 0.0f));
        target = XMVectorAdd(target, XMVectorSet(-shakeX * 0.25f, -shakeY * 0.15f, 0.0f, 0.0f));
    }

    const XMVECTOR cameraToPlayer = XMVectorSubtract(target, eye);
    const float cameraToPlayerLength = XMVectorGetX(XMVector3Length(cameraToPlayer));
    const XMVECTOR cameraToPlayerDirection = cameraToPlayerLength > 0.0001f
        ? XMVectorScale(cameraToPlayer, 1.0f / cameraToPlayerLength)
        : XMVectorZero();

    const auto intersectsViewSegment = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c) -> bool
    {
        constexpr float epsilon = 0.00001f;
        const XMVECTOR vertexA = XMLoadFloat3(&a);
        const XMVECTOR edgeAB = XMVectorSubtract(XMLoadFloat3(&b), vertexA);
        const XMVECTOR edgeAC = XMVectorSubtract(XMLoadFloat3(&c), vertexA);
        const XMVECTOR perpendicular = XMVector3Cross(cameraToPlayerDirection, edgeAC);
        const float determinant = XMVectorGetX(XMVector3Dot(edgeAB, perpendicular));
        if (std::fabs(determinant) < epsilon)
        {
            return false;
        }

        const float inverseDeterminant = 1.0f / determinant;
        const XMVECTOR originOffset = XMVectorSubtract(eye, vertexA);
        const float triangleU = XMVectorGetX(XMVector3Dot(originOffset, perpendicular)) * inverseDeterminant;
        if (triangleU < 0.0f || triangleU > 1.0f)
        {
            return false;
        }

        const XMVECTOR cross = XMVector3Cross(originOffset, edgeAB);
        const float triangleV = XMVectorGetX(XMVector3Dot(cameraToPlayerDirection, cross)) * inverseDeterminant;
        if (triangleV < 0.0f || triangleU + triangleV > 1.0f)
        {
            return false;
        }

        const float distance = XMVectorGetX(XMVector3Dot(edgeAC, cross)) * inverseDeterminant;
        return distance > 0.05f && distance < cameraToPlayerLength - 0.35f;
    };

    const auto isTerrainCellOccludingPlayer = [&](const NarakuMap::TerrainLayer& layer, int cellX, int cellZ) -> bool
    {
        if (cameraToPlayerLength <= 0.35f)
        {
            return false;
        }
        const XMFLOAT3 a = GetTerrainVertexWorld3D(layer, cellX, cellZ, 0.0f);
        const XMFLOAT3 b = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ, 0.0f);
        const XMFLOAT3 c = GetTerrainVertexWorld3D(layer, cellX, cellZ + 1, 0.0f);
        const XMFLOAT3 d = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ + 1, 0.0f);
        return intersectsViewSegment(a, b, c) || intersectsViewSegment(b, d, c);
    };

    // DirectXの標準的な上方向を使ってビュー行列を作ります。
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // 既存Geometoryはビュー/射影を転置して渡す設計なので、Main.cpp側の使い方に合わせます。
    XMFLOAT4X4 view;
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMMatrixLookAtLH(eye, target, up)));
    Geometory::SetView(view);
    Sprite::SetView(view);

    // 画面比率を使って、デバッグ用の遠近投影を設定します。
    XMFLOAT4X4 projection;
    const float aspect = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), aspect, 0.1f, 500.0f)));
    Geometory::SetProjection(projection);
    Sprite::SetProjection(projection);

    if (m_skyModel != nullptr)
    {
        XMFLOAT3 cameraPosition = {};
        XMStoreFloat3(&cameraPosition, eye);
        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(
            &wvp[0],
            XMMatrixTranspose(
                XMMatrixScaling(kSkySphereRadius, kSkySphereRadius, kSkySphereRadius) *
                XMMatrixTranslation(cameraPosition.x, cameraPosition.y, cameraPosition.z)));
        wvp[1] = view;
        wvp[2] = projection;

        SetDepthTest(false);
        SetCullingMode(D3D11_CULL_NONE);
        SetBlendMode(BLEND_NONE);
        ShaderList::SetWVP(wvp);
        ShaderList::SetLight({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f });
        m_skyModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        m_skyModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < m_skyModel->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = m_skyModel->GetMesh(meshIndex);
            if (mesh == nullptr)
            {
                continue;
            }
            Model::Material material = *m_skyModel->GetMaterial(mesh->materialID);
            material.diffuse = { 0.40f, 0.40f, 0.40f, 1.0f };
            material.ambient = { 0.40f, 0.40f, 0.40f, 1.0f };
            material.specular = { 0.0f, 0.0f, 0.0f, 1.0f };
            ShaderList::SetMaterial(material);
            m_skyModel->Draw(static_cast<int>(meshIndex));
        }
    }

    // 3Dデバッグ描画は深度テストを有効にして、前後関係を分かりやすくします。
    SetDepthTest(true);

    XMFLOAT3 cameraPosition = {};
    XMStoreFloat3(&cameraPosition, eye);
    SetCullingMode(D3D11_CULL_BACK);
    SetBlendMode(BLEND_NONE);
    DrawEnvironmentObjects(view, projection, cameraPosition);

    // 半透明床は両面から見える方がデバッグしやすいので、カリングを切ります。
    SetCullingMode(D3D11_CULL_NONE);

    // 既存の半透明床と同じ合成結果になるよう、バッチ描画でもアルファブレンドを有効にします。
    SetBlendMode(BLEND_ALPHA);
    m_terrainFloorVertexCount = 0;

    auto getLayerFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    // プレイヤーより浅い深度にあるレイヤーは、視界を塞がないよう上層扱いで薄くします。
    auto applyGameplayLayerAlpha = [this](const NarakuMap::TerrainLayer& layer, XMFLOAT4 color) -> XMFLOAT4
    {
        if (layer.layerDepth < m_player.depth - 0.01f)
        {
            color.w = m_debugPlayerParams.upperLayerAlpha;
        }
        if (m_mode == Mode::LayerTransition)
        {
            color.w *= std::max(0.08f, 1.0f - m_layerTransitionProgress);
        }
        return color;
    };

    // 実際の有効セルだけを半透明床として描画し、削除セルは穴として残します。
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        const XMFLOAT4 layerColor = applyGameplayLayerAlpha(layer, getLayerFloorColor(layer.groundTextureId));
        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                if ((flags & NarakuMap::CellAttributeRemoved) != 0u)
                {
                    continue;
                }

                if (!NarakuMap::IsVertexEnabled(layer, cellX, cellZ) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX, cellZ + 1) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ + 1))
                {
                    continue;
                }

                if (isTerrainCellOccludingPlayer(layer, cellX, cellZ))
                {
                    continue;
                }

                const XMFLOAT3 a = GetTerrainVertexWorld3D(layer, cellX, cellZ, -0.05f);
                const XMFLOAT3 b = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ, -0.05f);
                const XMFLOAT3 c = GetTerrainVertexWorld3D(layer, cellX, cellZ + 1, -0.05f);
                const XMFLOAT3 d = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ + 1, -0.05f);
                const XMFLOAT3 center(
                    (a.x + b.x + c.x + d.x) * 0.25f,
                    (a.y + b.y + c.y + d.y) * 0.25f,
                    (a.z + b.z + c.z + d.z) * 0.25f);
                AppendTerrainFloorQuad(center, { layer.cellSize, layer.cellSize }, layerColor);
            }
        }
    }

    // ロープ穴の目印として、地上側に暗い半透明板を重ねます。
    for (const RopePoint& rope : m_ropePoints)
    {
        AppendTerrainFloorQuad(ToWorld3D(rope.topPos, rope.topDepth, -0.04f), { 3.0f, 3.0f }, { 0.02f, 0.03f, 0.04f, 0.35f });
        AppendTerrainFloorQuad(ToWorld3D(rope.bottomPos, rope.bottomDepth, -0.04f), { 3.0f, 3.0f }, { 0.02f, 0.03f, 0.04f, 0.35f });
    }

    for (const LayerGateState& gate : m_layerGates)
    {
        const Vec2 point = gate.isEntry ? gate.ropePos : gate.loadPos;
        const DirectX::XMFLOAT4 color = gate.disabled
            ? DirectX::XMFLOAT4{ 0.45f, 0.12f, 0.12f, 0.55f }
            : (gate.isEntry
                ? DirectX::XMFLOAT4{ 0.20f, 0.55f, 1.0f, 0.55f }
                : DirectX::XMFLOAT4{ 0.82f, 0.50f, 0.12f, 0.55f });
        AppendTerrainFloorQuad(ToWorld3D(point, gate.depth, -0.03f), { 2.0f, 2.0f }, color);
    }
    DrawTerrainFloorBatch(view, projection);

    // 帰還地点を緑の柱で示します。
    const XMFLOAT3 returnBase = ToWorld3D(m_returnPoint, m_returnDepth, 0.05f);
    DrawDebugBox3D({ returnBase.x, returnBase.y + 0.25f, returnBase.z }, { 0.9f, 0.5f, 0.9f });

    // ロープの接続位置は上下端の箱で示します。
    for (const RopePoint& rope : m_ropePoints)
    {
        // ロープ上端を3D座標へ変換します。
        const XMFLOAT3 top = ToWorld3D(rope.topPos, rope.topDepth, 1.2f);

        // ロープ下端を3D座標へ変換します。
        const XMFLOAT3 bottom = ToWorld3D(rope.bottomPos, rope.bottomDepth, 0.0f);

        // 触れる位置が分かるように小さい箱を置きます。
        DrawDebugBox3D({ top.x, top.y, top.z }, { 0.35f, 0.35f, 0.35f });
        DrawDebugBox3D({ bottom.x, bottom.y, bottom.z }, { 0.30f, 0.30f, 0.30f });
    }

    // 採掘ポイントを箱で描画します。
    for (const MiningPoint& point : m_miningPoints)
    {
        // 未記録でも、近くまで来た採掘ポイントは現地で見えるようにします。
        const bool visibleInField = point.discovered || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField)
        {
            continue;
        }

        // 採掘ポイントの位置を深度0の地表として扱います。
        const XMFLOAT3 base = ToWorld3D(point.pos, point.depth, 0.15f);

        // 見た目4種類は箱の横幅だけ少し変えて区別します。
        const float width = 0.45f + 0.08f * static_cast<float>(point.visualType);
        DrawDebugBox3D({ base.x, base.y + 0.2f, base.z }, { width, 0.4f, width });

    }

    // 地面に落ちている旧器を小さい箱で示します。
    for (const GroundRelic& relic : m_groundRelics)
    {
        // 回収不能になった旧器は描画しません。
        if (!relic.active)
        {
            continue;
        }

        // 旧器の位置を3D座標へ変換します。
        const XMFLOAT3 base = ToWorld3D(relic.pos, relic.depth, 0.12f);

        // 小さな箱で旧器の本体を描きます。
        DrawDebugBox3D({ base.x, base.y + 0.12f, base.z }, { 0.35f, 0.25f, 0.35f });

    }

    // 敵が落とした食料を箱で描画します。
    for (const GroundFood& food : m_groundFoods)
    {
        if (!food.active) continue;
        const XMFLOAT3 base = ToWorld3D(food.pos, food.depth, 0.12f);
        DrawDebugBox3D({ base.x, base.y + 0.12f, base.z }, { 0.30f, 0.22f, 0.30f });
    }

    // 左上の緑スライムを全敵共通の静止ビルボードとしてまとめて描画します。
    DrawEnemyBillboardBatch(view, projection);

    // プレイヤーを青い箱で描きます。
    DrawDebugBox3D(playerCenter, { 0.55f, 1.2f, 0.55f });

    if (m_showCollisionDebug)
    {
        // 帰還範囲のデバッグ表示を追加
        DrawDebugSphere3D({ returnBase.x, returnBase.y + 0.05f, returnBase.z }, kReturnRange);

        // 未採掘の採掘ポイントのインタラクト範囲のデバッグ表示を追加
        for (const MiningPoint& point : m_miningPoints)
        {
            if (!point.mined)
            {
                const XMFLOAT3 base = ToWorld3D(point.pos, point.depth, 0.15f);
                DrawDebugSphere3D({ base.x, base.y + 0.05f, base.z }, kInteractRange);
            }
        }

    }

    if (m_attackHitTexture != nullptr && !m_attackHitEffects.empty())
    {
        const XMMATRIX viewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&view));
        XMFLOAT4X4 billboardFloat = {};
        XMStoreFloat4x4(&billboardFloat, XMMatrixInverse(nullptr, viewMatrix));
        billboardFloat._41 = 0.0f;
        billboardFloat._42 = 0.0f;
        billboardFloat._43 = 0.0f;
        const XMMATRIX billboard = XMLoadFloat4x4(&billboardFloat);

        SetBlendMode(BLEND_ADDALPHA);
        SetCullingMode(D3D11_CULL_NONE);
        Sprite::SetVertexShader(nullptr);
        Sprite::SetPixelShader(nullptr);
        Sprite::SetTexture(m_attackHitTexture);
        Sprite::SetSize({ 1.5f, 1.5f });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVScale({ 1.0f / static_cast<float>(kAttackHitEffectFrameCount), 1.0f });
        Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        for (const AttackHitEffect& effect : m_attackHitEffects)
        {
            const float elapsed = kAttackHitEffectDuration - effect.remainingTime;
            const int frame = std::max(
                0,
                std::min(
                    kAttackHitEffectFrameCount - 1,
                    static_cast<int>(elapsed / kAttackHitEffectFrameTime)));
            const XMFLOAT3 effectPosition = ToWorld3D(effect.pos, effect.depth, 0.65f);
            XMFLOAT4X4 effectWorld = {};
            XMStoreFloat4x4(
                &effectWorld,
                XMMatrixTranspose(
                    billboard * XMMatrixTranslation(effectPosition.x, effectPosition.y, effectPosition.z)));
            Sprite::SetWorld(effectWorld);
            Sprite::SetUVPos({ static_cast<float>(frame) / static_cast<float>(kAttackHitEffectFrameCount), 0.0f });
            Sprite::Draw();
        }
        SetBlendMode(BLEND_ALPHA);
    }

}

void SceneNarakuProto::DrawCompass() const
{
    using namespace DirectX;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 viewportMin = viewport->WorkPos;
    const ImVec2 viewportMax(
        viewport->WorkPos.x + viewport->WorkSize.x,
        viewport->WorkPos.y + viewport->WorkSize.y);
    const ImVec2 center(
        viewportMax.x - kCompassMargin - kCompassRadius,
        viewportMin.y + kCompassMargin + kCompassRadius);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->PushClipRect(viewportMin, viewportMax, true);
    drawList->AddCircle(center, kCompassRadius, IM_COL32(220, 230, 240, 220), 32, kCompassLineThickness);
    drawList->AddCircleFilled(center, 2.5f, IM_COL32(220, 230, 240, 230));

    const float cosPitch = std::cos(m_cameraPitch);
    const XMVECTOR cameraForward = XMVectorSet(
        -std::sin(m_cameraYaw) * cosPitch,
        -std::sin(m_cameraPitch),
        -std::cos(m_cameraYaw) * cosPitch,
        0.0f);
    const XMMATRIX view = XMMatrixLookToLH(
        XMVectorZero(),
        cameraForward,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMFLOAT3 worldDirections[] =
    {
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
    };

    for (int index = 0; index < 4; ++index)
    {
        XMFLOAT3 viewDirection = {};
        XMStoreFloat3(&viewDirection, XMVector3TransformNormal(XMLoadFloat3(&worldDirections[index]), view));
        const float length = std::sqrt(
            viewDirection.x * viewDirection.x +
            viewDirection.y * viewDirection.y);
        if (length <= 0.0001f)
        {
            continue;
        }

        const ImVec2 direction(viewDirection.x / length, -viewDirection.y / length);
        const ImVec2 endpoint(
            center.x + direction.x * (kCompassRadius - kCompassLinePadding),
            center.y + direction.y * (kCompassRadius - kCompassLinePadding));
        const ImU32 color = index == 0
            ? IM_COL32(245, 95, 95, 230)
            : IM_COL32(220, 230, 240, 220);
        drawList->AddLine(center, endpoint, color, kCompassLineThickness);

        const ImVec2 textSize = ImGui::CalcTextSize(kCompassDirectionLabels[index]);
        const ImVec2 labelCenter(
            center.x + direction.x * (kCompassRadius + kCompassLabelDistance),
            center.y + direction.y * (kCompassRadius + kCompassLabelDistance));
        ImVec2 textPosition(
            labelCenter.x - textSize.x * 0.5f,
            labelCenter.y - textSize.y * 0.5f);
        textPosition.x = std::max(viewportMin.x, std::min(textPosition.x, viewportMax.x - textSize.x));
        textPosition.y = std::max(viewportMin.y, std::min(textPosition.y, viewportMax.y - textSize.y));
        drawList->AddText(textPosition, color, kCompassDirectionLabels[index]);
    }

    drawList->PopClipRect();
}

void SceneNarakuProto::DrawField()
{
    // フィールドウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

    // フィールドウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(760.0f, 620.0f), ImGuiCond_FirstUseEver);

    // フィールド描画用ウィンドウを開始します。
    ImGui::Begin(u8"奈落塔プロト フィールド");

    // フィールドキャンバス左上のスクリーン座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // フィールドキャンバスの表示サイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 520.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // フィールド背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(30, 36, 34, 255));

    // フィールド外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 150, 140, 255));

    // 帰還地点のワールド座標を斜め見下ろし座標に変換します。
    Vec2 ret = WorldToObliqueCanvas(canvasPos, canvasSize, m_returnPoint, m_returnDepth);

    // 帰還地点を青い円で描きます。
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 9.0f, IM_COL32(80, 180, 255, 255));

    // すべてのロープを描画します。
    for (const RopePoint& rope : m_ropePoints)
    {
        const Vec2 top = WorldToObliqueCanvas(canvasPos, canvasSize, rope.topPos, rope.topDepth);
        const Vec2 bottom = WorldToObliqueCanvas(canvasPos, canvasSize, rope.bottomPos, rope.bottomDepth);
        draw->AddLine(ImVec2(top.x, top.y), ImVec2(bottom.x, bottom.y), IM_COL32(170, 120, 70, 255), 4.0f);
    }

    // 発見済み、または近くまで来た採掘ポイントを描画します。
    for (const MiningPoint& point : m_miningPoints)
    {
        // 未記録でも近くまで来たポイントは、初期記録済みと同じ色で見せます。
        const bool visibleInField = point.discovered || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField) continue;

        // 採掘ポイント座標を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, point.pos, point.depth);

        // 採掘済みは暗色、未採掘は黄色系にします。
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);

        // 見た目4種類は半径差だけで表現します。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 5.0f + static_cast<float>(point.visualType), color);
    }

    // 地面に置かれた旧器を描画します。
    for (const GroundRelic& relic : m_groundRelics)
    {
        // 非アクティブな旧器は描画しません。
        if (!relic.active) continue;

        // 旧器位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, relic.pos, relic.depth);

        // 旧器を小さな四角で描きます。
        draw->AddRectFilled(ImVec2(p.x - 4.0f, p.y - 4.0f), ImVec2(p.x + 4.0f, p.y + 4.0f), IM_COL32(240, 220, 130, 255));
    }

    // プレイヤーが置いたピンを描画します。
    for (const Vec2& pin : m_pins)
    {
        // ピン位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, pin);

        // ピンを赤い三角形で描きます。
        draw->AddTriangleFilled(ImVec2(p.x, p.y - 10.0f), ImVec2(p.x - 6.0f, p.y + 5.0f), ImVec2(p.x + 6.0f, p.y + 5.0f), IM_COL32(230, 80, 90, 255));
    }

    // 敵を描画します。
    for (const EnemyState& enemy : m_enemies)
    {
        // 死んだ敵は描画しません。
        if (!enemy.alive) continue;

        // 敵位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, enemy.pos);

        // 通常時の敵色を赤にします。
        ImU32 color = enemy.telegraphTimer > 0.0f ? IM_COL32(255, 200, 60, 255) : IM_COL32(210, 70, 70, 255);

        // 体当たり中はより強い赤で表示します。
        if (enemy.chargeTimer > 0.0f) color = IM_COL32(255, 80, 40, 255);

        // 敵を円で描きます。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 7.0f, color);
    }

    // プレイヤー位置を斜め見下ろし座標へ変換します。
    Vec2 player = WorldToObliqueCanvas(canvasPos, canvasSize, m_player.pos, m_player.depth);

    // プレイヤー本体を緑の円で描きます。
    draw->AddCircleFilled(ImVec2(player.x, player.y), 7.5f, IM_COL32(90, 220, 150, 255));

    // 向き表示の終点を計算します。
    Vec2 faceEnd = WorldToObliqueCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 1.2f)), m_player.depth);

    // プレイヤーの向きを短い線で描きます。
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 2.0f);

    // キャンバスぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    // 操作確認用の短い説明を表示します。
    ImGui::Text(u8"WASD 移動 / Shift短押し ステップ / Shift長押し 走り / Space ジャンプ / 左クリック 攻撃 / F 調べる / T 所持品 / ロープ中A/D 離脱");

    // 採掘中ならメインウィンドウ（フィールド）の中央に進行度バーを描画します。
    if (m_miningIndex >= 0)
    {
        float centerX = canvasPos.x + canvasSize.x * 0.5f;
        float centerY = canvasPos.y + canvasSize.y * 0.5f;

        float barWidth = 240.0f;
        float barHeight = 18.0f;
        float progress = std::max(0.0f, std::min(1.0f, 1.0f - (m_miningTimer / m_miningDuration)));

        std::string text = u8"採掘中...";
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        ImVec2 textPos = { centerX - textSize.x * 0.5f, centerY - 25.0f };
        draw->AddText(textPos, IM_COL32(255, 220, 60, 255), text.c_str());

        ImVec2 bgMin = { centerX - barWidth * 0.5f, centerY };
        ImVec2 bgMax = { centerX + barWidth * 0.5f, centerY + barHeight };
        draw->AddRectFilled(bgMin, bgMax, IM_COL32(10, 15, 15, 200), 4.0f);
        draw->AddRect(bgMin, bgMax, IM_COL32(130, 150, 140, 255), 4.0f, 0, 1.5f);

        if (progress > 0.0f)
        {
            ImVec2 fgMax = { bgMin.x + barWidth * progress, bgMax.y };
            draw->AddRectFilled(bgMin, fgMax, IM_COL32(240, 200, 50, 255), 4.0f);
        }
    }

    // フィールドウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawHud()
{
    constexpr float overlayWidth = 280.0f;
    constexpr float overlayHeight = 106.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 overlayPos(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 16.0f);
    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("PlayerStatusOverlay##Overlay", nullptr, flags))
    {
        const auto drawGauge = [](float ratio, const char* label, const ImVec4& color)
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.05f, 0.05f, 0.88f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
            ImGui::ProgressBar(std::max(0.0f, std::min(1.0f, ratio)), ImVec2(-1.0f, 18.0f), label);
            ImGui::PopStyleColor(2);
        };

        drawGauge(m_player.hp / 10.0f, "HP", ImVec4(0.78f, 0.18f, 0.16f, 1.0f));
        drawGauge(m_player.stamina / 100.0f, u8"スタミナ", ImVec4(0.18f, 0.68f, 0.36f, 1.0f));
        drawGauge(m_player.mental / 100.0f, u8"精神力", ImVec4(0.22f, 0.48f, 0.82f, 1.0f));
        drawGauge(m_player.upperLoad / kUpperLoadLimit, u8"上昇負荷 (Debug)", ImVec4(0.88f, 0.58f, 0.18f, 1.0f));
    }
    ImGui::End();
}

void SceneNarakuProto::DrawDebugPlayerTuning()
{
    // 調整ウィンドウの初期位置をHUDの右側へ置きます。
    ImGui::SetNextWindowPos(ImVec2(1240.0f, 20.0f), ImGuiCond_FirstUseEver);

    // 調整項目が見切れない程度の初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(380.0f, 430.0f), ImGuiCond_FirstUseEver);

    // プレイテスト専用の調整ウィンドウを開始します。
    ImGui::Begin(u8"プレイテスト調整");

    // 現在の重量補正を確認しながら調整できるよう、実効状態を先頭に表示します。
    ImGui::Text(u8"実効歩行速度: %.2f", GetMoveSpeed());
    ImGui::Text(u8"重量補正: %.0f%%", GetWeightRate() * 100.0f);
    ImGui::Separator();

    if (ImGui::Button(u8"調整値を保存"))
    {
        ClampDebugPlayerParams();
        NormalizeCameraSettings();
        ShowCenterNotification(SaveDebugPlayerParams()
            ? u8"プレイテスト調整を保存しました。"
            : u8"プレイテスト調整を保存できませんでした。");
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"初期値に戻す"))
    {
        ResetDebugPlayerParams();
    }
    ImGui::Separator();

    // 調整値は毎フレームクランプされますが、UI操作直後にも即座に丸めます。
    if (ImGui::CollapsingHeader(u8"移動", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 通常移動、走り、ロープ昇降の速度を調整します。
        ImGui::SliderFloat(u8"通常移動速度", &m_debugPlayerParams.walkSpeed, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat(u8"走り速度", &m_debugPlayerParams.runSpeed, 0.0f, 15.0f, "%.2f");
        ImGui::SliderFloat(u8"ロープ昇降速度", &m_debugPlayerParams.ropeSpeed, 0.0f, 10.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"戦闘", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 攻撃1回あたりのダメージと消費スタミナを調整します。
        ImGui::SliderFloat(u8"攻撃力", &m_debugPlayerParams.attackPower, 0.0f, 20.0f, "%.2f");
        ImGui::SliderFloat(u8"攻撃スタミナ消費", &m_debugPlayerParams.attackCost, 0.0f, 100.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"スタミナ", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 各行動のスタミナ消費量と自然回復速度を調整します。
        ImGui::SliderFloat(u8"走り秒間消費", &m_debugPlayerParams.runCostPerSecond, 0.0f, 30.0f, "%.2f");
        ImGui::SliderFloat(u8"ロープ秒間消費", &m_debugPlayerParams.ropeCostPerSecond, 0.0f, 30.0f, "%.2f");
        ImGui::SliderFloat(u8"採掘消費", &m_debugPlayerParams.miningCost, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat(u8"回避消費", &m_debugPlayerParams.stepCost, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat(u8"ジャンプ消費", &m_debugPlayerParams.jumpCost, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat(u8"回復速度", &m_debugPlayerParams.staminaRecoverPerSecond, 0.0f, 30.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"描画", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 現在深度より上にあるレイヤーの透明度を調整します。0に近いほど見えなくなります。
        ImGui::SliderFloat(u8"上層レイヤー透明度", &m_debugPlayerParams.upperLayerAlpha, 0.0f, 0.30f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"カメラ", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat(u8"仰角下限（真横=0度）", &m_cameraMinPitchDegrees, kCameraMinPitchDegrees, kCameraMaxPitchDegrees, "%.1f度");
        ImGui::SliderFloat(u8"仰角上限（真上=90度）", &m_cameraMaxPitchDegrees, kCameraMinPitchDegrees, kCameraMaxPitchDegrees, "%.1f度");
        ImGui::Text(u8"距離: %.2f / %.2f", m_cameraDistance, kCameraMaxDistance);
        NormalizeCameraSettings();
    }

    ClampDebugPlayerParams();

    // プレイテスト専用の調整ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawInventory()
{
    // 所持品ウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(160.0f, 80.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(500.0f, 460.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウを開始します。
    ImGui::Begin(u8"所持品", nullptr, ImGuiWindowFlags_NoCollapse);

    const float currentWeight = GetCurrentWeight();
    const float maxWeight = GetMaxWeight();
    ImGui::Text(u8"重量: %.0f / %.0f  %.0f%%", currentWeight, maxWeight, currentWeight / maxWeight * 100.0f);
    ImGui::Separator();

    // 所持旧器を一覧表示します。
    for (int i = 0; i < static_cast<int>(m_inventory.size()); ++i)
    {
        ImGui::PushID(i);

        // 表示対象の旧器を取得します。
        const RelicItem& item = m_inventory[i];

        // Selectableに渡す表示文字列を作ります。
        char label[128];

        // 未鑑定中は重量だけを見せ、売値は帰還鑑定まで伏せます。
        const std::size_t typeIndex = static_cast<std::size_t>(item.type);
        const bool identified = typeIndex < m_identifiedRelics.size() && m_identifiedRelics[typeIndex];
        if (identified)
        {
            std::snprintf(label, sizeof(label), u8"%s  重量 %.0f  売値 %d", GetRelicDisplayName(item), item.weight, item.value);
        }
        else
        {
            std::snprintf(label, sizeof(label), u8"%s  重量 %.0f  売値 ?", GetRelicDisplayName(item), item.weight);
        }

        // クリックされた旧器を選択状態にします。
        if (ImGui::Selectable(label, m_selectedInventory == i)) m_selectedInventory = i;

        ImGui::PopID();
    }

    // 有効な旧器が選ばれている時だけ捨てるボタンを出します。
    if (m_selectedInventory >= 0 && m_selectedInventory < static_cast<int>(m_inventory.size()))
    {
        // 選択旧器を現在位置に捨てます。
        if (ImGui::Button(u8"選択した旧器を捨てる"))
        {
            // 所持品から地面旧器へ移します。
            DropInventoryItem(m_selectedInventory);

            // 削除後の添字ズレを避けるため選択を解除します。
            m_selectedInventory = -1;
        }
    }

    ImGui::Separator();
    ImGui::Text(u8"食料: %d（重量 %d）", m_foodCount, m_foodCount);
    if (m_foodCount > 0 && m_player.hp < 10.0f && ImGui::Button(u8"食料を使う（HP+2）"))
    {
        UseFood();
    }

    // タブ切り替えは今後の実装予定として表示だけ残します。
    ImGui::Text(u8"Q/Eのタブ切り替えは後で実装します。現在は地図ピン用ウィンドウを使います。");

    // 所持品ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawRelicPrompt()
{
    // メインウィンドウの作業領域中央へ、確認ウィンドウ自身の中央を合わせます。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    // 旧器確認ウィンドウのサイズを固定します。
    ImGui::SetNextWindowSize(ImVec2(420.0f, 180.0f), ImGuiCond_Always);

    // 旧器確認ウィンドウを開始します。
    ImGui::Begin(u8"旧器を発見", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // 発見した旧器名を表示します。
    ImGui::Text(u8"%s を発見", GetRelicDisplayName(m_pendingRelic));

    // 発見した旧器の重量を表示します。
    ImGui::Text(u8"重量: %.0f", m_pendingRelic.weight);

    // 拾った後の総重量を表示します。
    ImGui::Text(u8"拾得後重量: %.0f / %.0f", GetCurrentWeight() + m_pendingRelic.weight, GetPickupWeightLimit());

    // 150%上限を超えない場合だけ拾うボタンを出します。
    if (GetCurrentWeight() + m_pendingRelic.weight <= GetPickupWeightLimit())
    {
        // 旧器を所持品に入れます。
        if (ImGui::Button(u8"拾う"))
        {
            // 発見中の旧器を所持品へ追加します。
            m_inventory.push_back(m_pendingRelic);

            // 探索モードへ戻ります。
            m_mode = Mode::Explore;

            // HUDログに取得を出します。
            AddMessage(u8"旧器を拾いました。");
        }

        // Leaveボタンを横並びにします。
        ImGui::SameLine();
    }

    // 150%上限を超える場合は拾えない理由を表示します。
    else
    {
        // 重量上限超過メッセージを表示します。
        ImGui::Text(u8"これ以上は重すぎて拾えません。");
    }

    // 旧器を拾わず地面に置いたままにします。
    if (ImGui::Button(u8"置いていく"))
    {
        // 発見中の旧器を地面旧器として再登録します。
        m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true });

        // 探索モードへ戻ります。
        m_mode = Mode::Explore;

        // HUDログに放置を出します。
        AddMessage(u8"旧器をその場に置きました。");
    }

    // 旧器確認ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawResult()
{
    ImGui::SetNextWindowPos(ImVec2(390.0f, 160.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(540.0f, 360.0f), ImGuiCond_Always);
    ImGui::Begin(m_mode == Mode::DeathResult ? u8"死亡リザルト" : u8"帰還リザルト", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text(u8"結果: %s", m_result.reason.c_str());
    ImGui::Text(u8"最大深度: %.2fm", m_result.maxDepth);
    ImGui::Text(u8"採掘した遺物: %d", m_result.minedCount);
    ImGui::Separator();

    if (m_mode == Mode::ReturnResult)
    {
        ImGui::Text(u8"持ち帰った遺物: %d", m_result.carriedRelics);
        ImGui::Text(u8"今回初めて鑑定した種類: %d", m_result.identifiedRelics);
        ImGui::Text(u8"遺物の売却見込額: %d", m_result.saleAmount);
        ImGui::Spacing();
        if (ImGui::Button(u8"商店へ", ImVec2(120.0f, 0.0f))) m_mode = Mode::GeneralShop;
        ImGui::SameLine();
        if (ImGui::Button(u8"武具屋へ", ImVec2(120.0f, 0.0f))) m_mode = Mode::Armory;
        ImGui::SameLine();
        if (ImGui::Button(u8"自宅へ", ImVec2(120.0f, 0.0f))) m_mode = Mode::Home;
        if (ImGui::Button(u8"持ち込みなしですぐに再潜行", ImVec2(372.0f, 0.0f)))
        {
            m_loadoutFoodCount = 0;
            m_loadoutRelics.fill(0);
            StartDive();
        }
    }
    else
    {
        ImGui::Text(u8"失った遺物: %d", m_result.lostRelics);
        if (ImGui::Button(u8"再挑戦")) RestartAfterDeath();
        ImGui::SameLine();
        if (ImGui::Button(u8"自宅へ")) m_mode = Mode::Home;
    }
    ImGui::End();
}

void SceneNarakuProto::DrawReturnConfirm()
{
    ImGui::SetNextWindowPos(ImVec2(460.0f, 230.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 170.0f), ImGuiCond_Always);
    ImGui::Begin(u8"帰還確認", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"現在の持ち物を持って帰還します。帰還後、未鑑定の遺物は自動で鑑定されます。");
    if (ImGui::Button(u8"帰還する", ImVec2(130.0f, 0.0f))) FinishReturn();
    ImGui::SameLine();
    if (ImGui::Button(u8"探索を続ける", ImVec2(130.0f, 0.0f))) m_mode = Mode::Explore;
    ImGui::End();
}

void SceneNarakuProto::DrawHome()
{
    ImGui::SetNextWindowPos(ImVec2(230.0f, 60.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(760.0f, 700.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"自宅 - 潜行準備", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text(u8"所持金: %d", m_money);
    ImGui::Separator();

    ImGui::Text(u8"頭装備");
    for (std::size_t i = 0; i < m_ownedHeadArmor.size(); ++i)
    {
        if (!m_ownedHeadArmor[i]) continue;
        const ArmorTier tier = static_cast<ArmorTier>(i);
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::RadioButton(GetArmorName(tier), m_equippedHeadArmor == tier)) m_equippedHeadArmor = tier;
        ImGui::PopID();
    }

    ImGui::Text(u8"胴装備");
    for (std::size_t i = 0; i < m_ownedBodyArmor.size(); ++i)
    {
        if (!m_ownedBodyArmor[i]) continue;
        const ArmorTier tier = static_cast<ArmorTier>(i);
        ImGui::PushID(100 + static_cast<int>(i));
        if (ImGui::RadioButton(GetArmorName(tier), m_equippedBodyArmor == tier)) m_equippedBodyArmor = tier;
        ImGui::PopID();
    }

    ImGui::Text(u8"武器");
    for (std::size_t i = 0; i < m_ownedWeapons.size(); ++i)
    {
        if (!m_ownedWeapons[i]) continue;
        const WeaponTier tier = static_cast<WeaponTier>(i);
        ImGui::PushID(200 + static_cast<int>(i));
        if (ImGui::RadioButton(GetWeaponName(tier), m_equippedWeapon == tier)) m_equippedWeapon = tier;
        ImGui::SameLine();
        ImGui::TextDisabled(u8"採掘速度 %.0f%%", (tier == WeaponTier::RustyPickaxe ? 1.0f : tier == WeaponTier::NormalPickaxe ? 1.35f : tier == WeaponTier::SturdyPickaxe ? 1.80f : tier == WeaponTier::SharpPickaxe ? 2.0f : 2.5f) * 100.0f);
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text(u8"次回の持ち込み");
    ImGui::Text(u8"食料  保管:%d  持込:%d", m_storedFoodCount, m_loadoutFoodCount);
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"-##food")) m_loadoutFoodCount = std::max(0, m_loadoutFoodCount - 1);
    ImGui::SameLine();
    if (ImGui::SmallButton(u8"+##food")) m_loadoutFoodCount = std::min(m_storedFoodCount, m_loadoutFoodCount + 1);

    float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount);
    for (std::size_t i = 0; i < m_storedRelics.size(); ++i)
    {
        const RelicType type = static_cast<RelicType>(i);
        loadoutWeight += GetRelicWeight(type) * static_cast<float>(m_loadoutRelics[i]);
        ImGui::PushID(300 + static_cast<int>(i));
        ImGui::Text(u8"%s  保管:%d  持込:%d", GetRelicTypeName(type), m_storedRelics[i], m_loadoutRelics[i]);
        ImGui::SameLine();
        if (ImGui::SmallButton("-")) m_loadoutRelics[i] = std::max(0, m_loadoutRelics[i] - 1);
        ImGui::SameLine();
        if (ImGui::SmallButton("+")) m_loadoutRelics[i] = std::min(m_storedRelics[i], m_loadoutRelics[i] + 1);
        ImGui::PopID();
    }
    const float maxWeight = GetMaxWeight();
    ImGui::Text(u8"開始重量: %.0f / %.0f", loadoutWeight, maxWeight);
    if (loadoutWeight > maxWeight) ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), u8"持ち込み重量を%.0f以下にしてください。", maxWeight);

    if (ImGui::Button(u8"潜行開始", ImVec2(160.0f, 0.0f)) && loadoutWeight <= maxWeight) StartDive();
    ImGui::SameLine();
    if (ImGui::Button(u8"商店", ImVec2(100.0f, 0.0f))) m_mode = Mode::GeneralShop;
    ImGui::SameLine();
    if (ImGui::Button(u8"武具屋", ImVec2(100.0f, 0.0f))) m_mode = Mode::Armory;
    ImGui::End();
}

void SceneNarakuProto::DrawGeneralShop()
{
    ImGui::SetNextWindowPos(ImVec2(300.0f, 90.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(680.0f, 620.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"商店", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text(u8"所持金: %d", m_money);
    ImGui::Separator();
    ImGui::Text(u8"購入");
    if (ImGui::Button(u8"食料を購入  5G（重量1 / HP+2）") && m_money >= 5) { m_money -= 5; ++m_storedFoodCount; }

    const RelicType shopTypes[] = { RelicType::ArmamentUpgrade, RelicType::WeaponUpgrade, RelicType::ArmorUpgrade };
    const int shopPrices[] = { 30, 35, 35 };
    for (int i = 0; i < 3; ++i)
    {
        ImGui::PushID(i);
        char label[160];
        std::snprintf(label, sizeof(label), u8"%sを購入  %dG", GetRelicTypeName(shopTypes[i]), shopPrices[i]);
        if (ImGui::Button(label) && m_money >= shopPrices[i])
        {
            m_money -= shopPrices[i];
            const std::size_t typeIndex = static_cast<std::size_t>(shopTypes[i]);
            ++m_storedRelics[typeIndex];
            m_identifiedRelics[typeIndex] = true;
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text(u8"遺物売却");
    bool hasSellableRelic = false;
    for (std::size_t i = 0; i < m_storedRelics.size(); ++i)
    {
        if (m_storedRelics[i] <= 0) continue;

        hasSellableRelic = true;
        const RelicType type = static_cast<RelicType>(i);
        ImGui::PushID(100 + static_cast<int>(i));
        ImGui::Text(u8"%s  %d個  1個%dG", GetRelicTypeName(type), m_storedRelics[i], GetRelicSellValue(type));
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"1個売却"))
        {
            --m_storedRelics[i];
            m_money += GetRelicSellValue(type);
            m_loadoutRelics[i] = std::min(m_loadoutRelics[i], m_storedRelics[i]);
        }
        ImGui::PopID();
    }
    if (!hasSellableRelic) ImGui::TextDisabled(u8"売却できる遺物はありません。");
    ImGui::TextDisabled(u8"食料は売却できません。");
    ImGui::Separator();
    if (ImGui::Button(u8"自宅へ")) m_mode = Mode::Home;
    ImGui::SameLine();
    if (ImGui::Button(u8"武具屋へ")) m_mode = Mode::Armory;
    ImGui::End();
}

void SceneNarakuProto::DrawArmory()
{
    static const int armorDiscountPrices[][2] = { { 10, 15 }, { 15, 20 }, { 20, 30 }, { 25, 35 }, { 35, 40 }, { 50, 60 } };
    static const int armorMoneyPrices[][2] = { { 10, 15 }, { 30, 40 }, { 40, 60 }, { 50, 70 }, { 70, 80 }, { 100, 120 } };
    static const int armorArmamentCosts[] = { 0, 0, 1, 2, 2, 3 };
    static const int armorMaterialCosts[] = { 0, 0, 1, 1, 2, 5 };
    static const int weaponDiscountPrices[] = { 5, 10, 25, 50, 150 };
    static const int weaponMoneyPrices[] = { 5, 10, 25, 50, 400 };
    static const int weaponArmamentCosts[] = { 0, 0, 0, 0, 2 };
    static const int weaponMaterialCosts[] = { 0, 0, 0, 0, 3 };

    ImGui::SetNextWindowPos(ImVec2(220.0f, 50.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(900.0f, 720.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"武具屋", nullptr, ImGuiWindowFlags_NoCollapse);
    ImGui::Text(u8"所持金: %d", m_money);
    ImGui::Text(u8"素材  武具:%d  武器:%d  装備:%d",
        m_storedRelics[static_cast<std::size_t>(RelicType::ArmamentUpgrade)],
        m_storedRelics[static_cast<std::size_t>(RelicType::WeaponUpgrade)],
        m_storedRelics[static_cast<std::size_t>(RelicType::ArmorUpgrade)]);
    ImGui::Separator();

    ImGui::Text(u8"頭・胴装備");
    for (std::size_t i = 0; i < static_cast<std::size_t>(ArmorTier::Count); ++i)
    {
        const ArmorTier tier = static_cast<ArmorTier>(i);
        ImGui::PushID(static_cast<int>(i));
        ImGui::Text(u8"%s", GetArmorName(tier));
        ImGui::TextDisabled(u8"割引 頭%dG/胴%dG（武具%d・装備%d）  金のみ 頭%dG/胴%dG",
            armorDiscountPrices[i][0], armorDiscountPrices[i][1], armorArmamentCosts[i], armorMaterialCosts[i],
            armorMoneyPrices[i][0], armorMoneyPrices[i][1]);
        if (m_ownedHeadArmor[i]) ImGui::TextDisabled(u8"頭:所有済み");
        else
        {
            if (ImGui::SmallButton(u8"頭 割引購入")) TryBuyArmor(tier, true, true);
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"頭 金のみ")) TryBuyArmor(tier, true, false);
        }
        if (m_ownedBodyArmor[i]) ImGui::TextDisabled(u8"胴:所有済み");
        else
        {
            if (ImGui::SmallButton(u8"胴 割引購入")) TryBuyArmor(tier, false, true);
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"胴 金のみ")) TryBuyArmor(tier, false, false);
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    ImGui::Text(u8"武器");
    for (std::size_t i = 0; i < static_cast<std::size_t>(WeaponTier::Count); ++i)
    {
        const WeaponTier tier = static_cast<WeaponTier>(i);
        ImGui::PushID(100 + static_cast<int>(i));
        ImGui::Text(u8"%s", GetWeaponName(tier));
        ImGui::TextDisabled(u8"割引 %dG（武具%d・武器%d）  金のみ %dG",
            weaponDiscountPrices[i], weaponArmamentCosts[i], weaponMaterialCosts[i], weaponMoneyPrices[i]);
        if (m_ownedWeapons[i]) ImGui::TextDisabled(u8"所有済み");
        else
        {
            if (ImGui::SmallButton(u8"割引購入")) TryBuyWeapon(tier, true);
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"金のみで購入")) TryBuyWeapon(tier, false);
        }
        ImGui::PopID();
    }

    ImGui::Separator();
    if (ImGui::Button(u8"自宅へ")) m_mode = Mode::Home;
    ImGui::SameLine();
    if (ImGui::Button(u8"商店へ")) m_mode = Mode::GeneralShop;
    ImGui::End();
}

void SceneNarakuProto::DrawMapControls()
{
    // マップウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(690.0f, 80.0f), ImGuiCond_FirstUseEver);

    // マップウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(360.0f, 380.0f), ImGuiCond_FirstUseEver);

    // マップウィンドウを開始します。
    ImGui::Begin(u8"地図", nullptr, ImGuiWindowFlags_NoCollapse);

    // 拡大縮小操作のUIを追加します。
    if (ImGui::Button("-"))
    {
        m_mapZoom = std::max(0.5f, m_mapZoom - 0.25f);
    }
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        m_mapZoom = std::min(5.0f, m_mapZoom + 0.25f);
    }
    ImGui::SameLine();
    ImGui::Text("Zoom %.2fx", m_mapZoom);
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        m_mapZoom = 2.0f;
        m_mapScrollOffset = { 0.0f, 0.0f };
    }

    // ミニマップ描画領域の左上座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // ミニマップ描画領域のサイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 460.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const Vec2 mapFocus = { m_player.pos.x + m_mapScrollOffset.x, m_player.pos.y + m_mapScrollOffset.y };

    // ミニマップ背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 28, 28, 255));

    // キャンバス外へのはみ出しを防ぐため、クリッピングを設定します。
    draw->PushClipRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    // 1. 地形セルの描画 (プレイヤー現在深度付近のレイヤーのみ)
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2) continue;
        if (std::fabs(layer.layerDepth - m_player.depth) > 0.5f) continue;

        const float halfWidth = (layer.gridWidth - 1) * layer.cellSize * 0.5f;
        const float halfHeight = (layer.gridHeight - 1) * layer.cellSize * 0.5f;
        const float minX = layer.center.x - halfWidth;
        const float minZ = layer.center.z - halfHeight;

        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                if (flags & NarakuMap::CellAttributeRemoved) continue;

                Vec2 c00 = { minX + cellX * layer.cellSize, minZ + cellZ * layer.cellSize };
                Vec2 c11 = { c00.x + layer.cellSize, c00.y + layer.cellSize };

                Vec2 p00 = WorldToCanvas(canvasPos, canvasSize, c00, m_mapZoom, mapFocus);
                Vec2 p11 = WorldToCanvas(canvasPos, canvasSize, c11, m_mapZoom, mapFocus);

                ImU32 color = IM_COL32(35, 55, 45, 200); // 通常歩行可能
                if (flags & NarakuMap::CellAttributeBlocked)
                {
                    color = IM_COL32(95, 38, 38, 220); // 通行不可（赤系）
                }
                else if (flags & NarakuMap::CellAttributeCliffEdge)
                {
                    color = IM_COL32(85, 75, 40, 200); // 崖端（黄系）
                }
                else if (flags & NarakuMap::CellAttributeHazard)
                {
                    color = IM_COL32(100, 38, 100, 200); // 危険地形（紫系）
                }

                draw->AddRectFilled(ImVec2(p00.x, p00.y), ImVec2(p11.x, p11.y), color);
                draw->AddRect(ImVec2(p00.x, p00.y), ImVec2(p11.x, p11.y), IM_COL32(30, 36, 36, 80));
            }
        }
    }

    // 2. 帰還地点の描画
    Vec2 ret = WorldToCanvas(canvasPos, canvasSize, m_returnPoint, m_mapZoom, mapFocus);
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 6.0f, IM_COL32(80, 180, 255, 255));

    // 3. 採掘ポイントの描画 (採掘済み、または発見済みのみ)
    for (const MiningPoint& point : m_miningPoints)
    {
        if (!(point.mined || point.discovered))
        {
            continue;
        }

        Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, mapFocus);
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.5f, color);
    }

    // 4. マップピンの描画
    for (const Vec2& pin : m_pins)
    {
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin, m_mapZoom, mapFocus);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, IM_COL32(230, 80, 90, 255));
    }

    // 5. プレイヤー位置と向きの描画
    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos, m_mapZoom, mapFocus);
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));
    Vec2 faceEnd = WorldToCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 3.0f / m_mapZoom)), m_mapZoom, mapFocus);
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 1.5f);

    // クリッピングを終了します。
    draw->PopClipRect();

    // ミニマップ外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 145, 145, 255));

    // マウスがミニマップ領域内にあるか調べます。
    bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));

    // 右クリックの長押しドラッグによるマップ移動
    static bool s_isDraggingMap = false;
    if (ImGui::IsMouseClicked(1) && hovered)
    {
        s_isDraggingMap = true;
    }
    if (!ImGui::IsMouseDown(1))
    {
        s_isDraggingMap = false;
    }

    if (s_isDraggingMap)
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * m_mapZoom;
            float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * m_mapZoom;
            if (std::fabs(scaleX) > 0.001f && std::fabs(scaleY) > 0.001f)
            {
                m_mapScrollOffset.x -= delta.x / scaleX;
                m_mapScrollOffset.y += delta.y / scaleY;
            }
        }
    }

    // マウスホイールによるスクロール拡縮
    if (hovered)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            m_mapZoom = std::max(0.5f, std::min(5.0f, m_mapZoom + wheel * 0.25f));
        }
    }

    // ミニマップ上で左クリックされたらピン設置/削除を行います。
    if (hovered && ImGui::IsMouseClicked(0))
    {
        // 現在のマウス座標を取得します。
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // スクリーン座標をワールド座標へ変換してピン操作します。
        TogglePinAt(ScreenToWorld(canvasPos, canvasSize, { mouse.x, mouse.y }, m_mapZoom, mapFocus));
    }

    // ミニマップ描画領域ぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    // ピン操作説明を表示します。
    ImGui::Text(u8"右ドラッグ: マップ移動   左クリック: ピン設置/削除   ホイール: 拡大縮小");

    // マップピンウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::ResetDebugPlayerParams()
{
    // 既存実装で使っていた固定値を、そのまま初期値として再設定します。
    m_debugPlayerParams.walkSpeed = 1.5f;
    m_debugPlayerParams.runSpeed = 2.5f;
    m_debugPlayerParams.ropeSpeed = 1.0f;
    m_debugPlayerParams.attackPower = 1.0f;
    m_debugPlayerParams.runCostPerSecond = 1.5f;
    m_debugPlayerParams.ropeCostPerSecond = 3.0f;
    m_debugPlayerParams.attackCost = 10.0f;
    m_debugPlayerParams.miningCost = 7.0f;
    m_debugPlayerParams.stepCost = 5.0f;
    m_debugPlayerParams.jumpCost = 5.0f;
    m_debugPlayerParams.staminaRecoverPerSecond = 2.0f;
    m_debugPlayerParams.upperLayerAlpha = 0.06f;
    m_cameraDistance = kCameraDefaultDistance;
    m_cameraMinPitchDegrees = kCameraDefaultMinPitchDegrees;
    m_cameraMaxPitchDegrees = kCameraDefaultMaxPitchDegrees;
    NormalizeCameraSettings();
}

bool SceneNarakuProto::LoadDebugPlayerParams()
{
    std::ifstream stream(kPlaytestConfigPath, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const std::string json = buffer.str();

    TryReadJsonFloat(json, "walkSpeed", m_debugPlayerParams.walkSpeed);
    TryReadJsonFloat(json, "runSpeed", m_debugPlayerParams.runSpeed);
    TryReadJsonFloat(json, "ropeSpeed", m_debugPlayerParams.ropeSpeed);
    TryReadJsonFloat(json, "attackPower", m_debugPlayerParams.attackPower);
    TryReadJsonFloat(json, "runCostPerSecond", m_debugPlayerParams.runCostPerSecond);
    TryReadJsonFloat(json, "ropeCostPerSecond", m_debugPlayerParams.ropeCostPerSecond);
    TryReadJsonFloat(json, "attackCost", m_debugPlayerParams.attackCost);
    TryReadJsonFloat(json, "miningCost", m_debugPlayerParams.miningCost);
    TryReadJsonFloat(json, "stepCost", m_debugPlayerParams.stepCost);
    TryReadJsonFloat(json, "jumpCost", m_debugPlayerParams.jumpCost);
    TryReadJsonFloat(json, "staminaRecoverPerSecond", m_debugPlayerParams.staminaRecoverPerSecond);
    TryReadJsonFloat(json, "upperLayerAlpha", m_debugPlayerParams.upperLayerAlpha);
    TryReadJsonFloat(json, "minimapPosX", m_debugPlayerParams.minimapPosX);
    TryReadJsonFloat(json, "minimapPosY", m_debugPlayerParams.minimapPosY);
    TryReadJsonFloat(json, "minimapSize", m_debugPlayerParams.minimapSize);
    TryReadJsonFloat(json, "showMinimap", m_debugPlayerParams.showMinimap);
    TryReadJsonFloat(json, "cameraDistance", m_cameraDistance);
    TryReadJsonFloat(json, "cameraMinPitchDegrees", m_cameraMinPitchDegrees);
    TryReadJsonFloat(json, "cameraMaxPitchDegrees", m_cameraMaxPitchDegrees);
    ClampDebugPlayerParams();
    NormalizeCameraSettings();
    return true;
}

bool SceneNarakuProto::SaveDebugPlayerParams() const
{
    std::ofstream stream(kPlaytestConfigPath, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }
    stream << "{\n"
        << "  \"walkSpeed\": " << m_debugPlayerParams.walkSpeed << ",\n"
        << "  \"runSpeed\": " << m_debugPlayerParams.runSpeed << ",\n"
        << "  \"ropeSpeed\": " << m_debugPlayerParams.ropeSpeed << ",\n"
        << "  \"attackPower\": " << m_debugPlayerParams.attackPower << ",\n"
        << "  \"runCostPerSecond\": " << m_debugPlayerParams.runCostPerSecond << ",\n"
        << "  \"ropeCostPerSecond\": " << m_debugPlayerParams.ropeCostPerSecond << ",\n"
        << "  \"attackCost\": " << m_debugPlayerParams.attackCost << ",\n"
        << "  \"miningCost\": " << m_debugPlayerParams.miningCost << ",\n"
        << "  \"stepCost\": " << m_debugPlayerParams.stepCost << ",\n"
        << "  \"jumpCost\": " << m_debugPlayerParams.jumpCost << ",\n"
        << "  \"staminaRecoverPerSecond\": " << m_debugPlayerParams.staminaRecoverPerSecond << ",\n"
        << "  \"upperLayerAlpha\": " << m_debugPlayerParams.upperLayerAlpha << ",\n"
        << "  \"minimapPosX\": " << m_debugPlayerParams.minimapPosX << ",\n"
        << "  \"minimapPosY\": " << m_debugPlayerParams.minimapPosY << ",\n"
        << "  \"minimapSize\": " << m_debugPlayerParams.minimapSize << ",\n"
        << "  \"showMinimap\": " << m_debugPlayerParams.showMinimap << ",\n"
        << "  \"cameraDistance\": " << m_cameraDistance << ",\n"
        << "  \"cameraMinPitchDegrees\": " << m_cameraMinPitchDegrees << ",\n"
        << "  \"cameraMaxPitchDegrees\": " << m_cameraMaxPitchDegrees << "\n"
        << "}\n";
    return stream.good();
}

void SceneNarakuProto::ClampDebugPlayerParams()
{
    // 速度、攻撃力、各種消費量、回復量は負値にしないよう安全側へ丸めます。
    m_debugPlayerParams.walkSpeed = std::max(0.0f, m_debugPlayerParams.walkSpeed);
    m_debugPlayerParams.runSpeed = std::max(0.0f, m_debugPlayerParams.runSpeed);
    m_debugPlayerParams.ropeSpeed = std::max(0.0f, m_debugPlayerParams.ropeSpeed);
    m_debugPlayerParams.attackPower = std::max(0.0f, m_debugPlayerParams.attackPower);
    m_debugPlayerParams.runCostPerSecond = std::max(0.0f, m_debugPlayerParams.runCostPerSecond);
    m_debugPlayerParams.ropeCostPerSecond = std::max(0.0f, m_debugPlayerParams.ropeCostPerSecond);
    m_debugPlayerParams.attackCost = std::max(0.0f, m_debugPlayerParams.attackCost);
    m_debugPlayerParams.miningCost = std::max(0.0f, m_debugPlayerParams.miningCost);
    m_debugPlayerParams.stepCost = std::max(0.0f, m_debugPlayerParams.stepCost);
    m_debugPlayerParams.jumpCost = std::max(0.0f, m_debugPlayerParams.jumpCost);
    m_debugPlayerParams.staminaRecoverPerSecond = std::max(0.0f, m_debugPlayerParams.staminaRecoverPerSecond);
    m_debugPlayerParams.upperLayerAlpha = std::max(0.0f, std::min(m_debugPlayerParams.upperLayerAlpha, 0.30f));
}

float SceneNarakuProto::GetMiningSpeedMultiplier() const
{
    switch (m_equippedWeapon)
    {
    case WeaponTier::NormalPickaxe: return 1.35f;
    case WeaponTier::SturdyPickaxe: return 1.80f;
    case WeaponTier::SharpPickaxe: return 2.00f;
    case WeaponTier::RelicPickaxe: return 2.50f;
    default: return 1.00f;
    }
}

const char* SceneNarakuProto::GetRelicTypeName(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade: return u8"武具強化遺物";
    case RelicType::WeaponUpgrade: return u8"武器強化遺物";
    case RelicType::ArmorUpgrade: return u8"装備強化遺物";
    case RelicType::Offensive: return u8"攻撃的遺物";
    case RelicType::Survival: return u8"生存的遺物";
    case RelicType::Cash: return u8"換金用遺物";
    case RelicType::MentalRecovery: return u8"精神力回復遺物";
    default: return u8"不明な遺物";
    }
}

const char* SceneNarakuProto::GetRelicDisplayName(const RelicItem& item) const
{
    const std::size_t index = static_cast<std::size_t>(item.type);
    return index < m_identifiedRelics.size() && m_identifiedRelics[index]
        ? GetRelicTypeName(item.type)
        : u8"未鑑定の遺物";
}

float SceneNarakuProto::GetRelicWeight(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade:
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade: return 5.0f;
    case RelicType::Offensive: return 8.0f;
    case RelicType::Survival: return 3.0f;
    case RelicType::Cash: return 10.0f;
    case RelicType::MentalRecovery: return 2.0f;
    default: return 0.0f;
    }
}

int SceneNarakuProto::GetRelicSellValue(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade: return 10;
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade: return 15;
    case RelicType::Offensive: return 50;
    case RelicType::Survival: return 60;
    case RelicType::Cash: return 30;
    case RelicType::MentalRecovery: return 5;
    default: return 0;
    }
}

SceneNarakuProto::RelicItem SceneNarakuProto::CreateRelic(RelicType type, const std::string& sourceName) const
{
    RelicItem item;
    item.name = sourceName;
    item.type = type;
    item.weight = GetRelicWeight(type);
    item.value = GetRelicSellValue(type);
    return item;
}

SceneNarakuProto::RelicItem SceneNarakuProto::CreateRandomRelic(const std::string& sourceName) const
{
    static std::mt19937 randomEngine(std::random_device{}());
    static std::uniform_int_distribution<int> distribution(0, static_cast<int>(RelicType::Count) - 1);
    return CreateRelic(static_cast<RelicType>(distribution(randomEngine)), sourceName);
}

void SceneNarakuProto::UseFood()
{
    if (m_foodCount <= 0)
    {
        AddMessage(u8"食料を持っていません。");
        ShowCenterNotification(u8"食料がない！");
        return;
    }
    if (m_player.hp >= 10.0f)
    {
        AddMessage(u8"体力が満タンのため食料を使いませんでした。");
        return;
    }
    --m_foodCount;
    m_player.hp = std::min(10.0f, m_player.hp + 2.0f);
    AddMessage(u8"食料を使い、体力を2回復しました。");
}

const char* SceneNarakuProto::GetArmorName(ArmorTier tier) const
{
    switch (tier)
    {
    case ArmorTier::Leather: return u8"革装備";
    case ArmorTier::Iron: return u8"鉄装備";
    case ArmorTier::RelicCovered: return u8"遺物で覆われたシリーズ";
    case ArmorTier::RelicHardened: return u8"遺物で固めたシリーズ";
    case ArmorTier::RelicEnhanced: return u8"遺物で強化されたシリーズ";
    case ArmorTier::Relic: return u8"遺物装備";
    default: return u8"不明な装備";
    }
}

bool SceneNarakuProto::HasRelicArmorSetEffect() const
{
    return m_equippedHeadArmor == ArmorTier::Relic &&
        m_equippedBodyArmor == ArmorTier::Relic;
}

const char* SceneNarakuProto::GetWeaponName(WeaponTier tier) const
{
    switch (tier)
    {
    case WeaponTier::RustyPickaxe: return u8"錆びれたつるはし";
    case WeaponTier::NormalPickaxe: return u8"普通のつるはし";
    case WeaponTier::SturdyPickaxe: return u8"丈夫なつるはし";
    case WeaponTier::SharpPickaxe: return u8"鋭利なつるはし";
    case WeaponTier::RelicPickaxe: return u8"遺物付きのつるはし";
    default: return u8"不明な武器";
    }
}

bool SceneNarakuProto::TryBuyArmor(ArmorTier tier, bool headSlot, bool useMaterials)
{
    static const int materialPrices[][2] = { { 10, 15 }, { 15, 20 }, { 20, 30 }, { 25, 35 }, { 35, 40 }, { 50, 60 } };
    static const int moneyPrices[][2] = { { 10, 15 }, { 30, 40 }, { 40, 60 }, { 50, 70 }, { 70, 80 }, { 100, 120 } };
    static const int armamentCosts[] = { 0, 0, 1, 2, 2, 3 };
    static const int armorCosts[] = { 0, 0, 1, 1, 2, 5 };
    const std::size_t tierIndex = static_cast<std::size_t>(tier);
    const int slotIndex = headSlot ? 0 : 1;
    std::array<bool, static_cast<std::size_t>(ArmorTier::Count)>& owned = headSlot ? m_ownedHeadArmor : m_ownedBodyArmor;
    if (owned[tierIndex]) return false;

    const int price = useMaterials ? materialPrices[tierIndex][slotIndex] : moneyPrices[tierIndex][slotIndex];
    const std::size_t armamentIndex = static_cast<std::size_t>(RelicType::ArmamentUpgrade);
    const std::size_t armorIndex = static_cast<std::size_t>(RelicType::ArmorUpgrade);
    if (m_money < price || (useMaterials && (m_storedRelics[armamentIndex] < armamentCosts[tierIndex] || m_storedRelics[armorIndex] < armorCosts[tierIndex])))
    {
        AddMessage(u8"購入に必要な金額または遺物が不足しています。");
        return false;
    }

    m_money -= price;
    if (useMaterials)
    {
        m_storedRelics[armamentIndex] -= armamentCosts[tierIndex];
        m_storedRelics[armorIndex] -= armorCosts[tierIndex];
        m_loadoutRelics[armamentIndex] = std::min(m_loadoutRelics[armamentIndex], m_storedRelics[armamentIndex]);
        m_loadoutRelics[armorIndex] = std::min(m_loadoutRelics[armorIndex], m_storedRelics[armorIndex]);
    }
    owned[tierIndex] = true;
    AddMessage(u8"装備を購入しました。");
    return true;
}

bool SceneNarakuProto::TryBuyWeapon(WeaponTier tier, bool useMaterials)
{
    static const int materialPrices[] = { 5, 10, 25, 50, 150 };
    static const int moneyPrices[] = { 5, 10, 25, 50, 400 };
    static const int armamentCosts[] = { 0, 0, 0, 0, 2 };
    static const int weaponCosts[] = { 0, 0, 0, 0, 3 };
    const std::size_t tierIndex = static_cast<std::size_t>(tier);
    if (m_ownedWeapons[tierIndex]) return false;

    const int price = useMaterials ? materialPrices[tierIndex] : moneyPrices[tierIndex];
    const std::size_t armamentIndex = static_cast<std::size_t>(RelicType::ArmamentUpgrade);
    const std::size_t weaponIndex = static_cast<std::size_t>(RelicType::WeaponUpgrade);
    if (m_money < price || (useMaterials && (m_storedRelics[armamentIndex] < armamentCosts[tierIndex] || m_storedRelics[weaponIndex] < weaponCosts[tierIndex])))
    {
        AddMessage(u8"購入に必要な金額または遺物が不足しています。");
        return false;
    }

    m_money -= price;
    if (useMaterials)
    {
        m_storedRelics[armamentIndex] -= armamentCosts[tierIndex];
        m_storedRelics[weaponIndex] -= weaponCosts[tierIndex];
        m_loadoutRelics[armamentIndex] = std::min(m_loadoutRelics[armamentIndex], m_storedRelics[armamentIndex]);
        m_loadoutRelics[weaponIndex] = std::min(m_loadoutRelics[weaponIndex], m_storedRelics[weaponIndex]);
    }
    m_ownedWeapons[tierIndex] = true;
    AddMessage(u8"武器を購入しました。");
    return true;
}

float SceneNarakuProto::GetCurrentWeight() const
{
    // 初期装備のつるはし重量10から計算を始めます。
    float weight = 10.0f;

    // 所持している旧器の重量をすべて足します。
    for (const RelicItem& item : m_inventory) weight += item.weight;

    // 食料は1個につき重量1として扱います。
    weight += static_cast<float>(m_foodCount);

    // 合計重量を返します。
    return weight;
}

float SceneNarakuProto::GetMaxWeight() const
{
    return HasRelicArmorSetEffect() ? 200.0f : kMaxWeight;
}

float SceneNarakuProto::GetPickupWeightLimit() const
{
    return std::max(kPickupWeightLimit, GetMaxWeight());
}

float SceneNarakuProto::GetWeightRate() const
{
    return GetCurrentWeight() / GetMaxWeight();
}

float SceneNarakuProto::GetMoveSpeed() const
{
    // まず通常歩行速度を基準にします。
    float speed = m_debugPlayerParams.walkSpeed;

    // 重量70%以上では移動速度を25%下げます。
    if (GetWeightRate() >= 0.70f) speed *= 0.75f;

    if (HasRelicArmorSetEffect()) speed *= kRelicArmorWalkMultiplier;

    // 重量補正済みの歩行速度を返します。
    return speed;
}

float SceneNarakuProto::GetStaminaCost(float baseCost) const
{
    // 重量90%以上ではスタミナ消費を2倍にします。
    return GetWeightRate() >= 0.90f ? baseCost * 2.0f : baseCost;
}

bool SceneNarakuProto::CanSpendStamina(float baseCost) const
{
    // 重量補正後の消費量を支払えるかどうかを返します。
    return m_player.stamina >= GetStaminaCost(baseCost) && m_player.stamina > 0.0f;
}

void SceneNarakuProto::SpendStamina(float baseCost)
{
    // 重量補正後の消費量を差し引き、0未満にならないようにします。
    m_player.stamina = std::max(0.0f, m_player.stamina - GetStaminaCost(baseCost));
}

bool SceneNarakuProto::IsNear(const Vec2& a, const Vec2& b, float range) const
{
    // 2点間距離が指定範囲内かを返します。
    return Distance(a, b) <= range;
}

SceneNarakuProto::Vec2 SceneNarakuProto::Normalize(const Vec2& value) const
{
    // ベクトルの長さを計算します。
    float length = std::sqrt(value.x * value.x + value.y * value.y);

    // 長さがほぼ0ならゼロ除算を避けてゼロベクトルを返します。
    if (length <= 0.0001f) return { 0.0f, 0.0f };

    // 各成分を長さで割って正規化します。
    return { value.x / length, value.y / length };
}

float SceneNarakuProto::Distance(const Vec2& a, const Vec2& b) const
{
    // X成分の差を計算します。
    float dx = a.x - b.x;

    // Y成分の差を計算します。
    float dy = a.y - b.y;

    // ピタゴラスの定理で距離を返します。
    return std::sqrt(dx * dx + dy * dy);
}

float SceneNarakuProto::Dot(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトルの内積を返します。
    return a.x * b.x + a.y * b.y;
}

SceneNarakuProto::Vec2 SceneNarakuProto::Add(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトル同士を加算します。
    return { a.x + b.x, a.y + b.y };
}

SceneNarakuProto::Vec2 SceneNarakuProto::Sub(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトル同士を減算します。
    return { a.x - b.x, a.y - b.y };
}

SceneNarakuProto::Vec2 SceneNarakuProto::Mul(const Vec2& a, float scalar) const
{
    // 2Dベクトルにスカラーを掛けます。
    return { a.x * scalar, a.y * scalar };
}

bool SceneNarakuProto::IsInsideFloor(const FloorRegion& floor, const Vec2& pos) const
{
    // 床矩形の左端から右端までに入っているかを調べます。
    const bool insideX = pos.x >= floor.center.x - floor.halfSize.x && pos.x <= floor.center.x + floor.halfSize.x;

    // 床矩形の奥端から手前端までに入っているかを調べます。
    const bool insideY = pos.y >= floor.center.y - floor.halfSize.y && pos.y <= floor.center.y + floor.halfSize.y;

    // XとYの両方が範囲内なら床上として扱います。
    return insideX && insideY;
}

const SceneNarakuProto::FloorRegion* SceneNarakuProto::FindFloorAt(const Vec2& pos, float depth) const
{
    constexpr float kFloorDepthTolerance = 0.20f;

    for (const FloorRegion& floor : m_floorRegions)
    {
        if (std::fabs(floor.depth - depth) > kFloorDepthTolerance)
        {
            continue;
        }

        if (!IsInsideFloor(floor, pos))
        {
            continue;
        }

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, floor.layerId);
        if (layerIndex >= 0)
        {
            const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
            int cellX = -1;
            int cellZ = -1;
            float fracX = 0.0f;
            float fracZ = 0.0f;
            if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
            {
                continue;
            }

            if (NarakuMap::HasCellAttributeFlag(layer, cellX, cellZ, NarakuMap::CellAttributeBlocked) ||
                NarakuMap::HasCellAttributeFlag(layer, cellX, cellZ, NarakuMap::CellAttributeRemoved))
            {
                continue;
            }
        }

        return &floor;
    }

    return nullptr;
}

bool SceneNarakuProto::HasFloorAt(const Vec2& pos, float depth) const
{
    // 床ポインタが見つかるかどうかだけを真偽値に変換します。
    return FindFloorAt(pos, depth) != nullptr;
}

std::uint32_t SceneNarakuProto::GetCellAttributeFlagsAt(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return NarakuMap::CellAttributeNone;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
    {
        return NarakuMap::CellAttributeNone;
    }

    return NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
}

bool SceneNarakuProto::CanTraverseGround(const Vec2& from, const Vec2& to, float depth) const
{
    if (!HasFloorAt(to, depth))
    {
        return false;
    }

    const float fromHeight = SampleTerrainHeightOffsetAt(from, depth);
    const float toHeight = SampleTerrainHeightOffsetAt(to, depth);
    const float climbDelta = toHeight - fromHeight;
    const float dropDelta = fromHeight - toHeight;
    const std::uint32_t fromFlags = GetCellAttributeFlagsAt(from, depth);
    const std::uint32_t toFlags = GetCellAttributeFlagsAt(to, depth);
    const bool dropAllowed = ((fromFlags | toFlags) & NarakuMap::CellAttributeDropAllowed) != 0u;
    const bool cliffEdge = ((fromFlags | toFlags) & NarakuMap::CellAttributeCliffEdge) != 0u;

    if (climbDelta > kMaxWalkClimbHeight + kSlopeHeightTolerance)
    {
        return false;
    }

    if (dropDelta > kMaxWalkDropHeight + kSlopeHeightTolerance && !dropAllowed)
    {
        return false;
    }

    if (cliffEdge && dropDelta > kCliffEdgeBlockDropHeight + kSlopeHeightTolerance && !dropAllowed)
    {
        return false;
    }

    return true;
}

SceneNarakuProto::Vec2 SceneNarakuProto::ResolveFloorMove(const Vec2& from, const Vec2& to, float depth) const
{
    auto tryResolveSingleStep = [this, depth](const Vec2& stepFrom, const Vec2& stepTo, Vec2& outResolved) -> bool
    {
        if (CanTraverseGround(stepFrom, stepTo, depth))
        {
            outResolved = stepTo;
            return true;
        }

        const Vec2 xOnly = { stepTo.x, stepFrom.y };
        if (CanTraverseGround(stepFrom, xOnly, depth))
        {
            outResolved = xOnly;
            return true;
        }

        const Vec2 yOnly = { stepFrom.x, stepTo.y };
        if (CanTraverseGround(stepFrom, yOnly, depth))
        {
            outResolved = yOnly;
            return true;
        }

        outResolved = stepFrom;
        return false;
    };

    const float totalDistance = Distance(from, to);
    if (totalDistance <= kSlopeMoveSampleStep)
    {
        Vec2 resolved = from;
        return tryResolveSingleStep(from, to, resolved) ? resolved : from;
    }

    const int stepCount = std::max(1, static_cast<int>(std::ceil(totalDistance / kSlopeMoveSampleStep)));
    const Vec2 delta = Sub(to, from);
    Vec2 current = from;

    for (int stepIndex = 1; stepIndex <= stepCount; ++stepIndex)
    {
        const float t = static_cast<float>(stepIndex) / static_cast<float>(stepCount);
        const Vec2 stepTarget = Add(from, Mul(delta, t));
        Vec2 resolved = current;
        if (!tryResolveSingleStep(current, stepTarget, resolved))
        {
            break;
        }
        current = resolved;
    }

    return current;
}

int SceneNarakuProto::FindNearestRopeIndex(float range) const
{
    // 近いロープを選ぶため、現在の最短距離を範囲上限から始めます。
    float bestDistance = range;

    // 見つかったロープ番号です。未発見なら -1 のままにします。
    int bestIndex = -1;

    // 全ロープを調べ、範囲内で一番近いものを探します。
    for (int i = 0; i < static_cast<int>(m_ropePoints.size()); ++i)
    {
        const RopePoint& rope = m_ropePoints[i];
        const float topDistance = std::fabs(m_player.depth - rope.topDepth) <= 0.35f ? Distance(m_player.pos, rope.topPos) : range + 1.0f;
        const float bottomDistance = std::fabs(m_player.depth - rope.bottomDepth) <= 0.35f ? Distance(m_player.pos, rope.bottomPos) : range + 1.0f;
        const float d = std::min(topDistance, bottomDistance);

        // 現在の候補より近ければ採用します。
        if (d <= bestDistance)
        {
            bestDistance = d;
            bestIndex = i;
        }
    }

    // 範囲内にロープがなければ -1 を返します。
    return bestIndex;
}

bool SceneNarakuProto::TryLeaveRopeSide(int ropeIndex, float leaveSign, const Vec2& cameraRight)
{
    // 無効なロープ番号なら何もしません。
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return false;
    }

    // 対象ロープを取得します。
    const RopePoint& rope = m_ropePoints[ropeIndex];

    const Vec2 ropePos = GetRopePosition(ropeIndex, m_ropeProgress);
    const Vec2 leavePos = Add(ropePos, Mul(cameraRight, leaveSign * 0.80f));

    // 候補位置に床があり、地形条件も満たすならそこへ降ります。
    if (CanTraverseGround(ropePos, leavePos, m_player.depth))
    {
        m_player.pos = leavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        m_player.grounded = true;
        m_player.verticalSpeed = 0.0f;
        m_player.airTime = 0.0f;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.landingRecoveryTimer = 0.0f;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 深度が端にかなり近い場合は端深度へ吸着して、降りられるかをもう一度試します。
    const bool useBottom = m_ropeProgress >= 0.5f;
    const float endpointDepth = useBottom ? rope.bottomDepth : rope.topDepth;
    const Vec2 endpointPos = useBottom ? rope.bottomPos : rope.topPos;
    const Vec2 endpointLeavePos = Add(endpointPos, Mul(cameraRight, leaveSign * 0.80f));
    if ((m_ropeProgress <= 0.05f || m_ropeProgress >= 0.95f) && CanTraverseGround(endpointPos, endpointLeavePos, endpointDepth))
    {
        m_player.depth = endpointDepth;
        m_player.pos = endpointLeavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        m_player.grounded = true;
        m_player.verticalSpeed = 0.0f;
        m_player.airTime = 0.0f;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.landingRecoveryTimer = 0.0f;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 周囲に足場がない場合はロープから離れません。
    return false;
}

void SceneNarakuProto::AddMessage(const std::string& message)
{
    // 新しいログを末尾に追加します。
    m_messages.push_back(message);

    // ログが増えすぎないよう古いものを削除します。
    if (m_messages.size() > 24) m_messages.erase(m_messages.begin());
}

void SceneNarakuProto::ShowCenterNotification(const std::string& message)
{
    m_centerNotification = message;
    m_centerNotificationTimer = 1.5f;
}

void SceneNarakuProto::DiscoverNearbyMiningPoints()
{
    // すべての採掘ポイントを確認します。
    for (MiningPoint& point : m_miningPoints)
    {
        // 未発見かつプレイヤーが近いポイントだけ発見済みにします。
        if (!point.discovered && std::fabs(m_player.depth - point.depth) <= 0.35f && IsNear(m_player.pos, point.pos, kDiscoverRange))
        {
            // 採掘ポイントを発見済みにします。
            point.discovered = true;

            // HUDログに発見を出します。
            AddMessage(u8"採掘ポイントを発見しました。");
        }
    }
}

void SceneNarakuProto::DropInventoryItem(int index)
{
    // 範囲外の番号なら何もしません。
    if (index < 0 || index >= static_cast<int>(m_inventory.size())) return;

    // 選択旧器を現在位置の地面旧器として追加します。
    m_groundRelics.push_back({ m_inventory[index], m_player.pos, m_player.depth, true });

    // 所持品から選択旧器を削除します。
    m_inventory.erase(m_inventory.begin() + index);

    // HUDログに捨てたことを出します。
    AddMessage(u8"旧器を捨てました。");
}

void SceneNarakuProto::TogglePinAt(const Vec2& worldPos)
{
    // 既存ピンの近くなら削除扱いにします。
    for (int i = 0; i < static_cast<int>(m_pins.size()); ++i)
    {
        // クリック位置から1m以内のピンを削除対象にします。
        if (Distance(m_pins[i], worldPos) <= 1.0f)
        {
            // 対象ピンを削除します。
            m_pins.erase(m_pins.begin() + i);

            // HUDログに削除を出します。
            AddMessage(u8"ピンを削除しました。");

            // 削除したので追加処理は行いません。
            return;
        }
    }

    // 近くにピンがなければ新しいピンを追加します。
    m_pins.push_back(worldPos);

    // HUDログに追加を出します。
    AddMessage(u8"ピンを設置しました。");
}

SceneNarakuProto::Vec2 SceneNarakuProto::ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos, float zoom, const Vec2& focusPos) const
{
    float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * zoom;
    float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * zoom;

    if (std::fabs(scaleX) < 0.001f) scaleX = 1.0f;
    if (std::fabs(scaleY) < 0.001f) scaleY = 1.0f;

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;

    float dx = (mousePos.x - centerX) / scaleX;
    float dy = (centerY - mousePos.y) / scaleY;

    return { focusPos.x + dx, focusPos.y + dy };
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float zoom, const Vec2& focusPos) const
{
    float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * zoom;
    float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * zoom;

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;

    float dx = worldPos.x - focusPos.x;
    float dy = worldPos.y - focusPos.y;

    return { centerX + dx * scaleX, centerY - dy * scaleY };
}

int SceneNarakuProto::FindLayerIndexByDepth(float depth, float tolerance) const
{
    for (int i = 0; i < static_cast<int>(m_runtimeMap.terrainLayers.size()); ++i)
    {
        if (std::fabs(m_runtimeMap.terrainLayers[i].layerDepth - depth) <= tolerance)
        {
            return i;
        }
    }
    return -1;
}

int SceneNarakuProto::FindLayerIndexAt(const Vec2& pos, float depth, float tolerance) const
{
    for (int i = 0; i < static_cast<int>(m_runtimeMap.terrainLayers.size()); ++i)
    {
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[i];
        if (std::fabs(layer.layerDepth - depth) > tolerance)
        {
            continue;
        }

        int cellX = -1;
        int cellZ = -1;
        float fracX = 0.0f;
        float fracZ = 0.0f;
        if (TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
        {
            return i;
        }
    }

    return -1;
}

bool SceneNarakuProto::TryGetLayerCellAt(const NarakuMap::TerrainLayer& layer, const Vec2& pos, int& outCellX, int& outCellZ, float& outFracX, float& outFracZ) const
{
    if (layer.gridWidth < 2 || layer.gridHeight < 2 || layer.cellSize <= 0.0f)
    {
        return false;
    }

    const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float localX = (pos.x - minX) / layer.cellSize;
    const float localZ = (pos.y - minZ) / layer.cellSize;
    if (localX < 0.0f || localZ < 0.0f || localX > static_cast<float>(layer.gridWidth - 1) || localZ > static_cast<float>(layer.gridHeight - 1))
    {
        return false;
    }

    outCellX = static_cast<int>(std::floor(localX));
    outCellZ = static_cast<int>(std::floor(localZ));
    outCellX = std::max(0, std::min(outCellX, layer.gridWidth - 2));
    outCellZ = std::max(0, std::min(outCellZ, layer.gridHeight - 2));
    outFracX = std::max(0.0f, std::min(localX - static_cast<float>(outCellX), 1.0f));
    outFracZ = std::max(0.0f, std::min(localZ - static_cast<float>(outCellZ), 1.0f));
    return true;
}

float SceneNarakuProto::SampleTerrainHeightOffsetAt(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return 0.0f;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
    {
        return 0.0f;
    }

    const float h00 = NarakuMap::GetVertexHeight(layer, cellX, cellZ);
    const float h10 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ);
    const float h01 = NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1);
    const float h11 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1);
    const float hx0 = h00 + (h10 - h00) * fracX;
    const float hx1 = h01 + (h11 - h01) * fracX;
    return hx0 + (hx1 - hx0) * fracZ;
}

float SceneNarakuProto::GetGroundWorldY(const Vec2& pos, float depth) const
{
    return SampleTerrainHeightOffsetAt(pos, depth) - depth * 0.35f;
}

float SceneNarakuProto::GetPlayerAirborneOffset() const
{
    if (m_player.grounded || m_player.onRope)
    {
        return 0.0f;
    }

    return std::max(0.0f, m_player.feetWorldY - GetGroundWorldY(m_player.pos, m_player.depth));
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetRopePosition(int ropeIndex, float progress) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return m_player.pos;
    }

    const RopePoint& rope = m_ropePoints[ropeIndex];
    const float t = std::max(0.0f, std::min(progress, 1.0f));
    return {
        rope.topPos.x + (rope.bottomPos.x - rope.topPos.x) * t,
        rope.topPos.y + (rope.bottomPos.y - rope.topPos.y) * t };
}

float SceneNarakuProto::GetRopeWorldY(int ropeIndex, float progress) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return GetGroundWorldY(m_player.pos, m_player.depth);
    }

    const RopePoint& rope = m_ropePoints[ropeIndex];
    const float topWorldY = GetGroundWorldY(rope.topPos, rope.topDepth);
    const float bottomWorldY = GetGroundWorldY(rope.bottomPos, rope.bottomDepth);
    const float t = std::max(0.0f, std::min(progress, 1.0f));
    return topWorldY + (bottomWorldY - topWorldY) * t;
}
DirectX::XMFLOAT3 SceneNarakuProto::GetTerrainVertexWorld3D(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ, float heightOffset) const
{
    const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float x = minX + static_cast<float>(gridX) * layer.cellSize;
    const float z = minZ + static_cast<float>(gridZ) * layer.cellSize;
    const float terrainHeight = NarakuMap::GetVertexHeight(layer, gridX, gridZ);
    return { x, terrainHeight + heightOffset - layer.layerDepth * 0.35f, z };
}

DirectX::XMFLOAT3 SceneNarakuProto::ToWorld3D(const Vec2& pos, float depth, float heightOffset) const
{
    const float terrainHeight = SampleTerrainHeightOffsetAt(pos, depth);
    return { pos.x, terrainHeight + heightOffset - depth * 0.35f, pos.y };
}

void SceneNarakuProto::DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale, float yawRad) const
{
    using namespace DirectX;

    // 指定サイズへ拡大する行列を作ります。
    const XMMATRIX scaling = XMMatrixScaling(scale.x, scale.y, scale.z);

    // Y軸回転で向きを調整する行列を作ります。
    const XMMATRIX rotation = XMMatrixRotationY(yawRad);

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 拡大、回転、移動の順でワールド行列を組み立てます。
    const XMMATRIX worldMatrix = scaling * rotation * translation;

    // 既存Geometoryは行列を転置して渡す前提なので、ワールド行列も転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);

    // デバッグ箱を描画します。
    Geometory::DrawBox();
}

void SceneNarakuProto::DrawDebugSphere3D(const DirectX::XMFLOAT3& pos, float radius) const
{
    using namespace DirectX;

    // 半径をXYZ同じ倍率として扱い、球サイズを調整します。
    const XMMATRIX scaling = XMMatrixScaling(radius, radius, radius);

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 拡大してから移動するワールド行列を組み立てます。
    const XMMATRIX worldMatrix = scaling * translation;

    // 既存Geometoryは行列を転置して渡す前提なので、ワールド行列も転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);

    // デバッグ球を描画します。
    Geometory::DrawSphere();
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToObliqueCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float depthOffset) const
{
    // 斜め見下ろし用にワールド座標を45度回したX成分へ変換します。
    float projectedX = worldPos.x - worldPos.y;

    // 斜め見下ろし用に奥行きを圧縮したY成分へ変換します。
    float projectedY = (worldPos.x + worldPos.y) * 0.45f;

    // 深度が大きいほど画面下へずらし、潜っている感覚を足します。
    projectedY += depthOffset * 0.35f;

    // 投影後のXをキャンバス幅に収まるよう0-1へ正規化します。
    float nx = (projectedX / (m_worldHalfSize * 2.0f) + 1.0f) * 0.5f;

    // 投影後のYをキャンバス高さに収まるよう0-1へ正規化します。
    float ny = (projectedY / (m_worldHalfSize * 1.35f) + 1.0f) * 0.5f;

    // 正規化座標をキャンバス上のスクリーン座標へ変換します。
    return { canvasPos.x + nx * canvasSize.x, canvasPos.y + ny * canvasSize.y };
}

void SceneNarakuProto::DrawMiniMap()
{
    if (m_debugPlayerParams.showMinimap < 0.5f) return;

    float posX = m_debugPlayerParams.minimapPosX;
    float posY = m_debugPlayerParams.minimapPosY;
    float size = m_debugPlayerParams.minimapSize;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(size, size), ImGuiCond_Always);

    ImGui::Begin("MiniMap#MiniMapWindow", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings);

    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };
    Vec2 canvasSize = { size, size };
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 24, 24, 180));
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(100, 120, 110, 255), 0.0f, 0, 1.5f);

    Vec2 ret = WorldToCanvas(canvasPos, canvasSize, m_returnPoint, m_mapZoom, m_player.pos);
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 6.0f, IM_COL32(80, 180, 255, 255));

    for (const RopePoint& rope : m_ropePoints)
    {
        const Vec2 top = WorldToCanvas(canvasPos, canvasSize, rope.topPos, m_mapZoom, m_player.pos);
        const Vec2 bottom = WorldToCanvas(canvasPos, canvasSize, rope.bottomPos, m_mapZoom, m_player.pos);
        draw->AddLine(ImVec2(top.x, top.y), ImVec2(bottom.x, bottom.y), IM_COL32(170, 120, 70, 255), 3.0f);
    }

    for (const MiningPoint& point : m_miningPoints)
    {
        const bool visible = point.mined || point.discovered;
        if (!visible) continue;

        Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, m_player.pos);
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, color);
    }

    for (const GroundRelic& relic : m_groundRelics)
    {
        if (!relic.active) continue;
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, relic.pos, m_mapZoom, m_player.pos);
        draw->AddRectFilled(ImVec2(p.x - 2.0f, p.y - 2.0f), ImVec2(p.x + 2.0f, p.y + 2.0f), IM_COL32(240, 220, 130, 255));
    }

    for (const Vec2& pin : m_pins)
    {
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin, m_mapZoom, m_player.pos);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 3.0f, IM_COL32(230, 80, 90, 255));
    }

    for (const EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive) continue;
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, enemy.pos, m_mapZoom, m_player.pos);
        ImU32 color = enemy.telegraphTimer > 0.0f ? IM_COL32(255, 200, 60, 255) : IM_COL32(210, 70, 70, 255);
        if (enemy.chargeTimer > 0.0f) color = IM_COL32(255, 80, 40, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 5.0f, color);
    }

    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos, m_mapZoom, m_player.pos);
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));

    Vec2 faceEnd = WorldToCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 3.0f / m_mapZoom)), m_mapZoom, m_player.pos);
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 1.5f);

    ImGui::End();
}

void SceneNarakuProto::DrawPlayerPositionDebug()
{
    // デバッグウィンドウを表示します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 400.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"プレイヤー位置デバッグ", nullptr))
    {
        // プレイヤーの現在位置と高さを表示
        ImGui::Text(u8"現在位置 X: %.3f, Z: %.3f", m_player.pos.x, m_player.pos.y);
        ImGui::Text(u8"現在高さ Y: %.3f (FeetWorldY: %.3f)", GetPlayerAirborneOffset() + GetGroundWorldY(m_player.pos, m_player.depth), m_player.feetWorldY);
        ImGui::Text(u8"現在深度: %.2f", m_player.depth);
        ImGui::Separator();

        // 生成済みマップから小ステージのグリッド座標を計算
        int gridX = -1;
        int gridZ = -1;
        int stageGridSize = 0;
        if (!m_runtimeMap.pieceNames.empty() && !m_runtimeMap.terrainLayers.empty())
        {
            stageGridSize = static_cast<int>(std::sqrt(static_cast<float>(m_runtimeMap.pieceNames.size())));
            if (stageGridSize * stageGridSize == static_cast<int>(m_runtimeMap.pieceNames.size()))
            {
                const NarakuMap::TerrainLayer& firstLayer = m_runtimeMap.terrainLayers.front();
                const float stageWidth = static_cast<float>(firstLayer.gridWidth - 1) * firstLayer.cellSize;
                const float stageHeight = static_cast<float>(firstLayer.gridHeight - 1) * firstLayer.cellSize;
                const float mapHalfWidth = stageWidth * static_cast<float>(stageGridSize) * 0.5f;
                const float mapHalfHeight = stageHeight * static_cast<float>(stageGridSize) * 0.5f;
                if (stageWidth > 0.0f && m_player.pos.x >= -mapHalfWidth && m_player.pos.x <= mapHalfWidth)
                {
                    gridX = std::min(
                        stageGridSize - 1,
                        static_cast<int>((m_player.pos.x + mapHalfWidth) / stageWidth));
                }
                if (stageHeight > 0.0f && m_player.pos.y >= -mapHalfHeight && m_player.pos.y <= mapHalfHeight)
                {
                    gridZ = std::min(
                        stageGridSize - 1,
                        static_cast<int>((m_player.pos.y + mapHalfHeight) / stageHeight));
                }
            }
        }

        ImGui::Text(u8"グリッド座標: (%d, %d)", gridX, gridZ);

        std::string pieceName = u8"不明";
        if (m_runtimeMap.pieceNames.empty())
        {
            pieceName = u8"未生成(再生成してください)";
        }
        else if (gridX >= 0 && gridX < stageGridSize && gridZ >= 0 && gridZ < stageGridSize)
        {
            size_t index = static_cast<size_t>(gridZ * stageGridSize + gridX);
            if (index < m_runtimeMap.pieceNames.size())
            {
                pieceName = m_runtimeMap.pieceNames[index];
            }
        }
        ImGui::Text(u8"小ステージ名: %s", pieceName.c_str());
        ImGui::Separator();

        // デバッグの障害調査として、プレイヤーの現在位置のセル属性も表示
        int currentLayerIndex = FindLayerIndexByDepth(m_player.depth);
        if (currentLayerIndex >= 0 && currentLayerIndex < static_cast<int>(m_runtimeMap.terrainLayers.size()))
        {
            const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[currentLayerIndex];
            float halfWidth = (layer.gridWidth - 1) * layer.cellSize * 0.5f;
            float halfHeight = (layer.gridHeight - 1) * layer.cellSize * 0.5f;
            float relativeX = m_player.pos.x - (layer.center.x - halfWidth);
            float relativeZ = m_player.pos.y - (layer.center.z - halfHeight);
            int cellX = static_cast<int>(std::floor(relativeX / layer.cellSize));
            int cellZ = static_cast<int>(std::floor(relativeZ / layer.cellSize));

            if (cellX >= 0 && cellX < layer.gridWidth - 1 && cellZ >= 0 && cellZ < layer.gridHeight - 1)
            {
                std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                std::string attr = "";
                if (flags & NarakuMap::CellAttributeBlocked) attr += "Blocked ";
                if (flags & NarakuMap::CellAttributeCliffEdge) attr += "CliffEdge ";
                if (flags & NarakuMap::CellAttributeHazard) attr += "Hazard ";
                if (flags & NarakuMap::CellAttributeRemoved) attr += "Removed ";
                if (attr.empty()) attr = "None (Walkable)";
                ImGui::Text(u8"現在セル(%d, %d) 属性: %s", cellX, cellZ, attr.c_str());
            }
            else
            {
                ImGui::Text(u8"現在セル: レイヤー範囲外 (%d, %d)", cellX, cellZ);
            }
        }
        else
        {
            ImGui::Text(u8"現在レイヤー: 不明 (深度 %.2f)", m_player.depth);
        }
    }
    ImGui::End();
}

void SceneNarakuProto::DrawMiningProgressBar()
{
    if (m_miningIndex < 0) return;

    float barWidth = 260.0f;
    float barHeight = 45.0f;

    // OS画面ではなく、メインウィンドウの作業領域中央を配置基準にします。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(barWidth, barHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("MiningProgressOverlay##Overlay", nullptr, flags))
    {
        std::string text = u8"採掘中...";
        float textWidth = ImGui::CalcTextSize(text.c_str()).x;
        ImGui::SetCursorPosX((barWidth - textWidth) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), text.c_str());

        float progress = std::max(0.0f, std::min(1.0f, 1.0f - (m_miningTimer / m_miningDuration)));
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 18.0f), "");
    }
    ImGui::End();
}

void SceneNarakuProto::DrawCenterNotification()
{
    if (m_centerNotificationTimer <= 0.0f || m_centerNotification.empty()) return;

    constexpr float overlayWidth = 420.0f;
    constexpr float overlayHeight = 48.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("CenterNotification##Overlay", nullptr, flags))
    {
        const ImVec2 textSize = ImGui::CalcTextSize(m_centerNotification.c_str());
        ImGui::SetCursorPos(ImVec2(
            std::max(0.0f, (overlayWidth - textSize.x) * 0.5f),
            std::max(0.0f, (overlayHeight - textSize.y) * 0.5f)));
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.22f, 1.0f), "%s", m_centerNotification.c_str());
    }
    ImGui::End();
}
