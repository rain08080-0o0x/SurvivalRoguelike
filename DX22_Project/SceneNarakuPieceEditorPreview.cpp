#include "SceneNarakuPieceEditor.h"
#include "EditorPerformanceProfiler.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "Model.h"
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
    ImGui::End();
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
        { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
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
    EDITOR_PROFILE_FUNCTION();
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

    const XMFLOAT4 axisColor = { 0.25f, 0.25f, 0.28f, 1.0f };
    const XMFLOAT4 selectedColor = { 0.95f, 0.70f, 0.20f, 1.0f };
    const XMFLOAT4 multiSelectedColor = { 0.30f, 0.55f, 0.95f, 1.0f };
    const XMFLOAT4 raisedColor = { 0.30f, 0.75f, 0.55f, 1.0f };
    const XMFLOAT4 flatColor = { 0.65f, 0.65f, 0.68f, 1.0f };
    const XMFLOAT4 cellHoverColor = { 1.0f, 1.0f, 1.0f, 1.0f };
    const XMFLOAT4 cellSelectedColor = { 1.0f, 0.85f, 0.20f, 1.0f };
    const XMFLOAT4 cellDeletedColor = { 0.85f, 0.40f, 0.40f, 1.0f };
    const XMFLOAT4 cellBlockedColor = { 0.95f, 0.15f, 0.15f, 1.0f };

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
        m_cameraYaw -= static_cast<float>(mouseDelta.x) * kCameraOrbitSpeed;
        const float pitchSign = m_invertOrbitY ? -1.0f : 1.0f;
        m_cameraPitch += static_cast<float>(mouseDelta.y) * kCameraOrbitSpeed * pitchSign;
    }

    m_cameraPitch = ClampFloat(m_cameraPitch, kMinCameraPitch, kMaxCameraPitch);

    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (IsMouseMiddlePress())
    {
        const float cosPitch = std::cos(m_cameraPitch);
        const XMVECTOR forward = XMVector3Normalize(XMVectorSet(
            -std::cos(m_cameraYaw) * cosPitch,
            -std::sin(m_cameraPitch),
            -std::sin(m_cameraYaw) * cosPitch,
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
        m_cameraTarget.x + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance
    };

    const XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 1.0f);
    const XMVECTOR target = XMVectorSet(m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z, 1.0f);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMStoreFloat4x4(&m_viewMatrix, XMMatrixLookAtLH(eye, target, up));

    const XMFLOAT2 viewportSize = GetPreviewViewportSize();
    const float aspect = viewportSize.x / viewportSize.y;
    XMStoreFloat4x4(
        &m_projectionMatrix,
        XMMatrixPerspectiveFovLH(XMConvertToRadians(kCameraFovDegrees), aspect, kCameraNearPlane, kCameraFarPlane));
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
    EDITOR_PROFILE_FUNCTION();
    const XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    const XMVECTOR clip = XMVector3TransformCoord(
        XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f),
        view * projection);

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
    float bestDistance = kPickThresholdPx;
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
            const float distance = std::sqrt(dx * dx + dy * dy);
            // 条件に該当する場合は、`bestDistance` の状態を更新します。
            if (distance < bestDistance)
            {
                bestDistance = distance;
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

    float bestDistance = kCellPickThresholdPx;
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
            const float distance = std::sqrt(dx * dx + dy * dy);
            // 条件に該当する場合は、`bestDistance` の状態を更新します。
            if (distance < bestDistance)
            {
                bestDistance = distance;
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


