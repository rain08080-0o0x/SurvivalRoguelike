#include "Scene3DEditor.h"
#include "Geometory.h"
#include "Transfer.h"
#include "CameraDebug.h"
#include "Sprite.h"
#include "Transfer.h"
#include "Defines.h"
#include <memory>

#define SAFE_UPDATE(p) if(p){p->Update();}

/// <summary>
/// 使用する数値は縮尺・回転・位置のみ
/// </summary>
/// <param name="set">Transformを代入</param>
/// <returns>XMFLOAT4X4が値として返却</returns>
static DirectX::XMFLOAT4X4 TransformToFloat4x4(Scene3DEditor::Transform set)
{
    using namespace DirectX;
    
    DirectX::XMFLOAT4X4 result;


    DirectX::XMMATRIX S = XMMatrixScaling(set.scale.x,set.scale.y,set.scale.z);
    DirectX::XMMATRIX R = 
        XMMatrixRotationX(set.rotate.x) * XMMatrixRotationY(set.rotate.y) * XMMatrixRotationZ(set.rotate.x);
    DirectX::XMMATRIX T = XMMatrixTranslation(set.pos.x, set.pos.y, set.pos.z);
    DirectX::XMMATRIX world = S * R * T;

    XMStoreFloat4x4(&result, XMMatrixTranspose(world));

    return result;
}

static DirectX::XMFLOAT3 AdditionFloat3(DirectX::XMFLOAT3 A, DirectX::XMFLOAT3 B)
{
    DirectX::XMFLOAT3 result;
    result.x = A.x + B.x;
    result.y = A.x + B.y;
    result.z = A.z + B.z;
    return result;
}

namespace
{
    const int kCameraModeGame = 0;
    const int kCameraModeDebug = 1;

    int NormalizeCameraMode(int mode)
    {
        return (mode == kCameraModeDebug) ? kCameraModeDebug : kCameraModeGame;
    }

    void ApplyCameraPose(CameraDebug* camera, const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& look)
    {
        if (camera)
        {
            camera->SetPose(eye, look);
        }
    }
}
void Scene3DEditor::RightArm(DirectX::XMFLOAT3 jointPos)
{
    m_armRight1.pos.x = cosf(-m_armRight1.rotate.y) * HALF(m_armRight1.scale.x) + jointPos.x;
    m_armRight1.pos.z = sinf(-m_armRight1.rotate.y) * HALF(m_armRight1.scale.x) + jointPos.z;
    m_armRight1.pos.y = m_armRight1.gpos.y;

    float offsetX = cosf(-m_armRight1.rotate.y) * (m_armRight1.scale.x) + jointPos.x;
    float offsetZ = sinf(-m_armRight1.rotate.y) * (m_armRight1.scale.x) + jointPos.z;

    m_armRight2.pos.x = cosf(-m_armRight2.rotate.y) * HALF(m_armRight2.scale.x) + offsetX;
    m_armRight2.pos.z = sinf(-m_armRight2.rotate.y) * HALF(m_armRight2.scale.x) + offsetZ;
    m_armRight2.pos.y = m_armRight2.gpos.y;
}

void Scene3DEditor::LeftArm(DirectX::XMFLOAT3 jointPos)
{
    m_armLeft1.pos.x = cosf(-m_armLeft1.rotate.y) * HALF(m_armLeft1.scale.x) + jointPos.x;
    m_armLeft1.pos.z = sinf(-m_armLeft1.rotate.y) * HALF(m_armLeft1.scale.x) + jointPos.z;
    m_armLeft1.pos.y = m_armLeft1.gpos.y;

    float offsetX = cosf(-m_armLeft1.rotate.y) * (m_armLeft1.scale.x) + jointPos.x;
    float offsetZ = sinf(-m_armLeft1.rotate.y) * (m_armLeft1.scale.x) + jointPos.z;

    m_armLeft2.pos.x = cosf(-m_armLeft2.rotate.y) * HALF(m_armLeft2.scale.x) + offsetX;
    m_armLeft2.pos.z = sinf(-m_armLeft2.rotate.y) * HALF(m_armLeft2.scale.x) + offsetZ;
    m_armRight2.pos.y = m_armRight2.gpos.y;
}

