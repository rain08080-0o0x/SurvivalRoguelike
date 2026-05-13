#include "BossAttackScript.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	const char* kDefaultBossAttackScriptPath = "Assets/boss_attack_profiles.json";
	const char* kMirrorBossAttackScriptPath = "DX22_Project/Assets/boss_attack_profiles.json";
	const char* kDebugBossAttackScriptPath = "x64/Debug/Assets/boss_attack_profiles.json";
	const char* kUpstreamMirrorBossAttackScriptPath = "../../DX22_Project/Assets/boss_attack_profiles.json";
	const char* kFinalBossAttackScriptPath = "Assets/final_boss_attack_profile.json";
	const char* kFinalMirrorBossAttackScriptPath = "DX22_Project/Assets/final_boss_attack_profile.json";
	const char* kFinalDebugBossAttackScriptPath = "x64/Debug/Assets/final_boss_attack_profile.json";
	const char* kFinalUpstreamMirrorBossAttackScriptPath = "../../DX22_Project/Assets/final_boss_attack_profile.json";
	const int kCurrentVersion = 6;

	bool IsFinalBossPath(const char* path)
	{
		if (!path) return false;
		return std::strcmp(path, kFinalBossAttackScriptPath) == 0 ||
			std::strcmp(path, kFinalMirrorBossAttackScriptPath) == 0 ||
			std::strcmp(path, kFinalDebugBossAttackScriptPath) == 0 ||
			std::strcmp(path, kFinalUpstreamMirrorBossAttackScriptPath) == 0;
	}

	BossAttackScript::Database MakeDefaultDatabaseForPath(const char* path)
	{
		if (!IsFinalBossPath(path))
		{
			return BossAttackScript::MakeDefaultDatabase();
		}

		BossAttackScript::Database database;
		database.profiles.push_back(BossAttackScript::MakeDefaultProfile(BossAttackScript::ProfileFinalBarrage));
		return database;
	}

	bool NeedsFinalBossProfileMigration(const BossAttackScript::Profile& profile)
	{
		static const char* kRequiredNames[] =
		{
			u8"\u5948\u843d\u843d\u4e0b",
			u8"\u7d2b\u76f4\u7dda\u65ac\u308a",
			u8"\u7d2b\u5341\u5b57\u65ac\u308a",
			u8"\u6ed1\u8d70\u5203A",
			u8"\u6ed1\u8d70\u5203B",
			u8"\u5186\u74b0\u53ce\u675f",
			u8"\u74b0\u72b6\u7206\u7834",
			u8"\u653e\u5c04\u6383\u5c04",
			u8"\u8d64\u6247\u6383\u5c04",
			u8"\u5927\u74b0",
			u8"\u7d42\u7109\u67d3\u3081"
		};
		if (profile.attacks.size() != (sizeof(kRequiredNames) / sizeof(kRequiredNames[0])))
		{
			return true;
		}
		for (const char* name : kRequiredNames)
		{
			bool found = false;
			for (const auto& attack : profile.attacks)
			{
				if (attack.name == name)
				{
					found = true;
					break;
				}
			}
			if (!found)
			{
				return true;
			}
		}
		return false;
	}

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	int ClampIntValue(int value, int minValue, int maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}

	struct JsonValue
	{
		enum Type
		{
			TypeNull = 0,
			TypeBool,
			TypeNumber,
			TypeString,
			TypeArray,
			TypeObject
		};

		Type type = TypeNull;
		bool boolValue = false;
		double numberValue = 0.0;
		std::string stringValue;
		std::vector<JsonValue> arrayValue;
		std::map<std::string, JsonValue> objectValue;
	};

	int HexDigitValue(char c)
	{
		if (c >= '0' && c <= '9') return c - '0';
		if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
		if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
		return -1;
	}

	void AppendUtf8CodePoint(std::string& outText, unsigned int codePoint)
	{
		if (codePoint <= 0x7F)
		{
			outText.push_back(static_cast<char>(codePoint));
		}
		else if (codePoint <= 0x7FF)
		{
			outText.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
			outText.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		}
		else if (codePoint <= 0xFFFF)
		{
			outText.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
			outText.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
			outText.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		}
		else
		{
			outText.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
			outText.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
			outText.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
			outText.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
		}
	}

	class JsonParser
	{
	public:
		explicit JsonParser(const std::string& text)
			: m_text(text)
		{
		}

		bool Parse(JsonValue& outValue)
		{
			SkipWs();
			if (!ParseValue(outValue)) return false;
			SkipWs();
			return m_pos == m_text.size();
		}

	private:
		void SkipWs()
		{
			while (m_pos < m_text.size() && std::isspace(static_cast<unsigned char>(m_text[m_pos])))
			{
				++m_pos;
			}
		}

		bool Match(const char* literal)
		{
			const size_t len = std::char_traits<char>::length(literal);
			if (m_text.compare(m_pos, len, literal) != 0) return false;
			m_pos += len;
			return true;
		}

		bool ParseValue(JsonValue& outValue)
		{
			SkipWs();
			if (m_pos >= m_text.size()) return false;
			const char c = m_text[m_pos];
			if (c == '{') return ParseObject(outValue);
			if (c == '[') return ParseArray(outValue);
			if (c == '"')
			{
				outValue.type = JsonValue::TypeString;
				return ParseString(outValue.stringValue);
			}
			if (c == '-' || (c >= '0' && c <= '9'))
			{
				outValue.type = JsonValue::TypeNumber;
				return ParseNumber(outValue.numberValue);
			}
			if (Match("true"))
			{
				outValue.type = JsonValue::TypeBool;
				outValue.boolValue = true;
				return true;
			}
			if (Match("false"))
			{
				outValue.type = JsonValue::TypeBool;
				outValue.boolValue = false;
				return true;
			}
			if (Match("null"))
			{
				outValue.type = JsonValue::TypeNull;
				return true;
			}
			return false;
		}

		bool ParseObject(JsonValue& outValue)
		{
			if (m_text[m_pos] != '{') return false;
			++m_pos;
			outValue.type = JsonValue::TypeObject;
			outValue.objectValue.clear();
			SkipWs();
			if (m_pos < m_text.size() && m_text[m_pos] == '}')
			{
				++m_pos;
				return true;
			}

			while (m_pos < m_text.size())
			{
				SkipWs();
				std::string key;
				if (!ParseString(key)) return false;
				SkipWs();
				if (m_pos >= m_text.size() || m_text[m_pos] != ':') return false;
				++m_pos;
				JsonValue value;
				if (!ParseValue(value)) return false;
				outValue.objectValue[key] = value;
				SkipWs();
				if (m_pos >= m_text.size()) return false;
				if (m_text[m_pos] == '}')
				{
					++m_pos;
					return true;
				}
				if (m_text[m_pos] != ',') return false;
				++m_pos;
			}
			return false;
		}

		bool ParseArray(JsonValue& outValue)
		{
			if (m_text[m_pos] != '[') return false;
			++m_pos;
			outValue.type = JsonValue::TypeArray;
			outValue.arrayValue.clear();
			SkipWs();
			if (m_pos < m_text.size() && m_text[m_pos] == ']')
			{
				++m_pos;
				return true;
			}

			while (m_pos < m_text.size())
			{
				JsonValue element;
				if (!ParseValue(element)) return false;
				outValue.arrayValue.push_back(element);
				SkipWs();
				if (m_pos >= m_text.size()) return false;
				if (m_text[m_pos] == ']')
				{
					++m_pos;
					return true;
				}
				if (m_text[m_pos] != ',') return false;
				++m_pos;
			}
			return false;
		}

		bool ParseString(std::string& outText)
		{
			if (m_pos >= m_text.size() || m_text[m_pos] != '"') return false;
			++m_pos;
			outText.clear();
			while (m_pos < m_text.size())
			{
				const char c = m_text[m_pos++];
				if (c == '"') return true;
				if (c == '\\')
				{
					if (m_pos >= m_text.size()) return false;
					const char escaped = m_text[m_pos++];
					switch (escaped)
					{
					case '"': outText.push_back('"'); break;
					case '\\': outText.push_back('\\'); break;
					case '/': outText.push_back('/'); break;
					case 'b': outText.push_back('\b'); break;
					case 'f': outText.push_back('\f'); break;
					case 'n': outText.push_back('\n'); break;
					case 'r': outText.push_back('\r'); break;
					case 't': outText.push_back('\t'); break;
					case 'u':
					{
						if ((m_pos + 4) > m_text.size()) return false;
						unsigned int codePoint = 0;
						for (int i = 0; i < 4; ++i)
						{
							const int digit = HexDigitValue(m_text[m_pos + i]);
							if (digit < 0) return false;
							codePoint = (codePoint << 4) | static_cast<unsigned int>(digit);
						}
						m_pos += 4;
						AppendUtf8CodePoint(outText, codePoint);
						break;
					}
					default: outText.push_back(escaped); break;
					}
				}
				else
				{
					outText.push_back(c);
				}
			}
			return false;
		}

		bool ParseNumber(double& outNumber)
		{
			const size_t start = m_pos;
			if (m_text[m_pos] == '-') ++m_pos;
			while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
			if (m_pos < m_text.size() && m_text[m_pos] == '.')
			{
				++m_pos;
				while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
			}
			if (m_pos < m_text.size() && (m_text[m_pos] == 'e' || m_text[m_pos] == 'E'))
			{
				++m_pos;
				if (m_pos < m_text.size() && (m_text[m_pos] == '+' || m_text[m_pos] == '-')) ++m_pos;
				while (m_pos < m_text.size() && std::isdigit(static_cast<unsigned char>(m_text[m_pos]))) ++m_pos;
			}
			if (start == m_pos) return false;
			try
			{
				outNumber = std::stod(m_text.substr(start, m_pos - start));
			}
			catch (...)
			{
				return false;
			}
			return true;
		}

	private:
		const std::string& m_text;
		size_t m_pos = 0;
	};

	const JsonValue* GetObjectField(const JsonValue& objectValue, const char* key)
	{
		if (objectValue.type != JsonValue::TypeObject) return nullptr;
		const auto it = objectValue.objectValue.find(key);
		if (it == objectValue.objectValue.end()) return nullptr;
		return &it->second;
	}

	bool ReadBool(const JsonValue& objectValue, const char* key, bool defaultValue)
	{
		const JsonValue* field = GetObjectField(objectValue, key);
		if (!field) return defaultValue;
		if (field->type == JsonValue::TypeBool) return field->boolValue;
		if (field->type == JsonValue::TypeNumber) return field->numberValue != 0.0;
		return defaultValue;
	}

	int ReadInt(const JsonValue& objectValue, const char* key, int defaultValue)
	{
		const JsonValue* field = GetObjectField(objectValue, key);
		if (!field) return defaultValue;
		if (field->type == JsonValue::TypeNumber) return static_cast<int>(field->numberValue);
		if (field->type == JsonValue::TypeBool) return field->boolValue ? 1 : 0;
		return defaultValue;
	}

	float ReadFloat(const JsonValue& objectValue, const char* key, float defaultValue)
	{
		const JsonValue* field = GetObjectField(objectValue, key);
		if (!field) return defaultValue;
		if (field->type == JsonValue::TypeNumber) return static_cast<float>(field->numberValue);
		return defaultValue;
	}

	std::string ReadString(const JsonValue& objectValue, const char* key, const std::string& defaultValue)
	{
		const JsonValue* field = GetObjectField(objectValue, key);
		if (!field) return defaultValue;
		if (field->type == JsonValue::TypeString) return field->stringValue;
		return defaultValue;
	}

	DirectX::XMFLOAT2 ReadFloat2(const JsonValue& objectValue, const char* key, const DirectX::XMFLOAT2& defaultValue)
	{
		const JsonValue* field = GetObjectField(objectValue, key);
		if (!field || field->type != JsonValue::TypeArray || field->arrayValue.size() < 2) return defaultValue;
		return {
			(field->arrayValue[0].type == JsonValue::TypeNumber) ? static_cast<float>(field->arrayValue[0].numberValue) : defaultValue.x,
			(field->arrayValue[1].type == JsonValue::TypeNumber) ? static_cast<float>(field->arrayValue[1].numberValue) : defaultValue.y
		};
	}

	DirectX::XMFLOAT3 ReadFloat3(const JsonValue& objectValue, const char* key, const DirectX::XMFLOAT3& defaultValue)
	{
		const JsonValue* field = GetObjectField(objectValue, key);
		if (!field || field->type != JsonValue::TypeArray || field->arrayValue.size() < 3) return defaultValue;
		return {
			(field->arrayValue[0].type == JsonValue::TypeNumber) ? static_cast<float>(field->arrayValue[0].numberValue) : defaultValue.x,
			(field->arrayValue[1].type == JsonValue::TypeNumber) ? static_cast<float>(field->arrayValue[1].numberValue) : defaultValue.y,
			(field->arrayValue[2].type == JsonValue::TypeNumber) ? static_cast<float>(field->arrayValue[2].numberValue) : defaultValue.z
		};
	}

	std::string EscapeJson(const std::string& text)
	{
		std::string result;
		for (char c : text)
		{
			switch (c)
			{
			case '\\': result += "\\\\"; break;
			case '"': result += "\\\""; break;
			case '\n': result += "\\n"; break;
			case '\r': result += "\\r"; break;
			case '\t': result += "\\t"; break;
			default: result.push_back(c); break;
			}
		}
		return result;
	}

	void AppendIndent(std::ostringstream& oss, int indent)
	{
		for (int i = 0; i < indent; ++i) oss << ' ';
	}

	void AppendBool(std::ostringstream& oss, bool value)
	{
		oss << (value ? "true" : "false");
	}

	void AppendString(std::ostringstream& oss, const std::string& value)
	{
		oss << '"' << EscapeJson(value) << '"';
	}

	void AppendFloat2(std::ostringstream& oss, const DirectX::XMFLOAT2& value)
	{
		oss << "[" << value.x << ", " << value.y << "]";
	}

	void AppendFloat3(std::ostringstream& oss, const DirectX::XMFLOAT3& value)
	{
		oss << "[" << value.x << ", " << value.y << ", " << value.z << "]";
	}

	bool WriteTextFile(const char* path, const std::string& text)
	{
		std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
		if (!ofs.is_open()) return false;
		ofs.write(text.data(), static_cast<std::streamsize>(text.size()));
		return ofs.good();
	}

	void ClampHitbox(BossAttackScript::Hitbox& hitbox)
	{
		hitbox.id = ClampIntValue(hitbox.id, 0, 9999);
		hitbox.shape = BossAttackScript::NormalizeColliderShape(hitbox.shape);
		hitbox.startMode = BossAttackScript::NormalizePositionMode(hitbox.startMode);
		hitbox.endMode = BossAttackScript::NormalizePositionMode(hitbox.endMode);
		hitbox.startRandomRadius = ClampFloat(hitbox.startRandomRadius, 0.0f, 50.0f);
		hitbox.endRandomRadius = ClampFloat(hitbox.endRandomRadius, 0.0f, 50.0f);
		hitbox.maxDistance = ClampFloat(hitbox.maxDistance, 0.0f, 50.0f);
		hitbox.lateralOffset = ClampFloat(hitbox.lateralOffset, -50.0f, 50.0f);
		hitbox.startSize.x = ClampFloat(hitbox.startSize.x, 0.05f, 30.0f);
		hitbox.startSize.y = ClampFloat(hitbox.startSize.y, 0.05f, 30.0f);
		hitbox.startSize.z = ClampFloat(hitbox.startSize.z, 0.05f, 30.0f);
		hitbox.endSize.x = ClampFloat(hitbox.endSize.x, 0.05f, 30.0f);
		hitbox.endSize.y = ClampFloat(hitbox.endSize.y, 0.05f, 30.0f);
		hitbox.endSize.z = ClampFloat(hitbox.endSize.z, 0.05f, 30.0f);
		if (hitbox.shape == BossAttackScript::ColliderShapeCircle)
		{
			hitbox.startSize.z = hitbox.startSize.x;
			hitbox.endSize.z = hitbox.endSize.x;
		}
		if (!hitbox.useEndPosition)
		{
			hitbox.endPos = hitbox.startPos;
			hitbox.endSize = hitbox.startSize;
		}
		hitbox.yawDeg = 0.0f;
		if (hitbox.name.empty()) hitbox.name = "Hitbox";
	}

	void ClampVisual(BossAttackScript::Visual& visual)
	{
		visual.size.x = ClampFloat(visual.size.x, 0.05f, 30.0f);
		visual.size.y = ClampFloat(visual.size.y, 0.05f, 30.0f);
		visual.spawnHeight = ClampFloat(visual.spawnHeight, 0.0f, 30.0f);
		visual.travelSec = ClampFloat(visual.travelSec, 0.01f, 20.0f);
		visual.spinDegPerSec = ClampFloat(visual.spinDegPerSec, -1080.0f, 1080.0f);
		if (!visual.texturePath.empty() && visual.texturePath.find("Assets/") != 0)
		{
			visual.texturePath = "Assets/" + visual.texturePath;
		}
	}

	void ClampLink(BossAttackScript::NextAttackLink& link)
	{
		link.attackIndex = ClampIntValue(link.attackIndex, -1, 9999);
		link.weight = ClampIntValue(link.weight, 1, 1000);
	}

	void ClampAttack(BossAttackScript::Attack& attack)
	{
		attack.telegraphSec = ClampFloat(attack.telegraphSec, 0.01f, 20.0f);
		attack.activeSec = ClampFloat(attack.activeSec, 0.01f, 20.0f);
		attack.cooldownSec = ClampFloat(attack.cooldownSec, 0.0f, 20.0f);
		attack.damageScale = ClampFloat(attack.damageScale, 0.0f, 50.0f);
		attack.repeatCount = ClampIntValue(attack.repeatCount, 1, 128);
		attack.repeatIntervalSec = ClampFloat(attack.repeatIntervalSec, 0.0f, 20.0f);
		attack.deliveryMode = BossAttackScript::NormalizeAttackDeliveryMode(attack.deliveryMode);
		attack.movePreset = BossAttackScript::NormalizeMovePreset(attack.movePreset);
		attack.moveSpeed = ClampFloat(attack.moveSpeed, 0.0f, 50.0f);
		attack.moveDistance = ClampFloat(attack.moveDistance, 0.0f, 50.0f);
		attack.spawnMode = BossAttackScript::NormalizeSpawnMode(attack.spawnMode);
		attack.spawnCount = ClampIntValue(attack.spawnCount, 1, 128);
		attack.randomRadius = ClampFloat(attack.randomRadius, 0.0f, 50.0f);
		attack.followStrength = ClampFloat(attack.followStrength, 0.0f, 1.0f);
		for (auto& link : attack.nextLinks) ClampLink(link);
		ClampHitbox(attack.hitbox);
		ClampVisual(attack.visual);
		attack.colliderIds.clear();
		attack.colliderIds.push_back(attack.hitbox.id);
		if (attack.name.empty()) attack.name = "Attack";
	}

	void SyncProfileColliders(BossAttackScript::Profile& profile)
	{
		profile.colliders.clear();
		for (size_t i = 0; i < profile.attacks.size(); ++i)
		{
			profile.attacks[i].hitbox.id = static_cast<int>(i);
			profile.attacks[i].colliderIds.clear();
			profile.attacks[i].colliderIds.push_back(static_cast<int>(i));
			profile.colliders.push_back(profile.attacks[i].hitbox);
		}
	}

	void ClampProfile(BossAttackScript::Profile& profile)
	{
		profile.type = BossAttackScript::NormalizeProfileType(profile.type);
		profile.hpScale = ClampFloat(profile.hpScale, 0.10f, 8.0f);
		profile.guardScale = ClampFloat(profile.guardScale, 0.10f, 8.0f);
		profile.sizeScale = ClampFloat(profile.sizeScale, 0.20f, 3.0f);
		profile.cooldownScale = ClampFloat(profile.cooldownScale, 0.10f, 4.0f);
		profile.telegraphScale = ClampFloat(profile.telegraphScale, 0.10f, 4.0f);
		profile.damageScale = ClampFloat(profile.damageScale, 0.10f, 8.0f);
		for (auto& attack : profile.attacks) ClampAttack(attack);
		if (profile.displayName.empty()) profile.displayName = BossAttackScript::GetProfileName(profile.type);
		SyncProfileColliders(profile);
	}

	void ClampDatabase(BossAttackScript::Database& database)
	{
		for (auto& profile : database.profiles) ClampProfile(profile);
		for (int type = 0; type < BossAttackScript::ProfileTypeCount; ++type)
		{
			if (!BossAttackScript::FindProfile(database, type))
			{
				database.profiles.push_back(BossAttackScript::MakeDefaultProfile(type));
			}
		}
		std::sort(database.profiles.begin(), database.profiles.end(), [](const BossAttackScript::Profile& a, const BossAttackScript::Profile& b)
		{
			return a.type < b.type;
		});
	}

	std::string MakeDefaultTexturePath(int profileType)
	{
		return (profileType == BossAttackScript::ProfileFinalBarrage)
			? "Assets/Texture/Effect/metal_blade.png"
			: "Assets/Texture/Game/rock.png";
	}

	void LoadHitboxFromJson(BossAttackScript::Hitbox& hitbox, const JsonValue& value)
	{
		hitbox.enabled = ReadBool(value, "enabled", hitbox.enabled);
		hitbox.name = ReadString(value, "name", hitbox.name);
		hitbox.shape = ReadInt(value, "shape", hitbox.shape);
		hitbox.startMode = ReadInt(value, "startMode", hitbox.startMode);
		hitbox.startPos = ReadFloat3(value, "startPos", hitbox.startPos);
		hitbox.startRandomRadius = ReadFloat(value, "startRandomRadius", hitbox.startRandomRadius);
		hitbox.useEndPosition = ReadBool(value, "useEndPosition", hitbox.useEndPosition);
		hitbox.endMode = ReadInt(value, "endMode", hitbox.endMode);
		hitbox.endPos = ReadFloat3(value, "endPos", hitbox.endPos);
		hitbox.endRandomRadius = ReadFloat(value, "endRandomRadius", hitbox.endRandomRadius);
		hitbox.usePlayerDirection = ReadBool(value, "usePlayerDirection", hitbox.usePlayerDirection);
		hitbox.maxDistance = ReadFloat(value, "maxDistance", hitbox.maxDistance);
		hitbox.lateralOffset = ReadFloat(value, "lateralOffset", hitbox.lateralOffset);
		hitbox.startSize = ReadFloat3(value, "startSize", hitbox.startSize);
		hitbox.endSize = ReadFloat3(value, "endSize", hitbox.endSize);
		ClampHitbox(hitbox);
	}

	void LoadVisualFromJson(BossAttackScript::Visual& visual, const JsonValue& value)
	{
		visual.enabled = ReadBool(value, "enabled", visual.enabled);
		visual.billboard = ReadBool(value, "billboard", visual.billboard);
		visual.texturePath = ReadString(value, "texturePath", visual.texturePath);
		visual.size = ReadFloat2(value, "size", visual.size);
		visual.spawnHeight = ReadFloat(value, "spawnHeight", visual.spawnHeight);
		visual.travelSec = ReadFloat(value, "travelSec", visual.travelSec);
		visual.spinDegPerSec = ReadFloat(value, "spinDegPerSec", visual.spinDegPerSec);
		ClampVisual(visual);
	}

	void LoadNextLinksFromJson(std::vector<BossAttackScript::NextAttackLink>& outLinks, const JsonValue& value)
	{
		outLinks.clear();
		if (value.type != JsonValue::TypeArray) return;
		for (const JsonValue& item : value.arrayValue)
		{
			if (item.type != JsonValue::TypeObject) continue;
			BossAttackScript::NextAttackLink link;
			link.enabled = ReadBool(item, "enabled", link.enabled);
			link.attackIndex = ReadInt(item, "attackIndex", link.attackIndex);
			link.weight = ReadInt(item, "weight", link.weight);
			ClampLink(link);
			outLinks.push_back(link);
		}
	}

	void LoadAttackFromJson(BossAttackScript::Attack& attack, const JsonValue& value)
	{
		attack.enabled = ReadBool(value, "enabled", attack.enabled);
		attack.name = ReadString(value, "name", attack.name);
		attack.telegraphSec = ReadFloat(value, "telegraphSec", attack.telegraphSec);
		attack.activeSec = ReadFloat(value, "activeSec", attack.activeSec);
		attack.cooldownSec = ReadFloat(value, "cooldownSec", attack.cooldownSec);
		attack.damageScale = ReadFloat(value, "damageScale", attack.damageScale);
		attack.repeatCount = ReadInt(value, "repeatCount", attack.repeatCount);
		attack.repeatIntervalSec = ReadFloat(value, "repeatIntervalSec", attack.repeatIntervalSec);
		attack.deliveryMode = ReadInt(value, "deliveryMode", attack.deliveryMode);
		attack.movePreset = ReadInt(value, "movePreset", attack.movePreset);
		attack.moveSpeed = ReadFloat(value, "moveSpeed", attack.moveSpeed);
		attack.moveDistance = ReadFloat(value, "moveDistance", attack.moveDistance);
		attack.spawnMode = ReadInt(value, "spawnMode", attack.spawnMode);
		attack.spawnCount = ReadInt(value, "spawnCount", attack.spawnCount);
		attack.randomRadius = ReadFloat(value, "randomRadius", attack.randomRadius);
		attack.followStrength = ReadFloat(value, "followStrength", attack.followStrength);

		if (const JsonValue* hitboxValue = GetObjectField(value, "hitbox"))
		{
			LoadHitboxFromJson(attack.hitbox, *hitboxValue);
		}
		if (const JsonValue* visualValue = GetObjectField(value, "visual"))
		{
			LoadVisualFromJson(attack.visual, *visualValue);
		}
		if (const JsonValue* nextLinks = GetObjectField(value, "nextLinks"))
		{
			LoadNextLinksFromJson(attack.nextLinks, *nextLinks);
		}
		ClampAttack(attack);
	}

	void LoadProfileFromJson(BossAttackScript::Profile& profile, const JsonValue& value)
	{
		profile.type = ReadInt(value, "type", profile.type);
		profile.displayName = ReadString(value, "displayName", profile.displayName);
		profile.hpScale = ReadFloat(value, "hpScale", profile.hpScale);
		profile.guardScale = ReadFloat(value, "guardScale", profile.guardScale);
		profile.sizeScale = ReadFloat(value, "sizeScale", profile.sizeScale);
		profile.cooldownScale = ReadFloat(value, "cooldownScale", profile.cooldownScale);
		profile.telegraphScale = ReadFloat(value, "telegraphScale", profile.telegraphScale);
		profile.damageScale = ReadFloat(value, "damageScale", profile.damageScale);
		profile.startsSpecial = ReadBool(value, "startsSpecial", profile.startsSpecial);
		profile.loops = ReadBool(value, "loops", profile.loops);
		profile.attacks.clear();
		if (const JsonValue* attacksValue = GetObjectField(value, "attacks"))
		{
			if (attacksValue->type == JsonValue::TypeArray)
			{
				for (const JsonValue& item : attacksValue->arrayValue)
				{
					if (item.type != JsonValue::TypeObject) continue;
					BossAttackScript::Attack attack;
					LoadAttackFromJson(attack, item);
					profile.attacks.push_back(attack);
				}
			}
		}
		ClampProfile(profile);
	}

	void AppendHitboxJson(std::ostringstream& oss, const BossAttackScript::Hitbox& hitbox, int indent)
	{
		AppendIndent(oss, indent); oss << "\"hitbox\": {\n";
		AppendIndent(oss, indent + 2); oss << "\"enabled\": "; AppendBool(oss, hitbox.enabled); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"name\": "; AppendString(oss, hitbox.name); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"shape\": " << hitbox.shape << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"startMode\": " << hitbox.startMode << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"startPos\": "; AppendFloat3(oss, hitbox.startPos); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"startRandomRadius\": " << hitbox.startRandomRadius << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"useEndPosition\": "; AppendBool(oss, hitbox.useEndPosition); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"endMode\": " << hitbox.endMode << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"endPos\": "; AppendFloat3(oss, hitbox.endPos); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"endRandomRadius\": " << hitbox.endRandomRadius << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"usePlayerDirection\": "; AppendBool(oss, hitbox.usePlayerDirection); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"maxDistance\": " << hitbox.maxDistance << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"lateralOffset\": " << hitbox.lateralOffset << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"startSize\": "; AppendFloat3(oss, hitbox.startSize); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"endSize\": "; AppendFloat3(oss, hitbox.endSize); oss << "\n";
		AppendIndent(oss, indent); oss << "}";
	}

	void AppendVisualJson(std::ostringstream& oss, const BossAttackScript::Visual& visual, int indent)
	{
		AppendIndent(oss, indent); oss << "\"visual\": {\n";
		AppendIndent(oss, indent + 2); oss << "\"enabled\": "; AppendBool(oss, visual.enabled); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"billboard\": "; AppendBool(oss, visual.billboard); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"texturePath\": "; AppendString(oss, visual.texturePath); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"size\": "; AppendFloat2(oss, visual.size); oss << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"spawnHeight\": " << visual.spawnHeight << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"travelSec\": " << visual.travelSec << ",\n";
		AppendIndent(oss, indent + 2); oss << "\"spinDegPerSec\": " << visual.spinDegPerSec << "\n";
		AppendIndent(oss, indent); oss << "}";
	}

	void AppendNextLinksJson(std::ostringstream& oss, const std::vector<BossAttackScript::NextAttackLink>& nextLinks, int indent)
	{
		AppendIndent(oss, indent); oss << "\"nextLinks\": [";
		if (!nextLinks.empty()) oss << "\n";
		for (size_t i = 0; i < nextLinks.size(); ++i)
		{
			const auto& link = nextLinks[i];
			AppendIndent(oss, indent + 2); oss << "{ \"enabled\": "; AppendBool(oss, link.enabled);
			oss << ", \"attackIndex\": " << link.attackIndex << ", \"weight\": " << link.weight << " }";
			if (i + 1 < nextLinks.size()) oss << ",";
			oss << "\n";
		}
		if (!nextLinks.empty()) AppendIndent(oss, indent);
		oss << "]";
	}
}

