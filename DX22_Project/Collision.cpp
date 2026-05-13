#include "Collision.h"

Collision::Result Collision::Hit(Box a, Box b)
{
    Result out = {};

    // 計算用の方に変換
    DirectX::XMVECTOR vPosA = DirectX::XMLoadFloat3(&a.center);
    DirectX::XMVECTOR vPosB = DirectX::XMLoadFloat3(&b.center);
    DirectX::XMVECTOR vSizeA = DirectX::XMLoadFloat3(&a.size);
    DirectX::XMVECTOR vSizeB = DirectX::XMLoadFloat3(&b.size);

    // ボックスの半分のサイズを取得
    vSizeA = DirectX::XMVectorScale(vSizeA, 0.5f);
    vSizeB = DirectX::XMVectorScale(vSizeB, 0.5f);

    // ボックスの各軸の最大値、最小値を取得
    DirectX::XMVECTOR vMaxA = DirectX::XMVectorAdd(vPosA, vSizeA);
    DirectX::XMVECTOR vMinA = DirectX::XMVectorSubtract(vPosA, vSizeA);
    DirectX::XMVECTOR vMaxB = DirectX::XMVectorAdd(vPosB, vSizeB);
    DirectX::XMVECTOR vMinB = DirectX::XMVectorSubtract(vPosB, vSizeB);
    DirectX::XMFLOAT3 maxA, minA, maxB, minB;
    //maxA vMaxA minA, maxB, minB
    DirectX::XMStoreFloat3(&maxA,vMaxA);
    DirectX::XMStoreFloat3(&minA,vMinA);
    DirectX::XMStoreFloat3(&maxB,vMaxB);
    DirectX::XMStoreFloat3(&minB,vMinB);
        //  
    out.isHit = false;

    //  
    if (maxA.x  >= minB.x && minA.x <= maxB.x) {
        if (maxA.y >= minB.y && minA.y <= maxB.y) {
            if ((maxA.z >= minB.z && minA.z <= maxB.z)) {
                //  
                out.isHit = true;

                DirectX::XMVECTOR vDist =
                    DirectX::XMVectorSubtract(vPosA, vPosB);
                vDist = DirectX::XMVectorAbs(vDist);

                DirectX::XMVECTOR vSumSize =
                    DirectX::XMVectorAdd(vSizeA, vSizeB);
                DirectX::XMVECTOR vOverlap =
                    DirectX::XMVectorSubtract(vSumSize, vDist);
                DirectX::XMFLOAT3 overlap;
                DirectX::XMStoreFloat3(&overlap, vOverlap);

                // 各軸のめり込み量のうち、最小のめり込み量の方向へ跳ね返す 
                if (overlap.x < overlap.y) {
                    if (overlap.x < overlap.z)
                        out.dir = { a.center.x < b.center.x ? -1.0f : 1.0f, 0.0f, 0.0f };
                    else
                        out.dir = { 0.0f, 0.0f, a.center.z < b.center.z ? -1.0f : 1.0f };
                }
                else {
                    if (overlap.y < overlap.z)
                        out.dir = { 0.0f,  a.center.y < b.center.y ? -1.0f : 1.0f, 0.0f };
                    else
                        out.dir = { 0.0f, 0.0f, a.center.z < b.center.z ? -1.0f : 1.0f };
                }
            }
        }
    }

    return out;
}

Collision::Result Collision::Hit(Sphere a, Sphere b)
{
    Result out = {};

    //  
    DirectX::XMVECTOR vPosA = DirectX::XMLoadFloat3(&a.center);
    DirectX::XMVECTOR vPosB = DirectX::XMLoadFloat3(&b.center);

    //  
    DirectX::XMVECTOR vDist = DirectX::XMVectorSubtract(vPosA, vPosB);

    //  
    DirectX::XMVECTOR vLen = vDist;
    float length;
    DirectX::XMStoreFloat(&length,vLen);

    //  
    out.isHit = (a.radius + b.radius) >= length;
    // if( a.radius + b.radius)>= lenths

    return out;
}

