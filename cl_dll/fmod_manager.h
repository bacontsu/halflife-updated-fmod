#pragma once

#include <string>
#include "FMOD/fmod.hpp"

typedef FMOD::Sound* Fmod_Sound;
typedef FMOD::ChannelGroup* Fmod_Group;

extern FMOD::System *fmod_system;
extern Fmod_Group fmod_mp3_group;
extern Fmod_Group fmod_sfx_group;

extern Fmod_Sound fmod_main_menu_music;

bool Fmod_Init(void);
void Fmod_Think(void);
Fmod_Sound Fmod_LoadSound(const char *path);
void Fmod_PlaySound(Fmod_Sound sound, Fmod_Group group, bool loop, float volume);
void Fmod_DestroySound(Fmod_Sound sound);
void Fmod_PlayMainMenuMusic(void);
void Fmod_Shutdown(void);

void _Fmod_Update_Volume(void);
bool _Fmod_Result_OK (FMOD_RESULT *result);