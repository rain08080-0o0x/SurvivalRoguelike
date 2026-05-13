//#pragma once
//// 空行を削除
//
//class Fade 
//{
//private:
//	bool  m_isFadeIn; // フェードインかフェードアウトを判定するフラグ 
//	float m_time;  // 現在のフェードの残り時間 
//	float m_maxTime; // フェードの最大時間 
//
//public:
//	Fade();
//	virtual ~Fade();
//
//	// 更新処理 
//	void Update();
//
//	// 描画処理 
//	void Draw();
//
//	// フェードの開始処理 
//	void Start(float time, bool isIn);
//
//	// フェード各種判定 
//	bool IsFinish() { return m_time <= 0.0f; }
//	bool IsFadeIn() { return m_isFadeIn; }
//	bool IsFadeOut() { return !m_isFadeIn; }
//
//	// フェードの経過割合を取得（開始時点で１、終了時点で0） 
//	float GetRate();
//
//	// フェードの透明度を取得 
//	float GetAlpha();
//
//protected:
//	// フェードイン,フェードアウトの処理は継承先で実装 
//	virtual void DrawFadeIn(float alpha) = 0;
//	virtual void DrawFadeOut(float alpha) = 0;
//};
