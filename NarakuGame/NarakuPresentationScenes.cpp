#include "NarakuPresentationScenes.h"

#include "NarakuGameSession.h"
#include "SceneNarakuProto.h"

namespace
{
    void UpdateRuntime()
    {
        NarakuGameSession::Instance().GetRuntime().Update();
    }

    void DrawRuntime()
    {
        NarakuGameSession::Instance().GetRuntime().Draw();
    }
}

void SceneNarakuTown::Update() { UpdateRuntime(); }
void SceneNarakuTown::Draw() { DrawRuntime(); }
void SceneNarakuDive::Update() { UpdateRuntime(); }
void SceneNarakuDive::Draw() { DrawRuntime(); }
void SceneNarakuResult::Update() { UpdateRuntime(); }
void SceneNarakuResult::Draw() { DrawRuntime(); }
