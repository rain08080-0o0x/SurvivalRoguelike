#include "Scene.h"

Scene::Scene()
{
}
Scene::~Scene()
{
}
//bool Scene::IsChangeScene()
//{
//	if (m_pFade)
//		return m_pFade->IsFinish() && m_pFade->IsFadeOut();
//	return false;
//}
//
//void Scene::SetNext(int next)
//{
//	m_next = next;
//	// 切り替え先が発生した際にフェードも実行 
//	if (m_pFade)
//		m_pFade->Start(1, false);
//}

void Scene::RootUpdate()
{
	Update();
}
void Scene::RootDraw()
{
	Draw();
}