int BossAttackScript::NormalizeProfileType(int profileType) { return ClampIntValue(profileType, 0, ProfileTypeCount - 1); }
int BossAttackScript::NormalizeSpawnMode(int spawnMode) { return ClampIntValue(spawnMode, 0, SpawnModeCount - 1); }
int BossAttackScript::NormalizeMovePreset(int movePreset) { return ClampIntValue(movePreset, 0, MovePresetCount - 1); }
int BossAttackScript::NormalizeAttackDeliveryMode(int deliveryMode) { return ClampIntValue(deliveryMode, 0, AttackDeliveryModeCount - 1); }
int BossAttackScript::NormalizePositionMode(int mode) { return ClampIntValue(mode, 0, PositionModeCount - 1); }
int BossAttackScript::NormalizeColliderStartMode(int mode) { return NormalizePositionMode(mode); }
int BossAttackScript::NormalizeColliderEndMode(int mode) { return NormalizePositionMode(mode); }
int BossAttackScript::NormalizeColliderShape(int shape) { return ClampIntValue(shape, 0, ColliderShapeCount - 1); }

const char* BossAttackScript::GetDefaultPath() { return kDefaultBossAttackScriptPath; }
const char* BossAttackScript::GetFinalBossPath() { return kFinalBossAttackScriptPath; }

