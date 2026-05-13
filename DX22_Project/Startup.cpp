#include <windows.h>
#include "Defines.h"
#include "Main.h"
#include <stdio.h>
#include <crtdbg.h>

#include"imgui_impl_win32.h"
#include "DirectX.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// timeGetTime周りの使用
// timeGetTime周りの使用
#pragma comment(lib, "winmm.lib")

//--- プロトタイプ宣言
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
	bool g_isFullscreen = false;
	HWND g_mainWindow = nullptr;
	WINDOWPLACEMENT g_windowPlacement = { sizeof(WINDOWPLACEMENT) };
	DWORD g_windowStyle = 0;
	DWORD g_windowExStyle = 0;

	void SetWindowFullscreen(HWND hWnd, bool fullscreen)
	{
		if (!hWnd || (g_isFullscreen == fullscreen))
			return;

		if (fullscreen)
		{
			g_windowStyle = static_cast<DWORD>(GetWindowLongPtr(hWnd, GWL_STYLE));
			g_windowExStyle = static_cast<DWORD>(GetWindowLongPtr(hWnd, GWL_EXSTYLE));
			g_windowPlacement.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(hWnd, &g_windowPlacement);

			HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO monitorInfo = {};
			monitorInfo.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(monitor, &monitorInfo);

			SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_APPWINDOW);
			SetWindowPos(
				hWnd,
				HWND_TOP,
				monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
				monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}
		else
		{
			SetWindowLongPtr(hWnd, GWL_STYLE, g_windowStyle);
			SetWindowLongPtr(hWnd, GWL_EXSTYLE, g_windowExStyle);
			SetWindowPlacement(hWnd, &g_windowPlacement);
			SetWindowPos(
				hWnd,
				nullptr,
				0, 0, 0, 0,
				SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
		}

		g_isFullscreen = fullscreen;
	}
}

void SetAppFullscreen(bool fullscreen)
{
	SetWindowFullscreen(g_mainWindow, fullscreen);
}

void ToggleAppFullscreen()
{
	SetWindowFullscreen(g_mainWindow, !g_isFullscreen);
}

bool IsAppFullscreen()
{
	return g_isFullscreen;
}


// エントリポイント
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//--- 変数宣言
	WNDCLASSEX wcex;
	HWND hWnd;
	MSG message;

	// ウィンドクラス情報の設定
	ZeroMemory(&wcex, sizeof(wcex));
	wcex.hInstance = hInstance;
	wcex.lpszClassName = "Class Name";
	wcex.lpfnWndProc = WndProc;
	wcex.style = CS_CLASSDC | CS_DBLCLKS;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hIconSm = wcex.hIcon;
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

	// ウィンドウクラス情報の登録
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, "Failed to RegisterClassEx", "Error", MB_OK);
		return 0;
	}

	// ウィンドウの作成
	RECT rect = {0, 0, SCREEN_WIDTH, SCREEN_HEIGHT};
	DWORD style = WS_CAPTION | WS_SYSMENU;
	DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;
	AdjustWindowRectEx(&rect, style, false, exStyle);
	hWnd = CreateWindowEx(
		exStyle, wcex.lpszClassName,
		APP_TITLE, style,
		CW_USEDEFAULT,CW_USEDEFAULT,
		rect.right - rect.left, rect.bottom - rect.top,
		HWND_DESKTOP,
		NULL, hInstance, NULL
	);
	g_mainWindow = hWnd;

	// ウィンドウの表示
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	RECT clientRect = {};
	GetClientRect(hWnd, &clientRect);
	UINT clientWidth = static_cast<UINT>(clientRect.right - clientRect.left);
	UINT clientHeight = static_cast<UINT>(clientRect.bottom - clientRect.top);
	if (clientWidth == 0) clientWidth = SCREEN_WIDTH;
	if (clientHeight == 0) clientHeight = SCREEN_HEIGHT;

	// 初期化処理
	if (FAILED(Init(hWnd, clientWidth, clientHeight)))
	{
		Uninit();
		UnregisterClass(wcex.lpszClassName, hInstance);
		return 0;
	}

	//--- FPS制御
	timeBeginPeriod(1);
	DWORD countStartTime = timeGetTime();
	DWORD preExecTime = countStartTime;

	//--- ウィンドウの管理
	while (1)
	{
		if (PeekMessage(&message, NULL, 0, 0, PM_NOREMOVE))
		{
			if (!GetMessage(&message, NULL, 0, 0))
			{
				break;
			}
			else
			{
				TranslateMessage(&message);
				DispatchMessage(&message);
			}
		}
		else
		{
			DWORD nowTime = timeGetTime();
			float diff = static_cast<float>(nowTime - preExecTime);
			if (diff >= 1000.0f / fFPS)
			{
				Update();
				Draw();
				preExecTime = nowTime;
			}
		}
	}


	// 終了時
	timeEndPeriod(1);
	Uninit();
	UnregisterClass(wcex.lpszClassName, hInstance);

	return 0;
}

// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const bool isRepeatKey = (lParam & 0x40000000) != 0;
	const bool isF11 = (message == WM_KEYDOWN) && (wParam == VK_F11);
	if (!isRepeatKey && isF11)
	{
		SetWindowFullscreen(hWnd, !g_isFullscreen);
		return 0;
	}

	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;
	switch (message)
	{
	case WM_SIZE:
		// 最小化中は何もしない
		if (wParam != SIZE_MINIMIZED)
		{
			UINT width = LOWORD(lParam);
			UINT height = HIWORD(lParam);
			OnResizeDirectX(width, height);
		}

		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, message, wParam, lParam);
}
