#include "SceneNarakuPieceEditor.h"
#include "EditorPerformanceProfiler.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "MeshBuffer.h"
#include "Model.h"
#include "NarakuMapData.h"
#include "Shader.h"
#include "ShaderList.h"
#include "Texture.h"
#include "imgui.h"
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <ctime>
#include <cstdint>
#include <cwchar>
#include <cwctype>
#include <fstream>
#include <iomanip>
#include <sstream>

using namespace DirectX;
#include "NarakuPieceEditorInternal.h"

bool SceneNarakuPieceEditor::EnsurePreviewRenderTarget(unsigned int width, unsigned int height)
{
    EDITOR_PROFILE_FUNCTION();
    const unsigned int safeWidth = (width > 0U) ? width : 1U;
    const unsigned int safeHeight = (height > 0U) ? height : 1U;
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_previewRenderTarget != nullptr &&
        m_previewDepthStencil != nullptr &&
        m_previewRenderWidth == safeWidth &&
        m_previewRenderHeight == safeHeight)
    {
        return true;
    }

    ReleasePreviewRenderTarget();

    m_previewRenderTarget = new RenderTarget();
    // 条件に該当する場合は、`ReleasePreviewRenderTarget` の処理を実行します。
    if (FAILED(m_previewRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, safeWidth, safeHeight)))
    {
        ReleasePreviewRenderTarget();
        return false;
    }

    m_previewDepthStencil = new DepthStencil();
    // 条件に該当する場合は、`ReleasePreviewRenderTarget` の処理を実行します。
    if (FAILED(m_previewDepthStencil->Create(safeWidth, safeHeight, false)))
    {
        ReleasePreviewRenderTarget();
        return false;
    }

    m_previewRenderWidth = safeWidth;
    m_previewRenderHeight = safeHeight;
    return true;
}

void SceneNarakuPieceEditor::ReleasePreviewRenderTarget()
{
    EDITOR_PROFILE_FUNCTION();
    SAFE_DELETE(m_previewDepthStencil);
    SAFE_DELETE(m_previewRenderTarget);
    m_previewRenderWidth = 0;
    m_previewRenderHeight = 0;
}

void SceneNarakuPieceEditor::LoadBasePreviewModels()
{
    EDITOR_PROFILE_FUNCTION();
    ReleaseBasePreviewModels();
    const char* modelPaths[] =
    {
        "Assets/Base/second_base.fbx",
        "Assets/Base/frontline_base.fbx",
    };

    for (std::size_t index = 0; index < m_basePreviewModels.size(); ++index)
    {
        const std::string resolvedPath = WideToUtf8(ResolvePieceHierarchyPath(Utf8ToWide(modelPaths[index])));
        Model* model = new Model();
        if (!model->LoadStatic(resolvedPath.c_str(), 1.0f, Model::ZFlip))
        {
            SAFE_DELETE(model);
            continue;
        }

        XMFLOAT3 minimum = { FLT_MAX, FLT_MAX, FLT_MAX };
        XMFLOAT3 maximum = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        bool hasVertex = false;
        for (unsigned int meshIndex = 0; meshIndex < model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            for (const Model::Vertex& vertex : mesh->vertices)
            {
                minimum.x = std::min(minimum.x, vertex.pos.x);
                minimum.y = std::min(minimum.y, vertex.pos.y);
                minimum.z = std::min(minimum.z, vertex.pos.z);
                maximum.x = std::max(maximum.x, vertex.pos.x);
                maximum.y = std::max(maximum.y, vertex.pos.y);
                maximum.z = std::max(maximum.z, vertex.pos.z);
                hasVertex = true;
            }
        }
        if (!hasVertex)
        {
            SAFE_DELETE(model);
            continue;
        }

        BasePreviewModelResource& resource = m_basePreviewModels[index];
        resource.model = model;
        resource.placementAnchor = {
            (minimum.x + maximum.x) * 0.5f,
            minimum.y,
            (minimum.z + maximum.z) * 0.5f };
        resource.horizontalSize = std::max(
            0.001f,
            std::max(maximum.x - minimum.x, maximum.z - minimum.z));
    }
}

void SceneNarakuPieceEditor::ReleaseBasePreviewModels()
{
    EDITOR_PROFILE_FUNCTION();
    for (BasePreviewModelResource& resource : m_basePreviewModels)
    {
        SAFE_DELETE(resource.model);
        resource.placementAnchor = {};
        resource.horizontalSize = 1.0f;
    }
}

void SceneNarakuPieceEditor::ReleaseSurfaceFacilityPreviewModel() const
{
    EDITOR_PROFILE_FUNCTION();
    SAFE_DELETE(m_surfaceFacilityPreviewModel);
    m_surfaceFacilityPreviewPath.clear();
    m_surfaceFacilityPreviewAnchor = {};
    m_surfaceFacilityPreviewWriteTime = 0;
}

void SceneNarakuPieceEditor::InitializeWaterOverlayPreview()
{
    EDITOR_PROFILE_FUNCTION();
    ReleaseWaterOverlayPreview();
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
Texture2D overlayTexture : register(t0);
SamplerState overlaySampler : register(s0);
float4 main(PS_IN input) : SV_TARGET {
    return overlayTexture.Sample(overlaySampler, input.uv) * input.color;
})HLSL";

    m_waterOverlayVS = new VertexShader();
    if (FAILED(m_waterOverlayVS->Compile(vertexShaderCode))) SAFE_DELETE(m_waterOverlayVS);
    m_waterOverlayPS = new PixelShader();
    if (FAILED(m_waterOverlayPS->Compile(pixelShaderCode))) SAFE_DELETE(m_waterOverlayPS);

    const unsigned int whitePixel = 0xffffffff;
    m_waterOverlayTexture = new Texture();
    if (FAILED(m_waterOverlayTexture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &whitePixel)))
        SAFE_DELETE(m_waterOverlayTexture);

    const char* groundTexturePaths[GroundTextureCount] =
    {
        "Assets/Texture/Tile/grass.png",
        "Assets/Texture/Tile/dirt.png",
        "Assets/Texture/Tile/cobble.png",
        "Assets/Texture/Tile/wetland.png"
    };
    for (std::size_t index = 0; index < GroundTextureCount; ++index)
    {
        m_groundTextures[index] = new Texture();
        if (FAILED(m_groundTextures[index]->Create(groundTexturePaths[index])))
            SAFE_DELETE(m_groundTextures[index]);
    }

    MeshBuffer::Description description = {};
    description.pVtx = m_waterOverlayVertices.data();
    description.vtxSize = sizeof(WaterOverlayVertex);
    description.vtxCount = static_cast<UINT>(m_waterOverlayVertices.size());
    description.isWrite = true;
    description.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    m_waterOverlayMesh = new MeshBuffer();
    if (FAILED(m_waterOverlayMesh->Create(description))) SAFE_DELETE(m_waterOverlayMesh);
}

void SceneNarakuPieceEditor::ReleaseWaterOverlayPreview()
{
    EDITOR_PROFILE_FUNCTION();
    SAFE_DELETE(m_waterOverlayMesh);
    SAFE_DELETE(m_waterOverlayTexture);
    for (Texture*& texture : m_groundTextures) SAFE_DELETE(texture);
    SAFE_DELETE(m_waterOverlayPS);
    SAFE_DELETE(m_waterOverlayVS);
}

