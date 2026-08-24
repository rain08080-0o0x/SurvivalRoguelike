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

void SceneNarakuPieceEditor::ReleaseEnvironmentModels()
{
    EDITOR_PROFILE_FUNCTION();
    // 対象コレクションの各要素を順に処理します。
    for (EnvironmentModelAsset& asset : m_environmentModels)
    {
        SAFE_DELETE(asset.thumbnailDepthStencil);
        SAFE_DELETE(asset.thumbnailRenderTarget);
        SAFE_DELETE(asset.model);
    }
    m_environmentModels.clear();
    m_selectedEnvironmentModelIndex = -1;
}

void SceneNarakuPieceEditor::UpdateEnvironmentModelBounds(EnvironmentModelAsset& asset)
{
    EDITOR_PROFILE_FUNCTION();
    asset.hasBounds = false;
    asset.boundsMin = { -0.5f, 0.0f, -0.5f };
    asset.boundsMax = { 0.5f, 1.0f, 0.5f };
    asset.previewAnchor = {};
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (asset.model == nullptr) return;

    XMFLOAT3 minValue = { FLT_MAX, FLT_MAX, FLT_MAX };
    XMFLOAT3 maxValue = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    bool hasVertex = false;
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        // 条件に該当する場合は、`for` の処理を実行します。
        if (mesh == nullptr) continue;
        // 対象コレクションの各要素を順に処理します。
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
    // 条件に該当する場合は、`asset.boundsMin` の状態を更新します。
    if (!hasVertex) return;

    asset.boundsMin = minValue;
    asset.boundsMax = maxValue;
    asset.previewAnchor = {
        (minValue.x + maxValue.x) * 0.5f,
        minValue.y,
        (minValue.z + maxValue.z) * 0.5f };
    asset.hasBounds = true;
    asset.thumbnailDirty = true;
}

bool SceneNarakuPieceEditor::EnsureEnvironmentModelThumbnail(EnvironmentModelAsset& asset, unsigned int size)
{
    EDITOR_PROFILE_FUNCTION();
    size = std::max(64U, size);
    // 条件に該当する場合は、現在の処理をここで終了します。
    if (asset.thumbnailRenderTarget != nullptr && asset.thumbnailDepthStencil != nullptr && asset.thumbnailSize == size)
    {
        return true;
    }

    SAFE_DELETE(asset.thumbnailDepthStencil);
    SAFE_DELETE(asset.thumbnailRenderTarget);
    asset.thumbnailRenderTarget = new RenderTarget();
    // 条件に該当する場合は、不要になったリソースを解放します。
    if (FAILED(asset.thumbnailRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, size, size)))
    {
        SAFE_DELETE(asset.thumbnailRenderTarget);
        return false;
    }
    asset.thumbnailDepthStencil = new DepthStencil();
    // 条件に該当する場合は、不要になったリソースを解放します。
    if (FAILED(asset.thumbnailDepthStencil->Create(size, size, false)))
    {
        SAFE_DELETE(asset.thumbnailDepthStencil);
        SAFE_DELETE(asset.thumbnailRenderTarget);
        return false;
    }
    asset.thumbnailSize = size;
    asset.thumbnailDirty = true;
    return true;
}

