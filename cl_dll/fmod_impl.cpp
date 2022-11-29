#include <filesystem>

#include "cl_dll.h"
#include "cl_util.h"
#include "extdll.h"
#include "hud.h"
#include "parsemsg.h"
#include "com_model.h"

#include "fmod_api.h"
#include "FMOD/fmod_errors.h"

#include <map>
#include <fstream>
#include <iostream>
#include <set>

namespace HLFMOD
{
	FMOD::System *fmod_system;
	FMOD::ChannelGroup* fmod_mp3_group;
	FMOD::ChannelGroup* fmod_sfx_group;

	std::unordered_map<std::string, FMOD::Sound*> fmod_cached_sounds;
	std::unordered_map<std::string, FMOD::Channel*> fmod_channels;
	std::unordered_map<std::string, FMOD::Sound*> fmod_tracks;

	std::vector<Fmod_Reverb_Sphere> fmod_reverb_spheres;

	FMOD::Channel* fmod_current_track;

	// Fmod uses meters. According to Valve, one Source unit = 3/4 inch = 1.905cm = 0.01905m. Assuming GoldSrc is identical.
	const float METERS_TO_HLUNITS = (float) (1.0/0.01905); // roughly 52.5
	const float HLUNITS_TO_METERS = 0.01905f;

	const float DEFAULT_MIN_ATTEN = 1.0f * METERS_TO_HLUNITS;
	const float DEFAULT_MAX_ATTEN = 10000.0f * METERS_TO_HLUNITS;

	FMOD_REVERB_PROPERTIES fmod_reverb_presets[] = {
		FMOD_PRESET_OFF,				//  0
		FMOD_PRESET_GENERIC,			//  1
		FMOD_PRESET_PADDEDCELL,			//  2
		FMOD_PRESET_ROOM,				//  3
		FMOD_PRESET_BATHROOM,			//  4
		FMOD_PRESET_LIVINGROOM,			//  5
		FMOD_PRESET_STONEROOM,			//  6
		FMOD_PRESET_AUDITORIUM,			//  7
		FMOD_PRESET_CONCERTHALL,		//  8
		FMOD_PRESET_CAVE,				//  9
		FMOD_PRESET_ARENA,				// 10
		FMOD_PRESET_HANGAR,				// 11
		FMOD_PRESET_CARPETTEDHALLWAY,	// 12
		FMOD_PRESET_HALLWAY,			// 13
		FMOD_PRESET_STONECORRIDOR,		// 14
		FMOD_PRESET_ALLEY,				// 15
		FMOD_PRESET_FOREST,				// 16
		FMOD_PRESET_CITY,				// 17
		FMOD_PRESET_MOUNTAINS,			// 18
		FMOD_PRESET_QUARRY,				// 19
		FMOD_PRESET_PLAIN,				// 20
		FMOD_PRESET_PARKINGLOT,			// 21
		FMOD_PRESET_SEWERPIPE,			// 22
		FMOD_PRESET_UNDERWATER			// 23
	};

	std::string last_level_name = "";

	bool Fmod_Init(void)
	{
		FMOD_RESULT result;
		//fmod_main_menu_music = NULL;
		fmod_current_track = NULL;

		// Create the main system object.
		result = FMOD::System_Create(&fmod_system);
		if (!_Fmod_Result_OK(&result)) return false;

		// Initialize FMOD.
		result = fmod_system->init(512, FMOD_INIT_NORMAL, 0);
		if (!_Fmod_Result_OK(&result)) return false;

		// Create mp3 channel group
		result = fmod_system->createChannelGroup("MP3", &fmod_mp3_group);
		if (!_Fmod_Result_OK(&result)) return false;
		fmod_mp3_group->setVolume(1.0f);

		// Create sfx channel group
		result = fmod_system->createChannelGroup("SFX", &fmod_sfx_group);
		if (!_Fmod_Result_OK(&result)) return false;
		fmod_sfx_group->setVolume(1.0f);

		_Fmod_Update_Volume();

		// TODO: Allow customizing doppler and rolloff scale in a cfg
		// distancefactor is set to default since we manually convert HL units into Fmod units (meters) for everything including velocity now.
		// distancefactor ONLY affects doppler, not things like attenuation, hence doing manual conversion now.
		// fmod_system->set3DSettings(1.0f, 1.0f, 1.0f);

		_Fmod_LoadTracks();

		// TODO: probably worth putting this in a "set track" function in the API
		// Play main menu music
		result = fmod_system->playSound(fmod_tracks.begin()->second, fmod_mp3_group, true, &fmod_current_track);
		if (!_Fmod_Result_OK(&result)) return false;

		fmod_current_track->setVolume(1.0f);
		fmod_current_track->setMode(FMOD_LOOP_NORMAL);
		fmod_current_track->setPaused(false);

		return true;
	}