void SceneNarakuPieceEditor::DrawGroundTextures3D() const
{
    EDITOR_PROFILE_FUNCTION();
    if (!m_showGroundTextures || m_waterOverlayMesh == nullptr ||
        m_waterOverlayVS == nullptr || m_waterOverlayPS == nullptr) return;

    XMFLOAT4X4 matrices[2] = {};
    XMStoreFloat4x4(&matrices[0], XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&matrices[1], XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
    m_waterOverlayVS->WriteBuffer(0, matrices);
    m_waterOverlayVS->Bind();
    SetCullingMode(D3D11_CULL_NONE);
    SetBlendMode(BLEND_NONE);

    for (std::size_t textureIndex = 0; textureIndex < GroundTextureCount; ++textureIndex)
    {
        Texture* const texture = m_groundTextures[textureIndex];
        if (texture == nullptr) continue;

        unsigned int vertexCount = 0;
        for (int cellZ = 0; cellZ < m_piece.gridDepth - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < m_piece.gridWidth - 1; ++cellX)
            {
                const NarakuPiece::CellData* const cell = GetCellData(cellX, cellZ);
                if (cell == nullptr || cell->deleted ||
                    cell->groundTextureId != static_cast<int>(textureIndex)) continue;
                if (vertexCount + 6u > m_waterOverlayVertices.size()) continue;

                const auto lower = [](XMFLOAT3 position)
                {
                    position.y -= 0.01f;
                    return position;
                };
                const XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
                const WaterOverlayVertex a = { lower(GetVertexWorldPosition(cellX, cellZ)), { 0.0f, 0.0f }, color };
                const WaterOverlayVertex b = { lower(GetVertexWorldPosition(cellX + 1, cellZ)), { 1.0f, 0.0f }, color };
                const WaterOverlayVertex c = { lower(GetVertexWorldPosition(cellX, cellZ + 1)), { 0.0f, 1.0f }, color };
                const WaterOverlayVertex d = { lower(GetVertexWorldPosition(cellX + 1, cellZ + 1)), { 1.0f, 1.0f }, color };
                WaterOverlayVertex* const destination = m_waterOverlayVertices.data() + vertexCount;
                destination[0] = a;
                destination[1] = b;
                destination[2] = c;
                destination[3] = c;
                destination[4] = b;
                destination[5] = d;
                vertexCount += 6u;
            }
        }
        if (vertexCount == 0u) continue;

        m_waterOverlayPS->SetTexture(0, texture);
        m_waterOverlayPS->Bind();
        m_waterOverlayMesh->Write(m_waterOverlayVertices.data());
        m_waterOverlayMesh->Draw(static_cast<int>(vertexCount));
    }

    SetCullingMode(D3D11_CULL_BACK);
}

void SceneNarakuPieceEditor::DrawWaterOverlays3D() const
{
    EDITOR_PROFILE_FUNCTION();
    if (m_waterOverlayMesh == nullptr || m_waterOverlayVS == nullptr ||
        m_waterOverlayPS == nullptr || m_waterOverlayTexture == nullptr) return;

    unsigned int vertexCount = 0;
    for (int cellZ = 0; cellZ < m_piece.gridDepth - 1; ++cellZ)
    {
        for (int cellX = 0; cellX < m_piece.gridWidth - 1; ++cellX)
        {
            const NarakuPiece::CellData* cell = GetCellData(cellX, cellZ);
            if (cell == nullptr || cell->deleted || cell->waterDepth == NarakuPiece::WaterDepth::None) continue;
            if (vertexCount + 6u > m_waterOverlayVertices.size()) continue;

            const XMFLOAT4 color = cell->waterDepth == NarakuPiece::WaterDepth::Puddle
                ? XMFLOAT4{ 0.22f, 0.72f, 0.96f, 0.44f }
                : (cell->waterDepth == NarakuPiece::WaterDepth::Pond
                    ? XMFLOAT4{ 0.12f, 0.58f, 0.90f, 0.54f }
                    : XMFLOAT4{ 0.06f, 0.40f, 0.78f, 0.64f });
            auto raised = [](XMFLOAT3 position)
            {
                position.y += kCellOverlayYOffset * 0.5f;
                return position;
            };
            const WaterOverlayVertex a = { raised(GetVertexWorldPosition(cellX, cellZ)), { 0.0f, 0.0f }, color };
            const WaterOverlayVertex b = { raised(GetVertexWorldPosition(cellX + 1, cellZ)), { 1.0f, 0.0f }, color };
            const WaterOverlayVertex c = { raised(GetVertexWorldPosition(cellX, cellZ + 1)), { 0.0f, 1.0f }, color };
            const WaterOverlayVertex d = { raised(GetVertexWorldPosition(cellX + 1, cellZ + 1)), { 1.0f, 1.0f }, color };
            WaterOverlayVertex* destination = m_waterOverlayVertices.data() + vertexCount;
            destination[0] = a;
            destination[1] = b;
            destination[2] = c;
            destination[3] = c;
            destination[4] = b;
            destination[5] = d;
            vertexCount += 6u;
        }
    }
    if (vertexCount == 0u) return;

    XMFLOAT4X4 matrices[2] = {};
    XMStoreFloat4x4(&matrices[0], XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&matrices[1], XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
    m_waterOverlayVS->WriteBuffer(0, matrices);
    m_waterOverlayVS->Bind();
    m_waterOverlayPS->SetTexture(0, m_waterOverlayTexture);
    m_waterOverlayPS->Bind();
    SetCullingMode(D3D11_CULL_NONE);
    SetBlendMode(BLEND_ALPHA);
    m_waterOverlayMesh->Write(m_waterOverlayVertices.data());
    m_waterOverlayMesh->Draw(static_cast<int>(vertexCount));
    SetBlendMode(BLEND_NONE);
    SetCullingMode(D3D11_CULL_BACK);
}

void SceneNarakuPieceEditor::DrawBasePreview3D() const
{
    EDITOR_PROFILE_FUNCTION();
    if (m_piece.baseType == NarakuPiece::BaseType::None) return;
    const std::size_t resourceIndex = m_piece.baseType == NarakuPiece::BaseType::SecondBase ? 0u : 1u;
    const BasePreviewModelResource& resource = m_basePreviewModels[resourceIndex];
    if (resource.model == nullptr) return;

    const int footprint = m_piece.baseType == NarakuPiece::BaseType::SecondBase ? 2 : 3;
    const XMFLOAT3 center = GetCellWorldPosition((m_piece.gridWidth - 2) / 2, (m_piece.gridDepth - 2) / 2);
    const float modelScale = static_cast<float>(footprint) * m_piece.cellSize / resource.horizontalSize;
    const float cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 eye = {
        m_cameraTarget.x + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance };

    XMFLOAT4X4 wvp[3] = {};
    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
        XMMatrixTranslation(-resource.placementAnchor.x, -resource.placementAnchor.y, -resource.placementAnchor.z) *
        XMMatrixScaling(modelScale, modelScale, modelScale) *
        XMMatrixTranslation(
            center.x - m_piece.baseModelOffsetX,
            center.y + m_piece.baseModelOffsetY,
            center.z + m_piece.baseModelOffsetZ)));
    XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    ShaderList::SetLight({ 1.0f, 1.0f, 1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f });
    SetBlendMode(BLEND_NONE);
    resource.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    resource.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int meshIndex = 0; meshIndex < resource.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = resource.model->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        const Model::Material* sourceMaterial = resource.model->GetMaterial(mesh->materialID);
        if (sourceMaterial != nullptr)
        {
            Model::Material material = *sourceMaterial;
            ShaderList::SetMaterial(material);
        }
        resource.model->Draw(static_cast<int>(meshIndex));
    }
}

