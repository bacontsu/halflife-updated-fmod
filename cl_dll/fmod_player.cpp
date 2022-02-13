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
	//Fmod_Release_Sounds(); // We now intelligently only release sounds that aren't used in the current map

	// Get all sound path strings from file
	std::vector<std::string> sound_paths;
	std::ifstream soundcache_file;

	sound_paths.reserve(100);
	soundcache_file.open(soundcache_path);

	if (!soundcache_file.is_open())
	{
		_Fmod_Report("WARNING", "Could not open soundcache file " + soundcache_path + ". No sounds were precached!");
		Fmod_Release_Sounds(); // Release all sounds
		return true;
	}
	else _Fmod_Report("INFO", "Precaching sounds from file: " + soundcache_path);

	std::string filename;
	while (std::getline(soundcache_file, filename)) sound_paths.push_back(filename);

	if (!soundcache_file.eof())
		_Fmod_Report("WARNING", "Stopped reading soundcache file " + soundcache_path + " before the end of file due to error.");

	soundcache_file.close();

	// Create a vector of bools that will indicate whether or not to unload a sound
	std::vector<bool> dont_unload(fmod_cached_sounds.size(), false);

	// Loop through our cached sounds.
	// Find should be faster on vectors than unsorted_map, so looping through fmod_cached_sounds and finding matches in sound_paths should be faster than vice versa.
	// This is based on the assumption that fmod_cached_sounds and sound_paths will be roughly the same size, but either could be larger than the other.
	// If we were really desparate to optimize load times, we could intelligently decide which one to loop through based on their respective sizes, but this should be fine.
	auto cached_sounds_it = fmod_cached_sounds.begin();
	while (cached_sounds_it != fmod_cached_sounds.end())
	{
		auto it = std::find(sound_paths.begin(), sound_paths.end(), cached_sounds_it->first);
		if (it != sound_paths.end())
		{
			// Found sound. Mark it as don't unload and remove the string from sound_paths
			int index = it - sound_paths.begin(); // kinda hacky
			dont_unload[index] = true;
			sound_paths.erase(it); // remove so we don't load an already loaded sound
		}

		cached_sounds_it++;
	}

	// Now run through and unload any sounds not marked to be kept
	int index = 0; // hacky use of non-relational index. Consider replacing with something better.
	cached_sounds_it = fmod_cached_sounds.begin();
	while (cached_sounds_it != fmod_cached_sounds.end())
	{
		if (!dont_unload[index])
		{
			// Release resources related to this sound
			cached_sounds_it->second->release();
			fmod_cached_sounds.erase(cached_sounds_it);
		}

		cached_sounds_it++;
		index++;
	}

	// Finally, load any remaining sounds that we didn't carry over from the previous map
	for (int i = 0; i < sound_paths.size(); i++) // array-style access is faster than access by iterator
	{
		FMOD::Sound *sound = Fmod_CacheSound(sound_paths[i].c_str(), false);
		if (!sound)
			_Fmod_Report("WARNING", "Error occured while attempting to precache " + sound_paths[i] + ". Sound was not precached.");
	}

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodAmb(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string msg = std::string(READ_STRING());
	bool looping = READ_BYTE();
	// TODO: Clean this up and put all the reads together for more visual clarity in the code
	Vector pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	FMOD_VECTOR fmod_pos = _Fmod_HLVecToFmodVec(pos);

	FMOD_VECTOR vel;
	vel.x = 0;
	vel.y = 0;
	vel.z = 0;

	int vol_int = READ_BYTE(); // 0-255. 100 = 100% volume
	float volume = vol_int / 100.0f; // convert 0-100 to 0-1.0 (floating point)

	int min_atten = READ_SHORT(); // 0-32767
	int max_atten = READ_LONG(); // 0-2147483647
	float pitch = READ_BYTE() / 100.0f; // 0-255. 100 = normal pitch, 200 = one octave up

	// TODO: sanitize inputs

	std::string channel_name = msg.substr(0, msg.find('\n'));
	std::string sound_path = msg.substr(msg.find('\n') + 1, std::string::npos);

	FMOD::Sound* sound = NULL;
	FMOD::Channel* channel = NULL;

	// TODO: Separate finding the sound into its own function
	auto sound_iter = fmod_cached_sounds.find(sound_path);

	if (sound_iter == fmod_cached_sounds.end())
	{
		_Fmod_Report("WARNING", "Entity " + channel_name + " playing uncached sound " + sound_path +
									". Add the sound to your [MAPNAME].bsp_soundcache.txt file.");
		_Fmod_Report("INFO", "Attempting to cache and play sound " + sound_path);
		sound = Fmod_CacheSound(sound_path.c_str(), false);
		if (!sound) return false; // Error will be reported by Fmod_CacheSound
	}
	else
		sound = sound_iter->second;

	// Non-looping sounds need a new channel every time they play
	// TODO: Consider destroying the old channel with the same name.
	// Right now, if an fmod_ambient is triggered before it finishes playing, it'll play twice (offset by the time between triggers).
	// If we destroy the old channel with the same name, it'll stop playback of the old channel before playing the new channel.
	if (!looping)
	{
		channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, false, volume);
		if (!channel) return false; // TODO: Report warning about failure to play here

		channel->set3DAttributes(&fmod_pos, &vel);
		channel->set3DMinMaxDistance(min_atten, max_atten);
		channel->setPitch(pitch);
		channel->setPaused(false);
	}

	// If looping, find the existing channel
	else
	{
		auto channel_iter = fmod_channels.find(channel_name);

		if (channel_iter == fmod_channels.end())
		{
			// TODO: send looping and volume info from entity
			channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, true, volume);
			if (!channel) return false; // TODO: Report warning about failure to play here
		}
		else channel = channel_iter->second;

		channel->set3DAttributes(&fmod_pos, &vel);
		channel->set3DMinMaxDistance(min_atten, max_atten);
		channel->setPitch(pitch);

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
	std::string sound_path = std::string(READ_STRING());
	bool looping = READ_BYTE();
	// TODO: Clean this up and put all the reads together for more visual clarity in the code

	int vol_int = READ_BYTE();		 // 0-255. 100 = 100% volume
	float volume = vol_int / 100.0f; // convert 0-100 to 0-1.0 (floating point)

	float pitch = READ_BYTE() / 100.0f; // 0-255. 100 = normal pitch, 200 = one octave up

	// TODO: sanitize inputs