const char* BossAttackScript::GetProfileName(int profileType)
{
	switch (NormalizeProfileType(profileType))
	{
	case ProfileHeavyMelee: return "Heavy Melee";
	case ProfileLightRanged: return "Light Ranged";
	case ProfileBalancedMid: return "Balanced Mid";
	case ProfileSwiftDebuff: return "Swift Debuff";
	default: return "Final Barrage";
	}
}

const char* BossAttackScript::GetProfileNameJp(int profileType)
{
	switch (NormalizeProfileType(profileType))
	{
	case ProfileHeavyMelee: return u8"\u8fd1\u63a5\u920d\u91cd";
	case ProfileLightRanged: return u8"\u9060\u8ddd\u96e2\u8efd\u88c5";
	case ProfileBalancedMid: return u8"\u4e2d\u8ddd\u96e2\u5747\u8861";
	case ProfileSwiftDebuff: return u8"\u4fca\u654f\u30c7\u30d0\u30d5";
	default: return u8"\u30e9\u30b9\u30dc\u30b9";
	}
}

const char* BossAttackScript::GetSpawnModeName(int spawnMode)
{
	switch (NormalizeSpawnMode(spawnMode))
	{
	case SpawnFixed: return "Fixed";
	case SpawnArenaRandom: return "Arena Random";
	case SpawnPlayerAreaRandom: return "Player Area Random";
	default: return "Player Position";
	}
}

