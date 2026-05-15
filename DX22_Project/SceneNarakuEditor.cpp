#include "SceneNarakuEditor.h"

#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "SceneManager.h"
#include "Sprite.h"
#include "Texture.h"
#include "imgui.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using namespace DirectX;

namespace
{
    /** @brief エディタ全体で共有する固定デルタ時間です。 */
    constexpr float kDt = 1.0f / fFPS;

    /** @brief 深度 1m を 3D ワールド高さへ変換するスケールです。 */
    constexpr float kDepthScale = 0.35f;

    /** @brief マウスピック時に有効とみなす画面上の距離です。 */
    constexpr float kPickThresholdPx = 18.0f;

    /** @brief 頂点高さドラッグ時の 1px あたりの変化量です。 */
    constexpr float kVertexDragScale = 0.035f;

    /** @brief カメラの最小ピッチです。 */
    constexpr float kMinPitch = 0.15f;

    /** @brief カメラの最大ピッチです。 */
    constexpr float kMaxPitch = 1.35f;

    /** @brief カメラ距離の最小値です。 */
    constexpr float kMinDistance = 6.0f;

    /** @brief カメラ距離の最大値です。 */
    constexpr float kMaxDistance = 120.0f;

    /** @brief 地面テクスチャ候補ラベルです。 */
    const char* kGroundTextureLabels[] =
    {
        u8"草地",
        u8"土",
        u8"岩",
        u8"湿地"
    };

    /** @brief `size_t` から `int` へ安全に変換します。 */
    int ToInt(size_t value)
    {
        return static_cast<int>(value);
    }

    /** @brief C++17 以前でも使える簡易 clamp です。 */
    template<typename T>
    T ClampValue(const T& value, const T& minValue, const T& maxValue)
    {
        return (value < minValue) ? minValue : ((value > maxValue) ? maxValue : value);
    }

    /** @brief 2D ベクトルの長さを返します。 */
    float Length2D(float x, float y)
    {
        return std::sqrt(x * x + y * y);
    }
}

SceneNarakuEditor::SceneNarakuEditor()
{
    // 半透明床プレビューに使う 1x1 白テクスチャを生成します。
    m_debugWhiteTexture = new Texture();

    // 単色テクスチャなので RGBA8 の白 1px だけ用意します。
    const unsigned int whitePixel = 0xffffffff;

    // Sprite 経由で色付き床を描くためのテクスチャを作成します。
    m_debugWhiteTexture->Create(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &whitePixel);

    // 既定マップの読み込みまたは新規生成を行います。
    InitializeMap();
}

SceneNarakuEditor::~SceneNarakuEditor()
{
    // 一時テクスチャを解放します。
    SAFE_DELETE(m_debugWhiteTexture);
}

void SceneNarakuEditor::InitializeMap()
{
    // まず保存済みマップの読み込みを試みます。
    LoadCurrentMap();

    // 読み込み後の選択番号を安全側へ寄せます。
    EnsureSelectionValid();

    // 最初のフォーカス位置を現在選択に合わせます。
    FocusSelection();
}

void SceneNarakuEditor::Update()
{
    // マウス・キーボードの編集挙動に使うカメラを先に更新します。
    UpdateCamera(kDt);

    // 現在のモードに応じた選択やドラッグ編集を更新します。
    UpdateSelectionAndEditing();

    // F キーで現在選択位置へカメラを寄せます。
    if (IsRawKeyTrigger('F'))
    {
        FocusSelection();
    }
}

void SceneNarakuEditor::Draw()
{
    // 3D プレビュー用の行列を更新してから描画へ入ります。
    ApplyCameraMatrices();

    // ワールド側の床、グリッド、各オブジェクトを描画します。
    DrawWorld();

    // ImGui 上の編集 UI を描画します。
    DrawEditorUi();
}

void SceneNarakuEditor::UpdateCamera(float dt)
{
    // ImGui がマウスやキーボードを掴んでいる間は、編集カメラ操作を抑えます。
    ImGuiIO& io = ImGui::GetIO();
    const bool allowMouseCamera = !io.WantCaptureMouse;
    const bool allowKeyboardCamera = !io.WantCaptureKeyboard;

    // マウスの相対移動量を取得します。
    const POINT mouseDelta = GetMouseDelta();

    // Alt+左ドラッグで注視点回りの Orbit を行います。
    if (allowMouseCamera && io.KeyAlt && IsMouseLeftPress())
    {
        m_cameraYaw -= static_cast<float>(mouseDelta.x) * 0.010f;
        m_cameraPitch -= static_cast<float>(mouseDelta.y) * 0.010f;
    }

    // ピッチが上下にひっくり返らないように制限します。
    m_cameraPitch = ClampValue(m_cameraPitch, kMinPitch, kMaxPitch);

    // 中ボタンドラッグではカメラの右方向と上方向へパンします。
    if (allowMouseCamera && IsMouseMiddlePress())
    {
        const XMVECTOR eye = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        const XMVECTOR target = XMVectorSet(
            std::cos(m_cameraPitch) * std::cos(m_cameraYaw),
            std::sin(m_cameraPitch),
            std::cos(m_cameraPitch) * std::sin(m_cameraYaw),
            0.0f);
        const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, target - eye));
        XMVECTOR cameraUp = XMVector3Normalize(XMVector3Cross(target - eye, right));

        XMFLOAT3 right3 = {};
        XMFLOAT3 up3 = {};
        XMStoreFloat3(&right3, right);
        XMStoreFloat3(&up3, cameraUp);

        // パン速度はズーム距離に比例させて近距離でも遠距離でも扱いやすくします。
        const float panScale = std::max(0.05f, m_cameraDistance * 0.0025f);
        const float horizontalDelta = static_cast<float>(mouseDelta.x);
        const float verticalDelta = static_cast<float>(mouseDelta.y);

        // 左右パンはドラッグ方向と同じ向きへ注視点が動くように合わせます。
        m_cameraTarget.x += right3.x * horizontalDelta * panScale;
        m_cameraTarget.y += right3.y * horizontalDelta * panScale;
        m_cameraTarget.z += right3.z * horizontalDelta * panScale;

        // 上下パンは従来どおり画面上下に追従させます。
        m_cameraTarget.x -= up3.x * verticalDelta * panScale;
        m_cameraTarget.y += up3.y * verticalDelta * panScale;
        m_cameraTarget.z -= up3.z * verticalDelta * panScale;
    }

    // ホイールは注視点距離のズームに使います。
    if (allowMouseCamera && io.MouseWheel != 0.0f)
    {
        m_cameraDistance -= io.MouseWheel * std::max(1.0f, m_cameraDistance * 0.10f);
        m_cameraDistance = ClampValue(m_cameraDistance, kMinDistance, kMaxDistance);
    }

    // 右ドラッグ中は WASD で平面移動のフライカメラ風操作を行います。
    if (allowMouseCamera && allowKeyboardCamera && IsMouseRightPress())
    {
        const float moveSpeed = std::max(4.0f, m_cameraDistance * 0.35f) * dt;
        const float forwardX = std::cos(m_cameraYaw);
        const float forwardZ = std::sin(m_cameraYaw);
        const float rightX = -forwardZ;
        const float rightZ = forwardX;

        // WASD に応じて注視点を平行移動します。
        if (IsRawKeyPress('W'))
        {
            m_cameraTarget.x -= forwardX * moveSpeed;
            m_cameraTarget.z -= forwardZ * moveSpeed;
        }
        if (IsRawKeyPress('S'))
        {
            m_cameraTarget.x += forwardX * moveSpeed;
            m_cameraTarget.z += forwardZ * moveSpeed;
        }
        if (IsRawKeyPress('D'))
        {
            m_cameraTarget.x += rightX * moveSpeed;
            m_cameraTarget.z += rightZ * moveSpeed;
        }
        if (IsRawKeyPress('A'))
        {
            m_cameraTarget.x -= rightX * moveSpeed;
            m_cameraTarget.z -= rightZ * moveSpeed;
        }
        if (IsRawKeyPress('E'))
        {
            m_cameraTarget.y += moveSpeed;
        }
        if (IsRawKeyPress('Q'))
        {
            m_cameraTarget.y -= moveSpeed;
        }
    }
}

