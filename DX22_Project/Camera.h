#pragma once


#include <DirectXMath.h> 

class Camera {
public:
	// コンストラクタ 
	Camera();

	// デストラクタ 
	virtual ~Camera();

	// 更新処理（継承先のクラスで必ず実装 
	virtual void Update() = 0;

	// ビュー行列の取得（デフォルトでは転置済みの行列を計算する 
	DirectX::XMFLOAT4X4 GetViewMatrix(bool transpose = true);
		// プロジェクション行列の取得（デフォルトでは転置済みの行列を計算する 
	DirectX::XMFLOAT4X4 GetProjectionMatrix(bool transpose = true);

	// 座標の取得 
	DirectX::XMFLOAT3 GetPos() const;

	// 注視点の取得 
	DirectX::XMFLOAT3 GetLook() const;
	DirectX::XMFLOAT3 GetUp() const;
	float GetFovy() const;
	float GetAspect() const;
	float GetNear() const;
	float GetFar() const;

	virtual void SetPos(DirectX::XMFLOAT3 pos) = 0;
	virtual void SetLook(DirectX::XMFLOAT3 set) = 0;

	virtual void LockPos(bool set) = 0;
protected:
	DirectX::XMFLOAT3 m_pos;  // 座標 
	DirectX::XMFLOAT3 m_look;  // 注視点 
	DirectX::XMFLOAT3 m_up;  // 上方ベクトル 
	float m_fovy;  // 画角 
	float m_aspect; // アスペクト比 
	float m_near;  // ニアクリップ 
	float m_far;  // ファークリップ 
};


