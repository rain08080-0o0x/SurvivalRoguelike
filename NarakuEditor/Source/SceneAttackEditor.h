#pragma once

#include "Scene.h"
#include "../../NarakuGame/Source/NarakuCombatData.h"
#include <DirectXMath.h>

class Model;
class RenderTarget;
class DepthStencil;

class SceneAttackEditor : public Scene
{
public:
    enum MenuCommand : unsigned int
    {
        MenuReload = 49500,
        MenuSave,
        MenuPublish,
        MenuNewAttack,
        MenuShowStageEditor,
        MenuShowPieceEditor,
        MenuToggleDataWindow,
        MenuToggleBasicWindow,
        MenuToggleTimelineWindow,
        MenuToggleCombatWindow,
        MenuToggleHitboxWindow,
        MenuToggleTransformWindow,
        MenuTogglePreviewWindow
    };

    SceneAttackEditor();
    ~SceneAttackEditor() override;
    void Update() override;
    void Draw() override;
    bool HandleNativeMenuCommand(unsigned int commandId);
    void SyncNativeMenuState(void* menuHandle) const;

private:
    void Reload();
    void SaveSelected();
    void Publish();
    void CreateAttack();
    void DrawSettingsWindow();
    void DrawTimelineWindow();
    void DrawFrameWindow();
    void DrawSaveConfirmation();
    void SaveNow(bool publish);
    void ChangeSelection(int index);
    int PlaybackFrames() const;
    void EditPreviewBox(const DirectX::XMFLOAT2& origin, const DirectX::XMFLOAT2& size);
    void DrawPreview();
    bool EnsurePreviewRenderTarget(unsigned int width, unsigned int height);
    void EnsurePreviewModel(const NarakuCombat::AttackDefinition& attack);
    void RenderPreview3D(const NarakuCombat::AttackDefinition& attack,
        unsigned int width, unsigned int height);
    void ResetPreviewCamera();
    void ReleasePreviewModel();
    void ReleasePreviewResources();
    NarakuCombat::AttackDefinition* GetSelectedAttack();

    NarakuCombat::Catalog m_catalog;
    int m_selected = -1;
    float m_previewFrame = 0.0f;
    bool m_playing = true;
    bool m_loopPlayback = true;
    float m_playbackSpeed = 1.0f;
    int m_selectedPhase = -1;
    bool m_editAttackBox = false;
    bool m_editDefaultBox = false;
    int m_dragFace = -1;
    DirectX::XMFLOAT2 m_dragStartMouse = {};
    DirectX::XMFLOAT2 m_dragScreenAxis = {};
    float m_dragStartHalfSize = 0.0f;
    int m_pendingSave = 0;
    bool m_openSaveConfirmation = false;
    std::vector<std::string> m_saveWarnings;
    float m_animationSeconds = 0.0f;
    int m_previewAnimation = -1;
    DirectX::XMFLOAT4X4 m_viewMatrix = {};
    DirectX::XMFLOAT4X4 m_projectionMatrix = {};
    bool m_showDataWindow = true;
    bool m_showTimelineWindow = true;
    bool m_showHitboxWindow = true;
    bool m_showPreviewWindow = true;
    RenderTarget* m_previewRenderTarget = nullptr;
    DepthStencil* m_previewDepthStencil = nullptr;
    Model* m_previewModel = nullptr;
    unsigned int m_previewWidth = 0;
    unsigned int m_previewHeight = 0;
    bool m_previewModelAttempted = false;
    std::string m_previewModelId;
    DirectX::XMFLOAT3 m_previewModelCenter = {};
    float m_previewModelLongestSize = 1.0f;
    float m_previewCameraYaw = 0.0f;
    float m_previewCameraPitch = 0.0f;
    float m_previewCameraZoom = 4.8f;
    DirectX::XMFLOAT3 m_previewCameraTarget = { 0.0f, 0.65f, 0.45f };
    std::string m_message;
};