const char* BossAttackScript::GetSpawnModeNameJp(int spawnMode)
{
	switch (NormalizeSpawnMode(spawnMode))
	{
	case SpawnFixed: return u8"\u56fa\u5b9a";
	case SpawnArenaRandom: return u8"\u30b9\u30c6\u30fc\u30b8\u5168\u57df\u30e9\u30f3\u30c0\u30e0";
	case SpawnPlayerAreaRandom: return u8"\u30d7\u30ec\u30a4\u30e4\u30fc\u5468\u8fba\u30e9\u30f3\u30c0\u30e0";
	default: return u8"\u30d7\u30ec\u30a4\u30e4\u30fc\u4f4d\u7f6e";
	}
}

const char* BossAttackScript::GetMovePresetName(int movePreset)
{
	switch (NormalizeMovePreset(movePreset))
	{
	case MoveNone: return "None";
	case MoveFacePlayer: return "Face Player";
	case MoveForward: return "Forward";
	case MoveChargePlayer: return "Charge Player";
	case MoveRetreat: return "Retreat";
	case MoveWarpBehindPlayer: return "Warp Behind";
	default: return "Move To Origin";
	}
}

const char* BossAttackScript::GetMovePresetNameJp(int movePreset)
{
	switch (NormalizeMovePreset(movePreset))
	{
	case MoveNone: return u8"\u306a\u3057";
	case MoveFacePlayer: return u8"\u30d7\u30ec\u30a4\u30e4\u30fc\u3092\u5411\u304f";
	case MoveForward: return u8"\u524d\u9032";
	case MoveChargePlayer: return u8"\u30d7\u30ec\u30a4\u30e4\u30fc\u3078\u7a81\u9032";
	case MoveRetreat: return u8"\u5f8c\u9000";
	case MoveWarpBehindPlayer: return u8"\u80cc\u5f8c\u30ef\u30fc\u30d7";
	default: return u8"\u653b\u6483\u59cb\u70b9\u3078\u79fb\u52d5";
	}
}