	void Fmod_Update(void)
	{
		FMOD_RESULT result;

		// Update FMOD
		result = fmod_system->update();
		_Fmod_Result_OK(&result);

		_Fmod_Update_Volume();

		// Get the forward and up vector
		Vector playerForward, playerRight, playerUp;
		gEngfuncs.pfnAngleVectors(gHUD.playerAngles, playerForward, playerRight, playerUp);

		// update position
		Fmod_Update_Listener_Position(gHUD.playerOrigin, gHUD.playerSpeed, playerForward, playerUp);

		// update entities position
		Fmod_Update_Sound_Sources();
	}

	void Fmod_Think(struct ref_params_s* pparams)
	{
		if (pparams->paused)
		{
			fmod_sfx_group->setPaused(true);
			fmod_mp3_group->setPaused(true);
		}
		else
		{
			fmod_sfx_group->setPaused(false);
			fmod_mp3_group->setPaused(false);
		}

		std::string level_name = gEngfuncs.pfnGetLevelName();

		// Level has changed
		if (level_name != last_level_name)
		{
			// Always pause level track on new level load for multiplayer (TODO: maybe allow this to be configured?)
			bool multiplayer = gEngfuncs.GetMaxClients() != 1;
			if (multiplayer && fmod_current_track)
			{
				fmod_current_track->setPaused(true);
			}

			// Cache sounds
			_Fmod_LoadSounds();

			last_level_name = level_name;
		}
	}

	void _Fmod_LoadSounds(void)
	{
		// We can't release channels here because this might get called *after* MsgFunc_FmodLoad which would kill any loaded channels
		// playing. This means we only ever unload channels when
		//Fmod_Release_Channels();
		//Fmod_Release_Sounds(); // We now intelligently only release sounds that aren't used in the current map

		// This is O(mn) currently. Possible O(n) solution might be to create a set of pairs (sound_path, want_loaded) that
		// represent all sound files in play (cached sounds + want_to_cache sounds, unique entries only) then iterate through
		// that.

		std::string gamedir = gEngfuncs.pfnGetGameDirectory();
		std::string level_name = gEngfuncs.pfnGetLevelName();

		std::string files_to_load_from[] = {
			gamedir + "/" + level_name + "_soundcache.txt",
			gamedir + "/alwayscache.txt"};

		// Get all sound path strings from cache files
		std::set<std::string> want_to_cache; // Complexity for find is much better with a set than a vector

		for (std::string cache_filepath : files_to_load_from)
		{
			std::ifstream soundcache_file;
			soundcache_file.open(cache_filepath);

			if (!soundcache_file.is_open())
			{
				_Fmod_Report("WARNING", "Could not open soundcache file " + cache_filepath + ". No sounds were precached!");
				Fmod_Release_Sounds(); // Release all sounds
				return;
			}
			else
				_Fmod_Report("INFO", "Precaching sounds from file: " + cache_filepath);

			std::string filename;
			while (std::getline(soundcache_file, filename))
				want_to_cache.insert(filename);

			if (!soundcache_file.eof())
				_Fmod_Report("WARNING", "Stopped reading soundcache file " + cache_filepath + " before the end of file due to unknown error.");

			soundcache_file.close();
		}

		std::vector<std::string> remove_me;

		// Iterate through cached sounds and handle sounds that should be unloaded, and wanted sounds that are already loaded
		auto cached_sounds_it = fmod_cached_sounds.begin();
		while (cached_sounds_it != fmod_cached_sounds.end())
		{
			auto want_to_cache_it = want_to_cache.find(cached_sounds_it->first);

			// Sound is already loaded
			if (want_to_cache_it != want_to_cache.end())
			{
				want_to_cache.erase(want_to_cache_it);
			}

			// Sound should be unloaded
			else
			{
				remove_me.push_back(cached_sounds_it->first);
			}

			cached_sounds_it++;
		}

		// Unload unwanted sounds
		for (size_t i = 0; i < remove_me.size(); i++)
		{
			auto sound_it = fmod_cached_sounds.find(remove_me[i]);
			sound_it->second->release();
			fmod_cached_sounds.erase(sound_it);
		}

		// Load remaining want_to_cache entries
		auto want_to_cache_it = want_to_cache.begin();
		while (want_to_cache_it != want_to_cache.end())
		{
			Fmod_CacheSound(want_to_cache_it->c_str(), false);
			want_to_cache_it++;
		}
	}