void SceneNarakuPieceEditor::DrawPreviewWindow()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`m_previewImageHovered` の状態を更新します。
    if (!m_showPreviewWindow)
    {
        m_previewImageHovered = false;
        m_previewImageTopLeft = {};
        m_previewImageScreenTopLeft = {};
        m_previewImageSize = {};
        return;
    }

    m_previewImageHovered = false;
    EDITOR_PROFILE_WINDOW(u8"3Dプレビュー");
    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    const ImVec2 workSize = (viewport != nullptr) ? viewport->WorkSize : ImVec2(1280.0f, 720.0f);
    ImGui::SetNextWindowPos(ImVec2(392.0f, 16.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(
        ImVec2(std::max(520.0f, workSize.x - 408.0f), std::max(360.0f, workSize.y * 0.62f)),
        ImGuiCond_FirstUseEver);
    // 条件に該当する場合は、`m_previewImageTopLeft` の状態を更新します。
    if (!ImGui::Begin(u8"3Dプレビュー", &m_showPreviewWindow))
    {
        m_previewImageTopLeft = {};
        m_previewImageScreenTopLeft = {};
        m_previewImageSize = {};
        ImGui::End();
        return;
    }

    ImVec2 area = ImGui::GetContentRegionAvail();
    area.x = std::max(area.x, 320.0f);
    area.y = std::max(area.y, 220.0f);

    m_previewRequestWidth = static_cast<unsigned int>(std::max(1.0f, area.x));
    m_previewRequestHeight = static_cast<unsigned int>(std::max(1.0f, area.y));

    const ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
    // 条件に該当する場合は、`ImGui::Image` の処理を実行します。
    if (m_previewRenderTarget != nullptr)
    {
        ImGui::Image(reinterpret_cast<ImTextureID>(m_previewRenderTarget->GetResource()), area);
        m_previewImageHovered = ImGui::IsItemHovered();
    }
    else
    {
        ImGui::InvisibleButton("##NarakuPiecePreviewPlaceholder", area);
        m_previewImageHovered = ImGui::IsItemHovered();
        ImDrawList* const drawList = ImGui::GetWindowDrawList();
        const ImVec2 rectMin = imageTopLeft;
        const ImVec2 rectMax(imageTopLeft.x + area.x, imageTopLeft.y + area.y);
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(10, 12, 14, 255));
        drawList->AddRect(rectMin, rectMax, IM_COL32(90, 98, 110, 255), 0.0f, 0, 1.5f);
    }

    const ImVec2 itemMin = ImGui::GetItemRectMin();
    const ImVec2 itemMax = ImGui::GetItemRectMax();
    m_previewImageScreenTopLeft = { itemMin.x, itemMin.y };
    m_previewImageTopLeft = ConvertImGuiScreenToClient(itemMin);
    m_previewImageSize =
    {
        std::max(0.0f, itemMax.x - itemMin.x),
        std::max(0.0f, itemMax.y - itemMin.y)
    };

    DrawSelectionRectangle();
    DrawPreviewCompass();
    DrawSlopeInfoOverlay();
    ImGui::End();
}

void SceneNarakuPieceEditor::DrawSlopeInfoOverlay() const
{
    EDITOR_PROFILE_FUNCTION();
    if (m_previewImageSize.x < 1.0f || m_previewImageSize.y < 1.0f) return;

    int cellX = m_hoverCellX;
    int cellZ = m_hoverCellZ;
    if (!IsValidCell(cellX, cellZ))
    {
        cellX = m_selectedCellX;
        cellZ = m_selectedCellZ;
    }

    std::string detail = u8"対象セル: 未選択";
    ImU32 detailColor = IM_COL32(225, 230, 235, 255);
    if (IsValidCell(cellX, cellZ))
    {
        const XMFLOAT3 p00 = GetVertexWorldPosition(cellX, cellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(cellX + 1, cellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(cellX, cellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(cellX + 1, cellZ + 1);
        const float slope = NarakuMap::CalculateMaximumSlopeDegrees(
            p00.y, p10.y, p01.y, p11.y, m_piece.cellSize);
        const NarakuPiece::CellData* const cell = GetCellData(cellX, cellZ);
        const bool slopeWalkable = slope <= NarakuMap::MaximumWalkableSlopeDegrees;
        const bool walkable = cell != nullptr && !cell->deleted && cell->walkable &&
            cell->waterDepth != NarakuPiece::WaterDepth::Lake && slopeWalkable;
        const char* status = walkable ? u8"登坂可" :
            (cell == nullptr || cell->deleted ? u8"削除セル" :
                (!cell->walkable ? u8"手動歩行禁止" :
                    (cell->waterDepth == NarakuPiece::WaterDepth::Lake ? u8"水域歩行禁止" : u8"登坂不可")));
        char buffer[192] = {};
        std::snprintf(buffer, sizeof(buffer),
            u8"セル(%d, %d)  傾斜 %.1f°  %s",
            cellX, cellZ, slope, status);
        detail = buffer;
        detailColor = walkable ? IM_COL32(130, 240, 170, 255) : IM_COL32(255, 80, 145, 255);
    }

    char limitBuffer[128] = {};
    std::snprintf(limitBuffer, sizeof(limitBuffer),
        u8"プレイヤー登坂上限: %.1f°",
        NarakuMap::MaximumWalkableSlopeDegrees);
    const ImVec2 imageMin(m_previewImageScreenTopLeft.x, m_previewImageScreenTopLeft.y);
    const ImVec2 firstTextPosition(imageMin.x + 12.0f, imageMin.y + 12.0f);
    const ImVec2 secondTextPosition(firstTextPosition.x, firstTextPosition.y + ImGui::GetTextLineHeightWithSpacing());
    const ImVec2 firstSize = ImGui::CalcTextSize(limitBuffer);
    const ImVec2 secondSize = ImGui::CalcTextSize(detail.c_str());
    const ImVec2 boxMax(
        firstTextPosition.x + std::max(firstSize.x, secondSize.x) + 10.0f,
        secondTextPosition.y + secondSize.y + 6.0f);
    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(
        ImVec2(firstTextPosition.x - 6.0f, firstTextPosition.y - 5.0f),
        boxMax,
        IM_COL32(10, 14, 18, 205), 4.0f);
    drawList->AddText(firstTextPosition, IM_COL32(235, 240, 245, 255), limitBuffer);
    drawList->AddText(secondTextPosition, detailColor, detail.c_str());
}

DirectX::XMFLOAT2 SceneNarakuPieceEditor::GetCompassScreenDirection(const XMFLOAT3& worldDirection) const
{
    EDITOR_PROFILE_FUNCTION();
    const XMVECTOR viewDirection = XMVector3TransformNormal(
        XMVectorSet(worldDirection.x, worldDirection.y, worldDirection.z, 0.0f),
        XMLoadFloat4x4(&m_viewMatrix));

    XMFLOAT3 viewSpaceDirection = {};
    XMStoreFloat3(&viewSpaceDirection, viewDirection);
    const float length = std::sqrt(
        viewSpaceDirection.x * viewSpaceDirection.x +
        viewSpaceDirection.y * viewSpaceDirection.y);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (length <= 0.0001f)
    {
        return {};
    }

    return
    {
        viewSpaceDirection.x / length,
        -viewSpaceDirection.y / length
    };
}

void SceneNarakuPieceEditor::DrawPreviewCompass() const
{
    EDITOR_PROFILE_FUNCTION();
    const float minimumCompassSize =
        kCompassRadius * 2.0f + kCompassMargin * 2.0f + kCompassLabelDistance * 2.0f;
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_previewImageSize.x < minimumCompassSize ||
        m_previewImageSize.y < minimumCompassSize)
    {
        return;
    }

    const ImVec2 imageMin(m_previewImageScreenTopLeft.x, m_previewImageScreenTopLeft.y);
    const ImVec2 imageMax(
        imageMin.x + m_previewImageSize.x,
        imageMin.y + m_previewImageSize.y);
    const ImVec2 center(
        imageMax.x - kCompassMargin - kCompassRadius,
        imageMin.y + kCompassMargin + kCompassRadius);
    ImDrawList* const drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(imageMin, imageMax, true);

    drawList->AddCircle(center, kCompassRadius, IM_COL32(220, 230, 240, 220), 32, kCompassLineThickness);
    drawList->AddCircleFilled(center, 2.5f, IM_COL32(220, 230, 240, 230));

    const XMFLOAT3 worldDirections[] =
    {
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, 1.0f },
        { -1.0f, 0.0f, 0.0f },
        { 1.0f, 0.0f, 0.0f },
    };

    // 対象コレクションの各要素を順に処理します。
    for (int index = 0; index < static_cast<int>(std::size(worldDirections)); ++index)
    {
        const XMFLOAT2 direction = GetCompassScreenDirection(worldDirections[index]);
        // 条件に該当する場合は、その要素を処理対象から除外します。
        if (direction.x == 0.0f && direction.y == 0.0f)
        {
            continue;
        }

        const ImVec2 endpoint(
            center.x + direction.x * (kCompassRadius - kCompassLinePadding),
            center.y + direction.y * (kCompassRadius - kCompassLinePadding));
        const ImU32 lineColor = (index == 0) ? IM_COL32(245, 95, 95, 230) : IM_COL32(220, 230, 240, 220);
        drawList->AddLine(center, endpoint, lineColor, kCompassLineThickness);

        const ImVec2 textSize = ImGui::CalcTextSize(kDirectionLabels[index]);
        const ImVec2 labelCenter(
            center.x + direction.x * (kCompassRadius + kCompassLabelDistance),
            center.y + direction.y * (kCompassRadius + kCompassLabelDistance));
        ImVec2 textPosition(
            labelCenter.x - textSize.x * 0.5f,
            labelCenter.y - textSize.y * 0.5f);
        const float maxTextX = std::max(imageMin.x, imageMax.x - textSize.x);
        const float maxTextY = std::max(imageMin.y, imageMax.y - textSize.y);
        textPosition.x = ClampFloat(textPosition.x, imageMin.x, maxTextX);
        textPosition.y = ClampFloat(textPosition.y, imageMin.y, maxTextY);
        drawList->AddText(textPosition, lineColor, kDirectionLabels[index]);
    }

    drawList->PopClipRect();
}

void SceneNarakuPieceEditor::RenderTerrainPreviewToTexture()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!EnsurePreviewRenderTarget(m_previewRequestWidth, m_previewRequestHeight))
    {
        return;
    }

    RenderTarget* previewTarget[] = { m_previewRenderTarget };
    SetRenderTargets(1, previewTarget, m_previewDepthStencil);

    const float clearColor[] = { 0.02f, 0.03f, 0.04f, 1.0f };
    m_previewRenderTarget->Clear(clearColor);
    m_previewDepthStencil->Clear();
    DrawTerrainPreview3D();

    RenderTarget* defaultTarget[] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
}

