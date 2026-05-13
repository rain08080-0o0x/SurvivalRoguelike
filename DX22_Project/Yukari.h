#pragma once
#include "UIObject.h"

#include "Defines.h"

enum Yukari_Type
{
	None,
	Normal,
	Happy,
	UnHappy,
	Think
};

class Yukari
{
public:
	Yukari();
	~Yukari();

	void Update();
	void Draw();

	void SetType(Yukari_Type set);
private:
	Yukari_Type m_now;
	Yukari_Type m_old;
	UIObject* m_pYukari[4];
	UIObject* m_pFukidasi;
};

