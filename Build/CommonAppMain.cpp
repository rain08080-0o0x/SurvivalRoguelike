#include "Main.h"

#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "SceneManager.h"
#include "ShaderList.h"
#include "Sprite.h"

#if defined(NARAKU_EDITOR_BUILD)
#include "EditorPerformanceProfiler.h"
#include "imgui.h"
#elif defined(_DEBUG)
#include "imgui.h"
#endif

#ifdef _DEBUG
#include "DebugUtil.h"

namespace
{
    struct FramePerfStats
    {
        float updateInputMs = 0.0f;
        float sceneUpdateMs = 0.0f;
        float updateTotalMs = 0.0f;
        float beginDrawMs = 0.0f;
        float sceneDrawMs = 0.0f;
        float overlayDrawMs = 0.0f;
        float endDrawMs = 0.0f;
        float drawTotalMs = 0.0f;
        float frameCpuMs = 0.0f;
        SceneManager::SceneType scene = SceneManager::SceneType::SCENE_TITLE;
    };

    FramePerfStats g_perfFrameWorking{};
    FramePerfStats g_perfFrameLast{};
    FramePerfStats g_perfFrameAvg{};
    bool g_showPerfOverlay = false;

    void BlendPerfValue(float& accum, float sample)
    {
        if (accum <= 0.0f)
        {
            accum = sample;
            return;
        }
        accum = accum * 0.85f + sample * 0.15f;
    }

    void CommitPerfFrame(const FramePerfStats& sample)
    {
        g_perfFrameLast = sample;
        BlendPerfValue(g_perfFrameAvg.updateInputMs, sample.updateInputMs);
        BlendPerfValue(g_perfFrameAvg.sceneUpdateMs, sample.sceneUpdateMs);
        BlendPerfValue(g_perfFrameAvg.updateTotalMs, sample.updateTotalMs);
        BlendPerfValue(g_perfFrameAvg.beginDrawMs, sample.beginDrawMs);
        BlendPerfValue(g_perfFrameAvg.sceneDrawMs, sample.sceneDrawMs);
        BlendPerfValue(g_perfFrameAvg.overlayDrawMs, sample.overlayDrawMs);
        BlendPerfValue(g_perfFrameAvg.endDrawMs, sample.endDrawMs);
        BlendPerfValue(g_perfFrameAvg.drawTotalMs, sample.drawTotalMs);
        BlendPerfValue(g_perfFrameAvg.frameCpuMs, sample.frameCpuMs);
        g_perfFrameAvg.scene = sample.scene;
    }

    const char* GetSceneProfileName(SceneManager::SceneType scene)
    {
        switch (scene)
        {
        case SceneManager::SceneType::SCENE_TITLE:              return "Title";
        case SceneManager::SceneType::SCENE_GAME:               return "Game";
        case SceneManager::SceneType::SCENE_RESULT:             return "Result";
        case SceneManager::SceneType::SCENE_ENGINE_EDITOR:      return "CastleEditor";
        case SceneManager::SceneType::SCENE_EFFECT_DEBUG:       return "EffectDebug";
        case SceneManager::SceneType::SCENE_BOSS_EDITOR:         return "BossEditor";
        case SceneManager::SceneType::SCENE_FINAL_BOSS_EDITOR:   return "FinalBossEditor";
        case SceneManager::SceneType::SCENE_NEW_BOSS_EDITOR:     return "NewLastBoss";
        case SceneManager::SceneType::SCENE_NARAKU_EDITOR:       return "NarakuEditor";
        case SceneManager::SceneType::SCENE_NARAKU_PIECE_EDITOR: return "NarakuPieceEditor";
        case SceneManager::SceneType::SCENE_NARAKU_PROTO:        return "NarakuProto";
        default:                                                 return "Unknown";
        }
    }

    void FormatHudFixedFloat(float value, char* out, size_t outSize)
    {
        float safeValue = value;
        if (safeValue < 0.0f) safeValue = 0.0f;
        if (safeValue > 999.99f) safeValue = 999.99f;
        sprintf_s(out, outSize, "%06.2f", safeValue);
    }

