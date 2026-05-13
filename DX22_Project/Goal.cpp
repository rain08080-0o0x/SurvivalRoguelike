#include "Goal.h"
#include "Sprite.h"
#include <Windows.h>

using namespace DirectX;

/**
 * @brief ゴールを生成し、描画テクスチャとスケールを設定します。
 * @param size 表示に使う拡大率です。
 */
Goal::Goal(XMFLOAT3 size)
    : m_pGoalTex(nullptr)
    , m_pCamera(nullptr)
{
    // ゴール専用画像を読み込み、クリア地点として見える状態にします。
    m_pGoalTex = new Texture();
    if (FAILED(m_pGoalTex->Create("Assets/Texture/GoalTex.png")))
    {
        MessageBox(nullptr, "Texture load failed.\nGoal.cpp", "Error", MB_OK);
    }

    // 呼び出し側で決めたサイズを保持し、描画へ使います。
    m_scale = size;
}

/**
 * @brief ゴールが保持するテクスチャを破棄します。
 */
Goal::~Goal()
{
    delete m_pGoalTex;
    m_pGoalTex = nullptr;
}

/**
 * @brief ゴールを軽く上下させて視認しやすくします。
 */
void Goal::Update()
{
    // 簡易アニメーションとして時間を進め、毎フレーム少しだけ上下させます。
    static float t = 0.0f;
    t += 0.05f;
    m_pos.y += sinf(t) * 0.01f;
}

/**
 * @brief ゴールをカメラ正面向きのスプライトとして描画します。
 */
void Goal::Draw()
{
    // カメラ未設定時は描画行列を組めないため処理しません。
    if (!m_pCamera || !m_pGoalTex) return;

    // スプライト描画に使う View / Projection を現在カメラから設定します。
    Sprite::SetView(m_pCamera->GetViewMatrix());
    Sprite::SetProjection(m_pCamera->GetProjectionMatrix());

    // ゴールは常にカメラ正面へ向けるため、ビルボード回転を作ります。
    XMMATRIX billboard = XMMatrixIdentity();

    if (m_pCamera)
    {
        // 転置していない View 行列を取得し、回転成分だけ抜き出します。
        XMFLOAT4X4 viewFloat;
		viewFloat = m_pCamera->GetViewMatrix(false);

        XMMATRIX viewMat = XMLoadFloat4x4(&viewFloat);

        // 逆行列でカメラの向きを打ち消し、正面向き板ポリに使います。
        XMMATRIX invView = XMMatrixInverse(nullptr, viewMat);

        XMFLOAT4X4 invViewFloat;
        XMStoreFloat4x4(&invViewFloat, invView);

        // 平行移動は別途与えるので、ここでは回転のみ残します。
        invViewFloat._41 = 0.0f;
        invViewFloat._42 = 0.0f;
        invViewFloat._43 = 0.0f;

        billboard = XMLoadFloat4x4(&invViewFloat);
    }

    // スケールと位置を合成し、ワールド行列を構築します。
    XMMATRIX world =
        billboard *
        XMMatrixScaling(m_scale.x, m_scale.y, 1.0f) *
        XMMatrixTranslation(m_pos.x, m_pos.y, m_pos.z);

    // Sprite が読む形式へ転置して書き出します。
    XMFLOAT4X4 worldFloat;
    XMStoreFloat4x4(&worldFloat, XMMatrixTranspose(world));

    // ゴールのスプライト描画設定をまとめて反映します。
    Sprite::SetColor({ 1,1,1,1 });
    Sprite::SetOffset({ 0,0 });
    Sprite::SetWorld(worldFloat);
    Sprite::SetSize({ 1.0f, 1.0f });
    Sprite::SetUVPos({ 0,0 });
    Sprite::SetUVScale({ 1,1 });
    Sprite::SetTexture(m_pGoalTex);
    Sprite::Draw();
}

/**
 * @brief 描画に使うカメラを設定します。
 * @param camera 新しく参照するカメラです。
 */
void Goal::SetCamera(Camera* camera)
{
    m_pCamera = camera;
}
