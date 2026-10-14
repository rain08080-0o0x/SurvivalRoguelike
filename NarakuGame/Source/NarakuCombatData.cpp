#include "NarakuCombatData.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <Windows.h>

#undef min
#undef max

namespace
{
    using namespace NarakuCombat;

    std::string Trim(const std::string& value)
    {
        const std::size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        const std::size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    bool ParseBool(const std::string& value) { return value == "1" || value == "true"; }

    bool ParseVec3(const std::string& value, Vec3& out)
    {
        std::istringstream stream(value);
        char commaA = 0, commaB = 0;
        return static_cast<bool>(stream >> out.x >> commaA >> out.y >> commaB >> out.z) &&
            commaA == ',' && commaB == ',';
    }

    std::string Vec3Text(const Vec3& value)
    {
        std::ostringstream stream;
        stream << value.x << ',' << value.y << ',' << value.z;
        return stream.str();
    }

    Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
    {
        return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
    }

    template<class T, class GetFrame>
    T EvaluateKeys(const std::vector<T>& keys, float frame, GetFrame getFrame)
    {
        if (keys.empty()) return {};
        if (frame <= static_cast<float>(getFrame(keys.front()))) return keys.front();
        if (frame >= static_cast<float>(getFrame(keys.back()))) return keys.back();
        for (std::size_t i = 1; i < keys.size(); ++i)
        {
            if (frame > static_cast<float>(getFrame(keys[i]))) continue;
            const T& a = keys[i - 1];
            const T& b = keys[i];
            const float span = static_cast<float>(getFrame(b) - getFrame(a));
            const float t = span <= 0.0f ? 0.0f : (frame - static_cast<float>(getFrame(a))) / span;
            T result = a;
            result.frame = static_cast<int>(frame);
            result.center = Lerp(a.center, b.center, t);
            result.size = Lerp(a.size, b.size, t);
            return result;
        }
        return keys.back();
    }
}

namespace NarakuCombat
{
    bool SaveModel(const std::string& path, const ModelDefinition& value, std::string* error)
    {
        std::ofstream stream(path, std::ios::trunc);
        if (!stream) { if (error) *error = "cannot open model definition: " + path; return false; }
        stream << "type=model\nversion=1\nid=" << value.id << "\npath=" << value.filePath
            << "\nscale=" << value.scale << "\nzFlip=" << (value.zFlip ? 1 : 0) << '\n';
        return stream.good();
    }

    bool SaveAttack(const std::string& path, const AttackDefinition& value, std::string* error)
    {
        std::ofstream stream(path, std::ios::trunc);
        if (!stream) { if (error) *error = "cannot open attack definition: " + path; return false; }
        stream << "type=attack\nversion=1\nid=" << value.id << "\nname=" << value.displayName
            << "\nuserKind=" << static_cast<int>(value.userKind) << "\nuserId=" << value.userId
            << "\npresentation=" << static_cast<int>(value.presentation) << "\nmodelId=" << value.modelId
            << "\nanimationPath=" << value.animationPath << "\nanimationClip=" << value.animationClip
            << "\nspriteStart=" << value.spriteStartFrame << "\nspriteCount=" << value.spriteFrameCount
            << "\ntotalFrames=" << value.totalFrames << "\nfps=" << value.framesPerSecond
            << "\ntelegraphStart=" << value.telegraphStartFrame << "\nattackStart=" << value.attackStartFrame
            << "\nattackEnd=" << value.attackEndFrame << "\nparryStart=" << value.parryStartFrame
            << "\nparryEnd=" << value.parryEndFrame << "\nmovementStart=" << value.movementStartFrame
            << "\nmovementEnd=" << value.movementEndFrame << "\nturnStart=" << value.turnStartFrame
            << "\nturnEnd=" << value.turnEndFrame << "\ncomboStart=" << value.comboInputStartFrame
            << "\ncomboEnd=" << value.comboInputEndFrame << "\ndodgeStart=" << value.dodgeCancelStartFrame
            << "\ndodgeEnd=" << value.dodgeCancelEndFrame << "\nnextAttack=" << value.nextAttackId
            << "\ndamage=" << value.damageMultiplier << "\nknockback=" << value.knockback
            << "\nstamina=" << value.staminaCost << "\ncooldown=" << value.cooldownSeconds
            << "\npiercesWalls=" << (value.piercesWalls ? 1 : 0)
            << "\npiercesEnemies=" << (value.piercesEnemies ? 1 : 0)
            << "\nsingleHit=" << (value.singleHitPerTarget ? 1 : 0) << '\n';
        for (const TransformKey& key : value.transformKeys)
            stream << "transform=" << key.frame << '|' << Vec3Text(key.position) << '|'
                << Vec3Text(key.rotationDegrees) << '|' << Vec3Text(key.scale) << '\n';
        for (const HitboxKey& key : value.hitboxKeys)
            stream << "hitbox=" << key.frame << '|' << Vec3Text(key.center) << '|' << Vec3Text(key.size) << '\n';
        return stream.good();
    }