void SceneNarakuPieceEditor::RenderEnvironmentModelThumbnail(EnvironmentModelAsset& asset, unsigned int size)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、追加条件を確認して処理を絞り込みます。
    if (asset.model == nullptr || !EnsureEnvironmentModelThumbnail(asset, size)) return;
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!asset.thumbnailDirty && asset.thumbnailRenderTarget->GetResource() != nullptr) return;

    RenderTarget* targets[1] = { asset.thumbnailRenderTarget };
    SetRenderTargets(1, targets, asset.thumbnailDepthStencil);
    const float clearColor[4] = { 0.055f, 0.065f, 0.080f, 1.0f };
    asset.thumbnailRenderTarget->Clear(clearColor);
    asset.thumbnailDepthStencil->Clear();

    const XMFLOAT3 modelSize = {
        std::max(0.001f, (asset.boundsMax.x - asset.boundsMin.x) * asset.defaultScale.x),
        std::max(0.001f, (asset.boundsMax.y - asset.boundsMin.y) * asset.defaultScale.y),
        std::max(0.001f, (asset.boundsMax.z - asset.boundsMin.z) * asset.defaultScale.z) };
    const float extent = std::max(0.25f, std::max(modelSize.x, std::max(modelSize.y, modelSize.z)));
    const XMFLOAT3 eye = { extent * 1.45f, extent * 1.10f, -extent * 1.80f };
    const XMFLOAT3 look = { 0.0f, modelSize.y * 0.45f, 0.0f };

    XMFLOAT4X4 wvp[3] = {};
    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
        XMMatrixTranslation(-asset.previewAnchor.x, -asset.previewAnchor.y, -asset.previewAnchor.z) *
        XMMatrixScaling(asset.defaultScale.x, asset.defaultScale.y, asset.defaultScale.z)));
    XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(XMMatrixLookAtLH(
        XMLoadFloat3(&eye), XMLoadFloat3(&look), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))));
    XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(XMMatrixPerspectiveFovLH(
        XMConvertToRadians(35.0f), 1.0f, 0.01f, std::max(100.0f, extent * 12.0f))));
    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (mesh == nullptr) continue;
        const Model::Material* sourceMaterial = asset.model->GetMaterial(mesh->materialID);
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (sourceMaterial != nullptr)
        {
            Model::Material material = *sourceMaterial;
            material.ambient = { 0.72f, 0.72f, 0.72f, 1.0f };
            ShaderList::SetMaterial(material);
        }
        asset.model->Draw(static_cast<int>(meshIndex));
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
    asset.thumbnailDirty = false;
}

void* SceneNarakuPieceEditor::GetEnvironmentModelThumbnailTextureId(int index, unsigned int size)
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (index < 0 || index >= static_cast<int>(m_environmentModels.size())) return nullptr;
    EnvironmentModelAsset& asset = m_environmentModels[static_cast<size_t>(index)];
    RenderEnvironmentModelThumbnail(asset, size);
    return asset.thumbnailRenderTarget != nullptr
        ? reinterpret_cast<void*>(asset.thumbnailRenderTarget->GetResource())
        : nullptr;
}

bool SceneNarakuPieceEditor::EnsureEnvironmentModelPopupPreview(unsigned int size)
{
    EDITOR_PROFILE_FUNCTION();
    size = std::max(128U, size);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (m_environmentModelPopupRenderTarget != nullptr &&
        m_environmentModelPopupDepthStencil != nullptr &&
        m_environmentModelPopupPreviewSize == size)
    {
        return true;
    }

    SAFE_DELETE(m_environmentModelPopupDepthStencil);
    SAFE_DELETE(m_environmentModelPopupRenderTarget);
    m_environmentModelPopupRenderTarget = new RenderTarget();
    // 条件に該当する場合は、不要になったリソースを解放します。
    if (FAILED(m_environmentModelPopupRenderTarget->Create(DXGI_FORMAT_R8G8B8A8_UNORM, size, size)))
    {
        SAFE_DELETE(m_environmentModelPopupRenderTarget);
        return false;
    }
    m_environmentModelPopupDepthStencil = new DepthStencil();
    // 条件に該当する場合は、不要になったリソースを解放します。
    if (FAILED(m_environmentModelPopupDepthStencil->Create(size, size, false)))
    {
        SAFE_DELETE(m_environmentModelPopupDepthStencil);
        SAFE_DELETE(m_environmentModelPopupRenderTarget);
        return false;
    }
    m_environmentModelPopupPreviewSize = size;
    return true;
}