const char* BossAttackScript::GetAttackDeliveryName(int deliveryMode)
{
	switch (NormalizeAttackDeliveryMode(deliveryMode))
	{
	case AttackDeliveryRemoteFalling: return "Remote Falling";
	case AttackDeliveryRemoteGround: return "Remote Ground";
	default: return "Boss Self";
	}
}

const char* BossAttackScript::GetAttackDeliveryNameJp(int deliveryMode)
{
	switch (NormalizeAttackDeliveryMode(deliveryMode))
	{
	case AttackDeliveryRemoteFalling: return u8"\u9060\u9694(\u843d\u4e0b)";
	case AttackDeliveryRemoteGround: return u8"\u9060\u9694(\u5730\u4e0a)";
	default: return u8"\u30dc\u30b9\u81ea\u8eab";
	}
}

const char* BossAttackScript::GetPositionModeName(int mode)
{
	switch (NormalizePositionMode(mode))
	{
	case PositionCurrent: return "Current";
	case PositionAbsolute: return "Absolute";
	case PositionPlayer: return "Player";
	default: return "Player Area Random";
	}
}

const char* BossAttackScript::GetPositionModeNameJp(int mode)
{
	switch (NormalizePositionMode(mode))
	{
	case PositionCurrent: return u8"\u73fe\u5728\u5730";
	case PositionAbsolute: return u8"\u5ea7\u6a19\u6307\u5b9a";
	case PositionPlayer: return u8"\u30d7\u30ec\u30a4\u30e4\u30fc\u4f4d\u7f6e";
	default: return u8"\u30d7\u30ec\u30a4\u30e4\u30fc\u5468\u8fba\u30e9\u30f3\u30c0\u30e0";
	}
}
const char* BossAttackScript::GetColliderStartModeName(int mode) { return GetPositionModeName(mode); }
const char* BossAttackScript::GetColliderStartModeNameJp(int mode) { return GetPositionModeNameJp(mode); }
const char* BossAttackScript::GetColliderEndModeName(int mode) { return GetPositionModeName(mode); }
const char* BossAttackScript::GetColliderEndModeNameJp(int mode) { return GetPositionModeNameJp(mode); }
const char* BossAttackScript::GetColliderShapeName(int shape) { return (NormalizeColliderShape(shape) == ColliderShapeCircle) ? "Circle" : "Box"; }
const char* BossAttackScript::GetColliderShapeNameJp(int shape) { return (NormalizeColliderShape(shape) == ColliderShapeCircle) ? u8"\u5186\u5f62" : u8"\u30dc\u30c3\u30af\u30b9"; }

