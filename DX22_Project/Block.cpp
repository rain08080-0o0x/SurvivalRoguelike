#include "Block.h"
#include "Geometory.h"

Block::Block()
{
	m_collision.size = DirectX::XMFLOAT3(3.0f,3.0f,1.0f);
	m_collision.center = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);

	m_pos = {5.0f,0.0f,0.0f};
}

Block::~Block()
{

}

void Block::Update()
{
	m_collision.center = m_pos;
}

void Block::Draw()
{
#if _DEBUG // デバッグで当たり判定表示 
	// 変換行列の計算 
	DirectX::XMMATRIX T = DirectX::XMMatrixTranslation(m_pos.x,m_pos.y,m_pos.z);
	DirectX::XMMATRIX S = DirectX::XMMatrixScaling(m_collision.size.x,m_collision.size.y,m_collision.size.z);
	DirectX::XMMATRIX mat;
	mat = S * T;
	mat = DirectX::XMMatrixTranspose(mat);
	// 変換行列を図形表示用に設定 
	DirectX::XMFLOAT4X4 fMat;
	DirectX::XMStoreFloat4x4(&fMat, mat);
	Geometory::SetWorld(fMat);

	// ボックス描画 
	Geometory::DrawBox();
#endif 
	//（モデルの表示処理が入るとGood！）
}

Collision::Box Block::GetCollision()
{
	return m_collision;
}

void Block::SetPos(DirectX::XMFLOAT3 pos)
{
	m_pos = pos;
}
