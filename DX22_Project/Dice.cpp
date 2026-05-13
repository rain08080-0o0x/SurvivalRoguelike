#include "Dice.h"
#include "Geometory.h"
#include "Transfer.h"
#include "Input.h"

#include "ShaderList.h"
#include "Sprite.h"
#include "Shader.h"
#include "Defines.h"
#include "UIObject.h"
#include<math.h>
#include <cfloat>   // FLT_MAX
#include <cmath>    // fabsf
using namespace DirectX;

// Vec3型に対して単項マイナス演算子を定義する
// Vec3.h などVec3構造体の定義ファイルに以下を追加してください

inline Vec3 operator-(const Vec3& v)
{
	return Vec3(-v.x, -v.y, -v.z);
}


// 摩擦
const float friction = 0.97f;
// 落下加速度
const float fall = 0.02f;
// 止まる加速度
const float under = 0.01f;
// 壁
const float wall = 2.5f;

static float ProjectRadiusOnAxis(const RigidBodyOBB & b, const Vec3 & nUnit)
{
	// r = Σ extents[i] * |n · axis[i]|
	return
		b.extents.x * fabsf(nUnit.Dot(b.axis[0])) +
		b.extents.y * fabsf(nUnit.Dot(b.axis[1])) +
		b.extents.z * fabsf(nUnit.Dot(b.axis[2]));
}

static bool TryAxisSAT(
	const RigidBodyOBB& A,
	const RigidBodyOBB& B,
	const Vec3& axisRaw,
	const Vec3& centerDelta,
	float& bestOverlap,
	Vec3& bestNormal)
{
	const float eps = 1e-8f;
	if (axisRaw.LengthSq() < eps)
		return true; // cross軸が潰れるケースは無視

	Vec3 axis = axisRaw.Normalize();

	const float dist = fabsf(centerDelta.Dot(axis));
	const float rA = ProjectRadiusOnAxis(A, axis);
	const float rB = ProjectRadiusOnAxis(B, axis);

	const float overlap = (rA + rB) - dist;
	if (overlap < 0.0f)
		return false; // 分離

	if (overlap < bestOverlap)
	{
		bestOverlap = overlap;

		// 法線を A -> B 方向に揃える
		const float s = (centerDelta.Dot(axis) < 0.0f) ? -1.0f : 1.0f;
		bestNormal = axis * s;
	}
	return true;
}

 //SATで衝突してたら Manifold を作る（contactPoint は簡易）
static bool BuildManifoldSAT(RigidBodyOBB& A, RigidBodyOBB& B, CollisionResolver::Manifold& outM)
{
	Vec3 d = B.center - A.center;

	float bestOverlap = FLT_MAX;
	Vec3  bestNormal(0.0f, 1.0f, 0.0f);

	// 15軸：Aの3軸 + Bの3軸 + cross 9軸
	Vec3 axes[15];
	int k = 0;
	axes[k++] = A.axis[0];
	axes[k++] = A.axis[1];
	axes[k++] = A.axis[2];
	axes[k++] = B.axis[0];
	axes[k++] = B.axis[1];
	axes[k++] = B.axis[2];

	for (int i = 0; i < 3; ++i)
		for (int j = 0; j < 3; ++j)
			axes[k++] = A.axis[i].Cross(B.axis[j]);

	for (int i = 0; i < 15; ++i)
	{
		if (!TryAxisSAT(A, B, axes[i], d, bestOverlap, bestNormal))
			return false;
	}

	// contactPoint（簡易）：支持点の中点
	const float rA = ProjectRadiusOnAxis(A, bestNormal);
	const float rB = ProjectRadiusOnAxis(B, bestNormal);

	const Vec3 pA = A.center + bestNormal * rA;
	const Vec3 pB = B.center - bestNormal * rB;

	outM.bodyA = &A;
	outM.bodyB = &B;
	outM.normal = bestNormal;
	outM.depth = bestOverlap;
	outM.contactPoint = (pA + pB) * 0.5f;
	return true;
}

// ここが汎用：配列の中の全Rigidbodyを総当たりで判定して解決する
static void ResolveAllPairs_SAT(RigidBodyOBB* bodies[], int maxCount, int solverIters, bool grounded[])
{
	// 初期化
	for (int i = 0; i < maxCount; ++i)
		grounded[i] = false;

	for (int iter = 0; iter < solverIters; ++iter)
	{
		for (int i = 0; i < maxCount; ++i)
		{
			if (bodies[i] == nullptr) continue;

			for (int j = i + 1; j < maxCount; ++j)
			{
				if (bodies[j] == nullptr) continue;

				RigidBodyOBB& A = *bodies[i];
				RigidBodyOBB& B = *bodies[j];

				if (A.invMass == 0.0f && B.invMass == 0.0f)
					continue;

				CollisionResolver::Manifold m;
				if (BuildManifoldSAT(A, B, m))
				{
					// 接地っぽい接触（ほぼ上下方向の法線）を拾う
					// normal.y の符号は組み合わせで変わるので絶対値で見る
					if (fabsf(m.normal.y) > 0.7f)
					{
						grounded[i] = true;
						grounded[j] = true;
					}

					CollisionResolver::ResolveCollision(m);
				}
			}
		}
	}
}

