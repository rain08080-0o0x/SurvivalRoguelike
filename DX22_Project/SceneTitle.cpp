#include "SceneTitle.h"
#include "SceneManager.h"
#include "Input.h"
#include "Transfer.h"
#include "Main.h"
#include "UIObject.h"
#include "Defines.h"
#include "imgui.h"

#include <cstdio>

namespace
{
    constexpr int kTitleMenuStart = 0;
    constexpr int kTitleMenuOption = 1;
    constexpr int kTitleMenuExit = 2;

    constexpr int kOptionRowMaster = 0;
    constexpr int kOptionRowBgm = 1;
    constexpr int kOptionRowSe = 2;
    constexpr int kOptionRowDisplay = 3;
    constexpr int kOptionRowKeyConfig = 4;
    constexpr int kOptionRowEffectDebug = 5;
    constexpr int kOptionRowBack = 6;
    constexpr int kOptionRowCount = 7;

    constexpr float kVolumeStep = 0.05f;
    constexpr int kDifficultyChoiceCount = 3;
    constexpr float kDifficultyFrameWidth = 760.0f;
    constexpr float kDifficultyFrameHeight = 500.0f;
    constexpr float kDifficultyButtonWidth = 420.0f;
    constexpr float kDifficultyButtonHeight = 92.0f;
    constexpr float kDifficultyBackWidth = 240.0f;
    constexpr float kDifficultyBackHeight = 68.0f;
    constexpr float kHoveredScale = 1.12f;
    constexpr float kDifficultyPanelOffsetY = kDifficultyFrameHeight * 0.5f;
    constexpr int kPreparationRowWeapon = 0;
    constexpr int kPreparationRowSkill1 = 1;
    constexpr int kPreparationRowSkill2 = 2;
    constexpr int kPreparationRowStart = 3;
    constexpr int kPreparationRowBack = 4;
    constexpr int kPreparationRowCount = 5;

    struct KeyConfigEntry
    {
        const char* label;
        int Transfer::InputConfig::*member;
        const char* padLabel;
    };

    const KeyConfigEntry kKeyConfigEntries[] =
    {
        { u8"Move Up",    &Transfer::InputConfig::moveUp,    "LeftStick Up / DPad Up" },
        { u8"Move Down",  &Transfer::InputConfig::moveDown,  "LeftStick Down / DPad Down" },
        { u8"Move Left",  &Transfer::InputConfig::moveLeft,  "LeftStick Left / DPad Left" },
        { u8"Move Right", &Transfer::InputConfig::moveRight, "LeftStick Right / DPad Right" },
        { u8"Dash",       &Transfer::InputConfig::dash,      "A" },
        { u8"Attack",     &Transfer::InputConfig::attack,    "B" },
        { u8"Skill 1",    &Transfer::InputConfig::skill1,    "Y" },
        { u8"Skill 2",    &Transfer::InputConfig::skill2,    "X" },
        { u8"Reroll",     &Transfer::InputConfig::reroll,    "Y" }
    };
    constexpr int kKeyConfigEntryCount = static_cast<int>(sizeof(kKeyConfigEntries) / sizeof(kKeyConfigEntries[0]));
    constexpr int kKeyConfigRowReset = kKeyConfigEntryCount;
    constexpr int kKeyConfigRowBack = kKeyConfigEntryCount + 1;
    constexpr int kKeyConfigRowCount = kKeyConfigEntryCount + 2;

