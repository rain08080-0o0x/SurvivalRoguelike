#pragma once

#include "SceneNarakuProto.h"

#include "NarakuStageGenerator.h"
#include "SceneManager.h"
#include "Defines.h"
#include "DirectX.h"
#include "Geometory.h"
#include "Input.h"
#include "NarakuUiNavigation.h"
#include "MeshBuffer.h"
#include "Model.h"
#include "Shader.h"
#include "ShaderList.h"
#include "Sprite.h"
#include "Texture.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <direct.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <numeric>
#include <random>
#include <sstream>
#include <stdexcept>

namespace SceneNarakuProtoImplementation
{
    constexpr const char* kPlaytestConfigPath = "Assets/Config/naraku_proto_playtest.json";
    constexpr const wchar_t* kEnvironmentModelCatalogRelativePath = L"Assets/Naraku/environment_models.cfg";
    constexpr const wchar_t* kRopeModelRelativePath = L"Assets/Model/rope/source/RoupeWithSceleton.fbx";
    constexpr const wchar_t* kRopeTextureRelativePath = L"Assets/Model/rope/textures/txtr.png";
    constexpr const wchar_t* kRopeSupportModelRelativePath = L"Assets/Model/rope/rope.fbx";
    constexpr const wchar_t* kRopeSupportTextureRelativePath = L"Assets/Base/texture/tree_bark1.jpg";
    constexpr const wchar_t* kMiningPointModelRelativePath = L"Assets/Model/Mining/garakuta.fbx";
    constexpr const wchar_t* kPickaxeModelRelativePath = L"Assets/Model/weapon/pickaxe.fbx";
    constexpr float kPickaxeVisualLength = 1.8f;
    constexpr float kRopeVisualDiameter = 0.08f;
    constexpr float kRopeSupportHeight = 1.6f;
    constexpr float kRopeAnchorHeight = 1.5f;
    constexpr float kRopePlayerHangOffset = 1.4f;
    constexpr float kBottomRopeGrabClearance = 0.1f;
    constexpr const wchar_t* kProgressDirectory = L"Assets/Save";
    constexpr const wchar_t* kProgressPath = L"Assets/Save/naraku_proto_save.dat";
    constexpr const wchar_t* kProgressTempPath = L"Assets/Save/naraku_proto_save.tmp";
    constexpr const wchar_t* kWeeklyWorldDirectory = L"Assets/Save/WeeklyWorld";
    constexpr const wchar_t* kWeeklyWorldPath = L"Assets/Save/WeeklyWorld/world.dat";
    constexpr const wchar_t* kWeeklyWorldTempPath = L"Assets/Save/WeeklyWorld/world.tmp";
    constexpr int kSaveVersion = 11;
    constexpr int kPreviousSaveVersion = 10;
    constexpr std::array<NarakuPiece::SurfaceFacilityType, 5> kSurfaceFacilityOrder = {
        NarakuPiece::SurfaceFacilityType::Home,
        NarakuPiece::SurfaceFacilityType::Shop,
        NarakuPiece::SurfaceFacilityType::Armory,
        NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk,
        NarakuPiece::SurfaceFacilityType::AbyssEntrance };
    constexpr double kGameDaySeconds = 24.0 * 60.0 * 60.0;
    constexpr double kGameWeekSeconds = 7.0 * kGameDaySeconds;
    // 実時間30分をゲーム内の1日として進めます。
    constexpr double kGameTimeScale = (24.0 * 60.0 * 60.0) / (30.0 * 60.0);
    constexpr double kQuestSlotCooldownSeconds = 5.0 * 60.0;
    constexpr std::size_t kQuestBoardSize = 7;
    constexpr int kMaximumActiveQuests = 3;
    constexpr std::array<float, 5> kWorldTimeDepthMultipliers = { 1.0f, 1.5f, 4.0f, 10.0f, 30.0f };

    /**
     * @brief 地上施設種別に対応するランタイムモデルIDを返します。
     * @param type 変換する地上施設種別です。
     * @return 対応するモデルIDです。未対応種別の場合は空文字列です。
     */
    inline const char* GetSurfaceFacilityModelId(NarakuPiece::SurfaceFacilityType type)
    {
        switch (type)
        {
        case NarakuPiece::SurfaceFacilityType::Home: return "surface_facility_home";
        case NarakuPiece::SurfaceFacilityType::Shop: return "surface_facility_shop";
        case NarakuPiece::SurfaceFacilityType::Armory: return "surface_facility_armory";
        case NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk: return "surface_facility_restaurant_quest_desk";
        case NarakuPiece::SurfaceFacilityType::AbyssEntrance: return "surface_facility_abyss_entrance";
        default: return "";
        }
    }

