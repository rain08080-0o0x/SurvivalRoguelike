/**
 * @file SceneNarakuProto.Rendering.cpp
 * @brief フィールド、モデル、HUD、およびデバッグ表示を実装します。
 *
 * SceneNarakuProtoImplementation.h の内部定数と乱数状態を共有して実装します。
 */

#include "SceneNarakuProtoImplementation.h"

using namespace SceneNarakuProtoImplementation;

void SceneNarakuProto::DrawScreenFade() const
{
    if (m_screenFadeAlpha <= 0.0f) return;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 maximum(
        viewport->Pos.x + viewport->Size.x,
        viewport->Pos.y + viewport->Size.y);
    const float normalizedAlpha = std::max(0.0f, std::min(1.0f, m_screenFadeAlpha));
    const int alpha = static_cast<int>(std::round(normalizedAlpha * 255.0f));
    ImGui::GetForegroundDrawList(viewport)->AddRectFilled(
        viewport->Pos,
        maximum,
        IM_COL32(0, 0, 0, alpha));
}

void SceneNarakuProto::Draw()
{
    NarakuUi::BeginFrame(static_cast<int>(m_mode));
    if (m_mode == Mode::Loading)
    {
        DrawLoadingScreen();
        DrawScreenFade();
        return;
    }

    const bool fadeActive = m_screenFadePhase != ScreenFadePhase::None;
    if (fadeActive) ImGui::BeginDisabled();

    // デバッグ用3Dフィールドを最初に描画します。
    Draw3DField();

    if (m_mode == Mode::Explore && m_fullness <= kFullnessCritical)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 minimum = viewport->WorkPos;
        const ImVec2 maximum(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
        ImDrawList* effect = ImGui::GetBackgroundDrawList();
        effect->AddRectFilled(minimum, maximum, IM_COL32(90, 28, 8, 26));
        effect->AddRect(minimum, maximum, IM_COL32(180, 55, 18, 120), 0.0f, 0, 18.0f);
    }

    if (m_mode == Mode::Explore || m_mode == Mode::LayerTransition)
    {
        DrawDehydrationVisionEffect();
        DrawUpperLoadVisionEffect();
    }

    DrawCompass();

    // 常時確認するステータスHUDを描画します。
    DrawHud();
    if (m_mode == Mode::Explore) DrawRouteInfo();
    if (m_mode == Mode::Surface) DrawSurfaceFacilityMarkers();
    if (m_mode == Mode::Explore) DrawBaseInteractionMarker();

#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
    // Debug版とEditor歩行確認でのみ調整・位置情報を表示します。
    DrawDebugPlayerTuning();
    DrawPlayerPositionDebug();
#endif
    // モード切り替えやメニュータブ切り替え時にUIウィンドウへ初期フォーカスを要求
    static Mode s_lastModeForFocus = Mode::Explore;
    static MenuTab s_lastMenuTabForFocus = MenuTab::Inventory;
    if (m_mode != s_lastModeForFocus || (m_mode == Mode::Inventory && m_activeMenuTab != s_lastMenuTabForFocus))
    {
        m_menuFocusPending = true;
        s_lastModeForFocus = m_mode;
        s_lastMenuTabForFocus = m_activeMenuTab;
    }

    if (m_menuFocusPending && m_mode != Mode::Explore && m_mode != Mode::Surface && m_mode != Mode::Loading)
    {
        ImGui::SetNextWindowFocus();
        m_menuFocusPending = false;
    }

    // 所持品モードでは所持品と地図ピンUIを重ねます。
    if (m_mode == Mode::Inventory)
    {
        if (m_activeMenuTab == MenuTab::Map) DrawMapControls();
        else if (m_activeMenuTab == MenuTab::Settings)
        {
            ImGui::SetNextWindowPos(ImVec2(160.0f, 80.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(720.0f, 600.0f), ImGuiCond_FirstUseEver);
            if (NarakuUi::Begin(u8"統合メニュー##SettingsWindow", nullptr, ImGuiWindowFlags_NoCollapse))
            {
                if (ImGui::Button(u8"← [LB] 地図", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Map; m_inventoryMapShowingMap = true; }
                ImGui::SameLine();
                if (ImGui::Button(u8"所持品", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Inventory; m_inventoryMapShowingMap = false; }
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), u8"【 設定 】 [RB] →");
                ImGui::SameLine();
                ImGui::TextDisabled(u8" (LB / RB でタブ切替, [B/Esc] 閉じる)");
                ImGui::Separator();
                ImGui::Spacing();

                m_inputSettings.DrawSettingsTab();
            }
            ImGui::End();
        }
        else DrawInventory();
    }

    // 旧器発見中は拾う/置く確認を重ねます。
    else if (m_mode == Mode::RelicPrompt) DrawRelicPrompt();
    else if (m_mode == Mode::WaterPrompt) DrawWaterPrompt();
    else if (m_mode == Mode::FishingConfirm) DrawFishingConfirm();

    else if (m_mode == Mode::ReturnConfirm) DrawReturnConfirm();

    else if (m_mode == Mode::AbandonConfirm) DrawAbandonConfirm();

    else if (m_mode == Mode::UninsuredDescentConfirm) DrawUninsuredDescentConfirm();

    // 帰還または死亡後はリザルトを重ねます。
    else if (m_mode == Mode::ReturnResult || m_mode == Mode::DeathResult) DrawResult();
    else if (m_mode == Mode::SaveError) DrawTransitionSaveError();

    else if (m_mode == Mode::Home) DrawHome();

    else if (m_mode == Mode::GeneralShop) DrawGeneralShop();

    else if (m_mode == Mode::Armory) DrawArmory();

    else if (m_mode == Mode::Restaurant) DrawRestaurant();

    else if (m_mode == Mode::QuestDesk) DrawQuestDesk();

    else if (m_mode == Mode::AbyssEntrance) DrawAbyssEntrance();

    else if (m_mode == Mode::SecondBase) DrawSecondBase();

    else if (m_mode == Mode::ForwardBase) DrawForwardBase();

    DrawCenterNotification();
    DrawGenerationFailurePopup();

    // 採掘（探窟）進行度バーを画面中央にオーバーレイ表示します。
    DrawMiningProgressBar();
    DrawFishingProgress();
    if (fadeActive) ImGui::EndDisabled();
    DrawScreenFade();
}

void SceneNarakuProto::DrawUninsuredDescentConfirm()
{
    ImGui::SetNextWindowPos(ImVec2(390.0f, 220.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 220.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"保険対象外の深度", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"階級適正より下の保険の対象外となり、死亡時に持ち物が保持される保険が適用されません。よろしいですか？");
    ImGui::Separator();
    if (ImGui::Button(u8"進入する", ImVec2(150.0f, 0.0f)))
    {
        const int gateIndex = m_pendingUninsuredGateIndex;
        m_pendingUninsuredGateIndex = -1;
        m_uninsuredDescentAcceptedThisDive = true;
        m_mode = Mode::Explore;
        TryUseLayerGate(gateIndex);
    }
    ImGui::SameLine();
    if (NarakuUi::BackButton(u8"キャンセル", ImVec2(150.0f, 0.0f)))
    {
        m_pendingUninsuredGateIndex = -1;
        m_mode = Mode::Explore;
    }
    ImGui::End();
}

void SceneNarakuProto::DrawLoadingScreen()
{
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
#if !defined(NARAKU_EDITOR_BUILD)
    flags |= ImGuiWindowFlags_NoInputs;
#endif
    if (ImGui::Begin("LayerLoading##NarakuProto", nullptr, flags))
    {
        const ImVec2 windowPos = ImGui::GetWindowPos();
        const ImVec2 windowSize = ImGui::GetWindowSize();
        const char* loadingText = u8"ロード中...";
        const ImVec2 textSize = ImGui::CalcTextSize(loadingText);
        ImGui::SetCursorScreenPos(ImVec2(
            windowPos.x + (windowSize.x - textSize.x) * 0.5f,
            windowPos.y + (windowSize.y - textSize.y) * 0.5f));
        ImGui::TextUnformatted(loadingText);

        if (!m_loadingStatus.empty())
        {
            const ImVec2 statusSize = ImGui::CalcTextSize(m_loadingStatus.c_str());
            ImGui::SetCursorScreenPos(ImVec2(
                windowPos.x + std::max(20.0f, (windowSize.x - statusSize.x) * 0.5f),
                windowPos.y + (windowSize.y - textSize.y) * 0.5f + 34.0f));
            ImGui::TextUnformatted(m_loadingStatus.c_str());
        }

        constexpr float barWidth = 260.0f;
        constexpr float margin = 28.0f;
        ImGui::SetCursorScreenPos(ImVec2(
            windowPos.x + windowSize.x - barWidth - margin,
            windowPos.y + windowSize.y - 24.0f - margin));
        char progressLabel[32] = {};
        std::snprintf(progressLabel, sizeof(progressLabel), "%.0f%%", m_loadingProgress * 100.0f);
        ImGui::ProgressBar(std::max(0.0f, std::min(1.0f, m_loadingProgress)), ImVec2(barWidth, 18.0f), progressLabel);

#if defined(NARAKU_EDITOR_BUILD)
        if (m_editorPreviewGenerationFailed)
        {
            const float panelWidth = std::min(760.0f, windowSize.x - 60.0f);
            ImGui::SetCursorScreenPos(ImVec2(windowPos.x + (windowSize.x - panelWidth) * 0.5f, windowPos.y + 80.0f));
            ImGui::BeginChild("PreviewGenerationFailure", ImVec2(panelWidth, windowSize.y - 180.0f), true);
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), u8"生成に失敗しました");
            ImGui::TextWrapped("%s", m_generationFailureSummary.c_str());
            ImGui::Separator();
            ImGui::TextWrapped("%s", m_generationFailureDetail.c_str());
            ImGui::Spacing();
            ImGui::TextWrapped(u8"詳細ログ: Assets/Logs/naraku_generation_failure.log");
            if (ImGui::Button(u8"Editorへ戻る", ImVec2(160.0f, 0.0f)))
            {
                SceneManager::ChangeScene(SceneManager::SCENE_NARAKU_EDITOR);
            }
            ImGui::EndChild();
        }
#endif
    }
    ImGui::End();
}

void SceneNarakuProto::DrawGenerationFailurePopup()
{
    if (m_openGenerationFailurePopup)
    {
        ImGui::OpenPopup(u8"生成失敗##NarakuProto");
        m_openGenerationFailurePopup = false;
    }
    if (!NarakuUi::BeginPopupModal(u8"生成失敗##NarakuProto", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        return;
    }

    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), "%s", m_generationFailureSummary.c_str());
    ImGui::Separator();
    ImGui::BeginChild("GenerationFailureDetail", ImVec2(700.0f, 280.0f), true);
    ImGui::TextUnformatted(m_generationFailureDetail.c_str());
    ImGui::EndChild();
    ImGui::TextUnformatted(u8"詳細ログ: Assets/Logs/naraku_generation_failure.log");
    if (ImGui::Button(u8"閉じる", ImVec2(120.0f, 0.0f)))
    {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void SceneNarakuProto::UpdateCameraControls()
{
    const float zoomDelta = GetActionCameraZoomDelta();
    if (zoomDelta != 0.0f)
    {
        m_cameraDistance -= zoomDelta;
    }

    float yawDelta = 0.0f;
    float pitchDelta = 0.0f;
    GetActionCameraRotation(yawDelta, pitchDelta);
    m_cameraYaw += yawDelta;
    m_cameraPitch += pitchDelta;

    NormalizeCameraSettings();

    constexpr float twoPi = DirectX::XM_2PI;
    if (m_cameraYaw > twoPi || m_cameraYaw < -twoPi)
    {
        m_cameraYaw = std::fmod(m_cameraYaw, twoPi);
    }
}

void SceneNarakuProto::NormalizeCameraSettings()
{
    m_cameraDistance = std::max(kCameraMinDistance, std::min(m_cameraDistance, kCameraMaxDistance));
    m_cameraMinPitchDegrees = std::max(kCameraMinPitchDegrees, std::min(m_cameraMinPitchDegrees, kCameraMaxPitchDegrees));
    m_cameraMaxPitchDegrees = std::max(kCameraMinPitchDegrees, std::min(m_cameraMaxPitchDegrees, kCameraMaxPitchDegrees));
    m_cameraMinPitchDegrees = std::min(m_cameraMinPitchDegrees, m_cameraMaxPitchDegrees);
    m_cameraMaxPitchDegrees = std::max(m_cameraMaxPitchDegrees, m_cameraMinPitchDegrees);
    const float minPitch = DirectX::XMConvertToRadians(m_cameraMinPitchDegrees);
    const float maxPitch = DirectX::XMConvertToRadians(m_cameraMaxPitchDegrees);
    m_cameraPitch = std::max(minPitch, std::min(m_cameraPitch, maxPitch));
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetCameraForward() const
{
    return Normalize({ -std::sin(m_cameraYaw), -std::cos(m_cameraYaw) });
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetCameraRight() const
{
    const Vec2 forward = GetCameraForward();
    return Normalize({ forward.y, -forward.x });
}

void SceneNarakuProto::ReleaseEnvironmentModels()
{
    for (EnvironmentModelResource& resource : m_environmentModels)
    {
        SAFE_DELETE(resource.model);
    }
    m_environmentModels.clear();
}

void SceneNarakuProto::LoadEnvironmentModels()
{
    ReleaseEnvironmentModels();
    const std::wstring catalogPath = ResolveRuntimeAssetPath(kEnvironmentModelCatalogRelativePath);
    std::ifstream input(catalogPath, std::ios::binary);
    if (!input) return;

    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line[0] == '#') continue;

        std::istringstream row(line);
        std::string id;
        std::string name;
        std::string modelPath;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float scaleZ = 1.0f;
        if (!(row >> std::quoted(id) >> std::quoted(name) >> std::quoted(modelPath)
            >> scaleX >> scaleY >> scaleZ))
        {
            continue;
        }
        int footprintX = 1;
        int footprintZ = 1;
        int colliderEnabled = 0;
        DirectX::XMFLOAT3 colliderCenter = {};
        DirectX::XMFLOAT3 colliderSize = { 1.0f, 1.0f, 1.0f };
        if (!(row >> footprintX >> footprintZ >> colliderEnabled
            >> colliderCenter.x >> colliderCenter.y >> colliderCenter.z
            >> colliderSize.x >> colliderSize.y >> colliderSize.z))
        {
            footprintX = 1;
            footprintZ = 1;
            colliderEnabled = 0;
            colliderCenter = {};
            colliderSize = { 1.0f, 1.0f, 1.0f };
        }

        const std::string resolvedModelPath = WideToUtf8(ResolveRuntimeAssetPath(Utf8ToWide(modelPath)));
        Model* model = new Model();
        if (!model->LoadStatic(resolvedModelPath.c_str(), 1.0f, Model::ZFlip)) SAFE_DELETE(model);

        DirectX::XMFLOAT3 minValue = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max() };
        DirectX::XMFLOAT3 maxValue = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest() };
        bool hasVertex = false;
        for (unsigned int meshIndex = 0; model != nullptr && meshIndex < model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            for (const Model::Vertex& vertex : mesh->vertices)
            {
                minValue.x = std::min(minValue.x, vertex.pos.x);
                minValue.y = std::min(minValue.y, vertex.pos.y);
                minValue.z = std::min(minValue.z, vertex.pos.z);
                maxValue.x = std::max(maxValue.x, vertex.pos.x);
                maxValue.y = std::max(maxValue.y, vertex.pos.y);
                maxValue.z = std::max(maxValue.z, vertex.pos.z);
                hasVertex = true;
            }
        }

        EnvironmentModelResource resource;
        resource.id = id;
        resource.model = model;
        resource.defaultScale = { scaleX, scaleY, scaleZ };
        resource.footprintX = std::max(1, footprintX);
        resource.footprintZ = std::max(1, footprintZ);
        resource.colliderEnabled = colliderEnabled != 0;
        resource.colliderCenter = colliderCenter;
        resource.colliderSize = colliderSize;
        std::string treeSource = name + ' ' + modelPath;
        std::transform(treeSource.begin(), treeSource.end(), treeSource.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        resource.isTree = treeSource.find("tree") != std::string::npos;
        if (model != nullptr && hasVertex)
        {
            resource.placementAnchor = {
                (minValue.x + maxValue.x) * 0.5f,
                minValue.y,
                (minValue.z + maxValue.z) * 0.5f };
            resource.horizontalSize = std::max(
                0.001f,
                std::max(maxValue.x - minValue.x, maxValue.z - minValue.z));
        }
        m_environmentModels.push_back(resource);
    }
}

