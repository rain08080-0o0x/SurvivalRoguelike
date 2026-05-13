#pragma once
#include "Scene.h"
#include <DirectXMath.h>
#include "Camera.h"

class CameraDebug;

class Scene3DEditor :
    public Scene
{
public:
    Scene3DEditor();
    ~Scene3DEditor();

    void Update()override;
    void Draw()override;

public:
    struct Transform
    {
        DirectX::XMFLOAT3 pos;      // 中心座標
        DirectX::XMFLOAT3 rotate;   // 回転角
        DirectX::XMFLOAT3 scale;    // サイズ
        DirectX::XMFLOAT3 gpos;     // グローバル座標
        DirectX::XMFLOAT3 opos;     // オブジェクト座標
    };
    struct BodyTransform
    {
        DirectX::XMFLOAT3 pos;              // 中心座標
        DirectX::XMFLOAT3 rotate;           // 回転角
        DirectX::XMFLOAT3 scale;            // サイズ
        DirectX::XMFLOAT3 jointRightArmPos; // 右腕接合部座標
        DirectX::XMFLOAT3 jointLeftArmPos;  // 左腕接合部座標
        DirectX::XMFLOAT3 jointRightLegPos; // 右脚接合部座標
        DirectX::XMFLOAT3 jointLeftLegPos;  // 左脚接合部座標
    };
private:
    void RightArm(DirectX::XMFLOAT3 jointPos);
    void LeftArm(DirectX::XMFLOAT3 jointPos);
    void RightLeg(DirectX::XMFLOAT3 jointPos);
    void LeftLeg(DirectX::XMFLOAT3 jointPos);
    void BodyTransformUpdate();
    // tran = m
    void SyncToTransfer();
    // m = tran
    void SyncFromTransfer();
private:
    Transform m_armRight1;
    Transform m_armRight2;
    Transform m_armLeft1;
    Transform m_armLeft2;
    Transform m_legRight1;
    Transform m_legRight2;
    Transform m_legLeft1;
    Transform m_legLeft2;
    BodyTransform m_body;

    Camera* m_pCamera;
    CameraDebug* m_pCameraGame;
    CameraDebug* m_pCameraDebug;
    int m_cameraMode;
};


