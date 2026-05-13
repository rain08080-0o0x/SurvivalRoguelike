#pragma once
#include <DirectXMath.h>
#include "GameObject.h"
#include "Texture.h"
#include "Camera.h"

/**
 * @brief ステージクリア判定に使うゴール表示オブジェクトです。
 */
class Goal :
    public GameObject
{
private:
    /** @brief ゴール表示に使うテクスチャです。 */
    Texture* m_pGoalTex;

    /** @brief ビルボード描画で参照するカメラです。 */
    Camera* m_pCamera;

    /** @brief ゴールスプライトの拡大率です。 */
    DirectX::XMFLOAT3 m_scale;

public:
    /**
     * @brief ゴールを生成し、テクスチャと表示サイズを初期化します。
     * @param size 表示に使う拡大率です。
     */
    Goal(DirectX::XMFLOAT3 size);

    /**
     * @brief ゴールが保持するリソースを解放します。
     */
    ~Goal();

    /**
     * @brief 簡易アニメーション用に位置を更新します。
     */
    void Update() override;

    /**
     * @brief ゴールをビルボードスプライトとして描画します。
     */
    void Draw() override;

    /**
     * @brief 描画に使うカメラを設定します。
     * @param camera ビルボード計算に使うカメラです。
     */
    void SetCamera(Camera* camera);
};
