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

namespace
{
    constexpr const char* kPlaytestConfigPath = "Assets/Config/naraku_proto_playtest.json";
    constexpr const wchar_t* kEnvironmentModelCatalogRelativePath = L"Assets/Naraku/environment_models.cfg";
    constexpr const wchar_t* kRopeModelRelativePath = L"Assets/Model/rope/source/RoupeWithSceleton.fbx";
    constexpr const wchar_t* kRopeTextureRelativePath = L"Assets/Model/rope/textures/txtr.png";
    constexpr const wchar_t* kRopeSupportModelRelativePath = L"Assets/Model/rope/rope.fbx";
    constexpr const wchar_t* kRopeSupportTextureRelativePath = L"Assets/Base/texture/tree_bark1.jpg";
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
    constexpr std::array<const wchar_t*, 5> kSurfacePieceFiles = {
        L"surface_home.json", L"surface_shop.json", L"surface_armory.json",
        L"surface_restaurant.json", L"surface_abyss_entrance.json" };
    constexpr double kGameDaySeconds = 24.0 * 60.0 * 60.0;
    constexpr double kGameWeekSeconds = 7.0 * kGameDaySeconds;
    // 実時間30分をゲーム内の1日として進めます。
    constexpr double kGameTimeScale = (24.0 * 60.0 * 60.0) / (30.0 * 60.0);
    constexpr double kQuestSlotCooldownSeconds = 5.0 * 60.0;
    constexpr std::size_t kQuestBoardSize = 7;
    constexpr int kMaximumActiveQuests = 3;
    constexpr std::array<float, 5> kWorldTimeDepthMultipliers = { 1.0f, 1.5f, 4.0f, 10.0f, 30.0f };

    std::string ReadCurrentMapVersion()
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

    EditorPreviewConfiguration LoadEditorPreviewConfiguration()
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

    int ClampDepth(int depth)
    {
        return std::max(1, std::min(5, depth));
    }

    float GetLevelInterpolatedValue(const LevelMultiplierRow& row, int level)
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

    float GetDepthLevelMultiplier(const DepthLevelMultiplierTable& table, int depth, int level)
    {
        return GetLevelInterpolatedValue(table[static_cast<std::size_t>(ClampDepth(depth) - 1)], level);
    }

    float GetMentalAbilityDepthMultiplier(int depth)
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

    float RoundToHundredth(float value)
    {
        return std::round(value * 100.0f) / 100.0f;
    }

    const DepthRules& GetRulesForDepth(int depth)
    {
        return kDepthRules[static_cast<std::size_t>(ClampDepth(depth) - 1)];
    }

    std::mt19937& RuntimeRandomEngine()
    {
        static std::mt19937 engine(std::random_device{}());
        return engine;
    }

    void SeedRuntimeRandom(unsigned long long seed)
    {
        const unsigned int low = static_cast<unsigned int>(seed);
        const unsigned int high = static_cast<unsigned int>(seed >> 32);
        std::seed_seq sequence = { low, high };
        RuntimeRandomEngine().seed(sequence);
        std::srand(low ^ high);
    }

    float RandomFloat(float minimum, float maximum)
    {
        std::uniform_real_distribution<float> distribution(minimum, maximum);
        return distribution(RuntimeRandomEngine());
    }

    int RandomInt(int minimum, int maximum)
    {
        std::uniform_int_distribution<int> distribution(minimum, maximum);
        return distribution(RuntimeRandomEngine());
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty()) return {};
        const int length = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
        if (length <= 1) return {};
        std::wstring result(static_cast<size_t>(length), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &result[0], length);
        result.pop_back();
        return result;
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty()) return {};
        const int length = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (length <= 1) return {};
        std::string result(static_cast<size_t>(length), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, &result[0], length, nullptr, nullptr);
        result.pop_back();
        return result;
    }

    std::wstring GetNarakuProjectRoot()
    {
        std::wstring mapPath = NarakuMap::ResolveMapPathForFileSystem(NarakuMap::GetDefaultMapPath());
        std::replace(mapPath.begin(), mapPath.end(), L'\\', L'/');
        const std::wstring marker = L"/Assets/Maps/";
        const size_t markerPos = mapPath.find(marker);
        return markerPos == std::wstring::npos ? std::wstring() : mapPath.substr(0, markerPos);
    }

    std::wstring ResolveProjectPath(const std::wstring& path)
    {
        if (path.size() >= 2 && path[1] == L':') return path;
        const std::wstring root = GetNarakuProjectRoot();
        if (root.empty()) return path;
        return root + L"/" + path;
    }

    std::wstring ResolveRuntimeAssetPath(const std::wstring& path)
    {
        if (path.size() >= 2 && path[1] == L':') return path;
        if (::GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) return path;
        return ResolveProjectPath(path);
    }

    bool TryReadJsonFloat(const std::string& json, const char* key, float& outValue)
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

    CharacterFrameSequence GetCharacterFrameSequence(
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
    const char* kRelicNames[] =
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

SceneNarakuProto::SceneNarakuProto()
{
    InitializeTerrainFloorBatch();
    InitializeEnemyBillboardBatch();

    m_attackHitTexture = new Texture();
    if (FAILED(m_attackHitTexture->Create("Assets/Texture/Effect/yellow_bom.png")))
    {
        SAFE_DELETE(m_attackHitTexture);
    }

    m_playerTexture = new Texture();
    if (FAILED(m_playerTexture->Create("Assets/Player/Chara/hero.png")))
    {
        SAFE_DELETE(m_playerTexture);
    }

    m_jumpEffectTexture = new Texture();
    if (FAILED(m_jumpEffectTexture->Create("Assets/Player/VFX/jumpFX.png")))
    {
        SAFE_DELETE(m_jumpEffectTexture);
    }

    m_skyModel = new Model();
    if (!m_skyModel->Load("Assets/Model/sky/sky.obj"))
    {
        SAFE_DELETE(m_skyModel);
    }

    LoadBaseModels();
    LoadRopeModel();

    // プレイテスト用の調整値を既定値で初期化します。
    ResetDebugPlayerParams();
    LoadDebugPlayerParams();

    // 初期装備を所有・装備済みにします。
    m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedWeapons[static_cast<std::size_t>(WeaponTier::RustyPickaxe)] = true;

    InitializeNewProgress();
    LoadProgress();

#if defined(NARAKU_EDITOR_BUILD)
    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    m_editorPreviewGenerationActive = true;
    m_editorPreviewGenerationSeed = preview.seed;
    m_loadingStatus = u8"15段階の構成を準備しています...";
    m_loadingProgress = 0.0f;
    m_loadingStep = 0;
    m_mode = Mode::Loading;
#else
    // フィールドを準備し、最初は自宅で持ち物を決められる状態にします。
    ResetRun();
    if (m_deathRecoveryPending)
    {
        m_result.reason = m_pendingDeathReason.empty() ? u8"死亡しました。" : m_pendingDeathReason;
        m_result.lostRelics = static_cast<int>(m_pendingDeathRecoveryRelics.size());
        m_result.levelBeforeDeath = m_pendingDeathLevelBefore;
        m_result.levelAfterDeath = m_pendingDeathLevelAfter;
        m_result.protectionConsumed = m_pendingDeathProtectionConsumed;
        m_mode = Mode::DeathResult;
    }
    else
    {
        BuildSurfaceRuntime(false);
        m_mode = Mode::Home;
    }
#endif
}

SceneNarakuProto::~SceneNarakuProto()
{
#if !defined(NARAKU_EDITOR_BUILD)
    if (m_mode == Mode::Explore || (m_mode == Mode::Inventory && m_overlayReturnMode == Mode::Explore) ||
        m_mode == Mode::RelicPrompt || m_mode == Mode::WaterPrompt ||
        m_mode == Mode::ReturnConfirm || m_mode == Mode::AbandonConfirm || m_mode == Mode::Loading ||
        m_mode == Mode::LayerTransition || m_mode == Mode::UninsuredDescentConfirm ||
        m_mode == Mode::SecondBase || m_mode == Mode::ForwardBase ||
        (m_mode == Mode::SaveError && m_pendingModeAfterSave == Mode::Explore))
    {
        if (m_mode == Mode::RelicPrompt && m_pendingRelicMiningIndex >= 0)
        {
            m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true,
                m_pendingRelicMiningIndex });
            m_pendingRelicMiningIndex = -1;
        }
        FailActivePromotionQuest();
        ApplyAbandonPenalty();
        m_inventory.clear();
        m_foodCount = 0;
        m_heatedFoodCount = 0;
        m_rationOneCount = 0;
        m_rawFishCount = 0;
        m_cookedFishCount = 0;
        m_rawSizedFish.fill(0);
        m_cookedSizedFish.fill(0);
        m_cartridgeCount = 0;
        m_waterBottles.clear();
        m_cookingKits.clear();
    }
    SaveProgress();
#endif
    ReleaseBaseModels();
    ReleaseRopeModel();
    ReleaseEnvironmentModels();
    ReleaseEnemyBillboardBatch();
    ReleaseTerrainFloorBatch();
    SAFE_DELETE(m_skyModel);
    SAFE_DELETE(m_attackHitTexture);
    SAFE_DELETE(m_jumpEffectTexture);
    SAFE_DELETE(m_playerTexture);
}

#if defined(NARAKU_EDITOR_BUILD)
bool SceneNarakuProto::VerifyPreviewGenerationDeterminism(std::string& outError)
{
    if (!NarakuStageGenerator::VerifyPieceAreaTagRules(outError)) return false;
    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    const auto finishGeneration = [&outError](SceneNarakuProto& scene)
    {
        constexpr int maximumFrames = 256;
        for (int frame = 0; frame < maximumFrames && scene.m_mode == Mode::Loading; ++frame)
        {
            scene.UpdateLoading();
        }
        if (scene.m_mode == Mode::Loading || scene.m_editorPreviewGenerationFailed)
        {
            outError = scene.m_generationFailureSummary;
            if (!scene.m_generationFailureDetail.empty())
            {
                outError += "\n" + scene.m_generationFailureDetail;
            }
            if (outError.empty()) outError = u8"15段階生成が規定フレーム内に完了しませんでした。";
            return false;
        }
        return true;
    };

    const auto createSignature = [](const SceneNarakuProto& scene)
    {
        std::ostringstream signature;
        signature << "startArea=" << scene.m_currentAreaIndex << '\n';
        std::array<int, 15> stageCounts = {};
        for (const AreaState& area : scene.m_areas)
        {
            const int stage = (area.depth - 1) * 3 + area.sublayer;
            if (stage >= 0 && stage < 15) ++stageCounts[static_cast<std::size_t>(stage)];
            signature << area.depth << ',' << area.sublayer << ',' << area.areaNumber << ':';
            for (const PlannedLayerGate& gate : area.plannedGates)
                signature << gate.isEntry << ',' << gate.destinationAreaIndex << ',' << gate.connectionId << ';';
            signature << '|';
            for (const std::string& pieceName : area.map.pieceNames) signature << pieceName << ';';
            signature << '\n';
        }
        signature << "counts:";
        for (int count : stageCounts) signature << count << ',';
        return signature.str();
    };

    std::string firstSignature;
    {
        SceneNarakuProto first;
        if (!finishGeneration(first)) return false;
        if (preview.spawnAtReturnArea &&
            (first.m_currentAreaIndex < 0 ||
                first.m_currentAreaIndex >= static_cast<int>(first.m_areas.size()) ||
                !first.m_areas[static_cast<std::size_t>(first.m_currentAreaIndex)].canReturn))
        {
            outError = u8"開始・帰還地点のあるエリアがプレビュー開始位置に選ばれていません。";
            return false;
        }
        firstSignature = createSignature(first);
    }
    std::string secondSignature;
    {
        SceneNarakuProto second;
        if (!finishGeneration(second)) return false;
        secondSignature = createSignature(second);
    }
    if (firstSignature != secondSignature)
    {
        outError = u8"段階別エリア数・接続ID・小ステージ配置IDが一致しません。";
        return false;
    }
    return true;
}

void SceneNarakuProto::FocusEditorOverviewOnCurrentArea()
{
    if (m_runtimeMap.terrainLayers.empty()) return;
    float centerX = 0.0f;
    float centerZ = 0.0f;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        centerX += layer.center.x;
        centerZ += layer.center.z;
    }
    const float divisor = static_cast<float>(m_runtimeMap.terrainLayers.size());
    m_player.pos = { centerX / divisor, centerZ / divisor };
    m_player.depth = m_runtimeMap.terrainLayers.front().layerDepth;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_cameraMaxPitchDegrees = 89.0f;
    m_cameraPitch = DirectX::XMConvertToRadians(89.0f);
    m_cameraDistance = std::max(20.0f, std::min(120.0f, m_worldHalfSize * 1.6f));
}

void SceneNarakuProto::SetEditorPreviewOverview(bool enabled)
{
    if (enabled == m_editorOverviewMode) return;
    if (enabled)
    {
        m_editorWalkPlayerState = m_player;
        m_editorWalkCameraPitch = m_cameraPitch;
        m_editorWalkCameraDistance = m_cameraDistance;
        m_editorWalkCameraMaxPitch = m_cameraMaxPitchDegrees;
        m_editorOverviewMode = true;
        FocusEditorOverviewOnCurrentArea();
    }
    else
    {
        m_editorOverviewMode = false;
        m_player = m_editorWalkPlayerState;
        m_cameraPitch = m_editorWalkCameraPitch;
        m_cameraDistance = m_editorWalkCameraDistance;
        m_cameraMaxPitchDegrees = m_editorWalkCameraMaxPitch;
    }
}

void SceneNarakuProto::ReturnEditorPreviewToStart()
{
    SetEditorPreviewOverview(false);
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (!m_areas[static_cast<std::size_t>(areaIndex)].canReturn) continue;
        ActivateArea(areaIndex, false);
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        break;
    }
}
#endif

void SceneNarakuProto::InitializeTerrainFloorBatch()
{
    const char* vertexShaderCode = R"HLSL(
struct VS_IN {
    float3 position : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
struct VS_OUT {
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
cbuffer Matrix : register(b0) {
    float4x4 view;
    float4x4 projection;
};
VS_OUT main(VS_IN input) {
    VS_OUT output;
    output.position = mul(float4(input.position, 1.0f), view);
    output.position = mul(output.position, projection);
    output.worldPosition = input.position;
    output.normal = input.normal;
    output.uv = input.uv;
    output.color = input.color;
    return output;
})HLSL";

    const char* pixelShaderCode = R"HLSL(
struct PS_IN {
    float4 position : SV_POSITION;
    float3 worldPosition : POSITION0;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
cbuffer Light : register(b0) {
    float4 directionalColor;
    float4 directionalDirection;
    float4 pointColorEnabled;
    float4 pointPositionRange;
    float4 spotColorEnabled;
    float4 spotPositionRange;
    float4 spotDirectionInnerCos;
    float4 spotOuterCos;
};
Texture2D terrainTexture : register(t0);
SamplerState terrainSampler : register(s0);
float4 main(PS_IN input) : SV_TARGET {
    float4 color = terrainTexture.Sample(terrainSampler, input.uv) * input.color;
    float3 normal = length(input.normal) > 0.001f ? normalize(input.normal) : float3(0.0f, 1.0f, 0.0f);
    float3 directionalToLight = normalize(-directionalDirection.xyz);
    float directionalAmount = saturate((dot(normal, directionalToLight) + 0.5f) / 1.5f);
    float3 illumination = directionalColor.rgb * directionalAmount;

    float3 toPoint = pointPositionRange.xyz - input.worldPosition;
    float pointDistance = length(toPoint);
    float pointAttenuation = pow(saturate(1.0f - pointDistance / max(0.001f, pointPositionRange.w)), 2.0f);
    illumination += pointColorEnabled.rgb * pointColorEnabled.w * pointAttenuation *
        saturate((dot(normal, normalize(toPoint)) + 0.5f) / 1.5f);

    float3 fromSpot = input.worldPosition - spotPositionRange.xyz;
    float spotDistance = length(fromSpot);
    float spotCone = smoothstep(spotOuterCos.x, spotDirectionInnerCos.w,
        dot(normalize(fromSpot), normalize(spotDirectionInnerCos.xyz)));
    float spotAttenuation = pow(saturate(1.0f - spotDistance / max(0.001f, spotPositionRange.w)), 2.0f);
    float spotNdotL = saturate((dot(normal, normalize(-fromSpot)) + 0.5f) / 1.5f);
    illumination += spotColorEnabled.rgb * spotColorEnabled.w * spotCone * spotAttenuation * spotNdotL;

    illumination = saturate(illumination);
    color.rgb *= max(illumination, 0.12f);
    return color;
})HLSL";

    m_terrainFloorVS = new VertexShader();
    if (FAILED(m_terrainFloorVS->Compile(vertexShaderCode)))
    {
        SAFE_DELETE(m_terrainFloorVS);
    }

    m_terrainFloorPS = new PixelShader();
    if (FAILED(m_terrainFloorPS->Compile(pixelShaderCode)))
    {
        SAFE_DELETE(m_terrainFloorPS);
    }

    const char* texturePaths[TerrainTextureCount] =
    {
        "Assets/Texture/Tile/grass.png",
        "Assets/Texture/Tile/dirt.png",
        "Assets/Texture/Tile/cobble.png",
        "Assets/Texture/Tile/wetland.png"
    };
    for (std::size_t index = 0; index < TerrainTextureCount; ++index)
    {
        m_terrainFloorTextures[index] = new Texture();
        if (FAILED(m_terrainFloorTextures[index]->Create(texturePaths[index])))
        {
            SAFE_DELETE(m_terrainFloorTextures[index]);
        }
    }
    const unsigned int whitePixel = 0xffffffff;
    m_terrainFloorTextures[TerrainTextureCount] = new Texture();
    if (FAILED(m_terrainFloorTextures[TerrainTextureCount]->Create(
            DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, &whitePixel)))
    {
        SAFE_DELETE(m_terrainFloorTextures[TerrainTextureCount]);
    }
}

void SceneNarakuProto::ReleaseTerrainFloorBatch()
{
    for (MeshBuffer*& mesh : m_terrainFloorMeshes) SAFE_DELETE(mesh);
    for (Texture*& texture : m_terrainFloorTextures) SAFE_DELETE(texture);
    SAFE_DELETE(m_terrainFloorPS);
    SAFE_DELETE(m_terrainFloorVS);
    for (std::vector<TerrainFloorVertex>& vertices : m_terrainFloorVertices) vertices.clear();
    m_terrainFloorVertexCounts.fill(0);
}

void SceneNarakuProto::RebuildTerrainFloorBatch()
{
    std::size_t quadCapacity = m_ropePoints.size() * 2u + m_layerGates.size();
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }
        quadCapacity += static_cast<std::size_t>(layer.gridWidth - 1) *
            static_cast<std::size_t>(layer.gridHeight - 1) * 2u;
    }

    for (MeshBuffer*& mesh : m_terrainFloorMeshes) SAFE_DELETE(mesh);
    for (std::vector<TerrainFloorVertex>& vertices : m_terrainFloorVertices) vertices.clear();
    m_terrainFloorVertexCounts.fill(0);
    if (quadCapacity == 0 || m_terrainFloorVS == nullptr || m_terrainFloorPS == nullptr)
    {
        return;
    }

    for (std::size_t batchIndex = 0; batchIndex < TerrainBatchCount; ++batchIndex)
    {
        std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
        vertices.resize(quadCapacity * 6u);
        MeshBuffer::Description desc = {};
        desc.pVtx = vertices.data();
        desc.vtxSize = sizeof(TerrainFloorVertex);
        desc.vtxCount = static_cast<UINT>(vertices.size());
        desc.isWrite = true;
        desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        m_terrainFloorMeshes[batchIndex] = new MeshBuffer();
        if (FAILED(m_terrainFloorMeshes[batchIndex]->Create(desc)))
        {
            SAFE_DELETE(m_terrainFloorMeshes[batchIndex]);
            vertices.clear();
        }
    }
}

void SceneNarakuProto::AppendTerrainFloorQuad(
    const DirectX::XMFLOAT3& center,
    const DirectX::XMFLOAT2& size,
    const DirectX::XMFLOAT4& color)
{
    const std::size_t batchIndex = TerrainTextureCount;
    unsigned int& vertexCount = m_terrainFloorVertexCounts[batchIndex];
    std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
    if (vertexCount + 6u > vertices.size())
    {
        return;
    }

    const float halfWidth = size.x * 0.5f;
    const float halfDepth = size.y * 0.5f;
    const DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    const TerrainFloorVertex topLeft = { { center.x - halfWidth, center.y, center.z + halfDepth }, up, { 0.0f, 0.0f }, color };
    const TerrainFloorVertex topRight = { { center.x + halfWidth, center.y, center.z + halfDepth }, up, { 1.0f, 0.0f }, color };
    const TerrainFloorVertex bottomLeft = { { center.x - halfWidth, center.y, center.z - halfDepth }, up, { 0.0f, 1.0f }, color };
    const TerrainFloorVertex bottomRight = { { center.x + halfWidth, center.y, center.z - halfDepth }, up, { 1.0f, 1.0f }, color };

    TerrainFloorVertex* destination = vertices.data() + vertexCount;
    destination[0] = topLeft;
    destination[1] = topRight;
    destination[2] = bottomLeft;
    destination[3] = bottomLeft;
    destination[4] = topRight;
    destination[5] = bottomRight;
    vertexCount += 6u;
}

void SceneNarakuProto::AppendTerrainFloorCell(
    const DirectX::XMFLOAT3& topLeft,
    const DirectX::XMFLOAT3& topRight,
    const DirectX::XMFLOAT3& bottomLeft,
    const DirectX::XMFLOAT3& bottomRight,
    const DirectX::XMFLOAT3& topLeftNormal,
    const DirectX::XMFLOAT3& topRightNormal,
    const DirectX::XMFLOAT3& bottomLeftNormal,
    const DirectX::XMFLOAT3& bottomRightNormal,
    const DirectX::XMFLOAT4& color,
    int textureId)
{
    const std::size_t batchIndex = static_cast<std::size_t>(std::max(0, std::min(textureId, 3)));
    unsigned int& vertexCount = m_terrainFloorVertexCounts[batchIndex];
    std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
    if (vertexCount + 6u > vertices.size()) return;

    const TerrainFloorVertex a = { topLeft, topLeftNormal, { 0.0f, 0.0f }, color };
    const TerrainFloorVertex b = { topRight, topRightNormal, { 1.0f, 0.0f }, color };
    const TerrainFloorVertex c = { bottomLeft, bottomLeftNormal, { 0.0f, 1.0f }, color };
    const TerrainFloorVertex d = { bottomRight, bottomRightNormal, { 1.0f, 1.0f }, color };
    TerrainFloorVertex* destination = vertices.data() + vertexCount;
    destination[0] = a;
    destination[1] = b;
    destination[2] = c;
    destination[3] = c;
    destination[4] = b;
    destination[5] = d;
    vertexCount += 6u;
}

void SceneNarakuProto::AppendTerrainFloorOverlayCell(
    const DirectX::XMFLOAT3& topLeft,
    const DirectX::XMFLOAT3& topRight,
    const DirectX::XMFLOAT3& bottomLeft,
    const DirectX::XMFLOAT3& bottomRight,
    const DirectX::XMFLOAT4& color)
{
    const std::size_t batchIndex = TerrainTextureCount;
    unsigned int& vertexCount = m_terrainFloorVertexCounts[batchIndex];
    std::vector<TerrainFloorVertex>& vertices = m_terrainFloorVertices[batchIndex];
    if (vertexCount + 6u > vertices.size()) return;

    const DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
    const TerrainFloorVertex a = { topLeft, up, { 0.0f, 0.0f }, color };
    const TerrainFloorVertex b = { topRight, up, { 1.0f, 0.0f }, color };
    const TerrainFloorVertex c = { bottomLeft, up, { 0.0f, 1.0f }, color };
    const TerrainFloorVertex d = { bottomRight, up, { 1.0f, 1.0f }, color };
    TerrainFloorVertex* destination = vertices.data() + vertexCount;
    destination[0] = a;
    destination[1] = b;
    destination[2] = c;
    destination[3] = c;
    destination[4] = b;
    destination[5] = d;
    vertexCount += 6u;
}

void SceneNarakuProto::DrawTerrainFloorBatch(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection)
{
    if (m_terrainFloorVS == nullptr || m_terrainFloorPS == nullptr)
    {
        return;
    }

    DirectX::XMFLOAT4X4 matrices[2] = { view, projection };
    m_terrainFloorVS->WriteBuffer(0, matrices);
    m_terrainFloorVS->Bind();
    ShaderList::ExtendedLight light = BuildSceneLight();
    m_terrainFloorPS->WriteBuffer(0, &light);
    m_terrainFloorPS->Bind();
    for (std::size_t batchIndex = 0; batchIndex < TerrainBatchCount; ++batchIndex)
    {
        MeshBuffer* mesh = m_terrainFloorMeshes[batchIndex];
        const unsigned int vertexCount = m_terrainFloorVertexCounts[batchIndex];
        if (mesh == nullptr || vertexCount == 0 || m_terrainFloorTextures[batchIndex] == nullptr) continue;
        SetBlendMode(batchIndex < TerrainTextureCount ? BLEND_NONE : BLEND_ALPHA);
        m_terrainFloorPS->SetTexture(0, m_terrainFloorTextures[batchIndex]);
        mesh->Write(m_terrainFloorVertices[batchIndex].data());
        mesh->Draw(static_cast<int>(vertexCount));
    }
}

void SceneNarakuProto::InitializeEnemyBillboardBatch()
{
    const char* vertexShaderCode = R"HLSL(
struct VS_IN {
    float3 position : POSITION0;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
struct VS_OUT {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
cbuffer Matrix : register(b0) {
    float4x4 view;
    float4x4 projection;
};
VS_OUT main(VS_IN input) {
    VS_OUT output;
    output.position = mul(float4(input.position, 1.0f), view);
    output.position = mul(output.position, projection);
    output.uv = input.uv;
    output.color = input.color;
    return output;
})HLSL";

    const char* pixelShaderCode = R"HLSL(
struct PS_IN {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};
Texture2D enemyTexture : register(t0);
SamplerState enemySampler : register(s0);
float4 main(PS_IN input) : SV_TARGET {
    float4 color = enemyTexture.Sample(enemySampler, input.uv) * input.color;
    clip(color.a - 0.01f);
    return color;
})HLSL";

    m_enemyBillboardVS = new VertexShader();
    if (FAILED(m_enemyBillboardVS->Compile(vertexShaderCode)))
    {
        SAFE_DELETE(m_enemyBillboardVS);
    }

    m_enemyBillboardPS = new PixelShader();
    if (FAILED(m_enemyBillboardPS->Compile(pixelShaderCode)))
    {
        SAFE_DELETE(m_enemyBillboardPS);
    }

    const char* texturePaths[] =
    {
        "Assets/Enemy/skeleton.png",
        "Assets/Enemy/monster.png",
    };
    for (std::size_t index = 0; index < m_enemyTextures.size(); ++index)
    {
        m_enemyTextures[index] = new Texture();
        if (FAILED(m_enemyTextures[index]->Create(texturePaths[index])))
        {
            SAFE_DELETE(m_enemyTextures[index]);
        }
    }
}

void SceneNarakuProto::ReleaseEnemyBillboardBatch()
{
    SAFE_DELETE(m_enemyBillboardMesh);
    SAFE_DELETE(m_enemyBillboardPS);
    SAFE_DELETE(m_enemyBillboardVS);
    for (Texture*& texture : m_enemyTextures)
    {
        SAFE_DELETE(texture);
    }
    m_enemyBillboardVertices.clear();
    m_enemyBillboardVertexCount = 0;
}

void SceneNarakuProto::RebuildEnemyBillboardBatch()
{
    SAFE_DELETE(m_enemyBillboardMesh);
    m_enemyBillboardVertices.clear();
    m_enemyBillboardVertexCount = 0;
    if (m_enemies.empty() || m_enemyBillboardVS == nullptr ||
        m_enemyBillboardPS == nullptr ||
        (m_enemyTextures[0] == nullptr && m_enemyTextures[1] == nullptr))
    {
        return;
    }

    m_enemyBillboardVertices.resize(m_enemies.size() * 6u);
    MeshBuffer::Description desc = {};
    desc.pVtx = m_enemyBillboardVertices.data();
    desc.vtxSize = sizeof(EnemyBillboardVertex);
    desc.vtxCount = static_cast<UINT>(m_enemyBillboardVertices.size());
    desc.isWrite = true;
    desc.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    m_enemyBillboardMesh = new MeshBuffer();
    if (FAILED(m_enemyBillboardMesh->Create(desc)))
    {
        SAFE_DELETE(m_enemyBillboardMesh);
        m_enemyBillboardVertices.clear();
    }
}

void SceneNarakuProto::DrawEnemyBillboardBatch(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    EnemyType enemyType,
    Texture* texture)
{
    using namespace DirectX;

    if (m_enemyBillboardMesh == nullptr || m_enemyBillboardVS == nullptr ||
        m_enemyBillboardPS == nullptr || texture == nullptr)
    {
        return;
    }

    constexpr float billboardWidth = 2.4f;
    constexpr float billboardHeight = 2.4f;
    const XMMATRIX viewMatrix = XMMatrixTranspose(XMLoadFloat4x4(&view));
    XMMATRIX billboard = XMMatrixInverse(nullptr, viewMatrix);
    billboard.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    const Vec2 cameraForward = GetCameraForward();
    const Vec2 cameraRight = GetCameraRight();

    m_enemyBillboardVertexCount = 0;
    for (const EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive || enemy.type != enemyType ||
            m_enemyBillboardVertexCount + 6u > m_enemyBillboardVertices.size())
        {
            continue;
        }

        const XMFLOAT3 center = ToWorld3D(
            enemy.pos,
            enemy.depth,
            billboardHeight * 0.5f);
        const XMMATRIX world =
            XMMatrixScaling(billboardWidth, billboardHeight, 1.0f) *
            billboard *
            XMMatrixTranslation(center.x, center.y, center.z);

        const bool attacking = enemy.telegraphTimer > 0.0f || enemy.chargeTimer > 0.0f;
        const float animationFps = attacking ? kEnemyAttackAnimationFps : kEnemyMoveAnimationFps;
        const CharacterFrameSequence sequence = GetCharacterFrameSequence(
            Dot(enemy.facing, cameraRight),
            Dot(enemy.facing, cameraForward),
            enemy.moving || attacking);
        const int sequenceFrame =
            static_cast<int>(m_characterAnimationTime * animationFps) % sequence.count;
        const int frame = sequence.frames[sequenceFrame];
        const int frameX = frame % kCharacterSpriteColumns;
        const int frameY = frame / kCharacterSpriteColumns;
        const float uvLeft = static_cast<float>(
            sequence.mirror ? frameX + 1 : frameX) / static_cast<float>(kCharacterSpriteColumns);
        const float uvRight = static_cast<float>(
            sequence.mirror ? frameX : frameX + 1) / static_cast<float>(kCharacterSpriteColumns);
        const float uvTop = static_cast<float>(frameY) / static_cast<float>(kCharacterSpriteRows);
        const float uvBottom = static_cast<float>(frameY + 1) / static_cast<float>(kCharacterSpriteRows);
        const XMFLOAT3 sceneLight = CalculateSceneLightColor(enemy.pos, enemy.depth, center.y);
        const XMFLOAT4 lightColor = { sceneLight.x, sceneLight.y, sceneLight.z, 1.0f };

        XMFLOAT3 topLeft = {};
        XMFLOAT3 topRight = {};
        XMFLOAT3 bottomLeft = {};
        XMFLOAT3 bottomRight = {};
        XMStoreFloat3(&topLeft, XMVector3TransformCoord(XMVectorSet(-0.5f, 0.5f, 0.0f, 1.0f), world));
        XMStoreFloat3(&topRight, XMVector3TransformCoord(XMVectorSet(0.5f, 0.5f, 0.0f, 1.0f), world));
        XMStoreFloat3(&bottomLeft, XMVector3TransformCoord(XMVectorSet(-0.5f, -0.5f, 0.0f, 1.0f), world));
        XMStoreFloat3(&bottomRight, XMVector3TransformCoord(XMVectorSet(0.5f, -0.5f, 0.0f, 1.0f), world));

        EnemyBillboardVertex* destination =
            m_enemyBillboardVertices.data() + m_enemyBillboardVertexCount;
        destination[0] = { topLeft, { uvLeft, uvTop }, lightColor };
        destination[1] = { topRight, { uvRight, uvTop }, lightColor };
        destination[2] = { bottomLeft, { uvLeft, uvBottom }, lightColor };
        destination[3] = { bottomLeft, { uvLeft, uvBottom }, lightColor };
        destination[4] = { topRight, { uvRight, uvTop }, lightColor };
        destination[5] = { bottomRight, { uvRight, uvBottom }, lightColor };
        m_enemyBillboardVertexCount += 6u;
    }

    if (m_enemyBillboardVertexCount == 0)
    {
        return;
    }

    XMFLOAT4X4 matrices[2] = { view, projection };
    m_enemyBillboardVS->WriteBuffer(0, matrices);
    m_enemyBillboardVS->Bind();
    m_enemyBillboardPS->SetTexture(0, texture);
    m_enemyBillboardPS->Bind();
    m_enemyBillboardMesh->Write(m_enemyBillboardVertices.data());

    SetCullingMode(D3D11_CULL_NONE);
    SetBlendMode(BLEND_ALPHA);
    SetSamplerState(SAMPLER_POINT);
    m_enemyBillboardMesh->Draw(static_cast<int>(m_enemyBillboardVertexCount));
    SetSamplerState(SAMPLER_LINEAR);
}

bool SceneNarakuProto::ResetRun(bool generateCompleteEditorPreview)
{
#if !defined(NARAKU_EDITOR_BUILD)
    (void)generateCompleteEditorPreview;
#endif
    int keepMoney = m_money;
    bool generated = false;
    std::string mapError;

    LoadEnvironmentModels();

#if !defined(NARAKU_EDITOR_BUILD)
    CaptureWeeklyWorld();
    if (m_weekResetPending)
    {
        m_weeklyAreas.clear();
        m_weekResetPending = false;
    }
    m_diveWorldSeed = m_weekSeed;
#endif

    m_player = PlayerState();
    m_player.hp = GetMaxHp();
    m_player.stamina = GetMaxStamina();
    m_player.mental = GetMaxMental();
    m_inventory.clear();
    m_foodCount = 0;
    m_heatedFoodCount = 0;
    m_rationOneCount = 0;
    m_rawFishCount = 0;
    m_cookedFishCount = 0;
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_cartridgeCount = 0;
    m_waterBottles.clear();
    m_cookingKits.clear();
    m_groundRelics.clear();
    m_runRelicAcquisitionDepths.clear();
    m_groundFoods.clear();
    m_miningPoints.clear();
    m_fishingPoints.clear();
    m_enemies.clear();
    m_attackHitEffects.clear();
    m_jumpEffects.clear();
    m_characterAnimationTime = 0.0f;
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_layerGates.clear();
    m_areas.clear();
    m_currentAreaIndex = -1;
    m_activeRope = -1;
    m_ropeProgress = 0.0f;
    m_pins.clear();
    m_surfaceFacilities.clear();
    m_messages.clear();
    m_centerNotification.clear();
    m_centerNotificationTimer = 0.0f;
    m_loadingSourceGateIndex = -1;
    m_pendingUninsuredGateIndex = -1;
    m_uninsuredDescentAcceptedThisDive = false;
    m_loadingStep = 0;
    m_loadingProgress = 0.0f;
    m_loadingStatus.clear();
    m_generationFailureSummary.clear();
    m_generationFailureDetail.clear();
    m_openGenerationFailurePopup = false;
    m_transitionSourceGateIndex = -1;
    m_transitionDestinationAreaIndex = -1;
    m_transitionDestinationGateIndex = -1;
    m_layerTransitionProgress = 0.0f;
    m_layerTransitionVisualOffset = 0.0f;
    m_layerTransitionAscending = false;
    m_heavyRunNotificationShown = false;
    m_shiftHold = 0.0f;
    m_shiftPendingStep = false;
    m_shiftWasPressed = false;
    m_shiftRunCommitted = false;
    m_qHoldTime = 0.0f;
    m_qWasPressed = false;
    m_qLongTriggered = false;
    m_upperLoadWardTimer = 0.0f;
    m_upperLoadVisionTimer = 0.0f;
    m_upperLoadVisionOcclusion = 0.0f;
    m_upperLoadFifthTimer = 0.0f;
    m_upperLoadFifthDamageRatio = 0.0f;
    m_upperLoadFifthDamageCooldown = 0.0f;
    m_miningSenseTimer = 0.0f;
    m_movementExpByDepth.fill(0.0f);
    m_cameraShakeTimer = 0.0f;
    m_miningTimer = 0.0f;
    m_miningDuration = kMiningTime;
    m_miningIndex = -1;
    m_fishingPhase = FishingPhase::None;
    m_fishingPointIndex = -1;
    m_fishingTimer = 0.0f;
    m_foodUseTimer = 0.0f;
    m_usingHeatedFood = false;
    m_unknownWeaponChargeTimer = 0.0f;
    m_unknownWeaponFiredThisHold = false;
    m_cookingTimer = 0.0f;
    m_cookingPreviousHp = 0.0f;
    m_cookingTarget = CookingTarget::None;
    m_cookingBottleIndex = -1;
    m_pendingWaterFlags = NarakuMap::CellAttributeNone;
    m_selectedInventory = -1;
    m_mapScrollOffset = { 0.0f, 0.0f };
    m_mode = Mode::Explore;
    m_result = RunResult();
    m_pendingDeathCause = DeathCause::Other;
    m_pendingRelicDepth = 0.0f;
    m_pendingRelicMiningIndex = -1;
    m_money = keepMoney;

#if defined(NARAKU_EDITOR_BUILD)
    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    SeedRuntimeRandom(preview.seed);
#else
    SeedRuntimeRandom(m_weekSeed);
#endif

#if !defined(NARAKU_EDITOR_BUILD)
    if (!m_weeklyAreas.empty())
    {
        m_areas = m_weeklyAreas;
        const auto start = std::find_if(m_areas.begin(), m_areas.end(),
            [](const AreaState& area) { return area.canReturn && area.generated; });
        if (start != m_areas.end())
        {
            const int startAreaIndex = static_cast<int>(std::distance(m_areas.begin(), start));
            ActivateArea(startAreaIndex, false);
            m_player.pos = m_startPoint;
            m_player.depth = m_startDepth;
            m_player.previousDepth = m_player.depth;
            m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
            m_player.previousWorldY = m_player.feetWorldY;
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.lastSafeGroundPos = m_player.pos;
            m_player.lastSafeGroundDepth = m_player.depth;
            m_player.hasSafeGroundPos = true;
            EnsureQuestBoard();
            return true;
        }
        m_weeklyAreas.clear();
        m_areas.clear();
    }
#endif

    if (!BuildDiveStructure())
    {
        ReportGenerationFailure(
            u8"15段階の接続構成を生成できませんでした。",
            u8"各段階のエリア数と層間出入口数を満たす接続構成が、最大試行回数内に作成できませんでした。");
        m_mode = Mode::Home;
        return false;
    }

    int initialAreaIndex = -1;
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (m_areas[areaIndex].canReturn) initialAreaIndex = areaIndex;
#if defined(NARAKU_EDITOR_BUILD)
        if (!preview.spawnAtReturnArea &&
            m_areas[areaIndex].depth == preview.depth &&
            m_areas[areaIndex].sublayer == preview.sublayer &&
            m_areas[areaIndex].areaNumber == preview.area)
        {
            initialAreaIndex = areaIndex;
            break;
        }
#endif
    }

    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (areaIndex != initialAreaIndex) continue;
        m_currentAreaIndex = areaIndex;
        int entryCount = 0;
        int exitCount = 0;
        for (const PlannedLayerGate& gate : m_areas[areaIndex].plannedGates)
            gate.isEntry ? ++entryCount : ++exitCount;
        const NarakuStageGenerator::AreaGenerationContext generationContext = {
            m_areas[areaIndex].depth, m_areas[areaIndex].sublayer, m_areas[areaIndex].areaNumber };
#if !defined(NARAKU_EDITOR_BUILD)
        constexpr const wchar_t* generated4x4MapPath = L"Assets/Maps/generated_naraku_map_4x4.json";
#endif
        for (int attempt = 0; attempt < kMapGenerationMaxAttempts; ++attempt)
        {
#if defined(NARAKU_EDITOR_BUILD)
            if (NarakuStageGenerator::GenerateFixed4x4AreaMapData(
                    m_runtimeMap, entryCount, exitCount, m_areas[areaIndex].canReturn, &generationContext, &mapError))
#else
            if (NarakuStageGenerator::GenerateFixed4x4AreaMap(
                    generated4x4MapPath, entryCount, exitCount, m_areas[areaIndex].canReturn, &generationContext, &mapError) &&
                NarakuMap::LoadMap(generated4x4MapPath, m_runtimeMap, &mapError))
#endif
            {
                generated = true;
#if !defined(NARAKU_EDITOR_BUILD)
                NarakuMap::SetCurrentMapPath(generated4x4MapPath);
#endif
                break;
            }
        }
        break;
    }

    if (!generated)
    {
        std::ostringstream summary;
        if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
        {
            const AreaState& failedArea = m_areas[static_cast<std::size_t>(m_currentAreaIndex)];
            summary << u8"初期エリアの生成に失敗しました: 第" << failedArea.depth << u8"層 "
                << GetSublayerName(failedArea.sublayer) << u8" エリア" << failedArea.areaNumber;
        }
        else
        {
            summary << u8"開始エリアを特定できませんでした。";
        }
        ReportGenerationFailure(summary.str(), mapError);
        m_mode = Mode::Home;
        return false;
    }

    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_worldHalfSize = 1.0f;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        const float halfWidth = static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f;
        const float halfHeight = static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f;
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.x) + halfWidth);
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.z) + halfHeight);
    }

    auto getLayerDepthById = [this](int layerId) -> float
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, layerId);
        return (layerIndex >= 0) ? m_runtimeMap.terrainLayers[layerIndex].layerDepth : 0.0f;
    };

    auto getFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    m_startPoint = { m_runtimeMap.playerStartPoint.xz.x, m_runtimeMap.playerStartPoint.xz.z };
    m_startDepth = getLayerDepthById(m_runtimeMap.playerStartPoint.layerId);
    m_returnPoint = m_startPoint;
    m_returnDepth = m_startDepth;

    m_player.pos = m_startPoint;
    m_player.depth = m_startDepth;
    m_player.previousDepth = m_startDepth;
    m_player.facing = { 0.0f, 1.0f };
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }

        const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
        const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
        m_floorRegions.push_back({
            { layer.center.x, layer.center.z },
            { width * 0.5f, height * 0.5f },
            layer.layerDepth,
            getFloorColor(layer.groundTextureId),
            layer.id });
    }

    for (const NarakuMap::RopePoint& rope : m_runtimeMap.ropes)
    {
        const int topIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.topLayerId);
        const int bottomIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.bottomLayerId);
        if (topIndex < 0 || bottomIndex < 0)
        {
            continue;
        }

        m_ropePoints.push_back({
            { rope.topXZ.x, rope.topXZ.z },
            { rope.bottomXZ.x, rope.bottomXZ.z },
            m_runtimeMap.terrainLayers[topIndex].layerDepth,
            m_runtimeMap.terrainLayers[bottomIndex].layerDepth });
    }

    for (const NarakuMap::LayerGatePoint& gate : m_runtimeMap.layerGates)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, gate.layerId);
        if (layerIndex < 0)
        {
            continue;
        }
        LayerGateState runtimeGate;
        runtimeGate.isEntry = gate.isEntry;
        runtimeGate.ropePos = { gate.ropeXZ.x, gate.ropeXZ.z };
        runtimeGate.loadPos = { gate.loadXZ.x, gate.loadXZ.z };
        runtimeGate.depth = m_runtimeMap.terrainLayers[layerIndex].layerDepth;
        m_layerGates.push_back(runtimeGate);
    }

    if (!AssignPlannedGates(m_currentAreaIndex))
    {
        const AreaState& failedArea = m_areas[static_cast<std::size_t>(m_currentAreaIndex)];
        std::ostringstream summary;
        summary << u8"層間出入口の割り当てに失敗しました: 第" << failedArea.depth << u8"層 "
            << GetSublayerName(failedArea.sublayer) << u8" エリア" << failedArea.areaNumber;
        ReportGenerationFailure(summary.str(), u8"生成された層間出入口の数または入口・出口の種別が、接続計画と一致しません。");
        m_mode = Mode::Home;
        return false;
    }

    int fallbackRelicIndex = 0;
    for (const NarakuMap::MiningPoint& point : m_runtimeMap.miningPoints)
    {
        if (!point.enabled)
        {
            continue;
        }

        MiningPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.visualType = point.visualType;
        runtimePoint.discovered = point.discovered;
        /** 再潜行ごとに採掘状態を初期化し、今回の潜行中だけ更新します。 */
        runtimePoint.mined = false;
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.relicName = point.relicName.empty() ? kRelicNames[fallbackRelicIndex % 8] : point.relicName;

        ++fallbackRelicIndex;
        m_miningPoints.push_back(runtimePoint);
    }

    SpawnEnemiesForCurrentArea();
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
    AddMessage(u8"奈落塔プロトタイプを開始しました。");
    if (generated)
    {
        AreaState& startArea = m_areas[m_currentAreaIndex];
        startArea.generated = true;
        startArea.firstAreaExpAwarded = true;
        startArea.firstAreaRewardAwarded = true;
        m_result.firstAreaCount = 1;
        AwardExp(static_cast<int>(100.0f * GetDepthExpMultiplier(1)));
        SaveCurrentAreaState();
#if defined(NARAKU_EDITOR_BUILD)
        if (generateCompleteEditorPreview)
        {
            const int previewStartArea = m_currentAreaIndex;
            for (int previewArea = 0; previewArea < static_cast<int>(m_areas.size()); ++previewArea)
            {
                if (previewArea == previewStartArea) continue;
                std::string previewError;
                if (!GeneratePlannedArea(previewArea, previewError))
                {
                    m_mode = Mode::Home;
                    const AreaState& failedArea = m_areas[static_cast<std::size_t>(previewArea)];
                    std::ostringstream failure;
                    failure << u8"生成プレビュー失敗: 第" << failedArea.depth << u8"層 "
                        << GetSublayerName(failedArea.sublayer) << u8" エリア" << failedArea.areaNumber
                        << " / seed=" << preview.seed;
                    ReportGenerationFailure(failure.str(), previewError);
                    return false;
                }
            }
            ActivateArea(previewStartArea, false);
            m_player.pos = m_startPoint;
            m_player.depth = m_startDepth;
            m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        }
#endif
    }
    EnsureQuestBoard();
    if (!generated)
    {
        m_mode = Mode::Home;
    }
    return generated;
}

bool SceneNarakuProto::BuildDiveStructure()
{
    constexpr int stageCount = 15;
    constexpr int maximumGatesPerArea = 4;
    constexpr int maximumAttempts = 2000;

    const EditorPreviewConfiguration preview = LoadEditorPreviewConfiguration();
    for (int attempt = 0; attempt < maximumAttempts; ++attempt)
    {
        m_areas.clear();
        std::array<std::vector<int>, stageCount> stageAreas;
        for (int stage = 0; stage < stageCount; ++stage)
        {
            const int configuredAreaCount = preview.areaCounts[static_cast<std::size_t>(stage)];
            const int areaCount = configuredAreaCount >= 1 && configuredAreaCount <= 3
                ? configuredAreaCount + 1 : RandomInt(2, 4);
            for (int number = 0; number < areaCount; ++number)
            {
                const int areaIndex = static_cast<int>(m_areas.size());
                m_areas.emplace_back();
                AreaState& area = m_areas.back();
                area.depth = stage / 3 + 1;
                area.sublayer = stage % 3;
                area.areaNumber = number + 1;
                stageAreas[stage].push_back(areaIndex);
            }
        }

        const int startAreaIndex = stageAreas[0][RandomInt(0, static_cast<int>(stageAreas[0].size()) - 1)];
        m_areas[startAreaIndex].canReturn = true;
        int nextConnectionId = 1;
        bool failed = false;

        const auto shuffledIndices = [](int count)
        {
            std::vector<int> result(static_cast<std::size_t>(count));
            for (int index = 0; index < count; ++index) result[static_cast<std::size_t>(index)] = index;
            for (int index = count - 1; index > 0; --index)
            {
                const int swapIndex = RandomInt(0, index);
                std::swap(result[static_cast<std::size_t>(index)], result[static_cast<std::size_t>(swapIndex)]);
            }
            return result;
        };

        for (int stage = 0; stage < stageCount - 1 && !failed; ++stage)
        {
            const int upperCount = static_cast<int>(stageAreas[stage].size());
            const int lowerCount = static_cast<int>(stageAreas[stage + 1].size());
            std::vector<std::pair<int, int>> edges;

            if (stage == 0)
            {
                const int edgeCount = upperCount + lowerCount - 1 + RandomInt(0, 1);
                bool connected = false;
                for (int edgeAttempt = 0; edgeAttempt < 1000 && !connected; ++edgeAttempt)
                {
                    edges.clear();
                    std::vector<int> upperDegree(static_cast<std::size_t>(upperCount), 0);
                    std::vector<int> lowerDegree(static_cast<std::size_t>(lowerCount), 0);
                    for (int edge = 0; edge < edgeCount; ++edge)
                    {
                        const int upper = RandomInt(0, upperCount - 1);
                        const int lower = RandomInt(0, lowerCount - 1);
                        edges.push_back({ upper, lower });
                        ++upperDegree[static_cast<std::size_t>(upper)];
                        ++lowerDegree[static_cast<std::size_t>(lower)];
                    }
                    if (std::find(upperDegree.begin(), upperDegree.end(), 0) != upperDegree.end() ||
                        std::find(lowerDegree.begin(), lowerDegree.end(), 0) != lowerDegree.end()) continue;

                    std::vector<bool> reachedUpper(static_cast<std::size_t>(upperCount), false);
                    std::vector<bool> reachedLower(static_cast<std::size_t>(lowerCount), false);
                    reachedUpper[static_cast<std::size_t>(
                        std::find(stageAreas[0].begin(), stageAreas[0].end(), startAreaIndex) - stageAreas[0].begin())] = true;
                    bool changed = true;
                    while (changed)
                    {
                        changed = false;
                        for (const auto& edge : edges)
                        {
                            if (reachedUpper[static_cast<std::size_t>(edge.first)] && !reachedLower[static_cast<std::size_t>(edge.second)])
                            { reachedLower[static_cast<std::size_t>(edge.second)] = true; changed = true; }
                            if (reachedLower[static_cast<std::size_t>(edge.second)] && !reachedUpper[static_cast<std::size_t>(edge.first)])
                            { reachedUpper[static_cast<std::size_t>(edge.first)] = true; changed = true; }
                        }
                    }
                    connected = std::find(reachedUpper.begin(), reachedUpper.end(), false) == reachedUpper.end() &&
                        std::find(reachedLower.begin(), reachedLower.end(), false) == reachedLower.end();
                }
                if (!connected) failed = true;
            }
            else
            {
                const std::vector<int> upperOrder = shuffledIndices(upperCount);
                const std::vector<int> lowerOrder = shuffledIndices(lowerCount);
                if (upperCount >= lowerCount)
                {
                    for (int index = 0; index < upperCount; ++index)
                        edges.push_back({ upperOrder[static_cast<std::size_t>(index)],
                            lowerOrder[static_cast<std::size_t>(index % lowerCount)] });
                }
                else
                {
                    for (int index = 0; index < lowerCount; ++index)
                        edges.push_back({ upperOrder[static_cast<std::size_t>(index % upperCount)],
                            lowerOrder[static_cast<std::size_t>(index)] });
                }
                if (RandomInt(0, 1) != 0)
                    edges.push_back({ RandomInt(0, upperCount - 1), RandomInt(0, lowerCount - 1) });
            }

            for (const auto& edge : edges)
            {
                const int upperArea = stageAreas[stage][static_cast<std::size_t>(edge.first)];
                const int lowerArea = stageAreas[stage + 1][static_cast<std::size_t>(edge.second)];
                const int connectionId = nextConnectionId++;
                m_areas[upperArea].plannedGates.push_back({ false, lowerArea, connectionId });
                m_areas[lowerArea].plannedGates.push_back({ true, upperArea, connectionId });
            }
        }

        if (failed) continue;
        for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
        {
            const AreaState& area = m_areas[areaIndex];
            if (area.plannedGates.empty() || static_cast<int>(area.plannedGates.size()) > maximumGatesPerArea)
            { failed = true; break; }
            const bool hasEntry = std::any_of(area.plannedGates.begin(), area.plannedGates.end(),
                [](const PlannedLayerGate& gate) { return gate.isEntry; });
            const bool hasExit = std::any_of(area.plannedGates.begin(), area.plannedGates.end(),
                [](const PlannedLayerGate& gate) { return !gate.isEntry; });
            if (area.sublayer + (area.depth - 1) * 3 > 0 && !hasEntry) { failed = true; break; }
            if (area.sublayer + (area.depth - 1) * 3 < stageCount - 1 && !hasExit) { failed = true; break; }
        }
        if (failed) continue;

        std::vector<bool> reached(m_areas.size(), false);
        std::vector<int> queue = { startAreaIndex };
        reached[static_cast<std::size_t>(startAreaIndex)] = true;
        for (std::size_t cursor = 0; cursor < queue.size(); ++cursor)
        {
            for (const PlannedLayerGate& gate : m_areas[queue[cursor]].plannedGates)
            {
                if (gate.destinationAreaIndex < 0 || reached[static_cast<std::size_t>(gate.destinationAreaIndex)]) continue;
                reached[static_cast<std::size_t>(gate.destinationAreaIndex)] = true;
                queue.push_back(gate.destinationAreaIndex);
            }
        }
        if (std::find(reached.begin(), reached.end(), false) != reached.end()) continue;
        return true;
    }
    m_areas.clear();
    return false;
}

bool SceneNarakuProto::AssignPlannedGates(int areaIndex)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size())) return false;
    const std::vector<PlannedLayerGate>& planned = m_areas[areaIndex].plannedGates;
    std::vector<bool> assigned(planned.size(), false);
    for (LayerGateState& gate : m_layerGates)
    {
        bool found = false;
        for (std::size_t index = 0; index < planned.size(); ++index)
        {
            if (assigned[index] || planned[index].isEntry != gate.isEntry) continue;
            gate.destinationAreaIndex = planned[index].destinationAreaIndex;
            gate.connectionId = planned[index].connectionId;
            assigned[index] = true;
            found = true;
            break;
        }
        if (!found) return false;
    }
    return m_layerGates.size() == planned.size() &&
        std::find(assigned.begin(), assigned.end(), false) == assigned.end();
}

bool SceneNarakuProto::GeneratePlannedArea(int areaIndex, std::string& outError)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size()))
    {
        outError = u8"生成対象のエリア番号が範囲外です。";
        return false;
    }
    AreaState& area = m_areas[areaIndex];
    int entryCount = 0;
    int exitCount = 0;
    for (const PlannedLayerGate& gate : area.plannedGates) gate.isEntry ? ++entryCount : ++exitCount;
    const NarakuStageGenerator::AreaGenerationContext generationContext = {
        area.depth, area.sublayer, area.areaNumber };

    wchar_t generatedPath[128] = {};
#if !defined(NARAKU_EDITOR_BUILD)
    std::swprintf(generatedPath, 128, L"Assets/Maps/generated_area_%03d.json", areaIndex);
#endif
    NarakuMap::MapData generatedMap;
    bool generated = false;
    for (int attempt = 0; attempt < kMapGenerationMaxAttempts; ++attempt)
    {
#if defined(NARAKU_EDITOR_BUILD)
        if (NarakuStageGenerator::GenerateFixed4x4AreaMapData(
                generatedMap, entryCount, exitCount, area.canReturn, &generationContext, &outError))
#else
        if (NarakuStageGenerator::GenerateFixed4x4AreaMap(
                generatedPath, entryCount, exitCount, area.canReturn, &generationContext, &outError) &&
            NarakuMap::LoadMap(generatedPath, generatedMap, &outError))
#endif
        { generated = true; break; }
    }
    if (!generated) return false;

    m_runtimeMap = std::move(generatedMap);
    m_currentAreaIndex = areaIndex;
    BuildCurrentAreaRuntime(false);
    if (!AssignPlannedGates(areaIndex))
    {
        outError = u8"生成された層間出入口の数または入口・出口の種別が、接続計画と一致しません。";
        return false;
    }
    area.generated = true;
    SaveCurrentAreaState();
    return true;
}

const char* SceneNarakuProto::GetSublayerName(int sublayer) const
{
    switch (sublayer)
    {
    case 0: return u8"上層";
    case 1: return u8"中層";
    case 2: return u8"下層";
    default: return u8"不明";
    }
}

void SceneNarakuProto::SaveCurrentAreaState()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    AreaState& area = m_areas[m_currentAreaIndex];
    area.map = m_runtimeMap;
    area.groundRelics = m_groundRelics;
    area.groundFoods = m_groundFoods;
    area.miningPoints = m_miningPoints;
    area.fishingPoints = m_fishingPoints;
    area.enemies = m_enemies;
    area.floorRegions = m_floorRegions;
    area.ropePoints = m_ropePoints;
    area.layerGates = m_layerGates;
    area.pins = m_pins;
    area.startPoint = m_startPoint;
    area.startDepth = m_startDepth;
    area.returnPoint = m_returnPoint;
    area.returnDepth = m_returnDepth;
    area.worldHalfSize = m_worldHalfSize;
}

void SceneNarakuProto::CaptureWeeklyWorld()
{
#if defined(NARAKU_EDITOR_BUILD)
    return;
#else
    SaveCurrentAreaState();
    if (!m_areas.empty() && !m_weekResetPending) m_weeklyAreas = m_areas;
#endif
}

bool SceneNarakuProto::SaveWeeklyWorld() const
{
#if defined(NARAKU_EDITOR_BUILD)
    return true;
#else
    _wmkdir(kWeeklyWorldDirectory);
    std::ofstream stream(kWeeklyWorldTempPath, std::ios::binary | std::ios::trunc);
    if (!stream) return false;
    const auto write = [&stream](const auto& value)
    { stream.write(reinterpret_cast<const char*>(&value), sizeof(value)); };
    const auto writeString = [&stream, &write](const std::string& value)
    {
        const std::uint64_t size = value.size();
        write(size);
        stream.write(value.data(), static_cast<std::streamsize>(size));
    };
    const auto writePodVector = [&stream, &write](const auto& values)
    {
        const std::uint64_t size = values.size();
        write(size);
        if (size > 0) stream.write(reinterpret_cast<const char*>(values.data()),
            static_cast<std::streamsize>(sizeof(values.front()) * size));
    };
    const std::uint32_t magic = 0x37574E4E;
    write(magic);
    write(m_diveWorldSeed);
    const std::uint64_t areaCount = m_weeklyAreas.size();
    write(areaCount);
    for (std::size_t areaIndex = 0; areaIndex < m_weeklyAreas.size(); ++areaIndex)
    {
        const AreaState& area = m_weeklyAreas[areaIndex];
        write(area.depth); write(area.sublayer); write(area.areaNumber); write(area.generated); write(area.canReturn);
        writePodVector(area.plannedGates);
        write(area.sensingTimer); write(area.respawnClock);
        write(area.discoveredEnemyCount); write(area.discoveredMiningCount); write(area.discoveredCliffCount);
        write(area.totalCliffCount); write(area.firstAreaExpAwarded); write(area.firstAreaRewardAwarded);
        writePodVector(area.discoveredCells); writePodVector(area.discoveredCliffs); write(area.cellExpThresholds);
        if (!area.generated) continue;
        wchar_t mapPath[160] = {};
        std::swprintf(mapPath, 160, L"Assets/Save/WeeklyWorld/area_%03zu.json", areaIndex);
        std::string mapError;
        if (!NarakuMap::SaveMap(mapPath, area.map, &mapError)) return false;
        const std::uint64_t relicCount = area.groundRelics.size(); write(relicCount);
        for (const GroundRelic& relic : area.groundRelics)
        {
            writeString(relic.item.name); write(relic.item.type); write(relic.item.weight); write(relic.item.value);
            write(relic.item.maxUses); write(relic.item.remainingUses); write(relic.item.acquisitionOrder);
            write(relic.item.broken); write(relic.item.stabilized); write(relic.item.autoTrigger);
            write(relic.pos); write(relic.depth); write(relic.active); write(relic.sourceMiningIndex);
        }
        writePodVector(area.groundFoods);
        const std::uint64_t miningCount = area.miningPoints.size(); write(miningCount);
        for (const MiningPoint& point : area.miningPoints)
        {
            write(point.pos); write(point.depth); write(point.visualType); write(point.discovered); write(point.mined);
            write(point.respawnTimer); write(point.extractionCount); write(point.outputPending); write(point.sensed);
            writeString(point.relicName);
        }
        const std::uint64_t fishingCount = area.fishingPoints.size(); write(fishingCount);
        for (const FishingPoint& point : area.fishingPoints)
        {
            write(point.pos); write(point.depth); write(point.lake); write(point.discovered);
            write(point.remainingUses); write(point.rechargeGameSeconds); writeString(point.id);
        }
        writePodVector(area.enemies); writePodVector(area.layerGates); writePodVector(area.pins);
    }
    stream.close();
    if (!stream.good()) return false;
    return MoveFileExW(kWeeklyWorldTempPath, kWeeklyWorldPath,
        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
#endif
}

bool SceneNarakuProto::LoadWeeklyWorld()
{
#if defined(NARAKU_EDITOR_BUILD)
    return false;
#else
    std::ifstream stream(kWeeklyWorldPath, std::ios::binary);
    if (!stream) return false;
    const auto read = [&stream](auto& value)
    { stream.read(reinterpret_cast<char*>(&value), sizeof(value)); };
    const auto readString = [&stream, &read](std::string& value)
    {
        std::uint64_t size = 0; read(size);
        if (size > 1024 * 1024) { stream.setstate(std::ios::failbit); return; }
        value.resize(static_cast<std::size_t>(size));
        if (size > 0) stream.read(&value[0], static_cast<std::streamsize>(size));
    };
    const auto readPodVector = [&stream, &read](auto& values)
    {
        std::uint64_t size = 0; read(size);
        if (size > 1000000) { stream.setstate(std::ios::failbit); return; }
        values.resize(static_cast<std::size_t>(size));
        if (size > 0) stream.read(reinterpret_cast<char*>(values.data()),
            static_cast<std::streamsize>(sizeof(values.front()) * size));
    };
    std::uint32_t magic = 0; read(magic);
    std::uint64_t savedSeed = 0; read(savedSeed);
    if (magic != 0x37574E4E || savedSeed != m_weekSeed || m_weekResetPending) return false;
    std::uint64_t areaCount = 0; read(areaCount);
    if (areaCount == 0 || areaCount > 1000) return false;
    m_weeklyAreas.clear(); m_weeklyAreas.resize(static_cast<std::size_t>(areaCount));
    for (std::size_t areaIndex = 0; areaIndex < m_weeklyAreas.size(); ++areaIndex)
    {
        AreaState& area = m_weeklyAreas[areaIndex];
        read(area.depth); read(area.sublayer); read(area.areaNumber); read(area.generated); read(area.canReturn);
        readPodVector(area.plannedGates);
        read(area.sensingTimer); read(area.respawnClock);
        read(area.discoveredEnemyCount); read(area.discoveredMiningCount); read(area.discoveredCliffCount);
        read(area.totalCliffCount); read(area.firstAreaExpAwarded); read(area.firstAreaRewardAwarded);
        readPodVector(area.discoveredCells); readPodVector(area.discoveredCliffs); read(area.cellExpThresholds);
        if (!area.generated) continue;
        wchar_t mapPath[160] = {};
        std::swprintf(mapPath, 160, L"Assets/Save/WeeklyWorld/area_%03zu.json", areaIndex);
        std::string mapError;
        if (!NarakuMap::LoadMap(mapPath, area.map, &mapError)) { m_weeklyAreas.clear(); return false; }
        std::uint64_t relicCount = 0; read(relicCount);
        if (relicCount > 100000) return false;
        area.groundRelics.resize(static_cast<std::size_t>(relicCount));
        for (GroundRelic& relic : area.groundRelics)
        {
            readString(relic.item.name); read(relic.item.type); read(relic.item.weight); read(relic.item.value);
            read(relic.item.maxUses); read(relic.item.remainingUses); read(relic.item.acquisitionOrder);
            read(relic.item.broken); read(relic.item.stabilized); read(relic.item.autoTrigger);
            read(relic.pos); read(relic.depth); read(relic.active); read(relic.sourceMiningIndex);
        }
        readPodVector(area.groundFoods);
        std::uint64_t miningCount = 0; read(miningCount);
        if (miningCount > 100000) return false;
        area.miningPoints.resize(static_cast<std::size_t>(miningCount));
        for (MiningPoint& point : area.miningPoints)
        {
            read(point.pos); read(point.depth); read(point.visualType); read(point.discovered); read(point.mined);
            read(point.respawnTimer); read(point.extractionCount); read(point.outputPending); read(point.sensed);
            readString(point.relicName);
        }
        std::uint64_t fishingCount = 0; read(fishingCount);
        if (fishingCount > 100000) return false;
        area.fishingPoints.resize(static_cast<std::size_t>(fishingCount));
        for (FishingPoint& point : area.fishingPoints)
        {
            read(point.pos); read(point.depth); read(point.lake); read(point.discovered);
            read(point.remainingUses); read(point.rechargeGameSeconds); readString(point.id);
        }
        readPodVector(area.enemies); readPodVector(area.layerGates); readPodVector(area.pins);
    }
    if (!stream.good()) { m_weeklyAreas.clear(); return false; }
    m_diveWorldSeed = savedSeed;
    return RestoreWeeklyAreaRuntimes();
#endif
}

bool SceneNarakuProto::RestoreWeeklyAreaRuntimes()
{
#if defined(NARAKU_EDITOR_BUILD)
    return false;
#else
    m_areas = m_weeklyAreas;
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        AreaState& area = m_areas[static_cast<std::size_t>(areaIndex)];
        if (!area.generated) continue;
        const std::vector<GroundRelic> groundRelics = area.groundRelics;
        const std::vector<GroundFood> groundFoods = area.groundFoods;
        const std::vector<MiningPoint> miningPoints = area.miningPoints;
        const std::vector<FishingPoint> fishingPoints = area.fishingPoints;
        const std::vector<EnemyState> enemies = area.enemies;
        const std::vector<LayerGateState> layerGates = area.layerGates;
        const std::vector<Vec2> pins = area.pins;
        const int discoveredEnemyCount = area.discoveredEnemyCount;
        const int discoveredMiningCount = area.discoveredMiningCount;
        const int discoveredCliffCount = area.discoveredCliffCount;
        const int totalCliffCount = area.totalCliffCount;
        const bool firstAreaExpAwarded = area.firstAreaExpAwarded;
        const bool firstAreaRewardAwarded = area.firstAreaRewardAwarded;
        const std::vector<std::uint8_t> discoveredCells = area.discoveredCells;
        const std::vector<std::uint8_t> discoveredCliffs = area.discoveredCliffs;
        const std::array<bool, 4> cellExpThresholds = area.cellExpThresholds;
        m_currentAreaIndex = areaIndex;
        m_runtimeMap = area.map;
        BuildCurrentAreaRuntime(false);
        if (!AssignPlannedGates(areaIndex)) return false;
        m_groundRelics = groundRelics; m_groundFoods = groundFoods; m_miningPoints = miningPoints;
        m_fishingPoints = fishingPoints;
        m_enemies = enemies; m_layerGates = layerGates; m_pins = pins;
        area.discoveredEnemyCount = discoveredEnemyCount;
        area.discoveredMiningCount = discoveredMiningCount;
        area.discoveredCliffCount = discoveredCliffCount;
        area.totalCliffCount = totalCliffCount;
        area.firstAreaExpAwarded = firstAreaExpAwarded;
        area.firstAreaRewardAwarded = firstAreaRewardAwarded;
        area.discoveredCells = discoveredCells;
        area.discoveredCliffs = discoveredCliffs;
        area.cellExpThresholds = cellExpThresholds;
        SaveCurrentAreaState();
    }
    m_weeklyAreas = m_areas;
    m_currentAreaIndex = -1;
    return true;
#endif
}

void SceneNarakuProto::ActivateArea(int areaIndex, bool placeAtEntry)
{
    if (areaIndex < 0 || areaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    const AreaState& area = m_areas[areaIndex];
    m_currentAreaIndex = areaIndex;
    m_runtimeMap = area.map;
    m_groundRelics = area.groundRelics;
    m_groundFoods = area.groundFoods;
    m_miningPoints = area.miningPoints;
    m_fishingPoints = area.fishingPoints;
    m_enemies = area.enemies;
    m_floorRegions = area.floorRegions;
    m_ropePoints = area.ropePoints;
    m_layerGates = area.layerGates;
    m_pins = area.pins;
    m_startPoint = area.startPoint;
    m_startDepth = area.startDepth;
    m_returnPoint = area.returnPoint;
    m_returnDepth = area.returnDepth;
    m_worldHalfSize = area.worldHalfSize;
    m_cameraCollisionDistance = -1.0f;
    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_activeRope = -1;
    m_player.onRope = false;

    if (placeAtEntry)
    {
        for (const LayerGateState& gate : m_layerGates)
        {
            if (!gate.isEntry)
            {
                continue;
            }
            m_player.pos = gate.ropePos;
            m_player.depth = gate.depth;
            break;
        }
    }
    m_player.previousDepth = m_player.depth;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.grounded = true;
    m_player.verticalSpeed = 0.0f;
    for (EnemyState& enemy : m_enemies)
        if (!enemy.alive && enemy.respawnTimer <= 0.0f) RespawnEnemy(enemy);
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
#if defined(NARAKU_EDITOR_BUILD)
    if (m_editorOverviewMode) FocusEditorOverviewOnCurrentArea();
#endif
}

void SceneNarakuProto::BuildCurrentAreaRuntime(bool placeAtStart)
{
    m_groundRelics.clear();
    m_groundFoods.clear();
    m_miningPoints.clear();
    m_fishingPoints.clear();
    m_enemies.clear();
    m_attackHitEffects.clear();
    m_jumpEffects.clear();
    m_floorRegions.clear();
    m_ropePoints.clear();
    m_layerGates.clear();
    m_pins.clear();
    m_activeRope = -1;
    m_miningIndex = -1;
    m_autoFallStartHeight = m_runtimeMap.autoFallStartHeight;
    m_worldHalfSize = 1.0f;

    auto getLayerDepthById = [this](int layerId) -> float
    {
        const int index = NarakuMap::FindLayerIndexById(m_runtimeMap, layerId);
        return index >= 0 ? m_runtimeMap.terrainLayers[index].layerDepth : 0.0f;
    };
    auto getFloorColor = [](int textureId) -> DirectX::XMFLOAT4
    {
        switch (textureId)
        {
        case 1: return { 0.42f, 0.33f, 0.20f, 0.20f };
        case 2: return { 0.25f, 0.36f, 0.55f, 0.28f };
        case 3: return { 0.25f, 0.45f, 0.36f, 0.24f };
        default: return { 0.18f, 0.45f, 0.30f, 0.18f };
        }
    };

    m_startPoint = { m_runtimeMap.playerStartPoint.xz.x, m_runtimeMap.playerStartPoint.xz.z };
    m_startDepth = getLayerDepthById(m_runtimeMap.playerStartPoint.layerId);
    m_returnPoint = m_startPoint;
    m_returnDepth = m_startDepth;

    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2)
        {
            continue;
        }
        const float width = static_cast<float>(layer.gridWidth - 1) * layer.cellSize;
        const float height = static_cast<float>(layer.gridHeight - 1) * layer.cellSize;
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.x) + width * 0.5f);
        m_worldHalfSize = std::max(m_worldHalfSize, std::fabs(layer.center.z) + height * 0.5f);
        m_floorRegions.push_back({ { layer.center.x, layer.center.z }, { width * 0.5f, height * 0.5f },
            layer.layerDepth, getFloorColor(layer.groundTextureId), layer.id });
    }

    for (const NarakuMap::RopePoint& rope : m_runtimeMap.ropes)
    {
        const int top = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.topLayerId);
        const int bottom = NarakuMap::FindLayerIndexById(m_runtimeMap, rope.bottomLayerId);
        if (top >= 0 && bottom >= 0)
        {
            m_ropePoints.push_back({ { rope.topXZ.x, rope.topXZ.z }, { rope.bottomXZ.x, rope.bottomXZ.z },
                m_runtimeMap.terrainLayers[top].layerDepth, m_runtimeMap.terrainLayers[bottom].layerDepth });
        }
    }

    for (const NarakuMap::LayerGatePoint& gate : m_runtimeMap.layerGates)
    {
        const int layer = NarakuMap::FindLayerIndexById(m_runtimeMap, gate.layerId);
        if (layer < 0)
        {
            continue;
        }
        LayerGateState runtimeGate;
        runtimeGate.isEntry = gate.isEntry;
        runtimeGate.ropePos = { gate.ropeXZ.x, gate.ropeXZ.z };
        runtimeGate.loadPos = { gate.loadXZ.x, gate.loadXZ.z };
        runtimeGate.depth = m_runtimeMap.terrainLayers[layer].layerDepth;
        m_layerGates.push_back(runtimeGate);
    }

    int relicIndex = 0;
    for (const NarakuMap::MiningPoint& point : m_runtimeMap.miningPoints)
    {
        if (!point.enabled)
        {
            continue;
        }
        MiningPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.visualType = point.visualType;
        runtimePoint.discovered = point.discovered;
        runtimePoint.mined = false;
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.relicName = point.relicName.empty() ? kRelicNames[relicIndex % 8] : point.relicName;
        ++relicIndex;
        m_miningPoints.push_back(runtimePoint);
    }

    for (const NarakuMap::FishingPoint& point : m_runtimeMap.fishingPoints)
    {
        FishingPoint runtimePoint;
        runtimePoint.pos = { point.xz.x, point.xz.z };
        runtimePoint.depth = getLayerDepthById(point.layerId);
        runtimePoint.lake = point.lake;
        runtimePoint.id = point.id;
        m_fishingPoints.push_back(runtimePoint);
    }

    SpawnEnemiesForCurrentArea();

    if (placeAtStart)
    {
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        for (const LayerGateState& gate : m_layerGates)
        {
            if (gate.isEntry)
            {
                m_player.pos = gate.ropePos;
                m_player.depth = gate.depth;
                break;
            }
        }
        m_player.previousDepth = m_player.depth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
    }
    RebuildTerrainFloorBatch();
    RebuildEnemyBillboardBatch();
}

bool SceneNarakuProto::BuildSurfaceRuntime(bool spawnAtAbyssEntrance)
{
    NarakuMap::MapData surfaceMap;
    NarakuMap::TerrainLayer layer;
    layer.id = 1;
    layer.center = { 0.0f, 0.0f };
    layer.layerDepth = 0.0f;
    layer.gridWidth = 33;
    layer.gridHeight = 17;
    layer.cellSize = 2.0f;
    layer.groundTextureId = 0;
    layer.heights.assign(static_cast<std::size_t>(layer.gridWidth * layer.gridHeight), 0.0f);
    layer.vertexEnabled.assign(layer.heights.size(), 0);
    layer.cellAttributeFlags.assign(static_cast<std::size_t>((layer.gridWidth - 1) * (layer.gridHeight - 1)),
        NarakuMap::CellAttributeRemoved);

    const std::array<NarakuPiece::GridPoint, 5> offsets = {
        NarakuPiece::GridPoint{ 0, 0 }, NarakuPiece::GridPoint{ 8, 0 },
        NarakuPiece::GridPoint{ 16, 0 }, NarakuPiece::GridPoint{ 24, 0 },
        NarakuPiece::GridPoint{ 12, 8 } };
    const float originX = -32.0f;
    const float originZ = -16.0f;
    m_surfaceFacilities.clear();

    for (std::size_t pieceIndex = 0; pieceIndex < kSurfacePieceFiles.size(); ++pieceIndex)
    {
        NarakuPiece::PieceData piece;
        std::string error;
        const std::wstring path = NarakuPiece::MakeSurfaceCompletedPiecePath(kSurfacePieceFiles[pieceIndex]);
        if (!NarakuPiece::LoadPieceData(path, piece, &error) || !piece.isSurface)
        {
            m_generationFailureSummary = u8"地上マップを読み込めませんでした。";
            m_generationFailureDetail = error.empty() ? "surface piece is missing or not tagged as surface" : error;
            m_openGenerationFailurePopup = true;
            return false;
        }
        const int offsetX = offsets[pieceIndex].x;
        const int offsetZ = offsets[pieceIndex].z;
        for (int z = 0; z <= 8; ++z)
        {
            for (int x = 0; x <= 8; ++x)
            {
                const int globalX = offsetX + x;
                const int globalZ = offsetZ + z;
                const int sourceX = std::min(7, x);
                const int sourceZ = std::min(7, z);
                const std::size_t globalVertex = static_cast<std::size_t>(globalZ * layer.gridWidth + globalX);
                const std::size_t sourceVertex = static_cast<std::size_t>(sourceZ * piece.gridWidth + sourceX);
                layer.vertexEnabled[globalVertex] = 1;
                if (sourceVertex < piece.heights.size()) layer.heights[globalVertex] = piece.heights[sourceVertex];
            }
        }
        for (int z = 0; z < 8; ++z)
        {
            for (int x = 0; x < 8; ++x)
            {
                const int globalX = offsetX + x;
                const int globalZ = offsetZ + z;
                const int sourceX = std::min(6, x);
                const int sourceZ = std::min(6, z);
                const std::size_t globalCell = static_cast<std::size_t>(globalZ * (layer.gridWidth - 1) + globalX);
                const std::size_t sourceCell = static_cast<std::size_t>(sourceZ * std::max(1, piece.gridWidth - 1) + sourceX);
                std::uint32_t flags = NarakuMap::CellAttributeNone;
                if (sourceCell < piece.cells.size())
                {
                    const NarakuPiece::CellData& cell = piece.cells[sourceCell];
                    if (cell.deleted) flags |= NarakuMap::CellAttributeRemoved;
                    if (!cell.walkable) flags |= NarakuMap::CellAttributeBlocked;
                    if (cell.waterDepth == NarakuPiece::WaterDepth::Puddle) flags |= NarakuMap::CellAttributeWaterPuddle;
                    if (cell.waterDepth == NarakuPiece::WaterDepth::Pond) flags |= NarakuMap::CellAttributeWaterPond;
                    if (cell.waterDepth == NarakuPiece::WaterDepth::Lake) flags |= NarakuMap::CellAttributeWaterLake | NarakuMap::CellAttributeBlocked;
                }
                layer.cellAttributeFlags[globalCell] = flags;
            }
        }

        if (piece.surfaceFacility.type != NarakuPiece::SurfaceFacilityType::None)
        {
            SurfaceFacilityState facility;
            facility.type = piece.surfaceFacility.type;
            facility.center = {
                originX + static_cast<float>(offsetX + piece.surfaceFacility.cell.x + 1) * layer.cellSize,
                originZ + static_cast<float>(offsetZ + piece.surfaceFacility.cell.z + 1) * layer.cellSize };
            facility.interactionPoint = facility.center;
            const float frontOffset = 3.0f;
            if (piece.surfaceFacility.facing == NarakuPiece::Direction::North) facility.interactionPoint.y -= frontOffset;
            else if (piece.surfaceFacility.facing == NarakuPiece::Direction::South) facility.interactionPoint.y += frontOffset;
            else if (piece.surfaceFacility.facing == NarakuPiece::Direction::East) facility.interactionPoint.x += frontOffset;
            else facility.interactionPoint.x -= frontOffset;
            facility.modelPath = piece.surfaceFacility.modelPath;
            m_surfaceFacilities.push_back(facility);

            for (int z = 0; z < 2; ++z)
                for (int x = 0; x < 2; ++x)
                {
                    const int gx = offsetX + piece.surfaceFacility.cell.x + x;
                    const int gz = offsetZ + piece.surfaceFacility.cell.z + z;
                    if (gx >= 0 && gx < layer.gridWidth - 1 && gz >= 0 && gz < layer.gridHeight - 1)
                        layer.cellAttributeFlags[static_cast<std::size_t>(gz * (layer.gridWidth - 1) + gx)] |= NarakuMap::CellAttributeBlocked;
                }

            if (!piece.surfaceFacility.modelPath.empty())
            {
                NarakuMap::EnvironmentObject object;
                object.modelId = piece.surfaceFacility.type == NarakuPiece::SurfaceFacilityType::Home ? "surface_home" : "";
                object.xz = { facility.center.x + piece.surfaceFacility.offsetX, facility.center.y + piece.surfaceFacility.offsetZ };
                object.layerId = layer.id;
                object.scaleX = piece.surfaceFacility.scaleX;
                object.scaleY = piece.surfaceFacility.scaleY;
                object.scaleZ = piece.surfaceFacility.scaleZ;
                object.offsetY = piece.surfaceFacility.offsetY;
                object.rotationQuarterTurns = piece.surfaceFacility.rotationQuarterTurns;
                if (!object.modelId.empty()) surfaceMap.environmentObjects.push_back(object);
            }
        }
    }

    surfaceMap.terrainLayers.push_back(std::move(layer));
    const NarakuPiece::SurfaceFacilityType spawnType = spawnAtAbyssEntrance
        ? NarakuPiece::SurfaceFacilityType::AbyssEntrance : NarakuPiece::SurfaceFacilityType::Home;
    auto spawn = std::find_if(m_surfaceFacilities.begin(), m_surfaceFacilities.end(),
        [spawnType](const SurfaceFacilityState& value) { return value.type == spawnType; });
    if (spawn == m_surfaceFacilities.end()) return false;
    surfaceMap.playerStartPoint = { { spawn->interactionPoint.x, spawn->interactionPoint.y }, 1 };
    surfaceMap.returnPoint = surfaceMap.playerStartPoint;
    m_runtimeMap = std::move(surfaceMap);
    m_currentAreaIndex = -1;
    BuildCurrentAreaRuntime(true);
    m_enemies.clear();
    m_miningPoints.clear();
    m_fishingPoints.clear();
    m_groundRelics.clear();
    m_groundFoods.clear();
    m_pins = m_surfacePins;
    return true;
}

void SceneNarakuProto::EnterSurface(bool spawnAtAbyssEntrance)
{
    if (!BuildSurfaceRuntime(spawnAtAbyssEntrance)) return;
    m_mode = Mode::Surface;
}

void SceneNarakuProto::TryInteractSurface()
{
    const auto facility = std::min_element(m_surfaceFacilities.begin(), m_surfaceFacilities.end(),
        [this](const SurfaceFacilityState& lhs, const SurfaceFacilityState& rhs)
        { return Distance(lhs.interactionPoint, m_player.pos) < Distance(rhs.interactionPoint, m_player.pos); });
    if (facility == m_surfaceFacilities.end() || Distance(facility->interactionPoint, m_player.pos) > 2.5f) return;
    switch (facility->type)
    {
    case NarakuPiece::SurfaceFacilityType::Home: m_mode = Mode::Home; break;
    case NarakuPiece::SurfaceFacilityType::Shop: m_mode = Mode::GeneralShop; break;
    case NarakuPiece::SurfaceFacilityType::Armory: m_mode = Mode::Armory; break;
    case NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk: m_mode = Mode::Restaurant; break;
    case NarakuPiece::SurfaceFacilityType::AbyssEntrance: m_mode = Mode::AbyssEntrance; break;
    default: break;
    }
}

void SceneNarakuProto::UpdateSurface(float dt)
{
    const Vec2 previousPosition = m_player.pos;
    UpdateCameraControls();
    ClampDebugPlayerParams();
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.feetWorldY;
    UpdateMovement(dt);
    m_surfaceWasMoving = Distance(previousPosition, m_player.pos) > 0.0001f;
    if (m_player.stamina < GetMaxStamina())
        m_player.stamina = std::min(GetMaxStamina(), m_player.stamina +
            m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier() * dt);
    if (IsActionInteractTrigger()) TryInteractSurface();
}

void SceneNarakuProto::TryUseLayerGate(int gateIndex)
{
    if (gateIndex < 0 || gateIndex >= static_cast<int>(m_layerGates.size()))
    {
        return;
    }

    LayerGateState& gate = m_layerGates[gateIndex];
    if (gate.disabled)
    {
        ShowCenterNotification(u8"この層間口は使用できない！");
        return;
    }
    if (gate.destinationAreaIndex >= 0)
    {
        if (gate.destinationAreaIndex < static_cast<int>(m_areas.size()) && m_areas[gate.destinationAreaIndex].generated)
        {
#if !defined(NARAKU_EDITOR_BUILD)
            const int destinationDepth = m_areas[static_cast<std::size_t>(gate.destinationAreaIndex)].depth;
            if (destinationDepth > GetCurrentDepth() &&
                destinationDepth > GetRankMaximumDepth(m_adventurerRank) &&
                !m_uninsuredDescentAcceptedThisDive)
            {
                m_pendingUninsuredGateIndex = gateIndex;
                m_mode = Mode::UninsuredDescentConfirm;
                return;
            }
#endif
            BeginLayerTransition(gateIndex, gate.destinationAreaIndex);
            return;
        }
        SaveCurrentAreaState();
        m_loadingSourceGateIndex = gateIndex;
        m_loadingStep = 0;
        m_loadingProgress = 0.05f;
        const AreaState& destinationArea = m_areas[static_cast<std::size_t>(gate.destinationAreaIndex)];
        std::ostringstream loadingStatus;
        loadingStatus << u8"準備中: 第" << destinationArea.depth << u8"層 "
            << GetSublayerName(destinationArea.sublayer) << u8" エリア" << destinationArea.areaNumber;
        m_loadingStatus = loadingStatus.str();
        m_mode = Mode::Loading;
        return;
    }
    ShowCenterNotification(u8"接続先がありません。");
}

void SceneNarakuProto::BeginLayerTransition(int sourceGateIndex, int destinationAreaIndex)
{
    if (sourceGateIndex < 0 || sourceGateIndex >= static_cast<int>(m_layerGates.size()) ||
        destinationAreaIndex < 0 || destinationAreaIndex >= static_cast<int>(m_areas.size()))
    {
        return;
    }

    int destinationGateIndex = -1;
    const std::vector<LayerGateState>& destinationGates = m_areas[destinationAreaIndex].layerGates;
    for (int i = 0; i < static_cast<int>(destinationGates.size()); ++i)
    {
        if (destinationGates[i].connectionId == m_layerGates[sourceGateIndex].connectionId)
        {
            destinationGateIndex = i;
            break;
        }
    }
    if (destinationGateIndex < 0)
    {
        ShowCenterNotification(u8"層間口の接続情報が不正です。");
        return;
    }
    if (m_cookingTarget != CookingTarget::None)
    {
        CancelCooking(u8"層間移動を開始したため調理を中断しました。料理セットの使用回数は戻りません。");
    }

    m_transitionSourceGateIndex = sourceGateIndex;
    m_transitionDestinationAreaIndex = destinationAreaIndex;
    m_transitionDestinationGateIndex = destinationGateIndex;
    m_layerTransitionProgress = 0.0f;
    m_layerTransitionVisualOffset = 0.0f;
    m_layerTransitionAscending = m_layerGates[sourceGateIndex].isEntry;
    m_player.pos = m_layerGates[sourceGateIndex].ropePos;
    m_player.depth = m_layerGates[sourceGateIndex].depth;
    m_player.onRope = false;
    const bool routeAlreadyDiscovered = m_layerGates[sourceGateIndex].routeDiscovered ||
        m_areas[destinationAreaIndex].layerGates[destinationGateIndex].routeDiscovered;
    m_layerGates[sourceGateIndex].routeDiscovered = true;
    m_areas[destinationAreaIndex].layerGates[destinationGateIndex].routeDiscovered = true;
    if (!routeAlreadyDiscovered)
    {
        const int destinationDepth = m_areas[destinationAreaIndex].depth;
        AwardExp(static_cast<int>(std::round(100.0f * GetDepthExpMultiplier(destinationDepth))));
    }
    SaveCurrentAreaState();
    m_mode = Mode::LayerTransition;
}

void SceneNarakuProto::ReportGenerationFailure(const std::string& summary, const std::string& detail)
{
    m_generationFailureSummary = summary;
    m_generationFailureDetail = detail.empty() ? u8"生成器から詳細理由が返されませんでした。" : detail;
    m_openGenerationFailurePopup = true;

    _mkdir("Assets");
    _mkdir("Assets/Logs");
    std::ofstream log("Assets/Logs/naraku_generation_failure.log", std::ios::app);
    if (log)
    {
        log << "=== Generation failure ===\n" << m_generationFailureSummary << '\n'
            << m_generationFailureDetail << "\n\n";
    }
}

#if defined(NARAKU_EDITOR_BUILD)
void SceneNarakuProto::UpdateEditorPreviewGeneration()
{
    if (m_editorPreviewGenerationFailed) return;

    if (m_loadingStep == 0)
    {
        m_loadingStep = 1;
        m_loadingProgress = 0.01f;
        m_loadingStatus = u8"15段階の接続構成を生成しています...";
        return;
    }

    if (m_loadingStep == 1)
    {
        m_generationFailureSummary.clear();
        m_generationFailureDetail.clear();
        m_openGenerationFailurePopup = false;
        if (!ResetRun(false))
        {
            m_editorPreviewGenerationFailed = true;
            m_mode = Mode::Loading;
            m_loadingStatus = u8"生成に失敗しました。";
            return;
        }

        m_editorPreviewStartArea = m_currentAreaIndex;
        m_editorPreviewGenerationCursor = 0;
        m_editorPreviewGenerationCompleted = 1;
        m_editorPreviewGenerationTotal = static_cast<int>(m_areas.size());
        m_loadingProgress = m_editorPreviewGenerationTotal > 0
            ? static_cast<float>(m_editorPreviewGenerationCompleted) /
                static_cast<float>(m_editorPreviewGenerationTotal)
            : 0.0f;
        m_loadingStep = 2;
        m_mode = Mode::Loading;
        return;
    }

    while (m_editorPreviewGenerationCursor < static_cast<int>(m_areas.size()) &&
        m_areas[static_cast<std::size_t>(m_editorPreviewGenerationCursor)].generated)
    {
        ++m_editorPreviewGenerationCursor;
    }

    if (m_editorPreviewGenerationCursor >= static_cast<int>(m_areas.size()))
    {
        ActivateArea(m_editorPreviewStartArea, false);
        m_player.pos = m_startPoint;
        m_player.depth = m_startDepth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_loadingProgress = 1.0f;
        m_loadingStatus = u8"生成完了";
        m_editorPreviewGenerationActive = false;
        m_mode = Mode::Explore;
        return;
    }

    const int areaIndex = m_editorPreviewGenerationCursor;
    const AreaState& area = m_areas[static_cast<std::size_t>(areaIndex)];
    std::ostringstream status;
    status << u8"生成中: 第" << area.depth << u8"層 " << GetSublayerName(area.sublayer)
        << u8" エリア" << area.areaNumber << "  ("
        << m_editorPreviewGenerationCompleted << '/' << m_editorPreviewGenerationTotal << ')';
    m_loadingStatus = status.str();

    std::string error;
    if (!GeneratePlannedArea(areaIndex, error))
    {
        std::ostringstream summary;
        summary << u8"15段階生成プレビューに失敗しました: 第" << area.depth << u8"層 "
            << GetSublayerName(area.sublayer) << u8" エリア" << area.areaNumber
            << " / seed=" << m_editorPreviewGenerationSeed;
        ReportGenerationFailure(summary.str(), error);
        m_editorPreviewGenerationFailed = true;
        m_loadingStatus = u8"生成に失敗しました。";
        return;
    }

    ++m_editorPreviewGenerationCompleted;
    ++m_editorPreviewGenerationCursor;
    m_loadingProgress = m_editorPreviewGenerationTotal > 0
        ? static_cast<float>(m_editorPreviewGenerationCompleted) /
            static_cast<float>(m_editorPreviewGenerationTotal)
        : 1.0f;
}
#endif

void SceneNarakuProto::UpdateLoading()
{
#if defined(NARAKU_EDITOR_BUILD)
    if (m_editorPreviewGenerationActive)
    {
        UpdateEditorPreviewGeneration();
        return;
    }
#endif
    if (m_loadingStep == 0)
    {
        m_loadingStep = 1;
        m_loadingProgress = 0.15f;
        return;
    }
    if (m_loadingStep != 1 || m_currentAreaIndex < 0 ||
        m_loadingSourceGateIndex < 0 || m_loadingSourceGateIndex >= static_cast<int>(m_layerGates.size()))
    {
        m_mode = Mode::Explore;
        return;
    }

    const int parentAreaIndex = m_currentAreaIndex;
    const int sourceGateIndex = m_loadingSourceGateIndex;
    const int destinationAreaIndex = m_layerGates[sourceGateIndex].destinationAreaIndex;
    if (destinationAreaIndex < 0 || destinationAreaIndex >= static_cast<int>(m_areas.size()))
    {
        m_mode = Mode::Explore;
        ReportGenerationFailure(u8"接続先エリアを生成できませんでした。", u8"層間出入口に有効な接続先エリアが設定されていません。" );
        ShowCenterNotification(u8"接続先がありません。詳細を表示します。");
        return;
    }
    std::string error;
    m_loadingProgress = 0.45f;
    const AreaState& destinationArea = m_areas[static_cast<std::size_t>(destinationAreaIndex)];
    std::ostringstream loadingStatus;
    loadingStatus << u8"生成中: 第" << destinationArea.depth << u8"層 "
        << GetSublayerName(destinationArea.sublayer) << u8" エリア" << destinationArea.areaNumber;
    m_loadingStatus = loadingStatus.str();
    if (!GeneratePlannedArea(destinationAreaIndex, error))
    {
        ActivateArea(parentAreaIndex, false);
        ++m_layerGates[sourceGateIndex].generationFailures;
        SaveCurrentAreaState();
        m_mode = Mode::Explore;
        m_loadingProgress = 0.0f;
        m_loadingStep = 0;
        std::ostringstream summary;
        summary << u8"接続先の生成に失敗しました: 第" << destinationArea.depth << u8"層 "
            << GetSublayerName(destinationArea.sublayer) << u8" エリア" << destinationArea.areaNumber;
        ReportGenerationFailure(summary.str(), error);
        ShowCenterNotification(u8"接続先の生成に失敗しました。詳細を表示します。");
        return;
    }
    ActivateArea(parentAreaIndex, false);
    m_layerGates[sourceGateIndex].previewReady = true;
    SaveCurrentAreaState();
    m_loadingProgress = 1.0f;
    m_loadingStep = 0;
    m_mode = Mode::Explore;
    ShowCenterNotification(u8"ルート情報を取得しました。もう一度Fで進入します。");
}

void SceneNarakuProto::UpdateLayerTransition(float dt)
{
    m_layerTransitionProgress = std::min(1.0f, m_layerTransitionProgress + dt / kLayerTransitionDuration);
    const float direction = m_layerTransitionAscending ? 1.0f : -1.0f;
    m_layerTransitionVisualOffset = direction * kLayerTransitionHeight * m_layerTransitionProgress;

    if (m_layerTransitionAscending)
    {
        m_player.upperLoad += kLayerTransitionHeight * dt / kLayerTransitionDuration;
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            TriggerUpperLoad();
            m_player.upperLoad = 0.0f;
            if (m_mode != Mode::LayerTransition) return;
        }
    }

    const float fadeStartProgress = 1.0f - kScreenFadeDuration / kLayerTransitionDuration;
    if (m_screenFadePhase == ScreenFadePhase::None && m_layerTransitionProgress >= fadeStartProgress)
    {
        BeginScreenFade(ScreenFadeAction::LayerTransition);
    }
    if (m_screenFadePhase != ScreenFadePhase::None) UpdateScreenFade(dt);
}

void SceneNarakuProto::BeginScreenFade(ScreenFadeAction action)
{
    if (action == ScreenFadeAction::None || m_screenFadePhase != ScreenFadePhase::None) return;
    m_screenFadeAction = action;
    m_screenFadePhase = ScreenFadePhase::FadeOut;
    m_screenFadeAlpha = 0.0f;
}

void SceneNarakuProto::UpdateScreenFade(float dt)
{
    if (m_screenFadePhase == ScreenFadePhase::None) return;
    const float delta = dt / kScreenFadeDuration;
    if (m_screenFadePhase == ScreenFadePhase::FadeOut)
    {
        m_screenFadeAlpha = std::min(1.0f, m_screenFadeAlpha + delta);
        if (m_screenFadeAlpha < 1.0f) return;

        const ScreenFadeAction action = m_screenFadeAction;
        if (action == ScreenFadeAction::StartDive) CompleteStartDive();
        else if (action == ScreenFadeAction::LayerTransition) CompleteLayerTransition();
        m_screenFadeAction = ScreenFadeAction::None;
        m_screenFadePhase = ScreenFadePhase::FadeIn;
        return;
    }

    m_screenFadeAlpha = std::max(0.0f, m_screenFadeAlpha - delta);
    if (m_screenFadeAlpha <= 0.0f)
    {
        m_screenFadeAlpha = 0.0f;
        m_screenFadePhase = ScreenFadePhase::None;
    }
}

void SceneNarakuProto::CompleteLayerTransition()
{

    SaveCurrentAreaState();
    const int destinationArea = m_transitionDestinationAreaIndex;
    const int destinationGate = m_transitionDestinationGateIndex;
    ActivateArea(destinationArea, false);
    AreaState& arrivedArea = m_areas[destinationArea];
    if (!arrivedArea.firstAreaExpAwarded)
    {
        arrivedArea.firstAreaExpAwarded = true;
        arrivedArea.firstAreaRewardAwarded = true;
        ++m_result.firstAreaCount;
        AwardExp(static_cast<int>(std::round(100.0f * GetDepthExpMultiplier(arrivedArea.depth))));
    }
    if (destinationGate >= 0 && destinationGate < static_cast<int>(m_layerGates.size()))
    {
        m_player.pos = m_layerGates[destinationGate].ropePos;
        m_player.depth = m_layerGates[destinationGate].depth;
        m_player.previousDepth = m_player.depth;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
    }
    m_layerTransitionVisualOffset = 0.0f;
    m_transitionSourceGateIndex = -1;
    m_transitionDestinationAreaIndex = -1;
    m_transitionDestinationGateIndex = -1;
    m_mode = Mode::Explore;
    UpdateImportantQuestArrival();
    SaveProgress();
}

void SceneNarakuProto::Update()
{
    if (m_screenFadePhase != ScreenFadePhase::None)
    {
        if (m_screenFadeAction == ScreenFadeAction::LayerTransition && m_mode == Mode::LayerTransition)
            UpdateLayerTransition(kDt);
        else
            UpdateScreenFade(kDt);
        return;
    }

    m_centerNotificationTimer = std::max(0.0f, m_centerNotificationTimer - kDt);
    m_characterAnimationTime += kDt;
    for (JumpEffect& effect : m_jumpEffects)
    {
        effect.remainingTime -= kDt;
    }
    m_jumpEffects.erase(
        std::remove_if(
            m_jumpEffects.begin(),
            m_jumpEffects.end(),
            [](const JumpEffect& effect) { return effect.remainingTime <= 0.0f; }),
        m_jumpEffects.end());

    if (m_mode == Mode::Loading)
    {
        UpdateLoading();
        return;
    }
    if (m_mode == Mode::LayerTransition)
    {
        UpdateLayerTransition(kDt);
        return;
    }

    UpdateWorldClockAndQuests(kDt);

#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
    // Cキーで当たり判定デバッグ表示を切り替えます。
    if (IsKeyTrigger('C'))
    {
        m_showCollisionDebug = !m_showCollisionDebug;
        AddMessage(m_showCollisionDebug ? "Collision Debug: ON" : "Collision Debug: OFF");
    }
#endif

    // 統合メニュー（地図・所持品・設定）の開閉と切替
    if (m_mode == Mode::Explore || m_mode == Mode::Surface)
    {
        if (IsActionMenuMapTrigger())
        {
            m_overlayReturnMode = m_mode;
            m_activeMenuTab = MenuTab::Map;
            m_inventoryMapShowingMap = true;
            m_mode = Mode::Inventory;
        }
        else if (IsActionMenuInventoryTrigger())
        {
            m_overlayReturnMode = m_mode;
            m_activeMenuTab = MenuTab::Inventory;
            m_inventoryMapShowingMap = false;
            m_mode = Mode::Inventory;
        }
        else if (IsActionMenuSettingsTrigger())
        {
            m_overlayReturnMode = m_mode;
            m_activeMenuTab = MenuTab::Settings;
            m_inputSettings.OnOpen();
            m_mode = Mode::Inventory;
        }
    }
    else if (m_mode == Mode::Inventory)
    {
        if (m_inputSettings.IsRebinding())
        {
            return;
        }

        // LB / RB (Q / E) によるタブ切替
        if (IsActionUITabPrevTrigger() && !IsActionMenuMapTrigger() && !IsActionMenuSettingsTrigger())
        {
            if (m_activeMenuTab == MenuTab::Map) m_activeMenuTab = MenuTab::Settings;
            else if (m_activeMenuTab == MenuTab::Settings) m_activeMenuTab = MenuTab::Inventory;
            else m_activeMenuTab = MenuTab::Map;
            m_inventoryMapShowingMap = (m_activeMenuTab == MenuTab::Map);
            if (m_activeMenuTab == MenuTab::Settings) m_inputSettings.OnOpen();
        }
        else if (IsActionUITabNextTrigger() && !IsActionMenuMapTrigger() && !IsActionMenuSettingsTrigger())
        {
            if (m_activeMenuTab == MenuTab::Map) m_activeMenuTab = MenuTab::Inventory;
            else if (m_activeMenuTab == MenuTab::Inventory) m_activeMenuTab = MenuTab::Settings;
            else m_activeMenuTab = MenuTab::Map;
            m_inventoryMapShowingMap = (m_activeMenuTab == MenuTab::Map);
            if (m_activeMenuTab == MenuTab::Settings) m_inputSettings.OnOpen();
        }
        // 直接開閉ショートカット
        else if (IsActionMenuMapTrigger())
        {
            if (m_activeMenuTab == MenuTab::Map) { m_mode = m_overlayReturnMode; SetInputGuardActive(true); }
            else { m_activeMenuTab = MenuTab::Map; m_inventoryMapShowingMap = true; }
        }
        else if (IsActionMenuInventoryTrigger())
        {
            if (m_activeMenuTab == MenuTab::Inventory) { m_mode = m_overlayReturnMode; SetInputGuardActive(true); }
            else { m_activeMenuTab = MenuTab::Inventory; m_inventoryMapShowingMap = false; }
        }
        else if (IsActionMenuSettingsTrigger())
        {
            if (IsActionUIBackTrigger() || m_activeMenuTab == MenuTab::Settings) { m_mode = m_overlayReturnMode; SetInputGuardActive(true); }
            else { m_activeMenuTab = MenuTab::Settings; m_inputSettings.OnOpen(); }
        }
        // Bボタン / Esc で閉じる
        else if (IsActionUIBackTrigger())
        {
            m_mode = m_overlayReturnMode;
            SetInputGuardActive(true);
        }
    }

    if ((m_mode == Mode::Explore || m_mode == Mode::Surface) && IsActionToggleLightTrigger())
    {
        TogglePortableLight();
    }
    if (m_mode == Mode::Explore || m_mode == Mode::Surface)
    {
        UpdatePortableLight(kDt);
    }

    // 探索モード中だけプレイヤーや敵などのゲーム更新を進めます。
    if (m_mode == Mode::Explore)
    {
        UpdateExplore(kDt);
    }
    else if (m_mode == Mode::Surface) UpdateSurface(kDt);
}

void SceneNarakuProto::UpdateExplore(float dt)
{
#if defined(NARAKU_EDITOR_BUILD)
    UpdateCameraControls();
    if (m_editorOverviewMode)
    {
        m_player.hp = GetMaxHp();
        m_player.stamina = GetMaxStamina();
        m_player.mental = GetMaxMental();
        m_fullness = kFullnessMaximum;
        m_hydration = kHydrationMaximum;
        return;
    }
    ClampDebugPlayerParams();
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.feetWorldY;
    UpdateMovement(dt);
    DiscoverNearbyMiningPoints();
    UpdateExplorationDiscovery();
    m_player.hp = GetMaxHp();
    m_player.stamina = GetMaxStamina();
    m_player.mental = GetMaxMental();
    m_fullness = kFullnessMaximum;
    m_hydration = kHydrationMaximum;
    if (IsActionInteractTrigger()) TryInteract();
    return;
#endif
    // 探索モード中だけ右ドラッグによるカメラ回転を受け付けます。
    UpdateCameraControls();

    // ImGui からの変更値が不正でもゲーム進行が壊れないよう毎フレーム丸めます。
    ClampDebugPlayerParams();

    // 今フレームの上昇量を後で計算できるよう、更新前の深度を保存します。
    m_player.previousDepth = m_player.depth;
    m_player.previousWorldY = m_player.onRope && m_activeRope >= 0
        ? GetRopeWorldY(m_activeRope, m_ropeProgress)
        : m_player.feetWorldY;

    // 入力に応じてプレイヤーの移動と行動制限を更新します。
    UpdateMovement(dt);
    UpdateMentalAbilities(dt);

    // 攻撃タイマーとスタミナ回復を更新します。
    UpdateAction(dt);

    // 採掘中なら採掘タイマーを進めます。
    UpdateMining(dt);
    UpdateHunger(dt);
    UpdateHydration(dt);
    UpdateCooking(dt);
    UpdateFishing(dt);
    if (m_mode != Mode::Explore) return;

    // 敵の追跡と体当たりを更新します。
    UpdateEnemies(dt);

    const EquipmentBonus currentEquipmentBonus = GetEquipmentBonus();
    const float hpRecoveryPerSecond = currentEquipmentBonus.hpRecoveryPerSecond +
        GetMaxHp() * currentEquipmentBonus.hpRecoveryMaxRatioPerSecond;
    if (hpRecoveryPerSecond > 0.0f && m_player.hp > 0.0f)
    {
        m_player.hp = std::min(GetMaxHp(), m_player.hp + hpRecoveryPerSecond * dt);
    }

    // 近くの未発見採掘ポイントを発見済みにします。
    DiscoverNearbyMiningPoints();
    UpdateExplorationDiscovery();

    // 深度変化から上昇負荷を更新します。
    UpdateUpperLoad(dt);
    if (m_mode != Mode::Explore) return;
    UpdateUpperLoadEffects(dt);
    if (m_mode != Mode::Explore) return;

    // リザルト用に今回の最大深度を記録します。
    m_result.maxDepth = std::max(m_result.maxDepth, GetCurrentDepth());
    const int currentDepth = GetCurrentDepth();
    if (currentDepth > m_maxReachedDepth)
    {
        m_maxReachedDepth = currentDepth;
        RefreshPromotionQuestAvailability();
        SaveProgress();
    }
    m_result.staySecondsByDepth[static_cast<std::size_t>(currentDepth - 1)] += dt;

    // Fキーで帰還、ロープ、拾う、採掘のいずれかを試します。
    if (IsActionInteractTrigger() && m_fishingPhase == FishingPhase::None) TryInteract();

    // 体力が0以下になったら死亡リザルトへ移行します。
    if (m_player.hp <= 0.0f && m_mode == Mode::Explore) StartDeath(u8"体力が0になりました。", DeathCause::Other);
}

SceneNarakuProto::PresentationScene SceneNarakuProto::GetPresentationScene() const
{
    if (m_mode == Mode::SaveError)
    {
        return m_saveErrorPresentation;
    }
    if (m_mode == Mode::Surface || m_mode == Mode::Home || m_mode == Mode::GeneralShop ||
        m_mode == Mode::Armory || m_mode == Mode::Restaurant || m_mode == Mode::QuestDesk ||
        m_mode == Mode::AbyssEntrance)
    {
        return PresentationScene::Town;
    }
    if (m_mode == Mode::ReturnResult || m_mode == Mode::DeathResult)
    {
        return PresentationScene::Result;
    }
    return PresentationScene::Dive;
}

void SceneNarakuProto::UpdateMovement(float dt)
{
    const Vec2 frameStartPos = m_player.pos;
    bool suppressMovementDistance = false;
    m_lastFrameRunning = false;
    m_lastFrameRopeMoving = false;

    // 通常の床に立っている位置だけを、歩行不可セルからの復帰先として保存します。
    if (m_player.grounded && !m_player.onRope && HasFloorAt(m_player.pos, m_player.depth))
    {
        m_player.lastSafeGroundPos = m_player.pos;
        m_player.lastSafeGroundDepth = m_player.depth;
        m_player.hasSafeGroundPos = true;
        m_player.blockedCellAirTime = 0.0f;
        m_player.blockedCellVelocity = {};
    }

    const bool startedOverBlockedCell =
        (GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeBlocked) != 0u;
    // WASD入力を集めるための移動ベクトルです。
    Vec2 input;

    // 採掘中は移動やジャンプ、ステップ入力を受け付けません。
    const bool isMining = m_miningIndex >= 0;

    // 着地直後の硬直を更新します。
    m_player.landingRecoveryTimer = std::max(0.0f, m_player.landingRecoveryTimer - dt);
    const bool inLandingRecovery = m_player.landingRecoveryTimer > 0.0f;

    // Wキーでカメラから見た前方向へ進みます。
    if (!isMining) { GetActionMoveVector(input.x, input.y); }

    // Sキーでカメラから見た後ろ方向へ進みます。


    // Aキーでカメラから見た左方向へ進みます。


    // Dキーでカメラから見た右方向へ進みます。


    // カメラから見た前方向を、斜め投影で画面上方向に見えるワールド方向へ対応させます。
    const Vec2 cameraForward = GetCameraForward();

    // カメラから見た右方向を、現在の3Dカメラで画面右方向に見えるワールド方向へ対応させます。
    const Vec2 cameraRight = GetCameraRight();

    // 入力をカメラ基準方向からワールド移動方向へ変換します。
    Vec2 move = Add(Mul(cameraRight, input.x), Mul(cameraForward, input.y));

    // 斜め移動が速くならないように正規化します。
    move = Normalize(move);

    // 入力がある時だけ向きを更新して、停止中の攻撃方向を維持します。
    if (move.x != 0.0f || move.y != 0.0f) m_player.facing = move;

    // Spaceが押された瞬間にジャンプ開始を試します。
    if (!isMining && !inLandingRecovery && IsActionJumpTrigger()) TryStartJump();

    // 左右Shiftの現在入力を物理キーとして取得します。
    const bool shiftPressed = !isMining && IsShiftPress();

    // 前フレームは押されておらず、今フレーム押されたらShift押下開始です。
    const bool shiftStarted = shiftPressed && !m_shiftWasPressed;

    // 前フレームは押されていて、今フレーム押されていなければShiftを離した瞬間です。
    const bool shiftReleased = !shiftPressed && m_shiftWasPressed;

    // Shiftが押された瞬間に「短押しならステップ」判定を保留します。
    if (shiftStarted)
    {
        // すぐにはステップせず、短押しか長押しかの判定を待ちます。
        m_shiftPendingStep = true;

        // 押下時間を0から測り直します。
        m_shiftHold = 0.0f;

        // まだ走りとして確定していない状態に戻します。
        m_shiftRunCommitted = false;
    }

    // このフレームで走り入力として扱うかどうかです。
    bool wantsRun = false;

    // Shift短押し/長押しの判定中なら押下時間を見ます。
    if (m_shiftPendingStep)
    {
        // Shiftを押し続けている時間を加算します。
        m_shiftHold += dt;

        // 閾値前に離されたら短押しステップとして扱います。
        if (shiftReleased)
        {
            // 長押し走りに確定していない短押しだけステップを開始します。
            if (!m_shiftRunCommitted && !inLandingRecovery)
            {
                // スタミナや重量条件を満たす場合だけステップを開始します。
                TryStartStep();
            }

            // 判定が終わったので保留を解除します。
            m_shiftPendingStep = false;

            // 走り確定状態も解除します。
            m_shiftRunCommitted = false;
        }

        // 閾値以上押されていたら長押し走りとして扱います。
        else if (m_shiftHold >= kShiftRunThreshold)
        {
            // このフレームの移動処理で走り速度を使わせます。
            wantsRun = true;

            // 閾値を超えたので、このShift入力はステップではなく走りとして確定します。
            m_shiftRunCommitted = true;
        }
    }

    // すでに長押し扱いになった後もShiftを押している間は走り扱いにします。
    else if (shiftPressed)
    {
        // このフレームの移動処理で走り速度を使わせます。
        wantsRun = true;
    }

    // Shiftが押されていないなら短押し判定状態を完全に解除します。
    if (!shiftPressed)
    {
        // ステップ保留を解除します。
        m_shiftPendingStep = false;

        // Shift押下時間も0に戻します。
        m_shiftHold = 0.0f;

        // 走り確定状態を解除します。
        m_shiftRunCommitted = false;
    }

    // 地面歩行開始時点の位置と高さを覚えて、段差踏み外し時の落下開始判定に使います。
    const Vec2 groundedMoveStartPos = m_player.pos;
    const float groundedMoveStartGroundY = GetGroundWorldY(m_player.pos, m_player.depth);

    // ノックバック中は敵から押し出される移動を先に適用します。
    if (m_player.knockbackTimer > 0.0f)
    {
        // ノックバック速度ぶんプレイヤー座標をずらします。
        const Vec2 knockbackTarget = Add(m_player.pos, Mul(m_player.knockbackVelocity, dt));
        m_player.pos = m_player.grounded
            ? ResolveFloorMove(m_player.pos, knockbackTarget, m_player.depth)
            : ResolveAirMove(m_player.pos, knockbackTarget, m_player.depth);

        // ノックバック残り時間を減らします。
        m_player.knockbackTimer = std::max(0.0f, m_player.knockbackTimer - dt);
    }

    // ステップ中は通常移動よりステップ移動を優先します。
    if (m_player.stepTimer > 0.0f)
    {
        // このフレーム開始時点の残り時間を保存します。
        float previous = m_player.stepTimer;

        // ステップ残り時間を減らします。
        m_player.stepTimer = std::max(0.0f, m_player.stepTimer - dt);

        // 無敵時間中だけ高速移動し、後硬直中は移動しません。
        if (previous > kStepRecoveryTime)
        {
            // 0.5秒で指定距離を進むようにステップ速度を計算します。
            const Vec2 stepTarget = Add(m_player.pos, Mul(m_player.facing, (kStepDistance / kStepInvincibleTime) * dt));
            m_player.pos = m_player.grounded
                ? ResolveFloorMove(m_player.pos, stepTarget, m_player.depth)
                : ResolveAirMove(m_player.pos, stepTarget, m_player.depth);
        }
    }

    // ロープに掴まっていない時だけ平面移動を行います。
    else if (!m_player.onRope && !inLandingRecovery)
    {
        // 通常歩行速度に重量70%以上の低下を反映します。
        float speed = GetMoveSpeed();

        // 100%以上の重量では走れないため、ここで走行可否を判定します。
        const bool tooHeavyToRun = GetCurrentWeight() >= GetMaxWeight();
        const std::uint32_t currentCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
        const bool inPond = (currentCellFlags & NarakuMap::CellAttributeWaterPond) != 0u;
        bool canRun = !tooHeavyToRun && !inPond && m_player.stamina > 0.0f;

        if (wantsRun && tooHeavyToRun && (move.x != 0.0f || move.y != 0.0f))
        {
            if (!m_heavyRunNotificationShown)
            {
                ShowCenterNotification(u8"重すぎて走れない！");
                m_heavyRunNotificationShown = true;
            }
        }
        else
        {
            m_heavyRunNotificationShown = false;
        }

        // 走り入力、移動入力、重量、スタミナをすべて満たした時だけ走ります。
        if (wantsRun && canRun && (move.x != 0.0f || move.y != 0.0f))
        {
            // 走り速度へ切り替えます。
            speed = GetRunSpeed();
            m_lastFrameRunning = true;

            // 1フレームぶんの走りスタミナを消費します。
            SpendStamina(m_debugPlayerParams.runCostPerSecond * dt);
        }
        else if (inPond && (move.x != 0.0f || move.y != 0.0f))
        {
            SpendStamina(m_debugPlayerParams.runCostPerSecond * dt);
        }

        // 決まった速度で平面座標を進めます。
        if (m_player.grounded || !startedOverBlockedCell)
        {
            const Vec2 moveTarget = Add(m_player.pos, Mul(move, speed * dt));
            m_player.pos = m_player.grounded
                ? ResolveFloorMove(m_player.pos, moveTarget, m_player.depth)
                : ResolveAirMove(m_player.pos, moveTarget, m_player.depth);
        }
    }

    // ロープに掴まっている時はW/Sを深度操作として扱います。
    if (m_player.onRope)
    {
        // ロープ番号が無効なら、操作不能にならないよう即座にロープ状態を解除します。
        if (m_activeRope < 0 || m_activeRope >= static_cast<int>(m_ropePoints.size()))
        {
            m_player.onRope = false;
            m_activeRope = -1;
            m_lastFrameMovementDistance = Distance(frameStartPos, m_player.pos);
            return;
        }

        // 現在つかまっているロープの上端/下端深度を参照します。
        const RopePoint& rope = m_ropePoints[m_activeRope];

        // 深度入力を一時的に保持します。
        float depthInput = 0.0f;

        // 昇降入力（W/Sキー、およびパッドの左スティック・十字キー）
        if (!isMining && input.y > 0.1f) depthInput -= 1.0f;
        if (!isMining && input.y < -0.1f) depthInput += 1.0f;

        // A/Dが押された瞬間はスタミナがなくてもロープから横へ離脱できるようにします。
        bool leaveLeft = false, leaveRight = false;
        GetActionMoveTriggers(leaveLeft, leaveRight);
        const bool wantsLeaveRope = !isMining && (leaveLeft || leaveRight);

        // ロープから離れる時は、掴まり状態を解除して少しだけ横へずらします。
        if (wantsLeaveRope)
        {
            // Aなら左、Dなら右へ離れる方向を決めます。
            const float leaveSign = leaveLeft ? -1.0f : 1.0f;

            // 現在深度に横へ降りられる床がある時だけロープを離します。
            if (!TryLeaveRopeSide(m_activeRope, leaveSign, cameraRight))
            {
                AddMessage(u8"足場がないためロープを離せません。");
            }
        }

        // 深度入力があり、スタミナを払える時だけ昇降します。
        if (m_player.onRope && depthInput != 0.0f && CanSpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt))
        {
            m_lastFrameRopeMoving = true;
            // ロープ昇降のスタミナを消費します。
            SpendStamina(m_debugPlayerParams.ropeCostPerSecond * dt);

            const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);
            const float horizontalLength = Distance(traversal.topPosition, traversal.bottomPosition);
            const float verticalLength = traversal.bottomWorldY - traversal.topWorldY;
            const float ropeLength = std::max(0.001f, std::sqrt(horizontalLength * horizontalLength + verticalLength * verticalLength));
            const float newProgress = m_ropeProgress + depthInput * GetRopeSpeed(depthInput < 0.0f) * dt / ropeLength;

            if (newProgress <= 0.0f && depthInput < 0.0f)
            {
                m_ropeProgress = 0.0f;
                m_player.depth = rope.topDepth;
                m_player.pos = rope.topPos;
                m_player.onRope = false;
                m_activeRope = -1;
                m_player.grounded = true;
                m_player.verticalSpeed = 0.0f;
                m_player.airTime = 0.0f;
                m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
                m_player.peakFeetWorldY = m_player.feetWorldY;
                m_player.landingRecoveryTimer = 0.0f;
                AddMessage(u8"ロープの上端に到達し、足場へ降りました。");
            }
            else
            {
                const float targetProgress = std::max(0.0f, std::min(1.0f, newProgress));
                const Vec2 targetPosition = GetRopePosition(m_activeRope, targetProgress);
                const float targetFeetWorldY = GetRopePlayerFeetWorldY(m_activeRope, targetProgress);
                const int bottomLayerIndex = FindLayerIndexAt(targetPosition, rope.bottomDepth);
                const bool touchesBottomGround = depthInput > 0.0f && bottomLayerIndex >= 0 &&
                    targetFeetWorldY <= GetGroundWorldY(targetPosition, rope.bottomDepth) + kSlopeHeightTolerance;

                if (touchesBottomGround && CanStandAt(targetPosition, rope.bottomDepth))
                {
                    m_ropeProgress = targetProgress;
                    m_player.depth = rope.bottomDepth;
                    m_player.pos = targetPosition;
                    m_player.onRope = false;
                    m_activeRope = -1;
                    m_player.grounded = true;
                    m_player.verticalSpeed = 0.0f;
                    m_player.airTime = 0.0f;
                    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
                    m_player.peakFeetWorldY = m_player.feetWorldY;
                    m_player.landingRecoveryTimer = 0.0f;
                    AddMessage(u8"足が地面に着いたため、ロープから降りました。");
                }
                else if (!touchesBottomGround)
                {
                    m_ropeProgress = targetProgress;
                    m_player.pos = targetPosition;
                    m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
                    m_player.feetWorldY = targetFeetWorldY;
                }
            }
        }
    }

    // 歩行不可セルへ飛び込んだ時の横移動速度を保存します。
    const Vec2 airborneMoveDelta = Sub(m_player.pos, groundedMoveStartPos);
    if (!m_player.grounded && !startedOverBlockedCell && dt > 0.0f &&
        Distance(groundedMoveStartPos, m_player.pos) > 0.001f)
    {
        m_player.blockedCellVelocity = Mul(airborneMoveDelta, 1.0f / dt);
    }

    std::uint32_t airborneCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
    if (!m_player.grounded && !m_player.onRope &&
        (airborneCellFlags & NarakuMap::CellAttributeBlocked) != 0u)
    {
        m_player.blockedCellAirTime += dt;

        const bool overLake = (airborneCellFlags & NarakuMap::CellAttributeWaterLake) != 0u;
        if (!overLake)
        {
            const Vec2 downhill = GetTerrainDownhillDirection(m_player.pos, m_player.depth);
            if (Distance({}, downhill) > 0.001f)
            {
                m_player.blockedCellVelocity = Mul(downhill, GetMoveSpeed());
            }
            else if (Distance({}, m_player.blockedCellVelocity) <= 0.001f)
            {
                m_player.blockedCellVelocity = Mul(m_player.facing, GetMoveSpeed());
            }

            const Vec2 slideStart = m_player.pos;
            const Vec2 slideTarget = Add(slideStart, Mul(m_player.blockedCellVelocity, dt));
            m_player.pos = ResolveAirMove(slideStart, slideTarget, m_player.depth);
            if (Distance(slideStart, m_player.pos) <= 0.001f)
            {
                m_player.blockedCellVelocity = Mul(m_player.blockedCellVelocity, -1.0f);
                m_player.pos = ResolveAirMove(
                    slideStart, Add(slideStart, Mul(m_player.blockedCellVelocity, dt)), m_player.depth);
            }
        }

        airborneCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
        const bool stillBlocked = (airborneCellFlags & NarakuMap::CellAttributeBlocked) != 0u;
        const bool stillOverLake = (airborneCellFlags & NarakuMap::CellAttributeWaterLake) != 0u;
        const float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        const bool submergedInLake = stillOverLake && m_player.feetWorldY <= groundWorldY - kLakeFallDepth;
        const bool timedOut = stillBlocked && m_player.blockedCellAirTime >= kBlockedCellReturnTime;

        if (submergedInLake || timedOut)
        {
            if (RestorePlayerToSafeGround())
            {
                suppressMovementDistance = true;
                if (submergedInLake)
                {
                    const float damage = GetMaxHp() * kLakeFallDamageRatio;
                    const bool preventedByRelic =
                        m_player.hp - damage <= 0.0f && TryConsumeSurvivalRelic(true, false);
                    if (!preventedByRelic)
                    {
                        m_player.hp = std::max(0.0f, m_player.hp - damage);
                        if (m_player.hp <= 0.0f)
                        {
                            StartDeath(u8"湖への落下で死亡しました。", DeathCause::Other);
                        }
                    }
                    AddMessage(preventedByRelic
                        ? u8"湖へ落下しましたが、生存的遺物が致命傷を防ぎました。"
                        : u8"湖へ落下し、安全な場所へ戻されました。最大HPの15%のダメージを受けました。");
                }
                else
                {
                    AddMessage(u8"歩行可能な場所へ戻れなかったため、安全な場所へ復帰しました。");
                }
            }
        }
    }
    else if (!m_player.grounded)
    {
        m_player.blockedCellAirTime = 0.0f;
    }

    // 地面を歩いてより低い地形へ出た時は、一定以上の落差で落下状態へ移ります。
    if (m_player.grounded && !m_player.onRope && !suppressMovementDistance)
    {
        const float currentGroundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        const float walkedDropHeight = groundedMoveStartGroundY - currentGroundWorldY;
        const bool movedHorizontally = Distance(groundedMoveStartPos, m_player.pos) > 0.01f;

        if (movedHorizontally && walkedDropHeight >= m_autoFallStartHeight)
        {
            m_player.grounded = false;
            m_player.airTime = 0.0f;
            m_player.verticalSpeed = 0.0f;
            m_player.feetWorldY = groundedMoveStartGroundY;
            m_player.peakFeetWorldY = groundedMoveStartGroundY;
            m_player.landingRecoveryTimer = 0.0f;
        }
    }

    // ジャンプ中なら実地形の高さを使って空中時間と着地を更新します。
    if (!m_player.grounded && !m_player.onRope)
    {
        const float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);

        // 空中にいる時間を加算します。
        m_player.airTime += dt;

        // 足元の絶対ワールド高さを縦速度ぶん進めます。
        m_player.feetWorldY += m_player.verticalSpeed * dt;

        // 重力で縦速度を減らします。
        m_player.verticalSpeed -= 9.8f * dt;

        // 最高到達点を更新して、着地時の落下距離計算に使います。
        m_player.peakFeetWorldY = std::max(m_player.peakFeetWorldY, m_player.feetWorldY);

        const std::uint32_t landingCellFlags = GetCellAttributeFlagsAt(m_player.pos, m_player.depth);
        const bool canLand = (landingCellFlags &
            (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved)) == 0u;

        // 降下中に歩行可能な地面へ届いたら着地します。
        if (canLand && m_player.verticalSpeed <= 0.0f && m_player.feetWorldY <= groundWorldY)
        {
            const float fallDistance = std::max(0.0f, m_player.peakFeetWorldY - groundWorldY);
            float landingRecovery = 0.0f;
            if (fallDistance >= 6.0f) landingRecovery = kLandingRecoveryHeavy;
            else if (fallDistance >= 4.0f) landingRecovery = kLandingRecoveryMedium;
            else if (fallDistance >= 2.0f) landingRecovery = kLandingRecoveryLight;

            constexpr float safeFallDistance = 2.0f;
            constexpr float lethalFallDistance = 20.0f;
            if (fallDistance >= lethalFallDistance)
            {
                StartDeath(u8"落下死しました。", DeathCause::Fall);
            }
            else if (fallDistance > safeFallDistance)
            {
                const float damageRatio = (fallDistance - safeFallDistance) /
                    (lethalFallDistance - safeFallDistance);
                const float fallDamage = GetMaxHp() * damageRatio;
                m_player.hp = std::max(0.0f, m_player.hp - fallDamage);
                if (m_player.hp <= 0.0f)
                {
                    StartDeath(u8"落下ダメージで死亡しました。", DeathCause::Fall);
                }
            }

            m_player.grounded = true;
            m_player.airTime = 0.0f;
            m_player.verticalSpeed = 0.0f;
            m_player.feetWorldY = groundWorldY;
            m_player.peakFeetWorldY = groundWorldY;
            m_player.landingRecoveryTimer = landingRecovery;
        }
    }
    else if (!m_player.onRope)
    {
        const float groundWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.feetWorldY = groundWorldY;
        m_player.peakFeetWorldY = groundWorldY;
    }

    // デバッグフィールド外へ出ないようX座標を制限します。
    m_player.pos.x = std::max(-m_worldHalfSize, std::min(m_player.pos.x, m_worldHalfSize));

    // デバッグフィールド外へ出ないようY座標を制限します。
    m_player.pos.y = std::max(-m_worldHalfSize, std::min(m_player.pos.y, m_worldHalfSize));

    // 危険地形に乗っている間は一定間隔でダメージを受けます。
    if (!m_player.onRope && (GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeHazard) != 0u)
    {
        m_hazardTickTimer -= dt;
        if (m_hazardTickTimer <= 0.0f)
        {
            m_hazardTickTimer += kHazardTickInterval;
            ApplyPlayerDamage(kHazardDamage, DeathCause::Other, u8"危険地形で死亡しました。");
            AddMessage(u8"危険地形でダメージを受けました。");
        }
    }
    else
    {
        m_hazardTickTimer = kHazardTickInterval;
    }

    m_lastFrameMovementDistance = suppressMovementDistance ? 0.0f : Distance(frameStartPos, m_player.pos);

    // 次フレームで押下/離上を判定できるよう、現在のShift状態を保存します。
    m_shiftWasPressed = shiftPressed;
}

void SceneNarakuProto::UpdateAction(float dt)
{
    m_rationFullnessWardTimer = std::max(0.0f, m_rationFullnessWardTimer - dt);
    m_rationHydrationPenaltyTimer = std::max(0.0f, m_rationHydrationPenaltyTimer - dt);
    m_unknownWeaponCooldownTimer = std::max(0.0f, m_unknownWeaponCooldownTimer - dt);

    if (m_foodUseTimer > 0.0f)
    {
        m_foodUseTimer = std::max(0.0f, m_foodUseTimer - dt);
        if (m_foodUseTimer <= 0.0f &&
            ((m_usingHeatedFood && m_heatedFoodCount > 0) || (!m_usingHeatedFood && m_foodCount > 0)))
        {
            if (m_usingHeatedFood && m_heatedFoodCount > 0)
            {
                --m_heatedFoodCount;
                m_player.hp = std::min(GetMaxHp(), m_player.hp + kHeatedFoodHpRecovery);
                m_fullness = std::min(kFullnessMaximum, m_fullness + kHeatedFoodFullnessRecovery);
                m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
                m_player.mental = std::min(GetMaxMental(), m_player.mental + kHeatedFoodMentalRecovery);
                AddMessage(u8"加熱食料でHP40、満腹度25、水分75、精神力5を回復しました。");
            }
            else if (!m_usingHeatedFood)
            {
                --m_foodCount;
                m_player.hp = std::min(GetMaxHp(), m_player.hp + kFoodHpRecovery);
                m_fullness = std::min(kFullnessMaximum, m_fullness + kFoodFullnessRecovery);
                m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
                AddMessage(u8"食料でHP20、満腹度10、水分75を回復しました。");
            }
            m_usingHeatedFood = false;
        }
    }

    m_cameraShakeTimer = std::max(0.0f, m_cameraShakeTimer - dt);
    for (AttackHitEffect& effect : m_attackHitEffects)
    {
        effect.remainingTime -= dt;
    }
    m_attackHitEffects.erase(
        std::remove_if(
            m_attackHitEffects.begin(),
            m_attackHitEffects.end(),
            [](const AttackHitEffect& effect) { return effect.remainingTime <= 0.0f; }),
        m_attackHitEffects.end());

    // 攻撃中なら攻撃タイマーを進めます。
    if (m_player.attackTimer > 0.0f)
    {
        // 前フレーム時点の残り時間を保存し、アクティブ判定の突入を検出します。
        float previous = m_player.attackTimer;

        // 攻撃残り時間を減らします。
        m_player.attackTimer = std::max(0.0f, m_player.attackTimer - dt);

        // 攻撃開始からの経過時間を計算します。
        float elapsed = kAttackTotal - m_player.attackTimer;

        // 前フレーム時点の攻撃開始からの経過時間を計算します。
        float previousElapsed = kAttackTotal - previous;

        // このフレームで攻撃判定時間に入った、または判定時間中かを見ます。
        bool activeThisFrame = previousElapsed < kAttackStartup + kAttackActive && elapsed >= kAttackStartup;

        // 攻撃判定が有効なフレームだけ敵との当たり判定を取ります。
        if (activeThisFrame)
        {
            Vec2 relicCenter = Add(m_player.pos, Mul(m_player.facing, kAttackRange));
            float relicDepth = m_player.depth;
            bool foundPrimaryHit = false;
            // すべての敵に対して攻撃が当たるか調べます。
            for (EnemyState& enemy : m_enemies)
            {
                // 死んでいる敵は判定しません。
                if (!enemy.alive || enemy.hitByPlayerAttack) continue;

                // プレイヤーから敵への方向を計算します。
                Vec2 toEnemy = Sub(enemy.pos, m_player.pos);

                // 射程内かつ前方にいる場合だけヒットさせます。
                if (Distance(enemy.pos, m_player.pos) <= kAttackRange && Dot(Normalize(toEnemy), m_player.facing) >= 0.866025f)
                {
                    enemy.hitByPlayerAttack = true;
                    m_attackHitEffects.push_back({ enemy.pos, enemy.depth, kAttackHitEffectDuration });

                    // つるはし1回ぶんのダメージを与えます。
                    enemy.hp -= GetAttackPower();
                    if (!foundPrimaryHit)
                    {
                        relicCenter = enemy.pos;
                        relicDepth = enemy.depth;
                        foundPrimaryHit = true;
                    }
                }
            }
            if (m_attackRelicTriggered)
            {
                m_attackHitEffects.push_back({ relicCenter, relicDepth, kAttackHitEffectDuration });
                for (EnemyState& enemy : m_enemies)
                {
                    if (!enemy.alive || enemy.hitByRelicAttack || std::fabs(enemy.depth - relicDepth) > 0.35f ||
                        Distance(enemy.pos, relicCenter) > kRelicAttackRadius) continue;
                    enemy.hitByRelicAttack = true;
                    enemy.hp -= GetAttackPower() * kRelicAttackDamageScale;
                }
            }
            for (EnemyState& enemy : m_enemies)
            {
                if (!enemy.alive || enemy.hp > 0.0f) continue;
                enemy.alive = false;
                enemy.respawnTimer = kEnemyRespawnTime;
                m_groundFoods.push_back({ enemy.pos, enemy.depth, true });
                AwardEnemyDefeat(enemy);
                AddMessage(u8"敵を倒しました。食料を落としました。");
            }
        }
    }

    if (m_equippedWeapon == WeaponTier::Unknown)
    {
        UpdateUnknownWeaponAttack(dt);
    }
    else
    {
        m_unknownWeaponChargeTimer = 0.0f;
        m_unknownWeaponFiredThisHold = false;
        // 左クリックが押された瞬間に攻撃開始を試します。
        if (IsActionAttackTrigger())
        {
            if (GetLastActiveDevice() == ActiveInputDevice::KeyboardMouse)
            {
                UpdateAimDirectionFromMouse();
            }
            TryStartAttack();
        }
    }

    // スタミナが最大未満なら自然回復できるかを判定します。
    if (m_player.stamina < GetMaxStamina() && m_player.attackTimer <= 0.0f && m_miningIndex < 0)
    {
        // Shift中またはロープ昇降中はスタミナ消費行動中として回復を止めます。
        // ロープは掴まっているだけなら回復し、実際に昇降している時だけ消費行動として扱います。
        const bool ropeClimbing = m_player.onRope && m_lastFrameRopeMoving;

        // Shift中またはロープ昇降中はスタミナ消費行動中として回復を止めます。
        bool spending = IsShiftPress() || ropeClimbing;

        // 消費行動中でなければ1フレームぶん回復します。
        if (!spending) m_player.stamina = std::min(GetMaxStamina(), m_player.stamina +
            m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier() *
            GetDepthLevelStaminaRecoveryMultiplier() * dt);
    }
}

void SceneNarakuProto::UpdateMining(float dt)
{
    // 採掘中でなければ何もしません。
    if (m_miningIndex < 0) return;

    // 被弾ノックバック、足場喪失（空中）、またはロープに掴まった場合は採掘を中断します。
    if (m_player.knockbackTimer > 0.0f)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        AddMessage(u8"ダメージを受けたため、採掘が中断されました。");
        return;
    }
    if (!m_player.grounded)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        AddMessage(u8"足場を失ったため、採掘が中断されました。");
        return;
    }
    if (m_player.onRope)
    {
        m_miningIndex = -1;
        m_miningTimer = 0.0f;
        return;
    }

    // 採掘完了までの残り時間を減らします。
    m_miningTimer -= dt;

    // まだ採掘が終わっていなければ戻ります。
    if (m_miningTimer > 0.0f) return;

    // 採掘中だったポイントを取得します。
    MiningPoint& point = m_miningPoints[m_miningIndex];

    // このポイントを採掘済みにします。
    point.mined = true;
    point.discovered = true;
    point.respawnTimer = kMiningPointRespawnTime;
    ++point.extractionCount;
    point.outputPending = true;

    // リザルト用の採掘数を増やします。
    ++m_result.minedCount;
    const int depth = GetCurrentDepth();
    ++m_result.minedByDepth[static_cast<std::size_t>(depth - 1)];
    AwardExp(static_cast<int>(std::round(20.0f * GetDepthExpMultiplier(depth))));

    // 発見確認に出す旧器を作ります。
    // 種類と重量は採掘した時点で確定し、鑑定状態は表示名だけに反映します。
    const std::uint64_t areaKey = m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size())
        ? (static_cast<std::uint64_t>(m_areas[static_cast<std::size_t>(m_currentAreaIndex)].depth) << 48) ^
            (static_cast<std::uint64_t>(m_areas[static_cast<std::size_t>(m_currentAreaIndex)].sublayer) << 40) ^
            (static_cast<std::uint64_t>(m_areas[static_cast<std::size_t>(m_currentAreaIndex)].areaNumber) << 32)
        : 0;
    SeedRuntimeRandom(m_diveWorldSeed ^ areaKey ^
        (static_cast<std::uint64_t>(m_miningIndex + 1) << 16) ^ point.extractionCount);
    m_pendingRelic = CreateRandomRelic(point.relicName);
    m_runRelicAcquisitionDepths[m_pendingRelic.acquisitionOrder] = depth;

    // 拾わず置く場合に戻す位置を採掘ポイント位置にします。
    m_pendingRelicPos = point.pos;
    m_pendingRelicDepth = point.depth;
    m_pendingRelicMiningIndex = m_miningIndex;

    // 採掘中番号を解除します。
    m_miningIndex = -1;

    // 採掘タイマーを0に戻します。
    m_miningTimer = 0.0f;

    // 旧器を拾うかどうかの確認画面へ移ります。
    m_mode = Mode::RelicPrompt;

    // HUDログに発見を出します。
    AddMessage(u8"旧器を発見しました。");
}

void SceneNarakuProto::UpdateEnemies(float dt)
{
    const int activity = GetCurrentActivity();
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive) continue;
        enemy.landingRecoveryTimer = std::max(0.0f, enemy.landingRecoveryTimer - dt);
        const Vec2 moveStartPos = enemy.pos;
        enemy.moving = false;
        const float moveStartGroundY = GetGroundWorldY(enemy.pos, enemy.depth);
        if (!enemy.grounded)
        {
            const float groundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
            enemy.airTime += dt;
            enemy.feetWorldY += enemy.verticalSpeed * dt;
            enemy.verticalSpeed -= 9.8f * dt;
            enemy.peakFeetWorldY = std::max(enemy.peakFeetWorldY, enemy.feetWorldY);

            if (enemy.verticalSpeed <= 0.0f && enemy.feetWorldY <= groundWorldY)
            {
                const float fallDistance = std::max(0.0f, enemy.peakFeetWorldY - groundWorldY);
                float landingRecovery = 0.0f;
                if (fallDistance >= 6.0f) landingRecovery = kLandingRecoveryHeavy;
                else if (fallDistance >= 4.0f) landingRecovery = kLandingRecoveryMedium;
                else if (fallDistance >= 2.0f) landingRecovery = kLandingRecoveryLight;

                enemy.grounded = true;
                enemy.airTime = 0.0f;
                enemy.verticalSpeed = 0.0f;
                enemy.feetWorldY = groundWorldY;
                enemy.peakFeetWorldY = groundWorldY;
                enemy.landingRecoveryTimer = landingRecovery;
            }

            continue;
        }
        if (enemy.landingRecoveryTimer > 0.0f)
        {
            enemy.feetWorldY = moveStartGroundY;
            enemy.peakFeetWorldY = moveStartGroundY;
            continue;
        }
        if (enemy.chargeTimer > 0.0f)
        {
            if (enemy.type == EnemyType::Territory && Distance(m_player.pos, enemy.territoryCenter) > enemy.territoryRadius)
            {
                enemy.chargeTimer = 0.0f;
                enemy.hasHitThisCharge = false;
                continue;
            }
            const float chargeProgress = std::max(
                0.0f,
                std::min(1.0f, 1.0f - enemy.chargeTimer / kEnemyChargeTime));
            const float chargeSpeedScale = kEnemyChargeStartSpeedScale +
                (kEnemyChargeEndSpeedScale - kEnemyChargeStartSpeedScale) * chargeProgress;
            const Vec2 chargeTarget = Add(
                enemy.pos,
                Mul(enemy.chargeDir, enemy.moveSpeed * 3.0f * chargeSpeedScale * dt));
            enemy.pos = ResolveFloorMove(enemy.pos, chargeTarget, enemy.depth);
            enemy.facing = enemy.chargeDir;
            enemy.moving = Distance(moveStartPos, enemy.pos) > 0.001f;
            enemy.chargeTimer = std::max(0.0f, enemy.chargeTimer - dt);
            if (enemy.chargeTimer <= 0.0f)
            {
                float intervalScale = 1.0f;
                if (activity >= 100) intervalScale = enemy.type == EnemyType::Charger ? 0.50f : 0.25f;
                else if (activity >= 65) intervalScale = enemy.type == EnemyType::Charger ? 0.75f : 0.70f;
                enemy.attackCooldown = enemy.attackInterval * intervalScale;
            }
            const bool sameDepthAsPlayer = std::fabs(enemy.depth - m_player.depth) <= 0.35f;
            if (sameDepthAsPlayer && !enemy.hasHitThisCharge && Distance(enemy.pos, m_player.pos) <= kEnemyHitRange)
            {
                bool invincible = m_player.stepTimer > kStepRecoveryTime;
                if (!invincible)
                {
                    ApplyPlayerDamage(enemy.attackDamage, DeathCause::Enemy, u8"敵の攻撃で死亡しました。");
                    m_cameraShakeTimer = kCameraShakeDuration;
                    m_player.knockbackTimer = kKnockbackTime;
                    Vec2 dir = Normalize(Sub(m_player.pos, enemy.pos));
                    m_player.knockbackVelocity = Mul(dir, kKnockbackDistance / kKnockbackTime);
                    AddMessage(u8"敵の体当たりを受けました。");
                }
                enemy.hasHitThisCharge = true;
            }
            continue;
        }
        if (enemy.telegraphTimer > 0.0f)
        {
            if (enemy.type == EnemyType::Territory && Distance(m_player.pos, enemy.territoryCenter) > enemy.territoryRadius)
            {
                enemy.telegraphTimer = 0.0f;
                continue;
            }
            const Vec2 telegraphDirection = Normalize(Sub(m_player.pos, enemy.pos));
            if (Distance({}, telegraphDirection) > 0.001f)
            {
                enemy.facing = telegraphDirection;
            }
            enemy.telegraphTimer = std::max(0.0f, enemy.telegraphTimer - dt);
            if (enemy.telegraphTimer <= 0.0f)
            {
                enemy.chargeDir = Normalize(Sub(m_player.pos, enemy.pos));
                enemy.chargeTimer = kEnemyChargeTime;
                enemy.hasHitThisCharge = false;
            }
            continue;
        }
        const bool sameDepthAsPlayer = std::fabs(enemy.depth - m_player.depth) <= 0.35f;
        Vec2 toPlayer = Sub(m_player.pos, enemy.pos);
        float dist = Distance(enemy.pos, m_player.pos);

        float searchScale = 1.0f;
        if (activity >= 100) searchScale = enemy.type == EnemyType::Charger ? 1.50f : 1.25f;
        else if (activity >= 40) searchScale = enemy.type == EnemyType::Charger ? 1.25f : 1.10f;
        const bool territoryHostile = enemy.type == EnemyType::Territory &&
            Distance(m_player.pos, enemy.territoryCenter) <= enemy.territoryRadius;
        const bool chargerHostile = enemy.type == EnemyType::Charger && dist <= enemy.searchRange * searchScale;
        const bool hostile = sameDepthAsPlayer && (territoryHostile || chargerHostile);

        if (hostile && dist > 0.1f)
        {
            const Vec2 moveDirection = Normalize(toPlayer);
            const Vec2 moveTarget = Add(enemy.pos, Mul(moveDirection, enemy.moveSpeed * dt));
            enemy.pos = ResolveFloorMove(enemy.pos, moveTarget, enemy.depth);
            enemy.facing = moveDirection;
        }
        else if (enemy.type == EnemyType::Territory)
        {
            Vec2 target = enemy.patrolPoints[enemy.patrolIndex];
            if (Distance(enemy.pos, enemy.territoryCenter) > enemy.territoryRadius) target = enemy.territoryCenter;
            if (Distance(enemy.pos, target) <= 0.3f)
            {
                enemy.patrolIndex = (enemy.patrolIndex + 1) % 3;
                target = enemy.patrolPoints[enemy.patrolIndex];
            }
            const Vec2 moveDirection = Normalize(Sub(target, enemy.pos));
            enemy.pos = ResolveFloorMove(enemy.pos, Add(enemy.pos, Mul(moveDirection, enemy.moveSpeed * dt)), enemy.depth);
            if (Distance({}, moveDirection) > 0.001f)
            {
                enemy.facing = moveDirection;
            }
        }

        enemy.attackCooldown -= dt;
        if (hostile && enemy.attackCooldown <= 0.0f && dist <= 3.0f)
        {
            enemy.telegraphTimer = activity >= 100 ? enemy.telegraphDuration * 0.50f : enemy.telegraphDuration;
        }
        const float currentGroundWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
        const float walkedDropHeight = moveStartGroundY - currentGroundWorldY;
        const bool movedHorizontally = Distance(moveStartPos, enemy.pos) > 0.01f;
        enemy.moving = movedHorizontally;
        if (movedHorizontally && walkedDropHeight >= m_autoFallStartHeight)
        {
            enemy.grounded = false;
            enemy.airTime = 0.0f;
            enemy.verticalSpeed = 0.0f;
            enemy.feetWorldY = moveStartGroundY;
            enemy.peakFeetWorldY = moveStartGroundY;
            enemy.landingRecoveryTimer = 0.0f;
            enemy.telegraphTimer = 0.0f;
            enemy.chargeTimer = 0.0f;
            enemy.hasHitThisCharge = false;
            continue;
        }
        enemy.feetWorldY = currentGroundWorldY;
        enemy.peakFeetWorldY = currentGroundWorldY;
    }
}

void SceneNarakuProto::UpdateUpperLoad(float dt)
{
    const float currentWorldY = m_player.onRope && m_activeRope >= 0
        ? GetRopeWorldY(m_activeRope, m_ropeProgress)
        : m_player.feetWorldY;
    const float ascent = (m_player.grounded || m_player.onRope)
        ? currentWorldY - m_player.previousWorldY
        : 0.0f;

    // 上昇している場合は上昇負荷ゲージを加算します。
    if (ascent > 0.0f)
    {
        // 上昇量そのものを内部ゲージに加算します。
        m_player.upperLoad += ascent;

        // 発症目安20mに達したら現在層の上昇負荷を発症させます。
        if (m_player.upperLoad >= kUpperLoadLimit)
        {
            TriggerUpperLoad();

            // 発症後は内部ゲージを0へ戻します。
            m_player.upperLoad = 0.0f;
        }
    }

    // 上昇していない場合は停止、平地、下降のすべてで一定速度回復します。
    else
    {
        // 1秒あたり1mぶん内部ゲージを減らします。
        m_player.upperLoad = std::max(0.0f, m_player.upperLoad - kUpperLoadRecoveryPerSecond * dt);
    }
}

void SceneNarakuProto::TryInteract()
{
    // 着地直後の硬直中はインタラクトを受け付けません。
    if (m_player.landingRecoveryTimer > 0.0f)
    {
        return;
    }

    // 採掘中は他のインタラクトを受け付けません。
    if (m_miningIndex >= 0)
    {
        return;
    }

    // 落下中は端点だけでなく、ロープ全体をFキーの操作対象にします。
    if (!m_player.grounded && !m_player.onRope && m_player.verticalSpeed < 0.0f)
    {
        float ropeProgress = 0.0f;
        const int ropeIndex = FindFallingRopeIndex(kFallingRopeGrabRadius, ropeProgress);
        if (ropeIndex >= 0)
        {
            const RopePoint& rope = m_ropePoints[ropeIndex];
            m_activeRope = ropeIndex;
            m_ropeProgress = ropeProgress;
            m_player.onRope = true;
            m_player.pos = GetRopePosition(m_activeRope, m_ropeProgress);
            m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
            m_player.grounded = false;
            m_player.verticalSpeed = 0.0f;
            m_player.airTime = 0.0f;
            m_player.feetWorldY = GetRopePlayerFeetWorldY(m_activeRope, m_ropeProgress);
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.landingRecoveryTimer = 0.0f;
            m_player.blockedCellAirTime = 0.0f;
            m_player.blockedCellVelocity = {};
            AddMessage(u8"落下中にロープをつかみました。");
            return;
        }
    }

#if !defined(NARAKU_EDITOR_BUILD)
    for (const NarakuMap::BasePoint& base : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base.layerId);
        if (layerIndex < 0 ||
            std::fabs(m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth - m_player.depth) > 0.35f ||
            !IsNear(m_player.pos, { base.xz.x, base.xz.z }, kInteractRange))
        {
            continue;
        }
        m_mode = base.type == NarakuMap::BaseType::SecondBase ? Mode::SecondBase : Mode::ForwardBase;
        return;
    }
#endif

    for (int gateIndex = 0; gateIndex < static_cast<int>(m_layerGates.size()); ++gateIndex)
    {
        const LayerGateState& gate = m_layerGates[gateIndex];
        const Vec2 interactPoint = gate.isEntry ? gate.ropePos : gate.loadPos;
        if (IsNear(m_player.pos, interactPoint, kInteractRange) &&
            std::fabs(m_player.depth - gate.depth) <= 0.35f)
        {
            TryUseLayerGate(gateIndex);
            return;
        }
    }

    // 帰還地点が最優先です。原点付近でFを押すと帰還します。
    const bool canReturnHere = m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()) &&
        m_areas[m_currentAreaIndex].canReturn;
    if (canReturnHere && IsNear(m_player.pos, m_returnPoint, kReturnRange) &&
        std::fabs(m_player.depth - m_returnDepth) <= 0.35f)
    {
        // 誤操作を避けるため、帰還処理の前に確認を表示します。
        m_mode = Mode::ReturnConfirm;

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }

    // 次にロープへの乗り降りを判定します。
    const int ropeIndex = FindNearestRopeIndex(kInteractRange);
    if (ropeIndex >= 0)
    {
        // 対象ロープを取得します。
        const RopePoint& rope = m_ropePoints[ropeIndex];

        // すでにロープ中なら、近い端へ吸着してロープを離します。
        if (m_player.onRope)
        {
            const bool useBottom = m_ropeProgress >= 0.5f;
            const float endpointDepth = useBottom ? rope.bottomDepth : rope.topDepth;
            const Vec2 endpointPosition = useBottom ? rope.bottomPos : rope.topPos;
            if (CanStandAt(endpointPosition, endpointDepth))
            {
                m_ropeProgress = useBottom ? 1.0f : 0.0f;
                m_player.depth = endpointDepth;
                m_player.pos = endpointPosition;
                m_player.onRope = false;
                m_activeRope = -1;
                m_player.grounded = true;
                m_player.verticalSpeed = 0.0f;
                m_player.airTime = 0.0f;
                m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
                m_player.peakFeetWorldY = m_player.feetWorldY;
                m_player.landingRecoveryTimer = 0.0f;
                AddMessage(u8"ロープを離しました。");
            }
            else
            {
                AddMessage(u8"歩行できない場所にはロープから降りられません。");
            }
        }

        // ロープ外なら、近い端へ吸着してロープにつかまります。
        else
        {
            const float topDistance = Distance(m_player.pos, rope.topPos) + std::fabs(m_player.depth - rope.topDepth);
            const float bottomDistance = Distance(m_player.pos, rope.bottomPos) + std::fabs(m_player.depth - rope.bottomDepth);
            m_ropeProgress = bottomDistance < topDistance
                ? GetBottomRopeGrabProgress(ropeIndex)
                : 0.0f;
            m_player.onRope = true;
            m_activeRope = ropeIndex;
            m_player.pos = GetRopePosition(m_activeRope, m_ropeProgress);
            m_player.depth = rope.topDepth + (rope.bottomDepth - rope.topDepth) * m_ropeProgress;
            m_player.grounded = false;
            m_player.verticalSpeed = 0.0f;
            m_player.airTime = 0.0f;
            m_player.feetWorldY = GetRopePlayerFeetWorldY(m_activeRope, m_ropeProgress);
            m_player.peakFeetWorldY = m_player.feetWorldY;
            m_player.landingRecoveryTimer = 0.0f;
            AddMessage(u8"ロープにつかまりました。");
        }

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }

#if defined(NARAKU_EDITOR_BUILD)
    // Editor歩行確認では層間出入口とロープだけを操作対象にします。
    return;
#endif

    if (TryInteractWithQuestTarget()) return;

    for (int index = 0; index < static_cast<int>(m_fishingPoints.size()); ++index)
    {
        FishingPoint& point = m_fishingPoints[static_cast<size_t>(index)];
        if (IsNear(m_player.pos, point.pos, kInteractRange) && std::fabs(m_player.depth - point.depth) <= 0.35f)
        {
            m_fishingPointIndex = index;
            m_mode = Mode::FishingConfirm;
            return;
        }
    }

    // フィールドに置かれた旧器が近くにあれば拾う確認へ移ります。
    for (GroundRelic& relic : m_groundRelics)
    {
        // 無効化済みの旧器は無視します。
        if (!relic.active) continue;

        // 近くにある旧器だけ反応します。
        if (IsNear(m_player.pos, relic.pos, kInteractRange) && std::fabs(m_player.depth - relic.depth) <= 0.35f)
        {
            // 拾う確認に表示する旧器を設定します。
            m_pendingRelic = relic.item;

            // 拾わず戻す場合の位置を記録します。
            m_pendingRelicPos = relic.pos;
            m_pendingRelicDepth = relic.depth;
            m_pendingRelicMiningIndex = relic.sourceMiningIndex;

            // 一旦地面側を無効化して二重取得を避けます。
            relic.active = false;

            // 旧器確認モードへ移ります。
            m_mode = Mode::RelicPrompt;

            // 1回のF入力で複数の対象を処理しないよう戻ります。
            return;
        }
    }

    // 敵が落とした食料を拾います。
    for (GroundFood& food : m_groundFoods)
    {
        if (!food.active) continue;
        if (IsNear(m_player.pos, food.pos, kInteractRange) && std::fabs(m_player.depth - food.depth) <= 0.35f)
        {
            if (GetCurrentWeight() + 1.0f > GetPickupWeightLimit())
            {
                AddMessage(u8"これ以上は重すぎて食料を拾えません。");
                return;
            }
            food.active = false;
            ++m_foodCount;
            AddMessage(u8"食料を1個拾いました。");
            return;
        }
    }

    const std::uint32_t waterFlags = GetNearbyWaterFlags();
    if (waterFlags != NarakuMap::CellAttributeNone)
    {
        if (GetCurrentDepth() == 5)
        {
            AddMessage(u8"第五層の湖水は飲水・採水には利用できません。");
            return;
        }
        m_pendingWaterFlags = waterFlags;
        m_mode = Mode::WaterPrompt;
        return;
    }

    // 最後に採掘ポイントの開始判定を行います。
    for (int i = 0; i < static_cast<int>(m_miningPoints.size()); ++i)
    {
        // 対象採掘ポイントを取得します。
        MiningPoint& point = m_miningPoints[i];

        // 採掘済み、または遠いポイントは無視します。
        if (point.mined || std::fabs(m_player.depth - point.depth) > 0.35f || !IsNear(m_player.pos, point.pos, kInteractRange)) continue;

        // スタミナが足りない場合は採掘を開始しません。
        if (!CanSpendStamina(m_debugPlayerParams.miningCost))
        {
            // HUDログにスタミナ不足を出します。
            AddMessage(u8"スタミナが足りないため採掘できません。");

            // 近くの採掘対象を見つけたので処理を終えます。
            return;
        }

        // 採掘開始時にスタミナを消費します。
        SpendStamina(m_debugPlayerParams.miningCost);
        m_fullness = std::max(0.0f, m_fullness - GetDepthLevelFullnessConsumptionMultiplier());
        m_hydration = std::max(0.0f, m_hydration - GetDepthLevelFullnessConsumptionMultiplier());

        // 採掘中のポイント番号を記録します。
        m_miningIndex = i;

        // 装備中のつるはしによる速度倍率を採掘所要時間へ反映します。
        m_miningDuration = kMiningTime / GetMiningSpeedMultiplier();
        m_miningTimer = m_miningDuration;

        // HUDログに採掘開始を出します。
        AddMessage(u8"採掘を開始しました。");

        // 1回のF入力で複数の対象を処理しないよう戻ります。
        return;
    }
}

void SceneNarakuProto::TryStartStep()
{
    // 着地直後の硬直中はステップさせません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 重量100%以上ではステップ不可です。
    if (GetCurrentWeight() >= GetMaxWeight())
    {
        AddMessage(u8"重量が重すぎてステップできません。");
        ShowCenterNotification(u8"重すぎてステップができない！");
        return;
    }

    // スタミナ不足ならステップ不可です。
    if (!CanSpendStamina(m_debugPlayerParams.stepCost)) { AddMessage(u8"スタミナが足りないためステップできません。"); return; }

    // ステップ1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.stepCost);
    m_fullness = std::max(0.0f, m_fullness - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());

    // 無敵時間と後硬直の合計時間を設定します。
    m_player.stepTimer = kStepInvincibleTime + kStepRecoveryTime;
}

void SceneNarakuProto::TryStartJump()
{
    // 着地直後の硬直中はジャンプさせません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 地上にいない時は二段ジャンプを許可しません。
    if (!m_player.grounded) return;

    // ロープ中はジャンプさせません。
    if (m_player.onRope) return;

    // 重量100%以上ではジャンプ不可です。
    if (GetCurrentWeight() >= GetMaxWeight()) { AddMessage(u8"重量が重すぎてジャンプできません。"); return; }

    // スタミナ不足ならジャンプ不可です。
    if (!CanSpendStamina(m_debugPlayerParams.jumpCost)) { AddMessage(u8"スタミナが足りないためジャンプできません。"); return; }

    // ジャンプ1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.jumpCost);
    m_fullness = std::max(0.0f, m_fullness - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.5f * GetDepthLevelFullnessConsumptionMultiplier());

    // 空中状態へ切り替えます。
    m_player.grounded = false;

    // 高さ1m程度を想定した初速を入れます。
    m_player.verticalSpeed = 4.45f;

    // 現在地面の絶対高さを足元基準として記録します。
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;

    m_jumpEffects.push_back({ m_player.pos, m_player.depth, kJumpEffectDuration });

    // 空中時間を0から測ります。
    m_player.airTime = 0.0f;
}

void SceneNarakuProto::TryStartAttack()
{
    // 着地直後の硬直中は攻撃させません。
    if (m_player.landingRecoveryTimer > 0.0f) return;

    // 攻撃中は次の攻撃を開始しません。
    if (m_player.attackTimer > 0.0f) return;

    // 採掘中は攻撃させません。
    if (m_miningIndex >= 0) return;

    // スタミナ不足なら攻撃不可です。
    if (!CanSpendStamina(m_debugPlayerParams.attackCost)) { AddMessage(u8"スタミナが足りないため攻撃できません。"); return; }

    // 攻撃1回ぶんのスタミナを消費します。
    SpendStamina(m_debugPlayerParams.attackCost);
    m_fullness = std::max(0.0f, m_fullness - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());

    m_attackRelicTriggered = false;
    int relicIndex = -1;
    int fewestUses = std::numeric_limits<int>::max();
    for (int i = 0; i < static_cast<int>(m_inventory.size()); ++i)
    {
        const RelicItem& item = m_inventory[i];
        if (item.type != RelicType::Offensive || item.broken || item.remainingUses <= 0) continue;
        if (item.remainingUses < fewestUses)
        {
            relicIndex = i;
            fewestUses = item.remainingUses;
        }
    }
    const float relicStaminaCost = GetMaxStamina() * 0.10f;
    if (relicIndex >= 0 && CanSpendStamina(relicStaminaCost))
    {
        SpendStamina(relicStaminaCost);
        RelicItem& item = m_inventory[relicIndex];
        --item.remainingUses;
        if (item.remainingUses <= 0)
        {
            item.remainingUses = 0;
            item.broken = true;
            item.value = 5;
        }
        m_attackRelicTriggered = true;
    }

    // 攻撃全体時間を設定します。
    m_player.attackTimer = kAttackTotal;

    // 連続攻撃が左右の往復に見えるよう、攻撃開始ごとに振り方向を反転します。
    m_player.attackSwingReverse = !m_player.attackSwingReverse;
    for (EnemyState& enemy : m_enemies)
    {
        enemy.hitByPlayerAttack = false;
        enemy.hitByRelicAttack = false;
    }
}

void SceneNarakuProto::UpdateUnknownWeaponAttack(float dt)
{
    if (!IsActionAttackPress())
    {
        m_unknownWeaponChargeTimer = 0.0f;
        m_unknownWeaponFiredThisHold = false;
        return;
    }
    if (m_unknownWeaponFiredThisHold || m_unknownWeaponCooldownTimer > 0.0f ||
        m_player.attackTimer > 0.0f || m_miningIndex >= 0 || m_player.landingRecoveryTimer > 0.0f)
    {
        return;
    }
    m_unknownWeaponChargeTimer += dt;
    if (m_unknownWeaponChargeTimer < kUnknownWeaponChargeDuration) return;
    m_unknownWeaponFiredThisHold = true;
    m_unknownWeaponChargeTimer = 0.0f;
    if (GetLastActiveDevice() == ActiveInputDevice::KeyboardMouse)
    {
        UpdateAimDirectionFromMouse();
    }
    FireUnknownWeapon();
}

void SceneNarakuProto::FireUnknownWeapon()
{
    const float staminaCost = m_debugPlayerParams.attackCost * 2.0f;
    if (!CanSpendStamina(staminaCost))
    {
        AddMessage(u8"スタミナが足りないため未知の武器を発射できません。");
        return;
    }
    SpendStamina(staminaCost);
    m_fullness = std::max(0.0f, m_fullness - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());
    m_hydration = std::max(0.0f, m_hydration - 0.75f * GetDepthLevelFullnessConsumptionMultiplier());
    m_player.attackTimer = kAttackTotal;
    m_attackRelicTriggered = false;
    m_unknownWeaponCooldownTimer = kUnknownWeaponCooldownDuration;

    int defeated = 0;
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive || std::fabs(enemy.depth - m_player.depth) > 0.35f) continue;
        const Vec2 relative = Sub(enemy.pos, m_player.pos);
        const float forward = Dot(relative, m_player.facing);
        const float side = std::fabs(relative.x * m_player.facing.y - relative.y * m_player.facing.x);
        if (forward <= 0.0f || side > kUnknownWeaponHalfWidth) continue;
        enemy.hp = 0.0f;
        enemy.alive = false;
        enemy.respawnTimer = kEnemyRespawnTime;
        m_attackHitEffects.push_back({ enemy.pos, enemy.depth, kAttackHitEffectDuration });
        m_groundFoods.push_back({ enemy.pos, enemy.depth, true });
        AwardEnemyDefeat(enemy);
        ++defeated;
    }
    AddMessage(defeated > 0
        ? u8"未知の武器が前方の敵を貫きました。"
        : u8"未知の武器を発射しました。");
}

void SceneNarakuProto::TriggerUpperLoad()
{
    if (TryPreventUpperLoad()) return;

    const int depth = GetCurrentDepth();
    const auto applyMentalAndVision = [this](float mentalLoss, float visionOcclusion)
    {
        if (mentalLoss > 0.0f)
        {
            ApplyMentalDamage(mentalLoss, DeathCause::UpperLoad, u8"上昇負荷で精神力が0になりました。");
        }
        m_upperLoadVisionTimer = mentalLoss;
        m_upperLoadVisionOcclusion = visionOcclusion;

        if (mentalLoss <= 0.0f)
        {
            AddMessage(u8"上昇負荷が発症しましたが、影響を受けませんでした。");
            return;
        }

        std::ostringstream message;
        message << u8"上昇負荷が発症しました。精神力-"
            << static_cast<int>(std::round(mentalLoss)) << u8"。";
        AddMessage(message.str());
    };

    switch (depth)
    {
    case 1:
        applyMentalAndVision(
            GetLevelInterpolatedValue(kUpperLoadFirstMentalLoss, m_level),
            GetLevelInterpolatedValue(kUpperLoadFirstVisionOcclusion, m_level));
        break;
    case 2:
        applyMentalAndVision(
            GetLevelInterpolatedValue(kUpperLoadSecondMentalLoss, m_level),
            GetLevelInterpolatedValue(kUpperLoadSecondVisionOcclusion, m_level));
        break;
    case 3:
        applyMentalAndVision(
            GetLevelInterpolatedValue(kUpperLoadSecondMentalLoss, m_level) * 1.5f,
            GetLevelInterpolatedValue(kUpperLoadSecondVisionOcclusion, m_level));
        break;
    case 4:
    {
        const float damageRatio = GetLevelInterpolatedValue(kUpperLoadFourthDamageRatios, m_level);
        const float damage = GetMaxHp() * damageRatio;
        if (m_player.hp - damage <= 0.0f && TryConsumeSurvivalRelic(true, false))
        {
            return;
        }
        m_player.hp = std::max(0.0f, m_player.hp - damage);
        if (m_player.hp <= 0.0f)
        {
            StartDeath(u8"上昇負荷で体力が0になりました。", DeathCause::UpperLoad);
            return;
        }
        AddMessage(u8"上昇負荷が発症し、最大HP割合ダメージを受けました。");
        break;
    }
    case 5:
    {
        const bool wasActive = m_upperLoadFifthTimer > 0.0f;
        m_upperLoadFifthTimer += GetLevelInterpolatedValue(kUpperLoadFifthDurations, m_level);
        m_upperLoadFifthDamageRatio = GetLevelInterpolatedValue(kUpperLoadFifthDamageRatios, m_level);
        if (!wasActive) m_upperLoadFifthDamageCooldown = 0.0f;
        AddMessage(u8"上昇負荷が発症し、盲目と移動負荷が発生しました。");
        break;
    }
    default:
        break;
    }
}

void SceneNarakuProto::UpdateUpperLoadEffects(float dt)
{
    if (m_upperLoadVisionTimer > 0.0f)
    {
        m_upperLoadVisionTimer = std::max(0.0f, m_upperLoadVisionTimer - dt);
        if (m_upperLoadVisionTimer <= 0.0f) m_upperLoadVisionOcclusion = 0.0f;
    }

    if (m_upperLoadFifthTimer <= 0.0f) return;

    m_upperLoadFifthDamageCooldown = std::max(0.0f, m_upperLoadFifthDamageCooldown - dt);
    float moveX = 0.0f, moveY = 0.0f;
    GetActionMoveVector(moveX, moveY);
    const bool movementInput = moveX != 0.0f || moveY != 0.0f;
    if (movementInput && m_upperLoadFifthDamageCooldown <= 0.0f)
    {
        const float damage = GetMaxHp() * m_upperLoadFifthDamageRatio;
        if (!(m_player.hp - damage <= 0.0f && TryConsumeSurvivalRelic(true, false)))
        {
            m_player.hp = std::max(0.0f, m_player.hp - damage);
            if (m_player.hp <= 0.0f)
            {
                StartDeath(u8"上昇負荷の移動ダメージで体力が0になりました。", DeathCause::UpperLoad);
                return;
            }
        }
        m_upperLoadFifthDamageCooldown = kUpperLoadFifthDamageInterval;
    }

    m_upperLoadFifthTimer = std::max(0.0f, m_upperLoadFifthTimer - dt);
    if (m_upperLoadFifthTimer <= 0.0f)
    {
        m_upperLoadFifthDamageRatio = 0.0f;
        m_upperLoadFifthDamageCooldown = 0.0f;
    }
}

void SceneNarakuProto::UpdateHunger(float dt)
{
    const int depth = GetCurrentDepth();
    float perMinute = 0.5f;
    if (m_miningIndex >= 0 || m_player.stepTimer > 0.0f) perMinute = 0.0f;
    else if (m_lastFrameRopeMoving) perMinute = 2.0f;
    else if (m_player.grounded && m_player.landingRecoveryTimer <= 0.0f && m_player.knockbackTimer <= 0.0f &&
        m_lastFrameMovementDistance > 0.001f) perMinute = m_lastFrameRunning ? 1.5f : 1.0f;
    if (m_rationFullnessWardTimer <= 0.0f)
    {
        m_fullness = std::max(0.0f,
            m_fullness - perMinute / 60.0f * GetDepthLevelFullnessConsumptionMultiplier() * dt);
    }
    m_player.stamina = std::min(m_player.stamina, GetMaxStamina());

    if (m_player.knockbackTimer <= 0.0f && m_lastFrameMovementDistance > 0.0f)
    {
        const std::size_t index = static_cast<std::size_t>(depth - 1);
        const float before = m_movementExpByDepth[index];
        const float cap = static_cast<float>(GetRulesForDepth(depth).movementExpCap);
        m_movementExpByDepth[index] = std::min(cap, before + m_lastFrameMovementDistance * GetDepthMovementExpMultiplier(depth));
        const int gained = static_cast<int>(std::floor(m_movementExpByDepth[index])) - static_cast<int>(std::floor(before));
        if (gained > 0) AwardExp(gained);
    }

    if (m_fullness <= 0.0f && m_mode == Mode::Explore)
    {
        StartDeath(u8"餓死しました。", DeathCause::Starvation);
    }
}

void SceneNarakuProto::UpdateHydration(float dt)
{
    float perMinute = 0.5f;
    if (m_miningIndex >= 0 || m_player.stepTimer > 0.0f) perMinute = 0.0f;
    else if (m_lastFrameRopeMoving) perMinute = 2.0f;
    else if (m_player.grounded && m_player.landingRecoveryTimer <= 0.0f && m_player.knockbackTimer <= 0.0f &&
        m_lastFrameMovementDistance > 0.001f) perMinute = m_lastFrameRunning ? 1.5f : 1.0f;
    const float rationPenalty = m_rationHydrationPenaltyTimer > 0.0f
        ? kRationHydrationPenaltyMultiplier : 1.0f;
    m_hydration = std::max(0.0f, m_hydration -
        perMinute / 60.0f * GetDepthLevelFullnessConsumptionMultiplier() * rationPenalty * dt);

    if (m_hydration <= 0.0f)
    {
        if (!m_hydrationWasZero)
        {
            m_hydrationWasZero = true;
            m_dehydrationZeroTimer = m_dehydrationVisionStrength > 0.0f
                ? kDehydrationDelay + m_dehydrationVisionStrength * kDehydrationTransitionDuration
                : 0.0f;
        }
        m_dehydrationZeroTimer += dt;
        const float progress = std::max(0.0f, std::min(1.0f,
            (m_dehydrationZeroTimer - kDehydrationDelay) / kDehydrationTransitionDuration));
        m_dehydrationVisionStrength = std::max(m_dehydrationVisionStrength, progress);
    }
    else
    {
        m_hydrationWasZero = false;
        m_dehydrationZeroTimer = 0.0f;
        m_dehydrationVisionStrength = std::max(0.0f,
            m_dehydrationVisionStrength - dt / kDehydrationTransitionDuration);
    }
}

void SceneNarakuProto::UpdateCooking(float dt)
{
    if (m_cookingTarget == CookingTarget::None) return;

    const bool interrupted = m_lastFrameMovementDistance > 0.001f ||
        m_player.attackTimer > 0.0f || m_player.stepTimer > 0.0f ||
        m_player.knockbackTimer > 0.0f || m_player.onRope || !m_player.grounded ||
        m_player.hp < m_cookingPreviousHp;
    if (interrupted)
    {
        CancelCooking(u8"行動または被弾により調理を中断しました。料理セットの使用回数は戻りません。");
        return;
    }

    m_cookingPreviousHp = m_player.hp;
    m_cookingTimer = std::max(0.0f, m_cookingTimer - dt);
    if (m_cookingTimer > 0.0f) return;

    if (m_cookingTarget == CookingTarget::BoilBottle &&
        m_cookingBottleIndex >= 0 && m_cookingBottleIndex < static_cast<int>(m_waterBottles.size()))
    {
        WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(m_cookingBottleIndex)];
        if (bottle.quality == WaterQuality::Unboiled && bottle.amount > 0.0f)
        {
            bottle.quality = WaterQuality::Boiled;
            bottle.foodPoisoningChance = 0.0f;
            AddMessage(u8"水筒の水を煮沸しました。");
        }
    }
    else if (m_cookingTarget == CookingTarget::HeatFood && m_foodCount > 0)
    {
        --m_foodCount;
        ++m_heatedFoodCount;
        AddMessage(u8"携帯食料を加熱しました。");
    }
    else if (m_cookingTarget == CookingTarget::CookFish && m_rawFishCount > 0)
    {
        --m_rawFishCount;
        ++m_cookedFishCount;
        AddMessage(u8"見たことない魚を調理しました。");
    }
    else if (m_cookingTarget == CookingTarget::CookSizedFish && m_cookingFishSize >= 0 && m_cookingFishSize < 3 &&
        m_rawSizedFish[static_cast<size_t>(m_cookingFishSize)] > 0)
    {
        --m_rawSizedFish[static_cast<size_t>(m_cookingFishSize)];
        ++m_cookedSizedFish[static_cast<size_t>(m_cookingFishSize)];
        AddMessage(u8"魚を調理しました。");
    }
    m_cookingTarget = CookingTarget::None;
    m_cookingFishSize = -1;
    m_cookingBottleIndex = -1;
}

void SceneNarakuProto::UpdateMentalAbilities(float dt)
{
    m_upperLoadWardTimer = std::max(0.0f, m_upperLoadWardTimer - dt);
    m_miningSenseTimer = std::max(0.0f, m_miningSenseTimer - dt);
    if (m_miningSenseTimer <= 0.0f)
    {
        for (MiningPoint& point : m_miningPoints) point.sensed = false;
        for (AreaState& area : m_areas)
            for (MiningPoint& point : area.miningPoints) point.sensed = false;
    }

    const bool pressed = IsActionSkillPress();
    if (pressed)
    {
        m_qHoldTime += dt;
        if (!m_qLongTriggered && m_qHoldTime >= kQHoldThreshold)
        {
            m_qLongTriggered = true;
            ActivateUpperLoadWard();
        }
    }
    else if (m_qWasPressed)
    {
        if (!m_qLongTriggered) ActivateMiningSense();
        m_qHoldTime = 0.0f;
        m_qLongTriggered = false;
    }
    m_qWasPressed = pressed;
}

void SceneNarakuProto::ActivateMiningSense()
{
    if (m_level < 30) { ShowCenterNotification(u8"Lv30で解放されます。"); return; }
    const float cost = 15.0f * GetMentalAbilityDepthMultiplier(GetCurrentDepth()) *
        GetDepthLevelMentalConsumptionMultiplier();
    if (m_player.mental < cost) { ShowCenterNotification(u8"精神力が足りない！"); return; }
    m_player.mental -= cost;
    m_miningSenseTimer = kMentalSenseDuration;

    std::vector<int> queue;
    std::vector<int> visited;
    if (m_currentAreaIndex >= 0) { queue.push_back(m_currentAreaIndex); visited.push_back(m_currentAreaIndex); }
    for (std::size_t cursor = 0; cursor < queue.size() && visited.size() < 4; ++cursor)
    {
        const AreaState& area = m_areas[queue[cursor]];
        for (const LayerGateState& gate : area.layerGates)
        {
            const int next = gate.destinationAreaIndex;
            if (next < 0 || next >= static_cast<int>(m_areas.size()) || m_areas[next].depth != GetCurrentDepth() ||
                std::find(visited.begin(), visited.end(), next) != visited.end()) continue;
            visited.push_back(next);
            queue.push_back(next);
            if (visited.size() >= 4) break;
        }
    }
    for (int areaIndex : visited)
    {
        if (areaIndex == m_currentAreaIndex) for (MiningPoint& point : m_miningPoints) point.sensed = true;
        else for (MiningPoint& point : m_areas[areaIndex].miningPoints) point.sensed = true;
    }
    ShowCenterNotification(u8"採掘地点を15秒間感知します。");
}

void SceneNarakuProto::ActivateUpperLoadWard()
{
    if (m_level < 30) { ShowCenterNotification(u8"Lv30で解放されます。"); return; }
    const int depth = GetCurrentDepth();
    const float cost = 30.0f * GetMentalAbilityDepthMultiplier(depth) *
        GetDepthLevelMentalConsumptionMultiplier();
    if (m_player.mental < cost) { ShowCenterNotification(u8"精神力が足りない！"); return; }
    m_player.mental -= cost;
    m_upperLoadWardTimer = static_cast<float>(1 + (std::min(m_level, 100) - 30) / 10);
    ShowCenterNotification(u8"遺物を鎮め、一定時間の上昇負荷を防ぎます。");
}

bool SceneNarakuProto::TryPreventUpperLoad()
{
    if (GetCurrentDepth() >= 6 && TryConsumeFatalUpperLoadCartridge()) return true;
    if (m_upperLoadWardTimer <= 0.0f) return false;
    AddMessage(u8"安定化の力が上昇負荷を防ぎました。");
    return true;
}

bool SceneNarakuProto::TryConsumeFatalUpperLoadCartridge()
{
    if (!HasUnknownArmorSetEffect() || m_cartridgeCount <= 0) return false;
    --m_cartridgeCount;
    AddMessage(u8"カートリッジが致命的な上昇負荷を防ぎ、精神力への影響も遮断しました。");
    return true;
}

void SceneNarakuProto::UpdateExplorationDiscovery()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size())) return;
    AreaState& area = m_areas[m_currentAreaIndex];
    int enemySeen = 0;
    for (EnemyState& enemy : m_enemies)
    {
        if (!enemy.discovered && Distance(enemy.pos, m_player.pos) <= kDiscoveryRange)
        {
            enemy.discovered = true;
            UpdateQuestDiscoveryProgress(true, false, GetCurrentDepth());
        }
        if (enemy.discovered) ++enemySeen;
    }
    int miningSeen = 0;
    for (MiningPoint& point : m_miningPoints)
    {
        if (!point.discovered && Distance(point.pos, m_player.pos) <= kDiscoveryRange)
        {
            point.discovered = true;
            UpdateQuestDiscoveryProgress(false, true, GetCurrentDepth());
        }
        if (point.discovered) ++miningSeen;
    }
    area.discoveredEnemyCount = enemySeen;
    area.discoveredMiningCount = miningSeen;
    UpdateImportantQuestExploration();

    std::size_t totalCells = 0;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
        totalCells += static_cast<std::size_t>(std::max(0, layer.gridWidth - 1) * std::max(0, layer.gridHeight - 1));
    if (area.discoveredCells.size() != totalCells) area.discoveredCells.assign(totalCells, 0);
    if (area.discoveredCliffs.size() != totalCells) area.discoveredCliffs.assign(totalCells, 0);
    std::size_t offset = 0;
    int validCells = 0;
    int discoveredValid = 0;
    int totalCliffs = 0;
    int discoveredCliffs = 0;
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        const int cellWidth = std::max(0, layer.gridWidth - 1);
        const int cellHeight = std::max(0, layer.gridHeight - 1);
        int playerCellX = -1, playerCellZ = -1;
        float fracX = 0.0f, fracZ = 0.0f;
        const bool onLayer = std::fabs(layer.layerDepth - m_player.depth) <= 0.35f &&
            TryGetLayerCellAt(layer, m_player.pos, playerCellX, playerCellZ, fracX, fracZ);
        for (int z = 0; z < cellHeight; ++z)
        {
            for (int x = 0; x < cellWidth; ++x)
            {
                const std::size_t index = offset + static_cast<std::size_t>(z * cellWidth + x);
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, x, z);
                if ((flags & NarakuMap::CellAttributeRemoved) != 0u) continue;
                ++validCells;
                if (onLayer && x == playerCellX && z == playerCellZ) area.discoveredCells[index] = 1;
                if (area.discoveredCells[index] != 0) ++discoveredValid;
                if ((flags & NarakuMap::CellAttributeCliffEdge) != 0u)
                {
                    ++totalCliffs;
                    const float width = static_cast<float>(cellWidth) * layer.cellSize;
                    const float height = static_cast<float>(cellHeight) * layer.cellSize;
                    const Vec2 center = {
                        layer.center.x - width * 0.5f + (static_cast<float>(x) + 0.5f) * layer.cellSize,
                        layer.center.z - height * 0.5f + (static_cast<float>(z) + 0.5f) * layer.cellSize };
                    if (std::fabs(layer.layerDepth - m_player.depth) <= 0.35f && Distance(center, m_player.pos) <= kDiscoveryRange)
                        area.discoveredCliffs[index] = 1;
                    if (area.discoveredCliffs[index] != 0) ++discoveredCliffs;
                }
            }
        }
        offset += static_cast<std::size_t>(cellWidth * cellHeight);
    }
    area.totalCliffCount = totalCliffs;
    area.discoveredCliffCount = discoveredCliffs;
    if (validCells > 0)
    {
        const float ratio = static_cast<float>(discoveredValid) / static_cast<float>(validCells);
        for (int threshold = 0; threshold < 4; ++threshold)
        {
            if (!area.cellExpThresholds[threshold] && ratio >= 0.25f * static_cast<float>(threshold + 1))
            {
                area.cellExpThresholds[threshold] = true;
                AwardExp(static_cast<int>(std::round(20.0f * GetDepthExpMultiplier(GetCurrentDepth()))));
            }
        }
    }
}

SceneNarakuProto::EnemyState SceneNarakuProto::CreateEnemy(EnemyType type, int depth, const Vec2& position) const
{
    const DepthRules& rules = GetRulesForDepth(depth);
    EnemyState enemy;
    enemy.type = type;
    enemy.pos = position;
    enemy.spawnPos = position;
    enemy.territoryCenter = position;
    enemy.depth = m_startDepth;
    for (const FloorRegion& floor : m_floorRegions)
    {
        if (IsInsideFloor(floor, position)) { enemy.depth = floor.depth; break; }
    }
    enemy.feetWorldY = GetGroundWorldY(enemy.pos, enemy.depth);
    enemy.peakFeetWorldY = enemy.feetWorldY;
    if (type == EnemyType::Charger)
    {
        enemy.maxHp = 30.0f * rules.enemyHp;
        enemy.attackDamage = 10.0f * rules.enemyAttack;
        enemy.searchRange = 8.0f;
        enemy.moveSpeed = kEnemyWalkSpeed * rules.enemyMove;
        enemy.attackInterval = kEnemyAttackInterval * rules.enemyInterval;
    }
    else
    {
        enemy.territoryRank = static_cast<TerritoryRank>(RandomInt(0, 2));
        const int rank = static_cast<int>(enemy.territoryRank);
        const float minimumAttack[] = { 15.0f, 20.0f, 25.0f };
        const float maximumAttack[] = { 20.0f, 25.0f, 30.0f };
        const float search[] = { 3.0f, 4.0f, 5.0f };
        const float divisor[] = { 6.0f, 5.0f, 4.0f };
        float stageSide = 24.0f;
        for (const FloorRegion& floor : m_floorRegions)
        {
            if (!IsInsideFloor(floor, position)) continue;
            stageSide = std::max(4.0f, std::min(floor.halfSize.x * 2.0f, floor.halfSize.y * 2.0f));
            break;
        }
        enemy.maxHp = 120.0f * rules.enemyHp;
        enemy.attackDamage = RandomFloat(minimumAttack[rank], maximumAttack[rank]) * rules.enemyAttack;
        enemy.searchRange = search[rank];
        enemy.territoryRadius = stageSide / divisor[rank];
        const float level40Growth = (1.0f - std::exp(-0.5f * (39.0f / 99.0f))) / (1.0f - std::exp(-0.5f));
        enemy.moveSpeed = (1.5f * (1.0f + 1.5f * level40Growth) * 0.75f) * rules.enemyMove;
        enemy.attackInterval = 1.0f * rules.enemyInterval;
        for (int i = 0; i < 3; ++i)
        {
            enemy.patrolPoints[i] = position;
            for (int attempt = 0; attempt < 16; ++attempt)
            {
                const float angle = DirectX::XM_2PI * static_cast<float>(i) / 3.0f + RandomFloat(-0.35f, 0.35f);
                const float radius = enemy.territoryRadius * RandomFloat(0.35f, 0.75f);
                const Vec2 candidate = {
                    position.x + std::cos(angle) * radius,
                    position.y + std::sin(angle) * radius
                };
                if (!HasFloorAt(candidate, enemy.depth)) continue;
                enemy.patrolPoints[i] = candidate;
                break;
            }
        }
    }
    enemy.hp = enemy.maxHp;
    enemy.attackCooldown = RandomFloat(0.0f, enemy.attackInterval);
    enemy.telegraphDuration = kEnemyTelegraphTime;
    return enemy;
}

bool SceneNarakuProto::HasTerritoryTreeDensity(const Vec2& position) const
{
    int treeCount = 0;
    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        const auto resource = std::find_if(m_environmentModels.begin(), m_environmentModels.end(),
            [&object](const EnvironmentModelResource& value) { return value.id == object.modelId; });
        if (resource == m_environmentModels.end() || !resource->isTree) continue;
        if (Distance(position, { object.xz.x, object.xz.z }) <= 5.0f && ++treeCount >= 10) return true;
    }
    return false;
}

SceneNarakuProto::Vec2 SceneNarakuProto::FindEnemySpawnPoint(float minimumPlayerDistance, bool requireTerritory, bool* found) const
{
    if (found) *found = false;
    if (m_floorRegions.empty()) return m_startPoint;
    for (int attempt = 0; attempt < 96; ++attempt)
    {
        const FloorRegion& floor = m_floorRegions[static_cast<std::size_t>(RandomInt(0, static_cast<int>(m_floorRegions.size()) - 1))];
        const Vec2 point = {
            RandomFloat(floor.center.x - floor.halfSize.x * 0.85f, floor.center.x + floor.halfSize.x * 0.85f),
            RandomFloat(floor.center.y - floor.halfSize.y * 0.85f, floor.center.y + floor.halfSize.y * 0.85f) };
        if (!HasFloorAt(point, floor.depth) || Distance(point, m_player.pos) < minimumPlayerDistance) continue;
        if (requireTerritory && !HasTerritoryTreeDensity(point)) continue;
        if (found) *found = true;
        return point;
    }
    return m_startPoint;
}

void SceneNarakuProto::SpawnEnemiesForCurrentArea()
{
    m_enemies.clear();
    const int depth = GetCurrentDepth();
    const DepthRules& rules = GetRulesForDepth(depth);
    const int chargerCount = RandomInt(0, rules.chargerMax);
    const int territoryCount = RandomInt(0, rules.territoryMax);
    for (int i = 0; i < chargerCount; ++i)
    {
        bool found = false;
        const Vec2 point = FindEnemySpawnPoint(3.0f, false, &found);
        if (found) m_enemies.push_back(CreateEnemy(EnemyType::Charger, depth, point));
    }
    for (int i = 0; i < territoryCount; ++i)
    {
        bool found = false;
        const Vec2 point = FindEnemySpawnPoint(3.0f, true, &found);
        if (found) m_enemies.push_back(CreateEnemy(EnemyType::Territory, depth, point));
    }
}

bool SceneNarakuProto::RespawnEnemy(EnemyState& enemy)
{
    bool found = false;
    const Vec2 point = FindEnemySpawnPoint(kEnemyRespawnMinPlayerDistance, enemy.type == EnemyType::Territory, &found);
    if (!found)
    {
        enemy.respawnTimer = kEnemyRespawnRetry;
        return false;
    }
    enemy = CreateEnemy(enemy.type, GetCurrentDepth(), point);
    return true;
}

void SceneNarakuProto::UpdateRespawns(float dt)
{
    for (EnemyState& enemy : m_enemies)
    {
        if (enemy.alive) continue;
        enemy.respawnTimer = std::max(0.0f, enemy.respawnTimer - dt);
        if (enemy.respawnTimer <= 0.0f) RespawnEnemy(enemy);
    }
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (areaIndex == m_currentAreaIndex) continue;
        for (EnemyState& enemy : m_areas[areaIndex].enemies)
            if (!enemy.alive) enemy.respawnTimer = std::max(0.0f, enemy.respawnTimer - dt);
    }
}

void SceneNarakuProto::UpdateMiningRespawns(float dt)
{
    auto updatePoints = [dt](std::vector<MiningPoint>& points)
    {
        for (MiningPoint& point : points)
        {
            if (!point.mined) continue;
            point.respawnTimer = std::max(0.0f, point.respawnTimer - dt);
            if (point.respawnTimer <= 0.0f && !point.outputPending)
            {
                point.mined = false;
                point.sensed = false;
            }
        }
    };
    updatePoints(m_miningPoints);
    for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
    {
        if (areaIndex != m_currentAreaIndex) updatePoints(m_areas[static_cast<std::size_t>(areaIndex)].miningPoints);
    }
}

void SceneNarakuProto::AwardEnemyDefeat(const EnemyState& enemy)
{
    const int depth = GetCurrentDepth();
    const int baseExp = enemy.type == EnemyType::Territory ? 250 : 100;
    AwardExp(static_cast<int>(std::round(baseExp * GetDepthExpMultiplier(depth))));
    const std::size_t index = static_cast<std::size_t>(depth - 1);
    if (enemy.type == EnemyType::Territory) ++m_result.territoryKillsByDepth[index];
    else ++m_result.chargerKillsByDepth[index];
    UpdateImportantQuestDefeat(enemy);
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.type != QuestType::Hunt || quest.targetDepth != depth) continue;
        if (quest.targetEnemyType >= 0 && quest.targetEnemyType != static_cast<int>(enemy.type)) continue;
        quest.progress = std::min(quest.targetCount, quest.progress + 1);
        if (quest.progress >= quest.targetCount) quest.status = QuestStatus::Complete;
    }
}

int SceneNarakuProto::CalculateReturnReward() const
{
    double reward = static_cast<double>(m_result.firstAreaCount * 150 + m_result.newRelicTypeCount * 150 + m_result.uniqueReward);
    for (int i = 0; i < 5; ++i)
    {
        const int depth = i + 1;
        reward += static_cast<double>(m_result.minedByDepth[i] * 5) * GetDepthRewardMultiplier(depth);
        reward += static_cast<double>(m_result.chargerKillsByDepth[i] * 10) * GetDepthRewardMultiplier(depth);
        reward += static_cast<double>(m_result.territoryKillsByDepth[i] * 20) * GetDepthRewardMultiplier(depth);
        reward += static_cast<double>(std::min(300.0f, m_result.staySecondsByDepth[i])) * GetDepthStayRewardMultiplier(depth);
    }
    return static_cast<int>(std::llround(reward));
}

bool SceneNarakuProto::IsShiftPress() const
{
    // 左Shiftまたは右Shiftが押されているかを直接確認します。
    if (IsActionDashStepPress()) return true;
    return false;
}

void SceneNarakuProto::StartDeath(const char* reason, DeathCause cause)
{
    CancelFishing(nullptr);
#if defined(NARAKU_EDITOR_BUILD)
    m_player.hp = GetMaxHp();
    m_player.mental = GetMaxMental();
    return;
#endif
    // すでに死亡リザルト中なら二重処理を防ぎます。
    if (m_mode == Mode::DeathResult)
    {
        const int extraLoss = std::max(0, GetDeathLevelLoss(cause) - GetDeathLevelLoss(m_pendingDeathCause));
        const float oldHp = GetMaxHp();
        const float oldStamina = GetMaxStamina();
        const float oldMental = GetMaxMental();
        int applicable = std::min(extraLoss, std::max(0, m_level - 1));
        const int protectedLevels = std::min(applicable, m_levelProtection);
        m_levelProtection -= protectedLevels;
        m_result.protectionConsumed += protectedLevels;
        applicable -= protectedLevels;
        m_level = std::max(1, m_level - applicable);
        m_result.levelAfterDeath = m_level;
        PreserveResourceRatios(oldHp, oldStamina, oldMental);
        if (extraLoss > 0) { m_pendingDeathCause = cause; m_result.reason = reason; SaveProgress(); }
        return;
    }

    const int deathDepth = GetCurrentDepth();
    FailActivePromotionQuest();
    ReturnCarriedLightsToStorage();

    // 死亡理由をリザルトに記録します。
    m_result.reason = reason;
    m_pendingDeathCause = cause;
    m_diedSinceLastDive = true;
    ApplyDeathPenalty(cause);

    // 死亡時に持っていた旧器数を記録します。保険対象なら選択確定まで別領域へ退避します。
    m_result.lostRelics = static_cast<int>(m_inventory.size());

    RestoreLostPropertyQuestTargetsAfterDeath();

    m_pendingDeathRecoveryRelics.clear();
    m_pendingDeathRecoveryBottles.clear();
    m_pendingDeathRecoveryCookingKits.clear();
    m_pendingDeathRecoveryFood = 0;
    m_pendingDeathRecoveryHeatedFood = 0;
    m_pendingDeathRecoveryRationOne = 0;
    m_pendingDeathRecoveryRawFish = 0;
    m_pendingDeathRecoveryCookedFish = 0;
    m_pendingDeathRecoveryRawSizedFish.fill(0);
    m_pendingDeathRecoveryCookedSizedFish.fill(0);
    m_pendingDeathRecoveryCartridges = 0;
    m_deathRecoveryPending = false;
    m_deathRecoveryDiscardConfirm = false;
    m_deathRecoveryDepth = deathDepth;
    m_deathRecoveryFee = 0;
    m_pendingDeathReason.clear();
    m_pendingDeathLevelBefore = m_result.levelBeforeDeath;
    m_pendingDeathLevelAfter = m_result.levelAfterDeath;
    m_pendingDeathProtectionConsumed = m_result.protectionConsumed;

    if (CanRecoverDeathInventory(deathDepth) &&
        (!m_inventory.empty() || m_foodCount > 0 || m_heatedFoodCount > 0 || m_rationOneCount > 0 ||
            m_rawFishCount > 0 || m_cookedFishCount > 0 || m_cartridgeCount > 0 ||
            std::accumulate(m_rawSizedFish.begin(), m_rawSizedFish.end(), 0) > 0 ||
            std::accumulate(m_cookedSizedFish.begin(), m_cookedSizedFish.end(), 0) > 0 ||
            !m_waterBottles.empty() || !m_cookingKits.empty()))
    {
        m_pendingDeathRecoveryRelics = m_inventory;
        for (RelicItem& item : m_pendingDeathRecoveryRelics)
        {
            item.stabilized = true;
        }
        m_pendingDeathRecoveryFood = m_foodCount;
        m_pendingDeathRecoveryHeatedFood = m_heatedFoodCount;
        m_pendingDeathRecoveryRationOne = m_rationOneCount;
        m_pendingDeathRecoveryRawFish = m_rawFishCount;
        m_pendingDeathRecoveryCookedFish = m_cookedFishCount;
        m_pendingDeathRecoveryRawSizedFish = m_rawSizedFish;
        m_pendingDeathRecoveryCookedSizedFish = m_cookedSizedFish;
        m_pendingDeathRecoveryCartridges = m_cartridgeCount;
        m_pendingDeathRecoveryBottles = m_waterBottles;
        for (WaterBottle& bottle : m_pendingDeathRecoveryBottles)
        {
            bottle.selectedForLoadout = false;
        }
        m_pendingDeathRecoveryCookingKits = m_cookingKits;
        for (CookingKit& kit : m_pendingDeathRecoveryCookingKits)
        {
            kit.selectedForLoadout = false;
        }

        const float feeRatio = deathDepth <= 3 ? 0.30f : (deathDepth == 4 ? 0.40f : 0.50f);
        m_deathRecoveryFee = std::max(1, static_cast<int>(std::lround(static_cast<double>(m_money) * feeRatio)));
        m_deathRecoveryPending = true;
        m_pendingDeathReason = reason;
        m_pendingDeathLevelBefore = m_result.levelBeforeDeath;
        m_pendingDeathLevelAfter = m_result.levelAfterDeath;
        m_pendingDeathProtectionConsumed = m_result.protectionConsumed;
    }

    // 死亡結果画面へ移すため、探索中の所持枠は空にします。
    m_inventory.clear();
    m_foodCount = 0;
    m_heatedFoodCount = 0;
    m_rationOneCount = 0;
    m_rawFishCount = 0;
    m_cookedFishCount = 0;
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_cartridgeCount = 0;
    m_waterBottles.clear();
    m_cookingKits.clear();
    m_portableLightOn = false;
    if (cause == DeathCause::Starvation) m_fullness = 50.0f;

    // 死亡リザルトモードへ移行します。
    CommitModeAfterSave(Mode::DeathResult, PresentationScene::Dive);
}

bool SceneNarakuProto::CanRecoverDeathInventory(int deathDepth) const
{
    return deathDepth >= 1 && deathDepth <= GetRankMaximumDepth(m_adventurerRank);
}

bool SceneNarakuProto::HasPendingDeathRecoveryItems() const
{
    return !m_pendingDeathRecoveryRelics.empty() || m_pendingDeathRecoveryFood > 0 ||
        m_pendingDeathRecoveryHeatedFood > 0 || m_pendingDeathRecoveryRationOne > 0 ||
        m_pendingDeathRecoveryRawFish > 0 || m_pendingDeathRecoveryCookedFish > 0 ||
        std::accumulate(m_pendingDeathRecoveryRawSizedFish.begin(), m_pendingDeathRecoveryRawSizedFish.end(), 0) > 0 ||
        std::accumulate(m_pendingDeathRecoveryCookedSizedFish.begin(), m_pendingDeathRecoveryCookedSizedFish.end(), 0) > 0 ||
        m_pendingDeathRecoveryCartridges > 0 || !m_pendingDeathRecoveryBottles.empty() ||
        !m_pendingDeathRecoveryCookingKits.empty();
}

void SceneNarakuProto::RestoreLostPropertyQuestTargetsAfterDeath()
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.type != QuestType::LostProperty || quest.status != QuestStatus::Complete ||
            !quest.targetInteracted)
        {
            continue;
        }

        quest.status = QuestStatus::Active;
        quest.progress = 0;
        quest.targetInteracted = false;
    }
}

void SceneNarakuProto::ResolveDeathRecovery(bool recover)
{
    if (!m_deathRecoveryPending)
    {
        return;
    }

    if (recover)
    {
        const int paid = std::min(m_money, m_deathRecoveryFee);
        m_money -= paid;
        m_questDebt += m_deathRecoveryFee - paid;

        m_storedInventory.insert(m_storedInventory.end(),
            m_pendingDeathRecoveryRelics.begin(), m_pendingDeathRecoveryRelics.end());
        m_storedFoodCount += m_pendingDeathRecoveryFood;
        m_storedHeatedFoodCount += m_pendingDeathRecoveryHeatedFood;
        m_storedRationOneCount += m_pendingDeathRecoveryRationOne;
        m_storedRawFishCount += m_pendingDeathRecoveryRawFish;
        m_storedCookedFishCount += m_pendingDeathRecoveryCookedFish;
        for (size_t i = 0; i < m_storedRawSizedFish.size(); ++i)
        {
            m_storedRawSizedFish[i] += m_pendingDeathRecoveryRawSizedFish[i];
            m_storedCookedSizedFish[i] += m_pendingDeathRecoveryCookedSizedFish[i];
        }
        m_storedCartridgeCount += m_pendingDeathRecoveryCartridges;
        m_storedWaterBottles.insert(m_storedWaterBottles.end(),
            m_pendingDeathRecoveryBottles.begin(), m_pendingDeathRecoveryBottles.end());
        m_storedCookingKits.insert(m_storedCookingKits.end(),
            m_pendingDeathRecoveryCookingKits.begin(), m_pendingDeathRecoveryCookingKits.end());
        ShowCenterNotification(m_deathRecoveryFee > paid
            ? u8"荷物を回収し、不足分を借金へ加算しました。"
            : u8"荷物を回収して自宅へ保管しました。");
        m_result.lostRelics = 0;
    }
    else
    {
        ShowCenterNotification(u8"回収を断り、荷物を破棄しました。");
    }

    m_pendingDeathRecoveryRelics.clear();
    m_pendingDeathRecoveryBottles.clear();
    m_pendingDeathRecoveryCookingKits.clear();
    m_pendingDeathRecoveryFood = 0;
    m_pendingDeathRecoveryHeatedFood = 0;
    m_pendingDeathRecoveryRationOne = 0;
    m_pendingDeathRecoveryRawFish = 0;
    m_pendingDeathRecoveryCookedFish = 0;
    m_pendingDeathRecoveryRawSizedFish.fill(0);
    m_pendingDeathRecoveryCookedSizedFish.fill(0);
    m_pendingDeathRecoveryCartridges = 0;
    m_deathRecoveryPending = false;
    m_deathRecoveryDiscardConfirm = false;
    m_deathRecoveryDepth = 0;
    m_deathRecoveryFee = 0;
    m_pendingDeathReason.clear();
    CommitModeAfterSave(Mode::DeathResult, PresentationScene::Result);
}

void SceneNarakuProto::FinishReturn()
{
    // 帰還理由をリザルトに記録します。
    m_result.reason = u8"生還しました。";

    // 持ち帰った旧器数を記録します。
    m_result.carriedRelics = static_cast<int>(m_inventory.size());

    // 商店で全売却した場合の参考額を集計します。
    m_result.saleAmount = 0;
    m_result.identifiedRelics = 0;
    UpdatePromotionQuestReturn(m_inventory);
    UpdateQuestReturnProgress(m_inventory);

    // 採掘時に決まっていた種類を公開し、遺物を自宅保管へ移します。
    for (const RelicItem& item : m_inventory)
    {
        const std::size_t index = static_cast<std::size_t>(item.type);
        if (!m_identifiedRelics[index])
        {
            m_identifiedRelics[index] = true;
            ++m_result.identifiedRelics;
        }
        RelicItem stored = item;
        stored.stabilized = true;
        m_storedInventory.push_back(stored);
        if (IsRelicSellable(stored)) m_result.saleAmount += stored.value;
        if (item.type == RelicType::Unique)
        {
            m_result.uniqueReward += 1000;
            m_uniqueRelicReturned = true;
            m_uniqueRelicCodexUnlocked = true;
            m_uniqueRelicAchievementUnlocked = true;
            m_uniqueRelicStoryUnlocked = true;
            UpdateImportantQuestUniqueReturn();
        }
    }
    m_result.newRelicTypeCount = m_result.identifiedRelics;
    m_result.explorationReward = CalculateReturnReward();
    const int debtPayment = std::min(m_questDebt, m_result.explorationReward);
    m_questDebt -= debtPayment;
    m_money += m_result.explorationReward - debtPayment;

    m_inventory.clear();

    // 使わずに持ち帰った食料も自宅へ戻します。
    m_storedFoodCount += m_foodCount;
    m_foodCount = 0;
    m_storedHeatedFoodCount += m_heatedFoodCount;
    m_heatedFoodCount = 0;
    m_storedWaterBottles.insert(m_storedWaterBottles.end(), m_waterBottles.begin(), m_waterBottles.end());
    m_waterBottles.clear();
    m_storedCookingKits.insert(m_storedCookingKits.end(), m_cookingKits.begin(), m_cookingKits.end());
    m_cookingKits.clear();
    m_storedRationOneCount += m_rationOneCount;
    m_rationOneCount = 0;
    m_storedRawFishCount += m_rawFishCount;
    m_rawFishCount = 0;
    m_storedCookedFishCount += m_cookedFishCount;
    m_cookedFishCount = 0;
    for (size_t i = 0; i < m_rawSizedFish.size(); ++i)
    {
        m_storedRawSizedFish[i] += m_rawSizedFish[i];
        m_storedCookedSizedFish[i] += m_cookedSizedFish[i];
    }
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_storedCartridgeCount += m_cartridgeCount;
    m_cartridgeCount = 0;
    ReturnCarriedLightsToStorage();

    // 生還した時は精神力を現在の最大値まで回復します。
    m_player.mental = GetMaxMental();

    // 帰還リザルトモードへ移行します。
    CommitModeAfterSave(Mode::ReturnResult, PresentationScene::Dive);
}

void SceneNarakuProto::StartDive(int targetDepth)
{
    if (m_screenFadePhase != ScreenFadePhase::None) return;
    targetDepth = std::max(1, std::min(3, targetDepth));
    if (m_loadoutFoodCount > m_storedFoodCount || m_loadoutHeatedFoodCount > m_storedHeatedFoodCount ||
        m_loadoutRationOneCount > m_storedRationOneCount || m_loadoutRawFishCount > m_storedRawFishCount ||
        m_loadoutCookedFishCount > m_storedCookedFishCount ||
        m_loadoutCartridgeCount > m_storedCartridgeCount)
    {
        AddMessage(u8"持ち込み予定数が自宅在庫を超えています。");
        return;
    }
    for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
    {
        if (m_loadoutRawSizedFish[i] > m_storedRawSizedFish[i] ||
            m_loadoutCookedSizedFish[i] > m_storedCookedSizedFish[i])
        { AddMessage(u8"持ち込み予定数が自宅在庫を超えています。"); return; }
    }
    float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount + m_loadoutHeatedFoodCount);
    loadoutWeight += static_cast<float>(m_loadoutRationOneCount) * kRationOneWeight;
    loadoutWeight += static_cast<float>(m_loadoutRawFishCount + m_loadoutCookedFishCount) * kUnknownFishWeight;
    for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
        loadoutWeight += static_cast<float>(m_loadoutRawSizedFish[i] + m_loadoutCookedSizedFish[i]) * kSizedFishWeights[i];
    loadoutWeight += static_cast<float>(m_loadoutCartridgeCount) * kCartridgeWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
        [](const WaterBottle& bottle) { return bottle.selectedForLoadout; })) * kWaterBottleWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedCookingKits.begin(), m_storedCookingKits.end(),
        [](const CookingKit& kit) { return kit.selectedForLoadout; })) * kCookingKitWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedPortableLights.begin(), m_storedPortableLights.end(),
        [](const PortableLight& light) { return light.selectedForLoadout; })) * kPortableLightWeight;
    for (std::size_t i = 0; i < m_loadoutRelics.size(); ++i)
    {
        if (m_loadoutRelics[i] > CountStoredRelics(static_cast<RelicType>(i)))
        {
            AddMessage(u8"持ち込み予定数が自宅在庫を超えています。");
            return;
        }
        loadoutWeight += GetRelicWeight(static_cast<RelicType>(i)) * static_cast<float>(m_loadoutRelics[i]);
    }
    if (loadoutWeight > GetMaxWeight())
    {
        AddMessage(u8"持ち込み重量が最大重量を超えているため潜行できません。");
        return;
    }

    if (targetDepth > 1)
    {
        const int directIndex = targetDepth - 2;
        const AdventurerRank requiredRank = targetDepth == 2 ? AdventurerRank::Black : AdventurerRank::Purple;
        const int fee = (targetDepth == 2 ? 500 : 2000) +
            250 * m_directGateWeeklyUses[static_cast<std::size_t>(directIndex)];
        if (static_cast<int>(m_adventurerRank) < static_cast<int>(requiredRank))
        {
            AddMessage(u8"現在の階級ではこの直通門を使用できません。");
            return;
        }
        if (m_money < fee)
        {
            AddMessage(u8"直通門の利用料金が不足しています。");
            return;
        }
    }

    m_pendingDiveDepth = targetDepth;
    BeginScreenFade(ScreenFadeAction::StartDive);
}

void SceneNarakuProto::CompleteStartDive()
{
    const PresentationScene sourceScene = GetPresentationScene();
    const float previousHp = m_player.hp;
    const float previousStamina = m_player.stamina;
    const float previousMental = m_player.mental;

    // フィールド側の一時状態を初期化してから、選択済みの持ち込み品を移します。
    if (!ResetRun())
    {
        BuildSurfaceRuntime(true);
        m_mode = Mode::AbyssEntrance;
        return;
    }
    if (m_pendingDiveDepth > 1)
    {
        const int directIndex = m_pendingDiveDepth - 2;
        int destinationArea = m_directGateTargetAreas[static_cast<std::size_t>(directIndex)];
        if (destinationArea < 0 || destinationArea >= static_cast<int>(m_areas.size()) ||
            m_areas[static_cast<std::size_t>(destinationArea)].depth != m_pendingDiveDepth ||
            m_areas[static_cast<std::size_t>(destinationArea)].sublayer != 0)
        {
            const auto destination = std::find_if(m_areas.begin(), m_areas.end(),
                [this](const AreaState& area) { return area.depth == m_pendingDiveDepth && area.sublayer == 0; });
            if (destination == m_areas.end())
            {
                ReportGenerationFailure(u8"直通門の到着先を選べませんでした。", "target upper sublayer is missing");
                BuildSurfaceRuntime(true);
                m_mode = Mode::AbyssEntrance;
                return;
            }
            destinationArea = static_cast<int>(std::distance(m_areas.begin(), destination));
            m_directGateTargetAreas[static_cast<std::size_t>(directIndex)] = destinationArea;
        }
        std::string generationError;
        if (!m_areas[static_cast<std::size_t>(destinationArea)].generated &&
            !GeneratePlannedArea(destinationArea, generationError))
        {
            ReportGenerationFailure(u8"直通門の到着先を生成できませんでした。", generationError);
            BuildSurfaceRuntime(true);
            m_mode = Mode::AbyssEntrance;
            return;
        }
        ActivateArea(destinationArea, false);
        bool foundSafeArrival = false;
        float bestDistanceSquared = std::numeric_limits<float>::max();
        Vec2 safeArrival = m_startPoint;
        float safeDepth = m_startDepth;
        for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
        {
            const float minX = layer.center.x - (layer.gridWidth - 1) * layer.cellSize * 0.5f;
            const float minZ = layer.center.z - (layer.gridHeight - 1) * layer.cellSize * 0.5f;
            for (int z = 0; z < layer.gridHeight - 2; ++z)
            {
                for (int x = 0; x < layer.gridWidth - 2; ++x)
                {
                    bool valid = true;
                    float minimumHeight = std::numeric_limits<float>::max();
                    float maximumHeight = std::numeric_limits<float>::lowest();
                    for (int dz = 0; dz < 2 && valid; ++dz)
                        for (int dx = 0; dx < 2; ++dx)
                        {
                            const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, x + dx, z + dz);
                            if ((flags & (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved |
                                NarakuMap::CellAttributeWaterLake)) != 0u) { valid = false; break; }
                            const float height = NarakuMap::GetVertexHeight(layer, x + dx, z + dz);
                            minimumHeight = std::min(minimumHeight, height);
                            maximumHeight = std::max(maximumHeight, height);
                        }
                    if (!valid || maximumHeight - minimumHeight > 0.20f) continue;
                    const Vec2 candidate = { minX + (x + 1.0f) * layer.cellSize, minZ + (z + 1.0f) * layer.cellSize };
                    const bool nearGate = std::any_of(m_layerGates.begin(), m_layerGates.end(),
                        [&](const LayerGateState& gate) { return Distance(candidate, gate.ropePos) < 3.0f || Distance(candidate, gate.loadPos) < 3.0f; });
                    const bool nearMining = std::any_of(m_miningPoints.begin(), m_miningPoints.end(),
                        [&](const MiningPoint& point) { return Distance(candidate, point.pos) < 2.5f; });
                    const bool nearEnemy = std::any_of(m_enemies.begin(), m_enemies.end(),
                        [&](const EnemyState& enemy) { return Distance(candidate, enemy.pos) < 4.0f; });
                    if (nearGate || nearMining || nearEnemy) continue;
                    const float distanceSquared = candidate.x * candidate.x + candidate.y * candidate.y;
                    if (distanceSquared < bestDistanceSquared)
                    {
                        bestDistanceSquared = distanceSquared;
                        safeArrival = candidate;
                        safeDepth = layer.layerDepth;
                        foundSafeArrival = true;
                    }
                }
            }
        }
        if (!foundSafeArrival)
        {
            for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
            {
                const float minX = layer.center.x - (layer.gridWidth - 1) * layer.cellSize * 0.5f;
                const float minZ = layer.center.z - (layer.gridHeight - 1) * layer.cellSize * 0.5f;
                for (int z = 0; z < layer.gridHeight - 1 && !foundSafeArrival; ++z)
                    for (int x = 0; x < layer.gridWidth - 1; ++x)
                    {
                        const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, x, z);
                        if ((flags & (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved |
                            NarakuMap::CellAttributeWaterLake)) != 0u) continue;
                        const Vec2 candidate = { minX + (x + 0.5f) * layer.cellSize, minZ + (z + 0.5f) * layer.cellSize };
                        const bool occupied = std::any_of(m_layerGates.begin(), m_layerGates.end(),
                            [&](const LayerGateState& gate) { return Distance(candidate, gate.ropePos) < 2.0f || Distance(candidate, gate.loadPos) < 2.0f; }) ||
                            std::any_of(m_miningPoints.begin(), m_miningPoints.end(),
                                [&](const MiningPoint& point) { return Distance(candidate, point.pos) < 2.0f; }) ||
                            std::any_of(m_enemies.begin(), m_enemies.end(),
                                [&](const EnemyState& enemy) { return Distance(candidate, enemy.pos) < 3.0f; });
                        if (occupied) continue;
                        safeArrival = candidate;
                        safeDepth = layer.layerDepth;
                        foundSafeArrival = true;
                        break;
                    }
                if (foundSafeArrival) break;
            }
        }
        if (!foundSafeArrival)
        {
            ReportGenerationFailure(u8"直通門の安全な到着地点を確保できませんでした。",
                "no flat walkable arrival cell away from enemies, mining points, water and layer gates");
            BuildSurfaceRuntime(true);
            m_mode = Mode::AbyssEntrance;
            return;
        }
        m_player.pos = safeArrival;
        m_player.depth = safeDepth;
        m_player.previousDepth = safeDepth;
        m_player.feetWorldY = GetGroundWorldY(safeArrival, safeDepth);
        m_player.previousWorldY = m_player.feetWorldY;
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.lastSafeGroundPos = safeArrival;
        m_player.lastSafeGroundDepth = safeDepth;
        m_player.hasSafeGroundPos = true;
        AreaState& arrivedArea = m_areas[static_cast<std::size_t>(destinationArea)];
        if (!arrivedArea.firstAreaExpAwarded)
        {
            arrivedArea.firstAreaExpAwarded = true;
            arrivedArea.firstAreaRewardAwarded = true;
            ++m_result.firstAreaCount;
            AwardExp(static_cast<int>(std::round(100.0f * GetDepthExpMultiplier(arrivedArea.depth))));
        }
        m_directGateTargetPositions[static_cast<std::size_t>(directIndex)] = m_player.pos;
        const int fee = (m_pendingDiveDepth == 2 ? 500 : 2000) +
            250 * m_directGateWeeklyUses[static_cast<std::size_t>(directIndex)];
        m_money -= fee;
        ++m_directGateWeeklyUses[static_cast<std::size_t>(directIndex)];
    }
    if (!m_diedSinceLastDive)
    {
        m_player.hp = std::min(GetMaxHp(), previousHp);
        m_player.stamina = std::min(GetMaxStamina(), previousStamina);
        m_player.mental = std::min(GetMaxMental(), previousMental);
    }
    m_diedSinceLastDive = false;
    m_foodCount = m_loadoutFoodCount;
    m_storedFoodCount -= m_loadoutFoodCount;
    m_loadoutFoodCount = 0;
    m_heatedFoodCount = m_loadoutHeatedFoodCount;
    m_storedHeatedFoodCount -= m_loadoutHeatedFoodCount;
    m_loadoutHeatedFoodCount = 0;
    m_rationOneCount = m_loadoutRationOneCount;
    m_storedRationOneCount -= m_loadoutRationOneCount;
    m_loadoutRationOneCount = 0;
    m_rawFishCount = m_loadoutRawFishCount;
    m_storedRawFishCount -= m_loadoutRawFishCount;
    m_loadoutRawFishCount = 0;
    m_cookedFishCount = m_loadoutCookedFishCount;
    m_storedCookedFishCount -= m_loadoutCookedFishCount;
    m_loadoutCookedFishCount = 0;
    for (size_t i = 0; i < m_rawSizedFish.size(); ++i)
    {
        m_rawSizedFish[i] = m_loadoutRawSizedFish[i];
        m_storedRawSizedFish[i] -= m_loadoutRawSizedFish[i];
        m_loadoutRawSizedFish[i] = 0;
        m_cookedSizedFish[i] = m_loadoutCookedSizedFish[i];
        m_storedCookedSizedFish[i] -= m_loadoutCookedSizedFish[i];
        m_loadoutCookedSizedFish[i] = 0;
    }
    m_cartridgeCount = m_loadoutCartridgeCount;
    m_storedCartridgeCount -= m_loadoutCartridgeCount;
    m_loadoutCartridgeCount = 0;

    for (auto it = m_storedWaterBottles.begin(); it != m_storedWaterBottles.end();)
    {
        if (it->selectedForLoadout)
        {
            m_waterBottles.push_back(*it);
            it = m_storedWaterBottles.erase(it);
        }
        else ++it;
    }
    for (auto it = m_storedCookingKits.begin(); it != m_storedCookingKits.end();)
    {
        if (it->selectedForLoadout)
        {
            m_cookingKits.push_back(*it);
            it = m_storedCookingKits.erase(it);
        }
        else ++it;
    }
    for (auto it = m_storedPortableLights.begin(); it != m_storedPortableLights.end();)
    {
        if (it->selectedForLoadout)
        {
            m_portableLights.push_back(*it);
            it = m_storedPortableLights.erase(it);
        }
        else ++it;
    }

    for (std::size_t i = 0; i < m_loadoutRelics.size(); ++i)
    {
        const RelicType type = static_cast<RelicType>(i);
        const int count = m_loadoutRelics[i];
        int remaining = count;
        for (auto it = m_storedInventory.begin(); it != m_storedInventory.end() && remaining > 0;)
        {
            if (it->type == type)
            {
                RelicItem item = *it;
                item.stabilized = true;
                m_inventory.push_back(item);
                it = m_storedInventory.erase(it);
                --remaining;
            }
            else ++it;
        }
        m_loadoutRelics[i] = 0;
    }
    if (CommitModeAfterSave(Mode::Explore, sourceScene))
    {
        UpdateImportantQuestArrival();
        SaveProgress();
    }
}

void SceneNarakuProto::RestartAfterDeath()
{
    // 死亡後は持ち込みなしで再挑戦します。
    m_loadoutFoodCount = 0;
    m_loadoutHeatedFoodCount = 0;
    m_loadoutRationOneCount = 0;
    m_loadoutRawFishCount = 0;
    m_loadoutCookedFishCount = 0;
    m_loadoutRawSizedFish.fill(0);
    m_loadoutCookedSizedFish.fill(0);
    m_loadoutCartridgeCount = 0;
    m_loadoutRelics.fill(0);
    for (WaterBottle& bottle : m_storedWaterBottles) bottle.selectedForLoadout = false;
    for (CookingKit& kit : m_storedCookingKits) kit.selectedForLoadout = false;
    StartDive();
}

void SceneNarakuProto::AbandonDive()
{
    if (m_mode == Mode::Home || m_mode == Mode::GeneralShop || m_mode == Mode::Armory || m_mode == Mode::Restaurant) return;
    FailActivePromotionQuest();
    ApplyAbandonPenalty();
    if (m_mode == Mode::RelicPrompt && m_pendingRelicMiningIndex >= 0)
    {
        m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true,
            m_pendingRelicMiningIndex });
        m_pendingRelicMiningIndex = -1;
    }
    m_result.reason = u8"探窟を放棄しました。";
    m_result.lostRelics = static_cast<int>(m_inventory.size());
    m_inventory.clear();
    m_foodCount = 0;
    m_heatedFoodCount = 0;
    m_rationOneCount = 0;
    m_rawFishCount = 0;
    m_cookedFishCount = 0;
    m_rawSizedFish.fill(0);
    m_cookedSizedFish.fill(0);
    m_cartridgeCount = 0;
    m_waterBottles.clear();
    m_cookingKits.clear();
    m_portableLights.clear();
    m_portableLightOn = false;
    CaptureWeeklyWorld();
    if (BuildSurfaceRuntime(false)) CommitModeAfterSave(Mode::Home, PresentationScene::Dive);
}

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
        std::string treeSource = name + ' ' + modelPath;
        std::transform(treeSource.begin(), treeSource.end(), treeSource.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        resource.isTree = treeSource.find("tree") != std::string::npos;
        if (model != nullptr && hasVertex)
        {
            resource.placementAnchor = {
                (minValue.x + maxValue.x) * 0.5f,
                minValue.y,
                (minValue.z + maxValue.z) * 0.5f };
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
            light.pointColorEnabled = { 0.90f, 0.90f, 0.90f, 0.90f };
            light.pointPositionRange = {
                -m_worldHalfSize * 1.25f,
                sideHeight,
                -m_worldHalfSize * 1.25f,
                std::max(1.0f, m_worldHalfSize * 3.0f) };
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
            const float range = std::max(1.0f, m_worldHalfSize * 3.0f);
            const float attenuation = std::max(0.0f,
                1.0f - std::sqrt(dx * dx + dy * dy + dz * dz) / range);
            const float side = 0.90f * attenuation * attenuation;
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

void SceneNarakuProto::DrawEnvironmentObjects(
    const DirectX::XMFLOAT4X4& view,
    const DirectX::XMFLOAT4X4& projection,
    const DirectX::XMFLOAT3& cameraPosition)
{
    using namespace DirectX;
    ShaderList::SetCameraPos(cameraPosition);
    ApplySceneLighting();

    for (const NarakuMap::EnvironmentObject& object : m_runtimeMap.environmentObjects)
    {
        const auto resourceIt = std::find_if(
            m_environmentModels.begin(),
            m_environmentModels.end(),
            [&](const EnvironmentModelResource& resource) { return resource.id == object.modelId; });
        if (resourceIt == m_environmentModels.end() || resourceIt->model == nullptr) continue;

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, object.layerId);
        if (layerIndex < 0) continue;
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
        const Vec2 position = { object.xz.x, object.xz.z };
        const float groundY = GetGroundWorldY(position, layer.layerDepth);

        XMFLOAT4X4 wvp[3] = {};
        XMStoreFloat4x4(&wvp[0], XMMatrixTranspose(
            XMMatrixTranslation(
                -resourceIt->placementAnchor.x,
                -resourceIt->placementAnchor.y,
                -resourceIt->placementAnchor.z) *
            XMMatrixScaling(object.scaleX, object.scaleY, object.scaleZ) *
            XMMatrixRotationY(XM_PIDIV2 * static_cast<float>(object.rotationQuarterTurns)) *
            XMMatrixTranslation(object.xz.x, groundY + object.offsetY, object.xz.z)));
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

void SceneNarakuProto::Draw3DField()
{
    using namespace DirectX;

    // プレイヤー位置を3D描画用の注視点に変換します。
    const float playerHeightOffset = (m_player.onRope && m_activeRope >= 0
        ? (m_player.feetWorldY - GetGroundWorldY(m_player.pos, m_player.depth))
        : GetPlayerAirborneOffset()) + m_layerTransitionVisualOffset;
    const XMFLOAT3 playerCenter = ToWorld3D(m_player.pos, m_player.depth, 0.7f + playerHeightOffset);

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
    XMFLOAT4X4 view;
    XMStoreFloat4x4(&view, XMMatrixTranspose(XMMatrixLookAtLH(eye, target, up)));
    Geometory::SetView(view);
    Sprite::SetView(view);

    // 画面比率を使って、デバッグ用の遠近投影を設定します。
    XMFLOAT4X4 projection;
    const float aspect = static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT);
    XMStoreFloat4x4(&projection, XMMatrixTranspose(XMMatrixPerspectiveFovLH(XMConvertToRadians(55.0f), aspect, 0.1f, 500.0f)));
    Geometory::SetProjection(projection);
    Sprite::SetProjection(projection);

    if (m_skyModel != nullptr)
    {
        XMFLOAT3 cameraPosition = {};
        XMStoreFloat3(&cameraPosition, eye);
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

    // 3Dデバッグ描画は深度テストを有効にして、前後関係を分かりやすくします。
    SetDepthTest(true);

    XMFLOAT3 cameraPosition = {};
    XMStoreFloat3(&cameraPosition, eye);
    SetCullingMode(D3D11_CULL_BACK);
    SetBlendMode(BLEND_NONE);
    DrawEnvironmentObjects(view, projection, cameraPosition);
    DrawBaseModels(view, projection, cameraPosition);
    DrawRopeModels(view, projection, cameraPosition);

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
        const XMFLOAT4 layerColor = applyGameplayLayerAlpha(layer, getLayerFloorColor(layer.groundTextureId));
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
                AppendTerrainFloorCell(
                    a, b, c, d,
                    GetTerrainVertexNormal(layer, cellX, cellZ),
                    GetTerrainVertexNormal(layer, cellX + 1, cellZ),
                    GetTerrainVertexNormal(layer, cellX, cellZ + 1),
                    GetTerrainVertexNormal(layer, cellX + 1, cellZ + 1),
                    layerColor,
                    layer.groundTextureId);

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

    // 帰還地点を緑の柱で示します。
    if (m_currentAreaIndex >= 0)
    {
        const XMFLOAT3 returnBase = ToWorld3D(m_returnPoint, m_returnDepth, 0.05f);
        DrawDebugBox3D({ returnBase.x, returnBase.y + 0.25f, returnBase.z }, { 0.9f, 0.5f, 0.9f });
    }

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

    // 採掘ポイントを箱で描画します。
    for (const MiningPoint& point : m_miningPoints)
    {
        // 未記録でも、近くまで来た採掘ポイントは現地で見えるようにします。
        const bool visibleInField = point.discovered || point.sensed || IsNear(m_player.pos, point.pos, kNearbyMiningVisibleRange);
        if (!visibleInField)
        {
            continue;
        }

        // 採掘ポイントの位置を深度0の地表として扱います。
        const XMFLOAT3 base = ToWorld3D(point.pos, point.depth, 0.15f);

        // 見た目4種類は箱の横幅だけ少し変えて区別します。
        const float width = 0.45f + 0.08f * static_cast<float>(point.visualType);
        DrawDebugBox3D({ base.x, base.y + 0.2f, base.z }, { width, 0.4f, width });

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

void SceneNarakuProto::DrawInventory()
{
    // 所持品ウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(160.0f, 80.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(620.0f, 700.0f), ImGuiCond_FirstUseEver);

    // 所持品ウィンドウを開始します。
    NarakuUi::Begin(u8"所持品", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(u8"← [LB] 地図", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Map; m_inventoryMapShowingMap = true; }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), u8"【 所持品 】");
    ImGui::SameLine();
    if (ImGui::Button(u8"設定 [RB] →", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Settings; m_inputSettings.OnOpen(); }
    ImGui::SameLine();
    ImGui::TextDisabled(u8" (LB/RB 切替, [B/Esc] 閉じる)");

    const float currentWeight = GetCurrentWeight();
    const float maxWeight = GetMaxWeight();
    ImGui::Text(u8"重量: %.0f / %.0f  %.0f%%", currentWeight, maxWeight, currentWeight / maxWeight * 100.0f);
    ImGui::Separator();

    // 所持旧器を一覧表示します。
    for (int i = 0; i < static_cast<int>(m_inventory.size()); ++i)
    {
        ImGui::PushID(i);

        // 表示対象の旧器を取得します。
        const RelicItem& item = m_inventory[i];

        // Selectableに渡す表示文字列を作ります。
        char label[128];

        // 未鑑定中は重量だけを見せ、売値は帰還鑑定まで伏せます。
        const std::size_t typeIndex = static_cast<std::size_t>(item.type);
        const bool identified = typeIndex < m_identifiedRelics.size() && m_identifiedRelics[typeIndex];
        if (identified)
        {
            if (item.type == RelicType::Offensive)
                std::snprintf(label, sizeof(label), u8"%s%s  重量 %.0f  使用 %d/%d", GetRelicDisplayName(item), item.broken ? u8"（破損）" : "", item.weight, item.remainingUses, item.maxUses);
            else
                std::snprintf(label, sizeof(label), u8"%s%s  重量 %.0f  売値 %d", GetRelicDisplayName(item), item.broken ? u8"（破損）" : "", item.weight, item.value);
        }
        else
        {
            std::snprintf(label, sizeof(label), u8"%s  重量 %.0f  売値 ?", GetRelicDisplayName(item), item.weight);
        }

        // クリックされた旧器を選択状態にします。
        if (ImGui::Selectable(label, m_selectedInventory == i)) m_selectedInventory = i;

        ImGui::PopID();
    }

    // 有効な旧器が選ばれている時だけ捨てるボタンを出します。
    if (m_selectedInventory >= 0 && m_selectedInventory < static_cast<int>(m_inventory.size()))
    {
        RelicItem& selected = m_inventory[m_selectedInventory];
        if (selected.type == RelicType::MentalRecovery && !selected.broken && ImGui::Button(u8"精神力を回復する"))
        {
            UseMentalRecoveryRelic(m_selectedInventory);
            m_selectedInventory = -1;
            ImGui::End();
            return;
        }
        else if (selected.type == RelicType::Survival)
        {
            if (ImGui::Checkbox(u8"致死時に自動発動", &selected.autoTrigger)) SaveProgress();
        }
        if (selected.type == RelicType::Unique)
            ImGui::TextWrapped(u8"不思議な力が込められている気がする");
        // 選択旧器を現在位置に捨てます。
        if (ImGui::Button(u8"選択した旧器を捨てる"))
        {
            // 所持品から地面旧器へ移します。
            DropInventoryItem(m_selectedInventory);

            // 削除後の添字ズレを避けるため選択を解除します。
            m_selectedInventory = -1;
        }
    }

    ImGui::Separator();
    ImGui::Text(u8"食料: %d（重量 %d）", m_foodCount, m_foodCount);
    if (m_overlayReturnMode != Mode::Surface && m_foodCount > 0 &&
        (m_player.hp < GetMaxHp() || m_fullness < kFullnessMaximum || m_hydration < kHydrationMaximum) &&
        ImGui::Button(u8"食料を使う（HP+20 / 満腹度+10 / 水分+75）"))
    {
        UseFood();
        if (m_foodUseTimer > 0.0f) m_mode = m_overlayReturnMode;
    }
    ImGui::Text(u8"加熱食料: %d（重量 %d）", m_heatedFoodCount, m_heatedFoodCount);
    if (m_overlayReturnMode != Mode::Surface && m_heatedFoodCount > 0 &&
        ImGui::Button(u8"加熱食料を使う（HP+40 / 満腹度+25 / 水分+75 / 精神力+5）"))
    {
        UseHeatedFood();
        if (m_foodUseTimer > 0.0f) m_mode = m_overlayReturnMode;
    }
    if (m_foodCount > 0 && !m_cookingKits.empty() && ImGui::Button(u8"食料を加熱する（60秒 / 料理セット1回）"))
    {
        StartCooking(CookingTarget::HeatFood);
        ImGui::End();
        return;
    }

    ImGui::Separator();
    ImGui::Text(u8"行動食1号: %d（重量 %.0f）", m_rationOneCount, m_rationOneCount * kRationOneWeight);
    if (m_overlayReturnMode != Mode::Surface && m_rationOneCount > 0 &&
        ImGui::Button(u8"行動食1号を使う（満腹度最大 / 5分間減少無効）"))
    {
        UseRationOne();
    }
    if (m_rationFullnessWardTimer > 0.0f)
        ImGui::TextDisabled(u8"満腹度減少無効: %.1f秒", m_rationFullnessWardTimer);
    if (m_rationHydrationPenaltyTimer > 0.0f)
        ImGui::TextDisabled(u8"水分消費1.25倍: %.1f秒", m_rationHydrationPenaltyTimer);

    ImGui::Text(u8"見たことない魚: 生 %d / 調理済み %d（重量 各%.0f）",
        m_rawFishCount, m_cookedFishCount, kUnknownFishWeight);
    if (m_overlayReturnMode != Mode::Surface && m_rawFishCount > 0 &&
        ImGui::Button(u8"魚を生で食べる（満腹度+10 / 精神力+5）")) UseRawFish();
    if (m_overlayReturnMode != Mode::Surface && m_cookedFishCount > 0 &&
        ImGui::Button(u8"調理済みの魚を食べる（満腹度+50 / 精神力+25）")) UseCookedFish();
    if (m_overlayReturnMode != Mode::Surface && m_rawFishCount > 0 && !m_cookingKits.empty() &&
        ImGui::Button(u8"魚を調理する（60秒 / 料理セット1回）"))
    {
        StartCooking(CookingTarget::CookFish);
        ImGui::End();
        return;
    }
    static const char* fishNames[] = { u8"魚（小）", u8"魚（中）", u8"魚（大）" };
    for (int size = 0; size < 3; ++size)
    {
        ImGui::PushID(3000 + size);
        ImGui::Text(u8"%s: 生 %d / 調理済み %d（重量 %.0f）", fishNames[size],
            m_rawSizedFish[static_cast<size_t>(size)], m_cookedSizedFish[static_cast<size_t>(size)], kSizedFishWeights[static_cast<size_t>(size)]);
        if (m_overlayReturnMode != Mode::Surface && m_rawSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"生で食べる"))
            UseSizedFish(static_cast<FishSize>(size), false);
        ImGui::SameLine();
        if (m_overlayReturnMode != Mode::Surface && m_cookedSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"調理済みを食べる"))
            UseSizedFish(static_cast<FishSize>(size), true);
        ImGui::SameLine();
        if (m_overlayReturnMode != Mode::Surface && m_rawSizedFish[static_cast<size_t>(size)] > 0 && !m_cookingKits.empty() && ImGui::SmallButton(u8"調理する"))
        {
            m_cookingFishSize = size;
            StartCooking(CookingTarget::CookSizedFish);
            ImGui::PopID(); ImGui::End(); return;
        }
        ImGui::PopID();
    }
    ImGui::Text(u8"カートリッジ: %d（重量 %.0f）", m_cartridgeCount,
        m_cartridgeCount * kCartridgeWeight);

    ImGui::Separator();
    ImGui::Text(u8"水分: %.1f / 100", m_hydration);
    for (int i = 0; i < static_cast<int>(m_waterBottles.size()); ++i)
    {
        WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(i)];
        ImGui::PushID(1000 + i);
        ImGui::Text(u8"水筒%d: %.0f/100（%s）", i + 1, bottle.amount, GetWaterQualityName(bottle.quality));
        if (bottle.amount > 0.0f && m_hydration < kHydrationMaximum && ImGui::SmallButton(u8"25飲む"))
            DrinkFromBottle(i);
        if (bottle.amount > 0.0f)
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"全量捨てる")) DiscardBottleWater(i);
        }
        if (bottle.amount > 0.0f && bottle.quality == WaterQuality::Unboiled && !m_cookingKits.empty())
        {
            ImGui::SameLine();
            if (ImGui::SmallButton(u8"煮沸する"))
            {
                StartCooking(CookingTarget::BoilBottle, i);
                ImGui::PopID();
                ImGui::End();
                return;
            }
        }
        ImGui::PopID();
    }
    ImGui::Text(u8"料理セット: %d個", static_cast<int>(m_cookingKits.size()));
    for (int i = 0; i < static_cast<int>(m_cookingKits.size()); ++i)
        ImGui::TextDisabled(u8"  %d: 残り%d/5回", i + 1, m_cookingKits[static_cast<std::size_t>(i)].remainingUses);

    ImGui::Separator();
    auto& accessibleLights = GetAccessibleLights();
    ImGui::Text(u8"ライト: %d個 / %s", static_cast<int>(accessibleLights.size()),
        m_portableLightOn ? u8"点灯中" : u8"消灯中");
    ImGui::BeginDisabled(!m_portableLightOn && !IsPortableLightAvailable());
    if (ImGui::Button(m_portableLightOn ? u8"ライトを消灯" : u8"ライトを点灯")) TogglePortableLight();
    ImGui::EndDisabled();
    for (int i = 0; i < static_cast<int>(accessibleLights.size()); ++i)
    {
        PortableLight& light = accessibleLights[static_cast<std::size_t>(i)];
        ImGui::PushID(5000 + i);
        if (light.broken)
            ImGui::Text(u8"ライト%d（破損 / 重量5 / 売値5G）", i + 1);
        else
            ImGui::Text(u8"ライト%d（残り %.0f:%02.0f / 重量5）", i + 1,
                std::floor(light.remainingSeconds / 60.0f), std::fmod(light.remainingSeconds, 60.0f));
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"捨てる"))
        {
            accessibleLights.erase(accessibleLights.begin() + i);
            if (!IsPortableLightAvailable()) m_portableLightOn = false;
            if (m_overlayReturnMode == Mode::Surface) SaveProgress();
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (ImGui::Button(u8"探窟を放棄して自宅へ戻る")) m_mode = Mode::AbandonConfirm;

    // 所持品ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawRelicPrompt()
{
    // メインウィンドウの作業領域中央へ、確認ウィンドウ自身の中央を合わせます。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));

    // 旧器確認ウィンドウのサイズを固定します。
    ImGui::SetNextWindowSize(ImVec2(420.0f, 180.0f), ImGuiCond_Always);

    // 旧器確認ウィンドウを開始します。
    NarakuUi::Begin(u8"旧器を発見", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);

    // 発見した旧器名を表示します。
    ImGui::Text(u8"%s を発見", GetRelicDisplayName(m_pendingRelic));

    // 発見した旧器の重量を表示します。
    ImGui::Text(u8"重量: %.0f", m_pendingRelic.weight);
    ImGui::Text(u8"活性度: +%d（拾得後 %d）", GetRelicActivity(m_pendingRelic), GetCurrentActivity() + GetRelicActivity(m_pendingRelic));

    // 拾った後の総重量を表示します。
    ImGui::Text(u8"拾得後重量: %.0f / %.0f", GetCurrentWeight() + m_pendingRelic.weight, GetPickupWeightLimit());

    // 150%上限を超えない場合だけ拾うボタンを出します。
    if (GetCurrentWeight() + m_pendingRelic.weight <= GetPickupWeightLimit())
    {
        // 旧器を所持品に入れます。
        if (ImGui::Button(u8"拾う"))
        {
            // 発見中の旧器を所持品へ追加します。
            m_inventory.push_back(m_pendingRelic);
            if (m_pendingRelicMiningIndex >= 0 &&
                m_pendingRelicMiningIndex < static_cast<int>(m_miningPoints.size()))
            {
                m_miningPoints[static_cast<std::size_t>(m_pendingRelicMiningIndex)].outputPending = false;
            }
            m_pendingRelicMiningIndex = -1;

            // 探索モードへ戻ります。
            m_mode = Mode::Explore;

            // HUDログに取得を出します。
            AddMessage(u8"旧器を拾いました。");
            SaveProgress();
        }

        // Leaveボタンを横並びにします。
        ImGui::SameLine();
    }

    // 150%上限を超える場合は拾えない理由を表示します。
    else
    {
        // 重量上限超過メッセージを表示します。
        ImGui::Text(u8"これ以上は重すぎて拾えません。");
    }

    // 旧器を拾わず地面に置いたままにします。
    if (NarakuUi::BackButton(u8"置いていく"))
    {
        // 発見中の旧器を地面旧器として再登録します。
        m_groundRelics.push_back({ m_pendingRelic, m_pendingRelicPos, m_pendingRelicDepth, true,
            m_pendingRelicMiningIndex });
        m_pendingRelicMiningIndex = -1;

        // 探索モードへ戻ります。
        m_mode = Mode::Explore;

        // HUDログに放置を出します。
        AddMessage(u8"旧器をその場に置きました。");
        SaveProgress();
    }

    // 旧器確認ウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::DrawReturnConfirm()
{
    ImGui::SetNextWindowPos(ImVec2(460.0f, 230.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(400.0f, 170.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"帰還確認", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"現在の持ち物を持って帰還します。帰還後、未鑑定の遺物は自動で鑑定されます。");
    if (ImGui::Button(u8"帰還する", ImVec2(130.0f, 0.0f))) FinishReturn();
    ImGui::SameLine();
    if (NarakuUi::BackButton(u8"探索を続ける", ImVec2(130.0f, 0.0f))) m_mode = Mode::Explore;
    ImGui::End();
}

void SceneNarakuProto::DrawWaterPrompt()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(430.0f, 190.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"水場", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    const char* waterType = (m_pendingWaterFlags & NarakuMap::CellAttributeWaterLake) != 0u
        ? u8"湖" : ((m_pendingWaterFlags & NarakuMap::CellAttributeWaterPond) != 0u ? u8"池" : u8"溜り");
    ImGui::Text(u8"水場区分: %s", waterType);
    ImGui::Text(u8"現在の水分: %.1f / 100", m_hydration);
    ImGui::TextDisabled(u8"奈落の水は未煮沸です。直接飲むと食中毒の可能性があります。");

    if (m_hydration < kHydrationMaximum && ImGui::Button(u8"直接飲む（水分+25）", ImVec2(190.0f, 0.0f)))
    {
        DrinkWater(kWaterDrinkAmount, WaterQuality::Unboiled, GetWaterFoodPoisoningChance());
        m_mode = Mode::Explore;
    }

    const auto emptyBottle = std::find_if(m_waterBottles.begin(), m_waterBottles.end(),
        [](const WaterBottle& bottle) { return bottle.amount <= 0.0f; });
    if (emptyBottle != m_waterBottles.end() && ImGui::Button(u8"空の水筒へ汲む（100）", ImVec2(190.0f, 0.0f)))
    {
        emptyBottle->amount = kWaterBottleCapacity;
        emptyBottle->quality = WaterQuality::Unboiled;
        emptyBottle->foodPoisoningChance = GetWaterFoodPoisoningChance();
        AddMessage(u8"空の水筒へ未煮沸の水を汲みました。");
        m_mode = Mode::Explore;
    }
    if (emptyBottle == m_waterBottles.end()) ImGui::TextDisabled(u8"空の水筒がありません。");

    if (NarakuUi::BackButton(u8"戻る")) m_mode = Mode::Explore;
    ImGui::End();
}

void SceneNarakuProto::DrawAbandonConfirm()
{
    ImGui::SetNextWindowPos(ImVec2(440.0f, 220.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(440.0f, 190.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"探窟放棄の確認", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::TextWrapped(u8"今回持ち込んだ物と取得物、帰還報酬を失い、レベル進行が0.5戻ります。満腹度と水分は維持されます。");
    if (ImGui::Button(u8"放棄する", ImVec2(130.0f, 0.0f))) AbandonDive();
    ImGui::SameLine();
    if (NarakuUi::BackButton(u8"戻る", ImVec2(130.0f, 0.0f))) m_mode = Mode::Inventory;
    ImGui::End();
}

void SceneNarakuProto::DrawCurrentStatus()
{
    if (!ImGui::CollapsingHeader(u8"現在のステータス", ImGuiTreeNodeFlags_DefaultOpen)) return;

    const EquipmentBonus equipment = GetEquipmentBonus();
    ImGui::Text(u8"装備  頭:%s / 胴:%s / 武器:%s", GetArmorName(m_equippedHeadArmor),
        GetArmorName(m_equippedBodyArmor), GetWeaponName(m_equippedWeapon));
    ImGui::Text(u8"Lv%d  HP %.0f / %.0f", m_level, m_player.hp, GetMaxHp());
    ImGui::Text(u8"スタミナ %.0f / %.0f  精神力 %.0f / %.0f", m_player.stamina, GetMaxStamina(), m_player.mental, GetMaxMental());
    ImGui::Text(u8"満腹度 %.0f / 100  水分 %.0f / 100  重量 %.0f / %.0f", m_fullness, m_hydration, GetCurrentWeight(), GetMaxWeight());
    ImGui::Text(u8"攻撃力 %.2f  防御倍率 %.0f%%", GetAttackPower(), GetDefenseMultiplier() * 100.0f);
    ImGui::Text(u8"歩行 %.2fm/s  走行 %.2fm/s", GetMoveSpeed(), GetRunSpeed());
    ImGui::Text(u8"スタミナ回復 %.2f/s  精神遺物回復 %.2f", m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier(),
        10.0f * GetMentalRecoveryMultiplier());
    ImGui::Text(u8"採掘速度 %.0f%%  ロープ上昇 %.2f / 降下 %.2f",
        GetMiningSpeedMultiplier() * 100.0f, GetRopeSpeed(true), GetRopeSpeed(false));
    ImGui::Text(u8"装備HP回復 %.2f/s", equipment.hpRecoveryPerSecond);
}

void SceneNarakuProto::DrawTownNavigation()
{
    ImGui::Separator();
    ImGui::TextUnformatted(u8"地上施設");
    const auto drawDestination = [this](const char* label, Mode destination)
    {
        const bool locked = destination == Mode::QuestDesk && !IsQuestDeskUnlocked();
        ImGui::BeginDisabled(m_mode == destination || locked);
        if (ImGui::Button(label, ImVec2(120.0f, 0.0f))) m_mode = destination;
        ImGui::EndDisabled();
        if (locked && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(u8"第一層上層へ入ると解放されます。");
    };
    drawDestination(u8"自宅", Mode::Home);
    ImGui::SameLine();
    drawDestination(u8"商店", Mode::GeneralShop);
    ImGui::SameLine();
    drawDestination(u8"武具屋", Mode::Armory);
    ImGui::SameLine();
    drawDestination(u8"レストラン", Mode::Restaurant);
    ImGui::SameLine();
    drawDestination(u8"依頼受付", Mode::QuestDesk);
}

void SceneNarakuProto::DrawTownFrame(
    const char* title,
    const std::function<void()>& drawCenterContent,
    Mode returnMode,
    const char* returnLabel)
{
    // ###TownMainWindow を指定することで、タイトルバーの施設名は切り替えつつ、
    // ImGui内部のウィンドウIDを共通化して場所切り替え時の位置・サイズ変動を防止する
    char windowTitleWithId[256];
    std::snprintf(windowTitleWithId, sizeof(windowTitleWithId), "%s###TownMainWindow",
        (title != nullptr && title[0] != '\0') ? title : u8"拠点");

    ImGui::SetNextWindowPos(ImVec2(100.0f, 40.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(1180.0f, 760.0f), ImGuiCond_FirstUseEver);
    if (!NarakuUi::Begin(windowTitleWithId, nullptr, ImGuiWindowFlags_NoCollapse))
    {
        ImGui::End();
        return;
    }

    if (NarakuUi::BackButton(returnLabel, ImVec2(140.0f, 0.0f))) m_mode = returnMode;
    if (m_mode == Mode::Restaurant || m_mode == Mode::QuestDesk)
    {
        ImGui::SameLine();
        ImGui::BeginDisabled(m_mode == Mode::Restaurant && !IsQuestDeskUnlocked());
        if (ImGui::Button(m_mode == Mode::Restaurant ? u8"依頼受付タブ" : u8"レストランタブ",
            ImVec2(150.0f, 0.0f)))
            m_mode = m_mode == Mode::Restaurant ? Mode::QuestDesk : Mode::Restaurant;
        ImGui::EndDisabled();
        if (m_mode == Mode::Restaurant && !IsQuestDeskUnlocked() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(u8"第一層上層へ到達すると解放されます。");
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("Town3PaneTable", 3, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("LeftPane", ImGuiTableColumnFlags_WidthStretch, 0.30f);
        ImGui::TableSetupColumn("CenterPane", ImGuiTableColumnFlags_WidthStretch, 0.40f);
        ImGui::TableSetupColumn("RightPane", ImGuiTableColumnFlags_WidthStretch, 0.30f);

        ImGui::TableNextRow();

        // 左ペイン (30%): 装備情報・次回持ち込み品
        ImGui::TableSetColumnIndex(0);
        DrawTownLeftPane();

        // 中央ペイン (40%): 各施設情報
        ImGui::TableSetColumnIndex(1);
        if (drawCenterContent)
        {
            drawCenterContent();
        }

        // 右ペイン (30%): プレイヤーステータス表
        ImGui::TableSetColumnIndex(2);
        DrawTownRightPane();

        ImGui::EndTable();
    }

    ImGui::End();

    DrawShopTransactionModal();
    DrawArmoryPurchaseModal();
}

void SceneNarakuProto::DrawTownLeftPane()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【装備情報】");
    ImGui::Separator();
    ImGui::Text(u8"武器: %s", GetWeaponName(m_equippedWeapon));
    ImGui::Text(u8"頭  : %s", GetArmorName(m_equippedHeadArmor));
    ImGui::Text(u8"胴体: %s", GetArmorName(m_equippedBodyArmor));
    if (HasRelicArmorSetEffect())
    {
        ImGui::TextColored(ImVec4(0.75f, 0.55f, 1.0f, 1.0f), u8"※遺物装備セット発動中");
    }
    if (HasUnknownArmorSetEffect())
    {
        ImGui::TextColored(ImVec4(0.35f, 1.0f, 0.65f, 1.0f), u8"※未知の装備セット発動中");
    }

    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【次回持ち込みアイテム】");
    ImGui::Separator();

    if (ImGui::BeginChild("TownLoadoutItemsList", ImVec2(0.0f, 320.0f), true))
    {
        bool hasItem = false;
        if (m_loadoutFoodCount > 0)
        {
            ImGui::BulletText(u8"食料 x %d", m_loadoutFoodCount);
            hasItem = true;
        }
        if (m_loadoutHeatedFoodCount > 0)
        {
            ImGui::BulletText(u8"加熱食料 x %d", m_loadoutHeatedFoodCount);
            hasItem = true;
        }
        if (m_loadoutRationOneCount > 0) { ImGui::BulletText(u8"行動食1号 x %d", m_loadoutRationOneCount); hasItem = true; }
        if (m_loadoutRawFishCount > 0) { ImGui::BulletText(u8"見たことない魚 x %d", m_loadoutRawFishCount); hasItem = true; }
        if (m_loadoutCookedFishCount > 0) { ImGui::BulletText(u8"調理済みの魚 x %d", m_loadoutCookedFishCount); hasItem = true; }
        if (m_loadoutCartridgeCount > 0) { ImGui::BulletText(u8"カートリッジ x %d", m_loadoutCartridgeCount); hasItem = true; }
        for (std::size_t i = 0; i < m_storedWaterBottles.size(); ++i)
        {
            const auto& bottle = m_storedWaterBottles[i];
            if (bottle.selectedForLoadout)
            {
                ImGui::BulletText(u8"水筒%d (水量%.0f, %s)", static_cast<int>(i + 1), bottle.amount, GetWaterQualityName(bottle.quality));
                hasItem = true;
            }
        }
        for (std::size_t i = 0; i < m_storedCookingKits.size(); ++i)
        {
            const auto& kit = m_storedCookingKits[i];
            if (kit.selectedForLoadout)
            {
                ImGui::BulletText(u8"料理セット%d (残%d回)", static_cast<int>(i + 1), kit.remainingUses);
                hasItem = true;
            }
        }
        for (std::size_t i = 0; i < m_storedPortableLights.size(); ++i)
        {
            const auto& light = m_storedPortableLights[i];
            if (light.selectedForLoadout)
            {
                ImGui::BulletText(u8"ライト%d (%s, 残%.0f秒)", static_cast<int>(i + 1),
                    light.broken ? u8"破損" : u8"使用可", light.remainingSeconds);
                hasItem = true;
            }
        }
        for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
        {
            if (m_loadoutRelics[i] > 0)
            {
                ImGui::BulletText(u8"%s x %d", GetRelicTypeName(static_cast<RelicType>(i)), m_loadoutRelics[i]);
                hasItem = true;
            }
        }
        if (!hasItem)
        {
            ImGui::TextDisabled(u8"（持ち込みアイテムなし）");
        }
    }
    ImGui::EndChild();

    float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount + m_loadoutHeatedFoodCount);
    loadoutWeight += static_cast<float>(m_loadoutRationOneCount) * kRationOneWeight;
    loadoutWeight += static_cast<float>(m_loadoutRawFishCount + m_loadoutCookedFishCount) * kUnknownFishWeight;
    for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
        loadoutWeight += static_cast<float>(m_loadoutRawSizedFish[i] + m_loadoutCookedSizedFish[i]) * kSizedFishWeights[i];
    loadoutWeight += static_cast<float>(m_loadoutCartridgeCount) * kCartridgeWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
        [](const WaterBottle& b) { return b.selectedForLoadout; })) * kWaterBottleWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedCookingKits.begin(), m_storedCookingKits.end(),
        [](const CookingKit& k) { return k.selectedForLoadout; })) * kCookingKitWeight;
    loadoutWeight += static_cast<float>(std::count_if(m_storedPortableLights.begin(), m_storedPortableLights.end(),
        [](const PortableLight& light) { return light.selectedForLoadout; })) * kPortableLightWeight;
    for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
    {
        loadoutWeight += GetRelicWeight(static_cast<RelicType>(i)) * static_cast<float>(m_loadoutRelics[i]);
    }
    const float maxWeight = GetMaxWeight();
    ImGui::Spacing();
    ImGui::Text(u8"開始重量: %.0f / %.0f", loadoutWeight, maxWeight);
    if (loadoutWeight > maxWeight)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.30f, 1.0f), u8"※重量制限オーバー");
    }
}

void SceneNarakuProto::DrawTownRightPane()
{
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【プレイヤー情報】");
    ImGui::Separator();

    if (ImGui::BeginTable("PlayerStatusTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn(u8"項目", ImGuiTableColumnFlags_WidthFixed, 140.0f);
        ImGui::TableSetupColumn(u8"内容", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto addRow = [](const char* label, const char* value) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(value);
        };

        char buf[128];

        std::snprintf(buf, sizeof(buf), "%d", m_level);
        addRow(u8"Lv", buf);

        addRow(u8"階級", GetRankName(m_adventurerRank));

        std::snprintf(buf, sizeof(buf), "%d G", m_money);
        addRow(u8"所持金", buf);

        std::snprintf(buf, sizeof(buf), "%.0f (%.0f)", GetMaxHp(), m_player.hp);
        addRow(u8"最大HP(現在HP)", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", GetMaxStamina());
        addRow(u8"最大スタミナ", buf);

        std::snprintf(buf, sizeof(buf), "%.0f (%.0f)", GetMaxMental(), m_player.mental);
        addRow(u8"最大精神力(現在精神力)", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", GetMaxWeight());
        addRow(u8"最大重量", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", m_fullness);
        addRow(u8"満腹度", buf);

        std::snprintf(buf, sizeof(buf), "%.0f", m_hydration);
        addRow(u8"水分", buf);

        std::snprintf(buf, sizeof(buf), "%.1f", GetAttackPower());
        addRow(u8"攻撃力", buf);

        std::snprintf(buf, sizeof(buf), "%.0f%%", GetDefenseMultiplier() * 100.0f);
        addRow(u8"防御力", buf);

        std::snprintf(buf, sizeof(buf), "%.1f /s", m_debugPlayerParams.staminaRecoverPerSecond * GetStaminaRecoveryMultiplier());
        addRow(u8"スタミナ回復速度", buf);

        std::snprintf(buf, sizeof(buf), "%.0f%%", GetMiningSpeedMultiplier() * 100.0f);
        addRow(u8"採掘速度", buf);

        std::snprintf(buf, sizeof(buf), "%.2fm/s : %.2fm/s", GetRopeSpeed(true), GetRopeSpeed(false));
        addRow(u8"ロープ昇降速度", buf);

        ImGui::EndTable();
    }

    ImGui::Spacing();
    static const char* kWeekdayNames[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
    const int weekSecond = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
    const int day = weekSecond / static_cast<int>(kGameDaySeconds);
    const int daySecond = weekSecond % static_cast<int>(kGameDaySeconds);
    const int hour = daySecond / 3600;
    const int minute = (daySecond % 3600) / 60;

    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【日時】");
    ImGui::SameLine();
    ImGui::Text(u8"%s曜日 %02d:%02d", kWeekdayNames[day], hour, minute);
    if (m_weekResetPending)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"※次回潜行時に新しい週へ更新されます");
    }
}

void SceneNarakuProto::DrawHome()
{
    DrawTownFrame(u8"自宅 - 潜行準備", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        if (m_uniqueRelicReturned)
            ImGui::TextWrapped(u8"欲望の揺籃: 図鑑登録・実績解除済み / 物語の進行条件を達成");
        ImGui::Separator();

        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【装備選択】");
        ImGui::Text(u8"頭装備:");
        for (std::size_t i = 0; i < m_ownedHeadArmor.size(); ++i)
        {
            if (!m_ownedHeadArmor[i]) continue;
            const ArmorTier tier = static_cast<ArmorTier>(i);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::RadioButton(GetArmorName(tier), m_equippedHeadArmor == tier)) { m_equippedHeadArmor = tier; SaveProgress(); }
            ImGui::SameLine();
            ImGui::TextDisabled(u8"%s", GetArmorEffectText(tier, true));
            ImGui::PopID();
        }

        ImGui::Text(u8"胴装備:");
        for (std::size_t i = 0; i < m_ownedBodyArmor.size(); ++i)
        {
            if (!m_ownedBodyArmor[i]) continue;
            const ArmorTier tier = static_cast<ArmorTier>(i);
            ImGui::PushID(100 + static_cast<int>(i));
            if (ImGui::RadioButton(GetArmorName(tier), m_equippedBodyArmor == tier)) { m_equippedBodyArmor = tier; SaveProgress(); }
            ImGui::SameLine();
            ImGui::TextDisabled(u8"%s", GetArmorEffectText(tier, false));
            ImGui::PopID();
        }

        ImGui::Text(u8"武器:");
        for (std::size_t i = 0; i < m_ownedWeapons.size(); ++i)
        {
            if (!m_ownedWeapons[i]) continue;
            const WeaponTier tier = static_cast<WeaponTier>(i);
            ImGui::PushID(200 + static_cast<int>(i));
            if (ImGui::RadioButton(GetWeaponName(tier), m_equippedWeapon == tier)) { m_equippedWeapon = tier; SaveProgress(); }
            ImGui::SameLine();
            ImGui::TextDisabled(u8"%s", GetWeaponEffectText(tier));
            ImGui::PopID();
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【持ち込み品調整】");
        ImGui::Text(u8"食料  保管:%d  持込:%d", m_storedFoodCount, m_loadoutFoodCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"-##food")) { m_loadoutFoodCount = std::max(0, m_loadoutFoodCount - 1); SaveProgress(); }
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"+##food")) { m_loadoutFoodCount = std::min(m_storedFoodCount, m_loadoutFoodCount + 1); SaveProgress(); }

        ImGui::Text(u8"加熱食料  保管:%d  持込:%d", m_storedHeatedFoodCount, m_loadoutHeatedFoodCount);
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"-##heatedFood")) { m_loadoutHeatedFoodCount = std::max(0, m_loadoutHeatedFoodCount - 1); SaveProgress(); }
        ImGui::SameLine();
        if (ImGui::SmallButton(u8"+##heatedFood")) { m_loadoutHeatedFoodCount = std::min(m_storedHeatedFoodCount, m_loadoutHeatedFoodCount + 1); SaveProgress(); }

        auto drawCountLoadout = [this](const char* label, const char* id, int stored, int& loadout)
        {
            ImGui::Text(u8"%s  保管:%d  持込:%d", label, stored, loadout);
            ImGui::SameLine();
            std::string minus = std::string("-##") + id;
            if (ImGui::SmallButton(minus.c_str())) { loadout = std::max(0, loadout - 1); SaveProgress(); }
            ImGui::SameLine();
            std::string plus = std::string("+##") + id;
            if (ImGui::SmallButton(plus.c_str())) { loadout = std::min(stored, loadout + 1); SaveProgress(); }
        };
        drawCountLoadout(u8"行動食1号", "rationOne", m_storedRationOneCount, m_loadoutRationOneCount);
        drawCountLoadout(u8"見たことない魚", "rawFish", m_storedRawFishCount, m_loadoutRawFishCount);
        drawCountLoadout(u8"調理済みの魚", "cookedFish", m_storedCookedFishCount, m_loadoutCookedFishCount);
        static const char* sizedFishNames[] = { u8"魚（小）", u8"魚（中）", u8"魚（大）" };
        for (int size = 0; size < 3; ++size)
        {
            const std::string rawId = "rawSizedFish" + std::to_string(size);
            const std::string cookedId = "cookedSizedFish" + std::to_string(size);
            drawCountLoadout((std::string(sizedFishNames[size]) + u8" 生").c_str(), rawId.c_str(),
                m_storedRawSizedFish[static_cast<size_t>(size)], m_loadoutRawSizedFish[static_cast<size_t>(size)]);
            drawCountLoadout((std::string(sizedFishNames[size]) + u8" 調理済み").c_str(), cookedId.c_str(),
                m_storedCookedSizedFish[static_cast<size_t>(size)], m_loadoutCookedSizedFish[static_cast<size_t>(size)]);
        }
        drawCountLoadout(u8"カートリッジ", "cartridge", m_storedCartridgeCount, m_loadoutCartridgeCount);

        for (int i = 0; i < static_cast<int>(m_storedWaterBottles.size()); ++i)
        {
            WaterBottle& bottle = m_storedWaterBottles[static_cast<std::size_t>(i)];
            ImGui::PushID(700 + i);
            if (ImGui::Checkbox(u8"##bottleLoadout", &bottle.selectedForLoadout)) SaveProgress();
            ImGui::SameLine();
            ImGui::Text(u8"水筒%d %.0f/100（%s） 重量15", i + 1, bottle.amount, GetWaterQualityName(bottle.quality));
            ImGui::PopID();
        }
        for (int i = 0; i < static_cast<int>(m_storedCookingKits.size()); ++i)
        {
            CookingKit& kit = m_storedCookingKits[static_cast<std::size_t>(i)];
            ImGui::PushID(800 + i);
            if (ImGui::Checkbox(u8"##kitLoadout", &kit.selectedForLoadout)) SaveProgress();
            ImGui::SameLine();
            ImGui::Text(u8"料理セット%d 残り%d/5回 重量5", i + 1, kit.remainingUses);
            ImGui::PopID();
        }
        for (int i = 0; i < static_cast<int>(m_storedPortableLights.size()); ++i)
        {
            PortableLight& light = m_storedPortableLights[static_cast<std::size_t>(i)];
            ImGui::PushID(900 + i);
            if (ImGui::Checkbox(u8"##lightLoadout", &light.selectedForLoadout)) SaveProgress();
            ImGui::SameLine();
            ImGui::Text(u8"ライト%d %s 残り%.0f秒 重量5", i + 1,
                light.broken ? u8"（破損）" : "", light.remainingSeconds);
            ImGui::PopID();
        }

        for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
        {
            const RelicType type = static_cast<RelicType>(i);
            ImGui::PushID(300 + static_cast<int>(i));
            const int storedCount = CountStoredRelics(type);
            ImGui::Text(u8"%s  保管:%d  持込:%d", GetRelicTypeName(type), storedCount, m_loadoutRelics[i]);
            ImGui::SameLine();
            if (ImGui::SmallButton("-")) { m_loadoutRelics[i] = std::max(0, m_loadoutRelics[i] - 1); SaveProgress(); }
            ImGui::SameLine();
            if (ImGui::SmallButton("+")) { m_loadoutRelics[i] = std::min(storedCount, m_loadoutRelics[i] + 1); SaveProgress(); }
            ImGui::PopID();
        }

        float loadoutWeight = 10.0f + static_cast<float>(m_loadoutFoodCount + m_loadoutHeatedFoodCount);
        loadoutWeight += static_cast<float>(m_loadoutRationOneCount) * kRationOneWeight;
        loadoutWeight += static_cast<float>(m_loadoutRawFishCount + m_loadoutCookedFishCount) * kUnknownFishWeight;
        for (size_t i = 0; i < m_loadoutRawSizedFish.size(); ++i)
            loadoutWeight += static_cast<float>(m_loadoutRawSizedFish[i] + m_loadoutCookedSizedFish[i]) * kSizedFishWeights[i];
        loadoutWeight += static_cast<float>(m_loadoutCartridgeCount) * kCartridgeWeight;
        loadoutWeight += static_cast<float>(std::count_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
            [](const WaterBottle& bottle) { return bottle.selectedForLoadout; })) * kWaterBottleWeight;
        loadoutWeight += static_cast<float>(std::count_if(m_storedCookingKits.begin(), m_storedCookingKits.end(),
            [](const CookingKit& kit) { return kit.selectedForLoadout; })) * kCookingKitWeight;
        loadoutWeight += static_cast<float>(std::count_if(m_storedPortableLights.begin(), m_storedPortableLights.end(),
            [](const PortableLight& light) { return light.selectedForLoadout; })) * kPortableLightWeight;
        for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
        {
            const RelicType type = static_cast<RelicType>(i);
            loadoutWeight += GetRelicWeight(type) * static_cast<float>(m_loadoutRelics[i]);
        }
        const float maxWeight = GetMaxWeight();

        ImGui::Separator();
        if (ImGui::Button(u8"任意保存", ImVec2(120.0f, 0.0f)))
            ShowCenterNotification(SaveProgress() ? u8"保存しました。" : u8"保存に失敗しました。");
    });
}

void SceneNarakuProto::DrawGeneralShop()
{
    DrawTownFrame(u8"商店", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();

        if (NarakuUi::BeginTabBar("ShopTabs"))
        {
            if (ImGui::BeginTabItem(u8"購入"))
            {
                ImGui::Spacing();
                auto drawBuyRow = [this](const char* name, int price, const char* desc, ShopTransactionMode mode, RelicType rType = RelicType::CashLow)
                {
                    ImGui::Text(u8"%s  %d G", name, price);
                    if (desc != nullptr && desc[0] != '\0')
                    {
                        ImGui::TextDisabled(u8"  %s", desc);
                    }
                    ImGui::SameLine();
                    char btnLabel[64];
                    std::snprintf(btnLabel, sizeof(btnLabel), u8"購入##%s", name);
                    const int maxAffordable = price > 0 ? (m_money / price) : 0;
                    ImGui::BeginDisabled(maxAffordable <= 0);
                    if (ImGui::SmallButton(btnLabel))
                    {
                        m_shopTransaction.mode = mode;
                        m_shopTransaction.relicType = rType;
                        m_shopTransaction.itemName = name;
                        m_shopTransaction.unitPrice = price;
                        m_shopTransaction.maxCount = maxAffordable;
                        m_shopTransaction.count = 1;
                        m_shopTransaction.openModal = true;
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                };

                drawBuyRow(u8"食料", kFoodPrice, u8"重量1 / HP+20 / 満腹度+10 / 水分+75", ShopTransactionMode::BuyFood);
                drawBuyRow(u8"水筒", kWaterBottlePrice, u8"重量15 / 容量100", ShopTransactionMode::BuyWaterBottle);
                drawBuyRow(u8"料理セット", kCookingKitPrice, u8"重量5 / 5回使用", ShopTransactionMode::BuyCookingKit);
                drawBuyRow(u8"ライト", kPortableLightPrice, u8"重量5 / 点灯可能時間15分", ShopTransactionMode::BuyPortableLight);

                const RelicType shopTypes[] = { RelicType::ArmamentUpgrade, RelicType::WeaponUpgrade, RelicType::ArmorUpgrade };
                const int shopPrices[] = { 600, 700, 700 };
                for (int i = 0; i < 3; ++i)
                {
                    drawBuyRow(GetRelicTypeName(shopTypes[i]), shopPrices[i], u8"強化用遺物素材", ShopTransactionMode::BuyRelic, shopTypes[i]);
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(u8"売却"))
            {
                ImGui::Spacing();
                bool hasSellable = false;
                for (std::size_t i = 0; i < static_cast<std::size_t>(RelicType::Count); ++i)
                {
                    const RelicType type = static_cast<RelicType>(i);
                    const int count = static_cast<int>(std::count_if(m_storedInventory.begin(), m_storedInventory.end(),
                        [type, this](const RelicItem& item) { return item.type == type && IsRelicSellable(item); }));
                    if (count <= 0) continue;
                    hasSellable = true;

                    auto sellIt = std::find_if(m_storedInventory.begin(), m_storedInventory.end(),
                        [type, this](const RelicItem& item) { return item.type == type && IsRelicSellable(item); });
                    const int saleValue = sellIt != m_storedInventory.end() ? sellIt->value : 0;

                    ImGui::Text(u8"%s (所持: %d個)  1個 %d G", GetRelicTypeName(type), count, saleValue);
                    ImGui::SameLine();
                    char btnLabel[64];
                    std::snprintf(btnLabel, sizeof(btnLabel), u8"売却##relic%d", static_cast<int>(i));
                    if (ImGui::SmallButton(btnLabel))
                    {
                        m_shopTransaction.mode = ShopTransactionMode::SellRelic;
                        m_shopTransaction.relicType = type;
                        m_shopTransaction.itemName = GetRelicTypeName(type);
                        m_shopTransaction.unitPrice = saleValue;
                        m_shopTransaction.maxCount = count;
                        m_shopTransaction.count = 1;
                        m_shopTransaction.openModal = true;
                    }
                    ImGui::Separator();
                }

                const int usableLightCount = static_cast<int>(std::count_if(
                    m_storedPortableLights.begin(), m_storedPortableLights.end(),
                    [](const PortableLight& light) { return !light.broken; }));
                const int brokenLightCount = static_cast<int>(m_storedPortableLights.size()) - usableLightCount;
                if (usableLightCount > 0 || brokenLightCount > 0) hasSellable = true;
                auto openLightSale = [this](const char* name, int count, int price, ShopTransactionMode mode)
                {
                    if (count <= 0) return;
                    ImGui::PushID(static_cast<int>(mode));
                    ImGui::Text(u8"%s (所持: %d個)  1個 %d G", name, count, price);
                    ImGui::SameLine();
                    if (ImGui::SmallButton(u8"売却"))
                    {
                        m_shopTransaction.mode = mode;
                        m_shopTransaction.itemName = name;
                        m_shopTransaction.unitPrice = price;
                        m_shopTransaction.maxCount = count;
                        m_shopTransaction.count = 1;
                        m_shopTransaction.openModal = true;
                    }
                    ImGui::Separator();
                    ImGui::PopID();
                };
                openLightSale(u8"ライト", usableLightCount, kPortableLightSellPrice,
                    ShopTransactionMode::SellPortableLight);
                openLightSale(u8"ライト（破損）", brokenLightCount, kBrokenPortableLightSellPrice,
                    ShopTransactionMode::SellBrokenPortableLight);

                if (!hasSellable)
                {
                    ImGui::TextDisabled(u8"売却できる遺物はありません。");
                }
                ImGui::Spacing();
                ImGui::TextDisabled(u8"※食料は売却できません。");

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    });
}

void SceneNarakuProto::DrawShopTransactionModal()
{
    if (m_shopTransaction.openModal)
    {
        ImGui::OpenPopup(u8"取引個数選択##ShopModal");
        m_shopTransaction.openModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(460.0f, 320.0f), ImGuiCond_Always);
    if (NarakuUi::BeginPopupModal(u8"取引個数選択##ShopModal", nullptr, ImGuiWindowFlags_NoResize))
    {
        const bool isBuy = m_shopTransaction.mode != ShopTransactionMode::SellRelic &&
            m_shopTransaction.mode != ShopTransactionMode::SellPortableLight &&
            m_shopTransaction.mode != ShopTransactionMode::SellBrokenPortableLight;
        ImGui::Text(u8"【%s】 %s", isBuy ? u8"購入" : u8"売却", m_shopTransaction.itemName.c_str());
        ImGui::Text(u8"単価: %d G  /  %s可能上限: %d 個",
            m_shopTransaction.unitPrice, isBuy ? u8"購入" : u8"売却", m_shopTransaction.maxCount);
        ImGui::Separator();

        auto clampQuantity = [this](int value) -> int {
            const int upper = std::max(1, m_shopTransaction.maxCount);
            return std::max(1, std::min(value, upper));
        };

        ImGui::Text(u8"数量:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("##ShopQuantity", &m_shopTransaction.count))
        {
            m_shopTransaction.count = clampQuantity(m_shopTransaction.count);
        }

        const int deltas[] = { -100, -10, -1, 1, 10, 100 };
        for (int i = 0; i < 6; ++i)
        {
            if (i > 0) ImGui::SameLine();
            char btnText[16];
            std::snprintf(btnText, sizeof(btnText), "%+d", deltas[i]);
            if (ImGui::Button(btnText, ImVec2(58.0f, 0.0f)))
            {
                m_shopTransaction.count = clampQuantity(m_shopTransaction.count + deltas[i]);
            }
        }

        ImGui::Spacing();
        ImGui::Separator();

        const int totalCost = m_shopTransaction.count * m_shopTransaction.unitPrice;
        const int expectedMoney = isBuy ? (m_money - totalCost) : (m_money + totalCost);
        ImGui::Text(u8"合計金額: %d G", totalCost);
        ImGui::Text(u8"所持金  : %d G  →  %d G (%s%d G)",
            m_money, expectedMoney, isBuy ? "-" : "+", totalCost);

        ImGui::Spacing();
        ImGui::Separator();

        const bool canExecute = (m_shopTransaction.maxCount > 0) &&
                                (m_shopTransaction.count >= 1) &&
                                (m_shopTransaction.count <= m_shopTransaction.maxCount) &&
                                (!isBuy || m_money >= totalCost);

        ImGui::BeginDisabled(!canExecute);
        if (ImGui::Button(isBuy ? u8"購入する" : u8"売却する", ImVec2(150.0f, 0.0f)))
        {
            ExecuteShopTransaction();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndDisabled();

        ImGui::SameLine();
        if (NarakuUi::BackButton(u8"キャンセル", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SceneNarakuProto::ExecuteShopTransaction()
{
    const int count = m_shopTransaction.count;
    if (count <= 0) return;

    const int totalCost = count * m_shopTransaction.unitPrice;

    switch (m_shopTransaction.mode)
    {
    case ShopTransactionMode::BuyFood:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            m_storedFoodCount += count;
            SaveProgress();
            ShowCenterNotification(u8"食料を購入しました。");
        }
        break;

    case ShopTransactionMode::BuyWaterBottle:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            for (int i = 0; i < count; ++i)
            {
                m_storedWaterBottles.push_back(WaterBottle());
            }
            SaveProgress();
            ShowCenterNotification(u8"水筒を購入しました。");
        }
        break;

    case ShopTransactionMode::BuyCookingKit:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            for (int i = 0; i < count; ++i)
            {
                m_storedCookingKits.push_back(CookingKit());
            }
            SaveProgress();
            ShowCenterNotification(u8"料理セットを購入しました。");
        }
        break;

    case ShopTransactionMode::BuyPortableLight:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            for (int i = 0; i < count; ++i) m_storedPortableLights.push_back(PortableLight());
            SaveProgress();
            ShowCenterNotification(u8"ライトを購入しました。");
        }
        break;

    case ShopTransactionMode::BuyRelic:
        if (m_money >= totalCost)
        {
            m_money -= totalCost;
            const std::size_t typeIndex = static_cast<std::size_t>(m_shopTransaction.relicType);
            for (int i = 0; i < count; ++i)
            {
                RelicItem item = CreateRelic(m_shopTransaction.relicType, GetRelicTypeName(m_shopTransaction.relicType));
                item.stabilized = true;
                m_storedInventory.push_back(item);
            }
            m_identifiedRelics[typeIndex] = true;
            SaveProgress();
            ShowCenterNotification(u8"遺物を購入しました。");
        }
        break;

    case ShopTransactionMode::SellRelic:
    {
        m_money += totalCost;
        int remainingToSell = count;
        const RelicType rType = m_shopTransaction.relicType;
        for (auto it = m_storedInventory.begin(); it != m_storedInventory.end() && remainingToSell > 0;)
        {
            if (it->type == rType && IsRelicSellable(*it))
            {
                it = m_storedInventory.erase(it);
                --remainingToSell;
            }
            else
            {
                ++it;
            }
        }
        const std::size_t typeIndex = static_cast<std::size_t>(rType);
        m_loadoutRelics[typeIndex] = std::min(m_loadoutRelics[typeIndex], CountStoredRelics(rType));
        SaveProgress();
        ShowCenterNotification(u8"遺物を売却しました。");
        break;
    }

    case ShopTransactionMode::SellPortableLight:
    case ShopTransactionMode::SellBrokenPortableLight:
    {
        const bool sellBroken = m_shopTransaction.mode == ShopTransactionMode::SellBrokenPortableLight;
        int remaining = count;
        for (auto it = m_storedPortableLights.begin(); it != m_storedPortableLights.end() && remaining > 0;)
        {
            if (it->broken == sellBroken) { it = m_storedPortableLights.erase(it); --remaining; }
            else ++it;
        }
        m_money += (count - remaining) * m_shopTransaction.unitPrice;
        SaveProgress();
        ShowCenterNotification(u8"ライトを売却しました。");
        break;
    }

    default:
        break;
    }
}

void SceneNarakuProto::DrawArmory()
{
    static const int armorDiscountPrices[][2] = { { 10, 15 }, { 150, 200 }, { 200, 300 }, { 250, 350 }, { 350, 400 }, { 5000, 6000 } };
    static const int armorMoneyPrices[][2] = { { 10, 15 }, { 150, 200 }, { 1000, 1500 }, { 1250, 1750 }, { 1750, 2000 }, { 25000, 30000 } };
    static const int armorArmamentCosts[][2] = { { 0, 0 }, { 0, 0 }, { 3, 4 }, { 4, 6 }, { 7, 10 }, { 17, 19 } };
    static const int armorMaterialCosts[][2] = { { 0, 0 }, { 0, 0 }, { 5, 7 }, { 7, 9 }, { 5, 7 }, { 15, 19 } };
    static const int weaponDiscountPrices[] = { 5, 500, 1250, 1500, 5000 };
    static const int weaponMoneyPrices[] = { 5, 500, 1250, 1500, 25000 };
    static const int weaponArmamentCosts[] = { 0, 0, 0, 0, 11 };
    static const int weaponMaterialCosts[] = { 0, 0, 0, 0, 21 };

    DrawTownFrame(u8"武具屋", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Text(u8"所持素材  武具強化:%d  武器強化:%d  防具強化:%d",
            CountStoredRelics(RelicType::ArmamentUpgrade),
            CountStoredRelics(RelicType::WeaponUpgrade),
            CountStoredRelics(RelicType::ArmorUpgrade));
        ImGui::Separator();

        if (NarakuUi::BeginTabBar("ArmoryCategoryTabs"))
        {
            // === 防具タブ ===
            if (ImGui::BeginTabItem(u8"防具"))
            {
                if (ImGui::BeginTable("ArmorSplitTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("ArmorList", ImGuiTableColumnFlags_WidthStretch, 0.46f);
                    ImGui::TableSetupColumn("ArmorDetail", ImGuiTableColumnFlags_WidthStretch, 0.54f);
                    ImGui::TableNextRow();

                    // --- 左側: 商品一覧 ---
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【防具一覧】");
                    ImGui::Separator();

                    if (ImGui::BeginChild("ArmorListChild", ImVec2(0.0f, 480.0f), false))
                    {
                        for (std::size_t i = 0; i < static_cast<std::size_t>(ArmorTier::Unknown); ++i)
                        {
                            const ArmorTier tier = static_cast<ArmorTier>(i);
                            const bool headOwned = m_ownedHeadArmor[i];
                            const bool bodyOwned = m_ownedBodyArmor[i];
                            const bool isSelected = (m_armoryUI.selectedArmorIndex == static_cast<int>(i));

                            ImGui::PushID(static_cast<int>(i));

                            const char* statusText = (headOwned && bodyOwned) ? u8" [一式所有]" :
                                                     (headOwned ? u8" [頭所有]" :
                                                     (bodyOwned ? u8" [胴所有]" : ""));
                            char itemHeader[128];
                            std::snprintf(itemHeader, sizeof(itemHeader), "%s%s", GetArmorName(tier), statusText);

                            if (ImGui::Selectable(itemHeader, isSelected))
                            {
                                m_armoryUI.selectedArmorIndex = static_cast<int>(i);
                            }

                            if (ImGui::SmallButton(u8"購入"))
                            {
                                m_armoryUI.selectedArmorIndex = static_cast<int>(i);
                                m_armoryUI.modalArmorTier = static_cast<int>(i);
                                m_armoryUI.modalIsWeapon = false;
                                m_armoryUI.modalArmorSlot = (!headOwned) ? 0 : 1;
                                m_armoryUI.openPurchaseModal = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(u8"情報を見る"))
                            {
                                m_armoryUI.selectedArmorIndex = static_cast<int>(i);
                            }
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();

                    // --- 右側: 選択商品の効果・ステータス上昇量・購入条件 ---
                    ImGui::TableSetColumnIndex(1);
                    const std::size_t selIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.selectedArmorIndex, static_cast<int>(ArmorTier::Unknown) - 1)));
                    const ArmorTier selTier = static_cast<ArmorTier>(selIndex);

                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【%s の詳細情報】", GetArmorName(selTier));
                    ImGui::Separator();

                    if (ImGui::BeginChild("ArmorDetailChild", ImVec2(0.0f, 480.0f), false))
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 部位効果");
                        ImGui::BulletText(u8"頭: %s", GetArmorEffectText(selTier, true));
                        ImGui::BulletText(u8"胴: %s", GetArmorEffectText(selTier, false));
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 装備時のステータス上昇・性能");
                        switch (selTier)
                        {
                        case ArmorTier::Leather:
                            ImGui::BulletText(u8"頭: 重量 5 / 防御倍率 5%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 8 / 防御倍率 10%% 軽減");
                            ImGui::TextDisabled(u8"  序盤探索向けの軽量な防具。");
                            break;
                        case ArmorTier::Iron:
                            ImGui::BulletText(u8"頭: 重量 12 / 防御倍率 15%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 18 / 防御倍率 25%% 軽減");
                            ImGui::TextDisabled(u8"  頑丈な金属防具。物理防御に優れるが重量が増加。");
                            break;
                        case ArmorTier::RelicCovered:
                            ImGui::BulletText(u8"頭: 重量 6 / 防御倍率 25%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 10 / 防御倍率 35%% 軽減");
                            ImGui::TextDisabled(u8"  遺物の薄膜で保護された防具。軽量かつ高耐久。");
                            break;
                        case ArmorTier::RelicHardened:
                            ImGui::BulletText(u8"頭: 重量 15 / 防御倍率 35%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 22 / 防御倍率 45%% 軽減");
                            ImGui::TextDisabled(u8"  遺物硬化処理により重撃・突進に耐える強固な装甲。");
                            break;
                        case ArmorTier::RelicEnhanced:
                            ImGui::BulletText(u8"頭: 重量 8 / 防御倍率 45%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 12 / 防御倍率 55%% 軽減");
                            ImGui::TextDisabled(u8"  深層探窟家向けの超高密度強化装甲。");
                            break;
                        case ArmorTier::Relic:
                            ImGui::BulletText(u8"頭: 重量 10 / 防御倍率 60%% 軽減");
                            ImGui::BulletText(u8"胴: 重量 15 / 防御倍率 70%% 軽減");
                            ImGui::TextColored(ImVec4(0.85f, 0.6f, 1.0f, 1.0f),
                                u8"★一式セット効果: 重量+100%% / 歩行+20%% / 走行+100%% / HP毎秒+2 / 攻撃+50%% / 防御+75%% / 採掘+50%%");
                            break;
                        default:
                            break;
                        }
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 購入に必要なコスト");
                        const bool hasMaterialPlan = (armorArmamentCosts[selIndex][0] > 0 || armorMaterialCosts[selIndex][0] > 0 ||
                                                      armorArmamentCosts[selIndex][1] > 0 || armorMaterialCosts[selIndex][1] > 0);
                        if (hasMaterialPlan)
                        {
                            ImGui::BulletText(u8"遺物併用（割引価格）:");
                            ImGui::Text(u8"  頭: %d G (武具遺物 %d個 / 防具遺物 %d個)",
                                armorDiscountPrices[selIndex][0], armorArmamentCosts[selIndex][0], armorMaterialCosts[selIndex][0]);
                            ImGui::Text(u8"  胴: %d G (武具遺物 %d個 / 防具遺物 %d個)",
                                armorDiscountPrices[selIndex][1], armorArmamentCosts[selIndex][1], armorMaterialCosts[selIndex][1]);
                        }
                        else
                        {
                            ImGui::BulletText(u8"素材不要（金のみで購入可能）");
                        }
                        ImGui::BulletText(u8"金のみで購入:");
                        ImGui::Text(u8"  頭: %d G / 胴: %d G", armorMoneyPrices[selIndex][0], armorMoneyPrices[selIndex][1]);

                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 現在の所有状況");
                        ImGui::BulletText(u8"頭: %s", m_ownedHeadArmor[selIndex] ? u8"所有済み" : u8"未所有");
                        ImGui::BulletText(u8"胴: %s", m_ownedBodyArmor[selIndex] ? u8"所有済み" : u8"未所有");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            // === 武器タブ ===
            if (ImGui::BeginTabItem(u8"武器"))
            {
                if (ImGui::BeginTable("WeaponSplitTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("WeaponList", ImGuiTableColumnFlags_WidthStretch, 0.46f);
                    ImGui::TableSetupColumn("WeaponDetail", ImGuiTableColumnFlags_WidthStretch, 0.54f);
                    ImGui::TableNextRow();

                    // --- 左側: 商品一覧 ---
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.3f, 1.0f), u8"【武器一覧】");
                    ImGui::Separator();

                    if (ImGui::BeginChild("WeaponListChild", ImVec2(0.0f, 480.0f), false))
                    {
                        for (std::size_t i = 0; i < static_cast<std::size_t>(WeaponTier::Unknown); ++i)
                        {
                            const WeaponTier tier = static_cast<WeaponTier>(i);
                            const bool owned = m_ownedWeapons[i];
                            const bool isSelected = (m_armoryUI.selectedWeaponIndex == static_cast<int>(i));

                            ImGui::PushID(100 + static_cast<int>(i));

                            char itemHeader[128];
                            std::snprintf(itemHeader, sizeof(itemHeader), "%s%s", GetWeaponName(tier), owned ? u8" [所有済み]" : "");

                            if (ImGui::Selectable(itemHeader, isSelected))
                            {
                                m_armoryUI.selectedWeaponIndex = static_cast<int>(i);
                            }

                            if (ImGui::SmallButton(u8"購入"))
                            {
                                m_armoryUI.selectedWeaponIndex = static_cast<int>(i);
                                m_armoryUI.modalWeaponTier = static_cast<int>(i);
                                m_armoryUI.modalIsWeapon = true;
                                m_armoryUI.openPurchaseModal = true;
                            }
                            ImGui::SameLine();
                            if (ImGui::SmallButton(u8"情報を見る"))
                            {
                                m_armoryUI.selectedWeaponIndex = static_cast<int>(i);
                            }
                            ImGui::Separator();
                            ImGui::PopID();
                        }
                    }
                    ImGui::EndChild();

                    // --- 右側: 選択商品の効果・ステータス上昇量・購入条件 ---
                    ImGui::TableSetColumnIndex(1);
                    const std::size_t selIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.selectedWeaponIndex, static_cast<int>(WeaponTier::Unknown) - 1)));
                    const WeaponTier selTier = static_cast<WeaponTier>(selIndex);

                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【%s の詳細情報】", GetWeaponName(selTier));
                    ImGui::Separator();

                    if (ImGui::BeginChild("WeaponDetailChild", ImVec2(0.0f, 480.0f), false))
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 武器効果");
                        ImGui::BulletText(u8"%s", GetWeaponEffectText(selTier));
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 装備時のステータス上昇・性能");
                        switch (selTier)
                        {
                        case WeaponTier::RustyPickaxe:
                            ImGui::BulletText(u8"攻撃力: 10 / 採掘速度: 基準(100%%)");
                            ImGui::TextDisabled(u8"  初期所持の古いピッケル。");
                            break;
                        case WeaponTier::NormalPickaxe:
                            ImGui::BulletText(u8"攻撃力: 15 / 採掘速度: +25%% (125%%)");
                            ImGui::TextDisabled(u8"  一般的な標準ピッケル。");
                            break;
                        case WeaponTier::SturdyPickaxe:
                            ImGui::BulletText(u8"攻撃力: 22 / 採掘速度: +50%% (150%%)");
                            ImGui::TextDisabled(u8"  強固な金属で鍛えられた良質なピッケル。");
                            break;
                        case WeaponTier::SharpPickaxe:
                            ImGui::BulletText(u8"攻撃力: 30 / 採掘速度: +75%% (175%%)");
                            ImGui::TextDisabled(u8"  鋭利な刃先を持ち、硬い鉱床も容易に穿つ。");
                            break;
                        case WeaponTier::RelicPickaxe:
                            ImGui::BulletText(u8"攻撃力: 45 / 採掘速度: +120%% (220%%)");
                            ImGui::TextColored(ImVec4(0.85f, 0.6f, 1.0f, 1.0f),
                                u8"★遺物ピッケル: 驚異的な採掘速度と威力を誇る伝説の逸品。");
                            break;
                        default:
                            break;
                        }
                        ImGui::Spacing();

                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 購入に必要なコスト");
                        const bool hasMaterialPlan = (weaponArmamentCosts[selIndex] > 0 || weaponMaterialCosts[selIndex] > 0);
                        if (hasMaterialPlan)
                        {
                            ImGui::BulletText(u8"遺物併用（割引価格）:");
                            ImGui::Text(u8"  %d G (武具遺物 %d個 / 武器遺物 %d個)",
                                weaponDiscountPrices[selIndex], weaponArmamentCosts[selIndex], weaponMaterialCosts[selIndex]);
                        }
                        else
                        {
                            ImGui::BulletText(u8"素材不要（金のみで購入可能）");
                        }
                        ImGui::BulletText(u8"金のみで購入: %d G", weaponMoneyPrices[selIndex]);

                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"■ 現在の所有状況");
                        ImGui::BulletText(u8"%s", m_ownedWeapons[selIndex] ? u8"所有済み" : u8"未所有");
                    }
                    ImGui::EndChild();

                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    });
}

void SceneNarakuProto::DrawArmoryPurchaseModal()
{
    static const int armorDiscountPrices[][2] = { { 10, 15 }, { 150, 200 }, { 200, 300 }, { 250, 350 }, { 350, 400 }, { 5000, 6000 } };
    static const int armorMoneyPrices[][2] = { { 10, 15 }, { 150, 200 }, { 1000, 1500 }, { 1250, 1750 }, { 1750, 2000 }, { 25000, 30000 } };
    static const int armorArmamentCosts[][2] = { { 0, 0 }, { 0, 0 }, { 3, 4 }, { 4, 6 }, { 7, 10 }, { 17, 19 } };
    static const int armorMaterialCosts[][2] = { { 0, 0 }, { 0, 0 }, { 5, 7 }, { 7, 9 }, { 5, 7 }, { 15, 19 } };
    static const int weaponDiscountPrices[] = { 5, 500, 1250, 1500, 5000 };
    static const int weaponMoneyPrices[] = { 5, 500, 1250, 1500, 25000 };
    static const int weaponArmamentCosts[] = { 0, 0, 0, 0, 11 };
    static const int weaponMaterialCosts[] = { 0, 0, 0, 0, 21 };

    if (m_armoryUI.openPurchaseModal)
    {
        ImGui::OpenPopup(u8"武具購入##ArmoryPurchasePopupModal");
        m_armoryUI.openPurchaseModal = false;
    }

    ImGui::SetNextWindowSize(ImVec2(480.0f, 380.0f), ImGuiCond_Always);
    if (NarakuUi::BeginPopupModal(u8"武具購入##ArmoryPurchasePopupModal", nullptr, ImGuiWindowFlags_NoResize))
    {
        if (m_armoryUI.modalIsWeapon)
        {
            // === 武器の購入 ===
            const std::size_t tierIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.modalWeaponTier, static_cast<int>(WeaponTier::Unknown) - 1)));
            const WeaponTier tier = static_cast<WeaponTier>(tierIndex);
            const bool owned = m_ownedWeapons[tierIndex];

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【武器購入】 %s", GetWeaponName(tier));
            ImGui::Separator();

            if (owned)
            {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"※この武器はすでに所有しています。");
            }
            else
            {
                const int discountPrice = weaponDiscountPrices[tierIndex];
                const int moneyPrice = weaponMoneyPrices[tierIndex];
                const int reqArmament = weaponArmamentCosts[tierIndex];
                const int reqWeapon = weaponMaterialCosts[tierIndex];
                const int curArmament = CountStoredRelics(RelicType::ArmamentUpgrade);
                const int curWeapon = CountStoredRelics(RelicType::WeaponUpgrade);

                const bool canBuyMaterial = (m_money >= discountPrice) && (curArmament >= reqArmament) && (curWeapon >= reqWeapon);
                const bool canBuyMoney = (m_money >= moneyPrice);

                // --- 遺物を消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 遺物を消費して購入（割引）");
                if (reqArmament > 0 || reqWeapon > 0)
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G)", discountPrice, m_money);
                    ImGui::Text(u8"必要素材: 武具強化 %d個 (所持: %d個) / 武器強化 %d個 (所持: %d個)",
                        reqArmament, curArmament, reqWeapon, curWeapon);
                }
                else
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G) ※素材不要", discountPrice, m_money);
                }

                ImGui::BeginDisabled(!canBuyMaterial);
                if (ImGui::Button(u8"遺物を消費して買う##BuyWeaponMat", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyWeapon(tier, true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();

                // --- 金のみを消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 金のみを消費して購入");
                ImGui::Text(u8"必要金: %d G (所持: %d G)", moneyPrice, m_money);

                ImGui::BeginDisabled(!canBuyMoney);
                if (ImGui::Button(u8"金のみを消費して買う##BuyWeaponMoney", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyWeapon(tier, false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
            }
        }
        else
        {
            // === 防具の購入 ===
            const std::size_t tierIndex = static_cast<std::size_t>(std::max(0, std::min(m_armoryUI.modalArmorTier, static_cast<int>(ArmorTier::Unknown) - 1)));
            const ArmorTier tier = static_cast<ArmorTier>(tierIndex);

            ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), u8"【防具購入】 %s", GetArmorName(tier));
            ImGui::TextUnformatted(u8"購入する部位を選択してください:");
            ImGui::RadioButton(u8"頭防具", &m_armoryUI.modalArmorSlot, 0);
            ImGui::SameLine();
            ImGui::RadioButton(u8"胴防具", &m_armoryUI.modalArmorSlot, 1);
            ImGui::Separator();

            const int slot = m_armoryUI.modalArmorSlot; // 0: 頭, 1: 胴
            const bool owned = (slot == 0) ? m_ownedHeadArmor[tierIndex] : m_ownedBodyArmor[tierIndex];

            if (owned)
            {
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), u8"※選択中の部位（%s）はすでに所有しています。", slot == 0 ? u8"頭" : u8"胴");
            }
            else
            {
                const int discountPrice = armorDiscountPrices[tierIndex][slot];
                const int moneyPrice = armorMoneyPrices[tierIndex][slot];
                const int reqArmament = armorArmamentCosts[tierIndex][slot];
                const int reqArmor = armorMaterialCosts[tierIndex][slot];
                const int curArmament = CountStoredRelics(RelicType::ArmamentUpgrade);
                const int curArmor = CountStoredRelics(RelicType::ArmorUpgrade);

                const bool canBuyMaterial = (m_money >= discountPrice) && (curArmament >= reqArmament) && (curArmor >= reqArmor);
                const bool canBuyMoney = (m_money >= moneyPrice);

                // --- 遺物を消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 遺物を消費して購入（割引）");
                if (reqArmament > 0 || reqArmor > 0)
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G)", discountPrice, m_money);
                    ImGui::Text(u8"必要素材: 武具強化 %d個 (所持: %d個) / 防具強化 %d個 (所持: %d個)",
                        reqArmament, curArmament, reqArmor, curArmor);
                }
                else
                {
                    ImGui::Text(u8"必要金: %d G (所持: %d G) ※素材不要", discountPrice, m_money);
                }

                ImGui::BeginDisabled(!canBuyMaterial);
                if (ImGui::Button(u8"遺物を消費して買う##BuyArmorMat", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyArmor(tier, slot == 0, true);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Separator();

                // --- 金のみを消費して買う ---
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), u8"▼ 金のみを消費して購入");
                ImGui::Text(u8"必要金: %d G (所持: %d G)", moneyPrice, m_money);

                ImGui::BeginDisabled(!canBuyMoney);
                if (ImGui::Button(u8"金のみを消費して買う##BuyArmorMoney", ImVec2(220.0f, 0.0f)))
                {
                    TryBuyArmor(tier, slot == 0, false);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        if (NarakuUi::BackButton(u8"閉じる", ImVec2(100.0f, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SceneNarakuProto::DrawRestaurant()
{
    DrawTownFrame(u8"地上レストラン", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();
        ImGui::TextWrapped(u8"50Gで満腹度を100にし、水分75、最大HPの75%%、最大精神力の50%%を回復します。");
        if (ImGui::Button(u8"食事をする（50G）", ImVec2(180.0f, 0.0f))) TryUseRestaurant();
        ImGui::Separator();
        ImGui::Text(u8"給水所（無料）");
        if (m_hydration < kHydrationMaximum && ImGui::Button(u8"安全な水を直接飲む（水分+25）"))
        {
            DrinkWater(kWaterDrinkAmount, WaterQuality::Safe, 0.0f);
            SaveProgress();
            ShowCenterNotification(u8"安全な水を飲みました。");
        }
        const auto emptyBottle = std::find_if(m_storedWaterBottles.begin(), m_storedWaterBottles.end(),
            [](const WaterBottle& bottle) { return bottle.amount <= 0.0f; });
        if (emptyBottle != m_storedWaterBottles.end() && ImGui::Button(u8"空の水筒1本を安全な水で満たす"))
        {
            emptyBottle->amount = kWaterBottleCapacity;
            emptyBottle->quality = WaterQuality::Safe;
            emptyBottle->foodPoisoningChance = 0.0f;
            SaveProgress();
            ShowCenterNotification(u8"水筒を安全な水で満たしました。");
        }
    });
}

void SceneNarakuProto::DrawBaseInteractionMarker() const
{
    const NarakuMap::BasePoint* nearest = nullptr;
    float nearestDistance = std::numeric_limits<float>::max();
    for (const NarakuMap::BasePoint& base : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base.layerId);
        if (layerIndex < 0 ||
            std::fabs(m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth - m_player.depth) > 0.35f)
        {
            continue;
        }
        const float distance = Distance(m_player.pos, { base.xz.x, base.xz.z });
        if (distance < nearestDistance)
        {
            nearestDistance = distance;
            nearest = &base;
        }
    }
    if (nearest == nullptr || nearestDistance > 3.5f) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y - 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowBgAlpha(0.75f);
    ImGui::Begin("BaseInteractionPrompt", nullptr, ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text(u8"%s  %s", nearest->type == NarakuMap::BaseType::SecondBase ? u8"第二拠点" : u8"前衛拠点",
        nearestDistance <= kInteractRange ? u8"[F] 利用" : u8"もう少し近づいてください");
    ImGui::End();
}

void SceneNarakuProto::FillSafeWater(bool useStoredBottles)
{
    std::vector<WaterBottle>& bottles = useStoredBottles ? m_storedWaterBottles : m_waterBottles;
    const auto empty = std::find_if(bottles.begin(), bottles.end(),
        [](const WaterBottle& bottle) { return bottle.amount <= 0.0f; });
    if (empty == bottles.end())
    {
        ShowCenterNotification(u8"空の水筒がありません。");
        return;
    }
    empty->amount = kWaterBottleCapacity;
    empty->quality = WaterQuality::Safe;
    empty->foodPoisoningChance = 0.0f;
    SaveProgress();
    ShowCenterNotification(u8"水筒を安全な水で満たしました。");
}

bool SceneNarakuProto::TryUseSecondBaseMeal()
{
    if (m_money < kSecondBaseMealPrice)
    {
        ShowCenterNotification(u8"所持金が足りません。");
        return false;
    }
    if (m_fullness >= kFullnessMaximum && m_hydration >= kHydrationMaximum &&
        m_player.hp >= GetMaxHp() && m_player.mental >= GetMaxMental())
    {
        ShowCenterNotification(u8"今は食事をする必要がありません。");
        return false;
    }
    m_money -= kSecondBaseMealPrice;
    m_fullness = kFullnessMaximum;
    m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
    m_player.hp = std::min(GetMaxHp(), m_player.hp + GetMaxHp() * kRestaurantHpRatio);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + GetMaxMental() * kRestaurantMentalRatio);
    SaveProgress();
    ShowCenterNotification(u8"第二拠点で食事をしました。");
    return true;
}

bool SceneNarakuProto::TryUseForwardBaseMeal()
{
    if (m_money < kForwardBaseMealPrice)
    {
        ShowCenterNotification(u8"所持金が足りません。");
        return false;
    }
    if (m_player.hp >= GetMaxHp() && m_fullness >= kFullnessMaximum)
    {
        ShowCenterNotification(u8"今は食事をする必要がありません。");
        return false;
    }
    m_money -= kForwardBaseMealPrice;
    m_player.hp = std::min(GetMaxHp(), m_player.hp + GetMaxHp() * kForwardBaseMealRecoveryRatio);
    m_fullness = std::min(kFullnessMaximum,
        m_fullness + kFullnessMaximum * kForwardBaseMealRecoveryRatio);
    SaveProgress();
    ShowCenterNotification(u8"前衛拠点で食事をしました。深層では精神までは休まりません。");
    return true;
}

void SceneNarakuProto::AdvanceWorldTime(double gameSeconds, double realSeconds)
{
    gameSeconds = std::max(0.0, gameSeconds);
    realSeconds = std::max(0.0, realSeconds);
    UpdateRespawns(static_cast<float>(realSeconds));
    UpdateMiningRespawns(static_cast<float>(realSeconds));
    UpdateFishingPointRecharge(gameSeconds);

    for (std::size_t index = 0; index < m_quests.size(); ++index)
    {
        QuestRecord& quest = m_quests[index];
        if (quest.status == QuestStatus::Available)
        {
            quest.cooldownSeconds -= gameSeconds;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
        else if (quest.status == QuestStatus::Active)
        {
            quest.remainingSeconds -= realSeconds;
            if (quest.remainingSeconds <= 0.0) FailQuest(index);
        }
        else if (quest.status == QuestStatus::Cooldown)
        {
            quest.cooldownSeconds -= realSeconds;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
    }

    m_gameWeekSeconds += gameSeconds;
    while (m_gameWeekSeconds >= kGameWeekSeconds)
    {
        m_gameWeekSeconds -= kGameWeekSeconds;
        BeginNewGameWeek();
    }
}

void SceneNarakuProto::StayAtSecondBase()
{
    if (m_money < kSecondBaseLodgingPrice)
    {
        ShowCenterNotification(u8"宿泊料金が足りません。");
        return;
    }
    const double current = std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds);
    const double dayStart = std::floor(current / kGameDaySeconds) * kGameDaySeconds;
    const double seven = dayStart + 7.0 * 60.0 * 60.0;
    const double target = current < seven ? seven : seven + kGameDaySeconds;
    const double elapsedGameSeconds = target - current;
    const double elapsedRealSeconds = elapsedGameSeconds / kGameTimeScale;
    m_money -= kSecondBaseLodgingPrice;
    m_player.mental = std::min(GetMaxMental(), m_player.mental + GetMaxMental() * 0.40f);
    AdvanceWorldTime(elapsedGameSeconds, elapsedRealSeconds);
    SaveProgress();
    ShowCenterNotification(u8"第二拠点に宿泊し、午前7時に起床しました。");
}

float SceneNarakuProto::GetSecondBaseSaleMultiplier(const RelicItem& item) const
{
    const int day = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds) /
        kGameDaySeconds);
    enum class Category { Cash, Upgrade, Special, Broken };
    Category category = Category::Special;
    if (item.broken) category = Category::Broken;
    else if (item.type == RelicType::CashLow || item.type == RelicType::CashHigh) category = Category::Cash;
    else if (item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade ||
        item.type == RelicType::ArmorUpgrade) category = Category::Upgrade;

    static constexpr Category high[] = { Category::Upgrade, Category::Special, Category::Upgrade,
        Category::Cash, Category::Cash, Category::Broken, Category::Special };
    static constexpr Category low[] = { Category::Broken, Category::Upgrade, Category::Special,
        Category::Upgrade, Category::Broken, Category::Special, Category::Cash };
    if (category == high[day]) return 1.50f;
    if (category == low[day]) return 0.75f;
    return 1.0f;
}

bool SceneNarakuProto::IsSecondBaseSellable(const RelicItem& item) const
{
    if (!IsRelicSellable(item)) return false;
    if (item.broken) return true;
    return item.type == RelicType::CashLow || item.type == RelicType::CashHigh ||
        item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade ||
        item.type == RelicType::ArmorUpgrade || item.type == RelicType::Offensive ||
        item.type == RelicType::Survival || item.type == RelicType::MentalRecovery;
}

int SceneNarakuProto::GetSecondBaseSaleValue(const RelicItem& item) const
{
    return static_cast<int>(std::lround(static_cast<double>(item.value) * GetSecondBaseSaleMultiplier(item)));
}

void SceneNarakuProto::DrawSecondBase()
{
    DrawTownFrame(u8"第二拠点", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();
        if (NarakuUi::BeginTabBar("SecondBaseTabs"))
        {
        if (ImGui::BeginTabItem(u8"食事"))
        {
            ImGui::TextWrapped(u8"地上と同じ食事を100Gで提供します。");
            if (ImGui::Button(u8"食事をする（100G）")) TryUseSecondBaseMeal();
            ImGui::Separator();
            ImGui::Text(u8"携帯食料 20G / 個");
            if (ImGui::Button(u8"携帯食料を1個買う") && m_money >= kFoodPrice * 2)
            {
                m_money -= kFoodPrice * 2;
                ++m_foodCount;
                SaveProgress();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"給水"))
        {
            if (ImGui::Button(u8"安全な水を直接飲む（水分+25）"))
            {
                DrinkWater(kWaterDrinkAmount, WaterQuality::Safe, 0.0f);
                SaveProgress();
            }
            if (ImGui::Button(u8"空の水筒1本を満たす")) FillSafeWater(false);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"宿泊"))
        {
            ImGui::TextWrapped(u8"1,000Gで次の午前7時まで休み、最大精神力の40%%分を回復します。時間依存イベントも進行します。");
            if (ImGui::Button(u8"宿泊する（1,000G）")) StayAtSecondBase();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"売却"))
        {
            static const char* weekdays[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
            const int day = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds) / kGameDaySeconds);
            ImGui::Text(u8"%s曜日の相場", weekdays[day]);
            bool found = false;
            for (int index = 0; index < static_cast<int>(m_inventory.size()); ++index)
            {
                const RelicItem& item = m_inventory[static_cast<std::size_t>(index)];
                if (!IsSecondBaseSellable(item)) continue;
                found = true;
                ImGui::PushID(index);
                ImGui::Text(u8"%s  %dG（%.2g倍）", GetRelicDisplayName(item),
                    GetSecondBaseSaleValue(item), GetSecondBaseSaleMultiplier(item));
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"売却"))
                {
                    m_money += GetSecondBaseSaleValue(item);
                    m_inventory.erase(m_inventory.begin() + index);
                    SaveProgress();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            for (int index = 0; index < static_cast<int>(m_portableLights.size()); ++index)
            {
                PortableLight& light = m_portableLights[static_cast<std::size_t>(index)];
                if (!light.broken) continue;
                found = true;
                const float multiplier = day == 5 ? 1.50f : (day == 0 || day == 4 ? 0.75f : 1.0f);
                const int price = static_cast<int>(std::lround(kBrokenPortableLightSellPrice * multiplier));
                ImGui::PushID(10000 + index);
                ImGui::Text(u8"ライト（破損）  %dG（%.2g倍）", price, multiplier);
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"売却"))
                {
                    m_money += price;
                    m_portableLights.erase(m_portableLights.begin() + index);
                    SaveProgress();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (!found) ImGui::TextDisabled(u8"売却できる遺物を所持していません。");
            ImGui::EndTabItem();
        }
            ImGui::EndTabBar();
        }
    }, Mode::Explore, u8"拠点を出る");
}

void SceneNarakuProto::DrawForwardBase()
{
    DrawTownFrame(u8"前衛拠点", [this]()
    {
        ImGui::Text(u8"所持金: %d G", m_money);
        ImGui::Separator();
        auto buyCurrent = [this](const char* name, int price, int& count)
        {
        ImGui::Text(u8"%s  %dG", name, price);
        ImGui::SameLine();
        ImGui::PushID(name);
        ImGui::BeginDisabled(m_money < price);
        if (ImGui::SmallButton(u8"購入"))
        {
            m_money -= price;
            ++count;
            SaveProgress();
            ShowCenterNotification(std::string(name) + u8"を購入しました。");
        }
        ImGui::EndDisabled();
        ImGui::PopID();
        };
        auto countCarriedRelics = [this](RelicType type)
        {
        return static_cast<int>(std::count_if(m_inventory.begin(), m_inventory.end(),
            [type](const RelicItem& item) { return item.type == type; }));
        };
        auto consumeCarriedRelics = [this](RelicType type, int count)
        {
        for (auto it = m_inventory.begin(); it != m_inventory.end() && count > 0;)
        {
            if (it->type == type)
            {
                it = m_inventory.erase(it);
                --count;
            }
            else ++it;
        }
        };
        if (NarakuUi::BeginTabBar("ForwardBaseTabs"))
        {
        if (ImGui::BeginTabItem(u8"商店"))
        {
            auto buyRelic = [this](RelicType type, int price)
            {
                const char* name = GetRelicTypeName(type);
                ImGui::Text(u8"%s  %dG", name, price);
                ImGui::SameLine();
                ImGui::PushID(static_cast<int>(type));
                ImGui::BeginDisabled(m_money < price);
                if (ImGui::SmallButton(u8"購入"))
                {
                    m_money -= price;
                    RelicItem item = CreateRelic(type, name);
                    item.stabilized = true;
                    m_inventory.push_back(item);
                    m_identifiedRelics[static_cast<std::size_t>(type)] = true;
                    SaveProgress();
                    ShowCenterNotification(std::string(name) + u8"を購入しました。");
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            };
            buyCurrent(u8"携帯食料", kFoodPrice, m_foodCount);
            ImGui::Text(u8"水筒  %dG", kWaterBottlePrice); ImGui::SameLine();
            if (ImGui::SmallButton(u8"購入##forwardBottle") && m_money >= kWaterBottlePrice)
            { m_money -= kWaterBottlePrice; m_waterBottles.push_back({}); SaveProgress(); }
            ImGui::Text(u8"料理セット  %dG", kCookingKitPrice); ImGui::SameLine();
            if (ImGui::SmallButton(u8"購入##forwardKit") && m_money >= kCookingKitPrice)
            { m_money -= kCookingKitPrice; m_cookingKits.push_back({}); SaveProgress(); }
            buyRelic(RelicType::ArmamentUpgrade, 600);
            buyRelic(RelicType::WeaponUpgrade, 700);
            buyRelic(RelicType::ArmorUpgrade, 700);
            ImGui::Separator();
            buyCurrent(u8"行動食1号", kRationOnePrice, m_rationOneCount);
            buyCurrent(u8"カートリッジ", kCartridgePrice, m_cartridgeCount);
            buyCurrent(u8"見たことない魚", kUnknownFishPrice, m_rawFishCount);
            ImGui::TextDisabled(u8"魚: 重量20 / 生食 満腹度+10・精神力+5 / 調理後 満腹度+50・精神力+25");
            if (m_rawFishCount > 0 && ImGui::SmallButton(u8"未調理の魚を売却（500G）"))
            { --m_rawFishCount; m_money += 500; SaveProgress(); }
            if (m_cookedFishCount > 0 && ImGui::SmallButton(u8"調理済みの魚を売却（400G）"))
            { --m_cookedFishCount; m_money += 400; SaveProgress(); }
            static const char* sizeNames[] = { u8"小", u8"中", u8"大" };
            for (int size = 0; size < 3; ++size)
            {
                ImGui::PushID(4200 + size);
                ImGui::Text(u8"魚（%s） 生:%d / 調理済み:%d", sizeNames[size],
                    m_rawSizedFish[static_cast<size_t>(size)], m_cookedSizedFish[static_cast<size_t>(size)]);
                if (m_rawSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"生を売却"))
                { --m_rawSizedFish[static_cast<size_t>(size)]; m_money += kRawSizedFishSellValues[static_cast<size_t>(size)]; SaveProgress(); }
                ImGui::SameLine();
                if (m_cookedSizedFish[static_cast<size_t>(size)] > 0 && ImGui::SmallButton(u8"調理済みを売却"))
                { --m_cookedSizedFish[static_cast<size_t>(size)]; m_money += kCookedSizedFishSellValues[static_cast<size_t>(size)]; SaveProgress(); }
                ImGui::PopID();
            }
            ImGui::SeparatorText(u8"遺物売却");
            bool hasSellableRelic = false;
            for (int index = 0; index < static_cast<int>(m_inventory.size()); ++index)
            {
                const RelicItem& item = m_inventory[static_cast<std::size_t>(index)];
                if (!IsRelicSellable(item)) continue;
                hasSellableRelic = true;
                ImGui::PushID(3000 + index);
                ImGui::Text(u8"%s  %dG", GetRelicDisplayName(item), item.value);
                ImGui::SameLine();
                if (ImGui::SmallButton(u8"売却"))
                {
                    m_money += item.value;
                    m_inventory.erase(m_inventory.begin() + index);
                    SaveProgress();
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            if (!hasSellableRelic) ImGui::TextDisabled(u8"売却できる遺物を所持していません。");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"武具屋"))
        {
            static const int armorDiscountPrices[][2] = { {10,15}, {150,200}, {200,300}, {250,350}, {350,400}, {5000,6000} };
            static const int armorMoneyPrices[][2] = { {10,15}, {150,200}, {1000,1500}, {1250,1750}, {1750,2000}, {25000,30000} };
            static const int armorArmamentCosts[][2] = { {0,0}, {0,0}, {3,4}, {4,6}, {7,10}, {17,19} };
            static const int armorMaterialCosts[][2] = { {0,0}, {0,0}, {5,7}, {7,9}, {5,7}, {15,19} };
            for (int tierValue = 0; tierValue < static_cast<int>(ArmorTier::Unknown); ++tierValue)
            {
                const ArmorTier tier = static_cast<ArmorTier>(tierValue);
                ImGui::PushID(1000 + tierValue);
                if (!m_ownedHeadArmor[static_cast<std::size_t>(tier)])
                {
                    ImGui::Text(u8"%s 頭", GetArmorName(tier)); ImGui::SameLine();
                    if (ImGui::SmallButton(u8"金のみ##headMoney")) TryBuyArmor(tier, true, false);
                    if (armorArmamentCosts[tierValue][0] > 0 || armorMaterialCosts[tierValue][0] > 0)
                    {
                        ImGui::SameLine();
                        const bool canBuy = m_money >= armorDiscountPrices[tierValue][0] &&
                            countCarriedRelics(RelicType::ArmamentUpgrade) >= armorArmamentCosts[tierValue][0] &&
                            countCarriedRelics(RelicType::ArmorUpgrade) >= armorMaterialCosts[tierValue][0];
                        ImGui::BeginDisabled(!canBuy);
                        if (ImGui::SmallButton(u8"素材併用##headMaterial"))
                        {
                            m_money -= armorDiscountPrices[tierValue][0];
                            consumeCarriedRelics(RelicType::ArmamentUpgrade, armorArmamentCosts[tierValue][0]);
                            consumeCarriedRelics(RelicType::ArmorUpgrade, armorMaterialCosts[tierValue][0]);
                            m_ownedHeadArmor[static_cast<std::size_t>(tier)] = true;
                            SaveProgress();
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::TextDisabled(u8"  金のみ %dG / 素材併用 %dG（武具%d・装備%d）",
                        armorMoneyPrices[tierValue][0], armorDiscountPrices[tierValue][0],
                        armorArmamentCosts[tierValue][0], armorMaterialCosts[tierValue][0]);
                }
                if (!m_ownedBodyArmor[static_cast<std::size_t>(tier)])
                {
                    ImGui::Text(u8"%s 胴", GetArmorName(tier)); ImGui::SameLine();
                    if (ImGui::SmallButton(u8"金のみ##bodyMoney")) TryBuyArmor(tier, false, false);
                    if (armorArmamentCosts[tierValue][1] > 0 || armorMaterialCosts[tierValue][1] > 0)
                    {
                        ImGui::SameLine();
                        const bool canBuy = m_money >= armorDiscountPrices[tierValue][1] &&
                            countCarriedRelics(RelicType::ArmamentUpgrade) >= armorArmamentCosts[tierValue][1] &&
                            countCarriedRelics(RelicType::ArmorUpgrade) >= armorMaterialCosts[tierValue][1];
                        ImGui::BeginDisabled(!canBuy);
                        if (ImGui::SmallButton(u8"素材併用##bodyMaterial"))
                        {
                            m_money -= armorDiscountPrices[tierValue][1];
                            consumeCarriedRelics(RelicType::ArmamentUpgrade, armorArmamentCosts[tierValue][1]);
                            consumeCarriedRelics(RelicType::ArmorUpgrade, armorMaterialCosts[tierValue][1]);
                            m_ownedBodyArmor[static_cast<std::size_t>(tier)] = true;
                            SaveProgress();
                        }
                        ImGui::EndDisabled();
                    }
                    ImGui::TextDisabled(u8"  金のみ %dG / 素材併用 %dG（武具%d・装備%d）",
                        armorMoneyPrices[tierValue][1], armorDiscountPrices[tierValue][1],
                        armorArmamentCosts[tierValue][1], armorMaterialCosts[tierValue][1]);
                }
                ImGui::PopID();
            }
            static const int weaponDiscountPrices[] = { 5,500,1250,1500,5000 };
            static const int weaponMoneyPrices[] = { 5,500,1250,1500,25000 };
            static const int weaponArmamentCosts[] = { 0,0,0,0,11 };
            static const int weaponMaterialCosts[] = { 0,0,0,0,21 };
            for (int tierValue = 0; tierValue < static_cast<int>(WeaponTier::Unknown); ++tierValue)
            {
                const WeaponTier tier = static_cast<WeaponTier>(tierValue);
                if (m_ownedWeapons[static_cast<std::size_t>(tier)]) continue;
                ImGui::PushID(2000 + tierValue);
                ImGui::Text(u8"%s", GetWeaponName(tier)); ImGui::SameLine();
                if (ImGui::SmallButton(u8"金のみ##weaponMoney")) TryBuyWeapon(tier, false);
                if (weaponArmamentCosts[tierValue] > 0 || weaponMaterialCosts[tierValue] > 0)
                {
                    ImGui::SameLine();
                    const bool canBuy = m_money >= weaponDiscountPrices[tierValue] &&
                        countCarriedRelics(RelicType::ArmamentUpgrade) >= weaponArmamentCosts[tierValue] &&
                        countCarriedRelics(RelicType::WeaponUpgrade) >= weaponMaterialCosts[tierValue];
                    ImGui::BeginDisabled(!canBuy);
                    if (ImGui::SmallButton(u8"素材併用##weaponMaterial"))
                    {
                        m_money -= weaponDiscountPrices[tierValue];
                        consumeCarriedRelics(RelicType::ArmamentUpgrade, weaponArmamentCosts[tierValue]);
                        consumeCarriedRelics(RelicType::WeaponUpgrade, weaponMaterialCosts[tierValue]);
                        m_ownedWeapons[static_cast<std::size_t>(tier)] = true;
                        SaveProgress();
                    }
                    ImGui::EndDisabled();
                }
                ImGui::TextDisabled(u8"  金のみ %dG / 素材併用 %dG（武具%d・武器%d）",
                    weaponMoneyPrices[tierValue], weaponDiscountPrices[tierValue],
                    weaponArmamentCosts[tierValue], weaponMaterialCosts[tierValue]);
                ImGui::PopID();
            }
            ImGui::Separator();
            if (!m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Unknown)])
            { ImGui::Text(u8"未知の装備-頭- %dG", kUnknownHeadPrice); ImGui::SameLine(); if (ImGui::SmallButton(u8"購入##unknownHead") && m_money >= kUnknownHeadPrice) { m_money -= kUnknownHeadPrice; m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Unknown)] = true; SaveProgress(); } }
            if (!m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Unknown)])
            { ImGui::Text(u8"未知の装備-胴- %dG", kUnknownBodyPrice); ImGui::SameLine(); if (ImGui::SmallButton(u8"購入##unknownBody") && m_money >= kUnknownBodyPrice) { m_money -= kUnknownBodyPrice; m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Unknown)] = true; SaveProgress(); } }
            if (!m_ownedWeapons[static_cast<std::size_t>(WeaponTier::Unknown)])
            { ImGui::Text(u8"未知の武器 %dG", kUnknownWeaponPrice); ImGui::SameLine(); if (ImGui::SmallButton(u8"購入##unknownWeapon") && m_money >= kUnknownWeaponPrice) { m_money -= kUnknownWeaponPrice; m_ownedWeapons[static_cast<std::size_t>(WeaponTier::Unknown)] = true; SaveProgress(); } }
            ImGui::TextWrapped(u8"未知の装備は頭と胴を揃えて装備した時だけ効果を発揮します。装備変更は自宅で行います。");
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(u8"レストラン"))
        {
            ImGui::TextWrapped(u8"200Gで最大HPと満腹度の80%%分を回復します。精神力は回復しません。");
            if (ImGui::Button(u8"食事をする（200G）")) TryUseForwardBaseMeal();
            ImGui::Separator();
            if (ImGui::Button(u8"安全な水を直接飲む（水分+25）"))
            { DrinkWater(kWaterDrinkAmount, WaterQuality::Safe, 0.0f); SaveProgress(); }
            if (ImGui::Button(u8"空の水筒1本を満たす")) FillSafeWater(false);
            ImGui::EndTabItem();
        }
            ImGui::EndTabBar();
        }
    }, Mode::Explore, u8"拠点を出る");
}

void SceneNarakuProto::DrawAbyssEntrance()
{
    ImGui::SetNextWindowPos(ImVec2(400.0f, 170.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(560.0f, 430.0f), ImGuiCond_Always);
    NarakuUi::Begin(u8"奈落塔出入口", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    ImGui::Text(u8"階級: %s  所持金: %dG", GetRankName(m_adventurerRank), m_money);
    DrawCurrentStatus();
    ImGui::Separator();
    ImGui::TextWrapped(u8"潜行準備は自宅でのみ変更できます。通常潜行、または解放済みの直通門を選択してください。");
    if (ImGui::Button(u8"通常潜行：第一層 上層", ImVec2(260.0f, 0.0f))) StartDive(1);

    const int secondFee = 500 + 250 * m_directGateWeeklyUses[0];
    ImGui::BeginDisabled(static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Black));
    if (ImGui::Button(u8"第二層 上層へ直通", ImVec2(260.0f, 0.0f))) StartDive(2);
    ImGui::SameLine(); ImGui::Text(u8"%dG（今週%d回利用）", secondFee, m_directGateWeeklyUses[0]);
    ImGui::EndDisabled();
    if (static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Black))
        ImGui::TextDisabled(u8"黒印以上で解放");

    const int thirdFee = 2000 + 250 * m_directGateWeeklyUses[1];
    ImGui::BeginDisabled(static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Purple));
    if (ImGui::Button(u8"第三層 上層へ直通", ImVec2(260.0f, 0.0f))) StartDive(3);
    ImGui::SameLine(); ImGui::Text(u8"%dG（今週%d回利用）", thirdFee, m_directGateWeeklyUses[1]);
    ImGui::EndDisabled();
    if (static_cast<int>(m_adventurerRank) < static_cast<int>(AdventurerRank::Purple))
        ImGui::TextDisabled(u8"紫印以上で解放");

    ImGui::Separator();
    if (NarakuUi::BackButton(u8"地上へ戻る", ImVec2(140.0f, 0.0f))) m_mode = Mode::Surface;
    ImGui::End();
}

void SceneNarakuProto::DrawSurfaceFacilityMarkers()
{
    const auto facility = std::min_element(m_surfaceFacilities.begin(), m_surfaceFacilities.end(),
        [this](const SurfaceFacilityState& lhs, const SurfaceFacilityState& rhs)
        { return Distance(lhs.interactionPoint, m_player.pos) < Distance(rhs.interactionPoint, m_player.pos); });
    if (facility == m_surfaceFacilities.end() || Distance(facility->interactionPoint, m_player.pos) > 3.5f) return;
    const char* name = u8"施設";
    if (facility->type == NarakuPiece::SurfaceFacilityType::Home) name = u8"自宅";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::Shop) name = u8"商店";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::Armory) name = u8"武具屋";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk) name = u8"レストラン・受注受付";
    else if (facility->type == NarakuPiece::SurfaceFacilityType::AbyssEntrance) name = u8"奈落塔出入口";
    ImGui::SetNextWindowPos(ImVec2(500.0f, 620.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.78f);
    ImGui::Begin(u8"地上施設案内", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::Text(u8"[F] %sを利用", name);
    ImGui::End();
}

const char* SceneNarakuProto::GetRankName(AdventurerRank rank) const
{
    switch (rank)
    {
    case AdventurerRank::Red: return u8"赤印";
    case AdventurerRank::Blue: return u8"青印";
    case AdventurerRank::Black: return u8"黒印";
    case AdventurerRank::Purple: return u8"紫印";
    default: return u8"不明";
    }
}

int SceneNarakuProto::GetRankMaximumDepth(AdventurerRank rank) const
{
    return std::min(5, static_cast<int>(rank) + 2);
}

const char* SceneNarakuProto::GetQuestName(QuestType type) const
{
    switch (type)
    {
    case QuestType::LostProperty: return u8"落とし物探し";
    case QuestType::RequestedRelic: return u8"これが欲しい！";
    case QuestType::Hunt: return u8"討伐";
    case QuestType::EnemySurvey: return u8"敵影調査";
    case QuestType::MiningSurvey: return u8"採掘地調査";
    case QuestType::UpgradePlan: return u8"強化計画";
    case QuestType::ValuableRelics: return u8"金目の物";
    case QuestType::SmallThings: return u8"塵も積もれば";
    case QuestType::Rescue: return u8"あの人が帰ってこない……";
    case QuestType::ScrapCollection: return u8"廃品回収";
    default: return u8"不明な依頼";
    }
}

std::string SceneNarakuProto::GetQuestDescription(const QuestRecord& quest) const
{
    std::ostringstream text;
    text << u8"対象: 第" << quest.targetDepth << u8"層\n";
    switch (quest.type)
    {
    case QuestType::LostProperty: text << u8"現地に落ちている依頼品を回収する。"; break;
    case QuestType::RequestedRelic: text << GetRelicTypeName(static_cast<RelicType>(quest.targetRelicType)) << u8"を1個譲渡する。"; break;
    case QuestType::Hunt: text << (quest.targetEnemyType == static_cast<int>(EnemyType::Territory) ? u8"縄張り型" : u8"突進型") << u8"を" << quest.targetCount << u8"体討伐する。"; break;
    case QuestType::EnemySurvey: text << u8"敵を" << quest.targetCount << u8"体発見する。"; break;
    case QuestType::MiningSurvey: text << u8"採掘地点を" << quest.targetCount << u8"箇所発見する。"; break;
    case QuestType::UpgradePlan: text << u8"受注後に強化素材を" << quest.targetCount << u8"個持ち帰る。"; break;
    case QuestType::ValuableRelics: text << u8"換金用遺物（高）を3個譲渡する。"; break;
    case QuestType::SmallThings: text << u8"換金用遺物（低）を10個譲渡する。"; break;
    case QuestType::Rescue: text << u8"食料" << 3 * quest.targetDepth << u8"個と水" << 20 * quest.targetDepth << u8"を届ける。"; break;
    case QuestType::ScrapCollection: text << u8"壊れた遺物を5個譲渡する。"; break;
    default: break;
    }
    return text.str();
}

void SceneNarakuProto::EnsureQuestBoard()
{
#if defined(NARAKU_EDITOR_BUILD)
    return;
#endif
    if (m_weekSeed == 0) m_weekSeed = 1;
    if (m_quests.size() < kQuestBoardSize) m_quests.resize(kQuestBoardSize);
    for (std::size_t index = 0; index < kQuestBoardSize; ++index)
        if (m_quests[index].id == 0) GenerateQuestForSlot(index);
    if (m_selectedQuest >= static_cast<int>(m_quests.size())) m_selectedQuest = 0;
}

void SceneNarakuProto::GenerateQuestForSlot(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size()) return;
    std::mt19937 random(static_cast<unsigned int>(m_weekSeed ^ (m_nextQuestId * 0x9E3779B97F4A7C15ull)));
    auto range = [&random](int minimum, int maximum)
    { return std::uniform_int_distribution<int>(minimum, maximum)(random); };
    std::array<int, static_cast<std::size_t>(QuestType::Count)> typeCounts = {};
    for (std::size_t index = 0; index < m_quests.size(); ++index)
        if (index != slotIndex && m_quests[index].id != 0 && m_quests[index].status != QuestStatus::Cooldown)
            ++typeCounts[static_cast<std::size_t>(m_quests[index].type)];
    int typeValue = range(0, static_cast<int>(QuestType::Count) - 1);
    for (int attempt = 0; attempt < 32 && typeCounts[static_cast<std::size_t>(typeValue)] >= 2; ++attempt)
        typeValue = range(0, static_cast<int>(QuestType::Count) - 1);
    if (typeCounts[static_cast<std::size_t>(typeValue)] >= 2)
        for (int candidate = 0; candidate < static_cast<int>(QuestType::Count); ++candidate)
            if (typeCounts[static_cast<std::size_t>(candidate)] < 2) { typeValue = candidate; break; }

    QuestRecord quest;
    quest.id = m_nextQuestId++;
    quest.type = static_cast<QuestType>(typeValue);
    quest.targetDepth = range(1, std::max(1, std::min(m_maxReachedDepth, GetRankMaximumDepth(m_adventurerRank))));
    const float depthRatio = static_cast<float>(quest.targetDepth - 1) / 4.0f;
    auto interpolate = [depthRatio](int minimum, int maximum)
    { return static_cast<int>(std::llround(minimum + (maximum - minimum) * depthRatio)); };
    switch (quest.type)
    {
    case QuestType::LostProperty: quest.reward = interpolate(1000, 5000); quest.remainingSeconds = interpolate(30, 60) * 60.0; break;
    case QuestType::RequestedRelic: quest.reward = interpolate(1000, 3000); quest.remainingSeconds = 20.0 * 60.0; quest.targetRelicType = range(0, static_cast<int>(RelicType::MentalRecovery)); break;
    case QuestType::Hunt: quest.reward = interpolate(2000, 6000); quest.remainingSeconds = interpolate(30, 90) * 60.0; quest.targetCount = range(1, 5); quest.targetEnemyType = range(0, 1); break;
    case QuestType::EnemySurvey: quest.reward = interpolate(300, 500); quest.remainingSeconds = interpolate(30, 60) * 60.0; quest.targetCount = range(2, 8); break;
    case QuestType::MiningSurvey: quest.reward = interpolate(100, 300); quest.remainingSeconds = interpolate(30, 45) * 60.0; quest.targetCount = range(2, 8); break;
    case QuestType::UpgradePlan: quest.reward = interpolate(1050, 2450); quest.remainingSeconds = interpolate(60, 120) * 60.0; quest.targetCount = range(3, 7); break;
    case QuestType::ValuableRelics: quest.reward = 900 * quest.targetDepth; quest.remainingSeconds = interpolate(30, 45) * 60.0; quest.targetCount = 3; quest.targetRelicType = static_cast<int>(RelicType::CashHigh); break;
    case QuestType::SmallThings: quest.reward = 1000 * quest.targetDepth; quest.remainingSeconds = interpolate(20, 40) * 60.0; quest.targetCount = 10; quest.targetRelicType = static_cast<int>(RelicType::CashLow); break;
    case QuestType::Rescue: quest.reward = interpolate(1000, 3000); quest.remainingSeconds = interpolate(60, 300) * 60.0; break;
    case QuestType::ScrapCollection: quest.reward = 100 * quest.targetDepth; quest.remainingSeconds = 30.0 * 60.0; quest.targetCount = 5; break;
    default: break;
    }
    std::vector<int> candidates;
    for (int area = 0; area < static_cast<int>(m_areas.size()); ++area)
        if (m_areas[area].depth == quest.targetDepth) candidates.push_back(area);
    if (!candidates.empty()) quest.targetAreaIndex = candidates[static_cast<std::size_t>(range(0, static_cast<int>(candidates.size()) - 1))];
    if (quest.type == QuestType::Rescue)
    {
        quest.targetAreaIndex = -1;
        quest.targetPositionReady = false;
    }
    const bool smallReset = quest.type == QuestType::RequestedRelic || quest.type == QuestType::UpgradePlan ||
        quest.type == QuestType::SmallThings || quest.type == QuestType::ScrapCollection;
    const bool endReset = quest.type == QuestType::Rescue;
    quest.cooldownSeconds = endReset ? std::max(1.0, kGameWeekSeconds - m_gameWeekSeconds)
        : (smallReset ? 3600.0 : (quest.type == QuestType::Hunt ? 18000.0 : 10800.0));
    if (quest.type == QuestType::LostProperty) quest.cooldownSeconds = quest.targetDepth <= 2 ? 3600.0 : 10800.0;
    m_quests[slotIndex] = quest;
}

void SceneNarakuProto::ResetAvailableQuestsForNewWeek()
{
    for (std::size_t index = 0; index < m_quests.size(); ++index)
        if (m_quests[index].status == QuestStatus::Available || m_quests[index].status == QuestStatus::Cooldown)
            GenerateQuestForSlot(index);
    ResetPromotionQuestFailuresForNewWeek();
}

void SceneNarakuProto::BeginNewGameWeek()
{
    m_weekSeed = m_weekSeed * 6364136223846793005ull + 1442695040888963407ull;
    m_weekResetPending = true;
    m_directGateWeeklyUses.fill(0);
    m_directGateTargetAreas.fill(-1);
    m_directGateTargetPositions.fill({});
    ResetAvailableQuestsForNewWeek();
    ShowCenterNotification(u8"週が変わりました。次回潜行から奈落の構成が更新されます。");
}

void SceneNarakuProto::UpdateWorldClockAndQuests(float dt)
{
#if defined(NARAKU_EDITOR_BUILD)
    return;
#endif
    if (m_mode != Mode::Explore && m_mode != Mode::Surface) return;
    if (m_mode == Mode::Surface && !m_surfaceWasMoving) return;
    if (m_mode == Mode::Explore)
    {
        UpdateRespawns(dt);
        UpdateMiningRespawns(dt);
    }
    const double worldDelta = dt * kGameTimeScale * (m_mode == Mode::Explore
        ? kWorldTimeDepthMultipliers[static_cast<std::size_t>(GetCurrentDepth() - 1)] : 1.0);
    UpdateFishingPointRecharge(worldDelta);
    m_gameWeekSeconds += worldDelta;
    while (m_gameWeekSeconds >= kGameWeekSeconds)
    {
        m_gameWeekSeconds -= kGameWeekSeconds;
        BeginNewGameWeek();
    }
    for (std::size_t index = 0; index < m_quests.size(); ++index)
    {
        QuestRecord& quest = m_quests[index];
        if (quest.status == QuestStatus::Available)
        {
            quest.cooldownSeconds -= worldDelta;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
        else if (quest.status == QuestStatus::Active)
        {
            quest.remainingSeconds -= dt;
            if (quest.remainingSeconds <= 0.0) FailQuest(index);
        }
        else if (quest.status == QuestStatus::Cooldown)
        {
            quest.cooldownSeconds -= dt;
            if (quest.cooldownSeconds <= 0.0) GenerateQuestForSlot(index);
        }
    }
}

bool SceneNarakuProto::ConsumeStoredRelicsForQuest(const QuestRecord& quest)
{
    int required = 0;
    int type = quest.targetRelicType;
    bool brokenOnly = false;
    if (quest.type == QuestType::RequestedRelic) required = 1;
    else if (quest.type == QuestType::ValuableRelics) required = 3;
    else if (quest.type == QuestType::SmallThings) required = 10;
    else if (quest.type == QuestType::ScrapCollection) { required = 5; brokenOnly = true; }
    else return true;
    auto matches = [type, brokenOnly](const RelicItem& item)
    { return brokenOnly ? item.broken : static_cast<int>(item.type) == type; };
    const int available = static_cast<int>(std::count_if(m_inventory.begin(), m_inventory.end(), matches) +
        std::count_if(m_storedInventory.begin(), m_storedInventory.end(), matches));
    if (available < required) return false;
    auto consume = [&](std::vector<RelicItem>& items)
    {
        for (auto it = items.begin(); it != items.end() && required > 0;)
        {
            if (matches(*it)) { it = items.erase(it); --required; }
            else ++it;
        }
    };
    consume(m_inventory);
    consume(m_storedInventory);
    for (std::size_t index = 0; index < m_loadoutRelics.size(); ++index)
        m_loadoutRelics[index] = std::min(m_loadoutRelics[index], CountStoredRelics(static_cast<RelicType>(index)));
    return true;
}

bool SceneNarakuProto::AcceptQuest(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size() || m_quests[slotIndex].status != QuestStatus::Available) return false;
    const int activeCount = static_cast<int>(std::count_if(m_quests.begin(), m_quests.end(), [](const QuestRecord& quest)
    { return quest.status == QuestStatus::Active || quest.status == QuestStatus::Complete; }));
    if (activeCount >= kMaximumActiveQuests) { ShowCenterNotification(u8"受注上限は3件です。"); return false; }
    QuestRecord& quest = m_quests[slotIndex];
    if (!ConsumeStoredRelicsForQuest(quest)) { ShowCenterNotification(u8"譲渡に必要な遺物が足りません。"); return false; }
    quest.status = QuestStatus::Active;
    quest.progress = 0;
    quest.acceptedAcquisitionOrder = m_nextRelicAcquisitionOrder;
    if (quest.type == QuestType::RequestedRelic || quest.type == QuestType::ValuableRelics ||
        quest.type == QuestType::SmallThings || quest.type == QuestType::ScrapCollection)
    {
        quest.progress = quest.targetCount;
        quest.status = QuestStatus::Complete;
    }
    if (quest.type == QuestType::Rescue)
    {
        std::vector<int> candidates;
        for (int area = 0; area < static_cast<int>(m_areas.size()); ++area)
            if (m_areas[area].depth == quest.targetDepth) candidates.push_back(area);
        if (!candidates.empty()) quest.targetAreaIndex = candidates[static_cast<std::size_t>(quest.id % candidates.size())];
        quest.targetPositionReady = false;
    }
    SaveProgress();
    return true;
}

void SceneNarakuProto::ReportQuest(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size() || m_quests[slotIndex].status != QuestStatus::Complete) return;
    m_money += m_quests[slotIndex].reward;
    if (m_quests[slotIndex].rewardItemType >= 0 && m_quests[slotIndex].rewardItemType < static_cast<int>(RelicType::Count))
    {
        for (int count = 0; count < m_quests[slotIndex].rewardItemCount; ++count)
        {
            RelicItem item = CreateRelic(static_cast<RelicType>(m_quests[slotIndex].rewardItemType), u8"依頼報酬");
            item.stabilized = true;
            item.acquisitionOrder = m_nextRelicAcquisitionOrder++;
            m_storedInventory.push_back(item);
        }
    }
    m_quests[slotIndex].status = QuestStatus::Cooldown;
    m_quests[slotIndex].cooldownSeconds = kQuestSlotCooldownSeconds;
    ShowCenterNotification(u8"依頼を報告し、報酬を受け取りました。");
    SaveProgress();
}

bool SceneNarakuProto::ForceSellOneLowestValueItem(int& remainingPenalty)
{
    enum class SaleKind { None, StoredRelic, CarriedRelic, Head, Body, Weapon };
    SaleKind kind = SaleKind::None;
    std::size_t selected = 0;
    int value = std::numeric_limits<int>::max();
    for (std::size_t index = 0; index < m_storedInventory.size(); ++index)
        if (IsRelicSellable(m_storedInventory[index]) && m_storedInventory[index].value > 0 && m_storedInventory[index].value < value)
        { kind = SaleKind::StoredRelic; selected = index; value = m_storedInventory[index].value; }
    for (std::size_t index = 0; index < m_inventory.size(); ++index)
        if (IsRelicSellable(m_inventory[index]) && m_inventory[index].value > 0 && m_inventory[index].value < value)
        { kind = SaleKind::CarriedRelic; selected = index; value = m_inventory[index].value; }
    static const int armorMoneyPrices[7][2] = { {10,15},{150,200},{1000,1500},{1250,1750},{1750,2000},{25000,30000},{kUnknownHeadPrice,kUnknownBodyPrice} };
    static const int weaponMoneyPrices[6] = { 5,500,1250,1500,25000,kUnknownWeaponPrice };
    for (std::size_t index = 0; index < 7; ++index)
    {
        const int headValue = armorMoneyPrices[index][0] / 10;
        if (m_ownedHeadArmor[index] && headValue > 0 && headValue < value) { kind = SaleKind::Head; selected = index; value = headValue; }
        const int bodyValue = armorMoneyPrices[index][1] / 10;
        if (m_ownedBodyArmor[index] && bodyValue > 0 && bodyValue < value) { kind = SaleKind::Body; selected = index; value = bodyValue; }
    }
    for (std::size_t index = 0; index < 6; ++index)
    {
        const int weaponValue = weaponMoneyPrices[index] / 10;
        if (m_ownedWeapons[index] && weaponValue > 0 && weaponValue < value) { kind = SaleKind::Weapon; selected = index; value = weaponValue; }
    }
    if (kind == SaleKind::None) return false;
    if (kind == SaleKind::StoredRelic)
    {
        const RelicType type = m_storedInventory[selected].type;
        m_storedInventory.erase(m_storedInventory.begin() + selected);
        const std::size_t typeIndex = static_cast<std::size_t>(type);
        m_loadoutRelics[typeIndex] = std::min(m_loadoutRelics[typeIndex], CountStoredRelics(type));
    }
    else if (kind == SaleKind::CarriedRelic) m_inventory.erase(m_inventory.begin() + selected);
    else if (kind == SaleKind::Head) { m_ownedHeadArmor[selected] = false; if (m_equippedHeadArmor == static_cast<ArmorTier>(selected)) m_equippedHeadArmor = ArmorTier::None; }
    else if (kind == SaleKind::Body) { m_ownedBodyArmor[selected] = false; if (m_equippedBodyArmor == static_cast<ArmorTier>(selected)) m_equippedBodyArmor = ArmorTier::None; }
    else { m_ownedWeapons[selected] = false; if (m_equippedWeapon == static_cast<WeaponTier>(selected)) m_equippedWeapon = WeaponTier::None; }
    remainingPenalty = std::max(0, remainingPenalty - value);
    return true;
}

void SceneNarakuProto::ApplyQuestPenalty(int penalty)
{
    const int paid = std::min(m_money, penalty);
    m_money -= paid;
    int remaining = penalty - paid;
    while (remaining > 0 && ForceSellOneLowestValueItem(remaining)) {}
    m_questDebt += remaining;
}

void SceneNarakuProto::FailQuest(std::size_t slotIndex)
{
    if (slotIndex >= m_quests.size() || m_quests[slotIndex].status != QuestStatus::Active) return;
    ApplyQuestPenalty(static_cast<int>(std::llround(m_quests[slotIndex].reward * 0.1)));
    m_quests[slotIndex].status = QuestStatus::Cooldown;
    m_quests[slotIndex].cooldownSeconds = kQuestSlotCooldownSeconds;
    ShowCenterNotification(u8"依頼の期限が切れ、違約金が発生しました。");
    SaveProgress();
}

void SceneNarakuProto::UpdateQuestDiscoveryProgress(bool enemyDiscovered, bool miningDiscovered, int depth)
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.targetDepth != depth) continue;
        if ((enemyDiscovered && quest.type == QuestType::EnemySurvey) ||
            (miningDiscovered && quest.type == QuestType::MiningSurvey))
        {
            quest.progress = std::min(quest.targetCount, quest.progress + 1);
            if (quest.progress >= quest.targetCount) quest.status = QuestStatus::Complete;
        }
    }
}

void SceneNarakuProto::UpdateQuestReturnProgress(const std::vector<RelicItem>& returnedItems)
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.type != QuestType::UpgradePlan) continue;
        for (const RelicItem& item : returnedItems)
        {
            const bool upgrade = item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade || item.type == RelicType::ArmorUpgrade;
            if (upgrade && item.acquisitionOrder >= quest.acceptedAcquisitionOrder) ++quest.progress;
        }
        quest.progress = std::min(quest.progress, quest.targetCount);
        if (quest.progress >= quest.targetCount) quest.status = QuestStatus::Complete;
    }
}

void SceneNarakuProto::InitializeImportantQuests()
{
    m_importantQuests.fill({});
    UnlockImportantQuest(ImportantQuestType::FirstJourney);
    UnlockImportantQuest(ImportantQuestType::FirstHunt);
    UnlockImportantQuest(ImportantQuestType::FirstTerritoryHunt);
    UnlockImportantQuest(ImportantQuestType::FindSecondBase);
    UnlockImportantQuest(ImportantQuestType::FindForwardBase);
    UnlockImportantQuest(ImportantQuestType::ReturnUniqueRelic);
    m_selectedImportantQuest = 0;
}

void SceneNarakuProto::UnlockImportantQuest(ImportantQuestType type)
{
    ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
    if (quest.status == ImportantQuestStatus::Locked) quest.status = ImportantQuestStatus::Active;
}

const char* SceneNarakuProto::GetImportantQuestName(ImportantQuestType type) const
{
    switch (type)
    {
    case ImportantQuestType::FirstJourney: return u8"初めての旅立ち";
    case ImportantQuestType::FirstClear: return u8"最初が肝心";
    case ImportantQuestType::ReachSecond: return u8"出発！第二層";
    case ImportantQuestType::ClearSecond: return u8"第二層制覇";
    case ImportantQuestType::ReachThird: return u8"中層到達";
    case ImportantQuestType::ClearThird: return u8"折り返し地点？";
    case ImportantQuestType::ReachFourth: return u8"先の見えない遠足";
    case ImportantQuestType::ClearFourth: return u8"ここが中間地点";
    case ImportantQuestType::ReachFifth: return u8"旅の終着点。そして始まり";
    case ImportantQuestType::ClearFifth: return u8"奈落への入り口";
    case ImportantQuestType::FirstHunt: return u8"初めての討伐";
    case ImportantQuestType::FiveHunts: return u8"連続討伐";
    case ImportantQuestType::ThirteenHunts: return u8"掃討";
    case ImportantQuestType::FirstTerritoryHunt: return u8"縄張り争いの勝者";
    case ImportantQuestType::ThreeTerritoryHunts: return u8"ここを拠点とする！";
    case ImportantQuestType::FindSecondBase: return u8"第二拠点";
    case ImportantQuestType::FindForwardBase: return u8"前衛拠点。到着";
    case ImportantQuestType::ReturnUniqueRelic: return u8"特異な物体";
    default: return u8"不明";
    }
}

const char* SceneNarakuProto::GetImportantQuestDescription(ImportantQuestType type) const
{
    switch (type)
    {
    case ImportantQuestType::FirstJourney: return u8"第一層上層へ到達する。";
    case ImportantQuestType::FirstClear: return u8"第一層の全エリアと採掘地点の7割を発見する。";
    case ImportantQuestType::ReachSecond: return u8"第二層上層へ到達する。";
    case ImportantQuestType::ClearSecond: return u8"第二層を踏破する。";
    case ImportantQuestType::ReachThird: return u8"第三層上層へ到達する。";
    case ImportantQuestType::ClearThird: return u8"第三層を踏破する。";
    case ImportantQuestType::ReachFourth: return u8"第四層上層へ到達する。";
    case ImportantQuestType::ClearFourth: return u8"第四層を踏破する。";
    case ImportantQuestType::ReachFifth: return u8"第五層上層へ到達する。";
    case ImportantQuestType::ClearFifth: return u8"第五層を踏破する。";
    case ImportantQuestType::FirstHunt: return u8"種類を問わず敵を1体討伐する。";
    case ImportantQuestType::FiveHunts: return u8"1回の潜行で敵を5体討伐する。";
    case ImportantQuestType::ThirteenHunts: return u8"1回の潜行で敵を13体討伐する。";
    case ImportantQuestType::FirstTerritoryHunt: return u8"縄張り型を1体討伐する。";
    case ImportantQuestType::ThreeTerritoryHunts: return u8"縄張り型を3体討伐する。";
    case ImportantQuestType::FindSecondBase: return u8"第二層中層の第二拠点を発見する。";
    case ImportantQuestType::FindForwardBase: return u8"第五層下層の前衛拠点へ到達する。";
    case ImportantQuestType::ReturnUniqueRelic: return u8"固有遺物を初めて生還して持ち帰る。";
    default: return "";
    }
}

int SceneNarakuProto::GetImportantQuestMoneyReward(ImportantQuestType type) const
{
    static constexpr int values[] = { 100,1000,100,2500,100,5000,100,7500,1000,10000,200,1000,2000,700,3000,100,1000,1000 };
    return values[static_cast<std::size_t>(type)];
}

int SceneNarakuProto::GetImportantQuestExpReward(ImportantQuestType type) const
{
    static constexpr int values[] = { 10,10,20,3000,20,5000,20,7500,200,17500,0,0,0,0,0,0,0,0 };
    return values[static_cast<std::size_t>(type)];
}

int SceneNarakuProto::GetImportantQuestFoodReward(ImportantQuestType type) const
{
    static constexpr int values[] = { 10,20,15,15,15,15,15,25,15,30,5,10,20,4,20,0,0,0 };
    return values[static_cast<std::size_t>(type)];
}

int SceneNarakuProto::GetImportantQuestWaterBottleReward(ImportantQuestType type) const
{
    return type == ImportantQuestType::FirstJourney ? 1 : 0;
}

bool SceneNarakuProto::IsQuestDeskUnlocked() const
{
    return m_importantQuests[static_cast<std::size_t>(ImportantQuestType::FirstJourney)].status >=
        ImportantQuestStatus::Complete;
}

void SceneNarakuProto::UpdateImportantQuestArrival()
{
    if (m_currentAreaIndex < 0 || m_currentAreaIndex >= static_cast<int>(m_areas.size())) return;
    const AreaState& area = m_areas[static_cast<std::size_t>(m_currentAreaIndex)];
    const auto complete = [this](ImportantQuestType type)
    {
        ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
        if (quest.status != ImportantQuestStatus::Active) return;
        quest.progress = 1;
        quest.status = ImportantQuestStatus::Complete;
        ShowCenterNotification(u8"重要クエストの条件を達成しました。");
    };
    if (area.sublayer == 0)
    {
        if (area.depth == 1) complete(ImportantQuestType::FirstJourney);
        else if (area.depth == 2) complete(ImportantQuestType::ReachSecond);
        else if (area.depth == 3) complete(ImportantQuestType::ReachThird);
        else if (area.depth == 4) complete(ImportantQuestType::ReachFourth);
        else if (area.depth == 5) complete(ImportantQuestType::ReachFifth);
    }
    UpdateImportantQuestExploration();
}

void SceneNarakuProto::UpdateImportantQuestExploration()
{
    const auto complete = [this](ImportantQuestType type)
    {
        ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
        if (quest.status != ImportantQuestStatus::Active) return;
        quest.progress = 1;
        quest.status = ImportantQuestStatus::Complete;
        ShowCenterNotification(u8"重要クエストの条件を達成しました。");
    };

    for (int depth = 1; depth <= 5; ++depth)
    {
        bool allReached = true;
        int miningTotal = 0;
        int miningDiscovered = 0;
        for (int areaIndex = 0; areaIndex < static_cast<int>(m_areas.size()); ++areaIndex)
        {
            const AreaState& area = m_areas[static_cast<std::size_t>(areaIndex)];
            if (area.depth != depth) continue;
            if (!area.generated) allReached = false;
            const std::vector<MiningPoint>& points = areaIndex == m_currentAreaIndex ? m_miningPoints : area.miningPoints;
            miningTotal += static_cast<int>(points.size());
            miningDiscovered += static_cast<int>(std::count_if(points.begin(), points.end(), [](const MiningPoint& point) { return point.discovered; }));
        }
        if (!allReached || miningTotal <= 0 || miningDiscovered * 10 < miningTotal * 7) continue;
        if (depth == 1) complete(ImportantQuestType::FirstClear);
        else if (depth == 2) complete(ImportantQuestType::ClearSecond);
        else if (depth == 3) complete(ImportantQuestType::ClearThird);
        else if (depth == 4) complete(ImportantQuestType::ClearFourth);
        else complete(ImportantQuestType::ClearFifth);
    }

    for (const NarakuMap::BasePoint& base : m_runtimeMap.bases)
    {
        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, base.layerId);
        if (layerIndex < 0 || std::fabs(m_runtimeMap.terrainLayers[static_cast<std::size_t>(layerIndex)].layerDepth - m_player.depth) > 0.35f ||
            Distance({ base.xz.x, base.xz.z }, m_player.pos) > kDiscoveryRange) continue;
        complete(base.type == NarakuMap::BaseType::SecondBase
            ? ImportantQuestType::FindSecondBase : ImportantQuestType::FindForwardBase);
    }
}

void SceneNarakuProto::UpdateImportantQuestDefeat(const EnemyState& enemy)
{
    const int totalKills = std::accumulate(m_result.chargerKillsByDepth.begin(), m_result.chargerKillsByDepth.end(), 0) +
        std::accumulate(m_result.territoryKillsByDepth.begin(), m_result.territoryKillsByDepth.end(), 0);
    const int territoryKills = std::accumulate(m_result.territoryKillsByDepth.begin(), m_result.territoryKillsByDepth.end(), 0);
    const auto update = [this](ImportantQuestType type, int progress, int required)
    {
        ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(type)];
        if (quest.status != ImportantQuestStatus::Active) return;
        quest.progress = std::min(required, progress);
        if (quest.progress >= required) quest.status = ImportantQuestStatus::Complete;
    };
    update(ImportantQuestType::FirstHunt, totalKills, 1);
    update(ImportantQuestType::FiveHunts, totalKills, 5);
    update(ImportantQuestType::ThirteenHunts, totalKills, 13);
    if (enemy.type == EnemyType::Territory)
    {
        update(ImportantQuestType::FirstTerritoryHunt, territoryKills, 1);
        update(ImportantQuestType::ThreeTerritoryHunts, territoryKills, 3);
    }
}

void SceneNarakuProto::UpdateImportantQuestUniqueReturn()
{
    ImportantQuestRecord& quest = m_importantQuests[static_cast<std::size_t>(ImportantQuestType::ReturnUniqueRelic)];
    if (quest.status == ImportantQuestStatus::Active)
    {
        quest.progress = 1;
        quest.status = ImportantQuestStatus::Complete;
    }
}

void SceneNarakuProto::ReportImportantQuest(std::size_t questIndex)
{
    if (questIndex >= m_importantQuests.size()) return;
    ImportantQuestRecord& quest = m_importantQuests[questIndex];
    if (quest.status != ImportantQuestStatus::Complete) return;
    const ImportantQuestType type = static_cast<ImportantQuestType>(questIndex);
    m_money += GetImportantQuestMoneyReward(type);
    AwardExp(GetImportantQuestExpReward(type));
    m_storedFoodCount += GetImportantQuestFoodReward(type);
    for (int count = 0; count < GetImportantQuestWaterBottleReward(type); ++count)
        m_storedWaterBottles.push_back({});
    quest.status = ImportantQuestStatus::Claimed;

    if (type == ImportantQuestType::FirstJourney) { UnlockImportantQuest(ImportantQuestType::FirstClear); UnlockImportantQuest(ImportantQuestType::ReachSecond); }
    else if (type == ImportantQuestType::ReachSecond) { UnlockImportantQuest(ImportantQuestType::ClearSecond); UnlockImportantQuest(ImportantQuestType::ReachThird); }
    else if (type == ImportantQuestType::ReachThird) { UnlockImportantQuest(ImportantQuestType::ClearThird); UnlockImportantQuest(ImportantQuestType::ReachFourth); }
    else if (type == ImportantQuestType::ReachFourth) { UnlockImportantQuest(ImportantQuestType::ClearFourth); UnlockImportantQuest(ImportantQuestType::ReachFifth); }
    else if (type == ImportantQuestType::ReachFifth) UnlockImportantQuest(ImportantQuestType::ClearFifth);
    else if (type == ImportantQuestType::FirstHunt) UnlockImportantQuest(ImportantQuestType::FiveHunts);
    else if (type == ImportantQuestType::FiveHunts) UnlockImportantQuest(ImportantQuestType::ThirteenHunts);
    else if (type == ImportantQuestType::FirstTerritoryHunt) UnlockImportantQuest(ImportantQuestType::ThreeTerritoryHunts);
    SaveProgress();
}

void SceneNarakuProto::InitializePromotionQuests()
{
    m_promotionQuests.fill({});
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        if (static_cast<int>(m_adventurerRank) >= static_cast<int>(index) + 1)
        {
            m_promotionQuests[index].status = PromotionQuestStatus::Claimed;
        }
    }
    m_selectedPromotionQuest = 0;
    RefreshPromotionQuestAvailability();
}

int SceneNarakuProto::GetPromotionQuestTargetDepth(std::size_t questIndex) const
{
    static constexpr int values[] = { 3, 4, 5 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestRequiredLevel(std::size_t questIndex) const
{
    static constexpr int values[] = { 20, 35, 55 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestRequiredCash(std::size_t questIndex) const
{
    static constexpr int values[] = { 3, 2, 3 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestRequiredUpgrade(std::size_t questIndex) const
{
    static constexpr int values[] = { 1, 5, 10 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestMoneyReward(std::size_t questIndex) const
{
    static constexpr int values[] = { 1000, 1500, 1000 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestExpReward(std::size_t questIndex) const
{
    static constexpr int values[] = { 1500, 3500, 5500 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

int SceneNarakuProto::GetPromotionQuestMaterialReward(std::size_t questIndex) const
{
    static constexpr int values[] = { 10, 5, 15 };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : 0;
}

const char* SceneNarakuProto::GetPromotionQuestName(std::size_t questIndex) const
{
    static constexpr const char* values[] = { u8"昇格試験！-青-", u8"昇格試験！-黒-", u8"昇格試験！-紫-" };
    return questIndex < m_promotionQuests.size() ? values[questIndex] : u8"不明";
}

void SceneNarakuProto::RefreshPromotionQuestAvailability()
{
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        PromotionQuestRecord& quest = m_promotionQuests[index];
        const int targetRank = static_cast<int>(index) + 1;
        if (static_cast<int>(m_adventurerRank) >= targetRank)
        {
            quest.status = PromotionQuestStatus::Claimed;
            continue;
        }
        if (quest.status != PromotionQuestStatus::Locked) continue;
        if (static_cast<int>(m_adventurerRank) == targetRank - 1 &&
            m_level >= GetPromotionQuestRequiredLevel(index) &&
            m_maxReachedDepth >= GetPromotionQuestTargetDepth(index))
        {
            quest.status = PromotionQuestStatus::Available;
        }
    }
}

void SceneNarakuProto::ResetPromotionQuestFailuresForNewWeek()
{
    for (PromotionQuestRecord& quest : m_promotionQuests)
    {
        if (quest.status == PromotionQuestStatus::FailedThisWeek)
        {
            quest = PromotionQuestRecord();
        }
    }
    RefreshPromotionQuestAvailability();
}

bool SceneNarakuProto::AcceptPromotionQuest(std::size_t questIndex)
{
    RefreshPromotionQuestAvailability();
    if (questIndex >= m_promotionQuests.size() ||
        m_promotionQuests[questIndex].status != PromotionQuestStatus::Available)
    {
        return false;
    }
    PromotionQuestRecord& quest = m_promotionQuests[questIndex];
    quest.status = PromotionQuestStatus::Active;
    quest.acceptedAcquisitionOrder = m_nextRelicAcquisitionOrder;
    quest.cashRelicProgress = 0;
    quest.upgradeRelicProgress = 0;
    ShowCenterNotification(u8"昇格試験を受注しました。指定深度から生還してください。");
    SaveProgress();
    return true;
}

void SceneNarakuProto::UpdatePromotionQuestReturn(const std::vector<RelicItem>& returnedItems)
{
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        PromotionQuestRecord& quest = m_promotionQuests[index];
        if (quest.status != PromotionQuestStatus::Active) continue;
        int cash = 0;
        int upgrade = 0;
        for (const RelicItem& item : returnedItems)
        {
            const auto depth = m_runRelicAcquisitionDepths.find(item.acquisitionOrder);
            if (item.acquisitionOrder < quest.acceptedAcquisitionOrder ||
                depth == m_runRelicAcquisitionDepths.end() ||
                depth->second != GetPromotionQuestTargetDepth(index))
            {
                continue;
            }
            if (item.type == RelicType::CashLow || item.type == RelicType::CashHigh) ++cash;
            else if (item.type == RelicType::ArmamentUpgrade || item.type == RelicType::WeaponUpgrade ||
                item.type == RelicType::ArmorUpgrade) ++upgrade;
        }
        quest.cashRelicProgress = std::min(cash, GetPromotionQuestRequiredCash(index));
        quest.upgradeRelicProgress = std::min(upgrade, GetPromotionQuestRequiredUpgrade(index));
        if (cash >= GetPromotionQuestRequiredCash(index) && upgrade >= GetPromotionQuestRequiredUpgrade(index))
        {
            quest.status = PromotionQuestStatus::Complete;
            ShowCenterNotification(u8"昇格試験の条件を達成しました。受付で報告できます。");
        }
        else
        {
            quest.status = PromotionQuestStatus::FailedThisWeek;
            ShowCenterNotification(u8"昇格試験に失敗しました。翌週まで再受注できません。");
        }
    }
}

void SceneNarakuProto::FailActivePromotionQuest()
{
    for (PromotionQuestRecord& quest : m_promotionQuests)
    {
        if (quest.status != PromotionQuestStatus::Active) continue;
        quest.status = PromotionQuestStatus::FailedThisWeek;
        ShowCenterNotification(u8"昇格試験に失敗しました。翌週まで再受注できません。");
    }
}

void SceneNarakuProto::ReportPromotionQuest(std::size_t questIndex)
{
    if (questIndex >= m_promotionQuests.size() ||
        m_promotionQuests[questIndex].status != PromotionQuestStatus::Complete)
    {
        return;
    }
    m_money += GetPromotionQuestMoneyReward(questIndex);
    AwardExp(GetPromotionQuestExpReward(questIndex));
    for (int count = 0; count < GetPromotionQuestMaterialReward(questIndex); ++count)
    {
        const RelicType type = static_cast<RelicType>(RandomInt(
            static_cast<int>(RelicType::ArmamentUpgrade), static_cast<int>(RelicType::ArmorUpgrade)));
        RelicItem item = CreateRelic(type, u8"昇格試験報酬");
        item.stabilized = true;
        m_storedInventory.push_back(item);
    }
    m_adventurerRank = static_cast<AdventurerRank>(static_cast<int>(questIndex) + 1);
    m_promotionQuests[questIndex].status = PromotionQuestStatus::Claimed;
    RefreshPromotionQuestAvailability();
    ShowCenterNotification(u8"昇格試験に合格し、階級が上がりました。");
    SaveProgress();
}

void SceneNarakuProto::EnsureQuestTargetPosition(QuestRecord& quest)
{
    if (quest.targetPositionReady || quest.targetAreaIndex != m_currentAreaIndex) return;
    quest.targetX = m_startPoint.x;
    quest.targetZ = m_startPoint.y;
    quest.targetLayerDepth = m_startDepth;
    if (!m_floorRegions.empty())
    {
        const FloorRegion& floor = m_floorRegions[static_cast<std::size_t>(quest.id % m_floorRegions.size())];
        quest.targetX = floor.center.x;
        quest.targetZ = floor.center.y;
        quest.targetLayerDepth = floor.depth;
    }
    quest.targetPositionReady = true;
}

bool SceneNarakuProto::TryInteractWithQuestTarget()
{
    for (QuestRecord& quest : m_quests)
    {
        if (quest.status != QuestStatus::Active || quest.targetAreaIndex != m_currentAreaIndex ||
            (quest.type != QuestType::LostProperty && quest.type != QuestType::Rescue)) continue;
        EnsureQuestTargetPosition(quest);
        if (!IsNear(m_player.pos, { quest.targetX, quest.targetZ }, kInteractRange) ||
            std::fabs(m_player.depth - quest.targetLayerDepth) > 0.35f) continue;
        if (quest.type == QuestType::Rescue)
        {
            const int foodRequired = 3 * quest.targetDepth;
            const float waterRequired = static_cast<float>(20 * quest.targetDepth);
            float water = 0.0f;
            for (const WaterBottle& bottle : m_waterBottles) if (bottle.quality != WaterQuality::None) water += bottle.amount;
            if (m_foodCount + m_heatedFoodCount < foodRequired || water < waterRequired)
            { ShowCenterNotification(u8"救助に必要な食料または水が足りません。"); return true; }
            int food = foodRequired;
            const int normal = std::min(food, m_foodCount); m_foodCount -= normal; food -= normal;
            m_heatedFoodCount -= food;
            float consumeWater = waterRequired;
            for (WaterBottle& bottle : m_waterBottles)
            {
                if (bottle.quality == WaterQuality::None || consumeWater <= 0.0f) continue;
                const float amount = std::min(consumeWater, bottle.amount);
                bottle.amount -= amount; consumeWater -= amount;
                if (bottle.amount <= 0.0f) { bottle.amount = 0.0f; bottle.quality = WaterQuality::None; bottle.foodPoisoningChance = 0.0f; }
            }
        }
        quest.progress = quest.targetCount;
        quest.targetInteracted = true;
        quest.status = QuestStatus::Complete;
        ShowCenterNotification(u8"依頼条件を達成しました。受付で報告できます。");
        SaveProgress();
        return true;
    }
    return false;
}

void SceneNarakuProto::DrawQuestTargets3D()
{
    for (QuestRecord& quest : m_quests)
    {
        const bool lostVisible = quest.type == QuestType::LostProperty && quest.status == QuestStatus::Available;
        const bool activeVisible = quest.status == QuestStatus::Active &&
            (quest.type == QuestType::LostProperty || quest.type == QuestType::Rescue);
        if ((!lostVisible && !activeVisible) || quest.targetAreaIndex != m_currentAreaIndex) continue;
        EnsureQuestTargetPosition(quest);
        const DirectX::XMFLOAT3 base = ToWorld3D({ quest.targetX, quest.targetZ }, quest.targetLayerDepth, 0.05f);
        const DirectX::XMFLOAT3 scale = quest.type == QuestType::Rescue
            ? DirectX::XMFLOAT3{ 0.6f, 1.4f, 0.6f } : DirectX::XMFLOAT3{ 0.4f, 0.3f, 0.4f };
        DrawDebugBox3D({ base.x, base.y + scale.y * 0.5f, base.z }, scale);
    }
}

void SceneNarakuProto::DrawQuestDesk()
{
    EnsureQuestBoard();
    RefreshPromotionQuestAvailability();

    DrawTownFrame(u8"依頼受付", [this]()
    {
        ImGui::Text(u8"階級: %s  保険対象: 第%d層まで  受注: %d/%d", GetRankName(m_adventurerRank),
            GetRankMaximumDepth(m_adventurerRank), static_cast<int>(std::count_if(m_quests.begin(), m_quests.end(), [](const QuestRecord& quest)
            { return quest.status == QuestStatus::Active || quest.status == QuestStatus::Complete; })), kMaximumActiveQuests);
        ImGui::Text(u8"所持金: %dG  借金: %dG", m_money, m_questDebt);
        static const char* kWeekdayNames[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
        const int questDeskWeekSecond = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
        const int questDeskDay = questDeskWeekSecond / static_cast<int>(kGameDaySeconds);
        const int questDeskDaySecond = questDeskWeekSecond % static_cast<int>(kGameDaySeconds);
        ImGui::Text(u8"ゲーム内時刻: %s曜日 %02d:%02d", kWeekdayNames[questDeskDay],
            questDeskDaySecond / 3600, (questDeskDaySecond % 3600) / 60);
        if (m_weekResetPending)
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"週が更新されました。次回潜行時に奈落が再生成されます。");

        if (NarakuUi::BeginTabBar("QuestTabs"))
        {
            if (ImGui::BeginTabItem(u8"通常クエスト"))
            {
                ImGui::BeginChild("QuestList", ImVec2(0.0f, 160.0f), true);
                for (std::size_t index = 0; index < m_quests.size(); ++index)
                {
                    const QuestRecord& quest = m_quests[index];
                    std::ostringstream label;
                    label << GetQuestName(quest.type) << "##quest" << index;
                    if (ImGui::Selectable(label.str().c_str(), m_selectedQuest == static_cast<int>(index))) m_selectedQuest = static_cast<int>(index);
                    const char* status = quest.status == QuestStatus::Available ? u8"募集中" : quest.status == QuestStatus::Active ? u8"受注中" :
                        quest.status == QuestStatus::Complete ? u8"報告可" : u8"更新待ち";
                    ImGui::TextDisabled(u8"%s", status);
                }
                ImGui::EndChild();

                ImGui::BeginChild("QuestDetail", ImVec2(0.0f, 260.0f), true);
                if (!m_quests.empty())
                {
                    QuestRecord& quest = m_quests[static_cast<std::size_t>(std::max(0, std::min(m_selectedQuest, static_cast<int>(m_quests.size()) - 1)))];
                    ImGui::Text(u8"%s", GetQuestName(quest.type));
                    ImGui::Separator();
                    ImGui::TextWrapped(u8"%s", GetQuestDescription(quest).c_str());
                    ImGui::Text(u8"進捗: %d / %d", quest.progress, quest.targetCount);
                    ImGui::Text(u8"報酬: %dG", quest.reward);
                    if (quest.status == QuestStatus::Active) ImGui::Text(u8"残り期限: %.0f分", std::max(0.0, quest.remainingSeconds) / 60.0);
                    if (quest.status == QuestStatus::Available && ImGui::Button(u8"受注する")) AcceptQuest(static_cast<std::size_t>(m_selectedQuest));
                    if (quest.status == QuestStatus::Complete && ImGui::Button(u8"報告する")) ReportQuest(static_cast<std::size_t>(m_selectedQuest));
                    if (quest.status == QuestStatus::Cooldown) ImGui::TextDisabled(u8"再募集まで %.0f秒", std::max(0.0, quest.cooldownSeconds));
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(u8"重要クエスト"))
            {
                ImGui::BeginChild("ImportantQuestList", ImVec2(0.0f, 160.0f), true);
                for (std::size_t index = 0; index < m_importantQuests.size(); ++index)
                {
                    const ImportantQuestRecord& quest = m_importantQuests[index];
                    if (quest.status == ImportantQuestStatus::Locked) continue;
                    const ImportantQuestType type = static_cast<ImportantQuestType>(index);
                    std::ostringstream label;
                    label << GetImportantQuestName(type) << "##important" << index;
                    if (ImGui::Selectable(label.str().c_str(), m_selectedImportantQuest == static_cast<int>(index)))
                        m_selectedImportantQuest = static_cast<int>(index);
                    ImGui::TextDisabled(u8"%s", quest.status == ImportantQuestStatus::Active ? u8"進行中" :
                        quest.status == ImportantQuestStatus::Complete ? u8"報告可" : u8"達成済み");
                }
                ImGui::EndChild();

                ImGui::BeginChild("ImportantQuestDetail", ImVec2(0.0f, 260.0f), true);
                const std::size_t index = static_cast<std::size_t>(std::max(0, std::min(m_selectedImportantQuest, static_cast<int>(m_importantQuests.size()) - 1)));
                const ImportantQuestRecord& quest = m_importantQuests[index];
                if (quest.status != ImportantQuestStatus::Locked)
                {
                    const ImportantQuestType type = static_cast<ImportantQuestType>(index);
                    int required = 1;
                    if (type == ImportantQuestType::FiveHunts) required = 5;
                    else if (type == ImportantQuestType::ThirteenHunts) required = 13;
                    else if (type == ImportantQuestType::ThreeTerritoryHunts) required = 3;
                    ImGui::TextUnformatted(GetImportantQuestName(type));
                    ImGui::Separator();
                    ImGui::TextWrapped(u8"%s", GetImportantQuestDescription(type));
                    ImGui::Text(u8"進捗: %d / %d", quest.progress, required);
                    ImGui::Text(u8"報酬: %dG / EXP %d / 食料 %d / 水筒 %d",
                        GetImportantQuestMoneyReward(type), GetImportantQuestExpReward(type),
                        GetImportantQuestFoodReward(type), GetImportantQuestWaterBottleReward(type));
                    if (quest.status == ImportantQuestStatus::Complete && ImGui::Button(u8"報告して報酬を受け取る"))
                        ReportImportantQuest(index);
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem(u8"緊急クエスト"))
            {
                ImGui::BeginChild("PromotionQuestList", ImVec2(0.0f, 160.0f), true);
                for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
                {
                    const PromotionQuestRecord& quest = m_promotionQuests[index];
                    std::ostringstream label;
                    label << GetPromotionQuestName(index) << "##promotion" << index;
                    if (ImGui::Selectable(label.str().c_str(), m_selectedPromotionQuest == static_cast<int>(index)))
                        m_selectedPromotionQuest = static_cast<int>(index);
                    const char* status = quest.status == PromotionQuestStatus::Locked ? u8"未解放" :
                        quest.status == PromotionQuestStatus::Available ? u8"受注可能" :
                        quest.status == PromotionQuestStatus::Active ? u8"受注中" :
                        quest.status == PromotionQuestStatus::Complete ? u8"報告可" :
                        quest.status == PromotionQuestStatus::FailedThisWeek ? u8"翌週まで受注不可" : u8"合格済み";
                    ImGui::TextDisabled(u8"%s", status);
                }
                ImGui::EndChild();

                ImGui::BeginChild("PromotionQuestDetail", ImVec2(0.0f, 260.0f), true);
                const std::size_t index = static_cast<std::size_t>(std::max(0,
                    std::min(m_selectedPromotionQuest, static_cast<int>(m_promotionQuests.size()) - 1)));
                const PromotionQuestRecord& quest = m_promotionQuests[index];
                ImGui::TextUnformatted(GetPromotionQuestName(index));
                ImGui::Separator();
                ImGui::TextWrapped(u8"適正な実力を持って受け、指定深度から生還すること。");
                ImGui::Text(u8"解放条件: Lv%d以上 / 第%d層へ到達済み",
                    GetPromotionQuestRequiredLevel(index), GetPromotionQuestTargetDepth(index));
                ImGui::Text(u8"達成条件: 第%d層で受注後に取得した遺物を持って生還",
                    GetPromotionQuestTargetDepth(index));
                ImGui::Text(u8"換金用遺物: %d / %d", quest.cashRelicProgress,
                    GetPromotionQuestRequiredCash(index));
                ImGui::Text(u8"強化系遺物: %d / %d", quest.upgradeRelicProgress,
                    GetPromotionQuestRequiredUpgrade(index));
                ImGui::Text(u8"報酬: %dG / EXP %d / 強化系遺物 %d個",
                    GetPromotionQuestMoneyReward(index), GetPromotionQuestExpReward(index),
                    GetPromotionQuestMaterialReward(index));
                ImGui::TextWrapped(u8"対象遺物は報告時に消費されません。通常帰還で不足、死亡、探索放棄の場合は失敗です。");
                if (quest.status == PromotionQuestStatus::Available && ImGui::Button(u8"昇格試験を受注する"))
                    AcceptPromotionQuest(index);
                else if (quest.status == PromotionQuestStatus::Complete && ImGui::Button(u8"報告して昇格する"))
                    ReportPromotionQuest(index);
                else if (quest.status == PromotionQuestStatus::FailedThisWeek)
                    ImGui::TextDisabled(u8"次の週の開始時に再受注可能になります。");
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }

        if (m_questDebt > 0 && CountStoredRelics(RelicType::Unique) > 0 && ImGui::Button(u8"欲望の揺籃を1個消費して借金1000Gを返済"))
        {
            RemoveStoredRelics(RelicType::Unique, 1);
            m_questDebt = std::max(0, m_questDebt - 1000);
            SaveProgress();
        }
    });
}

void SceneNarakuProto::DrawRouteInfo()
{
    for (const LayerGateState& gate : m_layerGates)
    {
        if (gate.isEntry || !IsNear(m_player.pos, gate.loadPos, 3.0f)) continue;
        ImGui::SetNextWindowPos(ImVec2(840.0f, 20.0f), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_Always);
        NarakuUi::Begin(u8"ルート傾向", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
        if (gate.destinationAreaIndex < 0 || gate.destinationAreaIndex >= static_cast<int>(m_areas.size()) ||
            !m_areas[gate.destinationAreaIndex].generated)
        {
            ImGui::TextWrapped(u8"Fでルートを調査します。調査後、進入前に傾向を確認できます。");
        }
        else
        {
            const AreaState& area = m_areas[gate.destinationAreaIndex];
            ImGui::Text(u8"接続先: 第%d層（%s） エリア%d", area.depth,
                GetSublayerName(area.sublayer), area.areaNumber);
            const DepthRules& rules = GetRulesForDepth(area.depth);
            const int enemyMaximum = rules.chargerMax + rules.territoryMax;
            const int miningMaximum = static_cast<int>(area.map.pieceNames.size()) * 5;
            if (static_cast<int>(area.enemies.size()) == enemyMaximum) ImGui::BulletText(u8"敵が多め");
            if (!area.miningPoints.empty() && static_cast<int>(area.miningPoints.size()) > miningMaximum - 5) ImGui::BulletText(u8"採掘地点が多め");
            const bool hasCliff = std::any_of(area.map.terrainLayers.begin(), area.map.terrainLayers.end(), [](const NarakuMap::TerrainLayer& layer)
            {
                return std::any_of(layer.cellAttributeFlags.begin(), layer.cellAttributeFlags.end(), [](std::uint32_t flags)
                { return (flags & NarakuMap::CellAttributeCliffEdge) != 0u; });
            });
            if (hasCliff) ImGui::BulletText(u8"崖あり");
            if (std::any_of(area.enemies.begin(), area.enemies.end(), [](const EnemyState& enemy) { return enemy.type == EnemyType::Territory; }))
                ImGui::BulletText(u8"縄張り型あり");
            if (area.enemies.empty() && area.miningPoints.empty() && !hasCliff) ImGui::TextDisabled(u8"目立った傾向なし");
            const int enemyRequired = static_cast<int>(std::ceil(static_cast<float>(area.enemies.size()) * 0.75f));
            const int miningRequired = static_cast<int>(std::ceil(static_cast<float>(area.miningPoints.size()) * 0.75f));
            if (area.discoveredEnemyCount >= enemyRequired)
                ImGui::Text(u8"記録済みの敵: %d体", static_cast<int>(area.enemies.size()));
            if (area.discoveredMiningCount >= miningRequired)
                ImGui::Text(u8"記録済みの採掘地点: %d箇所", static_cast<int>(area.miningPoints.size()));
            if (area.discoveredCliffCount > 0) ImGui::Text(u8"視認済みの崖あり");
            ImGui::Separator();
            ImGui::Text(u8"もう一度Fで進入");
        }
        ImGui::End();
        break;
    }
}

void SceneNarakuProto::DrawMapControls()
{
    // マップウィンドウの初期位置を指定します。
    ImGui::SetNextWindowPos(ImVec2(690.0f, 80.0f), ImGuiCond_FirstUseEver);

    // マップウィンドウの初期サイズを指定します。
    ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);

    // マップウィンドウを開始します。
    NarakuUi::Begin(u8"地図", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::Button(u8"← [LB] 設定", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Settings; m_inputSettings.OnOpen(); }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.3f, 0.9f, 1.0f, 1.0f), u8"【 地図 】");
    ImGui::SameLine();
    if (ImGui::Button(u8"所持品 [RB] →", ImVec2(140.0f, 28.0f))) { m_activeMenuTab = MenuTab::Inventory; m_inventoryMapShowingMap = false; }
    ImGui::SameLine();
    ImGui::TextDisabled(u8" (LB/RB 切替, [B/Esc] 閉じる)");
    if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
    {
        const AreaState& currentArea = m_areas[m_currentAreaIndex];
        ImGui::Text(u8"現在深度: 第%d層（%s） エリア%d", currentArea.depth,
            GetSublayerName(currentArea.sublayer), currentArea.areaNumber);
    }
    else ImGui::TextUnformatted(u8"現在地: 地上");
    static const char* kWeekdayNames[] = { u8"月", u8"火", u8"水", u8"木", u8"金", u8"土", u8"日" };
    const int mapWeekSecond = static_cast<int>(std::fmod(std::max(0.0, m_gameWeekSeconds), kGameWeekSeconds));
    const int mapDay = mapWeekSecond / static_cast<int>(kGameDaySeconds);
    const int mapDaySecond = mapWeekSecond % static_cast<int>(kGameDaySeconds);
    ImGui::Text(u8"ゲーム内時刻: %s曜日 %02d:%02d", kWeekdayNames[mapDay],
        mapDaySecond / 3600, (mapDaySecond % 3600) / 60);
    if (m_weekResetPending)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), u8"次回潜行時に新しい週のマップへ更新されます。");
    ImGui::Separator();

    if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
    {
        const AreaState& area = m_areas[m_currentAreaIndex];
        const int enemyRequired = static_cast<int>(std::ceil(static_cast<float>(m_enemies.size()) * 0.75f));
        const int miningRequired = static_cast<int>(std::ceil(static_cast<float>(m_miningPoints.size()) * 0.75f));
        ImGui::Text(u8"情報開示  敵:%d/%d  採掘:%d/%d", area.discoveredEnemyCount, enemyRequired,
            area.discoveredMiningCount, miningRequired);
        if (area.discoveredEnemyCount >= enemyRequired)
            ImGui::Text(u8"敵情報: 突進型%d / 縄張り型%d",
                static_cast<int>(std::count_if(m_enemies.begin(), m_enemies.end(), [](const EnemyState& enemy) { return enemy.type == EnemyType::Charger; })),
                static_cast<int>(std::count_if(m_enemies.begin(), m_enemies.end(), [](const EnemyState& enemy) { return enemy.type == EnemyType::Territory; })));
        if (area.discoveredMiningCount >= miningRequired) ImGui::Text(u8"採掘地点: %d", static_cast<int>(m_miningPoints.size()));
        if (area.discoveredCliffCount > 0) ImGui::Text(u8"崖: あり");
        ImGui::Separator();
    }

    // 拡大縮小操作のUIを追加します。
    if (ImGui::Button("-"))
    {
        m_mapZoom = std::max(0.5f, m_mapZoom - 0.25f);
    }
    ImGui::SameLine();
    if (ImGui::Button("+"))
    {
        m_mapZoom = std::min(5.0f, m_mapZoom + 0.25f);
    }
    ImGui::SameLine();
    ImGui::Text("Zoom %.2fx", m_mapZoom);
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        m_mapZoom = 2.0f;
        m_mapScrollOffset = { 0.0f, 0.0f };
    }

    // ミニマップ描画領域の左上座標を取得します。
    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };

    // ミニマップ描画領域のサイズを決めます。
    Vec2 canvasSize = { ImGui::GetContentRegionAvail().x, 460.0f };

    // ImGuiの直接描画リストを取得します。
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const Vec2 mapFocus = { m_player.pos.x + m_mapScrollOffset.x, m_player.pos.y + m_mapScrollOffset.y };

    // ミニマップ背景を塗ります。
    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(24, 28, 28, 255));

    // キャンバス外へのはみ出しを防ぐため、クリッピングを設定します。
    draw->PushClipRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), true);

    // 1. 地形セルの描画 (プレイヤー現在深度付近のレイヤーのみ)
    for (const NarakuMap::TerrainLayer& layer : m_runtimeMap.terrainLayers)
    {
        if (layer.gridWidth < 2 || layer.gridHeight < 2) continue;
        if (std::fabs(layer.layerDepth - m_player.depth) > 0.5f) continue;

        const float halfWidth = (layer.gridWidth - 1) * layer.cellSize * 0.5f;
        const float halfHeight = (layer.gridHeight - 1) * layer.cellSize * 0.5f;
        const float minX = layer.center.x - halfWidth;
        const float minZ = layer.center.z - halfHeight;

        for (int cellZ = 0; cellZ < layer.gridHeight - 1; ++cellZ)
        {
            for (int cellX = 0; cellX < layer.gridWidth - 1; ++cellX)
            {
                const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                if (flags & NarakuMap::CellAttributeRemoved) continue;

                Vec2 c00 = { minX + cellX * layer.cellSize, minZ + cellZ * layer.cellSize };
                Vec2 c11 = { c00.x + layer.cellSize, c00.y + layer.cellSize };

                Vec2 p00 = WorldToCanvas(canvasPos, canvasSize, c00, m_mapZoom, mapFocus);
                Vec2 p11 = WorldToCanvas(canvasPos, canvasSize, c11, m_mapZoom, mapFocus);

                ImU32 color = IM_COL32(35, 55, 45, 200); // 通常歩行可能
                if (flags & NarakuMap::CellAttributeBlocked)
                {
                    color = IM_COL32(95, 38, 38, 220); // 通行不可（赤系）
                }
                else if (flags & NarakuMap::CellAttributeCliffEdge)
                {
                    color = IM_COL32(85, 75, 40, 200); // 崖端（黄系）
                }
                else if (flags & NarakuMap::CellAttributeHazard)
                {
                    color = IM_COL32(100, 38, 100, 200); // 危険地形（紫系）
                }

                draw->AddRectFilled(ImVec2(p00.x, p00.y), ImVec2(p11.x, p11.y), color);
                draw->AddRect(ImVec2(p00.x, p00.y), ImVec2(p11.x, p11.y), IM_COL32(30, 36, 36, 80));
            }
        }
    }

    // 2. 帰還地点の描画
    if (m_overlayReturnMode != Mode::Surface)
    {
        Vec2 ret = WorldToCanvas(canvasPos, canvasSize, m_returnPoint, m_mapZoom, mapFocus);
        draw->AddCircleFilled(ImVec2(ret.x, ret.y), 6.0f, IM_COL32(80, 180, 255, 255));
    }

    // 3. 採掘ポイントの描画 (採掘済み、または発見済みのみ)
    for (const MiningPoint& point : m_miningPoints)
    {
        if (!(point.mined || point.discovered || point.sensed))
        {
            continue;
        }

        Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, mapFocus);
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.5f, color);
    }

    for (const FishingPoint& point : m_fishingPoints)
    {
        if (!point.discovered || std::fabs(point.depth - m_player.depth) > 0.5f) continue;
        const Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, mapFocus);
        const ImU32 color = point.remainingUses > 0 ? IM_COL32(55, 175, 235, 255) : IM_COL32(95, 95, 95, 255);
        draw->AddCircle(ImVec2(p.x, p.y), 6.0f, color, 12, 2.0f);
        draw->AddLine(ImVec2(p.x - 4.0f, p.y), ImVec2(p.x + 4.0f, p.y), color, 1.5f);
    }

    // 4. マップピンの描画
    for (const Vec2& pin : m_pins)
    {
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin, m_mapZoom, mapFocus);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, IM_COL32(230, 80, 90, 255));
    }

    // 5. プレイヤー位置と向きの描画
    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos, m_mapZoom, mapFocus);
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));
    Vec2 faceEnd = WorldToCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 3.0f / m_mapZoom)), m_mapZoom, mapFocus);
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 1.5f);

    // クリッピングを終了します。
    draw->PopClipRect();

    // ミニマップ外枠を描きます。
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(130, 145, 145, 255));

    // マウスがミニマップ領域内にあるか調べます。
    bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y));

    // 右クリックの長押しドラッグによるマップ移動
    static bool s_isDraggingMap = false;
    if (ImGui::IsMouseClicked(1) && hovered)
    {
        s_isDraggingMap = true;
    }

    if (m_overlayReturnMode == Mode::Surface)
    {
        for (const SurfaceFacilityState& facility : m_surfaceFacilities)
        {
            const Vec2 p = WorldToCanvas(canvasPos, canvasSize, facility.center, m_mapZoom, mapFocus);
            draw->AddRectFilled(ImVec2(p.x - 6.0f, p.y - 6.0f), ImVec2(p.x + 6.0f, p.y + 6.0f),
                facility.type == NarakuPiece::SurfaceFacilityType::AbyssEntrance
                    ? IM_COL32(190, 80, 230, 255) : IM_COL32(80, 220, 140, 255));
            if (ImGui::IsMouseHoveringRect(ImVec2(p.x - 8.0f, p.y - 8.0f), ImVec2(p.x + 8.0f, p.y + 8.0f)))
            {
                const char* description = u8"施設";
                if (facility.type == NarakuPiece::SurfaceFacilityType::Home) description = u8"自宅：装備・在庫・次回持ち込みを管理";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::Shop) description = u8"商店：消耗品と強化素材の売買";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::Armory) description = u8"武具屋：武器と防具の購入";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::RestaurantQuestDesk) description = u8"レストラン・受付：回復と依頼の管理";
                else if (facility.type == NarakuPiece::SurfaceFacilityType::AbyssEntrance) description = u8"奈落塔出入口：通常潜行と直通門";
                ImGui::SetTooltip(u8"%s", description);
            }
        }
    }
    if (!ImGui::IsMouseDown(1))
    {
        s_isDraggingMap = false;
    }

    if (s_isDraggingMap)
    {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        if (delta.x != 0.0f || delta.y != 0.0f)
        {
            float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * m_mapZoom;
            float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * m_mapZoom;
            if (std::fabs(scaleX) > 0.001f && std::fabs(scaleY) > 0.001f)
            {
                m_mapScrollOffset.x -= delta.x / scaleX;
                m_mapScrollOffset.y += delta.y / scaleY;
            }
        }
    }

    // マウスホイールによるスクロール拡縮
    if (hovered)
    {
        const float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f)
        {
            m_mapZoom = std::max(0.5f, std::min(5.0f, m_mapZoom + wheel * 0.25f));
        }
    }

    // ミニマップ上で左クリックされたらピン設置/削除を行います。
    if (hovered && ImGui::IsMouseClicked(0))
    {
        // 現在のマウス座標を取得します。
        ImVec2 mouse = ImGui::GetIO().MousePos;

        // スクリーン座標をワールド座標へ変換してピン操作します。
        TogglePinAt(ScreenToWorld(canvasPos, canvasSize, { mouse.x, mouse.y }, m_mapZoom, mapFocus));
    }

    // ミニマップ描画領域ぶんのImGuiレイアウト領域を確保します。
    ImGui::Dummy(ImVec2(canvasSize.x, canvasSize.y));

    float mapScrollX = 0.0f, mapScrollY = 0.0f;
    GetActionMapScroll(mapScrollX, mapScrollY);
    if (std::abs(mapScrollX) > 0.01f || std::abs(mapScrollY) > 0.01f)
    {
        const float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * m_mapZoom;
        const float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * m_mapZoom;
        if (std::fabs(scaleX) > 0.001f && std::fabs(scaleY) > 0.001f)
        {
            m_mapScrollOffset.x += (mapScrollX * 15.0f) / scaleX;
            m_mapScrollOffset.y += (mapScrollY * 15.0f) / scaleY;
        }
    }
    const float padMapZoom = GetActionMapZoom();
    if (std::abs(padMapZoom) > 0.01f)
    {
        m_mapZoom = std::max(0.5f, std::min(5.0f, m_mapZoom + padMapZoom * 0.05f));
    }
    if (IsActionMapPinTrigger())
    {
        TogglePinAt(mapFocus);
    }
    if (IsActionMapFocusPlayerTrigger())
    {
        m_mapScrollOffset = { 0.0f, 0.0f };
    }

    // ピン操作説明を表示します。
    ImGui::Text(u8"右ドラッグ: マップ移動   左クリック: ピン設置/削除   ホイール: 拡大縮小");

    // マップピンウィンドウを閉じます。
    ImGui::End();
}

void SceneNarakuProto::ResetDebugPlayerParams()
{
    // 既存実装で使っていた固定値を、そのまま初期値として再設定します。
    m_debugPlayerParams.walkSpeed = 1.5f;
    m_debugPlayerParams.runSpeed = 2.5f;
    m_debugPlayerParams.ropeSpeed = 1.0f;
    m_debugPlayerParams.attackPower = kPlayerBaseAttack;
    m_debugPlayerParams.runCostPerSecond = 1.5f;
    m_debugPlayerParams.ropeCostPerSecond = 3.0f;
    m_debugPlayerParams.attackCost = 10.0f;
    m_debugPlayerParams.miningCost = 7.0f;
    m_debugPlayerParams.stepCost = 5.0f;
    m_debugPlayerParams.jumpCost = 5.0f;
    m_debugPlayerParams.staminaRecoverPerSecond = 2.0f;
    m_debugPlayerParams.upperLayerAlpha = 0.06f;
    m_cameraDistance = kCameraDefaultDistance;
    m_cameraMinPitchDegrees = kCameraDefaultMinPitchDegrees;
    m_cameraMaxPitchDegrees = kCameraDefaultMaxPitchDegrees;
    NormalizeCameraSettings();
}

bool SceneNarakuProto::LoadDebugPlayerParams()
{
    std::ifstream stream(kPlaytestConfigPath, std::ios::binary);
    if (!stream)
    {
        return false;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    const std::string json = buffer.str();

    TryReadJsonFloat(json, "walkSpeed", m_debugPlayerParams.walkSpeed);
    TryReadJsonFloat(json, "runSpeed", m_debugPlayerParams.runSpeed);
    TryReadJsonFloat(json, "ropeSpeed", m_debugPlayerParams.ropeSpeed);
    m_debugPlayerParams.attackPower = kPlayerBaseAttack;
    TryReadJsonFloat(json, "runCostPerSecond", m_debugPlayerParams.runCostPerSecond);
    TryReadJsonFloat(json, "ropeCostPerSecond", m_debugPlayerParams.ropeCostPerSecond);
    TryReadJsonFloat(json, "attackCost", m_debugPlayerParams.attackCost);
    TryReadJsonFloat(json, "miningCost", m_debugPlayerParams.miningCost);
    TryReadJsonFloat(json, "stepCost", m_debugPlayerParams.stepCost);
    TryReadJsonFloat(json, "jumpCost", m_debugPlayerParams.jumpCost);
    TryReadJsonFloat(json, "staminaRecoverPerSecond", m_debugPlayerParams.staminaRecoverPerSecond);
    TryReadJsonFloat(json, "upperLayerAlpha", m_debugPlayerParams.upperLayerAlpha);
    TryReadJsonFloat(json, "minimapPosX", m_debugPlayerParams.minimapPosX);
    TryReadJsonFloat(json, "minimapPosY", m_debugPlayerParams.minimapPosY);
    TryReadJsonFloat(json, "minimapSize", m_debugPlayerParams.minimapSize);
    TryReadJsonFloat(json, "showMinimap", m_debugPlayerParams.showMinimap);
    TryReadJsonFloat(json, "cameraDistance", m_cameraDistance);
    TryReadJsonFloat(json, "cameraMinPitchDegrees", m_cameraMinPitchDegrees);
    TryReadJsonFloat(json, "cameraMaxPitchDegrees", m_cameraMaxPitchDegrees);
    ClampDebugPlayerParams();
    NormalizeCameraSettings();
    return true;
}

bool SceneNarakuProto::SaveDebugPlayerParams() const
{
    std::ofstream stream(kPlaytestConfigPath, std::ios::binary | std::ios::trunc);
    if (!stream)
    {
        return false;
    }
    stream << "{\n"
        << "  \"walkSpeed\": " << m_debugPlayerParams.walkSpeed << ",\n"
        << "  \"runSpeed\": " << m_debugPlayerParams.runSpeed << ",\n"
        << "  \"ropeSpeed\": " << m_debugPlayerParams.ropeSpeed << ",\n"
        << "  \"attackPower\": " << m_debugPlayerParams.attackPower << ",\n"
        << "  \"runCostPerSecond\": " << m_debugPlayerParams.runCostPerSecond << ",\n"
        << "  \"ropeCostPerSecond\": " << m_debugPlayerParams.ropeCostPerSecond << ",\n"
        << "  \"attackCost\": " << m_debugPlayerParams.attackCost << ",\n"
        << "  \"miningCost\": " << m_debugPlayerParams.miningCost << ",\n"
        << "  \"stepCost\": " << m_debugPlayerParams.stepCost << ",\n"
        << "  \"jumpCost\": " << m_debugPlayerParams.jumpCost << ",\n"
        << "  \"staminaRecoverPerSecond\": " << m_debugPlayerParams.staminaRecoverPerSecond << ",\n"
        << "  \"upperLayerAlpha\": " << m_debugPlayerParams.upperLayerAlpha << ",\n"
        << "  \"minimapPosX\": " << m_debugPlayerParams.minimapPosX << ",\n"
        << "  \"minimapPosY\": " << m_debugPlayerParams.minimapPosY << ",\n"
        << "  \"minimapSize\": " << m_debugPlayerParams.minimapSize << ",\n"
        << "  \"showMinimap\": " << m_debugPlayerParams.showMinimap << ",\n"
        << "  \"cameraDistance\": " << m_cameraDistance << ",\n"
        << "  \"cameraMinPitchDegrees\": " << m_cameraMinPitchDegrees << ",\n"
        << "  \"cameraMaxPitchDegrees\": " << m_cameraMaxPitchDegrees << "\n"
        << "}\n";
    return stream.good();
}

void SceneNarakuProto::InitializeNewProgress()
{
    m_money = 0;
    m_level = 1;
    m_currentExp = 0;
    m_levelProtection = 0;
    m_level100OverflowExp = 0;
    m_fullness = 75.0f;
    m_hydration = 70.0f;
    m_dehydrationZeroTimer = 0.0f;
    m_dehydrationVisionStrength = 0.0f;
    m_hydrationWasZero = false;
    m_storedFoodCount = 0;
    m_loadoutFoodCount = 0;
    m_storedHeatedFoodCount = 0;
    m_loadoutHeatedFoodCount = 0;
    m_storedRationOneCount = 0;
    m_loadoutRationOneCount = 0;
    m_storedRawFishCount = 0;
    m_loadoutRawFishCount = 0;
    m_storedCookedFishCount = 0;
    m_loadoutCookedFishCount = 0;
    m_storedRawSizedFish.fill(0);
    m_loadoutRawSizedFish.fill(0);
    m_storedCookedSizedFish.fill(0);
    m_loadoutCookedSizedFish.fill(0);
    m_storedCartridgeCount = 0;
    m_loadoutCartridgeCount = 0;
    m_rationFullnessWardTimer = 0.0f;
    m_rationHydrationPenaltyTimer = 0.0f;
    m_unknownWeaponChargeTimer = 0.0f;
    m_unknownWeaponCooldownTimer = 0.0f;
    m_unknownWeaponFiredThisHold = false;
    m_storedWaterBottles.clear();
    m_storedCookingKits.clear();
    m_portableLights.clear();
    m_storedPortableLights.clear();
    m_portableLightOn = false;
    m_storedInventory.clear();
    m_loadoutRelics.fill(0);
    m_identifiedRelics.fill(false);
    m_ownedHeadArmor.fill(false);
    m_ownedBodyArmor.fill(false);
    m_ownedWeapons.fill(false);
    m_ownedHeadArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedBodyArmor[static_cast<std::size_t>(ArmorTier::Leather)] = true;
    m_ownedWeapons[static_cast<std::size_t>(WeaponTier::RustyPickaxe)] = true;
    m_equippedHeadArmor = ArmorTier::Leather;
    m_equippedBodyArmor = ArmorTier::Leather;
    m_equippedWeapon = WeaponTier::RustyPickaxe;
    m_nextRelicAcquisitionOrder = 1;
    m_uniqueRelicReturned = false;
    m_uniqueRelicCodexUnlocked = false;
    m_uniqueRelicAchievementUnlocked = false;
    m_uniqueRelicStoryUnlocked = false;
    m_adventurerRank = AdventurerRank::Red;
    m_maxReachedDepth = 1;
    m_questDebt = 0;
    m_deathRecoveryPending = false;
    m_deathRecoveryDiscardConfirm = false;
    m_deathRecoveryDepth = 0;
    m_deathRecoveryFee = 0;
    m_pendingDeathRecoveryRelics.clear();
    m_pendingDeathRecoveryFood = 0;
    m_pendingDeathRecoveryHeatedFood = 0;
    m_pendingDeathRecoveryRationOne = 0;
    m_pendingDeathRecoveryRawFish = 0;
    m_pendingDeathRecoveryCookedFish = 0;
    m_pendingDeathRecoveryRawSizedFish.fill(0);
    m_pendingDeathRecoveryCookedSizedFish.fill(0);
    m_pendingDeathRecoveryCartridges = 0;
    m_pendingDeathRecoveryBottles.clear();
    m_pendingDeathRecoveryCookingKits.clear();
    m_pendingDeathReason.clear();
    m_pendingDeathLevelBefore = 1;
    m_pendingDeathLevelAfter = 1;
    m_pendingDeathProtectionConsumed = 0;
    m_gameWeekSeconds = 0.0;
    m_weekSeed = (static_cast<std::uint64_t>(std::random_device{}()) << 32) ^ std::random_device{}();
    if (m_weekSeed == 0) m_weekSeed = 1;
    m_diveWorldSeed = m_weekSeed;
    m_weekResetPending = false;
    m_weeklyAreas.clear();
    m_surfacePins.clear();
    m_directGateWeeklyUses.fill(0);
    m_directGateTargetAreas.fill(-1);
    m_directGateTargetPositions.fill({});
    m_nextQuestId = 1;
    m_quests.clear();
    m_selectedQuest = 0;
    InitializeImportantQuests();
    InitializePromotionQuests();
    m_loadedMapVersion = ReadCurrentMapVersion();
}

bool SceneNarakuProto::SaveProgress()
{
#if defined(NARAKU_EDITOR_BUILD)
    return true;
#endif
    CaptureWeeklyWorld();
    const std::wstring directory = kProgressDirectory;
    const std::wstring temporaryPath = kProgressTempPath;
    const std::wstring finalPath = kProgressPath;
    _wmkdir(directory.c_str());
    std::ofstream stream(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!stream) return false;

    auto writeBoolArray = [&stream](const char* key, const auto& values)
    {
        stream << key << '=';
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) stream << ',';
            stream << (values[i] ? 1 : 0);
        }
        stream << '\n';
    };
    auto writeIntArray = [&stream](const char* key, const auto& values)
    {
        stream << key << '=';
        for (std::size_t i = 0; i < values.size(); ++i)
        {
            if (i > 0) stream << ',';
            stream << values[i];
        }
        stream << '\n';
    };

    stream << "NARAKU_PROTO_SAVE\n";
    stream << "version=" << kSaveVersion << '\n';
    stream << "mapVersion=" << ReadCurrentMapVersion() << '\n';
    stream << "money=" << m_money << '\n';
    stream << "level=" << m_level << '\n';
    stream << "exp=" << m_currentExp << '\n';
    stream << "overflowExp=" << m_level100OverflowExp << '\n';
    stream << "protection=" << m_levelProtection << '\n';
    stream << std::fixed << std::setprecision(2) << "fullness=" << m_fullness << '\n';
    stream << "hydration=" << m_hydration << '\n';
    stream << "storedFood=" << m_storedFoodCount << '\n';
    stream << "loadoutFood=" << m_loadoutFoodCount << '\n';
    stream << "storedHeatedFood=" << m_storedHeatedFoodCount << '\n';
    stream << "loadoutHeatedFood=" << m_loadoutHeatedFoodCount << '\n';
    stream << "storedRationOne=" << m_storedRationOneCount << '\n';
    stream << "loadoutRationOne=" << m_loadoutRationOneCount << '\n';
    stream << "storedRawFish=" << m_storedRawFishCount << '\n';
    stream << "loadoutRawFish=" << m_loadoutRawFishCount << '\n';
    stream << "storedCookedFish=" << m_storedCookedFishCount << '\n';
    stream << "loadoutCookedFish=" << m_loadoutCookedFishCount << '\n';
    writeIntArray("storedRawSizedFish", m_storedRawSizedFish);
    writeIntArray("loadoutRawSizedFish", m_loadoutRawSizedFish);
    writeIntArray("storedCookedSizedFish", m_storedCookedSizedFish);
    writeIntArray("loadoutCookedSizedFish", m_loadoutCookedSizedFish);
    stream << "storedCartridges=" << m_storedCartridgeCount << '\n';
    stream << "loadoutCartridges=" << m_loadoutCartridgeCount << '\n';
    stream << "rationFullnessWard=" << m_rationFullnessWardTimer << '\n';
    stream << "rationHydrationPenalty=" << m_rationHydrationPenaltyTimer << '\n';
    stream << "unknownWeaponCooldown=" << m_unknownWeaponCooldownTimer << '\n';
    stream << "equippedHead=" << static_cast<int>(m_equippedHeadArmor) << '\n';
    stream << "equippedBody=" << static_cast<int>(m_equippedBodyArmor) << '\n';
    stream << "equippedWeapon=" << static_cast<int>(m_equippedWeapon) << '\n';
    stream << "nextOrder=" << m_nextRelicAcquisitionOrder << '\n';
    stream << "uniqueReturned=" << (m_uniqueRelicReturned ? 1 : 0) << '\n';
    stream << "uniqueCodex=" << (m_uniqueRelicCodexUnlocked ? 1 : 0) << '\n';
    stream << "uniqueAchievement=" << (m_uniqueRelicAchievementUnlocked ? 1 : 0) << '\n';
    stream << "uniqueStory=" << (m_uniqueRelicStoryUnlocked ? 1 : 0) << '\n';
    stream << "rank=" << static_cast<int>(m_adventurerRank) << '\n';
    stream << "maxReachedDepth=" << m_maxReachedDepth << '\n';
    stream << "questDebt=" << m_questDebt << '\n';
    stream << "deathRecoveryPending=" << (m_deathRecoveryPending ? 1 : 0) << '\n';
    stream << "deathRecoveryDepth=" << m_deathRecoveryDepth << '\n';
    stream << "deathRecoveryFee=" << m_deathRecoveryFee << '\n';
    stream << "deathRecoveryFood=" << m_pendingDeathRecoveryFood << '\n';
    stream << "deathRecoveryHeatedFood=" << m_pendingDeathRecoveryHeatedFood << '\n';
    stream << "deathRecoveryRationOne=" << m_pendingDeathRecoveryRationOne << '\n';
    stream << "deathRecoveryRawFish=" << m_pendingDeathRecoveryRawFish << '\n';
    stream << "deathRecoveryCookedFish=" << m_pendingDeathRecoveryCookedFish << '\n';
    writeIntArray("deathRecoveryRawSizedFish", m_pendingDeathRecoveryRawSizedFish);
    writeIntArray("deathRecoveryCookedSizedFish", m_pendingDeathRecoveryCookedSizedFish);
    stream << "deathRecoveryCartridges=" << m_pendingDeathRecoveryCartridges << '\n';
    stream << "deathRecoveryReason=" << m_pendingDeathReason << '\n';
    stream << "deathRecoveryLevelBefore=" << m_pendingDeathLevelBefore << '\n';
    stream << "deathRecoveryLevelAfter=" << m_pendingDeathLevelAfter << '\n';
    stream << "deathRecoveryProtection=" << m_pendingDeathProtectionConsumed << '\n';
    stream << "gameWeekSeconds=" << m_gameWeekSeconds << '\n';
    stream << "weekSeed=" << m_weekSeed << '\n';
    stream << "weekResetPending=" << (m_weekResetPending ? 1 : 0) << '\n';
    stream << "diveWorldSeed=" << m_diveWorldSeed << '\n';
    stream << "nextQuestId=" << m_nextQuestId << '\n';
    writeIntArray("directGateUses", m_directGateWeeklyUses);
    writeIntArray("directGateTargets", m_directGateTargetAreas);
    stream << "directGateTargetPositions=" << m_directGateTargetPositions[0].x << ',' << m_directGateTargetPositions[0].y
        << ',' << m_directGateTargetPositions[1].x << ',' << m_directGateTargetPositions[1].y << '\n';
    stream << "surfacePinCount=" << m_surfacePins.size() << '\n';
    for (const Vec2& pin : m_surfacePins) stream << "surfacePin=" << pin.x << '|' << pin.y << '\n';
    writeBoolArray("identified", m_identifiedRelics);
    writeBoolArray("ownedHead", m_ownedHeadArmor);
    writeBoolArray("ownedBody", m_ownedBodyArmor);
    writeBoolArray("ownedWeapon", m_ownedWeapons);
    writeIntArray("loadoutRelics", m_loadoutRelics);
    stream << "relicCount=" << m_storedInventory.size() << '\n';
    for (const RelicItem& item : m_storedInventory)
    {
        std::string safeName = item.name;
        std::replace(safeName.begin(), safeName.end(), '|', '/');
        stream << "relic=" << static_cast<int>(item.type) << '|' << item.maxUses << '|' << item.remainingUses << '|'
            << item.acquisitionOrder << '|' << (item.broken ? 1 : 0) << '|' << (item.stabilized ? 1 : 0) << '|'
            << (item.autoTrigger ? 1 : 0) << '|' << safeName << '\n';
    }
    stream << "deathRecoveryRelicCount=" << m_pendingDeathRecoveryRelics.size() << '\n';
    for (const RelicItem& item : m_pendingDeathRecoveryRelics)
    {
        std::string safeName = item.name;
        std::replace(safeName.begin(), safeName.end(), '|', '/');
        stream << "deathRecoveryRelic=" << static_cast<int>(item.type) << '|' << item.maxUses << '|'
            << item.remainingUses << '|' << item.acquisitionOrder << '|' << (item.broken ? 1 : 0) << '|'
            << (item.stabilized ? 1 : 0) << '|' << (item.autoTrigger ? 1 : 0) << '|' << safeName << '\n';
    }
    stream << "bottleCount=" << m_storedWaterBottles.size() << '\n';
    for (const WaterBottle& bottle : m_storedWaterBottles)
    {
        stream << "bottle=" << bottle.amount << '|' << bottle.foodPoisoningChance << '|'
            << static_cast<int>(bottle.quality) << '|' << (bottle.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "deathRecoveryBottleCount=" << m_pendingDeathRecoveryBottles.size() << '\n';
    for (const WaterBottle& bottle : m_pendingDeathRecoveryBottles)
    {
        stream << "deathRecoveryBottle=" << bottle.amount << '|' << bottle.foodPoisoningChance << '|'
            << static_cast<int>(bottle.quality) << '|' << (bottle.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "cookingKitCount=" << m_storedCookingKits.size() << '\n';
    for (const CookingKit& kit : m_storedCookingKits)
    {
        stream << "cookingKit=" << kit.remainingUses << '|' << (kit.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "portableLightCount=" << m_storedPortableLights.size() << '\n';
    for (const PortableLight& light : m_storedPortableLights)
    {
        stream << "portableLight=" << light.remainingSeconds << '|' << (light.broken ? 1 : 0) << '|'
            << (light.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "deathRecoveryCookingKitCount=" << m_pendingDeathRecoveryCookingKits.size() << '\n';
    for (const CookingKit& kit : m_pendingDeathRecoveryCookingKits)
    {
        stream << "deathRecoveryCookingKit=" << kit.remainingUses << '|'
            << (kit.selectedForLoadout ? 1 : 0) << '\n';
    }
    stream << "questCount=" << m_quests.size() << '\n';
    for (const QuestRecord& quest : m_quests)
    {
        stream << "quest=" << quest.id << '|' << static_cast<int>(quest.type) << '|' << static_cast<int>(quest.status) << '|'
            << quest.targetDepth << '|' << quest.targetCount << '|' << quest.progress << '|' << quest.reward << '|'
            << quest.rewardItemType << '|' << quest.rewardItemCount << '|' << quest.targetRelicType << '|'
            << quest.targetEnemyType << '|' << quest.targetAreaIndex << '|'
            << quest.targetX << '|' << quest.targetZ << '|' << quest.targetLayerDepth << '|' << quest.remainingSeconds << '|'
            << quest.cooldownSeconds << '|' << quest.acceptedAcquisitionOrder << '|'
            << (quest.targetPositionReady ? 1 : 0) << '|' << (quest.targetInteracted ? 1 : 0) << '\n';
    }
    stream << "importantQuestCount=" << m_importantQuests.size() << '\n';
    for (std::size_t index = 0; index < m_importantQuests.size(); ++index)
        stream << "importantQuest=" << index << '|' << static_cast<int>(m_importantQuests[index].status)
            << '|' << m_importantQuests[index].progress << '\n';
    stream << "promotionQuestCount=" << m_promotionQuests.size() << '\n';
    for (std::size_t index = 0; index < m_promotionQuests.size(); ++index)
    {
        const PromotionQuestRecord& quest = m_promotionQuests[index];
        stream << "promotionQuest=" << index << '|' << static_cast<int>(quest.status) << '|'
            << quest.acceptedAcquisitionOrder << '|' << quest.cashRelicProgress << '|'
            << quest.upgradeRelicProgress << '\n';
    }
    stream.close();
    if (!stream.good()) return false;
    if (!SaveWeeklyWorld()) return false;

    std::ifstream verify(temporaryPath, std::ios::binary);
    std::ostringstream verifyBuffer;
    verifyBuffer << verify.rdbuf();
    const std::string saved = verifyBuffer.str();
    if (!verify.good() && !verify.eof()) return false;
    if (saved.find("NARAKU_PROTO_SAVE\nversion=11\n") != 0 ||
        saved.find("\nmoney=") == std::string::npos || saved.find("\nlevel=") == std::string::npos ||
        saved.find("\nexp=") == std::string::npos || saved.find("\nfullness=") == std::string::npos ||
        saved.find("\nhydration=") == std::string::npos || saved.find("\nrelicCount=") == std::string::npos ||
        saved.find("\nbottleCount=") == std::string::npos || saved.find("\ncookingKitCount=") == std::string::npos ||
        saved.find("\nportableLightCount=") == std::string::npos ||
        saved.find("\nquestCount=") == std::string::npos || saved.find("\nimportantQuestCount=") == std::string::npos ||
        saved.find("\npromotionQuestCount=") == std::string::npos ||
        saved.find("\nstoredRationOne=") == std::string::npos ||
        saved.find("\nstoredRawFish=") == std::string::npos ||
        saved.find("\nstoredCookedFish=") == std::string::npos ||
        saved.find("\nstoredRawSizedFish=") == std::string::npos ||
        saved.find("\nstoredCartridges=") == std::string::npos ||
        saved.find("\ndirectGateUses=") == std::string::npos ||
        saved.find("\nsurfacePinCount=") == std::string::npos ||
        saved.find("\ndeathRecoveryPending=") == std::string::npos ||
        saved.find("\ndeathRecoveryRelicCount=") == std::string::npos ||
        saved.find("\ndeathRecoveryBottleCount=") == std::string::npos ||
        saved.find("\ndeathRecoveryCookingKitCount=") == std::string::npos) return false;
    verify.close();
    return MoveFileExW(temporaryPath.c_str(), finalPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
}

bool SceneNarakuProto::CommitModeAfterSave(Mode targetMode, PresentationScene sourceScene)
{
    if (SaveProgress())
    {
        m_mode = targetMode;
        return true;
    }
    m_pendingModeAfterSave = targetMode;
    m_saveErrorPresentation = sourceScene;
    m_mode = Mode::SaveError;
    return false;
}

bool SceneNarakuProto::LoadProgress()
{
#if defined(NARAKU_EDITOR_BUILD)
    return false;
#endif
    const std::wstring path = kProgressPath;
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::string magic;
    std::getline(stream, magic);
    if (magic != "NARAKU_PROTO_SAVE") return false;

    std::map<std::string, std::string> values;
    std::vector<std::string> relicLines;
    std::vector<std::string> deathRecoveryRelicLines;
    std::vector<std::string> bottleLines;
    std::vector<std::string> deathRecoveryBottleLines;
    std::vector<std::string> cookingKitLines;
    std::vector<std::string> portableLightLines;
    std::vector<std::string> deathRecoveryCookingKitLines;
    std::vector<std::string> questLines;
    std::vector<std::string> importantQuestLines;
    std::vector<std::string> promotionQuestLines;
    std::vector<std::string> surfacePinLines;
    std::string line;
    while (std::getline(stream, line))
    {
        const std::size_t separator = line.find('=');
        if (separator == std::string::npos) continue;
        const std::string key = line.substr(0, separator);
        const std::string value = line.substr(separator + 1);
        if (key == "relic") relicLines.push_back(value);
        else if (key == "deathRecoveryRelic") deathRecoveryRelicLines.push_back(value);
        else if (key == "bottle") bottleLines.push_back(value);
        else if (key == "deathRecoveryBottle") deathRecoveryBottleLines.push_back(value);
        else if (key == "cookingKit") cookingKitLines.push_back(value);
        else if (key == "portableLight") portableLightLines.push_back(value);
        else if (key == "deathRecoveryCookingKit") deathRecoveryCookingKitLines.push_back(value);
        else if (key == "quest") questLines.push_back(value);
        else if (key == "importantQuest") importantQuestLines.push_back(value);
        else if (key == "promotionQuest") promotionQuestLines.push_back(value);
        else if (key == "surfacePin") surfacePinLines.push_back(value);
        else values[key] = value;
    }
    try
    {
        if (!values.count("version") || !values.count("money") || !values.count("level")) return false;
        const int version = std::stoi(values["version"]);
        if (version != kSaveVersion && version != kPreviousSaveVersion) return false;
        if (!values.count("exp") || !values.count("fullness") ||
            !values.count("hydration") || !values.count("relicCount") || !values.count("bottleCount") ||
            !values.count("cookingKitCount") || !values.count("rank") || !values.count("questCount") ||
            !values.count("weekSeed") || !values.count("importantQuestCount")) return false;
        if (version == kSaveVersion &&
            (!values.count("deathRecoveryPending") || !values.count("deathRecoveryDepth") ||
                !values.count("deathRecoveryFee") || !values.count("deathRecoveryFood") ||
                !values.count("deathRecoveryHeatedFood") || !values.count("deathRecoveryRelicCount") ||
                !values.count("deathRecoveryBottleCount") || !values.count("deathRecoveryCookingKitCount")))
        {
            return false;
        }
        if (version >= 6 && !values.count("promotionQuestCount")) return false;
        if (version == kSaveVersion &&
            (!values.count("weekResetPending") || !values.count("diveWorldSeed") ||
                !values.count("directGateUses") || !values.count("directGateTargets") ||
                !values.count("directGateTargetPositions") || !values.count("surfacePinCount") ||
                !values.count("storedRationOne") || !values.count("loadoutRationOne") ||
                !values.count("storedRawFish") || !values.count("loadoutRawFish") ||
                !values.count("storedCookedFish") || !values.count("loadoutCookedFish") ||
                !values.count("storedCartridges") || !values.count("loadoutCartridges") ||
                !values.count("deathRecoveryRationOne") || !values.count("deathRecoveryRawFish") ||
                !values.count("deathRecoveryCookedFish") || !values.count("deathRecoveryCartridges") ||
                !values.count("storedRawSizedFish") || !values.count("loadoutRawSizedFish") ||
                !values.count("storedCookedSizedFish") || !values.count("loadoutCookedSizedFish") ||
                !values.count("deathRecoveryRawSizedFish") || !values.count("deathRecoveryCookedSizedFish") ||
                !values.count("rationFullnessWard") || !values.count("rationHydrationPenalty") ||
                !values.count("unknownWeaponCooldown") || !values.count("portableLightCount"))) return false;
        m_money = std::max(0, std::stoi(values["money"]));
        m_level = std::max(1, std::min(100, std::stoi(values["level"])));
        m_currentExp = values.count("exp") ? std::max(0, std::stoi(values["exp"])) : 0;
        m_fullness = values.count("fullness")
            ? std::max(0.0f, std::min(kFullnessMaximum, std::stof(values["fullness"])))
            : 75.0f;
        m_hydration = values.count("hydration")
            ? std::max(0.0f, std::min(kHydrationMaximum, std::stof(values["hydration"])))
            : 70.0f;
        if (values.count("overflowExp")) m_level100OverflowExp = std::max<std::int64_t>(0, std::stoll(values["overflowExp"]));
        if (values.count("protection")) m_levelProtection = std::max(0, std::stoi(values["protection"]));
        if (values.count("storedFood")) m_storedFoodCount = std::max(0, std::stoi(values["storedFood"]));
        if (values.count("loadoutFood")) m_loadoutFoodCount = std::max(0, std::stoi(values["loadoutFood"]));
        if (values.count("storedHeatedFood")) m_storedHeatedFoodCount = std::max(0, std::stoi(values["storedHeatedFood"]));
        if (values.count("loadoutHeatedFood")) m_loadoutHeatedFoodCount = std::max(0, std::stoi(values["loadoutHeatedFood"]));
        if (values.count("storedRationOne")) m_storedRationOneCount = std::max(0, std::stoi(values["storedRationOne"]));
        if (values.count("loadoutRationOne")) m_loadoutRationOneCount = std::max(0, std::stoi(values["loadoutRationOne"]));
        if (values.count("storedRawFish")) m_storedRawFishCount = std::max(0, std::stoi(values["storedRawFish"]));
        if (values.count("loadoutRawFish")) m_loadoutRawFishCount = std::max(0, std::stoi(values["loadoutRawFish"]));
        if (values.count("storedCookedFish")) m_storedCookedFishCount = std::max(0, std::stoi(values["storedCookedFish"]));
        if (values.count("loadoutCookedFish")) m_loadoutCookedFishCount = std::max(0, std::stoi(values["loadoutCookedFish"]));
        if (values.count("storedCartridges")) m_storedCartridgeCount = std::max(0, std::stoi(values["storedCartridges"]));
        if (values.count("loadoutCartridges")) m_loadoutCartridgeCount = std::max(0, std::stoi(values["loadoutCartridges"]));
        if (values.count("rationFullnessWard")) m_rationFullnessWardTimer = std::max(0.0f, std::stof(values["rationFullnessWard"]));
        if (values.count("rationHydrationPenalty")) m_rationHydrationPenaltyTimer = std::max(0.0f, std::stof(values["rationHydrationPenalty"]));
        if (values.count("unknownWeaponCooldown")) m_unknownWeaponCooldownTimer = std::max(0.0f, std::stof(values["unknownWeaponCooldown"]));
        if (values.count("equippedHead"))
        {
            int equipped = std::stoi(values["equippedHead"]);
            if (version == 8 && equipped == 6) equipped = static_cast<int>(ArmorTier::None);
            m_equippedHeadArmor = static_cast<ArmorTier>(std::max(0, std::min(static_cast<int>(ArmorTier::Count) - 1, equipped)));
        }
        if (values.count("equippedBody"))
        {
            int equipped = std::stoi(values["equippedBody"]);
            if (version == 8 && equipped == 6) equipped = static_cast<int>(ArmorTier::None);
            m_equippedBodyArmor = static_cast<ArmorTier>(std::max(0, std::min(static_cast<int>(ArmorTier::Count) - 1, equipped)));
        }
        if (values.count("equippedWeapon"))
        {
            int equipped = std::stoi(values["equippedWeapon"]);
            if (version == 8 && equipped == 5) equipped = static_cast<int>(WeaponTier::None);
            m_equippedWeapon = static_cast<WeaponTier>(std::max(0, std::min(static_cast<int>(WeaponTier::Count) - 1, equipped)));
        }
        if (values.count("nextOrder")) m_nextRelicAcquisitionOrder = std::max<std::uint64_t>(1, std::stoull(values["nextOrder"]));
        if (values.count("uniqueReturned")) m_uniqueRelicReturned = std::stoi(values["uniqueReturned"]) != 0;
        if (values.count("uniqueCodex")) m_uniqueRelicCodexUnlocked = std::stoi(values["uniqueCodex"]) != 0;
        if (values.count("uniqueAchievement")) m_uniqueRelicAchievementUnlocked = std::stoi(values["uniqueAchievement"]) != 0;
        if (values.count("uniqueStory")) m_uniqueRelicStoryUnlocked = std::stoi(values["uniqueStory"]) != 0;
        {
            m_adventurerRank = static_cast<AdventurerRank>(std::max(0, std::min(static_cast<int>(AdventurerRank::Count) - 1, std::stoi(values["rank"]))));
            m_maxReachedDepth = std::max(1, std::min(5, std::stoi(values["maxReachedDepth"])));
            m_questDebt = std::max(0, std::stoi(values["questDebt"]));
            m_gameWeekSeconds = std::max(0.0, std::min(kGameWeekSeconds, std::stod(values["gameWeekSeconds"])));
            m_weekSeed = std::max<std::uint64_t>(1, std::stoull(values["weekSeed"]));
            m_nextQuestId = std::max<std::uint64_t>(1, std::stoull(values["nextQuestId"]));
            m_weekResetPending = version >= 7 && values.count("weekResetPending") && std::stoi(values["weekResetPending"]) != 0;
            m_diveWorldSeed = version >= 7 && values.count("diveWorldSeed")
                ? std::max<std::uint64_t>(1, std::stoull(values["diveWorldSeed"])) : m_weekSeed;
        }
        if (version >= 5)
        {
            m_deathRecoveryPending = std::stoi(values["deathRecoveryPending"]) != 0;
            m_deathRecoveryDepth = std::max(0, std::min(5, std::stoi(values["deathRecoveryDepth"])));
            m_deathRecoveryFee = std::max(0, std::stoi(values["deathRecoveryFee"]));
            m_pendingDeathRecoveryFood = std::max(0, std::stoi(values["deathRecoveryFood"]));
            m_pendingDeathRecoveryHeatedFood = std::max(0, std::stoi(values["deathRecoveryHeatedFood"]));
            if (version >= 9)
            {
                m_pendingDeathRecoveryRationOne = std::max(0, std::stoi(values["deathRecoveryRationOne"]));
                m_pendingDeathRecoveryRawFish = std::max(0, std::stoi(values["deathRecoveryRawFish"]));
                m_pendingDeathRecoveryCookedFish = std::max(0, std::stoi(values["deathRecoveryCookedFish"]));
                m_pendingDeathRecoveryCartridges = std::max(0, std::stoi(values["deathRecoveryCartridges"]));
            }
            m_pendingDeathReason = values.count("deathRecoveryReason") ? values["deathRecoveryReason"] : std::string();
            m_pendingDeathLevelBefore = values.count("deathRecoveryLevelBefore")
                ? std::max(1, std::min(100, std::stoi(values["deathRecoveryLevelBefore"]))) : m_level;
            m_pendingDeathLevelAfter = values.count("deathRecoveryLevelAfter")
                ? std::max(1, std::min(100, std::stoi(values["deathRecoveryLevelAfter"]))) : m_level;
            m_pendingDeathProtectionConsumed = values.count("deathRecoveryProtection")
                ? std::max(0, std::stoi(values["deathRecoveryProtection"])) : 0;
        }

        const std::string currentMapVersion = ReadCurrentMapVersion();
        const std::string savedMapVersion = values.count("mapVersion") ? values["mapVersion"] : std::string();
        if (savedMapVersion != currentMapVersion)
        {
            m_areas.clear();
            m_weeklyAreas.clear();
            m_weekResetPending = true;
            m_currentAreaIndex = -1;
            m_result = RunResult();
        }
        m_loadedMapVersion = currentMapVersion;

        auto readArray = [&values](const char* key, auto& target)
        {
            if (!values.count(key)) return;
            std::istringstream input(values[key]);
            std::string token;
            std::size_t index = 0;
            while (std::getline(input, token, ',') && index < target.size())
            {
                target[index++] = std::stoi(token);
            }
        };
        readArray("identified", m_identifiedRelics);
        readArray("ownedHead", m_ownedHeadArmor);
        readArray("ownedBody", m_ownedBodyArmor);
        readArray("ownedWeapon", m_ownedWeapons);
        readArray("loadoutRelics", m_loadoutRelics);
        if (version >= 10)
        {
            readArray("storedRawSizedFish", m_storedRawSizedFish);
            readArray("loadoutRawSizedFish", m_loadoutRawSizedFish);
            readArray("storedCookedSizedFish", m_storedCookedSizedFish);
            readArray("loadoutCookedSizedFish", m_loadoutCookedSizedFish);
            readArray("deathRecoveryRawSizedFish", m_pendingDeathRecoveryRawSizedFish);
            readArray("deathRecoveryCookedSizedFish", m_pendingDeathRecoveryCookedSizedFish);
        }
        readArray("directGateUses", m_directGateWeeklyUses);
        readArray("directGateTargets", m_directGateTargetAreas);
        if (values.count("directGateTargetPositions"))
        {
            std::istringstream positions(values["directGateTargetPositions"]);
            std::string token;
            float parsed[4] = {};
            int index = 0;
            while (std::getline(positions, token, ',') && index < 4) parsed[index++] = std::stof(token);
            if (index == 4)
            {
                m_directGateTargetPositions[0] = { parsed[0], parsed[1] };
                m_directGateTargetPositions[1] = { parsed[2], parsed[3] };
            }
        }
        m_surfacePins.clear();
        for (const std::string& pinLine : surfacePinLines)
        {
            const std::size_t separator = pinLine.find('|');
            if (separator == std::string::npos) throw std::runtime_error("invalid surface pin");
            m_surfacePins.push_back({ std::stof(pinLine.substr(0, separator)), std::stof(pinLine.substr(separator + 1)) });
        }
        if (values.count("surfacePinCount") && std::stoull(values["surfacePinCount"]) != m_surfacePins.size())
            throw std::runtime_error("surface pin count mismatch");

        m_storedInventory.clear();
        for (const std::string& relicLine : relicLines)
        {
            std::vector<std::string> parts;
            std::istringstream input(relicLine);
            std::string part;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() < 8) throw std::runtime_error("invalid relic record");
            const int typeValue = std::stoi(parts[0]);
            if (typeValue < 0 || typeValue >= static_cast<int>(RelicType::Count)) throw std::runtime_error("invalid relic type");
            RelicItem item = CreateRelic(static_cast<RelicType>(typeValue), parts[7]);
            item.maxUses = std::max(0, std::stoi(parts[1]));
            item.remainingUses = std::max(0, std::stoi(parts[2]));
            item.acquisitionOrder = std::stoull(parts[3]);
            item.broken = std::stoi(parts[4]) != 0;
            item.stabilized = std::stoi(parts[5]) != 0;
            item.autoTrigger = std::stoi(parts[6]) != 0;
            if (item.broken) item.value = 5;
            m_storedInventory.push_back(item);
        }
        if (values.count("relicCount") && static_cast<std::size_t>(std::stoull(values["relicCount"])) != m_storedInventory.size())
            throw std::runtime_error("relic count mismatch");

        m_pendingDeathRecoveryRelics.clear();
        for (const std::string& relicLine : deathRecoveryRelicLines)
        {
            std::vector<std::string> parts;
            std::istringstream input(relicLine);
            std::string part;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() < 8) throw std::runtime_error("invalid death recovery relic record");
            const int typeValue = std::stoi(parts[0]);
            if (typeValue < 0 || typeValue >= static_cast<int>(RelicType::Count))
                throw std::runtime_error("invalid death recovery relic type");
            RelicItem item = CreateRelic(static_cast<RelicType>(typeValue), parts[7]);
            item.maxUses = std::max(0, std::stoi(parts[1]));
            item.remainingUses = std::max(0, std::stoi(parts[2]));
            item.acquisitionOrder = std::stoull(parts[3]);
            item.broken = std::stoi(parts[4]) != 0;
            item.stabilized = true;
            item.autoTrigger = std::stoi(parts[6]) != 0;
            if (item.broken) item.value = 5;
            m_pendingDeathRecoveryRelics.push_back(item);
        }
        if (version >= 5 &&
            static_cast<std::size_t>(std::stoull(values["deathRecoveryRelicCount"])) != m_pendingDeathRecoveryRelics.size())
        {
            throw std::runtime_error("death recovery relic count mismatch");
        }

        m_storedWaterBottles.clear();
        for (const std::string& bottleLine : bottleLines)
        {
            std::istringstream input(bottleLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 4) throw std::runtime_error("invalid bottle record");
            WaterBottle bottle;
            bottle.amount = std::max(0.0f, std::min(kWaterBottleCapacity, std::stof(parts[0])));
            bottle.foodPoisoningChance = std::max(0.0f, std::min(1.0f, std::stof(parts[1])));
            const int quality = std::stoi(parts[2]);
            if (quality < static_cast<int>(WaterQuality::None) || quality > static_cast<int>(WaterQuality::Boiled))
                throw std::runtime_error("invalid water quality");
            bottle.quality = static_cast<WaterQuality>(quality);
            bottle.selectedForLoadout = std::stoi(parts[3]) != 0;
            if (bottle.amount <= 0.0f)
            {
                bottle.quality = WaterQuality::None;
                bottle.foodPoisoningChance = 0.0f;
            }
            m_storedWaterBottles.push_back(bottle);
        }
        if (values.count("bottleCount") && static_cast<std::size_t>(std::stoull(values["bottleCount"])) != m_storedWaterBottles.size())
            throw std::runtime_error("bottle count mismatch");

        m_pendingDeathRecoveryBottles.clear();
        for (const std::string& bottleLine : deathRecoveryBottleLines)
        {
            std::istringstream input(bottleLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 4) throw std::runtime_error("invalid death recovery bottle record");
            WaterBottle bottle;
            bottle.amount = std::max(0.0f, std::min(kWaterBottleCapacity, std::stof(parts[0])));
            bottle.foodPoisoningChance = std::max(0.0f, std::min(1.0f, std::stof(parts[1])));
            const int quality = std::stoi(parts[2]);
            if (quality < static_cast<int>(WaterQuality::None) || quality > static_cast<int>(WaterQuality::Boiled))
                throw std::runtime_error("invalid death recovery water quality");
            bottle.quality = static_cast<WaterQuality>(quality);
            bottle.selectedForLoadout = false;
            if (bottle.amount <= 0.0f)
            {
                bottle.quality = WaterQuality::None;
                bottle.foodPoisoningChance = 0.0f;
            }
            m_pendingDeathRecoveryBottles.push_back(bottle);
        }
        if (version >= 5 &&
            static_cast<std::size_t>(std::stoull(values["deathRecoveryBottleCount"])) != m_pendingDeathRecoveryBottles.size())
        {
            throw std::runtime_error("death recovery bottle count mismatch");
        }

        m_storedCookingKits.clear();
        for (const std::string& kitLine : cookingKitLines)
        {
            std::istringstream input(kitLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 2) throw std::runtime_error("invalid cooking kit record");
            CookingKit kit;
            kit.remainingUses = std::max(1, std::min(kCookingKitMaxUses, std::stoi(parts[0])));
            kit.selectedForLoadout = std::stoi(parts[1]) != 0;
            m_storedCookingKits.push_back(kit);
        }
        if (values.count("cookingKitCount") && static_cast<std::size_t>(std::stoull(values["cookingKitCount"])) != m_storedCookingKits.size())
            throw std::runtime_error("cooking kit count mismatch");

        m_storedPortableLights.clear();
        for (const std::string& lightLine : portableLightLines)
        {
            std::istringstream input(lightLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 3) throw std::runtime_error("invalid portable light record");
            PortableLight light;
            light.remainingSeconds = std::max(0.0f, std::min(kPortableLightDuration, std::stof(parts[0])));
            light.broken = std::stoi(parts[1]) != 0 || light.remainingSeconds <= 0.0f;
            light.selectedForLoadout = std::stoi(parts[2]) != 0;
            m_storedPortableLights.push_back(light);
        }
        if (version == kSaveVersion &&
            static_cast<std::size_t>(std::stoull(values["portableLightCount"])) != m_storedPortableLights.size())
            throw std::runtime_error("portable light count mismatch");
        m_portableLights.clear();
        m_portableLightOn = false;

        m_pendingDeathRecoveryCookingKits.clear();
        for (const std::string& kitLine : deathRecoveryCookingKitLines)
        {
            std::istringstream input(kitLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 2) throw std::runtime_error("invalid death recovery cooking kit record");
            CookingKit kit;
            kit.remainingUses = std::max(1, std::min(kCookingKitMaxUses, std::stoi(parts[0])));
            kit.selectedForLoadout = false;
            m_pendingDeathRecoveryCookingKits.push_back(kit);
        }
        if (version >= 5 &&
            static_cast<std::size_t>(std::stoull(values["deathRecoveryCookingKitCount"])) != m_pendingDeathRecoveryCookingKits.size())
        {
            throw std::runtime_error("death recovery cooking kit count mismatch");
        }
        if (m_deathRecoveryPending && (!HasPendingDeathRecoveryItems() || m_deathRecoveryDepth < 1 || m_deathRecoveryFee < 1))
        {
            throw std::runtime_error("invalid pending death recovery state");
        }

        m_quests.clear();
        for (const std::string& questLine : questLines)
        {
            std::istringstream input(questLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 20) throw std::runtime_error("invalid quest record");
            QuestRecord quest;
            quest.id = std::stoull(parts[0]);
            const int type = std::stoi(parts[1]);
            const int status = std::stoi(parts[2]);
            if (type < 0 || type >= static_cast<int>(QuestType::Count) || status < 0 || status > static_cast<int>(QuestStatus::Cooldown))
                throw std::runtime_error("invalid quest enum");
            quest.type = static_cast<QuestType>(type);
            quest.status = static_cast<QuestStatus>(status);
            quest.targetDepth = std::max(1, std::min(5, std::stoi(parts[3])));
            quest.targetCount = std::max(1, std::stoi(parts[4]));
            quest.progress = std::max(0, std::stoi(parts[5]));
            quest.reward = std::max(0, std::stoi(parts[6]));
            quest.rewardItemType = std::stoi(parts[7]);
            quest.rewardItemCount = std::max(0, std::stoi(parts[8]));
            quest.targetRelicType = std::stoi(parts[9]);
            quest.targetEnemyType = std::stoi(parts[10]);
            quest.targetAreaIndex = std::stoi(parts[11]);
            quest.targetX = std::stof(parts[12]);
            quest.targetZ = std::stof(parts[13]);
            quest.targetLayerDepth = std::stof(parts[14]);
            quest.remainingSeconds = std::max(0.0, std::stod(parts[15]));
            quest.cooldownSeconds = std::max(0.0, std::stod(parts[16]));
            quest.acceptedAcquisitionOrder = std::stoull(parts[17]);
            quest.targetPositionReady = std::stoi(parts[18]) != 0;
            quest.targetInteracted = std::stoi(parts[19]) != 0;
            m_quests.push_back(quest);
        }
        if (static_cast<std::size_t>(std::stoull(values["questCount"])) != m_quests.size())
            throw std::runtime_error("quest count mismatch");
        for (const std::string& questLine : importantQuestLines)
        {
            std::istringstream input(questLine);
            std::string part;
            std::vector<std::string> parts;
            while (std::getline(input, part, '|')) parts.push_back(part);
            if (parts.size() != 3) throw std::runtime_error("invalid important quest record");
            const std::size_t index = static_cast<std::size_t>(std::stoull(parts[0]));
            const int status = std::stoi(parts[1]);
            if (index >= m_importantQuests.size() || status < 0 || status > static_cast<int>(ImportantQuestStatus::Claimed))
                throw std::runtime_error("invalid important quest enum");
            m_importantQuests[index].status = static_cast<ImportantQuestStatus>(status);
            m_importantQuests[index].progress = std::max(0, std::stoi(parts[2]));
        }
        if (static_cast<std::size_t>(std::stoull(values["importantQuestCount"])) != importantQuestLines.size() ||
            importantQuestLines.size() != m_importantQuests.size())
            throw std::runtime_error("important quest count mismatch");
        if (version >= 6)
        {
            m_promotionQuests.fill({});
            for (const std::string& questLine : promotionQuestLines)
            {
                std::istringstream input(questLine);
                std::string part;
                std::vector<std::string> parts;
                while (std::getline(input, part, '|')) parts.push_back(part);
                if (parts.size() != 5) throw std::runtime_error("invalid promotion quest record");
                const std::size_t index = static_cast<std::size_t>(std::stoull(parts[0]));
                const int status = std::stoi(parts[1]);
                if (index >= m_promotionQuests.size() || status < 0 ||
                    status > static_cast<int>(PromotionQuestStatus::Claimed))
                {
                    throw std::runtime_error("invalid promotion quest enum");
                }
                PromotionQuestRecord& quest = m_promotionQuests[index];
                quest.status = static_cast<PromotionQuestStatus>(status);
                quest.acceptedAcquisitionOrder = std::stoull(parts[2]);
                quest.cashRelicProgress = std::max(0, std::stoi(parts[3]));
                quest.upgradeRelicProgress = std::max(0, std::stoi(parts[4]));
            }
            if (static_cast<std::size_t>(std::stoull(values["promotionQuestCount"])) != promotionQuestLines.size() ||
                promotionQuestLines.size() != m_promotionQuests.size())
            {
                throw std::runtime_error("promotion quest count mismatch");
            }
            RefreshPromotionQuestAvailability();
        }
        else
        {
            InitializePromotionQuests();
        }
        if (version == kSaveVersion && !m_weekResetPending) LoadWeeklyWorld();
    }
    catch (...)
    {
        InitializeNewProgress();
        return false;
    }
    return true;
}

void SceneNarakuProto::ClampDebugPlayerParams()
{
    // 速度、攻撃力、各種消費量、回復量は負値にしないよう安全側へ丸めます。
    m_debugPlayerParams.walkSpeed = std::max(0.0f, m_debugPlayerParams.walkSpeed);
    m_debugPlayerParams.runSpeed = std::max(0.0f, m_debugPlayerParams.runSpeed);
    m_debugPlayerParams.ropeSpeed = std::max(0.0f, m_debugPlayerParams.ropeSpeed);
    m_debugPlayerParams.attackPower = std::max(0.0f, m_debugPlayerParams.attackPower);
    m_debugPlayerParams.runCostPerSecond = std::max(0.0f, m_debugPlayerParams.runCostPerSecond);
    m_debugPlayerParams.ropeCostPerSecond = std::max(0.0f, m_debugPlayerParams.ropeCostPerSecond);
    m_debugPlayerParams.attackCost = std::max(0.0f, m_debugPlayerParams.attackCost);
    m_debugPlayerParams.miningCost = std::max(0.0f, m_debugPlayerParams.miningCost);
    m_debugPlayerParams.stepCost = std::max(0.0f, m_debugPlayerParams.stepCost);
    m_debugPlayerParams.jumpCost = std::max(0.0f, m_debugPlayerParams.jumpCost);
    m_debugPlayerParams.staminaRecoverPerSecond = std::max(0.0f, m_debugPlayerParams.staminaRecoverPerSecond);
    m_debugPlayerParams.upperLayerAlpha = std::max(0.0f, std::min(m_debugPlayerParams.upperLayerAlpha, 0.30f));
}

const char* SceneNarakuProto::GetRelicTypeName(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade: return u8"武具強化遺物";
    case RelicType::WeaponUpgrade: return u8"武器強化遺物";
    case RelicType::ArmorUpgrade: return u8"装備強化遺物";
    case RelicType::Offensive: return u8"攻撃的遺物";
    case RelicType::Survival: return u8"生存的遺物";
    case RelicType::CashLow: return u8"換金用遺物（低）";
    case RelicType::CashHigh: return u8"換金用遺物（高）";
    case RelicType::MentalRecovery: return u8"精神力回復遺物";
    case RelicType::Unique: return u8"欲望の揺籃";
    default: return u8"不明な遺物";
    }
}

const char* SceneNarakuProto::GetRelicDisplayName(const RelicItem& item) const
{
    const std::size_t index = static_cast<std::size_t>(item.type);
    return index < m_identifiedRelics.size() && m_identifiedRelics[index]
        ? GetRelicTypeName(item.type)
        : u8"未鑑定の遺物";
}

float SceneNarakuProto::GetRelicWeight(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade:
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade: return 5.0f;
    case RelicType::Offensive: return 8.0f;
    case RelicType::Survival: return 3.0f;
    case RelicType::CashLow: return 2.0f;
    case RelicType::CashHigh: return 10.0f;
    case RelicType::MentalRecovery: return 2.0f;
    case RelicType::Unique: return 100.0f;
    default: return 0.0f;
    }
}

int SceneNarakuProto::GetRelicSellValue(RelicType type) const
{
    switch (type)
    {
    case RelicType::ArmamentUpgrade: return 60;
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade: return 70;
    case RelicType::Offensive: return 0;
    case RelicType::Survival: return 60;
    case RelicType::CashLow: return 5;
    case RelicType::CashHigh: return 60;
    case RelicType::MentalRecovery: return 5;
    case RelicType::Unique: return 0;
    default: return 0;
    }
}

SceneNarakuProto::RelicItem SceneNarakuProto::CreateRelic(RelicType type, const std::string& sourceName)
{
    RelicItem item;
    item.name = type == RelicType::Unique ? u8"欲望の揺籃" : sourceName;
    item.type = type;
    item.weight = GetRelicWeight(type);
    item.value = GetRelicSellValue(type);
    item.acquisitionOrder = m_nextRelicAcquisitionOrder++;
    if (type == RelicType::Offensive)
    {
        item.maxUses = RandomInt(15, 20);
        item.remainingUses = item.maxUses;
    }
    return item;
}

SceneNarakuProto::RelicItem SceneNarakuProto::CreateRandomRelic(const std::string& sourceName)
{
    const std::array<int, 5>& weights = GetRulesForDepth(GetCurrentDepth()).dropWeights;
    const int roll = RandomInt(1, 100);
    int cumulative = weights[0];
    if (roll <= cumulative) return CreateRelic(RelicType::CashLow, sourceName);
    cumulative += weights[1];
    if (roll <= cumulative) return CreateRelic(RelicType::CashHigh, sourceName);
    cumulative += weights[2];
    if (roll <= cumulative)
    {
        return CreateRelic(static_cast<RelicType>(RandomInt(
            static_cast<int>(RelicType::ArmamentUpgrade), static_cast<int>(RelicType::ArmorUpgrade))), sourceName);
    }
    cumulative += weights[3];
    if (roll <= cumulative)
    {
        const RelicType specials[] = { RelicType::Offensive, RelicType::Survival, RelicType::MentalRecovery };
        return CreateRelic(specials[RandomInt(0, 2)], sourceName);
    }
    return CreateRelic(RelicType::Unique, u8"欲望の揺籃");
}

int SceneNarakuProto::GetRelicActivity(const RelicItem& item) const
{
    if (item.stabilized || item.broken) return 0;
    switch (item.type)
    {
    case RelicType::CashLow: return 1;
    case RelicType::CashHigh: return 3;
    case RelicType::ArmamentUpgrade:
    case RelicType::WeaponUpgrade:
    case RelicType::ArmorUpgrade:
    case RelicType::MentalRecovery: return 2;
    case RelicType::Offensive:
    case RelicType::Survival: return 4;
    case RelicType::Unique: return 8;
    default: return 0;
    }
}

int SceneNarakuProto::GetCurrentActivity() const
{
    int activity = 0;
    for (const RelicItem& item : m_inventory) activity += GetRelicActivity(item);
    return activity;
}

bool SceneNarakuProto::IsRelicSellable(const RelicItem& item) const
{
    if (item.type == RelicType::Unique) return false;
    if (item.type == RelicType::Offensive && !item.broken) return false;
    return item.value > 0;
}

bool SceneNarakuProto::UseMentalRecoveryRelic(int inventoryIndex)
{
    if (inventoryIndex < 0 || inventoryIndex >= static_cast<int>(m_inventory.size())) return false;
    const RelicItem& item = m_inventory[inventoryIndex];
    if (item.type != RelicType::MentalRecovery || item.broken || m_player.mental >= GetMaxMental()) return false;
    m_player.mental = std::min(GetMaxMental(), m_player.mental + 10.0f * GetMentalRecoveryMultiplier());
    m_inventory.erase(m_inventory.begin() + inventoryIndex);
    AddMessage(u8"精神力回復遺物を使用しました。");
    return true;
}

bool SceneNarakuProto::TryConsumeSurvivalRelic(bool hpLethal, bool mentalLethal)
{
    for (RelicItem& item : m_inventory)
    {
        if (item.type != RelicType::Survival || item.broken || !item.autoTrigger) continue;
        item.broken = true;
        item.value = 5;
        if (hpLethal) m_player.hp = 1.0f;
        if (mentalLethal) m_player.mental = 1.0f;
        AddMessage(u8"生存的遺物が致命傷を防ぎ、破損しました。");
        return true;
    }
    return false;
}

void SceneNarakuProto::UseFood()
{
    if (m_foodCount <= 0)
    {
        AddMessage(u8"食料を持っていません。");
        ShowCenterNotification(u8"食料がない！");
        return;
    }
    if (m_foodUseTimer > 0.0f || m_cookingTarget != CookingTarget::None) return;
    if (m_player.hp >= GetMaxHp() && m_fullness >= kFullnessMaximum && m_hydration >= kHydrationMaximum)
    {
        AddMessage(u8"体力、満腹度、水分が満タンのため食料を使いませんでした。");
        return;
    }
    m_usingHeatedFood = false;
    m_foodUseTimer = 0.5f;
    AddMessage(u8"食料を食べ始めました。");
}

void SceneNarakuProto::UseHeatedFood()
{
    if (m_heatedFoodCount <= 0)
    {
        AddMessage(u8"加熱食料を持っていません。");
        return;
    }
    if (m_foodUseTimer > 0.0f || m_cookingTarget != CookingTarget::None) return;
    if (m_player.hp >= GetMaxHp() && m_fullness >= kFullnessMaximum &&
        m_hydration >= kHydrationMaximum && m_player.mental >= GetMaxMental())
    {
        AddMessage(u8"回復対象が満タンのため加熱食料を使いませんでした。");
        return;
    }
    m_usingHeatedFood = true;
    m_foodUseTimer = 0.5f;
    AddMessage(u8"加熱食料を食べ始めました。");
}

void SceneNarakuProto::UseRationOne()
{
    if (m_rationOneCount <= 0) return;
    --m_rationOneCount;
    m_fullness = kFullnessMaximum;
    m_rationFullnessWardTimer = kRationFullnessWardDuration;
    m_rationHydrationPenaltyTimer = kRationHydrationPenaltyDuration;
    AddMessage(u8"行動食1号を使用しました。5分間満腹度が減少しません。");
}

void SceneNarakuProto::UseRawFish()
{
    if (m_rawFishCount <= 0 ||
        (m_fullness >= kFullnessMaximum && m_player.mental >= GetMaxMental())) return;
    --m_rawFishCount;
    m_fullness = std::min(kFullnessMaximum, m_fullness + 10.0f);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + 5.0f);
    AddMessage(u8"見たことない魚を生で食べ、満腹度10、精神力5を回復しました。");
}

void SceneNarakuProto::UseCookedFish()
{
    if (m_cookedFishCount <= 0 ||
        (m_fullness >= kFullnessMaximum && m_player.mental >= GetMaxMental())) return;
    --m_cookedFishCount;
    m_fullness = std::min(kFullnessMaximum, m_fullness + 50.0f);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + 25.0f);
    AddMessage(u8"調理済みの魚を食べ、満腹度50、精神力25を回復しました。");
}

const char* SceneNarakuProto::GetWaterQualityName(WaterQuality quality) const
{
    switch (quality)
    {
    case WaterQuality::Safe: return u8"安全";
    case WaterQuality::Unboiled: return u8"未煮沸";
    case WaterQuality::Boiled: return u8"煮沸済";
    default: return u8"なし";
    }
}

void SceneNarakuProto::DrinkWater(float amount, WaterQuality quality, float foodPoisoningChance)
{
    if (m_hydration >= kHydrationMaximum || amount <= 0.0f) return;
    m_hydration = std::min(kHydrationMaximum, m_hydration + amount);
    if (quality == WaterQuality::Unboiled) ApplyFoodPoisoning(foodPoisoningChance);
    AddMessage(u8"水分を25回復しました。");
}

void SceneNarakuProto::ApplyFoodPoisoning(float chance)
{
    if (chance <= 0.0f || RandomFloat(0.0f, 1.0f) >= chance) return;
    const float damage = m_player.hp * 0.10f;
    m_player.hp = std::max(0.0f, m_player.hp - damage);
    AddMessage(u8"食中毒が発生し、現在HPの10%を失いました。");
}

void SceneNarakuProto::DrinkFromBottle(int bottleIndex)
{
    if (bottleIndex < 0 || bottleIndex >= static_cast<int>(m_waterBottles.size()) ||
        m_hydration >= kHydrationMaximum || m_cookingTarget != CookingTarget::None) return;
    WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(bottleIndex)];
    if (bottle.amount <= 0.0f) return;
    const float consumed = std::min(kWaterDrinkAmount, bottle.amount);
    const WaterQuality quality = bottle.quality;
    const float poisoningChance = bottle.foodPoisoningChance;
    bottle.amount -= consumed;
    if (bottle.amount <= 0.0f)
    {
        bottle.amount = 0.0f;
        bottle.quality = WaterQuality::None;
        bottle.foodPoisoningChance = 0.0f;
    }
    DrinkWater(consumed, quality, poisoningChance);
}

void SceneNarakuProto::DiscardBottleWater(int bottleIndex)
{
    if (bottleIndex < 0 || bottleIndex >= static_cast<int>(m_waterBottles.size()) ||
        m_cookingTarget != CookingTarget::None) return;
    WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(bottleIndex)];
    bottle.amount = 0.0f;
    bottle.foodPoisoningChance = 0.0f;
    bottle.quality = WaterQuality::None;
    AddMessage(u8"水筒の中身を捨てました。");
}

bool SceneNarakuProto::ConsumeCookingKitUse()
{
    if (m_cookingKits.empty()) return false;
    CookingKit& kit = m_cookingKits.front();
    --kit.remainingUses;
    if (kit.remainingUses <= 0) m_cookingKits.erase(m_cookingKits.begin());
    return true;
}

void SceneNarakuProto::StartCooking(CookingTarget target, int bottleIndex)
{
    if (target == CookingTarget::None || m_cookingTarget != CookingTarget::None) return;
    if (!m_player.grounded || m_player.onRope)
    {
        AddMessage(u8"歩行可能な足場でなければ料理セットを使用できません。");
        return;
    }
    if (target == CookingTarget::HeatFood && m_foodCount <= 0) return;
    if (target == CookingTarget::CookFish && m_rawFishCount <= 0) return;
    if (target == CookingTarget::CookSizedFish &&
        (m_cookingFishSize < 0 || m_cookingFishSize >= 3 || m_rawSizedFish[static_cast<size_t>(m_cookingFishSize)] <= 0)) return;
    if (target == CookingTarget::BoilBottle)
    {
        if (bottleIndex < 0 || bottleIndex >= static_cast<int>(m_waterBottles.size())) return;
        const WaterBottle& bottle = m_waterBottles[static_cast<std::size_t>(bottleIndex)];
        if (bottle.amount <= 0.0f || bottle.quality != WaterQuality::Unboiled) return;
    }
    if (!ConsumeCookingKitUse())
    {
        AddMessage(u8"料理セットを持っていません。");
        return;
    }
    m_cookingTarget = target;
    m_cookingBottleIndex = bottleIndex;
    m_cookingTimer = kCookingDuration;
    m_cookingPreviousHp = m_player.hp;
    m_mode = Mode::Explore;
    if (target == CookingTarget::BoilBottle)
        AddMessage(u8"水の煮沸を開始しました。60秒間その場を維持してください。");
    else if (target == CookingTarget::CookFish || target == CookingTarget::CookSizedFish)
        AddMessage(u8"魚の調理を開始しました。60秒間その場を維持してください。");
    else
        AddMessage(u8"食料の加熱を開始しました。60秒間その場を維持してください。");
}

void SceneNarakuProto::CancelCooking(const char* reason)
{
    if (m_cookingTarget == CookingTarget::None) return;
    m_cookingTarget = CookingTarget::None;
    m_cookingBottleIndex = -1;
    m_cookingFishSize = -1;
    m_cookingTimer = 0.0f;
    AddMessage(reason);
}

float SceneNarakuProto::GetWaterFoodPoisoningChance() const
{
    switch (GetCurrentDepth())
    {
    case 1: return 0.10f;
    case 2: return 0.30f;
    case 4: return 0.80f;
    default: return 0.0f;
    }
}

std::uint32_t SceneNarakuProto::GetNearbyWaterFlags() const
{
    constexpr std::uint32_t waterMask = NarakuMap::CellAttributeWaterPuddle |
        NarakuMap::CellAttributeWaterPond | NarakuMap::CellAttributeWaterLake;
    const std::uint32_t current = GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & waterMask;
    if (current != 0u) return current;

    const Vec2 offsets[] = {
        { kInteractRange, 0.0f }, { -kInteractRange, 0.0f },
        { 0.0f, kInteractRange }, { 0.0f, -kInteractRange }
    };
    for (const Vec2& offset : offsets)
    {
        const std::uint32_t flags = GetCellAttributeFlagsAt(Add(m_player.pos, offset), m_player.depth) & waterMask;
        if (flags != 0u) return flags;
    }
    return NarakuMap::CellAttributeNone;
}

bool SceneNarakuProto::TryUseRestaurant()
{
    if (m_money < kRestaurantPrice)
    {
        ShowCenterNotification(u8"所持金が足りません。");
        return false;
    }
    if (m_fullness >= kFullnessMaximum && m_hydration >= kHydrationMaximum &&
        m_player.hp >= GetMaxHp() && m_player.mental >= GetMaxMental())
    {
        ShowCenterNotification(u8"今は食事をする必要がありません。");
        return false;
    }
    m_money -= kRestaurantPrice;
    m_fullness = kFullnessMaximum;
    m_hydration = std::min(kHydrationMaximum, m_hydration + kFoodHydrationRecovery);
    m_player.hp = std::min(GetMaxHp(), m_player.hp + GetMaxHp() * kRestaurantHpRatio);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + GetMaxMental() * kRestaurantMentalRatio);
    SaveProgress();
    ShowCenterNotification(u8"食事で満腹度、HP、精神力、水分を回復しました。");
    return true;
}

const char* SceneNarakuProto::GetArmorName(ArmorTier tier) const
{
    switch (tier)
    {
    case ArmorTier::Leather: return u8"革装備";
    case ArmorTier::Iron: return u8"鉄装備";
    case ArmorTier::RelicCovered: return u8"遺物で覆われたシリーズ";
    case ArmorTier::RelicHardened: return u8"遺物で固めたシリーズ";
    case ArmorTier::RelicEnhanced: return u8"遺物で強化されたシリーズ";
    case ArmorTier::Relic: return u8"遺物装備";
    case ArmorTier::Unknown: return u8"未知の装備";
    case ArmorTier::None: return u8"装備なし";
    default: return u8"不明な装備";
    }
}

const char* SceneNarakuProto::GetArmorEffectText(ArmorTier tier, bool headSlot) const
{
    switch (tier)
    {
    case ArmorTier::Leather: return headSlot ? u8"防御力+4%" : u8"防御力+6%";
    case ArmorTier::Iron: return headSlot ? u8"防御力+10%" : u8"防御力+15%";
    case ArmorTier::RelicCovered: return headSlot ? u8"攻撃力+4% / 防御力+16%" : u8"攻撃力+6% / 防御力+24%";
    case ArmorTier::RelicHardened: return headSlot ? u8"防御力+32%" : u8"防御力+48%";
    case ArmorTier::RelicEnhanced: return headSlot ? u8"攻撃力+10% / 防御力+24%" : u8"攻撃力+15% / 防御力+36%";
    case ArmorTier::Relic: return headSlot ? u8"攻撃力+10% / 防御力+30%" : u8"攻撃力+15% / 防御力+45%";
    case ArmorTier::Unknown: return u8"頭・胴セットで専用効果発動";
    default: return u8"効果なし";
    }
}

bool SceneNarakuProto::HasRelicArmorSetEffect() const
{
    return m_equippedHeadArmor == ArmorTier::Relic &&
        m_equippedBodyArmor == ArmorTier::Relic;
}

bool SceneNarakuProto::HasUnknownArmorSetEffect() const
{
    return m_equippedHeadArmor == ArmorTier::Unknown &&
        m_equippedBodyArmor == ArmorTier::Unknown;
}

const char* SceneNarakuProto::GetWeaponName(WeaponTier tier) const
{
    switch (tier)
    {
    case WeaponTier::RustyPickaxe: return u8"錆びれたつるはし";
    case WeaponTier::NormalPickaxe: return u8"普通のつるはし";
    case WeaponTier::SturdyPickaxe: return u8"丈夫なつるはし";
    case WeaponTier::SharpPickaxe: return u8"鋭利なつるはし";
    case WeaponTier::RelicPickaxe: return u8"遺物付きのつるはし";
    case WeaponTier::Unknown: return u8"未知の武器";
    case WeaponTier::None: return u8"武器なし";
    default: return u8"不明な武器";
    }
}

const char* SceneNarakuProto::GetWeaponEffectText(WeaponTier tier) const
{
    switch (tier)
    {
    case WeaponTier::RustyPickaxe: return u8"攻撃力+0% / 採掘速度+0%";
    case WeaponTier::NormalPickaxe: return u8"攻撃力+0% / 採掘速度+35%";
    case WeaponTier::SturdyPickaxe: return u8"攻撃力+0% / 採掘速度+80%";
    case WeaponTier::SharpPickaxe: return u8"攻撃力+0% / 採掘速度+100%";
    case WeaponTier::RelicPickaxe: return u8"攻撃力+0% / 採掘速度+150%";
    case WeaponTier::Unknown: return u8"1秒溜め / 前方直線即死 / CT5秒";
    default: return u8"効果なし";
    }
}

bool SceneNarakuProto::TryBuyArmor(ArmorTier tier, bool headSlot, bool useMaterials)
{
    static const int materialPrices[][2] = { { 10, 15 }, { 150, 200 }, { 200, 300 }, { 250, 350 }, { 350, 400 }, { 5000, 6000 } };
    static const int moneyPrices[][2] = { { 10, 15 }, { 150, 200 }, { 1000, 1500 }, { 1250, 1750 }, { 1750, 2000 }, { 25000, 30000 } };
    static const int armamentCosts[][2] = { { 0, 0 }, { 0, 0 }, { 3, 4 }, { 4, 6 }, { 7, 10 }, { 17, 19 } };
    static const int armorCosts[][2] = { { 0, 0 }, { 0, 0 }, { 5, 7 }, { 7, 9 }, { 5, 7 }, { 15, 19 } };
    const std::size_t tierIndex = static_cast<std::size_t>(tier);
    const int slotIndex = headSlot ? 0 : 1;
    std::array<bool, static_cast<std::size_t>(ArmorTier::Count)>& owned = headSlot ? m_ownedHeadArmor : m_ownedBodyArmor;
    if (owned[tierIndex]) return false;

    const int price = useMaterials ? materialPrices[tierIndex][slotIndex] : moneyPrices[tierIndex][slotIndex];
    const std::size_t armamentIndex = static_cast<std::size_t>(RelicType::ArmamentUpgrade);
    const std::size_t armorIndex = static_cast<std::size_t>(RelicType::ArmorUpgrade);
    if (m_money < price || (useMaterials && (CountStoredRelics(RelicType::ArmamentUpgrade) < armamentCosts[tierIndex][slotIndex] ||
        CountStoredRelics(RelicType::ArmorUpgrade) < armorCosts[tierIndex][slotIndex])))
    {
        AddMessage(u8"購入に必要な金額または遺物が不足しています。");
        return false;
    }

    m_money -= price;
    if (useMaterials)
    {
        RemoveStoredRelics(RelicType::ArmamentUpgrade, armamentCosts[tierIndex][slotIndex]);
        RemoveStoredRelics(RelicType::ArmorUpgrade, armorCosts[tierIndex][slotIndex]);
        m_loadoutRelics[armamentIndex] = std::min(m_loadoutRelics[armamentIndex], CountStoredRelics(RelicType::ArmamentUpgrade));
        m_loadoutRelics[armorIndex] = std::min(m_loadoutRelics[armorIndex], CountStoredRelics(RelicType::ArmorUpgrade));
    }
    owned[tierIndex] = true;
    AddMessage(u8"装備を購入しました。");
    SaveProgress();
    return true;
}

bool SceneNarakuProto::TryBuyWeapon(WeaponTier tier, bool useMaterials)
{
    static const int materialPrices[] = { 5, 500, 1250, 1500, 5000 };
    static const int moneyPrices[] = { 5, 500, 1250, 1500, 25000 };
    static const int armamentCosts[] = { 0, 0, 0, 0, 11 };
    static const int weaponCosts[] = { 0, 0, 0, 0, 21 };
    const std::size_t tierIndex = static_cast<std::size_t>(tier);
    if (m_ownedWeapons[tierIndex]) return false;

    const int price = useMaterials ? materialPrices[tierIndex] : moneyPrices[tierIndex];
    const std::size_t armamentIndex = static_cast<std::size_t>(RelicType::ArmamentUpgrade);
    const std::size_t weaponIndex = static_cast<std::size_t>(RelicType::WeaponUpgrade);
    if (m_money < price || (useMaterials && (CountStoredRelics(RelicType::ArmamentUpgrade) < armamentCosts[tierIndex] ||
        CountStoredRelics(RelicType::WeaponUpgrade) < weaponCosts[tierIndex])))
    {
        AddMessage(u8"購入に必要な金額または遺物が不足しています。");
        return false;
    }

    m_money -= price;
    if (useMaterials)
    {
        RemoveStoredRelics(RelicType::ArmamentUpgrade, armamentCosts[tierIndex]);
        RemoveStoredRelics(RelicType::WeaponUpgrade, weaponCosts[tierIndex]);
        m_loadoutRelics[armamentIndex] = std::min(m_loadoutRelics[armamentIndex], CountStoredRelics(RelicType::ArmamentUpgrade));
        m_loadoutRelics[weaponIndex] = std::min(m_loadoutRelics[weaponIndex], CountStoredRelics(RelicType::WeaponUpgrade));
    }
    m_ownedWeapons[tierIndex] = true;
    AddMessage(u8"武器を購入しました。");
    SaveProgress();
    return true;
}

float SceneNarakuProto::GetCurrentWeight() const
{
    // 初期装備のつるはし重量10から計算を始めます。
    float weight = 10.0f;

    // 所持している旧器の重量をすべて足します。
    for (const RelicItem& item : m_inventory) weight += item.weight;

    // 食料は1個につき重量1として扱います。
    weight += static_cast<float>(m_foodCount);
    weight += static_cast<float>(m_heatedFoodCount);
    weight += static_cast<float>(m_rationOneCount) * kRationOneWeight;
    weight += static_cast<float>(m_rawFishCount + m_cookedFishCount) * kUnknownFishWeight;
    for (size_t i = 0; i < m_rawSizedFish.size(); ++i)
        weight += static_cast<float>(m_rawSizedFish[i] + m_cookedSizedFish[i]) * kSizedFishWeights[i];
    weight += static_cast<float>(m_cartridgeCount) * kCartridgeWeight;
    weight += static_cast<float>(m_waterBottles.size()) * kWaterBottleWeight;
    weight += static_cast<float>(m_cookingKits.size()) * kCookingKitWeight;
    weight += static_cast<float>(m_portableLights.size()) * kPortableLightWeight;

    // 合計重量を返します。
    return weight;
}

float SceneNarakuProto::GetPickupWeightLimit() const
{
    return std::max(kPickupWeightLimit, GetMaxWeight());
}

float SceneNarakuProto::GetWeightRate() const
{
    return GetCurrentWeight() / GetMaxWeight();
}

float SceneNarakuProto::GetMoveSpeed() const
{
    const float levelMove = 1.0f + (2.50f - 1.0f) * GetLevelGrowth();
    float speed = m_debugPlayerParams.walkSpeed * levelMove * (1.0f + GetEquipmentBonus().walkSpeed);

    // 重量70%以上では移動速度を25%下げます。
    if (GetWeightRate() >= 0.70f) speed *= 0.75f;

    // 重量補正済みの歩行速度を返します。
    return RoundToHundredth(speed);
}

float SceneNarakuProto::GetStaminaCost(float baseCost) const
{
    float cost = baseCost * GetDepthLevelStaminaConsumptionMultiplier();
    if ((GetCellAttributeFlagsAt(m_player.pos, m_player.depth) & NarakuMap::CellAttributeWaterPuddle) != 0u)
        cost *= 1.20f;
    if (GetWeightRate() >= 0.90f) cost *= 2.0f;
    if (m_fullness <= 10.0f) cost *= 1.50f;
    else if (m_fullness <= 30.0f) cost *= 1.25f;
    return cost;
}

float SceneNarakuProto::GetDepthLevelStaminaConsumptionMultiplier() const
{
    return GetDepthLevelMultiplier(kStaminaConsumptionMultipliers, GetCurrentDepth(), m_level);
}

float SceneNarakuProto::GetDepthLevelStaminaRecoveryMultiplier() const
{
    return GetDepthLevelMultiplier(kStaminaRecoveryMultipliers, GetCurrentDepth(), m_level);
}

float SceneNarakuProto::GetDepthLevelMentalConsumptionMultiplier() const
{
    return GetDepthLevelMultiplier(kMentalConsumptionMultipliers, GetCurrentDepth(), m_level);
}

float SceneNarakuProto::GetDepthLevelFullnessConsumptionMultiplier() const
{
    return GetDepthLevelMultiplier(kFullnessConsumptionMultipliers, GetCurrentDepth(), m_level);
}

bool SceneNarakuProto::CanSpendStamina(float baseCost) const
{
    // 深度・レベル、重量、飢餓の補正後の消費量を支払えるか返します。
    return m_player.stamina >= GetStaminaCost(baseCost) && m_player.stamina > 0.0f;
}

void SceneNarakuProto::SpendStamina(float baseCost)
{
    // 各補正後の消費量を差し引き、0未満にならないようにします。
    m_player.stamina = std::max(0.0f, m_player.stamina - GetStaminaCost(baseCost));
}

int SceneNarakuProto::GetCurrentDepth() const
{
    if (m_mode == Mode::Loading && m_loadingSourceGateIndex >= 0 &&
        m_loadingSourceGateIndex < static_cast<int>(m_layerGates.size()))
    {
        const int destination = m_layerGates[m_loadingSourceGateIndex].destinationAreaIndex;
        if (destination >= 0 && destination < static_cast<int>(m_areas.size()))
            return ClampDepth(m_areas[destination].depth);
    }
    if (m_currentAreaIndex >= 0 && m_currentAreaIndex < static_cast<int>(m_areas.size()))
    {
        return ClampDepth(m_areas[m_currentAreaIndex].depth);
    }
    return 1;
}

float SceneNarakuProto::GetDepthExpMultiplier(int depth) const { return GetRulesForDepth(depth).regularExp; }
float SceneNarakuProto::GetDepthMovementExpMultiplier(int depth) const { return GetRulesForDepth(depth).movementExp; }
float SceneNarakuProto::GetDepthRewardMultiplier(int depth) const { return GetRulesForDepth(depth).reward; }
float SceneNarakuProto::GetDepthStayRewardMultiplier(int depth) const { return GetRulesForDepth(depth).stayReward; }

float SceneNarakuProto::GetLevelGrowth() const
{
    const float t = static_cast<float>(std::max(0, std::min(99, m_level - 1))) / 99.0f;
    return (1.0f - std::exp(-0.5f * t)) / (1.0f - std::exp(-0.5f));
}

SceneNarakuProto::EquipmentBonus SceneNarakuProto::GetEquipmentBonus() const
{
    EquipmentBonus bonus;
    auto addArmor = [&bonus](ArmorTier tier, bool head)
    {
        const float slot = head ? 0.4f : 0.6f;
        switch (tier)
        {
        case ArmorTier::Leather: bonus.defense += 0.10f * slot; break;
        case ArmorTier::Iron: bonus.defense += 0.25f * slot; break;
        case ArmorTier::RelicCovered: bonus.attack += 0.10f * slot; bonus.defense += 0.40f * slot; break;
        case ArmorTier::RelicHardened: bonus.defense += 0.80f * slot; break;
        case ArmorTier::RelicEnhanced: bonus.attack += 0.25f * slot; bonus.defense += 0.60f * slot; break;
        case ArmorTier::Relic:
            bonus.attack += head ? 0.10f : 0.15f;
            bonus.defense += head ? 0.30f : 0.45f;
            break;
        case ArmorTier::Unknown:
            break;
        default: break;
        }
    };
    addArmor(m_equippedHeadArmor, true);
    addArmor(m_equippedBodyArmor, false);
    if (HasRelicArmorSetEffect())
    {
        bonus.maxWeight += 1.00f;
        bonus.walkSpeed += 0.20f;
        bonus.runSpeed += 1.00f;
        bonus.hpRecoveryPerSecond += 2.0f;
        bonus.attack += 0.25f;
        bonus.miningSpeed += 0.50f;
    }
    if (HasUnknownArmorSetEffect())
    {
        bonus.maxWeight += 3.00f;
        bonus.walkSpeed += 0.75f;
        bonus.runSpeed += 4.50f;
        bonus.hpRecoveryMaxRatioPerSecond += 0.02f;
        bonus.attack += 1.00f;
        bonus.defense += 3.45f;
        bonus.miningSpeed += 1.00f;
    }
    switch (m_equippedWeapon)
    {
    case WeaponTier::NormalPickaxe: bonus.miningSpeed += 0.35f; break;
    case WeaponTier::SturdyPickaxe: bonus.miningSpeed += 0.80f; break;
    case WeaponTier::SharpPickaxe: bonus.miningSpeed += 1.00f; break;
    case WeaponTier::RelicPickaxe: bonus.miningSpeed += 1.50f; break;
    default: break;
    }
    return bonus;
}

float SceneNarakuProto::GetMaxHp() const
{
    const float levelValue = kPlayerBaseMaxHp + (1200.0f - kPlayerBaseMaxHp) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxHp));
}

float SceneNarakuProto::GetMaxStamina() const
{
    const float levelValue = kPlayerBaseMaxStamina + (800.0f - kPlayerBaseMaxStamina) * GetLevelGrowth();
    const float hungerScale = m_fullness <= 10.0f ? 0.70f : (m_fullness <= 25.0f ? 0.90f : 1.0f);
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxStamina) * hungerScale);
}

float SceneNarakuProto::GetMaxMental() const
{
    const float levelValue = kPlayerBaseMaxMental + (700.0f - kPlayerBaseMaxMental) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxMental));
}

float SceneNarakuProto::GetMaxWeight() const
{
    const float levelValue = kMaxWeight + (500.0f - kMaxWeight) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().maxWeight));
}

float SceneNarakuProto::GetStaminaRecoveryMultiplier() const
{
    const float levelValue = 1.0f + (5.40f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().staminaRecovery));
}

float SceneNarakuProto::GetMentalRecoveryMultiplier() const
{
    const float levelValue = 1.0f + (2.50f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().mentalRecovery));
}

float SceneNarakuProto::GetAttackPower() const
{
    const float levelValue = kPlayerBaseAttack * (1.0f + (3.0f - 1.0f) * GetLevelGrowth());
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().attack));
}

float SceneNarakuProto::GetDefenseMultiplier() const
{
    const float levelValue = kPlayerBaseDefense * (1.0f + (4.50f - 1.0f) * GetLevelGrowth());
    return RoundToHundredth(levelValue * (1.0f + GetEquipmentBonus().defense));
}

float SceneNarakuProto::GetRunSpeed() const
{
    const float levelMove = 1.0f + (2.50f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(m_debugPlayerParams.runSpeed * levelMove * (1.0f + GetEquipmentBonus().runSpeed));
}

float SceneNarakuProto::GetRopeSpeed(bool ascending) const
{
    const float maximum = ascending ? 2.50f : 12.0f;
    const float levelSpeed = 1.0f + (maximum - 1.0f) * GetLevelGrowth();
    const EquipmentBonus bonus = GetEquipmentBonus();
    return RoundToHundredth(m_debugPlayerParams.ropeSpeed * levelSpeed *
        (1.0f + (ascending ? bonus.ropeAscentSpeed : bonus.ropeDescentSpeed)));
}

float SceneNarakuProto::GetMiningSpeedMultiplier() const
{
    const float levelSpeed = 1.0f + (10.0f - 1.0f) * GetLevelGrowth();
    return RoundToHundredth(levelSpeed * (1.0f + GetEquipmentBonus().miningSpeed));
}

void SceneNarakuProto::PreserveResourceRatios(float oldMaxHp, float oldMaxStamina, float oldMaxMental)
{
    const float hpRatio = oldMaxHp > 0.0f ? m_player.hp / oldMaxHp : 1.0f;
    const float staminaRatio = oldMaxStamina > 0.0f ? m_player.stamina / oldMaxStamina : 1.0f;
    const float mentalRatio = oldMaxMental > 0.0f ? m_player.mental / oldMaxMental : 1.0f;
    m_player.hp = std::max(0.0f, std::min(GetMaxHp(), GetMaxHp() * hpRatio));
    m_player.stamina = std::max(0.0f, std::min(GetMaxStamina(), GetMaxStamina() * staminaRatio));
    m_player.mental = std::max(0.0f, std::min(GetMaxMental(), GetMaxMental() * mentalRatio));
}

int SceneNarakuProto::GetRequiredExp(int level) const
{
    static const int anchorLevels[] = { 1, 10, 20, 30, 40, 50, 60, 70, 80, 90, 99 };
    static const int anchorExp[] = { 100, 1000, 4000, 7500, 12000, 25000, 87500, 156000, 468000, 785625, 1500000 };
    level = std::max(1, std::min(99, level));
    for (int i = 0; i < 10; ++i)
    {
        if (level > anchorLevels[i + 1]) continue;
        const float span = static_cast<float>(anchorLevels[i + 1] - anchorLevels[i]);
        const float t = span > 0.0f ? static_cast<float>(level - anchorLevels[i]) / span : 0.0f;
        const double ratio = static_cast<double>(anchorExp[i + 1]) / static_cast<double>(anchorExp[i]);
        return static_cast<int>(std::llround(static_cast<double>(anchorExp[i]) * std::pow(ratio, t)));
    }
    return anchorExp[10];
}

void SceneNarakuProto::AwardExp(int amount)
{
    if (amount <= 0) return;
    if (m_level >= 100)
    {
        m_level100OverflowExp += amount;
        while (m_level100OverflowExp >= kLevel100ProtectionExp && m_levelProtection < std::numeric_limits<int>::max())
        {
            m_level100OverflowExp -= kLevel100ProtectionExp;
            ++m_levelProtection;
        }
        return;
    }

    m_currentExp += amount;
    while (m_level < 100)
    {
        const int required = GetRequiredExp(m_level);
        if (m_currentExp < required) break;
        const float oldHp = GetMaxHp();
        const float oldStamina = GetMaxStamina();
        const float oldMental = GetMaxMental();
        m_currentExp -= required;
        ++m_level;
        PreserveResourceRatios(oldHp, oldStamina, oldMental);
        ShowCenterNotification(u8"レベルが上がった！");
    }
    if (m_level >= 100 && m_currentExp > 0)
    {
        m_level100OverflowExp += m_currentExp;
        m_currentExp = 0;
        while (m_level100OverflowExp >= kLevel100ProtectionExp && m_levelProtection < std::numeric_limits<int>::max())
        {
            m_level100OverflowExp -= kLevel100ProtectionExp;
            ++m_levelProtection;
        }
    }
}

std::string SceneNarakuProto::FormatExp(std::int64_t value) const
{
    if (value < 1000) return std::to_string(value);
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(1) << static_cast<double>(value) / 1000.0 << 'k';
    return stream.str();
}

int SceneNarakuProto::GetDeathLevelLoss(DeathCause cause) const
{
    switch (cause)
    {
    case DeathCause::Fall: return 5;
    case DeathCause::UpperLoad: return 3;
    case DeathCause::Enemy:
    case DeathCause::Starvation: return 2;
    default: return 1;
    }
}

void SceneNarakuProto::ApplyDeathPenalty(DeathCause cause)
{
    const float oldHp = GetMaxHp();
    const float oldStamina = GetMaxStamina();
    const float oldMental = GetMaxMental();
    int loss = std::min(GetDeathLevelLoss(cause), std::max(0, m_level - 1));
    m_result.levelBeforeDeath = m_level;
    const int protectedLevels = std::min(loss, m_levelProtection);
    m_levelProtection -= protectedLevels;
    m_result.protectionConsumed = protectedLevels;
    loss -= protectedLevels;
    m_level = std::max(1, m_level - loss);
    m_result.levelAfterDeath = m_level;
    m_currentExp = 0;
    m_level100OverflowExp = 0;
    PreserveResourceRatios(oldHp, oldStamina, oldMental);
}

void SceneNarakuProto::ApplyAbandonPenalty()
{
    if (m_level <= 1 && m_currentExp <= 0) return;
    const float progress = static_cast<float>(m_level) +
        static_cast<float>(m_currentExp) / static_cast<float>(GetRequiredExp(m_level));
    const float result = std::max(1.0f, progress - 0.5f);
    m_level = std::max(1, std::min(100, static_cast<int>(std::floor(result))));
    if (m_level >= 100) m_currentExp = 0;
    else m_currentExp = static_cast<int>(std::round((result - std::floor(result)) * GetRequiredExp(m_level)));
}

void SceneNarakuProto::ApplyPlayerDamage(float damage, DeathCause cause, const char* reason)
{
    const float applied = std::max(1.0f, damage / std::max(1.0f, GetDefenseMultiplier()));
    if (cause != DeathCause::Fall && cause != DeathCause::Starvation &&
        m_player.hp - applied <= 0.0f && TryConsumeSurvivalRelic(true, false)) return;
    m_player.hp = std::max(0.0f, m_player.hp - applied);
    if (m_player.hp <= 0.0f) StartDeath(reason, cause);
}

void SceneNarakuProto::ApplyMentalDamage(float damage, DeathCause cause, const char* reason)
{
    if (m_player.mental - damage <= 0.0f && TryConsumeSurvivalRelic(false, true)) return;
    m_player.mental = std::max(0.0f, m_player.mental - damage);
    if (m_player.mental <= 0.0f) StartDeath(reason, cause);
}

int SceneNarakuProto::CountStoredRelics(RelicType type) const
{
    return static_cast<int>(std::count_if(m_storedInventory.begin(), m_storedInventory.end(),
        [type](const RelicItem& item) { return item.type == type; }));
}

bool SceneNarakuProto::RemoveStoredRelics(RelicType type, int count)
{
    if (count < 0 || CountStoredRelics(type) < count) return false;
    for (auto it = m_storedInventory.begin(); it != m_storedInventory.end() && count > 0;)
    {
        if (it->type == type) { it = m_storedInventory.erase(it); --count; }
        else ++it;
    }
    return count == 0;
}

bool SceneNarakuProto::IsNear(const Vec2& a, const Vec2& b, float range) const
{
    // 2点間距離が指定範囲内かを返します。
    return Distance(a, b) <= range;
}

SceneNarakuProto::Vec2 SceneNarakuProto::Normalize(const Vec2& value) const
{
    // ベクトルの長さを計算します。
    float length = std::sqrt(value.x * value.x + value.y * value.y);

    // 長さがほぼ0ならゼロ除算を避けてゼロベクトルを返します。
    if (length <= 0.0001f) return { 0.0f, 0.0f };

    // 各成分を長さで割って正規化します。
    return { value.x / length, value.y / length };
}

float SceneNarakuProto::Distance(const Vec2& a, const Vec2& b) const
{
    // X成分の差を計算します。
    float dx = a.x - b.x;

    // Y成分の差を計算します。
    float dy = a.y - b.y;

    // ピタゴラスの定理で距離を返します。
    return std::sqrt(dx * dx + dy * dy);
}

float SceneNarakuProto::Dot(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトルの内積を返します。
    return a.x * b.x + a.y * b.y;
}

SceneNarakuProto::Vec2 SceneNarakuProto::Add(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトル同士を加算します。
    return { a.x + b.x, a.y + b.y };
}

SceneNarakuProto::Vec2 SceneNarakuProto::Sub(const Vec2& a, const Vec2& b) const
{
    // 2Dベクトル同士を減算します。
    return { a.x - b.x, a.y - b.y };
}

SceneNarakuProto::Vec2 SceneNarakuProto::Mul(const Vec2& a, float scalar) const
{
    // 2Dベクトルにスカラーを掛けます。
    return { a.x * scalar, a.y * scalar };
}

bool SceneNarakuProto::IsInsideFloor(const FloorRegion& floor, const Vec2& pos) const
{
    // 床矩形の左端から右端までに入っているかを調べます。
    const bool insideX = pos.x >= floor.center.x - floor.halfSize.x && pos.x <= floor.center.x + floor.halfSize.x;

    // 床矩形の奥端から手前端までに入っているかを調べます。
    const bool insideY = pos.y >= floor.center.y - floor.halfSize.y && pos.y <= floor.center.y + floor.halfSize.y;

    // XとYの両方が範囲内なら床上として扱います。
    return insideX && insideY;
}

const SceneNarakuProto::FloorRegion* SceneNarakuProto::FindFloorAt(const Vec2& pos, float depth) const
{
    constexpr float kFloorDepthTolerance = 0.20f;

    for (const FloorRegion& floor : m_floorRegions)
    {
        if (std::fabs(floor.depth - depth) > kFloorDepthTolerance)
        {
            continue;
        }

        if (!IsInsideFloor(floor, pos))
        {
            continue;
        }

        const int layerIndex = NarakuMap::FindLayerIndexById(m_runtimeMap, floor.layerId);
        if (layerIndex >= 0)
        {
            const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
            int cellX = -1;
            int cellZ = -1;
            float fracX = 0.0f;
            float fracZ = 0.0f;
            if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
            {
                continue;
            }

            if (!NarakuMap::IsCellWalkable(layer, cellX, cellZ))
            {
                continue;
            }
        }

        return &floor;
    }

    return nullptr;
}

bool SceneNarakuProto::HasFloorAt(const Vec2& pos, float depth) const
{
    // 床ポインタが見つかるかどうかだけを真偽値に変換します。
    return FindFloorAt(pos, depth) != nullptr;
}

bool SceneNarakuProto::CanStandAt(const Vec2& pos, float depth) const
{
    if (!HasFloorAt(pos, depth))
    {
        return false;
    }

    const std::uint32_t flags = GetCellAttributeFlagsAt(pos, depth);
    return (flags & (NarakuMap::CellAttributeBlocked | NarakuMap::CellAttributeRemoved)) == 0u;
}

std::uint32_t SceneNarakuProto::GetCellAttributeFlagsAt(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return NarakuMap::CellAttributeNone;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
    {
        return NarakuMap::CellAttributeNone;
    }

    std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
    if (!NarakuMap::IsCellWalkable(layer, cellX, cellZ))
    {
        flags |= NarakuMap::CellAttributeBlocked;
    }
    return flags;
}

void SceneNarakuProto::UseSizedFish(FishSize fishSize, bool cooked)
{
    const size_t index = static_cast<size_t>(fishSize);
    if (index >= m_rawSizedFish.size()) return;
    std::array<int, 3>& counts = cooked ? m_cookedSizedFish : m_rawSizedFish;
    if (counts[index] <= 0 || (m_fullness >= kFullnessMaximum && m_player.mental >= GetMaxMental())) return;
    --counts[index];
    const float fullness = cooked ? kCookedSizedFishFullness[index] : kRawSizedFishFullness[index];
    const float mental = cooked ? kCookedSizedFishMental[index] : kRawSizedFishMental[index];
    m_fullness = std::min(kFullnessMaximum, m_fullness + fullness);
    m_player.mental = std::min(GetMaxMental(), m_player.mental + mental);
    AddMessage(u8"魚を食べました。");
}

void SceneNarakuProto::UpdateFishingPointRecharge(double gameSeconds)
{
    const auto update = [gameSeconds](std::vector<FishingPoint>& points)
    {
        for (FishingPoint& point : points)
        {
            if (point.remainingUses >= kFishingMaximumUses)
            {
                point.remainingUses = kFishingMaximumUses;
                point.rechargeGameSeconds = 0.0;
                continue;
            }
            point.rechargeGameSeconds += std::max(0.0, gameSeconds);
            while (point.rechargeGameSeconds >= kFishingRechargeGameSeconds && point.remainingUses < kFishingMaximumUses)
            {
                point.rechargeGameSeconds -= kFishingRechargeGameSeconds;
                ++point.remainingUses;
            }
            if (point.remainingUses >= kFishingMaximumUses) point.rechargeGameSeconds = 0.0;
        }
    };
    update(m_fishingPoints);
    for (int index = 0; index < static_cast<int>(m_areas.size()); ++index)
        if (index != m_currentAreaIndex) update(m_areas[static_cast<size_t>(index)].fishingPoints);
}

void SceneNarakuProto::DrawFishingConfirm()
{
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f }, ImGuiCond_Always, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 390.0f, 155.0f }, ImGuiCond_Always);
    NarakuUi::Begin(u8"釣り地点", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize);
    if (m_fishingPointIndex >= 0 && m_fishingPointIndex < static_cast<int>(m_fishingPoints.size()))
    {
        const FishingPoint& point = m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)];
        ImGui::Text(u8"水場: %s / 残り回数: %d / %d", point.lake ? u8"湖" : u8"池",
            point.remainingUses, kFishingMaximumUses);
        ImGui::TextDisabled(u8"開始時に最大スタミナの10%%を消費します。");
        if (ImGui::Button(u8"釣る", { 120.0f, 0.0f })) StartFishing(m_fishingPointIndex);
        ImGui::SameLine();
    }
    if (NarakuUi::BackButton(u8"戻る", { 120.0f, 0.0f }))
    {
        m_fishingPointIndex = -1;
        m_mode = Mode::Explore;
    }
    ImGui::End();
}

void SceneNarakuProto::StartFishing(int pointIndex)
{
    if (pointIndex < 0 || pointIndex >= static_cast<int>(m_fishingPoints.size())) return;
    FishingPoint& point = m_fishingPoints[static_cast<size_t>(pointIndex)];
    if (GetCurrentDepth() != 5) { AddMessage(u8"釣りは第五層の池・湖で行えます。"); return; }
    const float staminaCost = GetMaxStamina() * kFishingStartStaminaRatio;
    if (point.remainingUses <= 0) { AddMessage(u8"この釣り地点では現在釣れません。"); return; }
    if (m_player.stamina < staminaCost) { AddMessage(u8"釣りを始めるスタミナが足りません。"); return; }
    if (GetCurrentWeight() + kSizedFishWeights.back() > GetPickupWeightLimit())
    { AddMessage(u8"大きな魚を持てる重量の空きがありません。"); return; }
    m_player.stamina -= staminaCost;
    --point.remainingUses;
    if (point.remainingUses == kFishingMaximumUses - 1) point.rechargeGameSeconds = 0.0;
    SaveCurrentAreaState();
    m_fishingPointIndex = pointIndex;
    m_fishingPhase = FishingPhase::Waiting;
    m_fishingTimer = RandomFloat(5.0f, 15.0f);
    m_fishingPreviousHp = m_player.hp;
    m_mode = Mode::Explore;
    AddMessage(u8"釣りを開始しました。反応があったらFキーを押してください。");
}

void SceneNarakuProto::CancelFishing(const char* reason)
{
    if (m_fishingPhase == FishingPhase::None) return;
    m_fishingPhase = FishingPhase::None;
    m_fishingPointIndex = -1;
    m_fishingTimer = 0.0f;
    if (reason && *reason) AddMessage(reason);
}

void SceneNarakuProto::CompleteFishing()
{
    if (m_fishingPointIndex < 0 || m_fishingPointIndex >= static_cast<int>(m_fishingPoints.size()))
    { CancelFishing(nullptr); return; }
    FishingPoint& point = m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)];
    const float roll = RandomFloat(0.0f, 1.0f);
    if (point.lake && GetCurrentDepth() == 5 && roll < 0.05f)
    {
        ++m_rawFishCount;
        AddMessage(u8"見たことない魚を釣り上げました。");
    }
    else
    {
        const float sizeRoll = RandomFloat(0.0f, 1.0f);
        int size = 0;
        if (point.lake) size = sizeRoll < 0.30f ? 0 : (sizeRoll < 0.70f ? 1 : 2);
        else size = sizeRoll < 0.60f ? 0 : (sizeRoll < 0.85f ? 1 : 2);
        ++m_rawSizedFish[static_cast<size_t>(size)];
        static const char* names[] = { u8"魚（小）", u8"魚（中）", u8"魚（大）" };
        AddMessage((std::string(names[size]) + u8"を釣り上げました。").c_str());
    }
    m_fishingPhase = FishingPhase::None;
    m_fishingPointIndex = -1;
    SaveCurrentAreaState();
}

void SceneNarakuProto::UpdateFishing(float dt)
{
    if (m_fishingPhase == FishingPhase::None) return;
    if (IsActionUIBackTrigger()) { CancelFishing(u8"釣りを中断しました。"); return; }
    if (m_lastFrameMovementDistance > 0.001f || m_player.attackTimer > 0.0f ||
        m_player.stepTimer > 0.0f || m_player.hp < m_fishingPreviousHp || !m_player.grounded || m_player.onRope)
    { CancelFishing(u8"行動または被弾により釣りが中断されました。"); return; }
    m_fishingPreviousHp = m_player.hp;
    m_fishingTimer -= dt;
    if (m_fishingPhase == FishingPhase::Waiting)
    {
        if (IsActionInteractTrigger()) { CancelFishing(u8"早く引きすぎて魚を逃しました。"); return; }
        if (m_fishingTimer <= 0.0f)
        {
            m_fishingPhase = FishingPhase::Bite;
            m_fishingTimer = m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)].lake ? 1.25f : 1.0f;
            AddMessage(u8"反応あり！ Fキーで引き上げます。");
        }
    }
    else if (m_fishingPhase == FishingPhase::Bite)
    {
        if (IsActionInteractTrigger()) { m_fishingPhase = FishingPhase::Landing; m_fishingTimer = kFishingLandingDuration; }
        else if (m_fishingTimer <= 0.0f) CancelFishing(u8"魚を逃しました。");
    }
    else if (m_fishingPhase == FishingPhase::Landing && m_fishingTimer <= 0.0f) CompleteFishing();
}

bool SceneNarakuProto::CanTraverseGround(const Vec2& from, const Vec2& to, float depth) const
{
    if (!HasFloorAt(to, depth))
    {
        return false;
    }

    const float fromHeight = SampleTerrainHeightOffsetAt(from, depth);
    const float toHeight = SampleTerrainHeightOffsetAt(to, depth);
    const float climbDelta = toHeight - fromHeight;
    const float dropDelta = fromHeight - toHeight;
    const std::uint32_t fromFlags = GetCellAttributeFlagsAt(from, depth);
    const std::uint32_t toFlags = GetCellAttributeFlagsAt(to, depth);
    const bool dropAllowed = ((fromFlags | toFlags) & NarakuMap::CellAttributeDropAllowed) != 0u;
    const bool cliffEdge = ((fromFlags | toFlags) & NarakuMap::CellAttributeCliffEdge) != 0u;

    if (climbDelta > kMaxWalkClimbHeight + kSlopeHeightTolerance)
    {
        return false;
    }

    if (dropDelta > kMaxWalkDropHeight + kSlopeHeightTolerance && !dropAllowed)
    {
        return false;
    }

    if (cliffEdge && dropDelta > kCliffEdgeBlockDropHeight + kSlopeHeightTolerance && !dropAllowed)
    {
        return false;
    }

    return true;
}

bool SceneNarakuProto::CanTraverseAir(const Vec2& to, float depth) const
{
    const int layerIndex = FindLayerIndexAt(to, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return false;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, to, cellX, cellZ, fracX, fracZ))
    {
        return false;
    }

    const std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
    if ((flags & NarakuMap::CellAttributeRemoved) != 0u)
    {
        return false;
    }

    if ((flags & NarakuMap::CellAttributeBlocked) == 0u &&
        GetGroundWorldY(to, depth) > m_player.feetWorldY + kSlopeHeightTolerance)
    {
        return false;
    }

    return true;
}

SceneNarakuProto::Vec2 SceneNarakuProto::ResolveFloorMove(const Vec2& from, const Vec2& to, float depth) const
{
    auto tryResolveSingleStep = [this, depth](const Vec2& stepFrom, const Vec2& stepTo, Vec2& outResolved) -> bool
    {
        if (CanTraverseGround(stepFrom, stepTo, depth))
        {
            outResolved = stepTo;
            return true;
        }

        const Vec2 xOnly = { stepTo.x, stepFrom.y };
        if (CanTraverseGround(stepFrom, xOnly, depth))
        {
            outResolved = xOnly;
            return true;
        }

        const Vec2 yOnly = { stepFrom.x, stepTo.y };
        if (CanTraverseGround(stepFrom, yOnly, depth))
        {
            outResolved = yOnly;
            return true;
        }

        outResolved = stepFrom;
        return false;
    };

    const float totalDistance = Distance(from, to);
    if (totalDistance <= kSlopeMoveSampleStep)
    {
        Vec2 resolved = from;
        return tryResolveSingleStep(from, to, resolved) ? resolved : from;
    }

    const int stepCount = std::max(1, static_cast<int>(std::ceil(totalDistance / kSlopeMoveSampleStep)));
    const Vec2 delta = Sub(to, from);
    Vec2 current = from;

    for (int stepIndex = 1; stepIndex <= stepCount; ++stepIndex)
    {
        const float t = static_cast<float>(stepIndex) / static_cast<float>(stepCount);
        const Vec2 stepTarget = Add(from, Mul(delta, t));
        Vec2 resolved = current;
        if (!tryResolveSingleStep(current, stepTarget, resolved))
        {
            break;
        }
        current = resolved;
    }

    return current;
}

SceneNarakuProto::Vec2 SceneNarakuProto::ResolveAirMove(const Vec2& from, const Vec2& to, float depth) const
{
    auto tryResolveSingleStep = [this, depth](const Vec2& stepFrom, const Vec2& stepTo, Vec2& outResolved) -> bool
    {
        if (CanTraverseAir(stepTo, depth))
        {
            outResolved = stepTo;
            return true;
        }

        const Vec2 xOnly = { stepTo.x, stepFrom.y };
        if (CanTraverseAir(xOnly, depth))
        {
            outResolved = xOnly;
            return true;
        }

        const Vec2 yOnly = { stepFrom.x, stepTo.y };
        if (CanTraverseAir(yOnly, depth))
        {
            outResolved = yOnly;
            return true;
        }

        outResolved = stepFrom;
        return false;
    };

    const float totalDistance = Distance(from, to);
    if (totalDistance <= kSlopeMoveSampleStep)
    {
        Vec2 resolved = from;
        return tryResolveSingleStep(from, to, resolved) ? resolved : from;
    }

    const int stepCount = std::max(1, static_cast<int>(std::ceil(totalDistance / kSlopeMoveSampleStep)));
    const Vec2 delta = Sub(to, from);
    Vec2 current = from;
    for (int stepIndex = 1; stepIndex <= stepCount; ++stepIndex)
    {
        const float t = static_cast<float>(stepIndex) / static_cast<float>(stepCount);
        const Vec2 stepTarget = Add(from, Mul(delta, t));
        Vec2 resolved = current;
        if (!tryResolveSingleStep(current, stepTarget, resolved))
        {
            break;
        }
        current = resolved;
    }
    return current;
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetTerrainDownhillDirection(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return {};
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ) || layer.cellSize <= 0.0f)
    {
        return {};
    }

    const float h00 = NarakuMap::GetVertexHeight(layer, cellX, cellZ);
    const float h10 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ);
    const float h01 = NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1);
    const float h11 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1);
    const float riseX = ((h10 - h00) * (1.0f - fracZ) + (h11 - h01) * fracZ) / layer.cellSize;
    const float riseZ = ((h01 - h00) * (1.0f - fracX) + (h11 - h10) * fracX) / layer.cellSize;
    if (std::sqrt(riseX * riseX + riseZ * riseZ) <= kSlopeHeightTolerance)
    {
        return {};
    }
    return Normalize({ -riseX, -riseZ });
}

bool SceneNarakuProto::RestorePlayerToSafeGround()
{
    if (!m_player.hasSafeGroundPos)
    {
        return false;
    }

    m_player.pos = m_player.lastSafeGroundPos;
    m_player.depth = m_player.lastSafeGroundDepth;
    m_player.grounded = true;
    m_player.onRope = false;
    m_activeRope = -1;
    m_player.airTime = 0.0f;
    m_player.verticalSpeed = 0.0f;
    m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
    m_player.peakFeetWorldY = m_player.feetWorldY;
    m_player.landingRecoveryTimer = 0.0f;
    m_player.blockedCellAirTime = 0.0f;
    m_player.blockedCellVelocity = {};
    return true;
}

int SceneNarakuProto::FindNearestRopeIndex(float range) const
{
    // 近いロープを選ぶため、現在の最短距離を範囲上限から始めます。
    float bestDistance = range;

    // 見つかったロープ番号です。未発見なら -1 のままにします。
    int bestIndex = -1;

    // 全ロープを調べ、範囲内で一番近いものを探します。
    for (int i = 0; i < static_cast<int>(m_ropePoints.size()); ++i)
    {
        const RopePoint& rope = m_ropePoints[i];
        const float topDistance = std::fabs(m_player.depth - rope.topDepth) <= 0.35f ? Distance(m_player.pos, rope.topPos) : range + 1.0f;
        const float bottomDistance = std::fabs(m_player.depth - rope.bottomDepth) <= 0.35f ? Distance(m_player.pos, rope.bottomPos) : range + 1.0f;
        const float d = std::min(topDistance, bottomDistance);

        // 現在の候補より近ければ採用します。
        if (d <= bestDistance)
        {
            bestDistance = d;
            bestIndex = i;
        }
    }

    // 範囲内にロープがなければ -1 を返します。
    return bestIndex;
}

int SceneNarakuProto::FindFallingRopeIndex(float radius, float& outProgress) const
{
    outProgress = 0.0f;
    const float radiusSq = radius * radius;
    float bestDistanceSq = radiusSq;
    int bestIndex = -1;

    const float playerX = m_player.pos.x;
    const float playerY = m_player.feetWorldY + kRopePlayerHangOffset;
    const float playerZ = m_player.pos.y;

    for (int i = 0; i < static_cast<int>(m_ropePoints.size()); ++i)
    {
        const RopePoint& rope = m_ropePoints[i];
        const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);
        const float segmentX = traversal.bottomPosition.x - traversal.topPosition.x;
        const float segmentY = traversal.bottomWorldY - traversal.topWorldY;
        const float segmentZ = traversal.bottomPosition.y - traversal.topPosition.y;
        const float segmentLengthSq = segmentX * segmentX + segmentY * segmentY + segmentZ * segmentZ;

        float progress = 0.0f;
        if (segmentLengthSq > 0.000001f)
        {
            const float playerFromTopX = playerX - traversal.topPosition.x;
            const float playerFromTopY = playerY - traversal.topWorldY;
            const float playerFromTopZ = playerZ - traversal.topPosition.y;
            progress = std::max(0.0f, std::min(1.0f,
                (playerFromTopX * segmentX + playerFromTopY * segmentY + playerFromTopZ * segmentZ) / segmentLengthSq));
        }

        const float closestX = traversal.topPosition.x + segmentX * progress;
        const float closestY = traversal.topWorldY + segmentY * progress;
        const float closestZ = traversal.topPosition.y + segmentZ * progress;
        const float distanceX = playerX - closestX;
        const float distanceY = playerY - closestY;
        const float distanceZ = playerZ - closestZ;
        const float distanceSq = distanceX * distanceX + distanceY * distanceY + distanceZ * distanceZ;

        if (distanceSq <= bestDistanceSq)
        {
            bestDistanceSq = distanceSq;
            bestIndex = i;
            outProgress = progress;
        }
    }

    return bestIndex;
}

bool SceneNarakuProto::TryLeaveRopeSide(int ropeIndex, float leaveSign, const Vec2& cameraRight)
{
    // 無効なロープ番号なら何もしません。
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return false;
    }

    // 対象ロープを取得します。
    const RopePoint& rope = m_ropePoints[ropeIndex];

    const Vec2 ropePos = GetRopePosition(ropeIndex, m_ropeProgress);
    const Vec2 leavePos = Add(ropePos, Mul(cameraRight, leaveSign * 0.80f));

    // 候補位置に床があり、地形条件も満たすならそこへ降ります。
    if (CanTraverseGround(ropePos, leavePos, m_player.depth))
    {
        m_player.pos = leavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        m_player.grounded = true;
        m_player.verticalSpeed = 0.0f;
        m_player.airTime = 0.0f;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.landingRecoveryTimer = 0.0f;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 深度が端にかなり近い場合は端深度へ吸着して、降りられるかをもう一度試します。
    const bool useBottom = m_ropeProgress >= 0.5f;
    const float endpointDepth = useBottom ? rope.bottomDepth : rope.topDepth;
    const Vec2 endpointPos = useBottom ? rope.bottomPos : rope.topPos;
    const Vec2 endpointLeavePos = Add(endpointPos, Mul(cameraRight, leaveSign * 0.80f));
    if ((m_ropeProgress <= 0.05f || m_ropeProgress >= 0.95f) && CanTraverseGround(endpointPos, endpointLeavePos, endpointDepth))
    {
        m_player.depth = endpointDepth;
        m_player.pos = endpointLeavePos;
        m_player.onRope = false;
        m_activeRope = -1;
        m_player.grounded = true;
        m_player.verticalSpeed = 0.0f;
        m_player.airTime = 0.0f;
        m_player.feetWorldY = GetGroundWorldY(m_player.pos, m_player.depth);
        m_player.peakFeetWorldY = m_player.feetWorldY;
        m_player.landingRecoveryTimer = 0.0f;
        AddMessage(u8"ロープを離しました。");
        return true;
    }

    // 周囲に足場がない場合はロープから離れません。
    return false;
}

void SceneNarakuProto::AddMessage(const std::string& message)
{
    // 新しいログを末尾に追加します。
    m_messages.push_back(message);

    // ログが増えすぎないよう古いものを削除します。
    if (m_messages.size() > 24) m_messages.erase(m_messages.begin());
}

void SceneNarakuProto::ShowCenterNotification(const std::string& message)
{
    m_centerNotification = message;
    m_centerNotificationTimer = 1.5f;
}

void SceneNarakuProto::DiscoverNearbyMiningPoints()
{
    // すべての採掘ポイントを確認します。
    for (MiningPoint& point : m_miningPoints)
    {
        // 未発見かつプレイヤーが近いポイントだけ発見済みにします。
        if (!point.discovered && std::fabs(m_player.depth - point.depth) <= 0.35f && IsNear(m_player.pos, point.pos, kDiscoverRange))
        {
            // 採掘ポイントを発見済みにします。
            point.discovered = true;

            // HUDログに発見を出します。
            AddMessage(u8"採掘ポイントを発見しました。");
        }
    }
    for (FishingPoint& point : m_fishingPoints)
    {
        if (!point.discovered && std::fabs(m_player.depth - point.depth) <= 0.35f &&
            IsNear(m_player.pos, point.pos, kDiscoveryRange))
        {
            point.discovered = true;
            AddMessage(u8"釣り地点を発見しました。");
        }
    }
}

void SceneNarakuProto::DropInventoryItem(int index)
{
    // 範囲外の番号なら何もしません。
    if (index < 0 || index >= static_cast<int>(m_inventory.size())) return;

    // 選択旧器を現在位置の地面旧器として追加します。
    m_groundRelics.push_back({ m_inventory[index], m_player.pos, m_player.depth, true, -1 });

    // 所持品から選択旧器を削除します。
    m_inventory.erase(m_inventory.begin() + index);

    // HUDログに捨てたことを出します。
    AddMessage(u8"旧器を捨てました。");
}

void SceneNarakuProto::TogglePinAt(const Vec2& worldPos)
{
    // 既存ピンの近くなら削除扱いにします。
    for (int i = 0; i < static_cast<int>(m_pins.size()); ++i)
    {
        // クリック位置から1m以内のピンを削除対象にします。
        if (Distance(m_pins[i], worldPos) <= 1.0f)
        {
            // 対象ピンを削除します。
            m_pins.erase(m_pins.begin() + i);
            if (m_overlayReturnMode == Mode::Surface) m_surfacePins = m_pins;

            // HUDログに削除を出します。
            AddMessage(u8"ピンを削除しました。");
            SaveProgress();

            // 削除したので追加処理は行いません。
            return;
        }
    }

    // 近くにピンがなければ新しいピンを追加します。
    m_pins.push_back(worldPos);
    if (m_overlayReturnMode == Mode::Surface) m_surfacePins = m_pins;

    // HUDログに追加を出します。
    AddMessage(u8"ピンを設置しました。");
    SaveProgress();
}

SceneNarakuProto::Vec2 SceneNarakuProto::ScreenToWorld(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& mousePos, float zoom, const Vec2& focusPos) const
{
    float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * zoom;
    float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * zoom;

    if (std::fabs(scaleX) < 0.001f) scaleX = 1.0f;
    if (std::fabs(scaleY) < 0.001f) scaleY = 1.0f;

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;

    float dx = (mousePos.x - centerX) / scaleX;
    float dy = (centerY - mousePos.y) / scaleY;

    return { focusPos.x + dx, focusPos.y + dy };
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float zoom, const Vec2& focusPos) const
{
    float scaleX = (canvasSize.x / (m_worldHalfSize * 2.0f)) * zoom;
    float scaleY = (canvasSize.y / (m_worldHalfSize * 2.0f)) * zoom;

    float centerX = canvasPos.x + canvasSize.x * 0.5f;
    float centerY = canvasPos.y + canvasSize.y * 0.5f;

    float dx = worldPos.x - focusPos.x;
    float dy = worldPos.y - focusPos.y;

    return { centerX + dx * scaleX, centerY - dy * scaleY };
}

int SceneNarakuProto::FindLayerIndexByDepth(float depth, float tolerance) const
{
    for (int i = 0; i < static_cast<int>(m_runtimeMap.terrainLayers.size()); ++i)
    {
        if (std::fabs(m_runtimeMap.terrainLayers[i].layerDepth - depth) <= tolerance)
        {
            return i;
        }
    }
    return -1;
}

int SceneNarakuProto::FindLayerIndexAt(const Vec2& pos, float depth, float tolerance) const
{
    for (int i = 0; i < static_cast<int>(m_runtimeMap.terrainLayers.size()); ++i)
    {
        const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[i];
        if (std::fabs(layer.layerDepth - depth) > tolerance)
        {
            continue;
        }

        int cellX = -1;
        int cellZ = -1;
        float fracX = 0.0f;
        float fracZ = 0.0f;
        if (TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
        {
            return i;
        }
    }

    return -1;
}

bool SceneNarakuProto::TryGetLayerCellAt(const NarakuMap::TerrainLayer& layer, const Vec2& pos, int& outCellX, int& outCellZ, float& outFracX, float& outFracZ) const
{
    if (layer.gridWidth < 2 || layer.gridHeight < 2 || layer.cellSize <= 0.0f)
    {
        return false;
    }

    const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float localX = (pos.x - minX) / layer.cellSize;
    const float localZ = (pos.y - minZ) / layer.cellSize;
    if (localX < 0.0f || localZ < 0.0f || localX > static_cast<float>(layer.gridWidth - 1) || localZ > static_cast<float>(layer.gridHeight - 1))
    {
        return false;
    }

    outCellX = static_cast<int>(std::floor(localX));
    outCellZ = static_cast<int>(std::floor(localZ));
    outCellX = std::max(0, std::min(outCellX, layer.gridWidth - 2));
    outCellZ = std::max(0, std::min(outCellZ, layer.gridHeight - 2));
    outFracX = std::max(0.0f, std::min(localX - static_cast<float>(outCellX), 1.0f));
    outFracZ = std::max(0.0f, std::min(localZ - static_cast<float>(outCellZ), 1.0f));
    return true;
}

float SceneNarakuProto::SampleTerrainHeightOffsetAt(const Vec2& pos, float depth) const
{
    const int layerIndex = FindLayerIndexAt(pos, depth);
    if (layerIndex < 0 || layerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return 0.0f;
    }

    const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[layerIndex];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(layer, pos, cellX, cellZ, fracX, fracZ))
    {
        return 0.0f;
    }

    const float h00 = NarakuMap::GetVertexHeight(layer, cellX, cellZ);
    const float h10 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ);
    const float h01 = NarakuMap::GetVertexHeight(layer, cellX, cellZ + 1);
    const float h11 = NarakuMap::GetVertexHeight(layer, cellX + 1, cellZ + 1);
    const float hx0 = h00 + (h10 - h00) * fracX;
    const float hx1 = h01 + (h11 - h01) * fracX;
    return hx0 + (hx1 - hx0) * fracZ;
}

float SceneNarakuProto::GetGroundWorldY(const Vec2& pos, float depth) const
{
    return SampleTerrainHeightOffsetAt(pos, depth) - depth * 0.35f;
}

float SceneNarakuProto::GetPlayerAirborneOffset() const
{
    if (m_player.grounded || m_player.onRope)
    {
        return 0.0f;
    }

    return std::max(0.0f, m_player.feetWorldY - GetGroundWorldY(m_player.pos, m_player.depth));
}

SceneNarakuProto::RopeTraversalEndpoints SceneNarakuProto::GetRopeTraversalEndpoints(const RopePoint& rope) const
{
    RopeTraversalEndpoints result = {};
    result.supportPosition = rope.topPos;
    result.supportGroundWorldY = GetGroundWorldY(rope.topPos, rope.topDepth);
    result.topPosition = rope.topPos;
    result.topWorldY = result.supportGroundWorldY;
    result.bottomPosition = rope.bottomPos;
    result.bottomWorldY = GetGroundWorldY(rope.bottomPos, rope.bottomDepth);

    if (m_ropeSupportModel == nullptr)
    {
        return result;
    }

    Vec2 descentDirection = Normalize(Sub(rope.bottomPos, rope.topPos));
    if (Distance({}, descentDirection) <= 0.001f)
    {
        descentDirection = { 0.0f, 1.0f };
    }

    const int topLayerIndex = FindLayerIndexAt(rope.topPos, rope.topDepth);
    if (topLayerIndex < 0 || topLayerIndex >= static_cast<int>(m_runtimeMap.terrainLayers.size()))
    {
        return result;
    }

    const NarakuMap::TerrainLayer& topLayer =
        m_runtimeMap.terrainLayers[static_cast<std::size_t>(topLayerIndex)];
    int cellX = -1;
    int cellZ = -1;
    float fracX = 0.0f;
    float fracZ = 0.0f;
    if (!TryGetLayerCellAt(topLayer, rope.topPos, cellX, cellZ, fracX, fracZ))
    {
        return result;
    }

    const float minX = topLayer.center.x -
        static_cast<float>(topLayer.gridWidth - 1) * topLayer.cellSize * 0.5f;
    const float minZ = topLayer.center.z -
        static_cast<float>(topLayer.gridHeight - 1) * topLayer.cellSize * 0.5f;
    result.supportPosition = {
        minX + (static_cast<float>(cellX) + 0.5f) * topLayer.cellSize,
        minZ + (static_cast<float>(cellZ) + 0.5f) * topLayer.cellSize };
    result.supportGroundWorldY = GetGroundWorldY(result.supportPosition, rope.topDepth);

    const float directionMaximum = std::max(
        std::fabs(descentDirection.x),
        std::fabs(descentDirection.y));
    const float outsideDistance = topLayer.cellSize * 0.5f /
        std::max(0.001f, directionMaximum) + kRopeVisualDiameter * 0.5f;
    result.topPosition = Add(result.supportPosition, Mul(descentDirection, outsideDistance));
    result.topWorldY = result.supportGroundWorldY + kRopeAnchorHeight;
    return result;
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetRopePosition(int ropeIndex, float progress) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return m_player.pos;
    }

    const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(m_ropePoints[ropeIndex]);
    const float t = std::max(0.0f, std::min(progress, 1.0f));
    return {
        traversal.topPosition.x + (traversal.bottomPosition.x - traversal.topPosition.x) * t,
        traversal.topPosition.y + (traversal.bottomPosition.y - traversal.topPosition.y) * t };
}

float SceneNarakuProto::GetRopeWorldY(int ropeIndex, float progress) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return GetGroundWorldY(m_player.pos, m_player.depth);
    }

    const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(m_ropePoints[ropeIndex]);
    const float t = std::max(0.0f, std::min(progress, 1.0f));
    return traversal.topWorldY + (traversal.bottomWorldY - traversal.topWorldY) * t;
}

float SceneNarakuProto::GetRopePlayerFeetWorldY(int ropeIndex, float progress) const
{
    return GetRopeWorldY(ropeIndex, progress) - kRopePlayerHangOffset;
}

float SceneNarakuProto::GetBottomRopeGrabProgress(int ropeIndex) const
{
    if (ropeIndex < 0 || ropeIndex >= static_cast<int>(m_ropePoints.size()))
    {
        return 1.0f;
    }

    const RopePoint& rope = m_ropePoints[ropeIndex];
    const RopeTraversalEndpoints traversal = GetRopeTraversalEndpoints(rope);
    const float targetFeetWorldY = GetGroundWorldY(rope.bottomPos, rope.bottomDepth) +
        kBottomRopeGrabClearance;
    const float targetGrabWorldY = targetFeetWorldY + kRopePlayerHangOffset;
    const float verticalDelta = traversal.bottomWorldY - traversal.topWorldY;
    if (std::fabs(verticalDelta) <= 0.0001f)
    {
        return 1.0f;
    }

    return std::max(0.0f, std::min(1.0f,
        (targetGrabWorldY - traversal.topWorldY) / verticalDelta));
}
DirectX::XMFLOAT3 SceneNarakuProto::GetTerrainVertexWorld3D(const NarakuMap::TerrainLayer& layer, int gridX, int gridZ, float heightOffset) const
{
    const float minX = layer.center.x - (static_cast<float>(layer.gridWidth - 1) * layer.cellSize * 0.5f);
    const float minZ = layer.center.z - (static_cast<float>(layer.gridHeight - 1) * layer.cellSize * 0.5f);
    const float x = minX + static_cast<float>(gridX) * layer.cellSize;
    const float z = minZ + static_cast<float>(gridZ) * layer.cellSize;
    const float terrainHeight = NarakuMap::GetVertexHeight(layer, gridX, gridZ);
    return { x, terrainHeight + heightOffset - layer.layerDepth * 0.35f, z };
}

DirectX::XMFLOAT3 SceneNarakuProto::GetTerrainVertexNormal(
    const NarakuMap::TerrainLayer& layer, int gridX, int gridZ) const
{
    using namespace DirectX;

    const int leftX = std::max(0, gridX - 1);
    const int rightX = std::min(layer.gridWidth - 1, gridX + 1);
    const int nearZ = std::max(0, gridZ - 1);
    const int farZ = std::min(layer.gridHeight - 1, gridZ + 1);
    const XMFLOAT3 left = GetTerrainVertexWorld3D(layer, leftX, gridZ);
    const XMFLOAT3 right = GetTerrainVertexWorld3D(layer, rightX, gridZ);
    const XMFLOAT3 nearPoint = GetTerrainVertexWorld3D(layer, gridX, nearZ);
    const XMFLOAT3 farPoint = GetTerrainVertexWorld3D(layer, gridX, farZ);

    const XMVECTOR tangentX = XMVectorSubtract(XMLoadFloat3(&right), XMLoadFloat3(&left));
    const XMVECTOR tangentZ = XMVectorSubtract(XMLoadFloat3(&farPoint), XMLoadFloat3(&nearPoint));
    XMVECTOR normalVector = XMVector3Cross(tangentZ, tangentX);
    if (XMVectorGetX(XMVector3LengthSq(normalVector)) <= 0.000001f)
    {
        normalVector = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    }
    else
    {
        normalVector = XMVector3Normalize(normalVector);
    }

    XMFLOAT3 normal = {};
    XMStoreFloat3(&normal, normalVector);
    if (normal.y < 0.0f)
    {
        normal.x = -normal.x;
        normal.y = -normal.y;
        normal.z = -normal.z;
    }
    return normal;
}

DirectX::XMFLOAT3 SceneNarakuProto::ToWorld3D(const Vec2& pos, float depth, float heightOffset) const
{
    const float terrainHeight = SampleTerrainHeightOffsetAt(pos, depth);
    return { pos.x, terrainHeight + heightOffset - depth * 0.35f, pos.y };
}

void SceneNarakuProto::DrawDebugBox3D(const DirectX::XMFLOAT3& pos, const DirectX::XMFLOAT3& scale, float yawRad) const
{
    using namespace DirectX;

    // 指定サイズへ拡大する行列を作ります。
    const XMMATRIX scaling = XMMatrixScaling(scale.x, scale.y, scale.z);

    // Y軸回転で向きを調整する行列を作ります。
    const XMMATRIX rotation = XMMatrixRotationY(yawRad);

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 拡大、回転、移動の順でワールド行列を組み立てます。
    const XMMATRIX worldMatrix = scaling * rotation * translation;

    // 既存Geometoryは行列を転置して渡す前提なので、ワールド行列も転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);

    // デバッグ箱を描画します。
    Geometory::DrawBox();
}

void SceneNarakuProto::DrawDebugSphere3D(const DirectX::XMFLOAT3& pos, float radius) const
{
    using namespace DirectX;

    // 半径をXYZ同じ倍率として扱い、球サイズを調整します。
    const XMMATRIX scaling = XMMatrixScaling(radius, radius, radius);

    // 指定位置へ移動する行列を作ります。
    const XMMATRIX translation = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // 拡大してから移動するワールド行列を組み立てます。
    const XMMATRIX worldMatrix = scaling * translation;

    // 既存Geometoryは行列を転置して渡す前提なので、ワールド行列も転置して保存します。
    XMFLOAT4X4 world;
    XMStoreFloat4x4(&world, XMMatrixTranspose(worldMatrix));
    Geometory::SetWorld(world);

    // デバッグ球を描画します。
    Geometory::DrawSphere();
}

SceneNarakuProto::Vec2 SceneNarakuProto::WorldToObliqueCanvas(const Vec2& canvasPos, const Vec2& canvasSize, const Vec2& worldPos, float depthOffset) const
{
    // 斜め見下ろし用にワールド座標を45度回したX成分へ変換します。
    float projectedX = worldPos.x - worldPos.y;

    // 斜め見下ろし用に奥行きを圧縮したY成分へ変換します。
    float projectedY = (worldPos.x + worldPos.y) * 0.45f;

    // 深度が大きいほど画面下へずらし、潜っている感覚を足します。
    projectedY += depthOffset * 0.35f;

    // 投影後のXをキャンバス幅に収まるよう0-1へ正規化します。
    float nx = (projectedX / (m_worldHalfSize * 2.0f) + 1.0f) * 0.5f;

    // 投影後のYをキャンバス高さに収まるよう0-1へ正規化します。
    float ny = (projectedY / (m_worldHalfSize * 1.35f) + 1.0f) * 0.5f;

    // 正規化座標をキャンバス上のスクリーン座標へ変換します。
    return { canvasPos.x + nx * canvasSize.x, canvasPos.y + ny * canvasSize.y };
}

void SceneNarakuProto::DrawMiniMap()
{
    if (m_debugPlayerParams.showMinimap < 0.5f) return;

    float posX = m_debugPlayerParams.minimapPosX;
    float posY = m_debugPlayerParams.minimapPosY;
    float size = m_debugPlayerParams.minimapSize;

    ImGui::SetNextWindowPos(ImVec2(posX, posY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(size, size), ImGuiCond_Always);

    ImGui::Begin("MiniMap#MiniMapWindow", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoSavedSettings);

    Vec2 canvasPos = { ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y };
    Vec2 canvasSize = { size, size };
    ImDrawList* draw = ImGui::GetWindowDrawList();

    draw->AddRectFilled(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(20, 24, 24, 180));
    draw->AddRect(ImVec2(canvasPos.x, canvasPos.y), ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), IM_COL32(100, 120, 110, 255), 0.0f, 0, 1.5f);

    Vec2 ret = WorldToCanvas(canvasPos, canvasSize, m_returnPoint, m_mapZoom, m_player.pos);
    draw->AddCircleFilled(ImVec2(ret.x, ret.y), 6.0f, IM_COL32(80, 180, 255, 255));

    for (const RopePoint& rope : m_ropePoints)
    {
        const Vec2 top = WorldToCanvas(canvasPos, canvasSize, rope.topPos, m_mapZoom, m_player.pos);
        const Vec2 bottom = WorldToCanvas(canvasPos, canvasSize, rope.bottomPos, m_mapZoom, m_player.pos);
        draw->AddLine(ImVec2(top.x, top.y), ImVec2(bottom.x, bottom.y), IM_COL32(170, 120, 70, 255), 3.0f);
    }

    for (const MiningPoint& point : m_miningPoints)
    {
        const bool visible = point.mined || point.discovered || point.sensed;
        if (!visible) continue;

        Vec2 p = WorldToCanvas(canvasPos, canvasSize, point.pos, m_mapZoom, m_player.pos);
        ImU32 color = point.mined ? IM_COL32(70, 70, 70, 255) : IM_COL32(185, 155, 90, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 4.0f, color);
    }

    for (const GroundRelic& relic : m_groundRelics)
    {
        if (!relic.active) continue;
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, relic.pos, m_mapZoom, m_player.pos);
        draw->AddRectFilled(ImVec2(p.x - 2.0f, p.y - 2.0f), ImVec2(p.x + 2.0f, p.y + 2.0f), IM_COL32(240, 220, 130, 255));
    }

    for (const Vec2& pin : m_pins)
    {
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, pin, m_mapZoom, m_player.pos);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 3.0f, IM_COL32(230, 80, 90, 255));
    }

    for (const EnemyState& enemy : m_enemies)
    {
        if (!enemy.alive) continue;
        if (enemy.type == EnemyType::Territory)
        {
            const Vec2 center = WorldToCanvas(canvasPos, canvasSize, enemy.territoryCenter, m_mapZoom, m_player.pos);
            const Vec2 edge = WorldToCanvas(canvasPos, canvasSize,
                Add(enemy.territoryCenter, { enemy.territoryRadius, 0.0f }), m_mapZoom, m_player.pos);
            draw->AddCircleFilled(ImVec2(center.x, center.y), std::fabs(edge.x - center.x), IM_COL32(20, 15, 28, 80));
        }
        Vec2 p = WorldToCanvas(canvasPos, canvasSize, enemy.pos, m_mapZoom, m_player.pos);
        ImU32 color = enemy.telegraphTimer > 0.0f ? IM_COL32(255, 200, 60, 255) : IM_COL32(210, 70, 70, 255);
        if (enemy.chargeTimer > 0.0f) color = IM_COL32(255, 80, 40, 255);
        draw->AddCircleFilled(ImVec2(p.x, p.y), 5.0f, color);
    }

    Vec2 player = WorldToCanvas(canvasPos, canvasSize, m_player.pos, m_mapZoom, m_player.pos);
    draw->AddCircleFilled(ImVec2(player.x, player.y), 5.0f, IM_COL32(90, 220, 150, 255));

    Vec2 faceEnd = WorldToCanvas(canvasPos, canvasSize, Add(m_player.pos, Mul(m_player.facing, 3.0f / m_mapZoom)), m_mapZoom, m_player.pos);
    draw->AddLine(ImVec2(player.x, player.y), ImVec2(faceEnd.x, faceEnd.y), IM_COL32(230, 250, 230, 255), 1.5f);

    ImGui::End();
}

#if defined(_DEBUG) || defined(NARAKU_EDITOR_BUILD)
void SceneNarakuProto::DrawPlayerPositionDebug()
{
    // デバッグウィンドウを表示します。
    ImGui::SetNextWindowPos(ImVec2(20.0f, 400.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(360.0f, 220.0f), ImGuiCond_FirstUseEver);

    if (ImGui::Begin(u8"プレイヤー位置デバッグ", nullptr, ImGuiWindowFlags_NoNavInputs))
    {
        // プレイヤーの現在位置と高さを表示
        ImGui::Text(u8"現在位置 X: %.3f, Z: %.3f", m_player.pos.x, m_player.pos.y);
        ImGui::Text(u8"現在高さ Y: %.3f (FeetWorldY: %.3f)", GetPlayerAirborneOffset() + GetGroundWorldY(m_player.pos, m_player.depth), m_player.feetWorldY);
        ImGui::Text(u8"現在深度: %.2f", m_player.depth);
        ImGui::Separator();

        // 生成済みマップから小ステージのグリッド座標を計算
        int gridX = -1;
        int gridZ = -1;
        int stageGridSize = 0;
        if (!m_runtimeMap.pieceNames.empty() && !m_runtimeMap.terrainLayers.empty())
        {
            stageGridSize = static_cast<int>(std::sqrt(static_cast<float>(m_runtimeMap.pieceNames.size())));
            if (stageGridSize * stageGridSize == static_cast<int>(m_runtimeMap.pieceNames.size()))
            {
                const NarakuMap::TerrainLayer& firstLayer = m_runtimeMap.terrainLayers.front();
                const float stageWidth = static_cast<float>(firstLayer.gridWidth - 1) * firstLayer.cellSize;
                const float stageHeight = static_cast<float>(firstLayer.gridHeight - 1) * firstLayer.cellSize;
                const float mapHalfWidth = stageWidth * static_cast<float>(stageGridSize) * 0.5f;
                const float mapHalfHeight = stageHeight * static_cast<float>(stageGridSize) * 0.5f;
                if (stageWidth > 0.0f && m_player.pos.x >= -mapHalfWidth && m_player.pos.x <= mapHalfWidth)
                {
                    gridX = std::min(
                        stageGridSize - 1,
                        static_cast<int>((m_player.pos.x + mapHalfWidth) / stageWidth));
                }
                if (stageHeight > 0.0f && m_player.pos.y >= -mapHalfHeight && m_player.pos.y <= mapHalfHeight)
                {
                    gridZ = std::min(
                        stageGridSize - 1,
                        static_cast<int>((m_player.pos.y + mapHalfHeight) / stageHeight));
                }
            }
        }

        ImGui::Text(u8"グリッド座標: (%d, %d)", gridX, gridZ);

        std::string pieceName = u8"不明";
        if (m_runtimeMap.pieceNames.empty())
        {
            pieceName = u8"未生成(再生成してください)";
        }
        else if (gridX >= 0 && gridX < stageGridSize && gridZ >= 0 && gridZ < stageGridSize)
        {
            size_t index = static_cast<size_t>(gridZ * stageGridSize + gridX);
            if (index < m_runtimeMap.pieceNames.size())
            {
                pieceName = m_runtimeMap.pieceNames[index];
            }
        }
        ImGui::Text(u8"小ステージ名: %s", pieceName.c_str());
        ImGui::Separator();

        // デバッグの障害調査として、プレイヤーの現在位置のセル属性も表示
        int currentLayerIndex = FindLayerIndexByDepth(m_player.depth);
        if (currentLayerIndex >= 0 && currentLayerIndex < static_cast<int>(m_runtimeMap.terrainLayers.size()))
        {
            const NarakuMap::TerrainLayer& layer = m_runtimeMap.terrainLayers[currentLayerIndex];
            float halfWidth = (layer.gridWidth - 1) * layer.cellSize * 0.5f;
            float halfHeight = (layer.gridHeight - 1) * layer.cellSize * 0.5f;
            float relativeX = m_player.pos.x - (layer.center.x - halfWidth);
            float relativeZ = m_player.pos.y - (layer.center.z - halfHeight);
            int cellX = static_cast<int>(std::floor(relativeX / layer.cellSize));
            int cellZ = static_cast<int>(std::floor(relativeZ / layer.cellSize));

            if (cellX >= 0 && cellX < layer.gridWidth - 1 && cellZ >= 0 && cellZ < layer.gridHeight - 1)
            {
                std::uint32_t flags = NarakuMap::GetCellAttributeFlags(layer, cellX, cellZ);
                std::string attr = "";
                if (flags & NarakuMap::CellAttributeBlocked) attr += "Blocked ";
                if (flags & NarakuMap::CellAttributeCliffEdge) attr += "CliffEdge ";
                if (flags & NarakuMap::CellAttributeHazard) attr += "Hazard ";
                if (flags & NarakuMap::CellAttributeRemoved) attr += "Removed ";
                if (attr.empty()) attr = "None (Walkable)";
                ImGui::Text(u8"現在セル(%d, %d) 属性: %s", cellX, cellZ, attr.c_str());
            }
            else
            {
                ImGui::Text(u8"現在セル: レイヤー範囲外 (%d, %d)", cellX, cellZ);
            }
        }
        else
        {
            ImGui::Text(u8"現在レイヤー: 不明 (深度 %.2f)", m_player.depth);
        }
    }
    ImGui::End();
}

#endif

void SceneNarakuProto::DrawMiningProgressBar()
{
    if (m_miningIndex < 0) return;

    float barWidth = 260.0f;
    float barHeight = 45.0f;

    // OS画面ではなく、メインウィンドウの作業領域中央を配置基準にします。
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(barWidth, barHeight), ImGuiCond_Always);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                             ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBackground |
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("MiningProgressOverlay##Overlay", nullptr, flags))
    {
        std::string text = u8"採掘中...";
        float textWidth = ImGui::CalcTextSize(text.c_str()).x;
        ImGui::SetCursorPosX((barWidth - textWidth) * 0.5f);
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.2f, 1.0f), text.c_str());

        float progress = std::max(0.0f, std::min(1.0f, 1.0f - (m_miningTimer / m_miningDuration)));
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 18.0f), "");
    }
    ImGui::End();
}

void SceneNarakuProto::DrawFishingProgress() const
{
    if (m_fishingPhase == FishingPhase::None) return;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.38f }, ImGuiCond_Always, { 0.5f, 0.5f });
    ImGui::SetNextWindowSize({ 330.0f, 62.0f }, ImGuiCond_Always);
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;
    if (ImGui::Begin("FishingProgress##Overlay", nullptr, flags))
    {
        if (m_fishingPhase == FishingPhase::Waiting) ImGui::TextUnformatted(u8"魚の反応を待っています…（Escで中断）");
        else if (m_fishingPhase == FishingPhase::Bite)
        {
            ImGui::TextColored({ 1.0f, 0.8f, 0.15f, 1.0f }, u8"反応あり！ Fキー！");
            const float window = m_fishingPointIndex >= 0 && m_fishingPoints[static_cast<size_t>(m_fishingPointIndex)].lake ? 1.25f : 1.0f;
            ImGui::ProgressBar(std::max(0.0f, m_fishingTimer / window), { -1.0f, 16.0f });
        }
        else ImGui::TextUnformatted(u8"引き上げ中…");
    }
    ImGui::End();
}

void SceneNarakuProto::DrawCenterNotification()
{
    if (m_centerNotificationTimer <= 0.0f || m_centerNotification.empty()) return;

    constexpr float overlayWidth = 420.0f;
    constexpr float overlayHeight = 48.0f;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const ImVec2 center(
        viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
        viewport->WorkPos.y + viewport->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(overlayWidth, overlayHeight), ImGuiCond_Always);

    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs;

    if (ImGui::Begin("CenterNotification##Overlay", nullptr, flags))
    {
        const ImVec2 textSize = ImGui::CalcTextSize(m_centerNotification.c_str());
        ImGui::SetCursorPos(ImVec2(
            std::max(0.0f, (overlayWidth - textSize.x) * 0.5f),
            std::max(0.0f, (overlayHeight - textSize.y) * 0.5f)));
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.22f, 1.0f), "%s", m_centerNotification.c_str());
    }
    ImGui::End();
}

SceneNarakuProto::Vec2 SceneNarakuProto::GetMouseAimGroundPosition()
{
    const POINT mousePos = GetMousePosition();
    HWND hWnd = GetActiveWindow();
    if (!hWnd) hWnd = GetForegroundWindow();
    RECT clientRect{};
    if (hWnd) GetClientRect(hWnd, &clientRect);
    const float screenW = static_cast<float>(std::max(1L, clientRect.right - clientRect.left));
    const float screenH = static_cast<float>(std::max(1L, clientRect.bottom - clientRect.top));

    // スクリーン正規化座標（NDC: -1〜1、画面中央が 0）
    const float ndcX = (static_cast<float>(mousePos.x) / screenW - 0.5f) * 2.0f;
    const float ndcY = (static_cast<float>(mousePos.y) / screenH - 0.5f) * -2.0f;

    // カメラ基準の向き
    const Vec2 camForward = GetCameraForward();
    const Vec2 camRight = GetCameraRight();

    // 画面中心（プレイヤー位置）からマウスカーソル方向へのオフセット
    const float aimRange = kAttackRange * 2.5f;
    return Add(m_player.pos, Add(Mul(camRight, ndcX * aimRange), Mul(camForward, ndcY * aimRange)));
}

void SceneNarakuProto::UpdateAimDirectionFromMouse()
{
    const Vec2 targetGround = GetMouseAimGroundPosition();
    const Vec2 toTarget = Sub(targetGround, m_player.pos);
    if (Distance(targetGround, m_player.pos) > 0.05f)
    {
        m_player.facing = Normalize(toTarget);
    }
}