void SceneNarakuEditor::UpdateSelectionAndEditing()
{
    // 常に選択インデックスの破綻を直してから入力を扱います。
    EnsureSelectionValid();

    // ImGui がマウスを使っている間は 3D 空間側の選択を止めます。
    if (ImGui::GetIO().WantCaptureMouse)
    {
        m_draggingVertexHeight = false;
        return;
    }

    // マウス位置は各ピッキングで共通利用します。
    const POINT mousePos = GetMousePosition();
    const POINT mouseDelta = GetMouseDelta();

    // Terrain モードではまず選択頂点の高さ Gizmo ドラッグを処理します。
    if (m_editMode == EditMode::Terrain && m_selectedVertex.layerIndex >= 0)
    {
        if (m_draggingVertexHeight && IsMouseLeftPress())
        {
            // ドラッグ中はマウス縦移動を Y 高さ変化へ変換します。
            NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
            const float currentHeight = NarakuMap::GetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
            NarakuMap::SetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ, currentHeight - static_cast<float>(mouseDelta.y) * kVertexDragScale);
            return;
        }

        // ボタンを離したら Gizmo ドラッグを終了します。
        if (!IsMouseLeftPress())
        {
            m_draggingVertexHeight = false;
        }
    }

    // 左クリック開始時に現在のモードに応じた選択を更新します。
    if (!IsMouseLeftTrigger())
    {
        return;
    }

    // Alt+左ドラッグは Orbit 用なのでピッキングを行いません。
    if (ImGui::GetIO().KeyAlt)
    {
        return;
    }

    // Terrain モードでは、まず既選択頂点のハンドルを掴めるか判定します。
    if (m_editMode == EditMode::Terrain && IsMouseNearSelectedVertexHandle(mousePos))
    {
        m_draggingVertexHeight = true;
        return;
    }

    // Terrain モードでは、選択中レイヤー上の最寄り頂点を拾います。
    if (m_editMode == EditMode::Terrain)
    {
        int gridX = -1;
        int gridZ = -1;
        if (PickTerrainVertex(mousePos, gridX, gridZ))
        {
            m_selectedVertex.layerIndex = m_selectedLayer;
            m_selectedVertex.gridX = gridX;
            m_selectedVertex.gridZ = gridZ;
            FocusSelection();
        }
        return;
    }

    // Rope モードでは最寄りロープを拾います。
    if (m_editMode == EditMode::Rope)
    {
        m_selectedRope = PickRope(mousePos);
        FocusSelection();
        return;
    }

    // Mining モードでは最寄り採掘ポイントを拾います。
    if (m_editMode == EditMode::MiningPoint)
    {
        m_selectedMiningPoint = PickMiningPoint(mousePos);
        FocusSelection();
    }
}

void SceneNarakuEditor::FocusSelection()
{
    // Terrain モードでは選択頂点へ注視点を合わせます。
    if (m_editMode == EditMode::Terrain &&
        m_selectedVertex.layerIndex >= 0 &&
        m_selectedVertex.layerIndex < ToInt(m_mapData.terrainLayers.size()))
    {
        const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
        const XMFLOAT3 world = GetVertexWorldPosition(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
        m_cameraTarget = world;
        return;
    }

    // Rope モードではロープ中央へ注視点を合わせます。
    if (m_editMode == EditMode::Rope &&
        m_selectedRope >= 0 &&
        m_selectedRope < ToInt(m_mapData.ropes.size()))
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[m_selectedRope];
        const int topIndex = FindLayerIndexById(rope.topLayerId);
        const int bottomIndex = FindLayerIndexById(rope.bottomLayerId);
        const float topDepth = (topIndex >= 0) ? m_mapData.terrainLayers[topIndex].layerDepth : 0.0f;
        const float bottomDepth = (bottomIndex >= 0) ? m_mapData.terrainLayers[bottomIndex].layerDepth : topDepth;
        m_cameraTarget = XMFLOAT3(rope.xz.x, ToWorldY((topDepth + bottomDepth) * 0.5f, 0.8f), rope.xz.z);
        return;
    }

    // Mining モードでは採掘ポイントへ注視点を合わせます。
    if (m_editMode == EditMode::MiningPoint &&
        m_selectedMiningPoint >= 0 &&
        m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[m_selectedMiningPoint];
        const int layerIndex = FindLayerIndexById(point.layerId);
        const float depth = (layerIndex >= 0) ? m_mapData.terrainLayers[layerIndex].layerDepth : 0.0f;
        m_cameraTarget = XMFLOAT3(point.xz.x, ToWorldY(depth, 0.5f), point.xz.z);
        return;
    }

    // 何も選択されていない場合は全体中心へ戻します。
    m_cameraTarget = { 0.0f, 0.0f, 0.0f };
}

void SceneNarakuEditor::SaveCurrentMap()
{
    // JSON 保存結果を UI へ返すためにメッセージバッファを用意します。
    std::string errorMessage;

    // 保存が成功したかどうかで表示文言を切り替えます。
    if (NarakuMap::SaveMap(NarakuMap::GetDefaultMapPath(), m_mapData, &errorMessage))
    {
        m_lastIoMessage = u8"保存しました";
    }
    else
    {
        m_lastIoMessage = u8"保存失敗: " + errorMessage;
    }
}

