#pragma once

#include "Scene.h"

class SceneNarakuTown final : public Scene
{
public:
    void Update() override;
    void Draw() override;
};

class SceneNarakuDive final : public Scene
{
public:
    void Update() override;
    void Draw() override;
};

class SceneNarakuResult final : public Scene
{
public:
    void Update() override;
    void Draw() override;
};