void SceneNarakuPieceEditor::RenderEnvironmentModelPopupPreview(unsigned int size)
{
    EDITOR_PROFILE_FUNCTION();
    Model* model = nullptr;
    XMFLOAT3 boundsMin = m_environmentModelPopupBoundsMin;
    XMFLOAT3 boundsMax = m_environmentModelPopupBoundsMax;
    XMFLOAT3 anchor = m_environmentModelPopupPreviewAnchor;
    // 条件に該当する場合は、`model` の状態を更新します。
    if (m_environmentModelPopupIsNew)
    {
        model = m_environmentModelPopupPreviewModel;
    }
    // 先の条件に該当せず、この条件を満たす場合は、対応する編集処理を実行します。
    else if (m_selectedEnvironmentModelIndex >= 0 &&
        m_selectedEnvironmentModelIndex < static_cast<int>(m_environmentModels.size()))
    {
        const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
        model = asset.model;
        boundsMin = asset.boundsMin;
        boundsMax = asset.boundsMax;
        anchor = asset.previewAnchor;
    }
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (model == nullptr || !EnsureEnvironmentModelPopupPreview(size)) return;

    RenderTarget* targets[1] = { m_environmentModelPopupRenderTarget };
    SetRenderTargets(1, targets, m_environmentModelPopupDepthStencil);
    const float clearColor[4] = { 0.055f, 0.065f, 0.080f, 1.0f };
    m_environmentModelPopupRenderTarget->Clear(clearColor);
    m_environmentModelPopupDepthStencil->Clear();

    const XMFLOAT3 scale = {
        std::max(0.01f, m_environmentModelScaleInput.x),
        std::max(0.01f, m_environmentModelScaleInput.y),
        std::max(0.01f, m_environmentModelScaleInput.z) };
    const XMFLOAT3 modelSize = {
        std::max(0.001f, (boundsMax.x - boundsMin.x) * scale.x),
        std::max(0.001f, (boundsMax.y - boundsMin.y) * scale.y),
        std::max(0.001f, (boundsMax.z - boundsMin.z) * scale.z) };
    const float cellSize = std::max(0.1f, m_piece.cellSize);
    const float extent = std::max(
        cellSize * 2.25f,
        std::max(modelSize.y, std::max(modelSize.x, modelSize.z)));
    const XMFLOAT3 eye = { extent * 1.45f, extent * 1.10f, -extent * 1.80f };
    const XMFLOAT3 look = { 0.0f, modelSize.y * 0.35f, 0.0f };
    const XMMATRIX view = XMMatrixLookAtLH(
        XMLoadFloat3(&eye), XMLoadFloat3(&look), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMMATRIX projection = XMMatrixPerspectiveFovLH(
        XMConvertToRadians(35.0f), 1.0f, 0.01f, std::max(100.0f, extent * 12.0f));

    XMFLOAT4X4 wvp[3] = {};
    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(XMMatrixIdentity()));
    XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(view));
    XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(projection));
    ShaderList::SetWVP(wvp);
    const float gridExtent = cellSize * 2.0f;
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (int line = -2; line <= 2; ++line)
    {
        const float offset = static_cast<float>(line) * cellSize;
        const XMFLOAT4 color = line == 0
            ? XMFLOAT4{ 0.45f, 0.65f, 0.85f, 1.0f }
            : XMFLOAT4{ 0.30f, 0.34f, 0.40f, 1.0f };
        Geometory::AddLine({ offset, 0.0f, -gridExtent }, { offset, 0.0f, gridExtent }, color);
        Geometory::AddLine({ -gridExtent, 0.0f, offset }, { gridExtent, 0.0f, offset }, color);
    }
    Geometory::DrawLines();

    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
        XMMatrixTranslation(-anchor.x, -anchor.y, -anchor.z) *
        XMMatrixScaling(scale.x, scale.y, scale.z)));
    ShaderList::SetWVP(wvp);
    ShaderList::SetCameraPos(eye);
    model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (unsigned int meshIndex = 0; meshIndex < model->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = model->GetMesh(meshIndex);
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (mesh == nullptr) continue;
        const Model::Material* sourceMaterial = model->GetMaterial(mesh->materialID);
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (sourceMaterial != nullptr)
        {
            Model::Material material = *sourceMaterial;
            material.ambient = { 0.72f, 0.72f, 0.72f, 1.0f };
            ShaderList::SetMaterial(material);
        }
        model->Draw(static_cast<int>(meshIndex));
    }

    RenderTarget* defaultTarget[1] = { GetDefaultRTV() };
    SetRenderTargets(1, defaultTarget, GetDefaultDSV());
}

void* SceneNarakuPieceEditor::GetEnvironmentModelPopupPreviewTextureId(unsigned int size)
{
    EDITOR_PROFILE_FUNCTION();
    RenderEnvironmentModelPopupPreview(size);
    return m_environmentModelPopupRenderTarget != nullptr
        ? reinterpret_cast<void*>(m_environmentModelPopupRenderTarget->GetResource())
        : nullptr;
}

void SceneNarakuPieceEditor::ReleaseEnvironmentModelPopupPreview()
{
    EDITOR_PROFILE_FUNCTION();
    SAFE_DELETE(m_environmentModelPopupPreviewModel);
    SAFE_DELETE(m_environmentModelPopupDepthStencil);
    SAFE_DELETE(m_environmentModelPopupRenderTarget);
    m_environmentModelPopupPreviewSize = 0;
    m_environmentModelPopupBoundsMin = { -0.5f, 0.0f, -0.5f };
    m_environmentModelPopupBoundsMax = { 0.5f, 1.0f, 0.5f };
    m_environmentModelPopupPreviewAnchor = {};
}

