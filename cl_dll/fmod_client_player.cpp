#include "cl_dll.h"
#include "cl_util.h"
#include "extdll.h"
#include "hud.h" // CHudFmodPlayer declared in hud.h
#include "parsemsg.h"
#include "vector.h"
#include "fmod_api.h"
#include "FMOD/fmod_errors.h"

#include <map>
#include <fstream>
#include <iostream>

using namespace HLFMOD;

DECLARE_MESSAGE(m_Fmod, FmodCache)
DECLARE_MESSAGE(m_Fmod, FmodAmb)
DECLARE_MESSAGE(m_Fmod, FmodEmit)
DECLARE_MESSAGE(m_Fmod, FmodTrk)
DECLARE_MESSAGE(m_Fmod, FmodRev)
DECLARE_MESSAGE(m_Fmod, FmodPause)
DECLARE_MESSAGE(m_Fmod, FmodStop)
DECLARE_MESSAGE(m_Fmod, FmodSeek)
DECLARE_MESSAGE(m_Fmod, FmodSave)
DECLARE_MESSAGE(m_Fmod, FmodLoad)

bool CHudFmodPlayer::Init()
{
	HOOK_MESSAGE(FmodCache);
	HOOK_MESSAGE(FmodAmb);
	HOOK_MESSAGE(FmodEmit);
	HOOK_MESSAGE(FmodTrk);
	HOOK_MESSAGE(FmodRev);
	HOOK_MESSAGE(FmodPause);
	HOOK_MESSAGE(FmodStop);
	HOOK_MESSAGE(FmodSeek);
	HOOK_MESSAGE(FmodSave);
	HOOK_MESSAGE(FmodLoad);

	gHUD.AddHudElem(this);
	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodSave(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string filename = std::string(READ_STRING());

	_Fmod_Report("INFO", "Saving the following file: " + filename);

	std::ofstream save_file;
	save_file.open(filename);

	// fmod_current_track entry: SOUND_NAME MODE VOLUME PITCH POSITION PAUSED

	FMOD::Sound *sound;
	fmod_current_track->getCurrentSound(&sound);
	std::string current_track_sound_name = "music/null.ogg";

	FMOD_MODE current_track_mode = FMOD_2D;
	float current_track_volume = 0.0f;
	float current_track_pitch = 1.0f;
	unsigned int current_track_position = 0;
	bool current_track_paused = true;
	
	// Do a quick and dirty find by value
	if (fmod_current_track)
	{
		auto tracks_it = fmod_tracks.begin();
		while (tracks_it != fmod_tracks.end())
		{
			if (tracks_it->second == sound)
			{
				current_track_sound_name = tracks_it->first;
				break;
			}

			tracks_it++;
		}

		if (tracks_it == fmod_tracks.end())
		{
			_Fmod_Report("ERROR", "Could not find fmod_current_track for savefile!");
			return false;
		}

		fmod_current_track->getMode(&current_track_mode);
		fmod_current_track->getVolume(&current_track_volume);
		fmod_current_track->getPitch(&current_track_pitch);
		fmod_current_track->getPosition(&current_track_position, FMOD_TIMEUNIT_PCM);
		fmod_current_track->getPaused(&current_track_paused);
	}

	// Save current track
	save_file << current_track_sound_name << " " << current_track_mode << " " << current_track_volume << " " << current_track_pitch
			  << " " << current_track_position << " " << current_track_paused << std::endl;

	// Save number of channels
	save_file << fmod_channels.size() << std::endl;

	auto channels_it = fmod_channels.begin();
	while (channels_it != fmod_channels.end())
	{
		// channel entry: ENT_NAME SOUND_NAME MODE X Y Z VOLUME MINDIST MAXDIST PITCH POSITION PAUSED
		std::string ent_name = channels_it->first;
		FMOD::Channel *channel = channels_it->second;

		FMOD_MODE mode;
		FMOD_VECTOR pos;
		FMOD_VECTOR vel; // unused
		float volume = 0.0f;
		// min_dist and max_dist are saved in Fmod units and loaded in Fmod units
		float min_dist = DEFAULT_MIN_ATTEN;
		float max_dist = DEFAULT_MAX_ATTEN;
		float pitch = 1.0f;
		unsigned int position = 0;
		bool paused = true;

		channel->getMode(&mode);
		channel->get3DAttributes(&pos, &vel);
		channel->getVolume(&volume);
		channel->get3DMinMaxDistance(&min_dist, &max_dist);
		channel->getPitch(&pitch);
		channel->getPosition(&position, FMOD_TIMEUNIT_PCM);
		channel->getPaused(&paused);

		// Find sound path
		std::string sound_name = "";
		FMOD::Sound *sound;
		channel->getCurrentSound(&sound);

		// Do quick and dirty find by value
		auto sounds_it = fmod_cached_sounds.begin();
		while (sounds_it != fmod_cached_sounds.end())
		{
			if (sounds_it->second == sound)
			{
				sound_name = sounds_it->first;
				break;
			}

			sounds_it++;
		}

		if (sounds_it == fmod_cached_sounds.end())
		{
			_Fmod_Report("ERROR", "Could not find sound for ent " + ent_name + " for savefile!");
			return false;
		}

		save_file << ent_name << " " << sound_name << " " << mode << " " << pos.x << " " << pos.y << " " << pos.z
			<< " " << volume << " " << min_dist << " " << max_dist << " " << pitch << " " << position << " " << paused;

		channels_it++;

		//if (channels_it != fmod_channels.end())
		save_file << std::endl;
	}

	// Save number of reverb spheres
	save_file << fmod_reverb_spheres.size() << std::endl;

	for (size_t i = 0; i < fmod_reverb_spheres.size(); i++)
	{
		FMOD::Reverb3D* reverb_sphere = std::get<0>(fmod_reverb_spheres[i]);
		int preset = std::get<1>(fmod_reverb_spheres[i]);

		FMOD_VECTOR vec;
		// min_dist and max_dist are saved in Fmod units and loaded in Fmod units
		float min_dist;
		float max_dist;

		reverb_sphere->get3DAttributes(&vec, &min_dist, &max_dist);

		save_file << preset << " " << vec.x << " " << vec.y << " " << vec.z << " " << min_dist << " " << max_dist << std::endl;
	}

	save_file.close();

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodLoad(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string filename = std::string(READ_STRING());

	Fmod_Release_Channels();

	std::ifstream save_file;
	save_file.open(filename);

	if (!save_file.is_open())
	{
		// TODO: player will sometimes make bogus savefiles since apparently CBasePlayer::Restore is called every level transition
		// For now, fail silently.
		return false;
	}

	_Fmod_Report("INFO", "Loading the following file: " + filename);

	bool mp3_paused = false;
	bool sfx_paused = false;

	fmod_mp3_group->getPaused(&mp3_paused);
	fmod_sfx_group->getPaused(&sfx_paused);

	fmod_mp3_group->setPaused(true);
	fmod_sfx_group->setPaused(true);

	fmod_current_track->stop();

	std::string current_track_sound_name = "";
	FMOD_MODE current_track_mode;
	float current_track_volume = 0.0f;
	float current_track_pitch = 1.0f;
	unsigned int current_track_position = 0;
	bool current_track_paused = true;

	save_file >> current_track_sound_name >> current_track_mode >> current_track_volume >> current_track_pitch >> current_track_position >> current_track_paused;
	// TODO: verify we didn't reach EOF

	auto it = fmod_tracks.find(current_track_sound_name);

	if (it == fmod_tracks.end())
	{
		_Fmod_Report("ERROR", "Could not find track " + current_track_sound_name + " while loading savefile!");
		return false;
	}

	FMOD::Sound *current_track_sound = it->second;

	FMOD_RESULT result = fmod_system->playSound(current_track_sound, fmod_mp3_group, true, &fmod_current_track);
	if (!_Fmod_Result_OK(&result))
		return false; // TODO: investigate if a failure here might create a memory leak

	fmod_current_track->setMode(current_track_mode);
	fmod_current_track->setVolume(current_track_volume);
	fmod_current_track->setPitch(current_track_pitch);
	fmod_current_track->setPosition(current_track_position, FMOD_TIMEUNIT_PCM);
	fmod_current_track->setPaused(current_track_paused);

	// Get number of channels
	int num_channels;
	save_file >> num_channels;
	// TODO: verify we didn't reach EOF

	for (int i = 0; i < num_channels; i++)
	{
		std::string ent_name = "";
		std::string sound_name = "";
		FMOD_MODE mode;
		FMOD_VECTOR pos;
		FMOD_VECTOR vel; // unused
		float volume = 0.0f;
		// min_dist and max_dist are saved in Fmod units and loaded in Fmod units
		float min_dist = DEFAULT_MIN_ATTEN;
		float max_dist = DEFAULT_MAX_ATTEN;
		float pitch = 1.0f;
		unsigned int position = 0;
		bool paused = true;

		save_file >> ent_name >> sound_name >> mode >> pos.x >> pos.y >> pos.z >> volume >> min_dist >> max_dist >> pitch >> position >> paused;
		vel.x = 0; vel.y = 0; vel.z = 0;

		FMOD::Sound *sound = nullptr;
		auto sound_it = fmod_cached_sounds.find(sound_name);

		if (sound_it == fmod_cached_sounds.end())
		{
			// When loading a save from a different map, this message might be received before _Fmod_LoadSounds gets called
			// This is fine -- just cache the sound here and _Fmod_LoadSounds will know not to load it redundantly
			sound = Fmod_CacheSound(sound_name.c_str(), false);
		}
		else
		{
			sound = sound_it->second;
		}

		FMOD::Channel *channel = Fmod_CreateChannel(sound, ent_name.c_str(), fmod_sfx_group, false, volume);
		channel->setMode(mode);
		channel->set3DAttributes(&pos, &vel);
		channel->set3DMinMaxDistance(min_dist, max_dist);
		channel->setPitch(pitch);
		channel->setPosition(position, FMOD_TIMEUNIT_PCM);
		channel->setPaused(paused);
	}

	// Get number of reverb spheres
	int num_reverb_spheres;
	save_file >> num_reverb_spheres;

	for (int i = 0; i < num_reverb_spheres; i++)
	{
		int preset;
		Vector vec;
		// min_dist and max_dist are saved in Fmod units and loaded in Fmod units
		float min_dist;
		float max_dist;

		save_file >> preset >> vec.x >> vec.y >> vec.z >> min_dist >> max_dist;

		Fmod_CreateReverbSphere(preset, vec, min_dist, max_dist);
	}

	save_file.close();

	fmod_mp3_group->setPaused(mp3_paused);
	fmod_sfx_group->setPaused(sfx_paused);

	return true;
}

// unused
bool CHudFmodPlayer::MsgFunc_FmodCache(const char* pszName, int iSize, void* pbuf)
{
	_Fmod_LoadSounds();
	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodAmb(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string msg = std::string(READ_STRING());
	bool looping = READ_BYTE();

	Vector pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	float volume = READ_FLOAT();

	// We call set3DMinMaxDistance directly in this function so we need to convert these to fmod units (meters)
	float min_atten_meters = READ_FLOAT() * HLUNITS_TO_METERS;
	float max_atten_meters = READ_FLOAT() * HLUNITS_TO_METERS;
	float pitch = READ_FLOAT();

	FMOD_VECTOR fmod_pos = _Fmod_HLVecToFmodVec(pos);

	FMOD_VECTOR vel;
	vel.x = 0;
	vel.y = 0;
	vel.z = 0;

	// TODO: sanitize inputs

	std::string channel_name = msg.substr(0, msg.find('\n'));
	std::string sound_path = msg.substr(msg.find('\n') + 1, std::string::npos);

	if (channel_name == "fmod_current_track")
	{
		_Fmod_Report("WARNING", "Tried to use fmod_ambient with fmod_current_track! This is not allowed!");
		return false;
	}

	FMOD::Sound* sound = Fmod_GetCachedSound(sound_path.c_str());
	if (!sound) return false; // Error reported in Fmod_GetCachedSound call stack

	FMOD::Channel* channel = NULL;

	// Non-looping sounds need a new channel every time they play
	// TODO: Consider destroying the old channel with the same name.
	// Right now, if an fmod_ambient is triggered before it finishes playing, it'll play twice (offset by the time between triggers).
	// If we destroy the old channel with the same name, it'll stop playback of the old channel before playing the new channel.
	if (!looping)
	{
		channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, false, volume);
		if (!channel)
		{
			_Fmod_Report("WARNING", "Failed to create channel " + channel_name);
			return false;
		}

		channel->set3DAttributes(&fmod_pos, &vel);
		channel->set3DMinMaxDistance(min_atten_meters, max_atten_meters);
		channel->setPitch(pitch);
		channel->setPaused(false);
	}

	// If looping, find the existing channel (or create new channel if none exists)
	else
	{
		FMOD::Channel* channel = Fmod_GetChannel(channel_name.c_str(), false);

		if (!channel)
		{
			// TODO: send looping and volume info from entity
			channel = Fmod_CreateChannel(sound, channel_name.c_str(), fmod_sfx_group, true, volume);
			if (!channel)
			{
				_Fmod_Report("WARNING", "Failed to create channel " + channel_name);
				return false;
			}
		}

		channel->set3DAttributes(&fmod_pos, &vel);
		channel->set3DMinMaxDistance(min_atten_meters, max_atten_meters);
		channel->setPitch(pitch);

		// When a looping fmod_ambient gets used, by default it'll flip the status of paused
		bool paused = false;
		channel->getPaused(&paused);
		channel->setPaused(!paused);
	}

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodEmit(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string msg = std::string(READ_STRING());
	bool looping = READ_BYTE();

	Vector pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	float volume = READ_FLOAT();

	float min_atten = READ_FLOAT();
	float max_atten = READ_FLOAT();
	float pitch = READ_FLOAT();

	std::string channel_name = msg.substr(0, msg.find('\n'));
	std::string sound_path = msg.substr(msg.find('\n') + 1, std::string::npos);

	FMOD::Sound* sound = Fmod_GetCachedSound(sound_path.c_str());
	if (!sound) return false; // Error reported in Fmod_GetCachedSound call stack

	Fmod_EmitSound(sound, channel_name.c_str(), volume, looping, pos, min_atten, max_atten, pitch);

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodTrk(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string sound_path = std::string(READ_STRING());
	bool looping = READ_BYTE();
	float volume = READ_FLOAT();
	float pitch = READ_FLOAT();

	// TODO: sanitize inputs

	FMOD::Sound* sound = Fmod_GetCachedTrack(sound_path.c_str());
	if (!sound) return false; // error reported in Fmod_GetCachedSound call stack

	// Create a fresh channel every time a track plays
	if (fmod_current_track) fmod_current_track->stop();
	FMOD_RESULT result = fmod_system->playSound(sound, fmod_mp3_group, true, &fmod_current_track);
	if (!_Fmod_Result_OK(&result)) return false; // TODO: investigate if a failure here might create a memory leak

    // Set channel properties
	fmod_current_track->setVolume(volume);
	if (looping)
	{
		// TODO: See why this doesn't work as expected
		/*FMOD_MODE mode;
		fmod_current_track->getMode(&mode);
		fmod_current_track->setMode(mode | FMOD_LOOP_NORMAL);*/

		fmod_current_track->setMode(FMOD_LOOP_NORMAL);
	}

	fmod_current_track->setPitch(pitch);
	fmod_current_track->setPaused(false);

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodRev(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);

	Vector pos;
	pos.x = READ_COORD();
	pos.y = READ_COORD();
	pos.z = READ_COORD();

	float min_dist = READ_FLOAT();
	long max_dist = READ_FLOAT();

	int preset = READ_BYTE();

	Fmod_CreateReverbSphere(preset, pos, min_dist, max_dist);

	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodPause(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string channel_name = std::string(READ_STRING());

	FMOD::Channel* channel = Fmod_GetChannel(channel_name.c_str());

	if (channel)
	{
		bool paused = false;
		channel->getPaused(&paused);
		channel->setPaused(!paused);
	}
	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodStop(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string channel_name = std::string(READ_STRING());

	FMOD::Channel* channel = Fmod_GetChannel(channel_name.c_str());

	if (channel)
	{
		channel->setPaused(true);
	}
	return true;
}

bool CHudFmodPlayer::MsgFunc_FmodSeek(const char* pszName, int iSize, void* pbuf)
{
	BEGIN_READ(pbuf, iSize);
	std::string channel_name = std::string(READ_STRING());
	float seek_to = READ_COORD();
	unsigned int position = (unsigned int)(seek_to * 1000); // convert seconds to milliseconds

	FMOD::Channel* channel = Fmod_GetChannel(channel_name.c_str());

	if (channel)
	{
		channel->setPosition(position, FMOD_TIMEUNIT_MS);
	}
	return true;
}

bool CHudFmodPlayer::VidInit() { return true; }
bool CHudFmodPlayer::Draw(float flTime) { return true; }
void CHudFmodPlayer::Reset() { return; }