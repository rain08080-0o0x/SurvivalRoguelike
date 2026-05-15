#pragma once

#include "Scene.h"
#include "NarakuMapData.h"

#include <Windows.h>
#include <DirectXMath.h>
#include <string>

class Texture;

/**
 * @brief 奈落塔用の固定マップを編集する地形生成エディタシーンです。
 *
 * 地形レイヤー、ロープ、採掘ポイントを ImGui と簡易 3D Gizmo で編集し、
 * JSON へ保存・読み込みします。
 */
class SceneNarakuEditor : public Scene
{
public:
    /** @brief エディタに必要な一時リソースと既定マップを初期化します。 */
    SceneNarakuEditor();

    /** @brief 一時リソースを解放します。 */
    ~SceneNarakuEditor() override;

    /** @brief カメラ操作、選択、編集入力を更新します。 */
    void Update() override;

    /** @brief 3D プレビューと ImGui エディタ UI を描画します。 */
    void Draw() override;

private:
    /** @brief 現在の編集対象モードです。 */
    enum class EditMode
    {
        Terrain = 0,
        Rope,
        MiningPoint
    };

    /** @brief 地形頂点の選択状態です。 */
    struct VertexSelection
    {
        /** @brief 選択中レイヤーの配列インデックスです。 */
        int layerIndex = -1;

        /** @brief レイヤー内の X 頂点番号です。 */
        int gridX = -1;

        /** @brief レイヤー内の Z 頂点番号です。 */
        int gridZ = -1;
    };

private:
    /** @brief マップ保存先から読み込み、失敗時は既定マップを用意します。 */
    void InitializeMap();

    /** @brief 現在の入力に応じてカメラを更新します。 */
    void UpdateCamera(float dt);

    /** @brief 現在の編集モードに応じた選択と編集操作を更新します。 */
    void UpdateSelectionAndEditing();

    /** @brief F キーやボタン呼び出し用のフォーカス処理です。 */
    void FocusSelection();

    /** @brief 保存先へ現在のマップを保存します。 */
    void SaveCurrentMap();

    /** @brief 保存先からマップを読み込みます。 */
    void LoadCurrentMap();

    /** @brief レイヤー、ロープ、採掘ポイントの選択番号を妥当な範囲へ直します。 */
    void EnsureSelectionValid();

    /** @brief 3D プレビューのビュー行列と射影行列を構築して描画系へ反映します。 */
    void ApplyCameraMatrices();

    /** @brief 3D プレビューのワイヤーやオブジェクトを描画します。 */
    void DrawWorld();

    /** @brief エディタ全体の ImGui UI を描画します。 */
    void DrawEditorUi();

    /** @brief 左側のマップ全体設定ウィンドウを描画します。 */
    void DrawMapWindow();

    /** @brief レイヤー編集ウィンドウを描画します。 */
    void DrawLayerWindow();

    /** @brief ロープ編集ウィンドウを描画します。 */
    void DrawRopeWindow();

    /** @brief 採掘ポイント編集ウィンドウを描画します。 */
    void DrawMiningWindow();

    /** @brief 操作説明とデバッグ状態を描画します。 */
    void DrawHelpWindow();

    /** @brief 頂点選択時の簡易 Gizmo を描画します。 */
    void DrawVertexGizmo();

    /** @brief 指定レイヤーの半透明床とグリッド線を描画します。 */
    void DrawTerrainLayer(const NarakuMap::TerrainLayer& layer, int layerIndex);

    /** @brief カーソル直下の頂点を Maya 風に色分け表示します。 */
    void DrawHoveredVertexHighlight();

    /** @brief ロープ一覧を 3D 上へ描画します。 */
    void DrawRopes();

    /** @brief 採掘ポイント一覧を 3D 上へ描画します。 */
    void DrawMiningPoints();