void SceneNarakuPieceEditor::LoadEnvironmentModelCatalog()
{
    EDITOR_PROFILE_FUNCTION();
    ReleaseEnvironmentModels();
    const std::wstring catalogPath = ResolvePieceHierarchyPath(Utf8ToWide(kEnvironmentModelCatalogPath));
    std::ifstream input(catalogPath, std::ios::binary);
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!input)
    {
        SetMessage(u8"環境モデルは未登録です");
        return;
    }

    std::string line;
    // 継続条件を満たす間、対象処理を繰り返します。
    while (std::getline(input, line))
    {
        // 条件に該当する場合は、後続処理に必要な値を準備します。
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        EnvironmentModelAsset asset;
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (!(row >> std::quoted(asset.id) >> std::quoted(asset.name) >> std::quoted(asset.path)
            >> asset.defaultScale.x >> asset.defaultScale.y >> asset.defaultScale.z))
        {
            continue;
        }
        const std::wstring modelPath = ResolvePieceHierarchyPath(Utf8ToWide(asset.path));
        const std::string modelPathUtf8 = WideToUtf8(modelPath);
        asset.model = new Model();
        // 条件に該当する場合は、不要になったリソースを解放します。
        if (!asset.model->Load(modelPathUtf8.c_str(), 1.0f, Model::ZFlip))
        {
            SAFE_DELETE(asset.model);
            continue;
        }
        UpdateEnvironmentModelBounds(asset);
        m_environmentModels.push_back(asset);
    }
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!m_environmentModels.empty()) m_selectedEnvironmentModelIndex = 0;
}

bool SceneNarakuPieceEditor::SaveEnvironmentModelCatalog()
{
    EDITOR_PROFILE_FUNCTION();
    const std::wstring catalogPath = ResolvePieceHierarchyPath(Utf8ToWide(kEnvironmentModelCatalogPath));
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!EnsureDirectoryExists(GetDirectoryPart(catalogPath)))
    {
        SetMessage(u8"環境モデル登録簿の保存先を作成できませんでした");
        return false;
    }
    std::ofstream output(catalogPath, std::ios::binary | std::ios::trunc);
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!output)
    {
        SetMessage(u8"環境モデル登録簿を保存できませんでした");
        return false;
    }
    output << "# id name path scaleX scaleY scaleZ\n";
    // 対象コレクションの各要素を順に処理します。
    for (const EnvironmentModelAsset& asset : m_environmentModels)
    {
        output << std::quoted(asset.id) << ' '
            << std::quoted(asset.name) << ' '
            << std::quoted(asset.path) << ' '
            << asset.defaultScale.x << ' '
            << asset.defaultScale.y << ' '
            << asset.defaultScale.z << '\n';
    }
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!output.good())
    {
        SetMessage(u8"環境モデル登録簿を書き込めませんでした");
        return false;
    }
    return true;
}

int SceneNarakuPieceEditor::FindEnvironmentModelIndexById(const std::string& modelId) const
{
    EDITOR_PROFILE_FUNCTION();
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (size_t index = 0; index < m_environmentModels.size(); ++index)
    {
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (m_environmentModels[index].id == modelId) return static_cast<int>(index);
    }
    return -1;
}

int SceneNarakuPieceEditor::FindEnvironmentObjectIndexByCell(int cellX, int cellZ) const
{
    EDITOR_PROFILE_FUNCTION();
    // 指定した範囲を順に走査し、対象要素を処理します。
    for (size_t index = 0; index < m_piece.environmentObjects.size(); ++index)
    {
        const NarakuPiece::EnvironmentObjectData& object = m_piece.environmentObjects[index];
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (object.cell.x == cellX && object.cell.z == cellZ) return static_cast<int>(index);
    }
    return -1;
}

bool SceneNarakuPieceEditor::HasEnvironmentObjectAt(int cellX, int cellZ) const
{
    EDITOR_PROFILE_FUNCTION();
    return FindEnvironmentObjectIndexByCell(cellX, cellZ) >= 0;
}

