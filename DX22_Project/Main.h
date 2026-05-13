#ifndef __MAIN_H__
#define __MAIN_H__

#include <Windows.h>

HRESULT Init(HWND hWnd, UINT width, UINT height);
void Uninit();
void Update();
void Draw();
void SetAppFullscreen(bool fullscreen);
void ToggleAppFullscreen();
bool IsAppFullscreen();

#endif // __MAIN_H__
