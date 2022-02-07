#pragma once

#include <string>
#include <unordered_map>
#include "FMOD/fmod.hpp"

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

// This typedef is just to avoid seeing the FMOD namespace outside of fmod_manager.cpp/h
typedef FMOD::ChannelGroup* Fmod_Group;

extern FMOD::System *fmod_system;
extern Fmod_Group fmod_mp3_group;
extern Fmod_Group fmod_sfx_group;

//extern Fmod_Sound fmod_main_menu_music;

extern std::unordered_map<std::string, FMOD::Sound*>   fmod_cached_sounds;
extern std::unordered_map<std::string, FMOD::Channel*> fmod_channels;

bool Fmod_Init(void);
void Fmod_Think(void);
void Fmod_Shutdown(void);

FMOD::Sound*   Fmod_CacheSound(const char* path, bool is_track);
FMOD::Channel* Fmod_CreateChannel(FMOD::Sound *sound, const char* name, Fmod_Group group, bool loop, float volume);

void _Fmod_Update_Volume(void);
void _Fmod_Warn(void);
bool _Fmod_Result_OK(FMOD_RESULT* result);