bool SceneNarakuPieceEditor::CanPlaceEnvironmentObject(int cellX, int cellZ, std::string& outMessage) const
{
    EDITOR_PROFILE_FUNCTION();
    const NarakuPiece::CellData* cell = GetCellData(cellX, cellZ);
    // 条件に該当する場合は、`outMessage` の状態を更新します。
    if (cell == nullptr || cell->deleted)
    {
        outMessage = u8"削除セルまたは範囲外には配置できません";
        return false;
    }
    // 条件に該当する場合は、`outMessage` の状態を更新します。
    if (HasEnvironmentObjectAt(cellX, cellZ))
    {
        outMessage = u8"このセルには環境オブジェクトが配置済みです";
        return false;
    }
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (FindMiningPointIndexByCell(cellX, cellZ) >= 0 ||
        (m_piece.startReturnCandidate.enabled && m_piece.startReturnCandidate.cell.x == cellX && m_piece.startReturnCandidate.cell.z == cellZ) ||
        (m_piece.layerTransition.loadPointEnabled && m_piece.layerTransition.loadPoint.x == cellX && m_piece.layerTransition.loadPoint.z == cellZ))
    {
        outMessage = u8"ロープ以外のゲームオブジェクトと同じセルには配置できません";
        return false;
    }
    return true;
}

void SceneNarakuPieceEditor::OpenNewEnvironmentModelDialog()
{
    EDITOR_PROFILE_FUNCTION();
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW dialog = {};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetPreviewHostWindow();
    dialog.lpstrFile = filePath;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"3D Model Files\0*.fbx;*.obj;*.gltf;*.glb;*.pmx;*.pmd\0All Files\0*.*\0";
    dialog.nFilterIndex = 1;
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (!GetOpenFileNameW(&dialog)) return;

    const std::wstring normalizedPath = NormalizePieceHierarchyPath(filePath);
    const std::string path = WideToUtf8(normalizedPath);
    std::string name = path;
    const size_t slash = name.find_last_of('/');
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (slash != std::string::npos) name = name.substr(slash + 1);
    const size_t dot = name.find_last_of('.');
    // 条件に該当する場合は、`ReleaseEnvironmentModelPopupPreview` の処理を実行します。
    if (dot != std::string::npos) name.resize(dot);

    ReleaseEnvironmentModelPopupPreview();
    const std::string resolvedPath = WideToUtf8(ResolvePieceHierarchyPath(normalizedPath));
    m_environmentModelPopupPreviewModel = new Model();
    // 条件に該当する場合は、不要になったリソースを解放します。
    if (!m_environmentModelPopupPreviewModel->Load(resolvedPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(m_environmentModelPopupPreviewModel);
    }
    else
    {
        EnvironmentModelAsset previewAsset;
        previewAsset.model = m_environmentModelPopupPreviewModel;
        UpdateEnvironmentModelBounds(previewAsset);
        m_environmentModelPopupBoundsMin = previewAsset.boundsMin;
        m_environmentModelPopupBoundsMax = previewAsset.boundsMax;
        m_environmentModelPopupPreviewAnchor = previewAsset.previewAnchor;
    }

    std::snprintf(m_environmentModelNameInput.data(), m_environmentModelNameInput.size(), "%s", name.c_str());
    std::snprintf(m_environmentModelPathInput.data(), m_environmentModelPathInput.size(), "%s", path.c_str());
    m_environmentModelScaleInput = { 1.0f, 1.0f, 1.0f };
    m_environmentModelPopupIsNew = true;
    m_requestOpenEnvironmentModelPopup = true;
}

void SceneNarakuPieceEditor::OpenEnvironmentModelSetting()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size()))
    {
        SetMessage(u8"設定するモデルをAssetsから選択してください");
        return;
    }
    ReleaseEnvironmentModelPopupPreview();
    const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
    std::snprintf(m_environmentModelNameInput.data(), m_environmentModelNameInput.size(), "%s", asset.name.c_str());
    std::snprintf(m_environmentModelPathInput.data(), m_environmentModelPathInput.size(), "%s", asset.path.c_str());
    m_environmentModelScaleInput = asset.defaultScale;
    m_environmentModelPopupIsNew = false;
    m_requestOpenEnvironmentModelPopup = true;
}

