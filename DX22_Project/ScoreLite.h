#pragma once
#include <string>
#include "UIObject.h"

class ScoreLite
{
public:
    ScoreLite(const char* digitsTexture, float x, float y, float digitW, float digitH, float spacing);
    ~ScoreLite();

    void SetScore(int v);
    void AddScore(int v);
    int GetScore() const;
    void SetPosition(float x, float y);

    void SetColor(float r, float g, float b, float a);
    void Draw();

private:
    void DrawNumberRightAligned(int value);

private:
    UIObject* m_digits;     // 0..9 が横並びの1枚
    int m_score;

    float m_x;
    float m_y;
    float m_digitW;
    float m_digitH;
    float m_spacing;

    float m_colorR;
    float m_colorG;
    float m_colorB;
    float m_colorA;
};
