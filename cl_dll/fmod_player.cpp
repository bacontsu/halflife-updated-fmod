#include "cl_dll.h"
#include "cl_util.h"
#include "extdll.h"
#include "hud.h" // CHudFmodPlayer declared in hud.h
#include "parsemsg.h"
#include "fmod_manager.h"
#include "FMOD/fmod_errors.h"

#include <fstream>
#include <iostream>

DECLARE_MESSAGE(m_Fmod, FmodCache)
DECLARE_MESSAGE(m_Fmod, FmodAmb)
DECLARE_MESSAGE(m_Fmod, FmodTrk)

bool CHudFmodPlayer::Init()
{
	HOOK_MESSAGE(FmodCache);
	HOOK_MESSAGE(FmodAmb);
	HOOK_MESSAGE(FmodTrk);

	gHUD.AddHudElem(this);
	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodCache(const char* pszName, int iSize, void* pbuf)
{
	std::string gamedir = gEngfuncs.pfnGetGameDirectory();
	std::string level_name = gEngfuncs.pfnGetLevelName();
	std::string soundcache_path = gamedir + "/" + level_name + "_soundcache.txt";

	Fmod_Release_Channels();
	Fmod_Release_Sounds(); // TODO: find elegant way to avoid unloading and reloading sounds. For now just release all.

	std::ifstream soundcache_file;
	soundcache_file.open(soundcache_path);

	if (!soundcache_file.is_open())
	{
		_Fmod_Report("WARNING", "Could not open soundcache file " + soundcache_path + ". No sounds were precached!");
		return true;
	}
	else
		_Fmod_Report("INFO", "Precaching from file: " + soundcache_path);

	std::string filename;
	while (std::getline(soundcache_file, filename))
	{
		FMOD::Sound* sound = Fmod_CacheSound(filename.c_str(), false);
		if (!sound)
		{
			_Fmod_Report("ERROR", "Error occured during precaching. Tried precaching: " + filename + ". Precaching stopped.");
			return false;
		}
	}

	if (!soundcache_file.eof())
		_Fmod_Report("WARNING", "Stopped reading soundcache file " + soundcache_path + " before the end of file due to error.");

	soundcache_file.close();

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodAmb(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string msg = std::string(READ_STRING());
	bool looping = READ_BYTE();

	FMOD_VECTOR pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD(); // Swap Y and Z. TODO: rotate vectors in pm_shared
	pos.z = READ_COORD();

	FMOD_VECTOR vel;
	vel.x = 0;
	vel.y = 0;
	vel.z = 0;

	std::string channel_name = msg.substr(0, msg.find('\n'));
	std::string sound_path = msg.substr(msg.find('\n') + 1, std::string::npos);

	FMOD::Sound* sound = NULL;
	FMOD::Channel* channel = NULL;

	auto sound_iter = fmod_cached_sounds.find(sound_path);

	if (sound_iter == fmod_cached_sounds.end())
	{
		_Fmod_Report("WARNING", "Entity " + channel_name + " playing uncached sound " + sound_path +
									". Add the sound to your [MAPNAME].bsp_soundcache.txt file.");
		_Fmod_Report("WARNING", "Attempting to cache and play sound " + sound_path);
		sound = Fmod_CacheSound(sound_path.c_str(), false);
		if (!sound)
			return false;
	}
	else
		sound = sound_iter->second;

	// Non-looping sounds need a new channel every time they play
	if (!looping)
	{
		channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, false, 1.0f);
		if (!channel)
			return false;

		channel->set3DAttributes(&pos, &vel);
		channel->setPaused(false);
	}

	// If looping, find the existing channel
	else
	{
		auto channel_iter = fmod_channels.find(channel_name);

		if (channel_iter == fmod_channels.end())
		{
			// TODO: send looping and volume info from entity
			channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, true, 1.0f);
			if (!channel)
				return false;
		}
		else
			channel = channel_iter->second;

		channel->set3DAttributes(&pos, &vel);

		// When a looping fmod_ambient gets used, by default it'll flip the status of paused
		bool paused = false;
		channel->getPaused(&paused);
		channel->setPaused(!paused);
	}

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodTrk(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	const char* msg = READ_STRING();

	//Fmod_Sound_Container sound = Fmod_LoadSound(msg);
	//Fmod_PlaySound(sound, fmod_mp3_group, false, 1.0f);

	return true;
}

bool CHudFmodPlayer::VidInit() { return true; }
bool CHudFmodPlayer::Draw(float flTime) { return true; }
void CHudFmodPlayer::Reset() { return; }