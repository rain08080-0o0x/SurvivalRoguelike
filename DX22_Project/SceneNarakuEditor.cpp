#include "SceneNarakuEditor.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "Texture.h"
#include "imgui.h"
#include <commdlg.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace DirectX;

namespace
{
    // Shared constants for editor timing, picking, and camera behavior.
    const float kDt = 1.0f / fFPS;

    constexpr float kDepthScale = 0.35f;

    constexpr float kPickThresholdPx = 18.0f;

    constexpr float kVertexDragScale = 0.035f;
    constexpr float kCellDragScale = 0.035f;

    constexpr float kMinPitch = 0.15f;

    constexpr float kMaxPitch = 1.35f;

    constexpr float kMinDistance = 6.0f;

    constexpr float kMaxDistance = 120.0f;

    DirectX::XMFLOAT2 GetEditorViewportSize()
    {
        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
        const float width = (displaySize.x > 1.0f) ? displaySize.x : static_cast<float>(SCREEN_WIDTH);
        const float height = (displaySize.y > 1.0f) ? displaySize.y : static_cast<float>(SCREEN_HEIGHT);
        return { width, height };
    }


    // Labels used by the ground texture selector.
    const char* kGroundTextureLabels[] =
{
    u8"草地",
    u8"土",
    u8"岩",
    u8"湿地"
};

    // Convert size_t to int for index-based editor code.
    int ToInt(size_t value)
    {
        return static_cast<int>(value);
    }

    // Clamp helper kept local so the editor does not depend on newer STL helpers.
    template<typename T>
    T ClampValue(const T& value, const T& minValue, const T& maxValue)
    {
        return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
    }

    // Return a 2D vector length in screen space.
    float Length2D(float x, float y)
    {
        return std::sqrt(x * x + y * y);
    }
}


    // Extract the parent directory portion from an absolute file path.
    std::wstring GetDirectoryPart(const wchar_t* path)
    {
        if (path == nullptr || path[0] == L'\0')
        {
            return {};
        }

        const std::wstring fullPath(path);
        const size_t slashPos = fullPath.find_last_of(L"\\/");
        return (slashPos == std::wstring::npos) ? std::wstring() : fullPath.substr(0, slashPos);
    }

// Build editor-only resources and load the current map.
SceneNarakuEditor::SceneNarakuEditor()
{

    m_debugWhiteTexture = new Texture();

    const unsigned int whitePixel = 0xffffffff;

    m_debugWhiteTexture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &whitePixel);
    InitializeMap();
}

// Release editor-only temporary resources.
SceneNarakuEditor::~SceneNarakuEditor()
{
    SAFE_DELETE(m_debugWhiteTexture);
}

// Load the current map and normalize the initial selection state.
void SceneNarakuEditor::InitializeMap()
{
    LoadCurrentMap();
    EnsureSelectionValid();
    FocusSelection();
}

// Advance camera input, picking, and edit interactions for one frame.
void SceneNarakuEditor::Update()
{
    HandleShortcuts();
    UpdateCamera(kDt);
    ApplyCameraMatrices();
    UpdateSelectionAndEditing();

    if (IsRawKeyTrigger('F'))
    {
        FocusSelection();
    }
}

// Render the 3D preview first, then draw the editor UI.
void SceneNarakuEditor::Draw()
{
    ApplyCameraMatrices();

    DrawWorld();

    DrawEditorUi();
    DrawMarqueeOverlay();
}

// Update the Unity-style editor camera from mouse and keyboard input.
void SceneNarakuEditor::UpdateCamera(float dt)
{
    ImGuiIO& io = ImGui::GetIO();

    const bool allowMouseCamera = !io.WantCaptureMouse;
    const bool allowKeyboardCamera = !io.WantCaptureKeyboard;

    const POINT mouseDelta = GetMouseDelta();

    if (allowMouseCamera && io.KeyAlt && IsMouseLeftPress())
    {
        m_cameraYaw -= static_cast<float>(mouseDelta.x) * 0.010f;
        m_cameraPitch -= static_cast<float>(mouseDelta.y) * 0.010f;
    }

    m_cameraPitch = ClampValue(m_cameraPitch, kMinPitch, kMaxPitch);

    if (allowMouseCamera && IsMouseMiddlePress())
    {
        const XMVECTOR eye = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMVECTOR target = XMVectorSet(
            std::cos(m_cameraPitch) * std::cos(m_cameraYaw),
            std::sin(m_cameraPitch),
            std::cos(m_cameraPitch) * std::sin(m_cameraYaw),
            0.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, target - eye));
        XMVECTOR cameraUp = XMVector3Normalize(XMVector3Cross(target - eye, right));

        XMFLOAT3 right3 = {};
        XMFLOAT3 up3 = {};
        XMStoreFloat3(&right3, right);
        XMStoreFloat3(&up3, cameraUp);

        const float panScale = std::max(0.05f, m_cameraDistance * 0.0025f);
        const float horizontalDelta = static_cast<float>(mouseDelta.x);
        const float verticalDelta = static_cast<float>(mouseDelta.y);

        m_cameraTarget.x += right3.x * horizontalDelta * panScale;
        m_cameraTarget.y += right3.y * horizontalDelta * panScale;
        m_cameraTarget.z += right3.z * horizontalDelta * panScale;

        m_cameraTarget.x -= up3.x * verticalDelta * panScale;
        m_cameraTarget.y -= up3.y * verticalDelta * panScale;
        m_cameraTarget.z -= up3.z * verticalDelta * panScale;
    }

    const float mouseWheelDelta = GetMouseWheelDelta();
    if (allowMouseCamera && mouseWheelDelta != 0.0f)
    {
        m_cameraDistance -= mouseWheelDelta * std::max(1.0f, m_cameraDistance * 0.10f);
        m_cameraDistance = ClampValue(m_cameraDistance, kMinDistance, kMaxDistance);
    }

    if (allowMouseCamera && allowKeyboardCamera && IsMouseRightPress())
    {
        const float moveSpeed = std::max(4.0f, m_cameraDistance * 0.35f) * dt;
        const float forwardX = std::cos(m_cameraYaw);
        const float forwardZ = std::sin(m_cameraYaw);
        const float rightX = -forwardZ;
        const float rightZ = forwardX;

        if (IsRawKeyPress('W'))
        {
            m_cameraTarget.x -= forwardX * moveSpeed;
            m_cameraTarget.z -= forwardZ * moveSpeed;
        }
        if (IsRawKeyPress('S'))
        {
            m_cameraTarget.x += forwardX * moveSpeed;
            m_cameraTarget.z += forwardZ * moveSpeed;
        }
        if (IsRawKeyPress('D'))
        {
            m_cameraTarget.x += rightX * moveSpeed;
            m_cameraTarget.z += rightZ * moveSpeed;
        }
        if (IsRawKeyPress('A'))
        {
            m_cameraTarget.x -= rightX * moveSpeed;
            m_cameraTarget.z -= rightZ * moveSpeed;
        }
        if (IsRawKeyPress('E'))
        {
            m_cameraTarget.y += moveSpeed;
        }
        if (IsRawKeyPress('Q'))
        {
            m_cameraTarget.y -= moveSpeed;
        }
    }
}

// Handle picking, gizmo drags, and mode-specific edit input.
void SceneNarakuEditor::UpdateSelectionAndEditing()
{
    EnsureSelectionValid();

    if (ImGui::GetIO().WantCaptureMouse)
    {
        m_draggingVertexHeight = false;
        m_draggingCellHeight = false;
        m_draggingRopePosition = false;
        m_draggingMiningPointPosition = false;
        m_draggingLayerPointPosition = false;
        m_marqueeSelecting = false;
        return;
    }

    const POINT mousePos = GetMousePosition();
    const POINT mouseDelta = GetMouseDelta();
    const bool shiftPressed = IsRawKeyPress(VK_SHIFT) || IsRawKeyPress(VK_LSHIFT) || IsRawKeyPress(VK_RSHIFT);

    if (m_editMode == EditMode::Terrain && !ImGui::GetIO().KeyAlt)
    {
        if (m_marqueeSelecting)
        {
            m_marqueeEnd = mousePos;
            if (!IsMouseLeftPress())
            {
                ApplyMarqueeSelection();
                m_marqueeSelecting = false;
            }
            return;
        }

        if (shiftPressed && IsMouseLeftTrigger())
        {
            m_marqueeSelecting = true;
            m_marqueeStart = mousePos;
            m_marqueeEnd = mousePos;
            return;
        }
    }

    if (m_editMode == EditMode::Rope)
    {
        if (m_draggingRopePosition)
        {
            if (IsMouseLeftPress() &&
                m_selectedRope >= 0 &&
                m_selectedRope < ToInt(m_mapData.ropes.size()))
            {
                DragPointOnLayerPlane(m_mapData.ropes[m_selectedRope].xz, m_mapData.ropes[m_selectedRope].topLayerId);
                return;
            }
            m_draggingRopePosition = false;
        }
    }

    if (m_editMode == EditMode::MiningPoint)
    {
        if (m_draggingMiningPointPosition)
        {
            if (IsMouseLeftPress() &&
                m_selectedMiningPoint >= 0 &&
                m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
            {
                DragPointOnLayerPlane(m_mapData.miningPoints[m_selectedMiningPoint].xz, m_mapData.miningPoints[m_selectedMiningPoint].layerId);
                return;
            }
            m_draggingMiningPointPosition = false;
        }
    }

    if (m_draggingLayerPointPosition)
    {
        if (IsMouseLeftPress())
        {
            if (NarakuMap::LayerPoint* point = GetSelectedLayerPoint())
            {
                DragPointOnLayerPlane(point->xz, point->layerId);
                return;
            }
        }
        m_draggingLayerPointPosition = false;
    }

    if (m_editMode == EditMode::Terrain)
    {
        if (m_terrainEditTarget == TerrainEditTarget::Vertex &&
            m_selectedVertex.layerIndex >= 0 &&
            m_draggingVertexHeight &&
            IsMouseLeftPress())
        {
            ApplyHeightDeltaToSelectedVertices(-static_cast<float>(mouseDelta.y) * kVertexDragScale);
            return;
        }

        if (m_terrainEditTarget == TerrainEditTarget::Face &&
            m_selectedCell.layerIndex >= 0 &&
            m_draggingCellHeight &&
            IsMouseLeftPress())
        {
            ApplyHeightDeltaToSelectedCells(-static_cast<float>(mouseDelta.y) * kCellDragScale);
            return;
        }

        if (!IsMouseLeftPress())
        {
            m_draggingVertexHeight = false;
            m_draggingCellHeight = false;
        }
    }

    if (!IsMouseLeftTrigger())
    {
        return;
    }

    if (ImGui::GetIO().KeyAlt)
    {
        return;
    }

    if (m_armLayerPointPlacement && m_selectedLayerPoint != LayerPointSelectionTarget::None)
    {
        if (NarakuMap::LayerPoint* point = GetSelectedLayerPoint())
        {
            PushUndoSnapshot();
            m_draggingLayerPointPosition = true;
            m_armLayerPointPlacement = false;
            DragPointOnLayerPlane(point->xz, point->layerId);
            if (m_autoFocusSelection)
            {
                FocusSelection();
            }
            return;
        }
        m_armLayerPointPlacement = false;
    }

    LayerPointSelectionTarget pickedLayerPoint = LayerPointSelectionTarget::None;
    if (PickLayerPoint(mousePos, pickedLayerPoint))
    {
        if (pickedLayerPoint == m_selectedLayerPoint)
        {
            if (NarakuMap::LayerPoint* point = GetSelectedLayerPoint())
            {
                PushUndoSnapshot();
                m_draggingLayerPointPosition = true;
                m_armLayerPointPlacement = false;
                DragPointOnLayerPlane(point->xz, point->layerId);
                return;
            }
        }

        m_selectedLayerPoint = pickedLayerPoint;
        m_armLayerPointPlacement = false;
        if (m_autoFocusSelection)
        {
            FocusSelection();
        }
        return;
    }

    if (m_editMode == EditMode::Terrain &&
        m_terrainEditTarget == TerrainEditTarget::Vertex &&
        IsMouseNearSelectedVertexHandle(mousePos))
    {
        PushUndoSnapshot();
        m_draggingVertexHeight = true;
        return;
    }

    if (m_editMode == EditMode::Terrain &&
        m_terrainEditTarget == TerrainEditTarget::Face &&
        IsMouseNearSelectedCellHandle(mousePos))
    {
        PushUndoSnapshot();
        m_draggingCellHeight = true;
        return;
    }


    if (m_editMode == EditMode::Terrain)
    {
        if (m_terrainEditTarget == TerrainEditTarget::Vertex)
        {
            int gridX = -1;
            int gridZ = -1;
            if (PickTerrainVertex(mousePos, gridX, gridZ))
            {
                ClearMultiSelection();
                m_selectedLayerPoint = LayerPointSelectionTarget::None;
                m_armLayerPointPlacement = false;
                m_selectedVertex.layerIndex = m_selectedLayer;
                m_selectedVertex.gridX = gridX;
                m_selectedVertex.gridZ = gridZ;
                m_selectedCell.layerIndex = m_selectedLayer;
                m_selectedCell.cellX = ClampValue(gridX, 0, std::max(0, m_mapData.terrainLayers[m_selectedLayer].gridWidth - 2));
                m_selectedCell.cellZ = ClampValue(gridZ, 0, std::max(0, m_mapData.terrainLayers[m_selectedLayer].gridHeight - 2));
                AddSelectedVertex(m_selectedLayer, gridX, gridZ);
                if (m_autoFocusSelection)
                {
                    FocusSelection();
                }
            }
        }
        else
        {
            int cellX = -1;
            int cellZ = -1;
            if (PickTerrainCell(mousePos, cellX, cellZ))
            {
                ClearMultiSelection();
                m_selectedLayerPoint = LayerPointSelectionTarget::None;
                m_armLayerPointPlacement = false;
                m_selectedCell.layerIndex = m_selectedLayer;
                m_selectedCell.cellX = cellX;
                m_selectedCell.cellZ = cellZ;
                m_selectedVertex.layerIndex = m_selectedLayer;
                m_selectedVertex.gridX = cellX;
                m_selectedVertex.gridZ = cellZ;
                AddSelectedCell(m_selectedLayer, cellX, cellZ);
                if (m_autoFocusSelection)
                {
                    FocusSelection();
                }
            }
        }
        return;
    }

    if (m_editMode == EditMode::Rope)
    {
        const int pickedRope = PickRope(mousePos);
        if (pickedRope >= 0)
        {
            m_editMode = EditMode::Rope;
            if (shiftPressed)
            {
                AddSelectedRope(pickedRope);
                m_selectedRope = pickedRope;
                m_selectedLayerPoint = LayerPointSelectionTarget::None;
                m_armLayerPointPlacement = false;
                if (m_autoFocusSelection)
                {
                    FocusSelection();
                }
                return;
            }

            if (pickedRope == m_selectedRope)
            {
                if (m_multiSelectedRopes.empty())
                {
                    AddSelectedRope(m_selectedRope);
                }
                PushUndoSnapshot();
                m_draggingRopePosition = true;
                DragPointOnLayerPlane(m_mapData.ropes[m_selectedRope].xz, m_mapData.ropes[m_selectedRope].topLayerId);
                return;
            }

            ClearMultiSelection();
            AddSelectedRope(pickedRope);
            m_selectedRope = pickedRope;
            m_selectedLayerPoint = LayerPointSelectionTarget::None;
            m_armLayerPointPlacement = false;
            if (m_autoFocusSelection)
            {
                FocusSelection();
            }
        }
        return;
    }

    if (m_editMode == EditMode::MiningPoint)
    {
        const int pickedPoint = PickMiningPoint(mousePos);
        if (pickedPoint >= 0)
        {
            m_editMode = EditMode::MiningPoint;
            if (shiftPressed)
            {
                AddSelectedMiningPoint(pickedPoint);
                m_selectedMiningPoint = pickedPoint;
                m_selectedLayerPoint = LayerPointSelectionTarget::None;
                m_armLayerPointPlacement = false;
                if (m_autoFocusSelection)
                {
                    FocusSelection();
                }
                return;
            }

            if (pickedPoint == m_selectedMiningPoint)
            {
                if (m_multiSelectedMiningPoints.empty())
                {
                    AddSelectedMiningPoint(m_selectedMiningPoint);
                }
                PushUndoSnapshot();
                m_draggingMiningPointPosition = true;
                DragPointOnLayerPlane(m_mapData.miningPoints[m_selectedMiningPoint].xz, m_mapData.miningPoints[m_selectedMiningPoint].layerId);
                return;
            }

            ClearMultiSelection();
            AddSelectedMiningPoint(pickedPoint);
            m_selectedMiningPoint = pickedPoint;
            m_selectedLayerPoint = LayerPointSelectionTarget::None;
            m_armLayerPointPlacement = false;
            if (m_autoFocusSelection)
            {
                FocusSelection();
            }
        }
    }
}

// Move the camera focus target to the currently selected object.
void SceneNarakuEditor::FocusSelection()
{

    if (m_selectedLayerPoint != LayerPointSelectionTarget::None)
    {
        const NarakuMap::LayerPoint* point = GetSelectedLayerPoint();
        if (point != nullptr)
        {
            const int layerIndex = FindLayerIndexById(point->layerId);
            const float depth = (layerIndex >= 0) ? m_mapData.terrainLayers[layerIndex].layerDepth : 0.0f;
            m_cameraTarget = XMFLOAT3(point->xz.x, ToWorldY(depth, 0.5f), point->xz.z);
            return;
        }
    }

    if (m_editMode == EditMode::Terrain &&
        m_terrainEditTarget == TerrainEditTarget::Vertex &&
        m_selectedVertex.layerIndex >= 0 &&
        m_selectedVertex.layerIndex < ToInt(m_mapData.terrainLayers.size()))
    {
        const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
        const XMFLOAT3 world = GetVertexWorldPosition(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
        m_cameraTarget = world;
        return;
    }

    if (m_editMode == EditMode::Terrain &&
        m_terrainEditTarget == TerrainEditTarget::Face &&
        m_selectedCell.layerIndex >= 0 &&
        m_selectedCell.layerIndex < ToInt(m_mapData.terrainLayers.size()))
    {
        const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedCell.layerIndex];
        int cellX = 0;
        int cellZ = 0;
        GetSelectedCellCoords(layer, cellX, cellZ);
        const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
        m_cameraTarget = XMFLOAT3(
            (p00.x + p10.x + p01.x + p11.x) * 0.25f,
            (p00.y + p10.y + p01.y + p11.y) * 0.25f,
            (p00.z + p10.z + p01.z + p11.z) * 0.25f);
        return;
    }

    if (m_editMode == EditMode::Rope &&
        m_selectedRope >= 0 &&
        m_selectedRope < ToInt(m_mapData.ropes.size()))
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[m_selectedRope];
        const int topIndex = FindLayerIndexById(rope.topLayerId);
        const int bottomIndex = FindLayerIndexById(rope.bottomLayerId);
        const float topDepth = (topIndex >= 0) ? m_mapData.terrainLayers[topIndex].layerDepth : 0.0f;
        const float bottomDepth = (bottomIndex >= 0) ? m_mapData.terrainLayers[bottomIndex].layerDepth : topDepth;
        m_cameraTarget = XMFLOAT3(rope.xz.x, ToWorldY((topDepth + bottomDepth) * 0.5f, 0.8f), rope.xz.z);
        return;
    }

    if (m_editMode == EditMode::MiningPoint &&
        m_selectedMiningPoint >= 0 &&
        m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[m_selectedMiningPoint];
        const int layerIndex = FindLayerIndexById(point.layerId);
        const float depth = (layerIndex >= 0) ? m_mapData.terrainLayers[layerIndex].layerDepth : 0.0f;
        m_cameraTarget = XMFLOAT3(point.xz.x, ToWorldY(depth, 0.5f), point.xz.z);
        return;
    }

    m_cameraTarget = { 0.0f, 0.0f, 0.0f };
}

