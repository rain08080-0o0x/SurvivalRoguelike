#pragma once
#include "GameObject.h"
#include "Texture.h"
#include "Sprite.h"
#include "Collision.h"
#include <DirectXMath.h>
#include "Camera.h"

class Enemy : public GameObject
{
public:
    /**
     * @brief 敵の性能プリセット種別です。
     */
    enum class Type
    {
        /** @brief 移動と手数を重視した軽量タイプです。 */
        Speed = 0,
        /** @brief 耐久と一撃の重さを重視した重量タイプです。 */
        Tank = 1,
        /** @brief 射程を伸ばした遠距離寄りタイプです。 */
        Ranged = 2
    };

    /**
     * @brief 敵を生成し、描画と基本ステータスを初期化します。
     */
    Enemy();

    /**
     * @brief 敵が保持する描画リソースを解放します。
     */
    ~Enemy();

    /**
     * @brief AI 状態に応じて徘徊または追跡を 1 フレーム分更新します。
     */
    void Update() override;

    /**
     * @brief 敵スプライトをビルボードで描画します。
     */
    void Draw() override;

    /**
     * @brief 敵の当たり判定と描画サイズを設定します。
     * @param size X/Y/Z のサイズです。
     */
    void SetSize(const DirectX::XMFLOAT3& size);

    /**
     * @brief 行動可能なステージサイズを設定します。
     * @param size ステージ一辺の長さです。
     */
    void SetStageSize(float size);

    /**
     * @brief 基本移動速度を設定します。
     * @param speed タイプ補正前の基準移動速度です。
     */
    void SetMoveSpeed(float speed);

    /**
     * @brief 敵種別を切り替え、種別ごとの能力値を適用します。
     * @param type 適用する敵種別です。
     */
    void SetType(Type type);

    /**
     * @brief 最大 HP を倍率で再計算します。
     * @param scale 最大 HP に掛ける倍率です。
     */
    void SetHpScale(float scale);

    /**
     * @brief 現在のサイズを返します。
     * @return 敵の X/Y/Z サイズです。
     */
    DirectX::XMFLOAT3 GetSize() const;

    /**
     * @brief AABB 判定用のボックスを返します。
     * @return 現在位置とサイズから組み立てた衝突ボックスです。
     */
    Collision::Box GetCollision() const;

    /**
     * @brief 敵にダメージを与えます。
     * @param amount 減算する HP 値です。
     */
    void Damage(int amount);

    /**
     * @brief 生存しているかを返します。
     * @return HP が 1 以上なら true、0 以下なら false です。
     */
    bool IsAlive() const;

    /**
     * @brief 現在 HP を返します。
     * @return 現在 HP です。
     */
    int GetHp() const;

    /**
     * @brief 最大 HP を返します。
     * @return 最大 HP です。
     */
    int GetMaxHp() const;

    /**
     * @brief 現在の移動状態を数値で返します。
     * @return Chase 中は 1、Wander 中は 0 です。
     */
    int GetState() const;

    /**
     * @brief 現在の敵種別を数値で返します。
     * @return Type 列挙値を int へ変換した値です。
     */
    int GetType() const;

    /**
     * @brief 攻撃射程倍率を返します。
     * @return 種別に応じた攻撃射程倍率です。
     */
    float GetAttackRangeScale() const;

    /**
     * @brief 攻撃予兆時間倍率を返します。
     * @return 種別に応じた windup 倍率です。
     */
    float GetAttackWindupScale() const;

    /**
     * @brief 攻撃クールタイム倍率を返します。
     * @return 種別に応じた cooldown 倍率です。
     */
    float GetAttackCooldownScale() const;

    /**
     * @brief 攻撃ダメージ倍率を返します。
     * @return 種別に応じたダメージ倍率です。
     */
    float GetAttackDamageScale() const;

    /**
     * @brief 描画時に参照するカメラを設定します。
     * @param camera ビルボード計算に使うカメラです。
     */
    void SetCamera(Camera* camera);

    /**
     * @brief 追跡対象となる座標を設定します。
     * @param targetPos 追跡先のワールド座標です。
     */
    void SetTargetPos(DirectX::XMFLOAT3 targetPos);

    /**
     * @brief 徘徊を無効化して常に追跡移動するか設定します。
     * @param forceChase true の間は追跡状態を維持します。
     */
    void SetForceChase(bool forceChase);
private:
    /**
     * @brief 敵の移動 AI 状態です。
     */
    enum class MoveState
    {
        /** @brief ランダム目標へ向かって徘徊している状態です。 */
        Wander,
        /** @brief プレイヤーを追跡している状態です。 */
        Chase
    };

    /** @brief ビルボード描画で参照するカメラです。 */
    Camera* m_pCamera;
    /** @brief 敵の見た目を描画するテクスチャです。 */
    Texture* m_pTexture;
    /** @brief 当たり判定と描画に使う敵サイズです。 */
    DirectX::XMFLOAT3 m_size;
    /** @brief 種別に応じて切り替える描画色です。 */
    DirectX::XMFLOAT4 m_color;
    /** @brief 実際に移動へ使う速度です。 */
    float m_moveSpeed;
    /** @brief 種別補正前の基準移動速度です。 */
    float m_moveSpeedBase;
    /** @brief 種別に応じた移動速度倍率です。 */
    float m_moveSpeedScale;
    /** @brief 行動可能なステージ一辺の長さです。 */
    float m_stageSize;
    /** @brief 旧仕様由来の予備移動方向です。 */
    float m_moveDirX;
    /** @brief 現在 HP です。 */
    int m_hp;
    /** @brief 最大 HP です。 */
    int m_maxHp;
    /** @brief 現在の敵種別です。 */
    Type m_type;
    /** @brief 攻撃射程倍率です。 */
    float m_attackRangeScale;
    /** @brief 攻撃予兆倍率です。 */
    float m_attackWindupScale;
    /** @brief 攻撃クールタイム倍率です。 */
    float m_attackCooldownScale;
    /** @brief 攻撃ダメージ倍率です。 */
    float m_attackDamageScale;
    /** @brief 追跡対象の現在座標です。 */
    DirectX::XMFLOAT3 m_targetPos;
    /** @brief 徘徊時に向かう一時目標地点です。 */
    DirectX::XMFLOAT3 m_wanderTarget;
    /** @brief 徘徊目標を再抽選するまでの残り時間です。 */
    float m_wanderTimer;
    /** @brief 現在の移動 AI 状態です。 */
    MoveState m_state;
    /** @brief true の間は徘徊へ戻らず追跡移動を続けます。 */
    bool m_forceChase;
};