    ImU32 GetPerfBorderColor(float frameMs)
    {
        const float frameBudget60 = 1000.0f / 60.0f;
        if (frameMs <= frameBudget60 * 0.90f)
        {
            return IM_COL32(90, 210, 120, 180);
        }
        if (frameMs <= frameBudget60)
        {
            return IM_COL32(255, 210, 90, 180);
        }
        return IM_COL32(255, 110, 110, 180);
    }

    void DrawPerformanceOverlay()
    {
        const bool f2Pressed = ImGui::IsKeyPressed(ImGuiKey_F2, false) || IsKeyTrigger(VK_F2);
        if (f2Pressed)
        {
            g_showPerfOverlay = !g_showPerfOverlay;
        }

        if (!g_showPerfOverlay)
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImDrawList* dl = ImGui::GetForegroundDrawList(vp);
        const ImVec2 pad(8.0f, 6.0f);
        const float frameBudget60 = 1000.0f / 60.0f;
        const FramePerfStats& perfLast = g_perfFrameLast;
        const FramePerfStats& perfAvg = g_perfFrameAvg;
        const ImU32 borderColor = GetPerfBorderColor(perfAvg.frameCpuMs);

        char fpsText[16]{};
        char cpuLastText[16]{};
        char cpuAvgText[16]{};
        char budget60Text[16]{};
        char updateText[16]{};
        char updateInputText[16]{};
        char updateSceneText[16]{};
        char inputKbmText[16]{};
        char inputXiText[16]{};
        char inputDiText[16]{};
        char drawText[16]{};
        char beginDrawText[16]{};
        char sceneDrawText[16]{};
        char overlayDrawText[16]{};
        char endDrawText[16]{};

        FormatHudFixedFloat(io.Framerate, fpsText, sizeof(fpsText));
        FormatHudFixedFloat(perfLast.frameCpuMs, cpuLastText, sizeof(cpuLastText));
        FormatHudFixedFloat(perfAvg.frameCpuMs, cpuAvgText, sizeof(cpuAvgText));
        FormatHudFixedFloat(frameBudget60, budget60Text, sizeof(budget60Text));
        FormatHudFixedFloat(perfLast.updateTotalMs, updateText, sizeof(updateText));
        FormatHudFixedFloat(perfLast.updateInputMs, updateInputText, sizeof(updateInputText));
        FormatHudFixedFloat(perfLast.sceneUpdateMs, updateSceneText, sizeof(updateSceneText));
        FormatHudFixedFloat(GetInputKeyboardMouseMs(), inputKbmText, sizeof(inputKbmText));
        FormatHudFixedFloat(GetInputXInputMs(), inputXiText, sizeof(inputXiText));
        FormatHudFixedFloat(GetInputDirectInputMs(), inputDiText, sizeof(inputDiText));
        FormatHudFixedFloat(perfLast.drawTotalMs, drawText, sizeof(drawText));
        FormatHudFixedFloat(perfLast.beginDrawMs, beginDrawText, sizeof(beginDrawText));
        FormatHudFixedFloat(perfLast.sceneDrawMs, sceneDrawText, sizeof(sceneDrawText));
        FormatHudFixedFloat(perfLast.overlayDrawMs, overlayDrawText, sizeof(overlayDrawText));
        FormatHudFixedFloat(perfLast.endDrawMs, endDrawText, sizeof(endDrawText));

        char hud[768];
        sprintf_s(
            hud,
            u8"FPS %s\n"
            u8"Scene %s\n"
            u8"CPU Last %s ms / Avg %s ms / 60FPS %s ms\n"
            u8"Update %s ms (Input %s / Scene %s)\n"
            u8"Input %s ms (KBM %s / XI %s / DI %s)\n"
            u8"Draw %s ms (Begin %s / Scene %s / Overlay %s / Present %s)\n"
            u8"マウス (%.0f, %.0f) [F2で閉じる]",
            fpsText,
            GetSceneProfileName(perfLast.scene),
            cpuLastText,
            cpuAvgText,
            budget60Text,
            updateText,
            updateInputText,
            updateSceneText,
            updateInputText,
            inputKbmText,
            inputXiText,
            inputDiText,
            drawText,
            beginDrawText,
            sceneDrawText,
            overlayDrawText,
            endDrawText,
            io.MousePos.x,
            io.MousePos.y);

        const char* hudTemplate =
            u8"FPS 888.88\n"
            u8"Scene NarakuPieceEditor\n"
            u8"CPU Last 888.88 ms / Avg 888.88 ms / 60FPS 888.88 ms\n"
            u8"Update 888.88 ms (Input 888.88 / Scene 888.88)\n"
            u8"Input 888.88 ms (KBM 888.88 / XI 888.88 / DI 888.88)\n"
            u8"Draw 888.88 ms (Begin 888.88 / Scene 888.88 / Overlay 888.88 / Present 888.88)\n"
            u8"マウス (8888, 8888) [F2で閉じる]";

        const ImVec2 textSize = ImGui::CalcTextSize(hudTemplate);
        const ImVec2 boxMin(vp->Pos.x + vp->Size.x - textSize.x - pad.x * 2.0f - 10.0f, vp->Pos.y + 10.0f);
        const ImVec2 boxMax(boxMin.x + textSize.x + pad.x * 2.0f, boxMin.y + textSize.y + pad.y * 2.0f);
        dl->AddRectFilled(boxMin, boxMax, IM_COL32(0, 0, 0, 160), 4.0f);
        dl->AddRect(boxMin, boxMax, borderColor, 4.0f);
        dl->AddText(ImVec2(boxMin.x + pad.x, boxMin.y + pad.y), IM_COL32(255, 255, 255, 255), hud);
    }
}
#endif

