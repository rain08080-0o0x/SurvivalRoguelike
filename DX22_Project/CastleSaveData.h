#pragma once

class SceneCastleEditor;

class CastleSaveData
{
public:
    static bool Save(const SceneCastleEditor& editor, const char* path = nullptr);
    static bool Load(SceneCastleEditor& editor, const char* path = nullptr);
    static const char* GetDefaultPath();
};