//static float LenSq3(const Vec3& v)
//{
//	return v.x * v.x + v.y * v.y + v.z * v.z;
//}

//static Vec3 NormalizeSafe(const Vec3& v, const Vec3& fallback)
//{
//	float l2 = LenSq3(v);
//	if (l2 < 1e-12f) return fallback;
//	float inv = 1.0f / sqrtf(l2);
//	return v * inv;
//}

//static void SnapOrientationToFace(RigidBodyOBB& b)
//{
//	const Vec3 worldUp(0.0f, 1.0f, 0.0f);
//
//	// 6面の法線候補（符号反転は * -1.0f で）
//	Vec3 normals[6] =
//	{
//		b.axis[0],
//		b.axis[0] * -1.0f,
//		b.axis[1],
//		b.axis[1] * -1.0f,
//		b.axis[2],
//		b.axis[2] * -1.0f,
//	};
//
//	// 一番上を向いてる面を選ぶ
//	int best = 0;
//	float bestDot = -FLT_MAX;
//	for (int i = 0; i < 6; ++i)
//	{
//		float d = normals[i].Dot(worldUp);
//		if (d > bestDot)
//		{
//			bestDot = d;
//			best = i;
//		}
//	}
//
//	// 上方向
//	Vec3 up = NormalizeSafe(normals[best], worldUp);
//
//	// forward = axis[2] を接地平面に投影
//	Vec3 forward = b.axis[2] + (up * (-up.Dot(b.axis[2])));
//	forward = NormalizeSafe(forward, Vec3(0.0f, 0.0f, 1.0f));
//
//	// right = forward x up
//	Vec3 right = forward.Cross(up);
//	right = NormalizeSafe(right, Vec3(1.0f, 0.0f, 0.0f));
//
//	// forward を再計算（直交保証）
//	forward = up.Cross(right);
//	forward = NormalizeSafe(forward, Vec3(0.0f, 0.0f, 1.0f));
//
//	b.axis[0] = right;
//	b.axis[1] = up;
//	b.axis[2] = forward;
//}

// body[support] の上面を「軸平行の床」として扱う簡易判定
// あなたの元コードは床が軸平行前提なので、その前提を崩さないための判定
static bool IsAxisAlignedFloorLike(const RigidBodyOBB& b)
{
	// 軸がだいたいワールド軸に揃っていればOK
	// きつすぎると床判定されないので閾値はゆるめ
	const float th = 0.95f;

	Vec3 ax = b.axis[0];
	Vec3 ay = b.axis[1];
	Vec3 az = b.axis[2];

	// 絶対値でワールド軸に近いか
	auto absf_ = [](float v) { return (v < 0.0f) ? -v : v; };

	bool xOK = (absf_(ax.x) > th && absf_(ax.y) < (1.0f - th) && absf_(ax.z) < (1.0f - th));
	bool yOK = (absf_(ay.y) > th && absf_(ay.x) < (1.0f - th) && absf_(ay.z) < (1.0f - th));
	bool zOK = (absf_(az.z) > th && absf_(az.x) < (1.0f - th) && absf_(az.y) < (1.0f - th));

	return xOK && yOK && zOK;
}

// あなたの基準方式：moving の頂点を使って support 上面（軸平行床）と複数点で解く
static void ResolveBoxOnTopPlane_MultiContact(RigidBodyOBB* support, RigidBodyOBB* moving)
{
	if (!support || !moving) return;

	Vec3 verts[8];
	moving->GetWorldVertices(verts);

	const float planeY = support->center.y + support->extents.y;

	// moving の最下点Y
	float minY = verts[0].y;
	for (int i = 1; i < 8; ++i)
		if (verts[i].y < minY) minY = verts[i].y;

	// 許容
	const float contactEps = 0.001f;
	const float minLayer = 0.01f;

	// support を軸平行床として扱うので AABB 範囲は center/extents でOK
	const float minX = support->center.x - support->extents.x;
	const float maxX = support->center.x + support->extents.x;
	const float minZ = support->center.z - support->extents.z;
	const float maxZ = support->center.z + support->extents.z;

	// 接触点を複数回流す（元コードの反復2回を維持）
	for (int iter = 0; iter < 2; ++iter)
	{
		for (int i = 0; i < 8; ++i)
		{
			if (verts[i].x < minX || verts[i].x > maxX) continue;
			if (verts[i].z < minZ || verts[i].z > maxZ) continue;

			if (verts[i].y > (minY + minLayer)) continue;

			const float depth = planeY - verts[i].y;
			if (depth <= -contactEps) continue;

			CollisionResolver::Manifold m;
			m.bodyA = support;              // 支持側
			m.bodyB = moving;               // 乗る側
			m.normal = Vec3(0.0f, 1.0f, 0.0f);
			m.depth = depth;

			m.contactPoint = verts[i];
			m.contactPoint.y = planeY;

			CollisionResolver::ResolveCollision(m);

			// 解決で姿勢や中心が動くので、頂点も更新しておく（同フレームの安定化）
			moving->GetWorldVertices(verts);
		}
	}
}

