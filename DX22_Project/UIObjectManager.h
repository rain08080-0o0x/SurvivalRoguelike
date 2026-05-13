#ifndef __UI_OBJECT_MANAGER_H__
#define __UI_OBJECT_MANAGER_H__

#include "UIManager.h"

class UIObject;

class UIObjectManager
{
public:
    enum class Layer
    {
        Game = 0,
        Debug,
        Count
    };

    void Add(UIObject* obj, Layer layer);
    void Remove(UIObject* obj);
    void Clear(Layer layer);
    void ClearAll();

    void Update(Layer layer);
    void Draw(Layer layer);
    void UpdateAll();
    void DrawAll();

private:
    UIManager m_layers[static_cast<int>(Layer::Count)];
};

#endif // __UI_OBJECT_MANAGER_H__