void Scene3DEditor::RightLeg(DirectX::XMFLOAT3 jointPos)
{
    m_legRight1.pos.x = cosf(-m_legRight1.rotate.y) * HALF(m_legRight1.scale.x) + jointPos.x;
    m_legRight1.pos.z = sinf(-m_legRight1.rotate.y) * HALF(m_legRight1.scale.x) + jointPos.z;
    m_legRight1.pos.y = m_legRight1.gpos.y;

    float offsetX = cosf(-m_legRight1.rotate.y) * (m_legRight1.scale.x) + jointPos.x;
    float offsetZ = sinf(-m_legRight1.rotate.y) * (m_legRight1.scale.x) + jointPos.z;

    m_legRight2.pos.x = cosf(-m_legRight2.rotate.y) * HALF(m_legRight2.scale.x) + offsetX;
    m_legRight2.pos.z = sinf(-m_legRight2.rotate.y) * HALF(m_legRight2.scale.x) + offsetZ;
    m_legRight2.pos.y = m_legRight2.gpos.y;
}

void Scene3DEditor::LeftLeg(DirectX::XMFLOAT3 jointPos)
{
    m_legLeft1.pos.x = cosf(-m_legLeft1.rotate.y) * HALF(m_legLeft1.scale.x) + jointPos.x;
    m_legLeft1.pos.z = sinf(-m_legLeft1.rotate.y) * HALF(m_legLeft1.scale.x) + jointPos.z;
    m_legLeft1.pos.y = m_legLeft1.gpos.y;

    float offsetX = cosf(-m_legLeft1.rotate.y) * (m_legLeft1.scale.x) + jointPos.x;
    float offsetZ = sinf(-m_legLeft1.rotate.y) * (m_legLeft1.scale.x) + jointPos.z;

    m_legLeft2.pos.x = cosf(-m_legLeft2.rotate.y) * HALF(m_legLeft2.scale.x) + offsetX;
    m_legLeft2.pos.z = sinf(-m_legLeft2.rotate.y) * HALF(m_legLeft2.scale.x) + offsetZ;
    m_legRight2.pos.y = m_legRight2.gpos.y;
}

#define AddBodyPos(set) AdditionFloat3(set,m_body.pos) 
void Scene3DEditor::BodyTransformUpdate()
{
    RightArm(AddBodyPos(m_body.jointRightArmPos));
    LeftArm(AddBodyPos(m_body.jointLeftArmPos));
    RightLeg(AddBodyPos(m_body.jointRightLegPos));
    LeftLeg(AddBodyPos(m_body.jointLeftLegPos));
}

Scene3DEditor::Scene3DEditor()
    : m_pCamera(nullptr)
    , m_pCameraGame(nullptr)
    , m_pCameraDebug(nullptr)
    , m_cameraMode(0)
{
    TRAN_INS;
    {
    // arm right 1
    m_armRight1.pos      = { 0,0,0 };
    m_armRight1.gpos     = { 0,0,0 };
    m_armRight1.opos     = { 0,0,0 };
    m_armRight1.scale    = { 3,1,1 };
    m_armRight1.rotate   = { 0,0,0 };
    // arm right 2
    m_armRight2.pos      = { 0,0,0 };
    m_armRight2.gpos     = { 0,0,0 };
    m_armRight2.opos     = { 0,0,0 };
    m_armRight2.scale    = { 3,1,1 };
    m_armRight2.rotate   = { 0,0,0 };
    // arm left 1
    m_armLeft1.pos      = { 0,0,0 };
    m_armLeft1.gpos     = { 0,0,0 };
    m_armLeft1.opos     = { 0,0,0 };
    m_armLeft1.scale    = { 3,1,1 };
    m_armLeft1.rotate   = { 0,PI,0 };
    // arm left 2
    m_armLeft2.pos      = { 0,0,0 };
    m_armLeft2.gpos     = { 0,0,0 };
    m_armLeft2.opos     = { 0,0,0 };
    m_armLeft2.scale    = { 3,1,1 };
    m_armLeft2.rotate   = { 0,PI,0 };
    // leg right 1
    m_legRight1.pos      = { 0,0,0 };
    m_legRight1.gpos     = { 0,0,0 };
    m_legRight1.opos     = { 0,0,0 };
    m_legRight1.scale    = { 3,1,1 };
    m_legRight1.rotate   = { 0,PI / 2.0f,0 };
    // leg right 2
    m_legRight2.pos      = { 0,0,0 };
    m_legRight2.gpos     = { 0,0,0 };
    m_legRight2.opos     = { 0,0,0 };
    m_legRight2.scale    = { 3,1,1 };
    m_legRight2.rotate   = { 0,PI / 2.0f,0 };
    // leg left 1
    m_legLeft1.pos      = { 0,0,0 };
    m_legLeft1.gpos     = { 0,0,0 };
    m_legLeft1.opos     = { 0,0,0 };
    m_legLeft1.scale    = { 3,1,1 };
    m_legLeft1.rotate   = { 0,PI / 2.0f,0 };
    // leg left 2
    m_legLeft2.pos      = { 0,0,0 };
    m_legLeft2.gpos     = { 0,0,0 };
    m_legLeft2.opos     = { 0,0,0 };
    m_legLeft2.scale    = { 3,1,1 };
    m_legLeft2.rotate   = { 0,PI / 2.0f,0 };
    // body
    m_body.pos      = { 0,0,0 };
    m_body.scale    = { 2.6f,1,5.3f };
    m_body.rotate   = { 0,0,0 };
    m_body.jointRightArmPos = {1.1f,0,2.15f};
    m_body.jointLeftArmPos = {-1.1f,0,2.15f};
    m_body.jointRightLegPos = { 0.8f,0,-2.7f };
    m_body.jointLeftLegPos = {-0.8f,0,-2.7f};
    SyncToTransfer();
    }
    m_pCameraGame = new CameraDebug();
    m_pCameraDebug = new CameraDebug();
    tran.cameraMode = NormalizeCameraMode(tran.cameraMode);
    m_cameraMode = tran.cameraMode;
    if (m_pCameraGame)
    {
        m_pCameraGame->LockPos(false);
        ApplyCameraPose(m_pCameraGame, tran.cameraGame.eye, tran.cameraGame.look);
    }
    if (m_pCameraDebug)
    {
        m_pCameraDebug->LockPos(false);
        ApplyCameraPose(m_pCameraDebug, tran.cameraDebug.eye, tran.cameraDebug.look);
    }
    m_pCamera = (m_cameraMode == kCameraModeDebug) ? static_cast<Camera*>(m_pCameraDebug)
        : static_cast<Camera*>(m_pCameraGame);
    tran.camera = (m_cameraMode == kCameraModeDebug) ? tran.cameraDebug : tran.cameraGame;
}