XMFLOAT2 SceneNarakuPieceEditor::GetPreviewViewportSize() const
{
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (m_previewImageSize.x >= 1.0f && m_previewImageSize.y >= 1.0f)
    {
        return m_previewImageSize;
    }

    return GetEditorViewportSize();
}

XMFLOAT2 SceneNarakuPieceEditor::ConvertImGuiScreenToClient(const ImVec2& screenPos) const
{
    EDITOR_PROFILE_FUNCTION();
    POINT client =
    {
        static_cast<LONG>(screenPos.x),
        static_cast<LONG>(screenPos.y)
    };

    HWND window = GetPreviewHostWindow();
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (window != nullptr)
    {
        ::ScreenToClient(window, &client);
        return { static_cast<float>(client.x), static_cast<float>(client.y) };
    }

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (viewport != nullptr)
    {
        return
        {
            screenPos.x - viewport->Pos.x,
            screenPos.y - viewport->Pos.y
        };
    }

    return { static_cast<float>(client.x), static_cast<float>(client.y) };
}

XMFLOAT2 SceneNarakuPieceEditor::ConvertClientToImGuiScreen(const POINT& clientPos) const
{
    EDITOR_PROFILE_FUNCTION();
    POINT screen = clientPos;
    HWND window = GetPreviewHostWindow();
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (window != nullptr)
    {
        ::ClientToScreen(window, &screen);
        return { static_cast<float>(screen.x), static_cast<float>(screen.y) };
    }

    const ImGuiViewport* const viewport = ImGui::GetMainViewport();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (viewport != nullptr)
    {
        return
        {
            static_cast<float>(clientPos.x) + viewport->Pos.x,
            static_cast<float>(clientPos.y) + viewport->Pos.y
        };
    }

    return { static_cast<float>(clientPos.x), static_cast<float>(clientPos.y) };
}

bool SceneNarakuPieceEditor::IsMouseInsidePreviewImage() const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (!m_showPreviewWindow || m_previewImageSize.x < 1.0f || m_previewImageSize.y < 1.0f)
    {
        return false;
    }

    const POINT mousePos = GetMousePosition();
    const float mouseX = static_cast<float>(mousePos.x);
    const float mouseY = static_cast<float>(mousePos.y);
    const float minX = m_previewImageTopLeft.x;
    const float minY = m_previewImageTopLeft.y;
    const float maxX = minX + m_previewImageSize.x;
    const float maxY = minY + m_previewImageSize.y;
    return mouseX >= minX && mouseX <= maxX && mouseY >= minY && mouseY <= maxY;
}


