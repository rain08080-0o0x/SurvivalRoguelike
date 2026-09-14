#include "SceneManager.h"

#include "NarakuGameSession.h"
#include "NarakuPresentationScenes.h"
#include "SceneNarakuProto.h"

Scene* SceneManager::m_pScene = nullptr;
SceneManager::SceneType SceneManager::m_current = SceneManager::SCENE_GAME;
SceneManager::SceneType SceneManager::m_next = SceneManager::SCENE_GAME;
SceneManager::ResultType SceneManager::m_result = SceneManager::None;
bool SceneManager::m_isChanging = false;

void SceneManager::Init()
{
    m_current = SCENE_GAME;
    m_next = SCENE_GAME;
    m_isChanging = false;
    CreateScene(m_current);
}

void SceneManager::Uninit()
{
    delete m_pScene;
    m_pScene = nullptr;
    NarakuGameSession::Instance().Shutdown();
}

void SceneManager::CreateScene(SceneType type)
{
    delete m_pScene;
    m_pScene = nullptr;
    if (type == SCENE_GAME)
    {
        m_pScene = new SceneNarakuTown();
    }
    else if (type == SCENE_NARAKU_PROTO)
    {
        m_pScene = new SceneNarakuDive();
    }
    else if (type == SCENE_RESULT)
    {
        m_pScene = new SceneNarakuResult();
    }
}

void SceneManager::ChangeScene(SceneType next)
{
    if ((next == SCENE_GAME || next == SCENE_NARAKU_PROTO || next == SCENE_RESULT) &&
        next != m_current)
    {
        m_next = next;
        m_isChanging = true;
    }
}

SceneManager::ResultType SceneManager::GetResultType() { return m_result; }
void SceneManager::ChangeResult(ResultType result) { m_result = result; }

void SceneManager::Update()
{
    if (m_isChanging)
    {
        m_current = m_next;
        CreateScene(m_current);
        m_isChanging = false;
    }
    if (m_pScene != nullptr)
    {
        m_pScene->RootUpdate();
    }

    const SceneNarakuProto::PresentationScene presentation =
        NarakuGameSession::Instance().GetRuntime().GetPresentationScene();
    const SceneType desired = presentation == SceneNarakuProto::PresentationScene::Town
        ? SCENE_GAME
        : (presentation == SceneNarakuProto::PresentationScene::Result ? SCENE_RESULT : SCENE_NARAKU_PROTO);
    ChangeScene(desired);
}

void SceneManager::Draw()
{
    if (m_pScene != nullptr)
    {
        m_pScene->RootDraw();
    }
}
