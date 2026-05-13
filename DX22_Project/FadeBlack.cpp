//#include "FadeBlack.h"
//#include "Sprite.h"
//
//FadeBlack::FadeBlack() 
//    : m_pTexture(nullptr)
//{
//    m_pTexture = new Texture;
//    if (FAILED(m_pTexture->Create("Assets/Texture/Fade.png")))
//        MessageBox(NULL, "Load failed FadeBlack.", "Error", MB_OK);
//}
//
//FadeBlack::~FadeBlack()
//{
//    if (m_pTexture) {
//        delete m_pTexture;
//        m_pTexture = nullptr;
//    }
//}
//
//void FadeBlack::DrawFade(float alpha)
//{
//    DirectX::XMFLOAT4X4 fWVP[3];
//    //2Dの表示設定やスプライトに設定する変換行列(ワールド, ビュー, プロジェクション)を計算
//
//
//	DirectX::XMMATRIX world, view, proj; // 各変換行列の格納先 
//
//	// 作成した行列を各変数へ格納 
//	world = DirectX::XMMatrixTranslation(0.0f, 0.0f, 0.0f);
//	view = DirectX::XMMatrixLookAtLH(
//		DirectX::XMVectorSet(0.0f, 1.5f, -2.0f, 0.0f),
//		DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f),
//		DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
//	proj =
//		DirectX::XMMatrixOrthographicOffCenterLH(
//			-640, 640,	// 横下限上限値
//			-360, 360,	// 縦下限上限値
//			0.001f,		// Near
//			1000.0f);	// Far
//	proj = DirectX::XMMatrixPerspectiveFovLH(
//		// DirectXMathに用意されている角度をラジアン角に変換する関数
//		DirectX::XMConvertToRadians(70.0f),	//角度
//		16.0f / 9.0f,						//アス比
//		0.1f,								//最小描画距離
//		100.0f);							//最長描画距離
//
//
//
//	// 計算用のデータから読み取り用のデータに変換 
//	DirectX::XMStoreFloat4x4(&fWVP[0], DirectX::XMMatrixTranspose(world));
//	DirectX::XMStoreFloat4x4(&fWVP[1], DirectX::XMMatrixTranspose(view));
//	DirectX::XMStoreFloat4x4(&fWVP[2], DirectX::XMMatrixTranspose(proj));
//
//    // フェードの表示設定 
//    Sprite::SetWorld(fWVP[0]);
//    Sprite::SetView(fWVP[1]);
//    Sprite::SetProjection(fWVP[2]);
//    Sprite::SetTexture(m_pTexture);
//    Sprite::SetSize({ 1280,720 });
//    Sprite::SetOffset({ 0.0f, 0.0f });
//    Sprite::SetColor({ 0.0f, 0.0f, 0.0f, alpha }); // 引数のアルファを元に透明度を設定 
//
//	SetDepthTest(false);
//    // フェードの描画 
//    Sprite::Draw();
//	SetDepthTest(true);
//
//}