void SceneNarakuPieceEditor::DrawTerrainPreview3D() const
{
    EDITOR_PROFILE_FUNCTION();
    XMFLOAT4X4 world = {};
    XMFLOAT4X4 view = {};
    XMFLOAT4X4 projection = {};

    XMStoreFloat4x4(&world, XMMatrixTranspose(XMMatrixIdentity()));
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);

    DrawGroundTextures3D();

    const XMFLOAT4 axisColor = { 0.25f, 0.25f, 0.28f, 1.0f };
    const XMFLOAT4 selectedColor = { 0.95f, 0.70f, 0.20f, 1.0f };
    const XMFLOAT4 multiSelectedColor = { 0.30f, 0.55f, 0.95f, 1.0f };
    const XMFLOAT4 raisedColor = { 0.30f, 0.75f, 0.55f, 1.0f };
    const XMFLOAT4 flatColor = { 0.65f, 0.65f, 0.68f, 1.0f };
    const XMFLOAT4 cellHoverColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT4 cellSelectedColor = { 1.0f, 0.85f, 0.20f, 1.0f };
    const XMFLOAT4 cellDeletedColor = { 0.85f, 0.40f, 0.40f, 1.0f };
    const XMFLOAT4 cellBlockedColor = { 0.95f, 0.15f, 0.15f, 1.0f };
    const XMFLOAT4 steepSlopeColor = { 1.0f, 0.20f, 0.55f, 1.0f };
    const XMFLOAT4 puddleColor = { 0.20f, 0.72f, 0.95f, 1.0f };
    const XMFLOAT4 pondColor = { 0.12f, 0.50f, 0.75f, 1.0f };
    const XMFLOAT4 lakeColor = { 0.06f, 0.28f, 0.48f, 1.0f };

    const float extentX = (static_cast<float>(m_piece.gridWidth - 1) * m_piece.cellSize) * 0.5f;
    const float extentZ = (static_cast<float>(m_piece.gridDepth - 1) * m_piece.cellSize) * 0.5f;
    Geometory::AddLine({ -extentX - 2.0f, 0.0f, 0.0f }, { extentX + 2.0f, 0.0f, 0.0f }, axisColor);
    Geometory::AddLine({ 0.0f, 0.0f, -extentZ - 2.0f }, { 0.0f, 0.0f, extentZ + 2.0f }, axisColor);

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            const XMFLOAT3 current = GetVertexWorldPosition(x, z);
            const bool isPrimarySelected = (x == m_selectedX && z == m_selectedZ);
            const bool isMultiSelected = IsVertexSelected(x, z);
            const bool hasHeight = std::fabs(current.y) > 0.001f;
            const XMFLOAT4 lineColor = isPrimarySelected ? selectedColor :
                (isMultiSelected ? multiSelectedColor : (hasHeight ? raisedColor : flatColor));

            // 条件に該当する場合は、`Geometory::AddLine` の処理を実行します。
            if (x + 1 < m_piece.gridWidth)
            {
                Geometory::AddLine(current, GetVertexWorldPosition(x + 1, z), lineColor);
            }
            // 条件に該当する場合は、`Geometory::AddLine` の処理を実行します。
            if (z + 1 < m_piece.gridDepth)
            {
                Geometory::AddLine(current, GetVertexWorldPosition(x, z + 1), lineColor);
            }
            // 条件に該当する場合は、後続処理に必要な値を準備します。
            if (isMultiSelected)
            {
                const XMFLOAT4 markerColor = isPrimarySelected ? selectedColor : multiSelectedColor;
                Geometory::AddLine(
                    { current.x, current.y + 0.1f, current.z },
                    { current.x, current.y + kSelectionMarkerHeight, current.z },
                    markerColor);
            }
        }
    }

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int cellZ = 0; cellZ < m_piece.gridDepth - 1; ++cellZ)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int cellX = 0; cellX < m_piece.gridWidth - 1; ++cellX)
        {
            const NarakuPiece::CellData* const cellData = GetCellData(cellX, cellZ);
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (cellData == nullptr)
            {
                continue;
            }

            const XMFLOAT3 p00 = GetVertexWorldPosition(cellX, cellZ);
            const XMFLOAT3 p10 = GetVertexWorldPosition(cellX + 1, cellZ);
            const XMFLOAT3 p01 = GetVertexWorldPosition(cellX, cellZ + 1);
            const XMFLOAT3 p11 = GetVertexWorldPosition(cellX + 1, cellZ + 1);
            const auto raise = [](const XMFLOAT3& pos)
            {
                return XMFLOAT3{ pos.x, pos.y + kCellOverlayYOffset, pos.z };
            };
            const XMFLOAT3 e00 = raise(p00);
            const XMFLOAT3 e10 = raise(p10);
            const XMFLOAT3 e01 = raise(p01);
            const XMFLOAT3 e11 = raise(p11);
            const XMFLOAT3 center = GetCellWorldPosition(cellX, cellZ);
            const XMFLOAT3 raisedCenter = { center.x, center.y + kCellOverlayYOffset, center.z };
            const float maximumSlopeDegrees = NarakuMap::CalculateMaximumSlopeDegrees(
                p00.y, p10.y, p01.y, p11.y, m_piece.cellSize);
            const bool automaticallyBlockedBySlope =
                !cellData->deleted && cellData->walkable &&
                maximumSlopeDegrees > NarakuMap::MaximumWalkableSlopeDegrees;

            if (cellData->waterDepth != NarakuPiece::WaterDepth::None)
            {
                const XMFLOAT4 waterColor = cellData->waterDepth == NarakuPiece::WaterDepth::Puddle
                    ? puddleColor : (cellData->waterDepth == NarakuPiece::WaterDepth::Pond ? pondColor : lakeColor);
                Geometory::AddLine(e00, e10, waterColor);
                Geometory::AddLine(e10, e11, waterColor);
                Geometory::AddLine(e11, e01, waterColor);
                Geometory::AddLine(e01, e00, waterColor);
                Geometory::AddLine(e00, e11, waterColor);
                Geometory::AddLine(e10, e01, waterColor);
            }

            // 条件に該当する場合は、`Geometory::AddLine` の処理を実行します。
            if (cellData->deleted)
            {
                Geometory::AddLine(e00, e11, cellDeletedColor);
                Geometory::AddLine(e10, e01, cellDeletedColor);
            }
            // 条件に該当する場合は、`Geometory::AddLine` の処理を実行します。
            if (!cellData->walkable)
            {
                Geometory::AddLine(
                    { raisedCenter.x - 0.30f, raisedCenter.y, raisedCenter.z },
                    { raisedCenter.x + 0.30f, raisedCenter.y, raisedCenter.z },
                    cellBlockedColor);
                Geometory::AddLine(
                    { raisedCenter.x, raisedCenter.y, raisedCenter.z - 0.30f },
                    { raisedCenter.x, raisedCenter.y, raisedCenter.z + 0.30f },
                    cellBlockedColor);
            }
            if (automaticallyBlockedBySlope)
            {
                Geometory::AddLine(e00, e10, steepSlopeColor);
                Geometory::AddLine(e10, e11, steepSlopeColor);
                Geometory::AddLine(e11, e01, steepSlopeColor);
                Geometory::AddLine(e01, e00, steepSlopeColor);
                Geometory::AddLine(e00, e11, steepSlopeColor);
                Geometory::AddLine(e10, e01, steepSlopeColor);
            }
            // 条件に該当する場合は、`Geometory::AddLine` の処理を実行します。
            if (IsCellSelected(cellX, cellZ))
            {
                Geometory::AddLine(e00, e10, cellSelectedColor);
                Geometory::AddLine(e10, e11, cellSelectedColor);
                Geometory::AddLine(e11, e01, cellSelectedColor);
                Geometory::AddLine(e01, e00, cellSelectedColor);
            }
            // 先の条件に該当せず、この条件を満たす場合は、`Geometory::AddLine` の処理を実行します。
            else if (cellX == m_hoverCellX && cellZ == m_hoverCellZ)
            {
                Geometory::AddLine(e00, e10, cellHoverColor);
                Geometory::AddLine(e10, e11, cellHoverColor);
                Geometory::AddLine(e11, e01, cellHoverColor);
                Geometory::AddLine(e01, e00, cellHoverColor);
            }
        }
    }

    Geometory::DrawLines();
    DrawWaterOverlays3D();

    // 対象コレクションの各要素を順に処理します。
    for (const VertexSelection& selection : m_selectedVertices)
    {
        const XMFLOAT3 selectedPos = GetVertexWorldPosition(selection.x, selection.z);
        const XMFLOAT3 boxScale = (selection.x == m_selectedX && selection.z == m_selectedZ)
            ? XMFLOAT3{ 0.45f, 0.45f, 0.45f }
            : XMFLOAT3{ 0.25f, 0.25f, 0.25f };
        DrawDebugBox3D({ selectedPos.x, selectedPos.y + 0.2f, selectedPos.z }, boxScale);
    }

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);

    static const XMFLOAT4 kMiningColors[] =
    {
        { 0.30f, 0.90f, 0.95f, 1.0f },
        { 0.25f, 0.95f, 0.45f, 1.0f },
        { 0.95f, 0.75f, 0.25f, 1.0f },
        { 0.95f, 0.45f, 0.70f, 1.0f },
    };

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (size_t index = 0; index < m_piece.miningPoints.size(); ++index)
    {
        const NarakuPiece::MiningPointData& point = m_piece.miningPoints[index];
        // 条件に該当する場合は、その要素を処理対象から除外します。
        if (!IsValidCell(point.cell.x, point.cell.z))
        {
            continue;
        }

        const XMFLOAT3 center = GetCellWorldPosition(point.cell.x, point.cell.z);
        const XMFLOAT4 color = kMiningColors[ClampInt(point.visualType, 0, 3)];
        const XMFLOAT3 scale = (m_selectedGridObjectKind == GridObjectKind::MiningPoint &&
            m_selectedMiningPointIndex == static_cast<int>(index))
            ? XMFLOAT3{ 0.60f, 0.60f, 0.60f }
            : XMFLOAT3{ 0.38f, 0.38f, 0.38f };
        DrawDebugWireBox3D({ center.x, center.y + 0.25f, center.z }, scale, color);
        Geometory::AddLine({ center.x, center.y, center.z }, { center.x, center.y + 0.9f, center.z }, color);
    }

    for (size_t index = 0; index < m_piece.fishingPoints.size(); ++index)
    {
        const NarakuPiece::FishingPointData& point = m_piece.fishingPoints[index];
        if (!IsValidCell(point.shoreCell.x, point.shoreCell.z) || !IsValidCell(point.waterCell.x, point.waterCell.z)) continue;
        const XMFLOAT3 shore = GetCellWorldPosition(point.shoreCell.x, point.shoreCell.z);
        const XMFLOAT3 water = GetCellWorldPosition(point.waterCell.x, point.waterCell.z);
        const bool selected = m_selectedGridObjectKind == GridObjectKind::FishingPoint &&
            m_selectedFishingPointIndex == static_cast<int>(index);
        const XMFLOAT4 color = selected
            ? XMFLOAT4{ 1.0f, 0.95f, 0.25f, 1.0f }
            : XMFLOAT4{ 0.20f, 0.75f, 1.0f, 1.0f };

        const float rodHeight = selected ? 2.2f : 1.8f;
        const float markerSize = selected ? 0.65f : 0.45f;
        const XMFLOAT3 rodBase{ shore.x, shore.y + 0.05f, shore.z };
        const XMFLOAT3 rodTop{ shore.x, shore.y + rodHeight, shore.z };
        const XMFLOAT3 floatCenter{ water.x, water.y + 0.20f, water.z };

        DrawDebugWireBox3D(
            { shore.x, shore.y + 0.08f, shore.z },
            { markerSize, 0.16f, markerSize },
            color);
        DrawDebugWireBox3D(
            { shore.x, shore.y + rodHeight * 0.5f, shore.z },
            { 0.12f, rodHeight, 0.12f },
            color);
        Geometory::AddLine(rodBase, rodTop, color);
        Geometory::AddLine(rodTop, floatCenter, color);

        DrawDebugWireBox3D(
            floatCenter,
            { selected ? 0.28f : 0.20f, selected ? 0.40f : 0.30f, selected ? 0.28f : 0.20f },
            color);
        Geometory::AddLine(
            { water.x - markerSize * 0.5f, water.y + 0.08f, water.z },
            { water.x + markerSize * 0.5f, water.y + 0.08f, water.z },
            color);
        Geometory::AddLine(
            { water.x, water.y + 0.08f, water.z - markerSize * 0.5f },
            { water.x, water.y + 0.08f, water.z + markerSize * 0.5f },
            color);
    }

    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (m_piece.rope.enabled && IsValidCell(m_piece.rope.top.x, m_piece.rope.top.z) && IsValidCell(m_piece.rope.bottom.x, m_piece.rope.bottom.z))
    {
        const XMFLOAT4 ropeColor = (m_selectedGridObjectKind == GridObjectKind::Rope)
            ? XMFLOAT4{ 1.0f, 0.82f, 0.35f, 1.0f }
            : XMFLOAT4{ 0.95f, 0.55f, 0.25f, 1.0f };
        const XMFLOAT3 top = GetCellWorldPosition(m_piece.rope.top.x, m_piece.rope.top.z);
        const XMFLOAT3 bottom = GetCellWorldPosition(m_piece.rope.bottom.x, m_piece.rope.bottom.z);
        Geometory::AddLine({ top.x, top.y, top.z }, { top.x, top.y + 1.2f, top.z }, ropeColor);
        Geometory::AddLine({ bottom.x, bottom.y, bottom.z }, { bottom.x, bottom.y + 1.2f, bottom.z }, ropeColor);
        Geometory::AddLine({ top.x, top.y + 1.2f, top.z }, { bottom.x, bottom.y + 1.2f, bottom.z }, ropeColor);
    }

    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (m_piece.startReturnCandidate.enabled && IsValidCell(m_piece.startReturnCandidate.cell.x, m_piece.startReturnCandidate.cell.z))
    {
        const XMFLOAT4 startColor = (m_selectedGridObjectKind == GridObjectKind::StartReturn)
            ? XMFLOAT4{ 0.95f, 0.95f, 0.40f, 1.0f }
            : XMFLOAT4{ 0.90f, 0.25f, 0.85f, 1.0f };
        const XMFLOAT3 center = GetCellWorldPosition(
            m_piece.startReturnCandidate.cell.x,
            m_piece.startReturnCandidate.cell.z);
        DrawDebugWireBox3D({ center.x, center.y + 0.4f, center.z }, { 0.72f, 0.72f, 0.72f }, startColor);
        Geometory::AddLine({ center.x - 0.5f, center.y + 0.1f, center.z }, { center.x + 0.5f, center.y + 0.1f, center.z }, startColor);
        Geometory::AddLine({ center.x, center.y + 0.1f, center.z - 0.5f }, { center.x, center.y + 0.1f, center.z + 0.5f }, startColor);
    }

    if (m_piece.isSurface && m_piece.surfaceFacility.type != NarakuPiece::SurfaceFacilityType::None &&
        IsValidCell(m_piece.surfaceFacility.cell.x, m_piece.surfaceFacility.cell.z) &&
        IsValidCell(m_piece.surfaceFacility.cell.x + 1, m_piece.surfaceFacility.cell.z + 1))
    {
        const XMFLOAT3 first = GetCellWorldPosition(m_piece.surfaceFacility.cell.x, m_piece.surfaceFacility.cell.z);
        const XMFLOAT3 last = GetCellWorldPosition(m_piece.surfaceFacility.cell.x + 1, m_piece.surfaceFacility.cell.z + 1);
        const XMFLOAT3 center = { (first.x + last.x) * 0.5f, std::max(first.y, last.y), (first.z + last.z) * 0.5f };
        const XMFLOAT4 color = m_piece.surfaceFacility.type == NarakuPiece::SurfaceFacilityType::AbyssEntrance
            ? XMFLOAT4{ 0.75f, 0.25f, 0.95f, 1.0f } : XMFLOAT4{ 0.25f, 0.95f, 0.55f, 1.0f };

        // 施設モデルの取得（環境モデルカタログから探索、または直接静的ロード）
        Model* facilityModel = nullptr;
        XMFLOAT3 placementAnchor = {};
        for (const auto& asset : m_environmentModels)
        {
            if (asset.model != nullptr && (asset.path == m_piece.surfaceFacility.modelPath || asset.id == m_piece.surfaceFacility.modelPath))
            {
                facilityModel = asset.model;
                placementAnchor = asset.previewAnchor;
                break;
            }
        }

        if (facilityModel == nullptr && !m_piece.surfaceFacility.modelPath.empty())
        {
            const std::wstring resolvedWide = ResolvePieceHierarchyPath(Utf8ToWide(m_piece.surfaceFacility.modelPath));
            WIN32_FILE_ATTRIBUTE_DATA attributes = {};
            unsigned long long writeTime = 0;
            if (::GetFileAttributesExW(resolvedWide.c_str(), GetFileExInfoStandard, &attributes))
            {
                writeTime = (static_cast<unsigned long long>(attributes.ftLastWriteTime.dwHighDateTime) << 32u) |
                    static_cast<unsigned long long>(attributes.ftLastWriteTime.dwLowDateTime);
            }
            if (m_surfaceFacilityPreviewPath != m_piece.surfaceFacility.modelPath ||
                m_surfaceFacilityPreviewWriteTime != writeTime)
            {
                ReleaseSurfaceFacilityPreviewModel();
                m_surfaceFacilityPreviewPath = m_piece.surfaceFacility.modelPath;
                m_surfaceFacilityPreviewWriteTime = writeTime;
                const std::string resolved = WideToUtf8(resolvedWide);
                m_surfaceFacilityPreviewModel = new Model();
                if (!m_surfaceFacilityPreviewModel->LoadStatic(resolved.c_str(), 1.0f, Model::ZFlip))
                {
                    SAFE_DELETE(m_surfaceFacilityPreviewModel);
                }
                else
                {
                    XMFLOAT3 minVal = { FLT_MAX, FLT_MAX, FLT_MAX };
                    XMFLOAT3 maxVal = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
                    bool hasV = false;
                    for (unsigned int mIdx = 0; mIdx < m_surfaceFacilityPreviewModel->GetMeshNum(); ++mIdx)
                    {
                        const Model::Mesh* m = m_surfaceFacilityPreviewModel->GetMesh(mIdx);
                        if (!m) continue;
                        for (const auto& v : m->vertices)
                        {
                            hasV = true;
                            minVal.x = std::min(minVal.x, v.pos.x);
                            minVal.y = std::min(minVal.y, v.pos.y);
                            minVal.z = std::min(minVal.z, v.pos.z);
                            maxVal.x = std::max(maxVal.x, v.pos.x);
                            maxVal.y = std::max(maxVal.y, v.pos.y);
                            maxVal.z = std::max(maxVal.z, v.pos.z);
                        }
                    }
                    if (hasV)
                    {
                        m_surfaceFacilityPreviewAnchor = { (minVal.x + maxVal.x) * 0.5f, minVal.y, (minVal.z + maxVal.z) * 0.5f };
                    }
                }
            }
            facilityModel = m_surfaceFacilityPreviewModel;
            placementAnchor = m_surfaceFacilityPreviewAnchor;
        }

        // 施設モデルの描画（位置補正・拡大率・Y回転を反映）
        if (facilityModel != nullptr)
        {
            const float cosPitch = std::cos(m_cameraPitch);
            const XMFLOAT3 eye =
            {
                m_cameraTarget.x + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance,
                m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
                m_cameraTarget.z + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance
            };

            const float posX = center.x - m_piece.surfaceFacility.offsetX;
            const float posY = center.y + m_piece.surfaceFacility.offsetY;
            const float posZ = center.z + m_piece.surfaceFacility.offsetZ;

            XMFLOAT4X4 wvp[3] = {};
            XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
                XMMatrixTranslation(-placementAnchor.x, -placementAnchor.y, -placementAnchor.z) *
                XMMatrixScaling(m_piece.surfaceFacility.scaleX, m_piece.surfaceFacility.scaleY, m_piece.surfaceFacility.scaleZ) *
                XMMatrixRotationY(-DirectX::XM_PIDIV2 * static_cast<float>(m_piece.surfaceFacility.rotationQuarterTurns)) *
                XMMatrixTranslation(posX, posY, posZ)));
            XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
            XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
            ShaderList::SetWVP(wvp);
            ShaderList::SetCameraPos(eye);
            facilityModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
            facilityModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
            for (unsigned int meshIndex = 0; meshIndex < facilityModel->GetMeshNum(); ++meshIndex)
            {
                const Model::Mesh* mesh = facilityModel->GetMesh(meshIndex);
                if (mesh == nullptr) continue;
                const Model::Material* sourceMaterial = facilityModel->GetMaterial(mesh->materialID);
                if (sourceMaterial != nullptr)
                {
                    Model::Material material = *sourceMaterial;
                    ShaderList::SetMaterial(material);
                }
                facilityModel->Draw(static_cast<int>(meshIndex));
            }
        }

        // 施設範囲枠の描画
        DrawDebugWireBox3D({ center.x, center.y + 0.4f, center.z },
            { m_piece.cellSize * 2.0f, 0.8f, m_piece.cellSize * 2.0f }, color);

        // 施設正面を示す矢印の描画
        XMFLOAT3 dir = { 0.0f, 0.0f, 1.0f }; // デフォルトSouth
        if (m_piece.surfaceFacility.facing == NarakuPiece::Direction::North) dir = { 0.0f, 0.0f, -1.0f };
        else if (m_piece.surfaceFacility.facing == NarakuPiece::Direction::South) dir = { 0.0f, 0.0f, 1.0f };
        else if (m_piece.surfaceFacility.facing == NarakuPiece::Direction::East) dir = { -1.0f, 0.0f, 0.0f };
        else if (m_piece.surfaceFacility.facing == NarakuPiece::Direction::West) dir = { 1.0f, 0.0f, 0.0f };

        const float arrowY = center.y + 0.15f;
        const XMFLOAT3 arrowStart = { center.x, arrowY, center.z };
        const float arrowLength = m_piece.cellSize * 1.6f;
        const XMFLOAT3 arrowEnd = { center.x + dir.x * arrowLength, arrowY, center.z + dir.z * arrowLength };
        const XMFLOAT4 arrowColor = { 1.0f, 0.85f, 0.15f, 1.0f };

        Geometory::AddLine(arrowStart, arrowEnd, arrowColor);

        const XMFLOAT3 side = { -dir.z, 0.0f, dir.x };
        const float headLength = 0.45f;
        const float headWidth = 0.25f;
        const XMFLOAT3 headLeft = {
            arrowEnd.x - dir.x * headLength + side.x * headWidth,
            arrowY,
            arrowEnd.z - dir.z * headLength + side.z * headWidth
        };
        const XMFLOAT3 headRight = {
            arrowEnd.x - dir.x * headLength - side.x * headWidth,
            arrowY,
            arrowEnd.z - dir.z * headLength - side.z * headWidth
        };
        Geometory::AddLine(arrowEnd, headLeft, arrowColor);
        Geometory::AddLine(arrowEnd, headRight, arrowColor);
    }

    DrawBasePreview3D();

    // 条件に該当する場合は、`IsValidCell` の処理を実行します。
    if (m_piece.layerTransition.ropePointEnabled &&
        IsValidCell(m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z))
    {
        const XMFLOAT3 center = GetCellWorldPosition(m_piece.layerTransition.ropePoint.x, m_piece.layerTransition.ropePoint.z);
        const XMFLOAT4 color = (m_selectedGridObjectKind == GridObjectKind::LayerRopePoint)
            ? XMFLOAT4{ 1.0f, 0.85f, 0.25f, 1.0f }
            : XMFLOAT4{ 0.95f, 0.65f, 0.15f, 1.0f };
        DrawDebugWireBox3D({ center.x, center.y + 0.55f, center.z }, { 0.65f, 1.10f, 0.65f }, color);
    }

    // 条件に該当する場合は、`IsValidCell` の処理を実行します。
    if (m_piece.layerTransition.loadPointEnabled &&
        IsValidCell(m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z))
    {
        const XMFLOAT3 center = GetCellWorldPosition(m_piece.layerTransition.loadPoint.x, m_piece.layerTransition.loadPoint.z);
        const XMFLOAT4 color = (m_selectedGridObjectKind == GridObjectKind::LayerLoadPoint)
            ? XMFLOAT4{ 0.35f, 0.95f, 1.0f, 1.0f }
            : XMFLOAT4{ 0.20f, 0.70f, 0.95f, 1.0f };
        DrawDebugWireBox3D({ center.x, center.y + 0.20f, center.z }, { 1.0f, 0.35f, 1.0f }, color);
    }

    // 条件に該当する場合は、`IsValidCell` の処理を実行します。
    if ((m_editMode == EditMode::GridObject || m_editMode == EditMode::EnvironmentObject) &&
        IsValidCell(m_hoverCellX, m_hoverCellZ))
    {
        const XMFLOAT4 hoverColor = { 1.0f, 1.0f, 1.0f, 0.95f };
        const XMFLOAT3 p00 = GetVertexWorldPosition(m_hoverCellX, m_hoverCellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(m_hoverCellX + 1, m_hoverCellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(m_hoverCellX, m_hoverCellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(m_hoverCellX + 1, m_hoverCellZ + 1);
        Geometory::AddLine(p00, p10, hoverColor);
        Geometory::AddLine(p10, p11, hoverColor);
        Geometory::AddLine(p11, p01, hoverColor);
        Geometory::AddLine(p01, p00, hoverColor);
    }

    DrawEnvironmentObjects3D();
    Geometory::DrawLines();
}


void SceneNarakuPieceEditor::UpdateCamera()
{
    EDITOR_PROFILE_FUNCTION();
    ImGuiIO& io = ImGui::GetIO();
    const bool mouseInPreview = IsMouseInsidePreviewImage();
    // 条件に該当する場合は、対応する編集処理を実行します。
    if ((!mouseInPreview && !m_previewImageHovered) ||
        (io.WantCaptureMouse && !m_previewImageHovered))
    {
        return;
    }

    const POINT mouseDelta = GetMouseDelta();
    const bool altPressed = IsEditorAltPressed(io);

    // 条件に該当する場合は、`m_cameraYaw` の状態を更新します。
    if (altPressed && IsMouseLeftPress())
    {
        m_cameraYaw += static_cast<float>(mouseDelta.x) * kCameraOrbitSpeed;
        const float pitchSign = m_invertOrbitY ? -1.0f : 1.0f;
        m_cameraPitch += static_cast<float>(mouseDelta.y) * kCameraOrbitSpeed * pitchSign;
    }

    m_cameraPitch = ClampFloat(m_cameraPitch, kMinCameraPitch, kMaxCameraPitch);

    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (IsMouseMiddlePress())
    {
        const float cosPitch = std::cos(m_cameraPitch);
        const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
            -std::sin(m_cameraYaw) * cosPitch,
            -std::sin(m_cameraPitch),
            -std::cos(m_cameraYaw) * cosPitch,
            0.0f));
        const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        const XMVECTOR right = XMVector3Normalize(XMVector3Cross(worldUp, forward));
        const XMVECTOR cameraUp = XMVector3Normalize(XMVector3Cross(forward, right));

        XMFLOAT3 right3 = {};
        XMFLOAT3 up3 = {};
        XMStoreFloat3(&right3, right);
        XMStoreFloat3(&up3, cameraUp);

        const float panScale = std::max(0.05f, m_cameraDistance * kCameraPanScaleFactor);
        const float horizontalDelta = static_cast<float>(mouseDelta.x);
        const float verticalDelta = static_cast<float>(mouseDelta.y);

        m_cameraTarget.x -= right3.x * horizontalDelta * panScale;
        m_cameraTarget.y -= right3.y * horizontalDelta * panScale;
        m_cameraTarget.z -= right3.z * horizontalDelta * panScale;

        m_cameraTarget.x += up3.x * verticalDelta * panScale;
        m_cameraTarget.y += up3.y * verticalDelta * panScale;
        m_cameraTarget.z += up3.z * verticalDelta * panScale;
    }

    const float wheelDelta = GetMouseWheelDelta();
    // 条件に該当する場合は、`m_cameraDistance` の状態を更新します。
    if (wheelDelta != 0.0f)
    {
        m_cameraDistance -= wheelDelta * std::max(1.0f, m_cameraDistance * 0.10f);
        m_cameraDistance = ClampFloat(m_cameraDistance, kMinCameraDistance, kMaxCameraDistance);
    }
}


