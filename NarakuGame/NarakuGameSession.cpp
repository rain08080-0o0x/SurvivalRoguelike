#include "NarakuGameSession.h"

#include "SceneNarakuProto.h"

NarakuGameSession& NarakuGameSession::Instance()
{
    static NarakuGameSession instance;
    return instance;
}

NarakuGameSession::~NarakuGameSession()
{
    Shutdown();
}

SceneNarakuProto& NarakuGameSession::GetRuntime()
{
    if (m_runtime == nullptr)
    {
        m_runtime = new SceneNarakuProto();
    }
    return *m_runtime;
}

void NarakuGameSession::Shutdown()
{
    delete m_runtime;
    m_runtime = nullptr;
}