void SceneNarakuProto::LoadBaseModels()
{
    using namespace DirectX;
    ReleaseBaseModels();
    const char* modelPaths[] =
    {
        "Assets/Base/second_base.fbx",
        "Assets/Base/frontline_base.fbx",
    };

    for (std::size_t index = 0; index < m_baseModels.size(); ++index)
    {
        Model* model = new Model();
        if (!model->Load(modelPaths[index], 1.0f, Model::ZFlip))
        {
            SAFE_DELETE(model);
            model = new Model();
            const std::string resolvedPath = WideToUtf8(ResolveProjectPath(Utf8ToWide(modelPaths[index])));
            if (!model->Load(resolvedPath.c_str(), 1.0f, Model::ZFlip))
            {
                SAFE_DELETE(model);
                continue;
            }
        }

        XMFLOAT3 minimum = {
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max(),
            std::numeric_limits<float>::max() };
        XMFLOAT3 maximum = {
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest(),
            std::numeric_limits<float>::lowest() };
        bool hasVertex = false;
        for (unsigned int meshIndex = 0; meshIndex < model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            for (const Model::Vertex& vertex : mesh->vertices)
            {
                minimum.x = std::min(minimum.x, vertex.pos.x);
                minimum.y = std::min(minimum.y, vertex.pos.y);
                minimum.z = std::min(minimum.z, vertex.pos.z);
                maximum.x = std::max(maximum.x, vertex.pos.x);
                maximum.y = std::max(maximum.y, vertex.pos.y);
                maximum.z = std::max(maximum.z, vertex.pos.z);
                hasVertex = true;
            }
        }
        if (!hasVertex)
        {
            SAFE_DELETE(model);
            continue;
        }

        BaseModelResource& resource = m_baseModels[index];
        resource.model = model;
        resource.placementAnchor = {
            (minimum.x + maximum.x) * 0.5f,
            minimum.y,
            (minimum.z + maximum.z) * 0.5f };
        resource.horizontalSize = std::max(
            0.001f,
            std::max(maximum.x - minimum.x, maximum.z - minimum.z));
    }
}

void SceneNarakuProto::ReleaseBaseModels()
{
    for (BaseModelResource& resource : m_baseModels)
    {
        SAFE_DELETE(resource.model);
        resource.placementAnchor = {};
        resource.horizontalSize = 1.0f;
    }
}

std::vector<SceneNarakuProto::PortableLight>& SceneNarakuProto::GetAccessibleLights()
{
    return (m_mode == Mode::Surface || (m_mode == Mode::Inventory && m_overlayReturnMode == Mode::Surface))
        ? m_storedPortableLights : m_portableLights;
}

const std::vector<SceneNarakuProto::PortableLight>& SceneNarakuProto::GetAccessibleLights() const
{
    return (m_mode == Mode::Surface || (m_mode == Mode::Inventory && m_overlayReturnMode == Mode::Surface))
        ? m_storedPortableLights : m_portableLights;
}

bool SceneNarakuProto::IsPortableLightAvailable() const
{
    const auto& lights = GetAccessibleLights();
    return std::any_of(lights.begin(), lights.end(), [](const PortableLight& light)
    {
        return !light.broken && light.remainingSeconds > 0.0f;
    });
}

void SceneNarakuProto::TogglePortableLight()
{
    if (m_portableLightOn)
    {
        m_portableLightOn = false;
        ShowCenterNotification(u8"ライトを消灯しました。");
        return;
    }
    if (!IsPortableLightAvailable())
    {
        ShowCenterNotification(u8"使用できるライトを所持していません。");
        return;
    }
    m_portableLightOn = true;
    ShowCenterNotification(u8"ライトを点灯しました。");
}

void SceneNarakuProto::UpdatePortableLight(float dt)
{
    if (!m_portableLightOn) return;
    auto& lights = GetAccessibleLights();
    auto current = std::min_element(lights.begin(), lights.end(), [](const PortableLight& left, const PortableLight& right)
    {
        const float leftTime = (!left.broken && left.remainingSeconds > 0.0f) ? left.remainingSeconds : FLT_MAX;
        const float rightTime = (!right.broken && right.remainingSeconds > 0.0f) ? right.remainingSeconds : FLT_MAX;
        return leftTime < rightTime;
    });
    if (current == lights.end() || current->broken || current->remainingSeconds <= 0.0f)
    {
        m_portableLightOn = false;
        return;
    }
    current->remainingSeconds = std::max(0.0f, current->remainingSeconds - dt);
    if (current->remainingSeconds > 0.0f) return;
    current->broken = true;
    if (IsPortableLightAvailable())
        ShowCenterNotification(u8"ライトが破損したため、次のライトへ自動で切り替えました。");
    else
    {
        m_portableLightOn = false;
        ShowCenterNotification(u8"ライトが破損しました。");
    }
}

void SceneNarakuProto::ReturnCarriedLightsToStorage()
{
    m_portableLightOn = false;
    m_storedPortableLights.insert(
        m_storedPortableLights.end(), m_portableLights.begin(), m_portableLights.end());
    m_portableLights.clear();
}

ShaderList::ExtendedLight SceneNarakuProto::BuildSceneLight() const
{
    using namespace DirectX;
    ShaderList::ExtendedLight light;
    const int depth = std::max(1, std::min(5, GetCurrentDepth()));
    const float depthIntensities[] = { 1.0f, 0.10f, 0.85f, 0.60f };
    if (depth <= 4)
    {
        const float intensity = depthIntensities[depth - 1];
        light.directionalColor = { intensity, intensity, intensity, 1.0f };
        light.directionalDirection = { 0.0f, -1.0f, 0.0f, 0.0f };
    }
    else
    {
        const auto base = std::find_if(m_runtimeMap.bases.begin(), m_runtimeMap.bases.end(),
            [](const NarakuMap::BasePoint& point) { return point.type == NarakuMap::BaseType::ForwardBase; });
        if (base != m_runtimeMap.bases.end())
        {
            const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base->layerId);
            const float layerDepth = layerIndex >= 0
                ? m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth
                : m_player.depth;
            const float groundY = GetGroundWorldY({ base->xz.x, base->xz.z }, layerDepth);
            light.directionalColor = {};
            light.pointColorEnabled = { 1.0f, 0.92f, 0.78f, 1.25f };
            light.pointPositionRange = { base->xz.x, groundY + 5.0f, base->xz.z,
                std::max(1.0f, m_worldHalfSize * 3.0f) };
        }
        else
        {
            const float sideHeight = m_player.feetWorldY + 8.0f;
            light.directionalColor = {};
            light.pointColorEnabled = { 1.0f, 1.0f, 1.0f, 1.60f };
            light.pointPositionRange = {
                -m_worldHalfSize * 1.25f,
                sideHeight,
                -m_worldHalfSize * 1.25f,
                std::max(1.0f, m_worldHalfSize * 8.0f) };
        }
    }
    if (m_portableLightOn && IsPortableLightAvailable())
    {
        const float downwardAngle = XMConvertToRadians(5.0f);
        const float horizontalScale = std::cos(downwardAngle);
        const DirectX::XMFLOAT3 cameraDirection = {
            -std::sin(m_cameraYaw) * horizontalScale,
            -std::sin(downwardAngle),
            -std::cos(m_cameraYaw) * horizontalScale };
        light.spotColorEnabled = { 1.0f, 0.88f, 0.70f, 1.50f };
        light.spotPositionRange = { m_player.pos.x, m_player.feetWorldY + 1.5f, m_player.pos.y, 45.0f };
        light.spotDirectionInnerCos = {
            cameraDirection.x,
            cameraDirection.y,
            cameraDirection.z,
            std::cos(XMConvertToRadians(15.0f)) };
        light.spotOuterCos = { std::cos(XMConvertToRadians(30.0f)), 0.0f, 0.0f, 0.0f };
    }
    return light;
}

void SceneNarakuProto::ApplySceneLighting() const
{
    ShaderList::ExtendedLight light = BuildSceneLight();
    ShaderList::SetExtendedLight(light);
}

DirectX::XMFLOAT3 SceneNarakuProto::CalculateSceneLightColor(
    const Vec2& position, float layerDepth, float worldY) const
{
    (void)layerDepth;
    const int depth = std::max(1, std::min(5, GetCurrentDepth()));
    float intensity = depth <= 4 ? std::array<float, 4>{ 1.0f, 0.10f, 0.85f, 0.60f }[depth - 1] : 0.0f;
    DirectX::XMFLOAT3 color = { intensity, intensity, intensity };
    if (depth == 5)
    {
        const auto base = std::find_if(m_runtimeMap.bases.begin(), m_runtimeMap.bases.end(),
            [](const NarakuMap::BasePoint& point) { return point.type == NarakuMap::BaseType::ForwardBase; });
        if (base != m_runtimeMap.bases.end())
        {
            const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base->layerId);
            const float baseDepth = layerIndex >= 0
                ? m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth : m_player.depth;
            const float baseY = GetGroundWorldY({ base->xz.x, base->xz.z }, baseDepth) + 5.0f;
            const float dx = position.x - base->xz.x;
            const float dz = position.y - base->xz.z;
            const float dy = worldY - baseY;
            const float range = std::max(1.0f, m_worldHalfSize * 3.0f);
            const float attenuation = std::max(0.0f, 1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) / range);
            const float point = 1.25f * attenuation * attenuation;
            color = { point, point * 0.92f, point * 0.78f };
        }
        else
        {
            const float lightX = -m_worldHalfSize * 1.25f;
            const float lightY = m_player.feetWorldY + 8.0f;
            const float lightZ = -m_worldHalfSize * 1.25f;
            const float dx = position.x - lightX;
            const float dy = worldY - lightY;
            const float dz = position.y - lightZ;
            const float range = std::max(1.0f, m_worldHalfSize * 8.0f);
            const float attenuation = std::max(0.0f,
                1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) / range);
            const float side = 1.60f * attenuation * attenuation;
            color = { side, side, side };
        }
    }
    if (m_portableLightOn && IsPortableLightAvailable())
    {
        const float downwardAngle = DirectX::XMConvertToRadians(5.0f);
        const float horizontalScale = std::cos(downwardAngle);
        const DirectX::XMFLOAT3 cameraDirection = {
            -std::sin(m_cameraYaw) * horizontalScale,
            -std::sin(downwardAngle),
            -std::cos(m_cameraYaw) * horizontalScale };
        const float offsetX = position.x - m_player.pos.x;
        const float offsetY = worldY - (m_player.feetWorldY + 1.5f);
        const float offsetZ = position.y - m_player.pos.y;
        const float distance = std::sqrt(offsetX * offsetX + offsetY * offsetY + offsetZ * offsetZ);
        if (distance > 0.001f && distance < 45.0f)
        {
            const float inverseDistance = 1.0f / distance;
            const float directionDot =
                offsetX * inverseDistance * cameraDirection.x +
                offsetY * inverseDistance * cameraDirection.y +
                offsetZ * inverseDistance * cameraDirection.z;
            const float cone = (directionDot - std::cos(DirectX::XMConvertToRadians(30.0f))) /
                std::max(0.001f, std::cos(DirectX::XMConvertToRadians(15.0f)) - std::cos(DirectX::XMConvertToRadians(30.0f)));
            const float attenuation = 1.0f - distance / 45.0f;
            const float spot = 1.5f * std::max(0.0f, std::min(1.0f, cone)) * attenuation * attenuation;
            color.x += spot;
            color.y += spot * 0.88f;
            color.z += spot * 0.70f;
        }
    }
    color.x = std::max(0.015f, std::min(1.5f, color.x));
    color.y = std::max(0.015f, std::min(1.5f, color.y));
    color.z = std::max(0.015f, std::min(1.5f, color.z));
    return color;
}