;
	FMOD::Sound* sound = NULL;

	// TODO: Separate finding the sound into its own function
	auto sound_iter = fmod_tracks.find(sound_path);

	if (sound_iter == fmod_tracks.end())
	{
		_Fmod_Report("WARNING", "Attempting to play track " + sound_path + " without caching it. Add it to your tracks.txt!");
		_Fmod_Report("INFO", "Attempting to cache and play track " + sound_path);
		sound = Fmod_CacheSound(sound_path.c_str(), true);
		if (!sound) return false; // Error will be reported by Fmod_CacheSound
	}
	else sound = sound_iter->second;

	// Create a fresh channel every time a track plays
	if (fmod_current_track) fmod_current_track->stop();
	FMOD_RESULT result = fmod_system->playSound(sound, fmod_mp3_group, true, &fmod_current_track);
	if (!_Fmod_Result_OK(&result)) return false; // TODO: investigate if a failure here might create a memory leak

    // Set channel properties
	fmod_current_track->setVolume(volume);
	if (looping) fmod_current_track->setMode(FMOD_LOOP_NORMAL);

	fmod_current_track->setPitch(pitch);
	fmod_current_track->setPaused(false);

	return true;
}

bool CHudFmodPlayer::VidInit() { return true; }
bool CHudFmodPlayer::Draw(float flTime) { return true; }
void CHudFmodPlayer::Reset() { return; }