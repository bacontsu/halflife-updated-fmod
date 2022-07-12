#pragma once

#include "vector.h"

namespace HLFMOD
{
	void UTIL_EmitSound(const char* sound_path, float volume);
	void UTIL_EmitSound(const char* sound_path, float volume, const Vector& pos);
	void UTIL_EmitSound(const char* sound_path, const char* channel_name, float volume, bool looping, const Vector& pos, float min_atten, float max_atten, float pitch);

	void UTIL_TogglePauseChannel(const char* channel_name);
	void UTIL_StopChannel(const char* channel_name);
}