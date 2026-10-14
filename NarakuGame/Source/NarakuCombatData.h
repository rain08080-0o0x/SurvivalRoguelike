#pragma once

#include <string>
#include <vector>

namespace NarakuCombat
{
    enum class UserKind { Player, Enemy };
    enum class PresentationKind { WeaponTransform, SpriteSequence, ModelAnimation };

    struct Vec3
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    struct TransformKey
    {
        int frame = 0;
        Vec3 position;
        Vec3 rotationDegrees;
        Vec3 scale = { 1.0f, 1.0f, 1.0f };
    };

    struct HitboxKey
    {
        int frame = 0;
        Vec3 center;
        Vec3 size = { 1.0f, 1.0f, 1.0f };
    };

    struct ModelDefinition
    {
        std::string sourcePath;
        std::string id;
        std::string filePath;
        float scale = 1.0f;
        bool zFlip = true;
    };

    struct AttackDefinition
    {
        std::string sourcePath;
        std::string id;
        std::string displayName;
        UserKind userKind = UserKind::Player;
        std::string userId;
        PresentationKind presentation = PresentationKind::WeaponTransform;
        std::string modelId;
        std::string animationPath;
        std::string animationClip;
        int spriteStartFrame = 0;
        int spriteFrameCount = 1;
        int totalFrames = 20;
        int framesPerSecond = 30;
        int telegraphStartFrame = 0;
        int attackStartFrame = 5;
        int attackEndFrame = 10;
        int parryStartFrame = -1;
        int parryEndFrame = -1;
        int movementStartFrame = -1;
        int movementEndFrame = -1;
        int turnStartFrame = -1;
        int turnEndFrame = -1;
        int comboInputStartFrame = -1;
        int comboInputEndFrame = -1;
        int dodgeCancelStartFrame = -1;
        int dodgeCancelEndFrame = -1;
        std::string nextAttackId;
        float damageMultiplier = 1.0f;
        float knockback = 0.0f;
        float staminaCost = 0.0f;
        float cooldownSeconds = 0.0f;
        bool piercesWalls = true;
        bool piercesEnemies = true;
        bool singleHitPerTarget = true;
        std::vector<TransformKey> transformKeys;
        std::vector<HitboxKey> hitboxKeys;
    };

    struct Catalog
    {
        std::vector<ModelDefinition> models;
        std::vector<AttackDefinition> attacks;
    };

    bool LoadCatalog(const std::string& directory, Catalog& outCatalog, std::string* error = nullptr);
    bool SaveModel(const std::string& path, const ModelDefinition& value, std::string* error = nullptr);
    bool SaveAttack(const std::string& path, const AttackDefinition& value, std::string* error = nullptr);
    bool LoadModel(const std::string& path, ModelDefinition& outValue, std::string* error = nullptr);
    bool LoadAttack(const std::string& path, AttackDefinition& outValue, std::string* error = nullptr);
    std::vector<std::string> Validate(const Catalog& catalog);
    const ModelDefinition* FindModel(const Catalog& catalog, const std::string& id);
    const AttackDefinition* FindAttack(const Catalog& catalog, const std::string& id);
    TransformKey EvaluateTransform(const AttackDefinition& attack, float frame);
    HitboxKey EvaluateHitbox(const AttackDefinition& attack, float frame);
}