Scene3DEditor::~Scene3DEditor()
{
    SAFE_DELETE(m_pCameraGame);
    SAFE_DELETE(m_pCameraDebug);
    m_pCamera = nullptr;
}

void Scene3DEditor::Update()
{
    SyncFromTransfer();

    TRAN_INS;
    const int nextMode = NormalizeCameraMode(tran.cameraMode);
    if (nextMode != m_cameraMode)
    {
        m_cameraMode = nextMode;
        m_pCamera = (m_cameraMode == kCameraModeDebug) ? static_cast<Camera*>(m_pCameraDebug)
            : static_cast<Camera*>(m_pCameraGame);
    }

    if (m_cameraMode == kCameraModeDebug)
    {
        tran.camera = tran.cameraDebug;
        ApplyCameraPose(m_pCameraDebug, tran.cameraDebug.eye, tran.cameraDebug.look);
    }
    else
    {
        tran.camera = tran.cameraGame;
        ApplyCameraPose(m_pCameraGame, tran.cameraGame.eye, tran.cameraGame.look);
    }

    SAFE_UPDATE(m_pCamera);

    if (m_cameraMode == kCameraModeDebug)
    {
        tran.cameraDebug = tran.camera;
    }
    else
    {
        tran.cameraGame = tran.camera;
    }

    BodyTransformUpdate();

    SyncToTransfer();
}

void Scene3DEditor::Draw()
{
    if (!m_pCamera) return;

    DirectX::XMFLOAT4X4 view = m_pCamera->GetViewMatrix();
    DirectX::XMFLOAT4X4 proj = m_pCamera->GetProjectionMatrix();

    Geometory::SetView(view);
    Geometory::SetProjection(proj);
    Sprite::SetView(view);
    Sprite::SetProjection(proj);

    Geometory::SetWorld(TransformToFloat4x4(m_armRight1));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_armRight2));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_armLeft1));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_armLeft2));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_legRight1));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_legRight2));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_legLeft1));
    Geometory::DrawBox();

    Geometory::SetWorld(TransformToFloat4x4(m_legLeft2));
    Geometory::DrawBox();
    Transform temp;
    temp.pos = m_body.pos;
    temp.rotate = m_body.rotate;
    temp.scale = m_body.scale;
    Geometory::SetWorld(TransformToFloat4x4(temp));
    Geometory::DrawBox();
}

