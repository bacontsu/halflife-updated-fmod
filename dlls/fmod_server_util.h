#pragma once

#include "vector.h"

namespace HLFMOD
{
	void UTIL_EmitSound(const char* sound_path, float volume);
	void UTIL_EmitSound(const char* sound_path, float volume, const Vector& pos);
	void UTIL_EmitSound(const char* sound_path, float volume, bool looping, const Vector& pos, float min_atten, float max_atten, float pitch);
}