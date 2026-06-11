#include <windows.h>
#include "Defines.h"
#include "Main.h"
#include <stdio.h>
#include <crtdbg.h>

#include"imgui_impl_win32.h"
#include "DirectX.h"
#include "Input.h"
#include "SceneManager.h"
#include "SceneNarakuEditor.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// timeGetTime蜻ｨ繧翫・菴ｿ逕ｨ
// timeGetTime蜻ｨ繧翫・菴ｿ逕ｨ
#pragma comment(lib, "winmm.lib")

//--- 繝励Ο繝医ち繧､繝怜ｮ｣險
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
	bool g_isFullscreen = false;
	HWND g_mainWindow = nullptr;
	HMENU g_editorMenuBar = nullptr;
	WINDOWPLACEMENT g_windowPlacement = { sizeof(WINDOWPLACEMENT) };
	DWORD g_windowStyle = 0;
	DWORD g_windowExStyle = 0;

	HMENU CreateNarakuEditorNativeMenu()
	{
		HMENU menuBar = CreateMenu();
		HMENU fileMenu = CreatePopupMenu();
		HMENU settingsMenu = CreatePopupMenu();
		HMENU alphaMenu = CreatePopupMenu();
		HMENU detailedLimitMenu = CreatePopupMenu();
		HMENU viewMenu = CreatePopupMenu();

		AppendMenuW(fileMenu, MF_STRING, SceneNarakuEditor::MenuSaveMap, L"\x4FDD\x5B58");
		AppendMenuW(fileMenu, MF_STRING, SceneNarakuEditor::MenuSaveMapAs, L"\x5225\x540D\x4FDD\x5B58...");
		AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(fileMenu, MF_STRING, SceneNarakuEditor::MenuOpenMap, L"\x958B\x304F...");
		AppendMenuW(fileMenu, MF_STRING, SceneNarakuEditor::MenuCreateNewMap, L"\x65B0\x898F");

		AppendMenuW(alphaMenu, MF_STRING, SceneNarakuEditor::MenuFrontAlpha015, L"0.15");
		AppendMenuW(alphaMenu, MF_STRING, SceneNarakuEditor::MenuFrontAlpha030, L"0.30");
		AppendMenuW(alphaMenu, MF_STRING, SceneNarakuEditor::MenuFrontAlpha050, L"0.50");
		AppendMenuW(alphaMenu, MF_STRING, SceneNarakuEditor::MenuFrontAlpha075, L"0.75");
		AppendMenuW(alphaMenu, MF_STRING, SceneNarakuEditor::MenuFrontAlpha100, L"1.00");

		AppendMenuW(detailedLimitMenu, MF_STRING, SceneNarakuEditor::MenuDetailedLimit128, L"128");
		AppendMenuW(detailedLimitMenu, MF_STRING, SceneNarakuEditor::MenuDetailedLimit256, L"256");
		AppendMenuW(detailedLimitMenu, MF_STRING, SceneNarakuEditor::MenuDetailedLimit512, L"512");
		AppendMenuW(detailedLimitMenu, MF_STRING, SceneNarakuEditor::MenuDetailedLimit1024, L"1024");
		AppendMenuW(detailedLimitMenu, MF_STRING, SceneNarakuEditor::MenuDetailedLimit2048, L"2048");
		AppendMenuW(detailedLimitMenu, MF_STRING, SceneNarakuEditor::MenuDetailedLimit4096, L"4096");

		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleAutoFocus, L"\x81EA\x52D5\x30D5\x30A9\x30FC\x30AB\x30B9\x540C\x671F");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleDisplaySettingsWindow, L"\x8868\x793A\x8A2D\x5B9A\x30A6\x30A3\x30F3\x30C9\x30A6");
		AppendMenuW(settingsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(alphaMenu), L"\x524D\x666F\x5730\x5F62\x30A2\x30EB\x30D5\x30A1");
		AppendMenuW(settingsMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(detailedLimitMenu), L"\x9AD8\x7CBE\x7D30\x30BB\x30EB\x4E0A\x9650");
		AppendMenuW(settingsMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleFloorPreview, L"\x5E8A\x30D7\x30EC\x30D3\x30E5\x30FC");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleGridLines, L"\x30B0\x30EA\x30C3\x30C9\x7DDA");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleFullGridLines, L"\x5168\x30B0\x30EA\x30C3\x30C9\x7DDA");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleDetailedFloorPreview, L"\x9AD8\x7CBE\x7D30\x5E8A\x30D7\x30EC\x30D3\x30E5\x30FC");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleRopePreview, L"\x30ED\x30FC\x30D7\x8868\x793A");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleMiningPreview, L"\x63A1\x6398\x30DD\x30A4\x30F3\x30C8\x8868\x793A");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleStartReturnPreview, L"\x958B\x59CB/\x5E30\x9084\x5730\x70B9\x8868\x793A");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleBoundaryPreview, L"\x5883\x754C\x8868\x793A");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleHoveredVertexPreview, L"\x30DB\x30D0\x30FC\x9802\x70B9\x8868\x793A");
		AppendMenuW(settingsMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuTogglePositionSnap, L"\x4F4D\x7F6E\x30B9\x30CA\x30C3\x30D7\x6709\x52B9");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleCellCenterSnap, L"\x30BB\x30EB\x4E2D\x5FC3\x30B9\x30CA\x30C3\x30D7");
		AppendMenuW(settingsMenu, MF_STRING, SceneNarakuEditor::MenuToggleInvertOrbitY, L"Alt+\x5DE6\x30C9\x30E9\x30C3\x30B0Y\x53CD\x8EE2");

		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleMapWindow, L"\x5730\x5F62\x30A8\x30C7\x30A3\x30BF\x672C\x4F53");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleLayerWindow, L"\x30EC\x30A4\x30E4\x30FC");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleRopeWindow, L"\x30ED\x30FC\x30D7");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleMiningWindow, L"\x63A1\x6398\x30DD\x30A4\x30F3\x30C8");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleInspectorWindow, L"\x30A4\x30F3\x30B9\x30DA\x30AF\x30BF");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleFeatureWindow, L"\x6A5F\x80FD\x7D39\x4ECB");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleOperationLogWindow, L"\x64CD\x4F5C\x30ED\x30B0");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuToggleHelpWindow, L"\x64CD\x4F5C\x8AAC\x660E");
		AppendMenuW(viewMenu, MF_SEPARATOR, 0, nullptr);
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuSnapTopView, L"\x771F\x4E0A\x30D3\x30E5\x30FC");
		AppendMenuW(viewMenu, MF_STRING, SceneNarakuEditor::MenuFocusSelection, L"\x9078\x629E\x5BFE\x8C61\x3078\x30D5\x30A9\x30FC\x30AB\x30B9");

		AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
		AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(settingsMenu), L"Settings");
		AppendMenuW(menuBar, MF_POPUP, reinterpret_cast<UINT_PTR>(viewMenu), L"View");
		return menuBar;
	}

	SceneNarakuEditor* GetNarakuEditorScene()
	{
		if (SceneManager::GetCurrent() != SceneManager::SCENE_NARAKU_EDITOR)
		{
			return nullptr;
		}

		return dynamic_cast<SceneNarakuEditor*>(SceneManager::GetScene());
	}

	void UpdateNarakuEditorNativeMenu(HWND hWnd)
	{
		if (hWnd == nullptr)
		{
			return;
		}

		SceneNarakuEditor* editor = GetNarakuEditorScene();
		HMENU targetMenu = (editor != nullptr) ? g_editorMenuBar : nullptr;
		if (GetMenu(hWnd) != targetMenu)
		{
			SetMenu(hWnd, targetMenu);
			DrawMenuBar(hWnd);
		}

		if (editor != nullptr && g_editorMenuBar != nullptr)
		{
			editor->SyncNativeMenuState(g_editorMenuBar);
		}
	}

	bool DispatchNarakuEditorNativeMenu(HWND hWnd, UINT commandId)
	{
		SceneNarakuEditor* editor = GetNarakuEditorScene();
		if (editor == nullptr)
		{
			return false;
		}

		if (!editor->HandleNativeMenuCommand(commandId))
		{
			return false;
		}

		UpdateNarakuEditorNativeMenu(hWnd);
		return true;
	}

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