void Scene3DEditor::SyncToTransfer()
{
    TRAN_INS;
    // Arm Right 1
    tran.modelediter.armRight1.subAngle = m_armRight1.rotate;
    tran.modelediter.armRight1.size = m_armRight1.scale;
    tran.modelediter.armRight1.pos = m_armRight1.gpos;
    // Arm Rgiht 2
    tran.modelediter.armRight2.subAngle = m_armRight2.rotate;
    tran.modelediter.armRight2.size = m_armRight2.scale;
    tran.modelediter.armRight2.pos = m_armRight2.gpos;
    // Arm Left 1
    tran.modelediter.armLeft1.subAngle = m_armLeft1.rotate;
    tran.modelediter.armLeft1.size = m_armLeft1.scale;
    tran.modelediter.armLeft1.pos = m_armLeft1.gpos;
    // Arm Left 2
    tran.modelediter.armLeft2.subAngle = m_armLeft2.rotate;
    tran.modelediter.armLeft2.size = m_armLeft2.scale;
    tran.modelediter.armLeft2.pos = m_armLeft2.gpos;
    // Leg Right 1
    tran.modelediter.legRight1.subAngle = m_legRight1.rotate;
    tran.modelediter.legRight1.size = m_legRight1.scale;
    tran.modelediter.legRight1.pos = m_legRight1.gpos;
    // Leg Rgiht 2
    tran.modelediter.legRight2.subAngle = m_legRight2.rotate;
    tran.modelediter.legRight2.size = m_legRight2.scale;
    tran.modelediter.legRight2.pos = m_legRight2.gpos;
    // Leg Left 1
    tran.modelediter.legLeft1.subAngle = m_legLeft1.rotate;
    tran.modelediter.legLeft1.size = m_legLeft1.scale;
    tran.modelediter.legLeft1.pos = m_legLeft1.gpos;
    // Leg Left 2
    tran.modelediter.legLeft2.subAngle = m_legLeft2.rotate;
    tran.modelediter.legLeft2.size = m_legLeft2.scale;
    tran.modelediter.legLeft2.pos = m_legLeft2.gpos;
    // Body
    tran.modelediter.body.pos = m_body.pos;
    tran.modelediter.body.size = m_body.scale;
    tran.modelediter.body.angle = m_body.rotate;
    tran.modelediter.body.jointRightArmPos = m_body.jointRightArmPos;
    tran.modelediter.body.jointLeftArmPos = m_body.jointLeftArmPos;
    tran.modelediter.body.jointRightLegPos = m_body.jointRightLegPos;
    tran.modelediter.body.jointLeftLegPos = m_body.jointLeftLegPos;
}

void Scene3DEditor::SyncFromTransfer()
{
    TRAN_INS;
    // Arm Right 1
    m_armRight1.rotate = tran.modelediter.armRight1.subAngle;
    m_armRight1.scale = tran.modelediter.armRight1.size;
    m_armRight1.gpos = tran.modelediter.armRight1.pos;
    // Arm Right 2
    m_armRight2.rotate = tran.modelediter.armRight2.subAngle;
    m_armRight2.scale = tran.modelediter.armRight2.size;
    m_armRight2.gpos = tran.modelediter.armRight2.pos;
    // Arm Left 1
    m_armLeft1.rotate = tran.modelediter.armLeft1.subAngle;
    m_armLeft1.scale = tran.modelediter.armLeft1.size;
    m_armLeft1.gpos = tran.modelediter.armLeft1.pos;
    // Arm Left 2
    m_armLeft2.rotate = tran.modelediter.armLeft2.subAngle;
    m_armLeft2.scale = tran.modelediter.armLeft2.size;
    m_armLeft2.gpos = tran.modelediter.armLeft2.pos;
    // Leg Right 1
    m_legRight1.rotate = tran.modelediter.legRight1.subAngle;
    m_legRight1.scale = tran.modelediter.legRight1.size;
    m_legRight1.gpos = tran.modelediter.legRight1.pos;
    // Leg Right 2
    m_legRight2.rotate = tran.modelediter.legRight2.subAngle;
    m_legRight2.scale = tran.modelediter.legRight2.size;
    m_legRight2.gpos = tran.modelediter.legRight2.pos;
    // Leg Left 1
    m_legLeft1.rotate = tran.modelediter.legLeft1.subAngle;
    m_legLeft1.scale = tran.modelediter.legLeft1.size;
    m_legLeft1.gpos = tran.modelediter.legLeft1.pos;
    // Leg Left 2
    m_legLeft2.rotate = tran.modelediter.legLeft2.subAngle;
    m_legLeft2.scale = tran.modelediter.legLeft2.size;
    m_legLeft2.gpos = tran.modelediter.legLeft2.pos;
    // Body
    m_body.pos = tran.modelediter.body.pos;
    m_body.scale = tran.modelediter.body.size;
    m_body.rotate = tran.modelediter.body.angle;
    m_body.jointRightArmPos = tran.modelediter.body.jointRightArmPos;
    m_body.jointLeftArmPos = tran.modelediter.body.jointLeftArmPos;
    m_body.jointRightLegPos = tran.modelediter.body.jointRightLegPos;
    m_body.jointLeftLegPos = tran.modelediter.body.jointLeftLegPos;
}








