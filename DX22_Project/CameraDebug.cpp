#include "CameraDebug.h"
#include "Transfer.h"
#include "SceneManager.h"
#include "imgui.h"
#include <cmath>
const float CameraSpeed = 0.1f;
const float CameraDebugRotate = 0.1f;
const float CameraDefaultDistance = -2.0f;

CameraDebug::CameraDebug()
	: m_radXZ(0.0f)
	, m_radY(-0.7f)
	, m_radius(-10.0f)
{
    TRAN_INS;
    m_look = { 0.0f,0.0f,0.0f };
    m_pos.x = m_look.x + m_radius * cosf(m_radY) * sinf(m_radXZ);
    m_pos.y = m_look.y + m_radius * sinf(m_radY);
    m_pos.z = m_look.z + m_radius * cosf(m_radY) * cosf(m_radXZ);

    isLock = false;

    tran.camera.eye = { 0,10,0.001f };
    tran.camera.look = m_look;
}

CameraDebug::~CameraDebug()
{

}

void CameraDebug::Update()
{
    TRAN_INS;
    if (!isLock)
    {
#ifdef _DEBUG
        using namespace DirectX;
        // 先生のやつ
        //--- 注視点の移動
        // ↑(+z)に移動 
        XMVECTOR pos = XMLoadFloat3(&m_pos);
        XMVECTOR look = XMLoadFloat3(&m_look);

        // 向いている方向（look - pos）
        XMVECTOR forward = look - pos;

        // 地面移動にしたいならY成分を潰す（XZ平面）
        forward = XMVectorSetY(forward, 0.0f);

        // ほぼゼロ長さ対策
        if (XMVectorGetX(XMVector3LengthSq(forward)) < 1e-8f)
            return;

        forward = XMVector3Normalize(forward);

        // ワールド上方向
        const XMVECTOR up = XMVectorSet(0, 1, 0, 0);

        // 右方向（左右が逆なら cross の順序を変える）
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));

        XMVECTOR delta = XMVectorZero();
        const POINT mouseDelta = GetMouseDelta();
        float mouseWheel = 0.0f;
        bool allowMouseCamera = true;
        if (ImGui::GetCurrentContext())
        {
            ImGuiIO& io = ImGui::GetIO();
            mouseWheel = io.MouseWheel;
            allowMouseCamera = !io.WantCaptureMouse;
        }

        if (allowMouseCamera)
        {
            if (IsMouseRightPress())
            {
                OrbitCamera(static_cast<float>(mouseDelta.x), static_cast<float>(mouseDelta.y));
            }
            if (IsMouseMiddlePress())
            {
                PanCamera(static_cast<float>(mouseDelta.x), static_cast<float>(mouseDelta.y));
            }
            if (std::fabs(mouseWheel) > 0.001f)
            {
                ZoomCamera(mouseWheel);
            }
        }

        //if (IsRawKeyPress(VK_UP))    delta += forward * CameraSpeed;
        //if (IsRawKeyPress(VK_DOWN))  delta -= forward * CameraSpeed;
        //if (IsRawKeyPress(VK_RIGHT)) delta += right * CameraSpeed;
        //if (IsRawKeyPress(VK_LEFT))  delta -= right * CameraSpeed;

        const bool allowVerticalMove = (SceneManager::GetCurrent() != SceneManager::SceneType::SCENE_GAME);
        if (allowVerticalMove)
        {
            // 上下移動（ワールドY）
            if (IsRawKeyPress(VK_PRIOR)) delta += up * CameraSpeed;
            if (IsRawKeyPress(VK_NEXT))  delta -= up * CameraSpeed;
        }

        // 視線を維持したまま平行移動：pos と look を同じだけ動かす
        pos += delta;
        look += delta;

        // m_posについては後でlook基準に動かすので消す
        //XMStoreFloat3(&m_pos, pos);
        XMStoreFloat3(&m_look, look);
        //--- カメラ位置の移動
        if (IsRawKeyPress('J')) { m_radXZ += CameraDebugRotate; }
        if (IsRawKeyPress('L')) { m_radXZ -= CameraDebugRotate; }
        if (IsRawKeyPress('I')) { m_radY -= CameraDebugRotate; }
        if (IsRawKeyPress('K')) { m_radY += CameraDebugRotate; }

        // --- カメラの距離
        if (IsRawKeyPress('E')) { m_radius += CameraSpeed; }
        if (IsRawKeyPress('Q')) { m_radius -= CameraSpeed; }

        // カメラの位置の計算
        m_pos.x = m_look.x + m_radius * cosf(m_radY) * sinf(m_radXZ);
        m_pos.y = m_look.y + m_radius * sinf(m_radY);
        m_pos.z = m_look.z + m_radius * cosf(m_radY) * cosf(m_radXZ);
        // Transferの更新
        tran.camera.eye = m_pos;
        tran.camera.look = m_look;
