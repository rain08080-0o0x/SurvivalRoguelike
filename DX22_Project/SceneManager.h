#pragma once
#include "Scene.h"

class SceneManager
{
public:
    enum SceneType
    {
        SCENE_TITLE = 0,
        SCENE_GAME,
        SCENE_RESULT,
        SCENE_ENGINE_EDITOR,
        SCENE_EFFECT_DEBUG,
        SCENE_BOSS_EDITOR,
        SCENE_FINAL_BOSS_EDITOR,
        SCENE_NEW_BOSS_EDITOR,
        SCENE_NARAKU_EDITOR,
        SCENE_NARAKU_PROTO,
        SCENE_MAX
    };

    enum ResultType
    {
        None,
        Win,
        Lose,
    };

public:
    static void Init();
    static void Uninit();
    static void Update();
    static void Draw();

    static void ChangeScene(SceneType next);
    static SceneType GetCurrent() { return m_current; }
    static Scene* GetScene() { return m_pScene; }

    static ResultType GetResultType();
    static void ChangeResult(ResultType set);

private:
    static void CreateScene(SceneType type);

private:
    static ResultType m_result;
    static Scene* m_pScene;
    static SceneType m_current;
    static SceneType m_next;
    static bool m_isChanging;
};

