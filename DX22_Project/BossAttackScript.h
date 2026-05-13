#pragma once

#include <DirectXMath.h>
#include <string>
#include <vector>

namespace BossAttackScript
{
	enum ProfileType
	{
		ProfileHeavyMelee = 0,
		ProfileLightRanged,
		ProfileBalancedMid,
		ProfileSwiftDebuff,
		ProfileFinalBarrage,
		ProfileTypeCount
	};

	enum SpawnMode
	{
		SpawnFixed = 0,
		SpawnArenaRandom,
		SpawnPlayerAreaRandom,
		SpawnPlayerPosition,
		SpawnModeCount
	};

	enum AttackDeliveryMode
	{
		AttackDeliveryBossSelf = 0,
		AttackDeliveryRemoteFalling,
		AttackDeliveryRemoteGround,
		AttackDeliveryModeCount
	};

	enum MovePreset
	{
		MoveNone = 0,
		MoveFacePlayer,
		MoveForward,
		MoveChargePlayer,
		MoveRetreat,
		MoveWarpBehindPlayer,
		MoveToAttackOrigin,
		MovePresetCount
	};

	enum PositionMode
	{
		PositionCurrent = 0,
		PositionAbsolute,
		PositionPlayer,
		PositionPlayerAreaRandom,
		PositionModeCount
	};

	enum ColliderStartModeCompat
	{
		ColliderStartCurrent = PositionCurrent,
		ColliderStartAbsolute = PositionAbsolute,
		ColliderStartPlayer = PositionPlayer,
		ColliderStartPlayerAreaRandom = PositionPlayerAreaRandom
	};

	enum ColliderEndModeCompat
	{
		ColliderEndAbsolute = PositionAbsolute,
		ColliderEndCurrentRelative = PositionCurrent,
		ColliderEndPlayer = PositionPlayer,
		ColliderEndPlayerAreaRandom = PositionPlayerAreaRandom
	};

	enum
	{
		ColliderStartModeCount = PositionModeCount,
		ColliderEndModeCount = PositionModeCount
	};

	enum ColliderShape
	{
		ColliderShapeBox = 0,
		ColliderShapeCircle,
		ColliderShapeCount
	};

	struct Hitbox
	{
		int id = 0;
		bool enabled = true;
		std::string name;
		int shape = ColliderShapeBox;
		int startMode = PositionCurrent;
		DirectX::XMFLOAT3 startPos = { 0.0f, 0.05f, 0.0f };
		float startRandomRadius = 0.0f;
		bool useEndPosition = false;
		int endMode = PositionAbsolute;
		DirectX::XMFLOAT3 endPos = { 0.0f, 0.05f, 0.0f };
		float endRandomRadius = 0.0f;
		bool usePlayerDirection = false;
		float maxDistance = 0.0f;
		float lateralOffset = 0.0f;
		DirectX::XMFLOAT3 startSize = { 1.0f, 0.10f, 1.0f };
		DirectX::XMFLOAT3 endSize = { 1.0f, 0.10f, 1.0f };
		float yawDeg = 0.0f;
	};

	using Collider = Hitbox;

	struct Visual
	{
		bool enabled = false;
		bool billboard = true;
		std::string texturePath = "Assets/Texture/Game/rock.png";
		DirectX::XMFLOAT2 size = { 1.0f, 1.0f };
		float spawnHeight = 2.5f;
		float travelSec = 0.35f;
		float spinDegPerSec = 0.0f;
	};

	struct NextAttackLink
	{
		bool enabled = true;
		int attackIndex = -1;
		int weight = 1;
	};

	struct Attack
	{
		bool enabled = true;
		std::string name;
		float telegraphSec = 0.60f;
		float activeSec = 0.25f;
		float cooldownSec = 0.60f;
		float damageScale = 1.0f;
		int repeatCount = 1;
		float repeatIntervalSec = 0.20f;
		int deliveryMode = AttackDeliveryBossSelf;
		int movePreset = MoveNone;
		float moveSpeed = 4.0f;
		float moveDistance = 2.0f;
		int spawnMode = SpawnFixed;
		int spawnCount = 1;
		float randomRadius = 2.0f;
		float followStrength = 0.0f;
		std::vector<int> colliderIds;
		Hitbox hitbox;
		Visual visual;
		std::vector<NextAttackLink> nextLinks;
	};

	struct Profile
	{
		int type = ProfileHeavyMelee;
		std::string displayName;
		float hpScale = 1.0f;
		float guardScale = 1.0f;
		float sizeScale = 1.0f;
		float cooldownScale = 1.0f;
		float telegraphScale = 1.0f;
		float damageScale = 1.0f;
		bool startsSpecial = false;
		bool loops = false;
		std::vector<Hitbox> colliders;
		std::vector<Attack> attacks;
	};

	struct Database
	{
		std::vector<Profile> profiles;
	};

	Database MakeDefaultDatabase();
	Profile MakeDefaultProfile(int profileType);
	bool Load(Database& outDatabase, const char* path = nullptr);
	bool Save(const Database& database, const char* path = nullptr);
	bool LoadProfile(Profile& outProfile, int profileType, const char* path = nullptr);
	const Profile* FindProfile(const Database& database, int profileType);
	Profile* FindProfile(Database& database, int profileType);
	const Hitbox* FindCollider(const Profile& profile, int colliderId);
	int ChooseNextAttackIndex(const Profile& profile, const Attack& attack);
	int NormalizeProfileType(int profileType);
	int NormalizeSpawnMode(int spawnMode);
	int NormalizeMovePreset(int movePreset);
	int NormalizeAttackDeliveryMode(int deliveryMode);
	int NormalizePositionMode(int mode);
	int NormalizeColliderStartMode(int mode);
	int NormalizeColliderEndMode(int mode);
	int NormalizeColliderShape(int shape);
	const char* GetDefaultPath();
	const char* GetFinalBossPath();
	const char* GetProfileName(int profileType);
	const char* GetProfileNameJp(int profileType);
	const char* GetSpawnModeName(int spawnMode);
	const char* GetSpawnModeNameJp(int spawnMode);
	const char* GetMovePresetName(int movePreset);
	const char* GetMovePresetNameJp(int movePreset);
	const char* GetAttackDeliveryName(int deliveryMode);
	const char* GetAttackDeliveryNameJp(int deliveryMode);
	const char* GetPositionModeName(int mode);
	const char* GetPositionModeNameJp(int mode);
	const char* GetColliderStartModeName(int mode);
	const char* GetColliderStartModeNameJp(int mode);
	const char* GetColliderEndModeName(int mode);
	const char* GetColliderEndModeNameJp(int mode);
	const char* GetColliderShapeName(int shape);
	const char* GetColliderShapeNameJp(int shape);
}