    const char* GetPreparationWeaponLabel(int weaponType)
    {
        switch (weaponType)
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
            return u8"\u4E00\u6483\u578B";
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            return u8"\u624B\u6570\u578B";
        case Transfer::RoguelikeUpgrade::WeaponRanged:
            return u8"\u9060\u8DDD\u96E2\u578B";
        case Transfer::RoguelikeUpgrade::WeaponBasic:
        default:
            return u8"\u57FA\u672C\u578B";
        }
    }

    const char* GetPreparationWeaponNote(int weaponType)
    {
        switch (weaponType)
        {
        case Transfer::RoguelikeUpgrade::WeaponHeavy:
            return u8"\u5E83\u3081\u306E\u8FD1\u63A5\u653B\u6483\u3002\u653B\u6483\u901F\u5EA6\u306F\u9045\u3081\u3067\u3001\u5F37\u3044\u30CE\u30C3\u30AF\u30D0\u30C3\u30AF\u304C\u767A\u751F\u3002CRT\u306F2\u56DE\u3054\u3068\u3002";
        case Transfer::RoguelikeUpgrade::WeaponRapid:
            return u8"\u3084\u3084\u77ED\u3044\u8FD1\u63A5\u653B\u6483\u3002\u653B\u6483\u901F\u5EA6\u304C\u304B\u306A\u308A\u901F\u3044\u3002CRT\u306F2\u56DE\u3054\u3068\u3002";
        case Transfer::RoguelikeUpgrade::WeaponRanged:
            return u8"\u6700\u3082\u5F37\u3044\u6575\u3078\u8CAB\u901A\u5F3E\u3092\u6483\u3064\u3002\u521D\u671F3\u30B9\u30C8\u30C3\u30AF\u30011\u79D2\u30671\u56DE\u5FA9\u3001CRT\u306F\u6BCE\u56DE\u767A\u751F\u3002";
        case Transfer::RoguelikeUpgrade::WeaponBasic:
        default:
            return u8"\u524D\u65B9\u3078\u6A19\u6E96\u7684\u306A\u8FD1\u63A5\u653B\u6483\u3002\u6271\u3044\u3084\u3059\u3044\u57FA\u6E96\u6B66\u5668\u3002CRT\u306F3\u56DE\u3054\u3068\u3002";
        }
    }

    const char* GetPreparationSkillLabel(int skillType)
    {
        switch (skillType)
        {
        case Transfer::RoguelikeUpgrade::ActionSkillWhirl:
            return u8"\u8599\u304E\u6255\u3044";
        case Transfer::RoguelikeUpgrade::ActionSkillRush:
            return u8"\u7A81\u9032";
        case Transfer::RoguelikeUpgrade::ActionSkillAmbush:
            return u8"\u5947\u8972";
        case Transfer::RoguelikeUpgrade::ActionSkillChainThrow:
            return u8"\u9396\u6295\u3052";
        case Transfer::RoguelikeUpgrade::ActionSkillFireball:
            return u8"\u706B\u7403";
        case Transfer::RoguelikeUpgrade::ActionSkillBloodSlash:
            return u8"\u51FA\u8840\u65AC";
        case Transfer::RoguelikeUpgrade::ActionSkillNone:
        default:
            return u8"\u306A\u3057";
        }
    }

    const char* GetPreparationSkillNote(int skillType)
    {
        switch (skillType)
        {
        case Transfer::RoguelikeUpgrade::ActionSkillWhirl:
            return u8"\u81EA\u5206\u4E2D\u5FC3\u306E\u5168\u65B9\u4F4D\u653B\u6483\u3002";
        case Transfer::RoguelikeUpgrade::ActionSkillRush:
            return u8"\u5411\u3044\u3066\u3044\u308B\u65B9\u5411\u3078\u7A81\u9032\u3057\u3001\u9032\u8DEF\u4E0A\u306E\u6575\u3092\u653B\u6483\u3002";
        case Transfer::RoguelikeUpgrade::ActionSkillAmbush:
            return u8"\u5F37\u6575\u306E\u80CC\u5F8C\u3078\u56DE\u308A\u8FBC\u307F\u3001\u5C0F\u7BC4\u56F2\u3092\u653B\u6483\u3002";
        case Transfer::RoguelikeUpgrade::ActionSkillChainThrow:
            return u8"\u4E00\u5B9A\u7BC4\u56F2\u5185\u306E\u6575\u3059\u3079\u3066\u3092\u653B\u6483\u3002";
        case Transfer::RoguelikeUpgrade::ActionSkillFireball:
            return u8"\u5F37\u6575\u3078\u5411\u3051\u3066\u706B\u7403\u3092\u653E\u3064\u3002";
        case Transfer::RoguelikeUpgrade::ActionSkillBloodSlash:
            return u8"\u524D\u65B9\u3078\u92ED\u3044\u65AC\u6483\u3092\u653E\u3064\u3002";
        case Transfer::RoguelikeUpgrade::ActionSkillNone:
        default:
            return u8"\u306A\u3057";
        }
    }

    int CyclePreparationSkillType(int currentSkill, int direction, int blockedSkill)
    {
        const int skillCount = Transfer::RoguelikeUpgrade::ActionSkillTypeCount - 1;
        if (skillCount <= 0)
        {
            return Transfer::RoguelikeUpgrade::ActionSkillWhirl;
        }

        int index = currentSkill - 1;
        if (index < 0 || index >= skillCount)
        {
            index = 0;
        }

        const int step = (direction < 0) ? -1 : 1;
        for (int attempt = 0; attempt < skillCount; ++attempt)
        {
            index += step;
            if (index < 0)
            {
                index += skillCount;
            }
            else if (index >= skillCount)
            {
                index -= skillCount;
            }
            const int nextSkill = index + 1;
            if (nextSkill != blockedSkill)
            {
                return nextSkill;
            }
        }
        return Transfer::RoguelikeUpgrade::ActionSkillWhirl;
    }

    void DrawPreparationOverlay(const Transfer& tran, int selectedRow, int weaponType, int skill1Type, int skill2Type)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport)
        {
            return;
        }

        ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
        if (!drawList)
        {
            return;
        }

        const float uiScale = (tran.gameplayDebug.pauseMenuUiScale < 0.75f) ? 0.75f
            : (tran.gameplayDebug.pauseMenuUiScale > 2.0f ? 2.0f : tran.gameplayDebug.pauseMenuUiScale);
        const ImVec2 panelSize(900.0f * uiScale, 610.0f * uiScale);
        const ImVec2 panelMin(
            viewport->Pos.x + (viewport->Size.x - panelSize.x) * 0.5f,
            viewport->Pos.y + (viewport->Size.y - panelSize.y) * 0.5f);
        const ImVec2 panelMax(panelMin.x + panelSize.x, panelMin.y + panelSize.y);
        const float corner = 18.0f * uiScale;
        const float rowHeight = 64.0f * uiScale;
        const float firstRowY = panelMin.y + 166.0f * uiScale;

        drawList->AddRectFilled(
            viewport->Pos,
            ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
            IM_COL32(0, 0, 0, 152));
        drawList->AddRectFilled(panelMin, panelMax, IM_COL32(20, 26, 34, 238), corner);
        drawList->AddRect(panelMin, panelMax, IM_COL32(164, 196, 224, 220), corner, 0, 2.0f);

        ImFont* font = ImGui::GetFont();
        const float baseFontSize = ImGui::GetFontSize();
        const float titleFontSize = baseFontSize * 1.55f * uiScale;
        const float headerFontSize = baseFontSize * 1.02f * uiScale;
        const float rowFontSize = baseFontSize * 1.05f * uiScale;
        const float noteFontSize = baseFontSize * 0.92f * uiScale;

        drawList->AddText(font, titleFontSize, ImVec2(panelMin.x + 28.0f * uiScale, panelMin.y + 26.0f * uiScale),
            IM_COL32(245, 247, 252, 255), u8"\u6E96\u5099\u30D5\u30A7\u30FC\u30BA");
        drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 30.0f * uiScale, panelMin.y + 70.0f * uiScale),
            IM_COL32(196, 208, 224, 255), u8"\u4E0A\u4E0B: \u9805\u76EE\u79FB\u52D5  \u5DE6\u53F3: \u5185\u5BB9\u5909\u66F4  Enter: \u958B\u59CB  Esc: \u623B\u308B");
        drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 30.0f * uiScale, panelMin.y + 94.0f * uiScale),
            IM_COL32(196, 208, 224, 255), u8"\u30E9\u30F3\u958B\u59CB\u524D\u306B\u6B66\u5668\u30921\u3064\u3001\u30B9\u30AD\u30EB\u30922\u3064\u9078\u629E\u3057\u307E\u3059\u3002");
        drawList->AddLine(
            ImVec2(panelMin.x + 28.0f * uiScale, panelMin.y + 126.0f * uiScale),
            ImVec2(panelMax.x - 28.0f * uiScale, panelMin.y + 126.0f * uiScale),
            IM_COL32(112, 132, 154, 190),
            1.5f);
        drawList->AddText(font, headerFontSize, ImVec2(panelMin.x + 32.0f * uiScale, panelMin.y + 136.0f * uiScale),
            IM_COL32(210, 220, 236, 255), u8"\u9805\u76EE");
        drawList->AddText(font, headerFontSize, ImVec2(panelMin.x + 246.0f * uiScale, panelMin.y + 136.0f * uiScale),
            IM_COL32(210, 220, 236, 255), u8"\u9078\u629E\u5185\u5BB9");
        drawList->AddText(font, headerFontSize, ImVec2(panelMin.x + 560.0f * uiScale, panelMin.y + 136.0f * uiScale),
            IM_COL32(210, 220, 236, 255), u8"\u8AAC\u660E");

        const char* labels[kPreparationRowCount] =
        {
            u8"\u6B66\u5668",
            u8"\u30B9\u30AD\u30EB1",
            u8"\u30B9\u30AD\u30EB2",
            u8"\u958B\u59CB",
            u8"\u623B\u308B"
        };
        const char* values[kPreparationRowCount] =
        {
            GetPreparationWeaponLabel(weaponType),
            GetPreparationSkillLabel(skill1Type),
            GetPreparationSkillLabel(skill2Type),
            u8"\u3053\u306E\u69CB\u6210\u3067\u958B\u59CB",
            u8"\u96E3\u6613\u5EA6\u9078\u629E\u3078\u623B\u308B"
        };
        const char* notes[kPreparationRowCount] =
        {
            GetPreparationWeaponNote(weaponType),
            GetPreparationSkillNote(skill1Type),
            GetPreparationSkillNote(skill2Type),
            u8"\u73FE\u5728\u306E\u6B66\u5668\u3068\u30B9\u30AD\u30EB\u69CB\u6210\u3067\u30B2\u30FC\u30E0\u3092\u958B\u59CB\u3057\u307E\u3059\u3002",
            u8"\u9078\u629E\u5185\u5BB9\u3092\u4FDD\u6301\u3057\u305F\u307E\u307E\u96E3\u6613\u5EA6\u9078\u629E\u3078\u623B\u308A\u307E\u3059\u3002"
        };

        for (int row = 0; row < kPreparationRowCount; ++row)
        {
            const float rowTop = firstRowY + rowHeight * static_cast<float>(row);
            const ImVec2 rowMin(panelMin.x + 24.0f * uiScale, rowTop);
            const ImVec2 rowMax(panelMax.x - 24.0f * uiScale, rowTop + rowHeight - 6.0f * uiScale);
            const bool selected = (selectedRow == row);
            const ImU32 fillColor = selected ? IM_COL32(36, 92, 164, 236) : IM_COL32(34, 40, 52, 220);
            drawList->AddRectFilled(rowMin, rowMax, fillColor, 10.0f * uiScale);
            drawList->AddRect(rowMin, rowMax,
                selected ? IM_COL32(240, 244, 255, 220) : IM_COL32(92, 104, 124, 150),
                10.0f * uiScale, 0, selected ? 2.0f : 1.0f);

            drawList->AddText(font, rowFontSize, ImVec2(panelMin.x + 34.0f * uiScale, rowTop + 14.0f * uiScale),
                IM_COL32(244, 246, 250, 255), labels[row]);
            drawList->AddText(font, rowFontSize, ImVec2(panelMin.x + 248.0f * uiScale, rowTop + 14.0f * uiScale),
                IM_COL32(255, 245, 190, 255), values[row]);
            drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 560.0f * uiScale, rowTop + 18.0f * uiScale),
                IM_COL32(202, 216, 232, 255), notes[row]);
        }

        char footer[256]{};
        sprintf_s(
            footer,
            u8"\u73FE\u5728\u306E\u69CB\u6210: %s / %s / %s",
            GetPreparationWeaponLabel(weaponType),
            GetPreparationSkillLabel(skill1Type),
            GetPreparationSkillLabel(skill2Type));
        drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 30.0f * uiScale, panelMax.y - 38.0f * uiScale),
            IM_COL32(196, 208, 224, 255), footer);
    }
    bool IsTitleConfirmTriggered()
    {
        return IsMenuConfirmTrigger();
    }

    int WrapIndex(int value, int count)
    {
        if (count <= 0) return 0;
        int wrapped = value % count;
        if (wrapped < 0) wrapped += count;
        return wrapped;
    }

    float ClampVolume(float value)
    {
        if (value < 0.0f) return 0.0f;
        if (value > 2.0f) return 2.0f;
        return value;
    }

    bool IsMouseOverUI(UIObject* pUI)
    {
        if (!pUI) return false;

        const POINT mousePos = GetMousePosition();
        const DirectX::XMFLOAT2 pos = pUI->GetPosition();
        const DirectX::XMFLOAT2 size = pUI->GetSize();
        const float left = pos.x - size.x * 0.5f;
        const float right = pos.x + size.x * 0.5f;
        const float top = pos.y;
        const float bottom = pos.y + size.y;
        return mousePos.x >= left && mousePos.x <= right && mousePos.y >= top && mousePos.y <= bottom;
    }

    void ApplyButtonVisual(UIObject* pUI, float baseWidth, float baseHeight, bool highlighted)
    {
        if (!pUI) return;

        const float scale = highlighted ? kHoveredScale : 1.0f;
        const float color = highlighted ? 1.0f : 0.8f;
        pUI->SetSize(baseWidth * scale, baseHeight * scale);
        pUI->SetColor(color, color, color, 1.0f);
    }

    BYTE NormalizeBindableKey(BYTE key)
    {
        if (key == VK_LSHIFT || key == VK_RSHIFT)
        {
            return VK_SHIFT;
        }
        return key;
    }

    const BYTE* GetBindableKeyList(size_t& outCount)
    {
        static const BYTE kBindableKeys[] =
        {
            'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M',
            'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',
            '0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
            VK_UP, VK_DOWN, VK_LEFT, VK_RIGHT,
            VK_SPACE, VK_RETURN, VK_LSHIFT, VK_RSHIFT, VK_TAB, VK_LCONTROL, VK_RCONTROL
        };
        outCount = sizeof(kBindableKeys) / sizeof(kBindableKeys[0]);
        return kBindableKeys;
    }

    BYTE FindTriggeredBindableKey()
    {
        size_t keyCount = 0;
        const BYTE* bindableKeys = GetBindableKeyList(keyCount);
        for (size_t i = 0; i < keyCount; ++i)
        {
            if (IsRawKeyTrigger(bindableKeys[i]))
            {
                return NormalizeBindableKey(bindableKeys[i]);
            }
        }
        return 0;
    }

    void FormatKeyLabel(BYTE key, char* out, size_t outSize)
    {
        if (!out || outSize == 0)
        {
            return;
        }

        switch (key)
        {
        case VK_UP:
            sprintf_s(out, outSize, "Up");
            return;
        case VK_DOWN:
            sprintf_s(out, outSize, "Down");
            return;
        case VK_LEFT:
            sprintf_s(out, outSize, "Left");
            return;
        case VK_RIGHT:
            sprintf_s(out, outSize, "Right");
            return;
        case VK_SPACE:
            sprintf_s(out, outSize, "Space");
            return;
        case VK_RETURN:
            sprintf_s(out, outSize, "Enter");
            return;
        case VK_SHIFT:
            sprintf_s(out, outSize, "Shift");
            return;
        case VK_TAB:
            sprintf_s(out, outSize, "Tab");
            return;
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
            sprintf_s(out, outSize, "Ctrl");
            return;
        default:
            break;
        }

        if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        {
            sprintf_s(out, outSize, "%c", key);
            return;
        }

        UINT scanCode = MapVirtualKeyA(key, MAPVK_VK_TO_VSC);
        if (scanCode != 0)
        {
            char buffer[128]{};
            if (GetKeyNameTextA(static_cast<LONG>(scanCode << 16), buffer, static_cast<int>(sizeof(buffer))) > 0)
            {
                sprintf_s(out, outSize, "%s", buffer);
                return;
            }
        }

        sprintf_s(out, outSize, "VK_%u", static_cast<unsigned int>(key));
    }

    void AssignKeyBinding(Transfer::InputConfig& input, int Transfer::InputConfig::*member, BYTE newKey)
    {
        if (!member || newKey == 0)
        {
            return;
        }

        const int previousKey = input.*member;
        if (previousKey == static_cast<int>(newKey))
        {
            return;
        }

        for (int i = 0; i < kKeyConfigEntryCount; ++i)
        {
            if (kKeyConfigEntries[i].member == member)
            {
                continue;
            }
            if (input.*(kKeyConfigEntries[i].member) == static_cast<int>(newKey))
            {
                input.*(kKeyConfigEntries[i].member) = previousKey;
                break;
            }
        }
        input.*member = newKey;
    }

    void DrawKeyConfigOverlay(const Transfer& tran, int selectedRow, bool capturing)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport)
        {
            return;
        }

        ImDrawList* drawList = ImGui::GetForegroundDrawList(viewport);
        if (!drawList)
        {
            return;
        }

        const float uiScale = (tran.gameplayDebug.pauseMenuUiScale < 0.75f) ? 0.75f
            : (tran.gameplayDebug.pauseMenuUiScale > 2.0f ? 2.0f : tran.gameplayDebug.pauseMenuUiScale);
        const ImVec2 panelSize(840.0f * uiScale, 620.0f * uiScale);
        const ImVec2 panelMin(
            viewport->Pos.x + (viewport->Size.x - panelSize.x) * 0.5f,
            viewport->Pos.y + (viewport->Size.y - panelSize.y) * 0.5f);
        const ImVec2 panelMax(panelMin.x + panelSize.x, panelMin.y + panelSize.y);
        const float corner = 18.0f * uiScale;
        const float rowHeight = 42.0f * uiScale;
        const float actionX = panelMin.x + 32.0f * uiScale;
        const float keyX = panelMin.x + 360.0f * uiScale;
        const float padX = panelMin.x + 575.0f * uiScale;
        const float firstRowY = panelMin.y + 150.0f * uiScale;

        drawList->AddRectFilled(
            viewport->Pos,
            ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
            IM_COL32(0, 0, 0, 150));
        drawList->AddRectFilled(panelMin, panelMax, IM_COL32(20, 26, 34, 235), corner);
        drawList->AddRect(panelMin, panelMax, IM_COL32(164, 196, 224, 220), corner, 0, 2.0f);

        ImFont* font = ImGui::GetFont();
        const float baseFontSize = ImGui::GetFontSize();
        const float titleFontSize = baseFontSize * 1.55f * uiScale;
        const float headerFontSize = baseFontSize * 1.05f * uiScale;
        const float rowFontSize = baseFontSize * 0.98f * uiScale;
        const float noteFontSize = baseFontSize * 0.92f * uiScale;

        drawList->AddText(font, titleFontSize, ImVec2(panelMin.x + 28.0f * uiScale, panelMin.y + 24.0f * uiScale),
            IM_COL32(245, 247, 252, 255), u8"Key Config");
        drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 30.0f * uiScale, panelMin.y + 70.0f * uiScale),
            IM_COL32(196, 208, 224, 255), u8"Enter / F / Space: change   Esc / Start: back");
        drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 30.0f * uiScale, panelMin.y + 94.0f * uiScale),
            IM_COL32(196, 208, 224, 255), u8"Esc is fixed. Controller bindings are fixed and shown as reference.");

        drawList->AddLine(
            ImVec2(panelMin.x + 28.0f * uiScale, panelMin.y + 126.0f * uiScale),
            ImVec2(panelMax.x - 28.0f * uiScale, panelMin.y + 126.0f * uiScale),
            IM_COL32(112, 132, 154, 190),
            1.5f);
        drawList->AddText(font, headerFontSize, ImVec2(actionX, panelMin.y + 132.0f * uiScale),
            IM_COL32(210, 220, 236, 255), u8"Action");
        drawList->AddText(font, headerFontSize, ImVec2(keyX, panelMin.y + 132.0f * uiScale),
            IM_COL32(210, 220, 236, 255), u8"Keyboard");
        drawList->AddText(font, headerFontSize, ImVec2(padX, panelMin.y + 132.0f * uiScale),
            IM_COL32(210, 220, 236, 255), u8"Controller");

        for (int row = 0; row < kKeyConfigRowCount; ++row)
        {
            const float rowTop = firstRowY + rowHeight * static_cast<float>(row);
            const ImVec2 rowMin(panelMin.x + 24.0f * uiScale, rowTop);
            const ImVec2 rowMax(panelMax.x - 24.0f * uiScale, rowTop + rowHeight - 4.0f * uiScale);
            const bool selected = (selectedRow == row);
            const bool captureRow = capturing && selected && row < kKeyConfigEntryCount;

            ImU32 fillColor = IM_COL32(34, 40, 52, 220);
            if (selected)
            {
                fillColor = captureRow ? IM_COL32(128, 84, 28, 240) : IM_COL32(36, 92, 164, 236);
            }
            drawList->AddRectFilled(rowMin, rowMax, fillColor, 10.0f * uiScale);
            drawList->AddRect(rowMin, rowMax,
                selected ? IM_COL32(240, 244, 255, 220) : IM_COL32(92, 104, 124, 150),
                10.0f * uiScale, 0, selected ? 2.0f : 1.0f);

            if (row < kKeyConfigEntryCount)
            {
                const KeyConfigEntry& entry = kKeyConfigEntries[row];
                const int keyValue = tran.input.*(entry.member);
                char keyLabel[64]{};
                FormatKeyLabel(static_cast<BYTE>(keyValue), keyLabel, sizeof(keyLabel));
                const char* currentKeyLabel = captureRow ? "Press key..." : keyLabel;
                drawList->AddText(font, rowFontSize, ImVec2(actionX, rowTop + 10.0f * uiScale),
                    IM_COL32(244, 246, 250, 255), entry.label);
                drawList->AddText(font, rowFontSize, ImVec2(keyX, rowTop + 10.0f * uiScale),
                    IM_COL32(255, 245, 190, 255), currentKeyLabel);
                drawList->AddText(font, rowFontSize, ImVec2(padX, rowTop + 10.0f * uiScale),
                    IM_COL32(202, 216, 232, 255), entry.padLabel);
            }
            else if (row == kKeyConfigRowReset)
            {
                drawList->AddText(font, rowFontSize, ImVec2(actionX, rowTop + 10.0f * uiScale),
                    IM_COL32(244, 246, 250, 255), u8"Reset to Default");
                drawList->AddText(font, rowFontSize, ImVec2(keyX, rowTop + 10.0f * uiScale),
                    IM_COL32(202, 216, 232, 255), u8"Restore WASD / Shift / F / O / P / R");
            }
            else
            {
                drawList->AddText(font, rowFontSize, ImVec2(actionX, rowTop + 10.0f * uiScale),
                    IM_COL32(244, 246, 250, 255), u8"Back");
                drawList->AddText(font, rowFontSize, ImVec2(keyX, rowTop + 10.0f * uiScale),
                    IM_COL32(202, 216, 232, 255), u8"Return to Title Option");
            }
        }

        const char* footerText = capturing
            ? u8"Waiting for input...  Esc / Start cancels capture."
            : u8"Menu close / Pause is fixed to Esc on keyboard and Start on controller.";
        drawList->AddText(font, noteFontSize, ImVec2(panelMin.x + 30.0f * uiScale, panelMax.y - 38.0f * uiScale),
            IM_COL32(196, 208, 224, 255), footerText);
    }
}