    bool LoadModel(const std::string& path, ModelDefinition& outValue, std::string* error)
    {
        std::ifstream stream(path);
        if (!stream) { if (error) *error = "cannot open model definition: " + path; return false; }
        ModelDefinition value;
        std::string line;
        try
        {
            while (std::getline(stream, line))
            {
                const std::size_t split = line.find('=');
                if (split == std::string::npos) continue;
                const std::string key = Trim(line.substr(0, split));
                const std::string data = Trim(line.substr(split + 1));
                if (key == "id") value.id = data;
                else if (key == "path") value.filePath = data;
                else if (key == "scale") value.scale = std::stof(data);
                else if (key == "zFlip") value.zFlip = ParseBool(data);
            }
        }
        catch (...) { if (error) *error = "invalid model definition: " + path; return false; }
        if (value.id.empty() || value.filePath.empty()) { if (error) *error = "model id/path is empty: " + path; return false; }
        value.sourcePath = path;
        outValue = value;
        return true;
    }

    bool LoadAttack(const std::string& path, AttackDefinition& outValue, std::string* error)
    {
        std::ifstream stream(path);
        if (!stream) { if (error) *error = "cannot open attack definition: " + path; return false; }
        AttackDefinition value;
        std::string line;
        try
        {
            while (std::getline(stream, line))
            {
                const std::size_t split = line.find('=');
                if (split == std::string::npos) continue;
                const std::string key = Trim(line.substr(0, split));
                const std::string data = Trim(line.substr(split + 1));
                if (key == "id") value.id = data;
                else if (key == "name") value.displayName = data;
                else if (key == "userKind") value.userKind = static_cast<UserKind>(std::stoi(data));
                else if (key == "userId") value.userId = data;
                else if (key == "presentation") value.presentation = static_cast<PresentationKind>(std::stoi(data));
                else if (key == "modelId") value.modelId = data;
                else if (key == "animationPath") value.animationPath = data;
                else if (key == "animationClip") value.animationClip = data;
                else if (key == "spriteStart") value.spriteStartFrame = std::stoi(data);
                else if (key == "spriteCount") value.spriteFrameCount = std::stoi(data);
                else if (key == "totalFrames") value.totalFrames = std::stoi(data);
                else if (key == "fps") value.framesPerSecond = std::stoi(data);
                else if (key == "telegraphStart") value.telegraphStartFrame = std::stoi(data);
                else if (key == "attackStart") value.attackStartFrame = std::stoi(data);
                else if (key == "attackEnd") value.attackEndFrame = std::stoi(data);
                else if (key == "parryStart") value.parryStartFrame = std::stoi(data);
                else if (key == "parryEnd") value.parryEndFrame = std::stoi(data);
                else if (key == "movementStart") value.movementStartFrame = std::stoi(data);
                else if (key == "movementEnd") value.movementEndFrame = std::stoi(data);
                else if (key == "turnStart") value.turnStartFrame = std::stoi(data);
                else if (key == "turnEnd") value.turnEndFrame = std::stoi(data);
                else if (key == "comboStart") value.comboInputStartFrame = std::stoi(data);
                else if (key == "comboEnd") value.comboInputEndFrame = std::stoi(data);
                else if (key == "dodgeStart") value.dodgeCancelStartFrame = std::stoi(data);
                else if (key == "dodgeEnd") value.dodgeCancelEndFrame = std::stoi(data);
                else if (key == "nextAttack") value.nextAttackId = data;
                else if (key == "damage") value.damageMultiplier = std::stof(data);
                else if (key == "knockback") value.knockback = std::stof(data);
                else if (key == "stamina") value.staminaCost = std::stof(data);
                else if (key == "cooldown") value.cooldownSeconds = std::stof(data);
                else if (key == "piercesWalls") value.piercesWalls = ParseBool(data);
                else if (key == "piercesEnemies") value.piercesEnemies = ParseBool(data);
                else if (key == "singleHit") value.singleHitPerTarget = ParseBool(data);
                else if (key == "transform")
                {
                    std::istringstream item(data); std::string part; TransformKey frame;
                    std::getline(item, part, '|'); frame.frame = std::stoi(part);
                    std::getline(item, part, '|'); if (!ParseVec3(part, frame.position)) throw 1;
                    std::getline(item, part, '|'); if (!ParseVec3(part, frame.rotationDegrees)) throw 1;
                    std::getline(item, part, '|'); if (!ParseVec3(part, frame.scale)) throw 1;
                    value.transformKeys.push_back(frame);
                }
                else if (key == "hitbox")
                {
                    std::istringstream item(data); std::string part; HitboxKey frame;
                    std::getline(item, part, '|'); frame.frame = std::stoi(part);
                    std::getline(item, part, '|'); if (!ParseVec3(part, frame.center)) throw 1;
                    std::getline(item, part, '|'); if (!ParseVec3(part, frame.size)) throw 1;
                    value.hitboxKeys.push_back(frame);
                }
            }
        }
        catch (...) { if (error) *error = "invalid attack definition: " + path; return false; }
        std::sort(value.transformKeys.begin(), value.transformKeys.end(), [](const auto& a, const auto& b) { return a.frame < b.frame; });
        std::sort(value.hitboxKeys.begin(), value.hitboxKeys.end(), [](const auto& a, const auto& b) { return a.frame < b.frame; });
        if (value.id.empty()) { if (error) *error = "attack id is empty: " + path; return false; }
        value.sourcePath = path;
        outValue = value;
        return true;
    }

