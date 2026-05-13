#pragma once

#include "BuildMap.h"
#include "BuildSelection.h"
#include "Scene.h"
#include <DirectXMath.h>
#include <functional>
#include <string>
#include <vector>

class Model;
class RenderTarget;
class DepthStencil;

class SceneCastleEditor : public Scene
{
    friend class CastleSaveData;

public:
    struct AssetInfo
    {
        std::string assetId;
        std::string name;
        std::string path;
        float scale = 1.0f;
        float yOffset = 0.0f;
    };

    using PlacementInfo = CastlePlacementInfo;

    enum class HistoryEntryType
    {
        AddPlacement,
        AddPlacementBatch,
        RemovePlacement,
        RemovePlacementBatch,
        UpdatePlacement,
    };

    enum class ToolMode
    {
        SelectSingle,
        SelectFill,
        PlaceSingle,
        PlacePaint,
        PlaceFill,
    };

    struct HistoryEntry
    {
        HistoryEntryType type = HistoryEntryType::AddPlacement;
        int index = -1;
        PlacementInfo beforePlacement{};
        PlacementInfo afterPlacement{};
        std::vector<PlacementInfo> placements;
        std::vector<int> indices;
        std::vector<int> beforeSelectedPlacementIndices;
        std::vector<int> afterSelectedPlacementIndices;
        int beforeSelectedPlacementIndex = -1;
        int afterSelectedPlacementIndex = -1;
    };

public:
    SceneCastleEditor();
    ~SceneCastleEditor() override;

    void Update() override;
    void Draw() override;

    int GetAssetCount() const;
    const AssetInfo* GetAssetInfo(int index) const;
    int GetSelectedAssetIndex() const;
    void SetSelectedAssetIndex(int index);
    bool AddAssetFromPath(const char* displayName, const char* path);
    bool RemoveAsset(int index);
    bool ReloadAssetCatalog();
    bool CanRemoveAsset(int index) const;
    const char* GetAssetCatalogPath() const;
    const char* GetAssetCatalogStatusMessage() const;
    bool IsAssetCatalogStatusError() const;
    ToolMode GetToolMode() const;
    void SetToolMode(ToolMode mode);
    void* GetAssetThumbnailTextureId(int index, unsigned int size);
    void* GetModelViewTextureId(unsigned int size);
    void HandleModelViewInput(bool hovered, bool rightDragging, float mouseDeltaX, float mouseDeltaY, float mouseWheel);
    void ResetModelViewCamera();

    int GetPlacementCount() const;
    const PlacementInfo* GetPlacement(int index) const;
    int GetSelectedPlacementIndex() const;
    int GetSelectedPlacementCount() const;
    int GetSelectedPlacementIndexAt(int order) const;
    bool IsPlacementSelected(int index) const;
    void ClearSelection();
    void SetSelectedPlacementIndex(int index);
    void AddSelectedPlacementIndex(int index);
    void ToggleSelectedPlacementIndex(int index);
    void DeleteSelectedPlacement();
    void UpdateSelectedPlacement(int gridX, int gridY, int gridZ, int rotationQuarterTurns);
    bool CanUndo() const;
    bool CanRedo() const;
    void Undo();
    void Redo();

    int GetActiveLayer() const;
    void SetActiveLayer(int layer);
    float GetGridSize() const;
    int GetGridHalfExtent() const;

    void RotatePreview(int deltaQuarterTurns);
    bool HasPreview() const;
    bool CanPlacePreview() const;
    int GetPreviewRotationQuarterTurns() const;
    bool IsFillStartActive() const;
    bool IsSelectFillStartActive() const;

    DirectX::XMFLOAT3 GetCameraEye() const;
    DirectX::XMFLOAT3 GetCameraLook() const;
    void SetCameraEye(const DirectX::XMFLOAT3& eye);
    void SetCameraLook(const DirectX::XMFLOAT3& look);
    void ResetCamera();
    float GetGridHeightStep() const;

    bool SaveCastleData(const char* path = nullptr) const;
    bool LoadCastleData(const char* path = nullptr);

    void HandleSceneViewInput(
        float localMouseX,
        float localMouseY,
        float viewWidth,
        float viewHeight,
        bool hovered,
        bool leftClicked,
        bool leftDown,
        bool ctrlDown,
        bool shiftDown,
        bool rightDragging,
        bool middleDragging,
        float mouseDeltaX,
        float mouseDeltaY,
        float mouseWheel);

private:
    enum class FillPlane
    {
        None,
        XZ,
        YZ,
        XY,
    };

