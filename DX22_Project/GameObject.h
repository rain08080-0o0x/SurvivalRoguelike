#pragma once

#include <DirectXMath.h>
#include "Model.h"

class GameObject 
{
protected:
	DirectX::XMFLOAT3 m_pos; // オブジェクトの座標 

public:
	//--- 基本処理 
	GameObject();
	virtual ~GameObject() = 0;
	virtual void Update() = 0;
	virtual void Draw() = 0;

	//--- 座標操作 
	DirectX::XMFLOAT3 GetPos();
	void SetPos(DirectX::XMFLOAT3 pos);

};