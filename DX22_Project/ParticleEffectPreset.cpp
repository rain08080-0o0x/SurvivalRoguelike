#include "ParticleEffectPreset.h"

#include <fstream>
#include <string>

namespace
{
	const char* kDefaultHitEffectPath = "Assets/hit_effect.cfg";
	const char* kMirrorHitEffectPath = "DX22_Project/Assets/hit_effect.cfg";
	const char* kDebugHitEffectPath = "x64/Debug/Assets/hit_effect.cfg";
	const char* kUpstreamMirrorHitEffectPath = "../../DX22_Project/Assets/hit_effect.cfg";

	bool IsDefaultPathArgument(const char* path)
	{
		return (path == nullptr) || (path[0] == '\0');
	}

	const char* ResolvePath(const char* path)
	{
		return IsDefaultPathArgument(path) ? kDefaultHitEffectPath : path;
	}

	void Trim(std::string& text)
	{
		const size_t begin = text.find_first_not_of(" \t\r\n");
		if (begin == std::string::npos)
		{
			text.clear();
			return;
		}
		const size_t end = text.find_last_not_of(" \t\r\n");
		text = text.substr(begin, end - begin + 1);
	}

	bool TryParseInt(const std::string& text, int& outValue)
	{
		try
		{
			outValue = std::stoi(text);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryParseFloat(const std::string& text, float& outValue)
	{
		try
		{
			outValue = std::stof(text);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	bool TryParseBool(const std::string& text, bool& outValue)
	{
		if (text == "1" || text == "true" || text == "True")
		{
			outValue = true;
			return true;
		}
		if (text == "0" || text == "false" || text == "False")
		{
			outValue = false;
			return true;
		}
		return false;
	}

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue) return minValue;
		if (value > maxValue) return maxValue;
		return value;
	}
}

ParticleEmitter2D::Settings ParticleEffectPreset::MakeDefaultSettings()
{
	ParticleEmitter2D::Settings settings{};
	settings.position = { 0.0f, 1.1f, 0.0f };
	settings.angle = { -15.0f, 0.0f, 0.0f };
	settings.startSize = { 0.85f, 0.85f };
	settings.endSize = { 0.20f, 0.20f };
	settings.startColor = { 1.0f, 0.82f, 0.30f, 1.0f };
	settings.endColor = { 1.0f, 0.22f, 0.12f, 0.0f };
	settings.emitSpeed = 2.6f;
	settings.duration = 1.4f;
	settings.emitCount = 64;
	settings.maxEmissionAngleDeg = 24.0f;
	settings.particleLifetime = 1.2f;
	settings.maxTravelDistance = 4.2f;
	settings.alive = true;
	settings.billboard = true;
	settings.loop = true;
	settings.worldSpace = true;
	settings.texturePath = "Assets/Texture/Star.png";
	return settings;
}

bool ParticleEffectPreset::Load(ParticleEmitter2D::Settings& outSettings, const char* path)
{
	outSettings = MakeDefaultSettings();

	std::ifstream ifs(ResolvePath(path));
	if (!ifs.is_open() && IsDefaultPathArgument(path))
	{
		const char* fallbackPaths[] =
		{
			kMirrorHitEffectPath,
			kDebugHitEffectPath,
			kUpstreamMirrorHitEffectPath
		};
		for (const char* fallbackPath : fallbackPaths)
		{
			ifs.open(fallbackPath);
			if (ifs.is_open())
			{
				break;
			}
			ifs.clear();
		}
	}
	if (!ifs.is_open())
	{
		return false;
	}

	std::string line;
	while (std::getline(ifs, line))
	{
		const size_t commentPos = line.find('#');
		if (commentPos != std::string::npos)
		{
			line.erase(commentPos);
		}
		Trim(line);
		if (line.empty())
		{
			continue;
		}

		const size_t equalPos = line.find('=');
		if (equalPos == std::string::npos)
		{
			continue;
		}

		std::string key = line.substr(0, equalPos);
		std::string value = line.substr(equalPos + 1);
		Trim(key);
		Trim(value);
		if (key.empty())
		{
			continue;
		}

		if (key == "format")
		{
			if (value != "ParticleEffectPreset")
			{
				return false;
			}
			continue;
		}
		if (key == "version")
		{
			continue;
		}

		if (key == "positionX") { TryParseFloat(value, outSettings.position.x); continue; }
		if (key == "positionY") { TryParseFloat(value, outSettings.position.y); continue; }
		if (key == "positionZ") { TryParseFloat(value, outSettings.position.z); continue; }
		if (key == "angleX") { TryParseFloat(value, outSettings.angle.x); continue; }
		if (key == "angleY") { TryParseFloat(value, outSettings.angle.y); continue; }
		if (key == "angleZ") { TryParseFloat(value, outSettings.angle.z); continue; }
		if (key == "startSizeX") { TryParseFloat(value, outSettings.startSize.x); continue; }
		if (key == "startSizeY") { TryParseFloat(value, outSettings.startSize.y); continue; }
		if (key == "endSizeX") { TryParseFloat(value, outSettings.endSize.x); continue; }
		if (key == "endSizeY") { TryParseFloat(value, outSettings.endSize.y); continue; }
		if (key == "startColorR") { TryParseFloat(value, outSettings.startColor.x); continue; }
		if (key == "startColorG") { TryParseFloat(value, outSettings.startColor.y); continue; }
		if (key == "startColorB") { TryParseFloat(value, outSettings.startColor.z); continue; }
		if (key == "startColorA") { TryParseFloat(value, outSettings.startColor.w); continue; }
		if (key == "endColorR") { TryParseFloat(value, outSettings.endColor.x); continue; }
		if (key == "endColorG") { TryParseFloat(value, outSettings.endColor.y); continue; }
		if (key == "endColorB") { TryParseFloat(value, outSettings.endColor.z); continue; }
		if (key == "endColorA") { TryParseFloat(value, outSettings.endColor.w); continue; }
		if (key == "emitSpeed") { TryParseFloat(value, outSettings.emitSpeed); continue; }
		if (key == "duration") { TryParseFloat(value, outSettings.duration); continue; }
		if (key == "emitCount") { TryParseInt(value, outSettings.emitCount); continue; }
		if (key == "maxEmissionAngleDeg") { TryParseFloat(value, outSettings.maxEmissionAngleDeg); continue; }
		if (key == "particleLifetime") { TryParseFloat(value, outSettings.particleLifetime); continue; }
		if (key == "maxTravelDistance") { TryParseFloat(value, outSettings.maxTravelDistance); continue; }
		if (key == "alive") { TryParseBool(value, outSettings.alive); continue; }
		if (key == "billboard") { TryParseBool(value, outSettings.billboard); continue; }
		if (key == "loop") { TryParseBool(value, outSettings.loop); continue; }
		if (key == "worldSpace") { TryParseBool(value, outSettings.worldSpace); continue; }
		if (key == "texturePath") { outSettings.texturePath = value; continue; }
	}

	outSettings.duration = ClampFloat(outSettings.duration, 0.01f, 20.0f);
	outSettings.particleLifetime = ClampFloat(outSettings.particleLifetime, 0.01f, 20.0f);
	outSettings.emitSpeed = ClampFloat(outSettings.emitSpeed, 0.0f, 20.0f);
	outSettings.maxEmissionAngleDeg = ClampFloat(outSettings.maxEmissionAngleDeg, 0.0f, 180.0f);
	outSettings.maxTravelDistance = ClampFloat(outSettings.maxTravelDistance, 0.0f, 50.0f);
	if (outSettings.emitCount < 0) outSettings.emitCount = 0;
	if (outSettings.emitCount > 512) outSettings.emitCount = 512;
	if (outSettings.startSize.x < 0.01f) outSettings.startSize.x = 0.01f;
	if (outSettings.startSize.y < 0.01f) outSettings.startSize.y = 0.01f;
	if (outSettings.endSize.x < 0.01f) outSettings.endSize.x = 0.01f;
	if (outSettings.endSize.y < 0.01f) outSettings.endSize.y = 0.01f;
	if (outSettings.texturePath.empty())
	{
		outSettings.texturePath = "Assets/Texture/Star.png";
	}
	return true;
}

bool ParticleEffectPreset::Save(const ParticleEmitter2D::Settings& settings, const char* path)
{
	auto writeToPath = [&](const char* targetPath) -> bool
	{
		std::ofstream ofs(targetPath, std::ios::trunc);
		if (!ofs.is_open())
		{
			return false;
		}

		ofs << "# DX22 particle effect preset\n";
		ofs << "format=ParticleEffectPreset\n";
		ofs << "version=1\n";
		ofs << "positionX=" << settings.position.x << "\n";
		ofs << "positionY=" << settings.position.y << "\n";
		ofs << "positionZ=" << settings.position.z << "\n";
		ofs << "angleX=" << settings.angle.x << "\n";
		ofs << "angleY=" << settings.angle.y << "\n";
		ofs << "angleZ=" << settings.angle.z << "\n";
		ofs << "startSizeX=" << settings.startSize.x << "\n";
		ofs << "startSizeY=" << settings.startSize.y << "\n";
		ofs << "endSizeX=" << settings.endSize.x << "\n";
		ofs << "endSizeY=" << settings.endSize.y << "\n";
		ofs << "startColorR=" << settings.startColor.x << "\n";
		ofs << "startColorG=" << settings.startColor.y << "\n";
		ofs << "startColorB=" << settings.startColor.z << "\n";
		ofs << "startColorA=" << settings.startColor.w << "\n";
		ofs << "endColorR=" << settings.endColor.x << "\n";
		ofs << "endColorG=" << settings.endColor.y << "\n";
		ofs << "endColorB=" << settings.endColor.z << "\n";
		ofs << "endColorA=" << settings.endColor.w << "\n";
		ofs << "emitSpeed=" << settings.emitSpeed << "\n";
		ofs << "duration=" << settings.duration << "\n";
		ofs << "emitCount=" << settings.emitCount << "\n";
		ofs << "maxEmissionAngleDeg=" << settings.maxEmissionAngleDeg << "\n";
		ofs << "particleLifetime=" << settings.particleLifetime << "\n";
		ofs << "maxTravelDistance=" << settings.maxTravelDistance << "\n";
		ofs << "alive=" << (settings.alive ? 1 : 0) << "\n";
		ofs << "billboard=" << (settings.billboard ? 1 : 0) << "\n";
		ofs << "loop=" << (settings.loop ? 1 : 0) << "\n";
		ofs << "worldSpace=" << (settings.worldSpace ? 1 : 0) << "\n";
		ofs << "texturePath=" << settings.texturePath << "\n";
		return true;
	};

	if (!IsDefaultPathArgument(path))
	{
		return writeToPath(ResolvePath(path));
	}

	bool anySaved = false;
	const char* syncPaths[] =
	{
		kDefaultHitEffectPath,
		kMirrorHitEffectPath,
		kDebugHitEffectPath,
		kUpstreamMirrorHitEffectPath
	};
	for (const char* syncPath : syncPaths)
	{
		if (writeToPath(syncPath))
		{
			anySaved = true;
		}
	}
	return anySaved;
}

const char* ParticleEffectPreset::GetDefaultPath()
{
	return kDefaultHitEffectPath;
}
