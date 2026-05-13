#pragma once
#include "Texture.h"
#include "Sprite.h"

class GaugeUI
{
private:
	Texture* m_pFrameTex;
	Texture* m_pGaugeTex;
	float m_rate;
	//  
	//  
	//  
public:
	//  
	GaugeUI();
	//  
	~GaugeUI();

	//  
	void Update();

	//  
	void Draw();

	//  
	void SetGauge(float rate);
};