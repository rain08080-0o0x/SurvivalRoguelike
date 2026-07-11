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
}