// Build an unused path for a new map file in the default map directory.
std::wstring SceneNarakuEditor::MakeUniqueUntitledMapPath() const
{
    const std::wstring baseDirectory = GetDirectoryPart(NarakuMap::GetDefaultMapPath());
    if (baseDirectory.empty())
    {
        return L"Untitled_01.json";
    }

    for (int index = 1; index <= 999; ++index)
    {
        wchar_t fileName[64] = {};
        swprintf_s(fileName, L"Untitled_%02d.json", index);
        const std::wstring candidate = baseDirectory + L"\\" + fileName;
        if (GetFileAttributesW(candidate.c_str()) == INVALID_FILE_ATTRIBUTES)
        {
            return candidate;
        }
    }

    return baseDirectory + L"\\Untitled_Overflow.json";
}

// Open a Win32 file dialog and return the chosen map path.
bool SceneNarakuEditor::PromptMapPath(bool saveDialog, std::wstring& outPath) const
{
    wchar_t filePath[MAX_PATH] = {};
    const wchar_t* currentPath = NarakuMap::GetCurrentMapPath();
    if (currentPath != nullptr && currentPath[0] != L'\0')
    {
        wcsncpy_s(filePath, currentPath, _TRUNCATE);
    }

    const std::wstring initialDirectory = GetDirectoryPart((currentPath != nullptr && currentPath[0] != L'\0')
        ? currentPath
        : NarakuMap::GetDefaultMapPath());

    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = L"JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filePath;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initialDirectory.empty() ? nullptr : initialDirectory.c_str();
    ofn.lpstrDefExt = L"json";
    ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    ofn.Flags |= saveDialog
        ? (OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT)
        : (OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST);

    const BOOL dialogResult = saveDialog ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (!dialogResult)
    {
        return false;
    }

    outPath = filePath;
    return true;
}

// Save the current map data to the active JSON file.
bool SceneNarakuEditor::SaveCurrentMap()
{
    std::string errorMessage;
    const wchar_t* savePath = NarakuMap::GetCurrentMapPath();
    if (NarakuMap::SaveMap(savePath, m_mapData, &errorMessage))
    {
        m_lastIoMessage = u8"保存しました: " + WideToUtf8(savePath);
        AppendOperationLog(m_lastIoMessage);
        return true;
    }

    m_lastIoMessage = u8"保存失敗: " + errorMessage;
    AppendOperationLog(m_lastIoMessage);
    return false;
}

// Choose a new path and save the current map there.
bool SceneNarakuEditor::SaveCurrentMapAs()
{
    std::wstring selectedPath;
    if (!PromptMapPath(true, selectedPath))
    {
        m_lastIoMessage = u8"別名保存をキャンセルしました";
        AppendOperationLog(m_lastIoMessage);
        return false;
    }

    NarakuMap::SetCurrentMapPath(selectedPath.c_str());
    return SaveCurrentMap();
}

// Load the active JSON file or regenerate a default map when missing.
bool SceneNarakuEditor::LoadCurrentMap()
{
    std::string errorMessage;
    if (NarakuMap::LoadMap(NarakuMap::GetCurrentMapPath(), m_mapData, &errorMessage))
    {
        EnsureSelectionValid();
        m_undoStack.clear();
        m_redoStack.clear();
        InvalidateValidationCache();
        m_lastIoMessage = u8"読込しました: " + WideToUtf8(NarakuMap::GetCurrentMapPath());
        AppendOperationLog(m_lastIoMessage);
        return true;
    }

    m_mapData = NarakuMap::CreateDefaultMap();
    for (NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
    {
        NarakuMap::EnsureLayerHeights(layer);
        NarakuMap::EnsureLayerVertexEnabled(layer);
        NarakuMap::EnsureLayerCellAttributes(layer);
    }

    std::string saveError;
    if (NarakuMap::SaveMap(NarakuMap::GetCurrentMapPath(), m_mapData, &saveError))
    {
        EnsureSelectionValid();
        m_undoStack.clear();
        m_redoStack.clear();
        InvalidateValidationCache();
        m_lastIoMessage = u8"初期マップを作成しました: " + WideToUtf8(NarakuMap::GetCurrentMapPath());
        AppendOperationLog(m_lastIoMessage);
        return true;
    }

    m_lastIoMessage = u8"読込失敗: " + errorMessage + u8" / 保存失敗: " + saveError;
    AppendOperationLog(m_lastIoMessage);
    return false;
}

// Select a map file and load it into the editor.
bool SceneNarakuEditor::OpenMapByDialog()
{
    std::wstring selectedPath;
    if (!PromptMapPath(false, selectedPath))
    {
        m_lastIoMessage = u8"読込をキャンセルしました";
        AppendOperationLog(m_lastIoMessage);
        return false;
    }

    NarakuMap::SetCurrentMapPath(selectedPath.c_str());
    const bool loaded = LoadCurrentMap();
    if (loaded)
    {
        FocusSelection();
    }
    return loaded;
}

// Create a new map from the default template and assign an untitled path.
void SceneNarakuEditor::CreateNewMap()
{
    NarakuMap::SetCurrentMapPath(MakeUniqueUntitledMapPath().c_str());
    m_mapData = NarakuMap::CreateDefaultMap();
    for (NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
    {
        NarakuMap::EnsureLayerHeights(layer);
        NarakuMap::EnsureLayerVertexEnabled(layer);
        NarakuMap::EnsureLayerCellAttributes(layer);
    }

    EnsureSelectionValid();
    m_undoStack.clear();
    m_redoStack.clear();
    InvalidateValidationCache();
    m_lastIoMessage = u8"新規マップを作成しました。必要なら別名保存してください: " + WideToUtf8(NarakuMap::GetCurrentMapPath());
    AppendOperationLog(m_lastIoMessage);
    FocusSelection();
}

// Clamp all selection indices and repair per-layer height storage.
void SceneNarakuEditor::EnsureSelectionValid()
{
    if (m_mapData.terrainLayers.empty())
    {
        m_selectedLayer = -1;
        m_selectedRope = -1;
        m_selectedMiningPoint = -1;
        m_selectedVertex = {};
        m_selectedCell = {};
        ClearMultiSelection();
        return;
    }

    for (NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
    {
        NarakuMap::EnsureLayerHeights(layer);
        NarakuMap::EnsureLayerCellAttributes(layer);
    }

    m_selectedLayer = ClampValue(m_selectedLayer, 0, ToInt(m_mapData.terrainLayers.size()) - 1);

    const int fallbackLayerId = m_mapData.terrainLayers[m_selectedLayer].id;
    if (NarakuMap::FindLayerIndexById(m_mapData, m_mapData.playerStartPoint.layerId) < 0)
    {
        m_mapData.playerStartPoint.layerId = fallbackLayerId;
    }
    if (NarakuMap::FindLayerIndexById(m_mapData, m_mapData.returnPoint.layerId) < 0)
    {
        m_mapData.returnPoint.layerId = fallbackLayerId;
    }

    if (m_mapData.ropes.empty())
    {
        m_selectedRope = -1;
        m_multiSelectedRopes.clear();
    }
    else
    {
        m_selectedRope = ClampValue(m_selectedRope, -1, ToInt(m_mapData.ropes.size()) - 1);
        m_multiSelectedRopes.erase(
            std::remove_if(
                m_multiSelectedRopes.begin(),
                m_multiSelectedRopes.end(),
                [this](int index)
                {
                    return index < 0 || index >= ToInt(m_mapData.ropes.size());
                }),
            m_multiSelectedRopes.end());
        if (m_selectedRope < 0 && !m_multiSelectedRopes.empty())
        {
            m_selectedRope = m_multiSelectedRopes.front();
        }
    }

    if (m_mapData.miningPoints.empty())
    {
        m_selectedMiningPoint = -1;
        m_multiSelectedMiningPoints.clear();
    }
    else
    {
        m_selectedMiningPoint = ClampValue(m_selectedMiningPoint, -1, ToInt(m_mapData.miningPoints.size()) - 1);
        m_multiSelectedMiningPoints.erase(
            std::remove_if(
                m_multiSelectedMiningPoints.begin(),
                m_multiSelectedMiningPoints.end(),
                [this](int index)
                {
                    return index < 0 || index >= ToInt(m_mapData.miningPoints.size());
                }),
            m_multiSelectedMiningPoints.end());
        if (m_selectedMiningPoint < 0 && !m_multiSelectedMiningPoints.empty())
        {
            m_selectedMiningPoint = m_multiSelectedMiningPoints.front();
        }
    }

    NarakuMap::TerrainLayer& currentLayer = m_mapData.terrainLayers[m_selectedLayer];
    if (m_selectedVertex.layerIndex < 0 ||
        m_selectedVertex.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        m_selectedVertex.layerIndex = m_selectedLayer;
        m_selectedVertex.gridX = 0;
        m_selectedVertex.gridZ = 0;
    }

    if (m_selectedVertex.layerIndex == m_selectedLayer)
    {
        m_selectedVertex.gridX = ClampValue(m_selectedVertex.gridX, 0, std::max(0, currentLayer.gridWidth - 1));
        m_selectedVertex.gridZ = ClampValue(m_selectedVertex.gridZ, 0, std::max(0, currentLayer.gridHeight - 1));
        if (!NarakuMap::IsVertexEnabled(currentLayer, m_selectedVertex.gridX, m_selectedVertex.gridZ))
        {
            bool foundEnabledVertex = false;
            for (int gridZ = 0; gridZ < currentLayer.gridHeight && !foundEnabledVertex; ++gridZ)
            {
                for (int gridX = 0; gridX < currentLayer.gridWidth; ++gridX)
                {
                    if (!NarakuMap::IsVertexEnabled(currentLayer, gridX, gridZ))
                    {
                        continue;
                    }

                    m_selectedVertex.gridX = gridX;
                    m_selectedVertex.gridZ = gridZ;
                    foundEnabledVertex = true;
                    break;
                }
            }
        }
    }

    if (m_selectedCell.layerIndex < 0 ||
        m_selectedCell.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        m_selectedCell.layerIndex = m_selectedLayer;
        m_selectedCell.cellX = 0;
        m_selectedCell.cellZ = 0;
    }

    if (m_selectedCell.layerIndex == m_selectedLayer)
    {
        m_selectedCell.cellX = ClampValue(m_selectedCell.cellX, 0, std::max(0, currentLayer.gridWidth - 2));
        m_selectedCell.cellZ = ClampValue(m_selectedCell.cellZ, 0, std::max(0, currentLayer.gridHeight - 2));
    }

    m_multiSelectedVertices.erase(
        std::remove_if(
            m_multiSelectedVertices.begin(),
            m_multiSelectedVertices.end(),
            [this](const VertexSelection& selection)
            {
                if (selection.layerIndex < 0 || selection.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
                {
                    return true;
                }
                const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[selection.layerIndex];
                return selection.gridX < 0 || selection.gridX >= layer.gridWidth || selection.gridZ < 0 || selection.gridZ >= layer.gridHeight ||
                    !NarakuMap::IsVertexEnabled(layer, selection.gridX, selection.gridZ);
            }),
        m_multiSelectedVertices.end());

    m_multiSelectedCells.erase(
        std::remove_if(
            m_multiSelectedCells.begin(),
            m_multiSelectedCells.end(),
            [this](const CellSelection& selection)
            {
                if (selection.layerIndex < 0 || selection.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
                {
                    return true;
                }
                const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[selection.layerIndex];
                return selection.cellX < 0 || selection.cellX >= std::max(0, layer.gridWidth - 1) ||
                    selection.cellZ < 0 || selection.cellZ >= std::max(0, layer.gridHeight - 1);
            }),
        m_multiSelectedCells.end());
}

// Build and submit view and projection matrices for rendering and picking.
void SceneNarakuEditor::ApplyCameraMatrices()
{

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
    const XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, target, up);

    const DirectX::XMFLOAT2 viewportSize = GetEditorViewportSize();
    const float aspect = viewportSize.x / viewportSize.y;
    const XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), aspect, 0.1f, 1000.0f);

    XMStoreFloat4x4(&m_viewMatrix, viewMatrix);
    XMStoreFloat4x4(&m_projectionMatrix, projectionMatrix);

    XMFLOAT4X4 drawView = {};
    XMFLOAT4X4 drawProjection = {};
    XMStoreFloat4x4(&drawView, XMMatrixTranspose(viewMatrix));
    XMStoreFloat4x4(&drawProjection, XMMatrixTranspose(projectionMatrix));
    Geometory::SetView(drawView);
    Geometory::SetProjection(drawProjection);
    Sprite::SetView(drawView);
    Sprite::SetProjection(drawProjection);
}

// Draw terrain, helpers, ropes, and mining points in the 3D preview.
void SceneNarakuEditor::DrawWorld()
{

    for (int i = 0; i < ToInt(m_mapData.terrainLayers.size()); ++i)
    {
        DrawTerrainLayer(m_mapData.terrainLayers[i], i);
    }

    if (m_editMode == EditMode::Terrain && m_terrainEditTarget == TerrainEditTarget::Vertex && m_showHoveredVertexPreview)
    {
        DrawHoveredVertexHighlight();
    }

    if (m_showRopePreview)
    {
        DrawRopes();
    }

    if (m_showMiningPreview)
    {
        DrawMiningPoints();
    }

    if (m_showStartReturnPreview)
    {
        DrawStartAndReturnPoints();
    }


    if (m_editMode == EditMode::Terrain)
    {
        DrawVertexGizmo();
    }

    Geometory::DrawLines();
}

// Draw every ImGui window that makes up the editor UI.
void SceneNarakuEditor::DrawEditorUi()
{

    DrawMapWindow();

    DrawLayerWindow();

    DrawRopeWindow();

    DrawMiningWindow();

    DrawInspectorWindow();

    DrawFeatureWindow();

    DrawOperationLogWindow();

    DrawHelpWindow();
}

// Draw file IO controls and the top-level edit mode selector.
void SceneNarakuEditor::DrawMapWindow()
{
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 420.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"奈落塔地形エディタ");

    const std::string pathText = WideToUtf8(NarakuMap::GetCurrentMapPath());
    ImGui::TextUnformatted(u8"現在のマップ");
    ImGui::TextWrapped("%s", pathText.c_str());

    if (ImGui::Button(u8"保存", ImVec2(80.0f, 0.0f)))
    {
        SaveCurrentMap();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"別名保存", ImVec2(96.0f, 0.0f)))
    {
        SaveCurrentMapAs();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"開く", ImVec2(72.0f, 0.0f)))
    {
        OpenMapByDialog();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"新規", ImVec2(72.0f, 0.0f)))
    {
        CreateNewMap();
    }

    if (ImGui::Button(u8"既定マップ内容に戻す", ImVec2(180.0f, 0.0f)))
    {
        PushUndoSnapshot();
        m_mapData = NarakuMap::CreateDefaultMap();
        EnsureSelectionValid();
        m_lastIoMessage = u8"既定内容に戻しました";
        AppendOperationLog(m_lastIoMessage);
        FocusSelection();
    }

    ImGui::Separator();
    ImGui::Text(u8"状態: %s", m_lastIoMessage.c_str());
    ImGui::Text(u8"Undo: %d / Redo: %d", ToInt(m_undoStack.size()), ToInt(m_redoStack.size()));
    ImGui::Text(u8"複数選択: 頂点 %d / 面 %d", ToInt(m_multiSelectedVertices.size()), ToInt(m_multiSelectedCells.size()));

    ImGui::Separator();
    ImGui::TextUnformatted(u8"編集モード");
    int modeValue = static_cast<int>(m_editMode);
    ImGui::RadioButton(u8"地形", &modeValue, static_cast<int>(EditMode::Terrain));
    ImGui::SameLine();
    ImGui::RadioButton(u8"ロープ", &modeValue, static_cast<int>(EditMode::Rope));
    ImGui::SameLine();
    ImGui::RadioButton(u8"採掘ポイント", &modeValue, static_cast<int>(EditMode::MiningPoint));
    m_editMode = static_cast<EditMode>(modeValue);

    ImGui::Separator();
    ImGui::SliderFloat(u8"前景地形アルファ", &m_frontLayerAlpha, 0.05f, 1.0f, "%.2f");

    auto drawLayerPointEditor = [this](const char* label, NarakuMap::LayerPoint& point, LayerPointSelectionTarget target)
    {
        ImGui::PushID(label);
        ImGui::Separator();
        ImGui::TextUnformatted(label);
        if (ImGui::Button(u8"選択してフォーカス", ImVec2(140.0f, 0.0f)))
        {
            m_selectedLayerPoint = target;
            m_armLayerPointPlacement = false;
            FocusSelection();
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"3D配置", ImVec2(96.0f, 0.0f)))
        {
            m_selectedLayerPoint = target;
            m_armLayerPointPlacement = true;
            m_lastIoMessage = std::string(label) + u8": シーン上を左クリックで配置します";
            AppendOperationLog(m_lastIoMessage);
            FocusSelection();
        }

        NarakuMap::Vec2 tempXZ = point.xz;
        if (ImGui::InputFloat2(u8"位置XZ", &tempXZ.x, "%.2f"))
        {
            PushUndoSnapshot();
            point.xz = tempXZ;
            if (m_enablePositionSnap)
            {
                ApplySnapToPoint(point.xz, point.layerId, m_snapToCellCenter);
            }
        }

        const std::string previewLayerLabel = std::to_string(point.layerId);
        if (ImGui::BeginCombo(u8"所属レイヤー", previewLayerLabel.c_str()))
        {
            for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
            {
                const bool selected = (point.layerId == layer.id);
                const std::string comboLabel = "Layer " + std::to_string(layer.id);
                if (ImGui::Selectable(comboLabel.c_str(), selected))
                {
                    PushUndoSnapshot();
                    point.layerId = layer.id;
                    if (m_enablePositionSnap)
                    {
                        ApplySnapToPoint(point.xz, point.layerId, m_snapToCellCenter);
                    }
                }
                if (selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        ImGui::PopID();
    };

    drawLayerPointEditor(u8"プレイヤー開始地点", m_mapData.playerStartPoint, LayerPointSelectionTarget::PlayerStart);
    drawLayerPointEditor(u8"帰還地点", m_mapData.returnPoint, LayerPointSelectionTarget::ReturnPoint);

    ImGui::Separator();

    float autoFallStartHeight = m_mapData.autoFallStartHeight;
    if (ImGui::SliderFloat(u8"自動落下開始高さ", &autoFallStartHeight, 0.10f, 3.00f, "%.2fm"))
    {
        PushUndoSnapshot();
        m_mapData.autoFallStartHeight = autoFallStartHeight;
    }

    const std::vector<NarakuMap::ValidationIssue>& issues = GetValidationIssues();
    ImGui::Text(u8"検証結果: %d件", static_cast<int>(issues.size()));
    for (const NarakuMap::ValidationIssue& issue : issues)
    {
        const ImVec4 color = (issue.severity == NarakuMap::ValidationIssue::Error)
            ? ImVec4(1.0f, 0.38f, 0.34f, 1.0f)
            : ImVec4(1.0f, 0.82f, 0.32f, 1.0f);
        ImGui::TextColored(color, "%s", issue.message.c_str());
    }

    if (ImGui::Button(u8"真上ビュー", ImVec2(110.0f, 0.0f)))
    {
        SnapCameraToTopView();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"プレイテスト起動", ImVec2(180.0f, 0.0f)))
    {
        if (SaveCurrentMap())
        {
            SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_PROTO);
        }
    }

    ImGui::End();
}