    /**
     * @brief 現在発行されているマップの時刻印と識別子を読み取ります。
     * @return 読み取った2行を結合したバージョン文字列です。読取不能時は空文字列です。
     */
    inline std::string ReadCurrentMapVersion()
    {
        std::ifstream stream("Assets/Maps/map_version.txt");
        std::string stamp;
        std::string identifier;
        std::getline(stream, stamp);
        std::getline(stream, identifier);
        return stream || (!stamp.empty() && !identifier.empty())
            ? stamp + ":" + identifier
            : std::string();
    }

    struct EditorPreviewConfiguration
    {
        unsigned long long seed = 1;
        int depth = 1;
        int sublayer = 0;
        int area = 1;
        bool spawnAtReturnArea = false;
        std::array<int, 15> areaCounts = {};
    };

    /**
     * @brief Editorプレビュー設定を既定値へ重ねて読み込みます。
     * @return 読込結果です。ゲームビルドまたは項目不正時は該当項目の既定値を保持します。
     */
    inline EditorPreviewConfiguration LoadEditorPreviewConfiguration()
    {
        EditorPreviewConfiguration result;
#if defined(NARAKU_EDITOR_BUILD)
        std::ifstream stream("Assets/Config/naraku_editor_preview.cfg");
        std::string line;
        while (std::getline(stream, line))
        {
            const std::size_t separator = line.find('=');
            if (separator == std::string::npos) continue;
            const std::string key = line.substr(0, separator);
            const std::string value = line.substr(separator + 1);
            try
            {
                if (key == "seed") result.seed = std::stoull(value);
                else if (key == "depth") result.depth = std::stoi(value);
                else if (key == "sublayer") result.sublayer = std::stoi(value);
                else if (key == "area") result.area = std::stoi(value);
                else if (key == "spawnAtReturnArea") result.spawnAtReturnArea = std::stoi(value) != 0;
                else if (key.rfind("areaCount", 0) == 0)
                {
                    const int stage = std::stoi(key.substr(9));
                    if (stage >= 0 && stage < static_cast<int>(result.areaCounts.size()))
                        result.areaCounts[static_cast<std::size_t>(stage)] = std::stoi(value);
                }
            }
            catch (...) {}
        }
#endif
        return result;
    }

    struct DepthRules
    {
        std::array<int, 5> dropWeights;
        float enemyHp;
        float enemyAttack;
        float enemyMove;
        float enemyInterval;
        float regularExp;
        float movementExp;
        int movementExpCap;
        float reward;
        float stayReward;
        int chargerMax;
        int territoryMax;
    };

    constexpr DepthRules kDepthRules[] =
    {
        { { 70, 15, 15, 0, 0 }, 1.00f, 1.00f, 1.00f, 1.000f, 1.0f,   1.0f, 100, 1.0f, 0.8f, 2, 1 },
        { { 40, 20, 30, 10, 0 }, 1.75f, 1.25f, 1.10f, 0.875f, 5.0f,   2.0f, 200, 1.5f, 1.2f, 2, 1 },
        { { 20, 30, 32, 17, 1 }, 4.50f, 1.50f, 1.25f, 0.750f, 17.5f,  3.0f, 300, 2.0f, 1.6f, 3, 1 },
        { { 10, 29, 35, 25, 1 }, 15.0f, 2.00f, 1.50f, 0.625f, 40.0f,  4.0f, 400, 3.5f, 2.0f, 4, 2 },
        { { 0, 27, 35, 35, 3 }, 25.0f, 2.50f, 2.00f, 0.500f, 100.0f, 5.0f, 500, 6.0f, 2.4f, 5, 3 }
    };

    using LevelMultiplierRow = std::array<float, 10>;
    using DepthLevelMultiplierTable = std::array<LevelMultiplierRow, 5>;

    constexpr DepthLevelMultiplierTable kStaminaConsumptionMultipliers = { {
        { 1.50f, 1.30f, 1.20f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.80f },
        { 1.60f, 1.35f, 1.20f, 1.10f, 1.00f, 1.00f, 1.00f, 1.00f, 0.90f, 0.80f },
        { 2.00f, 1.80f, 1.60f, 1.45f, 1.30f, 1.10f, 1.00f, 1.00f, 1.00f, 1.00f },
        { 2.50f, 2.20f, 1.80f, 1.65f, 1.45f, 1.25f, 1.10f, 1.00f, 1.00f, 1.00f },
        { 3.50f, 2.65f, 2.35f, 1.70f, 1.65f, 1.45f, 1.25f, 1.10f, 1.00f, 1.00f }
    } };