BossAttackScript::Profile BossAttackScript::MakeDefaultProfile(int profileType)
{
	Profile profile;
	profile.type = NormalizeProfileType(profileType);
	profile.displayName = GetProfileName(profile.type);
	profile.startsSpecial = false;
	profile.loops = (profile.type == ProfileFinalBarrage);

	auto makeBaseAttack = [&](const char* name, int deliveryMode) -> Attack
	{
		Attack attack;
		attack.name = name;
		attack.deliveryMode = deliveryMode;
		attack.hitbox.name = name;
		attack.hitbox.shape = ColliderShapeBox;
		attack.hitbox.startMode = PositionCurrent;
		attack.hitbox.startPos = { 0.0f, 0.05f, 0.0f };
		attack.hitbox.startSize = { 1.2f, 0.20f, 1.2f };
		attack.hitbox.endSize = attack.hitbox.startSize;
		attack.visual.enabled = true;
		attack.visual.texturePath = MakeDefaultTexturePath(profile.type);
		attack.visual.size = { 1.2f, 1.2f };
		attack.visual.travelSec = 0.35f;
		attack.visual.spawnHeight = (deliveryMode == AttackDeliveryRemoteGround) ? 0.1f : 3.5f;
		attack.visual.billboard = (deliveryMode == AttackDeliveryRemoteFalling);
		return attack;
	};

	if (profile.type == ProfileHeavyMelee)
	{
		Attack sweep = makeBaseAttack(u8"\u8599\u304e\u6255\u3044", AttackDeliveryBossSelf);
		sweep.movePreset = MoveForward;
		sweep.moveDistance = 3.2f;
		sweep.hitbox.useEndPosition = true;
		sweep.hitbox.endMode = PositionCurrent;
		sweep.hitbox.endPos = { 0.0f, 0.05f, 3.2f };
		sweep.hitbox.startSize = { 1.8f, 0.20f, 1.8f };
		sweep.hitbox.endSize = { 1.8f, 0.20f, 1.8f };
		sweep.damageScale = 1.10f;
		profile.attacks.push_back(sweep);

		Attack crush = makeBaseAttack(u8"\u53e9\u304d\u3064\u3051", AttackDeliveryBossSelf);
		crush.telegraphSec = 0.85f;
		crush.activeSec = 0.30f;
		crush.damageScale = 1.35f;
		crush.hitbox.shape = ColliderShapeCircle;
		crush.hitbox.useEndPosition = false;
		crush.hitbox.startSize = { 2.8f, 0.20f, 2.8f };
		crush.hitbox.endSize = crush.hitbox.startSize;
		crush.visual.billboard = false;
		crush.visual.size = { 2.2f, 2.2f };
		profile.attacks.push_back(crush);
	}
	else if (profile.type == ProfileLightRanged)
	{
		Attack blade = makeBaseAttack(u8"\u5730\u8d70\u5203", AttackDeliveryRemoteGround);
		blade.hitbox.useEndPosition = true;
		blade.hitbox.endMode = PositionPlayer;
		blade.hitbox.usePlayerDirection = true;
		blade.hitbox.maxDistance = 5.5f;
		blade.hitbox.startSize = { 0.9f, 0.20f, 0.9f };
		blade.hitbox.endSize = blade.hitbox.startSize;
		blade.visual.texturePath = "Assets/Texture/Effect/metal_blade.png";
		blade.visual.size = { 1.0f, 1.0f };
		blade.visual.billboard = false;
		profile.attacks.push_back(blade);

		Attack rain = makeBaseAttack(u8"\u843d\u4e0b\u9023\u5c04", AttackDeliveryRemoteFalling);
		rain.telegraphSec = 0.70f;
		rain.activeSec = 0.25f;
		rain.spawnMode = SpawnPlayerAreaRandom;
		rain.spawnCount = 4;
		rain.randomRadius = 2.6f;
		rain.hitbox.startMode = PositionPlayer;
		rain.hitbox.useEndPosition = false;
		rain.hitbox.shape = ColliderShapeCircle;
		rain.hitbox.startSize = { 1.4f, 0.20f, 1.4f };
		rain.hitbox.endSize = rain.hitbox.startSize;
		rain.visual.billboard = true;
		rain.visual.spawnHeight = 4.2f;
		profile.attacks.push_back(rain);
	}
	else if (profile.type == ProfileBalancedMid)
	{
		Attack rush = makeBaseAttack(u8"\u7a81\u9032\u7dda", AttackDeliveryBossSelf);
		rush.movePreset = MoveChargePlayer;
		rush.moveDistance = 4.5f;
		rush.hitbox.useEndPosition = true;
		rush.hitbox.endMode = PositionCurrent;
		rush.hitbox.endPos = { 0.0f, 0.05f, 4.5f };
		rush.hitbox.startSize = { 1.1f, 0.20f, 1.1f };
		rush.hitbox.endSize = rush.hitbox.startSize;
		rush.visual.billboard = false;
		profile.attacks.push_back(rush);

		Attack rain = makeBaseAttack(u8"\u843d\u4e0b\u4e8c\u9023", AttackDeliveryRemoteFalling);
		rain.spawnMode = SpawnPlayerAreaRandom;
		rain.spawnCount = 2;
		rain.randomRadius = 2.2f;
		rain.hitbox.startMode = PositionPlayer;
		rain.hitbox.useEndPosition = false;
		rain.hitbox.shape = ColliderShapeCircle;
		rain.hitbox.startSize = { 1.3f, 0.20f, 1.3f };
		rain.hitbox.endSize = rain.hitbox.startSize;
		rain.visual.billboard = true;
		profile.attacks.push_back(rain);
	}
	else if (profile.type == ProfileSwiftDebuff)
	{
		Attack ambush = makeBaseAttack(u8"\u4ea4\u5dee\u659c\u3081", AttackDeliveryBossSelf);
		ambush.telegraphSec = 0.45f;
		ambush.cooldownSec = 0.45f;
		ambush.movePreset = MoveWarpBehindPlayer;
		ambush.moveDistance = 1.5f;
		ambush.hitbox.useEndPosition = true;
		ambush.hitbox.endMode = PositionCurrent;
		ambush.hitbox.endPos = { 0.0f, 0.05f, 2.4f };
		ambush.hitbox.startSize = { 1.0f, 0.20f, 1.0f };
		ambush.hitbox.endSize = ambush.hitbox.startSize;
		profile.attacks.push_back(ambush);

		Attack sideBlade = makeBaseAttack(u8"\u504f\u5dee\u5203", AttackDeliveryRemoteGround);
		sideBlade.telegraphSec = 0.55f;
		sideBlade.hitbox.usePlayerDirection = true;
		sideBlade.hitbox.useEndPosition = true;
		sideBlade.hitbox.endMode = PositionPlayer;
		sideBlade.hitbox.maxDistance = 4.2f;
		sideBlade.hitbox.lateralOffset = 1.4f;
		sideBlade.hitbox.startSize = { 0.8f, 0.20f, 0.8f };
		sideBlade.hitbox.endSize = sideBlade.hitbox.startSize;
		sideBlade.visual.texturePath = "Assets/Texture/Effect/metal_blade.png";
		sideBlade.visual.billboard = false;
		sideBlade.visual.size = { 0.95f, 0.95f };
		profile.attacks.push_back(sideBlade);
	}
	else
	{
		profile.displayName = u8"\u30ca\u30cf\u30c8\u30e9";

		Attack abyssRain = makeBaseAttack(u8"\u5948\u843d\u843d\u4e0b", AttackDeliveryRemoteFalling);
		abyssRain.telegraphSec = 0.70f;
		abyssRain.cooldownSec = 0.25f;
		abyssRain.spawnMode = SpawnPlayerAreaRandom;
		abyssRain.spawnCount = 7;
		abyssRain.randomRadius = 3.0f;
		abyssRain.hitbox.startMode = PositionPlayer;
		abyssRain.hitbox.shape = ColliderShapeCircle;
		abyssRain.hitbox.useEndPosition = false;
		abyssRain.hitbox.startSize = { 1.55f, 0.20f, 1.55f };
		abyssRain.hitbox.endSize = abyssRain.hitbox.startSize;
		abyssRain.visual.texturePath = "Assets/Texture/Game/rock.png";
		abyssRain.visual.billboard = true;
		abyssRain.visual.spawnHeight = 4.8f;
		profile.attacks.push_back(abyssRain);

		Attack laserLine = makeBaseAttack(u8"\u7d2b\u76f4\u7dda\u65ac\u308a", AttackDeliveryRemoteGround);
		laserLine.telegraphSec = 0.62f;
		laserLine.cooldownSec = 0.20f;
		laserLine.hitbox.useEndPosition = true;
		laserLine.hitbox.endMode = PositionPlayer;
		laserLine.hitbox.usePlayerDirection = true;
		laserLine.hitbox.maxDistance = 8.5f;
		laserLine.hitbox.startSize = { 0.90f, 0.20f, 0.90f };
		laserLine.hitbox.endSize = laserLine.hitbox.startSize;
		laserLine.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		laserLine.visual.size = { 7.0f, 0.90f };
		laserLine.visual.billboard = false;
		profile.attacks.push_back(laserLine);

		Attack crossSlash = makeBaseAttack(u8"\u7d2b\u5341\u5b57\u65ac\u308a", AttackDeliveryRemoteGround);
		crossSlash.telegraphSec = 0.75f;
		crossSlash.cooldownSec = 0.25f;
		crossSlash.hitbox.useEndPosition = true;
		crossSlash.hitbox.endMode = PositionPlayer;
		crossSlash.hitbox.usePlayerDirection = true;
		crossSlash.hitbox.maxDistance = 8.0f;
		crossSlash.hitbox.startSize = { 0.85f, 0.20f, 0.85f };
		crossSlash.hitbox.endSize = crossSlash.hitbox.startSize;
		crossSlash.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		crossSlash.visual.size = { 7.0f, 0.80f };
		crossSlash.visual.billboard = false;
		profile.attacks.push_back(crossSlash);

		Attack bladeA = makeBaseAttack(u8"\u6ed1\u8d70\u5203A", AttackDeliveryRemoteGround);
		bladeA.telegraphSec = 0.50f;
		bladeA.cooldownSec = 0.15f;
		bladeA.hitbox.useEndPosition = true;
		bladeA.hitbox.endMode = PositionPlayer;
		bladeA.hitbox.usePlayerDirection = true;
		bladeA.hitbox.maxDistance = 6.5f;
		bladeA.hitbox.startSize = { 0.85f, 0.20f, 0.85f };
		bladeA.hitbox.endSize = bladeA.hitbox.startSize;
		bladeA.visual.texturePath = "Assets/Texture/Effect/metal_blade.png";
		bladeA.visual.size = { 1.05f, 1.05f };
		bladeA.visual.billboard = false;
		profile.attacks.push_back(bladeA);

		Attack bladeB = bladeA;
		bladeB.name = u8"\u6ed1\u8d70\u5203B";
		bladeB.hitbox.lateralOffset = 1.6f;
		profile.attacks.push_back(bladeB);

		Attack ring = makeBaseAttack(u8"\u5186\u74b0\u53ce\u675f", AttackDeliveryRemoteGround);
		ring.telegraphSec = 0.85f;
		ring.cooldownSec = 0.30f;
		ring.spawnMode = SpawnPlayerPosition;
		ring.spawnCount = 18;
		ring.randomRadius = 3.4f;
		ring.hitbox.shape = ColliderShapeCircle;
		ring.hitbox.useEndPosition = false;
		ring.hitbox.startMode = PositionPlayer;
		ring.hitbox.startSize = { 0.90f, 0.20f, 0.90f };
		ring.hitbox.endSize = ring.hitbox.startSize;
		ring.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		ring.visual.size = { 1.0f, 1.0f };
		ring.visual.billboard = false;
		profile.attacks.push_back(ring);

		Attack burst = makeBaseAttack(u8"\u74b0\u72b6\u7206\u7834", AttackDeliveryRemoteFalling);
		burst.telegraphSec = 0.70f;
		burst.cooldownSec = 0.25f;
		burst.spawnMode = SpawnPlayerPosition;
		burst.spawnCount = 6;
		burst.randomRadius = 2.4f;
		burst.hitbox.shape = ColliderShapeCircle;
		burst.hitbox.startMode = PositionPlayer;
		burst.hitbox.startSize = { 1.20f, 0.20f, 1.20f };
		burst.hitbox.endSize = burst.hitbox.startSize;
		burst.visual.texturePath = "Assets/Texture/Game/rock.png";
		burst.visual.billboard = true;
		burst.visual.spawnHeight = 3.8f;
		profile.attacks.push_back(burst);

		Attack radial = makeBaseAttack(u8"\u653e\u5c04\u6383\u5c04", AttackDeliveryRemoteGround);
		radial.telegraphSec = 0.80f;
		radial.cooldownSec = 0.25f;
		radial.spawnMode = SpawnFixed;
		radial.spawnCount = 7;
		radial.hitbox.usePlayerDirection = false;
		radial.hitbox.useEndPosition = false;
		radial.hitbox.startSize = { 0.75f, 0.20f, 7.5f };
		radial.hitbox.endSize = radial.hitbox.startSize;
		radial.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		radial.visual.size = { 7.5f, 0.75f };
		radial.visual.billboard = false;
		profile.attacks.push_back(radial);

		Attack fan = makeBaseAttack(u8"\u8d64\u6247\u6383\u5c04", AttackDeliveryRemoteGround);
		fan.telegraphSec = 0.95f;
		fan.cooldownSec = 0.30f;
		fan.spawnMode = SpawnFixed;
		fan.spawnCount = 4;
		fan.randomRadius = 110.0f;
		fan.hitbox.startSize = { 1.25f, 0.20f, 8.2f };
		fan.hitbox.endSize = fan.hitbox.startSize;
		fan.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		fan.visual.size = { 8.2f, 1.25f };
		fan.visual.billboard = false;
		profile.attacks.push_back(fan);

		Attack largeRing = makeBaseAttack(u8"\u5927\u74b0", AttackDeliveryRemoteGround);
		largeRing.telegraphSec = 1.00f;
		largeRing.cooldownSec = 0.30f;
		largeRing.spawnMode = SpawnFixed;
		largeRing.spawnCount = 24;
		largeRing.randomRadius = 4.8f;
		largeRing.hitbox.shape = ColliderShapeCircle;
		largeRing.hitbox.useEndPosition = false;
		largeRing.hitbox.startMode = PositionAbsolute;
		largeRing.hitbox.startPos = { 0.0f, 0.05f, 0.0f };
		largeRing.hitbox.startSize = { 1.05f, 0.20f, 1.05f };
		largeRing.hitbox.endSize = largeRing.hitbox.startSize;
		largeRing.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		largeRing.visual.size = { 1.1f, 1.1f };
		largeRing.visual.billboard = false;
		profile.attacks.push_back(largeRing);

		Attack lethal = makeBaseAttack(u8"\u7d42\u7109\u67d3\u3081", AttackDeliveryRemoteGround);
		lethal.telegraphSec = 2.20f;
		lethal.activeSec = 0.35f;
		lethal.cooldownSec = 1.00f;
		lethal.damageScale = 9999.0f;
		lethal.spawnMode = SpawnFixed;
		lethal.hitbox.shape = ColliderShapeBox;
		lethal.hitbox.useEndPosition = false;
		lethal.hitbox.startMode = PositionAbsolute;
		lethal.hitbox.startPos = { 0.0f, 0.05f, 0.0f };
		lethal.hitbox.startSize = { 12.0f, 0.20f, 12.0f };
		lethal.hitbox.endSize = lethal.hitbox.startSize;
		lethal.visual.texturePath = "Assets/Texture/Effect/laser_line.png";
		lethal.visual.size = { 12.0f, 12.0f };
		lethal.visual.billboard = false;
		profile.attacks.push_back(lethal);
	}

	ClampProfile(profile);
	return profile;
}