void SceneNarakuPieceEditor::DeleteSelectedEnvironmentModel()
{
    EDITOR_PROFILE_FUNCTION();
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size()))
    {
        SetMessage(u8"削除するモデルをAssetsから選択してください");
        return;
    }
    const std::string modelId = m_environmentModels[m_selectedEnvironmentModelIndex].id;
    const bool inUse = std::any_of(m_piece.environmentObjects.begin(), m_piece.environmentObjects.end(),
        [&](const NarakuPiece::EnvironmentObjectData& object) { return object.modelId == modelId; });
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (inUse)
    {
        SetMessage(u8"現在の小ステージで使用中のモデルは削除できません");
        return;
    }
    const int removedIndex = m_selectedEnvironmentModelIndex;
    EnvironmentModelAsset removedAsset = m_environmentModels[removedIndex];
    m_environmentModels.erase(m_environmentModels.begin() + removedIndex);
    m_selectedEnvironmentModelIndex = m_environmentModels.empty()
        ? -1 : std::min(removedIndex, static_cast<int>(m_environmentModels.size()) - 1);
    // 条件に該当する場合は、対応する編集処理を実行します。
    if (!SaveEnvironmentModelCatalog())
    {
        m_environmentModels.insert(m_environmentModels.begin() + removedIndex, removedAsset);
        m_selectedEnvironmentModelIndex = removedIndex;
        return;
    }
    SAFE_DELETE(removedAsset.model);
    SAFE_DELETE(removedAsset.thumbnailDepthStencil);
    SAFE_DELETE(removedAsset.thumbnailRenderTarget);
    SetMessage(u8"環境モデルの登録を削除しました");
}

void SceneNarakuPieceEditor::ApplyEnvironmentModelPopup()
{
    EDITOR_PROFILE_FUNCTION();
    const std::string name = m_environmentModelNameInput.data();
    const std::string path = m_environmentModelPathInput.data();
    // 必須入力が不足している場合は登録内容を変更しません。
    if (!IsEnvironmentModelPopupInputValid(name, path))
    {
        SetMessage(u8"モデル名、パス、0より大きいサイズが必要です");
        return;
    }

    const int previousSelectedIndex = m_selectedEnvironmentModelIndex;
    std::string previousName;
    XMFLOAT3 previousScale = {};
    bool previousThumbnailDirty = false;
    const bool applied = m_environmentModelPopupIsNew
        ? AddEnvironmentModelFromPopup(name, path)
        : UpdateEnvironmentModelFromPopup(
            name, previousName, previousScale, previousThumbnailDirty);
    // モデル読込または選択状態が無効ならポップアップを開いたままにします。
    if (!applied)
    {
        return;
    }
    // カタログ保存に失敗した場合はメモリ上の変更も適用前へ戻します。
    if (!SaveEnvironmentModelCatalog())
    {
        RollbackEnvironmentModelPopup(
            previousSelectedIndex,
            previousName,
            previousScale,
            previousThumbnailDirty);
        return;
    }

    ImGui::CloseCurrentPopup();
    SetMessage(m_environmentModelPopupIsNew ? u8"環境モデルを登録しました" : u8"モデル設定を更新しました");
}

bool SceneNarakuPieceEditor::IsEnvironmentModelPopupInputValid(
    const std::string& name,
    const std::string& path) const
{
    EDITOR_PROFILE_FUNCTION();
    return !name.empty() &&
        !path.empty() &&
        m_environmentModelScaleInput.x > 0.0f &&
        m_environmentModelScaleInput.y > 0.0f &&
        m_environmentModelScaleInput.z > 0.0f;
}

bool SceneNarakuPieceEditor::AddEnvironmentModelFromPopup(
    const std::string& name,
    const std::string& path)
{
    EDITOR_PROFILE_FUNCTION();
    Model* const model = AcquireEnvironmentModelPopupModel(path);
    // モデルファイルを読み込めない場合は一覧へ追加しません。
    if (model == nullptr)
    {
        SetMessage(u8"モデルを読み込めませんでした");
        return false;
    }

    EnvironmentModelAsset asset;
    asset.id = GenerateEnvironmentModelId();
    asset.name = name;
    asset.path = path;
    asset.defaultScale = m_environmentModelScaleInput;
    asset.model = model;
    UpdateEnvironmentModelBounds(asset);
    m_environmentModels.push_back(asset);
    m_environmentModelPopupPreviewModel = nullptr;
    m_selectedEnvironmentModelIndex = static_cast<int>(m_environmentModels.size()) - 1;
    return true;
}