void SceneNarakuEditor::LoadCurrentMap()
{
    // 読み込み失敗時も内容を UI へ返せるようにします。
    std::string errorMessage;

    // 保存済みファイルが読めたらそれを採用します。
    if (NarakuMap::LoadMap(NarakuMap::GetDefaultMapPath(), m_mapData, &errorMessage))
    {
        m_lastIoMessage = u8"読み込みました";
        EnsureSelectionValid();
        return;
    }

    // 初回起動などでファイルがない場合は既定マップを生成して保存します。
    m_mapData = NarakuMap::CreateDefaultMap();
    for (NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
    {
        NarakuMap::EnsureLayerHeights(layer);
    }

    // 既定マップの保存可否もメッセージへ反映します。
    std::string saveError;
    if (NarakuMap::SaveMap(NarakuMap::GetDefaultMapPath(), m_mapData, &saveError))
    {
        m_lastIoMessage = u8"既定マップを作成しました";
    }
    else
    {
        m_lastIoMessage = u8"既定マップ生成: " + errorMessage + u8" / 保存失敗: " + saveError;
    }
}

void SceneNarakuEditor::EnsureSelectionValid()
{
    // レイヤー配列が空なら選択もすべて解除します。
    if (m_mapData.terrainLayers.empty())
    {
        m_selectedLayer = -1;
        m_selectedRope = -1;
        m_selectedMiningPoint = -1;
        m_selectedVertex = {};
        return;
    }

    // 各レイヤーの高さ配列が壊れていても編集前に補正します。
    for (NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
    {
        NarakuMap::EnsureLayerHeights(layer);
    }

    // 現在選択レイヤーを有効範囲へ丸めます。
    m_selectedLayer = ClampValue(m_selectedLayer, 0, ToInt(m_mapData.terrainLayers.size()) - 1);

    // ロープ選択も配列範囲へ直します。
    if (m_mapData.ropes.empty())
    {
        m_selectedRope = -1;
    }
    else
    {
        m_selectedRope = ClampValue(m_selectedRope, -1, ToInt(m_mapData.ropes.size()) - 1);
    }

    // 採掘ポイント選択も配列範囲へ直します。
    if (m_mapData.miningPoints.empty())
    {
        m_selectedMiningPoint = -1;
    }
    else
    {
        m_selectedMiningPoint = ClampValue(m_selectedMiningPoint, -1, ToInt(m_mapData.miningPoints.size()) - 1);
    }

    // 頂点選択が別レイヤーや範囲外を指していたら現在レイヤーの左上へ寄せます。
    NarakuMap::TerrainLayer& currentLayer = m_mapData.terrainLayers[m_selectedLayer];
    if (m_selectedVertex.layerIndex < 0 ||
        m_selectedVertex.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        m_selectedVertex.layerIndex = m_selectedLayer;
        m_selectedVertex.gridX = 0;
        m_selectedVertex.gridZ = 0;
    }

    if (m_selectedVertex.layerIndex == m_selectedLayer)
    {
        m_selectedVertex.gridX = ClampValue(m_selectedVertex.gridX, 0, std::max(0, currentLayer.gridWidth - 1));
        m_selectedVertex.gridZ = ClampValue(m_selectedVertex.gridZ, 0, std::max(0, currentLayer.gridHeight - 1));
    }
}

void SceneNarakuEditor::ApplyCameraMatrices()
{
    // 球面座標からカメラ位置を計算します。
    const float cosPitch = std::cos(m_cameraPitch);
    const XMFLOAT3 eyePos =
    {
        m_cameraTarget.x + std::cos(m_cameraYaw) * cosPitch * m_cameraDistance,
        m_cameraTarget.y + std::sin(m_cameraPitch) * m_cameraDistance,
        m_cameraTarget.z + std::sin(m_cameraYaw) * cosPitch * m_cameraDistance
    };

    // 注視点と上方向からビュー行列を作ります。
    const XMVECTOR eye = XMVectorSet(eyePos.x, eyePos.y, eyePos.z, 1.0f);
    const XMVECTOR target = XMVectorSet(m_cameraTarget.x, m_cameraTarget.y, m_cameraTarget.z, 1.0f);
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    const XMMATRIX viewMatrix = XMMatrixLookAtLH(eye, target, up);

    // 画面比率に合わせた遠近投影行列を作ります。
    const float aspect = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
    const XMMATRIX projectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), aspect, 0.1f, 1000.0f);

    // ピッキング用に非転置版も保持します。
    XMStoreFloat4x4(&m_viewMatrix, viewMatrix);
    XMStoreFloat4x4(&m_projectionMatrix, projectionMatrix);

    // 既存描画クラスの仕様に合わせて転置した行列を渡します。
    XMFLOAT4X4 drawView = {};
    XMFLOAT4X4 drawProjection = {};
    XMStoreFloat4x4(&drawView, XMMatrixTranspose(viewMatrix));
    XMStoreFloat4x4(&drawProjection, XMMatrixTranspose(projectionMatrix));
    Geometory::SetView(drawView);
    Geometory::SetProjection(drawProjection);
    Sprite::SetView(drawView);
    Sprite::SetProjection(drawProjection);
}

void SceneNarakuEditor::DrawWorld()
{
    // 地形レイヤーは半透明床とプレビューグリッドを描画します。
    for (int i = 0; i < ToInt(m_mapData.terrainLayers.size()); ++i)
    {
        DrawTerrainLayer(m_mapData.terrainLayers[i], i);
    }

    // カーソル直下の頂点は選択状態とは別に強調します。
    DrawHoveredVertexHighlight();

    // ロープを立体的に描画します。
    DrawRopes();

    // 採掘ポイントを立体的に描画します。
    DrawMiningPoints();

    // Terrain モード時だけ頂点編集用 Gizmo を表示します。
    if (m_editMode == EditMode::Terrain)
    {
        DrawVertexGizmo();
    }

    // ワイヤー描画は最後にまとめてフラッシュします。
    Geometory::DrawLines();
}

void SceneNarakuEditor::DrawEditorUi()
{
    // 編集モード切り替えとファイル IO をまとめたウィンドウを描画します。
    DrawMapWindow();

    // レイヤー編集ウィンドウを描画します。
    DrawLayerWindow();

    // ロープ編集ウィンドウを描画します。
    DrawRopeWindow();

    // 採掘ポイント編集ウィンドウを描画します。
    DrawMiningWindow();

    // 操作説明を別ウィンドウで出します。
    DrawHelpWindow();
}

void SceneNarakuEditor::DrawMapWindow()
{
    // 画面左上へ固定で配置します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"奈落塔 地形エディタ");

    // 現在の保存先を明示して、どの JSON を触っているか分かるようにします。
    ImGui::Text(u8"保存先");
    const std::string pathText = WideToUtf8(NarakuMap::GetDefaultMapPath());
    ImGui::TextWrapped("%s", pathText.c_str());

    // 保存と読み込みは明示ボタンでのみ行います。
    if (ImGui::Button(u8"保存", ImVec2(96.0f, 0.0f)))
    {
        SaveCurrentMap();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"読み込み", ImVec2(96.0f, 0.0f)))
    {
        LoadCurrentMap();
        FocusSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"既定マップに戻す", ImVec2(130.0f, 0.0f)))
    {
        m_mapData = NarakuMap::CreateDefaultMap();
        EnsureSelectionValid();
        m_lastIoMessage = u8"既定マップへ戻しました";
        FocusSelection();
    }

    // 直近のファイル IO 結果を表示します。
    ImGui::Separator();
    ImGui::Text(u8"状態: %s", m_lastIoMessage.c_str());

    // 編集対象モードはタブ代わりにラジオボタンで切り替えます。
    ImGui::Separator();
    ImGui::Text(u8"編集モード");
    int modeValue = static_cast<int>(m_editMode);
    ImGui::RadioButton(u8"地形", &modeValue, static_cast<int>(EditMode::Terrain));
    ImGui::SameLine();
    ImGui::RadioButton(u8"ロープ", &modeValue, static_cast<int>(EditMode::Rope));
    ImGui::SameLine();
    ImGui::RadioButton(u8"採掘ポイント", &modeValue, static_cast<int>(EditMode::MiningPoint));
    m_editMode = static_cast<EditMode>(modeValue);

    // 手前地形の透過度はここでまとめて調整します。
    ImGui::Separator();
    ImGui::SliderFloat(u8"手前地形アルファ", &m_frontLayerAlpha, 0.05f, 1.0f, "%.2f");

    // 本編シーンへ戻る導線も残しておきます。
    if (ImGui::Button(u8"プロトシーンへ切替", ImVec2(180.0f, 0.0f)))
    {
        SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_PROTO);
    }

    ImGui::End();
}

