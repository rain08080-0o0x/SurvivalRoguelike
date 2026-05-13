#include "UIManager.h"
#include "UIObject.h"
#include <algorithm>

void UIManager::Add(UIObject* obj)
{
    if (!obj) return;
    m_list.push_back(obj);
}

void UIManager::Remove(UIObject* obj)
{
    auto it = std::remove(m_list.begin(), m_list.end(), obj);
    m_list.erase(it, m_list.end());
}

void UIManager::Clear()
{
    m_list.clear();
}

void UIManager::Update()
{
    for (auto* o : m_list) if (o) o->Update();
}

void UIManager::Draw()
{
    UIObject::Begin2D();
    for (auto* o : m_list) if (o) o->Draw();
    UIObject::End2D();
}