// 汎用：配列の全ペアを解く
//  - 静的かつ床っぽいものは「上面床」として複数点解決
//  - それ以外は SAT 単一マニフォールド
static void ResolveAllPairs_Generic(RigidBodyOBB* bodies[], int count, int solverIters)
{
	for (int iter = 0; iter < solverIters; ++iter)
	{
		for (int i = 0; i < count; ++i)
		{
			if (!bodies[i]) continue;

			for (int j = i + 1; j < count; ++j)
			{
				if (!bodies[j]) continue;

				RigidBodyOBB* A = bodies[i];
				RigidBodyOBB* B = bodies[j];

				// 両方静的ならスキップ
				if (A->invMass == 0.0f && B->invMass == 0.0f)
					continue;

				// 片方が静的で床っぽいなら、床上面の複数接触点で解く
				// どっちが床かを両方試す
				if (A->invMass == 0.0f && IsAxisAlignedFloorLike(*A))
				{
					ResolveBoxOnTopPlane_MultiContact(A, B);
					continue;
				}
				if (B->invMass == 0.0f && IsAxisAlignedFloorLike(*B))
				{
					ResolveBoxOnTopPlane_MultiContact(B, A);
					continue;
				}

				// それ以外は SAT
				CollisionResolver::Manifold m;
				if (BuildManifoldSAT(*A, *B, m))
				{
					CollisionResolver::ResolveCollision(m);
				}
			}
		}
	}
}

static DirectX::XMFLOAT4X4 MakeWorldFromOBB(const RigidBodyOBB& b, float modelUnitSize = 1.0f)
{
	using namespace DirectX;

	// extents は半サイズなので *2 で一辺の長さになる
	// モデルが「1辺 modelUnitSize の立方体」ならこれで一致する
	const float sx = (b.extents.x * 2.0f) / modelUnitSize;
	const float sy = (b.extents.y * 2.0f) / modelUnitSize;
	const float sz = (b.extents.z * 2.0f) / modelUnitSize;

	XMMATRIX S = XMMatrixScaling(sx, sy, sz);

	// axis[0]=Right, axis[1]=Up, axis[2]=Forward を回転行列にする
	// この並べ方で OK（最後に Transpose してシェーダへ渡す設計に合わせる）
	XMMATRIX R =
	{
		b.axis[0].x, b.axis[0].y, b.axis[0].z, 0.0f,
		b.axis[1].x, b.axis[1].y, b.axis[1].z, 0.0f,
		b.axis[2].x, b.axis[2].y, b.axis[2].z, 0.0f,
		0.0f,        0.0f,        0.0f,        1.0f
	};

	XMMATRIX T = XMMatrixTranslation(b.center.x, b.center.y, b.center.z);

	XMMATRIX W = S * R * T;

	XMFLOAT4X4 out;
	XMStoreFloat4x4(&out, XMMatrixTranspose(W)); // あなたの環境は Transpose 渡し前提
	return out;
}

static void DiceLock(RigidBodyOBB b)
{
	b.velocity = { 0,0,0 };
	b.angularVel = { 0,0,0 };
}
static void DiceRoll(RigidBodyOBB& b,float power)
{
	float rpx = (float)(rand() % 11) / 10.0f - (power / 10.0f);
	float rpy = (float)(rand() % 11) / 10.0f - (power / 10.0f) + 5.0f;
	float rpz = (float)(rand() % 11) / 10.0f - (power / 10.0f);
	float rvx = (float)(rand() % 11) / 10.0f * power;
	float rvy = (float)(rand() % 11) / 10.0f * power;
	float rvz = (float)(rand() % 11) / 10.0f * power;

	b.center = {rpx,rpy,rpz};
	b.angularVel = { rvx,rvy,rvz };
}
static float LenSq3(const Vec3& v)
{
	return v.x * v.x + v.y * v.y + v.z * v.z;
}

