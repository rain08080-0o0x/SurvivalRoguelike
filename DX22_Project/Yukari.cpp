#include "Yukari.h"
#include "Transfer.h"

Yukari::Yukari()
	: m_now(Yukari_Type::Normal)
	, m_old(Yukari_Type::Normal)
	, m_pYukari{ nullptr, nullptr, nullptr, nullptr }
	, m_pFukidasi(nullptr)
{
	TRAN_INS;

	DirectX::XMFLOAT2 size = { 500.0f,500.0f };
	DirectX::XMFLOAT2 pos = { SCREEN_WIDTH - (size.x / 2.0f) + 100.0f, SCREEN_HEIGHT - (size.y / 2.0f * 0.75f) };
	tran.yukari.pos = pos;
	tran.yukari.size = size;
	m_pYukari[0] = new UIObject("Yukari/現場猫.png", pos.x, pos.y, size.x, size.y);
	m_pYukari[1] = new UIObject("Yukari/happy.png", pos.x, pos.y, size.x, size.y);
	m_pYukari[2] = new UIObject("Yukari/unhappy.png", pos.x, pos.y, size.x, size.y);
	m_pYukari[3] = new UIObject("Yukari/think.png", pos.x, pos.y, size.x, size.y);



	// ここから吹き出し
	"Assets/Texture/Yukari/talk/iidesune.png";
	"Assets/Texture/Yukari/talk/ikimasuyo.png";
	"Assets/Texture/Yukari/talk/sate.png";
	"Assets/Texture/Yukari/talk/soukimasitaka.png";
	"Assets/Texture/Yukari/talk/nagare.png";
	"Assets/Texture/Yukari/talk/owattenai.png";

	pos = { 640,638 };
	size = { 656,151 };//3616:779
	tran.fuki.pos = pos;
	tran.fuki.size = size;
	m_pFukidasi = new UIObject("Yukari/talk/sate.png", pos.x, pos.y, size.x, size.y);

}

Yukari::~Yukari()
{
	for (int i = 0; i < 4; ++i)
	{
		delete m_pYukari[i];
		m_pYukari[i] = nullptr;
	}

	delete m_pFukidasi;
	m_pFukidasi = nullptr;
}


void Yukari::Update()
{
	m_old = m_now;
}

void Yukari::Draw()
{
	TRAN_INS;
	for (int i = 0; i < 4; i++)
	{
		if (!m_pYukari[i])
		{
			break;
		}

		DirectX::XMFLOAT2 pos;
		DirectX::XMFLOAT2 size;
		pos = tran.yukari.pos;
		size = tran.yukari.size;
		switch (m_now)
		{
		case None:
			break;
		case Normal:
			size.x *= 0.75f;
			m_pYukari[0]->SetPosition(pos);
			m_pYukari[0]->SetSize(size);
			m_pYukari[0]->Draw();
			break;
		case Happy:
			m_pYukari[1]->SetPosition(pos);
			m_pYukari[1]->SetSize(size);
			m_pYukari[1]->Draw();
			break;
		case UnHappy:
			m_pYukari[2]->SetPosition(pos);
			m_pYukari[2]->SetSize(size);
			m_pYukari[2]->Draw();
			break;
		case Think:
			m_pYukari[3]->SetPosition(pos);
			m_pYukari[3]->SetSize(size);
			m_pYukari[3]->Draw();
			break;
		default:
			break;
		}
	}
	if (m_pFukidasi)
	{
		static bool check;
		if (!check)
		{
			switch (m_now)
			{
			case None:
				break;
			case Normal:
				break;
			case Happy:
				switch (rand() % 2)
				{
				case 0:
					m_pFukidasi->SetTexture("Yukari/talk/iidesune.png");
					break;
				case 1:
					m_pFukidasi->SetTexture("Yukari/talk/nagare.png");
					break;
				}
				break;
			case UnHappy:
				switch (rand() % 2)
				{
				case 0:
					m_pFukidasi->SetTexture("Yukari/talk/owattenai.png");
					break;
				case 1:
					m_pFukidasi->SetTexture("Yukari/talk/soukimasitaka.png");
					break;
				}
				break;
			case Think:
				switch (rand() % 2)
				{
				case 0:
					m_pFukidasi->SetTexture("Yukari/talk/ikimasuyo.png");
					break;
				case 1:
					m_pFukidasi->SetTexture("Yukari/talk/sate.png");
					break;
				}

				break;
			default:
				break;
			}
			check = true;
		}
		if (m_old != m_now)
		{
			check = false;
		}
		m_pFukidasi->SetPosition(tran.fuki.pos);
		m_pFukidasi->SetSize(tran.fuki.size);
		m_pFukidasi->Draw();
	}
}

void Yukari::SetType(Yukari_Type set)
{
	m_now = set;
}
