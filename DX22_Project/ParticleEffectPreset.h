#pragma once

#include "ParticleEmitter2D.h"

class ParticleEffectPreset
{
public:
	static ParticleEmitter2D::Settings MakeDefaultSettings();
	static bool Load(ParticleEmitter2D::Settings& outSettings, const char* path = nullptr);
	static bool Save(const ParticleEmitter2D::Settings& settings, const char* path = nullptr);
	static const char* GetDefaultPath();
};