BossAttackScript::Database BossAttackScript::MakeDefaultDatabase()
{
	Database database;
	for (int type = 0; type < ProfileTypeCount; ++type) database.profiles.push_back(MakeDefaultProfile(type));
	return database;
}

const BossAttackScript::Profile* BossAttackScript::FindProfile(const Database& database, int profileType)
{
	const int normalizedType = NormalizeProfileType(profileType);
	for (const auto& profile : database.profiles) if (profile.type == normalizedType) return &profile;
	return nullptr;
}

BossAttackScript::Profile* BossAttackScript::FindProfile(Database& database, int profileType)
{
	const int normalizedType = NormalizeProfileType(profileType);
	for (auto& profile : database.profiles) if (profile.type == normalizedType) return &profile;
	return nullptr;
}

const BossAttackScript::Hitbox* BossAttackScript::FindCollider(const Profile& profile, int colliderId)
{
	if (colliderId < 0 || colliderId >= static_cast<int>(profile.colliders.size())) return nullptr;
	return &profile.colliders[colliderId];
}

int BossAttackScript::ChooseNextAttackIndex(const Profile& profile, const Attack& attack)
{
	int totalWeight = 0;
	for (const auto& link : attack.nextLinks)
	{
		if (!link.enabled) continue;
		if (link.attackIndex < 0 || link.attackIndex >= static_cast<int>(profile.attacks.size())) continue;
		if (!profile.attacks[link.attackIndex].enabled) continue;
		totalWeight += link.weight;
	}
	if (totalWeight <= 0) return -1;
	const int pick = std::rand() % totalWeight;
	int accum = 0;
	for (const auto& link : attack.nextLinks)
	{
		if (!link.enabled) continue;
		if (link.attackIndex < 0 || link.attackIndex >= static_cast<int>(profile.attacks.size())) continue;
		if (!profile.attacks[link.attackIndex].enabled) continue;
		accum += link.weight;
		if (pick < accum) return link.attackIndex;
	}
	return -1;
}