// Draw terrain layer settings and terrain edit controls.
void SceneNarakuEditor::DrawLayerWindow()
{
    ImGui::SetNextWindowPos(ImVec2(20.0f, 452.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(420.0f, 520.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"レイヤー");

    if (ImGui::Button(u8"レイヤー追加", ImVec2(110.0f, 0.0f)))
    {
        PushUndoSnapshot();
        m_mapData.terrainLayers.push_back(CreateNewLayer());
        m_selectedLayer = ToInt(m_mapData.terrainLayers.size()) - 1;
        m_selectedVertex = { m_selectedLayer, 0, 0 };
        m_selectedCell = { m_selectedLayer, 0, 0 };
        AppendOperationLog(u8"レイヤーを追加しました。");
        FocusSelection();
    }

    ImGui::SameLine();
    if (ImGui::Button(u8"選択レイヤー削除", ImVec2(150.0f, 0.0f)) && m_mapData.terrainLayers.size() > 1)
    {
        PushUndoSnapshot();
        const int removeLayerId = m_mapData.terrainLayers[m_selectedLayer].id;
        m_mapData.ropes.erase(
            std::remove_if(
                m_mapData.ropes.begin(),
                m_mapData.ropes.end(),
                [removeLayerId](const NarakuMap::RopePoint& rope)
                {
                    return rope.topLayerId == removeLayerId || rope.bottomLayerId == removeLayerId;
                }),
            m_mapData.ropes.end());

        m_mapData.miningPoints.erase(
            std::remove_if(
                m_mapData.miningPoints.begin(),
                m_mapData.miningPoints.end(),
                [removeLayerId](const NarakuMap::MiningPoint& point)
                {
                    return point.layerId == removeLayerId;
                }),
            m_mapData.miningPoints.end());

        if (m_mapData.playerStartPoint.layerId == removeLayerId)
        {
            m_mapData.playerStartPoint.layerId = m_mapData.terrainLayers.front().id;
            m_mapData.playerStartPoint.xz = { 0.0f, 0.0f };
        }
        if (m_mapData.returnPoint.layerId == removeLayerId)
        {
            m_mapData.returnPoint.layerId = m_mapData.terrainLayers.front().id;
            m_mapData.returnPoint.xz = { 0.0f, 0.0f };
        }

        m_mapData.terrainLayers.erase(m_mapData.terrainLayers.begin() + m_selectedLayer);
        EnsureSelectionValid();
        AppendOperationLog(u8"選択レイヤーを削除しました。");
        FocusSelection();
    }

    ImGui::Separator();
    for (int i = 0; i < ToInt(m_mapData.terrainLayers.size()); ++i)
    {
        const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[i];
        char label[160] = {};
        std::snprintf(
            label,
            sizeof(label),
            u8"Layer %d (depth %.2f)%s%s",
            layer.id,
            layer.layerDepth,
            layer.visible ? "" : " [Hidden]",
            layer.locked ? " [Locked]" : "");
        if (ImGui::Selectable(label, m_selectedLayer == i))
        {
            m_selectedLayer = i;
            m_selectedVertex.layerIndex = i;
            m_selectedCell.layerIndex = i;
            FocusSelection();
        }
    }

    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    NarakuMap::EnsureLayerHeights(layer);
    NarakuMap::EnsureLayerVertexEnabled(layer);
    NarakuMap::EnsureLayerCellAttributes(layer);

    int terrainTargetValue = static_cast<int>(m_terrainEditTarget);
    ImGui::TextUnformatted(u8"地形編集対象");
    ImGui::RadioButton(u8"頂点", &terrainTargetValue, static_cast<int>(TerrainEditTarget::Vertex));
    ImGui::SameLine();
    ImGui::RadioButton(u8"面", &terrainTargetValue, static_cast<int>(TerrainEditTarget::Face));
    m_terrainEditTarget = static_cast<TerrainEditTarget>(terrainTargetValue);
    ImGui::Separator();

    bool visible = layer.visible;
    if (ImGui::Checkbox(u8"表示", &visible))
    {
        PushUndoSnapshot();
        layer.visible = visible;
    }
    ImGui::SameLine();
    bool locked = layer.locked;
    if (ImGui::Checkbox(u8"編集ロック", &locked))
    {
        PushUndoSnapshot();
        layer.locked = locked;
    }

    ImGui::BeginDisabled(layer.locked);

    int layerId = layer.id;
    if (ImGui::InputInt(u8"ID", &layerId))
    {
        PushUndoSnapshot();
        layer.id = layerId;
    }

    NarakuMap::Vec2 center = layer.center;
    if (ImGui::InputFloat2(u8"中心XZ", &center.x, "%.2f"))
    {
        PushUndoSnapshot();
        layer.center = center;
    }

    float layerDepth = layer.layerDepth;
    if (ImGui::InputFloat(u8"層深度", &layerDepth, 0.1f, 1.0f, "%.2f"))
    {
        PushUndoSnapshot();
        layer.layerDepth = layerDepth;
    }

    int gridWidth = layer.gridWidth;
    if (ImGui::InputInt(u8"Grid Width", &gridWidth))
    {
        PushUndoSnapshot();
        layer.gridWidth = std::max(gridWidth, 2);
        NarakuMap::EnsureLayerHeights(layer);
        NarakuMap::EnsureLayerVertexEnabled(layer);
        NarakuMap::EnsureLayerCellAttributes(layer);
    }

    int gridHeight = layer.gridHeight;
    if (ImGui::InputInt(u8"Grid Height", &gridHeight))
    {
        PushUndoSnapshot();
        layer.gridHeight = std::max(gridHeight, 2);
        NarakuMap::EnsureLayerHeights(layer);
        NarakuMap::EnsureLayerVertexEnabled(layer);
        NarakuMap::EnsureLayerCellAttributes(layer);
    }

    float cellSize = layer.cellSize;
    if (ImGui::InputFloat(u8"Cell Size", &cellSize, 0.1f, 1.0f, "%.2f"))
    {
        PushUndoSnapshot();
        layer.cellSize = std::max(cellSize, 0.25f);
    }

    layer.gridWidth = std::max(layer.gridWidth, 2);
    layer.gridHeight = std::max(layer.gridHeight, 2);
    layer.cellSize = std::max(layer.cellSize, 0.25f);
    NarakuMap::EnsureLayerHeights(layer);
    NarakuMap::EnsureLayerVertexEnabled(layer);
    NarakuMap::EnsureLayerCellAttributes(layer);

    int textureId = ClampValue(layer.groundTextureId, 0, 3);
    if (ImGui::BeginCombo(u8"地面テクスチャ", GetGroundTextureLabel(textureId)))
    {
        for (int i = 0; i < 4; ++i)
        {
            const bool selected = (textureId == i);
            if (ImGui::Selectable(GetGroundTextureLabel(i), selected))
            {
                PushUndoSnapshot();
                textureId = i;
                layer.groundTextureId = textureId;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    layer.groundTextureId = textureId;

    ImGui::Separator();
    if (m_terrainEditTarget == TerrainEditTarget::Vertex)
    {
        ImGui::Text(u8"選択頂点: (%d, %d)", m_selectedVertex.gridX, m_selectedVertex.gridZ);
        float vertexHeight = NarakuMap::GetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
        if (ImGui::InputFloat(u8"頂点高さY", &vertexHeight, 0.05f, 0.25f, "%.2f"))
        {
            PushUndoSnapshot();
            NarakuMap::SetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ, vertexHeight);
        }

        ImGui::Text(u8"複数選択頂点数: %d", m_multiSelectedVertices.empty() ? 1 : ToInt(m_multiSelectedVertices.size()));
        if (ImGui::Button(u8"頂点を0に戻す", ImVec2(110.0f, 0.0f)))
        {
            PushUndoSnapshot();
            if (m_multiSelectedVertices.empty())
            {
                NarakuMap::SetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ, 0.0f);
            }
            else
            {
                for (const VertexSelection& selection : m_multiSelectedVertices)
                {
                    if (selection.layerIndex >= 0 && selection.layerIndex < ToInt(m_mapData.terrainLayers.size()))
                    {
                        NarakuMap::TerrainLayer& targetLayer = m_mapData.terrainLayers[selection.layerIndex];
                        NarakuMap::SetVertexHeight(targetLayer, selection.gridX, selection.gridZ, 0.0f);
                    }
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"選択頂点を+0.25m", ImVec2(140.0f, 0.0f)))
        {
            PushUndoSnapshot();
            ApplyHeightDeltaToSelectedVertices(0.25f);
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"選択頂点を-0.25m", ImVec2(140.0f, 0.0f)))
        {
            PushUndoSnapshot();
            ApplyHeightDeltaToSelectedVertices(-0.25f);
        }
        if (ImGui::Button(u8"選択頂点削除", ImVec2(140.0f, 0.0f)))
        {
            PushUndoSnapshot();
            if (!DeleteSelectedTerrainVertices() && !m_undoStack.empty())
            {
                m_undoStack.pop_back();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"無効頂点を復活", ImVec2(140.0f, 0.0f)))
        {
            PushUndoSnapshot();
            if (!RestoreSelectedTerrainGeometry() && !m_undoStack.empty())
            {
                m_undoStack.pop_back();
            }
        }
    }
    else
    {
        int cellX = 0;
        int cellZ = 0;
        GetSelectedCellCoords(layer, cellX, cellZ);
        ImGui::Text(u8"選択セル: (%d, %d)", cellX, cellZ);
        ImGui::Text(u8"複数選択セル数: %d", m_multiSelectedCells.empty() ? 1 : ToInt(m_multiSelectedCells.size()));

        float cellAverageHeight = GetSelectedCellAverageHeight(layer);
        if (ImGui::InputFloat(u8"面平均高さY", &cellAverageHeight, 0.05f, 0.25f, "%.2f"))
        {
            PushUndoSnapshot();
            const float currentAverageHeight = GetSelectedCellAverageHeight(layer);
            ApplyHeightDeltaToCell(layer, cellX, cellZ, cellAverageHeight - currentAverageHeight);
        }

        if (ImGui::Button(u8"面を+0.25m", ImVec2(110.0f, 0.0f)))
        {
            PushUndoSnapshot();
            ApplyHeightDeltaToSelectedCells(0.25f);
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"面を-0.25m", ImVec2(110.0f, 0.0f)))
        {
            PushUndoSnapshot();
            ApplyHeightDeltaToSelectedCells(-0.25f);
        }
        ImGui::SameLine();
        if (ImGui::Button(u8"選択面削除", ImVec2(110.0f, 0.0f)))
        {
            PushUndoSnapshot();
            if (!DeleteSelectedTerrainCells() && !m_undoStack.empty())
            {
                m_undoStack.pop_back();
            }
        }
        if (ImGui::Button(u8"選択面を復活", ImVec2(110.0f, 0.0f)))
        {
            PushUndoSnapshot();
            if (!RestoreSelectedTerrainGeometry() && !m_undoStack.empty())
            {
                m_undoStack.pop_back();
            }
        }

        ImGui::Separator();
        std::uint32_t cellFlags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
        bool blocked = (cellFlags & NarakuMap::CellAttributeBlocked) != 0u;
        bool cliffEdge = (cellFlags & NarakuMap::CellAttributeCliffEdge) != 0u;
        bool dropAllowed = (cellFlags & NarakuMap::CellAttributeDropAllowed) != 0u;
        bool ropeAnchor = (cellFlags & NarakuMap::CellAttributeRopeAnchor) != 0u;
        bool hazard = (cellFlags & NarakuMap::CellAttributeHazard) != 0u;
        const bool removed = (cellFlags & NarakuMap::CellAttributeRemoved) != 0u;

        ImGui::TextUnformatted(u8"セル属性");
        if (removed)
        {
            ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.25f, 1.0f), u8"このセルは削除済みです");
        }
        bool cellFlagsChanged = false;
        cellFlagsChanged |= ImGui::Checkbox(u8"歩行不可", &blocked);
        ImGui::SameLine();
        cellFlagsChanged |= ImGui::Checkbox(u8"崖境界", &cliffEdge);
        cellFlagsChanged |= ImGui::Checkbox(u8"落下可", &dropAllowed);
        ImGui::SameLine();
        cellFlagsChanged |= ImGui::Checkbox(u8"ロープ接続候補", &ropeAnchor);
        cellFlagsChanged |= ImGui::Checkbox(u8"危険地形", &hazard);

        std::uint32_t newFlags = NarakuMap::CellAttributeNone;
        if (blocked) newFlags |= NarakuMap::CellAttributeBlocked;
        if (cliffEdge) newFlags |= NarakuMap::CellAttributeCliffEdge;
        if (dropAllowed) newFlags |= NarakuMap::CellAttributeDropAllowed;
        if (ropeAnchor) newFlags |= NarakuMap::CellAttributeRopeAnchor;
        if (hazard) newFlags |= NarakuMap::CellAttributeHazard;
        if (removed) newFlags |= NarakuMap::CellAttributeRemoved;
        if (cellFlagsChanged)
        {
            PushUndoSnapshot();
        }
        if (m_multiSelectedCells.empty())
        {
            NarakuMap::SetCellAttributeFlags(layer, cellX, cellZ, newFlags);
        }
        else
        {
            for (const CellSelection& selection : m_multiSelectedCells)
            {
                if (selection.layerIndex >= 0 && selection.layerIndex < ToInt(m_mapData.terrainLayers.size()))
                {
                    NarakuMap::TerrainLayer& targetLayer = m_mapData.terrainLayers[selection.layerIndex];
                    NarakuMap::SetCellAttributeFlags(targetLayer, selection.cellX, selection.cellZ, newFlags);
                }
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button(u8"全頂点を0に戻す", ImVec2(130.0f, 0.0f)))
    {
        PushUndoSnapshot();
        std::fill(layer.heights.begin(), layer.heights.end(), 0.0f);
    }
    ImGui::EndDisabled();

    ImGui::End();
}

// Draw the rope list and rope property controls.
void SceneNarakuEditor::DrawRopeWindow()
{
    ImGui::SetNextWindowPos(ImVec2(900.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"ロープ");

    if (ImGui::Button(u8"ロープ追加", ImVec2(100.0f, 0.0f)))
    {
        PushUndoSnapshot();
        m_mapData.ropes.push_back(CreateNewRope());
        m_selectedRope = ToInt(m_mapData.ropes.size()) - 1;
        m_editMode = EditMode::Rope;
        ClearMultiSelection();
        AddSelectedRope(m_selectedRope);
        AppendOperationLog(u8"ロープを追加しました。");
        FocusSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"選択ロープ削除", ImVec2(130.0f, 0.0f)))
    {
        PushUndoSnapshot();
        DeleteCurrentSelection();
    }

    ImGui::Separator();
    for (int i = 0; i < ToInt(m_mapData.ropes.size()); ++i)
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[i];
        char label[128] = {};
        std::snprintf(label, sizeof(label), u8"Rope %d (Top %d / Bottom %d)", i, rope.topLayerId, rope.bottomLayerId);
        const bool selected = IsRopeMultiSelected(i) || m_selectedRope == i;
        if (ImGui::Selectable(label, selected))
        {
            m_editMode = EditMode::Rope;
            if (ImGui::GetIO().KeyShift)
            {
                AddSelectedRope(i);
            }
            else
            {
                ClearMultiSelection();
                AddSelectedRope(i);
            }
            m_selectedRope = i;
            FocusSelection();
        }
    }

    const int selectedRopeCount = m_multiSelectedRopes.empty()
        ? ((m_selectedRope >= 0 && m_selectedRope < ToInt(m_mapData.ropes.size())) ? 1 : 0)
        : ToInt(m_multiSelectedRopes.size());
    ImGui::Text(u8"複数選択ロープ数: %d", selectedRopeCount);

    if (m_selectedRope < 0 || m_selectedRope >= ToInt(m_mapData.ropes.size()))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    NarakuMap::RopePoint& rope = m_mapData.ropes[m_selectedRope];
    NarakuMap::Vec2 ropeXZ = rope.xz;
    if (ImGui::InputFloat2(u8"位置XZ", &ropeXZ.x, "%.2f"))
    {
        PushUndoSnapshot();
        rope.xz = ropeXZ;
        if (m_enablePositionSnap)
        {
            ApplySnapToPoint(rope.xz, rope.topLayerId, m_snapToCellCenter);
        }
    }

    if (ImGui::BeginCombo(u8"上層レイヤー", std::to_string(rope.topLayerId).c_str()))
    {
        for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
        {
            const bool selected = (rope.topLayerId == layer.id);
            const std::string label = "Layer " + std::to_string(layer.id);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                rope.topLayerId = layer.id;
                if (m_enablePositionSnap)
                {
                    ApplySnapToPoint(rope.xz, rope.topLayerId, m_snapToCellCenter);
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo(u8"下層レイヤー", std::to_string(rope.bottomLayerId).c_str()))
    {
        for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
        {
            const bool selected = (rope.bottomLayerId == layer.id);
            const std::string label = "Layer " + std::to_string(layer.id);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                rope.bottomLayerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (selectedRopeCount > 1)
    {
        ImGui::Separator();
        ImGui::TextUnformatted(u8"複数選択一括反映");
        if (ImGui::Button(u8"主選択の位置XZを一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int ropeIndex : m_multiSelectedRopes)
            {
                if (ropeIndex == m_selectedRope || ropeIndex < 0 || ropeIndex >= ToInt(m_mapData.ropes.size()))
                {
                    continue;
                }
                m_mapData.ropes[ropeIndex].xz = rope.xz;
                if (m_enablePositionSnap)
                {
                    ApplySnapToPoint(m_mapData.ropes[ropeIndex].xz, m_mapData.ropes[ropeIndex].topLayerId, m_snapToCellCenter);
                }
            }
        }
        if (ImGui::Button(u8"主選択の上層レイヤーを一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int ropeIndex : m_multiSelectedRopes)
            {
                if (ropeIndex < 0 || ropeIndex >= ToInt(m_mapData.ropes.size()))
                {
                    continue;
                }
                m_mapData.ropes[ropeIndex].topLayerId = rope.topLayerId;
                if (m_enablePositionSnap)
                {
                    ApplySnapToPoint(m_mapData.ropes[ropeIndex].xz, m_mapData.ropes[ropeIndex].topLayerId, m_snapToCellCenter);
                }
            }
        }
        if (ImGui::Button(u8"主選択の下層レイヤーを一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int ropeIndex : m_multiSelectedRopes)
            {
                if (ropeIndex < 0 || ropeIndex >= ToInt(m_mapData.ropes.size()))
                {
                    continue;
                }
                m_mapData.ropes[ropeIndex].bottomLayerId = rope.bottomLayerId;
            }
        }
    }

    ImGui::End();
}

// Draw the mining point list and mining point property controls.
void SceneNarakuEditor::DrawMiningWindow()
{
    ImGui::SetNextWindowPos(ImVec2(900.0f, 352.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(380.0f, 380.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"採掘ポイント");

    if (ImGui::Button(u8"採掘ポイント追加", ImVec2(130.0f, 0.0f)))
    {
        PushUndoSnapshot();
        m_mapData.miningPoints.push_back(CreateNewMiningPoint());
        m_selectedMiningPoint = ToInt(m_mapData.miningPoints.size()) - 1;
        m_editMode = EditMode::MiningPoint;
        ClearMultiSelection();
        AddSelectedMiningPoint(m_selectedMiningPoint);
        AppendOperationLog(u8"採掘ポイントを追加しました。");
        FocusSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"選択ポイント削除", ImVec2(130.0f, 0.0f)))
    {
        PushUndoSnapshot();
        DeleteCurrentSelection();
    }

    ImGui::Separator();
    for (int i = 0; i < ToInt(m_mapData.miningPoints.size()); ++i)
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[i];
        char label[160] = {};
        std::snprintf(label, sizeof(label), u8"Point %d (Layer %d / Type %d)%s", i, point.layerId, point.visualType, point.enabled ? "" : " [Disabled]");
        const bool selected = IsMiningPointMultiSelected(i) || m_selectedMiningPoint == i;
        if (ImGui::Selectable(label, selected))
        {
            m_editMode = EditMode::MiningPoint;
            if (ImGui::GetIO().KeyShift)
            {
                AddSelectedMiningPoint(i);
            }
            else
            {
                ClearMultiSelection();
                AddSelectedMiningPoint(i);
            }
            m_selectedMiningPoint = i;
            FocusSelection();
        }
    }

    const int selectedPointCount = m_multiSelectedMiningPoints.empty()
        ? ((m_selectedMiningPoint >= 0 && m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size())) ? 1 : 0)
        : ToInt(m_multiSelectedMiningPoints.size());
    ImGui::Text(u8"複数選択採掘ポイント数: %d", selectedPointCount);

    if (m_selectedMiningPoint < 0 || m_selectedMiningPoint >= ToInt(m_mapData.miningPoints.size()))
    {
        ImGui::End();
        return;
    }

    ImGui::Separator();
    NarakuMap::MiningPoint& point = m_mapData.miningPoints[m_selectedMiningPoint];
    NarakuMap::Vec2 pointXZ = point.xz;
    if (ImGui::InputFloat2(u8"位置XZ", &pointXZ.x, "%.2f"))
    {
        PushUndoSnapshot();
        point.xz = pointXZ;
        if (m_enablePositionSnap)
        {
            ApplySnapToPoint(point.xz, point.layerId, m_snapToCellCenter);
        }
    }

    int visualType = point.visualType;
    if (ImGui::InputInt(u8"見た目種別", &visualType))
    {
        PushUndoSnapshot();
        point.visualType = ClampValue(visualType, 0, 3);
    }
    point.visualType = ClampValue(point.visualType, 0, 3);

    bool discovered = point.discovered;
    if (ImGui::Checkbox(u8"初期発見済み", &discovered))
    {
        PushUndoSnapshot();
        point.discovered = discovered;
    }

    bool enabled = point.enabled;
    if (ImGui::Checkbox(u8"有効", &enabled))
    {
        PushUndoSnapshot();
        point.enabled = enabled;
    }
    ImGui::SameLine();
    bool respawnCandidate = point.respawnCandidate;
    if (ImGui::Checkbox(u8"再出現候補", &respawnCandidate))
    {
        PushUndoSnapshot();
        point.respawnCandidate = respawnCandidate;
    }

    char relicNameBuffer[128] = {};
    std::snprintf(relicNameBuffer, sizeof(relicNameBuffer), "%s", point.relicName.c_str());
    if (ImGui::InputText(u8"旧器名", relicNameBuffer, sizeof(relicNameBuffer)))
    {
        PushUndoSnapshot();
        point.relicName = relicNameBuffer;
    }

    if (ImGui::BeginCombo(u8"所属レイヤー", std::to_string(point.layerId).c_str()))
    {
        for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
        {
            const bool selected = (point.layerId == layer.id);
            const std::string label = "Layer " + std::to_string(layer.id);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                PushUndoSnapshot();
                point.layerId = layer.id;
                if (m_enablePositionSnap)
                {
                    ApplySnapToPoint(point.xz, point.layerId, m_snapToCellCenter);
                }
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (selectedPointCount > 1)
    {
        ImGui::Separator();
        ImGui::TextUnformatted(u8"複数選択一括反映");
        if (ImGui::Button(u8"位置XZを一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int pointIndex : m_multiSelectedMiningPoints)
            {
                if (pointIndex == m_selectedMiningPoint || pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
                {
                    continue;
                }
                m_mapData.miningPoints[pointIndex].xz = point.xz;
                if (m_enablePositionSnap)
                {
                    ApplySnapToPoint(m_mapData.miningPoints[pointIndex].xz, m_mapData.miningPoints[pointIndex].layerId, m_snapToCellCenter);
                }
            }
        }
        if (ImGui::Button(u8"見た目種別を一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int pointIndex : m_multiSelectedMiningPoints)
            {
                if (pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
                {
                    continue;
                }
                m_mapData.miningPoints[pointIndex].visualType = point.visualType;
            }
        }
        if (ImGui::Button(u8"初期発見済みを一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int pointIndex : m_multiSelectedMiningPoints)
            {
                if (pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
                {
                    continue;
                }
                m_mapData.miningPoints[pointIndex].discovered = point.discovered;
            }
        }
        if (ImGui::Button(u8"有効 / 再出現候補を一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int pointIndex : m_multiSelectedMiningPoints)
            {
                if (pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
                {
                    continue;
                }
                m_mapData.miningPoints[pointIndex].enabled = point.enabled;
                m_mapData.miningPoints[pointIndex].respawnCandidate = point.respawnCandidate;
            }
        }
        if (ImGui::Button(u8"旧器名を一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int pointIndex : m_multiSelectedMiningPoints)
            {
                if (pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
                {
                    continue;
                }
                m_mapData.miningPoints[pointIndex].relicName = point.relicName;
            }
        }
        if (ImGui::Button(u8"所属レイヤーを一括反映", ImVec2(-1.0f, 0.0f)))
        {
            PushUndoSnapshot();
            for (const int pointIndex : m_multiSelectedMiningPoints)
            {
                if (pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
                {
                    continue;
                }
                m_mapData.miningPoints[pointIndex].layerId = point.layerId;
                if (m_enablePositionSnap)
                {
                    ApplySnapToPoint(m_mapData.miningPoints[pointIndex].xz, m_mapData.miningPoints[pointIndex].layerId, m_snapToCellCenter);
                }
            }
        }
    }

    ImGui::End();
}

// Draw the editor control reference window.
void SceneNarakuEditor::DrawHelpWindow()
{
    ImGui::SetNextWindowPos(ImVec2(390.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 170.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"操作説明");
    ImGui::Text(u8"Alt + 左ドラッグ: Orbit");
    ImGui::Text(u8"中ドラッグ: Pan");
    ImGui::Text(u8"ホイール: Zoom");
    ImGui::Text(u8"右ドラッグ + WASD / Q / E: Fly");
    ImGui::Text(u8"左クリック: 選択 / ハンドルドラッグ");
    ImGui::Text(u8"Shift + 左ドラッグ: Terrain 矩形選択");
    ImGui::Text(u8"Delete: 選択削除   Ctrl+Z / Ctrl+Y: Undo / Redo");
    ImGui::Text(u8"T: 真上ビュー   F: 選択対象へフォーカス");
    ImGui::End();
}

void SceneNarakuEditor::DrawFeatureWindow()
{
    ImGui::SetNextWindowPos(ImVec2(390.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280.0f, 260.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"機能紹介");
    ImGui::TextUnformatted(u8"地形");
    ImGui::BulletText(u8"頂点 / 面の高さ編集");
    ImGui::BulletText(u8"面削除と面復活");
    ImGui::BulletText(u8"孤立頂点削除と無効頂点復活");
    ImGui::BulletText(u8"セル属性編集");
    ImGui::Separator();
    ImGui::TextUnformatted(u8"配置物");
    ImGui::BulletText(u8"ロープ配置と複数選択");
    ImGui::BulletText(u8"採掘ポイント配置と複数選択");
    ImGui::BulletText(u8"開始地点 / 帰還地点の3D配置");
    ImGui::Separator();
    ImGui::TextUnformatted(u8"運用");
    ImGui::BulletText(u8"保存 / 別名保存 / 読込 / 新規");
    ImGui::BulletText(u8"Undo / Redo");
    ImGui::BulletText(u8"検証結果表示");
    ImGui::BulletText(u8"プレイテスト起動");
    ImGui::Separator();
    ImGui::TextWrapped(u8"面を作る時は、削除済み面を選んで「選択面を復活」を使います。頂点は面復活で自動復活し、頂点モードでは無効頂点をまとめて復活できます。");
    ImGui::End();
}

// Draw a compact inspector window for the current primary selection.
void SceneNarakuEditor::DrawInspectorWindow()
{
    ImGui::SetNextWindowPos(ImVec2(1288.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 280.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"インスペクタ");

    const int vertexCount = m_multiSelectedVertices.empty() ? 1 : ToInt(m_multiSelectedVertices.size());
    const int cellCount = m_multiSelectedCells.empty() ? 1 : ToInt(m_multiSelectedCells.size());
    const char* modeLabel = (m_editMode == EditMode::Terrain)
        ? ((m_terrainEditTarget == TerrainEditTarget::Vertex) ? u8"地形 / 頂点" : u8"地形 / 面")
        : ((m_editMode == EditMode::Rope) ? u8"ロープ" : u8"採掘ポイント");
    ImGui::Text(u8"現在モード: %s", modeLabel);
    ImGui::Checkbox(u8"自動フォーカス同期", &m_autoFocusSelection);
    ImGui::Text(u8"開始/帰還地点選択: %s",
        (m_selectedLayerPoint == LayerPointSelectionTarget::PlayerStart) ? u8"開始地点" :
        (m_selectedLayerPoint == LayerPointSelectionTarget::ReturnPoint) ? u8"帰還地点" : u8"なし");

    if (m_editMode == EditMode::Terrain)
    {
        if (m_terrainEditTarget == TerrainEditTarget::Vertex)
        {
            ImGui::Text(u8"主選択頂点: L%d (%d, %d)", m_selectedVertex.layerIndex, m_selectedVertex.gridX, m_selectedVertex.gridZ);
            ImGui::Text(u8"複数選択頂点数: %d", vertexCount);
        }
        else
        {
            ImGui::Text(u8"主選択セル: L%d (%d, %d)", m_selectedCell.layerIndex, m_selectedCell.cellX, m_selectedCell.cellZ);
            ImGui::Text(u8"複数選択セル数: %d", cellCount);
        }
        if (ImGui::Button(u8"複数選択をクリア", ImVec2(150.0f, 0.0f)))
        {
            ClearMultiSelection();
            if (m_terrainEditTarget == TerrainEditTarget::Vertex)
            {
                AddSelectedVertex(m_selectedVertex.layerIndex, m_selectedVertex.gridX, m_selectedVertex.gridZ);
            }
            else
            {
                AddSelectedCell(m_selectedCell.layerIndex, m_selectedCell.cellX, m_selectedCell.cellZ);
            }
        }
    }
    else if (m_editMode == EditMode::Rope)
    {
        ImGui::Text(u8"主選択ロープ: %d", m_selectedRope);
        ImGui::Text(u8"複数選択ロープ数: %d", m_multiSelectedRopes.empty() ? ((m_selectedRope >= 0) ? 1 : 0) : ToInt(m_multiSelectedRopes.size()));
        if (ImGui::Button(u8"ロープ複数選択をクリア", ImVec2(170.0f, 0.0f)))
        {
            ClearMultiSelection();
            if (m_selectedRope >= 0)
            {
                AddSelectedRope(m_selectedRope);
            }
        }
    }
    else
    {
        ImGui::Text(u8"主選択採掘ポイント: %d", m_selectedMiningPoint);
        ImGui::Text(u8"複数選択採掘ポイント数: %d", m_multiSelectedMiningPoints.empty() ? ((m_selectedMiningPoint >= 0) ? 1 : 0) : ToInt(m_multiSelectedMiningPoints.size()));
        if (ImGui::Button(u8"採掘ポイント複数選択をクリア", ImVec2(210.0f, 0.0f)))
        {
            ClearMultiSelection();
            if (m_selectedMiningPoint >= 0)
            {
                AddSelectedMiningPoint(m_selectedMiningPoint);
            }
        }
    }

    if (ImGui::Button(u8"選択対象へフォーカス", ImVec2(160.0f, 0.0f)))
    {
        FocusSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"プレイテスト起動", ImVec2(120.0f, 0.0f)))
    {
        if (SaveCurrentMap())
        {
            SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_PROTO);
        }
    }

    ImGui::Separator();
    ImGui::TextUnformatted(u8"表示切替");
    ImGui::Checkbox(u8"床プレビュー", &m_showFloorPreview);
    ImGui::SameLine();
    ImGui::Checkbox(u8"グリッド線", &m_showGridLines);
    if (m_showGridLines)
    {
        ImGui::Checkbox(u8"全グリッド線", &m_showFullGridLines);
    }
    if (m_showFloorPreview)
    {
        ImGui::Checkbox(u8"高精細床プレビュー", &m_useDetailedFloorPreview);
        ImGui::SliderInt(u8"高精細セル上限", &m_detailedFloorPreviewCellLimit, 64, 4096);
    }
    ImGui::Checkbox(u8"ロープ", &m_showRopePreview);
    ImGui::SameLine();
    ImGui::Checkbox(u8"採掘ポイント", &m_showMiningPreview);
    ImGui::Checkbox(u8"開始/帰還地点", &m_showStartReturnPreview);
    ImGui::SameLine();
    ImGui::Checkbox(u8"境界表示", &m_showBoundaryPreview);
    ImGui::Checkbox(u8"ホバー頂点", &m_showHoveredVertexPreview);

    ImGui::Separator();
    ImGui::TextUnformatted(u8"位置スナップ");
    ImGui::Checkbox(u8"有効", &m_enablePositionSnap);
    ImGui::SameLine();
    ImGui::Checkbox(u8"セル中心へ", &m_snapToCellCenter);

    ImGui::End();
}

void SceneNarakuEditor::DrawOperationLogWindow()
{
    ImGui::SetNextWindowPos(ImVec2(1288.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"操作ログ");
    ImGui::Checkbox(u8"末尾へ自動追従", &m_autoScrollOperationLog);
    ImGui::SameLine();
    if (ImGui::Button(u8"クリア", ImVec2(70.0f, 0.0f)))
    {
        m_operationLogs.clear();
        m_lastIoMessage = u8"操作ログをクリアしました。";
    }

    ImGui::Separator();
    ImGui::BeginChild("OperationLogList", ImVec2(0.0f, 0.0f), true);
    for (const std::string& logLine : m_operationLogs)
    {
        ImGui::TextWrapped("%s", logLine.c_str());
    }
    if (m_autoScrollOperationLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 20.0f)
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

// Draw the current marquee rectangle as a 2D overlay.
void SceneNarakuEditor::DrawMarqueeOverlay()
{
    if (!m_marqueeSelecting)
    {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const ImVec2 minPos(
        static_cast<float>(std::min(m_marqueeStart.x, m_marqueeEnd.x)),
        static_cast<float>(std::min(m_marqueeStart.y, m_marqueeEnd.y)));
    const ImVec2 maxPos(
        static_cast<float>(std::max(m_marqueeStart.x, m_marqueeEnd.x)),
        static_cast<float>(std::max(m_marqueeStart.y, m_marqueeEnd.y)));

    drawList->AddRectFilled(minPos, maxPos, IM_COL32(255, 196, 64, 40));
    drawList->AddRect(minPos, maxPos, IM_COL32(255, 196, 64, 220), 0.0f, 0, 2.0f);
}

// Draw the active vertex or face height gizmo.
void SceneNarakuEditor::DrawVertexGizmo()
{

    if (m_selectedVertex.layerIndex < 0 ||
        m_selectedVertex.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
    XMFLOAT3 base = {};
    if (m_terrainEditTarget == TerrainEditTarget::Face)
    {
        int cellX = 0;
        int cellZ = 0;
        GetSelectedCellCoords(layer, cellX, cellZ);
        const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
        base = XMFLOAT3(
            (p00.x + p10.x + p01.x + p11.x) * 0.25f,
            (p00.y + p10.y + p01.y + p11.y) * 0.25f,
            (p00.z + p10.z + p01.z + p11.z) * 0.25f);
    }
    else
    {
        base = GetVertexWorldPosition(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
    }

    const XMFLOAT3 top = { base.x, base.y + 2.0f, base.z };
    Geometory::AddLine(base, top, { 0.2f, 0.95f, 0.35f, 1.0f });

    DrawDebugBox3D(base, { 0.14f, 0.14f, 0.14f });

    DrawDebugBox3D(top, { 0.20f, 0.20f, 0.20f });
}

// Draw one terrain layer, its preview floor, grid, and selection highlights.
void SceneNarakuEditor::DrawTerrainLayer(const NarakuMap::TerrainLayer& layer, int layerIndex)
{
    if (!layer.visible)
    {
        return;
    }

    XMFLOAT4 floorColor = GetGroundTextureTint(layer.groundTextureId);
    floorColor.w = GetLayerAlpha(layer);

    const XMFLOAT4 wireColor = (layerIndex == m_selectedLayer)
        ? XMFLOAT4(0.90f, 0.95f, 1.00f, 1.0f)
        : XMFLOAT4(0.55f, 0.58f, 0.65f, 1.0f);
    const float kGridLineLift = 0.03f;
    const bool drawDetailedFloorPreview = ShouldDrawDetailedFloorPreview(layer, layerIndex);

    if (m_showFloorPreview && !drawDetailedFloorPreview)
    {
        DrawTransparentFloor3D(GetLayerFloorCenter(layer), GetLayerFloorSize(layer), floorColor);
    }

    for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
    {
        for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
        {
            if (IsCellRemoved(layer, cellX, cellZ))
            {
                continue;
            }

            const bool allVerticesEnabled =
                NarakuMap::IsVertexEnabled(layer, cellX, cellZ) &&
                NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ) &&
                NarakuMap::IsVertexEnabled(layer, cellX, cellZ + 1) &&
                NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ + 1);
            if (!allVerticesEnabled)
            {
                continue;
            }

            if (drawDetailedFloorPreview)
            {
                DrawTerrainCellPreview(layer, cellX, cellZ, floorColor);
            }

            if (m_showGridLines)
            {
                XMFLOAT3 a = GetVertexWorldPosition(layer, cellX, cellZ);
                XMFLOAT3 b = GetVertexWorldPosition(layer, cellX + 1, cellZ);
                XMFLOAT3 c = GetVertexWorldPosition(layer, cellX, cellZ + 1);
                XMFLOAT3 d = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
                a.y += kGridLineLift;
                b.y += kGridLineLift;
                c.y += kGridLineLift;
                d.y += kGridLineLift;

                const bool drawTopEdge = m_showFullGridLines || (cellZ == 0) || IsCellRemoved(layer, cellX, cellZ - 1);
                const bool drawLeftEdge = m_showFullGridLines || (cellX == 0) || IsCellRemoved(layer, cellX - 1, cellZ);
                const bool drawBottomEdge = m_showFullGridLines || (cellZ == layer.gridHeight - 2) || IsCellRemoved(layer, cellX, cellZ + 1);
                const bool drawRightEdge = m_showFullGridLines || (cellX == layer.gridWidth - 2) || IsCellRemoved(layer, cellX + 1, cellZ);

                if (drawTopEdge)
                {
                    Geometory::AddLine(a, b, wireColor);
                }
                if (drawLeftEdge)
                {
                    Geometory::AddLine(c, a, wireColor);
                }
                if (drawBottomEdge)
                {
                    Geometory::AddLine(d, c, wireColor);
                }
                if (drawRightEdge)
                {
                    Geometory::AddLine(b, d, wireColor);
                }
            }
        }
    }

    if (layerIndex == m_selectedLayer)
    {
        const XMFLOAT3 center = GetLayerFloorCenter(layer);
        DrawDebugBox3D({ center.x, center.y + 0.10f, center.z }, { 0.35f, 0.20f, 0.35f });
    }

    if (m_showBoundaryPreview)
    {
        DrawBoundaryOverlay(layer);
    }

    if (m_terrainEditTarget == TerrainEditTarget::Vertex && !m_multiSelectedVertices.empty())
    {
        for (const VertexSelection& selection : m_multiSelectedVertices)
        {
            if (selection.layerIndex != layerIndex)
            {
                continue;
            }
            const XMFLOAT3 pos = GetVertexWorldPosition(layer, selection.gridX, selection.gridZ);
            const XMFLOAT4 color = (selection.gridX == m_selectedVertex.gridX && selection.gridZ == m_selectedVertex.gridZ)
                ? XMFLOAT4(1.0f, 0.95f, 0.55f, 1.0f)
                : XMFLOAT4(0.35f, 0.85f, 1.0f, 1.0f);
            const float radius = 0.16f;
            Geometory::AddLine({ pos.x - radius, pos.y, pos.z }, { pos.x + radius, pos.y, pos.z }, color);
            Geometory::AddLine({ pos.x, pos.y, pos.z - radius }, { pos.x, pos.y, pos.z + radius }, color);
        }
    }

    if (m_terrainEditTarget == TerrainEditTarget::Face && !m_multiSelectedCells.empty())
    {
        for (const CellSelection& selection : m_multiSelectedCells)
        {
            if (selection.layerIndex != layerIndex)
            {
                continue;
            }
            const XMFLOAT3 a = GetVertexWorldPosition(layer, selection.cellX, selection.cellZ);
            const XMFLOAT3 b = GetVertexWorldPosition(layer, selection.cellX + 1, selection.cellZ);
            const XMFLOAT3 c = GetVertexWorldPosition(layer, selection.cellX, selection.cellZ + 1);
            const XMFLOAT3 d = GetVertexWorldPosition(layer, selection.cellX + 1, selection.cellZ + 1);
            const XMFLOAT4 color = (selection.cellX == m_selectedCell.cellX && selection.cellZ == m_selectedCell.cellZ)
                ? XMFLOAT4(1.0f, 0.78f, 0.20f, 1.0f)
                : XMFLOAT4(0.20f, 0.85f, 1.0f, 0.95f);
            Geometory::AddLine(a, b, color);
            Geometory::AddLine(b, d, color);
            Geometory::AddLine(d, c, color);
            Geometory::AddLine(c, a, color);
        }
    }

    if (((m_terrainEditTarget == TerrainEditTarget::Vertex) && layerIndex == m_selectedVertex.layerIndex) ||
        ((m_terrainEditTarget == TerrainEditTarget::Face) && layerIndex == m_selectedCell.layerIndex))
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            return;
        }

        int cellX = 0;
        int cellZ = 0;
        GetSelectedCellCoords(layer, cellX, cellZ);

        const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);

        const XMFLOAT3 cellCenter(
            (p00.x + p10.x + p01.x + p11.x) * 0.25f,
            (p00.y + p10.y + p01.y + p11.y) * 0.25f + 0.02f,
            (p00.z + p10.z + p01.z + p11.z) * 0.25f);

        if (m_showFloorPreview)
        {
            DrawTransparentFloor3D(cellCenter,
                XMFLOAT2(layer.cellSize, layer.cellSize),
                XMFLOAT4(1.0f, 0.55f, 0.10f, 0.18f));
        }

        const XMFLOAT4 highlightColor = { 1.0f, 0.60f, 0.12f, 1.0f };
        Geometory::AddLine(p00, p10, highlightColor);
        Geometory::AddLine(p10, p11, highlightColor);
        Geometory::AddLine(p11, p01, highlightColor);
        Geometory::AddLine(p01, p00, highlightColor);
        Geometory::AddLine(p00, p11, { 1.0f, 0.75f, 0.25f, 0.65f });
        Geometory::AddLine(p10, p01, { 1.0f, 0.75f, 0.25f, 0.65f });
    }
}

// Highlight the terrain vertex currently under the cursor.
void SceneNarakuEditor::DrawHoveredVertexHighlight()
{

    if (m_editMode != EditMode::Terrain)
    {
        return;
    }

    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    int hoverGridX = -1;
    int hoverGridZ = -1;
    if (!PickTerrainVertex(GetMousePosition(), hoverGridX, hoverGridZ))
    {
        return;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    const XMFLOAT3 hoverPos = GetVertexWorldPosition(layer, hoverGridX, hoverGridZ);

    const bool isSelectedVertex =
        m_selectedVertex.layerIndex == m_selectedLayer &&
        m_selectedVertex.gridX == hoverGridX &&
        m_selectedVertex.gridZ == hoverGridZ;

    const XMFLOAT4 crossColor = isSelectedVertex
        ? XMFLOAT4(1.0f, 1.0f, 0.85f, 1.0f)
        : XMFLOAT4(0.35f, 1.0f, 0.35f, 1.0f);

    const float crossRadius = 0.22f;
    Geometory::AddLine(
        XMFLOAT3(hoverPos.x - crossRadius, hoverPos.y, hoverPos.z),
        XMFLOAT3(hoverPos.x + crossRadius, hoverPos.y, hoverPos.z),
        crossColor);
    Geometory::AddLine(
        XMFLOAT3(hoverPos.x, hoverPos.y, hoverPos.z - crossRadius),
        XMFLOAT3(hoverPos.x, hoverPos.y, hoverPos.z + crossRadius),
        crossColor);
    Geometory::AddLine(
        XMFLOAT3(hoverPos.x, hoverPos.y - crossRadius, hoverPos.z),
        XMFLOAT3(hoverPos.x, hoverPos.y + crossRadius, hoverPos.z),
        crossColor);

    DrawDebugBox3D(
        hoverPos,
        isSelectedVertex ? XMFLOAT3(0.12f, 0.12f, 0.12f) : XMFLOAT3(0.10f, 0.10f, 0.10f));
}

// Draw start and return point markers.
void SceneNarakuEditor::DrawStartAndReturnPoints()
{
    auto drawPoint = [this](const NarakuMap::LayerPoint& point, const XMFLOAT4& color, float heightOffset)
    {
        const int layerIndex = FindLayerIndexById(point.layerId);
        if (layerIndex < 0 || !m_mapData.terrainLayers[layerIndex].visible)
        {
            return;
        }
        const float depth = m_mapData.terrainLayers[layerIndex].layerDepth;
        const XMFLOAT3 base = { point.xz.x, ToWorldY(depth, heightOffset), point.xz.z };
        Geometory::AddLine(base, { base.x, base.y + 1.6f, base.z }, color);
        DrawDebugBox3D({ base.x, base.y + 0.22f, base.z }, { 0.30f, 0.30f, 0.30f });
    };

    drawPoint(
        m_mapData.playerStartPoint,
        (m_selectedLayerPoint == LayerPointSelectionTarget::PlayerStart) ? XMFLOAT4{ 0.95f, 1.00f, 0.32f, 1.0f } : XMFLOAT4{ 0.22f, 0.95f, 0.32f, 1.0f },
        0.10f);
    drawPoint(
        m_mapData.returnPoint,
        (m_selectedLayerPoint == LayerPointSelectionTarget::ReturnPoint) ? XMFLOAT4{ 1.00f, 0.95f, 0.32f, 1.0f } : XMFLOAT4{ 0.25f, 0.72f, 1.00f, 1.0f },
        0.10f);
}

// Draw colored boundary overlays for cell attributes.
void SceneNarakuEditor::DrawBoundaryOverlay(const NarakuMap::TerrainLayer& layer) const
{
    for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
    {
        for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
        {
            const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
            if (flags == NarakuMap::CellAttributeNone)
            {
                continue;
            }

            XMFLOAT4 color = { 0.90f, 0.50f, 0.20f, 0.90f };
            if ((flags & NarakuMap::CellAttributeRemoved) != 0u)
            {
                color = { 0.95f, 0.35f, 0.12f, 0.95f };
            }
            else if ((flags & NarakuMap::CellAttributeBlocked) != 0u)
            {
                color = { 1.00f, 0.28f, 0.22f, 0.95f };
            }
            else if ((flags & NarakuMap::CellAttributeHazard) != 0u)
            {
                color = { 1.00f, 0.20f, 0.82f, 0.95f };
            }
            else if ((flags & NarakuMap::CellAttributeRopeAnchor) != 0u)
            {
                color = { 1.00f, 0.86f, 0.18f, 0.95f };
            }
            else if ((flags & NarakuMap::CellAttributeDropAllowed) != 0u)
            {
                color = { 0.20f, 0.82f, 1.00f, 0.95f };
            }

            XMFLOAT3 a = GetVertexWorldPosition(layer, cellX, cellZ);
            XMFLOAT3 b = GetVertexWorldPosition(layer, cellX + 1, cellZ);
            XMFLOAT3 c = GetVertexWorldPosition(layer, cellX, cellZ + 1);
            XMFLOAT3 d = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
            a.y += 0.05f; b.y += 0.05f; c.y += 0.05f; d.y += 0.05f;
            Geometory::AddLine(a, b, color);
            Geometory::AddLine(b, d, color);
            Geometory::AddLine(d, c, color);
            Geometory::AddLine(c, a, color);
        }
    }
}

// Draw rope markers and the line that connects their layers.
void SceneNarakuEditor::DrawRopes()
{

    for (int i = 0; i < ToInt(m_mapData.ropes.size()); ++i)
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[i];
        const int topIndex = FindLayerIndexById(rope.topLayerId);
        const int bottomIndex = FindLayerIndexById(rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        const float topDepth = m_mapData.terrainLayers[topIndex].layerDepth;
        const float bottomDepth = m_mapData.terrainLayers[bottomIndex].layerDepth;
        const XMFLOAT3 top = { rope.xz.x, ToWorldY(topDepth, 1.1f), rope.xz.z };
        const XMFLOAT3 bottom = { rope.xz.x, ToWorldY(bottomDepth, 0.1f), rope.xz.z };

        XMFLOAT4 ropeColor = XMFLOAT4(0.92f, 0.64f, 0.18f, 1.0f);
        if (IsRopeMultiSelected(i))
        {
            ropeColor = XMFLOAT4(0.35f, 0.88f, 1.00f, 1.0f);
        }
        if (i == m_selectedRope)
        {
            ropeColor = XMFLOAT4(1.0f, 0.82f, 0.22f, 1.0f);
        }

        Geometory::AddLine(top, bottom, ropeColor);
        DrawDebugBox3D(top, { 0.22f, 0.22f, 0.22f });
        DrawDebugBox3D(bottom, { 0.18f, 0.18f, 0.18f });
    }
}

// Draw mining point markers in the preview world.
void SceneNarakuEditor::DrawMiningPoints()
{
    for (int i = 0; i < ToInt(m_mapData.miningPoints.size()); ++i)
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[i];
        const int layerIndex = FindLayerIndexById(point.layerId);
        if (layerIndex < 0 || !m_mapData.terrainLayers[layerIndex].visible)
        {
            continue;
        }

        const float depth = m_mapData.terrainLayers[layerIndex].layerDepth;
        const float width = 0.35f + 0.08f * static_cast<float>(ClampValue(point.visualType, 0, 3));
        const XMFLOAT3 base = { point.xz.x, ToWorldY(depth, 0.15f), point.xz.z };

        XMFLOAT4 color = point.discovered
            ? XMFLOAT4(0.20f, 0.88f, 0.95f, 1.0f)
            : XMFLOAT4(1.00f, 0.82f, 0.22f, 1.0f);
        if (!point.enabled)
        {
            color = { 0.45f, 0.45f, 0.45f, 1.0f };
        }
        if (IsMiningPointMultiSelected(i))
        {
            color = { 0.30f, 0.90f, 1.00f, 1.0f };
        }
        if (i == m_selectedMiningPoint)
        {
            color = { 1.00f, 0.36f, 0.22f, 1.0f };
        }

        DrawDebugBox3D({ base.x, base.y + 0.20f, base.z }, { width, 0.40f, width });
        Geometory::AddLine(base, { base.x, base.y + 1.4f, base.z }, color);
    }
}

// Draw a simple wire box in world space.
void SceneNarakuEditor::DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale) const
{

    const XMMATRIX scaling = XMMatrixScaling(scale.x, scale.y, scale.z);

    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);
    const XMMATRIX worldMatrix = scaling * translation;

    XMFLOAT4X4 world = {};
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);
    Geometory::DrawBox();
}

// Draw a tinted quad used for translucent floor previews.
void SceneNarakuEditor::DrawTransparentFloor3D(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color) const
{

    if (!m_debugWhiteTexture)
    {
        return;
    }

    Sprite::SetVertexShader(nullptr);
    Sprite::SetPixelShader(nullptr);

    Sprite::SetTexture(m_debugWhiteTexture);
    Sprite::SetSize(size);
    Sprite::SetOffset({ 0.0f, 0.0f });
    Sprite::SetUVPos({ 0.0f, 0.0f });
    Sprite::SetUVScale({ 1.0f, 1.0f });
    Sprite::SetColor(color);

    const XMMATRIX rotation = XMMatrixRotationX(XMConvertToRadians(90.0f));
    const XMMATRIX translation = XMMatrixTranslation(center.x, center.y, center.z);
    const XMMATRIX worldMatrix = rotation * translation;
    XMFLOAT4X4 world = {};
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Sprite::SetWorld(world);
    Sprite::Draw();
}

void SceneNarakuEditor::DrawTerrainCellPreview(
    const NarakuMap::TerrainLayer& layer,
    int cellX,
    int cellZ,
    const DirectX::XMFLOAT4& color) const
{
    const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
    const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
    const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
    const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
    const XMFLOAT3 center(
        (p00.x + p10.x + p01.x + p11.x) * 0.25f,
        (p00.y + p10.y + p01.y + p11.y) * 0.25f - 0.03f,
        (p00.z + p10.z + p01.z + p11.z) * 0.25f);
    DrawTransparentFloor3D(center, { layer.cellSize, layer.cellSize }, color);
}

// Convert layer depth and local height into world-space Y.
float SceneNarakuEditor::ToWorldY(float layerDepth, float height) const
{

    return height - layerDepth * kDepthScale;
}

// Convert a terrain grid vertex into world space.
DirectX::XMFLOAT3 SceneNarakuEditor::GetVertexWorldPosition(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const
{

    const float originX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float originZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);

    const float x = originX + static_cast<float>(gridX) * layer.cellSize;
    const float z = originZ + static_cast<float>(gridZ) * layer.cellSize;
    const float y = ToWorldY(layer.layerDepth, NarakuMap::GetVertexHeight(layer, gridX, gridZ));
    return { x, y, z };
}

// Return the preview floor center for one terrain layer.
DirectX::XMFLOAT3 SceneNarakuEditor::GetLayerFloorCenter(const NarakuMap::TerrainLayer& layer) const
{

    return { layer.center.x, ToWorldY(layer.layerDepth, -0.03f), layer.center.z };
}

// Return the preview floor size for one terrain layer.
DirectX::XMFLOAT2 SceneNarakuEditor::GetLayerFloorSize(const NarakuMap::TerrainLayer& layer) const
{

    const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
    const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
    return { std::max(width, layer.cellSize), std::max(height, layer.cellSize) };
}

// Resolve the selected cell coordinates for the active layer.
void SceneNarakuEditor::GetSelectedCellCoords(const NarakuMap::TerrainLayer& layer, int& outCellX, int& outCellZ) const
{
    if (m_selectedCell.layerIndex == m_selectedLayer)
    {
        outCellX = ClampValue(m_selectedCell.cellX, 0, std::max(0, layer.gridWidth - 2));
        outCellZ = ClampValue(m_selectedCell.cellZ, 0, std::max(0, layer.gridHeight - 2));
        return;
    }

    outCellX = ClampValue(m_selectedVertex.gridX, 0, std::max(0, layer.gridWidth - 2));
    outCellZ = ClampValue(m_selectedVertex.gridZ, 0, std::max(0, layer.gridHeight - 2));
}

// Return the average height of the selected terrain cell.
float SceneNarakuEditor::GetSelectedCellAverageHeight(const NarakuMap::TerrainLayer& layer) const
{
    int cellX = 0;
    int cellZ = 0;
    GetSelectedCellCoords(layer, cellX, cellZ);
    const float h00 = NarakuMap::GetVertexHeight(layer, cellX, cellZ);
    const float h10 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ);
    const float h01 = NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1);
    const float h11 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1);
    return (h00 + h10 + h01 + h11) * 0.25f;
}

// Offset the four vertices that make up a selected terrain cell.
void SceneNarakuEditor::ApplyHeightDeltaToCell(NarakuMap::TerrainLayer& layer, int cellX, int cellZ, float deltaHeight)
{
    NarakuMap::SetVertexHeight(layer, cellX, cellZ, NarakuMap::GetVertexHeight(layer, cellX, cellZ) + deltaHeight);
    NarakuMap::SetVertexHeight(layer, cellX + 1, cellZ, NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ) + deltaHeight);
    NarakuMap::SetVertexHeight(layer, cellX, cellZ + 1, NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1) + deltaHeight);
    NarakuMap::SetVertexHeight(layer, cellX + 1, cellZ + 1, NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1) + deltaHeight);
}

// Apply a height delta to the current vertex multi-selection.
void SceneNarakuEditor::ApplyHeightDeltaToSelectedVertices(float deltaHeight)
{
    if (m_multiSelectedVertices.empty())
    {
        if (m_selectedVertex.layerIndex >= 0 && m_selectedVertex.layerIndex < ToInt(m_mapData.terrainLayers.size()))
        {
            NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
            NarakuMap::SetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ, NarakuMap::GetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ) + deltaHeight);
        }
        return;
    }

    for (const VertexSelection& selection : m_multiSelectedVertices)
    {
        if (selection.layerIndex >= 0 && selection.layerIndex < ToInt(m_mapData.terrainLayers.size()))
        {
            NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[selection.layerIndex];
            NarakuMap::SetVertexHeight(layer, selection.gridX, selection.gridZ, NarakuMap::GetVertexHeight(layer, selection.gridX, selection.gridZ) + deltaHeight);
        }
    }
}

// Apply a height delta to the current face multi-selection.
void SceneNarakuEditor::ApplyHeightDeltaToSelectedCells(float deltaHeight)
{
    if (m_multiSelectedCells.empty())
    {
        if (m_selectedCell.layerIndex >= 0 && m_selectedCell.layerIndex < ToInt(m_mapData.terrainLayers.size()))
        {
            NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedCell.layerIndex];
            ApplyHeightDeltaToCell(layer, m_selectedCell.cellX, m_selectedCell.cellZ, deltaHeight);
        }
        return;
    }

    for (const CellSelection& selection : m_multiSelectedCells)
    {
        if (selection.layerIndex >= 0 && selection.layerIndex < ToInt(m_mapData.terrainLayers.size()))
        {
            NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[selection.layerIndex];
            ApplyHeightDeltaToCell(layer, selection.cellX, selection.cellZ, deltaHeight);
        }
    }
}

// Clear all multi-selection buffers.
void SceneNarakuEditor::ClearMultiSelection()
{
    m_multiSelectedVertices.clear();
    m_multiSelectedCells.clear();
    m_multiSelectedRopes.clear();
    m_multiSelectedMiningPoints.clear();
}

// Add one vertex to the current multi-selection set.
void SceneNarakuEditor::AddSelectedVertex(int layerIndex, int gridX, int gridZ)
{
    if (layerIndex < 0 || layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    if (!NarakuMap::IsVertexEnabled(m_mapData.terrainLayers[layerIndex], gridX, gridZ))
    {
        return;
    }

    for (const VertexSelection& selection : m_multiSelectedVertices)
    {
        if (selection.layerIndex == layerIndex && selection.gridX == gridX && selection.gridZ == gridZ)
        {
            return;
        }
    }
    m_multiSelectedVertices.push_back({ layerIndex, gridX, gridZ });
}

// Add one cell to the current multi-selection set.
void SceneNarakuEditor::AddSelectedCell(int layerIndex, int cellX, int cellZ)
{
    for (const CellSelection& selection : m_multiSelectedCells)
    {
        if (selection.layerIndex == layerIndex && selection.cellX == cellX && selection.cellZ == cellZ)
        {
            return;
        }
    }
    m_multiSelectedCells.push_back({ layerIndex, cellX, cellZ });
}

// Add one rope to the current multi-selection set.
void SceneNarakuEditor::AddSelectedRope(int ropeIndex)
{
    if (ropeIndex < 0 || ropeIndex >= ToInt(m_mapData.ropes.size()))
    {
        return;
    }

    if (std::find(m_multiSelectedRopes.begin(), m_multiSelectedRopes.end(), ropeIndex) != m_multiSelectedRopes.end())
    {
        return;
    }

    m_multiSelectedRopes.push_back(ropeIndex);
}

// Add one mining point to the current multi-selection set.
void SceneNarakuEditor::AddSelectedMiningPoint(int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= ToInt(m_mapData.miningPoints.size()))
    {
        return;
    }

    if (std::find(m_multiSelectedMiningPoints.begin(), m_multiSelectedMiningPoints.end(), pointIndex) != m_multiSelectedMiningPoints.end())
    {
        return;
    }

    m_multiSelectedMiningPoints.push_back(pointIndex);
}

// Test whether a vertex is currently inside the multi-selection set.
bool SceneNarakuEditor::IsVertexMultiSelected(int layerIndex, int gridX, int gridZ) const
{
    for (const VertexSelection& selection : m_multiSelectedVertices)
    {
        if (selection.layerIndex == layerIndex && selection.gridX == gridX && selection.gridZ == gridZ)
        {
            return true;
        }
    }
    return false;
}

// Test whether a cell is currently inside the multi-selection set.
bool SceneNarakuEditor::IsCellMultiSelected(int layerIndex, int cellX, int cellZ) const
{
    for (const CellSelection& selection : m_multiSelectedCells)
    {
        if (selection.layerIndex == layerIndex && selection.cellX == cellX && selection.cellZ == cellZ)
        {
            return true;
        }
    }
    return false;
}

// Test whether a rope is currently inside the multi-selection set.
bool SceneNarakuEditor::IsRopeMultiSelected(int ropeIndex) const
{
    return std::find(m_multiSelectedRopes.begin(), m_multiSelectedRopes.end(), ropeIndex) != m_multiSelectedRopes.end();
}

// Test whether a mining point is currently inside the multi-selection set.
bool SceneNarakuEditor::IsMiningPointMultiSelected(int pointIndex) const
{
    return std::find(m_multiSelectedMiningPoints.begin(), m_multiSelectedMiningPoints.end(), pointIndex) != m_multiSelectedMiningPoints.end();
}

bool SceneNarakuEditor::IsCellRemoved(const NarakuMap::TerrainLayer& layer, int cellX, int cellZ) const
{
    return (NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ) & NarakuMap::CellAttributeRemoved) != 0u;
}

bool SceneNarakuEditor::CanDeleteCell(
    const NarakuMap::TerrainLayer& layer,
    int cellX,
    int cellZ,
    const std::vector<CellSelection>& removingCells) const
{
    if (cellX < 0 || cellZ < 0 || cellX >= layer.gridWidth - 1 || cellZ >= layer.gridHeight - 1)
    {
        return false;
    }

    if (IsCellRemoved(layer, cellX, cellZ))
    {
        return false;
    }

    const bool isOuterBorder =
        cellX == 0 ||
        cellZ == 0 ||
        cellX == layer.gridWidth - 2 ||
        cellZ == layer.gridHeight - 2;
    if (isOuterBorder)
    {
        return true;
    }

    auto isPlannedRemoval = [this, &removingCells](int targetLayerIndex, int targetCellX, int targetCellZ) -> bool
    {
        for (const CellSelection& selection : removingCells)
        {
            if (selection.layerIndex == targetLayerIndex &&
                selection.cellX == targetCellX &&
                selection.cellZ == targetCellZ)
            {
                return true;
            }
        }
        return false;
    };

    return IsCellRemoved(layer, cellX - 1, cellZ) || isPlannedRemoval(m_selectedLayer, cellX - 1, cellZ) ||
        IsCellRemoved(layer, cellX + 1, cellZ) || isPlannedRemoval(m_selectedLayer, cellX + 1, cellZ) ||
        IsCellRemoved(layer, cellX, cellZ - 1) || isPlannedRemoval(m_selectedLayer, cellX, cellZ - 1) ||
        IsCellRemoved(layer, cellX, cellZ + 1) || isPlannedRemoval(m_selectedLayer, cellX, cellZ + 1);
}

bool SceneNarakuEditor::CanDeleteVertex(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const
{
    if (!NarakuMap::IsVertexEnabled(layer, gridX, gridZ))
    {
        return false;
    }

    const int adjacentOffsets[4][2] =
    {
        { -1, -1 },
        {  0, -1 },
        { -1,  0 },
        {  0,  0 }
    };

    for (int offsetIndex = 0; offsetIndex < 4; ++offsetIndex)
    {
        const int cellX = gridX + adjacentOffsets[offsetIndex][0];
        const int cellZ = gridZ + adjacentOffsets[offsetIndex][1];
        if (cellX < 0 || cellZ < 0 || cellX >= layer.gridWidth - 1 || cellZ >= layer.gridHeight - 1)
        {
            continue;
        }

        if (!IsCellRemoved(layer, cellX, cellZ))
        {
            return false;
        }
    }

    return true;
}

// Apply the current marquee rectangle to terrain multi-selection.
void SceneNarakuEditor::ApplyMarqueeSelection()
{
    ClearMultiSelection();

    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    const int minX = std::min(m_marqueeStart.x, m_marqueeEnd.x);
    const int maxX = std::max(m_marqueeStart.x, m_marqueeEnd.x);
    const int minY = std::min(m_marqueeStart.y, m_marqueeEnd.y);
    const int maxY = std::max(m_marqueeStart.y, m_marqueeEnd.y);
    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];

    if (m_terrainEditTarget == TerrainEditTarget::Vertex)
    {
        for (int gridZ = 0; gridZ < layer.gridHeight; ++gridZ)
        {
            for (int gridX = 0; gridX < layer.gridWidth; ++gridX)
            {
                XMFLOAT2 screen = {};
                if (!ProjectWorldToScreen(GetVertexWorldPosition(layer, gridX, gridZ), screen))
                {
                    continue;
                }
                if (screen.x >= minX && screen.x <= maxX && screen.y >= minY && screen.y <= maxY)
                {
                    AddSelectedVertex(m_selectedLayer, gridX, gridZ);
                }
            }
        }

        if (!m_multiSelectedVertices.empty())
        {
            m_selectedVertex = m_multiSelectedVertices.front();
            m_selectedCell = { m_selectedVertex.layerIndex, ClampValue(m_selectedVertex.gridX, 0, std::max(0, layer.gridWidth - 2)), ClampValue(m_selectedVertex.gridZ, 0, std::max(0, layer.gridHeight - 2)) };
        }
    }
    else
    {
        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
                const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
                const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
                const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
                const XMFLOAT3 center(
                    (p00.x + p10.x + p01.x + p11.x) * 0.25f,
                    (p00.y + p10.y + p01.y + p11.y) * 0.25f,
                    (p00.z + p10.z + p01.z + p11.z) * 0.25f);
                XMFLOAT2 screen = {};
                if (!ProjectWorldToScreen(center, screen))
                {
                    continue;
                }
                if (screen.x >= minX && screen.x <= maxX && screen.y >= minY && screen.y <= maxY)
                {
                    AddSelectedCell(m_selectedLayer, cellX, cellZ);
                }
            }
        }

        if (!m_multiSelectedCells.empty())
        {
            m_selectedCell = m_multiSelectedCells.front();
            m_selectedVertex = { m_selectedCell.layerIndex, m_selectedCell.cellX, m_selectedCell.cellZ };
        }
    }

    if (m_autoFocusSelection)
    {
        FocusSelection();
    }
}

// Snap an editable XZ point to the selected layer grid.
void SceneNarakuEditor::ApplySnapToPoint(NarakuMap::Vec2& pointXZ, int layerId, bool snapToCellCenter) const
{
    const int layerIndex = FindLayerIndexById(layerId);
    if (layerIndex < 0 || layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[layerIndex];
    const float originX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float originZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float offset = snapToCellCenter ? (layer.cellSize * 0.5f) : 0.0f;
    const float snappedX = std::round((pointXZ.x - originX - offset) / layer.cellSize) * layer.cellSize + originX + offset;
    const float snappedZ = std::round((pointXZ.z - originZ - offset) / layer.cellSize) * layer.cellSize + originZ + offset;
    pointXZ.x = snappedX;
    pointXZ.z = snappedZ;
}

// Project the mouse cursor onto a layer-aligned horizontal plane.
bool SceneNarakuEditor::ProjectMouseToLayerPlane(POINT mousePos, float layerDepth, NarakuMap::Vec2& outPointXZ) const
{
    const DirectX::XMFLOAT2 viewportSize = GetEditorViewportSize();
    const XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    const XMMATRIX world = XMMatrixIdentity();

    const XMVECTOR nearPoint = XMVector3Unproject(
        XMVectorSet(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y), 0.0f, 1.0f),
        0.0f,
        0.0f,
        viewportSize.x,
        viewportSize.y,
        0.0f,
        1.0f,
        projection,
        view,
        world);

    const XMVECTOR farPoint = XMVector3Unproject(
        XMVectorSet(static_cast<float>(mousePos.x), static_cast<float>(mousePos.y), 1.0f, 1.0f),
        0.0f,
        0.0f,
        viewportSize.x,
        viewportSize.y,
        0.0f,
        1.0f,
        projection,
        view,
        world);

    const XMVECTOR direction = farPoint - nearPoint;
    const float directionY = XMVectorGetY(direction);
    if (std::fabs(directionY) < 0.0001f)
    {
        return false;
    }

    const float planeY = ToWorldY(layerDepth, 0.0f);
    const float t = (planeY - XMVectorGetY(nearPoint)) / directionY;
    if (t < 0.0f)
    {
        return false;
    }

    const XMVECTOR hitPoint = nearPoint + direction * t;
    outPointXZ.x = XMVectorGetX(hitPoint);
    outPointXZ.z = XMVectorGetZ(hitPoint);
    return true;
}

// Move an XZ point by projecting the cursor onto the chosen layer plane.
void SceneNarakuEditor::DragPointOnLayerPlane(NarakuMap::Vec2& pointXZ, int layerId)
{
    const int layerIndex = FindLayerIndexById(layerId);
    if (layerIndex < 0 || layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    NarakuMap::Vec2 projectedPoint = {};
    if (!ProjectMouseToLayerPlane(GetMousePosition(), m_mapData.terrainLayers[layerIndex].layerDepth, projectedPoint))
    {
        return;
    }

    pointXZ = projectedPoint;
    if (m_enablePositionSnap)
    {
        ApplySnapToPoint(pointXZ, layerId, m_snapToCellCenter);
    }
}

// Project a world-space point into screen coordinates.
bool SceneNarakuEditor::ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPos, DirectX::XMFLOAT2& outScreen) const
{

    const XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    XMVECTOR clip = XMVector3TransformCoord(XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f), view * projection);
    XMFLOAT3 ndc = {};
    XMStoreFloat3(&ndc, clip);

    if (ndc.z < 0.0f || ndc.z > 1.0f)
    {
        return false;
    }

    const DirectX::XMFLOAT2 viewportSize = GetEditorViewportSize();
    outScreen.x = (ndc.x * 0.5f + 0.5f) * viewportSize.x;
    outScreen.y = (-ndc.y * 0.5f + 0.5f) * viewportSize.y;
    return true;
}

// Pick the nearest visible terrain vertex in screen space.
bool SceneNarakuEditor::PickTerrainVertex(POINT mousePos, int& outGridX, int& outGridZ) const
{

    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    float bestDistance = kPickThresholdPx;
    bool found = false;

    for (int gridZ = 0; gridZ < layer.gridHeight; ++gridZ)
    {
        for (int gridX = 0; gridX < layer.gridWidth; ++gridX)
        {
            if (!NarakuMap::IsVertexEnabled(layer, gridX, gridZ))
            {
                continue;
            }

            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(GetVertexWorldPosition(layer, gridX, gridZ), screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distance = Length2D(dx, dy);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outGridX = gridX;
                outGridZ = gridZ;
                found = true;
            }
        }
    }

    return found;
}

// Pick the nearest visible terrain cell center in screen space.
bool SceneNarakuEditor::PickTerrainCell(POINT mousePos, int& outCellX, int& outCellZ) const
{
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    if (layer.gridWidth < 2 || layer.gridHeight < 2)
    {
        return false;
    }

    float bestDistance = kPickThresholdPx * 1.5f;
    bool found = false;

    for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
    {
        for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
        {
            const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
            const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
            const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
            const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
            const XMFLOAT3 center(
                (p00.x + p10.x + p01.x + p11.x) * 0.25f,
                (p00.y + p10.y + p01.y + p11.y) * 0.25f,
                (p00.z + p10.z + p01.z + p11.z) * 0.25f);

            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(center, screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distance = Length2D(dx, dy);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outCellX = cellX;
                outCellZ = cellZ;
                found = true;
            }
        }
    }

    return found;
}

// Pick the nearest rope handle in screen space.
int SceneNarakuEditor::PickRope(POINT mousePos) const
{

    float bestDistance = kPickThresholdPx;
    int bestIndex = -1;
    for (int i = 0; i < ToInt(m_mapData.ropes.size()); ++i)
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[i];
        const int topIndex = FindLayerIndexById(rope.topLayerId);
        const int bottomIndex = FindLayerIndexById(rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        const float focusDepth = (m_mapData.terrainLayers[topIndex].layerDepth + m_mapData.terrainLayers[bottomIndex].layerDepth) * 0.5f;
        XMFLOAT2 screen = {};
        const XMFLOAT3 focusPos(rope.xz.x, ToWorldY(focusDepth, 0.8f), rope.xz.z);
        if (!ProjectWorldToScreen(focusPos, screen))
        {
            continue;
        }

        const float dx = static_cast<float>(mousePos.x) - screen.x;
        const float dy = static_cast<float>(mousePos.y) - screen.y;
        const float distance = Length2D(dx, dy);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// Pick the nearest mining point marker in screen space.
int SceneNarakuEditor::PickMiningPoint(POINT mousePos) const
{

    float bestDistance = kPickThresholdPx;
    int bestIndex = -1;
    for (int i = 0; i < ToInt(m_mapData.miningPoints.size()); ++i)
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[i];
        const int layerIndex = FindLayerIndexById(point.layerId);
        if (layerIndex < 0)
        {
            continue;
        }

        XMFLOAT2 screen = {};
        const XMFLOAT3 focusPos(point.xz.x, ToWorldY(m_mapData.terrainLayers[layerIndex].layerDepth, 0.4f), point.xz.z);
        if (!ProjectWorldToScreen(focusPos, screen))
        {
            continue;
        }

        const float dx = static_cast<float>(mousePos.x) - screen.x;
        const float dy = static_cast<float>(mousePos.y) - screen.y;
        const float distance = Length2D(dx, dy);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// Test whether the cursor is near the selected vertex handle.
bool SceneNarakuEditor::IsMouseNearSelectedVertexHandle(POINT mousePos) const
{

    if (m_selectedVertex.layerIndex < 0 ||
        m_selectedVertex.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
    const XMFLOAT3 vertex = GetVertexWorldPosition(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
    const XMFLOAT3 handle = { vertex.x, vertex.y + 2.0f, vertex.z };
    XMFLOAT2 screen = {};
    if (!ProjectWorldToScreen(handle, screen))
    {
        return false;
    }

    const float dx = static_cast<float>(mousePos.x) - screen.x;
    const float dy = static_cast<float>(mousePos.y) - screen.y;
    return Length2D(dx, dy) <= kPickThresholdPx;
}

// Test whether the cursor is near the selected face handle.
bool SceneNarakuEditor::IsMouseNearSelectedCellHandle(POINT mousePos) const
{
    if (m_selectedCell.layerIndex < 0 ||
        m_selectedCell.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedCell.layerIndex];
    if (layer.gridWidth < 2 || layer.gridHeight < 2)
    {
        return false;
    }

    int cellX = 0;
    int cellZ = 0;
    GetSelectedCellCoords(layer, cellX, cellZ);
    const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
    const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
    const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
    const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);
    const XMFLOAT3 handle(
        (p00.x + p10.x + p01.x + p11.x) * 0.25f,
        (p00.y + p10.y + p01.y + p11.y) * 0.25f + 2.0f,
        (p00.z + p10.z + p01.z + p11.z) * 0.25f);

    XMFLOAT2 screen = {};
    if (!ProjectWorldToScreen(handle, screen))
    {
        return false;
    }

    const float dx = static_cast<float>(mousePos.x) - screen.x;
    const float dy = static_cast<float>(mousePos.y) - screen.y;
    return Length2D(dx, dy) <= kPickThresholdPx;
}

// Return the depth currently used to drive foreground fading.
float SceneNarakuEditor::GetFocusDepth() const
{

    if (m_editMode == EditMode::Terrain &&
        m_selectedLayer >= 0 &&
        m_selectedLayer < ToInt(m_mapData.terrainLayers.size()))
    {
        return m_mapData.terrainLayers[m_selectedLayer].layerDepth;
    }

    if (m_editMode == EditMode::Rope &&
        m_selectedRope >= 0 &&
        m_selectedRope < ToInt(m_mapData.ropes.size()))
    {
        const int bottomIndex = FindLayerIndexById(m_mapData.ropes[m_selectedRope].bottomLayerId);
        if (bottomIndex >= 0)
        {
            return m_mapData.terrainLayers[bottomIndex].layerDepth;
        }
    }

    if (m_editMode == EditMode::MiningPoint &&
        m_selectedMiningPoint >= 0 &&
        m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
    {
        const int layerIndex = FindLayerIndexById(m_mapData.miningPoints[m_selectedMiningPoint].layerId);
        if (layerIndex >= 0)
        {
            return m_mapData.terrainLayers[layerIndex].layerDepth;
        }
    }

    return m_mapData.terrainLayers.empty() ? 0.0f : m_mapData.terrainLayers.front().layerDepth;
}

// Choose a display alpha for a layer based on the focus depth.
float SceneNarakuEditor::GetLayerAlpha(const NarakuMap::TerrainLayer& layer) const
{

    return (layer.layerDepth < GetFocusDepth()) ? m_frontLayerAlpha : 0.32f;
}

// Return the UI label for a ground texture slot.
const char* SceneNarakuEditor::GetGroundTextureLabel(int textureId) const
{

    return kGroundTextureLabels[ClampValue(textureId, 0, 3)];
}

// Return a temporary color tint for the preview floor.
DirectX::XMFLOAT4 SceneNarakuEditor::GetGroundTextureTint(int textureId) const
{

    switch (ClampValue(textureId, 0, 3))
    {
    case 1: return { 0.44f, 0.31f, 0.18f, 0.32f };
    case 2: return { 0.36f, 0.40f, 0.46f, 0.32f };
    case 3: return { 0.26f, 0.42f, 0.35f, 0.32f };
    default: return { 0.23f, 0.48f, 0.28f, 0.32f };
    }
}

// Convert a UTF-16 string to UTF-8 for ImGui display.
std::string SceneNarakuEditor::WideToUtf8(const wchar_t* text) const
{

    if (!text)
    {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1)
    {
        return {};
    }

    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], required, nullptr, nullptr);
    result.pop_back();
    return result;
}

void SceneNarakuEditor::AppendOperationLog(const std::string& message)
{
    if (message.empty())
    {
        return;
    }

    m_operationLogs.push_back(message);
    if (m_operationLogs.size() > 120)
    {
        m_operationLogs.erase(m_operationLogs.begin(), m_operationLogs.begin() + (m_operationLogs.size() - 120));
    }
}

void SceneNarakuEditor::InvalidateValidationCache()
{
    m_validationDirty = true;
}

const std::vector<NarakuMap::ValidationIssue>& SceneNarakuEditor::GetValidationIssues()
{
    if (m_validationDirty)
    {
        m_cachedValidationIssues = NarakuMap::ValidateMapData(m_mapData);
        m_validationDirty = false;
    }
    return m_cachedValidationIssues;
}

bool SceneNarakuEditor::ShouldDrawDetailedFloorPreview(const NarakuMap::TerrainLayer& layer, int layerIndex) const
{
    if (!m_showFloorPreview || !m_useDetailedFloorPreview)
    {
        return false;
    }

    if (layerIndex != m_selectedLayer)
    {
        return false;
    }

    const int cellCount = std::max(0, layer.gridWidth - 1) * std::max(0, layer.gridHeight - 1);
    return cellCount <= m_detailedFloorPreviewCellLimit;
}

// Find the terrain layer index that matches a layer ID.
int SceneNarakuEditor::FindLayerIndexById(int layerId) const
{

    for (int i = 0; i < ToInt(m_mapData.terrainLayers.size()); ++i)
    {
        if (m_mapData.terrainLayers[i].id == layerId)
        {
            return i;
        }
    }
    return -1;
}

// Create a default terrain layer for the editor.
NarakuMap::LayerPoint* SceneNarakuEditor::GetSelectedLayerPoint()
{
    switch (m_selectedLayerPoint)
    {
    case LayerPointSelectionTarget::PlayerStart:
        return &m_mapData.playerStartPoint;
    case LayerPointSelectionTarget::ReturnPoint:
        return &m_mapData.returnPoint;
    default:
        return nullptr;
    }
}

const NarakuMap::LayerPoint* SceneNarakuEditor::GetSelectedLayerPoint() const
{
    switch (m_selectedLayerPoint)
    {
    case LayerPointSelectionTarget::PlayerStart:
        return &m_mapData.playerStartPoint;
    case LayerPointSelectionTarget::ReturnPoint:
        return &m_mapData.returnPoint;
    default:
        return nullptr;
    }
}

bool SceneNarakuEditor::PickLayerPoint(POINT mousePos, LayerPointSelectionTarget& outTarget) const
{
    outTarget = LayerPointSelectionTarget::None;
    float bestDistance = kPickThresholdPx;

    auto tryPoint = [this, mousePos, &outTarget, &bestDistance](const NarakuMap::LayerPoint& point, LayerPointSelectionTarget target)
    {
        const int layerIndex = FindLayerIndexById(point.layerId);
        if (layerIndex < 0 || layerIndex >= ToInt(m_mapData.terrainLayers.size()) || !m_mapData.terrainLayers[layerIndex].visible)
        {
            return;
        }

        const float depth = m_mapData.terrainLayers[layerIndex].layerDepth;
        const XMFLOAT3 world = { point.xz.x, ToWorldY(depth, 0.35f), point.xz.z };
        XMFLOAT2 screen = {};
        if (!ProjectWorldToScreen(world, screen))
        {
            return;
        }

        const float distance = Length2D(screen.x - static_cast<float>(mousePos.x), screen.y - static_cast<float>(mousePos.y));
        if (distance <= bestDistance)
        {
            bestDistance = distance;
            outTarget = target;
        }
    };

    tryPoint(m_mapData.playerStartPoint, LayerPointSelectionTarget::PlayerStart);
    tryPoint(m_mapData.returnPoint, LayerPointSelectionTarget::ReturnPoint);
    return outTarget != LayerPointSelectionTarget::None;
}
NarakuMap::TerrainLayer SceneNarakuEditor::CreateNewLayer() const
{
    NarakuMap::TerrainLayer layer = {};
    layer.id = m_mapData.terrainLayers.empty() ? 0 : (m_mapData.terrainLayers.back().id + 1);
    layer.center = { 0.0f, 0.0f };
    layer.layerDepth = m_mapData.terrainLayers.empty() ? 0.0f : (m_mapData.terrainLayers.back().layerDepth + 4.0f);
    layer.gridWidth = 32;
    layer.gridHeight = 32;
    layer.cellSize = 1.0f;
    layer.groundTextureId = 0;
    layer.visible = true;
    layer.locked = false;
    NarakuMap::EnsureLayerHeights(layer);
    NarakuMap::EnsureLayerVertexEnabled(layer);
    NarakuMap::EnsureLayerCellAttributes(layer);
    return layer;
}

// Create a default rope that links the outermost layers.
NarakuMap::RopePoint SceneNarakuEditor::CreateNewRope() const
{

    NarakuMap::RopePoint rope = {};
    rope.xz = { 0.0f, 0.0f };
    if (!m_mapData.terrainLayers.empty())
    {
        rope.topLayerId = m_mapData.terrainLayers.front().id;
        rope.bottomLayerId = m_mapData.terrainLayers.back().id;
    }
    return rope;
}

// Create a default mining point on the selected layer.
NarakuMap::MiningPoint SceneNarakuEditor::CreateNewMiningPoint() const
{
    NarakuMap::MiningPoint point = {};
    point.xz = { 0.0f, 0.0f };
    point.layerId = (m_selectedLayer >= 0 && m_selectedLayer < ToInt(m_mapData.terrainLayers.size()))
        ? m_mapData.terrainLayers[m_selectedLayer].id
        : 0;
    point.visualType = 0;
    point.discovered = false;
    point.enabled = true;
    point.respawnCandidate = false;
    point.relicName.clear();
    return point;
}



SceneNarakuEditor::EditorSnapshot SceneNarakuEditor::MakeSnapshot() const
{
    EditorSnapshot snapshot = {};
    snapshot.mapData = m_mapData;
    snapshot.editMode = m_editMode;
    snapshot.terrainEditTarget = m_terrainEditTarget;
    snapshot.selectedLayer = m_selectedLayer;
    snapshot.selectedRope = m_selectedRope;
    snapshot.selectedMiningPoint = m_selectedMiningPoint;
    snapshot.selectedLayerPoint = m_selectedLayerPoint;
    snapshot.selectedVertex = m_selectedVertex;
    snapshot.selectedCell = m_selectedCell;
    snapshot.multiSelectedVertices = m_multiSelectedVertices;
    snapshot.multiSelectedCells = m_multiSelectedCells;
    snapshot.multiSelectedRopes = m_multiSelectedRopes;
    snapshot.multiSelectedMiningPoints = m_multiSelectedMiningPoints;
    return snapshot;
}

void SceneNarakuEditor::RestoreSnapshot(const EditorSnapshot& snapshot)
{
    m_mapData = snapshot.mapData;
    m_editMode = snapshot.editMode;
    m_terrainEditTarget = snapshot.terrainEditTarget;
    m_selectedLayer = snapshot.selectedLayer;
    m_selectedRope = snapshot.selectedRope;
    m_selectedMiningPoint = snapshot.selectedMiningPoint;
    m_selectedLayerPoint = snapshot.selectedLayerPoint;
    m_selectedVertex = snapshot.selectedVertex;
    m_selectedCell = snapshot.selectedCell;
    m_multiSelectedVertices = snapshot.multiSelectedVertices;
    m_multiSelectedCells = snapshot.multiSelectedCells;
    m_multiSelectedRopes = snapshot.multiSelectedRopes;
    m_multiSelectedMiningPoints = snapshot.multiSelectedMiningPoints;
    m_draggingVertexHeight = false;
    m_draggingCellHeight = false;
    m_draggingRopePosition = false;
    m_draggingMiningPointPosition = false;
    m_draggingLayerPointPosition = false;
    m_armLayerPointPlacement = false;
    EnsureSelectionValid();
    FocusSelection();
}

void SceneNarakuEditor::PushUndoSnapshot()
{
    m_undoStack.push_back(MakeSnapshot());
    if (m_undoStack.size() > 64)
    {
        m_undoStack.erase(m_undoStack.begin());
    }
    m_redoStack.clear();
    InvalidateValidationCache();
}

bool SceneNarakuEditor::Undo()
{
    if (m_undoStack.empty())
    {
        return false;
    }

    m_redoStack.push_back(MakeSnapshot());
    const EditorSnapshot snapshot = m_undoStack.back();
    m_undoStack.pop_back();
    RestoreSnapshot(snapshot);
    InvalidateValidationCache();
    m_lastIoMessage = u8"Undo を実行しました";
    AppendOperationLog(m_lastIoMessage);
    return true;
}

bool SceneNarakuEditor::Redo()
{
    if (m_redoStack.empty())
    {
        return false;
    }

    m_undoStack.push_back(MakeSnapshot());
    const EditorSnapshot snapshot = m_redoStack.back();
    m_redoStack.pop_back();
    RestoreSnapshot(snapshot);
    InvalidateValidationCache();
    m_lastIoMessage = u8"Redo を実行しました";
    AppendOperationLog(m_lastIoMessage);
    return true;
}

bool SceneNarakuEditor::DeleteSelectedTerrainCells()
{
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    if (layer.locked)
    {
        m_lastIoMessage = u8"編集ロック中のレイヤーではセル削除できません。";
        return false;
    }

    std::vector<CellSelection> removingCells = m_multiSelectedCells;
    if (removingCells.empty())
    {
        removingCells.push_back({ m_selectedLayer, m_selectedCell.cellX, m_selectedCell.cellZ });
    }

    std::vector<CellSelection> pendingCells = removingCells;
    std::vector<CellSelection> resolvedCells;
    auto isResolved = [&resolvedCells](int cellX, int cellZ) -> bool
    {
        for (const CellSelection& selection : resolvedCells)
        {
            if (selection.cellX == cellX && selection.cellZ == cellZ)
            {
                return true;
            }
        }
        return false;
    };

    while (!pendingCells.empty())
    {
        bool removedAnyCell = false;
        for (auto it = pendingCells.begin(); it != pendingCells.end();)
        {
            const bool isOuterBorder =
                it->cellX == 0 ||
                it->cellZ == 0 ||
                it->cellX == layer.gridWidth - 2 ||
                it->cellZ == layer.gridHeight - 2;

            const bool touchesRemovedBoundary =
                IsCellRemoved(layer, it->cellX - 1, it->cellZ) || isResolved(it->cellX - 1, it->cellZ) ||
                IsCellRemoved(layer, it->cellX + 1, it->cellZ) || isResolved(it->cellX + 1, it->cellZ) ||
                IsCellRemoved(layer, it->cellX, it->cellZ - 1) || isResolved(it->cellX, it->cellZ - 1) ||
                IsCellRemoved(layer, it->cellX, it->cellZ + 1) || isResolved(it->cellX, it->cellZ + 1);

            if (it->layerIndex != m_selectedLayer || IsCellRemoved(layer, it->cellX, it->cellZ) || (!isOuterBorder && !touchesRemovedBoundary))
            {
                ++it;
                continue;
            }

            resolvedCells.push_back(*it);
            it = pendingCells.erase(it);
            removedAnyCell = true;
        }

        if (!removedAnyCell)
        {
            m_lastIoMessage = u8"この面は削除できません。外周セルから連続する形でだけ削除できます。";
            return false;
        }
    }

    for (const CellSelection& selection : resolvedCells)
    {
        const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, selection.cellX, selection.cellZ);
        NarakuMap::SetCellAttributeFlags(layer, selection.cellX, selection.cellZ, flags | NarakuMap::CellAttributeRemoved);
    }

    m_lastIoMessage = u8"選択面を削除しました。";
    AppendOperationLog(m_lastIoMessage);
    return true;
}

bool SceneNarakuEditor::DeleteSelectedTerrainVertices()
{
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    if (layer.locked)
    {
        m_lastIoMessage = u8"編集ロック中のレイヤーでは頂点削除できません。";
        return false;
    }

    std::vector<VertexSelection> removingVertices = m_multiSelectedVertices;
    if (removingVertices.empty())
    {
        removingVertices.push_back(m_selectedVertex);
    }

    for (const VertexSelection& selection : removingVertices)
    {
        if (selection.layerIndex != m_selectedLayer || !CanDeleteVertex(layer, selection.gridX, selection.gridZ))
        {
            m_lastIoMessage = u8"この頂点は削除できません。先に周囲の面を削除して孤立頂点にしてください。";
            return false;
        }
    }

    for (const VertexSelection& selection : removingVertices)
    {
        NarakuMap::SetVertexEnabled(layer, selection.gridX, selection.gridZ, false);
    }

    EnsureSelectionValid();
    m_lastIoMessage = u8"孤立頂点を削除しました。";
    AppendOperationLog(m_lastIoMessage);
    return true;
}

bool SceneNarakuEditor::RestoreSelectedTerrainGeometry()
{
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    if (m_terrainEditTarget == TerrainEditTarget::Face)
    {
        bool restoredAnyCell = false;
        auto restoreCell = [&restoredAnyCell](NarakuMap::TerrainLayer& targetLayer, int targetCellX, int targetCellZ)
        {
            const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(targetLayer, targetCellX, targetCellZ);
            if ((flags & NarakuMap::CellAttributeRemoved) == 0u)
            {
                return;
            }

            NarakuMap::SetCellAttributeFlags(targetLayer, targetCellX, targetCellZ, flags & ~NarakuMap::CellAttributeRemoved);
            NarakuMap::SetVertexEnabled(targetLayer, targetCellX, targetCellZ, true);
            NarakuMap::SetVertexEnabled(targetLayer, targetCellX + 1, targetCellZ, true);
            NarakuMap::SetVertexEnabled(targetLayer, targetCellX, targetCellZ + 1, true);
            NarakuMap::SetVertexEnabled(targetLayer, targetCellX + 1, targetCellZ + 1, true);
            restoredAnyCell = true;
        };

        if (m_multiSelectedCells.empty())
        {
            restoreCell(m_mapData.terrainLayers[m_selectedLayer], m_selectedCell.cellX, m_selectedCell.cellZ);
        }
        else
        {
            for (const CellSelection& selection : m_multiSelectedCells)
            {
                if (selection.layerIndex < 0 || selection.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
                {
                    continue;
                }

                restoreCell(m_mapData.terrainLayers[selection.layerIndex], selection.cellX, selection.cellZ);
            }
        }

        if (!restoredAnyCell)
        {
            m_lastIoMessage = u8"復活できる削除済み面がありません。";
            return false;
        }

        m_lastIoMessage = u8"選択面を復活しました。";
        AppendOperationLog(m_lastIoMessage);
        return true;
    }

    NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    bool restoredAnyVertex = false;
    for (int gridZ = 0; gridZ < layer.gridHeight; ++gridZ)
    {
        for (int gridX = 0; gridX < layer.gridWidth; ++gridX)
        {
            if (NarakuMap::IsVertexEnabled(layer, gridX, gridZ))
            {
                continue;
            }

            NarakuMap::SetVertexEnabled(layer, gridX, gridZ, true);
            restoredAnyVertex = true;
        }
    }

    if (!restoredAnyVertex)
    {
        m_lastIoMessage = u8"復活できる無効頂点がありません。";
        return false;
    }

    EnsureSelectionValid();
    m_lastIoMessage = u8"現在レイヤーの無効頂点を復活しました。";
    AppendOperationLog(m_lastIoMessage);
    return true;
}

bool SceneNarakuEditor::DeleteCurrentSelection()
{
    if (m_editMode == EditMode::Terrain)
    {
        return (m_terrainEditTarget == TerrainEditTarget::Face)
            ? DeleteSelectedTerrainCells()
            : DeleteSelectedTerrainVertices();
    }

    if (m_editMode == EditMode::Rope && m_selectedRope >= 0 && m_selectedRope < ToInt(m_mapData.ropes.size()))
    {
        std::vector<int> removeIndices = m_multiSelectedRopes;
        if (removeIndices.empty())
        {
            removeIndices.push_back(m_selectedRope);
        }
        std::sort(removeIndices.begin(), removeIndices.end());
        removeIndices.erase(std::unique(removeIndices.begin(), removeIndices.end()), removeIndices.end());
        for (auto it = removeIndices.rbegin(); it != removeIndices.rend(); ++it)
        {
            if (*it >= 0 && *it < ToInt(m_mapData.ropes.size()))
            {
                m_mapData.ropes.erase(m_mapData.ropes.begin() + *it);
            }
        }
        EnsureSelectionValid();
        AppendOperationLog(u8"選択ロープを削除しました。");
        FocusSelection();
        return true;
    }

    if (m_editMode == EditMode::MiningPoint && m_selectedMiningPoint >= 0 && m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
    {
        std::vector<int> removeIndices = m_multiSelectedMiningPoints;
        if (removeIndices.empty())
        {
            removeIndices.push_back(m_selectedMiningPoint);
        }
        std::sort(removeIndices.begin(), removeIndices.end());
        removeIndices.erase(std::unique(removeIndices.begin(), removeIndices.end()), removeIndices.end());
        for (auto it = removeIndices.rbegin(); it != removeIndices.rend(); ++it)
        {
            if (*it >= 0 && *it < ToInt(m_mapData.miningPoints.size()))
            {
                m_mapData.miningPoints.erase(m_mapData.miningPoints.begin() + *it);
            }
        }
        EnsureSelectionValid();
        AppendOperationLog(u8"選択採掘ポイントを削除しました。");
        FocusSelection();
        return true;
    }

    return false;
}

void SceneNarakuEditor::SnapCameraToTopView()
{
    m_cameraPitch = DirectX::XMConvertToRadians(89.0f);
    m_cameraDistance = std::max(m_cameraDistance, 24.0f);
}

void SceneNarakuEditor::HandleShortcuts()
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput)
    {
        return;
    }

    const bool ctrl = IsRawKeyPress(VK_CONTROL) || IsRawKeyPress(VK_LCONTROL) || IsRawKeyPress(VK_RCONTROL);
    if (ctrl && IsRawKeyTrigger('Z'))
    {
        Undo();
        return;
    }

    if (ctrl && IsRawKeyTrigger('Y'))
    {
        Redo();
        return;
    }

    if (!io.WantCaptureKeyboard && IsRawKeyTrigger(VK_DELETE))
    {
        PushUndoSnapshot();
        if (DeleteCurrentSelection())
        {
            if (m_editMode != EditMode::Terrain)
            {
                m_lastIoMessage = u8"選択対象を削除しました";
            }
        }
        else if (!m_undoStack.empty())
        {
            m_undoStack.pop_back();
        }
    }

    if (!io.WantCaptureKeyboard && IsRawKeyTrigger('T'))
    {
        SnapCameraToTopView();
    }
}