    struct AssetState
    {
        AssetInfo info{};
        Model* model = nullptr;
        DirectX::XMFLOAT3 boundsMin{ -0.5f, -0.5f, -0.5f };
        DirectX::XMFLOAT3 boundsMax{ 0.5f, 0.5f, 0.5f };
        DirectX::XMFLOAT3 placementAnchor{ 0.0f, 0.0f, 0.0f };
        bool hasBounds = false;
        RenderTarget* thumbnailRT = nullptr;
        DepthStencil* thumbnailDS = nullptr;
        unsigned int thumbnailSize = 0;
        bool thumbnailDirty = true;
    };

private:
    bool LoadAssetCatalogInfos(std::vector<AssetInfo>& outAssetInfos, std::string& outError) const;
    bool SaveAssetCatalogInfos(const std::vector<AssetInfo>& assetInfos, std::string& outError) const;
    bool ApplyAssetCatalog(const std::vector<AssetInfo>& assetInfos, std::string& outError);
    bool IsAssetReferencedBySessionId(const std::string& assetId) const;
    int FindAssetIndexById(const std::string& assetId) const;
    void SetAssetCatalogStatus(const std::string& message, bool isError);
    void LoadAssets();
    void ReleaseAssets();
    bool EnsureThumbnailTargets(AssetState& asset, unsigned int size);
    void RenderAssetThumbnail(AssetState& asset, unsigned int size);
    bool EnsureModelViewTargets(unsigned int size);
    void RenderModelView(unsigned int size);
    int GetModelViewAssetIndex() const;
    void PushHistory(const HistoryEntry& entry);
    void FinishPaintStroke();
    void ClearPlacementToolState();
    void ApplySelectionState(const std::vector<int>& indices);
    int InsertPlacementInternal(const PlacementInfo& placement, int index);
    void RemovePlacementInternal(int index);
    void UpdatePlacementInternal(int index, const PlacementInfo& placement);
    void DrawGrid() const;
    void DrawPlacement(const PlacementInfo& placement, bool isPreview, bool isSelected) const;
    void DrawSelectionFrame(const PlacementInfo& placement) const;
    void DrawPlacementBounds(const PlacementInfo& placement, const DirectX::XMFLOAT4& color, float inflate) const;
    void AddGridCellFrame(int gridX, int gridY, int gridZ, const DirectX::XMFLOAT4& color, float inset) const;
    void SelectPlacementsInFillArea(int endGridX, int endGridY, int endGridZ, bool additive);
    bool IsFillPlaneAllowed(FillPlane plane, bool verticalOnly) const;
    FillPlane DetermineFillPlane(int startGridX, int startGridY, int startGridZ, int endGridX, int endGridY, int endGridZ) const;
    void ForEachFillCell(
        FillPlane plane,
        int startGridX,
        int startGridY,
        int startGridZ,
        int endGridX,
        int endGridY,
        int endGridZ,
        const std::function<void(int, int, int)>& visitor) const;
    void ForEachBoxCell(
        int startGridX,
        int startGridY,
        int startGridZ,
        int endGridX,
        int endGridY,
        int endGridZ,
        const std::function<void(int, int, int)>& visitor) const;

    bool IsInsideGrid(int gridX, int gridY, int gridZ) const;
    int FindPlacementAt(int gridX, int gridY, int gridZ) const;
    bool BuildMouseRay(
        float localMouseX,
        float localMouseY,
        float viewWidth,
        float viewHeight,
        DirectX::XMVECTOR& outOrigin,
        DirectX::XMVECTOR& outDirection) const;
    bool ComputePlacementPreviewFromRay(
        const DirectX::XMVECTOR& rayOrigin,
        const DirectX::XMVECTOR& rayDirection,
        int& outGridX,
        int& outGridY,
        int& outGridZ) const;
    int FindPlacementByRay(
        const DirectX::XMVECTOR& rayOrigin,
        const DirectX::XMVECTOR& rayDirection) const;
    bool RaycastToLayer(float localMouseX, float localMouseY, float viewWidth, float viewHeight, int layer, int& outGridX, int& outGridZ) const;
    bool RaycastToActiveLayer(float localMouseX, float localMouseY, float viewWidth, float viewHeight, int& outGridX, int& outGridZ) const;
    DirectX::XMFLOAT3 GridToWorld(int gridX, int gridY, int gridZ, float yOffset) const;

    void OrbitCamera(float deltaX, float deltaY);
    void PanCamera(float deltaX, float deltaY);
    void ZoomCamera(float delta);
    void SyncCameraAnglesFromPose();

private:
    std::vector<AssetState> m_assets;
    BuildMap m_buildMap;
    BuildSelection m_selection;
    std::vector<HistoryEntry> m_undoStack;
    std::vector<HistoryEntry> m_redoStack;
    std::string m_assetCatalogStatusMessage;
    bool m_assetCatalogStatusIsError;

    int m_selectedAssetIndex;
    ToolMode m_toolMode;
    int m_activeLayer;
    int m_previewGridX;
    int m_previewGridY;
    int m_previewGridZ;
    int m_previewRotationQuarterTurns;
    bool m_hasPreview;
    bool m_canPlacePreview;

    float m_gridSize;
    int m_gridHalfExtent;
    float m_gridHeightStep;

    DirectX::XMFLOAT3 m_cameraEye;
    DirectX::XMFLOAT3 m_cameraLook;
    DirectX::XMFLOAT3 m_cameraUp;
    float m_cameraFovy;
    float m_cameraNear;
    float m_cameraFar;
    float m_cameraAspect;
    float m_cameraYaw;
    float m_cameraPitch;
    float m_cameraDistance;

    RenderTarget* m_modelViewRT;
    DepthStencil* m_modelViewDS;
    unsigned int m_modelViewSize;
    float m_modelViewYaw;
    float m_modelViewPitch;
    float m_modelViewDistanceScale;

    bool m_paintStrokeActive;
    int m_paintStrokeStartIndex;
    int m_paintStrokePreviousSelectedIndex;
    std::vector<int> m_paintStrokePreviousSelectedIndices;
    std::vector<PlacementInfo> m_paintStrokePlacements;

    bool m_fillStartActive;
    int m_fillStartGridX;
    int m_fillStartGridY;
    int m_fillStartGridZ;
    int m_fillHeightOffset;

    bool m_selectFillStartActive;
    int m_selectFillStartGridX;
    int m_selectFillStartGridY;
    int m_selectFillStartGridZ;
    bool m_selectFillHasPreview;
    int m_selectFillPreviewGridX;
    int m_selectFillPreviewGridY;
    int m_selectFillPreviewGridZ;
    bool m_shiftVerticalFillMode;
};
