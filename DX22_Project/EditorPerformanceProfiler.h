#pragma once

#include <chrono>
#include <string>

/**
 * @brief Editor専用のCPU処理時間を集計してImGuiへ表示します。
 */
class EditorPerformanceProfiler
{
public:
    /** @brief 計測対象の分類です。 */
    enum class Category
    {
        Window,
        Class,
        Function,
    };

    /** @brief 現在フレームの集計を開始し、直前フレームの値を確定します。 */
    static void BeginFrame();

    /** @brief 指定した分類と名前へ計測値を加算します。 */
    static void Record(Category category, const char* name, double elapsedMilliseconds);

    /** @brief 同一クラスの最外側呼び出しかを判定し、クラス呼び出し深度を増やします。 */
    static bool EnterClass(const std::string& className);

    /** @brief クラス呼び出し深度を減らします。 */
    static void ExitClass(const std::string& className);

    /** @brief Editor処理時間ウィンドウを描画します。F3でも表示状態を切り替えられます。 */
    static void DrawWindow();

private:
    EditorPerformanceProfiler() = delete;
};

/**
 * @brief スコープを抜けるまでのCPU経過時間を自動記録します。
 */
class EditorPerformanceScope
{
public:
    EditorPerformanceScope(EditorPerformanceProfiler::Category category, const char* name);
    ~EditorPerformanceScope();

    EditorPerformanceScope(const EditorPerformanceScope&) = delete;
    EditorPerformanceScope& operator=(const EditorPerformanceScope&) = delete;

private:
    using Clock = std::chrono::steady_clock;

    EditorPerformanceProfiler::Category m_category;
    const char* m_name;
    std::string m_className;
    Clock::time_point m_startedAt;
    bool m_recordsClass = false;
};

#define EDITOR_PROFILE_JOIN_IMPL(left, right) left##right
#define EDITOR_PROFILE_JOIN(left, right) EDITOR_PROFILE_JOIN_IMPL(left, right)

/** @brief 現在の関数をEditor関数として計測します。 */
#define EDITOR_PROFILE_FUNCTION() \
    EditorPerformanceScope EDITOR_PROFILE_JOIN(editorFunctionScope_, __LINE__)( \
        EditorPerformanceProfiler::Category::Function, __FUNCTION__)

/** @brief 指定したImGuiウィンドウの描画処理を計測します。 */
#define EDITOR_PROFILE_WINDOW(name) \
    EditorPerformanceScope EDITOR_PROFILE_JOIN(editorWindowScope_, __LINE__)( \
        EditorPerformanceProfiler::Category::Window, name)