RigidBodyOBB::RigidBodyOBB(Vec3 pos, Vec3 size, float maasss)
    : center(pos)
    , extents(size * 0.5f)
    , mass(maasss)
    , velocity(0, 0, 0)
    , angularVel(0, 0, 0)
    , isGround(false)
{
    // 初期姿勢はワールド軸と同じ
    axis[0] = Vec3(1, 0, 0);
    axis[1] = Vec3(0, 1, 0);
    axis[2] = Vec3(0, 0, 1);

    if (mass > 0.0f) {
        invMass = 1.0f / mass;
        // 直方体の慣性モーメント計算 I = m/12 * (h^2 + d^2) ...
        float w2 = size.x * size.x;
        float h2 = size.y * size.y;
        float d2 = size.z * size.z;
        inertiaTensor.x = (1.0f / 12.0f) * mass * (h2 + d2);
        inertiaTensor.y = (1.0f / 12.0f) * mass * (w2 + d2);
        inertiaTensor.z = (1.0f / 12.0f) * mass * (w2 + h2);
    }
    else {
        invMass = 0.0f;
        inertiaTensor = Vec3(0, 0, 0);
        gravityEnabled = false;
    }
}

void RigidBodyOBB::GetWorldVertices(Vec3 outVertices[8]) const {
    Vec3 ax = axis[0] * extents.x;
    Vec3 ay = axis[1] * extents.y;
    Vec3 az = axis[2] * extents.z;

    // 8通りの組み合わせを展開
    outVertices[0] = center + ax + ay + az;
    outVertices[1] = center - ax + ay + az;
    outVertices[2] = center + ax - ay + az;
    outVertices[3] = center - ax - ay + az;
    outVertices[4] = center + ax + ay - az;
    outVertices[5] = center - ax + ay - az;
    outVertices[6] = center + ax - ay - az;
    outVertices[7] = center - ax - ay - az;
}

void RigidBodyOBB::GetWorldVertices(DirectX::XMFLOAT3 outVertices[8]) const
{
    Vec3 ax = axis[0] * extents.x;
    Vec3 ay = axis[1] * extents.y;
    Vec3 az = axis[2] * extents.z;

    Vec3 all[8];
    // 8通りの組み合わせを展開
    all[0] = center + ax + ay + az;
    all[1] = center - ax + ay + az;
    all[2] = center + ax - ay + az;
    all[3] = center - ax - ay + az;
    all[4] = center + ax + ay - az;
    all[5] = center - ax + ay - az;
    all[6] = center + ax - ay - az;
    all[7] = center - ax - ay - az;

    outVertices[0] = {all[0].x,all[0].y,all[0].z};
    outVertices[1] = {all[1].x,all[1].y,all[1].z};
    outVertices[2] = {all[2].x,all[2].y,all[2].z};
    outVertices[3] = {all[3].x,all[3].y,all[3].z};
    outVertices[4] = {all[4].x,all[4].y,all[4].z};
    outVertices[5] = {all[5].x,all[5].y,all[5].z};
    outVertices[6] = {all[6].x,all[6].y,all[6].z};
    outVertices[7] = {all[7].x,all[7].y,all[7].z};
}

