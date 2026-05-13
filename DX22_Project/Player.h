#pragma once
#include "GameObject.h"
#include "Texture.h"
#include "Sprite.h"
#include <DirectXMath.h>
#include "TrailEffect.h"
#include "Camera.h"

class TrailEffect;

/**
 * @brief プレイヤー本体の状態と描画、移動、回避処理を管理するクラスです。
 */
class Player : public GameObject
{
public:
    /**
     * @brief プレイヤーを生成し、必要な描画リソースを初期化します。
     * @param camera ビルボード描画と進行方向表示に使用するカメラです。
     */
    Player(Camera*);

    /**
     * @brief プレイヤーが確保したリソースを解放します。
     */
    ~Player();

    /**
     * @brief 転送データとの同期、移動、ステージ内補正を行ってプレイヤー状態を更新します。
     */
    void Update() override;

    /**
     * @brief プレイヤー本体とトレイルを描画します。
     */
    void Draw() override;

    /**
     * @brief プレイヤーの進行方向を示す地面マーカーを描画します。
     */
    void DrawDirectionMarker();

    /**
     * @brief 描画参照先となるカメラを差し替えます。
     * @param set 新しく参照するカメラです。
     */
    void SetCamera(Camera*set);

    /**
     * @brief 現在回避中かどうかを返します。
     * @return 回避移動中なら true、それ以外は false です。
     */
    bool IsEvading() const { return m_isDashing; }

    /**
     * @brief 回避クールタイムの残り時間を返します。
     * @return 次に回避可能になるまでの残り秒数です。
     */
    float GetEvadeCooldownRemain() const { return m_dashCooldownTimer; }

    /**
     * @brief 現在の実効回避クールタイムを返します。
     * @return 強化補正込みの回避クールタイム秒数です。
     */
    float GetEvadeCooldownDuration() const { return m_effectiveDashCooldown; }

private:
    /**
     * @brief Transfer からプレイヤー関連の最新値を取り込みます。
     */
    void SyncFromTransfer();

    /**
     * @brief プレイヤー更新結果を Transfer へ反映します。
     */
    void SyncToTransfer();

    /**
     * @brief 入力を解釈して通常移動または回避移動を処理します。
     * @param dt 今フレームで進める秒数です。
     */
    void ApplyMovement(float dt);

    /**
     * @brief プレイヤー座標をステージ範囲内に収めます。
     */
    void ClampToStage();


private:
    /** @brief ビルボード計算やトレイル描画で参照するカメラです。 */
    Camera* m_pCamera;

    /** @brief プレイヤー本体のスプライト描画に使うテクスチャです。 */
    Texture* m_pTexture;

    /** @brief 進行方向マーカー描画に使うテクスチャです。 */
    Texture* m_pDirectionTexture;

    /** @brief プレイヤーの当たり判定と描画サイズです。 */
    DirectX::XMFLOAT3 m_size;

    /** @brief 今フレームの移動速度をワールド座標系で保持します。 */
    DirectX::XMFLOAT3 m_velocity;

    /** @brief 直近の入力または回避方向から決まる向きです。 */
    DirectX::XMFLOAT3 m_facingDir;

    /** @brief プレイヤー描画時の色です。 */
    DirectX::XMFLOAT4 m_color;

    /** @brief 現在HPです。 */
    float m_hp;

    /** @brief 最大HPです。 */
    float m_maxHp;

    /** @brief 通常移動時の移動速度です。 */
    float m_moveSpeed;

    /** @brief 1回の回避で進む距離です。 */
    float m_dashDistance;

    /** @brief 基本となる回避クールタイムです。 */
    float m_dashCooldown;

    /** @brief 回避移動そのものの継続時間です。 */
    float m_dashDuration;

    /** @brief 回避動作の残り時間です。 */
    float m_dashTimer;

    /** @brief 次回回避までの残りクールタイムです。 */
    float m_dashCooldownTimer;

    /** @brief 強化補正を反映した現在の実効クールタイムです。 */
    float m_effectiveDashCooldown;

    /** @brief 回避開始時に固定する回避方向です。 */
    DirectX::XMFLOAT3 m_dashDir;

    /** @brief 回避動作中かどうかを表します。 */
    bool m_isDashing;

    /** @brief ステージ一辺の長さです。 */
    float m_stageSize;

    /** @brief プレイヤーの移動軌跡を描画するトレイルです。 */
    TrailEffect* m_pTrail;

    /** @brief トレイル用の描画テクスチャです。 */
    Texture* m_pTrailEffectTexture;
};