    bool LoadCatalog(const std::string& directory, Catalog& outCatalog, std::string* error)
    {
        Catalog result;
        const std::string pattern = directory + "\\*";
        WIN32_FIND_DATAA findData = {};
        HANDLE find = FindFirstFileA(pattern.c_str(), &findData);
        if (find == INVALID_HANDLE_VALUE)
        {
            if (error) *error = "combat data directory is missing: " + directory;
            return false;
        }
        std::vector<std::string> loadIssues;
        do
        {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) continue;
            const std::string name = findData.cFileName;
            const std::size_t dot = name.find_last_of('.');
            const std::string extension = dot == std::string::npos ? std::string() : name.substr(dot);
            const std::string path = directory + "\\" + name;
            std::string localError;
            if (extension == ".nmodel")
            {
                ModelDefinition value;
                if (!LoadModel(path, value, &localError)) loadIssues.push_back(localError);
                else result.models.push_back(value);
            }
            else if (extension == ".nattack")
            {
                AttackDefinition value;
                if (!LoadAttack(path, value, &localError)) loadIssues.push_back(localError);
                else result.attacks.push_back(value);
            }
        } while (FindNextFileA(find, &findData));
        FindClose(find);
        const std::vector<std::string> validationIssues = Validate(result);
        loadIssues.insert(loadIssues.end(), validationIssues.begin(), validationIssues.end());
        if (error)
        {
            error->clear();
            for (const std::string& issue : loadIssues)
            {
                if (!error->empty()) *error += "\n";
                *error += issue;
            }
        }
        outCatalog = std::move(result);
        return true;
    }

