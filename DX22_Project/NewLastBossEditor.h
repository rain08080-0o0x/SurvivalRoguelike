// SceneNewLastBossEditor.h

#pragma once
#include "Scene.h"
#include "Texture.h"
#include "Sprite.h"
#include "ShaderList.h"
#include "Camera.h"
#include <DirectXMath.h>

struct CharInfo
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT3 size;
    bool useBillboard;
    float yawDeg;
};

/// <summary>
/// 
/// </summary>
/// @param None     : 攻撃なし状態
/// @param UnUsed   : 攻撃未発動状態
/// @param Using    : 攻撃発動中
/// @param Used     : 攻撃発動後
enum AttackState
{
    None,
    UnUsed,
    UseStart,
    Using,
    Used,
};

class NewLastBoss : public Scene
{
public:
    NewLastBoss();
    ~NewLastBoss();

    void Init();
    void Uninit();
    void Update();
    void Draw();

private:

    void UpdatePlayer();
    void UpdateBoss();
    void DrawField();

    // ランダムスラッシュ攻撃を追加する関数。最大が10。
    // それ以上は追加できず初期化しないと再発動はできない。
    void AddAttack();

    // ランダムスラッシュの更新
    // プレイヤーの位置を中心にランダムな角度で攻撃が発生、
    // その場に一定時間の範囲を描画の後、攻撃が発生する。
    void RandomSlashUpdate();

    // ランダムスラッシュの描画
    // 当たり判定と、攻撃範囲を描画
    // テクスチャを出しているものの処理内容は当たり判定依存。
    void RandomSlashDraw();

    // クロス攻撃の更新
    // ステージ全体を斜めに切るように攻撃が発生。
    // 攻撃の向きは45度で、格子状
    void CrossUpdate();

    // クロス攻撃の描画
    // 当たり判定と、攻撃範囲を描画
    // テクスチャを出しているものの処理内容は当たり判定依存。
    void CrossDraw();

    // プレイヤー依存のクロス攻撃の更新
    // 最初にステージの中心からみたプレイヤーの位置を求め、
    // ステージの中心を通る直線を求めた角度で攻撃が発生。
    // その求めた角度に対して45度ずらした攻撃を追加で発生させる格子攻撃。
    // 攻撃は基の角度の方と45度ずらした方の両方で、
    // 攻撃が終わるたびにもう一度発動し段々攻撃が外側に増える。
    // 同心円状ならぬ同心平行状。
    void CircleCrossUpdate();
    // プレイヤー依存のクロス攻撃の描画
    void CircleCrossDraw();

    // 円形攻撃
    // 所謂、ドーナッツ型の当たり判定で、当たり判定の描画としては徐々に広がる
    // 同心円状
    void CircleAttackUpdate();


    void CircleAttackDraw();

    // デバッグ用のGUI描画関数。様々な情報を表示するための関数。
    // 随時更新
    void DrawDebugGUI();
    Camera* camera;
    Texture* m_pPlayer;
    Texture* m_pBoss;
    CharInfo player;
    CharInfo boss;

    float playerSpeed = 0.01f;

    Texture* m_pFieldtex;   // フィールドのテクスチャ
    Texture* m_pAtkTex;     // 攻撃のテクスチャ
	Texture* m_pKawasakiTex[6];    // コックカワサキのテクスチャ

    // ランダムスラッシュの攻撃状態を表す列挙型。
    // デフォはUnUsed
    // 攻撃最中はUseing
    // 攻撃後はUsed
    //
    enum class SlashState
    {
        UnUsed,
        Useing,
        Used
    };
    /// <summary>
    /// 攻撃の各情報をまとめた構造体。
    /// </summary>
    struct RandomSlash
    {
        SlashState isUsed;                // 最大同時攻撃数10を上限。攻撃が発生しているかどうか
        int frame;                  // 攻撃のフレームカウンタ。攻撃開始から何フレーム経過したかをカウントする。
        float angle;                // 攻撃角度。PI*2が一周。攻撃の向きを表す。
        DirectX::XMFLOAT3 start;    // 攻撃の中間
        DirectX::XMFLOAT3 end;      // 
        DirectX::XMFLOAT3 middle;   // 
    };

    RandomSlash rs_attack[10];
    int count = 0;

    enum CrossState
    {
        First,
        Second,
        Final,
        Max
    };
    CrossState crossState = CrossState::Max;

    struct CircleCrossDate
    {
        float angle;        // 攻撃角度。Playerに位置から定義
        int frame;        //攻撃のフレームカウンタ。攻撃開始から何フレーム経ったかをカウント。
		float interval;     // 攻撃のインターバル。攻撃開始から次の攻撃が発生するまでの時間。
        int atkCount;       // 攻撃回数。攻撃が何回発生したかをカウント。
    };

public:
    struct CircleCross
    {
        AttackState state = AttackState::Used;
        CircleCrossDate first;  // 最初の攻撃のデータ
        CircleCrossDate second;  // 最初の攻撃のデータ
        float width;        // 攻撃の幅。長さはステージを切るように長いのでほぼ固定。
        const float length = 10.0f * sqrtf(2.0f); // 攻撃の長さ。ステージを丁度斜めに切っても届くように長めに定義。
        int attackInterval;   // 二種類目(45度ずれ攻撃)の発生までの時間。フレーム数で設定
        int attackProgress;     // 攻撃の進行度。フレームカウント方式で進行。使用になったら進行。使用後リセット
    };
private:
    CircleCross cc; // CircleCross
    float sabun;
    float distance;
};
