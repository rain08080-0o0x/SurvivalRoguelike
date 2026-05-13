#pragma once
#include "GameObject.h"
#include "Camera.h"
#include "Collision.h"
#include "Transfer.h"
#include "Model.h"
#include "Defines.h"
#include <memory>
#include "UIManager.h"


class Dice
{
public:
    Dice();
    ~Dice();
    void Update(int);
    void Update(float dt = fFPS);
    void Draw();

    void SetCamera(Camera* pCamera);
    bool IsStop();


    void RollRandom(int index);

    void SetDiceTexture(int count,int num);

    bool isActive;
private:
    Camera* m_pCamera = nullptr;
    Model* m_pModel = nullptr;

    DirectX::XMFLOAT3 m_pos;    // 位置情報
    DirectX::XMFLOAT3 m_size;   // サイズ
    float m_mass;               // 質量

    DirectX::XMFLOAT3 vertex[8];
    // 物理本体
    RigidBodyOBB *body[MAX_DICE];

    // 表示用（必要なら）色などだけ Dice が持つ
    DirectX::XMFLOAT4 color = { 1,1,1,1 };


    UIObject* m_pDiceUI[3] = { nullptr, nullptr, nullptr };

};