void SceneNarakuEditor::DrawLayerWindow()
{
    // レイヤー編集は左中段へまとめます。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 252.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 420.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"レイヤー");

    // 新規レイヤー追加と削除をここで行います。
    if (ImGui::Button(u8"レイヤー追加", ImVec2(110.0f, 0.0f)))
    {
        m_mapData.terrainLayers.push_back(CreateNewLayer());
        m_selectedLayer = ToInt(m_mapData.terrainLayers.size()) - 1;
        m_selectedVertex = { m_selectedLayer, 0, 0 };
        FocusSelection();
    }

    ImGui::SameLine();
    if (ImGui::Button(u8"選択レイヤー削除", ImVec2(150.0f, 0.0f)) && m_mapData.terrainLayers.size() > 1)
    {
        const int removeLayerId = m_mapData.terrainLayers[m_selectedLayer].id;

        // 参照先が消えるロープと採掘ポイントは残さず消します。
        m_mapData.ropes.erase(
            std::remove_if(
                m_mapData.ropes.begin(),
                m_mapData.ropes.end(),
                [removeLayerId](const NarakuMap::RopePoint& rope)
                {
                    return rope.topLayerId == removeLayerId || rope.bottomLayerId == removeLayerId;
                }),
            m_mapData.ropes.end());

        m_mapData.miningPoints.erase(
            std::remove_if(
                m_mapData.miningPoints.begin(),
                m_mapData.miningPoints.end(),
                [removeLayerId](const NarakuMap::MiningPoint& point)
                {
                    return point.layerId == removeLayerId;
                }),
            m_mapData.miningPoints.end());

        m_mapData.terrainLayers.erase(m_mapData.terrainLayers.begin() + m_selectedLayer);
        EnsureSelectionValid();
        FocusSelection();
    }

    // レイヤー一覧を選択できるようにします。
    ImGui::Separator();
    for (int i = 0; i < ToInt(m_mapData.terrainLayers.size()); ++i)
    {
        const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[i];
        char label[128] = {};
        std::snprintf(label, sizeof(label), u8"Layer %d (depth %.2f)", layer.id, layer.layerDepth);
        if (ImGui::Selectable(label, m_selectedLayer == i))
        {
            m_selectedLayer = i;
            m_selectedVertex.layerIndex = i;
            FocusSelection();
        }
    }

    // レイヤー選択が無効なら詳細表示は行いません。
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        ImGui::End();
        return;
    }

    // 選択レイヤーの数値編集です。
    ImGui::Separator();
    NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    ImGui::InputInt(u8"ID", &layer.id);
    ImGui::InputFloat2(u8"中心XZ", &layer.center.x, "%.2f");
    ImGui::InputFloat(u8"層深度", &layer.layerDepth, 0.1f, 1.0f, "%.2f");
    ImGui::InputInt(u8"Grid Width", &layer.gridWidth);
    ImGui::InputInt(u8"Grid Height", &layer.gridHeight);
    ImGui::InputFloat(u8"Cell Size", &layer.cellSize, 0.1f, 1.0f, "%.2f");

    // グリッドサイズやセルサイズは最低値を持たせます。
    layer.gridWidth = std::max(layer.gridWidth, 2);
    layer.gridHeight = std::max(layer.gridHeight, 2);
    layer.cellSize = std::max(layer.cellSize, 0.25f);
    NarakuMap::EnsureLayerHeights(layer);

    // レイヤー単位の地面テクスチャを選択します。
    int textureId = ClampValue(layer.groundTextureId, 0, 3);
    if (ImGui::BeginCombo(u8"地面テクスチャ", GetGroundTextureLabel(textureId)))
    {
        for (int i = 0; i < 4; ++i)
        {
            const bool selected = (textureId == i);
            if (ImGui::Selectable(GetGroundTextureLabel(i), selected))
            {
                textureId = i;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    layer.groundTextureId = textureId;

    // 選択中頂点の情報と数値直入力欄を出します。
    ImGui::Separator();
    ImGui::Text(u8"選択頂点: (%d, %d)", m_selectedVertex.gridX, m_selectedVertex.gridZ);
    float vertexHeight = NarakuMap::GetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
    if (ImGui::InputFloat(u8"頂点高さY", &vertexHeight, 0.05f, 0.25f, "%.2f"))
    {
        NarakuMap::SetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ, vertexHeight);
    }

    if (ImGui::Button(u8"頂点を0に戻す", ImVec2(110.0f, 0.0f)))
    {
        NarakuMap::SetVertexHeight(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ, 0.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"全頂点を0に戻す", ImVec2(130.0f, 0.0f)))
    {
        std::fill(layer.heights.begin(), layer.heights.end(), 0.0f);
    }

    ImGui::End();
}

void SceneNarakuEditor::DrawRopeWindow()
{
    // ロープ編集は右上へ配置します。
    ImGui::SetNextWindowPos(ImVec2(900.0f, 20.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"ロープ");

    // ロープの追加と削除を行います。
    if (ImGui::Button(u8"ロープ追加", ImVec2(100.0f, 0.0f)))
    {
        m_mapData.ropes.push_back(CreateNewRope());
        m_selectedRope = ToInt(m_mapData.ropes.size()) - 1;
        m_editMode = EditMode::Rope;
        FocusSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"選択ロープ削除", ImVec2(130.0f, 0.0f)) &&
        m_selectedRope >= 0 &&
        m_selectedRope < ToInt(m_mapData.ropes.size()))
    {
        m_mapData.ropes.erase(m_mapData.ropes.begin() + m_selectedRope);
        EnsureSelectionValid();
        FocusSelection();
    }

    // ロープ一覧を表示して選択できます。
    ImGui::Separator();
    for (int i = 0; i < ToInt(m_mapData.ropes.size()); ++i)
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[i];
        char label[128] = {};
        std::snprintf(label, sizeof(label), u8"Rope %d (Top %d / Bottom %d)", i, rope.topLayerId, rope.bottomLayerId);
        if (ImGui::Selectable(label, m_selectedRope == i))
        {
            m_selectedRope = i;
            m_editMode = EditMode::Rope;
            FocusSelection();
        }
    }

    // 未選択なら詳細編集は出しません。
    if (m_selectedRope < 0 || m_selectedRope >= ToInt(m_mapData.ropes.size()))
    {
        ImGui::End();
        return;
    }

    // 選択ロープの数値編集です。
    ImGui::Separator();
    NarakuMap::RopePoint& rope = m_mapData.ropes[m_selectedRope];
    ImGui::InputFloat2(u8"位置XZ", &rope.xz.x, "%.2f");

    // 接続先レイヤーは ID コンボで選びます。
    if (ImGui::BeginCombo(u8"上層レイヤー", std::to_string(rope.topLayerId).c_str()))
    {
        for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
        {
            const bool selected = (rope.topLayerId == layer.id);
            const std::string label = "Layer " + std::to_string(layer.id);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                rope.topLayerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    if (ImGui::BeginCombo(u8"下層レイヤー", std::to_string(rope.bottomLayerId).c_str()))
    {
        for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
        {
            const bool selected = (rope.bottomLayerId == layer.id);
            const std::string label = "Layer " + std::to_string(layer.id);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                rope.bottomLayerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void SceneNarakuEditor::DrawMiningWindow()
{
    // 採掘ポイント編集は右中段へ配置します。
    ImGui::SetNextWindowPos(ImVec2(900.0f, 352.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 320.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"採掘ポイント");

    // 追加と削除をまとめます。
    if (ImGui::Button(u8"採掘ポイント追加", ImVec2(130.0f, 0.0f)))
    {
        m_mapData.miningPoints.push_back(CreateNewMiningPoint());
        m_selectedMiningPoint = ToInt(m_mapData.miningPoints.size()) - 1;
        m_editMode = EditMode::MiningPoint;
        FocusSelection();
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"選択ポイント削除", ImVec2(130.0f, 0.0f)) &&
        m_selectedMiningPoint >= 0 &&
        m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
    {
        m_mapData.miningPoints.erase(m_mapData.miningPoints.begin() + m_selectedMiningPoint);
        EnsureSelectionValid();
        FocusSelection();
    }

    // 一覧を出して選択可能にします。
    ImGui::Separator();
    for (int i = 0; i < ToInt(m_mapData.miningPoints.size()); ++i)
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[i];
        char label[128] = {};
        std::snprintf(label, sizeof(label), u8"Point %d (Layer %d / Type %d)", i, point.layerId, point.visualType);
        if (ImGui::Selectable(label, m_selectedMiningPoint == i))
        {
            m_selectedMiningPoint = i;
            m_editMode = EditMode::MiningPoint;
            FocusSelection();
        }
    }

    // 未選択時は詳細編集を描きません。
    if (m_selectedMiningPoint < 0 || m_selectedMiningPoint >= ToInt(m_mapData.miningPoints.size()))
    {
        ImGui::End();
        return;
    }

    // 採掘ポイントの指定 3 項目と位置を編集します。
    ImGui::Separator();
    NarakuMap::MiningPoint& point = m_mapData.miningPoints[m_selectedMiningPoint];
    ImGui::InputFloat2(u8"位置XZ", &point.xz.x, "%.2f");
    ImGui::InputInt(u8"見た目種別", &point.visualType);
    point.visualType = ClampValue(point.visualType, 0, 3);
    ImGui::Checkbox(u8"初期発見済み", &point.discovered);

    if (ImGui::BeginCombo(u8"所属レイヤー", std::to_string(point.layerId).c_str()))
    {
        for (const NarakuMap::TerrainLayer& layer : m_mapData.terrainLayers)
        {
            const bool selected = (point.layerId == layer.id);
            const std::string label = "Layer " + std::to_string(layer.id);
            if (ImGui::Selectable(label.c_str(), selected))
            {
                point.layerId = layer.id;
            }
            if (selected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::End();
}

void SceneNarakuEditor::DrawHelpWindow()
{
    // 操作説明は下段中央に出します。
    ImGui::SetNextWindowPos(ImVec2(390.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(500.0f, 140.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin(u8"操作");
    ImGui::Text(u8"Alt + 左ドラッグ: Orbit");
    ImGui::Text(u8"中ドラッグ: Pan");
    ImGui::Text(u8"ホイール: Zoom");
    ImGui::Text(u8"右ドラッグ + WASD / Q / E: Fly");
    ImGui::Text(u8"左クリック: 選択");
    ImGui::Text(u8"Terrain モードで選択頂点を再クリック: Y Gizmo ドラッグ");
    ImGui::Text(u8"F: 選択対象へフォーカス");
    ImGui::End();
}

void SceneNarakuEditor::DrawVertexGizmo()
{
    // レイヤーや頂点が未選択なら Gizmo は出しません。
    if (m_selectedVertex.layerIndex < 0 ||
        m_selectedVertex.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    // 選択頂点位置を取得します。
    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
    const XMFLOAT3 base = GetVertexWorldPosition(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);

    // Y 編集用の縦線を引きます。
    const XMFLOAT3 top = { base.x, base.y + 2.0f, base.z };
    Geometory::AddLine(base, top, { 0.2f, 0.95f, 0.35f, 1.0f });

    // 選択頂点そのものも分かりやすいように暖色で出します。
    DrawDebugBox3D(base, { 0.14f, 0.14f, 0.14f });

    // ハンドル先端に小さい箱を置いて、掴み位置を視認しやすくします。
    DrawDebugBox3D(top, { 0.20f, 0.20f, 0.20f });
}

void SceneNarakuEditor::DrawTerrainLayer(const NarakuMap::TerrainLayer& layer, int layerIndex)
{
    // 地面テクスチャ ID に対応する色を取得します。
    XMFLOAT4 floorColor = GetGroundTextureTint(layer.groundTextureId);

    // 手前側透過ルールを反映してアルファを決定します。
    floorColor.w = GetLayerAlpha(layer);

    // レイヤー全体の床板を半透明で描きます。
    DrawTransparentFloor3D(GetLayerFloorCenter(layer), GetLayerFloorSize(layer), floorColor);

    // レイヤー選択中なら少し明るい色にします。
    const XMFLOAT4 wireColor = (layerIndex == m_selectedLayer)
        ? XMFLOAT4(0.90f, 0.95f, 1.00f, 1.0f)
        : XMFLOAT4(0.55f, 0.58f, 0.65f, 1.0f);
    const float kGridLineLift = 0.03f;

    // Z 方向の横ラインを全グリッド分描きます。
    for (int gridZ = 0; gridZ < layer.gridHeight; ++gridZ)
    {
        for (int gridX = 0; gridX < layer.gridWidth - 1; ++gridX)
        {
            XMFLOAT3 a = GetVertexWorldPosition(layer, gridX, gridZ);
            XMFLOAT3 b = GetVertexWorldPosition(layer, gridX + 1, gridZ);
            a.y += kGridLineLift;
            b.y += kGridLineLift;
            Geometory::AddLine(a, b, wireColor);
        }
    }

    // X 方向の縦ラインも全グリッド分描きます。
    for (int gridX = 0; gridX < layer.gridWidth; ++gridX)
    {
        for (int gridZ = 0; gridZ < layer.gridHeight - 1; ++gridZ)
        {
            XMFLOAT3 a = GetVertexWorldPosition(layer, gridX, gridZ);
            XMFLOAT3 b = GetVertexWorldPosition(layer, gridX, gridZ + 1);
            a.y += kGridLineLift;
            b.y += kGridLineLift;
            Geometory::AddLine(a, b, wireColor);
        }
    }

    // 選択レイヤーの中心には軽い目印を置きます。
    if (layerIndex == m_selectedLayer)
    {
        const XMFLOAT3 center = GetLayerFloorCenter(layer);
        DrawDebugBox3D({ center.x, center.y + 0.10f, center.z }, { 0.35f, 0.20f, 0.35f });
    }

    // 選択中レイヤーでは、現在選んでいるセルを面と枠で強調表示します。
    if (layerIndex == m_selectedVertex.layerIndex &&
        layer.gridWidth >= 2 &&
        layer.gridHeight >= 2)
    {
        const int cellX = ClampValue(m_selectedVertex.gridX, 0, layer.gridWidth - 2);
        const int cellZ = ClampValue(m_selectedVertex.gridZ, 0, layer.gridHeight - 2);

        const XMFLOAT3 p00 = GetVertexWorldPosition(layer, cellX, cellZ);
        const XMFLOAT3 p10 = GetVertexWorldPosition(layer, cellX + 1, cellZ);
        const XMFLOAT3 p01 = GetVertexWorldPosition(layer, cellX, cellZ + 1);
        const XMFLOAT3 p11 = GetVertexWorldPosition(layer, cellX + 1, cellZ + 1);

        const XMFLOAT3 cellCenter(
            (p00.x + p10.x + p01.x + p11.x) * 0.25f,
            (p00.y + p10.y + p01.y + p11.y) * 0.25f + 0.02f,
            (p00.z + p10.z + p01.z + p11.z) * 0.25f);

        // 起伏はまだ床板ベースなので、セル面の平均高さに薄い面を重ねます。
        DrawTransparentFloor3D(cellCenter,
            XMFLOAT2(layer.cellSize, layer.cellSize),
            XMFLOAT4(1.0f, 0.55f, 0.10f, 0.18f));

        const XMFLOAT4 highlightColor = { 1.0f, 0.60f, 0.12f, 1.0f };
        Geometory::AddLine(p00, p10, highlightColor);
        Geometory::AddLine(p10, p11, highlightColor);
        Geometory::AddLine(p11, p01, highlightColor);
        Geometory::AddLine(p01, p00, highlightColor);
        Geometory::AddLine(p00, p11, { 1.0f, 0.75f, 0.25f, 0.65f });
        Geometory::AddLine(p10, p01, { 1.0f, 0.75f, 0.25f, 0.65f });
    }
}

void SceneNarakuEditor::DrawHoveredVertexHighlight()
{
    // 地形モード以外では頂点ホバー表示は不要です。
    if (m_editMode != EditMode::Terrain)
    {
        return;
    }

    // ImGui 上を操作している間はホバー表示を止めます。
    if (ImGui::GetIO().WantCaptureMouse)
    {
        return;
    }

    // 選択中レイヤーが無効なら何も出しません。
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return;
    }

    // カーソル直下の最寄り頂点を現在レイヤー上から拾います。
    int hoverGridX = -1;
    int hoverGridZ = -1;
    if (!PickTerrainVertex(GetMousePosition(), hoverGridX, hoverGridZ))
    {
        return;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    const XMFLOAT3 hoverPos = GetVertexWorldPosition(layer, hoverGridX, hoverGridZ);

    const bool isSelectedVertex =
        m_selectedVertex.layerIndex == m_selectedLayer &&
        m_selectedVertex.gridX == hoverGridX &&
        m_selectedVertex.gridZ == hoverGridZ;

    // Maya 風に、通常ホバーは緑、選択と重なった時は白寄りで出します。
    const XMFLOAT4 crossColor = isSelectedVertex
        ? XMFLOAT4(1.0f, 1.0f, 0.85f, 1.0f)
        : XMFLOAT4(0.35f, 1.0f, 0.35f, 1.0f);

    const float crossRadius = 0.22f;
    Geometory::AddLine(
        XMFLOAT3(hoverPos.x - crossRadius, hoverPos.y, hoverPos.z),
        XMFLOAT3(hoverPos.x + crossRadius, hoverPos.y, hoverPos.z),
        crossColor);
    Geometory::AddLine(
        XMFLOAT3(hoverPos.x, hoverPos.y, hoverPos.z - crossRadius),
        XMFLOAT3(hoverPos.x, hoverPos.y, hoverPos.z + crossRadius),
        crossColor);
    Geometory::AddLine(
        XMFLOAT3(hoverPos.x, hoverPos.y - crossRadius, hoverPos.z),
        XMFLOAT3(hoverPos.x, hoverPos.y + crossRadius, hoverPos.z),
        crossColor);

    // 点としても見えるように小さいマーカーを重ねます。
    DrawDebugBox3D(
        hoverPos,
        isSelectedVertex ? XMFLOAT3(0.12f, 0.12f, 0.12f) : XMFLOAT3(0.10f, 0.10f, 0.10f));
}

void SceneNarakuEditor::DrawRopes()
{
    // 各ロープを上層レイヤーから下層レイヤーへ縦線で描きます。
    for (int i = 0; i < ToInt(m_mapData.ropes.size()); ++i)
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[i];
        const int topIndex = FindLayerIndexById(rope.topLayerId);
        const int bottomIndex = FindLayerIndexById(rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        // 上端と下端の深度に応じて 3D 座標を決めます。
        const float topDepth = m_mapData.terrainLayers[topIndex].layerDepth;
        const float bottomDepth = m_mapData.terrainLayers[bottomIndex].layerDepth;
        const XMFLOAT3 top = { rope.xz.x, ToWorldY(topDepth, 1.1f), rope.xz.z };
        const XMFLOAT3 bottom = { rope.xz.x, ToWorldY(bottomDepth, 0.1f), rope.xz.z };

        // 選択ロープは少し明るい色で描きます。
        const XMFLOAT4 ropeColor = (i == m_selectedRope)
            ? XMFLOAT4(1.0f, 0.82f, 0.22f, 1.0f)
            : XMFLOAT4(0.92f, 0.64f, 0.18f, 1.0f);

        Geometory::AddLine(top, bottom, ropeColor);
        DrawDebugBox3D(top, { 0.22f, 0.22f, 0.22f });
        DrawDebugBox3D(bottom, { 0.18f, 0.18f, 0.18f });
    }
}

void SceneNarakuEditor::DrawMiningPoints()
{
    // 採掘ポイントは見た目種別に応じてサイズだけ少し変えます。
    for (int i = 0; i < ToInt(m_mapData.miningPoints.size()); ++i)
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[i];
        const int layerIndex = FindLayerIndexById(point.layerId);
        if (layerIndex < 0)
        {
            continue;
        }

        // 所属レイヤーの深度位置へマーカーを置きます。
        const float depth = m_mapData.terrainLayers[layerIndex].layerDepth;
        const float width = 0.35f + 0.08f * static_cast<float>(ClampValue(point.visualType, 0, 3));
        const XMFLOAT3 base = { point.xz.x, ToWorldY(depth, 0.15f), point.xz.z };

        // 初期発見済みフラグで色も変えます。
        XMFLOAT4 color = point.discovered
            ? XMFLOAT4(0.20f, 0.88f, 0.95f, 1.0f)
            : XMFLOAT4(1.00f, 0.82f, 0.22f, 1.0f);
        if (i == m_selectedMiningPoint)
        {
            color = { 1.00f, 0.36f, 0.22f, 1.0f };
        }

        DrawDebugBox3D({ base.x, base.y + 0.20f, base.z }, { width, 0.40f, width });
        Geometory::AddLine(base, { base.x, base.y + 1.4f, base.z }, color);
    }
}

void SceneNarakuEditor::DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale) const
{
    // 立方体描画用のスケール行列です。
    const XMMATRIX scaling = XMMatrixScaling(scale.x, scale.y, scale.z);

    // 今回は回転を持たないので平行移動だけ適用します。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);
    const XMMATRIX worldMatrix = scaling * translation;

    // 既存 Geometory は転置行列を受け取る前提です。
    XMFLOAT4X4 world = {};
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);
    Geometory::DrawBox();
}

void SceneNarakuEditor::DrawTransparentFloor3D(const DirectX::XMFLOAT3& center, const DirectX::XMFLOAT2& size, const DirectX::XMFLOAT4& color) const
{
    // テクスチャ未生成なら何も描けないので抜けます。
    if (!m_debugWhiteTexture)
    {
        return;
    }

    // Sprite の標準シェーダーへ戻します。
    Sprite::SetVertexShader(nullptr);
    Sprite::SetPixelShader(nullptr);

    // 白テクスチャへ色を掛けて半透明板を作ります。
    Sprite::SetTexture(m_debugWhiteTexture);
    Sprite::SetSize(size);
    Sprite::SetOffset({ 0.0f, 0.0f });
    Sprite::SetUVPos({ 0.0f, 0.0f });
    Sprite::SetUVScale({ 1.0f, 1.0f });
    Sprite::SetColor(color);

    // XY スプライトを XZ 平面へ寝かせてから配置します。
    const XMMATRIX rotation = XMMatrixRotationX(XMConvertToRadians(90.0f));
    const XMMATRIX translation = XMMatrixTranslation(center.x, center.y, center.z);
    const XMMATRIX worldMatrix = rotation * translation;
    XMFLOAT4X4 world = {};
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Sprite::SetWorld(world);
    Sprite::Draw();
}

float SceneNarakuEditor::ToWorldY(float layerDepth, float height) const
{
    // 深度が深いほど下へ行き、頂点高さはその上に加算します。
    return height - layerDepth * kDepthScale;
}

DirectX::XMFLOAT3 SceneNarakuEditor::GetVertexWorldPosition(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const
{
    // グリッドの左上基準をレイヤー中心から計算します。
    const float originX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float originZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);

    // 頂点番号から平面位置を求めます。
    const float x = originX + static_cast<float>(gridX) * layer.cellSize;
    const float z = originZ + static_cast<float>(gridZ) * layer.cellSize;
    const float y = ToWorldY(layer.layerDepth, NarakuMap::GetVertexHeight(layer, gridX, gridZ));
    return { x, y, z };
}

DirectX::XMFLOAT3 SceneNarakuEditor::GetLayerFloorCenter(const NarakuMap::TerrainLayer& layer) const
{
    // レイヤー中心をそのまま床中心に使い、高さは深度位置に合わせます。
    return { layer.center.x, ToWorldY(layer.layerDepth, -0.03f), layer.center.z };
}

DirectX::XMFLOAT2 SceneNarakuEditor::GetLayerFloorSize(const NarakuMap::TerrainLayer& layer) const
{
    // 頂点数から床面の全長を求めます。
    const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
    const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
    return { std::max(width, layer.cellSize), std::max(height, layer.cellSize) };
}

bool SceneNarakuEditor::ProjectWorldToScreen(const DirectX::XMFLOAT3& worldPos, DirectX::XMFLOAT2& outScreen) const
{
    // ワールド位置をクリップ空間へ変換します。
    const XMMATRIX view = XMLoadFloat4x4(&m_viewMatrix);
    const XMMATRIX projection = XMLoadFloat4x4(&m_projectionMatrix);
    XMVECTOR clip = XMVector3TransformCoord(XMVectorSet(worldPos.x, worldPos.y, worldPos.z, 1.0f), view * projection);
    XMFLOAT3 ndc = {};
    XMStoreFloat3(&ndc, clip);

    // カメラ後方や極端に外れた点は拾わないようにします。
    if (ndc.z < 0.0f || ndc.z > 1.0f)
    {
        return false;
    }

    outScreen.x = (ndc.x * 0.5f + 0.5f) * static_cast<float>(SCREEN_WIDTH);
    outScreen.y = (-ndc.y * 0.5f + 0.5f) * static_cast<float>(SCREEN_HEIGHT);
    return true;
}

bool SceneNarakuEditor::PickTerrainVertex(POINT mousePos, int& outGridX, int& outGridZ) const
{
    // 選択レイヤーがなければ拾えません。
    if (m_selectedLayer < 0 || m_selectedLayer >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedLayer];
    float bestDistance = kPickThresholdPx;
    bool found = false;

    // 単頂点編集なので全頂点から最寄りを拾います。
    for (int gridZ = 0; gridZ < layer.gridHeight; ++gridZ)
    {
        for (int gridX = 0; gridX < layer.gridWidth; ++gridX)
        {
            XMFLOAT2 screen = {};
            if (!ProjectWorldToScreen(GetVertexWorldPosition(layer, gridX, gridZ), screen))
            {
                continue;
            }

            const float dx = static_cast<float>(mousePos.x) - screen.x;
            const float dy = static_cast<float>(mousePos.y) - screen.y;
            const float distance = Length2D(dx, dy);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                outGridX = gridX;
                outGridZ = gridZ;
                found = true;
            }
        }
    }

    return found;
}

int SceneNarakuEditor::PickRope(POINT mousePos) const
{
    // 最寄りロープを画面上の距離で選びます。
    float bestDistance = kPickThresholdPx;
    int bestIndex = -1;
    for (int i = 0; i < ToInt(m_mapData.ropes.size()); ++i)
    {
        const NarakuMap::RopePoint& rope = m_mapData.ropes[i];
        const int topIndex = FindLayerIndexById(rope.topLayerId);
        const int bottomIndex = FindLayerIndexById(rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        const float focusDepth = (m_mapData.terrainLayers[topIndex].layerDepth + m_mapData.terrainLayers[bottomIndex].layerDepth) * 0.5f;
        XMFLOAT2 screen = {};
        const XMFLOAT3 focusPos(rope.xz.x, ToWorldY(focusDepth, 0.8f), rope.xz.z);
        if (!ProjectWorldToScreen(focusPos, screen))
        {
            continue;
        }

        const float dx = static_cast<float>(mousePos.x) - screen.x;
        const float dy = static_cast<float>(mousePos.y) - screen.y;
        const float distance = Length2D(dx, dy);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

int SceneNarakuEditor::PickMiningPoint(POINT mousePos) const
{
    // 最寄り採掘ポイントを画面距離で選びます。
    float bestDistance = kPickThresholdPx;
    int bestIndex = -1;
    for (int i = 0; i < ToInt(m_mapData.miningPoints.size()); ++i)
    {
        const NarakuMap::MiningPoint& point = m_mapData.miningPoints[i];
        const int layerIndex = FindLayerIndexById(point.layerId);
        if (layerIndex < 0)
        {
            continue;
        }

        XMFLOAT2 screen = {};
        const XMFLOAT3 focusPos(point.xz.x, ToWorldY(m_mapData.terrainLayers[layerIndex].layerDepth, 0.4f), point.xz.z);
        if (!ProjectWorldToScreen(focusPos, screen))
        {
            continue;
        }

        const float dx = static_cast<float>(mousePos.x) - screen.x;
        const float dy = static_cast<float>(mousePos.y) - screen.y;
        const float distance = Length2D(dx, dy);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

bool SceneNarakuEditor::IsMouseNearSelectedVertexHandle(POINT mousePos) const
{
    // 頂点が未選択ならハンドル判定も行いません。
    if (m_selectedVertex.layerIndex < 0 ||
        m_selectedVertex.layerIndex >= ToInt(m_mapData.terrainLayers.size()))
    {
        return false;
    }

    // ハンドル先端位置を画面投影してマウス距離を測ります。
    const NarakuMap::TerrainLayer& layer = m_mapData.terrainLayers[m_selectedVertex.layerIndex];
    const XMFLOAT3 vertex = GetVertexWorldPosition(layer, m_selectedVertex.gridX, m_selectedVertex.gridZ);
    const XMFLOAT3 handle = { vertex.x, vertex.y + 2.0f, vertex.z };
    XMFLOAT2 screen = {};
    if (!ProjectWorldToScreen(handle, screen))
    {
        return false;
    }

    const float dx = static_cast<float>(mousePos.x) - screen.x;
    const float dy = static_cast<float>(mousePos.y) - screen.y;
    return Length2D(dx, dy) <= kPickThresholdPx;
}

float SceneNarakuEditor::GetFocusDepth() const
{
    // Terrain モードでは現在レイヤーの深度を使います。
    if (m_editMode == EditMode::Terrain &&
        m_selectedLayer >= 0 &&
        m_selectedLayer < ToInt(m_mapData.terrainLayers.size()))
    {
        return m_mapData.terrainLayers[m_selectedLayer].layerDepth;
    }

    // Rope モードでは下層側を注目深度として使います。
    if (m_editMode == EditMode::Rope &&
        m_selectedRope >= 0 &&
        m_selectedRope < ToInt(m_mapData.ropes.size()))
    {
        const int bottomIndex = FindLayerIndexById(m_mapData.ropes[m_selectedRope].bottomLayerId);
        if (bottomIndex >= 0)
        {
            return m_mapData.terrainLayers[bottomIndex].layerDepth;
        }
    }

    // Mining モードでは所属レイヤー深度を使います。
    if (m_editMode == EditMode::MiningPoint &&
        m_selectedMiningPoint >= 0 &&
        m_selectedMiningPoint < ToInt(m_mapData.miningPoints.size()))
    {
        const int layerIndex = FindLayerIndexById(m_mapData.miningPoints[m_selectedMiningPoint].layerId);
        if (layerIndex >= 0)
        {
            return m_mapData.terrainLayers[layerIndex].layerDepth;
        }
    }

    // 基本は最上層深度を返します。
    return m_mapData.terrainLayers.empty() ? 0.0f : m_mapData.terrainLayers.front().layerDepth;
}

float SceneNarakuEditor::GetLayerAlpha(const NarakuMap::TerrainLayer& layer) const
{
    // 注目深度より浅い層は透過し、それ以外は見やすい標準値で描きます。
    return (layer.layerDepth < GetFocusDepth()) ? m_frontLayerAlpha : 0.32f;
}

const char* SceneNarakuEditor::GetGroundTextureLabel(int textureId) const
{
    // 範囲外 ID は草地へ丸めて表示します。
    return kGroundTextureLabels[ClampValue(textureId, 0, 3)];
}

DirectX::XMFLOAT4 SceneNarakuEditor::GetGroundTextureTint(int textureId) const
{
    // テクスチャ未接続の初期段階なので、ID ごとに色だけ変えます。
    switch (ClampValue(textureId, 0, 3))
    {
    case 1: return { 0.44f, 0.31f, 0.18f, 0.32f };
    case 2: return { 0.36f, 0.40f, 0.46f, 0.32f };
    case 3: return { 0.26f, 0.42f, 0.35f, 0.32f };
    default: return { 0.23f, 0.48f, 0.28f, 0.32f };
    }
}

std::string SceneNarakuEditor::WideToUtf8(const wchar_t* text) const
{
    // null 安全のため先頭で弾きます。
    if (!text)
    {
        return {};
    }

    // UTF-8 長を先に取得します。
    const int required = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (required <= 1)
    {
        return {};
    }

    // 末尾 null を除いた長さで文字列を作ります。
    std::string result(static_cast<size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, &result[0], required, nullptr, nullptr);
    result.pop_back();
    return result;
}

int SceneNarakuEditor::FindLayerIndexById(int layerId) const
{
    // ID 一致する最初のレイヤー番号を返します。
    for (int i = 0; i < ToInt(m_mapData.terrainLayers.size()); ++i)
    {
        if (m_mapData.terrainLayers[i].id == layerId)
        {
            return i;
        }
    }
    return -1;
}

NarakuMap::TerrainLayer SceneNarakuEditor::CreateNewLayer() const
{
    // 最終レイヤーより少し深い位置へ新規レイヤーを置きます。
    NarakuMap::TerrainLayer layer = {};
    layer.id = m_mapData.terrainLayers.empty() ? 0 : (m_mapData.terrainLayers.back().id + 1);
    layer.center = { 0.0f, 0.0f };
    layer.layerDepth = m_mapData.terrainLayers.empty() ? 0.0f : (m_mapData.terrainLayers.back().layerDepth + 4.0f);
    layer.gridWidth = 32;
    layer.gridHeight = 32;
    layer.cellSize = 1.0f;
    layer.groundTextureId = 0;
    NarakuMap::EnsureLayerHeights(layer);
    return layer;
}

NarakuMap::RopePoint SceneNarakuEditor::CreateNewRope() const
{
    // 既存レイヤー 2 枚を自動接続する初期ロープです。
    NarakuMap::RopePoint rope = {};
    rope.xz = { 0.0f, 0.0f };
    if (!m_mapData.terrainLayers.empty())
    {
        rope.topLayerId = m_mapData.terrainLayers.front().id;
        rope.bottomLayerId = m_mapData.terrainLayers.back().id;
    }
    return rope;
}

NarakuMap::MiningPoint SceneNarakuEditor::CreateNewMiningPoint() const
{
    // 現在選択レイヤーへ置く新規採掘ポイントです。
    NarakuMap::MiningPoint point = {};
    point.xz = { 0.0f, 0.0f };
    point.layerId = (m_selectedLayer >= 0 && m_selectedLayer < ToInt(m_mapData.terrainLayers.size()))
        ? m_mapData.terrainLayers[m_selectedLayer].id
        : 0;
    point.visualType = 0;
    point.discovered = false;
    return point;
}
