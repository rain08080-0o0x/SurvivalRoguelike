/*
 * @
 * 内部でCOMオブジェクトを利用するため、LoadTexture関数より後にInitSound関数呼び出すと
 * エラーになる
 */
#ifndef __SOUND_H__
#define __SOUND_H__

#include <xaudio2.h>
#pragma comment(lib, "xaudio2.lib")

//----------
// プロトタイプ宣言
//----------
HRESULT InitSound(void);
void UninitSound(void);
void UpdateSound(void);
void SetMasterVolume(float volume);
void SetBgmVolume(float volume);
void SetSeVolume(float volume);

// サウンドファイルの読み込み
XAUDIO2_BUFFER* LoadSound(const char *file, bool loop = false);

// サウンドの再生
#undef PlaySound // winapiのPlaySoundを無効にする
IXAudio2SourceVoice* PlaySound(XAUDIO2_BUFFER* pSound);

#endif // __SOUND_H__
