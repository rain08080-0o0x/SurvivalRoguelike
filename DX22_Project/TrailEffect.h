#pragma once
#include "PolylineEffect.h" 
#include "Player.h" 

class Player;

class TrailEffect
	: public PolylineEffect
{
private:
	Player* m_pPlayer; // プレイヤー情報 
	DirectX::XMFLOAT3 m_oldPos;
	// 以前にプレイヤーがいた位置 
public:
	// 初期化処理 
	TrailEffect(Player* pPlayer);
	// 終了処理 
	virtual ~TrailEffect() {}
protected:
	// ポリライン制御点更新処理 
	void UpdateControlPoints(LineID id, ControlPoints& controlPoints) final;
};