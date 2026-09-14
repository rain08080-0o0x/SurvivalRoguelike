#pragma once

class SceneNarakuProto;

class NarakuGameSession
{
public:
    static NarakuGameSession& Instance();

    SceneNarakuProto& GetRuntime();
    void Shutdown();

private:
    NarakuGameSession() = default;
    ~NarakuGameSession();
    NarakuGameSession(const NarakuGameSession&) = delete;
    NarakuGameSession& operator=(const NarakuGameSession&) = delete;

    SceneNarakuProto* m_runtime = nullptr;
};
