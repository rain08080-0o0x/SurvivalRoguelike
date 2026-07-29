#pragma once

#include <string>

namespace NarakuStageGenerator
{
    /**
     * @brief Completed ピース群から 3x3 固定のマップを生成して JSON 保存します。
     * @param outputMapPath 出力先マップパスです。nullptr または空文字なら既定パスを使います。
     * @param outError 失敗理由の出力先です。不要なら nullptr を指定できます。
     * @return 生成、検証、保存まで成功した場合 true を返します。
     */
    bool GenerateFixed3x3Map(const wchar_t* outputMapPath, std::string* outError = nullptr);

    /**
     * @brief Completed ピース群から 4x4 固定のマップを生成して JSON 保存します。
     * @param outputMapPath 出力先マップパスです。nullptr または空文字なら既定パスを使います。
     * @param outError 失敗理由の出力先です。不要なら nullptr を指定できます。
     * @return 生成、検証、保存まで成功した場合 true を返します。
     */
    bool GenerateFixed4x4Map(const wchar_t* outputMapPath, std::string* outError = nullptr);

    /** @brief 層入口・層出口の有無を指定して4x4エリアを生成します。 */
    bool GenerateFixed4x4AreaMap(
        const wchar_t* outputMapPath,
        bool requireLayerEntry,
        bool requireLayerExit,
        std::string* outError = nullptr);
}