bool BossAttackScript::Load(Database& outDatabase, const char* path)
{
	const char* resolvedPath = (path && path[0] != '\0') ? path : kDefaultBossAttackScriptPath;
	std::ifstream ifs(resolvedPath, std::ios::binary);
	if (!ifs.is_open())
	{
		outDatabase = MakeDefaultDatabaseForPath(resolvedPath);
		Save(outDatabase, resolvedPath);
		return true;
	}

	std::ostringstream buffer;
	buffer << ifs.rdbuf();
	const std::string text = buffer.str();
	JsonValue root;
	JsonParser parser(text);
	if (!parser.Parse(root) || root.type != JsonValue::TypeObject)
	{
		outDatabase = MakeDefaultDatabaseForPath(resolvedPath);
		return false;
	}

	Database database;
	const JsonValue* profilesValue = GetObjectField(root, "profiles");
	if (profilesValue && profilesValue->type == JsonValue::TypeArray)
	{
		for (const JsonValue& item : profilesValue->arrayValue)
		{
			if (item.type != JsonValue::TypeObject) continue;
			Profile profile;
			LoadProfileFromJson(profile, item);
			database.profiles.push_back(profile);
		}
	}

	if (database.profiles.empty())
	{
		database = MakeDefaultDatabaseForPath(resolvedPath);
	}
	ClampDatabase(database);
	outDatabase = database;
	return true;
}

bool BossAttackScript::Save(const Database& database, const char* path)
{
	Database clamped = database;
	ClampDatabase(clamped);

	std::ostringstream oss;
	oss << "{\n";
	AppendIndent(oss, 2); oss << "\"version\": " << kCurrentVersion << ",\n";
	AppendIndent(oss, 2); oss << "\"profiles\": [\n";
	for (size_t profileIndex = 0; profileIndex < clamped.profiles.size(); ++profileIndex)
	{
		const Profile& profile = clamped.profiles[profileIndex];
		AppendIndent(oss, 4); oss << "{\n";
		AppendIndent(oss, 6); oss << "\"type\": " << profile.type << ",\n";
		AppendIndent(oss, 6); oss << "\"displayName\": "; AppendString(oss, profile.displayName); oss << ",\n";
		AppendIndent(oss, 6); oss << "\"hpScale\": " << profile.hpScale << ",\n";
		AppendIndent(oss, 6); oss << "\"guardScale\": " << profile.guardScale << ",\n";
		AppendIndent(oss, 6); oss << "\"sizeScale\": " << profile.sizeScale << ",\n";
		AppendIndent(oss, 6); oss << "\"cooldownScale\": " << profile.cooldownScale << ",\n";
		AppendIndent(oss, 6); oss << "\"telegraphScale\": " << profile.telegraphScale << ",\n";
		AppendIndent(oss, 6); oss << "\"damageScale\": " << profile.damageScale << ",\n";
		AppendIndent(oss, 6); oss << "\"startsSpecial\": "; AppendBool(oss, profile.startsSpecial); oss << ",\n";
		AppendIndent(oss, 6); oss << "\"loops\": "; AppendBool(oss, profile.loops); oss << ",\n";
		AppendIndent(oss, 6); oss << "\"attacks\": [\n";
		for (size_t attackIndex = 0; attackIndex < profile.attacks.size(); ++attackIndex)
		{
			const Attack& attack = profile.attacks[attackIndex];
			AppendIndent(oss, 8); oss << "{\n";
			AppendIndent(oss, 10); oss << "\"enabled\": "; AppendBool(oss, attack.enabled); oss << ",\n";
			AppendIndent(oss, 10); oss << "\"name\": "; AppendString(oss, attack.name); oss << ",\n";
			AppendIndent(oss, 10); oss << "\"telegraphSec\": " << attack.telegraphSec << ",\n";
			AppendIndent(oss, 10); oss << "\"activeSec\": " << attack.activeSec << ",\n";
			AppendIndent(oss, 10); oss << "\"cooldownSec\": " << attack.cooldownSec << ",\n";
			AppendIndent(oss, 10); oss << "\"damageScale\": " << attack.damageScale << ",\n";
			AppendIndent(oss, 10); oss << "\"repeatCount\": " << attack.repeatCount << ",\n";
			AppendIndent(oss, 10); oss << "\"repeatIntervalSec\": " << attack.repeatIntervalSec << ",\n";
			AppendIndent(oss, 10); oss << "\"deliveryMode\": " << attack.deliveryMode << ",\n";
			AppendIndent(oss, 10); oss << "\"movePreset\": " << attack.movePreset << ",\n";
			AppendIndent(oss, 10); oss << "\"moveSpeed\": " << attack.moveSpeed << ",\n";
			AppendIndent(oss, 10); oss << "\"moveDistance\": " << attack.moveDistance << ",\n";
			AppendIndent(oss, 10); oss << "\"spawnMode\": " << attack.spawnMode << ",\n";
			AppendIndent(oss, 10); oss << "\"spawnCount\": " << attack.spawnCount << ",\n";
			AppendIndent(oss, 10); oss << "\"randomRadius\": " << attack.randomRadius << ",\n";
			AppendIndent(oss, 10); oss << "\"followStrength\": " << attack.followStrength << ",\n";
			AppendHitboxJson(oss, attack.hitbox, 10); oss << ",\n";
			AppendVisualJson(oss, attack.visual, 10); oss << ",\n";
			AppendNextLinksJson(oss, attack.nextLinks, 10); oss << "\n";
			AppendIndent(oss, 8); oss << "}";
			if (attackIndex + 1 < profile.attacks.size()) oss << ",";
			oss << "\n";
		}
		AppendIndent(oss, 6); oss << "]\n";
		AppendIndent(oss, 4); oss << "}";
		if (profileIndex + 1 < clamped.profiles.size()) oss << ",";
		oss << "\n";
	}
	AppendIndent(oss, 2); oss << "]\n";
	oss << "}\n";

	const char* resolved = (path && path[0] != '\0') ? path : kDefaultBossAttackScriptPath;
	const std::string text = oss.str();
	const bool mainOk = WriteTextFile(resolved, text);
	if ((!path || path[0] == '\0') ||
		std::strcmp(resolved, kDefaultBossAttackScriptPath) == 0)
	{
		WriteTextFile(kMirrorBossAttackScriptPath, text);
		WriteTextFile(kDebugBossAttackScriptPath, text);
		WriteTextFile(kUpstreamMirrorBossAttackScriptPath, text);
	}
	else if (IsFinalBossPath(resolved))
	{
		WriteTextFile(kFinalMirrorBossAttackScriptPath, text);
		WriteTextFile(kFinalDebugBossAttackScriptPath, text);
		WriteTextFile(kFinalUpstreamMirrorBossAttackScriptPath, text);
	}
	return mainOk;
}

bool BossAttackScript::LoadProfile(Profile& outProfile, int profileType, const char* path)
{
	const char* resolvedPath = (path && path[0] != '\0') ? path : kDefaultBossAttackScriptPath;
	Database database;
	if (!Load(database, resolvedPath)) return false;
	const Profile* profile = FindProfile(database, profileType);
	if (!profile)
	{
		outProfile = MakeDefaultProfile(profileType);
		return false;
	}
	if (profileType == ProfileFinalBarrage &&
		IsFinalBossPath(resolvedPath) &&
		NeedsFinalBossProfileMigration(*profile))
	{
		outProfile = MakeDefaultProfile(profileType);
		Database migrated;
		migrated.profiles.push_back(outProfile);
		Save(migrated, resolvedPath);
		return true;
	}
	outProfile = *profile;
	return true;
}
