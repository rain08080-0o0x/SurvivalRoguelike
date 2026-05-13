//#include "Fade.h"
//
//
//Fade::Fade() 
//	: m_isFadeIn(false)
//	, m_time(0.0f)
//	, m_maxTime(0.0f) 
//{
//
//}
//
//Fade::~Fade()
//{
//}
//
//void Fade::Update()
//{
//	// フェードの残り時間があるか判定 
//	if (IsFinish()) { return; }
//
//	// フェードのカウントダウン 
//	m_time -= 1.0f / 60.0f;
//	if (m_time < 0.0f)
//		m_time = 0.0f;
//}
//
//void Fade::Draw()
//{
//	if (IsFadeIn())
//		DrawFadeIn(GetAlpha());
//	else
//		DrawFadeOut(GetAlpha());
//}
//
//void Fade::Start(float time, bool isIn)
//{
//	m_isFadeIn = isIn;
//	m_time = time;
//	m_maxTime = time;
//}
//
//float Fade::GetRate()
//{
//	// フェード時間が0の場合は0で返す（0除算回避のため 
//	if (m_maxTime == 0.0f) { return 0.0f; }
//
//	return m_time / m_maxTime;
//}
//
//float Fade::GetAlpha()
//{
//	float alpha = GetRate();
//
//	// フェードアウトの時だけ透明度を変更 
//	if (IsFadeOut()) {
//		// 開始で0、終了で1となるよう計算（フェードインは開始で1,終了で0  
//		alpha = 1.0f - alpha;
//	}
//
//	return alpha;
//}
