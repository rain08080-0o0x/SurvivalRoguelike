#include "Geometory.h"
#include<cmath>
#include<vector>
using namespace std;

const int VERTEX_CYLINDER = 10;

void Geometory::MakeBox()
{
	//--- 頂点の作成

	Vertex vtx[] = {
	// -Z面
	{{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}}, // 0:左上
	{{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}}, // 1:右上
	{{-0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}}, // 2:左下
	{{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}}, // 3:右下

	// +Z面（奥）
	{{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}}, // 4:左上
	{{-0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}}, // 5:右上
	{{ 0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}}, // 6:左下
	{{-0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}}, // 7:右下

	// +X面（右）
	{{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f}}, // 8:左上
	{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}}, // 9:右上
	{{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f}}, // 10:左下
	{{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}}, // 11:右下

	// -X面（左）
	{{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}}, // 12:左上
	{{-0.5f,  0.5f, -0.5f}, {1.0f, 0.0f}}, // 13:右上
	{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}}, // 14:左下
	{{-0.5f, -0.5f, -0.5f}, {1.0f, 1.0f}}, // 15:右下

	// +Y面（上）
	{{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f}}, // 16:左上
	{{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f}}, // 17:右上
	{{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f}}, // 18:左下
	{{ 0.5f,  0.5f, -0.5f}, {1.0f, 1.0f}}, // 19:右下

	// -Y面（下）
	{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f}}, // 20:左上
	{{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f}}, // 21:右上
	{{-0.5f, -0.5f,  0.5f}, {0.0f, 1.0f}}, // 22:左下
	{{ 0.5f, -0.5f,  0.5f}, {1.0f, 1.0f}}, // 23:右下
	};
	//--- インデックスの作成
	int idx[] =
	{
		0, 1, 2,   1, 3, 2,   // -Z面
		4, 5, 6,   5, 7, 6,   // +Z面
		8, 9, 10,  9, 11, 10,  // +X面
		12, 13, 14, 13, 15, 14, // -X面
		16, 17, 18, 17, 19, 18, // +Y面
		20, 21, 22, 21, 23, 22  // -Y面
	};
	// バッファの作成
	MeshBuffer::Description desc = {};
	desc.pVtx = vtx;
	desc.vtxCount = 24;
	desc.vtxSize = 20;
	desc.pIdx = idx;
	desc.idxCount = 36;
	desc.idxSize = 4;
	desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pBox = new MeshBuffer();
	m_pBox->Create(desc);
}

void Geometory::MakeCylinder()
{
	//--- 円柱の頂点とインデックスの作成
	const int circleVtx = 16; // 円周の分割数
	const float radius = 0.5f;
	const float height = 1.0f;

	std::vector<Vertex> vtxVec;
	std::vector<int> idxVec;

	// 頂点インデックス:
	// 0: 上面中央
	// 1～circleVtx: 上面ྃ䍋缘
	// circleVtx+1～circleVtx*2+1: 下面中央と下面缘
	// circleVtx*2+2～circleVtx*3+1: 側面上辺
	// circleVtx*3+2～circleVtx*4+1: 側面下辺

	// 上面中央
	vtxVec.push_back({ {0.0f, height / 2.0f, 0.0f}, {0.5f, 0.5f} });
	// 上面外周
	for (int i = 0; i < circleVtx; ++i)
	{
		float angle = 2.0f * 3.14159f * i / circleVtx;
		float x = radius * cosf(angle);
		float z = radius * sinf(angle);
		// UV: 上面は中央から外側へ同心円状に
		float u = 0.5f + 0.5f * cosf(angle);
		float v = 0.5f + 0.5f * sinf(angle);
		vtxVec.push_back({ {x, height / 2.0f, z}, {u, v} });
	}

	// 上面のインデックス（扇型）
	for (int i = 1; i < circleVtx; ++i)
	{
		idxVec.push_back(0);
		idxVec.push_back(i + 1);
		idxVec.push_back(i);
	}
	idxVec.push_back(0);
	idxVec.push_back(1);
	idxVec.push_back(circleVtx);

	// 下面中央
	int bottomCenterIdx = vtxVec.size();
	vtxVec.push_back({ {0.0f, -height / 2.0f, 0.0f}, {0.5f, 0.5f} });
	// 下面外周
	for (int i = 0; i < circleVtx; ++i)
	{
		float angle = 2.0f * 3.14159f * i / circleVtx;
		float x = radius * cosf(angle);
		float z = radius * sinf(angle);
		// UV: 下面も中央から外側へ同心円状に
		float u = 0.5f + 0.5f * cosf(-angle);
		float v = 0.5f + 0.5f * sinf(-angle);

		//float u = 0.5f - cosf(angle) * 0.5f;  // X軸を反転
		//float v = 0.5f - sinf(angle) * 0.5f;  // Y軸を反転

		vtxVec.push_back({ {x, -height / 2.0f, z}, {u, v} });
	}

	// 下面のインデックス（扇型）
	int bottomStart = bottomCenterIdx + 1;
	for (int i = bottomStart; i < bottomStart + circleVtx - 1; ++i)
	{
		idxVec.push_back(bottomCenterIdx);
		idxVec.push_back(i + 1);
		idxVec.push_back(i);
	}
	idxVec.push_back(bottomCenterIdx);
	idxVec.push_back(bottomStart);
	idxVec.push_back(bottomStart + circleVtx - 1);

	// 側面の上の頂点
	int sideTopStart = vtxVec.size();
	for (int i = 0; i < circleVtx; ++i)
	{
		float angle = 2.0f * 3.14159f * i / circleVtx;
		float x = radius * cosf(angle);
		float z = radius * sinf(angle);
		// UV: U方向は円周方向(0～1)、V方向は高さ1.0
		vtxVec.push_back({ {x, height / 2.0f, z}, {i / (float)circleVtx, 1.0f} });
	}

	// 側面の下の頂点
	int sideBottomStart = vtxVec.size();
	for (int i = 0; i < circleVtx; ++i)
	{
		float angle = 2.0f * 3.14159f * i / circleVtx;
		float x = radius * cosf(angle);
		float z = radius * sinf(angle);
		// UV: U方向は円周方向(0～1)、V方向は高さ0.0
		vtxVec.push_back({ {x, -height / 2.0f, z}, {i / (float)circleVtx, 0.0f} });
	}

	// 側面のインデックス
	for (int i = 0; i < circleVtx; ++i)
	{
		int next = (i + 1) % circleVtx;
		// 四角形を2つの三角形に分割
		// 上側の三角形
		idxVec.push_back(sideTopStart + i);
		idxVec.push_back(sideTopStart + next);
		idxVec.push_back(sideBottomStart + i);
		// 下側の三角形
		idxVec.push_back(sideTopStart + next);
		idxVec.push_back(sideBottomStart + next);
		idxVec.push_back(sideBottomStart + i);
	}

	//--- バッファの作成
	MeshBuffer::Description desc = {};
	desc.pVtx = vtxVec.data();
	desc.vtxCount = vtxVec.size();
	desc.vtxSize = sizeof(Vertex);
	desc.pIdx = idxVec.data();
	desc.idxCount = idxVec.size();
	desc.idxSize = sizeof(int);
	desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	m_pCylinder = new MeshBuffer();
	m_pCylinder->Create(desc);
}

void Geometory::MakeSphere()
{
	//--- 頂点の作成

	//--- インデックスの作成

	// バッファの作成
}