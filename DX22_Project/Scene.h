
#pragma once
;
class Scene
{
public:
	void RootUpdate();
	void RootDraw();

	Scene();
	virtual ~Scene();
	virtual void Update() = 0;
	virtual void Draw() = 0;


//protected:
//	Fade* m_pFade;  // フェード処理クラス 
//	int  m_next;  // 切り替え先のシーン 
public:
	// シーンで実行するフェードクラスを設定 
	//void SetFade(Fade* fade) { m_pFade = fade; }

	//// 基本クラスでは、フェードアウトの終了を検知してシーンの切り替えを有効にする 
	//virtual bool IsChangeScene();

	//// 次の切り替え先シーンを取得 
	//int GetNext() { return m_next; }

	//// 切り替え先のシーンを設定 
	//void SetNext(int next);
};;