void SceneNarakuPieceEditor::UpdateCameraMatrices()
{
    EDITOR_PROFILE_FUNCTION();
    m_cameraPitch = ClampFloat(m_cameraPitch, kMinCameraPitch, kMaxCameraPitch);
    m_cameraDistance = ClampFloat(m_cameraDistance, kMinCameraDistance, kMaxCameraDistance);

    const float cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 eyePos =
    {
        m_cameraTarget.x + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance
    };

    const XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 1.0f);
    const XMVECTOR target = XMVectorSet(m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z, 1.0f);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(eye, target, up));

    const XMFLOAT2 viewportSize = GetPreviewViewportSize();
    const float aspect = viewportSize.x / viewportSize.y;
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(kCameraFovDegrees),
        aspect,
        kCameraNearPlane,
        kCameraFarPlane);
    XMStoreFloat4x4(&m_projectionMatrix, projection);
    XMStoreFloat4x4(&m_viewProjectionMatrix, XMLoadFloat4x4(&m_viewMatrix) * projection);
}

void SceneNarakuPieceEditor::ResetCamera()
{
    EDITOR_PROFILE_FUNCTION();
    m_cameraTarget = { 0.0f, 0.0f, 0.0f };
    m_cameraYaw = kInitialCameraYaw;
    m_cameraPitch = kInitialCameraPitch;
    m_cameraDistance = kInitialCameraDistance;
}