// 戻り値：確定したら true（outFace に出目が入る）
//         まだ動いてるなら false
static bool TryFinalizeDice(RigidBodyOBB& b, int& outFace)
{
	// 完全固定中なら既に確定済み扱い
	if (b.fullyLocked)
	{
		outFace = GetTopFace(b);
		return true;
	}

	// しきい値（必要なら調整）
	// v2 は速度^2、w2 は角速度^2
	const float v2 = LenSq3(b.velocity);
	const float w2 = LenSq3(b.angularVel);

	const float v2Sleep = 1e-6f;
	const float w2Sleep = 2e-5f;

	if (v2 > v2Sleep || w2 > w2Sleep)
		return false;

	// 止まった：出目確定
	outFace = GetTopFace(b);

	// 完全固定（物理停止）
	b.velocity = Vec3(0.0f, 0.0f, 0.0f);
	b.angularVel = Vec3(0.0f, 0.0f, 0.0f);

	b.SetGravity(false);
	b.SetFullyLocked(true);

	return true;
}

static float RandRange(float minV, float maxV)
{
	return minV + (maxV - minV) * (float(rand()) / float(RAND_MAX));
}

//static float ProjectRadiusOnAxis(const RigidBodyOBB& b, const Vec3& nUnit)
//{
//	return
//		b.extents.x * fabsf(nUnit.Dot(b.axis[0])) +
//		b.extents.y * fabsf(nUnit.Dot(b.axis[1])) +
//		b.extents.z * fabsf(nUnit.Dot(b.axis[2]));
//}

//static bool TryAxisSAT(
//	const RigidBodyOBB& A,
//	const RigidBodyOBB& B,
//	const Vec3& axisRaw,
//	const Vec3& centerDelta,
//	float& bestOverlap,
//	Vec3& bestNormal)
//{
//	const float eps = 1e-8f;
//	if (axisRaw.LengthSq() < eps)
//		return true; // cross軸が潰れる場合は無視
//
//	Vec3 axis = axisRaw.Normalize();
//
//	const float dist = fabsf(centerDelta.Dot(axis));
//	const float rA = ProjectRadiusOnAxis(A, axis);
//	const float rB = ProjectRadiusOnAxis(B, axis);
//
//	const float overlap = (rA + rB) - dist;
//	if (overlap < 0.0f)
//		return false; // 分離してる
//
//	if (overlap < bestOverlap)
//	{
//		bestOverlap = overlap;
//
//		// 法線の向きを A->B に揃える
//		const float s = (centerDelta.Dot(axis) < 0.0f) ? -1.0f : 1.0f;
//		bestNormal = axis * s;
//	}
//	return true;
//}

// SATで衝突してたら Manifold を作る（contactPointは簡易：支持点の中点）
//static bool BuildManifoldSAT(RigidBodyOBB& A, RigidBodyOBB& B, CollisionResolver::Manifold& outM)
//{
//	Vec3 d = B.center - A.center;
//
//	float bestOverlap = FLT_MAX;
//	Vec3 bestNormal(0.0f, 1.0f, 0.0f);
//
//	// 15軸：Aの3軸 + Bの3軸 + cross 9軸
//	Vec3 axes[15];
//	int k = 0;
//	axes[k++] = A.axis[0];
//	axes[k++] = A.axis[1];
//	axes[k++] = A.axis[2];
//	axes[k++] = B.axis[0];
//	axes[k++] = B.axis[1];
//	axes[k++] = B.axis[2];
//
//	for (int i = 0; i < 3; ++i)
//		for (int j = 0; j < 3; ++j)
//			axes[k++] = A.axis[i].Cross(B.axis[j]);
//
//	for (int i = 0; i < 15; ++i)
//	{
//		if (!TryAxisSAT(A, B, axes[i], d, bestOverlap, bestNormal))
//			return false;
//	}
//
//	// contactPoint（簡易）
//	const float rA = ProjectRadiusOnAxis(A, bestNormal);
//	const float rB = ProjectRadiusOnAxis(B, bestNormal);
//
//	const Vec3 pA = A.center + bestNormal * rA;
//	const Vec3 pB = B.center - bestNormal * rB;
//
//	outM.bodyA = &A;
//	outM.bodyB = &B;
//	outM.normal = bestNormal;
//	outM.depth = bestOverlap;
//	outM.contactPoint = (pA + pB) * 0.5f;
//	return true;
//}

