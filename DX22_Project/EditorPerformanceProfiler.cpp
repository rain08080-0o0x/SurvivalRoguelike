#include "EditorPerformanceProfiler.h"

#include "imgui.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace
{
    constexpr double kAveragePreviousWeight = 0.85;
    constexpr double kAverageSampleWeight = 0.15;

    struct ProfileEntry
    {
        std::string name;
        double currentFrameMilliseconds = 0.0;
        double previousFrameMilliseconds = 0.0;
        double averageMilliseconds = 0.0;
        double maximumMilliseconds = 0.0;
        unsigned int currentFrameCalls = 0;
        unsigned int previousFrameCalls = 0;
    };

    using ProfileEntries = std::unordered_map<std::string, ProfileEntry>;

    ProfileEntries g_windowEntries;
    ProfileEntries g_classEntries;
    ProfileEntries g_functionEntries;
    std::unordered_map<std::string, unsigned int> g_classCallDepths;
    bool g_showProfilerWindow = true;
    bool g_showOnlyActiveEntries = true;

    ProfileEntries& GetEntries(EditorPerformanceProfiler::Category category)
    {
        // 計測分類ごとに対応する保存先を選択します。
        switch (category)
        {
        case EditorPerformanceProfiler::Category::Window:
            return g_windowEntries;
        case EditorPerformanceProfiler::Category::Class:
            return g_classEntries;
        case EditorPerformanceProfiler::Category::Function:
        default:
            return g_functionEntries;
        }
    }

    void FinalizeEntries(ProfileEntries& entries)
    {
        // 登録済みの全項目について直前フレームの統計値を確定します。
        for (auto& pair : entries)
        {
            ProfileEntry& entry = pair.second;
            entry.previousFrameMilliseconds = entry.currentFrameMilliseconds;
            entry.previousFrameCalls = entry.currentFrameCalls;

            // 初回計測はその値を平均値の初期値として使用します。
            if (entry.currentFrameCalls > 0)
            {
                if (entry.averageMilliseconds <= 0.0)
                {
                    entry.averageMilliseconds = entry.currentFrameMilliseconds;
                }
                else
                {
                    entry.averageMilliseconds =
                        entry.averageMilliseconds * kAveragePreviousWeight +
                        entry.currentFrameMilliseconds * kAverageSampleWeight;
                }
                entry.maximumMilliseconds =
                    std::max(entry.maximumMilliseconds, entry.currentFrameMilliseconds);
            }

            entry.currentFrameMilliseconds = 0.0;
            entry.currentFrameCalls = 0;
        }
    }

    void ClearEntries()
    {
        g_windowEntries.clear();
        g_classEntries.clear();
        g_functionEntries.clear();
        g_classCallDepths.clear();
    }

    void DrawEntryTable(ProfileEntries& entries, const char* tableId, ImGuiTextFilter& filter)
    {
        std::vector<const ProfileEntry*> sortedEntries;
        sortedEntries.reserve(entries.size());

        // 表示条件を満たす項目だけを一覧へ集めます。
        for (const auto& pair : entries)
        {
            const ProfileEntry& entry = pair.second;
            if (g_showOnlyActiveEntries && entry.previousFrameCalls == 0)
            {
                continue;
            }
            if (!filter.PassFilter(entry.name.c_str()))
            {
                continue;
            }
            sortedEntries.push_back(&entry);
        }

        // 重い項目を先頭に表示してボトルネックを見つけやすくします。
        std::sort(sortedEntries.begin(), sortedEntries.end(),
            [](const ProfileEntry* left, const ProfileEntry* right)
            {
                if (left->previousFrameMilliseconds != right->previousFrameMilliseconds)
                {
                    return left->previousFrameMilliseconds > right->previousFrameMilliseconds;
                }
                return left->name < right->name;
            });

        const ImGuiTableFlags tableFlags =
            ImGuiTableFlags_Borders |
            ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable |
            ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_SizingStretchProp;
        if (!ImGui::BeginTable(tableId, 5, tableFlags, ImVec2(0.0f, 0.0f)))
        {
            return;
        }

        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn(u8"名前", ImGuiTableColumnFlags_WidthStretch, 3.0f);
        ImGui::TableSetupColumn(u8"前フレーム ms", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn(u8"平均 ms", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn(u8"最大 ms", ImGuiTableColumnFlags_WidthFixed, 85.0f);
        ImGui::TableSetupColumn(u8"呼出回数", ImGuiTableColumnFlags_WidthFixed, 75.0f);
        ImGui::TableHeadersRow();

        // 絞り込みと並び替えを反映した計測結果を行単位で描画します。
        for (const ProfileEntry* entry : sortedEntries)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(entry->name.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%.4f", entry->previousFrameMilliseconds);
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4f", entry->averageMilliseconds);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.4f", entry->maximumMilliseconds);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%u", entry->previousFrameCalls);
        }

        ImGui::EndTable();
    }

    std::string ExtractClassName(const char* functionName)
    {
        if (functionName == nullptr)
        {
            return std::string();
        }

        const std::string fullName(functionName);
        const size_t separator = fullName.rfind("::");
        if (separator == std::string::npos)
        {
            return std::string();
        }
        return fullName.substr(0, separator);
    }
}

void EditorPerformanceProfiler::BeginFrame()
{
    FinalizeEntries(g_windowEntries);
    FinalizeEntries(g_classEntries);
    FinalizeEntries(g_functionEntries);
}

void EditorPerformanceProfiler::Record(Category category, const char* name, double elapsedMilliseconds)
{
    if (name == nullptr || name[0] == '\0')
    {
        return;
    }

    ProfileEntries& entries = GetEntries(category);
    ProfileEntry& entry = entries[name];
    if (entry.name.empty())
    {
        entry.name = name;
    }
    entry.currentFrameMilliseconds += elapsedMilliseconds;
    ++entry.currentFrameCalls;
}

bool EditorPerformanceProfiler::EnterClass(const std::string& className)
{
    if (className.empty())
    {
        return false;
    }

    unsigned int& depth = g_classCallDepths[className];
    const bool isOutermostCall = depth == 0;
    ++depth;
    return isOutermostCall;
}

void EditorPerformanceProfiler::ExitClass(const std::string& className)
{
    const auto found = g_classCallDepths.find(className);
    if (found == g_classCallDepths.end())
    {
        return;
    }

    // 対応する関数スコープが終了した分だけ呼び出し深度を戻します。
    if (found->second > 0)
    {
        --found->second;
    }
}

void EditorPerformanceProfiler::DrawWindow()
{
    // F3入力で閉じた計測ウィンドウも再表示できるようにします。
    if (ImGui::IsKeyPressed(ImGuiKey_F3, false))
    {
        g_showProfilerWindow = !g_showProfilerWindow;
    }
    if (!g_showProfilerWindow)
    {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(u8"Editor 処理時間", &g_showProfilerWindow))
    {
        ImGui::End();
        return;
    }

    static ImGuiTextFilter filter;
    filter.Draw(u8"絞り込み", 260.0f);
    ImGui::SameLine();
    ImGui::Checkbox(u8"前フレームで実行した項目のみ", &g_showOnlyActiveEntries);
    ImGui::SameLine();
    if (ImGui::Button(u8"統計をリセット"))
    {
        ClearEntries();
    }
    ImGui::TextDisabled(u8"F3: 表示切替 / 時間はCPU側の包括時間（1フレーム内の合計）です");

    // 分類別のタブでウィンドウ、クラス、関数の統計を切り替えます。
    if (ImGui::BeginTabBar("##EditorPerformanceTabs"))
    {
        if (ImGui::BeginTabItem(u8"ウィンドウ"))
        {
            DrawEntryTable(g_windowEntries, "##EditorWindowPerformance", filter);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"クラス"))
        {
            DrawEntryTable(g_classEntries, "##EditorClassPerformance", filter);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"関数"))
        {
            DrawEntryTable(g_functionEntries, "##EditorFunctionPerformance", filter);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

EditorPerformanceScope::EditorPerformanceScope(
    EditorPerformanceProfiler::Category category,
    const char* name)
    : m_category(category)
    , m_name(name)
    , m_startedAt(Clock::now())
{
    if (m_category == EditorPerformanceProfiler::Category::Function)
    {
        m_className = ExtractClassName(m_name);
        m_recordsClass = EditorPerformanceProfiler::EnterClass(m_className);
    }
}

EditorPerformanceScope::~EditorPerformanceScope()
{
    const Clock::time_point finishedAt = Clock::now();
    const double elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(finishedAt - m_startedAt).count();

    EditorPerformanceProfiler::Record(m_category, m_name, elapsedMilliseconds);
    if (m_category == EditorPerformanceProfiler::Category::Function)
    {
        if (m_recordsClass)
        {
            EditorPerformanceProfiler::Record(
                EditorPerformanceProfiler::Category::Class,
                m_className.c_str(),
                elapsedMilliseconds);
        }
        EditorPerformanceProfiler::ExitClass(m_className);
    }
}