SceneTitle::SceneTitle()
    : m_pLogo(nullptr)
    , m_pStart(nullptr)
    , m_pOption(nullptr)
    , m_pHint(nullptr)
    , m_pDifficultyFrame(nullptr)
    , m_pDifficultyEasy(nullptr)
    , m_pDifficultyNormal(nullptr)
    , m_pDifficultyHard(nullptr)
    , m_pDifficultyBack(nullptr)
    , m_menuSelection(kTitleMenuStart)
    , m_isOptionOpen(false)
    , m_optionSelection(kOptionRowMaster)
    , m_isKeyConfigOpen(false)
    , m_keyConfigSelection(0)
    , m_isKeyConfigCapturing(false)
    , m_isDifficultyOpen(false)
    , m_difficultySelection(1)
    , m_isPreparationOpen(false)
    , m_preparationSelection(kPreparationRowWeapon)
    , m_preparationWeaponType(Transfer::RoguelikeUpgrade::WeaponBasic)
    , m_preparationSkillSlots
    {
        Transfer::RoguelikeUpgrade::ActionSkillWhirl,
        Transfer::RoguelikeUpgrade::ActionSkillRush
    }
{
    m_pLogo = new UIObject("Title/Title_Logo.png", SCREEN_WIDTH * 0.5f, 210.0f, 900.0f, 380.0f);
    m_pStart = new UIObject("Title/Btn_Start.png", SCREEN_WIDTH * 0.5f, 500.0f, 360.0f, 96.0f);
    m_pOption = new UIObject("Title/Option.png", SCREEN_WIDTH * 0.5f, 600.0f, 360.0f, 96.0f);
    m_pHint = new UIObject("Title/Title_Hint.png", SCREEN_WIDTH * 0.5f, 680.0f, 300.0f, 90.0f);
    m_pDifficultyFrame = new UIObject("Game/Frame.png", SCREEN_WIDTH * 0.5f, 200.0f + kDifficultyPanelOffsetY, kDifficultyFrameWidth, kDifficultyFrameHeight);
    m_pDifficultyEasy = new UIObject("Game/Easy.png", SCREEN_WIDTH * 0.5f, 290.0f, kDifficultyButtonWidth, kDifficultyButtonHeight);
    m_pDifficultyNormal = new UIObject("Game/Normal.png", SCREEN_WIDTH * 0.5f, 395.0f, kDifficultyButtonWidth, kDifficultyButtonHeight);
    m_pDifficultyHard = new UIObject("Game/Hard.png", SCREEN_WIDTH * 0.5f, 500.0f, kDifficultyButtonWidth, kDifficultyButtonHeight);
    m_pDifficultyBack = new UIObject("Game/Back.png", SCREEN_WIDTH * 0.5f, 622.0f, kDifficultyBackWidth, kDifficultyBackHeight);

    TRAN_INS;
    tran.gameplayDebug.titleOptionOpen = 0;
    tran.gameplayDebug.titleOptionSelection = 0;
    tran.gameplayDebug.titleOptionRequestClose = 0;
    tran.gameplayDebug.titleKeyConfigOpen = 0;
    tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
    m_difficultySelection = tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset);
    tran.gameplayDebug.titleDifficultyOpen = 0;
    tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;
    tran.gameplayDebug.titlePreparationOpen = 0;
}