Dice::Dice()
	: m_pCamera(nullptr)
{
	float size = sqrtf(2);
	m_pos = { 0.0f,0.0f,0.0f };
	m_size = { size,size,size };
	m_mass = 1.0f;
	isActive = true;
	Vec3 pos = { m_pos.x,m_pos.y,m_pos.z };
	Vec3 sizeV = {m_size.x,m_size.y,m_size.z};

	for (int i = 0; i < MAX_DICE; i++)
	{
		body[i] = nullptr;
	}

	body[0] = new RigidBodyOBB({ 2.0f,1.0f,0.0f }, {1.0f,1.0f,1.0f}, 10.0f);
	body[1] = new RigidBodyOBB({0.0f,0.0f,0.0f}, {25.0f,1.0f,25.0f}, 0.0f);
	body[2] = new RigidBodyOBB({-2.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f}, 10.0f);
	body[3] = new RigidBodyOBB({0.0f,1.0f,0.0f}, {1.0f,1.0f,1.0f}, 10.0f);
	DiceRoll(*body[0],10);
	DiceRoll(*body[2],10);
	DiceRoll(*body[3],10);
	// 四方の壁を生成
	body[4] = new RigidBodyOBB({ 0,5, wall }, { 2 * wall,10.0f,0.5f }, 100.0f);
	body[5] = new RigidBodyOBB({ 0,5,-wall }, { 2 * wall,10.0f,0.5f }, 100.0f);
	body[6] = new RigidBodyOBB({  wall,5,0 }, { 0.5f,10.0f,2 * wall }, 100.0f);
	body[7] = new RigidBodyOBB({ -wall,5,0 }, { 0.5f,10.0f,2 * wall }, 100.0f);

	body[4]->SetFullyLocked(true);
	body[5]->SetFullyLocked(true);
	body[6]->SetFullyLocked(true);
	body[7]->SetFullyLocked(true);
	// 頂点決め
	float sizehalf = HALF(0.5f);
	vertex[0] = {m_pos.x - size ,m_pos.y - size,m_pos.z - size};// 左下後ろ
	vertex[1] = {m_pos.x - size ,m_pos.y - size,m_pos.z + size};// 左下手前
	vertex[2] = {m_pos.x - size ,m_pos.y + size,m_pos.z - size};// 左上後ろ
	vertex[3] = {m_pos.x - size ,m_pos.y + size,m_pos.z + size};// 左上手前
	vertex[4] = {m_pos.x + size ,m_pos.y - size,m_pos.z - size};// 右下後ろ
	vertex[5] = {m_pos.x + size ,m_pos.y - size,m_pos.z + size};// 右下手前
	vertex[6] = {m_pos.x + size ,m_pos.y + size,m_pos.z - size};// 右上後ろ
	vertex[7] = {m_pos.x + size ,m_pos.y + size,m_pos.z + size};// 右上手前

	m_pModel = new Model();

	if (!m_pModel->Load("Assets/Model/Dice/dice.fbx", 1.f, Model::ZFlip)) { // 倍率と反転は省略可
		MessageBox(NULL, "Not found for dice", "Error", MB_OK); // エラーメッセージの表示
	}

}

Dice::~Dice()
{
	if (m_pModel)
	{
		//m_pModel->Reset();
		delete m_pModel;
		m_pModel = nullptr;
	}
	for (int i = 0; i < MAX_DICE; ++i)
	{
		delete body[i];
		body[i] = nullptr;
	}
	for (int i = 0; i < 3; ++i)
	{
		if (m_pDiceUI[i])
		{
			delete m_pDiceUI[i];
			m_pDiceUI[i] = nullptr;
		}
	}

}

