#pragma once
#include "Camera.h" 
#include "Input.h" 

class CameraDebug : public Camera
{
public:
	CameraDebug();
	~CameraDebug();

	void Update() final;

	void SetLook(DirectX::XMFLOAT3 set)final;
	void SetPos(DirectX::XMFLOAT3 set)final;

	void SetPose(const DirectX::XMFLOAT3& eye, const DirectX::XMFLOAT3& look);

	void LockPos(bool set)final;
private:
	void OrbitCamera(float deltaX, float deltaY);
	void PanCamera(float deltaX, float deltaY);
	void ZoomCamera(float delta);
	void SyncOrbitFromPose();
	float m_radXZ;
	float m_radY;
	float m_radius;
	bool isLock;
};
