#ifndef __POLYLINE_EFFECT_H__
#define __POLYLINE_EFFECT_H__

#include "MeshBuffer.h"
#include "Shader.h"
#include <DirectXMath.h>

class PolylineEffect
{
protected:
	// ポリライン制御用の点
	struct ControlPoint
	{
		DirectX::XMFLOAT3	pos;	// 座標
		DirectX::XMFLOAT3	normal;	// 法線
		float				bold;	// 太さ
		DirectX::XMFLOAT4	color;	// 色
	};
	using ControlPoints = std::vector<ControlPoint>;

	// ポリライン表示用の頂点情報
	struct Vertex
	{
		DirectX::XMFLOAT3	pos;	// 座標
		DirectX::XMFLOAT2	uv;		// UV
		DirectX::XMFLOAT4	color;	// 色
	};

	// 1ライン当たりの情報
	struct Line
	{
		ControlPoints		controlPoints;	// 制御点
		DirectX::XMFLOAT2	uvScale;		// ポリラインに貼り付けるUVサイズ
		DirectX::XMFLOAT2	uvOffset;		// ポリラインに貼り付けるUVのオフセット
		Vertex*				ptr;			// 頂点情報
	};
	using Lines = std::vector<Line>;

public:
	using LineID = int;

public:
	static const LineID LINE_NONE = -1; // ラインが生成出来なかったときの情報

public:	
	// ポリライン全体で使用する頂点数
	PolylineEffect(int maxVertices = 1000);
	~PolylineEffect();

	virtual void Update();
	void Draw();

	// 制御点の数を指定して内部で扱うポリラインを生成
	LineID AddLine(int controlPointNum);

	void SetView(DirectX::XMFLOAT4X4 view);
	void SetProjection(DirectX::XMFLOAT4X4 proj);
	void SetTexture(Texture* pTexture);
	void SetVertexShader(VertexShader* vs);
	void SetPixelShader(PixelShader* ps);



protected:
	// 制御点の初期化
	virtual void InitControlPoints(LineID id, ControlPoints& controlPoints);
	// 制御点の更新処理
	virtual void UpdateControlPoints(LineID id, ControlPoints& controlPoints) = 0;

private:
	// シェーダー情報
	static VertexShader* m_pDefVS;
	static PixelShader* m_pDefPS;
	static int m_shaderRefCount;
	static Texture* m_pDefTex;

private:
	Lines		m_lines;		// ポリラインの情報
	int			m_useVtxCount;	// 使用済み頂点数
	MeshBuffer*	m_pMesh;		// 表示メッシュ
	Vertex*		m_pVertices;	// 頂点データ

	// 表示情報
	VertexShader* m_pVS;
	PixelShader* m_pPS;
	DirectX::XMFLOAT4X4 m_matrix[2];
	Texture* m_pTexture;
};

#endif // __POLYLINE_EFFECT_H__