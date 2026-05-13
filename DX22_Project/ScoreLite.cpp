#include "ScoreLite.h"
#include <algorithm>

ScoreLite::ScoreLite(const char* digitsTexture, float x, float y, float digitW, float digitH, float spacing)
    : m_digits(nullptr)
    , m_score(0)
    , m_x(x)
    , m_y(y)
    , m_digitW(digitW)
    , m_digitH(digitH)
    , m_spacing(spacing)
    , m_colorR(1.0f)
    , m_colorG(1.0f)
    , m_colorB(1.0f)
    , m_colorA(1.0f)
{
    m_digits = new UIObject(digitsTexture, x, y, digitW, digitH);
    m_digits->SetUVPosition(0.0f, 0.0f);
    m_digits->SetUVScale(1.0f, 1.0f);
}

ScoreLite::~ScoreLite()
{
    delete m_digits;
    m_digits = nullptr;
}

void ScoreLite::SetScore(int v)
{
    m_score = std::max<int>(0, v);

}

void ScoreLite::AddScore(int v)
{
    m_score = std::max<int>(0, m_score + v);
}

int ScoreLite::GetScore() const
{
    return m_score;
}

void ScoreLite::SetPosition(float x, float y)
{
    m_x = x;
    m_y = y;
}

void ScoreLite::SetColor(float r, float g, float b, float a)
{
    m_colorR = r;
    m_colorG = g;
    m_colorB = b;
    m_colorA = a;
}

void ScoreLite::Draw()
{
    DrawNumberRightAligned(m_score);
}

void ScoreLite::DrawNumberRightAligned(int value)
{
    if (!m_digits) return;

    std::string s = std::to_string(std::max<int>(0, value));

    float offset = 0.0f;
    for (int i = 0; i < (int)s.size(); ++i)
    {
        const int digit = s[(int)s.size() - 1 - i] - '0';
        const float uvX = digit / 10.0f;
        const float uvW = 1.0f / 10.0f;

        const float x = m_x - offset;

        m_digits->SetPosition(x, m_y);
        m_digits->SetSize(m_digitW, m_digitH);
        m_digits->SetUVPosition(uvX, 0.0f);
        m_digits->SetUVScale(uvW, 1.0f);
        m_digits->SetColor(m_colorR, m_colorG, m_colorB, m_colorA);

        m_digits->Draw();

        offset += m_spacing;
    }
}