	void _Fmod_LoadTracks(void)
	{
		// TODO: This and CHudFmodPlayer::MsgFunc_FmodCache are almost identical. Maybe consolidate into a single function.
		std::string gamedir = gEngfuncs.pfnGetGameDirectory();
		std::string tracks_txt_path = gamedir + "/" + "tracks.txt";

		if (!std::filesystem::exists(tracks_txt_path))
		{
			tracks_txt_path = "valve/tracks.txt";
		}

		std::ifstream tracks_txt_file;
		tracks_txt_file.open(tracks_txt_path);

		if (!tracks_txt_file.is_open())
		{
			_Fmod_Report("WARNING", "Could not open soundcache file " + tracks_txt_path + ". No sounds were precached!");
			return;
		}
		else _Fmod_Report("INFO", "Precaching tracks from file: " + tracks_txt_path);

		std::string filename;
		while (std::getline(tracks_txt_file, filename))
		{
			// TODO: Need to handle Windows vs Unix line endings
			FMOD::Sound* sound = Fmod_CacheSound(filename.c_str(), true);
			if (!sound)
			{
				_Fmod_Report("ERROR", "Error occured during precaching tracks. Tried precaching: " + filename + ". Precaching stopped.");
				return;
			}
		}

		if (!tracks_txt_file.eof())
			_Fmod_Report("WARNING", "Stopped reading soundcache file " + tracks_txt_path + " before the end of file due to unknown error.");

		tracks_txt_file.close();
	}

	// Update entities origin
	void Fmod_Update_Sound_Sources()
	{
		for (int index = 0; index < 512; index++)
		{
			auto ent = gEngfuncs.GetEntityByIndex(index);
			if (ent && ent->model)
			{
				if (ent->model->type == mod_brush)
				{
					auto channel = Fmod_GetChannel(std::string(std::to_string(ent->index)).c_str(), false);
					if (channel)
					{
						FMOD_VECTOR org = _Fmod_HLVecToFmodVec(ent->curstate.origin + (ent->model->mins + ent->model->maxs) * 0.5f);
						FMOD_VECTOR vel;

						channel->set3DAttributes(&org, &vel);
					}
				}
				else
				{
					auto channel = Fmod_GetChannel(std::string(std::to_string(ent->index)).c_str(), false);
					if (channel)
					{
						FMOD_VECTOR org = _Fmod_HLVecToFmodVec(ent->curstate.origin);
						FMOD_VECTOR vel;

						channel->set3DAttributes(&org, &vel);
					}
				}
			}
		}
	}

