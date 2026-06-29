#include "SceneManager.h"
#include "SceneTitle.h"
#include "SceneGame.h"
#include "SceneResult.h"
#include "Scene3DEditor.h"
#include "SceneCastleEditor.h"
#include "SceneEffectDebug.h"
#include "SceneBossEditor.h"
#include "SceneFinalBossEditor.h"
#include "NewLastBossEditor.h"
#include "SceneNarakuEditor.h"
#include "SceneNarakuPieceEditor.h"
#include "SceneNarakuProto.h"

Scene* SceneManager::m_pScene = nullptr;
SceneManager::SceneType SceneManager::m_current = SceneManager::SCENE_NARAKU_PIECE_EDITOR;
SceneManager::SceneType SceneManager::m_next = SceneManager::SCENE_NARAKU_PIECE_EDITOR;
SceneManager::ResultType SceneManager::m_result = SceneManager::None;
bool SceneManager::m_isChanging = false;

void SceneManager::Init()
{
    m_current = SCENE_NARAKU_PIECE_EDITOR;
    m_next = SCENE_NARAKU_PIECE_EDITOR;
    m_isChanging = false;
    m_result = None;
    CreateScene(m_current);
}

void SceneManager::Uninit()
{
    if (m_pScene)
    {
        delete m_pScene;
        m_pScene = nullptr;
    }
}

void SceneManager::CreateScene(SceneType type)
{
    if (m_pScene)
    {
        delete m_pScene;
        m_pScene = nullptr;
    }

    switch (type)
    {
    case SCENE_TITLE:
        m_pScene = new SceneTitle();
        break;
    case SCENE_GAME:
        m_pScene = new SceneGame();
        break;
    case SCENE_RESULT:
        m_pScene = new SceneResult();
        break;
    case SCENE_ENGINE_EDITOR:
        m_pScene = new SceneCastleEditor();
        break;
    case SCENE_EFFECT_DEBUG:
        m_pScene = new SceneEffectDebug();
        break;
    case SCENE_BOSS_EDITOR:
        m_pScene = new SceneBossEditor();
        break;
    case SCENE_FINAL_BOSS_EDITOR:
        m_pScene = new SceneFinalBossEditor();
        break;
    case SCENE_NEW_BOSS_EDITOR:
        m_pScene = new NewLastBoss();
        break;
    case SCENE_NARAKU_EDITOR:
        m_pScene = new SceneNarakuEditor();
        break;
    case SCENE_NARAKU_PIECE_EDITOR:
        m_pScene = new SceneNarakuPieceEditor();
        break;
    case SCENE_NARAKU_PROTO:
        m_pScene = new SceneNarakuProto();
        break;
    default:
        m_pScene = new SceneTitle();
        break;
    }
}

void SceneManager::ChangeScene(SceneType next)
{
    // 同じシーンに変えるのは無視
    if (next == m_current) return;

    m_next = next;
    m_isChanging = true;
}

SceneManager::ResultType SceneManager::GetResultType()
{
    return m_result;
}

void SceneManager::ChangeResult(ResultType set)
{
    m_result = set;
}

void SceneManager::Update()
{
    if (m_isChanging)
    {
        // ここにフェード等を入れたいなら後で追加できる
        // 今は即切り替え
        m_current = m_next;
        CreateScene(m_current);
        m_isChanging = false;
    }

    if (m_pScene)
        m_pScene->RootUpdate();
}

void SceneManager::Draw()
{
    if (m_pScene)
        m_pScene->RootDraw();

    // フェード等を入れるなら、ここで上描き
}