    std::vector<std::string> Validate(const Catalog& catalog)
    {
        std::vector<std::string> issues;
        std::unordered_set<std::string> modelIds, attackIds;
        for (const ModelDefinition& model : catalog.models)
        {
            if (model.id.empty() || model.filePath.empty()) issues.push_back("model id/path is empty");
            else if (!modelIds.insert(model.id).second) issues.push_back("duplicate model id: " + model.id);
        }
        for (const AttackDefinition& attack : catalog.attacks)
        {
            if (attack.id.empty()) { issues.push_back("attack id is empty"); continue; }
            if (!attackIds.insert(attack.id).second) issues.push_back("duplicate attack id: " + attack.id);
            if (attack.totalFrames <= 0 || attack.framesPerSecond <= 0) issues.push_back("invalid frames: " + attack.id);
            if (attack.attackStartFrame < 0 || attack.attackEndFrame < attack.attackStartFrame || attack.attackEndFrame >= attack.totalFrames)
                issues.push_back("invalid attack interval: " + attack.id);
            if (!attack.modelId.empty() && modelIds.find(attack.modelId) == modelIds.end()) issues.push_back("unknown model id: " + attack.modelId);
            if (attack.hitboxKeys.empty()) issues.push_back("hitbox is missing: " + attack.id);
        }
        for (const AttackDefinition& attack : catalog.attacks)
            if (!attack.nextAttackId.empty() && attackIds.find(attack.nextAttackId) == attackIds.end())
                issues.push_back("unknown next attack id: " + attack.nextAttackId);
        return issues;
    }

    const ModelDefinition* FindModel(const Catalog& catalog, const std::string& id)
    {
        const auto found = std::find_if(catalog.models.begin(), catalog.models.end(), [&](const auto& value) { return value.id == id; });
        return found == catalog.models.end() ? nullptr : &*found;
    }

    const AttackDefinition* FindAttack(const Catalog& catalog, const std::string& id)
    {
        const auto found = std::find_if(catalog.attacks.begin(), catalog.attacks.end(), [&](const auto& value) { return value.id == id; });
        return found == catalog.attacks.end() ? nullptr : &*found;
    }

    TransformKey EvaluateTransform(const AttackDefinition& attack, float frame)
    {
        if (attack.transformKeys.empty()) return {};
        if (frame <= attack.transformKeys.front().frame) return attack.transformKeys.front();
        if (frame >= attack.transformKeys.back().frame) return attack.transformKeys.back();
        for (std::size_t i = 1; i < attack.transformKeys.size(); ++i)
        {
            const TransformKey& b = attack.transformKeys[i];
            if (frame > b.frame) continue;
            const TransformKey& a = attack.transformKeys[i - 1];
            const float t = (frame - a.frame) / static_cast<float>(std::max(1, b.frame - a.frame));
            TransformKey result; result.frame = static_cast<int>(frame);
            result.position = Lerp(a.position, b.position, t);
            result.rotationDegrees = Lerp(a.rotationDegrees, b.rotationDegrees, t);
            result.scale = Lerp(a.scale, b.scale, t);
            return result;
        }
        return attack.transformKeys.back();
    }

    HitboxKey EvaluateHitbox(const AttackDefinition& attack, float frame)
    {
        return EvaluateKeys(attack.hitboxKeys, frame, [](const HitboxKey& key) { return key.frame; });
    }
}
