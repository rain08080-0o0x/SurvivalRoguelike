#include "TrailEffect.h"

TrailEffect::TrailEffect(Player* pPlayer)
	: m_pPlayer(pPlayer)
{
	m_pPlayer = pPlayer;
	m_oldPos = m_pPlayer->GetPos();
}

void TrailEffect::UpdateControlPoints(LineID id, ControlPoints& controlPoints)
{
	// 毎フレームポリラインの幅を少しずつ小さくする 
	ControlPoints::iterator it = controlPoints.begin();
	while (it != controlPoints.end()) {
		it->bold *= 0.95f;
		++it;
	}
	// プレイヤーの移動ベクトルを計算 
	float distance = 0.0f;
	DirectX::XMFLOAT3 pos = m_pPlayer->GetPos();    // プレイヤー位置の取得 
	DirectX::XMVECTOR vOld = DirectX::XMLoadFloat3(&m_oldPos); // 前の位置 
	DirectX::XMVECTOR vNow = DirectX::XMLoadFloat3(&pos);  // 現在位置 
	DirectX::XMVECTOR vDir = DirectX::XMVectorSubtract(vNow,vOld);

	// ベクトルから距離を算出 
	DirectX::XMStoreFloat(&distance, DirectX::XMVector3Length(vDir));

	// プレイヤーが移動しているか判定  
	if (distance >= 0.2f) {
		// 制御点の情報を一つずつずらす 
		for (int i = controlPoints.size() - 1; i > 0; --i) {
			controlPoints[i] = controlPoints[i - 1];
		}

		// 先頭に新しいデータを設定 
		controlPoints[0].pos = pos;
		controlPoints[0].bold = 0.03f;

		// プレイヤーの配置位置を更新 
		m_oldPos = pos;
	}
}