#else
        m_pos = tran.camera.eye;
        m_look = tran.camera.look;
        tran.camera.eye = m_pos;
        tran.camera.look = m_look;
#endif
    }
    else
    {
        m_pos = tran.camera.eye;
        m_look = tran.camera.look;

        tran.camera.eye = m_pos;
        tran.camera.look = m_look;
    }
}

void CameraDebug::OrbitCamera(float deltaX, float deltaY)
{
    m_radXZ += deltaX * 0.01f;
    const float nextPitch = m_radY + deltaY * 0.01f;
    if (nextPitch < -1.35f)
    {
        m_radY = -1.35f;
    }
    else if (nextPitch > 1.35f)
    {
        m_radY = 1.35f;
    }
    else
    {
        m_radY = nextPitch;
    }
}

void CameraDebug::PanCamera(float deltaX, float deltaY)
{
    using namespace DirectX;
    const XMVECTOR eye = XMLoadFloat3(&m_pos);
    const XMVECTOR look = XMLoadFloat3(&m_look);
    XMVECTOR forward = look - eye;
    if (XMVectorGetX(XMVector3LengthSq(forward)) < 1.0e-6f)
    {
        forward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    }
    forward = XMVector3Normalize(forward);

    const XMVECTOR worldUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMVECTOR right = XMVector3Cross(worldUp, forward);
    if (XMVectorGetX(XMVector3LengthSq(right)) < 1.0e-6f)
    {
        right = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
    }
    right = XMVector3Normalize(right);
    XMVECTOR cameraUp = XMVector3Normalize(XMVector3Cross(forward, right));

    const float distance = std::fabs((m_radius != 0.0f) ? m_radius : CameraDefaultDistance);
    const float panScaleX = (distance * 0.15f > 1.0f) ? (distance * 0.15f) : 1.0f;
    const float panScaleY = (distance * 0.10f > 1.0f) ? (distance * 0.10f) : 1.0f;
    const XMVECTOR delta =
        right * (-deltaX * 0.01f * panScaleX) +
        cameraUp * (deltaY * 0.01f * panScaleY);

    XMVECTOR newLook = look + delta;
    XMStoreFloat3(&m_look, newLook);
}

void CameraDebug::ZoomCamera(float delta)
{
    float nextRadius = m_radius + delta;
    if (nextRadius > -2.5f)
    {
        nextRadius = -2.5f;
    }
    else if (nextRadius < -80.0f)
    {
        nextRadius = -80.0f;
    }
    m_radius = nextRadius;
}

void CameraDebug::SetLook(DirectX::XMFLOAT3 set)
{
    m_look = set;
    SyncOrbitFromPose();
    TRAN_INS;
    tran.camera.look = m_look;
}

void CameraDebug::SetPos(DirectX::XMFLOAT3 pos)
{
    m_pos = pos;
    SyncOrbitFromPose();
    TRAN_INS;
    tran.camera.eye = m_pos;
}

void CameraDebug::LockPos(bool set)
{
    isLock = set;
}

void CameraDebug::SetPose(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& look)
{
    m_pos = eye;
    m_look = look;
    SyncOrbitFromPose();
    TRAN_INS;
    tran.camera.eye = m_pos;
    tran.camera.look = m_look;
}

void CameraDebug::SyncOrbitFromPose()
{
    const float dx = m_pos.x - m_look.x;
    const float dy = m_pos.y - m_look.y;
    const float dz = m_pos.z - m_look.z;
    const float distSq = dx * dx + dy * dy + dz * dz;
    if (distSq < 1.0e-6f)
    {
        m_radius = 0.0f;
        return;
    }
    const float dist = sqrtf(distSq);
    m_radius = dist;
    m_radY = asinf(dy / dist);
    const float lenXZ = sqrtf(dx * dx + dz * dz);
    if (lenXZ > 1.0e-6f)
    {
        m_radXZ = atan2f(dx, dz);
    }
}

