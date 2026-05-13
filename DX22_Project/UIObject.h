/*****************************************************************//**
 * \file   UIObject.h
 * \brief  
 * 
 * \author 山本郁也
 * \date   January 2026
 *********************************************************************/
#pragma once
#include <string>
#include "Sprite.h"
#include "Texture.h"
#include <DirectXMath.h>

class UIObject
{
public:
	UIObject();
	UIObject(std::string RelativeTexturePathFromTextureFolder);
	UIObject(float PositionX, float PositionY);
	UIObject(float PositionX, float PositionY, float Width, float Height);
	UIObject(float PositionX, float PositionY, float Width, float Height, float RotationX, float RotationY, float RotationZ);
	UIObject(std::string RelativeTexturePathFromTextureFolder, float PositionX, float PositionY);
	UIObject(std::string RelativeTexturePathFromTextureFolder, float PositionX, float PositionY, float Width, float Height);
	UIObject(std::string RelativeTexturePathFromTextureFolder, float PositionX, float PositionY, float Width, float Height, float RotationX, float RotationY, float RotationZ);
	~UIObject();
	virtual void Update() {};
	void Draw();

	// 追加：UI描画の前後で1回だけ呼ぶ
	static void Begin2D();
	static void End2D();
	//===== Setter =====//

	void SetPosition(float X, float Y);
	void SetSize(float W, float H);
	void SetRotation(float X, float Y, float Z);
	void SetTexture(std::string RelativeTexturePathFromTextureFolder);
	void SetUVPosition(float X, float Y);
	void SetUVScale(float X, float Y);
	void SetColor(float R, float G, float B, float A);
	void GenerateGradient(float fromR, float fromG, float fromB, float fromA, float toR, float toG, float toB, float toA, int degree);

	//===== Getter =====//

	DirectX::XMFLOAT2 GetPosition(void);
	DirectX::XMFLOAT2 GetSize(void);
	DirectX::XMFLOAT3 GetRotation(void);
	DirectX::XMFLOAT2 GetUVPosition(void);
	DirectX::XMFLOAT2 GetUVScale(void);
	DirectX::XMFLOAT4 GetColor(int index = 0);

	//===== Setter as XMFLOATx =====//

	void SetPosition(DirectX::XMFLOAT2 pos);
	void SetSize(DirectX::XMFLOAT2 size);
	void SetRotation(DirectX::XMFLOAT3 rotation);
	void SetUVPosition(DirectX::XMFLOAT2 uvPos);
	void SetUVScale(DirectX::XMFLOAT2 uvScale);
	void SetColor(DirectX::XMFLOAT4 color);
	void GenerateGradient(DirectX::XMFLOAT4 colorFrom, DirectX::XMFLOAT4 colorTo, int degree);
private:
	Texture* m_pTexture;
	DirectX::XMFLOAT2 m_fPosition;
	DirectX::XMFLOAT2 m_fSize;
	DirectX::XMFLOAT3 m_fRotation;
	DirectX::XMFLOAT2 m_fUVPositon;
	DirectX::XMFLOAT2 m_fUVScale;
	DirectX::XMFLOAT4 m_fColor[4];
	// Begin/Endの内部状態
	static bool s_is2DBegin;
	static RenderTarget* s_prevRTV;
	static DepthStencil* s_prevDSV;
};