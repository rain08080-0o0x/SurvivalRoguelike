#include "UIObjectManager.h"

void UIObjectManager::Add(UIObject* obj, Layer layer)
{
    m_layers[static_cast<int>(layer)].Add(obj);
}

void UIObjectManager::Remove(UIObject* obj)
{
    for (int i = 0; i < static_cast<int>(Layer::Count); ++i)
    {
        m_layers[i].Remove(obj);
    }
}

void UIObjectManager::Clear(Layer layer)
{
    m_layers[static_cast<int>(layer)].Clear();
}

void UIObjectManager::ClearAll()
{
    for (int i = 0; i < static_cast<int>(Layer::Count); ++i)
    {
        m_layers[i].Clear();
    }
}

void UIObjectManager::Update(Layer layer)
{
    m_layers[static_cast<int>(layer)].Update();
}

void UIObjectManager::Draw(Layer layer)
{
    m_layers[static_cast<int>(layer)].Draw();
}

void UIObjectManager::UpdateAll()
{
    for (int i = 0; i < static_cast<int>(Layer::Count); ++i)
    {
        m_layers[i].Update();
    }
}

void UIObjectManager::DrawAll()
{
    for (int i = 0; i < static_cast<int>(Layer::Count); ++i)
    {
        m_layers[i].Draw();
    }
}