void RigidBodyOBB::Update(float dt)
{
    // 完全固定なら何もしない
    if (fullyLocked) return;

    if (isStatic) return;
    if (invMass <= 0.0f) return;
    if (isSleeping) return;

    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f;

    // ★毎フレームリセット（必須）
    isGround = false;

    Vec3 accel(0, 0, 0);

    if (gravityEnabled && !isGround)
        accel += gravity;

    accel += forceAccum * invMass;

    velocity += accel * dt;
    center += velocity * dt;

    // --- 回転の積分（axis を angularVel で更新） ---
    {
        // 角速度が小さいならスキップ
        float wLen2 = angularVel.x * angularVel.x + angularVel.y * angularVel.y + angularVel.z * angularVel.z;
        if (wLen2 > 1e-12f)
        {
            // 回転軸（正規化）
            float wLen = sqrtf(wLen2);
            Vec3 rotAxis = angularVel * (1.0f / wLen);

            // 回転角（ラジアン）
            float angle = wLen * dt;

            // DirectXの回転行列を作って axis を回す
            DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(
                DirectX::XMVectorSet(rotAxis.x, rotAxis.y, rotAxis.z, 0.0f),
                angle
            );

            auto RotateVec = [&](const Vec3& v) -> Vec3
                {
                    DirectX::XMVECTOR vv = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
                    vv = DirectX::XMVector3TransformNormal(vv, R);
                    Vec3 out;
                    DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&out, vv);
                    return out;
                };

            axis[0] = RotateVec(axis[0]);
            axis[1] = RotateVec(axis[1]);
            axis[2] = RotateVec(axis[2]);

            // 数値誤差で崩れるので正規直交化（超重要）
            axis[0] = axis[0].Normalize();
            axis[1] = (axis[1] - axis[0] * axis[0].Dot(axis[1])).Normalize(); // Gram-Schmidt
            axis[2] = axis[0].Cross(axis[1]); // 右手系を維持
        }
    }

    forceAccum = Vec3(0, 0, 0);

    // 地面スナップ
    {
        const float groundY = 0.0f;
        const float groundEps = 0.002f; // ★ヒステリシス

        DirectX::XMFLOAT3 vtx[8];
        GetWorldVertices(vtx);

        float minY = vtx[0].y;
        for (int i = 1; i < 8; ++i) minY = (vtx[i].y < minY) ? vtx[i].y : minY;

        float pen = groundY - minY;

        if (pen > -groundEps)
        {
            if (pen > 0.0f)
                center.y += pen;

            isGround = true;

            if (velocity.y < 0.0f) velocity.y = 0.0f;

            // この条件は実質意味が薄いので削ってOK（下で説明）
            // const float stopVy = 0.05f;
            // if (fabsf(velocity.y) < stopVy) velocity.y = 0.0f;
        }
    }

    // 摩擦/減衰（衝突後に入れるのは正しい）
    if (isGround)
    {
        velocity.x *= 0.80f;
        velocity.z *= 0.80f;

        angularVel.x *= 0.85f;
        angularVel.y *= 0.85f;
        angularVel.z *= 0.85f;
    }
    else
    {
        velocity.x *= 0.99f;
        velocity.z *= 0.99f;

        angularVel.x *= 0.995f;
        angularVel.y *= 0.995f;
        angularVel.z *= 0.995f;
    }
}

void RigidBodyOBB::IntegrateRotation(float dt)
{
    if (isStatic) return;
    if (invMass <= 0.0f) return;
    if (isSleeping) return;

    if (dt <= 0.0f) return;
    if (dt > 0.1f) dt = 0.1f;

    float wLen2 = angularVel.x * angularVel.x + angularVel.y * angularVel.y + angularVel.z * angularVel.z;
    if (wLen2 <= 1e-12f)
        return;

    float wLen = sqrtf(wLen2);
    Vec3 rotAxis = angularVel * (1.0f / wLen);
    float angle = wLen * dt;

    DirectX::XMMATRIX R = DirectX::XMMatrixRotationAxis(
        DirectX::XMVectorSet(rotAxis.x, rotAxis.y, rotAxis.z, 0.0f),
        angle
    );

    auto RotateVec = [&](const Vec3& v) -> Vec3
        {
            DirectX::XMVECTOR vv = DirectX::XMVectorSet(v.x, v.y, v.z, 0.0f);
            vv = DirectX::XMVector3TransformNormal(vv, R);
            DirectX::XMFLOAT3 tmp;
            DirectX::XMStoreFloat3(&tmp, vv);
            return Vec3(tmp.x, tmp.y, tmp.z);
        };

    axis[0] = RotateVec(axis[0]);
    axis[1] = RotateVec(axis[1]);
    axis[2] = RotateVec(axis[2]);

    axis[0].Normalize();
    axis[1] = (axis[1] - axis[0] * axis[0].Dot(axis[1])).Normalize();
    axis[2] = axis[0].Cross(axis[1]);
}