bool SceneNarakuPieceEditor::UpdateEnvironmentModelFromPopup(
    const std::string& name,
    std::string& outPreviousName,
    XMFLOAT3& outPreviousScale,
    bool& outPreviousThumbnailDirty)
{
    EDITOR_PROFILE_FUNCTION();
    const bool hasSelectedModel =
        m_selectedEnvironmentModelIndex >= 0 &&
        m_selectedEnvironmentModelIndex < static_cast<int>(m_environmentModels.size());
    // 有効な登録モデルが選択されていない場合は設定を更新しません。
    if (!hasSelectedModel)
    {
        return false;
    }

    EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
    outPreviousName = asset.name;
    outPreviousScale = asset.defaultScale;
    outPreviousThumbnailDirty = asset.thumbnailDirty;
    asset.name = name;
    asset.defaultScale = m_environmentModelScaleInput;
    asset.thumbnailDirty = true;
    return true;
}

void SceneNarakuPieceEditor::RollbackEnvironmentModelPopup(
    int previousSelectedIndex,
    const std::string& previousName,
    const XMFLOAT3& previousScale,
    bool previousThumbnailDirty)
{
    EDITOR_PROFILE_FUNCTION();
    // 新規登録時は追加した末尾要素を取り除き、プレビューモデルの所有権を戻します。
    if (m_environmentModelPopupIsNew)
    {
        EnvironmentModelAsset& addedAsset = m_environmentModels.back();
        m_environmentModelPopupPreviewModel = addedAsset.model;
        addedAsset.model = nullptr;
        SAFE_DELETE(addedAsset.thumbnailDepthStencil);
        SAFE_DELETE(addedAsset.thumbnailRenderTarget);
        m_environmentModels.pop_back();
        m_selectedEnvironmentModelIndex = previousSelectedIndex;
        return;
    }

    EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
    asset.name = previousName;
    asset.defaultScale = previousScale;
    asset.thumbnailDirty = previousThumbnailDirty;
}

Model* SceneNarakuPieceEditor::AcquireEnvironmentModelPopupModel(const std::string& path)
{
    EDITOR_PROFILE_FUNCTION();
    // 既にプレビューで読み込んだモデルがあれば同じインスタンスを登録へ引き継ぎます。
    if (m_environmentModelPopupPreviewModel != nullptr)
    {
        return m_environmentModelPopupPreviewModel;
    }

    const std::string resolvedPath = WideToUtf8(ResolvePieceHierarchyPath(Utf8ToWide(path)));
    Model* model = new Model();
    // モデル読込に失敗したインスタンスは呼び出し元へ渡しません。
    if (!model->Load(resolvedPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(model);
        return nullptr;
    }
    return model;
}

std::string SceneNarakuPieceEditor::GenerateEnvironmentModelId() const
{
    EDITOR_PROFILE_FUNCTION();
    unsigned int suffix = 0;
    std::string id;
    // 同じ秒に複数登録しても衝突しないIDが得られるまで連番を進めます。
    do
    {
        char buffer[64] = {};
        std::snprintf(
            buffer,
            sizeof(buffer),
            "environment_model_%lld_%u",
            static_cast<long long>(std::time(nullptr)),
            suffix++);
        id = buffer;
    } while (FindEnvironmentModelIndexById(id) >= 0);
    return id;
}

void SceneNarakuPieceEditor::DrawEnvironmentObjects3D() const
{
    EDITOR_PROFILE_FUNCTION();
    const float cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 eye =
    {
        m_cameraTarget.x + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance
    };

    // 指定した範囲を順に走査し、対象要素を処理します。
    for (size_t index = 0; index < m_piece.environmentObjects.size(); ++index)
    {
        const NarakuPiece::EnvironmentObjectData& object = m_piece.environmentObjects[index];
        const int assetIndex = FindEnvironmentModelIndexById(object.modelId);
        // 条件に該当する場合は、対応する編集処理を実行します。
        if (assetIndex < 0 || !IsValidCell(object.cell.x, object.cell.z)) continue;
        const EnvironmentModelAsset& asset = m_environmentModels[assetIndex];
        // 条件に該当する場合は、後続処理に必要な値を準備します。
        if (asset.model == nullptr) continue;

        const XMFLOAT3 center = GetCellWorldPosition(object.cell.x, object.cell.z);
        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(-asset.previewAnchor.x, -asset.previewAnchor.y, -asset.previewAnchor.z) *
            XMMatrixScaling(object.scaleX, object.scaleY, object.scaleZ) *
            XMMatrixTranslation(center.x, center.y, center.z)));
        XMStoreFloat4x4(&wvp[1], XMMatrixTranspose(XMLoadFloat4x4(&m_viewMatrix)));
        XMStoreFloat4x4(&wvp[2], XMMatrixTranspose(XMLoadFloat4x4(&m_projectionMatrix)));
        ShaderList::SetWVP(wvp);
        ShaderList::SetCameraPos(eye);
        asset.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        asset.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        // 指定した範囲を順に走査し、対象要素を処理します。
        for (unsigned int meshIndex = 0; meshIndex < asset.model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = asset.model->GetMesh(meshIndex);
            // 条件に該当する場合は、対応する編集処理を実行します。
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = asset.model->GetMaterial(mesh->materialID);
            // 条件に該当する場合は、対応する編集処理を実行します。
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                ShaderList::SetMaterial(material);
            }
            asset.model->Draw(static_cast<int>(meshIndex));
        }

        // 条件に該当する場合は、`DrawDebugWireBox3D` の処理を実行します。
        if (m_selectedEnvironmentObjectIndex == static_cast<int>(index))
        {
            DrawDebugWireBox3D(
                { center.x, center.y + 0.5f * object.scaleY, center.z },
                { std::max(0.5f, object.scaleX), std::max(0.5f, object.scaleY), std::max(0.5f, object.scaleZ) },
                { 0.25f, 0.85f, 1.0f, 1.0f });
        }
    }
}


