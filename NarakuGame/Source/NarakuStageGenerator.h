#pragma once

#include <string>

namespace NarakuMap
{
    struct MapData;
}

namespace NarakuStageGenerator
{
    struct AreaGenerationContext
    {
        int depth = 1;
        int sublayer = 0;
        int areaNumber = 1;
    };

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
        int layerEntryCount,
        int layerExitCount,
        bool requireStartReturn,
        const AreaGenerationContext* context,
        std::string* outError = nullptr);

    /** @brief 層入口・層出口の有無を指定し、ファイルへ保存せず4x4エリアを生成します。 */
    bool GenerateFixed4x4AreaMapData(
        NarakuMap::MapData& outMapData,
        int layerEntryCount,
        int layerExitCount,
        bool requireStartReturn,
        const AreaGenerationContext* context,
        std::string* outError = nullptr);

#if defined(NARAKU_EDITOR_BUILD)
    /** @brief 深度・上中下層タグの適合判定と優先順位を検証します。 */
    bool VerifyPieceAreaTagRules(std::string& outError);
#endif
}
