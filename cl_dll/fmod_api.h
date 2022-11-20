#pragma once

#include <string>
#include <unordered_map>
#include "FMOD/fmod.hpp"
#include "vector.h"

/* Notes on terminology: 
As far as fmod is concerned, a "sound" corresponds to a file on disk, but a "channel" is what you 
actually use to play the sound, pause, adjust the volume, etc. We only want to load a sound once, 
but we might want to play the sound multiple times simultaneously. Therefore we need to create sounds
and channels separately and treat them as distinct things, as you may have multiple channels linked to
a single sound. In short, sound = file and channel = what gets played.

A "group" is a collection of sounds. It is useful because a group can have its own properties that are 
then mixed with the properties of the sounds in its collection. The most obvious example is volume;
if a group has a volume property of 0.5f and a sound in that group has its own volume property of
0.75f, the final result is the sound will play at 0.375f volume (0.5f*0.75f=0.375f). In practice,
this implementation uses two groups that correspond to Half-Life's "SFX Volume" and "MP3 Volume"
sliders in the audio settings menu.
*/

namespace HLFMOD
{
	extern FMOD::System *fmod_system;
	extern FMOD::ChannelGroup* fmod_mp3_group;
	extern FMOD::ChannelGroup* fmod_sfx_group;

	extern std::unordered_map<std::string, FMOD::Sound*>   fmod_cached_sounds;
	extern std::unordered_map<std::string, FMOD::Channel*> fmod_channels;

	extern std::unordered_map<std::string, FMOD::Sound*> fmod_tracks;
	extern FMOD::Channel *fmod_current_track;

	typedef std::tuple<FMOD::Reverb3D*, uint8_t> Fmod_Reverb_Sphere;
	extern std::vector<Fmod_Reverb_Sphere> fmod_reverb_spheres;

	extern FMOD_REVERB_PROPERTIES fmod_reverb_presets[];

	extern const float METERS_TO_HLUNITS;
	extern const float HLUNITS_TO_METERS;
	extern const float DEFAULT_MIN_ATTEN;
	extern const float DEFAULT_MAX_ATTEN;

	bool Fmod_Init(void);
	void Fmod_Update(void);
	void Fmod_Think(struct ref_params_s *pparams);
	void Fmod_Update_Listener_Position(const Vector& pos, const Vector& vel, const Vector& forward, const Vector& up);
	void Fmod_Release_Sounds(void);
	void Fmod_Release_Channels(void);
	void Fmod_Shutdown(void);

	FMOD::Sound*    Fmod_CacheSound(const char* path, const bool is_track);
	FMOD::Sound*    Fmod_CacheSound(const char* path, const bool is_track, bool play_everywhere);
	FMOD::Sound*    Fmod_GetCachedSound(const char* sound_path);
	FMOD::Reverb3D* Fmod_CreateReverbSphere(int preset, const Vector& pos, const float min_distance, const float max_distance);
	FMOD::Channel*	Fmod_CreateChannel(FMOD::Sound* sound, const char* name, FMOD::ChannelGroup* group, const bool loop, const float volume);
	FMOD::Channel*	Fmod_GetChannel(const char* channel_name);
	FMOD::Channel*	Fmod_GetChannel(const char* channel_name, bool warn_if_not_found);

	FMOD::Channel* Fmod_EmitSound(const char* sound_path, float volume);
	FMOD::Channel* Fmod_EmitSound(FMOD::Sound* sound, float volume);
	FMOD::Channel* Fmod_EmitSound(FMOD::Sound* sound, const char* channel_name, float volume, const Vector& pos);
	FMOD::Channel* Fmod_EmitSound(FMOD::Sound* sound, const char* channel_name, float volume, bool looping, const Vector& pos, float min_atten, float max_atten, float pitch);

	// TODO: Maybe a Fmod_CacheTrack function?
	FMOD::Sound* Fmod_GetCachedTrack(const char* track_path);

	void _Fmod_LoadSounds(void);
	void _Fmod_LoadTracks(void);
	void _Fmod_LoadBSPGeo(void);
	void _Fmod_Update_Volume(void);
	void _Fmod_Report(const std::string &report_type, const std::string &info);
	bool _Fmod_Result_OK(FMOD_RESULT *result);
	FMOD_VECTOR _Fmod_HLVecToFmodVec(const Vector &vec);
	FMOD_VECTOR _Fmod_HLVecToFmodVec_NOSCALE(const Vector& vec);
}