float SceneNarakuProto::CalculateSceneLightIntensity(const Vec2& position, float layerDepth, float worldY) const
{
    const DirectX::XMFLOAT3 color = CalculateSceneLightColor(position, layerDepth, worldY);
    return std::max(color.x, std::max(color.y, color.z));
}

void SceneNarakuProto::LoadRopeModel()
{
    using namespace DirectX;
    ReleaseRopeModel();

    const std::string modelPath = WideToUtf8(ResolveRuntimeAssetPath(kRopeModelRelativePath));
    m_ropeModel = new Model();
    if (!m_ropeModel->LoadStatic(modelPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(m_ropeModel);
        return;
    }

    XMFLOAT3 minimum = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max() };
    XMFLOAT3 maximum = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest() };
    bool hasVertex = false;
    for (unsigned int meshIndex = 0; meshIndex < m_ropeModel->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = m_ropeModel->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        for (const Model::Vertex& vertex : mesh->vertices)
        {
            minimum.x = std::min(minimum.x, vertex.pos.x);
            minimum.y = std::min(minimum.y, vertex.pos.y);
            minimum.z = std::min(minimum.z, vertex.pos.z);
            maximum.x = std::max(maximum.x, vertex.pos.x);
            maximum.y = std::max(maximum.y, vertex.pos.y);
            maximum.z = std::max(maximum.z, vertex.pos.z);
            hasVertex = true;
        }
    }
    if (!hasVertex)
    {
        SAFE_DELETE(m_ropeModel);
        return;
    }

    m_ropeModelCenter = {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f };
    m_ropeModelSize = {
        std::max(0.001f, maximum.x - minimum.x),
        std::max(0.001f, maximum.y - minimum.y),
        std::max(0.001f, maximum.z - minimum.z) };
    m_ropeModelLengthAxis = 0;
    if (m_ropeModelSize.y > m_ropeModelSize.x) m_ropeModelLengthAxis = 1;
    const float longestSize = m_ropeModelLengthAxis == 0 ? m_ropeModelSize.x : m_ropeModelSize.y;
    if (m_ropeModelSize.z > longestSize) m_ropeModelLengthAxis = 2;

    const std::string texturePath = WideToUtf8(ResolveRuntimeAssetPath(kRopeTextureRelativePath));
    m_ropeTexture = new Texture();
    if (FAILED(m_ropeTexture->Create(texturePath.c_str())))
    {
        SAFE_DELETE(m_ropeTexture);
    }

    const std::string supportModelPath = WideToUtf8(ResolveRuntimeAssetPath(kRopeSupportModelRelativePath));
    m_ropeSupportModel = new Model();
    if (!m_ropeSupportModel->LoadStatic(supportModelPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(m_ropeSupportModel);
        return;
    }

    minimum = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max() };
    maximum = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest() };
    hasVertex = false;
    for (unsigned int meshIndex = 0; meshIndex < m_ropeSupportModel->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = m_ropeSupportModel->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        for (const Model::Vertex& vertex : mesh->vertices)
        {
            minimum.x = std::min(minimum.x, vertex.pos.x);
            minimum.y = std::min(minimum.y, vertex.pos.y);
            minimum.z = std::min(minimum.z, vertex.pos.z);
            maximum.x = std::max(maximum.x, vertex.pos.x);
            maximum.y = std::max(maximum.y, vertex.pos.y);
            maximum.z = std::max(maximum.z, vertex.pos.z);
            hasVertex = true;
        }
    }
    if (!hasVertex)
    {
        SAFE_DELETE(m_ropeSupportModel);
        return;
    }

    m_ropeSupportAnchor = {
        (minimum.x + maximum.x) * 0.5f,
        minimum.y,
        (minimum.z + maximum.z) * 0.5f };
    m_ropeSupportSize = {
        std::max(0.001f, maximum.x - minimum.x),
        std::max(0.001f, maximum.y - minimum.y),
        std::max(0.001f, maximum.z - minimum.z) };
    m_ropeSupportForwardAxis = m_ropeSupportSize.x >= m_ropeSupportSize.z ? 0 : 2;

    const std::string supportTexturePath = WideToUtf8(ResolveRuntimeAssetPath(kRopeSupportTextureRelativePath));
    m_ropeSupportTexture = new Texture();
    if (FAILED(m_ropeSupportTexture->Create(supportTexturePath.c_str())))
    {
        SAFE_DELETE(m_ropeSupportTexture);
    }
}

void SceneNarakuProto::ReleaseRopeModel()
{
    SAFE_DELETE(m_ropeModel);
    SAFE_DELETE(m_ropeTexture);
    SAFE_DELETE(m_ropeSupportModel);
    SAFE_DELETE(m_ropeSupportTexture);
    m_ropeModelCenter = {};
    m_ropeModelSize = { 1.0f, 1.0f, 1.0f };
    m_ropeModelLengthAxis = 1;
    m_ropeSupportAnchor = {};
    m_ropeSupportSize = { 1.0f, 1.0f, 1.0f };
    m_ropeSupportForwardAxis = 2;
}

void SceneNarakuProto::LoadMiningPointModel()
{
    using namespace DirectX;
    ReleaseMiningPointModel();

    const std::string modelPath = WideToUtf8(ResolveRuntimeAssetPath(kMiningPointModelRelativePath));
    m_miningPointModel = new Model();
    if (!m_miningPointModel->LoadStatic(modelPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(m_miningPointModel);
        return;
    }

    XMFLOAT3 minimum = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max() };
    XMFLOAT3 maximum = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest() };
    bool hasVertex = false;
    for (unsigned int meshIndex = 0; meshIndex < m_miningPointModel->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = m_miningPointModel->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        for (const Model::Vertex& vertex : mesh->vertices)
        {
            minimum.x = std::min(minimum.x, vertex.pos.x);
            minimum.y = std::min(minimum.y, vertex.pos.y);
            minimum.z = std::min(minimum.z, vertex.pos.z);
            maximum.x = std::max(maximum.x, vertex.pos.x);
            maximum.y = std::max(maximum.y, vertex.pos.y);
            maximum.z = std::max(maximum.z, vertex.pos.z);
            hasVertex = true;
        }
    }
    if (!hasVertex)
    {
        SAFE_DELETE(m_miningPointModel);
        return;
    }

    m_miningPointModelAnchor = {
        (minimum.x + maximum.x) * 0.5f,
        minimum.y,
        (minimum.z + maximum.z) * 0.5f };
    m_miningPointModelHorizontalSize = std::max(
        0.001f,
        std::max(maximum.x - minimum.x, maximum.z - minimum.z));
}

void SceneNarakuProto::ReleaseMiningPointModel()
{
    SAFE_DELETE(m_miningPointModel);
    m_miningPointModelAnchor = {};
    m_miningPointModelHorizontalSize = 1.0f;
}

void SceneNarakuProto::LoadPickaxeModel()
{
    using namespace DirectX;
    ReleasePickaxeModel();

    const std::string modelPath = WideToUtf8(ResolveRuntimeAssetPath(kPickaxeModelRelativePath));
    m_pickaxeModel = new Model();
    if (!m_pickaxeModel->LoadStatic(modelPath.c_str(), 1.0f, Model::ZFlip))
    {
        SAFE_DELETE(m_pickaxeModel);
        return;
    }

    XMFLOAT3 minimum = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max() };
    XMFLOAT3 maximum = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest() };
    bool hasVertex = false;
    for (unsigned int meshIndex = 0; meshIndex < m_pickaxeModel->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = m_pickaxeModel->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        for (const Model::Vertex& vertex : mesh->vertices)
        {
            minimum.x = std::min(minimum.x, vertex.pos.x);
            minimum.y = std::min(minimum.y, vertex.pos.y);
            minimum.z = std::min(minimum.z, vertex.pos.z);
            maximum.x = std::max(maximum.x, vertex.pos.x);
            maximum.y = std::max(maximum.y, vertex.pos.y);
            maximum.z = std::max(maximum.z, vertex.pos.z);
            hasVertex = true;
        }
    }
    if (!hasVertex)
    {
        SAFE_DELETE(m_pickaxeModel);
        return;
    }

    m_pickaxeModelCenter = {
        (minimum.x + maximum.x) * 0.5f,
        (minimum.y + maximum.y) * 0.5f,
        (minimum.z + maximum.z) * 0.5f };
    m_pickaxeModelLongestSize = std::max(
        0.001f,
        std::max(maximum.x - minimum.x, std::max(maximum.y - minimum.y, maximum.z - minimum.z)));
}

void SceneNarakuProto::ReleasePickaxeModel()
{
    SAFE_DELETE(m_pickaxeModel);
    m_pickaxeModelCenter = {};
    m_pickaxeModelLongestSize = 1.0f;
}

void SceneNarakuProto::DrawPickaxeModel(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& playerCenter)
{
    using namespace DirectX;
    if (m_pickaxeModel == nullptr || m_player.attackTimer <= 0.0f ||
        m_equippedWeapon == WeaponTier::Unknown)
    {
        return;
    }

    const float attackProgress = std::max(
        0.0f,
        std::min(1.0f, (kAttackTotal - m_player.attackTimer) / kAttackTotal));
    const float easedProgress = attackProgress * attackProgress * (3.0f - 2.0f * attackProgress);
    float swingAngle = XMConvertToRadians(-75.0f + 150.0f * easedProgress);
    if (m_player.attackSwingReverse) swingAngle = -swingAngle;

    const Vec2 facing = Normalize(m_player.facing);
    const float facingYaw = std::atan2(facing.x, facing.y);
    const float modelScale = kPickaxeVisualLength / m_pickaxeModelLongestSize;
    const XMMATRIX world =
        XMMatrixTranslation(
            -m_pickaxeModelCenter.x,
            -m_pickaxeModelCenter.y,
            -m_pickaxeModelCenter.z) *
        XMMatrixScaling(modelScale, modelScale, modelScale) *
        XMMatrixRotationZ(XMConvertToRadians(-35.0f)) *
        XMMatrixTranslation(0.55f, 0.30f, 0.65f) *
        XMMatrixRotationY(facingYaw + swingAngle) *
        XMMatrixTranslation(playerCenter.x, playerCenter.y, playerCenter.z);

    XMFLOAT4X4 wvp[3] = {};
    XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(world));
    wvp[1] = view;
    wvp[2] = projection;
    ShaderList::SetWVP(wvp);
    ApplySceneLighting();
    SetBlendMode(BLEND_NONE);
    SetCullingMode(D3D11_CULL_BACK);
    m_pickaxeModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
    m_pickaxeModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
    for (unsigned int meshIndex = 0; meshIndex < m_pickaxeModel->GetMeshNum(); ++meshIndex)
    {
        const Model::Mesh* mesh = m_pickaxeModel->GetMesh(meshIndex);
        if (mesh == nullptr) continue;
        const Model::Material* sourceMaterial = m_pickaxeModel->GetMaterial(mesh->materialID);
        if (sourceMaterial != nullptr)
        {
            Model::Material material = *sourceMaterial;
            ShaderList::SetMaterial(material);
        }
        m_pickaxeModel->Draw(static_cast<int>(meshIndex));
    }
}

void SceneNarakuProto::DrawMiningPointModels(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;
    if (m_miningPointModel == nullptr) return;

    ShaderList::SetCameraPos(cameraPosition);
    ApplySceneLighting();

    for (const MiningPoint& point : m_miningPoints)
    {
        const bool visibleInField = point.discovered || point.sensed ||
            IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField) continue;

        const int layerIndex = FindLayerIndexAt(point.pos, point.depth);
        const float cellSize = layerIndex >= 0
            ? m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].cellSize
            : 2.0f;
        const float modelScale = cellSize * 0.9f / m_miningPointModelHorizontalSize;
        const float groundY = GetGroundWorldY(point.pos, point.depth);

        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(
                -m_miningPointModelAnchor.x,
                -m_miningPointModelAnchor.y,
                -m_miningPointModelAnchor.z) *
            XMMatrixScaling(modelScale, modelScale, modelScale) *
            XMMatrixTranslation(point.pos.x, groundY, point.pos.y)));
        wvp[1] = view;
        wvp[2] = projection;
        ShaderList::SetWVP(wvp);
        m_miningPointModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        m_miningPointModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < m_miningPointModel->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = m_miningPointModel->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = m_miningPointModel->GetMaterial(mesh->materialID);
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                ShaderList::SetMaterial(material);
            }
            m_miningPointModel->Draw(static_cast<int>(meshIndex));
        }
    }
}