void Dice::Update(int)
{
	const float dt = 1.0f / fFPS;

	// 1) 先に全ボディを積分（ここで center/axis が最新になる）
	for (int i = 0; i < MAX_DICE; ++i)
	{
		if (body[i] == nullptr) continue;
		body[i]->Update(dt);
	}

	// 2) 汎用：配列の全ペア衝突解決
	const int solverIters = 4;
	ResolveAllPairs_Generic(body, MAX_DICE, solverIters);

	// 3) 衝突で変わった angularVel を同フレームで姿勢に反映
	for (int i = 0; i < MAX_DICE; ++i)
	{
		if (body[i] == nullptr) continue;
		body[i]->IntegrateRotation(dt);
	}

	// 4) Transfer（必要なら。今まで通り 0 と 1 を入れる）
	TRAN_INS;

	if (body[0])
	{
		tran.obj.A = { body[0]->center.x, body[0]->center.y, body[0]->center.z };
		tran.obj.Avel = { body[0]->velocity.x, body[0]->velocity.y, body[0]->velocity.z };
		tran.obj.AangVel = { body[0]->angularVel.x, body[0]->angularVel.y, body[0]->angularVel.z };
	}
	if (body[1])
	{
		tran.obj.B = { body[1]->center.x, body[1]->center.y, body[1]->center.z };
		tran.obj.Bvel = { body[1]->velocity.x, body[1]->velocity.y, body[1]->velocity.z };
		tran.obj.BangVel = { body[1]->angularVel.x, body[1]->angularVel.y, body[1]->angularVel.z };
	}

	// テスト操作（そのまま維持）
	static int count;
	if (IsKeyTrigger('Y'))
	{
		//DiceRoll(*body[0], 15.0f);
		//DiceRoll(*body[2], 15.0f);
		//DiceRoll(*body[3], 15.0f);
		//count = 0;
		//tran.dice.currentFaceNumber[0] = 0;
		//tran.dice.currentFaceNumber[2] = 0;
		//tran.dice.currentFaceNumber[3] = 0;
		//isActive = true;
	}
	if (IsKeyTrigger('R'))
	{
		count = 0;
	}
	// 五秒経過 or Tキーで強制停止
	if (count >= fFPS * 5 || IsKeyPress('T'))
	{
		body[0]->velocity = { 0,0,0 };
		body[0]->angularVel = { 0,0,0 };
		body[2]->velocity = { 0,0,0 };
		body[2]->angularVel = { 0,0,0 };
		body[3]->velocity = { 0,0,0 };
		body[3]->angularVel = { 0,0,0 };

		// NULLチェックを追加して安全にアクセスする
		if (body[0] != nullptr) {
			tran.dice.currentFaceNumber[0] = GetTopFace(*body[0]);
		}
		tran.dice.currentFaceNumber[0] = GetTopFace(*body[0]);
		tran.dice.currentFaceNumber[2] = GetTopFace(*body[2]);
		tran.dice.currentFaceNumber[3] = GetTopFace(*body[3]);
		isActive = false;

		int dice0 = tran.dice.currentFaceNumber[0];
		int dice2 = tran.dice.currentFaceNumber[2];
		int dice3 = tran.dice.currentFaceNumber[3];

		SetDiceTexture(0, dice0);
		SetDiceTexture(1, dice2);
		SetDiceTexture(2, dice3);
	}
	count++;
	body[4]->axis[0] = { 1,0,0 };
	body[4]->axis[1] = { 0,1,0 };
	body[4]->axis[2] = { 0,0,1 };
	body[4]->center = { 0,5,wall };
	body[4]->velocity = { 0,0,0 };
	body[4]->angularVel = { 0,0,0 };


	body[5]->axis[0] = { 1,0,0 };
	body[5]->axis[1] = { 0,1,0 };
	body[5]->axis[2] = { 0,0,1 };
	body[5]->center = { 0,5,-wall };
	body[5]->velocity = { 0,0,0 };
	body[5]->angularVel = { 0,0,0 };


	body[6]->axis[0] = { 1,0,0 };
	body[6]->axis[1] = { 0,1,0 };
	body[6]->axis[2] = { 0,0,1 };
	body[6]->center = { wall,5,0 };
	body[6]->velocity = { 0,0,0 };
	body[6]->angularVel = { 0,0,0 };


	body[7]->axis[0] = { 1,0,0 };
	body[7]->axis[1] = { 0,1,0 };
	body[7]->axis[2] = { 0,0,1 };
	body[7]->center = { -wall,5,0 };
	body[7]->velocity = { 0,0,0 };
	body[7]->angularVel = { 0,0,0 };

	// 出目保存（DiceクラスのメンバにしてもOK）
	static int g_face[MAX_DICE] = { 0 };

	for (int i = 0; i < MAX_DICE; ++i)
	{
		if (!body[i]) continue;
		if (i == 1)continue;

		// 壁や床は確定対象外ならスキップ
		if (body[i]->invMass == 0.0f) continue;

		int face = 0;
		if (TryFinalizeDice(*body[i], face))
		{
			g_face[i] = face;
		}
	}

}