SceneTitle::~SceneTitle()
{
    delete m_pLogo;  m_pLogo = nullptr;
    delete m_pStart; m_pStart = nullptr;
    delete m_pOption; m_pOption = nullptr;
    delete m_pHint;  m_pHint = nullptr;
    delete m_pDifficultyFrame; m_pDifficultyFrame = nullptr;
    delete m_pDifficultyEasy; m_pDifficultyEasy = nullptr;
    delete m_pDifficultyNormal; m_pDifficultyNormal = nullptr;
    delete m_pDifficultyHard; m_pDifficultyHard = nullptr;
    delete m_pDifficultyBack; m_pDifficultyBack = nullptr;
}

void SceneTitle::Update()
{
    TRAN_INS;

    if (tran.gameplayDebug.titleOptionRequestClose != 0)
    {
        tran.gameplayDebug.titleOptionRequestClose = 0;
        m_isKeyConfigOpen = false;
        m_isKeyConfigCapturing = false;
        if (m_isOptionOpen)
        {
            m_isOptionOpen = false;
            tran.gameplayDebug.titleKeyConfigOpen = 0;
            return;
        }
    }

    if (tran.gameplayDebug.titleKeyConfigRequestOpen != 0)
    {
        tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
        if (m_isOptionOpen)
        {
            m_isKeyConfigOpen = true;
            m_keyConfigSelection = 0;
            m_isKeyConfigCapturing = false;
        }
    }

    if (m_isPreparationOpen)
    {
        tran.gameplayDebug.titleOptionOpen = 0;
        tran.gameplayDebug.titleOptionSelection = 0;
        tran.gameplayDebug.titleOptionRequestClose = 0;
        tran.gameplayDebug.titleKeyConfigOpen = 0;
        tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
        tran.gameplayDebug.titleDifficultyOpen = 0;
        tran.gameplayDebug.titlePreparationOpen = 1;

        auto startPreparedRun = [&]()
        {
            tran.roguelike.loadoutWeaponType = m_preparationWeaponType;
            tran.roguelike.loadoutSkills[0] = m_preparationSkillSlots[0];
            tran.roguelike.loadoutSkills[1] = m_preparationSkillSlots[1];
            tran.RebuildDerivedTraitLevels();
            tran.gameplayDebug.titlePreparationOpen = 0;
            tran.gameplayDebug.titleKeyConfigOpen = 0;
            tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
            tran.gameplayDebug.runElapsedSec = 0.0f;
            tran.gameplayDebug.runRecordedSec = 0.0f;
            tran.gameplayDebug.runTimerRunning = 0;
            m_isPreparationOpen = false;
            SceneManager::ChangeScene(SceneManager::SCENE_GAME);
        };

        auto closePreparationOverlay = [&]()
        {
            m_isPreparationOpen = false;
            m_isDifficultyOpen = true;
            tran.gameplayDebug.titlePreparationOpen = 0;
            tran.gameplayDebug.titleDifficultyOpen = 1;
            tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;
        };

        if (IsMenuBackTrigger())
        {
            closePreparationOverlay();
            return;
        }

        if (IsKeyTrigger(VK_UP) || IsKeyTrigger('W'))
        {
            m_preparationSelection = WrapIndex(m_preparationSelection - 1, kPreparationRowCount);
        }
        if (IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S'))
        {
            m_preparationSelection = WrapIndex(m_preparationSelection + 1, kPreparationRowCount);
        }

        const bool moveLeft = IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A');
        const bool moveRight = IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D');
        if (moveLeft || moveRight)
        {
            const int direction = moveLeft ? -1 : 1;
            switch (m_preparationSelection)
            {
            case kPreparationRowWeapon:
                m_preparationWeaponType = WrapIndex(
                    m_preparationWeaponType + direction,
                    Transfer::RoguelikeUpgrade::WeaponTypeCount);
                break;
            case kPreparationRowSkill1:
                m_preparationSkillSlots[0] = CyclePreparationSkillType(
                    m_preparationSkillSlots[0],
                    direction,
                    m_preparationSkillSlots[1]);
                break;
            case kPreparationRowSkill2:
                m_preparationSkillSlots[1] = CyclePreparationSkillType(
                    m_preparationSkillSlots[1],
                    direction,
                    m_preparationSkillSlots[0]);
                break;
            default:
                break;
            }
        }

        if (IsTitleConfirmTriggered())
        {
            if (m_preparationSelection == kPreparationRowStart)
            {
                startPreparedRun();
                return;
            }
            if (m_preparationSelection == kPreparationRowBack)
            {
                closePreparationOverlay();
                return;
            }
        }
        return;
    }

    tran.gameplayDebug.titlePreparationOpen = 0;

    if (m_isDifficultyOpen)
    {
        tran.gameplayDebug.titleOptionOpen = 0;
        tran.gameplayDebug.titleOptionSelection = 0;
        tran.gameplayDebug.titleOptionRequestClose = 0;
        tran.gameplayDebug.titleKeyConfigOpen = 0;
        tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
        tran.gameplayDebug.titleDifficultyOpen = 1;
        tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;

        auto closeDifficultyOverlay = [&]()
        {
            m_isDifficultyOpen = false;
            tran.gameplayDebug.titleDifficultyOpen = 0;
            tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;
        };

        auto startGameWithDifficulty = [&](int difficulty)
        {
            const int selectedDifficulty = tran.NormalizeDifficultyPreset(difficulty);
            tran.ApplyDifficultyPreset(selectedDifficulty);
            tran.ResetRoguelikeUpgrade();
            m_preparationWeaponType = tran.roguelike.loadoutWeaponType;
            m_preparationSkillSlots[0] = Transfer::RoguelikeUpgrade::ActionSkillWhirl;
            m_preparationSkillSlots[1] = Transfer::RoguelikeUpgrade::ActionSkillRush;
            tran.roguelike.loadoutSkills[0] = m_preparationSkillSlots[0];
            tran.roguelike.loadoutSkills[1] = m_preparationSkillSlots[1];
            tran.RebuildDerivedTraitLevels();
            tran.gameplayDebug.requestBossBattle = 0;
            tran.gameplayDebug.bossBattleActive = 0;
            tran.gameplayDebug.showBossResultTimer = 0;
            tran.gameplayDebug.runElapsedSec = 0.0f;
            tran.gameplayDebug.runRecordedSec = 0.0f;
            tran.gameplayDebug.runTimerRunning = 0;
            tran.gameplayDebug.titleDifficultyOpen = 0;
            tran.gameplayDebug.titleDifficultySelection = selectedDifficulty;
            tran.gameplayDebug.titleKeyConfigOpen = 0;
            tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
            m_isDifficultyOpen = false;
            m_isPreparationOpen = true;
            m_preparationSelection = kPreparationRowWeapon;
            tran.gameplayDebug.titlePreparationOpen = 1;
        };

        if (IsMenuBackTrigger())
        {
            closeDifficultyOverlay();
            return;
        }

        if (IsKeyTrigger(VK_UP) || IsKeyTrigger('W') || IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A'))
        {
            m_difficultySelection = WrapIndex(m_difficultySelection - 1, kDifficultyChoiceCount);
        }
        if (IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S') || IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D'))
        {
            m_difficultySelection = WrapIndex(m_difficultySelection + 1, kDifficultyChoiceCount);
        }

        const bool easyHovered = IsMouseOverUI(m_pDifficultyEasy);
        const bool normalHovered = IsMouseOverUI(m_pDifficultyNormal);
        const bool hardHovered = IsMouseOverUI(m_pDifficultyHard);
        const bool backHovered = IsMouseOverUI(m_pDifficultyBack);

        if (easyHovered)
        {
            m_difficultySelection = 0;
        }
        else if (normalHovered)
        {
            m_difficultySelection = 1;
        }
        else if (hardHovered)
        {
            m_difficultySelection = 2;
        }

        tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;

        ApplyButtonVisual(m_pDifficultyEasy, kDifficultyButtonWidth, kDifficultyButtonHeight, m_difficultySelection == 0 || easyHovered);
        ApplyButtonVisual(m_pDifficultyNormal, kDifficultyButtonWidth, kDifficultyButtonHeight, m_difficultySelection == 1 || normalHovered);
        ApplyButtonVisual(m_pDifficultyHard, kDifficultyButtonWidth, kDifficultyButtonHeight, m_difficultySelection == 2 || hardHovered);
        ApplyButtonVisual(m_pDifficultyBack, kDifficultyBackWidth, kDifficultyBackHeight, backHovered);

        if (IsMouseLeftTrigger())
        {
            if (backHovered)
            {
                closeDifficultyOverlay();
                return;
            }
            if (easyHovered)
            {
                startGameWithDifficulty(0);
                return;
            }
            if (normalHovered)
            {
                startGameWithDifficulty(1);
                return;
            }
            if (hardHovered)
            {
                startGameWithDifficulty(2);
                return;
            }
        }

        if (IsTitleConfirmTriggered())
        {
            startGameWithDifficulty(m_difficultySelection);
            return;
        }
        return;
    }

    tran.gameplayDebug.titleDifficultyOpen = 0;
    tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;

    if (m_isKeyConfigOpen)
    {
        tran.gameplayDebug.titleOptionOpen = 1;
        tran.gameplayDebug.titleOptionSelection = m_optionSelection;
        tran.gameplayDebug.titleKeyConfigOpen = 1;

        if (IsMenuBackTrigger())
        {
            m_isKeyConfigOpen = false;
            tran.gameplayDebug.titleKeyConfigOpen = 0;
            return;
        }
        return;
    }

    tran.gameplayDebug.titleKeyConfigOpen = 0;

    if (m_isOptionOpen)
    {
        tran.gameplayDebug.titleOptionOpen = 1;
        tran.gameplayDebug.titleOptionSelection = m_optionSelection;

        if (IsMenuBackTrigger())
        {
            m_isOptionOpen = false;
            return;
        }

        if (IsKeyTrigger(VK_UP) || IsKeyTrigger('W'))
        {
            m_optionSelection = WrapIndex(m_optionSelection - 1, kOptionRowCount);
        }
        if (IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S'))
        {
            m_optionSelection = WrapIndex(m_optionSelection + 1, kOptionRowCount);
        }

        const bool decrease = IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A');
        const bool increase = IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D');
        if (decrease || increase)
        {
            const float delta = decrease ? -kVolumeStep : kVolumeStep;
            switch (m_optionSelection)
            {
            case kOptionRowMaster:
                tran.gameplay.volumeMaster = ClampVolume(tran.gameplay.volumeMaster + delta);
                break;
            case kOptionRowBgm:
                tran.gameplay.volumeBgm = ClampVolume(tran.gameplay.volumeBgm + delta);
                break;
            case kOptionRowSe:
                tran.gameplay.volumeSe = ClampVolume(tran.gameplay.volumeSe + delta);
                break;
            case kOptionRowDisplay:
                SetAppFullscreen(increase);
                break;
            default:
                break;
            }
        }

        if (IsTitleConfirmTriggered())
        {
            if (m_optionSelection == kOptionRowDisplay)
            {
                ToggleAppFullscreen();
            }
            else if (m_optionSelection == kOptionRowKeyConfig)
            {
                m_isKeyConfigOpen = true;
                m_keyConfigSelection = 0;
                m_isKeyConfigCapturing = false;
            }
            else if (m_optionSelection == kOptionRowEffectDebug)
            {
                m_isOptionOpen = false;
                m_isKeyConfigOpen = false;
                m_isKeyConfigCapturing = false;
                tran.gameplayDebug.titleOptionOpen = 0;
                tran.gameplayDebug.titleOptionSelection = 0;
                tran.gameplayDebug.titleOptionRequestClose = 0;
                tran.gameplayDebug.titleKeyConfigOpen = 0;
                tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
                SceneManager::ChangeScene(SceneManager::SCENE_EFFECT_DEBUG);
            }
            else if (m_optionSelection == kOptionRowBack)
            {
                m_isOptionOpen = false;
            }
        }
        return;
    }

    tran.gameplayDebug.titleOptionOpen = 0;
    tran.gameplayDebug.titleOptionSelection = 0;
    tran.gameplayDebug.titleOptionRequestClose = 0;
    tran.gameplayDebug.titleKeyConfigOpen = 0;
    tran.gameplayDebug.titleKeyConfigRequestOpen = 0;

    if (IsKeyTrigger(VK_ESCAPE))
    {
        PostQuitMessage(0);
        return;
    }

    if (IsKeyTrigger(VK_UP) || IsKeyTrigger('W') || IsKeyTrigger(VK_LEFT) || IsKeyTrigger('A'))
    {
        m_menuSelection = WrapIndex(m_menuSelection - 1, 3);
    }
    if (IsKeyTrigger(VK_DOWN) || IsKeyTrigger('S') || IsKeyTrigger(VK_RIGHT) || IsKeyTrigger('D'))
    {
        m_menuSelection = WrapIndex(m_menuSelection + 1, 3);
    }

    const bool startSelected = (m_menuSelection == kTitleMenuStart);
    const bool optionSelected = (m_menuSelection == kTitleMenuOption);
    const bool exitSelected = (m_menuSelection == kTitleMenuExit);
    const float selectedScale = 1.18f;

    if (m_pStart)
    {
        m_pStart->SetSize(startSelected ? 360.0f * selectedScale : 360.0f,
                          startSelected ? 96.0f * selectedScale : 96.0f);
        m_pStart->SetColor(startSelected ? 1.0f : 0.65f, startSelected ? 1.0f : 0.65f, startSelected ? 1.0f : 0.65f, 1.0f);
    }
    if (m_pOption)
    {
        m_pOption->SetSize(optionSelected ? 360.0f * selectedScale : 360.0f,
                           optionSelected ? 96.0f * selectedScale : 96.0f);
        m_pOption->SetColor(optionSelected ? 1.0f : 0.65f, optionSelected ? 1.0f : 0.65f, optionSelected ? 1.0f : 0.65f, 1.0f);
    }
    if (m_pHint)
    {
        m_pHint->SetSize(exitSelected ? 300.0f * selectedScale : 300.0f,
                         exitSelected ? 90.0f * selectedScale : 90.0f);
        m_pHint->SetColor(exitSelected ? 1.0f : 0.65f, exitSelected ? 1.0f : 0.65f, exitSelected ? 1.0f : 0.65f, 1.0f);
    }

    if (IsTitleConfirmTriggered())
    {
        if (startSelected)
        {
            m_isDifficultyOpen = true;
            m_difficultySelection = tran.NormalizeDifficultyPreset(tran.gameplayDebug.difficultyPreset);
            tran.gameplayDebug.titleDifficultyOpen = 1;
            tran.gameplayDebug.titleDifficultySelection = m_difficultySelection;
            return;
        }
        if (optionSelected)
        {
            m_isOptionOpen = true;
            m_optionSelection = kOptionRowMaster;
            m_isKeyConfigOpen = false;
            m_isKeyConfigCapturing = false;
            tran.gameplayDebug.titleOptionOpen = 1;
            tran.gameplayDebug.titleOptionSelection = m_optionSelection;
            tran.gameplayDebug.titleOptionRequestClose = 0;
            tran.gameplayDebug.titleKeyConfigOpen = 0;
            tran.gameplayDebug.titleKeyConfigRequestOpen = 0;
            return;
        }
        if (exitSelected)
        {
            PostQuitMessage(0);
            return;
        }
    }
}

void SceneTitle::Draw()
{
    UIObject::Begin2D();
    if (m_pLogo)  m_pLogo->Draw();
    if (m_pStart) m_pStart->Draw();
    if (m_pOption) m_pOption->Draw();
    if (m_pHint)  m_pHint->Draw();
    if (m_isDifficultyOpen)
    {
        if (m_pDifficultyFrame) m_pDifficultyFrame->Draw();
        if (m_pDifficultyEasy) m_pDifficultyEasy->Draw();
        if (m_pDifficultyNormal) m_pDifficultyNormal->Draw();
        if (m_pDifficultyHard) m_pDifficultyHard->Draw();
        if (m_pDifficultyBack) m_pDifficultyBack->Draw();
    }
    UIObject::End2D();

    if (m_isPreparationOpen)
    {
        TRAN_INS;
        DrawPreparationOverlay(
            tran,
            m_preparationSelection,
            m_preparationWeaponType,
            m_preparationSkillSlots[0],
            m_preparationSkillSlots[1]);
    }
}