void SceneNarakuProto::DrawEnvironmentObjects(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;
    ShaderList::SetCameraPos(cameraPosition);
    ApplySceneLighting();

    const auto drawObject = [&](const NarakuMap::EnvironmentObject& object)
    {
        const auto resourceIt = std::find_if(
            m_environmentModels.begin(),
            m_environmentModels.end(),
            [&](const EnvironmentModelResource& resource) { return resource.id == object.modelId; });
        if (resourceIt == m_environmentModels.end() || resourceIt->model == nullptr) return;

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, object.layerId);
        if (layerIndex < 0) return;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
        int footprintX = resourceIt->footprintX;
        int footprintZ = resourceIt->footprintZ;
        if ((object.rotationQuarterTurns & 1) != 0) std::swap(footprintX, footprintZ);
        Vec2 position = { object.xz.x, object.xz.z };
        if (object.footprintAnchored)
        {
            position.x += static_cast<float>(footprintX - 1) * layer.cellSize * 0.5f;
            position.y += static_cast<float>(footprintZ - 1) * layer.cellSize * 0.5f;
        }
        const float groundY = GetGroundWorldY(position, layer.layerDepth);

        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(
                -resourceIt->placementAnchor.x,
                -resourceIt->placementAnchor.y,
                -resourceIt->placementAnchor.z) *
            XMMatrixScaling(object.scaleX, object.scaleY, object.scaleZ) *
            XMMatrixRotationY(XM_PIDIV2 * static_cast<float>(object.rotationQuarterTurns)) *
            XMMatrixTranslation(position.x, groundY + object.offsetY, position.y)));
        wvp[1] = view;
        wvp[2] = projection;
        ShaderList::SetWVP(wvp);
        resourceIt->model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        resourceIt->model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < resourceIt->model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = resourceIt->model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = resourceIt->model->GetMaterial(mesh->materialID);
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                ShaderList::SetMaterial(material);
            }
            resourceIt->model->Draw(static_cast<int>(meshIndex));
        }
    };

    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        drawObject(object);
    }

    const bool canReturnHere = m_currentAreaIndex >= 0 &&
        m_currentAreaIndex < static_cast<int>(m_areas.size()) &&
        m_areas[static_cast<std::size_t>(m_currentAreaIndex)].canReturn;
    if (canReturnHere)
    {
        const int layerIndex = FindLayerIndexAt(m_returnPoint, m_returnDepth);
        if (layerIndex >= 0)
        {
            const auto resourceIt = std::find_if(
                m_environmentModels.begin(),
                m_environmentModels.end(),
                [](const EnvironmentModelResource& resource)
                {
                    return resource.id == "surface_facility_abyss_entrance";
                });
            if (resourceIt == m_environmentModels.end() || resourceIt->model == nullptr) return;

            const NarakuMap::TerrainLayer& layer =
                m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)];
            const float modelScale = layer.cellSize * 0.9f / resourceIt->horizontalSize;
            NarakuMap::EnvironmentObject returnGate;
            returnGate.modelId = "surface_facility_abyss_entrance";
            returnGate.xz = { m_returnPoint.x, m_returnPoint.y };
            returnGate.layerId = layer.id;
            returnGate.scaleX = modelScale;
            returnGate.scaleY = modelScale;
            returnGate.scaleZ = modelScale;
            drawObject(returnGate);
        }
    }
}

void SceneNarakuProto::DrawBaseModels(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;
    ShaderList::SetCameraPos(cameraPosition);
    ApplySceneLighting();

    for (const NarakuMap::BasePoint& basePoint : m_runtimeMap.bases)
    {
        const std::size_t resourceIndex = basePoint.type == NarakuMap::BaseType::SecondBase ? 0u : 1u;
        const BaseModelResource& resource = m_baseModels[resourceIndex];
        if (resource.model == nullptr) continue;

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, basePoint.layerId);
        if (layerIndex < 0) continue;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)];
        const Vec2 position = {
            basePoint.xz.x + basePoint.modelOffsetX,
            basePoint.xz.z + basePoint.modelOffsetZ };
        const float groundY = GetGroundWorldY(position, layer.layerDepth);
        const float targetSize = layer.cellSize * static_cast<float>(basePoint.footprintCells);
        const float modelScale = targetSize / resource.horizontalSize;

        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(
                -resource.placementAnchor.x,
                -resource.placementAnchor.y,
                -resource.placementAnchor.z) *
            XMMatrixScaling(modelScale, modelScale, modelScale) *
            XMMatrixTranslation(position.x, groundY + basePoint.modelOffsetY, position.y)));
        wvp[1] = view;
        wvp[2] = projection;
        ShaderList::SetWVP(wvp);
        resource.model->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        resource.model->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < resource.model->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = resource.model->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = resource.model->GetMaterial(mesh->materialID);
            if (sourceMaterial != nullptr)
            {
                Model::Material material = *sourceMaterial;
                ShaderList::SetMaterial(material);
            }
            resource.model->Draw(static_cast<int>(meshIndex));
        }
    }
}

void SceneNarakuProto::DrawRopeModels(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;
    if (m_ropeModel == nullptr) return;

    ShaderList::SetCameraPos(cameraPosition);
    ApplySceneLighting();

    const XMVECTOR sourceAxis = m_ropeModelLengthAxis == 0
        ? XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f)
        : (m_ropeModelLengthAxis == 1
            ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
            : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));

    for (const RopePoint& rope : m_ropePoints)
    {
        const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);

        Vec2 descentDirection = Normalize(Sub(rope.bottomPos, rope.topPos));
        if (Distance({}, descentDirection) <= 0.001f)
        {
            descentDirection = { 0.0f, 1.0f };
        }

        if (m_ropeSupportModel != nullptr)
        {
            const float supportScale = kRopeSupportHeight / m_ropeSupportSize.y;
            const float supportForwardSize = m_ropeSupportForwardAxis == 0
                ? m_ropeSupportSize.x
                : m_ropeSupportSize.z;
            const float outsideDistance = Distance(traversal.supportPosition, traversal.topPosition);
            const Vec2 sourceDirection = m_ropeSupportForwardAxis == 0
                ? Vec2{ 1.0f, 0.0f }
                : Vec2{ 0.0f, 1.0f };
            const float supportYaw = std::atan2(
                sourceDirection.y * descentDirection.x - sourceDirection.x * descentDirection.y,
                sourceDirection.x * descentDirection.x + sourceDirection.y * descentDirection.y) + XM_PI;
            const float supportForwardScale = outsideDistance > 0.0f
                ? outsideDistance / std::max(0.001f, supportForwardSize * 0.5f)
                : supportScale;
            const float supportScaleX = m_ropeSupportForwardAxis == 0 ? supportForwardScale : supportScale;
            const float supportScaleZ = m_ropeSupportForwardAxis == 2 ? supportForwardScale : supportScale;

            XMFLOAT4X4 supportWvp[3] = {};
            XMStoreFloat4x4(&supportWvp[0], XMMatrixTranspose(
                XMMatrixTranslation(
                    -m_ropeSupportAnchor.x,
                    -m_ropeSupportAnchor.y,
                    -m_ropeSupportAnchor.z) *
                XMMatrixScaling(supportScaleX, supportScale, supportScaleZ) *
                XMMatrixRotationY(supportYaw) *
                XMMatrixTranslation(
                    traversal.supportPosition.x,
                    traversal.supportGroundWorldY,
                    traversal.supportPosition.y)));
            supportWvp[1] = view;
            supportWvp[2] = projection;
            ShaderList::SetWVP(supportWvp);
            m_ropeSupportModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
            m_ropeSupportModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
            for (unsigned int meshIndex = 0; meshIndex < m_ropeSupportModel->GetMeshNum(); ++meshIndex)
            {
                const Model::Mesh* mesh = m_ropeSupportModel->GetMesh(meshIndex);
                if (mesh == nullptr) continue;
                const Model::Material* sourceMaterial = m_ropeSupportModel->GetMaterial(mesh->materialID);
                if (sourceMaterial == nullptr) continue;
                Model::Material material = *sourceMaterial;
                if (m_ropeSupportTexture != nullptr) material.pTexture = m_ropeSupportTexture;
                ShaderList::SetMaterial(material);
                m_ropeSupportModel->Draw(static_cast<int>(meshIndex));
            }
        }

        const XMVECTOR top = XMVectorSet(
            traversal.topPosition.x, traversal.topWorldY, traversal.topPosition.y, 0.0f);
        const XMVECTOR bottom = XMVectorSet(
            traversal.bottomPosition.x, traversal.bottomWorldY, traversal.bottomPosition.y, 0.0f);
        const XMVECTOR segment = XMVectorSubtract(bottom, top);
        const float segmentLength = XMVectorGetX(XMVector3Length(segment));
        if (segmentLength <= 0.001f) continue;

        const XMVECTOR targetAxis = XMVectorScale(segment, 1.0f / segmentLength);
        const float axisDot = std::max(-1.0f, std::min(1.0f,
            XMVectorGetX(XMVector3Dot(sourceAxis, targetAxis))));
        XMMATRIX rotation = XMMatrixIdentity();
        if (axisDot < 0.9999f)
        {
            XMVECTOR rotationAxis = XMVector3Cross(sourceAxis, targetAxis);
            if (axisDot <= -0.9999f)
            {
                const XMVECTOR perpendicular = m_ropeModelLengthAxis == 0
                    ? XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f)
                    : XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
                rotationAxis = XMVector3Normalize(XMVector3Cross(sourceAxis, perpendicular));
            }
            else
            {
                rotationAxis = XMVector3Normalize(rotationAxis);
            }
            rotation = XMMatrixRotationAxis(rotationAxis, std::acos(axisDot));
        }

        float scaleX = kRopeVisualDiameter / m_ropeModelSize.x;
        float scaleY = kRopeVisualDiameter / m_ropeModelSize.y;
        float scaleZ = kRopeVisualDiameter / m_ropeModelSize.z;
        if (m_ropeModelLengthAxis == 0) scaleX = segmentLength / m_ropeModelSize.x;
        else if (m_ropeModelLengthAxis == 1) scaleY = segmentLength / m_ropeModelSize.y;
        else scaleZ = segmentLength / m_ropeModelSize.z;

        XMFLOAT3 middle = {};
        XMStoreFloat3(&middle, XMVectorScale(XMVectorAdd(top, bottom), 0.5f));
        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(-m_ropeModelCenter.x, -m_ropeModelCenter.y, -m_ropeModelCenter.z) *
            XMMatrixScaling(scaleX, scaleY, scaleZ) *
            rotation *
            XMMatrixTranslation(middle.x, middle.y, middle.z)));
        wvp[1] = view;
        wvp[2] = projection;
        ShaderList::SetWVP(wvp);
        m_ropeModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        m_ropeModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < m_ropeModel->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = m_ropeModel->GetMesh(meshIndex);
            if (mesh == nullptr) continue;
            const Model::Material* sourceMaterial = m_ropeModel->GetMaterial(mesh->materialID);
            if (sourceMaterial == nullptr) continue;
            Model::Material material = *sourceMaterial;
            if (m_ropeTexture != nullptr) material.pTexture = m_ropeTexture;
            ShaderList::SetMaterial(material);
            m_ropeModel->Draw(static_cast<int>(meshIndex));
        }
    }
}

void SceneNarakuProto::SetupFieldCamera3D(
    DirectX::XMFLOAT4X4& view,
    DirectX::XMFLOAT4X4& projection,
    DirectX::XMFLOAT3& cameraPosition,
    DirectX::XMFLOAT3& playerCenter)
{
    using namespace DirectX;

    // プレイヤー位置を3D描画用の注視点に変換します。
    const float playerHeightOffset = (m_player.onRope && m_activeRope >= 0
        ? (m_player.feetWorldY - GetGroundWorldY(m_player.pos, m_player.depth))
        : GetPlayerAirborneOffset()) + m_layerTransitionVisualOffset;
    playerCenter = ToWorld3D(m_player.pos, m_player.depth, 0.7f + playerHeightOffset);

    // 初期状態でも同じ制約を適用し、最初の右ドラッグ前にカメラが高くなりすぎないようにします。
    NormalizeCameraSettings();
    const float verticalOffset = std::sin(m_cameraPitch) * m_cameraDistance;

    // 斜め見下ろしになるように、プレイヤーの右後ろ上方へカメラを置きます。
    // プレイヤーを注視点に固定し、yaw/pitchから一定距離の軌道位置を算出します。
    const float horizontalDistance = std::sqrt(std::max(
        0.0f,
        m_cameraDistance * m_cameraDistance - verticalOffset * verticalOffset));
    XMVECTOR eye = XMVectorSet(
        playerCenter.x + std::sin(m_cameraYaw) * horizontalDistance,
        playerCenter.y + verticalOffset,
        playerCenter.z + std::cos(m_cameraYaw) * horizontalDistance,
        0.0f);

    // カメラは常にプレイヤー付近を向くようにします。
    XMVECTOR target = XMVectorSet(playerCenter.x, playerCenter.y, playerCenter.z, 0.0f);

    const XMVECTOR targetToDesiredEye = XMVectorSubtract(eye, target);
    const float desiredCameraDistance = XMVectorGetX(XMVector3Length(targetToDesiredEye));
    const XMVECTOR cameraDirection = desiredCameraDistance > 0.0001f
        ? XMVectorScale(targetToDesiredEye, 1.0f / desiredCameraDistance)
        : XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
    float nearestTerrainDistance = desiredCameraDistance;

    const auto recordTerrainIntersection = [&](const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c)
    {
        constexpr float epsilon = 0.00001f;
        const XMVECTOR vertexA = XMLoadFloat3(&a);
        const XMVECTOR edgeAB = XMVectorSubtract(XMLoadFloat3(&b), vertexA);
        const XMVECTOR edgeAC = XMVectorSubtract(XMLoadFloat3(&c), vertexA);
        const XMVECTOR perpendicular = XMVector3Cross(cameraDirection, edgeAC);
        const float determinant = XMVectorGetX(XMVector3Dot(edgeAB, perpendicular));
        if (std::fabs(determinant) < epsilon) return;

        const float inverseDeterminant = 1.0f / determinant;
        const XMVECTOR originOffset = XMVectorSubtract(target, vertexA);
        const float triangleU = XMVectorGetX(XMVector3Dot(originOffset, perpendicular)) * inverseDeterminant;
        if (triangleU < 0.0f || triangleU > 1.0f) return;

        const XMVECTOR cross = XMVector3Cross(originOffset, edgeAB);
        const float triangleV = XMVectorGetX(XMVector3Dot(cameraDirection, cross)) * inverseDeterminant;
        if (triangleV < 0.0f || triangleU + triangleV > 1.0f) return;

        const float distance = XMVectorGetX(XMVector3Dot(edgeAC, cross)) * inverseDeterminant;
        if (distance > 0.75f && distance < nearestTerrainDistance)
            nearestTerrainDistance = distance;
    };

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                if ((flags & NarakuMap::CellAttributeRemoved) != 0u) continue;
                if (!NarakuMap::IsVertexEnabled(layer, cellX, cellZ) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX, cellZ + 1) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ + 1)) continue;
                const XMFLOAT3 a = GetTerrainVertexWorld3D(layer, cellX, cellZ);
                const XMFLOAT3 b = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ);
                const XMFLOAT3 c = GetTerrainVertexWorld3D(layer, cellX, cellZ + 1);
                const XMFLOAT3 d = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ + 1);
                recordTerrainIntersection(a, b, c);
                recordTerrainIntersection(b, d, c);
            }
        }
    }

    const float collisionTargetDistance = std::max(
        1.25f, std::min(desiredCameraDistance, nearestTerrainDistance - 0.30f));
    if (m_cameraCollisionDistance < 0.0f || collisionTargetDistance < m_cameraCollisionDistance)
        m_cameraCollisionDistance = collisionTargetDistance;
    else
        m_cameraCollisionDistance = std::min(
            collisionTargetDistance, m_cameraCollisionDistance + 8.0f * kDt);
    eye = XMVectorAdd(target, XMVectorScale(cameraDirection, m_cameraCollisionDistance));

    if (m_cameraShakeTimer > 0.0f)
    {
        const float elapsed = kCameraShakeDuration - m_cameraShakeTimer;
        const float attenuation = m_cameraShakeTimer / kCameraShakeDuration;
        const float shakeX = std::sin(elapsed * 92.0f) * kCameraShakeAmplitude * attenuation;
        const float shakeY = std::cos(elapsed * 117.0f) * kCameraShakeAmplitude * attenuation;
        eye = XMVectorAdd(eye, XMVectorSet(shakeX, shakeY, 0.0f, 0.0f));
        target = XMVectorAdd(target, XMVectorSet(-shakeX * 0.25f, -shakeY * 0.15f, 0.0f, 0.0f));
    }

    // DirectXの標準的な上方向を使ってビュー行列を作ります。
    const XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    // 既存Geometoryはビュー/射影を転置して渡す設計なので、Main.cpp側の使い方に合わせます。
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMMatrixLookAtLH(eye, target, up)));
    Geometory::SetView(view);
    Sprite::SetView(view);

    // 画面比率を使って、デバッグ用の遠近投影を設定します。
    const float aspect = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), aspect, 0.1f, 500.0f)));
    Geometory::SetProjection(projection);
    Sprite::SetProjection(projection);
    XMStoreFloat3(&cameraPosition, eye);
}

