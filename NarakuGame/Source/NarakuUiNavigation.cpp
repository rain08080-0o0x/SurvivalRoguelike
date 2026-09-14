#include "NarakuUiNavigation.h"
#include "Input.h"
#include "imgui_internal.h"

namespace
{
    int g_screen = -1;
    bool g_focusPending = false;
    bool g_backConsumed = false;
    bool g_tabConsumed = false;

    bool BackRequested()
    {
        if (g_backConsumed || IsInputCaptureActive() || IsInputGuarded()) return false;
        ImGuiContext& context = *ImGui::GetCurrentContext();
        if (context.ActiveIdPreviousFrame != 0) return false;
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return false;
        if (!IsActionUIBackTrigger()) return false;
        g_backConsumed = true;
        return true;
    }
}

void NarakuUi::BeginFrame(int screen)
{
    g_focusPending = screen != g_screen;
    g_screen = screen;
    g_backConsumed = false;
    g_tabConsumed = false;
}

bool NarakuUi::Begin(const char* name, bool* open, ImGuiWindowFlags flags)
{
    const bool visible = ImGui::Begin(name, open, flags);
    if (visible && g_focusPending && !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImGui::FocusWindow(window, ImGuiFocusRequestFlags_RestoreFocusedChild);
        if (ImGui::GetCurrentContext()->NavId == 0) ImGui::NavInitWindow(window, false);
        g_focusPending = false;
    }
    return visible;
}

bool NarakuUi::BeginPopupModal(const char* name, bool* open, ImGuiWindowFlags flags)
{
    if (!ImGui::BeginPopupModal(name, open, flags)) return false;
    if (BackRequested())
    {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return false;
    }
    return true;
}

bool NarakuUi::BackButton(const char* label, const ImVec2& size)
{
    const bool clicked = ImGui::Button(label, size);
    return clicked || BackRequested();
}

bool NarakuUi::BeginTabBar(const char* name, ImGuiTabBarFlags flags)
{
    if (!ImGui::BeginTabBar(name, flags)) return false;
    if (!g_tabConsumed && !IsInputCaptureActive() && !IsInputGuarded() &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        const int step = IsActionUITabNextTrigger() ? 1 : IsActionUITabPrevTrigger() ? -1 : 0;
        ImGuiTabBar* bar = ImGui::GetCurrentContext()->CurrentTabBar;
        if (step && bar->Tabs.Size > 1)
        {
            for (int i = 0; i < bar->Tabs.Size; ++i)
                if (bar->Tabs[i].ID == bar->SelectedTabId)
                {
                    ImGui::TabBarQueueFocus(bar, &bar->Tabs[(i + step + bar->Tabs.Size) % bar->Tabs.Size]);
                    g_tabConsumed = true;
                    break;
                }
        }
    }
    return true;
}
