#pragma once
#include"GameObject.h"
#include "Collision.h"

class Block
	:public GameObject
{
public:
	Block();
	~Block();
	
	void Update()override;
	void Draw()override;

	void SetPos(DirectX::XMFLOAT3 pos);

	Collision::Box GetCollision();
private:
	Collision::Box m_collision;
};