void SceneNarakuProto::DrawFieldSky3D(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;

    if (m_skyModel != nullptr)
    {
        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(
            &wvp[0],
            XMMatrixTranspose(
                XMMatrixScaling(kSkySphereRadius, kSkySphereRadius, kSkySphereRadius) *
                XMMatrixTranslation(cameraPosition.x, cameraPosition.y, cameraPosition.z)));
        wvp[1] = view;
        wvp[2] = projection;

        SetDepthTest(false);
        SetCullingMode(D3D11_CULL_NONE);
        SetBlendMode(BLEND_NONE);
        ShaderList::SetWVP(wvp);
        ApplySceneLighting();
        m_skyModel->SetVertexShader(ShaderList::GetVS(ShaderList::VS_WORLD));
        m_skyModel->SetPixelShader(ShaderList::GetPS(ShaderList::PS_LAMBERT));
        for (unsigned int meshIndex = 0; meshIndex < m_skyModel->GetMeshNum(); ++meshIndex)
        {
            const Model::Mesh* mesh = m_skyModel->GetMesh(meshIndex);
            if (mesh == nullptr)
            {
                continue;
            }
            Model::Material material = *m_skyModel->GetMaterial(mesh->materialID);
            material.diffuse = { 0.40f, 0.40f, 0.40f, 1.0f };
            material.ambient = { 0.40f, 0.40f, 0.40f, 1.0f };
            material.specular = { 0.0f, 0.0f, 0.0f, 1.0f };
            ShaderList::SetMaterial(material);
            m_skyModel->Draw(static_cast<int>(meshIndex));
        }
    }

}

void SceneNarakuProto::DrawFieldWorldGeometry3D(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;

    // 3Dデバッグ描画は深度テストを有効にして、前後関係を分かりやすくします。
    SetDepthTest(true);

    SetCullingMode(D3D11_CULL_BACK);
    SetBlendMode(BLEND_NONE);
    DrawEnvironmentObjects(view, projection, cameraPosition);
    DrawBaseModels(view, projection, cameraPosition);
    DrawRopeModels(view, projection, cameraPosition);
    DrawMiningPointModels(view, projection, cameraPosition);

    // 半透明床は両面から見える方がデバッグしやすいので、カリングを切ります。
    SetCullingMode(D3D11_CULL_NONE);

    // 既存の半透明床と同じ合成結果になるよう、バッチ描画でもアルファブレンドを有効にします。
    SetBlendMode(BLEND_ALPHA);
    m_terrainFloorVertexCounts.fill(0);

    auto getLayerFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 1.0f };
        case 2: return { 0.25f, 0.36f, 0.55f, 1.0f };
        case 3: return { 0.25f, 0.45f, 0.36f, 1.0f };
        default: return { 0.18f, 0.45f, 0.30f, 1.0f };
        }
    };

    // 現在は地形を常に不透明で描画します。
    auto applyGameplayLayerAlpha = [](const NarakuMap::TerrainLayer&, XMFLOAT4 color) -> XMFLOAT4
    {
        color.w = 1.0f;
        return color;
    };

    // 実際の有効セルだけを不透明な地形として描画し、削除セルは穴として残します。
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                if ((flags & NarakuMap::CellAttributeRemoved) != 0u)
                {
                    continue;
                }

                if (!NarakuMap::IsVertexEnabled(layer, cellX, cellZ) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX, cellZ + 1) ||
                    !NarakuMap::IsVertexEnabled(layer, cellX + 1, cellZ + 1))
                {
                    continue;
                }

                const XMFLOAT3 a = GetTerrainVertexWorld3D(layer, cellX, cellZ, -0.05f);
                const XMFLOAT3 b = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ, -0.05f);
                const XMFLOAT3 c = GetTerrainVertexWorld3D(layer, cellX, cellZ + 1, -0.05f);
                const XMFLOAT3 d = GetTerrainVertexWorld3D(layer, cellX + 1, cellZ + 1, -0.05f);
                const int groundTextureId = NarakuMap::GetCellGroundTextureId(layer, cellX, cellZ);
                const XMFLOAT4 cellColor = applyGameplayLayerAlpha(layer, getLayerFloorColor(groundTextureId));
                AppendTerrainFloorCell(
                    a, b, c, d,
                    GetTerrainVertexNormal(layer, cellX, cellZ),
                    GetTerrainVertexNormal(layer, cellX + 1, cellZ),
                    GetTerrainVertexNormal(layer, cellX, cellZ + 1),
                    GetTerrainVertexNormal(layer, cellX + 1, cellZ + 1),
                    cellColor,
                    groundTextureId);

                XMFLOAT4 waterColor = {};
                if ((flags & NarakuMap::CellAttributeWaterLake) != 0u)
                    waterColor = { 0.06f, 0.40f, 0.78f, 0.64f };
                else if ((flags & NarakuMap::CellAttributeWaterPond) != 0u)
                    waterColor = { 0.12f, 0.58f, 0.90f, 0.54f };
                else if ((flags & NarakuMap::CellAttributeWaterPuddle) != 0u)
                    waterColor = { 0.22f, 0.72f, 0.96f, 0.44f };
                if (waterColor.w > 0.0f)
                {
                    auto raiseWater = [](XMFLOAT3 position)
                    {
                        position.y += 0.025f;
                        return position;
                    };
                    AppendTerrainFloorOverlayCell(
                        raiseWater(a), raiseWater(b), raiseWater(c), raiseWater(d), waterColor);
                }
            }
        }
    }

    // ロープ穴の目印として、地上側に暗い半透明板を重ねます。
    for (const RopePoint& rope : m_ropePoints)
    {
        AppendTerrainFloorQuad(ToWorld3D(rope.topPos, rope.topDepth, -0.04f), { 3.0f, 3.0f }, { 0.02f, 0.03f, 0.04f, 0.35f });
        AppendTerrainFloorQuad(ToWorld3D(rope.bottomPos, rope.bottomDepth, -0.04f), { 3.0f, 3.0f }, { 0.02f, 0.03f, 0.04f, 0.35f });
    }

    for (const LayerGateState& gate : m_layerGates)
    {
        const Vec2 point = gate.isEntry ? gate.ropePos : gate.loadPos;
        const DirectX::XMFLOAT4 color = gate.disabled
            ? DirectX::XMFLOAT4{ 0.45f, 0.12f, 0.12f, 0.55f }
            : (gate.isEntry
                ? DirectX::XMFLOAT4{ 0.20f, 0.55f, 1.0f, 0.55f }
                : DirectX::XMFLOAT4{ 0.82f, 0.50f, 0.12f, 0.55f });
        AppendTerrainFloorQuad(ToWorld3D(point, gate.depth, -0.03f), { 2.0f, 2.0f }, color);
    }
    DrawTerrainFloorBatch(view, projection);

    // 拠点モデルの占有範囲をEditorとゲームで共通のライン表示にします。
    XMFLOAT4X4 identity;
    XMStoreFloat4x4(&identity, XMMatrixIdentity());
    Geometory::SetWorld(identity);
    for (const NarakuMap::BasePoint& basePoint : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, basePoint.layerId);
        if (layerIndex < 0) continue;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)];
        const float halfSize = layer.cellSize * static_cast<float>(basePoint.footprintCells) * 0.5f;
        const float y = GetGroundWorldY({ basePoint.xz.x, basePoint.xz.z }, layer.layerDepth) + 0.08f;
        const XMFLOAT4 color = basePoint.type == NarakuMap::BaseType::SecondBase
            ? XMFLOAT4{ 0.25f, 0.90f, 1.0f, 1.0f }
            : XMFLOAT4{ 1.0f, 0.72f, 0.20f, 1.0f };
        const XMFLOAT3 nw = { basePoint.xz.x - halfSize, y, basePoint.xz.z - halfSize };
        const XMFLOAT3 ne = { basePoint.xz.x + halfSize, y, basePoint.xz.z - halfSize };
        const XMFLOAT3 se = { basePoint.xz.x + halfSize, y, basePoint.xz.z + halfSize };
        const XMFLOAT3 sw = { basePoint.xz.x - halfSize, y, basePoint.xz.z + halfSize };
        Geometory::AddLine(nw, ne, color);
        Geometory::AddLine(ne, se, color);
        Geometory::AddLine(se, sw, color);
        Geometory::AddLine(sw, nw, color);
        Geometory::AddLine(nw, se, color);
        Geometory::AddLine(ne, sw, color);
    }
    for (const SurfaceFacilityState& facility : m_surfaceFacilities)
    {
        const float halfSize = 2.0f;
        const float y = GetGroundWorldY(facility.center, facility.depth) + 0.08f;
        const XMFLOAT4 color = facility.type == NarakuPiece::SurfaceFacilityType::AbyssEntrance
            ? XMFLOAT4{ 0.75f, 0.20f, 0.90f, 1.0f } : XMFLOAT4{ 0.25f, 0.95f, 0.55f, 1.0f };
        const XMFLOAT3 nw = { facility.center.x - halfSize, y, facility.center.y - halfSize };
        const XMFLOAT3 ne = { facility.center.x + halfSize, y, facility.center.y - halfSize };
        const XMFLOAT3 se = { facility.center.x + halfSize, y, facility.center.y + halfSize };
        const XMFLOAT3 sw = { facility.center.x - halfSize, y, facility.center.y + halfSize };
        Geometory::AddLine(nw, ne, color); Geometory::AddLine(ne, se, color);
        Geometory::AddLine(se, sw, color); Geometory::AddLine(sw, nw, color);
        Geometory::AddLine(nw, se, color); Geometory::AddLine(ne, sw, color);
    }
    Geometory::DrawLines();

}