void SceneNarakuPieceEditor::UpdateEnvironmentObjectEditing()
{
    EDITOR_PROFILE_FUNCTION();
    ImGuiIO& io = ImGui::GetIO();
    const bool altPressed = IsEditorAltPressed(io);
    const POINT mousePos = GetMousePosition();
    const bool allowPreviewInput = IsMouseInsidePreviewImage() || m_previewImageHovered;

    // 条件に該当する場合は、対応する編集処理を実行します。
    if (allowPreviewInput && PickTerrainCell(mousePos, m_hoverCellX, m_hoverCellZ))
    {
    }
    else
    {
        m_hoverCellX = -1;
        m_hoverCellZ = -1;
    }
    // 条件に該当する場合は、後続処理に必要な値を準備します。
    if (!allowPreviewInput || (io.WantCaptureMouse && !m_previewImageHovered) || altPressed || !IsMouseLeftTrigger()) return;

    int cellX = -1;
    int cellZ = -1;
    // 条件に該当する場合は、`m_selectedEnvironmentObjectIndex` の状態を更新します。
    if (!PickTerrainCell(mousePos, cellX, cellZ))
    {
        m_selectedEnvironmentObjectIndex = -1;
        return;
    }

    const int existingIndex = FindEnvironmentObjectIndexByCell(cellX, cellZ);
    // 条件に該当する場合は、`m_selectedEnvironmentObjectIndex` の状態を更新します。
    if (existingIndex >= 0)
    {
        m_selectedEnvironmentObjectIndex = existingIndex;
        const int modelIndex = FindEnvironmentModelIndexById(m_piece.environmentObjects[existingIndex].modelId);
        // 条件に該当する場合は、`SetMessage` の処理を実行します。
        if (modelIndex >= 0) m_selectedEnvironmentModelIndex = modelIndex;
        SetMessage(u8"環境オブジェクトを選択しました");
        return;
    }
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (m_selectedEnvironmentModelIndex < 0 || m_selectedEnvironmentModelIndex >= static_cast<int>(m_environmentModels.size()))
    {
        SetMessage(u8"Assetsから配置するモデルを選択してください");
        return;
    }

    std::string placeError;
    // 条件に該当する場合は、`SetMessage` の処理を実行します。
    if (!CanPlaceEnvironmentObject(cellX, cellZ, placeError))
    {
        SetMessage(placeError);
        return;
    }

    const EnvironmentModelAsset& asset = m_environmentModels[m_selectedEnvironmentModelIndex];
    PushUndoSnapshot();
    NarakuPiece::EnvironmentObjectData object;
    object.modelId = asset.id;
    object.cell = { cellX, cellZ };
    object.scaleX = asset.defaultScale.x;
    object.scaleY = asset.defaultScale.y;
    object.scaleZ = asset.defaultScale.z;
    m_piece.environmentObjects.push_back(object);
    m_selectedEnvironmentObjectIndex = static_cast<int>(m_piece.environmentObjects.size()) - 1;
    MarkPieceDirty();
    SetMessage(u8"環境オブジェクトを配置しました");
}


