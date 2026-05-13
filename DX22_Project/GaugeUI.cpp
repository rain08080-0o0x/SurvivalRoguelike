#include "GaugeUI.h"
#include "DirectXMath.h"
#include "Defines.h"
#include "ShaderList.h"

GaugeUI::GaugeUI()
	: m_pFrameTex(nullptr)
	, m_pGaugeTex(nullptr)
	, m_rate(0.1f)
{
	//  
	m_pFrameTex = new Texture();
	if (FAILED(m_pFrameTex->Create("Assets/Texture/UIFrame.png"))) {
		MessageBox(NULL, "Texture load failed.¥nUI.cpp", "Error", MB_OK);
	}
	//  
	m_pGaugeTex = new Texture();
	if (FAILED(m_pGaugeTex->Create("Assets/Texture/UIGauge.png"))) {
		MessageBox(NULL, "Texture load failed.¥nUI.cpp", "Error", MB_OK);
	}
}

GaugeUI::~GaugeUI()
{
	if (m_pFrameTex) {
		delete m_pFrameTex;
		m_pFrameTex = nullptr;
	} if (m_pGaugeTex) {
		delete m_pGaugeTex;
		m_pGaugeTex = nullptr;
	}
}

void GaugeUI::Update()
{

}

void GaugeUI::Draw()
{
	// 2D 
	DirectX::XMFLOAT4X4 world, view, proj;
	DirectX::XMMATRIX mView = DirectX::XMMatrixLookAtLH(
		DirectX::XMVectorSet(0,0,-10,0),
		DirectX::XMVectorSet(0,0,0,0),
		DirectX::XMVectorSet(0,1,0,0)
	);
	DirectX::XMMATRIX mProj = DirectX::XMMatrixOrthographicOffCenterLH(
		0.0f,SCREEN_WIDTH,
		SCREEN_HEIGHT,0.0,
		1,
		100
	);
    DirectX::XMStoreFloat4x4(&view, DirectX::XMMatrixTranspose(mView));
    DirectX::XMStoreFloat4x4(&proj, DirectX::XMMatrixTranspose(mProj));

	//  
	Sprite::SetView(view);
	Sprite::SetProjection(proj);

	// (0) (1) 
	DirectX::XMFLOAT2 pos = { SCREEN_WIDTH * 0.5f,SCREEN_HEIGHT * 0.5f };
	DirectX::XMFLOAT2 size[2] = { {256,66},{258,64} };
	Texture* pTexture[2] = { m_pFrameTex,m_pGaugeTex };

	// (0) (1) 
	for (int i = 1; i >= 0; --i) {
		//  
		DirectX::XMMATRIX T =
			DirectX::XMMatrixTranslation(pos.x - size[i].x * 0.5f, pos.y, 0.0f);
		DirectX::XMMATRIX S;
		if (i == 0) //  
			S = DirectX::XMMatrixScaling(1.0f, -1.0f, 1.0f);
		else   // m_rate 
			S = DirectX::XMMatrixScaling(m_rate, -0.6f, 1.0f);
		DirectX::XMMATRIX mWorld = S * T;
		DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranspose(mWorld));

		Sprite::SetWorld(world);      //  
		Sprite::SetSize(size[i]);      //  
		Sprite::SetOffset({ size[i].x * 0.5f, 0.0f }); //  
		Sprite::SetColor({ 1.0f,1.0f,1.0f,1.0f });
		Sprite::SetTexture(pTexture[i]);    //  
		Sprite::Draw();
	}
}

void GaugeUI::SetGauge(float rate)
{
	m_rate = rate;
}