	void Fmod_Update_Listener_Position(const Vector& pos, const Vector& vel, const Vector& forward, const Vector& up)
	{
		FMOD_RESULT result;

		// Conver Vectors into FMOD vectors
		FMOD_VECTOR FMODPlayerPosition = _Fmod_HLVecToFmodVec(pos);
		FMOD_VECTOR FMODPlayerVelocity = _Fmod_HLVecToFmodVec(vel);
		FMOD_VECTOR FMODPlayerForward = _Fmod_HLVecToFmodVec_NOSCALE(forward);
		FMOD_VECTOR FMODPlayerUp = _Fmod_HLVecToFmodVec_NOSCALE(up);

		result = fmod_system->set3DListenerAttributes(0, &FMODPlayerPosition, &FMODPlayerVelocity, &FMODPlayerForward, &FMODPlayerUp);
		_Fmod_Result_OK(&result);
	}

	void Fmod_Release_Sounds(void)
	{
		auto sounds_it = fmod_cached_sounds.begin();

		while (sounds_it != fmod_cached_sounds.end())
		{
			sounds_it->second->release();
			sounds_it++;
		}

		fmod_cached_sounds.clear();
	}

	void Fmod_Release_Channels(void)
	{
		auto channels_it = fmod_channels.begin();

		while (channels_it != fmod_channels.end())
		{
			channels_it->second->stop();
			channels_it++;
		}

		fmod_channels.clear();
	}

	FMOD::Sound* Fmod_CacheSound(const char* path, const bool is_track)
	{
		return Fmod_CacheSound(path, is_track, false);
	}

	FMOD::Sound* Fmod_CacheSound(const char* path, const bool is_track, bool play_everywhere)
	{
		FMOD_RESULT result;
		FMOD::Sound *sound = NULL;

		std::string gamedir = gEngfuncs.pfnGetGameDirectory();
		std::string full_path = gamedir + "/" + path; 

		if (!std::filesystem::exists(full_path))
		{
			full_path = "valve/" + std::string(path);
		}

		// override play_everywhere if filename ends in _2D
		int ext_period_index = full_path.find_last_of('.');
		if (full_path[ext_period_index - 3] == '_' &&
			full_path[ext_period_index - 2] == '2' &&
			full_path[ext_period_index - 1] == 'D')
		{
			play_everywhere = true;
		}

		// Create the sound/stream from the file on disk
		if (is_track)
		{
			result = fmod_system->createStream(full_path.c_str(), FMOD_2D, NULL, &sound);
			if (!_Fmod_Result_OK(&result)) return NULL; // TODO: investigate if a failure here might create a memory leak

			// If all went okay, insert the track into the cache
			fmod_tracks.insert(std::pair(path, sound));
		}
		else
		{
			if (play_everywhere)
				result = fmod_system->createSound(full_path.c_str(), FMOD_2D, NULL, &sound);
			else
				result = fmod_system->createSound(full_path.c_str(), FMOD_3D, NULL, &sound);

			if (!_Fmod_Result_OK(&result)) return NULL; // TODO: investigate if a failure here might create a memory leak

			// If all went okay, insert the sound into the cache
			fmod_cached_sounds.insert(std::pair(path, sound));
		}

		return sound;
	}

	FMOD::Reverb3D* Fmod_CreateReverbSphere(int preset, const Vector& pos, const float min_distance, const float max_distance)
	{
		FMOD_RESULT result;
		FMOD::Reverb3D* reverb_sphere = NULL;

		result = fmod_system->createReverb3D(&reverb_sphere);
		if (!_Fmod_Result_OK(&result)) return NULL; // TODO: investigate if a failure here might create a memory leak

		FMOD_REVERB_PROPERTIES properties = fmod_reverb_presets[preset];

		FMOD_VECTOR fmod_pos = _Fmod_HLVecToFmodVec(pos);

		reverb_sphere->setProperties(&properties);
		reverb_sphere->set3DAttributes(&fmod_pos, min_distance * HLUNITS_TO_METERS, max_distance * HLUNITS_TO_METERS);

		Fmod_Reverb_Sphere rev_tuple(reverb_sphere, preset);
		fmod_reverb_spheres.push_back(rev_tuple);

		return reverb_sphere;
	}