// 更新処理 
void Dice::Update(float dt)
{
	{
		// 2. 衝突判定 (SAT等で別途実装が必要。ここでは結果が得られたと仮定)
		// 例えば、boxAの頂点を計算し、boxBに含まれるかチェックするなど
		Vec3 vertsA[8];
		body[0]->GetWorldVertices(vertsA);

		// boxA の頂点から、boxB の上面（簡易）への接触点を作って解決する
		{
			const float planeY = body[1]->center.y + body[1]->extents.y;

			// Aの最下点を取る（面接地なら4頂点がここに集まる）
			float minY = vertsA[0].y;
			for (int i = 1; i < 8; ++i)
			{
				if (vertsA[i].y < minY) minY = vertsA[i].y;
			}

			// 多少の誤差許容
			const float contactEps = 0.001f;
			// 「最下層付近」とみなす高さ幅（ここを広げすぎると傾きやすくなる）
			const float minLayer = 0.01f;

			// B上面の矩形範囲（Bを軸平行の床として扱う簡易）
			const float minX = body[1]->center.x - body[1]->extents.x;
			const float maxX = body[1]->center.x + body[1]->extents.x;
			const float minZ = body[1]->center.z - body[1]->extents.z;
			const float maxZ = body[1]->center.z + body[1]->extents.z;

			// 反復（2回だけ）
			for (int iter = 0; iter < 2; ++iter)
			{
				for (int i = 0; i < 8; ++i)
				{
					// B上面の真上にある頂点だけ採用（外なら無視）
					if (vertsA[i].x < minX || vertsA[i].x > maxX) continue;
					if (vertsA[i].z < minZ || vertsA[i].z > maxZ) continue;

					// 最下層付近の頂点だけ接触点にする（面なら複数点になる）
					if (vertsA[i].y > (minY + minLayer)) continue;

					// めり込み（または接触）しているか
					const float depth = planeY - vertsA[i].y;
					if (depth <= -contactEps) continue;

					CollisionResolver::Manifold m;
					m.bodyA = body[1];                 // 支持側（床）
					m.bodyB = body[0];                 // 乗る側
					m.normal = Vec3(0, 1, 0);    // 上向き法線（簡易）
					m.depth = depth;

					m.contactPoint = vertsA[i];
					m.contactPoint.y = planeY;   // 接触点を面上へ

					CollisionResolver::ResolveCollision(m);
				}
			}
		}
	}

	// 3. 物理更新
	for(int i = 0;i < MAX_DICE;i++)
	{
		if (body[i] == nullptr)continue;

		body[i]->Update(1.0f / 60.0f);
	}
	TRAN_INS;
	DirectX::XMFLOAT3
		pos =
	{
		body[0]->center.x,
		body[0]->center.y,
		body[0]->center.z
	};

	tran.obj.A = pos;
	pos =
	{
		body[1]->center.x,
		body[1]->center.y,
		body[1]->center.z
	};
	tran.obj.B = pos;
	tran.obj.Avel = { body[0]->velocity.x,body[0]->velocity.y,body[0]->velocity.z };
	tran.obj.Bvel = { body[1]->velocity.x,body[1]->velocity.y,body[1]->velocity.z };
	tran.obj.AangVel = { body[0]->angularVel.x,body[0]->angularVel.y,body[0]->angularVel.z };
	tran.obj.BangVel = { body[1]->angularVel.x,body[1]->angularVel.y,body[1]->angularVel.z };
	// デバッグ用
	if (IsKeyTrigger('Y'))
	{
		body[0]->center = { 0.0,5.0f,0.0f };
		body[2]->center = { 0.0,7.0f,0.0f };
		body[3]->center = { 0.0,10.0f,0.0f };
	}
	else
	if (IsKeyPress('R'))
	{
		body[0]->angularVel.z += 3.14f;
	}

}


// 描画処理 
void Dice::Draw()
{
	using namespace DirectX;


	TRAN_INS;

	DirectX::XMFLOAT4 color = { 1.0f,1.0f,1.0f,1.0f };
	for(int i = 0;i < MAX_DICE;i++)
	{
		// 使ってないDiceは0を代入
		//tran.dice.currentFaceNumber[i] = 0;
		if (i == 4 || i == 5 || i == 6 || i == 7)continue;

		if (body[i] != nullptr)
		{
			color = { 1.0f,1.0f,1.0f,1.0f };
			color.x = 1.0f - (((i + 1) & 0b1) != 0);
			color.y = 1.0f - (((i + 1) & 0b10) != 0);
			color.z = 1.0f - (((i + 1) & 0b100) != 0);

			if (color.x == 0.0f && color.y == 0.0f && color.z == 0.0f)
			{
				color = { 1.0f,1.0f,1.0f,1.0f };
			}
			color = { 0.0f,1.0f,0.0f,1.0f };
			DirectX::XMFLOAT3 vtxA[8];
			body[i]->GetWorldVertices(vtxA);
			Geometory::AddLine(vtxA[0], vtxA[1], color);
			Geometory::AddLine(vtxA[1], vtxA[3], color);
			Geometory::AddLine(vtxA[3], vtxA[2], color);
			Geometory::AddLine(vtxA[2], vtxA[0], color);
			Geometory::AddLine(vtxA[0 + 4], vtxA[1 + 4], color);
			Geometory::AddLine(vtxA[1 + 4], vtxA[3 + 4], color);
			Geometory::AddLine(vtxA[3 + 4], vtxA[2 + 4], color);
			Geometory::AddLine(vtxA[2 + 4], vtxA[0 + 4], color);
			Geometory::AddLine(vtxA[0], vtxA[4], color);
			Geometory::AddLine(vtxA[1], vtxA[5], color);
			Geometory::AddLine(vtxA[2], vtxA[6], color);
			Geometory::AddLine(vtxA[3], vtxA[7], color);

			// 床を描画しない
			if (i == 1)continue;
			// 壁も描画しない
			if (i == 4)continue;
			if (i == 5)continue;
			if (i == 6)continue;
			if (i == 7)continue;

			DirectX::XMFLOAT4X4 fWVP[3];

			// World
			fWVP[0] = MakeWorldFromOBB(*body[i], 1.0f);


			// モデルに変換行列を設定 
			fWVP[1] = m_pCamera->GetViewMatrix();
			fWVP[2] = m_pCamera->GetProjectionMatrix();



			// シェーダーへ変換行列を設定 
			ShaderList::SetWVP(fWVP); // SetWVP関数の引数にはXMFLOAT4X4型で要素数３の配列のアドレスを渡す 


			// モデルに使用する頂点シェーダー、ピクセルシェーダーを設定 
			m_pModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
			m_pModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));

			// 仮置きしているボックスにカメラを設定
			Geometory::SetView(fWVP[1]);
			Geometory::SetProjection(fWVP[2]);

			// 仮置きしているボックスにカメラを設定 
			Geometory::SetView(m_pCamera->GetViewMatrix());
			Geometory::SetProjection(m_pCamera->GetProjectionMatrix());

			// Spriteへカメラの行列を設定 
			Sprite::SetView(m_pCamera->GetViewMatrix());
			Sprite::SetProjection(m_pCamera->GetProjectionMatrix());

			// マテリアル別にメッシュを表示 
			for (unsigned int i = 0; i < m_pModel->GetMeshNum(); ++i) {
				// モデルのメッシュを取得 
				const Model::Mesh* mesh = m_pModel->GetMesh(i);
				// メッシュに割り当てられているマテリアルを取得 
				Model::Material material = *m_pModel->GetMaterial(mesh->materialID);
				material.ambient = {0.7f,0.7f,0.7f,1.0f};
				// シェーダーへマテリアルを設定 
				ShaderList::SetMaterial(material);
				// モデルの描画 
				m_pModel->Draw(i);
			}

			//tran.dice.currentFaceNumber[i] = GetTopFace(*body[i]);
		}
	}
	if(!isActive)
	for (int i = 0; i < 3; ++i)
	{
		if (m_pDiceUI[i])
		{
			m_pDiceUI[i]->Draw();
		}
	}

}