void SceneNarakuProto::DrawFieldActors3D(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& playerCenter)
{
    using namespace DirectX;

    // モデルを読み込めない場合だけ、従来の上下端表示を残します。
    if (m_ropeModel == nullptr)
    {
        for (const RopePoint& rope : m_ropePoints)
        {
            const XMFLOAT3 top = ToWorld3D(rope.topPos, rope.topDepth, 1.2f);
            const XMFLOAT3 bottom = ToWorld3D(rope.bottomPos, rope.bottomDepth, 0.0f);
            DrawDebugBox3D({ top.x, top.y, top.z }, { 0.35f, 0.35f, 0.35f });
            DrawDebugBox3D({ bottom.x, bottom.y, bottom.z }, { 0.30f, 0.30f, 0.30f });
        }
    }

    // モデルを読み込めない場合だけ、採掘ポイントを従来の箱で描画します。
    if (m_miningPointModel == nullptr)
    {
        for (const MiningPoint& point : m_miningPoints)
        {
            const bool visibleInField = point.discovered || point.sensed || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
            if (!visibleInField) continue;

            const XMFLOAT3 base = ToWorld3D(point.pos, point.depth, 0.15f);
            const float width = 0.45f + 0.08f * static_cast<float>(point.visualType);
            DrawDebugBox3D({ base.x, base.y + 0.2f, base.z }, { width, 0.4f, width });
        }
    }
    for (const FishingPoint& point : m_fishingPoints)
    {
        const XMFLOAT3 base = ToWorld3D(point.pos, point.depth, 0.12f);
        const float height = point.remainingUses > 0 ? 0.9f : 0.35f;
        DrawDebugBox3D({ base.x, base.y + height * 0.5f, base.z }, { 0.12f, height, 0.12f });
    }

    // 地面に落ちている旧器を小さい箱で示します。
    for (const GroundRelic& relic : m_groundRelics)
    {
        // 回収不能になった旧器は描画しません。
        if (!relic.active)
        {
            continue;
        }

        // 旧器の位置を3D座標へ変換します。
        const XMFLOAT3 base = ToWorld3D(relic.pos, relic.depth, 0.12f);

        // 小さな箱で旧器の本体を描きます。
        DrawDebugBox3D({ base.x, base.y + 0.12f, base.z }, { 0.35f, 0.25f, 0.35f });

    }

    // 敵が落とした食料を箱で描画します。
    for (const GroundFood& food : m_groundFoods)
    {
        if (!food.active) continue;
        const XMFLOAT3 base = ToWorld3D(food.pos, food.depth, 0.12f);
        DrawDebugBox3D({ base.x, base.y + 0.12f, base.z }, { 0.30f, 0.22f, 0.30f });
    }

    DrawQuestTargets3D();

    DrawEnemyBillboardBatch(view, projection, EnemyType::Charger, m_enemyTextures[0]);
    DrawEnemyBillboardBatch(view, projection, EnemyType::Territory, m_enemyTextures[1]);

    const XMMATRIX characterViewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&view));
    XMFLOAT4X4 characterBillboardFloat = {};
    XMStoreFloat4x4(&characterBillboardFloat, XMMatrixInverse(nullptr, characterViewMatrix));
    characterBillboardFloat._41 = 0.0f;
    characterBillboardFloat._42 = 0.0f;
    characterBillboardFloat._43 = 0.0f;
    const XMMATRIX characterBillboard = XMLoadFloat4x4(&characterBillboardFloat);

    if (m_jumpEffectTexture != nullptr && !m_jumpEffects.empty())
    {
        SetBlendMode(BLEND_ALPHA);
        SetCullingMode(D3D11_CULL_NONE);
        SetSamplerState(SAMPLER_POINT);
        Sprite::SetVertexShader(nullptr);
        Sprite::SetPixelShader(nullptr);
        Sprite::SetTexture(m_jumpEffectTexture);
        Sprite::SetSize({ 2.4f, 2.4f });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVScale({
            1.0f / static_cast<float>(kJumpEffectColumns),
            1.0f / static_cast<float>(kJumpEffectRows) });
        Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        for (const JumpEffect& effect : m_jumpEffects)
        {
            const float elapsed = kJumpEffectDuration - effect.remainingTime;
            const int frame = std::max(
                0,
                std::min(
                    kJumpEffectFrameCount - 1,
                    static_cast<int>(elapsed / kJumpEffectFrameTime)));
            const int frameX = frame % kJumpEffectColumns;
            const int frameY = frame / kJumpEffectColumns;
            const XMFLOAT3 effectPosition = ToWorld3D(effect.pos, effect.depth, 0.9f);
            XMFLOAT4X4 effectWorld = {};
            XMStoreFloat4x4(
                &effectWorld,
                XMMatrixTranspose(
                    characterBillboard *
                    XMMatrixTranslation(effectPosition.x, effectPosition.y, effectPosition.z)));
            Sprite::SetWorld(effectWorld);
            Sprite::SetUVPos({
                static_cast<float>(frameX) / static_cast<float>(kJumpEffectColumns),
                static_cast<float>(frameY) / static_cast<float>(kJumpEffectRows) });
            Sprite::Draw();
        }
        SetSamplerState(SAMPLER_LINEAR);
    }

    if (m_playerTexture != nullptr)
    {
        const bool moving =
            m_lastFrameMovementDistance > 0.001f || m_lastFrameRopeMoving;
        const Vec2 cameraForward = GetCameraForward();
        const Vec2 cameraRight = GetCameraRight();
        const CharacterFrameSequence sequence = GetCharacterFrameSequence(
            Dot(m_player.facing, cameraRight),
            Dot(m_player.facing, cameraForward),
            moving);
        const int sequenceFrame =
            static_cast<int>(m_characterAnimationTime * kPlayerMoveAnimationFps) % sequence.count;
        const int frame = sequence.frames[sequenceFrame];
        const int frameX = frame % kCharacterSpriteColumns;
        const int frameY = frame / kCharacterSpriteColumns;
        XMFLOAT4X4 playerWorld = {};
        XMStoreFloat4x4(
            &playerWorld,
            XMMatrixTranspose(
                characterBillboard *
                XMMatrixTranslation(playerCenter.x, playerCenter.y, playerCenter.z)));

        SetBlendMode(BLEND_ALPHA);
        SetCullingMode(D3D11_CULL_NONE);
        SetSamplerState(SAMPLER_POINT);
        Sprite::SetVertexShader(nullptr);
        Sprite::SetPixelShader(nullptr);
        Sprite::SetTexture(m_playerTexture);
        Sprite::SetSize({ 2.4f, 2.4f });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVScale({
            (sequence.mirror ? -1.0f : 1.0f) / static_cast<float>(kCharacterSpriteColumns),
            1.0f / static_cast<float>(kCharacterSpriteRows) });
        Sprite::SetUVPos({
            static_cast<float>(sequence.mirror ? frameX + 1 : frameX) /
                static_cast<float>(kCharacterSpriteColumns),
            static_cast<float>(frameY) / static_cast<float>(kCharacterSpriteRows) });
        XMFLOAT3 playerLight = CalculateSceneLightColor(m_player.pos, m_player.depth, playerCenter.y);
        if (m_portableLightOn)
        {
            playerLight.x = std::max(playerLight.x, 1.0f);
            playerLight.y = std::max(playerLight.y, 0.88f);
            playerLight.z = std::max(playerLight.z, 0.70f);
        }
        Sprite::SetColor({ playerLight.x, playerLight.y, playerLight.z, 1.0f });
        Sprite::SetWorld(playerWorld);
        Sprite::Draw();
        SetSamplerState(SAMPLER_LINEAR);
    }

    DrawPickaxeModel(view, projection, playerCenter);

#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
    if (m_showCollisionDebug)
    {
        // 帰還範囲のデバッグ表示を追加
        if (m_currentAreaIndex >= 0)
        {
            const XMFLOAT3 returnDebugBase = ToWorld3D(m_returnPoint, m_returnDepth, 0.05f);
            DrawDebugSphere3D({ returnDebugBase.x, returnDebugBase.y + 0.05f, returnDebugBase.z }, kReturnRange);
        }

        // 未採掘の採掘ポイントのインタラクト範囲のデバッグ表示を追加
        for (const MiningPoint& point : m_miningPoints)
        {
            if (!point.mined)
            {
                const XMFLOAT3 base = ToWorld3D(point.pos, point.depth, 0.15f);
                DrawDebugSphere3D({ base.x, base.y + 0.05f, base.z }, kInteractRange);
            }
        }

    }
#endif

}

void SceneNarakuProto::DrawAttackHitEffects3D(const DirectX::XMFLOAT4X4& view)
{
    using namespace DirectX;

    if (m_attackHitTexture != nullptr && !m_attackHitEffects.empty())
    {
        const XMMATRIX viewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&view));
        XMFLOAT4X4 billboardFloat = {};
        XMStoreFloat4x4(&billboardFloat, XMMatrixInverse(nullptr, viewMatrix));
        billboardFloat._41 = 0.0f;
        billboardFloat._42 = 0.0f;
        billboardFloat._43 = 0.0f;
        const XMMATRIX billboard = XMLoadFloat4x4(&billboardFloat);

        SetBlendMode(BLEND_ADDALPHA);
        SetCullingMode(D3D11_CULL_NONE);
        Sprite::SetVertexShader(nullptr);
        Sprite::SetPixelShader(nullptr);
        Sprite::SetTexture(m_attackHitTexture);
        Sprite::SetSize({ 1.5f, 1.5f });
        Sprite::SetOffset({ 0.0f, 0.0f });
        Sprite::SetUVScale({ 1.0f / static_cast<float>(kAttackHitEffectFrameCount), 1.0f });
        Sprite::SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        for (const AttackHitEffect& effect : m_attackHitEffects)
        {
            const float elapsed = kAttackHitEffectDuration - effect.remainingTime;
            const int frame = std::max(
                0,
                std::min(
                    kAttackHitEffectFrameCount - 1,
                    static_cast<int>(elapsed / kAttackHitEffectFrameTime)));
            const XMFLOAT3 effectPosition = ToWorld3D(effect.pos, effect.depth, 0.65f);
            XMFLOAT4X4 effectWorld = {};
            XMStoreFloat4x4(
                &effectWorld,
                XMMatrixTranspose(
                    billboard * XMMatrixTranslation(effectPosition.x, effectPosition.y, effectPosition.z)));
            Sprite::SetWorld(effectWorld);
            Sprite::SetUVPos({ static_cast<float>(frame) / static_cast<float>(kAttackHitEffectFrameCount), 0.0f });
            Sprite::Draw();
        }
        SetBlendMode(BLEND_ALPHA);
    }
}

void SceneNarakuProto::Draw3DField()
{
    DirectX::XMFLOAT4X4 view = {};
    DirectX::XMFLOAT4X4 projection = {};
    DirectX::XMFLOAT3 cameraPosition = {};
    DirectX::XMFLOAT3 playerCenter = {};
    SetupFieldCamera3D(view, projection, cameraPosition, playerCenter);
    DrawFieldSky3D(view, projection, cameraPosition);
    DrawFieldWorldGeometry3D(view, projection, cameraPosition);
    DrawFieldActors3D(view, projection, playerCenter);
    DrawAttackHitEffects3D(view);
}
void SceneNarakuProto::DrawUpperLoadVisionEffect() const
{
    if (m_upperLoadFifthTimer <= 0.0f &&
        (m_upperLoadVisionTimer <= 0.0f || m_upperLoadVisionOcclusion <= 0.0f))
    {
        return;
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 minimum = viewport->WorkPos;
    const ImVec2 maximum(
        viewport->WorkPos.x + viewport->WorkSize.x,
        viewport->WorkPos.y + viewport->WorkSize.y);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();

    if (m_upperLoadFifthTimer > 0.0f)
    {
        draw->AddRectFilled(minimum, maximum, IM_COL32(0, 0, 0, 255));
        return;
    }

    const float occlusion = std::max(0.0f, std::min(1.0f, m_upperLoadVisionOcclusion));
    if (occlusion <= 0.0f) return;
    if (occlusion >= 1.0f)
    {
        draw->AddRectFilled(minimum, maximum, IM_COL32(0, 0, 0, 255));
        return;
    }

    constexpr int sliceCount = 128;
    constexpr float pi = 3.14159265358979323846f;
    const ImVec2 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
    const float visibleScale = std::sqrt((1.0f - occlusion) * 4.0f / pi);
    const float radiusX = (maximum.x - minimum.x) * 0.5f * visibleScale;
    const float radiusY = (maximum.y - minimum.y) * 0.5f * visibleScale;
    const float sliceHeight = (maximum.y - minimum.y) / static_cast<float>(sliceCount);
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    for (int slice = 0; slice < sliceCount; ++slice)
    {
        const float top = minimum.y + sliceHeight * static_cast<float>(slice);
        const float bottom = slice == sliceCount - 1 ? maximum.y : top + sliceHeight + 1.0f;
        const float middle = (top + bottom) * 0.5f;
        const float normalizedY = radiusY > 0.0f ? (middle - center.y) / radiusY : 2.0f;
        float halfVisibleWidth = 0.0f;
        if (std::fabs(normalizedY) < 1.0f)
        {
            halfVisibleWidth = radiusX * std::sqrt(std::max(0.0f, 1.0f - normalizedY * normalizedY));
        }
        const float visibleLeft = std::max(minimum.x, center.x - halfVisibleWidth);
        const float visibleRight = std::min(maximum.x, center.x + halfVisibleWidth);
        if (visibleLeft > minimum.x)
            draw->AddRectFilled(ImVec2(minimum.x, top), ImVec2(visibleLeft, bottom), black);
        if (visibleRight < maximum.x)
            draw->AddRectFilled(ImVec2(visibleRight, top), ImVec2(maximum.x, bottom), black);
    }
}

void SceneNarakuProto::DrawDehydrationVisionEffect() const
{
    const float occlusion = std::max(0.0f, std::min(kDehydrationMaxOcclusion,
        m_dehydrationVisionStrength * kDehydrationMaxOcclusion));
    if (occlusion <= 0.0f) return;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 minimum = viewport->WorkPos;
    const ImVec2 maximum(
        viewport->WorkPos.x + viewport->WorkSize.x,
        viewport->WorkPos.y + viewport->WorkSize.y);
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    constexpr int sliceCount = 128;
    constexpr float pi = 3.14159265358979323846f;
    const ImVec2 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f);
    const float visibleScale = std::sqrt((1.0f - occlusion) * 4.0f / pi);
    const float radiusX = (maximum.x - minimum.x) * 0.5f * visibleScale;
    const float radiusY = (maximum.y - minimum.y) * 0.5f * visibleScale;
    const float sliceHeight = (maximum.y - minimum.y) / static_cast<float>(sliceCount);
    const ImU32 black = IM_COL32(0, 0, 0, 255);
    for (int slice = 0; slice < sliceCount; ++slice)
    {
        const float top = minimum.y + sliceHeight * static_cast<float>(slice);
        const float bottom = slice == sliceCount - 1 ? maximum.y : top + sliceHeight + 1.0f;
        const float middle = (top + bottom) * 0.5f;
        const float normalizedY = radiusY > 0.0f ? (middle - center.y) / radiusY : 2.0f;
        float halfVisibleWidth = 0.0f;
        if (std::fabs(normalizedY) < 1.0f)
            halfVisibleWidth = radiusX * std::sqrt(std::max(0.0f, 1.0f - normalizedY * normalizedY));
        const float visibleLeft = std::max(minimum.x, center.x - halfVisibleWidth);
        const float visibleRight = std::min(maximum.x, center.x + halfVisibleWidth);
        if (visibleLeft > minimum.x)
            draw->AddRectFilled(ImVec2(minimum.x, top), ImVec2(visibleLeft, bottom), black);
        if (visibleRight < maximum.x)
            draw->AddRectFilled(ImVec2(visibleRight, top), ImVec2(maximum.x, bottom), black);
    }
}