	FMOD::Channel* Fmod_CreateChannel(FMOD::Sound* sound, const char* name, FMOD::ChannelGroup* group, const bool loop, const float volume)
	{
		FMOD_RESULT result;
		FMOD::Channel* channel = NULL;

		// Create the sound channel
		// We "play" the sound to create the channel, but it's starting paused so we can set properties on it before it /actually/ plays.
		result = fmod_system->playSound(sound, group, true, &channel);
		if (!_Fmod_Result_OK(&result)) return NULL; // TODO: investigate if a failure here might create a memory leak

		// Set channel properties
		channel->setVolume(volume);
		if (loop)
		{
			// TODO: See why this doesn't work as expected
			/* FMOD_MODE mode;
			channel->getMode(&mode);
			channel->setMode(mode | FMOD_LOOP_NORMAL);*/

			channel->setMode(FMOD_LOOP_NORMAL);
		}

		// If all went okay, stick it in the channel list. Only looping sounds store a reference.
		fmod_channels.insert(std::pair(name, channel));

		return channel;
	}

	FMOD::Sound* Fmod_GetCachedSound(const char* sound_path)
	{
		FMOD::Sound* sound = NULL;

		auto sound_iter = fmod_cached_sounds.find(sound_path);

		if (sound_iter == fmod_cached_sounds.end())
		{
			_Fmod_Report("WARNING", "Trying to play uncached sound " + std::string(sound_path) +
										". Add the sound to your [MAPNAME].bsp_soundcache.txt file.");
			_Fmod_Report("INFO", "Attempting to cache and play sound " + std::string(sound_path));
			sound = Fmod_CacheSound(sound_path, false);
		}
		else
			sound = sound_iter->second;

		return sound;
	}

	FMOD::Sound* Fmod_GetCachedTrack(const char* track_path)
	{
		FMOD::Sound* track = NULL;

		auto track_iter = fmod_tracks.find(track_path);

		if (track_iter == fmod_tracks.end())
		{
			_Fmod_Report("WARNING", "Trying to play uncached track " + std::string(track_path) +
										". Add the sound to your tracks.txt file.");
			_Fmod_Report("INFO", "Attempting to cache and play track " + std::string(track_path));
			track = Fmod_CacheSound(track_path, true);
		}
		else
			track = track_iter->second;

		return track;
	}

	FMOD::Channel* Fmod_EmitSound(const char* sound_path, float volume)
	{
		FMOD::Sound* sound = Fmod_GetCachedSound(sound_path);

		Vector dummyVec;
		return Fmod_EmitSound(sound, "NONAME", volume, false, dummyVec, DEFAULT_MIN_ATTEN, DEFAULT_MAX_ATTEN, 1.0f);
	}

	FMOD::Channel* Fmod_GetChannel(const char* channel_name)
	{
		return Fmod_GetChannel(channel_name, true);
	}

	FMOD::Channel* Fmod_GetChannel(const char* channel_name, bool warn_if_not_found)
	{
		FMOD::Channel* channel = NULL;

		if (std::string(channel_name) == "fmod_current_track")
		{
			channel = fmod_current_track;
		}

		else
		{
			auto it = fmod_channels.find(channel_name);
			if (it == fmod_channels.end())
			{
				if (warn_if_not_found) _Fmod_Report("WARNING", "Tried to find unknown channel " + std::string(channel_name));
				return NULL;
			}
			channel = it->second;
		}

		return channel;
	}

	FMOD::Channel* Fmod_EmitSound(FMOD::Sound* sound, float volume)
	{
		Vector dummyVec;
		return Fmod_EmitSound(sound, "NONAME", volume, false, dummyVec, DEFAULT_MIN_ATTEN, DEFAULT_MAX_ATTEN, 1.0f);
	}