#include <crtdbg.h>
#include <cstdlib>
#include <ctime>

HRESULT Init(HWND hWnd, UINT width, UINT height)
{
#ifdef _DEBUG
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    const HRESULT result = InitDirectX(hWnd, width, height, false);
    if (FAILED(result))
    {
        return result;
    }

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    Geometory::Init();
    Sprite::Init();
    InitInput(hWnd);
    ShaderList::Init();
    SceneManager::Init();
    return result;
}

void Uninit()
{
    SceneManager::Uninit();
    ShaderList::Uninit();
    UninitInput();
    Sprite::Uninit();
    Geometory::Uninit();
    UninitDirectX();
}

void Update()
{
#if defined(NARAKU_EDITOR_BUILD)
    EditorPerformanceProfiler::BeginFrame();
#endif

#ifdef _DEBUG
    const double updateStart = NowMS();
    double sectionStart = updateStart;
#endif

    UpdateInput();

#ifdef _DEBUG
    g_perfFrameWorking.updateInputMs = static_cast<float>(NowMS() - sectionStart);
    sectionStart = NowMS();
#endif

    SceneManager::Update();

#ifdef _DEBUG
    g_perfFrameWorking.sceneUpdateMs = static_cast<float>(NowMS() - sectionStart);
    g_perfFrameWorking.updateTotalMs = static_cast<float>(NowMS() - updateStart);
    g_perfFrameWorking.scene = SceneManager::GetCurrent();
#endif
}

void Draw()
{
#ifdef _DEBUG
    const double drawStart = NowMS();
    double sectionStart = drawStart;
#endif

    BeginDrawDirectX();

#ifdef _DEBUG
    g_perfFrameWorking.beginDrawMs = static_cast<float>(NowMS() - sectionStart);
#endif

#if defined(NARAKU_EDITOR_BUILD)
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
    }
#endif

#ifdef _DEBUG
    sectionStart = NowMS();
#endif

    SceneManager::Draw();

#ifdef _DEBUG
    g_perfFrameWorking.sceneDrawMs = static_cast<float>(NowMS() - sectionStart);
#endif

#if defined(NARAKU_EDITOR_BUILD)
    EditorPerformanceProfiler::DrawWindow();
#endif

#ifdef _DEBUG
    sectionStart = NowMS();
    DrawPerformanceOverlay();
    g_perfFrameWorking.overlayDrawMs = static_cast<float>(NowMS() - sectionStart);
    sectionStart = NowMS();
#endif

    EndDrawDirectX();

#ifdef _DEBUG
    g_perfFrameWorking.endDrawMs = static_cast<float>(NowMS() - sectionStart);
    g_perfFrameWorking.drawTotalMs = static_cast<float>(NowMS() - drawStart);
    g_perfFrameWorking.frameCpuMs = g_perfFrameWorking.updateTotalMs + g_perfFrameWorking.drawTotalMs;
    g_perfFrameWorking.scene = SceneManager::GetCurrent();
    CommitPerfFrame(g_perfFrameWorking);
#endif
}