void SceneNarakuProto::DrawCompass() const
{
    using namespace DirectX;

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 viewportMin = viewport->WorkPos;
    const ImVec2 viewportMax(
        viewport->WorkPos.x + viewport->WorkSize.x,
        viewport->WorkPos.y + viewport->WorkSize.y);
    const ImVec2 center(
        viewportMax.x - kCompassMargin - kCompassRadius,
        viewportMin.y + kCompassMargin + kCompassRadius);

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    drawList->PushClipRect(viewportMin, viewportMax, true);
    drawList->AddCircle(center, kCompassRadius, IM_COL32(220, 230, 240, 220), 32, kCompassLineThickness);
    drawList->AddCircleFilled(center, 2.5f, IM_COL32(220, 230, 240, 230));

    const float cosPitch = std::cos(m_cameraPitch);
    const XMVECTOR cameraForward = XMVectorSet(
        -std::sin(m_cameraYaw) * cosPitch,
        -std::sin(m_cameraPitch),
        -std::cos(m_cameraYaw) * cosPitch,
        0.0f);
    const XMMATRIX view = XMMatrixLookToLH(
        XMVectorZero(),
        cameraForward,
        XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    const XMFLOAT3 worldDirections[] =
    {
        { 0.0f, 0.0f, -1.0f },
        { 0.0f, 0.0f, 1.0f },
        { 1.0f, 0.0f, 0.0f },
        { -1.0f, 0.0f, 0.0f },
    };

    for (int index = 0; index < 4; ++index)
    {
        XMFLOAT3 viewDirection = {};
        XMStoreFloat3(&viewDirection, XMVector3TransformNormal(XMLoadFloat3(&worldDirections[index]), view));
        const float length = std::sqrt(
            viewDirection.x * viewDirection.x +
            viewDirection.y * viewDirection.y);
        if (length <= 0.0001f)
        {
            continue;
        }

        const ImVec2 direction(viewDirection.x / length, -viewDirection.y / length);
        const ImVec2 endpoint(
            center.x + direction.x * (kCompassRadius - kCompassLinePadding),
            center.y + direction.y * (kCompassRadius - kCompassLinePadding));
        const ImU32 color = index == 0
            ? IM_COL32(245, 95, 95, 230)
            : IM_COL32(220, 230, 240, 220);
        drawList->AddLine(center, endpoint, color, kCompassLineThickness);

        const ImVec2 textSize = ImGui::CalcTextSize(kCompassDirectionLabels[index]);
        const ImVec2 labelCenter(
            center.x + direction.x * (kCompassRadius + kCompassLabelDistance),
            center.y + direction.y * (kCompassRadius + kCompassLabelDistance));
        ImVec2 textPosition(
            labelCenter.x - textSize.x * 0.5f,
            labelCenter.y - textSize.y * 0.5f);
        textPosition.x = std::max(viewportMin.x, std::min(textPosition.x, viewportMax.x - textSize.x));
        textPosition.y = std::max(viewportMin.y, std::min(textPosition.y, viewportMax.y - textSize.y));
        drawList->AddText(textPosition, color, kCompassDirectionLabels[index]);
    }

    drawList->PopClipRect();
}

void SceneNarakuProto::DrawField()
{
    // フィールドウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 20.0f), ImGuiCond_FirstUseEver);

    // フィールドウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(760.0f, 620.0f), ImGuiCond_FirstUseEver);

    // フィールド描画用ウィンドウを開始します。
    ImGui::Begin(u8"奈落塔プロト フィールド");

    // フィールドキャンバス左上のスクリーン座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // フィールドキャンバスの表示サイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 520.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();

    // フィールド背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(30, 36, 34, 255));

    // フィールド外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 150, 140, 255));

    // 帰還地点のワールド座標を斜め見下ろし座標に変換します。
    Vec2 ret = WorldToObliqueCanvas(canvasPos, canvasSize, m_returnPoint, m_returnDepth);

    // 帰還地点を青い円で描きます。
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 9.0f, IM_COL32(80, 180, 255, 255));

    // すべてのロープを描画します。
    for (const RopePoint& rope : m_ropePoints)
    {
        const Vec2 top = WorldToObliqueCanvas(canvasPos, canvasSize, rope.topPos, rope.topDepth);
        const Vec2 bottom = WorldToObliqueCanvas(canvasPos, canvasSize, rope.bottomPos, rope.bottomDepth);
        draw->AddLine(ImVec2(top.x, top.y), ImVec2(bottom.x, bottom.y), IM_COL32(170, 120, 70, 255), 4.0f);
    }

    // 発見済み、または近くまで来た採掘ポイントを描画します。
    for (const MiningPoint& point : m_miningPoints)
    {
        // 未記録でも近くまで来たポイントは、初期記録済みと同じ色で見せます。
        const bool visibleInField = point.discovered || point.sensed || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField) continue;

        // 採掘ポイント座標を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, point.pos, point.depth);

        // 採掘済みは暗色、未採掘は黄色系にします。
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);

        // 見た目4種類は半径差だけで表現します。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 5.0f + static_cast<float>(point.visualType), color);
    }

    // 地面に置かれた旧器を描画します。
    for (const GroundRelic& relic : m_groundRelics)
    {
        // 非アクティブな旧器は描画しません。
        if (!relic.active) continue;

        // 旧器位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, relic.pos, relic.depth);

        // 旧器を小さな四角で描きます。
        draw->AddRectFilled(ImVec2(p.x - 4.0f, p.y - 4.0f), ImVec2(p.x + 4.0f, p.y + 4.0f), IM_COL32(240, 220, 130, 255));
    }

    // プレイヤーが置いたピンを描画します。
    for (const Vec2& pin : m_pins)
    {
        // ピン位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, pin);

        // ピンを赤い三角形で描きます。
        draw->AddTriangleFilled(ImVec2(p.x, p.y - 10.0f), ImVec2(p.x - 6.0f, p.y + 5.0f), ImVec2(p.x + 6.0f, p.y + 5.0f), IM_COL32(230, 80, 90, 255));
    }

    // 敵を描画します。
    for (const EnemyState& enemy : m_enemies)
    {
        // 死んだ敵は描画しません。
        if (!enemy.alive) continue;

        // 敵位置を斜め見下ろし座標へ変換します。
        Vec2 p = WorldToObliqueCanvas(canvasPos, canvasSize, enemy.pos);

        // 通常時の敵色を赤にします。
        ImU32 color = enemy.telegraphTimer > 0.0f ? IM_COL32(255, 200, 60, 255) : IM_COL32(210, 70, 70, 255);

        // 体当たり中はより強い赤で表示します。
        if (enemy.chargeTimer > 0.0f) color = IM_COL32(255, 80, 40, 255);

        // 敵を円で描きます。
        draw->AddCircleFilled(ImVec2(p.x, p.y), 7.0f, color);
    }

    // プレイヤー位置を斜め見下ろし座標へ変換します。
    Vec2 player = WorldToObliqueCanvas(canvasPos, canvasSize, m_player.pos, m_player.depth);

    // プレイヤー本体を緑の円で描きます。
    draw->AddCircleFilled(ImVec2(player.x, player.y), 7.5f, IM_COL32(90, 220, 150, 255));

    // 向き表示の終点を計算します。
    Vec2 faceEnd = WorldToObliqueCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 1.2f)), m_player.depth);

    // プレイヤーの向きを短い線で描きます。
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 2.0f);

    // キャンバスぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    // 操作確認用の短い説明を表示します。
    ImGui::Text(u8"WASD 移動 / Shift短押し ステップ / Shift長押し 走り / Space ジャンプ / 左クリック 攻撃 / F 調べる / T 所持品 / ロープ中A/D 離脱");

    // 採掘中ならメインウィンドウ（フィールド）の中央に進行度バーを描画します。
    if (m_miningIndex >= 0)
    {
        float centerX = canvasPos.x + canvasSize.x * 0.5f;
        float centerY = canvasPos.y + canvasSize.y * 0.5f;

        float barWidth = 240.0f;
        float barHeight = 18.0f;
        float progress = std::max(0.0f, std::min(1.0f, 1.0f - (m_miningTimer / m_miningDuration)));

        std::string text = u8"採掘中...";
        ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
        ImVec2 textPos = { centerX - textSize.x * 0.5f, centerY - 25.0f };
        draw->AddText(textPos, IM_COL32(255, 220, 60, 255), text.c_str());

        ImVec2 bgMin = { centerX - barWidth * 0.5f, centerY };
        ImVec2 bgMax = { centerX + barWidth * 0.5f, centerY + barHeight };
        draw->AddRectFilled(bgMin, bgMax, IM_COL32(10, 15, 15, 200), 4.0f);
        draw->AddRect(bgMin, bgMax, IM_COL32(130, 150, 140, 255), 4.0f, 0, 1.5f);

        if (progress > 0.0f)
        {
            ImVec2 fgMax = { bgMin.x + barWidth * progress, bgMax.y };
            draw->AddRectFilled(bgMin, fgMax, IM_COL32(240, 200, 50, 255), 4.0f);
        }
    }

    // フィールドウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawHud()
{
    constexpr float fixedGaugeWindowWidth = 280.0f;
    constexpr float overlayHeight = 305.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float overlayWidth = std::max(fixedGaugeWindowWidth, viewport->WorkSize.x - 32.0f);
    const ImVec2 overlayPos(viewport->WorkPos.x + 16.0f, viewport->WorkPos.y + 16.0f);
    ImGui::SetNextWindowPos(overlayPos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("PlayerStatusOverlay##Overlay", nullptr, flags))
    {
        const auto drawGauge = [](float ratio, const char* label, const ImVec4& color, float width)
        {
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.04f, 0.05f, 0.05f, 0.88f));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
            ImGui::ProgressBar(std::max(0.0f, std::min(1.0f, ratio)), ImVec2(width, 18.0f), label);
            ImGui::PopStyleColor(2);
        };

        constexpr float level100MaxHp = 1200.0f;
        constexpr float maximumHpWindowRatio = 0.75f;
        const float gaugeWidthPerHp = viewport->WorkSize.x * maximumHpWindowRatio / level100MaxHp;
        const float fixedGaugeWidth = fixedGaugeWindowWidth - ImGui::GetStyle().WindowPadding.x * 2.0f;
        const float hpGaugeWidth = GetMaxHp() * gaugeWidthPerHp;
        const float staminaGaugeWidth = GetMaxStamina() * gaugeWidthPerHp;
        const float mentalGaugeWidth = GetMaxMental() * gaugeWidthPerHp;

        drawGauge(m_player.hp / GetMaxHp(), "HP", ImVec4(0.78f, 0.18f, 0.16f, 1.0f), hpGaugeWidth);
        drawGauge(m_player.stamina / GetMaxStamina(), u8"スタミナ", ImVec4(0.18f, 0.68f, 0.36f, 1.0f), staminaGaugeWidth);
        drawGauge(m_player.mental / GetMaxMental(), u8"精神力", ImVec4(0.22f, 0.48f, 0.82f, 1.0f), mentalGaugeWidth);
        drawGauge(m_fullness / kFullnessMaximum, u8"満腹度", ImVec4(0.78f, 0.56f, 0.20f, 1.0f), fixedGaugeWidth);
        drawGauge(m_hydration / kHydrationMaximum, u8"水分", ImVec4(0.15f, 0.62f, 0.86f, 1.0f), fixedGaugeWidth);
        char activityLabel[64];
        std::snprintf(activityLabel, sizeof(activityLabel), u8"奈落活性度 %d", GetCurrentActivity());
        drawGauge(std::min(1.0f, GetCurrentActivity() / 100.0f), activityLabel, ImVec4(0.70f, 0.20f, 0.72f, 1.0f), fixedGaugeWidth);
        drawGauge(m_player.upperLoad / kUpperLoadLimit, u8"上昇負荷 (Debug)", ImVec4(0.88f, 0.58f, 0.18f, 1.0f), fixedGaugeWidth);
        if (m_level < 100)
            ImGui::Text(u8"Lv%d  EXP %s / %s", m_level, FormatExp(m_currentExp).c_str(), FormatExp(GetRequiredExp(m_level)).c_str());
        else
            ImGui::Text(u8"Lv100  保護:%d  余剰:%s", m_levelProtection, FormatExp(m_level100OverflowExp).c_str());
        if (m_fullness <= kFullnessCritical) ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.15f, 1.0f), u8"飢餓警告");
        else if (m_fullness <= 25.0f) ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.15f, 1.0f), u8"空腹（最大スタミナ-10%%）");
        else if (m_fullness <= kFullnessWarning) ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.20f, 1.0f), u8"軽い空腹");
        const int activity = GetCurrentActivity();
        ImGui::Text(u8"活性段階: %s", activity >= 100 ? u8"危険活性" : activity >= 65 ? u8"高活性" : activity >= 40 ? u8"中活性" : u8"低活性");
        ImGui::Text(u8"精神力能力: %s", m_level >= 30 ? u8"Q短押し/長押しで使用" : u8"Lv30で解放");
        if (m_miningSenseTimer > 0.0f) ImGui::Text(u8"採掘感知: %.0f秒", std::ceil(m_miningSenseTimer));
        if (m_upperLoadWardTimer > 0.0f) ImGui::Text(u8"上昇負荷無効: %.0f秒", std::ceil(m_upperLoadWardTimer));
        if (m_upperLoadVisionTimer > 0.0f) ImGui::Text(u8"上昇負荷・視界不良: %.0f秒", std::ceil(m_upperLoadVisionTimer));
        if (m_upperLoadFifthTimer > 0.0f) ImGui::Text(u8"上昇負荷・盲目: %.0f秒", std::ceil(m_upperLoadFifthTimer));
        if (m_dehydrationVisionStrength > 0.0f)
            ImGui::Text(u8"脱水視界不良: %.0f%%", m_dehydrationVisionStrength * 100.0f);
        if (m_portableLightOn)
        {
            const auto& lights = GetAccessibleLights();
            const auto active = std::min_element(lights.begin(), lights.end(), [](const PortableLight& left, const PortableLight& right)
            {
                const float leftTime = (!left.broken && left.remainingSeconds > 0.0f) ? left.remainingSeconds : FLT_MAX;
                const float rightTime = (!right.broken && right.remainingSeconds > 0.0f) ? right.remainingSeconds : FLT_MAX;
                return leftTime < rightTime;
            });
            if (active != lights.end() && !active->broken)
                ImGui::Text(u8"ライト点灯中: %.0f:%02.0f", std::floor(active->remainingSeconds / 60.0f),
                    std::fmod(active->remainingSeconds, 60.0f));
        }
        if (m_cookingTarget != CookingTarget::None)
            ImGui::Text(u8"%s: %.0f秒", m_cookingTarget == CookingTarget::BoilBottle ? u8"煮沸中" : u8"加熱中", std::ceil(m_cookingTimer));
    }
    ImGui::End();
}