    /** @brief ワールド上の立方体マーカーを描画します。 */
    void DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale) const;

    /** @brief 半透明床を Sprite ベースで描画します。 */
    void DrawTransparentFloor3D(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color) const;

    /** @brief レイヤー深度と頂点高さからワールド Y を計算します。 */
    float ToWorldY(float layerDepth, float height) const;

    /** @brief レイヤー頂点をワールド座標へ変換します。 */
    DirectX::XMFLOAT3 GetVertexWorldPosition(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const;

    /** @brief レイヤー中心とサイズから半透明床の中心を求めます。 */
    DirectX::XMFLOAT3 GetLayerFloorCenter(const NarakuMap::TerrainLayer& layer) const;

    /** @brief レイヤーサイズを半透明床の XY サイズへ変換します。 */
    DirectX::XMFLOAT2 GetLayerFloorSize(const NarakuMap::TerrainLayer& layer) const;

    /** @brief 対象位置を画面座標へ投影します。 */
    bool ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPos, DirectX::XMFLOAT2& outScreen) const;

    /** @brief 現在選択中レイヤー上の最寄り頂点を拾います。 */
    bool PickTerrainVertex(POINT mousePos, int& outGridX, int& outGridZ) const;

    /** @brief 画面クリックから最寄りロープを拾います。 */
    int PickRope(POINT mousePos) const;

    /** @brief 画面クリックから最寄り採掘ポイントを拾います。 */
    int PickMiningPoint(POINT mousePos) const;

    /** @brief 頂点 Gizmo をドラッグ開始できるか判定します。 */
    bool IsMouseNearSelectedVertexHandle(POINT mousePos) const;

    /** @brief 現在注目中の深度を返します。 */
    float GetFocusDepth() const;

    /** @brief 指定レイヤーの描画アルファを決定します。 */
    float GetLayerAlpha(const NarakuMap::TerrainLayer& layer) const;

    /** @brief テクスチャ ID に対応する表示名を返します。 */
    const char* GetGroundTextureLabel(int textureId) const;

    /** @brief テクスチャ ID に対応するプレビュー色を返します。 */
    DirectX::XMFLOAT4 GetGroundTextureTint(int textureId) const;

    /** @brief ワイド文字列を ImGui 表示用 UTF-8 文字列へ変換します。 */
    std::string WideToUtf8(const wchar_t* text) const;

    /** @brief 指定レイヤー ID の配列インデックスを返します。 */
    int FindLayerIndexById(int layerId) const;

    /** @brief 新規レイヤー用の初期データを生成します。 */
    NarakuMap::TerrainLayer CreateNewLayer() const;

    /** @brief 新規ロープ用の初期データを生成します。 */
    NarakuMap::RopePoint CreateNewRope() const;

    /** @brief 新規採掘ポイント用の初期データを生成します。 */
    NarakuMap::MiningPoint CreateNewMiningPoint() const;

private:
    /** @brief 半透明床描画用の 1x1 白テクスチャです。 */
    Texture* m_debugWhiteTexture = nullptr;

    /** @brief 現在編集中のマップデータです。 */
    NarakuMap::MapData m_mapData;

    /** @brief 現在の編集対象モードです。 */
    EditMode m_editMode = EditMode::Terrain;

    /** @brief 選択中レイヤー番号です。 */
    int m_selectedLayer = 0;

    /** @brief 選択中ロープ番号です。 */
    int m_selectedRope = -1;

    /** @brief 選択中採掘ポイント番号です。 */
    int m_selectedMiningPoint = -1;

    /** @brief 選択中頂点情報です。 */
    VertexSelection m_selectedVertex;

    /** @brief 頂点高さドラッグ中かどうかです。 */
    bool m_draggingVertexHeight = false;

    /** @brief 直近のマップ IO メッセージです。 */
    std::string m_lastIoMessage;

    /** @brief レイヤー手前透過時の最小アルファ値です。 */
    float m_frontLayerAlpha = 0.22f;

    /** @brief 編集カメラの注視点です。 */
    DirectX::XMFLOAT3 m_cameraTarget = { 0.0f, 0.0f, 0.0f };

    /** @brief 編集カメラの Yaw 角です。 */
    float m_cameraYaw = DirectX::XMConvertToRadians(225.0f);

    /** @brief 編集カメラの Pitch 角です。 */
    float m_cameraPitch = DirectX::XMConvertToRadians(38.0f);

    /** @brief 編集カメラの注視点からの距離です。 */
    float m_cameraDistance = 38.0f;

    /** @brief 現在フレームで使うビュー行列です。 */
    DirectX::XMFLOAT4X4 m_viewMatrix = {};

    /** @brief 現在フレームで使う射影行列です。 */
    DirectX::XMFLOAT4X4 m_projectionMatrix = {};
};
