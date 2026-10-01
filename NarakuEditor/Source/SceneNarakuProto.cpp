/**
 * @file SceneNarakuProto.Lifecycle.cpp
 * @brief 生成、破棄、Editorプレビュー、および描画バッチのライフサイクルを実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

SceneNarakuProto::SceneNarakuProto()
{
    InitializeTerrainFloorBatch();
    InitializeEnemyBillboardBatch();

    m_attackHitTexture = new Texture();
    if (FAILED(m_attackHitTexture->Create("Assets/Texture/Effect/yellow_bom.png")))
    {
        SAFE_DELETE(m_attackHitTexture);
    }

    m_playerTexture = new Texture();
    if (FAILED(m_playerTexture->Create("Assets/Player/Chara/hero.png")))
    {
        SAFE_DELETE(m_playerTexture);
    }

    m_jumpEffectTexture = new Texture();
    if (FAILED(m_jumpEffectTexture->Create("Assets/Player/VFX/jumpFX.png")))
    {
        SAFE_DELETE(m_jumpEffectTexture);
    }

    m_skyModel = new Model();
    if (!m_skyModel->Load("Assets/Model/sky/sky.obj"))
    {
        SAFE_DELETE(m_skyModel);
    }

    LoadBaseModels();
    LoadRopeModel();
    LoadMiningPointModel();
    LoadPickaxeModel();

    // プレイテスト用の調整値を既定値で初期化します。
    ResetDebugPlayerParams();
    LoadDebugPlayerParams();

    // 初期装備を所有・装備済みにします。
    m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedWeapons[static_cast<std::size_t>(WeaponTier::RustyPickaxe)] = true;

    InitializeNewProgress();
    LoadProgress();

#if defined(NARAKU_EDITOR_BUILD)
    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    m_editorPreviewGenerationActive = true;
    m_editorPreviewGenerationSeed = preview.seed;
    m_loadingStatus = u8"15段階の構成を準備しています...";
    m_loadingProgress = 0.0f;
    m_loadingStep = 0;
    m_mode = Mode::Loading;
#else
    // フィールドを準備し、最初は自宅で持ち物を決められる状態にします。
    ResetRun();
    if (m_deathRecoveryPending)
    {
        m_result.reason = m_pendingDeathReason.empty() ? u8"死亡しました。" : m_pendingDeathReason;
        m_result.lostRelics = static_cast<int>(m_pendingDeathRecoveryRelics.size());
        m_result.levelBeforeDeath = m_pendingDeathLevelBefore;
        m_result.levelAfterDeath = m_pendingDeathLevelAfter;
        m_result.protectionConsumed = m_pendingDeathProtectionConsumed;
        m_mode = Mode::DeathResult;
    }
    else
    {
        BuildSurfaceRuntime(false);
        m_mode = Mode::Home;
    }
#endif
}

SceneNarakuProto::~SceneNarakuProto()
{
#if !defined(NARAKU_EDITOR_BUILD)
    if (m_mode == Mode::Explore || (m_mode == Mode::Inventory && m_overlayReturnMode == Mode::Explore) ||
        m_mode == Mode::RelicPrompt || m_mode == Mode::WaterPrompt ||
        m_mode == Mode::ReturnConfirm || m_mode == Mode::AbandonConfirm || m_mode == Mode::Loading ||
        m_mode == Mode::LayerTransition || m_mode == Mode::UninsuredDescentConfirm ||
        m_mode == Mode::SecondBase || m_mode == Mode::ForwardBase ||
        (m_mode == Mode::SaveError && m_pendingModeAfterSave == Mode::Explore))
    {
        if (m_mode == Mode::RelicPrompt && m_pendingRelicMiningIndex >= 0)
        {
            m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true,
                m_pendingRelicMiningIndex });
            m_pendingRelicMiningIndex = -1;
        }
        FailActivePromotionQuest();
        ApplyAbandonPenalty();
        m_inventory.clear();
        m_foodCount = 0;
        m_heatedFoodCount = 0;
        m_rationOneCount = 0;
        m_rawFishCount = 0;
        m_cookedFishCount = 0;
        m_rawSizedFish.fill(0);
        m_cookedSizedFish.fill(0);
        m_cartridgeCount = 0;
        m_waterBottles.clear();
        m_cookingKits.clear();
    }
    SaveProgress();
#endif
    ReleaseBaseModels();
    ReleaseRopeModel();
    ReleaseMiningPointModel();
    ReleasePickaxeModel();
    ReleaseEnvironmentModels();
    ReleaseEnemyBillboardBatch();
    ReleaseTerrainFloorBatch();
    SAFE_DELETE(m_skyModel);
    SAFE_DELETE(m_attackHitTexture);
    SAFE_DELETE(m_jumpEffectTexture);
    SAFE_DELETE(m_playerTexture);
}

#if defined(NARAKU_EDITOR_BUILD)
bool SceneNarakuProto::VerifyPreviewGenerationDeterminism(std::string& outError)
{
    if (!NarakuStageGenerator::VerifyPieceAreaTagRules(outError)) return false;
    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    const auto finishGeneration = [&outError](SceneNarakuProto& scene)
    {
        constexpr int maximumFrames = 256;
        for (int frame = 0; frame < maximumFrames && scene.m_mode == Mode::Loading; ++frame)
        {
            scene.UpdateLoading();
        }
        if (scene.m_mode == Mode::Loading || scene.m_editorPreviewGenerationFailed)
        {
            outError = scene.m_generationFailureSummary;
            if (!scene.m_generationFailureDetail.empty())
            {
                outError += "\n" + scene.m_generationFailureDetail;
            }
            if (outError.empty()) outError = u8"15段階生成が規定フレーム内に完了しませんでした。";
            return false;
        }
        return true;
    };

    const auto createSignature = [](const SceneNarakuProto& scene)
    {
        std::ostringstream signature;
        signature << "startArea=" << scene.m_currentAreaIndex << '\n';
        std::array<int, 15> stageCounts = {};
        for (const AreaState& area : scene.m_areas)
        {
            const int stage = (area.depth - 1) * 3 + area.sublayer;
            if (stage >= 0 && stage < 15) ++stageCounts[static_cast<std::size_t>(stage)];
            signature << area.depth << ',' << area.sublayer << ',' << area.areaNumber << ':';
            for (const PlannedLayerGate& gate : area.plannedGates)
                signature << gate.isEntry << ',' << gate.destinationAreaIndex << ',' << gate.connectionId << ';';
            signature << '|';
            for (const std::string& pieceName : area.map.pieceNames) signature << pieceName << ';';
            signature << '\n';
        }
        signature << "counts:";
        for (int count : stageCounts) signature << count << ',';
        return signature.str();
    };

    std::string firstSignature;
    {
        SceneNarakuProto first;
        if (!finishGeneration(first)) return false;
        if (preview.spawnAtReturnArea &&
            (first.m_currentAreaIndex < 0 ||
                first.m_currentAreaIndex >= static_cast<int>(first.m_areas.size()) ||
                !first.m_areas[static_cast<std::size_t>(first.m_currentAreaIndex)].canReturn))
        {
            outError = u8"開始・帰還地点のあるエリアがプレビュー開始位置に選ばれていません。";
            return false;
        }
        firstSignature = createSignature(first);
    }
    std::string secondSignature;
    {
        SceneNarakuProto second;
        if (!finishGeneration(second)) return false;
        secondSignature = createSignature(second);
    }
    if (firstSignature != secondSignature)
    {
        outError = u8"段階別エリア数・接続ID・小ステージ配置IDが一致しません。";
        return false;
    }
    return true;
}

void SceneNarakuProto::FocusEditorOverviewOnCurrentArea()
{
    if (m_runtimeMap.terrainLayers.empty()) return;
    float centerX = 0.0f;
    float centerZ = 0.0f;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        centerX += layer.center.x;
        centerZ += layer.center.z;
    }
    const float divisor = static_cast<float>(m_runtimeMap.terrainLayers.size());
    m_player.pos = { centerX / divisor, centerZ / divisor };
    m_player.depth = m_runtimeMap.terrainLayers.front().layerDepth;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_cameraMaxPitchDegrees = 89.0f;
    m_cameraPitch = DirectX::XMConvertToRadians(89.0f);
    m_cameraDistance = std::max(20.0f, std::min(120.0f, m_worldHalfSize * 1.6f));
}

void SceneNarakuProto::SetEditorPreviewOverview(bool enabled)
{
    if (enabled == m_editorOverviewMode) return;
    if (enabled)
    {
        m_editorWalkPlayerState = m_player;
        m_editorWalkCameraPitch = m_cameraPitch;
        m_editorWalkCameraDistance = m_cameraDistance;
        m_editorWalkCameraMaxPitch = m_cameraMaxPitchDegrees;
        m_editorOverviewMode = true;
        FocusEditorOverviewOnCurrentArea();
    }
    else
    {
        m_editorOverviewMode = false;
        m_player = m_editorWalkPlayerState;
        m_cameraPitch = m_editorWalkCameraPitch;
        m_cameraDistance = m_editorWalkCameraDistance;
        m_cameraMaxPitchDegrees = m_editorWalkCameraMaxPitch;
    }
}

void SceneNarakuProto::ReturnEditorPreviewToStart()
{
    SetEditorPreviewOverview(false);
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (!m_areas[static_cast<std::size_t>(areaIndex)].canReturn) continue;
        ActivateArea(areaIndex, false);
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        break;
    }
}
#endif

void SceneNarakuProto::InitializeTerrainFloorBatch()
{
    const char* vertexShaderCode = R"HLSL(
struct VS_IN {
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
struct VS_OUT {
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
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
    output.worldPosition = input.position;
    output.normal = input.normal;
    output.uv = input.uv;
    output.color = input.color;
    return output;
})HLSL";

    const char* pixelShaderCode = R"HLSL(
struct PS_IN {
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
cbuffer Light : register(b0) {
    float4 directionalColor;
    float4 directionalDirection;
    float4 pointColorEnabled;
    float4 pointPositionRange;
    float4 spotColorEnabled;
    float4 spotPositionRange;
    float4 spotDirectionInnerCos;
    float4 spotOuterCos;
};
Texture2D terrainTexture : register(t0);
SamplerState terrainSampler : register(s0);
float4 main(PS_IN input) : SV_TARGET {
    float4 color = terrainTexture.Sample(terrainSampler, input.uv) * input.color;
    float3 normal = length(input.normal) > 0.001f ? normalize(input.normal) : float3(0.0f, 1.0f, 0.0f);
    float3 directionalToLight = normalize(-directionalDirection.xyz);
    float directionalAmount = saturate((dot(normal, directionalToLight) + 0.5f) / 1.5f);
    float3 illumination = directionalColor.rgb * directionalAmount;

    float3 toPoint = pointPositionRange.xyz - input.worldPosition;
    float pointDistance = length(toPoint);
    float pointAttenuation = pow(saturate(1.0f - pointDistance / max(0.001f, pointPositionRange.w)), 2.0f);
    illumination += pointColorEnabled.rgb * pointColorEnabled.w * pointAttenuation *
        saturate((dot(normal, normalize(toPoint)) + 0.5f) / 1.5f);

    float3 fromSpot = input.worldPosition - spotPositionRange.xyz;
    float spotDistance = length(fromSpot);
    float spotCone = smoothstep(spotOuterCos.x, spotDirectionInnerCos.w,
        dot(normalize(fromSpot), normalize(spotDirectionInnerCos.xyz)));
    float spotAttenuation = pow(saturate(1.0f - spotDistance / max(0.001f, spotPositionRange.w)), 2.0f);
    float spotNdotL = saturate((dot(normal, normalize(-fromSpot)) + 0.5f) / 1.5f);
    illumination += spotColorEnabled.rgb * spotColorEnabled.w * spotCone * spotAttenuation * spotNdotL;

    illumination = saturate(illumination);
    color.rgb *= max(illumination, 0.12f);
    return color;
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

    const char* texturePaths[TerrainTextureCount] =
    {
        "Assets/Texture/Tile/grass.png",
        "Assets/Texture/Tile/dirt.png",
        "Assets/Texture/Tile/cobble.png",
        "Assets/Texture/Tile/wetland.png"
    };
    for (std::size_t index = 0; index < TerrainTextureCount; ++index)
    {
        m_terrainFloorTextures[index] = new Texture();
        if (FAILED(m_terrainFloorTextures[index]->Create(texturePaths[index])))
        {
            SAFE_DELETE(m_terrainFloorTextures[index]);
        }
    }
    const unsigned int whitePixel = 0xffffffff;
    m_terrainFloorTextures[TerrainTextureCount] = new Texture();
    if (FAILED(m_terrainFloorTextures[TerrainTextureCount]->Create(
            DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &whitePixel)))
    {
        SAFE_DELETE(m_terrainFloorTextures[TerrainTextureCount]);
    }
}

void SceneNarakuProto::ReleaseTerrainFloorBatch()
{
    for (MeshBuffer*& mesh : m_terrainFloorMeshes) SAFE_DELETE(mesh);
    for (Texture*& texture : m_terrainFloorTextures) SAFE_DELETE(texture);
    SAFE_DELETE(m_terrainFloorPS);
    SAFE_DELETE(m_terrainFloorVS);
    for (std::vector<TerrainFloorVertex>& vertices : m_terrainFloorVertices) vertices.clear();
    m_terrainFloorVertexCounts.fill(0);
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
            static_cast<std::size_t>(layer.gridHeight - 1) * 2u;
    }

    for (MeshBuffer*& mesh : m_terrainFloorMeshes) SAFE_DELETE(mesh);
    for (std::vector<TerrainFloorVertex>& vertices : m_terrainFloorVertices) vertices.clear();
    m_terrainFloorVertexCounts.fill(0);
    if (quadCapacity == 0 || m_terrainFloorVS == nullptr || m_terrainFloorPS == nullptr)
    {
        return;
    }

    for (std::size_t batchIndex = 0; batchIndex < TerrainBatchCount; ++batchIndex)
    {
        std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
        vertices.resize(quadCapacity * 6u);
        MeshBuffer::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(TerrainFloorVertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.isWrite = true;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        m_terrainFloorMeshes[batchIndex] = new MeshBuffer();
        if (FAILED(m_terrainFloorMeshes[batchIndex]->Create(desc)))
        {
            SAFE_DELETE(m_terrainFloorMeshes[batchIndex]);
            vertices.clear();
        }
    }
}

void SceneNarakuProto::AppendTerrainFloorQuad(
    const DirectX::XMFLOAT3& center,
    const DirectX::XMFLOAT2& size,
    const DirectX::XMFLOAT4& color)
{
    const std::size_t batchIndex = TerrainTextureCount;
    unsigned int& vertexCount = m_terrainFloorVertexCounts[batchIndex];
    std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
    if (vertexCount + 6u > vertices.size())
    {
        return;
    }

    const float halfWidth = size.x * 0.5f;
    const float halfDepth = size.y * 0.5f;
    const DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    const TerrainFloorVertex topLeft = { { center.x - halfWidth, center.y, center.z + halfDepth }, up, { 0.0f, 0.0f }, color };
    const TerrainFloorVertex topRight = { { center.x + halfWidth, center.y, center.z + halfDepth }, up, { 1.0f, 0.0f }, color };
    const TerrainFloorVertex bottomLeft = { { center.x - halfWidth, center.y, center.z - halfDepth }, up, { 0.0f, 1.0f }, color };
    const TerrainFloorVertex bottomRight = { { center.x + halfWidth, center.y, center.z - halfDepth }, up, { 1.0f, 1.0f }, color };

    TerrainFloorVertex* destination = vertices.data() + vertexCount;
    destination[0] = topLeft;
    destination[1] = topRight;
    destination[2] = bottomLeft;
    destination[3] = bottomLeft;
    destination[4] = topRight;
    destination[5] = bottomRight;
    vertexCount += 6u;
}

void SceneNarakuProto::AppendTerrainFloorCell(
    const DirectX::XMFLOAT3& topLeft,
    const DirectX::XMFLOAT3& topRight,
    const DirectX::XMFLOAT3& bottomLeft,
    const DirectX::XMFLOAT3& bottomRight,
    const DirectX::XMFLOAT3& topLeftNormal,
    const DirectX::XMFLOAT3& topRightNormal,
    const DirectX::XMFLOAT3& bottomLeftNormal,
    const DirectX::XMFLOAT3& bottomRightNormal,
    const DirectX::XMFLOAT4& color,
    int textureId)
{
    const std::size_t batchIndex = static_cast<std::size_t>(std::max(0, std::min(textureId, 3)));
    unsigned int& vertexCount = m_terrainFloorVertexCounts[batchIndex];
    std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
    if (vertexCount + 6u > vertices.size()) return;

    const TerrainFloorVertex a = { topLeft, topLeftNormal, { 0.0f, 0.0f }, color };
    const TerrainFloorVertex b = { topRight, topRightNormal, { 1.0f, 0.0f }, color };
    const TerrainFloorVertex c = { bottomLeft, bottomLeftNormal, { 0.0f, 1.0f }, color };
    const TerrainFloorVertex d = { bottomRight, bottomRightNormal, { 1.0f, 1.0f }, color };
    TerrainFloorVertex* destination = vertices.data() + vertexCount;
    destination[0] = a;
    destination[1] = b;
    destination[2] = c;
    destination[3] = c;
    destination[4] = b;
    destination[5] = d;
    vertexCount += 6u;
}

void SceneNarakuProto::AppendTerrainFloorOverlayCell(
    const DirectX::XMFLOAT3& topLeft,
    const DirectX::XMFLOAT3& topRight,
    const DirectX::XMFLOAT3& bottomLeft,
    const DirectX::XMFLOAT3& bottomRight,
    const DirectX::XMFLOAT4& color)
{
    const std::size_t batchIndex = TerrainTextureCount;
    unsigned int& vertexCount = m_terrainFloorVertexCounts[batchIndex];
    std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
    if (vertexCount + 6u > vertices.size()) return;

    const DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    const TerrainFloorVertex a = { topLeft, up, { 0.0f, 0.0f }, color };
    const TerrainFloorVertex b = { topRight, up, { 1.0f, 0.0f }, color };
    const TerrainFloorVertex c = { bottomLeft, up, { 0.0f, 1.0f }, color };
    const TerrainFloorVertex d = { bottomRight, up, { 1.0f, 1.0f }, color };
    TerrainFloorVertex* destination = vertices.data() + vertexCount;
    destination[0] = a;
    destination[1] = b;
    destination[2] = c;
    destination[3] = c;
    destination[4] = b;
    destination[5] = d;
    vertexCount += 6u;
}

void SceneNarakuProto::DrawTerrainFloorBatch(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection)
{
    if (m_terrainFloorVS == nullptr || m_terrainFloorPS == nullptr)
    {
        return;
    }

    DirectX::XMFLOAT4X4 matrices[2] = { view, projection };
    m_terrainFloorVS->WriteBuffer(0, matrices);
    m_terrainFloorVS->Bind();
    ShaderList::ExtendedLight light = BuildSceneLight();
    m_terrainFloorPS->WriteBuffer(0, &light);
    m_terrainFloorPS->Bind();
    for (std::size_t batchIndex = 0; batchIndex < TerrainBatchCount; ++batchIndex)
    {
        MeshBuffer* mesh = m_terrainFloorMeshes[batchIndex];
        const unsigned int vertexCount = m_terrainFloorVertexCounts[batchIndex];
        if (mesh == nullptr || vertexCount == 0 || m_terrainFloorTextures[batchIndex] == nullptr) continue;
        SetBlendMode(batchIndex < TerrainTextureCount ? BLEND_NONE : BLEND_ALPHA);
        m_terrainFloorPS->SetTexture(0, m_terrainFloorTextures[batchIndex]);
        mesh->Write(m_terrainFloorVertices[batchIndex].data());
        mesh->Draw(static_cast<int>(vertexCount));
    }
}

void SceneNarakuProto::InitializeEnemyBillboardBatch()
{
    const char* vertexShaderCode = R"HLSL(
struct VS_IN {
    float3 position : POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
struct VS_OUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
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
    output.uv = input.uv;
    output.color = input.color;
    return output;
})HLSL";

    const char* pixelShaderCode = R"HLSL(
struct PS_IN {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
Texture2D enemyTexture : register(t0);
SamplerState enemySampler : register(s0);
float4 main(PS_IN input) : SV_TARGET {
    float4 color = enemyTexture.Sample(enemySampler, input.uv) * input.color;
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

    const char* texturePaths[] =
    {
        "Assets/Enemy/skeleton.png",
        "Assets/Enemy/monster.png",
    };
    for (std::size_t index = 0; index < m_enemyTextures.size(); ++index)
    {
        m_enemyTextures[index] = new Texture();
        if (FAILED(m_enemyTextures[index]->Create(texturePaths[index])))
        {
            SAFE_DELETE(m_enemyTextures[index]);
        }
    }
}

void SceneNarakuProto::ReleaseEnemyBillboardBatch()
{
    SAFE_DELETE(m_enemyBillboardMesh);
    SAFE_DELETE(m_enemyBillboardPS);
    SAFE_DELETE(m_enemyBillboardVS);
    for (Texture*& texture : m_enemyTextures)
    {
        SAFE_DELETE(texture);
    }
    m_enemyBillboardVertices.clear();
    m_enemyBillboardVertexCount = 0;
}

void SceneNarakuProto::RebuildEnemyBillboardBatch()
{
    SAFE_DELETE(m_enemyBillboardMesh);
    m_enemyBillboardVertices.clear();
    m_enemyBillboardVertexCount = 0;
    if (m_enemies.empty() || m_enemyBillboardVS == nullptr ||
        m_enemyBillboardPS == nullptr ||
        (m_enemyTextures[0] == nullptr && m_enemyTextures[1] == nullptr))
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
    const DirectX::XMFLOAT4X4& projection,
    EnemyType enemyType,
    Texture* texture)
{
    using namespace DirectX;

    if (m_enemyBillboardMesh == nullptr || m_enemyBillboardVS == nullptr ||
        m_enemyBillboardPS == nullptr || texture == nullptr)
    {
        return;
    }

    constexpr float billboardWidth = 2.4f;
    constexpr float billboardHeight = 2.4f;
    const XMMATRIX viewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&view));
    XMMATRIX billboard = XMMatrixInverse(nullptr, viewMatrix);
    billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    const Vec2 cameraForward = GetCameraForward();
    const Vec2 cameraRight = GetCameraRight();

    m_enemyBillboardVertexCount = 0;
    for (const EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive || enemy.type != enemyType ||
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

        const bool attacking = enemy.telegraphTimer > 0.0f || enemy.chargeTimer > 0.0f;
        const float animationFps = attacking ? kEnemyAttackAnimationFps : kEnemyMoveAnimationFps;
        const CharacterFrameSequence sequence = GetCharacterFrameSequence(
            Dot(enemy.facing, cameraRight),
            Dot(enemy.facing, cameraForward),
            enemy.moving || attacking);
        const int sequenceFrame =
            static_cast<int>(m_characterAnimationTime * animationFps) % sequence.count;
        const int frame = sequence.frames[sequenceFrame];
        const int frameX = frame % kCharacterSpriteColumns;
        const int frameY = frame / kCharacterSpriteColumns;
        const float uvLeft = static_cast<float>(
            sequence.mirror ? frameX + 1 : frameX) / static_cast<float>(kCharacterSpriteColumns);
        const float uvRight = static_cast<float>(
            sequence.mirror ? frameX : frameX + 1) / static_cast<float>(kCharacterSpriteColumns);
        const float uvTop = static_cast<float>(frameY) / static_cast<float>(kCharacterSpriteRows);
        const float uvBottom = static_cast<float>(frameY + 1) / static_cast<float>(kCharacterSpriteRows);
        const XMFLOAT3 sceneLight = CalculateSceneLightColor(enemy.pos, enemy.depth, center.y);
        const XMFLOAT4 lightColor = { sceneLight.x, sceneLight.y, sceneLight.z, 1.0f };

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
        destination[0] = { topLeft, { uvLeft, uvTop }, lightColor };
        destination[1] = { topRight, { uvRight, uvTop }, lightColor };
        destination[2] = { bottomLeft, { uvLeft, uvBottom }, lightColor };
        destination[3] = { bottomLeft, { uvLeft, uvBottom }, lightColor };
        destination[4] = { topRight, { uvRight, uvTop }, lightColor };
        destination[5] = { bottomRight, { uvRight, uvBottom }, lightColor };
        m_enemyBillboardVertexCount += 6u;
    }

    if (m_enemyBillboardVertexCount == 0)
    {
        return;
    }

    XMFLOAT4X4 matrices[2] = { view, projection };
    m_enemyBillboardVS->WriteBuffer(0, matrices);
    m_enemyBillboardVS->Bind();
    m_enemyBillboardPS->SetTexture(0, texture);
    m_enemyBillboardPS->Bind();
    m_enemyBillboardMesh->Write(m_enemyBillboardVertices.data());

    SetCullingMode(D3D11_CULL_NONE);
    SetBlendMode(BLEND_ALPHA);
    SetSamplerState(SAMPLER_POINT);
    m_enemyBillboardMesh->Draw(static_cast<int>(m_enemyBillboardVertexCount));
    SetSamplerState(SAMPLER_LINEAR);
}
