#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "UserMessages.h"
#include "fmod_server_util.h"

#define DEFAULT_MIN_ATTEN 40.0f
#define DEFAULT_MAX_ATTEN 40000.0f

namespace HLFMOD
{
	void UTIL_EmitSound(const char* sound_path, float volume)
	{
		Vector dummyVec;
		UTIL_EmitSound(sound_path, volume, false, dummyVec, DEFAULT_MIN_ATTEN, DEFAULT_MAX_ATTEN, 1.0f);
	}

	void UTIL_EmitSound(const char* sound_path, float volume, const Vector& pos)
	{
		UTIL_EmitSound(sound_path, volume, false, pos, DEFAULT_MIN_ATTEN, DEFAULT_MAX_ATTEN, 1.0f);
	}

	void UTIL_EmitSound(const char* sound_path, float volume, bool looping, const Vector& pos, float min_atten, float max_atten, float pitch)
	{
		MESSAGE_BEGIN(MSG_ALL, gmsgFmodEmit, NULL);
		WRITE_STRING(sound_path);
		WRITE_BYTE(looping);
		WRITE_COORD(pos.x);
		WRITE_COORD(pos.y);
		WRITE_COORD(pos.z);
		WRITE_COORD(volume);	
		WRITE_COORD(min_atten); 
		WRITE_LONG((long)max_atten); // for some reason this refuses to work as a float. +/- 1 inch shouldn't matter for max_atten though.
		WRITE_COORD(pitch);		
		MESSAGE_END();
	}
}