void RigidBodyOBB::SetGravity(bool enable)
{
	gravityEnabled = enable;
}

void RigidBodyOBB::SetFullyLocked(bool lock)
{
    fullyLocked = lock;

    if (lock)
    {
        // 物理的に完全停止
        velocity = Vec3(0.0f, 0.0f, 0.0f);
        angularVel = Vec3(0.0f, 0.0f, 0.0f);

        gravityEnabled = false;
    }
}




void CollisionResolver::ResolveCollision(Manifold& m) 
{
    RigidBodyOBB* A = m.bodyA;
    RigidBodyOBB* B = m.bodyB;

    // 静止オブジェクト同士なら無視
    if (A->invMass == 0 && B->invMass == 0) return;

    // 1. 衝突点へのベクトル（重心 r から衝突点 p へのベクトル）
    Vec3 rA = m.contactPoint - A->center;
    Vec3 rB = m.contactPoint - B->center;

    // 2. 衝突点における相対速度の計算
    // v_point = v_center + angularVel x r
    Vec3 velA = A->velocity + A->angularVel.Cross(rA);
    Vec3 velB = B->velocity + B->angularVel.Cross(rB);
    Vec3 relativeVel = velB - velA;

    // 相対速度の法線方向成分
    float velAlongNormal = relativeVel.Dot(m.normal);

    // 離れようとしているなら処理しない

// 離れていく方向なら、速度応答（反発）は不要。
// ただし、めり込み(m.depth)が残っていることがあるので、位置補正は実行させる。
// ここで return してしまうと、静止接触が崩れて傾きの原因になる。
    if (velAlongNormal > 0.0f)
    {
        velAlongNormal = 0.0f; // インパルスが 0 になるようにする（位置補正は下で行う）
    }


    // 3. 反発係数 (e)
    float e = 0.5f; // 0:非弾性, 1:完全弾性

    // 衝突速度が小さい（例えば重力の影響程度）なら、反発係数を0にして弾まないようにする
    // 閾値は重力加速度やフレームレートによりますが、1.0f～2.0f程度で調整してください
    if (std::abs(velAlongNormal) < 1.0f)
    {
        e = 0.0f;
    }

    // 4. インパルス（撃力）のスカラ値 j の計算
    // 公式: j = -(1+e)(v_rel・n) / (1/Ma + 1/Mb + (Ia^-1(rA x n) x rA + ... )・n)

    float raxn_sq = 0.0f;
    float rbxn_sq = 0.0f;

    // Aの回転成分への影響計算
    Vec3 raxn = rA.Cross(m.normal);
    Vec3 iaInv_raxn = ApplyInertiaInverse(A, raxn); // 慣性テンソルの逆行列を掛ける
    raxn_sq = iaInv_raxn.Cross(rA).Dot(m.normal);

    // Bの回転成分への影響計算
    Vec3 rbxn = rB.Cross(m.normal);
    Vec3 ibInv_rbxn = ApplyInertiaInverse(B, rbxn);
    rbxn_sq = ibInv_rbxn.Cross(rB).Dot(m.normal);

    float j = -(1.0f + e) * velAlongNormal;
    j /= (A->invMass + B->invMass + raxn_sq + rbxn_sq);

    // 5. インパルスベクトルの適用
    Vec3 impulse = m.normal * j;

    // Aへの適用（逆方向）
    if (A->invMass > 0) {
        // 移動量（速度）の変化
        A->velocity = A->velocity - (impulse * A->invMass);

        // 角運動量（角速度）の変化: Torque = r x F -> AngularVel += I^-1 * (r x Impulse)
        Vec3 torqueImpulse = rA.Cross(impulse);
        A->angularVel = A->angularVel - ApplyInertiaInverse(A, torqueImpulse);
    }

    // Bへの適用（正方向）
    if (B->invMass > 0) {
        B->velocity = B->velocity + (impulse * B->invMass);

        Vec3 torqueImpulse = rB.Cross(impulse);
        B->angularVel = B->angularVel + ApplyInertiaInverse(B, torqueImpulse);
    }

    // ※めり込み解消（Positional Correction)は一旦なし
    if (true)
    // 速度解決と別に「位置」を押し戻さないと、食い込みが残って不安定になる
    {
        const float percent = 0.25f;  // 押し戻しを強める（積み上げ/面接地が安定しやすい）
        const float slop = 0.001f;    // 小さいめり込みは許容して震えを減らす

        const float invMassSum = A->invMass + B->invMass;

        if (invMassSum > 0.0f)
        {
            float depth = m.depth;
            float correctionMag = std::max(depth - slop, 0.0f) * (percent / invMassSum);
            Vec3 correction = m.normal * correctionMag;

            if (A->invMass > 0.0f) A->center = A->center - correction * A->invMass;
            if (B->invMass > 0.0f) B->center = B->center + correction * B->invMass;
        }
    }
}

