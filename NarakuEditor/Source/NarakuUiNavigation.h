#pragma once

#include "imgui.h"

namespace NarakuUi
{
    void BeginFrame(int screen);
    bool Begin(const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0);
    bool BeginPopupModal(const char* name, bool* open = nullptr, ImGuiWindowFlags flags = 0);
    bool BeginTabBar(const char* name, ImGuiTabBarFlags flags = 0);
    bool BackButton(const char* label, const ImVec2& size = ImVec2(0, 0));
}