    constexpr DepthLevelMultiplierTable kStaminaRecoveryMultipliers = { {
        { 0.65f, 0.75f, 0.80f, 0.85f, 0.95f, 1.00f, 1.00f, 1.00f, 1.10f, 1.20f },
        { 0.60f, 0.70f, 0.75f, 0.80f, 0.90f, 0.95f, 1.00f, 1.00f, 1.10f, 1.20f },
        { 0.50f, 0.55f, 0.60f, 0.65f, 0.70f, 0.80f, 0.90f, 1.00f, 1.00f, 1.00f },
        { 0.50f, 0.50f, 0.55f, 0.60f, 0.65f, 0.75f, 0.80f, 0.95f, 1.00f, 1.00f },
        { 0.45f, 0.50f, 0.55f, 0.60f, 0.70f, 0.75f, 0.75f, 0.90f, 1.00f, 1.00f }
    } };

    constexpr DepthLevelMultiplierTable kMentalConsumptionMultipliers = { {
        { 1.00f, 1.00f, 0.90f, 0.75f, 0.50f, 0.30f, 0.15f, 0.00f, 0.00f, 0.00f },
        { 1.25f, 1.00f, 1.00f, 0.85f, 0.60f, 0.45f, 0.20f, 0.10f, 0.00f, 0.00f },
        { 2.00f, 1.50f, 1.25f, 1.10f, 1.00f, 0.95f, 0.65f, 0.35f, 0.20f, 0.00f },
        { 3.50f, 3.25f, 2.50f, 1.75f, 1.50f, 1.10f, 1.00f, 0.90f, 0.75f, 0.55f },
        { 5.00f, 4.50f, 3.75f, 3.00f, 1.75f, 1.50f, 1.25f, 1.10f, 1.00f, 0.90f }
    } };

