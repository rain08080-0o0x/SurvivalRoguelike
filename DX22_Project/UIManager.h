#pragma once
#include <vector>

class UIObject;

class UIManager
{
public:
    void Add(UIObject* obj);
    void Remove(UIObject* obj);
    void Clear();

    void Update();
    void Draw();

private:
    std::vector<UIObject*> m_list;
};