bool SceneNarakuPieceEditor::ProjectWorldToScreen(const XMFLOAT3& worldPos, XMFLOAT2& outScreen) const
{
    const XMVECTOR clip = XMVector3TransformCoord(
        XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f),
        XMLoadFloat4x4(&m_viewProjectionMatrix));

    XMFLOAT3 ndc = {};
    XMStoreFloat3(&ndc, clip);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (ndc.z < 0.0f || ndc.z > 1.0f)
    {
        return false;
    }

    const XMFLOAT2 viewportSize = GetPreviewViewportSize();
    outScreen.x = (ndc.x * 0.5f + 0.5f) * viewportSize.x + m_previewImageTopLeft.x;
    outScreen.y = (-ndc.y * 0.5f + 0.5f) * viewportSize.y + m_previewImageTopLeft.y;
    return true;
}

bool SceneNarakuPieceEditor::PickTerrainVertex(POINT mousePos, int& outX, int& outZ) const
{
    EDITOR_PROFILE_FUNCTION();
    float bestDistanceSquared = kPickThresholdPx * kPickThresholdPx;
    bool found = false;

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int z = 0; z < m_piece.gridDepth; ++z)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int x = 0; x < m_piece.gridWidth; ++x)
        {
            XMFLOAT2 screen = {};
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (!ProjectWorldToScreen(GetVertexWorldPosition(x, z), screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distanceSquared = dx * dx + dy * dy;
            // 条件に該当する場合は、最短距離の二乗値を更新します。
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                outX = x;
                outZ = z;
                found = true;
            }
        }
    }

    return found;
}