    constexpr DepthLevelMultiplierTable kFullnessConsumptionMultipliers = { {
        { 1.10f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
        { 1.15f, 1.05f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
        { 1.45f, 1.25f, 1.10f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
        { 1.60f, 1.45f, 1.30f, 1.25f, 1.10f, 1.00f, 1.00f, 1.00f, 1.00f, 1.00f },
        { 1.65f, 1.50f, 1.40f, 1.30f, 1.20f, 1.05f, 1.00f, 1.00f, 1.00f, 1.00f }
    } };

    constexpr LevelMultiplierRow kUpperLoadFirstMentalLoss =
        { 10.0f, 6.0f, 3.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr LevelMultiplierRow kUpperLoadFirstVisionOcclusion =
        { 0.50f, 0.25f, 0.125f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr LevelMultiplierRow kUpperLoadSecondMentalLoss =
        { 30.0f, 24.0f, 15.0f, 8.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr LevelMultiplierRow kUpperLoadSecondVisionOcclusion =
        { 0.75f, 0.50f, 0.25f, 0.125f, 0.0625f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr LevelMultiplierRow kUpperLoadFourthDamageRatios =
        { 0.50f, 0.50f, 0.50f, 0.30f, 0.24f, 0.125f, 0.10f, 0.08f, 0.05f, 0.025f };
    constexpr LevelMultiplierRow kUpperLoadFifthDamageRatios =
        { 0.95f, 0.95f, 0.95f, 0.95f, 0.80f, 0.65f, 0.45f, 0.20f, 0.125f, 0.03f };
    constexpr LevelMultiplierRow kUpperLoadFifthDurations =
        { 15.0f, 15.0f, 15.0f, 13.0f, 13.0f, 10.0f, 8.0f, 5.0f, 3.0f, 1.0f };

    constexpr float kPlayerBaseMaxHp = 100.0f;
    constexpr float kPlayerBaseMaxStamina = 100.0f;
    constexpr float kPlayerBaseMaxMental = 100.0f;
    constexpr float kPlayerBaseAttack = 10.0f;
    constexpr float kPlayerBaseDefense = 1.0f;
    constexpr float kFullnessMaximum = 100.0f;
    constexpr float kFullnessWarning = 30.0f;
    constexpr float kFullnessCritical = 10.0f;
    constexpr float kRestaurantHpRatio = 0.75f;
    constexpr float kRestaurantMentalRatio = 0.50f;
    constexpr int kRestaurantPrice = 50;
    constexpr int kFoodPrice = 10;
    constexpr float kFoodHpRecovery = 20.0f;
    constexpr float kFoodFullnessRecovery = 10.0f;
    constexpr float kFoodHydrationRecovery = 75.0f;
    constexpr float kHeatedFoodHpRecovery = 40.0f;
    constexpr float kHeatedFoodFullnessRecovery = 25.0f;
    constexpr float kHeatedFoodMentalRecovery = 5.0f;
    constexpr float kHydrationMaximum = 100.0f;
    constexpr float kWaterDrinkAmount = 25.0f;
    constexpr float kWaterBottleCapacity = 100.0f;
    constexpr float kWaterBottleWeight = 15.0f;
    constexpr float kCookingKitWeight = 5.0f;
    constexpr int kWaterBottlePrice = 500;
    constexpr int kCookingKitPrice = 100;
    constexpr int kCookingKitMaxUses = 5;
    constexpr int kPortableLightPrice = 250;
    constexpr int kPortableLightSellPrice = 25;
    constexpr int kBrokenPortableLightSellPrice = 5;
    constexpr float kPortableLightWeight = 5.0f;
    constexpr float kPortableLightDuration = 15.0f * 60.0f;
    constexpr float kCookingDuration = 60.0f;
    constexpr int kSecondBaseMealPrice = kRestaurantPrice * 2;
    constexpr int kSecondBaseLodgingPrice = 1000;
    constexpr int kForwardBaseMealPrice = 200;
    constexpr float kForwardBaseMealRecoveryRatio = 0.80f;
    constexpr int kRationOnePrice = 200;
    constexpr float kRationOneWeight = 1.0f;
    constexpr float kRationFullnessWardDuration = 300.0f;
    constexpr float kRationHydrationPenaltyDuration = 30.0f;
    constexpr float kRationHydrationPenaltyMultiplier = 1.25f;
    constexpr int kUnknownHeadPrice = 300000;
    constexpr int kUnknownBodyPrice = 500000;
    constexpr int kCartridgePrice = 100000;
    constexpr float kCartridgeWeight = 1.0f;
    constexpr int kUnknownWeaponPrice = 1000000;
    constexpr float kUnknownWeaponChargeDuration = 1.0f;
    constexpr float kUnknownWeaponCooldownDuration = 5.0f;
    constexpr float kUnknownWeaponHalfWidth = 0.25f;
    constexpr int kUnknownFishPrice = 750;
    constexpr float kUnknownFishWeight = 20.0f;
    constexpr std::array<float, 3> kSizedFishWeights = { 10.0f, 20.0f, 30.0f };
    constexpr std::array<int, 3> kRawSizedFishSellValues = { 100, 300, 500 };
    constexpr std::array<int, 3> kCookedSizedFishSellValues = { 50, 150, 250 };
    constexpr std::array<float, 3> kRawSizedFishFullness = { 10.0f, 20.0f, 30.0f };
    constexpr std::array<float, 3> kRawSizedFishMental = { 5.0f, 10.0f, 15.0f };
    constexpr std::array<float, 3> kCookedSizedFishFullness = { 50.0f, 70.0f, 100.0f };
    constexpr std::array<float, 3> kCookedSizedFishMental = { 25.0f, 35.0f, 50.0f };
    constexpr int kFishingMaximumUses = 5;
    constexpr double kFishingRechargeGameSeconds = 10.0 * 60.0;
    constexpr float kFishingStartStaminaRatio = 0.10f;
    constexpr float kFishingLandingDuration = 1.0f;
    constexpr float kDehydrationDelay = 60.0f;
    constexpr float kDehydrationTransitionDuration = 60.0f;
    constexpr float kDehydrationMaxOcclusion = 0.50f;
    constexpr float kRelicAttackRadius = 2.0f;
    constexpr float kRelicAttackDamageScale = 12.0f;
    constexpr float kMentalSenseDuration = 15.0f;
    constexpr float kQHoldThreshold = 1.0f;
    constexpr float kEnemyRespawnTime = 300.0f;
    constexpr float kEnemyRespawnRetry = 15.0f;
    constexpr float kMiningPointRespawnTime = 10.0f * 60.0f;
    constexpr float kEnemyRespawnMinPlayerDistance = 15.0f;
    constexpr float kDiscoveryRange = 7.5f;
    constexpr int kLevel100ProtectionExp = 1500000;

    /**
     * @brief 深度をゲームで扱う第一層から第五層の範囲へ制限します。
     * @param depth 制限前の深度です。
     * @return 1以上5以下へ制限した深度です。
     */
    inline int ClampDepth(int depth)
    {
        return std::max(1, std::min(5, depth));
    }

    /**
     * @brief 10レベル刻みの補正表を現在レベルに合わせて線形補間します。
     * @param row レベル10刻みの補正値です。
     * @param level 補間対象のプレイヤーレベルです。
     * @return レベル1から100の範囲で補間した補正値です。
     */
    inline float GetLevelInterpolatedValue(const LevelMultiplierRow& row, int level)
    {
        const int clampedLevel = std::max(1, std::min(100, level));
        if (clampedLevel <= 10)
        {
            return row.front();
        }

        const int upperIndex = (clampedLevel - 1) / 10;
        const int lowerIndex = upperIndex - 1;
        const int lowerLevel = upperIndex * 10;
        const float t = static_cast<float>(clampedLevel - lowerLevel) / 10.0f;
        return row[static_cast<std::size_t>(lowerIndex)] +
            (row[static_cast<std::size_t>(upperIndex)] - row[static_cast<std::size_t>(lowerIndex)]) * t;
    }

    /**
     * @brief 深度別レベル補正表から対象深度・レベルの補正値を求めます。
     * @param table 深度別のレベル補正表です。
     * @param depth 対象深度です。
     * @param level 対象レベルです。
     * @return 深度を制限し、レベル間を補間した補正値です。
     */
    inline float GetDepthLevelMultiplier(const DepthLevelMultiplierTable& table, int depth, int level)
    {
        return GetLevelInterpolatedValue(table[static_cast<std::size_t>(ClampDepth(depth) - 1)], level);
    }

    /**
     * @brief 精神能力の消費量へ適用する深度倍率を返します。
     * @param depth 対象深度です。
     * @return 第一層から第五層に対応する消費倍率です。
     */
    inline float GetMentalAbilityDepthMultiplier(int depth)
    {
        switch (ClampDepth(depth))
        {
        case 1: return 1.0f;
        case 2: return 1.5f;
        case 3: return 2.25f;
        case 4: return 3.75f;
        default: return 7.5f;
        }
    }

    /**
     * @brief 浮動小数値を小数第2位へ丸めます。
     * @param value 丸める値です。
     * @return 小数第2位へ丸めた値です。
     */
    inline float RoundToHundredth(float value)
    {
        return std::round(value * 100.0f) / 100.0f;
    }

    /**
     * @brief 対象深度に適用する戦闘・報酬ルールを返します。
     * @param depth 対象深度です。
     * @return 第一層から第五層へ制限した深度ルールへの参照です。
     */
    inline const DepthRules& GetRulesForDepth(int depth)
    {
        return kDepthRules[static_cast<std::size_t>(ClampDepth(depth) - 1)];
    }

    /**
     * @brief 全責務ファイルで共有するゲーム進行用乱数エンジンを返します。
     * @return プログラム内で1つだけ保持される乱数エンジンへの参照です。
     */
    inline std::mt19937& RuntimeRandomEngine()
    {
        static std::mt19937 engine(std::random_device{}());
        return engine;
    }

    /**
     * @brief ゲーム進行用乱数と既存の標準乱数へ同じ生成元シードを設定します。
     * @param seed 64bitの生成シードです。
     */
    inline void SeedRuntimeRandom(unsigned long long seed)
    {
        const unsigned int low = static_cast<unsigned int>(seed);
        const unsigned int high = static_cast<unsigned int>(seed >> 32);
        std::seed_seq sequence = { low, high };
        RuntimeRandomEngine().seed(sequence);
        std::srand(low ^ high);
    }

    /**
     * @brief 共有乱数エンジンから指定範囲の浮動小数乱数を生成します。
     * @param minimum 生成範囲の下限です。
     * @param maximum 生成範囲の上限です。
     * @return 一様分布から生成した値です。
     */
    inline float RandomFloat(float minimum, float maximum)
    {
        std::uniform_real_distribution<float> distribution(minimum, maximum);
        return distribution(RuntimeRandomEngine());
    }

    /**
     * @brief 共有乱数エンジンから指定範囲の整数乱数を生成します。
     * @param minimum 生成範囲の下限です。
     * @param maximum 生成範囲の上限です。
     * @return 両端を含む一様分布から生成した値です。
     */
    inline int RandomInt(int minimum, int maximum)
    {
        std::uniform_int_distribution<int> distribution(minimum, maximum);
        return distribution(RuntimeRandomEngine());
    }

    /**
     * @brief UTF-8文字列をWindows API用のワイド文字列へ変換します。
     * @param text 変換するUTF-8文字列です。
     * @return 変換後の文字列です。空入力または変換失敗時は空文字列です。
     */
    inline std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (length <= 1) return {};
        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
        result.pop_back();
        return result;
    }

    /**
     * @brief ワイド文字列を保存・表示用のUTF-8文字列へ変換します。
     * @param text 変換するワイド文字列です。
     * @return 変換後の文字列です。空入力または変換失敗時は空文字列です。
     */
    inline std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1) return {};
        std::string result(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
        result.pop_back();
        return result;
    }

    /**
     * @brief 既定マップの解決先からNarakuプロジェクトルートを求めます。
     * @return 解決したプロジェクトルートです。判定不能時は空文字列です。
     */
    inline std::wstring GetNarakuProjectRoot()
    {
        std::wstring mapPath = NarakuMap::ResolveMapPathForFileSystem(NarakuMap::GetDefaultMapPath());
        std::replace(mapPath.begin(), mapPath.end(), L'\\', L'/');
        const std::wstring marker = L"/Assets/Maps/";
        const size_t markerPos = mapPath.find(marker);
        return markerPos == std::wstring::npos ? std::wstring() : mapPath.substr(0, markerPos);
    }

    /**
     * @brief 相対パスをNarakuプロジェクトルート基準のパスへ解決します。
     * @param path 解決する絶対または相対パスです。
     * @return 絶対パスはそのまま、相対パスはプロジェクトルートと結合して返します。
     */
    inline std::wstring ResolveProjectPath(const std::wstring& path)
    {
        if (path.size() >= 2 && path[1] == L':') return path;
        const std::wstring root = GetNarakuProjectRoot();
        if (root.empty()) return path;
        return root + L"/" + path;
    }

    /**
     * @brief 実行ディレクトリとプロジェクトルートの順にアセットパスを解決します。
     * @param path 解決するアセットパスです。
     * @return 実在する実行時パス、またはプロジェクトルート基準へ解決したパスです。
     */
    inline std::wstring ResolveRuntimeAssetPath(const std::wstring& path)
    {
        if (path.size() >= 2 && path[1] == L':') return path;
        if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
        return ResolveProjectPath(path);
    }

    /**
     * @brief 簡易設定JSONから指定キーの有限な浮動小数値を読み取ります。
     * @param json 検索するJSON文字列です。
     * @param key 読み取るキー名です。
     * @param outValue 読取成功時に値を書き込む出力先です。
     * @return キーと有限値を読み取れた場合はtrueです。
     */
    inline bool TryReadJsonFloat(const std::string& json, const char* key, float& outValue)
    {
        const std::string token = std::string("\"") + key + "\"";
        const std::size_t keyPos = json.find(token);
        if (keyPos == std::string::npos)
        {
            return false;
        }
        const std::size_t colonPos = json.find(':', keyPos + token.size());
        if (colonPos == std::string::npos)
        {
            return false;
        }
        char* end = nullptr;
        const float value = std::strtof(json.c_str() + colonPos + 1, &end);
        if (end == json.c_str() + colonPos + 1 || !std::isfinite(value))
        {
            return false;
        }
        outValue = value;
        return true;
    }

    // 既存プロジェクトは固定FPS前提なので、1フレーム秒数も固定値で扱います。
    constexpr float kDt = 1.0f / fFPS;
    // ステップで進む距離です。
    constexpr float kStepDistance = 5.0f;
    // ステップの無敵時間です。
    constexpr float kStepInvincibleTime = 0.5f;
    // ステップ後に操作を戻すまでの硬直時間です。
    constexpr float kStepRecoveryTime = 0.5f;
    // 攻撃ボタンを押してから攻撃判定が出るまでの時間です。
    constexpr float kAttackStartup = 0.25f;
    // 攻撃判定が有効な時間です。
    constexpr float kAttackActive = 0.15f;
    // 攻撃判定後の硬直時間です。
    constexpr float kAttackRecovery = 0.40f;
    // 攻撃全体の長さです。
    constexpr float kAttackTotal = kAttackStartup + kAttackActive + kAttackRecovery;
    // 採掘モーション完了までの時間です。
    constexpr float kMiningTime = 2.0f;
    // 現在層の上昇負荷が発症する累計上昇量です。
    constexpr float kUpperLoadLimit = 20.0f;
    // 上昇していない時に1秒あたり回復する上昇負荷ゲージ量です。
    constexpr float kUpperLoadRecoveryPerSecond = 1.0f;
    constexpr float kUpperLoadFifthDamageInterval = 1.0f;
    // 通常の最大重量です。100%以上でも歩けますが一部行動が制限されます。
    constexpr float kMaxWeight = 100.0f;
    // 拾える限界重量です。これを超える拾得は拒否します。
    constexpr float kPickupWeightLimit = 150.0f;
    // 敵の通常移動速度です。既定プレイヤー通常速度1.5m/sの50%です。
    constexpr float kEnemyWalkSpeed = 0.75f;
    // 敵の体当たり速度です。敵通常移動の3倍です。
    constexpr float kEnemyChargeSpeed = kEnemyWalkSpeed * 3.0f;
    // 敵が次の体当たりを開始するまでの間隔です。
    constexpr float kEnemyAttackInterval = 5.0f;
    // 敵の体当たり前予備動作時間です。
    constexpr float kEnemyTelegraphTime = 0.55f;
    // 敵の体当たり移動時間です。
    constexpr float kEnemyChargeTime = 0.45f;
    constexpr float kEnemyChargeStartSpeedScale = 0.35f;
    constexpr float kEnemyChargeEndSpeedScale = 1.65f;
    // 敵の体当たりが命中する距離です。
    constexpr float kEnemyHitRange = 0.45f;
    // 敵の体当たり命中時に押し出す距離です。
    constexpr float kKnockbackDistance = 1.5f;
    // ノックバックが続く時間です。
    constexpr float kKnockbackTime = 0.25f;
    // 採掘、ロープ、地面旧器に反応する距離です。
    constexpr float kInteractRange = 1.0f;
    // 落下中にロープをつかめる円柱状の判定半径です（直径1m）。
    constexpr float kFallingRopeGrabRadius = 0.5f;
    // 帰還地点に反応する距離です。
    constexpr float kReturnRange = 1.4f;
    // 未発見採掘ポイントを発見する距離です。
    constexpr float kDiscoverRange = 3.0f;
    constexpr float kNearbyMiningVisibleRange = 8.0f;
    // つるはし攻撃の射程です。
    constexpr float kAttackRange = 1.15f;
    constexpr int kAttackHitEffectFrameCount = 10;
    constexpr float kAttackHitEffectFrameTime = 1.0f / 30.0f;
    constexpr float kAttackHitEffectDuration = kAttackHitEffectFrameCount * kAttackHitEffectFrameTime;
    constexpr int kCharacterSpriteColumns = 9;
    constexpr int kCharacterSpriteRows = 7;
    constexpr float kPlayerMoveAnimationFps = 10.0f;
    constexpr float kEnemyMoveAnimationFps = 8.0f;
    constexpr float kEnemyAttackAnimationFps = 16.0f;
    constexpr int kIdleDownFrames[] = { 0, 1, 2, 3 };
    constexpr int kIdleDownRightFrames[] = { 4, 5, 6, 7 };
    constexpr int kIdleRightFrames[] = { 8, 9, 10, 11 };
    constexpr int kIdleRightUpFrames[] = { 12, 13, 14, 15 };
    constexpr int kIdleUpFrames[] = { 16, 17, 18, 19 };
    constexpr int kMoveDownFrames[] = { 20, 21, 22, 23, 24, 25, 26, 27 };
    constexpr int kMoveDownRightFrames[] = { 28, 29, 30, 31, 32, 33, 34, 35 };
    constexpr int kMoveRightFrames[] = { 36, 37, 38, 39, 40, 41, 42, 43 };
    constexpr int kMoveRightUpFrames[] = { 53, 54, 55, 56, 57, 58, 59 };
    constexpr int kMoveUpFrames[] = { 44, 45, 46, 47, 48, 49, 50, 51 };

    struct CharacterFrameSequence
    {
        const int* frames = nullptr;
        int count = 0;
        bool mirror = false;
    };

    /**
     * @brief 画面上の移動方向と移動状態からプレイヤーのアニメーション列を選択します。
     * @param screenRight 画面右方向の移動成分です。
     * @param screenUp 画面上方向の移動成分です。
     * @param moving 移動アニメーションを選ぶ場合はtrueです。
     * @return 使用するフレーム列、枚数、左右反転状態です。
     */
    inline CharacterFrameSequence GetCharacterFrameSequence(
        float screenRight,
        float screenUp,
        bool moving)
    {
        constexpr float kPi = 3.14159265358979323846f;
        const float directionAngle = std::atan2(std::fabs(screenRight), screenUp);
        const bool mirror = screenRight < 0.0f;

        if (directionAngle < kPi * 0.125f)
        {
            return moving
                ? CharacterFrameSequence{ kMoveUpFrames, 8, false }
                : CharacterFrameSequence{ kIdleUpFrames, 4, false };
        }
        if (directionAngle < kPi * 0.375f)
        {
            return moving
                ? CharacterFrameSequence{ kMoveRightUpFrames, 7, mirror }
                : CharacterFrameSequence{ kIdleRightUpFrames, 4, mirror };
        }
        if (directionAngle < kPi * 0.625f)
        {
            return moving
                ? CharacterFrameSequence{ kMoveRightFrames, 8, mirror }
                : CharacterFrameSequence{ kIdleRightFrames, 4, mirror };
        }
        if (directionAngle < kPi * 0.875f)
        {
            return moving
                ? CharacterFrameSequence{ kMoveDownRightFrames, 8, mirror }
                : CharacterFrameSequence{ kIdleDownRightFrames, 4, mirror };
        }
        return moving
            ? CharacterFrameSequence{ kMoveDownFrames, 8, false }
            : CharacterFrameSequence{ kIdleDownFrames, 4, false };
    }

    constexpr int kJumpEffectColumns = 4;
    constexpr int kJumpEffectRows = 2;
    constexpr int kJumpEffectFrameCount = kJumpEffectColumns * kJumpEffectRows;
    constexpr float kJumpEffectFrameTime = 1.0f / 12.0f;
    constexpr float kJumpEffectDuration = kJumpEffectFrameCount * kJumpEffectFrameTime;
    constexpr float kCameraShakeDuration = 0.24f;
    constexpr float kCameraShakeAmplitude = 0.18f;
    constexpr float kSkySphereRadius = 180.0f;
    /** @brief 探索カメラの注視点からの固定距離です。 */
    constexpr float kCameraDefaultDistance = 13.8564f;
    /** @brief カメラ仰角の下限（真横を0度、真上を90度）です。 */
    constexpr float kCameraMinPitchDegrees = 1.0f;
    constexpr float kCameraMaxPitchDegrees = 89.0f;
    constexpr float kCameraDefaultMinPitchDegrees = 10.0f;
    constexpr float kCameraDefaultMaxPitchDegrees = 60.0f;
    constexpr float kCameraMinDistance = 6.0f;
#if defined(NARAKU_EDITOR_BUILD)
    constexpr float kCameraMaxDistance = 120.0f;
#else
    constexpr float kCameraMaxDistance = kCameraDefaultDistance;
#endif
    constexpr int kMapGenerationMaxAttempts = 5;
    constexpr float kLayerTransitionDuration = 1.5f;
    constexpr float kLayerTransitionHeight = 6.0f;
    constexpr float kScreenFadeDuration = 0.4f;
    constexpr float kCompassRadius = 30.0f;
    constexpr float kCompassMargin = 12.0f;
    constexpr float kCompassLineThickness = 1.5f;
    constexpr float kCompassLinePadding = 2.0f;
    constexpr float kCompassLabelDistance = 8.0f;
    const char* const kCompassDirectionLabels[] = { u8"北", u8"南", u8"東", u8"西" };
    // Shiftをこの秒数以上押し続けたら走り扱いにします。
    constexpr float kShiftRunThreshold = 0.18f;
    // 通常歩行で乗り越えられる上り段差です。
    constexpr float kMaxWalkClimbHeight = 0.55f;
    // 通常歩行でそのまま降りられる下り段差です。これを超える下りは落下可属性が必要です。
    constexpr float kMaxWalkDropHeight = 0.80f;
    // 歩行移動後に地面へ即吸着せず、そのまま落下へ移る下り落差です。
    constexpr float kAutoFallStartHeight = 0.90f;
    // 崖境界セルで通常歩行を止める下り段差です。
    constexpr float kCliffEdgeBlockDropHeight = 0.20f;
    // 危険地形の継続ダメージ間隔です。
    constexpr float kHazardTickInterval = 0.50f;
    // 危険地形1回ぶんのダメージです。
    constexpr float kHazardDamage = 5.0f;
    // 斜面移動時に経路を分割する1区間の基準長です。
    constexpr float kSlopeMoveSampleStep = 0.25f;
    // セル境界の浮動小数誤差で通行不可になりにくくするための許容値です。
    constexpr float kSlopeHeightTolerance = 0.03f;
    // 歩行不可セルから脱出できない場合に安全地点へ戻すまでの時間です。
    constexpr float kBlockedCellReturnTime = 5.0f;
    // 湖へ落下したと判定する地表からの距離です。
    constexpr float kLakeFallDepth = 1.0f;
    // 湖へ落下した時に失う最大HP割合です。
    constexpr float kLakeFallDamageRatio = 0.15f;
    // 軽い落下で発生する着地硬直時間です。
    constexpr float kLandingRecoveryLight = 0.14f;
    // 中程度の落下で発生する着地硬直時間です。
    constexpr float kLandingRecoveryMedium = 0.24f;
    // 重い落下で発生する着地硬直時間です。
    constexpr float kLandingRecoveryHeavy = 0.38f;

    // プロトタイプで使う4級旧器名です。コード側では文字化け回避のため英字にしています。
    constexpr const char* kRelicNames[] =
    {
        u8"錆びた輪",
        u8"欠けた歯車",
        u8"古びた留め具",
        u8"音のしない鈴",
        u8"黒ずんだ皿片",
        u8"ひび入り硝子",
        u8"曲がった鍵片",
        u8"くすんだ小筒"
    };
}