Vec3 CollisionResolver::ApplyInertiaInverse(const RigidBodyOBB* body, const Vec3& v) {
    if (body->invMass == 0) return Vec3(0, 0, 0);

    // 1. ベクトル v をボディのローカル空間へ（転置行列を掛けるのと同義）
    //    Axisが正規直交基底なので、内積で成分分解できる
    float x = body->axis[0].Dot(v);
    float y = body->axis[1].Dot(v);
    float z = body->axis[2].Dot(v);

    // 2. ローカル空間で逆慣性モーメントを掛ける (対角成分のみなので単純な掛け算)
    //    I^-1 = (1/Ix, 1/Iy, 1/Iz)
    if (body->inertiaTensor.x != 0) x /= body->inertiaTensor.x;
    if (body->inertiaTensor.y != 0) y /= body->inertiaTensor.y;
    if (body->inertiaTensor.z != 0) z /= body->inertiaTensor.z;

    // 3. 結果をワールド空間へ戻す
    return body->axis[0] * x + body->axis[1] * y + body->axis[2] * z;
}


void RigidBodyOBB::AddLinearVelocity(const Vec3& dv) { velocity += dv; }
void RigidBodyOBB::AddAngularVelocity(const Vec3& dw) { angularVel += dw; }

void RigidBodyOBB::ClampLinear(float vMax) { ; }
void RigidBodyOBB::ClampAngular(float wMax) { ; }

int GetTopFace(const RigidBodyOBB& b)
{
    const Vec3 worldUp(0.0f, 1.0f, 0.0f);

    // 6面の法線（符号反転は * -1.0f）
    Vec3 normals[6] =
    {
        b.axis[0],              // +X
        b.axis[0] * -1.0f,      // -X
        b.axis[1],              // +Y
        b.axis[1] * -1.0f,      // -Y
        b.axis[2],              // +Z
        b.axis[2] * -1.0f       // -Z
    };

    // 対応する出目
    int faceValue[6] =
    {
        4, // +X
        3, // -X
        1, // +Y
        6, // -Y
        2, // +Z
        5  // -Z
    };

    int bestFace = 1;
    float bestDot = -FLT_MAX;

    for (int i = 0; i < 6; ++i)
    {
        float d = normals[i].Dot(worldUp);
        if (d > bestDot)
        {
            bestDot = d;
            bestFace = faceValue[i];
        }
    }

    return bestFace;
}