// 繧ｨ繝ｳ繝医Μ繝昴う繝ｳ繝・
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	//--- 螟画焚螳｣險
	WNDCLASSEX wcex;
	HWND hWnd;
	MSG message;

	// 繧ｦ繧｣繝ｳ繝峨け繝ｩ繧ｹ諠・ｱ縺ｮ險ｭ螳・
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

	// 繧ｦ繧｣繝ｳ繝峨え繧ｯ繝ｩ繧ｹ諠・ｱ縺ｮ逋ｻ骭ｲ
	if (!RegisterClassEx(&wcex))
	{
		MessageBox(NULL, "Failed to RegisterClassEx", "Error", MB_OK);
		return 0;
	}

	// 繧ｦ繧｣繝ｳ繝峨え縺ｮ菴懈・
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

	// 繧ｦ繧｣繝ｳ繝峨え縺ｮ陦ｨ遉ｺ
	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	RECT clientRect = {};
	GetClientRect(hWnd, &clientRect);
	UINT clientWidth = static_cast<UINT>(clientRect.right - clientRect.left);
	UINT clientHeight = static_cast<UINT>(clientRect.bottom - clientRect.top);
	if (clientWidth == 0) clientWidth = SCREEN_WIDTH;
	if (clientHeight == 0) clientHeight = SCREEN_HEIGHT;

	// 蛻晄悄蛹門・逅・
	if (FAILED(Init(hWnd, clientWidth, clientHeight)))
	{
		Uninit();
		UnregisterClass(wcex.lpszClassName, hInstance);
		return 0;
	}

	g_editorMenuBar = CreateNarakuEditorNativeMenu();
	UpdateNarakuEditorNativeMenu(hWnd);

	//--- FPS蛻ｶ蠕｡
	timeBeginPeriod(1);
	DWORD countStartTime = timeGetTime();
	DWORD preExecTime = countStartTime;

	//--- 繧ｦ繧｣繝ｳ繝峨え縺ｮ邂｡逅・
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
				UpdateNarakuEditorNativeMenu(hWnd);
				Draw();
				preExecTime = nowTime;
			}
		}
	}


	// 邨ゆｺ・凾
	timeEndPeriod(1);
	if (g_editorMenuBar != nullptr)
	{
		SetMenu(hWnd, nullptr);
		DestroyMenu(g_editorMenuBar);
		g_editorMenuBar = nullptr;
	}
	Uninit();
	UnregisterClass(wcex.lpszClassName, hInstance);

	return 0;
}

// 繧ｦ繧｣繝ｳ繝峨え繝励Ο繧ｷ繝ｼ繧ｸ繝｣
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	const bool isRepeatKey = (lParam & 0x40000000) != 0;
	const bool isF11 = (message == WM_KEYDOWN) && (wParam == VK_F11);
	if (!isRepeatKey && isF11)
	{
		SetWindowFullscreen(hWnd, !g_isFullscreen);
		return 0;
	}

	if (message == WM_MOUSEWHEEL)
	{
		PushMouseWheelDelta(static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) /
			static_cast<float>(WHEEL_DELTA));
	}

	if (ImGui_ImplWin32_WndProcHandler(hWnd, message, wParam, lParam))
		return true;
	switch (message)
	{
	case WM_COMMAND:
		if (DispatchNarakuEditorNativeMenu(hWnd, LOWORD(wParam)))
		{
			return 0;
		}
		break;
	case WM_SIZE:
		// 譛蟆丞喧荳ｭ縺ｯ菴輔ｂ縺励↑縺・
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
