#ifndef __DEFINES_H__
#define __DEFINES_H__

#include <assert.h>
#include <Windows.h>
#include <stdarg.h>
#include <stdio.h>

static const int FPS = 120;
static const float fFPS = static_cast<float>(FPS);

// リソースパス
#define ASSET(path)	"Assets/"path

// 3D空間定義
#define CMETER(value) (value * 0.01f)
#define METER(value) (value * 1.0f)
#define MSEC(value) (value / fFPS)
#define CMSEC(value) MSEC(CMETER(value))
#define HALF(test) (test / 2.0f)
static const float GRAVITY = 0.98f;

// タイトル
static const char* APP_TITLE = "Genbaneko";

// 画面サイズ
static const int SCREEN_WIDTH	= 1280;
static const int SCREEN_HEIGHT	= 720;

// グリッドサイズ(デバッグ用
static const int	DEBUG_GRID_NUM		= 10;			// グリッド中心から端までの線の本数
static const float	DEBUG_GRID_MARGIN	= METER(1.0f);	// グリッド配置幅

// 最大サイコロ数
#define MAX_DICE (10)

#define PI (3.1415926535F)

#endif // __DEFINES_H__