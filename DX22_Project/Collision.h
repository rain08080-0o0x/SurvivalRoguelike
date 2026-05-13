#pragma once
#include<DirectXMath.h>

class Collision {
public:
    //--- 当たり判定
    // 立方体
    struct Box
    {
        DirectX::XMFLOAT3 center;   // 中心座標
        DirectX::XMFLOAT3 size;     // サイズ
    };
    // 球
    struct Sphere 
    {
        DirectX::XMFLOAT3 center;   // 中心座標
        float radius;               // 半径
    };
    //--- 当たり判定の結果
    struct Result 
    {
        bool isHit;             // 当たったかどうか
        DirectX::XMFLOAT3 dir;  // ヒット方向
        //      
    };
public:
    //  立方体同士の当たり判定
    static Result Hit(Box a, Box b);

    /// <summary>
    /// 
    /// </summary>
    /// <param name="a"></param>
    /// <param name="b"></param>
    /// <returns></returns>
    static Result Hit(Sphere a, Sphere b);
};

#include <cmath>
#include <algorithm>

// 基本的な3次元ベクトル（DirectXのXMFLOAT3相当だが演算を追加）
struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    // ベクトル演算のオーバーロード
    Vec3 operator+(const Vec3& v) const { return Vec3(x + v.x, y + v.y, z + v.z); }
    Vec3& operator+=(const Vec3& v)
    {
        x += v.x; y += v.y; z += v.z;
        return *this;
    }
    Vec3 operator-(const Vec3& v) const { return Vec3(x - v.x, y - v.y, z - v.z); }
    Vec3 operator*(float s) const { return Vec3(x * s, y * s, z * s); }
    Vec3 operator=(const DirectX::XMFLOAT3 f3)const { return Vec3(f3.x, f3.y, f3.z); }
    // 内積
    float Dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }

    // 外積（回転計算に必須）
    Vec3 Cross(const Vec3& v) const {
        return Vec3(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
    }

    // 正規化
    Vec3 Normalize() const {
        float len = std::sqrt(x * x + y * y + z * z);
        if (len > 0) return Vec3(x / len, y / len, z / len);
        return *this;
    }

    static Vec3 Vector3Zero() { return Vec3(0.0f, 0.0f, 0.0f); }
    // ===== Vec3 追加メソッド =====

// 長さの2乗（sqrtしないので高速）
    float LengthSq() const
    {
        return x * x + y * y + z * z;
    }

    // ベクトルの長さ
    float Length() const
    {
        return sqrtf(LengthSq());
    }

    // 正規化（自分を書き換える）
    Vec3& Normalize()
    {
        float len = Length();
        if (len > 0.00001f)
        {
            x /= len;
            y /= len;
            z /= len;
        }
        return *this;
    }

    // 正規化したコピーを返す
    Vec3 Normalized() const
    {
        float len = Length();
        if (len > 0.00001f)
            return Vec3(x / len, y / len, z / len);
        return Vec3(0, 0, 0);
    }

};
// Collision.h（RigidBodyOBB に追加）


// 物理挙動を持つOBBオブジェクト
class RigidBodyOBB
{
public:
    // 形状データ
    Vec3 center;        // 重心位置
    Vec3 extents;       // ハーフサイズ (width/2, height/2, depth/2)

    // 姿勢データ（回転行列の代わりに3つの軸ベクトルを持つ）
    Vec3 axis[3];       // 0:Right(X), 1:Up(Y), 2:Forward(Z)

    // 物理パラメータ
    float mass;         // 質量 (0なら静的オブジェクト)
    float invMass;      // 1/質量
    Vec3 velocity;      // 線形速度
    Vec3 angularVel;    // 角速度（ラジアン/秒）
    Vec3 inertiaTensor; // 慣性モーメント（直方体近似、ローカル軸）

    bool isGround;      // 地面

    // ---- 地面スナップ＆停止の閾値 ----
    float groundEps = 0.001f;      // 地面判定の許容(めり込み許容)
    float stopVelY = 0.05f;       // これ以下の落下速度なら止める(m/s換算なら値は小さめ)
    float stopMoveY = 0.0005f;     // これ以下のY移動なら止める
    float sleepTime = 0.2f;        // 安定がこれだけ続いたらsleep

