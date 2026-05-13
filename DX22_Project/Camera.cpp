#include "Camera.h"

Camera::Camera()
	: m_pos(0.0f,1.5f,-2.0f)
	, m_look(0.0f, 0.0f, 0.0f)
	, m_up(0.0f, 1.0f, 0.0f)
	, m_fovy(DirectX::XMConvertToRadians(60.0f))
	, m_aspect(16.0f / 9.0f)
	, m_near(0.03f)
	, m_far(1000.0f) 
{
}// Camera.cpp に追加（デストラクタ実装）
Camera::~Camera() 
{
}

DirectX::XMFLOAT4X4 Camera::GetViewMatrix(bool transpose)
{
	DirectX::XMFLOAT4X4 mat;
	DirectX::XMMATRIX view;
	view = DirectX::XMMatrixLookAtLH(
		DirectX::XMVectorSet(m_pos.x, m_pos.y, m_pos.z, 0.0f),
		DirectX::XMVectorSet(m_look.x, m_look.y, m_look.z, 0.0f), // x を使う
		DirectX::XMVectorSet(m_up.x, m_up.y, m_up.z, 0.0f)        // メンバ m_up を使う
	);
	if (transpose) {
		view = DirectX::XMMatrixTranspose(view);
	}
	DirectX::XMStoreFloat4x4(&mat, view);
	return mat;
}

DirectX::XMFLOAT4X4 Camera::GetProjectionMatrix(bool transpose)
{
	DirectX::XMFLOAT4X4 mat;
	DirectX::XMMATRIX proj;
	proj = DirectX::XMMatrixPerspectiveFovLH(
		m_fovy,
		m_aspect,
		m_near,
		m_far
	);
	if (transpose) {
		proj = DirectX::XMMatrixTranspose(proj);
	}
	DirectX::XMStoreFloat4x4(&mat, proj);
	return mat;
}

DirectX::XMFLOAT3 Camera::GetPos() const
{
	return m_pos;
}

DirectX::XMFLOAT3 Camera::GetLook() const
{
	return m_look;
}

DirectX::XMFLOAT3 Camera::GetUp() const
{
	return m_up;
}

float Camera::GetFovy() const
{
	return m_fovy;
}

float Camera::GetAspect() const
{
	return m_aspect;
}

float Camera::GetNear() const
{
	return m_near;
}

float Camera::GetFar() const
{
	return m_far;
}