// カメラの設定 
void Dice::SetCamera(Camera* pCamera)
{
	m_pCamera = pCamera;
}

bool Dice::IsStop()
{
	return !isActive;
}

void Dice::RollRandom(int index)
{
	if (index < 0 || index >= MAX_DICE) return;
	if (body[index] == nullptr) return;

	RigidBodyOBB& b = *body[index];


	b.SetFullyLocked(false);
	b.SetGravity(true);
	isActive = true;

	// 1) 一旦安定化
	b.velocity = Vec3(0.0f, 0.0f, 0.0f);
	b.angularVel = Vec3(0.0f, 0.0f, 0.0f);

	// 2) 少し持ち上げる（床との即衝突防止）
	b.center.y = 2.5f;

	// 3) 重力ON
	b.SetGravity(true);

	// 4) 初速（上＋横）
	b.velocity.y = RandRange(4.0f, 6.0f);
	b.velocity.x = RandRange(-1.5f, 1.5f);
	b.velocity.z = RandRange(-1.5f, 1.5f);

	// 5) 角速度（どの軸にも回る）
	b.angularVel.x = RandRange(-8.0f, 8.0f);
	b.angularVel.y = RandRange(-8.0f, 8.0f);
	b.angularVel.z = RandRange(-8.0f, 8.0f);

	// 6) スリープ解除（あるなら）
	b.WakeUp();
}
void Dice::SetDiceTexture(int count, int num)
{
	std::string DiceTex;
	DirectX::XMFLOAT2 pos;
	DirectX::XMFLOAT2 size = { 50.0f, 50.0f };

	if (count < 0 || count >= 3) return;

	switch (count)
	{
	case 0: pos = { 640.0f - 50.0f, 25.0f }; break;
	case 1: pos = { 640.0f,         25.0f }; break;
	case 2: pos = { 640.0f + 50.0f, 25.0f }; break;
	default: return;
	}

	switch (num)
	{
	case 1: DiceTex = "Dice/one.png";   break;
	case 2: DiceTex = "Dice/two.png";   break;
	case 3: DiceTex = "Dice/three.png"; break;
	case 4: DiceTex = "Dice/four.png";  break;
	case 5: DiceTex = "Dice/five.png";  break;
	case 6: DiceTex = "Dice/six.png";   break;
	default: return;
	}

	// 初回だけ生成＆Add
	if (!m_pDiceUI[count])
	{
		m_pDiceUI[count] = new UIObject(DiceTex.c_str(), pos.x, pos.y, size.x, size.y);
		return;
	}

	// 2回目以降：テクスチャ差し替え（UIObjectにその機能が必要）
	m_pDiceUI[count]->SetTexture(DiceTex.c_str());
}