    float stableTimer = 0.0f;      // 内部用

    // 完全停止フラグ
    bool fullyLocked = false;

    DirectX::XMFLOAT4 orientation = { 0,0,0,1 }; // quaternion

    /// <summary>
    /// RidigBodyのコンストラクタ
    /// </summary>
    /// <param name="pos">中心位置</param>
    /// <param name="size"サイズ</param>
    /// <param name="m">質量</param>
    RigidBodyOBB(Vec3 pos, Vec3 size, float m);

    // ワールド空間の8頂点を計算して取得
    // これが「頂点ごとにベクトルを用意」に相当します
    // 0+++		┃ 奥 5_______4
    // 1-++		┃      /         /
    // 2+-+		┃  7/_____ / 6
    // 3--+		┃   
    // 4++-		┃ 前 1_______0
    // 5-+-		┃      /        /
    // 6+--		┃  3/_____ / 2
    // 7---		┃ 
    //_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/_/
    void GetWorldVertices(Vec3 outVertices[8]) const;
    void GetWorldVertices(DirectX::XMFLOAT3 outVertices[8]) const;

    // 積分更新（毎フレーム呼ぶ）デフォルト引数(1フレーム)
    void Update(float dt = 1.0f / 60.0f);
    void AddLinearVelocity(const Vec3& dv);
    void AddAngularVelocity(const Vec3& dw);
    void ClampLinear(float vMax);
    void ClampAngular(float wMax);
    void IntegrateRotation(float dt);
    // 重力適応変更関数
    void SetGravity(bool enable);
    // 完全停止設定関数
    void SetFullyLocked(bool lock);
    // ===== Sleep / Gravity (追加ブロック) =====
    
    // 外力（力の蓄積）
    Vec3 forceAccum = Vec3(0, 0, 0);

    // 重力（加速度）とON/OFF
    Vec3 gravity = Vec3(0, -9.8f, 0);
    bool gravityEnabled = true;

    // 状態
    bool isStatic = false;     // 完全固定（Updateしない）
    bool isSleeping = false;   // 眠り（Updateしない）

    // 接地（毎フレーム外部で更新してOK）
    bool grounded = false;

    // sleep判定用
    Vec3 lastCenter = Vec3(0, 0, 0);
    float sleepTimer = 0.0f;

    // 閾値（調整用）
    float linearSleepEps = 0.05f;     // m/s
    float angularSleepEps = 0.2f;     // rad/s
    float moveSleepEps = 0.001f;      // m
    float sleepTimeToEnter = 0.5f;    // sec

    // 追加メソッド（RigidBodyOBB内に宣言）
    void ApplyForce(const Vec3& f)
    {
        if (isStatic) return;
        if (isSleeping) return;
        forceAccum += f;
    }

    void WakeUp()
    {
        isSleeping = false;
        sleepTimer = 0.0f;
        gravityEnabled = true;
    }

    void Sleep()
    {
        isSleeping = true;
        velocity = Vec3(0, 0, 0);
        angularVel = Vec3(0, 0, 0);
        forceAccum = Vec3(0, 0, 0);
        gravityEnabled = false;
    }

};

class CollisionResolver {
public:
    // 衝突情報構造体
    struct Manifold {
        RigidBodyOBB* bodyA;
        RigidBodyOBB* bodyB;
        Vec3 contactPoint; // ワールド空間での衝突点
        Vec3 normal;       // AからBへの衝突法線
        float depth;       // めり込み量
    };

    // 衝突応答の計算（移動量と回転量の適用）
    static void ResolveCollision(Manifold& m);

private:
    // ワールド空間での慣性テンソルの逆変換適用
    // I_world^-1 * v を計算する関数
    // Matrixを使わないため、ベクトルをローカルに戻して計算し、ワールドに戻す
    static Vec3 ApplyInertiaInverse(const RigidBodyOBB* body, const Vec3& v);
};

int GetTopFace(const RigidBodyOBB& b);