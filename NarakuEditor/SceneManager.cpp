#include "SceneManager.h"

#include "SceneNarakuEditor.h"
#include "SceneNarakuPieceEditor.h"
#include "SceneNarakuProto.h"

Scene* SceneManager::m_pScene = nullptr;
SceneManager::SceneType SceneManager::m_current = SceneManager::SCENE_NARAKU_PIECE_EDITOR;
SceneManager::SceneType SceneManager::m_next = SceneManager::SCENE_NARAKU_PIECE_EDITOR;
SceneManager::ResultType SceneManager::m_result = SceneManager::None;
bool SceneManager::m_isChanging = false;
bool SceneManager::m_openGeneratedPreviewOnNextEditor = false;

void SceneManager::Init()
{
    m_current = SCENE_NARAKU_PIECE_EDITOR;
    m_next = SCENE_NARAKU_PIECE_EDITOR;
    m_isChanging = false;
    m_openGeneratedPreviewOnNextEditor = false;
    CreateScene(m_current);
}

void SceneManager::Uninit()
{
    delete m_pScene;
    m_pScene = nullptr;
}

void SceneManager::CreateScene(SceneType type)
{
    delete m_pScene;
    m_pScene = nullptr;
    switch (type)
    {
    case SCENE_NARAKU_EDITOR:
    {
        auto* editor = new SceneNarakuEditor();
        if (m_openGeneratedPreviewOnNextEditor)
        {
            editor->ShowGeneratedPreviewWindow();
        }
        m_openGeneratedPreviewOnNextEditor = false;
        m_pScene = editor;
        break;
    }
    case SCENE_NARAKU_PIECE_EDITOR:
        m_pScene = new SceneNarakuPieceEditor();
        break;
    case SCENE_NARAKU_PROTO:
        m_pScene = new SceneNarakuProto();
        break;
    default:
        m_pScene = new SceneNarakuPieceEditor();
        m_current = SCENE_NARAKU_PIECE_EDITOR;
        m_next = SCENE_NARAKU_PIECE_EDITOR;
        break;
    }
}

void SceneManager::ChangeToNarakuEditorGeneratedPreview()
{
    m_openGeneratedPreviewOnNextEditor = true;
    ChangeScene(SCENE_NARAKU_EDITOR);
}

void SceneManager::ChangeScene(SceneType next)
{
    if (next != SCENE_NARAKU_EDITOR && next != SCENE_NARAKU_PIECE_EDITOR &&
        next != SCENE_NARAKU_PROTO)
    {
        return;
    }
    if (next != m_current)
    {
        m_next = next;
        m_isChanging = true;
    }
}

void SceneManager::ReloadCurrentScene()
{
    CreateScene(m_current);
    m_next = m_current;
    m_isChanging = false;
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
}

void SceneManager::Draw()
{
    if (m_pScene != nullptr)
    {
        m_pScene->RootDraw();
    }
}