#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
void SceneNarakuProto::DrawDebugPlayerTuning()
{
    // 調整ウィンドウの初期位置をHUDの右側へ置きます。
    ImGui::SetNextWindowPos(ImVec2(1240.0f, 20.0f), ImGuiCond_FirstUseEver);

    // 調整項目が見切れない程度の初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(380.0f, 430.0f), ImGuiCond_FirstUseEver);

    // プレイテスト専用の調整ウィンドウを開始します。
    ImGui::Begin(u8"プレイテスト調整", nullptr, ImGuiWindowFlags_NoNavInputs);

    // 現在の重量補正を確認しながら調整できるよう、実効状態を先頭に表示します。
    ImGui::Text(u8"実効歩行速度: %.2f", GetMoveSpeed());
    ImGui::Text(u8"重量補正: %.0f%%", GetWeightRate() * 100.0f);
    ImGui::Separator();

    if (ImGui::CollapsingHeader(u8"進行デバッグ", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (ImGui::InputInt(u8"所持金", &m_money, 100, 1000))
        {
            m_money = std::max(0, m_money);
            SaveProgress();
        }

        int requestedLevel = m_level;
        if (ImGui::InputInt(u8"レベル", &requestedLevel, 1, 10))
        {
            requestedLevel = std::max(1, std::min(100, requestedLevel));
            if (requestedLevel != m_level)
            {
                const float oldMaxHp = GetMaxHp();
                const float oldMaxStamina = GetMaxStamina();
                const float oldMaxMental = GetMaxMental();
                m_level = requestedLevel;
                m_currentExp = 0;
                PreserveResourceRatios(oldMaxHp, oldMaxStamina, oldMaxMental);
                SaveProgress();
            }
        }

#if defined(_DEBUG) && !defined(NARAKU_EDITOR_BUILD)
        ImGui::SeparatorText(u8"ゲーム内日時");
        static bool dateControlInitialized = false;
        static int debugWeekday = 0;
        static int debugHour = 0;
        static int debugMinute = 0;
        const int currentDebugSecond = static_cast<int>(std::fmod(
            std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
        if (!dateControlInitialized)
        {
            debugWeekday = currentDebugSecond / static_cast<int>(kGameDaySeconds);
            const int daySecond = currentDebugSecond % static_cast<int>(kGameDaySeconds);
            debugHour = daySecond / 3600;
            debugMinute = (daySecond % 3600) / 60;
            dateControlInitialized = true;
        }
        const char* weekdayNames[] = { u8"月曜日", u8"火曜日", u8"水曜日", u8"木曜日", u8"金曜日", u8"土曜日", u8"日曜日" };
        const int currentDebugDaySecond = currentDebugSecond % static_cast<int>(kGameDaySeconds);
        ImGui::Text(u8"現在: %s %02d:%02d",
            weekdayNames[currentDebugSecond / static_cast<int>(kGameDaySeconds)],
            currentDebugDaySecond / 3600, (currentDebugDaySecond % 3600) / 60);
        ImGui::Combo(u8"変更先の曜日", &debugWeekday, weekdayNames, 7);
        ImGui::SliderInt(u8"変更先の時", &debugHour, 0, 23);
        ImGui::SliderInt(u8"変更先の分", &debugMinute, 0, 59);
        ImGui::TextDisabled(u8"現在時刻以前を指定すると、次週の指定日時へ進みます。");
        ImGui::TextDisabled(u8"敵・採掘復活と依頼期限の実時間タイマーは進みません。");
        if (ImGui::Button(u8"指定日時へ進める"))
            DebugAdvanceGameDate(debugWeekday, debugHour, debugMinute);

        ImGui::SeparatorText(u8"エリアワープ");
        static int warpDepth = 1;
        static int warpSublayer = 0;
        static int warpAreaNumber = 1;
        const char* sublayerNames[] = { u8"上層", u8"中層", u8"下層" };
        if (ImGui::SliderInt(u8"ワープ先の層", &warpDepth, 1, 5)) warpAreaNumber = 1;
        if (ImGui::Combo(u8"ワープ先の区分", &warpSublayer, sublayerNames, 3)) warpAreaNumber = 1;

        int maximumAreaNumber = 0;
        for (const AreaState& area : m_areas)
        {
            if (area.depth == warpDepth && area.sublayer == warpSublayer)
                maximumAreaNumber = std::max(maximumAreaNumber, area.areaNumber);
        }
        warpAreaNumber = std::max(1, std::min(std::max(1, maximumAreaNumber), warpAreaNumber));
        ImGui::SliderInt(u8"ワープ先のエリア番号", &warpAreaNumber, 1, std::max(1, maximumAreaNumber));
        if (maximumAreaNumber <= 0) ImGui::TextDisabled(u8"該当するエリアがありません。");
        if (maximumAreaNumber > 0 && ImGui::Button(u8"指定エリアへワープ"))
            DebugWarpToArea(warpDepth, warpSublayer, warpAreaNumber);
#endif
    }
    ImGui::Separator();

    if (ImGui::Button(u8"調整値を保存"))
    {
        ClampDebugPlayerParams();
        NormalizeCameraSettings();
        ShowCenterNotification(SaveDebugPlayerParams()
            ? u8"プレイテスト調整を保存しました。"
            : u8"プレイテスト調整を保存できませんでした。");
    }
    ImGui::SameLine();
    if (ImGui::Button(u8"初期値に戻す"))
    {
        ResetDebugPlayerParams();
    }
    ImGui::Separator();

    // 調整値は毎フレームクランプされますが、UI操作直後にも即座に丸めます。
    if (ImGui::CollapsingHeader(u8"移動", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 通常移動、走り、ロープ昇降の速度を調整します。
        ImGui::SliderFloat(u8"通常移動速度", &m_debugPlayerParams.walkSpeed, 0.0f, 10.0f, "%.2f");
        ImGui::SliderFloat(u8"走り速度", &m_debugPlayerParams.runSpeed, 0.0f, 15.0f, "%.2f");
        ImGui::SliderFloat(u8"ロープ昇降速度", &m_debugPlayerParams.ropeSpeed, 0.0f, 10.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"戦闘", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 攻撃1回あたりのダメージと消費スタミナを調整します。
        ImGui::Text(u8"基礎攻撃力: %.0f  実効攻撃力: %.0f", kPlayerBaseAttack, GetAttackPower());
        ImGui::SliderFloat(u8"攻撃スタミナ消費", &m_debugPlayerParams.attackCost, 0.0f, 100.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"スタミナ", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 各行動のスタミナ消費量と自然回復速度を調整します。
        ImGui::SliderFloat(u8"走り秒間消費", &m_debugPlayerParams.runCostPerSecond, 0.0f, 30.0f, "%.2f");
        ImGui::SliderFloat(u8"ロープ秒間消費", &m_debugPlayerParams.ropeCostPerSecond, 0.0f, 30.0f, "%.2f");
        ImGui::SliderFloat(u8"採掘消費", &m_debugPlayerParams.miningCost, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat(u8"回避消費", &m_debugPlayerParams.stepCost, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat(u8"ジャンプ消費", &m_debugPlayerParams.jumpCost, 0.0f, 100.0f, "%.2f");
        ImGui::SliderFloat(u8"回復速度", &m_debugPlayerParams.staminaRecoverPerSecond, 0.0f, 30.0f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"描画", ImGuiTreeNodeFlags_DefaultOpen))
    {
        // 現在深度より上にあるレイヤーの透明度を調整します。0に近いほど見えなくなります。
        ImGui::SliderFloat(u8"上層レイヤー透明度", &m_debugPlayerParams.upperLayerAlpha, 0.0f, 0.30f, "%.2f");
    }

    if (ImGui::CollapsingHeader(u8"カメラ", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::SliderFloat(u8"仰角下限（真横=0度）", &m_cameraMinPitchDegrees, kCameraMinPitchDegrees, kCameraMaxPitchDegrees, "%.1f度");
        ImGui::SliderFloat(u8"仰角上限（真上=90度）", &m_cameraMaxPitchDegrees, kCameraMinPitchDegrees, kCameraMaxPitchDegrees, "%.1f度");
        ImGui::SliderFloat(u8"プレイヤーとの距離", &m_cameraDistance,
            kCameraMinDistance, kCameraMaxDistance, "%.2f");
        NormalizeCameraSettings();
    }

    ClampDebugPlayerParams();

    // プレイテスト専用の調整ウィンドウを閉じます。
    ImGui::End();
}

#if defined(_DEBUG) && !defined(NARAKU_EDITOR_BUILD)
void SceneNarakuProto::DebugAdvanceGameDate(int weekday, int hour, int minute)
{
    weekday = std::max(0, std::min(6, weekday));
    hour = std::max(0, std::min(23, hour));
    minute = std::max(0, std::min(59, minute));
    const double targetSeconds = static_cast<double>(weekday) * kGameDaySeconds +
        static_cast<double>(hour * 3600 + minute * 60);
    const double currentSeconds = std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds);
    const bool entersNextWeek = targetSeconds <= currentSeconds;
    m_gameWeekSeconds = targetSeconds;
    if (entersNextWeek) BeginNewGameWeek();
    else ShowCenterNotification(u8"ゲーム内日時を変更しました。");
    SaveProgress();
}

bool SceneNarakuProto::DebugWarpToArea(int depth, int sublayer, int areaNumber)
{
    const auto destination = std::find_if(
        m_areas.begin(), m_areas.end(),
        [depth, sublayer, areaNumber](const AreaState& area)
        {
            return area.depth == depth && area.sublayer == sublayer && area.areaNumber == areaNumber;
        });
    if (destination == m_areas.end())
    {
        ShowCenterNotification(u8"指定したワープ先がありません。");
        return false;
    }

    const int destinationAreaIndex = static_cast<int>(std::distance(m_areas.begin(), destination));
    const int sourceAreaIndex = m_currentAreaIndex;
    if (m_cookingTarget != CookingTarget::None)
        CancelCooking(u8"デバッグワープを実行したため調理を中断しました。料理セットの使用回数は戻りません。");
    SaveCurrentAreaState();

    if (!m_areas[static_cast<std::size_t>(destinationAreaIndex)].generated)
    {
        std::string error;
        if (!GeneratePlannedArea(destinationAreaIndex, error))
        {
            if (sourceAreaIndex >= 0 && sourceAreaIndex < static_cast<int>(m_areas.size()) &&
                m_areas[static_cast<std::size_t>(sourceAreaIndex)].generated)
            {
                ActivateArea(sourceAreaIndex, false);
            }
            ReportGenerationFailure(u8"デバッグワープ先を生成できませんでした。", error);
            ShowCenterNotification(u8"デバッグワープに失敗しました。詳細を表示します。");
            return false;
        }
    }
    else
    {
        ActivateArea(destinationAreaIndex, false);
    }

    m_player.pos = m_startPoint;
    m_player.depth = m_startDepth;
    for (const LayerGateState& gate : m_layerGates)
    {
        if (!gate.isEntry) continue;
        m_player.pos = gate.ropePos;
        m_player.depth = gate.depth;
        break;
    }
    m_player.previousDepth = m_player.depth;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.previousWorldY = m_player.feetWorldY;
    m_player.grounded = true;
    m_player.verticalSpeed = 0.0f;
    m_player.airTime = 0.0f;
    m_player.attackTimer = 0.0f;
    m_player.stepTimer = 0.0f;
    m_player.knockbackTimer = 0.0f;
    m_player.knockbackVelocity = {};
    m_player.landingRecoveryTimer = 0.0f;
    m_player.lastSafeGroundPos = m_player.pos;
    m_player.lastSafeGroundDepth = m_player.depth;
    m_player.hasSafeGroundPos = true;
    m_player.blockedCellAirTime = 0.0f;
    m_player.blockedCellVelocity = {};
    m_player.onRope = false;
    m_activeRope = -1;
    m_miningIndex = -1;
    m_mode = Mode::Explore;

    std::ostringstream message;
    message << u8"第" << depth << u8"層（" << GetSublayerName(sublayer)
        << u8"）エリア" << areaNumber << u8"へワープしました。";
    ShowCenterNotification(message.str());
    return true;
}
#endif

#endif