	FMOD::Channel* Fmod_EmitSound(FMOD::Sound* sound, const char* channel_name, float volume, const Vector& pos)
	{
		return Fmod_EmitSound(sound, channel_name, volume, false, pos, DEFAULT_MIN_ATTEN, DEFAULT_MAX_ATTEN, 1.0f);
	}

	FMOD::Channel* Fmod_EmitSound(FMOD::Sound* sound, const char* channel_name, float volume, bool looping, const Vector& pos, float min_atten, float max_atten, float pitch)
	{
		FMOD_VECTOR fmod_pos = _Fmod_HLVecToFmodVec(pos);
		FMOD_VECTOR vel;
		vel.x = 0;
		vel.y = 0;
		vel.z = 0;

		FMOD::Channel* channel = NULL;

		// Always create a new channel for EmitSound
		channel = Fmod_CreateChannel(sound, channel_name, fmod_sfx_group, looping, volume);
		if (!channel)
		{
			_Fmod_Report("WARNING", "Failed to create channel " + std::string(channel_name));
			return NULL;
		}

		channel->set3DAttributes(&fmod_pos, &vel);
		channel->set3DMinMaxDistance(min_atten * HLUNITS_TO_METERS, max_atten * HLUNITS_TO_METERS);
		channel->setPitch(pitch);
		channel->setPaused(false);

		return channel;
	}

	void Fmod_Shutdown(void)
	{
		FMOD_RESULT result;

		result = fmod_system->release();
		_Fmod_Result_OK(&result);
	}

	void _Fmod_Update_Volume(void)
	{
		// Check if user has changed volumes and update accordingly
		float new_mp3_vol = CVAR_GET_FLOAT("MP3Volume");
		float old_mp3_vol = 1.0f;
		fmod_mp3_group->getVolume(&old_mp3_vol);
		if (new_mp3_vol != old_mp3_vol)
			fmod_mp3_group->setVolume(new_mp3_vol);

		float new_sfx_vol = CVAR_GET_FLOAT("volume");
		float old_sfx_vol = 1.0f;
		fmod_sfx_group->getVolume(&old_sfx_vol);
		if (new_sfx_vol != old_sfx_vol)
			fmod_sfx_group->setVolume(new_sfx_vol);
	}

	// Check result and return true if OK or false if not OK
	// Sometimes return value is ignored and this is used for throwing warnings only
	bool _Fmod_Result_OK (FMOD_RESULT *result)
	{
		if (*result != FMOD_OK)
		{
			std::string fmod_error_str = FMOD_ErrorString(*result);
			std::string error_msg = "FMOD ERROR: " + *result + std::string(": ") + fmod_error_str + "\n";
			fprintf(stderr, error_msg.c_str());
			ConsolePrint(error_msg.c_str());
			return false;
		}

		return true;
	}

	// Shortcut to send message to both stderr and console
	void _Fmod_Report(const std::string &report_type, const std::string &info)
	{
		std::string msg = "FMOD " + report_type + ": " + info + "\n";
		fprintf(stderr, msg.c_str());
		ConsolePrint(msg.c_str());
	}

	// Convert HL's coordinate system to Fmod (WITH UNIT SCALING)
	FMOD_VECTOR _Fmod_HLVecToFmodVec(const Vector &vec)
	{
		FMOD_VECTOR FMODVector;
		FMODVector.z = vec.x * HLUNITS_TO_METERS;
		FMODVector.x = -vec.y * HLUNITS_TO_METERS;
		FMODVector.y = vec.z * HLUNITS_TO_METERS;

		return FMODVector;
	}

	// Convert HL's coordinate system to Fmod (NO UNIT SCALING - shortcut for direction vectors)
	FMOD_VECTOR _Fmod_HLVecToFmodVec_NOSCALE(const Vector& vec)
	{
		FMOD_VECTOR FMODVector;
		FMODVector.z = vec.x;
		FMODVector.x = -vec.y;
		FMODVector.y = vec.z;

		return FMODVector;
	}

}