bool SceneNarakuPieceEditor::PickTerrainCell(POINT mousePos, int& outX, int& outZ) const
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (m_piece.gridWidth < 2 || m_piece.gridDepth < 2)
    {
        return false;
    }

    float bestDistanceSquared = kCellPickThresholdPx * kCellPickThresholdPx;
    bool found = false;

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int z = 0; z < m_piece.gridDepth - 1; ++z)
    {
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (int x = 0; x < m_piece.gridWidth - 1; ++x)
        {
            const XMFLOAT3 center = GetCellWorldPosition(x, z);
            XMFLOAT2 screen = {};
            // 条件に該当する場合は、その要素を処理対象から除外します。
            if (!ProjectWorldToScreen(center, screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distanceSquared = dx * dx + dy * dy;
            // 条件に該当する場合は、最短距離の二乗値を更新します。
            if (distanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = distanceSquared;
                outX = x;
                outZ = z;
                found = true;
            }
        }
    }

    return found;
}

void SceneNarakuPieceEditor::DrawDebugBox3D(const XMFLOAT3& pos, const XMFLOAT3& scale) const
{
    EDITOR_PROFILE_FUNCTION();
    const XMMATRIX worldMatrix = XMMatrixScaling(scale.x, scale.y, scale.z) * XMMatrixTranslation(pos.x, pos.y, pos.z);

    XMFLOAT4X4 world = {};
    XMFLOAT4X4 view = {};
    XMFLOAT4X4 projection = {};

    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));

    Geometory::SetWorld(world);
    Geometory::SetView(view);
    Geometory::SetProjection(projection);
    Geometory::DrawBox();
}

void SceneNarakuPieceEditor::DrawDebugWireBox3D(const XMFLOAT3& pos, const XMFLOAT3& scale, const XMFLOAT4& color) const
{
    EDITOR_PROFILE_FUNCTION();
    const float halfX = scale.x * 0.5f;
    const float halfY = scale.y * 0.5f;
    const float halfZ = scale.z * 0.5f;
    const XMFLOAT3 corners[8] =
    {
        { pos.x - halfX, pos.y - halfY, pos.z - halfZ },
        { pos.x + halfX, pos.y - halfY, pos.z - halfZ },
        { pos.x - halfX, pos.y - halfY, pos.z + halfZ },
        { pos.x + halfX, pos.y - halfY, pos.z + halfZ },
        { pos.x - halfX, pos.y + halfY, pos.z - halfZ },
        { pos.x + halfX, pos.y + halfY, pos.z - halfZ },
        { pos.x - halfX, pos.y + halfY, pos.z + halfZ },
        { pos.x + halfX, pos.y + halfY, pos.z + halfZ },
    };
    const int edges[12][2] =
    {
        { 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
        { 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
        { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
    };

    // 対象コレクションの各要素を順に処理します。
    for (const auto& edge : edges)
    {
        Geometory::AddLine(corners[edge[0]], corners[edge[1]], color);
    }
}


