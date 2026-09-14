#include <windows.h>

#include "Defines.h"
#include "DirectX.h"
#include "Input.h"
#include "Main.h"
#include "imgui_impl_win32.h"

#include <crtdbg.h>
#include <direct.h>

#pragma comment(lib, "winmm.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
    bool g_fullscreen = false;
    HWND g_window = nullptr;
    WINDOWPLACEMENT g_placement = { sizeof(WINDOWPLACEMENT) };
    DWORD g_windowStyle = 0;
    DWORD g_windowExStyle = 0;

    bool DirectoryExists(const wchar_t* path)
    {
        const DWORD attributes = GetFileAttributesW(path);
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    void ConfigureRuntimeWorkingDirectory()
    {
        if (DirectoryExists(L"Assets"))
        {
            return;
        }
        wchar_t executablePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, executablePath, MAX_PATH) == 0)
        {
            return;
        }
        wchar_t* separator = wcsrchr(executablePath, L'\\');
        if (separator != nullptr)
        {
            *separator = L'\0';
            SetCurrentDirectoryW(executablePath);
        }
    }

    void MigrateLegacySaveIfNeeded()
    {
        const wchar_t* destinationDirectory = L"Assets\\Save";
        const wchar_t* destination = L"Assets\\Save\\naraku_proto_save.dat";
        if (GetFileAttributesW(destination) != INVALID_FILE_ATTRIBUTES)
        {
            return;
        }

        CreateDirectoryW(L"Assets", nullptr);
        CreateDirectoryW(destinationDirectory, nullptr);
        const wchar_t* candidates[] =
        {
            L"..\\Assets\\Save\\naraku_proto_save.dat",
            L"..\\DX22_Project\\Assets\\Save\\naraku_proto_save.dat",
            L"..\\..\\..\\Assets\\Save\\naraku_proto_save.dat",
            L"..\\..\\..\\DX22_Project\\Assets\\Save\\naraku_proto_save.dat",
            L"..\\..\\..\\NarakuGame\\Assets\\Save\\naraku_proto_save.dat"
        };
        for (const wchar_t* source : candidates)
        {
            if (GetFileAttributesW(source) != INVALID_FILE_ATTRIBUTES &&
                CopyFileW(source, destination, TRUE))
            {
                break;
            }
        }
    }

    void SetWindowFullscreen(HWND window, bool fullscreen)
    {
        if (window == nullptr || fullscreen == g_fullscreen)
        {
            return;
        }

        if (fullscreen)
        {
            g_windowStyle = static_cast<DWORD>(GetWindowLongPtr(window, GWL_STYLE));
            g_windowExStyle = static_cast<DWORD>(GetWindowLongPtr(window, GWL_EXSTYLE));
            GetWindowPlacement(window, &g_placement);
            MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
            GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo);
            SetWindowLongPtr(window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            SetWindowLongPtr(window, GWL_EXSTYLE, WS_EX_APPWINDOW);
            SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
        else
        {
            SetWindowLongPtr(window, GWL_STYLE, g_windowStyle);
            SetWindowLongPtr(window, GWL_EXSTYLE, g_windowExStyle);
            SetWindowPlacement(window, &g_placement);
            SetWindowPos(window, nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        }
        g_fullscreen = fullscreen;
    }
}

void SetAppFullscreen(bool fullscreen) { SetWindowFullscreen(g_window, fullscreen); }
void ToggleAppFullscreen() { SetWindowFullscreen(g_window, !g_fullscreen); }
bool IsAppFullscreen() { return g_fullscreen; }

LRESULT CALLBACK NarakuGameWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_KEYDOWN && wParam == VK_F11 && (lParam & 0x40000000) == 0)
    {
        ToggleAppFullscreen();
        return 0;
    }
    if (message == WM_MOUSEWHEEL)
    {
        PushMouseWheelDelta(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
            static_cast<float>(WHEEL_DELTA));
    }
    if (ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
    {
        return TRUE;
    }

    switch (message)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            OnResizeDirectX(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int commandShow)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    ConfigureRuntimeWorkingDirectory();
    MigrateLegacySaveIfNeeded();
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(WNDCLASSEXW);
    windowClass.style = CS_CLASSDC | CS_DBLCLKS;
    windowClass.lpfnWndProc = NarakuGameWndProc;
    windowClass.hInstance = instance;
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hIconSm = windowClass.hIcon;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    windowClass.lpszClassName = L"NarakuTowerGameWindow";
    if (!RegisterClassExW(&windowClass))
    {
        return 1;
    }

    RECT rectangle = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    const DWORD style = WS_CAPTION | WS_SYSMENU;
    const DWORD extendedStyle = WS_EX_OVERLAPPEDWINDOW;
    AdjustWindowRectEx(&rectangle, style, FALSE, extendedStyle);
    g_window = CreateWindowExW(extendedStyle, windowClass.lpszClassName, L"奈落塔", style,
        CW_USEDEFAULT, CW_USEDEFAULT, rectangle.right - rectangle.left,
        rectangle.bottom - rectangle.top, nullptr, nullptr, instance, nullptr);
    if (g_window == nullptr)
    {
        UnregisterClassW(windowClass.lpszClassName, instance);
        return 1;
    }

    ShowWindow(g_window, commandShow);
    UpdateWindow(g_window);
    RECT client = {};
    GetClientRect(g_window, &client);
    if (FAILED(Init(g_window, client.right - client.left, client.bottom - client.top)))
    {
        UnregisterClassW(windowClass.lpszClassName, instance);
        return 1;
    }

    timeBeginPeriod(1);
    DWORD previousFrame = timeGetTime();
    MSG message = {};
    bool running = true;
    while (running)
    {
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                running = false;
                break;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        if (!running)
        {
            break;
        }

        const DWORD now = timeGetTime();
        if (static_cast<float>(now - previousFrame) >= 1000.0f / fFPS)
        {
            Update();
            Draw();
            previousFrame = now;
        }
    }

    timeEndPeriod(1);
    Uninit();
    UnregisterClassW(windowClass.lpszClassName, instance);
    return 0;
}
